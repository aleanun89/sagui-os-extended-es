// SPDX-License-Identifier: GPL-3.0-only
/* ayuda-gui [TEMA | ruta.md[#ancla]] - la Ayuda de SaguiOS (D455): lee desde dentro del sistema los
 * Markdown «educativos» (guías del kernel y de las aplicaciones, terminal, atajos...) y los manuales
 * de los lenguajes de programación.
 *
 * Reutiliza el visor de Markdown (markdown.c, D045): se incluye entero y se usa su maquetación
 * (maquetar_nucleo) y su dibujo de letras; aquí solo cambian la ventana (índice a la izquierda, barra
 * de herramientas), la navegación (enlaces entre documentos, #anclas, atrás y adelante) y la búsqueda.
 *
 *  - El índice se descubre solo: se recorren /ayuda y sus carpetas; cada .md es una entrada. La
 *    categoría y el orden salen de una cabecera opcional en la primera línea,
 *        <!-- categoria: Programación/C; orden: 10; titulo: Corto -->
 *    (categoria/subcategoría), o de la carpeta (guias/ -> «Conoce SaguiOS», lenguajes/ ->
 *    «Programación»), o de la tabla `conocidos` para los archivos de siempre.
 *  - Idioma: si existen X_EN.md o X_PT.md y el sistema está en inglés o portugués, se muestra esa
 *    versión; si no, la española. El botón de idioma cambia entre ES, EN y PT.
 *  - Buscar: en el documento abierto (resalta y salta de una coincidencia a otra) y en toda la ayuda
 *    (lista de resultados en el panel izquierdo).
 *  - Los bloques de código llevan un botón «Copiar» y el tamaño de la letra es 1x o 2x. Los colores
 *    siguen el esquema vigente (D428): con uno oscuro, la luminosidad del documento se invierte.
 *
 * Las imágenes (![texto](ruta.png) en un párrafo propio) se dibujan sin invertir con el esquema oscuro.
 * Pendiente (ver docs/decisoes.md D455): resaltado de la sintaxis. */
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define principal md_principal_visor
#include "markdown.c"
#undef principal

#define MOD_CTRL 2   /* los de kernel/inc/teclado.h (modificadores de EV_TECLA) */
#define MOD_ALT  4

/* ------------------------------------------------------------ idiomas y documentos */

enum { L_ES, L_EN, L_PT };
static const char *const ay_sufijo[3] = { "", "_EN", "_PT" };
static const char *const ay_nombre_lang[3] = { "ES", "EN", "PT" };

#define AY_RAIZ      "/ayuda"
#define AY_MAX_DOCS  400
#define AY_MAX_NODOS 600

typedef struct {
    char     base[96];        /* ruta sin «.md» ni sufijo de idioma: /ayuda/guias/guia-kernel */
    char     titulo[3][64];   /* título (corto) en cada idioma; vacío = no hay esa versión */
    char     cat[40], sub[32];
    int      orden;
} ay_doc_t;

static ay_doc_t *ay_docs;  static int ay_ndocs, ay_cap_docs;
static int ay_lang;

/* Archivos de siempre que no llevan cabecera (se les puede poner una: manda la cabecera). */
static const struct { const char *nome, *cat, *sub; int orden; } ay_conocidos[] = {
    { "guia-kernel", "Conoce SaguiOS", "", 1 }, { "guia-aplicaciones", "Conoce SaguiOS", "", 2 },
    { "ayuda", "Conoce SaguiOS", "", 3 },
    { "terminal", "Conoce SaguiOS", "", 10 }, { "atajos", "Conoce SaguiOS", "", 11 },
    { "archivos", "Conoce SaguiOS", "", 12 }, { "energia", "Conoce SaguiOS", "", 13 },
    { "estilos", "Conoce SaguiOS", "", 14 }, { "sonidos", "Conoce SaguiOS", "", 15 },
    { "markdown", "Conoce SaguiOS", "", 16 }, { "reparar", "Conoce SaguiOS", "", 17 },
    { "escaner", "Conoce SaguiOS", "", 18 }, { "bases-de-datos", "Conoce SaguiOS", "", 19 },
    { "tcc", "Programación", "C", 10 }, { "laboratorio-c", "Programación", "C", 11 },
    { "pascal", "Programación", "Pascal", 10 },
    { "python-tk", "Programación", "Python", 10 }, { "visualpythons", "Programación", "Python", 11 },
    { "visualpythons-manual", "Programación", "Python", 12 },
    { "lua", "Programación", "Lua", 10 }, { "awk", "Programación", "awk", 10 },
    { "make", "Programación", "Make", 10 }, { "git", "Programación", "Git", 10 },
    { "opengl-es", "Programación", "OpenGL ES", 10 },
    { "mico", "Juegos y aplicaciones", "", 10 }, { "llm", "Juegos y aplicaciones", "", 11 },
    { "jabuti", "Juegos y aplicaciones", "", 12 }, { "chip8", "Juegos y aplicaciones", "", 20 },
    { "doom", "Juegos y aplicaciones", "", 21 }, { "tzar", "Juegos y aplicaciones", "", 22 },
    { "aoen", "Juegos y aplicaciones", "", 23 }, { "modificar-aoen", "Juegos y aplicaciones", "", 24 },
    { "brumacanto", "Juegos y aplicaciones", "", 25 },
};

static int ay_min(int c)
{
    c &= 255;
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    return c;
}

/* Posición de q (de qn letras) dentro de s (de n), sin distinguir mayúsculas; -1 si no está. */
static int ay_acha(const char *s, int n, const char *q, int qn)
{
    if (qn <= 0) return -1;
    for (int i = 0; i + qn <= n; i++) {
        int k = 0;
        while (k < qn && ay_min((unsigned char)s[i + k]) == ay_min((unsigned char)q[k])) k++;
        if (k == qn) return i;
    }
    return -1;
}

static bool ay_termina(const char *s, const char *suf)
{
    int a = (int)tam_texto(s), b = (int)tam_texto(suf);
    return a >= b && compara_texto(s + a - b, suf) == 0;
}

static bool ay_existe(const char *c)
{
    info_arquivo_t inf;
    return info_arquivo(c, &inf) >= 0;
}

/* «/ayuda/x» + idioma -> «/ayuda/x_EN.md» (o la española si esa versión no existe). */
static void ay_ruta(const char *base, int lang, char *dst, size_t max)
{
    if (lang != L_ES) {
        formatar_texto(dst, max, "%s%s.md", base, ay_sufijo[lang]);
        if (ay_existe(dst)) return;
    }
    formatar_texto(dst, max, "%s.md", base);
}

static int ay_buscar_base(const char *base)
{
    for (int i = 0; i < ay_ndocs; i++)
        if (compara_texto(ay_docs[i].base, base) == 0) return i;
    return -1;
}

/* Lee el principio de un archivo (Latin-1, terminado en cero). */
static int ay_cabeza(const char *c, char *buf, int max)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return -1;
    int n = ler(fd, buf, (unsigned)(max - 1));
    fechar(fd);
    if (n < 0) n = 0;
    n = (int)utf8_para_latin1(buf, (size_t)n, NULL);
    buf[n] = 0;
    return n;
}

/* Valor de «clave:» dentro de una cabecera <!-- ... --> (hasta ';' o '-->'), sin espacios. */
static bool ay_clave(const char *cab, const char *clave, char *dst, int max)
{
    const char *p = cab;
    int kn = (int)tam_texto(clave);
    for (;;) {
        const char *q = p;
        while (*q && !(compara_texto_n(q, clave, (size_t)kn) == 0 && q[kn] == ':')) q++;
        if (!*q) return false;
        if (q > cab && q[-1] != ' ' && q[-1] != ';' && q[-1] != '\n' && q[-1] != '-') { p = q + 1; continue; }
        q += kn + 1;
        while (*q == ' ') q++;
        int n = 0;
        while (*q && *q != ';' && *q != '\n' && !(q[0] == '-' && q[1] == '-' && q[2] == '>') && n < max - 1) dst[n++] = *q++;
        while (n > 0 && dst[n - 1] == ' ') n--;
        dst[n] = 0;
        return true;
    }
}

/* Título corto: la primera línea «# ...» sin `*_ y sin lo que va tras «:» (lua: el lenguaje... -> lua). */
static void ay_titulo_de(const char *cab, char *dst, int max)
{
    dst[0] = 0;
    const char *p = cab;
    while (*p) {
        if (p[0] == '#' && p[1] == ' ') {
            p += 2;
            int n = 0;
            while (*p && *p != '\n' && n < max - 1) {
                if (*p != '`' && *p != '*') dst[n++] = *p;
                p++;
            }
            while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '\r')) n--;
            dst[n] = 0;
            char *dp = acha_char(dst, ':');
            if (dp && dp - dst >= 2 && dp[1] == ' ') *dp = 0;
            return;
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

static void ay_registrar_archivo(const char *dir, const char *nome)
{
    int lang = L_ES, n = (int)tam_texto(nome);
    char raiz[64];
    if (n < 4 || compara_texto(nome + n - 3, ".md") != 0) return;
    copia_texto(raiz, nome, sizeof raiz);
    raiz[n - 3] = 0;
    if (ay_termina(raiz, "_EN")) { lang = L_EN; raiz[tam_texto(raiz) - 3] = 0; }
    else if (ay_termina(raiz, "_PT")) { lang = L_PT; raiz[tam_texto(raiz) - 3] = 0; }
    char base[96], ruta[120];
    formatar_texto(base, sizeof base, "%s/%s", dir, raiz);
    formatar_texto(ruta, sizeof ruta, "%s%s.md", base, ay_sufijo[lang]);
    char cab[1536];
    if (ay_cabeza(ruta, cab, sizeof cab) < 0) return;
    int d = ay_buscar_base(base);
    if (d < 0) {
        if (lang != L_ES) {   /* una variante sin la española no es un documento */
            char es[120];
            formatar_texto(es, sizeof es, "%s.md", base);
            if (!ay_existe(es)) return;
        }
        ay_docs = crescer(ay_docs, &ay_cap_docs, ay_ndocs + 1, sizeof(ay_doc_t));
        d = ay_ndocs++;
        memset(&ay_docs[d], 0, sizeof(ay_doc_t));
        copia_texto(ay_docs[d].base, base, sizeof ay_docs[d].base);
        ay_docs[d].orden = 500;
    }
    ay_doc_t *x = &ay_docs[d];
    char corto[64] = "";
    ay_clave(cab, "titulo", corto, sizeof corto);
    if (!corto[0]) ay_titulo_de(cab, corto, sizeof corto);
    if (!corto[0]) copia_texto(corto, raiz, sizeof corto);
    copia_texto(x->titulo[lang], corto, sizeof x->titulo[lang]);
    if (lang != L_ES) return;
    /* categoría: cabecera > tabla de siempre > carpeta */
    char cat[72] = "";
    if (ay_clave(cab, "categoria", cat, sizeof cat) && cat[0]) {
        char *b = acha_char(cat, '/');
        if (b) { *b++ = 0; copia_texto(x->sub, b, sizeof x->sub); }
        copia_texto(x->cat, cat, sizeof x->cat);
    } else {
        bool hallado = false;
        for (unsigned k = 0; k < sizeof ay_conocidos / sizeof ay_conocidos[0]; k++)
            if (compara_texto(ay_conocidos[k].nome, raiz) == 0) {
                copia_texto(x->cat, ay_conocidos[k].cat, sizeof x->cat);
                copia_texto(x->sub, ay_conocidos[k].sub, sizeof x->sub);
                x->orden = ay_conocidos[k].orden;
                hallado = true;
                break;
            }
        if (!hallado) {
            bool lenguajes = ay_termina(dir, "/lenguajes");
            copia_texto(x->cat, lenguajes ? "Programación" : "Conoce SaguiOS", sizeof x->cat);
        }
    }
    char ord[16];
    if (ay_clave(cab, "orden", ord, sizeof ord)) x->orden = texto_para_int(ord);
}

static void ay_recorrer(const char *dir, int prof)
{
    entrada_pasta_t *v = NULL;
    int n = listar_tudo(dir, &v);
    if (n <= 0) return;
    for (int i = 0; i < n; i++) {
        if (v[i].nome[0] == '.') continue;
        if (v[i].tipo == TIPO_PASTA) {
            if (prof < 3) {
                char sub[96];
                formatar_texto(sub, sizeof sub, "%s/%s", dir, v[i].nome);
                ay_recorrer(sub, prof + 1);
            }
        } else if (v[i].tipo == TIPO_ARQUIVO) ay_registrar_archivo(dir, v[i].nome);
    }
    libera(v);
}

/* ------------------------------------------------------------ el árbol del índice */

enum { N_CAT, N_SUB, N_DOC };
typedef struct {
    uint8_t tipo, nivel;
    bool    abierto;
    int     doc;            /* N_DOC: índice en ay_docs; N_SUB con un solo documento: también (se comporta como hoja) */
    int     padre;
    char    clave[40];      /* categoría o subcategoría sin traducir */
} ay_nodo_t;

static ay_nodo_t ay_nodos[AY_MAX_NODOS];
static int ay_nnodos;

/* Nombres de categoría que salen en el índice (para el extractor de traducciones: se traducen con _() al dibujar). */
static const char *const ay_cats_nombres[] __attribute__((unused)) = { N_("Conoce SaguiOS"), N_("Programación"), N_("Juegos y aplicaciones") };

static int ay_rango_cat(const char *c)
{
    if (compara_texto(c, "Conoce SaguiOS") == 0) return 0;
    if (compara_texto(c, "Programación") == 0) return 1;
    if (compara_texto(c, "Juegos y aplicaciones") == 0) return 2;
    return 3;
}

static int ay_comp_ci(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = ay_min((unsigned char)*a), y = ay_min((unsigned char)*b);
        if (x != y) return x - y;
        if (!x) return 0;
    }
}

static int ay_comparar(const ay_doc_t *a, const ay_doc_t *b)
{
    int d = ay_rango_cat(a->cat) - ay_rango_cat(b->cat);
    if (d) return d;
    d = ay_comp_ci(a->cat, b->cat);
    if (d) return d;
    if ((a->sub[0] != 0) != (b->sub[0] != 0)) return a->sub[0] ? 1 : -1;   /* primero los sueltos de la categoría */
    d = ay_comp_ci(a->sub, b->sub);
    if (d) return d;
    if (a->orden != b->orden) return a->orden - b->orden;
    return ay_comp_ci(a->titulo[L_ES], b->titulo[L_ES]);
}

/* Reordena los documentos (inserción: son unos cientos) y arma los nodos. */
static void ay_armar_arbol(void)
{
    for (int i = 1; i < ay_ndocs; i++) {
        ay_doc_t t = ay_docs[i];
        int k = i - 1;
        while (k >= 0 && ay_comparar(&ay_docs[k], &t) > 0) { ay_docs[k + 1] = ay_docs[k]; k--; }
        ay_docs[k + 1] = t;
    }
    ay_nnodos = 0;
    int cat_n = -1, sub_n = -1;
    for (int i = 0; i < ay_ndocs && ay_nnodos + 3 < AY_MAX_NODOS; i++) {
        ay_doc_t *d = &ay_docs[i];
        if (cat_n < 0 || compara_texto(ay_nodos[cat_n].clave, d->cat) != 0) {
            cat_n = ay_nnodos++;
            ay_nodos[cat_n] = (ay_nodo_t){ N_CAT, 0, true, -1, -1, "" };
            copia_texto(ay_nodos[cat_n].clave, d->cat, sizeof ay_nodos[cat_n].clave);
            sub_n = -1;
        }
        if (!d->sub[0]) {
            ay_nodos[ay_nnodos++] = (ay_nodo_t){ N_DOC, 1, false, i, cat_n, "" };
            sub_n = -1;
            continue;
        }
        if (sub_n < 0 || compara_texto(ay_nodos[sub_n].clave, d->sub) != 0) {
            sub_n = ay_nnodos++;
            ay_nodos[sub_n] = (ay_nodo_t){ N_SUB, 1, false, i, cat_n, "" };
            copia_texto(ay_nodos[sub_n].clave, d->sub, sizeof ay_nodos[sub_n].clave);
        } else ay_nodos[sub_n].doc = -2;   /* más de un documento: es un grupo de verdad */
        ay_nodos[ay_nnodos++] = (ay_nodo_t){ N_DOC, 2, false, i, sub_n, "" };
    }
}

/* Un grupo con un solo documento se comporta como una hoja (y su documento no se muestra aparte). */
static bool ay_sub_hoja(int n) { return ay_nodos[n].tipo == N_SUB && ay_nodos[n].doc >= 0; }

static bool ay_nodo_visible(int n)
{
    int p = ay_nodos[n].padre;
    while (p >= 0) {
        if (!ay_nodos[p].abierto) return false;
        p = ay_nodos[p].padre;
    }
    if (ay_nodos[n].tipo == N_DOC && ay_nodos[n].padre >= 0 && ay_sub_hoja(ay_nodos[n].padre)) return false;
    return true;
}

static const char *ay_etiqueta(int n, char *buf, int max)
{
    ay_nodo_t *x = &ay_nodos[n];
    if (x->tipo == N_CAT) return _(x->clave);
    if (x->tipo == N_SUB) return x->clave;
    ay_doc_t *d = &ay_docs[x->doc];
    const char *t = d->titulo[ay_lang][0] ? d->titulo[ay_lang] : d->titulo[L_ES];
    copia_texto(buf, t, (size_t)max);
    return buf;
}

/* ------------------------------------------------------------ estado */

#define AY_IND_FILA 20
#define AY_IND_RES  36

static char ay_cam[140];           /* el documento abierto */
static int  ay_doc_act = -1;       /* su índice en ay_docs (o -1) */
static int  ay_zoom = 1;
static int  ay_desl;               /* desplazamiento del visor, en píxeles de pantalla */
static char ay_aviso[160];
static int  ay_ind_desl, ay_sel_nodo = -1;
static int  ay_ver, ay_ver_maq = -1, ay_larg_maq = -1, ay_zoom_maq;
static campo_busca_t ay_campo;
static botao_t ay_bt[5];          /* atrás, adelante, A-, A+, idioma */
static botao_t ay_bt_ant, ay_bt_sig, ay_bt_todo;

/* coincidencias en el documento */
typedef struct { int el, off, n; } ay_coin_t;
static ay_coin_t *ay_coin;  static int ay_ncoin, ay_cap_coin, ay_icoin = -1;
static char ay_consulta[128];

/* resultados de la búsqueda en toda la ayuda */
typedef struct { int doc; int linea; char texto[84]; } ay_res_t;
static ay_res_t *ay_res;  static int ay_nres, ay_cap_res;
static bool ay_modo_res;
static char ay_consulta_res[128];

/* historial */
#define AY_HIST 40
static struct { char cam[140]; int desl; } ay_hist[AY_HIST];
static int ay_hn, ay_hi = -1;

static bool ay_escuro(void)
{
    uint32_t c = COR_CAMPO;
    int l = (int)((c >> 16) & 255) * 3 + (int)((c >> 8) & 255) * 6 + (int)(c & 255);
    return l < 1280;
}

/* Color del documento: tal cual con un esquema claro; con uno oscuro se invierte la luminosidad. */
static uint32_t ay_cor(uint32_t c)
{
    if (!ay_escuro() || c == (uint32_t)COR_TEXTO || c == (uint32_t)COR_TEXTO_FRACO) return c;
    int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int d = 255 - (mx + mn);
    r += d; g += d; b += d;
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint32_t)(r << 16 | g << 8 | b);
}

static int ay_izq(void) { return LARG < 700 ? 190 : 240; }

static void ay_geom_visor(painel_t *p)
{
    int il = ay_izq() + 1;
    *p = (painel_t){ il, BARRA, LARG - il, ALT - BARRA - RODAPE };
}

static int ay_ancho_unid(void)
{
    painel_t p;
    ay_geom_visor(&p);
    return (p.l - ROLAGEM) / ay_zoom;
}

static void ay_rehacer_coincidencias(void);

static void ay_maquetar(void)
{
    int w = ay_ancho_unid();
    if (ay_ver_maq == ay_ver && ay_larg_maq == w && ay_zoom_maq == ay_zoom) return;
    mt = false; CW = LETRA_L; GAP = 8;
    reg_titulos = false;
    ruta_doc = ay_cam;
    maquetar_nucleo(txt, tam, MARGEM, w - MARGEM);
    ay_ver_maq = ay_ver; ay_larg_maq = w; ay_zoom_maq = ay_zoom;
    ay_rehacer_coincidencias();   /* las coincidencias dependen de la maquetación */
    /* dónde empiezan los primeros enlaces y bloques de código (para las pruebas) */
    for (int k = 0, ult = -1, vistos = 0; k < nels && vistos < 12; k++)
        if (els[k].tipo == T_TEXTO && els[k].enlace > ult) {
            ult = els[k].enlace;
            char u[80];
            int n = enls[ult].n < 79 ? enls[ult].n : 79;
            memcpy(u, pool + enls[ult].off, (size_t)n);
            u[n] = 0;
            registrar("ayuda: enlace-en %s en %d,%d\n", u, els[k].x, els[k].y);
            vistos++;
        }
    for (int b = 0; b < nbcods && b < 4; b++) registrar("ayuda: bloque %d en %d,%d,%d\n", b, bcods[b].x, bcods[b].y, bcods[b].l);
}

static int ay_alto_doc(void) { return doc_alt * ay_zoom; }

static void ay_limitar_desl(void)
{
    painel_t p;
    ay_geom_visor(&p);
    int max = ay_alto_doc() - p.a;
    if (ay_desl > max) ay_desl = max;
    if (ay_desl < 0) ay_desl = 0;
}

/* ------------------------------------------------------------ coincidencias en el documento */

static void ay_rehacer_coincidencias(void)
{
    ay_ncoin = 0;
    int qn = (int)tam_texto(ay_consulta);
    if (qn == 0) { ay_icoin = -1; return; }
    for (int k = 0; k < nels; k++) {
        elem_t *e = &els[k];
        if (e->tipo != T_TEXTO) continue;
        int pos = 0;
        while (pos < e->n) {
            int r = ay_acha(pool + e->off + pos, e->n - pos, ay_consulta, qn);
            if (r < 0) break;
            ay_coin = crescer(ay_coin, &ay_cap_coin, ay_ncoin + 1, sizeof(ay_coin_t));
            ay_coin[ay_ncoin++] = (ay_coin_t){ k, pos + r, qn };
            pos += r + qn;
            if (ay_ncoin >= 2000) return;
        }
    }
    if (ay_icoin >= ay_ncoin) ay_icoin = ay_ncoin - 1;
}

static void ay_ir_coincidencia(int i)
{
    if (ay_ncoin == 0) { ay_icoin = -1; return; }
    ay_icoin = (i % ay_ncoin + ay_ncoin) % ay_ncoin;
    painel_t p;
    ay_geom_visor(&p);
    int y = els[ay_coin[ay_icoin].el].y * ay_zoom;
    if (y < ay_desl + 20 || y > ay_desl + p.a - 40) ay_desl = y - p.a / 3;
    ay_limitar_desl();
}

/* ------------------------------------------------------------ cargar un documento */

static void ay_titulo_ventana(void)
{
    char t[160];
    const char *n = ay_doc_act >= 0 ? (ay_docs[ay_doc_act].titulo[ay_lang][0] ? ay_docs[ay_doc_act].titulo[ay_lang]
                                                                              : ay_docs[ay_doc_act].titulo[L_ES]) : nome_base(ay_cam);
    formatar_texto(t, sizeof t, "%s - %s", _("Ayuda"), n);
    titulo_janela(j->id, t);
}

static void ay_abrir_ancestros(int n)
{
    for (int p = ay_nodos[n].padre; p >= 0; p = ay_nodos[p].padre) ay_nodos[p].abierto = true;
}

static int ay_nodo_de_doc(int d)
{
    for (int n = 0; n < ay_nnodos; n++)
        if (ay_nodos[n].tipo == N_DOC && ay_nodos[n].doc == d) {
            if (ay_nodos[n].padre >= 0 && ay_sub_hoja(ay_nodos[n].padre)) return ay_nodos[n].padre;
            return n;
        }
    return -1;
}

static void ay_slug(const char *s, int n, char *dst, int max)
{
    int k = 0;
    for (int i = 0; i < n && k < max - 1; i++) {
        int c = ay_min((unsigned char)s[i]);
        if (c == ' ' || c == '-') dst[k++] = '-';
        else if (c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c >= 0xC0) dst[k++] = (char)c;
    }
    dst[k] = 0;
}

static bool ay_ir_ancla(const char *ancla)
{
    if (!ancla || !ancla[0]) return false;
    char a[96];
    ay_slug(ancla, (int)tam_texto(ancla), a, sizeof a);
    for (int i = 0; i < ntits; i++) {
        char s[130];
        ay_slug(tit_pool + tits[i].off, tits[i].n, s, sizeof s);
        if (compara_texto(s, a) == 0) {
            ay_desl = (tits[i].y - 6) * ay_zoom;
            ay_limitar_desl();
            return true;
        }
    }
    return false;
}

/* Carga el archivo en el visor. «ancla»: el título al que saltar (o NULL). */
static bool ay_cargar(const char *ruta, const char *ancla, int desl_prev)
{
    int n, erro = 0;
    char *buf = ler_arquivo(ruta, &n, &erro);
    if (!buf) {
        formatar_texto(ay_aviso, sizeof ay_aviso, _("No se puede abrir %s (%s)"), ruta, nome_erro(erro));
        return false;
    }
    txt = crescer(txt, &cap_txt, n + 1, 1);
    memcpy(txt, buf, (size_t)n);
    txt[n] = 0;
    tam = n;
    libera(buf);
    copia_texto(ay_cam, ruta, sizeof ay_cam);
    ay_ver++;
    ay_aviso[0] = 0;
    /* el documento y su idioma */
    char base[140];
    copia_texto(base, ruta, sizeof base);
    base[tam_texto(base) - 3] = 0;
    if (ay_termina(base, "_EN")) { base[tam_texto(base) - 3] = 0; ay_lang = L_EN; }
    else if (ay_termina(base, "_PT")) { base[tam_texto(base) - 3] = 0; ay_lang = L_PT; }
    ay_doc_act = ay_buscar_base(base);
    ay_desl = desl_prev;
    ay_icoin = -1;
    ay_maquetar();
    if (ancla && ancla[0] && !ay_ir_ancla(ancla)) formatar_texto(ay_aviso, sizeof ay_aviso, _("No se encuentra el apartado #%s"), ancla);
    ay_limitar_desl();
    if (ay_doc_act >= 0) {
        int nn = ay_nodo_de_doc(ay_doc_act);
        if (nn >= 0) { ay_abrir_ancestros(nn); ay_sel_nodo = nn; }
    }
    if (ay_consulta[0] && ay_ncoin > 0 && !(ancla && ancla[0])) ay_ir_coincidencia(0);
    ay_titulo_ventana();
    registrar("ayuda: abierto %s (%d bytes, %d titulos, %d bloques de codigo, idioma %s)\n", ruta, n, ntits, nbcods, ay_nombre_lang[ay_lang]);
    return true;
}

static void ay_historial_actual(void)
{
    if (ay_hi >= 0) ay_hist[ay_hi].desl = ay_desl;
}

/* Abre «ruta» (con #ancla opcional) y la añade al historial. */
static bool ay_abrir(const char *ruta, const char *ancla, bool al_historial)
{
    ay_historial_actual();
    if (!ay_cargar(ruta, ancla, 0)) return false;
    if (al_historial) {
        if (ay_hi >= 0 && compara_texto(ay_hist[ay_hi].cam, ruta) == 0) {}   /* el mismo documento (otra ancla): no se repite */
        else {
            if (ay_hi + 1 >= AY_HIST) { memmove(ay_hist, ay_hist + 1, sizeof ay_hist[0] * (AY_HIST - 1)); ay_hi--; }
            ay_hi++;
            ay_hn = ay_hi + 1;
        }
        copia_texto(ay_hist[ay_hi].cam, ruta, sizeof ay_hist[ay_hi].cam);
        ay_hist[ay_hi].desl = ay_desl;
    }
    return true;
}

static void ay_navegar(int delta)
{
    int ni = ay_hi + delta;
    if (ni < 0 || ni >= ay_hn) return;
    ay_historial_actual();
    if (ay_cargar(ay_hist[ni].cam, NULL, ay_hist[ni].desl)) {
        ay_hi = ni;
        registrar("ayuda: historial %s %s\n", delta < 0 ? "atras" : "adelante", ay_cam);
    }
}

/* ------------------------------------------------------------ enlaces */

static void ay_normalizar(const char *dest, char *norm, int max)
{
    int nn = 0;
    norm[0] = 0;
    const char *p = dest;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *f = p;
        while (*f && *f != '/') f++;
        int k = (int)(f - p);
        if (k == 1 && p[0] == '.') {}
        else if (k == 2 && p[0] == '.' && p[1] == '.') { while (nn > 0 && norm[nn - 1] != '/') nn--; if (nn > 0) nn--; }
        else if (nn + k + 2 < max) { norm[nn++] = '/'; memcpy(norm + nn, p, (size_t)k); nn += k; }
        norm[nn] = 0;
        p = f;
    }
    if (!nn) copia_texto(norm, "/", (size_t)max);
}

static void ay_seguir_enlace(int idx)
{
    char url[200];
    int n = enls[idx].n < 199 ? enls[idx].n : 199;
    memcpy(url, pool + enls[idx].off, (size_t)n);
    url[n] = 0;
    registrar("ayuda: enlace %s\n", url);
    char ancla[96] = "";
    char *h = acha_char(url, '#');
    if (h) { copia_texto(ancla, h + 1, sizeof ancla); *h = 0; }
    int u = (int)tam_texto(url);
    if (u == 0) {   /* #ancla del mismo documento */
        if (ay_ir_ancla(ancla)) { ay_historial_actual(); registrar("ayuda: ancla %s\n", ancla); }
        else formatar_texto(ay_aviso, sizeof ay_aviso, _("No se encuentra el apartado #%s"), ancla);
        return;
    }
    if (acha_char(url, ':') != NULL) {
        formatar_texto(ay_aviso, sizeof ay_aviso, _("Enlace externo (no se abre): %s"), url);
        return;
    }
    char dest[256];
    if (url[0] == '/') copia_texto(dest, url, sizeof dest);
    else {
        copia_texto(dest, ay_cam, sizeof dest);
        char *b = acha_ultimo_char(dest, '/');
        if (b) b[1] = 0; else dest[0] = 0;
        junta_texto(dest, url, sizeof dest);
    }
    char norm[256];
    ay_normalizar(dest, norm, sizeof norm);
    if (!ay_termina(norm, ".md")) {
        formatar_texto(ay_aviso, sizeof ay_aviso, _("No es un documento de la ayuda: %s"), url);
        return;
    }
    /* un enlace a «x_PT.md» elige ese idioma; uno a «x.md» sigue el idioma vigente */
    char base[256];
    copia_texto(base, norm, sizeof base);
    base[tam_texto(base) - 3] = 0;
    int lang = ay_lang;
    if (ay_termina(base, "_EN")) { lang = L_EN; base[tam_texto(base) - 3] = 0; }
    else if (ay_termina(base, "_PT")) { lang = L_PT; base[tam_texto(base) - 3] = 0; }
    else if (ay_termina(base, "_ES")) base[tam_texto(base) - 3] = 0;
    char ruta[270];
    if (lang != ay_lang) {   /* el enlace de idioma de las guías */
        formatar_texto(ruta, sizeof ruta, "%s%s.md", base, ay_sufijo[lang]);
        if (!ay_existe(ruta)) formatar_texto(ruta, sizeof ruta, "%s.md", base);
    } else ay_ruta(base, lang, ruta, sizeof ruta);
    if (!ay_existe(ruta)) {
        formatar_texto(ay_aviso, sizeof ay_aviso, _("No está en la ayuda: %s"), url);
        return;
    }
    ay_abrir(ruta, ancla, true);
}

/* ------------------------------------------------------------ búsqueda en toda la ayuda */

static void ay_buscar_todo(const char *q)
{
    int qn = (int)tam_texto(q);
    ay_nres = 0;
    int docs_con = 0;
    copia_texto(ay_consulta_res, q, sizeof ay_consulta_res);
    if (qn == 0) { ay_modo_res = false; return; }
    for (int d = 0; d < ay_ndocs && ay_nres < 150; d++) {
        char ruta[200];
        ay_ruta(ay_docs[d].base, ay_lang, ruta, sizeof ruta);
        int n, erro = 0;
        char *buf = ler_arquivo(ruta, &n, &erro);
        if (!buf) continue;
        int linea = 0, en_doc = 0, ini = 0;
        for (int i = 0; i <= n && en_doc < 4 && ay_nres < 150; i++) {
            if (i < n && buf[i] != '\n') continue;
            linea++;
            int r = ay_acha(buf + ini, i - ini, q, qn);
            if (r >= 0) {
                const char *s = buf + ini;
                int len = i - ini;
                while (len > 0 && (*s == ' ' || *s == '#' || *s == '-' || *s == '*' || *s == '>' || *s == '|')) { s++; len--; r--; }
                if (r > 30) { s += r - 30; len -= r - 30; }
                if (len > 80) len = 80;
                ay_res = crescer(ay_res, &ay_cap_res, ay_nres + 1, sizeof(ay_res_t));
                ay_res[ay_nres].doc = d;
                ay_res[ay_nres].linea = linea;
                memcpy(ay_res[ay_nres].texto, s, (size_t)len);
                ay_res[ay_nres].texto[len] = 0;
                ay_nres++;
                en_doc++;
            }
            ini = i + 1;
        }
        if (en_doc) docs_con++;
        libera(buf);
    }
    ay_modo_res = true;
    ay_ind_desl = 0;
    registrar("ayuda: busqueda global '%s' %d resultados en %d documentos\n", q, ay_nres, docs_con);
}

/* ------------------------------------------------------------ dibujo */

static void ay_dib_barra(void)
{
    g_retangulo(j, 0, 0, LARG, BARRA, COR_FUNDO);
    g_retangulo(j, 0, BARRA - 1, LARG, 1, COR_BORDA);
    int x = 8;
    ay_bt[0] = (botao_t){ x, 6, 28, 24, "<", false }; x += 32;
    ay_bt[1] = (botao_t){ x, 6, 28, 24, ">", false }; x += 36;
    ay_bt[2] = (botao_t){ x, 6, 30, 24, "A-", false }; x += 32;
    ay_bt[3] = (botao_t){ x, 6, 30, 24, "A+", false }; x += 36;
    ay_bt[4] = (botao_t){ x, 6, 36, 24, ay_nombre_lang[ay_lang], false }; x += 44;
    int fin = LARG - 8;
    const char *s_todo = _("Todo"), *s_ant = _("Ant."), *s_sig = _("Sig.");
    int wt = g_largura_texto(s_todo) + 16, wa = g_largura_texto(s_ant) + 16, ws = g_largura_texto(s_sig) + 16;
    ay_bt_todo = (botao_t){ fin - wt, 6, wt, 24, s_todo, false }; fin -= wt + 4;
    ay_bt_sig = (botao_t){ fin - ws, 6, ws, 24, s_sig, false }; fin -= ws + 4;
    ay_bt_ant = (botao_t){ fin - wa, 6, wa, 24, s_ant, false }; fin -= wa + 8;
    for (int i = 0; i < 5; i++) botao_desenhar(j, &ay_bt[i]);
    botao_desenhar(j, &ay_bt_ant); botao_desenhar(j, &ay_bt_sig); botao_desenhar(j, &ay_bt_todo);
    ay_campo.c.x = x; ay_campo.c.y = 5; ay_campo.c.l = fin - x;
    ay_campo.a = 26;
    ay_campo.dica = _("Buscar en la ayuda");
    ay_campo.fundo = COR_FUNDO;
    campo_busca_desenhar(j, &ay_campo);
    jan_mostrar_area(j, 0, 0, LARG, BARRA);
}

static void ay_triangulo(int x, int y, bool abierto, uint32_t cor)
{
    for (int k = 0; k < 5; k++) {
        if (abierto) g_retangulo(j, x + k, y + k, 9 - 2 * k, 1, cor);
        else g_retangulo(j, x + k, y + k - 0, 1, 9 - 2 * k, cor);
    }
}

/* Los nodos visibles, en orden; devuelve cuántos. */
static int ay_filas(int *v, int max)
{
    int c = 0;
    for (int n = 0; n < ay_nnodos && c < max; n++)
        if (ay_nodo_visible(n)) v[c++] = n;
    return c;
}

static void ay_dib_indice(void)
{
    int w = ay_izq(), top = BARRA, a = ALT - BARRA - RODAPE;
    clip = (area_tela_t){ 0, top, w, a };
    g_retangulo(j, 0, top, w, a, COR_LISTA_FUNDO);
    g_retangulo(j, w, top, 1, a, COR_BORDA);
    if (ay_modo_res) {
        g_retangulo(j, 0, top, w, 26, COR_FUNDO);
        char t[96];
        formatar_texto(t, sizeof t, "< %s (%d)", _("Índice"), ay_nres);
        g_texto(j, 8, top + 5, t, COR_ENLACE);
        int y = top + 28 - ay_ind_desl;
        for (int i = 0; i < ay_nres; i++, y += AY_IND_RES) {
            if (y + AY_IND_RES < top + 26 || y > top + a) continue;
            const char *tt = ay_docs[ay_res[i].doc].titulo[ay_lang][0] ? ay_docs[ay_res[i].doc].titulo[ay_lang] : ay_docs[ay_res[i].doc].titulo[L_ES];
            int cab = (w - 12) / LETRA_L;
            g_texto_n(j, 8, y + 2, tt, (int)tam_texto(tt) < cab ? (int)tam_texto(tt) : cab, COR_TEXTO);
            g_texto_n(j, 8, y + 18, ay_res[i].texto, (int)tam_texto(ay_res[i].texto) < cab ? (int)tam_texto(ay_res[i].texto) : cab, COR_TEXTO_FRACO);
            g_retangulo(j, 4, y + AY_IND_RES - 1, w - 8, 1, COR_BORDA);
        }
        jan_mostrar_area(j, 0, top, w + 1, a);
        return;
    }
    int filas[AY_MAX_NODOS];
    int nf = ay_filas(filas, AY_MAX_NODOS);
    int y = top + 4 - ay_ind_desl;
    for (int i = 0; i < nf; i++, y += AY_IND_FILA) {
        if (y + AY_IND_FILA < top || y > top + a) continue;
        ay_nodo_t *x = &ay_nodos[filas[i]];
        char buf[80];
        const char *et = ay_etiqueta(filas[i], buf, sizeof buf);
        bool sel = filas[i] == ay_sel_nodo;
        if (sel) g_retangulo(j, 2, y, w - 4, AY_IND_FILA, COR_SELECAO);
        uint32_t cor = sel ? COR_SEL_TEXTO : COR_TEXTO;
        int ind = 8 + x->nivel * 14;
        bool grupo = x->tipo == N_CAT || (x->tipo == N_SUB && !ay_sub_hoja(filas[i]));
        if (grupo) ay_triangulo(ind, y + 6, x->abierto, cor);
        int tx = ind + (grupo ? 14 : 4);
        int cab = (w - tx - 6) / LETRA_L, n = (int)tam_texto(et);
        if (x->tipo == N_CAT) {   /* negrita simulada */
            g_texto_n(j, tx, y + 2, et, n < cab ? n : cab, cor);
            g_texto_n(j, tx + 1, y + 2, et, n < cab ? n : cab, cor);
        } else g_texto_n(j, tx, y + 2, et, n < cab ? n : cab, cor);
    }
    jan_mostrar_area(j, 0, top, w + 1, a);
}

static void ay_dib_visor(void)
{
    painel_t p;
    ay_geom_visor(&p);
    ay_maquetar();
    ay_limitar_desl();
    int z = ay_zoom, vl = p.l - ROLAGEM;
    clip = (area_tela_t){ p.x, p.y, vl, p.a };
    g_retangulo(j, p.x, p.y, vl, p.a, COR_CAMPO);
    int oy = p.y - ay_desl;
    uint32_t amar = ay_escuro() ? 0x7A6400 : 0xFFE680, naranja = ay_escuro() ? 0xA55A00 : 0xFFB347;
    for (int paso = 0; paso < 3; paso++) {
        if (paso == 1) {   /* las coincidencias van sobre los fondos y bajo el texto */
            for (int c = 0; c < ay_ncoin; c++) {
                elem_t *e = &els[ay_coin[c].el];
                int cw = LETRA_L * e->escala * z;
                int x = p.x + e->x * z + ay_coin[c].off * cw, y = oy + e->y * z;
                if (y + 16 * e->escala * z < p.y || y > p.y + p.a) continue;
                ret_clip(x, y, ay_coin[c].n * cw, 16 * e->escala * z, c == ay_icoin ? naranja : amar);
            }
            continue;
        }
        for (int k = 0; k < nels; k++) {
            elem_t *e = &els[k];
            if ((e->tipo == T_TEXTO) != (paso == 2)) continue;
            int ya = e->tipo == T_LINHA ? (e->a < 0 ? e->a : 0) : 0, yb = e->tipo == T_LINHA ? (e->a > 0 ? e->a : 0) + 1 : e->a;
            if ((e->y + yb) * z < ay_desl || (e->y + ya) * z > ay_desl + p.a) continue;
            int x = p.x + e->x * z, y = oy + e->y * z;
            uint32_t cor = ay_cor(e->cor);
            if (e->tipo == T_RET) ret_clip(x, y, e->l * z, e->a * z, cor);
            else if (e->tipo == T_IMG)   /* las imágenes no se invierten con el esquema oscuro */
                imagem_desenhar(j->px, j->larg, clip.x, clip.y, clip.l, clip.a, &imgc[e->off].im, x, y, e->l * z, e->a * z, true);
            else if (e->tipo == T_LINHA) linha_clip(x, y, e->l * z, e->a * z, cor);
            else {
                int esc = e->escala * z, cw = LETRA_L * esc;
                for (int q = 0; q < e->n; q++)
                    letra(x + q * cw, y, (unsigned char)pool[e->off + q], cor, esc, e->estilo);
                if (e->estilo & E_ENLACE) ret_clip(x, y + 15 * esc, e->n * cw, esc, cor);
                if (e->estilo & E_TACHADO) ret_clip(x, y + 8 * esc, e->n * cw, 1, cor);
            }
        }
    }
    /* botón «Copiar» de cada bloque de código */
    const char *s_copiar = _("Copiar");
    int wc = g_largura_texto(s_copiar) + 12;
    for (int b = 0; b < nbcods; b++) {
        if (bcods[b].a < 24) continue;
        int bx = p.x + (bcods[b].x + bcods[b].l) * z - wc - 6, by = oy + bcods[b].y * z + 3;
        if (by + 18 < p.y || by > p.y + p.a) continue;
        ret_clip(bx, by, wc, 18, COR_BOTAO);
        ret_clip(bx, by, wc, 1, COR_BORDA); ret_clip(bx, by + 17, wc, 1, COR_BORDA);
        ret_clip(bx, by, 1, 18, COR_BORDA); ret_clip(bx + wc - 1, by, 1, 18, COR_BORDA);
        texto_cor(bx + 6, by + 1, s_copiar, (int)tam_texto(s_copiar), COR_TEXTO);
    }
    barra_rolagem(p.x + vl, p.y, p.a, ay_alto_doc(), p.a, ay_desl, &pol_y, &pol_a);
    jan_mostrar_area(j, p.x, p.y, p.l, p.a);
}

static void ay_dib_rodape(void)
{
    int y = ALT - RODAPE;
    clip = (area_tela_t){ 0, y, LARG, RODAPE };
    g_retangulo(j, 0, y, LARG, RODAPE, COR_FUNDO);
    g_retangulo(j, 0, y, LARG, 1, COR_BORDA);
    char dir[64];
    painel_t p;
    ay_geom_visor(&p);
    int max = ay_alto_doc() - p.a;
    if (ay_consulta[0])
        formatar_texto(dir, sizeof dir, ay_ncoin ? "%d/%d  %d%%" : "0  %d%%", ay_ncoin ? ay_icoin + 1 : max > 0 ? ay_desl * 100 / max : 100,
                       ay_ncoin ? ay_ncoin : 0, max > 0 ? ay_desl * 100 / max : 100);
    else formatar_texto(dir, sizeof dir, "%d%%", max > 0 ? ay_desl * 100 / max : 100);
    if (ay_consulta[0] && !ay_ncoin) formatar_texto(dir, sizeof dir, "%s", _("sin coincidencias"));
    char izq[200];
    if (ay_aviso[0]) copia_texto(izq, ay_aviso, sizeof izq);
    else {
        const char *id = ay_lang == L_ES ? "" : ay_doc_act >= 0 && !ay_docs[ay_doc_act].titulo[ay_lang][0] ? _(" (solo en español)") : "";
        formatar_texto(izq, sizeof izq, "%s%s", ay_cam, id);
    }
    int n = (int)tam_texto(izq), cabe = (LARG - 16 - g_largura_texto(dir) - 24) / LETRA_L;
    texto_cor(8, y + 2, izq, n < cabe ? n : cabe, ay_aviso[0] ? COR_ENLACE : COR_TEXTO_FRACO);
    texto_cor(LARG - 8 - g_largura_texto(dir), y + 2, dir, (int)tam_texto(dir), COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, y, LARG, RODAPE);
}

static void ay_dib_todo(void)
{
    ay_dib_barra();
    ay_dib_indice();
    ay_dib_visor();
    ay_dib_rodape();
    jan_mostrar(j);
}

/* ------------------------------------------------------------ eventos */

static bool ay_dentro(int x, int y, int rx, int ry, int rl, int ra) { return x >= rx && y >= ry && x < rx + rl && y < ry + ra; }

static void ay_registrar_filas(void)
{
    if (ay_modo_res) return;
    int filas[AY_MAX_NODOS];
    int nf = ay_filas(filas, AY_MAX_NODOS);
    for (int i = 0; i < nf; i++) {
        char buf[80];
        ay_nodo_t *x = &ay_nodos[filas[i]];
        const char *et = ay_etiqueta(filas[i], buf, sizeof buf);
        registrar("ayuda: fila %d %s %s\n", i, x->tipo == N_CAT ? "cat" : (x->tipo == N_SUB && !ay_sub_hoja(filas[i])) ? "sub" : "doc", et);
    }
}

static void ay_cambiar_idioma(int lang)
{
    if (lang == ay_lang) return;
    ay_lang = lang;
    registrar("ayuda: idioma %s\n", ay_nombre_lang[lang]);
    if (ay_doc_act >= 0) {
        char ruta[200];
        ay_ruta(ay_docs[ay_doc_act].base, lang, ruta, sizeof ruta);
        int d = ay_desl;
        if (compara_texto(ruta, ay_cam) != 0) { ay_historial_actual(); ay_cargar(ruta, NULL, d); if (ay_hi >= 0) { copia_texto(ay_hist[ay_hi].cam, ay_cam, sizeof ay_hist[ay_hi].cam); } }
        else ay_titulo_ventana();
    } else ay_titulo_ventana();
    ay_lang = lang;   /* ay_cargar lo ajusta según el nombre del archivo: si no había versión, vuelve al español, y se quiere ver el aviso */
    if (ay_doc_act >= 0 && !ay_docs[ay_doc_act].titulo[lang][0]) formatar_texto(ay_aviso, sizeof ay_aviso, "%s", _("No hay versión en ese idioma: se muestra la española"));
    if (ay_modo_res) ay_buscar_todo(ay_consulta_res);
    ay_registrar_filas();
}

static void ay_abrir_nodo(int n)
{
    ay_nodo_t *x = &ay_nodos[n];
    if (x->tipo == N_CAT || (x->tipo == N_SUB && !ay_sub_hoja(n))) {
        x->abierto = !x->abierto;
        ay_registrar_filas();
        return;
    }
    int d = x->doc;
    char ruta[200];
    ay_ruta(ay_docs[d].base, ay_lang, ruta, sizeof ruta);
    ay_abrir(ruta, NULL, true);
}

static void ay_clic_indice(int my)
{
    int top = BARRA;
    if (ay_modo_res) {
        if (my < top + 26) { ay_modo_res = false; ay_ind_desl = 0; ay_dib_indice(); return; }
        int i = (my - top - 28 + ay_ind_desl) / AY_IND_RES;
        if (i < 0 || i >= ay_nres) return;
        char ruta[200];
        ay_ruta(ay_docs[ay_res[i].doc].base, ay_lang, ruta, sizeof ruta);
        copia_texto(ay_consulta, ay_consulta_res, sizeof ay_consulta);
        copia_texto(ay_campo.c.texto, ay_consulta_res, sizeof ay_campo.c.texto);
        ay_campo.c.cursor = (int)tam_texto(ay_campo.c.texto);
        ay_icoin = -1;
        ay_abrir(ruta, NULL, true);
        registrar("ayuda: resultado %d abre %s (%d coincidencias)\n", i, ay_cam, ay_ncoin);
        return;
    }
    int filas[AY_MAX_NODOS];
    int nf = ay_filas(filas, AY_MAX_NODOS);
    int i = (my - top - 4 + ay_ind_desl) / AY_IND_FILA;
    if (i < 0 || i >= nf) return;
    ay_abrir_nodo(filas[i]);
}

static int ay_enlace_en(int mx, int my)
{
    painel_t p;
    ay_geom_visor(&p);
    int dx = (mx - p.x) / ay_zoom, dy = (my - p.y + ay_desl) / ay_zoom;
    for (int k = 0; k < nels; k++) {
        elem_t *e = &els[k];
        if (e->tipo == T_TEXTO && e->enlace >= 0 && dx >= e->x && dx < e->x + e->l && dy >= e->y && dy < e->y + e->a)
            return e->enlace;
    }
    return -1;
}

static int ay_copiar_en(int mx, int my)
{
    painel_t p;
    ay_geom_visor(&p);
    const char *s_copiar = _("Copiar");
    int wc = g_largura_texto(s_copiar) + 12;
    for (int b = 0; b < nbcods; b++) {
        if (bcods[b].a < 24) continue;
        int bx = p.x + (bcods[b].x + bcods[b].l) * ay_zoom - wc - 6, by = p.y - ay_desl + bcods[b].y * ay_zoom + 3;
        if (ay_dentro(mx, my, bx, by, wc, 18)) return b;
    }
    return -1;
}

static void ay_busqueda_doc(const char *q)
{
    copia_texto(ay_consulta, q, sizeof ay_consulta);
    ay_icoin = -1;
    ay_rehacer_coincidencias();
    if (ay_ncoin) ay_ir_coincidencia(0);
    registrar("ayuda: buscar '%s' %d coincidencias\n", ay_consulta, ay_ncoin);
}

static void ay_cambiar_zoom(int z)
{
    if (z < 1) z = 1;
    if (z > 2) z = 2;
    if (z == ay_zoom) return;
    int alto = ay_alto_doc();
    ay_zoom = z;
    ay_maquetar();
    if (alto > 0) ay_desl = (int)((long long)ay_desl * ay_alto_doc() / alto);
    ay_limitar_desl();
    registrar("ayuda: tamano de letra %dx\n", z);
}

static void ay_nuevo_tamano(void)
{
    int alto = ay_alto_doc();
    ay_maquetar();
    if (alto > 0) ay_desl = (int)((long long)ay_desl * ay_alto_doc() / alto);
    ay_limitar_desl();
    registrar("ayuda: ventana %dx%d\n", LARG, ALT);
    ay_dib_todo();
}

static void ay_doc_vecino(int delta)
{
    if (ay_doc_act < 0) return;
    int d = ay_doc_act + delta;
    if (d < 0 || d >= ay_ndocs) return;
    char ruta[200];
    ay_ruta(ay_docs[d].base, ay_lang, ruta, sizeof ruta);
    ay_abrir(ruta, NULL, true);
}

/* Atiende un evento; false = cerrar. */
static bool ay_tratar(evento_t *e, bool *r_barra, bool *r_ind, bool *r_vis)
{
    if (e->tipo == EV_FECHAR) return false;
    if (jan_redimensionar(j, e)) { ay_nuevo_tamano(); return true; }
    if (e->tipo == EV_TEMA) { *r_barra = *r_ind = *r_vis = true; return true; }
    painel_t pv;
    ay_geom_visor(&pv);
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (e->x < ay_izq()) {
            int antes = ay_ind_desl;
            int total = ay_modo_res ? ay_nres * AY_IND_RES + 30 : 0;
            if (!ay_modo_res) { int f[AY_MAX_NODOS]; total = ay_filas(f, AY_MAX_NODOS) * AY_IND_FILA + 8; }
            ay_ind_desl += n * RODA_LINHAS * LETRA_A;
            int max = total - (ALT - BARRA - RODAPE);
            if (ay_ind_desl > max) ay_ind_desl = max;
            if (ay_ind_desl < 0) ay_ind_desl = 0;
            if (ay_ind_desl != antes) *r_ind = true;
        } else {
            int antes = ay_desl;
            ay_desl += n * RODA_LINHAS * LETRA_A;
            ay_limitar_desl();
            if (ay_desl != antes) { *r_vis = true; registrar("ayuda: rueda %d, visor en %d px de %d\n", n, ay_desl, ay_alto_doc()); }
        }
        return true;
    }
    /* botones de la barra */
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU) {
        for (int i = 0; i < 5; i++)
            if (botao_tratar(j, &ay_bt[i], e)) {
                if (i == 0) ay_navegar(-1);
                else if (i == 1) ay_navegar(1);
                else if (i == 2) ay_cambiar_zoom(ay_zoom - 1);
                else if (i == 3) ay_cambiar_zoom(ay_zoom + 1);
                else ay_cambiar_idioma((ay_lang + 1) % 3);
                *r_barra = *r_ind = *r_vis = true;
                return true;
            }
        if (botao_tratar(j, &ay_bt_ant, e)) { ay_ir_coincidencia(ay_icoin - 1); *r_vis = true; return true; }
        if (botao_tratar(j, &ay_bt_sig, e)) { ay_ir_coincidencia(ay_icoin + 1); *r_vis = true; return true; }
        if (botao_tratar(j, &ay_bt_todo, e)) {
            ay_buscar_todo(ay_campo.c.texto);
            *r_ind = true;
            return true;
        }
    }
    if (e->tipo == EV_TECLA) {   /* atajos que valen con el campo de búsqueda enfocado o no */
        unsigned t = e->tecla, m = e->modificadores;
        if (t == 6) { ay_campo.c.foco = true; campo_busca_desenhar(j, &ay_campo); jan_mostrar_area(j, 0, 0, LARG, BARRA); return true; }   /* Ctrl+F */
        if (t == 7) { ay_buscar_todo(ay_campo.c.texto); *r_ind = true; return true; }                                                     /* Ctrl+G */
        if (t == 0x10C) { ay_ir_coincidencia(ay_icoin + ((m & 1) ? -1 : 1)); *r_vis = true; return true; }                                /* F3 */
    }
    /* el campo de búsqueda */
    if (e->tipo == EV_MOVER || e->tipo == EV_CLIQUE || (e->tipo == EV_TECLA && ay_campo.c.foco)) {
        bool foco_antes = ay_campo.c.foco;
        unsigned t = e->tipo == EV_TECLA ? e->tecla : 0;
        bool para_visor = t == TECLA_PGUP || t == TECLA_PGDN || t == TECLA_CIMA || t == TECLA_BAIXO || t == 0x10C /* F3 */;
        if (!(e->tipo == EV_TECLA && para_visor)) {
            int r = campo_busca_tratar(j, &ay_campo, e);
            if (r == BUSCA_MUDOU) {
                ay_busqueda_doc(ay_campo.c.texto);
                *r_vis = true;
            } else if (r == BUSCA_ENTER) {
                if (e->modificadores & MOD_CTRL) { ay_buscar_todo(ay_campo.c.texto); *r_ind = true; }
                else if (ay_consulta[0] && compara_texto(ay_consulta, ay_campo.c.texto) == 0) { ay_ir_coincidencia(ay_icoin + ((e->modificadores & 1) ? -1 : 1)); *r_vis = true; }
                else { ay_busqueda_doc(ay_campo.c.texto); *r_vis = true; }
            } else if (r == BUSCA_LIMPOU) {
                ay_busqueda_doc("");
                *r_vis = true;
            }
            if (e->tipo == EV_TECLA) { *r_barra = false; return true; }
            if (e->tipo == EV_MOVER) return true;
            (void)foco_antes;
        }
    }
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla, m = e->modificadores;
        if (t == TECLA_ESC) {
            if (ay_modo_res) { ay_modo_res = false; *r_ind = true; }
            else if (ay_consulta[0]) { ay_campo.c.texto[0] = 0; ay_campo.c.cursor = 0; ay_busqueda_doc(""); *r_barra = *r_vis = true; }
            return true;
        }
        if ((m & MOD_ALT) && t == TECLA_ESQUERDA) { ay_navegar(-1); *r_ind = *r_vis = true; return true; }
        if ((m & MOD_ALT) && t == TECLA_DIREITA) { ay_navegar(1); *r_ind = *r_vis = true; return true; }
        if (t == '\b') { ay_navegar(-1); *r_ind = *r_vis = true; return true; }
        if ((m & MOD_CTRL) && t == TECLA_CIMA) { ay_doc_vecino(-1); *r_ind = *r_vis = true; return true; }
        if ((m & MOD_CTRL) && t == TECLA_BAIXO) { ay_doc_vecino(1); *r_ind = *r_vis = true; return true; }
        if (t == '+' || t == '=') { ay_cambiar_zoom(ay_zoom + 1); *r_vis = true; return true; }
        if (t == '-') { ay_cambiar_zoom(ay_zoom - 1); *r_vis = true; return true; }
        int antes = ay_desl;
        if (t == TECLA_CIMA) ay_desl -= 36;
        else if (t == TECLA_BAIXO) ay_desl += 36;
        else if (t == TECLA_PGUP) ay_desl -= pv.a - 36;
        else if (t == TECLA_PGDN || t == ' ') ay_desl += pv.a - 36;
        else if (t == TECLA_HOME) ay_desl = 0;
        else if (t == TECLA_END) ay_desl = ay_alto_doc();
        else return true;
        ay_limitar_desl();
        if (ay_desl != antes) *r_vis = true;
        return true;
    }
    if (e->tipo == EV_SOLTOU) { arrastando = false; return true; }
    if (e->tipo == EV_MOVER && arrastando && (e->botoes & 1)) {
        int trilho = pv.a - 4, total = ay_alto_doc() - pv.a;
        if (total > 0 && trilho > pol_a) {
            ay_desl = (e->y - arraste_dy - pv.y - 2) * total / (trilho - pol_a);
            ay_limitar_desl();
            *r_vis = true;
        }
        return true;
    }
    if (e->tipo != EV_CLIQUE || !(e->botoes & 1)) return true;
    if (e->x < ay_izq() && e->y >= BARRA && e->y < ALT - RODAPE) { ay_clic_indice(e->y); *r_ind = *r_vis = true; return true; }
    if (ay_dentro(e->x, e->y, pv.x, pv.y, pv.l, pv.a)) {
        if (e->x >= pv.x + pv.l - ROLAGEM) {
            if (e->y >= pol_y && e->y < pol_y + pol_a) { arrastando = true; arraste_dy = e->y - pol_y; return true; }
            ay_desl += e->y < pol_y ? -(pv.a - 36) : pv.a - 36;
            ay_limitar_desl();
            *r_vis = true;
            return true;
        }
        int cb = ay_copiar_en(e->x, e->y);
        if (cb >= 0) {
            copiar_texto(cod_pool + bcods[cb].off, (unsigned)bcods[cb].n);
            int lin = 0;
            for (int q = 0; q < bcods[cb].n; q++) if (cod_pool[bcods[cb].off + q] == '\n') lin++;
            formatar_texto(ay_aviso, sizeof ay_aviso, _("Código copiado (%d líneas)"), lin);
            registrar("ayuda: copiado bloque %d (%d bytes, %d lineas)\n", cb, bcods[cb].n, lin);
            *r_vis = true;
            return true;
        }
        int idx = ay_enlace_en(e->x, e->y);
        if (idx >= 0) { ay_seguir_enlace(idx); *r_ind = *r_vis = true; }
    }
    return true;
}

/* Un tema pedido: nombre del archivo (lua), parte del título o ruta absoluta. Con #ancla. */
static bool ay_resolver_tema(const char *tema, char *ruta, int max, char *ancla, int amax)
{
    char t[160];
    ancla[0] = 0;
    copia_texto(t, tema, sizeof t);
    char *h = acha_char(t, '#');
    if (h) { copia_texto(ancla, h + 1, (size_t)amax); *h = 0; }
    if (t[0] == '/') {
        if (ay_termina(t, ".md")) copia_texto(ruta, t, (size_t)max);
        else formatar_texto(ruta, (size_t)max, "%s.md", t);
        return ay_existe(ruta);
    }
    int tn = (int)tam_texto(t);
    if (ay_termina(t, ".md")) { t[tn - 3] = 0; tn -= 3; }
    int d = -1;
    for (int i = 0; i < ay_ndocs && d < 0; i++) {
        const char *b = acha_ultimo_char(ay_docs[i].base, '/');
        b = b ? b + 1 : ay_docs[i].base;
        if (ay_comp_ci(b, t) == 0) d = i;
    }
    for (int i = 0; i < ay_ndocs && d < 0; i++)
        for (int l = 0; l < 3 && d < 0; l++)
            if (ay_docs[i].titulo[l][0] && ay_acha(ay_docs[i].titulo[l], (int)tam_texto(ay_docs[i].titulo[l]), t, tn) >= 0) d = i;
    if (d < 0) return false;
    ay_ruta(ay_docs[d].base, ay_lang, ruta, (size_t)max);
    return true;
}

int principal(int argc, char **argv)
{
    registrar("ayuda: iniciando\n");
    {
        const char *l = idioma_actual();
        ay_lang = l && compara_texto(l, "en") == 0 ? L_EN : l && compara_texto_n(l, "pt", 2) == 0 ? L_PT : L_ES;
    }
    for (int i = 1; i < argc; i++)
        if (compara_texto(argv[i], "--es") == 0) ay_lang = L_ES;
        else if (compara_texto(argv[i], "--en") == 0) ay_lang = L_EN;
        else if (compara_texto(argv[i], "--pt") == 0) ay_lang = L_PT;
    ay_recorrer(AY_RAIZ, 0);
    ay_armar_arbol();
    const char *tema = NULL;
    for (int i = 1; i < argc; i++) if (argv[i][0] != '-') tema = argv[i];
    char ruta_t[200], ancla_t[96];
    bool tema_ok = tema && ay_resolver_tema(tema, ruta_t, sizeof ruta_t, ancla_t, sizeof ancla_t);
    j = jan_abrir(_("Ayuda"), 900, 620);
    if (!j) {   /* sin escritorio: el documento (o el índice) en texto, como «markdown -t» */
        if (tema_ok) return modo_texto(ruta_t, 78);
        for (int d = 0; d < ay_ndocs; d++) imprimir("%s  %s\n", ay_docs[d].base, ay_docs[d].titulo[L_ES]);
        if (tema) { imprimir_em(ERROS, "ayuda: no hay ayuda sobre %s\n", tema); return 1; }
        return 0;
    }
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    reg_titulos = false;
    int cats = 0, subs = 0, hojas = 0;
    for (int n = 0; n < ay_nnodos; n++) {
        if (ay_nodos[n].tipo == N_CAT) cats++;
        else if (ay_nodos[n].tipo == N_SUB) subs++;
        else hojas++;
    }
    registrar("ayuda: indice %d documentos, %d categorias, %d grupos\n", ay_ndocs, cats, subs);
    ay_campo.c.foco = false;
    ay_hi = -1; ay_hn = 0;
    bool ok = tema_ok && ay_abrir(ruta_t, ancla_t, true);
    if (!ok) {
        if (tema) formatar_texto(ay_aviso, sizeof ay_aviso, _("No hay ayuda sobre «%s»"), tema);
        char ruta[200];
        ay_ruta(AY_RAIZ "/ayuda", ay_lang, ruta, sizeof ruta);
        if (!ay_existe(ruta) && ay_ndocs > 0) ay_ruta(ay_docs[0].base, ay_lang, ruta, sizeof ruta);
        ay_abrir(ruta, NULL, true);
        if (tema) formatar_texto(ay_aviso, sizeof ay_aviso, _("No hay ayuda sobre «%s»"), tema);
    }
    ay_dib_todo();
    ay_registrar_filas();
    registrar("ayuda: pronto\n");

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        bool rb = false, ri = false, rv = false;
        continuar = ay_tratar(&e, &rb, &ri, &rv);
        while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = ay_tratar(&e, &rb, &ri, &rv);
        if (!continuar) break;
        if (rb) ay_dib_barra();
        if (ri) ay_dib_indice();
        if (rv) ay_dib_visor();
        if (rb || ri || rv) ay_dib_rodape();
    }
    registrar("ayuda: fechado\n");
    jan_fechar(j);
    return 0;
}
