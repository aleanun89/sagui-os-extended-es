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

/* ---- demo de los componentes modernos (v8.5.5.2, D423): `componentes --moderno` ---- */
#define DM_L 760
#define DM_A 480
enum { DB_PRIM, DB_SEC, DB_PLANO, DB_PELIGRO, DB_ICONO, DB_ICONO_ON, DB_REDONDO, DB_DESAB, DB_FOCO, DB_AVISO, DB_N };
static mbotao_t dm_bt[DB_N];
static segmentado_t dm_vistas, dm_rango;
static interruptor_t dm_sw[3];
static chip_t dm_chip[4];
static campo_busca_t dm_busca;
static cartao_t dm_cartao[3];
static dica_t dm_dica;
static toast_t dm_toast;
static int dm_fase;
static int dm_chips_ocultos;   /* los chips con «x» que se cerraron (bit k) */
static int dm_foco;

static void dm_seccion(jan_t *j, int y, const char *t)
{
    g_texto(j, 16, y, t, mod_tema.acento);
    g_retangulo(j, 16 + g_largura_texto(t) + 8, y + 8, DM_L - 32 - g_largura_texto(t) - 8, 1, mod_tema.borde);
}

static void dm_construir(void)
{
    static const char *const ic[DB_N] = { "guardar", NULL, "ajustes", "excluir", "voltar", "estrela", "informacao", NULL, NULL, "aviso" };
    static const char *const tx[DB_N] = { "Guardar", "Cancelar", "Opciones", "Borrar", NULL, NULL, NULL, "No disponible", "Con foco", "Mostrar aviso" };
    static const int va[DB_N] = { MB_PRIMARIO, MB_SECUNDARIO, MB_PLANO, MB_PELIGRO, MB_ICONO, MB_ICONO, MB_ICONO_REDONDO, MB_SECUNDARIO, MB_SECUNDARIO, MB_PRIMARIO };
    static const char *const di[DB_N] = { "Guardar el documento (Ctrl+S)", "Cancelar", "Opciones", "Borrar la selección", "Volver atrás",
                                          "Favorito", "Información", "Este botón está deshabilitado", "Botón con foco de teclado", "Muestra un toast" };
    int x = 16;
    for (int k = 0; k < DB_N; k++) {
        mbotao_t *b = &dm_bt[k];
        memset(b, 0, sizeof *b);
        b->variante = va[k]; b->texto = tx[k]; b->icone = ic[k]; b->dica = di[k];
        if (k == DB_AVISO) { b->x = DM_L - 16 - 150; b->y = 424; b->l = 0; mbotao_ajustar(b); b->x = DM_L - 16 - b->l; continue; }
        b->x = x; b->y = 40;
        mbotao_ajustar(b);
        x += b->l + 8;
    }
    dm_bt[DB_ICONO_ON].ativo = true;
    dm_bt[DB_DESAB].desabilitado = true;
    dm_bt[DB_FOCO].foco = true;
    memset(&dm_vistas, 0, sizeof dm_vistas);
    dm_vistas.x = 16; dm_vistas.y = 100; dm_vistas.n = 4; dm_vistas.hover = -1;
    static const char *const vi[4] = { "lista", "tabela", "quadro", "imagem" };
    static const char *const vd[4] = { "Lista", "Detalles", "Iconos", "Mosaicos" };
    for (int k = 0; k < 4; k++) { dm_vistas.icones[k] = vi[k]; dm_vistas.dicas[k] = vd[k]; }
    segmentado_ajustar(&dm_vistas);
    memset(&dm_rango, 0, sizeof dm_rango);
    dm_rango.x = 190; dm_rango.y = 100; dm_rango.n = 3; dm_rango.hover = -1; dm_rango.sel = 1;
    dm_rango.textos[0] = "Día"; dm_rango.textos[1] = "Semana"; dm_rango.textos[2] = "Mes";
    segmentado_ajustar(&dm_rango);
    static const char *const sw_t[3] = { "Sonido", "Avisos", "Sincronizar" };
    int sx = dm_rango.x + dm_rango.l + 24;
    for (int k = 0; k < 3; k++) {
        memset(&dm_sw[k], 0, sizeof dm_sw[k]);
        dm_sw[k].x = sx; dm_sw[k].y = 104; dm_sw[k].texto = sw_t[k];
        sx += interruptor_largura(&dm_sw[k]) + 20;
    }
    dm_sw[0].ligado = true;
    static const char *const ct[4] = { "Documentos", "Imágenes", "Importante", "foto.png" };
    int cx = 16;
    for (int k = 0; k < 4; k++) {
        memset(&dm_chip[k], 0, sizeof dm_chip[k]);
        dm_chip[k].texto = ct[k];
        dm_chip[k].x = cx; dm_chip[k].y = 166;
        if (k == 0) dm_chip[k].sel = true;
        if (k == 2) dm_chip[k].cor = 0xD83A2E;
        if (k == 3) dm_chip[k].fechavel = true;
        chip_ajustar(&dm_chip[k]);
        cx += dm_chip[k].l + 8;
    }
    memset(&dm_busca, 0, sizeof dm_busca);
    dm_busca.c.x = 440; dm_busca.c.y = 164; dm_busca.c.l = 200;
    dm_busca.dica = "Buscar";
    static const char *const tt[3] = { "Favoritos", "Unidades", "Recientes" };
    static const char *const ti[3] = { "estrela", "disco", "historico" };
    for (int k = 0; k < 3; k++) {
        memset(&dm_cartao[k], 0, sizeof dm_cartao[k]);
        dm_cartao[k].x = 16 + k * 244; dm_cartao[k].y = 318; dm_cartao[k].l = 232; dm_cartao[k].a = 88;
        dm_cartao[k].titulo = tt[k]; dm_cartao[k].icone = ti[k];
    }
    dm_cartao[1].sel = true;
}

static void dm_chip_pos(void)
{
    int cx = 16;
    for (int k = 0; k < 4; k++) {
        dm_chip[k].x = cx;
        if (!(dm_chips_ocultos & (1 << k))) cx += dm_chip[k].l + 8;
    }
}

static void dm_barras(jan_t *j)
{
    g_retangulo(j, 16, 232, DM_L - 32, 66, mod_tema.barra);
    g_texto(j, 16, 232, "30 %", mod_tema.texto_fraco);
    progresso_mod_desenhar(j, 120, 236, 240, 8, 30, 0, mod_tema.barra);
    g_texto(j, 16, 254, "78 % (uso)", mod_tema.texto_fraco);
    progresso_mod_desenhar(j, 120, 258, 240, 8, 78, MP_USO, mod_tema.barra);
    g_texto(j, 16, 276, "96 % (uso)", mod_tema.texto_fraco);
    progresso_mod_desenhar(j, 120, 280, 240, 8, 96, MP_USO, mod_tema.barra);
    g_texto(j, 400, 232, "Indeterminado", mod_tema.texto_fraco);
    progresso_mod_indet(j, 400, 254, 240, 8, dm_fase, mod_tema.barra);
    g_texto(j, 400, 270, "Insignias", mod_tema.texto_fraco);
    int x = 400 + 8 * 10 + 6;
    x += insignia_desenhar(j, x, 270, 3, 0) + 6;
    x += insignia_desenhar(j, x, 270, 12, mod_tema.acento) + 6;
    insignia_desenhar(j, x, 270, 120, mod_tema.foco);
}

static void dm_tarjetas(jan_t *j)
{
    for (int k = 0; k < 3; k++) {
        cartao_t *c = &dm_cartao[k];
        cartao_desenhar(j, c);
        int x, y, l, a;
        cartao_interior(c, &x, &y, &l, &a);
        if (k == 0) {
            g_texto(j, x, y, "Documentos", mod_tema.texto);
            g_texto(j, x, y + 18, "Descargas", mod_tema.texto);
        } else if (k == 1) {
            g_texto(j, x, y, "Sistema", mod_tema.texto);
            progresso_mod_desenhar(j, x, y + 20, l, 8, 42, MP_USO, mod_tema.superficie);
            g_texto(j, x, y + 32, "12 GB libres", mod_tema.texto_fraco);
        } else {
            g_texto(j, x, y, "informe.pdf", mod_tema.texto);
            g_texto(j, x, y + 18, "notas.txt", mod_tema.texto_fraco);
        }
    }
}

static void dm_todo(jan_t *j)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, mod_tema.barra);
    dm_seccion(j, 12, "Botones");
    for (int k = 0; k < DB_N; k++) mbotao_desenhar(j, &dm_bt[k]);
    dm_seccion(j, 76, "Selectores e interruptores");
    segmentado_desenhar(j, &dm_vistas);
    segmentado_desenhar(j, &dm_rango);
    for (int k = 0; k < 3; k++) interruptor_desenhar(j, &dm_sw[k]);
    dm_seccion(j, 142, "Chips y búsqueda");
    for (int k = 0; k < 4; k++) if (!(dm_chips_ocultos & (1 << k))) chip_desenhar(j, &dm_chip[k]);
    campo_busca_desenhar(j, &dm_busca);
    dm_seccion(j, 208, "Progreso e insignias");
    dm_barras(j);
    dm_seccion(j, 298, "Tarjetas");
    dm_tarjetas(j);
    g_texto(j, 16, 430, "Ratón sobre un botón: tooltip. Tab: foco.", mod_tema.texto_fraco);
    jan_mostrar(j);
}

static void dm_registrar_posiciones(void)
{
    for (int k = 0; k < DB_N; k++) registrar("componentes: pos boton%d %d %d %d %d\n", k, dm_bt[k].x, dm_bt[k].y, dm_bt[k].l, dm_bt[k].a);
    for (int k = 0; k < 4; k++) { int x, y, l, a; segmentado_rect(&dm_vistas, k, &x, &y, &l, &a); registrar("componentes: pos vista%d %d %d %d %d\n", k, x, y, l, a); }
    for (int k = 0; k < 3; k++) registrar("componentes: pos sw%d %d %d %d %d\n", k, dm_sw[k].x, dm_sw[k].y, interruptor_largura(&dm_sw[k]), INTERRUPTOR_A);
    registrar("componentes: pos chip3 %d %d %d %d\n", dm_chip[3].x, dm_chip[3].y, dm_chip[3].l, dm_chip[3].a);
    registrar("componentes: pos busca %d %d %d %d\n", dm_busca.c.x, dm_busca.c.y, dm_busca.c.l, mod_tema.altura);
}

static int demo_moderno(void)
{
    jan_t *j = jan_abrir("Componentes modernos", DM_L, DM_A);
    if (!j) return 1;
    mod_tema_padrao(&mod_tema);
    dica_iniciar(&dm_dica);
    toast_iniciar(&dm_toast);
    dm_construir();
    dm_todo(j);
    dm_registrar_posiciones();
    registrar("componentes: moderno pronto\n");
    evento_t e;
    unsigned ult = tempo_ms();
    for (;;) {
        if (proximo_evento(j->id, &e, false) != 1) {
            dica_tic(j, &dm_dica);
            toast_tic(j, &dm_toast);
            if (tempo_ms() - ult >= 60) {
                ult = tempo_ms();
                if (!dm_toast.visivel && !dm_dica.visivel) { dm_fase += 3; dm_barras(j); }
            }
            dormir(20);
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_MOVER) {
            int alvo = -1, ex = 0, ey = 0, el = 0, ea = 0;
            const char *txt = NULL;
            for (int k = 0; k < DB_N && alvo < 0; k++)
                if (dm_bt[k].dica && e.x >= dm_bt[k].x && e.x < dm_bt[k].x + dm_bt[k].l && e.y >= dm_bt[k].y && e.y < dm_bt[k].y + dm_bt[k].a) {
                    alvo = k; txt = dm_bt[k].dica; ex = dm_bt[k].x; ey = dm_bt[k].y; el = dm_bt[k].l; ea = dm_bt[k].a;
                }
            int sk = segmentado_em(&dm_vistas, e.x, e.y);
            if (alvo < 0 && sk >= 0) { alvo = 100 + sk; txt = dm_vistas.dicas[sk]; segmentado_rect(&dm_vistas, sk, &ex, &ey, &el, &ea); }
            dica_mover(j, &dm_dica, alvo, txt, ex, ey, el, ea);
        } else {
            dica_ocultar(j, &dm_dica);
        }
        if (e.tipo == EV_TECLA && e.tecla == '\t') {   /* el foco de teclado pasa al siguiente botón */
            dm_bt[dm_foco].foco = false;
            mbotao_desenhar(j, &dm_bt[dm_foco]);
            do dm_foco = (dm_foco + 1) % DB_N; while (dm_bt[dm_foco].desabilitado);
            dm_bt[dm_foco].foco = true;
            mbotao_desenhar(j, &dm_bt[dm_foco]);
            continue;
        }
        for (int k = 0; k < DB_N; k++) {
            if (!mbotao_tratar(j, &dm_bt[k], &e)) continue;
            registrar("componentes: boton %d\n", k);
            if (k == DB_AVISO) { toast_mostrar(j, &dm_toast, "Documento guardado", TOAST_OK, 2200); registrar("componentes: toast\n"); }
            if (k == DB_ICONO_ON) { dm_bt[k].ativo = !dm_bt[k].ativo; mbotao_desenhar(j, &dm_bt[k]); }
        }
        int sv = segmentado_tratar(j, &dm_vistas, &e);
        if (sv >= 0) registrar("componentes: vista %d\n", sv);
        int sr = segmentado_tratar(j, &dm_rango, &e);
        if (sr >= 0) registrar("componentes: rango %d\n", sr);
        for (int k = 0; k < 3; k++) {
            if (!interruptor_tratar(j, &dm_sw[k], &e)) continue;
            registrar("componentes: interruptor %d %s\n", k, dm_sw[k].ligado ? "sim" : "nao");
        }
        for (int k = 0; k < 4; k++) {
            if (dm_chips_ocultos & (1 << k)) continue;
            int r = chip_tratar(j, &dm_chip[k], &e);
            if (r == CHIP_CLIQUE && k < 3) { dm_chip[k].sel = !dm_chip[k].sel; chip_desenhar(j, &dm_chip[k]); registrar("componentes: chip %d %s\n", k, dm_chip[k].sel ? "sim" : "nao"); }
            if (r == CHIP_FECHAR) {
                dm_chips_ocultos |= 1 << k;
                dm_chip_pos();
                g_retangulo(j, 16, 160, 410, 34, mod_tema.barra);
                for (int q = 0; q < 4; q++) if (!(dm_chips_ocultos & (1 << q))) chip_desenhar(j, &dm_chip[q]);
                jan_mostrar_area(j, 16, 160, 410, 34);
                registrar("componentes: chip fechado %d\n", k);
            }
        }
        int rb = campo_busca_tratar(j, &dm_busca, &e);
        if (rb == BUSCA_MUDOU) registrar("componentes: busca '%s'\n", dm_busca.c.texto);
        else if (rb == BUSCA_LIMPOU) registrar("componentes: busca limpiada\n");
        else if (rb == BUSCA_ENTER) registrar("componentes: busca enter '%s'\n", dm_busca.c.texto);
        for (int k = 0; k < 3; k++) {
            bool h = cartao_tratar(j, &dm_cartao[k], &e);
            (void)h;
        }
    }
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--escritorio") == 0) return demo_escritorio();
    if (argc >= 2 && compara_texto(argv[1], "--moderno") == 0) return demo_moderno();
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
