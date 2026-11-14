// SPDX-License-Identifier: GPL-2.0-or-later
/* gpuid.h - familia y generación de la GPU por su identificador PCI (mejora 054 v5; D104).
 *
 * Tabla de rangos conocidos de AMD/ATI e Intel; ante la duda, «desconocida».
 * Solo código en C sin dependencias: lo usa infopc y lo comprueba el anfitrión
 * (tests/h77_sgl.py). Se llama solo con dispositivos PCI de clase 0x03 (pantalla). */
#ifndef SAGUI_GPUID_H
#define SAGUI_GPUID_H

typedef struct {
    const char *familia;     /* «AMD R6xx/R7xx», «Intel Gen3», «desconocida»... */
    const char *detalle;     /* nombres comerciales o de chip */
    const char *motor;       /* si SaguiGL tendrá motor por hardware o solo por software */
} gpu_id_t;

typedef struct { unsigned short a, b; } gpu_faixa_t;

static inline int gpu_em(const gpu_faixa_t *f, int n, unsigned short m)
{
    for (int i = 0; i < n; i++)
        if (m >= f[i].a && m <= f[i].b) return 1;
    return 0;
}
#define GPU_EM(tab, m) gpu_em(tab, (int)(sizeof tab / sizeof tab[0]), m)

static inline gpu_id_t gpu_identificar(unsigned short fab, unsigned short mod)
{
    static const char *const SO = "solo por software";
    gpu_id_t r = { "desconocida", "", SO };
    if (fab == 0x1002) {                                   /* AMD / ATI */
        static const gpu_faixa_t r100[] = { { 0x4136, 0x4137 }, { 0x4966, 0x496F }, { 0x4C57, 0x4C66 }, { 0x5144, 0x515E }, { 0x5960, 0x5969 } };
        static const gpu_faixa_t r300[] = { { 0x4144, 0x4177 }, { 0x4A48, 0x4A6F }, { 0x4E44, 0x4E57 }, { 0x5460, 0x5465 },
                                            { 0x5548, 0x555F }, { 0x564A, 0x5657 }, { 0x5834, 0x5835 }, { 0x5954, 0x5955 },
                                            { 0x5A41, 0x5A42 }, { 0x5A61, 0x5A62 }, { 0x5B60, 0x5B65 }, { 0x5D48, 0x5D57 },
                                            { 0x5E48, 0x5E4F } };
        static const gpu_faixa_t r5xx[] = { { 0x7100, 0x72FF }, { 0x791E, 0x791F }, { 0x7941, 0x7942 } };
        static const gpu_faixa_t r6xx[] = { { 0x9400, 0x95FF }, { 0x9610, 0x9616 }, { 0x9710, 0x9715 } };
        static const gpu_faixa_t evergreen[] = { { 0x6880, 0x68FF }, { 0x9640, 0x964F }, { 0x9800, 0x9808 } };
        static const gpu_faixa_t depois[] = { { 0x6600, 0x687F }, { 0x6980, 0x69FF }, { 0x7300, 0x75FF }, { 0x9900, 0x99FF },
                                              { 0x1300, 0x13FF }, { 0x15D0, 0x15FF } };
        if (GPU_EM(r6xx, mod)) { r.familia = "AMD R6xx/R7xx"; r.detalle = "Radeon HD 2000-4000, HD 3200/4200 integradas"; r.motor = "previsto: motor r600 (mejora 054, B1; el primero)"; }
        else if (GPU_EM(evergreen, mod)) { r.familia = "AMD Evergreen"; r.detalle = "Radeon HD 5000, Ontario/Llano"; r.motor = "previsto después del R6xx (mejora 054, B2)"; }
        else if (GPU_EM(r5xx, mod)) { r.familia = "AMD R5xx"; r.detalle = "Radeon X1000 (R520/RV5xx), Xpress 1200"; r.motor = "previsto después (mejora 054, B2)"; }
        else if (GPU_EM(r300, mod)) { r.familia = "AMD R300/R420"; r.detalle = "Radeon 9500-X850, Xpress 200"; r.motor = "previsto al final (mejora 054, B2)"; }
        else if (GPU_EM(depois, mod)) { r.familia = "AMD posterior (Northern Islands, GCN, RDNA)"; r.detalle = "Radeon HD 6000 y posteriores"; r.motor = "solo por software (fuera del plan: amdgpu necesita PSP/SMU)"; }
        else if (GPU_EM(r100, mod)) { r.familia = "AMD R100/R200"; r.detalle = "Radeon 7000-9250, Rage"; }
    } else if (fab == 0x8086) {                            /* Intel */
        static const gpu_faixa_t g2[] = { { 0x7121, 0x7125 }, { 0x1132, 0x1132 }, { 0x3577, 0x3577 }, { 0x2562, 0x2562 },
                                          { 0x3582, 0x3582 }, { 0x2572, 0x2572 } };
        static const gpu_faixa_t g3[] = { { 0x2582, 0x2582 }, { 0x258A, 0x258A }, { 0x2592, 0x2592 }, { 0x2772, 0x2776 },
                                          { 0x27A2, 0x27A6 }, { 0x27AE, 0x27AE }, { 0x29B2, 0x29B3 }, { 0x29C2, 0x29C3 },
                                          { 0x29D2, 0x29D3 }, { 0xA001, 0xA002 }, { 0xA011, 0xA012 } };
        static const gpu_faixa_t g4[] = { { 0x2972, 0x2973 }, { 0x2982, 0x2983 }, { 0x2992, 0x2993 }, { 0x29A2, 0x29A3 },
                                          { 0x2A02, 0x2A03 }, { 0x2A12, 0x2A13 }, { 0x2A42, 0x2A43 }, { 0x2E02, 0x2E03 },
                                          { 0x2E12, 0x2E13 }, { 0x2E22, 0x2E23 }, { 0x2E32, 0x2E33 }, { 0x2E42, 0x2E43 },
                                          { 0x2E92, 0x2E93 } };
        static const gpu_faixa_t g5[] = { { 0x0042, 0x0042 }, { 0x0046, 0x0046 } };
        static const gpu_faixa_t g6[] = { { 0x0102, 0x0102 }, { 0x0106, 0x0106 }, { 0x010A, 0x010A }, { 0x0112, 0x0112 },
                                          { 0x0116, 0x0116 }, { 0x0122, 0x0122 }, { 0x0126, 0x0126 } };
        static const gpu_faixa_t depois[] = { { 0x0152, 0x01FF }, { 0x0400, 0x0DFF }, { 0x1600, 0x16FF }, { 0x1900, 0x19FF },
                                              { 0x22B0, 0x22BF }, { 0x3E00, 0x3EFF }, { 0x4600, 0x4FFF }, { 0x5600, 0x56FF },
                                              { 0x5900, 0x5A7F }, { 0x8700, 0x87FF }, { 0x9A00, 0x9BFF }, { 0xA700, 0xA7FF } };
        if (GPU_EM(g3, mod)) { r.familia = "Intel Gen3"; r.detalle = "GMA 900/950/3100/3150 (i915, i945, G33, Pineview)"; r.motor = "previsto: motor i915 (mejora 055, v8)"; }
        else if (GPU_EM(g6, mod)) { r.familia = "Intel Gen6"; r.detalle = "HD 2000/3000 (Sandy Bridge)"; r.motor = "previsto más adelante (mejora 055)"; }
        else if (GPU_EM(g2, mod)) { r.familia = "Intel Gen2"; r.detalle = "i810/i815/i830-i865"; }
        else if (GPU_EM(g4, mod)) { r.familia = "Intel Gen4"; r.detalle = "GMA 3000/X3100/X3500/X4500 (i965, GM45, G45)"; }
        else if (GPU_EM(g5, mod)) { r.familia = "Intel Gen5"; r.detalle = "HD Graphics (Ironlake, Clarkdale/Arrandale)"; }
        else if (GPU_EM(depois, mod)) { r.familia = "Intel posterior (Gen7 o más)"; r.detalle = "Ivy Bridge y posteriores"; }
    } else {
        r.familia = "otra";
        r.detalle = "no es AMD ni Intel";
    }
    return r;
}

#endif
