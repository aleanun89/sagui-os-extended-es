// SPDX-License-Identifier: GPL-2.0-or-later
/* visorpdf [arquivo] - o Visor de PDF do SaguiOS (melhora 059, D138).
 *
 * Abre um PDF e mostra as suas páginas numa janela. O motor (userland/pdf/)
 * desenha a página num buffer RGBA e o programa compõe esse buffer sobre a
 * janela, assim dá para rolar e aplicar zoom sem voltar a desenhar.
 *
 * Controlos:
 *   <- ->        página anterior e seguinte
 *   cima baixo   rolar
 *   PgUp PgDn    rolar uma página
 *   Inicio Fim   primeira e última página
 *   + -          zoom
 *   0            ajustar à janela
 *   1            tamanho real (100%)
 *   F11          tela cheia
 *   Ctrl+O       abrir outro arquivo
 *
 * Modos para as provas:
 *   visorpdf -t arquivo              abre o PDF e diz quantas páginas tem,
 *                                     o título e o tamanho (sem janela)
 *   visorpdf --ppm arquivo saida.ppm [página [zoom [palavra-passe]]]
 *                                     desenha a página e grava um PPM (P6),
 *                                     para comparar com o render de referência
 */
#include "sagui.h"
#include "gui.h"
#include "pdf.h"

#define LARG_INICIO 900
#define ALT_INICIO  700
#define LARG_MIN 520
#define ALT_MIN  380
#define BARRA      36
#define RODAPE     26
#define ZOOM_MIN   25
#define ZOOM_MAX   300
#define PASSO_ZOOM 25
#define QTD_BT     6

/* ------------------------------------------------------------ estado */

static jan_t *j;
static pdf_t *doc;
static char caminho[256];
static int  pagina = 0;
static int  n_paginas = 0;
static int  zoom = 100;
static uint8_t *render;        /* o RGBA da página desenhada */
static int  render_larg, render_alt;
static int  desl_x, desl_y;    /* quanto se já andou (em pixels do render) */
static bool tela_cheia;
static char aviso[160];

static botao_t bt[QTD_BT];
static const char *const ROT_BT[QTD_BT] = { "Anterior", "Seguinte", "-", "+", "Ajustar", "1:1" };

/* ------------------------------------------------------------ desenho */

/* O pedaço da página que se vê. */
static void area_pagina(int *x, int *y, int *l, int *a)
{
    *x = 0;
    *y = BARRA;
    *l = j->larg;
    *a = j->alt - BARRA - RODAPE;
    if (*l < 1) *l = 1;
    if (*a < 1) *a = 1;
}

/* Desenha a barra de botões de cima. */
static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, j->larg, BARRA, 0xE6E9EC);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);

    int x = 8;
    for (int i = 0; i < QTD_BT; i++) {
        int l = g_largura_texto(ROT_BT[i]) + 20;
        bt[i].x = x; bt[i].y = 7; bt[i].l = l; bt[i].a = 24;
        bt[i].texto = ROT_BT[i];
        bt[i].apertado = false;
        botao_desenhar(j, &bt[i]);
        x += l + 4;
    }

    /* O número de página, a la derecha da barra. */
    char texto[64];
    formatar_texto(texto, sizeof texto, "Página %d de %d", pagina + 1, n_paginas);
    int tx = j->larg - 12 - g_largura_texto(texto);
    if (tx > x + 8) g_texto(j, tx, (BARRA - LETRA_A) / 2, texto, COR_TEXTO);
}

/* A barra de baixo: o nome do arquivo e o estado. */
static void desenhar_rodape(void)
{
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E9EC);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);

    int larg_pl = pdf_pagina_largura(doc, pagina);
    int alt_pl = pdf_pagina_altura(doc, pagina);
    char dir[64];
    formatar_texto(dir, sizeof dir, "%d x %d pt   %d%%", larg_pl, alt_pl, zoom);
    g_texto(j, j->larg - 12 - g_largura_texto(dir), y + (RODAPE - LETRA_A) / 2, dir, COR_TEXTO_FRACO);

    if (aviso[0]) {
        g_texto_n(j, 10, y + (RODAPE - LETRA_A) / 2, aviso, (j->larg - 12 - g_largura_texto(dir) - 20) / LETRA_L,
                  COR_DESTAQUE);
    } else {
        g_texto_n(j, 10, y + (RODAPE - LETRA_A) / 2, caminho, (j->larg - 12 - g_largura_texto(dir) - 20) / LETRA_L,
                  COR_TEXTO_FRACO);
    }
}

/* Pinta a página (a parte que se vê) sobre a janela. */
static void desenhar_pagina(void)
{
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    g_retangulo(j, x0, y0, vl, va, 0x6A737C);
    if (!doc || !render) return;

    /* Donde cae la página dentro del área. */
    int px = x0 + (vl - render_larg) / 2;
    int py = y0 + (va - render_alt) / 2;
    if (render_larg > vl) px = x0 - desl_x;
    if (render_alt > va) py = y0 - desl_y;

    /* El papel: blanco. */
    g_retangulo(j, px, py, render_larg, render_alt, 0xFFFFFF);

    /* El recorte: la parte del render que se ve. */
    int sx = 0, sy = 0;
    if (px < x0) { sx = x0 - px; px = x0; }
    if (py < y0) { sy = y0 - py; py = y0; }
    int cl = render_larg - sx;
    int ca = render_alt - sy;
    if (px + cl > x0 + vl) cl = x0 + vl - px;
    if (py + ca > y0 + va) ca = y0 + va - py;
    if (cl <= 0 || ca <= 0) return;

    for (int y = 0; y < ca; y++) {
        const uint8_t *src = render + ((size_t)(sy + y) * render_larg + sx) * 4;
        uint32_t *dst = j->px + (size_t)(py + y) * j->larg + px;
        for (int x = 0; x < cl; x++) {
            uint32_t a = src[x * 4 + 3];
            if (!a) continue;
            /* Mezcla sobre el papel blanco. */
            uint32_t r = src[x * 4], g = src[x * 4 + 1], b = src[x * 4 + 2];
            r = (r * a + 255 * (255 - a)) / 255;
            g = (g * a + 255 * (255 - a)) / 255;
            b = (b * a + 255 * (255 - a)) / 255;
            dst[x] = (r << 16) | (g << 8) | b;
        }
    }
    g_moldura(j, px - 1, py - 1, cl + 2, ca + 2, 0x4A525A);
}

static void desenhar_tudo(void)
{
    desenhar_barra();
    desenhar_pagina();
    desenhar_rodape();
    jan_mostrar(j);
}

/* ------------------------------------------------------------ o PDF */

/* Desenha a página atual no buffer. */
static bool renderizar(void)
{
    if (!doc) return false;
    int pl = pdf_pagina_largura(doc, pagina);
    int pa = pdf_pagina_altura(doc, pagina);
    if (pl <= 0 || pa <= 0) return false;

    int larg = pl * zoom / 100;
    int alt = pa * zoom / 100;
    if (larg < 1) larg = 1;
    if (alt < 1) alt = 1;
    /* Tope de memória: el búfer son 4 bytes por píxel. */
    while ((size_t)larg * alt * 4 > 24u * 1024 * 1024) {
        larg = larg * 3 / 4;
        alt = alt * 3 / 4;
        if (larg < 1 || alt < 1) return false;
    }

    if (larg != render_larg || alt != render_alt) {
        if (render) libera(render);
        render = aloca((size_t)larg * alt * 4);
        if (!render) {
            render_larg = render_alt = 0;
            formatar_texto(aviso, sizeof aviso, "Sem memória para desenhar a página");
            return false;
        }
        render_larg = larg;
        render_alt = alt;
        desl_x = desl_y = 0;
    }
    memset(render, 0, (size_t)larg * alt * 4);
    int r = pdf_render(doc, pagina, larg, alt, render);
    if (r != 0) {
        formatar_texto(aviso, sizeof aviso, "Erro ao desenhar: %s", pdf_erro_texto(r));
        return false;
    }
    return true;
}

/* O zoom que hace que la página quepa en la ventana. */
static void ajustar_zoom(void)
{
    if (!doc) return;
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    int pl = pdf_pagina_largura(doc, pagina);
    int pa = pdf_pagina_altura(doc, pagina);
    if (pl <= 0 || pa <= 0) return;
    int z1 = vl * 100 / pl;
    int z2 = va * 100 / pa;
    int z = (z1 < z2) ? z1 : z2;
    z = z / 5 * 5;              /* a múltiplos de 5 */
    if (z < ZOOM_MIN) z = ZOOM_MIN;
    if (z > ZOOM_MAX) z = ZOOM_MAX;
    zoom = z;
}

static void ir_pagina(int p)
{
    if (!doc || n_paginas <= 0) return;
    if (p < 0) p = 0;
    if (p >= n_paginas) p = n_paginas - 1;
    if (p == pagina) return;
    pagina = p;
    ajustar_zoom();
    renderizar();
    desenhar_tudo();
}

/* Abre un PDF (o lo vuelve a abrir). */
static bool abrir_arquivo(const char *caminho_pdf)
{
    int erro = 0;
    pdf_t *d = pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        /* Quizá pide contraseña. */
        char senha[64];
        formatar_texto(aviso, sizeof aviso, "%s", pdf_erro_texto(erro));
        if (pedir_texto("Visor de PDF", "Contraseña do PDF:", senha, sizeof senha)) {
            d = pdf_abrir_com_contrasena(caminho_pdf, senha, &erro);
        }
    }
    if (!d) {
        mensagem("Visor de PDF", pdf_erro_texto(erro));
        return false;
    }
    if (pdf_necesita_contrasena(d)) {
        char senha[64];
        if (!pedir_texto("Visor de PDF", "Contraseña do PDF:", senha, sizeof senha)) {
            pdf_fechar(d);
            return false;
        }
        pdf_fechar(d);
        d = pdf_abrir_com_contrasena(caminho_pdf, senha, &erro);
        if (!d) {
            mensagem("Visor de PDF", pdf_erro_texto(erro));
            return false;
        }
    }
    if (doc) pdf_fechar(doc);
    doc = d;
    copia_texto(caminho, caminho_pdf, sizeof caminho);
    n_paginas = pdf_paginas(doc);
    pagina = 0;
    aviso[0] = 0;
    ajustar_zoom();
    if (!renderizar()) return false;
    desenhar_tudo();
    /* O aviso vai para a serie: assim da para saber que ja esta pronto
     * (como faz o Archivos). */
    imprimir_em(SAIDA, "visorpdf: pronto (%d páginas)\n", n_paginas);
    return true;
}

/* ------------------------------------------------------------ eventos */

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            tela_cheia = cheia;
            ajustar_zoom();
            renderizar();
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_TECLA) {
        aviso[0] = 0;
        switch (e->tecla) {
        case TECLA_ESQUERDA: ir_pagina(pagina - 1); return true;
        case TECLA_DIREITA: ir_pagina(pagina + 1); return true;
        case TECLA_CIMA: desl_y -= 48; if (desl_y < 0) desl_y = 0; desenhar_pagina(); desenhar_rodape(); return true;
        case TECLA_BAIXO: desl_y += 48; if (desl_y > render_alt) desl_y = render_alt; desenhar_pagina(); desenhar_rodape(); return true;
        case TECLA_PGUP: desl_y -= (j->alt - BARRA - RODAPE); if (desl_y < 0) desl_y = 0; desenhar_pagina(); desenhar_rodape(); return true;
        case TECLA_PGDN: desl_y += (j->alt - BARRA - RODAPE); if (desl_y > render_alt) desl_y = render_alt; desenhar_pagina(); desenhar_rodape(); return true;
        case TECLA_HOME: ir_pagina(0); return true;
        case TECLA_END: ir_pagina(n_paginas - 1); return true;
        case '+': case '=': zoom += PASSO_ZOOM; if (zoom > ZOOM_MAX) zoom = ZOOM_MAX; renderizar(); desenhar_tudo(); return true;
        case '-': zoom -= PASSO_ZOOM; if (zoom < ZOOM_MIN) zoom = ZOOM_MIN; renderizar(); desenhar_tudo(); return true;
        case '0': ajustar_zoom(); renderizar(); desenhar_tudo(); return true;
        case '1': zoom = 100; renderizar(); desenhar_tudo(); return true;
        case TECLA_F11: jan_tela_cheia(j, !tela_cheia); return true;
        case 15: {   /* Ctrl+O */
            char outro[256];
            copia_texto(outro, caminho, sizeof outro);
            if (pedir_texto("Visor de PDF", "Caminho do PDF:", outro, sizeof outro))
                abrir_arquivo(outro);
            return true;
        }
        default: return true;
        }
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (n == 0) n = 1;
        desl_y += n * 48;
        if (desl_y < 0) desl_y = 0;
        if (desl_y > render_alt) desl_y = render_alt;
        desenhar_pagina();
        desenhar_rodape();
        return true;
    }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU) {
        for (int i = 0; i < QTD_BT; i++)
            if (botao_tratar(j, &bt[i], e)) {
                aviso[0] = 0;
                switch (i) {
                case 0: ir_pagina(pagina - 1); return true;
                case 1: ir_pagina(pagina + 1); return true;
                case 2: zoom -= PASSO_ZOOM; if (zoom < ZOOM_MIN) zoom = ZOOM_MIN; renderizar(); desenhar_tudo(); return true;
                case 3: zoom += PASSO_ZOOM; if (zoom > ZOOM_MAX) zoom = ZOOM_MAX; renderizar(); desenhar_tudo(); return true;
                case 4: ajustar_zoom(); renderizar(); desenhar_tudo(); return true;
                case 5: zoom = 100; renderizar(); desenhar_tudo(); return true;
                }
            }
    }
    return true;
}

/* ------------------------------------------------------------ modos de prova */

/* -t: abre o PDF e diz o que tem. */
static int modo_testar(const char *caminho_pdf, const char *contrasena)
{
    int erro = 0;
    pdf_t *d = contrasena ? pdf_abrir_com_contrasena(caminho_pdf, contrasena, &erro)
                           : pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    if (pdf_necesita_contrasena(d)) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(PDF_ERRO_CIFRADO));
        pdf_fechar(d);
        return 1;
    }
    int n = pdf_paginas(d);
    imprimir_em(SAIDA, "paginas: %d\n", n);
    const char *t = pdf_titulo(d);
    if (t && t[0]) imprimir_em(SAIDA, "titulo: %s\n", t);
    const char *a = pdf_autor(d);
    if (a && a[0]) imprimir_em(SAIDA, "autor: %s\n", a);
    for (int i = 0; i < n; i++)
        imprimir_em(SAIDA, "pagina %d: %d x %d\n", i + 1, pdf_pagina_largura(d, i), pdf_pagina_altura(d, i));

    unsigned t0 = tempo_ms();
    int larg = pdf_pagina_largura(d, 0) * 100 / 100;
    int alt = pdf_pagina_altura(d, 0);
    if (larg > 0 && alt > 0) {
        uint8_t *buf = aloca((size_t)larg * alt * 4);
        if (buf) {
            memset(buf, 0, (size_t)larg * alt * 4);
            int r = pdf_render(d, 0, larg, alt, buf);
            unsigned t1 = tempo_ms();
            /* Um checksum simples para comparar com o render de referência. */
            unsigned soma = 0;
            for (size_t k = 0; k < (size_t)larg * alt * 4; k += 997) soma = soma * 31 + buf[k];
            imprimir_em(SAIDA, "render: %d ms, soma %u\n", t1 - t0, soma);
            libera(buf);
            if (r != 0) {
                imprimir_em(ERROS, "visorpdf: erro ao desenhar: %s\n", pdf_erro_texto(r));
                pdf_fechar(d);
                return 1;
            }
        }
    }
    pdf_fechar(d);
    return 0;
}

/* --ppm: desenha a página e grava um PPM (P6) para comparar. */
static int modo_ppm(const char *caminho_pdf, const char *saida, int pag, int z,
                    const char *senha)
{
    int erro = 0;
    pdf_t *d = senha ? pdf_abrir_com_contrasena(caminho_pdf, senha, &erro)
                     : pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    if (pdf_necesita_contrasena(d)) {
        imprimir_em(ERROS, "visorpdf: %s está cifrado: falta a palavra-passe\n", caminho_pdf);
        pdf_fechar(d);
        return 1;
    }
    if (pag < 0 || pag >= pdf_paginas(d)) {
        imprimir_em(ERROS, "visorpdf: não há essa página\n");
        pdf_fechar(d);
        return 1;
    }
    int larg = pdf_pagina_largura(d, pag) * z / 100;
    int alt = pdf_pagina_altura(d, pag) * z / 100;
    if (larg < 1) larg = 1;
    if (alt < 1) alt = 1;
    uint8_t *buf = aloca((size_t)larg * alt * 4);
    if (!buf) {
        imprimir_em(ERROS, "visorpdf: sem memória\n");
        pdf_fechar(d);
        return 1;
    }
    memset(buf, 0, (size_t)larg * alt * 4);
    int r = pdf_render(d, pag, larg, alt, buf);
    if (r != 0) {
        imprimir_em(ERROS, "visorpdf: %s\n", pdf_erro_texto(r));
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    int fd = abrir(saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        imprimir_em(ERROS, "visorpdf: não consegui criar %s\n", saida);
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    char cab[64];
    int n = formatar_texto(cab, sizeof cab, "P6\n%d %d\n255\n", larg, alt);
    /* escrever_tudo: a escrever pode escrever menos bytes do que se pede
     * (o ficheiro fica truncado se nao se repetir). */
    if (escrever_tudo(fd, cab, (unsigned)n) < 0) {
        imprimir_em(ERROS, "visorpdf: não consegui escrever %s\n", saida);
        fechar(fd);
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    uint8_t *linha = aloca((size_t)larg * 3);
    for (int y = 0; y < alt; y++) {
        for (int x = 0; x < larg; x++) {
            const uint8_t *p = buf + ((size_t)y * larg + x) * 4;
            uint32_t a = p[3];
            linha[x * 3 + 0] = (uint8_t)((p[0] * a + 255 * (255 - a)) / 255);
            linha[x * 3 + 1] = (uint8_t)((p[1] * a + 255 * (255 - a)) / 255);
            linha[x * 3 + 2] = (uint8_t)((p[2] * a + 255 * (255 - a)) / 255);
        }
        escrever_tudo(fd, linha, (unsigned)(larg * 3));
    }
    libera(linha);
    fechar(fd);
    libera(buf);
    pdf_fechar(d);
    imprimir_em(SAIDA, "ppm: %s (%dx%d)\n", saida, larg, alt);
    return 0;
}

/* ------------------------------------------------------------ programa */

int principal(int argc, char **argv)
{
    int i = 1;
    if (i < argc && (compara_texto(argv[i], "-t") == 0 || compara_texto(argv[i], "--testar") == 0)) {
        if (i + 1 >= argc) { imprimir_em(ERROS, "uso: visorpdf -t arquivo [palavra-passe]\n"); return 2; }
        return modo_testar(argv[i + 1], i + 2 < argc ? argv[i + 2] : NULL);
    }
    if (i < argc && compara_texto(argv[i], "--ppm") == 0) {
        if (i + 3 >= argc) { imprimir_em(ERROS, "uso: visorpdf --ppm arquivo saida.ppm [página [zoom [palavra-passe]]]\n"); return 2; }
        int pag = i + 3 < argc ? texto_para_int(argv[i + 3]) : 1;
        int z = i + 4 < argc ? texto_para_int(argv[i + 4]) : 100;
        const char *senha = i + 5 < argc ? argv[i + 5] : NULL;
        return modo_ppm(argv[i + 1], argv[i + 2], pag - 1, z, senha);
    }
    if (i >= argc) {
        imprimir_em(ERROS, "uso: visorpdf [-t|--ppm] arquivo\n");
        return 2;
    }

    j = jan_abrir("Visor de PDF", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, LARG_MIN, ALT_MIN);

    if (!abrir_arquivo(argv[i])) {
        jan_fechar(j);
        return 1;
    }

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (esperar_evento(j->id, &e, 200) == 1) continuar = tratar(&e);
    }
    registrar("visorpdf: fechado\n");
    if (render) libera(render);
    if (doc) pdf_fechar(doc);
    jan_fechar(j);
    return 0;
}
