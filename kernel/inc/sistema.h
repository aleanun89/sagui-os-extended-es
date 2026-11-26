// SPDX-License-Identifier: GPL-3.0-only
/* sistema.h - chamadas de sistema (int 0x80). */
#ifndef SAGUI_SISTEMA_H
#define SAGUI_SISTEMA_H
#include "tipos.h"
#include "abi.h"

/* Valor especial devolvido por uma chamada que bloqueou e deve ser
 * executada de novo quando o processo acordar (D010). */
#define SIS_REPETIR ((int32_t)0x80000000)

typedef int32_t (*chamada_t)(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);

void sistema_iniciar(void);
bool sistema_tem_chamada(uint32_t n);   /* D255: para la autoprueba de permisos.c */

/* Validação de ponteiros vindos do anel 3: o intervalo inteiro precisa estar
 * na área dos programas e mapeado para o usuário (e gravável, se for escrita). */
bool usuario_ok(const void *p, uint32_t n, bool escrita);
/* Copia um texto do programa para o kernel; devolve o tamanho ou um erro. */
int  usuario_texto(const char *origem, char *destino, uint32_t max);

/* Implementadas em vários arquivos: */
int32_t sis_sair(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* D401: procesos POSIX */
int32_t sis_bifurcar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_substituir(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_esperar_pid(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_sinal(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_sondar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_descritor(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_pid(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_matar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_executar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_esperar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_dormir(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_tempo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_data(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_info_sistema(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_aumentar_memoria(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_reiniciar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_desligar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_acpi(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_registrar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_abrir(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_ler(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_escrever(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_fechar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_truncar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_sincronizar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_posicionar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_info_arquivo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_criar_pasta(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_remover_pasta(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_apagar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_listar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_mudar_pasta(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_pasta_atual(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_renomear(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_pipe(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_duplicar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_disponivel(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_modo_terminal(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_limpar_tela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_criar_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_fechar_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_atualizar_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_proximo_evento(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_titulo_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_listar_discos(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_montar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_desmontar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_teclas_seguradas(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_teclado(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_abrir_tv(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_primeiro_plano(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_estilo_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_acento(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D112 */
int32_t sis_cursor(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D482 */
int32_t sis_esquema(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D428 */
int32_t sis_ponte(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);     /* D485: unidades de red (ponte.c) */
int32_t sis_chaveiro(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);  /* D485: el llavero (chaveiro.c) */
void    ponte_revisar(void);                                         /* D485: las unidades sin servidor */
int     ponte_bandeja(uint32_t uid, int *sem_rede, char *nome, uint32_t max);   /* D485: la bandeja */
void    chaveiro_esquecer_menos(uint32_t uid);                       /* D485: al cambiar de sesión */
int32_t sis_idioma(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D313 */
int32_t sis_criar_widget(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D365 */
int32_t sis_fundo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t aparencia_info(uint32_t ptr, uint32_t n);
int32_t sis_mover_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_frequencia(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D106 */
int32_t sis_redimensionar_janela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_pedir_fechar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_menu_contextual(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D051 */
int32_t sis_modos_video(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D058/D059 */
int32_t sis_mudar_modo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_grupo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_interrupcoes(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_tipo_descritor(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* Área de transferência (D074): kernel/nucleo/portapapeis.c */
int32_t sis_copiar_texto(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_colar_texto(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* Aleatoriedade (D061): kernel/nucleo/aleatorio.c */
int32_t sis_aleatorio(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_info_aleatorio(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* Rede (D046): kernel/rede/socket.c e rede.c */
int32_t sis_socket(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_conectar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_escutar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_aceitar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_enviar_para(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_receber_de(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_resolver(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_config_rede(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_ping(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_opcao_socket(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_copiar_imagem(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_colar_imagem(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_avisar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_capturar_tela(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D125 */
int32_t sis_esperar_rede(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D117 */
/* Hora e zona (D080): kernel/nucleo/hora.c e kernel/fs/arquivos.c */
int32_t sis_hora(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_zona(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_datas_arquivo(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* Os BAR de um dispositivo PCI no espaço do programa (D158): kernel/dispositivos/bar.c */
int32_t sis_mapear_bar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
/* Hilos de usuario (D167), en kernel/nucleo/hilos.c */
int32_t sis_hilo_criar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_hilo_sair(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_hilo_esperar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_ceder(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_futex_esperar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_futex_acordar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_hilo_id(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_planificador(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D174 */
int32_t sis_disco_blocos(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D179 */
int32_t sis_memoria(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D209 */
int32_t sis_servicos(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D216 */
int32_t sis_inyectar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D251 */
int32_t sis_usuario(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);    /* D255 */
int32_t sis_permisos_arq(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);   /* D282 */
int32_t sis_desejar_bar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#endif
