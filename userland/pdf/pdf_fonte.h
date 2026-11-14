// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_fonte.h - tipografías del motor de PDF (mejora 059, D138).
 *
 * El PDF guarda las fuentes de dos formas:
 *
 *   incrustadas  un programa de fuente dentro del PDF: TrueType (FontFile2),
 *                CFF/OpenType (FontFile3) o Type 1 (FontFile). Son estas las
 *                que tienen los glifos de verdad, y son las que se dibujan.
 *   las 14 basicas  Helvetica, Times, Courier y sus variantes: no van
 *                incrustadas dentro del PDF, y hay que dibujarlas con una
 *                fuente propia del sistema.
 *
 * Este modulo dibuja el texto: recibe los codigos de la cadena y devuelve
 * los contornos de los glifos (en el sistema de coordenadas de la fuente,
 * 1/1000 de em) y sus anchos.
 *
 * Que se dibuje el glifo de verdad (contorno vectorial) o con el bitmap del
 * sistema depende de si la fuente va incrustada. */
#ifndef SAGUI_PDF_FONTE_H
#define SAGUI_PDF_FONTE_H
#include "sagui.h"
#include "pdf_geom.h"

/* Los tipos de fuente incrustada que se saben leer. */
#define PDF_FONTE_TRUETYPE  1   /* FontFile2: glifos 'glyf' */
#define PDF_FONTE_CFF       2   /* FontFile3 (Type1C u OpenType): charstrings CFF */
#define PDF_FONTE_TYPE1     3   /* FontFile: charstrings Type 1 */

/* Una fuente cargada. */
typedef struct pdf_fonte pdf_fonte_t;

/* Carga una fuente incrustada. 'dados' es el programa de fuente ya
 * descomprimido (el contenido del stream de FontFile). Devuelve NULL si el
 * programa no se puede leer. */
pdf_fonte_t *pdf_fonte_carregar(int tipo, const uint8_t *dados, int tam);
void         pdf_fonte_liberar(pdf_fonte_t *f);

/* Las 14 fuentes basicas: se cargan por nombre ("Helvetica", "Times-Roman",
 * "Courier-Bold"...). Devuelve NULL si el nombre no es una de las basicas.
 * Se dibujan con los contornos que trae el sistema (no hay glifos reales
 * incrustados en el PDF). */
pdf_fonte_t *pdf_fonte_basica(const char *nome);

/* El nombre de la fuente, para la informacion de la pagina. */
const char *pdf_fonte_nome(const pdf_fonte_t *f);

/* Un glifo: su contorno (en 1/1000 de em, con la Y hacia arriba) y su ancho
 * (en 1/1000 de em). El llamante se queda con el camino. */
typedef struct {
    pdf_caminho_t contorno;   /* puede estar vacio (un espacio) */
    int           largura;    /* en 1/1000 de em */
} pdf_glifo_t;

/* El glifo de un codigo de caracter, con la codificacion de la fuente ya
 * aplicada (el PDF puede redefinir los codigos con /Differences). */
bool pdf_fonte_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *glifo);
void pdf_glifo_liberar(pdf_glifo_t *g);

/* El glifo de un CID de una fuente Type0 (CIDFontType0/2). En TrueType
 * 'cid' debe venir ya traducido por el /CIDToGIDMap (es el glifo). */
bool pdf_fonte_glifo_cid(pdf_fonte_t *f, int cid, pdf_glifo_t *glifo);

/* El ancho de un codigo de caracter (en 1/1000 de em), sin dibujar el
 * glifo. Lo usan los ajustes de texto (kerning, espaciado). */
int pdf_fonte_largura(pdf_fonte_t *f, int codigo);

/* Aplica la tabla /Differences de la fuente: asigna un nombre de glifo a un
 * codigo. Se llama antes de pdf_fonte_glifo. */
void pdf_fonte_differences(pdf_fonte_t *f, int codigo, const char *nome_glifo);

/* El alto y el bajo de la fuente (en 1/1000 de em), para la linea base. */
int pdf_fonte_ascendente(pdf_fonte_t *f);
int pdf_fonte_descendente(pdf_fonte_t *f);

#endif
