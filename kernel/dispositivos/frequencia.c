// SPDX-License-Identifier: GPL-2.0-or-later
/* frequencia.c - driver de frecuencia de la CPU con límite de temperatura
 * (D106, mejora 061).
 *
 * Separado del monitor de hardware (sensores.c, D092), que sigue siendo de
 * solo lectura. Este driver es el único que escribe en un MSR, y solo en el
 * registro de «estado deseado»:
 *   - AMD familia 10h en adelante (HwPstate, CPUID 80000007H:EDX[7]):
 *     PStateCtl (0xC0010062) = número de P-state, entre los que la BIOS
 *     declaró (0xC0010064 + n con PstateEn) y nunca más rápido que
 *     PStateCurLim (0xC0010061), el límite de la propia plataforma;
 *   - Intel con EIST desde Nehalem (familia 6, modelo >= 1Ah): el
 *     multiplicador de IA32_PERF_CTL (0x199, bits 15:8), entre el mínimo y el
 *     máximo sin turbo de MSR_PLATFORM_INFO (0xCE). Los Core 2 y anteriores
 *     quedan fuera: allí el mismo registro lleva el VID (la tensión) y aquí no
 *     se toca nunca una tensión; sin las tablas _PSS de ACPI (mejora 030) no
 *     hay valores de la BIOS que copiar.
 * Nada de FID/VID a mano (K8), nada en máquinas virtuales, nada de turbo
 * forzado ni de frecuencias inventadas.
 *
 * Seguridad (mantenedor, 2026-11-08):
 *   - límite de 79 °C: al llegar, el estado más lento hasta bajar de 72 °C;
 *     el límite se puede bajar, nunca subir (frequencia_pol.c);
 *   - la temperatura se lee cada 250 ms con las fuentes del monitor; sin
 *     lectura no se pasa del estado que dejó la BIOS (salvo que se fuerce);
 *   - cada escritura pasa por msr_escrever_seguro(): un #GP se absorbe y el
 *     driver queda desactivado hasta reiniciar;
 *   - se desactiva con la cabecera de arranque (mkimagem.py --sin-frecuencia),
 *     con «desactivar=si» en /home/.frecuencia o /etc/frecuencia, o con
 *     «frecuencia --desactivar»: vuelve al estado de la BIOS y no escribe más.
 * Varias CPU (D175, con mkimagem.py --smp): los registros de P-state son de
 * cada CPU y un wrmsr solo cambia la CPU que lo ejecuta, así que nunca se
 * escribe el de otra. La CPU que decide (la tarea, en la CPU 0, o una llamada
 * al sistema) deja en fc[c] el estado deseado de cada CPU y sube su
 * secuencia; cada CPU lo aplica ella misma en su tic (frequencia_tic_cpu,
 * con el cerrojo general tomado) y lee allí su propio registro de estado. La
 * política decide por núcleo físico (cpu_nucleo_fisico: los hilos SMT de un
 * núcleo reciben el mismo estado) o por paquete; el límite de temperatura es
 * uno y manda sobre todas. Un #GP en cualquier CPU desactiva el driver en
 * todas, y desactivarlo devuelve cada CPU al estado en que la dejó la BIOS.
 * Los ventiladores no se tocan (los lleva el EC del
 * portátil). Sin avisos emergentes: el estado se ve en INFO_FREQUENCIA
 * (Administrador de tareas, «sensores», «frecuencia») y en el log. */
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "frequencia.h"
#include "frequencia_pol.h"
#include "info_hardware.h"
#include "memoria.h"
#include "msr.h"
#include "processo.h"
#include "relogio.h"
#include "sensores_conv.h"
#include "sistema.h"
#include "texto.h"
#include "trava_geral.h"
#include "vfs.h"

#define MSR_PLATFORM_INFO   0xCE
#define MSR_PERF_STATUS     0x198
#define MSR_PERF_CTL        0x199
#define MSR_MISC_ENABLE     0x1A0
#define MSR_PM_ENABLE       0x770     /* HWP: con el bit 0, la CPU decide sola e ignora PERF_CTL */
#define MSR_AMD_PSTATE_LIM  0xC0010061u
#define MSR_AMD_PSTATE_CTL  0xC0010062u
#define MSR_AMD_PSTATE_ST   0xC0010063u
#define MSR_AMD_PSTATE0     0xC0010064u
#define MSR_INEXISTENTE     0x0BADF00Du   /* para la prueba del #GP (solo simulado) */

#define PERIODO_MS 250

static info_frequencia_t inf;
static fpol_t pol;
static uint32_t fam, mod;
static uint8_t mult[FREQ_MAX_ESTADOS];   /* Intel: multiplicador de cada estado */
static int ult_limitada = -1;
static bool tarefa_ligada;
/* Simulación (imagen de prueba): nada toca el hardware salvo la prueba del #GP.
 * Una tabla en memoria hace de registro de cada CPU. */
static int sim_temp = 50000, sim_uso = -1;
static int sim_status[CPU_MAX], sim_uso_cpu[CPU_MAX];
static int sim_gp_cpu = -1;   /* la CPU cuya próxima escritura va al MSR inexistente */

/* Cada CPU (D175). Lo escribe quien decide y lo lee la propia CPU, siempre
 * con el cerrojo general tomado (D165): no hace falta otro cerrojo. */
#define NCPU (CPU_MAX < FPOL_MAX_NUCLEOS ? CPU_MAX : FPOL_MAX_NUCLEOS)
typedef struct {
    volatile uint32_t seq_pedido;   /* sube cuando hay algo que hacer en esa CPU */
    volatile uint32_t seq_feito;    /* la CPU lo iguala cuando lo ha hecho */
    int      desejado;              /* estado que tiene que escribir (-1 = solo leer el suyo) */
    int      atual;                 /* su registro de estado (-1 = aún sin leer) */
    int      pedido;                /* lo último que escribió (-1 = nada) */
    int      bios;                  /* el que dejó la BIOS en ella (-1 = aún sin leer) */
    int      alvo;                  /* lo que decidió la política */
    int      rapido;                /* su PStateCurLim (el más rápido permitido) */
    uint32_t escritas, uso;
    uint32_t tiques_ant, ociosos_ant;
} freq_cpu_t;
static freq_cpu_t fc[NCPU];
static fpol_nucleo_t fn[NCPU];

static inline void cpuid(uint32_t f, uint32_t s, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(s));
}

static void nao_disponivel(const char *motivo)
{
    inf.estado = FREQ_EST_NAO_DISPONIVEL;
    inf.mecanismo = FREQ_MEC_NENHUM;
    inf.qtd_estados = 0;
    txt_copia_n(inf.motivo, motivo, sizeof inf.motivo);
}

static void por_estado(uint32_t khz, const char *nome)
{
    if (inf.qtd_estados >= FREQ_MAX_ESTADOS) return;
    inf.estado_khz[inf.qtd_estados] = khz;
    txt_copia_n(inf.estado_nome[inf.qtd_estados], nome, sizeof inf.estado_nome[0]);
    inf.qtd_estados++;
}

/* ------------------------------------------------------------ AMD */

static bool detectar_amd(uint32_t x7d)
{
    uint64_t v;
    if (fam < 0x10) {
        nao_disponivel(fam == 0x0F ? "AMD K8: los P-states son FID/VID a mano (tensiones): no se tocan"
                                   : "familia de AMD sin P-states por hardware");
        return false;
    }
    if (!((x7d >> 7) & 1)) { nao_disponivel("la CPU no anuncia HwPstate (CPUID 80000007H)"); return false; }
    if (!msr_ler_seguro(MSR_AMD_PSTATE_LIM, &v) || !msr_ler_seguro(MSR_AMD_PSTATE_ST, &v)) {
        nao_disponivel("los MSR de P-state (0xC0010061/63) no responden");
        return false;
    }
    msr_ler_seguro(MSR_AMD_PSTATE_LIM, &v);
    uint32_t maxval = (uint32_t)((v >> 4) & 7);
    /* En 10h, 15h y 16h con Core Performance Boost, los estados de boost van
     * delante en la tabla (0xC0010064+) pero no en la numeración que se
     * escribe en PStateCtl: el desfase es la diferencia entre los estados
     * habilitados y PstateMaxVal + 1. Solo afecta a la frecuencia mostrada. */
    uint32_t hab = 0, boost = 0;
    for (uint32_t i = 0; i < 8; i++) {
        if (!msr_ler_seguro(MSR_AMD_PSTATE0 + i, &v) || !(v >> 63)) break;
        hab++;
    }
    if ((fam == 0x10 || fam == 0x15 || fam == 0x16) && ((x7d >> 9) & 1) && hab > maxval + 1) boost = hab - (maxval + 1);
    for (uint32_t i = 0; i <= maxval && i + boost < 8; i++) {
        if (!msr_ler_seguro(MSR_AMD_PSTATE0 + i + boost, &v) || !(v >> 63)) break;
        char n[8];
        snformatar(n, sizeof n, "P%u", i);
        por_estado(sconv_amd_pstate_khz(fam, v), n);
    }
    if (inf.qtd_estados < 2) {
        nao_disponivel("la BIOS solo declara un P-state: no hay nada que elegir");
        return false;
    }
    msr_ler_seguro(MSR_AMD_PSTATE_ST, &v);
    inf.bios = (int)(v & 7);
    if (inf.bios >= (int)inf.qtd_estados) inf.bios = (int)inf.qtd_estados - 1;
    inf.mecanismo = FREQ_MEC_AMD;
    snformatar(inf.mecanismo_txt, sizeof inf.mecanismo_txt, "AMD familia %xh: PStateCtl (MSR 0xC0010062)", fam);
    return true;
}

/* ------------------------------------------------------------ Intel */

static bool detectar_intel(uint32_t c1c, uint32_t c6a)
{
    uint64_t v;
    if (fam != 6 || mod < 0x1A || sconv_intel_core2(mod)) {
        nao_disponivel(fam == 6 && mod >= 0x0D ? "Intel anterior a Nehalem: PERF_CTL incluye la tensión (VID) y no se toca"
                                                : "Intel sin EIST conocido");
        return false;
    }
    if (!((c1c >> 7) & 1)) { nao_disponivel("la CPU no anuncia EIST (CPUID 01H:ECX[7])"); return false; }
    if (!msr_ler_seguro(MSR_MISC_ENABLE, &v) || !((v >> 16) & 1)) {
        nao_disponivel("EIST desactivado por la BIOS (IA32_MISC_ENABLE[16])");
        return false;
    }
    if (((c6a >> 7) & 1) && msr_ler_seguro(MSR_PM_ENABLE, &v) && (v & 1)) {
        nao_disponivel("HWP activo: la CPU elige su frecuencia sola");
        return false;
    }
    if (!msr_ler_seguro(MSR_PLATFORM_INFO, &v)) { nao_disponivel("MSR_PLATFORM_INFO (0xCE) no responde"); return false; }
    uint32_t max = (uint32_t)((v >> 8) & 0xFF), min = (uint32_t)((v >> 40) & 0xFF);
    if (!max) { nao_disponivel("MSR_PLATFORM_INFO sin multiplicador máximo"); return false; }
    if (!min || min > max) min = max;
    if (min == max) { nao_disponivel("un solo multiplicador: no hay nada que elegir"); return false; }
    uint64_t c, st;
    if (!msr_ler_seguro(MSR_PERF_CTL, &c) || !msr_ler_seguro(MSR_PERF_STATUS, &st)) {
        nao_disponivel("IA32_PERF_CTL/STATUS (0x199/0x198) no responden");
        return false;
    }
    /* El TSC va al multiplicador máximo sin turbo: de ahí el reloj base. */
    uint32_t tsc = hw_tsc_khz(), base = tsc ? tsc / max : 100000;
    uint32_t n = max - min + 1;
    for (uint32_t k = 0; k < FREQ_MAX_ESTADOS && k < n; k++) {
        uint32_t m = n <= FREQ_MAX_ESTADOS ? max - k : max - (k * (max - min) + (FREQ_MAX_ESTADOS - 1) / 2) / (FREQ_MAX_ESTADOS - 1);
        char nome[8];
        snformatar(nome, sizeof nome, "x%u", m);
        mult[inf.qtd_estados] = (uint8_t)m;
        por_estado(m * base, nome);
    }
    uint32_t pedido = (uint32_t)((c >> 8) & 0xFF);
    if (!pedido) pedido = (uint32_t)((st >> 8) & 0xFF);
    inf.bios = (int)inf.qtd_estados - 1;
    for (uint32_t k = 0; k < inf.qtd_estados; k++)
        if (mult[k] <= pedido) { inf.bios = (int)k; break; }
    inf.mecanismo = FREQ_MEC_INTEL;
    txt_copia_n(inf.mecanismo_txt, "Intel EIST: IA32_PERF_CTL (MSR 0x199)", sizeof inf.mecanismo_txt);
    return true;
}

/* ------------------------------------------------------------ simulación */

static void detectar_simulado(void)
{
    /* Como el HP dv7 (dos P-states, la BIOS en el lento) más uno intermedio,
     * para que «a demanda» tenga escalones. */
    por_estado(2100000, "P0");
    por_estado(1600000, "P1");
    por_estado(1050000, "P2");
    inf.bios = 2;
    /* Cada CPU con su registro en memoria. La CPU 1 la deja «la BIOS» en P1
     * para comprobar que cada una vuelve al suyo al desactivar (D175). */
    for (int c = 0; c < NCPU; c++) {
        sim_status[c] = c == 1 ? 1 : 2;
        sim_uso_cpu[c] = -1;
    }
    inf.mecanismo = FREQ_MEC_SIMULADO;
    txt_copia_n(inf.mecanismo_txt, "simulado (imagen de prueba, sin hardware)", sizeof inf.mecanismo_txt);
}

/* ------------------------------------------------------------ hardware */

static int ler_atual(void)
{
    uint64_t v;
    if (inf.mecanismo == FREQ_MEC_SIMULADO) {
        uint32_t c = cpu_atual();
        return c < NCPU ? sim_status[c] : -1;
    }
    if (inf.mecanismo == FREQ_MEC_AMD) {
        if (!msr_ler_seguro(MSR_AMD_PSTATE_ST, &v)) return -1;
        int p = (int)(v & 7);
        return p < (int)inf.qtd_estados ? p : (int)inf.qtd_estados - 1;
    }
    if (inf.mecanismo == FREQ_MEC_INTEL) {
        if (!msr_ler_seguro(MSR_PERF_STATUS, &v)) return -1;
        uint32_t m = (uint32_t)((v >> 8) & 0xFF);
        for (uint32_t k = 0; k < inf.qtd_estados; k++)   /* el primero que no pase del actual (turbo = el 0) */
            if (mult[k] <= m) return (int)k;
        return (int)inf.qtd_estados - 1;
    }
    return -1;
}

static int ler_rapido(void)
{
    uint64_t v;
    if (inf.mecanismo == FREQ_MEC_AMD && msr_ler_seguro(MSR_AMD_PSTATE_LIM, &v)) {
        int r = (int)(v & 7);   /* CurPstateLimit: el más rápido que permite la plataforma ahora */
        return r < (int)inf.qtd_estados ? r : (int)inf.qtd_estados - 1;
    }
    return 0;
}

static void falhar(const char *onde)
{
    inf.estado = FREQ_EST_FALHOU;
    snformatar(inf.motivo, sizeof inf.motivo, "#GP al escribir %s: driver desactivado hasta reiniciar", onde);
    klog("[FREQ] #GP al escribir %s: driver desactivado (la CPU se queda en el estado en que este)\n", onde);
}

/* El estado de la BIOS en la CPU que ejecuta esto (en Intel, lo que pide su
 * PERF_CTL; si no, su registro de estado). -1 = no se puede leer. */
static int ler_bios_aqui(void)
{
    if (inf.mecanismo == FREQ_MEC_INTEL) {
        uint64_t c, st;
        uint32_t pedido = 0;
        if (msr_ler_seguro(MSR_PERF_CTL, &c)) pedido = (uint32_t)((c >> 8) & 0xFF);
        if (!pedido && msr_ler_seguro(MSR_PERF_STATUS, &st)) pedido = (uint32_t)((st >> 8) & 0xFF);
        if (!pedido) return -1;
        for (uint32_t k = 0; k < inf.qtd_estados; k++)
            if (mult[k] <= pedido) return (int)k;
        return (int)inf.qtd_estados - 1;
    }
    return ler_atual();
}

/* Escribe el estado e en el registro de LA CPU QUE EJECUTA ESTO (un wrmsr no
 * llega a otra). Con el cerrojo general y las interrupciones apagadas. */
static bool escrever(int e)
{
    bool ok = true;
    uint32_t cpu = cpu_atual();
    if (e < 0 || e >= (int)inf.qtd_estados || cpu >= NCPU) return false;
    if (inf.mecanismo == FREQ_MEC_SIMULADO) {
        if (sim_gp_cpu == (int)cpu) {
            sim_gp_cpu = -1;
            /* Un MSR que no existe: con KVM (y en una CPU real) da #GP y lo
             * absorbe el tratador; el QEMU sin KVM lo ignora, y entonces el
             * fallo se simula para probar el mismo camino. */
            bool escreveu = msr_escrever_seguro(MSR_INEXISTENTE, 0);
            klog("[FREQ] prueba: escritura en 0x%x -> %s (CPU %u)\n", MSR_INEXISTENTE,
                 escreveu ? "aceptada (sin #GP: fallo simulado)" : "#GP absorbido", cpu);
            falhar(escreveu ? "MSR 0xBADF00D (simulado)" : "MSR 0xBADF00D");
            return false;
        }
        sim_status[cpu] = e;
    } else if (inf.mecanismo == FREQ_MEC_AMD) {
        ok = msr_escrever_seguro(MSR_AMD_PSTATE_CTL, (uint64_t)(uint32_t)e);
        if (!ok) falhar("PStateCtl (0xC0010062)");
    } else if (inf.mecanismo == FREQ_MEC_INTEL) {
        uint64_t v;
        /* Solo el campo del multiplicador; el resto del registro, como estaba. */
        ok = msr_ler_seguro(MSR_PERF_CTL, &v);
        if (ok) ok = msr_escrever_seguro(MSR_PERF_CTL, (v & ~0xFF00ull) | ((uint64_t)mult[e] << 8));
        if (!ok) falhar("IA32_PERF_CTL (0x199)");
    } else {
        return false;
    }
    if (ok) {
        fc[cpu].pedido = e;
        fc[cpu].escritas++;
        if (cpu == 0) inf.pedido = e;
        inf.escritas++;
    }
    return ok;
}

static bool cpu_ligada(uint32_t c)
{
    if (c == 0) return true;
    if (c >= NCPU || c >= cpu_numero()) return false;
    cpu_dados_t *d = cpu_dados(c);
    return d && d->estado != CPU_LIVRE;
}

static uint32_t qtd_cpus(void)
{
    uint32_t n = cpu_numero();
    if (n < 1) n = 1;
    return n > NCPU ? NCPU : n;
}

/* Lo pendiente de la CPU que ejecuta esto: leer su estado de la BIOS la
 * primera vez, escribir el estado deseado (nunca más rápido que su
 * PStateCurLim) y leer su registro de estado. Con el cerrojo general y las
 * interrupciones apagadas. */
static void aplicar_aqui(void)
{
    uint32_t c = cpu_atual();
    if (c >= NCPU) return;
    freq_cpu_t *f = &fc[c];
    uint32_t seq = f->seq_pedido;
    if (inf.mecanismo != FREQ_MEC_NENHUM && inf.estado != FREQ_EST_FALHOU && inf.estado != FREQ_EST_NAO_DISPONIVEL) {
        if (f->bios < 0) {
            f->bios = ler_bios_aqui();
            if (c && f->bios >= 0)
                klog("[FREQ] CPU %u: la BIOS la dejo en %s\n", c, inf.estado_nome[f->bios]);
        }
        f->rapido = ler_rapido();
        if (inf.mecanismo == FREQ_MEC_SIMULADO && sim_gp_cpu == (int)c && inf.estado == FREQ_EST_ATIVO) {
            escrever(f->pedido == 0 ? 1 : 0);   /* la prueba del #GP en esta CPU */
        } else {
            int e = f->desejado;
            if (e >= 0 && f->bios >= 0) {
                if (e < f->rapido) e = f->rapido;
                if (e != f->pedido && !(f->pedido < 0 && e == ler_atual())) escrever(e);
            }
        }
        f->atual = ler_atual();
        if (c == 0) inf.atual = f->atual;
    }
    f->seq_feito = seq;
}

/* El tic de cada CPU (D175): si tiene algo pendiente, lo hace ella misma. Sin
 * nada pendiente cuesta una comparación; con algo, toma el cerrojo general
 * solo si está libre (si no, en el tic siguiente). */
void frequencia_tic_cpu(void)
{
    uint32_t c = cpu_atual();
    if (c >= NCPU || fc[c].seq_pedido == fc[c].seq_feito) return;
    if (!trava_geral_tentar()) return;
    aplicar_aqui();
}

/* Pide a la CPU c que haga lo pendiente: si es esta, ahora mismo. */
static void avisar(uint32_t c)
{
    fc[c].seq_pedido++;
    if (c == cpu_atual()) aplicar_aqui();
}

/* ------------------------------------------------------------ ciclo */

static void graus(char *b, size_t n, int mc)
{
    if (mc == FREQ_TEMP_ND) txt_copia_n(b, "sin lectura", n);
    else snformatar(b, n, "%d C", (mc + 500) / 1000);
}

/* El uso de cada CPU desde el periodo anterior: sus tiques y sus tiques
 * ociosos (cpu_dados, D157). */
static void medir_usos(uint32_t n)
{
    for (uint32_t c = 0; c < n; c++) {
        cpu_dados_t *d = cpu_dados(c);
        if (!d || !cpu_ligada(c)) continue;
        uint32_t t = d->tiques, oc = d->tiques_ociosos;
        uint32_t dt = t - fc[c].tiques_ant, doc = oc - fc[c].ociosos_ant;
        fc[c].tiques_ant = t;
        fc[c].ociosos_ant = oc;
        if (!dt) continue;
        if (doc > dt) doc = dt;
        fc[c].uso = 100 - doc * 100 / dt;
    }
}

/* Un periodo de decisión. Con las interrupciones desactivadas (lo llaman la
 * tarea, en la CPU 0, y las llamadas al sistema, en cualquier CPU). Decide
 * para todas las CPU y deja a cada una su estado; la que ejecuta esto aplica
 * el suyo ya, las demás en su tic. */
static void ciclo(bool novo_periodo)
{
    int t = FREQ_TEMP_ND;
    bool ok;
    uint32_t n = qtd_cpus();
    if (inf.mecanismo == FREQ_MEC_SIMULADO) {
        ok = sim_temp != FREQ_TEMP_ND;
        t = sim_temp;
    } else {
        ok = sensores_temp_cpu(&t);
    }
    if (ok && !fpol_temp_valida(t)) ok = false;
    inf.temp_mc = ok ? t : FREQ_TEMP_ND;
    if (novo_periodo) medir_usos(n);
    unsigned usos[NCPU];
    int dom[NCPU], alvos[NCPU];
    for (uint32_t c = 0; c < n; c++) {
        usos[c] = fc[c].uso;
        if (inf.mecanismo == FREQ_MEC_SIMULADO && sim_uso_cpu[c] >= 0) usos[c] = (unsigned)sim_uso_cpu[c];
        else if (inf.mecanismo == FREQ_MEC_SIMULADO && sim_uso >= 0) usos[c] = (unsigned)sim_uso;
        dom[c] = (int)cpu_nucleo_fisico(c);
        fn[c].bios = cpu_ligada(c) ? fc[c].bios : -1;
    }
    inf.uso_cpu = usos[0];
    if (cpu_atual() == 0) {   /* el registro de la CPU 0 se lee en la CPU 0 */
        fc[0].atual = inf.atual = ler_atual();
        fc[0].rapido = ler_rapido();
    }
    if (inf.estado != FREQ_EST_ATIVO) {
        /* Desactivado: cada periodo, que cada CPU relea su estado (y aplique
         * la vuelta a la BIOS si aún no lo hizo). */
        if (novo_periodo)
            for (uint32_t c = 0; c < n; c++)
                if (cpu_ligada(c)) avisar(c);
        return;
    }

    pol.rapido = fc[0].rapido;
    fpol_decidir_nucleos(&pol, fn, dom, (int)n, ok, t, novo_periodo ? usos : NULL, alvos);
    char g[16];
    graus(g, sizeof g, inf.temp_mc);
    if (ult_limitada != (int)pol.limitada) {
        if (pol.limitada)
            klog("[FREQ] limitada por temperatura: %s >= limite %d C; estado mas lento (%s)%s\n", g, pol.limite_mc / 1000,
                 inf.estado_nome[inf.qtd_estados - 1], n > 1 ? " en todas las CPU" : "");
        else if (ult_limitada == 1)
            klog("[FREQ] fin de la limitacion por temperatura: %s < %d C\n", g, fpol_retorno_mc(&pol) / 1000);
        ult_limitada = pol.limitada;
    }
    static const char *const nomes[] = { "rendimiento", "ahorro", "a demanda" };
    for (uint32_t c = 0; c < n; c++) {
        if (!cpu_ligada(c)) continue;
        int alvo = alvos[c];
        fc[c].alvo = alvo;
        if (alvo >= 0 && alvo != fc[c].desejado) {
            /* Sin escritura previa y con la CPU ya en el estado decidido, no
             * hay nada que contar en el log (ni que escribir). */
            int antes = fc[c].pedido >= 0 ? fc[c].pedido : fc[c].atual;
            if (alvo != antes) {
                char quem[12] = "";
                if (c) snformatar(quem, sizeof quem, "CPU %u: ", c);
                klog("[FREQ] %s%s -> %s (%s, %s, uso %u%%%s%s)\n", quem, antes >= 0 ? inf.estado_nome[antes] : "?",
                     inf.estado_nome[alvo], nomes[pol.politica <= 2 ? pol.politica : 0], g, usos[c],
                     pol.limitada ? ", limitada" : "", !ok && !pol.forcado ? ", sin temperatura: no mas que la BIOS" : "");
            }
            fc[c].desejado = alvo;
            avisar(c);
        } else if (novo_periodo || fc[c].bios < 0) {
            avisar(c);   /* que relea su estado (o el de la BIOS, la primera vez) */
        }
    }
}

static void tarefa_frequencia(void *arg)
{
    (void)arg;
    static const char canal = 0;
    for (;;) {
        int_desliga();
        if (inf.estado == FREQ_EST_ATIVO || inf.estado == FREQ_EST_DESATIVADO) ciclo(true);
        processo_atual()->acordar_em = relogio_tiques() + PERIODO_MS * TIQUES_POR_SEGUNDO / 1000;
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

/* ------------------------------------------------------------ configuración */

static int politica_por_nome(const char *v)
{
    if (txt_compara(v, "rendimiento") == 0 || txt_compara(v, "rendimento") == 0) return FPOL_RENDIMENTO;
    if (txt_compara(v, "ahorro") == 0 || txt_compara(v, "economia") == 0) return FPOL_ECONOMIA;
    if (txt_compara(v, "demanda") == 0 || txt_compara(v, "a-demanda") == 0) return FPOL_DEMANDA;
    return -1;
}

static bool sim(const char *v)
{
    return txt_compara(v, "si") == 0 || txt_compara(v, "sí") == 0 || txt_compara(v, "1") == 0;
}

static void desativar(uint32_t origem);

/* /home/.frecuencia (persistente) o /etc/frecuencia: líneas «clave=valor»
 * (politica=rendimiento|ahorro|demanda, limite=75, desactivar=si). Forzar
 * sin temperatura no se guarda: tiene que pedirse en cada arranque. */
static void ler_config(void)
{
    static const char *const caminhos[] = { "/home/.frecuencia", "/etc/frecuencia" };
    for (int c = 0; c < 2; c++) {
        uint8_t *txt;
        uint32_t tam;
        if (vfs_ler_inteiro(caminhos[c], &txt, &tam) < 0) continue;
        char *s = kmalloc(tam + 1);
        if (!s) { kfree(txt); return; }
        memcpy(s, txt, tam);
        s[tam] = 0;
        kfree(txt);
        for (char *l = s; *l;) {
            char *fim = l;
            while (*fim && *fim != '\n') fim++;
            char guarda = *fim;
            *fim = 0;
            char *ig = l;
            while (*ig && *ig != '=') ig++;
            if (*l != '#' && *ig == '=') {
                *ig = 0;
                char *v = ig + 1;
                for (char *p = v; *p; p++)
                    if (*p == '\r' || *p == ' ') { *p = 0; break; }
                if (txt_compara(l, "politica") == 0 && politica_por_nome(v) >= 0) pol.politica = politica_por_nome(v);
                else if (txt_compara(l, "limite") == 0) {
                    int g = 0;
                    for (char *p = v; *p >= '0' && *p <= '9'; p++) g = g * 10 + (*p - '0');
                    if (!fpol_mudar_limite(&pol, g * 1000))
                        klog("[FREQ] %s: limite %s rechazado (de 40 a 79 C)\n", caminhos[c], v);
                } else if (txt_compara(l, "coordinacion") == 0) {
                    if (txt_compara(v, "paquete") == 0) pol.coordenacao = FPOL_PAQUETE;
                    else if (txt_compara(v, "nucleo") == 0 || txt_compara(v, "núcleo") == 0) pol.coordenacao = FPOL_POR_NUCLEO;
                } else if (txt_compara(l, "desactivar") == 0 && sim(v)) {
                    desativar(2);
                }
            }
            *fim = guarda;
            l = *fim ? fim + 1 : fim;
        }
        kfree(s);
        klog("[FREQ] configuracion leida de %s\n", caminhos[c]);
        return;
    }
}

static void desativar(uint32_t origem)
{
    if (inf.estado != FREQ_EST_ATIVO) return;
    inf.estado = FREQ_EST_DESATIVADO;
    inf.origem_desativado = origem;
    /* Cada CPU vuelve al estado en que la dejó la BIOS si se llegó a cambiar;
     * lo que aún no había aplicado se olvida. Esta CPU, ya; las demás, en su
     * tic (D175). */
    uint32_t n = qtd_cpus(), ligadas = 0;
    for (uint32_t c = 0; c < n; c++) {
        if (!cpu_ligada(c)) continue;
        ligadas++;
        fc[c].desejado = fc[c].pedido >= 0 && fc[c].pedido != fc[c].bios ? fc[c].bios : -1;
        avisar(c);
    }
    if (inf.estado == FREQ_EST_FALHOU) return;
    static const char *const de[] = { "", "opcion de arranque", "archivo de configuracion", "comando" };
    klog("[FREQ] desactivado (%s): la CPU queda en %s, como la dejo la BIOS%s\n", de[origem <= 3 ? origem : 0],
         inf.estado_nome[inf.bios], ligadas > 1 ? " (cada CPU en el suyo)" : "");
}

/* ------------------------------------------------------------ arranque */

void frequencia_iniciar(void)
{
    uint32_t r[4], c1c = 0, c6a = 0, x7d = 0;
    memset(&inf, 0, sizeof inf);
    inf.versao = 2;
    memset(fc, 0, sizeof fc);
    for (int c = 0; c < NCPU; c++) {
        fc[c].desejado = fc[c].atual = fc[c].pedido = fc[c].bios = fc[c].alvo = -1;
        fpol_nucleo_iniciar(&fn[c], -1);
    }
    inf.atual = inf.pedido = -1;
    inf.temp_mc = FREQ_TEMP_ND;
    inf.periodo_ms = PERIODO_MS;
    uint8_t cab = cabecalho_boot[CAB_FREQ];
    bool simulado = cab & CAB_FREQ_SIMULADA;

    cpuid(0, 0, r);
    uint32_t max = r[0];
    bool intel = r[1] == 0x756E6547 && r[3] == 0x49656E69 && r[2] == 0x6C65746E;
    bool amd = r[1] == 0x68747541 && r[3] == 0x69746E65 && r[2] == 0x444D4163;
    bool hv = false, tem_msr = false;
    if (max >= 1) {
        cpuid(1, 0, r);
        uint32_t bf = (r[0] >> 8) & 0xF, bm = (r[0] >> 4) & 0xF;
        fam = bf == 0xF ? bf + ((r[0] >> 20) & 0xFF) : bf;
        mod = (bf == 6 || bf == 0xF) ? (((r[0] >> 16) & 0xF) << 4) + bm : bm;
        c1c = r[2];
        hv = (r[2] >> 31) & 1;
        tem_msr = (r[3] >> 5) & 1;
    }
    if (max >= 6) { cpuid(6, 0, r); c6a = r[0]; }
    cpuid(0x80000000u, 0, r);
    if (r[0] >= 0x80000007u && r[0] < 0x8000FFFFu) { cpuid(0x80000007u, 0, r); x7d = r[3]; }

    bool i0 = int_salva_desliga();
    bool achou;
    if (simulado) { detectar_simulado(); achou = true; }
    else if (hv) { nao_disponivel("máquina virtual: el hipervisor no expone los P-states"); achou = false; }
    else if (!tem_msr) { nao_disponivel("la CPU no tiene MSR"); achou = false; }
    else if (amd) achou = detectar_amd(x7d);
    else if (intel) achou = detectar_intel(c1c, c6a);
    else { nao_disponivel("fabricante sin control de frecuencia conocido"); achou = false; }

    if (!achou) {
        int_restaura(i0);
        klog("[FREQ] control de frecuencia no disponible: %s\n", inf.motivo);
        return;
    }
    inf.estado = FREQ_EST_ATIVO;
    fpol_iniciar(&pol, (int)inf.qtd_estados, inf.bios);
    pol.rapido = fc[0].rapido = ler_rapido();
    inf.atual = fc[0].atual = ler_atual();
    fc[0].bios = inf.bios;
    fpol_nucleo_iniciar(&fn[0], inf.bios);
    {
        char lista[96];
        int k = 0;
        lista[0] = 0;
        for (uint32_t i = 0; i < inf.qtd_estados && k < (int)sizeof lista - 16; i++)
            k += snformatar(lista + k, sizeof lista - (size_t)k, "%s%s %u MHz", i ? ", " : "", inf.estado_nome[i],
                            inf.estado_khz[i] / 1000);
        klog("[FREQ] %s; estados: %s; BIOS %s, actual %s, permitido hasta %s\n", inf.mecanismo_txt, lista,
             inf.estado_nome[inf.bios], inf.atual >= 0 ? inf.estado_nome[inf.atual] : "?", inf.estado_nome[pol.rapido]);
    }
    {
        uint32_t n = qtd_cpus(), ligadas = 0;
        for (uint32_t c = 0; c < n; c++) {
            cpu_dados_t *d = cpu_dados(c);
            if (!d || !cpu_ligada(c)) continue;
            ligadas++;
            fc[c].tiques_ant = d->tiques;
            fc[c].ociosos_ant = d->tiques_ociosos;
            if (c) avisar(c);   /* que lea el estado en que la dejó la BIOS */
        }
        if (ligadas > 1)
            klog("[FREQ] %u CPU: cada una escribe su propio registro en su tic; coordinacion %s\n", ligadas,
                 pol.coordenacao == FPOL_PAQUETE ? "por paquete" : "por nucleo");
    }
    if (cab & CAB_FREQ_DESATIVAR) desativar(1);
    else ler_config();
    if (inf.estado == FREQ_EST_ATIVO) ciclo(false);
    int_restaura(i0);
    if (!tarefa_ligada) { tarefa_ligada = true; tarefa_kernel("frecuencia", tarefa_frequencia, NULL); }
}

/* ------------------------------------------------------------ llamadas */

static void copiar_estado(void)
{
    inf.politica = (uint32_t)pol.politica;
    inf.rapido = pol.rapido;
    inf.limite_mc = pol.limite_mc;
    inf.retorno_mc = fpol_retorno_mc(&pol);
    inf.limitada = pol.limitada && inf.estado == FREQ_EST_ATIVO;
    inf.vezes_limitada = pol.vezes_limitada;
    inf.forcado = pol.forcado;
    inf.temp_simulada = inf.mecanismo == FREQ_MEC_SIMULADO;
    inf.coordenacao = (uint32_t)pol.coordenacao;
    uint32_t ligadas = 0;
    for (uint32_t c = 0; c < qtd_cpus(); c++) ligadas += cpu_ligada(c);
    inf.cpus = ligadas;
}

bool frequencia_info_kernel(info_frequencia_t *f)
{
    bool i0 = int_salva_desliga();
    copiar_estado();
    *f = inf;
    int_restaura(i0);
    return inf.versao != 0;
}

int32_t frequencia_info(uint32_t ptr, uint32_t n)
{
    if (n < sizeof(info_frequencia_t) || !usuario_ok((void *)ptr, sizeof(info_frequencia_t), true)) return ERRO_ENDERECO;
    bool i0 = int_salva_desliga();
    copiar_estado();
    memcpy((void *)ptr, &inf, sizeof inf);
    int_restaura(i0);
    return 0;
}

/* INFO_FREQ_NUCLEOS (D175): una entrada por CPU. */
int32_t frequencia_info_nucleos(uint32_t ptr, uint32_t n)
{
    if (inf.versao == 0 || inf.estado == FREQ_EST_NAO_DISPONIVEL) return ERRO_NAO_EXISTE;
    uint32_t q = qtd_cpus();
    if (n > q) n = q;
    if (!n) return 0;
    if (!usuario_ok((void *)ptr, n * sizeof(info_freq_nucleo_t), true)) return ERRO_ENDERECO;
    info_freq_nucleo_t *v = (info_freq_nucleo_t *)ptr;
    bool i0 = int_salva_desliga();
    for (uint32_t c = 0; c < n; c++) {
        info_freq_nucleo_t e;
        memset(&e, 0, sizeof e);
        e.cpu = c;
        e.ligada = cpu_ligada(c);
        e.nucleo = cpu_nucleo_fisico(c);
        e.atual = fc[c].atual;
        e.pedido = fc[c].pedido;
        e.alvo = fc[c].alvo;
        e.bios = fc[c].bios;
        e.uso = fc[c].uso;
        if (inf.mecanismo == FREQ_MEC_SIMULADO && sim_uso_cpu[c] >= 0) e.uso = (uint32_t)sim_uso_cpu[c];
        else if (inf.mecanismo == FREQ_MEC_SIMULADO && sim_uso >= 0) e.uso = (uint32_t)sim_uso;
        e.khz = e.atual >= 0 && e.atual < (int)inf.qtd_estados ? inf.estado_khz[e.atual] : 0;
        e.escritas = fc[c].escritas;
        e.pendente = e.ligada && fc[c].seq_pedido != fc[c].seq_feito;
        v[c] = e;
    }
    int_restaura(i0);
    return (int32_t)n;
}

int32_t sis_frequencia(uint32_t op, uint32_t arg, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    int32_t r = 0;
    bool i0 = int_salva_desliga();
    bool simulado = inf.mecanismo == FREQ_MEC_SIMULADO;
    if (inf.mecanismo == FREQ_MEC_NENHUM) r = ERRO_NAO_EXISTE;
    else if (op == FREQ_OP_POLITICA) {
        if (arg > FPOL_DEMANDA) r = ERRO_INVALIDO;
        else {
            pol.politica = (int)arg;
            pol.calmos = 0;
            for (int k = 0; k < NCPU; k++) fn[k].calmos = 0;
            static const char *const nomes[] = { "rendimiento", "ahorro", "a demanda" };
            klog("[FREQ] politica: %s\n", nomes[arg]);
        }
    } else if (op == FREQ_OP_LIMITE) {
        if (!fpol_mudar_limite(&pol, (int)arg)) {
            klog("[FREQ] limite %d C rechazado: de 40 a 79 C (no se puede subir de 79)\n", (int)arg / 1000);
            r = ERRO_INVALIDO;
        } else {
            klog("[FREQ] limite de temperatura: %d C (vuelve a subir por debajo de %d C)\n", pol.limite_mc / 1000,
                 fpol_retorno_mc(&pol) / 1000);
        }
    } else if (op == FREQ_OP_DESATIVAR) {
        if (inf.estado == FREQ_EST_FALHOU) r = ERRO_ACESSO;
        else desativar(3);
    } else if (op == FREQ_OP_ATIVAR) {
        if (inf.estado == FREQ_EST_FALHOU) r = ERRO_ACESSO;
        else if (inf.estado == FREQ_EST_DESATIVADO) {
            inf.estado = FREQ_EST_ATIVO;
            inf.origem_desativado = 0;
            inf.pedido = -1;
            for (int k = 0; k < NCPU; k++) fc[k].pedido = fc[k].desejado = -1;
            ult_limitada = -1;
            klog("[FREQ] activado de nuevo\n");
        }
    } else if (op == FREQ_OP_FORCAR) {
        pol.forcado = arg != 0;
        klog("[FREQ] %s\n", pol.forcado ? "FORZADO: sube aunque no se pueda leer la temperatura (riesgo del usuario)"
                                        : "sin forzar: sin temperatura, no mas que la BIOS");
    } else if (op == FREQ_OP_SIM_TEMP && simulado) {
        sim_temp = (int)arg;
        char g[16];
        graus(g, sizeof g, sim_temp);
        klog("[FREQ] simulado: temperatura %s\n", g);
    } else if (op == FREQ_OP_SIM_USO && simulado) {
        sim_uso = (int)arg > 100 ? 100 : (int)arg;
    } else if (op == FREQ_OP_SIM_GP && simulado) {
        /* La próxima escritura de esa CPU va al MSR inexistente; la hace ella
         * misma (ya, si es esta; si no, en su tic). */
        if (arg >= NCPU || !cpu_ligada(arg)) r = ERRO_INVALIDO;
        else {
            sim_gp_cpu = (int)arg;
            if (inf.estado == FREQ_EST_ATIVO) avisar(arg);
        }
    } else if (op == FREQ_OP_COORDENACAO) {
        if (arg > FREQ_COORD_PAQUETE) r = ERRO_INVALIDO;
        else {
            pol.coordenacao = (int)arg;
            for (int k = 0; k < NCPU; k++) fn[k].calmos = 0;
            klog("[FREQ] coordinacion: %s\n", arg == FREQ_COORD_PAQUETE ? "por paquete (el mismo estado en todas)" : "por nucleo");
        }
    } else if (op == FREQ_OP_SIM_USO_CPU && simulado) {
        uint32_t c = arg >> 16, u = arg & 0xFFFF;
        if (c >= NCPU) r = ERRO_INVALIDO;
        else sim_uso_cpu[c] = u == 0xFFFF ? -1 : u > 100 ? 100 : (int)u;
    } else {
        r = (op >= FREQ_OP_SIM_TEMP && op <= FREQ_OP_SIM_GP) || op == FREQ_OP_SIM_USO_CPU ? ERRO_ACESSO : ERRO_INVALIDO;
    }
    if (r == 0 && (inf.estado == FREQ_EST_ATIVO || inf.estado == FREQ_EST_DESATIVADO)) ciclo(false);
    int_restaura(i0);
    return r;
}
