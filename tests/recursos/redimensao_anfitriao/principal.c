// SPDX-License-Identifier: GPL-2.0-or-later
/* principal.c - corre redimensao.c (D213) en el anfitrión sobre un archivo que hace de
 * partición (h213). Uso:
 *   redimensao verificar <archivo>
 *   redimensao minimo <archivo>
 *   redimensao redimensionar <archivo> <sectores>
 *   redimensao crecer-fat32 <archivo>          (fs_crescer_fat32, de formatacao.c)
 *   redimensao reducir-fat32 <archivo> <sectores>   (D318)
 *   redimensao minimo-fat32 <archivo>                (D318)
 *   redimensao formatear-fat32|formatear-ext2 <archivo> <etiqueta>
 * Escribe el mensaje y termina con 0 si fue bien, 1 si no. */
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

int blocos_zerar(const char *nome, uint32_t lba, uint32_t n)
{
    static uint8_t z[512];
    for (uint32_t i = 0; i < n; i++) if (blocos_escrever(nome, lba + i, 1, z) < 0) return ERRO_INVALIDO;
    return 0;
}

int blocos_descartar(const char *nome, uint32_t lba, uint32_t n) { (void)nome; (void)lba; (void)n; return ERRO_NAO_SUPORTADO; }
int bytes_aleatorios(void *p, unsigned n) { memset(p, 0x5A, n); return (int)n; }
uint32_t tempo_ms(void) { return 0; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: redimensao verificar|minimo|redimensionar <archivo> [sectores]\n"); return 2; }
    f = fopen(argv[2], "r+b");
    if (!f) { perror(argv[2]); return 2; }
    fseeko(f, 0, SEEK_END);
    setores = (uint32_t)(ftello(f) / 512);
    char m[400] = "";
    int r = 0;
    if (strcmp(argv[1], "verificar") == 0) r = fs_verificar("vol", m, sizeof m);
    else if (strcmp(argv[1], "minimo") == 0) { printf("%u\n", fs_ext2_minimo("vol")); return 0; }
    else if (strcmp(argv[1], "redimensionar") == 0 && argc > 3)
        r = fs_redimensionar_ext2("vol", (uint32_t)strtoul(argv[3], NULL, 10), NULL, NULL, m, sizeof m);
    else if (strcmp(argv[1], "crecer-fat32") == 0) r = fs_crescer_fat32("vol", m, sizeof m);
    else if (strcmp(argv[1], "reducir-fat32") == 0 && argc > 3)
        r = fs_reduzir_fat32("vol", (uint32_t)strtoul(argv[3], NULL, 10), m, sizeof m);
    else if (strcmp(argv[1], "minimo-fat32") == 0) { printf("%u\n", fs_fat32_minimo("vol")); return 0; }
    else if (strcmp(argv[1], "formatear-fat32") == 0 && argc > 3) r = fs_formatar_fat32("vol", argv[3], NULL, NULL, m, sizeof m);
    else if (strcmp(argv[1], "formatear-ext2") == 0 && argc > 3) r = fs_formatar_ext2("vol", argv[3], NULL, NULL, m, sizeof m);
    else return 2;
    fclose(f);
    printf("%s\n", m);
    return r < 0 ? 1 : 0;
}
