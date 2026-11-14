// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_cripto.h - cifrado de los PDF protegidos (mejora 059, D138).
 *
 * Los PDF pueden ir cifrados (el "standard security handler" de la norma
 * ISO 32000-1, apartado 7.6). Hay tres metodos:
 *
 *   V1/V2  RC4 de 40 o 128 bits (RC4 es un cifrado de flujo: XOR con la
 *          keystream)
 *   V4     AES-128 en modo CBC (o RC4, si el diccionario lo pide)
 *   V5/V6  AES-256 en modo CBC (V6 anade el hash 2.B de la norma)
 *
 * Todo se ha escrito aqui a partir de la norma, sin codigo ajeno. El MD5
 * (RFC 1321) hace falta para el algoritmo 2 (derivar la clave) y el RC4 sale
 * de la descripcion clasica del cifrado de flujo.
 *
 * La clave de un objeto depende de su numero de objeto (algoritmo 1 de la
 * norma), asi que el descifrado se pide objeto a objeto. */
#ifndef SAGUI_PDF_CRIPTO_H
#define SAGUI_PDF_CRIPTO_H
#include "sagui.h"

/* Los errores del descifrado. */
#define PDF_CRIPTO_OK        0
#define PDF_CRIPTO_MAL       (-1)   /* datos que no son lo que deberian */
#define PDF_CRIPTO_SOPORTE  (-2)   /* metodo de cifrado que no se sabe */

/* MD5: 16 bytes de digest. */
void pdf_md5(const uint8_t *dados, int n, uint8_t *saida);
/* MD5 de un bloque con su longitud (el "MD5 de la norma 32000-1" usa el
 * bloque sin el relleno de longitud). */
void pdf_md5_bloco(const uint8_t *dados, int n, uint8_t *saida);

/* RC4: XOR con la keystream. */
void pdf_rc4(const uint8_t *chave, int nchave, const uint8_t *ent, int n, uint8_t *saida);

/* AES-128/256 en modo CBC, solo descifrado (el AES del proyecto, en
 * userland/libtls/aes.c, solo cifra: el GCM usa CTR y no necesita el
 * inverso). El padding del PDF es PKCS#5 y se quita al final. */
int pdf_aes_cbc_descifrar(const uint8_t *chave, int nchave, const uint8_t iv[16],
                          const uint8_t *ent, int n, uint8_t *saida);

/* El contexto de un documento cifrado. */
typedef struct {
    int      metodo;      /* 0 = sin cifrar, 1 = RC4, 2 = AES-128, 3 = AES-256 */
    int      versao;      /* V = 1, 2, 4, 5 */
    int      r;           /* la "revision" (R) del gestor: 2, 3, 4, 5, 6 */
    uint8_t  chave[32];   /* la clave de archivo (32 bytes para AES-256) */
    int      nchave;
    bool     cifrar_meta; /* /EncryptMetadata false */
} pdf_cripto_t;

/* Calcula la clave de un objeto a partir de la clave de archivo y del numero
 * de objeto (algoritmo 1 de la norma 32000-1, apartado 7.6.2). */
void pdf_cripto_chave_objeto(const pdf_cripto_t *c, int num, int gen, uint8_t *saida, int *n);

/* Prepara el cifrado de un documento: deriva la clave de archivo a partir de
 * la contrasena del usuario (algoritmo 2 de la norma, y sus variantes R5/R6).
 * Los parametros salen del diccionario /Encrypt y del /ID del trailer:
 *
 *   o, n_o   : los bytes de la cadena /O
 *   u, n_u   : los bytes de la cadena /U
 *   p        : el entero /P (con signo, tal cual esta en el PDF)
 *   v, r     : /V y /R
 *   length   : /Length en bits (128 si no esta)
 *   id, n_id : la primera cadena del /ID del trailer
 *   cifrar_meta : el /EncryptMetadata (true si no esta)
 *
 * Devuelve PDF_CRIPTO_OK si la contrasena es la correcta, PDF_CRIPTO_MAL si no
 * lo es y PDF_CRIPTO_SOPORTE si el metodo de cifrado no se sabe descifrar.
 * La interfaz es plana a proposito: este modulo no depende del analizador. */
int pdf_cripto_preparar(pdf_cripto_t *c, const char *contrasena,
                        const uint8_t *o, int n_o,
                        const uint8_t *u, int n_u,
                        int p, int v, int r, int length,
                        const uint8_t *id, int n_id,
                        bool cifrar_meta);

/* Como pdf_cripto_preparar, mas para el V=5 (revisiones 5 y 6, AES-256).
 * Aqui la clave de archivo va cifrada en el /OE y en el /UE del diccionario
 * /Encrypt, asi que hacen falta esos dos campos (y el /ID no se usa).
 * Los mismos valores de retorno: PDF_CRIPTO_OK, PDF_CRIPTO_MAL o
 * PDF_CRIPTO_SOPORTE. */
int pdf_cripto_preparar_v5(pdf_cripto_t *c, const char *contrasena,
                           const uint8_t *o, int n_o,
                           const uint8_t *u, int n_u,
                           const uint8_t *oe, int n_oe,
                           const uint8_t *ue, int n_ue,
                           int p, int v, int r, bool cifrar_meta);

/* Descifra un objeto (una cadena o el contenido de un stream). Los datos
 * que no son del objeto (el diccionario, por ejemplo) NO se descifran: el
 * llamante decide. */
int pdf_cripto_descifrar(const pdf_cripto_t *c, uint8_t *dados, int n, int num, int gen);

#endif
