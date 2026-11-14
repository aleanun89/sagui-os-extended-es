// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_lex.h - léxico y objetos del PDF (mejora 059, D138).
 *
 * Un PDF es un fichero de objetos: un PDF puede tener objetos dentro de
 * streams, asi que no se puede leer de un tirón. Este modulo solo ofrece
 * las piezas para leerlos:
 *
 *   - el lexico: nombres, cadenas, numeros, diccionarios y arreglos;
 *   - la tabla de referencias cruzadas (xref), que dice donde esta cada
 *     objeto dentro del fichero;
 *   - los streams: los bytes de un objeto con su filtro (FlateDecode,
 *     LZWDecode, ASCIIHexDecode, ASCII85Decode, RunLengthDecode,
 *     DCTDecode) ya descomprimidos.
 *
 * Los objetos se guardan en una tabla plana (por numero de objeto), con un
 * cache de los que ya se han leido. */
#ifndef SAGUI_PDF_LEX_H
#define SAGUI_PDF_LEX_H
#include "sagui.h"
#include "pdf_geom.h"

/* Los tipos de objeto del PDF. */
typedef enum {
    PDF_OBJ_NULO,
    PDF_OBJ_BOOL,
    PDF_OBJ_NUMERO,
    PDF_OBJ_CADENA,     /* (texto) o <hex> */
    PDF_OBJ_NOME,       /* /Nombre */
    PDF_OBJ_ARREGLO,
    PDF_OBJ_DICCIONARIO,
    PDF_OBJ_STREAM,     /* un diccionario + bytes */
    PDF_OBJ_REFERENCIA  /* "n g R" */
} pdf_obj_tipo_t;

typedef struct pdf_obj pdf_obj_t;

/* Un valor de un diccionario: el nombre y el valor. */
typedef struct {
    char       *chave;      /* el nombre, sin la "/" */
    pdf_obj_t  *valor;
} pdf_entrada_t;

struct pdf_obj {
    pdf_obj_tipo_t tipo;
    bool      booleano;
    bool      inteiro;    /* el numero era entero (para las referencias) */
    double    numero;
    char     *cadena;      /* PDF_OBJ_CADENA: los bytes (con su \0 al final) */
    int       tam_cadena;  /* longitud real, sin contar el \0 */
    char     *nome;        /* PDF_OBJ_NOME: el nombre, sin la "/" */
    pdf_obj_t **itens;     /* PDF_OBJ_ARREGLO */
    int       n_itens;
    pdf_entrada_t *entradas;  /* PDF_OBJ_DICCIONARIO */
    int       n_entradas;
    uint8_t   *datos;      /* PDF_OBJ_STREAM: los bytes (ya descomprimidos) */
    int       tam_datos;
    int       num, gen;    /* PDF_OBJ_REFERENCIA */
};

/* Un documento abierto. */
typedef struct pdf_doc pdf_doc_t;

/* Abre el PDF y lee la xref. *erro recibe un PDF_ERRO_*. */
pdf_doc_t *pdf_doc_abrir(const char *caminho, int *erro);
/* Abre con contrasena (para los PDF cifrados). */
pdf_doc_t *pdf_doc_abrir_com_contrasena(const char *caminho, const char *contrasena, int *erro);
void      pdf_doc_fechar(pdf_doc_t *d);

/* El numero de objetos del documento (el mas alto + 1). */
int pdf_doc_num_objetos(const pdf_doc_t *d);

/* El objeto numero 'num'. Devuelve NULL si no existe. El objeto pertenece
 * al documento (no hay que liberarlo). */
pdf_obj_t *pdf_doc_objeto(pdf_doc_t *d, int num);

/* Busca una clave en un diccionario. Devuelve NULL si no esta. */
pdf_obj_t *pdf_dict_buscar(const pdf_obj_t *dict, const char *chave);
/* Lo mismo, pero exigiendo un tipo. */
pdf_obj_t *pdf_dict_buscar_tipo(const pdf_obj_t *dict, const char *chave, pdf_obj_tipo_t tipo);
/* El numero de un objeto referenciado ("n g R"), o -1. */
int pdf_obj_numero(const pdf_obj_t *o);

/* Descomprime un FlateDecode (zlib o DEFLATE crudo). Devuelve en *saida un
 * buffer nuevo (con un 0 de mas al final) o false. Tolera flujos cortados. */
bool pdf_inflar(const uint8_t *ent, int n_ent, uint8_t **saida, int *tam);

/* El catalogo (el /Root del trailer). */
pdf_obj_t *pdf_doc_catalogo(pdf_doc_t *d);
/* El numero de paginas (del arbol de paginas). */
int pdf_doc_paginas(const pdf_doc_t *d);
/* La pagina numero 'i' (0 = la primera). Devuelve NULL si no hay. */
pdf_obj_t *pdf_doc_pagina(pdf_doc_t *d, int i);
/* Un objeto del /Info (metadatos). */
pdf_obj_t *pdf_doc_info(pdf_doc_t *d);
/* Los metadatos, ya en UTF-8 ("" si no hay). */
const char *pdf_doc_titulo(const pdf_doc_t *d);
const char *pdf_doc_autor(const pdf_doc_t *d);
const char *pdf_doc_produtor(const pdf_doc_t *d);
/* La cadena de un objeto de texto (metadatos). Devuelve "" si no hay. */
const char *pdf_doc_texto(pdf_doc_t *d, pdf_obj_t *obj);

/* El ancho y el alto de una pagina, en puntos. Se leen del /MediaBox (con
 * el /CropBox si hay) y se invierten si el /Rotate es 90 o 270. */
int pdf_doc_pagina_largura(pdf_doc_t *d, int i);
int pdf_doc_pagina_altura(pdf_doc_t *d, int i);

/* Si el documento esta cifrado y hace falta contrasena. */
bool pdf_doc_contrasena_necesaria(pdf_doc_t *d);

#endif
