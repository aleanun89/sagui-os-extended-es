// SPDX-License-Identifier: GPL-2.0-or-later
/* protocolo.c - sesión de red (TCP + TLS) y los protocolos SMTP, POP3 e IMAP (082).
 *
 * Los tres son de líneas terminadas en CRLF. Cada respuesta se comprueba por su código:
 * SMTP usa códigos de tres cifras (las multilínea llevan guion: "250-..."), POP3 usa
 * "+OK"/"-ERR" e IMAP usa etiquetas (A001 OK ...) con líneas "* ..." sin etiqueta.
 *
 * Sin libc, sin hilos, sin coma flotante. Los búferes grandes van con aloca o static
 * (la pila del proceso es de 64 KB). */
#include "correo.h"

#define LINHA_MAX 1024   /* una línea de protocolo; las más largas se truncan */

/* ================================================================ sesión */

int sessao_abrir(sessao_t *s, const char *host, int porta, bool tls, const tls_config_t *cfg)
{
    memset(s, 0, sizeof *s);
    url_t u;
    memset(&u, 0, sizeof u);
    copia_texto(u.host, host, sizeof u.host);
    u.porta = porta;
    u.https = tls;
    copia_texto(u.caminho, "/", sizeof u.caminho);
    tls_config_t vazio;
    memset(&vazio, 0, sizeof vazio);
    int r = conexao_abrir(&s->con, &u, tls ? cfg : &vazio);
    if (r < 0) {
        copia_texto(s->erro, s->con.erro[0] ? s->con.erro : nome_erro(r), sizeof s->erro);
        return r;
    }
    return 0;
}

/* Rellena el búfer de lectura. Devuelve bytes leídos, 0 = fin, <0 = error. */
static int sessao_recebe(sessao_t *s)
{
    int r = conexao_ler(&s->con, s->rec, sizeof s->rec);
    s->rec_n = r > 0 ? (unsigned)r : 0;
    s->rec_pos = 0;
    return r;
}

int sessao_linha(sessao_t *s, char *buf, unsigned max)
{
    unsigned o = 0;
    for (;;) {
        if (s->rec_pos >= s->rec_n) {
            int r = sessao_recebe(s);
            if (r <= 0) {
                if (o) break;   /* lo que quedó sin CRLF se devuelve tal cual */
                copia_texto(s->erro, r == 0 ? "la conexion se cerro" : "error de lectura", sizeof s->erro);
                buf[0] = 0;
                return r == 0 ? -1 : r;
            }
        }
        char c = s->rec[s->rec_pos++];
        if (c == '\n') break;
        if (c == '\r') continue;
        if (o + 1 < max) buf[o] = c;
        o++;
    }
    if (o >= max) o = max - 1;
    buf[o] = 0;
    return (int)o;
}

int sessao_escrever(sessao_t *s, const void *buf, unsigned n)
{
    int r = conexao_escrever(&s->con, buf, n);
    if (r < 0) copia_texto(s->erro, "error de escritura", sizeof s->erro);
    return r;
}

int sessao_escrever_linha(sessao_t *s, const char *linha)
{
    if (sessao_escrever(s, linha, (unsigned)tam_texto(linha)) < 0) return -1;
    return sessao_escrever(s, "\r\n", 2);
}

/* Pasa la conexión a TLS con el mismo fd (STARTTLS). El búfer de lectura se descarta:
 * tras el "220" de STARTTLS el servidor no manda nada más en claro. */
int sessao_starttls(sessao_t *s, const char *host, const tls_config_t *cfg)
{
    tls_config_t k = *cfg;
    k.host = host;
    s->con.tls = tls_novo(&k, s->con.fd);
    if (!s->con.tls) { copia_texto(s->erro, "sin memoria para TLS", sizeof s->erro); return TLS_ERRO_MEMORIA; }
    s->rec_n = s->rec_pos = 0;
    int r = tls_conectar(s->con.tls);
    if (r < 0) copia_texto(s->erro, tls_erro(s->con.tls), sizeof s->erro);
    return r < 0 ? r : 0;
}

void sessao_fechar(sessao_t *s)
{
    conexao_fechar(&s->con);
}

/* ===================================================== SMTP (enviar) */

/* Lee una respuesta SMTP completa. Devuelve el código de tres cifras (o <0), y deja en
 * "texto" el de la última línea. Las líneas "250-..." siguen hasta "250 ...".
 * Si "todo" no es NULL, recibe la respuesta entera, líneas unidas con '\n': hace falta
 * para leer las extensiones que anuncia el EHLO. */
static int smtp_resposta(sessao_t *s, char *texto, unsigned max, char *todo, unsigned maxtodo)
{
    static char linha[LINHA_MAX];   /* static: no en la pila */
    texto[0] = 0;
    unsigned lt = 0;
    if (todo && maxtodo) todo[0] = 0;
    for (;;) {
        int n = sessao_linha(s, linha, sizeof linha);
        if (n < 0) { copia_texto(texto, s->erro, max); return n; }
        if (n < 3 || !(linha[0] >= '0' && linha[0] <= '9') || !(linha[1] >= '0' && linha[1] <= '9') ||
            !(linha[2] >= '0' && linha[2] <= '9')) {
            copia_texto(texto, linha, max);
            return 0;   /* línea que no es respuesta */
        }
        int codigo = (linha[0] - '0') * 100 + (linha[1] - '0') * 10 + (linha[2] - '0');
        copia_texto(texto, n > 4 ? linha + 4 : "", max);
        if (todo && maxtodo) {
            for (int i = 0; i < n && lt + 2 < maxtodo; i++) todo[lt++] = linha[i];
            if (lt + 1 < maxtodo) todo[lt++] = '\n';
            todo[lt] = 0;
        }
        if (n < 4 || linha[3] != '-') return codigo;
    }
}

/* ¿Está "sub" dentro de "s"? (búsqueda simple, sin regex.) */
static bool contem(const char *s, const char *sub)
{
    if (!s || !sub || !*sub) return false;
    size_t n = tam_texto(sub);
    for (const char *p = s; *p; p++) {
        if (compara_texto_n(p, sub, n) == 0) return true;
    }
    return false;
}

/* Envía una línea y comprueba que la respuesta tenga el código "esperado".
 * Devuelve 0 si coincide; si no, el código recibido (o <0) con el texto en "texto". */
static int smtp_cmd(sessao_t *s, const char *cmd, int esperado, char *texto, unsigned max,
                    char *todo, unsigned maxtodo)
{
    if (cmd && sessao_escrever_linha(s, cmd) < 0) { copia_texto(texto, s->erro, max); return -1; }
    int c = smtp_resposta(s, texto, max, todo, maxtodo);
    if (c < 0) return c;
    return c == esperado ? 0 : c;
}

/* Base64 de una cadena (para AUTH LOGIN). */
static void b64_texto(const char *t, char *dst, unsigned max)
{
    base64_codificar((const uint8_t *)t, tam_texto(t), dst, max);
}

/* Texto de error: "etapa: código texto" o el error de la sesión. */
static void smtp_err(char *erro, unsigned max, const char *etapa, int c, const char *texto)
{
    if (c < 0) formatar_texto(erro, max, "%s: %s", etapa, texto);
    else formatar_texto(erro, max, "%s: %d %s", etapa, c, texto);
}

int smtp_enviar(const smtp_envio_t *e, char *erro, unsigned max)
{
    sessao_t s;
    char texto[256];
    char cmd[320];
    static char b64[512];   /* usuario y clave en base64 */
    int c;

    if (sessao_abrir(&s, e->host, e->porta, e->tls, e->cfg) < 0) {
        copia_texto(erro, s.erro, max);
        return -1;
    }
    /* Saludo: 220. */
    c = smtp_resposta(&s, texto, sizeof texto, NULL, 0);
    if (c != 220) { smtp_err(erro, max, "saludo SMTP", c, texto); goto fim; }

    /* EHLO: el servidor anuncia sus extensiones (STARTTLS, AUTH). */
    static char ehlo[1024];
    c = smtp_cmd(&s, "EHLO saguios", 250, texto, sizeof texto, ehlo, sizeof ehlo);
    if (c) { smtp_err(erro, max, "EHLO", c, texto); goto fim; }

    /* STARTTLS (587): pasa a TLS y repite EHLO, como manda el RFC 3207. Solo si el
     * servidor lo anuncia; y si lo rechaza, se sigue en claro (como hacen los
     * clientes de verdad: muchos servidores de los puertos 25 no lo ofrecen). */
    if (e->starttls && !e->tls && contem(ehlo, "STARTTLS")) {
        c = smtp_cmd(&s, "STARTTLS", 220, texto, sizeof texto, NULL, 0);
        if (c < 0) { smtp_err(erro, max, "STARTTLS", c, texto); goto fim; }
        if (c) {
            /* El servidor no lo quiere (4xx o 5xx): se continúa sin cifrar. */
            c = 0;
        } else {
            if (sessao_starttls(&s, e->host, e->cfg) < 0) { copia_texto(erro, s.erro, max); goto fim; }
            c = smtp_cmd(&s, "EHLO saguios", 250, texto, sizeof texto, ehlo, sizeof ehlo);
            if (c) { smtp_err(erro, max, "EHLO tras STARTTLS", c, texto); goto fim; }
        }
    }

    /* AUTH LOGIN: el servidor pide el usuario (334) y luego la clave (334), y da 235. */
    if (e->usuario && e->usuario[0]) {
        c = smtp_cmd(&s, "AUTH LOGIN", 334, texto, sizeof texto, NULL, 0);
        if (c) { smtp_err(erro, max, "AUTH", c, texto); goto fim; }
        b64_texto(e->usuario, b64, sizeof b64);
        c = smtp_cmd(&s, b64, 334, texto, sizeof texto, NULL, 0);
        if (c) { smtp_err(erro, max, "usuario", c, texto); goto fim; }
        b64_texto(e->clave ? e->clave : "", b64, sizeof b64);
        c = smtp_cmd(&s, b64, 235, texto, sizeof texto, NULL, 0);
        if (c) { smtp_err(erro, max, "clave", c, texto); goto fim; }
    }

    formatar_texto(cmd, sizeof cmd, "MAIL FROM:<%s>", e->de);
    c = smtp_cmd(&s, cmd, 250, texto, sizeof texto, NULL, 0);
    if (c) { smtp_err(erro, max, "MAIL FROM", c, texto); goto fim; }
    formatar_texto(cmd, sizeof cmd, "RCPT TO:<%s>", e->para);
    c = smtp_cmd(&s, cmd, 250, texto, sizeof texto, NULL, 0);
    if (c) { smtp_err(erro, max, "RCPT TO", c, texto); goto fim; }

    /* DATA: responde 354 y espera el mensaje, que acaba con "." en una línea sola. */
    c = smtp_cmd(&s, "DATA", 354, texto, sizeof texto, NULL, 0);
    if (c) { smtp_err(erro, max, "DATA", c, texto); goto fim; }

    /* Cuerpo: las líneas que empiezan por "." se duplican (transparencia, RFC 5321). */
    const char *m = e->mensaje;
    unsigned n = e->nmensaje, i = 0;
    bool inicio = true;
    while (i < n) {
        if (inicio && m[i] == '.' && sessao_escrever(&s, ".", 1) < 0) goto erro_rede;
        unsigned fim = i;
        while (fim < n && m[fim] != '\n') fim++;
        unsigned k = fim < n ? fim + 1 : n;   /* incluye el "\n" */
        if (sessao_escrever(&s, m + i, k - i) < 0) goto erro_rede;
        inicio = m[k - 1] == '\n';
        i = k;
    }
    if (n && m[n - 1] != '\n' && sessao_escrever(&s, "\r\n", 2) < 0) goto erro_rede;
    c = smtp_cmd(&s, ".", 250, texto, sizeof texto, NULL, 0);
    if (c) { smtp_err(erro, max, "mensaje rechazado", c, texto); goto fim; }

    smtp_cmd(&s, "QUIT", 221, texto, sizeof texto, NULL, 0);   /* el cierre no cambia el resultado */
    sessao_fechar(&s);
    copia_texto(erro, "", max);
    return 0;

erro_rede:
    copia_texto(erro, s.erro, max);
fim:
    sessao_fechar(&s);
    return -1;
}

/* ===================================================== POP3 (recibir) */

/* Comando POP3 y su respuesta en "texto". 0 = "+OK"; si no, -1 y el texto de -ERR. */
static int pop_cmd(sessao_t *s, const char *cmd, char *texto, unsigned max)
{
    if (cmd && sessao_escrever_linha(s, cmd) < 0) { copia_texto(texto, s->erro, max); return -1; }
    int n = sessao_linha(s, texto, max);
    if (n < 0) return -1;
    return (texto[0] == '+' && texto[1] == 'O' && texto[2] == 'K') ? 0 : -1;
}

/* Lee el cuerpo multilínea de RETR hasta la línea ".". Quita el punto de transparencia y
 * deja las líneas con CRLF. Devuelve un búfer aloca'd en *dados (libera el llamante). */
static int pop_multilinha(sessao_t *s, char **dados, unsigned *ndados, char *erro, unsigned max)
{
    unsigned cap = 16384, n = 0;
    char *b = aloca(cap);
    if (!b) { copia_texto(erro, "sin memoria", max); return -1; }
    static char linha[LINHA_MAX];
    for (;;) {
        int k = sessao_linha(s, linha, sizeof linha);
        if (k < 0) { copia_texto(erro, s->erro, max); libera(b); return -1; }
        if (k == 1 && linha[0] == '.') break;   /* fin de la respuesta */
        const char *p = linha;
        unsigned lk = (unsigned)k;
        if (linha[0] == '.') { p++; lk--; }     /* "..x" -> ".x" */
        if (n + lk + 2 + 1 > cap) {
            unsigned novo = cap * 2;
            char *nb = realoca(b, novo);
            if (!nb) { copia_texto(erro, "sin memoria", max); libera(b); return -1; }
            b = nb;
            cap = novo;
        }
        memcpy(b + n, p, lk);
        n += lk;
        b[n++] = '\r';
        b[n++] = '\n';
    }
    b[n] = 0;
    *dados = b;
    *ndados = n;
    return (int)n;
}

int pop3_receber(const buzon_cfg_t *b, mensagem_fn f, void *ctx, char *erro, unsigned max)
{
    sessao_t s;
    char texto[256];
    char cmd[320];
    int entregues = -1;
    if (sessao_abrir(&s, b->host, b->porta, b->tls, b->cfg) < 0) { copia_texto(erro, s.erro, max); return -1; }

    /* Saludo "+OK ...". */
    if (pop_cmd(&s, NULL, texto, sizeof texto) != 0) { formatar_texto(erro, max, "saludo POP3: %s", texto); goto fim; }
    formatar_texto(cmd, sizeof cmd, "USER %s", b->usuario ? b->usuario : "");
    if (pop_cmd(&s, cmd, texto, sizeof texto) != 0) { formatar_texto(erro, max, "USER: %s", texto); goto fim; }
    formatar_texto(cmd, sizeof cmd, "PASS %s", b->clave ? b->clave : "");
    if (pop_cmd(&s, cmd, texto, sizeof texto) != 0) { formatar_texto(erro, max, "autenticacion: %s", texto); goto fim; }

    /* STAT -> "+OK <nmensajes> <tamaño>". */
    if (pop_cmd(&s, "STAT", texto, sizeof texto) != 0) { formatar_texto(erro, max, "STAT: %s", texto); goto fim; }
    int total = texto_para_int(texto + 4);

    entregues = 0;
    for (int i = 1; i <= total; i++) {
        formatar_texto(cmd, sizeof cmd, "RETR %d", i);
        if (pop_cmd(&s, cmd, texto, sizeof texto) != 0) { formatar_texto(erro, max, "RETR %d: %s", i, texto); entregues = -1; goto fim; }
        char *dados = NULL;
        unsigned nd = 0;
        if (pop_multilinha(&s, &dados, &nd, erro, max) < 0) { entregues = -1; goto fim; }
        int r = f(ctx, i, dados, nd);
        libera(dados);
        if (r < 0) { copia_texto(erro, "cancelado", max); entregues = -1; goto fim; }
        entregues++;
    }
    pop_cmd(&s, "QUIT", texto, sizeof texto);

fim:
    sessao_fechar(&s);
    return entregues;
}

/* ===================================================== IMAP (recibir) */

/* Comando IMAP con etiqueta. Lee hasta la línea con la etiqueta, que debe ser "OK".
 * Las líneas "* n EXISTS" dan el número de mensajes en *existentes (si no es NULL).
 * Los literales {N} de líneas sin etiqueta se saltan (no hacen falta aquí). */
static int imap_cmd(sessao_t *s, const char *eti, const char *cmd, int *existentes, char *erro, unsigned max);

/* Lee exactamente n bytes de la sesión a "dst" (o los descarta si dst es NULL). */
static int imap_ler_bytes(sessao_t *s, char *dst, unsigned n)
{
    unsigned got = 0;
    while (got < n) {
        if (s->rec_pos >= s->rec_n && sessao_recebe(s) <= 0) {
            copia_texto(s->erro, "conexion cortada en un literal", sizeof s->erro);
            return -1;
        }
        unsigned disp = s->rec_n - s->rec_pos, falta = n - got;
        unsigned cop = disp < falta ? disp : falta;
        if (dst) memcpy(dst + got, s->rec + s->rec_pos, cop);
        got += cop;
        s->rec_pos += cop;
    }
    return 0;
}

/* Si la línea acaba en "{N}" (un literal que sigue), devuelve N; si no, -1. */
static int imap_literal_final(const char *l, int n)
{
    if (n < 3 || l[n - 1] != '}') return -1;
    int i = n - 2;
    while (i >= 0 && l[i] >= '0' && l[i] <= '9') i--;
    if (i < 0 || l[i] != '{' || i == n - 2) return -1;
    return texto_para_int(l + i + 1);
}

static int imap_cmd(sessao_t *s, const char *eti, const char *cmd, int *existentes, char *erro, unsigned max)
{
    char linha[600];
    formatar_texto(linha, sizeof linha, "%s %s", eti, cmd);
    if (sessao_escrever_linha(s, linha) < 0) { copia_texto(erro, s->erro, max); return -1; }
    static char r[LINHA_MAX];
    unsigned etl = (unsigned)tam_texto(eti);
    for (;;) {
        int n = sessao_linha(s, r, sizeof r);
        if (n < 0) { copia_texto(erro, s->erro, max); return -1; }
        if ((unsigned)n > etl && compara_texto_n(r, eti, etl) == 0 && r[etl] == ' ') {
            if (compara_texto_n(r + etl + 1, "OK", 2) == 0) return 0;
            formatar_texto(erro, max, "%s", r + etl + 1);
            return -2;
        }
        /* "* n EXISTS": cuántos mensajes hay en la carpeta. */
        if (existentes && r[0] == '*' && r[1] == ' ') {
            int v = texto_para_int(r + 2);
            if (v > 0 && acha_char(r + 2, 'E') && compara_texto_n(acha_char(r + 2, 'E'), "EXISTS", 6) == 0)
                *existentes = v;
        }
        int lit = imap_literal_final(r, n);
        if (lit >= 0 && imap_ler_bytes(s, NULL, (unsigned)lit) < 0) { copia_texto(erro, s->erro, max); return -1; }
    }
}

/* Pide el mensaje i con BODY.PEEK[] (no lo marca como leído) y lo devuelve en *dados.
 * El literal de la respuesta trae el mensaje entero. */
static int imap_fetch(sessao_t *s, const char *eti, int i, char **dados, unsigned *ndados, char *erro, unsigned max)
{
    char linha[64];
    formatar_texto(linha, sizeof linha, "FETCH %d (BODY.PEEK[])", i);
    char cmd[80];
    formatar_texto(cmd, sizeof cmd, "%s", linha);
    char enviada[120];
    formatar_texto(enviada, sizeof enviada, "%s %s", eti, cmd);
    if (sessao_escrever_linha(s, enviada) < 0) { copia_texto(erro, s->erro, max); return -1; }

    *dados = NULL;
    *ndados = 0;
    static char r[LINHA_MAX];
    unsigned etl = (unsigned)tam_texto(eti);
    for (;;) {
        int n = sessao_linha(s, r, sizeof r);
        if (n < 0) { copia_texto(erro, s->erro, max); libera(*dados); *dados = NULL; return -1; }
        if ((unsigned)n > etl && compara_texto_n(r, eti, etl) == 0 && r[etl] == ' ') {
            if (compara_texto_n(r + etl + 1, "OK", 2) == 0) return 0;
            formatar_texto(erro, max, "FETCH %d: %s", i, r + etl + 1);
            libera(*dados);
            *dados = NULL;
            return -2;
        }
        int lit = imap_literal_final(r, n);
        if (lit >= 0) {
            /* La línea "* i FETCH (BODY[] {N}" es seguida de N bytes del mensaje. */
            char *b = aloca((unsigned)lit + 2);
            if (!b) { copia_texto(erro, "sin memoria", max); return -1; }
            if (imap_ler_bytes(s, b, (unsigned)lit) < 0) { libera(b); copia_texto(erro, s->erro, max); return -1; }
            b[lit] = 0;
            libera(*dados);
            *dados = b;
            *ndados = (unsigned)lit;
        }
    }
}

int imap_receber(const buzon_cfg_t *b, mensagem_fn f, void *ctx, char *erro, unsigned max)
{
    sessao_t s;
    static char r[LINHA_MAX];
    int existentes = 0;
    int entregues = -1;
    if (sessao_abrir(&s, b->host, b->porta, b->tls, b->cfg) < 0) { copia_texto(erro, s.erro, max); return -1; }

    /* Saludo "* OK ...". */
    if (sessao_linha(&s, r, sizeof r) < 0 || r[0] != '*') { formatar_texto(erro, max, "saludo IMAP: %s", r); goto fim; }

    /* LOGIN con el usuario y la clave entre comillas (se escapan " y \). */
    {
        static char cmd[300];
        char u[128], p[128];
        unsigned o = 0;
        for (const char *q = b->usuario ? b->usuario : ""; *q && o + 2 < sizeof u; q++) { if (*q == '"' || *q == '\\') u[o++] = '\\'; u[o++] = *q; }
        u[o] = 0;
        o = 0;
        for (const char *q = b->clave ? b->clave : ""; *q && o + 2 < sizeof p; q++) { if (*q == '"' || *q == '\\') p[o++] = '\\'; p[o++] = *q; }
        p[o] = 0;
        formatar_texto(cmd, sizeof cmd, "LOGIN \"%s\" \"%s\"", u, p);
        if (imap_cmd(&s, "A001", cmd, NULL, erro, max) != 0) {
            if (!erro[0]) copia_texto(erro, "autenticacion", max);
            goto fim;
        }
    }

    /* SELECT INBOX: la respuesta trae "* n EXISTS" con el número de mensajes. */
    if (imap_cmd(&s, "A002", "SELECT INBOX", &existentes, erro, max) != 0) goto fim;

    entregues = 0;
    for (int i = 1; i <= existentes; i++) {
        char eti[16];
        formatar_texto(eti, sizeof eti, "A%03d", i + 2);
        char *dados = NULL;
        unsigned nd = 0;
        if (imap_fetch(&s, eti, i, &dados, &nd, erro, max) != 0) { entregues = -1; libera(dados); goto fim; }
        if (!dados) { formatar_texto(erro, max, "FETCH %d sin cuerpo", i); entregues = -1; goto fim; }
        int r2 = f(ctx, i, dados, nd);
        libera(dados);
        if (r2 < 0) { copia_texto(erro, "cancelado", max); entregues = -1; goto fim; }
        entregues++;
    }
    imap_cmd(&s, "A999", "LOGOUT", NULL, erro, max);

fim:
    sessao_fechar(&s);
    return entregues;
}
