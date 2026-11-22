/* SPDX-License-Identifier: GPL-3.0-only */
/* awk_sagui.h - lo que The One True AWK pide a la libc y SaguiOS no tiene (D440). Se incluye con -include. */
#ifndef AWK_SAGUI_H
#define AWK_SAGUI_H
#include <signal.h>
#ifndef FPE_FLTUND
#define FPE_FLTUND 5
#endif
#ifndef FPE_FLTRES
#define FPE_FLTRES 6
#endif
#ifndef FPE_FLTINV
#define FPE_FLTINV 7
#endif
#ifndef FPE_FLTSUB
#define FPE_FLTSUB 8
#endif
#endif
