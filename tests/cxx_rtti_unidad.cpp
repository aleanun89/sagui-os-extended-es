// SPDX-License-Identifier: GPL-2.0-or-later
// cxx_rtti_unidad.cpp - prueba en el PC del RTTI del runtime propio (userland/libcxx/rt/rtti.cpp, D294).
//
// Se compila con -frtti y se enlaza con desenrolar.c, abi_tipos.c, excecoes.cpp, abi.cpp y rtti.cpp
// (sin libstdc++): dynamic_cast hacia abajo, en cruz, con base virtual y con base privada, typeid,
// std::bad_cast al fallar un dynamic_cast de referencia, y dynamic_cast<void *>.
// Escribe «FIN 0 fallos» y sale con 0 si pasa todo.
#include <typeinfo>

#include "cxxrt_sistema_pc.h"

static bool contiene(const char *p, const char *q)
{
    for (; *p; p++) {
        const char *a = p, *b = q;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

static int fallos, total;
#define COMPROBAR(c, msg)                                          \
    do {                                                           \
        total++;                                                   \
        if (!(c)) { fallos++; escribir(1, "FALLO: "); escribir(1, msg); escribir(1, "\n"); } \
    } while (0)

struct Animal { virtual ~Animal() {} int a = 1; };
struct Perro : Animal { int p = 2; };
struct Cachorro : Perro { int c = 3; };
struct Gato : Animal { int g = 4; };

// Herencia múltiple: Nadador y Volador son bases distintas de Pato.
struct Nadador { virtual ~Nadador() {} int n = 10; };
struct Volador { virtual ~Volador() {} int v = 20; };
struct Pato : Nadador, Volador { int pt = 30; };
struct Ganso : Nadador { int gs = 40; };

// Diamante con base virtual.
struct Raiz { virtual ~Raiz() {} int r = 100; };
struct Izq : virtual Raiz { int i = 1; };
struct Der : virtual Raiz { int d = 2; };
struct Punta : Izq, Der { int pu = 3; };

// Base privada: no se puede convertir a ella con dynamic_cast.
struct Privada { virtual ~Privada() {} };
struct ConPrivada : private Privada, public Nadador {};

// Dos subobjetos del mismo tipo: la conversión a la base repetida es ambigua.
struct Dup1 : Nadador {};
struct Dup2 : Nadador {};
struct Ambigua : Dup1, Dup2 {};

int main(int argc, char **argv)
{
    (void)argv;
    Cachorro cac;
    Animal *a = &cac;
    COMPROBAR(dynamic_cast<Perro *>(a) == static_cast<Perro *>(&cac), "bajar Animal -> Perro");
    COMPROBAR(dynamic_cast<Cachorro *>(a) == &cac, "bajar Animal -> Cachorro (tipo dinámico)");
    COMPROBAR(dynamic_cast<Gato *>(a) == nullptr, "bajar a un hermano da nulo");
    Perro p;
    Animal *ap = &p;
    COMPROBAR(dynamic_cast<Cachorro *>(ap) == nullptr, "bajar a una derivada que no es da nulo");
    COMPROBAR(dynamic_cast<Perro *>(ap) == &p && dynamic_cast<Perro *>(ap)->p == 2, "bajar y leer un campo");
    Animal *nulo = nullptr;
    COMPROBAR(dynamic_cast<Perro *>(nulo) == nullptr, "el puntero nulo se queda nulo");
    COMPROBAR(dynamic_cast<Animal *>(&cac) == a, "subir (lo resuelve el compilador)");

    Pato pato;
    Nadador *n = &pato;
    Volador *vo = dynamic_cast<Volador *>(n);
    COMPROBAR(vo == static_cast<Volador *>(&pato) && vo->v == 20, "en cruz Nadador -> Volador");
    COMPROBAR(dynamic_cast<Pato *>(n) == &pato, "bajar Nadador -> Pato (base no inicial)");
    COMPROBAR(dynamic_cast<Pato *>(vo) == &pato && dynamic_cast<Pato *>(vo)->pt == 30, "bajar Volador -> Pato (base con desplazamiento)");
    Ganso ganso;
    Nadador *ng = &ganso;
    COMPROBAR(dynamic_cast<Volador *>(ng) == nullptr, "en cruz a algo que el objeto no tiene da nulo");
    COMPROBAR(dynamic_cast<void *>(vo) == static_cast<void *>(&pato), "dynamic_cast<void *> da el objeto completo");

    Punta punta;
    Raiz *raiz = &punta;
    COMPROBAR(dynamic_cast<Punta *>(raiz) == &punta, "bajar desde una base virtual");
    COMPROBAR(dynamic_cast<Izq *>(raiz) == static_cast<Izq *>(&punta), "base virtual -> Izq");
    COMPROBAR(dynamic_cast<Der *>(raiz) == static_cast<Der *>(&punta), "base virtual -> Der");
    Izq *iz = &punta;
    COMPROBAR(dynamic_cast<Der *>(iz) == static_cast<Der *>(&punta), "en cruz Izq -> Der (por la base virtual)");
    COMPROBAR(dynamic_cast<Punta *>(iz) == &punta && dynamic_cast<Punta *>(iz)->pu == 3, "bajar Izq -> Punta");

    ConPrivada cp;
    Nadador *nc = &cp;
    COMPROBAR(dynamic_cast<Privada *>(nc) == nullptr, "una base privada no es accesible");
    Ambigua amb;
    Dup1 *d1 = &amb;
    COMPROBAR(dynamic_cast<Dup2 *>(d1) == static_cast<Dup2 *>(&amb), "en cruz entre dos ramas");
    Ambigua *am = &amb;
    Nadador *x = static_cast<Dup1 *>(am);
    COMPROBAR(dynamic_cast<Ambigua *>(x) == &amb, "bajar desde una base repetida si el origen es unívoco");

    // Referencias: bad_cast.
    bool lanzo = false;
    try {
        Animal &r = p;
        (void)dynamic_cast<Gato &>(r);
    } catch (const std::bad_cast &) {
        lanzo = true;
    }
    COMPROBAR(lanzo, "dynamic_cast de referencia que falla lanza std::bad_cast");
    lanzo = false;
    try {
        Animal &r = cac;
        Perro &pr = dynamic_cast<Perro &>(r);
        COMPROBAR(&pr == static_cast<Perro *>(&cac), "dynamic_cast de referencia que acierta");
    } catch (...) {
        lanzo = true;
    }
    COMPROBAR(!lanzo, "sin excepción cuando acierta");
    lanzo = false;
    try {
        throw std::bad_cast();
    } catch (const std::exception &e) {
        lanzo = strcmp(e.what(), "std::bad_cast") == 0;
    }
    COMPROBAR(lanzo, "bad_cast es una std::exception");

    // typeid.
    Animal *tp = &p;
    COMPROBAR(typeid(*tp) == typeid(Perro), "typeid del tipo dinámico");
    COMPROBAR(typeid(*tp) != typeid(Animal), "typeid distingue la base");
    COMPROBAR(typeid(Gato) != typeid(Perro) && typeid(Perro) == typeid(Perro), "typeid de tipos");
    COMPROBAR(typeid(int) == typeid(int) && typeid(int) != typeid(long), "typeid de tipos básicos");
    COMPROBAR(contiene(typeid(Cachorro).name(), "Cachorro"), "typeid().name() lleva el nombre");
    COMPROBAR(typeid(Perro).before(typeid(Gato)) != typeid(Gato).before(typeid(Perro)), "before da un orden total");
    lanzo = false;
    try {
        Animal *np = argc > 100 ? &p : nullptr;
        (void)typeid(*np);
    } catch (const std::bad_typeid &) {
        lanzo = true;
    }
    COMPROBAR(lanzo, "typeid de un puntero nulo lanza std::bad_typeid");

    escribir(1, fallos ? "FIN con fallos\n" : "FIN 0 fallos\n");
    (void)total;
    return fallos ? 1 : 0;
}
