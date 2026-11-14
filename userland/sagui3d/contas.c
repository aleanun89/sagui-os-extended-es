// SPDX-License-Identifier: GPL-3.0-only
/* contas.c - punto fijo, seno y ángulos del Sagui 3D.
 *
 * Los programas de SaguiOS no usan números de punto flotante (la FPU no se
 * guarda en el cambio de procesos), así que todo es entero: "fixo" tiene 16 bits
 * de fracción. Multiplicar dos fixos da 32 bits de fracción, que caben en un
 * entero de 64 bits; el resultado vuelve a 16 con un desplazamiento.
 * Dividir 64 por 32 bits pediría una función de la biblioteca del compilador,
 * que no usamos: la instrucción "idiv" del x86 hace exactamente eso (EDX:EAX
 * dividido por un registro), siempre que el cociente quepa en 32 bits;
 * por eso el muldiv verifica antes y satura. */
#include "jogo.h"

#define MAXIMO 0x7FFFFFFF

fixo muldiv(fixo a, fixo b, fixo c)
{
    int64_t p = (int64_t)a * b;
    bool negativo = (p < 0) != (c < 0);
    if (c == 0) return negativo ? -MAXIMO : MAXIMO;
    uint64_t ap = p < 0 ? (uint64_t)(-p) : (uint64_t)p;
    uint32_t ac = c < 0 ? (uint32_t)(-(int64_t)c) : (uint32_t)c;
    if ((ap >> 31) >= ac) return negativo ? -MAXIMO : MAXIMO;   /* no cabría */
    uint32_t lo = (uint32_t)p, hi = (uint32_t)((uint64_t)p >> 32);
    int32_t q, r;
    __asm__("idivl %4" : "=a"(q), "=d"(r) : "0"(lo), "1"(hi), "rm"(c) : "cc");
    (void)r;
    return q;
}

fixo fdiv(fixo a, fixo b) { return muldiv(a, UM, b); }

int32_t raiz(uint32_t n)
{
    /* Método de ir acertando un bit por vez, del más alto al más bajo. */
    uint32_t r = 0;
    for (uint32_t bit = 1u << 15; bit; bit >>= 1) {
        uint32_t t = r | bit;
        if (t * t <= n) r = t;
    }
    return (int32_t)r;
}

/* ------------------------------------------------------------ seno */

/* Tabla de un cuarto de vuelta (0 a 90 grados, 1025 valores), con 30 bits de
 * fracción durante la cuenta. Empieza en el vector (1, 0) y gira un paso por vez
 * mediante la rotación de un ángulo pequeño d:
 *     x' = x cos d - y sin d,    y' = x sin d + y cos d
 * cos d y sin d (d = 2pi/4096) se calcularon a mano una vez y entran aquí
 * como constantes. El error acumulado en 1024 pasos queda por debajo de 1/65536,
 * que es la precisión del fixo. El resto de la vuelta sale por simetría. */
static fixo quarto[QUARTO + 1];

void contas_iniciar(void)
{
    const int64_t COS_D = 1073740561;   /* cos(2pi/4096) * 2^30 */
    const int64_t SEN_D = 1647099;      /* sen(2pi/4096) * 2^30 */
    int64_t x = (int64_t)1 << 30, y = 0;
    for (int i = 0; i <= QUARTO; i++) {
        quarto[i] = (fixo)((y + (1 << 13)) >> 14);
        int64_t nx = (x * COS_D - y * SEN_D + ((int64_t)1 << 29)) >> 30;
        int64_t ny = (x * SEN_D + y * COS_D + ((int64_t)1 << 29)) >> 30;
        x = nx;
        y = ny;
    }
    quarto[QUARTO] = UM;
}

fixo seno(int a)
{
    a = ANG(a);
    if (a <= QUARTO) return quarto[a];
    if (a <= 2 * QUARTO) return quarto[2 * QUARTO - a];
    if (a <= 3 * QUARTO) return -quarto[a - 2 * QUARTO];
    return -quarto[VOLTA - a];
}

fixo cosseno(int a) { return seno(a + QUARTO); }

/* Ángulo de un vector: reduce al primer octavo (0 a 45 grados) cambiando
 * signos y ejes, halla el ángulo cuya tangente es |menor|/|mayor| por búsqueda
 * binaria en la tabla y deshace los cambios. */
int angulo_de(fixo dx, fixo dy)
{
    if (dx == 0 && dy == 0) return 0;
    fixo ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    bool troca = ay > ax;
    fixo t = troca ? fdiv(ax, ay) : fdiv(ay, ax);   /* 0 a 1 */
    int lo = 0, hi = QUARTO / 2;
    while (lo < hi) {
        int meio = (lo + hi + 1) / 2;
        fixo tg = fdiv(quarto[meio], quarto[QUARTO - meio]);
        if (tg <= t) lo = meio; else hi = meio - 1;
    }
    int a = troca ? QUARTO - lo : lo;                /* primer cuadrante */
    if (dx < 0) a = 2 * QUARTO - a;
    if (dy < 0) a = VOLTA - a;
    return ANG(a);
}

/* ------------------------------------------------------------ sorteo */
static uint32_t estado_sorteio = 0x5A6B1C2Du;

uint32_t sorteio(void)
{
    /* Mezcla por desplazamientos y o-exclusivo: rápido y suficiente
     * para elegir hacia dónde mira un mono. */
    uint32_t x = estado_sorteio;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    estado_sorteio = x;
    return x;
}
