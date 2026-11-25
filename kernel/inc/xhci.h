// SPDX-License-Identifier: GPL-2.0-or-later
/* xhci.h - controlador USB 3.0 (xHCI) (D155, D214).
 *
 * El xHCI lleva todos los puertos de un PC moderno (USB 3.x y también USB 2.0
 * y 1.1, sin compañeros). usb.c hace la enumeración igual que con el EHCI;
 * aquí está el transporte: anillos de órdenes, de eventos y de transferencia,
 * ranuras y contextos. */
#ifndef SAGUI_XHCI_H
#define SAGUI_XHCI_H
#include "usb.h"

/* Busca los controladores xHCI en el PCI, se los pide a la BIOS, los reinicia
 * y los pone en marcha. Devuelve cuántos quedan en marcha. */
int  xhci_iniciar_todos(void);
int  xhci_quantidade(void);
xhci_t *xhci_controlador(int i);
int  xhci_parar_todos(void);   /* D450: apagar/reiniciar: Run/Stop = 0 en todos (20 ms cada uno); cuántos se pararon */
void xhci_modo_assincrono(bool ligado);   /* D095/D211: IRQ (con su sonda) y esperas que duermen */

#define XHCI_MAX_CTL    4
#define XHCI_MAX_PORTAS 32   /* puertos que se usan de cada controlador */

/* Puertos del controlador, desde 0. */
int  xhci_portas(xhci_t *c);
uint32_t xhci_porta_estado(xhci_t *c, int p);      /* PORTSC tal cual */
bool xhci_porta_conectada(xhci_t *c, int p);
bool xhci_porta_mudou(xhci_t *c, int p);           /* lee y limpia los bits de cambio */
int  xhci_porta_resetar(xhci_t *c, int p);         /* velocidad USB_VEL_* (>0) o USB_ERRO_* */
void xhci_porta_resultado(xhci_t *c, int p, int r);   /* paso donde se quedó la enumeración (informe) */
int  xhci_porta_protocolo(xhci_t *c, int p);       /* 2 (USB 2.0) o 3 (USB 3.x); 0 = desconocido */

void xhci_esperar_ms(uint32_t ms);   /* por el TSC */

/* Dispositivos: ranura y dirección (Enable Slot + Address Device), tamaño del
 * paquete del endpoint 0 (Evaluate Context), endpoints bulk e interrupción
 * (Configure Endpoint), datos de hub en el contexto de la ranura y liberación. */
int  xhci_endereco(usb_disp_t *d);
int  xhci_mps0(usb_disp_t *d, uint16_t mps);
int  xhci_configurar(usb_disp_t *d);
int  xhci_hub(usb_disp_t *d, uint8_t portas, uint8_t ttt, bool mtt);
void xhci_liberar(usb_disp_t *d);

/* Transferencias: bytes transferidos o USB_ERRO_*. */
int  xhci_controle(usb_disp_t *d, const uint8_t setup[8], void *dados, uint16_t tam, bool entrada);
int  xhci_bulk(usb_disp_t *d, bool entrada, void *dados, uint32_t tam);
/* D468: usb_bulk_cadeia por el xHCI (un TD por parte, todos en el anillo antes del timbre) y el
 * tamaño máximo de una transferencia bulk. */
int  xhci_bulk_cadeia(usb_disp_t *d, bool entrada, usb_parte_t *p, int n);
uint32_t xhci_bulk_max(void);
/* D468: pone a cero la alternancia o la secuencia de un endpoint bulk en el controlador (tras el
 * CLEAR_FEATURE que la pone a cero en el dispositivo). No hace nada si el disp. no es de un xHCI. */
void xhci_reiniciar_ep(usb_disp_t *d, bool entrada);
/* Interrupción de entrada (HID): se arma una lectura y se consulta sin esperar
 * (>0 bytes, 0 nada todavía, <0 error). xhci_int_ler vuelve a armarla. */
int  xhci_int_armar(usb_disp_t *d, uint8_t ep, uint16_t tam);
int  xhci_int_ler(usb_disp_t *d, uint8_t ep, uint8_t *dados, int max);
int  xhci_rx_abrir(usb_disp_t *d, uint32_t buffer_fis, uint16_t tam);   /* D215: Wi-Fi */
/* D238: colas de varias transferencias bulk en vuelo a la vez (el Wi-Fi), sobre
 * buffers del que llama (n de tam bytes seguidos desde base, sin cruzar 64 KB).
 * De entrada, todas armadas siempre; xhci_fila_ler da la más antigua terminada
 * (>0 bytes, 0 nada, <0 error) y la vuelve a armar. De salida, xhci_fila_enviar
 * la pone sin esperar (n puesto, 0 cola llena, <0 error). */
int  xhci_fila_abrir(usb_disp_t *d, bool entrada, uint32_t base, uint32_t tam, int n);
int  xhci_fila_ler(usb_disp_t *d, uint8_t *dados, int max);
int  xhci_fila_enviar(usb_disp_t *d, const void *dados, uint32_t n);
bool xhci_fila_info(usb_disp_t *d, bool entrada, uint32_t *emvoo, uint32_t *completadas, uint32_t *perdidas);
void xhci_fila_fechar(usb_disp_t *d, bool entrada);

/* La tarea de HID se despierta con la IRQ del controlador. */
void xhci_avisar_hid(const void *canal);
/* D227: la tarea "usb" se despierta cuando el controlador avisa de un cambio en
 * un puerto (USBSTS.PCD, con el evento Port Status Change), sin esperar a su
 * próxima vuelta de consulta. */
void xhci_avisar_usb(const void *canal);
/* D238: la tarea que lee las colas se despierta con cada IRQ del controlador. */
void xhci_avisar_fila(const void *canal);
void xhci_processar_todos(void);   /* consume los eventos pendientes (tareas USB) */

int  xhci_relatorio(char *s, int max);

#endif
