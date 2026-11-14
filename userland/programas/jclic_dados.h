// SPDX-License-Identifier: GPL-2.0-or-later
/* jclic_dados.h - proyecto JClic: abrir el ZIP o el .jclic suelto, medios y actividades (D185).
 *
 * Formato (resumen, de cómo lo escriben JClic y JClic.js; el código es propio):
 *   <JClicProject><settings><title/>...</settings>
 *     <sequence><item name="..." [activity="..."] [delay navButtons]><jump id="forward|back" action="JUMP|STOP|RETURN" sequence="..."/></item>...
 *     <activities><activity class="@puzzles.DoublePuzzle" name="..."> <messages><cell type="initial|final|finalError">...
 *        <settings><container bgColor><counters .../></container><window bgColor/></settings>
 *        <cells id="primary|secondary|acrossClues|downClues" rows cols cellWidth cellHeight border image>
 *            <style><font size bold/><color foreground background inactive/></style><shaper class cols rows/>
 *            <cell id item image txtAlign><style/><media type="PLAY_AUDIO" file/><p>texto</p></cell>...
 *        <textGrid rows columns><text><row>LETRAS</row>...</text></textGrid><clues><clue id>palabra</clue>...
 *        <scramble times primary secondary/><layout position="AB|BA|AUB|BUA"/>
 *     <mediaBag><media name file/>...
 *
 * La memoria de cada actividad sale de un arenero propio que se suelta al cambiar de actividad. */
#ifndef JCLIC_DADOS_H
#define JCLIC_DADOS_H
#include "sagui.h"
#include "compressao.h"
#include "imagem.h"
#include "jclic_xml.h"

/* ------------------------------------------------------------ estilos y celdas */

typedef struct {
    uint32_t fg, bg, inativo;
    bool     tem_inativo;
    int      tam;          /* tamaño de fuente pedido (puntos); 0 = el de por defecto */
    bool     neg;
} estilo_t;

typedef struct {
    char    *txt, *img, *som, *salto;   /* texto (Latin-1, '\n' entre párrafos), medio de imagen, de sonido y actividad a la que salta (RUN_CLIC_ACTIVITY) */
    int      id, item;          /* -1 = sin el atributo */
    estilo_t est;               /* ya mezclado con el del grupo */
    uint8_t  ah, av;            /* 0 izquierda/arriba, 1 centro, 2 derecha/abajo */
} cel_t;

#define G_PRIM 0
#define G_SEC  1
#define G_ACR  2   /* acrossClues */
#define G_BAJ  3   /* downClues */

typedef struct {
    bool     tem;
    int      lin, col;
    int      cw, ch;            /* tamaño natural de la celda, en píxeles */
    bool     borda;
    char    *imagem;            /* imagen que se corta en rejilla (puzles) */
    estilo_t est;
    int      n;
    cel_t   *c;
} grupo_t;

enum { T_NADA, T_INFO, T_DOBLE, T_INTERC, T_BURACO, T_ASOC_S, T_ASOC_C, T_MEMORIA, T_EXPLORA, T_IDENT,
       T_SOPA, T_CRUZ, T_TEXTO, T_OUTRO };

#define TG_MAX 40
#define PAL_MAX 40

typedef struct {
    xn      *no;
    char    *nome, *classe, *descr;
    int      tipo;
    char     classe_curta[48];
    grupo_t  g[4];
    cel_t    msg[3];            /* 0 inicial, 1 final, 2 final con error */
    bool     tem_msg[3];
    uint32_t cor_cont, cor_jan;
    bool     c_tempo, c_pontos, c_acoes;
    char     layout[8];
    bool     emb_prim, emb_sec, emb_alg;
    /* sopa de letras y crucigrama */
    int      tg_lin, tg_col, tg_cw, tg_ch;
    char     tg[TG_MAX][TG_MAX + 1];
    uint32_t tg_bg;
    int      n_pal;
    char    *pal[PAL_MAX];
    char     alfabeto[64];
} ativ_t;

/* ------------------------------------------------------------ el proyecto */

#define SEQ_MAX 1024
typedef struct {
    char *nome, *id;
    char *ativ;
    char *salto_f, *salto_a;    /* nombre de la secuencia de destino de los saltos (NULL = ninguno) */
    int   acao_f, acao_a;       /* 0 nada, 1 JUMP, 2 STOP, 3 RETURN */
} item_t;

static arena_t ar_proj, ar_at;
static uint8_t *proj_dados;
static int proj_n;
static zip_leitor_t proj_zip;
static bool proj_eh_zip;
static char proj_pasta[256], proj_arquivo[256];
static char proj_titulo[128], proj_autor[128], proj_descr[512];
static xn *proj_raiz;
static item_t *seq;
static int n_seq;
static xn **ativs_no;
static int n_ativs;
static xn *proj_midias;

static int minuscula(int c) { return c >= 'A' && c <= 'Z' ? c | 0x20 : c; }
static bool igual_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (minuscula((unsigned char)*a) != minuscula((unsigned char)*b)) return false;
    return !*a && !*b;
}
static bool termina_em(const char *s, const char *suf)
{
    int a = (int)tam_texto(s), b = (int)tam_texto(suf);
    return a >= b && igual_ci(s + a - b, suf);
}
static bool contem(const char *s, const char *p)
{
    int n = (int)tam_texto(p);
    for (; *s; s++) if (!compara_texto_n(s, p, (size_t)n)) return true;
    return false;
}
static const char *base_nome(const char *s)
{
    const char *r = s;
    for (; *s; s++) if (*s == '/' || *s == '\\') r = s + 1;
    return r;
}

static void projeto_fechar(void)
{
    if (proj_eh_zip) zip_leitor_liberar(&proj_zip);
    proj_eh_zip = false;
    libera(proj_dados);
    proj_dados = NULL;
    proj_n = 0;
    ar_soltar(&ar_proj);
    ar_soltar(&ar_at);
    proj_raiz = NULL; seq = NULL; n_seq = 0; ativs_no = NULL; n_ativs = 0; proj_midias = NULL;
}

/* Ficheros del ZIP: busca por la ruta entera y, si no, por el nombre base (sin distinguir mayúsculas). */
static int zip_achar(const char *nome)
{
    if (!proj_eh_zip) return -1;
    char limpo[256];
    copia_texto(limpo, nome, sizeof limpo);
    for (char *p = limpo; *p; p++) if (*p == '\\') *p = '/';
    for (int i = 0; i < proj_zip.cnt; i++)
        if (!proj_zip.e[i].pasta && igual_ci(proj_zip.e[i].nome, limpo)) return i;
    const char *b = base_nome(limpo);
    for (int i = 0; i < proj_zip.cnt; i++)
        if (!proj_zip.e[i].pasta && igual_ci(base_nome(proj_zip.e[i].nome), b)) return i;
    return -1;
}

/* El nombre de fichero de un medio: el mediaBag traduce "aplauso.wav" a "aplauso.mp3". */
static const char *midia_arquivo(const char *nome)
{
    if (proj_midias)
        for (xn *m = x_filho(proj_midias, "media"); m; m = x_irm(m, "media"))
            if (!compara_texto(x_atd(m, "name", ""), nome)) return x_atd(m, "file", nome);
    return nome;
}

/* Lee un medio entero (libera con libera); NULL si no existe. */
static uint8_t *midia_ler(const char *nome, int *n)
{
    const char *f = midia_arquivo(nome);
    if (proj_eh_zip) {
        int i = zip_achar(f);
        if (i < 0 && f != nome) i = zip_achar(nome);
        if (i < 0) return NULL;
        uint8_t *o = NULL;
        int r = zip_extrair(&proj_zip, i, &o);
        if (r < 0) return NULL;
        *n = (int)proj_zip.e[i].tu;
        return o;
    }
    char c[400];
    caminho_juntar(c, sizeof c, proj_pasta, f);
    uint8_t *b = carregar_arquivo(c, n);
    if (!b && f != nome) { caminho_juntar(c, sizeof c, proj_pasta, nome); b = carregar_arquivo(c, n); }
    if (!b) { caminho_juntar(c, sizeof c, proj_pasta, base_nome(f)); b = carregar_arquivo(c, n); }
    return b;
}

/* ------------------------------------------------------------ imágenes */

#define CACHE_IMG 40
#define IMG_LADO_VISOR 1100   /* las imágenes mayores se reducen al cargar (memoria) */
typedef struct { char nome[64]; imagem_t im; bool ok; } img_cache_t;
static img_cache_t cache_img[CACHE_IMG];
static int n_cache_img;

static void cache_limpar(void)
{
    for (int i = 0; i < n_cache_img; i++) if (cache_img[i].ok) imagem_liberar(&cache_img[i].im);
    n_cache_img = 0;
}

static imagem_t *img_obter(const char *nome)
{
    if (!nome || !*nome) return NULL;
    for (int i = 0; i < n_cache_img; i++)
        if (!compara_texto(cache_img[i].nome, nome)) return cache_img[i].ok ? &cache_img[i].im : NULL;
    if (n_cache_img >= CACHE_IMG) cache_limpar();
    img_cache_t *c = &cache_img[n_cache_img++];
    memset(c, 0, sizeof *c);
    copia_texto(c->nome, nome, sizeof c->nome);
    int n = 0;
    uint8_t *d = midia_ler(nome, &n);
    if (!d) return NULL;
    int r = imagem_decodificar(d, n, &c->im);
    libera(d);
    if (r != 0) { registrar("jclic: imagen %s: %s", nome, imagem_erro(r)); return NULL; }
    if (c->im.larg > IMG_LADO_VISOR || c->im.alt > IMG_LADO_VISOR) {
        int l = c->im.larg, a = c->im.alt;
        if (l >= a) { a = a * IMG_LADO_VISOR / l; l = IMG_LADO_VISOR; } else { l = l * IMG_LADO_VISOR / a; a = IMG_LADO_VISOR; }
        imagem_t red;
        if (l < 1) l = 1;
        if (a < 1) a = 1;
        if (imagem_escalar(&red, &c->im, l, a, true)) { imagem_liberar(&c->im); c->im = red; }
    }
    c->ok = true;
    return &c->im;
}

/* ------------------------------------------------------------ proyecto */

static int projeto_abrir(const char *caminho)
{
    projeto_fechar();
    cache_limpar();
    copia_texto(proj_arquivo, caminho, sizeof proj_arquivo);
    copia_texto(proj_pasta, caminho, sizeof proj_pasta);
    char *b = acha_ultimo_char(proj_pasta, '/');
    if (b) { if (b == proj_pasta) b[1] = 0; else *b = 0; } else copia_texto(proj_pasta, ".", sizeof proj_pasta);
    info_arquivo_t inf;
    int r = info_arquivo(caminho, &inf);
    if (r < 0) return r;
    if (inf.tamanho > 48u * 1024 * 1024) return COMP_ERRO_GRANDE;
    proj_dados = carregar_arquivo(caminho, &proj_n);
    if (!proj_dados) return ERRO_SEM_MEMORIA;
    uint8_t *xml = proj_dados;
    int xml_n = proj_n;
    uint8_t *extra = NULL;
    if (proj_n > 4 && proj_dados[0] == 'P' && proj_dados[1] == 'K') {
        r = zip_abrir(&proj_zip, proj_dados, proj_n);
        if (r < 0) return r;
        proj_eh_zip = true;
        int k = -1;
        for (int i = 0; i < proj_zip.cnt; i++)
            if (!proj_zip.e[i].pasta && termina_em(proj_zip.e[i].nome, ".jclic")) { k = i; break; }
        if (k < 0) return COMP_ERRO_FORMATO;
        r = zip_extrair(&proj_zip, k, &extra);
        if (r < 0) return r;
        xml = extra;
        xml_n = (int)proj_zip.e[k].tu;
    }
    const char *erro = "";
    proj_raiz = xml_analisar(&ar_proj, (const char *)xml, xml_n, &erro);
    libera(extra);
    if (!proj_raiz || compara_texto(proj_raiz->nome, "JClicProject")) {
        registrar("jclic: XML no valido: %s", proj_raiz ? "no es un JClicProject" : erro);
        proj_raiz = NULL;
        return COMP_ERRO_FORMATO;
    }
    xn *st = x_filho(proj_raiz, "settings");
    xn *t = x_filho(st, "title");
    copia_texto(proj_titulo, t && t->txt ? t->txt : base_nome(caminho), sizeof proj_titulo);
    xn *au = x_filho(st, "author");
    copia_texto(proj_autor, au ? x_atd(au, "name", "") : "", sizeof proj_autor);
    char *ds = x_paragrafos(&ar_proj, x_filho(st, "description"));
    copia_texto(proj_descr, ds ? ds : "", sizeof proj_descr);
    proj_midias = x_filho(proj_raiz, "mediaBag");
    /* actividades */
    xn *as = x_filho(proj_raiz, "activities");
    int na = 0;
    for (xn *a = x_filho(as, "activity"); a; a = x_irm(a, "activity")) na++;
    ativs_no = ar_pedir(&ar_proj, (size_t)(na + 1) * sizeof(xn *));
    for (xn *a = x_filho(as, "activity"); a && ativs_no; a = x_irm(a, "activity")) ativs_no[n_ativs++] = a;
    /* secuencia */
    xn *sq = x_filho(proj_raiz, "sequence");
    int ns = 0;
    for (xn *i = x_filho(sq, "item"); i; i = x_irm(i, "item")) ns++;
    if (ns > SEQ_MAX) ns = SEQ_MAX;
    seq = ar_pedir(&ar_proj, (size_t)(ns + 1) * sizeof(item_t));
    if (!seq) return ERRO_SEM_MEMORIA;
    for (xn *i = x_filho(sq, "item"); i && n_seq < ns; i = x_irm(i, "item")) {
        item_t *it = &seq[n_seq++];
        it->nome = (char *)x_atd(i, "name", "");
        it->id = (char *)x_atd(i, "id", "");
        it->ativ = (char *)x_atd(i, "activity", it->nome);
        for (xn *j = x_filho(i, "jump"); j; j = x_irm(j, "jump")) {
            const char *id = x_atd(j, "id", ""), *ac = x_atd(j, "action", "");
            int a = !compara_texto(ac, "JUMP") ? 1 : !compara_texto(ac, "STOP") ? 2 : !compara_texto(ac, "RETURN") ? 3 : 0;
            const char *dest = x_at(j, "sequence");
            if (!dest && x_at(j, "tag")) { dest = x_at(j, "tag"); if (!a) a = 1; }   /* el salto por etiqueta: al item con ese id */
            if (!compara_texto(id, "forward")) { it->acao_f = a; it->salto_f = (char *)dest; }
            else if (!compara_texto(id, "back")) { it->acao_a = a; it->salto_a = (char *)dest; }
        }
    }
    if (n_seq == 0) {   /* sin secuencia: todas las actividades, en orden */
        seq = ar_pedir(&ar_proj, (size_t)(n_ativs + 1) * sizeof(item_t));
        for (int k = 0; k < n_ativs && seq; k++) {
            seq[n_seq].nome = (char *)x_atd(ativs_no[k], "name", "");
            seq[n_seq].ativ = seq[n_seq].nome;
            seq[n_seq].id = "";
            n_seq++;
        }
    }
    if (n_seq == 0) return COMP_ERRO_FORMATO;
    return 0;
}

static xn *ativ_achar(const char *nome)
{
    for (int i = 0; i < n_ativs; i++) if (!compara_texto(x_atd(ativs_no[i], "name", ""), nome)) return ativs_no[i];
    return NULL;
}

/* ------------------------------------------------------------ una actividad */

static estilo_t estilo_padrao(void)
{
    estilo_t e;
    memset(&e, 0, sizeof e);
    e.fg = 0x000000;
    e.bg = 0xFFFFFF;
    return e;
}

static estilo_t estilo_ler(const xn *st, estilo_t base)
{
    if (!st) return base;
    xn *f = x_filho(st, "font");
    if (f) {
        int t = x_int(f, "size", 0);
        if (t > 0) base.tam = t;
        if (x_at(f, "bold")) base.neg = x_bool(f, "bold", false);
    }
    xn *c = x_filho(st, "color");
    if (c) {
        if (x_at(c, "foreground")) base.fg = x_cor(x_at(c, "foreground"), base.fg);
        if (x_at(c, "background")) base.bg = x_cor(x_at(c, "background"), base.bg);
        if (x_at(c, "inactive")) { base.inativo = x_cor(x_at(c, "inactive"), 0); base.tem_inativo = true; }
    }
    return base;
}

static void cel_ler(arena_t *ar, const xn *no, const estilo_t *base, cel_t *c)
{
    memset(c, 0, sizeof *c);
    c->id = x_int(no, "id", -1);
    c->item = x_int(no, "item", -1);
    c->est = estilo_ler(x_filho(no, "style"), *base);
    c->txt = x_paragrafos(ar, no);
    const char *im = x_at(no, "image");
    c->img = im && *im ? (char *)im : NULL;
    for (xn *m = x_filho(no, "media"); m; m = x_irm(m, "media"))
    {
        const char *tp = x_atd(m, "type", "");
        if (!compara_texto(tp, "PLAY_AUDIO") && x_at(m, "file") && !c->som) c->som = (char *)x_at(m, "file");
        else if (!compara_texto(tp, "RUN_CLIC_ACTIVITY") && x_at(m, "file")) c->salto = (char *)x_at(m, "file");
    }
    c->ah = 1;
    c->av = 1;
    const char *al = x_at(no, "txtAlign");
    if (al) {   /* "left,top", "middle,bottom"... */
        const char *v = acha_char(al, ',');
        if (!compara_texto_n(al, "left", 4)) c->ah = 0; else if (!compara_texto_n(al, "right", 5)) c->ah = 2;
        if (v) { v++; if (!compara_texto_n(v, "top", 3)) c->av = 0; else if (!compara_texto_n(v, "bottom", 6)) c->av = 2; }
    }
}

static void grupo_ler(arena_t *ar, const xn *no, grupo_t *g)
{
    memset(g, 0, sizeof *g);
    g->tem = true;
    g->col = x_int(no, "cols", x_int(no, "columns", 0));
    g->lin = x_int(no, "rows", 0);
    xn *sh = x_filho(no, "shaper");
    if (sh) {
        if (!g->col) g->col = x_int(sh, "cols", 0);
        if (!g->lin) g->lin = x_int(sh, "rows", 0);
    }
    g->cw = x_int(no, "cellWidth", 0);
    g->ch = x_int(no, "cellHeight", 0);
    g->borda = x_bool(no, "border", true);
    const char *im = x_at(no, "image");
    g->imagem = im && *im ? (char *)im : NULL;
    g->est = estilo_ler(x_filho(no, "style"), estilo_padrao());
    int n = 0;
    for (xn *c = x_filho(no, "cell"); c; c = x_irm(c, "cell")) n++;
    if (n == 0 && g->imagem && g->col > 0 && g->lin > 0) n = g->col * g->lin;
    if (g->col <= 0 && g->lin <= 0) { g->col = n > 0 ? n : 1; g->lin = 1; }
    else if (g->col <= 0) g->col = (n + g->lin - 1) / g->lin;
    else if (g->lin <= 0) g->lin = (n + g->col - 1) / g->col;
    if (n < g->col * g->lin) n = g->col * g->lin;   /* se rellena con celdas vacías */
    if (n > 2000) n = 2000;
    g->n = n;
    g->c = ar_pedir(ar, (size_t)n * sizeof(cel_t));
    if (!g->c) { g->n = 0; return; }
    int k = 0;
    for (xn *c = x_filho(no, "cell"); c && k < n; c = x_irm(c, "cell")) cel_ler(ar, c, &g->est, &g->c[k++]);
    for (; k < n; k++) { memset(&g->c[k], 0, sizeof(cel_t)); g->c[k].id = g->c[k].item = -1; g->c[k].txt = ""; g->c[k].est = g->est; g->c[k].ah = g->c[k].av = 1; }
    /* una imagen para cortar: el tamaño de la celda es el de la imagen entre la rejilla */
    if (g->imagem && !g->c[0].img) {
        imagem_t *im = img_obter(g->imagem);
        if (im) { g->cw = im->larg / g->col; g->ch = im->alt / g->lin; }
    }
    if (g->cw <= 0) g->cw = 100;
    if (g->ch <= 0) g->ch = 100;
}

static int tipo_da_classe(const char *cl)
{
    if (contem(cl, "InformationScreen")) return T_INFO;
    if (contem(cl, "DoublePuzzle")) return T_DOBLE;
    if (contem(cl, "ExchangePuzzle")) return T_INTERC;
    if (contem(cl, "HolePuzzle")) return T_BURACO;
    if (contem(cl, "SimpleAssociation")) return T_ASOC_S;
    if (contem(cl, "ComplexAssociation")) return T_ASOC_C;
    if (contem(cl, "MemoryGame")) return T_MEMORIA;
    if (contem(cl, "panels.Explore") || contem(cl, "Explore")) return T_EXPLORA;
    if (contem(cl, "panels.Identify")) return T_IDENT;
    if (contem(cl, "WordSearch")) return T_SOPA;
    if (contem(cl, "CrossWord")) return T_CRUZ;
    if (contem(cl, "text.")) return T_TEXTO;
    return T_OUTRO;
}

static const char *nome_do_tipo(int t)
{
    switch (t) {
    case T_INFO: return "pantalla de informacion";
    case T_DOBLE: return "puzle doble";
    case T_INTERC: return "puzle de intercambio";
    case T_BURACO: return "puzle de agujero";
    case T_ASOC_S: return "asociacion simple";
    case T_ASOC_C: return "asociacion compleja";
    case T_MEMORIA: return "juego de memoria";
    case T_EXPLORA: return "explorar";
    case T_IDENT: return "identificar";
    case T_SOPA: return "sopa de letras";
    case T_CRUZ: return "crucigrama";
    case T_TEXTO: return "actividad de texto";
    default: return "tipo desconocido";
    }
}

/* Carga la actividad "nome" en *a (memoria en ar_at, que se suelta antes). false = no existe. */
static bool ativ_ler(const char *nome, ativ_t *a)
{
    ar_soltar(&ar_at);
    cache_limpar();
    memset(a, 0, sizeof *a);
    xn *no = ativ_achar(nome);
    if (!no) return false;
    a->no = no;
    a->nome = (char *)x_atd(no, "name", "");
    a->classe = (char *)x_atd(no, "class", "");
    a->tipo = tipo_da_classe(a->classe);
    copia_texto(a->classe_curta, a->classe[0] == '@' ? a->classe + 1 : a->classe, sizeof a->classe_curta);
    a->descr = x_paragrafos(&ar_at, x_filho(no, "description"));
    a->cor_cont = 0xFFFFFF; a->cor_jan = 0xFFFFFF;
    a->c_tempo = a->c_pontos = a->c_acoes = true;
    copia_texto(a->layout, "AB", sizeof a->layout);
    xn *se = x_filho(no, "settings");
    if (se) {
        xn *ct = x_filho(se, "container");
        if (ct) {
            a->cor_cont = x_cor(x_at(ct, "bgColor"), a->cor_cont);
            xn *co = x_filho(ct, "counters");
            if (co) { a->c_tempo = x_bool(co, "time", true); a->c_pontos = x_bool(co, "score", true); a->c_acoes = x_bool(co, "actions", true); }
        }
        xn *wi = x_filho(se, "window");
        if (wi) a->cor_jan = x_cor(x_at(wi, "bgColor"), a->cor_jan);
    }
    xn *ms = x_filho(no, "messages");
    for (xn *c = x_filho(ms, "cell"); c; c = x_irm(c, "cell")) {
        const char *t = x_atd(c, "type", "");
        int k = !compara_texto(t, "initial") ? 0 : !compara_texto(t, "final") ? 1 : !compara_texto(t, "finalError") ? 2 : -1;
        if (k < 0) continue;
        estilo_t base = estilo_padrao();
        base.bg = 0xFFFFCC;
        cel_ler(&ar_at, c, &base, &a->msg[k]);
        a->tem_msg[k] = true;
    }
    for (xn *c = x_filho(no, "cells"); c; c = x_irm(c, "cells")) {
        const char *id = x_atd(c, "id", "primary");
        int k = !compara_texto(id, "primary") ? G_PRIM : !compara_texto(id, "secondary") ? G_SEC
              : !compara_texto(id, "acrossClues") ? G_ACR : !compara_texto(id, "downClues") ? G_BAJ : -1;
        if (k < 0 || a->g[k].tem) continue;
        grupo_ler(&ar_at, c, &a->g[k]);
        xn *sh = x_filho(c, "shaper");
        if (sh && contem(x_atd(sh, "class", ""), "Holes")) {   /* zonas con forma sobre una imagen: no se juegan */
            a->tipo = T_OUTRO;
            copia_texto(a->classe_curta, "formas @Holes", sizeof a->classe_curta);
        }
    }
    xn *sc = x_filho(no, "scramble");
    if (sc) { a->emb_prim = x_bool(sc, "primary", false); a->emb_sec = x_bool(sc, "secondary", false); a->emb_alg = true; }
    xn *lo = x_filho(no, "layout");
    if (lo) copia_texto(a->layout, x_atd(lo, "position", "AB"), sizeof a->layout);
    xn *tg = x_filho(no, "textGrid");
    if (tg) {
        a->tg_lin = x_int(tg, "rows", 0);
        a->tg_col = x_int(tg, "columns", x_int(tg, "cols", 0));
        a->tg_cw = x_int(tg, "cellWidth", 30);
        a->tg_ch = x_int(tg, "cellHeight", 30);
        estilo_t es = estilo_ler(x_filho(tg, "style"), estilo_padrao());
        a->tg_bg = es.bg;
        copia_texto(a->alfabeto, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", sizeof a->alfabeto);
        xn *rc = x_filho(tg, "randomChars");
        if (rc && rc->txt && rc->txt[0]) {
            const char *s = rc->txt;
            while (xml_esp((unsigned char)*s)) s++;
            copia_texto(a->alfabeto, s, sizeof a->alfabeto);
        }
        int r = 0;
        for (xn *row = x_filho(x_filho(tg, "text"), "row"); row && r < TG_MAX; row = x_irm(row, "row"), r++) {
            const char *t = row->txt ? row->txt : "";
            int l = (int)tam_texto(t);
            if (l > TG_MAX) l = TG_MAX;
            memcpy(a->tg[r], t, (size_t)l);
            a->tg[r][l] = 0;
            if (l > a->tg_col) a->tg_col = l;
        }
        if (r > a->tg_lin) a->tg_lin = r;
        if (a->tg_lin > TG_MAX) a->tg_lin = TG_MAX;
        if (a->tg_col > TG_MAX) a->tg_col = TG_MAX;
        for (int y = 0; y < a->tg_lin; y++) {   /* las filas cortas se rellenan */
            int l = (int)tam_texto(a->tg[y]);
            for (int x = l; x < a->tg_col; x++) a->tg[y][x] = '*';
            a->tg[y][a->tg_col] = 0;
        }
    }
    for (xn *c = x_filho(x_filho(no, "clues"), "clue"); c && a->n_pal < PAL_MAX; c = x_irm(c, "clue")) {
        if (!c->txt) continue;
        const char *t = c->txt;
        while (xml_esp((unsigned char)*t)) t++;
        int l = (int)tam_texto(t);
        while (l > 0 && xml_esp((unsigned char)t[l - 1])) l--;
        if (l > 0) a->pal[a->n_pal++] = ar_texto(&ar_at, t, l);
    }
    return true;
}
#endif
