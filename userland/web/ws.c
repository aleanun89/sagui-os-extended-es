// SPDX-License-Identifier: GPL-2.0-or-later
/* ws.c - WebSocket de cliente (RFC 6455) para el Cipó, sobre TCP o TLS (D487).
 *
 * El apretón de manos se hace de forma bloqueante al abrir; después la conexión se sondea sin
 * esperar (ws_receber) desde el temporizador del JavaScript, así que no hay hilos. Las tramas
 * del cliente van siempre enmascaradas. Si la conexión es wss, cada operación TLS toma el
 * mismo cerrojo que las descargas HTTPS (la TLS no es reentrante). */
#include "rede.h"
#include "tls.h"

#define WS_MAX 8
#define WS_COLA 64
#define WS_MSG_MAX (4 * 1024 * 1024)

typedef struct { uint8_t *d; int n, tipo, cod; } msg_t;
typedef struct {
    bool uso;
    int estado;                 /* 0 conectando, 1 abierto, 2 cerrando, 3 cerrado */
    conexao_t cn;
    bool tls;
    uint8_t *rx; int rxn, rxcap;
    uint8_t *frag; int fragn, fragcap, fragtipo;
    msg_t cola[WS_COLA]; int cola_ini, cola_n;
    bool cierre_entregado, cierre_enviado;
    int cod_cierre;
    char protocolo[48];
} ws_t;
static ws_t sockets[WS_MAX];
static tls_config_t cfg;
static mutex_t *tls_lock;

void ws_definir_trava(mutex_t *m) { tls_lock = m; }

/* ---- SHA-1 y base64 (solo para el apretón de manos) ---- */
static uint32_t rol(uint32_t v, int n) { return v << n | v >> (32 - n); }

static void sha1(const uint8_t *d, int n, uint8_t o[20])
{
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    uint8_t blk[64];
    int total = n + 1 + 8, pad = (64 - total % 64) % 64;
    total += pad;
    for (int off = 0; off < total; off += 64) {
        for (int i = 0; i < 64; i++) {
            int p = off + i;
            uint8_t b = 0;
            if (p < n) b = d[p];
            else if (p == n) b = 0x80;
            else if (p >= total - 8) { int sh = (total - 1 - p) * 8; b = sh < 32 ? (uint8_t)(((uint32_t)n * 8u) >> sh) : 0; }
            blk[i] = b;
        }
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)blk[i * 4] << 24 | (uint32_t)blk[i * 4 + 1] << 16 | (uint32_t)blk[i * 4 + 2] << 8 | blk[i * 4 + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & dd); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ dd; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & dd) | (c & dd); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ dd; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = dd; dd = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += dd; h[4] += e;
    }
    for (int i = 0; i < 5; i++) { o[i * 4] = (uint8_t)(h[i] >> 24); o[i * 4 + 1] = (uint8_t)(h[i] >> 16); o[i * 4 + 2] = (uint8_t)(h[i] >> 8); o[i * 4 + 3] = (uint8_t)h[i]; }
}

static void base64(const uint8_t *d, int n, char *o)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int k = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o[k++] = t[v >> 18 & 63]; o[k++] = t[v >> 12 & 63];
        o[k++] = i + 1 < n ? t[v >> 6 & 63] : '=';
        o[k++] = i + 2 < n ? t[v & 63] : '=';
    }
    o[k] = 0;
}

/* ---- E/S con o sin TLS ---- */
static void trava_tomar(ws_t *w) { if (w->tls && tls_lock) mutex_tomar(tls_lock); }
static void trava_largar(ws_t *w) { if (w->tls && tls_lock) mutex_largar(tls_lock); }

static int es_escribir(ws_t *w, const void *b, int n)
{
    trava_tomar(w);
    int r = conexao_escrever(&w->cn, b, (unsigned)n);
    trava_largar(w);
    return r;
}

static bool hay_datos(ws_t *w)
{
    if (w->cn.tls && tls_pendente(w->cn.tls) > 0) return true;
    sondagem_t v = { w->cn.fd, SONDA_LER, 0 };
    int r = sondar(&v, 1, 0);
    return r > 0 && (v.prontos & (SONDA_LER | SONDA_FECHADO | SONDA_ERRO));
}

static bool rx_crecer(ws_t *w, int mas)
{
    if (w->rxn + mas <= w->rxcap) return true;
    int nc = w->rxcap ? w->rxcap : 4096;
    while (nc < w->rxn + mas) nc *= 2;
    uint8_t *p = realoca(w->rx, (size_t)nc);
    if (!p) return false;
    w->rx = p; w->rxcap = nc;
    return true;
}

static void encolar(ws_t *w, int tipo, uint8_t *d, int n, int cod)
{
    if (w->cola_n >= WS_COLA) { libera(d); return; }
    msg_t *m = &w->cola[(w->cola_ini + w->cola_n) % WS_COLA];
    m->d = d; m->n = n; m->tipo = tipo; m->cod = cod;
    w->cola_n++;
}

static void enviar_trama(ws_t *w, int opcode, const uint8_t *d, int n)
{
    uint8_t *f = aloca((size_t)n + 14);
    if (!f) return;
    int k = 0;
    f[k++] = (uint8_t)(0x80 | opcode);
    if (n < 126) f[k++] = (uint8_t)(0x80 | n);
    else if (n < 65536) { f[k++] = 0x80 | 126; f[k++] = (uint8_t)(n >> 8); f[k++] = (uint8_t)n; }
    else { f[k++] = 0x80 | 127; f[k++] = 0; f[k++] = 0; f[k++] = 0; f[k++] = 0; f[k++] = (uint8_t)(n >> 24); f[k++] = (uint8_t)(n >> 16); f[k++] = (uint8_t)(n >> 8); f[k++] = (uint8_t)n; }
    uint8_t mk[4];
    bytes_aleatorios(mk, 4);
    memcpy(f + k, mk, 4); k += 4;
    for (int i = 0; i < n; i++) f[k + i] = d[i] ^ mk[i & 3];
    es_escribir(w, f, k + n);
    libera(f);
}

static void cerrar_socket(ws_t *w, int cod)
{
    if (w->estado == 3) return;
    w->estado = 3;
    w->cod_cierre = cod;
    trava_tomar(w);
    conexao_fechar(&w->cn);
    trava_largar(w);
}

/* Interpreta las tramas completas del búfer de entrada. */
static void analizar(ws_t *w)
{
    for (;;) {
        if (w->rxn < 2) return;
        uint8_t b0 = w->rx[0], b1 = w->rx[1];
        int op = b0 & 15, mask = (b1 & 0x80) != 0;
        uint32_t len = b1 & 0x7F;
        int cab = 2;
        if (len == 126) { if (w->rxn < 4) return; len = (uint32_t)w->rx[2] << 8 | w->rx[3]; cab = 4; }
        else if (len == 127) {
            if (w->rxn < 10) return;
            if (w->rx[2] | w->rx[3] | w->rx[4] | w->rx[5]) { cerrar_socket(w, 1009); return; }
            len = (uint32_t)w->rx[6] << 24 | (uint32_t)w->rx[7] << 16 | (uint32_t)w->rx[8] << 8 | w->rx[9];
            cab = 10;
        }
        if (len > WS_MSG_MAX) { cerrar_socket(w, 1009); return; }
        if (mask) cab += 4;
        if (w->rxn < cab + (int)len) return;
        uint8_t *pl = w->rx + cab;
        if (mask) for (uint32_t i = 0; i < len; i++) pl[i] ^= w->rx[cab - 4 + (i & 3)];
        bool fin = (b0 & 0x80) != 0;
        if (op == 8) {
            int cod = len >= 2 ? pl[0] << 8 | pl[1] : 1005;
            if (!w->cierre_enviado) { w->cierre_enviado = true; enviar_trama(w, 8, pl, (int)(len >= 2 ? 2 : 0)); }
            cerrar_socket(w, cod);
            return;
        } else if (op == 9) {
            enviar_trama(w, 10, pl, (int)len);
        } else if (op == 10) {
            /* pong: se ignora */
        } else if (op == 0 || op == 1 || op == 2) {
            if (op != 0) { w->fragn = 0; w->fragtipo = op; }
            if (w->fragn + (int)len > WS_MSG_MAX) { cerrar_socket(w, 1009); return; }
            if (w->fragn + (int)len + 1 > w->fragcap) {
                int nc = w->fragcap ? w->fragcap : 1024;
                while (nc < w->fragn + (int)len + 1) nc *= 2;
                uint8_t *p = realoca(w->frag, (size_t)nc);
                if (!p) { cerrar_socket(w, 1011); return; }
                w->frag = p; w->fragcap = nc;
            }
            memcpy(w->frag + w->fragn, pl, len);
            w->fragn += (int)len;
            if (fin) {
                uint8_t *d = aloca((size_t)w->fragn + 1);
                if (d) { memcpy(d, w->frag, (size_t)w->fragn); d[w->fragn] = 0; encolar(w, w->fragtipo, d, w->fragn, 0); }
                w->fragn = 0;
            }
        }
        int total = cab + (int)len;
        memmove(w->rx, w->rx + total, (size_t)(w->rxn - total));
        w->rxn -= total;
    }
}

/* Lee lo que haya sin esperar. */
static void bombear(ws_t *w)
{
    if (w->estado == 3 || w->estado == 0) return;
    for (int vuelta = 0; vuelta < 16 && w->estado != 3; vuelta++) {
        if (!hay_datos(w)) break;
        if (!rx_crecer(w, 4096)) { cerrar_socket(w, 1011); break; }
        trava_tomar(w);
        int n = conexao_ler(&w->cn, w->rx + w->rxn, 4096);
        trava_largar(w);
        if (n <= 0) { cerrar_socket(w, 1006); break; }
        w->rxn += n;
        analizar(w);
    }
}

int ws_abrir(const char *url, const char *origem, const char *protos, char *erro, int emax)
{
    erro[0] = 0;
    char http[REDE_URL_MAX];
    if (comeca_com(url, "ws://")) formatar_texto(http, sizeof http, "http://%s", url + 5);
    else if (comeca_com(url, "wss://")) formatar_texto(http, sizeof http, "https://%s", url + 6);
    else { copia_texto(erro, "esquema no válido", (size_t)emax); return -1; }
    url_t u;
    if (!url_separar(http, &u)) { copia_texto(erro, "dirección no válida", (size_t)emax); return -1; }
    /* mixto: una página https no abre ws:// */
    if (origem && comeca_com(origem, "https://") && !u.https) { copia_texto(erro, "contenido mixto bloqueado", (size_t)emax); return -1; }
    int id = -1;
    for (int i = 0; i < WS_MAX; i++) if (!sockets[i].uso) { id = i; break; }
    if (id < 0) { copia_texto(erro, "demasiados WebSocket", (size_t)emax); return -1; }
    ws_t *w = &sockets[id];
    memset(w, 0, sizeof *w);
    w->uso = true;
    w->tls = u.https;
    char e2[160];
    if (u.https) {
        if (tls_lock) mutex_tomar(tls_lock);
        if (!cfg.raizes && !tls_config_padrao(&cfg, NULL, e2, sizeof e2)) { if (tls_lock) mutex_largar(tls_lock); copia_texto(erro, "sin raíces de certificados", (size_t)emax); w->uso = false; return -1; }
    }
    int rc = conexao_abrir(&w->cn, &u, &cfg);
    if (u.https && tls_lock) mutex_largar(tls_lock);
    if (rc < 0) { formatar_texto(erro, (size_t)emax, "%s", w->cn.erro); conexao_fechar(&w->cn); w->uso = false; return -1; }
    /* apretón de manos */
    uint8_t clave[16];
    char k64[32], pedido[2048];
    bytes_aleatorios(clave, 16);
    base64(clave, 16, k64);
    char og[200] = "", ck[1024] = "", extra[160] = "", porta[8] = "";
    if (origem) rede_origem(origem, og, sizeof og);
    rede_cookie_cabecalho(http, origem, false, "GET", true, ck, sizeof ck);
    if (protos && protos[0]) formatar_texto(extra, sizeof extra, "Sec-WebSocket-Protocol: %s\r\n", protos);
    if (u.porta != (u.https ? 443 : 80)) formatar_texto(porta, sizeof porta, ":%d", u.porta);
    int np = formatar_texto(pedido, sizeof pedido,
        "GET %s HTTP/1.1\r\nHost: %s%s\r\nUser-Agent: " REDE_UA "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n%s%s%s%s%s%s",
        u.caminho, u.host, porta, k64, og[0] ? "Origin: " : "", og, og[0] ? "\r\n" : "", extra,
        ck[0] ? "Cookie: " : "", ck);
    if (ck[0]) np += formatar_texto(pedido + np, sizeof pedido - (size_t)np, "\r\n");
    np += formatar_texto(pedido + np, sizeof pedido - (size_t)np, "\r\n");
    if (es_escribir(w, pedido, np) < 0) { copia_texto(erro, "no se pudo enviar el apretón de manos", (size_t)emax); cerrar_socket(w, 1006); w->uso = false; return -1; }
    tempo_limite(w->cn.fd, 10000);
    char *cab = aloca(8192);
    if (!cab) { cerrar_socket(w, 1006); w->uso = false; return -1; }
    int nc = 0, fin = -1;
    while (fin < 0 && nc < 8000) {
        trava_tomar(w);
        int n = conexao_ler(&w->cn, cab + nc, (unsigned)(8190 - nc));
        trava_largar(w);
        if (n <= 0) break;
        nc += n;
        for (int i = 3; i < nc; i++) if (cab[i - 3] == '\r' && cab[i - 2] == '\n' && cab[i - 1] == '\r' && cab[i] == '\n') { fin = i + 1; break; }
    }
    tempo_limite(w->cn.fd, 0);
    bool ok = false;
    if (fin > 0) {
        int resto = nc - fin;   /* lo que sobre son tramas */
        uint8_t *sobra = resto > 0 ? aloca((size_t)resto) : NULL;
        if (sobra) memcpy(sobra, cab + fin, (size_t)resto);
        cab[fin] = 0;
        char acc[80], esperado[32], cat[80 + 40];
        uint8_t dg[20];
        formatar_texto(cat, sizeof cat, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", k64);
        sha1((const uint8_t *)cat, (int)tam_texto(cat), dg);
        base64(dg, 20, esperado);
        ok = comeca_com(cab, "HTTP/1.1 101") && rede_cab_valor(cab, "Sec-WebSocket-Accept", acc, sizeof acc) && compara_texto(acc, esperado) == 0;
        if (ok) {
            rede_cookies_da_resposta(http, cab);
            rede_cab_valor(cab, "Sec-WebSocket-Protocol", w->protocolo, sizeof w->protocolo);
            if (sobra && rx_crecer(w, resto)) { memcpy(w->rx, sobra, (size_t)resto); w->rxn = resto; }
        } else formatar_texto(erro, (size_t)emax, "el servidor no aceptó el WebSocket");
        libera(sobra);
    } else copia_texto(erro, "sin respuesta al apretón de manos", (size_t)emax);
    libera(cab);
    if (!ok) { cerrar_socket(w, 1006); w->uso = false; return -1; }
    w->estado = 1;
    analizar(w);
    return id;
}

int ws_estado(int id) { return id >= 0 && id < WS_MAX && sockets[id].uso ? sockets[id].estado : 3; }
const char *ws_protocolo(int id) { return id >= 0 && id < WS_MAX && sockets[id].uso ? sockets[id].protocolo : ""; }

int ws_enviar(int id, const void *d, int n, bool binario)
{
    if (id < 0 || id >= WS_MAX || !sockets[id].uso || sockets[id].estado != 1) return -1;
    if (n > WS_MSG_MAX) return -1;
    enviar_trama(&sockets[id], binario ? 2 : 1, d, n);
    return 0;
}

int ws_fechar(int id, int codigo, const char *razao)
{
    if (id < 0 || id >= WS_MAX || !sockets[id].uso) return -1;
    ws_t *w = &sockets[id];
    if (w->estado == 1) {
        uint8_t b[130] = { 0 };
        int n = 0;
        if (codigo > 0) {
            b[n++] = (uint8_t)(codigo >> 8); b[n++] = (uint8_t)codigo;
            for (const char *p = razao ? razao : ""; *p && n < 125; p++) b[n++] = (uint8_t)*p;
        }
        enviar_trama(w, 8, b, n);
        w->cierre_enviado = true;
        w->estado = 2;
        w->cod_cierre = codigo > 0 ? codigo : 1005;
    } else if (w->estado == 0) cerrar_socket(w, 1006);
    return 0;
}

int ws_receber(int id, uint8_t **dados, int *tipo, int *codigo)
{
    if (id < 0 || id >= WS_MAX || !sockets[id].uso) return -1;
    ws_t *w = &sockets[id];
    bombear(w);
    if (w->cola_n > 0) {
        msg_t *m = &w->cola[w->cola_ini];
        *dados = m->d; *tipo = m->tipo; *codigo = m->cod;
        int n = m->n;
        w->cola_ini = (w->cola_ini + 1) % WS_COLA;
        w->cola_n--;
        return n;
    }
    if (w->estado == 3 && !w->cierre_entregado) {   /* el último aviso: la conexión terminó */
        w->cierre_entregado = true;
        *dados = NULL; *tipo = 8; *codigo = w->cod_cierre;
        return 0;
    }
    return -1;
}

int ws_ativos(void)
{
    int n = 0;
    for (int i = 0; i < WS_MAX; i++) if (sockets[i].uso && sockets[i].estado != 3) n++;
    return n;
}

void ws_liberar(int id)
{
    if (id < 0 || id >= WS_MAX || !sockets[id].uso) return;
    ws_t *w = &sockets[id];
    cerrar_socket(w, 1000);
    for (int i = 0; i < w->cola_n; i++) libera(w->cola[(w->cola_ini + i) % WS_COLA].d);
    libera(w->rx); libera(w->frag);
    memset(w, 0, sizeof *w);
}

void ws_liberar_todos(void)
{
    for (int i = 0; i < WS_MAX; i++) ws_liberar(i);
}
