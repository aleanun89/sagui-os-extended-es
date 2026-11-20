// SPDX-License-Identifier: GPL-3.0-only
/* principal.c - el Laboratorio de C (D414): editor con resaltado (el del Mico: documento.c, realce.c,
 * desenho.c y vista.c), botón «Ejecutar», errores con su línea (clic = ir a ella), comprobación de límites
 * (-b), la salida del programa en un panel y ejemplos para aprender.
 *
 * Compila con libtcc en memoria (compilador.c) y ejecuta lo compilado en un proceso hijo: un fallo o un
 * bucle infinito no se llevan el Laboratorio por delante (el botón «Detener» lo mata).
 *
 * vista.c es el editor del Mico y espera algunas variables y funciones suyas (abas, foco, sujo...):
 * aquí van en una versión mínima, para un solo documento. */
#include "mico.h"
#include "compilador.h"

#define CARPETA_EJEMPLOS "/apps/labc/ejemplos"
#define LIN_MAX   1000          /* líneas que guarda el panel de salida */
#define LIN_ANCHO 160
#define MAX_ERR   64
#define PANEL_A   180
#define BARRA_A   30
#define ESTADO_A  20
#define ABAS_A    20

/* ------------------------------------------------------------ lo que pide vista.c */
doc_t *abas[MAX_ABAS];
int    qtd_abas = 1, aba_atual;
int    tab_largura = 4;
bool   tab_espacos = true;
int    foco = FOCO_EDITOR;
int    busca_modo = BUSCA_FECHADA;
ret_t  r_editor;
unsigned sujo;
char   aviso[160];
static unsigned aviso_ate;

void avisar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(aviso, sizeof aviso, fmt, ap);
    va_end(ap);
    aviso_ate = tempo_ms() + 4000;
    sujo |= R_ESTADO;
}

int busca_na_linha(const char *s, int n, int *ini, int *fim, int max)
{
    (void)s; (void)n; (void)ini; (void)fim; (void)max;
    return 0;
}

/* ------------------------------------------------------------ estado */

static const char *const ejemplos_nombre[] = {
    "1. Hola mundo", "2. Bucles", "3. Arrays", "4. Punteros", "5. Estructuras", "6. Ficheros",
    "7. Entrada de teclado", "8. Una ventana (libsagui)", "9. Error de limites (-b)", "10. Errores de compilacion",
};
static const char *const ejemplos_fichero[] = {
    "01-hola.c", "02-bucles.c", "03-arrays.c", "04-punteros.c", "05-structs.c", "06-ficheros.c",
    "07-entrada.c", "08-ventana.c", "09-limites.c", "10-errores.c",
};
#define N_EJEMPLOS ((int)(sizeof ejemplos_fichero / sizeof ejemplos_fichero[0]))

static botao_t b_nuevo = { 6, 3, 56, 24, "Nuevo", false }, b_abrir = { 66, 3, 56, 24, "Abrir", false },
               b_guardar = { 126, 3, 70, 24, "Guardar", false },
               b_ejec = { 210, 3, 130, 24, "Ejecutar (F5)", false }, b_parar = { 344, 3, 80, 24, "Detener", false };
static casilla_t c_limites = { 438, 8, false };
static combo_t  cb_ejemplos;
static caixa_t  entrada;

static lab_error_t errs[MAX_ERR];
static int n_errs, err_sel = -1, err_topo;
static int tiempo_compilacion;

static char lin[LIN_MAX][LIN_ANCHO];
static int n_lin, col_lin, saida_topo;
static bool seguir = true;
static enum { PANEL_SAIDA, PANEL_ERRS } panel_vista = PANEL_SAIDA;

static int hijo = -1;
static unsigned hijo_ini;
static bool sair_pedido;
static int versao_marcas = -1;

static ret_t r_barra, r_abas_p, r_panel, r_entrada;
ret_t r_estado;

/* ------------------------------------------------------------ salida del programa */

static void saida_limpar(void)
{
    n_lin = 0; col_lin = 0; saida_topo = 0; seguir = true;
    memset(lin, 0, sizeof lin[0]);
}

static void saida_nueva_linea(void)
{
    registrar("labc: salida: %s", lin[n_lin]);
    if (n_lin + 1 >= LIN_MAX) {
        memmove(lin[0], lin[1], sizeof lin[0] * (LIN_MAX - 1));
        n_lin--;
    }
    n_lin++;
    lin[n_lin][0] = 0;
    col_lin = 0;
}

static void saida_texto(const char *s, int n)
{
    if (n_lin == 0 && col_lin == 0 && !lin[0][0]) n_lin = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\r') continue;
        if (c == '\n') { saida_nueva_linea(); continue; }
        if (c == '\t') {
            do { if (col_lin < LIN_ANCHO - 1) { lin[n_lin][col_lin++] = ' '; lin[n_lin][col_lin] = 0; } } while (col_lin % 8);
            continue;
        }
        if (c < 32) continue;
        if (col_lin >= LIN_ANCHO - 1) saida_nueva_linea();
        lin[n_lin][col_lin++] = (char)c;
        lin[n_lin][col_lin] = 0;
    }
    sujo |= R_PAINEL;
}

static void saida_aviso(const char *fmt, ...)
{
    char t[200];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (col_lin) saida_nueva_linea();
    saida_texto(t, (int)tam_texto(t));
    saida_nueva_linea();
}

/* ------------------------------------------------------------ geometría */

void layout(void)
{
    int W = j->larg, H = j->alt;
    int pa = PANEL_A;
    if (pa > H - BARRA_A - ESTADO_A - 120) pa = H - BARRA_A - ESTADO_A - 120;
    if (pa < 80) pa = 80;
    r_barra = (ret_t){ 0, 0, W, BARRA_A };
    r_editor = (ret_t){ 0, BARRA_A, W, H - BARRA_A - pa - ESTADO_A };
    r_abas_p = (ret_t){ 0, r_editor.y + r_editor.a, W, ABAS_A };
    r_panel = (ret_t){ 0, r_abas_p.y + ABAS_A, W, pa - ABAS_A };
    r_estado = (ret_t){ 0, H - ESTADO_A, W, ESTADO_A };
    r_entrada = (ret_t){ 0, r_panel.y + r_panel.a - 24, W, 24 };
    entrada.x = 86;
    entrada.y = r_entrada.y + 3;
    entrada.l = W - 100;
    cb_ejemplos.y = 4;
    cb_ejemplos.x = W - 250 > 640 ? W - 250 : 640;
    cb_ejemplos.l = W - cb_ejemplos.x - 6;
    if (cb_ejemplos.l < 120) cb_ejemplos.l = 120;
    vista_sujar_tudo();
    sujo |= R_TUDO;
}

/* ------------------------------------------------------------ dibujo */

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, j->larg, BARRA_A, 0xE4E8E1);
    g_retangulo(j, 0, BARRA_A - 1, j->larg, 1, 0xB9C0B6);
    botao_desenhar(j, &b_nuevo);
    botao_desenhar(j, &b_abrir);
    botao_desenhar(j, &b_guardar);
    botao_desenhar(j, &b_ejec);
    botao_desenhar(j, &b_parar);
    casilla_desenhar(j, &c_limites);
    rotulo(j, 458, 7, "Comprobar limites (-b)", 0x1E2328);
    rotulo(j, cb_ejemplos.x - 66, 7, "Ejemplos:", 0x1E2328);
    combo_desenhar(j, &cb_ejemplos);
    d_mostrar(r_barra);
}

static void desenhar_abas(void)
{
    ret_t r = r_abas_p;
    recorte = (ret_t){ 0, 0, j->larg, j->alt };
    d_ret(r.x, r.y, r.l, r.a, tema->aba);
    char t[40];
    formatar_texto(t, sizeof t, " Salida ");
    int w1 = (int)tam_texto(t) * LETRA_L + 8;
    d_ret(4, r.y + 1, w1, r.a - 1, panel_vista == PANEL_SAIDA ? tema->aba_ativa : tema->aba);
    d_texto(8, r.y + 2, t, (int)tam_texto(t), tema->aba_texto);
    int nerr = 0;
    for (int i = 0; i < n_errs; i++) if (!errs[i].aviso) nerr++;
    formatar_texto(t, sizeof t, " Errores (%d) ", n_errs);
    int w2 = (int)tam_texto(t) * LETRA_L + 8;
    d_ret(8 + w1, r.y + 1, w2, r.a - 1, panel_vista == PANEL_ERRS ? tema->aba_ativa : tema->aba);
    d_texto(12 + w1, r.y + 2, t, (int)tam_texto(t), nerr ? tema->erro : tema->aba_texto);
    d_ret(0, r.y + r.a - 1, r.l, 1, tema->barra_borda);
    d_mostrar(r);
}

static int lineas_panel(void)
{
    int a = r_panel.a - (panel_vista == PANEL_SAIDA ? 24 : 0);
    int v = a / LETRA_A;
    return v < 1 ? 1 : v;
}

static void desenhar_panel(void)
{
    ret_t r = r_panel;
    recorte = r;
    d_ret(r.x, r.y, r.l, r.a, tema->term_fundo);
    int visibles = lineas_panel();
    if (panel_vista == PANEL_SAIDA) {
        int total = n_lin + 1;
        if (seguir) saida_topo = total - visibles > 0 ? total - visibles : 0;
        if (saida_topo > total - 1) saida_topo = total - 1;
        if (saida_topo < 0) saida_topo = 0;
        for (int i = 0; i < visibles && saida_topo + i < total; i++) {
            const char *s = lin[saida_topo + i];
            d_texto(6, r.y + i * LETRA_A, s, (int)tam_texto(s), tema->term_texto);
        }
        recorte = r_entrada;
        d_ret(r_entrada.x, r_entrada.y, r_entrada.l, r_entrada.a, tema->painel);
        d_ret(0, r_entrada.y, r_entrada.l, 1, tema->barra_borda);
        d_texto(6, r_entrada.y + 5, "Entrada:", 8, tema->painel_texto);
        caixa_desenhar(j, &entrada);
    } else {
        if (n_errs == 0) {
            d_texto(6, r.y + 2, "Sin errores ni avisos.", 22, tema->term_texto);
        }
        for (int i = 0; i < visibles && err_topo + i < n_errs; i++) {
            int k = err_topo + i;
            int y = r.y + i * LETRA_A;
            if (k == err_sel) d_ret(0, y, r.l, LETRA_A, 0x2A4A3A);
            char t[LAB_MSG_MAX + 40];
            if (errs[k].linea) formatar_texto(t, sizeof t, "Linea %d: %s: %s", errs[k].linea, errs[k].aviso ? "aviso" : "error", errs[k].msg);
            else formatar_texto(t, sizeof t, "%s: %s", errs[k].aviso ? "aviso" : "error", errs[k].msg);
            d_texto_ate(6, y, t, r.l - 12, errs[k].aviso ? tema->aviso : tema->erro);
        }
    }
    d_mostrar(r);
}

static void desenhar_estado(void)
{
    ret_t r = r_estado;
    recorte = r;
    d_ret(r.x, r.y, r.l, r.a, tema->estado);
    doc_t *d = DOC;
    int l = doc_linha_de(d, d->cursor), c = d->cursor - doc_ini(d, l);
    char t[200];
    const char *nombre = d->caminho[0] ? doc_nome(d) : "(sin guardar)";
    formatar_texto(t, sizeof t, " %s%s   Lin %d, Col %d", nombre, doc_modificado(d) ? " *" : "", l + 1, c + 1);
    d_texto(4, r.y + 2, t, (int)tam_texto(t), tema->estado_texto);
    const char *msg = aviso[0] && (int)(tempo_ms() - aviso_ate) < 0 ? aviso : NULL;
    char u[120];
    if (!msg) {
        if (hijo >= 0) msg = "ejecutando...";
        else if (tiempo_compilacion >= 0) { formatar_texto(u, sizeof u, "compilado en %d ms", tiempo_compilacion); msg = u; }
    }
    if (msg) {
        int w = (int)tam_texto(msg) * LETRA_L;
        d_texto(r.l - w - 8, r.y + 2, msg, (int)tam_texto(msg), tema->estado_texto);
    }
    d_mostrar(r);
}

/* Marcas rojas en el margen de las líneas con error (se dibujan tras el editor). */
static void desenhar_marcas(void)
{
    doc_t *d = DOC;
    recorte = r_editor;
    for (int i = 0; i < n_errs; i++) {
        if (errs[i].linea <= 0 || errs[i].aviso) continue;
        int l = errs[i].linea - 1, y = r_editor.y + (l - d->topo) * LETRA_A;
        if (l < d->topo || y + LETRA_A > r_editor.y + r_editor.a) continue;
        d_ret(r_editor.x + 1, y + 2, 4, LETRA_A - 4, tema->erro);
    }
    d_mostrar(r_editor);
}

static void redesenhar(void)
{
    if (!sujo) return;
    unsigned s = sujo;
    sujo = 0;
    if (s & R_MENU) desenhar_barra();
    if (s & R_ABAS) desenhar_abas();
    if (s & R_EDITOR) { vista_desenhar(false); desenhar_marcas(); }
    if (s & R_PAINEL) desenhar_panel();
    if (s & R_ESTADO) desenhar_estado();
}

/* ------------------------------------------------------------ acciones */

static void ir_a_linea(int linea)
{
    doc_t *d = DOC;
    if (linea < 1) return;
    if (linea > d->nli) linea = d->nli;
    foco = FOCO_EDITOR;
    registrar("labc: ir a la linea %d", linea);
    vista_ir(doc_ini(d, linea - 1), false);
    vista_cursor_visivel();
    vista_sujar_tudo();
    sujo |= R_ESTADO;
}

static void limpiar_marcas(void)
{
    if (n_errs) { n_errs = 0; err_sel = -1; vista_sujar_tudo(); sujo |= R_ABAS | R_PAINEL; }
}

static void parar_hijo(void)
{
    if (hijo < 0) return;
    registrar("labc: detener programa %d", hijo);
    lab_detener(hijo);
}

static void cerrar_hijo(void)
{
    lab_cerrar();
    hijo = -1;
}

static void ejecutar(void)
{
    if (hijo >= 0) { avisar("ya hay un programa en marcha (Detener)"); return; }
    doc_t *d = DOC;
    const char *fuente = doc_texto(d);
    saida_limpar();
    err_sel = -1;
    int n = 0, ms = 0;
    int r = lab_compilar(fuente, c_limites.marcada, errs, MAX_ERR, &n, &ms);
    n_errs = n;
    tiempo_compilacion = ms;
    registrar("labc: compilar %s: %d mensajes, %d ms", r < 0 ? "con errores" : "bien", n, ms);
    for (int i = 0; i < n; i++) registrar("labc: %s linea %d: %s", errs[i].aviso ? "aviso" : "error", errs[i].linea, errs[i].msg);
    vista_sujar_tudo();
    sujo |= R_TUDO;
    if (r < 0) {
        panel_vista = PANEL_ERRS;
        err_topo = 0;
        for (int i = 0; i < n; i++) if (!errs[i].aviso) { err_sel = i; break; }
        if (err_sel >= 0 && errs[err_sel].linea) ir_a_linea(errs[err_sel].linea);
        saida_aviso("La compilacion ha fallado (%d mensajes). Mira la pestana Errores.", n);
        avisar("hay errores de compilacion");
        return;
    }
    panel_vista = n_errs ? PANEL_ERRS : PANEL_SAIDA;
    if (n_errs) saida_aviso("Compilado con %d avisos.", n_errs);
    saida_aviso("--- ejecutando%s ---", c_limites.marcada ? " (con -b)" : "");
    int pid = lab_lanzar();
    if (pid < 0) { saida_aviso("no se ha podido lanzar el programa"); return; }
    hijo = pid;
    hijo_ini = tempo_ms();
    entrada.texto[0] = 0;
    entrada.cursor = 0;
    registrar("labc: programa %d en marcha", pid);
}

static void guardar(bool como)
{
    doc_t *d = DOC;
    if (como || !d->caminho[0]) {
        char c[CAMINHO_MAX];
        copia_texto(c, "/home/", sizeof c);
        if (d->caminho[0]) copia_texto(c, d->caminho, sizeof c);
        if (!dialogo_guardar("Guardar programa", "Programa en C (*.c)|*.c|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) return;
        copia_texto(d->caminho, c, sizeof d->caminho);
    }
    int e = doc_salvar(d);
    if (e) { mensagem("Laboratorio de C", "No se ha podido guardar el archivo."); return; }
    avisar("guardado %s", doc_nome(d));
    sujo |= R_ESTADO;
}

static bool pode_descartar(void)
{
    if (!doc_modificado(DOC)) return true;
    return confirmar("Laboratorio de C", "Hay cambios sin guardar. Los descarto?");
}

static void cargar_texto(const char *s, int n, const char *camino)
{
    doc_t *d = DOC;
    doc_definir(d, s, n);
    d->cursor = d->ancora = -1;
    d->cursor = 0;
    d->topo = d->esq = 0;
    d->ling = LING_C;
    copia_texto(d->caminho, camino ? camino : "", sizeof d->caminho);
    d->salvo = d->feitos;
    limpiar_marcas();
    saida_limpar();
    tiempo_compilacion = -1;
    vista_doc_trocou();
    vista_sujar_tudo();
    sujo |= R_TUDO;
}

static const char plantilla[] =
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hola\\n\");\n    return 0;\n}\n";

static void nuevo(void)
{
    if (!pode_descartar()) return;
    cargar_texto(plantilla, (int)sizeof plantilla - 1, NULL);
}

static bool abrir_ruta(const char *c)
{
    int n, erro;
    bool u;
    char *s = ler_arquivo(c, &n, &erro, &u);
    if (!s) { mensagem("Laboratorio de C", "No se ha podido abrir el archivo."); return false; }
    cargar_texto(s, n, c);
    DOC->utf8 = u;
    libera(s);
    return true;
}

static void abrir_dialogo(void)
{
    if (!pode_descartar()) return;
    char c[CAMINHO_MAX];
    copia_texto(c, "/home/", sizeof c);
    if (!dialogo_abrir("Abrir programa", "Programa en C (*.c;*.h)|*.c;*.h|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) return;
    abrir_ruta(c);
}

static void abrir_ejemplo(int i)
{
    if (i < 0 || i >= N_EJEMPLOS) return;
    if (!pode_descartar()) return;
    char c[CAMINHO_MAX];
    formatar_texto(c, sizeof c, "%s/%s", CARPETA_EJEMPLOS, ejemplos_fichero[i]);
    int n, erro;
    bool u;
    char *s = ler_arquivo(c, &n, &erro, &u);
    if (!s) { mensagem("Laboratorio de C", "No se encuentra el ejemplo."); return; }
    cargar_texto(s, n, NULL);   /* sin ruta: guardar pide el nombre y no pisa el ejemplo */
    libera(s);
    avisar("ejemplo %s", ejemplos_nombre[i]);
}

/* ------------------------------------------------------------ eventos */

static void enviar_entrada(void)
{
    if (hijo < 0) return;
    char t[140];
    formatar_texto(t, sizeof t, "%s\n", entrada.texto);
    lab_enviar(t, (int)tam_texto(t));
    saida_texto(t, (int)tam_texto(t));
    caixa_definir(&entrada, "");
}

static void leer_salida(void)
{
    char b[512];
    for (int k = 0; k < 8; k++) {
        int n = lab_leer(b, sizeof b);
        if (n <= 0) break;
        saida_texto(b, n);
    }
}

static void vigilar_hijo(void)
{
    if (hijo < 0) return;
    leer_salida();
    int cod = 0;
    int st = lab_estado(hijo, &cod);
    if (st == 0) return;
    leer_salida();
    int dur = (int)(tempo_ms() - hijo_ini);
    registrar("labc: programa terminado st=%d cod=%d", st, cod);
    if (st == 1) saida_aviso("--- el programa ha terminado (codigo %d, %d ms) ---", cod, dur);
    else saida_aviso("--- el programa se ha parado (senal %d) ---", cod);
    cerrar_hijo();
    sujo |= R_ESTADO | R_PAINEL;
}

static bool en(ret_t r, int x, int y) { return dentro(x, y, r); }

static void clique_abas(int x)
{
    int w1 = 8 * 8 + 8;
    if (x >= 4 && x < 4 + w1) panel_vista = PANEL_SAIDA;
    else if (x >= 8 + w1) panel_vista = PANEL_ERRS;
    sujo |= R_ABAS | R_PAINEL;
}

static void tratar_teclado(unsigned t, unsigned m)
{
    bool ctrl = m & M_CTRL, shift = m & M_SHIFT;
    if (t == T_F5) { ejecutar(); return; }
    if (t == T_F6 || (t == TECLA_ESC && hijo >= 0)) { parar_hijo(); return; }
    if (ctrl) {
        switch (t) {
        case 14: nuevo(); return;
        case 15: abrir_dialogo(); return;
        case 19: guardar(shift); return;
        case 17: if (pode_descartar()) sair_pedido = true; return;
        case 26: vista_desfazer(shift); return;
        case 25: vista_desfazer(true); return;
        case 24: vista_cortar(); return;
        case 3:  vista_copiar(); return;
        case 22: vista_colar(); return;
        case 1:  vista_selecionar_tudo(); return;
        case 4:  vista_duplicar(); return;
        case '/': case '7': vista_comentar(); return;
        case ' ': vista_completar(); return;
        }
    }
    vista_tecla(t, m);
}

static void tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) { if (pode_descartar()) sair_pedido = true; return; }
    if (jan_redimensionar(j, e)) { layout(); return; }
    int pc = combo_tratar(j, &cb_ejemplos, e);
    if (pc == COMBO_SELECIONOU) { abrir_ejemplo(cb_ejemplos.sel); sujo |= R_MENU; return; }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU) {
        /* los botones cuentan el clic al soltar */
        bool p1 = botao_tratar(j, &b_nuevo, e), p2 = botao_tratar(j, &b_abrir, e), p3 = botao_tratar(j, &b_guardar, e),
             p4 = botao_tratar(j, &b_ejec, e), p5 = botao_tratar(j, &b_parar, e);
        if (p1) nuevo();
        else if (p2) abrir_dialogo();
        else if (p3) guardar(false);
        else if (p4) ejecutar();
        else if (p5) parar_hijo();
        if (p1 || p2 || p3 || p4 || p5) { sujo |= R_MENU; return; }
    }
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla, m = e->modificadores;
        if (foco == FOCO_TERMINAL && !(m & M_CTRL) && t != T_F5 && t != T_F6) {
            if (caixa_tratar(j, &entrada, e)) enviar_entrada();
            sujo |= R_PAINEL;
            if (t == TECLA_ESC && hijo < 0) foco = FOCO_EDITOR;
            if (t == TECLA_ESC && hijo >= 0) parar_hijo();
            return;
        }
        tratar_teclado(t, m);
        return;
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (en(r_editor, e->x, e->y)) vista_rodar(n);
        else if (en(r_panel, e->x, e->y)) {
            if (panel_vista == PANEL_SAIDA) {
                seguir = false;
                saida_topo -= n * 3;
                int tope = n_lin + 1 - lineas_panel();
                if (saida_topo >= tope) { saida_topo = tope > 0 ? tope : 0; seguir = true; }
                if (saida_topo < 0) saida_topo = 0;
            } else {
                err_topo -= n * 3;
                int tope = n_errs - lineas_panel();
                if (err_topo > tope) err_topo = tope;
                if (err_topo < 0) err_topo = 0;
            }
            sujo |= R_PAINEL;
        }
        return;
    }
    if (e->tipo == EV_CLIQUE) {
        if (en(r_barra, e->x, e->y)) {
            casilla_tratar(&c_limites, e);
            sujo |= R_MENU;
            return;
        }
        if (en(r_editor, e->x, e->y)) {
            foco = FOCO_EDITOR;
            entrada.foco = false;
            vista_clique(e->x, e->y, (int)e->botoes, (int)e->cliques, e->modificadores);
            sujo |= R_PAINEL;
            return;
        }
        if (en(r_abas_p, e->x, e->y)) { clique_abas(e->x); return; }
        if (en(r_panel, e->x, e->y)) {
            if (panel_vista == PANEL_SAIDA && en(r_entrada, e->x, e->y)) {
                foco = FOCO_TERMINAL;
                entrada.foco = true;
                caixa_tratar(j, &entrada, e);
                sujo |= R_PAINEL;
            } else if (panel_vista == PANEL_ERRS) {
                int k = err_topo + (e->y - r_panel.y) / LETRA_A;
                if (k >= 0 && k < n_errs) {
                    err_sel = k;
                    if (errs[k].linea) ir_a_linea(errs[k].linea);
                    sujo |= R_PAINEL;
                }
            }
            return;
        }
        return;
    }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) { vista_mover(e->x, e->y); return; }
    if (e->tipo == EV_SOLTOU) { vista_soltar(); return; }
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    j = jan_abrir("Laboratorio de C", 960, 680);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 640, 420);
    abas[0] = doc_novo();
    abas[0]->ling = LING_C;
    static const char *items_ej[10];
    for (int i = 0; i < N_EJEMPLOS; i++) items_ej[i] = ejemplos_nombre[i];
    combo_iniciar(&cb_ejemplos, 700, 4, 250, items_ej, N_EJEMPLOS, false, 41);
    combo_definir(&cb_ejemplos, "Elegir un ejemplo");
    entrada.x = 86; entrada.l = 400; entrada.texto[0] = 0;
    tiempo_compilacion = -1;
    layout();
    if (argc > 1) abrir_ruta(argv[1]);
    else {
        char c[CAMINHO_MAX];
        formatar_texto(c, sizeof c, "%s/%s", CARPETA_EJEMPLOS, ejemplos_fichero[0]);
        int n, erro;
        bool u;
        char *s = ler_arquivo(c, &n, &erro, &u);
        if (s) { cargar_texto(s, n, NULL); libera(s); }
        else cargar_texto(plantilla, (int)sizeof plantilla - 1, NULL);
    }
    sujo = R_TUDO;
    vista_sujar_tudo();
    redesenhar();
    jan_mostrar(j);
    registrar("labc: listo");

    evento_t e;
    while (!sair_pedido) {
        bool fez = false;
        int lote = 0;
        while (!sair_pedido && lote < 64 && proximo_evento(j->id, &e, false) == 1) { tratar(&e); fez = true; lote++; }
        if (sair_pedido) break;
        if (hijo >= 0) { vigilar_hijo(); fez = true; }
        if (DOC->versao != versao_marcas) {
            if (versao_marcas >= 0 && n_errs && panel_vista == PANEL_ERRS) { /* se conservan hasta compilar de nuevo */ }
            versao_marcas = DOC->versao;
            sujo |= R_ESTADO;
        }
        if (aviso[0] && (int)(tempo_ms() - aviso_ate) > 0) { aviso[0] = 0; sujo |= R_ESTADO; }
        redesenhar();
        if (!fez) dormir(hijo >= 0 ? 15 : 10);
        else dormir(hijo >= 0 ? 5 : 0);
    }
    if (hijo >= 0) { parar_hijo(); cerrar_hijo(); }
    registrar("labc: cerrado");
    jan_fechar(j);
    return 0;
}
