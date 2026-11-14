// SPDX-License-Identifier: GPL-2.0-or-later
/* plataforma_sagui.c - lite-monkey en SaguiOS (/bin/litemonkey), con la libsagui.
 *
 * Diferencias con el PC:
 *   - memoria: un asignador por clases de tamaño encima de aloca(), porque el
 *     de la libsagui busca el primer hueco recorriendo todos los bloques y con
 *     miles de documentos sería cuadrático;
 *   - espera: no hay select; se mira cada socket con disponivel() y, si nada
 *     está listo, se duerme en SIS_ESPERAR_REDE (D117) hasta que la red entregue
 *     algo a algún socket;
 *   - sincronizar: SIS_SINCRONIZAR (D116), que vacía la caché del volumen;
 *   - no se puede escuchar en una sola dirección: --escuchar se ignora;
 *   - el cliente lento bloquea las escrituras (escrever espera a que quepa). */
#include "plataforma.h"

/* ------------------------------------------------------------ memoria */

#define CLASES      26
#define LOSA        (256u * 1024u)
#define CAB         8u
#define GRANDE      0xFFu

static const uint32_t TAM_CLASE[CLASES] = {
    16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072,
    4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536, 98304,
};
static void *libres[CLASES];
static uint8_t *losa;
static uint32_t losa_resto;

static int clase_de(size_t n)
{
    for (int i = 0; i < CLASES; i++)
        if (n <= TAM_CLASE[i]) return i;
    return -1;
}

void *lmk_aloca(size_t n)
{
    if (!n) n = 1;
    int c = clase_de(n);
    uint32_t *h;
    if (c < 0) {   /* grande: directo a la libsagui */
        h = aloca(n + CAB);
        if (!h) return NULL;
        h[0] = GRANDE;
        h[1] = (uint32_t)n;
        return h + 2;
    }
    if (libres[c]) {
        h = (uint32_t *)libres[c] - 2;
        libres[c] = *(void **)libres[c];
        return h + 2;
    }
    uint32_t total = TAM_CLASE[c] + CAB;
    if (losa_resto < total) {
        losa = aloca(LOSA);
        if (!losa) { losa_resto = 0; return NULL; }
        losa_resto = LOSA;
    }
    h = (uint32_t *)losa;
    losa += total;
    losa_resto -= total;
    h[0] = (uint32_t)c;
    h[1] = TAM_CLASE[c];
    return h + 2;
}

void lmk_libera(void *p)
{
    if (!p) return;
    uint32_t *h = (uint32_t *)p - 2;
    if (h[0] == GRANDE) { libera(h); return; }
    *(void **)p = libres[h[0]];
    libres[h[0]] = p;
}

void *lmk_realoca(void *p, size_t n)
{
    if (!p) return lmk_aloca(n);
    uint32_t *h = (uint32_t *)p - 2;
    uint32_t cap = h[1];
    if (n <= cap && (h[0] != GRANDE || n > cap / 2)) return p;
    void *q = lmk_aloca(n);
    if (!q) return NULL;
    memcpy(q, p, n < cap ? n : cap);
    lmk_libera(p);
    return q;
}

/* La libsagui no trae la división de 64 bits de la libgcc. */
static uint64_t divmod64(uint64_t n, uint64_t d, uint64_t *resto)
{
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ULL << i; }
    }
    if (resto) *resto = r;
    return q;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) { return divmod64(n, d, NULL); }
uint64_t __umoddi3(uint64_t n, uint64_t d) { uint64_t r; divmod64(n, d, &r); return r; }
int64_t __divdi3(int64_t n, int64_t d)
{
    bool neg = (n < 0) != (d < 0);
    uint64_t q = divmod64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, NULL);
    return neg ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t r;
    divmod64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
    return n < 0 ? -(int64_t)r : (int64_t)r;
}

/* ------------------------------------------------------------ reloj, azar, registro */

uint64_t plat_ms(void)
{
    static uint32_t ultimo, vueltas;
    uint32_t t = tempo_ms();
    if (t < ultimo) vueltas++;
    ultimo = t;
    return (uint64_t)vueltas << 32 | t;
}

uint64_t plat_epoca_ms(void) { return (uint64_t)hora_utc(NULL) * 1000u + tempo_ms() % 1000u; }

void plat_azar(void *buf, uint32_t n)
{
    uint8_t *p = buf;
    while (n) {
        uint32_t k = n > 4096 ? 4096 : n;
        bytes_aleatorios(p, k);
        p += k;
        n -= k;
    }
}

void plat_dormir(uint32_t ms) { dormir(ms); }

void plat_escribir_log(const char *texto)
{
    /* Los textos del núcleo están en UTF-8 (van en el BSON); la consola es Latin-1. */
    char b[1024];
    size_t n = tam_texto(texto);
    if (n > sizeof b - 2) n = sizeof b - 2;
    memcpy(b, texto, n);
    n = utf8_para_latin1(b, n, NULL);
    b[n++] = '\n';
    escrever_tudo(ERROS, b, (unsigned)n);
}

/* ------------------------------------------------------------ archivos */

int plat_abrir(const char *ruta, int modo)
{
    int m = 0;
    if (modo & PLAT_LEER) m |= ABRIR_LER;
    if (modo & PLAT_ESCRIBIR) m |= ABRIR_ESCREVER;
    if (modo & PLAT_CREAR) m |= ABRIR_CRIAR;
    if (modo & PLAT_TRUNCAR) m |= ABRIR_TRUNCAR;
    return abrir(ruta, m);
}

int plat_leer_en(int fd, uint32_t off, void *buf, uint32_t n)
{
    if (posicionar(fd, (int)off, POS_INICIO) < 0) return 0;
    uint32_t hecho = 0;
    while (hecho < n) {
        int r = ler(fd, (uint8_t *)buf + hecho, n - hecho);
        if (r <= 0) break;
        hecho += (uint32_t)r;
    }
    return (int)hecho;
}

bool plat_escribir_en(int fd, uint32_t off, const void *buf, uint32_t n)
{
    if (posicionar(fd, (int)off, POS_INICIO) != (int)off) return false;
    uint32_t hecho = 0;
    while (hecho < n) {
        int r = escrever(fd, (const uint8_t *)buf + hecho, n - hecho);
        if (r <= 0) return false;
        hecho += (uint32_t)r;
    }
    return true;
}

bool plat_sincronizar(int fd) { return sincronizar(fd) == 0; }   /* SIS_SINCRONIZAR (D116) */

void plat_cerrar(int fd) { fechar(fd); }

bool plat_tamano(int fd, uint32_t *n)
{
    int r = posicionar(fd, 0, POS_FIM);
    if (r < 0) return false;
    *n = (uint32_t)r;
    return true;
}

bool plat_renombrar(const char *de, const char *a) { return renomear(de, a) == 0; }
bool plat_borrar(const char *ruta) { return apagar(ruta) == 0; }

bool plat_existe_carpeta(const char *ruta)
{
    info_arquivo_t i;
    return info_arquivo(ruta, &i) == 0 && i.tipo == TIPO_PASTA;
}

bool plat_crear_carpeta(const char *ruta) { return criar_pasta(ruta) == 0 || plat_existe_carpeta(ruta); }
bool plat_borrar_carpeta(const char *ruta) { return remover_pasta(ruta) == 0; }

bool plat_listar(const char *ruta, plat_listar_fn f, void *ctx)
{
    if (!plat_existe_carpeta(ruta)) return false;
    entrada_pasta_t e;
    for (int i = 0; listar(ruta, i, &e) == 1; i++) {
        if (!compara_texto(e.nome, ".") || !compara_texto(e.nome, "..")) continue;
        f(ctx, e.nome, e.tipo == TIPO_PASTA);
    }
    return true;
}

/* ------------------------------------------------------------ red */

bool plat_parar(void) { return false; }   /* Ctrl+C termina el proceso; el kernel cierra los archivos */

int plat_escuchar(const char *direccion, int puerto)
{
    (void)direccion;
    int s = criar_socket(SOCK_TCP);
    if (s < 0) return -1;
    if (escutar(s, puerto, 16) < 0) { fechar(s); return -1; }
    return s;
}

int plat_aceptar(int fd, char *quien, uint32_t max)
{
    if (disponivel(fd) <= 0) return -1;
    endereco_rede_t de;
    int c = aceitar_conexao(fd, &de);
    if (c < 0) return -1;
    char ip[16];
    formatar_texto(quien, max, "%s:%u", ip_para_texto(de.ip, ip), de.porta);
    return c;
}

int plat_recibir(int fd, void *buf, uint32_t n)
{
    int d = disponivel(fd);
    if (d == 0) return -1;
    if (d < 0) return d == ERRO_PIPE_QUEBRADO ? 0 : -2;
    int r = ler(fd, buf, (uint32_t)d < n ? (uint32_t)d : n);
    if (r > 0) return r;
    return r == 0 ? 0 : -2;
}

int plat_enviar(int fd, const void *buf, uint32_t n)
{
    int r = escrever(fd, buf, n);
    return r < 0 ? -2 : r;
}

void plat_cerrar_socket(int fd) { fechar(fd); }

int plat_esperar(const int *fds, const uint8_t *quiere_escribir, uint8_t *listo, int n, int plazo_ms)
{
    static unsigned visto;
    uint64_t fin = plat_ms() + (uint64_t)(plazo_ms > 0 ? plazo_ms : 0);
    for (;;) {
        int listos = 0;
        for (int i = 0; i < n; i++) {
            listo[i] = 0;
            if (disponivel(fds[i]) != 0) listo[i] |= 1;
            if (quiere_escribir[i]) listo[i] |= 2;
            if (listo[i]) listos++;
        }
        uint64_t ahora = plat_ms();
        if (listos || ahora >= fin) return listos;
        int r = esperar_rede(visto, (unsigned)(fin - ahora));
        if (r < 0) dormir(10);   /* núcleo sin SIS_ESPERAR_REDE */
        else visto = (unsigned)r;
    }
}

/* ------------------------------------------------------------ valores por defecto */

const char *plat_carpeta_datos(void)
{
    /* /home/litemonkey si /home es un disco persistente con escritura; si no, /tmp. */
    static info_montagem_t m[12];
    int n = info_sistema(INFO_MONTAGENS, m, 12);
    for (int i = 0; i < n && i < 12; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return "/home/litemonkey";
    return "/tmp/litemonkey";
}

uint32_t plat_memoria_mb(void) { return 16; }

int principal(int argc, char **argv) { return lmk_principal(argc, argv); }
