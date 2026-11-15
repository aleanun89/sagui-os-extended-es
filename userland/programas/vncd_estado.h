// SPDX-License-Identifier: GPL-2.0-or-later
/* vncd_estado.h - el archivo de estado que escribe el servicio vncd (D273) y su lector, para
 * «vncd --estado», la ventana indicadora y la pestaña «Escritorio remoto» de Configuración.
 *   vncd PID PUERTO local|red-local SOLO_VER(0|1) CLAVE(0|1)
 *   sesion PID IP DESDE_S SOLO_VER(0|1)        (una por cada cliente ya autenticado; DESDE_S = tempo_ms()/1000)
 *   fin */
#ifndef SAGUI_VNCD_ESTADO_H
#define SAGUI_VNCD_ESTADO_H
#include "sagui.h"
#include "servicos.h"

#define VNCD_ARQ_ESTADO SERV_DIR_COMUM "/vncd.estado"
#define VNCD_MAX_SES    8

typedef struct { int pid, puerto, solo_ver, clave, n; bool red_local; struct { int pid; char ip[16]; unsigned seg; int solo_ver; } s[VNCD_MAX_SES]; } vnc_estado_t;

/* Devuelve false si no hay archivo (el servicio no está en marcha) o está a medias. */
static inline bool vncd_estado_ler(vnc_estado_t *e)
{
    memset(e, 0, sizeof *e);
    int fd = abrir(VNCD_ARQ_ESTADO, ABRIR_LER);
    if (fd < 0) return false;
    static char b[1100];
    int n = ler(fd, b, sizeof b - 1);
    fechar(fd);
    if (n <= 0) return false;
    b[n] = 0;
    bool completo = false;
    for (char *p = b; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *c[6];
        int nc = 0;
        for (char *q = p; *q && nc < 6;) {
            c[nc++] = q;
            while (*q && *q != ' ') q++;
            if (*q) *q++ = 0;
        }
        if (nc >= 6 && compara_texto(c[0], "vncd") == 0) {
            e->pid = texto_para_int(c[1]); e->puerto = texto_para_int(c[2]);
            e->red_local = compara_texto(c[3], "red-local") == 0;
            e->solo_ver = texto_para_int(c[4]); e->clave = texto_para_int(c[5]);
        } else if (nc >= 5 && compara_texto(c[0], "sesion") == 0 && e->n < VNCD_MAX_SES) {
            e->s[e->n].pid = texto_para_int(c[1]);
            copia_texto(e->s[e->n].ip, c[2], sizeof e->s[e->n].ip);
            { unsigned d = (unsigned)texto_para_int(c[3]), ya = tempo_ms() / 1000; e->s[e->n].seg = ya > d ? ya - d : 0; }
            e->s[e->n].solo_ver = texto_para_int(c[4]);
            e->n++;
        } else if (compara_texto(c[0], "fin") == 0) completo = true;
        if (!fim) break;
        p = fim + 1;
    }
    return completo;
}

#endif
