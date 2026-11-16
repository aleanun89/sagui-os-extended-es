// SPDX-License-Identifier: GPL-2.0-or-later
/* visor [archivo | carpeta] - el Visor de imágenes de SaguiOS (D055).
 *     visor -i archivo [x y]...  - en la terminal: formato, tamaño, una suma
 *                                  de los píxeles y el color de los píxeles pedidos.
 *
 * Lee BMP, PPM/PGM/PBM y PNG con la biblioteca de imágenes de la libsagui
 * (imagem.h, D054). La imagen se dibuja ajustada a la ventana o con un zoom
 * fijo (1:1, +, -, rueda); cuando es más grande que la ventana, se arrastra con el
 * mouse o con las flechas. Las flechas a los lados (cuando la imagen entra),
 * AvPág/RePág, Inicio/Fin y los botones < > recorren las imágenes de la misma
 * carpeta, en orden alfabético.
 *
 * Debajo del 100% la imagen se reduce con el filtro bilineal (sin serrucho
 * en las líneas finas); arriba, con el vecino más cercano (los píxeles quedan
 * cuadrados, como se espera al ampliar). Mientras se arrastra, siempre el
 * vecino más cercano, que es más rápido.
 *
 * La ventana es redimensionable y maximizable; en pantalla completa (F11, D044) la
 * barra de botones y la de estado desaparecen y el fondo queda negro.
 *
 * Sin archivo, el área de la imagen muestra la lista de las imágenes de /imagenes (o
 * de la carpeta actual): doble clic o Enter abre. */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"

#define LARG_INICIO 720
#define ALT_INICIO  520
#define LARG_MIN 360
#define ALT_MIN  200
#define BARRA  34
#define RODAPE 22
#define MAX_ARQS 256
#define COR_AREA 0x2B3036
#define ZOOM_1 1000          /* zoom en milésimas: 1000 = 100% */
#define LADO_ESCALADO_MAX 32768

static jan_t *j;
static imagem_t img;
static bool tem_imagem;
static char caminho[200];
static char pasta[200] = "/imagenes";
static char arqs[MAX_ARQS][64];
static int qtd_arqs, atual = -1;

static bool ajustar = true;   /* ajustar a la ventana (el zoom acompaña el tamaño) */
static int zoom = ZOOM_1;
static int ox, oy;            /* esquina de la imagen escalada, relativa al área */
static bool arrastando;
static int arr_x, arr_y;
static bool tela_cheia;
static bool mover_pendente;   /* arrastró: redibuja después de juntar los eventos */
static char aviso[120];

static lista_t seletor;       /* sin imagen: las imágenes de la carpeta */

static botao_t bt[] = {
    { 8, 5, 64, 24, N_("Abrir"), false },
    { 80, 5, 28, 24, "<", false },
    { 112, 5, 28, 24, ">", false },
    { 148, 5, 72, 24, N_("Ajustar"), false },
    { 226, 5, 44, 24, "1:1", false },
    { 278, 5, 28, 24, "-", false },
    { 310, 5, 28, 24, "+", false },
};
#define QTD_BT (int)(sizeof bt / sizeof bt[0])

/* ------------------------------------------------------------ geometría */

static int area_x(void) { return 0; }
static int area_y(void) { return tela_cheia ? 0 : BARRA; }
static int area_l(void) { return j->larg; }
static int area_a(void) { int a = j->alt - (tela_cheia ? 0 : BARRA + RODAPE); return a > 1 ? a : 1; }

static int escalado_l(void) { int v = (int)((unsigned)img.larg * (unsigned)zoom / ZOOM_1); return v > 0 ? v : 1; }
static int escalado_a(void) { int v = (int)((unsigned)img.alt * (unsigned)zoom / ZOOM_1); return v > 0 ? v : 1; }

static int zoom_maximo(void)
{
    int lado = img.larg > img.alt ? img.larg : img.alt;
    int m = LADO_ESCALADO_MAX / lado * ZOOM_1;   /* la cuenta de zoom alrededor del cursor cabe en 32 bits */
    return m < 16 * ZOOM_1 ? (m > ZOOM_1 ? m : ZOOM_1) : 16 * ZOOM_1;
}

/* El zoom que hace que la imagen entre entera en el área (puede ampliar). */
static int zoom_ajustado(void)
{
    int zl = (int)((unsigned)area_l() * ZOOM_1 / (unsigned)img.larg);
    int za = (int)((unsigned)area_a() * ZOOM_1 / (unsigned)img.alt);
    int z = zl < za ? zl : za;
    int m = zoom_maximo();
    if (z > m) z = m;
    return z < 10 ? 10 : z;
}

/* Centra en el eje en que la imagen es menor que el área; en el otro, no
 * deja espacio vacío en el borde. */
static void limitar(void)
{
    int sl = escalado_l(), sa = escalado_a(), al = area_l(), aa = area_a();
    if (sl <= al) ox = (al - sl) / 2;
    else { if (ox > 0) ox = 0; if (ox < al - sl) ox = al - sl; }
    if (sa <= aa) oy = (aa - sa) / 2;
    else { if (oy > 0) oy = 0; if (oy < aa - sa) oy = aa - sa; }
}

/* Cambia el zoom manteniendo quieto el punto de la imagen que está en (cx, cy) del área. */
static void mudar_zoom(int novo, int cx, int cy)
{
    int m = zoom_maximo();
    if (novo > m) novo = m;
    if (novo < 10) novo = 10;
    /* |cx - ox| <= LADO_ESCALADO_MAX + área y nuevo <= 16000: el producto cabe
     * en 32 bits (y no necesita la división de 64 bits de libgcc). */
    int dx = cx - ox, dy = cy - oy;
    ox = cx - dx * novo / zoom;
    oy = cy - dy * novo / zoom;
    zoom = novo;
    ajustar = false;
    limitar();
    registrar("visor: zoom %d%% en (%d,%d), imagen en (%d,%d)\n", zoom / 10, cx, cy, ox, oy);
}

static const int degraus[] = { 50, 100, 125, 250, 333, 500, 667, 1000, 1500, 2000, 3000, 4000, 6000, 8000, 12000, 16000 };
#define QTD_DEGRAUS (int)(sizeof degraus / sizeof degraus[0])

static int degrau_acima(int z)
{
    for (int i = 0; i < QTD_DEGRAUS; i++) if (degraus[i] > z + 5) return degraus[i];
    return degraus[QTD_DEGRAUS - 1];
}

static int degrau_abaixo(int z)
{
    for (int i = QTD_DEGRAUS - 1; i >= 0; i--) if (degraus[i] < z - 5) return degraus[i];
    return degraus[0];
}

/* ------------------------------------------------------------ dibujo */

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static void desenhar_barra(void)
{
    if (tela_cheia) return;
    g_retangulo(j, 0, 0, j->larg, BARRA, COR_FUNDO);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);
    for (int i = 0; i < QTD_BT; i++) botao_desenhar(j, &bt[i]);
    if (j->larg > 520) g_texto(j, 352, 9, _("Rueda: zoom  F11: pantalla completa"), COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, 0, j->larg, BARRA);
}

static void desenhar_rodape(void)
{
    if (tela_cheia) return;
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (!tem_imagem) formatar_texto(t, sizeof t, _("Ninguna imagen abierta (%s)"), pasta);
    else if (qtd_arqs > 0 && atual >= 0)
        formatar_texto(t, sizeof t, _("%s  |  %d x %d  |  %s  |  %d%%%s  |  %d de %d"), nome_curto(caminho), img.larg, img.alt,
                       img.formato, zoom / 10, ajustar ? _(" (ajustada)") : "", atual + 1, qtd_arqs);
    else
        formatar_texto(t, sizeof t, "%s  |  %d x %d  |  %s  |  %d%%%s", nome_curto(caminho), img.larg, img.alt,
                       img.formato, zoom / 10, ajustar ? _(" (ajustada)") : "");
    g_texto_n(j, 8, y + 3, t, (j->larg - 16) / LETRA_L, COR_TEXTO);
    jan_mostrar_area(j, 0, y, j->larg, RODAPE);
}

static void desenhar_seletor(void)
{
    int ax = area_x(), ay = area_y(), al = area_l(), aa = area_a();
    g_retangulo(j, ax, ay, al, aa, COR_FUNDO);
    g_texto(j, ax + 16, ay + 10, _("Ninguna imagen abierta. Elige una (doble clic o Intro), pulsa Abrir"), COR_TEXTO);
    g_texto(j, ax + 16, ay + 28, _("(Ctrl+O) o usa en el terminal: visor archivo.bmp"), COR_TEXTO);
    seletor.x = ax + 16;
    seletor.y = ay + 52;
    seletor.l = al - 32 < 200 ? 200 : al - 32;
    seletor.a = aa - 64 < 44 ? 44 : aa - 64;
    if (qtd_arqs == 0) {
        char t[240];
        formatar_texto(t, sizeof t, _("No hay imágenes en %s"), pasta);
        g_texto(j, seletor.x, seletor.y + 4, t, COR_TEXTO_FRACO);
    } else lista_desenhar(j, &seletor);
    jan_mostrar_area(j, ax, ay, al, aa);
}

static void desenhar_imagem(void)
{
    if (!tem_imagem) { desenhar_seletor(); return; }
    int ax = area_x(), ay = area_y(), al = area_l(), aa = area_a();
    if (ajustar) zoom = zoom_ajustado();
    limitar();
    g_retangulo(j, ax, ay, al, aa, tela_cheia ? 0 : COR_AREA);
    bool suave = zoom < ZOOM_1 && !arrastando;
    imagem_desenhar(j->px, j->larg, ax, ay, al, aa, &img, ax + ox, ay + oy, escalado_l(), escalado_a(), suave);
    jan_mostrar_area(j, ax, ay, al, aa);
}

static void atualizar_titulo(void)
{
    char t[120];
    if (tem_imagem) formatar_texto(t, sizeof t, _("%s - Visor de imágenes"), nome_curto(caminho));
    else copia_texto(t, _("Visor de imágenes"), sizeof t);
    titulo_janela(j->id, t);
}

static void desenhar_tudo(void)
{
    if (tela_cheia) g_retangulo(j, 0, 0, j->larg, j->alt, 0);
    desenhar_barra();
    desenhar_imagem();
    desenhar_rodape();
}

/* ------------------------------------------------------------ carpeta y archivos */

/* Lista las imágenes de la carpeta, en orden alfabético (inserción: son pocas). */
static void listar_pasta(const char *p)
{
    copia_texto(pasta, p, sizeof pasta);
    qtd_arqs = 0;
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) > 0 && qtd_arqs < MAX_ARQS; i++) {
        if (e.tipo == TIPO_PASTA || !imagem_extensao(e.nome)) continue;
        int k = qtd_arqs++;
        while (k > 0 && compara_texto(arqs[k - 1], e.nome) > 0) { copia_texto(arqs[k], arqs[k - 1], 64); k--; }
        copia_texto(arqs[k], e.nome, 64);
    }
    seletor.itens = arqs;
    seletor.qtd = qtd_arqs;
    seletor.sel = qtd_arqs ? 0 : -1;
    seletor.topo = 0;
}

static void caminho_em_pasta(char *saida, size_t max, const char *nome)
{
    copia_texto(saida, pasta, max);
    if (compara_texto(pasta, "/") != 0) junta_texto(saida, "/", max);
    junta_texto(saida, nome, max);
}

static bool abrir_imagem(const char *c, bool relistar)
{
    imagem_t nova;
    int r = imagem_ler(c, &nova);
    if (r < 0) {
        formatar_texto(aviso, sizeof aviso, _("No se ha podido abrir %s: %s"), nome_curto(c), imagem_erro(r));
        registrar("visor: erro ao abrir %s (%d)\n", c, r);
        return false;
    }
    if (tem_imagem) imagem_liberar(&img);
    img = nova;
    tem_imagem = true;
    copia_texto(caminho, c, sizeof caminho);
    aviso[0] = 0;
    ajustar = true;
    if (relistar) {
        char p[200];
        copia_texto(p, c, sizeof p);
        char *b = acha_ultimo_char(p, '/');
        if (!b) pasta_atual(p, sizeof p);
        else if (b == p) p[1] = 0;
        else *b = 0;
        listar_pasta(p);
    }
    atual = -1;
    for (int i = 0; i < qtd_arqs; i++)
        if (compara_texto(arqs[i], nome_curto(c)) == 0) atual = i;
    registrar("visor: aberto %s %s %dx%d (%d de %d)\n", caminho, img.formato, img.larg, img.alt, atual + 1, qtd_arqs);
    atualizar_titulo();
    return true;
}

/* Abre un archivo, o la primera imagen de una carpeta. */
static bool abrir_caminho(const char *c)
{
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) >= 0 && inf.tipo == TIPO_PASTA) {
        listar_pasta(c);
        if (qtd_arqs == 0) {
            formatar_texto(aviso, sizeof aviso, _("No hay imágenes en %s"), c);
            if (tem_imagem) { imagem_liberar(&img); tem_imagem = false; }
            atualizar_titulo();
            return false;
        }
        char a[200];
        caminho_em_pasta(a, sizeof a, arqs[0]);
        return abrir_imagem(a, false);
    }
    return abrir_imagem(c, true);
}

static void outra_imagem(int passo, bool absoluto)
{
    if (qtd_arqs == 0) return;
    int i = absoluto ? passo : atual + passo;
    if (!absoluto && atual < 0) i = 0;
    if (i < 0) i = qtd_arqs - 1;       /* da la vuelta, como suelen hacer los visores */
    if (i >= qtd_arqs) i = 0;
    if (i == atual) return;
    char c[200];
    caminho_em_pasta(c, sizeof c, arqs[i]);
    abrir_imagem(c, false);
    desenhar_tudo();
}

static void pedir_arquivo(void)
{
    char c[200];
    if (tem_imagem) copia_texto(c, caminho, sizeof c);
    else { copia_texto(c, pasta, sizeof c); junta_texto(c, "/", sizeof c); }
    if (dialogo_abrir(_("Abrir imagen"), _("Imágenes|*.bmp;*.ppm;*.pgm;*.png;*.jpg;*.jpeg;*.gif|Todos los archivos (*.*)|*"), c, c, sizeof c) && c[0]) {
        abrir_caminho(c);
        desenhar_tudo();
    }
}

/* ------------------------------------------------------------ eventos */

static void rolar(int dx, int dy)
{
    ox += dx;
    oy += dy;
    limitar();
    registrar("visor: desplazada a (%d,%d)\n", ox, oy);
    desenhar_imagem();
}

static bool cabe_na_largura(void) { return escalado_l() <= area_l(); }
static bool cabe_na_altura(void) { return escalado_a() <= area_a(); }

static void tecla(unsigned t)
{
    if (t == 15) { pedir_arquivo(); return; }   /* Ctrl+O */
    if (!tem_imagem) {
        evento_t e = { .tipo = EV_TECLA, .tecla = t };
        if (lista_tratar(j, &seletor, &e) == LISTA_ATIVOU) {
            char c[200];
            caminho_em_pasta(c, sizeof c, arqs[seletor.sel]);
            abrir_imagem(c, false);
            desenhar_tudo();
        }
        return;
    }
    int cx = area_l() / 2, cy = area_a() / 2;
    switch (t) {
    case '+': case '=': mudar_zoom(degrau_acima(zoom), cx, cy); break;
    case '-': mudar_zoom(degrau_abaixo(zoom), cx, cy); break;
    case '1': mudar_zoom(ZOOM_1, cx, cy); break;
    case '0': case 'a': case 'A': ajustar = true; registrar("visor: ajustar\n"); break;
    case TECLA_PGDN: case ' ': outra_imagem(1, false); return;
    case TECLA_PGUP: case '\b': outra_imagem(-1, false); return;
    case TECLA_HOME: outra_imagem(0, true); return;
    case TECLA_END: outra_imagem(qtd_arqs - 1, true); return;
    case TECLA_ESQUERDA: if (cabe_na_largura()) { outra_imagem(-1, false); return; } rolar(48, 0); break;
    case TECLA_DIREITA: if (cabe_na_largura()) { outra_imagem(1, false); return; } rolar(-48, 0); break;
    case TECLA_CIMA: if (!cabe_na_altura()) rolar(0, 48); return;
    case TECLA_BAIXO: if (!cabe_na_altura()) rolar(0, -48); return;
    default: return;
    }
    desenhar_imagem();
    desenhar_rodape();
}

static void botao(int i)
{
    int cx = area_l() / 2, cy = area_a() / 2;
    switch (i) {
    case 0: pedir_arquivo(); return;
    case 1: outra_imagem(-1, false); return;
    case 2: outra_imagem(1, false); return;
    case 3: if (tem_imagem) { ajustar = true; registrar("visor: ajustar\n"); } break;
    case 4: if (tem_imagem) mudar_zoom(ZOOM_1, cx, cy); break;
    case 5: if (tem_imagem) mudar_zoom(degrau_abaixo(zoom), cx, cy); break;
    case 6: if (tem_imagem) mudar_zoom(degrau_acima(zoom), cx, cy); break;
    }
    desenhar_imagem();
    desenhar_rodape();
}

/* Devuelve false para cerrar. */
static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            tela_cheia = cheia;
            registrar("visor: ventana %dx%d%s\n", j->larg, j->alt, tela_cheia ? " (pantalla completa)" : "");
            desenhar_tudo();
            if (tem_imagem)
                registrar("visor: imagen a %d%% en (%d,%d) %dx%d\n", zoom / 10, ox, oy, escalado_l(), escalado_a());
        }
        return true;
    }
    if (e->tipo == EV_TECLA) { aviso[0] = 0; tecla(e->tecla); return true; }
    if (!tela_cheia && (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU))
        for (int i = 0; i < QTD_BT; i++)
            if (botao_tratar(j, &bt[i], e)) { aviso[0] = 0; botao(i); return true; }
    int mx = e->x - area_x(), my = e->y - area_y();
    bool na_area = mx >= 0 && my >= 0 && mx < area_l() && my < area_a();
    if (!tem_imagem) {
        if (lista_tratar(j, &seletor, e) == LISTA_ATIVOU) {
            char c[200];
            caminho_em_pasta(c, sizeof c, arqs[seletor.sel]);
            abrir_imagem(c, false);
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_RODA && na_area) {
        int n = (int)e->cliques, z = zoom;
        for (; n < 0; n++) z = degrau_acima(z);   /* rueda hacia arriba: acerca */
        for (; n > 0; n--) z = degrau_abaixo(z);
        mudar_zoom(z, mx, my);
        desenhar_imagem();
        desenhar_rodape();
        return true;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && na_area) {
        if (e->cliques >= 2) {   /* doble clic: alterna entre ajustada y 1:1 */
            if (ajustar) mudar_zoom(ZOOM_1, mx, my); else ajustar = true;
            desenhar_imagem();
            desenhar_rodape();
            return true;
        }
        arrastando = true;
        arr_x = e->x; arr_y = e->y;
        return true;
    }
    if (e->tipo == EV_MOVER && arrastando && (e->botoes & 1)) {
        int dx = e->x - arr_x, dy = e->y - arr_y;
        arr_x = e->x; arr_y = e->y;
        if (dx || dy) { ox += dx; oy += dy; limitar(); mover_pendente = true; }
        return true;   /* dibuja una vez después de juntar los movimientos */
    }
    if (e->tipo == EV_SOLTOU && arrastando) {
        arrastando = false;
        registrar("visor: arrastada a (%d,%d)\n", ox, oy);
        desenhar_imagem();
    }
    return true;
}

/* ------------------------------------------------------------ modo -i (terminal) */

static int informar(int argc, char **argv)
{
    if (argc < 3) { escrever_texto(ERROS, "uso: visor -i archivo [x y]...\n"); return 1; }
    imagem_t im;
    int r = imagem_ler(argv[2], &im);
    if (r < 0) { imprimir_em(ERROS, "visor: %s: %s\n", argv[2], imagem_erro(r)); return 1; }
    /* FNV-1a de los bytes R, G, B de todos los píxeles: la prueba compara con la cuenta en el host. */
    uint32_t h = 2166136261u;
    for (int i = 0; i < im.larg * im.alt; i++)
        for (int k = 16; k >= 0; k -= 8) { h ^= (im.px[i] >> k) & 255; h *= 16777619u; }
    imprimir("%s: %s, %d x %d, suma %08x\n", argv[2], im.formato, im.larg, im.alt, h);
    for (int i = 3; i + 1 < argc; i += 2) {
        int x = texto_para_int(argv[i]), y = texto_para_int(argv[i + 1]);
        if (x < 0 || y < 0 || x >= im.larg || y >= im.alt) imprimir("pixel %d,%d: fuera\n", x, y);
        else imprimir("pixel %d,%d: %06x\n", x, y, im.px[y * im.larg + x]);
    }
    imagem_liberar(&im);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-i") == 0) return informar(argc, argv);
    j = jan_abrir(_("Visor de imágenes"), LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    seletor.icone = NULL;
    if (argc > 1) abrir_caminho(argv[1]);
    else {
        info_arquivo_t inf;
        if (info_arquivo(pasta, &inf) < 0) pasta_atual(pasta, sizeof pasta);
        listar_pasta(pasta);
    }
    atualizar_titulo();
    desenhar_tudo();
    registrar("visor: pronto (%s)\n", tem_imagem ? caminho : "sin imagen");

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        continuar = tratar(&e);
        while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        if (continuar && mover_pendente) { mover_pendente = false; desenhar_imagem(); }
    }
    registrar("visor: fechado\n");
    if (tem_imagem) imagem_liberar(&img);
    jan_fechar(j);
    return 0;
}
