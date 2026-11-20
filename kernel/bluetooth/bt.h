// SPDX-License-Identifier: GPL-2.0-or-later
/* bt.h - pila Bluetooth clásico (BR/EDR) de SaguiOS (D279, mejora 059, v8.2).
 *
 * Escrita desde las especificaciones públicas (Bluetooth Core, HID Profile 1.1, SDP,
 * Assigned Numbers). Nada de BlueZ ni de Linux. Capas:
 *   bt_hci.c     órdenes y eventos HCI, flujo de ACL, inicio del controlador, claves de enlace
 *   bt_l2cap.c   señalización L2CAP, canales, reensamblado
 *   bt_sdp.c     cliente SDP (busca el servicio HID del dispositivo)
 *   bt_hid.c     HIDP sobre L2CAP (PSM 0x11 control y 0x13 interrupción)
 *   bt_gestor.c  tabla de dispositivos, flujo conectar/emparejar, claves guardadas
 * La pila no sabe nada del USB ni del resto del kernel: todo lo de fuera entra por bt_plat_t,
 * así se compila también en el PC (tests/bt_unidad.c con un controlador simulado). No hay
 * asignación dinámica ni cerrojos: una sola tarea llama a bt_hci_evento/bt_hci_acl/bt_tic y a
 * las operaciones del gestor (el kernel las serializa con una trava). */
#ifndef SAGUI_BT_H
#define SAGUI_BT_H

#ifdef BT_ANFITRION
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#else
#include "tipos.h"
#include "texto.h"
#endif

/* ------------------------------------------------------------------ límites */
#define BT_MAX_DISP      8     /* dispositivos conocidos (emparejados y vistos) */
#define BT_MAX_LIGACOES  4     /* conexiones ACL a la vez */
#define BT_MAX_CANAIS    10    /* canales L2CAP */
#define BT_ACL_FILA      16    /* paquetes ACL esperando crédito del controlador */
#define BT_ACL_FILA_TAM  96    /* bytes de cada uno (todo lo que mandamos es corto) */
#define BT_CMD_FILA      16
#define BT_CMD_PAR       255   /* parámetros de una orden (los registros .hcd y los fragmentos Realtek pasan de 248) */
#define BT_SDP_MAX       1024  /* bytes de la respuesta SDP completa */
#define BT_DESC_MAX      512   /* descriptor de informe HID guardado */
#define BT_NOME_MAX      32
#define BT_CONF_MAX      2048  /* bytes del archivo de configuración */

/* ------------------------------------------------------------------ plataforma */
typedef struct {
    int      (*enviar_cmd)(const uint8_t *p, int n);   /* orden HCI: opcode(2) len(1) params; 0 o <0 */
    int      (*enviar_acl)(const uint8_t *p, int n);   /* paquete ACL: manejador+flags(2) len(2) datos */
    uint32_t (*ms)(void);                              /* milisegundos monótonos */
    void     (*guardar)(const char *texto, int n);     /* el archivo de configuración (claves incluidas) */
    /* Un dispositivo HID quedó listo: tipo BT_T_*, y su descriptor de informe (puede faltar). Devuelve 1 si un
     * ratón se entiende con ese descriptor en protocolo normal (entonces no se pide el protocolo boot). */
    int      (*hid_conectar)(int tipo, const uint8_t *desc, int n);
    void     (*teclado)(const uint8_t *r, int n);      /* informe de teclado, formato boot (8 bytes) */
    void     (*raton)(const uint8_t *r, int n, bool boot);   /* informe de ratón: boot (3 o 4 bytes) o el del descriptor */
    void     (*soltar_entrada)(int tipo);              /* un dispositivo HID se fue: suelta teclas */
    void     (*registro)(const char *texto);           /* línea de registro (dmesg) */
    /* D411: firmware de los controladores que lo piden. NULL = no hay (el inicio sigue sin parche). */
    uint16_t usb_vid, usb_pid;                         /* identificador USB del controlador (0 = se desconoce) */
    int      (*firmware)(const char *nome, const uint8_t **dados, int *n);   /* /lib/firmware/<nome>; 0 o <0 */
    void     (*firmware_liberar)(const uint8_t *dados);
    /* Atheros (ath3k): petición de control de fabricante (tipo con el bit de dirección) y bulk de salida. Bloquean. */
    int      (*usb_vendor)(uint8_t tipo, uint8_t pedido, uint16_t valor, uint16_t indice, void *d, int n);
    int      (*usb_bulk_salida)(const void *d, int n);
} bt_plat_t;

/* ------------------------------------------------------------------ estado del adaptador */
#define BT_SEM_ADAPTADOR 0
#define BT_INICIANDO     1
#define BT_PRONTO        2
#define BT_FALHOU        3
#define BT_APAGADO       4

/* Estado de un dispositivo en el flujo de conexión. */
#define BT_D_LIVRE       0   /* sin enlace */
#define BT_D_LIGANDO     1   /* Create_Connection en marcha */
#define BT_D_AUTENTICANDO 2  /* emparejamiento / autenticación */
#define BT_D_CIFRANDO    3
#define BT_D_SDP         4   /* canal SDP y consulta */
#define BT_D_HID         5   /* abriendo los canales HID */
#define BT_D_CONECTADO   6
#define BT_D_ERRO        7   /* el último intento falló (ver erro) */

/* Tipo de dispositivo (por la clase o el SDP). */
#define BT_T_OUTRO    0
#define BT_T_TECLADO  1
#define BT_T_RATO     2
#define BT_T_COMBO    3
#define BT_T_BLE      4   /* baja energía (BLE): solo se detecta (D411) */
#define BT_T_BLE_HID  5   /* BLE con apariencia o servicio HID (HOGP): se detecta, aún no se conecta */

/* Modo de la confirmación pendiente. */
#define BT_CONF_NADA     0
#define BT_CONF_COMPARAR 1   /* ¿coincide el número con el del dispositivo? sí/no */
#define BT_CONF_MOSTRAR  2   /* escribir el número en el teclado del dispositivo (passkey) */

typedef struct {
    bool     usado;
    uint8_t  addr[6];          /* en orden HCI (el byte 0 es el menos significativo) */
    char     nome[BT_NOME_MAX];
    uint32_t clase;
    int8_t   rssi;
    bool     visto;            /* salió en la última búsqueda */
    bool     tem_chave;
    uint8_t  chave[16];
    uint8_t  tipo_chave;
    uint8_t  tipo;             /* BT_T_* */
    uint8_t  estado;           /* BT_D_* */
    bool     so_parear;        /* el flujo se detiene tras emparejar */
    bool     iniciado_aqui;    /* la conexión la empezamos nosotros */
    uint8_t  io_remoto;        /* capacidad de E/S que dijo el dispositivo */
    uint16_t manejador;        /* manejador ACL (0xFFFF = ninguno) */
    int8_t   canal_sdp, canal_ctl, canal_int;
    uint16_t psm_ctl, psm_int;
    uint8_t  subclasse_hid;
    bool     hid_ok;           /* el SDP dijo que es HID */
    bool     modo_boot;        /* el dispositivo está en protocolo boot (informes fijos) */
    bool     le;               /* salió en una búsqueda de baja energía */
    uint8_t  addr_tipo_le;     /* 0 pública, 1 aleatoria (de la última publicidad LE) */
    bool     cerrando;         /* lo cerramos nosotros */
    uint32_t desde;            /* ms: inicio del paso actual (tiempo límite) */
    char     erro[48];
} bt_disp_t;

typedef struct {
    uint32_t estado;           /* BT_* */
    uint8_t  addr[6];
    uint8_t  hci_ver, lmp_ver;
    uint16_t fabricante, hci_rev, lmp_sub;
    bool     buscando;
    bool     energia;
    uint8_t  conf_modo;        /* BT_CONF_* */
    uint32_t conf_num;
    int8_t   conf_disp;
    uint16_t acl_mtu;
    uint8_t  acl_num, acl_libres;
    uint32_t eventos, acl_rx, acl_tx, erros_hci;
    char     erro[64];
} bt_info_t;

/* ------------------------------------------------------------------ HCI (bt_hci.c) */
void bt_hci_iniciar(const bt_plat_t *plat);          /* arranque en frío: tablas a cero y Reset */
void bt_hci_evento(const uint8_t *p, int n);         /* evento HCI: code(1) len(1) params */
void bt_hci_acl_entrada(const uint8_t *p, int n);    /* paquete ACL recibido */
int  bt_hci_cmd(uint16_t op, const void *par, int n);/* encola una orden */
int  bt_hci_acl_enviar(uint16_t manejador, const uint8_t *dados, int n);   /* 0 o <0 */
void bt_hci_tic(void);                               /* tiempos límite y fila de órdenes */
bool bt_hci_listo(void);
void bt_hci_energia(bool e);
bool bt_hci_le(void);                                /* el controlador anuncia baja energía (LE) */                         /* conectable o no (y se recuerda) */
void bt_hci_info(bt_info_t *i);
const bt_plat_t *bt_plat(void);
void bt_log(const char *fmt, ...);
uint32_t bt_agora(void);

#define BT_OP(ogf, ocf)  ((uint16_t)(((ogf) << 10) | (ocf)))
#define OP_RESET         BT_OP(3, 0x03)
#define OP_SET_EVMASK    BT_OP(3, 0x01)
#define OP_WR_NOME       BT_OP(3, 0x13)
#define OP_WR_SCAN       BT_OP(3, 0x1A)
#define OP_WR_CLASE      BT_OP(3, 0x24)
#define OP_WR_INQMODE    BT_OP(3, 0x45)
#define OP_WR_SSP        BT_OP(3, 0x56)
#define OP_RD_NOME       BT_OP(3, 0x14)
#define OP_FW_BCM_MINI   BT_OP(0x3F, 0x2E)   /* Broadcom: Download_Minidriver */
#define OP_FW_RTL_DESCARGA BT_OP(0x3F, 0x20) /* Realtek: un fragmento del parche */
#define OP_FW_RTL_ROM    BT_OP(0x3F, 0x6D)   /* Realtek: leer la versión de la ROM */
#define OP_LE_PARAM_SCAN BT_OP(8, 0x0B)
#define OP_LE_SCAN       BT_OP(8, 0x0C)
#define OP_RD_VERSAO     BT_OP(4, 0x01)
#define OP_RD_BUFFER     BT_OP(4, 0x05)
#define OP_RD_FEATURES   BT_OP(4, 0x03)
#define OP_RD_ADDR       BT_OP(4, 0x09)
#define OP_INQUIRY       BT_OP(1, 0x01)
#define OP_INQUIRY_CANC  BT_OP(1, 0x02)
#define OP_CRIAR_CONEX   BT_OP(1, 0x05)
#define OP_DESCONECTAR   BT_OP(1, 0x06)
#define OP_CANCELAR_CONEX BT_OP(1, 0x08)
#define OP_ACEITAR_CONEX BT_OP(1, 0x09)
#define OP_REJEITAR_CONEX BT_OP(1, 0x0A)
#define OP_CHAVE_SIM     BT_OP(1, 0x0B)
#define OP_CHAVE_NAO     BT_OP(1, 0x0C)
#define OP_PIN_SIM       BT_OP(1, 0x0D)
#define OP_PIN_NAO       BT_OP(1, 0x0E)
#define OP_AUTENTICAR    BT_OP(1, 0x11)
#define OP_CIFRAR        BT_OP(1, 0x13)
#define OP_NOME_REMOTO   BT_OP(1, 0x19)
#define OP_IO_SIM        BT_OP(1, 0x2B)
#define OP_CONF_SIM      BT_OP(1, 0x2C)
#define OP_CONF_NAO      BT_OP(1, 0x2D)
#define OP_IO_NAO        BT_OP(1, 0x34)

/* Códigos de evento */
#define EV_INQ_FIM       0x01
#define EV_INQ_RES       0x02
#define EV_CONEXAO       0x03
#define EV_PEDIDO_CONEX  0x04
#define EV_DESCONEXAO    0x05
#define EV_AUTENTICACAO  0x06
#define EV_NOME_REMOTO   0x07
#define EV_CIFRADO       0x08
#define EV_CMD_COMPLETO  0x0E
#define EV_CMD_ESTADO    0x0F
#define EV_ERRO_HW       0x10
#define EV_PACOTES_COMP  0x13
#define EV_LE            0x3E
#define EV_PEDIDO_CHAVE  0x17
#define EV_NOVA_CHAVE    0x18
#define EV_PEDIDO_PIN    0x16
#define EV_INQ_RSSI      0x22
#define EV_INQ_EXT       0x2F
#define EV_PEDIDO_IO     0x31
#define EV_RESPOSTA_IO   0x32
#define EV_CONFIRMAR     0x33
#define EV_SSP_FIM       0x36
#define EV_PASSKEY_MOSTRAR 0x3B

/* ------------------------------------------------------------------ L2CAP (bt_l2cap.c) */
#define PSM_SDP  0x0001
#define PSM_HID_CTL 0x0011
#define PSM_HID_INT 0x0013

#define L2_CERRADO     0
#define L2_ESPERA_CONEX 1   /* mandamos Connection Request */
#define L2_CONFIGURANDO 2
#define L2_ABERTO      3
#define L2_CERRANDO    4

typedef struct {
    bool     usado;
    uint8_t  estado;
    uint8_t  id;               /* identificador de la última petición nuestra */
    bool     cfg_local, cfg_remoto;   /* nuestra configuración aceptada / la suya aceptada */
    bool     iniciado_aqui;
    uint16_t manejador, psm, scid, dcid, mtu_remoto;
    uint32_t desde;
} bt_canal_t;

void bt_l2cap_reiniciar(void);
int  bt_l2cap_conectar(uint16_t manejador, uint16_t psm);          /* índice de canal o <0 */
void bt_l2cap_cerrar(int canal);
int  bt_l2cap_enviar(int canal, const uint8_t *dados, int n);
void bt_l2cap_acl(uint16_t manejador, uint8_t pb, const uint8_t *dados, int n);
void bt_l2cap_ligacao_caiu(uint16_t manejador);
void bt_l2cap_tic(void);
const bt_canal_t *bt_l2cap_canal(int i);
/* hacia arriba (los implementa el gestor): */
void bt_g_canal_aberto(int canal);
void bt_g_canal_fechado(int canal, uint16_t manejador, uint16_t psm, int motivo);
void bt_g_canal_dados(int canal, const uint8_t *dados, int n);
void bt_g_hid_desenchufado(int canal);                   /* el dispositivo pidió desenchufar el cable virtual */
bool bt_g_aceitar_psm(uint16_t manejador, uint16_t psm);

/* ------------------------------------------------------------------ SDP (bt_sdp.c) */
typedef struct {
    bool     ok;                   /* hay un servicio HID */
    uint16_t psm_ctl, psm_int;
    uint8_t  subclasse;
    bool     boot;
    char     nome[BT_NOME_MAX];
    uint8_t  desc[BT_DESC_MAX];
    uint16_t desc_tam;
} bt_sdp_hid_t;
void bt_sdp_pedir(int canal);                            /* manda la petición (y las continuaciones) */
/* Procesa una respuesta; devuelve 1 = acabó (resultado en *r), 0 = sigue (se pidió la continuación), <0 error. */
int  bt_sdp_resposta(int canal, const uint8_t *dados, int n, bt_sdp_hid_t *r);
int  bt_sdp_analisar(const uint8_t *dados, int n, bt_sdp_hid_t *r);   /* solo el análisis (pruebas) */
void bt_sdp_reiniciar(void);

/* ------------------------------------------------------------------ HID (bt_hid.c) */
int  bt_hid_protocolo_boot(int canal_ctl);               /* SET_PROTOCOL(boot) */
void bt_hid_dados_ctl(int canal, const uint8_t *d, int n);
void bt_hid_dados_int(int canal, const uint8_t *d, int n, uint8_t tipo, bool boot);
int  bt_hid_leds(int canal_int, uint8_t leds);
int  bt_hid_desconectar_virtual(int canal_ctl);          /* HID_CONTROL: virtual cable unplug */

/* ------------------------------------------------------------------ gestor (bt_gestor.c) */
void bt_g_iniciar(void);                                 /* tras bt_hci_iniciar: tablas a cero */
void bt_g_carregar(const char *texto, int n);            /* archivo de configuración */
int  bt_g_serializar(char *saida, int max);
void bt_g_adaptador_pronto(void);                        /* HCI terminó de iniciar */
/* eventos HCI hacia el gestor */
void bt_g_inq_resultado(const uint8_t *addr, uint32_t clase, int8_t rssi, const char *nome);
void bt_g_inq_fim(void);
/* D411: baja energía. Un anuncio visto en la búsqueda LE; el resultado de las órdenes LE. */
void bt_g_le_anuncio(const uint8_t *addr, uint8_t addr_tipo, int8_t rssi, const uint8_t *ad, int n);
void bt_g_le_orden(uint16_t op, uint8_t estado);
void bt_g_nome_remoto(const uint8_t *addr, int estado, const char *nome);
void bt_g_conexao(int estado, uint16_t manejador, const uint8_t *addr, bool entrante);
void bt_g_desconexao(uint16_t manejador, uint8_t motivo);
void bt_g_autenticacao(uint16_t manejador, int estado);
void bt_g_cifrado(uint16_t manejador, int estado, bool ligado);
bool bt_g_chave_pedida(const uint8_t *addr, uint8_t *chave);          /* true = la tenemos */
void bt_g_chave_nova(const uint8_t *addr, const uint8_t *chave, uint8_t tipo);
bool bt_g_io_pedido(const uint8_t *addr);                 /* ¿emparejamos con este dispositivo? */
void bt_g_io_resposta(const uint8_t *addr, uint8_t io);
void bt_g_confirmar_pedido(const uint8_t *addr, uint32_t num);
void bt_g_passkey_mostrar(const uint8_t *addr, uint32_t num);
void bt_g_pin_pedido(const uint8_t *addr);
void bt_g_ssp_fim(const uint8_t *addr, int estado);
bool bt_g_pedido_conexao(const uint8_t *addr);           /* ¿aceptamos la conexión entrante? */
void bt_g_tic(void);
/* operaciones (SIS_BLUETOOTH) */
int  bt_buscar(void);
int  bt_conectar(int idx, bool so_parear);
int  bt_desconectar(int idx);
int  bt_esquecer(int idx);
int  bt_confirmar(bool sim);
int  bt_energia(bool ligada);
int  bt_n_disp(void);
const bt_disp_t *bt_disp(int i);
void bt_g_info(bt_info_t *i);
bt_disp_t *bt_g_por_addr(const uint8_t *addr, bool criar);
bt_disp_t *bt_g_por_manejador(uint16_t manejador);
bt_disp_t *bt_g_por_canal(int canal);
int  bt_g_indice(const bt_disp_t *d);
void bt_g_erro(const char *msg);
void bt_g_mudou(void);                                   /* algo cambió en la configuración: guardar */
void bt_g_set_estado_adaptador(int estado);
void bt_g_adaptador_retirado(void);
bool bt_g_energia(void);

/* Utilidades */
#ifdef BT_ANFITRION
#define bt_strlen(s) strlen(s)
#else
#define bt_strlen(s) txt_tamanho(s)
#endif
/* Copia con corte y NUL final. */
static inline void bt_copia(char *dst, const char *src, int max)
{
    int i = 0;
    for (; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}
int  bt_hex(char *saida, const uint8_t *d, int n);
int  bt_addr_texto(char *saida, const uint8_t *addr);    /* "AA:BB:CC:DD:EE:FF" (orden de lectura) */
bool bt_addr_ler(const char *s, uint8_t *addr);
static inline uint16_t bt_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint16_t bt_be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline void bt_poe16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void bt_poe_be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

#endif
