// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu_energia_logica.h - lógica pura de la energía de la GPU (D399): lo que se puede probar en el PC.
 *
 *   - las tablas de datos de la BIOS ATOM de las Radeon: la cabecera de la ROM (firmas y PCIR), la
 *     tabla maestra, FirmwareInfo (relojes por defecto, reloj de referencia), PowerPlayInfo 1 a 3 (los
 *     estados de energía de las R5xx, con su tensión por GPIO o por identificador) y GPIO_Pin_LUT;
 *   - el reloj de un PLL a partir de sus divisores;
 *   - qué estado es «bajo» y cuál «alto», y el gobernador «auto» (como el dynpm de Linux: sube con el
 *     motor ocupado de forma sostenida y baja con él en reposo, con histéresis).
 *
 * Todo lee de un búfer con su tamaño: ningún byte fuera de la imagen (una BIOS rota da «no hay tabla»,
 * nunca una lectura fuera). Unidades de la BIOS: relojes en 10 kHz.
 *
 * Referencia (MIT): Linux 6.6, drivers/gpu/drm/radeon/radeon_atombios.c (radeon_atom_get_clock_info,
 * radeon_atombios_parse_power_table_1_3, radeon_atombios_parse_misc_flags_1_3,
 * radeon_atombios_lookup_gpio), radeon_pm.c (radeon_set_power_state, radeon_dynpm_idle_work_handler),
 * r100.c (r100_pm_get_dynpm_state), r420.c (r420_pm_init_profile), rs600.c (rs600_pm_misc) y
 * atombios.h (las estructuras, empaquetadas a 1 byte). Código propio: solo desplazamientos, bits y el
 * orden de las decisiones. Lo prueba tests/gpu_energia_unidad.c (h401). */
#ifndef SAGUI_GPU_ENERGIA_LOGICA_H
#define SAGUI_GPU_ENERGIA_LOGICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/* ------------------------------------------------------------ índices de las tablas maestras (atombios.h) */
#define ATOM_DADOS_FIRMWARE      4    /* FirmwareInfo */
#define ATOM_DADOS_VRAM_USO      11   /* VRAM_UsageByFirmware */
#define ATOM_DADOS_GPIO_PINOS    12   /* GPIO_Pin_LUT */
#define ATOM_DADOS_POWERPLAY     15   /* PowerPlayInfo */
#define ATOM_DADOS_IIO           23   /* IndirectIOAccess */
#define ATOM_DADOS_N             34
#define ATOM_CMD_SET_SCLK        10   /* SetEngineClock: PS[0] = reloj en 10 kHz */
#define ATOM_CMD_SET_MCLK        11   /* SetMemoryClock */
#define ATOM_CMD_GET_MCLK        47   /* GetMemoryClock: devuelve en PS[0] */
#define ATOM_CMD_GET_SCLK        48   /* GetEngineClock */
#define ATOM_CMD_SET_VOLTAGE     67   /* SetVoltage (las R5xx con tensión por identificador) */
#define ATOM_CMD_N               81

/* ulMiscInfo / ulMiscInfo2 de cada estado (ATOM_PM_MISCINFO_*) */
#define ATOM_PM_TENSAO_GPIO          0x00000004u   /* VOLTAGE_DROP_SUPPORT */
#define ATOM_PM_TENSAO_ALTA          0x00000008u   /* VOLTAGE_DROP_ACTIVE_HIGH */
#define ATOM_PM_TENSAO_PROGRAMAR     0x00000080u   /* PROGRAM_VOLTAGE: el índice es un identificador de VDDC */
#define ATOM_PM_SCLK_REDUZIDO        0x00000100u
#define ATOM_PM_TENSAO_DINAMICA      0x00000200u
#define ATOM_PM_EQUILIBRIO           0x00000800u   /* LOAD_BALANCE_EN */
#define ATOM_PM_DC_PADRAO            0x00001000u
#define ATOM_PM_DC_BAIXO             0x00002000u
#define ATOM_PM_PADRAO_DRIVER        0x00008000u   /* DRIVER_DEFAULT_MODE */
#define ATOM_PM_POUPANCA             0x00040000u   /* POWER_SAVING_MODE */
#define ATOM_PM_3D                   0x08000000u   /* 3D_ACCELERATION_EN */
#define ATOM_PM2_AC_LIGEIRO          0x00000001u   /* SYSTEM_AC_LITE_MODE */
#define ATOM_PM2_VDDCI_DINAMICA      0x00000020u

#define GPU_EN_MAX_ESTADOS 8          /* ATOM_MAX_NUMBEROF_POWER_BLOCK */

/* Tipos de estado (como POWER_STATE_TYPE_* de Linux) */
enum { GPU_EN_T_NINGUNO = 0, GPU_EN_T_AHORRO, GPU_EN_T_BATERIA, GPU_EN_T_EQUILIBRADO, GPU_EN_T_RENDIMIENTO, GPU_EN_T_DEFECTO };
/* Tensión de un estado */
enum { GPU_EN_V_NINGUNA = 0, GPU_EN_V_GPIO, GPU_EN_V_VDDC };

typedef struct {
    uint32_t sclk, mclk;          /* 10 kHz */
    uint32_t misc, misc2;
    uint8_t  pcie_lanes;
    uint8_t  tensao;              /* GPU_EN_V_* */
    uint8_t  gpio_valido, ativo_alto;
    uint32_t gpio_reg;            /* registro MMIO en bytes (usGpioPin_AIndex * 4) */
    uint32_t gpio_mascara;
    uint8_t  vddc_id, gpio_id;
    uint8_t  tipo;                /* GPU_EN_T_* */
    uint8_t  sem_ecra;            /* RADEON_PM_MODE_NO_DISPLAY: no vale con la pantalla encendida */
    uint8_t  so_um_ecra;          /* RADEON_PM_STATE_SINGLE_DISPLAY_ONLY */
    uint8_t  indice_bios;         /* posición en la tabla de la BIOS (las vacías se saltan) */
    uint8_t  temp_min, temp_max;
} gpu_en_estado_t;

typedef struct {
    uint8_t  frev, crev;
    uint8_t  termico, termico_i2c, termico_dir;   /* ucOverdriveThermalController, I2cLine, ControllerAddress */
    uint8_t  tam_entrada;         /* ucSizeOfPowerModeEntry (Linux no lo usa: avanza por el tamaño de la estructura) */
    uint8_t  n_bios;              /* ucNumOfPowerModeEntries */
    int      n;                   /* estados válidos */
    int      padrao;              /* índice del estado por defecto */
    gpu_en_estado_t e[GPU_EN_MAX_ESTADOS];
} gpu_en_tabela_t;

typedef struct {
    uint8_t  frev, crev;
    uint32_t sclk_def, mclk_def;  /* 10 kHz */
    uint32_t sclk_max, mclk_max;  /* ulASICMaxEngineClock / MemoryClock */
    uint32_t ref;                 /* usReferenceClock, 10 kHz */
    uint8_t  temp_max;            /* ucASICMaxTemperature */
    uint16_t vddc_arranque_mv;    /* 1.4: usBootUpVDDCVoltage (0 = no lo dice) */
} gpu_en_firmware_t;

/* ------------------------------------------------------------ lectura acotada */
static inline uint32_t gen_u8(const uint8_t *b, uint32_t tam, uint32_t o) { return o < tam ? b[o] : 0; }
static inline uint32_t gen_u16(const uint8_t *b, uint32_t tam, uint32_t o) { return gen_u8(b, tam, o) | gen_u8(b, tam, o + 1) << 8; }
static inline uint32_t gen_u32(const uint8_t *b, uint32_t tam, uint32_t o) { return gen_u16(b, tam, o) | gen_u16(b, tam, o + 2) << 16; }

/* La imagen de una ROM de vídeo ATOM: 55 AA, « 761295520» en 0x30, la cabecera ATOM (puntero en 0x48,
 * firma «ATOM» en +4) y la estructura PCIR (puntero en 0x18) con el fabricante y el modelo. Devuelve
 * 0 si vale; si no, un texto corto con el porqué. *rom_cab, *cmd y *dados: dónde están las tablas. */
static inline const char *atom_rom_validar(const uint8_t *b, uint32_t tam, uint16_t *ven, uint16_t *dev,
                                           uint32_t *rom_cab, uint32_t *cmd, uint32_t *dados)
{
    if (tam < 0x200) return "imagen demasiado corta";
    if (gen_u16(b, tam, 0) != 0xAA55) return "sin la firma 55 AA";
    static const char ati[] = " 761295520";
    for (uint32_t i = 0; i < sizeof ati - 1; i++)
        if (gen_u8(b, tam, 0x30 + i) != (uint8_t)ati[i]) return "sin la firma de ATI en 0x30";
    uint32_t pcir = gen_u16(b, tam, 0x18);
    if (pcir + 8 > tam || gen_u32(b, tam, pcir) != 0x52494350u) return "sin la estructura PCIR";
    if (ven) *ven = (uint16_t)gen_u16(b, tam, pcir + 4);
    if (dev) *dev = (uint16_t)gen_u16(b, tam, pcir + 6);
    uint32_t rc = gen_u16(b, tam, 0x48);
    if (rc + 0x24 > tam || gen_u32(b, tam, rc + 4) != 0x4D4F5441u) return "sin la cabecera ATOM";
    uint32_t c = gen_u16(b, tam, rc + 0x1E), d = gen_u16(b, tam, rc + 0x20);
    if (!c || c + 4 > tam || c + gen_u16(b, tam, c) > tam) return "tabla maestra de ordenes fuera de la imagen";
    if (!d || d + 4 > tam || d + gen_u16(b, tam, d) > tam) return "tabla maestra de datos fuera de la imagen";
    if (rom_cab) *rom_cab = rc;
    if (cmd) *cmd = c;
    if (dados) *dados = d;
    return 0;
}

/* ¿Tiene la tabla maestra en "mestra" la entrada "idx"? Las BIOS antiguas tienen tablas maestras más
 * cortas que la lista de atombios.h: lo de detrás sería otra tabla (Linux no lo mira). */
static inline bool atom_mestra_tem(const uint8_t *b, uint32_t tam, uint32_t mestra, int idx)
{
    uint32_t t = gen_u16(b, tam, mestra);
    return idx >= 0 && t >= 4 && 4 + 2 * (uint32_t)idx + 2 <= t && mestra + t <= tam;
}

/* Una tabla de datos: su desplazamiento, tamaño (usStructureSize) y revisiones. false si no está o no cabe. */
static inline bool atom_tabela_dados(const uint8_t *b, uint32_t tam, uint32_t dados, int idx, uint32_t *off,
                                     uint32_t *tamanho, uint8_t *frev, uint8_t *crev)
{
    if (!atom_mestra_tem(b, tam, dados, idx)) return false;
    uint32_t o = gen_u16(b, tam, dados + 4 + 2 * (uint32_t)idx);
    if (!o || o + 4 > tam) return false;
    uint32_t t = gen_u16(b, tam, o);
    if (t < 4 || o + t > tam) return false;
    if (off) *off = o;
    if (tamanho) *tamanho = t;
    if (frev) *frev = (uint8_t)gen_u8(b, tam, o + 2);
    if (crev) *crev = (uint8_t)gen_u8(b, tam, o + 3);
    return true;
}

static inline bool atom_tabela_cmd(const uint8_t *b, uint32_t tam, uint32_t cmd, int idx)
{
    if (!atom_mestra_tem(b, tam, cmd, idx)) return false;
    uint32_t o = gen_u16(b, tam, cmd + 4 + 2 * (uint32_t)idx);
    return o && o + 6 <= tam && gen_u16(b, tam, o) >= 6 && o + gen_u16(b, tam, o) <= tam;
}

/* FirmwareInfo 1.1 a 1.4 (las R5xx a R7xx): los campos que se usan están en el mismo sitio en las
 * cuatro; usReferenceClock en 0x52. 2.x (Evergreen y después): relojes por defecto en el mismo sitio,
 * el resto no se lee. */
static inline bool atom_firmware(const uint8_t *b, uint32_t tam, uint32_t dados, gpu_en_firmware_t *f)
{
    uint32_t o, t;
    uint8_t fr, cr;
    for (uint32_t i = 0; i < sizeof *f; i++) ((uint8_t *)f)[i] = 0;
    if (!atom_tabela_dados(b, tam, dados, ATOM_DADOS_FIRMWARE, &o, &t, &fr, &cr) || t < 0x14) return false;
    f->frev = fr; f->crev = cr;
    f->sclk_def = gen_u32(b, tam, o + 0x08);
    f->mclk_def = gen_u32(b, tam, o + 0x0C);
    if (fr == 1 && t >= 0x54) {
        f->sclk_max = gen_u32(b, tam, o + 0x24);
        f->mclk_max = gen_u32(b, tam, o + 0x28);
        f->temp_max = (uint8_t)gen_u8(b, tam, o + 0x2C);
        f->ref = gen_u16(b, tam, o + 0x52);
        if (cr >= 4) f->vddc_arranque_mv = (uint16_t)gen_u16(b, tam, o + 0x2E);
    }
    return f->sclk_def != 0;
}

/* GPIO_Pin_LUT: el registro y el bit de un pin por su identificador (radeon_atombios_lookup_gpio). */
static inline bool atom_gpio_pino(const uint8_t *b, uint32_t tam, uint32_t dados, uint8_t id, uint32_t *reg, uint32_t *bit)
{
    uint32_t o, t;
    if (!atom_tabela_dados(b, tam, dados, ATOM_DADOS_GPIO_PINOS, &o, &t, 0, 0)) return false;
    for (uint32_t p = o + 4; p + 4 <= o + t; p += 4)
        if (gen_u8(b, tam, p + 3) == id) {
            *reg = gen_u16(b, tam, p) * 4;
            *bit = gen_u8(b, tam, p + 2);
            return *bit < 32;
        }
    return false;
}

/* Tipo y marcas de un estado (radeon_atombios_parse_misc_flags_1_3; el orden importa). */
static inline void gpu_en_marcas(gpu_en_tabela_t *t, int i, uint32_t misc, uint32_t misc2)
{
    gpu_en_estado_t *e = &t->e[i];
    e->misc = misc; e->misc2 = misc2;
    if (misc & ATOM_PM_POUPANCA) e->tipo = GPU_EN_T_AHORRO;
    if (misc & ATOM_PM_DC_PADRAO) e->tipo = GPU_EN_T_BATERIA;
    if (misc & ATOM_PM_DC_BAIXO) e->tipo = GPU_EN_T_BATERIA;
    if (misc & ATOM_PM_EQUILIBRIO) e->tipo = GPU_EN_T_EQUILIBRADO;
    if (misc & ATOM_PM_3D) { e->tipo = GPU_EN_T_RENDIMIENTO; e->so_um_ecra = 0; }
    if (misc2 & ATOM_PM2_AC_LIGEIRO) e->tipo = GPU_EN_T_EQUILIBRADO;
    if (misc & ATOM_PM_PADRAO_DRIVER) { e->tipo = GPU_EN_T_DEFECTO; t->padrao = i; }
    else if (i == 0) e->sem_ecra = 1;
}

/* PowerPlayInfo 1, 2 y 3 (frev; las R5xx y algunas R6xx): radeon_atombios_parse_power_table_1_3.
 * Las entradas avanzan por el tamaño de la estructura empaquetada (21, 21 y 22 bytes), como Linux.
 * Devuelve los estados válidos (los que tienen los dos relojes). frev 4 o más (PPLib, R6xx en
 * adelante) todavía no: 0 con t->frev puesto. */
static inline int atom_powerplay(const uint8_t *b, uint32_t tam, uint32_t dados, gpu_en_tabela_t *t)
{
    uint32_t o, tt;
    uint8_t fr, cr;
    for (uint32_t i = 0; i < sizeof *t; i++) ((uint8_t *)t)[i] = 0;
    t->padrao = -1;
    if (!atom_tabela_dados(b, tam, dados, ATOM_DADOS_POWERPLAY, &o, &tt, &fr, &cr)) return 0;
    t->frev = fr; t->crev = cr;
    if (fr < 1 || fr > 3) return 0;
    t->termico = (uint8_t)gen_u8(b, tam, o + 4);
    t->termico_i2c = (uint8_t)gen_u8(b, tam, o + 5);
    t->termico_dir = (uint8_t)gen_u8(b, tam, o + 7);
    t->tam_entrada = (uint8_t)gen_u8(b, tam, o + 8);
    t->n_bios = (uint8_t)gen_u8(b, tam, o + 9);
    uint32_t n = t->n_bios > GPU_EN_MAX_ESTADOS ? GPU_EN_MAX_ESTADOS : t->n_bios;
    uint32_t passo = fr == 3 ? 22 : 21;
    int k = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = o + 10 + i * passo;
        if (p + passo > o + tt || p + passo > tam) break;          /* la tabla dice más de lo que cabe */
        gpu_en_estado_t *e = &t->e[k];
        for (uint32_t z = 0; z < sizeof *e; z++) ((uint8_t *)e)[z] = 0;
        uint32_t misc = gen_u32(b, tam, p), misc2 = 0;
        if (fr == 1) {
            e->sclk = gen_u16(b, tam, p + 12);
            e->mclk = gen_u16(b, tam, p + 14);
        } else {
            misc2 = gen_u32(b, tam, p + 4);
            e->sclk = gen_u32(b, tam, p + 8);
            e->mclk = gen_u32(b, tam, p + 12);
        }
        if (!e->sclk || !e->mclk) continue;                          /* skip invalid modes */
        uint8_t vid = (uint8_t)gen_u8(b, tam, p + 16);
        e->temp_min = (uint8_t)gen_u8(b, tam, p + 18);
        e->temp_max = (uint8_t)gen_u8(b, tam, p + 19);
        e->pcie_lanes = (uint8_t)gen_u8(b, tam, p + 20);
        e->indice_bios = (uint8_t)i;
        if (misc & (ATOM_PM_TENSAO_GPIO | ATOM_PM_TENSAO_ALTA)) {
            e->tensao = GPU_EN_V_GPIO;
            e->gpio_id = vid;
            uint32_t reg, bit;
            if (atom_gpio_pino(b, tam, dados, vid, &reg, &bit)) {
                e->gpio_valido = 1; e->gpio_reg = reg; e->gpio_mascara = 1u << bit;
            }
            e->ativo_alto = (misc & ATOM_PM_TENSAO_ALTA) ? 1 : 0;
        } else if (misc & ATOM_PM_TENSAO_PROGRAMAR) {
            e->tensao = GPU_EN_V_VDDC;
            e->vddc_id = vid;
        }
        e->so_um_ecra = 1;
        gpu_en_marcas(t, k, misc, misc2);
        k++;
    }
    t->n = k;
    /* last mode is usually default */
    if (k && t->padrao < 0) {
        t->padrao = k - 1;
        t->e[k - 1].tipo = GPU_EN_T_DEFECTO;
        t->e[k - 1].so_um_ecra = 0;
        t->e[k - 1].misc = t->e[k - 1].misc2 = 0;
    }
    return k;
}

/* El bit de tensión de un estado con GPIO (rs600_pm_misc): con VOLTAGE_DROP_SUPPORT el pin va al
 * nivel «activo» (alto si ACTIVE_HIGH); si no, al contrario. Devuelve el registro con el bit puesto. */
static inline uint32_t gpu_en_gpio_valor(const gpu_en_estado_t *e, uint32_t reg_atual)
{
    bool activo = (e->misc & ATOM_PM_TENSAO_GPIO) != 0;
    bool alto = activo ? e->ativo_alto : !e->ativo_alto;
    return alto ? (reg_atual | e->gpio_mascara) : (reg_atual & ~e->gpio_mascara);
}

/* El reloj (kHz) de un PLL: referencia * realimentación / (divisor de referencia * divisor posterior).
 * fb_x2: las Radeon clásicas cuentan la realimentación por dos (radeon_legacy_get_engine_clock). */
static inline uint32_t gpu_en_pll_khz(uint32_t ref_khz, uint32_t ref_div, uint32_t fb_div, uint32_t post_div, bool fb_x2)
{
    if (!ref_div || !post_div) return 0;
    uint64_t v = (uint64_t)ref_khz * fb_div * (fb_x2 ? 2u : 1u);
    return (uint32_t)(v / ((uint64_t)ref_div * post_div));
}

/* El SPLL/MPLL de las Radeon R100-R4xx (formato clásico, PLL 0x0A M_SPLL_REF_FB_DIV y el divisor
 * posterior en los bits 2:0 de 0x0D SCLK_CNTL / 0x12 MCLK_CNTL): 1, 2, 4 u 8. En las R5xx NO está
 * confirmado que sea el mismo formato: SaguiOS lo enseña como «formato clásico, sin confirmar» y los
 * relojes de verdad los da la BIOS (GetEngineClock). sclk/mclk en kHz. */
static inline void gpu_en_pll_classico(uint32_t ref_khz, uint32_t ref_fb, uint32_t sclk_cntl, uint32_t mclk_cntl,
                                       uint32_t *sclk_khz, uint32_t *mclk_khz)
{
    static const uint8_t post[8] = { 1, 2, 4, 8, 3, 0, 6, 12 };
    uint32_t rd = ref_fb & 0xFF, mfb = (ref_fb >> 8) & 0xFF, sfb = (ref_fb >> 16) & 0xFF;
    *sclk_khz = gpu_en_pll_khz(ref_khz, rd, sfb, post[sclk_cntl & 7], true);
    *mclk_khz = gpu_en_pll_khz(ref_khz, rd, mfb, post[mclk_cntl & 7], true);
}

/* ------------------------------------------------------------ elegir el estado */

/* ¿Vale este estado con «ecras» pantallas encendidas? (r100_pm_get_dynpm_state) */
static inline bool gpu_en_estado_serve(const gpu_en_tabela_t *t, int i, int ecras)
{
    if (i < 0 || i >= t->n) return false;
    if (ecras > 0 && t->e[i].sem_ecra) return false;
    if (ecras > 1 && t->e[i].so_um_ecra) return false;
    return true;
}

/* «bajo»: el estado de menos reloj de motor que vale con las pantallas encendidas (Linux, perfil
 * «low» de r420_pm_init_profile, usa el 0 aunque sea «sin pantalla»; aquí no: como dynpm). */
static inline int gpu_en_estado_baixo(const gpu_en_tabela_t *t, int ecras)
{
    int m = -1;
    for (int i = 0; i < t->n; i++)
        if (gpu_en_estado_serve(t, i, ecras) && (m < 0 || t->e[i].sclk < t->e[m].sclk)) m = i;
    return m;
}

/* «alto»: el de por defecto (perfil «high» de Linux). */
static inline int gpu_en_estado_alto(const gpu_en_tabela_t *t) { return t->padrao; }

/* Relojes que se piden de verdad: nunca por encima de los de por defecto de la BIOS (radeon_set_power_state). */
static inline void gpu_en_relojes_alvo(const gpu_en_estado_t *e, const gpu_en_firmware_t *f, uint32_t *sclk, uint32_t *mclk)
{
    *sclk = e->sclk;
    *mclk = e->mclk;
    if (f->sclk_def && *sclk > f->sclk_def) *sclk = f->sclk_def;
    if (f->mclk_def && *mclk > f->mclk_def) *mclk = f->mclk_def;
}

/* ¿Lo leído (GetEngineClock) es lo pedido? El PLL redondea: vale un 3 % o 2 MHz. */
static inline bool gpu_en_relogio_ok(uint32_t pedido, uint32_t lido)
{
    uint32_t d = pedido > lido ? pedido - lido : lido - pedido;
    return d <= 200 || d * 100 <= pedido * 3;
}

/* ------------------------------------------------------------ gobernador «auto» */

#define GPU_EN_AUTO_JANELA_MS   100   /* RADEON_IDLE_LOOP_MS */
#define GPU_EN_AUTO_SUBIR       3     /* ventanas seguidas ocupadas para subir (300 ms; Linux: 200 ms) */
#define GPU_EN_AUTO_BAIXAR      20    /* ventanas seguidas en reposo para bajar (2 s) */
#define GPU_EN_AUTO_PAUSA       10    /* ventanas mínimas entre dos cambios (1 s) */
#define GPU_EN_AUTO_ORDENS_2D   8     /* órdenes 2D o volcados en una ventana para contar como ocupado */

typedef struct {
    int ocupadas, livres;         /* ventanas seguidas */
    int desde_mudanca;
} gpu_en_auto_t;

/* Una ventana: d2d órdenes 2D y volcados, d3d lotes del 3D. Devuelve +1 (subir un estado), -1 (bajar
 * uno) o 0. El 3D trabajando cuenta siempre como ocupado. */
static inline int gpu_en_auto_passo(gpu_en_auto_t *a, uint32_t d2d, uint32_t d3d)
{
    bool ocupado = d3d > 0 || d2d >= GPU_EN_AUTO_ORDENS_2D;
    bool reposo = d3d == 0 && d2d == 0;
    if (ocupado) { a->ocupadas++; a->livres = 0; }
    else if (reposo) { a->livres++; a->ocupadas = 0; }
    else { a->ocupadas = 0; a->livres = 0; }   /* algo de trabajo: ni sube ni baja (histéresis) */
    a->desde_mudanca++;
    if (a->desde_mudanca < GPU_EN_AUTO_PAUSA) return 0;
    if (a->ocupadas >= GPU_EN_AUTO_SUBIR) { a->ocupadas = 0; a->desde_mudanca = 0; return 1; }
    if (a->livres >= GPU_EN_AUTO_BAIXAR) { a->livres = 0; a->desde_mudanca = 0; return -1; }
    return 0;
}

/* El estado vecino que vale (hacia arriba o abajo en reloj de motor) entre «bajo» y «alto». -1: ninguno. */
static inline int gpu_en_vizinho(const gpu_en_tabela_t *t, int atual, int dir, int ecras)
{
    int melhor = -1;
    uint32_t s = atual >= 0 && atual < t->n ? t->e[atual].sclk : 0;
    for (int i = 0; i < t->n; i++) {
        if (i == atual || !gpu_en_estado_serve(t, i, ecras)) continue;
        if (dir > 0 && t->e[i].sclk > s && (melhor < 0 || t->e[i].sclk < t->e[melhor].sclk)) melhor = i;
        if (dir < 0 && t->e[i].sclk < s && (melhor < 0 || t->e[i].sclk > t->e[melhor].sclk)) melhor = i;
    }
    /* «auto» no pasa del de por defecto */
    if (dir > 0 && melhor >= 0 && t->padrao >= 0 && t->e[melhor].sclk > t->e[t->padrao].sclk) return -1;
    return melhor;
}

#endif
