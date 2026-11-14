/* float.h próprio do MicroPython no SaguiOS (D082).
 *
 * O float.h do GCC diz FLT_EVAL_METHOD = 2 quando a conta é feita pela x87
 * (/bin/python-x87), e o rint.c da libm_dbl passaria a arredondar com
 * 2^63 em vez de 2^52 (round(2.675, 2) daria 0.0). Na variante x87 a
 * precisão da FPU fica em 53 bits (inicio_python.c): cada conta em double
 * é arredondada como no SSE2, que é o caso FLT_EVAL_METHOD = 0. Os valores
 * vêm das macros que o próprio GCC define. */
#ifndef SAGUI_FLOAT_H
#define SAGUI_FLOAT_H

#define FLT_EVAL_METHOD 0
#define FLT_RADIX       __FLT_RADIX__

#define FLT_MANT_DIG    __FLT_MANT_DIG__
#define FLT_DIG         __FLT_DIG__
#define FLT_EPSILON     __FLT_EPSILON__
#define FLT_MIN         __FLT_MIN__
#define FLT_MAX         __FLT_MAX__
#define FLT_MIN_EXP     __FLT_MIN_EXP__
#define FLT_MAX_EXP     __FLT_MAX_EXP__

#define DBL_MANT_DIG    __DBL_MANT_DIG__
#define DBL_DIG         __DBL_DIG__
#define DBL_EPSILON     __DBL_EPSILON__
#define DBL_MIN         __DBL_MIN__
#define DBL_MAX         __DBL_MAX__
#define DBL_MIN_EXP     __DBL_MIN_EXP__
#define DBL_MAX_EXP     __DBL_MAX_EXP__

#define LDBL_MANT_DIG   __LDBL_MANT_DIG__
#define LDBL_EPSILON    __LDBL_EPSILON__
#define LDBL_MIN        __LDBL_MIN__
#define LDBL_MAX        __LDBL_MAX__
#define LDBL_MAX_EXP    __LDBL_MAX_EXP__

#endif
