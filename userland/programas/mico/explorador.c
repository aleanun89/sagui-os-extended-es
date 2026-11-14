// SPDX-License-Identifier: GPL-2.0-or-later
/* explorador.c - el árbol de la carpeta abierta, en el panel de la izquierda.
 *
 * El árbol es "aplanado": una lista de las líneas visibles (nombre, camino,
 * nivel), rehecha cada vez que se abre o cierra una carpeta o se crea,
 * renombra o borra algo. Las carpetas abiertas quedan en una lista de caminos,
 * para que rehacer el árbol las mantenga abiertas. Carpetas primero, después
 * los archivos, en orden alfabético sin distinguir mayúsculas; los nombres que
 * empiezan por "." quedan ocultos, salvo el ".mico" (la configuración).
 *
 * Menú contextual (D051): Abrir, Nuevo archivo, Nueva carpeta, Renombrar,
 * Borrar (carpetas enteras, después de confirmar), Actualizar y Abrir
 * terminal aquí. */
#include "mico.h"

#define MAX_NOS 3000
#define MAX_ABERTAS 128
#define ALT_LINHA 18
#define CABECALHO 24

typedef struct { char nome[NOME_MAX]; char caminho[CAMINHO_MAX]; short nivel; bool pasta; } no_t;

static no_t *nos;
static int qtd, sel = -1, topo;
static char abertas[MAX_ABERTAS][CAMINHO_MAX];
static int qtd_abertas;
static char marcado[CAMINHO_MAX];
static int alvo_menu = -1;

static char minuscula(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static int compara_nomes(const entrada_pasta_t *a, const entrada_pasta_t *b)
{
    bool pa = a->tipo == TIPO_PASTA, pb = b->tipo == TIPO_PASTA;
    if (pa != pb) return pa ? -1 : 1;
    for (int i = 0;; i++) {
        char x = minuscula(a->nome[i]), y = minuscula(b->nome[i]);
        if (x != y || !x) return (unsigned char)x - (unsigned char)y;
    }
}

static bool aberta(const char *c)
{
    for (int i = 0; i < qtd_abertas; i++) if (compara_texto(abertas[i], c) == 0) return true;
    return false;
}

static void alternar(const char *c)
{
    for (int i = 0; i < qtd_abertas; i++)
        if (compara_texto(abertas[i], c) == 0) {
            memmove(abertas[i], abertas[i + 1], (size_t)(qtd_abertas - i - 1) * CAMINHO_MAX);
            qtd_abertas--;
            return;
        }
    if (qtd_abertas < MAX_ABERTAS) copia_texto(abertas[qtd_abertas++], c, CAMINHO_MAX);
}

static void juntar_caminho(char *dst, const char *p, const char *nome)
{
    if (p[0] == '/' && !p[1]) formatar_texto(dst, CAMINHO_MAX, "/%s", nome);
    else formatar_texto(dst, CAMINHO_MAX, "%s/%s", p, nome);
}

static bool escondido(const char *nome) { return nome[0] == '.' && compara_texto(nome, ".mico") != 0; }

/* Las entradas de una carpeta, ya ordenadas. Devuelve cuántas (hasta max). */
static int ler_pasta(const char *p, entrada_pasta_t *v, int max)
{
    int n = 0;
    entrada_pasta_t e;
    for (int i = 0; n < max && listar(p, i, &e) == 1; i++) {
        if (escondido(e.nome)) continue;
        int k = n++;   /* inserción ordenada: las carpetas suelen tener pocas entradas */
        while (k > 0 && compara_nomes(&v[k - 1], &e) > 0) { v[k] = v[k - 1]; k--; }
        v[k] = e;
    }
    return n;
}

static void montar_arvore(const char *p, int nivel)
{
    if (nivel > 12) return;
    entrada_pasta_t *v = aloca(sizeof(entrada_pasta_t) * 512);
    if (!v) return;
    int n = ler_pasta(p, v, 512);
    for (int i = 0; i < n && qtd < MAX_NOS; i++) {
        no_t *no = &nos[qtd++];
        copia_texto(no->nome, v[i].nome, sizeof no->nome);
        juntar_caminho(no->caminho, p, v[i].nome);
        no->nivel = (short)nivel;
        no->pasta = v[i].tipo == TIPO_PASTA;
        if (no->pasta && aberta(no->caminho)) montar_arvore(no->caminho, nivel + 1);
    }
    libera(v);
}

void expl_atualizar(void)
{
    if (!nos) nos = aloca(sizeof(no_t) * MAX_NOS);
    if (!nos) return;
    char antes[CAMINHO_MAX] = "";
    if (sel >= 0 && sel < qtd) copia_texto(antes, nos[sel].caminho, sizeof antes);
    qtd = 0;
    if (pasta[0]) montar_arvore(pasta, 0);
    sel = -1;
    for (int i = 0; i < qtd; i++) if (compara_texto(nos[i].caminho, antes) == 0) sel = i;
    if (topo > qtd - 1) topo = qtd > 0 ? qtd - 1 : 0;
    sujo |= R_LATERAL;
}

void expl_abrir_pasta(const char *p)
{
    qtd_abertas = 0;
    topo = 0;
    sel = -1;
    expl_atualizar();
    registrar("mico: carpeta %s (%d entradas)", p, qtd);
}

void expl_marcar(const char *caminho)
{
    if (compara_texto(marcado, caminho) == 0) return;
    copia_texto(marcado, caminho, sizeof marcado);
    sujo |= R_LATERAL;
}

/* ------------------------------------------------------------ dibujo */

static int linhas_vis(void) { int v = (r_lateral.a - CABECALHO) / ALT_LINHA; return v < 1 ? 1 : v; }

static uint32_t cor_do_arquivo(const char *nome)
{
    switch (ling_do_nome(nome, "", 0)) {
    case LING_C: return 0x3B7DC4;
    case LING_PY: return 0xD9A400;
    case LING_SH: return 0x2E9E5B;
    case LING_MD: return 0x8B5CB8;
    case LING_C8: return 0xC4553B;
    default: return 0x9AA19A;
    }
}

void expl_desenhar(void)
{
    if (!ver_lateral) return;
    recorte = r_lateral;
    d_ret(r_lateral.x, r_lateral.y, r_lateral.l, r_lateral.a, tema->lateral);
    d_ret(r_lateral.x + r_lateral.l - 1, r_lateral.y, 1, r_lateral.a, tema->barra_borda);
    /* encabezado: el nombre de la carpeta */
    const char *nome = pasta[0] ? (acha_ultimo_char(pasta, '/') && acha_ultimo_char(pasta, '/')[1] ? acha_ultimo_char(pasta, '/') + 1 : pasta) : "Sin carpeta";
    d_ret(r_lateral.x, r_lateral.y, r_lateral.l - 1, CABECALHO, tema->barra);
    d_ret(r_lateral.x + 6, r_lateral.y + 8, 10, 8, tema->lateral_pasta);
    d_ret(r_lateral.x + 6, r_lateral.y + 6, 5, 2, tema->lateral_pasta);
    d_texto_ate(r_lateral.x + 22, r_lateral.y + 4, nome, r_lateral.l - 30, foco == FOCO_EXPLORADOR ? tema->destaque : tema->lateral_texto);
    if (!pasta[0]) {
        d_texto_ate(r_lateral.x + 8, r_lateral.y + CABECALHO + 8, "Archivo > Abrir", r_lateral.l - 12, tema->margem_texto);
        d_texto_ate(r_lateral.x + 8, r_lateral.y + CABECALHO + 26, "carpeta...", r_lateral.l - 12, tema->margem_texto);
    }
    int vis = linhas_vis();
    for (int k = 0; k <= vis && topo + k < qtd; k++) {
        no_t *no = &nos[topo + k];
        int y = r_lateral.y + CABECALHO + k * ALT_LINHA, x = r_lateral.x + 6 + no->nivel * 12;
        bool m = compara_texto(no->caminho, marcado) == 0;
        if (topo + k == sel) d_ret(r_lateral.x, y, r_lateral.l - 1, ALT_LINHA, tema->lateral_sel);
        if (m) d_ret(r_lateral.x, y, 3, ALT_LINHA, tema->destaque);
        if (no->pasta) {
            bool ab = aberta(no->caminho);
            for (int i = 0; i < 4; i++) {   /* triángulo: hacia la derecha (cerrada) o hacia abajo (abierta) */
                if (ab) d_ret(x + i, y + 7 + i, 8 - 2 * i, 1, tema->lateral_texto);
                else d_ret(x + 2 + i, y + 5 + i, 1, 8 - 2 * i, tema->lateral_texto);
            }
            d_ret(x + 11, y + 6, 11, 8, tema->lateral_pasta);
            d_ret(x + 11, y + 4, 5, 2, tema->lateral_pasta);
        } else {
            d_ret(x + 13, y + 3, 8, 12, cor_do_arquivo(no->nome));
            d_ret(x + 15, y + 5, 4, 1, tema->lateral);
            d_ret(x + 15, y + 8, 4, 1, tema->lateral);
        }
        d_texto_ate(x + 26, y + 1, no->nome, r_lateral.x + r_lateral.l - x - 30, m ? tema->destaque : tema->lateral_texto);
    }
    d_mostrar(r_lateral);
}

/* ------------------------------------------------------------ acciones */

static void pasta_do_alvo(int i, char *dst)
{
    if (i < 0 || i >= qtd) { copia_texto(dst, pasta, CAMINHO_MAX); return; }
    if (nos[i].pasta) { copia_texto(dst, nos[i].caminho, CAMINHO_MAX); return; }
    copia_texto(dst, nos[i].caminho, CAMINHO_MAX);
    char *b = acha_ultimo_char(dst, '/');
    if (b && b != dst) *b = 0; else copia_texto(dst, "/", CAMINHO_MAX);
}

static void ativar(int i)
{
    if (i < 0 || i >= qtd) return;
    sel = i;
    if (nos[i].pasta) { alternar(nos[i].caminho); expl_atualizar(); }
    else if (abrir_arquivo(nos[i].caminho, true) >= 0) foco = FOCO_EDITOR;   /* abrió: escribir es el próximo paso */
    sujo |= R_LATERAL;
}

static bool apagar_tudo(const char *c)
{
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) < 0) return false;
    if (inf.tipo != TIPO_PASTA) return apagar(c) >= 0;
    entrada_pasta_t e;
    char filho[CAMINHO_MAX];
    while (listar(c, 0, &e) == 1) {   /* siempre la primera: la lista se achica con cada borrado */
        juntar_caminho(filho, c, e.nome);
        if (!apagar_tudo(filho)) return false;
    }
    return remover_pasta(c) >= 0;
}

/* Pestañas abiertas en un camino que fue renombrado (el archivo o una carpeta encima de él). */
static void abas_renomeadas(const char *de, const char *para)
{
    int n = (int)tam_texto(de);
    for (int i = 0; i < qtd_abas; i++) {
        char *c = abas[i]->caminho;
        if (compara_texto_n(c, de, (size_t)n) == 0 && (c[n] == 0 || c[n] == '/')) {
            char novo[CAMINHO_MAX];
            formatar_texto(novo, sizeof novo, "%s%s", para, c + n);
            copia_texto(c, novo, CAMINHO_MAX);
        }
    }
    sujo |= R_ABAS;
    titulo_atualizar();
}

void expl_menu(int idx)
{
    int i = alvo_menu;
    char base[CAMINHO_MAX], c[CAMINHO_MAX], nome[NOME_MAX];
    pasta_do_alvo(i, base);
    switch (idx) {
    case 0: ativar(i); break;
    case 1: case 2:
        nome[0] = 0;
        if (!pedir_texto(idx == 1 ? "Nuevo archivo" : "Nueva carpeta", idx == 1 ? "Nombre del archivo:" : "Nombre de la carpeta:", nome, sizeof nome) || !nome[0]) break;
        juntar_caminho(c, base, nome);
        if (idx == 1) {
            int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR);
            if (fd < 0) { avisar("No se ha podido crear %s (%s)", c, nome_erro(fd)); break; }
            fechar(fd);
        } else {
            int r = criar_pasta(c);
            if (r < 0) { avisar("No se ha podido crear %s (%s)", c, nome_erro(r)); break; }
        }
        if (compara_texto(base, pasta) != 0 && !aberta(base)) alternar(base);
        expl_atualizar();
        registrar("mico: creado %s", c);
        if (idx == 1) abrir_arquivo(c, true);
        break;
    case 3:
        if (i < 0) break;
        copia_texto(nome, nos[i].nome, sizeof nome);
        if (!pedir_texto("Renombrar", "Nombre nuevo:", nome, sizeof nome) || !nome[0] || compara_texto(nome, nos[i].nome) == 0) break;
        pasta_do_alvo(i, base);
        if (nos[i].pasta) {   /* la base de una carpeta es ella misma: sube un nivel */
            char *b = acha_ultimo_char(base, '/');
            if (b && b != base) *b = 0; else copia_texto(base, "/", sizeof base);
        }
        juntar_caminho(c, base, nome);
        {
            char de[CAMINHO_MAX];
            copia_texto(de, nos[i].caminho, sizeof de);
            int r = renomear(de, c);
            if (r < 0) { avisar("No se ha podido renombrar (%s)", nome_erro(r)); break; }
            registrar("mico: renombrado %s -> %s", de, c);
            abas_renomeadas(de, c);
        }
        expl_atualizar();
        break;
    case 4: {
        if (i < 0) break;
        char p[CAMINHO_MAX + 64];
        formatar_texto(p, sizeof p, nos[i].pasta ? "¿Borrar la carpeta «%s» y todo lo que contiene?" : "¿Borrar «%s»?", nos[i].nome);
        if (!confirmar("Borrar", p)) break;
        copia_texto(c, nos[i].caminho, sizeof c);
        if (!apagar_tudo(c)) avisar("No se ha podido borrar %s", c);
        else { registrar("mico: borrado %s", c); avisar("Borrado: %s", c); }
        expl_atualizar();
        break;
    }
    case 6: expl_atualizar(); break;
    case 7: painel_cd(base); break;
    }
}

void expl_clique(int x, int y, int botoes, int cliques)
{
    (void)x; (void)cliques;
    foco = FOCO_EXPLORADOR;
    int i = y < r_lateral.y + CABECALHO ? -1 : topo + (y - r_lateral.y - CABECALHO) / ALT_LINHA;
    if (i >= qtd) i = -1;
    if (botoes & 2) {
        alvo_menu = i;
        if (i >= 0) sel = i;
        sujo |= R_LATERAL;
        bool tem = i >= 0;
        char itens[220];
        formatar_texto(itens, sizeof itens, "%sAbrir\nNuevo archivo...\nNueva carpeta...\n%sRenombrar...\tF2\n%sBorrar\tSupr\n-\nActualizar\nAbrir terminal aquí",
                       tem ? "" : "~", tem ? "" : "~", tem ? "" : "~");
        menu_contextual(j->id, x, y, itens, 2);
        return;
    }
    if (!(botoes & 1)) return;
    if (i >= 0) ativar(i);
    sujo |= R_LATERAL;
}

void expl_rodar(int n)
{
    int antes = topo, vis = linhas_vis();
    topo += n * RODA_LINHAS;
    if (topo > qtd - vis) topo = qtd - vis;
    if (topo < 0) topo = 0;
    if (topo != antes) sujo |= R_LATERAL;
}

bool expl_tecla(unsigned t)
{
    int vis = linhas_vis();
    if (t == TECLA_CIMA) { if (sel > 0) sel--; else sel = 0; }
    else if (t == TECLA_BAIXO) { if (sel < qtd - 1) sel++; }
    else if (t == '\n') ativar(sel);
    else if ((t == TECLA_DIREITA || t == TECLA_ESQUERDA) && sel >= 0 && nos[sel].pasta) {
        if (aberta(nos[sel].caminho) != (t == TECLA_DIREITA)) { alternar(nos[sel].caminho); expl_atualizar(); }
    }
    else if (t == TECLA_DELETE && sel >= 0) { alvo_menu = sel; expl_menu(4); }
    else if (t == T_F1 + 1 && sel >= 0) { alvo_menu = sel; expl_menu(3); }
    else return false;
    if (sel >= 0 && sel < topo) topo = sel;
    if (sel >= topo + vis) topo = sel - vis + 1;
    sujo |= R_LATERAL;
    return true;
}

/* Todos los archivos de la carpeta, a cualquier profundidad (Ctrl+P y buscar en la carpeta). */
static void coletar(const char *p, char (*lista)[CAMINHO_MAX], int max, int *n, int nivel)
{
    if (nivel > 8 || *n >= max) return;
    entrada_pasta_t *v = aloca(sizeof(entrada_pasta_t) * 512);
    if (!v) return;
    int k = ler_pasta(p, v, 512);
    for (int i = 0; i < k && *n < max; i++) {
        char c[CAMINHO_MAX];
        juntar_caminho(c, p, v[i].nome);
        if (v[i].tipo == TIPO_PASTA) coletar(c, lista, max, n, nivel + 1);
        else copia_texto(lista[(*n)++], c, CAMINHO_MAX);
    }
    libera(v);
}

int expl_arquivos(char (*lista)[CAMINHO_MAX], int max)
{
    int n = 0;
    if (pasta[0]) coletar(pasta, lista, max, &n, 0);
    return n;
}
