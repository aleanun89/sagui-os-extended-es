// SPDX-License-Identifier: GPL-2.0-or-later
/* mico [carpeta | archivo...] | mico -t archivo - el Mico, editor de código
 * de SaguiOS (mejora 037, D075).
 *
 * La ventana tiene, de arriba hacia abajo: la barra de menús (Archivo, Editar,
 * Buscar, Ver, Ejecutar, Ayuda; cada botón abre un menú contextual con
 * submenús, D051/D064), las pestañas, la barra de búsqueda (cuando está abierta), el editor
 * y el panel de abajo (Terminal, Problemas, Búsqueda); a la izquierda, el
 * explorador de la carpeta abierta; abajo, la barra de estado.
 *
 * Bucle de eventos: el Mico no puede dormir esperando un evento de la ventana,
 * porque la terminal del panel tiene que leerse aunque nadie toque el
 * mouse. Cada vuelta atiende los eventos que llegaron, lee la terminal, corre los
 * relojes (copia de recuperación cada 5 s, verificación del archivo en el
 * disco cada 3 s) y solo después redibuja lo que quedó sucio. Sin nada para
 * hacer, duerme 10 ms.
 *
 * Todas las acciones tienen un número (A_* en mico.h) y un nombre: los menús, los
 * atajos y la paleta (Ctrl+Mayús+P) pasan por el mismo executar_acao().
 *
 * Modo -t: imprime la clase de cada letra de un archivo (una línea de
 * letras por línea del archivo: k palabra clave, t tipo, s cadena, c
 * comentario, n número, p preprocesador, f función, h título, e énfasis,
 * r registro, l etiqueta, v variable, "." el resto). Es lo que usa el test para
 * verificar el realce sin depender de píxeles. */
#include "mico.h"

#define LARG_INICIO 960
#define ALT_INICIO  640
#define MENU_A   24
#define ABAS_A   26
#define ESTADO_A 20
#define RECUP_MS 5000
#define DISCO_MS 3000
#define AVISO_MS 6000

doc_t *abas[MAX_ABAS];
int    qtd_abas, aba_atual;
char   pasta[CAMINHO_MAX];
int    tab_largura = 4;
bool   tab_espacos = true;
bool   ver_lateral = true, ver_painel = true;
int    foco = FOCO_EDITOR;
ret_t  r_menu, r_abas, r_lateral, r_busca, r_editor, r_painel, r_estado;
char   aviso[160];
unsigned sujo = R_TUDO;
static unsigned aviso_ate;
static bool tema_escuro_ligado;
static int arrastando_em;     /* región que recibió el EV_CLIQUE (los EV_MOVER van para ella) */
static int menu_aberto = -1;
static bool recuperando;      /* abriendo copias de recuperación: no pregunta de nuevo */


/* ------------------------------------------------------------ acciones */

const acao_t acoes[QTD_ACOES] = {
    [A_NOVO] = { "Nuevo archivo", "Ctrl+N" },              [A_ABRIR] = { "Abrir archivo...", "Ctrl+O" },
    [A_ABRIR_PASTA] = { "Abrir carpeta...", NULL },         [A_ABRIR_NOME] = { "Abrir por nombre...", "Ctrl+P" },
    [A_GUARDAR] = { "Guardar", "Ctrl+S" },                  [A_GUARDAR_COMO] = { "Guardar como...", "Ctrl+Mayús+S" },
    [A_GUARDAR_TODO] = { "Guardar todo", NULL },            [A_CERRAR_ABA] = { "Cerrar pestaña", "Ctrl+W" },
    [A_SALIR] = { "Salir", "Ctrl+Q" },
    [A_DESHACER] = { "Deshacer", "Ctrl+Z" },                [A_REHACER] = { "Rehacer", "Ctrl+Y" },
    [A_CORTAR] = { "Cortar", "Ctrl+X" },                    [A_COPIAR] = { "Copiar", "Ctrl+C" },
    [A_PEGAR] = { "Pegar", "Ctrl+V" },                      [A_SEL_TODO] = { "Seleccionar todo", "Ctrl+A" },
    [A_COMENTAR] = { "Comentar o descomentar", "Ctrl+/" },  [A_DUPLICAR] = { "Duplicar línea", "Ctrl+D" },
    [A_SUBIR_LINEA] = { "Subir línea", "Alt+Arriba" },      [A_BAJAR_LINEA] = { "Bajar línea", "Alt+Abajo" },
    [A_COMPLETAR] = { "Completar palabra", "Ctrl+Espacio" },
    [A_TAB_ESPACIOS] = { "Sangrar con espacios", NULL },    [A_TAB_2] = { "Tabulación de 2", NULL },
    [A_TAB_4] = { "Tabulación de 4", NULL },                [A_TAB_8] = { "Tabulación de 8", NULL },
    [A_BUSCAR] = { "Buscar...", "Ctrl+F" },                 [A_REEMPLAZAR] = { "Reemplazar...", "Ctrl+H" },
    [A_BUSCAR_SIG] = { "Buscar siguiente", "F3" },          [A_BUSCAR_ANT] = { "Buscar anterior", "Mayús+F3" },
    [A_BUSCAR_PASTA] = { "Buscar en la carpeta...", "Ctrl+Mayús+F" },
    [A_IR_LINEA] = { "Ir a la línea...", "Ctrl+G" },
    [A_PALETA] = { "Paleta de órdenes...", "Ctrl+Mayús+P" },
    [A_VER_LATERAL] = { "Explorador", "Ctrl+B" },           [A_VER_PAINEL] = { "Panel inferior", "Ctrl+J" },
    [A_VER_TERMINAL] = { "Terminal", "F6" },                [A_VER_PROBLEMAS] = { "Problemas", NULL },
    [A_VER_BUSQUEDA] = { "Resultados de búsqueda", NULL },
    [A_TEMA_CLARO] = { "Tema claro", NULL },                [A_TEMA_OSCURO] = { "Tema oscuro", NULL },
    [A_PANTALLA] = { "Pantalla completa", "F11" },
    [A_EJECUTAR] = { "Ejecutar", "F5" },                    [A_DETENER] = { "Detener", "Mayús+F5" },
    [A_LIMPIAR_TERM] = { "Limpiar terminal", NULL },
    [A_SIG_ABA] = { "Pestaña siguiente", "Ctrl+Tab" },      [A_ANT_ABA] = { "Pestaña anterior", "Ctrl+Mayús+Tab" },
    [A_AYUDA] = { "Ayuda de Mico", "F1" },                  [A_ACERCA] = { "Acerca de Mico", NULL },
    [A_VER_ASISTENTE] = { "Asistente IA", NULL },
};

bool acao_marcada(int a)
{
    switch (a) {
    case A_TAB_ESPACIOS: return tab_espacos;
    case A_TAB_2: return tab_largura == 2;
    case A_TAB_4: return tab_largura == 4;
    case A_TAB_8: return tab_largura == 8;
    case A_VER_LATERAL: return ver_lateral;
    case A_VER_PAINEL: return ver_painel;
    case A_TEMA_CLARO: return !tema_escuro_ligado;
    case A_TEMA_OSCURO: return tema_escuro_ligado;
    }
    return false;
}

bool acao_ativa(int a)
{
    switch (a) {
    case A_DESHACER: return DOC->feitos > 0;
    case A_REHACER: return DOC->feitos < DOC->nops;
    case A_ABRIR_NOME: case A_BUSCAR_PASTA: return pasta[0] != 0;
    }
    return true;   /* Cortar y Copiar sin selección valen para la línea entera */
}

/* ------------------------------------------------------------ utilidades */

void avisar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(aviso, sizeof aviso, fmt, ap);
    va_end(ap);
    aviso_ate = tempo_ms() + AVISO_MS;
    sujo |= R_ESTADO;
}

void caminho_absoluto(const char *rel, char *dst)
{
    if (rel[0] == '/') { copia_texto(dst, rel, CAMINHO_MAX); return; }
    const char *b = pasta[0] ? pasta : "/";
    while (rel[0] == '.' && rel[1] == '/') rel += 2;
    if (b[0] == '/' && !b[1]) formatar_texto(dst, CAMINHO_MAX, "/%s", rel);
    else formatar_texto(dst, CAMINHO_MAX, "%s/%s", b, rel);
}

static const char *nome_pasta(void)
{
    const char *b = acha_ultimo_char(pasta, '/');
    return b && b[1] ? b + 1 : pasta;
}

void titulo_atualizar(void)
{
    static char ultimo[200];
    char t[200];
    doc_t *d = DOC;
    if (pasta[0]) formatar_texto(t, sizeof t, "Mico - %s%s - %s", doc_nome(d), doc_modificado(d) ? " *" : "", nome_pasta());
    else formatar_texto(t, sizeof t, "Mico - %s%s", doc_nome(d), doc_modificado(d) ? " *" : "");
    if (compara_texto(t, ultimo) == 0) return;
    copia_texto(ultimo, t, sizeof ultimo);
    titulo_janela(j->id, t);
    registrar("mico: titulo %s", t);
}

/* ------------------------------------------------------------ configuración (.mico) */

#define MAX_CMD 12
static struct { char ext[16]; char cmd[200]; } cmds[MAX_CMD];
static int qtd_cmds;

static void aplicar_tema(bool escuro)
{
    tema_escuro_ligado = escuro;
    tema = escuro ? &tema_escuro : &tema_claro;
    sujo |= R_TUDO;
    vista_sujar_tudo();
}

static char *aparar(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    int n = (int)tam_texto(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
    return s;
}

/* Lee <carpeta>/.mico: "clave = valor" por línea, "#" comenta. Claves: tab,
 * espacios (sí/no), tema (claro/oscuro), ejecutar (para todos) y
 * ejecutar.EXT (solo para esa extensión). */
static void ler_config(void)
{
    qtd_cmds = 0;
    if (!pasta[0]) return;
    char c[CAMINHO_MAX];
    caminho_absoluto(".mico", c);
    int n, erro;
    bool u;
    char *s = ler_arquivo(c, &n, &erro, &u);
    if (!s) return;
    int lidas = 0;
    for (char *l = s; l && *l;) {
        char *f = acha_char(l, '\n');
        if (f) *f = 0;
        char *p = aparar(l), *ig = acha_char(p, '=');
        if (*p && *p != '#' && ig) {
            *ig = 0;
            char *k = aparar(p), *v = ig + 1;
            for (char *h = v; *h; h++)   /* comentario al final de la línea: " # ..." */
                if (*h == '#' && h > v && (h[-1] == ' ' || h[-1] == '\t')) { *h = 0; break; }
            v = aparar(v);
            lidas++;
            if (compara_texto(k, "tab") == 0) { int t = texto_para_int(v); if (t >= 1 && t <= 16) tab_largura = t; }
            else if (compara_texto(k, "espacios") == 0) tab_espacos = !(v[0] == 'n' || v[0] == 'N' || v[0] == '0');
            else if (compara_texto(k, "tema") == 0) aplicar_tema(v[0] == 'o' || v[0] == 'O' || v[0] == 'e');
            else if (comeca_com(k, "ejecutar") && qtd_cmds < MAX_CMD) {
                const char *e = k[8] == '.' ? k + 9 : "";
                copia_texto(cmds[qtd_cmds].ext, e[0] == '.' ? e + 1 : e, sizeof cmds[0].ext);
                copia_texto(cmds[qtd_cmds].cmd, v, sizeof cmds[0].cmd);
                qtd_cmds++;
            } else lidas--;
        }
        l = f ? f + 1 : NULL;
    }
    libera(s);
    registrar("mico: .mico leido (%d opciones, %d ordenes de ejecutar)", lidas, qtd_cmds);
}

/* ------------------------------------------------------------ diseño */

void layout(void)
{
    int L = j->larg, A = j->alt;
    r_menu = (ret_t){ 0, 0, L, MENU_A };
    r_estado = (ret_t){ 0, A - ESTADO_A, L, ESTADO_A };
    int lat = ver_lateral ? (L / 4 < 220 ? L / 4 : 220) : 0;
    if (lat && lat < 140) lat = 140;
    r_lateral = (ret_t){ 0, MENU_A, lat, A - MENU_A - ESTADO_A };
    int meio = A - MENU_A - ESTADO_A, pa = 0;
    if (ver_painel) { pa = meio * 32 / 100; if (pa < 120) pa = 120; if (pa > 260) pa = 260; if (pa > meio - 120) pa = meio - 120; if (pa < 60) pa = 60; }
    r_painel = (ret_t){ lat, A - ESTADO_A - pa, L - lat, pa };
    r_abas = (ret_t){ lat, MENU_A, L - lat, ABAS_A };
    int hb = busca_altura();
    r_busca = (ret_t){ lat, MENU_A + ABAS_A, L - lat, hb };
    r_editor = (ret_t){ lat, MENU_A + ABAS_A + hb, L - lat, A - ESTADO_A - pa - MENU_A - ABAS_A - hb };
    if (ver_painel) painel_tamanho();
    registrar("mico: layout %dx%d, lateral %d, editor %d,%d %dx%d, panel %d,%d %dx%d", L, A, lat,
              r_editor.x, r_editor.y, r_editor.l, r_editor.a, r_painel.x, r_painel.y, r_painel.l, r_painel.a);
    vista_cursor_visivel();
    vista_sujar_tudo();
    sujo |= R_TUDO;
}

/* ------------------------------------------------------------ menús de la barra */

#define SEP  (-1)
#define FIM  (-2)
#define SUB_FIM (-3)
#define SUB(k) (-10 - (k))
static const char *const nome_sub[] = { "Sangría", "Tema" };
static const short menu_archivo[] = { A_NOVO, A_ABRIR, A_ABRIR_PASTA, A_ABRIR_NOME, SEP, A_GUARDAR, A_GUARDAR_COMO, A_GUARDAR_TODO,
                                      SEP, A_CERRAR_ABA, SEP, A_SALIR, FIM };
static const short menu_editar[] = { A_DESHACER, A_REHACER, SEP, A_CORTAR, A_COPIAR, A_PEGAR, A_SEL_TODO, SEP, A_COMENTAR,
                                     A_DUPLICAR, A_SUBIR_LINEA, A_BAJAR_LINEA, A_COMPLETAR, SEP,
                                     SUB(0), A_TAB_ESPACIOS, SEP, A_TAB_2, A_TAB_4, A_TAB_8, SUB_FIM, FIM };
static const short menu_buscar[] = { A_BUSCAR, A_REEMPLAZAR, A_BUSCAR_SIG, A_BUSCAR_ANT, SEP, A_BUSCAR_PASTA, SEP, A_IR_LINEA, FIM };
static const short menu_ver[] = { A_PALETA, SEP, A_VER_LATERAL, A_VER_PAINEL, SEP, A_VER_TERMINAL, A_VER_PROBLEMAS, A_VER_BUSQUEDA, A_VER_ASISTENTE,
                                  SEP, SUB(1), A_TEMA_CLARO, A_TEMA_OSCURO, SUB_FIM, A_PANTALLA, SEP, A_SIG_ABA, A_ANT_ABA, FIM };
static const short menu_ejecutar[] = { A_EJECUTAR, A_DETENER, SEP, A_LIMPIAR_TERM, FIM };
static const short menu_ayuda[] = { A_AYUDA, A_ACERCA, FIM };
static const char *const nome_menu[] = { "Archivo", "Editar", "Buscar", "Ver", "Ejecutar", "Ayuda" };
static const short *const def_menu[] = { menu_archivo, menu_editar, menu_buscar, menu_ver, menu_ejecutar, menu_ayuda };
#define QTD_MENUS 6
static short linha_acao[MENU_MAX_ITENS];

static int menu_x(int m) { int x = 6; for (int i = 0; i < m; i++) x += (int)tam_texto(nome_menu[i]) * LETRA_L + 16; return x; }

static void desenhar_menu(void)
{
    recorte = r_menu;
    d_ret(0, 0, r_menu.l, MENU_A, tema->barra);
    d_ret(0, MENU_A - 1, r_menu.l, 1, tema->barra_borda);
    for (int m = 0; m < QTD_MENUS; m++) {
        int x = menu_x(m), w = (int)tam_texto(nome_menu[m]) * LETRA_L + 16;
        if (m == menu_aberto) d_ret(x, 2, w, MENU_A - 4, tema->lateral_sel);
        d_texto(x + 8, 4, nome_menu[m], (int)tam_texto(nome_menu[m]), tema->barra_texto);
    }
    /* A la derecha, el "logo" del Mico: una M en un cuadradito verde. */
    int x = r_menu.l - 54;
    d_ret(x, 4, 16, 16, tema->destaque);
    d_letra(x + 4, 4, 'M', 0xFFFFFF);
    d_texto(x + 20, 4, "Mico", 4, tema->margem_texto);
    d_mostrar(r_menu);
}

static void abrir_menu(int m)
{
    char itens[1600];
    int n = 0, linhas = 0;
    for (const short *p = def_menu[m]; *p != FIM && linhas < MENU_MAX_ITENS; p++) {
        if (*p == SEP) n += formatar_texto(itens + n, sizeof itens - (size_t)n, "-");
        else if (*p == SUB_FIM) n += formatar_texto(itens + n, sizeof itens - (size_t)n, "<");
        else if (*p <= -10) n += formatar_texto(itens + n, sizeof itens - (size_t)n, ">%s", nome_sub[-10 - *p]);
        else {
            const acao_t *a = &acoes[*p];
            n += formatar_texto(itens + n, sizeof itens - (size_t)n, "%s%s%s%s%s", acao_ativa(*p) ? "" : "~",
                                acao_marcada(*p) ? "*" : "", a->nome, a->atalho ? "\t" : "", a->atalho ? a->atalho : "");
        }
        linha_acao[linhas++] = *p;
        itens[n++] = '\n';
    }
    if (n) itens[n - 1] = 0;
    menu_aberto = m;
    sujo |= R_MENU;
    menu_contextual(j->id, menu_x(m), MENU_A - 1, itens, 10 + m);
    registrar("mico: menu %s abierto (%d lineas)", nome_menu[m], linhas);
}

/* ------------------------------------------------------------ pestañas */

static int aba_larg(int i)
{
    int w = (int)tam_texto(doc_nome(abas[i])) * LETRA_L + 48;
    return w > 240 ? 240 : w;
}
static int abas_desl;   /* desplazamiento de la franja de pestañas cuando no caben */

static void desenhar_abas(void)
{
    recorte = r_abas;
    d_ret(r_abas.x, r_abas.y, r_abas.l, r_abas.a, tema->aba);
    d_ret(r_abas.x, r_abas.y + r_abas.a - 1, r_abas.l, 1, tema->barra_borda);
    int x0 = 0, xa = 0;
    for (int i = 0; i < qtd_abas; i++) { if (i == aba_atual) xa = x0; x0 += aba_larg(i); }
    if (xa - abas_desl < 0) abas_desl = xa;
    if (xa + aba_larg(aba_atual) - abas_desl > r_abas.l) abas_desl = xa + aba_larg(aba_atual) - r_abas.l;
    int x = r_abas.x - abas_desl;
    for (int i = 0; i < qtd_abas; i++) {
        int w = aba_larg(i);
        bool at = i == aba_atual;
        d_ret(x, r_abas.y + 2, w - 2, r_abas.a - 2, at ? tema->aba_ativa : tema->aba);
        if (at) d_ret(x, r_abas.y + 2, w - 2, 2, tema->aba_marca);
        d_ret(x + w - 2, r_abas.y + 6, 1, r_abas.a - 12, tema->barra_borda);
        d_ret(x + 8, r_abas.y + 10, 6, 6, tema->cl[abas[i]->ling == LING_TEXTO ? CL_COMENTARIO : CL_CHAVE]);
        d_texto_ate(x + 20, r_abas.y + 6, doc_nome(abas[i]), w - 46, at ? tema->aba_texto : tema->margem_texto);
        if (doc_modificado(abas[i])) d_ret(x + w - 18, r_abas.y + 11, 6, 6, tema->aba_marca);
        else d_letra(x + w - 19, r_abas.y + 5, 'x', tema->margem_texto);
        x += w;
    }
    d_mostrar(r_abas);
}

static void trocar_aba(int i)
{
    if (i < 0 || i >= qtd_abas) return;
    aba_atual = i;
    expl_marcar(DOC->caminho);
    vista_doc_trocou();
    vista_cursor_visivel();
    titulo_atualizar();
    sujo |= R_ABAS | R_ESTADO | R_BUSCA;
}

static int nova_aba(doc_t *d)
{
    if (qtd_abas >= MAX_ABAS) { avisar("Demasiadas pestañas abiertas"); doc_liberar(d); return -1; }
    abas[qtd_abas] = d;
    return qtd_abas++;
}

static bool salvar_como(doc_t *d);

static bool salvar(doc_t *d)
{
    if (!d->caminho[0]) return salvar_como(d);
    unsigned t0 = tempo_ms();
    int r = doc_salvar(d);
    if (r < 0) {
        char m[CAMINHO_MAX + 60];
        formatar_texto(m, sizeof m, "No se ha podido guardar\n%s\n(%s)", d->caminho, nome_erro(r));
        mensagem("Mico", m);
        return false;
    }
    recup_apagar(d);
    registrar("mico: guardado %s (%d bytes, %d lineas, %u ms)", d->caminho, d->disco_tam, d->nli, tempo_ms() - t0);
    avisar("Guardado: %s", d->caminho);
    const char *n = doc_nome(d);
    if (compara_texto(n, ".mico") == 0) ler_config();
    sujo |= R_ABAS | R_ESTADO;
    titulo_atualizar();
    return true;
}

static bool salvar_como(doc_t *d)
{
    char c[CAMINHO_MAX];
    if (d->caminho[0]) copia_texto(c, d->caminho, sizeof c);
    else formatar_texto(c, sizeof c, "%s/", pasta[0] && !(pasta[0] == '/' && !pasta[1]) ? pasta : "");
    if (!pedir_texto("Guardar como", "Guardar en (ej.: /tmp/programa.c):", c, sizeof c) || !c[0]) return false;
    recup_apagar(d);   /* la copia de recuperación tiene el nombre antiguo */
    caminho_absoluto(c, d->caminho);
    int ling = ling_do_nome(d->caminho, "", 0);
    if (ling != d->ling) { d->ling = ling; d->est_ok = 0; vista_sujar_tudo(); }
    bool ok = salvar(d);
    if (ok) { expl_atualizar(); expl_marcar(d->caminho); }
    return ok;
}

/* Pregunta antes de tirar los cambios de d. false = desistir. */
static bool pode_fechar(doc_t *d)
{
    if (!doc_modificado(d)) return true;
    char m[CAMINHO_MAX + 80];
    formatar_texto(m, sizeof m, "«%s» tiene cambios sin guardar.\n¿Guardarlos antes de cerrar?", doc_nome(d));
    if (confirmar("Mico", m)) return salvar(d);
    return true;
}

static void fechar_aba(int i)
{
    doc_t *d = abas[i];
    if (!pode_fechar(d)) return;
    recup_apagar(d);
    registrar("mico: pestana cerrada %s", doc_nome(d));
    doc_liberar(d);
    memmove(abas + i, abas + i + 1, (size_t)(qtd_abas - i - 1) * sizeof(doc_t *));
    qtd_abas--;
    if (!qtd_abas) nova_aba(doc_novo());
    if (aba_atual >= qtd_abas) aba_atual = qtd_abas - 1;
    else if (aba_atual > i) aba_atual--;
    trocar_aba(aba_atual);
}

/* Aplica un texto recuperado como una edición (deshacible): el archivo del
 * disco sigue siendo el "guardado". */
static void aplicar_recuperado(doc_t *d, const char *t, int n)
{
    doc_grupo(d, true);
    doc_apagar(d, 0, d->tam);
    doc_inserir(d, 0, t, n);
    doc_grupo(d, false);
    d->cursor = 0;
    d->ancora = -1;
    d->est_ok = 0;
}

static bool achar_recup(const char *caminho, char *arquivo)
{
    formatar_texto(arquivo, 80, "%s/%x.rec", pasta_recup, hash_texto(caminho, (int)tam_texto(caminho)));
    info_arquivo_t inf;
    return info_arquivo(arquivo, &inf) >= 0;
}

int abrir_arquivo(const char *caminho, bool focar)
{
    char c[CAMINHO_MAX];
    caminho_absoluto(caminho, c);
    if (focar) foco = FOCO_EDITOR;   /* abrir para ver: el teclado va al texto */
    for (int i = 0; i < qtd_abas; i++)
        if (compara_texto(abas[i]->caminho, c) == 0) { if (focar) trocar_aba(i); return i; }
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) >= 0 && inf.tipo == TIPO_PASTA) { avisar("%s es una carpeta", c); return -1; }
    unsigned t0 = tempo_ms();
    doc_t *d = doc_novo();
    int erro = 0;
    if (!doc_carregar(d, c, &erro)) {
        if (erro != ERRO_NAO_EXISTE) {
            char m[CAMINHO_MAX + 60];
            formatar_texto(m, sizeof m, "No se ha podido abrir\n%s\n(%s)", c, nome_erro(erro));
            mensagem("Mico", m);
            doc_liberar(d);
            return -1;
        }
        copia_texto(d->caminho, c, sizeof d->caminho);   /* archivo nuevo: nace al guardar */
        d->ling = ling_do_nome(c, "", 0);
        avisar("Archivo nuevo: %s", c);
    }
    registrar("mico: abierto %s (%d lineas, %d bytes, %s, %u ms)", c, d->nli, d->tam, nome_ling[d->ling], tempo_ms() - t0);
    /* ¿Copia de recuperación de una sesión que terminó mal? */
    char rec[80];
    if (achar_recup(c, rec)) {
        char m[CAMINHO_MAX + 120];
        formatar_texto(m, sizeof m, "Hay cambios sin guardar de «%s»\nde una sesión anterior de Mico.\n¿Recuperarlos?", doc_nome(d));
        char cam[CAMINHO_MAX];
        int n;
        char *t = recup_ler(rec, cam, &n);
        if (t && (recuperando || confirmar("Mico - recuperación", m))) {
            aplicar_recuperado(d, t, n);
            registrar("mico: recuperado %s (%d bytes)", c, n);
            avisar("Recuperados los cambios sin guardar de %s", doc_nome(d));
        } else apagar(rec);
        if (t) libera(t);
    }
    /* La única pestaña es un "sin nombre" vacío e intacto: el archivo ocupa su lugar. */
    if (qtd_abas == 1 && !abas[0]->caminho[0] && abas[0]->tam == 0 && !doc_modificado(abas[0])) {
        doc_liberar(abas[0]);
        abas[0] = d;
        if (focar || aba_atual == 0) trocar_aba(0);
        return 0;
    }
    int i = nova_aba(d);
    if (i >= 0 && focar) trocar_aba(i);
    sujo |= R_ABAS;
    return i;
}

void ir_para(const char *caminho, int linha, int coluna)
{
    int i = abrir_arquivo(caminho, true);
    if (i < 0) return;
    doc_t *d = DOC;
    if (linha < 1) linha = 1;
    if (linha > d->nli) linha = d->nli;
    int p = doc_ini(d, linha - 1) + (coluna > 1 ? coluna - 1 : 0);
    if (p > doc_fim(d, linha - 1)) p = doc_fim(d, linha - 1);
    vista_ir(p, false);
    int vis = vista_linhas_visiveis();
    if (linha - 1 < d->topo || linha - 1 >= d->topo + vis) {
        d->topo = linha - 1 - vis / 3;
        if (d->topo < 0) d->topo = 0;
    }
    vista_sujar_tudo();
    foco = FOCO_EDITOR;
    sujo |= R_PAINEL;
    registrar("mico: cursor en %s:%d", doc_nome(d), linha);
}

static void abrir_pasta(const char *p)
{
    char c[CAMINHO_MAX];
    if (p[0] == '/') copia_texto(c, p, sizeof c);
    else { char a[CAMINHO_MAX]; pasta_atual(a, sizeof a); formatar_texto(c, sizeof c, "%s/%s", a[1] ? a : "", p); }
    int n = (int)tam_texto(c);
    while (n > 1 && c[n - 1] == '/') c[--n] = 0;
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) < 0 || inf.tipo != TIPO_PASTA) { avisar("No es una carpeta: %s", c); return; }
    copia_texto(pasta, c, sizeof pasta);
    tab_largura = 4; tab_espacos = true;
    ler_config();
    expl_abrir_pasta(pasta);
    titulo_atualizar();
    if (!ver_lateral) { ver_lateral = true; layout(); }
}

/* ------------------------------------------------------------ F5 */

static void montar_comando(const char *modelo, doc_t *d, char *out, int max)
{
    char base[NOME_MAX], dir[CAMINHO_MAX];
    copia_texto(base, doc_nome(d), sizeof base);
    char *pt = acha_ultimo_char(base, '.');
    if (pt && pt != base) *pt = 0;
    copia_texto(dir, d->caminho, sizeof dir);
    char *b = acha_ultimo_char(dir, '/');
    if (b && b != dir) *b = 0; else copia_texto(dir, "/", sizeof dir);
    int n = 0;
    for (const char *p = modelo; *p && n < max - 1; p++) {
        const char *v = NULL;
        if (*p == '{') {
            if (comeca_com(p, "{archivo}")) { v = d->caminho; p += 8; }
            else if (comeca_com(p, "{nombre}")) { v = doc_nome(d); p += 7; }
            else if (comeca_com(p, "{base}")) { v = base; p += 5; }
            else if (comeca_com(p, "{carpeta}")) { v = dir; p += 8; }
        }
        if (v) while (*v && n < max - 1) out[n++] = *v++;
        else out[n++] = *p;
    }
    out[n] = 0;
}

static void executar_atual(void)
{
    doc_t *d = DOC;
    if (!d->caminho[0] && !salvar_como(d)) return;
    if (doc_modificado(d) && !salvar(d)) return;
    const char *ext = acha_ultimo_char(doc_nome(d), '.');
    ext = ext ? ext + 1 : "";
    const char *modelo = NULL;
    for (int i = 0; i < qtd_cmds && !modelo; i++) if (compara_texto(cmds[i].ext, ext) == 0) modelo = cmds[i].cmd;
    for (int i = 0; i < qtd_cmds && !modelo; i++) if (!cmds[i].ext[0]) modelo = cmds[i].cmd;
    if (!modelo) {
        switch (d->ling) {
        case LING_C:  modelo = "cc {archivo} -o /tmp/{base} && /tmp/{base}"; break;
        case LING_PY: modelo = "python {archivo}"; break;
        case LING_SH: modelo = "sh {archivo}"; break;
        case LING_C8: modelo = "chip8asm {archivo} -o /tmp/{base}.ch8 && chip8 /tmp/{base}.ch8"; break;
        case LING_MD: modelo = "markdown {archivo}"; break;
        }
    }
    /* Pascal (mejora 040): sin resaltado propio, se reconoce por la extensión */
    if (!modelo && compara_texto(ext, "pas") == 0) modelo = "pascal {archivo} -o /tmp/{base} && /tmp/{base}";
    if (!modelo) { avisar("No sé ejecutar archivos .%s (configúralo en .mico)", ext); return; }
    char cmd[400];
    cmd[0] = 0x15;   /* Ctrl+U: borra lo que estuviera escrito en el prompt */
    montar_comando(modelo, d, cmd + 1, (int)sizeof cmd - 2);
    int n = (int)tam_texto(cmd);
    cmd[n++] = '\n';
    cmd[n] = 0;
    execucao_comecou();
    ver_painel = true;
    painel_aba = PAINEL_TERMINAL;
    layout();
    registrar("mico: F5 %s", cmd + 1);
    painel_enviar(cmd, n);
    avisar("Ejecutando %s", doc_nome(d));
}

/* ------------------------------------------------------------ acciones */

static void alternar_painel(void) { ver_painel = !ver_painel; if (!ver_painel && foco == FOCO_TERMINAL) foco = FOCO_EDITOR; layout(); }

static bool sair_pedido;

static bool pode_sair(void)
{
    int n = 0;
    for (int i = 0; i < qtd_abas; i++) n += doc_modificado(abas[i]);
    if (!n) return true;
    char m[200];
    formatar_texto(m, sizeof m, n == 1 ? "Hay %d archivo con cambios sin guardar.\n¿Guardarlo antes de salir?"
                                       : "Hay %d archivos con cambios sin guardar.\n¿Guardarlos antes de salir?", n);
    if (confirmar("Mico", m)) {
        for (int i = 0; i < qtd_abas; i++)
            if (doc_modificado(abas[i])) { trocar_aba(i); if (!salvar(abas[i])) return false; }
    }
    return true;
}

void executar_acao(int a)
{
    doc_t *d = DOC;
    switch (a) {
    case A_NOVO: { int i = nova_aba(doc_novo()); if (i >= 0) trocar_aba(i); foco = FOCO_EDITOR; break; }
    case A_ABRIR: {
        char c[CAMINHO_MAX];
        formatar_texto(c, sizeof c, "%s/", pasta[0] && pasta[1] ? pasta : "");
        if (pedir_texto("Abrir archivo", "Ruta del archivo:", c, sizeof c) && c[0]) abrir_arquivo(c, true);
        break;
    }
    case A_ABRIR_PASTA: {
        char c[CAMINHO_MAX];
        copia_texto(c, pasta[0] ? pasta : "/", sizeof c);
        if (pedir_texto("Abrir carpeta", "Carpeta:", c, sizeof c) && c[0]) { abrir_pasta(c); painel_cd(pasta); foco = FOCO_EDITOR; }
        break;
    }
    case A_ABRIR_NOME: paleta_abrir(true); break;
    case A_GUARDAR: salvar(d); break;
    case A_GUARDAR_COMO: salvar_como(d); break;
    case A_GUARDAR_TODO: for (int i = 0; i < qtd_abas; i++) if (doc_modificado(abas[i]) || !abas[i]->caminho[0]) salvar(abas[i]); break;
    case A_CERRAR_ABA: fechar_aba(aba_atual); break;
    case A_SALIR: if (pode_sair()) sair_pedido = true; break;
    case A_DESHACER: vista_desfazer(false); break;
    case A_REHACER: vista_desfazer(true); break;
    case A_CORTAR: vista_cortar(); break;
    case A_COPIAR: vista_copiar(); break;
    case A_PEGAR: vista_colar(); break;
    case A_SEL_TODO: vista_selecionar_tudo(); break;
    case A_COMENTAR: vista_comentar(); break;
    case A_DUPLICAR: vista_duplicar(); break;
    case A_SUBIR_LINEA: vista_mover_linhas(-1); break;
    case A_BAJAR_LINEA: vista_mover_linhas(1); break;
    case A_COMPLETAR: vista_completar(); break;
    case A_TAB_ESPACIOS: tab_espacos = !tab_espacos; sujo |= R_ESTADO; break;
    case A_TAB_2: case A_TAB_4: case A_TAB_8:
        tab_largura = a == A_TAB_2 ? 2 : a == A_TAB_4 ? 4 : 8;
        vista_sujar_tudo(); sujo |= R_ESTADO;
        break;
    case A_BUSCAR: busca_abrir(BUSCA_BUSCAR); break;
    case A_REEMPLAZAR: busca_abrir(BUSCA_TROCAR); break;
    case A_BUSCAR_SIG: busca_proximo(1); break;
    case A_BUSCAR_ANT: busca_proximo(-1); break;
    case A_BUSCAR_PASTA: busca_abrir(BUSCA_PASTA); break;
    case A_IR_LINEA: busca_abrir(BUSCA_LINHA); break;
    case A_PALETA: paleta_abrir(false); break;
    case A_VER_LATERAL: ver_lateral = !ver_lateral; if (!ver_lateral && foco == FOCO_EXPLORADOR) foco = FOCO_EDITOR; layout(); break;
    case A_VER_PAINEL: alternar_painel(); break;
    case A_VER_TERMINAL:
        if (!ver_painel || painel_aba != PAINEL_TERMINAL) { ver_painel = true; painel_aba = PAINEL_TERMINAL; foco = FOCO_TERMINAL; layout(); }
        else { foco = foco == FOCO_TERMINAL ? FOCO_EDITOR : FOCO_TERMINAL; sujo |= R_PAINEL; vista_sujar_tudo(); }
        break;
    case A_VER_PROBLEMAS: ver_painel = true; painel_aba = PAINEL_PROBLEMAS; layout(); break;
    case A_VER_BUSQUEDA: ver_painel = true; painel_aba = PAINEL_BUSCA; layout(); break;
    case A_VER_ASISTENTE: ver_painel = true; painel_aba = PAINEL_ASSISTENTE; foco = FOCO_ASISTENTE; layout(); break;
    case A_TEMA_CLARO: aplicar_tema(false); break;
    case A_TEMA_OSCURO: aplicar_tema(true); break;
    case A_PANTALLA: jan_tela_cheia(j, !(j->estilo & JAN_TELA_CHEIA)); break;
    case A_EJECUTAR: executar_atual(); break;
    case A_DETENER: painel_enviar("\x03", 1); avisar("Interrupción (Ctrl+C) enviada al terminal"); break;
    case A_LIMPIAR_TERM: painel_enviar("\x0c", 1); break;
    case A_SIG_ABA: trocar_aba((aba_atual + 1) % qtd_abas); break;
    case A_ANT_ABA: trocar_aba((aba_atual + qtd_abas - 1) % qtd_abas); break;
    case A_AYUDA: abrir_arquivo("/ayuda/mico.md", true); break;
    case A_ACERCA:
        mensagem("Acerca de Mico", "Mico, el editor de código de SaguiOS.\n"
                 "Pestañas, explorador, resaltado de C, Python,\n"
                 "sh, Markdown y CHIP-8, terminal y problemas.\n"
                 "F1: ayuda (/ayuda/mico.md).");
        break;
    }
    if (a != A_SALIR) titulo_atualizar();
}

/* Atajos que valen en cualquier panel. true = era un atajo. */
bool tecla_global(unsigned t, unsigned mods)
{
    bool ctrl = mods & M_CTRL, shift = mods & M_SHIFT;
    bool ed = foco == FOCO_EDITOR;
    int a = -1;
    if (t == T_F5) a = shift ? A_DETENER : A_EJECUTAR;
    else if (t == T_F1) a = A_AYUDA;
    else if (t == T_F3) a = shift ? A_BUSCAR_ANT : A_BUSCAR_SIG;
    else if (t == T_F6) a = A_VER_TERMINAL;
    else if (ctrl) {
        switch (t) {
        case 14: a = A_NOVO; break;                                   /* Ctrl+N */
        case 15: a = A_ABRIR; break;                                  /* Ctrl+O */
        case 19: a = shift ? A_GUARDAR_COMO : A_GUARDAR; break;      /* Ctrl+S */
        case 23: a = A_CERRAR_ABA; break;                             /* Ctrl+W */
        case 17: a = A_SALIR; break;                                  /* Ctrl+Q */
        case 16: a = shift ? A_PALETA : A_ABRIR_NOME; break;         /* Ctrl+P */
        case 6:  a = shift ? A_BUSCAR_PASTA : A_BUSCAR; break;       /* Ctrl+F */
        case 8:  a = A_REEMPLAZAR; break;                             /* Ctrl+H */
        case 7:  a = A_IR_LINEA; break;                               /* Ctrl+G */
        case 2:  a = A_VER_LATERAL; break;                            /* Ctrl+B */
        case 10: a = A_VER_PAINEL; break;                             /* Ctrl+J */
        case '\t': a = shift ? A_ANT_ABA : A_SIG_ABA; break;          /* Ctrl+Tab */
        case TECLA_PGDN: a = A_SIG_ABA; break;
        case TECLA_PGUP: a = A_ANT_ABA; break;
        }
        if (a < 0 && ed) {   /* estos solo en el editor: en la terminal, Ctrl+C, Ctrl+D... son del shell */
            switch (t) {
            case 26: a = shift ? A_REHACER : A_DESHACER; break;      /* Ctrl+Z */
            case 25: a = A_REHACER; break;                            /* Ctrl+Y */
            case 24: a = A_CORTAR; break;                             /* Ctrl+X */
            case 3:  a = A_COPIAR; break;                             /* Ctrl+C */
            case 22: a = A_PEGAR; break;                              /* Ctrl+V */
            case 1:  a = A_SEL_TODO; break;                           /* Ctrl+A */
            case 4:  a = A_DUPLICAR; break;                           /* Ctrl+D */
            case '/': case '7': a = A_COMENTAR; break;                /* Ctrl+/ (en ES, Mayús+7) */
            case ' ': a = A_COMPLETAR; break;                         /* Ctrl+Espacio */
            }
        }
    }
    if (a < 0) return false;
    if (busca_modo != BUSCA_FECHADA && (a == A_PEGAR) && ed) return false;   /* Ctrl+V en la barra de búsqueda */
    registrar("mico: atajo -> %s", acoes[a].nome);
    executar_acao(a);
    return true;
}

/* ------------------------------------------------------------ estado */

static void desenhar_estado(void)
{
    doc_t *d = DOC;
    recorte = r_estado;
    d_ret(r_estado.x, r_estado.y, r_estado.l, r_estado.a, tema->estado);
    if (aviso[0] && (int)(tempo_ms() - aviso_ate) > 0) aviso[0] = 0;
    int l = doc_linha_de(d, d->cursor), s0, s1;
    char dir[160];
    int n = formatar_texto(dir, sizeof dir, "Lín %d, col %d", l + 1, d->cursor - doc_ini(d, l) + 1);
    if (vista_selecao(&s0, &s1)) n += formatar_texto(dir + n, sizeof dir - (size_t)n, " (%d sel.)", s1 - s0);
    n += formatar_texto(dir + n, sizeof dir - (size_t)n, "   %s   %s: %d   %s", nome_ling[d->ling],
                        tab_espacos ? "Espacios" : "Tab", tab_largura, d->utf8 ? "UTF-8" : "Latin-1");
    int dx = r_estado.l - 8 - n * LETRA_L;
    d_texto(dx, r_estado.y + 2, dir, n, tema->estado_texto);
    const char *esq = aviso[0] ? aviso : d->caminho[0] ? d->caminho : "Sin guardar";
    d_texto_ate(8, r_estado.y + 2, esq, dx - 24, tema->estado_texto);
    d_mostrar(r_estado);
}

/* ------------------------------------------------------------ eventos */

enum { REG_NADA, REG_MENU, REG_ABAS, REG_LATERAL, REG_BUSCA, REG_EDITOR, REG_PAINEL, REG_ESTADO };

static int regiao(int x, int y)
{
    if (dentro(x, y, r_menu)) return REG_MENU;
    if (ver_lateral && dentro(x, y, r_lateral)) return REG_LATERAL;
    if (dentro(x, y, r_abas)) return REG_ABAS;
    if (busca_modo != BUSCA_FECHADA && dentro(x, y, r_busca)) return REG_BUSCA;
    if (dentro(x, y, r_editor)) return REG_EDITOR;
    if (ver_painel && dentro(x, y, r_painel)) return REG_PAINEL;
    if (dentro(x, y, r_estado)) return REG_ESTADO;
    return REG_NADA;
}

static int aba_menu = -1;   /* pestaña clicada con el botón derecho */

static void clique_abas(int x, int botoes)
{
    int cx = r_abas.x - abas_desl;
    for (int i = 0; i < qtd_abas; i++) {
        int w = aba_larg(i);
        if (x >= cx && x < cx + w) {
            if (botoes & 2) {
                aba_menu = i;
                menu_contextual(j->id, x, r_abas.y + r_abas.a, "Cerrar\tCtrl+W\nCerrar las demás\n-\nCopiar la ruta", 5);
                return;
            }
            if (x >= cx + w - 24) fechar_aba(i);
            else trocar_aba(i);
            foco = FOCO_EDITOR;
            return;
        }
        cx += w;
    }
}

static void menu_editor(int x, int y)
{
    menu_contextual(j->id, x, y, "Cortar\tCtrl+X\nCopiar\tCtrl+C\nPegar\tCtrl+V\n-\nSeleccionar todo\tCtrl+A\n"
                    "Comentar o descomentar\tCtrl+/\nDuplicar línea\tCtrl+D\n-\nIr a la línea...\tCtrl+G\n"
                    "Buscar...\tCtrl+F\nPaleta de órdenes...\tCtrl+Mayús+P\n-\nExplicar código\nCorregir con Problemas\nGenerar función", 3);
}

static void tratar_menu(int codigo, int idx)
{
    registrar("mico: menu %d -> %d", codigo, idx);
    if (codigo >= 10) {
        menu_aberto = -1;
        sujo |= R_MENU;
        if (idx >= 0 && idx < MENU_MAX_ITENS && linha_acao[idx] >= 0) executar_acao(linha_acao[idx]);
        return;
    }
    if (idx < 0) return;
    if (codigo == 2) expl_menu(idx);
    else if (codigo == 3) {
        static const int a[] = { A_CORTAR, A_COPIAR, A_PEGAR, -1, A_SEL_TODO, A_COMENTAR, A_DUPLICAR, -1,
                                 A_IR_LINEA, A_BUSCAR, A_PALETA, -1, -100, -101, -102 };
        if (idx < (int)(sizeof a / sizeof a[0])) {
            if (a[idx] >= 0) executar_acao(a[idx]);
            else if (a[idx] == -100) painel_ia_acao(1);
            else if (a[idx] == -101) painel_ia_acao(2);
            else if (a[idx] == -102) painel_ia_acao(3);
        }
    } else if (codigo == 4) painel_menu(idx);
    else if (codigo == 5 && aba_menu >= 0 && aba_menu < qtd_abas) {
        if (idx == 0) fechar_aba(aba_menu);
        else if (idx == 1) {
            doc_t *manter = abas[aba_menu];
            for (int i = qtd_abas - 1; i >= 0; i--) if (abas[i] != manter && qtd_abas > 1) fechar_aba(i);
        } else if (idx == 3) { const char *c = abas[aba_menu]->caminho; copiar_texto(c, tam_texto(c)); avisar("Ruta copiada"); }
    }
}

static void tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) { if (pode_sair()) sair_pedido = true; return; }
    if (jan_redimensionar(j, e)) { layout(); registrar("mico: ventana %dx%d", j->larg, j->alt); return; }
    if (e->tipo == EV_MENU) { tratar_menu((int)e->tecla, e->x); return; }
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla, m = e->modificadores;
        if (painel_ia_em_curso()) { if (t == TECLA_ESC) painel_ia_deter(); return; }
        if (foco == FOCO_ASISTENTE && painel_ia_tecla(t, m)) return;
        if (paleta_aberta) { paleta_tecla(t, m); return; }
        if (tecla_global(t, m)) return;
        if (foco == FOCO_EDITOR && busca_tecla(t, m)) return;
        if (foco == FOCO_TERMINAL) { painel_tecla(t, m); return; }
        if (foco == FOCO_EXPLORADOR) { if (expl_tecla(t)) return; if (t == TECLA_ESC) { foco = FOCO_EDITOR; sujo |= R_LATERAL; vista_sujar_tudo(); } return; }
        if (t == TECLA_ESC && busca_modo != BUSCA_FECHADA) { busca_fechar(); return; }
        vista_tecla(t, m);
        return;
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques, r = regiao(e->x, e->y);
        if (r == REG_EDITOR) vista_rodar(n);
        else if (r == REG_LATERAL) expl_rodar(n);
        else if (r == REG_PAINEL) painel_rodar(n);
        return;
    }
    if (e->tipo == EV_CLIQUE) {
        if (paleta_aberta && paleta_clique(e->x, e->y)) return;
        int r = regiao(e->x, e->y), antes = foco;
        arrastando_em = r;
        switch (r) {
        case REG_MENU:
            for (int m = 0; m < QTD_MENUS; m++) {
                int x = menu_x(m), w = (int)tam_texto(nome_menu[m]) * LETRA_L + 16;
                if (e->x >= x && e->x < x + w) { abrir_menu(m); break; }
            }
            break;
        case REG_ABAS: clique_abas(e->x, (int)e->botoes); break;
        case REG_LATERAL: expl_clique(e->x, e->y, (int)e->botoes, (int)e->cliques); break;
        case REG_BUSCA: foco = FOCO_EDITOR; busca_clique(e->x, e->y); break;
        case REG_EDITOR:
            foco = FOCO_EDITOR;
            if (e->botoes & 2) menu_editor(e->x, e->y);
            else vista_clique(e->x, e->y, (int)e->botoes, (int)e->cliques, e->modificadores);
            break;
        case REG_PAINEL: painel_clique(e->x, e->y, (int)e->botoes, (int)e->cliques); break;
        }
        if (foco != antes) { sujo |= R_LATERAL | R_PAINEL; vista_sujar_tudo(); }
        return;
    }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) {
        if (arrastando_em == REG_EDITOR) vista_mover(e->x, e->y);
        else if (arrastando_em == REG_PAINEL) painel_mover(e->x, e->y);
        return;
    }
    if (e->tipo == EV_SOLTOU) {
        if (arrastando_em == REG_EDITOR) vista_soltar();
        else if (arrastando_em == REG_PAINEL) painel_soltar();
        arrastando_em = REG_NADA;
    }
}

static void redesenhar(void)
{
    if (!sujo) return;
    unsigned s = sujo;
    sujo = 0;
    if (s & R_MENU) desenhar_menu();
    if (s & R_ABAS) desenhar_abas();
    if (s & R_LATERAL) expl_desenhar();
    if (s & R_BUSCA) busca_desenhar();
    if (s & R_EDITOR) vista_desenhar(false);
    if (s & R_PAINEL) painel_desenhar();
    if (s & R_ESTADO) desenhar_estado();
    if (paleta_aberta && (s & (R_EDITOR | R_ABAS | R_LATERAL | R_BUSCA))) paleta_desenhar();
}

/* ------------------------------------------------------------ relojes */

static void cuidar_recuperacao(void)
{
    for (int i = 0; i < qtd_abas; i++) {
        doc_t *d = abas[i];
        if (d->versao == d->versao_recup) continue;
        if (doc_modificado(d)) recup_gravar(d);
        else { recup_apagar(d); d->versao_recup = d->versao; }
    }
}

static void conferir_disco(void)
{
    doc_t *d = DOC;
    int r = doc_conferir_disco(d);
    if (r == 0) return;
    if (r < 0) { d->disco_ok = false; avisar("«%s» ya no existe en el disco", doc_nome(d)); registrar("mico: %s borrado por fuera", d->caminho); return; }
    registrar("mico: %s cambio en disco", d->caminho);
    bool recarregar = !doc_modificado(d);
    if (!recarregar) {
        char m[CAMINHO_MAX + 120];
        formatar_texto(m, sizeof m, "«%s» ha cambiado en el disco.\n¿Recargarlo? Se perderán los cambios hechos aquí.", doc_nome(d));
        recarregar = confirmar("Mico", m);
    }
    int lin = doc_linha_de(d, d->cursor), topo = d->topo, erro;
    char c[CAMINHO_MAX];
    copia_texto(c, d->caminho, sizeof c);
    if (recarregar && doc_carregar(d, c, &erro)) {
        if (lin >= d->nli) lin = d->nli - 1;
        d->cursor = doc_ini(d, lin);
        d->topo = topo < d->nli ? topo : 0;
        recup_apagar(d);
        avisar("«%s» cambió en el disco: recargado", doc_nome(d));
        registrar("mico: recargado %s", c);
        vista_doc_trocou();
    } else if (!recarregar) doc_marcar_disco(d);   /* no preguntar de nuevo por la misma versión del disco */
}

/* ------------------------------------------------------------ recuperación al comienzo */

static bool dentro_da_pasta(const char *c)
{
    int n = (int)tam_texto(pasta);
    if (!pasta[0]) return false;
    if (n == 1) return true;
    return compara_texto_n(c, pasta, (size_t)n) == 0 && c[n] == '/';
}

static void oferecer_recuperacao(void)
{
    static char cams[16][CAMINHO_MAX], arqs[16][80];
    int n = recup_listar(cams, arqs, 16), k = 0;
    char nomes[200] = "";
    for (int i = 0; i < n; i++) {
        bool aberto = false;
        for (int a = 0; a < qtd_abas; a++) if (cams[i][0] && compara_texto(abas[a]->caminho, cams[i]) == 0) aberto = true;
        if (!(!cams[i][0] || dentro_da_pasta(cams[i])) || aberto) { arqs[i][0] = 0; continue; }
        if (k < 3) {
            const char *b = acha_ultimo_char(cams[i], '/');
            junta_texto(nomes, "\n  ", sizeof nomes);
            junta_texto(nomes, cams[i][0] ? (b ? b + 1 : cams[i]) : "(sin nombre)", sizeof nomes);
        }
        k++;
    }
    if (!k) return;
    char m[400];
    formatar_texto(m, sizeof m, "Mico se cerró sin guardar %d archivo%s:%s%s\n¿Recuperar los cambios?", k, k == 1 ? "" : "s", nomes, k > 3 ? "\n  ..." : "");
    registrar("mico: %d copias de recuperacion", k);
    bool sim = confirmar("Mico - recuperación", m);
    recuperando = true;
    for (int i = 0; i < n; i++) {
        if (!arqs[i][0]) continue;
        if (!sim) { apagar(arqs[i]); continue; }
        if (cams[i][0]) { abrir_arquivo(cams[i], true); continue; }
        char cam[CAMINHO_MAX];
        int tn;
        char *t = recup_ler(arqs[i], cam, &tn);
        apagar(arqs[i]);   /* el sin nombre obtiene otra copia, con el pid nuevo */
        if (!t) continue;
        doc_t *d = doc_novo();
        aplicar_recuperado(d, t, tn);
        libera(t);
        int a = nova_aba(d);
        if (a >= 0) trocar_aba(a);
    }
    recuperando = false;
    if (sim) registrar("mico: recuperados %d archivos", k);
    sujo |= R_TUDO;
}

/* ------------------------------------------------------------ modo -t */

static int modo_texto(const char *c)
{
    int n, erro;
    bool u;
    char *s = ler_arquivo(c, &n, &erro, &u);
    if (!s) { imprimir_em(ERROS, "mico: no se ha podido abrir %s (%s)\n", c, nome_erro(erro)); return 1; }
    int ling = ling_do_nome(c, s, n < 64 ? n : 64), linhas = 1;
    for (int i = 0; i < n; i++) linhas += s[i] == '\n';
    imprimir("mico -t: %s, %s, %d lineas\n", c, nome_ling[ling], linhas);
    static uint8_t cl[LINHA_MAX];
    static char out[LINHA_MAX + 1];
    int est = 0;
    for (int i = 0, l = 1, ini = 0; i <= n; i++) {
        if (i < n && s[i] != '\n') continue;
        int k = i - ini < LINHA_MAX ? i - ini : LINHA_MAX;
        est = realce_linha(ling, s + ini, k, est, cl);
        for (int x = 0; x < k; x++) out[x] = s[ini + x] == ' ' || s[ini + x] == '\t' ? ' ' : letra_classe[cl[x]];
        out[k] = 0;
        imprimir("%4d|%s\n", l, out);
        l++;
        ini = i + 1;
    }
    libera(s);
    return 0;
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-t") == 0) {
        if (argc < 3) { escrever_texto(ERROS, "uso: mico -t archivo\n"); return 1; }
        return modo_texto(argv[2]);
    }
    j = jan_abrir("Mico", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 560, 360);
    recup_iniciar();
    nova_aba(doc_novo());
    /* Argumentos: una carpeta (pasa a ser la carpeta abierta) y/o archivos. Un archivo
     * solo también abre su carpeta en el explorador. */
    for (int i = 1; i < argc; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) >= 0 && inf.tipo == TIPO_PASTA) abrir_pasta(argv[i]);
    }
    for (int i = 1; i < argc; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) >= 0 && inf.tipo == TIPO_PASTA) continue;
        char c[CAMINHO_MAX];
        if (argv[i][0] == '/') copia_texto(c, argv[i], sizeof c);
        else { char a[CAMINHO_MAX]; pasta_atual(a, sizeof a); formatar_texto(c, sizeof c, "%s/%s", a[1] ? a : "", argv[i]); }
        if (!pasta[0]) {
            char p[CAMINHO_MAX];
            copia_texto(p, c, sizeof p);
            char *b = acha_ultimo_char(p, '/');
            if (b && b != p) { *b = 0; abrir_pasta(p); }
        }
        abrir_arquivo(c, true);
    }
    painel_iniciar();
    layout();
    titulo_atualizar();
    redesenhar();
    jan_mostrar(j);
    oferecer_recuperacao();
    registrar("mico: pronto (%d pestanas, carpeta %s)", qtd_abas, pasta[0] ? pasta : "-");

    unsigned prox_recup = tempo_ms() + RECUP_MS, prox_disco = tempo_ms() + DISCO_MS;
    evento_t e;
    while (!sair_pedido) {
        bool fez = false;
        int lote = 0;
        while (!sair_pedido && lote < 64 && proximo_evento(j->id, &e, false) == 1) { tratar(&e); fez = true; lote++; }
        if (sair_pedido) break;
        if (painel_ler()) fez = true;
        unsigned agora = tempo_ms();
        if ((int)(agora - prox_recup) >= 0) { cuidar_recuperacao(); prox_recup = agora + RECUP_MS; }
        if ((int)(agora - prox_disco) >= 0) { conferir_disco(); prox_disco = agora + DISCO_MS; }
        if (aviso[0] && (int)(agora - aviso_ate) > 0) sujo |= R_ESTADO;
        redesenhar();
        painel_terminal_mostrar();
        titulo_atualizar();
        if (!fez) dormir(10);
    }
    /* Salida limpia: las copias de recuperación de los que se descartaron desaparecen. */
    for (int i = 0; i < qtd_abas; i++) recup_apagar(abas[i]);
    painel_fim();
    registrar("mico: cerrado");
    jan_fechar(j);
    return 0;
}
