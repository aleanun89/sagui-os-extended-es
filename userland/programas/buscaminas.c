// SPDX-License-Identifier: GPL-2.0-or-later
/* buscaminas - el Buscaminas de SaguiOS (D248).
 *
 * Juego propio: los colores, la bandera, la mina, el sagüí de la cara y los dígitos del
 * contador se dibujan aquí mismo, en código (ni gráficos ni sonidos copiados de otro juego).
 *
 * Niveles: Principiante 9x9/10, Intermedio 16x16/40, Experto 30x16/99 y Personalizado.
 * Ratón: clic izquierdo descubre; clic derecho pone bandera (y, si está activa, la marca «?»);
 * un clic sobre un número con todas sus banderas descubre las vecinas (acorde), también con el
 * botón central o con izquierdo+derecho. Teclado: flechas, espacio/Intro descubre, F o M
 * bandera, F2 partida nueva, 1/2/3 nivel (4 personalizado), R récords, Q alterna la marca «?».
 * La primera casilla nunca es mina: las minas se colocan después del primer clic y, si caben,
 * dejan libre también su entorno (así el primer clic abre una zona).
 *
 * Opciones: --nivel 1|2|3, --tam COLS,FILAS,MINAS (personalizado), --semilla N (partida
 * repetible, para las pruebas). Los mejores tiempos van en /home/.config/buscaminas.conf
 * (o /tmp/buscaminas.conf si no hay /home). Todo es entero. */
#include "sagui.h"
#include "gui.h"

extern const unsigned char fonte_sagui[256][16];

#define CEL      24             /* lado de una casilla */
#define MAX_C    36
#define MAX_F    20
#define MAX_N    (MAX_C * MAX_F)
#define LARG_MIN 500
#define FAIXA    24             /* franja de niveles arriba */
#define CAB_Y    28
#define CAB_A    48
#define TAB_Y    (CAB_Y + CAB_A + 8)
#define MARG     12

#define F_MINA   1
#define F_ABERTA 2
#define F_BAND   4
#define F_DUVIDA 8
#define F_ERRO   16             /* bandera mal puesta (al perder) */
#define F_BOOM   32             /* la mina que reventó */

enum { E_ESPERA, E_JOGO, E_PERDIDA, E_GANADA };
enum { M_JOGO, M_RECORDS };

static const char *const nomes_nivel[4] = { N_("Principiante"), N_("Intermedio"), N_("Experto"), N_("Personal") };
static const char *const chaves[3] = { "principiante", "intermedio", "experto" };

static jan_t *jan;
static int nivel = 1;                 /* 1..4 */
static int cols = 9, filas = 9, minas = 10;
static int c_cols = 20, c_filas = 12, c_minas = 40;   /* el personalizado */
static uint8_t cel[MAX_N];
static int fila_q[MAX_N];
static int estado = E_ESPERA, modo = M_JOGO;
static int abertas, banderas;
static unsigned t_ini, t_fim;
static uint32_t semente, rng;
static bool usa_duvida = true;
static int cur_x = -1, cur_y = -1;    /* cursor del teclado */
static int recorde[3];                /* segundos; 0 = ninguno */
static int ox, ancho_v, alto_v;
static bool sair_ja, boton_esq;
static int ult_tempo = -1;
static char msg[80];

/* ------------------------------------------------------------ azar */

static uint32_t aleatorio(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

/* ------------------------------------------------------------ récords */

static const char *ruta_conf(bool escribir)
{
    static char r[40];
    if (escribir) {
        criar_pasta("/home/.config");
        int fd = abrir("/home/.config/buscaminas.conf", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd >= 0) { fechar(fd); return "/home/.config/buscaminas.conf"; }
        return "/tmp/buscaminas.conf";
    }
    info_arquivo_t inf;
    if (info_arquivo("/home/.config/buscaminas.conf", &inf) == 0) return "/home/.config/buscaminas.conf";
    (void)r;
    return "/tmp/buscaminas.conf";
}

static void conf_ler(void)
{
    char buf[256];
    int fd = abrir(ruta_conf(false), ABRIR_LER);
    if (fd < 0) return;
    int n = ler(fd, buf, sizeof buf - 1);
    fechar(fd);
    if (n <= 0) return;
    buf[n] = 0;
    for (char *p = buf; *p;) {
        char *fin = p;
        while (*fin && *fin != '\n') fin++;
        char sv = *fin; *fin = 0;
        char *ig = p;
        while (*ig && *ig != '=') ig++;
        if (*ig == '=') {
            *ig = 0;
            int v = texto_para_int(ig + 1);
            for (int k = 0; k < 3; k++) if (compara_texto(p, chaves[k]) == 0) recorde[k] = v > 0 ? v : 0;
            if (compara_texto(p, "marcas") == 0) usa_duvida = v != 0;
        }
        *fin = sv;
        p = sv ? fin + 1 : fin;
    }
}

static void conf_escrever(void)
{
    char buf[200];
    int n = formatar_texto(buf, sizeof buf, "principiante=%d\nintermedio=%d\nexperto=%d\nmarcas=%d\n",
                           recorde[0], recorde[1], recorde[2], usa_duvida ? 1 : 0);
    int fd = abrir(ruta_conf(true), ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever(fd, buf, (unsigned)n);
    fechar(fd);
}

/* ------------------------------------------------------------ reglas */

#define IDX(x, y) ((y) * cols + (x))

static int vecinas_mina(int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy;
            if ((dx || dy) && a >= 0 && a < cols && b >= 0 && b < filas && (cel[IDX(a, b)] & F_MINA)) n++;
        }
    return n;
}

static int vecinas_band(int x, int y)
{
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy;
            if ((dx || dy) && a >= 0 && a < cols && b >= 0 && b < filas && (cel[IDX(a, b)] & F_BAND)) n++;
        }
    return n;
}

static void nueva_partida(void)
{
    for (int i = 0; i < cols * filas; i++) cel[i] = 0;
    estado = E_ESPERA; abertas = 0; banderas = 0; t_ini = t_fim = 0; ult_tempo = -1;
    msg[0] = 0; modo = M_JOGO;
    rng = semente ? semente : 0x9E3779B9u;
    for (int i = 0; i < 8; i++) aleatorio();
}

/* Pone las minas tras el primer clic: ni la casilla ni (si caben) su entorno. */
static void colocar_minas(int px, int py)
{
    int total = cols * filas;
    bool entorno = total - 9 >= minas;
    int cand[MAX_N], nc = 0;
    for (int y = 0; y < filas; y++)
        for (int x = 0; x < cols; x++) {
            int dx = x - px, dy = y - py;
            bool excl = entorno ? (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) : (dx == 0 && dy == 0);
            if (!excl) cand[nc++] = IDX(x, y);
        }
    for (int k = 0; k < minas && nc > 0; k++) {
        int r = (int)(aleatorio() % (uint32_t)nc);
        cel[cand[r]] |= F_MINA;
        cand[r] = cand[--nc];
    }
}

static void log_estado(const char *que)
{
    static const char *const en[4] = { "espera", "jugando", "perdida", "ganada" };
    registrar("buscaminas: estado %s %s niv=%d %dx%d minas=%d rest=%d abiertas=%d banderas=%d t=%d",
              que, en[estado], nivel, cols, filas, minas, minas - banderas, abertas, banderas,
              (int)((t_fim ? t_fim : tempo_ms()) - t_ini) / 1000);
    for (int y = 0; y < filas; y++) {
        char f[MAX_C + 1];
        for (int x = 0; x < cols; x++) {
            uint8_t c = cel[IDX(x, y)];
            char ch = '#';
            if (c & F_ABERTA) { int n = vecinas_mina(x, y); ch = n ? (char)('0' + n) : '.'; }
            else if (c & F_BAND) ch = 'F';
            else if (c & F_DUVIDA) ch = '?';
            f[x] = ch;
        }
        f[cols] = 0;
        registrar("buscaminas: f%d %s", y, f);
    }
}

static void log_minas(void)
{
    for (int y = 0; y < filas; y++) {
        char f[MAX_C + 1];
        for (int x = 0; x < cols; x++) f[x] = (cel[IDX(x, y)] & F_MINA) ? '*' : '.';
        f[cols] = 0;
        registrar("buscaminas: m%d %s", y, f);
    }
}

static int recorde_idx(void) { return nivel >= 1 && nivel <= 3 ? nivel - 1 : -1; }

static void ganar(void)
{
    estado = E_GANADA;
    t_fim = tempo_ms();
    for (int i = 0; i < cols * filas; i++)
        if ((cel[i] & F_MINA) && !(cel[i] & F_BAND)) { cel[i] |= F_BAND; cel[i] &= (uint8_t)~F_DUVIDA; }
    banderas = minas;
    int seg = (int)(t_fim - t_ini) / 1000;
    if (seg < 1) seg = 1;
    int k = recorde_idx();
    msg[0] = 0;
    if (k >= 0) {
        if (recorde[k] == 0 || seg < recorde[k]) {
            recorde[k] = seg;
            conf_escrever();
            formatar_texto(msg, sizeof msg, _("¡Nuevo récord de %s: %d s!"), _(nomes_nivel[k]), seg);
            registrar("buscaminas: record %s %d", chaves[k], seg);
        } else formatar_texto(msg, sizeof msg, _("¡Ganaste en %d s!"), seg);
    } else formatar_texto(msg, sizeof msg, _("¡Ganaste en %d s!"), seg);
}

static void perder(int bx, int by)
{
    estado = E_PERDIDA;
    t_fim = tempo_ms();
    for (int i = 0; i < cols * filas; i++) {
        if ((cel[i] & F_MINA) && !(cel[i] & F_BAND)) cel[i] |= F_ABERTA;
        if (!(cel[i] & F_MINA) && (cel[i] & F_BAND)) cel[i] |= F_ERRO;
    }
    cel[IDX(bx, by)] |= F_BOOM;
    formatar_texto(msg, sizeof msg, _("¡Boom! F2 para otra partida"));
}

/* Descubre desde (x,y) con una cola (sin recursión). */
static void descobrir(int x, int y)
{
    if (x < 0 || x >= cols || y < 0 || y >= filas) return;
    uint8_t *c = &cel[IDX(x, y)];
    if (*c & (F_ABERTA | F_BAND)) return;
    if (estado == E_ESPERA) { colocar_minas(x, y); estado = E_JOGO; t_ini = tempo_ms(); log_minas(); }
    if (estado != E_JOGO) return;
    *c &= (uint8_t)~F_DUVIDA;
    if (*c & F_MINA) { perder(x, y); return; }
    int ini = 0, fim = 0;
    fila_q[fim++] = IDX(x, y);
    *c |= F_ABERTA; abertas++;
    while (ini < fim) {
        int i = fila_q[ini++], cx = i % cols, cy = i / cols;
        if (vecinas_mina(cx, cy)) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int a = cx + dx, b = cy + dy;
                if (a < 0 || a >= cols || b < 0 || b >= filas) continue;
                uint8_t *v = &cel[IDX(a, b)];
                if (*v & (F_ABERTA | F_BAND)) continue;
                *v &= (uint8_t)~F_DUVIDA;
                *v |= F_ABERTA; abertas++;
                fila_q[fim++] = IDX(a, b);
            }
    }
    if (abertas == cols * filas - minas) ganar();
}

static void acorde(int x, int y)
{
    if (estado != E_JOGO) return;
    uint8_t c = cel[IDX(x, y)];
    int n = vecinas_mina(x, y);
    if (!(c & F_ABERTA) || n == 0 || vecinas_band(x, y) != n) return;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy;
            if ((dx || dy) && a >= 0 && a < cols && b >= 0 && b < filas) descobrir(a, b);
            if (estado != E_JOGO) return;
        }
}

static void marcar(int x, int y)
{
    if (estado != E_JOGO && estado != E_ESPERA) return;
    uint8_t *c = &cel[IDX(x, y)];
    if (*c & F_ABERTA) return;
    if (*c & F_BAND) { *c &= (uint8_t)~F_BAND; banderas--; if (usa_duvida) *c |= F_DUVIDA; }
    else if (*c & F_DUVIDA) *c &= (uint8_t)~F_DUVIDA;
    else { *c |= F_BAND; banderas++; }
}

/* ------------------------------------------------------------ dibujo */

static const char *const sp_banda[12] = {
    "......OO....",
    "......OOOO..",
    "......OOOOOO",
    "......OOOOO.",
    "......OOO...",
    "......K.....",
    "......K.....",
    "......K.....",
    ".....KK.....",
    "....KKKK....",
    "...KKKKKK...",
    "............",
};
static uint32_t pal_banda(char c) { return c == 'O' ? 0xE8651A : c == 'K' ? 0x2A2420 : 0xFF000000; }

static const char *const sp_mina[14] = {
    ".......K......",
    "...K...K...K..",
    "....K..K..K...",
    ".....KKKKK....",
    "..KKKKKKKKKK..",
    ".KKKKKKRRKKKK.",
    "KKKKKKRrRKKKKK",
    ".KKKKKKRRKKKK.",
    "..KKKKKKKKKK..",
    ".....KKKKK....",
    "....K..K..K...",
    "...K...K...K..",
    ".......K......",
    "..............",
};
static uint32_t pal_mina(char c) { return c == 'K' ? 0x23201E : c == 'R' ? 0xD03A2A : c == 'r' ? 0xFFB0A0 : 0xFF000000; }

/* La cara: un sagüí. P pelaje, C cara, O ojos/boca, W blanco, R rojo, G gafas. */
static const char *const sp_cara[4][16] = {
    { /* normal */
    "................",
    "...PPP....PPP...",
    "..PCCCP..PCCCP..",
    "..PCCPPPPPPCCP..",
    "...PPCCCCCCPP...",
    "...PCCOCCOCCP...",
    "...PCCOCCOCCP...",
    "..PPCCCCCCCCPP..",
    "..PCCCCOOCCCCP..",
    "..PCCCCCCCCCCP..",
    "...PCCOOOOCCP...",
    "...PPCCCCCCPP...",
    "....PPPPPPPP....",
    "................",
    "................",
    "................" },
    { /* asombrado (mientras se aprieta) */
    "................",
    "...PPP....PPP...",
    "..PCCCP..PCCCP..",
    "..PCCPPPPPPCCP..",
    "...PPCCCCCCPP...",
    "...PCWOCCWOCP...",
    "...PCWOCCWOCP...",
    "..PPCCCCCCCCPP..",
    "..PCCCCCCCCCCP..",
    "..PCCCCOOCCCCP..",
    "...PCCCOOCCCP...",
    "...PPCCCCCCPP...",
    "....PPPPPPPP....",
    "................",
    "................",
    "................" },
    { /* ganó: gafas y sonrisa */
    "................",
    "...PPP....PPP...",
    "..PCCCP..PCCCP..",
    "..PCCPPPPPPCCP..",
    "...PPCCCCCCPP...",
    "...PGGGGGGGGP...",
    "...PGGGPPGGGP...",
    "..PPCGGCCGGCPP..",
    "..PCCCCCCCCCCP..",
    "..PCOCCCCCCOCP..",
    "...PCOOOOOOCP...",
    "...PPCCCCCCPP...",
    "....PPPPPPPP....",
    "................",
    "................",
    "................" },
    { /* perdió: ojos en cruz y boca triste */
    "................",
    "...PPP....PPP...",
    "..PCCCP..PCCCP..",
    "..PCCPPPPPPCCP..",
    "...PPCCCCCCPP...",
    "...PCORCCROCP...",
    "...PCCOCCOCCP...",
    "..PPCORCCROCPP..",
    "..PCCCCCCCCCCP..",
    "..PCCCOOOOCCCP..",
    "...PCCOCCOCCP...",
    "...PPCCCCCCPP...",
    "....PPPPPPPP....",
    "................",
    "................",
    "................" },
};
static uint32_t pal_cara(char c)
{
    switch (c) {
    case 'P': return 0x5A3E26; case 'C': return 0xF2D2A0; case 'O': return 0x2A1C12;
    case 'W': return 0xFFFFFF; case 'R': return 0xD03A2A; case 'G': return 0x1C2A44;
    default: return 0xFF000000;
    }
}

static const uint32_t cor_num[9] = { 0, 0x1F5FD0, 0x1C8A3C, 0xD02E2E, 0x232E8C, 0x8C2A1C, 0x1C8C8C, 0x2A2420, 0x6A737C };

/* Un glifo de la fuente del sistema en negrita (dibujado dos veces, 1 px de desfase). */
static void glifo2(int x, int y, int ch, uint32_t cor)
{
    char t[2] = { (char)ch, 0 };
    g_texto(jan, x, y, t, cor);
    g_texto(jan, x + 1, y, t, cor);
}

static void tab_geom(int *tx, int *ty) { *tx = ox; *ty = TAB_Y; }

static void desenhar_casilla(int x, int y)
{
    int tx, ty;
    tab_geom(&tx, &ty);
    int px = tx + x * CEL, py = ty + y * CEL;
    uint8_t c = cel[IDX(x, y)];
    bool aberta = (c & F_ABERTA) != 0;
    if (!aberta) {
        g_retangulo(jan, px, py, CEL, CEL, 0x2E8B57);
        g_retangulo(jan, px, py, CEL, 2, 0x7FD6A0);
        g_retangulo(jan, px, py, 2, CEL, 0x7FD6A0);
        g_retangulo(jan, px, py + CEL - 2, CEL, 2, 0x155C36);
        g_retangulo(jan, px + CEL - 2, py, 2, CEL, 0x155C36);
        if (c & F_BAND) g_desenho(jan, px + 6, py + 6, sp_banda, 12, 12, pal_banda, 1);
        else if (c & F_DUVIDA) glifo2(px + 7, py + 4, '?', 0xFFF3C0);
    } else {
        uint32_t fundo = (c & F_BOOM) ? 0xE05040 : 0xEFE6CC;
        g_retangulo(jan, px, py, CEL, CEL, fundo);
        g_retangulo(jan, px, py, CEL, 1, 0xC9BC98);
        g_retangulo(jan, px, py, 1, CEL, 0xC9BC98);
        if (c & F_MINA) g_desenho(jan, px + 5, py + 5, sp_mina, 14, 14, pal_mina, 1);
        else {
            int n = vecinas_mina(x, y);
            if (n) glifo2(px + 7, py + 4, '0' + n, cor_num[n]);
        }
    }
    if (c & F_ERRO) {                         /* bandera mal puesta: aspa roja */
        g_linha(jan, px + 3, py + 3, px + CEL - 4, py + CEL - 4, 0xD02020);
        g_linha(jan, px + 4, py + 3, px + CEL - 3, py + CEL - 4, 0xD02020);
        g_linha(jan, px + CEL - 4, py + 3, px + 3, py + CEL - 4, 0xD02020);
        g_linha(jan, px + CEL - 3, py + 3, px + 4, py + CEL - 4, 0xD02020);
    }
    if (x == cur_x && y == cur_y) g_moldura(jan, px + 1, py + 1, CEL - 2, CEL - 2, 0x1F5FD0);
}

/* Dígito de siete segmentos, hecho con rectángulos (14x24). */
static void digito7(int x, int y, int d)
{
    static const uint8_t seg[10] = { 0x77, 0x24, 0x5D, 0x6D, 0x2E, 0x6B, 0x7B, 0x25, 0x7F, 0x6F };
    uint8_t m = d < 0 ? 0x08 : seg[d];            /* d < 0: sólo el guion */
    uint32_t on = 0xFF5030, off = 0x3A1A14;
    g_retangulo(jan, x, y, 14, 24, 0x140A08);
    g_retangulo(jan, x + 2, y + 1, 10, 2, (m & 1) ? on : off);      /* arriba */
    g_retangulo(jan, x + 1, y + 3, 2, 8, (m & 2) ? on : off);       /* arriba izq */
    g_retangulo(jan, x + 11, y + 3, 2, 8, (m & 4) ? on : off);      /* arriba der */
    g_retangulo(jan, x + 2, y + 11, 10, 2, (m & 8) ? on : off);     /* medio */
    g_retangulo(jan, x + 1, y + 13, 2, 8, (m & 16) ? on : off);     /* abajo izq */
    g_retangulo(jan, x + 11, y + 13, 2, 8, (m & 32) ? on : off);    /* abajo der */
    g_retangulo(jan, x + 2, y + 21, 10, 2, (m & 64) ? on : off);    /* abajo */
}

static void contador7(int x, int y, int v)
{
    if (v < -99) v = -99;
    if (v > 999) v = 999;
    if (v < 0) { digito7(x, y, -1); digito7(x + 16, y, (-v) / 10); digito7(x + 32, y, (-v) % 10); return; }
    digito7(x, y, v / 100); digito7(x + 16, y, (v / 10) % 10); digito7(x + 32, y, v % 10);
}

static int segundos(void)
{
    if (estado == E_ESPERA) return 0;
    unsigned fin = estado == E_JOGO ? tempo_ms() : t_fim;
    int s = (int)(fin - t_ini) / 1000;
    return s > 999 ? 999 : s;
}

static void cara_rect(int *x, int *y) { *x = ancho_v / 2 - 20; *y = CAB_Y + 4; }

static void desenhar_cabecera(void)
{
    g_retangulo(jan, 0, CAB_Y, ancho_v, CAB_A, 0xD9D4C4);
    g_retangulo(jan, 0, CAB_Y, ancho_v, 2, 0xFFFFFF);
    contador7(MARG + 4, CAB_Y + 12, minas - banderas);
    contador7(ancho_v - MARG - 4 - 46, CAB_Y + 12, segundos());
    int fx, fy;
    cara_rect(&fx, &fy);
    g_retangulo(jan, fx, fy, 40, 40, 0xC8C2AE);
    g_retangulo(jan, fx, fy, 40, 2, 0xFFFFFF);
    g_retangulo(jan, fx, fy, 2, 40, 0xFFFFFF);
    g_retangulo(jan, fx, fy + 38, 40, 2, 0x7A7462);
    g_retangulo(jan, fx + 38, fy, 2, 40, 0x7A7462);
    int k = estado == E_GANADA ? 2 : estado == E_PERDIDA ? 3 : boton_esq ? 1 : 0;
    g_desenho(jan, fx + 4, fy + 4, sp_cara[k], 16, 16, pal_cara, 2);
}

/* Las posiciones (x0,ancho) de los rótulos de la franja de niveles. */
static const char *const rot_faixa[5] = { N_("Principiante"), N_("Intermedio"), N_("Experto"), N_("Personal"), N_("Récords") };
static void faixa_pos(int k, int *x, int *l)
{
    int xx = 8;
    for (int i = 0; i < k; i++) xx += g_largura_texto(_(rot_faixa[i])) + 22;
    *x = xx; *l = g_largura_texto(_(rot_faixa[k])) + 12;
}

static void desenhar_faixa(void)
{
    g_retangulo(jan, 0, 0, ancho_v, FAIXA, 0x155C36);
    for (int k = 0; k < 5; k++) {
        int x, l;
        faixa_pos(k, &x, &l);
        bool sel = k == nivel - 1 && k < 4;
        if (sel) g_retangulo(jan, x - 4, 2, l, FAIXA - 4, 0x2E8B57);
        g_texto(jan, x + 2, 4, _(rot_faixa[k]), sel ? 0xFFFFFF : 0xCFEBD9);
    }
}

static void desenhar_records(void)
{
    int tx, ty;
    tab_geom(&tx, &ty);
    g_retangulo(jan, 0, TAB_Y - 4, ancho_v, alto_v - TAB_Y + 4, 0xEFE6CC);
    g_texto(jan, 24, TAB_Y + 8, _("Mejores tiempos"), 0x155C36);
    for (int k = 0; k < 3; k++) {
        char t[64];
        if (recorde[k]) formatar_texto(t, sizeof t, "%-14s %d s", _(nomes_nivel[k]), recorde[k]);
        else formatar_texto(t, sizeof t, "%-14s --", _(nomes_nivel[k]));
        g_texto(jan, 24, TAB_Y + 36 + k * 22, t, COR_TEXTO);
    }
    g_texto(jan, 24, TAB_Y + 36 + 3 * 22 + 8, _("Clic o tecla para volver"), COR_TEXTO_FRACO);
}

static void redesenhar(void)
{
    g_retangulo(jan, 0, 0, ancho_v, alto_v, 0xD9D4C4);
    desenhar_faixa();
    desenhar_cabecera();
    if (modo == M_RECORDS) desenhar_records();
    else {
        for (int y = 0; y < filas; y++)
            for (int x = 0; x < cols; x++) desenhar_casilla(x, y);
        g_texto(jan, MARG, alto_v - 20, msg[0] ? msg : _("Clic der: bandera. F2: nueva. 1-3: nivel."), COR_TEXTO);
    }
    jan_mostrar(jan);
}

static void cabecera_viva(void)
{
    int s = segundos();
    if (s == ult_tempo) return;
    ult_tempo = s;
    desenhar_cabecera();
    jan_mostrar_area(jan, 0, CAB_Y, ancho_v, CAB_A);
}

/* ------------------------------------------------------------ ventana */

static void abrir_ventana(void)
{
    ancho_v = cols * CEL + 2 * MARG;
    if (ancho_v < LARG_MIN) ancho_v = LARG_MIN;
    alto_v = TAB_Y + filas * CEL + 28;
    ox = (ancho_v - cols * CEL) / 2;
    if (jan) jan_fechar(jan);
    jan = jan_abrir("Buscaminas", ancho_v, alto_v);
    if (!jan) { registrar("buscaminas: no se ha abierto la ventana"); sair_ja = true; return; }
    registrar("buscaminas: layout L=%d A=%d ox=%d oy=%d cel=%d cols=%d filas=%d", ancho_v, alto_v, ox, TAB_Y, CEL, cols, filas);
}

static void elegir_nivel(int n)
{
    nivel = n;
    if (n == 1) { cols = 9; filas = 9; minas = 10; }
    else if (n == 2) { cols = 16; filas = 16; minas = 40; }
    else if (n == 3) { cols = 30; filas = 16; minas = 99; }
    else { cols = c_cols; filas = c_filas; minas = c_minas; }
    cur_x = cur_y = -1;
    nueva_partida();
}

static bool ajustar_tam(int c, int f, int m)
{
    if (c < 5) c = 5;
    if (c > MAX_C) c = MAX_C;
    if (f < 5) f = 5;
    if (f > MAX_F) f = MAX_F;
    if (m < 1) m = 1;
    if (m > c * f - 9) m = c * f - 9;
    c_cols = c; c_filas = f; c_minas = m;
    return true;
}

static void pedir_personal(void)
{
    char r[40];
    formatar_texto(r, sizeof r, "%d,%d,%d", c_cols, c_filas, c_minas);
    if (!pedir_texto(_("Personalizado"), _("Columnas,filas,minas (ej. 20,12,40):"), r, sizeof r)) return;
    int v[3] = { c_cols, c_filas, c_minas }, k = 0;
    const char *p = r;
    while (*p && k < 3) {
        while (*p == ' ') p++;
        v[k++] = texto_para_int(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    ajustar_tam(v[0], v[1], v[2]);
    elegir_nivel(4);
    abrir_ventana();
}

static void cambiar_nivel(int n)
{
    if (n == 4) { pedir_personal(); redesenhar(); log_estado("nivel"); return; }
    elegir_nivel(n);
    abrir_ventana();
    redesenhar();
    log_estado("nivel");
}

static void partida_nueva_tecla(void)
{
    uint32_t b = 0;
    bytes_aleatorios(&b, sizeof b);
    semente = (b ^ tempo_ms()) | 1;
    nueva_partida();
    redesenhar();
    log_estado("nueva");
}

/* ------------------------------------------------------------ entrada */

static bool en_casilla(int mx, int my, int *x, int *y)
{
    int tx, ty;
    tab_geom(&tx, &ty);
    if (mx < tx || my < ty) return false;
    *x = (mx - tx) / CEL; *y = (my - ty) / CEL;
    return *x < cols && *y < filas;
}

static void tratar_clique(int mx, int my, unsigned botoes)
{
    if (modo == M_RECORDS) { modo = M_JOGO; redesenhar(); return; }
    if (my < FAIXA) {
        for (int k = 0; k < 5; k++) {
            int x, l;
            faixa_pos(k, &x, &l);
            if (mx >= x - 4 && mx < x - 4 + l) {
                if (k == 4) { modo = M_RECORDS; redesenhar(); } else cambiar_nivel(k + 1);
                return;
            }
        }
        return;
    }
    int fx, fy;
    cara_rect(&fx, &fy);
    if (mx >= fx && mx < fx + 40 && my >= fy && my < fy + 40) { nueva_partida(); redesenhar(); log_estado("nueva"); return; }
    int x, y;
    if (!en_casilla(mx, my, &x, &y)) return;
    cur_x = x; cur_y = y;
    unsigned c = cel[IDX(x, y)];
    if (((botoes & 3) == 3) || (botoes & 4)) acorde(x, y);
    else if (botoes & 1) {
        boton_esq = true;
        if (c & F_ABERTA) acorde(x, y); else descobrir(x, y);
    } else if (botoes & 2) marcar(x, y);
    redesenhar();
    log_estado("clic");
}

static void tratar_tecla(unsigned t)
{
    if (modo == M_RECORDS) { modo = M_JOGO; redesenhar(); return; }
    if (cur_x < 0) { cur_x = cols / 2; cur_y = filas / 2; }
    if (t == TECLA_ESQUERDA && cur_x > 0) cur_x--;
    else if (t == TECLA_DIREITA && cur_x < cols - 1) cur_x++;
    else if (t == TECLA_CIMA && cur_y > 0) cur_y--;
    else if (t == TECLA_BAIXO && cur_y < filas - 1) cur_y++;
    else if (t == ' ' || t == '\n') {
        if (cel[IDX(cur_x, cur_y)] & F_ABERTA) acorde(cur_x, cur_y); else descobrir(cur_x, cur_y);
    }
    else if (t == 'f' || t == 'F' || t == 'm' || t == 'M') marcar(cur_x, cur_y);
    else if (t == 0x10B) { partida_nueva_tecla(); return; }
    else if (t >= '1' && t <= '4') { cambiar_nivel((int)(t - '0')); return; }
    else if (t == 'r' || t == 'R') { modo = M_RECORDS; }
    else if (t == 'q' || t == 'Q') { usa_duvida = !usa_duvida; conf_escrever(); }
    else return;
    redesenhar();
    log_estado("tecla");
}

int principal(int argc, char **argv)
{
    int niv = 1;
    uint32_t sem = 0;
    bool tam = false;
    for (int k = 1; k < argc; k++) {
        if (compara_texto(argv[k], "--nivel") == 0 && k + 1 < argc) niv = texto_para_int(argv[++k]);
        else if (compara_texto(argv[k], "--semilla") == 0 && k + 1 < argc) sem = (uint32_t)texto_para_int(argv[++k]);
        else if (compara_texto(argv[k], "--tam") == 0 && k + 1 < argc) {
            const char *p = argv[++k];
            int v[3] = { 9, 9, 10 };
            for (int i = 0; i < 3 && *p; i++) {
                v[i] = texto_para_int(p);
                while (*p && *p != ',') p++;
                if (*p == ',') p++;
            }
            ajustar_tam(v[0], v[1], v[2]);
            tam = true;
        } else { imprimir("uso: buscaminas [--nivel 1|2|3|4] [--tam COLS,FILAS,MINAS] [--semilla N]\n"); return 2; }
    }
    if (tam) niv = 4;
    if (niv < 1 || niv > 4) niv = 1;
    if (sem == 0) {
        uint32_t b = 0;
        bytes_aleatorios(&b, sizeof b);
        sem = (b ^ tempo_ms()) | 1;
    }
    semente = sem;
    conf_ler();
    elegir_nivel(niv);
    abrir_ventana();
    if (sair_ja) return 1;
    redesenhar();
    registrar("buscaminas: pronto");
    log_estado("inicio");

    evento_t ev;
    while (!sair_ja) {
        int r = esperar_evento(jan->id, &ev, estado == E_JOGO ? 250 : -1);
        if (r == 1) {
            do {
                switch (ev.tipo) {
                case EV_FECHAR: sair_ja = true; break;
                case EV_CLIQUE: tratar_clique(ev.x, ev.y, ev.botoes); break;
                case EV_SOLTOU: if (boton_esq) { boton_esq = false; redesenhar(); } break;
                case EV_TECLA: tratar_tecla(ev.tecla); break;
                default: break;
                }
            } while (!sair_ja && proximo_evento(jan->id, &ev, false) == 1);
        } else if (estado == E_JOGO) cabecera_viva();
    }
    registrar("buscaminas: cerrado");
    jan_fechar(jan);
    return 0;
}
