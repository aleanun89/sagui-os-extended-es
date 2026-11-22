// SPDX-License-Identifier: GPL-3.0-only
/* editor [archivo] - el Editor de texto de SaguiOS (H25).
 *
 * Una barra de botones (Nuevo, Abrir, Guardar, Guardar como) y el área de texto
 * del kit gráfico. Ctrl+S guarda. El título muestra el nombre del archivo y un "*"
 * cuando hay un cambio sin guardar; cerrar con cambios pregunta si quiere
 * guardar. El texto es Latin-1, como en todo el sistema.
 * La ventana es redimensionable y maximizable (D044): el área de texto
 * acompaña el tamaño.
 * Botón derecho en el área de texto (D051): menú con las acciones de la barra y, desde
 * el portapapeles del sistema (D074), Cortar, Copiar, Pegar y
 * Seleccionar todo. La selección (Shift + flechas o arrastrar) y los atajos
 * Ctrl+X/C/V/A quedan en la propia área de texto de la libsagui.
 * El botón «Vista» (D196) enseña el documento con las fuentes vectoriales elegidas en
 * Configuración > Fuentes (documentos para el cuerpo, títulos para la primera línea),
 * con ajuste de línea, tamaño con A- / A+ y desplazamiento con la rueda; «Edición» vuelve. */
#include "sagui.h"
#include "gui.h"
#include "fontes.h"

#define LARG_INICIO 700
#define ALT_INICIO  480
#define LARG (j->larg)
#define ALT  (j->alt)
#define BARRA 40

static jan_t *j;
static area_t area;
static char caminho[160];
static bool modificado_mostrado = true;

static const char *nome_curto(void)
{
    if (!caminho[0]) return _("sin nombre");
    const char *b = acha_ultimo_char(caminho, '/');
    return b ? b + 1 : caminho;
}

static void atualizar_titulo(void)
{
    if (modificado_mostrado == area.modificado) return;
    modificado_mostrado = area.modificado;
    char t[96];
    formatar_texto(t, sizeof t, _("Editor - %s%s"), nome_curto(), area.modificado ? " *" : "");
    titulo_janela(j->id, t);
    registrar("editor: titulo %s\n", t);
}

static void forcar_titulo(void)
{
    modificado_mostrado = !area.modificado;
    atualizar_titulo();
}

static bool abrir_arquivo(const char *c)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) {
        char m[200];
        formatar_texto(m, sizeof m, _("No se ha podido abrir\n%s\n(%s)"), c, nome_erro(fd));
        mensagem("Editor", m);
        return false;
    }
    char *buf = aloca(65536);
    int n = 0, r;
    while (n < 65535 && (r = ler(fd, buf + n, (unsigned)(65535 - n))) > 0) n += r;
    fechar(fd);
    n = (int)utf8_para_latin1(buf, (size_t)n, NULL);   /* textos de afuera suelen venir en UTF-8 (D027) */
    area_definir(&area, buf, n);
    libera(buf);
    copia_texto(caminho, c, sizeof caminho);
    registrar("editor: aberto %s (%d bytes)\n", caminho, n);
    forcar_titulo();
    area_desenhar(j, &area);
    return true;
}

/* Filtros de los diálogos de archivos (mejora 103). */
static const char *const filtros_texto = N_("Texto (*.txt)|*.txt|Código y Markdown|*.c;*.h;*.py;*.md;*.sh|Todos los archivos (*.*)|*");

static bool salvar_como(void);

static bool salvar(void)
{
    if (!caminho[0]) return salvar_como();
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0 || escrever_tudo(fd, area.buf, (unsigned)area.tam) != area.tam) {
        if (fd >= 0) fechar(fd);
        char m[200];
        formatar_texto(m, sizeof m, _("No se ha podido guardar\n%s"), caminho);
        mensagem("Editor", m);
        return false;
    }
    fechar(fd);
    area.modificado = false;
    registrar("editor: salvo %s (%d bytes)\n", caminho, area.tam);
    forcar_titulo();
    return true;
}

static bool salvar_como(void)
{
    char c[160];
    copia_texto(c, caminho[0] ? caminho : "/tmp/", sizeof c);
    if (!dialogo_guardar(_("Guardar como"), _(filtros_texto), c, c, sizeof c) || !c[0])
        return false;
    copia_texto(caminho, c, sizeof caminho);
    return salvar();
}

/* Antes de tirar el texto: pregunta si quiere guardar. Devuelve false si la
 * persona quiso guardar y no salió bien (ahí no descarta nada). */
static bool pode_descartar(void)
{
    if (!area.modificado) return true;
    if (confirmar("Editor", _("El texto tiene cambios sin guardar.\n¿Guardar antes?")))
        return salvar();
    return true;
}

#define NBT 7   /* Nuevo, Abrir, Guardar, Guardar como, Vista/Edición, A-, A+ */
static botao_t bt[NBT] = {
    { 8, 7, 80, 26, N_("Nuevo"), false }, { 96, 7, 80, 26, N_("Abrir"), false },
    { 184, 7, 80, 26, N_("Guardar"), false }, { 272, 7, 110, 26, N_("Guardar como"), false },
    { 390, 7, 80, 26, N_("Vista"), false }, { 478, 7, 36, 26, "A-", false }, { 518, 7, 36, 26, "A+", false },
};

/* La vista con fuentes vectoriales (D196). */
static bool vista;
static fonte_t *fdoc, *ftit;
static bool fuentes_probadas;
static int vista_px = 18, vista_topo;

static void fuentes_abrir(void)
{
    if (fuentes_probadas) return;
    fuentes_probadas = true;
    fdoc = fuentes_abrir_defecto(FUENTE_DOCUMENTOS);
    ftit = fuentes_abrir_defecto(FUENTE_TITULOS);
    if (!fdoc) fdoc = ftit;
    if (!ftit) ftit = fdoc;
}

/* Recorre el documento con ajuste de línea por palabras, dibujándolo (o solo contando)
 * a partir de la línea vista_topo. Devuelve el total de líneas. */
static int vista_lineas(bool dibujar)
{
    const int x0 = 14, ancho = LARG - 28, ymax = ALT - 10;
    const char *t = area.buf;
    int n = area.tam, linea = 0, y = BARRA + 10, i = 0;
    bool primera = true;
    while (i <= n) {
        int fin = i;
        while (fin < n && t[fin] != '\n') fin++;
        fonte_t *f = primera ? ftit : fdoc;
        int px = primera && ftit != fdoc ? vista_px * 3 / 2 : vista_px;
        int alto = fonte_altura_normal(f, px);
        int a = i;
        do {
            int corte = a, p = a;
            while (p < fin) {
                int q = p;
                while (q < fin && t[q] != ' ') q++;
                if (corte > a && fonte_medir_n(f, t + a, q - a, px) > ancho) break;
                corte = q;
                p = q < fin ? q + 1 : fin;
            }
            if (linea >= vista_topo) {
                if (dibujar && y + alto <= ymax) fonte_texto_buf_n(f, j->px, j->larg, j->alt, x0, y, t + a, corte - a, px, 0x000000);
                y += alto;
            }
            linea++;
            a = corte;
            while (a < fin && t[a] == ' ') a++;
        } while (a < fin);
        primera = false;
        i = fin + 1;
    }
    return linea;
}

static void vista_desenhar(void)
{
    g_retangulo(j, 6, BARRA + 4, LARG - 12, ALT - BARRA - 10, 0xFFFFFF);
    g_moldura(j, 6, BARRA + 4, LARG - 12, ALT - BARRA - 10, COR_BORDA);
    if (!fdoc) {
        g_texto(j, 16, BARRA + 16, _("No hay ninguna fuente elegida: Configuración > Fuentes."), 0xB03000);
        return;
    }
    int total = vista_lineas(false);
    if (vista_topo > total - 1) vista_topo = total > 0 ? total - 1 : 0;
    vista_lineas(true);
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, BARRA, 0xE6E9EC);
    g_retangulo(j, 0, BARRA - 1, LARG, 1, COR_BORDA);
    if (LARG >= 700) rotulo(j, LARG - 120, 12, _("Ctrl+S guarda"), COR_TEXTO_FRACO);
    bt[4].texto = vista ? _("Edición") : _("Vista");
    for (int i = 0; i < (vista ? NBT : 5); i++) botao_desenhar(j, &bt[i]);
}

/* Redibuja el área central: el texto editable o la vista. */
static void redibujar_centro(void)
{
    if (vista) vista_desenhar();
    else area_desenhar(j, &area);
}

/* Después de EV_REDIMENSIONAR: el área de texto ocupa lo que sobre. */
static void novo_tamanho(void)
{
    g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
    desenhar_barra();
    area_mudar_tamanho(&area, LARG - 12, ALT - BARRA - 10);
    redibujar_centro();
    jan_mostrar(j);
    registrar("editor: janela %dx%d, %d linhas visiveis\n", LARG, ALT, area_linhas_visiveis(&area));
}

/* D428: otro esquema de apariencia: se repinta todo con los colores nuevos. */
static void repintar_tema(void)
{
    g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
    desenhar_barra();
    redibujar_centro();
    jan_mostrar(j);
}

int principal(int argc, char **argv)
{
    j = jan_abrir(_("Editor - sin nombre"), LARG_INICIO, ALT_INICIO);
    tema_ao_mudar(repintar_tema);   /* D428 */
    if (!j || !area_iniciar(&area, 6, BARRA + 4, LARG - 12, ALT - BARRA - 10)) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 400, 160);
    desenhar_barra();
    area.foco = true;
    if (argc > 1) abrir_arquivo(argv[1]);
    else forcar_titulo();
    area_desenhar(j, &area);
    jan_mostrar(j);
    registrar("editor: pronto\n");

    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) {
            if (pode_descartar()) break;
            continue;
        }
        if (jan_redimensionar(j, &e)) { novo_tamanho(); continue; }
        if (e.tipo == EV_TECLA && e.tecla == 19) {          /* Ctrl+S */
            salvar();
            continue;
        }
        int acao = -1;
        for (int i = 0; i < 4; i++)
            if (botao_tratar(j, &bt[i], &e)) acao = i;
        /* La vista con fuentes vectoriales (D196). */
        {
            bool cambio = false;
            if (botao_tratar(j, &bt[4], &e)) {
                vista = !vista;
                if (vista) {
                    fuentes_abrir();
                    vista_topo = 0;
                    char nf[64] = "", ne[16];
                    if (fdoc) fonte_nombre(fdoc, nf, sizeof nf, ne, sizeof ne);
                    registrar("editor: vista con fuente %s %d px\n", fdoc ? nf : "(ninguna)", vista_px);
                } else registrar("editor: edicion\n");
                cambio = true;
            } else if (vista && botao_tratar(j, &bt[5], &e)) {
                if (vista_px > 10) vista_px -= 2;
                registrar("editor: vista %d px\n", vista_px);
                cambio = true;
            } else if (vista && botao_tratar(j, &bt[6], &e)) {
                if (vista_px < 60) vista_px += 2;
                registrar("editor: vista %d px\n", vista_px);
                cambio = true;
            } else if (vista && e.tipo == EV_RODA) {
                vista_topo -= (int)e.cliques * 3;
                if (vista_topo < 0) vista_topo = 0;
                cambio = true;
            }
            if (cambio) {
                g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
                desenhar_barra();
                redibujar_centro();
                jan_mostrar(j);
                continue;
            }
            if (vista) {
                if (acao >= 0) { vista = false; g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO); desenhar_barra(); area_desenhar(j, &area); }
                else if (e.tipo != EV_MENU) continue;   /* la vista no edita */
            }
        }
        /* Menú contextual (D051): las líneas son las mismas acciones de los botones,
         * con un separador entre _("Abrir") y _("Guardar") (que cuenta como línea). */
        if (e.tipo == EV_CLIQUE && (e.botoes & 2) && e.y >= BARRA) {
            /* Las líneas 0-4 son las de antes (los tests cuentan con ellas); las de
             * edición vienen después de otro separador. Sin selección, Cortar y
             * Copiar aparecen desactivadas. */
            bool sel = area.ancora >= 0 && area.ancora != area.cursor;
            char itens[200];
            formatar_texto(itens, sizeof itens, "%s\n%s\n-\n%s\tCtrl+S\n%s\n-\n"
                           "%s%s\tCtrl+X\n%s%s\tCtrl+C\n%s\tCtrl+V\n%s\tCtrl+A",
                           _("Nuevo"), _("Abrir..."), _("Guardar"), _("Guardar como..."),
                           sel ? "" : "~", _("Cortar"), sel ? "" : "~", _("Copiar"), _("Pegar"), _("Seleccionar todo"));
            menu_contextual(j->id, e.x, e.y, itens, 0);
            continue;
        }
        if (e.tipo == EV_MENU && e.x >= 0) {
            registrar("editor: menu -> %d\n", e.x);
            if (e.x <= 4) acao = e.x < 2 ? e.x : e.x - 1;
            else if (e.x == 6) area_cortar(j, &area);
            else if (e.x == 7) area_copiar(&area);
            else if (e.x == 8) area_colar(j, &area);
            else if (e.x == 9) {
                evento_t a = { .tipo = EV_TECLA, .tecla = 1 };
                area_tratar(j, &area, &a);
            }
            atualizar_titulo();
            if (acao < 0) continue;
        }
        if (acao == 0 && pode_descartar()) {
            area_definir(&area, "", 0);
            caminho[0] = 0;
            forcar_titulo();
            area_desenhar(j, &area);
        }
        if (acao == 1 && pode_descartar()) {
            char c[160];
            copia_texto(c, caminho[0] ? caminho : "/", sizeof c);
            if (dialogo_abrir(_("Abrir"), _(filtros_texto), c, c, sizeof c)) abrir_arquivo(c);
        }
        if (acao == 2) salvar();
        if (acao == 3) salvar_como();
        area_tratar(j, &area, &e);
        atualizar_titulo();
    }
    registrar("editor: fechado\n");
    jan_fechar(j);
    return 0;
}
