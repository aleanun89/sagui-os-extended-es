// SPDX-License-Identifier: GPL-3.0-only
/* pic.c - los dos controladores de interrupción 8259 (maestro y esclavo).
 *
 * Después del arranque, el PIC entrega las IRQ 0-7 en los vectores 8-15, que en modo
 * protegido son excepciones de la CPU (8 es "doble fallo"!). Por eso el primer
 * trabajo es reprogramarlos: IRQ 0-7 en 0x20-0x27 y IRQ 8-15 en 0x28-0x2F.
 * Empiezan todas enmascaradas; cada driver habilita la suya con pic_habilitar(). */
#include "cpu.h"
#include "portas.h"

#define PIC1_CMD  0x20
#define PIC1_DADO 0x21
#define PIC2_CMD  0xA0
#define PIC2_DADO 0xA1

static void escreve(uint16_t porta, uint8_t v)
{
    porta_escreve8(porta, v);
    porta_pausa();   /* los 8259 antiguos necesitaban un tiempo entre comandos */
}

void pic_iniciar(void)
{
    /* Secuencia de inicialización (ICW1 a ICW4) definida por la hoja de datos del 8259. */
    escreve(PIC1_CMD, 0x11);          /* ICW1: habrá ICW4, modo cascada */
    escreve(PIC2_CMD, 0x11);
    escreve(PIC1_DADO, VETOR_IRQ0);   /* ICW2: vector base */
    escreve(PIC2_DADO, VETOR_IRQ0 + 8);
    escreve(PIC1_DADO, 0x04);         /* ICW3: esclavo conectado en la línea 2 del maestro */
    escreve(PIC2_DADO, 0x02);         /*        identidad del esclavo = 2 */
    escreve(PIC1_DADO, 0x01);         /* ICW4: modo 8086 */
    escreve(PIC2_DADO, 0x01);
    escreve(PIC1_DADO, 0xFB);         /* todo enmascarado, menos la línea 2 (cascada) */
    escreve(PIC2_DADO, 0xFF);
}

void pic_habilitar(uint8_t irq)
{
    uint16_t porta = irq < 8 ? PIC1_DADO : PIC2_DADO;
    porta_escreve8(porta, porta_le8(porta) & ~(1 << (irq & 7)));
}

void pic_desabilitar(uint8_t irq)
{
    uint16_t porta = irq < 8 ? PIC1_DADO : PIC2_DADO;
    porta_escreve8(porta, porta_le8(porta) | (1 << (irq & 7)));
}

void pic_fim(uint8_t irq)
{
    if (irq >= 8)
        porta_escreve8(PIC2_CMD, 0x20);
    porta_escreve8(PIC1_CMD, 0x20);
}

/* IRQ 7 y 15 pueden llegar "de mentira" (ruido en la línea). Leyendo el registro
 * de servicio (ISR) se puede saber: si el bit no está encendido, es espuria y no
 * envía EOI (en el esclavo, el maestro todavía necesita el EOI de la línea 2). */
bool pic_espuria(uint8_t irq)
{
    if (irq != 7 && irq != 15)
        return false;
    uint16_t cmd = irq == 7 ? PIC1_CMD : PIC2_CMD;
    porta_escreve8(cmd, 0x0B);
    if (porta_le8(cmd) & 0x80)
        return false;
    if (irq == 15)
        porta_escreve8(PIC1_CMD, 0x20);
    return true;
}

/* D211: ¿la IRQ está "en servicio" en el 8259? En el modo mixto (unas
 * líneas por el IOAPIC y otras por el 8259) el fin de interrupción solo va
 * al 8259 si fue él quien la entregó. Deja el 8259 leyendo el ISR, como
 * pic_espuria(). */
bool pic_em_servico(uint8_t irq)
{
    if (irq >= 16) return false;
    if (irq >= 8) {
        porta_escreve8(PIC2_CMD, 0x0B);
        return (porta_le8(PIC2_CMD) & (1u << (irq - 8))) != 0;
    }
    porta_escreve8(PIC1_CMD, 0x0B);
    return (porta_le8(PIC1_CMD) & (1u << irq)) != 0;
}

/* D302: las peticiones pendientes (IRR) de los dos 8259, para saber en qué línea pide una tarjeta
 * ISA sin tener que atenderla (el IRR se enciende aunque la línea esté enmascarada). Deja el 8259
 * leyendo el ISR, como pic_espuria(). */
uint16_t pic_pedidos(void)
{
    porta_escreve8(PIC1_CMD, 0x0A);
    porta_escreve8(PIC2_CMD, 0x0A);
    uint16_t r = (uint16_t)(porta_le8(PIC1_CMD) | porta_le8(PIC2_CMD) << 8);
    porta_escreve8(PIC1_CMD, 0x0B);
    porta_escreve8(PIC2_CMD, 0x0B);
    return r;
}
