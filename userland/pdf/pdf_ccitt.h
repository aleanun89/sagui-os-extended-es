// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_ccitt.h - el filtro CCITTFaxDecode (fax grupo 3 y 4) del motor de PDF.
 *
 * Lo usan los PDF escaneados en blanco y negro. Ver pdf_ccitt.c. */
#ifndef SAGUI_PDF_CCITT_H
#define SAGUI_PDF_CCITT_H
#include "sagui.h"

/* Los /DecodeParms del filtro. */
typedef struct {
    int  k;          /* /K: < 0 grupo 4, 0 grupo 3 1D, > 0 grupo 3 mixto */
    int  colunas;    /* /Columns (1728 por defecto) */
    int  filas;      /* /Rows (0 = hasta que se acaben los datos) */
    bool preto_1;    /* /BlackIs1 */
    bool alinhar;    /* /EncodedByteAlign */
} pdf_ccitt_par_t;

/* Descomprime: *saida (nuevo) recibe la imagen de 1 bit, filas alineadas a
 * byte, negro = 0 (o 1 con /BlackIs1). Si los datos se cortan, se queda lo
 * leido. Devuelve false si no salio ninguna fila. */
bool pdf_ccitt_decodificar(const uint8_t *ent, int n, const pdf_ccitt_par_t *par,
                           uint8_t **saida, int *tam);

#endif
