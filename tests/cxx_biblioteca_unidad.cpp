// SPDX-License-Identifier: GPL-2.0-or-later
// cxx_biblioteca_unidad.cpp - prueba de unidad en el PC de las cabeceras de C++ del SaguiOS (D230, h230).
//
//   g++ -std=gnu++17 -nostdinc++ -I userland/libcxx/include -o cxx_biblioteca_unidad
//       tests/cxx_biblioteca_unidad.cpp /usr/lib/x86_64-linux-gnu/libstdc++.so.6 -lm
//
// Las cabeceras de userland/libcxx/include (<string>, <chrono>, <ratio>,
// <stdexcept>, <cstdlib>/<cmath> con std::) con la libc y el runtime del PC
// (el runtime propio ya tiene su prueba, cxxrt_unidad.cpp): lo que se
// comprueba es la lógica de las plantillas, que es la misma en el SaguiOS.
// Escribe "todo bien" y sale con 0 si pasa todo.
#include <string>
#include <chrono>
#include <ratio>
#include <stdexcept>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <utility>

static int fallos, total;
#define COMPROBAR(c, ...)                                                                                              \
    do {                                                                                                               \
        total++;                                                                                                       \
        if (!(c)) {                                                                                                    \
            fallos++;                                                                                                  \
            std::printf("FALLO %s:%d: ", __FILE__, __LINE__);                                                          \
            std::printf(__VA_ARGS__);                                                                                  \
            std::printf("\n");                                                                                         \
        }                                                                                                              \
    } while (0)

static void prueba_construir()
{
    std::string v;
    COMPROBAR(v.empty() && v.size() == 0 && v.c_str()[0] == 0, "vacía, con su cero");
    std::string a("hola"), b(a), c(3, 'x'), d("abcdef", 3), e(a, 1, 2);
    COMPROBAR(a == "hola" && b == a && c == "xxx" && d == "abc" && e == "ol", "constructores");
    std::string m(std::move(b));
    COMPROBAR(m == "hola" && b.empty() && b.c_str()[0] == 0, "mover deja el origen vacío y válido");
    std::string f = {'a', 'b'};
    COMPROBAR(f == "ab", "lista de inicialización");
    const char txt[] = "xyz";
    std::string g(txt, txt + 3);
    COMPROBAR(g == "xyz", "por iteradores");
}

static void prueba_modificar()
{
    std::string s = "Vanilla";
    s += ' ';
    s += "Conquer";
    s.append(3, '!');
    COMPROBAR(s == "Vanilla Conquer!!!", "+= y append (%s)", s.c_str());
    s.erase(s.size() - 3);
    s.insert(0, "[");
    s.push_back(']');
    COMPROBAR(s == "[Vanilla Conquer]", "erase, insert y push_back (%s)", s.c_str());
    s.replace(1, 7, "Tiberian");
    COMPROBAR(s == "[Tiberian Conquer]", "replace (%s)", s.c_str());
    s.append(s);   // a sí misma
    COMPROBAR(s == "[Tiberian Conquer][Tiberian Conquer]", "append de sí misma (%s)", s.c_str());
    s.assign(s, 1, 8);
    COMPROBAR(s == "Tiberian", "assign de un trozo de sí misma (%s)", s.c_str());
    std::string grande;
    for (int i = 0; i < 1000; i++) grande += static_cast<char>('a' + i % 26);
    COMPROBAR(grande.size() == 1000 && grande[999] == 'a' + 999 % 26 && grande.capacity() >= 1000, "crece");
    // El uso de common/ini.cpp: un búfer escrito por C en &s[0] hasta capacity().
    std::string buf(128, '\0');
    std::snprintf(&buf[0], buf.capacity(), "%s=%d", "Clave", 42);
    buf.resize(std::strlen(&buf[0]));
    COMPROBAR(buf == "Clave=42" && buf.size() == 8, "escribir en &s[0] y resize (%s)", buf.c_str());
    std::string t = "abc";
    t.resize(5, 'z');
    COMPROBAR(t == "abczz", "resize con relleno");
    t.clear();
    COMPROBAR(t.empty() && t.c_str()[0] == 0, "clear");
}

static void prueba_buscar()
{
    const std::string s = "/jogos/CnC/gdi/GENERAL.MIX";
    COMPROBAR(s.find("CnC") == 7, "find");
    COMPROBAR(s.find('/') == 0 && s.find('/', 1) == 6, "find de un carácter");
    COMPROBAR(s.rfind('/') == 14 && s.find_last_of("/") == 14, "rfind y find_last_of");
    COMPROBAR(s.find_first_of("/", 1) == 6, "find_first_of desde una posición (paths_posix)");
    COMPROBAR(s.find("nada") == std::string::npos && s.find(':') == std::string::npos, "npos si no está");
    COMPROBAR(s.substr(0, s.find_last_of("/")) == "/jogos/CnC/gdi", "substr hasta la última barra");
    COMPROBAR(s.find_first_not_of("/jog") == 5 && s.find_last_not_of("XIM.") == 21, "find_*_not_of");
    COMPROBAR(s.rfind("gdi") == 11 && s.rfind("gdi", 5) == std::string::npos, "rfind de una cadena");
    // El uso de video_sdl2.cpp: «16:10» partido por los dos puntos.
    std::string ar = "16:10";
    size_t p = ar.find(":");
    float r = std::stof(ar.substr(0, p)) / std::stof(ar.substr(p + 1, ar.length() - p));
    COMPROBAR(r > 1.59f && r < 1.61f, "stof de los trozos (%f)", r);
}

static void prueba_comparar()
{
    std::string a = "abc", b = "abd";
    COMPROBAR(a < b && b > a && a <= a && a >= a && a != b && a == "abc" && "abc" == a, "operadores");
    COMPROBAR(a.compare(b) < 0 && b.compare(a) > 0 && a.compare("abc") == 0, "compare");
    std::string alto = "\xE9";   // é en Latin-1: mayor que 'z' como unsigned char
    COMPROBAR(alto > "z", "compara como unsigned char, como memcmp");
    std::string x = std::string("a") + "b" + 'c' + std::string("d");
    COMPROBAR(x == "abcd", "operator+ (%s)", x.c_str());
    COMPROBAR(std::string("pre") + "fijo" == "prefijo" && "a" + std::string("b") == "ab", "operator+ con char*");
}

static void prueba_numeros()
{
    COMPROBAR(std::to_string(-42) == "-42" && std::to_string(7u) == "7" && std::to_string(123456789012LL) == "123456789012",
              "to_string");
    size_t idx = 0;
    COMPROBAR(std::stoi(std::string("  17x"), &idx) == 17 && idx == 4, "stoi con idx (%zu)", idx);
    COMPROBAR(std::stoi(std::string("ff"), nullptr, 16) == 255, "stoi en base 16");
    COMPROBAR(std::stod(std::string("2.5")) == 2.5, "stod");
    bool lancou = false;
    try {
        std::stoi(std::string("nada"));
    } catch (const std::invalid_argument &e) {
        lancou = std::strcmp(e.what(), "stoi") == 0 || std::strstr(e.what(), "sto") != nullptr;
    }
    COMPROBAR(lancou, "stoi de algo que no es un número lanza invalid_argument");
    lancou = false;
    try {
        std::string("abc").at(3);
    } catch (const std::logic_error &e) {   // out_of_range, por su base
        lancou = true;
    }
    COMPROBAR(lancou, "at() fuera de rango lanza out_of_range (recogida por su base)");
    lancou = false;
    try {
        throw std::runtime_error(std::string("con std::string"));
    } catch (const std::exception &e) {
        lancou = std::strcmp(e.what(), "con std::string") == 0;
    }
    COMPROBAR(lancou, "runtime_error con std::string, recogida como std::exception");
}

static void prueba_cabeceras_c()
{
    COMPROBAR(std::abs(-3) == 3 && std::abs(-2.5) == 2.5 && std::abs(-1.5f) == 1.5f && std::abs(-7L) == 7L, "std::abs");
    COMPROBAR(std::pow(2.0, 10.0) == 1024.0 && std::sqrt(16.0f) == 4.0f, "std::pow y std::sqrt");
    COMPROBAR(std::isnan(static_cast<double>(NAN)) && !isnan(1.0), "std::isnan y el isnan global");
    char d[8];
    std::memcpy(d, "abc", 4);
    COMPROBAR(std::strcmp(d, "abc") == 0 && std::getenv("NO_EXISTE_SAGUI_230") == nullptr, "std::memcpy, std::getenv");
}

static void prueba_chrono()
{
    using namespace std::chrono;
    COMPROBAR(duration_cast<milliseconds>(seconds(3)).count() == 3000, "segundos a milisegundos");
    COMPROBAR(duration_cast<seconds>(milliseconds(2999)).count() == 2, "trunca como el estándar");
    COMPROBAR(duration_cast<microseconds>(nanoseconds(1500)).count() == 1, "nanosegundos a microsegundos");
    microseconds us = milliseconds(5);   // la conversión exacta, implícita
    COMPROBAR(us.count() == 5000, "milisegundos a microsegundos, implícita");
    auto a = steady_clock::now();
    auto b = steady_clock::now();
    COMPROBAR(b >= a && duration_cast<milliseconds>(b - a).count() >= 0, "steady_clock no va hacia atrás");
    // El uso de common/framelimit.cpp.
    auto inicio = steady_clock::now();
    unsigned int min_frame = 8333;
    inicio += microseconds{min_frame};
    COMPROBAR(duration_cast<microseconds>(inicio - a).count() >= 8333, "time_point += microseconds");
    long long ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    COMPROBAR(ms > 1500000000000LL, "system_clock en milisegundos desde 1970 (%lld)", ms);
    COMPROBAR((std::ratio<10, 4>::num == 5 && std::ratio<10, 4>::den == 2), "ratio se reduce");
}

int main()
{
    prueba_construir();
    prueba_modificar();
    prueba_buscar();
    prueba_comparar();
    prueba_numeros();
    prueba_cabeceras_c();
    prueba_chrono();
    std::printf("%d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) std::printf("todo bien\n");
    return fallos ? 1 : 0;
}
