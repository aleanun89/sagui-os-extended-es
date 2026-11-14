// SPDX-License-Identifier: GPL-2.0-or-later
/* top - los procesos en vivo, en el terminal (D232). Para la vista gráfica: tareas. */
#include "sagui.h"

static const char *AYUDA =
    "uso: top [-d SEGUNDOS] [-n VECES] [-b] [-o cpu|mem]\n"
    "  Muestra cada pocos segundos (2 por defecto) el uso de CPU y memoria y los procesos que más CPU gastan.\n"
    "  Ctrl+C sale.   -n N  se detiene tras N pantallas   -b  modo por lotes: no limpia la pantalla\n"
    "  -o mem  ordena por memoria en vez de por CPU\n"
    "  El % de CPU de un proceso es la parte del tiempo del intervalo que estuvo ejecutando (100% = una CPU entera).\n";

#define MAXP 64

typedef struct { int pid; unsigned cpu_ms; } muestra_t;

static info_processo_t v[MAXP];
static muestra_t ant[MAXP];
static int nant;

static unsigned cpu_anterior(int pid, unsigned actual)
{
    for (int i = 0; i < nant; i++) if (ant[i].pid == pid) return ant[i].cpu_ms;
    return actual;
}

int principal(int argc, char **argv)
{
    unsigned ms = 2000;
    int veces = -1;
    bool lotes = false, por_mem = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "d:n:bo:", &o)) >= 0) {
        switch (r) {
        case 'd': {
            unsigned e = 0, d = 0;
            const char *p = o.valor;
            while (*p >= '0' && *p <= '9') e = e * 10 + (unsigned)(*p++ - '0');
            if (*p == '.' && p[1] >= '0' && p[1] <= '9') d = (unsigned)(p[1] - '0') * 100;
            ms = e * 1000 + d;
            if (ms < 200) ms = 200;
            break;
        }
        case 'n': if (!texto_para_numero(o.valor, &veces) || veces < 1) { imprimir_em(ERROS, "top: -n: número no válido\n"); return 2; } break;
        case 'b': lotes = true; break;
        case 'o': por_mem = compara_texto(o.valor, "mem") == 0; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("top", r, &o);
        }
    }
    bool limpiar = !lotes && eh_terminal(SAIDA);
    static info_nucleo_t nuc[64];
    int ncpu = info_sistema(INFO_NUCLEOS, nuc, 64);
    if (ncpu < 1) ncpu = 1;
    unsigned t_ant = tempo_ms();
    int n = info_sistema(INFO_PROCESSOS, v, MAXP);
    for (int i = 0; i < n; i++) { ant[i].pid = v[i].pid; ant[i].cpu_ms = v[i].cpu_ms; }
    nant = n;
    for (int k = 0; veces < 0 || k < veces; k++) {
        dormir(ms);
        n = info_sistema(INFO_PROCESSOS, v, MAXP);
        if (n < 0) { imprimir_em(ERROS, "top: %s\n", nome_erro(n)); return 1; }
        unsigned t = tempo_ms(), dt = t - t_ant ? t - t_ant : 1;
        t_ant = t;
        unsigned pct[MAXP];   /* en décimas de % */
        unsigned total = 0;
        for (int i = 0; i < n; i++) {
            unsigned d = v[i].cpu_ms - cpu_anterior(v[i].pid, v[i].cpu_ms);
            pct[i] = d * 1000 / dt;
            if (v[i].pid != 0) total += pct[i];
        }
        int ord[MAXP];
        for (int i = 0; i < n; i++) ord[i] = i;
        for (int i = 1; i < n; i++) {   /* inserción: pocos procesos */
            int x = ord[i], j = i;
            while (j > 0 && (por_mem ? v[ord[j - 1]].memoria_kb < v[x].memoria_kb : pct[ord[j - 1]] < pct[x])) { ord[j] = ord[j - 1]; j--; }
            ord[j] = x;
        }
        info_memoria_t m;
        memset(&m, 0, sizeof m);
        info_sistema(INFO_MEMORIA, &m, sizeof m);
        unsigned seg = tempo_ms() / 1000;
        if (limpiar) limpar_tela();
        else if (k) imprimir("\n");
        unsigned por_cpu = total / (unsigned)ncpu;
        imprimir("top - encendido %u:%02u:%02u, %d procesos, %d CPU\n", seg / 3600, seg / 60 % 60, seg % 60, n, ncpu);
        imprimir("CPU: %u,%u%%   Memoria: %u KB usados de %u KB (%u KB libres)\n", por_cpu / 10, por_cpu % 10, m.usada_kb, m.gerida_kb, m.livre_kb);
        imprimir("\n  PID PADRE ESTADO      CPU%%  MEMORIA  T.CPU(ms) NOMBRE\n");
        static const char *const est[] = { "libre", "listo", "ejecutando", "bloqueado", "zombi" };
        int mostrar = limpiar ? 20 : n;
        for (int i = 0; i < n && i < mostrar; i++) {
            const info_processo_t *p = &v[ord[i]];
            imprimir("%5d %5d %-10s %3u,%u %5u KB %10u %s%s\n", p->pid, p->pai, p->estado <= 4 ? est[p->estado] : "?",
                     pct[ord[i]] / 10, pct[ord[i]] % 10, p->memoria_kb, p->cpu_ms, p->nome, p->usuario ? "" : " (kernel)");
        }
        for (int i = 0; i < n; i++) { ant[i].pid = v[i].pid; ant[i].cpu_ms = v[i].cpu_ms; }
        nant = n;
    }
    return 0;
}
