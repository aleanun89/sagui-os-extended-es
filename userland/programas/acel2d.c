// SPDX-License-Identifier: GPL-2.0-or-later
/* acel2d - el 2D por la GPU del escritorio (D195).
 *
 *   acel2d            estado: qué acelerador se usa (o por qué la CPU), cuántas
 *                     copias y rellenos ha hecho la GPU y la medida del arrastre
 *   acel2d probar     autoprueba completa ahora: copias solapadas en todas las
 *                     direcciones, rellenos, los bordes y muchas órdenes al azar,
 *                     comparadas píxel a píxel con la CPU, y una medida de la GPU
 *                     contra la CPU. Si el acelerador quedó apagado en el
 *                     arranque, se vuelve a iniciar solo para la prueba (sirve
 *                     para mirarlo en el hardware: el detalle va al log del kernel,
 *                     líneas [ACEL2D]).
 *
 *   acel2d probar --detalle   lo mismo y, además, copias sueltas de varios tamaños con el tiempo de
 *                     cada una (GPU y CPU) y los registros del motor antes y después, volcados al log
 *                     [ACEL2D] (D246): para sacar una foto o hacer «dmesg > /home/acel.txt»
 *
 *   acel2d apagar     apaga el acelerador (todo pasa a la CPU) sin regrabar (D244)
 *   acel2d encender   lo vuelve a elegir y a poner a prueba, como al arrancar (D244)
 *
 * El nombre del archivo es el del comando. */
#include "sagui.h"
#include "contas.h"

int principal(int argc, char **argv)
{
    /* D255: probar, apagar y encender tocan la GPU: solo el administrador. */
    if (argc > 1 && compara_texto(argv[1], "-h") != 0 && compara_texto(argv[1], "--ayuda") != 0 && falta_admin("acel2d"))
        return 1;
    if (argc > 1 && (compara_texto(argv[1], "probar") == 0 || compara_texto(argv[1], "-p") == 0)) {
        info_acel2d_prova_t r;
        memset(&r, 0, sizeof r);
        int detalle = argc > 2 && (compara_texto(argv[2], "--detalle") == 0 || compara_texto(argv[2], "-d") == 0);
        r.versao = detalle ? 2 : 0;
        if (info_sistema(INFO_ACEL2D_PROVA, &r, sizeof r) != 0) {
            imprimir_em(ERROS, "acel2d: el kernel no tiene la prueba\n");
            return 1;
        }
        imprimir("acel2d probar: %s\n", r.texto);
        if (r.resultado >= 0)
            imprimir("acel2d probar: %u ordenes, %u pixeles distintos, suma GPU %08x CPU %08x\n",
                     r.ordens, r.diferentes, r.soma_gpu, r.soma_cpu);
        if (r.resultado == 1)
            imprimir("acel2d probar: medida: GPU %u us, CPU %u us\n", r.us_gpu, r.us_cpu);
        if (detalle) imprimir("acel2d probar: detalle volcado al log del kernel (lineas [ACEL2D]; dmesg > /home/acel.txt)\n");
        imprimir("acel2d probar: %s\n", r.resultado == 1 ? "BIEN" : r.resultado == 0 ? "MAL" : "sin acelerador");
        return r.resultado == 1 ? 0 : 1;
    }
    if (argc > 1 && (compara_texto(argv[1], "apagar") == 0 || compara_texto(argv[1], "encender") == 0)) {
        int enc = compara_texto(argv[1], "encender") == 0;
        int r = info_sistema(INFO_ACEL2D_CTL, (void *)(long)enc, 0);
        info_acel2d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_ACEL2D, &v, sizeof v);
        imprimir("acel2d %s: %s (%s)\n", argv[1], v.ativo ? "acelerador activo" : "la CPU hace todo", v.motivo);
        return enc ? (r == 0 ? 0 : 1) : 0;
    }
    if (argc > 1) {
        imprimir_em(ERROS, "uso: acel2d [probar [--detalle] | apagar | encender]\n");
        return 1;
    }
    info_acel2d_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_ACEL2D, &v, sizeof v) != 0) {
        imprimir_em(ERROS, "acel2d: el kernel no lo sabe\n");
        return 1;
    }
    imprimir("Aceleracion 2D: %s\n", v.ativo ? v.nome : "CPU");
    imprimir("  %s\n", v.motivo);
    if (v.modelo) imprimir("  tarjeta: %s; plazo de la GPU en sesion: %u ms\n",
                           v.confirmada ? "confirmada" : "por autoprueba", v.prazo_ms);
    if (v.apagado) imprimir("  apagado %s: hasta «acel2d encender» todo lo hace la CPU\n",
                            v.apagado == 2 ? "a mano" : "por el vigilante (la GPU no respondio)");
    if (v.us_autoprova)
        imprimir("  autoprueba del arranque: %u us\n", v.us_autoprova);
    if (v.us_gpu || v.us_cpu)
        imprimir("  velocidad al arrancar: GPU %u us, CPU %u us\n", v.us_gpu, v.us_cpu);
    if (v.modelo) imprimir("  tarjeta %04x:%04x\n", v.fabricante, v.modelo);
    imprimir("  copias %u, rellenos %u por la GPU, fallos %u\n", v.copias, v.rellenos, v.falhas);
    if (v.quadros)
        imprimir("  arrastre: %u ventanas, %u cuadros, %u kciclos por cuadro\n", v.arrastos, v.quadros, v.kciclos_quadro);
    return 0;
}
