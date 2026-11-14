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

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
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
