/* rtti.cpp - RTTI del runtime de C++: dynamic_cast, typeid y sus errores (D294).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Escrito a partir del ABI de C++ de Itanium (la disposición de los typeinfo y de las tablas
 * virtuales), sin código de libsupc++. Lo que da el runtime:
 *
 *   - __dynamic_cast(origen, tipo_origen, tipo_destino, pista): el dynamic_cast de g++ entre
 *     punteros o referencias a clases polimórficas, con herencia simple, múltiple y virtual,
 *     bajando (de la base a la derivada) y en cruz (de una base a otra del mismo objeto);
 *   - __cxa_bad_cast y __cxa_bad_typeid, que lanzan std::bad_cast y std::bad_typeid;
 *   - el destructor de std::type_info (su «función clave»).
 *
 * Los tipos fundamentales y las tablas de los typeinfo están en abi_tipos.c. Los typeinfo de
 * las clases los emite g++ con -frtti: vptr (una de las tres tablas class/si/vmi), nombre y,
 * según la clase, sus bases. Detrás del vptr de un objeto: [-1] = typeinfo del tipo dinámico y
 * [-2] = desplazamiento hasta el objeto completo.
 *
 * Solo se explora el tipo dinámico del objeto: se buscan sus subobjetos del tipo destino por
 * caminos públicos; entre ellos vale el que contiene el subobjeto origen (bajar) o, si ninguno
 * lo contiene (cruzar), el único que hay. Con dos candidatos distintos es ambiguo y devuelve 0.
 */
#include <stddef.h>
#include <stdint.h>
extern "C" int strcmp(const char *, const char *);

#include <typeinfo>

extern "C" {
extern const void *_ZTVN10__cxxabiv117__class_type_infoE[];
extern const void *_ZTVN10__cxxabiv120__si_class_type_infoE[];
extern const void *_ZTVN10__cxxabiv121__vmi_class_type_infoE[];
}

namespace {

struct ti {
    const void *vtabela;
    const char *nome;
};
struct ti_si : ti {
    const ti *base;
};
struct base_vmi {
    const ti *tipo;
    long deslocamento_e_flags;   /* bit 0: virtual; bit 1: pública; el resto (>> 8): desplazamiento */
};
struct ti_vmi : ti {
    unsigned int flags;
    unsigned int n_bases;
    base_vmi bases[1];
};

const unsigned BASE_VIRTUAL = 1, BASE_PUBLICA = 2;

bool e_classe(const ti *t) { return t->vtabela == &_ZTVN10__cxxabiv117__class_type_infoE[2]; }
bool e_si(const ti *t) { return t->vtabela == &_ZTVN10__cxxabiv120__si_class_type_infoE[2]; }
bool e_vmi(const ti *t) { return t->vtabela == &_ZTVN10__cxxabiv121__vmi_class_type_infoE[2]; }

bool mesmo(const ti *a, const ti *b)
{
    if (a == b) return true;
    if (!a || !b || a->nome[0] == '*' || b->nome[0] == '*') return false;
    return strcmp(a->nome, b->nome) == 0;
}

/* Las bases directas de una clase: para cada una, su tipo, su dirección dentro de «obj» y si
 * el camino es público. Devuelve cuántas. */
struct filho {
    const ti *tipo;
    const char *obj;
    bool publica;
};
int bases_de(const ti *t, const char *obj, filho *saida, int max)
{
    if (e_si(t)) {
        saida[0] = { static_cast<const ti_si *>(t)->base, obj, true };
        return 1;
    }
    if (!e_vmi(t)) return 0;
    const ti_vmi *v = static_cast<const ti_vmi *>(t);
    int n = 0;
    for (unsigned i = 0; i < v->n_bases && n < max; i++) {
        long of = v->bases[i].deslocamento_e_flags;
        long desl = of >> 8;
        if (of & BASE_VIRTUAL) {
            const char *vt = *reinterpret_cast<const char *const *>(obj);
            desl = *reinterpret_cast<const long *>(vt + desl);   /* el desplazamiento está en la tabla virtual */
        }
        saida[n++] = { v->bases[i].tipo, obj + desl, (of & BASE_PUBLICA) != 0 };
    }
    return n;
}

const int MAX_FILHOS = 32, MAX_ACHADOS = 16;

struct achado {
    const char *obj;
    bool contem_origem;
};
struct busca {
    const ti *destino, *origem;
    const char *obj_origem;
    achado achados[MAX_ACHADOS];
    int n;
};

/* ¿El subobjeto de tipo «origem» en «obj_origem» está (por camino público) dentro de «obj»? */
bool contem(const ti *t, const char *obj, const ti *origem, const char *obj_origem, int prof)
{
    if (mesmo(t, origem) && obj == obj_origem) return true;
    if (prof > 24) return false;
    filho f[MAX_FILHOS];
    int n = bases_de(t, obj, f, MAX_FILHOS);
    for (int i = 0; i < n; i++)
        if (f[i].publica && contem(f[i].tipo, f[i].obj, origem, obj_origem, prof + 1)) return true;
    return false;
}

/* Todos los subobjetos de tipo destino (sin repetir direcciones) por caminos públicos. */
void buscar(busca &b, const ti *t, const char *obj, int prof)
{
    if (prof > 24) return;
    if (mesmo(t, b.destino)) {
        for (int i = 0; i < b.n; i++)
            if (b.achados[i].obj == obj) return;   /* el mismo subobjeto por otro camino (base virtual) */
        if (b.n < MAX_ACHADOS)
            b.achados[b.n++] = { obj, contem(t, obj, b.origem, b.obj_origem, 0) };
        return;
    }
    filho f[MAX_FILHOS];
    int n = bases_de(t, obj, f, MAX_FILHOS);
    for (int i = 0; i < n; i++)
        if (f[i].publica) buscar(b, f[i].tipo, f[i].obj, prof + 1);
}

} // namespace

extern "C" {

void *__dynamic_cast(const void *src, const void *tipo_origem, const void *tipo_destino, ptrdiff_t pista)
{
    (void)pista;
    if (!src) return 0;
    const char *vt = *reinterpret_cast<const char *const *>(src);
    const ti *dinamico = reinterpret_cast<const ti *const *>(vt)[-1];
    ptrdiff_t topo = reinterpret_cast<const ptrdiff_t *>(vt)[-2];
    if (!dinamico || (!e_classe(dinamico) && !e_si(dinamico) && !e_vmi(dinamico))) return 0;   /* sin RTTI */
    const char *completo = static_cast<const char *>(src) + topo;

    busca b;
    b.destino = static_cast<const ti *>(tipo_destino);
    b.origem = static_cast<const ti *>(tipo_origem);
    b.obj_origem = static_cast<const char *>(src);
    b.n = 0;
    buscar(b, dinamico, completo, 0);

    const char *res = 0;
    int cont = 0;
    for (int i = 0; i < b.n; i++)
        if (b.achados[i].contem_origem) { res = b.achados[i].obj; cont++; }
    if (cont == 1) return const_cast<char *>(res);
    if (cont > 1) return 0;
    if (b.n == 1) return const_cast<char *>(b.achados[0].obj);   /* en cruz */
    return 0;
}

void __cxa_bad_cast()
{
    throw std::bad_cast();
}

void __cxa_bad_typeid()
{
    throw std::bad_typeid();
}

} // extern "C"

namespace std {
type_info::~type_info() {}
} // namespace std
