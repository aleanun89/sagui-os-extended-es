// SPDX-License-Identifier: GPL-2.0-or-later
/* chip8.h - o núcleo do emulador CHIP-8 / SUPER-CHIP 1.1 / XO-CHIP (D070).
 *
 * O núcleo não desenha, não toca e não lê o teclado: o programa entrega as
 * 16 teclas a cada quadro, chama c8_executar com as instruções do quadro e
 * c8_quadro a 60 Hz, e lê c->tela (um byte por pixel, bit 0 = plano 1,
 * bit 1 = plano 2) quando c->tela_mudou. Assim o mesmo código roda na
 * janela, no modo de texto (-t) e no host, nas provas. */
#ifndef CHIP8_H
#define CHIP8_H
#include "c8base.h"

#define C8_LARG_MAX 128
#define C8_ALT_MAX  64
#define C8_INICIO   0x200     /* onde a ROM é carregada */
#define C8_FONTE    0x000     /* fonte pequena 4x5: 16 x 5 bytes */
#define C8_FONTE_G  0x050     /* fonte grande 8x10 (SCHIP): 16 x 10 bytes */

enum { C8_CHIP8, C8_SCHIP, C8_XOCHIP, C8_QTD_MODOS };
enum { C8_RODANDO, C8_TERMINOU, C8_ERRO };

/* Comportamentos que mudaram entre as máquinas (os "quirks"). Cada modo tem
 * os seus por padrão (c8_quirks_padrao); a linha de comando pode trocar. */
typedef struct {
    bool desloc_vy;       /* 8XY6/8XYE deslocam VY (COSMAC, XO-CHIP) ou o próprio VX (SCHIP) */
    bool incrementa_i;    /* FX55/FX65 deixam I depois do último registro (COSMAC, XO-CHIP) */
    bool salto_vx;        /* BNNN vira BXNN: soma VX em vez de V0 (SCHIP) */
    bool recorta;         /* o sprite é cortado na borda (senão dá a volta, XO-CHIP) */
    bool zera_vf;         /* 8XY1/8XY2/8XY3 zeram VF (COSMAC) */
    bool espera_vblank;   /* DXYN encerra o quadro (COSMAC esperava o retraço) */
} c8_quirks_t;

typedef struct {
    int         modo;
    c8_quirks_t q;
    uint8_t     mem[65536];
    uint32_t    mascara;          /* 0xFFF (4 KB) ou 0xFFFF (XO-CHIP, 64 KB) */
    uint8_t     v[16];
    uint16_t    i, pc;
    uint16_t    pilha[16];
    int         sp;
    uint8_t     dt, st;           /* temporizadores de retardo e de som */
    bool        alta;             /* 128x64 (00FF) */
    int         larg, alt;
    uint8_t     tela[C8_LARG_MAX * C8_ALT_MAX];
    uint8_t     planos;           /* planos de desenho (FN01); 1 fora do XO-CHIP */
    uint8_t     rpl[16];          /* "flags" do HP-48 (FX75/FX85) */
    uint8_t     padrao[16];       /* áudio do XO-CHIP (F002) */
    bool        tem_padrao;
    uint8_t     tom;              /* FX3A; 64 = 4000 amostras/s */
    uint16_t    teclas;           /* bit k = tecla k apertada agora */
    int         espera_reg;       /* FX0A: registro que recebe a tecla; -1 = não espera */
    uint16_t    espera_vistas;    /* teclas apertadas desde que FX0A começou */
    bool        vblank;           /* já desenhou neste quadro com espera_vblank */
    int         estado;           /* C8_RODANDO, C8_TERMINOU (00FD) ou C8_ERRO */
    char        erro[80];
    bool        tela_mudou;
    uint64_t    linhas_sujas;     /* bit y = a linha y mudou (a janela só redesenha essas) */
    uint32_t    semente;
    uint32_t    instrucoes;       /* executadas desde o início */
    int         tam_rom;
} c8_t;

void        c8_quirks_padrao(int modo, c8_quirks_t *q);
void        c8_iniciar(c8_t *c, int modo, const c8_quirks_t *q, uint32_t semente);
bool        c8_carregar(c8_t *c, const uint8_t *rom, int n);   /* false = não cabe na memória do modo */
int         c8_executar(c8_t *c, int n);      /* até n instruções; devolve quantas rodaram */
void        c8_quadro(c8_t *c);               /* 1/60 s: temporizadores e fim da espera de vblank */
void        c8_teclas(c8_t *c, uint16_t teclas);
uint32_t    c8_freq_som(const c8_t *c);       /* Hz a tocar agora (0 = silêncio) */
uint32_t    c8_taxa_padrao(const c8_t *c);    /* amostras/s do padrão XO-CHIP (FX3A) */
uint32_t    c8_soma_tela(const c8_t *c);      /* FNV-1a da tela, para as provas */
int         c8_rom_max(int modo);

const char *c8_nome_modo(int modo);           /* "CHIP-8", "SUPER-CHIP", "XO-CHIP" */
const char *c8_id_modo(int modo);             /* "chip8", "schip", "xochip" */
int         c8_modo_por_nome(const char *s);  /* -1 se não conhece */
int         c8_modo_por_extensao(const char *arquivo);   /* .ch8 .sc8 .xo8; -1 se outra */
bool        c8_quirk_texto(c8_quirks_t *q, const char *opcao);   /* "nome=valor" (ver ajuda) */
void        c8_quirks_descrever(const c8_quirks_t *q, char *buf, int max);

/* desmontador.c: o texto da instrução em mem[end] na sintaxe do chip8asm.
 * Devolve o tamanho (2, ou 4 no F000 NNNN). Opcode sem sentido sai "dw". */
int         c8_desmontar(const uint8_t *mem, uint32_t mascara, uint32_t end, char *buf, int max);

/* relatorio.c: a linha de comando (comum à janela e ao modo de texto) e o
 * modo de texto "-t N", que roda N quadros sem janela e mostra a tela e os
 * registros: é como as provas conferem o emulador, no SaguiOS e no host. */
typedef struct {
    int         modo;            /* -1 = pela extensão da ROM */
    const char *quirks[12];      /* "-q nome=valor", aplicados sobre os do modo */
    int         qtd_quirks;
    int         ipf;             /* instruções por quadro; 0 = o padrão do modo */
    const char *paleta;          /* -p nome ou número */
    const char *teclado;         /* -k es | abnt2 | numerico */
    int         quadros;         /* -t N; 0 = abrir a janela */
    uint32_t    semente;         /* -s N (o modo de texto usa uma fixa) */
    const char *eventos[32];     /* -e quadro:teclas (hex), só no modo de texto */
    int         qtd_eventos;
    const char *rom;
} c8_opcoes_t;

int  c8_ipf_padrao(int modo);
bool c8_ler_opcoes(int argc, char **argv, c8_opcoes_t *o);   /* false = já mostrou o erro */
void c8_uso(void);
/* Inicia a máquina com o modo e os quirks das opções e carrega a ROM. */
bool c8_preparar(c8_t *c, const c8_opcoes_t *o, const uint8_t *rom, int tam, int modo, char *erro, int max);
int  c8_relatorio(const c8_opcoes_t *o);
int  c8_principal_texto(int argc, char **argv);   /* o "main" do emulador no host */

#endif
