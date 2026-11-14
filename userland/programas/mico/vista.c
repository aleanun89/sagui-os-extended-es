// SPDX-License-Identifier: GPL-2.0-or-later
/* vista.c - el editor en pantalla: dibujo, teclado y mouse.
 *
 * Dibujo por líneas sucias: cada cambio marca el intervalo de líneas del
 * documento que cambió (vista_sujar); al final del lote de eventos solo esas
 * líneas se pintan y solo la franja de ellas va a la pantalla. Desplazar,
 * cambiar de pestaña o cambiar el ancho del margen de los números pinta todo. Escribir
 * en una línea sucia solo esa (el documento dice si el estado del realce al final de la línea
 * cambió; si cambió, ensucia hasta el fin del área visible).
 *
 * Las tabulaciones quedan en el texto como '\t' y solo se expanden en el dibujo (hasta
 * la próxima columna múltiplo de tab_largura); las columnas "visuales" sirven
 * para el cursor, el mouse y el desplazamiento horizontal. */
#include "mico.h"

#define ROLAGEM 12
#define DUPLO_MS 500

static int suj_l0 = 1 << 30, suj_l1 = -1;
static bool suj_tudo = true;
static int margem;                     /* ancho del margen de los números */
static int par_a = -1, par_b = -1;     /* paréntesis que se corresponden (fondo) */
static bool arrastando, arrastando_barra;
static int barra_dy;
static unsigned ult_duplo;             /* para reconocer el triple clic */
static int ult_duplo_pos = -1;

static char lb[LINHA_MAX + 1];
static uint8_t cls[LINHA_MAX + 1];

/* ------------------------------------------------------------ geometría */

static int digitos(int n) { int k = 1; while (n >= 10) { n /= 10; k++; } return k; }
static int calc_margem(void) { int k = digitos(DOC->nli); if (k < 3) k = 3; return k * LETRA_L + 16; }
int vista_linhas_visiveis(void) { int v = r_editor.a / LETRA_A; return v < 1 ? 1 : v; }
static int texto_x(void) { return r_editor.x + margem + 4; }
static int colunas(void) { int c = (r_editor.l - margem - 4 - ROLAGEM) / LETRA_L; return c < 1 ? 1 : c; }

/* Columna visual de la posición pos (en la línea l), con las tabulaciones expandidas. */
static int col_visual(doc_t *d, int l, int pos)
{
    int ini = doc_ini(d, l), v = 0;
    for (int p = ini; p < pos; p++) {
        if (doc_car(d, p) == '\t') v = (v / tab_largura + 1) * tab_largura;
        else v++;
    }
    return v;
}

/* Posición de la línea l más cerca de la columna visual vc. */
static int pos_de_col(doc_t *d, int l, int vc)
{
    int ini = doc_ini(d, l), fim = doc_fim(d, l), v = 0;
    for (int p = ini; p < fim; p++) {
        int w = doc_car(d, p) == '\t' ? (v / tab_largura + 1) * tab_largura - v : 1;
        if (vc < v + (w + 1) / 2) return p;
        v += w;
    }
    return fim;
}

static bool palavra_car(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (unsigned char)c >= 0xC0; }

/* ------------------------------------------------------------ suciedad */

void vista_sujar(int l0, int l1)
{
    if (l0 > l1) { int t = l0; l0 = l1; l1 = t; }
    if (l0 < suj_l0) suj_l0 = l0;
    if (l1 > suj_l1) suj_l1 = l1;
    sujo |= R_EDITOR;
}

void vista_sujar_tudo(void) { suj_tudo = true; sujo |= R_EDITOR; }

bool vista_selecao(int *s0, int *s1)
{
    doc_t *d = DOC;
    if (d->ancora < 0 || d->ancora == d->cursor) return false;
    *s0 = d->ancora < d->cursor ? d->ancora : d->cursor;
    *s1 = d->ancora < d->cursor ? d->cursor : d->ancora;
    return true;
}

/* Líneas que cubre la selección actual (o solo la del cursor). */
static void linhas_sel(int *l0, int *l1)
{
    doc_t *d = DOC;
    int a = d->ancora >= 0 ? d->ancora : d->cursor;
    *l0 = doc_linha_de(d, a < d->cursor ? a : d->cursor);
    *l1 = doc_linha_de(d, a < d->cursor ? d->cursor : a);
}

/* ------------------------------------------------------------ paréntesis */

static char par_de(char c)
{
    switch (c) { case '(': return ')'; case '[': return ']'; case '{': return '}';
                 case ')': return '('; case ']': return '['; case '}': return '{'; }
    return 0;
}

static void achar_par(void)
{
    doc_t *d = DOC;
    int a = -1, b = -1, p = -1;
    if (par_de(doc_car(d, d->cursor))) p = d->cursor;
    else if (d->cursor > 0 && par_de(doc_car(d, d->cursor - 1))) p = d->cursor - 1;
    if (p >= 0) {
        char c = doc_car(d, p), o = par_de(c);
        bool frente = c == '(' || c == '[' || c == '{';
        int nivel = 0;
        for (int q = p, passos = 0; q >= 0 && q < d->tam && passos < 20000; q += frente ? 1 : -1, passos++) {
            char x = doc_car(d, q);
            if (x == c) nivel++;
            else if (x == o && --nivel == 0) { a = p; b = q; break; }
        }
    }
    if (a != par_a || b != par_b) {
        if (par_a >= 0) vista_sujar(doc_linha_de(d, par_a), doc_linha_de(d, par_a));
        if (par_b >= 0) vista_sujar(doc_linha_de(d, par_b), doc_linha_de(d, par_b));
        par_a = a; par_b = b;
        if (a >= 0) { vista_sujar(doc_linha_de(d, a), doc_linha_de(d, a)); vista_sujar(doc_linha_de(d, b), doc_linha_de(d, b)); }
    }
}

/* ------------------------------------------------------------ dibujo */

static void desenhar_linha(int k)
{
    doc_t *d = DOC;
    int l = d->topo + k, y = r_editor.y + k * LETRA_A, tx = texto_x();
    int lc = doc_linha_de(d, d->cursor);
    bool atual = l == lc && foco == FOCO_EDITOR;
    uint32_t fundo = atual ? tema->linha_atual : tema->fundo;
    recorte = r_editor;
    d_ret(r_editor.x, y, margem, LETRA_A, tema->margem_fundo);
    d_ret(r_editor.x + margem, y, r_editor.l - margem - ROLAGEM, LETRA_A, fundo);
    if (l >= d->nli) return;
    char num[12];
    int nn = formatar_texto(num, sizeof num, "%d", l + 1);
    d_texto(r_editor.x + margem - 8 - nn * LETRA_L, y, num, nn, l == lc ? tema->margem_atual : tema->margem_texto);

    int n = doc_copiar_linha(d, l, lb, LINHA_MAX), ini = doc_ini(d, l);
    lb[n] = 0;
    realce_linha(d->ling, lb, n, doc_estado(d, l), cls);
    int s0, s1, esq = d->esq, cols = colunas();
    bool sel = vista_selecao(&s0, &s1);
    int bi[64], bf[64], nb = busca_modo != BUSCA_FECHADA && busca_modo != BUSCA_LINHA ? busca_na_linha(lb, n, bi, bf, 64) : 0, kb = 0;
    recorte = (ret_t){ r_editor.x + margem + 2, r_editor.y, r_editor.l - margem - 2 - ROLAGEM, r_editor.a };
    /* fondos: búsqueda, paréntesis, selección (en este orden, la selección por encima) */
    int v = 0;
    for (int i = 0; i <= n; i++) {
        int w = i < n && lb[i] == '\t' ? (v / tab_largura + 1) * tab_largura - v : 1;
        if (v + w > esq && v < esq + cols + 1) {
            int x = tx + (v - esq) * LETRA_L, p = ini + i;
            while (kb < nb && bf[kb] <= i) kb++;
            if (i < n && kb < nb && bi[kb] <= i) d_ret(x, y, w * LETRA_L, LETRA_A, tema->busca);
            if (p == par_a || p == par_b) d_ret(x, y, LETRA_L, LETRA_A, tema->par);
            if (sel && p >= s0 && p < s1) d_ret(x, y, (i < n ? w : 1) * LETRA_L, LETRA_A, tema->selecao);
        }
        v += w;
        if (v > esq + cols + 1) break;
    }
    v = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)lb[i];
        int w = c == '\t' ? (v / tab_largura + 1) * tab_largura - v : 1;
        if (v >= esq && c != '\t') d_letra(tx + (v - esq) * LETRA_L, y, c, tema->cl[cls[i]]);
        v += w;
        if (v > esq + cols + 1) break;
    }
    if (l == lc) {
        int cv = col_visual(d, l, d->cursor);
        if (cv >= esq && cv <= esq + cols)
            d_ret(tx + (cv - esq) * LETRA_L - 1, y, foco == FOCO_EDITOR ? 2 : 1, LETRA_A, tema->cursor);
    }
}

static void desenhar_barra(void)
{
    doc_t *d = DOC;
    recorte = r_editor;
    int x = r_editor.x + r_editor.l - ROLAGEM, vis = vista_linhas_visiveis();
    d_ret(x, r_editor.y, ROLAGEM, r_editor.a, tema->margem_fundo);
    int trilho = r_editor.a - 4, total = d->nli + vis - 1, pa = trilho, py = 0;
    if (total > vis) {
        pa = trilho * vis / total;
        if (pa < 16) pa = 16;
        py = (trilho - pa) * d->topo / (total - vis);
    }
    d_ret(x + 3, r_editor.y + 2 + py, ROLAGEM - 6, pa, tema->margem_texto);
    /* ¿Marcas de las ocurrencias de la búsqueda y de los problemas? Se mantiene simple: solo el cursor. */
    int lc = doc_linha_de(d, d->cursor);
    d_ret(x + 1, r_editor.y + 2 + (int)((unsigned)trilho * (unsigned)lc / (unsigned)(d->nli > 1 ? d->nli : 1)), ROLAGEM - 2, 2, tema->cursor);
}

void vista_desenhar(bool tudo)
{
    doc_t *d = DOC;
    int m = calc_margem();
    if (m != margem) { margem = m; tudo = true; }
    achar_par();
    int vis = vista_linhas_visiveis();
    if (tudo || suj_tudo) {
        unsigned t0 = tempo_ms();
        for (int k = 0; k <= vis; k++) desenhar_linha(k);
        recorte = r_editor;
        int resto = r_editor.a - vis * LETRA_A;   /* sobra de menos de una línea abajo */
        if (resto > 0 && d->topo + vis >= d->nli) { d_ret(r_editor.x, r_editor.y + vis * LETRA_A, margem, resto, tema->margem_fundo); }
        desenhar_barra();
        d_mostrar(r_editor);
        if (d->nli >= 10000)
            registrar("mico: dibujo completo, linea %d de %d, %u ms", d->topo + 1, d->nli, tempo_ms() - t0);
    } else if (suj_l1 >= 0) {
        int k0 = suj_l0 - d->topo, k1 = suj_l1 - d->topo;
        if (k0 < 0) k0 = 0;
        if (k1 > vis) k1 = vis;
        for (int k = k0; k <= k1; k++) desenhar_linha(k);
        desenhar_barra();
        if (k0 <= k1) {
            ret_t r = { r_editor.x, r_editor.y + k0 * LETRA_A, r_editor.l, (k1 - k0 + 1) * LETRA_A };
            if (r.y + r.a > r_editor.y + r_editor.a) r.a = r_editor.y + r_editor.a - r.y;
            d_mostrar(r);
        }
        d_mostrar((ret_t){ r_editor.x + r_editor.l - ROLAGEM, r_editor.y, ROLAGEM, r_editor.a });
    }
    suj_tudo = false;
    suj_l0 = 1 << 30;
    suj_l1 = -1;
}

void vista_doc_trocou(void)
{
    par_a = par_b = -1;
    arrastando = arrastando_barra = false;
    vista_sujar_tudo();
    sujo |= R_ESTADO | R_ABAS;
}

/* ------------------------------------------------------------ cursor y desplazamiento */

void vista_cursor_visivel(void)
{
    doc_t *d = DOC;
    int lin = doc_linha_de(d, d->cursor), vis = vista_linhas_visiveis(), topo = d->topo, esq = d->esq;
    if (lin < d->topo) d->topo = lin;
    if (lin >= d->topo + vis) d->topo = lin - vis + 1;
    if (d->topo < 0) d->topo = 0;
    int cv = col_visual(d, lin, d->cursor), cols = colunas();
    if (cv < d->esq) d->esq = cv > 4 ? cv - 4 : 0;
    if (cv >= d->esq + cols) d->esq = cv - cols + 8;
    if (d->topo != topo || d->esq != esq) vista_sujar_tudo();
    sujo |= R_ESTADO;
}

/* Pone el cursor en pos; seleccionar = extender la selección (Shift). Ensucia las
 * líneas que cambiaron de apariencia: la del cursor viejo, la del nuevo y las de la
 * selección vieja y nueva. */
void vista_ir(int pos, bool selecionar)
{
    doc_t *d = DOC;
    if (pos < 0) pos = 0;
    if (pos > d->tam) pos = d->tam;
    int a0 = d->ancora >= 0 ? d->ancora : d->cursor;
    int l_antes0 = doc_linha_de(d, a0 < d->cursor ? a0 : d->cursor), l_antes1 = doc_linha_de(d, a0 < d->cursor ? d->cursor : a0);
    if (selecionar) { if (d->ancora < 0) d->ancora = d->cursor; }
    else d->ancora = -1;
    d->cursor = pos;
    if (d->ancora == d->cursor && !selecionar) d->ancora = -1;
    vista_sujar(l_antes0, l_antes1);
    int l0, l1;
    linhas_sel(&l0, &l1);
    vista_sujar(l0, l1);
    vista_cursor_visivel();
}

void vista_selecionar(int a, int b)
{
    vista_ir(a, false);
    vista_ir(b, true);
}

void vista_selecionar_tudo(void) { vista_selecionar(0, DOC->tam); }

void vista_rodar(int n)
{
    doc_t *d = DOC;
    int antes = d->topo, vis = vista_linhas_visiveis();
    d->topo += n * RODA_LINHAS;
    if (d->topo > d->nli - vis) d->topo = d->nli - vis;
    if (d->topo < 0) d->topo = 0;
    if (d->topo != antes) vista_sujar_tudo();
}

/* ------------------------------------------------------------ editar */

static void unidade(char *u, int *n, int vcol)
{
    if (tab_espacos) { *n = tab_largura - vcol % tab_largura; memset(u, ' ', (size_t)*n); }
    else { u[0] = '\t'; *n = 1; }
}

/* Después de cualquier edición: qué redibujar. */
void vista_apos_edicao(void)
{
    doc_t *d = DOC;
    if (d->ed_so_linha) vista_sujar(d->ed_lin, d->ed_lin);
    else vista_sujar(d->ed_lin, d->topo + vista_linhas_visiveis() + 1);
    if (calc_margem() != margem) vista_sujar_tudo();
    d->col_desejada = -1;
    vista_cursor_visivel();
    sujo |= R_ABAS | R_ESTADO;
}

static bool apagar_selecao(void)
{
    int s0, s1;
    if (!vista_selecao(&s0, &s1)) { DOC->ancora = -1; return false; }
    doc_t *d = DOC;
    int l0 = doc_linha_de(d, s0), l1 = doc_linha_de(d, s1);
    doc_apagar(d, s0, s1 - s0);
    d->cursor = s0;
    d->ancora = -1;
    if (l1 != l0) d->ed_so_linha = false;
    return true;
}

static void inserir(const char *s, int n)
{
    doc_t *d = DOC;
    doc_inserir(d, d->cursor, s, n);
    d->cursor += n;
}

/* Sangría (espacios y tabs del comienzo) de la línea l, en bytes. */
static int recuo(doc_t *d, int l)
{
    int i = doc_ini(d, l), f = doc_fim(d, l), k = 0;
    while (i + k < f && (doc_car(d, i + k) == ' ' || doc_car(d, i + k) == '\t')) k++;
    return k;
}

void vista_inserir_texto(const char *s, int n)
{
    doc_t *d = DOC;
    doc_grupo(d, true);
    apagar_selecao();
    inserir(s, n);
    doc_grupo(d, false);
    vista_apos_edicao();
}

/* Enter con sangría automática: copia la sangría de la línea y agrega un nivel
 * después de "{", "(", "[" o, en Python, de ":". Entre "{" y "}" la llave de
 * cierre baja a su propia línea. */
static void enter(void)
{
    doc_t *d = DOC;
    doc_grupo(d, true);
    apagar_selecao();
    int l = doc_linha_de(d, d->cursor), ini = doc_ini(d, l), k = recuo(d, l);
    if (k > d->cursor - ini) k = d->cursor - ini;
    char b[256 + 2];
    int n = 0;
    b[n++] = '\n';
    for (int i = 0; i < k && n < 200; i++) b[n++] = doc_car(d, ini + i);
    int p = d->cursor - 1;
    while (p >= ini && (doc_car(d, p) == ' ' || doc_car(d, p) == '\t')) p--;
    char antes = p >= ini ? doc_car(d, p) : 0, depois = doc_car(d, d->cursor);
    bool abre = antes == '{' || antes == '(' || antes == '[' || (d->ling == LING_PY && antes == ':');
    if (d->ling == LING_TEXTO || d->ling == LING_MD) abre = false;
    if (abre) {
        char u[16];
        int un, vc = col_visual(d, l, ini + k);
        unidade(u, &un, vc);
        int base = n;
        memcpy(b + n, u, (size_t)un); n += un;
        if (par_de(antes) == depois && depois) {
            inserir(b, n);
            doc_inserir(d, d->cursor, b, base);   /* "\n" + sangría antes del cierre, que queda abajo */
            doc_grupo(d, false);
            d->ed_so_linha = false;
            vista_apos_edicao();
            return;
        }
    }
    inserir(b, n);
    doc_grupo(d, false);
    vista_apos_edicao();
}

/* Tab con varias líneas seleccionadas: sangra todas; Mayús+Tab: deshace un nivel. */
static void recuar_linhas(bool menos)
{
    doc_t *d = DOC;
    int l0, l1, s0, s1;
    bool sel = vista_selecao(&s0, &s1);
    linhas_sel(&l0, &l1);
    if (sel && l1 > l0 && s1 == doc_ini(d, l1)) l1--;
    doc_grupo(d, true);
    for (int l = l0; l <= l1; l++) {
        int ini = doc_ini(d, l);
        if (menos) {
            int k = 0;
            if (doc_car(d, ini) == '\t') k = 1;
            else while (k < tab_largura && doc_car(d, ini + k) == ' ') k++;
            if (k) doc_apagar(d, ini, k);
        } else if (doc_tam_linha(d, l) > 0) {
            char u[16];
            int un;
            unidade(u, &un, 0);
            doc_inserir(d, ini, u, un);
        }
    }
    doc_grupo(d, false);
    d->ancora = doc_ini(d, l0);
    d->cursor = doc_fim(d, l1);
    d->ed_lin = l0;
    d->ed_so_linha = false;
    vista_apos_edicao();
}

void vista_comentar(void)
{
    doc_t *d = DOC;
    const char *pre = comentario_de(d->ling);
    if (!pre) { avisar("%s no tiene comentarios de línea", nome_ling[d->ling]); return; }
    int pn = (int)tam_texto(pre), l0, l1, s0, s1;
    bool sel = vista_selecao(&s0, &s1);
    linhas_sel(&l0, &l1);
    if (sel && l1 > l0 && s1 == doc_ini(d, l1)) l1--;
    /* ¿Todas (las no vacías) ya comentadas? Entonces descomenta. */
    bool todas = true;
    int menor = 1 << 30;
    for (int l = l0; l <= l1; l++) {
        int k = recuo(d, l);
        if (k == doc_tam_linha(d, l)) continue;
        if (k < menor) menor = k;
        char t[8];
        doc_pedaco(d, doc_ini(d, l) + k, pn, t);
        if (compara_texto_n(t, pre, (size_t)pn) != 0) todas = false;
    }
    if (menor == 1 << 30) return;
    doc_grupo(d, true);
    for (int l = l0; l <= l1; l++) {
        int k = recuo(d, l), ini = doc_ini(d, l);
        if (k == doc_tam_linha(d, l)) continue;
        if (todas) doc_apagar(d, ini + k, pn + (doc_car(d, ini + k + pn) == ' ' ? 1 : 0));
        else {
            char t[8];
            formatar_texto(t, sizeof t, "%s ", pre);
            doc_inserir(d, ini + menor, t, pn + 1);
        }
    }
    doc_grupo(d, false);
    if (sel) { d->ancora = doc_ini(d, l0); d->cursor = doc_fim(d, l1); }
    else d->cursor = doc_fim(d, l0) < d->cursor ? doc_fim(d, l0) : d->cursor;
    d->ed_lin = l0;
    d->ed_so_linha = l0 == l1;
    registrar("mico: %s lineas %d-%d", todas ? "descomentadas" : "comentadas", l0 + 1, l1 + 1);
    vista_apos_edicao();
}

void vista_duplicar(void)
{
    doc_t *d = DOC;
    int l0, l1;
    linhas_sel(&l0, &l1);
    int ini = doc_ini(d, l0), fim = doc_fim(d, l1), n = fim - ini;
    char *t = aloca((size_t)n + 1);
    if (!t) return;
    t[0] = '\n';
    doc_pedaco(d, ini, n, t + 1);
    doc_inserir(d, fim, t, n + 1);
    libera(t);
    d->cursor += n + 1;
    if (d->ancora >= 0) d->ancora += n + 1;
    d->ed_so_linha = false;
    vista_apos_edicao();
}

/* Alt+Arriba / Alt+Abajo: cambia el bloque de líneas con la vecina. */
void vista_mover_linhas(int dir)
{
    doc_t *d = DOC;
    int l0, l1, s0, s1;
    bool sel = vista_selecao(&s0, &s1);
    linhas_sel(&l0, &l1);
    if (sel && l1 > l0 && s1 == doc_ini(d, l1)) l1--;
    if ((dir < 0 && l0 == 0) || (dir > 0 && l1 >= d->nli - 1)) return;
    int a = dir < 0 ? l0 - 1 : l0, b = dir < 0 ? l1 : l1 + 1;
    int ini = doc_ini(d, a), fim = doc_fim(d, b), n = fim - ini;
    char *t = aloca((size_t)n + 1), *u = aloca((size_t)n + 1);
    if (!t || !u) { libera(t); libera(u); return; }
    doc_pedaco(d, ini, n, t);
    int viz = dir < 0 ? doc_tam_linha(d, a) : doc_tam_linha(d, b);   /* la línea que cambia de lugar */
    int bloco = n - viz - 1;
    if (dir < 0) { memcpy(u, t + viz + 1, (size_t)bloco); u[bloco] = '\n'; memcpy(u + bloco + 1, t, (size_t)viz); }
    else { memcpy(u, t + bloco + 1, (size_t)viz); u[viz] = '\n'; memcpy(u + viz + 1, t, (size_t)bloco); }
    int cur = d->cursor, anc = d->ancora;
    doc_grupo(d, true);
    doc_apagar(d, ini, n);
    doc_inserir(d, ini, u, n);
    doc_grupo(d, false);
    libera(t); libera(u);
    int desl = dir < 0 ? -(viz + 1) : viz + 1;
    d->cursor = cur + desl;
    d->ancora = anc >= 0 ? anc + desl : -1;
    d->ed_lin = a;
    d->ed_so_linha = false;
    vista_apos_edicao();
}

void vista_desfazer(bool refazer)
{
    doc_t *d = DOC;
    if (!(refazer ? doc_refazer(d) : doc_desfazer(d))) { avisar(refazer ? "Nada que rehacer" : "Nada que deshacer"); return; }
    vista_apos_edicao();
    registrar("mico: %s, documento %d bytes hash %08x", refazer ? "rehacer" : "deshacer", d->tam, (unsigned)hash_texto(doc_texto(d), d->tam));
}

void vista_copiar(void)
{
    doc_t *d = DOC;
    int s0, s1;
    if (!vista_selecao(&s0, &s1)) {   /* sin selección: la línea entera, como los editores de código */
        int l = doc_linha_de(d, d->cursor);
        s0 = doc_ini(d, l);
        s1 = l + 1 < d->nli ? doc_ini(d, l + 1) : doc_fim(d, l);
    }
    int n = s1 - s0;
    if (n > PORTAPAPEIS_MAX) n = PORTAPAPEIS_MAX;
    char *t = aloca((size_t)n + 1);
    if (!t) return;
    doc_pedaco(d, s0, n, t);
    copiar_texto(t, (unsigned)n);
    libera(t);
    registrar("mico: copiado %d bytes", n);
    avisar("Copiado (%d caracteres)", n);
}

void vista_cortar(void)
{
    doc_t *d = DOC;
    int s0, s1;
    if (!vista_selecao(&s0, &s1)) {
        int l = doc_linha_de(d, d->cursor);
        d->ancora = doc_ini(d, l);
        d->cursor = l + 1 < d->nli ? doc_ini(d, l + 1) : doc_fim(d, l);
    }
    vista_copiar();
    apagar_selecao();
    d->ed_so_linha = false;
    vista_apos_edicao();
}

void vista_colar(void)
{
    int n = colar_texto(NULL, 0);
    if (n <= 0) { avisar("El portapapeles está vacío"); return; }
    char *b = aloca((size_t)n + 1);
    if (!b) return;
    n = colar_texto(b, (unsigned)n + 1);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r' && b[i] != 0) b[k++] = b[i];
    vista_inserir_texto(b, k);
    libera(b);
    registrar("mico: pegado %d bytes", k);
}

/* Ctrl+Espacio: completa la palabra con otra del propio documento (la más
 * cercana antes del cursor, después las de después). Apretar de nuevo cambia por la
 * próxima candidata. */
void vista_completar(void)
{
    static int ult_pos = -1, ult_versao, ult_n, idx;
    static char prefixo[64];
    doc_t *d = DOC;
    if (!(ult_pos == d->cursor && ult_versao == d->versao)) {
        int p = d->cursor;
        while (p > 0 && palavra_car(doc_car(d, p - 1)) && d->cursor - p < 63) p--;
        int n = d->cursor - p;
        if (!n) return;
        doc_pedaco(d, p, n, prefixo);
        prefixo[n] = 0;
        idx = 0;
        ult_n = 0;
    } else {
        doc_apagar(d, d->cursor - ult_n, ult_n);
        d->cursor -= ult_n;
        idx++;
    }
    const char *s = doc_texto(d);
    int pn = (int)tam_texto(prefixo), achou = 0;
    char cand[64] = "";
    char vistos[16][64];
    int qv = 0;
    for (int volta = 0; volta < 2 && !cand[0]; volta++) {
        for (int i = 0; i + pn < d->tam && !cand[0]; i++) {
            int pos = volta == 0 ? d->cursor - pn - 1 - i : d->cursor + i;
            if (volta == 0 && pos < 0) break;
            if (pos < 0 || pos >= d->tam) continue;
            if (pos > 0 && palavra_car(s[pos - 1])) continue;
            if (compara_texto_n(s + pos, prefixo, (size_t)pn) != 0) continue;
            int f = pos + pn;
            while (f < d->tam && palavra_car(s[f]) && f - pos < 63) f++;
            if (f - pos <= pn || pos == d->cursor - pn) continue;
            char w[64];
            memcpy(w, s + pos, (size_t)(f - pos));
            w[f - pos] = 0;
            bool repetida = false;
            for (int k = 0; k < qv; k++) if (compara_texto(vistos[k], w) == 0) repetida = true;
            if (repetida) continue;
            if (qv < 16) copia_texto(vistos[qv++], w, 64);
            if (achou++ == idx) copia_texto(cand, w, sizeof cand);
        }
    }
    if (!cand[0]) {
        if (idx > 0) { idx = -1; ult_pos = d->cursor; ult_versao = d->versao; ult_n = 0; }
        avisar("Sin más palabras para «%s»", prefixo);
        vista_apos_edicao();
        return;
    }
    int n = (int)tam_texto(cand) - pn;
    doc_inserir(d, d->cursor, cand + pn, n);
    d->cursor += n;
    ult_n = n;
    ult_pos = d->cursor;
    ult_versao = d->versao;
    vista_apos_edicao();
}

/* ------------------------------------------------------------ teclado */

static int palavra_ao_lado(doc_t *d, int p, int dir)
{
    if (dir < 0) {
        while (p > 0 && !palavra_car(doc_car(d, p - 1))) p--;
        while (p > 0 && palavra_car(doc_car(d, p - 1))) p--;
    } else {
        while (p < d->tam && !palavra_car(doc_car(d, p))) p++;
        while (p < d->tam && palavra_car(doc_car(d, p))) p++;
    }
    return p;
}

bool vista_tecla(unsigned t, unsigned mods)
{
    doc_t *d = DOC;
    bool shift = mods & M_SHIFT, ctrl = mods & M_CTRL, alt = mods & M_ALT;
    int lin = doc_linha_de(d, d->cursor), vis = vista_linhas_visiveis();
    int col = d->col_desejada >= 0 ? d->col_desejada : col_visual(d, lin, d->cursor);
    if (alt && (t == TECLA_CIMA || t == TECLA_BAIXO)) { vista_mover_linhas(t == TECLA_CIMA ? -1 : 1); return true; }
    if (ctrl && (t == TECLA_CIMA || t == TECLA_BAIXO)) {   /* desplaza sin mover el cursor */
        d->topo += t == TECLA_CIMA ? -1 : 1;
        if (d->topo < 0) d->topo = 0;
        if (d->topo > d->nli - 1) d->topo = d->nli - 1;
        vista_sujar_tudo();
        return true;
    }
    int destino = -1, sa, sb;
    bool vertical = false;
    switch (t) {
    case TECLA_ESQUERDA:   /* con selección y sin Shift: va a su punta */
        if (!shift && vista_selecao(&sa, &sb)) destino = sa;
        else destino = ctrl ? palavra_ao_lado(d, d->cursor, -1) : (d->cursor > 0 ? d->cursor - 1 : 0);
        break;
    case TECLA_DIREITA:
        if (!shift && vista_selecao(&sa, &sb)) destino = sb;
        else destino = ctrl ? palavra_ao_lado(d, d->cursor, 1) : d->cursor + 1;
        break;
    case TECLA_CIMA: if (lin > 0) destino = pos_de_col(d, lin - 1, col); else destino = 0; vertical = true; break;
    case TECLA_BAIXO: if (lin < d->nli - 1) destino = pos_de_col(d, lin + 1, col); else destino = d->tam; vertical = true; break;
    case TECLA_PGUP: {
        int l = lin - vis < 0 ? 0 : lin - vis;
        d->topo -= vis; if (d->topo < 0) d->topo = 0;
        vista_sujar_tudo();
        destino = pos_de_col(d, l, col); vertical = true;
        break;
    }
    case TECLA_PGDN: {
        int l = lin + vis > d->nli - 1 ? d->nli - 1 : lin + vis;
        d->topo += vis; if (d->topo > d->nli - vis) d->topo = d->nli - vis; if (d->topo < 0) d->topo = 0;
        vista_sujar_tudo();
        destino = pos_de_col(d, l, col); vertical = true;
        break;
    }
    case TECLA_HOME:
        if (ctrl) destino = 0;
        else {   /* primero el comienzo del texto de la línea, después la columna 0 */
            int p = doc_ini(d, lin) + recuo(d, lin);
            destino = d->cursor == p ? doc_ini(d, lin) : p;
        }
        break;
    case TECLA_END: destino = ctrl ? d->tam : doc_fim(d, lin); break;
    }
    if (destino >= 0) {
        int cd = vertical ? col : -1;
        vista_ir(destino, shift);
        d->col_desejada = cd;
        return true;
    }
    if (t == '\b') {
        if (apagar_selecao()) { vista_apos_edicao(); return true; }
        if (d->cursor == 0) return true;
        int ini = doc_ini(d, lin), k = recuo(d, lin);
        char a = doc_car(d, d->cursor - 1), b = doc_car(d, d->cursor);
        if (par_de(a) && par_de(a) == b && (a == '(' || a == '[' || a == '{')) { doc_apagar(d, d->cursor - 1, 2); d->cursor--; }
        else if ((a == '"' || a == '\'') && b == a) { doc_apagar(d, d->cursor - 1, 2); d->cursor--; }
        else if (tab_espacos && d->cursor - ini <= k && d->cursor > ini && a == ' ') {
            /* en la sangría: vuelve hasta la columna de tabulación anterior */
            int cv = col_visual(d, lin, d->cursor), alvo = (cv - 1) / tab_largura * tab_largura, n = 0;
            while (n < cv - alvo && doc_car(d, d->cursor - 1 - n) == ' ') n++;
            doc_apagar(d, d->cursor - n, n);
            d->cursor -= n;
        } else { doc_apagar(d, d->cursor - 1, 1); d->cursor--; }
        vista_apos_edicao();
        return true;
    }
    if (t == TECLA_DELETE) {
        if (!apagar_selecao()) doc_apagar(d, d->cursor, 1);
        vista_apos_edicao();
        return true;
    }
    if (t == '\n') { enter(); return true; }
    if (t == '\t') {
        int l0, l1;
        linhas_sel(&l0, &l1);
        if (shift || l1 > l0) { recuar_linhas(shift); return true; }
        char u[16];
        int un;
        apagar_selecao();
        unidade(u, &un, col_visual(d, doc_linha_de(d, d->cursor), d->cursor));
        inserir(u, un);
        vista_apos_edicao();
        return true;
    }
    if (t >= 0x20 && t < 0x100 && t != 0x7F && !ctrl) {
        char c = (char)t, prox = doc_car(d, d->cursor);
        bool codigo = d->ling != LING_TEXTO;
        int s0, s1;
        bool sel = vista_selecao(&s0, &s1);
        /* cierre igual al que ya está a la derecha: solo salta */
        if (codigo && !sel && (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') && prox == c) {
            vista_ir(d->cursor + 1, false);
            return true;
        }
        doc_grupo(d, true);
        apagar_selecao();
        char par = 0;
        if (codigo && (c == '(' || c == '[' || c == '{') && (!prox || !palavra_car(prox))) par = par_de(c);
        if (codigo && (c == '"' || c == '\'') && (!prox || !palavra_car(prox)) &&
            !(d->cursor > 0 && palavra_car(doc_car(d, d->cursor - 1))) && d->ling != LING_MD) par = c;
        if (c == '}' && d->ling == LING_C) {   /* "}" solo en la línea: un nivel menos */
            int l = doc_linha_de(d, d->cursor), ini = doc_ini(d, l), k = recuo(d, l);
            if (k == d->cursor - ini && k == doc_tam_linha(d, l) && k > 0) {
                int tira = 0;
                if (doc_car(d, ini + k - 1) == '\t') tira = 1;
                else while (tira < tab_largura && tira < k && doc_car(d, ini + k - 1 - tira) == ' ') tira++;
                doc_apagar(d, d->cursor - tira, tira);
                d->cursor -= tira;
            }
        }
        if (par) { char b[2] = { c, par }; doc_inserir(d, d->cursor, b, 2); d->cursor++; }
        else inserir(&c, 1);
        doc_grupo(d, false);
        vista_apos_edicao();
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ mouse */

static int posicao_em(int x, int y)
{
    doc_t *d = DOC;
    int l = d->topo + (y - r_editor.y) / LETRA_A;
    if (y < r_editor.y) l = d->topo - 1;
    if (l < 0) return 0;
    if (l >= d->nli) return d->tam;
    int vc = d->esq + (x - texto_x() + LETRA_L / 2) / LETRA_L;
    if (x < texto_x()) vc = d->esq + (x - texto_x()) / LETRA_L;
    if (vc < 0) vc = 0;
    return pos_de_col(d, l, vc);
}

void vista_clique(int x, int y, int botoes, int cliques, unsigned mods)
{
    doc_t *d = DOC;
    if (!(botoes & 1)) return;
    int bx = r_editor.x + r_editor.l - ROLAGEM;
    if (x >= bx) {   /* barra de desplazamiento: arrastrar el pulgar, o una página */
        int vis = vista_linhas_visiveis(), trilho = r_editor.a - 4, total = d->nli + vis - 1, pa = trilho, py = 0;
        if (total > vis) { pa = trilho * vis / total; if (pa < 16) pa = 16; py = (trilho - pa) * d->topo / (total - vis); }
        int py_abs = r_editor.y + 2 + py;
        if (y >= py_abs && y < py_abs + pa) { arrastando_barra = true; barra_dy = y - py_abs; return; }
        d->topo += y < py_abs ? -vis : vis;
        if (d->topo > d->nli - vis) d->topo = d->nli - vis;
        if (d->topo < 0) d->topo = 0;
        vista_sujar_tudo();
        return;
    }
    if (x < r_editor.x + margem) {   /* en el margen de los números: la línea entera */
        int l = d->topo + (y - r_editor.y) / LETRA_A;
        if (l >= d->nli) l = d->nli - 1;
        vista_selecionar(doc_ini(d, l), l + 1 < d->nli ? doc_ini(d, l + 1) : doc_fim(d, l));
        return;
    }
    int p = posicao_em(x, y);
    unsigned agora = tempo_ms();
    if (cliques == 1 && agora - ult_duplo < DUPLO_MS && ult_duplo_pos >= 0 && doc_linha_de(d, p) == doc_linha_de(d, ult_duplo_pos)) {
        int l = doc_linha_de(d, p);   /* triple clic: la línea */
        vista_selecionar(doc_ini(d, l), l + 1 < d->nli ? doc_ini(d, l + 1) : doc_fim(d, l));
        ult_duplo_pos = -1;
        registrar("mico: clic triple, linea %d", l + 1);
        return;
    }
    if (cliques >= 2) {   /* doble: la palabra */
        int a = p, b = p;
        while (a > 0 && palavra_car(doc_car(d, a - 1))) a--;
        while (b < d->tam && palavra_car(doc_car(d, b))) b++;
        if (a == b && b < d->tam) b++;
        vista_selecionar(a, b);
        ult_duplo = agora;
        ult_duplo_pos = p;
        registrar("mico: doble clic, palabra de %d letras", b - a);
        return;
    }
    vista_ir(p, mods & M_SHIFT);
    arrastando = true;
}

void vista_mover(int x, int y)
{
    doc_t *d = DOC;
    if (arrastando_barra) {
        int vis = vista_linhas_visiveis(), trilho = r_editor.a - 4, total = d->nli + vis - 1, pa = trilho;
        if (total > vis) { pa = trilho * vis / total; if (pa < 16) pa = 16; }
        if (trilho > pa) {
            int novo = (y - barra_dy - r_editor.y - 2) * (total - vis) / (trilho - pa);
            if (novo > d->nli - vis) novo = d->nli - vis;
            if (novo < 0) novo = 0;
            if (novo != d->topo) { d->topo = novo; vista_sujar_tudo(); }
        }
        return;
    }
    if (!arrastando) return;
    if (y < r_editor.y && d->topo > 0) { d->topo--; vista_sujar_tudo(); }
    if (y >= r_editor.y + r_editor.a && d->topo < d->nli - 1) { d->topo++; vista_sujar_tudo(); }
    int p = posicao_em(x, y);
    if (p != d->cursor) vista_ir(p, true);
}

void vista_soltar(void)
{
    arrastando = arrastando_barra = false;
    doc_t *d = DOC;
    if (d->ancora == d->cursor) d->ancora = -1;
}
