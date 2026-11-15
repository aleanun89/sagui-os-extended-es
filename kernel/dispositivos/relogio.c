// SPDX-License-Identifier: GPL-3.0-only
/* relogio.c - el PIT (temporizador programable) y el reloj CMOS.
 *
 * PIT: el canal 0 recibe un reloj fijo de 1.193.182 Hz y cuenta de N hasta 0,
 * generando la IRQ 0 en cada vuelta. Con N = 11932 tenemos ~100 interrupciones por
 * segundo, el "latido" del sistema: cuenta el tiempo desde el arranque y, en la fase 3,
 * es quien quita la CPU a un proceso para dársela a otro.
 *
 * CMOS: el chip de reloj de la placa base guarda fecha y hora incluso apagado.
 * Los registros se leen por los puertos 0x70 (índice) y 0x71 (dato). */
#include "relogio.h"
#include "cpu.h"
#include "portas.h"
#include "processo.h"
#include "i8042.h"
#include "som.h"
#include "audio.h"
#include "usb.h"
#include "lapic.h"
#include "frequencia.h"

#define PIT_FREQ   1193182u
#define PIT_CANAL0 0x40
#define PIT_CMD    0x43

static volatile uint32_t tiques;

/* En cada tic: cuenta el tiempo y deja que el planificador decida si cambia de
 * proceso. Es esto lo que hace preemptiva la multitarea: ningún programa
 * necesita "ceder el turno". */
static quadro_t *ao_tique(quadro_t *q)
{
    tiques++;
    som_tique();     /* pitido sin bloquear a nadie (B3) */
    audio_tique();   /* el mezclador también corre aquí: sondeo si la tarjeta no interrumpe (D089) */
    uvc_tique();     /* cámara UVC: recoge los paquetes isócronos recibidos y monta cuadros (D277) */
    i8042_tique();   /* red de seguridad: ningún byte queda parado en la cola del teclado/touchpad */
    frequencia_tic_cpu();   /* el P-state de esta CPU, si hay algo pendiente (D175) */
    if (!processo_atual())
        return q;   /* antes de que existan los procesos */
    return escalonador_tique(q);
}

/* O tic de um processador secundario: da-o o seu proprio temporizador do
 * APIC local, e so faz o plano do escalonador (os dispositivos sao do
 * processador principal) (D157). */
quadro_t *relogio_tic_cpu(quadro_t *q)
{
    /* O APIC precisa do EOI: sem ele o bit "em servico" fica levantado e o
     * temporizador soava uma vez so (D157). Antes de planificar: las
     * interrupciones siguen apagadas hasta el iret. */
    lapic_fim();
    frequencia_tic_cpu();   /* D175: su P-state lo escribe ella misma (toma el cerrojo solo si hay algo) */
    return escalonador_tique_secundaria(q);   /* sin el cerrojo general si no hace falta (D165) */
}

void relogio_iniciar(void)
{
    uint32_t divisor = (PIT_FREQ + TIQUES_POR_SEGUNDO / 2) / TIQUES_POR_SEGUNDO;
    porta_escreve8(PIT_CMD, 0x34);              /* canal 0, byte bajo y alto, modo 2 (generador de tasa) */
    porta_escreve8(PIT_CANAL0, divisor & 0xFF);
    porta_escreve8(PIT_CANAL0, divisor >> 8);
    irq_registrar(0, ao_tique);
}

uint32_t relogio_tiques(void)   { return tiques; }
uint32_t relogio_segundos(void) { return tiques / TIQUES_POR_SEGUNDO; }

void relogio_dormir_ms(uint32_t ms)
{
    uint32_t alvo = tiques + (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    while ((int32_t)(alvo - tiques) > 0)
        cpu_espera();   /* duerme hasta la próxima interrupción */
}

/* ------------------------------------------------------------ CMOS */

static uint8_t cmos_le(uint8_t reg)
{
    porta_escreve8(0x70, reg | 0x80);   /* bit 7 = mantiene la NMI desactivada durante el acceso */
    return porta_le8(0x71);
}

static bool cmos_atualizando(void)
{
    return cmos_le(0x0A) & 0x80;
}

static uint8_t de_bcd(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static void ler_bruto(uint8_t r[7])
{
    while (cmos_atualizando())
        ;
    r[0] = cmos_le(0x00); r[1] = cmos_le(0x02); r[2] = cmos_le(0x04);
    r[3] = cmos_le(0x07); r[4] = cmos_le(0x08); r[5] = cmos_le(0x09);
    r[6] = cmos_le(0x32);   /* siglo, cuando la placa lo guarda (ACPI) */
}

void cmos_ler_data(data_hora_t *d)
{
    /* El chip actualiza los registros una vez por segundo; una lectura en
     * medio de eso puede mezclar valores. Leemos hasta que dos lecturas seguidas
     * den el mismo resultado. */
    uint8_t a[7], b[7];
    ler_bruto(b);
    do {
        for (int i = 0; i < 7; i++) a[i] = b[i];
        ler_bruto(b);
    } while (a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3] || a[4] != b[4] || a[5] != b[5]);

    uint8_t status_b = cmos_le(0x0B);
    bool bcd = !(status_b & 0x04);
    bool h12 = !(status_b & 0x02);
    uint8_t pm = b[2] & 0x80;
    b[2] &= 0x7F;
    if (bcd)
        for (int i = 0; i < 7; i++) b[i] = de_bcd(b[i]);
    if (h12) {
        if (pm && b[2] < 12) b[2] += 12;
        if (!pm && b[2] == 12) b[2] = 0;
    }
    d->segundo = b[0]; d->minuto = b[1]; d->hora = b[2];
    d->dia = b[3]; d->mes = b[4];
    uint32_t seculo = (b[6] >= 19 && b[6] <= 21) ? b[6] : 20;
    d->ano = (uint16_t)(seculo * 100 + b[5]);
}

static void cmos_escreve(uint8_t reg, uint8_t v)
{
    porta_escreve8(0x70, reg | 0x80);
    porta_escreve8(0x71, v);
}

static uint8_t para_bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

/* Graba fecha y hora en el RTC (D080). Respeta el formato que eligió la BIOS
 * (registro B: BCD o binario, 12 o 24 horas) en vez de imponer el nuestro:
 * Windows y la BIOS siguen leyendo a su manera. El bit SET (B7) detiene
 * las actualizaciones mientras escribimos, como manda la hoja del MC146818; el
 * siglo (0x32) solo se escribe si ya parecía válido, porque no toda placa
 * usa ese registro para eso. */
void cmos_gravar_data(const data_hora_t *d)
{
    bool i0 = int_salva_desliga();
    uint8_t b = cmos_le(0x0B);
    bool bcd = !(b & 0x04), h12 = !(b & 0x02);
    uint8_t seculo = cmos_le(0x32);
    if (bcd) seculo = de_bcd(seculo);
    bool grava_seculo = seculo >= 19 && seculo <= 21;
    uint8_t v[6] = { d->segundo, d->minuto, d->hora, d->dia, d->mes, (uint8_t)(d->ano % 100) };
    uint8_t pm = 0;
    if (h12) {
        pm = v[2] >= 12 ? 0x80 : 0;
        v[2] = (uint8_t)(v[2] % 12 == 0 ? 12 : v[2] % 12);
    }
    if (bcd) for (int i = 0; i < 6; i++) v[i] = para_bcd(v[i]);
    v[2] |= pm;
    cmos_escreve(0x0B, b | 0x80);    /* SET: congela el reloj */
    cmos_escreve(0x00, v[0]); cmos_escreve(0x02, v[1]); cmos_escreve(0x04, v[2]);
    cmos_escreve(0x07, v[3]); cmos_escreve(0x08, v[4]); cmos_escreve(0x09, v[5]);
    cmos_escreve(0x06, (uint8_t)(dia_da_semana(d->ano, d->mes, d->dia) + 1));   /* 1 = domingo */
    if (grava_seculo) {
        uint8_t s = (uint8_t)(d->ano / 100);
        cmos_escreve(0x32, bcd ? para_bcd(s) : s);
    }
    cmos_escreve(0x0B, b & 0x7F);    /* suelta: vuelve a contar desde aquí */
    int_restaura(i0);
}

/* Día de la semana: cuenta los días desde 01/01/2000, que fue un sábado.
 * Devolve 0 = domingo ... 6 = sábado. */
int dia_da_semana(int ano, int mes, int dia)
{
    static const int dias_mes[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (ano < 2000) return 0;
    int total = 0;
    for (int a = 2000; a < ano; a++)
        total += (a % 4 == 0 && (a % 100 != 0 || a % 400 == 0)) ? 366 : 365;
    for (int m = 1; m < mes && m <= 12; m++)
        total += dias_mes[m - 1] + (m == 2 && ano % 4 == 0 && (ano % 100 != 0 || ano % 400 == 0));
    total += dia - 1;
    return (total + 6) % 7;
}
