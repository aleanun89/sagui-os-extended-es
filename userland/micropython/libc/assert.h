/* assert.h mínimo para o MicroPython no SaguiOS (sem libc). */
#ifndef SAGUI_ASSERT_H
#define SAGUI_ASSERT_H
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
void __assert_func(const char *arq, int linha, const char *func, const char *expr);
#define assert(x) ((x) ? (void)0 : __assert_func(__FILE__, __LINE__, __func__, #x))
#endif
#endif
