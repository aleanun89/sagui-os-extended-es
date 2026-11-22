// SPDX-License-Identifier: GPL-2.0-or-later
/* planificadores.c - las políticas del planificador entre las CPU (D174).
 *
 * escalonar() (processos.c) hace el cambio de proceso; aquí se decide QUÉ
 * proceso toma cada CPU. Hay cuatro políticas, elegibles al arrancar
 * (mkimagem.py --planificador) y en caliente (SIS_PLANIFICADOR, la orden
 * "planificador"):
 *
 *   rr          round-robin global: cada CPU recorre la tabla desde el último
 *               que eligió y toma el primero listo. Es el de siempre y el de
 *               por defecto.
 *   afinidad    cada proceso vuelve a la CPU donde corrió (su caché sigue
 *               caliente). Otra CPU solo se lo lleva si ella no tiene nada
 *               suyo y él lleva FRIO_TIQUES sin correr (robo de trabajo).
 *   equilibrio  una cola por CPU (los listos cuya "casa" es esa CPU); el
 *               proceso nuevo va a la CPU menos cargada; cada
 *               PERIODO_EQUILIBRIO tics la CPU 0 pasa uno de la más cargada a
 *               la menos cargada; una CPU que se queda sin nada roba ya.
 *   prioridad   cada proceso tiene un nice (-20..19) y un uso reciente de
 *               CPU que decae: gana el de menor nice*2 + uso/2 - espera/2. Los
 *               interactivos (duermen antes de gastar la rebanada) tienen
 *               poco uso y pasan delante; al despertar, si hay una CPU con un
 *               proceso bastante peor, se le quita (IPI). El nice cambia
 *               también la rebanada.
 *
 * Las "colas" no son listas: son los procesos de la tabla con p->casa == cpu.
 * La tabla es la única verdad (estado, na_cpu); una lista aparte habría que
 * mantenerla en cada sitio que cambia el estado (bloquear, acordar, el
 * reloj, los futex, terminar...) y un descuido dejaría un proceso fuera para
 * siempre. Con 64 entradas, recorrerla cuesta menos que un cambio de CR3.
 *
 * Topología (SMT): en afinidad y equilibrio, si el hermano de esta CPU (otro
 * hilo del mismo núcleo físico) trabaja y hay un núcleo entero ocioso, el
 * proceso se deja para ese núcleo (como el reparto SMT de Linux).
 *
 * Alta prioridad de audio (D436, PLAN_AUDIO, con cualquier política): un hilo marcado que queda listo pasa
 * delante de los demás y, si su CPU corre otro, se le pide planificar ya. Solo mientras gaste poco (su uso
 * reciente, como mucho PLAN_AUDIO_MAX_USO tics por periodo): un hilo que acapara la CPU vuelve a ir por
 * turnos él solo, así que no hace falta ser root. Pensado para un hilo que mezcla sonido y duerme enseguida.
 *
 * Todo corre con el cerrojo general (D165). Solo "trocar" se lee sin él (en
 * el tic de un secundario), y por eso es volatile. */
#include "planificador.h"
#include "processo.h"
#include "cpu_dados.h"
#include "smp.h"
#include "relogio.h"
#include "console.h"
#include "sistema.h"
#include "texto.h"
#include "abi.h"
#include "info_hardware.h"

#define FRIO_TIQUES         3    /* afinidad: sin correr 30 ms, su caché ya no cuenta */
#define PERIODO_EQUILIBRIO  5    /* equilibrio: reparto cada 50 ms */
#define PERIODO_RECENTE     25   /* el uso reciente decae a la mitad cada 250 ms */
#define ESPERA_MAX          120  /* prioridad: la espera cuenta hasta 1,2 s */
#define MARGEM_PREEMPCAO    4    /* prioridad: quitar la CPU solo si el otro es bastante peor */

static uint32_t ativo = PLANIFICADOR_RR;

static const char *const nomes[PLANIFICADOR_MAX + 1] = {
    NULL, "rr", "afinidad", "equilibrio", "prioridad",
};

/* Datos por CPU de los planificadores: aparte de cpu_dados_t, que tiene que
 * caber en su línea de caché. */
static struct {
    volatile uint32_t trocar;    /* planificar ya (pedido con el cerrojo) */
    uint32_t ociosos_ant;        /* tiques_ociosos en el último reparto */
    uint32_t ociosos_delta;      /* tiques ociosos del último periodo */
} por_cpu[CPU_MAX];

/* ------------------------------------------------------------ utilidades */

const char *planificador_nome(uint32_t n) { return n <= PLANIFICADOR_MAX ? nomes[n] : NULL; }
uint32_t planificador_ativo(void) { return ativo; }

void planificador_iniciar(uint32_t pedido)
{
    if (pedido >= 1 && pedido <= PLANIFICADOR_MAX) {
        ativo = pedido;
        klog("[SAGUI] planificador: %s (opcion de arranque)\n", nomes[ativo]);
    } else {
        ativo = PLANIFICADOR_RR;
        klog("[SAGUI] planificador: %s (por defecto)\n", nomes[ativo]);
    }
}

uint32_t planificador_trocar_para(uint32_t n)
{
    if (n < 1 || n > PLANIFICADOR_MAX) return 0;
    uint32_t antes = ativo;
    ativo = n;
    if (antes != n) klog("[SAGUI] planificador: %s (antes %s)\n", nomes[n], nomes[antes]);
    return antes;
}

/* Las CPU que trabajan: la 0 siempre; un secundario, si arrancó. */
static uint32_t cpus_ativas(void)
{
    uint32_t m = 1;
    if (!smp_arrancadas()) return m;
    for (uint32_t c = 1; c < CPU_MAX; c++)
        if (cpu_dados(c)->estado != CPU_LIVRE) m |= 1u << c;
    return m;
}

bool planificador_pode_cpu(const processo_t *p, uint32_t cpu)
{
    /* Las tareas del kernel, solo en la CPU 0 (D165). */
    if (!p->usuario) return cpu == 0;
    uint32_t m = p->mascara_cpu;
    if (!m || (m & (1u << cpu))) return true;
    /* Una máscara sin ninguna CPU que trabaje no deja al proceso sin correr. */
    return (m & cpus_ativas()) == 0;
}

/* ¿Puede esta CPU tomar este proceso? (D165)
 *   - listo, y que no lo tenga otra CPU (na_cpu);
 *   - en una CPU que su máscara permite (las tareas del kernel, en la 0). */
static bool elegivel(const processo_t *c, uint32_t cpu, const processo_t *atual)
{
    if (c->estado != P_PRONTO || c->pid == 0) return false;
    if (c->na_cpu && c != atual) return false;
    return planificador_pode_cpu(c, cpu);
}

static bool casa_valida(const processo_t *p, uint32_t ativas)
{
    return p->casa < CPU_MAX && (ativas & (1u << p->casa)) && planificador_pode_cpu(p, p->casa);
}

static bool ociosa(uint32_t c) { return cpu_dados(c)->estado == CPU_OCIOSA; }

/* D436: ¿cuenta ahora su alta prioridad de audio? Solo si gasta poco. */
static bool audio_vale(const processo_t *p)
{
    if (!p->prio_audio || !p->usuario) return false;
    return p->recente + (p->tiques_cpu - p->tiques_vistos) <= PLAN_AUDIO_MAX_USO;
}

/* D436: el hilo de audio listo que lleva más tiempo esperando (NULL: ninguno). */
static processo_t *escolher_audio(uint32_t cpu, processo_t *atual)
{
    processo_t *m = NULL;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *c = processo_indice(i);
        if (!c->prio_audio || !elegivel(c, cpu, atual) || !audio_vale(c)) continue;
        if (!m || (int32_t)(c->saiu_em - m->saiu_em) < 0) m = c;
    }
    return m;
}

/* Despertar la CPU "c" al salir del kernel (kernel_saida, D165). */
static void avisar_cpu(uint32_t c)
{
    if (!cpu_gs_lista() || c == cpu_atual()) return;
    cpu_dados(cpu_atual())->acordar |= 1u << c;
}

void planificador_pedir_troca(uint32_t cpu)
{
    if (cpu >= CPU_MAX) return;
    por_cpu[cpu].trocar = 1;
    avisar_cpu(cpu);
}

bool planificador_troca_pendente(uint32_t cpu)
{
    return cpu < CPU_MAX && por_cpu[cpu].trocar;
}

/* ------------------------------------------------------------ topología */

/* ¿Trabaja algún hermano de "c" (otro hilo de su núcleo físico)? */
static bool irma_ocupada(uint32_t c, uint32_t ativas)
{
    for (uint32_t s = 0; s < CPU_MAX; s++)
        if (s != c && (ativas & (1u << s)) && cpu_irmas(c, s) && !ociosa(s)) return true;
    return false;
}

/* Si esta CPU tiene el hermano ocupado y hay otra CPU ociosa con todo su
 * núcleo ocioso que puede correr "p", esa (para que "p" tenga un núcleo
 * para él solo). -1 si no hay o no hace falta. */
static int nucleo_inteiro_para(const processo_t *p, uint32_t cpu, uint32_t ativas)
{
    if (!p->usuario || !irma_ocupada(cpu, ativas)) return -1;
    for (uint32_t d = 0; d < CPU_MAX; d++) {
        if (d == cpu || !(ativas & (1u << d)) || cpu_irmas(cpu, d)) continue;
        if (ociosa(d) && !irma_ocupada(d, ativas) && planificador_pode_cpu(p, d)) return (int)d;
    }
    return -1;
}

/* ------------------------------------------------------------ carga */

/* Procesos de cada CPU: los que corren en ella y los listos de su cola. */
static void calcular_carga(uint32_t carga[CPU_MAX], uint32_t ativas)
{
    for (uint32_t c = 0; c < CPU_MAX; c++) carga[c] = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->pid == 0) continue;
        if (p->na_cpu && p->cpu < CPU_MAX) carga[p->cpu]++;
        else if (p->estado == P_PRONTO && casa_valida(p, ativas)) carga[p->casa]++;
    }
}

static uint32_t carga_irmas(const uint32_t carga[CPU_MAX], uint32_t c, uint32_t ativas)
{
    uint32_t n = 0;
    for (uint32_t s = 0; s < CPU_MAX; s++)
        if (s != c && (ativas & (1u << s)) && cpu_irmas(c, s)) n += carga[s];
    return n;
}

/* La CPU menos cargada que puede correr "p": menos procesos; luego, menos
 * trabajo en sus hermanos (SMT); luego, más tiempo ociosa en el último
 * periodo (usa tiques_ociosos de cpu_dados). */
static int menos_carregada(const processo_t *p, const uint32_t carga[CPU_MAX], uint32_t ativas)
{
    int melhor = -1;
    uint32_t m_carga = 0, m_irmas = 0, m_ocio = 0;
    for (uint32_t c = 0; c < CPU_MAX; c++) {
        if (!(ativas & (1u << c)) || !planificador_pode_cpu(p, c)) continue;
        uint32_t ci = carga_irmas(carga, c, ativas), oc = por_cpu[c].ociosos_delta;
        if (melhor < 0 || carga[c] < m_carga || (carga[c] == m_carga && (ci < m_irmas ||
            (ci == m_irmas && oc > m_ocio)))) {
            melhor = (int)c; m_carga = carga[c]; m_irmas = ci; m_ocio = oc;
        }
    }
    return melhor;
}

/* ------------------------------------------------------------ prioridad */

static int prioridade(const processo_t *p, uint32_t agora)
{
    int uso = (int)(p->recente + (p->tiques_cpu - p->tiques_vistos));
    int pr = p->nice * 2 + uso / 2;
    if (p->estado == P_PRONTO && !p->na_cpu) {   /* espera en la cola: envejece */
        uint32_t e = agora - p->saiu_em;
        if (e > ESPERA_MAX) e = ESPERA_MAX;
        pr -= (int)e / 2;
    }
    return pr;
}

/* ------------------------------------------------------------ elegir */

static processo_t *escolher_rr(uint32_t cpu, processo_t *atual)
{
    int ini = (int)cpu_dados(cpu)->ultimo;
    for (int k = 1; k <= MAX_PROCESSOS; k++) {
        processo_t *c = processo_indice((ini + k) % MAX_PROCESSOS);
        if (elegivel(c, cpu, atual)) return c;
    }
    return NULL;
}

/* La parte común de afinidad y equilibrio: el siguiente de la cola de esta
 * CPU (los suyos y los que no tienen casa), por turnos. Si esta CPU tiene el
 * hermano ocupado y hay un núcleo entero ocioso, el proceso pasa a la cola
 * de ese núcleo y se lo despierta. */
static processo_t *escolher_da_fila(uint32_t cpu, processo_t *atual, uint32_t ativas)
{
    int ini = (int)cpu_dados(cpu)->ultimo;
    bool passou = false;   /* uno por vez: ese núcleo deja de estar ocioso */
    for (int k = 1; k <= MAX_PROCESSOS; k++) {
        processo_t *c = processo_indice((ini + k) % MAX_PROCESSOS);
        if (!elegivel(c, cpu, atual)) continue;
        if (c->casa != cpu && casa_valida(c, ativas)) continue;   /* de la cola de otra CPU */
        int d = passou ? -1 : nucleo_inteiro_para(c, cpu, ativas);
        if (d >= 0) {
            c->casa = (uint32_t)d;
            avisar_cpu((uint32_t)d);
            passou = true;
            continue;
        }
        return c;
    }
    return NULL;
}

/* El robo, cuando esta CPU no tiene nada suyo. Si hay un núcleo entero
 * ocioso y esta CPU tiene el hermano ocupado, que robe aquel. */
static processo_t *roubar(uint32_t cpu, processo_t *escolhido, uint32_t ativas)
{
    if (!escolhido) return NULL;
    int d = nucleo_inteiro_para(escolhido, cpu, ativas);
    if (d >= 0) {
        escolhido->casa = (uint32_t)d;
        avisar_cpu((uint32_t)d);
        return NULL;
    }
    return escolhido;
}

static processo_t *escolher_afinidade(uint32_t cpu, processo_t *atual, uint32_t agora, uint32_t ativas)
{
    processo_t *p = escolher_da_fila(cpu, atual, ativas);
    if (p) return p;
    /* Robo: el que lleva más tiempo sin correr, si ya está frío. */
    processo_t *melhor = NULL;
    uint32_t espera_max = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *c = processo_indice(i);
        if (c == atual || !elegivel(c, cpu, atual)) continue;
        uint32_t e = agora - c->saiu_em;
        if (e < FRIO_TIQUES) continue;
        if (!melhor || e > espera_max) { melhor = c; espera_max = e; }
    }
    return roubar(cpu, melhor, ativas);
}

static processo_t *escolher_equilibrio(uint32_t cpu, processo_t *atual, uint32_t agora, uint32_t ativas)
{
    processo_t *p = escolher_da_fila(cpu, atual, ativas);
    if (p) return p;
    /* Robo inmediato: de la cola más cargada; en ella, el que más espera. */
    uint32_t carga[CPU_MAX];
    calcular_carga(carga, ativas);
    processo_t *melhor = NULL;
    uint32_t m_carga = 0, m_espera = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *c = processo_indice(i);
        if (c == atual || !elegivel(c, cpu, atual)) continue;
        uint32_t cg = casa_valida(c, ativas) ? carga[c->casa] : 0;
        uint32_t e = agora - c->saiu_em;
        if (!melhor || cg > m_carga || (cg == m_carga && e > m_espera)) {
            melhor = c; m_carga = cg; m_espera = e;
        }
    }
    return roubar(cpu, melhor, ativas);
}

static processo_t *escolher_prioridade(uint32_t cpu, processo_t *atual, uint32_t agora)
{
    /* El de mejor prioridad; a igualdad, el primero por turnos desde el
     * último elegido (así los iguales se alternan, el actual el último). */
    int ini = (int)cpu_dados(cpu)->ultimo;
    processo_t *melhor = NULL;
    int m_pr = 0;
    for (int k = 1; k <= MAX_PROCESSOS; k++) {
        processo_t *c = processo_indice((ini + k) % MAX_PROCESSOS);
        if (!elegivel(c, cpu, atual)) continue;
        int pr = prioridade(c, agora);
        if (c->casa == cpu) pr--;   /* un poco de preferencia por su caché */
        if (!melhor || pr < m_pr) { melhor = c; m_pr = pr; }
    }
    return melhor;
}

processo_t *planificador_escolher(uint32_t cpu, processo_t *atual)
{
    if (cpu < CPU_MAX) por_cpu[cpu].trocar = 0;   /* la petición se atiende ahora */
    uint32_t agora = relogio_tiques();
    processo_t *a = escolher_audio(cpu, atual);   /* D436 */
    if (a) return a;
    switch (ativo) {
    case PLANIFICADOR_AFINIDAD:   return escolher_afinidade(cpu, atual, agora, cpus_ativas());
    case PLANIFICADOR_EQUILIBRIO: return escolher_equilibrio(cpu, atual, agora, cpus_ativas());
    case PLANIFICADOR_PRIORIDAD:  return escolher_prioridade(cpu, atual, agora);
    default:                      return escolher_rr(cpu, atual);
    }
}

uint32_t planificador_fatia(const processo_t *p)
{
    if (ativo != PLANIFICADOR_PRIORIDAD || !p) return QUANTUM_TIQUES;
    if (p->nice <= -10) return 4;
    if (p->nice < 0) return 3;
    if (p->nice >= 10) return 1;
    return QUANTUM_TIQUES;
}

/* ------------------------------------------------------------ ciclo de vida */

static inline uint64_t ler_tsc(void)
{
    uint32_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return ((uint64_t)d << 32) | a;
}

void planificador_tomou(processo_t *p, uint32_t cpu)
{
    if (p->execucoes && p->cpu != cpu) p->migracoes++;   /* cambió de CPU */
    p->execucoes++;
    p->casa = cpu;
    if (p->pronto_tsc) {
        /* De despertar a correr. Los TSC de las CPU van a la par (KVM y el
         * hardware de los últimos 15 años); un salto absurdo no se cuenta. */
        uint64_t dt = ler_tsc() - p->pronto_tsc;
        p->pronto_tsc = 0;
        if (dt < (1ull << 40)) {
            uint32_t kc = (uint32_t)(dt >> 10);
            if (kc > 1000000u) kc = 1000000u;
            if (p->lat_soma_kc > 0xF0000000u) {   /* un proceso de días: la media sigue valiendo */
                p->lat_soma_kc /= 2;
                p->lat_n /= 2;
            }
            p->lat_soma_kc += kc;
            if (kc > p->lat_max_kc) p->lat_max_kc = kc;
            p->lat_n++;
        }
    }
}

void planificador_herdar(processo_t *p, int pai)
{
    p->casa = CASA_NENHUMA;
    p->saiu_em = relogio_tiques() - ESPERA_MAX;   /* sin caché que cuidar */
    processo_t *pp = pai > 0 ? processo_por_pid(pai) : NULL;
    if (!pp) return;
    pp = processo_lider(pp);
    p->mascara_cpu = pp->mascara_cpu;   /* como en Unix: los hijos y los hilos heredan */
    p->nice = pp->nice;
}

void planificador_pronto_novo(processo_t *p)
{
    if (!p->usuario) { p->casa = 0; return; }
    if (ativo != PLANIFICADOR_EQUILIBRIO) return;   /* los demás: lo toma la primera CPU que pueda */
    uint32_t ativas = cpus_ativas(), carga[CPU_MAX];
    calcular_carga(carga, ativas);
    int c = menos_carregada(p, carga, ativas);
    if (c >= 0) p->casa = (uint32_t)c;
}

int planificador_ociosa_para(const processo_t *p, uint32_t eu, uint32_t marcadas)
{
    uint32_t ativas = cpus_ativas();
    int primeira = -1, inteira = -1;
    for (uint32_t c = 0; c < CPU_MAX; c++) {
        if (c == eu || !(ativas & (1u << c)) || (marcadas & (1u << c))) continue;
        if (!ociosa(c) || !planificador_pode_cpu(p, c)) continue;
        if (ativo == PLANIFICADOR_RR || ativo == PLANIFICADOR_PRIORIDAD) return (int)c;
        if (c == p->casa) return (int)c;   /* su CPU está libre */
        if (primeira < 0) primeira = (int)c;
        if (inteira < 0 && !irma_ocupada(c, ativas)) inteira = (int)c;
    }
    return inteira >= 0 ? inteira : primeira;
}

/* prioridad: "p" quedó listo. Si no hay CPU ociosa que pueda tomarlo y una
 * corre algo bastante peor, se le pide planificar ya. */
void planificador_despertou(processo_t *p)
{
    if (!p || p->pid == 0) return;
    if (!p->pronto_tsc) p->pronto_tsc = ler_tsc();   /* para medir la latencia */
    uint32_t ativas = cpus_ativas();
    if (audio_vale(p)) {
        /* D436: alta prioridad de audio. Con una CPU ociosa la despierta avisar_ociosa; si no, la primera que
         * corre algo sin esa prioridad planifica ya (en la CPU 0, al salir de este tic o de esta llamada). */
        int alvo_cpu = -1;
        for (uint32_t c = 0; c < CPU_MAX; c++) {
            if (!(ativas & (1u << c)) || !planificador_pode_cpu(p, c)) continue;
            processo_t *a = cpu_dados(c)->atual;
            if (!a || a->pid == 0 || ociosa(c)) return;
            if (a == p || audio_vale(a)) continue;
            if (alvo_cpu < 0) alvo_cpu = (int)c;
        }
        if (alvo_cpu >= 0) planificador_pedir_troca((uint32_t)alvo_cpu);
        return;
    }
    if (ativo != PLANIFICADOR_PRIORIDAD) return;
    uint32_t agora = relogio_tiques();
    int pp = prioridade(p, agora);
    int pior = -1, pior_pr = 0;
    for (uint32_t c = 0; c < CPU_MAX; c++) {
        if (!(ativas & (1u << c)) || !planificador_pode_cpu(p, c)) continue;
        cpu_dados_t *d = cpu_dados(c);
        processo_t *a = d->atual;
        if (!a || a->pid == 0 || ociosa(c)) return;   /* la despierta avisar_ociosa */
        if (a == p || por_cpu[c].trocar) continue;
        int ap = prioridade(a, agora);
        if (pior < 0 || ap > pior_pr) { pior = (int)c; pior_pr = ap; }
    }
    if (pior >= 0 && pior_pr >= pp + MARGEM_PREEMPCAO)
        planificador_pedir_troca((uint32_t)pior);
}

/* ------------------------------------------------------------ el tic */

static void decair_recente(void)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado == P_LIVRE) continue;
        /* tiques_cpu lo suma el tic de cada CPU sin el cerrojo: aquí solo se
         * lee; "recente" lo escribe solo esta función. */
        uint32_t t = p->tiques_cpu;
        p->recente = (p->recente + (t - p->tiques_vistos)) / 2;
        p->tiques_vistos = t;
    }
}

/* equilibrio: pasa un proceso listo de la CPU más cargada a la menos
 * cargada si se llevan dos o más. Solo procesos de usuario que no corren. */
static void equilibrar(uint32_t agora)
{
    uint32_t ativas = cpus_ativas();
    for (uint32_t c = 0; c < CPU_MAX; c++) {
        if (!(ativas & (1u << c))) continue;
        uint32_t o = cpu_dados(c)->tiques_ociosos;
        por_cpu[c].ociosos_delta = o - por_cpu[c].ociosos_ant;
        por_cpu[c].ociosos_ant = o;
    }
    if (!smp_arrancadas()) return;
    uint32_t carga[CPU_MAX];
    calcular_carga(carga, ativas);
    int max = -1;
    for (uint32_t c = 0; c < CPU_MAX; c++)
        if ((ativas & (1u << c)) && (max < 0 || carga[c] > carga[max])) max = (int)c;
    if (max < 0 || carga[max] < 2) return;
    /* La víctima: la más fría de la cola de "max" que pueda irse a la menos
     * cargada de las CPU que su máscara permite. */
    processo_t *vitima = NULL;
    int destino = -1;
    uint32_t m_espera = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado != P_PRONTO || p->na_cpu || !p->usuario || p->casa != (uint32_t)max) continue;
        int d = menos_carregada(p, carga, ativas);
        if (d < 0 || d == max || carga[d] + 2 > carga[max]) continue;
        uint32_t e = agora - p->saiu_em;
        if (!vitima || e > m_espera) { vitima = p; destino = d; m_espera = e; }
    }
    if (!vitima) return;
    vitima->casa = (uint32_t)destino;
    if (ociosa((uint32_t)destino)) avisar_cpu((uint32_t)destino);
}

void planificador_tique0(uint32_t agora)
{
    static uint32_t prox_recente, prox_equilibrio;
    if ((int32_t)(agora - prox_recente) >= 0) {
        prox_recente = agora + PERIODO_RECENTE;
        decair_recente();
    }
    if (ativo == PLANIFICADOR_EQUILIBRIO && (int32_t)(agora - prox_equilibrio) >= 0) {
        prox_equilibrio = agora + PERIODO_EQUILIBRIO;
        equilibrar(agora);
    }
}

/* ------------------------------------------------------------ la llamada */

/* Aplica la máscara a una entrada: si corre donde ya no puede, que su CPU
 * planifique ya; si su cola es de una CPU prohibida, se queda sin casa. */
static void aplicar_mascara(processo_t *t, uint32_t m)
{
    t->mascara_cpu = m;
    if (t->casa != CASA_NENHUMA && !planificador_pode_cpu(t, t->casa)) t->casa = CASA_NENHUMA;
    if (t == processo_atual()) {
        if (!planificador_pode_cpu(t, cpu_atual())) planificador_pedir_troca(cpu_atual());
    } else if (t->na_cpu && !planificador_pode_cpu(t, t->cpu)) {
        planificador_pedir_troca(t->cpu);
    }
}

static processo_t *alvo(uint32_t pid)
{
    processo_t *p = pid == 0 ? processo_atual() : processo_por_pid((int)pid);
    if (!p || p->estado == P_ZUMBI || p->estado == P_LIVRE) return NULL;
    return p;
}

static int32_t copiar_nome(uint32_t n, uint32_t buf, uint32_t tam)
{
    const char *nome = planificador_nome(n);
    if (!nome) return ERRO_INVALIDO;
    if (!buf) return 0;
    if (tam == 0 || !usuario_ok((void *)buf, tam, true)) return ERRO_ENDERECO;
    txt_copia_n((char *)buf, nome, tam);
    return 0;
}

int32_t sis_planificador(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t e)
{
    (void)e;
    switch (op) {
    case PLAN_CONSULTAR: {
        int32_t r = copiar_nome(ativo, a, b);
        return r < 0 ? r : (int32_t)ativo;
    }
    case PLAN_ESCOLHER: {
        uint32_t antes = planificador_trocar_para(a);
        return antes ? (int32_t)antes : ERRO_INVALIDO;
    }
    case PLAN_NOME:
        return copiar_nome(a, b, c);
    case PLAN_AFINIDADE: {
        processo_t *p = alvo(a);
        if (!p) return ERRO_SEM_PROCESSO;
        processo_t *l = processo_lider(p);
        int32_t antes = (int32_t)l->mascara_cpu;
        if (b == 0xFFFFFFFFu) return antes;   /* solo consultar */
        if (!l->usuario) return ERRO_ACESSO;  /* las tareas del kernel, en la CPU 0 */
        uint32_t m = b & ((1u << CPU_MAX) - 1);
        if (b && !(m & cpus_ativas())) return ERRO_INVALIDO;   /* ninguna CPU que trabaje */
        /* Todo el proceso: el líder y sus hilos (D167). */
        for (int i = 1; i < MAX_PROCESSOS; i++) {
            processo_t *t = processo_indice(i);
            if (t->estado != P_LIVRE && t->estado != P_ZUMBI && (t == l || t->lider == l)) aplicar_mascara(t, m);
        }
        return antes;
    }
    case PLAN_PRIORIDADE: {
        processo_t *p = alvo(a);
        if (!p) return ERRO_SEM_PROCESSO;
        processo_t *l = processo_lider(p);
        int n = (int32_t)b;
        if (n < -20 || n > 19) return ERRO_INVALIDO;
        for (int i = 1; i < MAX_PROCESSOS; i++) {
            processo_t *t = processo_indice(i);
            if (t->estado != P_LIVRE && (t == l || t->lider == l)) t->nice = n;
        }
        return 0;
    }
    case PLAN_AUDIO: {
        /* D436: solo el hilo que llama; uno por proceso (al encenderlo en uno, se apaga en los demás). */
        processo_t *eu = processo_atual();
        int32_t antes = eu->prio_audio ? 1 : 0;
        if (a == 0xFFFFFFFFu) return antes;
        if (a > 1) return ERRO_INVALIDO;
        if (!eu->usuario) return ERRO_ACESSO;
        if (a) {
            processo_t *l = processo_lider(eu);
            for (int i = 1; i < MAX_PROCESSOS; i++) {
                processo_t *t = processo_indice(i);
                if (t != eu && t->estado != P_LIVRE && (t == l || t->lider == l)) t->prio_audio = false;
            }
        }
        eu->prio_audio = a != 0;
        if (a && !antes) klog("[PLANIFICADOR] hilo %d (%s): alta prioridad de audio\n", eu->pid, eu->nome);
        return antes;
    }
    case PLAN_INFO: {
        processo_t *p = alvo(a);
        if (!p) return ERRO_SEM_PROCESSO;
        info_planificacion_t *v = (info_planificacion_t *)b;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        memset(v, 0, sizeof *v);
        v->pid = p->pid;
        v->cpu = p->cpu;
        v->mascara = p->mascara_cpu;
        v->nice = p->nice;
        v->migracoes = p->migracoes;
        v->execucoes = p->execucoes;
        v->recente = p->recente + (p->tiques_cpu - p->tiques_vistos);
        v->prioridade = prioridade(p, relogio_tiques());
        uint32_t mhz = hw_tsc_khz() / 1000;
        if (mhz && p->lat_n) {
            v->lat_media_us = (p->lat_soma_kc / p->lat_n) * 1024u / mhz;
            v->lat_max_us = p->lat_max_kc / mhz * 1024u + (p->lat_max_kc % mhz) * 1024u / mhz;
        }
        v->despertares = p->lat_n;
        if (mhz) v->lat_total_us = p->lat_soma_kc / mhz * 1024u + (p->lat_soma_kc % mhz) * 1024u / mhz;
        return 0;
    }
    default:
        return ERRO_INVALIDO;
    }
}
