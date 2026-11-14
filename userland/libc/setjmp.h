/* setjmp.h - saltos no locales (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_SETJMP_H
#define NS_SETJMP_H

/* ebx, esi, edi, ebp, esp y eip: libc/saltos.c (ensamblador i386). Sin
 * señales, la máscara no se guarda: sigsetjmp es setjmp. */
typedef unsigned long jmp_buf[6];
typedef jmp_buf sigjmp_buf;
int  setjmp(jmp_buf b) __attribute__((returns_twice));
void longjmp(jmp_buf b, int v) __attribute__((noreturn));
#define sigsetjmp(b, s)   setjmp(b)
#define siglongjmp(b, v)  longjmp(b, v)
#define _setjmp(b)        setjmp(b)
#define _longjmp(b, v)    longjmp(b, v)
#endif
