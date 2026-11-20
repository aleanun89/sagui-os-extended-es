// SPDX-License-Identifier: GPL-3.0-only
/* painel_unidad.c - prueba en el PC de kernel/dispositivos/painel_logica.h (D407, h408 unidad).
 *
 * QEMU no emula ningún panel táctil: aquí se construyen a mano, bit a bit según el formato de
 * cada fabricante, los paquetes que mandaría el panel (Synaptics con W y AGM, ALPS v2, Elantech
 * v2, v3 y v4) y se comprueba lo que sale: el modelo reconocido por sus respuestas, la
 * decodificación (posición, presión, dedos, botones, palanca) y los gestos (mover, toque = clic,
 * dos y tres dedos, doble toque para arrastrar, doble clic, desplazamiento con dos dedos y por el
 * borde, sentido natural, palma, al escribir, panel apagado).
 * gcc -O2 -Wall -Wextra -o painel_unidad tests/painel_unidad.c && ./painel_unidad */
#include <stdio.h>
#include <string.h>
#include "../kernel/dispositivos/painel_logica.h"

static int falhas, checagens;
#define CHECA(c, ...) do { checagens++; if (!(c)) { falhas++; printf("FALLO (%s:%d): ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ constructores de paquetes */

/* Synaptics absoluto con W: x, y crudos (y hacia arriba), z, w (0-15), botones izq/der. */
static void syn_pac(uint8_t p[6], int x, int y, int z, int w, int bot)
{
    p[0] = (uint8_t)(0x80 | ((w & 0x0C) << 2) | ((w & 0x02) << 1) | (bot & 3));
    p[1] = (uint8_t)(((y >> 8) & 0x0F) << 4 | ((x >> 8) & 0x0F));
    p[2] = (uint8_t)z;
    p[3] = (uint8_t)(0xC0 | ((x >> 12) & 1) << 4 | ((y >> 12) & 1) << 5 | (w & 1) << 2 | (bot & 3));
    p[4] = (uint8_t)x;
    p[5] = (uint8_t)y;
}

/* Paquete AGM (W = 2): segundo dedo a media resolución. */
static void syn_agm(uint8_t p[6], int x, int y)
{
    int hx = x >> 1, hy = y >> 1;
    syn_pac(p, 0, 0, 0, 2, 0);
    p[1] = (uint8_t)hx;
    p[2] = (uint8_t)hy;
    p[4] = (uint8_t)(((hy >> 8) & 0x0F) << 4 | ((hx >> 8) & 0x0F));
    p[5] = 0;
}

static void alps_pac(uint8_t p[6], const painel_modelo_t *m, int x, int y, int z, int bot, int ges, int fin)
{
    p[0] = m->alps_b0;
    p[1] = (uint8_t)(x & 0x7F);
    p[2] = (uint8_t)(((x >> 7) & 0x0F) << 3 | (fin ? 2 : 0) | (ges ? 1 : 0));
    p[3] = (uint8_t)(((y >> 7) & 0x07) << 4 | (bot & 7));
    p[4] = (uint8_t)(y & 0x7F);
    p[5] = (uint8_t)(z & 0x7F);
}

static void elan2_pac1(uint8_t p[6], int dedos, int x, int y, int pres, int larg, int bot)
{
    p[0] = (uint8_t)(dedos << 6 | ((larg >> 2) & 3) << 4 | 0x04 | (bot & 3));
    p[1] = (uint8_t)((pres & 0xF0) | ((x >> 8) & 0x0F));
    p[2] = (uint8_t)x;
    p[3] = (uint8_t)((larg & 3) << 4 | 0x02);
    p[4] = (uint8_t)((pres & 0x0F) << 4 | ((y >> 8) & 0x0F));
    p[5] = (uint8_t)y;
}

/* Dos dedos en v2: coordenadas de 9 bits (un cuarto de la resolución). */
static void elan2_pac2(uint8_t p[6], int x1, int y1, int x2, int y2)
{
    p[0] = (uint8_t)(2 << 6 | ((y1 >> 8) & 1) << 5 | ((x1 >> 8) & 1) << 4 | 0x04);
    p[1] = (uint8_t)x1;
    p[2] = (uint8_t)y1;
    p[3] = (uint8_t)(((y2 >> 8) & 1) << 5 | ((x2 >> 8) & 1) << 4 | 0x02);
    p[4] = (uint8_t)x2;
    p[5] = (uint8_t)y2;
}

static void elan3_pac(uint8_t p[6], bool cabeca, int dedos, int x, int y, int pres, int bot)
{
    p[0] = (uint8_t)(dedos << 6 | (cabeca ? 0x04 : 0x0C) | (bot & 3));
    p[1] = (uint8_t)((pres & 0xF0) | ((x >> 8) & 0x0F));
    p[2] = (uint8_t)x;
    p[3] = cabeca ? 0x02 : 0x0C;
    p[4] = (uint8_t)((pres & 0x0F) << 4 | ((y >> 8) & 0x0F));
    p[5] = (uint8_t)y;
}

static void elan4_estado(uint8_t p[6], int mascara)
{
    p[0] = 0x00; p[1] = (uint8_t)mascara; p[2] = 0; p[3] = 0x10; p[4] = 0; p[5] = 0;
}

static void elan4_cabeca(uint8_t p[6], int id, int x, int y, int pres)
{
    p[0] = 0x00;
    p[1] = (uint8_t)((pres & 0xF0) | ((x >> 8) & 0x0F));
    p[2] = (uint8_t)x;
    p[3] = (uint8_t)((id + 1) << 5 | 0x10 | 1);
    p[4] = (uint8_t)((pres & 0x0F) << 4 | ((y >> 8) & 0x0F));
    p[5] = (uint8_t)y;
}

static void elan4_mov(uint8_t p[6], int id, int dx, int dy, int id2, int dx2, int dy2)
{
    p[0] = (uint8_t)((id + 1) << 5);
    p[1] = (uint8_t)(int8_t)dx; p[2] = (uint8_t)(int8_t)dy;
    p[3] = (uint8_t)((id2 + 1) << 5 | 0x10 | 2);
    p[4] = (uint8_t)(int8_t)dx2; p[5] = (uint8_t)(int8_t)dy2;
}

/* ------------------------------------------------------------ un panel con sus gestos */

typedef struct {
    painel_decod_t d;
    painel_gestos_t g;
    painel_conf_t c;
    uint32_t agora, tecla;
    /* lo que ha salido */
    int dx, dy, rv, rh, eventos;
    uint8_t botoes;
    int pulsados[8], soltados[8];   /* por bit de botón */
} sim_t;

static void sim_iniciar(sim_t *s, const painel_modelo_t *m)
{
    memset(s, 0, sizeof *s);
    painel_decod_iniciar(&s->d, m);
    painel_gestos_iniciar(&s->g);
    painel_conf_padrao(&s->c);
    s->agora = 10000;
    s->tecla = 0;
}

static void sim_saida(sim_t *s, const painel_saida_t *o)
{
    for (int i = 0; i < o->n; i++) {
        const painel_evento_t *e = &o->e[i];
        s->dx += e->dx; s->dy += e->dy; s->rv += e->rv; s->rh += e->rh;
        uint8_t mud = e->botoes ^ s->botoes;
        for (int b = 0; b < 3; b++)
            if (mud & (1 << b)) { if (e->botoes & (1 << b)) s->pulsados[b]++; else s->soltados[b]++; }
        s->botoes = e->botoes;
        s->eventos++;
    }
}

static void sim_zerar(sim_t *s)
{
    s->dx = s->dy = s->rv = s->rh = s->eventos = 0;
    memset(s->pulsados, 0, sizeof s->pulsados);
    memset(s->soltados, 0, sizeof s->soltados);
}

/* Un paquete, 12,5 ms después del anterior (80 paquetes por segundo). */
static int sim_pacote(sim_t *s, const uint8_t p[6])
{
    painel_quadro_t q;
    s->agora += 12;
    int r = painel_decodificar(&s->d, p, &q);
    if (r == 1) {
        painel_saida_t o;
        painel_gestos_quadro(&s->g, &s->d.m, &s->c, &q, s->agora, s->tecla, &o);
        sim_saida(s, &o);
    }
    return r;
}

static void sim_esperar(sim_t *s, int ms)
{
    for (int t = 0; t < ms; t += 10) {
        s->agora += 10;
        painel_saida_t o;
        painel_gestos_tique(&s->g, s->agora, &o);
        sim_saida(s, &o);
    }
}

/* Synaptics: dedo en (x, y) crudos (y hacia arriba), n paquetes moviéndose (vx, vy) cada uno. */
static void syn_arrastar(sim_t *s, int x, int y, int vx, int vy, int n, int z, int w)
{
    uint8_t p[6];
    for (int i = 0; i < n; i++) { syn_pac(p, x + vx * i, y + vy * i, z, w, 0); sim_pacote(s, p); }
}

static void syn_soltar(sim_t *s)
{
    uint8_t p[6];
    syn_pac(p, 0, 0, 0, 0, 0);
    sim_pacote(s, p);
    sim_pacote(s, p);
}

/* ------------------------------------------------------------ Synaptics */

static void syn_modelo_t(painel_modelo_t *m, bool agm)
{
    memset(m, 0, sizeof *m);
    uint8_t id[3] = { 0x01, 0x47, 0x18 };   /* versión 8.1, modelo 1 */
    int ma = 0, me = 0;
    CHECA(syn_identificar(id, &ma, &me) && ma == 8 && me == 1, "identificar 8.1: %d.%d", ma, me);
    uint8_t no[3] = { 0x01, 0x00, 0x64 };
    CHECA(!syn_identificar(no, &ma, &me), "un ratón PS/2 (status 01 00 64) no es Synaptics");
    uint32_t cap = 0xD04773;   /* extendida, 5 consultas, botón central... */
    cap = (1u << 23) | (4u << 20) | (1u << 18) | 0x4700 | (1u << 7) | 2 | 1;
    uint32_t e0c = agm ? (1u << 19) | (1u << 17) : (1u << 17);
    uint8_t max[3] = { 5888 >> 5, ((4950 >> 1) & 0x0F) << 4 | ((5888 >> 1) & 0x0F), 4950 >> 5 };
    syn_modelo(m, ma, me, cap, e0c, max);
    CHECA(m->tipo == PAINEL_SYNAPTICS && m->versao == 8, "modelo Synaptics");
    CHECA(m->xmax == 5888 && m->yinv == 4950, "máximos 0x0D: %d x %d", m->xmax, m->yinv);
    CHECA(m->multidedo && m->boton_medio && m->passthrough, "capacidades: multidedo, central, palanca");
    CHECA(m->agm == agm, "AGM según 0x0C");
}

static void t_synaptics_decod(void)
{
    painel_modelo_t m;
    syn_modelo_t(&m, true);
    painel_decod_t d;
    painel_decod_iniciar(&d, &m);
    uint8_t p[6];
    painel_quadro_t q;
    syn_pac(p, 3000, 2500, 60, 5, 1);
    CHECA(painel_decodificar(&d, p, &q) == 1, "un dedo");
    CHECA(q.dedos == 1 && q.x == 3000 && q.y == 4950 - 2500 && q.z == 60 && q.largura == 5 && q.botoes == PNL_ESQ,
          "un dedo: dedos %d x %d y %d z %d w %d b %d", q.dedos, q.x, q.y, q.z, q.largura, q.botoes);
    syn_pac(p, 5800, 4900, 80, 4, 2);   /* x y y con el bit 12 */
    painel_decodificar(&d, p, &q);
    CHECA(q.x == 5800 && q.y == 50 && q.botoes == PNL_DIR, "bits 12 de x e y: %d %d", q.x, q.y);
    /* presión baja: no hay dedo; con histéresis */
    syn_pac(p, 3000, 2500, 20, 5, 0);
    painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 0, "z 20: sin dedo");
    syn_pac(p, 3000, 2500, 31, 5, 0); painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 1, "z 31: dedo");
    syn_pac(p, 3000, 2500, 27, 5, 0); painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 1, "z 27 tras 31: sigue el dedo (histéresis)");
    syn_pac(p, 3000, 2500, 24, 5, 0); painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 0, "z 24: se fue");
    /* dos dedos: W = 0, con AGM */
    syn_agm(p, 4000, 3000);
    CHECA(painel_decodificar(&d, p, &q) == 0, "AGM: no da cuadro");
    syn_pac(p, 2000, 2000, 70, 0, 0);
    painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 2 && q.tem2 && q.x2 == 4000 && q.y2 == 4950 - 3000, "dos dedos con AGM: %d (%d,%d)", q.dedos, q.x2, q.y2);
    syn_pac(p, 2000, 2000, 70, 1, 0); painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 3, "W = 1: tres dedos");
    /* botón central: bit 0 de los bytes 0 y 3 distinto */
    syn_pac(p, 2000, 2000, 70, 5, 0);
    p[3] |= 1;
    painel_decodificar(&d, p, &q);
    CHECA(q.botoes == PNL_MEIO, "botón central: %d", q.botoes);
    /* la palanca (W = 3): paquete PS/2 en los bytes 1, 4 y 5 */
    syn_pac(p, 0, 0, 0, 3, 0);
    p[1] = 0x08 | 0x20 | 1;   /* dy negativo, botón izquierdo */
    p[4] = 5; p[5] = (uint8_t)-7;
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.relativo && q.rdx == 5 && q.rdy == 7 && (q.botoes & PNL_ESQ),
          "palanca: %d %d %d", q.rdx, q.rdy, q.botoes);
    /* sincronía: un paquete PS/2 relativo (bit 3 a 1) no vale */
    uint8_t ps2[6] = { 0x08, 1, 1, 0x08, 1, 1 };
    CHECA(painel_decodificar(&d, ps2, &q) == -1, "un paquete relativo se rechaza");
    CHECA(!painel_byte_ok(&m, 0, 0x08) && painel_byte_ok(&m, 0, 0x80) && !painel_byte_ok(&m, 3, 0x80) && painel_byte_ok(&m, 3, 0xC0),
          "bytes de sincronía 0 y 3");
    uint8_t a[4];
    syn_argumentos(0xC5, a);
    CHECA(a[0] == 3 && a[1] == 0 && a[2] == 1 && a[3] == 1, "orden especial 0xC5 en cuatro E8");
}

static void t_synaptics_gestos(void)
{
    painel_modelo_t m;
    syn_modelo_t(&m, true);
    sim_t s;

    /* mover a la derecha y hacia arriba del panel (= arriba en la pantalla) */
    sim_iniciar(&s, &m);
    syn_arrastar(&s, 2500, 2500, 20, 0, 20, 60, 5);
    CHECA(s.dx > 50 && s.dy == 0 && s.botoes == 0, "mover a la derecha: dx %d dy %d", s.dx, s.dy);
    int lento = s.dx;
    syn_soltar(&s);
    sim_zerar(&s);
    syn_arrastar(&s, 2500, 2500, 0, 20, 20, 60, 5);
    CHECA(s.dy < -50 && s.dx == 0, "subir el dedo sube el cursor: dy %d", s.dy);
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    /* aceleración: la misma distancia más deprisa da más píxeles */
    syn_arrastar(&s, 1600, 2500, 100, 0, 5, 60, 5);
    CHECA(s.dx > lento * 3 / 2, "aceleración: %d rápido frente a %d lento", s.dx, lento);
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    /* sensibilidad */
    s.c.sensib = 10;
    syn_arrastar(&s, 2500, 2500, 20, 0, 20, 60, 5);
    CHECA(s.dx > lento * 3 / 2, "sensibilidad 10: %d frente a %d", s.dx, lento);
    s.c.sensib = 5;
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);

    /* toque = clic izquierdo: se pulsa al levantar y se suelta al acabar la espera */
    syn_arrastar(&s, 3000, 3000, 1, 0, 6, 60, 5);
    syn_soltar(&s);
    CHECA(s.pulsados[0] == 1 && s.soltados[0] == 0 && s.dx == 0, "toque: izquierdo pulsado (%d, dx %d)", s.pulsados[0], s.dx);
    sim_esperar(&s, 300);
    CHECA(s.soltados[0] == 1 && s.botoes == 0, "toque: izquierdo suelto tras la espera");
    sim_zerar(&s);
    /* sin «toque para clic» */
    s.c.toque = false;
    syn_arrastar(&s, 3000, 3000, 1, 0, 6, 60, 5); syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.pulsados[0] == 0, "toque apagado: sin clic");
    s.c.toque = true; sim_zerar(&s);
    /* un contacto largo no es toque */
    syn_arrastar(&s, 3000, 3000, 0, 0, 30, 60, 5); syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.pulsados[0] == 0, "contacto de 360 ms: sin clic");
    sim_zerar(&s);
    /* dos dedos: derecho; tres: central (pulsado y suelto enseguida) */
    syn_arrastar(&s, 3000, 3000, 0, 0, 2, 60, 5);
    syn_arrastar(&s, 3000, 3000, 0, 0, 4, 60, 0);
    syn_soltar(&s);
    CHECA(s.pulsados[1] == 1 && s.soltados[1] == 1 && s.pulsados[0] == 0, "toque con dos dedos: derecho");
    sim_esperar(&s, 300); sim_zerar(&s);
    syn_arrastar(&s, 3000, 3000, 0, 0, 2, 60, 5);
    syn_arrastar(&s, 3000, 3000, 0, 0, 4, 60, 1);
    syn_soltar(&s);
    CHECA(s.pulsados[2] == 1 && s.soltados[2] == 1, "toque con tres dedos: central");
    sim_esperar(&s, 300); sim_zerar(&s);

    /* doble toque y arrastrar: el botón sigue pulsado mientras se mueve */
    syn_arrastar(&s, 3000, 3000, 0, 0, 5, 60, 5);
    syn_soltar(&s);
    sim_esperar(&s, 60);
    syn_arrastar(&s, 3000, 3000, 30, 0, 25, 60, 5);
    CHECA(s.botoes == PNL_ESQ && s.dx > 50 && s.soltados[0] == 0, "arrastrar: botón pulsado y dx %d", s.dx);
    syn_soltar(&s);
    CHECA(s.botoes == 0 && s.soltados[0] == 1, "arrastrar: al levantar se suelta");
    sim_esperar(&s, 300); sim_zerar(&s);
    /* doble clic: dos toques cortos */
    syn_arrastar(&s, 3000, 3000, 0, 0, 5, 60, 5); syn_soltar(&s);
    sim_esperar(&s, 50);
    syn_arrastar(&s, 3000, 3000, 0, 0, 5, 60, 5); syn_soltar(&s);
    sim_esperar(&s, 300);
    CHECA(s.pulsados[0] == 2 && s.soltados[0] == 2, "doble clic: %d pulsados, %d soltados", s.pulsados[0], s.soltados[0]);
    sim_zerar(&s);

    /* desplazamiento con dos dedos: bajar los dedos = rueda hacia abajo (sentido clásico) */
    syn_arrastar(&s, 3000, 3500, 0, -40, 25, 60, 0);
    CHECA(s.rv > 3 && s.rh == 0 && s.dx == 0 && s.dy == 0, "dos dedos abajo: rueda %d, sin mover (%d,%d)", s.rv, s.dx, s.dy);
    syn_soltar(&s);
    CHECA(s.pulsados[1] == 0, "desplazar no hace clic derecho");
    sim_esperar(&s, 300); sim_zerar(&s);
    s.c.natural = true;
    syn_arrastar(&s, 3000, 3500, 0, -40, 25, 60, 0);
    CHECA(s.rv < -3, "sentido natural: rueda %d", s.rv);
    syn_soltar(&s); sim_zerar(&s);
    s.c.natural = false;
    syn_arrastar(&s, 2000, 3000, 40, 0, 25, 60, 0);
    CHECA(s.rh > 3 && s.rv == 0, "dos dedos a la derecha: horizontal %d", s.rh);
    syn_soltar(&s); sim_zerar(&s);
    s.c.dois_dedos = false;
    syn_arrastar(&s, 3000, 3500, 0, -40, 25, 60, 0);
    CHECA(s.rv == 0, "sin desplazamiento con dos dedos: nada de rueda");
    s.c.dois_dedos = true;
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    /* levantar un dedo tras desplazar no mueve el cursor */
    syn_arrastar(&s, 3000, 3500, 0, -40, 10, 60, 0);
    syn_arrastar(&s, 3200, 3100, 0, -40, 10, 60, 5);
    CHECA(s.dx == 0 && s.dy == 0, "de dos a un dedo: el cursor no salta (%d,%d)", s.dx, s.dy);
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);

    /* por el borde: derecho (vertical) e inferior (horizontal), solo si se pide */
    syn_arrastar(&s, 5800, 4000, 0, -40, 20, 60, 5);
    CHECA(s.rv == 0 && s.dy > 0, "borde apagado: el borde derecho mueve el cursor");
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    s.c.borda = true;
    syn_arrastar(&s, 5800, 4000, 0, -40, 20, 60, 5);
    CHECA(s.rv >= 3 && s.dx == 0 && s.dy == 0, "borde derecho: rueda %d", s.rv);
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    syn_arrastar(&s, 2000, 1450, 40, 0, 20, 60, 5);   /* y cruda baja = abajo del panel */
    CHECA(s.rh >= 3 && s.dx == 0, "borde inferior: horizontal %d", s.rh);
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    s.c.borda = false;

    /* palma: ancho grande o presión muy alta */
    syn_arrastar(&s, 3000, 3000, 20, 0, 20, 90, 14);
    syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.dx == 0 && s.pulsados[0] == 0 && s.g.palmas == 1, "palma (W 14): ni mueve ni pulsa");
    sim_zerar(&s);
    syn_arrastar(&s, 3000, 3000, 0, 0, 4, 230, 5);
    syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.pulsados[0] == 0, "palma (z 230): sin clic");
    sim_zerar(&s);

    /* al escribir: un toque 100 ms después de una tecla no cuenta; 500 ms después sí */
    s.tecla = s.agora - 100;
    syn_arrastar(&s, 3000, 3000, 20, 0, 10, 60, 5);
    syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.dx == 0 && s.g.teclado == 1, "al escribir: no se mueve (%d)", s.dx);
    sim_zerar(&s);
    s.tecla = s.agora - 500;
    syn_arrastar(&s, 3000, 3000, 20, 0, 10, 60, 5);
    CHECA(s.dx > 0, "medio segundo después: se mueve");
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    s.c.ms_teclado = 0;
    s.tecla = s.agora;
    syn_arrastar(&s, 3000, 3000, 20, 0, 10, 60, 5);
    CHECA(s.dx > 0, "sin «desactivar al escribir»: se mueve");
    syn_soltar(&s); sim_esperar(&s, 300); sim_zerar(&s);
    s.c.ms_teclado = 300;

    /* panel apagado (ratón USB): nada */
    s.c.desligado = true;
    syn_arrastar(&s, 3000, 3000, 20, 0, 10, 60, 5); syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.dx == 0 && s.pulsados[0] == 0, "apagado: nada");
    s.c.desligado = false; sim_zerar(&s);

    /* botones físicos */
    uint8_t p[6];
    syn_pac(p, 0, 0, 0, 0, 1); sim_pacote(&s, p);
    syn_pac(p, 0, 0, 0, 0, 0); sim_pacote(&s, p);
    CHECA(s.pulsados[0] == 1 && s.soltados[0] == 1, "botón físico izquierdo");
    sim_esperar(&s, 300); sim_zerar(&s);
    /* clickpad: apoyar el dedo, hundir el panel (botón) y levantar enseguida: un solo clic, no además un toque */
    syn_pac(p, 3000, 3000, 60, 5, 0); sim_pacote(&s, p);
    syn_pac(p, 3000, 3000, 80, 5, 1); sim_pacote(&s, p);
    syn_pac(p, 3000, 3000, 80, 5, 1); sim_pacote(&s, p);
    syn_pac(p, 3000, 3000, 60, 5, 0); sim_pacote(&s, p);
    syn_soltar(&s); sim_esperar(&s, 300);
    CHECA(s.pulsados[0] == 1 && s.soltados[0] == 1, "clickpad: un clic físico, sin toque (%d pulsados)", s.pulsados[0]);
    sim_zerar(&s);
    /* el dedo deja de mandar paquetes sin levantarse (se perdió el último): se da por levantado */
    syn_arrastar(&s, 3000, 3000, 0, 0, 5, 60, 5);
    sim_esperar(&s, 600);
    CHECA(!s.g.contato, "silencio de 600 ms: contacto terminado");
}

/* ------------------------------------------------------------ ALPS */

static void t_alps(void)
{
    uint8_t e6[3] = { 0, 0, 0x64 }, e6q[3] = { 0, 2, 0x64 };
    CHECA(alps_e6(e6) && !alps_e6(e6q), "informe E6 de ALPS; el de QEMU (00 02 64) no");
    painel_modelo_t m;
    memset(&m, 0, sizeof m);
    uint8_t v4[3] = { 0x73, 0x02, 0x64 }, mezcla[3] = { 0x62, 0x02, 0x14 }, qemu[3] = { 0x10, 0x00, 0x64 };
    CHECA(!alps_modelo(&m, v4) && !alps_modelo(&m, mezcla) && !alps_modelo(&m, qemu), "v4, intercalado y QEMU: no");
    uint8_t e7[3] = { 0x73, 0x02, 0x50 };
    CHECA(alps_modelo(&m, e7) && m.tipo == PAINEL_ALPS && m.alps_b0 == 0xCF, "Dell Vostro 1400 (73 02 50)");
    painel_decod_t d;
    painel_decod_iniciar(&d, &m);
    uint8_t p[6];
    painel_quadro_t q;
    alps_pac(p, &m, 700, 500, 50, 1, 0, 1);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 1 && q.x == 700 && q.y == 500 && q.z == 50 && q.botoes == PNL_ESQ,
          "ALPS: x %d y %d z %d b %d", q.x, q.y, q.z, q.botoes);
    p[2] |= 0x80;
    CHECA(painel_decodificar(&d, p, &q) == -1, "ALPS: un byte con el bit 7 se rechaza");
    alps_pac(p, &m, 0, 0, 0, 0, 1, 0);
    painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 1 && q.z == 40, "ALPS: toque del firmware = contacto corto");
    /* gestos con ALPS: mover hacia abajo (la y ya va hacia abajo) */
    sim_t s;
    sim_iniciar(&s, &m);
    for (int i = 0; i < 20; i++) { alps_pac(p, &m, 500, 300 + i * 5, 50, 0, 0, 1); sim_pacote(&s, p); }
    CHECA(s.dy > 30 && s.dx == 0, "ALPS: bajar el dedo baja el cursor (%d)", s.dy);
    alps_pac(p, &m, 0, 0, 0, 0, 0, 0); sim_pacote(&s, p);
    /* palanca en el mismo paquete (z = 127) */
    uint8_t e7p[3] = { 0x73, 0x00, 0x0A };
    CHECA(alps_modelo(&m, e7p) && m.alps_palanca, "ThinkPad R61: palanca");
    painel_decod_iniciar(&d, &m);
    alps_pac(p, &m, 768 - 3, 4, 127, 0, 0, 0);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.relativo && q.rdx == -3 && q.rdy == 4, "ALPS palanca: %d %d", q.rdx, q.rdy);
}

/* ------------------------------------------------------------ Elantech */

static void t_elantech(void)
{
    uint8_t mg[3] = { 0x3C, 0x03, 0xC8 }, qemu[3] = { 0x00, 0x02, 0x64 };
    CHECA(elan_magico(mg) && !elan_magico(qemu), "golpe mágico de Elantech");
    CHECA(elan_versao_hw(0x020022) == 0 && elan_versao_hw(0x020600) == 0, "firmware v1: no");
    CHECA(elan_versao_hw(0x040216) == 2 && elan_versao_hw(0x020800) == 2, "firmware v2");
    CHECA(elan_versao_hw(0x150500) == 3 && elan_versao_hw(0x460f02) == 4 && elan_versao_hw(0x381f17) == 4, "v3 y v4: %d %d",
          elan_versao_hw(0x150500), elan_versao_hw(0x381f17));
    painel_modelo_t m;
    memset(&m, 0, sizeof m);
    CHECA(!elan_modelo(&m, 0x150500, NULL), "v3 sin máximos: no");
    uint8_t raro[3] = { 0, 0, 0 };
    CHECA(!elan_modelo(&m, 0x150500, raro), "v3 con máximos absurdos: no");

    /* v2 */
    CHECA(!elan_modelo(&m, 0x020600 + 0x100, NULL) && !elan_modelo(&m, 0x020030, NULL), "v2 sin presión (firmware < 0x020800): no");
    CHECA(elan_modelo(&m, 0x040216, NULL) && m.versao == 2 && m.xmax == 1152, "v2");
    painel_decod_t d;
    painel_decod_iniciar(&d, &m);
    uint8_t p[6];
    painel_quadro_t q;
    elan2_pac1(p, 1, 600, 300, 0x5A, 6, 2);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 1 && q.x == 600 && q.y == 768 - 300 && q.z == 0x5A && q.largura == 6 && q.botoes == PNL_DIR,
          "v2 un dedo: x %d y %d z %d w %d", q.x, q.y, q.z, q.largura);
    elan2_pac2(p, 100, 50, 260, 150);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 2 && q.x == 400 && q.y == 768 - 200 && q.tem2 && q.x2 == 1040 && q.y2 == 768 - 600,
          "v2 dos dedos: (%d,%d) (%d,%d)", q.x, q.y, q.x2, q.y2);
    p[3] = 0x05;
    CHECA(painel_decodificar(&d, p, &q) == -1, "v2: byte 3 fuera de sincronía");
    /* gestos v2: dos dedos moviéndose = desplazamiento */
    sim_t s;
    sim_iniciar(&s, &m);
    for (int i = 0; i < 20; i++) { elan2_pac2(p, 100, 150 - i * 3, 200, 150 - i * 3); sim_pacote(&s, p); }
    CHECA(s.rv > 2 && s.dx == 0 && s.dy == 0, "v2: dos dedos hacia abajo = rueda %d", s.rv);
    elan2_pac1(p, 0, 0, 0, 0, 0, 0); sim_pacote(&s, p);
    sim_esperar(&s, 300);
    CHECA(s.pulsados[1] == 0, "v2: desplazar no es clic derecho");

    /* v3: cabeza y cola con dos dedos */
    uint8_t id[3] = { (0x05 << 4) | 0x0B, 0xC0, 0x80 };   /* xmax 0xBC0 = 3008, ymax 0x580 = 1408 */
    CHECA(elan_modelo(&m, 0x150500, id) && m.versao == 3 && m.xmax == 3008 && m.ymax == 1408, "v3: máximos %d x %d", m.xmax, m.ymax);
    painel_decod_iniciar(&d, &m);
    elan3_pac(p, true, 1, 1500, 700, 0x40, 0);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 1 && q.x == 1500 && q.y == 1408 - 700 && q.z == 0x40, "v3 un dedo");
    elan3_pac(p, true, 2, 1000, 500, 0x40, 0);
    CHECA(painel_decodificar(&d, p, &q) == 0, "v3 dos dedos: la cabeza espera la cola");
    elan3_pac(p, false, 2, 2000, 900, 0x40, 0);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 2 && q.x == 1000 && q.x2 == 2000 && q.y2 == 1408 - 900, "v3 dos dedos: cola");
    uint8_t reb[6] = { 0xC4, 0xFF, 0xFF, 0x02, 0xFF, 0xFF };
    CHECA(painel_decodificar(&d, reb, &q) == 0, "v3: paquete de rebote ignorado");

    /* v4: estado, cabeza y movimiento */
    uint8_t id4[3] = { (0x07 << 4) | 0x0C, 0x80, 0x20 };   /* 3200 x 1824 */
    CHECA(elan_modelo(&m, 0x381f17, id4) && m.versao == 4, "v4");
    painel_decod_iniciar(&d, &m);
    elan4_estado(p, 0x01);
    CHECA(painel_decodificar(&d, p, &q) == 0, "v4 estado: un dedo nuevo, sin posición aún");
    elan4_cabeca(p, 0, 1600, 900, 0x50);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.dedos == 1 && q.x == 1600 && q.y == 1824 - 900, "v4 cabeza: %d %d", q.x, q.y);
    elan4_mov(p, 0, 10, -4, -1, 0, 0);
    CHECA(painel_decodificar(&d, p, &q) == 1 && q.x == 1610 && q.y == 1824 - 900 + 4, "v4 movimiento: %d %d", q.x, q.y);
    p[0] |= 0x08;
    CHECA(painel_decodificar(&d, p, &q) == -1, "v4: bit 3 del byte 0 a 1 se rechaza");
    elan4_estado(p, 0x03);
    elan4_cabeca(p, 1, 2400, 900, 0x50);
    painel_decodificar(&d, p, &q);
    CHECA(q.dedos == 2 && q.tem2 && q.x2 == 2400, "v4 dos dedos");
    /* gestos v4: tocar con dos dedos = derecho */
    sim_iniciar(&s, &m);
    elan4_estado(p, 0x01); sim_pacote(&s, p);
    elan4_cabeca(p, 0, 1600, 900, 0x50); sim_pacote(&s, p);
    elan4_estado(p, 0x03); sim_pacote(&s, p);
    elan4_cabeca(p, 1, 2000, 900, 0x50); sim_pacote(&s, p);
    elan4_estado(p, 0x00); sim_pacote(&s, p);
    CHECA(s.pulsados[1] == 1 && s.soltados[1] == 1 && s.pulsados[0] == 0, "v4: toque con dos dedos = derecho");
    /* y desplazar con dos dedos a la izquierda */
    sim_zerar(&s);
    elan4_estado(p, 0x03); sim_pacote(&s, p);
    elan4_cabeca(p, 0, 2000, 900, 0x50); sim_pacote(&s, p);
    elan4_cabeca(p, 1, 2300, 900, 0x50); sim_pacote(&s, p);
    for (int i = 0; i < 30; i++) { elan4_mov(p, 0, -15, 0, 1, -15, 0); sim_pacote(&s, p); }
    CHECA(s.rh < -2 && s.dx == 0, "v4: dos dedos a la izquierda = horizontal %d", s.rh);
}

int main(void)
{
    t_synaptics_decod();
    t_synaptics_gestos();
    t_alps();
    t_elantech();
    if (falhas) { printf("%d de %d comprobaciones fallan\n", falhas, checagens); return 1; }
    printf("%d comprobaciones: todo bien\n", checagens);
    return 0;
}
