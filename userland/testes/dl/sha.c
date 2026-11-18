// SPDX-License-Identifier: GPL-2.0-or-later
/* sha.c - libdlsha.so (D378, h385): el SHA-256 de kernel/lib/sha256.c
 * compilado con -fPIC, para medir el coste del código PIC en i386 (la GOT
 * en EBX) frente al mismo código enlazado de forma estática. */
#include "../../../kernel/lib/sha256.c"
