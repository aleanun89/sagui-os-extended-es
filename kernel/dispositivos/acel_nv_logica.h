/* acel_nv_logica.h - la parte pura del acelerador 2D de las GeForce 6/7 (D224):
 * los modelos (identificadores PCI exactos y el chip que tiene que decir
 * PMC_BOOT_0), el formato de las órdenes del búfer (cabeceras, puntos,
 * tamaños, saltos), la tabla hash de PRAMIN, el pitch que lee la CPU del
 * CRTC y dónde va el búfer de órdenes en la memoria de vídeo. Sin acceso al
 * hardware, para probarla en el anfitrión (tests/recursos/acel_nv_anfitriao.c,
 * tests/h225_acel2d_nvidia.py) con el mismo código que compila el kernel. */
#ifndef SAGUI_ACEL_NV_LOGICA_H
#define SAGUI_ACEL_NV_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* ---------------------------------------------------------- modelos */

/* Cada modelo: su identificador PCI exacto (fabricante 0x10DE) y los chips que
 * puede decir PMC_BOOT_0 (bits 28:20). chip_b = 0: solo chip_a; chip_a = 0:
 * cualquiera de la familia (las tarjetas AGP con puente, 0x00Fx). */
typedef struct {
    uint16_t    modelo;
    uint8_t     chip_a, chip_b;
    const char *nome;
} nv_modelo_t;

static const nv_modelo_t nv_modelos[] = {
    /* NV40 / NV45 */
    { 0x0040, 0x40, 0x45, "GeForce 6800 Ultra" }, { 0x0041, 0x40, 0x45, "GeForce 6800" },
    { 0x0042, 0x40, 0x45, "GeForce 6800 LE" },    { 0x0043, 0x40, 0x45, "GeForce 6800 XE" },
    { 0x0044, 0x40, 0x45, "GeForce 6800 XT" },    { 0x0045, 0x40, 0x45, "GeForce 6800 GT" },
    { 0x0046, 0x40, 0x45, "GeForce 6800 GT" },    { 0x0047, 0x40, 0x45, "GeForce 6800 GS" },
    { 0x0048, 0x40, 0x45, "GeForce 6800 XT" },    { 0x004E, 0x40, 0x45, "Quadro FX 4000" },
    /* NV41 / NV42 */
    { 0x00C0, 0x41, 0x42, "GeForce 6800 GS" },    { 0x00C1, 0x41, 0x42, "GeForce 6800" },
    { 0x00C2, 0x41, 0x42, "GeForce 6800 LE" },    { 0x00C3, 0x41, 0x42, "GeForce 6800 XT" },
    { 0x00C8, 0x41, 0x42, "GeForce Go 6800" },    { 0x00C9, 0x41, 0x42, "GeForce Go 6800 Ultra" },
    { 0x00CC, 0x41, 0x42, "Quadro FX Go1400" },   { 0x00CD, 0x41, 0x42, "Quadro FX 3450/4000 SDI" },
    { 0x00CE, 0x41, 0x42, "Quadro FX 1400" },
    /* AGP con puente HSI (el chip lo dice PMC_BOOT_0) */
    { 0x00F1, 0, 0, "GeForce 6600 GT (AGP)" },     { 0x00F2, 0, 0, "GeForce 6600 (AGP)" },
    { 0x00F3, 0, 0, "GeForce 6200 (AGP)" },        { 0x00F4, 0, 0, "GeForce 6600 LE (AGP)" },
    { 0x00F5, 0, 0, "GeForce 7800 GS (AGP)" },     { 0x00F6, 0, 0, "GeForce 6800 GS/XT (AGP)" },
    { 0x00F8, 0, 0, "Quadro FX 3400/4400" },       { 0x00F9, 0, 0, "GeForce 6800 GT/Ultra" },
    /* NV43 */
    { 0x0140, 0x43, 0, "GeForce 6600 GT" },       { 0x0141, 0x43, 0, "GeForce 6600" },
    { 0x0142, 0x43, 0, "GeForce 6600 LE" },       { 0x0143, 0x43, 0, "GeForce 6600 VE" },
    { 0x0144, 0x43, 0, "GeForce Go 6600" },       { 0x0145, 0x43, 0, "GeForce 6610 XL" },
    { 0x0146, 0x43, 0, "GeForce Go 6600 TE/6200 TE" }, { 0x0147, 0x43, 0, "GeForce 6700 XL" },
    { 0x0148, 0x43, 0, "GeForce Go 6600" },       { 0x0149, 0x43, 0, "GeForce Go 6600 GT" },
    { 0x014A, 0x43, 0, "Quadro NVS 440" },        { 0x014C, 0x43, 0, "Quadro FX 550" },
    { 0x014D, 0x43, 0, "Quadro FX 550" },         { 0x014E, 0x43, 0, "Quadro FX 540" },
    { 0x014F, 0x43, 0, "GeForce 6200" },
    /* NV44 */
    { 0x0160, 0x44, 0, "GeForce 6500" },          { 0x0161, 0x44, 0, "GeForce 6200 TurboCache" },
    { 0x0162, 0x44, 0, "GeForce 6200SE TurboCache" }, { 0x0163, 0x44, 0, "GeForce 6200 LE" },
    { 0x0164, 0x44, 0, "GeForce Go 6200" },       { 0x0165, 0x44, 0, "Quadro NVS 285" },
    { 0x0166, 0x44, 0, "GeForce Go 6400" },       { 0x0167, 0x44, 0, "GeForce Go 6200" },
    { 0x0168, 0x44, 0, "GeForce Go 6400" },       { 0x0169, 0x44, 0, "GeForce 6250" },
    { 0x016A, 0x44, 0, "GeForce 7100 GS" },
    /* NV48 (chip 0x45) */
    { 0x0211, 0x45, 0x40, "GeForce 6800" },       { 0x0212, 0x45, 0x40, "GeForce 6800 LE" },
    { 0x0215, 0x45, 0x40, "GeForce 6800 GT" },    { 0x0218, 0x45, 0x40, "GeForce 6800 XT" },
    /* NV44A */
    { 0x0221, 0x4A, 0, "GeForce 6200" },          { 0x0222, 0x4A, 0, "GeForce 6200 A-LE" },
    /* G70 */
    { 0x0090, 0x47, 0, "GeForce 7800 GTX" },      { 0x0091, 0x47, 0, "GeForce 7800 GTX" },
    { 0x0092, 0x47, 0, "GeForce 7800 GT" },       { 0x0093, 0x47, 0, "GeForce 7800 GS" },
    { 0x0095, 0x47, 0, "GeForce 7800 SLI" },      { 0x0098, 0x47, 0, "GeForce Go 7800" },
    { 0x0099, 0x47, 0, "GeForce Go 7800 GTX" },   { 0x009D, 0x47, 0, "Quadro FX 4500" },
    /* G72 */
    { 0x01D0, 0x46, 0, "GeForce 7350 LE" },       { 0x01D1, 0x46, 0, "GeForce 7300 LE" },
    { 0x01D3, 0x46, 0, "GeForce 7300 SE" },       { 0x01D6, 0x46, 0, "GeForce Go 7200" },
    { 0x01D7, 0x46, 0, "GeForce Go 7300" },       { 0x01D8, 0x46, 0, "GeForce Go 7400" },
    { 0x01D9, 0x46, 0, "GeForce Go 7400 GS" },    { 0x01DA, 0x46, 0, "Quadro NVS 110M" },
    { 0x01DB, 0x46, 0, "Quadro NVS 120M" },       { 0x01DC, 0x46, 0, "Quadro FX 350M" },
    { 0x01DD, 0x46, 0, "GeForce 7500 LE" },       { 0x01DE, 0x46, 0, "Quadro FX 350" },
    { 0x01DF, 0x46, 0, "GeForce 7300 GS" },
    /* G71 */
    { 0x0290, 0x49, 0, "GeForce 7900 GTX" },      { 0x0291, 0x49, 0, "GeForce 7900 GT" },
    { 0x0292, 0x49, 0, "GeForce 7900 GS" },       { 0x0293, 0x49, 0, "GeForce 7950 GX2" },
    { 0x0294, 0x49, 0, "GeForce 7950 GX2" },      { 0x0295, 0x49, 0, "GeForce 7950 GT" },
    { 0x0297, 0x49, 0, "GeForce Go 7950 GTX" },   { 0x0298, 0x49, 0, "GeForce Go 7900 GS" },
    { 0x0299, 0x49, 0, "GeForce Go 7900 GTX" },   { 0x029A, 0x49, 0, "Quadro FX 2500M" },
    { 0x029B, 0x49, 0, "Quadro FX 1500M" },       { 0x029C, 0x49, 0, "Quadro FX 5500" },
    { 0x029D, 0x49, 0, "Quadro FX 3500" },        { 0x029E, 0x49, 0, "Quadro FX 1500" },
    { 0x029F, 0x49, 0, "Quadro FX 4500 X2" },
    /* G73 */
    { 0x0390, 0x4B, 0, "GeForce 7650 GS" },       { 0x0391, 0x4B, 0, "GeForce 7600 GT" },
    { 0x0392, 0x4B, 0, "GeForce 7600 GS" },       { 0x0393, 0x4B, 0, "GeForce 7300 GT" },
    { 0x0394, 0x4B, 0, "GeForce 7600 LE" },       { 0x0395, 0x4B, 0, "GeForce 7300 GT" },
    { 0x0397, 0x4B, 0, "GeForce Go 7700" },       { 0x0398, 0x4B, 0, "GeForce Go 7600" },
    { 0x0399, 0x4B, 0, "GeForce Go 7600 GT" },    { 0x039A, 0x4B, 0, "Quadro NVS 300M" },
    { 0x039B, 0x4B, 0, "GeForce Go 7900 SE" },    { 0x039C, 0x4B, 0, "Quadro FX 550M" },
    { 0x039E, 0x4B, 0, "Quadro FX 560" },
    /* Integradas en el chipset nForce: la memoria de vídeo es RAM del sistema (UMA). */
    /* C51 (chip 0x4E): nForce 410/430 */
    { 0x0240, 0x4E, 0, "GeForce 6150 (C51)" },    { 0x0241, 0x4E, 0, "GeForce 6150 LE (C51)" },
    { 0x0242, 0x4E, 0, "GeForce 6100 (C51)" },    { 0x0244, 0x4E, 0, "GeForce Go 6150 (C51)" },
    { 0x0245, 0x4E, 0, "Quadro NVS 210S / GeForce 6150LE (C51)" }, { 0x0247, 0x4E, 0, "GeForce Go 6100 (C51)" },
    /* C61 (chip 0x4C): nForce 400/405/420/430 */
    { 0x03D0, 0x4C, 0, "GeForce 6150SE (C61)" },  { 0x03D1, 0x4C, 0, "GeForce 6100 nForce 405 (C61)" },
    { 0x03D2, 0x4C, 0, "GeForce 6100 nForce 400 (C61)" }, { 0x03D5, 0x4C, 0, "GeForce 6100 nForce 420 (C61)" },
    { 0x03D6, 0x4C, 0, "GeForce 7025 nForce 630a (C61)" },
    /* C67 y C68 (chips 0x67 y 0x68): nForce 610M/630M y 630a */
    { 0x0531, 0x67, 0x68, "GeForce 7150M nForce 630M (C67)" }, { 0x0533, 0x67, 0x68, "GeForce 7000M nForce 610M (C67)" },
    { 0x053A, 0x68, 0x67, "GeForce 7050 PV nForce 630a (C68)" }, { 0x053B, 0x68, 0x67, "GeForce 7050 PV nForce 630a (C68)" },
    { 0x053E, 0x68, 0x67, "GeForce 7025 nForce 630a (C68)" },
    /* C73 (chip 0x63): nForce 610i/620i/630i */
    { 0x07E0, 0x63, 0, "GeForce 7150 nForce 630i (C73)" }, { 0x07E1, 0x63, 0, "GeForce 7100 nForce 630i (C73)" },
    { 0x07E2, 0x63, 0, "GeForce 7050 nForce 630i (C73)" }, { 0x07E3, 0x63, 0, "GeForce 7050 nForce 610i (C73)" },
    { 0x07E5, 0x63, 0, "GeForce 7100 nForce 620i (C73)" },
};
#define NV_N_MODELOS (int)(sizeof nv_modelos / sizeof nv_modelos[0])

static inline const nv_modelo_t *nv_modelo(uint16_t fab, uint16_t modelo)
{
    if (fab != 0x10DE) return 0;
    for (int i = 0; i < NV_N_MODELOS; i++)
        if (nv_modelos[i].modelo == modelo) return &nv_modelos[i];
    return 0;
}

/* Los chips NV4x/G7x de este driver (el número de PMC_BOOT_0, como nouveau). */
static inline bool nv_chip_suportado(int chip)
{
    switch (chip) {
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4E: case 0x63: case 0x67: case 0x68:
        return true;
    default:
        return false;
    }
}

/* El chip de PMC_BOOT_0 (bits 28:20; -1 si el registro no tiene el formato de
 * las NV10 en adelante: bits 28:24 a cero). */
static inline int nv_chip_de_boot0(uint32_t boot0)
{
    if (!(boot0 & 0x1F000000u)) return -1;
    return (int)((boot0 >> 20) & 0x1FF);
}

/* ¿El chip que dice la tarjeta es el que se espera de su identificador PCI? */
static inline bool nv_modelo_aceita(const nv_modelo_t *m, int chip)
{
    if (!m || !nv_chip_suportado(chip)) return false;
    if (!m->chip_a) return true;
    return chip == m->chip_a || (m->chip_b && chip == m->chip_b);
}

/* Las integradas en el chipset (memoria de vídeo = RAM del sistema). */
static inline bool nv_chip_integrado(int chip)
{
    return chip == 0x4C || chip == 0x4E || chip == 0x63 || chip == 0x67 || chip == 0x68;
}

/* Los chips que usan PMC 0x1700-0x170C (el «nv44_mc» de nouveau; X.Org los
 * escribe en las de PCI 0x016x, 0x01Dx, 0x024x, 0x03Dx y 0x053x). */
static inline bool nv_chip_pmc1700(int chip)
{
    return chip == 0x44 || chip == 0x46 || chip == 0x4A || nv_chip_integrado(chip);
}

/* Dónde copia PGRAPH la configuración de la memoria (PFB 0x100200/0x100204). */
#define NV_MEMCFG_9A4  0x1   /* NV40: 0x4009A4/A8 y 0x4069A4/A8 */
#define NV_MEMCFG_9F0  0x2   /* 0x4009F0/F4 */
#define NV_MEMCFG_DF0  0x4   /* 0x400DF0/F4 */
static inline int nv_memcfg(int chip)
{
    switch (chip) {
    case 0x40: return NV_MEMCFG_9A4;
    case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x4A: case 0x4E: return NV_MEMCFG_9F0;
    case 0x46: case 0x47: case 0x49: case 0x4B: return NV_MEMCFG_DF0;
    /* X.Org (por la máscara del PCI) dice 0x9F0 y nouveau (por el chip) 0xDF0:
     * se escriben las dos (D224). */
    case 0x4C: case 0x63: case 0x67: case 0x68: return NV_MEMCFG_9F0 | NV_MEMCFG_DF0;
    default: return 0;
    }
}

/* Las regiones de tiling de PFB que hay que mirar (que estén apagadas): NV40
 * en 0x100240 + 16·i (8); las G7x y la C61 en 0x100600 (15); el resto en
 * 0x100600 (12). */
static inline int nv_regioes_tiling(int chip, uint32_t *base)
{
    if (chip == 0x40) { *base = 0x100240; return 8; }
    *base = 0x100600;
    switch (chip) {
    case 0x46: case 0x47: case 0x49: case 0x4B: case 0x4C: return 15;
    default: return 12;
    }
}

/* ---------------------------------------------------------- órdenes */

#define NV_SUBCANAL_MAX   7
#define NV_METODO_MAX     0x1FFC
#define NV_CONTA_MAX      2047

/* La cabecera de una orden (NVDmaStart): n palabras a partir de «metodo» del
 * subcanal, con el método ya desplazado (subcanal << 13 | método, como en
 * nv_dma.h). */
static inline uint32_t nv_cabecalho(uint32_t metodo_subcanal, uint32_t n)
{
    return (n << 18) | metodo_subcanal;
}

static inline bool nv_cabecalho_valido(uint32_t metodo_subcanal, uint32_t n)
{
    uint32_t metodo = metodo_subcanal & 0x1FFF, sub = metodo_subcanal >> 13;
    return !(metodo & 3) && sub <= NV_SUBCANAL_MAX && n >= 1 && n <= NV_CONTA_MAX;
}

/* El salto (NV_DMA_SALTO | byte) al principio del búfer. */
static inline uint32_t nv_salto(uint32_t byte) { return 0x20000000u | (byte & 0x1FFFFFFCu); }

/* Puntos y tamaños de IMAGE_BLIT: y en los 16 bits altos, x en los bajos. */
static inline uint32_t nv_ponto(int x, int y)   { return ((uint32_t)y << 16) | ((uint32_t)x & 0xFFFFu); }
static inline uint32_t nv_tamanho(int l, int a) { return ((uint32_t)a << 16) | ((uint32_t)l & 0xFFFFu); }
/* Y los de RECT (GDI_RECTANGLE_TEXT): al revés, x arriba y y abajo. */
static inline uint32_t nv_rect_ponto(int x, int y)   { return ((uint32_t)x << 16) | ((uint32_t)y & 0xFFFFu); }
static inline uint32_t nv_rect_tamanho(int l, int a) { return ((uint32_t)l << 16) | ((uint32_t)a & 0xFFFFu); }
/* Los dos pitch (origen y destino) de SURFACE_PITCH. */
static inline uint32_t nv_pitch_surface(uint32_t pitch) { return (pitch << 16) | (pitch & 0xFFFFu); }

/* Coordenadas que aceptan los objetos 2D: 16 bits sin signo (sin pasar de 0x7FFF). */
static inline bool nv_coord_valida(int x, int y, int l, int a)
{
    return x >= 0 && y >= 0 && l > 0 && a > 0 && x + l <= 0x7FFF && y + a <= 0x7FFF;
}

/* ---------------------------------------------------------- el anillo */

/* El búfer de órdenes: NV_ANEL palabras; las SKIPS primeras son ceros (sin
 * orden) para el salto del final (ver nv_2d.h). Una orden de n palabras cabe
 * si después queda sitio para el salto. */
#define NV_ANEL        8192          /* palabras (32 KB) */
#define NV_ANEL_SKIPS  8
static inline bool nv_anel_cabe(uint32_t atual, uint32_t n)
{
    return atual + n + 1 <= NV_ANEL - 1;
}

/* ---------------------------------------------------------- PRAMIN */

/* La tabla hash del canal 0 (nouveau nv04_ramht_hash): se pliega el
 * identificador en trozos de «bits» bits; el resultado, en bytes (8 por entrada). */
static inline uint32_t nv_ramht_hash(uint32_t handle, int bits, int canal)
{
    uint32_t h = 0;
    while (handle) {
        h ^= handle & ((1u << bits) - 1);
        handle >>= bits;
    }
    h ^= (uint32_t)canal << (bits - 4);
    return h << 3;
}

/* ---------------------------------------------------------- el CRTC */

/* El pitch en bytes que tiene el CRTC: CR13 (bits 7:0 del «offset», en grupos de
 * 8 bytes), CR19 bits 7:5 (bits 10:8) y CR42 bit 6 (bit 11). */
static inline uint32_t nv_pitch_crtc(uint8_t cr13, uint8_t cr19, uint8_t cr42)
{
    uint32_t off = (uint32_t)cr13 | ((uint32_t)(cr19 >> 5) & 7u) << 8 | ((uint32_t)(cr42 >> 6) & 1u) << 11;
    return off * 8;
}

/* La memoria de vídeo que dice PFB 0x10020C (bits 31:20, en bytes; como
 * nv10GetConfig, con el mismo tope de 256 MB). */
static inline uint32_t nv_vram_bytes(uint32_t pfb20c)
{
    uint32_t kb = (pfb20c & 0xFFF00000u) >> 10;
    if (kb > 256 * 1024) kb = 256 * 1024;
    return kb * 1024;
}

/* ---------------------------------------------------------- el búfer en la memoria */

#define NV_PB_BYTES       (NV_ANEL * 4)
#define NV_RAMIN_RESERVA  (1024u * 1024u)    /* el último MB de la VRAM: PRAMIN */

enum { NV_PB_OK = 0, NV_PB_SEM_VBE, NV_PB_RAMIN, NV_PB_ALINHAMENTO };

/* Dónde va el búfer de órdenes (desplazamiento en la VRAM): justo debajo de la
 * pantalla y de las filas de la zona de prueba, alineado a 4 KB, dentro de la
 * memoria que el VBE deja ver a la CPU (desde el principio de la pantalla) y
 * lejos del último MB de la VRAM (PRAMIN, los objetos y la tabla hash). */
static inline int nv_colocar_pb(uint32_t fb_off, uint32_t pitch, uint32_t alt, uint32_t linhas_zona,
                                uint32_t vram, uint32_t vbe_mem, uint32_t *pb_off)
{
    if (fb_off & 0xFF) return NV_PB_ALINHAMENTO;
    uint64_t fim_tela = (uint64_t)pitch * (alt + linhas_zona);
    uint64_t rel = (fim_tela + 4095) & ~(uint64_t)4095;        /* desde el principio de la pantalla */
    if (rel + NV_PB_BYTES > vbe_mem) return NV_PB_SEM_VBE;
    uint64_t abs = (uint64_t)fb_off + rel;
    if (vram < NV_RAMIN_RESERVA || abs + NV_PB_BYTES > (uint64_t)vram - NV_RAMIN_RESERVA) return NV_PB_RAMIN;
    *pb_off = (uint32_t)abs;
    return NV_PB_OK;
}

#endif
