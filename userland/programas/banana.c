// SPDX-License-Identifier: GPL-3.0-only
/* banana - Sagui Pega Banana, el juego de SaguiOS (H27).
 *
 * El sagui se mueve abajo con las flechas; bananas caen desde arriba. Cada
 * banana atrapada vale un punto; la banana que llega al suelo quita una vida (son 3).
 * La velocidad aumenta con los puntos. Pantalla de inicio, pantalla de fin de juego y
 * récord de la sesión (y en el pendrive, si hay uno montado en /usb: fase 6).
 *
 * Solo enteros: posiciones en "subpíxeles" (x16) para que la velocidad pueda
 * crecer de a poco sin punto flotante. Para no redibujar la ventana
 * entera en cada cuadro, el fondo queda guardado en una copia, y cada objeto
 * que se mueve borra su propia estela copiando el trozo de fondo de vuelta. */
#include "sagui.h"
#include "gui.h"

#define LARG 480
#define ALT  400
#define TOPO 28                   /* barra de puntos */
#define CHAO 24
#define CHAO_Y (ALT - CHAO)
#define ESC_SAGUI 3
#define SAGUI_L (16 * ESC_SAGUI)
#define SAGUI_A (16 * ESC_SAGUI)
#define ESC_BANANA 2
#define BANANA_L (8 * ESC_BANANA)
#define BANANA_A (12 * ESC_BANANA)
#define MAX_BANANAS 6
#define QUADRO_MS 40
#define PASSO_SAGUI 16

static const char *const sprite_sagui[16] = {
    "................",
    "....EEEEEEEE....",
    ".BBEEEEBBEEEEBB.",
    "BBBEERRRRRREEBBB",
    "BBBERPRRRRPREBBB",
    ".BBERRRRRRRREBB.",
    "....ERRPPRRE....",
    ".....EEEEEE.....",
    "..M.MMMMMMMM.M..",
    "..MMMCCCCCCMMM..",
    "...MMCcccCCMM...",
    "....MCcccCCM...E",
    "....MCCCCCCM..EC",
    "....MMMMMMMM.EC.",
    "....EE.MM.EEEC..",
    "...EEE....EE....",
};

static const char *const sprite_banana[12] = {
    "......PE",
    ".....PEP",
    "....PYP.",
    "...PYYP.",
    "..PYYYP.",
    ".PYYYAP.",
    "PYYYYAP.",
    "PYYYAP..",
    "PYYAP...",
    ".PAAP...",
    "..PP....",
    "........",
};

static const char *const sprite_coracao[7] = {
    ".RR.RR.", "RRRRRRR", "RRRRRRR", ".RRRRR.", "..RRR..", "...R...", ".......",
};

static uint32_t paleta(char c)
{
    switch (c) {
    case 'P': return 0x141210; case 'E': return 0x3C2D23; case 'M': return 0x6E553C;
    case 'C': return 0x968C7D; case 'c': return 0xBEB4A5; case 'B': return 0xF5F5F0;
    case 'R': return 0xAA7864; case 'Y': return 0xFFD52E; case 'A': return 0xD9A400;
    default: return 0xFF000000;
    }
}

static uint32_t paleta_coracao(char c) { return c == 'R' ? 0xE03C3C : 0xFF000000; }

typedef struct { bool viva; int x, y16; } banana_t;   /* y en 1/16 de píxel */

static jan_t *j;
static uint32_t *fundo;
static banana_t bananas[MAX_BANANAS];
static int sagui_x, pontos, vidas, recorde;
static uint32_t semente;

static uint32_t aleatorio(void)
{
    semente = semente * 1103515245u + 12345u;
    return semente >> 8;
}

/* ------------------------------------------------------------ dibujo */

static void desenhar_fundo(void)
{
    for (int y = TOPO; y < ALT; y++) {
        uint32_t c;
        if (y >= CHAO_Y) c = y < CHAO_Y + 4 ? 0x2E9E44 : 0x7A5230;          /* pasto y tierra */
        else {
            int t = (y - TOPO) * 255 / (CHAO_Y - TOPO);                        /* cielo: azul hasta casi blanco */
            c = ((uint32_t)(120 + t * 100 / 255) << 16) | ((uint32_t)(190 + t * 50 / 255) << 8) | 245;
        }
        for (int x = 0; x < LARG; x++) fundo[y * LARG + x] = c;
    }
    /* Dos nubes simples. */
    for (int k = 0; k < 2; k++) {
        int cx = 90 + k * 260, cy = 70 + k * 30;
        for (int y = -14; y <= 14; y++)
            for (int x = -40; x <= 40; x++)
                if (x * x / 4 + y * y * 4 < 420 && cy + y >= TOPO)
                    fundo[(cy + y) * LARG + cx + x] = 0xFAFCFF;
    }
}

static void restaurar(int x, int y, int l, int a)
{
    for (int yy = y; yy < y + a; yy++) {
        if (yy < TOPO || yy >= ALT) continue;
        for (int xx = x; xx < x + l; xx++)
            if (xx >= 0 && xx < LARG) j->px[yy * LARG + xx] = fundo[yy * LARG + xx];
    }
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, TOPO, 0x18202A);
    char t[64];
    formatar_texto(t, sizeof t, "Puntos: %d", pontos);
    g_texto(j, 10, 6, t, 0xFFDF4A);
    formatar_texto(t, sizeof t, "Récord: %d", recorde);
    g_texto(j, 170, 6, t, 0xD0D4D8);
    g_texto(j, LARG - 120, 6, "Vidas:", 0xD0D4D8);
    for (int i = 0; i < vidas; i++)
        g_desenho(j, LARG - 66 + i * 20, 8, sprite_coracao, 7, 7, paleta_coracao, 2);
    jan_mostrar_area(j, 0, 0, LARG, TOPO);
}

static void desenhar_sagui(void)
{
    g_desenho(j, sagui_x, CHAO_Y - SAGUI_A, sprite_sagui, 16, 16, paleta, ESC_SAGUI);
}

static void texto_centro(int y, const char *t, uint32_t cor)
{
    g_texto(j, (LARG - g_largura_texto(t)) / 2, y, t, cor);
}

static void tela_inicio(void)
{
    memcpy(j->px + TOPO * LARG, fundo + TOPO * LARG, (size_t)(LARG * (ALT - TOPO)) * 4);
    desenhar_barra();
    g_desenho(j, LARG / 2 - 48, 90, sprite_sagui, 16, 16, paleta, 6);
    g_desenho(j, LARG / 2 + 70, 120, sprite_banana, 8, 12, paleta, 4);
    texto_centro(56, "SAGUI ATRAPA BANANAS", 0x14337E);
    texto_centro(200, "Usa las flechas para mover al sagui.", 0x1E2328);
    texto_centro(220, "¡Atrapa las bananas antes de que caigan al suelo!", 0x1E2328);
    texto_centro(260, "Pulsa Enter (o haz clic) para empezar", 0x0B7A3E);
    jan_mostrar(j);
}

static void tela_fim(void)
{
    g_retangulo(j, 60, 120, LARG - 120, 140, 0x18202A);
    g_moldura(j, 60, 120, LARG - 120, 140, 0xFFDF4A);
    texto_centro(138, "FIN DEL JUEGO", 0xFFDF4A);
    char t[64];
    formatar_texto(t, sizeof t, "Has atrapado %d banana%s.", pontos, pontos == 1 ? "" : "s");
    texto_centro(170, t, 0xFFFFFF);
    formatar_texto(t, sizeof t, "Récord: %d", recorde);
    texto_centro(192, t, 0xD0D4D8);
    texto_centro(228, "Enter para volver a jugar", 0x3CDC6E);
    jan_mostrar_area(j, 60, 120, LARG - 120, 140);
}

/* ------------------------------------------------------------ récord en el pendrive (fase 6) */

static void ler_recorde(void)
{
    int fd = abrir("/usb/sagui-recorde.txt", ABRIR_LER);
    if (fd < 0) return;
    char b[16] = { 0 };
    ler(fd, b, sizeof b - 1);
    fechar(fd);
    recorde = texto_para_int(b);
}

static void gravar_recorde(void)
{
    int fd = abrir("/usb/sagui-recorde.txt", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    char b[16];
    int n = formatar_texto(b, sizeof b, "%d\n", recorde);
    escrever(fd, b, (unsigned)n);
    fechar(fd);
    registrar("banana: recorde %d salvo no pendrive", recorde);
}

/* ------------------------------------------------------------ juego */

static int velocidade16(void) { return 32 + pontos * 4; }           /* 2 px por cuadro, más 1/4 por punto */
static int intervalo_bananas(void) { int q = 45 - pontos; return q < 14 ? 14 : q; }   /* en cuadros */

static void nova_banana(void)
{
    for (int i = 0; i < MAX_BANANAS; i++)
        if (!bananas[i].viva) {
            bananas[i].viva = true;
            bananas[i].x = 8 + (int)(aleatorio() % (LARG - 16 - BANANA_L));
            bananas[i].y16 = TOPO * 16;
            registrar("banana: nova banana em x=%d", bananas[i].x);
            return;
        }
}

static void mover_sagui(int dx)
{
    int novo = sagui_x + dx;
    if (novo < 0) novo = 0;
    if (novo > LARG - SAGUI_L) novo = LARG - SAGUI_L;
    if (novo == sagui_x) return;
    restaurar(sagui_x, CHAO_Y - SAGUI_A, SAGUI_L, SAGUI_A);
    jan_mostrar_area(j, sagui_x, CHAO_Y - SAGUI_A, SAGUI_L, SAGUI_A);
    sagui_x = novo;
    desenhar_sagui();
    jan_mostrar_area(j, sagui_x, CHAO_Y - SAGUI_A, SAGUI_L, SAGUI_A);
    registrar("banana: sagui x=%d", sagui_x);
}

/* Devuelve false cuando se cerró la ventana. */
static bool jogar(void)
{
    pontos = 0;
    vidas = 3;
    sagui_x = (LARG - SAGUI_L) / 2;
    for (int i = 0; i < MAX_BANANAS; i++) bananas[i].viva = false;
    memcpy(j->px + TOPO * LARG, fundo + TOPO * LARG, (size_t)(LARG * (ALT - TOPO)) * 4);
    desenhar_barra();
    desenhar_sagui();
    jan_mostrar(j);
    registrar("banana: inicio");
    int ate_proxima = 1;
    evento_t e;
    while (vidas > 0) {
        while (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) return false;
            if (e.tipo == EV_TECLA && e.tecla == TECLA_ESQUERDA) mover_sagui(-PASSO_SAGUI);
            if (e.tipo == EV_TECLA && e.tecla == TECLA_DIREITA) mover_sagui(PASSO_SAGUI);
        }
        if (--ate_proxima <= 0) { nova_banana(); ate_proxima = intervalo_bananas(); }
        bool barra = false;
        for (int i = 0; i < MAX_BANANAS; i++) {
            banana_t *b = &bananas[i];
            if (!b->viva) continue;
            int y_ant = b->y16 / 16;
            b->y16 += velocidade16();
            int y = b->y16 / 16;
            restaurar(b->x, y_ant, BANANA_L, BANANA_A);
            /* Atrapó: la banana tocó al sagui (contando solo la parte de arriba de él). */
            bool pegou = y + BANANA_A >= CHAO_Y - SAGUI_A + 6 && y < CHAO_Y - 8 &&
                         b->x + BANANA_L > sagui_x + 4 && b->x < sagui_x + SAGUI_L - 4;
            if (pegou || y + BANANA_A >= CHAO_Y) {
                b->viva = false;
                /* Efecto (B3, D090): agudo y corto al atrapar, grave y descendente al perder una vida. */
                if (pegou) {
                    static const nota_t pegou_n[] = { {1319, 60} };
                    pontos++;
                    registrar("banana: pegou! pontos %d", pontos);
                    efeito_tocar(pegou_n, 1);
                }
                else {
                    static const nota_t perdeu[] = { {330, 120}, {220, 200} };
                    vidas--;
                    registrar("banana: caiu no chao, vidas %d", vidas);
                    efeito_tocar(perdeu, 2);
                }
                barra = true;
                desenhar_sagui();
                jan_mostrar_area(j, b->x, y_ant, BANANA_L, y + BANANA_A - y_ant);
                continue;
            }
            g_desenho(j, b->x, y, sprite_banana, 8, 12, paleta, ESC_BANANA);
            jan_mostrar_area(j, b->x, y_ant, BANANA_L, y + BANANA_A - y_ant);
        }
        if (barra) desenhar_barra();
        dormir(QUADRO_MS);
    }
    bool novo_recorde = pontos > recorde;
    if (novo_recorde) recorde = pontos;
    registrar("banana: fim de jogo, pontos %d, recorde %d", pontos, recorde);
    desenhar_barra();
    tela_fim();
    if (novo_recorde) gravar_recorde();
    return true;
}

static bool esperar_enter(void)
{
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) return false;
        if ((e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == ' ')) || e.tipo == EV_CLIQUE) return true;
    }
}

int principal(int argc, char **argv)
{
    j = jan_abrir("Sagui Atrapa Bananas", LARG, ALT);
    fundo = aloca((size_t)(LARG * ALT) * 4);
    if (!j || !fundo) return 1;
    semente = argc > 1 ? (uint32_t)texto_para_int(argv[1]) : tempo_ms() | 1;
    desenhar_fundo();
    ler_recorde();
    vidas = 3;   /* la barra de la pantalla de inicio ya muestra los tres corazones (visto en la G470) */
    tela_inicio();
    registrar("banana: tela de inicio");
    while (esperar_enter() && jogar())
        ;
    jan_fechar(j);
    return 0;
}
