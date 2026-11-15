// SPDX-License-Identifier: GPL-3.0-only
/* componentes - demostración del kit gráfico de la libsagui (H22): etiqueta,
 * botón, caja de texto, área de texto con desplazamiento, lista y diálogos.
 * Desde la v6.1 también enseña los componentes nuevos (mejora 042): el
 * lienzo, la barra de progreso y la casilla de verificación. Cada acción
 * también se escribe en la salida, para los tests automáticos. */
#include "sagui.h"
#include "gui.h"

static char itens[16][64] = { "Archivos", "Terminal", "Editor", "Calculadora", "Sagui Atrapa Bananas",
                              "Acerca de SaguiOS", "Pendrive", "Procesos", "Memoria", "Reloj",
                              "Pipes", "Paginación" };

/* ---- demo de los componentes de escritorio (mejora 116, D291): `componentes --escritorio` ---- */
static void icono_demo(jan_t *j, int x, int y, bool ativo)
{
    uint32_t c = ativo ? W98_AZUL : W98_SOMBRA;
    g_retangulo(j, x + 2, y + 2, 12, 12, c);
    g_retangulo(j, x + 4, y + 4, 8, 8, COR_BRANCO);
}

static int demo_escritorio(void)
{
    jan_t *j = jan_abrir("Componentes de escritorio", 640, 380);
    if (!j) return 1;
    g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE);
    static const char *const fontes[3] = { "Arial", "Times New Roman", "Courier New" };
    barra_menu_t menu;
    memset(&menu, 0, sizeof menu);
    menu.nomes[0] = "Archivo"; menu.itens[0] = "Nuevo\nAbrir...\n-\nSalir";
    menu.nomes[1] = "Ayuda";   menu.itens[1] = "Acerca de";
    menu.n = 2;
    barra_menu_iniciar(&menu, 0, 0, j->larg);
    barra_item_t itens[5] = {
        { 1, icono_demo, NULL, false, false, false, false, 0 },
        { 2, NULL, "N", true, true, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { 3, icono_demo, NULL, false, false, false, true, 0 },
        { -1, NULL, NULL, false, false, false, false, 150 },
    };
    barra_ferr_t ferr;
    barra_ferr_iniciar(&ferr, 0, 20, j->larg, itens, 5);
    combo_t combo;
    combo_iniciar(&combo, barra_ferr_x(&ferr, 4), 23, 146, fontes, 3, false, 7201);
    combo_definir(&combo, fontes[0]);
    regua_t regua;
    regua_iniciar(&regua, 0, 48, j->larg);
    regua.ox = 40; regua.zoom = 60;
    rte_t *rte = rte_criar(0, 68, j->larg, j->alt - 68 - 22);
    if (!rte) return 1;
    rte_zoom(rte, 60);
    rte_inserir(rte, "Control de texto con formato de la libsagui.", 44);
    static const int larg[3] = { 120, 120, 100 };
    barra_estado_t estado;
    barra_estado_iniciar(&estado, 0, j->alt - 22, j->larg, 3, larg);
    barra_estado_texto(&estado, 0, "Sección 1");
    barra_estado_texto(&estado, 1, "Lin 1");
    barra_estado_texto(&estado, 2, "Col 45");
    barra_menu_desenhar(j, &menu);
    barra_ferr_desenhar(j, &ferr);
    combo_desenhar(j, &combo);
    regua_desenhar(j, &regua);
    rte_desenhar(j, rte);
    barra_estado_desenhar(j, &estado);
    jan_mostrar(j);
    registrar("componentes: escritorio pronto\n");
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        int m = barra_menu_tratar(j, &menu, &e);
        if (m >= 0) registrar("componentes: menu %d item %d\n", m / 100, m % 100);
        int id = barra_ferr_tratar(j, &ferr, &e);
        if (id) registrar("componentes: barra boton %d marcado %d\n", id, itens[1].marcado);
        if (combo_tratar(j, &combo, &e) == COMBO_SELECIONOU) registrar("componentes: combo %s\n", fontes[combo.sel]);
        regua_tratar(j, &regua, &e);
        rte_tratar(j, rte, &e);
    }
    rte_liberar(rte);
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--escritorio") == 0) return demo_escritorio();
    jan_t *j = jan_abrir("Componentes de libsagui", 560, 400);
    if (!j) return 1;
    rotulo(j, 16, 12, "Etiqueta: el kit gráfico corre en el propio programa (anillo 3).", COR_TEXTO);
    botao_t contar = { 16, 40, 140, 28, "Haz clic aquí", false };
    botao_t apagar = { 16, 352, 160, 28, "Borrar texto...", false };
    botao_t aviso = { 190, 352, 120, 28, "Mensaje", false };
    caixa_t caixa;
    memset(&caixa, 0, sizeof caixa);
    caixa.x = 16; caixa.y = 82; caixa.l = 300;
    caixa_definir(&caixa, "Escribe aquí");
    area_t area;
    area_iniciar(&area, 16, 120, 300, 220);
    char texto[1400];
    int n = 0;
    for (int i = 1; i <= 30; i++)
        n += formatar_texto(texto + n, sizeof texto - (size_t)n, "Línea %d del área de texto, con scroll.\n", i);
    area_definir(&area, texto, n);
    lista_t lista = { 332, 120, 212, 220, itens, 12, 0, 0, NULL };
    int cliques = 0;
    char buf[64];

    /* Componentes nuevos de la v6.1 (mejora 042). */
    lenzo_t *lenzo = lenzo_criar(332, 20, 212, 84);
    if (lenzo) {
        lenzo_retangulo(lenzo, 10, 10, 60, 40, 0xFFE8C8DC, COR_DESTAQUE, true, 1);
        lenzo_elipse(lenzo, 80, 10, 50, 40, 0xFFC8DCF0, COR_SELECAO, true, 1);
        lenzo_linha(lenzo, 140, 12, 200, 58, 0xFFE06060, 2);
        int pontos[6] = { 140, 60, 170, 74, 200, 60 };
        lenzo_poligono(lenzo, pontos, 3, 0xFFC8E8C8, COR_DESTAQUE, true, 1);
        lenzo_texto(lenzo, 8, 66, "Lienzo (v6.1)", COR_TEXTO);
    }
    progresso_t *barra = progresso_criar(16, 300, 220, 16, false);
    if (barra) {
        progresso_definir(barra, 40);
        progresso_texto(barra, true);
    }
    progresso_t *barra_v = progresso_criar(250, 296, 16, 44, true);
    if (barra_v) {
        progresso_indeterminado(barra_v, true);
    }
    casilla_t casilla = { 290, 302, false };

    botao_desenhar(j, &contar);
    botao_desenhar(j, &apagar);
    botao_desenhar(j, &aviso);
    rotulo(j, 170, 46, "clics: 0    ", COR_TEXTO_FRACO);
    caixa_desenhar(j, &caixa);
    area_desenhar(j, &area);
    lista_desenhar(j, &lista);
    if (lenzo) lenzo_desenhar(j, lenzo);
    if (barra) progresso_desenhar(j, barra);
    if (barra_v) progresso_desenhar(j, barra_v);
    rotulo(j, 308, 302, "Casilla", COR_TEXTO);
    casilla_desenhar(j, &casilla);
    jan_mostrar(j);
    registrar("componentes: pronto\n");

    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (botao_tratar(j, &contar, &e)) {
            formatar_texto(buf, sizeof buf, "clics: %d", ++cliques);
            g_retangulo(j, 170, 46, 200, 16, COR_FUNDO);
            rotulo(j, 170, 46, buf, COR_TEXTO);
            jan_mostrar_area(j, 170, 46, 200, 16);
            registrar("componentes: botao clicado %d\n", cliques);
        }
        if (botao_tratar(j, &apagar, &e)) {
            bool sim = confirmar("Confirmar", "¿Seguro?\nEsto borra el texto del área.");
            registrar("componentes: confirmar -> %s\n", sim ? "sim" : "nao");
            if (sim) { area_definir(&area, "", 0); area_desenhar(j, &area); }
        }
        if (botao_tratar(j, &aviso, &e)) {
            mensagem("Mensaje", "¡Hola! Este es un cuadro de mensaje\nde SaguiOS.");
            registrar("componentes: mensagem fechada\n");
        }
        if (caixa_tratar(j, &caixa, &e))
            registrar("componentes: caixa enter: %s\n", caixa.texto);
        if (area_tratar(j, &area, &e))
            registrar("componentes: area mudou (%d letras)\n", area.tam);
        else if (e.tipo == EV_TECLA && area.foco)
            registrar("componentes: area topo %d\n", area.topo);
        if (casilla_tratar(&casilla, &e)) {
            casilla_desenhar(j, &casilla);
            registrar("componentes: casilla %s\n", casilla.marcada ? "marcada" : "desmarcada");
        }
        /* A barra indeterminada anda com o tic do relogio. */
        if (barra_v && progresso_avancar(barra_v)) {
            retangulo_t r;
            progresso_rect(barra_v, &r);
            progresso_desenhar(j, barra_v);
            jan_mostrar_area(j, r.x - 2, r.y - 2, r.largura + 4, r.altura + 4);
        }
        if (lenzo && lenzo_tratar(j, lenzo, &e)) {
            lenzo_desenhar(j, lenzo);
            registrar("componentes: lenzo elemento %d\n", lenzo_selecionado(lenzo));
        }
        if (e.tipo == EV_TECLA && (caixa.foco || area.foco)) continue;   /* teclas: solo para quien tiene el foco */
        int r = lista_tratar(j, &lista, &e);
        if (r == LISTA_SELECIONOU) registrar("componentes: lista selecionou %s\n", itens[lista.sel]);
        if (r == LISTA_ATIVOU) registrar("componentes: lista ativou %s\n", itens[lista.sel]);
    }
    jan_fechar(j);
    return 0;
}
