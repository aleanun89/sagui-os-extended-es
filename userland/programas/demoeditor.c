// SPDX-License-Identifier: GPL-3.0-only
/* demoeditor - demostración de los componentes de editor de la libsagui (D454, v8.5.6).
 *
 * Monta una disposición de editor: árbol de la escena | lienzo y código | inspector, y abajo los recursos (paleta de
 * casillas) y la consola, separados por paneles divisibles. Sirve para probar los componentes y para las capturas.
 * Datos sintéticos: una escena de cuatro objetos y una hoja de casillas dibujada por el propio programa.
 *
 *   - árbol: elegir un objeto; arrastrarlo al lienzo lo coloca allí (o a la consola, que solo avisa)
 *   - lienzo: rueda = zoom, botón central o Espacio + arrastrar = desplazar, rectángulo para elegir, asas para mover, escalar y girar
 *   - inspector: editar la posición, el tamaño, el giro, el color, la visibilidad, la capa, la opacidad y el nombre
 *   - recursos: elegir casillas; arrastrar las elegidas al lienzo crea un objeto por casilla (un solo paso de deshacer)
 *   - código: Lua con resaltado, sangría automática, Ctrl+F para buscar; «Comprobar» marca el error de bloques en el código y la consola
 *   - deshacer/rehacer de todo lo anterior (Ctrl+Z / Ctrl+Y fuera del código)
 * Cada acción también se escribe en la serie con «demoeditor: ...» para las pruebas automáticas. */
#include "sagui.h"
#include "gui.h"

#define VENT_L 960
#define VENT_A 640
#define BARRA_A 36
#define TITULO_A 18

enum { H_ARBOL = 1, H_VISTA, H_CODIGO, H_INSPETOR, H_RECURSOS, H_CONSOLA };
enum { F_NADA, F_ARBOL, F_VISTA, F_CODIGO, F_INSPETOR, F_RECURSOS, F_CONSOLA };
enum { TIPO_OBJ = 1, TIPO_CASILLA = 2 };

/* ------------------------------------------------------------ la escena */

typedef struct {
    char nome[32];
    int x, y, l, a, rot;
    uint32_t cor;          /* 0xAARRGGBB */
    int visivel, capa, opac, escala;   /* escala en centésimas */
    char script[64];
    int viva;
} obj_t;

#define MAXOBJ 24
static obj_t objs[MAXOBJ];
static int nobj, sel = -1;

static void obj_novo(const char *nome, int x, int y, int l, int a, uint32_t cor)
{
    obj_t *o = &objs[nobj++];
    memset(o, 0, sizeof *o);
    copia_texto(o->nome, nome, sizeof o->nome);
    o->x = x; o->y = y; o->l = l; o->a = a; o->cor = cor;
    o->visivel = 1; o->capa = 1; o->opac = 100; o->escala = 100; o->viva = 1;
    copia_texto(o->script, "/home/jugador.lua", sizeof o->script);
}

/* ------------------------------------------------------------ estado de la interfaz */

static jan_t *jn;
static paineis_t pn;
static arvore_t arv;
static const char *arv_txt[MAXOBJ + 1];
static unsigned char arv_niv[MAXOBJ + 1], arv_abr[MAXOBJ + 1];
static int arv_obj[MAXOBJ + 1];
static char arv_nomes[MAXOBJ + 1][40];
static edvista_t vs;
static inspetor_t ins;
static ins_prop_t props[16];
static edpaleta_t pal;
static imagem_t hoja;
static consola_t con;
static codigo_t *cod;
static pilha_desfazer_t pilha;
static mbotao_t btn[8];
static int foco = F_NADA;
static bool sincronizando;
static bool dnd_sobre_vista;
static char disp_guardada[160];
static bool gesto;
static obj_t gesto_antes;

enum { B_DESFAZER, B_REFAZER, B_ALINHAR, B_COMPROBAR, B_GUARDAR, B_RESTAURAR, B_IMAN, B_REJILLA };
enum { PS_TRANSF, PI_X, PI_Y, PI_L, PI_A, PI_ROT, PI_ESC, PS_APAR, PI_COR, PI_VIS, PI_CAPA, PI_OPAC, PS_DADOS, PI_NOME, PI_SCRIPT, PROPS_N };

static const char *const capas[3] = { N_("Fondo"), N_("Juego"), N_("Interfaz") };

static const char *const codigo_inicial =
    "-- Script del jugador\n"
    "local velocidad = 4\n"
    "\n"
    "function mover(dx, dy)\n"
    "  x = x + dx * velocidad\n"
    "  if x > 100 then\n"
    "    x = 100\n"
    "  end\n"
    "end\n";

/* ------------------------------------------------------------ geometría */

static void rect(int h, int *x, int *y, int *l, int *a)
{
    paineis_rect(&pn, h, x, y, l, a);
}

static void layout(void)
{
    paineis_mover(&pn, 0, BARRA_A, jn->larg, jn->alt - BARRA_A);
    int x, y, l, a;
    rect(H_ARBOL, &x, &y, &l, &a);
    arv.x = x; arv.y = y + TITULO_A; arv.l = l; arv.a = a > TITULO_A ? a - TITULO_A : 1;
    rect(H_VISTA, &x, &y, &l, &a);
    edvista_mover(&vs, x, y, l, a);
    rect(H_CODIGO, &x, &y, &l, &a);
    codigo_mover(cod, x, y, l, a);
    rect(H_INSPETOR, &x, &y, &l, &a);
    ins.x = x; ins.y = y + TITULO_A; ins.l = l; ins.a = a > TITULO_A ? a - TITULO_A : 1;
    ins.col_nome = l * 45 / 100 < 70 ? 70 : l * 45 / 100;
    rect(H_RECURSOS, &x, &y, &l, &a);
    pal.x = x; pal.y = y + TITULO_A; pal.l = l; pal.a = a > TITULO_A ? a - TITULO_A : 1;
    rect(H_CONSOLA, &x, &y, &l, &a);
    con.x = x; con.y = y; con.l = l; con.a = a;
}

static void log_posiciones(void)
{
    registrar("demoeditor: pos arbol %d %d %d %d\n", arv.x, arv.y, arv.l, arv.a);
    registrar("demoeditor: pos vista %d %d %d %d\n", vs.x, vs.y, vs.l, vs.a);
    int cx, cy, cl, ca;
    rect(H_CODIGO, &cx, &cy, &cl, &ca);
    registrar("demoeditor: pos codigo %d %d %d %d\n", cx, cy, cl, ca);
    registrar("demoeditor: pos inspetor %d %d %d %d\n", ins.x, ins.y, ins.l, ins.a);
    registrar("demoeditor: pos recursos %d %d %d %d\n", pal.x, pal.y, pal.l, pal.a);
    registrar("demoeditor: pos consola %d %d %d %d\n", con.x, con.y, con.l, con.a);
    for (int i = 0; i < pn.n; i++)
        if (pn.no[i].tipo != PD_FOLHA)
            registrar("demoeditor: pos sep%d %d %d %d %d plegado %d\n", i, pn.no[i].sx, pn.no[i].sy, pn.no[i].sl, pn.no[i].sh, pn.no[i].plegado);
    for (int k = 0; k < 5; k++) {
        int bx, by, bl, ba;
        consola_boton(&con, k, &bx, &by, &bl, &ba);
        registrar("demoeditor: pos cbtn%d %d %d %d %d\n", k, bx, by, bl, ba);
    }
    for (int i = 0; i < 8; i++) registrar("demoeditor: pos boton%d %d %d %d %d\n", i, btn[i].x, btn[i].y, btn[i].l, btn[i].a);
}

/* ------------------------------------------------------------ sincronizar escena, árbol, inspector y lienzo */

static void reconstruir_arbol(void)
{
    int n = 0;
    copia_texto(arv_nomes[n], _("Escena"), sizeof arv_nomes[n]);
    arv_niv[n] = 0; arv_obj[n] = -1; arv_abr[n] = 1; n++;
    for (int i = 0; i < nobj; i++) {
        if (!objs[i].viva) continue;
        copia_texto(arv_nomes[n], objs[i].nome, sizeof arv_nomes[n]);
        arv_niv[n] = 1; arv_obj[n] = i; arv_abr[n] = 0; n++;
    }
    for (int i = 0; i < n; i++) arv_txt[i] = arv_nomes[i];
    arv.n = n;
    arv.sel = -1;
    for (int i = 0; i < n; i++) if (arv_obj[i] == sel) arv.sel = i;
}

static void cargar_inspetor(void)
{
    sincronizando = true;
    obj_t *o = sel >= 0 ? &objs[sel] : NULL;
    int vals[16] = { 0 };
    if (o) { vals[PI_X] = o->x; vals[PI_Y] = o->y; vals[PI_L] = o->l; vals[PI_A] = o->a; vals[PI_ROT] = o->rot; }
    bool ab_t = props[PS_TRANSF].tipo != INS_SECAO || props[PS_TRANSF].aberta;
    bool ab_a = props[PS_APAR].tipo != INS_SECAO || props[PS_APAR].aberta;
    bool ab_d = props[PS_DADOS].tipo != INS_SECAO || props[PS_DADOS].aberta;
    inspetor_secao(&props[PS_TRANSF], N_("Transformación"), ab_t);
    inspetor_int(&props[PI_X], N_("Posición X"), vals[PI_X], -9999, 9999, 1);
    inspetor_int(&props[PI_Y], N_("Posición Y"), vals[PI_Y], -9999, 9999, 1);
    inspetor_int(&props[PI_L], N_("Ancho"), vals[PI_L], 1, 9999, 1);
    inspetor_int(&props[PI_A], N_("Alto"), vals[PI_A], 1, 9999, 1);
    inspetor_int(&props[PI_ROT], N_("Giro"), vals[PI_ROT], 0, 359, 5);
    inspetor_fixo(&props[PI_ESC], N_("Escala"), o ? o->escala : 100, 2, 10, 1000, 10);
    inspetor_secao(&props[PS_APAR], N_("Apariencia"), ab_a);
    inspetor_cor(&props[PI_COR], N_("Color"), o ? o->cor : 0xFF808080, true);
    inspetor_bool(&props[PI_VIS], N_("Visible"), o ? o->visivel : 1);
    inspetor_enum(&props[PI_CAPA], N_("Capa"), o ? o->capa : 1, capas, 3);
    inspetor_deslizador(&props[PI_OPAC], N_("Opacidad"), o ? o->opac : 100, 0, 100);
    inspetor_secao(&props[PS_DADOS], N_("Datos"), ab_d);
    inspetor_texto(&props[PI_NOME], N_("Nombre"), o ? o->nome : "");
    inspetor_arquivo(&props[PI_SCRIPT], N_("Script"), o ? o->script : "", "Lua (*.lua)|*.lua|Todos|*");
    for (int i = 0; i < PROPS_N; i++) if (!o && props[i].tipo != INS_SECAO) props[i].desab = true;
    sincronizando = false;
}

static void sincronizar_tudo(void)
{
    if (sel >= 0 && !objs[sel].viva) sel = -1;
    reconstruir_arbol();
    cargar_inspetor();
    if (sel >= 0) edvista_selecionar(&vs, objs[sel].x, objs[sel].y, objs[sel].l, objs[sel].a, objs[sel].rot);
    else edvista_selecionar(&vs, 0, 0, 0, 0, 0);
}

static void registrar_sel(void)
{
    if (sel >= 0) registrar("demoeditor: sel %s %d %d %d %d rot %d\n", objs[sel].nome, objs[sel].x, objs[sel].y, objs[sel].l, objs[sel].a, objs[sel].rot);
    else registrar("demoeditor: sel nada\n");
}

static void aviso_con(int nivel, const char *fmt, ...)
{
    char t[CONSOLA_TXT];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    consola_escrever(&con, nivel, t);
}

/* ------------------------------------------------------------ deshacer */

typedef struct { int idx; obj_t antes, depois; } cmd_obj_t;
typedef struct { int idx; } cmd_nuevo_t;

static void ap_obj(void *d, bool refazer)
{
    cmd_obj_t *c = d;
    objs[c->idx] = refazer ? c->depois : c->antes;
}

static void ap_nuevo(void *d, bool refazer)
{
    cmd_nuevo_t *c = d;
    objs[c->idx].viva = refazer ? 1 : 0;
}

static void registrar_cambio(int idx, const obj_t *antes, const obj_t *depois, const char *nome)
{
    if (memcmp(antes, depois, sizeof *antes) == 0) return;
    cmd_obj_t *c = aloca(sizeof *c);
    if (!c) return;
    c->idx = idx; c->antes = *antes; c->depois = *depois;
    ed_desfazer_registrar(&pilha, nome, ap_obj, libera, c);
}

static void actualizar_botones(void)
{
    btn[B_DESFAZER].desabilitado = !ed_desfazer_pode(&pilha);
    btn[B_REFAZER].desabilitado = !ed_refazer_pode(&pilha);
}

static void log_pilha(const char *que)
{
    const char *n = ed_desfazer_nome(&pilha);
    int soma = 0, vivos = 0;
    for (int i = 0; i < nobj; i++)
        if (objs[i].viva) { vivos++; soma += objs[i].x * 3 + objs[i].y * 5 + objs[i].l * 7 + objs[i].a * 11 + objs[i].rot * 13; }
    registrar("demoeditor: %s deshacer %d rehacer %d (%s) objetos %d suma %d\n", que, ed_desfazer_pode(&pilha), ed_refazer_pode(&pilha), n ? n : "-", vivos, soma);
}

/* ------------------------------------------------------------ dibujo */

static void titulo_panel(int h, const char *t)
{
    int x, y, l, a;
    rect(h, &x, &y, &l, &a);
    if (l <= 0 || a <= 0) return;
    g_retangulo(jn, x, y, l, TITULO_A, mod_tema.barra);
    g_retangulo(jn, x, y + TITULO_A - 1, l, 1, mod_tema.borde);
    g_texto(jn, x + 6, y + 1, idioma_traducir_sl(t), mod_tema.texto_fraco);
}

static void redibujar(void)
{
    g_retangulo(jn, 0, 0, jn->larg, jn->alt, COR_FUNDO);
    g_retangulo(jn, 0, 0, jn->larg, BARRA_A, mod_tema.barra);
    g_retangulo(jn, 0, BARRA_A - 1, jn->larg, 1, mod_tema.borde);
    btn[B_IMAN].ativo = vs.iman;
    btn[B_REJILLA].ativo = vs.grade > 0;
    actualizar_botones();
    for (int i = 0; i < 8; i++) mbotao_desenhar(jn, &btn[i]);
    paineis_desenhar(jn, &pn);
    titulo_panel(H_ARBOL, N_("Escena"));
    titulo_panel(H_INSPETOR, N_("Inspector"));
    titulo_panel(H_RECURSOS, N_("Recursos"));
    int x, y, l, a;
    if (paineis_rect(&pn, H_ARBOL, &x, &y, &l, &a)) arvore_desenhar(jn, &arv);
    if (paineis_rect(&pn, H_VISTA, &x, &y, &l, &a)) edvista_desenhar(jn, &vs);
    if (paineis_rect(&pn, H_CODIGO, &x, &y, &l, &a)) codigo_desenhar(jn, cod);
    if (paineis_rect(&pn, H_INSPETOR, &x, &y, &l, &a)) inspetor_desenhar(jn, &ins);
    if (paineis_rect(&pn, H_RECURSOS, &x, &y, &l, &a)) edpaleta_desenhar(jn, &pal);
    if (paineis_rect(&pn, H_CONSOLA, &x, &y, &l, &a)) consola_desenhar(jn, &con);
    if (dnd_sobre_vista) g_moldura(jn, vs.x + 1, vs.y + 1, vs.l - 2, vs.a - 2, mod_tema.acento);
    jan_mostrar(jn);
}

/* El lienzo: los objetos de la escena. */
static int obj_em(int wx, int wy)
{
    for (int i = nobj - 1; i >= 0; i--) {
        obj_t *o = &objs[i];
        if (!o->viva || !o->visivel) continue;
        int dx = 2 * wx - (2 * o->x + o->l), dy = 2 * wy - (2 * o->y + o->a);
        int s = edvista_seno(o->rot), c = edvista_seno(o->rot + 90);
        int u = (dx * c + dy * s) / 1000, w = (-dx * s + dy * c) / 1000;
        if (u >= -o->l && u <= o->l && w >= -o->a && w <= o->a) return i;
    }
    return -1;
}

static void desenhar_obj(jan_t *j, const edvista_t *v, const obj_t *o)
{
    uint32_t cor = o->cor & 0xFFFFFF;
    unsigned alfa = (((o->cor >> 24) & 255) * (unsigned)o->opac / 100);
    uint32_t fundo = mod_mezclar(mod_tema.pista, mod_tema.superficie, 180);
    uint32_t fill = mod_mezclar(fundo, cor, alfa);
    if (o->rot == 0) {
        edvista_ret(j, v, o->x, o->y, o->l, o->a, fill);
        edvista_moldura(j, v, o->x, o->y, o->l, o->a, mod_mezclar(cor, 0x000000, 90));
    } else {   /* girado: relleno con líneas paralelas y contorno */
        int cx2 = 2 * o->x + o->l, cy2 = 2 * o->y + o->a;
        int s = edvista_seno(o->rot), c = edvista_seno(o->rot + 90);
        int paso = 2000 / v->zoom;
        if (paso < 1) paso = 1;
        for (int u = -o->l; u <= o->l; u += paso) {
            int x0 = cx2 + (u * c + o->a * s) / 1000, y0 = cy2 + (u * s - o->a * c) / 1000;
            int x1 = cx2 + (u * c - o->a * s) / 1000, y1 = cy2 + (u * s + o->a * c) / 1000;
            edvista_linha(j, v, x0 / 2, y0 / 2, x1 / 2, y1 / 2, fill);
        }
        int px[4], py[4];
        static const signed char q[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
        for (int k = 0; k < 4; k++) {
            int u = q[k][0] * o->l, w = q[k][1] * o->a;
            px[k] = (cx2 + (u * c - w * s) / 1000) / 2;
            py[k] = (cy2 + (u * s + w * c) / 1000) / 2;
        }
        for (int k = 0; k < 4; k++) edvista_linha(j, v, px[k], py[k], px[(k + 1) % 4], py[(k + 1) % 4], mod_mezclar(cor, 0x000000, 90));
    }
    int sx, sy;
    edvista_para_tela(v, o->x, o->y, &sx, &sy);
    edvista_texto(j, v, o->x, o->y - 18000 / v->zoom, o->nome, mod_tema.texto_fraco);
    (void)sx; (void)sy;
}

static void edvista_desenhar_cb(void *ctx, jan_t *j, edvista_t *v)
{
    (void)ctx;
    for (int i = 0; i < nobj; i++) if (objs[i].viva && objs[i].visivel && objs[i].capa == 0) desenhar_obj(j, v, &objs[i]);
    for (int i = 0; i < nobj; i++) if (objs[i].viva && objs[i].visivel && objs[i].capa == 1) desenhar_obj(j, v, &objs[i]);
    for (int i = 0; i < nobj; i++) if (objs[i].viva && objs[i].visivel && objs[i].capa == 2) desenhar_obj(j, v, &objs[i]);
}

static int edvista_acertar_cb(void *ctx, int wx, int wy) { (void)ctx; return obj_em(wx, wy); }

static void fim_gesto(void)
{
    if (!gesto) return;
    gesto = false;
    if (sel >= 0) registrar_cambio(sel, &gesto_antes, &objs[sel], N_("Transformar objeto"));
}

static void edvista_aviso_cb(void *ctx, int ev, int a, int b, int c, int d, bool fim)
{
    (void)ctx;
    switch (ev) {
    case EDVISTA_E_CLIQUE:
        sel = a;
        if (a >= 0) edvista_selecionar(&vs, objs[a].x, objs[a].y, objs[a].l, objs[a].a, objs[a].rot);
        else edvista_selecionar(&vs, 0, 0, 0, 0, 0);
        sincronizar_tudo();
        registrar("demoeditor: clic mundo %d %d\n", b, c);
        registrar_sel();
        break;
    case EDVISTA_E_RET: {
        int n = 0, primero = -1;
        for (int i = 0; i < nobj; i++) {
            obj_t *o = &objs[i];
            if (!o->viva || !o->visivel) continue;
            if (o->x < c && o->x + o->l > a && o->y < d && o->y + o->a > b) { n++; if (primero < 0) primero = i; }
        }
        sel = primero;
        sincronizar_tudo();
        registrar("demoeditor: rectangulo %d %d %d %d toca %d\n", a, b, c, d, n);
        registrar_sel();
        break;
    }
    case EDVISTA_E_MOVER: case EDVISTA_E_ESCALAR: case EDVISTA_E_GIRAR:
        if (sel < 0) break;
        if (!gesto && !fim) { gesto = true; gesto_antes = objs[sel]; }
        if (ev == EDVISTA_E_GIRAR) objs[sel].rot = a;
        else { objs[sel].x = a; objs[sel].y = b; objs[sel].l = c; objs[sel].a = d; }
        if (fim) {
            fim_gesto();
            sincronizar_tudo();
            registrar("demoeditor: %s %s %d %d %d %d rot %d\n", ev == EDVISTA_E_MOVER ? "movido" : ev == EDVISTA_E_ESCALAR ? "escalado" : "girado",
                      objs[sel].nome, objs[sel].x, objs[sel].y, objs[sel].l, objs[sel].a, objs[sel].rot);
            aviso_con(CON_MSG, "%s: %s (%d, %d) %dx%d", ev == EDVISTA_E_MOVER ? _("Movido") : ev == EDVISTA_E_ESCALAR ? _("Escalado") : _("Girado"),
                      objs[sel].nome, objs[sel].x, objs[sel].y, objs[sel].l, objs[sel].a);
        } else {
            cargar_inspetor();
        }
        break;
    case EDVISTA_E_ZOOM:
        registrar("demoeditor: zoom %d ox %d oy %d\n", vs.zoom, vs.ox, vs.oy);
        break;
    case EDVISTA_E_DESLOCAR:
        if (fim) registrar("demoeditor: desplazado ox %d oy %d zoom %d\n", vs.ox, vs.oy, vs.zoom);
        break;
    }
}

/* ------------------------------------------------------------ inspector */

static void ins_mudou(void *ctx, inspetor_t *s, int i)
{
    (void)ctx; (void)s;
    if (sincronizando || sel < 0) return;
    obj_t antes = objs[sel], *o = &objs[sel];
    switch (i) {
    case PI_X: o->x = props[i].valor; break;
    case PI_Y: o->y = props[i].valor; break;
    case PI_L: o->l = props[i].valor; break;
    case PI_A: o->a = props[i].valor; break;
    case PI_ROT: o->rot = props[i].valor; break;
    case PI_ESC: o->escala = props[i].valor; break;
    case PI_COR: o->cor = (uint32_t)props[i].valor; break;
    case PI_VIS: o->visivel = props[i].valor; break;
    case PI_CAPA: o->capa = props[i].valor; break;
    case PI_OPAC: o->opac = props[i].valor; break;
    case PI_NOME: copia_texto(o->nome, props[i].texto, sizeof o->nome); break;
    case PI_SCRIPT: copia_texto(o->script, props[i].texto, sizeof o->script); break;
    default: return;
    }
    registrar_cambio(sel, &antes, o, N_("Cambiar propiedad"));
    char v[96];
    inspetor_valor_texto(&props[i], v, sizeof v);
    registrar("demoeditor: prop %s = %s\n", props[i].nome, v);
    if (i == PI_COR) registrar("demoeditor: cor %08x\n", (unsigned)props[i].valor);
    edvista_selecionar(&vs, o->x, o->y, o->l, o->a, o->rot);
    if (i == PI_NOME) reconstruir_arbol();
}

/* ------------------------------------------------------------ arrastrar y soltar */

static void criar_objeto(const char *nome, int x, int y, int l, int a, uint32_t cor)
{
    if (nobj >= MAXOBJ) return;
    obj_novo(nome, x, y, l, a, cor);
    cmd_nuevo_t *c = aloca(sizeof *c);
    if (!c) return;
    c->idx = nobj - 1;
    ed_desfazer_registrar(&pilha, N_("Crear objeto"), ap_nuevo, libera, c);
}

static bool alvo_vista(void *ctx, int fase, const arrsol_item_t *it, int x, int y)
{
    (void)ctx;
    if (fase == ARRSOL_SAI) { dnd_sobre_vista = false; registrar("demoeditor: dnd sai vista\n"); redibujar(); return false; }
    if (fase == ARRSOL_ENTRA) { dnd_sobre_vista = true; registrar("demoeditor: dnd entra vista tipo %d\n", it->tipo); redibujar(); return true; }
    if (fase == ARRSOL_MOVE) return true;
    /* soltar */
    dnd_sobre_vista = false;
    int wx, wy;
    edvista_para_mundo(&vs, x, y, &wx, &wy);
    wx = edvista_iman(&vs, wx); wy = edvista_iman(&vs, wy);
    if (it->tipo == TIPO_OBJ) {
        int idx = it->dados[0];
        if (idx >= 0 && idx < nobj && objs[idx].viva) {
            obj_t antes = objs[idx];
            objs[idx].x = wx; objs[idx].y = wy;
            registrar_cambio(idx, &antes, &objs[idx], N_("Soltar objeto"));
            sel = idx;
            registrar("demoeditor: dnd soltar objeto %s en %d %d\n", objs[idx].nome, wx, wy);
            aviso_con(CON_MSG, "%s: %s (%d, %d)", _("Objeto soltado"), objs[idx].nome, wx, wy);
        }
    } else if (it->tipo == TIPO_CASILLA) {
        int c0 = it->dados[0], f0 = it->dados[1], c1 = it->dados[2], f1 = it->dados[3], n = 0;
        ed_desfazer_agrupar_inicio(&pilha, N_("Pintar casillas"));
        for (int f = f0; f <= f1; f++)
            for (int c = c0; c <= c1; c++) {
                char nom[32];
                formatar_texto(nom, sizeof nom, "Casilla %d,%d", c, f);
                criar_objeto(nom, wx + (c - c0) * 32, wy + (f - f0) * 32, 32, 32, 0xFF000000u | (uint32_t)(0x40 + c * 24) << 16 | (uint32_t)(0x60 + f * 40) << 8 | 0xA0);
                n++;
            }
        ed_desfazer_agrupar_fim(&pilha);
        sel = nobj - 1;
        registrar("demoeditor: dnd soltar casillas %d en %d %d\n", n, wx, wy);
        aviso_con(CON_AVISO, "%d %s", n, _("casillas pintadas"));
    }
    sincronizar_tudo();
    return true;
}

static bool alvo_consola(void *ctx, int fase, const arrsol_item_t *it, int x, int y)
{
    (void)ctx; (void)x; (void)y;
    if (it->tipo != TIPO_OBJ) return false;
    if (fase == ARRSOL_ENTRA) registrar("demoeditor: dnd entra consola\n");
    if (fase == ARRSOL_SOLTA) { registrar("demoeditor: dnd soltar consola %s\n", objs[it->dados[0]].nome); aviso_con(CON_AVISO, "%s %s", objs[it->dados[0]].nome, _("no se puede soltar aquí")); return true; }
    return true;
}

static void registrar_alvos(void)
{
    arrsol_alvos_limpar();
    arrsol_alvo(vs.x, vs.y, vs.l, vs.a, alvo_vista, NULL);
    arrsol_alvo(con.x, con.y, con.l, con.a, alvo_consola, NULL);
}

/* ------------------------------------------------------------ comprobar el código Lua (bloques) */

static bool comprobar_lua(const char *txt, int n, int *linha, char *msg, int max)
{
    struct { int linha; const char *que; } pila[64];
    int np = 0, st = 0, lin = 1, i = 0;
    while (i <= n) {
        int ini = i;
        while (i < n && txt[i] != '\n') i++;
        int ln = i - ini;
        unsigned char cls[512];
        int m = ln < 512 ? ln : 512;
        int st2 = edrealce_linha(&edrealce_lua, txt + ini, m, st, cls);
        for (int k = 0; k < m;) {
            if (cls[k] != RC_CHAVE) { k++; continue; }
            int e = k;
            while (e < m && cls[e] == RC_CHAVE) e++;
            int w = e - k;
            const char *p = txt + ini + k;
            if ((w == 8 && !memcmp(p, "function", 8)) || (w == 2 && !memcmp(p, "if", 2) && 1) || (w == 2 && !memcmp(p, "do", 2)) || (w == 6 && !memcmp(p, "repeat", 6))) {
                if (np < 64) { pila[np].linha = lin; pila[np].que = w == 8 ? "function" : w == 6 ? "repeat" : w == 2 && p[0] == 'i' ? "if" : "do"; np++; }
            } else if (w == 3 && !memcmp(p, "end", 3)) {
                if (np == 0) { *linha = lin; formatar_texto(msg, (size_t)max, "'end' sin abrir"); return false; }
                np--;
            } else if (w == 5 && !memcmp(p, "until", 5)) {
                if (np == 0) { *linha = lin; formatar_texto(msg, (size_t)max, "'until' sin abrir"); return false; }
                np--;
            }
            k = e;
        }
        st = st2;
        lin++;
        i++;
    }
    if (np > 0) { *linha = pila[np - 1].linha; formatar_texto(msg, (size_t)max, "falta 'end' para '%s'", pila[np - 1].que); return false; }
    return true;
}

static void comprobar_codigo(void)
{
    int n, linha = 0;
    char msg[80];
    const char *t = codigo_texto(cod, &n);
    if (comprobar_lua(t, n, &linha, msg, sizeof msg)) {
        codigo_marcar_erro(cod, 0, "");
        aviso_con(CON_MSG, "%s", _("Código correcto"));
        registrar("demoeditor: comprobar ok\n");
    } else {
        codigo_marcar_erro(cod, linha, msg);
        char t2[CONSOLA_TXT];
        formatar_texto(t2, sizeof t2, "script.lua:%d: %s", linha, msg);
        consola_escrever_linha(&con, CON_ERRO, linha, t2);
        registrar("demoeditor: comprobar error linha %d: %s\n", linha, msg);
    }
}

/* ------------------------------------------------------------ otras acciones */

static void alinhar_todo(void)
{
    ed_desfazer_agrupar_inicio(&pilha, N_("Alinear todo"));
    int n = 0;
    for (int i = 0; i < nobj; i++) {
        if (!objs[i].viva) continue;
        obj_t antes = objs[i];
        objs[i].x = (objs[i].x + 8) / 16 * 16 - (objs[i].x + 8 < 0 && (objs[i].x + 8) % 16 ? 16 : 0);
        objs[i].y = (objs[i].y + 8) / 16 * 16 - (objs[i].y + 8 < 0 && (objs[i].y + 8) % 16 ? 16 : 0);
        if (memcmp(&antes, &objs[i], sizeof antes)) n++;
        registrar_cambio(i, &antes, &objs[i], N_("Alinear todo"));
    }
    ed_desfazer_agrupar_fim(&pilha);
    aviso_con(CON_MSG, "%s: %d", _("Objetos alineados"), n);
    registrar("demoeditor: alinear %d\n", n);
    sincronizar_tudo();
}

static void gerar_hoja(void)
{
    hoja.larg = 128; hoja.alt = 64;
    hoja.px = aloca_zerado(128 * 64 * 4);
    if (!hoja.px) return;
    for (int f = 0; f < 4; f++)
        for (int c = 0; c < 8; c++) {
            int h = (c + f * 8) * 360 / 32, r, g, b;
            cor_hsv_para_rgb(h, 170, 230, &r, &g, &b);
            uint32_t base = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) {
                    uint32_t px = base;
                    if (x == 0 || y == 0 || x == 15 || y == 15) px = mod_mezclar(base, 0x000000, 90);
                    else if (((x / 4) + (y / 4) + c + f) & 1) px = mod_mezclar(base, 0xFFFFFF, 40);
                    hoja.px[(f * 16 + y) * 128 + c * 16 + x] = px;
                }
        }
}

static void usar_foco(int f)
{
    foco = f;
    codigo_foco(cod, f == F_CODIGO);
}

static int foco_em(int x, int y)
{
    struct { int h, f; } t[] = { { H_ARBOL, F_ARBOL }, { H_VISTA, F_VISTA }, { H_CODIGO, F_CODIGO }, { H_INSPETOR, F_INSPETOR }, { H_RECURSOS, F_RECURSOS }, { H_CONSOLA, F_CONSOLA } };
    for (int i = 0; i < 6; i++) {
        int rx, ry, rl, ra;
        if (paineis_rect(&pn, t[i].h, &rx, &ry, &rl, &ra) && x >= rx && y >= ry && x < rx + rl && y < ry + ra) return t[i].f;
    }
    return F_NADA;
}

static void dibujar_resaltado_log(void)
{
    /* clases del resaltado de la línea del cursor, para las pruebas */
    int n, k = codigo_linha(cod) - 1;
    const char *t = codigo_texto(cod, &n);
    int ini = 0, lin = 0, st = 0;
    unsigned char cls[512];
    char cadena[520];
    for (int i = 0; i <= n; i++) {
        if (i == n || t[i] == '\n') {
            int m = i - ini < 512 ? i - ini : 512;
            int st2 = edrealce_linha(&edrealce_lua, t + ini, m, st, cls);
            if (lin == k) {
                for (int q = 0; q < m; q++) cadena[q] = (char)('0' + cls[q]);
                cadena[m] = 0;
                registrar("demoeditor: realce %d %s\n", lin + 1, cadena);
                break;
            }
            st = st2;
            lin++;
            ini = i + 1;
        }
    }
}

static void log_codigo(void)
{
    int n;
    codigo_texto(cod, &n);
    registrar("demoeditor: codigo lin %d col %d lineas %d bytes %d\n", codigo_linha(cod), codigo_coluna(cod), codigo_linhas(cod), n);
    dibujar_resaltado_log();
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jn = jan_abrir(_("Demo del editor"), VENT_L, VENT_A);
    if (!jn) return 1;
    jan_estilo(jn, JAN_REDIMENSIONAVEL, 760, 520);
    ed_desfazer_iniciar(&pilha, 200);
    obj_novo("Jugador", 32, 32, 48, 64, 0xFF3A9AD9);
    obj_novo("Enemigo", 160, 80, 40, 40, 0xFFD94A3A);
    obj_novo("Fondo", -64, -48, 96, 32, 0xFF7DBE5A);
    obj_novo("Moneda", 112, -16, 24, 24, 0xFFE8B923);
    objs[2].capa = 0;
    gerar_hoja();
    cod = codigo_criar(0, 0, 100, 100);
    if (!cod) return 1;
    codigo_regras(cod, &edrealce_lua);
    codigo_definir(cod, codigo_inicial, -1);
    consola_iniciar(&con, 0, 0, 100, 100);
    consola_escrever(&con, CON_MSG, _("Escena cargada: 4 objetos"));
    consola_escrever(&con, CON_AVISO, _("El script del jugador no se ha comprobado"));
    consola_escrever(&con, CON_ERRO, _("Ejemplo de error de la consola"));

    /* paneles */
    paineis_iniciar(&pn, 0, BARRA_A, VENT_L, VENT_A - BARRA_A);
    int n_arbol = paineis_folha(&pn, H_ARBOL), n_vista = paineis_folha(&pn, H_VISTA), n_cod = paineis_folha(&pn, H_CODIGO);
    int n_ins = paineis_folha(&pn, H_INSPETOR), n_rec = paineis_folha(&pn, H_RECURSOS), n_con = paineis_folha(&pn, H_CONSOLA);
    int centro = paineis_dividir(&pn, PD_PILHA, n_vista, n_cod, 560, 100, 80, 0);
    int medio = paineis_dividir(&pn, PD_LADO, centro, n_ins, 700, 200, 160, 2);
    int alto = paineis_dividir(&pn, PD_LADO, n_arbol, medio, 180, 100, 300, 1);
    int bajo = paineis_dividir(&pn, PD_LADO, n_rec, n_con, 350, 120, 150, 0);
    paineis_dividir(&pn, PD_PILHA, alto, bajo, 710, 200, 80, 2);

    /* componentes */
    memset(&arv, 0, sizeof arv);
    arv.textos = arv_txt; arv.nivel = arv_niv; arv.aberto = arv_abr; arv.sel = -1;
    edvista_iniciar(&vs, 0, 0, 100, 100);
    vs.ox = 90; vs.oy = 110;
    vs.ctx = NULL; vs.desenhar = edvista_desenhar_cb; vs.acertar = edvista_acertar_cb; vs.aviso = edvista_aviso_cb;
    vs.iman = true;
    inspetor_iniciar(&ins, 0, 0, 100, 100, props, PROPS_N);
    ins.mudou = ins_mudou;
    memset(props, 0, sizeof props);
    props[PS_TRANSF].aberta = props[PS_APAR].aberta = props[PS_DADOS].aberta = true;
    edpaleta_iniciar(&pal, 0, 0, 100, 100, &hoja, 16, 16);
    edpaleta_definir_selecao(&pal, 0, 0, 0, 0);

    /* barra de botones */
    static const char *const rot[8] = { N_("Deshacer"), N_("Rehacer"), N_("Alinear todo"), N_("Comprobar código"), N_("Guardar disposición"), N_("Restaurar"), N_("Imán"), N_("Rejilla") };
    int bx = 6;
    for (int i = 0; i < 8; i++) {
        memset(&btn[i], 0, sizeof btn[i]);
        btn[i].texto = rot[i];
        btn[i].variante = i >= 6 ? MB_SECUNDARIO : i == 3 ? MB_PRIMARIO : MB_SECUNDARIO;
        btn[i].fundo = mod_tema.barra;
        btn[i].y = 4; btn[i].a = 28;
        mbotao_ajustar(&btn[i]);
        btn[i].x = bx;
        bx += btn[i].l + 6;
    }

    layout();
    sincronizar_tudo();
    reconstruir_arbol();
    redibujar();
    log_posiciones();
    log_codigo();
    registrar("demoeditor: pronto\n");

    evento_t e;
    for (;;) {
        if (proximo_evento(jn->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(jn, &e)) { layout(); redibujar(); log_posiciones(); continue; }
        if (e.tipo == EV_TEMA) { redibujar(); continue; }

        /* arrastrar y soltar: se queda con los eventos mientras dura */
        registrar_alvos();
        int d = arrsol_tratar(jn, &e);
        if (d != ARRSOL_NADA) {
            if (d == ARRSOL_FIM || d == ARRSOL_CANCELOU) {
                dnd_sobre_vista = false;
                if (d == ARRSOL_CANCELOU) registrar("demoeditor: dnd cancelado\n");
                actualizar_botones();
                log_pilha("dnd");
                redibujar();
            }
            continue;
        }

        bool cambio_global = false;
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            usar_foco(foco_em(e.x, e.y));
            /* ¿empieza un arrastre? (una fila de objeto del árbol, o una casilla ya elegida) */
            if (foco == F_ARBOL) {
                int fila = (e.y - arv.y - 2) / 20 + arv.topo;
                int visibles = 0, item = -1;
                for (int i = 0; i < arv.n; i++) if (arvore_visivel(&arv, i)) { if (visibles == fila) { item = i; break; } visibles++; }
                if (item >= 0 && arv_obj[item] >= 0 && e.x > arv.x + 22) {
                    arrsol_item_t it;
                    memset(&it, 0, sizeof it);
                    it.tipo = TIPO_OBJ; it.dados[0] = (unsigned char)arv_obj[item]; it.n = 1;
                    copia_texto(it.rotulo, arv_nomes[item], sizeof it.rotulo);
                    arrsol_preparar(&it, e.x, e.y);
                }
            } else if (foco == F_RECURSOS) {
                int cl = pal.cel_l * pal.escala, ca = pal.cel_a * pal.escala;
                int c = (e.x - pal.x + pal.cx) / cl, f = (e.y - pal.y + pal.cy) / ca;
                int c0, f0, c1, f1;
                edpaleta_selecao(&pal, &c0, &f0, &c1, &f1);
                if (c >= c0 && c <= c1 && f >= f0 && f <= f1 && !(e.modificadores & 1)) {
                    arrsol_item_t it;
                    memset(&it, 0, sizeof it);
                    it.tipo = TIPO_CASILLA; it.dados[0] = (unsigned char)c0; it.dados[1] = (unsigned char)f0; it.dados[2] = (unsigned char)c1; it.dados[3] = (unsigned char)f1; it.n = 4;
                    formatar_texto(it.rotulo, sizeof it.rotulo, "%dx%d %s", c1 - c0 + 1, f1 - f0 + 1, _("casillas"));
                    arrsol_preparar(&it, e.x, e.y);
                    continue;   /* no cambia la selección de la paleta */
                }
            }
        }

        /* paneles */
        int pr = paineis_tratar(jn, &pn, &e);
        if (pr == PAINEIS_ARRASTA || pr == PAINEIS_PLEGOU) {
            layout();
            redibujar();
            if (pr == PAINEIS_PLEGOU) log_posiciones();
            continue;
        }
        if (pr == PAINEIS_SOLTOU) {
            layout(); redibujar();
            char b[160];
            paineis_guardar(&pn, b, sizeof b);
            registrar("demoeditor: separadores %s\n", b);
            log_posiciones();
            continue;
        }
        if (e.tipo == EV_MOVER && pn.arrasto >= 0) continue;

        /* barra de botones */
        for (int i = 0; i < 8; i++) {
            if (!mbotao_tratar(jn, &btn[i], &e)) continue;
            if (btn[i].desabilitado) continue;
            switch (i) {
            case B_DESFAZER: if (ed_desfazer(&pilha)) { sincronizar_tudo(); log_pilha("desfazer"); aviso_con(CON_MSG, "%s", _("Deshecho")); } break;
            case B_REFAZER: if (ed_refazer(&pilha)) { sincronizar_tudo(); log_pilha("refazer"); aviso_con(CON_MSG, "%s", _("Rehecho")); } break;
            case B_ALINHAR: alinhar_todo(); log_pilha("alinear"); break;
            case B_COMPROBAR: comprobar_codigo(); break;
            case B_GUARDAR: paineis_guardar(&pn, disp_guardada, sizeof disp_guardada); registrar("demoeditor: disposicion guardada %s\n", disp_guardada); aviso_con(CON_MSG, "%s", _("Disposición guardada")); break;
            case B_RESTAURAR:
                if (disp_guardada[0] && paineis_restaurar(&pn, disp_guardada)) { layout(); registrar("demoeditor: disposicion restaurada %s\n", disp_guardada); log_posiciones(); aviso_con(CON_MSG, "%s", _("Disposición restaurada")); }
                break;
            case B_IMAN: vs.iman = !vs.iman; registrar("demoeditor: iman %d\n", vs.iman); break;
            case B_REJILLA: vs.grade = vs.grade ? 0 : 16; registrar("demoeditor: rejilla %d\n", vs.grade); break;
            }
            cambio_global = true;
        }

        /* Ctrl+Z / Ctrl+Y fuera del código */
        if (e.tipo == EV_TECLA && foco != F_CODIGO && foco != F_INSPETOR && (e.tecla == 26 || e.tecla == 25)) {
            bool h = e.tecla == 26 ? ed_desfazer(&pilha) : ed_refazer(&pilha);
            if (h) { sincronizar_tudo(); log_pilha(e.tecla == 26 ? "desfazer" : "refazer"); cambio_global = true; }
        }

        /* componentes; el teclado, solo al que tiene el foco */
        bool tecla = e.tipo == EV_TECLA;
        int selant = sel;
        if (!tecla || foco == F_ARBOL) {
            int r = arvore_tratar(jn, &arv, &e);
            if (r == ARVORE_ESCOLHEU && arv.sel >= 0) {
                sel = arv_obj[arv.sel];
                sincronizar_tudo();
                registrar("demoeditor: arbol elegido %d\n", arv.sel);
                registrar_sel();
                cambio_global = true;
            }
        }
        if (!tecla || foco == F_VISTA) {
            edvista_tratar(jn, &vs, &e);
            if (sel != selant) cambio_global = true;
        }
        if (!tecla || foco == F_INSPETOR) {
            int r = inspetor_tratar(jn, &ins, &e);
            if (r == INSPETOR_MUDOU) cambio_global = true;
        }
        if (!tecla || foco == F_RECURSOS) {
            int r = edpaleta_tratar(jn, &pal, &e);
            if (r == EDPALETA_ESCOLHEU) {
                int c0, f0, c1, f1;
                edpaleta_selecao(&pal, &c0, &f0, &c1, &f1);
                registrar("demoeditor: casillas %d %d %d %d\n", c0, f0, c1, f1);
                aviso_con(CON_MSG, "%s: %dx%d", _("Casillas elegidas"), c1 - c0 + 1, f1 - f0 + 1);
                cambio_global = true;
            }
        }
        if (!tecla || foco == F_CONSOLA) {
            int r = consola_tratar(jn, &con, &e);
            if (r == CONSOLA_IR) { codigo_ir_linha(cod, con.ir_linha); registrar("demoeditor: ir a linea %d\n", con.ir_linha); usar_foco(F_CODIGO); cambio_global = true; }
            else if (r == CONSOLA_MUDOU) registrar("demoeditor: consola visibles %d msg %d aviso %d error %d mascara %d copiado %d\n", consola_visiveis(&con), con.cont[0], con.cont[1], con.cont[2], con.mascara, con.copiado);
        }
        if (!tecla || foco == F_CODIGO) {
            int r = codigo_tratar(jn, cod, &e);
            if (r == CODIGO_MUDOU) { log_codigo(); }
            else if (r == CODIGO_MOVEU && e.tipo == EV_TECLA) log_codigo();
        }
        if (e.tipo == EV_MENU) { /* los menús del inspector ya los trató inspetor_tratar */ }
        if (cambio_global) { layout(); redibujar(); }
    }
    codigo_liberar(cod);
    ed_desfazer_liberar(&pilha);
    jan_fechar(jn);
    return 0;
}
