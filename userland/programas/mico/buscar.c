// SPDX-License-Identifier: GPL-2.0-or-later
/* buscar.c - la barra de búsqueda del Mico (encima del editor).
 *
 * Cuatro modos con la misma barra: Buscar (Ctrl+F), Reemplazar (Ctrl+H, una
 * segunda línea con el texto nuevo), Buscar en la carpeta (Ctrl+Mayús+F, los
 * resultados van a la pestaña Búsqueda del panel) e Ir a la línea (Ctrl+G,
 * "número" o "número:columna").
 *
 * La búsqueda es incremental: cada letra escrita busca de nuevo desde el
 * comienzo de la selección actual. Opciones: Mayúsculas (Alt+M, distinguir) y Palabra
 * completa (Alt+P); también se pulsan en la barra. Las ocurrencias visibles
 * reciben un fondo propio (vista.c pregunta busca_na_linha). */
#include "mico.h"

int busca_modo = BUSCA_FECHADA;
bool busca_mai, busca_palavra;
static char q[128], troca[128];
static int campo;                 /* 0 = buscar, 1 = reemplazar */
static bool trocar_q;             /* el texto que ya estaba en el campo se reemplaza con la primera tecla */
static int total = -1, atual, versao_contada = -1;
static ret_t b_mai, b_pal, b_um, b_todos;

#define LINHA_A 28

int busca_altura(void) { return busca_modo == BUSCA_FECHADA ? 0 : busca_modo == BUSCA_TROCAR ? 2 * LINHA_A : LINHA_A; }

static unsigned char min_l1(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c + 32);
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return (unsigned char)(c + 32);   /* À-Þ (menos ×) */
    return c;
}

static bool pal(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (unsigned char)c >= 0xC0; }

static bool casa(const char *s, int n, int i, const char *x, int xn, bool mai, bool palavra)
{
    if (i + xn > n) return false;
    for (int k = 0; k < xn; k++) {
        unsigned char a = (unsigned char)s[i + k], b = (unsigned char)x[k];
        if (mai ? a != b : min_l1(a) != min_l1(b)) return false;
    }
    if (palavra && ((i > 0 && pal(s[i - 1])) || (i + xn < n && pal(s[i + xn])))) return false;
    return true;
}

bool acha_em(const char *s, int n, const char *x, int xn, bool mai, bool palavra, int de, int *onde)
{
    if (xn <= 0) return false;
    unsigned char p = min_l1((unsigned char)x[0]);
    for (int i = de < 0 ? 0 : de; i + xn <= n; i++) {
        if (min_l1((unsigned char)s[i]) != p) continue;   /* filtro rápido por la primera letra */
        if (casa(s, n, i, x, xn, mai, palavra)) { *onde = i; return true; }
    }
    return false;
}

int busca_na_linha(const char *s, int n, int *ini, int *fim, int max)
{
    int qn = (int)tam_texto(q), k = 0, i = 0, onde;
    if (!qn || busca_modo == BUSCA_LINHA) return 0;
    while (k < max && acha_em(s, n, q, qn, busca_mai, busca_palavra, i, &onde)) {
        ini[k] = onde; fim[k] = onde + qn; k++;
        i = onde + qn;
    }
    return k;
}

static void contar(void)
{
    doc_t *d = DOC;
    int qn = (int)tam_texto(q);
    total = 0;
    atual = 0;
    versao_contada = d->versao;
    if (!qn) return;
    const char *s = doc_texto(d);
    int i = 0, onde, s0, s1;
    bool sel = vista_selecao(&s0, &s1);
    while (total < 100000 && acha_em(s, d->tam, q, qn, busca_mai, busca_palavra, i, &onde)) {
        total++;
        if (sel && onde == s0) atual = total;
        i = onde + 1;
    }
}

/* Busca desde "de" hacia adelante (dir > 0) o desde "de" hacia atrás, dando la
 * vuelta en el documento. */
static bool procurar(int de, int dir)
{
    doc_t *d = DOC;
    int qn = (int)tam_texto(q), onde = -1;
    if (!qn) return false;
    const char *s = doc_texto(d);
    if (dir > 0) {
        if (!acha_em(s, d->tam, q, qn, busca_mai, busca_palavra, de, &onde) &&
            !acha_em(s, d->tam, q, qn, busca_mai, busca_palavra, 0, &onde)) onde = -1;
    } else {
        for (int pass = 0; pass < 2 && onde < 0; pass++)
            for (int i = pass == 0 ? de : d->tam - qn; i >= 0; i--)
                if (casa(s, d->tam, i, q, qn, busca_mai, busca_palavra)) { onde = i; break; }
    }
    if (onde < 0) {
        avisar("No se encontró «%s»", q);
        registrar("mico: buscar '%s' sin resultados", q);
        total = 0;
        sujo |= R_BUSCA;
        return false;
    }
    vista_selecionar(onde, onde + qn);
    contar();
    registrar("mico: buscar '%s' -> linea %d (%d de %d)", q, doc_linha_de(d, onde) + 1, atual, total);
    sujo |= R_BUSCA | R_ESTADO;
    return true;
}

bool busca_proximo(int dir)
{
    if (!q[0]) { busca_abrir(BUSCA_BUSCAR); return false; }
    int s0, s1;
    doc_t *d = DOC;
    if (!vista_selecao(&s0, &s1)) s0 = s1 = d->cursor;
    return procurar(dir > 0 ? s1 : s0 - 1, dir);
}

static void incremental(void)
{
    int s0, s1;
    if (!vista_selecao(&s0, &s1)) s0 = DOC->cursor;
    vista_sujar_tudo();
    if (q[0]) procurar(s0, 1);
    else { total = -1; sujo |= R_BUSCA; }
}

static bool trocar_um(void)
{
    doc_t *d = DOC;
    int s0, s1, qn = (int)tam_texto(q), tn = (int)tam_texto(troca);
    if (vista_selecao(&s0, &s1) && s1 - s0 == qn && casa(doc_texto(d), d->tam, s0, q, qn, busca_mai, busca_palavra)) {
        doc_grupo(d, true);
        doc_apagar(d, s0, qn);
        doc_inserir(d, s0, troca, tn);
        doc_grupo(d, false);
        d->ancora = -1;
        d->cursor = s0 + tn;
        vista_apos_edicao();
        registrar("mico: reemplazado en la linea %d", doc_linha_de(d, s0) + 1);
    }
    return procurar(d->cursor, 1);
}

int busca_trocar_todos(void)
{
    doc_t *d = DOC;
    int qn = (int)tam_texto(q), tn = (int)tam_texto(troca);
    if (!qn) return 0;
    const char *s = doc_texto(d);
    int cap = 256, n = 0, i = 0, onde;
    int *pos = aloca(sizeof(int) * (size_t)cap);
    if (!pos) return 0;
    while (acha_em(s, d->tam, q, qn, busca_mai, busca_palavra, i, &onde)) {
        if (n == cap) { int *p = realoca(pos, sizeof(int) * (size_t)cap * 2); if (!p) break; pos = p; cap *= 2; }
        pos[n++] = onde;
        i = onde + qn;
    }
    if (n) {
        /* Hacia atrás: las posiciones de antes siguen valiendo. */
        doc_grupo(d, true);
        for (int k = n - 1; k >= 0; k--) {
            doc_apagar(d, pos[k], qn);
            doc_inserir(d, pos[k], troca, tn);
        }
        doc_grupo(d, false);
        d->ancora = -1;
        if (d->cursor > d->tam) d->cursor = d->tam;
        d->ed_lin = doc_linha_de(d, pos[0]);
        d->ed_so_linha = false;
        vista_apos_edicao();
        vista_sujar_tudo();
    }
    libera(pos);
    registrar("mico: reemplazadas %d apariciones de '%s'", n, q);
    avisar("%d reemplazo%s", n, n == 1 ? "" : "s");
    total = -1;
    sujo |= R_BUSCA;
    return n;
}

void busca_na_pasta(void)
{
    if (!pasta[0]) { avisar("Abre una carpeta primero (Archivo > Abrir carpeta)"); return; }
    int qn = (int)tam_texto(q);
    if (!qn) return;
    char (*lista)[CAMINHO_MAX] = aloca(sizeof(*lista) * 1000);
    if (!lista) return;
    unsigned t0 = tempo_ms();
    int na = expl_arquivos(lista, 1000), com = 0;
    busca_res_limpar();
    for (int a = 0; a < na && busca_res_qtd() < 400; a++) {
        info_arquivo_t inf;
        if (info_arquivo(lista[a], &inf) < 0 || inf.tamanho > 1024 * 1024) continue;
        int n, erro;
        char *s = ler_arquivo(lista[a], &n, &erro, &(bool){ false });
        if (!s) continue;
        bool achou = false;
        for (int i = 0, l = 0, ini = 0; i <= n; i++) {
            if (i < n && s[i] != '\n') continue;
            int onde;
            if (acha_em(s + ini, i - ini, q, qn, busca_mai, busca_palavra, 0, &onde)) {
                char t[160];
                int k = i - ini < 159 ? i - ini : 159;
                memcpy(t, s + ini, (size_t)k);
                t[k] = 0;
                busca_res_add(lista[a], l + 1, onde + 1, t);
                achou = true;
            }
            l++;
            ini = i + 1;
        }
        com += achou;
        libera(s);
    }
    libera(lista);
    registrar("mico: busqueda en carpeta '%s': %d resultados en %d archivos (%d revisados, %u ms)", q, busca_res_qtd(), com, na, tempo_ms() - t0);
    avisar("%d resultado%s en %d archivo%s", busca_res_qtd(), busca_res_qtd() == 1 ? "" : "s", com, com == 1 ? "" : "s");
    ver_painel = true;
    painel_aba = PAINEL_BUSCA;
    layout();
}

static void ir_linha(void)
{
    doc_t *d = DOC;
    int l = 0, c = 0, i = 0;
    while (q[i] >= '0' && q[i] <= '9') l = l * 10 + (q[i++] - '0');
    if (q[i] == ':' || q[i] == ',') { i++; while (q[i] >= '0' && q[i] <= '9') c = c * 10 + (q[i++] - '0'); }
    if (l < 1) return;
    if (l > d->nli) l = d->nli;
    int p = doc_ini(d, l - 1) + (c > 1 ? c - 1 : 0);
    if (p > doc_fim(d, l - 1)) p = doc_fim(d, l - 1);
    vista_ir(p, false);
    /* la línea pedida en el medio de la pantalla, no pegada al borde */
    int vis = vista_linhas_visiveis();
    d->topo = l - 1 - vis / 3;
    if (d->topo > d->nli - vis) d->topo = d->nli - vis;
    if (d->topo < 0) d->topo = 0;
    vista_sujar_tudo();
    registrar("mico: ir a la linea %d", l);
    q[0] = 0;
    busca_fechar();
}

/* ------------------------------------------------------------ abrir y cerrar */

void busca_abrir(int modo)
{
    int s0, s1;
    doc_t *d = DOC;
    if (modo == BUSCA_LINHA) { q[0] = 0; }
    else if (vista_selecao(&s0, &s1) && s1 - s0 < (int)sizeof q && doc_linha_de(d, s0) == doc_linha_de(d, s1)) {
        doc_pedaco(d, s0, s1 - s0, q);   /* la selección se vuelve el texto buscado */
        q[s1 - s0] = 0;
    } else if (busca_modo == BUSCA_LINHA) q[0] = 0;
    busca_modo = modo;
    trocar_q = q[0] != 0;
    foco = FOCO_EDITOR;   /* la barra pertenece al editor: las teclas van a ella */
    campo = 0;
    total = -1;
    layout();
    registrar("mico: barra de busqueda (modo %d)", modo);
}

void busca_fechar(void)
{
    if (busca_modo == BUSCA_FECHADA) return;
    busca_modo = BUSCA_FECHADA;
    layout();
}

/* ------------------------------------------------------------ dibujo */

static void caixa(int x, int y, int l, const char *t, bool ativa, bool selecionado)
{
    d_ret(x, y, l, 22, tema->fundo);
    d_moldura(x, y, l, 22, ativa ? tema->destaque : tema->barra_borda);
    int n = (int)tam_texto(t), cabe = (l - 12) / LETRA_L, ini = n > cabe ? n - cabe : 0;
    if (selecionado && n) d_ret(x + 5, y + 3, (n - ini) * LETRA_L, LETRA_A, tema->selecao);
    d_texto(x + 5, y + 3, t + ini, n - ini, tema->texto);
    if (ativa) d_ret(x + 5 + (n - ini) * LETRA_L, y + 3, 2, LETRA_A, tema->cursor);
}

static ret_t botao(int x, int y, const char *t, bool ligado)
{
    int l = (int)tam_texto(t) * LETRA_L + 12;
    d_ret(x, y, l, 22, ligado ? tema->destaque : tema->aba);
    d_moldura(x, y, l, 22, tema->barra_borda);
    d_texto(x + 6, y + 3, t, (int)tam_texto(t), ligado ? 0xFFFFFF : tema->barra_texto);
    return (ret_t){ x, y, l, 22 };
}

void busca_desenhar(void)
{
    if (busca_modo == BUSCA_FECHADA) return;
    if (busca_modo != BUSCA_LINHA && total >= 0 && versao_contada != DOC->versao) contar();
    recorte = r_busca;
    d_ret(r_busca.x, r_busca.y, r_busca.l, r_busca.a, tema->barra);
    d_ret(r_busca.x, r_busca.y + r_busca.a - 1, r_busca.l, 1, tema->barra_borda);
    const char *rot = busca_modo == BUSCA_LINHA ? "Ir a la línea:" : busca_modo == BUSCA_PASTA ? "En la carpeta:" : "Buscar:";
    int x = r_busca.x + 8, y = r_busca.y + 3, lx = 15 * LETRA_L;
    d_texto(x, y + 3, rot, (int)tam_texto(rot), tema->barra_texto);
    caixa(x + lx, y, 240, q, campo == 0, trocar_q && campo == 0);
    int bx = x + lx + 248;
    if (busca_modo == BUSCA_LINHA) {
        char t[64];
        int n = formatar_texto(t, sizeof t, "1 - %d   (línea o línea:columna)", DOC->nli);
        d_texto(bx, y + 3, t, n, tema->margem_texto);
    } else {
        b_mai = botao(bx, y, "Aa", busca_mai);
        b_pal = botao(bx + b_mai.l + 4, y, "Palabra", busca_palavra);
        int tx = b_pal.x + b_pal.l + 10;
        char t[64];
        int n = 0;
        if (busca_modo == BUSCA_PASTA) n = formatar_texto(t, sizeof t, "Intro: buscar");
        else if (total == 0) n = formatar_texto(t, sizeof t, "Sin resultados");
        else if (total > 0) n = formatar_texto(t, sizeof t, "%d de %d", atual, total);
        d_texto(tx, y + 3, t, n, total == 0 && busca_modo != BUSCA_PASTA ? tema->erro : tema->margem_texto);
    }
    if (busca_modo == BUSCA_TROCAR) {
        y += LINHA_A;
        d_texto(x, y + 3, "Reemplazar:", 11, tema->barra_texto);
        caixa(x + lx, y, 240, troca, campo == 1, false);
        b_um = botao(bx, y, "Reemplazar", false);
        b_todos = botao(bx + b_um.l + 4, y, "Todos", false);
    }
    d_mostrar(r_busca);
}

/* ------------------------------------------------------------ eventos */

bool busca_tecla(unsigned t, unsigned mods)
{
    if (busca_modo == BUSCA_FECHADA) return false;
    char *c = campo ? troca : q;
    if (!campo && trocar_q && (t == '\b' || t == 22 || (t >= 0x20 && t < 0x100 && t != 0x7F && !(mods & (M_CTRL | M_ALT))))) {
        trocar_q = false;   /* como en un campo con todo el texto seleccionado */
        q[0] = 0;
        if (t == '\b') { sujo |= R_BUSCA; return true; }
    }
    if (t != TECLA_ESC && !(mods & M_ALT)) trocar_q = false;
    int n = (int)tam_texto(c);
    if (t == TECLA_ESC) { busca_fechar(); return true; }
    if ((mods & M_ALT) && (t == 'm' || t == 'M')) { busca_mai = !busca_mai; total = -1; incremental(); return true; }
    if ((mods & M_ALT) && (t == 'p' || t == 'P')) { busca_palavra = !busca_palavra; total = -1; incremental(); return true; }
    if (t == '\t' && busca_modo == BUSCA_TROCAR) { campo = !campo; sujo |= R_BUSCA; return true; }
    if (t == '\n') {
        if (busca_modo == BUSCA_LINHA) ir_linha();
        else if (busca_modo == BUSCA_PASTA) busca_na_pasta();
        else if (busca_modo == BUSCA_TROCAR && campo == 1) { if (mods & M_SHIFT) busca_trocar_todos(); else trocar_um(); }
        else busca_proximo((mods & M_SHIFT) ? -1 : 1);
        return true;
    }
    if (t == '\b') {
        if (n > 0) c[n - 1] = 0;
        if (!campo && busca_modo != BUSCA_LINHA && busca_modo != BUSCA_PASTA) incremental();
        sujo |= R_BUSCA;
        return true;
    }
    if (t == 22) {   /* Ctrl+V: la primera línea del portapapeles */
        char b[128];
        colar_texto(b, sizeof b);
        for (int i = 0; b[i] && b[i] != '\n' && n < 126; i++) c[n++] = b[i];
        c[n] = 0;
        if (!campo && busca_modo == BUSCA_BUSCAR) incremental();
        sujo |= R_BUSCA;
        return true;
    }
    if (t >= 0x20 && t < 0x100 && t != 0x7F && !(mods & (M_CTRL | M_ALT))) {
        if (busca_modo == BUSCA_LINHA && !((t >= '0' && t <= '9') || t == ':' || t == ',')) return true;
        if (n < 126) { c[n] = (char)t; c[n + 1] = 0; }
        if (!campo && (busca_modo == BUSCA_BUSCAR || busca_modo == BUSCA_TROCAR)) incremental();
        sujo |= R_BUSCA;
        return true;
    }
    return false;
}

void busca_clique(int x, int y)
{
    if (busca_modo == BUSCA_LINHA) return;
    if (dentro(x, y, b_mai)) { busca_mai = !busca_mai; total = -1; incremental(); }
    else if (dentro(x, y, b_pal)) { busca_palavra = !busca_palavra; total = -1; incremental(); }
    else if (busca_modo == BUSCA_TROCAR && dentro(x, y, b_um)) trocar_um();
    else if (busca_modo == BUSCA_TROCAR && dentro(x, y, b_todos)) busca_trocar_todos();
    else if (busca_modo == BUSCA_TROCAR) campo = y >= r_busca.y + LINHA_A;
    sujo |= R_BUSCA;
}
