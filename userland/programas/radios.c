// SPDX-License-Identifier: GPL-2.0-or-later
/* radios - estado de las radios (Wi-Fi, Bluetooth, WWAN) de los portátiles HP por WMI, y
 * encenderlas o apagarlas (D405). También lista los bloques WMI (PNP0C14) que tiene la
 * máquina, de cualquier fabricante: sirve para preparar drivers nuevos. */
#include "sagui.h"

static const char *AYUDA =
    "uso: radios [estado | wifi si|no | bluetooth si|no | wwan si|no | wmi]\n"
    "  estado        (por defecto) qué radios hay y si están encendidas o bloqueadas\n"
    "  wifi si|no    enciende o apaga la Wi-Fi (igual bluetooth y wwan)\n"
    "  wmi           los bloques WMI de ACPI que declara la BIOS (GUID, tipo)\n"
    "Necesita una imagen con --uacpi y un portátil HP con WMI (dispositivo PNP0C14).\n"
    "Si el botón inalámbrico bloquea la radio, hay que pulsarlo: el sistema no puede\n"
    "quitar ese bloqueo. En estos HP la tarjeta Wi-Fi desaparece del PCI con la radio\n"
    "apagada: al encenderla, si aparece, hay que reiniciar para usarla.\n";

static const char *const nomes[RADIOS_MAX] = { "Wi-Fi", "Bluetooth", "WWAN" };
static const char *const ordens[RADIOS_MAX] = { "wifi", "bluetooth", "wwan" };

static info_radios_t r;

static int ler_radios(void)
{
    if (info_sistema(INFO_RADIOS, &r, sizeof r) < 0) {
        imprimir_em(ERROS, "radios: el sistema no informa de las radios\n");
        return -1;
    }
    return 0;
}

static int estado(void)
{
    if (ler_radios() < 0) return 1;
    if (r.uacpi != UACPI_ATIVO) {
        imprimir("Radios: no disponible (%s)\n", r.uacpi == UACPI_INICIANDO ? "leyendo el ACPI, prueba dentro de unos segundos"
                                                 : r.nota[0] ? r.nota : "sin intérprete ACPI");
        return 1;
    }
    if (!r.hp) {
        imprimir("Radios: este equipo no tiene el WMI de HP (%u bloques WMI; «radios wmi» los lista)\n", r.n_wmi);
        return 1;
    }
    if (!r.lido) {
        imprimir("Radios: la BIOS no dio el estado (código %u)\n", r.codigo);
        return 1;
    }
    for (int i = 0; i < RADIOS_MAX; i++) {
        if (!(r.presentes & (1u << i))) continue;
        bool sw = r.sw & (1u << i), hw = r.hw & (1u << i);
        imprimir("%-10s %s", nomes[i], sw && hw ? "encendida" : "apagada");
        if (!hw) imprimir(" (bloqueada por el botón inalámbrico)");
        else if (!sw) imprimir(" (apagada por software)");
        imprimir("\n");
    }
    if (!r.presentes) imprimir("La BIOS dice que no hay radios\n");
    if (r.reiniciar && r.pci_novos) imprimir("Reinicia para usar la tarjeta Wi-Fi (%u funciones nuevas en el PCI)\n", r.pci_novos);
    else if (r.reiniciar) imprimir("Reinicia para usar la tarjeta Wi-Fi (no ha aparecido en el PCI)\n");
    if (r.nota[0]) imprimir("Último: %s\n", r.nota);
    imprimir("Eventos WMI: %u; teclas del panel: %u", r.eventos, r.teclas);
    if (r.teclas) imprimir(" (última %x)", r.ultima_tecla);
    imprimir("\n");
    return 0;
}

static int wmi(void)
{
    if (ler_radios() < 0) return 1;
    if (!r.n_wmi) {
        imprimir("No hay bloques WMI (%s)\n", r.uacpi == UACPI_ATIVO ? "la BIOS no declara ningún PNP0C14" : "sin intérprete ACPI");
        return 1;
    }
    imprimir("%u bloques WMI:\n", r.n_wmi);
    for (unsigned i = 0; i < r.n_wmi && i < RADIOS_WMI_MAX; i++) {
        const info_wmi_bloco_t *b = &r.wmi[i];
        if (b->flags & 8) imprimir("  %s  evento   Notify %x", b->guid, b->notificacao);
        else imprimir("  %s  %s  %s", b->guid, (b->flags & 2) ? "método " : "datos  ", b->id);
        imprimir("  (%u inst., flags %x)\n", b->instancias, b->flags);
    }
    if (r.n_wmi > RADIOS_WMI_MAX) imprimir("  ... y %u más\n", r.n_wmi - RADIOS_WMI_MAX);
    return 0;
}

static int mudar(int radio, bool ligar)
{
    int e = info_sistema(INFO_RADIOS_MUDAR, (void *)(unsigned long)(radio | (ligar ? 1 << 8 : 0)), 0);
    if (e == ERRO_OCUPADO) {
        imprimir("radios: la BIOS aún no ha contestado; mira «radios» dentro de unos segundos\n");
        return 1;
    }
    if (e < 0) {
        imprimir_em(ERROS, "radios: no se pudo %s %s: %s\n", ligar ? "encender" : "apagar", nomes[radio],
                    e == ERRO_NAO_EXISTE ? "no hay esa radio o no hay WMI de HP" : nome_erro(e));
        return 1;
    }
    return estado();
}

int principal(int argc, char **argv)
{
    if (argc < 2 || compara_texto(argv[1], "estado") == 0) return estado();
    if (compara_texto(argv[1], "-h") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0) {
        escrever_texto(SAIDA, AYUDA);
        return 0;
    }
    if (compara_texto(argv[1], "wmi") == 0) return wmi();
    for (int i = 0; i < RADIOS_MAX; i++) {
        if (compara_texto(argv[1], ordens[i]) != 0) continue;
        if (argc < 3) {
            if (ler_radios() < 0) return 1;
            return estado();
        }
        if (compara_texto(argv[2], "si") == 0 || compara_texto(argv[2], "sí") == 0) return mudar(i, true);
        if (compara_texto(argv[2], "no") == 0) return mudar(i, false);
        break;
    }
    escrever_texto(ERROS, AYUDA);
    return 2;
}
