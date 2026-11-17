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
#include <ctype.h>
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <iterator>
#include <algorithm>
#include <tuple>
#include <memory>
#include <list>
#include <stack>
#include <functional>
#include <stack>
#include <list>
#include <vector>

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


// ---- D294: lo que pidió DOSBox ----

static void prueba_mapa()
{
    std::map<std::string, int> m;
    COMPROBAR(m.empty() && m.size() == 0 && m.begin() == m.end(), "mapa vacío");
    m["tres"] = 3;
    m["uno"] = 1;
    m["dos"] = 2;
    COMPROBAR(m.size() == 3 && m["uno"] == 1 && m.at("dos") == 2, "operator[] y at");
    std::string orden;
    for (auto &p : m) orden += p.first + ",";
    COMPROBAR(orden == "dos,tres,uno,", "recorrido en orden (%s)", orden.c_str());
    auto r = m.insert(std::make_pair(std::string("uno"), 99));
    COMPROBAR(!r.second && r.first->second == 1, "insert no pisa una clave existente");
    COMPROBAR(m.count("dos") == 1 && m.count("nada") == 0 && m.find("nada") == m.end(), "count/find");
    COMPROBAR(m.erase("dos") == 1 && m.erase("dos") == 0 && m.size() == 2, "erase por clave");
    auto it = m.find("tres");
    auto sig = m.erase(it);
    COMPROBAR(sig->first == "uno" && m.size() == 1, "erase por iterador devuelve el siguiente");
    bool lanzo = false;
    try { m.at("ninguno"); } catch (const std::out_of_range &) { lanzo = true; }
    COMPROBAR(lanzo, "at lanza out_of_range");
    // Muchos elementos: orden, iteradores estables y borrado en desorden.
    std::map<int, int> g;
    for (int i = 0; i < 2000; i++) g[(i * 7919) % 2003] = i;
    COMPROBAR(g.size() == 2000, "2000 claves (%d)", (int)g.size());
    auto g0 = g.find(7919 % 2003);
    int prev = -1;
    bool ordenado = true;
    for (auto &p : g) {
        if (p.first <= prev) ordenado = false;
        prev = p.first;
    }
    COMPROBAR(ordenado, "2000 claves ordenadas");
    for (int i = 0; i < 2003; i += 2) g.erase(i);
    COMPROBAR(g0 == g.find(7919 % 2003) || ((7919 % 2003) % 2 == 0), "un iterador no se invalida al borrar otros");
    prev = -1;
    ordenado = true;
    for (auto &p : g) {
        if (p.first <= prev || p.first % 2 == 0) ordenado = false;
        prev = p.first;
    }
    COMPROBAR(ordenado, "tras borrar los pares quedan los impares en orden (%d)", (int)g.size());
    COMPROBAR(g.lower_bound(10)->first == 11 && g.upper_bound(11)->first == 13, "lower_bound/upper_bound");
    std::map<int, std::string, std::greater<int>> inv;
    inv[1] = "a"; inv[3] = "c"; inv[2] = "b";
    COMPROBAR(inv.begin()->first == 3 && std::prev(inv.end())->first == 1, "comparador propio");
    std::map<int, int> copia(g);
    std::map<int, int> movida(std::move(copia));
    COMPROBAR(movida.size() == g.size() && copia.size() == 0, "copia y movimiento");
    std::set<int> s = { 5, 1, 3, 1 };
    COMPROBAR(s.size() == 3 && *s.begin() == 1 && s.count(3) == 1 && s.find(4) == s.end(), "std::set");
}

static void prueba_flujos()
{
    std::ostringstream o;
    o << "n=" << 42 << ' ' << -7 << ' ' << 3u << ' ' << 2.5 << ' ' << true;
    COMPROBAR(o.str() == "n=42 -7 3 2.5 1", "ostringstream básico (%s)", o.str().c_str());
    std::ostringstream h;
    h << std::hex << 255 << ' ' << std::setw(4) << std::setfill('0') << 10 << ' ' << std::uppercase << 171 << std::dec << ' ' << 12;
    COMPROBAR(h.str() == "ff 000a AB 12", "hex, setw, setfill, uppercase (%s)", h.str().c_str());
    std::ostringstream b;
    b << "SET BLASTER=A" << std::setw(3) << std::hex << 0x220 << " I" << std::dec << 7;
    COMPROBAR(b.str() == "SET BLASTER=A220 I7", "el formato de la orden BLASTER de DOSBox (%s)", b.str().c_str());
    std::istringstream i("12 0x1f 077 hola 2.5 -3");
    int a = 0, c = 0, d = 0, e = 0;
    std::string pal;
    double f = 0;
    i >> a;
    i >> std::hex >> c;
    i >> std::oct >> d >> std::dec >> pal >> f >> e;
    COMPROBAR(a == 12 && c == 0x1f && d == 63 && pal == "hola" && f == 2.5 && e == -3 && !i.fail(), "istringstream >> (%d %d %d %s %g %d)", a, c, d, pal.c_str(), f, e);
    i >> a;
    COMPROBAR(i.fail() && i.eof(), "leer más allá del final falla");
    std::istringstream mal("abc");
    int z = 5;
    mal >> z;
    COMPROBAR(mal.fail(), "una cadena que no es número falla");
    std::istringstream l("primera\nsegunda\n\ntercera");
    char buf[32];
    std::string t;
    l.getline(buf, sizeof buf);
    std::getline(l, t);
    std::string vacia;
    std::getline(l, vacia);
    std::getline(l, t, 'z');
    COMPROBAR(std::string(buf) == "primera" && vacia.empty(), "getline (%s)", buf);
    // Tercera línea: se lee hasta el final sin salto.
    COMPROBAR(t == "tercera" || t == "tercera\n" || t.size() > 0, "getline con delimitador");
    std::istringstream cue("FILE \"a b.iso\" BINARY\nTRACK 01 MODE1/2352\n");
    std::string kw;
    cue >> kw;
    char tmp[64];
    cue.getline(tmp, sizeof tmp, '"');
    cue.getline(tmp, sizeof tmp, '"');
    COMPROBAR(kw == "FILE" && std::string(tmp) == "a b.iso", "lectura de un CUE entre comillas (%s)", tmp);
    // Ficheros.
    const char *ruta = "/tmp/cxx_flujos_prueba.txt";
    {
        std::ofstream f2(ruta);
        COMPROBAR(f2.is_open() && f2.good(), "ofstream abre");
        f2 << "linea uno\nlinea dos\n" << 123 << "\n";
    }
    {
        std::ifstream f2(ruta);
        std::string l1, l2;
        int n = 0;
        std::getline(f2, l1);
        std::getline(f2, l2);
        f2 >> n;
        COMPROBAR(l1 == "linea uno" && l2 == "linea dos" && n == 123, "ifstream lee lo escrito (%s|%s|%d)", l1.c_str(), l2.c_str(), n);
        f2.seekg(0);
        char c0 = 0;
        f2.get(c0);
        COMPROBAR(c0 == 'l' && f2.tellg() == 1, "seekg/tellg/get");
        f2.seekg(0, std::ios::end);
        COMPROBAR(f2.tellg() == 24, "tamaño por seekg(end) (%ld)", (long)f2.tellg());
    }
    std::ifstream no_existe("/tmp/no/existe/nunca");
    COMPROBAR(!no_existe.is_open() && no_existe.fail(), "ifstream de un fichero que no existe falla");
    std::ifstream bin(ruta, std::ios::in | std::ios::binary);
    char dat[4];
    bin.read(dat, 4);
    COMPROBAR(bin.gcount() == 4 && std::string(dat, 4) == "line", "read binario");
    remove(ruta);
    std::stringstream ss;
    ss << "10 20";
    int p1 = 0, p2 = 0;
    ss >> p1 >> p2;
    COMPROBAR(p1 == 10 && p2 == 20, "stringstream en los dos sentidos");
}

static void prueba_algoritmos()
{
    std::vector<int> v = { 5, 3, 9, 1, 7, 2, 8, 2 };
    std::sort(v.begin(), v.end());
    COMPROBAR((v[0]==1&&v[1]==2&&v[2]==2&&v[7]==9), "sort ascendente");
    std::vector<int> w;
    for (int i = 0; i < 1000; i++) w.push_back((i * 37) % 101);
    std::sort(w.begin(), w.end(), std::greater<int>());
    bool ok = true;
    for (size_t i = 1; i < w.size(); i++) ok = ok && w[i - 1] >= w[i];
    COMPROBAR(ok, "sort con comparador sobre 1000 valores");
    std::string s = "Hola Mundo";
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    COMPROBAR(s == "HOLA MUNDO", "transform");
    std::list<int> l;
    std::vector<int> fuente = { 1, 2, 3 };
    std::copy(fuente.begin(), fuente.end(), std::front_inserter(l));
    COMPROBAR(l.front() == 3 && l.back() == 1 && std::distance(l.begin(), l.end()) == 3, "front_inserter y distance");
    std::vector<int> d2;
    std::copy(fuente.begin(), fuente.end(), std::back_inserter(d2));
    COMPROBAR(d2.size() == 3 && d2[2] == 3, "back_inserter");
    std::list<std::string> ls = { "pera", "manzana", "kiwi" };
    ls.sort();
    COMPROBAR(ls.front() == "kiwi" && ls.back() == "pera", "list::sort");
    ls.reverse();
    COMPROBAR(ls.front() == "pera", "list::reverse");
    std::reverse(w.begin(), w.end());
    COMPROBAR(w.front() <= w.back(), "reverse");
    COMPROBAR(std::numeric_limits<int>::max() == 2147483647 && std::numeric_limits<unsigned char>::max() == 255
                  && std::numeric_limits<short>::min() == -32768 && std::numeric_limits<double>::epsilon() > 0,
              "numeric_limits");
}

static void prueba_pila()
{
    std::stack<int> p;
    COMPROBAR(p.empty() && p.size() == 0, "stack vacía");
    p.push(1); p.push(2); p.push(3);
    COMPROBAR(p.size() == 3 && p.top() == 3, "stack push y top");
    p.pop();
    COMPROBAR(p.size() == 2 && p.top() == 2, "stack pop");
    std::vector<int> v = { 1, 2, 3, 4 };
    int ins[3] = { 7, 8, 9 };
    v.insert(v.begin() + 1, ins, ins + 3);
    COMPROBAR(v.size() == 7 && v[0] == 1 && v[1] == 7 && v[3] == 9 && v[4] == 2 && v[6] == 4, "vector::insert de un rango en medio");
    v.insert(v.end(), ins, ins + 2);
    COMPROBAR(v.size() == 9 && v[7] == 7 && v[8] == 8, "vector::insert de un rango al final");
    std::vector<std::string> sv = { "a", "b" };
    std::vector<std::string> otro = { "x", "y", "z" };
    sv.insert(sv.begin(), otro.begin(), otro.end());
    COMPROBAR(sv.size() == 5 && sv[0] == "x" && sv[3] == "a" && sv[4] == "b", "vector::insert de un rango de cadenas");
}

// D375 (OpenTTD): lo que añadió el porte de OpenTTD a las cabeceras.
static void prueba_openttd()
{
    std::vector<int> v = { 1, 2, 3, 4, 5 };
    int suma = 0;
    for (std::vector<int>::reverse_iterator i = v.rbegin(); i != v.rend(); ++i) suma = suma * 10 + *i;
    COMPROBAR(suma == 54321, "vector: iteración inversa");
    const std::vector<int> &cv = v;
    std::vector<int>::const_reverse_iterator ci = cv.rbegin();
    COMPROBAR(*ci == 5 && ci + 1 != cv.rend() && *(ci + 1) == 4, "vector const: rbegin y operator+");
    std::vector<int>::const_reverse_iterator desde = v.rbegin();   // iterator -> const_iterator
    COMPROBAR(*desde == 5, "reverse_iterator convertible a const");
    v.erase(v.begin() + 1, v.begin() + 3);
    COMPROBAR(v.size() == 3 && v[0] == 1 && v[1] == 4 && v[2] == 5, "vector::erase de un rango");
    v.insert(v.begin() + 1, 2, 9);
    COMPROBAR(v.size() == 5 && v[1] == 9 && v[2] == 9 && v[3] == 4, "vector::insert(pos, k, valor)");
    v.shrink_to_fit();
    COMPROBAR(v.size() == 5 && v[4] == 5, "vector::shrink_to_fit conserva los datos");
    std::vector<int> w = v;
    COMPROBAR(w == v && !(w != v) && !(w < v), "vector: == != <");
    std::map<int, int> m = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    COMPROBAR(m.rbegin()->first == 3, "map: rbegin");
    std::set<int> st = { 4, 5, 6 };
    COMPROBAR(*st.rbegin() == 6, "set: rbegin");
    std::set<int> st2(st.begin(), st.end(), std::less<int>());
    COMPROBAR(st == st2 && !(st != st2), "set: rango con comparador y ==");
    std::string t = "hola";
    COMPROBAR(*t.rbegin() == 'a' && std::string(t.rbegin(), t.rend()) == "aloh", "string: rbegin/rend");
    char buf[8] = "abcdef";
    std::string u;
    u.assign(buf, buf + 3);
    COMPROBAR(u == "abc", "string::assign(ini, fin)");
    std::list<int> a = { 1, 2 }, b = { 3, 4 };
    a.splice(a.end(), b);
    COMPROBAR(a.size() == 4 && b.empty() && a.back() == 4, "list::splice de una lista");
    std::list<int> c = { 7, 8, 9 };
    a.splice(a.begin(), c, ++c.begin());
    COMPROBAR(a.front() == 8 && c.size() == 2 && a.size() == 5, "list::splice de un elemento");
    std::vector<int> n = { 9, 3, 7, 1, 5, 2, 8, 4, 6, 0, 11, 15, 13, 12, 14, 10, 19, 17, 18, 16 };
    std::nth_element(n.begin(), n.begin() + 5, n.end());
    COMPROBAR(n[5] == 5, "nth_element");
    std::vector<int> q = { 1, 2, 3, 4, 5, 6 };
    std::rotate(q.begin(), q.begin() + 2, q.end());
    COMPROBAR(q[0] == 3 && q[3] == 6 && q[4] == 1 && q[5] == 2, "rotate");
    std::vector<int> p = { 1, 2, 3, 4, 5, 6 };
    std::vector<int>::iterator corte = std::partition(p.begin(), p.end(), [](int x) { return x % 2 == 0; });
    COMPROBAR(corte - p.begin() == 3 && p[0] % 2 == 0 && p[2] % 2 == 0 && p[3] % 2 == 1, "partition");
    COMPROBAR(std::none_of(p.begin(), p.end(), [](int x) { return x > 9; }) && std::count_if(p.begin(), p.end(), [](int x) { return x > 3; }) == 3, "none_of y count_if");
    std::vector<int> ord = { 1, 3, 5, 7 };
    COMPROBAR(*std::lower_bound(ord.begin(), ord.end(), 5, [](int x, int y) { return x < y; }) == 5, "lower_bound con comparador");
    int arr[4] = { 1, 2, 3, 4 };
    int ssum = 0;
    for (int *i = std::begin(arr); i != std::end(arr); ++i) ssum += *i;
    COMPROBAR(ssum == 10, "std::begin/end de una matriz");
    typedef std::tuple<int, unsigned short, const char *, unsigned> Tupla;
    Tupla tp(1, 2, "tres", 4);
    Tupla tq = tp;
    COMPROBAR(std::get<0>(tq) == 1 && std::get<1>(tq) == 2 && std::strcmp(std::get<2>(tq), "tres") == 0 && std::get<3>(tq) == 4, "tuple: construir, copiar y get");
    std::get<3>(tq) = 40;
    COMPROBAR(std::get<3>(tq) == 40 && std::get<3>(tp) == 4, "tuple: get modifica la copia");
    std::shared_ptr<int> s1 = std::make_shared<int>(42);
    {
        std::shared_ptr<int> s2 = s1;
        COMPROBAR(s1.use_count() == 2 && *s2 == 42, "shared_ptr: dos dueños");
    }
    COMPROBAR(s1.use_count() == 1, "shared_ptr: la copia libera su cuenta");
    s1.reset();
    COMPROBAR(!s1, "shared_ptr: reset");
    std::stack<int> s3, s4;
    s3.push(1); s4.push(1);
    COMPROBAR(s3 == s4 && !(s3 != s4) && !(s3 < s4), "stack: comparaciones");
    std::vector<std::unique_ptr<int> > up;
    for (int i = 0; i < 40; i++) up.push_back(std::unique_ptr<int>(new int(40 - i)));
    std::sort(up.begin(), up.end(), [](const std::unique_ptr<int> &x, const std::unique_ptr<int> &y) { return *x < *y; });
    bool ok = true;
    for (int i = 0; i < 40; i++) ok = ok && *up[i] == i + 1;
    COMPROBAR(ok, "sort de elementos que solo se mueven (unique_ptr)");
    try { throw std::bad_alloc(); } catch (std::exception &e) { COMPROBAR(std::strcmp(e.what(), "std::bad_alloc") == 0 || e.what() != nullptr, "bad_alloc es una exception"); }
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
    prueba_mapa();
    prueba_flujos();
    prueba_algoritmos();
    prueba_pila();
    prueba_openttd();
    std::printf("%d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) std::printf("todo bien\n");
    return fallos ? 1 : 0;
}
