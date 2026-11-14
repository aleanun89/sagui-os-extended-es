/* fenv.h - el entorno de coma flotante, mínimo (D210).
 *
 * Parte de la libc común (userland/libc). El SaguiOS no cambia el modo de
 * redondeo ni mira las excepciones de la FPU: siempre redondea al más cercano.
 * fesetround solo acepta ese modo; las excepciones nunca se ven levantadas.
 * Lo pide QuickJS (lo incluye, sin usarlo en el SaguiOS).
 */
#ifndef NS_FENV_H
#define NS_FENV_H

/* Los valores de la x87 (bits 10-11 de la palabra de control), como la glibc. */
#define FE_TONEAREST  0
#define FE_DOWNWARD   0x400
#define FE_UPWARD     0x800
#define FE_TOWARDZERO 0xc00

#define FE_INVALID    1
#define FE_DIVBYZERO  4
#define FE_OVERFLOW   8
#define FE_UNDERFLOW  16
#define FE_INEXACT    32
#define FE_ALL_EXCEPT 61

typedef unsigned short fexcept_t;
typedef struct { unsigned short control, estado; } fenv_t;

static inline int fegetround(void) { return FE_TONEAREST; }
static inline int fesetround(int modo) { return modo == FE_TONEAREST ? 0 : -1; }
static inline int feclearexcept(int e) { (void)e; return 0; }
static inline int fetestexcept(int e) { (void)e; return 0; }
static inline int feraiseexcept(int e) { (void)e; return 0; }

#endif
