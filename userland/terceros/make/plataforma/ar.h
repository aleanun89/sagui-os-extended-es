/* SPDX-License-Identifier: GPL-3.0-only */
/* ar.h - formato de los archivos «ar» (lo que lee arscan.c de GNU make para «lib.a(obj.o)»). */
#ifndef SAGUI_AR_H
#define SAGUI_AR_H
#define ARMAG  "!<arch>\n"
#define SARMAG 8
#define ARFMAG "`\n"
struct ar_hdr {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6], ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
};
#endif
