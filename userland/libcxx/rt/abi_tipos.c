/* abi_tipos.c - los typeinfo que el compilador no genera (D228).
 *
 * g++ genera el typeinfo de cada clase que se lanza o se recoge, pero el de
 * los tipos fundamentales (int, char, double...) y el de los punteros a ellos
 * los da por hechos en la biblioteca de C++: «throw 3» hace referencia a
 * _ZTIi, el typeinfo de int. Aquí están, con los nombres del ABI de Itanium.
 *
 * Cada typeinfo empieza con un puntero a la tabla virtual de su clase (dos
 * palabras dentro de ella, como hace g++). Las tablas no tienen funciones de
 * verdad: este runtime no usa RTTI (-fno-rtti) y la personalidad
 * (excecoes.cpp) solo compara sus direcciones para saber de qué clase es un
 * typeinfo. Se definen todas las que g++ puede nombrar para que enlacen.
 *
 * Está en C para poder escribir los nombres del ABI tal cual.
 */
#define TABELA(nome) const void *nome[8] = { 0 }

TABELA(_ZTVN10__cxxabiv117__class_type_infoE);
TABELA(_ZTVN10__cxxabiv120__si_class_type_infoE);
TABELA(_ZTVN10__cxxabiv121__vmi_class_type_infoE);
TABELA(_ZTVN10__cxxabiv123__fundamental_type_infoE);
TABELA(_ZTVN10__cxxabiv119__pointer_type_infoE);
TABELA(_ZTVN10__cxxabiv129__pointer_to_member_type_infoE);
TABELA(_ZTVN10__cxxabiv120__function_type_infoE);
TABELA(_ZTVN10__cxxabiv116__enum_type_infoE);
TABELA(_ZTVN10__cxxabiv117__array_type_infoE);

struct tipo_fundamental { const void *vtabela; const char *nome; };
struct tipo_ponteiro { const void *vtabela; const char *nome; unsigned int qualificadores; const void *apontado; };

/* Para cada tipo fundamental X (con el código del ABI): _ZTSX (el nombre),
 * _ZTIX (el typeinfo), y los de X* y const X*. */
#define FUND(cod) \
    const char _ZTS##cod[] = #cod; \
    const char _ZTSP##cod[] = "P" #cod; \
    const char _ZTSPK##cod[] = "PK" #cod; \
    const struct tipo_fundamental _ZTI##cod = { &_ZTVN10__cxxabiv123__fundamental_type_infoE[2], _ZTS##cod }; \
    const struct tipo_ponteiro _ZTIP##cod = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], _ZTSP##cod, 0, &_ZTI##cod }; \
    const struct tipo_ponteiro _ZTIPK##cod = { &_ZTVN10__cxxabiv119__pointer_type_infoE[2], _ZTSPK##cod, 1, &_ZTI##cod };

FUND(v)   /* void */
FUND(b)   /* bool */
FUND(w)   /* wchar_t */
FUND(c)   /* char */
FUND(a)   /* signed char */
FUND(h)   /* unsigned char */
FUND(s)   /* short */
FUND(t)   /* unsigned short */
FUND(i)   /* int */
FUND(j)   /* unsigned int */
FUND(l)   /* long */
FUND(m)   /* unsigned long */
FUND(x)   /* long long */
FUND(y)   /* unsigned long long */
FUND(f)   /* float */
FUND(d)   /* double */
FUND(e)   /* long double */
FUND(Dn)  /* std::nullptr_t */
FUND(Ds)  /* char16_t */
FUND(Di)  /* char32_t */
FUND(Du)  /* char8_t */
