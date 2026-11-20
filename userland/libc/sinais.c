/* sinais.c - procesos y señales POSIX de la libc común, sobre las llamadas del kernel (D401):
 * fork, exec*, waitpid, wait, system, getppid, alarm, pause, las señales (signal, sigaction,
 * raise, kill, sigprocmask, sigsuspend, sigpending, sigaltstack, sigsetjmp/siglongjmp), poll y
 * select.
 *
 * Parte de la libc común (userland/libc, D210). Un tratador lo llama el kernel con el marco de
 * Linux i386 en la pila; al volver, «ret» salta a __sinal_restaurar, que pide al kernel que
 * restaure el contexto (SIS_SINAL_VOLTAR). Por eso sigaction pone siempre SA_RESTORER. */
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
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sagui.h"

extern char **environ;

static int falha(int r)
{
    errno = __ns_errno_de(r);
    return -1;
}

/* La vuelta de un tratador: al hacer «ret», esp apunta a [señal][&info][&contexto]. */
void __sinal_restaurar(void);
__asm__(".text\n"
        ".globl __sinal_restaurar\n"
        ".type __sinal_restaurar, @function\n"
        "__sinal_restaurar:\n"
        "    movl 8(%esp), %ebx\n"
        "    movl $144, %eax\n"          /* SIS_SINAL_VOLTAR */
        "    int $0x80\n"
        "    hlt\n");                    /* no vuelve; si volviera, #GP: SIGSEGV */

/* ------------------------------------------------------------- procesos */

pid_t fork(void)
{
    int r = bifurcar();
    return r < 0 ? falha(r) : r;
}

/* vfork es fork: con copia al escribir cuesta poco (D401). */
pid_t vfork(void)
{
    int r = bifurcar();
    return r < 0 ? falha(r) : r;
}

int execve(const char *caminho, char *const argv[], char *const envp[])
{
    int r = substituir(caminho, argv, envp);
    return falha(r);   /* solo vuelve si falló */
}

int execv(const char *caminho, char *const argv[])
{
    return execve(caminho, argv, environ);
}

/* Busca en PATH (o /bin si no hay) si el nombre no tiene «/». */
int execvpe(const char *arq, char *const argv[], char *const envp[])
{
    if (!arq || !*arq) { errno = ENOENT; return -1; }
    if (strchr(arq, '/')) return execve(arq, argv, envp);
    const char *path = getenv("PATH");
    if (!path || !*path) path = "/bin";
    char c[256];
    int erro = ENOENT;
    while (*path) {
        const char *fim = strchr(path, ':');
        size_t n = fim ? (size_t)(fim - path) : strlen(path);
        if (n + 1 + strlen(arq) + 1 <= sizeof c) {
            if (n) { memcpy(c, path, n); c[n++] = '/'; }
            strcpy(c + n, arq);
            execve(c, argv, envp);
            if (errno != ENOENT) erro = errno;   /* p. ej. EACCES: se recuerda, como glibc */
        }
        path = fim ? fim + 1 : path + strlen(path);
    }
    errno = erro;
    return -1;
}

int execvp(const char *arq, char *const argv[])
{
    return execvpe(arq, argv, environ);
}

#define EXECL_MAX 64

static int juntar_args(const char *arg, va_list ap, char **v, char ***envp)
{
    int n = 0;
    v[n++] = (char *)arg;
    while (arg) {
        if (n >= EXECL_MAX) { errno = E2BIG; return -1; }
        arg = va_arg(ap, const char *);
        v[n++] = (char *)arg;
    }
    if (envp) *envp = va_arg(ap, char **);
    return 0;
}

int execl(const char *caminho, const char *arg, ...)
{
    char *v[EXECL_MAX + 1];
    va_list ap;
    va_start(ap, arg);
    int r = juntar_args(arg, ap, v, NULL);
    va_end(ap);
    return r < 0 ? -1 : execv(caminho, v);
}

int execlp(const char *arq, const char *arg, ...)
{
    char *v[EXECL_MAX + 1];
    va_list ap;
    va_start(ap, arg);
    int r = juntar_args(arg, ap, v, NULL);
    va_end(ap);
    return r < 0 ? -1 : execvp(arq, v);
}

int execle(const char *caminho, const char *arg, ...)
{
    char *v[EXECL_MAX + 1];
    char **envp = NULL;
    va_list ap;
    va_start(ap, arg);
    int r = juntar_args(arg, ap, v, &envp);
    va_end(ap);
    return r < 0 ? -1 : execve(caminho, v, envp);
}

pid_t waitpid(pid_t pid, int *estado, int opcoes)
{
    for (;;) {
        int r = esperar_pid(pid, estado, opcoes & WNOHANG ? ESPERA_SEM_BLOQUEAR : 0);
        if (r == ERRO_SEM_PROCESSO) { errno = ECHILD; return -1; }
        return r < 0 ? falha(r) : r;
    }
}

pid_t wait(int *estado) { return waitpid(-1, estado, 0); }

pid_t getppid(void) { return sinal_op(SINOP_PAI, 0, 0, 0); }

pid_t setsid(void)
{
    int r = definir_grupo(0, 0);
    return r < 0 ? falha(r) : getpid();
}

pid_t getpgid(pid_t pid)
{
    int r = sinal_op(SINOP_GRUPO, (unsigned)pid, 0, 0);
    return r < 0 ? falha(r) : r;
}

pid_t getpgrp(void) { return getpgid(0); }

int setpgid(pid_t pid, pid_t grupo)
{
    int r = definir_grupo(pid, grupo);
    return r < 0 ? falha(r) : 0;
}

int pipe2(int fds[2], int flags)
{
    int r = criar_pipe(fds);
    if (r < 0) return falha(r);
    for (int i = 0; i < 2; i++) {
        if (flags & O_CLOEXEC) descritor_op(fds[i], DESC_FIXAR_FD, DESC_FD_FECHAR_EXEC);
        if (flags & O_NONBLOCK) descritor_op(fds[i], DESC_FIXAR_FL, DESC_FL_NAO_BLOQUEAR);
    }
    return 0;
}

/* system: /bin/sh -c con fork + exec, SIGINT y SIGQUIT ignoradas y SIGCHLD bloqueada mientras
 * espera, como pide POSIX. */
int system(const char *cmd)
{
    if (!cmd) return 1;
    fflush(NULL);
    struct sigaction ign, int_antes, quit_antes;
    sigset_t bloq, antes;
    memset(&ign, 0, sizeof ign);
    ign.sa_handler = SIG_IGN;
    sigaction(SIGINT, &ign, &int_antes);
    sigaction(SIGQUIT, &ign, &quit_antes);
    sigemptyset(&bloq);
    sigaddset(&bloq, SIGCHLD);
    sigprocmask(SIG_BLOCK, &bloq, &antes);
    int st = -1;
    pid_t p = fork();
    if (p == 0) {
        sigaction(SIGINT, &int_antes, NULL);
        sigaction(SIGQUIT, &quit_antes, NULL);
        sigprocmask(SIG_SETMASK, &antes, NULL);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    if (p > 0) {
        while (waitpid(p, &st, 0) < 0 && errno == EINTR) { }
    }
    sigaction(SIGINT, &int_antes, NULL);
    sigaction(SIGQUIT, &quit_antes, NULL);
    sigprocmask(SIG_SETMASK, &antes, NULL);
    return p < 0 ? -1 : st;
}

unsigned alarm(unsigned segundos)
{
    int r = sinal_op(SINOP_ALARME, segundos > 0x7FFFFFFFu / 1000 ? 0x7FFFFFFFu : segundos * 1000u, 0, 0);
    return r <= 0 ? 0 : (unsigned)(r + 999) / 1000u;
}

int pause(void)
{
    sigset_t m;
    sigprocmask(SIG_BLOCK, NULL, &m);
    return sigsuspend(&m);
}

/* ------------------------------------------------------------- señales */

int sigaction(int sinal, const struct sigaction *a, struct sigaction *antiga)
{
    acao_sinal_t nova, velha;
    if (a) {
        nova.tratador = (unsigned)a->sa_handler;
        nova.mascara = (unsigned)a->sa_mask;
        nova.flags = (unsigned)a->sa_flags | SA_RESTORER;
        nova.restaurador = (unsigned)__sinal_restaurar;
    }
    int r = sinal_op(SINOP_ACAO, (unsigned)sinal, a ? (unsigned)&nova : 0, antiga ? (unsigned)&velha : 0);
    if (r < 0) return falha(r);
    if (antiga) {
        memset(antiga, 0, sizeof *antiga);
        antiga->sa_handler = (sighandler_t)velha.tratador;
        antiga->sa_mask = velha.mascara;
        antiga->sa_flags = (int)(velha.flags & ~(unsigned)SA_RESTORER);
        antiga->sa_restorer = (void (*)(void))velha.restaurador;
    }
    return 0;
}

/* signal(): semántica BSD, como glibc (SA_RESTART y el tratador se queda). */
sighandler_t signal(int sinal, sighandler_t tratador)
{
    struct sigaction a, antes;
    memset(&a, 0, sizeof a);
    a.sa_handler = tratador;
    a.sa_flags = SA_RESTART;
    if (sigaction(sinal, &a, &antes) < 0) return SIG_ERR;
    return antes.sa_handler;
}

sighandler_t bsd_signal(int sinal, sighandler_t tratador) { return signal(sinal, tratador); }

int siginterrupt(int sinal, int sim)
{
    struct sigaction a;
    if (sigaction(sinal, NULL, &a) < 0) return -1;
    if (sim) a.sa_flags &= ~SA_RESTART; else a.sa_flags |= SA_RESTART;
    return sigaction(sinal, &a, NULL);
}

/* raise: al propio hilo; el kernel la entrega al volver de esta misma llamada. */
int raise(int sinal)
{
    int r = sinal_op(SINOP_HILO, 0, (unsigned)sinal, 0);
    return r < 0 ? falha(r) : 0;
}

int kill(pid_t pid, int sinal)
{
    int r = sinal_op(SINOP_ENVIAR, (unsigned)pid, (unsigned)sinal, 0);
    if (r == ERRO_ACESSO) { errno = EPERM; return -1; }
    return r < 0 ? falha(r) : 0;
}

int killpg(pid_t grupo, int sinal)
{
    if (grupo <= 1) { errno = EINVAL; return -1; }
    return kill(-grupo, sinal);
}

int sigemptyset(sigset_t *s) { *s = 0; return 0; }
int sigfillset(sigset_t *s) { *s = ~0ul; return 0; }

int sigaddset(sigset_t *s, int sinal)
{
    if (sinal <= 0 || sinal >= NSIG) { errno = EINVAL; return -1; }
    *s |= 1ul << sinal;
    return 0;
}

int sigdelset(sigset_t *s, int sinal)
{
    if (sinal <= 0 || sinal >= NSIG) { errno = EINVAL; return -1; }
    *s &= ~(1ul << sinal);
    return 0;
}

int sigismember(const sigset_t *s, int sinal)
{
    if (sinal <= 0 || sinal >= NSIG) { errno = EINVAL; return -1; }
    return (int)((*s >> sinal) & 1);
}

int sigprocmask(int como, const sigset_t *s, sigset_t *antigo)
{
    int r = sinal_op(SINOP_MASCARA, (unsigned)como, (unsigned)s, (unsigned)antigo);
    return r < 0 ? falha(r) : 0;
}

int pthread_sigmask(int como, const sigset_t *s, sigset_t *antigo)
{
    int r = sinal_op(SINOP_MASCARA, (unsigned)como, (unsigned)s, (unsigned)antigo);
    return r < 0 ? __ns_errno_de(r) : 0;
}

int sigsuspend(const sigset_t *mascara)
{
    int r = sinal_op(SINOP_SUSPENDER, (unsigned)mascara, 0, 0);
    return falha(r < 0 ? r : ERRO_INTERROMPIDO);
}

int sigpending(sigset_t *s)
{
    int r = sinal_op(SINOP_PENDENTES, (unsigned)s, 0, 0);
    return r < 0 ? falha(r) : 0;
}

int sigaltstack(const stack_t *nova, stack_t *antiga)
{
    pilha_sinal_t n, v;
    if (nova) {
        n.base = (unsigned)nova->ss_sp;
        n.flags = (unsigned)nova->ss_flags;
        n.tamanho = (unsigned)nova->ss_size;
    }
    int r = sinal_op(SINOP_PILHA, nova ? (unsigned)&n : 0, antiga ? (unsigned)&v : 0, 0);
    if (r == ERRO_ACESSO) { errno = EPERM; return -1; }
    if (r < 0) return falha(r);
    if (antiga) {
        antiga->ss_sp = (void *)v.base;
        antiga->ss_flags = (int)v.flags;
        antiga->ss_size = v.tamanho;
    }
    return 0;
}

void __sigjmp_salvar(sigjmp_buf b, int salvar)
{
    b->salvo = salvar;
    if (salvar) sigprocmask(SIG_BLOCK, NULL, (sigset_t *)&b->mascara);
}

void siglongjmp(sigjmp_buf b, int v)
{
    if (b->salvo) sigprocmask(SIG_SETMASK, (const sigset_t *)&b->mascara, NULL);
    longjmp(b->b, v);
}

const char *const sys_siglist[NSIG] = {
    "Señal 0", "Colgado", "Interrumpido", "Salir", "Instrucción no válida", "Trampa de depuración",
    "Abortado", "Error de bus", "Excepción de coma flotante", "Matado", "Señal de usuario 1",
    "Violación de segmento", "Señal de usuario 2", "Tubería rota", "Alarma", "Terminado",
    "Fallo de la pila del coprocesador", "Hijo terminado", "Continuar", "Parado (señal)", "Parado",
    "Parado (entrada de terminal)", "Parado (salida de terminal)", "Condición urgente de E/S",
    "Límite de CPU superado", "Límite de tamaño de fichero superado", "Alarma virtual",
    "Alarma de perfilado", "Ventana cambiada", "E/S posible", "Fallo de alimentación", "Llamada al sistema errónea",
};

char *strsignal(int sinal)
{
    static char b[32];
    if (sinal > 0 && sinal < NSIG) return (char *)sys_siglist[sinal];
    snprintf(b, sizeof b, "Señal desconocida %d", sinal);
    return b;
}

void psignal(int sinal, const char *s)
{
    if (s && *s) fprintf(stderr, "%s: %s\n", s, strsignal(sinal));
    else fprintf(stderr, "%s\n", strsignal(sinal));
}

/* ------------------------------------------------------------- poll y select */

/* Débiles: un port que trae su propio poll/select (openrsync, D314) se queda con el suyo. */
__attribute__((weak)) int poll(struct pollfd *fds, nfds_t n, int ms)
{
    /* struct pollfd y sondagem_t son iguales (fd, events, revents). */
    int r = sondar((sondagem_t *)fds, n, ms);
    return r < 0 ? falha(r) : r;
}

__attribute__((weak)) int select(int n, fd_set *l, fd_set *e, fd_set *x, struct timeval *prazo)
{
    struct pollfd v[FD_SETSIZE];
    int k = 0;
    if (n > FD_SETSIZE) n = FD_SETSIZE;
    for (int fd = 0; fd < n; fd++) {
        short ev = 0;
        if (l && FD_ISSET(fd, l)) ev |= POLLIN;
        if (e && FD_ISSET(fd, e)) ev |= POLLOUT;
        if (x && FD_ISSET(fd, x)) ev |= POLLPRI;
        if (!ev) continue;
        v[k].fd = fd;
        v[k].events = ev;
        v[k].revents = 0;
        k++;
    }
    int ms = -1;
    if (prazo) {
        long t = prazo->tv_sec * 1000L + (prazo->tv_usec + 999) / 1000;
        ms = t < 0 ? 0 : t > 0x7FFFFFFFL ? 0x7FFFFFFF : (int)t;
    }
    int r = poll(v, (nfds_t)k, ms);
    if (r < 0) return -1;
    for (int i = 0; i < k; i++)
        if (v[i].revents & POLLNVAL) { errno = EBADF; return -1; }
    if (l) FD_ZERO(l);
    if (e) FD_ZERO(e);
    if (x) FD_ZERO(x);
    int listos = 0;
    for (int i = 0; i < k; i++) {
        short rv = v[i].revents;
        /* Como Linux: fin y error cuentan como «se puede leer» (la lectura no se bloquea). */
        if (l && (v[i].events & POLLIN) && (rv & (POLLIN | POLLHUP | POLLERR))) { FD_SET(v[i].fd, l); listos++; }
        if (e && (v[i].events & POLLOUT) && (rv & (POLLOUT | POLLERR))) { FD_SET(v[i].fd, e); listos++; }
        if (x && (v[i].events & POLLPRI) && (rv & POLLPRI)) { FD_SET(v[i].fd, x); listos++; }
    }
    return listos;
}
