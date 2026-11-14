// SPDX-License-Identifier: GPL-3.0-only
/* sagsh.h - el shell de SaguiOS (D037), dividido en módulos:
 *
 *   util.c        textos y listas que crecen (reserva), arena del árbol
 *   analise.c     léxico + analizador recursivo: texto -> árbol (no_t)
 *   desanalise.c  árbol -> texto (para trabajos, "type" y para los sub-shells)
 *   expansao.c    $VAR, ${...}, $(...), `...`, $((...)), ~, división, glob
 *   aritmetica.c  evaluador de $((...))
 *   variaveis.c   variables, entorno, parámetros, funciones, alias
 *   executar.c    ejecución del árbol: redirecciones, pipes, trabajos
 *   internos.c    comandos internos (cd, echo, test, read, set...)
 *   ajuda.c       tabla de comandos (nombres en portugués) y el "ajuda"
 *   edicao.c      edición de la línea: historial, Tab, flechas (modo crudo)
 *   principal.c   opciones, bucle de lectura, scripts, archivo de inicio
 *
 * No hay fork (D010): los programas se ejecutan con executar() y heredan 0, 1
 * y 2; el shell arregla los descriptores con duplicar() antes y restaura después.
 * Lo que necesita correr "en paralelo" y es interno (una etapa de pipe que es un
 * bucle, un bloque en segundo plano) se convierte en un /bin/sagsh hijo que recibe el
 * estado (variables, funciones, alias) en un preludio de texto. */
#ifndef SAGSH_H
#define SAGSH_H
#include "sagui.h"

/* ------------------------------------------------------------ util.c */
typedef struct { char *s; int n, cap; } texto_t;     /* texto que crece */
typedef struct { char **v; int n, cap; } lista_t;    /* lista de textos (v[n] = NULL) */

void *sh_aloca(size_t n);
void *sh_realoca(void *p, size_t n);
char *sh_dup(const char *s);
char *sh_dup_n(const char *s, int n);
void  tx_car(texto_t *t, char c);
void  tx_junta(texto_t *t, const char *s);
void  tx_junta_n(texto_t *t, const char *s, int n);
void  tx_formatar(texto_t *t, const char *fmt, ...);
char *tx_texto(texto_t *t);     /* nunca NULL; continúa desde el texto_t */
void  tx_liberar(texto_t *t);
void  lista_poe(lista_t *l, char *s);
void  lista_liberar(lista_t *l);   /* libera también los textos */
bool  eh_nome(const char *s);      /* [A-Za-z_][A-Za-z0-9_]* */
bool  eh_nome_n(const char *s, int n);
bool  eh_numero(const char *s);    /* entero decimal, con signo opcional */
int   para_int(const char *s);
void  aspas_simples(texto_t *t, const char *s);   /* 'texto' seguro para el shell */
bool  sh_tem_term(void);                          /* TERM definido y distinto de "dumb" (D263) */
bool  sh_cor(void);                               /* ademas, NO_COLOR sin definir: se pueden mandar colores */
bool  sh_raiz_visivel(void);                      /* uid 0 y un sitio donde se vea (terminal con TERM, o modo rescate) */
void  historico_carregar(void);                   /* lee /home/.sagsh_historial (solo con TERM) */
int   erro(const char *fmt, ...);                 /* "sagsh: ..." en la salida de errores; devuelve 1 */
int   escrever_saida(int fd, const char *s, int n); /* marca pipe roto */
void  ordenar(char **v, int n);

typedef struct bloco_arena { struct bloco_arena *prox; int usado, tam; } bloco_arena_t;
typedef struct { bloco_arena_t *blocos; int fixa; } arena_t;   /* fija: tiene función definida */
arena_t *arena_nova(void);
void    *arena_aloca(arena_t *a, size_t n);
char    *arena_dup_n(arena_t *a, const char *s, int n);
void     arena_liberar(arena_t *a);   /* no libera si es "fixa" */

/* ------------------------------------------------------------ analise.c */
enum {   /* redirecciones */
    R_ENTRADA, R_SAIDA, R_ACRESCENTA, R_FORCAR, R_LER_ESCREVER,
    R_DUP_ENTRADA, R_DUP_SAIDA, R_HEREDOC, R_HERESTRING, R_TUDO, R_TUDO_ACRESCENTA
};

typedef struct redir {
    int    tipo;
    int    fd;             /* descriptor redirigido (-1 = el predeterminado del tipo) */
    char  *alvo;           /* palabra cruda (archivo, descriptor o delimitador) */
    char  *corpo;          /* here-doc: el texto */
    bool   expandir;       /* here-doc con delimitador sin comillas: expande $ y ` */
    bool   tira_tab;       /* <<- */
    struct redir *prox;
} redir_t;

enum {
    N_SIMPLES, N_PIPE, N_E, N_OU, N_LISTA, N_SUBSHELL, N_GRUPO,
    N_SE, N_ENQUANTO, N_ATE, N_PARA, N_CASO, N_FUNCAO
};

struct no;
typedef struct item_caso {
    char **padroes;
    int    npadroes;
    struct no *corpo;
    struct item_caso *prox;
} item_caso_t;

typedef struct no {
    int      tipo;
    int      linha;
    redir_t *redir;
    char   **palavras;  int npal;     /* SIMPLE: palabras; PARA: lista */
    char   **atribs;    int natr;     /* SIMPLE: NOMBRE=valor antes del comando */
    struct no **filhos; int nfilhos;  /* PIPE: etapas; LISTA: elementos */
    bool    *fundo;                   /* LISTA: el elemento termina con & */
    struct no *a, *b, *c;             /* E/OU: a b; SE: a então b senão c; bucles: a cond, b cuerpo */
    char    *nome;                    /* PARA: variable; FUNCAO: nombre; CASO: palabra */
    bool     negar;                   /* PIPE empieza con ! */
    bool     com_lista;               /* PARA tiene "in" */
    item_caso_t *itens;
    arena_t *arena;                   /* FUNCAO: arena del árbol (queda fija) */
} no_t;

enum { A_OK, A_VAZIO, A_INCOMPLETO, A_ERRO };

typedef struct {
    int    estado;       /* A_* */
    no_t  *no;           /* A_OK */
    int    fim;          /* posición después del comando */
    int    linha_erro;
    char   perto[48];    /* texto cerca del error */
} resultado_t;

/* Analiza un "comando completo" (hasta el fin de la línea) de s a partir de ini.
 * fim_entrada: ya no llega más texto (un script entero): lo que quede abierto es
 * error, y no "incompleto". */
resultado_t analisar(arena_t *ar, const char *s, int ini, int linha, bool fim_entrada);
/* Busca el ")" que cierra un $( ... ) que empieza en s[ini] (después del "$(").
 * Devuelve la posición del ")" o -1 (incompleto) / -2 (error). */
int achar_fecha_subst(const char *s, int ini);
bool eh_palavra_reservada(const char *s);

/* ------------------------------------------------------------ desanalise.c */
void desanalisar(no_t *n, texto_t *t, bool compacto);
char *texto_do_no(no_t *n, bool compacto);   /* reserva */

/* ------------------------------------------------------------ expansao.c */
bool  expandir_palavras(char **cruas, int n, lista_t *saida);   /* con división y glob */
char *expandir_texto(const char *crua, bool til);               /* sin división ni glob (asignación, redir) */
char *expandir_padrao(const char *crua);                        /* case: las comillas se vuelven \ */
char *expandir_heredoc(const char *corpo);
char *expandir_prompt(const char *ps);
char *tirar_aspas(const char *crua);
bool  casar(const char *padrao, const char *texto);
bool  tem_glob(const char *s);
void  glob_caminhos(const char *padrao, lista_t *saida);        /* sin coincidencia: nada */
extern bool erro_expansao;                                       /* ${x:?} o sustitución inválida */

/* ------------------------------------------------------------ aritmetica.c */
bool avaliar_aritmetica(const char *expr, int *valor);   /* false = error (ya avisado) */

/* ------------------------------------------------------------ variaveis.c */
#define V_EXPORTADA 1
#define V_SO_LEITURA 2

typedef struct { char *nome, *valor; int flags; } var_t;
typedef struct { char *nome; no_t *corpo; } funcao_t;
typedef struct { char *nome, *valor; } apelido_t;

const char *var_obter(const char *nome);        /* NULL = no definida */
bool        var_definir(const char *nome, const char *valor, int flags_mais);   /* false: solo lectura */
void        var_remover(const char *nome);
var_t      *var_achar(const char *nome);
void        var_exportar(const char *nome);
void        var_listar(bool so_exportadas);
char      **ambiente(lista_t *extras);          /* environ para executar (extras: NOMBRE=valor solo de este) */
void        var_local(const char *nome);        /* "local": guarda el valor para restaurar al volver */
void        locais_entrar(void);
void        locais_sair(void);

funcao_t   *funcao_achar(const char *nome);
void        funcao_definir(const char *nome, no_t *corpo);
void        funcao_remover(const char *nome);
apelido_t  *apelido_achar(const char *nome);
void        apelido_definir(const char *nome, const char *valor);
bool        apelido_remover(const char *nome);
void        apelidos_listar(void);
void        apelidos_limpar(void);

/* Parámetros posicionales ($1...) */
extern lista_t params;
void params_definir(char **v, int n);
void params_trocar(lista_t *novos, lista_t *antigos);

/* Estado entero, para los sub-shells "( ... )" y $( ... ) en el propio proceso. */
typedef struct estado_salvo estado_salvo_t;
estado_salvo_t *estado_salvar(void);
void            estado_restaurar(estado_salvo_t *e);
void            serializar_estado(texto_t *t);    /* preludio para un sagsh hijo */

/* ------------------------------------------------------------ executar.c */
/* F_ABORTAR: error grave (pila agotada): vuelve hasta el bucle principal */
enum { F_NORMAL, F_PARAR, F_CONTINUAR, F_VOLTAR, F_SAIR, F_INTERROMPIDO, F_PIPE, F_ABORTAR };

typedef struct {
    int   status;            /* $? */
    int   status_subst;      /* $? del último $( ) del comando actual (-1 = ninguno) */
    int   pid;               /* $$ */
    int   ultimo_fundo;      /* $! */
    char *nome0;             /* $0 */
    bool  interativo;
    bool  terminal;          /* la entrada 0 es un terminal */
    bool  opt_e, opt_u, opt_x, opt_f, opt_n, opt_a;
    int   fluxo;             /* F_* */
    int   nivel;             /* cuántos bucles atraviesa el break/continue */
    int   lacos;             /* profundidad de bucles (para break) */
    int   funcoes;           /* profundidad de funciones (para return/local) */
    int   condicao;          /* > 0: dentro de if/while/&&/||: set -e no vale */
    int   base_ctrl_c;       /* interrupcoes() al comienzo de la línea */
    int   em_estagio;        /* ejecutando una etapa de pipe dentro del shell */
    int   subshell;          /* profundidad de ( ) / $( ) en el propio proceso */
    int   linha;             /* línea actual del script (mensajes) */
    char *script;            /* nombre del script, para los mensajes de error */
    char *pilha_base;        /* cima de la pila en principal() (límite de recursión) */
} shell_t;
extern shell_t sh;

int  executar_no(no_t *n);
int  executar_texto(const char *texto, const char *nome);   /* eval, source, -c */
void relatar_sintaxe(resultado_t *r);
int  executar_subshell_texto(const char *texto);            /* $( ... ) */
bool checar_interrupcao(void);
void recolher_trabalhos(bool avisar);
void listar_trabalhos(void);
int  esperar_trabalhos(int argc, char **argv);
int  trazer_trabalho(int argc, char **argv);
int  pids_do_trabalho(const char *spec, int *pids, int max);
int  fd_livre(void);
char *achar_programa(const char *nome);   /* ruta en PATH, o NULL (reserva) */
int  status_de_codigo(int codigo);

/* ------------------------------------------------------------ internos.c */
typedef int (*interno_f)(int argc, char **argv);
interno_f interno_achar(const char *nome);
bool      interno_especial(const char *nome);   /* las asignaciones antes de él quedan */
const char *nome_canonico(const char *nome);    /* alias en portugués -> nombre */
const char *nome_interno(int i);                /* para el Tab; NULL al final */
char       *ler_arquivo_texto(const char *caminho, int *erro_r);   /* UTF-8 -> Latin-1 */
void        rodar_trap_saida(void);

/* ------------------------------------------------------------ ajuda.c */
typedef struct {
    const char *nome, *apelido;
    bool        interno;
    bool        grafico;   /* abre una ventana: se ejecuta en segundo plano, sin retener el shell */
    const char *curta;     /* hasta 17 caracteres, para la lista de "ajuda" */
    const char *uso;
    const char *explicacao;
} comando_t;
extern const comando_t comandos[];
extern const int qtd_comandos;
const comando_t *achar_comando(const char *nome);
int interno_ajuda(int argc, char **argv);

/* ------------------------------------------------------------ edicao.c */
int  ler_linha_editada(const char *prompt, char **linha);   /* -1 al final de la entrada */
void historico_por(const char *linha);
void historico_listar(int ultimos);
void historico_limpar(void);
extern const char *prompt_atual;

/* ------------------------------------------------------------ principal.c */
void sair_do_shell(int codigo) __attribute__((noreturn));

#endif
