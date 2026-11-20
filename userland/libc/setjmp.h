/* setjmp.h - saltos no locales (D184; sigsetjmp guarda la máscara desde la D401).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SETJMP_H
#define NS_SETJMP_H

/* ebx, esi, edi, ebp, esp y eip: libc/saltos.c (ensamblador i386). */
typedef unsigned long jmp_buf[6];
int  setjmp(jmp_buf b) __attribute__((returns_twice));
void longjmp(jmp_buf b, int v) __attribute__((noreturn));
/* D401: sigsetjmp(b, 1) guarda además la máscara de señales del hilo, y siglongjmp la repone
 * (salir de un tratador de SIGSEGV con siglongjmp deja la señal desbloqueada). La máscara se
 * guarda antes del setjmp: es la misma en los dos momentos. */
typedef struct { jmp_buf b; int salvo; unsigned long mascara; } sigjmp_buf[1];
void __sigjmp_salvar(sigjmp_buf b, int salvar);
void siglongjmp(sigjmp_buf b, int v) __attribute__((noreturn));
#define sigsetjmp(e, s)   (__sigjmp_salvar((e), (s)), setjmp((e)->b))
#define _setjmp(b)        setjmp(b)
#define _longjmp(b, v)    longjmp(b, v)
#endif
