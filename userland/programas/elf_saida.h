// SPDX-License-Identifier: GPL-2.0-or-later
/* elf_saida.h - escribe el ejecutable ELF32 de los compiladores de SaguiOS (cc
 * y pascal): el código, los datos y la BSS de gerador_x86.h se convierten en tres
 * segmentos PT_LOAD con direcciones fijas, sin reubicación (D085). Se incluye
 * después de gerador_x86.h. */

static void por16(uint8_t *b, int p, uint32_t v) { b[p] = v & 0xFF; b[p + 1] = (v >> 8) & 0xFF; }
static void por32(uint8_t *b, int p, uint32_t v) { por16(b, p, v & 0xFFFF); por16(b, p + 2, v >> 16); }

static void cabecalho_programa(uint8_t *b, int p, uint32_t desloc, uint32_t virt, uint32_t no_arq, uint32_t na_mem, uint32_t flags)
{
    por32(b, p, 1);                /* PT_LOAD */
    por32(b, p + 4, desloc);
    por32(b, p + 8, virt);
    por32(b, p + 12, virt);
    por32(b, p + 16, no_arq);
    por32(b, p + 20, na_mem);
    por32(b, p + 24, flags);       /* 4 = ler, 2 = escrever, 1 = executar */
    por32(b, p + 28, 0x1000);
}

static int escrever_elf(const char *caminho)
{
    static uint8_t zeros[4096];
    uint8_t cab[COD_DESLOC];
    memset(cab, 0, sizeof cab);
    uint32_t fim_texto = COD_DESLOC + ncod;
     /* Linux exige desplazamiento en el archivo = dirección (módulo 4 KB); el
      * cargador de SaguiOS copia byte a byte y no necesita eso. */
    uint32_t desloc_dados = modo_linux ? (fim_texto + 4095) & ~4095u : fim_texto;
    int nph = 1 + (ndados > 0) + (nbss > 0);
    cab[0] = 0x7F; cab[1] = 'E'; cab[2] = 'L'; cab[3] = 'F';
    cab[4] = 1;                    /* 32 bits */
    cab[5] = 1;                    /* little-endian */
    cab[6] = 1;
    por16(cab, 16, 2);             /* executável */
    por16(cab, 18, 3);             /* Intel 386 */
    por32(cab, 20, 1);
    por32(cab, 24, COD_BASE);      /* entrada: _inicio */
    por32(cab, 28, 52);            /* cabeceras de programa justo después de ésta */
    por16(cab, 40, 52);
    por16(cab, 42, 32);
    por16(cab, 44, (uint32_t)nph);
    por16(cab, 46, 40);
    int p = 52;
    cabecalho_programa(cab, p, 0, ARQ_BASE, fim_texto, fim_texto, 5);
    p += 32;
    if (ndados) { cabecalho_programa(cab, p, desloc_dados, DADOS_BASE, ndados, ndados, 6); p += 32; }
    if (nbss) cabecalho_programa(cab, p, 0, BSS_BASE, 0, nbss, 6);

    apagar(caminho);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, cab, sizeof cab);
    if (r >= 0) r = escrever_tudo(fd, cod, (unsigned)ncod);
    if (r >= 0 && ndados) {
        uint32_t falta = desloc_dados - fim_texto;
        while (r >= 0 && falta) {
            uint32_t k = falta > sizeof zeros ? sizeof zeros : falta;
            r = escrever_tudo(fd, zeros, k);
            falta -= k;
        }
        if (r >= 0) r = escrever_tudo(fd, dados, (unsigned)ndados);
    }
    fechar(fd);
    return r < 0 ? r : 0;
}
