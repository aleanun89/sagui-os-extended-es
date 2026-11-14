// SPDX-License-Identifier: GPL-2.0-or-later
/* mico.h - el Mico, editor de código de SaguiOS (mejora 037, D075).
 *
 * División de los archivos:
 *   documento.c  el texto de cada pestaña: gap buffer, índice de líneas,
 *                deshacer/rehacer, leer, guardar (atómico) y recuperación
 *   realce.c     colores por lenguaje (C, Python, sh, Markdown, CHIP-8)
 *   desenho.c    temas, letras de la fuente 8x16 y rectángulos con recorte
 *   vista.c      el editor en pantalla: dibujo por líneas sucias, teclas, mouse
 *   explorador.c el árbol de la carpeta abierta
 *   painel.c     panel de abajo: terminal (D036), Problemas, Búsqueda y Asistente IA
 *   buscar.c     buscar y reemplazar, ir a la línea, buscar en la carpeta
 *   paleta.c     paleta de acciones (Ctrl+Mayús+P) y abrir por nombre (Ctrl+P)
 *   principal.c  ventana, diseño, menús, pestañas, F5, .mico y el bucle de eventos
 */
#ifndef MICO_H
#define MICO_H
#include "sagui.h"
#include "gui.h"
#include "ia.h"

extern const unsigned char fonte_sagui[256][16];

/* Teclas sin carácter que abi.h no nombra para los programas (ver el
 * enum de teclado.h del kernel: F1 = 0x10A). */
#define T_F1  0x10A
#define T_F3  0x10C
#define T_F5  0x10E
#define T_F6  0x10F
#define M_SHIFT 1
#define M_CTRL  2
#define M_ALT   4   /* solo en las teclas (D075) */

#define CAMINHO_MAX 256
#define LINHA_MAX   4096   /* letras de una línea que se colorean y dibujan */

/* ------------------------------------------------------------ lenguajes y realce */
enum { LING_TEXTO, LING_C, LING_PY, LING_SH, LING_MD, LING_C8, QTD_LING };
enum { CL_NORMAL, CL_CHAVE, CL_TIPO, CL_CADEIA, CL_COMENTARIO, CL_NUMERO, CL_PREPROC,
       CL_FUNCAO, CL_TITULO, CL_ENFASE, CL_REGISTRO, CL_ROTULO, CL_VARIAVEL, QTD_CLASSES };
extern const char *const nome_ling[QTD_LING];
extern const char letra_classe[QTD_CLASSES + 1];   /* para el modo -t */
int  ling_do_nome(const char *caminho, const char *inicio, int n);
/* Colorea una línea: devuelve el estado al comienzo de la siguiente. classes puede ser
 * NULL (solo acompaña el estado: comentarios y cadenas de varias líneas). */
int  realce_linha(int ling, const char *s, int n, int estado, uint8_t *classes);
const char *comentario_de(int ling);   /* "//", "#", ";" o NULL */

/* ------------------------------------------------------------ documento */
typedef struct {
    uint8_t  apagou;      /* 0 = insertó t en pos; 1 = borró t de pos */
    int      pos, n;
    char    *t;
    int      cur_antes;
    unsigned grupo;       /* las ops del mismo grupo (≠ 0) se deshacen juntas */
    unsigned tempo;
} op_t;

typedef struct {
    char    *buf;          /* gap buffer: texto = buf[0, g0) + buf[g1, cap) */
    int      cap, g0, g1, tam;
    int     *li;           /* inicio de cada línea; li[nli] = tam + 1 */
    int      nli, cap_li;
    uint8_t *est;          /* estado del realce al comienzo de cada línea */
    int      cap_est, est_ok;   /* est[0..est_ok] son válidos */
    op_t    *ops;
    int      nops, cap_ops, feitos, salvo;   /* salvo = hechos cuando se guardó (-1 = nunca más igual) */
    unsigned grupo;        /* grupo abierto (doc_grupo) o 0 */
    int      mem_ops;
    char     caminho[CAMINHO_MAX];   /* "" = sin nombre */
    int      ling;
    bool     utf8;         /* el archivo vino en UTF-8: vuelve en UTF-8 */
    int      cursor, ancora, topo, esq, col_desejada;
    uint32_t disco_tam, disco_hash;  /* para notar cambios hechos por otro programa */
    bool     disco_ok;
    int      versao, versao_recup;
    int      id;           /* para el nombre de la copia de recuperación de los sin nombre */
    /* Última edición (la vista decide qué redibujar): línea y si solo ella cambió. */
    int      ed_lin;
    bool     ed_so_linha;
} doc_t;

doc_t *doc_novo(void);
void   doc_liberar(doc_t *d);
bool   doc_carregar(doc_t *d, const char *caminho, int *erro);
void   doc_definir(doc_t *d, const char *s, int n);     /* reemplaza todo, sin deshacer */
int    doc_salvar(doc_t *d);                            /* 0 = ok; guardado atómico */
char   doc_car(doc_t *d, int pos);
void   doc_pedaco(doc_t *d, int pos, int n, char *dst);
const char *doc_texto(doc_t *d);                        /* continuo y terminado en cero (mueve el gap al final) */
int    doc_linha_de(doc_t *d, int pos);
static inline int doc_ini(doc_t *d, int l) { return d->li[l]; }
static inline int doc_fim(doc_t *d, int l) { return d->li[l + 1] - 1; }
static inline int doc_tam_linha(doc_t *d, int l) { return d->li[l + 1] - 1 - d->li[l]; }
int    doc_copiar_linha(doc_t *d, int l, char *dst, int max);   /* devuelve el tamaño copiado */
void   doc_inserir(doc_t *d, int pos, const char *s, int n);
void   doc_apagar(doc_t *d, int pos, int n);
void   doc_grupo(doc_t *d, bool abrir);                 /* varias ediciones = un paso de deshacer */
bool   doc_desfazer(doc_t *d);
bool   doc_refazer(doc_t *d);
static inline bool doc_modificado(doc_t *d) { return d->feitos != d->salvo; }
int    doc_estado(doc_t *d, int l);                     /* estado del realce al comienzo de la línea l */
const char *doc_nome(doc_t *d);                          /* solo el nombre del archivo */
uint32_t hash_texto(const char *s, int n);
void   doc_marcar_disco(doc_t *d);                      /* recuerda tamaño y hash de lo que está en el disco */
int    doc_conferir_disco(doc_t *d);                    /* 0 = igual, 1 = cambió, -1 = desapareció */
char  *ler_arquivo(const char *caminho, int *n, int *erro, bool *utf8);
int    gravar_atomico(const char *caminho, const char *s, int n);

/* Recuperación (copias de lo que no se guardó). */
extern char pasta_recup[64];
void recup_iniciar(void);
void recup_gravar(doc_t *d);
void recup_apagar(doc_t *d);
int  recup_listar(char caminhos[][CAMINHO_MAX], char arquivos[][80], int max);
char *recup_ler(const char *arquivo, char *caminho, int *n);

/* ------------------------------------------------------------ dibujo y tema */
typedef struct {
    uint32_t fundo, texto, linha_atual, selecao, busca, par;
    uint32_t margem_fundo, margem_texto, margem_atual, cursor;
    uint32_t barra, barra_texto, barra_borda;
    uint32_t aba, aba_ativa, aba_texto, aba_marca;
    uint32_t lateral, lateral_texto, lateral_sel, lateral_pasta;
    uint32_t painel, painel_texto, term_fundo, term_texto;
    uint32_t estado, estado_texto, destaque, erro, aviso;
    uint32_t cl[QTD_CLASSES];
} tema_t;
extern const tema_t *tema;
extern const tema_t tema_claro, tema_escuro;

typedef struct { int x, y, l, a; } ret_t;
extern jan_t *j;
extern ret_t recorte;
void d_ret(int x, int y, int l, int a, uint32_t cor);         /* con recorte */
void d_letra(int x, int y, unsigned char c, uint32_t cor);
void d_texto(int x, int y, const char *s, int n, uint32_t cor);
int  d_texto_ate(int x, int y, const char *s, int max_px, uint32_t cor);   /* corta con "..." */
void d_moldura(int x, int y, int l, int a, uint32_t cor);
void d_mostrar(ret_t r);
static inline bool dentro(int px, int py, ret_t r) { return px >= r.x && py >= r.y && px < r.x + r.l && py < r.y + r.a; }

/* ------------------------------------------------------------ estado global (principal.c) */
#define MAX_ABAS 24
extern doc_t *abas[MAX_ABAS];
extern int    qtd_abas, aba_atual;
#define DOC (abas[aba_atual])
extern char   pasta[CAMINHO_MAX];        /* carpeta abierta ("" = ninguna) */
extern int    tab_largura;               /* columnas por tabulación */
extern bool   tab_espacos;               /* Tab inserta espacios */
extern bool   ver_lateral, ver_painel;
enum { FOCO_EDITOR, FOCO_TERMINAL, FOCO_EXPLORADOR, FOCO_ASISTENTE };
extern int    foco;
extern ret_t  r_menu, r_abas, r_lateral, r_busca, r_editor, r_painel, r_estado;
extern char   aviso[160];
/* Lo que hay que redibujar al final del lote de eventos. */
enum { R_MENU = 1, R_ABAS = 2, R_EDITOR = 4, R_LATERAL = 8, R_PAINEL = 16, R_ESTADO = 32, R_BUSCA = 64, R_TUDO = 127 };
extern unsigned sujo;
void layout(void);
void avisar(const char *fmt, ...);
int  abrir_arquivo(const char *caminho, bool focar);     /* índice de la pestaña o -1 */
void ir_para(const char *caminho, int linha, int coluna);
void caminho_absoluto(const char *rel, char *dst);
void titulo_atualizar(void);
void executar_acao(int a);
bool tecla_global(unsigned t, unsigned mods);

/* ------------------------------------------------------------ vista */
void vista_doc_trocou(void);                 /* otra pestaña, o cambió todo el texto */
void vista_desenhar(bool tudo);
void vista_sujar(int l0, int l1);
void vista_sujar_tudo(void);
bool vista_tecla(unsigned t, unsigned mods);
void vista_clique(int x, int y, int botoes, int cliques, unsigned mods);
void vista_mover(int x, int y);
void vista_soltar(void);
void vista_rodar(int n);
void vista_cursor_visivel(void);
void vista_ir(int pos, bool selecionar);
void vista_selecionar(int a, int b);
bool vista_selecao(int *s0, int *s1);
void vista_copiar(void);
void vista_cortar(void);
void vista_colar(void);
void vista_selecionar_tudo(void);
void vista_comentar(void);
void vista_duplicar(void);
void vista_mover_linhas(int dir);
void vista_desfazer(bool refazer);
void vista_completar(void);
int  vista_linhas_visiveis(void);
void vista_apos_edicao(void);
void vista_inserir_texto(const char *s, int n);   /* cambia la selección, con sangría automática si es "\n" */

/* ------------------------------------------------------------ explorador */
void expl_abrir_pasta(const char *p);
void expl_atualizar(void);
void expl_desenhar(void);
void expl_clique(int x, int y, int botoes, int cliques);
void expl_rodar(int n);
bool expl_tecla(unsigned t);
void expl_menu(int idx);
void expl_marcar(const char *caminho);   /* resalta el archivo de la pestaña actual */
int  expl_arquivos(char (*lista)[CAMINHO_MAX], int max);   /* todos los archivos de la carpeta (recursivo) */

/* ------------------------------------------------------------ panel de abajo */
enum { PAINEL_TERMINAL, PAINEL_PROBLEMAS, PAINEL_BUSCA, PAINEL_ASSISTENTE };
extern int painel_aba;
void painel_iniciar(void);
void painel_fim(void);
bool painel_ler(void);                         /* lee la terminal virtual; true = llegó algo */
void painel_desenhar(void);
void painel_tamanho(void);
void painel_clique(int x, int y, int botoes, int cliques);
void painel_mover(int x, int y);
void painel_soltar(void);
void painel_rodar(int n);
void painel_tecla(unsigned t, unsigned mods);
void painel_terminal_mostrar(void);            /* solo las líneas de la terminal que cambiaron */
void painel_enviar(const char *s, int n);      /* texto para el shell, como si se escribiera */
void painel_menu(int idx);
void painel_copiar(void);
void painel_ia_acao(int acao);
void painel_ia_deter(void);
bool painel_ia_em_curso(void);
bool painel_ia_tecla(unsigned t, unsigned mods);
void painel_colar(void);
void painel_cd(const char *p);
void problemas_limpar(void);
int  problemas_qtd(void);
void busca_res_limpar(void);
void busca_res_add(const char *arq, int lin, int col, const char *texto);
int  busca_res_qtd(void);
void execucao_comecou(void);                   /* F5: los problemas son de la próxima salida */

/* ------------------------------------------------------------ propuestas de la IA (diferenca.c, D240) */
int  prop_criar(doc_t *d, int l0, int n, bool todo, const char *resp, int rn);
bool prop_ativa(void);
void prop_descartar(void);                     /* sin avisos: al pedir otra cosa */
void prop_descartar_acao(void);                /* el botón «Descartar» */
void prop_aplicar_tudo(void);
void prop_aplicar_bloco(void);
void prop_mover(int dir);
void prop_rodar(int n);
void prop_desenhar(ret_t r);
bool prop_clique(int x, int y);
bool prop_tecla(unsigned t);

/* ------------------------------------------------------------ buscar */
enum { BUSCA_FECHADA, BUSCA_BUSCAR, BUSCA_TROCAR, BUSCA_PASTA, BUSCA_LINHA };
extern int busca_modo;
extern bool busca_mai, busca_palavra;
int  busca_altura(void);
void busca_abrir(int modo);
void busca_fechar(void);
void busca_desenhar(void);
bool busca_tecla(unsigned t, unsigned mods);
void busca_clique(int x, int y);
bool busca_proximo(int dir);
int  busca_trocar_todos(void);
void busca_na_pasta(void);
/* Ocurrencias en una línea (para pintar el fondo): devuelve cuántas, en ini[]/fim[] (columnas del texto). */
int  busca_na_linha(const char *s, int n, int *ini, int *fim, int max);
bool acha_em(const char *s, int n, const char *q, int qn, bool mai, bool palavra, int de, int *onde);

/* ------------------------------------------------------------ paleta */
extern bool paleta_aberta;
void paleta_abrir(bool arquivos);
void paleta_fechar(void);
void paleta_desenhar(void);
bool paleta_tecla(unsigned t, unsigned mods);
bool paleta_clique(int x, int y);
ret_t paleta_ret(void);

/* ------------------------------------------------------------ acciones (principal.c) */
enum {
    A_NOVO, A_ABRIR, A_ABRIR_PASTA, A_ABRIR_NOME, A_GUARDAR, A_GUARDAR_COMO, A_GUARDAR_TODO,
    A_CERRAR_ABA, A_SALIR,
    A_DESHACER, A_REHACER, A_CORTAR, A_COPIAR, A_PEGAR, A_SEL_TODO, A_COMENTAR, A_DUPLICAR,
    A_SUBIR_LINEA, A_BAJAR_LINEA, A_COMPLETAR, A_TAB_ESPACIOS, A_TAB_2, A_TAB_4, A_TAB_8,
    A_BUSCAR, A_REEMPLAZAR, A_BUSCAR_SIG, A_BUSCAR_ANT, A_BUSCAR_PASTA, A_IR_LINEA,
    A_PALETA, A_VER_LATERAL, A_VER_PAINEL, A_VER_TERMINAL, A_VER_PROBLEMAS, A_VER_BUSQUEDA,
    A_TEMA_CLARO, A_TEMA_OSCURO, A_PANTALLA,
    A_EJECUTAR, A_DETENER, A_LIMPIAR_TERM, A_SIG_ABA, A_ANT_ABA,
    A_AYUDA, A_ACERCA, A_VER_ASISTENTE,
    QTD_ACOES
};
typedef struct { const char *nome, *atalho; } acao_t;
extern const acao_t acoes[QTD_ACOES];
bool acao_marcada(int a);
bool acao_ativa(int a);

#endif
