// SPDX-License-Identifier: GPL-3.0-only
/* rede.h - placas de rede, pilha IPv4 (ARP, IP, ICMP, UDP, DHCP, DNS, TCP)
 * e sockets. Ver D046.
 *
 * Regra de concorrência: todo o código da rede roda com as interrupções
 * desligadas (nas chamadas de sistema, que já entram assim; na interrupção
 * da placa; e na tarefa "rede", que desliga antes de mexer em qualquer
 * coisa). Não há travas: nunca duas partes mexem no mesmo estado ao mesmo
 * tempo, e nenhuma função daqui religa as interrupções no meio. */
#ifndef SAGUI_REDE_H
#define SAGUI_REDE_H
#include "tipos.h"
#include "abi.h"
#include "pci.h"

#define ETH_MAX   1514   /* quadro Ethernet sem o CRC */
#define ETH_MIN   60

/* ------------------------------------------------------------ placas */

/* O que um driver entrega para a pilha. Uma placa por vez (a primeira
 * suportada que aparecer no PCI). */
typedef struct placa {
    const char *nome;          /* driver: "e1000", "rtl8139", "ne2000", "rtl810x" */
    const char *descricao;     /* "Intel 82540EM (PRO/1000 MT)" */
    uint8_t     mac[6];
    pci_disp_t  pci;
    int         irq;           /* linha do PIC; -1 = só sondagem */
    bool        experimental;
    bool (*enviar)(const void *quadro, uint32_t n);   /* false = fila de envio cheia */
    int  (*receber)(void *destino);   /* bytes; 0 = nada chegou; <0 = quadro com erro (descartado) */
    bool (*cabo)(void);               /* enlace ativo */
    bool (*atender)(void);            /* na IRQ: lê e limpa a causa; false = não era desta placa */
    /* D211: pide una interrupción de la tarjeta (autoprueba de la IRQ); NULL = no sabe. */
    bool (*provocar_irq)(void);
    /* D091: texto de diagnóstico do driver para o "ip" (NULL = não tem). */
    void (*diagnostico)(char *s, uint32_t max);
    /* D124: ajustes en caliente (REDE_AJUSTAR_DRIVER); NULL = no tiene. 0 o ERRO_*. */
    int32_t (*ajustar)(const ajuste_driver_t *a);
    /* estatísticas (a pilha conta) */
    uint32_t rx_pacotes, tx_pacotes, rx_bytes, tx_bytes;
    uint32_t rx_erros, tx_erros, descartados, irqs;
} placa_t;

bool e1000_procurar(placa_t *p);
bool rtl8139_procurar(placa_t *p);
/* D329: NE2000 (DP8390) en PCI (RTL8029 y clones) y en ISA (0x300/0x280/0x320/0x340, solo si no
 * hay otra tarjeta; apagado por defecto, se activa con mkimagem.py --ne2k-isa; IRQ con --ne2k-irq o autodetectada). */
bool ne2000_procurar(placa_t *p);
bool ne2000_isa_procurar(placa_t *p);
/* Família RTL8101E/8102E/8105E/8168 (r8169): sem como testar, só ativa a
 * pedido (ifconfig eth0 experimental). Com ativar=false só detecta. */
bool r8169_procurar(placa_t *p, bool ativar);
/* Atheros AR8131/8132/8151/8152 (alc, D200): igual, experimental: con
 * ativar=false solo detecta (y lee la MAC que dejó la BIOS). */
bool alc_procurar(placa_t *p, bool ativar);

/* D215: la interfaz wlan0 (pincho Wi-Fi, kernel/rede/wifi.c). La pila tiene
 * una interfaz activa: mientras el Wi-Fi está conectado, la wlan0 ocupa su
 * lugar y la eth0 queda aparcada (sus quadros se descartan); al desconectar,
 * vuelve la eth0. registrar(NULL) = el pincho se retiró. */
void rede_wifi_registrar(placa_t *p);
void rede_wifi_conectada(bool sim);
/* D238: la wlan0 dejó tramas para la pila: la tarea de la red no espera al tic. */
void rede_acordar(void);

/* ------------------------------------------------------------ pilha (rede.c) */

void        rede_iniciar(void);        /* procura a placa, liga a tarefa e o DHCP */
placa_t    *rede_placa(void);          /* NULL = sem placa */
bool        rede_pronta(void);         /* tem placa e IP */
uint32_t    rede_meu_ip(void);
/* Loopback (D079): 127.0.0.0/8 e o próprio endereço não saem pela placa;
 * os pacotes dão a volta numa fila e entram de novo pela tarefa da rede.
 * O endereço de origem de um pacote para 127.x é 127.0.0.1. */
uint32_t    rede_origem_para(uint32_t destino);
bool        rede_eh_local(uint32_t ip);
/* D363: el túnel wg0 (tunel.c). */
uint32_t    rede_mss_para(uint32_t destino);   /* MSS del TCP según la interfaz de salida */
void        rede_tunel_chegou(const uint8_t *p, uint32_t n);
void        rede_dns_limpar(void);
extern bool rede_forcar_fisica;                /* socket.c, mientras manda un datagrama OPC_FISICA */
void        rede_linha_boot(char *s, int max, bool *aviso);   /* "[ ok ] Red: ..." (sem o prefixo) */
bool        rede_dhcp_terminou(void);  /* para o boot esperar um pouco */
uint32_t    rede_tsc_khz(void);
/* Bandeja de la barra (D114): resumen de eth0 y renovación del DHCP sin esperar. */
bool        rede_bandeja(info_rede_t *v);   /* true = hay tarjeta (soportada o no) */
void        rede_renovar_dhcp(void);

uint8_t *rede_carga(void);             /* onde montar o conteúdo do próximo pacote IP */
/* Manda o pacote montado em rede_carga() (n bytes). false = sem rota/placa. */
bool     ip_enviar(uint32_t destino, uint8_t protocolo, uint32_t n);
bool     udp_enviar(uint32_t destino, uint16_t porta_origem, uint16_t porta_destino, uint32_t n);   /* dados em rede_carga()+8 */
uint32_t soma_parcial(uint32_t s, const uint8_t *p, uint32_t n);
uint16_t soma_final(uint32_t s);
uint16_t soma_pseudo(uint32_t origem, uint32_t destino, uint8_t proto, const uint8_t *p, uint32_t n);

#define IP_ICMP  1
#define IP_TCP   6
#define IP_UDP   17
#define IP_TODOS 0xFFFFFFFFu

/* Números em ordem de rede (big-endian) dentro dos pacotes. */
static inline uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static inline void poe16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static inline void poe32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* ------------------------------------------------------------ TCP (tcp.c) */

typedef struct tcp_conexao tcp_t;
void    tcp_receber(uint32_t origem, uint32_t destino, const uint8_t *seg, uint32_t n);
void    tcp_tique(void);
tcp_t  *tcp_abrir(uint32_t ip, uint16_t porta, int32_t *erro);           /* manda o SYN */
tcp_t  *tcp_nova_escuta(uint16_t porta, int fila, int32_t *erro);
int32_t tcp_situacao(tcp_t *c);       /* 0 conectada, 1 conectando, <0 erro */
tcp_t  *tcp_tirar_da_fila(tcp_t *escuta);   /* NULL = ninguém esperando */
/* ler/escrever devolvem SIS_REPETIR quando teriam de esperar (canal: a conexão) */
int32_t tcp_ler(tcp_t *c, char *destino, uint32_t n);
int32_t tcp_escrever(tcp_t *c, const char *origem, uint32_t n);
int32_t tcp_disponivel(tcp_t *c);
void    rede_atividade(void);          /* socket.c: algo llegó a algún socket (D117) */
void    tcp_fechar_envio(tcp_t *c);
void    tcp_soltar(tcp_t *c);         /* o socket fechou */
void    tcp_enderecos(tcp_t *c, uint32_t *ip_l, uint16_t *p_l, uint32_t *ip_r, uint16_t *p_r);
uint32_t tcp_estado(tcp_t *c);        /* CON_* */
int     tcp_listar(void *v, int max); /* INFO_CONEXOES */
uint16_t tcp_porta_livre(void);
int     tcp_listar_detalhado(info_socket_t *o, int max);   /* INFO_SOCKETS (D079) */
int32_t tcp_abortar(uint32_t id);                         /* REDE_FECHAR */

/* ------------------------------------------------------------ sockets (socket.c) */

typedef struct socket socket_t;
void    udp_chegou_socket(uint32_t origem, uint32_t destino, uint16_t po, uint16_t pd, const uint8_t *d, uint32_t n);
int     udp_listar(void *v, int max);
int32_t socket_ler(socket_t *s, char *buf, uint32_t n);
int32_t socket_escrever(socket_t *s, const char *buf, uint32_t n);
int32_t socket_disponivel(socket_t *s);
void    socket_fechar(socket_t *s);
void    socket_nao_bloquear(socket_t *s, bool sim);   /* D401: O_NONBLOCK */
uint32_t socket_sondar(socket_t *s);                  /* D401: SONDA_* para poll */
int32_t rede_info(uint32_t tipo, uint32_t ptr, uint32_t n);

/* arquivos.c: um descritor novo para um socket (ou erro). */
int arquivos_novo_socket(socket_t *s);
socket_t *arquivos_socket(uint32_t fd);

#endif
