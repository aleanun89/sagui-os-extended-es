// SPDX-License-Identifier: GPL-2.0-or-later
/* des.h - DES (FIPS 46-3) para la autenticación «VNC Authentication» del RFB
 * (RFC 6143, 7.2.2), mejora 097 (D252). Solo se usa para eso: DES es débil y no
 * debe servir para proteger nada más. Código propio. */
#ifndef SAGUI_DES_H
#define SAGUI_DES_H

#include "sagui.h"

/* Cifra (cifrar = true) o descifra un bloque de 8 bytes con la clave de 8 bytes
 * tal cual (los bits de paridad se ignoran). */
void des_bloque(const uint8_t clave[8], const uint8_t ent[8], uint8_t sal[8], bool cifrar);

/* La clave del RFB: la contraseña rellena con ceros (o cortada) a 8 bytes y con
 * los bits de cada byte en orden inverso (el bit menos significativo primero). */
void vnc_clave_des(const char *contrasena, uint8_t clave[8]);

/* Respuesta a un reto de 16 bytes: los dos bloques cifrados con DES en modo ECB. */
void vnc_respuesta(const char *contrasena, const uint8_t reto[16], uint8_t resp[16]);

#endif
