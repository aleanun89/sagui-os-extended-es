// SPDX-License-Identifier: GPL-2.0-or-later
// cxxrt_unidad.cpp - prueba en el PC del runtime de C++ del SaguiOS (D228).
//
// Se compila con el runtime (userland/libcxx/rt/) y el guion de enlace de los
// programas de C++ (userland/programa_cxx.ld), sin la libc del PC: lo poco que
// hace falta (malloc, free, abort, la salida) está aquí, sobre las llamadas al
// sistema de Linux de 32 bits. Lo lanza tests/h228_openjazz.py (parte
// «unidad») con varios niveles de optimización: con -Os g++ empuja los
// argumentos (DW_CFA_GNU_args_size) y con -O0 usa el marco con EBP.
//
// Cada caso escribe «ok <nombre>» o «FALLO <nombre>»; al final, «FIN <n>
// fallos». Con el argumento «sin-captura» lanza una excepción que nadie
// recoge: tiene que acabar con el código 134 y el aviso de std::terminate.

#include "cxxrt_sistema_pc.h"

// ------------------------------------------------------------ los casos

static int fallos = 0;
static int destruidos = 0;

static void caso(const char *nombre, bool bien)
{
    escribir(1, bien ? "ok " : "FALLO ");
    escribir(1, nombre);
    escribir(1, "\n");
    if (!bien) fallos++;
}

struct Guardia {
    int *contador;
    explicit Guardia(int *c) : contador(c) {}
    ~Guardia() { (*contador)++; }
};

struct Base { int codigo; virtual ~Base() {} };
struct Derivada : Base { int extra; ~Derivada() override { destruidos++; } };

struct FallaAlConstruir {
    int datos[8];
    explicit FallaAlConstruir(int x) { if (x) throw x; datos[0] = 0; }
};

__attribute__((noinline)) static void lanzar_int(int v) { throw v; }

__attribute__((noinline)) static int profundo(int n, int *cont)
{
    Guardia g(cont);
    if (n == 0) lanzar_int(42);
    return profundo(n - 1, cont) + 1;
}

// Muchos argumentos empujados en la llamada que lanza (con -Os: GNU_args_size).
__attribute__((noinline)) static int muchos(int a, int b, int c, int d, int e, int f, int g, int h)
{
    if (a + b + c + d + e + f + g + h == 36) throw 7;
    return a;
}

// Una variable alineada a 64 obliga a realinear la pila (las CFI con expresiones).
__attribute__((noinline)) static int realineada(int x)
{
    alignas(64) volatile int v[16];
    v[0] = x;
    Guardia g(&destruidos);
    if (v[0] > 0) lanzar_int(v[0]);
    return v[0];
}

__attribute__((noinline)) static void relanzar()
{
    try {
        lanzar_int(5);
    } catch (int e) {
        if (e == 5) throw;
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == 's') {   // sin-captura
        lanzar_int(1);
        return 0;
    }

    { // 1. en la misma función
        int r = 0;
        try { lanzar_int(3); } catch (int e) { r = e; }
        caso("mismo-cuadro", r == 3);
    }
    { // 2. a través de cuadros con destructores
        int cont = 0, r = 0;
        try { profundo(10, &cont); } catch (int e) { r = e; }
        caso("destructores", r == 42 && cont == 11);
    }
    { // 3. relanzar con «throw;»
        int r = 0;
        try { relanzar(); } catch (int e) { r = e; }
        caso("relanzar", r == 5);
    }
    { // 4. catch (...)
        bool visto = false;
        try { lanzar_int(9); } catch (...) { visto = true; }
        caso("catch-todo", visto);
    }
    { // 5. una clase derivada recogida por su base, y destruida al acabar
        int r = 0;
        destruidos = 0;
        try { Derivada d; d.codigo = 77; throw d; } catch (Base &b) { r = b.codigo; }
        caso("base-derivada", r == 77 && destruidos == 2);
    }
    { // 6. el constructor lanza en un new: se libera la memoria
        int antes = vivos, r = 0;
        FallaAlConstruir *p = nullptr;
        try { p = new FallaAlConstruir(13); } catch (int e) { r = e; }
        caso("new-que-falla", r == 13 && p == nullptr && vivos == antes);
    }
    { // 7. una excepción dentro de un catch
        int r = 0;
        try {
            try { lanzar_int(1); }
            catch (int) {
                try { lanzar_int(2); } catch (int e2) { r = e2 * 10; }
            }
        } catch (...) { r = -1; }
        caso("anidada", r == 20);
    }
    { // 8. un puntero
        const char *r = nullptr;
        try { throw "hola"; } catch (const char *s) { r = s; }
        caso("puntero", r && r[0] == 'h' && r[3] == 'a');
    }
    { // 9. argumentos empujados
        int r = 0;
        try { muchos(1, 2, 3, 4, 5, 6, 7, 8); } catch (int e) { r = e; }
        caso("argumentos", r == 7);
    }
    { // 10. pila realineada
        int r = 0;
        destruidos = 0;
        try { realineada(31); } catch (int e) { r = e; }
        caso("realineada", r == 31 && destruidos == 1);
    }
    { // 11. el catch no coincide: sigue subiendo
        int r = 0;
        try {
            try { lanzar_int(4); } catch (const char *) { r = -1; }
        } catch (int e) { r = e; }
        caso("no-coincide", r == 4);
    }
    { // 12. todo liberado
        caso("sin-fugas", vivos == 0);
    }

    escribir(1, fallos ? "FIN con fallos\n" : "FIN 0 fallos\n");
    return fallos ? 1 : 0;
}
