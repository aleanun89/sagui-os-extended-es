// SPDX-License-Identifier: GPL-2.0-or-later
/* wmi_wdg.h - el puente WMI de ACPI (PNP0C14) sin hardware ni uACPI: el análisis de _WDG
 * y los formatos de los búferes de HP (D405).
 *
 * Fuentes (solo especificaciones y hechos de interfaz, ningún código de otro driver):
 *   - Microsoft, «Windows Instrumentation: WMI and ACPI» (el documento público del
 *     dispositivo PNP0C14): _WDG es un búfer de bloques de 20 bytes
 *       GUID (16, en el orden binario de Microsoft: Data1-Data3 en little endian)
 *       ObjectId[2] (dos letras: WQxx, WSxx, WMxx, WCxx) o, en un evento,
 *         NotifyId (1 byte, el valor del Notify) y un byte reservado
 *       InstanceCount (1)
 *       Flags (1): 1 = caro (WCxx para encender la recogida), 2 = método,
 *         4 = texto, 8 = evento;
 *     métodos: WMxx(instancia, id del método, búfer de entrada); datos: WQxx(instancia),
 *     WSxx(instancia, búfer); eventos: WExx(1/0) para encenderlos y _WED(NotifyId)
 *     para leer los datos del último;
 *   - el AML de las propias BIOS de HP (dispositivo WMID, métodos WMAD/WMAA/HWMC/WHCM):
 *     el búfer de entrada «SECU», orden, tipo, tamaño y datos; la respuesta con la
 *     firma, el código y los datos. Ver docs/decisoes.md (D405).
 *
 * Las pruebas compilan este mismo archivo (kernel/lib/wmi_wdg.c) en el anfitrión con
 * -DSAGUI_HOST (tests/h407_hp_wmi.py). */
#ifndef SAGUI_WMI_WDG_H
#define SAGUI_WMI_WDG_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif

#define WMI_BLOCO_BYTES   20
#define WMI_CARO          0x01   /* WCxx(1) antes de leer */
#define WMI_METODO        0x02
#define WMI_TEXTO         0x04
#define WMI_EVENTO        0x08

typedef struct {
    uint8_t guid[16];      /* tal cual en _WDG (orden binario de Microsoft) */
    char    id[3];         /* ObjectId de dos letras y cero ("" en un evento) */
    uint8_t notificacao;   /* NotifyId de un evento (0 si no es evento) */
    uint8_t instancias;
    uint8_t flags;         /* WMI_* */
} wmi_bloco_t;

/* Analiza el búfer de _WDG. Devuelve cuántos bloques hay (todos, aunque solo se copien
 * los max primeros) o -1 si el tamaño no es múltiplo de 20 o un bloque no tiene sentido
 * (ObjectId que no son dos caracteres imprimibles en un bloque que no es evento). */
int  wmi_wdg_analisar(const uint8_t *buf, uint32_t n, wmi_bloco_t *saida, int max);

/* "5FB7F034-2C63-45E9-BE91-3D44E2C707E4" (mayúsculas o minúsculas) -> los 16 bytes en el
 * orden de _WDG. false si el texto no es un GUID. */
bool wmi_guid_ler(const char *texto, uint8_t guid[16]);
/* Los 16 bytes -> texto en mayúsculas (37 bytes con el cero). */
void wmi_guid_texto(const uint8_t guid[16], char saida[37]);
/* Índice del bloque con ese GUID, o -1. */
int  wmi_achar(const wmi_bloco_t *v, int n, const uint8_t guid[16]);
/* Índice del bloque de evento con ese NotifyId, o -1. */
int  wmi_achar_evento(const wmi_bloco_t *v, int n, uint8_t notificacao);
/* Nombre del método AML de un bloque: "WM"/"WQ"/"WS"/"WC" + ObjectId; "WE" + el NotifyId
 * en dos cifras hexadecimales en mayúsculas para un evento. saida tiene 5 bytes. */
void wmi_nome_metodo(const wmi_bloco_t *b, const char *prefixo, char saida[5]);

/* ---------------------------------------------------------------- HP */

#define HPWMI_GUID_BIOS    "5FB7F034-2C63-45E9-BE91-3D44E2C707E4"   /* método: consultas a la BIOS */
#define HPWMI_GUID_EVENTO  "95F24279-4D7B-4334-9387-ACCDC67EF61C"   /* evento: teclas, radios, etc. */

#define HPWMI_FIRMA        0x55434553u   /* «SECU» en el búfer de entrada */
#define HPWMI_LER          1             /* orden: leer */
#define HPWMI_ESCREVER     2             /* orden: escribir */
#define HPWMI_CABECALHO    16            /* firma, orden, tipo, tamaño */
#define HPWMI_RESPOSTA_CAB 8             /* firma y código de la respuesta */

/* Tipos de consulta (CommandType) que usan las BIOS de consumo de HP de 2008-2012. */
#define HPWMI_T_RADIOS     0x05   /* estado de las radios (4 bytes) */
#define HPWMI_T_TECLA      0x0C   /* código de la última tecla del panel (bezel) */

/* Eventos (primer dword de _WED). */
#define HPWMI_EV_ACOPLE       0x01
#define HPWMI_EV_DISCO        0x02   /* aparcar el disco (acelerómetro) */
#define HPWMI_EV_CARGADOR     0x03   /* adaptador «inteligente» */
#define HPWMI_EV_TECLA        0x04   /* tecla del panel: el código con HPWMI_T_TECLA */
#define HPWMI_EV_RADIOS       0x05   /* botón inalámbrico o cambio de radios */
#define HPWMI_EV_CPU_BATERIA  0x06
#define HPWMI_EV_BLOQUEO      0x07
#define HPWMI_EV_TAPA         0x08

/* Las radios: el bit «r» del primer byte dice si la radio existe; el byte r+1 su estado,
 * con 0x02 = encendida por software y 0x08 = encendida por hardware (el botón o el
 * interruptor no la bloquean). Encenderla o apagarla es escribir el dword
 * (máscara << 8) | encendidas. */
#define HPWMI_RADIO_WIFI   0
#define HPWMI_RADIO_BT     1
#define HPWMI_RADIO_WWAN   2
#define HPWMI_RADIOS       3
#define HPWMI_RADIO_SW     0x02
#define HPWMI_RADIO_HW     0x08

typedef struct {
    bool presente, sw, hw;   /* existe; encendida por software; no bloqueada por hardware */
} hpwmi_radio_t;

/* Monta el búfer de entrada (cabecera y n bytes de datos). Devuelve los bytes escritos
 * (16 + n) o 0 si no cabe. */
uint32_t hpwmi_montar(uint8_t *buf, uint32_t max, uint32_t ordem, uint32_t tipo, const void *dados, uint32_t n);
/* Analiza la respuesta: *codigo (0 = bien), *dados y *ndados (lo que va tras los 8 bytes).
 * false si es demasiado corta. */
bool hpwmi_resposta(const uint8_t *buf, uint32_t n, uint32_t *codigo, const uint8_t **dados, uint32_t *ndados);
/* Id de método de WMAD según cuántos bytes de respuesta se esperan: 1 -> 0, 2 -> 4,
 * 3 -> 128, 4 -> 1024, 5 -> 4096 (lo que reservan las BIOS). */
uint32_t hpwmi_id_metodo(uint32_t bytes_resposta);
/* Los 4 bytes del estado -> las tres radios. */
void hpwmi_radios_ler(const uint8_t estado[4], hpwmi_radio_t r[HPWMI_RADIOS]);
/* El dword que pide encender (ligar) o apagar la radio "radio". */
uint32_t hpwmi_radios_pedido(int radio, bool ligar);
/* Los datos de _WED: 8 bytes (evento, dato) o 16 (evento, dato, ...). false si no. */
bool hpwmi_evento_ler(const uint8_t *buf, uint32_t n, uint32_t *evento, uint32_t *dado);

/* Lo que se hace con una tecla del panel (código de HPWMI_T_TECLA). */
enum { HPWMI_ACAO_NADA, HPWMI_ACAO_VOL_MAIS, HPWMI_ACAO_VOL_MENOS, HPWMI_ACAO_SILENCIO, HPWMI_ACAO_AVISO };
typedef struct { uint32_t codigo; int acao; const char *nome; } hpwmi_tecla_t;
/* La entrada de la tabla para ese código, o NULL si no se conoce. */
const hpwmi_tecla_t *hpwmi_tecla(uint32_t codigo);

#endif
