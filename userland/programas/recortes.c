// SPDX-License-Identifier: GPL-2.0-or-later
/* recortes [--recorte] [-t segundos] - recortes de pantalla (D125).
 *
 * La pantalla se congela y se oscurece (una ventana de pantalla completa con la captura);
 * se arrastra un rectángulo con el ratón, o se hace clic en una ventana (su marco entero),
 * o Intro toma la pantalla completa; Esc cancela. Al soltar sale una ventana con la vista
 * previa y los botones Guardar, Copiar, Abrir en Paint y Nuevo recorte.
 * Windows+Mayús+S lo lanza con --recorte (directo al recorte). Sin --recorte abre una
 * ventanita con el retardo (ahora, 3, 5 o 10 s) para capturar menús abiertos.
 *
 * Copiar: portapapeles de imagen del kernel (SIS_COPIAR_IMAGEM); Paint la pega con Ctrl+V. */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"

#define MAXJ 32
static imagem_t snap, escura;
static info_janela_t jans[MAXJ];
static int qtd_jans;

static bool capturar(void)
{
    int rect[4] = { 0, 0, 0, 0 };
    unsigned max = 1920u * 1200u;
    uint32_t *px = aloca(max * 4);
    if (!px) return false;
    int n = capturar_tela(0, rect, px, max);
    if (n < 0) { libera(px); return false; }
    snap.larg = rect[2]; snap.alt = rect[3]; snap.px = px;
    if (!imagem_criar(&escura, snap.larg, snap.alt, 0)) return false;
    for (int i = 0; i < n; i++) escura.px[i] = (px[i] >> 1) & 0x7F7F7F;
    qtd_jans = info_sistema(INFO_JANELAS, jans, MAXJ);
    if (qtd_jans < 0) qtd_jans = 0;
    return true;
}

/* ---- pantalla de selección */

static jan_t *ov;

static void copiar_rect(const imagem_t *de, int x, int y, int l, int a)
{
    if (x < 0) { l += x; x = 0; }
    if (y < 0) { a += y; y = 0; }
    if (x + l > ov->larg) l = ov->larg - x;
    if (y + a > ov->alt) a = ov->alt - y;
    for (int yy = y; yy < y + a; yy++) memcpy(ov->px + yy * ov->larg + x, de->px + yy * snap.larg + x, (size_t)l * 4);
}

static void norm(int x0, int y0, int x1, int y1, int *x, int *y, int *l, int *a)
{
    *x = x0 < x1 ? x0 : x1; *y = y0 < y1 ? y0 : y1;
    *l = (x0 < x1 ? x1 - x0 : x0 - x1) + 1; *a = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
}

static void desenhar_sel(int x, int y, int l, int a, int ox, int oy, int ol, int oa)
{
    int bx = ox < x ? ox : x, by = oy < y ? oy : y;
    int bx1 = ox + ol > x + l ? ox + ol : x + l, by1 = oy + oa > y + a ? oy + oa : y + a;
    bx -= 2; by -= 2; bx1 += 2; by1 += 40;
    if (ol > 0) copiar_rect(&escura, bx, by, bx1 - bx, by1 - by);
    if (l > 0) {
        copiar_rect(&snap, x, y, l, a);
        g_moldura(ov, x - 1, y - 1, l + 2, a + 2, 0xFFFFFF);
        char t[24];
        formatar_texto(t, sizeof t, "%dx%d", l, a);
        int ty = y + a + 6;
        if (ty + 16 > ov->alt) ty = y + a - 20;
        g_retangulo(ov, x, ty, g_largura_texto(t) + 8, 18, 0x18202A);
        g_texto(ov, x + 4, ty + 1, t, 0xFFFFFF);
    }
    jan_mostrar_area(ov, bx < 0 ? 0 : bx, by < 0 ? 0 : by, bx1 - bx, by1 - by);
}

/* true = hay recorte en (x, y, l, a). */
static bool seleccionar(int *rx, int *ry, int *rl, int *ra)
{
    ov = jan_abrir("Recortes", 640, 480);
    if (!ov) return false;
    jan_estilo(ov, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 100, 100);
    jan_tela_cheia(ov, true);
    bool listo = false, ok = false;
    bool arrastra = false;
    int x0 = 0, y0 = 0, sx = 0, sy = 0, sl = 0, sa = 0;
    evento_t e;
    for (int n = 0; n < 4000 && !listo; n++) {
        if (proximo_evento(ov->id, &e, true) < 0) break;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_REDIMENSIONAR) {
            if (e.tecla != JAN_ESTADO_TELA_CHEIA) break;   /* Esc: sale de la pantalla completa = cancelar */
            if (!jan_redimensionar(ov, &e)) break;
            copiar_rect(&escura, 0, 0, ov->larg, ov->alt);
            jan_mostrar(ov);
        } else if (e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == '\r')) {
            *rx = 0; *ry = 0; *rl = snap.larg; *ra = snap.alt; ok = listo = true;
        } else if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            arrastra = true; x0 = e.x; y0 = e.y;
        } else if (e.tipo == EV_MOVER && arrastra && (e.botoes & 1)) {
            int x, y, l, a;
            norm(x0, y0, e.x, e.y, &x, &y, &l, &a);
            desenhar_sel(x, y, l, a, sx, sy, sl, sa);
            sx = x; sy = y; sl = l; sa = a;
        } else if (e.tipo == EV_SOLTOU && arrastra) {
            arrastra = false;
            int x, y, l, a;
            norm(x0, y0, e.x, e.y, &x, &y, &l, &a);
            if (l >= 4 && a >= 4) { *rx = x; *ry = y; *rl = l; *ra = a; ok = listo = true; }
            else {   /* un clic suelto: la ventana de arriba que contiene el punto */
                for (int k = qtd_jans - 1; k >= 0; k--) {
                    int wx = jans[k].x - 1, wy = jans[k].y - 1, wl = jans[k].largura + 6, wa = jans[k].altura + 28;
                    if (x0 >= wx && x0 < wx + wl && y0 >= wy && y0 < wy + wa) {
                        if (wx < 0) { wl += wx; wx = 0; }
                        if (wy < 0) { wa += wy; wy = 0; }
                        if (wx + wl > snap.larg) wl = snap.larg - wx;
                        if (wy + wa > snap.alt) wa = snap.alt - wy;
                        *rx = wx; *ry = wy; *rl = wl; *ra = wa; ok = listo = true;
                        break;
                    }
                }
            }
        }
    }
    jan_fechar(ov);
    ov = NULL;
    return ok;
}

/* ---- guardar */

static void nombre_auto(char *nome, int max, const char *dir)
{
    data_t d;
    ler_data(&d);
    formatar_texto(nome, (size_t)max, "%s/recorte-%04u%02u%02u-%02u%02u%02u.png", dir, d.ano, d.mes, d.dia, d.hora, d.minuto, d.segundo);
}

static int gravar(const char *caminho, const imagem_t *im)
{
    return imagem_gravar_png(caminho, im);
}

static const char *pasta_capturas(void)
{
    info_arquivo_t inf;
    criar_pasta("/home/capturas");
    if (info_arquivo("/home/capturas", &inf) >= 0) return "/home/capturas";
    criar_pasta("/tmp/capturas");
    return "/tmp/capturas";
}

enum { A_NADA, A_NUEVO, A_SALIR };

static int previa(const imagem_t *im)
{
    int pl = im->larg, pa = im->alt;
    if (pl > 640 || pa > 400) {   /* cabe en 640x400 */
        if (pl * 400 > pa * 640) { pa = pa * 640 / pl; pl = 640; } else { pl = pl * 400 / pa; pa = 400; }
    }
    if (pl < 1) pl = 1;
    if (pa < 1) pa = 1;
    int W = pl + 24 < 480 ? 480 : pl + 24, H = pa + 24 + 48;
    jan_t *j = jan_abrir("Recortes", W, H);
    if (!j) return A_SALIR;
    jan_estilo(j, JAN_DIALOGO, W, H);
    botao_t bt[4] = {
        { 12, H - 40, 90, 28, "Guardar", false }, { 110, H - 40, 90, 28, "Copiar", false },
        { 208, H - 40, 130, 28, "Abrir en Paint", false }, { 346, H - 40, 120, 28, "Nuevo recorte", false } };
    g_retangulo(j, 0, 0, W, H, COR_FUNDO);
    imagem_desenhar(j->px, j->larg, 0, 0, W, H, im, 12, 12, pl, pa, true);
    g_moldura(j, 11, 11, pl + 2, pa + 2, COR_BORDA);
    for (int i = 0; i < 4; i++) botao_desenhar(j, &bt[i]);
    char msg[100] = "";
    jan_mostrar(j);
    int acao = A_NADA;
    evento_t e;
    while (acao == A_NADA) {
        if (proximo_evento(j->id, &e, true) < 0 || e.tipo == EV_FECHAR) { acao = A_SALIR; break; }
        int i;
        for (i = 0; i < 4; i++) if (botao_tratar(j, &bt[i], &e)) break;
        msg[0] = 0;
        if (i == 0) {
            char c[200];
            nombre_auto(c, sizeof c, pasta_capturas());
            if (pedir_texto("Guardar recorte", "Archivo (.png, .bmp):", c, sizeof c) && c[0]) {
                int n = (int)tam_texto(c);
                bool bmp = n > 4 && !compara_texto(c + n - 4, ".bmp");
                int r = bmp ? imagem_gravar_bmp(c, im) : gravar(c, im);
                formatar_texto(msg, sizeof msg, r < 0 ? "No se pudo guardar." : "Guardado en %s", c);
                if (r >= 0) aviso_tela(msg, c);
            }
        } else if (i == 1) {
            int r = copiar_imagem(im->larg, im->alt, im->px);
            copia_texto(msg, r < 0 ? "No se pudo copiar." : "Imagen copiada (Ctrl+V en Paint)", sizeof msg);
            if (r >= 0) aviso_tela(msg, NULL);
        } else if (i == 2) {
            criar_pasta("/tmp/recortes");
            char c[200];
            nombre_auto(c, sizeof c, "/tmp/recortes");
            if (gravar(c, im) >= 0) { char *av[] = { "paint", c, NULL }; executar("/bin/paint", av); }
            else copia_texto(msg, "No se pudo guardar para Paint.", sizeof msg);
        } else if (i == 3) acao = A_NUEVO;
        if (msg[0] || i < 4) {
            g_retangulo(j, 0, H - 64, W, 22, COR_FUNDO);
            g_texto(j, 12, H - 62, msg, COR_TEXTO);
            for (int k = 0; k < 4; k++) botao_desenhar(j, &bt[k]);
            jan_mostrar(j);
        }
    }
    jan_fechar(j);
    return acao;
}

/* ventanita inicial: retardo. Devuelve los segundos, o -1 para salir. */
static int lanzador(void)
{
    jan_t *j = jan_abrir("Recortes", 360, 110);
    if (!j) return -1;
    jan_estilo(j, JAN_DIALOGO, 360, 110);
    botao_t bt[4] = { { 12, 50, 76, 30, "Ahora", false }, { 96, 50, 76, 30, "3 s", false },
                      { 180, 50, 76, 30, "5 s", false }, { 264, 50, 76, 30, "10 s", false } };
    static const int seg[4] = { 0, 3, 5, 10 };
    rotulo(j, 12, 14, "Nuevo recorte (el retardo sirve para menús abiertos):", COR_TEXTO);
    for (int i = 0; i < 4; i++) botao_desenhar(j, &bt[i]);
    jan_mostrar(j);
    evento_t e;
    int r = -1;
    while (r < 0) {
        if (proximo_evento(j->id, &e, true) < 0 || e.tipo == EV_FECHAR) break;
        for (int i = 0; i < 4; i++) if (botao_tratar(j, &bt[i], &e)) r = seg[i];
    }
    jan_fechar(j);
    return r;
}

int principal(int argc, char **argv)
{
    bool directo = false;
    int espera = 0;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "--recorte")) directo = true;
        else if (!compara_texto(argv[i], "-t") && i + 1 < argc) { espera = texto_para_int(argv[++i]); directo = true; }
    }
    for (;;) {
        if (!directo) {
            int s = lanzador();
            if (s < 0) return 0;
            espera = s;
        }
        dormir((unsigned)espera * 1000 + (directo ? 0 : 400));   /* 400 ms: que se vaya la ventanita */
        directo = false;
        espera = 0;
        if (!capturar()) { imprimir("recortes: no se pudo capturar la pantalla\n"); return 1; }
        int x, y, l, a;
        if (!seleccionar(&x, &y, &l, &a)) return 0;
        imagem_t rec;
        if (!imagem_criar(&rec, l, a, 0)) return 1;
        for (int yy = 0; yy < a; yy++) memcpy(rec.px + yy * l, snap.px + (y + yy) * snap.larg + x, (size_t)l * 4);
        libera(snap.px); imagem_liberar(&escura);
        int ac = previa(&rec);
        imagem_liberar(&rec);
        if (ac != A_NUEVO) return 0;
        directo = true;   /* nuevo recorte: directo */
    }
}
