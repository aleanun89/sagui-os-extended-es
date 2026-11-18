/* SPDX-License-Identifier: GPL-3.0-only */
/* sys/ucontext.h - lo mínimo para el código de trazas de TCC (tccrun.c, bt-exe.c) en SaguiOS (D384).
 * SaguiOS no entrega señales por fallos del procesador (signal.h de la libc común): el tratador que
 * instala TCC nunca se llama con un contexto real, pero el código tiene que compilar. Los registros,
 * en el orden de Linux i386. */
#ifndef SAGUI_SYS_UCONTEXT_H
#define SAGUI_SYS_UCONTEXT_H

enum { REG_GS, REG_FS, REG_ES, REG_DS, REG_EDI, REG_ESI, REG_EBP, REG_ESP, REG_EBX, REG_EDX, REG_ECX, REG_EAX,
       REG_TRAPNO, REG_ERR, REG_EIP, REG_CS, REG_EFL, REG_UESP, REG_SS, NGREG };
#define REG_EIP REG_EIP
#define REG_EBP REG_EBP

typedef int greg_t;
typedef struct { greg_t gregs[NGREG]; } mcontext_t;
typedef struct ucontext { unsigned long uc_flags; struct ucontext *uc_link; mcontext_t uc_mcontext; } ucontext_t;

/* códigos de si_code que nombra tccrun.c */
#ifndef FPE_INTDIV
#define FPE_INTDIV 1
#define FPE_FLTDIV 3
#endif

#endif
