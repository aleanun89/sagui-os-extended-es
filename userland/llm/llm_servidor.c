// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_servidor.c - `llm --servidor` (D380): la IA local como un servidor compatible con la
 * API de OpenAI, para que `ia`, `chatia` y los agentes de Pluma, Macaco y Correo la usen sin
 * cambios (proveedor «llama.cpp», http://127.0.0.1:8080/v1).
 *
 *   GET  /v1/models              el modelo cargado
 *   POST /v1/chat/completions    messages, stream, max_tokens, temperature, seed,
 *                                tools y tool_choice (fase 2), imágenes en «content» (fase 2)
 *
 * Solo atiende conexiones de 127.0.0.1 (el modelo es del equipo, no de la red) y una
 * petición cada vez. Fase 2: las herramientas («tools») van en el sistema como pide la
 * plantilla de LFM3 y los <tool_call> que genera el modelo vuelven como «tool_calls»
 * (llm_ferramentas.c), así que los agentes (D295) funcionan igual que con la API remota; las
 * imágenes (data:image/...;base64) pasan por el proyector de visión (llm_visao.c, --mmproj).
 * Entre peticiones se conserva la caché: si la conversación nueva empieza por todo lo que ya
 * está en la caché (lo normal en un chat), solo se lee lo que falta. El estado de la
 * convolución no se puede rebobinar, así que con cualquier otra cosa (o con imágenes) se
 * empieza de cero. */
#include "llm_programa.h"

#ifdef LLM_ANFITRIAO
#define AG_ANFITRION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#else
#include "sagui.h"
#endif
/* El lector de JSON (userland/ia/json.c) se compila en llm_ferramentas.c. */
#include "llm_ferramentas.h"

#define CORPO_MAX (8u << 20)   /* con imágenes en base64 */
#define TOKS_MAX 8192
#define PRAZO_MS 10000   /* lo que espera la cabecera y el cuerpo: una conexión ociosa no bloquea el servidor */

/* ------------------------------------------------------------ red */
#ifdef LLM_ANFITRIAO
static int ouvir(int porta)
{
    signal(SIGPIPE, SIG_IGN);   /* un cliente que cierra no tumba el servidor */
    int fd = socket(AF_INET, SOCK_STREAM, 0), um = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)porta); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) || listen(fd, 4)) { close(fd); return -1; }
    return fd;
}
static int aceitar(int fd, bool *local)
{
    struct sockaddr_in a; socklen_t n = sizeof a;
    int c = accept(fd, (struct sockaddr *)&a, &n);
    *local = c >= 0 && ntohl(a.sin_addr.s_addr) == 0x7F000001u;
    struct timeval tv = { PRAZO_MS / 1000, 0 };
    if (c >= 0) setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    return c;
}
static int receber(int fd, char *b, int n) { return (int)read(fd, b, (size_t)n); }
static void fechar_c(int fd) { close(fd); }
#else
static int ouvir(int porta)
{
    int fd = criar_socket(SOCK_TCP);
    if (fd < 0) return fd;
    if (escutar(fd, porta, 4) < 0) { fechar(fd); return -1; }
    return fd;
}
static int aceitar(int fd, bool *local)
{
    endereco_rede_t de;
    int c = aceitar_conexao(fd, &de);
    *local = c >= 0 && de.ip == 0x7F000001u;
    if (c >= 0) tempo_limite(c, PRAZO_MS);
    return c;
}
static int receber(int fd, char *b, int n) { return ler(fd, b, (unsigned)n); }
static void fechar_c(int fd) { fechar(fd); }
#endif

static void enviar(int fd, const char *s, int n) { llm_escrever(fd, s, (size_t)n); }
static void enviar_txt(int fd, const char *s) { enviar(fd, s, (int)llm_strlen(s)); }

static void resposta(int fd, int codigo, const char *tipo, const char *corpo)
{
    char cab[256];
    int n = (int)llm_strlen(corpo);
    llm_formatar(cab, sizeof cab, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                 codigo, codigo == 200 ? "OK" : codigo == 404 ? "Not Found" : "Bad Request", tipo, n);
    enviar_txt(fd, cab);
    enviar(fd, corpo, n);
}

static void erro_json(int fd, int codigo, const char *msg)
{
    char b[512];
    llm_formatar(b, sizeof b, "{\"error\":{\"message\":\"%s\",\"type\":\"invalid_request_error\"}}", msg);
    resposta(fd, codigo, "application/json", b);
}

/* Texto -> cadena JSON (con comillas). -> bytes */
static int json_cadena(char *o, int max, const char *s, int n)
{
    int k = 0;
    if (max < 3) return 0;
    o[k++] = '"';
    for (int i = 0; i < n && k < max - 8; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20) k += llm_formatar(o + k, (size_t)(max - k), "\\u%04x", c);
        else o[k++] = (char)c;
    }
    o[k++] = '"';
    o[k] = 0;
    return k;
}

/* ------------------------------------------------------------ petición */

typedef struct {
    int fd;
    bool stream;
    bool herramientas;       /* la petición trae «tools»: se buscan <tool_call> en la respuesta */
    bool em_ferramenta;      /* ya salió <tool_call>: lo que sigue no se manda como texto */
    char modelo[200];        /* ya como cadena JSON (con comillas y escapes) */
    llm_buf_t todo;          /* lo generado, con el prefijo forzado (tool_choice) delante */
    int enviado;             /* bytes de «todo» ya mandados por el streaming */
} envio_t;

static void sse_dados(envio_t *e, const char *delta, const char *fim)
{
    llm_buf_t j = { 0 };
    llm_buf_txt(&j, "data: {\"id\":\"llm-local\",\"object\":\"chat.completion.chunk\",\"model\":");
    llm_buf_txt(&j, e->modelo);
    llm_buf_txt(&j, ",\"choices\":[{\"index\":0,\"delta\":");
    llm_buf_txt(&j, delta);
    llm_buf_txt(&j, ",\"finish_reason\":");
    llm_buf_txt(&j, fim ? fim : "null");
    llm_buf_txt(&j, "}]}\n\n");
    if (j.p) enviar(e->fd, j.p, j.n);
    llm_buf_libera(&j);
}

static void sse_texto(envio_t *e, const char *t, int n)
{
    if (n <= 0) return;
    llm_buf_t d = { 0 };
    llm_buf_txt(&d, "{\"content\":");
    llm_json_cadeia(&d, t, n);
    llm_buf_txt(&d, "}");
    if (d.p) sse_dados(e, d.p, NULL);
    llm_buf_libera(&d);
}

/* Manda lo que ya se puede (en trozos que acaban en espacio o salto de línea). Si hay
 * herramientas, nunca la marca <tool_call> ni lo que podría ser su principio. */
static void mandar_pendente(envio_t *e, bool tudo)
{
    if (!e->stream || e->em_ferramenta || !e->todo.p) return;
    int lim = e->todo.n;
    if (e->herramientas) {
        int g, p = llm_ferramentas_marca(e->todo.p + e->enviado, lim - e->enviado, &g);
        if (p >= 0) { lim = e->enviado + p; e->em_ferramenta = true; tudo = true; }
        else lim -= g;
    }
    if (lim <= e->enviado) return;
    if (!tudo) {
        int corte = -1;
        for (int i = lim - 1; i >= e->enviado; i--) if (e->todo.p[i] == ' ' || e->todo.p[i] == '\n') { corte = i + 1; break; }
        if (corte < 0 && lim - e->enviado < 400) return;
        if (corte > 0) lim = corte;
    }
    sse_texto(e, e->todo.p + e->enviado, lim - e->enviado);
    e->enviado = lim;
}

static void ao_texto(void *ctx, const char *t, int n)
{
    envio_t *e = ctx;
    if (e->todo.n + n > 256 * 1024) return;
    llm_buf_add(&e->todo, t, n);
    mandar_pendente(e, false);
}

static float numero_json(const char *corpo, const json_tok_t *tk, int i, float por_omision)
{
    if (i < 0 || tk[i].tipo != JSON_NUM) return por_omision;
    float v = 0, f = 0.1f;
    bool pt = false;
    for (int k = tk[i].ini; k < tk[i].fim; k++) {
        char c = corpo[k];
        if (c == '.') pt = true;
        else if (c >= '0' && c <= '9') { if (pt) { v += f * (c - '0'); f *= 0.1f; } else v = v * 10 + (c - '0'); }
        else break;
    }
    return v;
}

/* La conversación en tokens; las imágenes, aparte, con la posición donde van. */
#define IMAGENS_MAX 4
typedef struct {
    llm_sessao_t *s;
    int *t, n;
    int n_img, img_pos[IMAGENS_MAX];
    llm_buf_t img[IMAGENS_MAX];
    bool demasiadas;
} coletor_t;

static void ao_seg(void *ctx, int tipo, const char *x, int n)
{
    coletor_t *c = ctx;
    if (tipo == SEG_IMAGEM) {
        if (c->n_img >= IMAGENS_MAX) { c->demasiadas = true; return; }
        c->img_pos[c->n_img] = c->n;
        llm_buf_add(&c->img[c->n_img], x, n);
        c->n_img++;
        return;
    }
    if (c->n >= TOKS_MAX) return;
    c->n += llm_tokenizar(&c->s->t, x, (size_t)n, tipo == SEG_CONTROLE, c->t + c->n, TOKS_MAX - c->n);
}

static void coletor_libera(coletor_t *c)
{
    for (int i = 0; i < IMAGENS_MAX; i++) llm_buf_libera(&c->img[i]);
    llm_libera(c->t);
}

/* «data:image/...;base64,XXXX» -> bytes. -> bytes (<0 si no vale) */
static int base64_url(const char *u, int n, uint8_t **dados)
{
    int i = 0;
    *dados = NULL;
    if (n < 5 || llm_strncmp(u, "data:", 5)) return -1;
    while (i < n && u[i] != ',') i++;
    if (i >= n || i < 7 || llm_strncmp(u + i - 7, ";base64", 7)) return -1;
    i++;
    uint8_t *d = llm_aloca((size_t)(n - i) / 4 * 3 + 4);
    if (!d) return -2;
    int k = 0, bits = 0;
    uint32_t acc = 0;
    for (; i < n; i++) {
        char c = u[i];
        int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 :
                c == '+' || c == '-' ? 62 : c == '/' || c == '_' ? 63 : -1;
        if (c == '=') break;
        if (v < 0) { if (c == '\n' || c == '\r' || c == ' ') continue; llm_libera(d); return -1; }
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; d[k++] = (uint8_t)(acc >> bits); }
    }
    *dados = d;
    return k;
}

/* Lee la petición en «tk» y la pasa por la red. -> tokens leídos; <0 con el error ya contestado */
static int ler_conversa(llm_sessao_t *s, int fd, const char *corpo, const json_tok_t *tk, const char *sistema, llm_plantilla_t *info)
{
    char erro[200];
    coletor_t c;
    llm_memset(&c, 0, sizeof c);
    c.s = s;
    c.t = llm_aloca(TOKS_MAX * sizeof(int));
    if (!c.t) { erro_json(fd, 400, "sin memoria"); return -1; }
    llm_segs_t sg = { ao_seg, &c };
    if (llm_plantilla(corpo, tk, sistema, &sg, info, erro, sizeof erro)) { coletor_libera(&c); erro_json(fd, 400, erro); return -1; }
    if (c.demasiadas) { coletor_libera(&c); erro_json(fd, 400, "demasiadas imagenes en la conversacion (como mucho 4)"); return -1; }
    int n = c.n;
    if (n >= TOKS_MAX || n + 8 > s->e.kv.ctx) {
        coletor_libera(&c);
        erro_json(fd, 400, "la conversacion no cabe en el contexto de la IA local");
        return -1;
    }
    if (c.n_img && !llm_visao_ativa(s)) {
        coletor_libera(&c);
        erro_json(fd, 400, "la IA local no ve imagenes con este modelo: falta el proyector de vision (mmproj, llm --mmproj)");
        return -1;
    }
    /* ¿Sigue lo que ya está en la caché? (con imágenes, siempre de cero) */
    int base = 0;
    if (!c.n_img && s->hist_n > 0 && s->hist_n < n && s->e.kv.pos == s->hist_n) {
        bool igual = true;
        for (int k = 0; k < s->hist_n && igual; k++) igual = s->hist[k] == c.t[k];
        if (igual) base = s->hist_n;
    }
    if (!base) { llm_estado_reiniciar(&s->e); s->hist_n = 0; }
    unsigned t0 = llm_ms();
    int r = 0, feito = base, n_img_tok = 0;
    for (int i = 0; i < c.n_img && !r; i++) {
        r = llm_alimentar(s, c.t + feito, c.img_pos[i] - feito);
        feito = c.img_pos[i];
        if (r) break;
        uint8_t *d = NULL;
        int nd = c.img[i].p ? base64_url(c.img[i].p, c.img[i].n, &d) : -1;
        if (nd <= 0) { llm_libera(d); coletor_libera(&c); erro_json(fd, 400, "imagen no valida: solo data:image/...;base64,"); return -1; }
        int nt = llm_imagem_alimentar(s, d, (size_t)nd, erro, sizeof erro);
        llm_libera(d);
        if (nt < 0) { coletor_libera(&c); erro_json(fd, 400, erro); return -1; }
        n_img_tok += nt;
    }
    if (!r) r = llm_alimentar(s, c.t + feito, n - feito);
    coletor_libera(&c);
    if (r) { erro_json(fd, 400, "la conversacion no cabe en el contexto de la IA local"); return -1; }
    llm_eprintf("[llm] petición: %d tokens nuevos (%d ya en la caché)%s en %u ms\n", n - base + n_img_tok, base,
                n_img_tok ? " con imágenes" : "", llm_ms() - t0);
    return n + n_img_tok;
}

static void chat(llm_sessao_t *s, int fd, const char *corpo, int ncorpo, const char *sistema)
{
    static json_tok_t tk[16384];
    int nt = json_analisar(corpo, ncorpo, tk, 16384);
    if (nt < 1 || tk[0].tipo != JSON_OBJ) { erro_json(fd, 400, "JSON no valido"); return; }
    int i;
    bool stream = (i = json_campo(corpo, tk, 0, "stream")) >= 0 && json_verdadero(corpo, tk, i);
    int n_max = (i = json_campo(corpo, tk, 0, "max_tokens")) >= 0 ? json_int(corpo, tk, i, 512) : 512;
    if (n_max < 1 || n_max > 8192) n_max = 512;
    float temp_antes = s->a.temperatura;
    s->a.temperatura = numero_json(corpo, tk, json_campo(corpo, tk, 0, "temperature"), temp_antes);
    if ((i = json_campo(corpo, tk, 0, "seed")) >= 0) llm_amostra_iniciar_semente(&s->a, (uint32_t)json_int(corpo, tk, i, 0));

    llm_plantilla_t info;
    int n = ler_conversa(s, fd, corpo, tk, sistema, &info);
    if (n < 0) { s->a.temperatura = temp_antes; return; }

    envio_t e;
    llm_memset(&e, 0, sizeof e);
    e.fd = fd; e.stream = stream; e.herramientas = info.tools >= 0;
    {
        const char *nm = s->m.nome[0] ? s->m.nome : "local";
        json_cadena(e.modelo, sizeof e.modelo, nm, (int)llm_strlen(nm));
    }
    if (info.prefixo[0]) {   /* tool_choice forzado: el modelo sigue desde ahí; eso no se manda como texto */
        llm_buf_txt(&e.todo, info.prefixo);
        e.enviado = e.todo.n;
        e.em_ferramenta = true;
    }
    if (stream) enviar_txt(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");
    llm_saida_t sd;
    llm_memset(&sd, 0, sizeof sd);
    sd.f = ao_texto; sd.ctx = &e; sd.utf8 = true; sd.ocultar_think = true;
    bool cheio;
    int g = llm_gerar(s, n_max, &sd, &cheio);
    if (!cheio && s->ultimo_fim >= 0) llm_fim_turno(s);   /* el <|im_end|> en la caché, para seguir */
    llm_buf_t conteudo = { 0 }, chamadas = { 0 };
    int qtd = e.todo.p ? llm_ferramentas_ler(corpo, tk, &info, e.todo.p, e.todo.n, &conteudo, &chamadas, stream) : 0;
    const char *fim = qtd ? "\"tool_calls\"" : cheio || g >= n_max ? "\"length\"" : "\"stop\"";
    if (stream) {
        mandar_pendente(&e, true);
        if (qtd && chamadas.p) {
            llm_buf_t d = { 0 };
            llm_buf_txt(&d, "{\"tool_calls\":");
            llm_buf_add(&d, chamadas.p, chamadas.n);
            llm_buf_txt(&d, "}");
            if (d.p) sse_dados(&e, d.p, NULL);
            llm_buf_libera(&d);
        }
        sse_dados(&e, "{}", fim);
        enviar_txt(fd, "data: [DONE]\n\n");
    } else {
        llm_buf_t j = { 0 };
        char u[160];
        llm_buf_txt(&j, "{\"id\":\"llm-local\",\"object\":\"chat.completion\",\"model\":");
        llm_buf_txt(&j, e.modelo);
        llm_buf_txt(&j, ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":");
        if (qtd && !conteudo.n) llm_buf_txt(&j, "null");
        else llm_json_cadeia(&j, conteudo.p ? conteudo.p : "", conteudo.n);
        if (qtd && chamadas.p) { llm_buf_txt(&j, ",\"tool_calls\":"); llm_buf_add(&j, chamadas.p, chamadas.n); }
        llm_formatar(u, sizeof u, "},\"finish_reason\":%s}],\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
                     fim, n, g, n + g);
        llm_buf_txt(&j, u);
        if (j.p && !j.sem_memoria) resposta(fd, 200, "application/json", j.p);
        else erro_json(fd, 400, "sin memoria");
        llm_buf_libera(&j);
    }
    llm_buf_libera(&conteudo); llm_buf_libera(&chamadas); llm_buf_libera(&e.todo);
    s->a.temperatura = temp_antes;
}

int llm_servidor(llm_sessao_t *s, int porta, const char *sistema)
{
    int fd = ouvir(porta);
    if (fd < 0) { llm_eprintf("llm: no se puede escuchar en el puerto %d\n", porta); return 1; }
    const char *nome = s->m.nome[0] ? s->m.nome : "local";
    llm_printf("IA local en http://127.0.0.1:%d/v1 (modelo %s, contexto %d). En Configuración > IA: proveedor «llama.cpp».\n",
               porta, nome, s->e.kv.ctx);
    char *b = llm_aloca(CORPO_MAX + 8192);
    if (!b) return 1;
    static char sis_u8[4096];   /* el --sistema llega del terminal (Latin-1 en SaguiOS) */
    if (sistema) { llm_latin1_utf8(sistema, sis_u8, sizeof sis_u8); sistema = sis_u8; }
    for (;;) {
        bool local;
        int c = aceitar(fd, &local);
        if (c < 0) continue;
        if (!local) { fechar_c(c); continue; }   /* solo este equipo */
        int n = 0, cab = -1, clen = 0;
        unsigned t0 = llm_ms();
        while (n < (int)CORPO_MAX + 8000 && llm_ms() - t0 < PRAZO_MS) {
            int r = receber(c, b + n, (int)CORPO_MAX + 8000 - n);
            if (r <= 0) break;
            n += r;
            b[n] = 0;
            if (cab < 0) {
                for (int i = 3; i < n; i++)
                    if (b[i - 3] == '\r' && b[i - 2] == '\n' && b[i - 1] == '\r' && b[i] == '\n') { cab = i + 1; break; }
                bool achou = false;
                if (cab >= 0)
                    for (int i = 0; i < cab - 15 && !achou; i++)
                        if ((b[i] == 'C' || b[i] == 'c') && !llm_strncmp(b + i + 1, "ontent-", 7) &&
                            (b[i + 8] == 'L' || b[i + 8] == 'l') && !llm_strncmp(b + i + 9, "ength:", 6)) {
                            const char *p = b + i + 15;
                            int dig = 0;
                            while (*p == ' ') p++;
                            while (*p >= '0' && *p <= '9' && dig < 7) { clen = clen * 10 + (*p++ - '0'); dig++; }
                            if ((*p >= '0' && *p <= '9') || clen > (int)CORPO_MAX) clen = -1;   /* demasiado grande */
                            achou = true;   /* vale la primera */
                        }
            }
            if (cab >= 0 && n >= cab + clen) break;
        }
        if (cab < 0 || clen < 0) { if (clen < 0) erro_json(c, 400, "cuerpo demasiado grande"); fechar_c(c); continue; }
        if (!llm_strncmp(b, "GET /v1/models", 14) || !llm_strncmp(b, "GET /models", 11)) {
            char j[256], nm[100];
            json_cadena(nm, sizeof nm, nome, (int)llm_strlen(nome));
            llm_formatar(j, sizeof j, "{\"object\":\"list\",\"data\":[{\"id\":%s,\"object\":\"model\",\"owned_by\":\"saguios\"}]}", nm);
            resposta(c, 200, "application/json", j);
        } else if (!llm_strncmp(b, "POST /v1/chat/completions", 25) || !llm_strncmp(b, "POST /chat/completions", 22)) {
            chat(s, c, b + cab, n - cab < clen ? n - cab : clen, sistema);
        } else erro_json(c, 404, "solo /v1/models y /v1/chat/completions");
        fechar_c(c);
    }
}

/* Para las pruebas (h412): manda una petición (JSON de un fichero) a /v1/chat/completions de un
 * `llm --servidor` de este equipo y escribe la respuesta tal cual (cabecera incluida). */
int llm_servidor_pedir(int porta, const char *corpo, size_t n)
{
#ifdef LLM_ANFITRIAO
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)porta); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof a)) { llm_eprintf("llm: no hay servidor en el puerto %d\n", porta); return 1; }
#else
    int fd = conectar_tcp("127.0.0.1", porta, 10000);
    if (fd < 0) { llm_eprintf("llm: no hay servidor en el puerto %d\n", porta); return 1; }
#endif
    char cab[200];
    int k = llm_formatar(cab, sizeof cab, "POST /v1/chat/completions HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                         "Content-Length: %u\r\nConnection: close\r\n\r\n", (unsigned)n);
    enviar(fd, cab, k);
    enviar(fd, corpo, (int)n);
    char b[4096];
    int r;
    while ((r = receber(fd, b, sizeof b)) > 0) llm_escrever(1, b, (size_t)r);
    fechar_c(fd);
    return 0;
}
