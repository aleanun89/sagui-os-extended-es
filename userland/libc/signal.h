/* signal.h - señales POSIX (D184; de verdad desde la D401).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en userland/libc/sinais.c.
 * Los números, siginfo_t (128 bytes) y ucontext_t son los de Linux i386
 * (el kernel los deja así en la pila al llamar a un tratador, D401). */
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
#define _NSIG   NSIG
/* Banderas de sigaction (las de Linux). */
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO   0x00000004
#define SA_RESTORER  0x04000000
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000
#define SA_NOMASK    SA_NODEFER
#define SA_ONESHOT   SA_RESETHAND
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
/* si_code */
#define SI_USER     0
#define SI_KERNEL   0x80
#define SEGV_MAPERR 1
#define SEGV_ACCERR 2
#define BUS_ADRALN  1
#define BUS_ADRERR  2
#define ILL_ILLOPC  1
#define TRAP_BRKPT  1
#define FPE_INTDIV  1
#define FPE_INTOVF  2
#define FPE_FLTDIV  3
#define FPE_FLTOVF  4
#define FPE_FLTUND  5
#define FPE_FLTRES  6
#define FPE_FLTINV  7
#define CLD_EXITED  1
#define CLD_KILLED  2
#define CLD_DUMPED  3

typedef struct {
    int si_signo, si_errno, si_code;
    union {
        struct { pid_t pid; uid_t uid; int status; } _proc;
        void *addr;
        int _pad[29];
    } _u;
} siginfo_t;   /* 128 bytes, como en Linux */
#define si_pid    _u._proc.pid
#define si_uid    _u._proc.uid
#define si_status _u._proc.status
#define si_addr   _u.addr

struct sigaction {
    union {
        sighandler_t sa_handler;
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);   /* lo pone la libc */
};

/* sigaltstack */
typedef struct {
    void  *ss_sp;
    int    ss_flags;
    size_t ss_size;
} stack_t;
#define SS_ONSTACK   1
#define SS_DISABLE   2
#define MINSIGSTKSZ  2048
#define SIGSTKSZ     8192

sighandler_t signal(int sinal, sighandler_t tratador);
sighandler_t bsd_signal(int sinal, sighandler_t tratador);
int raise(int sinal);
int kill(pid_t pid, int sinal);
int killpg(pid_t grupo, int sinal);
int sigaction(int sinal, const struct sigaction *a, struct sigaction *antiga);
int sigemptyset(sigset_t *s);
int sigfillset(sigset_t *s);
int sigaddset(sigset_t *s, int sinal);
int sigdelset(sigset_t *s, int sinal);
int sigismember(const sigset_t *s, int sinal);
int sigprocmask(int como, const sigset_t *s, sigset_t *antigo);
int pthread_sigmask(int como, const sigset_t *s, sigset_t *antigo);
int sigsuspend(const sigset_t *mascara);
int sigpending(sigset_t *s);
int sigaltstack(const stack_t *nova, stack_t *antiga);
int siginterrupt(int sinal, int sim);
char *strsignal(int sinal);
void psignal(int sinal, const char *s);
extern const char *const sys_siglist[NSIG];
#endif
