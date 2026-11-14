// SPDX-License-Identifier: GPL-2.0-or-later
/* chip8asm - montador e desmontador de CHIP-8 / SUPER-CHIP / XO-CHIP (D071).
 *
 *     chip8asm fuente.asm [-o juego.ch8]   monta (sem -o: troca .asm por .ch8)
 *     chip8asm -d rom.ch8 [-o salida.asm]  desmonta (sem -o: na tela)
 *
 * O mesmo arquivo compila para o SaguiOS e para o host (-DSAGUI_HOST): o
 * build monta as ROMs dos jogos com a versão do host. */
#include "chip8.h"
#include "montador.h"

static void uso(void)
{
    c8_erro("uso: chip8asm fuente.asm [-o salida.ch8]\n"
            "     chip8asm -d rom.ch8 [-o salida.asm]\n"
            "Ensamblador de CHIP-8, SUPER-CHIP y XO-CHIP. La sintaxis está en /ayuda/chip8.md.\n");
}

/* Texto que cresce (a saída do desmontador). */
typedef struct { char *b; int n, cap; } texto_t;

static void acrescentar(texto_t *t, const char *s)
{
    int k = (int)tam_texto(s);
    if (t->n + k + 1 > t->cap) {
        int cap = t->cap ? t->cap * 2 : 4096;
        while (cap < t->n + k + 1) cap *= 2;
        t->b = realoca(t->b, (size_t)cap);
        t->cap = cap;
    }
    memcpy(t->b + t->n, s, (size_t)k + 1);
    t->n += k;
}

static int desmontar_rom(const char *arq, const char *saida)
{
    int tam;
    uint8_t *rom = c8_ler_arquivo(arq, &tam);
    if (!rom) { c8_errof("chip8asm: no se ha podido leer %s\n", arq); return 1; }
    if (tam > 65536 - 0x200) { c8_errof("chip8asm: %s es demasiado grande (%d bytes)\n", arq, tam); return 1; }
    /* Memória de 64 KB com a ROM em 0x200: o desmontador lê como o emulador. */
    uint8_t *mem = aloca_zerado(65536);
    memcpy(mem + 0x200, rom, (size_t)tam);
    texto_t t = { 0, 0, 0 };
    char l[160], ins[64];
    const char *nome = acha_ultimo_char(arq, '/');
    formatar_texto(l, sizeof l, "; %s: %d bytes, desensamblado por chip8asm\n", nome ? nome + 1 : arq, tam);
    acrescentar(&t, l);
    int fim = 0x200 + tam;
    for (int end = 0x200; end < fim;) {
        int n = end + 1 < fim ? c8_desmontar(mem, 0xFFFF, (uint32_t)end, ins, sizeof ins) : 0;
        if (n == 4 && end + 3 >= fim) n = 2, formatar_texto(ins, sizeof ins, "dw 0x%04x", mem[end] << 8 | mem[end + 1]);
        if (n == 0) {
            formatar_texto(ins, sizeof ins, "db 0x%02x", mem[end]);
            formatar_texto(l, sizeof l, "    %-24s; %03x: %02x\n", ins, end, mem[end]);
            n = 1;
        } else if (n == 4) {
            formatar_texto(l, sizeof l, "    %-24s; %03x: %02x%02x %02x%02x\n", ins, end, mem[end], mem[end + 1],
                           mem[end + 2], mem[end + 3]);
        } else {
            formatar_texto(l, sizeof l, "    %-24s; %03x: %02x%02x\n", ins, end, mem[end], mem[end + 1]);
        }
        acrescentar(&t, l);
        end += n;
    }
    int r = 0;
    if (saida) {
        if (!c8_gravar_arquivo(saida, t.b, t.n)) { c8_errof("chip8asm: no se ha podido escribir %s\n", saida); r = 1; }
        else c8_saidaf("%s: %d bytes desensamblados en %s\n", arq, tam, saida);
    } else c8_saida(t.b);
    libera(t.b);
    libera(mem);
    libera(rom);
    return r;
}

int principal(int argc, char **argv)
{
    const char *entrada = NULL, *saida = NULL;
    bool des = false;
    for (int k = 1; k < argc; k++) {
        if (compara_texto(argv[k], "-o") == 0 && k + 1 < argc) saida = argv[++k];
        else if (compara_texto(argv[k], "-d") == 0) des = true;
        else if (compara_texto(argv[k], "-h") == 0 || compara_texto(argv[k], "--ayuda") == 0) { uso(); return 0; }
        else if (argv[k][0] == '-') { c8_errof("chip8asm: opción desconocida %s\n", argv[k]); uso(); return 2; }
        else if (!entrada) entrada = argv[k];
        else { uso(); return 2; }
    }
    if (!entrada) { uso(); return 2; }
    if (des) return desmontar_rom(entrada, saida);

    char nome[200];
    if (!saida) {
        copia_texto(nome, entrada, sizeof nome - 4);
        int n = (int)tam_texto(nome);
        if (n > 4 && compara_texto(nome + n - 4, ".asm") == 0) nome[n - 4] = 0;
        int k = (int)tam_texto(nome);
        nome[k] = '.'; nome[k + 1] = 'c'; nome[k + 2] = 'h'; nome[k + 3] = '8'; nome[k + 4] = 0;
        saida = nome;
    }
    uint8_t *rom;
    int tam;
    int erros = c8_montar(entrada, &rom, &tam);
    if (erros < 0) return 1;
    if (erros) { c8_errof("chip8asm: %d error%s, no se ha generado %s\n", erros, erros == 1 ? "" : "es", saida); return 1; }
    if (!c8_gravar_arquivo(saida, rom, tam)) { c8_errof("chip8asm: no se ha podido escribir %s\n", saida); return 1; }
    int modo = c8_modo_por_extensao(saida);
    if (modo >= 0 && tam > c8_rom_max(modo))
        c8_errof("chip8asm: aviso: %d bytes no caben en la memoria de %s (%d)\n", tam, c8_nome_modo(modo), c8_rom_max(modo));
    c8_saidaf("%s: %d bytes en %s\n", entrada, tam, saida);
    libera(rom);
    return 0;
}
