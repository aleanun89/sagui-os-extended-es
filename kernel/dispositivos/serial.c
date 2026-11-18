// SPDX-License-Identifier: GPL-3.0-only
/* serial.c - puerto serie COM1 (0x3F8), el "log" del kernel.
 *
 * En QEMU el serial va a un archivo, que leen las pruebas automáticas. En el
 * G470 no hay conector serie; escribir en él no hace daño (los bytes se pierden).
 * Configuración: 38400 baudios, 8 bits, sin paridad, 1 bit de parada. */
#include "console.h"
#include "cpu.h"
#include "portas.h"

#define COM1 0x3F8

static bool serial_ok;
/* D385: con una 16550A (FIFO de 16 bytes) no hace falta mirar el registro de línea antes de cada byte: cuando dice
 * «vacío» (THRE, que con la FIFO encendida quiere decir FIFO vacía) caben 16. Así se lee el registro una vez cada 16
 * bytes en vez de una por byte (en QEMU, la mitad de salidas de la máquina virtual). Sin FIFO (8250/16450), uno. */
static uint8_t fifo_tam = 1, livres;

void serial_iniciar(void)
{
    porta_escreve8(COM1 + 1, 0x00);   /* sin interrupciones del serie */
    porta_escreve8(COM1 + 3, 0x80);   /* DLAB=1: los dos próximos bytes son el divisor */
    porta_escreve8(COM1 + 0, 3);      /* 115200 / 3 = 38400 baudios */
    porta_escreve8(COM1 + 1, 0);
    porta_escreve8(COM1 + 3, 0x03);   /* 8N1, DLAB=0 */
    porta_escreve8(COM1 + 2, 0xC7);   /* FIFO activada y limpia */
    porta_escreve8(COM1 + 4, 0x03);   /* DTR + RTS */
    /* Si no hay UART (el registro de línea vuelve 0xFF), no esperamos por ella. */
    serial_ok = porta_le8(COM1 + 5) != 0xFF;
    if (serial_ok && (porta_le8(COM1 + 2) & 0xC0) == 0xC0) fifo_tam = 16;   /* IIR bits 6-7: FIFO funcionando */
}

void serial_char(char c)
{
    if (!serial_ok)
        return;
    if (c == '\n')
        serial_char('\r');
    /* La cuenta y el byte, sin interrupciones (un klog desde una interrupción en medio gastaría los mismos
     * huecos); pero la ESPERA a que la FIFO se vacíe se hace con las interrupciones como estaban: cada vuelta
     * apaga solo para mirar el LSR y, si hay hueco, escribir (revisión de la 8.5.4.10: esperar con cli perdía
     * ticks y retrasaba teclado y audio, y con una UART atascada podía tardar mucho). Se supone una 16550A
     * (THRE = FIFO vacía); con FIFO de 1, como antes. */
    for (int i = 0;; i++) {
        bool i0 = int_salva_desliga();
        if (!livres && (porta_le8(COM1 + 5) & 0x20))
            livres = fifo_tam;
        if (livres || i >= 100000) {   /* con límite: una UART atascada no cuelga el kernel */
            if (!livres)
                livres = fifo_tam;
            porta_escreve8(COM1, (uint8_t)c);
            livres--;
            int_restaura(i0);
            return;
        }
        int_restaura(i0);
    }
}

void serial_texto(const char *s)
{
    while (*s)
        serial_char(*s++);
}
