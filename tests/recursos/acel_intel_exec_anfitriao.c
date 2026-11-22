/* acel_intel_exec_anfitriao.c - prueba en el anfitrión de la lógica pura de Gen9-Gen12 por execlists
 * (D434, h434). Usa el MISMO acel_intel_logica.h que el kernel: imagen de registros del contexto
 * (la de i915), descriptor, PPGTT, PAT, secuencia de ELSP/ELSQ, CSB, marca, memoria robada, PTE de la
 * GGTT, plano de Gen9+ y órdenes del blitter.
 * Sin hardware: lo que no se puede probar aquí (la respuesta real del motor) va en h434 como
 * «sin probar». Sale con 0 si todo cuadra. */
#include <stdio.h>
#include <string.h>
#include "../../kernel/dispositivos/acel_intel_logica.h"

static int falhas;

static void confere(int ok, const char *txt)
{
    printf("%s %s\n", ok ? "ok" : "FALHA", txt);
    if (!ok) falhas++;
}

static void modelos(void)
{
    int exec = 0;
    for (uint32_t m = 0; m <= 0xFFFF; m++)
        if (intel_variante_exec((uint16_t)m, NULL)) exec++;
    confere(exec == 147, "147 identificadores exactos de execlists (Gen9 86, Gen11 15, Gen12 46)");
    confere(intel_variante_exec(0x46A6, NULL) == INTEL_VAR_TGL, "i3-1220P: 0x46A6 es Gen12");
    confere(intel_variante_exec(0x1916, NULL) == INTEL_VAR_SKL && intel_var_geracao(INTEL_VAR_SKL) == 9,
            "Skylake: Gen9 (variante SKL)");
    confere(intel_var_geracao(INTEL_VAR_ICL) == 11 && intel_var_geracao(INTEL_VAR_TGL) == 12, "Ice Lake Gen11, Tiger Gen12");
    confere(intel_var_geracao(INTEL_VAR_BDW) == 8 && intel_var_geracao(INTEL_VAR_IVB) == 7, "Gen7 y Gen8 no cambian");
    confere(intel_variante(0x1916, NULL) == INTEL_VAR_NADA, "el driver de Gen3-8 no acepta IDs de Gen9");
    confere(intel_familia_sem_2d(0x9B41, NULL, NULL) == 0 && intel_familia_sem_2d(0x46A8, NULL, NULL) == 0,
            "Comet Lake y Alder Lake-P no son «sin 2D»");
    const char *p = NULL;
    confere(intel_familia_sem_2d(0x0A84, NULL, &p) == 9 && p && strstr(p, "D434"), "Apollo Lake: Gen9 sin lista");
}

static void contexto(void)
{
    static uint32_t pag[1024];
    confere(intel_lrc_tabela(9) && intel_lrc_tabela(9) == intel_lrc_tabela(11) && intel_lrc_tabela(12) &&
            intel_lrc_tabela(12) != intel_lrc_tabela(9), "tablas de i915: Gen9 y Gen11 la misma (gen9_xcs), Gen12 la suya");
    confere(!intel_lrc_tabela(8) && !intel_lrc_tabela(10), "Gen8 y Gen10 no van por aquí");
    confere(!intel_lrc_monta(pag, 1024, 8, 0x22000u, 0, 0, 0, 0, 0), "Gen8: la imagen se niega");
    confere(!intel_lrc_monta(pag, 512, 9, 0x22000u, 0, 0, 0, 0, 0), "una página demasiado pequeña: se niega");
    confere(intel_lrc_monta(pag, 1024, 9, 0x22000u, 0, 0x00000, 0x00AB0000u, 0x00000001u, 0x0000000123456000ull),
            "Gen9: imagen montada");
    confere(pag[0] == 0 && pag[1] == ((0x22u << 23) | 27u | (1u << 12)), "Gen9: NOP y MI_LOAD_REGISTER_IMM(14) con FORCE_POSTED");
    /* La dirección de cada registro es la base del motor + el desplazamiento (set_offsets de i915). */
    confere(pag[2] == 0x22244u && pag[4] == 0x22034u && pag[6] == 0x22030u && pag[8] == 0x22038u && pag[10] == 0x2203Cu,
            "Gen9: CONTEXT_CONTROL, HEAD, TAIL, START y CTL con la dirección completa (0x22000 + desplazamiento)");
    confere(pag[28] == 0x221C8u && pag[29] == 0 && pag[30] == 0 && pag[33] == ((0x22u << 23) | 17u | (1u << 12)),
            "Gen9: tras los 14 pares, NOP(3) y LRI(9) en la palabra 0x21");
    confere(pag[0x22] == 0x223A8u && pag[0x24] == 0x2228Cu && pag[0x32] == 0x22270u,
            "Gen9: CTX_TIMESTAMP en 0x22 y PDP3_UDW..PDP0_LDW en 0x24..0x32");
    confere(pag[0x41] == ((0x22u << 23) | 1u | (1u << 12)) && pag[0x42] == 0x22200u, "Gen9: LRI(1) de 0x200 en 0x41");
    confere(pag[0x51] == ((0x22u << 23) | 87u | (1u << 12)) && pag[0x52] == 0x22028u && pag[0x54] == 0x2209Cu &&
            pag[0x50 + 2 * 44] == 0x22068u, "Gen9: LRI(44) en 0x51, de 0x028 a 0x068 (MI_MODE en 0x54)");
    confere(pag[0x52 + 2 * 44] == (0x0Au << 23), "Gen9: la imagen se cierra con MI_BATCH_BUFFER_END");
    confere(pag[3] == 0x000F0009u, "Gen9: CONTEXT_CONTROL con el inhibidor de carga y sin los de guardado (0x000F0009)");
    confere(pag[9] == 0x00AB0000u && pag[11] == 0x00000001u, "Gen9: RING_START y RING_CTL (4 KB, válido)");
    confere(pag[0x31] == 0x1u && pag[0x33] == 0x23456000u && pag[0x25] == 0 && pag[0x2F] == 0,
            "Gen9: la PML4 en PDP0 (4 niveles); PDP1-3 a 0");
    confere(pag[0x55] == (1u << 24), "Gen9: RING_MI_MODE sin STOP_RING (con su máscara)");

    confere(intel_lrc_monta(pag, 1024, 11, 0x22000u, 0x40, 0x80, 0x00AB0000u, 1, 0x1000), "Gen11: imagen montada");
    confere(pag[1] == ((0x22u << 23) | 27u | (1u << 12) | (1u << 19)) && pag[2] == 0x22244u,
            "Gen11: LRI con LRM_CS_MMIO y la dirección completa");
    confere(pag[3] == 0x00090009u, "Gen11: CONTEXT_CONTROL sin tocar el guardado (0x00090009)");
    confere(pag[5] == 0x40 && pag[7] == 0x80, "Gen11: HEAD y TAIL puestos");
    confere(pag[0x52 + 2 * 44] == ((0x0Au << 23) | 1u), "Gen11: MI_BATCH_BUFFER_END con el bit 0");

    confere(intel_lrc_monta(pag, 1024, 12, 0x22000u, 0, 0, 0, 1, 0x5000) &&
            pag[1] == ((0x22u << 23) | 25u | (1u << 12) | (1u << 19)), "Gen12: LRI de 13 registros con LRM_CS_MMIO");
    confere(pag[2 + 2 * 11] == 0x22180u && pag[2 + 2 * 12] == 0x222B4u, "Gen12: 0x180 y 0x2B4 en la imagen");
    confere(pag[28] == 0 && pag[33] == ((0x22u << 23) | 17u | (1u << 12) | (1u << 19)) && pag[0x32] == 0x22270u &&
            pag[0x33] == 0x5000u, "Gen12: NOP(5), LRI(9) en 0x21 y la PML4 en PDP0");
    confere(pag[0x34] == ((0x0Au << 23) | 1u) && pag[0x61] == (1u << 24), "Gen12: cierre en 0x34 y MI_MODE (0x60) sin STOP_RING");
}

static void descritor(void)
{
    uint64_t d9 = intel_ctx_descritor(0x00123000u, 9, 3);
    confere((uint32_t)d9 == 0x00123119u && (d9 >> 32) == 1u,
            "descriptor Gen9: dirección | válido | privilegio | 4 niveles (3 << 3) y ctx id 1");
    uint64_t d12 = intel_ctx_descritor(0x00123000u, 12, 3);
    confere((uint32_t)d12 == 0x00123119u && (d12 >> 32) == ((1u << 5) | (3u << 29)),
            "descriptor Gen12: ctx id en 37, instancia 0 y clase COPY (3) en 61");
    confere(((uint32_t)intel_ctx_descritor(0x00123FFFu, 9, 3) & 0xFFFu) == 0x119u, "la dirección se alinea a 4 KB");
}

static void ppgtt(void)
{
    static uint32_t t[(3 + 8) * 1024];
    /* Pantalla en la gráfica 0x00FFF000 (cruza un límite de 2 MB) hasta 0x01400000. */
    uint32_t ini = 0x00FFF000u, fim = 0x01400000u;
    uint32_t n = intel_ppgtt_n_pt(ini, fim);
    confere(n == 3 && intel_ppgtt_n_pt(0, 0x200000u) == 1 && intel_ppgtt_n_pt(0x1000u, 0x1000u) == 0,
            "tablas de 2 MB necesarias para el tramo");
    confere(!intel_ppgtt_monta(t, n, 0x80000000ull, ini + 4, fim, 0x7C000000ull), "inicio sin alinear: se niega");
    confere(!intel_ppgtt_monta(t, n + 1, 0x80000000ull, ini, fim, 0x7C000000ull), "número de tablas que no cuadra: se niega");
    confere(!intel_ppgtt_monta(t, 1, 0x80000000ull, 0x3FFFF000u, 0x40001000u, 0), "más allá de 1 GB: se niega");
    confere(intel_ppgtt_monta(t, n, 0x180000000ull, ini, fim, 0x7C000000ull), "PPGTT montada");
    confere(t[0] == (0x80001000u | EX_PPGTT_BITS) && t[1] == 1u && t[2] == 0, "PML4[0] -> PDP (física de 64 bits)");
    confere(t[1024] == (0x80002000u | EX_PPGTT_BITS) && t[1025] == 1u, "PDP[0] -> PD");
    uint32_t pd0 = ini >> 21;
    confere(t[2048 + 2 * pd0] == (0x80003000u | EX_PPGTT_BITS) && t[2048 + 2 * (pd0 + 2)] == (0x80005000u | EX_PPGTT_BITS) &&
            t[2048 + 2 * (pd0 - 1)] == 0 && t[2048 + 2 * (pd0 + 3)] == 0, "PD: solo las 3 tablas del tramo");
    uint32_t e0 = (ini >> 12) & 511u;
    confere(t[3072 + 2 * e0] == (0x7C000000u | EX_PPGTT_BITS) && t[3072 + 2 * e0 - 2] == 0,
            "la primera página gráfica va a la física de la pantalla; la anterior, sin mapear");
    uint32_t ult = fim - 4096u, j = (ult >> 21) - pd0, e = (ult >> 12) & 511u;
    confere(t[3072 + 1024 * j + 2 * e] == ((0x7C000000u + (ult - ini)) | EX_PPGTT_BITS) &&
            t[3072 + 1024 * j + 2 * e + 2] == 0, "la última página del tramo, contigua; la siguiente, sin mapear");
    confere((EX_PPGTT_BITS & 0x1Bu) == 0x1Bu, "entradas: presente, escritura y PWT|PCD (índice 3 del PAT)");
    confere(intel_pat3_reg(9) == 0x40E0u && intel_pat3_valor(9, 0xAABBCCDDu) == 0x00BBCCDDu,
            "PAT Gen9: byte 3 de GEN8_PRIVATE_PAT_LO a UC");
    confere(intel_pat3_reg(11) == 0x40ECu && intel_pat3_reg(12) == 0x480Cu && intel_pat3_valor(12, 5) == 0,
            "PAT Gen11 (GEN10_PAT_INDEX(3)) y Gen12 (GEN12_PAT_INDEX(3)) a UC");
}

static void envio(void)
{
    uint32_t off[4], val[4];
    uint64_t d = intel_ctx_descritor(0x00123000u, 9, 3);
    int n = intel_envio_seq(d, false, off, val);
    confere(n == 4 && off[0] == 0x230 && off[3] == 0x230 && val[0] == 0 && val[1] == 0 && val[2] == 1 &&
            val[3] == 0x00123119u, "ELSP (Gen9): puerto 1 vacío, puerto 0 alto y bajo; el último dispara");
    n = intel_envio_seq(d, true, off, val);
    confere(n == 4 && off[2] == 0x510 && val[2] == 0x00123119u && off[3] == 0x514 && val[3] == 1u &&
            off[0] == 0x518 && off[1] == 0x51C, "ELSQ (Gen11+): puerto 0 en 0x510/0x514 y puerto 1 en 0x518/0x51C");
    confere(intel_exec_ocupado(0x10) && intel_exec_ocupado(1u << 18) && !intel_exec_ocupado(0) &&
            !intel_exec_ocupado(1u << 17), "EXECLIST_STATUS: válido o activo del puerto 0 = ocupado; el puerto 1 no cuenta");
}

static void csb(void)
{
    uint32_t w, r;
    intel_csb_ptrs((2u << 8) | 5u, 9, &w, &r);
    confere(w == 5 && r == 2 && intel_csb_novas(w, r, 9) == 3, "CSB Gen9: escritura 5, lectura 2, tres entradas nuevas");
    intel_csb_ptrs((0xAu << 8) | 0xBu, 11, &w, &r);
    confere(w == 0xB && r == 0xA && intel_csb_novas(w, r, 11) == 1, "CSB Gen11: punteros de 4 bits, una entrada");
    confere(intel_csb_novas(0, 5, 9) == 1, "CSB: la vuelta de 5 a 0 cuenta una entrada (anillo de 6)");
    confere(intel_csb_novas(7, 5, 9) == 0 && intel_csb_novas(0xF, 11, 12) == 0, "CSB: puntero fuera del anillo: ninguna");
    confere(intel_csb_avisar(5, 9) == 0x07000500u, "CSB: el aviso de lectura es la máscara 7<<8 con valor 5<<8");
    confere(intel_csb_reiniciar(9) == 0xFFFF0505u && intel_csb_reiniciar(12) == 0xFFFF0B0Bu,
            "CSB: puesta a cero como reset_csb_pointers (última entrada)");
    confere(intel_csb_novas(0, 5, 9) == 1 && intel_csb_novas(0, 11, 12) == 1, "CSB: tras la puesta a cero, la primera es la 0");
    confere(intel_csb_mmio(0) == 0x370u && intel_csb_mmio(5) == 0x398u && intel_csb_mmio(6) == 0x3C0u &&
            intel_csb_mmio(11) == 0x3E8u, "CSB en el MMIO: 0x370 (0-5) y 0x3C0 (6-11)");
    confere(intel_csb_terminou(0x08, 1, 9) && intel_csb_terminou(0x18, 1, 11) && !intel_csb_terminou(0x01, 1, 9) &&
            !intel_csb_terminou(0x10, 1, 9), "CSB Gen9-11: ACTIVE_IDLE (bit 3) = motor libre");
    /* Gen12: baja = contexto al que va (25:15), alta = el que deja (25:15). */
    uint32_t ocioso = 0x7FFu << 15, ctx1 = 1u << 15;
    confere(intel_csb_terminou(ocioso, ctx1, 12), "CSB Gen12: de ctx 1 a «ninguno» (0x7FF) = motor libre");
    confere(!intel_csb_terminou(ctx1 | 1u, ocioso, 12), "CSB Gen12: de «ninguno» a ctx 1 (cola nueva) = no");
    confere(!intel_csb_terminou(ocioso, ocioso, 12) && !intel_csb_terminou(0x08, 0, 12),
            "CSB Gen12: el formato de Gen9 no vale");
    uint32_t b[4];
    confere(intel_cmd_marca(b, 0x00ABC200u, 0x1234u) == 4 && b[0] == ((0x26u << 23) | 2u | (1u << 14)) &&
            b[1] == (0x00ABC200u | 4u) && b[2] == 0 && b[3] == 0x1234u,
            "marca: MI_FLUSH_DW con escritura de palabra en la GGTT (USE_GTT)");
}

static void robada(void)
{
    uint64_t b, t, rb;
    uint32_t rt;
    confere(intel_robada9(9, 0x0200u, 0x7C000001u, 0xFFFFFFFFu, &b, &t) && b == 0x7C000000ull && t == (64ull << 20),
            "robada Gen9: GMS 2 = 64 MB, base de BDSM (la mitad alta no cuenta)");
    confere(intel_robada9(12, 0xF100u, 0x7B800000u, 0x1u, &b, &t) && b == 0x17B800000ull && t == (8ull << 20),
            "robada Gen12: GMS 0xF1 = 8 MB (pasos de 4 MB) y base de 64 bits (0xC0/0xC4)");
    confere(intel_robada9(11, 0xF000u, 0x7F000000u, 0, &b, &t) && t == (4ull << 20), "robada Gen11: GMS 0xF0 = 4 MB");
    confere(!intel_robada9(12, 0x0000u, 0x7B800000u, 0, &b, &t), "robada sin tamaño: no se sabe");
    confere(!intel_reservado9(9, 0x7F800000u, 0, 0x80000000ull, &rb, &rt), "reservada Gen9 sin habilitar: no hay");
    confere(intel_reservado9(9, 0x7F800001u, 0, 0x80000000ull, &rb, &rt) && rb == 0x7F800000ull && rt == (8u << 20),
            "reservada Gen9: de la base al final de la robada");
    confere(intel_reservado9(12, 0x7F800080u, 0, 0x80000000ull, &rb, &rt) && rb == 0x7F800000ull && rt == (8u << 20),
            "reservada Gen12: sin bit de habilitar (icl_get_stolen_reserved)");
}

static void gtt_plano_ordenes(void)
{
    uint32_t lo, hi;
    intel_pte_gen9(0x123456000ull, 0x0Au, &lo, &hi);
    confere(lo == 0x2345600Bu && hi == 1u, "PTE: dirección 45:12 con válida y el molde de bits bajos");
    uint32_t c = (1u << 31) | (4u << 24);
    confere(intel_plano9_conferir(12, c, 0x400000u, 4096u / 64u, 0, 0x400000u, 4096u) == INTEL_PLANO_OK,
            "plano Gen9: XRGB8888 lineal con el pitch de la pantalla");
    confere(intel_plano9_conferir(12, c | (1u << 10), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_EM_TILES,
            "plano Gen9 en tiles: la CPU");
    confere(intel_plano9_conferir(12, (1u << 31) | (6u << 24), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_FORMATO_MAU,
            "plano Gen9 con otro formato: la CPU");
    confere(intel_plano9_conferir(12, c, 0x400000u, 32, 0, 0x400000u, 4096u) == INTEL_PLANO_PITCH_MAU,
            "plano Gen9 con otro pitch: la CPU");
    confere(intel_plano9_conferir(12, c, 0x400000u, 64, 0x10u, 0x400000u, 4096u) == INTEL_PLANO_OUTRO,
            "plano Gen9 con desplazamiento x/y: no es la pantalla");
    confere(intel_plano9_conferir(12, c, 0x500000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_OUTRO,
            "plano Gen9 en otra dirección: no muestra la pantalla");
    confere(intel_plano9_conferir(12, c | (1u << 20), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_FORMATO_MAU,
            "plano con ORDER_RGBX (XBGR8888): la CPU");
    confere(intel_plano9_conferir(12, c | (1u << 23), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_FORMATO_MAU &&
            intel_plano9_conferir(9, c | (1u << 23), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_OK,
            "plano Ice Lake+: el bit 23 es del formato; en Gen9 no");
    confere(intel_plano9_conferir(12, c | (1u << 15), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_FORMATO_MAU &&
            intel_plano9_conferir(12, c | (1u << 4), 0x400000u, 64, 0, 0x400000u, 4096u) == INTEL_PLANO_FORMATO_MAU,
            "plano comprimido (render o medios): la CPU");
    confere(intel_plano9_conferir(12, c, 0x400000u, 7680u / 64u, 0, 0x400000u, 7680u) == INTEL_PLANO_OK &&
            intel_plano9_conferir(12, c, 0x400000u, 0x800u | (4096u / 64u), 0, 0x400000u, 4096u) == INTEL_PLANO_PITCH_MAU,
            "plano: stride de 1920x4 bien; bits altos del stride cuentan");

    uint32_t b[INTEL_PAL_MAX + 4];
    int n = intel_cmd_relleno(b, 9, 4096, 0x400000u, 1, 2, 3, 4, 0xFF00FF00u);
    confere(n == 7 && (b[0] >> 29) == 2 && (b[0] & 0x3F) == 5, "relleno Gen9: 7 palabras, cabecera de 64 bits");
    n = intel_cmd_copia(b, 11, 4096, 0x400000u, 1, 2, 3, 4, 5, 6);
    confere(n == 10 && (b[0] & 0x3F) == 8, "copia Gen11: 10 palabras, direcciones de 64 bits");
    n = intel_cmd_flush(b, 12);
    confere(n == 4 && (b[0] >> 23) == 0x26, "vaciado Gen12: MI_FLUSH_DW de 4 palabras");
}

int main(void)
{
    modelos();
    contexto();
    descritor();
    ppgtt();
    envio();
    csb();
    robada();
    gtt_plano_ordenes();
    printf("%s: %d falhas\n", falhas ? "FALHOU" : "TODO OK", falhas);
    return falhas ? 1 : 0;
}
