// SPDX-License-Identifier: GPL-2.0-or-later
/* intel_rps.c - la frecuencia de la GPU Intel Gen9 para el 2D (D434) y el 3D (D438, D448): un gobernador común.
 *
 * Sin i915 nadie pide reloj y la GPU se queda en lo que dejó el firmware (100 MHz en la UHD 620 del Dell, 11 veces
 * menos que RP0). D448 lo arregló solo para el 3D; el 2D seguía a 100 MHz (60 copias de pantalla: 48,7 ms frente a
 * 7,4 en RP0). Ahora los dos llaman aquí: quien manda trabajo pide RP0 (GEN6_RPNSWREQ = GEN9_FREQUENCY(RP0 x
 * GEN9_FREQ_SCALER), como gen6_rps_set de i915); la tarea «intel-rps» vuelve a lo que había pedido el firmware tras
 * 1 s sin trabajo de nadie, y al soltar el último usuario. El PCU sigue mandando: no pasa de sus límites de potencia
 * y temperatura. Solo Gen9 (los registros son los de i915 para Gen9: intel_rps.c, i915_reg.h). */
#include "intel_rps.h"
#include "console.h"
#include "prazo.h"
#include "processo.h"
#include "relogio.h"
#include "cpu.h"
#include "info_hardware.h"
#include "texto.h"

#define RPNSWREQ       0xA008u           /* GEN6_RPNSWREQ: Gen9, la frecuencia pedida en los bits 31:23 (x 50/3 MHz) */
#define RPSTAT1        0xA01Cu           /* GEN6_RPSTAT1: CAGF en los bits 31:23 (Gen9) */
#define RP_STATE_CAP   0x145998u         /* espejo del MCHBAR: RP0 7:0, RP1 15:8, RPn 23:16 (x 50 MHz) */
#define REPOUSO_MS     1000u

static volatile uint8_t *mmio;
static volatile bool alta;
static volatile uint32_t usuarios;
static volatile uint64_t ult_trabalho;
static bool pode, tocada, tarefa;
static uint32_t antes, rp0_req, rp0_mhz, n_subidas, n_descidas;

static inline uint32_t rd(uint32_t r)             { return *(volatile uint32_t *)(mmio + r); }
static inline void     wr(uint32_t r, uint32_t v) { *(volatile uint32_t *)(mmio + r) = v; }

/* Cada 200 ms: si nadie ha mandado trabajo en 1 s, vuelve a la frecuencia del firmware. */
static void tarefa_rps(void *arg)
{
    (void)arg;
    static const char canal = 0;
    for (;;) {
        int_desliga();
        if (alta && pode && mmio && hw_tsc_khz() && prazo_tsc() - ult_trabalho > (uint64_t)hw_tsc_khz() * REPOUSO_MS) {
            wr(RPNSWREQ, antes);
            alta = false;
            n_descidas++;
        }
        processo_atual()->acordar_em = relogio_tiques() + 200u * TIQUES_POR_SEGUNDO / 1000u;
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

bool intel_rps_ligar(volatile uint8_t *m, uint32_t quem)
{
    if (!m || !hw_tsc_khz()) return false;
    if (!mmio) {
        mmio = m;
        uint32_t cap = rd(RP_STATE_CAP);
        if (!(cap & 0xFFu) || (cap & 0xFFu) > 40u) {   /* RP0 de 50 MHz a 2 GHz: si no, no se toca */
            klog("[GPU] intel rps: RP0 no creible (RP_STATE_CAP %x): la frecuencia no se toca\n", cap);
            return false;
        }
        rp0_mhz = (cap & 0xFFu) * 50u;
        rp0_req = ((cap & 0xFFu) * 3u) << 23;
        pode = true;
        klog("[GPU] intel rps: RP0 %u MHz, RP1 %u MHz, RPn %u MHz; ahora %u MHz (pedida %x)\n", rp0_mhz, ((cap >> 8) & 0xFFu) * 50u,
             ((cap >> 16) & 0xFFu) * 50u, (rd(RPSTAT1) >> 23) * 50u / 3u, rd(RPNSWREQ));
    }
    if (!pode) return false;
    if (!tarefa) {
        tarefa = tarefa_kernel("intel-rps", tarefa_rps, NULL) != NULL;
        if (!tarefa) { klog("[GPU] intel rps: sin la tarea: no se pide RP0\n"); pode = false; return false; }
    }
    if (!usuarios && !tocada) { antes = rd(RPNSWREQ); tocada = true; }
    usuarios |= quem;
    return true;
}

void intel_rps_trabalho(void)
{
    if (!pode || !usuarios) return;
    /* Revisión (depurador-drivers, 2026-11-22): con las interrupciones cerradas, como la tarea. En 32 bits
     * «ult_trabalho» son dos escrituras: si el tic cortaba entre las dos (el 3D llama aquí con las interrupciones
     * abiertas), la tarea podía leer media marca vieja y bajar la frecuencia con trabajo recién mandado. */
    bool sim = int_salva_desliga();
    ult_trabalho = prazo_tsc();   /* antes de mirar «alta»: la tarea no baja con trabajo recién mandado */
    if (!alta) { wr(RPNSWREQ, rp0_req); alta = true; n_subidas++; }
    int_restaura(sim);
}

void intel_rps_soltar(uint32_t quem)
{
    bool sim = int_salva_desliga();   /* revisión: que la tarea no vea «alta» a medio cambiar */
    usuarios &= ~quem;
    if (!usuarios && tocada && mmio) {
        wr(RPNSWREQ, antes);
        alta = false;
        tocada = false;
    }
    int_restaura(sim);
}

uint32_t intel_rps_rp0_mhz(void) { return pode ? rp0_mhz : 0; }

void intel_rps_texto(char *b, int n)
{
    if (!mmio) { snformatar(b, (size_t)n, "frecuencia sin tocar"); return; }
    snformatar(b, (size_t)n, "frecuencia %u MHz (pedida %x), %s; RP0 %u MHz; %u subidas, %u vueltas al reposo; usuarios %s%s; la del firmware %x",
               (rd(RPSTAT1) >> 23) * 50u / 3u, rd(RPNSWREQ), !pode ? "sin tocar" : alta ? "en RP0" : "en reposo", rp0_mhz, n_subidas,
               n_descidas, (usuarios & INTEL_RPS_2D) ? "2D " : "", (usuarios & INTEL_RPS_3D) ? "3D" : "", antes);
}
