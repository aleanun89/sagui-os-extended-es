// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d - el 3D por la GPU (D285).
 *
 *   gpu3d               estado: qué driver hay (o por qué SaguiGL va por software),
 *                       quién tiene el motor y cuánto ha dibujado
 *   gpu3d probar        enciende el motor si hace falta y repite la autoprueba (limpiar,
 *                       color, Z, mezcla, textura, color interpolado, especular y Z de 16
 *                       bits, mirando píxeles del cuadro) y la medida (triángulos por
 *                       segundo, relleno y lo que cuesta traer el cuadro)
 *   gpu3d probar --detalle   lo mismo y los registros de la GPU en el log del kernel
 *                       (líneas [GPU3D]; «dmesg > /home/gpu3d.txt»)
 *   gpu3d apagar        SaguiGL por software hasta «gpu3d encender»
 *   gpu3d encender      vuelve a encenderlo y a probarlo (órdenes por lotes)
 *   gpu3d encender anillo    igual, con las órdenes escritas en el anillo (Intel): para
 *                       comparar los dos caminos en una máquina real
 *
 * Para comparar con SaguiGL por software: «sgldemo -g».
 * El nombre del archivo es el del comando. */
#include "sagui.h"
#include "contas.h"

static const char *nome_estado(unsigned e)
{
    switch (e) {
    case GPU3D_SIN_PROBAR: return "sin probar (se prueba con el primer uso)";
    case GPU3D_ATIVO:      return "activo";
    case GPU3D_FALHOU:     return "apagado por un fallo (software)";
    case GPU3D_APAGADO:    return "apagado a mano (software)";
    case GPU3D_DESATIVADO: return "desactivado por la opcion de arranque (software)";
    default:               return "sin GPU con driver 3D (software)";
    }
}

static const char *const provas[GPU3D_N_PROVAS] = {
    "limpiar", "rectangulo de color", "Z", "mezcla", "textura", "color interpolado", "especular", "Z de 16 bits",
};

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-h") != 0 && compara_texto(argv[1], "--ayuda") != 0 && falta_admin("gpu3d"))
        return 1;
    if (argc > 1 && (compara_texto(argv[1], "probar") == 0 || compara_texto(argv[1], "-p") == 0)) {
        info_gpu3d_prova_t r;
        memset(&r, 0, sizeof r);
        int detalle = argc > 2 && (compara_texto(argv[2], "--detalle") == 0 || compara_texto(argv[2], "-d") == 0);
        r.versao = detalle ? 2 : 0;
        if (info_sistema(INFO_GPU3D_PROVA, &r, sizeof r) != 0) {
            imprimir_em(ERROS, "gpu3d: el kernel no tiene la prueba\n");
            return 1;
        }
        imprimir("gpu3d probar: %s\n", r.texto);
        if (r.resultado >= 0) {
            for (int i = 0; i < GPU3D_N_PROVAS; i++)
                if (r.feitas & (1u << i))
                    imprimir("gpu3d probar:   %d %-20s %s\n", i, provas[i], r.provas & (1u << i) ? "bien" : "MAL");
            if (r.falhou >= 0 && (r.esperado || r.obtido))
                imprimir("gpu3d probar:   pixel %d,%d: %06x, se esperaba %06x\n", r.x, r.y, r.obtido, r.esperado);
            imprimir("gpu3d probar: autoprueba en %u us\n", r.us_autoprova);
        }
        if (r.resultado == 1) {
            imprimir("gpu3d probar: medida: %u triangulos pequenos en %u us (%u/s)\n", r.tri_pequenos, r.us_pequenos, r.tri_s);
            imprimir("gpu3d probar: medida: %u triangulos grandes, %u pixeles con mezcla en %u us (%u kpix/s)\n",
                     r.tri_grandes, r.pix_grandes, r.us_grandes, r.kpix_s);
            imprimir("gpu3d probar: medida: traer %u pixeles a la RAM: %u us\n", r.pix_leitura, r.us_leitura);
            imprimir("gpu3d probar: puntuacion 3D: %u\n", r.pontos);
        }
        if (detalle) imprimir("gpu3d probar: detalle en el log del kernel (lineas [GPU3D]; dmesg > /home/gpu3d.txt)\n");
        imprimir("gpu3d probar: %s\n", r.resultado == 1 ? "BIEN" : r.resultado == 0 ? "MAL" : "sin 3D por GPU");
        return r.resultado == 1 ? 0 : 1;
    }
    if (argc > 1 && (compara_texto(argv[1], "apagar") == 0 || compara_texto(argv[1], "encender") == 0)) {
        int enc = compara_texto(argv[1], "encender") == 0;
        int anel = enc && argc > 2 && compara_texto(argv[2], "anillo") == 0;
        int r = info_sistema(INFO_GPU3D_CTL, (void *)(long)(enc ? (anel ? 2 : 1) : 0), 0);
        info_gpu3d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_GPU3D, &v, sizeof v);
        imprimir("gpu3d %s: %s (%s)\n", argv[1], nome_estado(v.estado), v.motivo);
        return enc ? (r == 0 ? 0 : 1) : 0;
    }
    if (argc > 1) {
        imprimir_em(ERROS, "uso: gpu3d [probar [--detalle] | apagar | encender [anillo]]\n");
        return 1;
    }
    info_gpu3d_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_GPU3D, &v, sizeof v) != 0) {
        imprimir_em(ERROS, "gpu3d: el kernel no lo sabe\n");
        return 1;
    }
    imprimir("3D por GPU: %s\n", nome_estado(v.estado));
    imprimir("  driver %s, tarjeta %s\n", v.nome, v.placa);
    imprimir("  %s\n", v.motivo);
    if (v.estado == GPU3D_ATIVO) {
        imprimir("  ordenes: %s; plazo del vigilante: %u ms; memoria de GPU: %u KB (%u libres)\n",
                 v.variante ? "en el anillo" : "por lotes", v.prazo_ms, v.memoria_kb, v.livre_kb);
        if (v.dono) imprimir("  motor en uso por el proceso %u: destino %ux%u, Z pedido de %u bits\n", v.dono, v.larg, v.alt, v.zbits);
        else imprimir("  motor libre\n");
    }
    if (v.us_autoprova) imprimir("  autoprueba: %u us\n", v.us_autoprova);
    imprimir("  triangulos %u en %u entregas, cuadros %u, fallos %u, peticiones no atendidas %u\n",
             v.triangulos, v.lotes, v.quadros, v.falhas, v.recusas);
    if (v.pontos) imprimir("  ultima medida: %u triangulos/s, %u kpix/s, %u puntos\n", v.tri_s, v.kpix_s, v.pontos);
    return 0;
}
