// SPDX-License-Identifier: GPL-2.0-or-later
/* cipo_js.h - lo que el Cipó hace con el JavaScript de la página (D271, D488).
 *
 * Solo lo incluye cipo.c (usa sus variables: el motor, el búfer de la página,
 * la rolagem). Va aparte para que los cambios del DOM y los eventos no se
 * mezclen con los de la red (D487) y los de CSS (D489), que tocan cipo.c a la vez.
 *
 * El puente del QuickJS (qjs_bridge.c y qjs_dom.js) tiene la copia del DOM que
 * ve el script; cada cambio llega aquí por un gancho y se pasa al motor
 * (motor_dom_*, en motor_dom.h), que marca solo lo que cambió. La maquetación se
 * hace una vez por vuelta del bucle (js_refrescar), o antes si el script pide una
 * medida (js_layout_sync); se vuelve a pintar solo la franja de la página que
 * cambió. Los textos del puente vienen en UTF-8 y el motor guarda Latin-1.
 */

static bool js_dom_sujo;                 /* el script cambió el DOM: hay que maquetar */
static int  js_faixa_y0 = 1 << 30, js_faixa_y1 = -(1 << 30);   /* lo que hay que volver a pintar (página) */
static int  js_layouts, js_layouts_inc;  /* estadística de la página: maquetaciones por el script */
static unsigned js_ms_layout, js_ms_pintura;
static int  js_hist_pendente;            /* history.back/forward pedido por el script (se hace fuera de él) */
static int  js_rol_visto = -1;           /* la rolagem que el script ya conoce (evento scroll) */
static uint8_t *motor_baixar(const char *url, unsigned *n);
static void desenhar_pagina(void);
static void pag_fb_pintar(int f0, int n);
static void pag_fb_apresentar(void);
static void desenhar_barra_rolagem(void);
static void rolar_px(int px);
static int latin1_para_utf8(const char *s, int n, char *o, int max);

/* Una copia en Latin-1 de un texto UTF-8 (aloca; la libera quien llama). */
static char *js_latin1(const char *s)
{
    if (!s) return NULL;
    size_t n = tam_texto(s);
    char *b = aloca(n + 1);
    if (!b) return NULL;
    memcpy(b, s, n + 1);
    b[utf8_para_latin1(b, n, NULL)] = 0;
    return b;
}

static void js_atributo(int dom, const char *nome, const char *valor)
{
    if (!motor || !nome) return;
    char *b = valor ? js_latin1(valor) : NULL;
    if (valor && !b) return;
    if (motor_dom_atributo(motor, dom, nome, b)) js_dom_sujo = true;
    if (b) libera(b);
}

static void js_texto(int dom, const char *txt)
{
    if (!motor || !txt) return;
    char *b = js_latin1(txt);
    if (!b) return;
    if (motor_dom_texto(motor, dom, b)) js_dom_sujo = true;
    libera(b);
}

/* ---- el DOM mutable (D488) ---- */

static int js_dom_criar(const char *tag, const char *texto)
{
    if (!motor) return -1;
    char *b = texto ? js_latin1(texto) : NULL;
    int id = motor_dom_criar(motor, tag, b);
    if (b) libera(b);
    return id;
}

static int js_dom_inserir(int pai, int no, int antes)
{
    if (!motor || !motor_dom_inserir(motor, pai, no, antes)) return 0;
    js_dom_sujo = true;
    return 1;
}

static int js_dom_tirar(int no)
{
    if (!motor || !motor_dom_tirar(motor, no)) return 0;
    js_dom_sujo = true;
    return 1;
}

static int js_dom_dado(int no, const char *txt)
{
    if (!motor) return 0;
    char *b = js_latin1(txt);
    if (!b) return 0;
    bool ok = motor_dom_dado(motor, no, b);
    libera(b);
    if (ok) js_dom_sujo = true;
    return ok;
}

static char *js_dom_html(int no, const char *html, int *n)
{
    *n = 0;
    if (!motor) return NULL;
    char *b = js_latin1(html);
    if (!b) return NULL;
    bool ok = motor_dom_html(motor, no, b, (int)tam_texto(b));
    libera(b);
    if (!ok) return NULL;
    js_dom_sujo = true;
    if (no == motor_dom_corpo(motor)) registrar("cipo: js: o corpo foi trocado (%d bytes, sem recarregar a pagina)\n", (int)tam_texto(html));
    return motor_dom_exportar(motor, no, n);
}

/* La maquetación pendiente, ahora (el script pidió una medida, o acabó la vuelta). */
static void js_layout_sync(void)
{
    if (!js_dom_sujo || !motor) return;
    js_dom_sujo = false;
    unsigned t0 = tempo_ms();
    motor_altura = motor_layout(motor, LARG - ROLAGEM, AREA_A);
    if (motor_altura < 0) motor_altura = 0;
    unsigned dt = tempo_ms() - t0;
    linhas = (motor_altura + LINHA_A - 1) / LINHA_A + 1;
    int est = 0, med = 0, y0 = 0, y1 = 0;
    motor_dom_estadistica(motor, &est, &med, &y0, &y1);
    js_layouts++;
    if (est >= 0) js_layouts_inc++;
    js_ms_layout += dt;
    if (y0 < js_faixa_y0) js_faixa_y0 = y0;
    if (y1 > js_faixa_y1) js_faixa_y1 = y1;
    if (js_layouts <= 40 || js_layouts % 100 == 0)
        registrar("cipo: js: layout %d %s: %d estilos, %d medidas guardadas, franja %d-%d, %u ms\n", js_layouts,
                  est >= 0 ? "incremental" : "completo", est, med, y0, y1 > 1000000 ? 1000000 : y1, dt);
    const char *pend[1];
    if (conf.imagens && motor_urls_pendentes(motor, pend, 1) > 0) recursos_pendentes = true;   /* el script puso imágenes */
}

/* Vuelve a pintar solo [y0, y1) de la página (lo que se ve de esa franja). */
static void js_pintar_faixa(int y0, int y1)
{
    if (!motor || painel) return;
    if (!pag_fb_ok || pintura_pendente || vid || !pag_fb_garantir() || motor_tem_camadas(motor)) { desenhar_pagina(); return; }
    int max = motor_altura > AREA_A ? motor_altura - AREA_A : 0;
    if (rol_px > max) { rol_px = max; topo = (rol_px + LINHA_A / 2) / LINHA_A; desenhar_pagina(); return; }
    int f0 = y0 - rol_px, f1 = y1 > (1 << 29) ? pag_fb_a : y1 - rol_px;
    if (f0 < 0) f0 = 0;
    if (f1 > pag_fb_a) f1 = pag_fb_a;
    desenhar_barra_rolagem();
    if (f1 <= f0) { jan_mostrar_area(j, LARG - ROLAGEM, ABA_Y, ROLAGEM, AREA_A); return; }
    unsigned t0 = tempo_ms();
    pag_fb_pintar(f0, f1 - f0);
    for (int y = f0; y < f1; y++)
        memcpy(j->px + (ABA_Y + y) * j->larg, pag_fb + y * pag_fb_l, (size_t)pag_fb_l * 4);
    jan_mostrar_area(j, 0, ABA_Y + f0, LARG - ROLAGEM, f1 - f0);
    jan_mostrar_area(j, LARG - ROLAGEM, ABA_Y, ROLAGEM, AREA_A);
    unsigned dt = tempo_ms() - t0;
    js_ms_pintura += dt;
    if (js_layouts <= 40 || js_layouts % 100 == 0) registrar("cipo: js: pintadas %d de %d filas en %u ms\n", f1 - f0, pag_fb_a, dt);
}

/* Al acabar una tarea del script: una sola maquetación y la franja que cambió. */
static void js_refrescar(void)
{
    js_layout_sync();
    if (js_faixa_y1 <= js_faixa_y0) return;
    int y0 = js_faixa_y0, y1 = js_faixa_y1;
    js_faixa_y0 = 1 << 30; js_faixa_y1 = -(1 << 30);
    js_pintar_faixa(y0, y1);
}

/* ---- medidas, desplazamiento e historial ---- */

static int js_caixa(int no, int *o)
{
    if (!motor) return 0;
    js_layout_sync();          /* «medidas bajo demanda»: solo si algo cambió */
    return motor_dom_caixa(motor, no, o) ? 1 : 0;
}

static int js_estilo(int no, const char *prop, char *o, int max)
{
    if (!motor) return 0;
    js_layout_sync();
    if (!motor_dom_estilo(motor, no, prop, o, max)) return 0;
    /* el motor da Latin-1 (font-family): al puente, UTF-8 */
    char t[256];
    int n = latin1_para_utf8(o, (int)tam_texto(o), t, (int)sizeof t);
    if (n < max) memcpy(o, t, (size_t)n + 1);
    return 1;
}

static int js_elemento_em(int x, int y)
{
    if (!motor) return -1;
    js_layout_sync();
    return motor_dom_em(motor, x, y + rol_px);
}

static void js_g_janela(int *o)
{
    if (motor) js_layout_sync();
    o[0] = LARG - ROLAGEM; o[1] = AREA_A; o[2] = 0; o[3] = rol_px;
    o[4] = LARG - ROLAGEM; o[5] = motor_altura;
}

static void js_g_rolar(int x, int y)
{
    (void)x;
    if (!motor) return;
    js_layout_sync();
    rolar_px(y);               /* el evento scroll sale en la vuelta siguiente (js_volta) */
}

/* pushState / replaceState / location.hash: la dirección cambia sin navegar.
 * La entrada que se deja queda en el historial marcada como «del mismo
 * documento» (js_geracao): al volver a ella no se descarga, se avisa al script
 * (popstate). */
static int historico_doc[HIST_MAX], futuro_doc[HIST_MAX];
static void js_historia(const char *url, int substituir)
{
    char *u = js_latin1(url);
    if (!u) return;
    if (!substituir && url_atual[0]) {
        if (qtd_historico == HIST_MAX) {
            memmove(historico[0], historico[1], sizeof historico[0] * (HIST_MAX - 1));
            memmove(historico_pos, historico_pos + 1, sizeof historico_pos[0] * (HIST_MAX - 1));
            memmove(historico_doc, historico_doc + 1, sizeof historico_doc[0] * (HIST_MAX - 1));
            qtd_historico--;
        }
        copia_texto(historico[qtd_historico], url_atual, URL_MAX);
        historico_pos[qtd_historico] = rol_px;
        historico_doc[qtd_historico] = js_geracao;
        qtd_historico++;
        qtd_futuro = 0;
    }
    copia_texto(url_atual, u, sizeof url_atual);
    caixa_definir(&endereco, url_atual);
    if (qtd_abas) copia_texto(abas[aba_atual].url, url_atual, sizeof abas[aba_atual].url);
    registrar("cipo: js: historia %s %s\n", substituir ? "replace" : "push", url_atual);
    libera(u);
    desenhar_barra();
}

static void js_ir(int delta) { js_hist_pendente = delta; }

/* scrollTop/scrollLeft de los bloques con overflow (D489) */
static int js_rolagem(int no, int *o)
{
    if (!motor) return 0;
    js_layout_sync();
    return motor_dom_rolagem(motor, no, o) ? 1 : 0;
}

static int js_rolar_caixa(int no, int x, int y)
{
    if (!motor) return 0;
    js_layout_sync();
    if (!motor_dom_rolar(motor, no, x, y)) return 0;
    js_faixa_y0 = 0; js_faixa_y1 = 1 << 30;     /* se pinta lo visible en la vuelta */
    return 1;
}

/* Para la prueba (h488): la firma de la lista de pintura después de maquetar lo
 * pendiente, y el remaquetado incremental apagado o encendido (apagarlo deja
 * pendiente una maquetación completa). */
static unsigned js_firma(void)
{
    if (!motor) return 0;
    js_layout_sync();
    return motor_dom_assinatura(motor);
}

static void js_incremental(int sim)
{
    if (!motor) return;
    motor_dom_incremental(motor, sim != 0);
    if (!sim) js_dom_sujo = true;
    registrar("cipo: js: remaquetado incremental %s\n", sim ? "encendido" : "apagado");
}

/* ¿Solo cambia el #? (la misma página) */
static bool js_so_ancora(const char *a, const char *b)
{
    int i = 0;
    while (a[i] && a[i] == b[i] && a[i] != '#') i++;
    return (a[i] == '#' || !a[i]) && (b[i] == '#' || !b[i]) && (a[i] == '#' || b[i] == '#');
}

/* Volver/avanzar dentro del mismo documento: popstate (y hashchange si solo cambió el #). */
static void js_popstate(int delta, const char *velha)
{
    char json[64];
    formatar_texto(json, sizeof json, "{\"delta\":%d,\"hash\":%d}", delta, js_so_ancora(velha, url_atual) ? 1 : 0);
    registrar("cipo: js: popstate %d %s\n", delta, url_atual);
    caixa_definir(&endereco, url_atual);
    desenhar_barra();
    qjs_despachar(js, -1, "popstate", json);
    js_refrescar();
}

/* ---- los eventos de la persona ---- */

static void js_json_rato(char *o, int max, int x, int y, int botao, int botoes, int cliques, int mods)
{
    formatar_texto(o, (size_t)max, "{\"x\":%d,\"y\":%d,\"botao\":%d,\"botoes\":%d,\"detalhe\":%d,\"shift\":%d,\"ctrl\":%d,\"alt\":%d}",
                   x, y, botao, botoes, cliques, mods & 1 ? 1 : 0, mods & 2 ? 1 : 0, mods & 4 ? 1 : 0);
}

/* El elemento del DOM bajo el puntero (coordenadas de la ventana), o -1 fuera de la página. */
static int js_alvo_rato(int x, int y)
{
    if (!motor || y < ABA_Y || y >= ALT - RODAPE || x >= LARG - ROLAGEM) return -1;
    return motor_dom_em(motor, x, y - ABA_Y + rol_px);
}

/* Botón izquierdo en la página: mousedown, mouseup, click (y dblclick). Devuelve true
 * si el script canceló el click (el navegador no sigue el enlace). */
static bool js_clique_pagina(int x, int y, int cliques, int mods)
{
    if (!js || !motor) return false;
    int t = js_alvo_rato(x, y);
    if (t < 0) return false;
    char json[192];
    js_json_rato(json, sizeof json, x, y - ABA_Y, 0, 1, cliques ? cliques : 1, mods);
    qjs_despachar(js, t, "mousedown", json);
    js_json_rato(json, sizeof json, x, y - ABA_Y, 0, 0, cliques ? cliques : 1, mods);
    qjs_despachar(js, t, "mouseup", json);
    int c = qjs_despachar(js, t, "click", json);
    if (cliques == 2) qjs_despachar(js, t, "dblclick", json);
    js_refrescar();
    return c == 1;
}

static bool js_menu_pagina(int x, int y, int mods)
{
    if (!js || !motor) return false;
    int t = js_alvo_rato(x, y);
    if (t < 0) return false;
    char json[192];
    js_json_rato(json, sizeof json, x, y - ABA_Y, 2, 2, 1, mods);
    int c = qjs_despachar(js, t, "contextmenu", json);
    js_refrescar();
    return c == 1;
}

static void js_mover_rato(int x, int y, int botoes)
{
    if (!js || !motor || !(qjs_quer(js) & QJS_QUER_RATO)) return;
    int t = js_alvo_rato(x, y);
    if (t < 0) return;
    char json[192];
    js_json_rato(json, sizeof json, x, y - ABA_Y, 0, botoes, 0, 0);
    qjs_despachar(js, t, "mousemove", json);
    js_refrescar();
}

/* El nombre de la tecla (KeyboardEvent.key), en UTF-8. */
static void js_nome_tecla(unsigned t, char *o, int max)
{
    const char *n = NULL;
    switch (t) {
    case TECLA_CIMA: n = "ArrowUp"; break;
    case TECLA_BAIXO: n = "ArrowDown"; break;
    case TECLA_ESQUERDA: n = "ArrowLeft"; break;
    case TECLA_DIREITA: n = "ArrowRight"; break;
    case TECLA_HOME: n = "Home"; break;
    case TECLA_END: n = "End"; break;
    case TECLA_PGUP: n = "PageUp"; break;
    case TECLA_PGDN: n = "PageDown"; break;
    case TECLA_DELETE: n = "Delete"; break;
    case TECLA_ESC: case 27: n = "Escape"; break;
    case '\n': case '\r': n = "Enter"; break;
    case '\b': n = "Backspace"; break;
    case '\t': n = "Tab"; break;
    default: break;
    }
    if (n) { copia_texto(o, n, (size_t)max); return; }
    if (t >= 32 && t < 256) {
        char c = (char)t;
        latin1_para_utf8(&c, 1, o, max);
        return;
    }
    copia_texto(o, "Unidentified", (size_t)max);
}

/* Una tecla para la página: keydown, keypress (si es un carácter) y keyup.
 * true = el script la canceló (no se desplaza, no se escribe). */
static bool js_tecla(unsigned t, int mods)
{
    if (!js || !motor) return false;
    char k[16], json[128], ke[40];
    js_nome_tecla(t, k, sizeof k);
    int n = 0;
    for (const char *q = k; *q && n < 36; q++) { if (*q == '"' || *q == '\\') ke[n++] = '\\'; ke[n++] = *q; }
    ke[n] = 0;
    formatar_texto(json, sizeof json, "{\"tecla\":\"%s\",\"shift\":%d,\"ctrl\":%d,\"alt\":%d}", ke, mods & 1 ? 1 : 0, mods & 2 ? 1 : 0, mods & 4 ? 1 : 0);
    int c = qjs_despachar(js, -1, "keydown", json);
    if (c != 1 && ((t >= 32 && t < 256) || t == '\n')) qjs_despachar(js, -1, "keypress", json);
    qjs_despachar(js, -1, "keyup", json);
    js_refrescar();
    return c == 1;
}

/* El campo con foco cambió de valor (UTF-8): input. */
static void js_campo_input(void)
{
    if (!js || !motor) return;
    int d = motor_dom_foco(motor);
    if (d < 0) return;
    const char *v = motor_foco_valor(motor);
    int n = (int)tam_texto(v ? v : "");
    char *json = aloca((size_t)n * 2 + 32);
    if (!json) return;
    int k = formatar_texto(json, 16, "{\"valor\":\"");
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)v[i];
        if (c == '"' || c == '\\') json[k++] = '\\';
        if (c >= 32) json[k++] = (char)c;
    }
    json[k++] = '"'; json[k++] = '}'; json[k] = 0;
    qjs_despachar(js, d, "input", json);
    libera(json);
    js_refrescar();
}

/* Envío de un formulario (Intro en un campo, o botón de envío): change y submit.
 * true = el script lo canceló. */
static bool js_formulario(int dom)
{
    if (!js || dom < 0) return false;
    qjs_despachar(js, dom, "change", "{}");
    int c = qjs_despachar(js, dom, "submit", "{}");
    js_refrescar();
    if (c == 1) registrar("cipo: js: el envio lo cancelo el script\n");
    return c == 1;
}

/* Una vuelta del bucle: los temporizadores y los quadros; la rolagem nueva (scroll). */
static void js_volta(void)
{
    if (!js) return;
    if (js_rol_visto != rol_px && motor) {
        if (js_rol_visto >= 0) qjs_despachar(js, -1, "scroll", "{}");
        js_rol_visto = rol_px;
    }
    if (qjs_tique(js, tempo_ms()) > 0) js_refrescar();
    else if (js_dom_sujo) js_refrescar();
}

/* Cuántos ms puede dormir el bucle sin retrasar al script. */
static int js_espera_ms(int maximo)
{
    if (!js) return maximo;
    int e = qjs_espera(js);
    if (e < 0 || e > maximo) return maximo;
    return e < 1 ? 1 : e;
}
