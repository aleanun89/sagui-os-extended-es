// SPDX-License-Identifier: GPL-2.0-or-later
/* zonas_termicas.c - las zonas térmicas del ACPI (ThermalZone, _TMP), para enseñarlas junto a la GPU (D399).
 *
 * Solo con uACPI (imágenes --uacpi). Corre en la tarea «acpi-aml», la única que llama a uACPI
 * (uacpi_sagui.h): busca las zonas una vez y lee _TMP (décimas de kelvin) cada 10 s, pero solo
 * mientras alguien mira (INFO_GPU_ENERGIA en los últimos 30 s), como los sensores (D092): el _TMP
 * de un portátil suele leer el controlador embebido. Una zona del ACPI NO es la temperatura de la
 * tarjeta: Tareas y Configuración la enseñan aparte, con el nombre que le da el firmware. */
#include "gpu_energia.h"
#include "console.h"
#include "cpu.h"
#include "relogio.h"
#include "texto.h"
#include "uacpi_sagui.h"


extern volatile uint32_t gpu_energia_pedido_s;
static uacpi_namespace_node *zonas[4];
static char nomes[4][8];
static int mili[4];
static bool valida[4];
static int nz;
static bool lidas;
static uint32_t ultimo_s;

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    if (nz >= 4) return UACPI_ITERATION_DECISION_BREAK;
    uacpi_object_name n = uacpi_namespace_node_name(no);
    zonas[nz] = no;
    for (int i = 0; i < 4; i++) nomes[nz][i] = n.text[i] >= ' ' && n.text[i] < 127 ? n.text[i] : '?';
    nomes[nz][4] = 0;
    nz++;
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void zonas_termicas_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_THERMAL_ZONE_BIT, UACPI_MAX_DEPTH_ANY, NULL);
    klog("[GPUENERGIA] ACPI: %d zona(s) termica(s)%s%s%s%s%s\n", nz, nz ? ":" : "", nz > 0 ? " " : "", nz > 0 ? nomes[0] : "",
         nz > 1 ? " " : "", nz > 1 ? nomes[1] : "");
}

void zonas_termicas_ciclo(void)
{
    if (!nz) return;
    uint32_t agora = relogio_segundos();
    if (lidas && agora - gpu_energia_pedido_s > 30) return;   /* nadie mira */
    if (lidas && agora - ultimo_s < 10) return;
    ultimo_s = agora;
    for (int i = 0; i < nz; i++) {
        uacpi_u64 v = 0;
        bool ok = uacpi_eval_simple_integer(zonas[i], "_TMP", &v) == UACPI_STATUS_OK && v > 2000 && v < 4732;   /* -73 a 200 C */
        bool i0 = int_salva_desliga();
        valida[i] = ok;
        if (ok) mili[i] = (int)v * 100 - 273150;
        int_restaura(i0);
        if (!lidas) klog("[GPUENERGIA] ACPI: zona %s: %s%d mC\n", nomes[i], ok ? "" : "sin _TMP valido ", ok ? mili[i] : 0);
    }
    lidas = true;
}

int zonas_termicas_ler(int *mc, char (*nome)[8], int max)
{
    int k = 0;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < nz && k < max; i++) {
        if (!valida[i]) continue;
        mc[k] = mili[i];
        txt_copia_n(nome[k], nomes[i], 8);
        k++;
    }
    int_restaura(i0);
    return k;
}
