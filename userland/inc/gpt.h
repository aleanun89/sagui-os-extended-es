// SPDX-License-Identifier: GPL-2.0-or-later
/* gpt.h - tablas de particiones GPT: lectura con comprobación de CRC y escritura
 * completa (cabecera principal, cabecera de respaldo, entradas y MBR protector) (D266).
 *
 * Lo usan "particiones", la ventana de "discos" y el instalador. Todo va por
 * SIS_DISCO_BLOCOS; para escribir, el disco tiene que estar reservado con
 * DBLOCO_CONF_GPT (el kernel, si no, rechaza cualquier escritura en la GPT).
 *
 * Límites (los del kernel, D179): sectores de 512 bytes, direcciones de 32 bits (2 TB)
 * y solo las cuatro primeras entradas usadas se ven como particiones (hdb1..hdb4: el
 * número es el de la ENTRADA, empezando en 1). Las demás se conservan intactas. */
#ifndef SAGUI_GPT_H
#define SAGUI_GPT_H
#include "sagui.h"

#define GPT_ENTRADAS   128u
#define GPT_TAM_ENT    128u
#define GPT_SETORES_ENT 32u          /* GPT_ENTRADAS * GPT_TAM_ENT / 512 */
#define GPT_VISIVEIS   4             /* las que ve el kernel como particiones */
#define GPT_MB         2048u         /* alineación: 1 MiB */

enum { GPT_PRINCIPAL = 1, GPT_RESPALDO = 2 };   /* de cuál de las dos cabeceras se leyó */

typedef struct {
    char     disco[12];
    uint32_t setores;                /* tamaño del disco */
    uint32_t esc;                    /* D283: sectores de 512 por bloque real (1; 8 en un NVMe de bloques de 4096). Las
                                      * direcciones de la API van siempre en sectores de 512; esc solo cuenta al leer y escribir el disco */
    uint8_t  guid[16];               /* GUID del disco */
    uint32_t primeiro, ultimo;       /* primer y último sector utilizables */
    int      origem;                 /* GPT_PRINCIPAL o GPT_RESPALDO */
    bool     respaldo_ok;            /* leída la principal: ¿es válida también la de respaldo? */
    uint8_t  ent[GPT_ENTRADAS * GPT_TAM_ENT];
} gpt_t;

/* GUID de tipo, tal como van en el disco (mezcla de extremos). */
extern const uint8_t GPT_TIPO_ESP[16];        /* C12A7328-F81F-11D2-BA4B-00A0C93EC93B */
extern const uint8_t GPT_TIPO_LINUX[16];      /* 0FC63DAF-8483-4772-8E79-3D69D8477DE4 */
extern const uint8_t GPT_TIPO_DADOS_MS[16];   /* EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 */
extern const uint8_t GPT_TIPO_SAGUI[16];      /* 53414755-494F-4F53-8001-534147554953: partición de sistema de SaguiOS */

uint32_t gpt_crc(const void *dados, size_t n);

/* Lee la GPT validando firma, revisión, CRC de la cabecera y de las entradas. Si la principal no
 * vale, usa la de respaldo (g->origem lo dice). Error (ERRO_FORMATO...) con el motivo en erro. */
int  gpt_ler(const char *disco, gpt_t *g, char *erro, int max);
/* Una tabla vacía para un disco de "setores" sectores, con GUID de disco aleatorio. */
void gpt_nova(gpt_t *g, const char *disco, uint32_t setores);

bool        gpt_usada(const gpt_t *g, int slot);          /* slot 0..127 */
uint32_t    gpt_inicio(const gpt_t *g, int slot);
uint32_t    gpt_setores_ent(const gpt_t *g, int slot);    /* 0 si pasa de los 32 bits */
bool        gpt_alem_32bits(const gpt_t *g, int slot);
const uint8_t *gpt_tipo(const gpt_t *g, int slot);
void        gpt_nome_ent(const gpt_t *g, int slot, char *s, int max);   /* nombre UTF-16 -> Latin-1 */
int         gpt_num_usadas(const gpt_t *g);
int         gpt_numero_kernel(const gpt_t *g, int slot);  /* 1.. si el kernel la ve como partición, si no 0 */
const char *gpt_nome_tipo(const uint8_t *guid);           /* "EFI (sistema)", ... o NULL */
void        gpt_guid_texto(const uint8_t *guid, char *s, int max);
uint8_t     gpt_tipo_mbr(const uint8_t *guid);            /* el tipo equivalente del MBR (0x99 = otro) */
const uint8_t *gpt_guid_de_tipo_mbr(uint8_t tipo);        /* NULL si no hay equivalente (extendida...) */

/* Operaciones en memoria; devuelven el slot (adicionar) o 0, o un error con el motivo. */
int  gpt_adicionar(gpt_t *g, const uint8_t *tipo, uint32_t inicio, uint32_t setores, const char *nome, char *erro, int max);
int  gpt_remover(gpt_t *g, int slot, char *erro, int max);
bool gpt_validar(gpt_t *g, char *erro, int max);          /* fija primeiro/ultimo según el disco */

/* Escribe todo: entradas y cabecera de respaldo, entradas y cabecera principal y, si
 * reescribir_mbr o el MBR actual no es protector, el MBR protector (con "codigo" en sus 446
 * primeros bytes, o ceros si es NULL). Después relee y comprueba CRC y contenido, y manda al
 * kernel releer la tabla. El disco tiene que estar reservado con DBLOCO_CONF_GPT. */
int  gpt_gravar(gpt_t *g, bool reescrever_mbr, const uint8_t *codigo, char *erro, int max);

#endif
