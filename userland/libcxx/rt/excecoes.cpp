/* excecoes.cpp - las excepciones de C++: el lado del lenguaje (D228).
 *
 * El código que genera g++ para «throw» y «catch» llama a estas funciones del
 * ABI de C++ de Itanium:
 *
 *   throw x;      -> __cxa_allocate_exception(tamaño), construye x ahí,
 *                    __cxa_throw(objeto, typeinfo, destructor)
 *   catch (T &e)  -> el landing pad recibe la excepción en EAX y el
 *                    «selector» (qué catch) en EDX; llama a
 *                    __cxa_begin_catch y, al salir del bloque, a __cxa_end_catch
 *   throw;        -> __cxa_rethrow
 *
 * y la «personalidad» (__gxx_personality_v0) es la que, para cada cuadro de
 * la pila, lee la tabla de C++ de la función (la LSDA de .gcc_except_table)
 * y decide si allí hay un catch para la excepción, solo destructores o nada.
 * Quien sube por la pila es desenrolar.c.
 *
 * Lo que se sabe emparejar: el mismo tipo (por la dirección de su typeinfo o
 * por su nombre), catch (...), una clase base con herencia simple
 * (__si_class_type_info) y punteros a esos tipos. La herencia múltiple o
 * virtual (__vmi_class_type_info) no se empareja con sus bases: solo con su
 * propio tipo. Es lo que necesitan los ports que hay (OpenJazz lanza enteros).
 *
 * Un solo hilo: la pila de excepciones capturadas es global.
 */
#include <stddef.h>
#include <stdint.h>

#include "desenrolar.h"

extern "C" {
void *malloc(size_t n);
void  free(void *p);
void  abort(void) __attribute__((noreturn));
int   strcmp(const char *a, const char *b);
}

namespace {

/* La disposición de los typeinfo de g++ (el ABI): un puntero a la tabla
 * virtual y el nombre; detrás, lo de cada clase. */
struct tipo_info {
    const void *vtabela;
    const char *nome;
};
struct tipo_si : tipo_info {          /* __si_class_type_info: una base */
    const tipo_info *base;
};
struct tipo_ponteiro : tipo_info {    /* __pointer_type_info */
    unsigned int qualificadores;
    const tipo_info *apontado;
};

} // namespace

/* Las tablas virtuales de los typeinfo (abi_tipos.c): solo se comparan sus
 * direcciones para saber de qué clase es un typeinfo. El vptr apunta dos
 * palabras dentro de la tabla. */
extern "C" const void *_ZTVN10__cxxabiv120__si_class_type_infoE[];
extern "C" const void *_ZTVN10__cxxabiv119__pointer_type_infoE[];

static bool e_si(const tipo_info *t) { return t->vtabela == &_ZTVN10__cxxabiv120__si_class_type_infoE[2]; }
static bool e_ponteiro(const tipo_info *t) { return t->vtabela == &_ZTVN10__cxxabiv119__pointer_type_infoE[2]; }

/* El mismo tipo: la misma dirección o el mismo nombre. Un nombre que
 * empieza por '*' es de un tipo local al fichero y solo vale la dirección. */
static bool mesmo_tipo(const tipo_info *a, const tipo_info *b)
{
    if (a == b) return true;
    if (!a || !b || !a->nome || !b->nome) return false;
    if (a->nome[0] == '*' || b->nome[0] == '*') return false;
    return strcmp(a->nome, b->nome) == 0;
}

/* ¿Un catch de «captura» recoge un objeto lanzado de tipo «lancado»? */
static bool casa(const tipo_info *captura, const tipo_info *lancado)
{
    if (mesmo_tipo(captura, lancado)) return true;
    if (e_ponteiro(captura) && e_ponteiro(lancado)) {
        const tipo_ponteiro *c = static_cast<const tipo_ponteiro *>(captura);
        const tipo_ponteiro *l = static_cast<const tipo_ponteiro *>(lancado);
        /* No se puede quitar un const: T const* no entra en un catch (T*). */
        if ((l->qualificadores & ~c->qualificadores) & 3u) return false;
        return casa(c->apontado, l->apontado);
    }
    for (const tipo_info *t = lancado; e_si(t); ) {   /* subir por las bases */
        t = static_cast<const tipo_si *>(t)->base;
        if (mesmo_tipo(captura, t)) return true;
    }
    return false;
}

/* ------------------------------------------------------------ la cabecera */

namespace {

/* Lo que va delante de cada objeto lanzado. La _Unwind_Exception va la
 * última: el objeto empieza justo detrás (y queda alineado a 16). */
struct cabecalho {
    void *bruto;                         /* lo que devolvió malloc */
    const tipo_info *tipo;
    void (*destrutor)(void *);
    cabecalho *proximo;                  /* la pila de las capturadas */
    int manejadores;                     /* catch activos; negativo = relanzada */
    int seletor;                         /* caché de la fase 1 para la 2 */
    uintptr_t landing;
    void *ajustado;                      /* lo que recibe el catch */
    _Unwind_Exception unwind;
};

} // namespace

/* "GNUCC++\0": la clase de las excepciones de C++ de g++. */
static const uint64_t CLASSE_CXX = 0x474E5543432B2B00ULL;

static cabecalho *capturadas = nullptr;
static int nao_capturadas = 0;

static cabecalho *de_objeto(void *obj) { return reinterpret_cast<cabecalho *>(obj) - 1; }
static cabecalho *de_unwind(_Unwind_Exception *ue)
{
    return reinterpret_cast<cabecalho *>(reinterpret_cast<char *>(ue) - offsetof(cabecalho, unwind));
}
static void *objeto_de(cabecalho *h) { return h + 1; }

[[noreturn]] static void parar(const char *motivo)
{
    cxxrt_aviso(motivo);
    abort();
}

namespace std {
[[noreturn]] void terminate() noexcept;
[[noreturn]] void terminate() noexcept { parar("C++: std::terminate (excepción sin capturar o lanzada donde no se puede)"); }
bool uncaught_exception() noexcept { return nao_capturadas > 0; }
int uncaught_exceptions() noexcept { return nao_capturadas; }
} // namespace std

extern "C" {

void *__cxa_begin_catch(void *uev) noexcept;

void *__cxa_allocate_exception(size_t tamanho) noexcept
{
    size_t total = sizeof(cabecalho) + tamanho + 16;
    void *bruto = malloc(total);
    if (!bruto) parar("C++: no hay memoria para lanzar una excepción");
    uintptr_t p = (reinterpret_cast<uintptr_t>(bruto) + 15) & ~static_cast<uintptr_t>(15);
    cabecalho *h = reinterpret_cast<cabecalho *>(p);
    char *z = reinterpret_cast<char *>(h);
    for (size_t i = 0; i < sizeof(cabecalho); i++) z[i] = 0;
    h->bruto = bruto;
    return objeto_de(h);
}

void __cxa_free_exception(void *obj) noexcept
{
    if (obj) free(de_objeto(obj)->bruto);
}

static void destruir(cabecalho *h)
{
    if (h->destrutor) h->destrutor(objeto_de(h));
    free(h->bruto);
}

/* Si una excepción nuestra la recoge alguien de fuera (no pasa: solo hay
 * C++ de g++), se libera así. */
static void limpar_estrangeira(_Unwind_Reason_Code, _Unwind_Exception *ue)
{
    destruir(de_unwind(ue));
}

[[noreturn]] void __cxa_throw(void *obj, void *tinfo, void (*destrutor)(void *))
{
    cabecalho *h = de_objeto(obj);
    h->tipo = static_cast<const tipo_info *>(tinfo);
    h->destrutor = destrutor;
    h->manejadores = 0;
    h->unwind.exception_class = CLASSE_CXX;
    h->unwind.exception_cleanup = limpar_estrangeira;
    nao_capturadas++;
    _Unwind_RaiseException(&h->unwind);
    /* Si vuelve, nadie la recoge: como en g++, se termina sin destruir nada. */
    __cxa_begin_catch(&h->unwind);
    std::terminate();
}

void *__cxa_get_exception_ptr(void *ue) noexcept
{
    return de_unwind(static_cast<_Unwind_Exception *>(ue))->ajustado;
}

void *__cxa_begin_catch(void *uev) noexcept
{
    _Unwind_Exception *ue = static_cast<_Unwind_Exception *>(uev);
    if (ue->exception_class != CLASSE_CXX) return nullptr;   /* de fuera: catch (...) sin objeto */
    cabecalho *h = de_unwind(ue);
    int n = h->manejadores;
    n = n < 0 ? -n + 1 : n + 1;      /* relanzada y recogida otra vez: vuelve a ser normal */
    h->manejadores = n;
    if (h != capturadas) {
        h->proximo = capturadas;
        capturadas = h;
    }
    if (nao_capturadas > 0) nao_capturadas--;
    return h->ajustado;
}

void __cxa_end_catch()
{
    cabecalho *h = capturadas;
    if (!h) return;
    int n = h->manejadores;
    if (n < 0) {                     /* relanzada: sigue viva, la recogerá otro */
        if (++n == 0) capturadas = h->proximo;
    } else if (--n == 0) {
        capturadas = h->proximo;
        destruir(h);
        return;
    }
    h->manejadores = n;
}

[[noreturn]] void __cxa_rethrow()
{
    cabecalho *h = capturadas;
    if (!h) std::terminate();        /* «throw;» fuera de un catch */
    h->manejadores = -h->manejadores;
    nao_capturadas++;
    _Unwind_Resume_or_Rethrow(&h->unwind);
    __cxa_begin_catch(&h->unwind);
    std::terminate();
}

void *__cxa_current_exception_type() noexcept
{
    return capturadas ? const_cast<tipo_info *>(capturadas->tipo) : nullptr;
}

[[noreturn]] void __cxa_call_unexpected(void *) { std::terminate(); }
[[noreturn]] void __cxa_call_terminate(void *) noexcept { std::terminate(); }
[[noreturn]] void __cxa_throw_bad_array_new_length() { parar("C++: new[] con un tamaño imposible"); }
[[noreturn]] void __cxa_bad_cast() { parar("C++: dynamic_cast imposible"); }
[[noreturn]] void __cxa_bad_typeid() { parar("C++: typeid de un puntero nulo"); }

/* ------------------------------------------------------------ la personalidad */

enum achado_t { NADA, LIMPEZA, MANEJADOR, TERMINAR };

/* Lee la LSDA del cuadro y dice qué hay para el IP actual. */
static achado_t ler_lsda(_Unwind_Context *ctx, bool nossa, cabecalho *h,
                         uintptr_t *landing, int *seletor, void **ajustado)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(_Unwind_GetLanguageSpecificData(ctx));
    if (!p) return NADA;
    uintptr_t inicio = _Unwind_GetRegionStart(ctx);
    uintptr_t ip = _Unwind_GetIP(ctx) - 1;

    uintptr_t lp_inicio = inicio;
    uint8_t cod = *p++;
    if (cod != DW_EH_PE_omit && !dw_ler_ponteiro(&p, cod, inicio, &lp_inicio)) return TERMINAR;
    uint8_t cod_tipos = *p++;
    const uint8_t *tipos = nullptr;
    if (cod_tipos != DW_EH_PE_omit) {
        uintptr_t desl = dw_ler_uleb(&p);
        tipos = p + desl;
    }
    uint8_t cod_cs = *p++;
    uintptr_t tam_cs = dw_ler_uleb(&p);
    const uint8_t *acoes = p + tam_cs;

    while (p < acoes) {
        uintptr_t ini, len, lp;
        if (!dw_ler_ponteiro(&p, cod_cs, 0, &ini) || !dw_ler_ponteiro(&p, cod_cs, 0, &len)
            || !dw_ler_ponteiro(&p, cod_cs, 0, &lp)) return TERMINAR;
        uintptr_t acao = dw_ler_uleb(&p);
        if (ip < inicio + ini) break;            /* la tabla va ordenada: no está */
        if (ip >= inicio + ini + len) continue;
        if (lp == 0) return NADA;                /* nada que hacer aquí */
        *landing = lp_inicio + lp;
        if (acao == 0) { *seletor = 0; return LIMPEZA; }

        bool limpeza = false;
        const uint8_t *a = acoes + acao - 1;
        unsigned tam = dw_tamanho_codificacao(cod_tipos);
        for (;;) {
            const uint8_t *q = a;
            intptr_t filtro = dw_ler_sleb(&q);
            const uint8_t *pos_desl = q;
            intptr_t desl = dw_ler_sleb(&q);
            if (filtro > 0) {                    /* un catch */
                if (!tipos || !tam) return TERMINAR;
                const uint8_t *t = tipos - filtro * static_cast<intptr_t>(tam);
                uintptr_t ti;
                if (!dw_ler_ponteiro(&t, cod_tipos, 0, &ti)) return TERMINAR;
                const tipo_info *tc = reinterpret_cast<const tipo_info *>(ti);
                if (!tc) {                       /* catch (...) */
                    *seletor = static_cast<int>(filtro);
                    *ajustado = nossa ? objeto_de(h) : nullptr;
                    return MANEJADOR;
                }
                if (nossa && casa(tc, h->tipo)) {
                    *seletor = static_cast<int>(filtro);
                    void *obj = objeto_de(h);
                    /* Para un puntero, el catch recibe el puntero, no dónde está. */
                    *ajustado = e_ponteiro(h->tipo) ? *static_cast<void **>(obj) : obj;
                    return MANEJADOR;
                }
            } else if (filtro == 0) {
                limpeza = true;
            } else {                             /* especificación throw(...) */
                bool permitida = false;
                if (nossa && tipos) {
                    const uint8_t *e = tipos + (-filtro - 1);
                    for (;;) {
                        uintptr_t idx = dw_ler_uleb(&e);
                        if (idx == 0) break;
                        const uint8_t *t = tipos - static_cast<intptr_t>(idx) * static_cast<intptr_t>(tam);
                        uintptr_t ti;
                        if (!dw_ler_ponteiro(&t, cod_tipos, 0, &ti)) break;
                        if (casa(reinterpret_cast<const tipo_info *>(ti), h->tipo)) { permitida = true; break; }
                    }
                }
                if (!permitida) {
                    *seletor = static_cast<int>(filtro);
                    *ajustado = nossa ? objeto_de(h) : nullptr;
                    return MANEJADOR;
                }
            }
            if (desl == 0) break;
            a = pos_desl + desl;
        }
        if (limpeza) { *seletor = 0; return LIMPEZA; }
        return NADA;
    }
    /* Un IP que no está en la tabla: la función no deja pasar excepciones
     * (noexcept): hay que terminar. */
    return TERMINAR;
}

_Unwind_Reason_Code __gxx_personality_v0(int versao, _Unwind_Action acoes, uint64_t classe,
                                         _Unwind_Exception *ue, _Unwind_Context *ctx)
{
    if (versao != 1 || !ue || !ctx) return _URC_FATAL_PHASE1_ERROR;
    bool nossa = (classe == CLASSE_CXX);
    cabecalho *h = nossa ? de_unwind(ue) : nullptr;

    /* Fase 2 en el cuadro del manejador: lo que se encontró en la fase 1. */
    if ((acoes & _UA_CLEANUP_PHASE) && (acoes & _UA_HANDLER_FRAME) && nossa) {
        if (h->landing == 0) std::terminate();
        _Unwind_SetGR(ctx, 0, reinterpret_cast<uintptr_t>(ue));            /* EAX */
        _Unwind_SetGR(ctx, 2, static_cast<uintptr_t>(h->seletor));          /* EDX */
        _Unwind_SetIP(ctx, h->landing);
        return _URC_INSTALL_CONTEXT;
    }

    uintptr_t landing = 0;
    int seletor = 0;
    void *ajustado = nullptr;
    achado_t a = ler_lsda(ctx, nossa, h, &landing, &seletor, &ajustado);

    if (acoes & _UA_SEARCH_PHASE) {
        if (a == MANEJADOR || a == TERMINAR) {
            if (nossa) {
                h->seletor = seletor;
                h->landing = (a == TERMINAR) ? 0 : landing;
                h->ajustado = ajustado;
            }
            return _URC_HANDLER_FOUND;
        }
        return _URC_CONTINUE_UNWIND;
    }

    /* Fase 2 en un cuadro intermedio: solo los destructores. */
    if (a == TERMINAR) std::terminate();
    if (a == LIMPEZA) {
        _Unwind_SetGR(ctx, 0, reinterpret_cast<uintptr_t>(ue));
        _Unwind_SetGR(ctx, 2, 0);
        _Unwind_SetIP(ctx, landing);
        return _URC_INSTALL_CONTEXT;
    }
    return _URC_CONTINUE_UNWIND;
}

} // extern "C"
