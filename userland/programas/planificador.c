// SPDX-License-Identifier: GPL-2.0-or-later
/* planificador - el planificador de procesos entre las CPU (D174).
 *
 *   planificador              el activo y los que hay
 *   planificador NOMBRE       cambia en caliente (rr, afinidad, equilibrio, prioridad)
 *   planificador -p [PID]     por proceso: CPU, máscara, nice, migraciones, prioridad
 *   planificador nice PID N   el nice de un proceso (-20..19; solo cuenta con "prioridad")
 *
 * La máscara de CPU se cambia con la orden "afinidad". */
#include "sagui.h"

static const char *const explicacao[PLANIFICADOR_MAX + 1] = {
    "",
    "round-robin global: cada CPU toma el siguiente listo de la tabla",
    "cada proceso vuelve a su CPU; otra solo se lo lleva si está libre y él, frío",
    "una cola por CPU, el nuevo a la menos cargada y reparto cada 50 ms",
    "nice y uso reciente: los interactivos pasan delante y quitan la CPU",
};

static void mostrar_planificadores(void)
{
    char nome[24];
    int ativo = planificador_consultar(nome, sizeof nome);
    imprimir("Planificador activo: %s\n", ativo > 0 ? nome : "?");
    for (int i = 1; i <= PLANIFICADOR_MAX; i++) {
        if (planificador_nome(i, nome, sizeof nome) != 0) continue;
        imprimir(" %c %-11s %s\n", i == ativo ? '*' : ' ', nome, explicacao[i]);
    }
}

static void mascara_texto(unsigned m, char *t, unsigned n)
{
    if (!m) { formatar_texto(t, n, "todas"); return; }
    unsigned k = 0;
    t[0] = 0;
    for (int c = 0; c < 32 && k + 4 < n; c++)
        if (m & (1u << c)) k += (unsigned)formatar_texto(t + k, n - k, k ? ",%d" : "%d", c);
}

static int linha_processo(int pid, const char *nome)
{
    info_planificacion_t v;
    if (planificacion_info(pid, &v) != 0) return 0;
    char m[40];
    mascara_texto(v.mascara, m, sizeof m);
    imprimir("%5d %-16s %3u %-9s %4d %6u %6u %5d\n", v.pid, nome, v.cpu, m, v.nice, v.migracoes,
             v.execucoes, v.prioridade);
    return 1;
}

static void por_processo(int so)
{
    static info_processo_t p[64];
    int n = info_sistema(INFO_PROCESSOS, p, 64);
    imprimir("  PID Nombre           CPU CPUs      nice  migr.  ejec.  prio\n");
    for (int i = 0; i < n; i++) {
        if (p[i].pid == 0 || p[i].estado == 4) continue;   /* el ocioso y los zombis */
        if (so && p[i].pid != so) continue;
        linha_processo(p[i].pid, p[i].nome);
    }
}

int principal(int argc, char **argv)
{
    if (argc < 2) { mostrar_planificadores(); return 0; }
    if (compara_texto(argv[1], "-p") == 0) {
        por_processo(argc > 2 ? texto_para_int(argv[2]) : 0);
        return 0;
    }
    if (compara_texto(argv[1], "nice") == 0) {
        if (argc != 4) { imprimir_em(ERROS, "uso: planificador nice <pid> <n>   (-20..19)\n"); return 1; }
        int pid = texto_para_int(argv[2]), n = texto_para_int(argv[3]);
        info_planificacion_t v;
        int antes = planificacion_info(pid, &v) == 0 ? v.nice : 0;
        int r = prioridade_processo(pid, n);
        if (r < 0) {
            imprimir_em(ERROS, "planificador: %d: %s\n", pid, r == ERRO_INVALIDO ? "el nice va de -20 a 19" : nome_erro(r));
            return 1;
        }
        imprimir("Proceso %d: nice %d (antes %d)\n", pid, n, antes);
        return 0;
    }
    if (argv[1][0] == '-' || argc > 2) {
        imprimir_em(ERROS, "uso: planificador [rr|afinidad|equilibrio|prioridad] | -p [pid] | nice <pid> <n>\n");
        return 1;
    }
    int numero = planificador_numero(argv[1]);
    if (numero < 0) {
        imprimir_em(ERROS, "planificador: no conozco \"%s\"\n", argv[1]);
        mostrar_planificadores();
        return 1;
    }
    int antes = planificador_escolher(numero);
    char a[24] = "?";
    if (antes > 0) planificador_nome(antes, a, sizeof a);
    imprimir("Planificador: %s (antes %s)\n", argv[1], a);
    registrar("planificador: %s (antes %s)", argv[1], a);
    return 0;
}
