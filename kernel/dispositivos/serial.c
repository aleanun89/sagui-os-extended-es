// SPDX-License-Identifier: GPL-3.0-only
/* serial.c - puerto serie COM1 (0x3F8), el "log" del kernel.
 *
 * En QEMU el serial va a un archivo, que leen las pruebas automáticas. En el
 * G470 no hay conector serie; escribir en él no hace daño (los bytes se pierden).
 * Configuración: 38400 baudios, 8 bits, sin paridad, 1 bit de parada. */
#include "console.h"
#include "portas.h"

#define COM1 0x3F8

static bool serial_ok;

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
}

void serial_char(char c)
{
    if (!serial_ok)
        return;
    if (c == '\n')
        serial_char('\r');
    /* Espera a que el registro de transmisión se vacíe, con límite. */
    for (int i = 0; i < 100000 && !(porta_le8(COM1 + 5) & 0x20); i++)
        ;
    porta_escreve8(COM1, (uint8_t)c);
}

void serial_texto(const char *s)
{
    while (*s)
        serial_char(*s++);
}
