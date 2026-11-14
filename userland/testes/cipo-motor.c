// SPDX-License-Identifier: GPL-2.0-or-later
/* cipo-motor - teste do motor da web do Cipó (melhoria 081).
 *
 * Carrega um HTML, faz o layout, pinta a pagina e diz no log o que aconteceu:
 * quantos nós a árvore tem, quantos comandos de pintura houve e quantos
 * pixels ficaram fora do fundo. E o teste minimo do motor: se isto pintar
 * texto com as fontes do sistema, o motor funciona dentro do SaguiOS.
 *
 * Segunda fase: o filtro CSS (grayscale, brightness, opacity, blur) e as
 * animações (com @keyframes e com o relógio do motor, motor_tick). Aqui não
 * há OCR: o programa diz as cores dos pixels que pintou e o teste le these
 * valores do log.
 *
 * Quarta fase: as transições CSS (transition). Uma página cuja cor e cuja
 * largura mudam (por causa de uma regra @media) tem de mostrar os valores do
 * meio da transição, e uma página sem transition fica como sempre.
 */
#include "motor.h"
#include "js.h"

/* Os ganchos: este teste nao tem rede, so uma pagina local. */
static uint8_t *baixar(const char *url, unsigned *n)
{
    (void)url;
    *n = 0;
    return NULL;
}

static bool imagem(const uint8_t *d, unsigned n, uint32_t **px, int *l, int *a)
{
    return web_imagem_decodificar(d, n, px, l, a);
}

static const uint32_t *tela(int id, int *l, int *a)
{
    (void)id;
    if (l) *l = 0;
    if (a) *a = 0;
    return NULL;
}

#define LARG 400
#define ALT 64

/* A cor de um pixel da faixa pintada (0xRRGGBB). */
static uint32_t cor_em(uint32_t *px, int passo, int x, int y)
{
    if (x < 0 || x >= LARG || y < 0 || y >= ALT) return 0;
    return px[y * passo + x] & 0xFFFFFF;
}

/* Quantos pixels da região são exatamente a cor dada. */
static int quantos(uint32_t *px, int passo, int x0, int y0, int x1, int y1, uint32_t cor)
{
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if ((px[y * passo + x] & 0xFFFFFF) == cor) n++;
    return n;
}

/* O primeiro pixel da região que não é o fundo (o texto). */
static uint32_t primeiro(uint32_t *px, int passo, int x0, int y0, int x1, int y1, uint32_t fundo)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            uint32_t c = px[y * passo + x] & 0xFFFFFF;
            if (c != fundo) return c;
        }
    return 0;
}

/* A segunda fase: o filtro CSS e as animações. */
static void fase_filtro_anim(void)
{
    motor_ganchos_t g;
    g.baixar = baixar;
    g.imagem = imagem;
    g.tela = tela;

    motor_t *m = motor_novo(&g);
    if (!m) { registrar("filtro: sem motor\n"); return; }

    /* As caixas são absolutas para os pixels serem previsíveis. */
    const char *html =
        "<html><head><style>"
        "@keyframes troca { from { background-color: #ff0000; } to { background-color: #0000ff; } }"
        "</style></head><body>"
        "<div style=\"position:absolute;left:10px;top:10px;width:40px;height:20px;background:#ff0000;filter:grayscale(1)\"></div>"
        "<div style=\"position:absolute;left:60px;top:10px;width:40px;height:20px;background:#804000;filter:brightness(2)\"></div>"
        "<div style=\"position:absolute;left:110px;top:10px;width:40px;height:20px;background:#00ff00;filter:opacity(0.5)\"></div>"
        "<div style=\"position:absolute;left:160px;top:10px;width:40px;height:20px;background:#ff0000;filter:blur(4px)\"></div>"
        "<div style=\"position:absolute;left:210px;top:10px;width:40px;height:20px;background:#00ff00;animation:troca 1s\"></div>"
        "<div style=\"position:absolute;left:10px;top:40px;width:120px;height:20px;background:#ffffff;filter:grayscale(1)\">"
        "<span style=\"color:#ff0000\">T</span></div>"
        "</body></html>";

    motor_ler(m, html, (int)tam_texto(html), "http://exemplo/");
    motor_layout(m, LARG, ALT);
    uint32_t fundo = motor_fundo(m);
    uint32_t *px = (uint32_t *)aloca((size_t)LARG * (size_t)ALT * sizeof(uint32_t));
    if (!px) { registrar("filtro: sem memoria\n"); motor_fim(m); return; }
    for (int i = 0; i < LARG * ALT; i++) px[i] = fundo;
    motor_pintar(m, px, LARG, LARG, 0, ALT);

    /* o texto herda o filtro do contentor: tem de sair cinzento */
    uint32_t texto = primeiro(px, LARG, 10, 40, 130, 62, 0xFFFFFF);
    uint32_t cinza = cor_em(px, LARG, 20, 15);
    uint32_t brilho = cor_em(px, LARG, 70, 15);
    uint32_t opac = cor_em(px, LARG, 120, 15);
    int n_texto = quantos(px, LARG, 10, 40, 130, 62, 0x4c4c4c);
    int n_texto2 = quantos(px, LARG, 10, 40, 130, 62, 0x9a9a9a);
    int n_preto = quantos(px, LARG, 10, 40, 130, 62, 0x000000);
    int n_branco = quantos(px, LARG, 10, 40, 130, 62, 0xffffff);
    int n_vermelho = 0;
    for (int y = 40; y < 62; y++)
        for (int x = 10; x < 130; x++) {
            uint32_t c = px[y * LARG + x] & 0xFFFFFF;
            if (((c >> 16) & 255) > ((c >> 8) & 255) + 40) n_vermelho++;
        }

    /* o desfoque: os cantos da caixa já não são o vermelho puro */
    uint32_t canto = cor_em(px, LARG, 161, 11);
    int vermelhos = quantos(px, LARG, 160, 10, 200, 30, 0xFF0000);

    /* a animação: o instante decide a cor */
    uint32_t base = cor_em(px, LARG, 220, 15);
    bool t0 = motor_tick(m, 0);
    for (int i = 0; i < LARG * ALT; i++) px[i] = fundo;
    motor_pintar(m, px, LARG, LARG, 0, ALT);
    uint32_t c0 = cor_em(px, LARG, 220, 15);
    bool t500 = motor_tick(m, 500);
    for (int i = 0; i < LARG * ALT; i++) px[i] = fundo;
    motor_pintar(m, px, LARG, LARG, 0, ALT);
    uint32_t c500 = cor_em(px, LARG, 220, 15);
    bool t500b = motor_tick(m, 500);
    bool t1000 = motor_tick(m, 1000);

    registrar("filtro: cinza=%06x brilho=%06x opac=%06x canto=%06x texto=%06x vermelhos=%d\n",
              cinza, brilho, opac, canto, texto, vermelhos);
    registrar("texto: ncinza=%d n9a=%d npreto=%d nbranco=%d nvermelho=%d\n",
              n_texto, n_texto2, n_preto, n_branco, n_vermelho);
    registrar("anim: base=%06x t0=%06x t500=%06x tick0=%d tick500=%d tick500b=%d tick1000=%d\n",
              base, c0, c500, t0 ? 1 : 0, t500 ? 1 : 0, t500b ? 1 : 0, t1000 ? 1 : 0);

    libera(px);
    motor_fim(m);
}

/* A terceira fase: o tamanho do <img> quando o HTML traz width/height.
 * A imagem é um PNG 4x2 (vermelho) em data:, para que o tamanho natural
 * seja pequeno e se veja que o atributo manda. */
#define IMG_PNG "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAQAAAACCAIAAADwyuo0AAAAEElEQVR4nGP4z8AARwzIHABvqgf5gNwAKAAAAABJRU5ErkJggg=="

/* A caixa que os pixels de uma cor ocupam dentro da região. */
static void caixa_cor(uint32_t *px, int passo, int x0, int y0, int x1, int y1, uint32_t cor, int *l, int *a)
{
    int minx = x1, maxx = x0 - 1, miny = y1, maxy = y0 - 1;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            if ((px[y * passo + x] & 0xFFFFFF) != cor) continue;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    *l = maxx >= minx ? maxx - minx + 1 : 0;
    *a = maxy >= miny ? maxy - miny + 1 : 0;
}

static void caixa(uint32_t *px, int passo, int x0, int y0, int x1, int y1, int *l, int *a)
{
    caixa_cor(px, passo, x0, y0, x1, y1, 0xFF0000, l, a);
}

static void fase_imagem(void)
{
    motor_ganchos_t g;
    g.baixar = baixar;
    g.imagem = imagem;
    g.tela = tela;

    motor_t *m = motor_novo(&g);
    if (!m) { registrar("img: sem motor\n"); return; }

    const char *html =
        "<html><body>"
        "<img style=\"position:absolute;left:10px;top:10px\" src=\"" IMG_PNG "\" width=\"120\" height=\"60\">"
        "<img style=\"position:absolute;left:10px;top:80px\" src=\"" IMG_PNG "\" width=\"80\">"
        "<img style=\"position:absolute;left:10px;top:130px\" src=\"" IMG_PNG "\" height=\"30\">"
        "<img style=\"position:absolute;left:10px;top:180px;width:200px\" src=\"" IMG_PNG "\" width=\"120\" height=\"60\">"
        "<img style=\"position:absolute;left:250px;top:10px\" src=\"http://exemplo/nao-existe.png\" alt=\"Foto\" width=\"100\" height=\"20\">"
        "</body></html>";

    motor_ler(m, html, (int)tam_texto(html), "http://exemplo/");
    motor_imagens(m, 8);          /* as imagens chegam e são descodificadas */
    motor_layout(m, LARG, 260);
    uint32_t fundo = motor_fundo(m);
    uint32_t *px = (uint32_t *)aloca((size_t)LARG * 260 * sizeof(uint32_t));
    if (!px) { registrar("img: sem memoria\n"); motor_fim(m); return; }
    for (int i = 0; i < LARG * 260; i++) px[i] = fundo;
    motor_pintar(m, px, LARG, LARG, 0, 260);

    int l1, a1, l2, a2, l3, a3, l4, a4, l5, a5;
    caixa(px, LARG, 0, 0, 200, 70, &l1, &a1);
    caixa(px, LARG, 0, 75, 200, 125, &l2, &a2);
    caixa(px, LARG, 0, 128, 200, 170, &l3, &a3);
    caixa(px, LARG, 0, 175, 250, 245, &l4, &a4);
    /* o texto alt: cinzento, dentro da caixa da imagem */
    caixa_cor(px, LARG, 250, 0, 380, 40, 0x6A737C, &l5, &a5);
    registrar("img: imgl=%d imga=%d img2l=%d img2a=%d img3l=%d img3a=%d img4l=%d img4a=%d altl=%d alta=%d\n",
              l1, a1, l2, a2, l3, a3, l4, a4, l5, a5);

    libera(px);
    motor_fim(m);
}

/* A quarta fase: as transições CSS.
 *
 * A janela estreita faz entrar uma regra @media, que muda a cor de uma caixa
 * e a largura de outra. Com transition, os valores mudam aos poucos (o motor
 * interpola); sem transition, mudam de repente. O programa diz as cores que
 * pintou e as larguras, e o teste le esses valores do log. */

#define LARG_TRANS 400
#define ALT_TRANS 64

/* A página com transição (a cor e a largura mudam com a janela estreita). */
static const char *html_trans =
    "<html><head><style>"
    ".caixa{position:absolute;left:10px;top:10px;width:100px;height:20px;background:#ff0000;transition:background-color 300ms}"
    ".larga{position:absolute;left:10px;top:40px;width:100px;height:20px;background:#00ff00;transition:width 300ms linear}"
    ".forma{position:absolute;left:120px;top:10px;width:60px;height:20px;background:#ffff00;"
    "transition-property:background-color;transition-duration:300ms;transition-timing-function:ease-out}"
    "@media (max-width: 300px){.caixa{background-color:#0000ff}.larga{width:150px}.forma{background-color:#00ff00}}"
    "</style></head><body>"
    "<div class=\"caixa\"></div>"
    "<div class=\"larga\"></div>"
    "<div class=\"forma\"></div>"
    "</body></html>";

/* A mesma página sem transition: os números da pintura têm de ser os de
 * sempre (a transição não pode mudar a página). */
static const char *html_sem_trans =
    "<html><head><style>"
    ".caixa{position:absolute;left:10px;top:10px;width:100px;height:20px;background:#ff0000}"
    ".larga{position:absolute;left:10px;top:40px;width:100px;height:20px;background:#00ff00}"
    ".forma{position:absolute;left:120px;top:10px;width:60px;height:20px;background:#ffff00}"
    "@media (max-width: 300px){.caixa{background-color:#0000ff}.larga{width:150px}.forma{background-color:#00ff00}}"
    "</style></head><body>"
    "<div class=\"caixa\"></div>"
    "<div class=\"larga\"></div>"
    "<div class=\"forma\"></div>"
    "</body></html>";

/* Os números da pintura de uma página (para comparar com e sem transição). */
static void numeros_pagina(const char *html, int *nos, int *cmds, int *alt)
{
    motor_ganchos_t g;
    g.baixar = baixar;
    g.imagem = imagem;
    g.tela = tela;
    motor_t *m = motor_novo(&g);
    if (!m) { *nos = *cmds = *alt = -1; return; }
    motor_ler(m, html, (int)tam_texto(html), "http://exemplo/");
    int a = motor_layout(m, 400, 64);
    int imgs = 0;
    motor_numeros(m, nos, cmds, &imgs);
    *alt = a;
    motor_fim(m);
}

static void fase_transicao(void)
{
    motor_ganchos_t g;
    g.baixar = baixar;
    g.imagem = imagem;
    g.tela = tela;

    motor_t *m = motor_novo(&g);
    if (!m) { registrar("trans: sem motor\n"); return; }
    motor_ler(m, html_trans, (int)tam_texto(html_trans), "http://exemplo/");
    motor_tick(m, 0);                 /* o relógio do motor começa */
    motor_layout(m, LARG_TRANS, ALT_TRANS);   /* janela larga: a caixa é vermelha */

    uint32_t fundo = motor_fundo(m);
    uint32_t *px = (uint32_t *)aloca((size_t)LARG_TRANS * (size_t)ALT_TRANS * sizeof(uint32_t));
    if (!px) { registrar("trans: sem memoria\n"); motor_fim(m); return; }

    /* pinta a faixa e guarda as cores e a largura da caixa verde */
    #define PINTAR() do { \
        for (int i = 0; i < LARG_TRANS * ALT_TRANS; i++) px[i] = fundo; \
        motor_pintar(m, px, LARG_TRANS, LARG_TRANS, 0, ALT_TRANS); \
    } while (0)

    PINTAR();
    uint32_t cor_ini = cor_em(px, LARG_TRANS, 20, 15);
    uint32_t cor_forma_ini = cor_em(px, LARG_TRANS, 130, 15);
    int l_ini = quantos(px, LARG_TRANS, 0, 45, 300, 46, 0x00FF00);

    /* a janela estreita: entra a regra do @media e os valores mudam */
    motor_layout(m, 200, ALT_TRANS);
    PINTAR();
    uint32_t cor_t0 = cor_em(px, LARG_TRANS, 20, 15);
    uint32_t cor_forma_t0 = cor_em(px, LARG_TRANS, 130, 15);
    int l_t0 = quantos(px, LARG_TRANS, 0, 45, 300, 46, 0x00FF00);

    /* a meio da transição os valores são intermédios */
    bool t150 = motor_tick(m, 150);
    PINTAR();
    uint32_t cor_t150 = cor_em(px, LARG_TRANS, 20, 15);
    uint32_t cor_forma_t150 = cor_em(px, LARG_TRANS, 130, 15);
    int l_t150 = quantos(px, LARG_TRANS, 0, 45, 300, 46, 0x00FF00);

    /* no fim a transição acaba e o valor é o novo */
    bool t400 = motor_tick(m, 400);
    PINTAR();
    uint32_t cor_t400 = cor_em(px, LARG_TRANS, 20, 15);
    uint32_t cor_forma_t400 = cor_em(px, LARG_TRANS, 130, 15);
    int l_t400 = quantos(px, LARG_TRANS, 0, 45, 300, 46, 0x00FF00);

    /* depois disto o motor não tem mais nada para pintar */
    bool t500 = motor_tick(m, 500);

    int nos1 = 0, cmds1 = 0, alt1 = 0, nos2 = 0, cmds2 = 0, alt2 = 0;
    numeros_pagina(html_trans, &nos1, &cmds1, &alt1);
    numeros_pagina(html_sem_trans, &nos2, &cmds2, &alt2);

    registrar("trans: ini=%06x t0=%06x t150=%06x t400=%06x tick150=%d tick400=%d tick500=%d\n",
              cor_ini, cor_t0, cor_t150, cor_t400, t150 ? 1 : 0, t400 ? 1 : 0, t500 ? 1 : 0);
    registrar("transf: ini=%06x t0=%06x t150=%06x t400=%06x\n",
              cor_forma_ini, cor_forma_t0, cor_forma_t150, cor_forma_t400);
    registrar("transl: l_ini=%d l_t0=%d l_t150=%d l_t400=%d\n", l_ini, l_t0, l_t150, l_t400);
    registrar("transn: nos=%d cmds=%d alt=%d nos2=%d cmds2=%d alt2=%d\n",
              nos1, cmds1, alt1, nos2, cmds2, alt2);

    #undef PINTAR
    libera(px);
    motor_fim(m);
}

int principal(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    motor_ganchos_t g;
    g.baixar = baixar;
    g.imagem = imagem;
    g.tela = tela;

    motor_t *m = motor_novo(&g);
    if (!m) { registrar("motor: nao consegui criar\n"); return 1; }

    const char *html =
        "<html><head><style>h1 { color: #204080; } p { font-size: 18px; }</style></head>"
        "<body><h1>SaguiOS</h1>"
        "<p>Isto e um paragrafo com <b>negrito</b> e um <a href=\"http://x/\">link</a>.</p>"
        "<ul><li>primeiro item</li><li>segundo item</li></ul>"
        "</body></html>";

    motor_ler(m, html, (int)tam_texto(html), "http://exemplo/");
    int alt = motor_layout(m, 400, 300);
    int nos = 0, cmds = 0, imgs = 0;
    motor_numeros(m, &nos, &cmds, &imgs);

    uint32_t fundo = motor_fundo(m);
    uint32_t *px = (uint32_t *)aloca((size_t)400 * (size_t)(alt > 0 ? alt : 1) * sizeof(uint32_t));
    if (!px) { registrar("motor: sem memoria\n"); motor_fim(m); return 1; }
    for (int i = 0; i < 400 * (alt > 0 ? alt : 1); i++) px[i] = fundo;
    motor_pintar(m, px, 400, 400, 0, alt);

    int pintados = 0;
    for (int i = 0; i < 400 * (alt > 0 ? alt : 1); i++)
        if (px[i] != fundo) pintados++;

    registrar("motor: nos=%d cmds=%d imgs=%d alt=%d links=%d pintados=%d\n",
              nos, cmds, imgs, alt, motor_links(m), pintados);
    motor_fim(m);

    fase_filtro_anim();
    fase_imagem();
    fase_transicao();
    return 0;
}
