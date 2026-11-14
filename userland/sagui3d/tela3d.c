// SPDX-License-Identifier: GPL-3.0-only
/* tela3d.c - el dibujo en perspectiva del Sagui 3D.
 *
 * La idea, a partir de la geometría:
 *
 * 1. Cámara. Cada punto del mapa se convierte en dos medidas respecto del sagui:
 *    "fondo" (cuánto está adelante, en la dirección de la mirada) y "lado" (cuánto
 *    está a la derecha). En la pantalla, por semejanza de triángulos, la columna es
 *    centro + lado * FOCO / fondo, y una altura h aparece en la línea
 *    horizonte - (h - ojo) * FOCO / fondo.
 *
 * 2. Sin ojo de pez. El "fondo" es la distancia medida a lo largo de la dirección
 *    de la mirada, y no la distancia en línea recta hasta el punto. Una pared recta
 *    tiene el mismo "fondo" que el plano de la pantalla ve, así que sigue recta
 *    en la pantalla. Si se usara la distancia en línea recta, las columnas de las esquinas
 *    de la pantalla (que están más lejos en línea recta) se achicarían y la pared
 *    quedaría curva.
 *
 * 3. Columna por columna, con una "rendija". Cada columna de la pantalla guarda la
 *    franja de líneas que todavía no fue pintada (tope y fondo de la rendija).
 *    El dibujo empieza en la sala del sagui. Para cada lado de la sala que está de
 *    frente a él, cada columna que ese lado ocupa recibe el techo de la sala
 *    de arriba, el suelo de abajo y:
 *      - si el lado es pared, la pared entera, y la columna se cierra;
 *      - si es pasaje, solo los escalones (el pedazo de pared entre los dos
 *        techos y entre los dos suelos), y la rendija se achica al hueco del
 *        pasaje. Después la sala vecina se dibuja solo en esas columnas,
 *        dentro de la rendija que sobró.
 *    Como las salas son convexas, los lados de una sala nunca se cubren en la
 *    pantalla, así que el orden en que se visitan los lados no importa, y nada
 *    se pinta dos veces. Una sala vista por un pasaje se dibuja
 *    después de la sala de adelante, pero solo en el agujero que dejó la de adelante:
 *    no hace falta ordenar paredes por distancia.
 *
 * 4. Dónde cae la pared en la columna. Para cada columna, el rayo que sale del ojo
 *    se cruza con el segmento del lado (una cuenta de recta contra recta), lo que
 *    da el "fondo" exacto y la posición a lo largo de la pared para la textura.
 *
 * 5. Suelo y techo. En una línea de la pantalla debajo del horizonte, el suelo de altura
 *    h está a fondo = (ojo - h) * FOCO / (línea - horizonte), y con el
 *    fondo y la dirección de la columna sale el punto del mapa, que dice el píxel de la
 *    textura. FOCO / (línea - horizonte) se calcula una vez por línea.
 *
 * 6. Cosas (monos, bananas...). Después de las paredes, cada cosa se vuelve un
 *    rectángulo de pie, dibujado de atrás hacia adelante y recortado por la
 *    rendija que su sala tenía en cada columna (guardada en el paso 3):
 *    así un mono detrás de un pasaje desaparece detrás del marco. */
#include "jogo.h"

uint32_t vista[VA][VL];

#define PERTO (UM / 2)          /* nada más cerca que medio palmo se dibuja */
#define MAX_PROFUNDIDADE 32     /* salas vistas unas a través de las otras */

static int16_t fresta_topo[VL], fresta_fundo[VL];
static int32_t fresta_da_sala[MAX_SALAS][VL];   /* tope | fondo << 16, o -1 */
static fixo raio[VL];             /* (columna - centro) / FOCO */
static fixo dir_x[VL], dir_y[VL]; /* dirección de cada columna en el mapa (fondo = 1) */
static fixo inverso[VA];          /* FOCO / (horizonte - centro de la línea), con signo */
static int  arvores[VL];
static uint32_t cor_arvores[VL];
static uint32_t ceu[VA];
static const camera_t *cam;
static fixo cs, sn;

static inline int teto_fixo(fixo v) { return (v + UM - 1) >> 16; }

/* Limita una distancia en la pantalla a ±4096 píxeles antes de sumar al centro:
 * un punto casi pegado al ojo daría un número que no cabe en 32 bits
 * (visto en la primera prueba: la pared de la derecha desaparecía). */
static inline fixo limitar(fixo v)
{
    return v > FX(4096) ? FX(4096) : (v < -FX(4096) ? -FX(4096) : v);
}

static inline fixo coluna_do_ponto(fixo lado, fixo fundo)
{
    return FX(VL / 2) + limitar(muldiv(lado, FX(FOCO), fundo));
}

/* Línea de la pantalla donde queda la altura h vista a una distancia "fondo"
 * (en fixo, para redondear como la columna). */
static inline fixo linha_da_altura(fixo h, fixo fundo)
{
    return FX(HORIZONTE) - limitar(muldiv(h - cam->z, FX(FOCO), fundo));
}

/* Primera línea cuyo centro queda debajo de la frontera y. */
static inline int primeira_abaixo(fixo y) { return teto_fixo(y - UM / 2); }

static inline int nivel_de_luz(int luz, fixo fundo)
{
    int n = luz - INTEIRO(fundo) / 80;
    return n < 0 ? 0 : (n > 15 ? 15 : n);
}

/* Techo (o cielo) y suelo de una sala en una columna, entre las líneas y0 e y1. */
static void pintar_plano(const sala_t *s, int x, int y0, int y1, bool de_cima)
{
    if (y0 > y1) return;
    fixo h = de_cima ? s->teto : s->chao;
    int tex = de_cima ? s->forro : s->piso;
    if (tex == T_CEU) {
        /* Cielo arriba, la silueta de los árboles (un solo color) de la línea
         * HORIZONTE - arvores[x] para abajo: dos bucles sin prueba por píxel. */
        int corte = HORIZONTE - arvores[x];
        int y = y0;
        for (; y <= y1 && y < corte; y++) vista[y][x] = ceu[y];
        for (uint32_t c = cor_arvores[x]; y <= y1; y++) vista[y][x] = c;
        return;
    }
    fixo altura = h - cam->z;
    const uint8_t (*t)[TEX] = texturas[tex];
    for (int y = y0; y <= y1; y++) {
        fixo inv = inverso[y];
        /* El plano solo aparece del lado correcto del horizonte: un techo arriba del
         * ojo arriba, un suelo debajo del ojo abajo. */
        if ((altura > 0) != (inv > 0)) { vista[y][x] = sombra[0][1]; continue; }
        fixo fundo = fmul(altura, inv);
        fixo wx = cam->x + fmul(fundo, dir_x[x]);
        fixo wy = cam->y + fmul(fundo, dir_y[x]);
        int c = t[(wy >> 17) & (TEX - 1)][(wx >> 17) & (TEX - 1)];
        vista[y][x] = sombra[nivel_de_luz(s->luz, fundo)][c];
    }
}

/* Un pedazo de pared en una columna: líneas y0..y1, textura alineada por el
 * alto del pedazo (altura "alto"), posición u a lo largo de la pared. */
static void pintar_parede(int tex, int luz, int x, int y0, int y1, fixo alto, fixo u, fixo fundo)
{
    if (y0 > y1) return;
    const uint8_t (*t)[TEX] = texturas[tex];
    int tu = (u >> 17) & (TEX - 1);
    const uint32_t *cor = sombra[nivel_de_luz(luz, fundo)];
    /* Altura (en el mundo) del centro de la línea y0, y cuánto baja por línea. */
    fixo passo = fundo / FOCO;
    fixo z = cam->z + fmul(FX(HORIZONTE) - FX(y0) - UM / 2, passo);
    for (int y = y0; y <= y1; y++) {
        fixo v = alto - z;
        vista[y][x] = cor[t[(v >> 17) & (TEX - 1)][tu]];
        z -= passo;
    }
}

static void desenhar_sala(int si, int x0, int x1, int profundidade)
{
    if (x0 > x1 || profundidade > MAX_PROFUNDIDADE) return;
    const sala_t *s = &mapa.salas[si];
    for (int x = x0; x <= x1; x++)
        if (fresta_topo[x] <= fresta_fundo[x])
            fresta_da_sala[si][x] = (int32_t)((uint16_t)fresta_topo[x] | ((uint32_t)(uint16_t)fresta_fundo[x] << 16));

    for (int l = 0; l < s->n; l++) {
        const lado_t *d = &s->lados[l];
        if (distancia_ao_lado(s, l, cam->x, cam->y) <= 0) continue;   /* de espaldas al sagui */
        /* Desde adentro de la sala, la esquina b queda a la izquierda en la pantalla y la a a la derecha. */
        const ponto_t *pe = &mapa.pontos[d->b], *pd = &mapa.pontos[d->a];
        fixo dxe = pe->x - cam->x, dye = pe->y - cam->y, dxd = pd->x - cam->x, dyd = pd->y - cam->y;
        fixo fe = fmul(dxe, cs) + fmul(dye, sn), le = fmul(dxe, sn) - fmul(dye, cs);
        fixo fd = fmul(dxd, cs) + fmul(dyd, sn), ld = fmul(dxd, sn) - fmul(dyd, cs);
        if (fe < PERTO && fd < PERTO) continue;   /* todo detrás del ojo */
        /* Recorta el pedazo detrás del ojo para hallar las columnas que ocupa el lado. */
        fixo cfe = fe, cle = le, cfd = fd, cld = ld;
        if (cfe < PERTO) { fixo t = fdiv(PERTO - fe, fd - fe); cle = le + fmul(t, ld - le); cfe = PERTO; }
        if (cfd < PERTO) { fixo t = fdiv(PERTO - fd, fe - fd); cld = ld + fmul(t, le - ld); cfd = PERTO; }
        fixo xe = coluna_do_ponto(cle, cfe);
        fixo xd = coluna_do_ponto(cld, cfd);
        int ca = teto_fixo(xe - UM / 2), cb = teto_fixo(xd - UM / 2) - 1;
        if (ca < x0) ca = x0;
        if (cb > x1) cb = x1;
        if (ca > cb) continue;

        bool passagem = d->vizinha >= 0 && !d->secreta;
        const sala_t *n = passagem ? &mapa.salas[d->vizinha] : NULL;
        int abertas = 0;
        for (int x = ca; x <= cb; x++) {
            int topo = fresta_topo[x], fundo = fresta_fundo[x];
            if (topo > fundo) continue;
            /* Rayo de la columna contra el segmento (de la esquina izquierda a la derecha). */
            fixo k = raio[x];
            fixo num = fmul(k, fe) - le;
            fixo den = (ld - le) - fmul(k, fd - fe);
            fixo t = den ? fdiv(num, den) : 0;
            if (t < 0) t = 0;
            if (t > UM) t = UM;
            fixo prof = fe + fmul(t, fd - fe);
            if (prof < PERTO) prof = PERTO;
            fixo u = fmul(t, d->comprimento);

            int lt = primeira_abaixo(linha_da_altura(s->teto, prof));
            int lc = primeira_abaixo(linha_da_altura(s->chao, prof));
            pintar_plano(s, x, topo, (lt - 1 < fundo ? lt - 1 : fundo), true);
            pintar_plano(s, x, (lc > topo ? lc : topo), fundo, false);
            if (!passagem) {
                pintar_parede(d->textura, s->luz, x, lt > topo ? lt : topo, lc - 1 < fundo ? lc - 1 : fundo, s->teto, u, prof);
                fresta_topo[x] = 1;
                fresta_fundo[x] = 0;
                continue;
            }
            int nt = lt, nc = lc;
            if (n->teto < s->teto) {
                nt = primeira_abaixo(linha_da_altura(n->teto, prof));
                pintar_parede(d->textura, s->luz, x, lt > topo ? lt : topo, nt - 1 < fundo ? nt - 1 : fundo, s->teto, u, prof);
            }
            if (n->chao > s->chao) {
                nc = primeira_abaixo(linha_da_altura(n->chao, prof));
                pintar_parede(d->textura, s->luz, x, nc > topo ? nc : topo, lc - 1 < fundo ? lc - 1 : fundo, n->chao, u, prof);
            }
            int novo_topo = nt > topo ? nt : topo, novo_fundo = nc - 1 < fundo ? nc - 1 : fundo;
            fresta_topo[x] = (int16_t)novo_topo;
            fresta_fundo[x] = (int16_t)novo_fundo;
            if (novo_topo <= novo_fundo) abertas++;
        }
        if (passagem && abertas) desenhar_sala(d->vizinha, ca, cb, profundidade + 1);
    }
}

/* ------------------------------------------------------------ cosas */

typedef struct {
    fixo fundo, lado, base;   /* base = altura del pie en el mundo */
    int  figura, sala, luz;
    fixo largura, altura;     /* en palmos (fixo) */
    bool espelho;
} desenho_t;

static desenho_t lista[MAX_COISAS + 32];
static int qtd_lista;

static void anotar(fixo x, fixo y, fixo base, int sala, int figura, int larg, int alt, bool espelho)
{
    if (qtd_lista >= (int)(sizeof lista / sizeof lista[0]) || sala < 0) return;
    fixo dx = x - cam->x, dy = y - cam->y;
    fixo f = fmul(dx, cs) + fmul(dy, sn);
    if (f < FX(4)) return;
    desenho_t *d = &lista[qtd_lista++];
    d->fundo = f;
    d->lado = fmul(dx, sn) - fmul(dy, cs);
    d->base = base;
    d->sala = sala;
    d->figura = figura;
    d->largura = FX(larg);
    d->altura = FX(alt);
    d->espelho = espelho;
    d->luz = mapa.salas[sala].luz < 5 ? 5 : mapa.salas[sala].luz;   /* las cosas nunca desaparecen del todo en la oscuridad */
}

static void pintar_coisa(const desenho_t *d)
{
    const figura_t *f = &figuras[d->figura];
    if (!f->px) return;
    fixo esc = fdiv(FX(FOCO), d->fundo);          /* píxeles por palmo */
    fixo meio = coluna_do_ponto(d->lado, d->fundo);
    fixo larg = fmul(d->largura, esc), alt = fmul(d->altura, esc);
    if (larg < UM / 4 || alt < UM / 4) return;
    fixo esq = meio - larg / 2;
    fixo pe = linha_da_altura(d->base, d->fundo), cabeca = pe - alt;
    int x0 = primeira_abaixo(esq), x1 = primeira_abaixo(esq + larg) - 1;
    int y0 = primeira_abaixo(cabeca), y1 = primeira_abaixo(pe) - 1;
    if (x0 < 0) x0 = 0;
    if (x1 >= VL) x1 = VL - 1;
    const uint32_t *cor = sombra[nivel_de_luz(d->luz, d->fundo)];
    fixo passo_u = fdiv(FX(f->l), larg), passo_v = fdiv(FX(f->a), alt);
    for (int x = x0; x <= x1; x++) {
        int32_t fr = fresta_da_sala[d->sala][x];
        if (fr < 0) continue;
        int ftopo = (int16_t)(fr & 0xFFFF), ffundo = (int16_t)((uint32_t)fr >> 16);
        int u = INTEIRO(fmul(FX(x) + UM / 2 - esq, passo_u));
        if (u < 0) u = 0;
        if (u >= f->l) u = f->l - 1;
        if (d->espelho) u = f->l - 1 - u;
        int ya = y0 > ftopo ? y0 : ftopo, yb = y1 < ffundo ? y1 : ffundo;
        for (int y = ya; y <= yb; y++) {
            int v = INTEIRO(fmul(FX(y) + UM / 2 - cabeca, passo_v));
            if (v < 0 || v >= f->a) continue;
            uint8_t p = f->px[v * f->l + u];
            if (p) vista[y][x] = cor[p];
        }
    }
}

static void desenhar_coisas(int relogio)
{
    qtd_lista = 0;
    for (int i = 0; i < mapa.qtd_coisas; i++) {
        const coisa_t *c = &mapa.coisas[i];
        if (!c->ativa) continue;
        fixo chao = mapa.salas[c->sala].chao;
        switch (c->tipo) {
        case C_BANANA: {
            fixo pulo = seno(relogio * 40 + i * 500) * 3;     /* flota despacio */
            anotar(c->x, c->y, chao + FX(6) + pulo, c->sala, F_BANANA, 14, 14, false);
            break;
        }
        case C_CACHO:
            anotar(c->x, c->y, chao + FX(6) + seno(relogio * 40) * 3, c->sala, F_CACHO, 22, 22, false);
            break;
        case C_POTE:     anotar(c->x, c->y, chao, c->sala, F_POTE, 18, 14, false); break;
        case C_CASCA:    anotar(c->x, c->y, chao, c->sala, F_CASCA, 16, 8, false); break;
        case C_PORTEIRA: anotar(c->x, c->y, chao, c->sala, porteira_aberta() ? F_PORTEIRA_ABERTA : F_PORTEIRA, 64, 80, false); break;
        case C_SEMENTE_VOANDO: anotar(c->x, c->y, c->z, c->sala, F_SEMENTE, 5, 5, false); break;
        case C_MACACO: {
            if (c->estado == M_FORA) break;
            bool tonto = c->estado == M_TONTO || c->estado == M_ESCORREGANDO;
            /* Resbalando, gira: la figura se da vuelta de un lado a otro. */
            bool espelho = c->estado == M_ESCORREGANDO ? (relogio / 3) % 2 : (c->estado == M_FUGINDO);
            anotar(c->x, c->y, chao, c->sala, tonto ? F_MACACO_TONTO : F_MACACO, 30, 40, espelho);
            if (tonto)   /* estrellitas girando alrededor de la cabeza */
                for (int e = 0; e < 3; e++) {
                    int a = relogio * 120 + e * (VOLTA / 3);
                    anotar(c->x + seno(a + QUARTO) * 12, c->y + seno(a) * 12, chao + FX(42), c->sala, F_ESTRELA, 6, 6, false);
                }
            break;
        }
        }
    }
    /* De atrás hacia adelante (ordenación por inserción: son pocas cosas). */
    for (int i = 1; i < qtd_lista; i++) {
        desenho_t t = lista[i];
        int j = i - 1;
        while (j >= 0 && lista[j].fundo < t.fundo) { lista[j + 1] = lista[j]; j--; }
        lista[j + 1] = t;
    }
    for (int i = 0; i < qtd_lista; i++) pintar_coisa(&lista[i]);
}

/* ------------------------------------------------------------ cuadro */

void tela3d_desenhar(const camera_t *c, int relogio)
{
    cam = c;
    cs = cosseno(c->ang);
    sn = seno(c->ang);
    for (int x = 0; x < VL; x++) {
        fresta_topo[x] = 0;
        fresta_fundo[x] = VA - 1;
        raio[x] = (FX(x - VL / 2) + UM / 2) / FOCO;
        /* Punto a fondo 1 en la columna x: frente + radio * derecha, con derecha = (sn, -cs). */
        dir_x[x] = cs + fmul(raio[x], sn);
        dir_y[x] = sn - fmul(raio[x], cs);
        int a = c->ang - (x - VL / 2) * 3;   /* ~3 pasos de ángulo por columna */
        arvores[x] = altura_arvores(a);
        cor_arvores[x] = cor_arvore(a);
    }
    for (int y = 0; y < VA; y++) {
        fixo d = FX(HORIZONTE) - FX(y) - UM / 2;
        inverso[y] = fdiv(FX(FOCO), d);
        ceu[y] = y < HORIZONTE ? degrade_ceu(y) : 0;
    }
    for (int s = 0; s < mapa.qtd_salas; s++)
        for (int x = 0; x < VL; x++) fresta_da_sala[s][x] = -1;
    if (c->sala >= 0) desenhar_sala(c->sala, 0, VL - 1, 0);
    /* Columnas que nada cerró (no debería pasar): pinta de oscuro. */
    for (int x = 0; x < VL; x++)
        for (int y = fresta_topo[x]; y <= fresta_fundo[x]; y++) vista[y][x] = sombra[0][1];
    desenhar_coisas(relogio);
}
