// SPDX-License-Identifier: GPL-2.0-or-later
/* inyectar - prueba de SIS_INYECTAR (D251, mejora 097; tests/h249_inyectar.py).
 *
 *   inyectar --ventana     abre una ventana en (100,100), crea /tmp/inyectar.listo y
 *                          anota en el registro cada evento que recibe;
 *   inyectar --servicio    pide el permiso; si se lo dan (solo como /bin/vncd lanzado por
 *                          el gestor de servicios), comprueba que rechaza argumentos malos,
 *                          espera a la ventana y le manda clics, teclas, rueda y Alt+Tab;
 *                          al final suelta el permiso, lo vuelve a pedir y termina con Alt
 *                          pulsado (el kernel debe soltarlo solo).
 * Todo va al registro del sistema (serie) con el prefijo «inyectar». */
#include "sagui.h"

#define VX 100            /* marco de la ventana en la pantalla */
#define VY 100
#define CX (VX + 2 + 60)  /* un punto dentro del área de la ventana (borde 2, título 24) */
#define CY (VY + 24 + 80)

static int raton(int x, int y, int b) { return inyectar_entrada(INY_RATON, (x << 16) | y, b); }

static void tecla(int t, int mods)
{
    inyectar_entrada(INY_TECLA, t, INY_PULSADA | mods);
    dormir(30);
    inyectar_entrada(INY_TECLA, t, mods);
    dormir(30);
}

static void clic(int x, int y)
{
    raton(x, y, 0);
    dormir(100);
    raton(x, y, INY_BOTON_IZQ);
    dormir(60);
    raton(x, y, 0);
    dormir(300);
}

static int ventana(void)
{
    uint32_t *px;
    int id = criar_janela("Inyectar", 300, 200, &px);
    if (id < 0) { registrar("inyectar-ventana: erro %d\n", id); return 1; }
    for (int i = 0; i < 300 * 200; i++) px[i] = 0x2E8B57;
    atualizar_janela(id, NULL);
    mover_janela(id, VX, VY);
    int fd = abrir("/tmp/inyectar.listo", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever(fd, "1\n", 2); fechar(fd); }
    registrar("inyectar-ventana: lista (id %d)\n", id);
    evento_t e;
    for (;;) {
        if (proximo_evento(id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_CLIQUE)
            registrar("inyectar-ventana: clique (%d,%d) botao %u\n", e.x, e.y, e.botoes);
        else if (e.tipo == EV_SOLTOU)
            registrar("inyectar-ventana: soltou (%d,%d)\n", e.x, e.y);
        else if (e.tipo == EV_TECLA)
            registrar("inyectar-ventana: tecla %u mods %u\n", e.tecla, e.modificadores);
        else if (e.tipo == EV_RODA)
            registrar("inyectar-ventana: roda %d\n", (int)e.cliques);
    }
    registrar("inyectar-ventana: fin\n");
    return 0;
}

static int servico(bool falso)
{
    int antes = inyectar_entrada(INY_TECLA, 'x', INY_PULSADA);
    int r = inyectar_entrada(INY_ESTADO, 1, 0);
    registrar("inyectar: %s antes=%d estado=%d\n", falso ? "falso" : "servicio", antes, r);
    if (r != 0) return 0;

    /* Argumentos fuera de rango: todos ERRO_INVALIDO, y nada llega a ninguna parte. */
    int v1 = inyectar_entrada(INY_TECLA, 0x5000, INY_PULSADA);
    int v2 = inyectar_entrada(INY_TECLA, 'a', 0x100);
    int v3 = inyectar_entrada(INY_RUEDA, 500, 0);
    int v4 = inyectar_entrada(INY_RATON, 0, 0x80);
    int v5 = inyectar_entrada(99, 0, 0);
    int v6 = inyectar_entrada(INY_ESTADO, 2, 0);
    int v7 = inyectar_entrada(INY_TECLA, -5, INY_PULSADA);
    registrar("inyectar: validacion %d %d %d %d %d %d %d\n", v1, v2, v3, v4, v5, v6, v7);
    /* Posición fuera de la pantalla: se recorta. */
    raton(30000, 30000, 0);
    inyectar_entrada(INY_RUEDA, 1, 0);   /* el kernel anota dónde: debe ser la esquina */

    info_arquivo_t ia;
    int esperas = 0;
    while (info_arquivo("/tmp/inyectar.listo", &ia) < 0) {
        if (++esperas > 3000) { registrar("inyectar: la ventana no aparecio\n"); return 1; }
        dormir(100);
    }
    dormir(1000);
    registrar("inyectar: empieza\n");

    clic(CX, CY);                                   /* foco y clic en la ventana */
    tecla('h', 0);
    tecla('A', INY_MOD_SHIFT);
    tecla('c', INY_MOD_CTRL);                       /* Ctrl+C: llega el 3 */
    tecla(0xE1, 0);                                 /* á */
    tecla(0x100 + 14, 0);                           /* TECLA_F5 */
    dormir(300);
    raton(CX + 10, CY + 10, 0);
    dormir(100);
    inyectar_entrada(INY_RUEDA, 2, 0);
    dormir(100);
    inyectar_entrada(INY_RUEDA, -1, 0);
    dormir(300);
    registrar("inyectar: alt+tab\n");
    inyectar_entrada(INY_TECLA, 0, INY_MOD_ALT);   /* Alt solo */
    dormir(50);
    tecla('\t', INY_MOD_ALT);
    dormir(300);
    inyectar_entrada(INY_TECLA, 0, 0);             /* suelta Alt: elige la otra ventana */
    dormir(600);
    tecla('x', 0);                                  /* va al Terminal, no a la ventana */
    dormir(300);
    clic(CX, CY);                                   /* el clic devuelve el foco a la ventana */
    tecla('z', 0);
    dormir(300);
    r = inyectar_entrada(INY_ESTADO, 0, 0);
    int depois = inyectar_entrada(INY_TECLA, 'y', INY_PULSADA);
    registrar("inyectar: soltado %d despues=%d\n", r, depois);
    r = inyectar_entrada(INY_ESTADO, 1, 0);
    inyectar_entrada(INY_TECLA, 0, INY_MOD_ALT | INY_MOD_CTRL);   /* sale con Ctrl+Alt pulsados */
    registrar("inyectar: otra vez %d; termina con Ctrl+Alt pulsados\n", r);
    dormir(200);
    return 0;
}

int principal(int argc, char **argv)
{
    bool falso = false;
    for (int i = 1; i < argc; i++)
        if (compara_texto(argv[i], "--falso") == 0) falso = true;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--ventana") == 0) return ventana();
        if (compara_texto(argv[i], "--servicio") == 0) return servico(falso);
    }
    registrar("inyectar: uso: inyectar --ventana | --servicio [--falso]\n");
    imprimir("uso: inyectar --ventana | --servicio [--falso]\n");
    return 2;
}
