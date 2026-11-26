/* qjs_bridge.h - o QuickJS com um DOM mínimo para o Cipó (D149).
 *
 * O motor da web (motor.c) nao corre JavaScript: o interpretador que havia
 * (js.c) so era usado pela copia de referencia do Cipo. Agora as paginas
 * correm o QuickJS (bellard/quickjs, MIT), com um DOM mínimo:
 *
 *   document.title          (ler e escrever)
 *   document.body.innerHTML (ler e escrever)
 *   document.body.textContent (ler e escrever)
 *   document.URL            (a pagina que esta aberta)
 *   console.log(...)        (escreve no registo do SO)
 *   alert(...)              (a pagina mostra um aviso)
 *   setTimeout(fn, ms) / clearTimeout(id)
 *
 * E a janela, que e o que faz uma pagina de reenvio (a do DuckDuckGo, por
 * exemplo) funcionar:
 *
 *   window                  (o proprio objecto global)
 *   window.parent / .top / .self
 *   location.href = "..."   (muda de pagina)
 *   location.assign("...") / location.replace("...")
 *   location.reload()
 *   navigator.userAgent
 *
 * E, desde a D160, o DOM de verdade: a pagina e uma arvore de elementos que
 * o script pode consultar (getElementById, getElementsByTagName,
 * querySelector, querySelectorAll) e em que pode pôr um tratador de clique
 * (elemento.onclick e elemento.addEventListener("click", ...)).
 */
#ifndef SAGUI_QJS_BRIDGE_H
#define SAGUI_QJS_BRIDGE_H

/* Os ganchos para o browser: o que o script fez e o que o SO tem de
 * mostrar. Todos podem ser NULL.
 *
 * A ordem dos campos importa: quem os inicializa tem de os por todos, na
 * posicao certa (o ultimo que se acrescente fica no fim). */
typedef struct {
    void (*titulo)(const char *t);      /* document.title = ... */
    void (*corpo)(const char *html);    /* document.body.innerHTML = ... */
    void (*alerta)(const char *t);      /* alert(...) */
    void (*log)(const char *t);         /* console.log(...) */
    void (*navegar)(const char *url);   /* location.href = ... (D159) */
    /* D271: o script muda um atributo de um elemento (valor NULL = tira-o) ou o seu texto;
     * dom e o identificador do no. Textos em UTF-8. */
    void (*atributo)(int dom, const char *nome, const char *valor);
    void (*texto)(int dom, const char *txt);
    /* fetch e XMLHttpRequest (so GET): devolve o corpo (bloco de aloca, o ponte liberta-o) ou NULL */
    char *(*baixar)(const char *url, int *n);
    /* D488: o DOM mutavel, as medidas, o desplazamiento e o historial. Textos em UTF-8;
     * os nos pelo seu identificador (D160). Quem inicializa os ganchos, que o faca com
     * designadores (.dom_criar = ...): assim os campos novos de outras decisoes nao trocam. */
    int   (*dom_criar)(const char *tag, const char *texto);   /* no novo solto ("#texto" = texto); id ou -1 */
    int   (*dom_inserir)(int pai, int no, int antes);          /* antes = -1: no fim */
    int   (*dom_tirar)(int no);
    int   (*dom_dado)(int no, const char *txt);                /* o texto de um no de texto */
    char *(*dom_html)(int no, const char *html, int *n);       /* innerHTML: devolve a lista dos nos novos (aloca) */
    int   (*caixa)(int no, int *o16);                          /* como motor_caixas; maqueta antes se o DOM mudou */
    int   (*estilo)(int no, const char *prop, char *o, int max);
    int   (*elemento_em)(int x, int y);                        /* coordenadas da area da pagina */
    void  (*janela)(int *o6);                                  /* innerWidth, innerHeight, scrollX, scrollY, largura e altura da pagina */
    void  (*rolar)(int x, int y);                              /* scrollTo */
    void  (*historia)(const char *url, int substituir);        /* pushState / replaceState / location.hash */
    void  (*ir)(int delta);                                    /* history.back / forward / go */
    /* para la prueba (h488): la firma de la pintura y apagar el remaquetado incremental */
    unsigned (*firma)(void);
    void  (*incremental)(int sim);
    /* scrollTop/scrollLeft de los bloques con overflow (D489): [x, y, max x, max y], y moverlos */
    int   (*rolagem)(int no, int *o4);
    int   (*rolar_caixa)(int no, int x, int y);
} qjs_ganchos_t;

typedef struct qjs_ponte qjs_ponte_t;

/* Cria o ponte (runtime, contexto e DOM). NULL se a memoria falhar. */
qjs_ponte_t *qjs_criar(const qjs_ganchos_t *g);

/* Executa um script. Devolve 0 se correu bem e -1 se deu erro (a mensagem
 * do erro fica no registo do SO). */
int qjs_executar(qjs_ponte_t *p, const char *codigo, int n, const char *nome);

/* Como qjs_executar, mas escreve o valor do resultado em `saida` (o texto do
 * valor, como o console.log o mostraria). */
int qjs_executar_texto(qjs_ponte_t *p, const char *codigo, int n, const char *nome,
                        char *saida, int max_saida);

/* Corre os temporizadores que ja venceram. Devolve quantos correram. */
int qjs_tique(qjs_ponte_t *p, unsigned agora_ms);

/* D271: o tamanho da janela (innerWidth, innerHeight e matchMedia) e o fim da carga da
 * pagina (DOMContentLoaded e load: chamar depois de correr os <script>). */
void qjs_definir_janela(qjs_ponte_t *p, int larg, int alt);
void qjs_carga(qjs_ponte_t *p);

/* O que a pagina ja tinha antes do script: o titulo e o HTML do corpo. */
void qjs_definir_titulo(qjs_ponte_t *p, const char *t);
void qjs_definir_corpo(qjs_ponte_t *p, const char *html);

/* O endereco da pagina que esta aberta: e o que o location devolve e o que
 * serve de base aos enderecos relativos que o script pedir. */
void qjs_definir_url(qjs_ponte_t *p, const char *url);

/* O titulo que o script pôs (NULL se nao pôs nada). */
const char *qjs_titulo(qjs_ponte_t *p);

/* O HTML do corpo, se o script o trocou (NULL se nao tocou nele). */
const char *qjs_corpo(qjs_ponte_t *p);

/* As ultimas linhas que o script escreveu no console (NULL se nenhuma). */
const char *qjs_console(qjs_ponte_t *p);

/* Os avisos que o script deu (NULL se nenhum). */
const char *qjs_alerta(qjs_ponte_t *p);

/* ---- o DOM da pagina (D160) ----
 *
 * O navegador (o Cipo) manda ao ponte a arvore da pagina como texto: uma
 * linha por no, com os campos separados por tabulador. O formato e:
 *
 *   D<TAB><id do body><TAB><id do html>
 *   N<TAB><id><TAB><id do pai><TAB><etiqueta><TAB><id><TAB><classe>
 *      <TAB><n de atributos><TAB><texto>
 *   A<TAB><nome><TAB><valor>          (n de atributos linhas a seguir)
 *
 * A etiqueta de um no de texto e "#texto"; o texto dos outros nos e vazio.
 * Nos valores, a contrabarra, o tabulador e a linha nova sao escapados com
 * uma contrabarra ("\\", "\\t", "\\n", "\\r"). O texto vem em UTF-8.
 *
 * A lista substitui a anterior: o DOM e o da pagina que esta aberta. */
void qjs_definir_dom(qjs_ponte_t *p, const char *fonte, int n);

/* O clique num elemento do DOM: chama o tratador (o onclick e os ouvintes
 * de "click") com o evento (que tem type e target, e um preventDefault).
 * Devolve 1 se o evento foi cancelado: nesse caso o browser nao navega. */
int qjs_clique(qjs_ponte_t *p, int dom);

/* Os elementos que tem um tratador de clique, para o browser os marcar como
 * alvo do rato. Devolve quantos identificadores escreveu. */
int qjs_clique_ids(qjs_ponte_t *p, int *ids, int max);

/* ---- o bucle de eventos e os eventos (D488) ----
 *
 * qjs_tique corre uma volta: as tarefas vencidas (temporizadores), cada uma com
 * as suas microtarefas, ate ~12 ms, e depois os requestAnimationFrame (no
 * maximo um quadro cada 15 ms). Devolve quantos callbacks correram. qjs_espera
 * diz quantos ms faltam para a proxima coisa (-1: nada), para o navegador
 * dormir o justo. */
int qjs_espera(qjs_ponte_t *p);
int qjs_animando(qjs_ponte_t *p);      /* ha requestAnimationFrame pedidos */

/* Um evento da pessoa num no do DOM (dom = -1: o elemento com foco, ou o body).
 * tipo: click, dblclick, mousedown, mouseup, mousemove, contextmenu, keydown,
 * keypress, keyup, input, change, submit, focus, blur, scroll, resize, popstate,
 * hashchange. json: os dados ({"x":..,"y":..,"botao":..,"tecla":"a","shift":1,
 * "valor":"...","delta":-1}). Devolve 1 se o script cancelou a accao do
 * navegador (preventDefault), 0 se nao, -1 se nao ha JavaScript. */
int qjs_despachar(qjs_ponte_t *p, int dom, const char *tipo, const char *json);

/* A pagina tem ouvintes de movimento do rato / de teclado: o navegador so
 * manda esses eventos quando sao precisos. */
#define QJS_QUER_RATO  1
#define QJS_QUER_TECLA 2
int qjs_quer(qjs_ponte_t *p);

/* Chama uma funcao global do JavaScript como uma tarefa do bucle (com as
 * microtarefas depois). json: o argumento; se for um array, os seus elementos
 * sao os argumentos (ate 3). Devolve o resultado inteiro (-1 se falhou).
 * Para a rede (D487): uma descarga em segundo plano que acaba chama
 *   qjs_chamar(p, "__sagui_resolver", "[id, true, {...}]")
 * e a promessa que o JavaScript pediu com __sagui_pendente() fica resolvida. */
int qjs_chamar(qjs_ponte_t *p, const char *funcao, const char *json);

void qjs_destruir(qjs_ponte_t *p);

#endif
