// SPDX-License-Identifier: GPL-2.0-or-later
/* fuentes - instalar, quitar y ver las fuentes del sistema (D356).
 *
 *   fuentes [listar]                    las familias instaladas, con su origen
 *   fuentes instalar [--sistema] RUTA...  un .ttf/.otf, varios o una carpeta; antes se valida con la libfontes
 *   fuentes quitar [--sistema] FAMILIA  borra los archivos de la familia (de la cuenta, o del sistema con --sistema)
 *   fuentes actualizar                  avisa a los programas abiertos de que vuelvan a leer las carpetas
 *   fuentes --ventana [ARCHIVO]         la ventana: lista con muestra dibujada con la propia fuente; con un
 *                                       ARCHIVO enseña su vista previa con el botón «Instalar» (lo abre Archivos)
 *
 * Dos sitios: para la cuenta, /home/fuentes (sin ser root); para todas, /apps/fuentes (SAGUI-APPS, en el
 * disco, así sobrevive a los reinicios; hace falta ser administrador: en la ventana se pide con el diálogo
 * de permisos del sistema, D269). Los programas con selector de fuente vuelven a leer las carpetas al
 * abrir el selector; además instalar y quitar suben /tmp/fuentes.cambio (fuentes_cambio()). */
#include "sagui.h"
#include "fontes.h"
#include "contas.h"
#include "gui.h"

#define MAX_FAM 250
static familia_fuente_t fam[MAX_FAM];
static int nfam;

/* Los textos de fuentes_error_texto (libfontes) se marcan aquí para el catálogo de idiomas. */
static const char *const errores_marcados[] = {
    N_("el archivo no existe o no se puede leer"), N_("solo se instalan archivos .ttf y .otf"),
    N_("el archivo es demasiado pequeño o demasiado grande"), N_("no es una fuente válida (archivo dañado)"),
    N_("la fuente no trae glifos de texto"), N_("no se pudo copiar a la carpeta de destino"),
    N_("sin permiso para escribir en la carpeta de destino"),
};
static const char *error_traducido(int r) { (void)errores_marcados; return _(fuentes_error_texto(r)); }

static const char *origen(const familia_fuente_t *f)
{
    for (int e = 0; e < 4; e++) {
        const char *r = f->ruta[e];
        if (!r[0]) continue;
        if (compara_texto_n(r, FUENTES_DIR_SISTEMA "/", 8) == 0) return _("base");
        if (compara_texto_n(r, FUENTES_DIR_APPS "/", 14) == 0) return _("todas las cuentas");
        return _("esta cuenta");
    }
    return "";
}

static void estilos_txt(const familia_fuente_t *f, char *d, int n)
{
    static const char *const nom[4] = { N_("Regular"), N_("Negrita"), N_("Cursiva"), N_("Negrita cursiva") };
    d[0] = 0;
    for (int e = 0; e < 4; e++) {
        if (!f->ruta[e][0]) continue;
        size_t l = tam_texto(d);
        formatar_texto(d + l, (size_t)n - l, "%s%s", l ? ", " : "", _(nom[e]));
    }
}

static void recargar(void)
{
    nfam = fuentes_sistema_listar(fam, MAX_FAM);
}

/* ------------------------------------------------------------ terminal */

static bool es_root_o_admin(void) { return sou_root(); }

static int listar_cmd(void)
{
    recargar();
    imprimir(_("%d familias de fuentes:\n"), nfam);
    for (int i = 0; i < nfam; i++) {
        char st[96];
        estilos_txt(&fam[i], st, sizeof st);
        imprimir("  %-34s %-22s %s\n", fam[i].familia, st, origen(&fam[i]));
    }
    return 0;
}

static int instalar_uno(const char *ruta, const char *dir)
{
    info_arquivo_t inf;
    if (info_arquivo(ruta, &inf) < 0) { imprimir_em(ERROS, _("fuentes: %s: no existe\n"), ruta); return 1; }
    if (inf.tipo == TIPO_PASTA) {
        int malas = 0;
        int bien = fuentes_instalar_carpeta(ruta, dir, &malas);
        imprimir(_("fuentes: %d instaladas desde %s (%d rechazadas)\n"), bien, ruta, malas);
        return bien > 0 || malas == 0 ? 0 : 1;
    }
    char familia[48];
    int r = fuentes_instalar(ruta, dir, familia, sizeof familia);
    if (r != FUENTES_OK) { imprimir_em(ERROS, "fuentes: %s: %s\n", ruta, error_traducido(r)); return 1; }
    imprimir(_("fuentes: instalada «%s» en %s\n"), familia, dir);
    return 0;
}

static int instalar_cmd(int argc, char **argv)
{
    bool sistema = false;
    char dir[96];
    int hechos = 0, rc = 0;
    fuentes_dir_usuario(dir, sizeof dir);
    for (int i = 2; i < argc; i++) if (!compara_texto(argv[i], "--sistema")) sistema = true;
    if (sistema) {
        if (!es_root_o_admin()) { imprimir_em(ERROS, _("fuentes: para todas las cuentas hace falta ser administrador: usa «admin fuentes instalar --sistema ...»\n")); return 1; }
        copia_texto(dir, FUENTES_DIR_APPS, sizeof dir);
    }
    for (int i = 2; i < argc; i++) {
        if (!compara_texto(argv[i], "--sistema")) continue;
        rc |= instalar_uno(argv[i], dir);
        hechos++;
    }
    if (!hechos) { imprimir_em(ERROS, _("uso: fuentes instalar [--sistema] ARCHIVO.ttf|.otf|CARPETA...\n")); return 2; }
    fuentes_cambio_marcar();
    return rc;
}

static int quitar_cmd(int argc, char **argv)
{
    bool sistema = false;
    const char *nombre = NULL;
    for (int i = 2; i < argc; i++) { if (!compara_texto(argv[i], "--sistema")) sistema = true; else nombre = argv[i]; }
    if (!nombre) { imprimir_em(ERROS, _("uso: fuentes quitar [--sistema] FAMILIA\n")); return 2; }
    char dir[96];
    fuentes_dir_usuario(dir, sizeof dir);
    if (sistema) {
        if (!es_root_o_admin()) { imprimir_em(ERROS, _("fuentes: para todas las cuentas hace falta ser administrador: usa «admin fuentes quitar --sistema ...»\n")); return 1; }
        copia_texto(dir, FUENTES_DIR_APPS, sizeof dir);
    }
    int err = 0;
    int n = fuentes_quitar(dir, nombre, &err);
    if (n == 0) {
        recargar();
        for (int i = 0; i < nfam; i++)
            if (!compara_texto(fam[i].familia, nombre)) {
                imprimir_em(ERROS, _("fuentes: «%s» es de %s: no se quita desde aquí (usa --sistema como administrador)\n"), nombre, origen(&fam[i]));
                return 1;
            }
        imprimir_em(ERROS, _("fuentes: no hay ninguna familia «%s»\n"), nombre);
        return 1;
    }
    if (err) imprimir_em(ERROS, _("fuentes: %d archivos no se pudieron borrar\n"), err);
    imprimir(_("fuentes: quitada «%s» (%d archivos)\n"), nombre, n);
    fuentes_cambio_marcar();
    return err ? 1 : 0;
}

/* ------------------------------------------------------------ ventana */

#define VL 600
#define VA 480
#define MAX_PREV 3
static jan_t *jw;
static char itens[MAX_FAM][64];
static lista_t lista = { 12, 34, VL - 24, 180, itens, 0, -1, 0, 0, 0 };
static botao_t b_inst = { 12, 0, 104, 26, N_("Instalar..."), false },
               b_carp = { 124, 0, 104, 26, N_("Carpeta..."), false },
               b_quitar = { 236, 0, 84, 26, N_("Quitar"), false },
               b_act = { 328, 0, 104, 26, N_("Actualizar"), false },
               b_ok = { 0, 0, 104, 26, N_("Instalar"), false },
               b_todos = { 0, 0, 190, 26, N_("Instalar para todos"), false },
               b_cerrar = { 0, 0, 90, 26, N_("Cerrar"), false };
static casilla_t c_todas;
static fonte_t *pf;                 /* la fuente de la vista previa */
static char pf_ruta[96];
static char msg_gui[200];
static bool msg_err;
static unsigned cambio_visto;
static char arch[160];              /* modo vista previa: el archivo a instalar */
static char arch_fam[48], arch_est[24];
static int arch_val;                /* resultado de fuentes_validar */
static bool arch_hecho;

static void cargar_lista(void)
{
    recargar();
    for (int i = 0; i < nfam; i++) {
        char nm[48];
        copia_texto(nm, fam[i].familia, 35);       /* a 34 caracteres, para alinear la columna del origen */
        formatar_texto(itens[i], sizeof itens[i], "%-34s %s", nm, origen(&fam[i]));
    }
    lista.qtd = nfam;
    if (lista.sel >= nfam) lista.sel = nfam - 1;
    if (lista.sel < 0 && nfam) lista.sel = 0;
    cambio_visto = fuentes_cambio();
}

static void usar_fuente(const char *ruta)
{
    if (ruta && pf_ruta[0] && !compara_texto(pf_ruta, ruta)) return;
    if (pf) { fonte_fechar(pf); pf = NULL; }
    pf_ruta[0] = 0;
    if (!ruta) return;
    pf = fonte_abrir_archivo(ruta);
    copia_texto(pf_ruta, ruta, sizeof pf_ruta);
}

/* Una línea de muestra, recortada a lo que cabe. */
static void linea(int x, int y, int ancho, const char *t, int px)
{
    if (!pf) return;
    int n = (int)tam_texto(t);
    while (n > 1 && fonte_medir_n(pf, t, n, px) > ancho) n--;
    fonte_texto_buf_n(pf, jw->px, jw->larg, jw->alt, x, y, t, n, px, COR_TEXTO);
}

static void muestra(int x, int y, int l, int a)
{
    g_retangulo(jw, x, y, l, a, 0xFFFFFF);
    g_moldura(jw, x, y, l, a, 0x9AA3AB);
    if (!pf) { rotulo(jw, x + 10, y + 10, _("Sin muestra."), COR_TEXTO_FRACO); return; }
    int yy = y + 6;
    linea(x + 10, yy, l - 20, "El veloz murciélago hindú comía feliz cardillo y kiwi", 15);
    yy += fonte_altura_normal(pf, 15) + 4;
    linea(x + 10, yy, l - 20, "ABCDEFGHIJKLMNÑOPQRSTUVWXYZ 0123456789", 22);
    yy += fonte_altura_normal(pf, 22) + 4;
    linea(x + 10, yy, l - 20, "abcdefghijklmnñopqrstuvwxyz ¿?¡!", 32);
}

static void mensaje_fijar(bool err, const char *m) { msg_err = err; copia_texto(msg_gui, m, sizeof msg_gui); }

static void dibujar(void)
{
    int L = jw->larg, A = jw->alt;
    g_retangulo(jw, 0, 0, L, A, COR_FUNDO);
    if (arch[0]) {
        rotulo(jw, 12, 12, _("Vista previa de la fuente"), COR_TEXTO);
        char t[200];
        if (arch_val == FUENTES_OK) {
            formatar_texto(t, sizeof t, "%s %s", arch_fam, arch_est);
            rotulo(jw, 12, 34, t, COR_TEXTO);
        } else {
            formatar_texto(t, sizeof t, _("No se puede instalar: %s."), error_traducido(arch_val));
            rotulo(jw, 12, 34, t, 0xB02020);
        }
        const char *b = arch;
        for (const char *p = arch; *p; p++) if (*p == '/') b = p + 1;
        rotulo(jw, 12, 54, b, COR_TEXTO_FRACO);
        muestra(12, 80, L - 24, A - 80 - 118);
        int y = A - 100;
        char d[96];
        fuentes_dir_usuario(d, sizeof d);
        formatar_texto(t, sizeof t, _("Esta cuenta: %s"), d);
        rotulo(jw, 12, y - 12, t, COR_TEXTO_FRACO);
        formatar_texto(t, sizeof t, _("Todas las cuentas: %s (pide permisos de administrador)"), FUENTES_DIR_APPS);
        rotulo(jw, 12, y + 5, t, COR_TEXTO_FRACO);
        b_ok.x = 12; b_ok.y = y + 24;
        b_todos.x = 124; b_todos.y = y + 24;
        b_cerrar.x = L - 12 - b_cerrar.l; b_cerrar.y = y + 24;
        b_ok.texto = arch_hecho ? _("Instalada") : _("Instalar");
        if (arch_val == FUENTES_OK && !arch_hecho) { botao_desenhar(jw, &b_ok); botao_desenhar(jw, &b_todos); }
        botao_desenhar(jw, &b_cerrar);
        if (msg_gui[0]) rotulo(jw, 12, y + 58, msg_gui, msg_err ? 0xB02020 : COR_TEXTO);
        jan_mostrar(jw);
        return;
    }
    char t[160];
    formatar_texto(t, sizeof t, _("%d familias de fuentes instaladas"), nfam);
    rotulo(jw, 12, 12, t, COR_TEXTO);
    int y0 = A - 122;
    lista.l = L - 24;
    lista.a = y0 - 150 - lista.y - 8;
    if (lista.a < 60) lista.a = 60;
    lista_desenhar(jw, &lista);
    if (lista.sel >= 0 && lista.sel < nfam) {
        char st[96];
        estilos_txt(&fam[lista.sel], st, sizeof st);
        rotulo(jw, 12, lista.y + lista.a + 6, st, COR_TEXTO_FRACO);
    }
    muestra(12, lista.y + lista.a + 26, L - 24, y0 - (lista.y + lista.a + 26) - 8);
    c_todas.x = 12; c_todas.y = y0 + 2;
    casilla_desenhar(jw, &c_todas);
    rotulo(jw, 32, y0 + 1, _("Para todas las cuentas (pide permisos de administrador)"), COR_TEXTO);
    b_inst.y = b_carp.y = b_quitar.y = b_act.y = y0 + 28;
    botao_desenhar(jw, &b_inst); botao_desenhar(jw, &b_carp); botao_desenhar(jw, &b_quitar); botao_desenhar(jw, &b_act);
    if (msg_gui[0]) rotulo(jw, 12, y0 + 64, msg_gui, msg_err ? 0xB02020 : COR_TEXTO);
    else rotulo(jw, 12, y0 + 64, _("Elige una fuente de la lista para verla, o instala una nueva."), COR_TEXTO_FRACO);
    jan_mostrar(jw);
}

static void seleccionar(void)
{
    if (lista.sel < 0 || lista.sel >= nfam) { usar_fuente(NULL); return; }
    const familia_fuente_t *f = &fam[lista.sel];
    for (int e = 0; e < 4; e++) if (f->ruta[e][0]) { if (compara_texto(pf_ruta, f->ruta[e])) { pf_ruta[0] = 0; usar_fuente(f->ruta[e]); } return; }
}

/* Instala en la carpeta de la cuenta o, con «todas», en la del sistema pidiendo permisos si hace falta. */
static int instalar_en(const char *ruta, bool todas, char *familia, int nf)
{
    char dir[96];
    if (todas) copia_texto(dir, FUENTES_DIR_APPS, sizeof dir); else fuentes_dir_usuario(dir, sizeof dir);
    int r = fuentes_instalar(ruta, dir, familia, nf);
    if (r == FUENTES_E_PERMISO && todas && !sou_root()) {
        int q = COM_ADMIN(_("instalar fuentes para todas las cuentas"), ({ int rr = fuentes_instalar(ruta, dir, familia, nf); rr == FUENTES_E_PERMISO ? ERRO_ACESSO : rr; }));
        r = q == ERRO_ACESSO ? FUENTES_E_PERMISO : q;
    }
    return r;
}

static void accion_instalar(const char *ruta, bool todas)
{
    char f[48], t[200];
    int r = instalar_en(ruta, todas, f, sizeof f);
    if (r != FUENTES_OK) {
        formatar_texto(t, sizeof t, _("No se instaló: %s."), error_traducido(r));
        mensaje_fijar(true, t);
        registrar("fuentes: rechazada %s (%d)", ruta, r);
        return;
    }
    fuentes_cambio_marcar();
    formatar_texto(t, sizeof t, _("Instalada «%s»."), f);
    mensaje_fijar(false, t);
    registrar("fuentes: instalada %s desde %s (%s)", f, ruta, todas ? "sistema" : "cuenta");
    cargar_lista();
    for (int i = 0; i < nfam; i++) if (!compara_texto(fam[i].familia, f)) { lista.sel = i; break; }
    pf_ruta[0] = 0;
    seleccionar();
}

static void accion_quitar(void)
{
    if (lista.sel < 0 || lista.sel >= nfam) { mensaje_fijar(true, _("Elige primero una fuente de la lista.")); return; }
    char nombre[48], t[200];
    copia_texto(nombre, fam[lista.sel].familia, sizeof nombre);
    const char *o = origen(&fam[lista.sel]);
    bool de_sistema = !compara_texto(o, _("todas las cuentas"));
    if (!compara_texto(o, _("base"))) { mensaje_fijar(true, _("Las fuentes de la base del sistema no se quitan.")); return; }
    formatar_texto(t, sizeof t, _("¿Quitar la fuente «%s»?"), nombre);
    if (!confirmar(_("Fuentes"), t)) return;
    char dir[96];
    if (de_sistema) copia_texto(dir, FUENTES_DIR_APPS, sizeof dir); else fuentes_dir_usuario(dir, sizeof dir);
    int err = 0;
    int n = fuentes_quitar(dir, nombre, &err);
    if (n == 0 && de_sistema && !sou_root()) {
        int q = COM_ADMIN(_("quitar fuentes de todas las cuentas"), ({ int nn = fuentes_quitar(dir, nombre, &err); (nn == 0 && err) ? ERRO_ACESSO : nn; }));
        n = q >= 0 ? q : 0;
    }
    if (n == 0) { mensaje_fijar(true, _("No se pudo quitar (¿faltan permisos?).")); return; }
    fuentes_cambio_marcar();
    formatar_texto(t, sizeof t, _("Quitada «%s»."), nombre);
    mensaje_fijar(false, t);
    registrar("fuentes: quitada %s (%d archivos)", nombre, n);
    pf_ruta[0] = 0;
    cargar_lista();
    seleccionar();
}

static int ventana(const char *archivo)
{
    if (archivo) {
        copia_texto(arch, archivo, sizeof arch);
        arch_val = fuentes_validar(arch, arch_fam, sizeof arch_fam, arch_est, sizeof arch_est);
        registrar("fuentes: vista previa %s (%d)", arch, arch_val);
    }
    jw = jan_abrir(archivo ? _("Instalar fuente") : _("Fuentes"), archivo ? 560 : VL, archivo ? 380 : VA);
    if (!jw) { imprimir(_("fuentes: no se pudo abrir la ventana (¿hay escritorio?)\n")); return 1; }
    jan_estilo(jw, JAN_REDIMENSIONAVEL, 480, archivo ? 340 : 420);
    if (archivo) { if (arch_val == FUENTES_OK) usar_fuente(arch); }
    else { cargar_lista(); seleccionar(); }
    dibujar();
    evento_t e;
    for (;;) {
        if (esperar_evento(jw->id, &e, 500) != 1) {
            if (!archivo && fuentes_cambio() != cambio_visto) { pf_ruta[0] = 0; cargar_lista(); seleccionar(); dibujar(); }
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(jw, &e)) { dibujar(); continue; }
        if (e.tipo != EV_CLIQUE && e.tipo != EV_SOLTOU && e.tipo != EV_TECLA && e.tipo != EV_RODA) continue;
        if (archivo) {
            if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
            if (arch_val == FUENTES_OK && !arch_hecho) {
                if (botao_tratar(jw, &b_ok, &e)) { accion_instalar(arch, false); arch_hecho = msg_err == false; }
                else if (botao_tratar(jw, &b_todos, &e)) { accion_instalar(arch, true); arch_hecho = msg_err == false; }
            }
            if (botao_tratar(jw, &b_cerrar, &e)) break;
        } else {
            int q = lista_tratar(jw, &lista, &e);
            if (q != LISTA_NADA) { msg_gui[0] = 0; seleccionar(); }
            else if (casilla_tratar(&c_todas, &e)) { }
            else if (botao_tratar(jw, &b_inst, &e)) {
                char r[160];
                if (dialogo_abrir(_("Instalar fuente"), _("Fuentes (*.ttf)|*.ttf|Fuentes (*.otf)|*.otf|Todos|*"), "/", r, sizeof r)) accion_instalar(r, c_todas.marcada);
            } else if (botao_tratar(jw, &b_carp, &e)) {
                char r[160];
                if (dialogo_carpeta(_("Instalar las fuentes de una carpeta"), "/", r, sizeof r)) {
                    char dir[96];
                    if (c_todas.marcada) copia_texto(dir, FUENTES_DIR_APPS, sizeof dir); else fuentes_dir_usuario(dir, sizeof dir);
                    int malas = 0, bien = fuentes_instalar_carpeta(r, dir, &malas);
                    char t[160];
                    formatar_texto(t, sizeof t, _("%d instaladas, %d rechazadas."), bien, malas);
                    mensaje_fijar(bien == 0 && malas > 0, t);
                    fuentes_cambio_marcar();
                    cargar_lista();
                }
            } else if (botao_tratar(jw, &b_quitar, &e)) accion_quitar();
            else if (botao_tratar(jw, &b_act, &e)) { pf_ruta[0] = 0; fuentes_cambio_marcar(); cargar_lista(); seleccionar(); mensaje_fijar(false, _("Lista actualizada.")); }
        }
        dibujar();
    }
    usar_fuente(NULL);
    jan_fechar(jw);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && (!compara_texto(argv[1], "--ventana") || !compara_texto(argv[1], "-v"))) return ventana(argc > 2 ? argv[2] : NULL);
    if (argc < 2 || !compara_texto(argv[1], "listar")) return listar_cmd();
    if (!compara_texto(argv[1], "instalar")) return instalar_cmd(argc, argv);
    if (!compara_texto(argv[1], "quitar")) return quitar_cmd(argc, argv);
    if (!compara_texto(argv[1], "actualizar")) {
        fuentes_cambio_marcar();
        recargar();
        imprimir(_("fuentes: lista actualizada, %d familias\n"), nfam);
        return 0;
    }
    imprimir_em(ERROS, _("uso: fuentes [listar | instalar [--sistema] ARCHIVO|CARPETA... | quitar [--sistema] FAMILIA | actualizar | --ventana [ARCHIVO]]\n"));
    return 2;
}
