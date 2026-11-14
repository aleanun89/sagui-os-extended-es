// SPDX-License-Identifier: GPL-2.0-or-later
/* atapi.h - lectores de CD/DVD (ATAPI), solo lectura (D207).
 *
 * La parte común (órdenes SCSI de 12 bytes, sense, capacidad, la vista en
 * sectores de 512 y el registro en la capa de discos) está en atapi.c. Cada
 * controlador pone el "transporte": cómo se manda un paquete y se reciben los
 * datos (PIO en el IDE, ata.c; FIS con el bit ATAPI en el AHCI, ahci.c). */
#ifndef SAGUI_ATAPI_H
#define SAGUI_ATAPI_H
#include "tipos.h"
#include "disco.h"

/* Resultado de un paquete: >= 0 = bytes recibidos; si no, uno de estos. */
#define ATAPI_FALHA     (-1)   /* el transporte no respondió (tiempo agotado, bus) */
#define ATAPI_CHECK     (-2)   /* el lector dijo ERR (CHECK CONDITION): hay que pedir el sense */

typedef struct atapi atapi_t;

/* Manda el paquete "cdb" (12 bytes) y recibe hasta "bytes" en "buf" (NULL si
 * no hay datos). En ATAPI_CHECK deja el registro de error en a->erro_reg (la
 * sense key va en los 4 bits altos). */
typedef int (*atapi_pacote_t)(atapi_t *a, const uint8_t *cdb, void *buf, uint32_t bytes, uint32_t espera_ms);

struct atapi {
    void          *transporte;   /* el canal IDE o el puerto AHCI */
    int            unidade;      /* IDE: 0 = maestro, 1 = esclavo; AHCI: el puerto */
    atapi_pacote_t pacote;
    bool           pacote16;     /* el lector pide paquetes de 16 bytes (IDENTIFY palabra 0) */
    bool           dma;          /* AHCI: el lector admite DMA (IDENTIFY palabra 49, bit 8) */
    char           onde[24];     /* "IDE 1f0/0", "AHCI puerto 1" (para el log) */
    char           modelo[48];
    uint32_t       blocos;       /* capacidad en bloques de 2048 (0 = sin disco) */
    uint8_t        erro_reg;     /* registro de error del último ATAPI_CHECK */
    uint8_t        sense, asc, ascq;   /* el último sense leído */
    uint8_t       *bloco;        /* un bloque de 2048 para las lecturas sueltas */
    uint32_t       bloco_lba;    /* qué bloque tiene "bloco" (0xFFFFFFFF = ninguno) */
    disco_t       *disco;        /* "cd0" */
    disco_t       *imagem;       /* "cd0img": la imagen El Torito de dentro (o NULL) */
    /* D233: el disco que se mete y se saca con el sistema en marcha. */
    uint8_t        medio;        /* MEDIO_* (atapi.c): lo último que se sabe del lector */
    bool           mudou;        /* el lector dijo "se ha cambiado el disco" (6/28h) y aún no se ha mirado */
    bool           audio;        /* CD de audio (ninguna pista de datos en la tabla de contenido) */
    uint32_t       sessao;       /* bloque donde empieza la última sesión (READ TOC formato 1) */
    uint32_t       assinatura;   /* para saber si tras un 6/28h es el mismo disco */
    uint8_t        falhas;       /* sondeos seguidos sin respuesta del lector */
    uint32_t       proxima;      /* tic del próximo sondeo */
    struct cd_toc *toc;          /* D233: pistas y CD-TEXT del disco (cdda.h) */
    disco_t       *disco_audio;  /* D233: "cd0audio", las pistas de audio como WAV (o NULL) */
    uint16_t       erros_audio;  /* sectores de audio ilegibles en este disco (el log anota 20) */
};

/* Lo llama el driver al encontrar un lector (con el IDENTIFY PACKET ya leído).
 * Copia "a" y registra "cdN"; devuelve el registro o NULL. */
atapi_t *atapi_registrar(const atapi_t *a, const uint16_t *identify);

/* D207: si se arrancó de un CD, monta en /cd el CD del que se arrancó (el que
 * lleva la imagen con la cabecera de este arranque). */
bool atapi_montar_cd_arranque(void);

/* Cuántos lectores de CD/DVD se han registrado. */
int atapi_quantidade(void);

/* Vuelve a mirar el medio (por si se ha cambiado el disco): capacidad e ISO. */
bool atapi_sondar(disco_t *d);

/* D233: tras montar el CD de arranque, monta en /cd un CD de datos que ya esté en
 * un lector y arranca la tarea que mira cada 2 s si se mete o se saca un disco
 * (TEST UNIT READY): monta solo el nuevo en /cd, avisa, y desmonta el que se saca. */
void atapi_iniciar_vigilancia(void);

#endif
