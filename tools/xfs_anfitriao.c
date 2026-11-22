// SPDX-License-Identifier: GPL-3.0-only
/* xfs_anfitriao.c - el driver XFS del kernel (kernel/fs/xfs.c) corriendo en el PC sobre una
 * imagen, para la prueba h446 (D446) y para depurar sin arrancar QEMU. Se compila de 32 bits
 * (como el kernel) con las cabeceras del kernel, sin las de la libc (chocan con tipos.h):
 *
 *   gcc -m32 -static -Ikernel/inc -o xfs_anfitriao tools/xfs_anfitriao.c kernel/fs/xfs.c \
 *       kernel/lib/crc32c.c kernel/lib/formatar.c
 *
 * Uso: xfs_anfitriao IMAGEN [-s SECTOR_INICIAL] ORDEN [RUTA]
 *   ls RUTA        nombre, tipo y tamaño de cada entrada
 *   cat RUTA       el contenido, tal cual, a la salida
 *   arbol          todo el volumen: «tipo tamaño ruta» por línea (16 niveles como mucho)
 *   info RUTA      tipo y tamaño
 *   escribir RUTA  lo que devuelve abrir para escribir (tiene que ser ERRO_SO_LEITURA)
 *   montar         solo monta (y dice el código)
 * Sale con 0 si todo fue bien, 1 si el montaje falló y 2 si una orden falló. */
#include "xfs.h"
#include "montagem.h"
#include "console.h"
#include "memoria.h"
#include "texto.h"

/* De la libc (sin sus cabeceras). */
int printf(const char *fmt, ...);
int fprintf(void *f, const char *fmt, ...);
extern void *stderr;
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
int open(const char *c, int f, ...);
int close(int fd);
ssize_t pread64(int fd, void *buf, size_t n, int64_t off);
ssize_t write(int fd, const void *buf, size_t n);
int64_t lseek64(int fd, int64_t off, int de);
long strtol(const char *s, char **fim, int base);

static int fd_img;
static int txt_compara_anf(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
static uint64_t inicio;

size_t txt_tamanho(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
char *txt_copia_n(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    if (!n) return dst;
    for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return dst;
}
void *kmalloc(size_t n) { return malloc(n); }
void *kcalloc(size_t n) { return calloc(1, n); }
void kfree(void *p) { free(p); }

void klog(const char *fmt, ...)
{
    char t[512];
    va_list ap;
    va_start(ap, fmt);
    formatar(t, sizeof t, fmt, ap);
    va_end(ap);
    fprintf(stderr, "klog: %s", t);
}

void disco_avisar(int tipo, const char *fmt, ...)
{
    char t[512];
    va_list ap;
    (void)tipo;
    va_start(ap, fmt);
    formatar(t, sizeof t, fmt, ap);
    va_end(ap);
    fprintf(stderr, "AVISO: %s\n", t);
}

int disco_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    if (lba >= d->setores || n > d->setores - lba) return ERRO_ACESSO;
    int64_t off = (int64_t)(inicio + lba) * 512;
    return pread64(fd_img, buf, (size_t)n * 512, off) == (ssize_t)(n * 512) ? 0 : ERRO_ACESSO;
}

static disco_t disco;
static void *vol;
static uint8_t arq[256];

static const char *nome_tipo(unsigned t) { return t == TIPO_PASTA ? "d" : t == TIPO_PROGRAMA ? "x" : "f"; }

static int cat(const char *rel)
{
    uint32_t tam;
    int r = fs_xfs.abrir(vol, rel, ABRIR_LER, arq, &tam);
    if (r < 0) { fprintf(stderr, "cat %s: %d\n", rel, r); return 2; }
    static uint8_t buf[200000];
    uint32_t pos = 0, passo = 4096;
    while (pos < tam) {
        /* tamaños de lectura variados, como los de los programas */
        uint32_t n = passo;
        if (n > tam - pos) n = tam - pos;
        int k = fs_xfs.ler(vol, arq, pos, buf, n);
        if (k <= 0) { fprintf(stderr, "cat %s: ler(%u) = %d\n", rel, pos, k); return 2; }
        write(1, buf, (size_t)k);
        pos += (uint32_t)k;
        passo = passo == 4096 ? 100000 : passo == 100000 ? 333 : 4096;
    }
    fs_xfs.fechar(vol, arq);
    return 0;
}

static int arvore(char *rel, int prof)
{
    entrada_pasta_t e;
    int erros = 0;
    size_t n = txt_tamanho(rel);
    for (int i = 0; ; i++) {
        int r = fs_xfs.listar(vol, rel, i, &e);
        if (r < 0) { fprintf(stderr, "listar %s: %d\n", rel, r); return 1; }
        if (r == 0) break;
        if (n + 1 + txt_tamanho(e.nome) + 1 > 1024) continue;
        size_t k = n;
        if (k) rel[k++] = '/';
        txt_copia_n(rel + k, e.nome, 1024 - k);
        info_arquivo_t inf;
        int ri = fs_xfs.info(vol, rel, &inf);
        printf("%s %u %s\n", nome_tipo(e.tipo), e.tamanho, rel);
        if (ri < 0) { fprintf(stderr, "info %s: %d\n", rel, ri); erros++; }
        if (e.tipo == TIPO_PASTA && prof < 16) {
            erros += arvore(rel, prof + 1);
        }
        rel[n] = 0;
        /* listar(i + 1) después de recorrer una subcarpeta: el cursor ya no vale y se empieza de nuevo */
    }
    return erros;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: xfs_anfitriao IMAGEN [-s SECTOR] ORDEN [RUTA]\n"); return 1; }
    int a = 2;
    if (argc > 4 && argv[2][0] == '-' && argv[2][1] == 's') { inicio = (uint64_t)strtol(argv[3], NULL, 10); a = 4; }
    fd_img = open(argv[1], 0);
    if (fd_img < 0) { fprintf(stderr, "no se abre %s\n", argv[1]); return 1; }
    int64_t tam = lseek64(fd_img, 0, 2);
    disco.presente = true;
    disco.usado = true;
    disco.tam_setor = 512;
    txt_copia_n(disco.nome, "img", sizeof disco.nome);
    disco.setores = (uint32_t)((uint64_t)tam / 512 - inicio);
    uint32_t opcoes = 0;
    int r = fs_xfs.montar(&disco, &opcoes, &vol);
    const char *ordem = argv[a], *rel = a + 1 < argc ? argv[a + 1] : "";
    if (r < 0) { printf("montar: %d\n", r); return 1; }
    int res = 0;
    if (!txt_compara_anf(ordem, "montar")) printf("montar: %d, opciones %x\n", r, opcoes);
    else if (!txt_compara_anf(ordem, "ls")) {
        entrada_pasta_t e;
        for (int i = 0; ; i++) {
            int k = fs_xfs.listar(vol, rel, i, &e);
            if (k < 0) { fprintf(stderr, "ls: %d\n", k); res = 2; break; }
            if (!k) break;
            printf("%s\t%s\t%u\n", e.nome, nome_tipo(e.tipo), e.tamanho);
        }
    } else if (!txt_compara_anf(ordem, "cat")) res = cat(rel);
    else if (!txt_compara_anf(ordem, "info")) {
        info_arquivo_t inf;
        int k = fs_xfs.info(vol, rel, &inf);
        if (k < 0) { printf("info: %d\n", k); res = 2; }
        else printf("%s %u\n", nome_tipo(inf.tipo), inf.tamanho);
    } else if (!txt_compara_anf(ordem, "escribir")) {
        uint32_t t;
        printf("abrir: %d\n", fs_xfs.abrir(vol, rel, ABRIR_ESCREVER, arq, &t));
        printf("crear: %d\n", fs_xfs.abrir(vol, "nuevo-de-sagui.txt", ABRIR_ESCREVER | ABRIR_CRIAR, arq, &t));
        printf("carpeta: %d\n", fs_xfs.criar_pasta(vol, "carpeta-nueva"));
        printf("borrar: %d\n", fs_xfs.apagar(vol, rel));
    } else if (!txt_compara_anf(ordem, "arbol")) {
        static char c[1100];
        res = arvore(c, 0) ? 2 : 0;
    } else { fprintf(stderr, "orden desconocida: %s\n", ordem); res = 1; }
    fs_xfs.desmontar(vol, false);
    return res;
}
