// SPDX-License-Identifier: GPL-2.0-or-later
/* sb16.c - la Sound Blaster 16 de Creative, en el bus ISA (D302).
 *
 * La tarjeta tiene un DSP (el procesador que convierte y mueve las muestras) que se manda con
 * órdenes de un byte por el puerto base + 0xC, y un mezclador CT1745 (base + 4 índice, base + 5
 * dato). Las muestras llegan por el DMA del 8237 (isa.c): un canal de 8 bits (0, 1, 3) y uno de
 * 16 bits (5, 6, 7). La IRQ, en la 2/9, 5, 7 o 10.
 *
 * Cómo se busca, sin tocar nada que no sea suyo:
 *   - solo si no hay tarjeta de sonido PCI con driver (audio.c): en los equipos modernos, con HDA,
 *     no se toca ningún puerto ISA;
 *   - en cada puerto posible (220h, 240h, 260h, 280h), antes de escribir se lee: el bit 7 del
 *     puerto de escritura del DSP (base + 0xC) tiene que bajar (DSP listo). Sin nada en el bus se
 *     lee FFh y no se escribe;
 *   - el reset del DSP (base + 6) y la respuesta AAh; la versión (orden E1h): 4.xx = SB16;
 *   - la IRQ y el DMA, del mezclador (registros 80h y 81h); la IRQ se comprueba pidiendo una al
 *     DSP (orden F2h) y mirando el IRR del 8259 con las interrupciones apagadas; el DMA, con una
 *     transferencia corta de prueba (silencio) en el canal;
 *   - si nada responde y hay un puente ISA, el ISA PnP (isapnp.c) configura una Vibra/AWE PnP.
 * Todas las esperas tienen límite (en vueltas, también con las interrupciones apagadas).
 *
 * Reproducción: 16 bits estéreo a 44,1 kHz (la tasa más alta del SB16) en el canal de 16 bits con
 * auto-init: un anillo de 16384 cuadros (64 KB, 371 ms) que el DSP recorre sin parar, con una
 * IRQ cada medio anillo (que se reconoce leyendo base + 0xF, o base + 0xE en las de 8 bits). El
 * mezclador del kernel escribe delante de la posición, que se lee del contador del 8237. Sin
 * canal de 16 bits, las muestras de 16 bits van por el de 8 (lo hace el propio SB16).
 *
 * Grabación (D292): micrófono, línea o CD, 16 bits estéreo a 44,1 kHz por el mismo canal de 16
 * bits. El SB16 solo hace una cosa a la vez en ese canal: mientras se graba, la salida se calla y
 * su posición avanza con el reloj (los programas que reproducen no se bloquean); al acabar la
 * grabación vuelve a sonar (semidúplex; ver la D302).
 *
 * Mezclador CT1745: maestro y PCM a 0 dB (el volumen es del mezclador del kernel, como en las
 * demás tarjetas), línea y CD a -14 dB hacia la salida (el cable de audio del CD-ROM suena), el
 * micrófono apagado en la salida (evita el acople) y el altavoz del PC a -6 dB. Se cambian con
 * "sonido --mezclador".
 *
 * Referencias: Creative "Sound Blaster Series Hardware Programming Guide" (DSP 4.xx y CT1745) y,
 * como consulta, el driver sb16 de FreeBSD (licencia BSD). Código propio. */
#include "sb16.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "isa.h"
#include "pci.h"
#include "portas.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

/* Puertos (desde la base) */
#define P_MIX_IND   0x4
#define P_MIX_DADO  0x5
#define P_RESET     0x6
#define P_LER       0xA
#define P_ESCREVER  0xC     /* escribir: orden o dato; leer: bit 7 = ocupado */
#define P_ESTADO    0xE     /* bit 7 = hay dato que leer; leerlo reconoce la IRQ de 8 bits */
#define P_ACK16     0xF     /* leerlo reconoce la IRQ de 16 bits */

/* Órdenes del DSP */
#define DSP_TAXA_SAIDA  0x41
#define DSP_TAXA_ENTRADA 0x42
#define DSP_16_SAIDA_AUTO 0xB6   /* 16 bits, D/A, auto-init, FIFO */
#define DSP_16_ENTRADA_AUTO 0xBE /* 16 bits, A/D, auto-init, FIFO */
#define DSP_16_SAIDA_UNA 0xB0    /* 16 bits, D/A, un bloque, sin FIFO */
#define DSP_8_SAIDA_UNA  0xC0    /* 8 bits, D/A, un bloque, sin FIFO */
#define DSP_MODO_ESTEREO_SINAL 0x30
#define DSP_MODO_MONO_SINAL    0x10
#define DSP_MODO_MONO_SEM_SINAL 0x00
#define DSP_PAUSA_8     0xD0
#define DSP_ALTAVOZ_SIM 0xD1
#define DSP_ALTAVOZ_NAO 0xD3
#define DSP_PAUSA_16    0xD5
#define DSP_FIM_AUTO_16 0xD9
#define DSP_FIM_AUTO_8  0xDA
#define DSP_VERSAO      0xE1
#define DSP_IRQ_8       0xF2

/* Mezclador CT1745 */
#define MIX_RESET     0x00
#define MIX_MAESTRO   0x30   /* 30h/31h izquierda/derecha; bits 7-3, 0 dB = F8h, pasos de 2 dB */
#define MIX_PCM       0x32
#define MIX_MIDI      0x34
#define MIX_CD        0x36
#define MIX_LINEA     0x38
#define MIX_MICRO     0x3A   /* mono */
#define MIX_ALTAVOZ   0x3B   /* bits 7-6: -18 a 0 dB */
#define MIX_SALIDA_SW 0x3C   /* bit 0 micro, 1-2 CD D/I, 3-4 línea D/I */
#define MIX_ENTRADA_I 0x3D   /* fuentes de la grabación, izquierda (mismos bits; 5-6 MIDI) */
#define MIX_ENTRADA_D 0x3E
#define MIX_GANHO_ENT 0x3F   /* 3Fh/40h: ganancia de entrada, bits 7-6 (x1, x2, x4, x8) */
#define MIX_GANHO_SAI 0x41   /* 41h/42h */
#define MIX_AGUDOS    0x44   /* 44h-47h: agudos y graves, 80h = plano */
#define MIX_IRQ       0x80
#define MIX_DMA       0x81
#define MIX_ESTADO_IRQ 0x82  /* bit 0: IRQ de 8 bits; bit 1: de 16 bits; bit 2: MPU-401 */

#define TAXA        44100
#define QUADROS     16384    /* 64 KB: 16 bits estéreo */
#define CAP_QUADROS 16384
#define SEM         0xFF

enum { PARADA, TOCANDO, GRAVANDO };

static uint16_t base;
static uint8_t  irq = SEM, dma8 = SEM, dma16 = SEM;
static uint8_t  canal;                       /* el de las muestras de 16 bits: dma16, o dma8 si no hay */
static uint8_t  ver_maior, ver_menor;
static uint8_t  opl;                         /* 0, 2 o 3 */
static bool     pnp;
static char     pnp_id[12];
static uint32_t anel_fis, cap_fis;
static int16_t *anel, *cap_anel;
static volatile uint8_t modo;
static bool     quer_tocar;                  /* el mezclador quiere sonar mientras se graba */
static uint32_t virt_t0, virt_pos;           /* la posición que avanza con el reloj mientras se graba */
static uint8_t  nivel[SB16_CANALES] = { 100, 100, 77, 77, 0, 66 };
static uint32_t n_irqs, n_irq8, n_irq16;
static captura_hw_t cap_hw;
static int      cap_entrada_atual;
static unsigned cap_ganho = 70;

/* ------------------------------------------------------------ DSP y mezclador */

#define VOLTAS 200000u        /* ~0,2 s de puerto 80h en el peor caso: nada se queda esperando para siempre */

static bool dsp_escrever(uint8_t v)
{
    for (uint32_t i = 0; i < VOLTAS; i++)
        if (!(porta_le8(base + P_ESCREVER) & 0x80)) { porta_escreve8(base + P_ESCREVER, v); return true; }
        else porta_pausa();
    return false;
}

static int dsp_ler(void)
{
    for (uint32_t i = 0; i < VOLTAS; i++)
        if (porta_le8(base + P_ESTADO) & 0x80) return porta_le8(base + P_LER);
        else porta_pausa();
    return -1;
}

static bool dsp_reset(void)
{
    porta_escreve8(base + P_RESET, 1);
    for (int i = 0; i < 10; i++) porta_pausa();           /* >= 3 µs */
    porta_escreve8(base + P_RESET, 0);
    for (uint32_t i = 0; i < 1000; i++) {                 /* la hoja dice ~100 µs */
        if ((porta_le8(base + P_ESTADO) & 0x80) && porta_le8(base + P_LER) == 0xAA) return true;
        porta_pausa();
    }
    return false;
}

/* Tira los datos que el DSP tenga para leer (la orden F2h deja un AAh, por ejemplo). */
static void dsp_vaciar(void)
{
    for (int i = 0; i < 16 && (porta_le8(base + P_ESTADO) & 0x80); i++) porta_le8(base + P_LER);
}

static uint8_t mix_ler(uint8_t r)
{
    bool i0 = int_salva_desliga();
    porta_escreve8(base + P_MIX_IND, r);
    uint8_t v = porta_le8(base + P_MIX_DADO);
    int_restaura(i0);
    return v;
}

static void mix_escrever(uint8_t r, uint8_t v)
{
    bool i0 = int_salva_desliga();
    porta_escreve8(base + P_MIX_IND, r);
    porta_escreve8(base + P_MIX_DADO, v);
    int_restaura(i0);
}

static void dsp_taxa(uint8_t ordem, uint32_t taxa)
{
    dsp_escrever(ordem);
    dsp_escrever((uint8_t)(taxa >> 8));
    dsp_escrever((uint8_t)taxa);
}

/* ------------------------------------------------------------ detección */

/* Antes de escribir nada en el puerto: el DSP tiene que decir "listo" (bit 7 de base + 0xC a 0)
 * alguna vez en ~1 ms. Un bus vacío da FFh. */
static bool alguem_em(uint16_t b)
{
    for (int i = 0; i < 1000; i++) {
        if (!(porta_le8(b + P_ESCREVER) & 0x80)) return true;
        porta_pausa();
    }
    return false;
}

static bool detectar(uint16_t b)
{
    if (!alguem_em(b)) return false;
    base = b;
    if (!dsp_reset()) return false;
    if (!dsp_escrever(DSP_VERSAO)) return false;
    int ma = dsp_ler(), me = dsp_ler();
    if (ma < 0 || me < 0) return false;
    ver_maior = (uint8_t)ma;
    ver_menor = (uint8_t)me;
    return true;
}

static uint8_t irq_do_bit(uint8_t v)
{
    switch (v & 0x0F) {
    case 1: return 9;          /* la 2 del bus ISA llega como 9 en el AT */
    case 2: return 5;
    case 4: return 7;
    case 8: return 10;
    }
    return SEM;
}

/* Pide al DSP una IRQ de 8 bits (F2h) con las interrupciones apagadas y mira qué línea se enciende
 * en el IRR del 8259. -> la línea o SEM. */
static uint8_t irq_provar(void)
{
    const uint16_t candidatas = (1u << 5) | (1u << 7) | (1u << 9) | (1u << 10);
    bool i0 = int_salva_desliga();
    uint16_t antes = pic_pedidos();
    uint8_t r = SEM;
    if (dsp_escrever(DSP_IRQ_8)) {
        for (uint32_t i = 0; i < 20000 && r == SEM; i++) {
            uint16_t novos = (uint16_t)(pic_pedidos() & ~antes & candidatas);
            if (novos) r = (uint8_t)__builtin_ctz(novos);
            else porta_pausa();
        }
    }
    porta_le8(base + P_ESTADO);                          /* reconoce la IRQ de 8 bits */
    dsp_vaciar();
    int_restaura(i0);
    return r;
}

/* Una transferencia corta de silencio por el canal: comprueba que la tarjeta lo usa de verdad
 * (llega al final del contador). Con las interrupciones encendidas (el reloj mide el plazo). */
static bool dma_provar(uint8_t c, bool de16, uint32_t fis)
{
    if (!isa_dma_canal_valido(c) || (de16 && c < 4) || (!de16 && c >= 4)) return false;
    /* 32 bytes de silencio: el búfer de la grabación está a cero y sin usar todavía */
    if (!isa_dma_programar(c, fis, 32, ISA_DMA_LER_MEM)) return false;
    isa_dma_terminou(c);                                 /* limpia un TC viejo */
    isa_dma_ativar(c, true);
    dsp_taxa(DSP_TAXA_SAIDA, 8000);
    if (de16) { dsp_escrever(DSP_16_SAIDA_UNA); dsp_escrever(DSP_MODO_MONO_SINAL); dsp_escrever(15); dsp_escrever(0); }
    else { dsp_escrever(DSP_8_SAIDA_UNA); dsp_escrever(DSP_MODO_MONO_SEM_SINAL); dsp_escrever(31); dsp_escrever(0); }
    /* Al acabar el bloque el DSP pide su IRQ (bit 0 u 1 del registro 82h), y el 8237 marca TC. */
    uint8_t bit = de16 ? 2 : 1;
    bool ok = false;
    uint32_t lim = relogio_tiques() + TIQUES_POR_SEGUNDO / 5;
    while (!ok && (int32_t)(relogio_tiques() - lim) < 0) {
        ok = isa_dma_terminou(c) || (mix_ler(MIX_ESTADO_IRQ) & bit);
        if (!ok) cpu_espera();
    }
    for (int i = 0; i < 50000 && !ok; i++) ok = isa_dma_terminou(c) || (mix_ler(MIX_ESTADO_IRQ) & bit);   /* sin reloj */
    isa_dma_ativar(c, false);
    uint8_t st = mix_ler(MIX_ESTADO_IRQ);
    if (st & 1) porta_le8(base + P_ESTADO);
    if (st & 2) porta_le8(base + P_ACK16);
    if (!ok) {
        /* el DSP se quedó esperando un DMA que no llega: se reinicia */
        dsp_reset();
        st = mix_ler(MIX_ESTADO_IRQ);
        if (st & 1) porta_le8(base + P_ESTADO);
        if (st & 2) porta_le8(base + P_ACK16);
    }
    return ok;
}

/* ------------------------------------------------------------ mezclador CT1745 */

static void aplicar_nivel(int c)
{
    uint8_t v5 = (uint8_t)((nivel[c] * 31u / 100u) << 3);
    switch (c) {
    case SB16_CANAL_MAESTRO: mix_escrever(MIX_MAESTRO, v5); mix_escrever(MIX_MAESTRO + 1, v5); break;
    case SB16_CANAL_PCM:     mix_escrever(MIX_PCM, v5); mix_escrever(MIX_PCM + 1, v5); break;
    case SB16_CANAL_LINEA:   mix_escrever(MIX_LINEA, v5); mix_escrever(MIX_LINEA + 1, v5); break;
    case SB16_CANAL_CD:      mix_escrever(MIX_CD, v5); mix_escrever(MIX_CD + 1, v5); break;
    case SB16_CANAL_MICRO:   mix_escrever(MIX_MICRO, v5); break;
    case SB16_CANAL_ALTAVOZ: mix_escrever(MIX_ALTAVOZ, (uint8_t)((nivel[c] * 3u / 100u) << 6)); break;
    }
    /* lo que está a 0 no llega a la salida */
    uint8_t sw = 0;
    if (nivel[SB16_CANAL_MICRO]) sw |= 0x01;
    if (nivel[SB16_CANAL_CD]) sw |= 0x06;
    if (nivel[SB16_CANAL_LINEA]) sw |= 0x18;
    mix_escrever(MIX_SALIDA_SW, sw);
}

static void mezclador_iniciar(void)
{
    mix_escrever(MIX_RESET, 0);
    for (int i = 0; i < 100; i++) porta_pausa();
    for (int c = 0; c < SB16_CANALES; c++) aplicar_nivel(c);
    mix_escrever(MIX_MIDI, 0xC0);
    mix_escrever(MIX_MIDI + 1, 0xC0);
    mix_escrever(MIX_GANHO_SAI, 0);
    mix_escrever(MIX_GANHO_SAI + 1, 0);
    for (int r = MIX_AGUDOS; r < MIX_AGUDOS + 4; r++) mix_escrever((uint8_t)r, 0x80);
}

/* ------------------------------------------------------------ reproducción */

static void dsp_parar_16(void)
{
    dsp_escrever(DSP_PAUSA_16);
    dsp_escrever(DSP_FIM_AUTO_16);
    isa_dma_ativar(canal, false);
    uint8_t st = mix_ler(MIX_ESTADO_IRQ);
    if (st & 1) porta_le8(base + P_ESTADO);
    if (st & 2) porta_le8(base + P_ACK16);
}

/* El DSP cuenta muestras (cada canal una) por bloque, menos 1: medio anillo. */
static void dsp_auto_16(uint8_t ordem, uint32_t quadros)
{
    uint32_t n = quadros / 2 * 2 - 1;
    dsp_escrever(ordem);
    dsp_escrever(DSP_MODO_ESTEREO_SINAL);
    dsp_escrever((uint8_t)n);
    dsp_escrever((uint8_t)(n >> 8));
}

static void tocar_hw(void)
{
    isa_dma_programar(canal, anel_fis, QUADROS * 4, ISA_DMA_LER_MEM | ISA_DMA_AUTO);
    isa_dma_ativar(canal, true);
    dsp_escrever(DSP_ALTAVOZ_SIM);
    dsp_taxa(DSP_TAXA_SAIDA, TAXA);
    dsp_auto_16(DSP_16_SAIDA_AUTO, QUADROS);
    modo = TOCANDO;
}

static uint32_t virt_agora(void)
{
    return (virt_pos + (relogio_tiques() - virt_t0) * (TAXA / TIQUES_POR_SEGUNDO)) % QUADROS;
}

static void ligar(void)
{
    if (modo == GRAVANDO) {               /* semidúplex: suena cuando acabe la grabación */
        quer_tocar = true;
        virt_pos = 0;
        virt_t0 = relogio_tiques();
        return;
    }
    if (modo == TOCANDO) dsp_parar_16();
    tocar_hw();
}

static void parar(void)
{
    if (modo == GRAVANDO) { quer_tocar = false; return; }
    if (modo == TOCANDO) dsp_parar_16();
    modo = PARADA;
}

static uint32_t posicao(void)
{
    if (modo == GRAVANDO) return virt_agora();
    return (isa_dma_feito(canal) / 4) % QUADROS;
}

static bool atender(void)
{
    uint8_t st = mix_ler(MIX_ESTADO_IRQ);
    bool r = false;
    if (st & 1) { porta_le8(base + P_ESTADO); n_irq8++; r = true; }
    if (st & 2) { porta_le8(base + P_ACK16); n_irq16++; r = true; }
    if (r) n_irqs++;
    return r;
}

/* ------------------------------------------------------------ grabación (D292) */

static bool cap_ligar(void)
{
    if (!cap_fis) return false;
    bool i0 = int_salva_desliga();
    if (modo == TOCANDO) {
        virt_pos = posicao();
        virt_t0 = relogio_tiques();
        dsp_parar_16();
        quer_tocar = true;
    } else quer_tocar = false;
    memset(cap_anel, 0, CAP_QUADROS * 4);
    isa_dma_programar(canal, cap_fis, CAP_QUADROS * 4, ISA_DMA_ESCREVER_MEM | ISA_DMA_AUTO);
    isa_dma_ativar(canal, true);
    dsp_taxa(DSP_TAXA_ENTRADA, TAXA);
    dsp_auto_16(DSP_16_ENTRADA_AUTO, CAP_QUADROS);
    modo = GRAVANDO;
    int_restaura(i0);
    klog("[SB16] grabando%s\n", quer_tocar ? " (la salida se calla mientras: semiduplex)" : "");
    return true;
}

static void cap_parar(void)
{
    bool i0 = int_salva_desliga();
    if (modo == GRAVANDO) {
        dsp_parar_16();
        modo = PARADA;
        if (quer_tocar) tocar_hw();
        quer_tocar = false;
    }
    int_restaura(i0);
}

static uint32_t cap_posicao(void)
{
    if (modo != GRAVANDO) return 0;
    return (isa_dma_feito(canal) / 4) % CAP_QUADROS;
}

static bool cap_entrada(int i)
{
    static const uint8_t esq[3] = { 0x01, 0x10, 0x04 }, dir[3] = { 0x01, 0x08, 0x02 };
    if (i < 0 || i > 2) return false;
    mix_escrever(MIX_ENTRADA_I, esq[i]);
    mix_escrever(MIX_ENTRADA_D, dir[i]);
    cap_entrada_atual = i;
    return true;
}

static void cap_ganho_def(unsigned pct)
{
    cap_ganho = pct > 100 ? 100 : pct;
    uint8_t g = (uint8_t)((cap_ganho >= 90 ? 3 : cap_ganho >= 60 ? 2 : cap_ganho >= 30 ? 1 : 0) << 6);
    mix_escrever(MIX_GANHO_ENT, g);
    mix_escrever(MIX_GANHO_ENT + 1, g);
    if (cap_entrada_atual == 0)          /* el micrófono: su nivel (el de la salida sigue en el interruptor) */
        mix_escrever(MIX_MICRO, (uint8_t)((cap_ganho * 31u / 100u) << 3));
}

static void cap_preparar(void)
{
    memset(&cap_hw, 0, sizeof cap_hw);
    void *mem;
    cap_fis = isa_dma_bufer(CAP_QUADROS * 4, true, &mem);
    if (!cap_fis) return;
    cap_anel = (int16_t *)mem;
    cap_hw.taxa = TAXA;
    cap_hw.quadros = CAP_QUADROS;
    cap_hw.anel = (const volatile int16_t *)cap_anel;
    cap_hw.n_entradas = 3;
    txt_copia_n(cap_hw.nombre[0], "Micr\xf3" "fono", 24);
    txt_copia_n(cap_hw.nombre[1], "Entrada de l\xednea", 24);
    txt_copia_n(cap_hw.nombre[2], "CD", 24);
    cap_hw.ligar = cap_ligar;
    cap_hw.parar = cap_parar;
    cap_hw.posicao = cap_posicao;
    cap_hw.entrada = cap_entrada;
    cap_hw.ganho = cap_ganho_def;
}

/* ------------------------------------------------------------ OPL (FM) */

static void opl_escrever(uint8_t r, uint8_t v)
{
    porta_escreve8(0x388, r);
    for (int i = 0; i < 6; i++) porta_le8(0x388);
    porta_escreve8(0x389, v);
    for (int i = 0; i < 35; i++) porta_le8(0x388);
}

/* La prueba del temporizador de AdLib: el temporizador 1 a FFh, se arranca y a los 80 µs las
 * banderas de estado (bits 7-5) tienen que valer C0h. Un OPL3 da 0 en los bits 2-1 (el OPL2, 6). */
static uint8_t opl_detectar(void)
{
    opl_escrever(4, 0x60);
    opl_escrever(4, 0x80);
    uint8_t s1 = porta_le8(0x388);
    opl_escrever(2, 0xFF);
    opl_escrever(4, 0x21);
    for (int i = 0; i < 200; i++) porta_pausa();
    uint8_t s2 = porta_le8(0x388);
    opl_escrever(4, 0x60);
    opl_escrever(4, 0x80);
    if ((s1 & 0xE0) != 0 || (s2 & 0xE0) != 0xC0) return 0;
    return (s1 & 0x06) ? 2 : 3;
}

/* ------------------------------------------------------------ búsqueda */

static void escolher_dma(uint8_t v)
{
    if (v & 0x02) dma8 = 1;
    else if (v & 0x01) dma8 = 0;
    else if (v & 0x08) dma8 = 3;
    if (v & 0x20) dma16 = 5;
    else if (v & 0x40) dma16 = 6;
    else if (v & 0x80) dma16 = 7;
}

bool sb16_procurar(placa_som_t *p)
{
    static const uint16_t portas[] = { 0x220, 0x240, 0x260, 0x280 };
    bool achou = false;
    for (unsigned i = 0; i < sizeof portas / sizeof portas[0] && !achou; i++) achou = detectar(portas[i]);
    isapnp_sb_t cfg;
    memset(&cfg, 0, sizeof cfg);
    if (achou) {
        /* ya activa (puentes o BIOS PnP): solo para saber si es PnP y cuál; no cambia su configuración */
        uint16_t b = base;
        isapnp_configurar_sb(&cfg, false);
        base = b;
        txt_copia_n(pnp_id, cfg.id, sizeof pnp_id);
    } else if (isapnp_configurar_sb(&cfg, true)) {
        achou = detectar(cfg.porta);
        if (achou) {
            pnp = true;
            txt_copia_n(pnp_id, cfg.id, sizeof pnp_id);
        } else klog("[SB16] la tarjeta ISA PnP %s no responde en %xh tras configurarla\n", cfg.id, cfg.porta);
    }
    if (!achou) { base = 0; return false; }

    char det[64];
    if (ver_maior < 4) {
        /* Sound Blaster 1.x/2.0/Pro (DSP 1-3): sin driver todavía (mejora 126) */
        snformatar(det, sizeof det, "DSP %u.%02u, sin driver (solo SB16)", ver_maior, ver_menor);
        isa_registrar(ver_maior == 3 ? "Sound Blaster Pro" : "Sound Blaster", base, SEM, SEM, SEM, det);
        klog("[SB16] %xh: DSP %u.%02u (no es SB16): sin driver\n", base, ver_maior, ver_menor);
        return false;
    }

    /* IRQ y DMA: del mezclador (antes del reset del mezclador), o de la configuración PnP */
    uint8_t ri = mix_ler(MIX_IRQ), rd = mix_ler(MIX_DMA);
    if (pnp) { irq = cfg.irq; dma8 = cfg.dma8; dma16 = cfg.dma16; }
    else {
        irq = irq_do_bit(ri);
        if (rd != 0xFF) escolher_dma(rd);
    }
    uint8_t vista = irq_provar();
    if (vista != SEM && vista != irq) {
        klog("[SB16] el mezclador dice IRQ %d pero la tarjeta pide la %u: se usa la %u\n", irq == SEM ? -1 : irq, vista, vista);
        irq = vista;
    }

    void *mem;
    anel_fis = isa_dma_bufer(QUADROS * 4, true, (void **)&mem);
    if (!anel_fis) { klog("[SB16] sin memoria baja para el DMA\n"); return false; }
    anel = (int16_t *)mem;
    cap_preparar();
    uint32_t prova = cap_fis ? cap_fis : anel_fis;

    /* Comprueba el DMA con una transferencia corta; si no vale (o el mezclador no lo dijo), prueba los demás. */
    if (dma16 != SEM && !dma_provar(dma16, true, prova)) {
        klog("[SB16] el DMA %u (16 bits) no responde\n", dma16);
        dma16 = SEM;
    }
    if (dma8 != SEM && !dma_provar(dma8, false, prova)) {
        klog("[SB16] el DMA %u (8 bits) no responde\n", dma8);
        dma8 = SEM;
    }
    if (dma16 == SEM && !pnp) {
        static const uint8_t c16[3] = { 5, 6, 7 };
        for (int i = 0; i < 3 && dma16 == SEM; i++) if (dma_provar(c16[i], true, prova)) dma16 = c16[i];
    }
    if (dma8 == SEM && !pnp) {
        static const uint8_t c8[3] = { 1, 3, 0 };
        for (int i = 0; i < 3 && dma8 == SEM; i++) if (dma_provar(c8[i], false, prova)) dma8 = c8[i];
    }
    canal = dma16 != SEM ? dma16 : dma8;

    mezclador_iniciar();
    opl = opl_detectar();
    cap_entrada(0);
    cap_ganho_def(cap_ganho);
    dsp_escrever(DSP_ALTAVOZ_SIM);

    char dmas[24];
    if (dma16 != SEM && dma8 != SEM) snformatar(dmas, sizeof dmas, "DMA %u y %u", dma8, dma16);
    else if (canal != SEM) snformatar(dmas, sizeof dmas, "DMA %u", canal);
    else txt_copia_n(dmas, "sin DMA", sizeof dmas);
    char irqs[12];
    if (irq != SEM) snformatar(irqs, sizeof irqs, "IRQ %u", irq);
    else txt_copia_n(irqs, "sin IRQ", sizeof irqs);
    snformatar(det, sizeof det, "DSP %u.%02u%s%s%s%s", ver_maior, ver_menor, opl == 3 ? ", OPL3 en 388h" : opl == 2 ? ", OPL2 en 388h" : "",
               pnp_id[0] ? ", ISA PnP " : "", pnp_id, canal == SEM ? ", sin DMA" : "");
    isa_registrar("Sound Blaster 16 (Creative)", base, irq, dma8, dma16, det);
    klog("[SB16] %xh: DSP %u.%02u, %s, %s%s%s%s\n", base, ver_maior, ver_menor, irqs, dmas,
         opl ? (opl == 3 ? ", OPL3 (FM sin driver)" : ", OPL2 (FM sin driver)") : "",
         pnp ? ", configurada por ISA PnP " : pnp_id[0] ? ", ISA PnP " : "", pnp_id);
    if (canal == SEM) {
        klog("[SB16] sin canal de DMA que funcione: sin sonido\n");
        return false;
    }

    p->driver = "sb16";
    txt_copia_n(p->placa, "Sound Blaster 16", sizeof p->placa);
    snformatar(p->codec, sizeof p->codec, "DSP %u.%02u; puerto %xh, %s, %s%s", ver_maior, ver_menor, base, irqs, dmas,
               opl == 3 ? "; OPL3" : opl == 2 ? "; OPL2" : "");
    memset(&p->pci, 0, sizeof p->pci);
    p->irq = irq == SEM ? 0 : irq;
    p->taxa = TAXA;
    p->quadros = QUADROS;
    p->anel = anel;
    p->ligar = ligar;
    p->parar = parar;
    p->posicao = posicao;
    p->atender = atender;
    p->cap = cap_hw;
    return true;
}

/* ------------------------------------------------------------ SB16_OP_* (sonido) */

int32_t sb16_chamada(uint32_t op, uint32_t a, uint32_t b)
{
    if (!base) return ERRO_NAO_EXISTE;
    if (op == SB16_OP_ESTADO) {
        info_sb16_t *s = (info_sb16_t *)a;
        if (b < sizeof *s || !usuario_ok(s, sizeof *s, true)) return ERRO_ENDERECO;
        memset(s, 0, sizeof *s);
        s->puerto = base;
        s->opl_puerto = opl ? 0x388 : 0;
        s->irq = irq;
        s->dma8 = dma8;
        s->dma16 = dma16;
        s->dsp_mayor = ver_maior;
        s->dsp_menor = ver_menor;
        s->opl = opl;
        s->pnp = pnp;
        s->modo = modo;
        for (int c = 0; c < SB16_CANALES; c++) s->nivel[c] = nivel[c];
        s->irqs = n_irqs;
        s->irqs_8 = n_irq8;
        s->irqs_16 = n_irq16;
        txt_copia_n(s->pnp_id, pnp_id, sizeof s->pnp_id);
        return 0;
    }
    if (op == SB16_OP_CANAL) {
        if (a >= SB16_CANALES) return ERRO_INVALIDO;
        int32_t v = (int32_t)b;
        if (v >= 0) {
            nivel[a] = (uint8_t)(v > 100 ? 100 : v);
            aplicar_nivel((int)a);
            klog("[SB16] mezclador: canal %u a %u\n", a, nivel[a]);
        }
        return nivel[a];
    }
    return ERRO_INVALIDO;
}
