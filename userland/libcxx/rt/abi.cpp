/* abi.cpp - lo mínimo del runtime de C++ que no son excepciones (D228).
 *
 *   - operator new / delete (y sus formas [] , con tamaño y nothrow) sobre el
 *     malloc de la libc común (D210). Son débiles (SAGUI_SUSTITUIBLE): el C++
 *     deja que el programa ponga los suyos (Vanilla Conquer, en common/newdel.cpp)
 *     y, como el runtime se enlaza en objetos y no en una biblioteca, sin eso el
 *     enlace daba «multiple definition» (D230). Sin memoria, new no lanza
 *     std::bad_alloc (no hay biblioteca estándar): avisa y para el programa,
 *     que es lo que acabaría pasando en un juego sin manejador para eso.
 *   - __cxa_pure_virtual y __cxa_deleted_virtual: una llamada a una función
 *     virtual pura (un objeto a medio construir o ya destruido) para el
 *     programa con un aviso.
 *   - __cxa_atexit y __dso_handle: los destructores de los objetos globales y
 *     de los estáticos locales. Se ejecutan al revés, en exit() (la libc los
 *     llama por atexit); si principal vuelve sin exit(), no se ejecutan (el
 *     sistema recoge la memoria igual).
 *   - __cxa_guard_*: los estáticos locales se construyen una vez. Un solo
 *     hilo: basta un byte.
 *
 * Los constructores de los objetos globales no están aquí: los llama
 * userland/libsagui/inicio.asm desde .init_array, antes de principal.
 */
#include <stddef.h>
#include <stdint.h>

#include <new>

#include "desenrolar.h"

extern "C" {
void *malloc(size_t n);
void  free(void *p);
void  abort(void) __attribute__((noreturn));
int   atexit(void (*f)(void));
}

namespace std {
const nothrow_t nothrow{};
} // namespace std

[[noreturn]] static void sem_memoria()
{
    cxxrt_aviso("C++: operator new sin memoria");
    abort();
}

static void *reservar(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) sem_memoria();
    return p;
}

/* Débil: si el programa define la suya, gana la del programa (función sustituible). */
#define SAGUI_SUSTITUIBLE __attribute__((weak))

SAGUI_SUSTITUIBLE void *operator new(size_t n) { return reservar(n); }
SAGUI_SUSTITUIBLE void *operator new[](size_t n) { return reservar(n); }
SAGUI_SUSTITUIBLE void *operator new(size_t n, const std::nothrow_t &) noexcept { return malloc(n ? n : 1); }
SAGUI_SUSTITUIBLE void *operator new[](size_t n, const std::nothrow_t &) noexcept { return malloc(n ? n : 1); }
SAGUI_SUSTITUIBLE void operator delete(void *p) noexcept { free(p); }
SAGUI_SUSTITUIBLE void operator delete[](void *p) noexcept { free(p); }
SAGUI_SUSTITUIBLE void operator delete(void *p, size_t) noexcept { free(p); }
SAGUI_SUSTITUIBLE void operator delete[](void *p, size_t) noexcept { free(p); }
SAGUI_SUSTITUIBLE void operator delete(void *p, const std::nothrow_t &) noexcept { free(p); }
SAGUI_SUSTITUIBLE void operator delete[](void *p, const std::nothrow_t &) noexcept { free(p); }

/* Las formas alineadas (C++17, tipos con alignas mayor que 16): se guarda el
 * puntero de malloc justo antes del bloque alineado. */
static void *reservar_alinhado(size_t n, size_t alin)
{
    if (alin < sizeof(void *)) alin = sizeof(void *);
    char *bruto = static_cast<char *>(malloc(n + alin + sizeof(void *)));
    if (!bruto) return nullptr;
    uintptr_t p = (reinterpret_cast<uintptr_t>(bruto) + sizeof(void *) + alin - 1) & ~(static_cast<uintptr_t>(alin) - 1);
    reinterpret_cast<void **>(p)[-1] = bruto;
    return reinterpret_cast<void *>(p);
}
static void liberar_alinhado(void *p)
{
    if (p) free(static_cast<void **>(p)[-1]);
}
SAGUI_SUSTITUIBLE void *operator new(size_t n, std::align_val_t a)
{
    void *p = reservar_alinhado(n, static_cast<size_t>(a));
    if (!p) sem_memoria();
    return p;
}
SAGUI_SUSTITUIBLE void *operator new[](size_t n, std::align_val_t a) { return operator new(n, a); }
SAGUI_SUSTITUIBLE void operator delete(void *p, std::align_val_t) noexcept { liberar_alinhado(p); }
SAGUI_SUSTITUIBLE void operator delete[](void *p, std::align_val_t) noexcept { liberar_alinhado(p); }
SAGUI_SUSTITUIBLE void operator delete(void *p, size_t, std::align_val_t) noexcept { liberar_alinhado(p); }
SAGUI_SUSTITUIBLE void operator delete[](void *p, size_t, std::align_val_t) noexcept { liberar_alinhado(p); }

extern "C" {

[[noreturn]] void __cxa_pure_virtual()
{
    cxxrt_aviso("C++: llamada a una función virtual pura");
    abort();
}

[[noreturn]] void __cxa_deleted_virtual()
{
    cxxrt_aviso("C++: llamada a una función virtual borrada");
    abort();
}

/* ---- destructores de los objetos globales ---- */

void *__dso_handle = &__dso_handle;

namespace {
struct ao_sair_t {
    void (*f)(void *);
    void *arg;
    ao_sair_t *prox;
};
}

static ao_sair_t *lista_ao_sair = nullptr;
static bool registrado = false;

static void executar_ao_sair()
{
    while (lista_ao_sair) {
        ao_sair_t *a = lista_ao_sair;
        lista_ao_sair = a->prox;
        a->f(a->arg);
        free(a);
    }
}

int __cxa_atexit(void (*f)(void *), void *arg, void *dso)
{
    (void)dso;
    ao_sair_t *a = static_cast<ao_sair_t *>(malloc(sizeof(ao_sair_t)));
    if (!a) return -1;
    a->f = f;
    a->arg = arg;
    a->prox = lista_ao_sair;
    lista_ao_sair = a;
    if (!registrado) {
        registrado = true;
        atexit(executar_ao_sair);
    }
    return 0;
}

void __cxa_finalize(void *dso)
{
    (void)dso;
    executar_ao_sair();
}

/* ---- estáticos locales ----
 * El primer byte de la guarda: 1 = ya construido. El segundo: en
 * construcción (si la construcción vuelve a pedirse a sí misma, es un error
 * del programa: se para). */
int __cxa_guard_acquire(uint64_t *guarda)
{
    uint8_t *g = reinterpret_cast<uint8_t *>(guarda);
    if (g[0]) return 0;
    if (g[1]) {
        cxxrt_aviso("C++: un estático local se usa mientras se construye");
        abort();
    }
    g[1] = 1;
    return 1;
}

void __cxa_guard_release(uint64_t *guarda)
{
    uint8_t *g = reinterpret_cast<uint8_t *>(guarda);
    g[0] = 1;
    g[1] = 0;
}

void __cxa_guard_abort(uint64_t *guarda)
{
    reinterpret_cast<uint8_t *>(guarda)[1] = 0;
}

} // extern "C"
