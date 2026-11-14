// SPDX-License-Identifier: GPL-2.0-or-later
/* spider - Solitario Spider de SaguiOS (D173).
 *
 * Dos barajas (104 cartas), 10 columnas y 5 repartos de 10 cartas, como el Spider del
 * Windows Me. Niveles de 1, 2 y 4 palos. Se mueve una carta o una escalera descendente
 * del mismo palo; se deja sobre una carta una unidad mayor (de cualquier palo) o en una
 * columna vacía; al juntar K..A del mismo palo, la escalera se retira.
 *
 * Todo se dibuja a mano en el propio programa (sin fuentes de palos ni imágenes):
 *   - las cartas son "sprites" que se construyen una vez por tamaño (esquinas redondeadas
 *     con borde suavizado, palos vectoriales con 3x3 muestras, figuras J/Q/K con corona);
 *   - la escena (tapete, columnas, montones) se guarda en un búfer aparte; arrastrar una
 *     pila sólo copia la escena y pone encima las cartas que siguen al ratón.
 *
 * Teclas: F2 partida nueva, Ctrl+Z deshacer, H o P pista, D o espacio reparte, Esc cancela.
 * Ratón: arrastrar y soltar; un clic mueve la carta (o la escalera) a la mejor columna; un
 * clic en el montón de abajo a la derecha reparte; el botón derecho abre el menú.
 *
 * Opciones: --nivel 1|2|4, --semilla N (partida repetible), --autoprueba (comprueba las reglas
 * sin ventana y escribe el resultado por la serie), --casi-ganada (una partida a un clic de ganar,
 * para ver la animación final).
 *
 * Solo enteros (la libsagui no usa coma flotante): los bordes suaves salen de la distancia al
 * cuadrado y los palos de funciones implícitas muestreadas. */
#include "sagui.h"
#include "gui.h"

extern const unsigned char fonte_sagui[256][16];

/* ------------------------------------------------------------------ reglas */

#define N_COL      10
#define MAX_PILHA  104
#define N_FEITAS   8
#define MAX_DESF   120

/* Una carta cabe en un byte: bit 7 = boca arriba, bits 4-5 = palo, bits 0-3 = valor (1..13). */
typedef uint8_t carta_t;
#define C_VALOR(c) ((int)((c) & 15))
#define C_PALO(c)  ((int)(((c) >> 4) & 3))
#define C_CIMA(c)  (((c) & 0x80) != 0)
#define FAZER(palo, valor, cima) ((carta_t)(((cima) ? 0x80 : 0) | ((palo) << 4) | (valor)))

enum { P_PICAS, P_CORAZONES, P_TREBOLES, P_DIAMANTES };

typedef struct {
    carta_t  col[N_COL][MAX_PILHA];
    uint8_t  n[N_COL];
    carta_t  estoque[50];
    uint8_t  n_estoque;
    uint8_t  feitas;                 /* escaleras retiradas */
    uint8_t  feitas_palo[N_FEITAS];
    int      pontos, movs;
} estado_t;

static estado_t   e;                 /* la partida */
static estado_t   e_inicial;         /* tal como se repartió (para "Reiniciar") */
static estado_t  *desf;              /* anillo de deshacer */
static int        desf_ini, desf_n;
static int        nivel = 1;         /* palos: 1, 2 o 4 */
static uint32_t   semente, rng;
static bool       gui_ativo;         /* false en la autoprueba: nada de posiciones ni animaciones */

static uint32_t aleat(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

static void desf_empurrar(void)
{
    if (!desf) return;
    if (desf_n == MAX_DESF) { desf_ini = (desf_ini + 1) % MAX_DESF; desf_n--; }
    desf[(desf_ini + desf_n) % MAX_DESF] = e;
    desf_n++;
}

static bool desf_voltar(void)
{
    if (!desf || desf_n == 0) return false;
    e = desf[(desf_ini + desf_n - 1) % MAX_DESF];
    desf_n--;
    return true;
}

/* Reparte una partida nueva: 54 cartas a las columnas (4 de 6 y 6 de 5) y 50 al montón. */
static void nova_partida(int niv, uint32_t sem)
{
    carta_t baralha[104];
    int k = 0;
    nivel = niv;
    semente = sem;
    rng = sem ? sem : 1;
    for (int i = 0; i < 8; i++) {
        int palo = i % niv;
        for (int v = 1; v <= 13; v++) baralha[k++] = FAZER(palo, v, 0);
    }
    for (int i = 103; i > 0; i--) {
        int r = (int)(aleat() % (uint32_t)(i + 1));
        carta_t t = baralha[i]; baralha[i] = baralha[r]; baralha[r] = t;
    }
    memset(&e, 0, sizeof e);
    k = 0;
    for (int c = 0; c < N_COL; c++) {
        int q = c < 4 ? 6 : 5;
        for (int i = 0; i < q; i++) e.col[c][i] = baralha[k++];
        e.n[c] = (uint8_t)q;
        e.col[c][q - 1] |= 0x80;
    }
    for (int i = 0; i < 50; i++) e.estoque[i] = baralha[k++];
    e.n_estoque = 50;
    e.pontos = 500;
    e_inicial = e;
    desf_ini = desf_n = 0;
}

/* Desde qué posición hacia abajo la columna es una escalera descendente del mismo palo, toda
 * boca arriba: sólo esas cartas se pueden coger. */
static int inicio_sequencia(const estado_t *s, int c)
{
    int n = s->n[c];
    if (n == 0) return 0;
    int i = n - 1;
    while (i > 0) {
        carta_t a = s->col[c][i - 1], b = s->col[c][i];
        if (!C_CIMA(a) || C_PALO(a) != C_PALO(b) || C_VALOR(a) != C_VALOR(b) + 1) break;
        i--;
    }
    return i;
}

static bool pode_pegar(const estado_t *s, int c, int i)
{
    return i >= 0 && i < s->n[c] && C_CIMA(s->col[c][i]) && i >= inicio_sequencia(s, c);
}

/* ¿Se puede dejar una pila cuya primera carta vale "valor" en la columna c? */
static bool pode_soltar(const estado_t *s, int c, int valor)
{
    if (s->n[c] == 0) return true;
    return C_VALOR(s->col[c][s->n[c] - 1]) == valor + 1;
}

/* Escalera K..A completa al final de la columna c. */
static bool tem_completa(const estado_t *s, int c)
{
    int n = s->n[c];
    if (n < 13) return false;
    for (int k = 0; k < 13; k++) {
        carta_t x = s->col[c][n - 13 + k];
        if (!C_CIMA(x) || C_VALOR(x) != 13 - k || C_PALO(x) != C_PALO(s->col[c][n - 1])) return false;
    }
    return true;
}

typedef struct {
    int col, palo;
    int x[13], y[13];                /* dónde estaban las 13 cartas (K..A) antes de retirarlas */
} retirada_t;
#define MAX_RET 4
static retirada_t ret_lista[MAX_RET];
static int        ret_n;

static void posicao_carta(const estado_t *s, int c, int i, int *x, int *y);   /* de la geometría */

static void tirar_completas(estado_t *s)
{
    for (int c = 0; c < N_COL; c++) {
        while (tem_completa(s, c)) {
            int n = s->n[c];
            if (gui_ativo && ret_n < MAX_RET) {
                retirada_t *r = &ret_lista[ret_n++];
                r->col = c;
                r->palo = C_PALO(s->col[c][n - 1]);
                for (int k = 0; k < 13; k++) posicao_carta(s, c, n - 13 + k, &r->x[k], &r->y[k]);
            }
            if (s->feitas < N_FEITAS) s->feitas_palo[s->feitas] = (uint8_t)C_PALO(s->col[c][n - 1]);
            s->feitas++;
            s->pontos += 100;
            s->n[c] = (uint8_t)(n - 13);
            if (s->n[c] > 0) s->col[c][s->n[c] - 1] |= 0x80;
        }
    }
}

/* Mueve la pila que empieza en col[de][i] a la columna "para". Lo hace sólo si es legal. */
static bool executar_jogada(int de, int i, int para)
{
    if (de == para || !pode_pegar(&e, de, i)) return false;
    if (!pode_soltar(&e, para, C_VALOR(e.col[de][i]))) return false;
    desf_empurrar();
    int q = e.n[de] - i;
    memcpy(&e.col[para][e.n[para]], &e.col[de][i], (size_t)q);
    e.n[para] = (uint8_t)(e.n[para] + q);
    e.n[de] = (uint8_t)i;
    if (i > 0) e.col[de][i - 1] |= 0x80;
    if (e.pontos > 0) e.pontos--;
    e.movs++;
    ret_n = 0;
    tirar_completas(&e);
    return true;
}

enum { REP_OK, REP_SEM_CARTAS, REP_COLUNA_VAZIA };

static int executar_reparto(void)
{
    if (e.n_estoque == 0) return REP_SEM_CARTAS;
    for (int c = 0; c < N_COL; c++) if (e.n[c] == 0) return REP_COLUNA_VAZIA;
    desf_empurrar();
    for (int c = 0; c < N_COL; c++) {
        carta_t x = e.estoque[--e.n_estoque];
        e.col[c][e.n[c]++] = x | 0x80;
    }
    if (e.pontos > 0) e.pontos--;
    e.movs++;
    ret_n = 0;
    tirar_completas(&e);
    return REP_OK;
}

/* ---- pista y movimiento automático ---- */

typedef struct { int de, i, para, valor; } cand_t;
#define MAX_CAND 400

static int candidatos(const estado_t *s, cand_t *v, int max)
{
    int n = 0;
    for (int c = 0; c < N_COL; c++) {
        int ini = inicio_sequencia(s, c);
        for (int i = ini; i < s->n[c]; i++) {
            if (!C_CIMA(s->col[c][i])) continue;
            carta_t x = s->col[c][i];
            bool revela = i > 0 && !C_CIMA(s->col[c][i - 1]);
            bool parte = i > 0 && C_CIMA(s->col[c][i - 1]) && C_PALO(s->col[c][i - 1]) == C_PALO(x) &&
                         C_VALOR(s->col[c][i - 1]) == C_VALOR(x) + 1;
            for (int t = 0; t < N_COL; t++) {
                if (t == c || n >= max) continue;
                int val;
                if (s->n[t] == 0) {
                    if (i == 0) continue;               /* una columna entera a otra vacía no sirve de nada */
                    val = 2;
                } else {
                    carta_t top = s->col[t][s->n[t] - 1];
                    if (C_VALOR(top) != C_VALOR(x) + 1) continue;
                    val = 10 + (C_PALO(top) == C_PALO(x) ? 20 : 0);
                }
                if (parte && val < 30) continue;        /* no romper una escalera por nada */
                if (revela) val += 8;
                val += s->n[c] - i;
                v[n].de = c; v[n].i = i; v[n].para = t; v[n].valor = val;
                n++;
            }
        }
    }
    return n;
}

/* La mejor columna para una pila (clic): palo igual > otro palo > vacía. -1 = ninguna. */
static int melhor_destino(const estado_t *s, int c, int i)
{
    carta_t x = s->col[c][i];
    int melhor = -1, mp = 0, mn = -1;
    for (int t = 0; t < N_COL; t++) {
        if (t == c) continue;
        int p;
        if (s->n[t] == 0) { if (i == 0) continue; p = 1; }
        else {
            carta_t top = s->col[t][s->n[t] - 1];
            if (C_VALOR(top) != C_VALOR(x) + 1) continue;
            p = C_PALO(top) == C_PALO(x) ? 3 : 2;
        }
        if (p > mp || (p == mp && s->n[t] > mn)) { melhor = t; mp = p; mn = s->n[t]; }
    }
    return melhor;
}

/* ------------------------------------------------------------------ autoprueba */

static int ap_total, ap_fallos;

static void ap_ok(bool cond, const char *msg)
{
    ap_total++;
    if (!cond) {
        ap_fallos++;
        registrar("spider: autoprueba FALLO: %s", msg);
        imprimir("FALLO: %s\n", msg);
    }
}

/* Que ninguna carta se pierda ni se duplique: cuenta (palo, valor) en columnas, montón y retiradas. */
static bool invariante(const estado_t *s, int niv)
{
    int cont[4][14];
    memset(cont, 0, sizeof cont);
    int total = 0;
    for (int c = 0; c < N_COL; c++)
        for (int i = 0; i < s->n[c]; i++) { cont[C_PALO(s->col[c][i])][C_VALOR(s->col[c][i])]++; total++; }
    for (int i = 0; i < s->n_estoque; i++) { cont[C_PALO(s->estoque[i])][C_VALOR(s->estoque[i])]++; total++; }
    for (int k = 0; k < s->feitas && k < N_FEITAS; k++)
        for (int v = 1; v <= 13; v++) { cont[s->feitas_palo[k]][v]++; total++; }
    if (total != 104) return false;
    int copias = 8 / niv;
    for (int p = 0; p < 4; p++)
        for (int v = 1; v <= 13; v++)
            if (cont[p][v] != (p < niv ? copias : 0)) return false;
    return true;
}

static void limpar_estado(void)
{
    memset(&e, 0, sizeof e);
    e.pontos = 500;
    desf_ini = desf_n = 0;
}

static void por(int c, int palo, int valor, bool cima)
{
    e.col[c][e.n[c]++] = FAZER(palo, valor, cima);
}

static bool iguais(const estado_t *a, const estado_t *b) { return memcmp(a, b, sizeof *a) == 0; }

static int autoprueba(uint32_t sem, int niv_forcado)
{
    gui_ativo = false;
    ap_total = ap_fallos = 0;
    registrar("spider: autoprueba inicio (semilla %u)", sem);

    /* 1. El reparto, en los tres niveles. */
    static const int niveis[3] = { 1, 2, 4 };
    for (int k = 0; k < 3; k++) {
        int niv = niveis[k];
        nova_partida(niv, sem + (uint32_t)k);
        ap_ok(invariante(&e, niv), "reparto: 104 cartas, copias correctas");
        bool formato = true, cimas = true;
        for (int c = 0; c < N_COL; c++) {
            if (e.n[c] != (c < 4 ? 6 : 5)) formato = false;
            for (int i = 0; i < e.n[c]; i++)
                if (C_CIMA(e.col[c][i]) != (i == e.n[c] - 1)) cimas = false;
        }
        ap_ok(formato, "reparto: 4 columnas de 6 y 6 de 5");
        ap_ok(cimas, "reparto: sólo la última carta boca arriba");
        ap_ok(e.n_estoque == 50 && e.pontos == 500 && e.movs == 0 && e.feitas == 0, "reparto: 50 en el montón, 500 puntos");
    }
    {   /* misma semilla, misma partida; otra semilla, otra */
        nova_partida(2, 77); estado_t a = e;
        nova_partida(2, 77); estado_t b = e;
        nova_partida(2, 78);
        ap_ok(iguais(&a, &b), "la semilla repite la partida");
        ap_ok(!iguais(&a, &e), "otra semilla da otra partida");
    }

    /* 2. Qué se puede coger y dónde se puede soltar (nivel de 2 palos). */
    nivel = 2;
    limpar_estado();
    por(0, P_PICAS, 9, false);
    por(0, P_PICAS, 6, true); por(0, P_PICAS, 5, true); por(0, P_CORAZONES, 4, true); por(0, P_CORAZONES, 3, true);
    por(1, P_CORAZONES, 7, true);
    por(2, P_PICAS, 6, true);
    por(3, P_PICAS, 5, true);
    ap_ok(!pode_pegar(&e, 0, 0), "no se coge una carta boca abajo");
    ap_ok(pode_pegar(&e, 0, 4) && pode_pegar(&e, 0, 3), "se coge una carta y una escalera del mismo palo");
    ap_ok(!pode_pegar(&e, 0, 2) && !pode_pegar(&e, 0, 1), "no se coge una pila de palos mezclados");
    ap_ok(pode_soltar(&e, 1, 6), "6 sobre 7 de otro palo: se puede");
    ap_ok(!pode_soltar(&e, 1, 5), "5 sobre 7: no");
    ap_ok(!pode_soltar(&e, 1, 7), "7 sobre 7: no");
    ap_ok(pode_soltar(&e, 5, 12), "columna vacía: se puede soltar cualquier cosa");
    {
        estado_t antes = e;
        ap_ok(!executar_jogada(0, 4, 3), "movimiento ilegal rechazado (3 sobre 5 no)");
        ap_ok(iguais(&antes, &e), "el movimiento ilegal no cambia nada");
        ap_ok(executar_jogada(0, 3, 1) == false, "4-3 sobre 7: no vale (4 sobre 7)");
        ap_ok(executar_jogada(0, 2, 1) == false, "pila mezclada no se mueve");
        ap_ok(executar_jogada(2, 0, 1), "6 de picas sobre 7 de corazones");
        ap_ok(e.pontos == 499 && e.movs == 1 && e.n[2] == 0 && e.n[1] == 2, "movimiento: -1 punto, +1 movimiento");
        ap_ok(desf_voltar() && iguais(&antes, &e), "deshacer devuelve la partida exacta");
        ap_ok(!desf_voltar(), "sin nada que deshacer");
    }

    /* 3. Revelar la carta de debajo y completar K..A (el 13 rojo de la columna 0 cae en el último paso). */
    limpar_estado();
    por(0, P_PICAS, 3, false);                       /* se revela al sacar la escalera */
    for (int v = 13; v >= 2; v--) por(0, P_PICAS, v, true);
    por(1, P_PICAS, 1, true);
    ap_ok(inicio_sequencia(&e, 0) == 1, "K..2 es una sola escalera");
    ap_ok(executar_jogada(1, 0, 0), "el A de picas se pone sobre el 2");
    ap_ok(e.feitas == 1 && e.feitas_palo[0] == P_PICAS, "K..A retirada: una escalera completa");
    ap_ok(e.pontos == 500 - 1 + 100, "puntos: 500 - 1 + 100");
    ap_ok(e.n[0] == 1 && C_CIMA(e.col[0][0]) && C_VALOR(e.col[0][0]) == 3, "se da la vuelta la carta que queda debajo");
    ap_ok(desf_voltar() && e.feitas == 0 && e.n[0] == 13 && !C_CIMA(e.col[0][0]), "deshacer devuelve la escalera");

    /* 4. Un reparto con una columna vacía no se admite; sin ella sí. */
    nova_partida(1, sem + 10);
    {
        estado_t a = e;
        e.n[3] = 0;
        estado_t b = e;
        ap_ok(executar_reparto() == REP_COLUNA_VAZIA && iguais(&b, &e), "no se reparte con una columna vacía");
        e = a;
        ap_ok(executar_reparto() == REP_OK, "se reparte sin columnas vacías");
        bool ok = e.n_estoque == 40 && e.pontos == 499 && e.movs == 1;
        for (int c = 0; c < N_COL; c++) if (e.n[c] != a.n[c] + 1 || !C_CIMA(e.col[c][e.n[c] - 1])) ok = false;
        ap_ok(ok, "reparto: una carta boca arriba por columna, 40 en el montón");
        ap_ok(invariante(&e, 1), "reparto: no se pierde ninguna carta");
        ap_ok(desf_voltar() && iguais(&a, &e), "deshacer el reparto");
        e.n_estoque = 0;
        ap_ok(executar_reparto() == REP_SEM_CARTAS, "sin montón no se reparte");
    }

    /* 5. Pista y clic: la pista que da siempre es legal. */
    nova_partida(1, sem + 20);
    {
        cand_t v[MAX_CAND];
        int n = candidatos(&e, v, MAX_CAND);
        bool legales = true;
        for (int k = 0; k < n; k++)
            if (!pode_pegar(&e, v[k].de, v[k].i) || !pode_soltar(&e, v[k].para, C_VALOR(e.col[v[k].de][v[k].i]))) legales = false;
        ap_ok(legales, "todas las pistas son movimientos legales");
    }

    /* 6. Una partida jugada sola con las pistas (sin repetir posiciones): se conservan las 104 cartas
     * y deshacer funciona. */
    static uint32_t visto[1024];
    for (int k = 0; k < 3; k++) {
        int niv = niv_forcado ? niv_forcado : niveis[k];
        nova_partida(niv, sem + 100 + (uint32_t)k);
        memset(visto, 0, sizeof visto);
        int pasos = 0;
        bool inv = true;
        while (pasos < 800 && e.feitas < 8) {
            cand_t v[MAX_CAND];
            int n = candidatos(&e, v, MAX_CAND);
            bool fez = false;
            for (int tent = 0; tent < n && !fez; tent++) {
                int melhor = -1;
                for (int q = 0; q < n; q++)
                    if (v[q].valor >= 0 && (melhor < 0 || v[q].valor > v[melhor].valor)) melhor = q;
                if (melhor < 0 || v[melhor].valor < 10) break;
                cand_t c = v[melhor];
                v[melhor].valor = -1;
                estado_t salvo = e;
                int tmp_desf = desf_n;
                (void)tmp_desf;
                executar_jogada(c.de, c.i, c.para);
                uint32_t hh = 2166136261u;
                for (size_t q = 0; q < sizeof(e.col) + sizeof(e.n); q++) hh = (hh ^ ((const uint8_t *)&e)[q]) * 16777619u;
                if (visto[hh & 1023] == hh) { e = salvo; desf_n--; continue; }
                visto[hh & 1023] = hh;
                fez = true;
            }
            if (!fez && executar_reparto() == REP_OK) fez = true;
            if (!fez) break;
            if (!invariante(&e, niv)) { inv = false; break; }
            pasos++;
        }
        ap_ok(inv, "la partida jugada sola conserva las 104 cartas");
        estado_t fim = e;
        int voltas = 0;
        while (desf_voltar()) voltas++;
        ap_ok(iguais(&e, &e_inicial) || voltas >= MAX_DESF - 1, "deshacerlo todo vuelve al reparto");
        registrar("spider: autoprueba partida nivel %d: %d pasos, %d escaleras, %d puntos", niv, pasos, fim.feitas, fim.pontos);
    }

    /* 7. Ganar: ocho escaleras retiradas. */
    limpar_estado();
    e.feitas = 7;
    for (int k = 0; k < 7; k++) e.feitas_palo[k] = P_PICAS;
    for (int v = 13; v >= 2; v--) por(0, P_PICAS, v, true);
    por(1, P_PICAS, 1, true);
    ap_ok(e.feitas < 8, "antes de la última escalera no se ha ganado");
    ap_ok(executar_jogada(1, 0, 0) && e.feitas == 8, "la octava escalera gana la partida");

    registrar("spider: autoprueba %s (%d comprobaciones, %d fallos)", ap_fallos ? "FALLO" : "ok", ap_total, ap_fallos);
    imprimir("spider autoprueba: %s (%d comprobaciones, %d fallos)\n", ap_fallos ? "FALLO" : "ok", ap_total, ap_fallos);
    return ap_fallos ? 1 : 0;
}

/* ------------------------------------------------------------------ dibujo básico */

/* Destino de todo el dibujo: la escena, un sprite o la propia ventana. */
static uint32_t *T_px;
static int       T_l, T_a;
static int       cx0, cy0, cx1, cy1;      /* recorte */

static void alvo(uint32_t *px, int l, int a)
{
    T_px = px; T_l = l; T_a = a;
    cx0 = 0; cy0 = 0; cx1 = l; cy1 = a;
}

static void recorte_tudo(void) { cx0 = 0; cy0 = 0; cx1 = T_l; cy1 = T_a; }

/* Construir un sprite cambia el destino de dibujo: quien lo pide en medio de otra cosa lo recupera. */
typedef struct { uint32_t *px; int l, a, x0, y0, x1, y1; } alvo_salvo_t;
static alvo_salvo_t alvo_guardar(void)
{
    alvo_salvo_t s = { T_px, T_l, T_a, cx0, cy0, cx1, cy1 };
    return s;
}
static void alvo_restaurar(const alvo_salvo_t *s)
{
    T_px = s->px; T_l = s->l; T_a = s->a;
    cx0 = s->x0; cy0 = s->y0; cx1 = s->x1; cy1 = s->y1;
}

/* a = 0..256 */
static inline uint32_t misturar(uint32_t d, uint32_t c, int a)
{
    uint32_t rb = (((d & 0xFF00FF) * (uint32_t)(256 - a) + (c & 0xFF00FF) * (uint32_t)a) >> 8) & 0xFF00FF;
    uint32_t g  = (((d & 0x00FF00) * (uint32_t)(256 - a) + (c & 0x00FF00) * (uint32_t)a) >> 8) & 0x00FF00;
    return rb | g;
}

static inline void ponto(int x, int y, uint32_t c, int a)
{
    if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 || a <= 0) return;
    uint32_t *p = &T_px[y * T_l + x];
    *p = a >= 256 ? c : misturar(*p, c, a);
}

static void ret(int x, int y, int w, int h, uint32_t c, int a)
{
    int x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x >= x1 || a <= 0) return;
    for (int yy = y; yy < y1; yy++) {
        uint32_t *p = T_px + yy * T_l + x;
        if (a >= 256) preencher32(p, c, (size_t)(x1 - x));
        else for (int xx = x; xx < x1; xx++, p++) *p = misturar(*p, c, a);
    }
}

static inline int clamp256(int v) { return v < 0 ? 0 : v > 256 ? 256 : v; }

/* Cobertura (0..256) del píxel (dx, dy) de una esquina de radio r; (0,0) es la punta de la esquina. */
static inline int cobertura(int dx, int dy, int r)
{
    int ex = 2 * r - (2 * dx + 1), ey = 2 * r - (2 * dy + 1);
    return clamp256(128 + (4 * r * r - ex * ex - ey * ey) * 32 / r);
}

/* Rectángulo redondeado con bordes suaves. */
static void rrect(int x, int y, int w, int h, int r, uint32_t c, int a)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int dy = 0; dy < h; dy++) {
        int yy = y + dy;
        if (yy < cy0 || yy >= cy1) continue;
        if (r > 0 && (dy < r || dy >= h - r)) {
            int ry = dy < r ? dy : h - 1 - dy;
            for (int dx = 0; dx < r; dx++) {
                int cv = cobertura(dx, ry, r);
                if (cv <= 0) continue;
                int aa = a >= 256 ? cv : (cv * a) >> 8;
                ponto(x + dx, yy, c, aa);
                ponto(x + w - 1 - dx, yy, c, aa);
            }
            ret(x + r, yy, w - 2 * r, 1, c, a);
        } else {
            ret(x, yy, w, 1, c, a);
        }
    }
}

/* Disco con centro (cx2/2, cy2/2) y radio r2/2 (medios píxeles: los centros caen entre píxeles). */
static void disco2(int cx2, int cy2, int r2, uint32_t c, int a)
{
    int x0 = (cx2 - r2) / 2 - 1, x1 = (cx2 + r2) / 2 + 2;
    int y0 = (cy2 - r2) / 2 - 1, y1 = (cy2 + r2) / 2 + 2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int ex = 2 * x + 1 - cx2, ey = 2 * y + 1 - cy2;
            int cv = clamp256(128 + (r2 * r2 - ex * ex - ey * ey) * 64 / (r2 ? r2 : 1));
            if (cv > 0) ponto(x, y, c, a >= 256 ? cv : (cv * a) >> 8);
        }
}

/* Triángulo relleno con 3x3 muestras por píxel. Vértices en píxeles. */
static void triangulo(int ax, int ay, int bx, int by, int cx, int cy, uint32_t c, int a)
{
    int minx = ax < bx ? ax : bx, maxx = ax > bx ? ax : bx;
    int miny = ay < by ? ay : by, maxy = ay > by ? ay : by;
    if (cx < minx) minx = cx;
    if (cx > maxx) maxx = cx;
    if (cy < miny) miny = cy;
    if (cy > maxy) maxy = cy;
    int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area == 0) return;
    int sg = area > 0 ? 1 : -1;
    for (int y = miny - 1; y <= maxy; y++)
        for (int x = minx - 1; x <= maxx; x++) {
            int cnt = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    int px = (x * 3 + sx) * 2 + 1, py = (y * 3 + sy) * 2 + 1;       /* en sextos de píxel */
                    int w0 = ((bx - ax) * 6) * (py - ay * 6) - ((by - ay) * 6) * (px - ax * 6);
                    int w1 = ((cx - bx) * 6) * (py - by * 6) - ((cy - by) * 6) * (px - bx * 6);
                    int w2 = ((ax - cx) * 6) * (py - cy * 6) - ((ay - cy) * 6) * (px - cx * 6);
                    if (sg * w0 >= 0 && sg * w1 >= 0 && sg * w2 >= 0) cnt++;
                }
            if (cnt) ponto(x, y, c, (cnt * 256 / 9) * a >> 8);
        }
}

/* Texto de 8x16 con la letra del sistema. */
static void texto(int x, int y, const char *t, uint32_t cor)
{
    for (; *t; t++, x += LETRA_L) {
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int lin = 0; lin < 16; lin++)
            for (int col = 0; col < 8; col++)
                if (g[lin] & (0x80 >> col)) ponto(x + col, y + lin, cor, 256);
    }
}

static void texto_neg(int x, int y, const char *t, uint32_t cor)
{
    texto(x, y, t, cor);
    texto(x + 1, y, t, cor);
}

static void texto_sombra(int x, int y, const char *t, uint32_t cor)
{
    texto_neg(x + 1, y + 1, t, 0x0A1A10);
    texto_neg(x, y, t, cor);
}

/* Una letra del sistema ampliada (o reducida) a gw x gh píxeles, por vecino más cercano. */
static void letra_esc(int x, int y, int ch, int gw, int gh, uint32_t cor)
{
    const unsigned char *g = fonte_sagui[(uint8_t)ch];
    for (int dy = 0; dy < gh; dy++) {
        int sy = dy * 16 / gh;
        for (int dx = 0; dx < gw; dx++) {
            int sx = dx * 8 / gw;
            if (g[sy] & (0x80 >> sx)) ponto(x + dx, y + dy, cor, 256);
        }
    }
}

/* ------------------------------------------------------------------ palos vectoriales */

#define K_PALO 330        /* medio lado del cuadrado de la figura, en 1/256 de la unidad de forma */

static int d2(int x, int y, int cx, int cy) { return (x - cx) * (x - cx) + (y - cy) * (y - cy); }
#define CIRC(x, y, cx, cy, r) (d2(x, y, cx, cy) <= (r) * (r))
static int absi(int v) { return v < 0 ? -v : v; }

/* (x, y) en unidades de 1/256, y hacia arriba. */
static bool forma_coracao(int x, int y)
{
    if (CIRC(x, y, -133, 108, 164) || CIRC(x, y, 133, 108, 164)) return true;
    return y <= 108 && y >= -256 && absi(x) * 5 <= (y + 256) * 4;
}

static bool dentro_palo(int palo, int x, int y)
{
    switch (palo) {
    case P_CORAZONES: return forma_coracao(x, y);
    case P_DIAMANTES: return absi(x) * 120 + absi(y) * 85 <= 26112;
    case P_PICAS:
        if (forma_coracao(x, 56 - y)) return true;
        return y <= -77 && y >= -320 && absi(x) <= 10 + (-77 - y) * 130 / 243;
    default:   /* tréboles */
        if (CIRC(x, y, 0, 158, 133) || CIRC(x, y, -158, -46, 133) || CIRC(x, y, 158, -46, 133) || CIRC(x, y, 0, 0, 100)) return true;
        return y <= -51 && y >= -320 && absi(x) <= 12 + (-51 - y) * 130 / 270;
    }
}

static inline bool palo_vermelho(int palo) { return palo == P_CORAZONES || palo == P_DIAMANTES; }
#define COR_VERMELHO 0xC8161F
#define COR_PRETO    0x15171C

/* Dibuja el palo en el cuadrado de lado s centrado en (cx, cy). */
static void palo_desenhar(int palo, int cx, int cy, int s, uint32_t cor, int a)
{
    if (s < 4) return;
    int xs[3 * 128], ys[3 * 128];
    if (s > 128) s = 128;
    for (int k = 0; k < 3 * s; k++) {
        xs[k] = ((2 * k + 1) * K_PALO) / (3 * s) - K_PALO;
        ys[k] = K_PALO - ((2 * k + 1) * K_PALO) / (3 * s);
    }
    int ox = cx - s / 2, oy = cy - s / 2;
    for (int j = 0; j < s; j++)
        for (int i = 0; i < s; i++) {
            int cnt = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++)
                    if (dentro_palo(palo, xs[i * 3 + sx], ys[j * 3 + sy])) cnt++;
            if (cnt) ponto(ox + i, oy + j, cor, (cnt * 256 / 9) * a >> 8);
        }
}

/* ------------------------------------------------------------------ geometría */

#define BARRA_A   30
#define UP_MIN    13
#define DN_MIN    4

static struct {
    int l, a;
    int cw, ch, gap, x0, margem, r;
    int ytab, yfundo;
    int dn_base, up_base;
} L;

static void calcular_layout(int l, int a)
{
    L.l = l; L.a = a;
    L.margem = l / 80 < 6 ? 6 : l / 80;
    L.gap = l / 110 < 4 ? 4 : l / 110;
    L.cw = (l - 2 * L.margem - 9 * L.gap) / 10;
    if (L.cw < 40) L.cw = 40;
    if (L.cw > 110) L.cw = 110;
    L.ch = L.cw * 14 / 10;
    L.r = L.cw / 11 < 3 ? 3 : L.cw / 11;
    L.x0 = (l - (10 * L.cw + 9 * L.gap)) / 2;
    L.ytab = BARRA_A + 10;
    L.yfundo = a - L.ch - L.margem;
    L.dn_base = L.ch / 12 < 5 ? 5 : L.ch / 12;
    L.up_base = L.ch / 4;
}

static int x_col(int c) { return L.x0 + c * (L.cw + L.gap); }

static void desloc_col(const estado_t *s, int c, int *dn, int *up)
{
    int n = s->n[c], nb = 0;
    while (nb < n && !C_CIMA(s->col[c][nb])) nb++;
    int nu = n - nb;
    int d = L.dn_base, u = L.up_base;
    int disp = L.yfundo - 6 - L.ytab - L.ch;
    int precisa = nb * d + (nu > 1 ? (nu - 1) * u : 0);
    if (precisa > disp) {
        if (nu > 1) u = (disp - nb * d) / (nu - 1);
        if (nu <= 1 || u < UP_MIN) {
            if (nu > 1) u = UP_MIN;
            if (nb) {
                d = (disp - (nu > 1 ? (nu - 1) * u : 0)) / nb;
                if (d > L.dn_base) d = L.dn_base;
                if (d < DN_MIN) d = DN_MIN;
            }
        }
        if (u > L.up_base) u = L.up_base;
        if (u < 8) u = 8;
    }
    *dn = d; *up = u;
}

static int y_carta(const estado_t *s, int c, int i)
{
    int dn, up;
    desloc_col(s, c, &dn, &up);
    int y = L.ytab;
    for (int k = 0; k < i; k++) y += C_CIMA(s->col[c][k]) ? up : dn;
    return y;
}

static void posicao_carta(const estado_t *s, int c, int i, int *x, int *y)
{
    *x = x_col(c);
    *y = y_carta(s, c, i);
}

/* La carta de la columna c bajo la altura y (-1 = ninguna). */
static int carta_em(const estado_t *s, int c, int y)
{
    int n = s->n[c];
    if (n == 0) return -1;
    int dn, up;
    desloc_col(s, c, &dn, &up);
    int yy = L.ytab;
    int escolhida = -1;
    for (int i = 0; i < n; i++) {
        int alt = i == n - 1 ? L.ch : (C_CIMA(s->col[c][i]) ? up : dn);
        if (y >= yy && y < yy + alt) escolhida = i;
        yy += alt;
    }
    return escolhida;
}

static int passo_feitas(void) { return L.cw / 3; }
static int x_feita(int k) { return L.x0 + k * passo_feitas(); }
static int passo_estoque(void) { return L.cw / 5 < 3 ? 3 : L.cw / 5; }

/* Rectángulo del montón de repartos (contiene todos los dorsos apilados). */
static void rect_estoque(const estado_t *s, int *x, int *y, int *w, int *h)
{
    int q = (s->n_estoque + 9) / 10;
    if (q < 1) q = 1;
    int total = L.cw + (q - 1) * passo_estoque();
    *x = L.x0 + 10 * L.cw + 9 * L.gap - total;
    *y = L.yfundo;
    *w = total;
    *h = L.ch;
}

/* ------------------------------------------------------------------ sprites de cartas */

static uint32_t *spr_face[52];
static uint32_t *spr_dorso;
static uint8_t  *spr_mascara;
static int       spr_cw, spr_ch;

static void sprites_liberar(void)
{
    for (int i = 0; i < 52; i++) { if (spr_face[i]) libera(spr_face[i]); spr_face[i] = NULL; }
    if (spr_dorso) libera(spr_dorso);
    if (spr_mascara) libera(spr_mascara);
    spr_dorso = NULL; spr_mascara = NULL;
    spr_cw = spr_ch = 0;
}

static void sprites_preparar(void)
{
    if (spr_cw == L.cw && spr_ch == L.ch && spr_mascara) return;
    alvo_salvo_t sv = alvo_guardar();
    sprites_liberar();
    spr_cw = L.cw; spr_ch = L.ch;
    spr_mascara = aloca_zerado((size_t)spr_cw * (size_t)spr_ch);
    uint32_t *m = aloca_zerado((size_t)spr_cw * (size_t)spr_ch * 4);
    if (!spr_mascara || !m) { if (m) libera(m); alvo_restaurar(&sv); return; }
    alvo(m, spr_cw, spr_ch);
    rrect(0, 0, spr_cw, spr_ch, L.r, 0xFFFFFF, 256);
    for (int i = 0; i < spr_cw * spr_ch; i++) { int v = (int)(m[i] & 0xFF); spr_mascara[i] = (uint8_t)v; }
    libera(m);
    alvo_restaurar(&sv);
}

/* Aplica la máscara al sprite: el color ya viene premultiplicado (se dibujó sobre negro). */
static void sprite_fechar(uint32_t *sp)
{
    for (int i = 0; i < spr_cw * spr_ch; i++) sp[i] = (sp[i] & 0xFFFFFF) | ((uint32_t)spr_mascara[i] << 24);
}

/* Copia girada 180 grados una zona del sprite a la zona simétrica (esquina opuesta). */
static void copiar_girado(uint32_t *sp, int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int sx = x + i, sy = y + j;
            int dx = spr_cw - 1 - sx, dy = spr_ch - 1 - sy;
            sp[dy * spr_cw + dx] = sp[sy * spr_cw + sx];
        }
}

static const char *const nome_valor[14] = { "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };

/* Posiciones de los palos del 2 al 10: x en 0..2 (izquierda, centro, derecha), y en doceavos de la altura útil. */
static const struct { int8_t x, y; } pips[11][10] = {
    {{0,0}},
    {{0,0}},
    {{1,0}, {1,12}},
    {{1,0}, {1,6}, {1,12}},
    {{0,0}, {2,0}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {1,6}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {0,6}, {2,6}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {1,3}, {0,6}, {2,6}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {1,3}, {0,6}, {2,6}, {1,9}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {0,4}, {2,4}, {1,6}, {0,8}, {2,8}, {0,12}, {2,12}},
    {{0,0}, {2,0}, {1,2}, {0,4}, {2,4}, {0,8}, {2,8}, {1,10}, {0,12}, {2,12}},
};
static const int8_t pips_n[11] = { 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

/* La corona o el gorro de una figura, en la mitad de arriba de su panel. (x, y) = esquina, w = ancho. */
static void arte_figura(int valor, int x, int y, int w, int h, uint32_t cor)
{
    uint32_t ouro = 0xE0B020, ouro_esc = 0xA67C10;
    int bh = h / 4 < 2 ? 2 : h / 4;
    if (valor == 13) {                          /* rey: corona de tres puntas con joyas */
        ret(x, y + h - bh, w, bh, ouro, 256);
        ret(x, y + h - bh, w, 1, ouro_esc, 256);
        triangulo(x, y + h - bh, x + w / 3, y + h - bh, x, y, ouro, 256);
        triangulo(x + w / 3, y + h - bh, x + 2 * w / 3, y + h - bh, x + w / 2, y, ouro, 256);
        triangulo(x + 2 * w / 3, y + h - bh, x + w, y + h - bh, x + w, y, ouro, 256);
        disco2(2 * x + 1, 2 * y + 2, 3, cor, 256);
        disco2(2 * (x + w / 2) + 1, 2 * y + 2, 3, cor, 256);
        disco2(2 * (x + w) - 1, 2 * y + 2, 3, cor, 256);
    } else if (valor == 12) {                   /* dama: diadema con perlas */
        ret(x + 1, y + h - bh, w - 2, bh, ouro, 256);
        for (int k = 0; k < 5; k++) {
            int px = x + 2 + k * (w - 5) / 4;
            ret(px, y + h / 3 + (k == 2 ? -h / 6 : 0), 2, h - h / 3 - bh + (k == 2 ? h / 6 : 0), ouro, 256);
            disco2(2 * px + 2, 2 * (y + h / 3 + (k == 2 ? -h / 6 : 0)), 4, 0xFFFFFF, 256);
        }
    } else {                                    /* jota: gorro con pluma */
        triangulo(x + w / 6, y + h, x + w - w / 6, y + h, x + w / 2, y + h / 5, ouro, 256);
        ret(x, y + h - bh / 2 - 1, w, bh / 2 + 1, ouro_esc, 256);
        int px = x + w - w / 4;
        for (int k = 0; k < h / 2; k++) ponto(px + k / 2, y + h / 3 - k, cor, 256);
        for (int k = 0; k < h / 2; k++) ponto(px + 1 + k / 2, y + h / 3 - k, cor, 256);
    }
}

static uint32_t *criar_face(int palo, int valor)
{
    int cw = spr_cw, ch = spr_ch;
    uint32_t *sp = aloca_zerado((size_t)cw * (size_t)ch * 4);
    if (!sp) return NULL;
    alvo(sp, cw, ch);
    uint32_t cor = palo_vermelho(palo) ? COR_VERMELHO : COR_PRETO;

    rrect(0, 0, cw, ch, L.r, 0x4A5058, 256);
    rrect(1, 1, cw - 2, ch - 2, L.r > 1 ? L.r - 1 : 1, 0xFBFAF4, 256);

    /* índice arriba a la izquierda: valor y palo pequeño */
    const char *nv = nome_valor[valor];
    int tw = (int)tam_texto(nv) * 8;
    texto_neg(4 + (valor == 10 ? 0 : 1), 3, nv, cor);
    int sp_s = cw / 6 < 9 ? 9 : cw / 6;
    palo_desenhar(palo, 4 + (tw > 8 ? tw / 2 : 4) + 1, 3 + 16 + sp_s / 2 + 1, sp_s, cor, 256);
    int ix_w = tw + 8 > sp_s + 8 ? tw + 8 : sp_s + 8;

    int ay = ch * 24 / 100, ah = ch - 2 * ay;   /* zona de los palos */
    if (valor == 1) {
        palo_desenhar(palo, cw / 2, ch / 2, cw * 55 / 100, cor, 256);
        /* un aro fino alrededor, como en los ases elegantes */
    } else if (valor >= 2 && valor <= 10) {
        int ps = cw * 16 / 100 < 8 ? 8 : cw * 16 / 100;
        int xc[3] = { cw * 35 / 100, cw / 2, cw * 65 / 100 };
        for (int k = 0; k < pips_n[valor]; k++) {
            int px = xc[(int)pips[valor][k].x];
            int py = ay + pips[valor][k].y * ah / 12;
            palo_desenhar(palo, px, py, ps, cor, 256);
        }
    } else {
        /* J, Q, K: un panel dorado con la figura de cabeza y la misma al revés (giro de 180 grados) */
        int px0 = cw * 22 / 100, py0 = ch * 14 / 100, pw = cw - 2 * px0, ph = ch - 2 * py0;
        uint32_t fundo = valor == 13 ? 0xFFF0C4 : valor == 12 ? 0xFFE2E6 : 0xDCEBFF;
        rrect(px0, py0, pw, ph, 3, 0xB8901C, 256);
        rrect(px0 + 1, py0 + 1, pw - 2, ph - 2, 2, fundo, 256);
        int mh = ph / 2 - 3;                              /* alto de la mitad útil */
        int ch_ = mh * 30 / 100 < 5 ? 5 : mh * 30 / 100;  /* alto de la corona */
        arte_figura(valor, px0 + 5, py0 + 4, pw - 10, ch_, cor);
        int lh = mh - ch_ - 5, lw = lh * 6 / 10;
        if (lh < 12) { lh = 12; lw = 8; }
        int ly = py0 + 4 + ch_ + 3;
        char l1[2] = { nome_valor[valor][0], 0 };
        letra_esc(px0 + pw / 2 - lw - 1, ly, l1[0], lw, lh, cor);
        letra_esc(px0 + pw / 2 - lw, ly, l1[0], lw, lh, cor);
        palo_desenhar(palo, px0 + pw / 2 + lw / 2 + 4, ly + lh / 2, lw + 3 > pw / 4 ? pw / 4 : lw + 3, cor, 256);
        /* línea de la mitad y la mitad de abajo, girada */
        for (int j = 0; j < ph / 2; j++)
            for (int i = 0; i < pw; i++) {
                int sx = px0 + i, sy = py0 + j;
                int dx = px0 + pw - 1 - i, dy = py0 + ph - 1 - j;
                if (dy > sy) sp[dy * cw + dx] = sp[sy * cw + sx];
            }
        ret(px0 + 1, py0 + ph / 2, pw - 2, 1, 0xB8901C, 256);
    }
    (void)ix_w;

    /* el índice de abajo a la derecha: el de arriba, girado */
    copiar_girado(sp, 3, 3, ix_w + 2 < cw / 3 ? ix_w + 2 : cw / 3, 16 + sp_s + 4);
    sprite_fechar(sp);
    return sp;
}

static uint32_t *criar_dorso(void)
{
    int cw = spr_cw, ch = spr_ch;
    uint32_t *sp = aloca_zerado((size_t)cw * (size_t)ch * 4);
    if (!sp) return NULL;
    alvo(sp, cw, ch);
    rrect(0, 0, cw, ch, L.r, 0x30343C, 256);
    rrect(1, 1, cw - 2, ch - 2, L.r - 1 > 1 ? L.r - 1 : 1, 0xF4F1E6, 256);
    int m = 4;
    rrect(m, m, cw - 2 * m, ch - 2 * m, L.r > 3 ? L.r - 2 : 2, 0x17398A, 256);
    /* degradado vertical suave */
    for (int y = m; y < ch - m; y++) {
        int t = (y - m) * 256 / (ch - 2 * m);
        ret(m + 1, y, cw - 2 * m - 2, 1, 0x0A1F5C, t * 90 / 256);
    }
    /* celosía de rombos */
    int paso = cw / 7 < 6 ? 6 : cw / 7;
    for (int y = m + 2; y < ch - m - 2; y++)
        for (int x = m + 2; x < cw - m - 2; x++) {
            int u = (x + y) % paso, v = (x - y + 4096) % paso;
            if (u == 0 || v == 0) ponto(x, y, 0x5C82D8, 130);
        }
    /* marco dorado interior */
    int m2 = m + 3;
    for (int k = 0; k < 1; k++) {
        ret(m2, m2, cw - 2 * m2, 1, 0xE2B536, 256);
        ret(m2, ch - m2 - 1, cw - 2 * m2, 1, 0xE2B536, 256);
        ret(m2, m2, 1, ch - 2 * m2, 0xE2B536, 256);
        ret(cw - m2 - 1, m2, 1, ch - 2 * m2, 0xE2B536, 256);
    }
    /* medallón con la cara de un sagui */
    int R = cw * 26 / 100;
    int cx2 = cw, cy2 = ch;                              /* centro en medios píxeles */
    disco2(cx2, cy2, 2 * R + 4, 0xE2B536, 256);
    disco2(cx2, cy2, 2 * R + 1, 0x0B5A2E, 256);
    disco2(cx2 - R * 2 * 8 / 10, cy2 - R / 3, R * 2 * 38 / 100, 0x6E553C, 256);     /* orejas */
    disco2(cx2 + R * 2 * 8 / 10, cy2 - R / 3, R * 2 * 38 / 100, 0x6E553C, 256);
    disco2(cx2 - R * 2 * 8 / 10, cy2 - R / 3, R * 2 * 20 / 100, 0xC89A82, 256);
    disco2(cx2 + R * 2 * 8 / 10, cy2 - R / 3, R * 2 * 20 / 100, 0xC89A82, 256);
    disco2(cx2, cy2 + 1, R * 2 * 82 / 100, 0x6E553C, 256);                           /* cabeza */
    disco2(cx2, cy2 - R * 2 * 40 / 100, R * 2 * 42 / 100, 0xF5F5F0, 256);            /* mechón blanco */
    disco2(cx2, cy2 + R * 2 * 16 / 100, R * 2 * 58 / 100, 0xD6B48C, 256);            /* cara */
    int ro = R * 2 * 12 / 100 < 2 ? 2 : R * 2 * 12 / 100;
    disco2(cx2 - R * 2 * 26 / 100, cy2 + R * 2 * 4 / 100, ro, 0x1A1410, 256);        /* ojos */
    disco2(cx2 + R * 2 * 26 / 100, cy2 + R * 2 * 4 / 100, ro, 0x1A1410, 256);
    disco2(cx2, cy2 + R * 2 * 36 / 100, ro, 0x5A3A2A, 256);                          /* nariz */
    sprite_fechar(sp);
    return sp;
}

static uint32_t *sprite_face(int palo, int valor)
{
    sprites_preparar();
    int k = palo * 13 + valor - 1;
    if (!spr_face[k]) {
        alvo_salvo_t sv = alvo_guardar();
        spr_face[k] = criar_face(palo, valor);
        alvo_restaurar(&sv);
    }
    return spr_face[k];
}

static uint32_t *sprite_dorso(void)
{
    sprites_preparar();
    if (!spr_dorso) {
        alvo_salvo_t sv = alvo_guardar();
        spr_dorso = criar_dorso();
        alvo_restaurar(&sv);
    }
    return spr_dorso;
}

/* Pone un sprite en (x, y) del destino, mezclando por el alfa de arriba. Sólo las "vh" primeras filas. */
static void blit(const uint32_t *sp, int x, int y, int vh)
{
    if (!sp) return;
    int w = spr_cw;
    if (vh > spr_ch) vh = spr_ch;
    for (int j = 0; j < vh; j++) {
        int yy = y + j;
        if (yy < cy0 || yy >= cy1) continue;
        const uint32_t *s = sp + j * w;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < cx0 || xx >= cx1) continue;
            uint32_t p = s[i];
            uint32_t a = p >> 24;
            if (a == 0) continue;
            uint32_t *d = &T_px[yy * T_l + xx];
            if (a >= 255) *d = p & 0xFFFFFF;
            else {
                uint32_t dd = *d;
                uint32_t ia = 256 - a;
                uint32_t rb = ((((dd & 0xFF00FF) * ia) >> 8) & 0xFF00FF) + (p & 0xFF00FF);
                uint32_t g = ((((dd & 0x00FF00) * ia) >> 8) & 0x00FF00) + (p & 0x00FF00);
                uint32_t sat = rb & 0x01000100;               /* un canal pasó de 255: se queda en 255 */
                rb = (rb | (sat - (sat >> 8))) & 0x00FF00FF;
                if (g & 0x10000) g = 0xFF00;
                *d = rb | (g & 0x00FF00);
            }
        }
    }
}

static void sombra_carta(int x, int y, int vh, int fundo)
{
    rrect(x + 2, y + 2, L.cw, vh + 2 > L.ch ? L.ch : vh + 2, L.r, 0x000000, fundo);
}

/* ------------------------------------------------------------------ escena */

static jan_t    *jan;
static uint32_t *tapete;               /* fondo con degradado, del tamaño de la ventana */
static uint32_t *cena;                 /* escena completa sin lo que se arrastra */

static int      ocultar_col = -1, ocultar_de;     /* cartas que no se dibujan (arrastre, animaciones) */
static int      ocultar_ult_desde = N_COL;        /* reparto: la última carta de las columnas >= k aún no aterrizó */
static int      feitas_vis = -1;                  /* escaleras que se muestran abajo (-1 = todas) */

typedef struct { int x, y, w, h; } caixa_r;
static caixa_r  bt_juego, bt_nueva, bt_deshacer, bt_pista;
static char     msg[80];
static uint32_t msg_ate;

typedef enum { M_JOGO, M_NIVEL, M_VITORIA } modo_t;
static modo_t   modo = M_JOGO;
static caixa_r  nb_rect[3];
static int      niv_dialogo = 1;

static void definir_msg(const char *t, unsigned ms)
{
    copia_texto(msg, t, sizeof msg);
    msg_ate = tempo_ms() + ms;
}

static void tapete_construir(void)
{
    int l = jan->larg, a = jan->alt;
    if (tapete) libera(tapete);
    tapete = aloca((size_t)l * (size_t)a * 4);
    if (!tapete) return;
    uint32_t h = 12345;
    for (int y = 0; y < a; y++) {
        int dy = 2 * y - a;
        int ty = dy * dy * 256 / (a * a);
        for (int x = 0; x < l; x++) {
            int dx = 2 * x - l;
            int tx = dx * dx * 256 / (l * l);
            int t = (tx + ty) / 2;                           /* 0 en el centro, ~256 en la esquina */
            int r = 20 - t * 14 / 256, g = 140 - t * 78 / 256, b = 72 - t * 42 / 256;
            h = h * 1103515245u + 12345u;
            int ruido = (int)((h >> 20) & 7) - 3;
            r += ruido / 2; g += ruido; b += ruido / 2;
            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            tapete[y * l + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}

static void caixa_botao(const caixa_r *b, const char *t, bool ativo)
{
    rrect(b->x + 1, b->y + 2, b->w, b->h, 5, 0x000000, 70);
    rrect(b->x, b->y, b->w, b->h, 5, ativo ? 0x3C8A58 : 0x2A5A3C, 256);
    rrect(b->x + 1, b->y + 1, b->w - 2, b->h - 2, 4, ativo ? 0x2E7447 : 0x1F4A30, 256);
    texto_neg(b->x + (b->w - (int)tam_texto(t) * 8) / 2, b->y + (b->h - 16) / 2, t, ativo ? 0xF2F2E8 : 0x9DB8A6);
}

static void desenhar_barra(void)
{
    ret(0, 0, L.l, BARRA_A, 0x06200F, 150);
    ret(0, BARRA_A, L.l, 1, 0x000000, 90);
    ret(0, BARRA_A + 1, L.l, 1, 0x5FB07C, 50);
    int x = 6, y = 3, h = BARRA_A - 6;
    const char *rot[4] = { "Juego", "Nueva", "Deshacer", "Pista" };
    caixa_r *bt[4] = { &bt_juego, &bt_nueva, &bt_deshacer, &bt_pista };
    for (int k = 0; k < 4; k++) {
        int w = (int)tam_texto(rot[k]) * 8 + 20;
        bt[k]->x = x; bt[k]->y = y; bt[k]->w = w; bt[k]->h = h;
        bool ativo = k != 2 || desf_n > 0;
        caixa_botao(bt[k], rot[k], ativo);
        x += w + 6;
    }
    char s[96];
    formatar_texto(s, sizeof s, "Puntos: %d   Mov.: %d   %d %s", e.pontos, e.movs, nivel, nivel == 1 ? "palo" : "palos");
    int w = (int)tam_texto(s) * 8;
    texto_sombra(L.l - w - 10, 7, s, 0xF2F2E8);
}

static void desenhar_ranhura(int x, int y)
{
    rrect(x, y, L.cw, L.ch, L.r, 0x000000, 40);
    rrect(x + 1, y + 1, L.cw - 2, L.ch - 2, L.r > 1 ? L.r - 1 : 1, 0x0A6A38, 90);
    rrect(x + 2, y + 2, L.cw - 4, L.ch - 4, L.r > 2 ? L.r - 2 : 1, 0x000000, 28);
}

static void desenhar_cena(void)
{
    if (!cena || !tapete) return;
    sprites_preparar();
    alvo(cena, jan->larg, jan->alt);
    memcpy(cena, tapete, (size_t)jan->larg * (size_t)jan->alt * 4);
    desenhar_barra();

    for (int c = 0; c < N_COL; c++) desenhar_ranhura(x_col(c), L.ytab);

    /* completadas, abajo a la izquierda */
    int nf = feitas_vis >= 0 ? feitas_vis : e.feitas;
    for (int k = 0; k < N_FEITAS; k++)
        if (k >= nf) desenhar_ranhura(x_feita(k), L.yfundo);
    for (int k = 0; k < nf && k < N_FEITAS; k++) {
        int x = x_feita(k), vh = L.ch;
        recorte_tudo();
        sombra_carta(x, L.yfundo, vh, 60);
        blit(sprite_face(e.feitas_palo[k], 13), x, L.yfundo, vh);
    }

    /* montón de repartos, abajo a la derecha */
    {
        int q = (e.n_estoque + 9) / 10, sx, sy, sw, sh;
        rect_estoque(&e, &sx, &sy, &sw, &sh);
        if (q == 0) desenhar_ranhura(sx, sy);
        for (int k = 0; k < q; k++) {
            int x = sx + k * passo_estoque(), vh = L.ch;
            sombra_carta(x, sy, vh, 60);
            blit(sprite_dorso(), x, sy, vh);
        }
    }

    /* columnas */
    for (int c = 0; c < N_COL; c++) {
        int dn, up;
        desloc_col(&e, c, &dn, &up);
        int y = L.ytab;
        for (int i = 0; i < e.n[c]; i++) {
            carta_t x = e.col[c][i];
            int alt = i == e.n[c] - 1 ? L.ch : (C_CIMA(x) ? up : dn);
            bool oculta = (c == ocultar_col && i >= ocultar_de) || (i == e.n[c] - 1 && c >= ocultar_ult_desde);
            if (!oculta) {
                int vh = alt + 2 < L.ch ? alt + 2 : L.ch;
                sombra_carta(x_col(c), y, vh, 55);
                blit(C_CIMA(x) ? sprite_face(C_PALO(x), C_VALOR(x)) : sprite_dorso(), x_col(c), y, vh);
            }
            y += alt;
        }
    }

    /* mensaje, abajo en el medio */
    if (msg[0] && tempo_ms() < msg_ate) {
        int w = (int)tam_texto(msg) * 8;
        int fx0 = x_feita(7) + L.cw, fx1 = L.l - 10;
        int sx, sy, sw, sh;
        rect_estoque(&e, &sx, &sy, &sw, &sh);
        fx1 = sx;
        int tx = fx0 + (fx1 - fx0 - w) / 2;
        if (tx < fx0) tx = fx0;
        texto_sombra(tx, L.yfundo + L.ch / 2 - 8, msg, 0xFFF3B0);
    }

    /* diálogo de nivel */
    if (modo == M_NIVEL) {
        ret(0, 0, jan->larg, jan->alt, 0x000000, 140);
        int pw = L.l - 40 < 560 ? L.l - 40 : 560, ph = 230;
        int px = (L.l - pw) / 2, py = (L.a - ph) / 2;
        rrect(px - 3, py - 3, pw + 6, ph + 6, 14, 0x000000, 90);
        rrect(px, py, pw, ph, 12, 0xD9B33A, 256);
        rrect(px + 2, py + 2, pw - 4, ph - 4, 11, 0x12402A, 256);
        const char *tit = "Nueva partida";
        texto_sombra(px + (pw - (int)tam_texto(tit) * 8) / 2, py + 14, tit, 0xFFE9A0);
        const char *sub = "Elige la dificultad";
        texto(px + (pw - (int)tam_texto(sub) * 8) / 2, py + 36, sub, 0xC8DDCF);
        int bw = (pw - 4 * 16) / 3, bh = 112, by = py + 66;
        const int niv[3] = { 1, 2, 4 };
        const char *nom[3] = { "Fácil - 1 palo", "Medio - 2 palos", "Difícil - 4 palos" };
        for (int k = 0; k < 3; k++) {
            int bx = px + 16 + k * (bw + 16);
            nb_rect[k].x = bx; nb_rect[k].y = by; nb_rect[k].w = bw; nb_rect[k].h = bh;
            bool sel = niv_dialogo == niv[k];
            rrect(bx, by, bw, bh, 8, sel ? 0xFFD84A : 0x3F7A58, 256);
            rrect(bx + 2, by + 2, bw - 4, bh - 4, 7, sel ? 0x1E5A3A : 0x0E3622, 256);
            int ps = bw / 4 > 34 ? 34 : bw / 4;
            for (int q = 0; q < niv[k]; q++) {
                int sx = bx + bw / 2 + (q * 2 - (niv[k] - 1)) * (ps * 6 / 10 + 1);
                int pal = q;
                palo_desenhar(pal, sx, by + 32, ps, palo_vermelho(pal) ? 0xFF5A5A : 0xF2F2E8, 256);
            }
            char t[8];
            formatar_texto(t, sizeof t, "%c", '0' + niv[k]);
            texto_neg(bx + bw / 2 - 4, by + 58, t, sel ? 0xFFE9A0 : 0xC8DDCF);
            int w = (int)tam_texto(nom[k]) * 8;
            if (w > bw - 6) w = bw - 6;
            texto(bx + (bw - w) / 2, by + bh - 26, nom[k], sel ? 0xFFFFFF : 0xB5CCBD);
        }
        const char *ayuda = "Pulsa 1, 2 o 4 - Esc cancela";
        texto(px + (pw - (int)tam_texto(ayuda) * 8) / 2, py + ph - 26, ayuda, 0x9DB8A6);
    }
}

/* ------------------------------------------------------------------ ventana y presentación */

static bool    arrastando, a_pegar;       /* a_pegar: botón apretado sobre una carta (todavía puede ser un clic) */
static int     ar_col, ar_i, ar_dx, ar_dy, ar_x, ar_y, ar_px, ar_py;
static bool    dica_ativa;
static cand_t  dica;
static int     dica_pos;                  /* cuál de las pistas, para ir rotando */
static int     dica_estoque;
static uint32_t dica_ate;
static bool    quer_sair;
static evento_t ev_redim;
static bool    tem_redim;

static void contorno(int x, int y, int w, int h, uint32_t cor, int a)
{
    for (int k = 0; k < 3; k++) {
        int aa = a >> k;
        ret(x - k, y - k, w + 2 * k, 2, cor, aa);
        ret(x - k, y + h - 2 + k, w + 2 * k, 2, cor, aa);
        ret(x - k, y - k, 2, h + 2 * k, cor, aa);
        ret(x + w - 2 + k, y - k, 2, h + 2 * k, cor, aa);
    }
}

static void apresentar(void)
{
    if (!cena) return;
    alvo(jan->px, jan->larg, jan->alt);
    memcpy(jan->px, cena, (size_t)jan->larg * (size_t)jan->alt * 4);

    if (dica_ativa && modo == M_JOGO) {
        int pulso = 150 + (int)((tempo_ms() / 6) % 100);
        if (pulso > 250) pulso = 500 - pulso;
        if (dica_estoque) {
            int sx, sy, sw, sh;
            rect_estoque(&e, &sx, &sy, &sw, &sh);
            contorno(sx, sy, sw, sh, 0xFFE24A, pulso);
        } else {
            int x = x_col(dica.de), y = y_carta(&e, dica.de, dica.i);
            int altf = y_carta(&e, dica.de, e.n[dica.de] - 1) + L.ch - y;
            contorno(x, y, L.cw, altf, 0xFFE24A, pulso);
            int tx = x_col(dica.para);
            int ty = e.n[dica.para] ? y_carta(&e, dica.para, e.n[dica.para] - 1) : L.ytab;
            contorno(tx, ty, L.cw, L.ch, 0x7CFF9A, pulso);
        }
    }

    if (arrastando && modo == M_JOGO) {
        int dn, up;
        desloc_col(&e, ar_col, &dn, &up);
        int q = e.n[ar_col] - ar_i;
        int y = ar_y;
        for (int k = 0; k < q; k++) {
            carta_t x = e.col[ar_col][ar_i + k];
            sombra_carta(ar_x + 3, y + 3, L.ch, 70);
            blit(sprite_face(C_PALO(x), C_VALOR(x)), ar_x, y, L.ch);
            y += up;
        }
    }
    jan_mostrar(jan);
}

static void redesenhar(void)
{
    desenhar_cena();
    apresentar();
}

static void registrar_estado(const char *porque)
{
    char s[400];
    int n = 0;
    n += formatar_texto(s + n, sizeof s - (size_t)n, "spider: estado %s n=", porque);
    for (int c = 0; c < N_COL; c++) n += formatar_texto(s + n, sizeof s - (size_t)n, "%d%s", e.n[c], c < N_COL - 1 ? "," : "");
    n += formatar_texto(s + n, sizeof s - (size_t)n, " ult=");
    for (int c = 0; c < N_COL; c++) {
        carta_t x = e.n[c] ? e.col[c][e.n[c] - 1] : 0;
        n += formatar_texto(s + n, sizeof s - (size_t)n, "%s%d/%d", c ? "," : "", e.n[c] ? C_PALO(x) : -1, e.n[c] ? C_VALOR(x) : 0);
    }
    n += formatar_texto(s + n, sizeof s - (size_t)n, " y=");
    for (int c = 0; c < N_COL; c++)
        n += formatar_texto(s + n, sizeof s - (size_t)n, "%s%d", c ? "," : "", e.n[c] ? y_carta(&e, c, e.n[c] - 1) : L.ytab);
    formatar_texto(s + n, sizeof s - (size_t)n, " estoque=%d feitas=%d pontos=%d movs=%d", e.n_estoque, e.feitas, e.pontos, e.movs);
    registrar("%s", s);
}

static void registrar_layout(void)
{
    int sx, sy, sw, sh;
    rect_estoque(&e, &sx, &sy, &sw, &sh);
    registrar("spider: layout L=%d A=%d cw=%d ch=%d x0=%d gap=%d ytab=%d yfundo=%d estoque=%d,%d,%d,%d",
              L.l, L.a, L.cw, L.ch, L.x0, L.gap, L.ytab, L.yfundo, sx, sy, sw, sh);
}

static void reconstruir(void)
{
    calcular_layout(jan->larg, jan->alt);
    sprites_liberar();
    if (cena) libera(cena);
    cena = aloca((size_t)jan->larg * (size_t)jan->alt * 4);
    tapete_construir();
    registrar_layout();
}

/* ---- animaciones (bloquean unos cuadros; sólo vigilan el cierre y el cambio de tamaño) ---- */

static void vigiar_eventos(void)
{
    evento_t ev;
    while (proximo_evento(jan->id, &ev, false) == 1) {
        if (ev.tipo == EV_FECHAR) quer_sair = true;
        if (ev.tipo == EV_REDIMENSIONAR) { ev_redim = ev; tem_redim = true; }
    }
}

static int suave(int t, int total)       /* 0..256 con arranque y frenado suaves */
{
    int u = t * 256 / total;
    return u * u * (3 * 256 - 2 * u) / (256 * 256);
}

/* Desliza n cartas desde (x0, y0) hasta (x1, y1) sobre la escena actual (ya dibujada sin ellas). */
static void deslizar(const carta_t *cartas, int n, int x0, int y0, int x1, int y1, int paso, int cuadros)
{
    for (int f = 1; f <= cuadros && !quer_sair; f++) {
        int t = suave(f, cuadros);
        int x = x0 + (x1 - x0) * t / 256, y = y0 + (y1 - y0) * t / 256;
        alvo(jan->px, jan->larg, jan->alt);
        memcpy(jan->px, cena, (size_t)jan->larg * (size_t)jan->alt * 4);
        for (int k = 0; k < n; k++) {
            sombra_carta(x + 3, y + k * paso + 3, L.ch, 70);
            blit(C_CIMA(cartas[k]) ? sprite_face(C_PALO(cartas[k]), C_VALOR(cartas[k])) : sprite_dorso(), x, y + k * paso, L.ch);
        }
        jan_mostrar(jan);
        dormir(14);
        vigiar_eventos();
    }
}

static void animar_reparto(void)
{
    /* las cartas salen del dorso de más a la derecha del montón */
    int topo_x = L.x0 + 10 * L.cw + 9 * L.gap - L.cw;
    for (int k = 0; k < N_COL && !quer_sair; k++) {
        ocultar_ult_desde = k;
        desenhar_cena();
        carta_t d = FAZER(0, 1, 0);
        deslizar(&d, 1, topo_x, L.yfundo, x_col(k), y_carta(&e, k, e.n[k] - 1), 0, 5);
    }
    ocultar_ult_desde = N_COL;
}

static void animar_retirada(void)
{
    for (int r = 0; r < ret_n && !quer_sair; r++) {
        retirada_t rr = ret_lista[r];
        int destino = e.feitas - ret_n + r;           /* ranura de la escalera que se retira */
        feitas_vis = destino;
        desenhar_cena();
        int fx = x_feita(destino), fy = L.yfundo;
        int cuadros = 12 * 2 + 14;
        for (int f = 0; f <= cuadros && !quer_sair; f++) {
            alvo(jan->px, jan->larg, jan->alt);
            memcpy(jan->px, cena, (size_t)jan->larg * (size_t)jan->alt * 4);
            for (int k = 0; k < 13; k++) {                /* K sale primero; el A, último */
                int ini = k * 2;
                int t = f - ini;
                int x, y;
                if (t <= 0) { x = rr.x[k]; y = rr.y[k]; }
                else if (t >= 14) { x = fx; y = fy; }
                else { int s = suave(t, 14); x = rr.x[k] + (fx - rr.x[k]) * s / 256; y = rr.y[k] + (fy - rr.y[k]) * s / 256; }
                sombra_carta(x + 2, y + 2, L.ch, 60);
                blit(sprite_face(rr.palo, 13 - k), x, y, L.ch);
            }
            jan_mostrar(jan);
            dormir(14);
            vigiar_eventos();
        }
        feitas_vis = -1;
    }
    ret_n = 0;
}

/* ---- victoria ---- */

typedef struct { int x16, y16, vx, vy, palo, valor; bool viva; } bola_t;
#define MAX_BOLAS 24
static bola_t bolas[MAX_BOLAS];
static int     v_lanzadas, v_cuadro;

static void vitoria_iniciar(void)
{
    modo = M_VITORIA;
    v_lanzadas = 0; v_cuadro = 0;
    memset(bolas, 0, sizeof bolas);
    feitas_vis = e.feitas;
    desenhar_cena();
    alvo(jan->px, jan->larg, jan->alt);
    memcpy(jan->px, cena, (size_t)jan->larg * (size_t)jan->alt * 4);
    jan_mostrar(jan);
    registrar("spider: victoria pontos=%d movs=%d", e.pontos, e.movs);
}

static void vitoria_cuadro(void)
{
    alvo(jan->px, jan->larg, jan->alt);
    /* una carta nueva cada tres cuadros, saliendo de las pilas de abajo (K primero) */
    if (v_cuadro % 3 == 0 && v_lanzadas < 104) {
        int pila = (v_lanzadas / 13) % N_FEITAS, valor = 13 - (v_lanzadas % 13);
        for (int b = 0; b < MAX_BOLAS; b++)
            if (!bolas[b].viva) {
                bolas[b].x16 = x_feita(7 - pila) * 16;
                bolas[b].y16 = L.yfundo * 16;
                int v = (int)(aleat() % 9) + 3;
                bolas[b].vx = (aleat() & 1 ? v : -v) * 16;
                bolas[b].vy = -((int)(aleat() % 10) + 4) * 16;
                bolas[b].palo = e.feitas_palo[7 - pila];
                bolas[b].valor = valor;
                bolas[b].viva = true;
                v_lanzadas++;
                break;
            }
    }
    int ux0 = jan->larg, uy0 = jan->alt, ux1 = 0, uy1 = 0;
    for (int b = 0; b < MAX_BOLAS; b++) {
        bola_t *o = &bolas[b];
        if (!o->viva) continue;
        o->x16 += o->vx; o->y16 += o->vy; o->vy += 14;
        int chao = (jan->alt - L.ch) * 16;
        if (o->y16 > chao) { o->y16 = chao; o->vy = -o->vy * 8 / 10; if (o->vy > -40) o->vy = -40; }
        int x = o->x16 / 16, y = o->y16 / 16;
        if (x < -L.cw || x > jan->larg) { o->viva = false; continue; }
        blit(sprite_face(o->palo, o->valor), x, y, L.ch);
        if (x < ux0) ux0 = x;
        if (y < uy0) uy0 = y;
        if (x + L.cw > ux1) ux1 = x + L.cw;
        if (y + L.ch > uy1) uy1 = y + L.ch;
    }
    /* cartel */
    const char *t1 = "¡Has ganado!";
    int w = L.l - 40 < 360 ? L.l - 40 : 360, h = 76, px = (L.l - w) / 2, py = L.a / 2 - h / 2;
    rrect(px, py, w, h, 12, 0xD9B33A, 230);
    rrect(px + 2, py + 2, w - 4, h - 4, 11, 0x0E3A24, 235);
    int tw = (int)tam_texto(t1) * 16;
    for (int k = 0; (size_t)k < tam_texto(t1); k++) letra_esc(px + (w - tw) / 2 + k * 16, py + 8, t1[k], 16, 32, 0xFFE9A0);
    char s[80];
    formatar_texto(s, sizeof s, "Puntos: %d  Movimientos: %d", e.pontos, e.movs);
    texto(px + (w - (int)tam_texto(s) * 8) / 2, py + 44, s, 0xE8F0EA);
    texto(px + (w - 33 * 8) / 2, py + 58, "Pulsa una tecla para jugar otra", 0x9DB8A6);
    jan_mostrar(jan);
    v_cuadro++;
    (void)ux0; (void)uy0; (void)ux1; (void)uy1;
}

/* ------------------------------------------------------------------ acciones */

static void depois_da_acao(void)
{
    dica_ativa = false;
    registrar_estado("tras");
    if (e.feitas >= N_FEITAS) { vitoria_iniciar(); return; }
}

static uint32_t semente_nova(void)
{
    uint32_t b = 0;
    bytes_aleatorios(&b, sizeof b);
    return (b ^ tempo_ms() ^ (aleat() << 3)) | 1;
}

static void partida_nova(int niv, uint32_t sem, bool casi_ganada)
{
    feitas_vis = -1;
    nova_partida(niv, sem);
    if (casi_ganada) {
        memset(&e, 0, sizeof e);
        e.pontos = 500 - 40;
        e.movs = 40;
        e.feitas = 7;
        for (int k = 0; k < 7; k++) e.feitas_palo[k] = P_PICAS;
        for (int v = 13; v >= 2; v--) e.col[0][e.n[0]++] = FAZER(P_PICAS, v, 1);
        e.col[1][e.n[1]++] = FAZER(P_PICAS, 1, 1);
        e_inicial = e;
        desf_ini = desf_n = 0;
    }
    modo = M_JOGO;
    dica_ativa = false; arrastando = false; a_pegar = false;
    msg[0] = 0;
    registrar("spider: partida nivel=%d semilla=%u", nivel, sem);
    registrar_layout();
    registrar_estado("nueva");
}

static void mostrar_dica(void)
{
    cand_t v[MAX_CAND];
    int n = candidatos(&e, v, MAX_CAND);
    dica_estoque = 0;
    if (n == 0) {
        if (e.n_estoque) {
            bool vazia = false;
            for (int c = 0; c < N_COL; c++) if (!e.n[c]) vazia = true;
            if (vazia) definir_msg("Rellena las columnas vacías antes de repartir", 3500);
            else { dica_estoque = 1; dica_ativa = true; dica_ate = tempo_ms() + 3500; definir_msg("Reparte del montón", 3500); }
            registrar("spider: dica reparto");
        } else {
            definir_msg("No hay más movimientos", 3500);
            registrar("spider: dica nenhuma");
        }
        return;
    }
    /* de mejor a peor; cada pulsación pasa a la siguiente */
    int ordem[MAX_CAND];
    for (int k = 0; k < n; k++) ordem[k] = k;
    for (int a = 1; a < n; a++) {
        int t = ordem[a], b = a - 1;
        while (b >= 0 && v[ordem[b]].valor < v[t].valor) { ordem[b + 1] = ordem[b]; b--; }
        ordem[b + 1] = t;
    }
    if (dica_pos >= n) dica_pos = 0;
    dica = v[ordem[dica_pos++]];
    dica_ativa = true;
    dica_ate = tempo_ms() + 3500;
    registrar("spider: dica de=%d i=%d para=%d x=%d y=%d xd=%d yd=%d", dica.de, dica.i, dica.para,
              x_col(dica.de), y_carta(&e, dica.de, dica.i), x_col(dica.para),
              e.n[dica.para] ? y_carta(&e, dica.para, e.n[dica.para] - 1) : L.ytab);
}

static void fazer_reparto(void)
{
    int sx, sy, sw, sh;
    rect_estoque(&e, &sx, &sy, &sw, &sh);
    int r = executar_reparto();
    if (r == REP_COLUNA_VAZIA) {
        definir_msg("No se puede repartir con una columna vacía", 3500);
        registrar("spider: reparto negado (columna vacia)");
        redesenhar();
        return;
    }
    if (r == REP_SEM_CARTAS) {
        definir_msg("No quedan más repartos", 3000);
        registrar("spider: reparto negado (sin cartas)");
        redesenhar();
        return;
    }
    registrar("spider: reparto ok estoque=%d", e.n_estoque);
    dica_ativa = false;
    if (ret_n == 0) animar_reparto();          /* si el reparto completa una escalera, sólo se anima la retirada */
    else animar_retirada();
    depois_da_acao();
    redesenhar();
}

/* Tras mover (el estado ya cambió): anima el deslizamiento desde la posición de soltar. */
static void aterrizar(int de, int i, int para, int x0, int y0, int q)
{
    (void)de; (void)i;
    int ret_guardadas = ret_n;
    retirada_t tmp[MAX_RET];
    memcpy(tmp, ret_lista, sizeof tmp);
    carta_t pila[MAX_PILHA];
    int base = e.n[para] - q;
    if (ret_guardadas) base = -1;
    if (base >= 0) {
        memcpy(pila, &e.col[para][base], (size_t)q);
        ocultar_col = para; ocultar_de = base;
        desenhar_cena();
        int dn, up;
        desloc_col(&e, para, &dn, &up);
        deslizar(pila, q, x0, y0, x_col(para), y_carta(&e, para, base), up, 5);
        ocultar_col = -1;
    }
    ret_n = ret_guardadas;
    memcpy(ret_lista, tmp, sizeof tmp);
    if (ret_n) animar_retirada();
}

static void desfazer(void)
{
    if (!desf_voltar()) { definir_msg("No hay nada que deshacer", 2000); redesenhar(); return; }
    registrar("spider: deshacer");
    dica_ativa = false;
    registrar_estado("deshacer");
    redesenhar();
}

static void abrir_dialogo(void)
{
    modo = M_NIVEL;
    niv_dialogo = nivel;
    dica_ativa = false; arrastando = false; a_pegar = false;
    registrar("spider: dialogo de nivel");
    redesenhar();
}

static bool dentro_r(const caixa_r *r, int x, int y) { return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h; }

static void mostrar_menu(int x, int y)
{
    menu_contextual(jan->id, x, y,
                    "Nueva partida\tF2\nReiniciar partida\n-\nDeshacer\tCtrl+Z\nPista\tH\nRepartir\tD\n-\nSalir", 1);
}

static void tratar_menu(int lin)
{
    switch (lin) {
    case 0: abrir_dialogo(); break;
    case 1: e = e_inicial; desf_ini = desf_n = 0; modo = M_JOGO; registrar("spider: reiniciar"); registrar_estado("reiniciar"); redesenhar(); break;
    case 3: desfazer(); break;
    case 4: mostrar_dica(); redesenhar(); break;
    case 5: fazer_reparto(); break;
    case 7: quer_sair = true; break;
    default: break;
    }
}

/* Columna donde se suelta una pila arrastrada: la que más se solapa con la carta de arriba. */
static int coluna_do_drop(int x, int y)
{
    int melhor = -1, ma = 0;
    for (int c = 0; c < N_COL; c++) {
        int cx = x_col(c);
        int ox = (x + L.cw < cx + L.cw ? x + L.cw : cx + L.cw) - (x > cx ? x : cx);
        int fim = (c == ar_col ? L.ytab + L.ch : e.n[c] ? y_carta(&e, c, e.n[c] - 1) + L.ch : L.ytab + L.ch);
        int ini = L.ytab;
        int oy = (y + L.ch < fim ? y + L.ch : fim) - (y > ini ? y : ini);
        if (ox <= 0 || oy <= 0) continue;
        if (ox * oy > ma) { ma = ox * oy; melhor = c; }
    }
    return melhor;
}

static void soltar(int x, int y)
{
    int q = e.n[ar_col] - ar_i;
    int para = coluna_do_drop(ar_x, ar_y);
    (void)x; (void)y;
    arrastando = false;
    bool legal = para >= 0 && para != ar_col && pode_soltar(&e, para, C_VALOR(e.col[ar_col][ar_i]));
    if (legal) {
        int de = ar_col, i = ar_i, x0 = ar_x, y0 = ar_y;
        executar_jogada(de, i, para);
        registrar("spider: jogada ok de=%d i=%d para=%d", de, i, para);
        ocultar_col = -1;
        aterrizar(de, i, para, x0, y0, q);
        depois_da_acao();
        redesenhar();
    } else {
        registrar("spider: jogada invalida de=%d i=%d para=%d", ar_col, ar_i, para);
        registrar_estado("invalida");
        carta_t pila[MAX_PILHA];
        memcpy(pila, &e.col[ar_col][ar_i], (size_t)q);
        int dn, up;
        desloc_col(&e, ar_col, &dn, &up);
        ocultar_col = ar_col; ocultar_de = ar_i;
        desenhar_cena();
        deslizar(pila, q, ar_x, ar_y, x_col(ar_col), y_carta(&e, ar_col, ar_i), up, 6);
        ocultar_col = -1;
        redesenhar();
    }
}

static void clique_em(int x, int y, int botao)
{
    if (botao & 2) { mostrar_menu(x, y); return; }
    if (modo == M_VITORIA) { abrir_dialogo(); return; }
    if (modo == M_NIVEL) {
        const int niv[3] = { 1, 2, 4 };
        for (int k = 0; k < 3; k++)
            if (dentro_r(&nb_rect[k], x, y)) { partida_nova(niv[k], semente_nova(), false); redesenhar(); return; }
        return;
    }
    dica_ativa = false;
    if (dentro_r(&bt_juego, x, y)) { mostrar_menu(bt_juego.x, bt_juego.y + bt_juego.h); return; }
    if (dentro_r(&bt_nueva, x, y)) { abrir_dialogo(); return; }
    if (dentro_r(&bt_deshacer, x, y)) { desfazer(); return; }
    if (dentro_r(&bt_pista, x, y)) { mostrar_dica(); redesenhar(); return; }
    int sx, sy, sw, sh;
    rect_estoque(&e, &sx, &sy, &sw, &sh);
    if (e.n_estoque && x >= sx && x < sx + sw && y >= sy && y < sy + sh) { fazer_reparto(); return; }
    for (int c = 0; c < N_COL; c++) {
        int cx = x_col(c);
        if (x < cx || x >= cx + L.cw) continue;
        int i = carta_em(&e, c, y);
        if (i < 0 || !pode_pegar(&e, c, i)) return;
        a_pegar = true;
        ar_col = c; ar_i = i;
        ar_px = x; ar_py = y;
        ar_dx = x - cx; ar_dy = y - y_carta(&e, c, i);
        ar_x = cx; ar_y = y_carta(&e, c, i);
        return;
    }
}

static void mover_rato(int x, int y)
{
    if (!a_pegar) return;
    if (!arrastando) {
        int dx = x - ar_px, dy = y - ar_py;
        if (dx * dx + dy * dy < 25) return;
        arrastando = true;
        ocultar_col = ar_col; ocultar_de = ar_i;
        desenhar_cena();
    }
    ar_x = x - ar_dx; ar_y = y - ar_dy;
    apresentar();
}

static void soltou_rato(int x, int y)
{
    if (!a_pegar) return;
    a_pegar = false;
    if (arrastando) { soltar(x, y); return; }
    /* un clic sin arrastrar: a la mejor columna */
    int de = ar_col, i = ar_i;
    int para = melhor_destino(&e, de, i);
    if (para < 0) {
        definir_msg("Ese movimiento no es posible", 1800);
        registrar("spider: clic sin destino de=%d i=%d", de, i);
        redesenhar();
        return;
    }
    int q = e.n[de] - i;
    int x0 = x_col(de), y0 = y_carta(&e, de, i);
    executar_jogada(de, i, para);
    registrar("spider: jogada ok de=%d i=%d para=%d (clic)", de, i, para);
    aterrizar(de, i, para, x0, y0, q);
    depois_da_acao();
    redesenhar();
}

static void tratar_tecla(const evento_t *ev)
{
    unsigned t = ev->tecla;
    bool ctrl = (ev->modificadores & 2) != 0;
    if (modo == M_VITORIA) { abrir_dialogo(); return; }
    if (modo == M_NIVEL) {
        if (t == TECLA_ESC) { modo = M_JOGO; redesenhar(); return; }
        int niv = t == '1' ? 1 : t == '2' ? 2 : t == '4' ? 4 : t == '\n' ? niv_dialogo : 0;
        if (niv) { partida_nova(niv, semente_nova(), false); redesenhar(); }
        return;
    }
    if (t == 0x10B) { abrir_dialogo(); return; }                                /* F2 */
    if ((ctrl && (t == 26 || t == 'z' || t == 'Z')) || t == 'u' || t == 'U') { desfazer(); return; }
    if (t == 'h' || t == 'H' || t == 'p' || t == 'P') { mostrar_dica(); redesenhar(); return; }
    if (t == 'd' || t == 'D' || t == ' ') { fazer_reparto(); return; }
    if (t == TECLA_ESC) { dica_ativa = false; redesenhar(); }
}

int principal(int argc, char **argv)
{
    int niv = 1;
    bool auto_p = false, casi = false;
    uint32_t sem = 0;
    for (int k = 1; k < argc; k++) {
        if (compara_texto(argv[k], "--autoprueba") == 0) auto_p = true;
        else if (compara_texto(argv[k], "--casi-ganada") == 0) casi = true;
        else if (compara_texto(argv[k], "--nivel") == 0 && k + 1 < argc) niv = texto_para_int(argv[++k]);
        else if (compara_texto(argv[k], "--semilla") == 0 && k + 1 < argc) sem = (uint32_t)texto_para_int(argv[++k]);
        else { imprimir("uso: spider [--nivel 1|2|4] [--semilla N] [--autoprueba] [--casi-ganada]\n"); return 2; }
    }
    if (niv != 1 && niv != 2 && niv != 4) niv = 1;
    if (sem == 0) {
        uint32_t b = 0;
        bytes_aleatorios(&b, sizeof b);
        sem = (b ^ tempo_ms()) | 1;
    }
    desf = aloca((size_t)MAX_DESF * sizeof(estado_t));
    if (!desf) { imprimir("spider: sin memoria\n"); return 1; }
    if (auto_p) return autoprueba(sem, 0);

    gui_ativo = true;
    jan = jan_abrir("Solitario Spider", 800, 540);
    if (!jan) { registrar("spider: no se ha abierto la ventana"); return 1; }
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 640, 440);
    reconstruir();
    partida_nova(niv, sem, casi);
    redesenhar();
    registrar("spider: pronto");

    evento_t ev;
    while (!quer_sair) {
        if (tem_redim) {
            tem_redim = false;
            if (jan_redimensionar(jan, &ev_redim)) {
                reconstruir();
                registrar_estado("tamano");
                redesenhar();
            }
        }
        bool pulsa = dica_ativa && modo == M_JOGO;
        bool com_msg = msg[0] && tempo_ms() < msg_ate && modo == M_JOGO;
        int espera = modo == M_VITORIA ? 30 : pulsa ? 50 : com_msg ? (int)(msg_ate - tempo_ms()) + 5 : -1;
        int r = esperar_evento(jan->id, &ev, espera);
        if (r == 1) {
            do {
                switch (ev.tipo) {
                case EV_FECHAR: quer_sair = true; break;
                case EV_CLIQUE: if (ev.botoes & 3) clique_em(ev.x, ev.y, (int)ev.botoes); break;
                case EV_MOVER: if (modo == M_JOGO) mover_rato(ev.x, ev.y); break;
                case EV_SOLTOU: if (ev.botoes & 1) soltou_rato(ev.x, ev.y); break;
                case EV_TECLA: tratar_tecla(&ev); break;
                case EV_REDIMENSIONAR: ev_redim = ev; tem_redim = true; break;
                case EV_MENU: if (ev.tecla == 1 && (int)ev.x >= 0) tratar_menu(ev.x); break;
                default: break;
                }
            } while (!quer_sair && !tem_redim && proximo_evento(jan->id, &ev, false) == 1);
        } else if (modo == M_VITORIA) {
            vitoria_cuadro();
        } else if (pulsa || com_msg) {
            if (dica_ativa && tempo_ms() > dica_ate) dica_ativa = false;
            if (msg[0] && tempo_ms() >= msg_ate) { msg[0] = 0; redesenhar(); }
            else if (dica_ativa) apresentar();
        }
    }
    registrar("spider: cerrado");
    jan_fechar(jan);
    return 0;
}
