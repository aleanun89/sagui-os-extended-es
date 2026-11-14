/* sagui_mp.h - o que os arquivos do MicroPython no SaguiOS usam da libsagui
 * e uns dos outros.
 *
 * Não dá para incluir o sagui.h junto com os cabeçalhos do MicroPython (os
 * dois definem bool, size_t e os uintN_t de jeitos diferentes; ver
 * libc_sagui.c). Por isso os protótipos da libsagui que o pegamento usa são
 * repetidos aqui só com tipos básicos do C, e as estruturas do abi.h que
 * atravessam a fronteira (info_arquivo_t, entrada_pasta_t, data_t,
 * endereco_rede_t) vêm do próprio abi.h, que não depende de nada.
 *
 * O que precisa da libtls (TLS, SHA-384/512) passa por ponte_sagui.c, com a
 * interface em ponte_sagui.h. */
#ifndef SAGUI_MP_H
#define SAGUI_MP_H

#include <stddef.h>
#include <stdint.h>
#include "abi.h"
#include "py/obj.h"
#include "py/mpthread.h"

/* Una llamada a la libsagui que puede esperar mucho (red, entrada, otro
 * proceso), hecha sin el GIL (D170): mientras espera corren los demás hilos
 * de Python. Solo para llamadas que no crean ni tocan objetos de Python. */
#define SEM_GIL(chamada) ({ MP_THREAD_GIL_EXIT(); __typeof__(chamada) _r_sem_gil = (chamada); \
                            MP_THREAD_GIL_ENTER(); _r_sem_gil; })

/* ---- libsagui ---- */
int   ler(int fd, void *buf, unsigned n);
int   escrever(int fd, const void *buf, unsigned n);
int   escrever_tudo(int fd, const void *buf, unsigned n);
int   abrir(const char *caminho, int modo);
int   fechar(int fd);
int   posicionar(int fd, int desloc, int origem);
int   info_arquivo(const char *caminho, info_arquivo_t *info);
int   criar_pasta(const char *caminho);
int   remover_pasta(const char *caminho);
int   apagar(const char *caminho);
int   listar(const char *caminho, int indice, entrada_pasta_t *e);
int   mudar_pasta(const char *caminho);
int   pasta_atual(char *buf, unsigned n);
int   renomear(const char *origem, const char *destino);
int   tipo_descritor(int fd);
int   disponivel(int fd);
int   executar_env(const char *caminho, char *const argv[], char *const envp[]);
int   esperar(int pid, int *codigo);
int   primeiro_plano(int grupo);
int   definir_grupo(int pid, int grupo);
void *aloca(size_t n);
void *realoca(void *p, size_t n);
void  libera(void *p);
unsigned tempo_ms(void);
int   dormir(unsigned ms);
int   ler_data(data_t *d);
int   bytes_aleatorios(void *buf, unsigned n);
int   ler_linha(int fd, char *buf, unsigned max);
const char *nome_erro(int erro);
const char *obter_env(const char *nome);
void  sair(int codigo) __attribute__((noreturn));
extern char **environ;
/* hilos (D167; los usa mpthreadport.c, D170) */
int   hilo_criar(void (*entrada)(void), void *pilha_topo);
void  hilo_sair(int codigo) __attribute__((noreturn));
int   hilo_esperar(int tid, int *codigo);
int   thread_id(void);
int   futex_esperar(volatile uint32_t *palavra, uint32_t valor, unsigned ms);
int   futex_acordar(volatile uint32_t *palavra, int quantos);
/* rede (D046) */
int   criar_socket(int tipo);
int   conectar(int fd, uint32_t ip, int porta);
int   escutar(int fd, int porta, int fila);
int   aceitar_conexao(int fd, endereco_rede_t *de);
int   enviar_para(int fd, const void *buf, unsigned n, uint32_t ip, int porta);
int   receber_de(int fd, void *buf, unsigned n, endereco_rede_t *de);
int   opcao_socket(int fd, int opcao, unsigned valor);
int   tempo_limite(int fd, unsigned ms);
int   resolver(const char *nome, uint32_t *ip);
int   config_rede(int op, config_rede_t *c);

/* ---- texto: o terminal e os arquivos do SaguiOS são Latin-1; o str do
 * MicroPython é UTF-8 (D068) ---- */
/* Latin-1 -> UTF-8 num vstr (acrescenta). */
void sagui_latin1_para_vstr(vstr_t *v, const char *s, size_t n);
/* str do MicroPython (UTF-8) -> texto Latin-1 terminado em zero, em buf (até
 * max-1 bytes). Caracteres fora do Latin-1 viram '?'. Devolve o tamanho; se
 * não coube, levanta OSError (para caminhos: ENAMETOOLONG não existe aqui). */
size_t sagui_str_para_latin1(mp_obj_t s, char *buf, size_t max);
/* Texto Latin-1 do sistema (mensagens, nomes de arquivo) -> str. */
mp_obj_t sagui_str_de_latin1(const char *s);
/* Texto que pode vir em UTF-8 ou em Latin-1 (fonte .py, argumentos): se já é
 * UTF-8 válido fica igual; se não, é tratado como Latin-1. Devolve um
 * buffer do coletor (m_new) e o tamanho em *n (e a capacidade em *cap). */
char *sagui_para_utf8(const char *s, size_t n, size_t *tam, size_t *cap);

/* ---- erros ---- */
int  sagui_errno(int erro_sagui);   /* ERRO_* do abi.h -> MP_E* */
/* Levanta OSError(errno, "mensagem em espanhol[: contexto]"). */
NORETURN void sagui_levantar(int erro_sagui, const char *contexto);
NORETURN void sagui_levantar_obj(int erro_sagui, mp_obj_t contexto);

/* ---- arquivos (arquivos_sagui.c) ---- */
extern const mp_obj_type_t sagui_tipo_texto;     /* TextIOWrapper */
extern const mp_obj_type_t sagui_tipo_binario;   /* FileIO */
/* Saída padrão do MicroPython (mp_hal_stdout_tx_strn): o mesmo conversor
 * de sys.stdout, para não misturar pedaços de um caractere UTF-8. */
void sagui_saida_texto(int fd, const char *s, size_t n);
/* Um byte da entrada padrão já em UTF-8 (input(), REPL); -1 no fim. */
int  sagui_entrada_byte(void);
const mp_print_t *sagui_print_erros(void);
void sagui_iniciar_padrao(void);   /* olha o que é a entrada padrão (antes de ler) */

/* Um objeto de arquivo sobre um descritor que não é nosso (socket.makefile). */
mp_obj_t sagui_arquivo_de_fd(int fd, bool texto);

/* ---- rede (rede_sagui.c) ---- */
extern const mp_obj_type_t sagui_tipo_timeout;   /* socket.timeout */
NORETURN void sagui_levantar_rede(int erro_sagui);   /* ERRO_TEMPO vira socket.timeout */
int  sagui_socket_fd(mp_obj_t sock);
bool sagui_socket_bloqueante(mp_obj_t sock);
void sagui_socket_fechar(mp_obj_t sock);

/* ---- sistema ---- */
/* Ambiente atual (os.environ) como vetor "NOME=valor" para executar_env. */
char **sagui_ambiente(void);
uint32_t sagui_aleatorio32(void);

#endif
