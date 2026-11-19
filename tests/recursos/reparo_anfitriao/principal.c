// SPDX-License-Identifier: GPL-2.0-or-later
/* principal.c - corre reparo_ext2.c (D397) en el anfitrión sobre un archivo que hace de partición
 * (h397, parte A). Uso:
 *   reparo analisar <archivo>              (como «reparar -n»: no escribe)
 *   reparo reparar <archivo> [copia]       (como «reparar -y»; con copia, guarda antes los bloques)
 *   reparo deshacer <archivo> <copia>
 * Sale con 0 si estaba bien, 1 si lo ha reparado (o, al analizar, si hay problemas), 8 si hay un error. */
#include "particoes.h"

static FILE *f;
static uint32_t setores;

void registrar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "registro: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

int blocos_info(const char *nome, info_bloco_t *i)
{
    (void)nome;
    memset(i, 0, sizeof *i);
    i->setores = setores;
    return 0;
}

int blocos_ler(const char *nome, uint32_t lba, uint32_t n, void *buf)
{
    (void)nome;
    if (lba >= setores || n > setores - lba) return ERRO_INVALIDO;
    if (fseeko(f, (off_t)lba * 512, SEEK_SET) || fread(buf, 512, n, f) != n) return ERRO_INVALIDO;
    return 0;
}

int blocos_escrever(const char *nome, uint32_t lba, uint32_t n, const void *buf)
{
    (void)nome;
    if (lba >= setores || n > setores - lba) return ERRO_INVALIDO;
    if (fseeko(f, (off_t)lba * 512, SEEK_SET) || fwrite(buf, 512, n, f) != n) return ERRO_INVALIDO;
    return 0;
}

int abrir(const char *c, int modo)
{
    int fl = (modo & ABRIR_ESCREVER) ? O_WRONLY : O_RDONLY;
    if (modo & ABRIR_CRIAR) fl |= O_CREAT;
    if (modo & ABRIR_TRUNCAR) fl |= O_TRUNC;
    int fd = open(c, fl, 0644);
    return fd < 0 ? ERRO_NAO_EXISTE : fd;
}
int ler(int fd, void *buf, unsigned n) { return (int)read(fd, buf, n); }
int fechar(int fd) { return close(fd); }
int escrever_tudo(int fd, const void *buf, unsigned n) { return write(fd, buf, n) == (ssize_t)n ? (int)n : ERRO_ACESSO; }
int bytes_aleatorios(void *p, unsigned n) { memset(p, 0x5A, n); return (int)n; }
uint32_t tempo_ms(void) { return 0; }

static void aviso(const char *l, void *ctx) { (void)ctx; printf("%s\n", l); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: reparo analisar|reparar|deshacer <archivo> [copia]\n"); return 2; }
    f = fopen(argv[2], strcmp(argv[1], "analisar") == 0 ? "rb" : "r+b");
    if (!f) { perror(argv[2]); return 2; }
    fseeko(f, 0, SEEK_END);
    setores = (uint32_t)(ftello(f) / 512);
    char m[400] = "";
    if (strcmp(argv[1], "deshacer") == 0 && argc > 3) {
        int r = reparo_desfazer(argv[3], "vol", m, sizeof m);
        printf("%s\n", m);
        return r < 0 ? 8 : 0;
    }
    reparo_t *rp;
    int n = reparo_analisar("vol", aviso, NULL, &rp, m, sizeof m);
    if (n < 0) { printf("ERROR: %s\n", m); return 8; }
    reparo_resumo(rp);
    uint32_t bytes;
    printf("bloques que cambian: %u\n", reparo_blocos(rp, &bytes));
    int r = n ? 1 : 0;
    if (strcmp(argv[1], "reparar") == 0 && n) {
        if (argc > 3 && reparo_guardar_copia(rp, argv[3], m, sizeof m) < 0) { printf("ERROR: %s\n", m); r = 8; }
        else {
            if (argc > 3) printf("%s\n", m);
            if (reparo_gravar(rp, m, sizeof m) < 0) r = 8;
            printf("%s\n", m);
        }
    }
    reparo_fechar(rp);
    fclose(f);
    return r;
}
