// SPDX-License-Identifier: GPL-2.0-or-later
/* planif.c - prueba y medida de los planificadores (D174, h144).
 *
 * Uso: /testes/planif NOMBRE [PARTES]
 *   NOMBRE: el planificador a poner en caliente (rr, afinidad, equilibrio,
 *           prioridad), "-" para dejar el que hay, o "todos" (sin --smp:
 *           cada uno un momento).
 *   PARTES (por defecto "bmlao"):
 *     b  bench 4 c, bench 4 h y bench 4 s (las ganancias)
 *     m  migraciones: 4 y 6 procesos de cálculo, cuántas veces cambian de CPU
 *     l  latencia: un proceso que duerme 10 ms en bucle mientras 4 calculan;
 *        el kernel mide de despertar a correr (PLAN_INFO)
 *     a  afinidad fija: un proceso con máscara de una CPU no sale de ella
 *     o  las órdenes "planificador" y "afinidad"
 *     t  topología: 2 procesos de cálculo, ¿en núcleos físicos distintos?
 * Todo se escribe en el registro (serie) con "planif: ..." y acaba con
 * "planif: fin". */
#include "sagui.h"

#define MS_CALCULO 2000

static int ncpu;
static info_nucleo_t nuc[8];

static int lancar(const char *modo, unsigned ms)
{
    char t[12];
    formatar_texto(t, sizeof t, "%u", ms);
    char *args[5] = { "/testes/planif", "--hijo", (char *)modo, t, 0 };
    return executar("/testes/planif", args);
}

static int correr(char *const args[])
{
    int pid = executar(args[0], args);
    int codigo = -1;
    if (pid > 0) esperar(pid, &codigo);
    return pid > 0 ? codigo : pid;
}

/* El hijo: calcula "ms" milisegundos (o duerme en bucle, modo "d"). */
static int hijo(const char *modo, unsigned ms)
{
    unsigned t0 = tempo_ms();
    volatile unsigned x = 1;
    while (tempo_ms() - t0 < ms)
        for (int i = 0; i < 200000; i++) x = x * 1664525u + 1013904223u;
    (void)modo;
    return x == 0;
}

static void bench(void)
{
    static const char *const modos[] = { "c", "h", "s" };
    for (int i = 0; i < 3; i++) {
        registrar("planif: bench %s", modos[i]);
        char *args[] = { "/testes/bench", "4", (char *)modos[i], 0 };
        correr(args);
    }
}

/* n procesos de cálculo; mientras corren se miran sus migraciones (al
 * acabar ya no se pueden consultar: son zombis). */
static void migraciones(int n)
{
    int pids[8];
    info_planificacion_t ult[8];
    memset(ult, 0, sizeof ult);
    for (int i = 0; i < n; i++) pids[i] = lancar("c", MS_CALCULO);
    unsigned t0 = tempo_ms();
    while (tempo_ms() - t0 < MS_CALCULO + 500) {
        dormir(50);
        int vivos = 0;
        for (int i = 0; i < n; i++) {
            info_planificacion_t v;
            if (pids[i] > 0 && planificacion_info(pids[i], &v) == 0 && v.execucoes) { ult[i] = v; vivos++; }
        }
        if (!vivos && tempo_ms() - t0 > MS_CALCULO) break;
    }
    unsigned migr = 0, exec = 0, maxm = 0;
    for (int i = 0; i < n; i++) {
        int c;
        if (pids[i] > 0) esperar(pids[i], &c);
        migr += ult[i].migracoes;
        exec += ult[i].execucoes;
        if (ult[i].migracoes > maxm) maxm = ult[i].migracoes;
    }
    registrar("planif: migraciones %d procesos: total %u (max %u por proceso), ejecuciones %u", n, migr, maxm, exec);
}

/* La latencia: este proceso duerme 10 ms 150 veces con 4 procesos de
 * cálculo; luego, la sin carga para comparar. */
static void latencia_uma(const char *rotulo, int carga)
{
    int pids[4];
    for (int i = 0; i < carga; i++) pids[i] = lancar("c", 3000);
    dormir(200);
    info_planificacion_t a, b;
    planificacion_info(0, &a);
    for (int i = 0; i < 150; i++) dormir(10);
    planificacion_info(0, &b);
    for (int i = 0; i < carga; i++) {
        int c;
        if (pids[i] > 0) esperar(pids[i], &c);
    }
    /* La media de estos despertares (la del kernel es de toda la vida del
     * proceso); el máximo sí es de toda la vida. */
    unsigned n = b.despertares - a.despertares;
    unsigned media = n ? (b.lat_total_us - a.lat_total_us) / n : 0;
    registrar("planif: latencia %s: media %u us, max %u us (%u despertares)", rotulo, media, b.lat_max_us, n);
}

static void latencia(void)
{
    latencia_uma("sin carga", 0);
    /* El proceso es nuevo en cada medida: si no, la media mezclaría las dos. */
    char *args[] = { "/testes/planif", "--latencia", 0 };
    correr(args);
}

static int cpu_de(int pid)
{
    ncpu = info_sistema(INFO_NUCLEOS, nuc, 8);
    for (int c = 0; c < ncpu; c++)
        if ((int)nuc[c].pid == pid) return c;
    return -1;
}

/* Un proceso con máscara de una sola CPU (la última) no corre en otra. */
static void afinidad_fija(void)
{
    ncpu = info_sistema(INFO_NUCLEOS, nuc, 8);
    int alvo = ncpu > 1 ? ncpu - 1 : 0;
    int pids[5];
    for (int i = 0; i < 5; i++) pids[i] = lancar("c", 1500);
    int r = afinidade_cpu(pids[0], 1u << alvo);
    dormir(100);
    int vistas = 0, fora = 0;
    for (int k = 0; k < 40; k++) {
        dormir(25);
        info_planificacion_t v;
        if (planificacion_info(pids[0], &v) == 0 && v.execucoes) {
            vistas++;
            if (v.cpu != (unsigned)alvo) fora++;
        }
        int c = cpu_de(pids[0]);
        if (c >= 0 && c != alvo) fora++;
    }
    for (int i = 0; i < 5; i++) {
        int c;
        if (pids[i] > 0) esperar(pids[i], &c);
    }
    /* Una máscara sin ninguna CPU que trabaje se rechaza. */
    int pid = lancar("c", 100);
    int mala = afinidade_cpu(pid, 0x80u);
    int c;
    esperar(pid, &c);
    registrar("planif: afinidad fija CPU %d: %s (%d muestras, %d fuera, r=%d, mascara imposible %s)", alvo,
              fora == 0 && vistas > 0 && r >= 0 ? "respetada" : "FALLO", vistas, fora, r,
              ncpu < 8 && mala >= 0 ? "ACEPTADA" : "rechazada");
}

static void ordens(void)
{
    char *a1[] = { "/bin/planificador", 0 };
    correr(a1);
    int pid = lancar("c", 600);
    char p[12];
    formatar_texto(p, sizeof p, "%d", pid);
    char *a2[] = { "/bin/afinidad", p, "-c", "0", 0 };
    int r2 = correr(a2);
    char *a3[] = { "/bin/afinidad", p, 0 };
    correr(a3);
    char *a4[] = { "/bin/planificador", "nice", p, "5", 0 };
    int r4 = correr(a4);
    info_planificacion_t v;
    int ok = planificacion_info(pid, &v) == 0 && v.nice == 5 && v.mascara == 1;
    char *a5[] = { "/bin/planificador", "-p", p, 0 };
    correr(a5);
    int c;
    esperar(pid, &c);
    registrar("planif: ordenes: %s (afinidad %d, nice %d)", ok && r2 == 0 && r4 == 0 ? "ok" : "FALLO", r2, r4);
}

/* Topología: dos procesos de cálculo; ¿cuántas muestras en núcleos físicos
 * distintos? */
static void topologia(void)
{
    ncpu = info_sistema(INFO_NUCLEOS, nuc, 8);
    char lin[160];
    unsigned k = 0;
    for (int c = 0; c < ncpu && k < sizeof lin - 24; c++)
        k += (unsigned)formatar_texto(lin + k, sizeof lin - k, " CPU%d=n%u.h%u", c, nuc[c].nucleo, nuc[c].hilo);
    registrar("planif: topologia:%s", lin);
    /* Tres procesos y el primero acaba pronto: sin mirar la topología, los
     * dos que quedan pueden acabar en los dos hilos de un núcleo mientras el
     * otro núcleo está entero ocioso (el round-robin los deja así). */
    int corto = lancar("c", 300);
    int pids[2] = { lancar("c", 2400), lancar("c", 2400) };
    int cc;
    esperar(corto, &cc);
    dormir(300);
    int distintos = 0, mismos = 0;
    for (int s = 0; s < 60; s++) {
        dormir(25);
        int c0 = cpu_de(pids[0]), c1 = cpu_de(pids[1]);
        if (c0 < 0 || c1 < 0) continue;
        if (nuc[c0].nucleo != nuc[c1].nucleo) distintos++;
        else mismos++;
    }
    for (int i = 0; i < 2; i++) {
        int c;
        esperar(pids[i], &c);
    }
    registrar("planif: topologia 2 procesos: %d muestras en nucleos distintos, %d en el mismo", distintos, mismos);
}

int principal(int argc, char **argv)
{
    if (argc > 3 && compara_texto(argv[1], "--hijo") == 0)
        return hijo(argv[2], (unsigned)texto_para_int(argv[3]));
    if (argc > 1 && compara_texto(argv[1], "--latencia") == 0) {
        latencia_uma("con 4 calculando", 4);
        return 0;
    }
    if (argc < 2) {
        imprimir_em(ERROS, "uso: /testes/planif rr|afinidad|equilibrio|prioridad|-|todos [bmlaot]\n");
        return 1;
    }
    const char *partes = argc > 2 ? argv[2] : "bmlao";
    char nome[24];
    ncpu = info_sistema(INFO_NUCLEOS, nuc, 8);
    if (compara_texto(argv[1], "todos") == 0) {
        /* Sin --smp: cada planificador, en caliente, con un poco de trabajo. */
        for (int n = 1; n <= PLANIFICADOR_MAX; n++) {
            planificador_escolher(n);
            planificador_consultar(nome, sizeof nome);
            registrar("planif: con %s", nome);
            char *args[] = { "/testes/bench", "4", "c", 0 };
            correr(args);
            latencia_uma("con 4 calculando", 4);
        }
        registrar("planif: fin");
        return 0;
    }
    if (compara_texto(argv[1], "-") != 0) {
        int n = planificador_numero(argv[1]);
        if (n < 0) { registrar("planif: no conozco %s", argv[1]); return 1; }
        int antes = planificador_escolher(n);
        registrar("planif: cambio en caliente a %s: %s", argv[1], antes > 0 ? "ok" : "FALLO");
    }
    planificador_consultar(nome, sizeof nome);
    registrar("planif: activo %s, %d CPU", nome, ncpu);
    for (const char *p = partes; *p; p++) {
        switch (*p) {
        case 'b': bench(); break;
        case 'm': migraciones(4); migraciones(6); break;
        case 'l': latencia(); break;
        case 'a': afinidad_fija(); break;
        case 'o': ordens(); break;
        case 't': topologia(); break;
        }
    }
    registrar("planif: fin");
    return 0;
}
