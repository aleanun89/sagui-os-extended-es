// SPDX-License-Identifier: GPL-2.0-or-later
/* macaco_cinta.h - la cinta (Ribbon) de Macaco (mejora 132, D334): Inicio, Insertar, Fórmulas, Datos y Vista.
 * Se incluye en macaco.c justo antes de executar_acao(). El resto de macaco.c solo llama a las funciones
 * macaco_cinta_*() (declaradas arriba): así la fusión con otras ramas que tocan macaco.c es sencilla. Con
 * mc_activa = false todo vuelve a los menús y barras de siempre (Ver > «Cinta», o Ctrl+F1 en ese modo). */

static void executar_acao(int a);

enum { A_C_FONTE = 900, A_C_TAM, A_C_COR, A_C_REL, A_C_BORDE };

#define MC_ARCHIVO "/etc/macaco.modo"

static cinta_t mc;
static bool mc_activa = true;
static bool mc_log;

#define CT(idv, tp, fl, tx, tp_tip, kt_, ic) { .id = (idv), .tipo = (tp), .flags = (fl), .texto = (tx), .tip = (tp_tip), .kt = (kt_), .icone = (ic) }

static const char *const mc_tams[] = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "22", "24", "28", "36", "48", "72" };
static const int mc_ids_bordes[] = { A_BOR_0, A_BOR_0 + 1, A_BOR_0 + 2, A_BOR_0 + 3, A_BOR_0 + 4, A_BOR_0 + 5, A_BOR_0 + 6 };
static const int mc_ids_pegar[] = { A_COLAR, A_TUDO };
static const int mc_ids_graf[] = { A_GRAF_B, A_GRAF_H, A_GRAF_T };

static cinta_ctrl_t mcc_acceso[] = {
    CT(A_GUARDAR, CC_BOTON, CCF_SIN_TEXTO, "Guardar", "Guardar\nGuarda el libro\nCtrl+S", "1", ic98_guardar),
    CT(A_DESFAZER, CC_BOTON, CCF_SIN_TEXTO, "Deshacer", "Deshacer\nDeshace la última acción\nCtrl+Z", "2", ic98_desfazer),
    CT(A_ABRIR, CC_BOTON, CCF_SIN_TEXTO, "Abrir", "Abrir\nAbre un libro\nCtrl+O", "3", ic98_abrir),
};
static cinta_ctrl_t mcc_portapapeles[] = {
    { .id = A_COLAR, .tipo = CC_DIVIDIDO, .flags = CCF_GRANDE, .texto = "Pegar", .tip = "Pegar\nPega el portapapeles en la celda\nCtrl+V", .kt = "V",
      .icone = ic98_colar, .menu = "Pegar\nSeleccionar todo", .ids = mc_ids_pegar },
    CT(A_CORTAR, CC_BOTON, 0, "Cortar", "Cortar\nQuita las celdas y las guarda\nCtrl+X", "X", ic98_cortar),
    CT(A_COPIAR, CC_BOTON, 0, "Copiar", "Copiar\nGuarda las celdas en el portapapeles\nCtrl+C", "C", ic98_copiar),
    CT(A_BROCHA, CC_BOTON, CCF_SIN_TEXTO | CCF_SIGUE, "Copiar formato", "Copiar formato\nCopia el formato de la celda y lo pega en las que elija\nCtrl+Mayús+C", "FB", ic_brocha),
};
static cinta_ctrl_t mcc_fuente[] = {
    { .id = A_C_FONTE, .tipo = CC_COMBO, .tip = "Fuente\nFamilia de la letra", .kt = "FF", .lista = NOMES_FONTE, .n_lista = 3, .ancho = 140 },
    { .id = A_C_TAM, .tipo = CC_COMBO, .flags = CCF_SIGUE | CCF_EDITABLE, .tip = "Tamaño\nTamaño de la letra en puntos", .kt = "FS",
      .lista = mc_tams, .n_lista = 15, .ancho = 56 },
    CT(A_NEGRITA, CC_CONMUT, 0, "N", "Negrita\nCtrl+B", "1", NULL),
    CT(A_CURSIVA, CC_CONMUT, CCF_SIGUE, "K", "Cursiva\nCtrl+I", "2", NULL),
    CT(A_SUBLIN, CC_CONMUT, CCF_SIGUE, "S", "Subrayado\nCtrl+U", "3", NULL),
    { .id = A_C_BORDE, .tipo = CC_DESPLEG, .flags = CCF_SIGUE | CCF_SIN_TEXTO, .texto = "Bordes", .tip = "Bordes\nBordes de las celdas", .kt = "B",
      .icone = ic_bordas, .menu = "Sin bordes\nContorno\nTodos los bordes\nBorde inferior\nBorde superior\nBorde izquierdo\nBorde derecho", .ids = mc_ids_bordes },
    { .id = A_C_REL, .tipo = CC_COR, .flags = CCF_SIGUE, .texto = "Color de relleno", .tip = "Relleno\nColor del fondo de las celdas", .kt = "4",
      .icone = ic_relleno, .cor = 0xFFFF00 },
    { .id = A_C_COR, .tipo = CC_COR, .flags = CCF_SIGUE, .texto = "Color de la letra", .tip = "Color de la letra\nColor del texto de las celdas", .kt = "5",
      .icone = ic_cor_txt, .cor = 0xFF0000 },
};
static cinta_ctrl_t mcc_alineacion[] = {
    { .id = A_ESQ, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO, .texto = "Izquierda", .tip = "Alinear a la izquierda", .kt = "AI", .icone = ic98_esq, .excl = 1 },
    { .id = A_CENTRO, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO | CCF_SIGUE, .texto = "Centro", .tip = "Centrar", .kt = "AC", .icone = ic98_centro, .excl = 1 },
    { .id = A_DIR, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO | CCF_SIGUE, .texto = "Derecha", .tip = "Alinear a la derecha", .kt = "AD", .icone = ic98_dir, .excl = 1 },
    CT(A_AJUSTAR, CC_CONMUT, CCF_SIN_TEXTO | CCF_SIGUE, "Ajustar texto", "Ajustar texto\nMuestra el texto largo en varias líneas dentro de la celda", "AJ", ic_ajustar),
};
static cinta_ctrl_t mcc_numero[] = {
    CT(A_MOEDA, CC_BOTON, 0, "Moneda", "Estilo de moneda\nMuestra el número como moneda", "M", NULL),
    CT(A_PERCENT, CC_BOTON, 0, "Porcentaje", "Estilo porcentual\nMultiplica por 100 y añade %", "P", NULL),
    CT(A_MILHAR, CC_BOTON, 0, "Miles", "Separador de miles", "SM", NULL),
    CT(A_DEC_MAIS, CC_BOTON, CCF_SIN_TEXTO | CCF_SALTO, "Más decimales", "Más decimales", "D1", ic_dec_mais),
    CT(A_DEC_MENOS, CC_BOTON, CCF_SIN_TEXTO | CCF_SIGUE, "Menos decimales", "Menos decimales", "D2", ic_dec_menos),
};
static cinta_ctrl_t mcc_celdas[] = {
    CT(A_LINHA_INS, CC_BOTON, 0, "Insertar fila", "Insertar fila\nAñade una fila sobre la celda activa", "IF", NULL),
    CT(A_LINHA_DEL, CC_BOTON, 0, "Eliminar fila", "Eliminar fila", "EF", NULL),
    CT(A_COL_INS, CC_BOTON, CCF_SALTO, "Insertar columna", "Insertar columna\nAñade una columna a la izquierda", "IC", NULL),
    CT(A_COL_DEL, CC_BOTON, 0, "Eliminar columna", "Eliminar columna", "EC", NULL),
    CT(A_COMB_CENTRAR, CC_BOTON, CCF_SALTO, "Combinar y centrar", "Combinar y centrar\nUne las celdas elegidas y centra el texto", "CM", ic_combinar),
    CT(A_SEPARAR, CC_BOTON, 0, "Separar celdas", "Separar celdas\nDeshace la combinación", "SC", NULL),
};
static cinta_ctrl_t mcc_edicion[] = {
    CT(A_SOMA, CC_BOTON, CCF_GRANDE, "Autosuma", "Autosuma\nSuma las celdas de encima o de la izquierda", "AS", ic_soma),
    CT(A_ORD_A, CC_BOTON, 0, "Ordenar A-Z", "Ordenar de A a Z", "OA", ic_ord_a),
    CT(A_ORD_Z, CC_BOTON, 0, "Ordenar Z-A", "Ordenar de Z a A", "OZ", ic_ord_z),
    CT(A_BUSCAR, CC_BOTON, 0, "Buscar", "Buscar y reemplazar\nBusca un texto en la hoja y lo cambia por otro\nCtrl+F", "SB", NULL),
};
static cinta_ctrl_t mcc_graficos[] = {
    { .id = A_GRAFICO, .tipo = CC_DIVIDIDO, .flags = CCF_GRANDE, .texto = "Gráfico", .tip = "Gráfico\nCrea un gráfico con las celdas elegidas", .kt = "G",
      .icone = ic_grafico, .menu = "Gráfico de barras\nGráfico de barras horizontales\nGráfico de tarta", .ids = mc_ids_graf },
};
static cinta_ctrl_t mcc_comentarios[] = {
    CT(A_COMENTARIO, CC_BOTON, CCF_GRANDE, "Comentario", "Comentario\nAñade o edita el comentario de la celda\nMayús+F2", "CN", NULL),
    CT(A_COM_BORRAR, CC_BOTON, 0, "Eliminar comentario", "Eliminar comentario", "CQ", NULL),
};
static cinta_ctrl_t mcc_hojas[] = {
    CT(A_FOLHA_NOVA, CC_BOTON, 0, "Hoja nueva", "Hoja nueva\nAñade una hoja al libro", "HN", NULL),
    CT(A_FOLHA_DEL, CC_BOTON, 0, "Eliminar hoja", "Eliminar hoja", "HE", NULL),
    CT(A_FOLHA_REN, CC_BOTON, 0, "Cambiar nombre", "Cambiar nombre de la hoja", "HR", NULL),
};
static cinta_ctrl_t mcc_funciones[] = {
    CT(A_FUNCAO, CC_BOTON, CCF_GRANDE, "Insertar función", "Insertar función\nAbre el asistente de funciones\nMayús+F3", "IN", ic_igual),
    CT(A_SOMA, CC_BOTON, CCF_GRANDE, "Autosuma", "Autosuma", "AS", ic_soma),
};
static cinta_ctrl_t mcc_calculo[] = {
    CT(A_RECALC, CC_BOTON, 0, "Recalcular", "Recalcular\nVuelve a calcular todas las fórmulas", "RC", NULL),
    CT(A_FORMULAS, CC_CONMUT, 0, "Mostrar fórmulas", "Mostrar fórmulas\nMuestra las fórmulas en vez de los resultados", "MF", NULL),
};
static cinta_ctrl_t mcc_datos[] = {
    CT(A_ORD_A, CC_BOTON, CCF_GRANDE, "Ordenar A-Z", "Ordenar de A a Z", "OA", ic_ord_a),
    CT(A_ORD_Z, CC_BOTON, CCF_GRANDE, "Ordenar Z-A", "Ordenar de Z a A", "OZ", ic_ord_z),
};
static cinta_ctrl_t mcc_filtro[] = {
    CT(A_AUTOFILTRO, CC_CONMUT, CCF_GRANDE, "Autofiltro", "Autofiltro\nPone flechas de filtro en la fila de encabezado\nCtrl+Mayús+L", "AF", NULL),
    CT(A_FILTRO_QUITAR, CC_BOTON, 0, "Quitar filtros", "Quitar filtros\nMuestra todas las filas otra vez", "QF", NULL),
};
static cinta_ctrl_t mcc_herram[] = {
    CT(A_VALIDACION, CC_BOTON, CCF_GRANDE, "Validación de datos", "Validación de datos\nLimita lo que se puede escribir: una lista o un rango de números", "VD", NULL),
};
static cinta_ctrl_t mcc_condicional[] = {
    CT(A_FORMATO_COND, CC_BOTON, CCF_GRANDE, "Formato condicional", "Formato condicional\nColorea las celdas según su valor (mayor, menor, igual, entre)", "FC", NULL),
    CT(A_COND_BORRAR, CC_BOTON, 0, "Borrar reglas", "Borrar las reglas de formato condicional", "BC", NULL),
};
static cinta_ctrl_t mcc_csv[] = {
    CT(A_CSV_IMP, CC_BOTON, 0, "Importar CSV...", "Importar CSV\nLee un archivo de valores separados", "CI", NULL),
    CT(A_CSV_EXP, CC_BOTON, 0, "Exportar CSV...", "Exportar CSV\nGuarda la hoja como CSV", "CE", NULL),
    CT(A_IR, CC_BOTON, 0, "Ir a...", "Ir a\nSalta a una celda o a un intervalo\nCtrl+G", "IR", NULL),
};
static cinta_ctrl_t mcc_mostrar[] = {
    CT(A_GRADE, CC_CONMUT, 0, "Cuadrícula", "Cuadrícula\nMuestra u oculta las líneas de la hoja", "CU", NULL),
    CT(A_FORMULAS, CC_CONMUT, 0, "Fórmulas", "Mostrar fórmulas", "MF", NULL),
    CT(A_LARG, CC_BOTON, 0, "Ancho de columnas", "Ancho de las columnas", "AN", NULL),
    CT(A_INMOV, CC_BOTON, 0, "Inmovilizar paneles", "Inmovilizar paneles\nFija las filas y columnas de arriba y de la izquierda", "IM", NULL),
};
static cinta_ctrl_t mcc_ventana[] = {
    CT(A_ASISTENTE, CC_CONMUT, CCF_GRANDE, "Asistente IA", "Asistente IA\nAbre el panel del asistente\nF12", "IA", ic_ia),
    CT(A_CLASICO, CC_BOTON, CCF_GRANDE, "Menús clásicos", "Menús clásicos\nVuelve a los menús y barras de siempre", "MC", ic98_abrir),
};

static cinta_grupo_t mcg_inicio[] = {
    CINTA_GRUPO("Portapapeles", mcc_portapapeles, 0, 6),
    CINTA_GRUPO("Fuente", mcc_fuente, A_CELULAS, 7),
    CINTA_GRUPO("Alineación", mcc_alineacion, 0, 3),
    CINTA_GRUPO("Número", mcc_numero, 0, 2),
    CINTA_GRUPO("Celdas", mcc_celdas, 0, 1),
    CINTA_GRUPO("Edición", mcc_edicion, 0, 5),
};
static cinta_grupo_t mcg_insertar[] = {
    CINTA_GRUPO("Gráficos", mcc_graficos, 0, 1),
    CINTA_GRUPO("Hojas", mcc_hojas, 0, 2),
    CINTA_GRUPO("Comentarios", mcc_comentarios, 0, 3),
};
static cinta_grupo_t mcg_formulas[] = {
    CINTA_GRUPO("Funciones", mcc_funciones, 0, 1),
    CINTA_GRUPO("Cálculo", mcc_calculo, 0, 2),
};
static cinta_grupo_t mcg_datos[] = {
    CINTA_GRUPO("Ordenar", mcc_datos, 0, 1),
    CINTA_GRUPO("Archivos y celdas", mcc_csv, 0, 2),
    CINTA_GRUPO("Filtro", mcc_filtro, 0, 3),
    CINTA_GRUPO("Herramientas", mcc_herram, 0, 4),
    CINTA_GRUPO("Formato condicional", mcc_condicional, 0, 5),
};
static cinta_grupo_t mcg_vista[] = {
    CINTA_GRUPO("Mostrar", mcc_mostrar, 0, 1),
    CINTA_GRUPO("Ventana", mcc_ventana, 0, 2),
};
static cinta_pest_t mcp[] = {
    { .nome = "Inicio", .kt = "I", .grupos = mcg_inicio, .n = 6 },
    { .nome = "Insertar", .kt = "N", .grupos = mcg_insertar, .n = 3 },
    { .nome = "Fórmulas", .kt = "M", .grupos = mcg_formulas, .n = 2 },
    { .nome = "Datos", .kt = "D", .grupos = mcg_datos, .n = 5 },
    { .nome = "Vista", .kt = "V", .grupos = mcg_vista, .n = 2 },
};

static const int mc_ids_archivo[] = { A_NOVO, A_ABRIR, 0, A_GUARDAR, A_GUARDAR_COMO, 0, A_CSV_IMP, A_CSV_EXP, A_PDF, 0, A_SAIR };
static const char *const mc_menu_archivo =
    "Nuevo\tCtrl+N\nAbrir...\tCtrl+O\n-\nGuardar\tCtrl+S\nGuardar como...\n-\nImportar CSV...\nExportar CSV...\nExportar a PDF...\tCtrl+P\n-\nSalir";

static int macaco_barra_a(void) { return mc_activa ? cinta_altura(&mc) : MENU_A + 2 * FERR_A; }

static void mc_escribir_modo(void)
{
    int fd = abrir(MC_ARCHIVO, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever_tudo(fd, mc_activa ? "cinta\n" : "clasico\n", mc_activa ? 6 : 8);
    fechar(fd);
}

/* Opciones --cinta-log, --clasico y --cinta (se quitan de argv, como ag_app_opciones). */
static void macaco_cinta_args(int *argc, char **argv)
{
    int fd = abrir(MC_ARCHIVO, ABRIR_LER);
    if (fd >= 0) {
        char b[16];
        int n = ler(fd, b, sizeof b);
        fechar(fd);
        if (n >= 7 && memcmp(b, "clasico", 7) == 0) mc_activa = false;
    }
    int k = 1;
    for (int i = 1; i < *argc; i++) {
        if (compara_texto(argv[i], "--cinta-log") == 0) mc_log = true;
        else if (compara_texto(argv[i], "--clasico") == 0) mc_activa = false;
        else if (compara_texto(argv[i], "--cinta") == 0) mc_activa = true;
        else argv[k++] = argv[i];
    }
    *argc = k;
}

static void macaco_cinta_iniciar(void)
{
    mc.acceso = mcc_acceso; mc.n_acceso = 3;
    cinta_iniciar(&mc, 0, 0, jan->larg, mcp, 5);
    mc.archivo = mc_menu_archivo;
    mc.archivo_ids = mc_ids_archivo;
    mc.depurar = mc_log;
    cinta_traducir(&mc);
}

static void macaco_cinta_layout(void)
{
    if (!mc_activa) return;
    cinta_largura(&mc, 0, 0, jan->larg);
    /* los controles clásicos se apartan: así no se pisan con la hoja */
    ferr1.n = 0; ferr2.n = 0; menu.n = 0;
    c_fonte.y = c_tam.y = -300;
}

static bool macaco_cinta_desenhar(void)
{
    if (!mc_activa) return false;
    const fmt_t *f = fmt_de(celula_de(folha_ativa(), cel_l, cel_c));
    cinta_marcar(&mc, A_NEGRITA, (f->est & EST_NEGRITA) != 0);
    cinta_marcar(&mc, A_CURSIVA, (f->est & EST_CURSIVA) != 0);
    cinta_marcar(&mc, A_SUBLIN, (f->est & EST_SUBLIN) != 0);
    cinta_marcar(&mc, A_ESQ, false); cinta_marcar(&mc, A_CENTRO, false); cinta_marcar(&mc, A_DIR, false);
    if (f->al == AL_ESQ) cinta_marcar(&mc, A_ESQ, true);
    else if (f->al == AL_CENTRO) cinta_marcar(&mc, A_CENTRO, true);
    else if (f->al == AL_DIR) cinta_marcar(&mc, A_DIR, true);
    cinta_marcar(&mc, A_AJUSTAR, f->aj != 0);
    cinta_marcar(&mc, A_AUTOFILTRO, folha_ativa()->af_ativo);
    cinta_marcar(&mc, A_GRADE, grade_visivel);
    cinta_marcar(&mc, A_FORMULAS, mostrar_formulas);
    cinta_marcar(&mc, A_ASISTENTE, ag_visible);
    cinta_ctrl_t *kf = cinta_buscar(&mc, A_C_FONTE), *kt = cinta_buscar(&mc, A_C_TAM);
    if (kf && !kf->cb.foco) combo_definir(&kf->cb, NOMES_FONTE[f->fam < 3 ? f->fam : 0]);
    if (kt && !kt->cb.foco) { char t[8]; formatar_texto(t, sizeof t, "%d", f->tam ? f->tam : 10); combo_definir(&kt->cb, t); }
    cinta_desenhar(jan, &mc);
    return true;
}

static void macaco_cinta_alternar(void)
{
    mc_activa = !mc_activa;
    mc_escribir_modo();
    registrar("macaco: modo %s", mc_activa ? "cinta" : "clasico");
    layout();
}

static void macaco_cinta_orden(int id)
{
    cinta_ctrl_t *k = cinta_buscar(&mc, id);
    confirmar_edicao();
    if (id >= A_BOR_0 && id < A_BOR_0 + 7) { bordas_aplicar(id - A_BOR_0); return; }
    switch (id) {
    case A_C_FONTE: if (k && k->cb.sel >= 0) formato_aplicar(FO_FAM, (uint32_t)k->cb.sel, NULL); return;
    case A_C_TAM: {
        int v = k ? texto_para_int(k->cb.texto) : 0;
        if (v >= 1 && v <= 72) formato_aplicar(FO_TAM, (uint32_t)v, NULL);
        return;
    }
    case A_C_COR: if (k) { cor_txt_atual = k->cor; formato_aplicar(FO_COR, k->cor, NULL); } return;
    case A_C_REL: if (k) { cor_rel_atual = k->cor; formato_aplicar(FO_REL, REL_ATIVO | (k->cor & 0xFFFFFF), NULL); } return;
    case A_C_BORDE: return;
    case A_CLASICO: macaco_cinta_alternar(); return;
    }
    executar_acao(id);
}

/* true si la cinta se quedó con el evento */
static bool macaco_cinta_evento(const evento_t *e)
{
    if (!mc_activa) {
        if (e->tipo == EV_TECLA && e->tecla == 0x10A && (e->modificadores & 2)) { macaco_cinta_alternar(); return true; }
        return false;
    }
    int id = cinta_tratar(jan, &mc, e);
    if (cinta_cambio(&mc)) layout();
    if (id > 0) { macaco_cinta_orden(id); return true; }
    if (id == CINTA_TRATADO) return true;
    return false;
}
