// SPDX-License-Identifier: GPL-3.0-only
/* traductor - editor de traducciones de SaguiOS, al estilo de Poedit (D335, mejora 133).
 *
 *   traductor [archivo.po|.pot]                abre la ventana
 *   traductor --estadisticas A.po              recuento (traducidas, dudosas, sin traducir)
 *   traductor --validar A.po                   los problemas de formato, atajos, etiquetas y cabecera
 *   traductor --actualizar A.po A.pot          como msgmerge: nuevas, dudosas y obsoletas
 *   traductor --generar A.po [SALIDA.cat]      el catálogo que carga el sistema (D313)
 *
 * La lógica (leer y escribir .po, validar, msgmerge, .cat, memoria de traducción) está en
 * traductor_po.h y la prueba el PC (tests/traductor_unidad.c). Aquí solo la ventana y las órdenes.
 * Los textos se guardan en UTF-8, como en el archivo; la ventana los enseña en Latin-1. */
#include "sagui.h"
#include "gui.h"
#include "ia.h"
#include "catalogo.h"
#include "traductor_po.h"

#define LARG 880
#define ALT  600
#define MEM_USUARIO "/home/.config/traductor/memoria.po"

/* ------------------------------------------------------------ estado */

static po_t      po;
static char      ruta[160];          /* el archivo abierto ("" = ninguno) */
static char      idioma[IDIOMA_MAX]; /* el idioma del catálogo ("pt_BR"...) */
static bool      es_pot, modificado;
static po_t      mem;                /* memoria de traducción: catálogos del sistema + la del usuario */
static bool      mem_cargada;

static jan_t    *j;
static barra_menu_t menu;
static barra_estado_t estado;
static caixa_t   busca;
static area_t    a_orig, a_trad;
static lista_t   lista;
static lista_t   l_sug;
static char      sug_itens[4][64], sug_texto[4][512];
static int       n_sug;
static botao_t   b_mem, b_ia, b_sig, b_copiar;
static int       *vista;             /* índices en po.e de lo que enseña la lista */
static int       n_vista, cap_vista;
static int       filtro;             /* 0 todas, 1 sin traducir, 2 dudosas, 3 traducidas */
static int       sel = -1;           /* la entrada elegida (índice en po.e) */
static int       forma;              /* la forma de plural que se edita */
static char      aviso[3][160];      /* problemas de validación de la forma actual */
static int       n_aviso;
static char      barra_msg[96];
static bool      foco_lista = true;

#define LISTA_X 8
#define LISTA_Y 54
#define LISTA_L 864
#define LISTA_A 164
#define PL_X 8
static const int FILTRO_L[4] = { 64, 120, 80, 104 };

/* ------------------------------------------------------------ utilidades */

static char *a_l1(const char *u8)
{
    int n = (int)tam_texto(u8) + 1;
    char *d = aloca((size_t)n);
    if (d) po_u8_l1(u8, d, n);
    return d;
}

static const char *base_de(const char *r)
{
    const char *b = r;
    for (const char *q = r; *q; q++) if (*q == '/') b = q + 1;
    return b;
}

static char *leer_arquivo(const char *caminho, int *n)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return NULL;
    unsigned t = 0;
    char *d = ler_todo_fd(fd, &t);
    fechar(fd);
    if (d) *n = (int)t;
    return d;
}

static bool escribir_arquivo(const char *caminho, const void *d, int n)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    int r = escrever_tudo(fd, d, (unsigned)n);
    fechar(fd);
    return r >= 0;
}

static bool termina_en(const char *s, const char *suf)
{
    int a = (int)tam_texto(s), b = (int)tam_texto(suf);
    return a >= b && compara_texto(s + a - b, suf) == 0;
}

/* La ruta con la extensión cambiada por «.cat». */
static void ruta_cat(const char *r, char *dst, int max)
{
    copia_texto(dst, r, (size_t)max);
    int n = (int)tam_texto(dst);
    if (termina_en(dst, ".po")) n -= 3;
    else if (termina_en(dst, ".pot")) n -= 4;
    if (n + 5 > max) n = max - 5;
    copia_texto(dst + n, ".cat", (size_t)(max - n));
}

/* El idioma por la ruta (.../idiomas/pt_BR/x.po) o por la cabecera. */
static void detectar_idioma(void)
{
    idioma[0] = 0;
    char h[40];
    if (po_cab_campo(&po, "Language", h, sizeof h) && h[0]) idioma_normalizar(h, idioma);
    if (idioma[0]) return;
    const char *q = ruta;
    for (; *q; q++) {
        if (compara_texto_n(q, "/idiomas/", 9) == 0) {
            char d[IDIOMA_MAX];
            int k = 0;
            for (const char *r = q + 9; *r && *r != '/' && k < IDIOMA_MAX - 1; r++) d[k++] = *r;
            d[k] = 0;
            idioma_normalizar(d, idioma);
            return;
        }
    }
}

/* Reglas de la cabecera: lo que falta o no es UTF-8 se arregla al guardar. */
static void reparar_cabecera(void)
{
    int h = po_validar_cabecera(&po);
    if (!h) return;
    if (h & (PO_H_MIME | PO_H_CHARSET)) {
        po_cab_fijar(&po, "MIME-Version", "1.0");
        po_cab_fijar(&po, "Content-Type", "text/plain; charset=UTF-8");
        po_cab_fijar(&po, "Content-Transfer-Encoding", "8bit");
    }
    if ((h & PO_H_IDIOMA) && idioma[0]) po_cab_fijar(&po, "Language", idioma);
    if (h & PO_H_PLURALES) po_cab_fijar(&po, "Plural-Forms", po_plurales_de(idioma));
    registrar("traductor: cabecera corregida (%d)", h);
}

/* ------------------------------------------------------------ memoria de traducción */

static void cargar_memoria(void)
{
    if (mem_cargada) return;
    mem_cargada = true;
    po_iniciar(&mem);
    if (idioma[0]) {
        char dir[64];
        formatar_texto(dir, sizeof dir, "/sistema/idiomas/%s", idioma);
        entrada_pasta_t ep;
        for (int i = 0; listar(dir, i, &ep) > 0 && i < 64; i++) {
            if (!termina_en(ep.nome, ".cat")) continue;
            char r[128];
            formatar_texto(r, sizeof r, "%s/%s", dir, ep.nome);
            int n = 0;
            char *d = leer_arquivo(r, &n);
            if (d) { po_de_cat(&mem, (const uint8_t *)d, n); libera(d); }
        }
    }
    int n = 0;
    char *d = leer_arquivo(MEM_USUARIO, &n);
    if (d) {
        po_t u;
        po_iniciar(&u);
        if (po_leer(&u, d, n, NULL, 0) == 0)
            for (int i = 0; i < u.n; i++) {
                if (!po_en_lista(&u.e[i]) || u.e[i].id_pl) continue;
                po_ent_t c = po_copiar_ent(&u.e[i]);
                po_anadir(&mem, &c);
            }
        po_liberar(&u);
        libera(d);
    }
    registrar("traductor: memoria de traduccion con %d cadenas", mem.n);
}

/* Lo traducido y no dudoso se suma a la memoria del usuario (por cadena original). */
static void guardar_memoria(void)
{
    po_t u;
    po_iniciar(&u);
    int n = 0;
    char *d = leer_arquivo(MEM_USUARIO, &n);
    if (d) { po_leer(&u, d, n, NULL, 0); libera(d); }
    int nuevas = 0;
    for (int i = 0; i < po.n; i++) {
        const po_ent_t *e = &po.e[i];
        if (!po_en_lista(e) || e->id_pl || e->fuzzy || !po_completa(e) || po_validar_ent(&po, e)) continue;
        int k = po_buscar(&u, NULL, e->id);
        if (k >= 0) { if (compara_texto(u.e[k].str[0], e->str[0]) != 0) po_fijar(&u.e[k].str[0], e->str[0]); continue; }
        po_ent_t c;
        memset(&c, 0, sizeof c);
        c.id = po_dup(e->id); c.str[0] = po_dup(e->str[0]); c.n_str = 1;
        po_anadir(&u, &c);
        nuevas++;
    }
    if (u.n > 0) {
        criar_pasta("/home/.config");
        criar_pasta("/home/.config/traductor");
        int t;
        char *s = po_escribir(&u, &t);
        if (s) { escribir_arquivo(MEM_USUARIO, s, t); libera(s); }
    }
    registrar("traductor: memoria del usuario +%d", nuevas);
    po_liberar(&u);
    mem_cargada = false;
    po_liberar(&mem);
}

/* ------------------------------------------------------------ vista (filtros y búsqueda) */

static bool en_filtro(const po_ent_t *e)
{
    int st = po_estado(e);
    if (filtro == 1) return st == PO_SIN;
    if (filtro == 2) return st == PO_DUDOSA;
    if (filtro == 3) return st == PO_TRAD;
    return true;
}

static void refrescar_vista(void)
{
    if (!vista) { cap_vista = 256; vista = aloca(sizeof(int) * (size_t)cap_vista); }
    if (po.n + 1 > cap_vista) { cap_vista = po.n + 64; vista = realoca(vista, sizeof(int) * (size_t)cap_vista); }
    char *q = po_l1_u8(busca.texto, (int)tam_texto(busca.texto));
    n_vista = 0;
    for (int i = 0; i < po.n; i++) {
        const po_ent_t *e = &po.e[i];
        if (!po_en_lista(e) || !en_filtro(e)) continue;
        if (q && q[0] && !po_coincide(e, q)) continue;
        vista[n_vista++] = i;
    }
    if (q) libera(q);
    lista.qtd = n_vista;
    lista.sel = -1;
    for (int i = 0; i < n_vista; i++) if (vista[i] == sel) lista.sel = i;
    if (lista.sel < 0) { sel = -1; }
    if (lista.topo > n_vista - 1) lista.topo = n_vista > 0 ? n_vista - 1 : 0;
}

/* ------------------------------------------------------------ validación en vivo */

static void validar_actual(void)
{
    n_aviso = 0;
    if (sel < 0) return;
    const po_ent_t *e = &po.e[sel];
    char det[2][48];
    int v = po_validar(&po, e, forma, det);
    if (!v) return;
    if (v & PO_V_FORMATO) formatar_texto(aviso[n_aviso++], sizeof aviso[0], _("Formato: el original lleva %s y la traducción %s"), det[0], det[1]);
    if (v & PO_V_ESCAPES) copia_texto(aviso[n_aviso++], _("Saltos de línea o tabuladores distintos de los del original"), sizeof aviso[0]);
    if (v & PO_V_ATAJO) copia_texto(aviso[n_aviso++], _("El atajo (& o _) o el texto tras el tabulador no coincide"), sizeof aviso[0]);
    if (n_aviso < 3 && (v & PO_V_ETIQUETA)) copia_texto(aviso[n_aviso++], _("Las etiquetas <...> no coinciden con las del original"), sizeof aviso[0]);
    if (n_aviso < 3 && (v & PO_V_LONGITUD)) copia_texto(aviso[n_aviso++], _("Es mucho más larga que el original: quizá no quepa"), sizeof aviso[0]);
    registrar("traductor: validacion %d forma %d: %s", v, forma, e->id);
}

/* ------------------------------------------------------------ sugerencias */

static void sugerencias_memoria(void)
{
    n_sug = 0;
    if (sel < 0 || po.e[sel].id_pl) return;
    cargar_memoria();
    const po_t *fu[1] = { &mem };
    po_sug_t r[3];
    int n = po_memoria(fu, 1, po.e[sel].id, r, 3);
    for (int i = 0; i < n; i++) {
        char *t = a_l1(r[i].trad);
        if (!t) continue;
        copia_texto(sug_texto[n_sug], t, sizeof sug_texto[0]);
        formatar_texto(sug_itens[n_sug], sizeof sug_itens[0], "%d%% %s", r[i].puntos, t);
        libera(t);
        n_sug++;
    }
    l_sug.qtd = n_sug;
    l_sug.sel = -1;
    registrar("traductor: memoria: %d sugerencias para %s", n_sug, po.e[sel].id);
}

static void ia_texto(const char *t, int n, void *ctx)
{
    char *dst = ctx;
    int k = (int)tam_texto(dst);
    for (int i = 0; i < n && k < 500; i++) dst[k++] = t[i];
    dst[k] = 0;
}
static void ia_fin(int completa, void *ctx) { (void)completa; (void)ctx; }
static int ia_no_parar(void *ctx) { (void)ctx; return 0; }

/* Una propuesta del agente de IA del sistema: solo se añade a la lista, el traductor la acepta o no. */
static void sugerir_ia(void)
{
    if (sel < 0 || po.e[sel].id_pl) return;
    ia_config_t cfg;
    ia_config_carregar(&cfg);
    ia_config_escolher(&cfg, IA_USO_CHATIA);
    if (!cfg.url[0]) {
        copia_texto(barra_msg, _("La IA no está configurada (Configuración > IA)"), sizeof barra_msg);
        registrar("traductor: ia sin configurar");
        return;
    }
    char *o = a_l1(po.e[sel].id);
    char *p = aloca(2048);
    if (!o || !p) { if (o) libera(o); if (p) libera(p); return; }
    formatar_texto(p, 2048, "Traduce esta cadena de interfaz del español al idioma «%s». Conserva exactamente %%s, %%d, \\n, "
                   "los atajos (& o _) y las etiquetas. Responde solo con la traducción, sin comillas.\n%s", idioma[0] ? idioma : "en", o);
    char resp[520] = "", err[160] = "";
    copia_texto(barra_msg, _("Consultando a la IA..."), sizeof barra_msg);
    int r = ia_chat(&cfg, p, ia_texto, resp, ia_fin, NULL, ia_no_parar, NULL, err, sizeof err);
    libera(o); libera(p);
    if (r < 0 || !resp[0]) {
        formatar_texto(barra_msg, sizeof barra_msg, _("La IA no respondió: %s"), err);
        registrar("traductor: ia error: %s", err);
        return;
    }
    char *a = resp;
    while (*a == ' ' || *a == '\n' || *a == '"') a++;
    int n = (int)tam_texto(a);
    while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\n' || a[n - 1] == '"')) a[--n] = 0;
    /* la propuesta va la primera, marcada «IA» */
    if (n_sug >= 4) n_sug = 3;
    for (int i = n_sug; i > 0; i--) { memcpy(sug_itens[i], sug_itens[i - 1], sizeof sug_itens[0]); memcpy(sug_texto[i], sug_texto[i - 1], sizeof sug_texto[0]); }
    copia_texto(sug_texto[0], a, sizeof sug_texto[0]);
    formatar_texto(sug_itens[0], sizeof sug_itens[0], "IA %s", a);
    n_sug++;
    l_sug.qtd = n_sug;
    l_sug.sel = 0;
    barra_msg[0] = 0;
    registrar("traductor: ia propone: %s", a);
}

/* ------------------------------------------------------------ editor de la cadena */

static void cargar_areas(void)
{
    if (sel < 0) { area_definir(&a_orig, "", 0); area_definir(&a_trad, "", 0); return; }
    po_ent_t *e = &po.e[sel];
    const char *o = (e->id_pl && forma >= 1) ? e->id_pl : e->id;
    char *lo = a_l1(o), *lt = a_l1(e->str[forma] ? e->str[forma] : "");
    if (lo) { area_definir(&a_orig, lo, (int)tam_texto(lo)); libera(lo); }
    if (lt) { area_definir(&a_trad, lt, (int)tam_texto(lt)); libera(lt); }
    a_orig.somente_leitura = true;
    a_trad.somente_leitura = es_pot;
    a_trad.modificado = false;
}

static void seleccionar(int idx_vista)
{
    if (idx_vista < 0 || idx_vista >= n_vista) { sel = -1; }
    else { sel = vista[idx_vista]; lista.sel = idx_vista; }
    forma = 0;
    cargar_areas();
    validar_actual();
    sugerencias_memoria();
    if (sel >= 0) registrar("traductor: cadena %d: %s", idx_vista, po.e[sel].id);
}

/* El texto de la traducción cambió: pasa a la entrada (solo si es distinto de lo que ya se veía). */
static void confirmar_cambio(void)
{
    if (sel < 0 || es_pot) return;
    po_ent_t *e = &po.e[sel];
    char *ac = a_l1(e->str[forma] ? e->str[forma] : "");
    bool igual = ac && compara_texto(ac, a_trad.buf) == 0;
    if (ac) libera(ac);
    if (igual) return;
    char *u = po_l1_u8(a_trad.buf, a_trad.tam);
    if (!u) return;
    if (e->str[forma]) libera(e->str[forma]);
    e->str[forma] = u;
    if (e->fuzzy && po_completa(e)) e->fuzzy = false;   /* corregirla la da por buena, como Poedit */
    modificado = true;
    validar_actual();
}

static void marcar_dudosa(void)
{
    if (sel < 0 || es_pot) return;
    po.e[sel].fuzzy = !po.e[sel].fuzzy;
    modificado = true;
    registrar("traductor: dudosa %s: %s", po.e[sel].fuzzy ? "si" : "no", po.e[sel].id);
}

static void usar_sugerencia(int i)
{
    if (i < 0 || i >= n_sug || sel < 0 || es_pot) return;
    area_definir(&a_trad, sug_texto[i], (int)tam_texto(sug_texto[i]));
    confirmar_cambio();
    registrar("traductor: sugerencia aplicada: %s", sug_texto[i]);
}

static void copiar_original(void)
{
    if (sel < 0 || es_pot) return;
    area_definir(&a_trad, a_orig.buf, a_orig.tam);
    confirmar_cambio();
}

static void siguiente_sin_traducir(void)
{
    if (n_vista == 0) return;
    int desde = lista.sel < 0 ? 0 : lista.sel + 1;
    for (int k = 0; k < n_vista; k++) {
        int i = (desde + k) % n_vista;
        if (po_estado(&po.e[vista[i]]) != PO_TRAD) {
            seleccionar(i);
            if (i >= lista.topo + (LISTA_A - 4) / LISTA_ALTURA_ITEM || i < lista.topo) lista.topo = i;
            return;
        }
    }
}

/* ------------------------------------------------------------ dibujo */

#define COR_AVISO 0xC02020
#define COR_DUDOSA 0xD8A800
#define COR_OK 0x2E9E4F

static void recorte_linea(char *dst, int max, const char *u8)
{
    char *l = a_l1(u8);
    int k = 0;
    if (l) {
        for (const char *q = l; *q && k < max - 3; q++) {
            if (*q == '\n') { dst[k++] = '\\'; dst[k++] = 'n'; }
            else if (*q == '\t') { dst[k++] = '\\'; dst[k++] = 't'; }
            else dst[k++] = *q;
        }
        libera(l);
    }
    dst[k] = 0;
}

static void desenhar_lista(jan_t *jj, lista_t *l)
{
    int vis = (l->a - 4) / LISTA_ALTURA_ITEM;
    g_retangulo(jj, l->x, l->y, l->l, l->a, COR_BRANCO);
    g_moldura(jj, l->x, l->y, l->l, l->a, COR_BORDA);
    int col2 = l->x + l->l / 2 + 8, cols = (l->l / 2 - 40) / LETRA_L;
    char t[200];
    for (int k = 0; k < vis && l->topo + k < l->qtd; k++) {
        int i = l->topo + k, y = l->y + 2 + k * LISTA_ALTURA_ITEM;
        const po_ent_t *e = &po.e[vista[i]];
        bool s = i == l->sel;
        if (s) g_retangulo(jj, l->x + 2, y, l->l - 12, LISTA_ALTURA_ITEM, COR_SELECAO);
        int st = po_estado(e);
        g_retangulo(jj, l->x + 8, y + 5, 8, 10, st == PO_TRAD ? COR_OK : st == PO_DUDOSA ? COR_DUDOSA : 0xB8BEC4);
        recorte_linea(t, sizeof t, e->id);
        g_texto_n(jj, l->x + 24, y + 2, t, cols, s ? COR_BRANCO : COR_TEXTO);
        recorte_linea(t, sizeof t, e->str[0] ? e->str[0] : "");
        g_texto_n(jj, col2, y + 2, t, cols, s ? COR_BRANCO : (st == PO_DUDOSA ? 0x8A6A00 : COR_TEXTO));
        if (po_validar_ent(&po, e)) g_texto(jj, l->x + l->l - 24, y + 2, "!", s ? COR_BRANCO : COR_AVISO);
    }
    if (l->qtd > vis) {
        int trilho = l->a - 4, pa = trilho * vis / l->qtd;
        if (pa < 8) pa = 8;
        int py = (trilho - pa) * l->topo / (l->qtd - vis);
        g_retangulo(jj, l->x + l->l - 6, l->y + 2 + py, 4, pa, 0x9AA3AC);
    }
}

static void desenhar_sug(jan_t *jj, lista_t *l)
{
    g_retangulo(jj, l->x, l->y, l->l, l->a, COR_BRANCO);
    g_moldura(jj, l->x, l->y, l->l, l->a, COR_BORDA);
    for (int i = 0; i < l->qtd && i < 3; i++) {
        int y = l->y + 2 + i * LISTA_ALTURA_ITEM;
        if (i == l->sel) g_retangulo(jj, l->x + 2, y, l->l - 4, LISTA_ALTURA_ITEM, COR_SELECAO);
        g_texto_n(jj, l->x + 6, y + 2, l->itens[i], (l->l - 12) / LETRA_L, i == l->sel ? COR_BRANCO : COR_TEXTO);
    }
}

static void estado_texto(void)
{
    po_stats_t s;
    po_estadisticas(&po, &s);
    char t[96];
    formatar_texto(t, sizeof t, "%s%s", ruta[0] ? base_de(ruta) : _("(sin archivo)"), modificado ? " *" : "");
    barra_estado_texto(&estado, 0, t);
    int pc = s.total ? s.traducidas * 100 / s.total : 0;
    formatar_texto(t, sizeof t, _("%d cadenas"), s.total);
    barra_estado_texto(&estado, 1, t);
    formatar_texto(t, sizeof t, _("Traducidas: %d%%"), pc);
    barra_estado_texto(&estado, 2, t);
    formatar_texto(t, sizeof t, _("Dudosas: %d"), s.dudosas);
    barra_estado_texto(&estado, 3, t);
    if (barra_msg[0]) copia_texto(t, barra_msg, sizeof t);
    else formatar_texto(t, sizeof t, _("Sin traducir: %d"), s.sin);
    barra_estado_texto(&estado, 4, t);
}

static void linea_info(int y, const char *rot, const char *valor)
{
    if (!valor || !valor[0]) return;
    char *l = a_l1(valor);
    char t[160];
    formatar_texto(t, sizeof t, "%s %s", rot, l ? l : "");
    for (char *q = t; *q; q++) if (*q == '\n') *q = ' ';
    g_texto_n(j, 8, y, t, 106, COR_TEXTO_FRACO);
    if (l) libera(l);
}

static void desenhar_tudo(void)
{
    g_retangulo(j, 0, 0, LARG, ALT, COR_FUNDO);
    barra_menu_desenhar(j, &menu);
    /* filtros */
    static const char *const nomes_filtro[4] = { N_("Todas"), N_("Sin traducir"), N_("Dudosas"), N_("Traducidas") };
    int x = 8;
    for (int i = 0; i < 4; i++) {
        g_relevo(j, x, 26, FILTRO_L[i], 22, i == filtro);
        const char *tx = _(nomes_filtro[i]);
        g_texto(j, x + (FILTRO_L[i] - (int)tam_texto(tx) * LETRA_L) / 2, 31, tx, COR_TEXTO);
        x += FILTRO_L[i] + 4;
    }
    rotulo(j, 560, 31, _("Buscar:"), COR_TEXTO);
    busca.x = 624; busca.y = 28; busca.l = 248;
    caixa_desenhar(j, &busca);
    desenhar_lista(j, &lista);
    /* información de la cadena */
    if (sel >= 0) {
        const po_ent_t *e = &po.e[sel];
        linea_info(222, _("Contexto:"), e->ctxt);
        linea_info(238, _("Referencias:"), e->refs);
        linea_info(254, _("Comentarios:"), e->ext ? e->ext : e->com);
    }
    rotulo(j, 8, 272, _("Original:"), COR_TEXTO);
    if (sel >= 0 && po.e[sel].id_pl) {   /* las formas de plural, como pestañas pequeñas */
        int px = 200;
        for (int k = 0; k < po.e[sel].n_str; k++) {
            char t[8];
            formatar_texto(t, sizeof t, "[%d]", k);
            g_relevo(j, px, 268, 40, 18, k == forma);
            g_texto(j, px + 8, 271, t, COR_TEXTO);
            px += 44;
        }
    }
    area_desenhar(j, &a_orig);
    rotulo(j, 8, 346, _("Traducción:"), COR_TEXTO);
    area_desenhar(j, &a_trad);
    for (int i = 0; i < n_aviso; i++) g_texto_n(j, 8, 430 + i * 16, aviso[i], 106, COR_AVISO);
    if (!n_aviso && sel >= 0) g_texto(j, 8, 430, _("Sin problemas"), COR_OK);
    /* línea de botones */
    g_retangulo(j, 8, 474, 16, 16, COR_BRANCO);
    g_moldura(j, 8, 474, 16, 16, COR_BORDA);
    if (sel >= 0 && po.e[sel].fuzzy) g_texto(j, 12, 474, "x", COR_AVISO);
    g_texto(j, 30, 474, _("Dudosa"), COR_TEXTO);
    boton98(j, &b_copiar);
    boton98(j, &b_mem);
    boton98(j, &b_ia);
    boton98(j, &b_sig);
    rotulo(j, 8, 496, _("Sugerencias (doble clic para usar):"), COR_TEXTO_FRACO);
    desenhar_sug(j, &l_sug);
    estado_texto();
    barra_estado_desenhar(j, &estado);
    jan_mostrar(j);
}

/* ------------------------------------------------------------ acciones */

static void abrir_catalogo(const char *r)
{
    int n = 0;
    char *d = leer_arquivo(r, &n);
    if (!d) { mensagem(_("Traductor"), _("No se pudo leer el archivo.")); return; }
    po_t nuevo;
    po_iniciar(&nuevo);
    char err[96];
    int rc = po_leer(&nuevo, d, n, err, sizeof err);
    libera(d);
    if (rc) {
        char t[200];
        formatar_texto(t, sizeof t, _("El archivo no es un .po válido: %s"), err);
        mensagem(_("Traductor"), t);
        po_liberar(&nuevo);
        return;
    }
    po_liberar(&po);
    po = nuevo;
    copia_texto(ruta, r, sizeof ruta);
    es_pot = termina_en(r, ".pot");
    modificado = false;
    detectar_idioma();
    if (mem_cargada) { mem_cargada = false; po_liberar(&mem); }
    sel = -1; filtro = 0; busca.texto[0] = 0; busca.cursor = 0; lista.topo = 0;
    refrescar_vista();
    seleccionar(n_vista > 0 ? 0 : -1);
    po_stats_t s;
    po_estadisticas(&po, &s);
    registrar("traductor: abierto %s (%d cadenas, %d traducidas, %d dudosas, %d sin traducir, idioma %s%s)", r, s.total,
              s.traducidas, s.dudosas, s.sin, idioma, es_pot ? ", plantilla" : "");
}

static int problemas_totales(void)
{
    int n = 0;
    for (int i = 0; i < po.n; i++) if (po_en_lista(&po.e[i]) && po_validar_ent(&po, &po.e[i])) n++;
    return n;
}

static bool guardar(const char *destino)
{
    if (!destino[0]) return false;
    confirmar_cambio();
    int mal = problemas_totales();
    if (mal) {
        char t[160];
        formatar_texto(t, sizeof t, _("Hay %d cadenas con problemas de validación. ¿Guardar igualmente?"), mal);
        if (!confirmar(_("Traductor"), t)) return false;
    }
    reparar_cabecera();
    int n;
    char *s = po_escribir(&po, &n);
    if (!s) return false;
    bool ok = escribir_arquivo(destino, s, n);
    libera(s);
    if (!ok) { mensagem(_("Traductor"), _("No se pudo guardar el archivo.")); return false; }
    copia_texto(ruta, destino, sizeof ruta);
    modificado = false;
    guardar_memoria();
    registrar("traductor: guardado %s (%d bytes, %d con problemas)", destino, n, mal);
    return true;
}

static void guardar_como(void)
{
    char r[160];
    copia_texto(r, ruta[0] ? ruta : "/home/", sizeof r);
    if (dialogo_guardar(_("Guardar catálogo"), "Catálogos (*.po)|*.po|Todos|*", r, r, sizeof r)) guardar(r);
}

static void abrir_dialogo(void)
{
    char r[160];
    copia_texto(r, ruta[0] ? ruta : "/home/", sizeof r);
    if (dialogo_abrir(_("Abrir catálogo"), "Catálogos (*.po;*.pot)|*.po;*.pot|Todos|*", r, r, sizeof r)) abrir_catalogo(r);
}

/* Nuevo catálogo desde una plantilla .pot, para el idioma que se pida. */
static void nuevo_desde_pot(void)
{
    char r[160] = "/home/";
    if (!dialogo_abrir(_("Plantilla (.pot)"), "Plantillas (*.pot)|*.pot|Todos|*", r, r, sizeof r)) return;
    char lang[IDIOMA_MAX + 8] = "";
    if (!pedir_texto(_("Traductor"), _("Idioma del catálogo nuevo (pt_BR, en, fr...):"), lang, sizeof lang) || !lang[0]) return;
    int n = 0;
    char *d = leer_arquivo(r, &n);
    if (!d) { mensagem(_("Traductor"), _("No se pudo leer el archivo.")); return; }
    po_t pot, nuevo;
    po_iniciar(&pot);
    if (po_leer(&pot, d, n, NULL, 0) != 0) { libera(d); po_liberar(&pot); mensagem(_("Traductor"), _("El archivo no es un .po válido.")); return; }
    libera(d);
    char norm[IDIOMA_MAX];
    idioma_normalizar(lang, norm);
    if (!po_desde_pot(&nuevo, &pot, norm, po_plurales_de(norm))) { po_liberar(&pot); return; }
    po_liberar(&pot);
    po_liberar(&po);
    po = nuevo;
    ruta[0] = 0; es_pot = false; modificado = true;
    copia_texto(idioma, norm, sizeof idioma);
    if (mem_cargada) { mem_cargada = false; po_liberar(&mem); }
    sel = -1; filtro = 0; busca.texto[0] = 0; busca.cursor = 0; lista.topo = 0;
    refrescar_vista();
    seleccionar(n_vista > 0 ? 0 : -1);
    registrar("traductor: nuevo catalogo %s desde %s", idioma, r);
}

/* Como msgmerge: las cadenas nuevas, las parecidas como dudosas y las que desaparecen como obsoletas. */
static void actualizar_desde_pot(void)
{
    if (es_pot) { mensagem(_("Traductor"), _("Esto ya es una plantilla: abre un catálogo .po.")); return; }
    char r[160];
    copia_texto(r, ruta[0] ? ruta : "/home/", sizeof r);
    if (!dialogo_abrir(_("Actualizar desde la plantilla (.pot)"), "Plantillas (*.pot)|*.pot|Todos|*", r, r, sizeof r)) return;
    int n = 0;
    char *d = leer_arquivo(r, &n);
    if (!d) { mensagem(_("Traductor"), _("No se pudo leer el archivo.")); return; }
    po_t pot;
    po_iniciar(&pot);
    char err[96];
    if (po_leer(&pot, d, n, err, sizeof err) != 0) { libera(d); po_liberar(&pot); mensagem(_("Traductor"), _("El archivo no es un .po válido.")); return; }
    libera(d);
    confirmar_cambio();
    po_merge_t m;
    bool ok = po_actualizar(&po, &pot, &m);
    po_liberar(&pot);
    if (!ok) return;
    modificado = true;
    sel = -1;
    refrescar_vista();
    seleccionar(n_vista > 0 ? 0 : -1);
    char t[200];
    formatar_texto(t, sizeof t, _("Actualizado: %d nuevas, %d dudosas, %d obsoletas, %d sin cambios."), m.nuevas, m.dudosas, m.obsoletas, m.iguales);
    registrar("traductor: actualizado desde %s: nuevas %d dudosas %d obsoletas %d iguales %d", r, m.nuevas, m.dudosas, m.obsoletas, m.iguales);
    mensagem(_("Traductor"), t);
}

/* Genera el .cat. Si el sistema usa este idioma, se recarga en caliente (D313: IDIOMA_FIXAR). */
static void generar_cat(void)
{
    if (es_pot) { mensagem(_("Traductor"), _("Una plantilla no genera catálogo.")); return; }
    confirmar_cambio();
    if (!idioma[0]) { mensagem(_("Traductor"), _("No se sabe el idioma del catálogo (falta Language).")); return; }
    int tam, ne, nom;
    uint8_t *c = po_a_cat(&po, &tam, &ne, &nom);
    if (!c) return;
    char nombre[64];
    copia_texto(nombre, ruta[0] ? base_de(ruta) : "catalogo", sizeof nombre);
    int ln = (int)tam_texto(nombre);
    if (ln > 3 && nombre[ln - 3] == '.' && nombre[ln - 2] == 'p' && nombre[ln - 1] == 'o') nombre[ln - 3] = 0;
    char dest[160], dir[96];
    formatar_texto(dir, sizeof dir, "/sistema/idiomas/%s", idioma);
    criar_pasta(dir);
    formatar_texto(dest, sizeof dest, "%s/%s.cat", dir, nombre);
    bool en_sistema = escribir_arquivo(dest, c, tam);
    if (!en_sistema) {   /* sin permiso: junto al .po */
        if (ruta[0]) ruta_cat(ruta, dest, sizeof dest);
        else formatar_texto(dest, sizeof dest, "/home/%s.cat", nombre);
        if (!escribir_arquivo(dest, c, tam)) { libera(c); mensagem(_("Traductor"), _("No se pudo escribir el catálogo.")); return; }
    }
    libera(c);
    bool recargado = false;
    if (en_sistema && compara_texto(idioma_actual(), idioma) == 0) {
        /* El kernel no vuelve a cargar si el idioma no cambia: para su catálogo se pasa por el español. */
        if (compara_texto(nombre, "kernel") == 0) idioma_fijar("es");
        recargado = idioma_fijar(idioma) == 0;
    }
    registrar("traductor: catalogo %s (%d entradas, %d omitidas, %s)", dest, ne, nom, en_sistema ? (recargado ? "recargado" : "en el sistema") : "junto al .po");
    char t[256];
    formatar_texto(t, sizeof t, en_sistema ? (recargado ? _("Catálogo generado (%d cadenas) y recargado: %s") : _("Catálogo generado (%d cadenas): %s. Elige ese idioma en Configuración para verlo."))
                                          : _("Sin permiso para /sistema: catálogo guardado (%d cadenas) en %s"), ne, dest);
    copia_texto(barra_msg, en_sistema ? (recargado ? _("Catálogo recargado") : _("Catálogo generado")) : _("Catálogo guardado aparte"), sizeof barra_msg);
    mensagem(_("Traductor"), t);
}

static void ver_estadisticas(void)
{
    po_stats_t s;
    po_estadisticas(&po, &s);
    char t[400];
    int pc = s.total ? s.traducidas * 100 / s.total : 0;
    formatar_texto(t, sizeof t, _("Cadenas: %d\nTraducidas: %d (%d%%)\nDudosas: %d\nSin traducir: %d\nObsoletas: %d\nCon problemas: %d"),
                   s.total, s.traducidas, pc, s.dudosas, s.sin, s.obsoletas, problemas_totales());
    registrar("traductor: estadisticas total %d traducidas %d dudosas %d sin %d obsoletas %d", s.total, s.traducidas, s.dudosas, s.sin, s.obsoletas);
    mensagem(_("Estadísticas"), t);
}

static void validar_todo(void)
{
    confirmar_cambio();
    int mal = problemas_totales(), h = po_validar_cabecera(&po);
    char t[400];
    formatar_texto(t, sizeof t, _("Cadenas con problemas: %d\nCabecera: %s"), mal, h ? _("incompleta (se corrige al guardar)") : _("correcta"));
    registrar("traductor: validar todo: %d con problemas, cabecera %d", mal, h);
    if (mal) {   /* salta a la primera con problemas */
        for (int i = 0; i < n_vista; i++) if (po_validar_ent(&po, &po.e[vista[i]])) { seleccionar(i); lista.topo = i; break; }
    }
    mensagem(_("Validación"), t);
}

static bool salir_permitido(void)
{
    if (!modificado) return true;
    return confirmar(_("Traductor"), _("Hay cambios sin guardar. ¿Salir sin guardar?"));
}

/* ------------------------------------------------------------ menús */

static char txt_menu[3][420];
static char nom_menu[3][24];

static void armar_menus(void)
{
    formatar_texto(txt_menu[0], sizeof txt_menu[0], "%s\tCtrl+O\n%s\n-\n%s\tCtrl+S\n%s\n-\n%s", _("Abrir..."), _("Nuevo desde plantilla (.pot)..."),
                   _("Guardar"), _("Guardar como..."), _("Salir"));
    formatar_texto(txt_menu[1], sizeof txt_menu[1], "%s\tCtrl+U\n%s\tCtrl+G\n-\n%s\tCtrl+E\n%s\tCtrl+R", _("Actualizar desde el .pot..."),
                   _("Generar catálogo (.cat)"), _("Estadísticas"), _("Validar todo"));
    formatar_texto(txt_menu[2], sizeof txt_menu[2], "%s\tCtrl+D\n%s\tCtrl+N\n%s\tCtrl+B\n-\n%s\tCtrl+L\n%s\tCtrl+Y", _("Dudosa"),
                   _("Siguiente sin traducir"), _("Copiar el original"), _("Sugerencias de la memoria"), _("Sugerencia de la IA"));
    copia_texto(nom_menu[0], _("Archivo"), sizeof nom_menu[0]);
    copia_texto(nom_menu[1], _("Catálogo"), sizeof nom_menu[1]);
    copia_texto(nom_menu[2], _("Cadena"), sizeof nom_menu[2]);
    for (int i = 0; i < 3; i++) { menu.nomes[i] = nom_menu[i]; menu.itens[i] = txt_menu[i]; }
    menu.n = 3;
    barra_menu_iniciar(&menu, 0, 0, LARG);
}

/* Devuelve false para salir. */
static bool accion_menu(int m)
{
    switch (m) {
    case BARRA_MENU_ITEM(0, 0): abrir_dialogo(); break;
    case BARRA_MENU_ITEM(0, 1): nuevo_desde_pot(); break;
    case BARRA_MENU_ITEM(0, 3): if (ruta[0] && !es_pot) guardar(ruta); else guardar_como(); break;
    case BARRA_MENU_ITEM(0, 4): guardar_como(); break;
    case BARRA_MENU_ITEM(0, 6): return !salir_permitido();
    case BARRA_MENU_ITEM(1, 0): actualizar_desde_pot(); break;
    case BARRA_MENU_ITEM(1, 1): generar_cat(); break;
    case BARRA_MENU_ITEM(1, 3): ver_estadisticas(); break;
    case BARRA_MENU_ITEM(1, 4): validar_todo(); break;
    case BARRA_MENU_ITEM(2, 0): marcar_dudosa(); break;
    case BARRA_MENU_ITEM(2, 1): siguiente_sin_traducir(); break;
    case BARRA_MENU_ITEM(2, 2): copiar_original(); break;
    case BARRA_MENU_ITEM(2, 4): sugerencias_memoria(); break;
    case BARRA_MENU_ITEM(2, 5): sugerir_ia(); break;
    default: break;
    }
    return true;
}

/* ------------------------------------------------------------ eventos */

static void cambiar_filtro(int f)
{
    if (f == filtro) return;
    filtro = f;
    refrescar_vista();
    registrar("traductor: filtro %d: %d cadenas", f, n_vista);
}

static void colocar(void)
{
    lista.x = LISTA_X; lista.y = LISTA_Y; lista.l = LISTA_L; lista.a = LISTA_A;
    lista.itens = NULL;
    lista.desenhar = desenhar_lista;
    area_iniciar(&a_orig, 8, 288, 864, 54);
    area_iniciar(&a_trad, 8, 362, 864, 64);
    a_orig.somente_leitura = true;
    l_sug = (lista_t){ .x = 8, .y = 512, .l = 864, .a = 64, .itens = sug_itens, .qtd = 0, .sel = -1 };
    l_sug.desenhar = desenhar_sug;
    b_copiar = (botao_t){ .x = 120, .y = 470, .l = 140, .a = 24, .texto = N_("Copiar original") };
    b_mem = (botao_t){ .x = 268, .y = 470, .l = 120, .a = 24, .texto = N_("Memoria") };
    b_ia = (botao_t){ .x = 396, .y = 470, .l = 120, .a = 24, .texto = N_("Sugerir con IA") };
    b_sig = (botao_t){ .x = 524, .y = 470, .l = 200, .a = 24, .texto = N_("Siguiente sin traducir") };
    static const int la[5] = { 300, 110, 140, 110, 0 };
    barra_estado_iniciar(&estado, 0, ALT - 22, LARG, 5, la);
    memset(&busca, 0, sizeof busca);
}

static bool tratar(const evento_t *e)
{
    if (e->tipo == EV_FECHAR) return salir_permitido() ? false : true;
    int m = barra_menu_tratar(j, &menu, e);
    if (m >= 0) return accion_menu(m);
    /* botones */
    if (boton98_tratar(j, &b_copiar, e)) { copiar_original(); return true; }
    if (boton98_tratar(j, &b_mem, e)) { sugerencias_memoria(); return true; }
    if (boton98_tratar(j, &b_ia, e)) { sugerir_ia(); return true; }
    if (boton98_tratar(j, &b_sig, e)) { siguiente_sin_traducir(); return true; }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        /* filtros */
        int x = 8;
        for (int i = 0; i < 4; i++) {
            if (e->x >= x && e->x < x + FILTRO_L[i] && e->y >= 26 && e->y < 48) { cambiar_filtro(i); return true; }
            x += FILTRO_L[i] + 4;
        }
        /* casilla de dudosa */
        if (e->x >= 8 && e->x < 90 && e->y >= 474 && e->y < 492) { marcar_dudosa(); return true; }
        /* formas de plural: pestañas pequeñas sobre el original */
        if (sel >= 0 && po.e[sel].id_pl && e->y >= 268 && e->y < 286) {
            int px = 200;
            for (int k = 0; k < po.e[sel].n_str; k++) {
                if (e->x >= px && e->x < px + 40) {
                    confirmar_cambio();
                    forma = k;
                    cargar_areas(); validar_actual();
                    return true;
                }
                px += 44;
            }
        }
    }
    /* atajos globales */
    if (e->tipo == EV_TECLA) {
        switch (e->tecla) {
        case 15: abrir_dialogo(); return true;              /* Ctrl+O */
        case 19: if (ruta[0] && !es_pot) guardar(ruta); else guardar_como(); return true;   /* Ctrl+S */
        case 4: marcar_dudosa(); return true;                /* Ctrl+D */
        case 21: actualizar_desde_pot(); return true;        /* Ctrl+U */
        case 7: generar_cat(); return true;                  /* Ctrl+G */
        case 5: ver_estadisticas(); return true;             /* Ctrl+E */
        case 18: validar_todo(); return true;                /* Ctrl+R */
        case 14: siguiente_sin_traducir(); return true;      /* Ctrl+N */
        case 2: copiar_original(); return true;              /* Ctrl+B */
        case 12: sugerencias_memoria(); return true;         /* Ctrl+L */
        case 25: sugerir_ia(); return true;                  /* Ctrl+Y */
        case '1': case '2': case '3':
            if (!a_trad.foco && !busca.foco && l_sug.qtd > 0 && (e->modificadores & 2)) { usar_sugerencia((int)e->tecla - '1'); return true; }
            break;
        default: break;
        }
    }
    /* la caja de búsqueda */
    bool busca_antes = busca.foco;
    char antes[128];
    copia_texto(antes, busca.texto, sizeof antes);
    caixa_tratar(j, &busca, e);
    if (e->tipo == EV_CLIQUE) {
        bool en_busca = busca.foco;
        if (en_busca != busca_antes || en_busca) foco_lista = false;
    }
    if (compara_texto(antes, busca.texto) != 0) { refrescar_vista(); registrar("traductor: busqueda «%s»: %d cadenas", busca.texto, n_vista); return true; }
    if (busca.foco && e->tipo == EV_TECLA) return true;
    /* las sugerencias */
    if (e->tipo == EV_CLIQUE && e->x >= l_sug.x && e->x < l_sug.x + l_sug.l && e->y >= l_sug.y && e->y < l_sug.y + l_sug.a) {
        int k = (e->y - l_sug.y - 2) / LISTA_ALTURA_ITEM;
        if (k >= 0 && k < n_sug) { l_sug.sel = k; if (e->cliques >= 2) usar_sugerencia(k); }
        return true;
    }
    /* las áreas de texto */
    area_tratar(j, &a_orig, e);
    bool cambio = area_tratar(j, &a_trad, e);
    if (cambio) { confirmar_cambio(); return true; }
    if (e->tipo == EV_CLIQUE && (a_trad.foco || a_orig.foco)) { foco_lista = false; return true; }
    /* la lista */
    if (e->tipo == EV_CLIQUE && e->x >= lista.x && e->x < lista.x + lista.l && e->y >= lista.y && e->y < lista.y + lista.a) {
        foco_lista = true; a_trad.foco = a_orig.foco = busca.foco = false;
    }
    if (foco_lista || e->tipo == EV_RODA) {
        int antes_sel = lista.sel;
        int r = lista_tratar(j, &lista, e);
        if (r != LISTA_NADA && lista.sel != antes_sel) { confirmar_cambio(); seleccionar(lista.sel); }
        else if (r != LISTA_NADA && lista.sel == antes_sel && sel < 0) seleccionar(lista.sel);
        if (r == LISTA_ATIVOU) { a_trad.foco = true; foco_lista = false; }
    }
    return true;
}

/* ------------------------------------------------------------ modo línea de órdenes */

static po_t *cargar_cli(const char *r)
{
    int n = 0;
    char *d = leer_arquivo(r, &n);
    if (!d) { imprimir_em(ERROS, "traductor: no se puede leer %s\n", r); return NULL; }
    po_t *p = aloca(sizeof *p);
    po_iniciar(p);
    char err[96];
    int rc = po_leer(p, d, n, err, sizeof err);
    libera(d);
    if (rc) { imprimir_em(ERROS, "traductor: %s: %s\n", r, err); return NULL; }
    return p;
}

static int guardar_cli(const po_t *p, const char *r)
{
    int n;
    char *s = po_escribir(p, &n);
    bool ok = s && escribir_arquivo(r, s, n);
    if (s) libera(s);
    if (!ok) imprimir_em(ERROS, "traductor: no se puede escribir %s\n", r);
    return ok ? 0 : 1;
}

static int modo_cli(int argc, char **argv)
{
    const char *o = argv[1];
    if (compara_texto(o, "--estadisticas") == 0 && argc == 3) {
        po_t *p = cargar_cli(argv[2]);
        if (!p) return 2;
        po_stats_t s;
        po_estadisticas(p, &s);
        imprimir("total %d traducidas %d dudosas %d sin %d obsoletas %d (%d%%)\n", s.total, s.traducidas, s.dudosas, s.sin, s.obsoletas,
                 s.total ? s.traducidas * 100 / s.total : 0);
        return 0;
    }
    if (compara_texto(o, "--validar") == 0 && argc == 3) {
        po_t *p = cargar_cli(argv[2]);
        if (!p) return 2;
        int mal = 0, h = po_validar_cabecera(p);
        for (int i = 0; i < p->n; i++) {
            if (!po_en_lista(&p->e[i])) continue;
            int v = po_validar_ent(p, &p->e[i]);
            if (v) {
                char *l = a_l1(p->e[i].id);
                imprimir("problema %d: %s\n", v, l ? l : "");
                if (l) libera(l);
                mal++;
            }
        }
        if (h) imprimir("cabecera %d\n", h);
        imprimir("validar: %d cadenas con problemas, cabecera %s\n", mal, h ? "incompleta" : "correcta");
        return mal || h ? 1 : 0;
    }
    if (compara_texto(o, "--actualizar") == 0 && argc == 4) {
        po_t *p = cargar_cli(argv[2]), *pot = cargar_cli(argv[3]);
        if (!p || !pot) return 2;
        po_merge_t m;
        if (!po_actualizar(p, pot, &m)) return 1;
        imprimir("actualizado: nuevas %d dudosas %d obsoletas %d iguales %d\n", m.nuevas, m.dudosas, m.obsoletas, m.iguales);
        return guardar_cli(p, argv[2]);
    }
    if (compara_texto(o, "--generar") == 0 && (argc == 3 || argc == 4)) {
        po_t *p = cargar_cli(argv[2]);
        if (!p) return 2;
        int tam, ne, nom;
        uint8_t *c = po_a_cat(p, &tam, &ne, &nom);
        if (!c) return 1;
        char dest[160];
        if (argc == 4) copia_texto(dest, argv[3], sizeof dest);
        else ruta_cat(argv[2], dest, sizeof dest);
        if (!escribir_arquivo(dest, c, tam)) { imprimir_em(ERROS, "traductor: no se puede escribir %s\n", dest); return 1; }
        imprimir("generado %s: %d cadenas, %d omitidas (no caben en Latin-1)\n", dest, ne, nom);
        return 0;
    }
    imprimir_em(ERROS, "uso: traductor [archivo.po]\n"
                       "     traductor --estadisticas A.po | --validar A.po | --actualizar A.po A.pot | --generar A.po [SALIDA.cat]\n");
    return 2;
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == '-') return modo_cli(argc, argv);
    po_iniciar(&po);
    j = jan_abrir(_("Traductor"), LARG, ALT);
    if (!j) return 1;
    colocar();
    armar_menus();
    refrescar_vista();
    if (argc > 1) abrir_catalogo(argv[1]);
    desenhar_tudo();
    registrar("traductor: pronto");
    evento_t e;
    bool seguir = true;
    while (seguir) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        seguir = tratar(&e);
        while (seguir && proximo_evento(j->id, &e, false) == 1) seguir = tratar(&e);
        if (!seguir) break;
        desenhar_tudo();
    }
    registrar("traductor: fechado");
    jan_fechar(j);
    return 0;
}
