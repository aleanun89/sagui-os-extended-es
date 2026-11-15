// SPDX-License-Identifier: GPL-2.0-or-later
/* ac97.c - controladores AC'97 de Intel (ICH) y compatibles (D089).
 *
 * El controlador tiene dos franjas de puertos: el NAM ("native audio mixer",
 * BAR0), que son los registros del códec AC'97 (volúmenes, tasa), y el NABM
 * ("native audio bus master", BAR1), que son los canales de DMA. Solo usamos el
 * canal de salida PCM (PO, en NABM + 0x10). Lee una lista de 32
 * descriptores (dirección, tamaño en muestras y el bit "interrumpe al final");
 * CIV es el descriptor actual, PICB cuántas muestras faltan en él y LVI el último
 * válido: el DMA para cuando termina el LVI. Para reproducir el anillo sin parar, el
 * LVI se mantiene siempre uno detrás del CIV (en cada lectura de la posición).
 *
 * Los 32 descriptores apuntan a 32 trozos de 1024 cuadros de un anillo de
 * 32768 cuadros (16 bits, estéreo, 48 kHz: la tasa fija del AC'97 sin VRA).
 *
 * Referencia: Intel "I/O Controller Hub AC'97 Programmer's Reference
 * Manual" y la especificación AC'97 2.3 (registros del códec). El SiS 7012
 * intercambia SR y PICB y cuenta bytes en vez de muestras. */
#include "audio.h"
#include "console.h"
#include "portas.h"
#include "relogio.h"
#include "texto.h"

/* NAM (códec): entradas de grabación (D292) */
#define NAM_FONE_MIC   0x0E
#define NAM_LINEA      0x10
#define NAM_AUX        0x16
#define NAM_REC_SEL    0x1A
#define NAM_REC_GANHO  0x1C
/* NABM: canal de entrada PCM (PI, en NABM + 0x00): mismos registros que el PO, 0x10 más abajo */
#define PI_BASE        0x00
/* NAM (códec) */
#define NAM_RESET      0x00
#define NAM_MESTRE     0x02
#define NAM_FONES      0x04
#define NAM_PCM        0x18
#define NAM_ENERGIA    0x26
#define NAM_ID1        0x7C
#define NAM_ID2        0x7E
/* NABM: canal de salida PCM y control global */
#define PO_BDBAR       0x10
#define PO_CIV         0x14
#define PO_LVI         0x15
#define PO_SR          0x16
#define PO_PICB        0x18
#define PO_CR          0x1B
#define GLOB_CNT       0x2C
#define GLOB_STA       0x30

#define CR_RPBM  0x01   /* DMA activado */
#define CR_RR    0x02   /* reinicia o canal */
#define CR_IOCE  0x10   /* interrupción al final de cada descriptor */
#define SR_LIMPAR 0x1C  /* LVBCI | BCIS | FIFOE: escribir 1 borra */
#define BD_IOC   0x8000

#define N_DESC   32
#define PEDACO   1024                  /* cuadros por descriptor */
#define QUADROS  (N_DESC * PEDACO)     /* 32768 */

static uint16_t nam, nabm;
static uint8_t  reg_sr = PO_SR, reg_picb = PO_PICB;
static uint32_t unidade = 2;           /* bytes por unidad del PICB y de los descriptores (SiS: 1) */
static uint32_t bdl_fis, anel_fis;
static uint32_t *bdl;

static const struct { uint16_t fab, mod; const char *nome; bool sis; } modelos[] = {
    { 0x8086, 0x2415, "Intel 82801AA (ICH) AC'97", false },
    { 0x8086, 0x2425, "Intel 82801AB (ICH0) AC'97", false },
    { 0x8086, 0x2445, "Intel 82801BA (ICH2) AC'97", false },
    { 0x8086, 0x2485, "Intel 82801CA (ICH3) AC'97", false },
    { 0x8086, 0x24C5, "Intel 82801DB (ICH4) AC'97", false },
    { 0x8086, 0x24D5, "Intel 82801EB (ICH5) AC'97", false },
    { 0x8086, 0x25A6, "Intel 6300ESB AC'97", false },
    { 0x8086, 0x266E, "Intel 82801FB (ICH6) AC'97", false },
    { 0x8086, 0x27DE, "Intel 82801GB (ICH7) AC'97", false },
    { 0x8086, 0x7195, "Intel 82443MX AC'97", false },
    { 0x1039, 0x7012, "SiS 7012 AC'97", true },
    { 0x10DE, 0x01B1, "NVIDIA nForce AC'97", false },
    { 0x10DE, 0x006A, "NVIDIA nForce2 AC'97", false },
    { 0x10DE, 0x00DA, "NVIDIA nForce3 AC'97", false },
    { 0x10DE, 0x00EA, "NVIDIA nForce3 AC'97", false },
    { 0x10DE, 0x0059, "NVIDIA CK804 AC'97", false },
    { 0x10DE, 0x008A, "NVIDIA MCP2S AC'97", false },
    { 0x10DE, 0x003A, "NVIDIA MCP04 AC'97", false },
    { 0x1022, 0x746D, "AMD 8111 AC'97", false },
    { 0x1022, 0x7445, "AMD 768 AC'97", false },
};

static bool antes(uint32_t limite) { return (int32_t)(relogio_tiques() - limite) < 0; }

static void canal_reiniciar(void)
{
    porta_escreve8(nabm + PO_CR, 0);
    porta_escreve8(nabm + PO_CR, CR_RR);
    uint32_t lim = relogio_tiques() + 3;
    while ((porta_le8(nabm + PO_CR) & CR_RR) && antes(lim)) porta_pausa();
    porta_escreve16(nabm + reg_sr, SR_LIMPAR);
}

static void ligar(void)
{
    canal_reiniciar();
    porta_escreve32(nabm + PO_BDBAR, bdl_fis);
    porta_escreve8(nabm + PO_LVI, N_DESC - 1);
    porta_escreve8(nabm + PO_CR, CR_RPBM | CR_IOCE);
}

static void parar(void)
{
    canal_reiniciar();
}

/* Cuadro que la tarjeta está reproduciendo. Aprovecha para empujar el LVI. */
static uint32_t posicao(void)
{
    uint8_t civ = 0;
    uint32_t picb = 0;
    for (int t = 0; t < 4; t++) {   /* CIV y PICB no se leen juntos: comprueba que el CIV no cambió a mitad */
        civ = porta_le8(nabm + PO_CIV) & (N_DESC - 1);
        picb = porta_le16(nabm + reg_picb);
        if ((porta_le8(nabm + PO_CIV) & (N_DESC - 1)) == civ) break;
    }
    porta_escreve8(nabm + PO_LVI, (uint8_t)((civ + N_DESC - 1) & (N_DESC - 1)));
    uint32_t faltam = picb * unidade / 4;   /* cuadros que faltan en el descriptor actual */
    if (!picb || faltam > PEDACO) faltam = PEDACO;   /* todavía no empezó el descriptor */
    return civ * PEDACO + (PEDACO - faltam);
}

static bool atender(void)
{
    uint16_t sr = porta_le16(nabm + reg_sr);
    if (!(sr & SR_LIMPAR)) return false;
    porta_escreve16(nabm + reg_sr, sr & SR_LIMPAR);
    return true;
}

/* ------------------------------------------------------------ captura (D292) */

/* Canal PCM IN (PI): 32 descriptores de 512 cuadros = un anillo de 16384 cuadros a 48 kHz, sin interrupción
 * (el tic del reloj vacía el anillo). Entradas: micrófono, línea y auxiliar, por el selector de grabación. */
#define CAP_DESC     32
#define CAP_PEDACO   512
#define CAP_QUADROS  (CAP_DESC * CAP_PEDACO)
static uint32_t cap_bdl_fis, cap_anel_fis;
static uint32_t *cap_bdl;
static captura_hw_t cap_hw;
static const uint8_t cap_fonte[3] = { 0, 4, 3 };   /* REC_SEL: micrófono, línea, auxiliar */

static void cap_reiniciar(void)
{
    porta_escreve8(nabm + PI_BASE + 0x0B, 0);
    porta_escreve8(nabm + PI_BASE + 0x0B, CR_RR);
    uint32_t lim = relogio_tiques() + 3;
    while ((porta_le8(nabm + PI_BASE + 0x0B) & CR_RR) && antes(lim)) porta_pausa();
    porta_escreve16(nabm + reg_sr - 0x10, SR_LIMPAR);
}

static bool cap_ligar(void)
{
    cap_reiniciar();
    porta_escreve32(nabm + PI_BASE + 0x00, cap_bdl_fis);
    porta_escreve8(nabm + PI_BASE + 0x05, CAP_DESC - 1);
    porta_escreve8(nabm + PI_BASE + 0x0B, CR_RPBM);
    return true;
}

static void cap_parar(void) { cap_reiniciar(); }

static uint32_t cap_posicao(void)
{
    uint8_t civ = 0;
    uint32_t picb = 0;
    for (int t = 0; t < 4; t++) {
        civ = porta_le8(nabm + PI_BASE + 0x04) & (CAP_DESC - 1);
        picb = porta_le16(nabm + reg_picb - 0x10);
        if ((porta_le8(nabm + PI_BASE + 0x04) & (CAP_DESC - 1)) == civ) break;
    }
    porta_escreve8(nabm + PI_BASE + 0x05, (uint8_t)((civ + CAP_DESC - 1) & (CAP_DESC - 1)));
    uint32_t faltam = picb * unidade / 4;
    if (!picb || faltam > CAP_PEDACO) faltam = CAP_PEDACO;
    return civ * CAP_PEDACO + (CAP_PEDACO - faltam);
}

static bool cap_entrada(int i)
{
    if (i < 0 || i > 2) return false;
    uint16_t f = cap_fonte[i];
    porta_escreve16(nam + NAM_REC_SEL, (uint16_t)(f << 8 | f));
    /* la fuente elegida sin silencio y a 0 dB (el micrófono sin el refuerzo de +20 dB) */
    porta_escreve16(nam + (i == 0 ? NAM_FONE_MIC : i == 1 ? NAM_LINEA : NAM_AUX), i == 0 ? 0x0008 : 0x0808);
    return true;
}

static void cap_ganho(unsigned pct)
{
    uint16_t g = (uint16_t)(pct * 15 / 100);       /* 0-15 pasos de 1,5 dB */
    porta_escreve16(nam + NAM_REC_GANHO, (uint16_t)(g << 8 | g));
}

static void cap_preparar(void)
{
    memset(&cap_hw, 0, sizeof cap_hw);
    void *mem;
    uint32_t fis = audio_dma_alocar(1 + CAP_QUADROS * 4 / 4096, &mem);
    if (!fis) return;
    cap_bdl = (uint32_t *)mem;
    cap_bdl_fis = fis;
    cap_anel_fis = fis + 4096;
    for (int i = 0; i < CAP_DESC; i++) {
        cap_bdl[2 * i] = cap_anel_fis + (uint32_t)i * CAP_PEDACO * 4;
        cap_bdl[2 * i + 1] = CAP_PEDACO * 4 / unidade;   /* sin el bit IOC */
    }
    cap_hw.taxa = 48000;
    cap_hw.quadros = CAP_QUADROS;
    cap_hw.anel = (const volatile int16_t *)((uint8_t *)mem + 4096);
    cap_hw.n_entradas = 3;
    txt_copia_n(cap_hw.nombre[0], "Micr\xf3" "fono", 24);
    txt_copia_n(cap_hw.nombre[1], "Entrada de l\xednea", 24);
    txt_copia_n(cap_hw.nombre[2], "Auxiliar", 24);
    cap_hw.ligar = cap_ligar;
    cap_hw.parar = cap_parar;
    cap_hw.posicao = cap_posicao;
    cap_hw.entrada = cap_entrada;
    cap_hw.ganho = cap_ganho;
    cap_reiniciar();
}

bool ac97_procurar(placa_som_t *p)
{
    pci_disp_t v[4];
    int n = pci_procurar_classe(0x04, 0x01, v, 4);   /* multimedia, audio */
    pci_disp_t *d = NULL;
    const char *nome = NULL;
    bool sis = false;
    for (int i = 0; i < n && !d; i++)
        for (uint32_t k = 0; k < sizeof modelos / sizeof modelos[0]; k++)
            if (v[i].fabricante == modelos[k].fab && v[i].modelo == modelos[k].mod) {
                d = &v[i];
                nome = modelos[k].nome;
                sis = modelos[k].sis;
                break;
            }
    if (!d) return false;
    uint32_t b0 = pci_le32(d, 0x10), b1 = pci_le32(d, 0x14);
    if (!(b0 & 1) || !(b1 & 1)) { klog("[AC97] BARs sem portas de E/S\n"); return false; }
    nam = (uint16_t)(b0 & ~3u);
    nabm = (uint16_t)(b1 & ~3u);
    if (sis) { reg_sr = PO_PICB; reg_picb = PO_SR; unidade = 1; }
    /* puertos de E/S + maestro del bus; interrupciones permitidas */
    pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x5) & ~(1u << 10));

    /* Saca el códec del reset ("cold reset" desactivado) y espera a que esté listo. */
    uint32_t g = porta_le32(nabm + GLOB_CNT);
    if (!(g & 2)) porta_escreve32(nabm + GLOB_CNT, (g | 2) & ~4u);
    uint32_t lim = relogio_tiques() + TIQUES_POR_SEGUNDO / 2;
    while (!(porta_le32(nabm + GLOB_STA) & (1u << 8)) && antes(lim)) porta_pausa();
    if (!(porta_le32(nabm + GLOB_STA) & (1u << 8))) { klog("[AC97] codec nao ficou pronto\n"); return false; }

    porta_escreve16(nam + NAM_RESET, 0);   /* volúmenes del códec por defecto */
    porta_escreve16(nam + NAM_ENERGIA, 0);
    lim = relogio_tiques() + TIQUES_POR_SEGUNDO / 2;   /* DAC, ADC, mixer y referencia listos */
    while ((porta_le16(nam + NAM_ENERGIA) & 0xF) != 0xF && antes(lim)) porta_pausa();
    porta_escreve16(nam + NAM_MESTRE, 0x0000);   /* 0 dB, sin silencio: el volumen es del mezclador */
    porta_escreve16(nam + NAM_FONES, 0x0000);
    porta_escreve16(nam + NAM_PCM, 0x0808);      /* 0 dB */
    uint32_t id = (uint32_t)porta_le16(nam + NAM_ID1) << 16 | porta_le16(nam + NAM_ID2);

    uint32_t mem = audio_dma_alocar(1 + QUADROS * 4 / 4096, (void **)&bdl);
    if (!mem) { klog("[AC97] sem memoria\n"); return false; }
    bdl_fis = mem;
    anel_fis = mem + 4096;
    for (int i = 0; i < N_DESC; i++) {
        bdl[2 * i] = anel_fis + (uint32_t)i * PEDACO * 4;
        bdl[2 * i + 1] = (PEDACO * 4 / unidade) | ((uint32_t)BD_IOC << 16);
    }

    p->driver = "ac97";
    txt_copia_n(p->placa, nome, sizeof p->placa);
    snformatar(p->codec, sizeof p->codec, "codec AC'97 %04x:%04x", id >> 16, id & 0xFFFF);
    p->pci = *d;
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    p->taxa = 48000;
    p->quadros = QUADROS;
    p->anel = (int16_t *)((uint8_t *)bdl + 4096);
    p->ligar = ligar;
    p->parar = parar;
    p->posicao = posicao;
    p->atender = atender;
    cap_preparar();
    p->cap = cap_hw;
    canal_reiniciar();
    klog("[AC97] %02x:%02x.%x (%04x:%04x) NAM %x NABM %x, irq %d, %s\n", d->barramento, d->dispositivo,
         d->funcao, d->fabricante, d->modelo, nam, nabm, p->irq, p->codec);
    return true;
}
