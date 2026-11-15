// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf.h - motor de PDF del SaguiOS (mejora 059, D138).
 *
 * Abre un documento PDF y lo dibuja pagina a pagina. El motor vive en su
 * propia carpeta (userland/pdf/) porque usa coma flotante: la libsagui comun
 * se compila con -mgeneral-regs-only (D076) y no puede, pero un programa con
 * carpeta propia si puede (como sagui3d con SSE2 o cc con la x87).
 *
 * Aqui solo hay enteros: el motor dibuja en un bucle RGBA de 4 bytes por
 * pixel y el programa lo compone sobre el fondo de la ventana.
 *
 * Uso:
 *
 *   int erro;
 *   pdf_t *d = pdf_abrir("/usb/libro.pdf", &erro);
 *   if (!d) { ... pdf_erro_texto(erro) ... }
 *   int n = pdf_paginas(d);
 *   uint8_t *buf = aloca((size_t)larg * alt * 4);
 *   pdf_render(d, 0, larg, alt, buf);
 *   ...
 *   pdf_fechar(d);
 *
 * El motor no dibuja nada en la pantalla: solo escribe los pixeles del
 * recuadro que le pidan, asi que el programa puede dibujar solo la parte
 * visible de la pagina (lo que se ve con el zoom puesto). */
#ifndef SAGUI_PDF_H
#define SAGUI_PDF_H
#include "sagui.h"

/* Errores propios (los de archivo son los ERRO_* del sistema). */
#define PDF_ERRO_FORMATO    (-230)   /* no parece un PDF */
#define PDF_ERRO_CORROMPIDO (-231)   /* datos invalidos o archivo cortado */
#define PDF_ERRO_MEMORIA    (-232)   /* no hay memoria para el documento */
#define PDF_ERRO_CIFRADO    (-233)   /* esta cifrado y no se pudo abrir */
#define PDF_ERRO_PAGINA     (-234)   /* no hay esa pagina */
#define PDF_ERRO_SEM_FONTE  (-235)   /* falta una fuente y no se puede seguir */

typedef struct pdf_doc pdf_t;

/* Abre el documento y lee su estructura (indice, paginas y metadatos).
 * Devuelve NULL con *erro = un PDF_ERRO_* (o un ERRO_* del sistema si el
 * archivo no se pudo abrir). */
pdf_t *pdf_abrir(const char *caminho, int *erro);
void   pdf_fechar(pdf_t *d);

int         pdf_paginas(const pdf_t *d);          /* numero de paginas */
const char *pdf_titulo(const pdf_t *d);           /* metadato Title */
const char *pdf_autor(const pdf_t *d);            /* metadato Author */
const char *pdf_produtor(const pdf_t *d);         /* metadato Producer */

/* Ancho y alto de una pagina, en puntos de PDF (1/72 de pulgada).
 * Sirve para saber cuanto ocupa antes de dibujar. */
int pdf_pagina_largura(pdf_t *d, int pagina);
int pdf_pagina_altura(pdf_t *d, int pagina);

/* Dibuja la pagina (0 = la primera) en un bucle RGBA de 4 bytes por pixel
 * (R, G, B, A), de largo x alto. Devuelve 0 si se dibujo, o un PDF_ERRO_*.
 * El fondo se deja transparente (A = 0) donde no se dibujo nada. */
int pdf_render(pdf_t *d, int pagina, int largo, int alto, uint8_t *rgba);

/* True si el documento se puede abrir sin contrasena (lo normal). Si hace
 * falta, el programa puede pedirla con pedir_texto() y volver a abrir. */
bool pdf_necesita_contrasena(pdf_t *d);

/* Abre el documento con contrasena (para los PDF cifrados con contrasena de
 * usuario). La contrasena es la que escribe el usuario; se pasa tal cual. */
pdf_t *pdf_abrir_com_contrasena(const char *caminho, const char *contrasena, int *erro);

/* Un carácter del texto de una página (mejora 105): su Unicode (Latin-1 y
 * más) y su caja en décimas de punto, con el origen arriba a la izquierda
 * de la página (y hacia abajo, sin girar). uni = 32 son los espacios que el
 * motor intercala entre palabras y líneas (ancho 0). */
typedef struct { int uni, x, y, l, a; } pdf_car_t;

/* El texto de la página, en el orden del documento. Devuelve 0 y un búfer
 * (se libera con libera()) con *n caracteres, o un PDF_ERRO_*. */
int pdf_texto_pagina(pdf_t *d, int pagina, pdf_car_t **cars, int *n);

/* Una entrada del índice (marcadores) del PDF. El título viene en UTF-8
 * (cortado a 95 bytes); pagina = 0.. o -1 si el destino no se resolvió. */
typedef struct { char titulo[96]; int pagina; int nivel; } pdf_marca_t;

/* Devuelve el número de marcadores (0 si no hay) y en *marcas un búfer que
 * se libera con libera(). Orden de lectura, con el nivel de anidación. */
int pdf_indice(pdf_t *d, pdf_marca_t **marcas);

const char *pdf_erro_texto(int erro);

#endif
