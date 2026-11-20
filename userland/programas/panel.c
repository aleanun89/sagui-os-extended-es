// SPDX-License-Identifier: GPL-3.0-only
/* panel - el panel táctil (touchpad) desde la consola (D407).
 *
 *   panel                     el modelo reconocido (Synaptics, ALPS, Elantech o ninguno), sus contadores y las opciones
 *   panel CLAVE=N ...         cambia opciones y las guarda en la configuración de la persona (sagui.conf):
 *                             toque, dos_dedos, borde, natural (0/1), sensibilidad (1-10), escribir_ms (0-2000),
 *                             sin_usb (0/1)
 *   panel ps2 | panel auto    usar el panel como ratón PS/2 de siempre (sin gestos) o volver al modo con gestos
 *   panel restablecer         las opciones de serie
 * «touchpad» es el mismo programa. Lo hace «configuracion panel ...», que es quien guarda el archivo; lo que el kernel
 * detectó al arrancar está también en el registro (dmesg | grep PAINEL). */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc >= 2 && (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) {
        imprimir("uso: %s [mostrar | CLAVE=N... | ps2 | auto | restablecer]\n"
                 "  claves: toque, dos_dedos, borde, natural (0/1), sensibilidad (1-10), escribir_ms (0-2000), sin_usb (0/1)\n"
                 "  ps2: el panel como ratón PS/2 de siempre (sin gestos); auto: con gestos si se reconoce\n", argv[0]);
        return 0;
    }
    static char *av[24];
    static char modo[16];
    int n = 0;
    av[n++] = "configuracion";
    av[n++] = "panel";
    for (int i = 1; i < argc && n < 22; i++) {
        if (compara_texto(argv[i], "ps2") == 0 || compara_texto(argv[i], "auto") == 0) {
            formatar_texto(modo, sizeof modo, "modo=%s", argv[i]);
            av[n++] = modo;
        } else av[n++] = argv[i];
    }
    av[n] = NULL;
    int pid = executar("/bin/configuracion", av);
    if (pid < 0) { imprimir_em(ERROS, "%s: no se pudo lanzar configuracion: %s\n", argv[0], nome_erro(pid)); return 1; }
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo;
}
