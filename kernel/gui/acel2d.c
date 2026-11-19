// SPDX-License-Identifier: GPL-2.0-or-later
/* acel2d.c - 2D por la GPU para el escritorio: la capa común (D195).
 *
 * El escritorio se compone en un búfer en RAM (video.c) y la CPU copia a la
 * tarjeta lo que cambió. La GPU ayuda donde los píxeles ya están en la
 * memoria de vídeo: mover un rectángulo de la pantalla (arrastrar una ventana,
 * desplazar la terminal) y rellenar uno (borrar). video.c hace la misma
 * operación en el búfer de atrás con la CPU, así que los dos siguen iguales.
 *
 * Al arrancar:
 *   1. se busca en el PCI una tarjeta de la tabla (VMware SVGA II, Intel Gen3 a
 *      Gen8 (D359), Radeon R100-R500, R6xx/R7xx, HD 5000-6000 y GCN 1.0, 3dfx Banshee/Voodoo3/VSA-100, ATI Mach64/Rage, S3 Trio64/ViRGE); la cabecera de arranque puede pedir la CPU
 *      (--sin-acel2d) o forzar un acelerador concreto (--acel2d=...);
 *   2. el acelerador se inicia y se pone a prueba: copias solapadas en las
 *      cuatro direcciones y rellenos en una zona de la memoria de vídeo (debajo
 *      de la pantalla visible si hay memoria; si no, o en la VMware, en las
 *      últimas filas visibles, que después se redibujan), comparadas píxel a
 *      píxel con lo mismo hecho por la CPU, con tiempo máximo;
 *   3. solo si todo coincide y responde a tiempo se activa. Si no, se apaga, se
 *      queda la CPU y el motivo va al log ([ACEL2D]) y a infopc.
 * Después, cualquier orden que no termine a tiempo apaga el acelerador y
 * vuelve a la CPU, que redibuja la pantalla entera desde el búfer de atrás:
 * un fallo de la GPU nunca deja la máquina sin pantalla ni colgada.
 *
 * Con 32 bits por píxel en el orden 0x00RRGGBB (video_formato_direto). En 24 bits
 * (0xRRGGBB empaquetado) y 16 (RGB565), solo los aceleradores que lo declaran
 * (outras_profundidades, D305: la 3dfx; D308: Mach64 y S3); en los demás casos la conversión la hace
 * la copia de la CPU y nada cambia. La zona de prueba se lee y se escribe con los
 * bytes por píxel de la tarjeta; la referencia de la CPU guarda los mismos valores
 * (en el formato de la tarjeta) en un uint32_t por píxel. */
#include "acel2d.h"
#include "../dispositivos/acel_intel_logica.h"   /* D359: familias de Intel sin 2D */
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "memalta.h"
#include "memoria.h"
#include "paginacao.h"
#include "rapido.h"
#include "sensores_conv.h"
#include "sistema.h"
#include "texto.h"
#include "video.h"
#include "kconf.h"
#include "../dispositivos/volcado_logica.h"   /* D387 */
#include "rastro.h"

#define ZONA_A        ACEL2D_ZONA_A   /* filas de la zona de prueba */
#define ZONA_BOOT_A   48     /* la autoprueba del arranque usa solo las primeras */
#define PRAZO_MS      250    /* tiempo máximo de cada espera de la GPU al arrancar (iniciar, autoprueba) */
#define PRAZO_SESION_MS 30   /* D244: en sesión, cualquier espera de la GPU pasa de esto y se apaga */
#define ORDENS_PROVA  160    /* órdenes aleatorias de la autoprueba completa */
#define VOLTAS_BENCH  60     /* copias de la medida */

/* Tarjetas conocidas: fabricante, intervalo de modelos, acelerador y generación.
 * Para añadir una tarjeta de una familia que ya existe basta una línea. */
typedef struct {
    uint16_t fab, de, ate;
    const acel_driver_t *drv;
    int geracao;
    const char *nome;
} acel_placa_t;

static const acel_placa_t placas[] = {
    { 0x15AD, 0x0405, 0x0405, &acel_vmware, 2, "VMware SVGA II" },
    /* D305: 3dfx con motor 2D (Banshee, Voodoo3, VSA-100). La generación: 0 = Banshee,
     * 1 = Voodoo3, 2 = Voodoo4/5 (VSA-100). Los modelos exactos; acel_tdfx.c los vuelve a mirar. */
    { 0x121A, 0x0003, 0x0003, &acel_tdfx, 0, "3dfx Voodoo Banshee" },
    { 0x121A, 0x0005, 0x0005, &acel_tdfx, 1, "3dfx Voodoo3 (y Velocity 100/200)" },
    { 0x121A, 0x0009, 0x0009, &acel_tdfx, 2, "3dfx Voodoo4/5 (VSA-100)" },
    /* D308: ATI Mach64 y Rage (hasta la Rage XL/Mobility) y S3 Trio64/ViRGE. Los intervalos
     * cubren las familias; acel_mach64.c y acel_s3.c solo siguen con los modelos exactos de
     * su lista (y si el identificador que da el propio chip coincide). La generación: Mach64
     * 0 = GX/CX, 1 = CT/VT/GT (Rage II), 2 = Rage Pro/LT Pro/XL/Mobility; S3 0 = ViRGE, 1 = Trio64. */
    { 0x1002, 0x4358, 0x4358, &acel_mach64, 0, "ATI Mach64 CX" },
    { 0x1002, 0x4758, 0x4758, &acel_mach64, 0, "ATI Mach64 GX" },
    { 0x1002, 0x4354, 0x4354, &acel_mach64, 1, "ATI Mach64 CT" },
    { 0x1002, 0x4554, 0x4554, &acel_mach64, 1, "ATI Mach64 ET" },
    { 0x1002, 0x5654, 0x5656, &acel_mach64, 1, "ATI Mach64 VT" },
    { 0x1002, 0x4754, 0x4757, &acel_mach64, 1, "ATI 3D Rage / Rage II" },
    { 0x1002, 0x4759, 0x475A, &acel_mach64, 1, "ATI 3D Rage IIC" },
    { 0x1002, 0x4C47, 0x4C47, &acel_mach64, 1, "ATI 3D Rage LT" },
    { 0x1002, 0x4C54, 0x4C54, &acel_mach64, 1, "ATI 3D Rage LT" },
    { 0x1002, 0x4742, 0x4751, &acel_mach64, 2, "ATI Rage Pro / Rage XL" },
    { 0x1002, 0x4752, 0x4753, &acel_mach64, 2, "ATI Rage XL/XC" },
    { 0x1002, 0x4C42, 0x4C53, &acel_mach64, 2, "ATI Rage LT Pro / Mobility" },
    { 0x5333, 0x5631, 0x5631, &acel_s3, 0, "S3 ViRGE" },
    { 0x5333, 0x883D, 0x883D, &acel_s3, 0, "S3 ViRGE/VX" },
    { 0x5333, 0x8A01, 0x8A01, &acel_s3, 0, "S3 ViRGE/DX/GX" },
    { 0x5333, 0x8A10, 0x8A10, &acel_s3, 0, "S3 ViRGE/GX2" },
    { 0x5333, 0x8904, 0x8904, &acel_s3, 0, "S3 Trio3D" },
    { 0x5333, 0x8A13, 0x8A13, &acel_s3, 0, "S3 Trio3D/2X" },
    { 0x5333, 0x8811, 0x8814, &acel_s3, 1, "S3 Trio64/Trio64V+" },
    { 0x5333, 0x8901, 0x8901, &acel_s3, 1, "S3 Trio64V2" },
    /* NVIDIA GeForce 6/7 (NV4x/G7x) y las integradas de los nForce (D224): los
     * intervalos de cada familia; acel_nv.c solo sigue con los identificadores
     * exactos de su lista y si PMC_BOOT_0 dice el chip esperado. */
    { 0x10DE, 0x0040, 0x004E, &acel_nv, 0, "GeForce 6800 (NV40/NV45)" },
    { 0x10DE, 0x00C0, 0x00CE, &acel_nv, 0, "GeForce 6800 (NV41/NV42)" },
    { 0x10DE, 0x00F1, 0x00F9, &acel_nv, 0, "GeForce 6/7 AGP (puente HSI)" },
    { 0x10DE, 0x0140, 0x014F, &acel_nv, 0, "GeForce 6600 (NV43)" },
    { 0x10DE, 0x0160, 0x016A, &acel_nv, 0, "GeForce 6200 (NV44)" },
    { 0x10DE, 0x0211, 0x0218, &acel_nv, 0, "GeForce 6800 (NV48)" },
    { 0x10DE, 0x0221, 0x0222, &acel_nv, 0, "GeForce 6200 (NV44A)" },
    { 0x10DE, 0x0090, 0x009D, &acel_nv, 0, "GeForce 7800 (G70)" },
    { 0x10DE, 0x01D0, 0x01DF, &acel_nv, 0, "GeForce 7300 (G72)" },
    { 0x10DE, 0x0290, 0x029F, &acel_nv, 0, "GeForce 7900 (G71)" },
    { 0x10DE, 0x0390, 0x039E, &acel_nv, 0, "GeForce 7600 (G73)" },
    { 0x10DE, 0x0240, 0x0247, &acel_nv, 0, "GeForce 6150/6100 (C51)" },
    { 0x10DE, 0x03D0, 0x03D6, &acel_nv, 0, "GeForce 6150SE/6100 (C61)" },
    { 0x10DE, 0x0531, 0x053E, &acel_nv, 0, "GeForce 7050/7025 (C67/C68)" },
    { 0x10DE, 0x07E0, 0x07E5, &acel_nv, 0, "GeForce 7150/7100/7050 (C73)" },
    /* Intel Gen3: el blitter va por el ring de comandos principal (el Amilo: 945GM). */
    { 0x8086, 0x2582, 0x2582, &acel_intel, 3, "Intel 915G" },
    { 0x8086, 0x2592, 0x2592, &acel_intel, 3, "Intel 915GM" },
    { 0x8086, 0x2772, 0x2772, &acel_intel, 3, "Intel 945G" },
    { 0x8086, 0x27A2, 0x27A2, &acel_intel, 3, "Intel 945GM" },
    { 0x8086, 0x27AE, 0x27AE, &acel_intel, 3, "Intel 945GME" },
    { 0x8086, 0x29B2, 0x29B2, &acel_intel, 3, "Intel Q35" },
    { 0x8086, 0x29C2, 0x29C2, &acel_intel, 3, "Intel G33" },
    { 0x8086, 0x29D2, 0x29D2, &acel_intel, 3, "Intel Q33" },
    { 0x8086, 0xA001, 0xA001, &acel_intel, 3, "Intel Pineview G" },
    { 0x8086, 0xA011, 0xA011, &acel_intel, 3, "Intel Pineview M" },
    /* Gen4 y Gen5: las mismas órdenes del blitter, también por el ring principal. */
    { 0x8086, 0x2972, 0x2972, &acel_intel, 4, "Intel 946GZ" },
    { 0x8086, 0x2982, 0x2982, &acel_intel, 4, "Intel G35" },
    { 0x8086, 0x2992, 0x2992, &acel_intel, 4, "Intel Q965" },
    { 0x8086, 0x29A2, 0x29A2, &acel_intel, 4, "Intel G965" },
    { 0x8086, 0x2A02, 0x2A02, &acel_intel, 4, "Intel GM965" },
    { 0x8086, 0x2A12, 0x2A12, &acel_intel, 4, "Intel GME965" },
    { 0x8086, 0x2A42, 0x2A42, &acel_intel, 4, "Intel GM45" },
    { 0x8086, 0x2E02, 0x2E92, &acel_intel, 4, "Intel G4x" },
    { 0x8086, 0x0042, 0x0042, &acel_intel, 5, "Intel HD (Ironlake)" },
    { 0x8086, 0x0046, 0x0046, &acel_intel, 5, "Intel HD (Ironlake M)" },
    /* Gen6 (Sandy Bridge, HD 2000/3000: el G470): ring propio del blitter. */
    { 0x8086, 0x0102, 0x0102, &acel_intel, 6, "Intel HD 2000" },
    { 0x8086, 0x0106, 0x0106, &acel_intel, 6, "Intel HD 2000 M" },
    { 0x8086, 0x010A, 0x010A, &acel_intel, 6, "Intel HD P3000" },
    { 0x8086, 0x0112, 0x0112, &acel_intel, 6, "Intel HD 3000" },
    { 0x8086, 0x0116, 0x0116, &acel_intel, 6, "Intel HD 3000 M" },
    { 0x8086, 0x0122, 0x0122, &acel_intel, 6, "Intel HD 3000" },
    { 0x8086, 0x0126, 0x0126, &acel_intel, 6, "Intel HD 3000 M" },
    /* D359: Gen7 (Ivy Bridge; la HD 4000 del L850 es 0x0166), Gen7.5 (Haswell) y Gen8
     * (Broadwell) por el anillo heredado del BCS. Los intervalos de Haswell y Broadwell
     * tienen huecos: acel_intel.c solo sigue con la lista exacta de acel_intel_logica.h. */
    { 0x8086, 0x0152, 0x0152, &acel_intel, 7, "Intel HD 2500 (Ivy Bridge)" },
    { 0x8086, 0x0156, 0x0156, &acel_intel, 7, "Intel HD 2500 M (Ivy Bridge)" },
    { 0x8086, 0x015A, 0x015A, &acel_intel, 7, "Intel HD P2500 (Ivy Bridge)" },
    { 0x8086, 0x0162, 0x0162, &acel_intel, 7, "Intel HD 4000 (Ivy Bridge)" },
    { 0x8086, 0x0166, 0x0166, &acel_intel, 7, "Intel HD 4000 M (Ivy Bridge)" },
    { 0x8086, 0x016A, 0x016A, &acel_intel, 7, "Intel HD P4000 (Ivy Bridge)" },
    { 0x8086, 0x0402, 0x042E, &acel_intel, 7, "Intel HD (Haswell)" },
    { 0x8086, 0x0A02, 0x0A2E, &acel_intel, 7, "Intel HD (Haswell ULT/ULX)" },
    { 0x8086, 0x0C02, 0x0C2E, &acel_intel, 7, "Intel HD (Haswell SDV)" },
    { 0x8086, 0x0D02, 0x0D2E, &acel_intel, 7, "Intel Iris Pro (Haswell CRW)" },
    { 0x8086, 0x1602, 0x163E, &acel_intel, 8, "Intel HD (Broadwell)" },
    /* Radeon con el motor 2D clásico por registros (sin microcódigo). La
     * generación: 2 = R5xx (AVIVO; la X1400 es RV515/M54, 0x7145), 1 = R300/R420,
     * 0 = R100/R200. Los intervalos son los de la D158 (userland/libamd/amd.c). */
    { 0x1002, 0x7100, 0x72FF, &acel_radeon, 2, "Radeon X1000 (R5xx)" },
    { 0x1002, 0x791E, 0x791F, &acel_radeon, 2, "Radeon Xpress 1200 (RS690)" },
    { 0x1002, 0x7941, 0x7942, &acel_radeon, 2, "Radeon Xpress 1250 (RS600)" },
    { 0x1002, 0x4144, 0x4177, &acel_radeon, 1, "Radeon R300" },
    { 0x1002, 0x4A48, 0x4A6F, &acel_radeon, 1, "Radeon R420" },
    { 0x1002, 0x4E44, 0x4E57, &acel_radeon, 1, "Radeon R300" },
    { 0x1002, 0x5460, 0x5465, &acel_radeon, 1, "Radeon RV370" },
    { 0x1002, 0x5548, 0x555F, &acel_radeon, 1, "Radeon R423" },
    { 0x1002, 0x564A, 0x5657, &acel_radeon, 1, "Radeon RV410" },
    { 0x1002, 0x5B60, 0x5B65, &acel_radeon, 1, "Radeon RV370" },
    { 0x1002, 0x5D48, 0x5D57, &acel_radeon, 1, "Radeon R430" },
    { 0x1002, 0x5E48, 0x5E4F, &acel_radeon, 1, "Radeon RV410" },
    { 0x1002, 0x4C57, 0x4C66, &acel_radeon, 0, "Radeon Mobility (R100/R200)" },
    { 0x1002, 0x5144, 0x515E, &acel_radeon, 0, "Radeon 7000-9250" },
    { 0x1002, 0x5960, 0x5969, &acel_radeon, 0, "Radeon 9200 (RV280)" },
    /* Radeon R6xx/R7xx por el motor DMA (D223). La generación: 7 = R7xx (se
     * elige sola; la HD 4550 del mantenedor es RV710), 6 = R6xx (solo con
     * --acel2d=radeon). Aquí van las familias; el driver solo sigue si el
     * modelo está en su lista exacta (acel_r600.c), y si no, no toca nada. */
    { 0x1002, 0x9540, 0x955F, &acel_r600, 7, "Radeon HD 4350/4550 (RV710)" },
    { 0x1002, 0x9480, 0x949F, &acel_r600, 7, "Radeon HD 4650/4670 (RV730)" },
    { 0x1002, 0x94A0, 0x94B9, &acel_r600, 7, "Radeon HD 4770 (RV740)" },
    { 0x1002, 0x9440, 0x947B, &acel_r600, 7, "Radeon HD 4850/4870 (RV770)" },
    { 0x1002, 0x95C0, 0x95CF, &acel_r600, 6, "Radeon HD 3450 (RV620)" },
    { 0x1002, 0x9590, 0x959B, &acel_r600, 6, "Radeon HD 3650 (RV635)" },
    { 0x1002, 0x94C0, 0x94CD, &acel_r600, 6, "Radeon HD 2400 (RV610)" },
    { 0x1002, 0x9580, 0x958F, &acel_r600, 6, "Radeon HD 2600 (RV630)" },
    { 0x1002, 0x9500, 0x9519, &acel_r600, 6, "Radeon HD 3850/3870 (RV670)" },
    { 0x1002, 0x9400, 0x940F, &acel_r600, 6, "Radeon HD 2900 (R600)" },
    /* D286: Evergreen (HD 5000) y Northern Islands (HD 6000) por el mismo driver (generación 8; Cayman,
     * con dos motores DMA, la 9). La lista exacta está en evergreen_dma.c. Sin las integradas de las APU. */
    { 0x1002, 0x68E0, 0x68FE, &acel_r600, 8, "Radeon HD 5400 (Cedar)" },
    { 0x1002, 0x68C0, 0x68DE, &acel_r600, 8, "Radeon HD 5500/5600 (Redwood)" },
    { 0x1002, 0x68A0, 0x68BF, &acel_r600, 8, "Radeon HD 5700 (Juniper)" },
    { 0x1002, 0x6880, 0x689E, &acel_r600, 8, "Radeon HD 5800/5900 (Cypress/Hemlock)" },
    { 0x1002, 0x6720, 0x673E, &acel_r600, 8, "Radeon HD 6800 (Barts)" },
    { 0x1002, 0x6740, 0x675F, &acel_r600, 8, "Radeon HD 6500/6600 (Turks)" },
    { 0x1002, 0x6840, 0x6859, &acel_r600, 8, "Radeon HD 7500M/7600M (Turks)" },
    { 0x1002, 0x6760, 0x677B, &acel_r600, 8, "Radeon HD 6400 (Caicos)" },
    { 0x1002, 0x6700, 0x671F, &acel_r600, 9, "Radeon HD 6900 (Cayman)" },
    /* D360: Radeon GCN por el motor DMA (acel_gcn.c; la lista exacta en gcn_dma.c). Generación 10 = GCN 1.0
     * (Southern Islands, sin microcódigo): se elige sola, con la autoprueba completa. 11-13 (GCN 1.1, 1.2-4 y
     * Vega) solo dicen en el log la familia y por qué copia la CPU, sin tocar la tarjeta. Sin Hainan ni
     * Topaz (sin pantalla), sin las APU y sin 1002:684C (cae en la fila de Turks). */
    { 0x1002, 0x6780, 0x679F, &acel_gcn, 10, "Radeon HD 7900 / R9 280 (Tahiti)" },
    { 0x1002, 0x6800, 0x6819, &acel_gcn, 10, "Radeon HD 7800 / R9 270 (Pitcairn)" },
    { 0x1002, 0x6820, 0x683F, &acel_gcn, 10, "Radeon HD 7700 / R7 250X (Cape Verde)" },
    { 0x1002, 0x6600, 0x6631, &acel_gcn, 10, "Radeon R7 240/250 (Oland)" },
    { 0x1002, 0x6640, 0x665F, &acel_gcn, 11, "Radeon HD 7790 / R7 260 (Bonaire)" },
    { 0x1002, 0x67A0, 0x67BE, &acel_gcn, 11, "Radeon R9 290/390 (Hawaii)" },
    { 0x1002, 0x6920, 0x6939, &acel_gcn, 12, "Radeon R9 285/380 (Tonga)" },
    { 0x1002, 0x7300, 0x730F, &acel_gcn, 12, "Radeon R9 Fury/Nano (Fiji)" },
    { 0x1002, 0x67C0, 0x67DF, &acel_gcn, 12, "Radeon RX 470/480/570/580 (Polaris 10)" },
    { 0x1002, 0x6FDF, 0x6FDF, &acel_gcn, 12, "Radeon RX 580 2048SP (Polaris 10)" },
    { 0x1002, 0x67E0, 0x67FF, &acel_gcn, 12, "Radeon RX 460/560 (Polaris 11)" },
    { 0x1002, 0x6980, 0x699F, &acel_gcn, 12, "Radeon RX 540/550 (Polaris 12)" },
    { 0x1002, 0x694C, 0x694F, &acel_gcn, 12, "Radeon RX Vega M" },
    { 0x1002, 0x6860, 0x687F, &acel_gcn, 13, "Radeon RX Vega 56/64 (Vega 10)" },
    { 0x1002, 0x69A0, 0x69AF, &acel_gcn, 13, "Radeon Pro Vega 20 (Vega 12)" },
    { 0x1002, 0x66A0, 0x66AF, &acel_gcn, 13, "Radeon VII (Vega 20)" },
};
#define N_PLACAS (int)(sizeof placas / sizeof placas[0])

static const acel_driver_t *drv;      /* acelerador elegido (aunque haya fallado) */
static const acel_placa_t *placa;

/* D244: tarjetas confirmadas, es decir, probadas con éxito. En esas basta la autoprueba corta del
 * arranque; en las demás el acelerador solo se activa si pasa la autoprueba completa (copias
 * solapadas en las cuatro direcciones, rellenos, bordes y muchas órdenes al azar, con tiempo).
 * La VMware SVGA II (la del emulador, donde corre la batería) y las tarjetas reales que el
 * mantenedor ha probado, con su fecha (las demás siguen «por probar» en docs/decisoes.md). */
static const struct { uint16_t fab, modelo; } confirmadas[] = {
    { 0x15AD, 0x0405 },   /* VMware SVGA II */
    { 0x1002, 0x7145 },   /* Mobility X1400 (RV515) del Amilo: acel2d probar --detalle, 176 órdenes iguales, 2026-11-18 (D386) */
    { 0x1002, 0x9553 },   /* Mobility HD 4530/4570 (RV710) del HP dv7: acel2d probar --detalle, 176 órdenes iguales, y junto al 3D, 2026-11-19 (D402) */
};
static bool confirmada;       /* la tarjeta elegida está en la lista */
static pci_disp_t disp;
/* El volcado por la GPU (D387, GART) y la vuelta (D391) solo se encienden solos al arrancar en las tarjetas
 * donde ya se han probado en hardware real (como D244), cada uno por separado. La X1400 del Amilo se colgó
 * con la primera versión (2026-11-19); con el arreglo de esperar a cada franja pasó más de 20.000 volcados
 * sin fallo, y 60.000 cuadros 3D con volcado con el 3D por el CP (v8.5.5, D387): el volcado se enciende solo
 * en ella. La vuelta, no (sin prueba propia en hardware real): «acel2d vuelta encender». */
static const struct { uint16_t fab, modelo; bool volcado, vuelta; } volcado_confirmadas[] = {
    { 0x1002, 0x7145, true, false },   /* Mobility X1400 (RV515) del Amilo: volcado, 2026-11-19 */
};
static bool volcado_confirmada(bool vuelta)
{
    for (unsigned k = 0; k < sizeof volcado_confirmadas / sizeof volcado_confirmadas[0]; k++)
        if (volcado_confirmadas[k].fab && volcado_confirmadas[k].fab == disp.fabricante && volcado_confirmadas[k].modelo == disp.modelo)
            return vuelta ? volcado_confirmadas[k].vuelta : volcado_confirmadas[k].volcado;
    return false;
}
static bool apagado_a_mano;   /* «acel2d apagar» */
static bool ativo, iniciado;
static uint8_t modo;                 /* CAB_ACEL2D */
static char motivo[160] = "sin iniciar";
static uint32_t vram_real_kb;          /* D387: la memoria de vídeo real (0 = no se sabe) */
static char vram_real_fonte[32];

void acel2d_vram_real(uint64_t bytes, const char *fonte)
{
    if (vram_real_kb || bytes < (4ull << 20) || bytes > (64ull << 30)) return;
    vram_real_kb = (uint32_t)(bytes >> 10);
    txt_copia_n(vram_real_fonte, fonte, sizeof vram_real_fonte);
    klog("[ACEL2D] memoria de video real: %u MB (%s); el VBE anuncia %u KB\n", vram_real_kb >> 10, fonte, hw_video_memoria_kb());
}
static acel_tela_t tela;
static uint32_t n_copias, n_rellenos, n_falhas, n_arrastos, n_quadros;
static uint64_t ciclos_arrasto;
static uint32_t sw_ops[ACEL_SW_N], sw_kpx[ACEL_SW_N];   /* D346: lo que hizo la CPU (operaciones y miles de píxeles) */
static uint32_t gpu_kpx_copia, gpu_kpx_relleno;          /* D346: píxeles movidos por la GPU (en miles) */
static uint32_t gpu_px_resto_c, gpu_px_resto_r;
void acel2d_contar_sw(int tipo, uint32_t px)
{
    static uint32_t resto[ACEL_SW_N];
    if ((unsigned)tipo >= ACEL_SW_N) return;
    sw_ops[tipo]++;
    resto[tipo] += px;
    sw_kpx[tipo] += resto[tipo] / 1000; resto[tipo] %= 1000;
}
static uint8_t *zona;                /* zona de prueba en la memoria de vídeo (ventana del kernel) */
static uint32_t px_mascara = 0x00FFFFFF;   /* D305: los bits de un píxel de la tarjeta (0xFFFF en RGB565) */
static int zona_y;                   /* su primera fila */
static uint32_t cmd_original;        /* registro de órdenes del PCI de la tarjeta antes de tocarla */
static uint32_t us_autoprova, us_gpu_boot, us_cpu_boot;   /* medidas del arranque (infopc, acel2d) */

/* D387: el volcado del búfer de atrás por la GPU (más abajo). */
static int  vg_estado = VOLCADO_NO;
static bool vg_pendente, vg_sim, vg_mapeado, vg_gart;
static const acel_volcado_t *vops;
static char vg_motivo[96] = "sin iniciar";
static void volcado_soltar(int novo_estado, const char *m);
static void volcado_falhou(const char *m);
static uint32_t vg_ops, vg_kpx, vg_resto, vg_cpu_ops, vg_us_gpu, vg_us_cpu;
#define VOLCADO_PRAZO_MS 100     /* espera de un volcado (la pantalla entera son unos ms) */
#define VOLCADO_MIN_PX   4096    /* rectángulos más pequeños: la CPU (no compensa dejar la GPU pendiente) */
#define VOLCADO_PROVA_A  ZONA_BOOT_A
static void sim_executar(void);
static void volcado_probar(bool detalle);

/* D391: la vuelta del cuadro del 3D a la RAM por la GPU (más abajo). Usa la misma GART que el volcado:
 * la GART queda encendida mientras uno de los dos esté activo (vg_gart). */
static int  rt_estado = VOLCADO_NO;
static bool rt_mascara = true;          /* la GPU quita el alfa (ROP origen AND 0x00FFFFFF) */
static bool rt_lectura_ok;              /* la lectura por la GART superó su autoprueba en esta sesión */
static char rt_motivo[96] = "sin iniciar";
static uint32_t rt_ops, rt_kpx, rt_resto, rt_cpu_ops, rt_us_gpu, rt_us_cpu, rt_avisos;
static void retorno_soltar(int novo_estado, const char *m);

/* ------------------------------------------------------------------ plazos */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void acel_prazo(acel_prazo_t *p, uint32_t ms)
{
    uint32_t khz = hw_tsc_khz();
    if (ativo && ms > PRAZO_SESION_MS) ms = PRAZO_SESION_MS;   /* D244: en sesión, plazos de unos ms */
    p->fim = khz ? tsc() + (uint64_t)khz * ms : 0;
    p->voltas = 0;
    /* Sin TSC medido: vueltas (cada una lee un registro, ~1 us en hardware real). */
    p->max_voltas = ms * 1000u;
}

/* D285: un plazo que la sesión no recorta (las cargas 3D tardan más que una copia). */
void acel_prazo_longo(acel_prazo_t *p, uint32_t ms)
{
    uint32_t khz = hw_tsc_khz();
    p->fim = khz ? tsc() + (uint64_t)khz * ms : 0;
    p->voltas = 0;
    p->max_voltas = ms * 1000u;
}

bool acel_prazo_esgotado(acel_prazo_t *p)
{
    p->voltas++;
    if (p->fim) return tsc() >= p->fim;
    return p->voltas >= p->max_voltas;
}

/* Vacía los búferes de write-combining antes de que la GPU lea lo que la CPU escribió. */
/* Depuración del Amilo: marcas de progreso en la pantalla (ver vmarca, más abajo). */
static void vmarca_cuadro(int x0, int y0, uint32_t cor, bool tras);
static bool vmarca_ocupado_ok(void);   /* dbg5 */
#define VMARCA_A_X 2
#define VMARCA_B_X 14
#define VMARCA_C_X 26
#define VMARCA_EST_Y (2 + 3 * 12)

static inline void vaciar_wc(void)
{
    __asm__ volatile("lock; orl $0, (%%esp)" ::: "memory");
}

/* D305: píxeles de la zona de prueba con los bytes por píxel de la tarjeta (4, 3 o 2). */
static inline uint8_t *zona_px(int x, int y)
{
    return zona + (uint32_t)y * tela.pitch + (uint32_t)x * (uint32_t)tela.bytes_px;
}

static inline uint32_t zona_ler(int x, int y)
{
    const uint8_t *p = zona_px(x, y);
    if (tela.bytes_px == 4) return *(const uint32_t *)p;
    if (tela.bytes_px == 3) return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    return *(const uint16_t *)p;
}

static inline void zona_escrever(int x, int y, uint32_t v)
{
    uint8_t *p = zona_px(x, y);
    if (tela.bytes_px == 4) *(uint32_t *)p = v;
    else if (tela.bytes_px == 3) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); }
    else *(uint16_t *)p = (uint16_t)v;
}

/* Un color 0x00RRGGBB en el formato de la tarjeta, como lo escribiría la copia de la CPU. */
static uint32_t cor_placa(uint32_t c)
{
    if (tela.bytes_px == 4) return c & 0x00FFFFFF;
    return video_cor_placa(c & 0x00FFFFFF) & px_mascara;
}

/* ------------------------------------------------------------ la CPU de referencia */

static void cpu_copiar(uint32_t *b, int passo, int ox, int oy, int dx, int dy, int l, int a)
{
    if (dy > oy)
        for (int j = a - 1; j >= 0; j--)
            memmove(b + (dy + j) * passo + dx, b + (oy + j) * passo + ox, (size_t)l * 4);
    else
        for (int j = 0; j < a; j++)
            memmove(b + (dy + j) * passo + dx, b + (oy + j) * passo + ox, (size_t)l * 4);
}

static void cpu_preencher(uint32_t *b, int passo, int x, int y, int l, int a, uint32_t cor)
{
    for (int j = 0; j < a; j++)
        for (int i = 0; i < l; i++) b[(y + j) * passo + x + i] = cor;
}

/* ------------------------------------------------------------------ la autoprueba */

typedef struct { int tipo, ox, oy, dx, dy, l, a; uint32_t cor; } ordem_t;   /* tipo 0 copia, 1 relleno */

static uint32_t semente;
static uint32_t aleatorio(uint32_t n) { semente = semente * 1103515245u + 12345u; return ((semente >> 8) % n); }

/* Las órdenes fijas: solapes en las cuatro direcciones, en la misma fila y en la
 * misma columna, rellenos, y los bordes de la zona (columna 0, la última, la
 * primera y la última fila). Las coordenadas son de la zona (ancho L, alto A). */
static int ordens_fixas(ordem_t *o, int L, int A)
{
    int n = 0;
    o[n++] = (ordem_t){ 1, 0, 0, 10, 5, 30, 12, 0x00A1B2C3 };
    o[n++] = (ordem_t){ 0, 0, 0, 7, 3, 60, 30, 0 };            /* hacia abajo y a la derecha, solapada */
    o[n++] = (ordem_t){ 0, 20, 15, 11, 9, 70, 25, 0 };         /* hacia arriba y a la izquierda */
    o[n++] = (ordem_t){ 0, 30, 36, 36, 36, 60, 8, 0 };         /* misma fila, a la derecha */
    o[n++] = (ordem_t){ 0, 40, 20, 33, 20, 50, 10, 0 };        /* misma fila, a la izquierda */
    o[n++] = (ordem_t){ 0, 100, 2, 100, 6, 20, A - 8, 0 };     /* misma columna, hacia abajo */
    o[n++] = (ordem_t){ 0, 130, 9, 130, 1, 20, A - 10, 0 };    /* misma columna, hacia arriba */
    o[n++] = (ordem_t){ 0, 150, 1, 140, 5, 40, A - 6, 0 };     /* abajo a la izquierda */
    o[n++] = (ordem_t){ 0, 160, 7, 171, 2, 40, A - 9, 0 };     /* arriba a la derecha */
    o[n++] = (ordem_t){ 1, L - 1, A - 1, 0, 0, 1, 1, 0x00FFFFFF };   /* el último píxel */
    o[n++] = (ordem_t){ 1, 0, 0, 0, 0, 1, A, 0x00102030 };          /* la columna 0 */
    o[n++] = (ordem_t){ 0, L - 64, 0, 0, A - 16, 64, 16, 0 };        /* de la esquina derecha a la izquierda */
    o[n++] = (ordem_t){ 0, 0, A - 20, L - 50, 0, 50, 20, 0 };        /* y al revés */
    o[n++] = (ordem_t){ 0, 1, 0, 0, 0, L - 1, A, 0 };                /* toda la zona un píxel a la izquierda */
    o[n++] = (ordem_t){ 0, 0, 0, 0, 1, L, A - 1, 0 };                /* toda la zona una fila hacia abajo */
    o[n++] = (ordem_t){ 1, L - 33, 3, 0, 0, 33, A - 3, 0x00FF00FF }; /* hasta el borde derecho */
    return n;
}

static void ordem_aleatoria(ordem_t *o, int L, int A)
{
    o->tipo = (int)aleatorio(4) == 0;
    o->l = 1 + (int)aleatorio((uint32_t)L);
    o->a = 1 + (int)aleatorio((uint32_t)A);
    o->ox = (int)aleatorio((uint32_t)(L - o->l + 1));
    o->oy = (int)aleatorio((uint32_t)(A - o->a + 1));
    /* El destino cerca del origen la mitad de las veces: así hay muchos solapes. */
    if (aleatorio(2)) {
        o->dx = o->ox + (int)aleatorio(17) - 8;
        o->dy = o->oy + (int)aleatorio(9) - 4;
        if (o->dx < 0) o->dx = 0;
        if (o->dy < 0) o->dy = 0;
        if (o->dx > L - o->l) o->dx = L - o->l;
        if (o->dy > A - o->a) o->dy = A - o->a;
    } else {
        o->dx = (int)aleatorio((uint32_t)(L - o->l + 1));
        o->dy = (int)aleatorio((uint32_t)(A - o->a + 1));
    }
    o->cor = aleatorio(0x1000000);
}

static bool executar_gpu(const ordem_t *o)
{
    if (o->tipo) return drv->preencher(o->ox, zona_y + o->oy, o->l, o->a, cor_placa(o->cor));
    return drv->copiar(o->ox, zona_y + o->oy, o->dx, zona_y + o->dy, o->l, o->a);
}

static void executar_cpu(uint32_t *ref, int L, const ordem_t *o)
{
    if (o->tipo) cpu_preencher(ref, L, o->ox, o->oy, o->l, o->a, cor_placa(o->cor));
    else cpu_copiar(ref, L, o->ox, o->oy, o->dx, o->dy, o->l, o->a);
}

/* Pone la zona y la referencia en el mismo dibujo (que cambia en cada píxel). */
static void zona_padrao(uint32_t *ref, int L, int A)
{
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            uint32_t v = ((uint32_t)x * 2654435761u ^ (uint32_t)y * 40503u ^ ((uint32_t)(x + y) << 13)) & px_mascara;
            zona_escrever(x, y, v);
            ref[y * L + x] = v;
        }
    vaciar_wc();
}

/* La zona visible (VMware, o sin memoria debajo) se redibuja desde el búfer de atrás. */
static void zona_restaurar(int A)
{
    if (zona_y < tela.alt) {
        video_marcar(0, zona_y, tela.larg, A);
        video_mostrar();
    }
}

/* La autoprueba: devuelve true si la GPU dio los mismos píxeles que la CPU. */
static bool autoprova(bool completa, bool simular, info_acel2d_prova_t *r, char *m, int nm)
{
    int L = tela.larg, A = completa ? ZONA_A : ZONA_BOOT_A;
    uint32_t *ref = kmalloc((uint32_t)(L * A) * 4);
    if (!ref) { snformatar(m, (size_t)nm, "sin memoria para la autoprueba"); return false; }
    static ordem_t fixas[32];
    int nf = ordens_fixas(fixas, L, A);
    int total = nf + (completa ? ORDENS_PROVA : 24);
    semente = 0x5A6E1u;
    zona_padrao(ref, L, A);
    bool ok = true;
    for (int k = 0; k < total && ok; k++) {
        ordem_t o = k < nf ? fixas[k] : (ordem_t){ 0 };
        if (k >= nf) ordem_aleatoria(&o, L, A);
        if (!executar_gpu(&o)) { snformatar(m, (size_t)nm, "la GPU rechazo la orden %d", k); ok = false; break; }
        executar_cpu(ref, L, &o);
        /* Cada pocas órdenes se espera: así una orden mal hecha se ve antes. */
        if ((k & 7) == 7 && !drv->esperar()) { snformatar(m, (size_t)nm, "tiempo agotado en la orden %d", k); ok = false; }
    }
    bool tempo = ok && simular && (modo & ACEL2D_PROVA_TEMPO);
    if (ok && (tempo || !drv->esperar())) { snformatar(m, (size_t)nm, "tiempo agotado al final%s", tempo ? " (simulado)" : ""); ok = false; }
    uint32_t dif = 0, sg = 0, sc = 0;
    int px = -1, py = -1;
    if (ok) {
        if (simular && (modo & ACEL2D_PROVA_PIXEL)) zona_escrever(L / 3, A / 2, zona_ler(L / 3, A / 2) ^ 0x00FF00);
        for (int y = 0; y < A; y++)
            for (int x = 0; x < L; x++) {
                uint32_t g = zona_ler(x, y) & px_mascara, c = ref[y * L + x];
                sg = sg * 31 + g; sc = sc * 31 + c;
                if (g != c) { if (!dif) { px = x; py = y; klog("[ACEL2D] primer pixel distinto: gpu %x cpu %x\n", g, c); } dif++; }
            }
        if (dif) {
            snformatar(m, (size_t)nm, "%u pixeles distintos de la CPU (el primero en %d,%d)%s", dif, px, py,
                       simular && (modo & ACEL2D_PROVA_PIXEL) ? " (simulado)" : "");
            ok = false;
        }
    }
    if (r) { r->ordens = (uint32_t)total; r->diferentes = dif; r->soma_gpu = sg; r->soma_cpu = sc; }
    klog("[ACEL2D] autoprueba %s: %d ordenes en %dx%d (fila %d, %s), %s%s%s, suma gpu %x cpu %x\n",
         completa ? "completa" : "del arranque", total, L, A, zona_y, zona_y >= tela.alt ? "fuera de la pantalla" : "visible",
         ok ? "IGUAL que la CPU" : "FALLO", ok ? "" : ": ", ok ? "" : m, sg, sc);
    kfree(ref);
    return ok;
}

/* La medida: VOLTAS_BENCH copias de casi toda la zona por la GPU y lo que hace
 * la CPU sin acelerador (mover en el búfer de atrás y copiar el destino a la
 * tarjeta). Devuelve los microsegundos de cada una. */
static void medir(info_acel2d_prova_t *r)
{
    int L = tela.larg, A = ZONA_A, l = L - 8, a = A - 4;
    uint32_t *ref = kmalloc((uint32_t)(L * A) * 4);
    if (!ref) return;
    memset(ref, 0, (uint32_t)(L * A) * 4);
    uint32_t khz = hw_tsc_khz();
    uint64_t t0 = tsc();
    bool ok = true;
    for (int k = 0; k < VOLTAS_BENCH && ok; k++)
        ok = (k & 1) ? drv->copiar(4, zona_y + 2, 0, zona_y, l, a) : drv->copiar(0, zona_y, 4, zona_y + 2, l, a);
    if (ok) ok = drv->esperar();
    uint64_t t1 = tsc();
    bool sse = rapido_inicio();
    for (int k = 0; k < VOLTAS_BENCH; k++) {
        int ox = (k & 1) ? 4 : 0, oy = (k & 1) ? 2 : 0, dx = 4 - ox, dy = 2 - oy;
        cpu_copiar(ref, L, ox, oy, dx, dy, l, a);
        for (int j = 0; j < a; j++)
            rapido_copiar_tela(zona_px(dx, dy + j), ref + (dy + j) * L + dx, (uint32_t)(l * tela.bytes_px));
    }
    rapido_fim(sse);
    vaciar_wc();
    uint64_t t2 = tsc();
    kfree(ref);
    if (khz) {
        r->us_gpu = ok ? (uint32_t)sconv_div64((t1 - t0) * 1000, khz) : 0;
        r->us_cpu = (uint32_t)sconv_div64((t2 - t1) * 1000, khz);
    }
    klog("[ACEL2D] medida: %d copias de %dx%d: GPU %u us, CPU %u us%s\n", VOLTAS_BENCH, l, a, r->us_gpu, r->us_cpu,
         ok ? "" : " (la GPU fallo)");
}

/* D243: tres tandas de órdenes iguales por la GPU y por la CPU (rellenos, copias sin solape y
 * copias solapadas). Cada tanda parte del mismo dibujo; las sumas de la zona tienen que coincidir.
 * La CPU hace lo que haría sin acelerador: mover en el búfer y escribir el destino en la tarjeta. */
#define OPS_BENCH 120
static uint32_t soma_zona(const uint32_t *b, int passo, int L, int A)
{
    uint32_t h = 0;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) h = h * 31 + (b[y * passo + x] & px_mascara);
    return h;
}

/* D305: la misma suma sobre la zona de la tarjeta, con sus bytes por píxel. */
static uint32_t soma_placa(int L, int A)
{
    uint32_t h = 0;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) h = h * 31 + (zona_ler(x, y) & px_mascara);
    return h;
}

static void medir_tandas(info_acel2d_prova_t *r)
{
    int L = tela.larg, A = ZONA_A;
    uint32_t khz = hw_tsc_khz();
    uint32_t *ref = kmalloc((uint32_t)(L * A) * 4);
    ordem_t *os = kmalloc(OPS_BENCH * sizeof(ordem_t));
    if (!ref || !os || !khz) { if (ref) kfree(ref); if (os) kfree(os); return; }
    for (int t = 0; t < 3; t++) {
        semente = 0xBE7C0u + (uint32_t)t;
        uint32_t pix = 0;
        for (int k = 0; k < OPS_BENCH; k++) {
            ordem_t *o = &os[k];
            o->tipo = t == 0;
            o->l = 16 + (int)aleatorio((uint32_t)(L / 2 - 16));
            o->a = 4 + (int)aleatorio((uint32_t)(A / 2 - 4));
            o->ox = (int)aleatorio((uint32_t)(L - o->l + 1));
            o->oy = (int)aleatorio((uint32_t)(A - o->a + 1));
            o->cor = aleatorio(0x1000000);
            if (t == 2) {   /* destino a pocos píxeles del origen: siempre solapa */
                o->dx = o->ox + (int)aleatorio(9) - 4;
                o->dy = o->oy + (int)aleatorio(5) - 2;
                if (o->dx < 0) o->dx = 0;
                if (o->dy < 0) o->dy = 0;
                if (o->dx > L - o->l) o->dx = L - o->l;
                if (o->dy > A - o->a) o->dy = A - o->a;
            } else {
                o->dx = (int)aleatorio((uint32_t)(L - o->l + 1));
                o->dy = (int)aleatorio((uint32_t)(A - o->a + 1));
            }
            pix += (uint32_t)(o->l * o->a);
        }
        r->b[t].ops = OPS_BENCH;
        r->b[t].pixeis = pix;
        zona_padrao(ref, L, A);
        uint64_t t0 = tsc();
        bool ok = true;
        for (int k = 0; k < OPS_BENCH && ok; k++) ok = executar_gpu(&os[k]);
        if (ok) ok = drv->esperar();
        uint64_t t1 = tsc();
        r->b[t].us_gpu = ok ? (uint32_t)sconv_div64((t1 - t0) * 1000, khz) : 0;
        r->b[t].soma_gpu = ok ? soma_placa(L, A) : 0;
        zona_padrao(ref, L, A);
        bool sse = rapido_inicio();
        uint64_t t2 = tsc();
        for (int k = 0; k < OPS_BENCH; k++) {
            const ordem_t *o = &os[k];
            executar_cpu(ref, L, o);
            int dx = o->tipo ? o->ox : o->dx, dy = o->tipo ? o->oy : o->dy;
            for (int j = 0; j < o->a; j++)
                rapido_copiar_tela(zona_px(dx, dy + j), ref + (dy + j) * L + dx, (uint32_t)(o->l * tela.bytes_px));
        }
        rapido_fim(sse);
        vaciar_wc();
        uint64_t t3 = tsc();
        r->b[t].us_cpu = (uint32_t)sconv_div64((t3 - t2) * 1000, khz);
        r->b[t].soma_cpu = soma_zona(ref, L, L, A);
        klog("[ACEL2D] tanda %d: %d ordenes, %u pixeles: GPU %u us (suma %x), CPU %u us (suma %x)\n", t, OPS_BENCH, pix,
             r->b[t].us_gpu, r->b[t].soma_gpu, r->b[t].us_cpu, r->b[t].soma_cpu);
    }
    r->bn = 3;
    kfree(os);
    kfree(ref);
}

/* La comparación de velocidad del arranque: las mismas copias de la zona por la
 * GPU y lo que haría la CPU sin acelerador para que la pantalla cambie igual
 * (escribir el rectángulo de destino en la tarjeta desde la RAM; mover el búfer
 * de atrás lo hace la CPU en los dos casos). Así se elige de verdad: en una
 * máquina virtual con la memoria de vídeo mapeada directamente (KVM) la GPU
 * emulada puede ser más lenta que la CPU. */
#define VOLTAS_BOOT 8
static bool comparar_velocidade(uint32_t *ug, uint32_t *uc)
{
    int L = tela.larg, A = ZONA_BOOT_A, l = L - 8, a = A - 4;
    uint32_t khz = hw_tsc_khz();
    uint32_t *ram = kmalloc((uint32_t)(L * A) * 4);
    if (!ram || !khz) { if (ram) kfree(ram); return false; }
    memset(ram, 0, (uint32_t)(L * A) * 4);
    uint64_t t0 = tsc();
    bool ok = true;
    for (int k = 0; k < VOLTAS_BOOT && ok; k++)
        ok = (k & 1) ? drv->copiar(4, zona_y + 2, 0, zona_y, l, a) : drv->copiar(0, zona_y, 4, zona_y + 2, l, a);
    if (ok) ok = drv->esperar();
    uint64_t t1 = tsc();
    bool sse = rapido_inicio();
    for (int k = 0; k < VOLTAS_BOOT; k++) {
        int dx = (k & 1) ? 0 : 4, dy = (k & 1) ? 0 : 2;
        for (int j = 0; j < a; j++)
            rapido_copiar_tela(zona_px(dx, dy + j), ram + (dy + j) * L + dx, (uint32_t)(l * tela.bytes_px));
        if (drv->atualizar) drv->atualizar(dx, zona_y + dy, l, a);   /* la VMware lo pide también con la CPU */
    }
    rapido_fim(sse);
    vaciar_wc();
    if (ok && drv->atualizar) ok = drv->esperar();
    uint64_t t2 = tsc();
    kfree(ram);
    *ug = (uint32_t)sconv_div64((t1 - t0) * 1000, khz);
    *uc = (uint32_t)sconv_div64((t2 - t1) * 1000, khz);
    return ok;
}

/* Los BAR de memoria (de 32 bits, o de 64 por debajo de 4 GB) de una tarjeta. */
static int bars_memoria(const pci_disp_t *d, uint32_t *base, int max)
{
    int n = 0;
    for (uint8_t r = 0x10; r <= 0x24 && n < max; r += 4) {
        uint32_t v = pci_le32(d, r);
        if (v & 1) continue;                       /* de E/S */
        bool b64 = ((v >> 1) & 3) == 2;
        uint32_t alto = b64 && r < 0x24 ? pci_le32(d, (uint8_t)(r + 4)) : 0;
        if (b64) r += 4;
        if (alto || !(v & ~0xFu)) continue;        /* por encima de 4 GB o sin asignar */
        base[n++] = v & ~0xFu;
    }
    return n;
}

/* Qué tarjeta de vídeo tiene la pantalla del VBE, solo leyendo el espacio de
 * configuración del PCI (sin tocar ningún registro de ninguna GPU): la dueña del
 * BAR de memoria de base más alta que no pasa de vesa_fb (como mucho 512 MB por
 * debajo). Así, en un portátil con dos GPU (una Intel y una Radeon, por
 * ejemplo) nunca se programa la que no muestra la pantalla. -1: ninguna. */
static int dona_da_tela(const pci_disp_t *v, int n, uint32_t fb)
{
    int dona = -1;
    uint32_t melhor = 0;
    for (int i = 0; i < n; i++) {
        uint32_t b[6];
        int nb = bars_memoria(&v[i], b, 6);
        for (int k = 0; k < nb; k++)
            if (b[k] <= fb && fb - b[k] < 0x20000000u && (dona < 0 || b[k] > melhor)) { dona = i; melhor = b[k]; }
    }
    if (dona >= 0 && !(pci_le32(&v[dona], 0x04) & 0x02)) {
        klog("[ACEL2D] la tarjeta %04x:%04x tiene la memoria apagada en el PCI (orden %x)\n",
             v[dona].fabricante, v[dona].modelo, pci_le32(&v[dona], 0x04) & 0xFFFF);
        return -1;
    }
    return dona;
}

/* Apaga el acelerador y deja el registro de órdenes del PCI como estaba. */
static void soltar(void)
{
    /* D387: la GART, antes que el motor (el volcado real va sobre el 2D: sin 2D, sin volcado). D391: la vuelta
     * también (la misma GART y el mismo motor). */
    if (vg_gart && !vg_sim) {
        if (rt_estado == VOLCADO_ACTIVO) retorno_soltar(VOLCADO_FALLO, "se apago el acelerador 2D");
        volcado_soltar(vg_estado == VOLCADO_ACTIVO ? VOLCADO_FALLO : vg_estado, "se apago el acelerador 2D");
    }
    if (drv && drv->desligar) drv->desligar();
    if (placa) pci_escreve32(&disp, 0x04, cmd_original & 0xFFFFu);   /* el estado (bits altos) no se toca */
    iniciado = false;
}

/* ------------------------------------------------------------------ elección */

static const char *nome_modo(int m)
{
    switch (m) {
    case ACEL2D_CPU: return "cpu";
    case ACEL2D_VMWARE: return "vmware";
    case ACEL2D_INTEL: return "intel";
    case ACEL2D_RADEON: return "radeon";
    case ACEL2D_NVIDIA: return "nvidia";
    case ACEL2D_TDFX: return "tdfx";
    case ACEL2D_MACH64: return "mach64";
    case ACEL2D_S3: return "s3";
    default: return "auto";
    }
}

static bool preparar_zona(void)
{
    int A = ZONA_A;
    bool cabe = tela.memoria >= tela.pitch * (uint32_t)(tela.alt + A);
    if (drv->fora_da_tela && cabe) {
        zona_y = tela.alt;
        if (!zona) zona = (uint8_t *)janela_mapear_tipo(tela.fb_fisico + tela.pitch * (uint32_t)zona_y,
                                                          tela.pitch * (uint32_t)A, PG_PCD | PG_PWT);
    } else {
        zona_y = tela.alt - A;
        zona = video_placa() + tela.pitch * (uint32_t)zona_y;
    }
    return zona != NULL;
}

void acel2d_iniciar(void)
{
    modo = cabecalho_boot[CAB_ACEL2D];
    int pedido = modo & ACEL2D_MODO_MASCARA;
    /* D323: modo seguro de la aceleración (Mayús al arrancar o CAB_ACEL_SEGURO): la CPU hace todo. */
    bool seguro = (info_boot.opcoes & OPC_BOOT_ACEL_SEGURO) || (modo & CAB_ACEL_SEGURO);
    if (seguro) pedido = ACEL2D_CPU;
    if (!video_ativo()) { snformatar(motivo, sizeof motivo, "sin modo grafico"); return; }
    tela.fb_fisico = info_boot.vesa_fb;
    tela.pitch = video_pitch();
    tela.larg = (int)video_largura();
    tela.alt = (int)video_altura();
    tela.memoria = hw_video_memoria_kb() * 1024;
    if (tela.memoria < tela.pitch * (uint32_t)tela.alt) tela.memoria = tela.pitch * (uint32_t)tela.alt;
    if (pedido == ACEL2D_CPU) {
        snformatar(motivo, sizeof motivo, seguro ? "modo seguro de la aceleracion: la CPU hace todo" : "la opcion de arranque pide la CPU (--sin-acel2d)");
        klog("[ACEL2D] desactivado: %s\n", motivo);
        return;
    }
    /* D305: además de 32 bits 0x00RRGGBB, 24 bits 0xRRGGBB empaquetado y RGB565, pero estos dos
     * solo con un acelerador que los acepte (se mira al elegirlo, más abajo). */
    bool direto = video_formato_direto();
    uint32_t bpp = video_bpp();
    bool outra = !direto &&
        ((bpp == 24 && video_cor_placa(0xFF0000) == 0xFF0000 && video_cor_placa(0x00FF00) == 0x00FF00 &&
          video_cor_placa(0x0000FF) == 0x0000FF) ||
         (bpp == 16 && video_cor_placa(0xFF0000) == 0xF800 && video_cor_placa(0x00FF00) == 0x07E0 &&
          video_cor_placa(0x0000FF) == 0x001F));
    tela.bytes_px = direto ? 4 : (int)(bpp + 7) / 8;
    px_mascara = tela.bytes_px == 2 ? 0xFFFFu : 0x00FFFFFFu;
    if ((!direto && !outra) || tela.larg < 320 || tela.alt < 2 * ZONA_A) {
        snformatar(motivo, sizeof motivo, "la pantalla no es de 32 bits 0x00RRGGBB (%u bits): copia la CPU", video_bpp());
        klog("[ACEL2D] CPU: %s\n", motivo);
        return;
    }
    /* Las tarjetas de vídeo (VGA, XGA, 3D y «otra»: la 2.ª función de algunas
     * Intel, o la GPU dedicada de un portátil con dos). */
    pci_disp_t v[8];
    int n = 0;
    static const uint8_t subclasses[] = { 0x00, 0x01, 0x02, 0x80 };
    for (int k = 0; k < 4 && n < 8; k++) n += pci_procurar_classe(0x03, subclasses[k], v + n, 8 - n);
    for (int i = 0; i < n; i++)
        klog("[ACEL2D] tarjeta de video %04x:%04x en %u:%u.%u (clase 03%02x)\n", v[i].fabricante, v[i].modelo,
             v[i].barramento, v[i].dispositivo, v[i].funcao, v[i].subclasse);
    /* Solo se mira la que tiene la pantalla, y solo si es de una familia de la tabla. */
    int d = dona_da_tela(v, n, tela.fb_fisico);
    if (d < 0) {
        snformatar(motivo, sizeof motivo, "ninguna tarjeta de video tiene la pantalla (%x) en sus BAR: copia la CPU",
                   tela.fb_fisico);
        klog("[ACEL2D] CPU: %s\n", motivo);
        return;
    }
    klog("[ACEL2D] la pantalla (%x) es de la tarjeta %04x:%04x\n", tela.fb_fisico, v[d].fabricante, v[d].modelo);
    for (int k = 0; k < N_PLACAS; k++) {
        const acel_placa_t *p = &placas[k];
        if (p->fab != v[d].fabricante || v[d].modelo < p->de || v[d].modelo > p->ate) continue;
        if (pedido != ACEL2D_AUTO && p->drv->modo != pedido) continue;
        placa = p; disp = v[d]; drv = p->drv;
        break;
    }
    if (!placa) {
        if (pedido != ACEL2D_AUTO)
            snformatar(motivo, sizeof motivo, "se pidio el acelerador %s y la pantalla no es de una tarjeta suya (%04x:%04x)",
                       nome_modo(pedido), v[d].fabricante, v[d].modelo);
        else {
            /* D359: las Intel que se reconocen pero no llevan 2D dicen por qué (sin tocarlas). */
            const char *fam = NULL, *porque = NULL;
            int g = v[d].fabricante == 0x8086 ? intel_familia_sem_2d(v[d].modelo, &fam, &porque) : 0;
            if (g)
                snformatar(motivo, sizeof motivo, "la tarjeta %04x:%04x (Intel Gen%d, %s): %s: copia la CPU (no se toca)",
                           v[d].fabricante, v[d].modelo, g, fam, porque);
            else
                snformatar(motivo, sizeof motivo, "la tarjeta %04x:%04x no es de una familia con acelerador: copia la CPU (no se toca)",
                           v[d].fabricante, v[d].modelo);
        }
        klog("[ACEL2D] CPU: %s\n", motivo);
        return;
    }
    klog("[ACEL2D] %s (%04x:%04x, generacion %d): acelerador %s%s\n", placa->nome, disp.fabricante, disp.modelo,
         placa->geracao, drv->nome, pedido != ACEL2D_AUTO ? " (forzado)" : "");
    if (!direto && !drv->outras_profundidades) {
        snformatar(motivo, sizeof motivo, "la pantalla no es de 32 bits 0x00RRGGBB (%u bits): copia la CPU", bpp);
        klog("[ACEL2D] CPU: %s\n", motivo);
        placa = NULL; drv = NULL;
        return;
    }
    if (!direto) klog("[ACEL2D] pantalla de %u bits (%d bytes por pixel): el acelerador %s la acepta (D305)\n", bpp,
                      tela.bytes_px, drv->nome);
    /* Los plazos de la GPU se miden con el TSC: sin él no se arriesga nada. */
    if (!hw_tsc_khz()) {
        snformatar(motivo, sizeof motivo, "%s: sin TSC medido para los plazos de la GPU: copia la CPU", placa->nome);
        klog("[ACEL2D] CPU: %s\n", motivo);
        placa = NULL; drv = NULL;
        return;
    }
    cmd_original = pci_le32(&disp, 0x04);
    char m[128] = "";   /* D357: cabe «etapa N:» delante del motivo del driver */
    /* Iniciar puede esperar (la sonda de la VMware, hasta 3 s): con las
     * interrupciones activas, para que el reloj siga contando. */
    uint64_t t0 = tsc();
    if (!drv->iniciar(&disp, placa->geracao, &tela, m, sizeof m)) {
        soltar();
        snformatar(motivo, sizeof motivo, "%s: no se pudo iniciar: %s", placa->nome, m);
        klog("[ACEL2D] CPU: %s\n", motivo);
        n_falhas++;
        return;
    }
    iniciado = true;
    uint32_t khz = hw_tsc_khz();
    klog("[ACEL2D] %s iniciado en %u ms\n", drv->nome, (uint32_t)sconv_div64(tsc() - t0, khz));
    bool sim = int_salva_desliga();   /* la autoprueba, sin que nada más toque la pantalla */
    if (!preparar_zona()) {
        soltar();
        int_restaura(sim);
        snformatar(motivo, sizeof motivo, "%s: sin ventana para la zona de prueba", placa->nome);
        klog("[ACEL2D] CPU: %s\n", motivo);
        return;
    }
    t0 = tsc();
    confirmada = false;
    for (unsigned k = 0; k < sizeof confirmadas / sizeof confirmadas[0]; k++)
        if (confirmadas[k].fab == disp.fabricante && confirmadas[k].modelo == disp.modelo) confirmada = true;
    /* D244: en una tarjeta sin confirmar (elegida sola) la autoprueba es la completa. */
    bool completa = !confirmada && pedido == ACEL2D_AUTO;
    klog("[ACEL2D] tarjeta %s: autoprueba %s\n", confirmada ? "confirmada" : "no confirmada",
         completa ? "completa" : "corta");
    bool ok = autoprova(completa, true, NULL, m, sizeof m);
    us_autoprova = (uint32_t)sconv_div64((tsc() - t0) * 1000, khz);
    if (!us_autoprova) us_autoprova = 1;
    klog("[ACEL2D] la autoprueba del arranque tardo %u us\n", us_autoprova);
    /* ¿Compensa? Solo en la elección automática: forzado, se usa aunque sea más lento. */
    bool lenta = false;
    if (ok && pedido == ACEL2D_AUTO) {
        if (!comparar_velocidade(&us_gpu_boot, &us_cpu_boot)) {
            ok = false;
            snformatar(m, sizeof m, "tiempo agotado al medir la velocidad");
        } else {
            lenta = us_gpu_boot > us_cpu_boot + us_cpu_boot / 2;
            klog("[ACEL2D] velocidad: %d copias de %dx%d: GPU %u us, CPU %u us: %s\n", VOLTAS_BOOT, tela.larg - 8,
                 ZONA_BOOT_A - 4, us_gpu_boot, us_cpu_boot, lenta ? "la GPU es mas lenta, se queda la CPU" : "compensa la GPU");
        }
    }
    zona_restaurar(completa ? ZONA_A : ZONA_BOOT_A);
    if (!ok || lenta) {
        soltar();
        int_restaura(sim);
        video_mostrar_tudo();
        if (lenta)
            snformatar(motivo, sizeof motivo, "%s: autoprueba superada, pero la GPU (%u us) es mas lenta que la CPU (%u us)",
                       placa->nome, us_gpu_boot, us_cpu_boot);
        else
            snformatar(motivo, sizeof motivo, "%s: autoprueba fallida (%s)", placa->nome, m);
        klog("[ACEL2D] CPU: %s\n", motivo);
        if (!lenta) n_falhas++;
        return;
    }
    ativo = true;
    int_restaura(sim);
    if (us_gpu_boot)
        snformatar(motivo, sizeof motivo, "%s (%04x:%04x, %s): autoprueba superada en %u ms; GPU %u us, CPU %u us",
                   placa->nome, disp.fabricante, disp.modelo, confirmada ? "confirmada" : "por autoprueba",
                   (us_autoprova + 999) / 1000, us_gpu_boot, us_cpu_boot);
    else
        snformatar(motivo, sizeof motivo, "%s (%04x:%04x, %s): autoprueba superada en %u ms (forzado)",
                   placa->nome, disp.fabricante, disp.modelo, confirmada ? "confirmada" : "por autoprueba",
                   (us_autoprova + 999) / 1000);
    klog("[ACEL2D] activo: %s, acelerador %s\n", motivo, drv->nome);
    video_mostrar_tudo();
}

bool acel2d_ativo(void) { return ativo; }

/* D285: la tarjeta que tiene la pantalla y su familia, para la capa 3D (aunque el
 * 2D haya quedado en la CPU). «suspeito»: el 2D falló o lo apagó el vigilante. */
bool acel2d_placa(pci_disp_t *d, int *geracao, const acel_driver_t **driver, acel_tela_t *t, bool *suspeito)
{
    if (!placa || !drv) return false;
    *d = disp; *geracao = placa->geracao; *driver = drv; *t = tela;
    *suspeito = n_falhas != 0 && !ativo;
    return true;
}
const char *acel2d_nome(void) { return ativo ? drv->nome : "cpu"; }
const char *acel2d_motivo(void) { return motivo; }
bool acel2d_forcado(void) { return (modo & ACEL2D_MODO_MASCARA) > ACEL2D_CPU; }

void acel2d_desligar(const char *m)
{
    if (!ativo) return;
    ativo = false;
    n_falhas++;
    soltar();
    /* D244: el vigilante. Para el resto de la sesión todo pasa a la CPU, sin esperar más. */
    snformatar(motivo, sizeof motivo, "%s: apagado por el vigilante, sigue la CPU (%s)", placa ? placa->nome : "?", m);
    klog("[ACEL2D] apagado: %s\n", m);
    video_mostrar_tudo();   /* la CPU redibuja todo desde el búfer de atrás */
}

/* «acel2d apagar»: a mano, sin contar como fallo. */
int32_t acel2d_apagar(void)
{
    if (!ativo) return 0;
    ativo = false;
    apagado_a_mano = true;
    if (vg_gart && !vg_sim) {
        if (rt_estado == VOLCADO_ACTIVO) retorno_soltar(VOLCADO_A_MANO, "apagado a mano con el 2D (acel2d apagar)");
        volcado_soltar(VOLCADO_A_MANO, "apagado a mano con el 2D (acel2d apagar)");
    }
    soltar();
    snformatar(motivo, sizeof motivo, "%s: apagado a mano (acel2d apagar), sigue la CPU", placa ? placa->nome : "?");
    klog("[ACEL2D] apagado: a mano (acel2d apagar)\n");
    video_mostrar_tudo();
    return 0;
}

/* «acel2d encender»: repite la elección y la autoprueba del arranque. */
int32_t acel2d_acender(void)
{
    if (ativo) return 0;
    klog("[ACEL2D] encender a mano: se repite la autoprueba\n");
    placa = NULL; drv = NULL; iniciado = false;
    acel2d_iniciar();
    if (ativo && !vg_sim && vg_estado != VOLCADO_ACTIVO && rt_estado != VOLCADO_ACTIVO) acel2d_volcado_iniciar();   /* D387, D391 */
    return ativo ? 0 : ERRO_INVALIDO;
}

/* ------------------------------------------------------------------ las órdenes */

/* Recorta origen y destino juntos a la pantalla visible. */
static bool recortar(int *ox, int *oy, int *dx, int *dy, int *l, int *a)
{
    int d;
    if (*ox < 0) { d = -*ox; *ox += d; *dx += d; *l -= d; }
    if (*oy < 0) { d = -*oy; *oy += d; *dy += d; *a -= d; }
    if (*dx < 0) { d = -*dx; *ox += d; *dx += d; *l -= d; }
    if (*dy < 0) { d = -*dy; *oy += d; *dy += d; *a -= d; }
    if (*ox + *l > tela.larg) *l = tela.larg - *ox;
    if (*dx + *l > tela.larg) *l = tela.larg - *dx;
    if (*oy + *a > tela.alt) *a = tela.alt - *oy;
    if (*dy + *a > tela.alt) *a = tela.alt - *dy;
    return *l > 0 && *a > 0;
}

/* Las órdenes se escriben con las interrupciones cerradas: la IRQ del ratón
 * también llama a video_mostrar (y este a acel_atualizar), y dos órdenes a medio
 * escribir en la FIFO de la VMware a la vez la estropearían. */
bool acel_copiar(int ox, int oy, int dx, int dy, int l, int a)
{
    if (!ativo) return false;
    if (!recortar(&ox, &oy, &dx, &dy, &l, &a)) return true;
    /* D246: una copia pequeña sale más cara por el motor (paquete, valla, espera) que por la CPU:
     * se devuelve false sin apagar nada y quien llama la hace con el búfer de atrás. */
    if (drv->min_pixeis && (uint32_t)l * (uint32_t)a < drv->min_pixeis) return false;
    bool sim = int_salva_desliga();
    if (vg_pendente && vg_sim) sim_executar();   /* D387: el motor hace las órdenes en orden */
    vaciar_wc();
    bool ok = drv->copiar(ox, oy, dx, dy, l, a);
    int_restaura(sim);
    if (!ok) { acel2d_desligar("la GPU rechazo una copia"); return false; }
    n_copias++;
    gpu_px_resto_c += (uint32_t)l * (uint32_t)a; gpu_kpx_copia += gpu_px_resto_c / 1000; gpu_px_resto_c %= 1000;
    return true;
}

bool acel_rellenar(int x, int y, int l, int a, uint32_t cor)
{
    if (!ativo) return false;
    int dx = x, dy = y;
    if (!recortar(&x, &y, &dx, &dy, &l, &a)) return true;
    bool sim = int_salva_desliga();
    if (vg_pendente && vg_sim) sim_executar();   /* D387 */
    vaciar_wc();
    bool ok = drv->preencher(x, y, l, a, cor_placa(cor));
    int_restaura(sim);
    if (!ok) { acel2d_desligar("la GPU rechazo un relleno"); return false; }
    n_rellenos++;
    gpu_px_resto_r += (uint32_t)l * (uint32_t)a; gpu_kpx_relleno += gpu_px_resto_r / 1000; gpu_px_resto_r %= 1000;
    return true;
}

bool acel_esperar(void)
{
    if (!ativo) return false;
    bool sim = int_salva_desliga();
    bool ok = drv->esperar();
    int_restaura(sim);
    if (ok) return true;
    acel2d_desligar("tiempo agotado esperando a la GPU");
    return false;
}

/* D246: la CPU va a escribir en la pantalla: si el motor sigue copiando, se espera antes (en la
 * práctica ya se espera tras cada orden; esto lo garantiza para cualquier camino nuevo). */
void acel_sincronizar(void)
{
    if (vg_pendente) {   /* D387: un volcado por la GPU sin terminar */
        bool sim = int_salva_desliga();
        if (!vg_sim) rastro_marca(0x0300u);   /* dbg5: B, esperando a la GPU */
        if (vmarca_ocupado_ok()) vmarca_cuadro(VMARCA_B_X, VMARCA_EST_Y, 0x00FF0000u, true);   /* B: esperando a la GPU */
        bool ok = vops && vops->esperar(VOLCADO_PRAZO_MS);
        if (!vg_sim) rastro_marca(0x0301u);
        vmarca_cuadro(VMARCA_B_X, VMARCA_EST_Y, 0x0000FF00u, true);   /* B: fuera */
        vg_pendente = false;
        int_restaura(sim);
        if (!ok) volcado_falhou("tiempo agotado esperando un volcado de la GPU");
    }
    if (!ativo || !drv->assincrono) return;
    (void)acel_esperar();
}

void acel_atualizar(int x, int y, int l, int a)
{
    if (!ativo || !drv->atualizar) return;
    int dx = x, dy = y;
    if (!recortar(&x, &y, &dx, &dy, &l, &a)) return;
    bool sim = int_salva_desliga();
    bool ok = drv->atualizar(x, y, l, a);
    int_restaura(sim);
    /* Una tarjeta que solo enseña lo que se le pide y no lo acepta dejaría la
     * pantalla congelada: se apaga y vuelve a enseñar el framebuffer del VBE. */
    if (!ok) acel2d_desligar("la GPU no acepta la orden de mostrar");
}

/* ------------------------------------------------------------------ infopc y la orden */

void acel2d_contar_arrasto(uint32_t quadros, uint64_t ciclos)
{
    if (!quadros) return;
    n_arrastos++;
    n_quadros += quadros;
    ciclos_arrasto += ciclos;
}

int32_t acel2d_info(uint32_t ptr, uint32_t n)
{
    info_acel2d_t *v = (info_acel2d_t *)ptr;
    /* D387: los programas de antes piden el tamaño sin los campos del volcado. */
    if (n < INFO_ACEL2D_TAM_V1 || !usuario_ok((void *)ptr, n < sizeof *v ? n : sizeof *v, true)) return ERRO_ENDERECO;
    /* D391: y los del 8.5.4.10r2 sin la vuelta; D387: y los de antes de la memoria de vídeo real. */
    bool com_volcado = n >= INFO_ACEL2D_TAM_V2, com_retorno = n >= INFO_ACEL2D_TAM_V3, com_vram = n >= sizeof *v;
    memset(v, 0, com_vram ? sizeof *v : com_retorno ? INFO_ACEL2D_TAM_V3 : com_volcado ? INFO_ACEL2D_TAM_V2 : INFO_ACEL2D_TAM_V1);
    v->versao = 1;
    v->ativo = ativo;
    v->modo = modo;
    v->fabricante = placa ? disp.fabricante : 0;
    v->modelo = placa ? disp.modelo : 0;
    txt_copia_n(v->nome, acel2d_nome(), sizeof v->nome);
    txt_copia_n(v->motivo, motivo, sizeof v->motivo);
    if (placa) txt_copia_n(v->placa, placa->nome, sizeof v->placa);   /* issue #43: infopc nombra la tarjeta con esta tabla */
    v->copias = n_copias; v->rellenos = n_rellenos; v->falhas = n_falhas;
    v->arrastos = n_arrastos; v->quadros = n_quadros;
    v->kciclos_quadro = n_quadros ? (uint32_t)sconv_div64(ciclos_arrasto, n_quadros * 1000u) : 0;
    v->us_autoprova = us_autoprova; v->us_gpu = us_gpu_boot; v->us_cpu = us_cpu_boot;
    v->confirmada = confirmada;
    v->apagado = !ativo && (n_falhas || apagado_a_mano) ? (apagado_a_mano ? 2u : 1u) : 0;
    v->prazo_ms = PRAZO_SESION_MS;
    v->kpx_gpu_copias = gpu_kpx_copia; v->kpx_gpu_rellenos = gpu_kpx_relleno;
    for (int i = 0; i < ACEL_SW_N; i++) { v->sw_ops[i] = sw_ops[i]; v->sw_kpx[i] = sw_kpx[i]; }
    if (com_volcado) {
        v->volcado_estado = (unsigned)vg_estado;
        v->volcado_ops = vg_ops; v->volcado_kpx = vg_kpx; v->volcado_cpu_ops = vg_cpu_ops;
        v->volcado_us_gpu = vg_us_gpu; v->volcado_us_cpu = vg_us_cpu;
        v->volcado_simulado = vg_sim;
        txt_copia_n(v->volcado_motivo, vg_motivo, sizeof v->volcado_motivo);
    }
    if (com_retorno) {
        v->retorno_estado = (unsigned)rt_estado;
        v->retorno_ops = rt_ops; v->retorno_kpx = rt_kpx; v->retorno_cpu_ops = rt_cpu_ops;
        v->retorno_us_gpu = rt_us_gpu; v->retorno_us_cpu = rt_us_cpu;
        v->retorno_mascara = rt_mascara;
        txt_copia_n(v->retorno_motivo, rt_motivo, sizeof v->retorno_motivo);
    }
    if (com_vram) {
        v->vram_kb = vram_real_kb;
        txt_copia_n(v->vram_fonte, vram_real_fonte, sizeof v->vram_fonte);
    }
    return 0;
}

/* D399 (gpu_energia.c): lo que el motor 2D ha hecho desde el arranque (órdenes, volcados y vueltas),
 * para el gobernador «auto»; y la autoprueba corta del arranque después de cambiar los relojes de la
 * tarjeta. 0: no hay 2D por la GPU (nada que probar); 1: igual que la CPU; -1: falló (no apaga nada:
 * decide quien llama, que antes vuelve a los relojes de la BIOS). */
uint32_t acel2d_ordens_total(void) { return n_copias + n_rellenos + vg_ops + rt_ops; }
bool acel2d_volcado_em_uso(void) { return vg_estado == VOLCADO_ACTIVO && !vg_sim; }
int acel2d_energia_autoprova(char *m, int nm)
{
    if (!ativo || !drv || !zona) return 0;
    acel_sincronizar();
    return autoprova(false, false, NULL, m, nm) ? 1 : -1;
}

/* D246: «acel2d probar --detalle». Órdenes sueltas de varios tamaños por la GPU y por la CPU, con el
 * tiempo de cada una (poner en cola, esperar) y los registros del motor antes y después, para que
 * una foto del log [ACEL2D] (o «dmesg > /home/acel.txt») diga dónde falla o cuándo no compensa. */
static void detalhar(void)
{
    uint32_t khz = hw_tsc_khz();
    int L = tela.larg, A = ZONA_A;
    uint32_t *ref = kmalloc((uint32_t)(L * A) * 4);
    if (!ref || !khz) { klog("[ACEL2D] detalle: sin memoria o sin TSC medido\n"); if (ref) kfree(ref); return; }
    zona_padrao(ref, L, A);
    static const struct { int l, a; } t[] = {
        { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 }, { 128, 32 }, { 256, 16 },
        { 0, 1 }, { 0, 8 }, { 0, 32 }, { 0, 56 },   /* l = 0: ancho completo */
    };
    klog("[ACEL2D] detalle: %s, pantalla %dx%d pitch %u, zona en la fila %d; TSC %u kHz\n", drv->nome, L, A, tela.pitch, zona_y, khz);
    if (drv->detalle) drv->detalle("antes de la primera orden");
    bool sse = rapido_inicio();
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        int l = t[i].l ? t[i].l : L, a = t[i].a, ox = 0, oy = 0, dx, dy;
        if (t[i].l) { dx = L - l; dy = 0; if (dx < l) continue; }
        else { dx = 0; dy = a <= ZONA_A / 2 ? a : 2; }   /* ancho completo: sin solape si cabe, si no 2 filas */
        if (dy + a > A) continue;
        uint64_t t0 = tsc();
        bool ok = drv->copiar(ox, zona_y + oy, dx, zona_y + dy, l, a);
        uint64_t t1 = tsc();
        if (ok) ok = drv->esperar();
        uint64_t t2 = tsc();
        executar_cpu(ref, L, &(ordem_t){ 0, ox, oy, dx, dy, l, a, 0 });
        for (int j = 0; j < a; j++)
            rapido_copiar_tela(zona_px(dx, dy + j), ref + (dy + j) * L + dx, (uint32_t)(l * tela.bytes_px));
        uint64_t t3 = tsc();
        klog("[ACEL2D] detalle: copia %dx%d%s (%u px)%s: GPU %u ns (cola %u + espera %u), CPU %u ns\n", l, a,
             t[i].l ? "" : " ancho completo", (uint32_t)(l * a), ok ? "" : " FALLO", (uint32_t)sconv_div64((t2 - t0) * 1000000, khz),
             (uint32_t)sconv_div64((t1 - t0) * 1000000, khz), (uint32_t)sconv_div64((t2 - t1) * 1000000, khz),
             (uint32_t)sconv_div64((t3 - t2) * 1000000, khz));
        if (i == 0 && drv->detalle) drv->detalle("despues de la primera orden");
        if (!ok) { if (drv->detalle) drv->detalle("tras el fallo"); break; }
    }
    rapido_fim(sse);
    vaciar_wc();
    if (drv->detalle) drv->detalle("al final");
    kfree(ref);
}

/* «acel2d probar»: la autoprueba completa (con los bordes y muchas órdenes
 * aleatorias) y la medida, ahora. Si el acelerador quedó apagado en el
 * arranque, se vuelve a iniciar solo para la prueba (para mirarlo en el
 * hardware) y se apaga otra vez: no se activa desde aquí. */
int32_t acel2d_prova(uint32_t ptr, uint32_t n)
{
    info_acel2d_prova_t *r = (info_acel2d_prova_t *)ptr;
    if (n < sizeof *r || !usuario_ok((void *)ptr, sizeof *r, true)) return ERRO_ENDERECO;
    bool com_detalhe = r->versao == 2;   /* D246: «--detalle» (el programa pone 2 antes de llamar) */
    memset(r, 0, sizeof *r);
    r->versao = 1;
    if (!drv) {
        r->resultado = -1;
        snformatar(r->texto, sizeof r->texto, "sin acelerador: %s", motivo);
        return 0;
    }
    bool sim = int_salva_desliga();
    char m[128] = "";   /* D357: cabe «etapa N:» delante del motivo del driver */
    bool temporal = !iniciado;
    if (temporal) cmd_original = pci_le32(&disp, 0x04);
    if (temporal && !drv->iniciar(&disp, placa->geracao, &tela, m, sizeof m)) {
        soltar();
        int_restaura(sim);
        r->resultado = 0;
        snformatar(r->texto, sizeof r->texto, "%s no se pudo iniciar: %s", drv->nome, m);
        klog("[ACEL2D] prueba: %s\n", r->texto);
        return 0;
    }
    if (temporal) iniciado = true;
    bool ok = preparar_zona() && autoprova(true, false, r, m, sizeof m);
    if (ok) { medir(r); medir_tandas(r); }
    if (com_detalhe) { if (ok) detalhar(); else if (drv->detalle) drv->detalle("tras el fallo de la autoprueba"); }
    if (ok && !temporal) volcado_probar(com_detalhe);   /* D387 */
    zona_restaurar(ZONA_A);
    if (temporal) soltar();
    int_restaura(sim);
    if (temporal) video_mostrar_tudo();
    r->resultado = ok ? 1 : 0;
    if (ok)
        snformatar(r->texto, sizeof r->texto, "%s: %u ordenes, los mismos pixeles que la CPU; %d copias: GPU %u us, CPU %u us",
                   drv->nome, r->ordens, VOLTAS_BENCH, r->us_gpu, r->us_cpu);
    else
        snformatar(r->texto, sizeof r->texto, "%s: FALLO: %s", drv->nome, m);
    klog("[ACEL2D] prueba: %s\n", r->texto);
    return 0;
}

/* ================================================================== D387: volcado por la GPU
 *
 * Lo que más pesa del escritorio en una máquina antigua es copiar el búfer de atrás (RAM) a la
 * pantalla (video_mostrar): cada píxel cruza el bus hacia la tarjeta escrito por la CPU. Con un
 * driver que sepa dar a la GPU acceso a la RAM del sistema (GART de las Radeon R5xx), esa copia la
 * hace el motor 2D de la tarjeta leyendo el búfer, y la CPU queda libre mientras tanto.
 *
 * Reglas (las mismas del 2D, D195/D244):
 *   - solo se enciende si su autoprueba da los mismos píxeles que la CPU (una región de prueba con
 *     un dibujo conocido y unas filas del búfer de atrás, copiadas a la zona de prueba de la VRAM)
 *     y no es más del doble de lenta que la CPU (a mano, «acel2d volcado encender», aunque lo sea);
 *   - la GPU copia sola: antes de que la CPU escriba en la pantalla o mueva el búfer de atrás se
 *     espera (acel_sincronizar), con un plazo; si se agota, el vigilante apaga el 2D y el volcado y
 *     la CPU redibuja todo;
 *   - solo con una pantalla de 32 bits 0x00RRGGBB; los rectángulos pequeños, la CPU;
 *   - se apaga con «kconfig volcado_gpu=no» (vale al reiniciar) o «acel2d volcado apagar».
 *
 * Para las pruebas en QEMU (que no emula ninguna GPU capaz de esto) hay una «GPU» de software
 * (--volcado-gpu-simulado): la misma tabla de páginas que la GART de la R5xx y las mismas franjas
 * (volcado_logica.h), copiando con retraso, cuando el motor de verdad lo haría (en la siguiente
 * orden o al esperar): así un camino que olvide esperar se ve en la pantalla. */

/* ------------------------------------------------------------ la «GPU» de software */

#define SIM_FILA 16
static struct { uint32_t *tabela; uint32_t npag; volcado_fonte_t f; } sim_reg[2];
static struct { int reg, x, y, l, a, dx, dy; } sim_fila[SIM_FILA];
static int sim_n;
static uint32_t sim_volcados;

static bool sim_preparar(char *m, int nm)
{
    (void)m; (void)nm;
    klog("[ACEL2D] volcado simulado: GART de software (pruebas)\n");
    return true;
}

static bool sim_mapear(int reg, const void *buf, uint32_t passo, uint32_t larg, uint32_t alt, char *m, int nm)
{
    if (!buf) {   /* revisión: la región sin uso */
        if (sim_reg[reg].tabela) kfree(sim_reg[reg].tabela);
        sim_reg[reg].tabela = NULL; sim_reg[reg].npag = 0;
        return true;
    }
    uint32_t v = (uint32_t)buf, desl = v & 0xFFFu;
    uint32_t np = volcado_paginas(desl, passo, alt);
    if (sim_reg[reg].tabela) kfree(sim_reg[reg].tabela);
    sim_reg[reg].tabela = kmalloc(np * 4);
    if (!sim_reg[reg].tabela) { snformatar(m, (size_t)nm, "sin memoria para la tabla"); return false; }
    for (uint32_t i = 0; i < np; i++) {
        uint32_t fl;
        fis_t f = espaco_traduzir(espaco_kernel(), (v & ~0xFFFu) + i * 4096u, &fl);
        if (!f || !gart_r300_pte(f & ~0xFFFull, GART_R300_LEITURA, &sim_reg[reg].tabela[i])) {
            snformatar(m, (size_t)nm, "la pagina %u del bufer no se puede apuntar", i);
            return false;
        }
    }
    sim_reg[reg].npag = np;
    sim_reg[reg].f = (volcado_fonte_t){ reg ? 0x20000000u : 0x10000000u, desl, passo, larg, alt };
    const char *porque;
    if (!volcado_fonte_ok(&sim_reg[reg].f, &porque)) { snformatar(m, (size_t)nm, "%s", porque); return false; }
    return true;
}

/* Lo que haría el motor con una franja: la dirección de cada fila, traducida por la tabla, página a página. */
static void sim_faixa(int reg, const volcado_faixa_t *fx)
{
    uint8_t *placa = video_placa();
    uint32_t pitch = video_pitch(), passo = sim_reg[reg].f.passo, g0 = sim_reg[reg].f.gtt_base;
    for (int j = 0; j < fx->a; j++) {
        uint32_t mc = fx->base + (uint32_t)(fx->sy + j) * passo + (uint32_t)fx->sx * 4u;
        uint32_t *d = (uint32_t *)(placa + (uint32_t)(fx->dy + j) * pitch) + fx->dx;
        uint32_t resta = (uint32_t)fx->l * 4u;
        while (resta) {
            uint32_t idx = (mc - g0) >> 12, dentro = 4096u - (mc & 0xFFFu), n = resta < dentro ? resta : dentro;
            if (idx >= sim_reg[reg].npag) return;   /* fuera de la ventana: la GART lo descartaría */
            const uint8_t *o = (const uint8_t *)(uint32_t)gart_r300_fis(sim_reg[reg].tabela[idx]) + (mc & 0xFFFu);
            rapido_copiar_tela(d, o, n);
            d += n / 4; mc += n; resta -= n;
        }
    }
}

/* D391: la región de escritura (2) de la simulada: entradas de la R5xx con ESCRITA solo mientras dura
 * una vuelta, y las vueltas pendientes (se hacen al esperar, como el motor de verdad). Una escritura
 * que caiga fuera de la región o en una entrada sin ESCRITA se pierde y se cuenta: la autoprueba y
 * h394 lo miran (una GART de verdad la descartaría o la mandaría a la página de relleno). */
#define SIM_GTT2 0x30000000u
static uint32_t *sim_tab2, sim_npag2, sim_perdidas;
static struct { acel_origem_t o; int l, a; retorno_destino_t d; bool mascara; } sim_rt[4];
static int sim_nrt;

static bool sim_mapear_escrita(const uint64_t *pag, uint32_t npag, char *m, int nm)
{
    if (!npag) {   /* todo a solo lectura otra vez */
        for (uint32_t i = 0; i < sim_npag2; i++) sim_tab2[i] &= ~GART_R300_ESCRITA;
        return true;
    }
    if (npag > sim_npag2 || !sim_tab2) {
        if (sim_tab2) kfree(sim_tab2);
        sim_tab2 = kmalloc(npag * 4);
        sim_npag2 = sim_tab2 ? npag : 0;
        if (!sim_tab2) { snformatar(m, (size_t)nm, "sin memoria para la tabla de escritura"); return false; }
    }
    for (uint32_t i = 0; i < sim_npag2; i++) sim_tab2[i] = 0;
    for (uint32_t i = 0; i < npag; i++) {
        if (!gart_r300_pte(pag[i], GART_R300_LEITURA | GART_R300_ESCRITA, &sim_tab2[i])) {
            for (uint32_t k = 0; k < sim_npag2; k++) sim_tab2[k] = 0;
            snformatar(m, (size_t)nm, "la pagina %u (fisica %x:%x) no la alcanza la GPU simulada", i, (uint32_t)(pag[i] >> 32), (uint32_t)pag[i]);
            return false;
        }
    }
    return true;
}

static bool sim_retornar(const acel_origem_t *o, int l, int a, uint32_t desl, uint32_t passo, bool mascara)
{
    if (!sim_tab2 || !o->cpu) return false;
    if (sim_nrt == 4) sim_executar();
    sim_rt[sim_nrt].o = *o; sim_rt[sim_nrt].l = l; sim_rt[sim_nrt].a = a;
    sim_rt[sim_nrt].d = (retorno_destino_t){ SIM_GTT2, desl, passo };
    sim_rt[sim_nrt].mascara = mascara;
    sim_nrt++;
    return true;
}

/* Lo que haría el motor con las vueltas pendientes: cada fila de cada franja, traducida por la tabla. */
static void sim_executar_retornos(void)
{
    for (int i = 0; i < sim_nrt; i++) {
        const retorno_destino_t *d = &sim_rt[i].d;
        int l = sim_rt[i].l, a = sim_rt[i].a, n = retorno_n_faixas(d, a, 128);
        for (int k = 0; k < n; k++) {
            volcado_faixa_t f;
            if (!retorno_faixa(d, l, a, 128, k, &f)) { sim_perdidas += (uint32_t)l; continue; }
            uint32_t p = (f.pitch_offset >> 22) * 64u;
            for (int j = 0; j < f.a; j++) {
                const uint32_t *src = (const uint32_t *)((const uint8_t *)sim_rt[i].o.cpu + (uint32_t)(f.sy + j) * sim_rt[i].o.passo);
                uint32_t mc = f.base + (uint32_t)j * p + (uint32_t)f.dx * 4u;
                for (int x = 0; x < l; ) {
                    /* Lo que queda de la fila dentro de esta página, por la tabla (la página puede ser alta: ventana temporal). */
                    uint32_t idx = (mc - SIM_GTT2) >> 12, n = (4096u - (mc & 0xFFFu)) / 4u;
                    if (n > (uint32_t)(l - x)) n = (uint32_t)(l - x);
                    if (mc < SIM_GTT2 || idx >= sim_npag2 || !(sim_tab2[idx] & GART_R300_ESCRITA)) sim_perdidas += n;
                    else {
                        uint32_t *d = fisica_mapear(gart_r300_fis(sim_tab2[idx]) + (mc & 0xFFFu));
                        for (uint32_t k = 0; k < n; k++) d[k] = sim_rt[i].mascara ? src[x + (int)k] & 0x00FFFFFFu : src[x + (int)k];
                        fisica_soltar(d);
                    }
                    x += (int)n; mc += n * 4u;
                }
            }
        }
    }
    sim_nrt = 0;
}

static void sim_executar(void)
{
    static volcado_faixa_t fx[64];
    sim_executar_retornos();
    bool sse = rapido_inicio();
    for (int i = 0; i < sim_n; i++) {
        int r = sim_fila[i].reg;
        /* La franja se calcula sobre el origen; el destino se desplaza aparte. */
        int n = volcado_trocear(&sim_reg[r].f, sim_fila[i].x, sim_fila[i].y, sim_fila[i].l, sim_fila[i].a, 128, fx, 64);
        for (int k = 0; k < n; k++) {
            fx[k].dx += sim_fila[i].dx - sim_fila[i].x;
            fx[k].dy += sim_fila[i].dy - sim_fila[i].y;
            sim_faixa(r, &fx[k]);
        }
        if (ativo && drv->atualizar) drv->atualizar(sim_fila[i].dx, sim_fila[i].dy, sim_fila[i].l, sim_fila[i].a);
    }
    rapido_fim(sse);
    sim_n = 0;
}

static bool sim_volcar(int reg, int x, int y, int l, int a, int dx, int dy)
{
    if (!sim_reg[reg].tabela) return false;
    if (sim_n == SIM_FILA) sim_executar();
    sim_fila[sim_n].reg = reg; sim_fila[sim_n].x = x; sim_fila[sim_n].y = y; sim_fila[sim_n].l = l; sim_fila[sim_n].a = a;
    sim_fila[sim_n].dx = dx; sim_fila[sim_n].dy = dy;
    sim_n++;
    sim_volcados++;
    return true;
}

static bool sim_esperar(uint32_t ms)
{
    (void)ms;
    bool havia_retorno = sim_nrt > 0;
    sim_executar();
    /* --volcado-gpu-prueba=vuelta-cuelgue (D391): a la 4.ª vuelta de la sesión la «GPU» deja de contestar. */
    if ((kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_RETORNO_CUELGUE) && havia_retorno && rt_estado == VOLCADO_ACTIVO && rt_ops >= 3) return false;
    /* --volcado-gpu-prueba=cuelgue: a los 8 volcados de la sesión la «GPU» deja de contestar. */
    if ((kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_VOLCADO_CUELGUE) && vg_estado == VOLCADO_ACTIVO && vg_ops >= 8) return false;
    return true;
}

static void sim_soltar(void)
{
    sim_n = 0; sim_nrt = 0;
    if (sim_tab2) kfree(sim_tab2);
    sim_tab2 = NULL; sim_npag2 = 0;
    for (int r = 0; r < 2; r++) { if (sim_reg[r].tabela) kfree(sim_reg[r].tabela); sim_reg[r].tabela = NULL; sim_reg[r].npag = 0; }
}

static void sim_detalle(const char *fase)
{
    klog("[ACEL2D] volcado simulado (%s): %u volcados, region 0 %u paginas, region 1 %u paginas; region 2 (escritura) %u paginas, "
         "%u escrituras perdidas\n", fase, sim_volcados, sim_reg[0].npag, sim_reg[1].npag, sim_npag2, sim_perdidas);
}

static const acel_volcado_t volcado_sim = { sim_preparar, sim_mapear, sim_volcar, sim_esperar, sim_soltar, sim_detalle,
                                            sim_mapear_escrita, sim_retornar };

/* ------------------------------------------------------------ autoprueba y medida */

/* La zona de prueba de la VRAM: la del 2D (debajo de la pantalla visible si cabe) o, sin acelerador
 * (solo con la simulada), las últimas filas visibles, que después se redibujan. */
static uint8_t *vz;
static int vz_y;
static bool vz_preparar(void)
{
    if (ativo && zona) { vz = zona; vz_y = zona_y; return true; }
    vz_y = (int)video_altura() - VOLCADO_PROVA_A;
    vz = video_placa() + video_pitch() * (uint32_t)vz_y;
    return vz_y > 0;
}
static inline uint32_t *vz_px(int x, int y) { return (uint32_t *)(vz + (uint32_t)y * video_pitch()) + x; }
static inline uint32_t prova_px(int x, int y) { return ((uint32_t)x * 2654435761u ^ (uint32_t)y * 40503u ^ 0x00A5C3u) & 0x00FFFFFFu; }

/* true si la GPU puso los mismos píxeles que la CPU. us_g/us_c: la misma copia de la región de
 * prueba hecha VOLTAS_VOLCADO veces por la GPU (con la espera) y por la CPU. */
/* ------------------------------------------------------------ depuración del Amilo (rama v8.5.4.10r2-amilo-dbg2)
 *
 * Marcas de progreso del volcado pintadas por la CPU en la esquina de arriba a la izquierda de la pantalla
 * visible, para que se vean en una foto aunque el equipo se cuelgue (sin depender del log). Cuadrados de
 * 10x10 en filas de 20: blancos, y naranja cada 5.º (para contar). Fila 4 (más abajo), tres cuadrados de
 * estado del volcado en marcha: A «dentro de vops->volcar» (rojo) / fuera (verde), B «esperando a la GPU en
 * acel_sincronizar» (rojo) / fuera (verde), C cambia de color en cada volcado (azul, cian, magenta, amarillo).
 * Mientras dura la autoprueba solo se pintan en la pantalla (el búfer de atrás se compara con la VRAM); antes
 * de video_mostrar_tudo se copian también al búfer de atrás, y desde entonces van a los dos. */
#define VMARCA_SI 1
static bool gart_decidindo;
static uint32_t vmarcas;   /* bit k-1: la marca k ya salió */
static void vmarca_cuadro(int x0, int y0, uint32_t cor, bool tras)
{
    uint8_t *t = video_placa();
    uint32_t *b = video_buffer(), p = video_pitch(), L = video_largura();
    if (!VMARCA_SI || vg_sim || !t || !video_formato_direto()) return;   /* solo con una GPU de verdad (h394 compara píxeles) */
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            ((volatile uint32_t *)(t + (uint32_t)(y0 + y) * p))[x0 + x] = cor;
            if (tras && b) b[(uint32_t)(y0 + y) * L + (uint32_t)(x0 + x)] = cor;
        }
    vaciar_wc();
}
/* dbg5: ¿se puede pintar una marca en la pantalla con el motor (quizá) ocupado? Solo con la opción MARCAS
 * («acel2d volcado paso 25»): hasta saber si la CPU escribiendo en la VRAM mientras el motor lee la RAM por la
 * GART tiene que ver con el cuelgue, esas marcas van solo al rastro. */
static bool vmarca_ocupado_ok(void) { return acel_radeon_dbg_marcas(); }

/* dbg5: una marca de los experimentos («acel2d volcado paso 7-17»), pintada por quien sabe que el motor está parado. */
void acel_marca_paso(int fila, int col, int estado, bool tras)
{
    vmarca_cuadro(2 + col * 12, 2 + fila * 12, estado == 0 ? 0x00FFFF00u : estado == 1 ? 0x00FFFFFFu : 0x00FF0000u, tras);
}

void acel_marca_fina(int fila, int col, int estado)
{
    /* Al búfer de atrás también solo con el volcado ya activo y fuera de la autoprueba (que lo compara con la VRAM). */
    bool tras = vg_estado == VOLCADO_ACTIVO && !gart_decidindo;
    if (!vg_sim) rastro_marca(0x10000u | ((uint32_t)fila << 12) | ((uint32_t)col << 4) | (uint32_t)estado);
    if (fila == 4 && !vmarca_ocupado_ok()) return;   /* las de gv_volcar: el motor está con las franjas de antes */
    vmarca_cuadro(2 + col * 12, 2 + fila * 12, estado == 0 ? 0x00FFFF00u : estado == 1 ? 0x00FFFFFFu : 0x00FF0000u, tras);
}
static void vmarca_pos(int k, int *x, int *y) { *x = 2 + ((k - 1) % 20) * 12; *y = 2 + ((k - 1) / 20) * 12; }
static void vmarca(int k, bool tras)
{
    int x, y;
    if (k < 1 || k > 32) return;
    if (!vg_sim) rastro_marca(0x0100u | (uint32_t)k);
    vmarcas |= 1u << (k - 1);
    /* dbg5: la 7, la 10 y la 13 salen con órdenes puestas y sin esperar (el motor ocupado): solo al rastro. */
    if ((k == 7 || k == 10 || k == 13) && !tras && !vmarca_ocupado_ok()) return;
    vmarca_pos(k, &x, &y);
    vmarca_cuadro(x, y, k % 5 == 0 ? 0x00FF8000u : 0x00FFFFFFu, tras);
}
static void vmarcas_limpar(void)
{
    vmarcas = 0;
    for (int k = 1; k <= 40; k++) { int x, y; vmarca_pos(k, &x, &y); vmarca_cuadro(x, y, 0x00000000u, false); }
}
static void vmarcas_ao_tras(void)
{
    for (int k = 1; k <= 32; k++) if (vmarcas & (1u << (k - 1))) vmarca(k, true);
}

#define VOLTAS_VOLCADO 8
static bool volcado_autoprova(char *m, int nm, uint32_t *us_g, uint32_t *us_c)
{
    int L = (int)video_largura(), A = VOLCADO_PROVA_A;
    uint32_t khz = hw_tsc_khz();
    if (!vz_preparar()) { snformatar(m, (size_t)nm, "sin zona de prueba"); return false; }
    uint32_t *p = kmalloc((uint32_t)(L * A) * 4), *ref = kmalloc((uint32_t)(L * A) * 4);
    if (!p || !ref) { if (p) kfree(p); if (ref) kfree(ref); snformatar(m, (size_t)nm, "sin memoria para la autoprueba"); return false; }
    vmarca(4, false);   /* autoprueba: memoria reservada */
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            p[y * L + x] = prova_px(x, y);
            ref[y * L + x] = 0x00102030u ^ (uint32_t)(x * 7 + y);
            *vz_px(x, y) = ref[y * L + x];
        }
    vaciar_wc();
    vmarca(5, false);   /* zona de prueba rellenada por la CPU */
    bool ok = vops->mapear(1, p, (uint32_t)L * 4u, (uint32_t)L, (uint32_t)A, m, nm);
    vmarca(6, false);   /* región 1 apuntada */
    /* Rectángulos: filas enteras, sueltos y desplazados, el primer y el último píxel, bordes de página. */
    static const struct { int x, y, l, a, dx, dy; } rs[] = {
        { 0, 0, 0, 6, 0, 0 },          /* l = 0: todo el ancho */
        { 5, 8, 100, 20, 200, 10 },
        { -64, 30, 64, 18, 0, 30 },    /* x < 0: desde el borde derecho */
        { 0, 47, 1, 1, -1, 0 },        /* dx < 0: al último píxel de la fila */
        { 1000, 6, 48, 2, 1000, 6 },   /* cruza el final de una fila de 4 KB (1024 de ancho) */
        { 3, 40, 300, 7, 3, 40 },
    };
    for (unsigned i = 0; ok && i < sizeof rs / sizeof rs[0]; i++) {
        int l = rs[i].l ? rs[i].l : L, a = rs[i].a;
        int x = rs[i].x < 0 ? L + rs[i].x : rs[i].x, y = rs[i].y, dx = rs[i].dx < 0 ? L + rs[i].dx : rs[i].dx, dy = rs[i].dy;
        if (x + l > L) l = L - x;
        if (dx + l > L) l = L - dx;
        if (l <= 0 || y + a > A || dy + a > A) continue;
        if (!vops->volcar(1, x, y, l, a, dx, vz_y + dy)) { snformatar(m, (size_t)nm, "la GPU rechazo el volcado %u", i); ok = false; break; }
        for (int j = 0; j < a; j++)
            for (int k = 0; k < l; k++) ref[(dy + j) * L + dx + k] = p[(y + j) * L + x + k];
    }
    vmarca(7, false);   /* los 6 rectángulos puestos al motor */
    if (ok && !vops->esperar(PRAZO_MS)) { snformatar(m, (size_t)nm, "tiempo agotado en la autoprueba"); ok = false; }
    vmarca(8, false);   /* espera terminada */
    if (ok && (kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_VOLCADO_FALLO)) *vz_px(L / 3, A / 2) ^= 0x00FF00u;
    uint32_t dif = 0;
    for (int y = 0; ok && y < A; y++)
        for (int x = 0; x < L; x++)
            if ((*vz_px(x, y) & 0x00FFFFFFu) != ref[y * L + x]) {
                if (!dif) {
                    klog("[ACEL2D] volcado: primer pixel distinto en %d,%d: gpu %x cpu %x\n", x, y, *vz_px(x, y), ref[y * L + x]);
                    snformatar(m, (size_t)nm, "pixeles distintos de la CPU (el primero en %d,%d)%s", x, y,
                               (kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_VOLCADO_FALLO) ? " (simulado)" : "");
                }
                dif++;
            }
    if (dif) ok = false;
    vmarca(9, false);   /* comparada la región 1 */
    if (!ok) vmarca(31, false);   /* (31: la autoprueba ya ha fallado) */
    /* El búfer de atrás de verdad (región 0): sus primeras filas a la zona, comparadas con él. */
    if (ok) {
        const uint32_t *tras = video_buffer();
        int a = A < (int)video_altura() ? A : (int)video_altura();
        bool v0 = vops->volcar(0, 0, 0, L, a, 0, vz_y);
        vmarca(10, false);   /* región 0 (búfer de atrás): copia puesta */
        if (!v0 || !vops->esperar(PRAZO_MS)) { snformatar(m, (size_t)nm, "fallo al volcar el bufer de atras"); ok = false; }
        vmarca(11, false);   /* región 0: espera terminada */
        for (int y = 0; ok && y < a; y++)
            for (int x = 0; x < L; x++)
                if ((*vz_px(x, y) & 0x00FFFFFFu) != (tras[(uint32_t)y * (uint32_t)L + (uint32_t)x] & 0x00FFFFFFu)) {
                    snformatar(m, (size_t)nm, "el bufer de atras llega distinto (fila %d, columna %d)", y, x);
                    ok = false;
                    break;
                }
    }
    vmarca(12, false);   /* región 0 comparada */
    /* dbg4 (Amilo): el búfer de atrás ENTERO, en franjas de A filas a la zona de prueba y comparadas una a una
     * (antes solo se probaban sus primeras A filas, y el primer volcado de verdad colgó el equipo dentro de
     * vops->volcar). Marcas en la fila 6 (columna = franja): amarillo antes de la copia, blanco igual, rojo
     * distinta. */
    if (ok) {
        const uint32_t *tras = video_buffer();
        int H = (int)video_altura();
        for (int y0 = 0, b = 0; ok && y0 < H; y0 += A, b++) {
            int a = H - y0 < A ? H - y0 : A;
            acel_marca_fina(5, b, 0);
            if (!vops->volcar(0, 0, y0, L, a, 0, vz_y) || !vops->esperar(PRAZO_MS)) {
                snformatar(m, (size_t)nm, "fallo al volcar la franja %d del bufer de atras", b);
                ok = false;
                acel_marca_fina(5, b, 2);
                break;
            }
            for (int y = 0; ok && y < a; y++)
                for (int x = 0; x < L; x++)
                    if ((*vz_px(x, y) & 0x00FFFFFFu) != (tras[(uint32_t)(y0 + y) * (uint32_t)L + (uint32_t)x] & 0x00FFFFFFu)) {
                        snformatar(m, (size_t)nm, "el bufer de atras llega distinto (franja %d, fila %d, columna %d)", b, y0 + y, x);
                        ok = false;
                        break;
                    }
            acel_marca_fina(5, b, ok ? 1 : 2);
        }
        klog("[ACEL2D] volcado: bufer de atras entero por franjas de %d filas: %s\n", A, ok ? "IGUAL" : m);
    }
    /* La medida: la región de prueba entera por la GPU y por la CPU. */
    if (ok && khz) {
        uint64_t t0 = tsc();
        for (int k = 0; ok && k < VOLTAS_VOLCADO; k++) ok = vops->volcar(1, 0, 0, L, A, 0, vz_y);
        vmarca(13, false);   /* medida: copias puestas */
        if (ok) ok = vops->esperar(PRAZO_MS);
        vmarca(14, false);   /* medida: GPU terminada */
        uint64_t t1 = tsc();
        bool sse = rapido_inicio();
        for (int k = 0; k < VOLTAS_VOLCADO; k++)
            for (int y = 0; y < A; y++) rapido_copiar_tela(vz_px(0, y), p + y * L, (uint32_t)L * 4u);
        rapido_fim(sse);
        vaciar_wc();
        uint64_t t2 = tsc();
        *us_g = (uint32_t)sconv_div64((t1 - t0) * 1000, khz);
        *us_c = (uint32_t)sconv_div64((t2 - t1) * 1000, khz);
        if (!ok) snformatar(m, (size_t)nm, "tiempo agotado al medir");
        vmarca(15, false);   /* medida: CPU terminada */
    }
    klog("[ACEL2D] volcado: autoprueba %s%s%s; %d volcados de %dx%d: GPU %u us, CPU %u us\n", ok ? "IGUAL que la CPU" : "FALLO",
         ok ? "" : ": ", ok ? "" : m, VOLTAS_VOLCADO, L, A, *us_g, *us_c);
    /* Revisión: la región 1 no puede quedar apuntando a «p» ya liberado (la GART de la R5xx no respeta el
     * «solo lectura» de las entradas: ver gart_vaciar en acel_radeon.c). Antes, el motor parado. */
    char m2[8];
    bool parado = ok || vops->esperar(PRAZO_MS);
    if (parado) vops->mapear(1, NULL, 0, 0, 0, m2, sizeof m2);
    else klog("[ACEL2D] volcado: el motor no para: el bufer de la autoprueba no se libera (la GART aun lo apunta)\n");
    if (parado) kfree(p);
    kfree(ref);
    vmarca(16, false);   /* autoprueba: región 1 soltada y memoria liberada */
    return ok;
}

/* ------------------------------------------------------------ encender, apagar */

/* D391: la GART se suelta cuando ni el volcado ni la vuelta la usan (no a mitad de gart_encender: el volcado
 * puede quedar apagado por lento antes de que se pruebe la vuelta, que necesita la GART). (gart_decidindo está
 * declarada con las marcas de depuración.) */
static void gart_soltar_se_livre(void)
{
    /* Revisión: la GART sigue por la vuelta pero sin volcado: la región 0 no se queda apuntando a un búfer de
     * atrás que puede liberarse (video_refazer) sin que nadie la vuelva a apuntar. */
    if (!gart_decidindo && vg_gart && vg_estado != VOLCADO_ACTIVO && rt_estado == VOLCADO_ACTIVO && vops) {
        char m2[8];
        vops->mapear(0, NULL, 0, 0, 0, m2, sizeof m2);
        vg_mapeado = false;
    }
    if (gart_decidindo || !vg_gart || vg_estado == VOLCADO_ACTIVO || rt_estado == VOLCADO_ACTIVO) return;
    if (vops) vops->soltar();
    vg_gart = false;
    rt_lectura_ok = false;
}

static void volcado_soltar(int novo_estado, const char *m)
{
    vg_mapeado = vg_pendente = false;
    vg_estado = novo_estado;
    if (m) {
        snformatar(vg_motivo, sizeof vg_motivo, "%s", m);
        klog("[ACEL2D] volcado por la GPU apagado: %s\n", m);
    }
    gart_soltar_se_livre();
}

/* El vigilante del volcado: con una GPU de verdad el motor es el del 2D, así que se apaga todo
 * (acel2d_desligar, que también suelta la GART) y la CPU redibuja; con la simulada, solo el volcado. */
static void volcado_falhou(const char *m)
{
    klog("[ACEL2D] volcado: %s\n", m);
    if (!vg_sim && ativo) { acel2d_desligar(m); vg_estado = VOLCADO_FALLO; snformatar(vg_motivo, sizeof vg_motivo, "%s", m); return; }
    volcado_soltar(VOLCADO_FALLO, m);
    video_mostrar_tudo();
}

static bool retorno_decidir(bool forcado);

/* Enciende la GART (si no lo estaba), apunta el búfer de atrás y prueba la LECTURA por ella (la
 * autoprueba del volcado: solo lee la RAM). Con eso decide el volcado (si «volcado») y, D391, la vuelta
 * (si «retorno»): la GPU solo escribe en la RAM si antes se ha visto que lee por la tabla justo las
 * páginas que se le apuntaron. */
static void gart_encender(bool volcado, bool retorno, bool forcado)
{
    char m[96] = "";
    uint32_t ug = 0, uc = 0;
    acel_sincronizar();
    bool sim = int_salva_desliga();
    vmarcas_limpar();
    vmarca(1, false);   /* gart_encender: empieza */
    if (!vg_gart) {
        if (!vops->preparar(m, sizeof m)) {
            int_restaura(sim);
            if (volcado) { vg_estado = VOLCADO_NO; snformatar(vg_motivo, sizeof vg_motivo, "%s", m); }
            if (retorno) { rt_estado = VOLCADO_NO; snformatar(rt_motivo, sizeof rt_motivo, "%s", m); }
            klog("[ACEL2D] volcado: no se puede: %s\n", m);
            return;
        }
        vg_gart = true;
    }
    vmarca(2, false);   /* GART preparada (como los pasos 0-4) */
    gart_decidindo = true;
    vg_mapeado = false;
    bool ok = vops->mapear(0, video_buffer(), video_largura() * 4u, video_largura(), video_altura(), m, sizeof m);
    vmarca(3, false);   /* región 0 (búfer de atrás) apuntada */
    if (!ok) vmarca(31, false);
    if (ok) ok = volcado_autoprova(m, sizeof m, &ug, &uc);
    rt_lectura_ok = ok;
    if (!ok && vops->detalle) vops->detalle("tras el fallo de la autoprueba");
    if (ok && vg_estado == VOLCADO_ACTIVO) vg_mapeado = true;   /* ya estaba activo (se enciende la vuelta) */
    if (!ok && !volcado && vg_estado == VOLCADO_ACTIVO) volcado_soltar(VOLCADO_FALLO, m);
    if (volcado) {
        vg_us_gpu = ug; vg_us_cpu = uc;
        if (!ok) volcado_soltar(VOLCADO_FALLO, m);
        else if (!forcado && ug > 2 * uc) {
            char t[96];
            snformatar(t, sizeof t, "autoprueba superada, pero la GPU (%u us) tarda mas del doble que la CPU (%u us)", ug, uc);
            volcado_soltar(VOLCADO_LENTO, t);
        } else {
            vg_mapeado = true;
            vg_estado = VOLCADO_ACTIVO;
            snformatar(vg_motivo, sizeof vg_motivo, "%s%s: GPU %u us, CPU %u us en la autoprueba", vg_sim ? "simulado" : drv->nome,
                       forcado ? " (a mano)" : "", ug, uc);
            klog("[ACEL2D] volcado por la GPU activo: %s\n", vg_motivo);
        }
    }
    if (retorno) {
        if (!ok) {
            rt_estado = VOLCADO_FALLO;
            snformatar(rt_motivo, sizeof rt_motivo, "sin probar: la lectura por la GART fallo (%s)", m);
            klog("[ACEL2D] vuelta: %s\n", rt_motivo);
        } else (void)retorno_decidir(forcado);
    }
    gart_decidindo = false;
    vmarca(17, false);   /* decidido (si la autoprueba falló, sale también la 31) */
    if (!ok) vmarca(31, false);
    if (vg_estado == VOLCADO_ACTIVO) vmarca(32, false);   /* (32: el volcado queda activo) */
    int_restaura(sim);
    vmarca(18, false);   /* interrupciones abiertas otra vez */
    gart_soltar_se_livre();
    vmarca(19, false);   /* GART soltada si nadie la usa */
    vmarcas_ao_tras();   /* las marcas también en el búfer de atrás, para que video_mostrar_tudo no las borre */
    video_mostrar_tudo();   /* la zona de prueba visible (sin acelerador) vuelve a su sitio */
    vmarca(20, true);   /* el primer video_mostrar_tudo ha vuelto */
}

void acel2d_volcado_iniciar(void)
{
    uint8_t op = kconf_cab(CAB_ACEL_OPC);
    vg_sim = (op & CAB_ACEL_VOLCADO_SIM) != 0;
    bool seguro = (info_boot.opcoes & OPC_BOOT_ACEL_SEGURO) || (modo & CAB_ACEL_SEGURO);
    const char *no = NULL;
    if (!video_ativo()) no = "sin modo grafico";
    else if (!video_formato_direto()) no = "la pantalla no es de 32 bits 0x00RRGGBB";
    else if (seguro) no = "modo seguro de la aceleracion";
    bool volcado = !no, retorno = !no;
    /* Lo apagado en kconfig se dice antes que la falta de acelerador (como antes de D391). */
    if (volcado && (op & CAB_ACEL_SIN_VOLCADO)) {
        volcado = false;
        vg_estado = VOLCADO_KCONFIG;
        txt_copia_n(vg_motivo, "apagado en las opciones del kernel (kconfig volcado_gpu=no)", sizeof vg_motivo);
        klog("[ACEL2D] volcado: %s\n", vg_motivo);
    }
    if (retorno && (op & CAB_ACEL_SIN_RETORNO)) {
        retorno = false;
        rt_estado = VOLCADO_KCONFIG;
        txt_copia_n(rt_motivo, "apagada en las opciones del kernel (kconfig vuelta_gpu=no)", sizeof rt_motivo);
        klog("[ACEL2D] vuelta: %s\n", rt_motivo);
    }
    if (!no && !vg_sim && !ativo) no = "sin acelerador 2D activo";
    else if (!no && !vg_sim && !drv->volcado) no = "este acelerador no sabe leer la RAM del sistema (solo Radeon R5xx, D387)";
    if (no) {
        if (vg_estado != VOLCADO_KCONFIG) {
            vg_estado = VOLCADO_NO;
            snformatar(vg_motivo, sizeof vg_motivo, "%s: copia la CPU", no);
            klog("[ACEL2D] volcado: %s\n", vg_motivo);
        }
        if (rt_estado != VOLCADO_KCONFIG) {
            rt_estado = VOLCADO_NO;
            snformatar(rt_motivo, sizeof rt_motivo, "%s: lee la CPU", no);
        }
        return;
    }
    vops = vg_sim ? &volcado_sim : drv->volcado;
    if (retorno && (!vops->mapear_escrita || !vops->retornar)) {
        retorno = false;
        rt_estado = VOLCADO_NO;
        txt_copia_n(rt_motivo, "este acelerador no sabe escribir en la RAM: lee la CPU", sizeof rt_motivo);
    }
    /* Amilo (2026-11-19): con la GART encendida al arrancar, la X1400 colgó el equipo entero (con las
     * interrupciones cerradas, sin volver de una lectura). Hasta que una tarjeta pase el volcado en hardware
     * real (lista volcado_confirmadas, el volcado y la vuelta por separado), la GART no se toca sola: solo
     * «acel2d volcado encender» o «acel2d vuelta encender» (que avisan de que pueden colgar), y «acel2d volcado
     * paso N» para depurar. La GPU simulada de las pruebas sigue igual. */
    if (!vg_sim && ((volcado && !volcado_confirmada(false)) || (retorno && !volcado_confirmada(true)))) {
        if (volcado && !volcado_confirmada(false)) {
            volcado = false;
            vg_estado = VOLCADO_NO;
            txt_copia_n(vg_motivo, "sin probar en esta tarjeta (puede colgar el equipo): «acel2d volcado encender» lo prueba",
                        sizeof vg_motivo);
            klog("[ACEL2D] volcado: %s\n", vg_motivo);
        }
        if (retorno && !volcado_confirmada(true)) {
            retorno = false;
            rt_estado = VOLCADO_NO;
            txt_copia_n(rt_motivo, "sin probar en esta tarjeta (puede colgar el equipo): «acel2d vuelta encender» la prueba",
                        sizeof rt_motivo);
            klog("[ACEL2D] vuelta: %s\n", rt_motivo);
        }
        /* Lo que sí está confirmado (el volcado de la X1400) sigue adelante solo. */
    }
    if (volcado || retorno) gart_encender(volcado, retorno, false);
}

bool acel_volcar(int x, int y, int l, int a)
{
    if (vg_estado != VOLCADO_ACTIVO || !vg_mapeado || (!vg_sim && !ativo)) return false;
    if ((uint32_t)l * (uint32_t)a < VOLCADO_MIN_PX) { vg_cpu_ops++; return false; }
    bool sim = int_salva_desliga();
    vmarca_cuadro(VMARCA_C_X, VMARCA_EST_Y, (const uint32_t[]){ 0x000000FFu, 0x0000FFFFu, 0x00FF00FFu, 0x00FFFF00u }[vg_ops & 3], true);
    vmarca_cuadro(VMARCA_A_X, VMARCA_EST_Y, 0x00FF0000u, true);   /* A: dentro de vops->volcar */
    vaciar_wc();
    if (!vg_sim) rastro_marca(0x0310u);   /* dbg5: A, dentro de vops->volcar */
    bool ok = vops->volcar(0, x, y, l, a, x, y);
    if (!vg_sim) rastro_marca(0x0311u);
    if (vmarca_ocupado_ok()) vmarca_cuadro(VMARCA_A_X, VMARCA_EST_Y, 0x0000FF00u, true);    /* A: fuera (el motor copia) */
    if (ok) {
        vg_pendente = true;
        vg_ops++;
        vg_resto += (uint32_t)l * (uint32_t)a; vg_kpx += vg_resto / 1000; vg_resto %= 1000;
    }
    int_restaura(sim);
    if (!ok) { volcado_falhou("la GPU rechazo un volcado"); return false; }
    return true;
}

void acel_volcado_buffer(bool antes)
{
    if (antes) { acel_sincronizar(); vg_mapeado = false; return; }
    if (vg_estado != VOLCADO_ACTIVO || !vops) return;
    char m[96] = "";
    bool sim = int_salva_desliga();
    vg_mapeado = vops->mapear(0, video_buffer(), video_largura() * 4u, video_largura(), video_altura(), m, sizeof m);
    int_restaura(sim);
    if (!vg_mapeado) {
        char t[96];
        snformatar(t, sizeof t, "el bufer de atras nuevo no se puede apuntar: %s", m);
        volcado_soltar(VOLCADO_NO, t);
    }
}

/* «acel2d volcado medir»: la pantalla entera (lo que ya muestra: no cambia nada) por la GPU y por la CPU. */
#define VOLTAS_MEDIR 10
static int32_t volcado_medir(void)
{
    uint32_t khz = hw_tsc_khz();
    if (vg_estado != VOLCADO_ACTIVO || !khz) return ERRO_INVALIDO;
    int L = (int)video_largura(), A = (int)video_altura();
    acel_sincronizar();
    bool sim = int_salva_desliga();
    uint64_t t0 = tsc();
    bool ok = true;
    for (int k = 0; ok && k < VOLTAS_MEDIR; k++) ok = vops->volcar(0, 0, 0, L, A, 0, 0);
    uint64_t t1 = tsc();
    if (ok) ok = vops->esperar(VOLCADO_PRAZO_MS * VOLTAS_MEDIR);
    uint64_t t2 = tsc();
    int_restaura(sim);
    /* La CPU, pantalla a pantalla con las interrupciones cerradas solo en cada una (sin WC, una
     * pantalla puede tardar decenas de ms: diez seguidas dejarían el reloj parado demasiado). */
    uint64_t tc = 0;
    for (int k = 0; k < VOLTAS_MEDIR; k++) {
        sim = int_salva_desliga();
        uint64_t a0 = tsc();
        video_volcar_cpu(0, 0, L, A);
        tc += tsc() - a0;
        int_restaura(sim);
    }
    uint64_t t3 = t2 + tc;
    uint32_t ug = (uint32_t)sconv_div64((t2 - t0) * 1000, khz), uq = (uint32_t)sconv_div64((t1 - t0) * 1000, khz);
    uint32_t uc = (uint32_t)sconv_div64((t3 - t2) * 1000, khz);
    uint32_t mb = (uint32_t)L * (uint32_t)A * 4u / 1000u * VOLTAS_MEDIR;   /* KB en total */
    klog("[ACEL2D] volcado medir: %d pantallas de %dx%d: GPU %u us (la CPU solo %u us poniendo las ordenes), CPU %u us; "
         "GPU %u MB/s, CPU %u MB/s%s\n", VOLTAS_MEDIR, L, A, ug, uq, uc, ug ? mb * 1000u / ug : 0, uc ? mb * 1000u / uc : 0,
         ok ? "" : " (la GPU no termino)");
    vg_us_gpu = ug / VOLTAS_MEDIR; vg_us_cpu = uc / VOLTAS_MEDIR;
    if (!ok) { volcado_falhou("tiempo agotado al medir el volcado"); return ERRO_INVALIDO; }
    return 0;
}

int32_t acel2d_volcado_ctl(int op)
{
    if (op == 2) {
        if (vg_estado != VOLCADO_ACTIVO) return 0;
        acel_sincronizar();
        volcado_soltar(VOLCADO_A_MANO, "apagado a mano (acel2d volcado apagar)");
        video_mostrar_tudo();
        return 0;
    }
    if (op == 3) {
        if (vg_estado == VOLCADO_ACTIVO) return 0;
        if (!vops) acel2d_volcado_iniciar();
        if (vg_estado != VOLCADO_ACTIVO && vops && (vg_sim || ativo) && video_formato_direto()) gart_encender(true, false, true);
        return vg_estado == VOLCADO_ACTIVO ? 0 : ERRO_INVALIDO;
    }
    if (op == 4) return volcado_medir();
    if (op >= 5 && op <= 7) return acel2d_retorno_ctl(op);   /* D391 */
    if (op >= 37 && op <= 45) {   /* D387: las opciones de la valla del 3D (gpu3d_r500.c): también con el 2D ya apagado */
        extern bool gpu3d_r500_opcao(int paso);
        return gpu3d_r500_opcao(op - 10) ? 0 : ERRO_INVALIDO;
    }
    if (op >= 10 && op <= 36) {   /* depuración (dbg5: hasta el 25; 26, D387): «acel2d volcado paso N» (ver gv_preparar_ate en acel_radeon.c) */
        if (!vops) acel2d_volcado_iniciar();
        if (vg_sim || !ativo || !drv || !drv->volcado || !drv->volcado->paso) {
            klog("[ACEL2D] volcado paso: este acelerador no tiene pasos de depuracion\n");
            return ERRO_INVALIDO;
        }
        /* D387: la opción 26 (el 2D por MMIO con el CP en marcha) solo cambia cómo se mandan las órdenes: vale con la
         * GART en uso. */
        if (vg_gart && op != 36) { klog("[ACEL2D] volcado paso: la GART esta en uso (apagar antes el volcado y la vuelta)\n"); return ERRO_INVALIDO; }
        char m[96] = "";
        acel_sincronizar();
        bool sim = int_salva_desliga();   /* como en el arranque (gart_encender) */
        bool ok = drv->volcado->paso(op - 10, m, sizeof m);
        int_restaura(sim);
        /* dbg5: tras un experimento la CPU repone la pantalla (marcas, testigos). */
        if (!(ativo && zona) || (op - 10 >= 7 && op - 10 < 20)) video_mostrar_tudo();
        return ok ? 0 : ERRO_INVALIDO;
    }
    return ERRO_INVALIDO;
}

/* «acel2d probar»: con el volcado activo, su autoprueba otra vez (y los registros con --detalle). */
static bool retorno_autoprova(char *m, int nm, uint32_t *us_g, uint32_t *us_c, bool *mascara);
static void retorno_falhou(const char *m);

static void volcado_probar(bool detalle)
{
    char m[96] = "";
    uint32_t ug = 0, uc = 0;
    if (vg_estado != VOLCADO_ACTIVO || !vops) klog("[ACEL2D] volcado: %s\n", vg_motivo);
    else {
        if (detalle && vops->detalle) vops->detalle("antes de probar");
        bool ok = volcado_autoprova(m, sizeof m, &ug, &uc);
        if (detalle && vops->detalle) vops->detalle(ok ? "al final" : "tras el fallo");
        if (!ok) volcado_falhou(m);
    }
    /* D391: la vuelta, si está activa (y lo sigue tras lo anterior). */
    if (rt_estado != VOLCADO_ACTIVO || !vops) { klog("[ACEL2D] vuelta: %s\n", rt_motivo); return; }
    bool mascara = rt_mascara;
    bool sim = int_salva_desliga();
    bool ok = retorno_autoprova(m, sizeof m, &ug, &uc, &mascara);
    int_restaura(sim);
    if (detalle && vops->detalle) vops->detalle(ok ? "tras la autoprueba de la vuelta" : "tras el fallo de la vuelta");
    if (!(ativo && zona)) video_mostrar_tudo();
    if (!ok) retorno_falhou(m);
}

/* ================================================================== D391: la vuelta del 3D a la RAM
 *
 * El 3D por la GPU dibuja en la VRAM; para que el cuadro llegue a la ventana, hasta ahora la CPU lo
 * leía de la VRAM (r5_ler), y la CPU lee la memoria de la tarjeta muy despacio (sin caché ni
 * combinación de lecturas: cada píxel es una lectura por el bus). Con la GART del volcado (D387) el
 * motor 2D puede ESCRIBIR en la RAM: copia el destino del 3D directamente al búfer del programa, y la
 * CPU no toca la VRAM. Después todo sigue igual (el escritorio compone la ventana y el volcado, D387,
 * la lleva a la pantalla), así que vale con ventanas tapadas, movidas o a medias.
 *
 * Reglas, porque aquí la GPU escribe en la RAM del sistema (un error podría estropear memoria):
 *   - solo si la LECTURA por la GART superó su autoprueba en esta sesión (la GPU usa nuestra tabla y
 *     lee justo las páginas apuntadas) y la vuelta supera la suya: un búfer del kernel con guardas
 *     delante, detrás y entre filas; si la GPU escribe un solo píxel fuera, no se enciende;
 *   - las entradas con permiso de escritura existen solo mientras dura cada copia (con las
 *     interrupciones cerradas y esperando a que termine, con plazo); después vuelven a la página de
 *     relleno, solo de lectura. Fuera de esa ventana la GPU no puede escribir en la RAM;
 *   - solo páginas presentes, de RAM (E820) y, si son del programa, suyas y con escritura;
 *   - un plazo agotado o una orden rechazada apaga la vuelta y, con la GPU de verdad, el 2D entero
 *     (el vigilante, D244): las entradas ya se han quitado y el bus maestro se apaga con la GART.
 *     Ese cuadro, y los siguientes, los lee la CPU como antes. */

#define RETORNO_PRAZO_MS 100
#define RETORNO_MAX_PAG  2048    /* 8 MB: la ventana más grande (ESPACO_JANELA, D059) */
#define RETORNO_PROVA_A  32
#define VOLTAS_RETORNO   8

static void retorno_soltar(int novo_estado, const char *m)
{
    rt_estado = novo_estado;
    if (m) {
        snformatar(rt_motivo, sizeof rt_motivo, "%s", m);
        klog("[ACEL2D] vuelta por la GPU apagada: %s\n", m);
    }
    gart_soltar_se_livre();
}

/* El vigilante de la vuelta: como el del volcado (el motor es el mismo). */
static void retorno_falhou(const char *m)
{
    klog("[ACEL2D] vuelta: %s\n", m);
    if (!vg_sim && ativo) {
        acel2d_desligar(m);
        rt_estado = VOLCADO_FALLO;
        snformatar(rt_motivo, sizeof rt_motivo, "%s", m);
        return;
    }
    retorno_soltar(VOLCADO_FALLO, m);
}

/* Una copia del origen a «dest» por la GPU, con las interrupciones ya cerradas. 1 = hecha (la GPU
 * terminó); 0 = no se puede (sin daño: la hace la CPU; motivo en m); -1 = la GPU falló (motivo en m). */
static int retorno_copiar(espaco_t e, bool usuario, const acel_origem_t *o, int l, int a, uint32_t *dest, int passo_px,
                          bool mascara, char *m, int nm)
{
    static uint64_t pag[RETORNO_MAX_PAG];
    const char *porque;
    retorno_destino_t d = { 0x1000u, (uint32_t)dest & 0xFFFu, (uint32_t)passo_px * 4u };
    if (passo_px <= 0 || !retorno_destino_ok(&d, l, a, &porque)) { snformatar(m, (size_t)nm, "%s", passo_px <= 0 ? "paso nulo" : porque); return 0; }
    /* La GPU de verdad lee el origen por su dirección del MC: alineada a 1 KB, paso múltiplo de 64. */
    if (!vg_sim && o->gpu != ACEL_ORIGEM_TELA && (!o->gpu || (o->gpu & 0x3FFu) || o->gpu > 0xFFFFFFFFull || (o->passo & 63u) ||
                                                   o->passo > 255u * 64u)) {
        snformatar(m, (size_t)nm, "el origen (MC %x, paso %u) no sirve al motor 2D", (uint32_t)o->gpu, o->passo);
        return 0;
    }
    uint32_t np = (retorno_bytes(&d, l, a) + 4095u) >> 12, v0 = (uint32_t)dest & ~0xFFFu;
    if (np > RETORNO_MAX_PAG) { snformatar(m, (size_t)nm, "destino de %u paginas: demasiado grande", np); return 0; }
    for (uint32_t i = 0; i < np; i++) {
        uint32_t fl = 0;
        fis_t f = espaco_traduzir(e, v0 + i * 4096u, &fl);
        if (!f) { snformatar(m, (size_t)nm, "la pagina %u del destino no esta presente", i); return 0; }
        if (usuario && (!(fl & PG_USUARIO) || !(fl & PG_ESCRITA))) {
            snformatar(m, (size_t)nm, "la pagina %u del destino no es del programa con escritura", i);
            return 0;
        }
        f &= ~(fis_t)0xFFF;
        if (!memfisica_sobre_ram(f, f + 4096u)) {
            snformatar(m, (size_t)nm, "la pagina %u del destino (fisica %x:%x) no es RAM", i, (uint32_t)(f >> 32), (uint32_t)f);
            return 0;
        }
        pag[i] = f;
    }
    if (!vops->mapear_escrita(pag, np, m, nm)) { vops->mapear_escrita(NULL, 0, m, nm); return 0; }
    bool ok = vops->retornar(o, l, a, d.desl, d.passo, mascara);
    if (!ok) snformatar(m, (size_t)nm, "la GPU rechazo la vuelta de %dx%d", l, a);
    else if (!vops->esperar(RETORNO_PRAZO_MS)) { snformatar(m, (size_t)nm, "tiempo agotado esperando la vuelta de %dx%d", l, a); ok = false; }
    /* Siempre: fuera de la copia la GPU no puede escribir en la RAM. */
    char m2[8];
    vops->mapear_escrita(NULL, 0, m2, sizeof m2);
    if (ok) vg_pendente = false;   /* la espera cubre también los volcados de antes (la misma cola) */
    return ok ? 1 : -1;
}

/* Lo de la zona de prueba (con un alfa que la vuelta tiene que quitar). */
static inline uint32_t retorno_px(int x, int y) { return (prova_px(y, x) ^ 0x000F0F0Fu) | ((uint32_t)(0x80 | ((x + y) & 0x7F)) << 24); }

/* La autoprueba: la zona de prueba de la VRAM (con un dibujo y alfa) a dos búferes del kernel, uno con el
 * paso del escritorio y otro con un paso torcido (fila a fila), rodeados de guardas. true si cada píxel
 * es el de la CPU y ninguna guarda cambió. *mascara: false si la GPU deja el alfa (lo demás igual). */
#define GUARDA 0xC3D2E1F0u
static bool retorno_autoprova(char *m, int nm, uint32_t *us_g, uint32_t *us_c, bool *mascara)
{
    int L = (int)video_largura(), A = RETORNO_PROVA_A;
    uint32_t khz = hw_tsc_khz();
    if (!vz_preparar()) { snformatar(m, (size_t)nm, "sin zona de prueba"); return false; }
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) *vz_px(x, y) = retorno_px(x, y);
    vaciar_wc();
    acel_origem_t o = { ACEL_ORIGEM_TELA, (const uint32_t *)vz_px(0, 0), video_pitch(), vz_y };
    bool ok = true;
    *mascara = true;
    for (int caso = 0; ok && caso < 2; caso++) {
        /* caso 0: paso del escritorio; caso 1: siete píxeles más (no múltiplo de 64: fila a fila) y el
         * destino empezando 1016 bytes dentro de una página. */
        int passo_px = caso ? L + 7 : L;
        uint32_t frente = caso ? 1016u + 4096u : 4096u, bytes = frente + (uint32_t)passo_px * 4u * (uint32_t)A + 8192u;
        uint8_t *bruto = kmalloc(bytes + 4096u);
        if (!bruto) { snformatar(m, (size_t)nm, "sin memoria para la autoprueba de la vuelta"); return false; }
        uint32_t *todo = (uint32_t *)(((uint32_t)bruto + 4095u) & ~4095u);   /* alineado: el desplazamiento lo pone «frente» */
        for (uint32_t i = 0; i < bytes / 4u; i++) todo[i] = GUARDA ^ i;
        uint32_t *dest = todo + frente / 4u;
        int r = -1;
        for (int intento = 0; intento < 2; intento++) {
            r = retorno_copiar(espaco_kernel(), false, &o, L, A, dest, passo_px, *mascara, m, nm);
            if (r != 1) break;
            if (kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_RETORNO_FALLO) dest[(A / 2) * passo_px + L / 3] ^= 0x00FF00u;
            /* Comparar todo el bloque: dentro del rectángulo el origen; fuera, la guarda. */
            uint32_t dif = 0, so_alfa = 0, fora = 0;
            int px = -1, py = -1;
            for (uint32_t i = 0; i < bytes / 4u; i++) {
                int32_t k = (int32_t)i - (int32_t)(frente / 4u);
                int y = k >= 0 ? k / passo_px : -1, x = k >= 0 ? k % passo_px : -1;
                uint32_t esperado = GUARDA ^ i;
                bool dentro = k >= 0 && y < A && x < L;
                if (dentro) esperado = *mascara ? retorno_px(x, y) & 0x00FFFFFFu : retorno_px(x, y);
                if (todo[i] == esperado) continue;
                if (!dentro) fora++;
                else if (*mascara && todo[i] == retorno_px(x, y)) so_alfa++;
                else { dif++; if (px < 0) { px = x; py = y; } }
            }
            if (fora) {
                snformatar(m, (size_t)nm, "la GPU escribio %u palabras FUERA del destino", fora);
                klog("[ACEL2D] vuelta: autoprueba: %u palabras fuera del destino (caso %d): la vuelta queda apagada\n", fora, caso);
                ok = false;
            } else if (dif) {
                snformatar(m, (size_t)nm, "pixeles distintos de la CPU (el primero en %d,%d)%s", px, py,
                           (kconf_cab(CAB_ACEL_OPC) & CAB_ACEL_RETORNO_FALLO) ? " (simulado)" : "");
                klog("[ACEL2D] vuelta: primer pixel distinto en %d,%d (caso %d): gpu %x esperado %x\n", px, py, caso,
                     dest[py * passo_px + px], retorno_px(px, py) & 0x00FFFFFFu);
                ok = false;
            } else if (so_alfa) {
                /* La GPU no aplicó la máscara (ROP origen AND color): se prueba sin ella; el alfa no lo usa el escritorio. */
                klog("[ACEL2D] vuelta: la GPU deja el alfa (%u pixeles): se prueba copiando tal cual\n", so_alfa);
                *mascara = false;
                for (uint32_t i = 0; i < bytes / 4u; i++) todo[i] = GUARDA ^ i;
                continue;
            }
            break;
        }
        if (r == 0) { klog("[ACEL2D] vuelta: autoprueba: no se puede: %s\n", m); ok = false; }
        if (r < 0) ok = false;
        /* La medida, con el primer búfer: la región entera por la GPU y la CPU leyendo la VRAM. */
        if (ok && caso == 0 && khz) {
            uint64_t t0 = tsc();
            for (int k = 0; ok && k < VOLTAS_RETORNO; k++) ok = retorno_copiar(espaco_kernel(), false, &o, L, A, dest, L, *mascara, m, nm) == 1;
            uint64_t t1 = tsc();
            /* La CPU, una sola vez (leer la VRAM es lo lento: con las interrupciones cerradas, no ocho). */
            for (int y = 0; y < A; y++) {
                const volatile uint32_t *src = vz_px(0, y);
                uint32_t *d = dest + y * L;
                for (int x = 0; x < L; x++) d[x] = src[x] & 0x00FFFFFFu;
            }
            uint64_t t2 = tsc();
            *us_g = (uint32_t)sconv_div64((t1 - t0) * 1000, khz);
            *us_c = (uint32_t)sconv_div64((t2 - t1) * 1000 * VOLTAS_RETORNO, khz);
            if (!ok) snformatar(m, (size_t)nm, "tiempo agotado al medir la vuelta");
        }
        kfree(bruto);
    }
    klog("[ACEL2D] vuelta: autoprueba %s%s%s; %d vueltas de %dx%d: GPU %u us, CPU leyendo la VRAM %u us%s\n",
         ok ? "IGUAL que la CPU" : "FALLO", ok ? "" : ": ", ok ? "" : m, VOLTAS_RETORNO, L, A, *us_g, *us_c,
         ok && !*mascara ? " (con el alfa de la VRAM)" : "");
    return ok;
}

/* Con la lectura por la GART ya probada: la autoprueba de la vuelta y el estado. Interrupciones cerradas. */
static bool retorno_decidir(bool forcado)
{
    char m[96] = "";
    uint32_t ug = 0, uc = 0;
    bool mascara = true;
    bool ok = retorno_autoprova(m, sizeof m, &ug, &uc, &mascara);
    rt_us_gpu = ug; rt_us_cpu = uc;
    if (!ok) {
        if (vops->detalle) vops->detalle("tras el fallo de la autoprueba de la vuelta");
        rt_estado = VOLCADO_FALLO;
        snformatar(rt_motivo, sizeof rt_motivo, "%s", m);
        klog("[ACEL2D] vuelta por la GPU apagada: %s\n", m);
        return false;
    }
    /* Con la GPU de verdad la CPU lee la VRAM muchas veces más despacio; si aun así la GPU tarda más
     * del doble, algo raro pasa (a mano se enciende igual). La simulada no se mide. */
    if (!vg_sim && !forcado && ug > 2 * uc) {
        rt_estado = VOLCADO_LENTO;
        snformatar(rt_motivo, sizeof rt_motivo, "autoprueba superada, pero la GPU (%u us) tarda mas del doble que la CPU (%u us)", ug, uc);
        klog("[ACEL2D] vuelta por la GPU apagada: %s\n", rt_motivo);
        return false;
    }
    rt_mascara = mascara;
    rt_estado = VOLCADO_ACTIVO;
    snformatar(rt_motivo, sizeof rt_motivo, "%s%s: GPU %u us, CPU %u us en la autoprueba%s", vg_sim ? "simulada" : drv->nome,
               forcado ? " (a mano)" : "", ug, uc, mascara ? "" : "; sin quitar el alfa");
    klog("[ACEL2D] vuelta por la GPU activa: %s\n", rt_motivo);
    return true;
}

bool acel_retornar(const acel_origem_t *o, int l, int a, uint32_t *dest, int passo_px)
{
    if (rt_estado != VOLCADO_ACTIVO || !vg_gart || !rt_lectura_ok || !vops || (!vg_sim && !ativo)) return false;
    bool usuario = (uint32_t)dest >= USUARIO_INICIO;
    char m[96] = "";
    bool sim = int_salva_desliga();
    int r = retorno_copiar(usuario ? espaco_atual() : espaco_kernel(), usuario, o, l, a, dest, passo_px, rt_mascara, m, sizeof m);
    int_restaura(sim);
    if (r < 0) { retorno_falhou(m); return false; }
    if (r == 0) {
        rt_cpu_ops++;
        if (rt_avisos < 4) { rt_avisos++; klog("[ACEL2D] vuelta: este cuadro (%dx%d) lo lee la CPU: %s\n", l, a, m); }
        return false;
    }
    rt_ops++;
    rt_resto += (uint32_t)l * (uint32_t)a; rt_kpx += rt_resto / 1000; rt_resto %= 1000;
    return true;
}

/* «acel2d vuelta medir»: la zona de prueba (filas de la pantalla, sin cambiarlas) a un búfer del kernel,
 * VOLTAS_RETORNO veces por la GPU y por la CPU leyendo la VRAM, con el tamaño del cubo de sgldemo. */
static int32_t retorno_medir(void)
{
    uint32_t khz = hw_tsc_khz();
    if (rt_estado != VOLCADO_ACTIVO || !khz || !vz_preparar()) return ERRO_INVALIDO;
    int L = (int)video_largura() < 320 ? (int)video_largura() : 320, A = RETORNO_PROVA_A;
    uint32_t *dest = kmalloc((uint32_t)(L * A) * 4u);
    if (!dest) return ERRO_SEM_MEMORIA;
    acel_origem_t o = { ACEL_ORIGEM_TELA, (const uint32_t *)vz_px(0, 0), video_pitch(), vz_y };
    char m[96] = "";
    acel_sincronizar();
    bool sim = int_salva_desliga();
    uint64_t t0 = tsc();
    int r = 1;
    for (int k = 0; r == 1 && k < VOLTAS_RETORNO; k++) r = retorno_copiar(espaco_kernel(), false, &o, L, A, dest, L, rt_mascara, m, sizeof m);
    uint64_t t1 = tsc();
    int_restaura(sim);
    uint64_t tc = 0;
    for (int k = 0; k < VOLTAS_RETORNO; k++) {
        sim = int_salva_desliga();
        uint64_t a0 = tsc();
        for (int y = 0; y < A; y++) {
            const volatile uint32_t *src = vz_px(0, y);
            for (int x = 0; x < L; x++) dest[y * L + x] = src[x] & 0x00FFFFFFu;
        }
        tc += tsc() - a0;
        int_restaura(sim);
    }
    kfree(dest);
    uint32_t ug = (uint32_t)sconv_div64((t1 - t0) * 1000, khz), uc = (uint32_t)sconv_div64(tc * 1000, khz);
    uint32_t kb = (uint32_t)L * (uint32_t)A * 4u / 1000u * VOLTAS_RETORNO;
    klog("[ACEL2D] vuelta medir: %d vueltas de %dx%d: GPU %u us, CPU leyendo la VRAM %u us; GPU %u MB/s, CPU %u MB/s%s%s\n",
         VOLTAS_RETORNO, L, A, ug, uc, ug ? kb * 1000u / ug : 0, uc ? kb * 1000u / uc : 0, r == 1 ? "" : "; fallo: ", r == 1 ? "" : m);
    if (vops->detalle) vops->detalle("tras medir la vuelta");
    if (r < 0) { retorno_falhou(m); return ERRO_INVALIDO; }
    if (r == 1) { rt_us_gpu = ug / VOLTAS_RETORNO; rt_us_cpu = uc / VOLTAS_RETORNO; }
    if (!(ativo && zona)) video_mostrar_tudo();   /* sin acelerador la zona son filas visibles */
    return r == 1 ? 0 : ERRO_INVALIDO;
}

int32_t acel2d_retorno_ctl(int op)
{
    if (op == 5) {
        if (rt_estado != VOLCADO_ACTIVO) return 0;
        acel_sincronizar();
        retorno_soltar(VOLCADO_A_MANO, "apagada a mano (acel2d vuelta apagar)");
        return 0;
    }
    if (op == 6) {
        if (rt_estado == VOLCADO_ACTIVO) return 0;
        if (!vops) acel2d_volcado_iniciar();
        if (rt_estado != VOLCADO_ACTIVO && vops && vops->retornar && (vg_sim || ativo) && video_formato_direto())
            gart_encender(false, true, true);
        return rt_estado == VOLCADO_ACTIVO ? 0 : ERRO_INVALIDO;
    }
    if (op == 7) return retorno_medir();
    return ERRO_INVALIDO;
}
