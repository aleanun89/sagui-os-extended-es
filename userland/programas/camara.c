// SPDX-License-Identifier: GPL-2.0-or-later
/* camara [-p] [--modo N] [--info] [--foto archivo [--cuadros N]] - la aplicación Cámara (D277).
 *
 * Vista previa de la cámara UVC (integrada o enchufada, driver kernel/usb/uvc.c) en una ventana:
 * botón Foto (guarda en PNG o, con cámaras MJPEG, también en JPEG, con el diálogo de guardar
 * de la D272), elección del tamaño y formato ("<" y ">"), brillo y contraste si la cámara los
 * tiene y la fuente de prueba (un patrón generado en el kernel) para probar sin cámara.
 *   -p, --prueba    usa la fuente de prueba
 *   --modo N        el modo N de la lista (--info la muestra)
 *   --info          en el terminal: cámara, modos y estado
 *   --foto archivo  sin ventana: captura unos cuadros (--cuadros N, 3 por defecto) y guarda el último
 * Sin cámara, la ventana lo dice y ofrece la fuente de prueba. La cámara no se enciende hasta que
 * esta aplicación (o la que use camara.h) la abre. */
#include "sagui.h"
#include "gui.h"
#include "camara.h"

#define LARG_INICIO 680
#define ALT_INICIO  560
#define BARRA  34
#define RODAPE 22

static jan_t *j;
static unsigned fuente = CAM_FUENTE_REAL;
static info_camara_t inf;          /* ESTADO de la fuente */
static info_camara_t cuadro_inf;   /* CUADRO: último cuadro */
static unsigned modo;
static bool aberta;
static uint8_t *buf;               /* último cuadro crudo */
static unsigned buf_cap, buf_n;
static unsigned seq_visto, quadros;
static imagem_t img;
static bool tem_imagem;
static char aviso[160];

static botao_t bt[] = {
    { 8, 5, 56, 24, N_("Foto"), false },
    { 72, 5, 28, 24, "<", false },
    { 104, 5, 28, 24, ">", false },
    { 340, 5, 64, 24, N_("Prueba"), false },
    { 412, 5, 28, 24, "B-", false },
    { 444, 5, 28, 24, "B+", false },
    { 480, 5, 28, 24, "C-", false },
    { 512, 5, 28, 24, "C+", false },
};
#define QTD_BT (int)(sizeof bt / sizeof bt[0])

static void nombre_modo(unsigned m, char *s, int max)
{
    const info_camara_modo_t *k = &inf.modos[m];
    formatar_texto(s, (size_t)max, "%ux%u %s %u fps", k->larg, k->alt, camara_nombre_formato(k->formato), k->fps);
}

/* ------------------------------------------------------------ dibujo */

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, j->larg, BARRA, COR_FUNDO);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);
    for (int i = 0; i < QTD_BT; i++) {
        if (i >= 4 && !(inf.controles & (i < 6 ? 1 : 2))) continue;
        botao_desenhar(j, &bt[i]);
    }
    char t[80] = "";
    if (inf.n_modos) nombre_modo(modo, t, sizeof t);
    g_texto(j, 140, 9, t, COR_TEXTO);
    jan_mostrar_area(j, 0, 0, j->larg, BARRA);
}

static void desenhar_rodape(void)
{
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (!inf.presente) copia_texto(t, _("Sin cámara. Pulsa Prueba para usar la fuente de prueba"), sizeof t);
    else formatar_texto(t, sizeof t, _("%s | %u cuadros | %u descartados"), inf.nome, quadros, inf.descartados);
    g_texto_n(j, 8, y + 3, t, (j->larg - 16) / LETRA_L, COR_TEXTO);
    jan_mostrar_area(j, 0, y, j->larg, RODAPE);
}

static void desenhar_imagem(void)
{
    int ay = BARRA, al = j->larg, aa = j->alt - BARRA - RODAPE;
    if (aa < 1) aa = 1;
    g_retangulo(j, 0, ay, al, aa, 0x14181C);
    if (tem_imagem) {
        int l = al, a = (int)((unsigned)img.alt * (unsigned)al / (unsigned)img.larg);
        if (a > aa) { a = aa; l = (int)((unsigned)img.larg * (unsigned)aa / (unsigned)img.alt); }
        if (l < 1) l = 1;
        if (a < 1) a = 1;
        imagem_desenhar(j->px, j->larg, 0, ay, al, aa, &img, (al - l) / 2, ay + (aa - a) / 2, l, a, false);
    } else if (!inf.presente)
        g_texto(j, 24, ay + 24, _("No se detecta ninguna cámara USB (UVC)."), 0xE0E0E0);
    else if (!aberta)
        g_texto(j, 24, ay + 24, _("La cámara no está abierta."), 0xE0E0E0);
    jan_mostrar_area(j, 0, ay, al, aa);
}

static void desenhar_tudo(void)
{
    desenhar_barra();
    desenhar_imagem();
    desenhar_rodape();
}

/* ------------------------------------------------------------ cámara */

static void fechar_cam(void)
{
    if (aberta) camara_cerrar();
    aberta = false;
}

static unsigned modo_inicial(void)
{
    unsigned mejor = 0;
    int pt = -1;
    for (unsigned m = 0; m < inf.n_modos; m++) {
        int p = 0;
        if (inf.modos[m].larg == 640 && inf.modos[m].alt == 480) p = 2;
        if (p && inf.modos[m].formato == CAM_FMT_MJPEG) p = 3;
        if (p > pt) { pt = p; mejor = m; }
    }
    return mejor;
}

static bool abrir_cam(void)
{
    fechar_cam();
    if (buf) { libera(buf); buf = NULL; }
    tem_imagem = false;
    aviso[0] = 0;
    camara_estado(fuente, &inf);
    if (!inf.presente || !inf.n_modos) return false;
    if (modo >= inf.n_modos) modo = modo_inicial();
    info_camara_t a;
    int r = camara_abrir(fuente, modo, &a);
    if (r < 0) {
        formatar_texto(aviso, sizeof aviso, _("No se pudo abrir la cámara (error %d)"), r);
        registrar("camara: abrir fallo %d\n", r);
        return false;
    }
    buf_cap = (unsigned)inf.modos[modo].larg * inf.modos[modo].alt * 2u + 4096u;
    buf = aloca(buf_cap);
    if (!buf) { fechar_cam(); copia_texto(aviso, _("Sin memoria"), sizeof aviso); return false; }
    aberta = true;
    seq_visto = 0;
    quadros = 0;
    registrar("camara: abierta %ux%u %s\n", a.larg, a.alt, camara_nombre_formato(a.formato));
    return true;
}

/* Pide el último cuadro; true si llegó uno nuevo (y lo deja convertido en img). */
static bool novo_cuadro(void)
{
    if (!aberta) return false;
    memcpy(&cuadro_inf, &inf, sizeof cuadro_inf);
    int n = camara_cuadro(&cuadro_inf, buf, buf_cap, seq_visto);
    if (n <= 0) return false;
    seq_visto = cuadro_inf.seq;
    buf_n = (unsigned)n;
    quadros++;
    imagem_t nova;
    if (camara_a_imagem(buf, buf_n, cuadro_inf.larg, cuadro_inf.alt, cuadro_inf.formato, &nova) < 0) return false;
    if (tem_imagem) imagem_liberar(&img);
    img = nova;
    tem_imagem = true;
    return true;
}

static void tirar_foto(void)
{
    if (!aberta || !buf_n || !tem_imagem) { copia_texto(aviso, _("No hay ningún cuadro que guardar"), sizeof aviso); return; }
    unsigned n = buf_n, l = cuadro_inf.larg, a = cuadro_inf.alt, f = cuadro_inf.formato;
    uint8_t *copia = aloca(n);                 /* el diálogo es modal: la vista previa sigue en pausa */
    if (!copia) return;
    memcpy(copia, buf, n);
    data_t d;
    ler_data(&d);
    char ini[120], ruta[200];
    criar_pasta("/home/fotos");
    formatar_texto(ini, sizeof ini, "/home/fotos/foto-%04u%02u%02u-%02u%02u%02u.png", d.ano, d.mes, d.dia, d.hora, d.minuto, d.segundo);
    if (dialogo_guardar(_("Guardar foto"), f == CAM_FMT_MJPEG ? _("PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg") : _("PNG (*.png)|*.png"), ini, ruta, sizeof ruta)) {
        int r = camara_guardar(ruta, copia, n, l, a, f);
        if (r == IMG_ERRO_NAO_SUPORTA) {       /* JPEG pedido con una cámara YUY2: se guarda como PNG */
            size_t m = tam_texto(ruta);
            while (m && ruta[m - 1] != '.') m--;
            if (m) { ruta[m] = 0; junta_texto(ruta, "png", sizeof ruta); r = camara_guardar(ruta, copia, n, l, a, f); }
        }
        if (r < 0) formatar_texto(aviso, sizeof aviso, _("No se pudo guardar la foto (%d)"), r);
        else { formatar_texto(aviso, sizeof aviso, "Foto guardada en %s", ruta); registrar("camara: foto guardada %s\n", ruta); }
    }
    libera(copia);
}

static void ajustar(unsigned ctl, int sentido)
{
    info_camara_t a;
    if (camara_ajuste(ctl, false, 0, &a) < 0) return;
    int paso = (a.maximo - a.minimo) / 10;
    if (paso < 1) paso = 1;
    int v = a.valor + sentido * paso;
    if (camara_ajuste(ctl, true, v, &a) >= 0)
        formatar_texto(aviso, sizeof aviso, _("%s: %d (de %d a %d)"), ctl == CAM_CTL_BRILLO ? _("Brillo") : _("Contraste"), a.valor, a.minimo, a.maximo);
}

static void botao(int i)
{
    switch (i) {
    case 0: tirar_foto(); break;
    case 1: case 2:
        if (inf.n_modos > 1) {
            modo = i == 1 ? (modo + inf.n_modos - 1) % inf.n_modos : (modo + 1) % inf.n_modos;
            abrir_cam();
        }
        break;
    case 3:
        fuente = fuente == CAM_FUENTE_PRUEBA ? CAM_FUENTE_REAL : CAM_FUENTE_PRUEBA;
        modo = 1000;
        abrir_cam();
        break;
    case 4: ajustar(CAM_CTL_BRILLO, -1); break;
    case 5: ajustar(CAM_CTL_BRILLO, 1); break;
    case 6: ajustar(CAM_CTL_CONTRASTE, -1); break;
    case 7: ajustar(CAM_CTL_CONTRASTE, 1); break;
    }
    desenhar_tudo();
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) { if (jan_redimensionar(j, e)) desenhar_tudo(); return true; }
    if (e->tipo == EV_TECLA) {
        if (e->tecla == ' ' || e->tecla == 'f' || e->tecla == 'F') botao(0);
        else if (e->tecla == 'p' || e->tecla == 'P') botao(3);
        return true;
    }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU)
        for (int i = 0; i < QTD_BT; i++) {
            if (i >= 4 && !(inf.controles & (i < 6 ? 1 : 2))) continue;
            if (botao_tratar(j, &bt[i], e)) { aviso[0] = 0; botao(i); return true; }
        }
    return true;
}

/* ------------------------------------------------------------ terminal */

static void imprimir_info(void)
{
    camara_estado(fuente, &inf);
    imprimir("fuente %s: %s (%s)\n", fuente == CAM_FUENTE_PRUEBA ? "prueba" : "real", inf.presente ? "presente" : "no hay cámara",
             inf.nome);
    for (unsigned m = 0; m < inf.n_modos; m++) {
        char t[64];
        nombre_modo(m, t, sizeof t);
        imprimir("  modo %u: %s\n", m, t);
    }
}

static int foto_sin_ventana(const char *ruta, int cuadros)
{
    if (!abrir_cam()) { imprimir("camara: %s\n", aviso[0] ? aviso : "sin cámara"); return 1; }
    int tomados = 0;
    for (int i = 0; i < 600 && tomados < cuadros; i++) {
        if (novo_cuadro()) tomados++;
        else dormir(10);
    }
    if (tomados < cuadros) { imprimir("camara: solo llegaron %d cuadros\n", tomados); fechar_cam(); return 1; }
    int r = camara_guardar(ruta, buf, buf_n, cuadro_inf.larg, cuadro_inf.alt, cuadro_inf.formato);
    camara_estado(fuente, &inf);
    imprimir("camara: %ux%u %s, %d cuadros, %u descartados, foto %s (%d)\n", cuadro_inf.larg, cuadro_inf.alt,
             camara_nombre_formato(cuadro_inf.formato), tomados, inf.descartados, r == 0 ? "guardada" : "NO guardada", r);
    fechar_cam();
    return r == 0 ? 0 : 1;
}

int principal(int argc, char **argv)
{
    bool info = false;
    const char *foto = NULL;
    int cuadros = 3;
    modo = 1000;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-p") || !compara_texto(argv[i], "--prueba")) fuente = CAM_FUENTE_PRUEBA;
        else if (!compara_texto(argv[i], "--info")) info = true;
        else if (!compara_texto(argv[i], "--modo") && i + 1 < argc) modo = (unsigned)texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "--foto") && i + 1 < argc) foto = argv[++i];
        else if (!compara_texto(argv[i], "--cuadros") && i + 1 < argc) cuadros = texto_para_int(argv[++i]);
        else { imprimir("uso: camara [-p] [--modo N] [--info] [--foto archivo [--cuadros N]]\n"); return 1; }
    }
    if (info) { imprimir_info(); return 0; }
    if (foto) return foto_sin_ventana(foto, cuadros);

    j = jan_abrir(_("Cámara"), LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 420, 300);
    abrir_cam();
    desenhar_tudo();
    registrar("camara: lista (%s)\n", inf.presente ? inf.nome : "sin camara");

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (esperar_evento(j->id, &e, aberta ? 8 : 100) == 1) {
            continuar = tratar(&e);
            while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        }
        if (continuar && novo_cuadro()) {
            desenhar_imagem();
            if ((quadros & 7) == 1) desenhar_rodape();
        }
    }
    registrar("camara: cerrada\n");
    fechar_cam();
    if (tem_imagem) imagem_liberar(&img);
    if (buf) libera(buf);
    jan_fechar(j);
    return 0;
}
