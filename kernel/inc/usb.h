// SPDX-License-Identifier: GPL-3.0-only
/* usb.h - controladores EHCI y xHCI, dispositivos USB, hubs, almacenamiento y HID
 * (fase 6; xHCI y HID en la v7.3, D214). */
#ifndef SAGUI_USB_H
#define SAGUI_USB_H
#include "tipos.h"
#include "pci.h"

#define USB_MAX_CONTROLADORES 4
#define USB_MAX_PORTAS        16
#define USB_MAX_DISP          16

/* Errores internos de las transferencias. */
#define USB_ERRO_PAROU  -100   /* el dispositivo respondió STALL */
#define USB_ERRO_TEMPO  -101   /* no respondió a tiempo */
#define USB_ERRO        -102   /* error de transmisión, el dispositivo desapareció... */

typedef struct ehci ehci_t;
typedef struct xhci xhci_t;
typedef struct uhci uhci_t;   /* D267: USB 1.1 de Intel/VIA (puertos de E/S) */
typedef struct ohci ohci_t;   /* D267: USB 1.1 de AMD/NVIDIA/SiS... (MMIO) */

/* Controlador al que está conectado un dispositivo (D214). */
#define USB_EHCI 0
#define USB_XHCI 1
#define USB_UHCI 2
#define USB_OHCI 3

/* Velocidades: los números son los del xHCI (Protocol Speed ID por omisión). */
#define USB_VEL_PLENA   1   /* 12 Mb/s (USB 1.1) */
#define USB_VEL_BAIXA   2   /* 1,5 Mb/s (teclados y ratones) */
#define USB_VEL_ALTA    3   /* 480 Mb/s (USB 2.0) */
#define USB_VEL_SUPER   4   /* 5 Gb/s (USB 3.0) */
#define USB_VEL_SUPERP  5   /* 10 Gb/s (USB 3.1) */

/* Una interfaz HID (teclado o ratón) de un dispositivo: un dispositivo puede
 * traer las dos (el receptor inalámbrico de un teclado con ratón). */
#define USB_MAX_HID 2
typedef struct {
    uint8_t  interface, subclasse, protocolo;
    uint8_t  ep;                /* endpoint de interrupción de entrada (número, sin el bit 0x80) */
    uint16_t mps;
    uint8_t  intervalo;         /* bInterval tal cual */
    uint16_t tam_relatorio;     /* longitud del descriptor de informe (0x22) */
    bool     xbox360;           /* D307: no es HID sino el mando de Xbox 360 con cable (0xFF/0x5D/0x01) */
} usb_hid_if_t;

/* D227: estado de un puerto (del controlador o de un hub) en la conexión en
 * caliente. Cada conexión nueva tiene hasta USB_INTENTOS_MAX intentos: los
 * USB_INTENTOS_RAPIDOS primeros enseguida y a 1, 2 y 5 s; los demás, cada 10 s. */
#define USB_PORTA_VAZIA        0   /* nada conectado (o todavía sin mirar) */
#define USB_PORTA_PENDENTE     1   /* conectado y sin reconocer: hay otro intento programado */
#define USB_PORTA_OK           2   /* reconocido */
#define USB_PORTA_DESISTIU     3   /* sin reconocer tras todos los intentos, o sin solución posible */
#define USB_PORTA_COMPANHEIRO  4   /* EHCI: USB 1.x, puerto cedido al controlador compañero */
#define USB_INTENTOS_RAPIDOS   4
#define USB_INTENTOS_MAX       10
#define USB_HUB_MAX_PORTAS     15  /* la "route string" del xHCI da 4 bits por nivel */

typedef struct {
    uint8_t  estado;       /* USB_PORTA_* */
    uint8_t  tentativas;   /* intentos hechos en esta conexión */
    uint8_t  passo;        /* paso donde se quedó el último intento (usb_nome_passo) */
    uint8_t  reservado;
    uint32_t proxima;      /* tic del próximo intento (USB_PORTA_PENDENTE) */
    uint32_t conexoes;     /* conexiones vistas en el puerto (diagnóstico) */
} usb_porta_t;

/* CAB_USB_PRUEBA (cabecera, byte 141; mkimagem.py --usb-prueba): fallos
 * simulados de la enumeración, solo para las pruebas (D227). */
#define USB_PRUEBA_PRIMEIRO 1   /* el primer intento de cada conexión falla (paso 3) */
#define USB_PRUEBA_SEMPRE   2   /* fallan todos los intentos */

/* Respuestas especiales de ehci_porta_resetar (D227). */
#define USB_PORTA_CEDIDA  -103   /* USB 1.x: el puerto pasa al compañero (bit PO) */
#define USB_PORTA_USB1    -104   /* USB 1.x de baja velocidad y sin compañero: no hay quien lo lleve */
#define EHCI_PORTA_PO     (1u << 13)   /* PORTSC: el puerto es del compañero */

typedef struct usb_disp {
    bool      usado;
    uint8_t   tipo_ctl;        /* USB_EHCI o USB_XHCI */
    ehci_t   *ctl;             /* si es EHCI */
    xhci_t   *xctl;            /* si es xHCI */
    uhci_t   *uctl;            /* si es UHCI (D267) */
    ohci_t   *octl;            /* si es OHCI (D267) */
    uint8_t   slot;            /* xHCI: ranura del dispositivo */
    uint8_t   velocidade;      /* USB_VEL_* */
    uint8_t   porta_raiz;      /* puerto del controlador (desde 0) por el que se llega */
    uint32_t  rota;            /* xHCI: "route string" (puerto de cada hub, 4 bits por nivel) */
    uint8_t   profundidade;    /* cuántos hubs hay por encima */
    uint8_t   tt_endereco;     /* baja/plena velocidad detrás de un hub de alta: dirección del hub (EHCI) */
    uint8_t   tt_slot;         /* ... y su ranura (xHCI) */
    uint8_t   tt_porta;        /* ... y el puerto de ese hub */
    uint16_t  bcd_usb;
    uint8_t   burst_entrada, burst_saida;  /* USB 3.0: ráfaga de los endpoints bulk */
    uint8_t   n_hid;
    usb_hid_if_t hid[USB_MAX_HID];
    uint8_t   endereco;
    uint16_t  mps0;            /* tamaño máximo de paquete del endpoint 0 (512 en USB 3.0) */
    struct usb_disp *hub;      /* NULL = conectado directo al puerto del controlador */
    uint8_t   porta;           /* puerto del hub (o del controlador) donde está */
    uint16_t  id_fabricante, id_produto;
    uint8_t   classe, subclasse, protocolo;   /* del dispositivo o de la interfaz */
    char      fabricante[32], produto[48];
    uint8_t   config;
    uint8_t   interface;
    uint8_t   ep_entrada, ep_saida;           /* endpoints bulk (almacenamiento) */
    uint16_t  mps_entrada, mps_saida;
    uint8_t   alterna_entrada, alterna_saida;  /* "data toggle" de cada endpoint bulk */
    uint8_t   hub_portas;                      /* si es hub (como mucho USB_HUB_MAX_PORTAS) */
    usb_porta_t hub_porta[USB_HUB_MAX_PORTAS];  /* si es hub: estado de cada puerto (D227) */
    void     *disco;                           /* si es almacenamiento */
    bool      wifi;                            /* el pincho Wi-Fi (D215) */
    bool      audio;                           /* tarjeta de audio USB registrada (D275) */
    uint8_t   ep_int;                          /* D279: endpoint de interrupción de entrada de la primera interfaz (eventos HCI) */
    uint16_t  mps_int;
    uint8_t   int_intervalo;
    bool      bt;                              /* controlador Bluetooth atendido por kernel/usb/usbbt.c (D279) */
    bool      camara;                          /* cámara UVC registrada (D277) */
    bool      ure;                             /* adaptador Ethernet USB Realtek atendido por kernel/rede/ure.c (D433) */
    /* D468: almacenamiento. Sectores por orden READ/WRITE(10) (baja sola si el pendrive falla con
     * trozos grandes) y contadores de órdenes (las cifras salen con «sync -v»). */
    uint16_t  armaz_trozo;
    uint8_t   armaz_reducoes;                  /* veces que se ha bajado el trozo */
    uint8_t   armaz_vpd;                       /* 0 = sin VPD B0; 1 = la dio; 2 = falló al pedirla */
    uint32_t  armaz_ord_ler, armaz_kb_ler;     /* órdenes de lectura y KB leídos */
    uint32_t  armaz_ord_escr, armaz_kb_escr;   /* órdenes de escritura y KB grabados */
    uint32_t  armaz_maior_kb;                  /* la orden más grande hecha (KB) */
    uint32_t  armaz_encadeadas;                /* D468: fases de datos y CSW (o CBW y datos) en una sola entrega */
} usb_disp_t;

/* D468: varias transferencias bulk seguidas en el MISMO endpoint, entregadas al controlador de
 * una vez (sin esperar a la interrupción entre una y otra): CBW + datos (salida) o datos + CSW
 * (entrada) de una orden de almacenamiento. Cada parte recibe en «r» los bytes hechos o un error
 * USB_ERRO_*; las partes que no llegaron a hacerse, USB_NAO_FEITA (la anterior falló o, en el
 * EHCI, vino corta: quien llama repite esa parte por separado). La alternancia (toggle) queda
 * como si se hubieran hecho de una en una. Solo una parte puede pasar de 512 bytes. */
#define USB_NAO_FEITA   -105
#define USB_CADEIA_MAX  3
typedef struct {
    void    *dados;
    uint32_t tam;
    int      r;
} usb_parte_t;
int  usb_bulk_cadeia(usb_disp_t *d, bool entrada, usb_parte_t *p, int n);
/* D468: bytes como mucho de una transferencia bulk por el controlador del dispositivo. */
uint32_t usb_bulk_max(usb_disp_t *d);

/* ---- ehci.c ---- */
struct ehci {
    pci_disp_t pci;
    volatile uint8_t *cap;     /* registros de capacidad */
    volatile uint32_t *op;     /* registros de operación */
    int       portas;
    uint8_t   proximo_endereco;
    bool      ok;
    int       companheiros;            /* controladores USB 1.1 asociados (0 en el G470) */
    bool      enderecos64;             /* usa las estructuras en formato de 64 bits */
    int       bios;                    /* 0 = no lo usaba, 1 = lo soltó, -1 = no lo soltó */
    int       resultado[USB_MAX_PORTAS];   /* del último intento de enumerar el puerto (0 = ok) */
    uint32_t  estruturas;              /* página con el anillo asíncrono solo de este controlador */
    uint32_t  periodica;               /* lista periódica (HID, D214); 0 = apagada */
    uint32_t  continua;                /* QH de recepción continua (Wi-Fi, D215); 0 = ninguna */
    uint32_t  ancho_iso;               /* D275: bytes por microcuadro reservados por flujos isócronos */
    uint8_t   irq;                     /* línea PCI del controlador (0 = sin IRQ utilizable) */
    bool      irq_usavel;              /* la IRQ ya llegó y sigue llegando (D095) */
    bool      esperando;               /* un proceso duerme esperando el fin de la transferencia */
    bool      irq_chegou;              /* la IRQ del fin de la transferencia actual llegó */
    int       esperas_sem_irq;         /* despertadas solo por el reloj, seguidas */
    uint32_t  irqs;                    /* cuántas IRQ de fin de transferencia ya llegaron */
    volatile bool     prova;           /* D211: autoprueba de la IRQ en curso (aviso IAA) */
    volatile uint32_t provas;          /* D211: avisos IAA de la autoprueba atendidos */
    uint32_t  qtds_bulk;               /* D468: páginas con los qTD de las transferencias bulk largas (0 = las 8 de siempre) */
};

int  ehci_iniciar_todos(void);                 /* devuelve cuántos controladores encendió */
int  ehci_quantidade(void);
int  ehci_parar_todos(void);   /* D450: apagar/reiniciar: Run/Stop = 0 (20 ms cada uno); cuántos se pararon */
void ehci_modo_assincrono(bool ligado);        /* D095: puede dormir esperando el DMA (solo con planificador y IRQ) */
ehci_t *ehci_controlador(int i);
uint32_t ehci_porta_estado(ehci_t *c, int porta);
bool ehci_porta_mudou(ehci_t *c, int porta);   /* lee y limpia "cambió la conexión" */
/* 0 = alta velocidad lista; USB_PORTA_CEDIDA / USB_PORTA_USB1 = USB 1.x (D227); otro <0 = no se habilitó */
int  ehci_porta_resetar(ehci_t *c, int porta);
void ehci_esperar_ms(ehci_t *c, uint32_t ms);
/* Transferencias síncronas. Devuelven bytes transferidos o un error USB_ERRO_*.
 * Del dispositivo salen la dirección, la velocidad y, para los de baja o plena
 * velocidad detrás de un hub de alta, el "transaction translator" (D214). */
int  ehci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada);
int  ehci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam);
/* D468: usb_bulk_cadeia por el EHCI y el tamaño máximo de una transferencia bulk. */
int  ehci_bulk_cadeia(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, usb_parte_t *p, int n);
uint32_t ehci_bulk_max(void);
/* Endpoints de interrupción (HID, D214) en la lista periódica: se abren, se
 * consultan sin esperar (>0 bytes recibidos, 0 nada nuevo, <0 error) y se cierran. */
int  ehci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps);
int  ehci_int_ler(int h, uint8_t *dados, int max);
void ehci_int_fechar(int h);
/* Recepción bulk continua (Wi-Fi, D215): una QH fija en el anillo asíncrono
 * con su buffer (físico, alineado a página, <= 16 KB); se consulta sin esperar
 * y sin el bloqueo de las transferencias. Una por controlador. */
int  ehci_rx_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps, uint32_t buffer_fis, uint32_t tam);
int  ehci_rx_ler(int h, uint8_t *dados, int max);
void ehci_rx_fechar(int h);
/* D238: la misma recepción con una cola de n qTD (n buffers de tam bytes
 * seguidos, alineados a página); se lee y se cierra con ehci_rx_ler/fechar. */
int  ehci_rx_abrir_fila(usb_disp_t *d, uint8_t ep, uint16_t mps, uint32_t buffer_fis, uint32_t tam, int n);
bool ehci_rx_info(int h, uint32_t *emvoo, uint32_t *completadas, uint32_t *erros);

/* ---- uhci.c y ohci.c (D267): USB 1.1, baja y plena velocidad ----
 * Misma interfaz que el EHCI: control, bulk (con su alternancia) y endpoints
 * de interrupción para el HID. Sin transferencias continuas por usb_rx_* (el Wi-Fi no
 * va por aquí); el UHCI tiene su propia recepción bulk en flujo (uhci_rx_*, D389) para
 * el Bluetooth. Los puertos devuelven la velocidad (USB_VEL_*). */
struct uhci {
    pci_disp_t pci;
    uint16_t  io;              /* base de los puertos de E/S (BAR 4) */
    int       portas;
    bool      ok;
    uint8_t   proximo_endereco;
    int       bios;            /* 0 = no lo usaba, 1 = lo soltó */
    int       resultado[USB_MAX_PORTAS];
    uint32_t  quadros;         /* lista de 1024 cuadros */
    uint32_t  pagina;          /* QH, paquete de setup y 256 TD */
    uint32_t  ultimo_td;       /* D389: palabra de estado del último TD con error (diagnóstico) */
    uint16_t  porta_antes, porta_depois;   /* D389: PORTSC antes y después del último reset */
};
int  uhci_iniciar_todos(void);
int  uhci_quantidade(void);
uhci_t *uhci_controlador(int i);
uint32_t uhci_porta_estado(uhci_t *c, int porta);
bool uhci_porta_conectada(uhci_t *c, int porta);
bool uhci_porta_mudou(uhci_t *c, int porta);
int  uhci_porta_resetar(uhci_t *c, int porta);      /* velocidad USB_VEL_* o <0 */
int  uhci_erro_texto(uint32_t ctrl, char *s, int max);   /* D389: los bits de error de un TD, en texto */
int  uhci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada);
int  uhci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam);
int  uhci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps);
int  uhci_int_ler(int h, uint8_t *dados, int max);
void uhci_int_fechar(int h);
int  uhci_rx_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps);   /* D389: bulk de entrada continuo (flujo de bytes) */
int  uhci_rx_ler(int h, uint8_t *dados, int max);
void uhci_rx_fechar(int h);
bool uhci_rx_info(int h, uint32_t *paquetes, uint32_t *erros);

struct ohci {
    pci_disp_t pci;
    volatile uint32_t *r;      /* registros (MMIO) */
    int       portas;
    bool      ok;
    uint8_t   proximo_endereco;
    int       bios;            /* 0 = no lo usaba, 1 = lo soltó (SMM), 2 = lo usaba sin SMM, -1 = no lo soltó */
    int       resultado[USB_MAX_PORTAS];
    uint32_t  pagina;          /* HCCA, ED de trabajo y 220 TD */
};
int  ohci_iniciar_todos(void);
int  ohci_quantidade(void);
ohci_t *ohci_controlador(int i);
uint32_t ohci_porta_estado(ohci_t *c, int porta);
bool ohci_porta_conectada(ohci_t *c, int porta);
bool ohci_porta_mudou(ohci_t *c, int porta);
int  ohci_porta_resetar(ohci_t *c, int porta);      /* velocidad USB_VEL_* o <0 */
int  ohci_controle(usb_disp_t *d, const void *setup, void *dados, uint16_t tam, bool entrada);
int  ohci_bulk(usb_disp_t *d, uint8_t ep, uint16_t mps, bool entrada, uint8_t *alterna, void *dados, uint32_t tam);
int  ohci_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps);
int  ohci_int_ler(int h, uint8_t *dados, int max);
void ohci_int_fechar(int h);
bool ehci_pode_dormir(void);   /* las esperas de UHCI/OHCI ceden la CPU con las mismas reglas que el EHCI */

/* ---- usb.c ---- */
void usb_iniciar(void);
int  usb_controle(usb_disp_t *d, uint8_t tipo, uint8_t pedido, uint16_t valor, uint16_t indice, void *dados, uint16_t tam);
/* Transferencia bulk por el endpoint bulk de entrada o de salida del dispositivo,
 * por el controlador que sea (D214). Bytes transferidos o USB_ERRO_*. */
int  usb_bulk(usb_disp_t *d, bool entrada, void *dados, uint32_t tam);
void usb_esperar_ms(usb_disp_t *d, uint32_t ms);
/* Endpoints de interrupción de los controladores que no son xHCI (HID). */
int  usb_int_abrir(usb_disp_t *d, uint8_t ep, uint16_t mps);
int  usb_int_ler(usb_disp_t *d, int h, uint8_t *dados, int max);
void usb_int_fechar(usb_disp_t *d, int h);
/* D227: espera de los puertos (reset, antirrebote). En la tarea "usb" duerme con
 * las interrupciones encendidas; en el arranque gira con el reloj del controlador
 * (c = EHCI) o con el TSC (c = NULL). */
void usb_pausa_ms(ehci_t *c, uint32_t ms);
const char *usb_nome_passo(int passo);         /* texto del paso donde se quedó una enumeración */
/* D215: recepción continua por el endpoint bulk de entrada (el pincho Wi-Fi):
 * buffer físico de 16 KB alineado a 16 KB; leer no espera (>0 bytes, 0 nada,
 * <0 error). Devuelve un identificador (>=0) o USB_ERRO. */
int  usb_rx_abrir(usb_disp_t *d, uint32_t buffer_fis, uint32_t tam);
int  usb_rx_ler(usb_disp_t *d, int h, uint8_t *dados, int max);
void usb_rx_fechar(usb_disp_t *d, int h);
/* D238: colas de varias transferencias bulk en vuelo a la vez (el Wi-Fi).
 *   - recepción: n lecturas de tam bytes (buffers seguidos desde base_fis,
 *     alineados a tam y a página, para siempre) armadas a la vez; se lee y se
 *     cierra con usb_rx_ler/usb_rx_fechar, como la de una sola. EHCI y xHCI.
 *   - envío (solo xHCI; con el EHCI, USB_ERRO y se sigue con usb_bulk): la
 *     trama se copia a uno de los n buffers y se manda sin esperar.
 *   - con el xHCI, cada IRQ del controlador despierta el canal dado (la tarea
 *     no espera al tic para ver lo que llegó). */
#define USB_FILA_MAX 8
int  usb_rx_abrir_fila(usb_disp_t *d, uint32_t base_fis, uint32_t tam, int n);
int  usb_tx_abrir_fila(usb_disp_t *d, uint32_t base_fis, uint32_t tam, int n);   /* 0 o USB_ERRO */
int  usb_tx_fila(usb_disp_t *d, const void *dados, uint32_t n);   /* n puesto, 0 cola llena, <0 error */
void usb_tx_fechar_fila(usb_disp_t *d);
bool usb_fila_info(usb_disp_t *d, int h, bool entrada, uint32_t *emvoo, uint32_t *completadas, uint32_t *perdidas);
void usb_avisar_fila(const void *canal);
const char *usb_nome_velocidade(int v);
const char *usb_nome_ctl(uint8_t tipo);       /* "EHCI", "xHCI", "UHCI" u "OHCI" */
usb_disp_t *usb_disp(int i);
void usb_verificar_conexoes(void);             /* conexión y retirada en caliente */


/* ---- transferencias isócronas (D275) ----
 * Un flujo isócrono es un anillo de datos en memoria física (contigua, por debajo
 * de 4 GB) que el controlador recorre sin parar, un paquete por (micro)cuadro, con
 * el tamaño que da la cuenta de bytes por segundo: el que escribe (salida) o lee
 * (entrada) detrás de la posición completada. La pila reparte los paquetes, mantiene
 * llena una cola de unos USB_ISO_COLA_MS milisegundos y cuenta lo completado; quien
 * usa el flujo solo mira usb_iso_posicao(), que también reabastece la cola (se llama
 * en cada tic y en cada IRQ del audio, con las interrupciones encendidas o no).
 * Entrada (D277, cámaras UVC): el anillo se divide en casillas de mps*mult bytes; cada
 * paquete cae en la siguiente casilla y usb_iso_ler() devuelve, en orden, dónde cayó y
 * cuántos bytes trae (isoent.h). Una casilla se reutiliza al leerla. */
#define USB_ISO_COLA_MS  32       /* milisegundos de paquetes que se mantienen en la cola */
#define USB_ISO_MAX      2        /* flujos isócronos abiertos a la vez por controlador */
#define USB_ISO_ANCHO_MAX 6000    /* EHCI: bytes isócronos+periódicos por microcuadro (80 % de 7500) */
typedef struct {
    uint8_t  ep;          /* número de endpoint (sin el bit de dirección) */
    bool     entrada;     /* false = salida (altavoz); true = entrada (cámara, D277): bytes_s y tam_quadro no se usan */
    uint8_t  mult;        /* transacciones por microcuadro de alta velocidad (0 o 1 = una; hasta 3) */
    uint16_t mps;         /* bytes máximos por transacción (sin multiplicador) */
    uint8_t  intervalo;   /* bInterval tal cual */
    uint32_t anel_fis;    /* anillo de datos (físico, contiguo) */
    uint32_t anel_tam;    /* bytes; múltiplo de tam_quadro */
    uint32_t bytes_s;     /* bytes por segundo del flujo */
    uint8_t  tam_quadro;  /* bytes de un cuadro de audio: cada paquete es un número entero */
} usb_iso_cfg_t;
typedef struct {          /* contadores de un flujo (diagnóstico) */
    uint32_t paquetes, completados, subdesbordes, errores, ancho;
} usb_iso_info_t;
/* Reparto de bytes por paquete (acumulador): bytes de este paquete para un periodo en
 * microsegundos; *acc guarda el resto (en millonésimas de cuadro). */
uint32_t usb_iso_calcular(uint32_t *acc, uint32_t bytes_s, uint8_t tam_quadro, uint32_t periodo_us, uint32_t mps);
int  usb_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg);   /* manejador >= 0 o USB_ERRO */
void usb_iso_ligar(usb_disp_t *d, int h);                      /* empieza por el inicio del anillo */
void usb_iso_parar(usb_disp_t *d, int h);                      /* deja de poner paquetes nuevos */
uint32_t usb_iso_posicao(usb_disp_t *d, int h);                /* bytes completados desde ligar (y reabastece) */
void usb_iso_fechar(usb_disp_t *d, int h);
bool usb_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i);
/* Entrada: siguiente paquete recibido y sin leer (llamar antes a usb_iso_posicao, que completa y reabastece). */
bool usb_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro);
bool xhci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro);
bool uhci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro);
bool ohci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro);
bool ehci_iso_ler(usb_disp_t *d, int h, uint32_t *off, uint32_t *len, bool *erro);
int  xhci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg);
void xhci_iso_ligar(usb_disp_t *d, int h);
void xhci_iso_parar(usb_disp_t *d, int h);
uint32_t xhci_iso_posicao(usb_disp_t *d, int h);
void xhci_iso_fechar(usb_disp_t *d, int h);
bool xhci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i);
int  uhci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg);
void uhci_iso_ligar(usb_disp_t *d, int h);
void uhci_iso_parar(usb_disp_t *d, int h);
uint32_t uhci_iso_posicao(usb_disp_t *d, int h);
void uhci_iso_fechar(usb_disp_t *d, int h);
bool uhci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i);
int  ohci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg);
void ohci_iso_ligar(usb_disp_t *d, int h);
void ohci_iso_parar(usb_disp_t *d, int h);
uint32_t ohci_iso_posicao(usb_disp_t *d, int h);
void ohci_iso_fechar(usb_disp_t *d, int h);
bool ohci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i);
int  ehci_iso_abrir(usb_disp_t *d, const usb_iso_cfg_t *cfg);
void ehci_iso_ligar(usb_disp_t *d, int h);
void ehci_iso_parar(usb_disp_t *d, int h);
uint32_t ehci_iso_posicao(usb_disp_t *d, int h);
void ehci_iso_fechar(usb_disp_t *d, int h);
bool ehci_iso_info(usb_disp_t *d, int h, usb_iso_info_t *i);

/* ---- usbaudio.c (D275): audio USB, clase Audio 1.0 ---- */
bool usbaudio_conectar(usb_disp_t *d);      /* ¿tiene interfaces de audio que se usan? */
void usbaudio_desconectar(usb_disp_t *d);
int  usbaudio_relatorio(char *s, int max);

/* ---- uvc.c (D277): cámaras UVC 1.1 y la fuente de prueba ---- */
bool uvc_conectar(usb_disp_t *d);          /* ¿es una cámara UVC que se pueda usar? */
void uvc_desconectar(usb_disp_t *d);
void uvc_tique(void);                      /* del reloj: recoge paquetes y monta cuadros */
int  uvc_relatorio(char *s, int max);

/* ---- armazenamento.c ---- */
/* >0 = disco registrado; 0 = no se puede usar (sin endpoints, sector raro); <0 = no
 * respondió: vale la pena otro intento con un reset nuevo (D227). */
int  armaz_conectar(usb_disp_t *d);
void armaz_desconectar(usb_disp_t *d);
void armaz_registar(void);   /* D468: las cifras de órdenes de cada pendrive, al registro («sync -v») */

/* ---- hid.c (D214): teclados y ratones USB; mandos y joysticks (D307) ---- */
void hid_conectar(usb_disp_t *d);
void hid_desconectar(usb_disp_t *d);
int  hid_relatorio(char *s, int max);

/* ---- Bluetooth (D279): kernel/usb/usbbt.c y kernel/bluetooth/ ---- */
bool bt_usb_reconhecer(usb_disp_t *d);       /* true = es un controlador Bluetooth (clase 0xE0/0x01/0x01) y queda atendido */
/* D433: adaptador Ethernet USB Realtek RTL8152/8153/8156 (kernel/rede/ure.c); true = es de esa familia (aunque no se use) */
bool ure_usb_reconhecer(usb_disp_t *d);
void ure_usb_retirado(usb_disp_t *d);
void bt_usb_retirado(usb_disp_t *d);
int  bt_relatorio(char *s, int max);

/* La entrada de los dispositivos HID Bluetooth sigue el mismo camino que la de los USB (hid.c). */
int  hid_bt_conectar(int tipo, const uint8_t *desc, int n);   /* 1 = ratón entendido por su descriptor (sin protocolo boot) */
void hid_bt_teclado(const uint8_t *r, int n);                 /* informe boot de 8 bytes */
void hid_bt_raton(const uint8_t *r, int n, bool boot);
void hid_bt_soltar(int tipo);
void hid_bt_tic(void);                                        /* repetición de tecla (cada tic) */
bool hid_bt_leds(uint8_t *leds);                              /* true si las luces del teclado cambiaron */

#endif
