// SPDX-License-Identifier: GPL-2.0-or-later
/* uacpi_sagui.h - lo que comparten la capa de uACPI (uacpi_sagui.c), el
 * controlador embebido (ec.c) y la batería (bateria.c). D192.
 *
 * Todo esto corre en una sola tarea del kernel, "acpi-aml", en la CPU 0: es
 * la única que llama a uACPI. El SCI (una IRQ) también entra en uACPI, pero
 * solo para los eventos fijos (botón, cerrojo global). */
#ifndef SAGUI_UACPI_SAGUI_H
#define SAGUI_UACPI_SAGUI_H
#include "tipos.h"
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include <uacpi/utilities.h>
#include <uacpi/opregion.h>
#include <uacpi/notify.h>
#include <uacpi/event.h>
#include <uacpi/resources.h>
#include "wmi_wdg.h"

/* uacpi_sagui.c */
void uacpi_sagui_dormir_ms(uint32_t ms);          /* duerme la tarea (no gira) */
void uacpi_sagui_trabalho_pendente(void);         /* corre las notificaciones encoladas */
void uacpi_sagui_acordar(void);                   /* despierta la tarea (un pedido nuevo, D405) */
bool uacpi_sagui_pode_dormir(void);               /* quien llama puede dormir (tarea o programa, fuera de IRQ) */

/* ec.c: el controlador embebido (PNP0C09) por sondeo, sin su GPE. */
bool     ec_iniciar(void);                 /* busca el EC e instala su manejador (antes de _INI) */
void     ec_atender_eventos(void);         /* SCI_EVT: consulta (0x84) y ejecuta _Qxx */
bool     ec_presente(void);
uint32_t ec_consultas(void);

/* bateria.c */
void bateria_iniciar(void);                /* busca PNP0C0A y ACPI0003 (después de _INI) */
void bateria_ciclo(void);                  /* lee _STA/_BIF/_BST/_PSR si toca */
void bateria_uacpi_estado(uint32_t estado, const char *motivo);   /* UACPI_* para INFO_BATERIA */

/* wmi.c: el puente WMI de ACPI (PNP0C14), común a todos los fabricantes (D405). Los
 * GUID van en texto ("5FB7F034-2C63-45E9-BE91-3D44E2C707E4"). Solo desde la tarea. */
#define WMI_ERRO_SEM_BLOCO  (-1)   /* no hay ese GUID (o no es de ese tipo) */
#define WMI_ERRO_ARG        (-2)   /* instancia fuera de rango */
#define WMI_ERRO_AML        (-3)   /* el método falló en el AML */
typedef void (*wmi_evento_f)(uint8_t notificacao, const uint8_t *dados, uint32_t n);   /* dados = _WED */
void wmi_iniciar(void);                                         /* busca PNP0C14 y lee _WDG (después de _INI) */
bool wmi_tem(const char *guid);
bool wmi_assinar_evento(const char *guid, wmi_evento_f f);      /* y WExx(1) */
int  wmi_metodo(const char *guid, uint32_t instancia, uint32_t metodo, const void *entrada, uint32_t nentrada,
                void *saida, uint32_t max, uint32_t *nsaida);   /* WMxx -> 0 o WMI_ERRO_* */
int  wmi_consultar(const char *guid, uint32_t instancia, void *saida, uint32_t max, uint32_t *nsaida);   /* WQxx */
uint32_t wmi_eventos(void);
bool wmi_zerar_inteiro(const char *guid, const char *nome);   /* un Name entero del dispositivo a 0 */
int  wmi_blocos(const wmi_bloco_t **v);                         /* los vistos en todos los _WDG */

/* hp_wmi.c: radios y teclas del panel de los HP (D405). */
void hp_wmi_iniciar(void);                 /* después de wmi_iniciar() */
void hp_wmi_ciclo(void);                   /* pedidos de los programas y la vuelta del PCI */

/* D403: lo propio de los portátiles (portatil.c, video_acpi.c, fujitsu.c, toshiba.c). */
#include "abi.h"

/* Un control del brillo: el primero que se registra manda (el ACPI Video, que
 * es el estándar, se busca antes que los de cada marca). */
typedef struct {
    int        fonte;                 /* PORTATIL_FONTE_* */
    int        niveis;                /* cuántos niveles (>= 2) */
    int      (*ler)(void);            /* índice actual; -1 = no se sabe */
    bool     (*fixar)(int indice);
    int      (*pct)(int indice);      /* 0-100 */
    uint32_t (*valor)(int indice);    /* el valor tal como lo usa el firmware */
    const char *nome;                 /* ruta del dispositivo, para el registro y `portatil` */
} brilho_drv_t;

void portatil_iniciar(void);              /* después de bateria_iniciar(): vídeo, Fujitsu, Toshiba */
void portatil_ciclo(void);                /* en cada vuelta de la tarea: pedidos y lecturas */
bool portatil_pendente(void);             /* hay un pedido sin atender (la tarea no debe dormir) */
void portatil_registrar_brilho(const brilho_drv_t *d);
int  portatil_brilho_fonte(void);         /* PORTATIL_FONTE_* del que manda */
void portatil_evento(int alvo, uint32_t valor);   /* cuenta un aviso del firmware (para `portatil`) */
void portatil_brilho_sincronizar(void);   /* el firmware cambió el brillo él solo: releer y avisar */
/* Ayudas para los drivers: ¿existe el método? y evaluar con argumentos enteros. */
bool portatil_tem_metodo(uacpi_namespace_node *no, const char *nome);
uacpi_status portatil_eval_ints(uacpi_namespace_node *no, const char *nome, const uint64_t *args, int n,
                                uacpi_object **ret);
uacpi_status portatil_eval_int(uacpi_namespace_node *no, const char *nome, const uint64_t *args, int n, uint64_t *v);
void portatil_nome(uacpi_namespace_node *no, char *dst, uint32_t max);

void video_acpi_iniciar(void);
void video_acpi_notificar(int alvo, uint32_t valor);   /* PORTATIL_ALVO_VIDEO o _BUS */
void video_acpi_info(info_portatil_t *p);
void fujitsu_iniciar(void);
void fujitsu_notificar(int alvo, uint32_t valor);      /* PORTATIL_ALVO_FUJ_BL o _FUJ_EXT */
void fujitsu_luz_radio(bool apagada);
void fujitsu_info(info_portatil_t *p);
void toshiba_iniciar(void);
void toshiba_notificar(uint32_t valor);
void toshiba_ciclo(void);
void toshiba_info(info_portatil_t *p);

#endif
