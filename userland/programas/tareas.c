// SPDX-License-Identifier: GPL-2.0-or-later
/* tareas - el Administrador de tareas (D043, D079, D216).
 *
 * Cinco pestañas:
 *   Procesos     tabla con PID, nombre, padre, estado, % de CPU, tiempo de CPU,
 *                memoria y ventanas; ordena por la columna pulsada en la cabecera;
 *                "Finalizar proceso" (con confirmación) usa SIS_MATAR.
 *   Rendimiento  gráficos de los últimos 60 s de CPU, memoria, temperatura y
 *                frecuencia de la CPU (monitor de hardware, D092: sólo consulta
 *                los sensores con la pestaña visible, una vez por segundo).
 *   Ventanas     las ventanas abiertas y "Cerrar ventana" (SIS_PEDIR_FECHAR,
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
    info_sistema(INFO_VERSAO, &versao, sizeof versao);
    int j = info_sistema(INFO_JANELAS, jans, MAXJ);
    qtd_jans = j < 0 ? 0 : j;
    if (tem_ant) {
        if (qtd_hist == HIST) {
            memmove(hist_cpu, hist_cpu + 1, (HIST - 1) * sizeof hist_cpu[0]);
            memmove(hist_mem, hist_mem + 1, (HIST - 1) * sizeof hist_mem[0]);
            qtd_hist--;
        }
        hist_cpu[qtd_hist] = cpu_total10;
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

enum { C_PID, C_NOME, C_PAI, C_ESTADO, C_CPU, C_TEMPO, C_MEM, C_JAN, C_NUCLEO, QTD_COLUNAS };

typedef struct { const char *titulo; int l; bool direita; } coluna_t;
static const coluna_t colunas[QTD_COLUNAS] = {
    { "PID", 48, true }, { "Nombre", 100, false }, { "Padre", 56, true }, { "Estado", 96, false },
    { "CPU %", 64, true }, { "T. CPU", 72, true }, { "Memoria", 72, true }, { "Ventana", 64, false },
    { "Núcleo", 56, true },
};

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
    }
    if (d == 0) d = a->p.pid - b->p.pid;   /* empate: por PID, siempre creciente */
    else if (ordem_desc) d = -d;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

static void ordenar(void)
{
    for (int i = 0; i < qtd_procs; i++) ordem[i] = i;
    for (int i = 1; i < qtd_procs; i++) {   /* inserción: como máximo 64 */
        int x = ordem[i], k = i - 1;
        while (k >= 0 && comparar(&procs[ordem[k]], &procs[x]) > 0) { ordem[k + 1] = ordem[k]; k--; }
        ordem[k + 1] = x;
    }
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
    for (int i = 0; i < qtd_procs; i++) {
        const proc_t *p = &procs[ordem[i]];
        char t[16];
        fmt_tempo(t, sizeof t, p->p.cpu_ms);
        char h[16] = "";
        if (p->p.n_hilos > 1) formatar_texto(h, sizeof h, " (%u hilos)", p->p.n_hilos);   /* D167 */
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
static int aba;                     /* 0 Procesos, 1 Rendimiento, 2 Ventanas, 3 Red, 4 Servicios */
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

#define QTD_ABAS 5
#define ABA_SERV 4
static const char *nomes_abas[QTD_ABAS] = { "Procesos", "Rendimiento", "Ventanas", "Red", "Servicios" };
static const char *nomes_log[QTD_ABAS] = { "procesos", "rendimiento", "ventanas", "red", "servicios" };
static const int abas_x[QTD_ABAS] = { 10, 112, 230, 328, 394 }, abas_l[QTD_ABAS] = { 100, 116, 96, 64, 92 };

static botao_t b_pausa  = { 12, 0, 110, 28, "Pausar", false };
static botao_t b_acao   = { 0, 0, 170, 28, "Finalizar proceso", false };
static botao_t b_serv_rein = { 0, 0, 96, 28, "Reiniciar", false };    /* D216: pestaña Servicios */
static botao_t b_serv_auto = { 0, 0, 110, 28, "Desactivar", false };

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
        g_texto(j, x + (abas_l[i] - g_largura_texto(nomes_abas[i])) / 2, y + (a - LETRA_A) / 2 + 1,
                nomes_abas[i], s ? COR_DESTAQUE : COR_TEXTO);
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

static void desenhar_tabela(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BRANCO);
    g_moldura(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_BORDA);
    /* Cabecera: pulsar ordena; la columna actual muestra ^ (creciente) o v. */
    int x = TAB_X + 1;
    for (int c = 0; c < QTD_COLUNAS; c++) {
        bool atual = c == ordem_col;
        g_retangulo(j, x, TAB_Y + 1, colunas[c].l, CAB_A, atual ? 0xD6E4D9 : COR_BOTAO);
        g_relevo(j, x, TAB_Y + 1, colunas[c].l, CAB_A, false);
        char t[24];
        formatar_texto(t, sizeof t, atual ? "%s %s" : "%s", colunas[c].titulo, ordem_desc ? "v" : "^");
        texto_coluna(x, TAB_Y + 3, colunas[c].l, t, colunas[c].direita, COR_TEXTO);
        x += colunas[c].l;
    }
    ordenar();
    int vis = linhas_visiveis();
    if (topo > qtd_procs - vis) topo = qtd_procs - vis;
    if (topo < 0) topo = 0;
    for (int k = 0; k < vis && topo + k < qtd_procs; k++) {
        const proc_t *p = &procs[ordem[topo + k]];
        int y = TAB_Y + 1 + CAB_A + 2 + k * LIN_A;
        bool s = p->p.pid == sel_pid;
        uint32_t cor = s ? COR_BRANCO : p->p.usuario ? COR_TEXTO : COR_TEXTO_FRACO;
        if (s) g_retangulo(j, TAB_X + 2, y, TAB_L - 4 - BARRA_L, LIN_A, COR_SELECAO);
        char t[40];
        x = TAB_X + 1;
        for (int c = 0; c < QTD_COLUNAS; c++) {
            switch (c) {
            case C_PID:    formatar_texto(t, sizeof t, "%d", p->p.pid); break;
            case C_NOME:   /* D167: con varios hilos, cuántos */
                           if (p->p.n_hilos > 1) formatar_texto(t, sizeof t, "%s (%u)", p->p.nome, p->p.n_hilos);
                           else copia_texto(t, p->p.nome, sizeof t);
                           break;
            case C_PAI:    formatar_texto(t, sizeof t, p->p.pid ? "%d" : "-", p->p.pai); break;
            case C_ESTADO: copia_texto(t, nome_estado(p->p.estado), sizeof t); break;
            case C_CPU:    formatar_texto(t, sizeof t, "%u,%u", p->cpu10 / 10, p->cpu10 % 10); break;
            case C_TEMPO:  fmt_tempo(t, sizeof t, p->p.cpu_ms); break;
            case C_MEM:    formatar_texto(t, sizeof t, "%u KB", p->p.memoria_kb); break;
            case C_JAN:    if (!p->p.janelas) t[0] = 0;
                           else formatar_texto(t, sizeof t, p->p.janelas > 1 ? "sí (%u)" : "sí", p->p.janelas);
                           break;
            case C_NUCLEO: formatar_texto(t, sizeof t, "%u", p->p.cpu); break;
            }
            texto_coluna(x, y + 1, colunas[c].l, t, colunas[c].direita, cor);
            x += colunas[c].l;
        }
    }
    /* Barra de desplazamiento (pulsar encima o debajo del pulgar desplaza una página). */
    int bx = TAB_X + TAB_L - BARRA_L - 1, by = TAB_Y + 1 + CAB_A, ba = TAB_A - CAB_A - 2;
    g_retangulo(j, bx, by, BARRA_L, ba, 0xE4E7EA);
    int pol_a = qtd_procs <= vis ? ba : ba * vis / qtd_procs;
    if (pol_a < 12) pol_a = 12;
    int pol_y = qtd_procs <= vis ? 0 : (ba - pol_a) * topo / (qtd_procs - vis);
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
    if (qtd_hist_sens < HIST) qtd_hist_sens++;
}

static void sem_dados(int x, int y, int l, int a, const char *t)
{
    g_retangulo(j, x, y, l, a, 0x10181E);
    g_moldura(j, x, y, l, a, COR_BORDA);
    int w = g_largura_texto(t);
    g_texto_n(j, x + (w < l - 16 ? (l - w) / 2 : 8), y + a / 2 - LETRA_A / 2, t, (l - 16) / LETRA_L, 0x9AA8B8);
}

static void desenhar_rendimento(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    char t[128], g[24];
    /* Cuatro gráficos en cuadrícula: CPU y memoria arriba; temperatura y
     * frecuencia abajo. La altura se ajusta a la ventana (D044). */
    int passo = (TAB_L - 16) / 2 / HIST;
    if (passo < 2) passo = 2;
    int gl = HIST * passo, x1 = TAB_X + (TAB_L - 2 * gl - 16) / 2, x2 = x1 + gl + 16;
    int ncols = qtd_nuc > 4 ? 2 : 1, nfil = (qtd_nuc + ncols - 1) / ncols;
    int reserva_nuc = 22 + nfil * 18;   /* sección "Núcleos" al pie */
    int ga = (TAB_A - 2 * 20 - 44 - 8 - reserva_nuc - (freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL ? 20 : 0)) / 2;
    if (ga > 130) ga = 130;
    if (ga < 40) ga = 40;
    int y1 = TAB_Y, y2 = TAB_Y + 20 + ga + 6;

    formatar_texto(t, sizeof t, "CPU: %u,%u %%", cpu_total10 / 10, cpu_total10 % 10);
    g_texto(j, x1, y1, t, COR_TEXTO);
    g_texto(j, x1 + gl - g_largura_texto("últimos 60 s"), y1, "últimos 60 s", COR_TEXTO_FRACO);
    desenhar_grafico(x1, y1 + 18, gl, ga, hist_cpu, qtd_hist, 0x3CDC6E, 0x165C32);
    unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
    formatar_texto(t, sizeof t, "Memoria: %u de %u MB (%u %%)", mem.usada_kb / 1024, mem.gerida_kb / 1024, (m10 + 5) / 10);
    g_texto_n(j, x2, y1, t, gl / LETRA_L, COR_TEXTO);
    desenhar_grafico(x2, y1 + 18, gl, ga, hist_mem, qtd_hist, 0x7EB6FF, 0x1E3E78);

    /* Temperatura: verde; amarillo a menos de 10 °C del máximo; rojo
     * con la CPU limitada (throttling). */
    int tm = sens_ok ? sensores_temp_maior(&sens) : -1;
    if (sens_ok && sens.qtd_temp) {
        bool limita = sens.throttling & (SENS_THR_TERMICO | SENS_THR_PROCHOT);
        bool perto = sens.max_mili_c > 0 && tm >= sens.max_mili_c - 10000;
        uint32_t cor = limita ? 0xFF5050 : perto ? 0xFFD040 : 0x3CDC6E;
        uint32_t fundo = limita ? 0x5C1616 : perto ? 0x5C4A10 : 0x165C32;
        formatar_texto(t, sizeof t, "Temperatura: %s%s", sensores_grau(g, sizeof g, tm), limita ? " LIMITANDO" : "");
        g_texto_n(j, x1, y2, t, gl / LETRA_L, limita ? 0xC02020 : perto ? 0x9A7000 : COR_TEXTO);
        if (sens.max_mili_c) {
            formatar_texto(t, sizeof t, "máx. %s", sensores_grau(g, sizeof g, sens.max_mili_c));
            g_texto(j, x1 + gl - g_largura_texto(t), y2, t, COR_TEXTO_FRACO);
        }
        desenhar_grafico(x1, y2 + 18, gl, ga, hist_temp, qtd_hist_sens, cor, fundo);
    } else {
        g_texto(j, x1, y2, "Temperatura: no disponible", COR_TEXTO);
        sem_dados(x1, y2 + 18, gl, ga, sens_ok && sens.hipervisor ? "sin sensor (máquina virtual)" : "sin sensor conocido");
    }
    const char *orig = "";
    unsigned f = freq_mostrada(&orig);
    /* Indicador del driver de frecuencia (D106), sin avisos emergentes: una
     * banda roja dentro del gráfico mientras la CPU esté limitada por
     * temperatura (y el gráfico en rojo). */
    bool limitada = freq_ok && freq.limitada;
    if (f) {
        formatar_texto(t, sizeof t, "Frecuencia: %s (%s)", sensores_mhz(g, sizeof g, f), orig);
        g_texto_n(j, x2, y2, t, gl / LETRA_L, COR_TEXTO);
        desenhar_grafico(x2, y2 + 18, gl, ga, hist_freq, qtd_hist_sens, limitada ? 0xFF5050 : 0xFFA94D,
                         limitada ? 0x5C1616 : 0x5C3A10);
    } else {
        formatar_texto(t, sizeof t, "Frecuencia: %s MHz nominal", sens.tsc_khz ? "" : "?");
        if (sens.tsc_khz) formatar_texto(t, sizeof t, "Frecuencia: %u MHz (nominal)", (sens.tsc_khz + 500) / 1000);
        g_texto_n(j, x2, y2, t, gl / LETRA_L, COR_TEXTO);
        sem_dados(x2, y2 + 18, gl, ga, "sin APERF/MPERF");
    }
    if (limitada) {   /* dentro del gráfico, arriba a la izquierda, sobre una banda roja */
        const char *ind = "limitada por temperatura";
        g_retangulo(j, x2 + 3, y2 + 21, g_largura_texto(ind) + 8, LETRA_A + 4, 0xB01818);
        g_texto(j, x2 + 7, y2 + 23, ind, 0xFFFFFF);
    }

    int yt = y2 + 18 + ga + 8;
    unsigned s = versao.segundos_ligado;
    formatar_texto(t, sizeof t, "Procesos: %d    Ventanas: %d    Encendido: %u:%02u:%02u    RAM física: %u MB", qtd_procs,
                   qtd_jans, s / 3600, s / 60 % 60, s % 60, mem.ram_total_kb / 1024);
    g_texto_n(j, x1, yt, t, (TAB_L - (x1 - TAB_X)) / LETRA_L, COR_TEXTO);
    if (sens_ok) {
        static const char *const turbo[] = { "n/d", "no", "desactivado", "activo" };
        char mult[24] = "n/d";
        if (sens.mult_x10) formatar_texto(mult, sizeof mult, "x%u,%u", sens.mult_x10 / 10, sens.mult_x10 % 10);
        formatar_texto(t, sizeof t, "Multiplicador: %s    Turbo: %s    VID: %s", mult, turbo[sens.turbo <= 3 ? sens.turbo : 0],
                       sens.vid_mv ? "" : "n/d");
        if (sens.vid_mv)
            formatar_texto(t, sizeof t, "Multiplicador: %s    Turbo: %s    VID: %u,%03u V", mult,
                           turbo[sens.turbo <= 3 ? sens.turbo : 0], sens.vid_mv / 1000, sens.vid_mv % 1000);
        g_texto_n(j, x1, yt + 20, t, (TAB_L - (x1 - TAB_X)) / LETRA_L, COR_TEXTO_FRACO);
    }
    if (freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL) {
        static const char *const est[] = { "no disponible", "activo", "desactivado", "desactivado (fallo)" };
        formatar_texto(t, sizeof t, "Control de frecuencia: %s, %s, límite %d °C", est[freq.estado <= 3 ? freq.estado : 0],
                       frequencia_politica(freq.politica), freq.limite_mc / 1000);
        g_texto_n(j, x1, yt + 40, t, (TAB_L - (x1 - TAB_X)) / LETRA_L, limitada ? 0xC02020 : COR_TEXTO_FRACO);
    }

    /* Sección "Núcleos": una fila por CPU con su estado, una barra y el % de uso. */
    int yn = yt + 20 * (1 + (sens_ok ? 1 : 0) + (freq_ok && freq.estado != FREQ_EST_NAO_DISPONIVEL ? 1 : 0));
    formatar_texto(t, sizeof t, "Núcleos: %d", qtd_nuc);
    g_texto(j, x1, yn, t, COR_TEXTO);
    int cl = (TAB_L - (x1 - TAB_X)) / ncols;
    for (int i = 0; i < qtd_nuc; i++) {
        int cx = x1 + (i / nfil) * cl, cy = yn + 18 + (i % nfil) * 18;
        unsigned u = nuc_uso10[i];
        formatar_texto(t, sizeof t, "CPU %u", nuc[i].indice);
        g_texto(j, cx, cy, t, COR_TEXTO);
        int bx = cx + 56, bl = cl > 330 ? 120 : 80;
        g_retangulo(j, bx, cy + 2, bl, 12, 0x10181E);
        g_moldura(j, bx, cy + 2, bl, 12, COR_BORDA);
        g_retangulo(j, bx + 1, cy + 3, (bl - 2) * u / 1000, 10, 0x3CDC6E);
        char fq[32];
        freq_da_cpu(nuc[i].indice, fq, sizeof fq);
        formatar_texto(t, sizeof t, "%u,%u %%  %s%s%s", u / 10, u % 10, nome_estado_nucleo(nuc[i].estado), fq[0] ? "  " : "", fq);
        g_texto_n(j, bx + bl + 8, cy, t, (cl - 56 - bl - 8) / LETRA_L, COR_TEXTO_FRACO);
    }
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
    g_texto(j, TAB_X, TAB_Y + 2, "Ventana                        Proceso", COR_TEXTO_FRACO);
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
                       servicos_texto_estado(s->estado), pid, mem, t, s->reinicios, s->arrancar ? "sí" : "no");
        if (compara_texto(s->nome, sel_serv) == 0) lista_serv.sel = i;
    }
    lista_serv.qtd = n;
    if (lista_serv.sel < 0) sel_serv[0] = 0;
}

static void desenhar_servicos(void)
{
    g_retangulo(j, TAB_X, TAB_Y, TAB_L, TAB_A, COR_FUNDO);
    if (qtd_servs < 0) {
        g_texto(j, TAB_X, TAB_Y + 2, "El supervisor de servicios no está en marcha.", COR_TEXTO);
        return;
    }
    char cab[80];
    formatar_texto(cab, sizeof cab, "%-14s %-11s %6s %8s %9s %4s  %s", "Nombre", "Estado", "PID", "Memoria", "Tiempo",
                   "Rein.", "Auto");
    g_texto(j, TAB_X + 4, TAB_Y + 2, cab, COR_TEXTO_FRACO);
    preparar_lista_servicos();
    lista_desenhar(j, &lista_serv);
    /* Debajo de la lista: la descripción y el detalle del seleccionado, o la última respuesta. */
    const servico_info_t *s = serv_selecionado();
    char t[160];
    if (s) formatar_texto(t, sizeof t, "%s%s%s", s->descricao[0] ? s->descricao : s->nome,
                          s->detalhe[0] ? " - " : "", s->detalhe);
    else copia_texto(t, aviso_serv[0] ? aviso_serv : "Selecciona un servicio de la lista.", sizeof t);
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
    if (!s) { mensagem("Servicios", "Selecciona primero un servicio de la lista."); return; }
    char nome[32], r[160];
    copia_texto(nome, s->nome, sizeof nome);
    int c = servicos_pedir(ordem, nome, r, sizeof r);
    registrar("tareas: servicio %s %s -> %d (%s)", ordem, nome, c, r);
    if (c < 0) mensagem("Servicios", c == ERRO_NAO_EXISTE ? "El supervisor de servicios no está en marcha."
                                     : c == ERRO_ACESSO ? "Hace falta ser administrador para manejar los servicios.\n"
                                                          "Usa «admin servicio ...» en el Terminal."   /* D255 */
                                                        : "El supervisor de servicios no contesta.");
    else if (c > 0) mensagem("Servicios", r);
    else copia_texto(aviso_serv, r, sizeof aviso_serv);
}

/* ------------------------------------------------------------ aba Red (D079) */

static bool dentro(const evento_t *e, int x, int y, int l, int a);

#define MAXIF 4
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
static botao_t b_renovar  = { 0, 0, 120, 22, "Renovar DHCP", false };
static botao_t b_ativar   = { 0, 0, 168, 22, "Desactivar interfaz", false };

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
    int k = info_sistema(INFO_SOCKETS, socks, MAXS);
    qtd_socks = k < 0 ? 0 : k;
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
static const char *titulos_s[QTD_COL_S] = { "Proto", "Local", "Remoto", "Estado", "PID", "Proceso" };
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
    g_texto(j, x + l - 4 - g_largura_texto("últimos 60 s"), y + 2, "últimos 60 s", 0x9AA8B8);
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
    if (!qtd_ifs) { g_texto(j, TAB_X, TAB_Y + 4, "No hay interfaces de red.", COR_TEXTO); return; }
    /* Selector de la interfaz y sus acciones. */
    for (int i = 0; i < qtd_ifs && i < MAXIF; i++) {
        b_if[i].texto = ifs[i].nome;
        botao_desenhar(j, &b_if[i]);
        if (i == if_sel) g_retangulo(j, b_if[i].x + 2, b_if[i].y + b_if[i].a - 3, b_if[i].l - 4, 2, COR_DESTAQUE);
    }
    const info_rede_t *r = &ifs[if_sel];
    if (!r->loopback) {
        b_ativar.texto = r->desativada ? "Activar interfaz" : "Desactivar interfaz";
        botao_desenhar(j, &b_ativar);
        if (r->metodo != REDE_ESTATICO || !r->ip) botao_desenhar(j, &b_renovar);
    }
    int x1 = TAB_X, x2 = TAB_X + (TAB_L > 700 ? TAB_L / 2 : 320), y = RED_INFO_Y;
    const char *estado = !r->ativa ? "sin driver activo" : r->desativada ? "Desactivada" : r->loopback ? "Activa" :
                         r->cabo ? "Conectada" : "Desconectada (sin cable)";
    formatar_texto(t, sizeof t, "Estado: %s", estado);
    g_texto(j, x1, y, t, r->desativada || (!r->cabo && !r->loopback) ? 0xB03A2E : COR_TEXTO);
    formatar_texto(t, sizeof t, "Driver: %s%s%s", r->driver[0] ? r->driver : "-", r->descricao[0] ? " - " : "", r->descricao);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    if (r->loopback) copia_texto(t, "MAC: -", sizeof t);
    else formatar_texto(t, sizeof t, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", r->mac[0], r->mac[1], r->mac[2], r->mac[3],
                        r->mac[4], r->mac[5]);
    g_texto(j, x1, y, t, COR_TEXTO);
    int bits = 0;
    for (uint32_t m = r->mascara; m & 0x80000000u; m <<= 1) bits++;
    if (r->ip) formatar_texto(t, sizeof t, "IPv4: %s/%d (%s)", ip_para_texto(r->ip, a), bits, ip_para_texto(r->mascara, b));
    else copia_texto(t, "IPv4: -", sizeof t);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    if (r->loopback) copia_texto(t, "Configuración: fija (127.0.0.1)", sizeof t);
    else if (r->metodo == REDE_POR_DHCP)
        formatar_texto(t, sizeof t, "Configuración: DHCP (quedan %u:%02u:%02u)", r->concessao_s / 3600, r->concessao_s / 60 % 60,
                       r->concessao_s % 60);
    else formatar_texto(t, sizeof t, "Configuración: %s", r->metodo == REDE_ESTATICO ? "estática" : "sin dirección");
    g_texto(j, x1, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, "Puerta: %s  DNS: %s", r->roteador ? ip_para_texto(r->roteador, a) : "-",
                   r->dns ? ip_para_texto(r->dns, b) : "-");
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    fmt_bytes(c, sizeof c, r->tx_bytes);
    fmt_bytes(d, sizeof d, r->rx_bytes);
    formatar_texto(t, sizeof t, "Enviado: %s (%u paquetes)", c, r->tx_pacotes);
    g_texto(j, x1, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, "Recibido: %s (%u paquetes)", d, r->rx_pacotes);
    g_texto_n(j, x2, y, t, (TAB_X + TAB_L - x2) / LETRA_L, COR_TEXTO);
    y += RED_LINHA;
    fmt_bytes(c, sizeof c, vel_tx[if_sel]);
    fmt_bytes(d, sizeof d, vel_rx[if_sel]);
    g_retangulo(j, x1, y + 5, 10, 4, 0xFFA94D);
    formatar_texto(t, sizeof t, "Envío: %s/s", c);
    g_texto(j, x1 + 14, y, t, COR_TEXTO);
    g_retangulo(j, x1 + 200, y + 5, 10, 4, 0x3CDC6E);
    formatar_texto(t, sizeof t, "Recepción: %s/s", d);
    g_texto(j, x1 + 214, y, t, COR_TEXTO);
    formatar_texto(t, sizeof t, "Errores: %u  Descartes: %u", r->rx_erros + r->tx_erros, r->descartados);
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
        formatar_texto(t, sizeof t, atual ? "%s %s" : "%s", titulos_s[k], desc_s ? "v" : "^");
        texto_coluna(x, ty + 3, larg_s[k], t, k == S_PID, COR_TEXTO);
        x += larg_s[k];
    }
    ordenar_socks();
    int vis = linhas_socks();
    if (topo_socks > qtd_socks - vis) topo_socks = qtd_socks - vis;
    if (topo_socks < 0) topo_socks = 0;
    if (!qtd_socks) g_texto(j, TAB_X + 8, ty + CAB_A + 6, "Sin conexiones.", COR_TEXTO_FRACO);
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
            case S_PROC:   copia_texto(t, p ? p->p.nome : s->c.pid ? "?" : "(sin dueño)", sizeof t); break;
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

static void renovar_dhcp(void)
{
    int r = config_rede(REDE_RENOVAR, NULL);
    registrar("tareas: renovar dhcp -> %d", r);
    red_mudou = true;
    if (r < 0) {
        char t[128];
        formatar_texto(t, sizeof t, "No se pudo renovar la concesión DHCP:\n%s", nome_erro(r));
        mensagem("Renovar DHCP", t);
    }
}

static void alternar_interface(void)
{
    if (!qtd_ifs || ifs[if_sel].loopback || !ifs[if_sel].pode_desativar) return;
    bool desligar = !ifs[if_sel].desativada;
    if (desligar && !confirmar("Desactivar interfaz", "¿Desactivar eth0?\n\nSe cortará el tráfico de red hasta\nque la vuelvas a activar."))
        return;
    int r = config_rede(desligar ? REDE_DESATIVAR : REDE_ATIVAR, NULL);
    registrar("tareas: %s %s -> %d", ifs[if_sel].nome, desligar ? "desactivar" : "activar", r);
    red_mudou = true;
}

static void fechar_conexao(void)
{
    const info_socket_t *s = sock_selecionado();
    if (!s) { mensagem("Cerrar conexión", "Selecciona primero una conexión de la lista."); return; }
    if (s->c.tipo != SOCK_TCP || !s->id) {
        mensagem("Cerrar conexión", "Solo se pueden cerrar conexiones TCP.\nUn socket UDP se cierra finalizando su proceso.");
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
    if (!confirmar("Cerrar conexión", t)) { registrar("tareas: cancelado"); return; }
    int res = config_rede(REDE_FECHAR, (config_rede_t *)id);
    registrar("tareas: cerrar conexion id %u -> %d", id, res);
    red_mudou = true;
    if (res < 0) {
        formatar_texto(t, sizeof t, "No se pudo cerrar la conexión:\n%s", nome_erro(res));
        mensagem("Cerrar conexión", t);
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
                   r && r->desativada ? "Activar interfaz" : "Desactivar interfaz...");
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

static void desenhar_rodape(void)
{
    g_retangulo(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6, COR_FUNDO);
    char t[96];
    unsigned m10 = qtd_hist ? hist_mem[qtd_hist - 1] : 0;
    formatar_texto(t, sizeof t, "Procesos: %d  CPU: %u%%  Mem.: %u%%%s", qtd_procs,
                   (cpu_total10 + 5) / 10, (m10 + 5) / 10, pausado ? "  (pausa)" : "");
    int x = b_pausa.x + b_pausa.l + 12;
    int x_fim = aba == ABA_SERV ? b_serv_auto.x : b_acao.x;
    g_texto_n(j, x, BOT_Y + 6, t, (x_fim - 8 - x) / LETRA_L, COR_TEXTO_FRACO);
    b_pausa.texto = pausado ? "Reanudar" : "Pausar";
    botao_desenhar(j, &b_pausa);
    if (aba == ABA_SERV) {
        const servico_info_t *s = serv_selecionado();
        bool marcha = s && (compara_texto(s->estado, SERV_E_MARCHA) == 0 || compara_texto(s->estado, SERV_E_ESPERANDO) == 0 ||
                            compara_texto(s->estado, SERV_E_REINICIANDO) == 0);
        b_acao.texto = marcha ? "Parar" : "Iniciar";
        b_serv_auto.texto = s && !s->arrancar ? "Activar" : "Desactivar";
        botao_desenhar(j, &b_serv_auto);
        botao_desenhar(j, &b_serv_rein);
        botao_desenhar(j, &b_acao);
    } else if (aba != 1) {
        b_acao.texto = aba == 0 ? "Finalizar proceso" : aba == 2 ? "Cerrar ventana" : "Cerrar conexión";
        botao_desenhar(j, &b_acao);
    } else if (freq_controlavel()) {
        /* Rendimiento: la política del driver de frecuencia (D106); cada clic pasa a la siguiente. */
        static char rot[40];
        formatar_texto(rot, sizeof rot, "Política: %s", frequencia_politica(freq.politica));
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

static void desenhar_tudo(void)
{
    g_retangulo(j, 0, PAINEL_Y + 1, LARG, ALT - PAINEL_Y - 1, COR_FUNDO);
    desenhar_abas();
    if (aba == 0) desenhar_tabela();
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
        registrar("tareas: sensores temp %d mC freq %u kHz fonte %u muestras %d", sens_ok && sens.qtd_temp ?
                  sensores_temp_maior(&sens) : -1, sens_ok ? sensores_freq_atual(&sens, NULL) : 0, sens.fonte, qtd_hist_sens);
        if (freq_ok)
            registrar("tareas: frecuencia estado %u atual %d %u kHz limitada %u temp %d", freq.estado, freq.atual,
                      freq_mostrada(NULL), freq.limitada, freq.temp_mc);
    }
    else if (maior)
        registrar("tareas: cpu %u%% mayor pid %d %s %u%%", (cpu_total10 + 5) / 10, maior->p.pid, maior->p.nome,
                  (maior->cpu10 + 5) / 10);
}

static void atualizar(void)
{
    amostrar();
    if (aba == 3) amostrar_red();   /* la red sólo se consulta con la pestaña a la vista */
    if (aba == 1) amostrar_sensores();   /* los sensores también (D092) */
    if (aba == ABA_SERV) amostrar_servicos();   /* y los servicios (D216) */
    registrar_amostra();
    if (aba == 0) { desenhar_tabela(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == 1) { desenhar_rendimento(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == 2) { desenhar_janelas(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else if (aba == ABA_SERV) { desenhar_servicos(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    else { desenhar_red(); jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A); }
    desenhar_rodape();
    jan_mostrar_area(j, 0, BOT_Y - 6, LARG, ALT - BOT_Y + 6);
}

static void finalizar_selecionado(void)
{
    const proc_t *p = proc_por_pid(sel_pid);
    if (!p) { mensagem("Finalizar proceso", "Selecciona primero un proceso de la lista."); return; }
    char t[256];
    if (p->p.pid == 0 || !p->p.usuario) {
        registrar("tareas: no se puede finalizar pid %d (%s)", p->p.pid, p->p.nome);
        formatar_texto(t, sizeof t, "\"%s\" (PID %d) es parte del kernel.\nNo se puede finalizar.", p->p.nome, p->p.pid);
        mensagem("Finalizar proceso", t);
        return;
    }
    const char *aviso = "";
    if (compara_texto(p->p.nome, "sagsh") == 0)
        aviso = "\n\nAtención: es un shell. Se perderá lo que\nesté haciendo y su Terminal dejará de responder.";
    else if (compara_texto(p->p.nome, "terminal") == 0)
        aviso = "\n\nAtención: es el Terminal. Se cerrará su\nventana y el shell que contiene.";
    else if (p->p.pid == meu_pid())
        aviso = "\n\nEs este mismo Administrador de tareas.";
    formatar_texto(t, sizeof t, "¿Finalizar el proceso \"%s\" (PID %d)?%s", p->p.nome, p->p.pid, aviso);
    int pid = p->p.pid;
    char nome[32];
    copia_texto(nome, p->p.nome, sizeof nome);
    registrar("tareas: confirmar finalizar pid %d (%s)", pid, nome);
    if (!confirmar("Finalizar proceso", t)) { registrar("tareas: cancelado"); return; }
    int r = matar(pid);
    registrar("tareas: finalizar pid %d (%s) -> %d", pid, nome, r);
    if (r < 0) {
        formatar_texto(t, sizeof t, "No se pudo finalizar el proceso %d:\n%s", pid, nome_erro(r));
        mensagem("Finalizar proceso", t);
    }
    sel_pid = -1;
}

static void cerrar_ventana(void)
{
    if (sel_jan < 0) { mensagem("Cerrar ventana", "Selecciona primero una ventana de la lista."); return; }
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
    if (!qtd_procs) return;
    ordenar();
    int pos = -1;
    for (int i = 0; i < qtd_procs; i++) if (procs[ordem[i]].p.pid == sel_pid) pos = i;
    pos = pos < 0 ? 0 : pos + delta;
    if (pos < 0) pos = 0;
    if (pos >= qtd_procs) pos = qtd_procs - 1;
    sel_pid = procs[ordem[pos]].p.pid;
    int vis = linhas_visiveis();
    if (pos < topo) topo = pos;
    if (pos >= topo + vis) topo = pos - vis + 1;
    desenhar_tabela();
    jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
}

static void tratar_tabela(const evento_t *e)
{
    if (e->tipo == EV_TECLA) {
        if (e->tecla == TECLA_BAIXO) mover_selecao(1);
        else if (e->tecla == TECLA_CIMA) mover_selecao(-1);
        else if (e->tecla == TECLA_PGDN) mover_selecao(linhas_visiveis());
        else if (e->tecla == TECLA_PGUP) mover_selecao(-linhas_visiveis());
        else if (e->tecla == TECLA_DELETE) { finalizar_selecionado(); atualizar(); }
        return;
    }
    if (e->tipo != EV_CLIQUE || !dentro(e, TAB_X, TAB_Y, TAB_L, TAB_A)) return;
    if (e->y < TAB_Y + 1 + CAB_A) {   /* cabecera: ordena */
        int x = TAB_X + 1;
        for (int c = 0; c < QTD_COLUNAS; x += colunas[c].l, c++) {
            if (e->x < x || e->x >= x + colunas[c].l) continue;
            if (c == ordem_col) ordem_desc = !ordem_desc;
            else { ordem_col = c; ordem_desc = c >= C_CPU; }   /* números grandes primeiro */
            registrar("tareas: ordenar por %s %s", colunas[c].titulo, ordem_desc ? "desc" : "asc");
            break;
        }
    } else if (e->x >= TAB_X + TAB_L - BARRA_L - 1) {   /* barra de rolagem */
        int vis = linhas_visiveis();
        topo += e->y < TAB_Y + TAB_A / 2 ? -vis : vis;
    } else {
        int k = (e->y - (TAB_Y + 1 + CAB_A + 2)) / LIN_A;
        if (k >= 0 && topo + k < qtd_procs) {
            ordenar();
            const proc_t *p = &procs[ordem[topo + k]];
            sel_pid = p->p.pid;
            registrar("tareas: seleccionado pid %d (%s)", p->p.pid, p->p.nome);
        }
    }
    desenhar_tabela();
    jan_mostrar_area(j, TAB_X, TAB_Y, TAB_L, TAB_A);
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-t") == 0) return modo_texto();
    j = jan_abrir("Administrador de tareas", LARG, ALT);
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
                    if (e.x >= abas_x[i] && e.x < abas_x[i] + abas_l[i] && aba != i) {
                        aba = i;
                        registrar("tareas: pestana %s", nomes_log[i]);
                        if (aba == 3) amostrar_red();   /* a velocidade aparece na amostra seguinte */
                        if (aba == 1) amostrar_sensores();
                        if (aba == ABA_SERV) { amostrar(); amostrar_servicos(); }
                        desenhar_tudo();
                        registrar_amostra();
                    }
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
                if (botao_tratar(j, &b_acao, &e)) o = compara_texto(b_acao.texto, "Parar") == 0 ? "parar" : "iniciar";
                else if (botao_tratar(j, &b_serv_rein, &e)) o = "reiniciar";
                else if (botao_tratar(j, &b_serv_auto, &e)) o = compara_texto(b_serv_auto.texto, "Activar") == 0 ? "activar" : "desactivar";
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
                if (aba == 0) finalizar_selecionado();
                else if (aba == 2) cerrar_ventana();
                else fechar_conexao();
                dormir(100);   /* deja que el proceso (o la ventana) termine antes de mirar de nuevo */
                atualizar();
                ultimo = tempo_ms();
                continue;
            }
            if (aba == 0) tratar_tabela(&e);
            else if (aba == 3) {
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
