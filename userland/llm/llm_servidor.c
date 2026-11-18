// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_servidor.c - `llm --servidor` (D380): la IA local como un servidor compatible con la
 * API de OpenAI, para que `ia`, `chatia` y los agentes de Pluma, Macaco y Correo la usen sin
 * cambios (proveedor «llama.cpp», http://127.0.0.1:8080/v1).
 *
 *   GET  /v1/models              el modelo cargado
 *   POST /v1/chat/completions    messages, stream, max_tokens, temperature, seed
 *
 * Solo atiende conexiones de 127.0.0.1 (el modelo es del equipo, no de la red) y una
 * petición cada vez. Las herramientas («tools») aún no: se contesta 400 con «tools» en el
 * mensaje y el cliente de la libsagui repite sin ellas (ia.c, erro_de_ferramentas).
 * Entre peticiones se conserva la caché: si la conversación nueva empieza por todo lo que ya
 * está en la caché (lo normal en un chat), solo se lee lo que falta. El estado de la
 * convolución no se puede rebobinar, así que con cualquier otra cosa se empieza de cero. */
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
/* El lector de JSON de los agentes (userland/ia/json.c, D295) entra aquí tal cual: así `llm`
 * no enlaza toda la biblioteca de IA (TLS incluido) solo por él. */
#include "../ia/json.c"

#define CORPO_MAX (1u << 20)
#define TOKS_MAX 8192
#define TEXTO_MAX (64 * 1024)
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
    char modelo[200];   /* ya como cadena JSON (con comillas y escapes) */
    char buf[512]; int n;               /* texto pendiente de enviar (se agrupa un poco) */
    char *todo; int todo_n, todo_max;   /* sin stream: la respuesta entera */
} envio_t;

static void sse_enviar(envio_t *e, const char *t, int n, const char *fim)
{
    char j[3300], c[3100];
    json_cadena(c, sizeof c, t, n);
    int k = llm_formatar(j, sizeof j, "data: {\"id\":\"llm-local\",\"object\":\"chat.completion.chunk\",\"model\":%s,"
                         "\"choices\":[{\"index\":0,\"delta\":{\"content\":%s},\"finish_reason\":%s}]}\n\n",
                         e->modelo, c, fim ? fim : "null");
    enviar(e->fd, j, k);
}

static void ao_texto(void *ctx, const char *t, int n)
{
    envio_t *e = ctx;
    if (!e->stream) {
        if (e->todo_n + n + 1 > e->todo_max) return;
        llm_memcpy(e->todo + e->todo_n, t, (size_t)n);
        e->todo_n += n;
        return;
    }
    for (int i = 0; i < n; i++) {
        e->buf[e->n++] = t[i];
        if (e->n >= (int)sizeof e->buf - 8 || t[i] == ' ' || t[i] == '\n') { sse_enviar(e, e->buf, e->n, NULL); e->n = 0; }
    }
}

/* Junta los tokens de un trozo de texto. */
static int juntar(llm_sessao_t *s, int *t, int n, const char *txt, bool esp)
{
    if (n >= TOKS_MAX) return n;
    return n + llm_tokenizar(&s->t, txt, llm_strlen(txt), esp, t + n, TOKS_MAX - n);
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

static void chat(llm_sessao_t *s, int fd, const char *corpo, int ncorpo, const char *sistema)
{
    static json_tok_t tk[8192];
    int nt = json_analisar(corpo, ncorpo, tk, 8192);
    if (nt < 1 || tk[0].tipo != JSON_OBJ) { erro_json(fd, 400, "JSON no valido"); return; }
    int tools = json_campo(corpo, tk, 0, "tools");
    if (tools >= 0 && tk[tools].tipo == JSON_ARR && tk[tools].n > 0) { erro_json(fd, 400, "tools: la IA local aun no usa herramientas"); return; }
    int msgs = json_campo(corpo, tk, 0, "messages");
    if (msgs < 0 || tk[msgs].tipo != JSON_ARR) { erro_json(fd, 400, "faltan los messages"); return; }
    int i;
    bool stream = (i = json_campo(corpo, tk, 0, "stream")) >= 0 && json_verdadero(corpo, tk, i);
    int n_max = (i = json_campo(corpo, tk, 0, "max_tokens")) >= 0 ? json_int(corpo, tk, i, 512) : 512;
    if (n_max < 1 || n_max > 8192) n_max = 512;
    float temp_antes = s->a.temperatura;
    s->a.temperatura = numero_json(corpo, tk, json_campo(corpo, tk, 0, "temperature"), temp_antes);
    if ((i = json_campo(corpo, tk, 0, "seed")) >= 0) llm_amostra_iniciar_semente(&s->a, (uint32_t)json_int(corpo, tk, i, 0));

    /* la conversación con la plantilla de LFM3 */
    int *t = llm_aloca(TOKS_MAX * sizeof(int));
    char *txt = llm_aloca(TEXTO_MAX);
    if (!t || !txt) { llm_libera(t); llm_libera(txt); erro_json(fd, 400, "sin memoria"); s->a.temperatura = temp_antes; return; }
    int n = juntar(s, t, 0, "<|startoftext|>", true);
    bool tem_sistema = false;
    for (int k = 0; k < tk[msgs].n && n < TOKS_MAX; k++) {
        int m = json_elem(tk, msgs, k);
        int pr = json_campo(corpo, tk, m, "role"), ct = json_campo(corpo, tk, m, "content");
        char papel[16] = "user";
        if (pr >= 0) json_texto(corpo, tk, pr, papel, sizeof papel);
        txt[0] = 0;
        if (ct >= 0 && tk[ct].tipo == JSON_STR) json_texto(corpo, tk, ct, txt, TEXTO_MAX);
        else if (ct >= 0 && tk[ct].tipo == JSON_ARR) {   /* [{"type":"text","text":...}] */
            int L = 0;
            for (int q = 0; q < tk[ct].n && L < TEXTO_MAX - 1; q++) {
                int it = json_elem(tk, ct, q), tx = json_campo(corpo, tk, it, "text");
                if (tx >= 0) { int r = json_texto(corpo, tk, tx, txt + L, TEXTO_MAX - L); if (r > 0) L += r; }
            }
        }
        /* solo los papeles de la plantilla: nada de tokens especiales metidos en «role» */
        if (llm_strcmp(papel, "system") && llm_strcmp(papel, "user") && llm_strcmp(papel, "assistant") && llm_strcmp(papel, "tool"))
            llm_formatar(papel, sizeof papel, "user");
        if (!llm_strcmp(papel, "system")) tem_sistema = true;
        if (!llm_strcmp(papel, "assistant")) {   /* lo pensado no vuelve a la conversación */
            const char *f = txt;
            for (const char *p = txt; *p; p++) if (!llm_strncmp(p, "</think>", 8)) f = p + 8;
            while (*f == ' ' || *f == '\n') f++;
            size_t L = llm_strlen(f);
            for (size_t q = 0; q <= L; q++) txt[q] = f[q];
        }
        if (k == 0 && !tem_sistema && sistema && *sistema) {
            n = juntar(s, t, n, "<|im_start|>system\n", true);
            n = juntar(s, t, n, sistema, false);
            n = juntar(s, t, n, "<|im_end|>\n", true);
        }
        char cab[64];
        /* Las respuestas anteriores, como el modelo las generó (con el <think></think> vacío): así la
         * conversación nueva empieza por lo que ya está en la caché y no se vuelve a leer. */
        llm_formatar(cab, sizeof cab, "<|im_start|>%s\n%s", papel, llm_strcmp(papel, "assistant") ? "" : "<think></think>");
        n = juntar(s, t, n, cab, true);
        n = juntar(s, t, n, txt, false);
        n = juntar(s, t, n, "<|im_end|>\n", true);
    }
    n = juntar(s, t, n, "<|im_start|>assistant\n<think></think>", true);
    llm_libera(txt);
    if (n >= TOKS_MAX || n + 8 > s->e.kv.ctx) {
        llm_libera(t);
        erro_json(fd, 400, "la conversacion no cabe en el contexto de la IA local");
        s->a.temperatura = temp_antes;
        return;
    }
    /* ¿Sigue lo que ya está en la caché? */
    int base = 0;
    if (s->hist_n > 0 && s->hist_n < n && s->e.kv.pos == s->hist_n) {
        bool igual = true;
        for (int k = 0; k < s->hist_n && igual; k++) igual = s->hist[k] == t[k];
        if (igual) base = s->hist_n;
    }
    if (!base) { llm_estado_reiniciar(&s->e); s->hist_n = 0; }
    unsigned t0 = llm_ms();
    int r = llm_alimentar(s, t + base, n - base);
    llm_libera(t);
    if (r) { erro_json(fd, 400, "la conversacion no cabe en el contexto de la IA local"); s->a.temperatura = temp_antes; return; }
    llm_eprintf("[llm] petición: %d tokens nuevos (%d ya en la caché) en %u ms\n", n - base, base, llm_ms() - t0);

    envio_t e;
    llm_memset(&e, 0, sizeof e);
    e.fd = fd; e.stream = stream;
    {
        const char *nm = s->m.nome[0] ? s->m.nome : "local";
        json_cadena(e.modelo, sizeof e.modelo, nm, (int)llm_strlen(nm));
    }
    if (stream) {
        enviar_txt(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n");
    } else {
        e.todo_max = 256 * 1024;
        e.todo = llm_aloca((size_t)e.todo_max);
        if (!e.todo) { erro_json(fd, 400, "sin memoria"); s->a.temperatura = temp_antes; return; }
    }
    llm_saida_t sd;
    llm_memset(&sd, 0, sizeof sd);
    sd.f = ao_texto; sd.ctx = &e; sd.utf8 = true; sd.ocultar_think = true;
    bool cheio;
    int g = llm_gerar(s, n_max, &sd, &cheio);
    const char *fim = cheio || g >= n_max ? "\"length\"" : "\"stop\"";
    if (!cheio && s->ultimo_fim >= 0) llm_fim_turno(s);   /* el <|im_end|> en la caché, para seguir */
    if (stream) {
        if (e.n) sse_enviar(&e, e.buf, e.n, NULL);
        sse_enviar(&e, "", 0, fim);
        enviar_txt(fd, "data: [DONE]\n\n");
    } else {
        int max = e.todo_n * 6 + 1024;
        char *j = llm_aloca((size_t)max), *c = llm_aloca((size_t)e.todo_n * 6 + 8);
        if (j && c) {
            json_cadena(c, e.todo_n * 6 + 8, e.todo, e.todo_n);
            llm_formatar(j, (size_t)max, "{\"id\":\"llm-local\",\"object\":\"chat.completion\",\"model\":%s,\"choices\":[{\"index\":0,"
                         "\"message\":{\"role\":\"assistant\",\"content\":%s},\"finish_reason\":%s}],"
                         "\"usage\":{\"prompt_tokens\":%d,\"completion_tokens\":%d,\"total_tokens\":%d}}",
                         e.modelo, c, fim, n, g, n + g);
            resposta(fd, 200, "application/json", j);
        }
        llm_libera(j); llm_libera(c); llm_libera(e.todo);
    }
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
