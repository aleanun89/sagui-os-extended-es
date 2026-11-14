// SPDX-License-Identifier: GPL-3.0-only
/* sistema.c - puerta de entrada de las llamadas al sistema (int 0x80).
 *
 * El puerto 0x80 de la IDT acepta "int" que viene del anillo 3 (D007). El despachador lee
 * el número en EAX, llama a la función de la tabla con EBX, ECX, EDX, ESI, EDI y
 * devuelve el resultado en EAX. Después mira el estado del proceso:
 *   - la función devolvió SIS_REPETIR: el proceso se bloqueó; el EIP vuelve 2
 *     bytes (el tamaño de "int 0x80") y la llamada se ejecuta de nuevo al despertar;
 *   - el proceso se bloqueó con resultado listo (ej.: dormir): graba el
 *     resultado y cede el turno;
 *   - el proceso terminó (salir, o fue terminado): cede el turno.
 * Ver D010. */
#include "sistema.h"
#include "console.h"
#include "cpu.h"
#include "paginacao.h"
#include "processo.h"
#include "texto.h"
#include "som.h"
#include "audio.h"
#include "cpu_dados.h"
#include "planificador.h"
#include "wifi.h"
#include "bloqueo.h"
#include "permisos.h"

static const chamada_t tabela[SIS_MAX] = {
    [SIS_EXECUTAR] = sis_executar,       [SIS_ESPERAR] = sis_esperar,
    [SIS_SAIR] = sis_sair,               [SIS_PID] = sis_pid,
    [SIS_MATAR] = sis_matar,
    [SIS_ABRIR] = sis_abrir,             [SIS_LER] = sis_ler,
    [SIS_ESCREVER] = sis_escrever,       [SIS_FECHAR] = sis_fechar,
    [SIS_POSICIONAR] = sis_posicionar,   [SIS_INFO_ARQUIVO] = sis_info_arquivo,
    [SIS_CRIAR_PASTA] = sis_criar_pasta, [SIS_REMOVER_PASTA] = sis_remover_pasta,
    [SIS_APAGAR] = sis_apagar,           [SIS_LISTAR] = sis_listar,
    [SIS_MUDAR_PASTA] = sis_mudar_pasta, [SIS_PASTA_ATUAL] = sis_pasta_atual,
    [SIS_RENOMEAR] = sis_renomear,
    [SIS_TRUNCAR] = sis_truncar,         [SIS_SINCRONIZAR] = sis_sincronizar,   /* D116 */
    [SIS_PIPE] = sis_pipe,               [SIS_DUPLICAR] = sis_duplicar,
    [SIS_DISPONIVEL] = sis_disponivel,
    [SIS_DORMIR] = sis_dormir,           [SIS_TEMPO] = sis_tempo,
    [SIS_DATA] = sis_data,               [SIS_INFO_SISTEMA] = sis_info_sistema,
    [SIS_AUMENTAR_MEM] = sis_aumentar_memoria, [SIS_REINICIAR] = sis_reiniciar,
    [SIS_DESLIGAR] = sis_desligar,       [SIS_ACPI] = sis_acpi,
    [SIS_REGISTRAR] = sis_registrar,
    [SIS_MODO_TERMINAL] = sis_modo_terminal,   [SIS_LIMPAR_TELA] = sis_limpar_tela,
    [SIS_CRIAR_JANELA] = sis_criar_janela,     [SIS_FECHAR_JANELA] = sis_fechar_janela,
    [SIS_ATUALIZAR_JANELA] = sis_atualizar_janela, [SIS_PROXIMO_EVENTO] = sis_proximo_evento,
    [SIS_TITULO_JANELA] = sis_titulo_janela,
    [SIS_LISTAR_DISCOS] = sis_listar_discos,   [SIS_MONTAR] = sis_montar,
    [SIS_DESMONTAR] = sis_desmontar,
    [SIS_BIP] = sis_bip,
    [SIS_TECLAS_SEGURADAS] = sis_teclas_seguradas,
    [SIS_TECLADO] = sis_teclado,
    [SIS_ABRIR_TV] = sis_abrir_tv,       [SIS_PRIMEIRO_PLANO] = sis_primeiro_plano,
    [SIS_ESTILO_JANELA] = sis_estilo_janela, [SIS_REDIMENSIONAR_JANELA] = sis_redimensionar_janela,
    [SIS_PEDIR_FECHAR] = sis_pedir_fechar,
    [SIS_MOVER_JANELA] = sis_mover_janela,   /* D102 */
    [SIS_FREQUENCIA] = sis_frequencia,       /* D106 */
    [SIS_FUNDO] = sis_fundo, [SIS_ACENTO] = sis_acento,   /* D112 */
    [SIS_MENU_CONTEXTUAL] = sis_menu_contextual,
    [SIS_MODOS_VIDEO] = sis_modos_video, [SIS_MUDAR_MODO] = sis_mudar_modo,
    [SIS_GRUPO] = sis_grupo,             [SIS_INTERRUPCOES] = sis_interrupcoes,
    [SIS_TIPO_DESCRITOR] = sis_tipo_descritor,
    [SIS_ALEATORIO] = sis_aleatorio,     [SIS_INFO_ALEATORIO] = sis_info_aleatorio,
    [SIS_COPIAR_TEXTO] = sis_copiar_texto, [SIS_COLAR_TEXTO] = sis_colar_texto,   /* D074 */
    /* Red (D046) */
    [SIS_SOCKET] = sis_socket,           [SIS_CONECTAR] = sis_conectar,
    [SIS_ESCUTAR] = sis_escutar,         [SIS_ACEITAR] = sis_aceitar,
    [SIS_ENVIAR_PARA] = sis_enviar_para, [SIS_RECEBER_DE] = sis_receber_de,
    [SIS_RESOLVER] = sis_resolver,       [SIS_CONFIG_REDE] = sis_config_rede,
    [SIS_PING] = sis_ping,               [SIS_OPCAO_SOCKET] = sis_opcao_socket,
    [SIS_ESPERAR_REDE] = sis_esperar_rede,   /* D117 (lite-monkey) */
    /* Hora y zona (D080) */
    [SIS_HORA] = sis_hora,               [SIS_ZONA] = sis_zona,
    [SIS_DATAS_ARQUIVO] = sis_datas_arquivo,
    /* Audio (D088) */
    [SIS_AUDIO_ABRIR] = sis_audio_abrir,       [SIS_AUDIO_ESCREVER] = sis_audio_escrever,
    [SIS_AUDIO_CONTROLE] = sis_audio_controle, [SIS_AUDIO_FECHAR] = sis_audio_fechar,
    [SIS_CAPTURAR_TELA] = sis_capturar_tela, [SIS_AVISAR] = sis_avisar,
    [SIS_COPIAR_IMAGEM] = sis_copiar_imagem, [SIS_COLAR_IMAGEM] = sis_colar_imagem,   /* D125 */   /* D125 */
    [SIS_AUDIO_HDA] = sis_audio_hda,     /* D107 */
    /* Os BAR de um dispositivo PCI no espaço do programa (D158) */
    [SIS_MAPEAR_BAR] = sis_mapear_bar,   [SIS_DESEJAR_BAR] = sis_desejar_bar,
    /* Hilos de usuario (D167) */
    [SIS_HILO_CRIAR] = sis_hilo_criar,   [SIS_HILO_SAIR] = sis_hilo_sair,
    [SIS_HILO_ESPERAR] = sis_hilo_esperar, [SIS_CEDER] = sis_ceder,
    [SIS_FUTEX_ESPERAR] = sis_futex_esperar, [SIS_FUTEX_ACORDAR] = sis_futex_acordar,
    [SIS_HILO_ID] = sis_hilo_id,
    /* Los planificadores (D174) */
    [SIS_PLANIFICADOR] = sis_planificador,
    /* Acceso en bruto a los discos: particiones e instalador (D179) */
    [SIS_DISCO_BLOCOS] = sis_disco_blocos,
    /* Páginas del área de datos: el malloc devuelve memoria (D209) */
    [SIS_MEMORIA] = sis_memoria,
    /* Wi-Fi (D215) */
    [SIS_WIFI] = sis_wifi,
    /* Bloqueo de publicidad por DNS (D218) */
    [SIS_BLOQUEO] = sis_bloqueo,
    /* El gestor de servicios (D216) */
    [SIS_SERVICOS] = sis_servicos,
    /* Entrada inyectada para el escritorio remoto (D251) */
    [SIS_INYECTAR] = sis_inyectar,
    /* Usuarios (D255) */
    [SIS_USUARIO] = sis_usuario,
};

bool sistema_tem_chamada(uint32_t n)
{
    return n < SIS_MAX && tabela[n];
}

bool usuario_ok(const void *p, uint32_t n, bool escrita)
{
    uint32_t ini = (uint32_t)p;
    if (n == 0) return true;
    if (ini < USUARIO_INICIO || ini >= USUARIO_FIM || n > USUARIO_FIM - ini)
        return false;
    for (uint32_t pag = ini & ~0xFFFu; pag < ini + n; pag += 4096) {
        uint32_t flags;
        /* D186: una página de pila aún no tocada se asigna ahora, para que
         * el kernel no falle al copiar en ella. */
        if (!espaco_traduzir(processo_atual()->espaco, pag, &flags) &&
            !(pilha_crescer(processo_atual(), pag) && espaco_traduzir(processo_atual()->espaco, pag, &flags)))
            return false;
        if (!(flags & PG_USUARIO)) return false;
        if (escrita && !(flags & PG_ESCRITA)) return false;
    }
    return true;
}

int usuario_texto(const char *origem, char *destino, uint32_t max)
{
    for (uint32_t i = 0; i < max; i++) {
        /* Comprueba cada página al entrar en ella (el texto puede cruzar páginas). */
        if ((i == 0 || (((uint32_t)origem + i) & 0xFFF) == 0) && !usuario_ok(origem + i, 1, false))
            return ERRO_ENDERECO;
        destino[i] = origem[i];
        if (!origem[i]) return (int)i;
    }
    destino[max - 1] = 0;
    return ERRO_INVALIDO;   /* demasiado largo */
}

static quadro_t *tratar_sistema(quadro_t *q)
{
    uint32_t n = q->eax;
    if (n >= SIS_MAX || !tabela[n]) {
        q->eax = (uint32_t)ERRO_INVALIDO;
        return q;
    }
    /* D255: quien no es root solo puede lo que permisos.c dice que es libre. */
    int32_t r = permisos_chamada(n, q->ebx, q->ecx, q->edx, q->esi, q->edi);
    if (r < 0) {
        q->eax = (uint32_t)r;
        return q;
    }
    r = tabela[n](q->ebx, q->ecx, q->edx, q->esi, q->edi);
    if (r == SIS_REPETIR) {
        q->eip -= 2;               /* ejecuta el "int 0x80" de nuevo al despertar */
        return escalonar(q);
    }
    q->eax = (uint32_t)r;
    if (processo_atual()->estado != P_EXECUTANDO)   /* se bloqueó con resultado listo, o terminó */
        return escalonar(q);
    /* D174: la llamada despertó a alguien que debe correr antes que este
     * (planificador "prioridad"), o le cambió la máscara de CPU. */
    if (planificador_troca_pendente(cpu_atual()))
        return escalonar(q);
    return q;
}

void sistema_iniciar(void)
{
    idt_registrar(VETOR_SISTEMA, tratar_sistema);
}
