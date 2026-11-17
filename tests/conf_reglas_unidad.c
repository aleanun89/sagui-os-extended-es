// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_reglas_unidad.c - prueba de unidad en el PC de las reglas de dependencia entre opciones (D367).
 *
 *   gcc -O2 -I userland/inc -o conf_reglas_unidad tests/conf_reglas_unidad.c userland/libsagui/conf_reglas.c
 *
 * Recorre TODAS las combinaciones de las opciones del kernel (sí/no, con y sin valor fijado) y del triestado de la
 * aceleración (con y sin anillo compartido) y, para cada combinación y cada cambio posible de una opción:
 *   - si el estado de partida es válido, aplicar el cambio y sus ajustes deja un estado válido;
 *   - un cambio que conf_bloqueo() rechaza se rechaza solo cuando de verdad rompería una regla REQUIERE o SIN_EFECTO. */
#include <stdio.h>
#include <string.h>
#include "conf_reglas.h"

static const char *const kn[] = { "pae", "nx", "cache_disco", "cache_alta", "escritura_diferida", "cache_max_mb", "sucios_max_mb",
                                  "vaciado_s", "frecuencia", "gobernador", "swap", "swap_mb" };
static const char *const tn[] = { "a_2d", "a_3d", "a_sdl", "a_disco" };
#define NK ((int)(sizeof kn / sizeof kn[0]))
#define NT 4
static int vk[NK], vt[NT];
static int menu_v, reloj_v;   /* D367: la barra y el menú Inicio (claves de sagui.conf) */
static int falhas, comprobaciones;

static int leer(const char *n, void *c)
{
    (void)c;
    for (int i = 0; i < NK; i++) if (!strcmp(kn[i], n)) return vk[i];
    for (int i = 0; i < NT; i++) if (!strcmp(tn[i], n)) return vt[i];
    if (!strcmp(n, "inicio_menu")) return menu_v;
    if (!strcmp(n, "barra_reloj")) return reloj_v;
    if (!strcmp(n, "inicio_buscador") || !strcmp(n, "inicio_translucido") || !strcmp(n, "barra_fecha") || !strcmp(n, "barra_segundos")) return 0;
    return -1;
}

static void poner(const char *n, int v)
{
    for (int i = 0; i < NK; i++) if (!strcmp(kn[i], n)) vk[i] = v;
    for (int i = 0; i < NT; i++) if (!strcmp(tn[i], n)) vt[i] = v;
}

#define COMPROBAR(c, ...) do { comprobaciones++; if (!(c)) { falhas++; if (falhas < 20) { printf("FALLO: " __VA_ARGS__); printf("\n"); } } } while (0)

static void estado_k(int m) { for (int i = 0; i < NK; i++) vk[i] = (m >> i) & 1; }
static void estado_t(int m) { int x = m; for (int i = 0; i < NT; i++) { vt[i] = x % 3; x /= 3; } }

static void cambios(int comparte)
{
    /* todos los cambios de una opción a un valor distinto, desde el estado actual */
    int total = NK + NT;
    for (int o = 0; o < total; o++) {
        const char *n = o < NK ? kn[o] : tn[o - NK];
        int *p = o < NK ? &vk[o] : &vt[o - NK];
        int ant = *p, nv = o < NK ? 2 : 3;
        for (int v = 0; v < nv; v++) {
            if (v == ant) continue;
            const char *bloq = conf_bloqueo(n, leer, NULL, comparte);
            int activa = conf_activo(n, v);
            conf_ajustes(n, v, leer, NULL, comparte, (conf_ajuste_t[8]){ { 0 } }, 8);   /* no debe romper nada con espacio justo */
            /* un cambio «activo» bloqueado se rechaza; el resto (apagar, o no bloqueado) se aplica con sus ajustes */
            if (activa && bloq) continue;
            conf_ajuste_t aj[8];
            int na = conf_ajustes(n, v, leer, NULL, comparte, aj, 8);
            int copia_k[NK], copia_t[NT];
            memcpy(copia_k, vk, sizeof vk); memcpy(copia_t, vt, sizeof vt);
            *p = v;
            for (int i = 0; i < na; i++) poner(aj[i].nome, aj[i].valor);
            const char *op = NULL;
            const char *mal = conf_invalido(leer, NULL, comparte, &op);
            COMPROBAR(!mal, "%s=%d (de %d) deja inválido «%s» (%s)", n, v, ant, op ? op : "?", mal ? mal : "");
            /* los ajustes solo tocan opciones REQUIERE que estaban activas y las dejan inactivas */
            for (int i = 0; i < na; i++) COMPROBAR(!conf_activo(aj[i].nome, aj[i].valor), "ajuste %s no deja inactiva la opción", aj[i].nome);
            memcpy(vk, copia_k, sizeof vk); memcpy(vt, copia_t, sizeof vt);
        }
    }
}

int main(void)
{
    int valido_k = 0;
    for (int comparte = 0; comparte <= 1; comparte++) {
        for (int mk = 0; mk < (1 << NK); mk++) {
            estado_k(mk);
            for (int mt = 0; mt < 81; mt++) {
                if ((mk & 0x3F) && mt % 3) continue;   /* para no multiplicar: el estado del kernel entero solo con a_2d = 0 salvo muestras */
                estado_t(mt);
                if (conf_invalido(leer, NULL, comparte, NULL)) continue;
                valido_k++;
                cambios(comparte);
            }
        }
    }
    /* los casos que pidió el mantenedor, uno a uno */
    estado_k(0); estado_t(0);
    COMPROBAR(conf_bloqueo("nx", leer, NULL, 0) != NULL, "nx sin pae está bloqueada");
    poner("pae", 1);
    COMPROBAR(conf_bloqueo("nx", leer, NULL, 0) == NULL, "nx con pae está libre");
    poner("nx", 1); poner("cache_alta", 1); poner("cache_disco", 1);
    conf_ajuste_t aj[8];
    int n = conf_ajustes("pae", 0, leer, NULL, 0, aj, 8);
    COMPROBAR(n == 2, "apagar pae apaga nx y cache_alta (%d)", n);
    n = conf_ajustes("cache_disco", 0, leer, NULL, 0, aj, 8);
    COMPROBAR(n == 1 && !strcmp(aj[0].nome, "cache_alta"), "apagar la caché de disco apaga la caché alta (%d)", n);
    poner("cache_disco", 0);
    COMPROBAR(conf_bloqueo("vaciado_s", leer, NULL, 0) != NULL, "vaciado_s sin caché de disco: sin efecto");
    poner("swap", 0);
    COMPROBAR(conf_bloqueo("swap_mb", leer, NULL, 0) != NULL, "swap_mb sin swap: sin efecto");
    estado_k(0);   /* (el estado del kernel de arriba ya no vale: caché alta sin caché de disco) */
    poner("a_2d", 2); poner("a_3d", 1); poner("a_sdl", 1);
    COMPROBAR(conf_invalido(leer, NULL, 0, NULL) == NULL, "sin anillo compartido, 3D activo con 2D apagado es válido");
    COMPROBAR(conf_invalido(leer, NULL, 1, NULL) != NULL, "con anillo compartido, 3D activo con 2D apagado es inválido");
    n = conf_ajustes("a_2d", 2, leer, NULL, 1, aj, 8);
    COMPROBAR(n == 2, "apagar el 2D en Intel apaga 3D y SDL (%d)", n);
    poner("a_3d", 2); poner("a_sdl", 1);
    COMPROBAR(conf_invalido(leer, NULL, 0, NULL) != NULL, "SDL activo con 3D desactivado es inválido");
    /* la barra: buscador y translúcido solo con el menú Moderno (3); fecha y segundos solo con el reloj visible */
    for (menu_v = 0; menu_v < 4; menu_v++)
        for (reloj_v = 0; reloj_v < 2; reloj_v++) {
            COMPROBAR((conf_bloqueo("inicio_buscador", leer, NULL, 0) == NULL) == (menu_v == 3), "buscador con menú %d", menu_v);
            COMPROBAR((conf_bloqueo("inicio_translucido", leer, NULL, 0) == NULL) == (menu_v == 3), "translúcido con menú %d", menu_v);
            COMPROBAR((conf_bloqueo("barra_fecha", leer, NULL, 0) == NULL) == (reloj_v == 1), "fecha con reloj %d", reloj_v);
            COMPROBAR((conf_bloqueo("barra_segundos", leer, NULL, 0) == NULL) == (reloj_v == 1), "segundos con reloj %d", reloj_v);
            COMPROBAR(conf_ajustes("inicio_menu", menu_v, leer, NULL, 0, (conf_ajuste_t[4]){ { 0 } }, 4) == 0, "cambiar el menú no arrastra nada");
        }
    printf("conf_reglas_unidad: %d estados de partida, %d comprobaciones, %d fallos\n", valido_k, comprobaciones, falhas);
    return falhas ? 1 : 0;
}
