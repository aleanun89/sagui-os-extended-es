// SPDX-License-Identifier: GPL-2.0-or-later
/* gart_r700_unidad.c - prueba en el PC de la lógica de la GART de las R7xx (D406,
 * kernel/dispositivos/gart_r700_logica.h): dónde va la ventana, las entradas de la tabla y las
 * escrituras para encenderla y apagarla, contra los valores de Linux 6.6 (rv770_pcie_gart_enable,
 * rv770_pcie_gart_disable, rs600_gart_get_page_entry) escritos aquí a mano. */
#include <stdio.h>
#include "../kernel/dispositivos/gart_r700_logica.h"

static int falhas;
#define VERIFICA(c, m) do { if (!(c)) { printf("FALLO: %s\n", m); falhas++; } } while (0)

static uint32_t valor(const r7g_par_t *p, int n, uint32_t reg, int *vezes)
{
    uint32_t v = 0;
    *vezes = 0;
    for (int i = 0; i < n; i++) if (p[i].reg == reg) { v = p[i].valor; (*vezes)++; }
    return v;
}

int main(void)
{
    uint64_t i, f, g;
    bool mudar;
    /* El dv7: MC_VM_FB_LOCATION 1f0000 = VRAM 0-0x1FFFFFFF (512 MB). */
    VERIFICA(r7g_fb(0x1F0000u, &i, &f) && i == 0 && f == 0x1FFFFFFFull, "FB_LOCATION en unidades de 16 MB");
    VERIFICA(!r7g_fb(0xFFFFFFFFu, &i, &f) && !r7g_fb(0x000F0010u, &i, &f), "FB_LOCATION sin sentido");
    VERIFICA(r7g_janela(0x1F0000u, 0, 0x1FFFFu, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 0 && g == 0x20000000ull && !mudar,
             "ventana detras de la VRAM (como radeon_gtt_location), apertura que ya cubre la VRAM");
    VERIFICA(r7g_janela(0x1F0000u, 0, 0, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 0 && g == 0x20000000ull && mudar,
             "apertura del sistema a cero: hay que ponerla y la ventana sigue detras");
    VERIFICA(r7g_janela(0x1F0000u, 1, 0, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 0 && mudar, "apertura al reves: se pone");
    /* Apertura del sistema que cubre también donde iría la ventana: no. */
    VERIFICA(r7g_janela(0x1F0000u, 0, 0xFFFFFu, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 4, "ventana dentro de la apertura del sistema");
    /* AGP encendida justo detrás de la VRAM (unidades de 64 KB). */
    VERIFICA(r7g_janela(0x1F0000u, 0, 0x1FFFFu, 0x2000u, 0x2FFFu, 16u << 20, &g, &mudar) == 5, "ventana en la AGP");
    /* VRAM al final de los 4 GB (como las R5xx en c0000000-ffffffff): la ventana va delante. */
    VERIFICA(r7g_janela(0x00FF00C0u, 0xC0000u, 0xFFFFFu, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 0 && g == 0xBF000000ull,
             "VRAM hasta 4 GB: la ventana delante");
    VERIFICA(r7g_janela(0x00FF0000u, 0, 0xFFFFFu, 0x0FFFFFFFu, 0x0FFFFFFFu, 16u << 20, &g, &mudar) == 3, "sin sitio");

    /* Entradas (R600_PTE_*: válida 1, sistema 2, snoop 4, lectura 0x20, escritura 0x40). */
    VERIFICA(r7g_pte(0x12345678ull) == 0x12345067ull, "entrada de una pagina de RAM");
    VERIFICA(r7g_pte(0x123456789000ull) == 0x123456789067ull, "entrada por encima de 4 GB");
    VERIFICA(r7g_pte_relleno(0x00ABC000ull) == 0x00ABC002ull, "relleno: solo «sistema», como la dummy de Linux");

    /* Encender (rv770_pcie_gart_enable). */
    r7g_par_t p[R7G_MAX_PARES];
    int n = r7g_ligar(false, 0x20000000ull, 0x20FFFFFFull, 0x00740000ull, 0x00ABC000ull, p), k;
    VERIFICA(n == 21 && n <= R7G_MAX_PARES, "21 escrituras (sin la RV740)");
    VERIFICA(r7g_ligar(true, 0, 0, 0, 0, p) == 22, "22 con la RV740 (MD_L1_TLB3)");
    n = r7g_ligar(false, 0x20000000ull, 0x20FFFFFFull, 0x00740000ull, 0x00ABC000ull, p);
    VERIFICA(p[0].reg == 0x1400 && p[0].valor == 0x1C203u, "VM_L2_CNTL primero: caché, fragmentos, LRU por escritura, cola 7");
    VERIFICA(valor(p, n, 0x1404, &k) == 0 && k == 1 && valor(p, n, 0x1408, &k) == 0x80u, "VM_L2_CNTL2 0, CNTL3 modo 2");
    uint32_t l1 = 0x16801Bu;
    VERIFICA(r7g_l1() == l1, "L1: TLB, fragmentos, NOT_IN_SYS, PASS_THRU, tamano y cola 5");
    VERIFICA(valor(p, n, 0x2654, &k) == l1 && valor(p, n, 0x2658, &k) == l1 && valor(p, n, 0x265C, &k) == l1 &&
             valor(p, n, 0x2234, &k) == l1 && valor(p, n, 0x2240, &k) == l1, "las TLB L1 MD y MB");
    VERIFICA(valor(p, n, 0x2698, &k) == 0 && k == 0, "MD_L1_TLB3 solo en la RV740");
    VERIFICA(valor(p, n, 0x155C, &k) == 0x20000u && valor(p, n, 0x157C, &k) == 0x20FFFu && valor(p, n, 0x153C, &k) == 0x740u,
             "inicio, fin y tabla en paginas de 4 KB");
    VERIFICA(valor(p, n, 0x1410, &k) == 0x11u && k == 1 && valor(p, n, 0x1518, &k) == 0xABCu, "contexto 0 encendido y pagina de fallo");
    int pos_ctx = -1, pos_base = -1;
    for (int j = 0; j < n; j++) { if (p[j].reg == 0x1410) pos_ctx = j; if (p[j].reg == 0x153C) pos_base = j; }
    VERIFICA(pos_base >= 0 && pos_ctx > pos_base, "el contexto se enciende despues de poner la tabla");
    VERIFICA(valor(p, n, 0x1414, &k) == 0 && k == 1 && valor(p, n, 0x1428, &k) == 0 && k == 1, "contextos 1 a 6 apagados");

    uint32_t r[R7G_MAX_PARES];
    VERIFICA(r7g_regs_tocados(false, r) == 21 && r[0] == 0x1400, "registros que se guardan antes");

    /* Apagar (rv770_pcie_gart_disable). */
    n = r7g_desligar(p);
    VERIFICA(n == 17 && p[0].reg == 0x1410 && p[0].valor == 0 && p[6].reg == 0x1428, "apagar: primero los 7 contextos");
    VERIFICA(valor(p, n, 0x1400, &k) == 0x1C002u && valor(p, n, 0x2654, &k) == 0x168000u, "apagar: L2 sin cache y L1 sin TLB");
    uint32_t lo, hi;
    r7g_aper(0x1F0000u, &lo, &hi);
    VERIFICA(lo == 0 && hi == 0x1FFFFu, "apertura del sistema como rv770_mc_program");

    if (falhas) { printf("gart_r700: %d fallos\n", falhas); return 1; }
    printf("gart_r700: ventana, entradas y registros de encender y apagar: todo bien\n");
    return 0;
}
