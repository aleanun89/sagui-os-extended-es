/* sys/ucontext.h - el contexto que el kernel deja al llamar a un tratador de señal (D401).
 *
 * Parte de la libc común (userland/libc). Es el ucontext_t de Linux i386 (el orden de los
 * registros de gregs es el de Linux: REG_EIP, REG_EBP... los usa TCC para «-bt»), seguido de dos
 * campos de SaguiOS. uc_mcontext.fpregs apunta al área de la FPU (FXSAVE o XSAVE, alineada a 64;
 * NULL si el hilo no la había usado). Igual que contexto_sinal_t de abi.h. */
#ifndef NS_SYS_UCONTEXT_H
#define NS_SYS_UCONTEXT_H

#include <signal.h>

enum { REG_GS = 0, REG_FS, REG_ES, REG_DS, REG_EDI, REG_ESI, REG_EBP, REG_ESP, REG_EBX, REG_EDX, REG_ECX,
       REG_EAX, REG_TRAPNO, REG_ERR, REG_EIP, REG_CS, REG_EFL, REG_UESP, REG_SS };
#define REG_GS     REG_GS
#define REG_FS     REG_FS
#define REG_ES     REG_ES
#define REG_DS     REG_DS
#define REG_EDI    REG_EDI
#define REG_ESI    REG_ESI
#define REG_EBP    REG_EBP
#define REG_ESP    REG_ESP
#define REG_EBX    REG_EBX
#define REG_EDX    REG_EDX
#define REG_ECX    REG_ECX
#define REG_EAX    REG_EAX
#define REG_TRAPNO REG_TRAPNO
#define REG_ERR    REG_ERR
#define REG_EIP    REG_EIP
#define REG_CS     REG_CS
#define REG_EFL    REG_EFL
#define REG_UESP   REG_UESP
#define REG_SS     REG_SS
#define NGREG 19

typedef int greg_t;
typedef greg_t gregset_t[NGREG];
typedef void *fpregset_t;
typedef struct {
    gregset_t gregs;
    fpregset_t fpregs;
    unsigned long oldmask, cr2;
} mcontext_t;

typedef struct ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
    unsigned long __fpu_formato;   /* 0 sin FPU, 1 FNSAVE, 2 FXSAVE, 3 XSAVE (D381) */
    unsigned long __reservado[3];
} ucontext_t;
#endif
