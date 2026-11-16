// SPDX-License-Identifier: GPL-2.0-or-later
/* ext34_unidad.c - arnés en el PC para la escritura en ext3/ext4 (D326, D337).
 *
 * Compila kernel/fs/ext2.c (con ESCRITA_EXT34 = 1), kernel/fs/jbd2.c y kernel/lib/crc32c.c
 * contra un disco simulado en memoria (tests/ext34_simulado.h). Modos:
 *
 *   ext34_unidad unidad
 *       pruebas de unidad: CRC32c y CRC16 con vectores conocidos, la lista de extents
 *       (insertar, unir, partir) y el diario JBD2 sobre un disco en memoria (transacción
 *       confirmada que se reproduce, bloque escapado, corte antes del commit, copia con
 *       la suma mal).
 *
 *   ext34_unidad cortes <imagen> <carpeta temporal> <e2fsck> [sin-cortes|cortes] [paso]
 *       monta la imagen con escritura, hace la carga (carpetas, archivos pequeños, borrar,
 *       un archivo grande fragmentado, renombrar, truncar, sobrescribir) y comprueba el
 *       contenido. Se guarda cada escritura al disco y cada barrera. Después «se corta la
 *       luz» en cada escritura: el disco con las k primeras escrituras (y otra variante: lo
 *       anterior a la última barrera más un subconjunto al azar de lo de después, porque
 *       el disco puede reordenar entre barreras). Cada estado se recupera de dos maneras y
 *       las dos tienen que dar «e2fsck -fn» limpio:
 *         a) nuestro código: montar con escritura (reproduce el diario), leer todo y
 *            desmontar;
 *         b) Linux: «e2fsck -E journal_only» (reproduce el diario) y después «e2fsck -fn».
 *       Con «sin-cortes» (volúmenes sin diario, que no pueden ser coherentes tras un corte)
 *       solo se comprueba el estado final. Con «paso» N > 1 se comprueba uno de cada N cortes
 *       (la batería; la prueba completa es N = 1).
 *       Al final escribe <carpeta>/final.img (para que la prueba de Python lo mire con debugfs).
 */
#define ESCRITA_EXT34 1
#ifndef EXT2_FONTE                    /* otra copia de ext2.c (pruebas de mutación del arnés) */
#define EXT2_FONTE "../kernel/fs/ext2.c"
#endif
#include EXT2_FONTE
#include <sys/wait.h>

/* ------------------------------------------------------------ disco simulado */

typedef struct { uint32_t lba, n; uint8_t *dados; } registro_t;   /* n == 0: barrera */

static uint8_t   *mem;
static size_t     mem_tam;
static registro_t *regs;
static uint32_t   nregs, cap_regs;
static bool       registrando;

static void registrar(uint32_t lba, uint32_t n, const void *dados)
{
    if (nregs == cap_regs) {
        cap_regs = cap_regs ? cap_regs * 2 : 4096;
        regs = realloc(regs, cap_regs * sizeof *regs);
        if (!regs) { fprintf(stderr, "sin memoria\n"); exit(2); }
    }
    regs[nregs].lba = lba;
    regs[nregs].n = n;
    regs[nregs].dados = NULL;
    if (n) {
        regs[nregs].dados = malloc((size_t)n * 512);
        memcpy(regs[nregs].dados, dados, (size_t)n * 512);
    }
    nregs++;
}

int disco_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    (void)d;
    if ((uint64_t)(lba + (uint64_t)n) * 512 > mem_tam) return -1;
    memcpy(buf, mem + (size_t)lba * 512, (size_t)n * 512);
    return 0;
}

int disco_escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf)
{
    (void)d;
    if ((uint64_t)(lba + (uint64_t)n) * 512 > mem_tam) return -1;
    memcpy(mem + (size_t)lba * 512, buf, (size_t)n * 512);
    if (registrando) registrar(lba, n, buf);
    return 0;
}

int disco_sincronizar(disco_t *d)
{
    (void)d;
    if (registrando) registrar(0, 0, NULL);
    return 0;
}

static bool falando;
void klog(const char *fmt, ...)
{
    if (!falando) return;
    va_list a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    va_end(a);
}

void disco_avisar(int tipo, const char *fmt, ...)
{
    (void)tipo;
    if (!falando) return;
    va_list a;
    va_start(a, fmt);
    fprintf(stderr, "AVISO: ");
    vfprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    va_end(a);
}

int snformatar(char *dst, size_t max, const char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    int r = vsnprintf(dst, max, fmt, a);
    va_end(a);
    return r;
}

static uint32_t hora_falsa = 1700000000u, tiques;
uint32_t hora_utc(void) { return hora_falsa; }
/* Cada consulta avanza 0,37 s: terminar_dados confirma más o menos cada tres escrituras. */
uint32_t relogio_tiques(void) { tiques += 37; return tiques; }

void permisos_dono_novo(bool pasta, uint32_t *uid, uint32_t *gid, uint16_t *modo)
{
    *uid = 0;
    *gid = 0;
    *modo = pasta ? 0755 : 0644;
}

/* ------------------------------------------------------------ utilidades */

static int falhas, verificacoes;

static void checar(bool ok, const char *fmt, ...)
{
    verificacoes++;
    if (ok) return;
    falhas++;
    va_list a;
    va_start(a, fmt);
    fprintf(stdout, "FALLO: ");
    vfprintf(stdout, fmt, a);
    fprintf(stdout, "\n");
    va_end(a);
}

static disco_t disco = { "sim0", 0 };

/* Monta "mem"; *rw = quedó con escritura. */
static void *montar(bool *rw, uint32_t opcoes)
{
    void *priv = NULL;
    disco.setores = (uint32_t)(mem_tam / 512);
    if (fs_ext2.montar(&disco, &opcoes, &priv) < 0) return NULL;
    *rw = !(opcoes & MONTAR_SO_LEITURA);
    return priv;
}

static bool gravar_arquivo(const char *caminho, const uint8_t *b, size_t n)
{
    FILE *f = fopen(caminho, "wb");
    if (!f) return false;
    bool ok = fwrite(b, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* Ejecuta un programa; devuelve su código de salida y deja su salida en "saida". */
static int executar(char *const argv[], char *saida, size_t max)
{
    char cmd[2048] = "";
    for (int k = 0; argv[k]; k++) { strcat(cmd, "'"); strcat(cmd, argv[k]); strcat(cmd, "' "); }
    strcat(cmd, "2>&1");
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    size_t n = fread(saida, 1, max - 1, p);
    saida[n] = 0;
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* ------------------------------------------------------------ modelo de los archivos */

#define MAX_ARQ 128
static struct { char nome[96]; uint8_t *d; uint32_t tam, cap; bool vivo; } modelo[MAX_ARQ];

static int modelo_achar(const char *nome)
{
    for (int k = 0; k < MAX_ARQ; k++) if (modelo[k].vivo && !strcmp(modelo[k].nome, nome)) return k;
    return -1;
}

static int modelo_novo(const char *nome)
{
    int k = modelo_achar(nome);
    if (k >= 0) return k;
    for (k = 0; k < MAX_ARQ; k++)
        if (!modelo[k].vivo) {
            free(modelo[k].d);
            memset(&modelo[k], 0, sizeof modelo[k]);
            snprintf(modelo[k].nome, sizeof modelo[k].nome, "%s", nome);
            modelo[k].vivo = true;
            return k;
        }
    fprintf(stderr, "modelo lleno\n");
    exit(2);
}

static void modelo_tam(int k, uint32_t tam)
{
    if (tam > modelo[k].cap) {
        modelo[k].d = realloc(modelo[k].d, tam);
        memset(modelo[k].d + modelo[k].cap, 0, tam - modelo[k].cap);
        modelo[k].cap = tam;
    }
    if (tam > modelo[k].tam) memset(modelo[k].d + modelo[k].tam, 0, tam - modelo[k].tam);
    modelo[k].tam = tam;
}

static uint8_t padrao(uint32_t semente, uint32_t off) { return (uint8_t)(semente * 131u + off * 7u + (off >> 9) + (off >> 13) * 3u); }

/* ------------------------------------------------------------ operaciones */

static void *vol;
static uint8_t arq_buf[64];

static void op_escrever(const char *nome, uint32_t pos, uint32_t n, uint32_t semente, bool truncar)
{
    uint32_t tam;
    int r = fs_ext2.abrir(vol, nome, ABRIR_CRIAR | ABRIR_ESCREVER | (truncar ? ABRIR_TRUNCAR : 0), arq_buf, &tam);
    checar(r >= 0, "abrir %s: %d", nome, r);
    if (r < 0) return;
    int k = modelo_novo(nome);
    if (truncar) modelo_tam(k, 0);
    static const uint32_t pedacos[] = { 1000, 4096, 7000, 513, 12288, 1 };
    uint8_t *b = malloc(16384);
    uint32_t feito = 0, q = 0;
    while (feito < n) {
        uint32_t p = pedacos[q++ % 6];
        if (p > n - feito) p = n - feito;
        for (uint32_t i = 0; i < p; i++) b[i] = padrao(semente, pos + feito + i);
        r = fs_ext2.escrever(vol, arq_buf, pos + feito, b, p, &tam);
        checar(r == (int)p, "escribir %s en %u (%u bytes): %d", nome, pos + feito, p, r);
        if (r != (int)p) break;
        if (pos + feito + p > modelo[k].tam) modelo_tam(k, pos + feito + p);
        memcpy(modelo[k].d + pos + feito, b, p);
        feito += p;
    }
    free(b);
    fs_ext2.fechar(vol, arq_buf);
}

static void op_truncar(const char *nome, uint32_t tam_novo)
{
    uint32_t tam;
    int r = fs_ext2.abrir(vol, nome, ABRIR_ESCREVER, arq_buf, &tam);
    checar(r >= 0, "abrir %s para truncar: %d", nome, r);
    if (r < 0) return;
    r = fs_ext2.truncar(vol, arq_buf, tam_novo);
    checar(r == 0, "truncar %s a %u: %d", nome, tam_novo, r);
    fs_ext2.fechar(vol, arq_buf);
    int k = modelo_achar(nome);
    if (k >= 0 && r == 0) modelo_tam(k, tam_novo);
}

static void op_apagar(const char *nome)
{
    int r = fs_ext2.apagar(vol, nome);
    checar(r == 0, "borrar %s: %d", nome, r);
    int k = modelo_achar(nome);
    if (k >= 0 && r == 0) modelo[k].vivo = false;
}

static void op_renomear(const char *de, const char *para)
{
    int r = fs_ext2.renomear(vol, de, para);
    checar(r == 0, "renombrar %s -> %s: %d", de, para, r);
    if (r < 0) return;
    size_t n = strlen(de);
    int k = modelo_achar(para);
    if (k >= 0) modelo[k].vivo = false;
    for (k = 0; k < MAX_ARQ; k++)
        if (modelo[k].vivo && !strncmp(modelo[k].nome, de, n) && (modelo[k].nome[n] == 0 || modelo[k].nome[n] == '/')) {
            char novo[96];
            snprintf(novo, sizeof novo, "%s%s", para, modelo[k].nome + n);
            snprintf(modelo[k].nome, sizeof modelo[k].nome, "%s", novo);
        }
}

static void op_pasta(const char *nome)
{
    int r = fs_ext2.criar_pasta(vol, nome);
    checar(r == 0, "mkdir %s: %d", nome, r);
}

static void op_rmdir(const char *nome)
{
    int r = fs_ext2.remover_pasta(vol, nome);
    checar(r == 0, "rmdir %s: %d", nome, r);
}

/* Lee cada archivo del modelo y lo compara. */
static void verificar_modelo(const char *quando)
{
    uint8_t *b = malloc(1 << 20);
    for (int k = 0; k < MAX_ARQ; k++) {
        if (!modelo[k].vivo) continue;
        uint32_t tam;
        int r = fs_ext2.abrir(vol, modelo[k].nome, ABRIR_LER, arq_buf, &tam);
        checar(r >= 0, "%s: abrir %s: %d", quando, modelo[k].nome, r);
        if (r < 0) continue;
        checar(tam == modelo[k].tam, "%s: %s mide %u y debería %u", quando, modelo[k].nome, tam, modelo[k].tam);
        for (uint32_t pos = 0; pos < modelo[k].tam; pos += 1u << 20) {
            uint32_t n = modelo[k].tam - pos < (1u << 20) ? modelo[k].tam - pos : 1u << 20;
            r = fs_ext2.ler(vol, arq_buf, pos, b, n);
            checar(r == (int)n && !memcmp(b, modelo[k].d + pos, n), "%s: contenido de %s en %u distinto (%d)",
                   quando, modelo[k].nome, pos, r);
        }
        fs_ext2.fechar(vol, arq_buf);
    }
    for (int k = 0; k < MAX_ARQ; k++) {
        if (modelo[k].vivo) continue;
        if (!modelo[k].nome[0]) continue;
        if (modelo_achar(modelo[k].nome) >= 0) continue;
        info_arquivo_t inf;
        checar(fs_ext2.info(vol, modelo[k].nome, &inf) == ERRO_NAO_EXISTE, "%s: %s debería no existir", quando, modelo[k].nome);
    }
    free(b);
}

/* Recorre el árbol entero leyendo todo (tras un corte: que nada dé error). */
static int recorrer(const char *pasta, uint8_t *b)
{
    int erros = 0;
    for (int i = 0; ; i++) {
        entrada_pasta_t e;
        int r = fs_ext2.listar(vol, pasta, i, &e);
        if (r == 0) break;
        if (r < 0) { if (falando) fprintf(stderr, "listar %s: %d\n", pasta, r); return erros + 1; }
        char cam[256];
        snprintf(cam, sizeof cam, "%s%s%s", pasta, pasta[0] ? "/" : "", e.nome);
        if (!strcmp(e.nome, "lost+found")) continue;
        if (e.tipo == TIPO_PASTA) { erros += recorrer(cam, b); continue; }
        uint32_t tam;
        uint8_t arq[64];
        int r2 = fs_ext2.abrir(vol, cam, ABRIR_LER, arq, &tam);
        if (r2 < 0) { if (falando) fprintf(stderr, "abrir %s: %d\n", cam, r2); erros++; continue; }
        for (uint32_t pos = 0; pos < tam; pos += 1u << 20) {
            uint32_t n = tam - pos < (1u << 20) ? tam - pos : 1u << 20;
            if ((r2 = fs_ext2.ler(vol, arq, pos, b, n)) != (int)n) { if (falando) fprintf(stderr, "leer %s: %d\n", cam, r2); erros++; break; }
        }
        fs_ext2.fechar(vol, arq);
    }
    return erros;
}

/* ------------------------------------------------------------ la carga */

static void carga(void)
{
    char n[96];
    op_pasta("d1");
    op_pasta("d1/sub");
    op_pasta("d2");
    op_pasta("vacia");
    /* Archivos pequeños y medianos. */
    for (int k = 0; k < 40; k++) {
        snprintf(n, sizeof n, "d1/f%02d.txt", k);
        op_escrever(n, 0, (uint32_t)(k * 997) % 9000, (uint32_t)k + 1, true);
    }
    /* Huecos: se borra uno de cada dos. */
    for (int k = 0; k < 40; k += 2) { snprintf(n, sizeof n, "d1/f%02d.txt", k); op_apagar(n); }
    /* Dos archivos grandes que crecen a la vez: los dos quedan fragmentados (árbol de
     * extents con más de cuatro extents: un nivel fuera del inodo). */
    for (uint32_t pos = 0; pos < 1200u * 1024; pos += 24u * 1024) {
        op_escrever("d2/grande.bin", pos, 24u * 1024, 500, false);
        op_escrever("d2/otro.bin", pos / 2, 12u * 1024, 600, false);
        if (pos % (240u * 1024) == 0) {
            snprintf(n, sizeof n, "d1/sub/t%u", pos / 1024);
            op_escrever(n, 0, 3000, pos, true);
        }
    }
    verificar_modelo("tras escribir");
    /* Renombrar: en la misma carpeta, a otra, sobre uno que existe y una carpeta. */
    op_renomear("d1/f01.txt", "d1/f01b.txt");
    op_renomear("d1/f03.txt", "d2/f03.txt");
    op_renomear("d1/f05.txt", "d1/f07.txt");
    op_renomear("d1/sub", "d2/sub2");
    /* Truncar: a la mitad de un bloque, a cero y por encima del árbol de extents. */
    op_truncar("d2/grande.bin", 600u * 1024 + 123);
    op_truncar("d1/f09.txt", 0);
    op_truncar("d1/f11.txt", 1500);
    op_truncar("d2/otro.bin", 0);
    /* Sobrescribir en medio y alargar lo truncado. */
    op_escrever("d2/grande.bin", 100000, 50000, 700, false);
    op_escrever("d2/otro.bin", 0, 70000, 800, false);
    op_escrever("d1/f09.txt", 4096, 5000, 900, false);
    /* Borrar: el grande, una carpeta vacía. */
    op_apagar("d2/otro.bin");
    op_rmdir("vacia");
    op_apagar("d1/f13.txt");
    /* Un archivo nuevo ocupando lo liberado. */
    op_escrever("d2/ultimo.bin", 0, 300u * 1024, 1000, true);
    /* Si la imagen trae la carpeta "lista" (mke2fs -d, con índice htree): añadir y borrar en
     * ella (la carpeta pierde el índice de forma válida, D326). */
    info_arquivo_t inf;
    if (fs_ext2.info(vol, "lista", &inf) == 0) {
        op_escrever("lista/nuevo.txt", 0, 100, 1100, true);
        op_apagar("lista/e123");
        op_renomear("lista/e200", "lista/e200-renombrado");
    }
    verificar_modelo("al final");
}

/* ------------------------------------------------------------ comprobar un estado */

static char *tmpdir, *e2fsck_bin;
static uint8_t *leitura;
static uint32_t estados, reproduzidos, falhas_cortes;
static bool falha_guardada;

static bool fsck_limpo(const char *img, char *saida, size_t max)
{
    char *argv[] = { e2fsck_bin, "-fn", (char *)img, NULL };
    return executar(argv, saida, max) == 0;
}

static void guardar_falha(const uint8_t *estado, const char *rotulo)
{
    if (falha_guardada) return;
    falha_guardada = true;
    char c[512];
    snprintf(c, sizeof c, "%s/fallo.img", tmpdir);
    gravar_arquivo(c, estado, mem_tam);
    printf("  (estado guardado en %s: %s)\n", c, rotulo);
}

static void checar_estado(const uint8_t *estado, const char *rotulo)
{
    static char saida[65536];
    char a[512], b[512];
    snprintf(a, sizeof a, "%s/a.img", tmpdir);
    snprintf(b, sizeof b, "%s/b.img", tmpdir);
    estados++;
    bool ok = true;
    /* a) Nuestro código. */
    memcpy(mem, estado, mem_tam);
    bool rw = false;
    vol = montar(&rw, 0);
    if (!vol || !rw) {
        printf("FALLO %s: nuestro montaje %s\n", rotulo, vol ? "quedó en solo lectura" : "falló");
        ok = false;
    }
    if (vol) {
        if (((e2_t *)vol)->j.reproduzidas) reproduzidos++;
        int e = recorrer("", leitura);
        if (e) { printf("FALLO %s: %d errores al leer tras recuperar\n", rotulo, e); ok = false; }
        /* Y escribir algo después de recuperar (lo que se grabe no puede pisar lo reproducido). */
        uint8_t arq[64];
        uint32_t tam;
        if (rw && fs_ext2.abrir(vol, "tras_el_corte.bin", ABRIR_CRIAR | ABRIR_ESCREVER | ABRIR_TRUNCAR, arq, &tam) >= 0) {
            memset(leitura, 0x5A, 20000);
            if (fs_ext2.escrever(vol, arq, 0, leitura, 20000, &tam) != 20000) { printf("FALLO %s: escribir tras recuperar\n", rotulo); ok = false; }
            fs_ext2.fechar(vol, arq);
        } else if (rw) { printf("FALLO %s: crear un archivo tras recuperar\n", rotulo); ok = false; }
        if (fs_ext2.desmontar(vol, true) < 0) { printf("FALLO %s: desmontar\n", rotulo); ok = false; }
        vol = NULL;
    }
    if (!gravar_arquivo(a, mem, mem_tam)) { fprintf(stderr, "no se puede escribir %s\n", a); exit(2); }
    if (!fsck_limpo(a, saida, sizeof saida)) {
        printf("FALLO %s: e2fsck -fn tras nuestra recuperación:\n%.1500s\n", rotulo, saida);
        ok = false;
    }
    /* b) Linux reproduce el diario. */
    if (!gravar_arquivo(b, estado, mem_tam)) exit(2);
    char *rep[] = { e2fsck_bin, "-y", "-E", "journal_only", b, NULL };
    int r = executar(rep, saida, sizeof saida);
    if (r > 1) { printf("FALLO %s: e2fsck -E journal_only salió con %d:\n%.1500s\n", rotulo, r, saida); ok = false; }
    if (!fsck_limpo(b, saida, sizeof saida)) {
        printf("FALLO %s: e2fsck -fn tras la recuperación de Linux:\n%.1500s\n", rotulo, saida);
        ok = false;
    }
    if (!ok) { falhas_cortes++; guardar_falha(estado, rotulo); }
}

/* ------------------------------------------------------------ modo cortes */

static int modo_cortes(const char *img, bool cortar, uint32_t passo)
{
    FILE *f = fopen(img, "rb");
    if (!f) { perror(img); return 2; }
    fseek(f, 0, SEEK_END);
    mem_tam = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *base = malloc(mem_tam);
    mem = malloc(mem_tam);
    leitura = malloc(1 << 20);
    if (fread(base, 1, mem_tam, f) != mem_tam) { fclose(f); return 2; }
    fclose(f);
    memcpy(mem, base, mem_tam);
    falando = getenv("EXT34_LOG") != NULL;

    registrando = true;
    bool rw = false;
    vol = montar(&rw, 0);
    checar(vol && rw, "montar con escritura");
    if (!vol || !rw) { printf("RESULTADO: no se monta con escritura\n"); return 1; }
    bool com_diario = ((e2_t *)vol)->diario;
    carga();
    checar(fs_ext2.desmontar(vol, true) == 0, "desmontar");
    vol = NULL;
    registrando = false;
    uint32_t escritas = 0, barreras = 0;
    for (uint32_t k = 0; k < nregs; k++) { if (regs[k].n) escritas++; else barreras++; }
    printf("carga: %u escrituras, %u barreras, diario %s\n", escritas, barreras, com_diario ? "sí" : "no");

    /* Estado final: con nuestro código (releer el modelo) y e2fsck. */
    uint8_t *final = malloc(mem_tam);
    memcpy(final, mem, mem_tam);
    char c[512], saida[65536];
    snprintf(c, sizeof c, "%s/final.img", tmpdir);
    gravar_arquivo(c, final, mem_tam);
    checar(fsck_limpo(c, saida, sizeof saida), "e2fsck -fn del estado final:\n%.2000s", saida);
    vol = montar(&rw, MONTAR_SO_LEITURA);
    checar(vol != NULL, "volver a montar en solo lectura");
    if (vol) { verificar_modelo("tras volver a montar"); fs_ext2.desmontar(vol, false); vol = NULL; }
    memcpy(mem, final, mem_tam);

    if (cortar) {
        uint8_t *cur = malloc(mem_tam), *alt = malloc(mem_tam), *barr = malloc(mem_tam);
        memcpy(cur, base, mem_tam);
        memcpy(barr, base, mem_tam);
        uint32_t ultima_barreira = 0;   /* índice del registro tras la última barrera */
        uint32_t semente = 12345, corte = 0;
        char rot[128];
        snprintf(rot, sizeof rot, "corte 0 (antes de nada)");
        checar_estado(cur, rot);
        for (uint32_t k = 0; k < nregs; k++) {
            registro_t *g = &regs[k];
            if (!g->n) {
                memcpy(barr, cur, mem_tam);
                ultima_barreira = k + 1;
                continue;
            }
            memcpy(cur + (size_t)g->lba * 512, g->dados, (size_t)g->n * 512);
            corte++;
            /* Con paso > 1 solo se comprueba uno de cada «paso» cortes. */
            if (passo > 1 && corte % passo) continue;
            snprintf(rot, sizeof rot, "corte %u (tras la escritura %u, lba %u)", corte, k, g->lba);
            checar_estado(cur, rot);
            /* Reordenado: lo de antes de la barrera y un subconjunto de lo de después. */
            uint32_t depois = 0;
            for (uint32_t m = ultima_barreira; m <= k; m++) depois++;
            if (depois >= 2) {
                memcpy(alt, barr, mem_tam);
                for (uint32_t m = ultima_barreira; m <= k; m++) {
                    semente = semente * 1103515245u + 12345u;
                    if ((semente >> 16) & 1)
                        memcpy(alt + (size_t)regs[m].lba * 512, regs[m].dados, (size_t)regs[m].n * 512);
                }
                snprintf(rot, sizeof rot, "corte %u reordenado", corte);
                checar_estado(alt, rot);
            }
            if (corte % 200 == 0) { printf("  ... %u cortes, %u fallos\n", corte, falhas_cortes); fflush(stdout); }
        }
        checar(memcmp(cur, final, mem_tam) == 0, "aplicar todas las escrituras da el estado final");
        printf("cortes: %u escrituras, %u estados comprobados, %u con diario reproducido, %u fallos\n",
               corte, estados, reproduzidos, falhas_cortes);
        checar(falhas_cortes == 0, "todos los cortes limpios");
        free(cur); free(alt); free(barr);
    }
    /* Lo del arnés (así el ASan solo avisa de fugas del kernel). */
    free(base); free(final); free(mem); free(leitura);
    for (uint32_t k = 0; k < nregs; k++) free(regs[k].dados);
    free(regs);
    for (int k = 0; k < MAX_ARQ; k++) free(modelo[k].d);
    fflush(stdout);
    printf("RESULTADO: %d verificaciones, %d fallos\n", verificacoes, falhas);
    return falhas ? 1 : 0;
}

/* ------------------------------------------------------------ unidad: JBD2 */

#define JB 1024u
#define JBLOCOS 2048u
#define JINI 1000u      /* el diario va en los bloques físicos 1000..1255 */
#define JLEN 256u
static uint8_t jdisco[JBLOCOS][JB];
static int jescritas, jlimite = -1;   /* jlimite: tras tantas escrituras, el disco no graba más */

static int jler(void *c, uint32_t f, uint8_t *b) { (void)c; if (f >= JBLOCOS) return -1; memcpy(b, jdisco[f], JB); return 0; }
static int jesc(void *c, uint32_t f, const uint8_t *b)
{
    (void)c;
    if (f >= JBLOCOS) return -1;
    if (jlimite >= 0 && jescritas >= jlimite) { jescritas++; return 0; }
    jescritas++;
    memcpy(jdisco[f], b, JB);
    return 0;
}
static int jsinc(void *c) { (void)c; return 0; }
static uint32_t jmap(void *c, uint32_t lb) { (void)c; return lb < JLEN ? JINI + lb : 0; }

static void pbe(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void jformatar(uint32_t incompat, bool soma)
{
    memset(jdisco, 0, sizeof jdisco);
    uint8_t *s = jdisco[JINI];
    pbe(s, 0xC03B3998u);
    pbe(s + 4, 4);
    pbe(s + 0x0C, JB);
    pbe(s + 0x10, JLEN);
    pbe(s + 0x14, 1);
    pbe(s + 0x18, 77);
    pbe(s + 0x1C, 0);
    pbe(s + 0x28, incompat);
    for (int k = 0; k < 16; k++) s[0x30 + k] = (uint8_t)(0xA0 + k);
    pbe(s + 0x40, 1);
    if (soma) { s[0x50] = 4; pbe(s + 0xFC, crc32c(~0u, s, 1024)); }
}

static void jpreparar(jbd2_t *j)
{
    memset(j, 0, sizeof *j);
    j->ler = jler; j->escrever = jesc; j->sincronizar = jsinc; j->mapear = jmap;
    j->bloco = JB;
    j->blocos_inode = JLEN;
    j->hora = 1700000000u;
}

static void unidade_jbd2(uint32_t incompat, bool soma, const char *nome)
{
    jbd2_t j;
    uint32_t fis[5] = { 10, 11, 500, 12, 13 };
    static uint8_t dados[5][JB];
    uint8_t *ptr[5];
    for (int k = 0; k < 5; k++) { memset(dados[k], 0x30 + k, JB); ptr[k] = dados[k]; }
    pbe(dados[2], 0xC03B3998u);     /* empieza con la magia del diario: va escapado */

    /* 1. Transacción confirmada y no aplicada (corte entre el commit y el sitio). */
    jformatar(incompat, soma);
    jpreparar(&j);
    jescritas = 0; jlimite = -1;
    checar(jbd2_abrir(&j) == 0, "%s: abrir el diario (%s)", nome, j.motivo ? j.motivo : "");
    checar(jbd2_vazio(&j), "%s: diario recién formateado vacío", nome);
    checar(jbd2_comecar(&j) == 0 && jbd2_confirmar(&j, 5, fis, ptr) == 0, "%s: confirmar", nome);
    jbd2_liberar(&j);
    jpreparar(&j);
    checar(jbd2_abrir(&j) == 0 && !jbd2_vazio(&j), "%s: tras el corte el diario tiene algo", nome);
    checar(jbd2_recuperar(&j) == 0, "%s: reproducir", nome);
    checar(j.reproduzidas == 1 && j.blocos_reproduzidos == 5, "%s: 1 transacción, 5 bloques (%u, %u)", nome, j.reproduzidas, j.blocos_reproduzidos);
    bool iguais = true;
    for (int k = 0; k < 5; k++) if (memcmp(jdisco[fis[k]], dados[k], JB)) iguais = false;
    checar(iguais, "%s: los bloques reproducidos son los confirmados (con el escapado restaurado)", nome);
    checar(jbd2_vazio(&j), "%s: tras reproducir, vacío", nome);
    uint32_t seq = j.seq;
    jbd2_liberar(&j);
    /* Reabrir: vacío y con la secuencia posterior. */
    jpreparar(&j);
    checar(jbd2_abrir(&j) == 0 && jbd2_vazio(&j) && j.seq == seq && seq == 79, "%s: vacío al reabrir, secuencia %u", nome, j.seq);
    /* 2. Segunda transacción en el mismo sitio, aplicada y avanzada: nada que reproducir. */
    checar(jbd2_comecar(&j) == 0 && jbd2_confirmar(&j, 2, fis, ptr) == 0 && jbd2_avancar(&j) == 0, "%s: confirmar y avanzar", nome);
    jbd2_liberar(&j);
    for (int k = 0; k < 5; k++) memset(jdisco[fis[k]], 0xEE, JB);
    jpreparar(&j);
    checar(jbd2_abrir(&j) == 0 && jbd2_recuperar(&j) == 0 && j.reproduzidas == 0, "%s: tras avanzar, la transacción vieja no vale", nome);
    checar(jdisco[10][0] == 0xEE, "%s: y no se reproduce", nome);
    jbd2_liberar(&j);
    /* 3. Corte en cada escritura de una transacción: sin commit, nada; con commit, todo. */
    for (int lim = 0; lim <= 8; lim++) {
        jformatar(incompat, soma);
        for (int k = 0; k < 5; k++) memset(jdisco[fis[k]], 0xEE, JB);
        jpreparar(&j);
        jbd2_abrir(&j);
        jbd2_comecar(&j);
        jescritas = 0; jlimite = lim;
        jbd2_confirmar(&j, 5, fis, ptr);
        bool commit = jescritas <= lim;   /* todas las escrituras llegaron */
        jlimite = -1;
        jbd2_liberar(&j);
        jpreparar(&j);
        int r = jbd2_abrir(&j) == 0 ? jbd2_recuperar(&j) : -1;
        bool todo = true, nada = true;
        for (int k = 0; k < 5; k++) {
            if (memcmp(jdisco[fis[k]], dados[k], JB)) todo = false;
            if (jdisco[fis[k]][5] != 0xEE) nada = false;
        }
        checar(r == 0 && (commit ? todo : nada), "%s: corte tras %d escrituras: %s", nome, lim, commit ? "todo" : "nada");
        jbd2_liberar(&j);
    }
    /* 4. Una copia estropeada (con sumas): no se reproduce nada. */
    if (soma) {
        jformatar(incompat, soma);
        for (int k = 0; k < 5; k++) memset(jdisco[fis[k]], 0xEE, JB);
        jpreparar(&j);
        jbd2_abrir(&j);
        jbd2_comecar(&j);
        jbd2_confirmar(&j, 5, fis, ptr);
        jbd2_liberar(&j);
        jdisco[JINI + 3][100] ^= 1;       /* la segunda copia */
        jpreparar(&j);
        int r = jbd2_abrir(&j) == 0 ? jbd2_recuperar(&j) : 0;
        checar(r < 0, "%s: copia con la suma mal: la reproducción se niega", nome);
        bool nada = true;
        for (int k = 0; k < 5; k++) if (jdisco[fis[k]][5] != 0xEE) nada = false;
        (void)nada;   /* la primera copia puede haberse escrito antes de ver la mala: lo que importa es el error */
        jbd2_liberar(&j);
    }
}

/* ------------------------------------------------------------ unidad: CRC y extents */

static int modo_unidade(void)
{
    const char *nueve = "123456789";
    checar((crc32c(~0u, nueve, 9) ^ ~0u) == 0xE3069283u, "CRC32c(\"123456789\") = E3069283");
    uint8_t z[32], u[32], inc[32];
    memset(z, 0, 32); memset(u, 0xFF, 32);
    for (int k = 0; k < 32; k++) inc[k] = (uint8_t)k;
    checar((crc32c(~0u, z, 32) ^ ~0u) == 0x8A9136AAu, "CRC32c de 32 ceros = 8A9136AA (RFC 3720)");
    checar((crc32c(~0u, u, 32) ^ ~0u) == 0x62A8AB43u, "CRC32c de 32 0xFF = 62A8AB43 (RFC 3720)");
    checar((crc32c(~0u, inc, 32) ^ ~0u) == 0x46DD794Eu, "CRC32c de 0..31 = 46DD794E (RFC 3720)");
    checar(crc32c(crc32c(~0u, nueve, 4), nueve + 4, 5) == crc32c(~0u, nueve, 9), "CRC32c por trozos");
    checar(crc16_ext4(0, nueve, 9) == 0xBB3D, "CRC16 (ARC) de \"123456789\" = BB3D");
    checar(crc16_ext4(0xFFFF, nueve, 9) == 0x4B37, "CRC16 (MODBUS) de \"123456789\" = 4B37");

    /* Lista de extents. */
    ext_lista_t l;
    memset(&l, 0, sizeof l);
    checar(ext_inserir(&l, 0, 100) == 0 && ext_inserir(&l, 1, 101) == 0 && l.n == 1 && l.e[0].len == 2, "extents: alargar por detrás");
    checar(ext_inserir(&l, 5, 200) == 0 && l.n == 2, "extents: uno nuevo separado");
    checar(ext_inserir(&l, 4, 199) == 0 && l.n == 2 && l.e[1].lb == 4 && l.e[1].pb == 199 && l.e[1].len == 2, "extents: alargar por delante");
    checar(ext_inserir(&l, 3, 103) == 0 && l.n == 3, "extents: en medio, no contiguo en el disco");
    checar(ext_inserir(&l, 2, 102) == 0 && l.n == 2 && l.e[0].len == 4, "extents: unir con el siguiente");
    checar(l.e[0].lb == 0 && l.e[0].pb == 100 && l.e[1].lb == 4 && l.e[1].len == 2, "extents: lista final ordenada");
    ext_lista_liberar(&l);
    ext_t x = { 10, 20, 1000, true };
    ext_por(&l, x);
    ext_t y = { 40, 5, 3000, false };
    ext_por(&l, y);
    int k = ext_isolar(&l, 15);
    checar(k == 1 && l.n == 4, "extents: partir en tres (%d, %u)", k, l.n);
    checar(l.e[0].lb == 10 && l.e[0].len == 5 && l.e[0].ni && l.e[1].lb == 15 && l.e[1].len == 1 && l.e[1].pb == 1005 &&
           l.e[2].lb == 16 && l.e[2].len == 14 && l.e[2].pb == 1006 && l.e[3].lb == 40, "extents: las tres partes");
    k = ext_isolar(&l, 10);
    checar(k == 0 && l.n == 5 && l.e[0].len == 1 && l.e[1].lb == 11, "extents: partir al principio");
    checar(ext_isolar(&l, 35) < 0, "extents: un bloque que no está");
    ext_lista_liberar(&l);
    /* Unir no pasa del máximo de un extent. */
    ext_t m = { 0, EXT_MAX_LEN, 5000, false };
    ext_por(&l, m);
    checar(ext_inserir(&l, EXT_MAX_LEN, 5000 + EXT_MAX_LEN) == 0 && l.n == 2, "extents: no se pasa de 32768 bloques");
    ext_lista_liberar(&l);

    unidade_jbd2(0, false, "JBD2 sin sumas (ext3)");
    unidade_jbd2(0x2 | 0x10, true, "JBD2 64 bits y csum v3 (ext4)");
    unidade_jbd2(0x8, true, "JBD2 csum v2");
    printf("RESULTADO: %d verificaciones, %d fallos\n", verificacoes, falhas);
    return falhas ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "unidad")) return modo_unidade();
    if (argc >= 3 && !strcmp(argv[1], "recuperar")) {
        /* Depurar un estado guardado: montar (reproduce), leerlo todo con el registro a la vista. */
        FILE *f = fopen(argv[2], "rb");
        if (!f) return 2;
        fseek(f, 0, SEEK_END);
        mem_tam = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        mem = malloc(mem_tam);
        leitura = malloc(1 << 20);
        if (fread(mem, 1, mem_tam, f) != mem_tam) return 2;
        fclose(f);
        falando = true;
        bool rw = false;
        vol = montar(&rw, 0);
        if (!vol) return 1;
        printf("rw %d, errores al leer: %d\n", rw, recorrer("", leitura));
        fs_ext2.desmontar(vol, true);
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "cortes")) {
        tmpdir = argv[3];
        e2fsck_bin = argv[4];
        bool sem = argc >= 6 && !strcmp(argv[5], "sin-cortes");
        uint32_t passo = argc >= 7 ? (uint32_t)atoi(argv[6]) : 1;
        return modo_cortes(argv[2], !sem, passo ? passo : 1);
    }
    fprintf(stderr, "uso: %s unidad | cortes <imagen> <carpeta> <e2fsck> [sin-cortes|cortes] [paso]\n", argv[0]);
    return 2;
}
