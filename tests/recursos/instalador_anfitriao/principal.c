// SPDX-License-Identifier: GPL-2.0-or-later
/* principal.c - la libsagui de particiones (D318) en el anfitrión, sobre un archivo que hace de
 * disco entero "hdb" (sus particiones se llaman hdb1, hdb5...). Uso (h318):
 *   disco tabla <img>
 *   disco planear <img> <número> <MB|0> [mover]      el plan de pt_planejar_tamanho (0 = lo máximo)
 *   disco tamano <img> <número> <MB|0>                pt_mudar_tamanho (moviendo si hace falta)
 *   disco reducir <img> <número> <MB>                 pt_executar(PA_REDIMENSIONAR)
 *   disco ampliar <img>                               pt_executar(PA_AMPLIAR)
 *   disco crear <img> <MB|0> ext2|fat32 logica|primaria <etiqueta>
 *   disco minimo <img> <número>                       fs_minimo
 * Escribe el mensaje y termina con 0 si fue bien, 1 si no. El deshacer va a $DISCO_TMP. */
#include "particoes.h"
#include "compressao.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static FILE *f;
static uint32_t total;

void registrar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "registro: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* "hdb" = el disco; "hdbN" = su partición N, según la tabla que haya ahora. */
static int volume(const char *nome, uint32_t *ini, uint32_t *n)
{
    if (strcmp(nome, "hdb") == 0) { *ini = 0; *n = total; return 0; }
    if (strncmp(nome, "hdb", 3) != 0) return ERRO_NAO_EXISTE;
    int num = atoi(nome + 3);
    pt_tabela_t t;
    if (pt_ler("hdb", &t) < 0) return ERRO_NAO_EXISTE;
    pt_part_t *p = pt_achar(&t, num);
    if (!p) return ERRO_NAO_EXISTE;
    *ini = p->inicio;
    *n = p->setores;
    return 0;
}

int blocos_info(const char *nome, info_bloco_t *i)
{
    uint32_t ini, n;
    memset(i, 0, sizeof *i);
    if (volume(nome, &ini, &n) < 0) return ERRO_NAO_EXISTE;
    i->setores = n;
    i->inicio = ini;
    i->bloco_fisico = 512;
    return 0;
}

int blocos_ler(const char *nome, uint32_t lba, uint32_t n, void *buf)
{
    uint32_t ini, tam;
    if (volume(nome, &ini, &tam) < 0) return ERRO_NAO_EXISTE;
    if (lba >= tam || n > tam - lba) return ERRO_INVALIDO;
    if (fseeko(f, ((off_t)ini + lba) * 512, SEEK_SET) || fread(buf, 512, n, f) != n) return ERRO_INVALIDO;
    return 0;
}

int blocos_escrever(const char *nome, uint32_t lba, uint32_t n, const void *buf)
{
    uint32_t ini, tam;
    if (volume(nome, &ini, &tam) < 0) return ERRO_NAO_EXISTE;
    if (lba >= tam || n > tam - lba) return ERRO_INVALIDO;
    if (fseeko(f, ((off_t)ini + lba) * 512, SEEK_SET) || fwrite(buf, 512, n, f) != n) return ERRO_INVALIDO;
    fflush(f);
    return 0;
}

int blocos_zerar(const char *nome, uint32_t lba, uint32_t n)
{
    static uint8_t z[512];
    for (uint32_t i = 0; i < n; i++) if (blocos_escrever(nome, lba + i, 1, z) < 0) return ERRO_INVALIDO;
    return 0;
}

int blocos_descartar(const char *nome, uint32_t lba, uint32_t n) { (void)nome; (void)lba; (void)n; return ERRO_NAO_SUPORTADO; }
int blocos_reservar(const char *d, int c) { (void)d; (void)c; return 0; }
int blocos_liberar(const char *d) { (void)d; return 0; }
int blocos_reler(const char *d) { (void)d; return 0; }
int bytes_aleatorios(void *p, unsigned n) { memset(p, 0x5A, n); return (int)n; }
uint32_t tempo_ms(void) { return 0; }
int listar_discos(int i, info_disco_t *d) { (void)i; (void)d; return 0; }   /* nada montado */
int desmontar(const char *p) { (void)p; return 0; }
int montar_em(const char *v, const char *p, int o) { (void)v; (void)p; (void)o; return 0; }

/* El deshacer de pt_gravar ("/tmp/particiones/...") va a $DISCO_TMP. */
static void ruta(const char *c, char *s, size_t max)
{
    const char *base = getenv("DISCO_TMP");
    snprintf(s, max, "%s/%s", base ? base : ".", strrchr(c, '/') ? strrchr(c, '/') + 1 : c);
}
int criar_pasta(const char *c) { (void)c; return 0; }
int abrir(const char *c, int modo)
{
    char s[512];
    ruta(c, s, sizeof s);
    int fl = (modo & ABRIR_ESCREVER) ? O_WRONLY : O_RDONLY;
    if (modo & ABRIR_CRIAR) fl |= O_CREAT;
    if (modo & ABRIR_TRUNCAR) fl |= O_TRUNC;
    int fd = open(s, fl, 0644);
    return fd < 0 ? ERRO_NAO_EXISTE : fd;
}
int fechar(int fd) { return close(fd); }
int apagar(const char *c) { char s[512]; ruta(c, s, sizeof s); return unlink(s); }
int escrever_tudo(int fd, const void *buf, unsigned n) { return write(fd, buf, n) == (ssize_t)n ? 0 : ERRO_INVALIDO; }
char *ler_todo_fd(int fd, unsigned *tam)
{
    struct stat st;
    if (fstat(fd, &st) < 0) return NULL;
    char *b = malloc((size_t)st.st_size + 1);
    if (!b) return NULL;
    ssize_t n = read(fd, b, (size_t)st.st_size);
    if (n < 0) { free(b); return NULL; }
    b[n] = 0;
    *tam = (unsigned)n;
    return b;
}

static void mostrar(void)
{
    pt_tabela_t t;
    if (pt_ler("hdb", &t) < 0) { printf("sin tabla\n"); return; }
    for (int i = 0; i < 4; i++) if (t.prim[i].tipo) printf("p%d %02x %u %u\n", i + 1, t.prim[i].tipo, t.prim[i].inicio, t.prim[i].setores);
    for (int k = 0; k < t.nlog; k++) printf("l%d %02x %u %u ebr=%u\n", k + 5, t.log[k].tipo, t.log[k].inicio, t.log[k].setores, t.log[k].ebr);
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    f = fopen(argv[2], "r+b");
    if (!f) { perror(argv[2]); return 2; }
    fseeko(f, 0, SEEK_END);
    total = (uint32_t)(ftello(f) / 512);
    char m[400] = "";
    int r = 0;
    const char *o = argv[1];
    if (strcmp(o, "tabla") == 0) { mostrar(); return 0; }
    if (strcmp(o, "minimo") == 0 && argc > 3) { char v[16]; snprintf(v, sizeof v, "hdb%s", argv[3]); printf("%u\n", fs_minimo(v)); return 0; }
    if ((strcmp(o, "planear") == 0 || strcmp(o, "tamano") == 0) && argc > 4) {
        pt_tabela_t t;
        pt_tamanho_t p;
        if (pt_ler("hdb", &t) < 0) return 1;
        bool mover = strcmp(o, "tamano") == 0 || (argc > 5 && strcmp(argv[5], "mover") == 0);
        r = pt_planejar_tamanho(&t, atoi(argv[3]), (uint32_t)atoi(argv[4]) * 2048u, mover, &p, m, sizeof m);
        if (r < 0) { printf("%s\n", m); return 1; }
        printf("antes=%u depois=%u maximo=%u sem_mover=%u ext_fim=%u nmover=%d", p.antes, p.depois, p.maximo, p.maximo_sem_mover, p.ext_fim, p.nmover);
        for (int j = 0; j < p.nmover; j++) printf(" %d->%u", p.mover[j], p.novo_ini[j]);
        printf("\n");
        if (strcmp(o, "planear") == 0) return 0;
        r = pt_mudar_tamanho(&p, 0, true, m, sizeof m);
    } else if (strcmp(o, "reducir") == 0 && argc > 4) {
        pt_acao_t a = { 0 };
        a.op = PA_REDIMENSIONAR; copia_texto(a.disco, "hdb", sizeof a.disco);
        a.numero = atoi(argv[3]); a.setores = (uint32_t)atoi(argv[4]) * 2048u;
        r = pt_executar(&a, m, sizeof m);
    } else if (strcmp(o, "ampliar") == 0) {
        pt_acao_t a = { 0 };
        a.op = PA_AMPLIAR; copia_texto(a.disco, "hdb", sizeof a.disco);
        r = pt_executar(&a, m, sizeof m);
    } else if (strcmp(o, "crear") == 0 && argc > 6) {
        pt_acao_t a = { 0 };
        a.op = PA_CRIAR; copia_texto(a.disco, "hdb", sizeof a.disco);
        a.setores = (uint32_t)atoi(argv[3]) * 2048u;
        a.fs = strcmp(argv[4], "fat32") == 0 ? PA_FS_FAT32 : PA_FS_EXT2;
        a.logica = strcmp(argv[5], "logica") == 0;
        copia_texto(a.rotulo, argv[6], sizeof a.rotulo);
        r = pt_executar(&a, m, sizeof m);
    } else return 2;
    printf("%s\n", m);
    mostrar();
    fclose(f);
    return r < 0 ? 1 : 0;
}

/* La CRC32 de la GPT (en SaguiOS, compressao.c). */
uint32_t crc32_somar(uint32_t crc, const void *dados, size_t n)
{
    const uint8_t *p = dados;
    crc = ~crc;
    while (n--) { crc ^= *p++; for (int k = 0; k < 8; k++) crc = crc & 1 ? 0xEDB88320u ^ (crc >> 1) : crc >> 1; }
    return ~crc;
}
