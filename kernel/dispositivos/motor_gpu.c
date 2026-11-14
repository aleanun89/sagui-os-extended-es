// SPDX-License-Identifier: GPL-2.0-or-later
/* motor_gpu.c - lo que el kernel sabe del motor 3D por la GPU (D158).
 *
 * El kernel no sabe acelerar: eso es cosa del programa (userland/libamd).
 * Lo que sí hace el kernel es:
 *
 *   - decir si la aceleración está permitida: la opción de arranque
 *     SGL_MOTOR=software la quita para todo el sistema (D158);
 *   - decir si hay una GPU AMD en el bus PCI y con qué identificador y
 *     revisión (eso sale del espacio de configuración, que el kernel ya
 *     sabía leer);
 *   - decir cuál es el plazo del vigilante de la GPU, para que el programa y
 *     el kernel coincidan.
 *
 * Los registros MMIO de la GPU, la familia y el motor los lee el programa,
 * mapeando el BAR con SIS_MAPEAR_BAR (kernel/dispositivos/bar.c). */
#include "bootinfo.h"
#include "pci.h"
#include "sistema.h"
#include "texto.h"
#include "tipos.h"

#define FABRICANTE_AMD 0x1002
#define CLASSE_VIDEO   0x03

int32_t motor_gpu_info(uint32_t ptr, uint32_t n)
{
    info_motor_gpu_t *v = (info_motor_gpu_t *)ptr;
    if (n < sizeof *v || !usuario_ok((void *)ptr, sizeof *v, true)) return ERRO_ENDERECO;

    memset(v, 0, sizeof *v);
    v->versao = 1;
    v->estado = MOTOR_GPU_SOFTWARE;
    v->watchdog_ms = MOTOR_GPU_VIGILANTE_MS;
    v->boot_opcoes = info_boot.opcoes;

    /* La opción de arranque manda sobre todo lo demás. */
    if (info_boot.opcoes & OPC_BOOT_SGL_SOFTWARE) {
        v->desativado = 1;
        v->estado = MOTOR_GPU_DESATIVADO;
        txt_copia_n(v->motivo, "la opcion de arranque pidio solo el motor por software",
                    sizeof v->motivo);
    }

    /* ¿Hay alguna GPU AMD? El PCI lo sabe sin mapear nada. */
    static pci_disp_t achados[8];
    int q = pci_procurar_classe(CLASSE_VIDEO, 0x00, achados, 8);
    for (int i = 0; i < q; i++) {
        if (achados[i].fabricante != FABRICANTE_AMD) continue;
        v->gpu_amd = 1;
        v->modelo = achados[i].modelo;
        v->revisao = pci_le32(&achados[i], 0x08) & 0xFF;   /* revision id */
        break;
    }
    if (!v->desativado) {
        if (v->gpu_amd) {
            v->aceleracao = 1;
            v->estado = MOTOR_GPU_POR_HARDWARE;
            txt_copia_n(v->motivo, "", sizeof v->motivo);
        } else {
            txt_copia_n(v->motivo, "no hay ninguna GPU AMD en el bus PCI", sizeof v->motivo);
        }
    }
    return 0;
}
