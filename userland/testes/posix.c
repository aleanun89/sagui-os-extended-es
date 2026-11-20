// SPDX-License-Identifier: GPL-3.0-only
/* posix.c - prueba de los procesos POSIX (D401, h403): fork con copia al escribir, exec, waitpid,
 * señales, poll/select y descriptores no bloqueantes, con la libc común.
 *
 *   posix                    todas las partes
 *   posix exec FD PIPE       (lo usa la parte «exec»: el programa tras execve)
 *
 * Cada comprobación sale como «  [ok] ...» o «  [FALHA] ...» y al final
 * «RESULTADO posix: SUCESSO (0 falhas)», como verifica.h. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ucontext.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "sagui.h"

static int falhas;

static void ok(int cond, const char *fmt, ...)
{
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("  [%s] %s\n", cond ? "ok" : "FALHA", b);
    fflush(stdout);
    if (!cond) falhas++;
}

static void parte(const char *nome)
{
    printf("POSIX %s\n", nome);
    fflush(stdout);
}

static unsigned livre_kb(void)
{
    info_memoria_t m;
    memset(&m, 0, sizeof m);
    info_sistema(INFO_MEMORIA, &m, INFO_MEMORIA_V1);
    return m.livre_kb;
}

static unsigned memoria_de(int pid)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].pid == pid) return v[i].memoria_kb;
    return 0;
}

static unsigned agora_ms(void) { return tempo_ms(); }

static void esperar_ms(unsigned ms) { dormir(ms); }

/* Lee exactamente n bytes (o hasta el fin). */
static int ler_tudo(int fd, void *b, int n)
{
    int k = 0;
    while (k < n) {
        int r = read(fd, (char *)b + k, (size_t)(n - k));
        if (r <= 0) break;
        k += r;
    }
    return k;
}

/* ------------------------------------------------------------------ 1. fork + exec + wait */

static int modo_exec(int argc, char **argv)
{
    /* Tras execve: el descriptor FD_CLOEXEC ya no existe; el de la tubería sí. */
    int fd = atoi(argv[2]), tub = atoi(argv[3]);
    int cerrado = fcntl(fd, F_GETFD) == -1 && errno == EBADF;
    int v[3] = { getpid(), getppid(), cerrado };
    write(tub, v, sizeof v);
    (void)argc;
    return 7;
}

static void p_fork_exec(void)
{
    parte("fork+exec+wait");
    int tub[2];
    pipe(tub);
    int fd = open("/testes/posix", O_RDONLY | O_CLOEXEC);
    ok(fd >= 0 && (fcntl(fd, F_GETFD) & FD_CLOEXEC), "open con O_CLOEXEC deja FD_CLOEXEC (fd %d)", fd);
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) {
        char a[16], b[16];
        snprintf(a, sizeof a, "%d", fd);
        snprintf(b, sizeof b, "%d", tub[1]);
        char *argv[] = { "posix", "exec", a, b, NULL };
        execv("/testes/posix", argv);
        _exit(99);
    }
    ok(p > 0, "fork devuelve el pid del hijo (%d)", p);
    close(tub[1]);
    int v[3] = { 0, 0, 0 };
    int n = ler_tudo(tub[0], v, sizeof v);
    close(tub[0]);
    int st = -1;
    pid_t w = waitpid(p, &st, 0);
    ok(n == sizeof v, "el programa nuevo escribió por la tubería heredada");
    ok(v[0] == p, "execve conserva el pid (%d = %d)", v[0], p);
    ok(v[1] == getpid(), "y el padre (getppid %d = %d)", v[1], getpid());
    ok(v[2] == 1, "el descriptor FD_CLOEXEC se cerró en el exec");
    ok(w == p && WIFEXITED(st) && WEXITSTATUS(st) == 7, "waitpid: salió con 7 (estado %#x)", st);
    close(fd);

    fflush(NULL);
    p = fork();
    if (p == 0) {
        char *argv[] = { "nada", NULL };
        execv("/no/existe/nada", argv);
        _exit(errno == ENOENT ? 3 : 4);
    }
    waitpid(p, &st, 0);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 3, "execve de algo que no existe vuelve con ENOENT y el proceso sigue");

    ok(waitpid(-1, &st, WNOHANG) == -1 && errno == ECHILD, "sin hijos: ECHILD");
    fflush(NULL);
    p = fork();
    if (p == 0) { esperar_ms(300); _exit(5); }
    ok(waitpid(p, &st, WNOHANG) == 0, "WNOHANG con el hijo vivo devuelve 0");
    ok(wait(&st) == p && WEXITSTATUS(st) == 5, "wait() recoge al hijo (5)");

    /* system y popen sobre fork + exec */
    int r = system("exit 9");
    ok(WIFEXITED(r) && WEXITSTATUS(r) == 9, "system(\"exit 9\") = %#x", r);
    FILE *f = popen("echo hola-popen", "r");
    char linha[64] = "";
    if (f) { fgets(linha, sizeof linha, f); }
    int pc = f ? pclose(f) : -1;
    ok(f && strncmp(linha, "hola-popen", 10) == 0 && pc == 0, "popen lee la salida de sh -c (\"%.20s\")", linha);
}

/* ------------------------------------------------------------------ 2. copia al escribir */

#define MB8 (8u * 1024u * 1024u)

static void p_cow(void)
{
    parte("copia al escribir");
    unsigned char *b = malloc(MB8);
    if (!b) { ok(0, "malloc de 8 MB"); return; }
    for (unsigned i = 0; i < MB8; i += 4096) b[i] = (unsigned char)(i >> 12);
    int a_hijo[2], a_pai[2];
    pipe(a_hijo);
    pipe(a_pai);
    unsigned mem_antes = memoria_de(getpid());
    unsigned l0 = livre_kb();
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) {
        char c;
        read(a_hijo[0], &c, 1);                         /* espera: el padre mide */
        int bien = 1;
        for (unsigned i = 0; i < MB8; i += 4096) if (b[i] != (unsigned char)(i >> 12)) bien = 0;
        for (unsigned i = 0; i < MB8; i += 4096) b[i] = 0xAA;   /* ahora sí: copia cada página */
        write(a_pai[1], &bien, sizeof bien);
        read(a_hijo[0], &c, 1);                         /* el padre mide otra vez */
        for (unsigned i = 0; i < MB8; i += 4096) if (b[i] != 0xAA) bien = 0;
        write(a_pai[1], &bien, sizeof bien);
        _exit(0);
    }
    esperar_ms(100);
    unsigned l1 = livre_kb();
    unsigned mp = memoria_de(getpid()), mh = memoria_de(p);
    ok(l0 - l1 < 1536, "fork de un proceso con 8 MB gasta %u KB (sin duplicar la memoria)", l0 - l1);
    ok(mh < mem_antes * 3 / 4 && mp + mh < mem_antes + mem_antes / 4 + 256,
       "Tareas reparte lo compartido: antes %u KB; padre %u KB e hijo %u KB", mem_antes, mp, mh);
    char c = 1;
    write(a_hijo[1], &c, 1);
    int bien = 0;
    read(a_pai[0], &bien, sizeof bien);
    unsigned l2 = livre_kb();
    ok(bien, "el hijo ve la memoria del padre");
    ok(l1 - l2 > 7000, "escribir las 2048 páginas en el hijo las copia (%u KB)", l1 - l2);
    for (unsigned i = 0; i < MB8; i += 4096) b[i] = 0x55;   /* el padre escribe lo suyo */
    write(a_hijo[1], &c, 1);
    read(a_pai[0], &bien, sizeof bien);
    int pai_bien = 1;
    for (unsigned i = 0; i < MB8; i += 4096) if (b[i] != 0x55) pai_bien = 0;
    ok(bien && pai_bien, "padre e hijo escriben y cada uno ve lo suyo");
    int st;
    waitpid(p, &st, 0);
    free(b);
    close(a_hijo[0]); close(a_hijo[1]); close(a_pai[0]); close(a_pai[1]);
}

/* ------------------------------------------------------------------ 3. muchos fork */

static void p_muchos(void)
{
    parte("200 fork seguidos");
    unsigned l0 = livre_kb();
    int bien = 0;
    unsigned t0 = agora_ms();
    for (int i = 0; i < 200; i++) {
        fflush(NULL);
        pid_t p = fork();
        if (p == 0) _exit(i & 0x7F);
        if (p < 0) break;
        int st;
        if (waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == (i & 0x7F)) bien++;
    }
    unsigned t = agora_ms() - t0;
    unsigned l1 = livre_kb();
    ok(bien == 200, "200 fork + _exit + waitpid con el código correcto (%d)", bien);
    int perdidos = (int)l0 - (int)l1;
    ok(perdidos < 192, "sin perder memoria: %d KB de diferencia", perdidos);
    printf("POSIX medida: 200 fork+exit+wait en %u ms\n", t);
}

/* ------------------------------------------------------------------ 4. fork con hilos */

static volatile int parar_hilos;
static volatile unsigned contadores[3];

static void *hilo_contar(void *arg)
{
    int i = (int)arg;
    while (!parar_hilos) { contadores[i]++; if ((contadores[i] & 1023) == 0) thread_yield(); }
    return 0;
}

static void *hilo_desbloqueia(void *arg)
{
    (void)arg;
    sigset_t m;
    sigemptyset(&m);
    sigaddset(&m, SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &m, NULL);
    while (!parar_hilos) thread_yield();
    return 0;
}

static void *hilo_que_bifurca(void *arg)
{
    (void)arg;
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) _exit(42);
    int st = 0;
    if (waitpid(p, &st, 0) != p) return (void *)-1;
    return (void *)(WIFEXITED(st) ? WEXITSTATUS(st) : -2);
}

/* Un hilo suma N exactamente en una página que el fork pone de copia al escribir una y otra vez:
 * si una CPU siguiera escribiendo en el marco viejo (TLB sin sincronizar), se perderían sumas. */
#define SUMAS 400000
static volatile unsigned soma_exacta;
static void *hilo_sumar(void *arg)
{
    (void)arg;
    for (int i = 0; i < SUMAS; i++) soma_exacta++;
    return 0;
}

static void p_hilos(void)
{
    parte("fork con hilos");
    thread_t *h[3];
    parar_hilos = 0;
    for (int i = 0; i < 3; i++) h[i] = thread_create(hilo_contar, (void *)i, 0);
    esperar_ms(50);
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) {
        unsigned a[3], b2[3];
        esperar_ms(50);
        for (int i = 0; i < 3; i++) a[i] = contadores[i];
        esperar_ms(150);
        for (int i = 0; i < 3; i++) b2[i] = contadores[i];
        int quietos = a[0] == b2[0] && a[1] == b2[1] && a[2] == b2[2];
        /* El hijo puede crear sus propios hilos. */
        thread_t *t = thread_create(hilo_que_bifurca, 0, 0);
        int r = t ? thread_join(t) : -1;
        _exit(quietos && r == 42 ? 0 : 1);
    }
    unsigned a0 = contadores[0];
    int st = -1;
    waitpid(p, &st, 0);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0, "en el hijo solo está el hilo que llamó (los otros no corren) y puede crear hilos");
    ok(contadores[0] != a0, "en el padre los hilos siguen");
    parar_hilos = 1;
    for (int i = 0; i < 3; i++) if (h[i]) thread_join(h[i]);
    thread_t *t = thread_create(hilo_que_bifurca, 0, 0);
    int r = t ? thread_join(t) : -1;
    ok(r == 42, "fork desde un hilo que no es el principal (el hijo salió con %d)", r);

    soma_exacta = 0;
    thread_t *s = thread_create(hilo_sumar, 0, 0);
    int forks = 0;
    while (soma_exacta < SUMAS - 1000 && forks < 60) {
        fflush(NULL);
        pid_t q = fork();
        if (q == 0) _exit(soma_exacta > SUMAS ? 1 : 0);
        if (q > 0) { waitpid(q, &st, 0); forks++; }
    }
    if (s) thread_join(s);
    ok(soma_exacta == SUMAS, "un hilo suma %u mientras se hacen %d fork: %u (ninguna escritura perdida)", SUMAS, forks,
       soma_exacta);
}

/* ------------------------------------------------------------------ 5. tuberías */

static void p_tuberias(void)
{
    parte("tuberias padre-hijo");
    int ida[2], volta[2];
    pipe(ida);
    pipe(volta);
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) {
        close(ida[1]);
        close(volta[0]);
        unsigned soma = 0, n = 0;
        unsigned char buf[700];
        int r;
        while ((r = read(ida[0], buf, sizeof buf)) > 0) {
            for (int i = 0; i < r; i++) soma = soma * 31 + buf[i];
            n += (unsigned)r;
        }
        unsigned v[2] = { soma, n };
        write(volta[1], v, sizeof v);
        _exit(0);
    }
    close(ida[0]);
    close(volta[1]);
    unsigned soma = 0;
    unsigned char buf[1000];
    for (int k = 0; k < 100; k++) {
        for (int i = 0; i < 1000; i++) { buf[i] = (unsigned char)(i * 7 + k); soma = soma * 31 + buf[i]; }
        for (int k2 = 0; k2 < (int)sizeof buf;) {   /* write puede escribir menos (la tubería tiene 512 bytes) */
            int w = write(ida[1], buf + k2, sizeof buf - (size_t)k2);
            if (w <= 0) break;
            k2 += w;
        }
    }
    close(ida[1]);   /* el hijo ve el fin */
    unsigned v[2] = { 0, 0 };
    ler_tudo(volta[0], v, sizeof v);
    close(volta[0]);
    int st;
    waitpid(p, &st, 0);
    ok(v[1] == 100000 && v[0] == soma, "100 KB por una tubería al hijo, y la suma de vuelta (%u bytes)", v[1]);
}

/* ------------------------------------------------------------------ 6. señales */

static volatile int visto_usr1, visto_usr2, visto_chld, visto_alrm, visto_pipe, hilo_usr1;
static volatile int info_pid, info_code;

static void trata_usr1(int s, siginfo_t *si, void *uc)
{
    (void)uc;
    visto_usr1 = s;
    info_pid = si->si_pid;
    info_code = si->si_code;
    hilo_usr1 = thread_id();
}
static void trata_usr2(int s) { visto_usr2 = s; }
static void trata_chld(int s) { (void)s; visto_chld++; }
static void trata_alrm(int s) { (void)s; visto_alrm++; }
static void trata_pipe(int s) { (void)s; visto_pipe++; }

static sigjmp_buf salto;
static volatile int fallo_sinal, fallo_code, fpu_formato;
static volatile unsigned fallo_addr, fallo_eip;

static void trata_fallo(int s, siginfo_t *si, void *uc)
{
    ucontext_t *u = uc;
    fallo_sinal = s;
    fallo_code = si->si_code;
    fallo_addr = (unsigned)si->si_addr;
    fallo_eip = (unsigned)u->uc_mcontext.gregs[REG_EIP];
    siglongjmp(salto, 1);
}

static void instalar(int s, void (*f)(int, siginfo_t *, void *), int flags)
{
    struct sigaction a;
    memset(&a, 0, sizeof a);
    a.sa_sigaction = f;
    a.sa_flags = SA_SIGINFO | flags;
    sigemptyset(&a.sa_mask);
    sigaction(s, &a, NULL);
}

static void instalar_simples(int s, void (*f)(int), int flags)
{
    struct sigaction a;
    memset(&a, 0, sizeof a);
    a.sa_handler = f;
    a.sa_flags = flags;
    sigemptyset(&a.sa_mask);
    sigaction(s, &a, NULL);
}

/* FPU: lo que el programa tenía en xmm0 (y la mitad alta de ymm0 con AVX) y en la x87 sigue ahí
 * después de un tratador que los cambia (el área del XSAVE en el marco, D381). */
static int tem_sse2, tem_avx;
static unsigned char padrao_simd[32] __attribute__((aligned(32)));
static unsigned char lido_simd[32] __attribute__((aligned(32)));

static void detectar_simd(void)
{
    unsigned a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    tem_sse2 = (d >> 26) & 1;
    if (((c >> 27) & 1) && ((c >> 28) & 1)) {   /* OSXSAVE y AVX */
        unsigned lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        tem_avx = (lo & 6) == 6;
    }
}

static void trata_fpu(int s, siginfo_t *si, void *uc)
{
    (void)s; (void)si;
    fpu_formato = (int)((ucontext_t *)uc)->__fpu_formato;
    if (tem_avx) __asm__ volatile("vpxor %%ymm0, %%ymm0, %%ymm0" ::: "memory");
    else if (tem_sse2) __asm__ volatile("pxor %%xmm0, %%xmm0" ::: "memory");
    volatile double x = 3.0;
    for (int i = 0; i < 100; i++) x = x * 1.25 + 0.5;
}

static void p_sinais(void)
{
    parte("senales");
    /* a. tratador con SA_SIGINFO y kill a uno mismo */
    instalar(SIGUSR1, trata_usr1, 0);
    visto_usr1 = 0;
    kill(getpid(), SIGUSR1);
    ok(visto_usr1 == SIGUSR1 && info_pid == getpid() && info_code == SI_USER,
       "kill a uno mismo: el tratador recibe SIGUSR1 con si_pid y SI_USER");

    /* b. máscara */
    instalar_simples(SIGUSR2, trata_usr2, 0);
    sigset_t m, antes, pend;
    sigemptyset(&m);
    sigaddset(&m, SIGUSR2);
    sigprocmask(SIG_BLOCK, &m, &antes);
    visto_usr2 = 0;
    raise(SIGUSR2);
    sigpending(&pend);
    ok(visto_usr2 == 0 && sigismember(&pend, SIGUSR2) == 1, "bloqueada: no se entrega y queda pendiente");
    sigprocmask(SIG_SETMASK, &antes, NULL);
    ok(visto_usr2 == SIGUSR2, "al desbloquearla se entrega");

    /* c. SIGCHLD */
    instalar_simples(SIGCHLD, trata_chld, SA_RESTART);
    visto_chld = 0;
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) _exit(0);
    int st;
    waitpid(p, &st, 0);
    esperar_ms(20);
    ok(visto_chld >= 1, "SIGCHLD al terminar un hijo (%d)", visto_chld);
    signal(SIGCHLD, SIG_DFL);

    /* d. fallos del procesador atrapados con siglongjmp (dos veces: la máscara vuelve) */
    instalar(SIGSEGV, trata_fallo, 0);
    instalar(SIGFPE, trata_fallo, 0);
    instalar(SIGILL, trata_fallo, 0);
    for (int vez = 0; vez < 2; vez++) {
        fallo_sinal = 0;
        if (sigsetjmp(salto, 1) == 0) {
            *(volatile int *)0x10 = 5;
            ok(0, "escribir en 0x10 debería fallar");
        }
        ok(fallo_sinal == SIGSEGV && fallo_addr == 0x10 && fallo_code == SEGV_MAPERR && fallo_eip >= 0x80000000u,
           "SIGSEGV atrapado (vez %d): si_addr %#x, eip %#x", vez + 1, fallo_addr, fallo_eip);
    }
    fallo_sinal = 0;
    volatile int cero = 0, dez = 10;
    if (sigsetjmp(salto, 1) == 0) { volatile int r = dez / cero; (void)r; }
    ok(fallo_sinal == SIGFPE && fallo_code == FPE_INTDIV, "dividir por cero: SIGFPE con FPE_INTDIV");
    fallo_sinal = 0;
    if (sigsetjmp(salto, 1) == 0) __asm__ volatile("ud2");
    ok(fallo_sinal == SIGILL && fallo_code == ILL_ILLOPC, "ud2: SIGILL con ILL_ILLOPC");
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);

    /* e. sin tratador, el fallo termina el proceso y waitpid lo dice */
    fflush(NULL);
    p = fork();
    if (p == 0) { *(volatile int *)0x10 = 1; _exit(0); }
    waitpid(p, &st, 0);
    ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV, "un hijo que falla sin tratador: WTERMSIG = SIGSEGV (%#x)", st);

    /* f. kill a otro proceso */
    fflush(NULL);
    p = fork();
    if (p == 0) { for (;;) pause(); }
    esperar_ms(50);
    kill(p, SIGTERM);
    waitpid(p, &st, 0);
    ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM, "kill(hijo, SIGTERM) lo termina (%#x)", st);

    /* g. SIGPIPE: con tratador, EPIPE y el tratador; por defecto (SIG_DFL), termina */
    instalar_simples(SIGPIPE, trata_pipe, 0);
    int t[2];
    pipe(t);
    close(t[0]);
    visto_pipe = 0;
    errno = 0;
    int w = write(t[1], "x", 1);
    ok(w == -1 && errno == EPIPE && visto_pipe == 1, "escribir sin lector: EPIPE y SIGPIPE");
    close(t[1]);
    fflush(NULL);
    p = fork();
    if (p == 0) {
        signal(SIGPIPE, SIG_DFL);
        int u[2];
        pipe(u);
        close(u[0]);
        write(u[1], "x", 1);
        _exit(0);
    }
    waitpid(p, &st, 0);
    ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGPIPE, "con SIG_DFL, SIGPIPE termina el proceso (%#x)", st);

    /* h. alarm, pause y EINTR / SA_RESTART */
    instalar_simples(SIGALRM, trata_alrm, 0);
    visto_alrm = 0;
    unsigned t0 = agora_ms();
    alarm(1);
    pause();
    unsigned dt = agora_ms() - t0;
    ok(visto_alrm == 1 && dt >= 900 && dt < 2500, "alarm(1) + pause: SIGALRM a los %u ms", dt);
    int vazio[2];
    pipe(vazio);
    alarm(1);
    char c;
    errno = 0;
    int r = read(vazio[0], &c, 1);
    ok(r == -1 && errno == EINTR, "read de una tubería vacía interrumpido por SIGALRM sin SA_RESTART: EINTR");
    instalar_simples(SIGALRM, trata_alrm, SA_RESTART);
    fflush(NULL);
    p = fork();
    if (p == 0) { esperar_ms(1600); write(vazio[1], "R", 1); _exit(0); }
    visto_alrm = 0;
    alarm(1);
    r = read(vazio[0], &c, 1);
    ok(r == 1 && c == 'R' && visto_alrm == 1, "con SA_RESTART el read sigue después del tratador y lee el dato");
    waitpid(p, &st, 0);
    close(vazio[0]);
    close(vazio[1]);
    signal(SIGALRM, SIG_DFL);

    /* i. sigsuspend */
    sigprocmask(SIG_BLOCK, &m, &antes);   /* SIGUSR2 bloqueada */
    fflush(NULL);
    pid_t pai = getpid();
    p = fork();
    if (p == 0) { esperar_ms(100); kill(pai, SIGUSR2); _exit(0); }
    visto_usr2 = 0;
    sigset_t espera = antes;
    sigdelset(&espera, SIGUSR2);
    r = sigsuspend(&espera);
    sigset_t agora;
    sigprocmask(SIG_BLOCK, NULL, &agora);
    ok(r == -1 && errno == EINTR && visto_usr2 == SIGUSR2 && sigismember(&agora, SIGUSR2) == 1,
       "sigsuspend espera la señal y vuelve con la máscara de antes");
    sigprocmask(SIG_SETMASK, &antes, NULL);
    waitpid(p, &st, 0);

    /* j. con hilos: la del proceso va a un hilo que no la bloquea */
    sigset_t so_usr1;
    sigemptyset(&so_usr1);
    sigaddset(&so_usr1, SIGUSR1);
    sigprocmask(SIG_BLOCK, &so_usr1, &antes);
    parar_hilos = 0;
    hilo_usr1 = 0;
    visto_usr1 = 0;
    thread_t *h = thread_create(hilo_desbloqueia, NULL, 0);
    esperar_ms(30);
    kill(getpid(), SIGUSR1);
    for (int i = 0; i < 50 && !visto_usr1; i++) esperar_ms(10);
    ok(visto_usr1 == SIGUSR1 && h && hilo_usr1 == thread_tid(h) && hilo_usr1 != thread_id(),
       "con el principal bloqueándola, SIGUSR1 la atiende el otro hilo (%d)", hilo_usr1);
    parar_hilos = 1;
    if (h) thread_join(h);
    sigprocmask(SIG_SETMASK, &antes, NULL);

    /* k. la FPU sobrevive al tratador */
    detectar_simd();
    for (int i = 0; i < 32; i++) padrao_simd[i] = (unsigned char)(i * 9 + 1);
    instalar(SIGUSR2, trata_fpu, 0);
    volatile double antes_x87 = 1.0 / 3.0, esperado_x87 = 1.0 / 3.0;   /* los dos redondeados a double */
    if (tem_avx) __asm__ volatile("vmovdqu %0, %%ymm0" :: "m"(padrao_simd) : "memory");
    else if (tem_sse2) __asm__ volatile("movdqu %0, %%xmm0" :: "m"(padrao_simd) : "memory");
    raise(SIGUSR2);
    if (tem_avx) __asm__ volatile("vmovdqu %%ymm0, %0" : "=m"(lido_simd) :: "memory");
    else if (tem_sse2) __asm__ volatile("movdqu %%xmm0, %0" : "=m"(lido_simd) :: "memory");
    int n = tem_avx ? 32 : tem_sse2 ? 16 : 0;
    ok(n == 0 || memcmp(padrao_simd, lido_simd, (size_t)n) == 0,
       "%s se conserva tras un tratador que lo borra (formato de la FPU %d)", tem_avx ? "ymm0 (AVX, XSAVE)" : "xmm0",
       fpu_formato);
    ok(antes_x87 == esperado_x87, "la x87 también");
    signal(SIGUSR2, SIG_DFL);
}

/* Ctrl+C por un terminal virtual: SIGINT al grupo en primer plano. */
static void p_ctrl_c(void)
{
    parte("Ctrl+C");
    for (int com_tratador = 1; com_tratador >= 0; com_tratador--) {
        int tv[2], tub[2];
        if (abrir_tv(tv) < 0) { ok(0, "abrir_tv"); return; }
        pipe(tub);
        fflush(NULL);
        pid_t p = fork();
        if (p == 0) {
            definir_grupo(0, 0);
            dup2(tv[1], 0);
            if (com_tratador) instalar_simples(SIGINT, trata_usr2, 0);
            primeiro_plano(getpid());
            visto_usr2 = 0;
            write(tub[1], "L", 1);
            pause();
            write(tub[1], visto_usr2 == SIGINT ? "I" : "?", 1);
            _exit(0);
        }
        char c = 0;
        read(tub[0], &c, 1);
        esperar_ms(50);
        write(tv[0], "\x03", 1);
        int st;
        waitpid(p, &st, 0);
        if (com_tratador) {
            c = 0;
            read(tub[0], &c, 1);
            ok(c == 'I' && WIFEXITED(st) && WEXITSTATUS(st) == 0, "Ctrl+C llega como SIGINT al tratador del programa");
        } else {
            ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGINT, "sin tratador, Ctrl+C lo termina con SIGINT (%#x)", st);
        }
        close(tub[0]); close(tub[1]); close(tv[0]); close(tv[1]);
    }
}

/* ------------------------------------------------------------------ 7. poll y select */

static void p_sondar(void)
{
    parte("poll y select");
    int t[2];
    pipe(t);
    struct pollfd v = { t[0], POLLIN, 0 };
    unsigned t0 = agora_ms();
    int r = poll(&v, 1, 300);
    unsigned dt = agora_ms() - t0;
    ok(r == 0 && dt >= 280 && dt < 1500, "poll sin datos vence el plazo de 300 ms (%u ms)", dt);
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) { esperar_ms(200); write(t[1], "ab", 2); _exit(0); }
    t0 = agora_ms();
    r = poll(&v, 1, 5000);
    dt = agora_ms() - t0;
    ok(r == 1 && (v.revents & POLLIN) && dt < 1500, "poll despierta cuando llegan datos (%u ms)", dt);
    char b[4];
    read(t[0], b, 2);
    int st;
    waitpid(p, &st, 0);

    fd_set l;
    FD_ZERO(&l);
    FD_SET(t[0], &l);
    struct timeval tv = { 0, 200000 };
    t0 = agora_ms();
    r = select(t[0] + 1, &l, NULL, NULL, &tv);
    dt = agora_ms() - t0;
    ok(r == 0 && dt >= 180, "select con plazo de 200 ms y nada que leer (%u ms)", dt);
    write(t[1], "c", 1);
    FD_ZERO(&l);
    FD_SET(t[0], &l);
    fd_set e;
    FD_ZERO(&e);
    FD_SET(t[1], &e);
    tv.tv_sec = 1; tv.tv_usec = 0;
    r = select(t[1] + 1, &l, &e, NULL, &tv);
    ok(r == 2 && FD_ISSET(t[0], &l) && FD_ISSET(t[1], &e), "select: legible y escribible");
    read(t[0], b, 1);

    fcntl(t[0], F_SETFL, O_NONBLOCK);
    errno = 0;
    r = read(t[0], b, 1);
    ok(r == -1 && errno == EAGAIN && (fcntl(t[0], F_GETFL) & O_NONBLOCK), "O_NONBLOCK: leer una tubería vacía da EAGAIN");
    close(t[1]);
    v.revents = 0;
    r = poll(&v, 1, 0);
    ok(r == 1 && (v.revents & POLLHUP), "sin escritores: POLLHUP");
    close(t[0]);
    int u[2];
    pipe(u);
    close(u[0]);
    struct pollfd w = { u[1], POLLOUT, 0 };
    r = poll(&w, 1, 0);
    ok(r == 1 && (w.revents & POLLERR), "sin lectores: POLLERR");
    close(u[1]);
    struct pollfd mal = { 29, POLLIN, 0 };
    r = poll(&mal, 1, 0);
    ok(r == 1 && (mal.revents & POLLNVAL), "un descriptor cerrado: POLLNVAL");

    /* sockets: connect no bloqueante, accept no bloqueante, datos */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(40503);
    a.sin_addr.s_addr = htonl(0x7F000001);
    int lr = bind(s, (struct sockaddr *)&a, sizeof a) == 0 && listen(s, 4) == 0;
    ok(lr, "escuchar en 127.0.0.1:40503");
    fcntl(s, F_SETFL, O_NONBLOCK);
    errno = 0;
    ok(accept(s, NULL, NULL) == -1 && errno == EAGAIN, "accept no bloqueante sin nadie: EAGAIN");
    int c2 = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    errno = 0;
    r = connect(c2, (struct sockaddr *)&a, sizeof a);
    ok(r == 0 || errno == EINPROGRESS, "connect no bloqueante: %s", r == 0 ? "ya conectado" : "EINPROGRESS");
    struct pollfd pc = { c2, POLLOUT, 0 };
    r = poll(&pc, 1, 3000);
    int erro = -1;
    socklen_t n = sizeof erro;
    getsockopt(c2, SOL_SOCKET, SO_ERROR, &erro, &n);
    ok(r == 1 && (pc.revents & POLLOUT) && erro == 0, "poll da POLLOUT al conectar y SO_ERROR = 0");
    struct pollfd ps = { s, POLLIN, 0 };
    r = poll(&ps, 1, 3000);
    int ac = accept(s, NULL, NULL);
    ok(r == 1 && ac >= 0, "poll ve la conexión entrante y accept la toma");
    send(c2, "hola", 4, 0);
    struct pollfd pa = { ac, POLLIN, 0 };
    r = poll(&pa, 1, 3000);
    char rb[8] = "";
    int rr = (int)recv(ac, rb, sizeof rb, 0);
    ok(r == 1 && rr == 4 && memcmp(rb, "hola", 4) == 0, "poll ve los datos del socket y recv los lee");
    fcntl(ac, F_SETFL, O_NONBLOCK);
    errno = 0;
    ok(recv(ac, rb, sizeof rb, 0) == -1 && errno == EAGAIN, "recv no bloqueante sin datos: EAGAIN");
    close(c2);
    r = poll(&pa, 1, 3000);
    ok(r == 1 && (pa.revents & (POLLIN | POLLHUP)), "el otro lado cierra: legible (fin)");
    close(ac);
    close(s);
}

/* ------------------------------------------------------------------ 8. tcc -bt */

/* Con señales, la traza de TCC (CONFIG_TCC_BACKTRACE) funciona para los fallos del procesador
 * (D384 lo dejó pendiente): el tratador de SIGSEGV lee REG_EIP y REG_EBP del ucontext. */
/* ------------------------------------------------------------------ 10. revisión aparte (D401) */

/* Lo que un programa malicioso podría intentar contra el kernel con las piezas nuevas: un contexto
 * falso en sigreturn, una pila de usuario que apunta al kernel al entregar una señal, tratadores
 * con direcciones del kernel, el kernel escribiendo en páginas de copia al escribir. Cada intento
 * va en un hijo: si el kernel lo para, el hijo muere con SIGSEGV; si algo se escapara, el hijo
 * sale con un código distinto de 0 (o el kernel cae y la prueba entera falla). */

static volatile int contexto_hostil_fpu;

static void trata_hostil(int s, siginfo_t *si, void *uc)
{
    (void)s; (void)si;
    ucontext_t *u = uc;
    greg_t *g = u->uc_mcontext.gregs;
    g[REG_CS] = 0x08;                 /* selectores del kernel */
    g[REG_SS] = 0x10;
    g[REG_DS] = g[REG_ES] = g[REG_FS] = g[REG_GS] = 0x10;
    g[REG_EFL] |= 0x3000 | 0x4000 | 0x20000 | 0x80000 | 0x100000;   /* IOPL 3, NT, VM, VIF, VIP */
    unsigned char *fp = u->uc_mcontext.fpregs;
    contexto_hostil_fpu = fp != NULL;
    if (fp) {
        unsigned m = 0xFFFFFFFFu;     /* MXCSR con los bits reservados: FXRSTOR/XRSTOR darían #GP */
        if (u->__fpu_formato >= 2) memcpy(fp + 24, &m, 4);
        if (u->__fpu_formato == 3) {
            memset(fp + 512, 0xFF, 8);    /* XSTATE_BV con todo */
            memset(fp + 520, 0xFF, 8);    /* XCOMP_BV con el bit 63 (formato compacto) */
            memset(fp + 528, 0x5A, 48);   /* lo reservado de la cabecera */
        }
    }
}

static volatile int cli_fallo;
static void trata_cli(int s) { (void)s; cli_fallo = 1; siglongjmp(salto, 1); }

static int hijo_contexto_hostil(void)
{
    /* La FPU en uso, para que el marco lleve su área. */
    volatile double x = 1.5;
    x = x * x;
    if (tem_sse2) __asm__ volatile("pxor %%xmm1, %%xmm1" ::: "memory");
    instalar(SIGUSR1, trata_hostil, 0);
    raise(SIGUSR1);
    /* Sigue aquí: el kernel tomó solo lo que un programa puede poner. */
    unsigned fl, cs, ss, ds, mx = 0;
    __asm__ volatile("pushfl; popl %0" : "=r"(fl));
    __asm__ volatile("movl %%cs, %0; movl %%ss, %1; movl %%ds, %2" : "=r"(cs), "=r"(ss), "=r"(ds));
    if (tem_sse2) __asm__ volatile("stmxcsr %0" : "=m"(mx));
    int r = 0;
    if (fl & 0x3000) r |= 1;           /* IOPL */
    if (fl & (0x4000 | 0x20000 | 0x80000 | 0x100000)) r |= 2;
    if ((cs & 3) != 3 || (ss & 3) != 3 || (ds & 3) != 3) r |= 4;
    if (mx & ~0x0002FFFFu) r |= 8;   /* reservado (el bit 17, MM, existe en AMD: lo admite su MXCSR_MASK) */
    if (!contexto_hostil_fpu) r |= 16;
    /* Con IOPL 0, «cli» sigue dando #GP (SIGSEGV). */
    signal(SIGSEGV, trata_cli);
    if (sigsetjmp(salto, 1) == 0) __asm__ volatile("cli; sti");
    if (!cli_fallo) r |= 32;
    return r;
}

static void trata_fp_mal(int s, siginfo_t *si, void *uc)
{
    (void)s; (void)si;
    ((ucontext_t *)uc)->uc_mcontext.fpregs = (void *)(s == SIGUSR1 ? 0x00100000u : 0x80001001u);
}

/* La señal se entrega con ESP apuntando a "esp": el kernel no puede montar el marco ahí. */
static void hijo_pila_hostil(unsigned esp)
{
    instalar(SIGUSR1, trata_fallo, 0);
    static unsigned guardado;
    __asm__ volatile("movl %%esp, %0\n\t"
                     "movl %1, %%esp\n\t"
                     "movl $143, %%eax\n\t"   /* SIS_SINAL */
                     "movl $3, %%ebx\n\t"     /* SINOP_HILO */
                     "xorl %%ecx, %%ecx\n\t"  /* a mí */
                     "movl $10, %%edx\n\t"    /* SIGUSR1 */
                     "int $0x80\n\t"
                     "movl %0, %%esp"
                     : "=m"(guardado) : "r"(esp) : "eax", "ebx", "ecx", "edx", "memory");
    _exit(3);   /* no debería llegar: el tratador no puede correr con esa pila */
}

static int esperar_estado(pid_t p)
{
    int st = -1;
    while (waitpid(p, &st, 0) < 0 && errno == EINTR) { }
    return st;
}

/* SIGFPE de la x87 (#MF): el tratador usa la x87 sin volver a fallar (empieza con la FPU limpia). */
static volatile int x87_tratador_ok;
static void trata_x87(int s, siginfo_t *si, void *uc)
{
    (void)s; (void)si; (void)uc;
    double r;
    __asm__ volatile("fld1; fld1; faddp; fstpl %0; fwait" : "=m"(r));
    x87_tratador_ok = r == 2.0;
    siglongjmp(salto, 1);
}

static void *hilo_lento(void *arg)
{
    (void)arg;
    sigset_t m;
    sigemptyset(&m);
    sigaddset(&m, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &m, NULL);   /* la toma el hilo que espera en join */
    esperar_ms(300);
    return (void *)5;
}

static int hijo_x87(void)
{
    instalar(SIGFPE, trata_x87, 0);
    if (sigsetjmp(salto, 1) == 0) {
        unsigned short cw = 0x037B;   /* la división por cero sin enmascarar */
        static const double cero = 0.0;
        __asm__ volatile("fldcw %0; fld1; fdivl %1; fwait" : : "m"(cw), "m"(cero));
        return 1;   /* no falló */
    }
    return x87_tratador_ok ? 0 : 2;
}

/* sigaltstack: el desbordamiento de la pila solo se atrapa en la pila alternativa. */
static unsigned char *pila_alt;
static volatile unsigned onde_tratador;
static volatile int alt_em_uso;

static void trata_estouro(int s, siginfo_t *si, void *uc)
{
    (void)s; (void)si; (void)uc;
    volatile int local;
    onde_tratador = (unsigned)&local;
    stack_t v;
    if (sigaltstack(NULL, &v) == 0) alt_em_uso = (v.ss_flags & SS_ONSTACK) != 0;
    siglongjmp(salto, 1);
}

static int profundo(int n)
{
    volatile char b[4000];
    b[0] = (char)n;
    b[3999] = (char)n;
    return profundo(n + 1) + b[0] + b[3999];
}

static int hijo_altstack(void)
{
    pila_alt = malloc(SIGSTKSZ * 4);
    stack_t a;
    a.ss_sp = pila_alt;
    a.ss_size = SIGSTKSZ * 4;
    a.ss_flags = 0;
    if (sigaltstack(&a, NULL) != 0) return 1;
    stack_t pequena = { pila_alt, 0, 100 };
    if (sigaltstack(&pequena, NULL) == 0 || errno != ENOMEM) return 2;   /* menos de MINSIGSTKSZ */
    instalar(SIGSEGV, trata_estouro, SA_ONSTACK);
    if (sigsetjmp(salto, 1) == 0) { profundo(0); return 3; }
    if (!(onde_tratador >= (unsigned)pila_alt && onde_tratador < (unsigned)pila_alt + SIGSTKSZ * 4)) return 4;
    if (!alt_em_uso) return 5;
    stack_t v;
    if (sigaltstack(NULL, &v) != 0 || (v.ss_flags & SS_ONSTACK)) return 6;   /* fuera: ya no en uso */
    return 0;
}

/* El kernel escribe (read, waitpid, poll) en páginas de copia al escribir del hijo: el padre no
 * ve nada de eso. */
static char pagina_cow[3 * 4096] __attribute__((aligned(4096)));

static void p_auditoria(void)
{
    parte("revision (D401)");
    int st;
    /* a. sigreturn con un contexto falso */
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) _exit(hijo_contexto_hostil());
    st = esperar_estado(p);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0,
       "sigreturn con selectores del kernel, IOPL 3, NT, VM y una FPU con basura: el kernel solo toma lo del programa (%#x)", st);

    /* b. fpregs que apunta al kernel o desalineado: el proceso muere, el kernel sigue */
    for (int vez = 0; vez < 2; vez++) {
        fflush(NULL);
        p = fork();
        if (p == 0) {
            volatile double x = 2.0;
            x = x * x;
            instalar(vez ? SIGUSR2 : SIGUSR1, trata_fp_mal, 0);
            raise(vez ? SIGUSR2 : SIGUSR1);
            _exit(x == 4.0 ? 9 : 8);
        }
        st = esperar_estado(p);
        ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV, "sigreturn con fpregs %s: SIGSEGV (%#x)",
           vez ? "desalineado" : "en el kernel", st);
    }

    /* c. pila del programa en el kernel, por encima de lo del programa o dando la vuelta */
    static const unsigned pilas[] = { 0x00100000u, 0x80000100u, 0xFFFFFFF0u, 0xFFC00800u, 0x10u };
    for (unsigned i = 0; i < sizeof pilas / sizeof pilas[0]; i++) {
        fflush(NULL);
        p = fork();
        if (p == 0) hijo_pila_hostil(pilas[i]);
        st = esperar_estado(p);
        ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV, "entregar una señal con ESP = %#x: el proceso muere con SIGSEGV (%#x)",
           pilas[i], st);
    }

    /* d. tratadores con direcciones del kernel: se rechazan */
    struct sigaction a;
    memset(&a, 0, sizeof a);
    a.sa_handler = (sighandler_t)0x00100000u;
    ok(sigaction(SIGUSR1, &a, NULL) == -1 && errno == EINVAL, "sigaction con un tratador en el kernel: EINVAL");
    ok(sigaction(SIGKILL, &a, NULL) == -1 && errno == EINVAL, "SIGKILL no se atrapa");

    /* e. un tratador con SA_NODEFER que vuelve a fallar sin fin: acaba en SIGSEGV, sin colgar */
    fflush(NULL);
    p = fork();
    if (p == 0) {
        memset(&a, 0, sizeof a);
        a.sa_handler = (sighandler_t)0xFF000000u;   /* del programa pero sin nada: falla al entrar */
        a.sa_flags = SA_NODEFER;
        sigaction(SIGSEGV, &a, NULL);
        *(volatile int *)0x20 = 1;
        _exit(4);
    }
    st = esperar_estado(p);
    ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV, "tratador que falla con SA_NODEFER: termina con SIGSEGV (%#x)", st);

    /* f. SIGFPE de la x87 con un tratador que usa la x87 */
    fflush(NULL);
    p = fork();
    if (p == 0) _exit(hijo_x87());
    st = esperar_estado(p);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0, "SIGFPE de la x87: el tratador empieza con la FPU limpia (%#x)", st);

    /* g. sigaltstack */
    fflush(NULL);
    p = fork();
    if (p == 0) _exit(hijo_altstack());
    st = esperar_estado(p);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0, "sigaltstack: el desbordamiento de la pila se atrapa en la alternativa (%#x)", st);

    /* h. el kernel escribiendo en las páginas de copia al escribir del hijo */
    memset(pagina_cow, 'P', sizeof pagina_cow);
    int t[2];
    pipe(t);
    fflush(NULL);
    p = fork();
    if (p == 0) {
        write(t[1], "HHHH", 4);
        int r = read(t[0], pagina_cow + 4096 - 2, 4);   /* cruza dos páginas compartidas */
        int *estado = (int *)(pagina_cow + 2 * 4096 + 8);
        pid_t n = fork();
        if (n == 0) _exit(77);
        waitpid(n, estado, 0);                           /* el kernel escribe el estado */
        struct pollfd *pf = (struct pollfd *)(pagina_cow + 2 * 4096 + 64);
        pf->fd = t[1]; pf->events = POLLOUT; pf->revents = 0;
        poll(pf, 1, 0);
        _exit(r == 4 && memcmp(pagina_cow + 4094, "HHHH", 4) == 0 && WEXITSTATUS(*estado) == 77 &&
              (pf->revents & POLLOUT) ? 0 : 1);
    }
    st = esperar_estado(p);
    int pai_igual = 1;
    for (unsigned i = 0; i < sizeof pagina_cow; i++) if (pagina_cow[i] != 'P') pai_igual = 0;
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0 && pai_igual,
       "read, waitpid y poll del hijo escriben en sus copias; el padre no ve nada (%#x, %d)", st, pai_igual);
    close(t[0]); close(t[1]);

    /* i. getpgrp/getpgid de verdad */
    fflush(NULL);
    p = fork();
    if (p == 0) {
        setpgid(0, 0);
        _exit(getpgrp() == getpid() && getpgid(0) == getpid() ? 0 : 1);
    }
    st = esperar_estado(p);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0 && getpgrp() == getpgid(getpid()),
       "getpgrp y getpgid dan el grupo (el mío %d)", getpgrp());

    /* j. SIGCHLD ignorada: los hijos no quedan zombis (más fork que entradas tiene la tabla) */
    unsigned l0 = livre_kb();
    signal(SIGCHLD, SIG_IGN);
    int hechos = 0;
    for (int i = 0; i < 150; i++) {
        fflush(NULL);
        p = fork();
        if (p == 0) _exit(0);
        if (p < 0) { esperar_ms(20); continue; }
        hechos++;
        if ((i & 15) == 15) esperar_ms(5);
    }
    esperar_ms(200);
    int w = waitpid(-1, &st, WNOHANG);
    signal(SIGCHLD, SIG_DFL);
    ok(hechos >= 140 && w == -1 && errno == ECHILD, "SIGCHLD ignorada: %d hijos, ninguno queda zombi", hechos);

    /* k. nietos huérfanos (el hijo sale antes que el nieto) y un hijo con hilos matado con SIGKILL:
     * la memoria vuelve entera */
    for (int i = 0; i < 80; i++) {
        fflush(NULL);
        p = fork();
        if (p == 0) {
            if (fork() == 0) { esperar_ms(5); _exit(0); }
            _exit(0);
        }
        esperar_estado(p);
    }
    for (int i = 0; i < 20; i++) {
        fflush(NULL);
        p = fork();
        if (p == 0) {
            memset(pagina_cow, i, sizeof pagina_cow);
            for (int k = 0; k < 3; k++) thread_create(hilo_contar, (void *)k, 0);
            for (;;) pause();
        }
        esperar_ms(10);
        kill(p, SIGKILL);
        st = esperar_estado(p);
        if (!(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL)) break;
    }
    esperar_ms(300);
    unsigned l1 = livre_kb();
    ok(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL && (int)l0 - (int)l1 < 256,
       "nietos huérfanos e hijos con hilos matados: sin perder memoria (%d KB)", (int)l0 - (int)l1);

    /* m. execve desde un proceso con hilos: el programa nuevo crea los suyos, cada uno con su pila */
    fflush(NULL);
    p = fork();
    if (p == 0) {
        parar_hilos = 0;
        for (int k = 0; k < 3; k++) thread_create(hilo_contar, (void *)k, 0);
        esperar_ms(20);
        char *argv[] = { "posix", "hilosnovos", NULL };
        execv("/testes/posix", argv);
        _exit(99);
    }
    st = esperar_estado(p);
    ok(WIFEXITED(st) && WEXITSTATUS(st) == 0, "execve con hilos: los hilos del programa nuevo no comparten pila (%#x)", st);

    /* l. join no da EINTR con una señal sin SA_RESTART (POSIX: pthread_join nunca la da) */
    instalar_simples(SIGALRM, trata_alrm, 0);
    visto_alrm = 0;
    thread_t *h = thread_create(hilo_lento, 0, 0);
    sinal_op(SINOP_ALARME, 100, 0, 0);   /* a los 100 ms, mientras join espera */
    int jr = h ? thread_join(h) : -1;
    ok(jr == 5 && visto_alrm == 1, "join interrumpido por SIGALRM sin SA_RESTART sigue esperando (%d, %d)", jr, visto_alrm);
    signal(SIGALRM, SIG_DFL);
}

static void p_tcc_bt(void)
{
    parte("tcc -bt");
    if (access("/apps/bin/tcc", X_OK) != 0) { printf("POSIX sin /apps/bin/tcc: se salta\n"); return; }
    FILE *f = fopen("/tmp/bt.c", "w");
    if (!f) { ok(0, "escribir /tmp/bt.c"); return; }
    fputs("#include <stdio.h>\n"
          "int *nulo;\n"
          "int falla(int x)\n"
          "{\n"
          "    return nulo[x];\n"
          "}\n"
          "int main(void)\n"
          "{\n"
          "    printf(\"antes\\n\");\n"
          "    fflush(stdout);\n"
          "    return falla(3);\n"
          "}\n", f);
    fclose(f);
    for (int run = 0; run < 2; run++) {
        const char *cmd = run ? "/apps/bin/tcc -bt -run /tmp/bt.c 2>&1"
                              : "/apps/bin/tcc -bt /tmp/bt.c -o /tmp/bt 2>&1 && /tmp/bt 2>&1";
        FILE *p = popen(cmd, "r");
        char saida[1024];
        size_t n = p ? fread(saida, 1, sizeof saida - 1, p) : 0;
        saida[n] = 0;
        int st = p ? pclose(p) : -1;
        for (char *c = saida; *c; c++) if (*c == '\n' && c[1]) *c = '|';
        printf("POSIX tcc%s: %s\n", run ? " -run" : "", saida);
        ok(strstr(saida, "antes") && strstr(saida, "bt.c:5") && strstr(saida, "invalid memory access") &&
           strstr(saida, "main"), "tcc -bt%s: la traza dice el fichero, la línea y main (estado %#x)",
           run ? " -run" : "", st);
    }
}

/* Tras un execve desde un proceso con hilos: los hilos nuevos tienen cada uno su hueco de pila
 * (los viejos, ya muertos, no sueltan los huecos de los nuevos al recogerlos). */
static volatile unsigned pilha_de[2];
static volatile int hilos_novos_parar;
static void *hilo_novo(void *arg)
{
    volatile int local = 0;
    pilha_de[(int)arg] = (unsigned)&local;
    while (!hilos_novos_parar) dormir(2);
    return (void *)local;
}

static int modo_hilos_novos(void)
{
    thread_t *a = thread_create(hilo_novo, (void *)0, 0);   /* antes de que nadie recoja los hilos viejos */
    dormir(30);                                              /* escalonar() recoge los zombis del exec */
    thread_t *b = thread_create(hilo_novo, (void *)1, 0);
    dormir(30);
    hilos_novos_parar = 1;
    if (a) thread_join(a);
    if (b) thread_join(b);
    unsigned d = pilha_de[0] > pilha_de[1] ? pilha_de[0] - pilha_de[1] : pilha_de[1] - pilha_de[0];
    return a && b && pilha_de[0] && pilha_de[1] && d >= 512 * 1024 ? 0 : 1;
}

int principal(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "exec") == 0) return modo_exec(argc, argv);
    if (argc >= 2 && strcmp(argv[1], "hilosnovos") == 0) return modo_hilos_novos();
    printf("POSIX inicio (pid %d)\n", getpid());
    fflush(stdout);
    p_fork_exec();
    p_cow();
    p_muchos();
    p_hilos();
    p_tuberias();
    p_sinais();
    p_ctrl_c();
    p_sondar();
    p_auditoria();
    p_tcc_bt();
    printf("RESULTADO posix: %s (%d falhas)\n", falhas ? "FALHOU" : "SUCESSO", falhas);
    fflush(stdout);
    return falhas;
}
