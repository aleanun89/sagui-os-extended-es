// SPDX-License-Identifier: GPL-2.0-or-later
/* energia - salvapantallas, bloqueo y apagado de la pantalla por inactividad, batería y «no molestar» (D422).
 *
 *   energia                 estado: inactividad, qué está en marcha, la configuración que lee el kernel y la batería
 *   energia recargar        vuelve a leer /etc/energia.conf (lo que guarda Configuración > Energía y pantalla)
 *   energia probar [N]      muestra ya el salvapantallas N (0 negro, 1 estrellas, 2 logotipo, 3 tuberías, 4 reloj)
 *   energia actividad       como una tecla: despierta y reinicia la cuenta
 *   energia nomolestar SEG  pide «no molestar» SEG segundos y lo suelta (para probarlo)
 *   energia simular SEG     (administrador) suma SEG segundos de inactividad: para las pruebas
 *   energia bateria PCT [enchufada|desenchufada] | sin | real   (administrador) batería simulada
 *
 * La configuración (de la máquina) está en /etc/energia.conf; se cambia desde Configuración o a mano.
 * El nombre del archivo es el del comando. */
#include "sagui.h"
#include "energia.h"

static const char *const estados[] = { "activo", "salvapantallas", "pantalla apagada" };

static int pedir(unsigned op, unsigned arg)
{
    return info_sistema(INFO_OCIO_CTL, (void *)(unsigned long)OCIO_PEDIDO(op, arg), 0);
}

static int estado(void)
{
    static info_ocio_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_OCIO, &v, sizeof v) != 0 || v.versao != 1) {
        imprimir_em(ERROS, "energia: el kernel no lo sabe\n");
        return 1;
    }
    imprimir("energia: estado %s inactivo %u s no_molestar %u bloqueada %u\n", estados[v.estado < 3 ? v.estado : 0], v.inactivo_s,
             v.no_molestar, v.bloqueada);
    imprimir("energia: configuracion %s salvapantallas %u/%u min bloqueo %u/%u min pantalla bateria %u red %u min\n",
             v.cargado ? "leida" : "por defecto", v.salva_efecto, v.salva_min, v.bloqueo_modo, v.bloqueo_min, v.pantalla_bat_min,
             v.pantalla_red_min);
    imprimir("energia: bateria %s%s pct %d %s aviso %u critica %u accion %u gobernador bat %u red %u\n",
             v.con_bateria ? "si" : "no", v.simulada ? " (simulada)" : "", v.bat_pct, v.enchufado ? "enchufada" : "desenchufada",
             v.bat_aviso, v.bat_critica, v.bat_accion, v.gob_bat, v.gob_red);
    imprimir("energia: veces salvapantallas %u bloqueo %u apagada %u avisos %u\n", v.veces_salva, v.veces_bloqueo, v.veces_apagada,
             v.avisos_bateria);
    return 0;
}

static int resultado(const char *que, int r)
{
    if (r == ERRO_ACESSO) { imprimir_em(ERROS, "energia: %s necesita permisos de administrador\n", que); return 1; }
    if (r < 0) { imprimir_em(ERROS, "energia: %s: error %d\n", que, r); return 1; }
    imprimir("energia: %s: hecho\n", que);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc < 2 || compara_texto(argv[1], "estado") == 0) return estado();
    const char *c = argv[1];
    if (compara_texto(c, "recargar") == 0) return resultado("recargar", pedir(OCIO_OP_RECARGAR, 0));
    if (compara_texto(c, "actividad") == 0) return resultado("actividad", pedir(OCIO_OP_ACTIVIDAD, 0));
    if (compara_texto(c, "probar") == 0) return resultado("probar", pedir(OCIO_OP_PROBAR, argc > 2 ? (unsigned)texto_para_int(argv[2]) : 1));
    if (compara_texto(c, "simular") == 0 && argc > 2) return resultado("simular", pedir(OCIO_OP_SIMULAR, (unsigned)texto_para_int(argv[2])));
    if (compara_texto(c, "nomolestar") == 0 && argc > 2) {
        int r = no_molestar(1);
        if (r < 0) return resultado("nomolestar", r);
        imprimir("energia: no molestar pedido\n");
        dormir((unsigned)texto_para_int(argv[2]) * 1000u);
        return resultado("nomolestar", no_molestar(0));
    }
    if (compara_texto(c, "bateria") == 0 && argc > 2) {
        unsigned arg;
        if (compara_texto(argv[2], "real") == 0) arg = 0xFFFF;
        else if (compara_texto(argv[2], "sin") == 0) arg = 0x200;
        else arg = (unsigned)texto_para_int(argv[2]) | (argc > 3 && compara_texto(argv[3], "enchufada") == 0 ? 0x100u : 0u);
        return resultado("bateria", pedir(OCIO_OP_BATERIA_SIM, arg));
    }
    imprimir_em(ERROS, "uso: energia [estado | recargar | probar [N] | actividad | nomolestar SEG | simular SEG | bateria PCT [enchufada] | sin | real]\n");
    return 1;
}
