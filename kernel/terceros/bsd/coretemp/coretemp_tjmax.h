// SPDX-License-Identifier: BSD-2-Clause
/*
 * Criterio de TjMax por modelo de CPU Intel sin el MSR 0x1A2, según el
 * driver coretemp(4) de FreeBSD (sys/dev/coretemp/coretemp.c, coretemp_attach).
 *
 * Copyright (c) 2007, 2008 Rui Paulo <rpaulo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * SaguiOS (D259): solo la decisión por modelo (la cadena de if de
 * coretemp_attach), reescrita en C freestanding con la firma que usa
 * kernel/lib/sensores_conv.c. Ver docs/terceros.md.
 */
#ifndef TERCEIROS_BSD_CORETEMP_TJMAX_H
#define TERCEIROS_BSD_CORETEMP_TJMAX_H

/* TjMax en milésimas de grado de las CPU sin el MSR 0x1A2: valor inicial de
 * 100 C (el de FreeBSD); modelos 0x0E y 0x0F (paso 2 o más): 85 C si el bit
 * 30 del MSR 0xEE lo dice (ee_85; el llamador no lo lee en los móviles);
 * modelo 0x17 paso 6 (Core 2 Duo móvil): 105 C; modelo 0x1C (Atom): 100 C
 * en el paso 10 (D400, N400, D500) y 90 C en los demás. */
static inline int tabela_bsd_intel_tjmax(uint32_t m, uint32_t stepping, bool movel, bool ee_85)
{
    (void)movel;
    if ((m == 0x0F && stepping >= 2) || m == 0x0E) return ee_85 ? 85000 : 100000;
    if (m == 0x17) return stepping == 6 ? 105000 : 100000;
    if (m == 0x1C) return stepping == 10 ? 100000 : 90000;
    return 100000;
}

#endif
