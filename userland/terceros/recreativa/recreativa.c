// SPDX-License-Identifier: GPL-2.0-or-later
/* recreativa [--pantalla-completa] - Recreativa, el lanzador de juegos de SaguiOS (D294, mejora 119; multisistema en D321).
 *
 * D321: lanzador MULTISISTEMA. Los sistemas (DOS, NES, Master System, Game Gear, Game Boy...) y sus emuladores
 * se registran con ficheros de texto (/etc/recreativa/sistemas/<sistema>.conf y emuladores/<emulador>.conf,
 * docs/recreativa.md): nombre, orden, extensiones de ROM, carpetas, emuladores con su orden de preferencia y sus
 * argumentos. Un emulador cuyo binario no existe no se muestra. A la izquierda va la lista de sistemas («Todos»,
 * «DOS», «NES»...); las ROM aparecen solas por extensión en las carpetas del sistema, se pueden añadir ROM o
 * carpetas a mano (diálogo común D272) y la lista de lo añadido va a /home/.config/recreativa/roms.conf.
 * DOS sigue siendo un sistema más con su juegos.conf de siempre (D294).
 *
 * Una lista de juegos para DOSBox (/apps/bin/dosbox) con el aspecto de Windows 98 de Correo (D289): barra de
 * menús, barra de herramientas con iconos (Jugar, Añadir, Propiedades, Configurar, Quitar, Carpeta), la lista
 * en cuadrícula (con portada opcional) o en detalles, y barra de estado. Es una aplicación no esencial (D194):
 * va a /apps junto con DOSBox.
 *
 * Los juegos se guardan en /home/dos/juegos.conf (texto, una sección [Nombre] por juego; se reescribe al
 * cambiar algo; los datos de los juegos nunca los toca ni los copia). Además aparecen solos las carpetas de
 * /home/dos y de /jogos (lo que se puso en input_share/games) que tienen un .exe, .com o .bat en su raíz; al
 * «Quitar» una de ellas se anota como oculta, sin borrar ningún archivo.
 *
 * Por juego (Propiedades): programa, núcleo de la CPU (auto, normal, dynamic), ciclos, memoria, tarjeta de
 * sonido, joystick (D307: el tipo que emula DOSBox a partir del mando del sistema), un CD (carpeta o imagen
 * ISO/CUE, como D:), órdenes previas y el programa de configuración (setup).
 * Jugar escribe /home/.config/recreativa-juego.conf con eso y lanza «dosbox -conf dosbox.conf -conf juego.conf
 * -noautoexec -c "mount c ..." ...»: el C: es la carpeta del juego y, al acabar, DOSBox se cierra.
 * La portada es opcional: portada.png|jpg|bmp en la carpeta del juego, o /home/dos/portadas/<nombre>.png.
 *
 * Teclas: Intro o doble clic juega; Insert añade; Alt+Intro propiedades; F5 busca otra vez; Supr quita;
 * flechas mueven la selección; Ctrl+L cambia entre cuadrícula y lista. Botón derecho: menú del juego.
 */
#include "gui.h"
#include "icones.h"
#include "imagem.h"
#include "sagui.h"

#define MAX_JUEGOS 400
#define MAX_SIST   16
#define MAX_EMU    6
#define MAX_CARP   24
#define CONF_ROMS  "/home/.config/recreativa/roms.conf"
#define DIR_RC     "/home/.config/recreativa"
#define SIDE_L     144
#define CONF_JUEGOS "/home/dos/juegos.conf"
#define DIR_DOS     "/home/dos"
#define CONF_BASE   "/home/.config/dosbox.conf"
#define CONF_JUEGO  "/home/.config/recreativa-juego.conf"

/* ------------------------------------------------------------- aspecto Windows 98 */
#define C_FACE    0xC0C0C0
#define C_LUZ     0xFFFFFF
#define C_SOMBRA  0x808080
#define C_OSCURO  0x404040
#define C_AZUL    0x000080
#define C_NEGRO   0x000000
#define C_BRANCO  0xFFFFFF

#define ALT_MENU   20
#define TB_Y       ALT_MENU
#define TB_ALT     46
#define CUERPO_Y   (TB_Y + TB_ALT + 2)
#define ALT_ESTADO 22
#define FILA_ALT   20
#define CELDA_L    120
#define CELDA_A    104
#define PORTADA_L  72
#define PORTADA_A  54

static void bisel(jan_t *j, int x, int y, int l, int a, bool hundido)
{
    uint32_t c1 = hundido ? C_SOMBRA : C_LUZ, c2 = hundido ? C_LUZ : C_OSCURO;
    g_retangulo(j, x, y, l, 1, c1);
    g_retangulo(j, x, y, 1, a, c1);
    g_retangulo(j, x, y + a - 1, l, 1, c2);
    g_retangulo(j, x + l - 1, y, 1, a, c2);
    if (!hundido) {
        g_retangulo(j, x + 1, y + a - 2, l - 2, 1, C_SOMBRA);
        g_retangulo(j, x + l - 2, y + 1, 1, a - 2, C_SOMBRA);
    }
}

/* Texto que cabe en maxpx píxeles; si no cabe, acaba en "...". */
static void texto_el(jan_t *j, int x, int y, const char *t, int maxpx, uint32_t cor)
{
    int n = (int)tam_texto(t), cab = maxpx / LETRA_L;
    if (cab <= 0) return;
    if (n <= cab) { g_texto_n(j, x, y, t, n, cor); return; }
    char b[130];
    int k = cab > 3 ? cab - 3 : 0;
    if (k > 120) k = 120;
    for (int i = 0; i < k; i++) b[i] = t[i];
    b[k] = 0;
    if (cab > 3) { b[k] = '.'; b[k + 1] = '.'; b[k + 2] = '.'; b[k + 3] = 0; }
    g_texto(j, x, y, b, cor);
}

/* ------------------------------------------------------------- utilidades de texto */
static void copiar(char *dst, int max, const char *src)
{
    int i = 0;
    for (; src && src[i] && i < max - 1; i++) dst[i] = src[i];
    dst[i] = 0;
}

static char minuscula(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static bool igual_sin_mayus(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (minuscula(*a) != minuscula(*b)) return false;
    return *a == *b;
}

static bool contiene_sin_mayus(const char *t, const char *buscado)
{
    for (; *t; t++) {
        const char *a = t, *b = buscado;
        while (*a && *b && minuscula(*a) == minuscula(*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

static bool termina_en(const char *t, const char *ext)
{
    int n = (int)tam_texto(t), e = (int)tam_texto(ext);
    if (n < e) return false;
    return igual_sin_mayus(t + n - e, ext);
}

static bool es_programa_dos(const char *nombre)
{
    return termina_en(nombre, ".exe") || termina_en(nombre, ".com") || termina_en(nombre, ".bat");
}

static bool es_cd_imagen(const char *ruta) { return termina_en(ruta, ".iso") || termina_en(ruta, ".cue"); }

static bool existe(const char *ruta)
{
    info_arquivo_t inf;
    return info_arquivo(ruta, &inf) >= 0;
}

static bool es_carpeta(const char *ruta)
{
    info_arquivo_t inf;
    return info_arquivo(ruta, &inf) >= 0 && inf.tipo == TIPO_PASTA;
}

static const char *nombre_de(const char *ruta)
{
    const char *p = ruta, *u = ruta;
    for (; *p; p++)
        if (*p == '/' && p[1]) u = p + 1;
    return u;
}

/* Nombre propuesto para una carpeta: «DOOM_II» -> «Doom Ii»... solo si va todo en mayúsculas o en minúsculas. */
static void proponer_nombre(const char *carpeta, char *dst, int max)
{
    char b[64];
    copiar(b, sizeof b, nombre_de(carpeta));
    int n = (int)tam_texto(b);
    if (n > 0 && b[n - 1] == '/') b[--n] = 0;
    bool may = false, min = false;
    for (int i = 0; i < n; i++) {
        if (b[i] >= 'A' && b[i] <= 'Z') may = true;
        if (b[i] >= 'a' && b[i] <= 'z') min = true;
    }
    bool inicio = true;
    for (int i = 0; i < n; i++) {
        if (b[i] == '_' || b[i] == '-') b[i] = ' ';
        if (!(may && min)) {
            if (inicio && b[i] >= 'a' && b[i] <= 'z') b[i] = (char)(b[i] - 32);
            else if (!inicio && b[i] >= 'A' && b[i] <= 'Z') b[i] = (char)(b[i] + 32);
        }
        inicio = (b[i] == ' ');
    }
    copiar(dst, max, b);
}

/* ------------------------------------------------------------- los juegos */
typedef struct {
    char nombre[64];
    char carpeta[128];
    char programa[64];
    char nucleo[12];     /* auto, normal, dynamic */
    char ciclos[12];     /* auto, max o un número */
    char sonido[12];     /* sb16, sbpro2, sb2, sb1, adlib, gus, none */
    char joystick[8];    /* D307: auto, 2axis, 4axis, fcs, ch, none (el joysticktype de DOSBox) */
    char cd[128];
    char antes[128];
    char setup[64];
    int  memoria;        /* MB */
    bool oculto;
    bool automatico;     /* encontrado solo; todavía no se guarda */
    int  sis;            /* D321: índice del sistema en sist[] (el de DOS para los juegos de DOSBox) */
    char rom[128];       /* D321: ruta de la ROM; vacío en los juegos de DOS */
    char emu[16];        /* D321: emulador elegido para este juego ("" = el predeterminado del sistema) */
    bool completa;       /* D321: arrancar a pantalla completa */
} juego_t;

static juego_t juegos[MAX_JUEGOS];
static int n_juegos;

/* ------------------------------------------------------------- registro de sistemas y emuladores (D321) */
typedef struct {
    char id[16], nombre[28], binario[72], args[160], args_completa[160];
    int  prioridad;
    bool defecto, dosbox;
} emu_t;
typedef struct {
    char id[16], nombre[32], icono[24];
    char carpetas[4][80];
    int  n_carp;
    char exts[12][8];
    int  n_ext, orden, hz;
    emu_t emus[MAX_EMU];
    int  n_emu;
} sistema_t;
static sistema_t sist[MAX_SIST];
static int n_sist;
static int sis_dos = -1;       /* índice del sistema «dos» (-1 si no está o no tiene DOSBox) */
static int sist_sel = -1;      /* filtro de la lista: -1 = «Todos» */
static struct { char ruta[96]; int sis; } carp_extra[MAX_CARP];   /* carpetas de ROM añadidas a mano */
static int n_carp_extra;

static const char *const NUCLEOS[] = { "auto", "normal", "dynamic" };
static const char *const CICLOS[] = { "auto", "max", "1000", "3000", "8000", "12000", "20000", "30000", "50000" };
static const char *const MEMORIAS[] = { "4", "8", "16", "32", "64" };
static const char *const SONIDOS[] = { "sb16", "sbpro2", "sb2", "sb1", "adlib", "gus", "none" };
static const char *const SONIDOS_NOMBRE[] = { "Sound Blaster 16", "Sound Blaster Pro", "Sound Blaster 2.0", "Sound Blaster",
                                              "AdLib", "Gravis UltraSound", "Ninguno (solo el altavoz)" };
#define N_NUCLEOS  3
#define N_CICLOS   9
#define N_MEMORIAS 5
#define N_SONIDOS  7
static const char *const JOYSTICKS[] = { "auto", "2axis", "4axis", "fcs", "ch", "none" };
#define N_JOYSTICKS 6

static void juego_defecto(juego_t *g)
{
    for (unsigned i = 0; i < sizeof *g; i++) ((char *)g)[i] = 0;
    copiar(g->nucleo, sizeof g->nucleo, "auto");
    copiar(g->ciclos, sizeof g->ciclos, "auto");
    copiar(g->sonido, sizeof g->sonido, "sb16");
    copiar(g->joystick, sizeof g->joystick, "auto");
    g->memoria = 16;
}

static int juego_por_carpeta(const char *carpeta)
{
    for (int i = 0; i < n_juegos; i++)
        if (!juegos[i].rom[0] && !compara_texto(juegos[i].carpeta, carpeta)) return i;
    return -1;
}

static int cargar_texto(const char *ruta, char *buf, int max)
{
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0) return -1;
    int n = 0, r;
    while (n < max - 1 && (r = ler(fd, buf + n, (unsigned)(max - 1 - n))) > 0) n += r;
    fechar(fd);
    buf[n] = 0;
    return n;
}


/* Siguiente línea de un texto en memoria, ya recortada. false al acabar. */
static bool leer_linea(char **cur, char **ln)
{
    char *p = *cur;
    if (!*p) return false;
    char *fin = p;
    while (*fin && *fin != '\n') fin++;
    char *sig = *fin ? fin + 1 : fin;
    *fin = 0;
    while (fin > p && (fin[-1] == '\r' || fin[-1] == ' ' || fin[-1] == '\t')) *--fin = 0;
    while (*p == ' ' || *p == '\t') p++;
    *ln = p;
    *cur = sig;
    return true;
}

/* Parte «clave=valor» de una línea; el comentario de después del valor (« #...») se corta. */
static bool clave_valor(char *ln, char **clave, char **valor)
{
    if (!*ln || *ln == '#') return false;
    char *eq = ln;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=') return false;
    *eq = 0;
    char *c = eq;
    while (c > ln && (c[-1] == ' ' || c[-1] == '\t')) *--c = 0;
    char *v = eq + 1;
    while (*v == ' ' || *v == '\t') v++;
    for (char *q = v; *q; q++)
        if ((*q == ' ' || *q == '\t') && q[1] == '#') { *q = 0; break; }
    int n = (int)tam_texto(v);
    while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = 0;
    *clave = ln;
    *valor = v;
    return true;
}

static int entero(const char *v)
{
    int n = 0;
    for (; *v >= '0' && *v <= '9'; v++) n = n * 10 + (*v - '0');
    return n;
}

static bool es_si(const char *v) { return v[0] == '1' || igual_sin_mayus(v, "si") || igual_sin_mayus(v, "sí") || igual_sin_mayus(v, "yes") || igual_sin_mayus(v, "true"); }

/* «a,b,c» -> elementos de ancho fijo (un punto inicial se quita: «.nes» vale como «nes»). */
static void dividir_lista(const char *v, char *dst, int ancho, int max, int *n)
{
    *n = 0;
    while (*v && *n < max) {
        while (*v == ' ' || *v == ',') v++;
        if (!*v) break;
        char *d = dst + *n * ancho;
        int k = 0;
        while (*v && *v != ',') { if (k < ancho - 1) d[k++] = *v; v++; }
        while (k > 0 && d[k - 1] == ' ') k--;
        d[k] = 0;
        if (d[0] == '.') for (int i = 0; i <= k; i++) d[i] = d[i + 1];
        if (d[0]) (*n)++;
    }
}

static sistema_t *sistema_por_id(const char *id)
{
    for (int i = 0; i < n_sist; i++)
        if (!compara_texto(sist[i].id, id)) return &sist[i];
    return NULL;
}

static void emu_defecto(emu_t *e)
{
    for (unsigned i = 0; i < sizeof *e; i++) ((char *)e)[i] = 0;
    e->prioridad = 50;
}

/* Un campo de emulador (sirve para los bloques [emulador x] y para emuladores/<x>.conf). */
static void emu_campo(emu_t *e, const char *k, const char *v)
{
    if (!compara_texto(k, "id")) copiar(e->id, sizeof e->id, v);
    else if (!compara_texto(k, "nombre")) copiar(e->nombre, sizeof e->nombre, v);
    else if (!compara_texto(k, "binario")) copiar(e->binario, sizeof e->binario, v);
    else if (!compara_texto(k, "args")) copiar(e->args, sizeof e->args, v);
    else if (!compara_texto(k, "args_completa")) copiar(e->args_completa, sizeof e->args_completa, v);
    else if (!compara_texto(k, "prioridad")) e->prioridad = entero(v);
    else if (!compara_texto(k, "predeterminado")) e->defecto = es_si(v);
    else if (!compara_texto(k, "tipo")) e->dosbox = !compara_texto(v, "dosbox");
}

/* sistemas/<id>.conf: los campos del sistema y bloques [emulador <id>]. Si ya hay uno con ese id, lo reemplaza. */
static void leer_sistema(const char *ruta, const char *id)
{
    static char buf[6144];
    if (cargar_texto(ruta, buf, sizeof buf) <= 0) return;
    sistema_t *si = sistema_por_id(id);
    if (!si) {
        if (n_sist >= MAX_SIST) return;
        si = &sist[n_sist++];
    }
    for (unsigned i = 0; i < sizeof *si; i++) ((char *)si)[i] = 0;
    copiar(si->id, sizeof si->id, id);
    copiar(si->nombre, sizeof si->nombre, id);
    si->orden = 50;
    si->hz = 60;
    emu_t *e = NULL;
    char *cur = buf, *ln, *k, *v;
    while (leer_linea(&cur, &ln)) {
        if (*ln == '[') {
            char *c = ln + 1;
            int n = 0;
            while (c[n] && c[n] != ']') n++;
            c[n] = 0;
            e = NULL;
            if (!compara_texto_n(c, "emulador ", 9) && si->n_emu < MAX_EMU) {
                e = &si->emus[si->n_emu++];
                emu_defecto(e);
                copiar(e->id, sizeof e->id, c + 9);
                copiar(e->nombre, sizeof e->nombre, c + 9);
            }
            continue;
        }
        if (!clave_valor(ln, &k, &v)) continue;
        if (e) { emu_campo(e, k, v); continue; }
        if (!compara_texto(k, "nombre")) copiar(si->nombre, sizeof si->nombre, v);
        else if (!compara_texto(k, "orden")) si->orden = entero(v);
        else if (!compara_texto(k, "icono")) copiar(si->icono, sizeof si->icono, v);
        else if (!compara_texto(k, "frecuencia")) si->hz = entero(v);
        else if (!compara_texto(k, "extensiones")) dividir_lista(v, &si->exts[0][0], 8, 12, &si->n_ext);
        else if (!compara_texto(k, "carpetas")) dividir_lista(v, &si->carpetas[0][0], 80, 4, &si->n_carp);
    }
}

/* emuladores/<x>.conf: un emulador que se suma a los sistemas de «sistemas=a,b». */
static void leer_emulador_suelto(const char *ruta)
{
    static char buf[2048];
    if (cargar_texto(ruta, buf, sizeof buf) <= 0) return;
    emu_t e;
    emu_defecto(&e);
    char lista[8][16];
    int nl = 0;
    char *cur = buf, *ln, *k, *v;
    while (leer_linea(&cur, &ln)) {
        if (!clave_valor(ln, &k, &v)) continue;
        if (!compara_texto(k, "sistemas")) dividir_lista(v, &lista[0][0], 16, 8, &nl);
        else emu_campo(&e, k, v);
    }
    if (!e.id[0] || !e.binario[0]) return;
    if (!e.nombre[0]) copiar(e.nombre, sizeof e.nombre, e.id);
    for (int i = 0; i < nl; i++) {
        sistema_t *si = sistema_por_id(lista[i]);
        if (!si || si->n_emu >= MAX_EMU) continue;
        bool ya = false;
        for (int j = 0; j < si->n_emu; j++)
            if (!compara_texto(si->emus[j].id, e.id)) ya = true;
        if (!ya) si->emus[si->n_emu++] = e;
    }
}

static void leer_directorio(const char *dir, bool emuladores)
{
    entrada_pasta_t en;
    for (int i = 0; listar(dir, i, &en) > 0 && i < 64; i++) {
        if (en.tipo == TIPO_PASTA || !termina_en(en.nome, ".conf") || en.nome[0] == '.') continue;
        char ruta[120], id[16];
        caminho_juntar(ruta, sizeof ruta, dir, en.nome);
        copiar(id, sizeof id, en.nome);
        id[tam_texto(id) - 5 < 15 ? tam_texto(id) - 5 : 15] = 0;
        if (emuladores) leer_emulador_suelto(ruta);
        else leer_sistema(ruta, id);
    }
}

static bool tiene_binario(const emu_t *e) { return e->binario[0] && existe(e->binario); }

/* Carga el registro: sistemas, emuladores sueltos; quita los emuladores sin binario y los sistemas sin emulador. */
static void cargar_registro(void)
{
    n_sist = 0;
    leer_directorio("/etc/recreativa/sistemas", false);
    leer_directorio(DIR_RC "/sistemas", false);
    leer_directorio("/etc/recreativa/emuladores", true);
    leer_directorio(DIR_RC "/emuladores", true);
    int k = 0;
    for (int i = 0; i < n_sist; i++) {
        sistema_t *si = &sist[i];
        int m = 0;
        for (int j = 0; j < si->n_emu; j++)
            if (tiene_binario(&si->emus[j])) si->emus[m++] = si->emus[j];
        si->n_emu = m;
        if (!m) continue;
        if (k != i) sist[k] = *si;
        k++;
    }
    n_sist = k;
    for (int i = 1; i < n_sist; i++) {              /* por orden (inserción: pocos sistemas) */
        sistema_t t = sist[i];
        int j = i - 1;
        while (j >= 0 && (sist[j].orden > t.orden)) { sist[j + 1] = sist[j]; j--; }
        sist[j + 1] = t;
    }
    sis_dos = -1;
    for (int i = 0; i < n_sist; i++)
        if (!compara_texto(sist[i].id, "dos")) sis_dos = i;
    if (sist_sel >= n_sist) sist_sel = -1;
}

/* El emulador de un juego: el elegido, si no el marcado como predeterminado y si no el de menor prioridad. */
static const emu_t *emu_de(const sistema_t *si, const char *elegido)
{
    if (elegido && elegido[0])
        for (int i = 0; i < si->n_emu; i++)
            if (!compara_texto(si->emus[i].id, elegido)) return &si->emus[i];
    for (int i = 0; i < si->n_emu; i++)
        if (si->emus[i].defecto) return &si->emus[i];
    const emu_t *m = &si->emus[0];
    for (int i = 1; i < si->n_emu; i++)
        if (si->emus[i].prioridad < m->prioridad) m = &si->emus[i];
    return m;
}

/* ¿La extensión de este archivo es de ROM del sistema? */
static int sistema_de_archivo(const char *nombre, int solo)
{
    const char *pt = NULL;
    for (const char *q = nombre; *q; q++)
        if (*q == '.') pt = q + 1;
    if (!pt || !*pt) return -1;
    for (int i = 0; i < n_sist; i++) {
        if (i == sis_dos || (solo >= 0 && i != solo)) continue;
        for (int j = 0; j < sist[i].n_ext; j++)
            if (igual_sin_mayus(pt, sist[i].exts[j])) return i;
    }
    return -1;
}

/* Lee juegos.conf: «[Nombre]» y «clave=valor». */
static void cargar_conf(void)
{
    static char buf[24576];
    n_juegos = 0;
    if (cargar_texto(CONF_JUEGOS, buf, sizeof buf) <= 0) return;
    juego_t *g = NULL;
    for (char *p = buf; *p;) {
        char *fin = p;
        while (*fin && *fin != '\n') fin++;
        char tmp = *fin;
        *fin = 0;
        if (fin > p && fin[-1] == '\r') fin[-1] = 0;
        if (*p == '[' && n_juegos < MAX_JUEGOS) {
            g = &juegos[n_juegos++];
            juego_defecto(g);
            g->sis = sis_dos;
            char *c = p + 1;
            int k = 0;
            while (c[k] && c[k] != ']') k++;
            c[k] = 0;
            copiar(g->nombre, sizeof g->nombre, c);
        } else if (g && *p && *p != '#') {
            char *eq = p;
            while (*eq && *eq != '=') eq++;
            if (*eq == '=') {
                *eq = 0;
                const char *v = eq + 1;
                if (!compara_texto(p, "carpeta")) copiar(g->carpeta, sizeof g->carpeta, v);
                else if (!compara_texto(p, "programa")) copiar(g->programa, sizeof g->programa, v);
                else if (!compara_texto(p, "nucleo")) copiar(g->nucleo, sizeof g->nucleo, v);
                else if (!compara_texto(p, "ciclos")) copiar(g->ciclos, sizeof g->ciclos, v);
                else if (!compara_texto(p, "sonido")) copiar(g->sonido, sizeof g->sonido, v);
                else if (!compara_texto(p, "joystick")) copiar(g->joystick, sizeof g->joystick, v);
                else if (!compara_texto(p, "cd")) copiar(g->cd, sizeof g->cd, v);
                else if (!compara_texto(p, "antes")) copiar(g->antes, sizeof g->antes, v);
                else if (!compara_texto(p, "setup")) copiar(g->setup, sizeof g->setup, v);
                else if (!compara_texto(p, "memoria")) {
                    int m = 0;
                    for (const char *d = v; *d >= '0' && *d <= '9'; d++) m = m * 10 + (*d - '0');
                    if (m >= 1 && m <= 63 + 1) g->memoria = m;
                } else if (!compara_texto(p, "oculto")) g->oculto = (v[0] == '1');
            }
        }
        *fin = tmp;
        p = tmp ? fin + 1 : fin;
    }
}

static void guardar_conf(void)
{
    static char buf[24576];
    int n = 0;
    n += formatar_texto(buf + n, sizeof buf - (unsigned)n,
                        "# Recreativa (D294): lista de juegos de DOS. Se reescribe al cambiar algo; los archivos de\n"
                        "# los juegos no se tocan nunca. Un juego con oculto=1 es una carpeta encontrada que se quitó.\n\n");
    for (int i = 0; i < n_juegos && n < (int)sizeof buf - 800; i++) {
        juego_t *g = &juegos[i];
        if (g->automatico || g->rom[0]) continue;
        if (g->oculto) {
            n += formatar_texto(buf + n, sizeof buf - (unsigned)n, "[%s]\ncarpeta=%s\noculto=1\n\n", g->nombre, g->carpeta);
            continue;
        }
        n += formatar_texto(buf + n, sizeof buf - (unsigned)n,
                            "[%s]\ncarpeta=%s\nprograma=%s\nnucleo=%s\nciclos=%s\nmemoria=%d\nsonido=%s\njoystick=%s\ncd=%s\nantes=%s\nsetup=%s\n\n",
                            g->nombre, g->carpeta, g->programa, g->nucleo, g->ciclos, g->memoria, g->sonido, g->joystick, g->cd,
                            g->antes, g->setup);
    }
    criar_pasta("/home");
    criar_pasta(DIR_DOS);
    int fd = abrir(CONF_JUEGOS, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) { registrar("[RECREATIVA] no se puede escribir %s", CONF_JUEGOS); return; }
    escrever_tudo(fd, buf, (unsigned)n);
    fechar(fd);
}


/* ------------------------------------------------------------- ROM de consolas (D321) */
static int juego_por_rom(const char *rom)
{
    for (int i = 0; i < n_juegos; i++)
        if (juegos[i].rom[0] && !compara_texto(juegos[i].rom, rom)) return i;
    return -1;
}

static void carpeta_de(const char *ruta, char *dst, int max)
{
    copiar(dst, max, ruta);
    int n = (int)tam_texto(dst);
    while (n > 1 && dst[n - 1] != '/') n--;
    if (n > 1) n--;
    dst[n] = 0;
}

/* Nombre propuesto para una ROM: su archivo sin la extensión, como se hace con las carpetas de DOS. */
static void proponer_nombre_rom(const char *rom, char *dst, int max)
{
    char b[128];
    copiar(b, sizeof b, rom);
    int n = (int)tam_texto(b);
    while (n > 0 && b[n - 1] != '.' && b[n - 1] != '/') n--;
    if (n > 0 && b[n - 1] == '.') b[n - 1] = 0;
    proponer_nombre(b, dst, max);
}

static void juego_rom_nuevo(juego_t *g, int sis, const char *rom)
{
    juego_defecto(g);
    g->sis = sis;
    copiar(g->rom, sizeof g->rom, rom);
    carpeta_de(rom, g->carpeta, sizeof g->carpeta);
    proponer_nombre_rom(rom, g->nombre, sizeof g->nombre);
}

/* roms.conf: [Nombre] con sistema, rom, emulador, completa, oculto; y [@carpeta] con sistema. */
static void cargar_roms(void)
{
    static char buf[24576];
    n_carp_extra = 0;
    if (cargar_texto(CONF_ROMS, buf, sizeof buf) <= 0) return;
    juego_t *g = NULL;
    int carp = -1;
    char *cur = buf, *ln, *k, *v;
    char nombre[64] = "";
    while (leer_linea(&cur, &ln)) {
        if (*ln == '[') {
            char *c = ln + 1;
            int n = 0;
            while (c[n] && c[n] != ']') n++;
            c[n] = 0;
            g = NULL;
            carp = -1;
            if (c[0] == '@') {
                if (n_carp_extra < MAX_CARP) {
                    carp = n_carp_extra++;
                    copiar(carp_extra[carp].ruta, sizeof carp_extra[carp].ruta, c + 1);
                    carp_extra[carp].sis = -1;
                }
            } else {
                copiar(nombre, sizeof nombre, c);
                g = NULL;
            }
            continue;
        }
        if (!clave_valor(ln, &k, &v)) continue;
        if (carp >= 0) {
            if (!compara_texto(k, "sistema")) {
                sistema_t *si = sistema_por_id(v);
                carp_extra[carp].sis = si ? (int)(si - sist) : -1;
            }
            continue;
        }
        if (!compara_texto(k, "rom") && n_juegos < MAX_JUEGOS && nombre[0]) {
            g = &juegos[n_juegos++];
            juego_defecto(g);
            g->sis = -1;
            copiar(g->rom, sizeof g->rom, v);
            carpeta_de(v, g->carpeta, sizeof g->carpeta);
            copiar(g->nombre, sizeof g->nombre, nombre);
        } else if (g) {
            if (!compara_texto(k, "sistema")) {
                sistema_t *si = sistema_por_id(v);
                g->sis = si ? (int)(si - sist) : -1;
            } else if (!compara_texto(k, "emulador")) copiar(g->emu, sizeof g->emu, v);
            else if (!compara_texto(k, "completa")) g->completa = es_si(v);
            else if (!compara_texto(k, "oculto")) g->oculto = es_si(v);
        }
    }
    /* Los de un sistema que ya no está (o sin binario) se descartan en la lista, no del fichero. */
}

static void guardar_roms(void)
{
    static char buf[24576];
    int n = formatar_texto(buf, sizeof buf,
                           "# Recreativa (D321): ROM de consolas añadidas, cambiadas u ocultas, y carpetas de ROM añadidas.\n"
                           "# Los archivos de las ROM no se tocan nunca. Se reescribe al cambiar algo.\n\n");
    for (int i = 0; i < n_carp_extra && n < (int)sizeof buf - 300; i++) {
        if (carp_extra[i].sis < 0) continue;
        n += formatar_texto(buf + n, sizeof buf - (unsigned)n, "[@%s]\nsistema=%s\n\n", carp_extra[i].ruta, sist[carp_extra[i].sis].id);
    }
    for (int i = 0; i < n_juegos && n < (int)sizeof buf - 600; i++) {
        juego_t *g = &juegos[i];
        if (!g->rom[0] || g->sis < 0) continue;
        if (g->automatico && !g->oculto) continue;     /* los encontrados solos y sin tocar no se guardan */
        n += formatar_texto(buf + n, sizeof buf - (unsigned)n, "[%s]\nrom=%s\nsistema=%s\nemulador=%s\ncompleta=%d\noculto=%d\n\n",
                            g->nombre, g->rom, sist[g->sis].id, g->emu, g->completa ? 1 : 0, g->oculto ? 1 : 0);
    }
    criar_pasta("/home");
    criar_pasta("/home/.config");
    criar_pasta(DIR_RC);
    int fd = abrir(CONF_ROMS, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) { registrar("[RECREATIVA] no se puede escribir %s", CONF_ROMS); return; }
    escrever_tudo(fd, buf, (unsigned)n);
    fechar(fd);
}

/* ROM de un sistema en una carpeta y, hasta «prof» niveles, en sus subcarpetas. */
static void escanear_roms(int sis, const char *dir, int prof)
{
    entrada_pasta_t e;
    char c[128];
    for (int i = 0; listar(dir, i, &e) > 0 && i < 512 && n_juegos < MAX_JUEGOS; i++) {
        if (e.nome[0] == '.') continue;
        caminho_juntar(c, sizeof c, dir, e.nome);
        if (e.tipo == TIPO_PASTA) {
            if (prof > 0 && !igual_sin_mayus(e.nome, "portadas") && !igual_sin_mayus(e.nome, "lost+found")) escanear_roms(sis, c, prof - 1);
            continue;
        }
        if (sistema_de_archivo(e.nome, sis) != sis) continue;
        if (juego_por_rom(c) >= 0) continue;
        juego_rom_nuevo(&juegos[n_juegos], sis, c);
        juegos[n_juegos].automatico = true;
        n_juegos++;
    }
}

/* Las ROM de las carpetas de cada sistema y de las carpetas añadidas a mano. */
static void buscar_roms(void)
{
    for (int s = 0; s < n_sist; s++) {
        if (s == sis_dos) continue;
        for (int c = 0; c < sist[s].n_carp; c++) escanear_roms(s, sist[s].carpetas[c], 2);
    }
    for (int i = 0; i < n_carp_extra; i++)
        if (carp_extra[i].sis >= 0 && carp_extra[i].sis < n_sist) escanear_roms(carp_extra[i].sis, carp_extra[i].ruta, 2);
    /* Las guardadas de un sistema que ya no está se quitan de la lista. */
    int k = 0;
    for (int i = 0; i < n_juegos; i++) {
        if (juegos[i].rom[0] && juegos[i].sis < 0) continue;
        if (k != i) juegos[k] = juegos[i];
        k++;
    }
    n_juegos = k;
}

/* ------------------------------------------------------------- detectar programas */
/* Programas de la raíz de una carpeta. Devuelve cuántos; el mejor (no setup/instalar...) en *mejor. */
#define MAX_PROGS 48
static char progs[MAX_PROGS][64];
static int n_progs;

static bool es_auxiliar(const char *n)
{
    static const char *const MALOS[] = { "setup", "install", "instal", "config", "uninst", "readme", "help", "patch", "crack",
                                         "sound", "setsound", "dos4gw", "pkunzip", "unzip", "cwsdpmi", "dosxmsf" };
    for (unsigned i = 0; i < sizeof MALOS / sizeof MALOS[0]; i++)
        if (contiene_sin_mayus(n, MALOS[i])) return true;
    return false;
}

static int buscar_programas(const char *carpeta, int *mejor)
{
    n_progs = 0;
    *mejor = -1;
    entrada_pasta_t e;
    for (int i = 0; listar(carpeta, i, &e) > 0 && i < 512 && n_progs < MAX_PROGS; i++) {
        if (e.tipo == TIPO_PASTA || !es_programa_dos(e.nome)) continue;
        copiar(progs[n_progs++], 64, e.nome);
    }
    char base[64];
    proponer_nombre(carpeta, base, sizeof base);
    int puntos = -1;
    for (int i = 0; i < n_progs; i++) {
        int p = 1;
        if (es_auxiliar(progs[i])) p = 0;
        else {
            if (termina_en(progs[i], ".exe")) p += 2;
            else if (termina_en(progs[i], ".com")) p += 1;
            if (contiene_sin_mayus(base, progs[i])) p += 3;
        }
        if (p > puntos) { puntos = p; *mejor = i; }
    }
    return n_progs;
}

/* Un setup entre los programas de la carpeta (setup.exe, install.exe...). */
static void buscar_setup(const char *carpeta, char *dst, int max)
{
    dst[0] = 0;
    entrada_pasta_t e;
    for (int i = 0; listar(carpeta, i, &e) > 0 && i < 512; i++) {
        if (e.tipo == TIPO_PASTA || !es_programa_dos(e.nome)) continue;
        if (contiene_sin_mayus(e.nome, "setup") || contiene_sin_mayus(e.nome, "install") || contiene_sin_mayus(e.nome, "config")) {
            copiar(dst, max, e.nome);
            return;
        }
    }
}

static bool carpeta_excluida(const char *nombre)
{
    static const char *const FUERA[] = { "chip8", "sagui3d", "jclic", "opentyrian", "openjazz", "quake", "conquer", "gnuboy",
                                         "capturas", "portadas", "lost+found" };
    for (unsigned i = 0; i < sizeof FUERA / sizeof FUERA[0]; i++)
        if (igual_sin_mayus(nombre, FUERA[i])) return true;
    return nombre[0] == '.';
}

/* Busca carpetas con programas de DOS en /home/dos y /jogos y las añade como automáticas. */
static void buscar_automaticos(void)
{
    if (sis_dos < 0) return;
    static const char *const RAICES[] = { DIR_DOS, "/jogos" };
    for (unsigned r = 0; r < 2; r++) {
        entrada_pasta_t e;
        for (int i = 0; listar(RAICES[r], i, &e) > 0 && i < 256; i++) {
            if (e.tipo != TIPO_PASTA || carpeta_excluida(e.nome) || n_juegos >= MAX_JUEGOS) continue;
            char c[128];
            caminho_juntar(c, sizeof c, RAICES[r], e.nome);
            if (juego_por_carpeta(c) >= 0) continue;
            int mejor;
            if (buscar_programas(c, &mejor) <= 0 || mejor < 0) continue;
            juego_t *g = &juegos[n_juegos++];
            juego_defecto(g);
            g->sis = sis_dos;
            proponer_nombre(c, g->nombre, sizeof g->nombre);
            copiar(g->carpeta, sizeof g->carpeta, c);
            copiar(g->programa, sizeof g->programa, progs[mejor]);
            buscar_setup(c, g->setup, sizeof g->setup);
            g->automatico = true;
        }
    }
}

/* ------------------------------------------------------------- lanzar DOSBox */
static int pid_dosbox;         /* el emulador en marcha (cualquiera; el nombre histórico es de D294) */
static char emu_activo[28];
static char estado[160] = "Listo";

static void definir_estado(const char *t) { copiar(estado, sizeof estado, t); }

static const char *sonido_nombre(const char *clave)
{
    for (int i = 0; i < N_SONIDOS; i++)
        if (!compara_texto(SONIDOS[i], clave)) return SONIDOS_NOMBRE[i];
    return clave;
}

/* Escribe /home/.config/recreativa-juego.conf con lo del juego. */
static bool escribir_conf_juego(const juego_t *g)
{
    char buf[1400];
    int n = 0;
    const char *sb = "sb16", *opl = "auto";
    bool gus = false;
    if (!compara_texto(g->sonido, "sbpro2")) sb = "sbpro2";
    else if (!compara_texto(g->sonido, "sb2")) sb = "sb2";
    else if (!compara_texto(g->sonido, "sb1")) sb = "sb1";
    else if (!compara_texto(g->sonido, "adlib")) { sb = "none"; opl = "opl2"; }
    else if (!compara_texto(g->sonido, "gus")) { sb = "none"; opl = "none"; gus = true; }
    else if (!compara_texto(g->sonido, "none")) { sb = "none"; opl = "none"; }
    n += formatar_texto(buf + n, sizeof buf - (unsigned)n,
                        "# Generado por Recreativa para %s: se reescribe en cada partida.\n[cpu]\ncore=%s\ncycles=%s\n\n"
                        "[dosbox]\nmemsize=%d\n\n[sblaster]\nsbtype=%s\noplmode=%s\n\n[gus]\ngus=%s\n\n[joystick]\njoysticktype=%s\n",
                        g->nombre, g->nucleo, g->ciclos, g->memoria, sb, opl, gus ? "true" : "false",
                        g->joystick[0] ? g->joystick : "auto");
    criar_pasta("/home");
    criar_pasta("/home/.config");
    int fd = abrir(CONF_JUEGO, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) return false;
    escrever_tudo(fd, buf, (unsigned)n);
    fechar(fd);
    return true;
}

/* Lanza DOSBox con el juego (setup = true: su programa de configuración). */
static void lanzar(const juego_t *g, bool setup, bool completa)
{
    const char *prog = setup ? g->setup : g->programa;
    if (!prog[0]) { mensagem("Recreativa", setup ? "Este juego no tiene programa de configuración." : "Este juego no tiene programa."); return; }
    if (!existe("/apps/bin/dosbox")) { mensagem("Recreativa", "No se encuentra /apps/bin/dosbox."); return; }
    if (!es_carpeta(g->carpeta)) { mensagem("Recreativa", "La carpeta del juego ya no existe."); return; }
    if (!escribir_conf_juego(g)) { mensagem("Recreativa", "No se puede escribir la configuración (¿/home de solo lectura?)."); return; }
    static char a_montar[200], a_cd[300], a_antes[160], a_prog[100];
    char *av[40];
    int n = 0;
    av[n++] = "dosbox";
    if (completa) av[n++] = "--pantalla-completa";
    if (existe(CONF_BASE)) { av[n++] = "-conf"; av[n++] = CONF_BASE; }
    av[n++] = "-conf"; av[n++] = CONF_JUEGO;
    av[n++] = "-noautoexec";
    formatar_texto(a_montar, sizeof a_montar, "mount c \"%s\"", g->carpeta);
    av[n++] = "-c"; av[n++] = a_montar;
    av[n++] = "-c"; av[n++] = "c:";
    if (g->cd[0] && existe(g->cd)) {
        if (es_cd_imagen(g->cd)) formatar_texto(a_cd, sizeof a_cd, "imgmount d \"%s\" -t iso", g->cd);
        else formatar_texto(a_cd, sizeof a_cd, "mount d \"%s\" -t cdrom", g->cd);
        av[n++] = "-c"; av[n++] = a_cd;
    }
    if (g->antes[0]) { copiar(a_antes, sizeof a_antes, g->antes); av[n++] = "-c"; av[n++] = a_antes; }
    copiar(a_prog, sizeof a_prog, prog);
    av[n++] = "-c"; av[n++] = a_prog;
    av[n++] = "-c"; av[n++] = "exit";
    av[n] = NULL;
    pid_dosbox = executar("/apps/bin/dosbox", av);
    copiar(emu_activo, sizeof emu_activo, "DOSBox");
    registrar("[RECREATIVA] %s %s en %s (pid %d)", setup ? "configurar" : "jugar", prog, g->carpeta, pid_dosbox);
    char m[160];
    formatar_texto(m, sizeof m, pid_dosbox > 0 ? "DOSBox en marcha: %s" : "No se ha podido lanzar DOSBox (%s)", g->nombre);
    definir_estado(m);
}


/* D321: sustituye {rom}, {nombre}, {carpeta} y {estados} de una plantilla de argumentos; cada palabra de la
 * plantilla (separadas por espacios) es un argumento, así que una ruta con espacios no se parte. */
static void expandir(const char *plantilla, const juego_t *g, const emu_t *e, char *dst, int max)
{
    int n = 0;
    for (const char *p = plantilla; *p && n < max - 1;) {
        const char *clave = NULL, *valor = "";
        char estados[64];
        if (!compara_texto_n(p, "{rom}", 5)) { clave = "{rom}"; valor = g->rom; }
        else if (!compara_texto_n(p, "{nombre}", 8)) { clave = "{nombre}"; valor = g->nombre; }
        else if (!compara_texto_n(p, "{carpeta}", 9)) { clave = "{carpeta}"; valor = g->carpeta; }
        else if (!compara_texto_n(p, "{estados}", 9)) {
            formatar_texto(estados, sizeof estados, "/home/.config/%s", e->id);
            clave = "{estados}"; valor = estados;
        }
        if (clave) {
            for (const char *q = valor; *q && n < max - 1; q++) dst[n++] = *q;
            p += tam_texto(clave);
        } else {
            dst[n++] = *p++;
        }
    }
    dst[n] = 0;
}

/* Lanza el emulador del sistema con la ROM del juego (D321). */
static void lanzar_rom(const juego_t *g, bool completa)
{
    if (g->sis < 0 || g->sis >= n_sist) return;
    const sistema_t *si = &sist[g->sis];
    const emu_t *e = emu_de(si, g->emu);
    if (!existe(g->rom)) { mensagem("Recreativa", "La ROM ya no existe."); return; }
    if (!tiene_binario(e)) { mensagem("Recreativa", "No se encuentra el emulador."); return; }
    static char linea[400];
    static char *av[24];
    int n = 0;
    av[n++] = (char *)nome_base(e->binario);
    bool comp = completa || g->completa;
    expandir((comp && e->args_completa[0]) ? e->args_completa : e->args, g, e, linea, sizeof linea);
    for (char *p = linea; *p && n < 22;) {
        while (*p == ' ') p++;
        if (!*p) break;
        av[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    av[n] = NULL;
    pid_dosbox = executar(e->binario, av);
    copiar(emu_activo, sizeof emu_activo, e->nombre);
    registrar("[RECREATIVA] jugar %s con %s (%s, pid %d)", g->rom, e->id, si->id, pid_dosbox);
    char m[160];
    formatar_texto(m, sizeof m, pid_dosbox > 0 ? "%s en marcha: %s" : "No se ha podido lanzar %s (%s)", e->nombre, g->nombre);
    definir_estado(m);
}

/* ------------------------------------------------------------- ventana principal */
static jan_t *jan;
static int sel = -1;
static bool vista_lista;
static int topo;               /* primera fila/celda visible (por filas de celdas en la cuadrícula) */
static int visibles[MAX_JUEGOS];   /* índices en juegos[] de los que no están ocultos */
static int n_vis;
static imagem_t portadas[MAX_JUEGOS];
static bool portada_probada[MAX_JUEGOS], portada_hay[MAX_JUEGOS];
static int cuerpo_x, cuerpo_y, cuerpo_l, cuerpo_a;

typedef struct { int x, l; const char *texto; const char *icono; int accion; } herr_t;
enum { A_JUGAR = 1, A_ANADIR, A_PROP, A_CONFIG, A_QUITAR, A_CARPETA, A_BUSCAR, A_VISTA, A_COMPLETA, A_SALIR, A_ACERCA,
       A_ANADIR_ROM, A_ANADIR_CARP, A_SIGUIENTE };
static herr_t tb[6] = {
    { 0, 0, "Jugar", "jogo", A_JUGAR },
    { 0, 0, "Añadir", "documento-novo", A_ANADIR },
    { 0, 0, "Propiedades", "ajustes", A_PROP },
    { 0, 0, "Configurar", "chave-inglesa", A_CONFIG },
    { 0, 0, "Quitar", "excluir", A_QUITAR },
    { 0, 0, "Carpeta", "pasta", A_CARPETA },
};
static int tb_apertado = -1;
static const char *const menu_nombres[3] = { "Juego", "Ver", "Ayuda" };
static int menu_x[3], menu_l[3];
static const char *const menu_textos[3] = {
    "Jugar\tIntro\nJugar a pantalla completa\nConfigurar (setup de DOS)\n-\nAñadir juego de DOS...\tInsert\nAñadir ROM...\nAñadir carpeta de ROM...\nPropiedades...\tAlt+Intro\nQuitar de la lista\tSupr\nAbrir carpeta\n-\nSalir",
    "Cuadrícula / lista\tCtrl+L\nBuscar juegos otra vez\tF5\nSiguiente sistema\tF6",
    "Acerca de Recreativa",
};
static const int menu_acciones[3][12] = {
    { A_JUGAR, A_COMPLETA, A_CONFIG, 0, A_ANADIR, A_ANADIR_ROM, A_ANADIR_CARP, A_PROP, A_QUITAR, A_CARPETA, 0, A_SALIR },
    { A_VISTA, A_BUSCAR, A_SIGUIENTE },
    { A_ACERCA },
};
#define MENU_CTX "Jugar\nJugar a pantalla completa\nConfigurar (setup de DOS)\n-\nPropiedades...\nQuitar de la lista\nAbrir carpeta"
static const int ctx_acciones[7] = { A_JUGAR, A_COMPLETA, A_CONFIG, 0, A_PROP, A_QUITAR, A_CARPETA };
static int menu_pendiente;     /* 0 = ninguno; 1 = menú de la barra (tabla), 2 = contextual */
static int menu_cual;

static void reconstruir_visibles(void)
{
    n_vis = 0;
    for (int i = 0; i < n_juegos; i++)
        if (!juegos[i].oculto && juegos[i].sis >= 0 && (sist_sel < 0 || juegos[i].sis == sist_sel)) visibles[n_vis++] = i;
    if (sel >= n_vis) sel = n_vis - 1;
    if (sel < 0 && n_vis) sel = 0;
}

static juego_t *seleccionado(void) { return (sel >= 0 && sel < n_vis) ? &juegos[visibles[sel]] : NULL; }

static void liberar_portadas(void)
{
    for (int i = 0; i < MAX_JUEGOS; i++) {
        if (portada_hay[i]) imagem_liberar(&portadas[i]);
        portada_hay[i] = portada_probada[i] = false;
    }
}

static void buscar_juegos(void)
{
    juego_t *s = seleccionado();
    char antes[128];
    copiar(antes, sizeof antes, s ? (s->rom[0] ? s->rom : s->carpeta) : "");
    liberar_portadas();
    cargar_registro();
    cargar_conf();
    cargar_roms();
    buscar_automaticos();
    buscar_roms();
    reconstruir_visibles();
    sel = n_vis ? 0 : -1;
    for (int i = 0; i < n_vis; i++)
        if (!compara_texto(juegos[visibles[i]].rom[0] ? juegos[visibles[i]].rom : juegos[visibles[i]].carpeta, antes)) sel = i;
}

/* La portada del juego i (índice en juegos[]), ya a PORTADA_L x PORTADA_A; NULL si no hay. */
static const imagem_t *portada_de(int i)
{
    if (!portada_probada[i]) {
        portada_probada[i] = true;
        static const char *const EXT[] = { "png", "jpg", "bmp", "gif" };
        char ruta[200];
        imagem_t grande;
        for (int k = 0; k < 8 && !portada_hay[i]; k++) {
            if (juegos[i].rom[0]) {
                /* ROM: una imagen con el mismo nombre que ella (en su carpeta) o /home/juegos/portadas/<nombre>. */
                char base[128];
                copiar(base, sizeof base, juegos[i].rom);
                int bn = (int)tam_texto(base);
                while (bn > 0 && base[bn - 1] != '.' && base[bn - 1] != '/') bn--;
                if (bn > 0 && base[bn - 1] == '.') base[bn - 1] = 0;
                if (k < 4) formatar_texto(ruta, sizeof ruta, "%s.%s", base, EXT[k]);
                else formatar_texto(ruta, sizeof ruta, "/home/juegos/portadas/%s.%s", juegos[i].nombre, EXT[k - 4]);
            } else if (k < 4) formatar_texto(ruta, sizeof ruta, "%s/portada.%s", juegos[i].carpeta, EXT[k]);
            else formatar_texto(ruta, sizeof ruta, "/home/dos/portadas/%s.%s", juegos[i].nombre, EXT[k - 4]);
            if (!existe(ruta) || imagem_ler(ruta, &grande) != 0) continue;
            if (imagem_escalar(&portadas[i], &grande, PORTADA_L, PORTADA_A, true)) portada_hay[i] = true;
            imagem_liberar(&grande);
        }
    }
    return portada_hay[i] ? &portadas[i] : NULL;
}

static int columnas(void)
{
    int c = (cuerpo_l - 8) / CELDA_L;
    return c < 1 ? 1 : c;
}

static int filas_visibles(void)
{
    int a = vista_lista ? (cuerpo_a - 22) / FILA_ALT : (cuerpo_a - 4) / CELDA_A;
    return a < 1 ? 1 : a;
}

static int total_filas(void) { return vista_lista ? n_vis : (n_vis + columnas() - 1) / columnas(); }

static void asegurar_visible(void)
{
    if (sel < 0) return;
    int fila = vista_lista ? sel : sel / columnas();
    if (fila < topo) topo = fila;
    if (fila >= topo + filas_visibles()) topo = fila - filas_visibles() + 1;
    if (topo > total_filas() - filas_visibles()) topo = total_filas() - filas_visibles();
    if (topo < 0) topo = 0;
}

static void layout(void)
{
    int x = 4;
    for (int i = 0; i < 6; i++) {
        int l = (int)tam_texto(tb[i].texto) * LETRA_L + 16;
        if (l < 60) l = 60;
        tb[i].x = x;
        tb[i].l = l;
        x += l;
        if (i == 0 || i == 3) x += 10;
    }
    x = 4;
    for (int i = 0; i < 3; i++) {
        menu_x[i] = x;
        menu_l[i] = (int)tam_texto(menu_nombres[i]) * LETRA_L + 16;
        x += menu_l[i];
    }
    cuerpo_x = 4 + SIDE_L + 4;
    cuerpo_y = CUERPO_Y;
    cuerpo_l = jan->larg - cuerpo_x - 4;
    cuerpo_a = jan->alt - CUERPO_Y - ALT_ESTADO - 4;
    asegurar_visible();
}

static void dibujar_cuerpo(void)
{
    g_retangulo(jan, cuerpo_x, cuerpo_y, cuerpo_l, cuerpo_a, C_BRANCO);
    bisel(jan, cuerpo_x - 1, cuerpo_y - 1, cuerpo_l + 2, cuerpo_a + 2, true);
    if (!n_vis) {
        g_texto(jan, cuerpo_x + 16, cuerpo_y + 16, "No hay juegos todavía.", C_NEGRO);
        if (sist_sel >= 0 && sist_sel != sis_dos) {
            char t[120];
            formatar_texto(t, sizeof t, "Pulse «Añadir» y elija una ROM de %s,", sist[sist_sel].nombre);
            g_texto(jan, cuerpo_x + 16, cuerpo_y + 40, t, C_SOMBRA);
            texto_el(jan, cuerpo_x + 16, cuerpo_y + 58, sist[sist_sel].n_carp ? "o copie las ROM a:" : "o copie las ROM a una carpeta.", cuerpo_l - 32, C_SOMBRA);
            for (int c = 0; c < sist[sist_sel].n_carp && c < 3; c++) texto_el(jan, cuerpo_x + 32, cuerpo_y + 76 + c * 18, sist[sist_sel].carpetas[c], cuerpo_l - 48, C_SOMBRA);
        } else {
            g_texto(jan, cuerpo_x + 16, cuerpo_y + 40, "Pulse «Añadir» y elija la carpeta de un juego de DOS,", C_SOMBRA);
            g_texto(jan, cuerpo_x + 16, cuerpo_y + 58, "o copie sus juegos a /home/dos (una carpeta por juego)", C_SOMBRA);
            g_texto(jan, cuerpo_x + 16, cuerpo_y + 76, "y las ROM de consolas a /home/juegos/<sistema>.", C_SOMBRA);
        }
        return;
    }
    if (vista_lista) {
        int y = cuerpo_y;
        g_retangulo(jan, cuerpo_x, y, cuerpo_l, 20, C_FACE);
        bisel(jan, cuerpo_x, y, cuerpo_l, 20, false);
        int c1 = cuerpo_x + 24, c2 = c1 + 220, c3 = cuerpo_x + cuerpo_l - 250;
        g_texto(jan, c1 + 4, y + 2, "Juego", C_NEGRO);
        g_texto(jan, c2 + 4, y + 2, "Carpeta", C_NEGRO);
        g_texto(jan, c3 + 4, y + 2, "Núcleo   Sonido", C_NEGRO);
        y += 22;
        for (int f = topo; f < n_vis && y + FILA_ALT <= cuerpo_y + cuerpo_a; f++, y += FILA_ALT) {
            juego_t *g = &juegos[visibles[f]];
            uint32_t col = C_NEGRO;
            if (f == sel) { g_retangulo(jan, cuerpo_x + 1, y, cuerpo_l - 2, FILA_ALT, C_AZUL); col = C_BRANCO; }
            icone_desenhar(jan, cuerpo_x + 4, y + 2, "jogo", 1);
            texto_el(jan, c1 + 4, y + 2, g->nombre, c2 - c1 - 8, col);
            texto_el(jan, c2 + 4, y + 2, g->carpeta, c3 - c2 - 8, col);
            char t[80];
            formatar_texto(t, sizeof t, "%s  %s", g->nucleo, sonido_nombre(g->sonido));
            texto_el(jan, c3 + 4, y + 2, t, 246, col);
        }
        return;
    }
    int cols = columnas();
    for (int f = 0; f < filas_visibles() + 1; f++) {
        for (int c = 0; c < cols; c++) {
            int k = (topo + f) * cols + c;
            if (k >= n_vis) break;
            int x = cuerpo_x + 4 + c * CELDA_L, y = cuerpo_y + 2 + f * CELDA_A;
            if (y + CELDA_A > cuerpo_y + cuerpo_a + 2) continue;
            juego_t *g = &juegos[visibles[k]];
            if (k == sel) g_retangulo(jan, x + 2, y + 2, CELDA_L - 4, CELDA_A - 4, 0xD0D8F0);
            if (k == sel) g_moldura(jan, x + 2, y + 2, CELDA_L - 4, CELDA_A - 4, C_AZUL);
            const imagem_t *p = portada_de(visibles[k]);
            int px = x + (CELDA_L - PORTADA_L) / 2, py = y + 8;
            if (p) {
                for (int yy = 0; yy < PORTADA_A; yy++)
                    for (int xx = 0; xx < PORTADA_L; xx++) {
                        int dx = px + xx, dy = py + yy;
                        if (dx >= 0 && dx < jan->larg && dy >= 0 && dy < jan->alt) jan->px[dy * jan->larg + dx] = p->px[yy * PORTADA_L + xx];
                    }
                g_moldura(jan, px - 1, py - 1, PORTADA_L + 2, PORTADA_A + 2, C_SOMBRA);
            } else {
                icone_desenhar(jan, x + (CELDA_L - 48) / 2, py + 3, "jogo", 3);
            }
            /* El nombre, en una o dos líneas. */
            int maxc = (CELDA_L - 8) / LETRA_L;
            char l1[40], l2[40];
            int n = (int)tam_texto(g->nombre), k1 = n <= maxc ? n : maxc;
            if (n > maxc) {
                int sp = k1;
                while (sp > 4 && g->nombre[sp] != ' ') sp--;
                if (g->nombre[sp] == ' ') k1 = sp;
            }
            for (int i = 0; i < k1 && i < 39; i++) l1[i] = g->nombre[i];
            l1[k1 < 39 ? k1 : 39] = 0;
            const char *resto = g->nombre + k1;
            while (*resto == ' ') resto++;
            copiar(l2, sizeof l2, resto);
            if ((int)tam_texto(l2) > maxc) { l2[maxc - 3] = '.'; l2[maxc - 2] = '.'; l2[maxc - 1] = '.'; l2[maxc] = 0; }
            uint32_t col = C_NEGRO;
            g_texto(jan, x + (CELDA_L - (int)tam_texto(l1) * LETRA_L) / 2, y + PORTADA_A + 14, l1, col);
            if (l2[0]) g_texto(jan, x + (CELDA_L - (int)tam_texto(l2) * LETRA_L) / 2, y + PORTADA_A + 30, l2, col);
        }
    }
}

#define SIDE_FILA 22
/* La lista de sistemas de la izquierda (D321): «Todos» y uno por sistema con emulador instalado. */
static void dibujar_lateral(void)
{
    g_retangulo(jan, 4, cuerpo_y, SIDE_L, cuerpo_a, C_BRANCO);
    bisel(jan, 3, cuerpo_y - 1, SIDE_L + 2, cuerpo_a + 2, true);
    for (int i = -1; i < n_sist; i++) {
        int y = cuerpo_y + 2 + (i + 1) * SIDE_FILA;
        if (y + SIDE_FILA > cuerpo_y + cuerpo_a) break;
        uint32_t col = C_NEGRO;
        if (i == sist_sel) { g_retangulo(jan, 5, y, SIDE_L - 2, SIDE_FILA - 2, C_AZUL); col = C_BRANCO; }
        const char *ic = "jogo";
        if (i >= 0 && sist[i].icono[0] && icones_procurar(sist[i].icono)) ic = sist[i].icono;
        icone_desenhar(jan, 8, y + 2, ic, 1);
        texto_el(jan, 30, y + 3, i < 0 ? "Todos" : sist[i].nombre, SIDE_L - 30, col);
    }
}

static void dibujar(void)
{
    g_retangulo(jan, 0, 0, jan->larg, jan->alt, C_FACE);
    /* menú */
    for (int i = 0; i < 3; i++) g_texto(jan, menu_x[i] + 8, 2, menu_nombres[i], C_NEGRO);
    g_retangulo(jan, 0, ALT_MENU - 1, jan->larg, 1, C_SOMBRA);
    /* barra de herramientas */
    g_retangulo(jan, 0, TB_Y + TB_ALT, jan->larg, 1, C_SOMBRA);
    g_retangulo(jan, 0, TB_Y + TB_ALT + 1, jan->larg, 1, C_LUZ);
    juego_t *s = seleccionado();
    for (int i = 0; i < 6; i++) {
        bool ap = tb_apertado == i;
        bool activo = s || tb[i].accion == A_ANADIR;
        if (tb[i].accion == A_CONFIG) activo = s && !s->rom[0];
        if (ap) bisel(jan, tb[i].x, TB_Y + 2, tb[i].l, TB_ALT - 4, true);
        int ix = tb[i].x + (tb[i].l - 16) / 2 + (ap ? 1 : 0);
        icone_desenhar(jan, ix, TB_Y + 5 + (ap ? 1 : 0), tb[i].icono, 1);
        int tx = tb[i].x + (tb[i].l - g_largura_texto(tb[i].texto)) / 2 + (ap ? 1 : 0);
        g_texto(jan, tx, TB_Y + 24 + (ap ? 1 : 0), tb[i].texto, activo ? C_NEGRO : C_SOMBRA);
        if (i == 0 || i == 3) {
            int sx = tb[i].x + tb[i].l + 4;
            g_retangulo(jan, sx, TB_Y + 4, 1, TB_ALT - 8, C_SOMBRA);
            g_retangulo(jan, sx + 1, TB_Y + 4, 1, TB_ALT - 8, C_LUZ);
        }
    }
    dibujar_lateral();
    dibujar_cuerpo();
    /* barra de estado */
    int y = jan->alt - ALT_ESTADO;
    g_retangulo(jan, 0, y, jan->larg, ALT_ESTADO, C_FACE);
    int esq = 200;
    bisel(jan, 2, y + 2, esq, ALT_ESTADO - 3, true);
    bisel(jan, esq + 4, y + 2, jan->larg - esq - 8, ALT_ESTADO - 3, true);
    char t[120];
    formatar_texto(t, sizeof t, "%d juego%s", n_vis, n_vis == 1 ? "" : "s");
    g_texto(jan, 8, y + 4, t, C_NEGRO);
    if (s) {
        char u[200];
        formatar_texto(u, sizeof u, "%s%s%s", estado[0] ? estado : "", estado[0] ? " - " : "", s->carpeta);
        texto_el(jan, esq + 10, y + 4, u, jan->larg - esq - 20, C_NEGRO);
    } else {
        texto_el(jan, esq + 10, y + 4, estado, jan->larg - esq - 20, C_NEGRO);
    }
    jan_mostrar(jan);
}

/* ------------------------------------------------------------- diálogos */
/* Selector «clic para cambiar»: el valor entre corchetes. */
typedef struct { int x, y, l; const char *const *valores; const char *const *nombres; int n; int cual; } selector_t;

static void selector_dibujar(jan_t *j, selector_t *s)
{
    g_retangulo(j, s->x, s->y, s->l, 22, C_FACE);
    bisel(j, s->x, s->y, s->l, 22, false);
    char t[60];
    formatar_texto(t, sizeof t, "%s  <>", (s->nombres ? s->nombres : s->valores)[s->cual]);
    texto_el(j, s->x + 6, s->y + 3, t, s->l - 10, C_NEGRO);
}

static bool selector_tratar(selector_t *s, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && e->x >= s->x && e->x < s->x + s->l && e->y >= s->y && e->y < s->y + 22) {
        s->cual = (e->botoes & 2) ? (s->cual + s->n - 1) % s->n : (s->cual + 1) % s->n;
        return true;
    }
    return false;
}

static int indice_de(const char *const *v, int n, const char *x)
{
    for (int i = 0; i < n; i++)
        if (!compara_texto(v[i], x)) return i;
    return 0;
}

/* Propiedades del juego (también al añadir). true = aceptado. */
static bool dialogo_propiedades(juego_t *g, bool nuevo)
{
    jan_t *d = jan_abrir(nuevo ? "Añadir juego" : "Propiedades del juego", 560, 470);
    if (!d) return false;
    caixa_t nombre = { 160, 14, 380, "", 0, true, false };
    caixa_t prog = { 160, 48, 300, "", 0, false, false };
    caixa_t cd = { 160, 262, 300, "", 0, false, false };
    caixa_t antes = { 160, 300, 380, "", 0, false, false };
    caixa_t setup = { 160, 338, 300, "", 0, false, false };
    caixa_t *campos[5] = { &nombre, &prog, &cd, &antes, &setup };
    caixa_definir(&nombre, g->nombre);
    caixa_definir(&prog, g->programa);
    caixa_definir(&cd, g->cd);
    caixa_definir(&antes, g->antes);
    caixa_definir(&setup, g->setup);
    selector_t nuc = { 160, 82, 180, NUCLEOS, NULL, N_NUCLEOS, indice_de(NUCLEOS, N_NUCLEOS, g->nucleo) };
    selector_t cic = { 160, 112, 180, CICLOS, NULL, N_CICLOS, indice_de(CICLOS, N_CICLOS, g->ciclos) };
    char mtxt[8];
    formatar_texto(mtxt, sizeof mtxt, "%d", g->memoria);
    selector_t mem = { 160, 142, 180, MEMORIAS, NULL, N_MEMORIAS, indice_de(MEMORIAS, N_MEMORIAS, mtxt) };
    selector_t son = { 160, 172, 260, SONIDOS, SONIDOS_NOMBRE, N_SONIDOS, indice_de(SONIDOS, N_SONIDOS, g->sonido) };
    selector_t joy = { 440, 82, 100, JOYSTICKS, NULL, N_JOYSTICKS, indice_de(JOYSTICKS, N_JOYSTICKS, g->joystick) };
    botao_t b_prog = { 468, 46, 72, 24, "Elegir...", false };
    botao_t b_cdc = { 468, 260, 72, 24, "Carpeta", false };
    botao_t b_cdi = { 468, 286, 72, 24, "Imagen", false };   /* debajo: ocupa también la línea de órdenes previas, que baja */
    botao_t b_set = { 468, 336, 72, 24, "Elegir...", false };
    botao_t ok = { 340, 424, 90, 28, "Aceptar", false };
    botao_t cancel = { 440, 424, 100, 28, "Cancelar", false };
    /* La línea de las órdenes previas va debajo de los dos botones del CD. */
    antes.y = 322;
    setup.y = 360;
    b_set.y = 358;
    bool aceptado = false;
    for (;;) {
        g_retangulo(d, 0, 0, d->larg, d->alt, C_FACE);
        g_texto(d, 16, 18, "Nombre:", C_NEGRO);
        g_texto(d, 16, 52, "Programa:", C_NEGRO);
        g_texto(d, 16, 86, "Núcleo de la CPU:", C_NEGRO);
        g_texto(d, 16, 116, "Ciclos:", C_NEGRO);
        g_texto(d, 16, 146, "Memoria (MB):", C_NEGRO);
        g_texto(d, 16, 176, "Sonido:", C_NEGRO);
        g_texto(d, 356, 86, "Joystick:", C_NEGRO);
        g_texto(d, 16, 212, "Núcleo: «auto» usa el recompilador dinámico en los juegos de 32 bits.", C_SOMBRA);
        g_texto(d, 16, 230, "Ciclos: «max» da toda la velocidad; «auto» ajusta solo.", C_SOMBRA);
        g_texto(d, 16, 266, "CD (D:):", C_NEGRO);
        g_texto(d, 16, 326, "Órdenes previas:", C_NEGRO);
        g_texto(d, 16, 364, "Configurar (setup):", C_NEGRO);
        g_texto(d, 16, 396, "Carpeta:", C_NEGRO);
        texto_el(d, 160, 396, g->carpeta, 380, C_SOMBRA);
        for (int i = 0; i < 5; i++) caixa_desenhar(d, campos[i]);
        selector_dibujar(d, &nuc);
        selector_dibujar(d, &cic);
        selector_dibujar(d, &mem);
        selector_dibujar(d, &son);
        selector_dibujar(d, &joy);
        botao_desenhar(d, &b_prog);
        botao_desenhar(d, &b_cdc);
        botao_desenhar(d, &b_cdi);
        botao_desenhar(d, &b_set);
        botao_desenhar(d, &ok);
        botao_desenhar(d, &cancel);
        jan_mostrar(d);
        evento_t e;
        if (proximo_evento(d->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(d, &cancel, &e)) break;
        if (botao_tratar(d, &ok, &e)) {
            if (!nombre.texto[0]) { mensagem("Recreativa", "El juego necesita un nombre."); continue; }
            aceptado = true;
            break;
        }
        if (botao_tratar(d, &b_prog, &e)) {
            char r[128];
            if (dialogo_abrir("Programa del juego", "Programas de DOS|*.exe;*.com;*.bat|Todos|*", g->carpeta, r, sizeof r)) {
                /* Debe estar dentro de la carpeta del juego: se guarda el nombre relativo. */
                int n = (int)tam_texto(g->carpeta);
                if (!compara_texto_n(r, g->carpeta, (size_t)n) && r[n] == '/') caixa_definir(&prog, r + n + 1);
                else caixa_definir(&prog, nombre_de(r));
            }
            continue;
        }
        if (botao_tratar(d, &b_set, &e)) {
            char r[128];
            if (dialogo_abrir("Programa de configuración", "Programas de DOS|*.exe;*.com;*.bat|Todos|*", g->carpeta, r, sizeof r)) {
                int n = (int)tam_texto(g->carpeta);
                if (!compara_texto_n(r, g->carpeta, (size_t)n) && r[n] == '/') caixa_definir(&setup, r + n + 1);
                else caixa_definir(&setup, nombre_de(r));
            }
            continue;
        }
        if (botao_tratar(d, &b_cdc, &e)) {
            char r[128];
            if (dialogo_carpeta("Carpeta del CD", cd.texto[0] ? cd.texto : "/", r, sizeof r)) caixa_definir(&cd, r);
            continue;
        }
        if (botao_tratar(d, &b_cdi, &e)) {
            char r[128];
            if (dialogo_abrir("Imagen de CD", "Imágenes de CD (*.iso;*.cue)|*.iso;*.cue|Todos|*", "/", r, sizeof r)) caixa_definir(&cd, r);
            continue;
        }
        if (selector_tratar(&nuc, &e) || selector_tratar(&cic, &e) || selector_tratar(&mem, &e) || selector_tratar(&son, &e)
            || selector_tratar(&joy, &e)) continue;
        if (e.tipo == EV_CLIQUE) {
            for (int i = 0; i < 5; i++) campos[i]->foco = false;
        }
        for (int i = 0; i < 5; i++) {
            caixa_tratar(d, campos[i], &e);
        }
        if (e.tipo == EV_TECLA && e.tecla == '\t') {
            int cual = 0;
            for (int i = 0; i < 5; i++) if (campos[i]->foco) cual = i;
            for (int i = 0; i < 5; i++) campos[i]->foco = false;
            campos[(cual + 1) % 5]->foco = true;
        }
    }
    if (aceptado) {
        copiar(g->nombre, sizeof g->nombre, nombre.texto);
        copiar(g->programa, sizeof g->programa, prog.texto);
        copiar(g->cd, sizeof g->cd, cd.texto);
        copiar(g->antes, sizeof g->antes, antes.texto);
        copiar(g->setup, sizeof g->setup, setup.texto);
        copiar(g->nucleo, sizeof g->nucleo, NUCLEOS[nuc.cual]);
        copiar(g->ciclos, sizeof g->ciclos, CICLOS[cic.cual]);
        copiar(g->sonido, sizeof g->sonido, SONIDOS[son.cual]);
        copiar(g->joystick, sizeof g->joystick, JOYSTICKS[joy.cual]);
        int m = 0;
        for (const char *p = MEMORIAS[mem.cual]; *p; p++) m = m * 10 + (*p - '0');
        g->memoria = m;
    }
    jan_fechar(d);
    return aceptado;
}


/* Propiedades de una ROM (D321): nombre, emulador (de los instalados del sistema) y pantalla completa. */
static bool dialogo_props_rom(juego_t *g)
{
    const sistema_t *si = &sist[g->sis];
    jan_t *d = jan_abrir("Propiedades del juego", 480, 250);
    if (!d) return false;
    caixa_t nombre = { 130, 14, 330, "", 0, true, false };
    caixa_definir(&nombre, g->nombre);
    const char *ids[MAX_EMU], *noms[MAX_EMU];
    for (int i = 0; i < si->n_emu; i++) { ids[i] = si->emus[i].id; noms[i] = si->emus[i].nombre; }
    const emu_t *actual = emu_de(si, g->emu);
    int cual = 0;
    for (int i = 0; i < si->n_emu; i++)
        if (!compara_texto(si->emus[i].id, actual->id)) cual = i;
    selector_t emu = { 130, 52, 220, ids, noms, si->n_emu, cual };
    static const char *const PANT[] = { "ventana", "completa" };
    static const char *const PANT_N[] = { "En una ventana", "A pantalla completa" };
    selector_t pan = { 130, 88, 220, PANT, PANT_N, 2, g->completa ? 1 : 0 };
    botao_t ok = { 250, 204, 90, 28, "Aceptar", false };
    botao_t cancel = { 350, 204, 100, 28, "Cancelar", false };
    bool aceptado = false;
    for (;;) {
        g_retangulo(d, 0, 0, d->larg, d->alt, C_FACE);
        g_texto(d, 16, 18, "Nombre:", C_NEGRO);
        g_texto(d, 16, 56, "Emulador:", C_NEGRO);
        g_texto(d, 16, 92, "Pantalla:", C_NEGRO);
        g_texto(d, 16, 128, "Sistema:", C_NEGRO);
        texto_el(d, 130, 128, si->nombre, 330, C_SOMBRA);
        g_texto(d, 16, 150, "ROM:", C_NEGRO);
        texto_el(d, 130, 150, g->rom, 330, C_SOMBRA);
        g_texto(d, 16, 172, "Guardado:", C_NEGRO);
        g_texto(d, 130, 172, "SRAM y estados en /home/.config/<emulador>/", C_SOMBRA);
        caixa_desenhar(d, &nombre);
        selector_dibujar(d, &emu);
        selector_dibujar(d, &pan);
        botao_desenhar(d, &ok);
        botao_desenhar(d, &cancel);
        jan_mostrar(d);
        evento_t e;
        if (proximo_evento(d->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(d, &cancel, &e)) break;
        if (botao_tratar(d, &ok, &e)) {
            if (!nombre.texto[0]) { mensagem("Recreativa", "El juego necesita un nombre."); continue; }
            aceptado = true;
            break;
        }
        if (selector_tratar(&emu, &e) || selector_tratar(&pan, &e)) continue;
        caixa_tratar(d, &nombre, &e);
    }
    if (aceptado) {
        copiar(g->nombre, sizeof g->nombre, nombre.texto);
        copiar(g->emu, sizeof g->emu, ids[emu.cual]);
        g->completa = pan.cual == 1;
    }
    jan_fechar(d);
    return aceptado;
}

static void elegir_sistema(int s)
{
    sist_sel = s;
    topo = 0;
    reconstruir_visibles();
    sel = n_vis ? 0 : -1;
    char t[100];
    formatar_texto(t, sizeof t, s < 0 ? "Todos los sistemas" : "Sistema: %s", s < 0 ? "" : sist[s].nombre);
    definir_estado(t);
    registrar("[RECREATIVA] sistema %s: %d juego(s)", s < 0 ? "todos" : sist[s].id, n_vis);
}

/* Añadir una ROM suelta con el diálogo común (D272). sis < 0: se deduce por la extensión. */
static void anadir_rom(int sis)
{
    char filtros[300] = "";
    int n = 0;
    if (sis >= 0) {
        n += formatar_texto(filtros + n, sizeof filtros - (unsigned)n, "ROM de %s|", sist[sis].nombre);
        for (int j = 0; j < sist[sis].n_ext; j++)
            n += formatar_texto(filtros + n, sizeof filtros - (unsigned)n, "%s*.%s", j ? ";" : "", sist[sis].exts[j]);
    } else {
        n += formatar_texto(filtros + n, sizeof filtros - (unsigned)n, "ROM de consolas|");
        bool primero = true;
        for (int i = 0; i < n_sist; i++) {
            if (i == sis_dos) continue;
            for (int j = 0; j < sist[i].n_ext && n < 250; j++) {
                n += formatar_texto(filtros + n, sizeof filtros - (unsigned)n, "%s*.%s", primero ? "" : ";", sist[i].exts[j]);
                primero = false;
            }
        }
    }
    formatar_texto(filtros + n, sizeof filtros - (unsigned)n, "|Todos|*");
    const char *ini = "/";
    if (sis >= 0)
        for (int c = 0; c < sist[sis].n_carp; c++)
            if (es_carpeta(sist[sis].carpetas[c])) { ini = sist[sis].carpetas[c]; break; }
    if (ini[0] == '/' && !ini[1] && es_carpeta("/home/juegos")) ini = "/home/juegos";
    char r[128];
    if (!dialogo_abrir("Añadir ROM", filtros, ini, r, sizeof r)) return;
    int s2 = sistema_de_archivo(nombre_de(r), sis);
    if (s2 < 0) {
        mensagem("Recreativa", sis >= 0 ? "Esa extensión no es de ROM de este sistema." : "No se sabe a qué sistema pertenece esa ROM (extensión desconocida).");
        return;
    }
    int ya = juego_por_rom(r);
    if (ya >= 0 && !juegos[ya].oculto) { mensagem("Recreativa", "Esa ROM ya está en la lista."); return; }
    if (ya >= 0) {
        juegos[ya].oculto = false;
        juegos[ya].automatico = false;
    } else {
        if (n_juegos >= MAX_JUEGOS) { mensagem("Recreativa", "Demasiados juegos."); return; }
        juego_rom_nuevo(&juegos[n_juegos], s2, r);
        n_juegos++;
    }
    guardar_roms();
    if (sist_sel >= 0 && sist_sel != s2) sist_sel = s2;
    buscar_juegos();
    for (int i = 0; i < n_vis; i++)
        if (!compara_texto(juegos[visibles[i]].rom, r)) sel = i;
    asegurar_visible();
    definir_estado("ROM añadida");
}

/* Añadir una carpeta de ROM (D272): se buscan en ella (y en sus subcarpetas) y la carpeta se recuerda. */
static void anadir_carpeta_rom(int sis, const char *dada)
{
    char carpeta[128];
    if (dada) copiar(carpeta, sizeof carpeta, dada);
    else if (!dialogo_carpeta("Carpeta de ROM", es_carpeta("/home/juegos") ? "/home/juegos" : "/", carpeta, sizeof carpeta)) return;
    int n = (int)tam_texto(carpeta);
    while (n > 1 && carpeta[n - 1] == '/') carpeta[--n] = 0;
    int s2 = sis;
    if (s2 < 0) {      /* sin sistema elegido: el de las primeras ROM que haya dentro */
        entrada_pasta_t e;
        for (int i = 0; listar(carpeta, i, &e) > 0 && i < 256 && s2 < 0; i++)
            if (e.tipo != TIPO_PASTA) s2 = sistema_de_archivo(e.nome, -1);
    }
    if (s2 < 0) { mensagem("Recreativa", "No hay ROM de ningún sistema conocido en esa carpeta."); return; }
    if (n_carp_extra >= MAX_CARP) { mensagem("Recreativa", "Demasiadas carpetas añadidas."); return; }
    for (int i = 0; i < n_carp_extra; i++)
        if (!compara_texto(carp_extra[i].ruta, carpeta) && carp_extra[i].sis == s2) { mensagem("Recreativa", "Esa carpeta ya está añadida."); return; }
    copiar(carp_extra[n_carp_extra].ruta, sizeof carp_extra[n_carp_extra].ruta, carpeta);
    carp_extra[n_carp_extra++].sis = s2;
    guardar_roms();
    buscar_juegos();
    char t[120];
    formatar_texto(t, sizeof t, "Carpeta añadida a %s", sist[s2].nombre);
    definir_estado(t);
    registrar("[RECREATIVA] carpeta de ROM %s añadida a %s", carpeta, sist[s2].id);
}

/* Añadir juego: carpeta (diálogo común D272), detección de los programas y nombre propuesto. */
static void anadir_juego(void)
{
    if (sist_sel >= 0 && sist_sel != sis_dos) { anadir_rom(sist_sel); return; }   /* D321: en un sistema de consolas, una ROM */
    char carpeta[128];
    if (!dialogo_carpeta("Carpeta del juego", existe("/home/dos") ? "/home/dos" : "/", carpeta, sizeof carpeta)) return;
    /* Sin barra final. */
    int n = (int)tam_texto(carpeta);
    while (n > 1 && carpeta[n - 1] == '/') carpeta[--n] = 0;
    if (juego_por_carpeta(carpeta) >= 0 && !juegos[juego_por_carpeta(carpeta)].automatico) {
        mensagem("Recreativa", "Esa carpeta ya está en la lista.");
        return;
    }
    int mejor;
    int hay = buscar_programas(carpeta, &mejor);
    juego_t nuevo;
    juego_defecto(&nuevo);
    copiar(nuevo.carpeta, sizeof nuevo.carpeta, carpeta);
    proponer_nombre(carpeta, nuevo.nombre, sizeof nuevo.nombre);
    if (hay > 0 && mejor >= 0) copiar(nuevo.programa, sizeof nuevo.programa, progs[mejor]);
    buscar_setup(carpeta, nuevo.setup, sizeof nuevo.setup);
    if (hay == 0) {
        /* ¿Es una carpeta de ROM de consolas? Entonces se añade como tal. */
        entrada_pasta_t e;
        for (int i = 0; listar(carpeta, i, &e) > 0 && i < 256; i++)
            if (e.tipo != TIPO_PASTA && sistema_de_archivo(e.nome, -1) >= 0) { anadir_carpeta_rom(-1, carpeta); return; }
        mensagem("Recreativa", "No hay ningún .exe, .com ni .bat en esa carpeta: elija el programa a mano.");
    }
    if (!dialogo_propiedades(&nuevo, true)) return;
    int viejo = juego_por_carpeta(carpeta);
    juego_t *g = viejo >= 0 ? &juegos[viejo] : (n_juegos < MAX_JUEGOS ? &juegos[n_juegos++] : NULL);
    if (!g) { mensagem("Recreativa", "Demasiados juegos."); return; }
    *g = nuevo;
    g->automatico = false;
    g->oculto = false;
    guardar_conf();
    buscar_juegos();
    for (int i = 0; i < n_vis; i++)
        if (!compara_texto(juegos[visibles[i]].carpeta, carpeta)) sel = i;
    asegurar_visible();
    definir_estado("Juego añadido");
}

static void propiedades(void)
{
    juego_t *g = seleccionado();
    if (!g) return;
    juego_t copia = *g;
    if (g->rom[0]) {
        if (!dialogo_props_rom(&copia)) return;
        *g = copia;
        g->automatico = false;      /* tocado: se guarda en roms.conf */
        guardar_roms();
        definir_estado("Propiedades guardadas");
        return;
    }
    if (!dialogo_propiedades(&copia, false)) return;
    *g = copia;
    g->automatico = false;
    guardar_conf();
    definir_estado("Propiedades guardadas");
}

static void quitar(void)
{
    juego_t *g = seleccionado();
    if (!g) return;
    char q[160];
    formatar_texto(q, sizeof q, "¿Quitar «%s» de la lista?\nLos archivos del juego no se borran.", g->nombre);
    if (!confirmar("Recreativa", q)) return;
    if (g->automatico) {
        g->oculto = true;       /* se anota como oculto para que no vuelva a aparecer */
        if (!g->rom[0]) g->automatico = false;
    } else {
        int i = (int)(g - juegos);
        for (int k = i; k + 1 < n_juegos; k++) juegos[k] = juegos[k + 1];
        n_juegos--;
    }
    guardar_conf();
    guardar_roms();
    buscar_juegos();
    definir_estado("Quitado de la lista (los archivos siguen en su carpeta)");
}

static void abrir_carpeta(void)
{
    juego_t *g = seleccionado();
    if (!g) return;
    char *av[3] = { "arquivos", g->carpeta, NULL };
    executar("/bin/arquivos", av);
}

static void acerca(void)
{
    mensagem("Acerca de Recreativa",
             "Recreativa - lanzador de juegos multisistema para SaguiOS.\n\n"
             "DOS con DOSBox 0.74-3; consolas con sus emuladores\n"
             "(los registrados en /etc/recreativa; docs/recreativa.md).\n"
             "Juegos de DOS: /home/dos, una carpeta por juego (juegos.conf).\n"
             "ROM: /home/juegos/<sistema> y /jogos/<sistema>.");
}

static void accion(int a)
{
    juego_t *g = seleccionado();
    switch (a) {
    case A_JUGAR: if (g) { if (g->rom[0]) lanzar_rom(g, false); else lanzar(g, false, false); } break;
    case A_COMPLETA: if (g) { if (g->rom[0]) lanzar_rom(g, true); else lanzar(g, false, true); } break;
    case A_CONFIG: if (g && !g->rom[0]) lanzar(g, true, false); break;
    case A_ANADIR: anadir_juego(); break;
    case A_ANADIR_ROM: anadir_rom(sist_sel >= 0 && sist_sel != sis_dos ? sist_sel : -1); break;
    case A_ANADIR_CARP: anadir_carpeta_rom(sist_sel >= 0 && sist_sel != sis_dos ? sist_sel : -1, NULL); break;
    case A_SIGUIENTE: elegir_sistema(sist_sel + 1 < n_sist ? sist_sel + 1 : -1); break;
    case A_PROP: propiedades(); break;
    case A_QUITAR: quitar(); break;
    case A_CARPETA: abrir_carpeta(); break;
    case A_BUSCAR: buscar_juegos(); definir_estado("Búsqueda terminada"); break;
    case A_VISTA: vista_lista = !vista_lista; topo = 0; asegurar_visible(); break;
    case A_ACERCA: acerca(); break;
    default: break;
    }
}

/* El juego bajo el punto (x, y) de la ventana, o -1. */
static int juego_en(int x, int y)
{
    if (x < cuerpo_x || x >= cuerpo_x + cuerpo_l || y < cuerpo_y || y >= cuerpo_y + cuerpo_a) return -1;
    if (vista_lista) {
        if (y < cuerpo_y + 22) return -1;
        int k = topo + (y - cuerpo_y - 22) / FILA_ALT;
        return k < n_vis ? k : -1;
    }
    int c = (x - cuerpo_x - 4) / CELDA_L, f = (y - cuerpo_y - 2) / CELDA_A;
    if (c < 0 || c >= columnas()) return -1;
    int k = (topo + f) * columnas() + c;
    return k >= 0 && k < n_vis ? k : -1;
}

static void tratar(const evento_t *e)
{
    if (e->tipo == EV_RODA) {
        int d = (int)e->cliques;
        topo += d > 0 ? 1 : -1;
        if (topo > total_filas() - filas_visibles()) topo = total_filas() - filas_visibles();
        if (topo < 0) topo = 0;
        return;
    }
    if (e->tipo == EV_MENU) {
        if (e->x >= 0) {
            int a = menu_pendiente == 2 ? (e->x < 7 ? ctx_acciones[e->x] : 0) : (e->x < 12 ? menu_acciones[menu_cual][e->x] : 0);
            if (a == A_SALIR) { jan_fechar(jan); registrar("[RECREATIVA] fin"); sair(0); }
            accion(a);
        }
        menu_pendiente = 0;
        return;
    }
    if (e->tipo == EV_CLIQUE) {
        if (e->y < ALT_MENU) {
            for (int i = 0; i < 3; i++)
                if (e->x >= menu_x[i] && e->x < menu_x[i] + menu_l[i]) {
                    menu_pendiente = 1;
                    menu_cual = i;
                    menu_contextual(jan->id, menu_x[i], ALT_MENU, menu_textos[i], 1);
                }
            return;
        }
        if (e->y >= TB_Y && e->y < TB_Y + TB_ALT) {
            for (int i = 0; i < 6; i++)
                if (e->x >= tb[i].x && e->x < tb[i].x + tb[i].l) { tb_apertado = i; return; }
            return;
        }
        if (e->x >= 4 && e->x < 4 + SIDE_L && e->y >= cuerpo_y) {
            int f = (e->y - cuerpo_y - 2) / SIDE_FILA - 1;
            if (f >= -1 && f < n_sist) elegir_sistema(f);
            return;
        }
        int k = juego_en(e->x, e->y);
        if (k >= 0) {
            sel = k;
            if (e->botoes & 2) {
                menu_pendiente = 2;
                menu_contextual(jan->id, e->x, e->y, MENU_CTX, 2);
            } else if (e->cliques >= 2) {
                accion(A_JUGAR);
            }
        }
        return;
    }
    if (e->tipo == EV_SOLTOU) {
        if (tb_apertado >= 0) {
            int i = tb_apertado;
            tb_apertado = -1;
            if (e->y >= TB_Y && e->y < TB_Y + TB_ALT && e->x >= tb[i].x && e->x < tb[i].x + tb[i].l) {
                juego_t *g = seleccionado();
                if (g || tb[i].accion == A_ANADIR) accion(tb[i].accion);
            }
        }
        return;
    }
    if (e->tipo == EV_TECLA) {
        int cols = vista_lista ? 1 : columnas();
        if (e->tecla == 10 || e->tecla == 13) { accion((e->modificadores & 4) ? A_PROP : A_JUGAR); return; }
        if (e->tecla == TECLA_INSERT) { accion(A_ANADIR); return; }
        if (e->tecla == TECLA_DELETE) { accion(A_QUITAR); return; }
        if (e->tecla == 12 || ((e->modificadores & 2) && (e->tecla == 'l' || e->tecla == 'L'))) { accion(A_VISTA); return; }
        if (e->tecla == 0x10A + 4) { accion(A_BUSCAR); return; }   /* F5 */
        if (e->tecla == 0x10A + 5) { accion(A_SIGUIENTE); return; }   /* F6 */
        int ns = sel;
        if (e->tecla == TECLA_DIREITA) ns = sel + 1;
        else if (e->tecla == TECLA_ESQUERDA) ns = sel - 1;
        else if (e->tecla == TECLA_BAIXO) ns = sel + cols;
        else if (e->tecla == TECLA_CIMA) ns = sel - cols;
        else if (e->tecla == TECLA_HOME) ns = 0;
        else if (e->tecla == TECLA_END) ns = n_vis - 1;
        if (ns != sel && ns >= 0 && ns < n_vis) { sel = ns; asegurar_visible(); }
    }
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    criar_pasta("/home");
    buscar_juegos();
    jan = jan_abrir("Recreativa", 780, 520);
    if (!jan) return 1;
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 560, 340);
    registrar("[RECREATIVA] %d juego(s)", n_vis);
    layout();
    for (;;) {
        dibujar();
        evento_t e;
        if (esperar_evento(jan->id, &e, 500) != 1) {
            int codigo;
            if (pid_dosbox > 0 && recolher(&codigo) == pid_dosbox) {
                char m[100];
                formatar_texto(m, sizeof m, "%s ha terminado (código %d)", emu_activo[0] ? emu_activo : "DOSBox", codigo);
                definir_estado(m);
                pid_dosbox = 0;
                registrar("[RECREATIVA] %s terminó con %d", emu_activo[0] ? emu_activo : "DOSBox", codigo);
            }
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_REDIMENSIONAR) {
            if (jan_redimensionar(jan, &e)) layout();
            continue;
        }
        tratar(&e);
    }
    jan_fechar(jan);
    registrar("[RECREATIVA] fin");
    return 0;
}
