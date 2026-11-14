/* signal.h - señales (no las hay en el SaguiOS) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SIGNAL_H
#define NS_SIGNAL_H

#include <sys/types.h>
typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);
typedef unsigned long sigset_t;
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)
/* Los números de Linux. */
#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGABRT  6
#define SIGIOT   6
#define SIGBUS   7
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG  23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO   29
#define SIGPOLL 29
#define SIGPWR  30
#define SIGSYS  31
#define NSIG    32
#define SA_SIGINFO 4
#define SA_RESTART 0x10000000
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
typedef struct { int si_signo, si_code; void *si_addr; } siginfo_t;
struct sigaction {
    sighandler_t sa_handler;
    void (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    int sa_flags;
};
/* El SaguiOS no tiene señales: signal() y sigaction() solo recuerdan el
 * tratador, que nunca llama nadie más que raise() (carencia anotada); las
 * máscaras se guardan y no hacen nada. kill() solo sabe terminar un proceso
 * (la señal 0 pregunta si existe). */
sighandler_t signal(int sinal, sighandler_t tratador);
int raise(int sinal);
int kill(pid_t pid, int sinal);
int sigaction(int sinal, const struct sigaction *a, struct sigaction *antiga);
int sigemptyset(sigset_t *s);
int sigfillset(sigset_t *s);
int sigaddset(sigset_t *s, int sinal);
int sigdelset(sigset_t *s, int sinal);
int sigismember(const sigset_t *s, int sinal);
int sigprocmask(int como, const sigset_t *s, sigset_t *antigo);
#endif
