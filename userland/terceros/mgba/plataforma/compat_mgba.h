// SPDX-License-Identifier: GPL-2.0-or-later
/* compat_mgba.h - lo que le falta a la libc del SaguiOS para compilar mGBA (D322); se fuerza con -include. */
#ifndef COMPAT_MGBA_H
#define COMPAT_MGBA_H
#include <inttypes.h>
#ifndef PRIXPTR
#define PRIXPTR "X"
#endif
#ifndef PRIxPTR
#define PRIxPTR "x"
#endif
#ifndef PRIuPTR
#define PRIuPTR "u"
#endif
#endif
