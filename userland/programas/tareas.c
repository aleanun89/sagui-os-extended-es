// SPDX-License-Identifier: GPL-2.0-or-later
/* tareas - el Administrador de tareas (D043, D079, D216).
 *
 * Seis pestañas (D368, modernizado):
 *   Procesos     tabla con PID, nombre, padre, estado, % de CPU, tiempo de CPU,
 *                memoria, ventanas y núcleo (más hilos y prioridad, a elección);
 *                ordena por la columna pulsada en la cabecera; escribir busca;
 *                botón derecho en la cabecera: columnas y árbol; en una fila:
 *                finalizar, prioridad, detalles; colores de «calor» por consumo;
 *                _("Finalizar proceso") (con confirmación) usa SIS_MATAR.
 *   Rendimiento  barra lateral con una miniatura por recurso (CPU, Memoria, Swap,
 *                Disco, Red, GPU, Sensores) y el panel grande del elegido, con
 *                gráficos de los últimos 60 s (monitor de hardware, D092: sólo
 *                consulta los sensores con la pestaña visible, una vez por segundo).
 *   Detalles     el proceso elegido: planificador, pila, hilos (INFO_HILOS).
 *   Ventanas     las ventanas abiertas y _("Cerrar ventana") (SIS_PEDIR_FECHAR,
 *                lo mismo que pulsar la X).
 *   Red          cada interfaz (eth0, lo): estado, driver, MAC, direcciones,
 *                contadores y velocidad; el gráfico del último minuto; las
 *                conexiones con el proceso dueño; Renovar DHCP, desactivar la
 *                interfaz y cortar una conexión (D079). Sólo consulta el kernel
 *                con la pestaña visible, una vez por segundo.
 *   Servicios    los servicios del supervisor (D216): estado, PID, memoria,
 *                tiempo en marcha, reinicios y arranque automático; Iniciar o
 *                Parar, Reiniciar y Activar/Desactivar (las mismas órdenes que
 *                "servicio"). Solo lee el estado con la pestaña visible.
 *
 * La ventana tiene tamaño variable (D044): las tablas y la pestaña Red usan el
 * espacio que haya.
 *
 * El % de CPU de un proceso es la diferencia de su cpu_ms entre dos
 * muestras dividida por el tiempo transcurrido. El uso total es todo lo que no es
 * del proceso 0, el ocioso (sólo se ejecuta cuando nadie más está listo).
 *
 * "tareas -t" muestra una muestra de 1 s en el terminal, como un "top" de una
 * sola pasada (útil en las pruebas). */
#include "sagui.h"
#include "gui.h"
#include "contas.h"
#include "sensores.h"
#include "servicos.h"

#define LARG_INI 664
#define ALT_INI  470
#define LARG_MIN 600
#define ALT_MIN  400
#define MAXP 64
#define MAXJ 32
#define HIST 60          /* segundos en los gráficos */

/* ------------------------------------------------------------ amostras */

typedef struct {
    info_processo_t p;
    unsigned cpu10;      /* % de CPU x 10 na última amostra */
    int      nice;       /* D368: solo si querer_nice */
    bool     tem_nice;
} proc_t;

static proc_t procs[MAXP];
static int qtd_procs;
static info_processo_t ant[MAXP];
static int qtd_ant;
static unsigned t_ant;
static bool tem_ant;
static unsigned cpu_total10;          /* % x 10 */
static info_memoria_t mem;
static info_versao_t versao;
static unsigned hist_cpu[HIST], hist_mem[HIST];   /* % x 10 */
static int qtd_hist;
static info_janela_t jans[MAXJ];
static int qtd_jans;

/* Núcleos (D157): una entrada por CPU; el uso sale de la diferencia de tiques
 * y de tiques ociosos entre dos muestras. */
#define MAXN 8
static info_nucleo_t nuc[MAXN], nuc_ant[MAXN];
static int qtd_nuc, qtd_nuc_ant;
static unsigned nuc_uso10[MAXN];      /* % x 10 de cada CPU */

/* D368: historias y tasas de lo que enseña la barra lateral de Rendimiento. */
static unsigned hist_nuc[MAXN][HIST];                    /* % x 10 de cada CPU */
static unsigned hist_swap[HIST];                         /* % x 10 del tope de la swap */
static unsigned hist_swap_e[HIST], hist_swap_s[HIST];    /* páginas por segundo traídas y sacadas */
static unsigned swap_e_s, swap_s_s, swap_e_ant, swap_s_ant;
static info_disco_cache_t disco_i, disco_ant;
static bool disco_ok;
static unsigned disco_rd_s, disco_wr_s;                  /* KB/s */
static unsigned hist_disco_rd[HIST], hist_disco_wr[HIST];
static unsigned por_segundo(unsigned agora, unsigned antes, unsigned dt);
static bool querer_nice;                                 /* la columna Prior. o Detalles necesitan el nice */

static info_frequencia_t freq;                       /* driver de frecuencia (D106) */
static bool freq_ok;
static info_freq_nucleo_t freq_nuc[MAXN];             /* el estado de cada CPU (D175) */
static int qtd_freq_nuc;

static void ler_freq_nucleos(void)
{
    qtd_freq_nuc = freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL ? frequencia_nucleos(freq_nuc, MAXN) : 0;
}

/* «P0 2100 MHz» de la CPU, o "" si el driver no la conoce. */
static const char *freq_da_cpu(unsigned cpu, char *b, int max)
{
    b[0] = 0;
    for (int k = 0; k < qtd_freq_nuc; k++) {
        const info_freq_nucleo_t *v = &freq_nuc[k];
        if (v->cpu != cpu || !v->ligada || v->atual < 0 || v->atual >= (int)freq.qtd_estados) continue;
        char m[24];
        formatar_texto(b, max, "%s %s", freq.estado_nome[v->atual], v->khz ? sensores_mhz(m, sizeof m, v->khz) : "");
    }
    return b;
}


static void amostrar_nucleos(void)
{
    static info_nucleo_t v[MAXN];
    int n = info_sistema(INFO_NUCLEOS, v, MAXN);
    if (n < 1) {   /* el núcleo no informa: una sola CPU, "CPU 0", con el uso total */
        memset(v, 0, sizeof v);
        v[0].estado = 1;
        n = 1;
    }
    for (int i = 0; i < n; i++) {
        unsigned u = 0;
        if (i < qtd_nuc_ant && v[i].tiques > nuc_ant[i].tiques) {
            unsigned dt = v[i].tiques - nuc_ant[i].tiques;
            unsigned di = v[i].tiques_ociosos - nuc_ant[i].tiques_ociosos;
            if (v[i].tiques_ociosos < nuc_ant[i].tiques_ociosos) di = 0;
            if (di > dt) di = dt;
            u = (dt - di) * 1000 / dt;
        } else if (qtd_nuc_ant == 0 && n == 1) {
            u = cpu_total10;   /* sin diferencia todavía: el uso total del sistema */
        }
        nuc_uso10[i] = u;
        nuc_ant[i] = v[i];
        nuc[i] = v[i];
    }
    qtd_nuc = qtd_nuc_ant = n;
}

static const char *nome_estado(unsigned e)
{
    static const char *n[] = { "libre", "listo", "ejecutando", "bloqueado", "zombi" };
    return e <= 4 ? n[e] : "?";
}

static const char *nome_estado_nucleo(unsigned e)
{
    return e == 1 ? "activa" : e == 2 ? "ociosa" : "libre";
}

static const info_processo_t *achar_ant(int pid)
{
    for (int i = 0; i < qtd_ant; i++)
        if (ant[i].pid == pid) return &ant[i];
    return NULL;
}

static void deslocar_hist(void)
{
    unsigned *const h[] = { hist_cpu, hist_mem, hist_swap, hist_swap_e, hist_swap_s, hist_disco_rd, hist_disco_wr };
    for (unsigned i = 0; i < sizeof h / sizeof h[0]; i++) memmove(h[i], h[i] + 1, (HIST - 1) * sizeof h[i][0]);
    for (int i = 0; i < MAXN; i++) memmove(hist_nuc[i], hist_nuc[i] + 1, (HIST - 1) * sizeof hist_nuc[i][0]);
}

/* Swap comprimida (D352) y actividad de la caché de disco (D368): tasas por segundo. */
static void amostrar_swap_disco(unsigned dt, bool medir)
{
    swap_s_s = swap_e_s = disco_rd_s = disco_wr_s = 0;
    if (medir && dt) {
        swap_s_s = por_segundo(mem.troca_saidas, swap_s_ant, dt);
        swap_e_s = por_segundo(mem.troca_entradas, swap_e_ant, dt);
    }
    swap_s_ant = mem.troca_saidas;
    swap_e_ant = mem.troca_entradas;
    info_disco_cache_t d;
    memset(&d, 0, sizeof d);
    disco_ok = info_sistema(INFO_DISCO_CACHE, &d, sizeof d) == 0 && d.versao == 1;
    if (!disco_ok) memset(&d, 0, sizeof d);
    if (medir && dt && disco_ok) {   /* un bloque son 4 KB */
        disco_rd_s = (por_segundo(d.falhas, disco_ant.falhas, dt) + por_segundo(d.anticipados, disco_ant.anticipados, dt)) * 4;
        disco_wr_s = por_segundo(d.gravados, disco_ant.gravados, dt) * 4;
    }
    disco_ant = d;
    disco_i = d;
}

static void amostrar(void)
{
    static info_processo_t v[MAXP];
    unsigned agora = tempo_ms();
    int n = info_sistema(INFO_PROCESSOS, v, MAXP);
    if (n < 0) n = 0;
    unsigned dt = agora - t_ant;
    unsigned ocioso10 = 0;
    for (int i = 0; i < n; i++) {
        procs[i].p = v[i];
        unsigned c = 0;
        if (tem_ant && dt > 0) {
            const info_processo_t *a = achar_ant(v[i].pid);
            unsigned antes = a ? a->cpu_ms : 0;   /* proceso nuevo: todo ocurrió en este intervalo */
            unsigned delta = v[i].cpu_ms >= antes ? v[i].cpu_ms - antes : 0;
            c = delta * 1000 / dt;
            if (c > 1000) c = 1000;
        }
        procs[i].cpu10 = c;
        procs[i].tem_nice = false;
        if (querer_nice) {
            info_planificacion_t pi;
            if (planificacion_info(v[i].pid, &pi) == 0) { procs[i].nice = pi.nice; procs[i].tem_nice = true; }
        }
        if (v[i].pid == 0) ocioso10 = c;
    }
    qtd_procs = n;
    if (tem_ant) {
        cpu_total10 = ocioso10 >= 1000 ? 0 : 1000 - ocioso10;
    }
    for (int i = 0; i < n; i++) ant[i] = v[i];
    qtd_ant = n;
    t_ant = agora;
    amostrar_nucleos();
    if (qtd_nuc == 1 && tem_ant) nuc_uso10[0] = cpu_total10;   /* con una CPU, coincide con el total */
    info_sistema(INFO_MEMORIA, &mem, sizeof mem);
    amostrar_swap_disco(dt, tem_ant);
    info_sistema(INFO_VERSAO, &versao, sizeof versao);
    int j = info_sistema(INFO_JANELAS, jans, MAXJ);
    qtd_jans = j < 0 ? 0 : j;
    if (tem_ant) {
        if (qtd_hist == HIST) { deslocar_hist(); qtd_hist--; }
        hist_cpu[qtd_hist] = cpu_total10;
        for (int i = 0; i < qtd_nuc && i < MAXN; i++) hist_nuc[i][qtd_hist] = nuc_uso10[i];
        hist_swap[qtd_hist] = mem.troca_teto_kb ? (unsigned)((unsigned long long)mem.troca_armazem_kb * 1000 / mem.troca_teto_kb) : 0;
        if (hist_swap[qtd_hist] > 1000) hist_swap[qtd_hist] = 1000;
        hist_swap_e[qtd_hist] = swap_e_s;
        hist_swap_s[qtd_hist] = swap_s_s;
        hist_disco_rd[qtd_hist] = disco_rd_s;
        hist_disco_wr[qtd_hist] = disco_wr_s;
        unsigned usada = mem.usada_kb, gerida = mem.gerida_kb;
        while (gerida > 4000000u) { usada >>= 4; gerida >>= 4; }   /* sin cuenta de 64 bits */
        hist_mem[qtd_hist] = gerida ? usada * 1000 / gerida : 0;
        qtd_hist++;
    }
    tem_ant = true;
}

static const proc_t *proc_por_pid(int pid)
{
    for (int i = 0; i < qtd_procs; i++)
        if (procs[i].p.pid == pid) return &procs[i];
    return NULL;
}

static void fmt_tempo(char *t, size_t max, unsigned ms)
{
    unsigned s = ms / 1000;
    formatar_texto(t, max, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
}

/* ------------------------------------------------------------ ordenación */

enum { C_PID, C_NOME, C_PAI, C_ESTADO, C_CPU, C_TEMPO, C_MEM, C_JAN, C_NUCLEO, C_HILOS, C_PRIO, QTD_COLUNAS };

typedef struct { const char *titulo; int l; bool direita; } coluna_t;
static const coluna_t colunas[QTD_COLUNAS] = {
    { N_("PID"), 48, true }, { N_("Nombre"), 100, false }, { N_("Padre"), 56, true }, { N_("Estado"), 96, false },
    { N_("CPU %"), 64, true }, { N_("T. CPU"), 72, true }, { N_("Memoria"), 80, true }, { N_("Ventana"), 64, false },
    { N_("Núcleo"), 56, true }, { N_("Hilos"), 52, true }, { N_("Prior."), 56, true },
};
/* Columnas visibles (D368): por defecto las de siempre; Hilos y Prior. se piden con el botón derecho en la cabecera. */
static unsigned col_vis = (1u << (C_NUCLEO + 1)) - 1;
static bool arbol;                      /* procesos como árbol (padre e hijos) */
static char filtro[32];                 /* búsqueda: texto dentro del nombre o del PID */

static int ordem_col = C_PID;
static bool ordem_desc;
static int ordem[MAXP];   /* índices en procs[], en el orden mostrado */

static int comparar(const proc_t *a, const proc_t *b)
{
    long long d = 0;
    switch (ordem_col) {
    case C_PID:    d = a->p.pid - b->p.pid; break;
    case C_NOME:   d = compara_texto(a->p.nome, b->p.nome); break;
    case C_PAI:    d = a->p.pai - b->p.pai; break;
    case C_ESTADO: d = (long long)a->p.estado - b->p.estado; break;
    case C_CPU:    d = (long long)a->cpu10 - b->cpu10; break;
    case C_TEMPO:  d = (long long)a->p.cpu_ms - b->p.cpu_ms; break;
    case C_MEM:    d = (long long)a->p.memoria_kb - b->p.memoria_kb; break;
    case C_JAN:    d = (long long)a->p.janelas - b->p.janelas; break;
    case C_NUCLEO: d = (long long)a->p.cpu - b->p.cpu; break;
    case C_HILOS:  d = (long long)a->p.n_hilos - b->p.n_hilos; break;
    case C_PRIO:   d = (long long)a->nice - b->nice; break;
    }
    if (d == 0) d = a->p.pid - b->p.pid;   /* empate: por PID, siempre creciente */
    else if (ordem_desc) d = -d;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

static int nvis;          /* filas mostradas: ordem[0..nvis) */
static int prof[MAXP];   /* profundidad en el árbol de cada fila mostrada */

static char baixa(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool contem_ci(const char *h, const char *a)
{
    for (; *h; h++) {
        int k = 0;
        while (a[k] && h[k] && baixa(h[k]) == baixa(a[k])) k++;
        if (!a[k]) return true;
    }
    return false;
}

static bool casa_filtro(const proc_t *p)
{
    if (!filtro[0]) return true;
    char t[16];
    formatar_texto(t, sizeof t, "%d", p->p.pid);
    return contem_ci(p->p.nome, filtro) || contem_ci(t, filtro);
}

static void emitir_arvore(const int *todos, bool *feito, int idx, int nivel)
{
    if (feito[idx] || nivel > 12) return;
    feito[idx] = true;
    if (casa_filtro(&procs[idx])) { ordem[nvis] = idx; prof[nvis++] = nivel; }
    for (int k = 0; k < qtd_procs; k++) {
        int c = todos[k];
        if (!feito[c] && procs[c].p.pai == procs[idx].p.pid && procs[c].p.pid != procs[idx].p.pid)
            emitir_arvore(todos, feito, c, nivel + 1);
    }
}

static void ordenar(void)
{
    static int todos[MAXP];
    static bool feito[MAXP];
    for (int i = 0; i < qtd_procs; i++) todos[i] = i;
    for (int i = 1; i < qtd_procs; i++) {   /* inserción: como máximo 64 */
        int x = todos[i], k = i - 1;
        while (k >= 0 && comparar(&procs[todos[k]], &procs[x]) > 0) { todos[k + 1] = todos[k]; k--; }
        todos[k + 1] = x;
    }
    nvis = 0;
    if (!arbol) {
        for (int i = 0; i < qtd_procs; i++)
            if (casa_filtro(&procs[todos[i]])) { ordem[nvis] = todos[i]; prof[nvis++] = 0; }
        return;
    }
    memset(feito, 0, sizeof feito);
    for (int i = 0; i < qtd_procs; i++) {   /* raíces: sin padre en la lista */
        const proc_t *p = &procs[todos[i]];
        if (p->p.pai == p->p.pid || !proc_por_pid(p->p.pai)) emitir_arvore(todos, feito, todos[i], 0);
    }
    for (int i = 0; i < qtd_procs; i++) emitir_arvore(todos, feito, todos[i], 0);   /* lo que quede (ciclos) */
}

/* ------------------------------------------------------------ modo texto (-t) */

/* D186: la pila que ocupa cada proceso (la del principal más la de sus hilos). */
static info_pilha_t pilhas[MAXP];
static int qtd_pilhas;

static unsigned pilha_de(int pid)
{
    for (int i = 0; i < qtd_pilhas; i++)
        if (pilhas[i].pid == pid) return pilhas[i].pilha_kb + pilhas[i].hilos_kb;
    return 0;
}

static int modo_texto(void)
{
    amostrar();
    dormir(1000);
    amostrar();
    ordem_col = C_CPU;
    ordem_desc = true;
    ordenar();
    unsigned s = versao.segundos_ligado;
    imprimir("CPU: %u,%u %%   Memoria: %u KB de %u KB   Procesos: %d   Encendido: %u:%02u:%02u\n",
             cpu_total10 / 10, cpu_total10 % 10, mem.usada_kb, mem.gerida_kb, qtd_procs,
             s / 3600, s / 60 % 60, s % 60);
    freq_ok = frequencia_ler(&freq);
    ler_freq_nucleos();
    char plan[24];
    if (planificador_consultar(plan, sizeof plan) > 0)
        imprimir("Planificador: %s\n", plan);   /* D174 */
    for (int i = 0; i < qtd_nuc; i++) {   /* D174: núcleo físico e hilo (SMT); D175: su frecuencia */
        char fq[32];
        freq_da_cpu(nuc[i].indice, fq, sizeof fq);
        imprimir("CPU %u: %u,%u %%  %s  (núcleo %u, hilo %u)%s%s\n", nuc[i].indice, nuc_uso10[i] / 10, nuc_uso10[i] % 10,
                 nome_estado_nucleo(nuc[i].estado), nuc[i].nucleo, nuc[i].hilo, fq[0] ? "  " : "", fq);
    }
    qtd_pilhas = info_sistema(INFO_PILHAS, pilhas, MAXP);
    if (qtd_pilhas < 0) qtd_pilhas = 0;
    imprimir("  PID PADRE ESTADO      CPU %%    T. CPU  MEMORIA    PILA VENT. NUCL. NOMBRE\n");
    for (int i = 0; i < nvis; i++) {
        const proc_t *p = &procs[ordem[i]];
        char t[16];
        fmt_tempo(t, sizeof t, p->p.cpu_ms);
        char h[16] = "";
        if (p->p.n_hilos > 1) formatar_texto(h, sizeof h, _(" (%u hilos)"), p->p.n_hilos);   /* D167 */
        imprimir("%5d %5d %-10s %3u,%u %9s %5u KB %4u KB %5u %5u %s%s%s\n", p->p.pid, p->p.pai, nome_estado(p->p.estado),
                 p->cpu10 / 10, p->cpu10 % 10, t, p->p.memoria_kb, pilha_de(p->p.pid), p->p.janelas, p->p.cpu, p->p.nome, h,
                 p->p.usuario ? "" : " (kernel)");
    }
    return 0;
}

/* ------------------------------------------------------------ ventana */

static jan_t *j;
static int larg = LARG_INI, alt = ALT_INI;   /* tamaño actual (D044) */
#define LARG larg
#define ALT  alt
static int aba;                     /* 0 Procesos, 1 Rendimiento, 2 Ventanas, 3 Red, 4 Servicios, 5 Detalles */
static bool pausado;
static int sel_pid = -1, topo;
static int sel_jan = -1;            /* id de la ventana seleccionada */

#define ABA_Y   8
#define ABA_A   24
#define PAINEL_Y (ABA_Y + ABA_A)
#define PAINEL_A (ALT - PAINEL_Y - 52)
#define TAB_X   12
#define TAB_Y   (PAINEL_Y + 10)
#define TAB_L   (LARG - 24)
#define TAB_A   (PAINEL_A - 20)
#define CAB_A   20
#define LIN_A   18
#define BARRA_L 12
#define BOT_Y   (ALT - 40)

#define QTD_ABAS 6
#define ABA_SERV 4
#define ABA_DET  5
static const char *nomes_abas[QTD_ABAS] = { N_("Procesos"), N_("Rendimiento"), N_("Ventanas"), N_("Red"), N_("Servicios"), N_("Detalles") };
static const char *nomes_log[QTD_ABAS] = { "procesos", "rendimiento", "ventanas", "red", "servicios", "detalles" };
static const int abas_x[QTD_ABAS] = { 10, 112, 230, 328, 394, 488 }, abas_l[QTD_ABAS] = { 100, 116, 96, 64, 92, 84 };

static botao_t b_pausa  = { 12, 0, 110, 28, N_("Pausar"), false };
static botao_t b_acao   = { 0, 0, 170, 28, N_("Finalizar proceso"), false };
static botao_t b_serv_rein = { 0, 0, 96, 28, N_("Reiniciar"), false };    /* D216: pestaña Servicios */
static botao_t b_serv_auto = { 0, 0, 110, 28, N_("Desactivar"), false };

static int linhas_visiveis(void) { return (TAB_A - CAB_A - 4) / LIN_A; }

static void desenhar_abas(void)
{
    g_retangulo(j, 0, 0, LARG, PAINEL_Y + 1, COR_FUNDO);
    g_retangulo(j, 0, PAINEL_Y, LARG, 1, COR_BORDA);
    for (int i = 0; i < QTD_ABAS; i++) {
        bool s = i == aba;
        int x = abas_x[i], y = s ? ABA_Y - 3 : ABA_Y, a = s ? ABA_A + 4 : ABA_A;
        g_retangulo(j, x, y, abas_l[i], a, s ? COR_BRANCO : COR_BOTAO);
        g_retangulo(j, x, y, abas_l[i], 1, COR_BORDA);
        g_retangulo(j, x, y, 1, a - (s ? 3 : 0), COR_BORDA);
        g_retangulo(j, x + abas_l[i] - 1, y, 1, a - (s ? 3 : 0), COR_BORDA);
        if (s) g_retangulo(j, x, y, abas_l[i], 3, COR_DESTAQUE);
        g_texto(j, x + (abas_l[i] - g_largura_texto(_(nomes_abas[i]))) / 2, y + (a - LETRA_A) / 2 + 1,
                _(nomes_abas[i]), s ? COR_DESTAQUE : COR_TEXTO);
    }
    jan_mostrar_area(j, 0, 0, LARG, PAINEL_Y + 1);
}

static void texto_coluna(int x, int y, int l, const char *t, bool direita, uint32_t cor)
{
    int cabem = (l - 8) / LETRA_L, n = (int)tam_texto(t);
    if (n > cabem) n = cabem;
    int tx = direita ? x + l - 6 - n * LETRA_L : x + 4;
    g_texto_n(j, tx, y, t, n, cor);
}

/* Color de «calor» de una celda: de transparente (0) a naranja intenso (1000), para ver de un vistazo
 * quién consume. Devuelve 0 por debajo del 5 %. */
static uint32_t calor(unsigned v1000)
{
    if (v1000 < 50) return 0;
    if (v1000 > 1000) v1000 = 1000;
    unsigned r = 0xFF, g = 0xF4 - (0xF4 - 0x9A) * v1000 / 1000, b = 0xD8 - (0xD8 - 0x5A) * v1000 / 1000;
    return r << 16 | g << 8 | b;
}

static void desenhar_tabela(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BRANCO);
    g_moldura(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BORDA);
    /* Cabecera: pulsar ordena; la columna actual muestra ^ (creciente) o v. */
    int x = TAB_X + 1;
    for (int c = 0; c < QTD_COLUNAS; c++) {
        if (!(col_vis >> c & 1)) continue;
        bool atual = c == ordem_col;
        g_retangulo(j, x, TAB_Y + 1, colunas[c].l, CAB_A, atual ? 0xD6E4D9 : COR_BOTAO);
        g_relevo(j, x, TAB_Y + 1, colunas[c].l, CAB_A, false);
        char t[24];
        formatar_texto(t, sizeof t, atual ? "%s %s" : "%s", _(colunas[c].titulo), ordem_desc ? "v" : "^");
        texto_coluna(x, TAB_Y + 3, colunas[c].l, t, colunas[c].direita, COR_TEXTO);
        x += colunas[c].l;
    }
    ordenar();
    int vis = linhas_visiveis();
    if (topo > nvis - vis) topo = nvis - vis;
    if (topo < 0) topo = 0;
    if (!nvis) g_texto(j, TAB_X + 8, TAB_Y + CAB_A + 8, filtro[0] ? _("Ningún proceso coincide con la búsqueda.") : "", COR_TEXTO_FRACO);
    unsigned gerida = mem.gerida_kb ? mem.gerida_kb : 1;
    for (int k = 0; k < vis && topo + k < nvis; k++) {
        const proc_t *p = &procs[ordem[topo + k]];
        int y = TAB_Y + 1 + CAB_A + 2 + k * LIN_A;
        bool s = p->p.pid == sel_pid;
        uint32_t cor = s ? COR_BRANCO : p->p.usuario ? COR_TEXTO : COR_TEXTO_FRACO;
        if (s) g_retangulo(j, TAB_X + 2, y, TAB_L - 4 - BARRA_L, LIN_A, COR_SELECAO);
        else if (k & 1) g_retangulo(j, TAB_X + 2, y, TAB_L - 4 - BARRA_L, LIN_A, 0xF8F9FA);   /* cebra */
        char t[64];
        x = TAB_X + 1;
        for (int c = 0; c < QTD_COLUNAS; c++) {
            if (!(col_vis >> c & 1)) continue;
            uint32_t calc = 0;
            switch (c) {
            case C_PID:    formatar_texto(t, sizeof t, "%d", p->p.pid); break;
            case C_NOME: {  /* D167: con varios hilos, cuántos; en árbol, sangría */
                char sang[32];
                int np = prof[topo + k] * 2;
                if (np > 24) np = 24;
                for (int q = 0; q < np; q++) sang[q] = ' ';
                sang[np] = 0;
                if (p->p.n_hilos > 1) formatar_texto(t, sizeof t, "%s%s (%u)", sang, p->p.nome, p->p.n_hilos);
                else formatar_texto(t, sizeof t, "%s%s", sang, p->p.nome);
                break;
            }
            case C_PAI:    formatar_texto(t, sizeof t, p->p.pid ? "%d" : "-", p->p.pai); break;
            case C_ESTADO: copia_texto(t, nome_estado(p->p.estado), sizeof t); break;
            case C_CPU:    formatar_texto(t, sizeof t, "%u,%u", p->cpu10 / 10, p->cpu10 % 10);
                           if (p->p.pid) { calc = calor(p->cpu10 * 2); }   /* el 50 % ya es máximo */
                           break;
            case C_TEMPO:  fmt_tempo(t, sizeof t, p->p.cpu_ms); break;
            case C_MEM:    formatar_texto(t, sizeof t, "%u KB", p->p.memoria_kb);
                           calc = calor((unsigned)((unsigned long long)p->p.memoria_kb * 4000 / gerida)); break;   /* 25 % = máximo */
            case C_JAN:    if (!p->p.janelas) t[0] = 0;
                           else formatar_texto(t, sizeof t, p->p.janelas > 1 ? _("sí (%u)") : _("sí"), p->p.janelas);
                           break;
            case C_NUCLEO: formatar_texto(t, sizeof t, "%u", p->p.cpu); break;
            case C_HILOS:  formatar_texto(t, sizeof t, "%u", p->p.n_hilos); break;
            case C_PRIO:   if (p->tem_nice) formatar_texto(t, sizeof t, "%d", p->nice); else copia_texto(t, "-", sizeof t); break;
            }
            if (calc && !s) g_retangulo(j, x, y, colunas[c].l, LIN_A, calc);
            texto_coluna(x, y + 1, colunas[c].l, t, colunas[c].direita, cor);
            x += colunas[c].l;
        }
    }
    /* Barra de desplazamiento (pulsar encima o debajo del pulgar desplaza una página). */
    int bx = TAB_X + TAB_L - BARRA_L - 1, by = TAB_Y + 1 + CAB_A, ba = TAB_A - CAB_A - 2;
    g_retangulo(j, bx, by, BARRA_L, ba, 0xE4E7EA);
    int pol_a = nvis <= vis ? ba : ba * vis / nvis;
    if (pol_a < 12) pol_a = 12;
    int pol_y = nvis <= vis ? 0 : (ba - pol_a) * topo / (nvis - vis);
    g_retangulo(j, bx + 2, by + pol_y, BARRA_L - 4, pol_a, 0x9AA3AC);
}

static void desenhar_grafico(int x, int y, int l, int a, const unsigned *v, int n, uint32_t linha, uint32_t preench)
{
    g_retangulo(j, x, y, l, a, 0x10181E);
    for (int k = 1; k < 4; k++) g_retangulo(j, x, y + a * k / 4, l, 1, 0x26343C);   /* 25 %, 50 %, 75 % */
    for (int k = 1; k < 6; k++) g_retangulo(j, x + l * k / 6, y, 1, a, 0x26343C);   /* a cada 10 s */
    /* La muestra más nueva queda a la derecha; cada segundo ocupa l / HIST píxeles. */
    int passo = l / HIST, px_ant = -1, py_ant = 0;
    for (int i = 0; i < n; i++) {
        int px = x + l - (n - i) * passo;
        int py = y + a - 1 - (int)((a - 2) * v[i] / 1000);
        g_retangulo(j, px, py, passo, y + a - py, preench);
        if (px_ant >= 0) g_linha(j, px_ant, py_ant, px, py, linha);
        g_linha(j, px, py, px + passo - 1, py, linha);
        px_ant = px + passo - 1; py_ant = py;
    }
    g_moldura(j, x, y, l, a, COR_BORDA);
}

/* Monitor de hardware (D092): muestras sólo con la pestaña Rendimiento visible. */
static info_sensores_t sens;
static bool sens_ok;
static unsigned hist_temp[HIST], hist_freq[HIST];   /* x 10 da escala do gráfico */
static int qtd_hist_sens;
static unsigned freq_escala;                         /* kHz do topo do gráfico */
/* La frecuencia del gráfico: la del monitor o, sin ella (sin APERF/MPERF ni
 * P-state legible), la del estado actual según el driver de frecuencia. */
static bool freq_controlavel(void)
{
    return freq_ok && (freq.estado == FREQ_EST_ATIVO || freq.estado == FREQ_EST_DESATIVADO);
}

static unsigned freq_mostrada(const char **orig)
{
    unsigned f = sens_ok ? sensores_freq_atual(&sens, orig) : 0;
    if (!f && freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL && freq.atual >= 0 &&
        freq.atual < (int)freq.qtd_estados && freq.estado_khz[freq.atual]) {
        f = freq.estado_khz[freq.atual];
        if (orig) *orig = freq.mecanismo == FREQ_MEC_SIMULADO ? "simulado" : "driver";
    }
    return f;
}

static void hist_por(unsigned *h, unsigned v)
{
    if (qtd_hist_sens < HIST) { h[qtd_hist_sens] = v; return; }
    for (int i = 1; i < HIST; i++) h[i - 1] = h[i];
    h[HIST - 1] = v;
}

static void amostrar_gpu_bateria(void);
static unsigned hist_g2d[HIST], hist_g3d[HIST];
static unsigned gpu2d_s, gpu3d_s;

static void amostrar_sensores(void)
{
    sens_ok = sensores_ler(&sens);
    freq_ok = frequencia_ler(&freq);
    ler_freq_nucleos();
    int t = sens_ok ? sensores_temp_maior(&sens) : -1;
    unsigned f = freq_mostrada(NULL);
    /* Temperatura: de 0 al TjMax (o 100 °C); frecuencia: de 0 a la mayor entre
     * la nominal, la máxima anunciada y la mayor vista (el turbo supera la nominal). */
    int tmax = sens.max_mili_c > 0 ? sens.max_mili_c : 100000;
    if (freq_escala < sens.tsc_khz) freq_escala = sens.tsc_khz;
    if (freq_escala < sens.max_khz) freq_escala = sens.max_khz;
    if (freq_escala < f) freq_escala = f;
    if (freq_ok && freq.qtd_estados && freq_escala < freq.estado_khz[0]) freq_escala = freq.estado_khz[0];
    unsigned vt = t > 0 ? (unsigned)t * 1000u / (unsigned)tmax : 0;   /* t < 200000: cabe en 32 bits */
    unsigned vf = freq_escala ? f / (freq_escala / 1000 ? freq_escala / 1000 : 1) : 0;
    hist_por(hist_temp, vt > 1000 ? 1000 : vt);
    hist_por(hist_freq, vf > 1000 ? 1000 : vf);
    amostrar_gpu_bateria();
    hist_por(hist_g2d, gpu2d_s);
    hist_por(hist_g3d, gpu3d_s);
    if (qtd_hist_sens < HIST) qtd_hist_sens++;
}

static void sem_dados(int x, int y, int l, int a, const char *t)
{
    g_retangulo(j, x, y, l, a, 0x10181E);
    g_moldura(j, x, y, l, a, COR_BORDA);
    int w = g_largura_texto(t);
    g_texto_n(j, x + (w < l - 16 ? (l - w) / 2 : 8), y + a / 2 - LETRA_A / 2, t, (l - 16) / LETRA_L, 0x9AA8B8);
}

/* Ítems de la lista de ventanas. */
static char itens_jan[MAXJ][64];
static int ids_jan[MAXJ];
static lista_t lista_jan = { TAB_X, 0, 0, 0, itens_jan, 0, -1, 0, NULL };

static void preparar_lista_janelas(void)
{
    /* De arriba abajo, como se ve en la pantalla. */
    int n = 0;
    lista_jan.sel = -1;
    for (int k = qtd_jans - 1; k >= 0; k--, n++) {
        const proc_t *p = proc_por_pid(jans[k].pid);
        formatar_texto(itens_jan[n], sizeof itens_jan[n], "%-30s %s (PID %d)%s", jans[k].titulo,
                       p ? p->p.nome : "?", jans[k].pid, jans[k].foco ? "  [activa]" : "");
        ids_jan[n] = jans[k].id;
        if (jans[k].id == sel_jan) lista_jan.sel = n;
    }
    lista_jan.qtd = n;
    if (lista_jan.sel < 0) sel_jan = -1;
    /* En el log, la lista en el orden mostrado, sólo cuando cambia. */
    static char antes[240];
    char t[240];
    t[0] = 0;
    for (int k = qtd_jans - 1; k >= 0; k--) {
        char item[64];
        formatar_texto(item, sizeof item, "%s%d %s", k == qtd_jans - 1 ? "" : "; ", jans[k].id, jans[k].titulo);
        junta_texto(t, item, sizeof t);
    }
    if (compara_texto(t, antes) != 0) {
        registrar("tareas: ventanas %d: %s", qtd_jans, t);
        copia_texto(antes, t, sizeof antes);
    }
}

static void desenhar_janelas(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    g_texto(j, TAB_X, TAB_Y + 2, _("Ventana                        Proceso"), COR_TEXTO_FRACO);
    preparar_lista_janelas();
    lista_desenhar(j, &lista_jan);
}

/* ------------------------------------------------------------ aba Servicios (D216) */

#define MAXSV 24
static servico_info_t servs[MAXSV];
static int qtd_servs = -1;                   /* -1: sin supervisor */
static char itens_serv[MAXSV][64];
static char sel_serv[32];                    /* nombre del seleccionado */
static lista_t lista_serv = { TAB_X, 0, 0, 0, itens_serv, 0, -1, 0, NULL };
static char aviso_serv[96];                  /* la respuesta de la última orden */

static void amostrar_servicos(void)
{
    qtd_servs = servicos_ler_estado(servs, MAXSV);
}

static const servico_info_t *serv_selecionado(void)
{
    for (int i = 0; i < qtd_servs; i++)
        if (compara_texto(servs[i].nome, sel_serv) == 0) return &servs[i];
    return NULL;
}

static void preparar_lista_servicos(void)
{
    int n = qtd_servs > 0 ? qtd_servs : 0;
    lista_serv.sel = -1;
    unsigned agora = tempo_ms();
    for (int i = 0; i < n; i++) {
        const servico_info_t *s = &servs[i];
        const proc_t *p = s->pid ? proc_por_pid(s->pid) : NULL;
        char pid[12], mem[16], t[16];
        formatar_texto(pid, sizeof pid, s->pid ? "%d" : "-", s->pid);
        if (p) formatar_texto(mem, sizeof mem, "%u KB", p->p.memoria_kb); else copia_texto(mem, "-", sizeof mem);
        if (compara_texto(s->estado, SERV_E_MARCHA) == 0) fmt_tempo(t, sizeof t, agora - s->desde_ms);
        else copia_texto(t, "-", sizeof t);
        formatar_texto(itens_serv[i], sizeof itens_serv[i], "%-14s %-11s %6s %8s %9s %4u  %s", s->nome,
                       servicos_texto_estado(s->estado), pid, mem, t, s->reinicios, s->arrancar ? _("sí") : "no");
        if (compara_texto(s->nome, sel_serv) == 0) lista_serv.sel = i;
    }
    lista_serv.qtd = n;
    if (lista_serv.sel < 0) sel_serv[0] = 0;
}

static void desenhar_servicos(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    if (qtd_servs < 0) {
        g_texto(j, TAB_X, TAB_Y + 2, _("El supervisor de servicios no está en marcha."), COR_TEXTO);
        return;
    }
    char cab[80];
    formatar_texto(cab, sizeof cab, "%-14s %-11s %6s %8s %9s %4s  %s", _("Nombre"), _("Estado"), _("PID"), _("Memoria"), _("Tiempo"),
                   _("Rein."), _("Auto"));
    g_texto(j, TAB_X + 4, TAB_Y + 2, cab, COR_TEXTO_FRACO);
    preparar_lista_servicos();
    lista_desenhar(j, &lista_serv);
    /* Debajo de la lista: la descripción y el detalle del seleccionado, o la última respuesta. */
    const servico_info_t *s = serv_selecionado();
    char t[160];
    if (s) formatar_texto(t, sizeof t, "%s%s%s", s->descricao[0] ? s->descricao : s->nome,
                          s->detalhe[0] ? " - " : "", s->detalhe);
    else copia_texto(t, aviso_serv[0] ? aviso_serv : _("Selecciona un servicio de la lista."), sizeof t);
    g_texto_n(j, TAB_X, lista_serv.y + lista_serv.a + 4, t, TAB_L / LETRA_L, COR_TEXTO_FRACO);
}

static void registrar_servicos(void)
{
    static char antes[240];
    char t[240];
    t[0] = 0;
    for (int i = 0; i < qtd_servs; i++) {
        char item[64];
        formatar_texto(item, sizeof item, "%s%s %s %d", i ? "; " : "", servs[i].nome, servs[i].estado, servs[i].pid);
        junta_texto(t, item, sizeof t);
    }
    if (compara_texto(t, antes) != 0) {
        registrar("tareas: servicios %d: %s", qtd_servs, t);
        copia_texto(antes, t, sizeof antes);
    }
}

/* Los botones de la pestaña: Iniciar/Parar (b_acao), Reiniciar y Activar/Desactivar. */
static void ordem_servico(const char *ordem)
{
    const servico_info_t *s = serv_selecionado();
    if (!s) { mensagem(N_("Servicios"), _("Selecciona primero un servicio de la lista.")); return; }
    char nome[32], r[160];
    copia_texto(nome, s->nome, sizeof nome);
    int c = COM_ADMIN("manejar los servicios del sistema", servicos_pedir(ordem, nome, r, sizeof r));   /* D269 */
    registrar("tareas: servicio %s %s -> %d (%s)", ordem, nome, c, r);
    if (c < 0) mensagem("Servicios", c == ERRO_NAO_EXISTE ? _("El supervisor de servicios no está en marcha.")
                                     : c == ERRO_ACESSO ? _("Hace falta ser administrador para manejar los servicios.\n"
                                                          "Usa «admin servicio ...» en el Terminal.")   /* D255 */
                                                        : _("El supervisor de servicios no contesta."));
    else if (c > 0) mensagem("Servicios", r);
    else copia_texto(aviso_serv, r, sizeof aviso_serv);
}

/* ------------------------------------------------------------ aba Red (D079) */

static bool dentro(const evento_t *e, int x, int y, int l, int a);

#define MAXIF 6   /* D445: eth0, eth1, wlan0, wg0 y lo caben */
#define MAXS  64

static info_rede_t ifs[MAXIF], ifs_ant[MAXIF];
static int qtd_ifs;
static bool tem_ifs_ant;
static unsigned t_red_ant;
static unsigned vel_rx[MAXIF], vel_tx[MAXIF];        /* bytes/s na última amostra */
static int if_sel;                                    /* interface mostrada */
static info_trafego_t trafego;                        /* eth0: el kernel guarda el minuto */
static unsigned hist_lo_rx[HIST], hist_lo_tx[HIST];   /* lo: sólo mientras la pestaña está abierta */
static int qtd_hist_lo;
static info_socket_t socks[MAXS];
static int qtd_socks;
static unsigned sel_sock = 0;                         /* id del kernel + 1000 * tipo; 0 = ninguna */
static int topo_socks;

static botao_t b_if[MAXIF];
static botao_t b_renovar  = { 0, 0, 120, 22, N_("Renovar DHCP"), false };
static botao_t b_ativar   = { 0, 0, 168, 22, N_("Desactivar interfaz"), false };

static const char *nome_estado_tcp(const info_socket_t *s)
{
    static const char *n[] = { "CERRADA", "ESCUCHANDO", "SYN_ENVIADO", "SYN_RECIBIDO", "ESTABLECIDA",
                               "FIN_ESPERA", "CIERRE_ESPERA", "ÚLTIMO_ACK" };
    if (s->c.tipo == SOCK_UDP) return "-";
    return s->estado_tcp <= 7 ? n[s->estado_tcp] : "?";
}

/* 1536 -> "1,5 KB" (vírgula decimal, como no resto da interface). */
static void fmt_bytes(char *t, size_t max, unsigned b)
{
    static const char *u[] = { "B", "KB", "MB", "GB" };
    if (b < 1024) { formatar_texto(t, max, "%u B", b); return; }
    unsigned k = 0, inteiro = b, resto = 0;
    while (inteiro >= 1024 && k < 3) { resto = inteiro % 1024; inteiro /= 1024; k++; }
    unsigned dec = resto * 10 / 1024;
    if (inteiro >= 100) formatar_texto(t, max, "%u %s", inteiro, u[k]);
    else formatar_texto(t, max, "%u,%u %s", inteiro, dec, u[k]);
}

static unsigned por_segundo(unsigned agora, unsigned antes, unsigned dt)
{
    unsigned d = agora - antes;   /* con vuelta de los contadores de 32 bits */
    if (!dt) return 0;
    return d < 4000000u ? d * 1000u / dt : d / dt * 1000u;
}

static unsigned id_sock(const info_socket_t *s) { return s->id ? s->id : 0x10000u + s->c.porta_local; }

static bool red_mudou;     /* una acción cambió algo: consultar de nuevo ya */

static void amostrar_red(void)
{
    unsigned agora = tempo_ms(), dt = agora - t_red_ant;
    /* Velocidad sólo con al menos medio segundo de intervalo: una consulta
     * justo después de un clic daría un número sin sentido. */
    bool medir = !tem_ifs_ant || dt >= 500;
    static info_rede_t v[MAXIF];
    int n = info_sistema(INFO_INTERFACES, v, MAXIF);
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++) {
        ifs[i] = v[i];
        if (!medir) continue;
        vel_rx[i] = vel_tx[i] = 0;
        if (tem_ifs_ant && i < qtd_ifs && compara_texto(ifs_ant[i].nome, v[i].nome) == 0) {
            vel_rx[i] = por_segundo(v[i].rx_bytes, ifs_ant[i].rx_bytes, dt);
            vel_tx[i] = por_segundo(v[i].tx_bytes, ifs_ant[i].tx_bytes, dt);
        }
        ifs_ant[i] = v[i];
    }
    qtd_ifs = n;
    if (if_sel >= qtd_ifs) if_sel = 0;
    info_sistema(INFO_TRAFEGO, &trafego, sizeof trafego);
    if (aba == 3) {   /* las conexiones solo con la pestaña Red */
        int k = info_sistema(INFO_SOCKETS, socks, MAXS);
        qtd_socks = k < 0 ? 0 : k;
    }
    if (!medir) return;
    bool primeira = !tem_ifs_ant;
    tem_ifs_ant = true;
    t_red_ant = agora;
    for (int i = 0; i < qtd_ifs && !primeira; i++) {
        if (!ifs[i].loopback) continue;
        if (qtd_hist_lo == HIST) {
            memmove(hist_lo_rx, hist_lo_rx + 1, (HIST - 1) * sizeof hist_lo_rx[0]);
            memmove(hist_lo_tx, hist_lo_tx + 1, (HIST - 1) * sizeof hist_lo_tx[0]);
            qtd_hist_lo--;
        }
        hist_lo_rx[qtd_hist_lo] = vel_rx[i];
        hist_lo_tx[qtd_hist_lo] = vel_tx[i];
        qtd_hist_lo++;
    }
}

/* ---- ordenación de las conexiones, como la tabla de procesos */
enum { S_PROTO, S_LOCAL, S_REMOTO, S_ESTADO, S_PID, S_PROC, QTD_COL_S };
static const char *titulos_s[QTD_COL_S] = { N_("Proto"), N_("Local"), N_("Remoto"), N_("Estado"), N_("PID"), N_("Proceso") };
static int larg_s[QTD_COL_S];
static int ordem_s = S_PROTO;
static bool desc_s;
static int ordem_socks[MAXS];

static int comparar_socks(const info_socket_t *a, const info_socket_t *b)
{
    long long d = 0;
    const proc_t *pa = proc_por_pid(a->c.pid), *pb = proc_por_pid(b->c.pid);
    switch (ordem_s) {
    case S_PROTO:  d = (long long)a->c.tipo - b->c.tipo; break;
    case S_LOCAL:  d = (long long)a->c.porta_local - b->c.porta_local; break;
    case S_REMOTO: d = a->c.ip_remoto != b->c.ip_remoto ? (a->c.ip_remoto < b->c.ip_remoto ? -1 : 1)
                                                         : (long long)a->c.porta_remota - b->c.porta_remota; break;
    case S_ESTADO: d = compara_texto(nome_estado_tcp(a), nome_estado_tcp(b)); break;
    case S_PID:    d = a->c.pid - b->c.pid; break;
    case S_PROC:   d = compara_texto(pa ? pa->p.nome : "", pb ? pb->p.nome : ""); break;
    }
    if (d == 0) d = (long long)a->c.porta_local - b->c.porta_local;
    else if (desc_s) d = -d;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

static void ordenar_socks(void)
{
    for (int i = 0; i < qtd_socks; i++) ordem_socks[i] = i;
    for (int i = 1; i < qtd_socks; i++) {
        int x = ordem_socks[i], k = i - 1;
        while (k >= 0 && comparar_socks(&socks[ordem_socks[k]], &socks[x]) > 0) { ordem_socks[k + 1] = ordem_socks[k]; k--; }
        ordem_socks[k + 1] = x;
    }
}

static const info_socket_t *sock_selecionado(void)
{
    for (int i = 0; i < qtd_socks; i++)
        if (id_sock(&socks[i]) == sel_sock) return &socks[i];
    return NULL;
}

/* ---- disposición: todo calculado a partir del panel (que cambia con la ventana) */
#define RED_INFO_Y  (TAB_Y + 28)
#define RED_LINHA   17
#define RED_GRAF_Y  (RED_INFO_Y + 5 * RED_LINHA + 6)
static int red_graf_a(void)
{
    int a = TAB_A / 5;
    return a < 56 ? 56 : a > 150 ? 150 : a;
}
#define RED_TAB_Y   (RED_GRAF_Y + red_graf_a() + 8)
#define RED_TAB_A   (TAB_Y + TAB_A - RED_TAB_Y)

static int linhas_socks(void) { int n = (RED_TAB_A - CAB_A - 4) / LIN_A; return n < 1 ? 1 : n; }

static void layout_red(void)
{
    for (int i = 0; i < MAXIF; i++) b_if[i] = (botao_t){ TAB_X + i * 66, TAB_Y, 62, 22, "", false };
    b_ativar.x = TAB_X + TAB_L - b_ativar.l;
    b_renovar.x = b_ativar.x - 8 - b_renovar.l;
    b_ativar.y = b_renovar.y = TAB_Y;
    /* Colunas: Local, Remoto e Proceso dividem o que sobra da largura. */
    int sobra = TAB_L - 2 - BARRA_L - (56 + 120 + 52);
    larg_s[S_PROTO] = 56; larg_s[S_ESTADO] = 120; larg_s[S_PID] = 52;
    larg_s[S_LOCAL] = sobra * 3 / 8; larg_s[S_REMOTO] = sobra * 3 / 8;
    larg_s[S_PROC] = sobra - larg_s[S_LOCAL] - larg_s[S_REMOTO];
}

static void endereco(char *t, size_t max, uint32_t ip, unsigned porta)
{
    char a[16];
    if (!ip && !porta) { copia_texto(t, "-", max); return; }
    if (!ip) { formatar_texto(t, max, "*:%u", porta); return; }
    ip_para_texto(ip, a);
    if (porta) formatar_texto(t, max, "%s:%u", a, porta);
    else formatar_texto(t, max, "%s:*", a);
}

static void desenhar_grafico_red(int x, int y, int l, int a)
{
    const unsigned *rx, *tx;
    int n;
    static unsigned rx_lo[HIST], tx_lo[HIST];
    if (qtd_ifs && ifs[if_sel].loopback) {
        for (int i = 0; i < qtd_hist_lo; i++) { rx_lo[i] = hist_lo_rx[i]; tx_lo[i] = hist_lo_tx[i]; }
        rx = rx_lo; tx = tx_lo; n = qtd_hist_lo;
    } else {
        n = (int)trafego.amostras;
        rx = trafego.rx + TRAFEGO_AMOSTRAS - n;
        tx = trafego.tx + TRAFEGO_AMOSTRAS - n;
    }
    /* Escala: la potencia de 2 justo por encima del mayor valor del minuto (1 KB/s
     * como mínimo), que da etiquetas redondas: 64 KB/s, 2 MB/s. */
    unsigned maior = 1024;
    for (int i = 0; i < n; i++) { if (rx[i] > maior) maior = rx[i]; if (tx[i] > maior) maior = tx[i]; }
    unsigned escala = 1024;
    while (escala < maior && escala < 0x80000000u) escala <<= 1;
    g_retangulo(j, x, y, l, a, 0x10181E);
    for (int k = 1; k < 4; k++) g_retangulo(j, x, y + a * k / 4, l, 1, 0x26343C);
    for (int k = 1; k < 6; k++) g_retangulo(j, x + l * k / 6, y, 1, a, 0x26343C);
    int passo = l / HIST;
    if (passo < 1) passo = 1;
    for (int serie = 0; serie < 2; serie++) {
        const unsigned *v = serie ? tx : rx;
        uint32_t cor = serie ? 0xFFA94D : 0x3CDC6E;
        int px_ant = -1, py_ant = 0;
        for (int i = 0; i < n; i++) {
            int px = x + l - (n - i) * passo;
            unsigned frac = escala >= 4000000u ? v[i] / (escala / 1000) : v[i] * 1000u / escala;
            if (frac > 1000) frac = 1000;
            int py = y + a - 1 - (int)((unsigned)(a - 2) * frac / 1000);
            if (px_ant >= 0) g_linha(j, px_ant, py_ant, px, py, cor);
            g_linha(j, px, py, px + passo - 1, py, cor);
            px_ant = px + passo - 1; py_ant = py;
        }
    }
    g_moldura(j, x, y, l, a, COR_BORDA);
    char t[48], e[24];
    fmt_bytes(e, sizeof e, escala);
    formatar_texto(t, sizeof t, "%s/s", e);
    g_texto(j, x + 4, y + 2, t, 0x9AA8B8);
    g_texto(j, x + l - 4 - g_largura_texto(_("últimos 60 s")), y + 2, _("últimos 60 s"), 0x9AA8B8);
    static unsigned escala_registrada, n_registrado;
    if (escala != escala_registrada || (n != (int)n_registrado && n % 10 == 0)) {
        unsigned soma_rx = 0, soma_tx = 0;
        for (int i = 0; i < n; i++) { soma_rx += rx[i]; soma_tx += tx[i]; }
        registrar("tareas: grafica red %s escala %u B/s muestras %d suma rx %u tx %u", qtd_ifs ? ifs[if_sel].nome : "-",
                  escala, n, soma_rx, soma_tx);
        escala_registrada = escala;
        n_registrado = (unsigned)n;
    }
}

static void desenhar_red(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    char t[160], a[16], b[16], c[24], d[24];
    if (!qtd_ifs) { g_texto(j, TAB_X, TAB_Y + 4, _("No hay interfaces de red."), COR_TEXTO); return; }
    /* Selector de la interfaz y sus acciones. Los botones b_if los comparte el panel Red de
     * Rendimiento, que los coloca en otro sitio: aquí se vuelven a colocar en cada dibujo (solo el
     * sitio y el tamaño: «apertado» se conserva, o el clic se perdería entre pulsar y soltar). */
    for (int i = 0; i < qtd_ifs && i < MAXIF; i++) {
        b_if[i].x = TAB_X + i * 66; b_if[i].y = TAB_Y; b_if[i].l = 62; b_if[i].a = 22;
        b_if[i].texto = ifs[i].nome;
        botao_desenhar(j, &b_if[i]);
        if (i == if_sel) g_retangulo(j, b_if[i].x + 2, b_if[i].y + b_if[i].a - 3, b_if[i].l - 4, 2, COR_DESTAQUE);
    }
    const info_rede_t *r = &ifs[if_sel];
    if (!r->loopback) {
        b_ativar.texto = r->desativada ? _("Activar interfaz") : _("Desactivar interfaz");
        botao_desenhar(j, &b_ativar);
        if (r->metodo != REDE_ESTATICO || !r->ip) botao_desenhar(j, &b_renovar);
    }
    int x1 = TAB_X, x2 = TAB_X + (TAB_L > 700 ? TAB_L / 2 : 320), y = RED_INFO_Y;
    const char *estado = !r->ativa ? "sin driver activo" : r->desativada ? "Desactivada" : r->loopback ? "Activa" :
                         r->cabo ? "Conectada" : "Desconectada (sin cable)";
    formatar_texto(t, sizeof t, _("Estado: %s"), estado);
    g_texto(j, x1, y, t, r->desativada || (!r->cabo && !r->loopback) ? 0xB03A2E : COR_TEXTO);
    formatar_texto(t, sizeof t, _("Driver: %s%s%s"), r->driver[0] ? r->driver : "-", r->descricao[0] ? " - " : "", r->descricao);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    if (r->loopback) copia_texto(t, "MAC: -", sizeof t);
    else formatar_texto(t, sizeof t, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", r->mac[0], r->mac[1], r->mac[2], r->mac[3],
                        r->mac[4], r->mac[5]);
    g_texto(j, x1, y, t, COR_TEXTO);
    int bits = 0;
    for (uint32_t m = r->mascara; m & 0x80000000u; m <<= 1) bits++;
    if (r->ip) formatar_texto(t, sizeof t, _("IPv4: %s/%d (%s)"), ip_para_texto(r->ip, a), bits, ip_para_texto(r->mascara, b));
    else copia_texto(t, "IPv4: -", sizeof t);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    if (r->loopback) copia_texto(t, _("Configuración: fija (127.0.0.1)"), sizeof t);
    else if (r->metodo == REDE_POR_DHCP)
        formatar_texto(t, sizeof t, "Configuración: DHCP (quedan %u:%02u:%02u)", r->concessao_s / 3600, r->concessao_s / 60 % 60,
                       r->concessao_s % 60);
    else formatar_texto(t, sizeof t, _("Configuración: %s"), r->metodo == REDE_ESTATICO ? _("estática") : _("sin dirección"));
    g_texto(j, x1, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Puerta: %s  DNS: %s"), r->roteador ? ip_para_texto(r->roteador, a) : "-",
                   r->dns ? ip_para_texto(r->dns, b) : "-");
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    fmt_bytes(c, sizeof c, r->tx_bytes);
    fmt_bytes(d, sizeof d, r->rx_bytes);
    formatar_texto(t, sizeof t, _("Enviado: %s (%u paquetes)"), c, r->tx_pacotes);
    g_texto(j, x1, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Recibido: %s (%u paquetes)"), d, r->rx_pacotes);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    fmt_bytes(c, sizeof c, vel_tx[if_sel]);
    fmt_bytes(d, sizeof d, vel_rx[if_sel]);
    g_retangulo(j, x1, y + 5, 10, 4, 0xFFA94D);
    formatar_texto(t, sizeof t, _("Envío: %s/s"), c);
    g_texto(j, x1 + 14, y, t, COR_TEXTO);
    g_retangulo(j, x1 + 200, y + 5, 10, 4, 0x3CDC6E);
    formatar_texto(t, sizeof t, _("Recepción: %s/s"), d);
    g_texto(j, x1 + 214, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Errores: %u  Descartes: %u"), r->rx_erros + r->tx_erros, r->descartados);
    g_texto_n(j, x2 + (x2 - x1 < 420 ? 110 : 0), y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    desenhar_grafico_red(TAB_X, RED_GRAF_Y, TAB_L, red_graf_a());

    /* Conexiones */
    int ty = RED_TAB_Y, ta = RED_TAB_A;
    g_retangulo(j, TAB_X, ty, TAB_L, ta, COR_BRANCO);
    g_moldura(j, TAB_X, ty, TAB_L, ta, COR_BORDA);
    int x = TAB_X + 1;
    for (int k = 0; k < QTD_COL_S; k++) {
        bool atual = k == ordem_s;
        g_retangulo(j, x, ty + 1, larg_s[k], CAB_A, atual ? 0xD6E4D9 : COR_BOTAO);
        g_relevo(j, x, ty + 1, larg_s[k], CAB_A, false);
        formatar_texto(t, sizeof t, atual ? "%s %s" : "%s", _(titulos_s[k]), desc_s ? "v" : "^");
        texto_coluna(x, ty + 3, larg_s[k], t, k == S_PID, COR_TEXTO);
        x += larg_s[k];
    }
    ordenar_socks();
    int vis = linhas_socks();
    if (topo_socks > qtd_socks - vis) topo_socks = qtd_socks - vis;
    if (topo_socks < 0) topo_socks = 0;
    if (!qtd_socks) g_texto(j, TAB_X + 8, ty + CAB_A + 6, _("Sin conexiones."), COR_TEXTO_FRACO);
    for (int k = 0; k < vis && topo_socks + k < qtd_socks; k++) {
        const info_socket_t *s = &socks[ordem_socks[topo_socks + k]];
        int ly = ty + 1 + CAB_A + 2 + k * LIN_A;
        bool sel = id_sock(s) == sel_sock;
        uint32_t cor = sel ? COR_BRANCO : s->c.pid ? COR_TEXTO : COR_TEXTO_FRACO;
        if (sel) g_retangulo(j, TAB_X + 2, ly, TAB_L - 4 - BARRA_L, LIN_A, COR_SELECAO);
        const proc_t *p = proc_por_pid(s->c.pid);
        x = TAB_X + 1;
        for (int col = 0; col < QTD_COL_S; col++) {
            switch (col) {
            case S_PROTO:  copia_texto(t, s->c.tipo == SOCK_TCP ? "TCP" : "UDP", sizeof t); break;
            case S_LOCAL:  endereco(t, sizeof t, s->c.ip_local, s->c.porta_local); break;
            case S_REMOTO: endereco(t, sizeof t, s->c.ip_remoto, s->c.porta_remota); break;
            case S_ESTADO: copia_texto(t, nome_estado_tcp(s), sizeof t); break;
            case S_PID:    if (s->c.pid) formatar_texto(t, sizeof t, "%d", s->c.pid); else copia_texto(t, "-", sizeof t); break;
            case S_PROC:   copia_texto(t, p ? p->p.nome : s->c.pid ? "?" : _("(sin dueño)"), sizeof t); break;
            }
            texto_coluna(x, ly + 1, larg_s[col], t, col == S_PID, cor);
            x += larg_s[col];
        }
    }
    int bx = TAB_X + TAB_L - BARRA_L - 1, by = ty + 1 + CAB_A, ba = ta - CAB_A - 2;
    g_retangulo(j, bx, by, BARRA_L, ba, 0xE4E7EA);
    int pol_a = qtd_socks <= vis ? ba : ba * vis / qtd_socks;
    if (pol_a < 12) pol_a = 12;
    int pol_y = qtd_socks <= vis ? 0 : (ba - pol_a) * topo_socks / (qtd_socks - vis);
    g_retangulo(j, bx + 2, by + pol_y, BARRA_L - 4, pol_a, 0x9AA3AC);
}

/* Línea del log en cada muestra de la pestaña Red (las pruebas siguen el programa por aquí). */
static void registrar_red(void)
{
    for (int i = 0; i < qtd_ifs; i++)
        registrar("tareas: red %s %s rx %u tx %u bytes rx %u tx %u paquetes vel rx %u tx %u B/s%s", ifs[i].nome,
                  ifs[i].desativada ? "desactivada" : ifs[i].cabo ? "conectada" : "desconectada", ifs[i].rx_bytes,
                  ifs[i].tx_bytes, ifs[i].rx_pacotes, ifs[i].tx_pacotes, vel_rx[i], vel_tx[i],
                  ifs[i].metodo == REDE_POR_DHCP ? " dhcp" : "");
    static char antes[600];
    char lista[600];
    lista[0] = 0;
    for (int i = 0; i < qtd_socks; i++) {
        const info_socket_t *s = &socks[i];
        const proc_t *p = proc_por_pid(s->c.pid);
        char l[24], r[24], item[120];
        endereco(l, sizeof l, s->c.ip_local, s->c.porta_local);
        endereco(r, sizeof r, s->c.ip_remoto, s->c.porta_remota);
        formatar_texto(item, sizeof item, "%s%s %s %s %s pid %d %s", i ? "; " : "", s->c.tipo == SOCK_TCP ? "TCP" : "UDP", l, r,
                       nome_estado_tcp(s), s->c.pid, p ? p->p.nome : "-");
        junta_texto(lista, item, sizeof lista);
    }
    if (compara_texto(lista, antes) != 0) {
        registrar("tareas: conexiones %d: %s", qtd_socks, lista);
        copia_texto(antes, lista, sizeof antes);
    }
}

/* D445: SIS_CONFIG_REDE con el nombre de la interfaz elegida (la que se ve en el panel). */
static int config_em(int op, const char *nome)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_CONFIG_REDE), "b"(op), "c"(0), "d"(nome), "S"(0), "D"(0) : "memory");
    return r;
}

static void renovar_dhcp(void)
{
    const char *nome = qtd_ifs && !ifs[if_sel].loopback ? ifs[if_sel].nome : NULL;
    int r = config_em(REDE_RENOVAR, nome);
    registrar("tareas: renovar dhcp -> %d", r);
    red_mudou = true;
    if (r < 0) {
        char t[128];
        formatar_texto(t, sizeof t, _("No se pudo renovar la concesión DHCP:\n%s"), nome_erro(r));
        mensagem(_("Renovar DHCP"), t);
    }
}

static void alternar_interface(void)
{
    if (!qtd_ifs || ifs[if_sel].loopback || !ifs[if_sel].pode_desativar) return;
    bool desligar = !ifs[if_sel].desativada;
    if (desligar && !confirmar(_("Desactivar interfaz"), _("¿Desactivar eth0?\n\nSe cortará el tráfico de red hasta\nque la vuelvas a activar.")))
        return;
    int r = config_em(desligar ? REDE_DESATIVAR : REDE_ATIVAR, ifs[if_sel].nome);
    registrar("tareas: %s %s -> %d", ifs[if_sel].nome, desligar ? "desactivar" : "activar", r);
    red_mudou = true;
}

static void fechar_conexao(void)
{
    const info_socket_t *s = sock_selecionado();
    if (!s) { mensagem(_("Cerrar conexión"), _("Selecciona primero una conexión de la lista.")); return; }
    if (s->c.tipo != SOCK_TCP || !s->id) {
        mensagem(_("Cerrar conexión"), _("Solo se pueden cerrar conexiones TCP.\nUn socket UDP se cierra finalizando su proceso."));
        return;
    }
    char t[256], l[24], r[24];
    endereco(l, sizeof l, s->c.ip_local, s->c.porta_local);
    endereco(r, sizeof r, s->c.ip_remoto, s->c.porta_remota);
    const proc_t *p = proc_por_pid(s->c.pid);
    unsigned id = s->id;
    formatar_texto(t, sizeof t, "¿Cerrar la conexión TCP\n%s -> %s\nde \"%s\" (PID %d)?\n\nEl otro extremo recibirá un RST y el\nprograma, un error al leer o escribir.",
                   l, r, p ? p->p.nome : "?", s->c.pid);
    registrar("tareas: confirmar cerrar conexion id %u", id);
    if (!confirmar(_("Cerrar conexión"), t)) { registrar("tareas: cancelado"); return; }
    int res = config_rede(REDE_FECHAR, (config_rede_t *)id);
    registrar("tareas: cerrar conexion id %u -> %d", id, res);
    red_mudou = true;
    if (res < 0) {
        formatar_texto(t, sizeof t, _("No se pudo cerrar la conexión:\n%s"), nome_erro(res));
        mensagem(_("Cerrar conexión"), t);
    }
    sel_sock = 0;
}

#define MENU_RED 7
enum { MR_CERRAR, MR_SEP, MR_RENOVAR, MR_INTERFACE };

static void menu_red(const evento_t *e)
{
    const info_rede_t *r = qtd_ifs ? &ifs[if_sel] : NULL;
    const info_socket_t *s = sock_selecionado();
    char itens[160];
    formatar_texto(itens, sizeof itens, "%sCerrar conexión...\n-\n%sRenovar DHCP\n%s%s",
                   s && s->c.tipo == SOCK_TCP && s->id ? "" : "~", r && !r->loopback ? "" : "~",
                   r && !r->loopback && r->pode_desativar ? "" : "~",
                   r && r->desativada ? _("Activar interfaz") : _("Desactivar interfaz..."));
    registrar("tareas: menu red");
    menu_contextual(j->id, e->x, e->y, itens, MENU_RED);
}

static void tratar_red(const evento_t *e)
{
    if (e->tipo == EV_MENU && (int)e->tecla == MENU_RED) {
        if (e->x == MR_CERRAR) fechar_conexao();
        else if (e->x == MR_RENOVAR) renovar_dhcp();
        else if (e->x == MR_INTERFACE) alternar_interface();
        return;
    }
    for (int i = 0; i < qtd_ifs && i < MAXIF; i++)
        if (botao_tratar(j, &b_if[i], e)) { if_sel = i; registrar("tareas: interfaz %s", ifs[i].nome); return; }
    if (qtd_ifs && !ifs[if_sel].loopback) {
        if (botao_tratar(j, &b_renovar, e)) { renovar_dhcp(); return; }
        if (botao_tratar(j, &b_ativar, e)) { alternar_interface(); return; }
    }
    int ty = RED_TAB_Y, ta = RED_TAB_A;
    if (e->tipo == EV_TECLA) {
        if (e->tecla == TECLA_DELETE) { fechar_conexao(); return; }
        if (e->tecla != TECLA_BAIXO && e->tecla != TECLA_CIMA) return;
        ordenar_socks();
        int pos = -1;
        for (int i = 0; i < qtd_socks; i++) if (id_sock(&socks[ordem_socks[i]]) == sel_sock) pos = i;
        pos = pos < 0 ? 0 : pos + (e->tecla == TECLA_BAIXO ? 1 : -1);
        if (pos >= qtd_socks) pos = qtd_socks - 1;
        if (pos < 0) return;
        sel_sock = id_sock(&socks[ordem_socks[pos]]);
        if (pos < topo_socks) topo_socks = pos;
        if (pos >= topo_socks + linhas_socks()) topo_socks = pos - linhas_socks() + 1;
        return;
    }
    if (e->tipo == EV_RODA && dentro(e, TAB_X, ty, TAB_L, ta)) { topo_socks += (int)e->cliques; return; }
    if (e->tipo != EV_CLIQUE) return;
    bool direito = (e->botoes & 2) != 0;
    if (dentro(e, TAB_X, ty, TAB_L, ta)) {
        if (e->y < ty + 1 + CAB_A) {
            if (direito) return;
            int x = TAB_X + 1;
            for (int c = 0; c < QTD_COL_S; x += larg_s[c], c++) {
                if (e->x < x || e->x >= x + larg_s[c]) continue;
                if (c == ordem_s) desc_s = !desc_s;
                else { ordem_s = c; desc_s = false; }
                registrar("tareas: ordenar conexiones por %s %s", titulos_s[c], desc_s ? "desc" : "asc");
                break;
            }
            return;
        }
        if (!direito && e->x >= TAB_X + TAB_L - BARRA_L - 1) {
            topo_socks += e->y < ty + ta / 2 ? -linhas_socks() : linhas_socks();
            return;
        }
        int k = (e->y - (ty + 1 + CAB_A + 2)) / LIN_A;
        if (k >= 0 && topo_socks + k < qtd_socks) {
            ordenar_socks();
            const info_socket_t *s = &socks[ordem_socks[topo_socks + k]];
            sel_sock = id_sock(s);
            registrar("tareas: seleccionada conexion %s puerto %u pid %d", s->c.tipo == SOCK_TCP ? "TCP" : "UDP",
                      s->c.porta_local, s->c.pid);
        }
        if (direito) { desenhar_red(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); menu_red(e); }
        return;
    }
    if (direito && dentro(e, TAB_X, TAB_Y, TAB_L, RED_TAB_Y - TAB_Y)) menu_red(e);
}

/* ------------------------------------------------------------ Rendimiento (D368) */

/* Barra lateral con una miniatura por recurso y, a la derecha, el panel grande del elegido. Los datos de CPU,
 * memoria, swap y disco se muestrean siempre (son una llamada barata); los de red, GPU, sensores y batería
 * solo con la pestaña a la vista. */
enum { R_CPU, R_MEM, R_SWAP, R_DISCO, R_RED, R_GPU, R_SENS, QTD_R };
static const char *nomes_r[QTD_R] = { N_("CPU"), N_("Memoria"), N_("Swap"), N_("Disco"), N_("Red"), N_("GPU"), N_("Sensores") };
static const char *nomes_r_log[QTD_R] = { "cpu", "memoria", "swap", "disco", "red", "gpu", "sensores" };
static const uint32_t acento_r[QTD_R] = { 0x1E8A4C, 0x2A6FD8, 0x8E5BC8, 0x1C9A94, 0xD0782A, 0xC23C7E, 0xB8860B };
static int rsel;                       /* recurso elegido */

#define SB_L   176
#define RP_X   (TAB_X + SB_L + 8)
#define RP_L   (TAB_L - SB_L - 8)
#define RP_Y   TAB_Y
#define RP_A   TAB_A

static info_acel2d_t a2d;
static info_gpu3d_t g3d;
static info_gpu_energia_t gen;   /* D399: relojes, temperatura y estado de energía */
static bool a2d_ok, g3d_ok, gen_ok;
static bool log_sens_forcar = true, log_gpu_forcar = true, log_energia_forcar = true;   /* D408/D399 */
static unsigned g2d_ant, g3d_ant, t_gpu_ant;
static info_bateria_t bat;
static bool bat_ok;

static void amostrar_gpu_bateria(void)
{
    unsigned agora = tempo_ms(), dt = agora - t_gpu_ant;
    memset(&a2d, 0, sizeof a2d);
    memset(&g3d, 0, sizeof g3d);
    a2d_ok = info_sistema(INFO_ACEL2D, &a2d, sizeof a2d) == 0 && a2d.versao == 1;
    g3d_ok = info_sistema(INFO_GPU3D, &g3d, sizeof g3d) == 0 && g3d.versao == 1;
    memset(&gen, 0, sizeof gen);
    gen_ok = info_sistema(INFO_GPU_ENERGIA, &gen, sizeof gen) == 0 && gen.versao == 1;
    unsigned o2 = a2d.copias + a2d.rellenos, o3 = g3d.quadros;
    gpu2d_s = gpu3d_s = 0;
    if (t_gpu_ant && dt >= 200) {
        gpu2d_s = por_segundo(o2, g2d_ant, dt);
        gpu3d_s = por_segundo(o3, g3d_ant, dt);
    }
    g2d_ant = o2; g3d_ant = o3; t_gpu_ant = agora;
    memset(&bat, 0, sizeof bat);
    bat_ok = info_sistema(INFO_BATERIA, &bat, sizeof bat) == 0 && bat.versao == 1 && bat.presente;
}

/* Un ancho múltiplo de HIST: cada segundo ocupa los mismos píxeles. */
static int ajusta(int l) { l -= l % HIST; return l < HIST ? HIST : l; }

/* Gráfico de tasas (una o dos series) con la escala automática: la potencia de 2 por encima del mayor valor. */
static unsigned grafico_taxa(int x, int y, int l, int a, const unsigned *v1, const unsigned *v2, int n,
                             uint32_t c1, uint32_t c2, const char *un)
{
    unsigned maior = 4;
    for (int i = 0; i < n; i++) {
        if (v1 && v1[i] > maior) maior = v1[i];
        if (v2 && v2[i] > maior) maior = v2[i];
    }
    unsigned escala = 4;
    while (escala < maior && escala < 0x80000000u) escala <<= 1;
    g_retangulo(j, x, y, l, a, 0x10181E);
    for (int k = 1; k < 4; k++) g_retangulo(j, x, y + a * k / 4, l, 1, 0x26343C);
    for (int k = 1; k < 6; k++) g_retangulo(j, x + l * k / 6, y, 1, a, 0x26343C);
    int passo = l / HIST;
    if (passo < 1) passo = 1;
    for (int serie = 0; serie < 2; serie++) {
        const unsigned *v = serie ? v2 : v1;
        if (!v) continue;
        int px_ant = -1, py_ant = 0;
        for (int i = 0; i < n; i++) {
            int px = x + l - (n - i) * passo;
            unsigned frac = escala >= 4000000u ? v[i] / (escala / 1000) : v[i] * 1000u / escala;
            if (frac > 1000) frac = 1000;
            int py = y + a - 1 - (int)((unsigned)(a - 2) * frac / 1000);
            if (px_ant >= 0) g_linha(j, px_ant, py_ant, px, py, serie ? c2 : c1);
            g_linha(j, px, py, px + passo - 1, py, serie ? c2 : c1);
            px_ant = px + passo - 1; py_ant = py;
        }
    }
    g_moldura(j, x, y, l, a, COR_BORDA);
    char t[40];
    formatar_texto(t, sizeof t, "%u %s", escala, un);
    g_texto(j, x + 4, y + 2, t, 0x9AA8B8);
    return escala;
}

/* Miniatura de la barra lateral: la misma serie, normalizada al mayor valor. */
static void miniatura_taxa(int x, int y, int l, int a, const unsigned *v1, const unsigned *v2, int n, uint32_t c, uint32_t f)
{
    unsigned tmp[HIST], maior = 1;
    for (int i = 0; i < n && i < HIST; i++) {
        tmp[i] = (v1 ? v1[i] : 0) + (v2 ? v2[i] : 0);
        if (tmp[i] > maior) maior = tmp[i];
    }
    for (int i = 0; i < n && i < HIST; i++)
        tmp[i] = maior >= 4000000u ? tmp[i] / (maior / 1000) : tmp[i] * 1000u / maior;
    desenhar_grafico(x, y, l, a, tmp, n, c, f);
}

static void fmt_mb(char *t, size_t max, unsigned kb)
{
    if (kb >= 1024 * 1024) formatar_texto(t, max, "%u,%u GB", kb / (1024 * 1024), kb / (1024 * 1024 / 10) % 10);
    else formatar_texto(t, max, "%u MB", kb / 1024);
}

static void fmt_khz(char *t, size_t max, unsigned khz)
{
    if (khz >= 1000000) formatar_texto(t, max, "%u,%u GHz", khz / 1000000, khz / 100000 % 10);
    else formatar_texto(t, max, "%u MHz", khz / 1000);
}

static const info_rede_t *rede_principal(int *idx)
{
    for (int i = 0; i < qtd_ifs; i++)
        if (!ifs[i].loopback) { if (idx) *idx = i; return &ifs[i]; }
    if (idx) *idx = 0;
    return qtd_ifs ? &ifs[0] : NULL;
}

/* Texto corto y miniatura de cada recurso. */
static void item_resumo(int r, char *t, size_t max)
{
    char a[24], b[24];
    t[0] = 0;
    switch (r) {
    case R_CPU: {
        unsigned f = freq_mostrada(NULL);
        if (f) { fmt_khz(a, sizeof a, f); formatar_texto(t, max, "%u%% %s", (cpu_total10 + 5) / 10, a); }
        else formatar_texto(t, max, "%u %%", (cpu_total10 + 5) / 10);
        break;
    }
    case R_MEM: {
        unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
        fmt_mb(a, sizeof a, mem.usada_kb);
        formatar_texto(t, max, "%u%% %s", (m10 + 5) / 10, a);
        break;
    }
    case R_SWAP:
        if (!mem.troca_teto_kb) copia_texto(t, _("Desactivada"), max);
        else { fmt_mb(a, sizeof a, mem.troca_armazem_kb); formatar_texto(t, max, "%s (%u%%)", a, (qtd_hist ? hist_swap[qtd_hist - 1] + 5 : 0) / 10); }
        break;
    case R_DISCO:
        if (!disco_ok || !disco_i.ativa) copia_texto(t, _("Sin caché"), max);
        else formatar_texto(t, max, "%u KB/s", disco_rd_s + disco_wr_s);
        break;
    case R_RED: {
        int i;
        const info_rede_t *r0 = rede_principal(&i);
        if (!r0) copia_texto(t, "-", max);
        else { fmt_bytes(a, sizeof a, vel_rx[i] + vel_tx[i]); formatar_texto(t, max, "%s/s", a); }
        break;
    }
    case R_GPU:
        formatar_texto(t, max, "2D: %s", a2d_ok && a2d.ativo ? a2d.nome : "CPU");
        break;
    case R_SENS: {
        int tm = sens_ok ? sensores_temp_maior(&sens) : -1;
        if (tm > 0) formatar_texto(t, max, "%s", sensores_grau(b, sizeof b, tm));
        else if (bat_ok) formatar_texto(t, max, _("Batería %d%%"), bat.porcentagem);
        else copia_texto(t, "n/d", max);
        break;
    }
    }
}

static void desenhar_item_lateral(int r)
{
    int ia = TAB_A / QTD_R;
    if (ia > 56) ia = 56;
    int x = TAB_X, y = TAB_Y + r * ia, h = ia - 3;
    bool s = r == rsel;
    g_retangulo(j, x, y, SB_L, h, s ? COR_BRANCO : COR_FUNDO);
    g_moldura(j, x, y, SB_L, h, s ? acento_r[r] : 0xC8CDD2);
    if (s) g_retangulo(j, x, y, 4, h, acento_r[r]);
    int ta = h - 10;
    if (ta < 24) ta = 24;
    int tx = x + 8, ty = y + 5;
    switch (r) {
    case R_CPU:   desenhar_grafico(tx, ty, HIST, ta, hist_cpu, qtd_hist, 0x3CDC6E, 0x165C32); break;
    case R_MEM:   desenhar_grafico(tx, ty, HIST, ta, hist_mem, qtd_hist, 0x7EB6FF, 0x1E3E78); break;
    case R_SWAP:  desenhar_grafico(tx, ty, HIST, ta, hist_swap, qtd_hist, 0xC792EA, 0x3A2A5C); break;
    case R_DISCO: miniatura_taxa(tx, ty, HIST, ta, hist_disco_rd, hist_disco_wr, qtd_hist, 0x4DD0C8, 0x12403E); break;
    case R_RED: {
        int n = (int)trafego.amostras;
        if (n > HIST) n = HIST;
        miniatura_taxa(tx, ty, HIST, ta, trafego.rx + TRAFEGO_AMOSTRAS - n, trafego.tx + TRAFEGO_AMOSTRAS - n, n, 0xFFA94D, 0x5C3A10);
        break;
    }
    case R_GPU:   miniatura_taxa(tx, ty, HIST, ta, hist_g2d, hist_g3d, qtd_hist_sens, 0xFF7AB6, 0x5C1E3E); break;
    case R_SENS:  desenhar_grafico(tx, ty, HIST, ta, hist_temp, qtd_hist_sens, 0xFFD040, 0x5C4A10); break;
    }
    char t[40];
    g_texto(j, x + 8 + HIST + 8, ty, _(nomes_r[r]), COR_TEXTO);
    item_resumo(r, t, sizeof t);
    g_texto_n(j, x + 8 + HIST + 8, ty + 18, t, (SB_L - 8 - HIST - 8 - 4) / LETRA_L, COR_TEXTO_FRACO);
}

/* Cabecera del panel grande; devuelve la Y donde empieza el cuerpo. */
static int painel_cab(const char *tit, const char *sub, uint32_t cor)
{
    g_retangulo(j, RP_X, RP_Y, RP_L, RP_A, COR_BRANCO);
    g_moldura(j, RP_X, RP_Y, RP_L, RP_A, COR_BORDA);
    g_retangulo(j, RP_X + 1, RP_Y + 1, RP_L - 2, 22, cor);
    g_texto(j, RP_X + 8, RP_Y + 4, tit, COR_BRANCO);
    if (sub && sub[0]) {
        int w = g_largura_texto(sub);
        if (w < RP_L - 24 - g_largura_texto(tit)) g_texto(j, RP_X + RP_L - 8 - w, RP_Y + 4, sub, 0xE8F0F8);
    }
    return RP_Y + 28;
}

/* Campos «etiqueta / valor» en cuadrícula, al pie del panel. */
typedef struct { const char *r; char v[72]; } campo_t;
static campo_t cps[20];
static int ncps;
#define CP(rot, ...) do { if (ncps < 20) { cps[ncps].r = (rot); formatar_texto(cps[ncps].v, sizeof cps[0].v, __VA_ARGS__); ncps++; } } while (0)

static int campos_cols(void) { int c = (RP_L - 16) / 150; return c < 1 ? 1 : c > 4 ? 4 : c; }
static int campos_altura(void) { int c = campos_cols(); return ((ncps + c - 1) / c) * 34 + 6; }

static void campos_desenhar(void)
{
    int c = campos_cols(), cw = (RP_L - 16) / c, y0 = RP_Y + RP_A - campos_altura();
    g_retangulo(j, RP_X + 8, y0 - 3, RP_L - 16, 1, 0xDDE1E5);
    for (int k = 0; k < ncps; k++) {
        int cx = RP_X + 8 + (k % c) * cw, cy = y0 + (k / c) * 34;
        g_texto_n(j, cx, cy, cps[k].r, (cw - 8) / LETRA_L, COR_TEXTO_FRACO);
        g_texto_n(j, cx, cy + 15, cps[k].v, (cw - 8) / LETRA_L, COR_TEXTO);
    }
}

static void rot_grafico(int y, const char *esq, const char *dir)
{
    g_texto_n(j, RP_X + 8, y, esq, (RP_L - 24) / LETRA_L - 14, COR_TEXTO);
    if (dir) g_texto(j, RP_X + RP_L - 8 - g_largura_texto(dir), y, dir, COR_TEXTO_FRACO);
}

static void leyenda(int x, int y, uint32_t cor, const char *t, int *avanco)
{
    g_retangulo(j, x, y + 4, 10, 8, cor);
    g_texto(j, x + 14, y, t, COR_TEXTO);
    *avanco = 14 + g_largura_texto(t) + 14;
}

static void painel_cpu(void)
{
    char a[24], b[48], plan[24];
    ncps = 0;
    unsigned f = freq_mostrada(NULL);
    unsigned hilos = 0;
    for (int i = 0; i < qtd_procs; i++) hilos += procs[i].p.n_hilos;
    unsigned s = versao.segundos_ligado;
    CP(_("Utilización"), "%u,%u %%", cpu_total10 / 10, cpu_total10 % 10);
    if (f) { fmt_khz(a, sizeof a, f); CP(_("Frecuencia"), "%s", a); } else CP(_("Frecuencia"), "n/d");
    if (freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL) CP(_("Gobernador"), "%s%s", frequencia_politica(freq.politica), freq.limitada ? _(" (limitada)") : "");
    else CP(_("Gobernador"), _("sin control"));
    CP(_("Procesos"), "%d", qtd_procs);
    CP(_("Hilos"), "%u", hilos);
    CP(_("Encendido"), "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
    CP(_("Núcleos lógicos"), "%d", qtd_nuc);
    if (planificador_consultar(plan, sizeof plan) > 0) {
        /* el nombre interno (rr, afinidad...) es el de la orden `planificador`; aquí, uno que se entienda */
        const char *nom = plan;
        if (compara_texto(plan, "rr") == 0) nom = _("Por turnos (round-robin)");
        else if (compara_texto(plan, "afinidad") == 0) nom = _("Por afinidad de CPU");
        else if (compara_texto(plan, "equilibrio") == 0) nom = _("Equilibrado entre CPU");
        else if (compara_texto(plan, "prioridad") == 0) nom = _("Por prioridad");
        CP(_("Planificador"), "%s", nom);
    }
    if (sens_ok && sens.mult_x10) { formatar_texto(b, sizeof b, "x%u,%u", sens.mult_x10 / 10, sens.mult_x10 % 10); CP(_("Multiplicador"), "%s", b); }
    int y = painel_cab(_("CPU"), _("últimos 60 s"), acento_r[R_CPU]);
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), nuc_n = qtd_nuc > 1 ? qtd_nuc : 0;
    int th = nuc_n ? livre * 4 / 10 - 16 : livre - 16;
    if (th < 36) th = 36;
    if (th > 150) th = 150;
    formatar_texto(b, sizeof b, _("Uso total de la CPU: %u,%u %%"), cpu_total10 / 10, cpu_total10 % 10);
    rot_grafico(y, b, "100 %");
    desenhar_grafico(RP_X + 8, y + 16, gl, th, hist_cpu, qtd_hist, 0x3CDC6E, 0x165C32);
    y += 16 + th + 6;
    if (nuc_n) {
        int cols = nuc_n <= 2 ? nuc_n : nuc_n <= 4 ? 2 : 4, rows = (nuc_n + cols - 1) / cols;
        int cw = (RP_L - 16 - (cols - 1) * 6) / cols;
        int ch = (RP_Y + RP_A - campos_altura() - 4 - y - 16 - (rows - 1) * 6) / rows;
        if (ch > 64) ch = 64;
        if (ch >= 20) {
            rot_grafico(y, _("Núcleos lógicos (cada uno con su uso)"), NULL);
            for (int i = 0; i < nuc_n && i < MAXN; i++) {
                int cx = RP_X + 8 + (i % cols) * (cw + 6), cy = y + 16 + (i / cols) * (ch + 6), cl = ajusta(cw);
                if (cl > cw) cl = cw;
                desenhar_grafico(cx, cy, cl, ch, hist_nuc[i], qtd_hist, 0x3CDC6E, 0x165C32);
                char fq[32];
                freq_da_cpu(nuc[i].indice, fq, sizeof fq);
                formatar_texto(b, sizeof b, "CPU %u: %u%%%s%s", nuc[i].indice, (nuc_uso10[i] + 5) / 10, fq[0] ? " " : "", fq);
                g_texto_n(j, cx + 3, cy + 2, b, (cl - 6) / LETRA_L, 0x9AA8B8);
            }
        }
    }
    campos_desenhar();
}

static void painel_mem(void)
{
    char a[24], b[24], t[96];
    ncps = 0;
    unsigned cache = mem.cache_kb, cache_alta = mem.cache_alta_kb > cache ? cache : mem.cache_alta_kb;
    unsigned app = mem.usada_kb > cache ? mem.usada_kb - cache : 0;
    fmt_mb(a, sizeof a, mem.ram_total_kb);
    CP(_("RAM física"), "%s", a);
    fmt_mb(a, sizeof a, app);
    CP(_("En uso (programas)"), "%s", a);
    fmt_mb(a, sizeof a, mem.livre_kb);
    CP(_("Libre"), "%s", a);
    fmt_mb(a, sizeof a, cache - cache_alta); fmt_mb(b, sizeof b, cache_alta);
    CP(_("Caché de disco"), _("%s baja + %s alta"), a, b);
    formatar_texto(t, sizeof t, "%s%s%s", mem.modo & MEM_MODO_PAE ? "PAE" : _("sin PAE"), mem.modo & MEM_MODO_NX ? " + NX" : "",
                   !(mem.modo & MEM_MODO_NX) && (mem.modo & MEM_MODO_CPU_NX) ? _(" (NX apagado)") : "");
    CP(_("Modo de memoria"), "%s", t);
    if (mem.alta_gerida_kb) {
        fmt_mb(a, sizeof a, mem.alta_livre_kb); fmt_mb(b, sizeof b, mem.alta_gerida_kb);
        CP(_("Memoria alta libre"), _("%s de %s"), a, b);
    }
    if (mem.superior_gerida_kb) {
        fmt_mb(a, sizeof a, mem.superior_livre_kb); fmt_mb(b, sizeof b, mem.superior_gerida_kb);
        CP(_("Por encima de 4 GB"), _("%s de %s"), a, b);
    }
    if (mem.compartida_kb) {   /* D378 (fase 3): el código de las bibliotecas, una vez para todos */
        fmt_mb(a, sizeof a, mem.compartida_kb); fmt_mb(b, sizeof b, mem.compartida_mapeada_kb);
        CP(_("Bibliotecas compartidas"), _("%s (sin compartir: %s)"), a, b);
    }
    unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
    fmt_mb(a, sizeof a, mem.usada_kb); fmt_mb(b, sizeof b, mem.gerida_kb);
    formatar_texto(t, sizeof t, _("Memoria: %s de %s (%u %%)"), a, b, (m10 + 5) / 10);
    int y = painel_cab(_("Memoria"), _("últimos 60 s"), acento_r[R_MEM]);
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = livre - 16 - 60;
    if (th < 36) th = 36;
    if (th > 160) th = 160;
    rot_grafico(y, t, "100 %");
    desenhar_grafico(RP_X + 8, y + 16, gl, th, hist_mem, qtd_hist, 0x7EB6FF, 0x1E3E78);
    y += 16 + th + 8;
    /* Barra apilada: programas, caché baja, caché alta, libre. */
    rot_grafico(y, _("Reparto de la memoria"), NULL);
    y += 18;
    unsigned seg[4] = { app, cache - cache_alta, cache_alta, mem.livre_kb }, tot = 0;
    for (int i = 0; i < 4; i++) tot += seg[i];
    while (tot > 4000000u) { for (int i = 0; i < 4; i++) seg[i] >>= 4; tot >>= 4; }
    if (!tot) tot = 1;
    static const uint32_t cs[4] = { 0x2A6FD8, 0x3FB58A, 0xA5DDB8, 0xDDE3E8 };
    int bl = RP_L - 16, bx = RP_X + 8, acc = 0;
    g_moldura(j, bx - 1, y - 1, bl + 2, 20, COR_BORDA);
    for (int i = 0; i < 4; i++) {
        int w = i == 3 ? bl - acc : (int)((unsigned long long)bl * seg[i] / tot);
        if (w > bl - acc) w = bl - acc;
        g_retangulo(j, bx + acc, y, w, 18, cs[i]);
        acc += w;
    }
    y += 22;
    int x = bx, av;
    static const char *lg[4] = { N_("En uso"), N_("Caché"), N_("Caché alta"), N_("Libre") };
    for (int i = 0; i < 4; i++) {
        const char *tx = _(lg[i]);
        if (x + 14 + g_largura_texto(tx) > bx + bl) break;
        leyenda(x, y, cs[i], tx, &av);
        x += av;
    }
    campos_desenhar();
}

static void painel_swap(void)
{
    char a[24], b[24], c[24];
    ncps = 0;
    int y = painel_cab(_("Swap (comprimida en RAM)"), mem.troca_teto_kb ? _("últimos 60 s") : "", acento_r[R_SWAP]);
    if (!mem.troca_teto_kb) {
        g_texto(j, RP_X + 12, y + 8, _("La swap está desactivada (arranque con --sin-swap)."), COR_TEXTO);
        g_texto(j, RP_X + 12, y + 28, _("Sin ella, los programas solo pueden usar la RAM libre."), COR_TEXTO_FRACO);
        return;
    }
    unsigned razao10 = mem.troca_comprimida_kb ? mem.troca_kb * 10u / mem.troca_comprimida_kb : 0;
    unsigned u10 = qtd_hist ? hist_swap[qtd_hist - 1] : 0;
    fmt_mb(a, sizeof a, mem.troca_teto_kb);
    CP(_("Tope del almacén"), "%s", a);
    fmt_mb(a, sizeof a, mem.troca_armazem_kb);
    CP(_("Usada (almacén)"), "%s (%u,%u %%)", a, u10 / 10, u10 % 10);
    fmt_mb(a, sizeof a, mem.troca_kb);
    CP(_("Páginas en la swap"), _("%s sin comprimir"), a);
    fmt_mb(a, sizeof a, mem.troca_comprimida_kb);
    CP(_("Comprimida en RAM"), "%s", a);
    CP(_("Ratio de compresión"), razao10 ? "%u,%u : 1" : "-", razao10 / 10, razao10 % 10);
    CP(_("Sacadas por segundo"), _("%u págs/s"), swap_s_s);
    CP(_("Traídas por segundo"), _("%u págs/s"), swap_e_s);
    formatar_texto(a, sizeof a, "%u", mem.troca_saidas);
    formatar_texto(b, sizeof b, "%u", mem.troca_entradas);
    formatar_texto(c, sizeof c, "%s / %s", a, b);
    CP(_("Total sacadas / traídas"), "%s", c);
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = (livre - 2 * 16 - 8) / 2;
    if (th < 30) th = 30;
    if (th > 130) th = 130;
    rot_grafico(y, _("Ocupación de la swap"), "100 %");
    desenhar_grafico(RP_X + 8, y + 16, gl, th, hist_swap, qtd_hist, 0xC792EA, 0x3A2A5C);
    y += 16 + th + 8;
    int av;
    rot_grafico(y, "", NULL);
    leyenda(RP_X + 8, y, 0xFFA94D, _("sacadas"), &av);
    leyenda(RP_X + 8 + av, y, 0x3CDC6E, _("traídas"), &av);
    grafico_taxa(RP_X + 8, y + 16, gl, th, hist_swap_s, hist_swap_e, qtd_hist, 0xFFA94D, 0x3CDC6E, _("págs/s"));
    campos_desenhar();
}

static void painel_disco(void)
{
    char a[24];
    ncps = 0;
    int y = painel_cab(_("Disco (caché de bloques)"), disco_ok && disco_i.ativa ? _("últimos 60 s") : "", acento_r[R_DISCO]);
    if (!disco_ok || !disco_i.ativa) {
        g_texto(j, RP_X + 12, y + 8, _("No hay caché de disco: el kernel no cuenta los accesos."), COR_TEXTO);
        g_texto(j, RP_X + 12, y + 28, _("(imagen con --sem-cache-disco o disco sin caché)"), COR_TEXTO_FRACO);
        return;
    }
    unsigned tot = disco_i.acertos + disco_i.falhas, taxa = tot ? (unsigned)((unsigned long long)disco_i.acertos * 100 / tot) : 0;
    CP(_("Lectura"), "%u KB/s", disco_rd_s);
    CP(_("Escritura"), "%u KB/s", disco_wr_s);
    fmt_mb(a, sizeof a, disco_i.blocos * 4);
    CP(_("Caché ocupada"), "%s (%u bloques)", a, disco_i.blocos);
    fmt_mb(a, sizeof a, disco_i.max_blocos * 4);
    CP(_("Tope de la caché"), "%s", a);
    CP(_("Pendiente de grabar"), _("%u bloques"), disco_i.sujos);
    CP(_("Aciertos de caché"), "%u %%", taxa);
    CP(_("Leído por adelantado"), _("%u bloques"), disco_i.anticipados);
    CP(_("Grabados / errores"), "%u / %u", disco_i.gravados, disco_i.erros);
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = livre - 16;
    if (th < 36) th = 36;
    if (th > 200) th = 200;
    int av;
    leyenda(RP_X + 8, y, 0x4DD0C8, _("lectura"), &av);
    leyenda(RP_X + 8 + av, y, 0xFFA94D, _("escritura"), &av);
    grafico_taxa(RP_X + 8, y + 16, gl, th, hist_disco_rd, hist_disco_wr, qtd_hist, 0x4DD0C8, 0xFFA94D, "KB/s");
    campos_desenhar();
}

static void painel_red(void)
{
    char a[24], b[24], c[24];
    ncps = 0;
    int y = painel_cab(_("Red"), _("últimos 60 s"), acento_r[R_RED]);
    if (!qtd_ifs) { g_texto(j, RP_X + 12, y + 8, _("No hay interfaces de red."), COR_TEXTO); return; }
    for (int i = 0; i < qtd_ifs && i < MAXIF; i++) {
        b_if[i] = (botao_t){ RP_X + 8 + i * 70, y, 66, 22, ifs[i].nome, false };
        botao_desenhar(j, &b_if[i]);
        if (i == if_sel) g_retangulo(j, b_if[i].x + 2, b_if[i].y + b_if[i].a - 3, b_if[i].l - 4, 2, COR_DESTAQUE);
    }
    y += 28;
    const info_rede_t *r = &ifs[if_sel];
    const char *estado = !r->ativa ? _("sin driver activo") : r->desativada ? _("Desactivada") : r->loopback ? _("Activa") :
                         r->cabo ? _("Conectada") : _("Sin cable");
    CP(_("Estado"), "%s", estado);
    CP(_("Adaptador"), "%s", r->driver[0] ? r->driver : "-");
    CP(_("IPv4"), "%s", r->ip ? ip_para_texto(r->ip, a) : "-");
    fmt_bytes(a, sizeof a, vel_tx[if_sel]); fmt_bytes(b, sizeof b, vel_rx[if_sel]);
    CP(_("Envío"), "%s/s", a);
    CP(_("Recepción"), "%s/s", b);
    fmt_bytes(a, sizeof a, r->tx_bytes); fmt_bytes(b, sizeof b, r->rx_bytes);
    CP(_("Total enviado"), "%s", a);
    CP(_("Total recibido"), "%s", b);
    formatar_texto(c, sizeof c, "%u / %u", r->tx_pacotes, r->rx_pacotes);
    CP(_("Paquetes (env. / rec.)"), "%s", c);
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = livre - 16;
    if (th < 36) th = 36;
    if (th > 200) th = 200;
    int av;
    leyenda(RP_X + 8, y, 0x3CDC6E, _("recepción"), &av);
    leyenda(RP_X + 8 + av, y, 0xFFA94D, _("envío"), &av);
    desenhar_grafico_red(RP_X + 8, y + 16, gl, th);
    campos_desenhar();
}

/* D399: relojes, temperatura, VRAM y estado de energía de la tarjeta (solo lectura). */
static void painel_gpu_energia(void)
{
    char a[24], b[24];
    if (!gen_ok || gen.soporte == GPU_EN_SIN) {
        CP(_("Relojes de la GPU"), "%s", _("sin driver de energía para esta tarjeta"));
        return;
    }
    static const char *const modos[] = { "?", N_("BIOS (sin tocar)"), N_("bajo"), N_("alto"), N_("automático"), N_("manual") };
    if (gen.sclk_khz) fmt_khz(a, sizeof a, gen.sclk_khz); else copia_texto(a, "?", sizeof a);
    if (gen.sclk_def_khz) fmt_khz(b, sizeof b, gen.sclk_def_khz); else copia_texto(b, "?", sizeof b);
    CP(_("Reloj del motor"), _("%s (BIOS: %s)%s"), a, b, gen.relojes_fuente == 2 ? _(", sin leer") : "");
    if (gen.mclk_khz) fmt_khz(a, sizeof a, gen.mclk_khz); else copia_texto(a, "?", sizeof a);
    if (gen.mclk_def_khz) fmt_khz(b, sizeof b, gen.mclk_def_khz); else copia_texto(b, "?", sizeof b);
    CP(_("Reloj de la memoria"), _("%s (BIOS: %s)"), a, b);
    if (gen.temp_mili_c != GPU_EN_SIN_SENSOR) CP(_("Temperatura de la GPU"), "%s", sensores_grau(a, sizeof a, gen.temp_mili_c));
    else if (gen.n_zonas) CP(_("Temperatura de la GPU"), _("sin sensor; ACPI %s: %s"), gen.zona_nome[0], sensores_grau(a, sizeof a, gen.zona_mili_c[0]));
    else CP(_("Temperatura de la GPU"), "%s", gen.temp_fuente);
    if (gen.vram_kb) CP(_("VRAM usada"), _("unos %u de %u MB"), (gen.vram_usada_kb + 1023) / 1024, gen.vram_kb / 1024);
    if (gen.estado >= 0) formatar_texto(a, sizeof a, _("estado %d"), gen.estado);
    else copia_texto(a, _("relojes de la BIOS"), sizeof a);
    CP(_("Energía"), "%s, %s%s", _(modos[gen.modo <= 5 ? gen.modo : 0]), a, gen.bloqueado ? _(" (bloqueado tras un fallo)") : "");
}

static void painel_gpu(void)
{
    char t[96];
    ncps = 0;
    int y = painel_cab(_("GPU"), _("actividad, últimos 60 s"), acento_r[R_GPU]);
    if (a2d_ok) {
        CP(_("2D del escritorio"), "%s", a2d.ativo ? a2d.nome : _("CPU (software)"));
        if (a2d.placa[0]) CP(_("Tarjeta"), "%s", a2d.placa);
        if (a2d.vram_kb) CP(_("Memoria de vídeo"), "%u MB", a2d.vram_kb / 1024);   /* D387: la de la tarjeta */
        CP(_("Órdenes 2D (copias + rellenos)"), "%u + %u", a2d.copias, a2d.rellenos);
        CP(_("Fallos 2D"), "%u", a2d.falhas);
        CP(_("Cuadros de arrastre"), "%u", a2d.quadros);
    } else CP(_("2D del escritorio"), _("sin información"));
    if (g3d_ok) {
        static const char *const e3[] = { N_("sin probar"), N_("activo"), N_("software"), N_("fallo") };
        CP(_("3D"), "%s", g3d.nome[0] ? g3d.nome : "software");
        if (g3d.placa[0]) CP(_("Tarjeta 3D"), "%s", g3d.placa);
        CP(_("Estado 3D"), "%s", _(e3[g3d.estado < 4 ? g3d.estado : 3]));
        CP(_("Triángulos / lotes"), "%u / %u", g3d.triangulos, g3d.lotes);
        if (g3d.memoria_kb) CP(_("Memoria de GPU (3D)"), _("%u de %u MB usados"), (g3d.memoria_kb - g3d.livre_kb) / 1024, g3d.memoria_kb / 1024);
    } else CP(_("3D"), _("sin información"));
    painel_gpu_energia();
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = livre - 16 - 20;
    if (th < 36) th = 36;
    if (th > 160) th = 160;
    int av;
    leyenda(RP_X + 8, y, 0xFF7AB6, _("2D (órdenes/s)"), &av);
    leyenda(RP_X + 8 + av, y, 0x7EB6FF, _("3D (cuadros/s)"), &av);
    grafico_taxa(RP_X + 8, y + 16, gl, th, hist_g2d, hist_g3d, qtd_hist_sens, 0xFF7AB6, 0x7EB6FF, "/s");
    y += 16 + th + 4;
    if (a2d_ok && a2d.motivo[0]) {
        formatar_texto(t, sizeof t, "%s", a2d.motivo);
        g_texto_n(j, RP_X + 8, y, t, (RP_L - 16) / LETRA_L, COR_TEXTO_FRACO);
    }
    campos_desenhar();
}

static void painel_sens(void)
{
    char a[32], b[32];
    ncps = 0;
    int y = painel_cab(_("Sensores"), _("últimos 60 s"), acento_r[R_SENS]);
    int tm = sens_ok ? sensores_temp_maior(&sens) : -1;
    unsigned f = freq_mostrada(NULL);
    bool limitada = freq_ok && freq.limitada;
    if (sens_ok && sens.qtd_temp) {
        CP(_("Temperatura"), "%s", sensores_grau(a, sizeof a, tm));
        if (sens.max_mili_c) CP(_("Máxima admitida"), "%s", sensores_grau(a, sizeof a, sens.max_mili_c));
    } else CP(_("Temperatura"), "%s", sens_ok && sens.hipervisor ? _("sin sensor (máquina virtual)") : _("sin sensor conocido"));
    if (f) { fmt_khz(a, sizeof a, f); CP(_("Frecuencia"), "%s", a); }
    if (limitada) CP(_("Estado"), _("limitada por temperatura"));
    if (bat_ok) {
        static const char *const eb[] = { N_("en reposo"), N_("descargando"), N_("cargando"), N_("crítica") };
        unsigned e = bat.estado & BATERIA_DESCARREGANDO ? 1 : bat.estado & BATERIA_CARREGANDO ? 2 : 0;
        if (bat.estado & BATERIA_CRITICA) e = 3;
        CP(_("Batería"), "%d %% (%s)", bat.porcentagem, _(eb[e]));
        if (bat.minutos >= 0) CP(_("Tiempo restante"), "%d:%02d", bat.minutos / 60, bat.minutos % 60);
        CP(_("Adaptador"), "%s", bat.adaptador == 2 ? _("enchufado") : bat.adaptador == 1 ? _("desenchufado") : "-");
    } else CP(_("Batería"), _("no hay"));
    int livre = RP_Y + RP_A - y - campos_altura() - 4;
    int gl = ajusta(RP_L - 16), th = (livre - 2 * 16 - 8) / 2;
    if (th < 30) th = 30;
    if (th > 130) th = 130;
    int tmax = sens.max_mili_c > 0 ? sens.max_mili_c : 100000;
    formatar_texto(b, sizeof b, tm > 0 ? "0-%d" : "", tmax / 1000);
    bool lim = sens_ok && (sens.throttling & (SENS_THR_TERMICO | SENS_THR_PROCHOT));
    rot_grafico(y, _("Temperatura"), b);
    if (sens_ok && sens.qtd_temp)
        desenhar_grafico(RP_X + 8, y + 16, gl, th, hist_temp, qtd_hist_sens, lim ? 0xFF5050 : 0xFFD040, lim ? 0x5C1616 : 0x5C4A10);
    else sem_dados(RP_X + 8, y + 16, gl, th, sens_ok && sens.hipervisor ? "sin sensor (máquina virtual)" : "sin sensor conocido");
    y += 16 + th + 8;
    rot_grafico(y, _("Frecuencia de la CPU"), NULL);
    if (f) desenhar_grafico(RP_X + 8, y + 16, gl, th, hist_freq, qtd_hist_sens, limitada ? 0xFF5050 : 0xFFA94D, limitada ? 0x5C1616 : 0x5C3A10);
    else sem_dados(RP_X + 8, y + 16, gl, th, "sin APERF/MPERF");
    campos_desenhar();
}

static void desenhar_rendimento(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    for (int r = 0; r < QTD_R; r++) desenhar_item_lateral(r);
    switch (rsel) {
    case R_CPU:   painel_cpu(); break;
    case R_MEM:   painel_mem(); break;
    case R_SWAP:  painel_swap(); break;
    case R_DISCO: painel_disco(); break;
    case R_RED:   painel_red(); break;
    case R_GPU:   painel_gpu(); break;
    default:      painel_sens(); break;
    }
}

/* Clic en la pestaña Rendimiento: elegir recurso o interfaz. true = hay que repintar. */
static bool tratar_rendimento(const evento_t *e)
{
    if (e->tipo != EV_CLIQUE) return false;
    int ia = TAB_A / QTD_R;
    if (ia > 56) ia = 56;
    if (e->x >= TAB_X && e->x < TAB_X + SB_L && e->y >= TAB_Y) {
        int r = (e->y - TAB_Y) / ia;
        if (r >= 0 && r < QTD_R && r != rsel) {
            rsel = r;
            registrar("tareas: recurso %s", nomes_r_log[r]);
            log_sens_forcar = log_gpu_forcar = log_energia_forcar = true;   /* D408: las líneas una vez al cambiar */
            return true;
        }
        return false;
    }
    if (rsel == R_RED)
        for (int i = 0; i < qtd_ifs && i < MAXIF; i++)
            if (botao_tratar(j, &b_if[i], e)) { if_sel = i; registrar("tareas: interfaz %s", ifs[i].nome); return true; }
    return false;
}

/* ------------------------------------------------------------ Detalles (D368) */

#define MAXH 128
static info_hilo_t hilos[MAXH];
static int qtd_hilos;
static info_planificacion_t plan_sel;
static bool plan_ok;

static void amostrar_detalhes(void)
{
    int n = info_sistema(INFO_HILOS, hilos, MAXH);
    qtd_hilos = n < 0 ? 0 : n;
    plan_ok = sel_pid >= 0 && planificacion_info(sel_pid, &plan_sel) == 0;
    int k = info_sistema(INFO_PILHAS, pilhas, MAXP);
    qtd_pilhas = k < 0 ? 0 : k;
}

static void desenhar_detalhes(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BRANCO);
    g_moldura(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BORDA);
    const proc_t *p = proc_por_pid(sel_pid);
    if (!p) {
        g_texto(j, TAB_X + 12, TAB_Y + 14, _("Elige un proceso en la pestaña Procesos para ver sus detalles e hilos."), COR_TEXTO_FRACO);
        return;
    }
    char t[96], a[24];
    g_retangulo(j, TAB_X + 1, TAB_Y + 1, TAB_L - 2, 24, COR_DESTAQUE);
    formatar_texto(t, sizeof t, "%s  (PID %d)", p->p.nome, p->p.pid);
    g_texto_n(j, TAB_X + 10, TAB_Y + 5, t, (TAB_L - 20) / LETRA_L, COR_BRANCO);
    ncps = 0;
    CP(_("Estado"), "%s", nome_estado(p->p.estado));
    CP(_("Padre"), p->p.pid ? "%d" : "-", p->p.pai);
    CP(_("CPU"), "%u,%u %%", p->cpu10 / 10, p->cpu10 % 10);
    fmt_tempo(a, sizeof a, p->p.cpu_ms);
    CP(_("Tiempo de CPU"), "%s", a);
    CP(_("Memoria"), "%u KB", p->p.memoria_kb);
    CP(_("Pila"), "%u KB", pilha_de(p->p.pid));
    CP(_("Hilos"), "%u", p->p.n_hilos);
    CP(_("Ventanas"), "%u", p->p.janelas);
    CP(_("Última CPU"), "%u", p->p.cpu);
    if (plan_ok) {
        CP(_("Prioridad (nice)"), "%d", plan_sel.nice);
        CP(_("Prioridad efectiva"), "%d", plan_sel.prioridade);
        if (plan_sel.mascara) CP(_("Afinidad"), "0x%x", plan_sel.mascara); else CP(_("Afinidad"), _("todas las CPU"));
        CP(_("Migraciones"), "%u", plan_sel.migracoes);
        CP(_("Latencia media"), "%u us", plan_sel.lat_media_us);
        CP(_("Latencia máxima"), "%u us", plan_sel.lat_max_us);
    }
    int c = (TAB_L - 16) / 150;
    if (c < 1) c = 1;
    if (c > 4) c = 4;
    int cw = (TAB_L - 16) / c, y0 = TAB_Y + 32;
    for (int k = 0; k < ncps; k++) {
        int cx = TAB_X + 8 + (k % c) * cw, cy = y0 + (k / c) * 34;
        g_texto_n(j, cx, cy, cps[k].r, (cw - 8) / LETRA_L, COR_TEXTO_FRACO);
        g_texto_n(j, cx, cy + 15, cps[k].v, (cw - 8) / LETRA_L, COR_TEXTO);
    }
    int ty = y0 + ((ncps + c - 1) / c) * 34 + 8;
    static const int hl[5] = { 72, 120, 100, 72, 72 };
    static const char *const ht[5] = { N_("TID"), N_("Estado"), N_("T. CPU"), N_("CPU"), N_("Prior.") };
    int x = TAB_X + 8;
    for (int k = 0; k < 5; k++) {
        g_retangulo(j, x, ty, hl[k], CAB_A, COR_BOTAO);
        g_relevo(j, x, ty, hl[k], CAB_A, false);
        texto_coluna(x, ty + 2, hl[k], _(ht[k]), k != 1, COR_TEXTO);
        x += hl[k];
    }
    int fila = 0, max = (TAB_Y + TAB_A - ty - CAB_A - 4) / LIN_A, total = 0;
    for (int i = 0; i < qtd_hilos; i++) {
        if (hilos[i].pid != sel_pid) continue;
        total++;
        if (fila >= max) continue;
        int y = ty + CAB_A + 2 + fila * LIN_A;
        if (fila & 1) g_retangulo(j, TAB_X + 8, y, 436, LIN_A, 0xF8F9FA);
        char c1[16], c2[24], c3[16], c4[16], c5[16];
        formatar_texto(c1, sizeof c1, "%d", hilos[i].tid);
        copia_texto(c2, nome_estado(hilos[i].estado), sizeof c2);
        fmt_tempo(c3, sizeof c3, hilos[i].cpu_ms);
        formatar_texto(c4, sizeof c4, "%u", hilos[i].cpu);
        info_planificacion_t pi;
        if (planificacion_info(hilos[i].tid, &pi) == 0) formatar_texto(c5, sizeof c5, "%d", pi.nice); else copia_texto(c5, "-", sizeof c5);
        const char *cs[5] = { c1, c2, c3, c4, c5 };
        x = TAB_X + 8;
        for (int k = 0; k < 5; k++) { texto_coluna(x, y + 1, hl[k], cs[k], k != 1, COR_TEXTO); x += hl[k]; }
        fila++;
    }
    if (!total) g_texto(j, TAB_X + 12, ty + CAB_A + 6, _("Este proceso solo tiene su hilo principal."), COR_TEXTO_FRACO);
    else if (total > fila) {
        formatar_texto(t, sizeof t, _("... y %d hilos más"), total - fila);
        g_texto(j, TAB_X + 12, TAB_Y + TAB_A - 20, t, COR_TEXTO_FRACO);
    }
}

static void desenhar_rodape(void)
{
    g_retangulo(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6, COR_FUNDO);
    char t[96];
    unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
    if (aba == 0 && filtro[0])
        formatar_texto(t, sizeof t, _("Buscar: %s_  (%d de %d; Esc borra)"), filtro, nvis, qtd_procs);
    else
        formatar_texto(t, sizeof t, _("Procesos: %d  CPU: %u%%  Mem.: %u%%%s"), qtd_procs,
                       (cpu_total10 + 5) / 10, (m10 + 5) / 10, pausado ? "  (pausa)" : "");
    int x = b_pausa.x + b_pausa.l + 12;
    int x_fim = aba == ABA_SERV ? b_serv_auto.x : b_acao.x;
    g_texto_n(j, x, BOT_Y + 6, t, (x_fim - 8 - x) / LETRA_L, COR_TEXTO_FRACO);
    b_pausa.texto = pausado ? _("Reanudar") : _("Pausar");
    botao_desenhar(j, &b_pausa);
    if (aba == ABA_SERV) {
        const servico_info_t *s = serv_selecionado();
        bool marcha = s && (compara_texto(s->estado, SERV_E_MARCHA) == 0 || compara_texto(s->estado, SERV_E_ESPERANDO) == 0 ||
                            compara_texto(s->estado, SERV_E_REINICIANDO) == 0);
        b_acao.texto = marcha ? _("Parar") : _("Iniciar");
        b_serv_auto.texto = s && !s->arrancar ? _("Activar") : _("Desactivar");
        botao_desenhar(j, &b_serv_auto);
        botao_desenhar(j, &b_serv_rein);
        botao_desenhar(j, &b_acao);
    } else if (aba != 1) {
        b_acao.texto = aba == 0 || aba == ABA_DET ? _("Finalizar proceso") : aba == 2 ? _("Cerrar ventana") : _("Cerrar conexión");
        botao_desenhar(j, &b_acao);
    } else if (freq_controlavel()) {
        /* Rendimiento: la política del driver de frecuencia (D106); cada clic pasa a la siguiente. */
        static char rot[40];
        formatar_texto(rot, sizeof rot, _("Política: %s"), frequencia_politica(freq.politica));
        b_acao.texto = rot;
        botao_desenhar(j, &b_acao);
    }
}

static void mudar_politica(void)
{
    unsigned p = (freq.politica + 1) % 3;
    int r = frequencia_controlar(FREQ_OP_POLITICA, (int)p);
    registrar("tareas: politica de frecuencia %s -> %d", frequencia_politica(p), r);
    freq_ok = frequencia_ler(&freq);
}

static unsigned sig_ant;   /* firma de lo dibujado en la tabla de procesos: sin cambios, no se repinta */

static void desenhar_tudo(void)
{
    sig_ant = 0;
    g_retangulo(j, 0, PAINEL_Y + 1, LARG, ALT - PAINEL_Y - 1, COR_FUNDO);
    desenhar_abas();
    if (aba == ABA_DET) desenhar_detalhes();
    else if (aba == 0) desenhar_tabela();
    else if (aba == 1) desenhar_rendimento();
    else if (aba == 2) desenhar_janelas();
    else if (aba == ABA_SERV) desenhar_servicos();
    else desenhar_red();
    desenhar_rodape();
    jan_mostrar(j);
}

/* Posiciones que dependen del tamaño de la ventana (D044). */
static void layout(void)
{
    b_pausa.y = b_acao.y = BOT_Y;
    b_acao.x = LARG - 12 - b_acao.l;
    b_serv_rein.y = b_serv_auto.y = BOT_Y;
    b_serv_rein.x = b_acao.x - 8 - b_serv_rein.l;
    b_serv_auto.x = b_serv_rein.x - 8 - b_serv_auto.l;
    lista_serv.y = TAB_Y + 24;
    lista_serv.l = TAB_L;
    lista_serv.a = TAB_A - 24 - 22;   /* debajo, una línea para el detalle */
    lista_jan.y = TAB_Y + 24;
    lista_jan.l = TAB_L;
    lista_jan.a = TAB_A - 24;
    layout_red();
}

/* D408 (de paso): las líneas de sensores y de la GPU se escribían en cada muestra (una por segundo) y llenaban
 * el dmesg en hardware real. Ahora solo al abrir la pestaña Rendimiento y cuando cambia algo que importa: la
 * fuente o la temperatura en 3 grados o más, la frecuencia en más de un 25 %, el driver 2D/3D, si la GPU pasa
 * de parada a trabajando (o al revés) y la batería. */

static bool log_sensores_mudou(int temp, unsigned freq, unsigned fonte)
{
    static int t0;
    static unsigned f0, fo0;
    int dt = temp > t0 ? temp - t0 : t0 - temp;
    unsigned df = freq > f0 ? freq - f0 : f0 - freq;
    if (!log_sens_forcar && fonte == fo0 && dt < 3000 && df * 4u <= f0) return false;
    log_sens_forcar = false;
    t0 = temp; f0 = freq; fo0 = fonte;
    return true;
}

/* D399 + D408: la línea de energía de la GPU, al abrir la pestaña o el recurso y cuando cambia el modo, el
 * estado, un reloj, la temperatura en 3 grados o más, o la VRAM usada en 1 MB o más. */
static bool log_energia_mudou(const info_gpu_energia_t *g)
{
    static info_gpu_energia_t a;
    int dt = g->temp_mili_c > a.temp_mili_c ? g->temp_mili_c - a.temp_mili_c : a.temp_mili_c - g->temp_mili_c;
    unsigned dv = g->vram_usada_kb > a.vram_usada_kb ? g->vram_usada_kb - a.vram_usada_kb : a.vram_usada_kb - g->vram_usada_kb;
    if (!log_gpu_forcar && !log_energia_forcar && g->soporte == a.soporte && g->modo == a.modo && g->estado == a.estado &&
        g->sclk_khz == a.sclk_khz && g->mclk_khz == a.mclk_khz && dt < 3000 && dv < 1024)
        return false;
    log_energia_forcar = false;
    a = *g;
    return true;
}

static bool log_gpu_mudou(const char *n2d, const char *n3d, bool ativa2d, bool ativa3d, int bateria)
{
    static char a2[24], a3[24];
    static bool v2, v3;
    static int b0;
    if (!log_gpu_forcar && !compara_texto(n2d, a2) && !compara_texto(n3d, a3) && ativa2d == v2 && ativa3d == v3 && bateria == b0)
        return false;
    log_gpu_forcar = false;
    copia_texto(a2, n2d, sizeof a2);
    copia_texto(a3, n3d, sizeof a3);
    v2 = ativa2d; v3 = ativa3d; b0 = bateria;
    return true;
}

/* Línea del log en cada muestra: las pruebas siguen el programa por aquí. */
static void registrar_amostra(void)
{
    static char lista_antes[240];
    char lista[240];
    lista[0] = 0;
    for (int i = 0; i < qtd_procs; i++) {
        char item[48];
        formatar_texto(item, sizeof item, "%s%d %s%s", i ? ", " : "", procs[i].p.pid, procs[i].p.nome,
                       procs[i].p.estado == 4 ? " (zombi)" : "");
        junta_texto(lista, item, sizeof lista);
    }
    if (compara_texto(lista, lista_antes) != 0) {
        registrar("tareas: lista %d procesos: %s", qtd_procs, lista);
        copia_texto(lista_antes, lista, sizeof lista_antes);
    }
    const proc_t *maior = NULL;
    for (int i = 0; i < qtd_procs; i++)
        if (procs[i].p.pid != 0 && (!maior || procs[i].cpu10 > maior->cpu10)) maior = &procs[i];
    unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
    if (aba == 3) registrar_red();
    else if (aba == ABA_SERV) registrar_servicos();
    else if (aba == 1) {
        registrar("tareas: rendimiento cpu %u%% mem %u/%u KB (%u%%) procesos %d muestras %d", (cpu_total10 + 5) / 10,
                  mem.usada_kb, mem.gerida_kb, (m10 + 5) / 10, qtd_procs, qtd_hist);
        char nl[96];
        nl[0] = 0;
        for (int i = 0; i < qtd_nuc; i++) {
            char it[24];
            formatar_texto(it, sizeof it, "%s%u=%u%%", i ? " " : "", nuc[i].indice, (nuc_uso10[i] + 5) / 10);
            junta_texto(nl, it, sizeof nl);
        }
        registrar("tareas: nucleos %d: %s", qtd_nuc, nl);
        for (int i = 0; i < qtd_freq_nuc; i++)   /* D175 */
            if (freq_nuc[i].ligada)
                registrar("tareas: frecuencia cpu %u atual %d %u kHz", freq_nuc[i].cpu, freq_nuc[i].atual, freq_nuc[i].khz);
        int s_temp = sens_ok && sens.qtd_temp ? sensores_temp_maior(&sens) : -1;
        unsigned s_freq = sens_ok ? sensores_freq_atual(&sens, NULL) : 0;
        if (log_sensores_mudou(s_temp, s_freq, sens.fonte))
            registrar("tareas: sensores temp %d mC freq %u kHz fonte %u muestras %d", s_temp, s_freq, sens.fonte, qtd_hist_sens);
        if (freq_ok)
            registrar("tareas: frecuencia estado %u atual %d %u kHz limitada %u temp %d", freq.estado, freq.atual,
                      freq_mostrada(NULL), freq.limitada, freq.temp_mc);
        /* D368: el recurso elegido y los datos de swap y disco (las pruebas los leen). */
        registrar("tareas: recurso %s", nomes_r_log[rsel]);
        registrar("tareas: swap tope %u KB almacen %u KB datos %u KB comprimida %u KB salidas %u entradas %u sale/s %u entra/s %u",
                  mem.troca_teto_kb, mem.troca_armazem_kb, mem.troca_kb, mem.troca_comprimida_kb, mem.troca_saidas,
                  mem.troca_entradas, swap_s_s, swap_e_s);
        registrar("tareas: disco ativa %u bloques %u/%u sucios %u aciertos %u fallos %u grabados %u lee %u KB/s escribe %u KB/s",
                  disco_i.ativa, disco_i.blocos, disco_i.max_blocos, disco_i.sujos, disco_i.acertos, disco_i.falhas,
                  disco_i.gravados, disco_rd_s, disco_wr_s);
        if (gen_ok && log_energia_mudou(&gen))   /* D399 (al abrir y si cambia algo, como D408) */
            registrar("tareas: gpu energia soporte %u modo %u estado %d motor %u memoria %u kHz temp %d zonas %u vram %u/%u KB",
                      gen.soporte, gen.modo, gen.estado, gen.sclk_khz, gen.mclk_khz, gen.temp_mili_c, gen.n_zonas,
                      gen.vram_usada_kb, gen.vram_kb);
        const char *n2d = a2d_ok && a2d.ativo ? a2d.nome : "cpu", *n3d = g3d_ok && g3d.nome[0] ? g3d.nome : "software";
        int pct = bat_ok ? bat.porcentagem : -1;
        if (log_gpu_mudou(n2d, n3d, gpu2d_s > 0, gpu3d_s > 0, pct))
            registrar("tareas: gpu 2d %s %u ops/s 3d %s %u cuadros/s bateria %d%%", n2d, gpu2d_s, n3d, gpu3d_s, pct);
    }
    else if (aba == ABA_DET) {
        int nh = 0;
        for (int i = 0; i < qtd_hilos; i++) if (hilos[i].pid == sel_pid) nh++;
        registrar("tareas: detalles pid %d hilos %d extra %d nice %d", sel_pid, proc_por_pid(sel_pid) ? (int)proc_por_pid(sel_pid)->p.n_hilos : -1,
                  nh, plan_ok ? plan_sel.nice : -99);
    }
    else if (maior)
        registrar("tareas: cpu %u%% mayor pid %d %s %u%%", (cpu_total10 + 5) / 10, maior->p.pid, maior->p.nome,
                  (maior->cpu10 + 5) / 10);
}

/* Firma de lo que dibuja la pestaña Procesos: si no cambia, no se repinta (el administrador casi no gasta CPU). */
static unsigned assinatura_tabela(void)
{
    unsigned h = 2166136261u;
#define MEZ(v) (h = (h ^ (unsigned)(v)) * 16777619u)
    MEZ(qtd_procs); MEZ(ordem_col); MEZ(ordem_desc); MEZ(arbol); MEZ(col_vis); MEZ(sel_pid); MEZ(topo); MEZ(LARG); MEZ(ALT);
    MEZ(pausado); MEZ(cpu_total10); MEZ(qtd_hist ? hist_mem[qtd_hist - 1] : 0); MEZ(mem.gerida_kb);
    for (const char *c = filtro; *c; c++) MEZ(*c);
    for (int i = 0; i < qtd_procs; i++) {
        MEZ(procs[i].p.pid); MEZ(procs[i].p.estado); MEZ(procs[i].cpu10); MEZ(procs[i].p.memoria_kb);
        MEZ(procs[i].p.n_hilos); MEZ(procs[i].p.cpu); MEZ(procs[i].p.janelas); MEZ(procs[i].p.cpu_ms / 1000);
        MEZ(procs[i].nice);
    }
#undef MEZ
    return h | 1;
}

static void atualizar_nice(void)
{
    querer_nice = (aba == 0 && (col_vis >> C_PRIO & 1)) || aba == ABA_DET;
}

static void atualizar(void)
{
    atualizar_nice();
    amostrar();
    if (aba == 3 || aba == 1) amostrar_red();   /* la red solo se consulta con la pestaña a la vista */
    if (aba == 1) amostrar_sensores();   /* los sensores también (D092) */
    if (aba == ABA_SERV) amostrar_servicos();   /* y los servicios (D216) */
    if (aba == ABA_DET) amostrar_detalhes();
    registrar_amostra();
    if (aba == 0) {
        unsigned sig = assinatura_tabela();
        if (sig == sig_ant) return;   /* nada ha cambiado desde el último dibujo */
        sig_ant = sig;
        desenhar_tabela(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
    }
    else if (aba == 1) { desenhar_rendimento(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == 2) { desenhar_janelas(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == ABA_SERV) { desenhar_servicos(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == ABA_DET) { desenhar_detalhes(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else { desenhar_red(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    desenhar_rodape();
    jan_mostrar_area(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6);
}

static void finalizar_selecionado(void)
{
    const proc_t *p = proc_por_pid(sel_pid);
    if (!p) { mensagem(_("Finalizar proceso"), _("Selecciona primero un proceso de la lista.")); return; }
    char t[256];
    if (p->p.pid == 0 || !p->p.usuario) {
        registrar("tareas: no se puede finalizar pid %d (%s)", p->p.pid, p->p.nome);
        formatar_texto(t, sizeof t, "\"%s\" (PID %d) es parte del kernel.\nNo se puede finalizar.", p->p.nome, p->p.pid);
        mensagem(_("Finalizar proceso"), t);
        return;
    }
    const char *aviso = "";
    if (compara_texto(p->p.nome, "sagsh") == 0)
        aviso = "\n\nAtención: es un shell. Se perderá lo que\nesté haciendo y su Terminal dejará de responder.";
    else if (compara_texto(p->p.nome, "terminal") == 0)
        aviso = "\n\nAtención: es el Terminal. Se cerrará su\nventana y el shell que contiene.";
    else if (p->p.pid == meu_pid())
        aviso = "\n\nEs este mismo Administrador de tareas.";
    formatar_texto(t, sizeof t, _("¿Finalizar el proceso \"%s\" (PID %d)?%s"), p->p.nome, p->p.pid, aviso);
    int pid = p->p.pid;
    char nome[32];
    copia_texto(nome, p->p.nome, sizeof nome);
    registrar("tareas: confirmar finalizar pid %d (%s)", pid, nome);
    if (!confirmar(_("Finalizar proceso"), t)) { registrar("tareas: cancelado"); return; }
    int r = COM_ADMIN("finalizar un proceso de otra cuenta", matar(pid));   /* D269 */
    registrar("tareas: finalizar pid %d (%s) -> %d", pid, nome, r);
    if (r < 0) {
        formatar_texto(t, sizeof t, _("No se pudo finalizar el proceso %d:\n%s"), pid, nome_erro(r));
        mensagem(_("Finalizar proceso"), t);
    }
    sel_pid = -1;
}

static void cerrar_ventana(void)
{
    if (sel_jan < 0) { mensagem(_("Cerrar ventana"), _("Selecciona primero una ventana de la lista.")); return; }
    int r = pedir_fechar_janela(sel_jan);
    registrar("tareas: cerrar ventana %d -> %d", sel_jan, r);
    sel_jan = -1;
}

static bool dentro(const evento_t *e, int x, int y, int l, int a)
{
    return e->x >= x && e->y >= y && e->x < x + l && e->y < y + a;
}

/* Selección por teclado: se mueve en el orden mostrado. */
static void mover_selecao(int delta)
{
    ordenar();
    if (!nvis) return;
    int pos = -1;
    for (int i = 0; i < nvis; i++) if (procs[ordem[i]].p.pid == sel_pid) pos = i;
    pos = pos < 0 ? 0 : pos + delta;
    if (pos < 0) pos = 0;
    if (pos >= nvis) pos = nvis - 1;
    sel_pid = procs[ordem[pos]].p.pid;
    int vis = linhas_visiveis();
    if (pos < topo) topo = pos;
    if (pos >= topo + vis) topo = pos - vis + 1;
    desenhar_tabela();
    jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
}

/* Menús del botón derecho (D368): en la cabecera, las columnas y el árbol; en una fila, las acciones. */
#define MENU_COLS  8
#define MENU_PROC  9
enum { MP_FINALIZAR, MP_SEP1, MP_MAIS, MP_MENOS, MP_SEP2, MP_DETALHES };

static void menu_colunas(const evento_t *e)
{
    char itens[400];
    itens[0] = 0;
    for (int c = C_PAI; c < QTD_COLUNAS; c++) {
        char l[64];
        formatar_texto(l, sizeof l, "%s%s\n", col_vis >> c & 1 ? "*" : "", _(colunas[c].titulo));
        junta_texto(itens, l, sizeof itens);
    }
    junta_texto(itens, "-\n", sizeof itens);
    junta_texto(itens, arbol ? "*" : "", sizeof itens);
    junta_texto(itens, _("Árbol de procesos"), sizeof itens);
    registrar("tareas: menu columnas");
    menu_contextual(j->id, e->x, e->y, itens, MENU_COLS);
}

static void menu_proceso(const evento_t *e)
{
    const proc_t *p = proc_por_pid(sel_pid);
    bool normal = p && p->p.pid != 0 && p->p.usuario;
    char itens[256];
    formatar_texto(itens, sizeof itens, "%s%s\n-\n%s%s\n%s%s\n-\n%s", normal ? "" : "~", _("Finalizar proceso"),
                   normal ? "" : "~", _("Prioridad más alta"), normal ? "" : "~", _("Prioridad más baja"), p ? "" : "~");
    junta_texto(itens, _("Detalles e hilos"), sizeof itens);
    registrar("tareas: menu proceso pid %d", sel_pid);
    menu_contextual(j->id, e->x, e->y, itens, MENU_PROC);
}

/* Cambia el nice del proceso elegido en `delta` (negativo = más prioridad: pide administrador, D269). */
static void mudar_prioridade(int delta)
{
    const proc_t *p = proc_por_pid(sel_pid);
    info_planificacion_t pi;
    if (!p || planificacion_info(p->p.pid, &pi) != 0) return;
    int nn = pi.nice + delta;
    if (nn < -20) nn = -20;
    if (nn > 19) nn = 19;
    int pid = p->p.pid;
    int r = COM_ADMIN("cambiar la prioridad de un proceso", prioridade_processo(pid, nn));
    registrar("tareas: prioridad pid %d %d -> %d: %d", pid, pi.nice, nn, r);
    if (r < 0) {
        char t[160];
        formatar_texto(t, sizeof t, _("No se pudo cambiar la prioridad del proceso %d:\n%s"), pid, nome_erro(r));
        mensagem(_("Prioridad"), t);
    }
}

static void ir_detalhes(void);

static void tratar_tabela(const evento_t *e)
{
    if (e->tipo == EV_MENU && (int)e->tecla == MENU_COLS) {
        int n = QTD_COLUNAS - C_PAI;
        if (e->x < n) {
            col_vis ^= 1u << (C_PAI + e->x);
            registrar("tareas: columnas 0x%x", col_vis);
        } else if (e->x == n + 1) {
            arbol = !arbol;
            registrar("tareas: arbol %s", arbol ? "si" : "no");
        }
        atualizar_nice();
        if (querer_nice) amostrar();
        desenhar_tabela();
        jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
        return;
    }
    if (e->tipo == EV_MENU && (int)e->tecla == MENU_PROC) {
        if (e->x == MP_FINALIZAR) { finalizar_selecionado(); atualizar(); }
        else if (e->x == MP_MAIS) { mudar_prioridade(-5); atualizar(); }
        else if (e->x == MP_MENOS) { mudar_prioridade(5); atualizar(); }
        else if (e->x == MP_DETALHES) ir_detalhes();
        return;
    }
    if (e->tipo == EV_TECLA) {
        unsigned k = e->tecla;
        if (k == TECLA_BAIXO) mover_selecao(1);
        else if (k == TECLA_CIMA) mover_selecao(-1);
        else if (k == TECLA_PGDN) mover_selecao(linhas_visiveis());
        else if (k == TECLA_PGUP) mover_selecao(-linhas_visiveis());
        else if (k == TECLA_DELETE) { finalizar_selecionado(); atualizar(); }
        else if (k == TECLA_ESC || k == 8 || k == 127 || (k >= 32 && k < 256 && !(e->modificadores & 6))) {
            size_t n = tam_texto(filtro);
            if (k == TECLA_ESC) filtro[0] = 0;
            else if (k == 8 || k == 127) { if (n) filtro[n - 1] = 0; }
            else if (n + 1 < sizeof filtro) { filtro[n] = (char)k; filtro[n + 1] = 0; }
            else return;
            topo = 0;
            registrar("tareas: buscar \"%s\"", filtro);
            desenhar_tabela();
            jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
            desenhar_rodape();
            jan_mostrar_area(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6);
        }
        return;
    }
    if (e->tipo != EV_CLIQUE || !dentro(e, TAB_X, TAB_Y, TAB_L, TAB_A)) return;
    bool direito = (e->botoes & 2) != 0;
    if (e->y < TAB_Y + 1 + CAB_A) {   /* cabecera: ordena (botón derecho: columnas) */
        if (direito) { menu_colunas(e); return; }
        int x = TAB_X + 1;
        for (int c = 0; c < QTD_COLUNAS; c++) {
            if (!(col_vis >> c & 1)) continue;
            if (e->x >= x && e->x < x + colunas[c].l) {
                if (c == ordem_col) ordem_desc = !ordem_desc;
                else { ordem_col = c; ordem_desc = c >= C_CPU; }   /* números grandes primero */
                registrar("tareas: ordenar por %s %s", colunas[c].titulo, ordem_desc ? "desc" : "asc");
                break;
            }
            x += colunas[c].l;
        }
    } else if (e->x >= TAB_X + TAB_L - BARRA_L - 1) {   /* barra de rolagem */
        int vis = linhas_visiveis();
        topo += e->y < TAB_Y + TAB_A / 2 ? -vis : vis;
    } else {
        int k = (e->y - (TAB_Y + 1 + CAB_A + 2)) / LIN_A;
        if (k >= 0 && topo + k < nvis) {
            ordenar();
            const proc_t *p = &procs[ordem[topo + k]];
            sel_pid = p->p.pid;
            registrar("tareas: seleccionado pid %d (%s)", p->p.pid, p->p.nome);
            if (e->cliques == 2 && !direito) { ir_detalhes(); return; }
        }
        if (direito && sel_pid >= 0) { desenhar_tabela(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); menu_proceso(e); return; }
    }
    desenhar_tabela();
    jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
}

static void mudar_aba(int i)
{
    aba = i;
    registrar("tareas: pestana %s", nomes_log[i]);
    log_sens_forcar = log_gpu_forcar = log_energia_forcar = true;   /* al abrir una pestaña, las líneas de sensores y GPU una vez */
    atualizar_nice();
    if (aba == 3) amostrar_red();   /* a velocidade aparece na amostra seguinte */
    if (aba == 1) { amostrar_red(); amostrar_sensores(); }
    if (aba == ABA_SERV) { amostrar(); amostrar_servicos(); }
    if (aba == ABA_DET) { amostrar(); amostrar_detalhes(); }
    desenhar_tudo();
    registrar_amostra();
}

static void ir_detalhes(void)
{
    if (sel_pid < 0) return;
    mudar_aba(ABA_DET);
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-t") == 0) return modo_texto();
    j = jan_abrir(_("Administrador de tareas"), LARG, ALT);
    if (!j) { imprimir_em(ERROS, "tareas: no se pudo abrir la ventana (usa \"tareas -t\" en modo texto)\n"); return 1; }
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    layout();
    amostrar();
    dormir(250);   /* una primera diferencia corta, para no empezar con 0 % en todo */
    amostrar();
    desenhar_tudo();
    registrar("tareas: pronto (pid %d)", meu_pid());
    registrar_amostra();
    unsigned ultimo = tempo_ms();
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) break;
            if (jan_redimensionar(j, &e)) {
                larg = j->larg; alt = j->alt;
                layout();
                registrar("tareas: tamano %dx%d", larg, alt);
                desenhar_tudo();
                continue;
            }
            if (e.tipo == EV_CLIQUE && e.y >= ABA_Y - 3 && e.y < PAINEL_Y) {
                for (int i = 0; i < QTD_ABAS; i++)
                    if (e.x >= abas_x[i] && e.x < abas_x[i] + abas_l[i] && aba != i) mudar_aba(i);
                continue;
            }
            if (botao_tratar(j, &b_pausa, &e)) {
                pausado = !pausado;
                registrar("tareas: %s", pausado ? "pausado" : "reanudado");
                desenhar_rodape();
                continue;
            }
            if (aba == 1 && freq_controlavel() && botao_tratar(j, &b_acao, &e)) {
                mudar_politica();
                desenhar_tudo();
                continue;
            }
            if (aba == ABA_SERV) {
                const char *o = NULL;
                if (botao_tratar(j, &b_acao, &e)) o = compara_texto(b_acao.texto, _("Parar")) == 0 ? "parar" : "iniciar";
                else if (botao_tratar(j, &b_serv_rein, &e)) o = "reiniciar";
                else if (botao_tratar(j, &b_serv_auto, &e)) o = compara_texto(b_serv_auto.texto, _("Activar")) == 0 ? "activar" : "desactivar";
                if (o) {
                    ordem_servico(o);
                    dormir(300);   /* deja que el supervisor lo aplique antes de mirar de nuevo */
                    atualizar();
                    ultimo = tempo_ms();
                    continue;
                }
                int r = lista_tratar(j, &lista_serv, &e);
                if (r != LISTA_NADA && lista_serv.sel >= 0 && lista_serv.sel < qtd_servs) {
                    copia_texto(sel_serv, servs[lista_serv.sel].nome, sizeof sel_serv);
                    aviso_serv[0] = 0;
                    registrar("tareas: seleccionado servicio %s", sel_serv);
                    desenhar_servicos();
                    jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
                    desenhar_rodape();
                    jan_mostrar_area(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6);
                }
                continue;
            }
            if (aba != 1 && botao_tratar(j, &b_acao, &e)) {
                if (aba == 0 || aba == ABA_DET) finalizar_selecionado();
                else if (aba == 2) cerrar_ventana();
                else fechar_conexao();
                dormir(100);   /* deja que el proceso (o la ventana) termine antes de mirar de nuevo */
                atualizar();
                ultimo = tempo_ms();
                continue;
            }
            if (aba == 0) tratar_tabela(&e);
            else if (aba == 1) {
                if (tratar_rendimento(&e)) { desenhar_rendimento(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
            } else if (aba == 3) {
                if (e.tipo == EV_MOVER) continue;
                tratar_red(&e);
                if (red_mudou) { red_mudou = false; dormir(100); amostrar_red(); }
                desenhar_red();
                jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
            } else if (aba == 2) {
                int r = lista_tratar(j, &lista_jan, &e);
                if (r != LISTA_NADA && lista_jan.sel >= 0) {
                    sel_jan = ids_jan[lista_jan.sel];
                    registrar("tareas: seleccionada ventana %d", sel_jan);
                }
            }
            continue;
        }
        if (!pausado && tempo_ms() - ultimo >= 1000) {
            ultimo = tempo_ms();
            atualizar();
        }
        dormir(50);
    }
    jan_fechar(j);
    return 0;
}
