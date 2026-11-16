// SPDX-License-Identifier: GPL-2.0-or-later
/* pluma_cinta.h - la cinta (Ribbon) de Pluma (mejora 132, D334): Inicio, Insertar, Diseño y Vista, más la
 * pestaña contextual «Objeto». Se incluye en pluma.c después de los iconos. Los ids son los mismos de acao(),
 * así que la cinta y los menús clásicos mandan las mismas órdenes. */

enum { A_C_TAM = 900, A_C_FONTE, A_C_ZOOM, A_C_COR, A_C_ESTILO, A_CLASICO };

#define CT(idv, tp, fl, tx, tp_tip, kt_, ic) { .id = (idv), .tipo = (tp), .flags = (fl), .texto = (tx), .tip = (tp_tip), .kt = (kt_), .icone = (ic) }

static const int ids_pegar[] = { A_COLAR, A_TUDO };
static const int ids_zoom[] = { A_Z50, A_Z75, A_Z100, A_Z150, A_Z200 };
static const char *const zoom_menu = "Zoom 50%\nZoom 75%\nZoom 100%\nZoom 150%\nZoom 200%";
static const char *lista_estilos[] = { N_("Normal"), N_("Título"), N_("Subtít."), N_("Cita") };

static cinta_ctrl_t cc_acceso[] = {
    CT(A_GUARDAR, CC_BOTON, CCF_SIN_TEXTO, N_("Guardar"), N_("Guardar\nGuarda el documento\nCtrl+S"), "1", ic_guardar),
    CT(A_DESFAZER, CC_BOTON, CCF_SIN_TEXTO, N_("Deshacer"), N_("Deshacer\nDeshace la última acción"), "2", ic_desfazer),
    CT(A_REFAZER, CC_BOTON, CCF_SIN_TEXTO, N_("Rehacer"), N_("Rehacer\nRepite lo deshecho"), "3", ic_refazer),
};

static cinta_ctrl_t cc_portapapeles[] = {
    CT(A_COLAR, CC_DIVIDIDO, CCF_GRANDE, N_("Pegar"), N_("Pegar\nInserta el contenido del portapapeles\nCtrl+V"), "V", ic_colar),
    CT(A_CORTAR, CC_BOTON, 0, N_("Cortar"), N_("Cortar\nQuita la selección y la guarda en el portapapeles"), "X", ic_cortar),
    CT(A_COPIAR, CC_BOTON, 0, N_("Copiar"), N_("Copiar\nGuarda la selección en el portapapeles"), "C", ic_copiar),
};
static cinta_ctrl_t cc_fuente[] = {
    CT(A_C_FONTE, CC_FUENTES, 0, NULL, N_("Fuente\nFamilia de la letra"), "FF", NULL),
    { .id = A_C_TAM, .tipo = CC_COMBO, .flags = CCF_SIGUE | CCF_EDITABLE, .tip = N_("Tamaño\nTamaño de la letra en puntos"), .kt = "FS",
      .lista = tams_txt, .n_lista = 16, .ancho = 56 },
    CT(A_NEGRITA, CC_CONMUT, 0, N_("N"), N_("Negrita\nPone la selección en negrita\nCtrl+B"), "1", NULL),
    CT(A_CURSIVA, CC_CONMUT, CCF_SIGUE, N_("K"), N_("Cursiva\nPone la selección en cursiva"), "2", NULL),
    CT(A_SUBLINHADO, CC_CONMUT, CCF_SIGUE, N_("S"), N_("Subrayado\nSubraya la selección"), "3", NULL),
    CT(A_C_COR, CC_COR, CCF_SIGUE, N_("Color de la letra"), N_("Color de la letra\nCambia el color del texto elegido"), "4", NULL),
};
static cinta_ctrl_t cc_paragrafo[] = {
    { .id = A_ESQ, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO, .texto = N_("Izquierda"), .tip = N_("Alinear a la izquierda"), .kt = "AI", .icone = ic_esq, .excl = 1 },
    { .id = A_CENTRO, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO | CCF_SIGUE, .texto = N_("Centro"), .tip = N_("Centrar"), .kt = "AC", .icone = ic_centro, .excl = 1 },
    { .id = A_DIR, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO | CCF_SIGUE, .texto = N_("Derecha"), .tip = N_("Alinear a la derecha"), .kt = "AD", .icone = ic_dir, .excl = 1 },
    { .id = A_JUST, .tipo = CC_EXCL, .flags = CCF_SIN_TEXTO | CCF_SIGUE, .texto = N_("Justificar"), .tip = N_("Justificar"), .kt = "AJ", .icone = ic_just, .excl = 1 },
    CT(A_VINETAS, CC_CONMUT, CCF_SIN_TEXTO, N_("Viñetas"), N_("Viñetas\nLista con viñetas"), "LV", ic_vinetas),
    CT(A_SANG_MENOS, CC_BOTON, CCF_SIN_TEXTO | CCF_SIGUE, N_("Menos sangría"), N_("Disminuir sangría"), "SM", ic_sang_menos),
    CT(A_SANG_MAIS, CC_BOTON, CCF_SIN_TEXTO | CCF_SIGUE, N_("Más sangría"), N_("Aumentar sangría"), "SA", ic_sang_mais),
};
static cinta_ctrl_t cc_estilos[] = {
    { .id = A_C_ESTILO, .tipo = CC_GALERIA, .tip = N_("Estilos\nAplica un estilo de texto"), .kt = "ES", .lista = lista_estilos, .n_lista = 4 },
};
static cinta_ctrl_t cc_edicion[] = {
    CT(A_BUSCAR, CC_BOTON, 0, N_("Buscar"), N_("Buscar\nBusca texto en el documento\nCtrl+F"), "B", ic_buscar),
    CT(A_SUBSTITUIR, CC_BOTON, 0, N_("Reemplazar"), N_("Reemplazar\nBusca y cambia texto\nCtrl+H"), "R", NULL),
    CT(A_TUDO, CC_BOTON, 0, N_("Seleccionar todo"), N_("Seleccionar todo\nSelecciona el documento entero"), "T", NULL),
};
static cinta_ctrl_t cc_asistente[] = {
    CT(A_ASISTENTE, CC_CONMUT, CCF_GRANDE, N_("Asistente IA"), N_("Asistente IA\nAbre el panel del asistente\nF12"), "IA", ic_ia),
};
static cinta_ctrl_t cc_ilustraciones[] = {
    CT(A_IMAGEM, CC_BOTON, CCF_GRANDE, N_("Imagen"), N_("Imagen\nInserta una imagen en el documento"), "I", ic_imagem),
    CT(A_WORDART, CC_BOTON, CCF_GRANDE, N_("WordArt"), N_("WordArt\nInserta un texto decorativo"), "W", ic_wordart),
};
static cinta_ctrl_t cc_texto_ins[] = {
    CT(A_DATA, CC_BOTON, 0, N_("Fecha"), N_("Fecha\nInserta la fecha de hoy"), "F", NULL),
    CT(A_PAGINA, CC_BOTON, 0, N_("Salto de página"), N_("Salto de página\nEmpieza una página nueva\nCtrl+Intro"), "S", NULL),
};
static cinta_ctrl_t cc_papel[] = {
    { .id = A_PAPEL_A4, .tipo = CC_EXCL, .texto = N_("A4"), .tip = N_("Papel A4"), .kt = "A", .excl = 2, .marcado = true },
    { .id = A_PAPEL_CARTA, .tipo = CC_EXCL, .texto = N_("Carta"), .tip = N_("Papel Carta"), .kt = "C", .excl = 2 },
};
static cinta_ctrl_t cc_formato[] = {
    CT(A_FONTE, CC_BOTON, 0, N_("Fuente..."), N_("Fuente\nAbre el diálogo de fuente"), "F", NULL),
    CT(A_PARAGRAFO, CC_BOTON, 0, N_("Párrafo..."), N_("Párrafo\nAbre el diálogo de párrafo"), "P", NULL),
};
static cinta_ctrl_t cc_zoom[] = {
    { .id = A_Z100, .tipo = CC_DESPLEG, .flags = CCF_GRANDE, .texto = N_("Zoom"), .tip = N_("Zoom\nCambia el tamaño de la hoja en pantalla"), .kt = "Z",
      .icone = ic_buscar, .menu = "Zoom 50%\nZoom 75%\nZoom 100%\nZoom 150%\nZoom 200%", .ids = ids_zoom },
    { .id = A_C_ZOOM, .tipo = CC_COMBO, .tip = N_("Porcentaje\nZoom en porcentaje"), .kt = "ZP", .lista = zooms_txt, .n_lista = 5, .ancho = 80 },
};
static cinta_ctrl_t cc_mostrar[] = {
    { .id = A_REGUA, .tipo = CC_CONMUT, .texto = N_("Regla"), .tip = N_("Regla\nMuestra u oculta la regla"), .kt = "R", .marcado = true },
    { .id = A_BARRA_DIBUJO, .tipo = CC_CONMUT, .texto = N_("Barra Dibujo"), .tip = N_("Barra Dibujo\nMuestra u oculta la barra de dibujo"), .kt = "D", .marcado = true },
    CT(A_CONTAR, CC_BOTON, 0, N_("Contar palabras"), N_("Contar palabras\nPáginas, palabras y párrafos"), "P", NULL),
};
static cinta_ctrl_t cc_interfaz[] = {
    CT(A_CLASICO, CC_BOTON, CCF_GRANDE, N_("Menús clásicos"), N_("Menús clásicos\nVuelve a los menús y barras de herramientas de siempre"), "M", ic_abrir),
};
static cinta_ctrl_t cc_objeto[] = {
    CT(A_FORMATO_IMAGEM, CC_BOTON, CCF_GRANDE, N_("Formato de imagen"), N_("Formato de imagen\nTamaño, borde y pie de la imagen"), "FI", ic_formato_imagem),
    CT(A_WORDART_EDITAR, CC_BOTON, CCF_GRANDE, N_("Editar WordArt"), N_("Editar WordArt\nCambia el texto y el estilo"), "EW", ic_wordart),
};

static cinta_grupo_t cg_inicio[] = {
    CINTA_GRUPO(N_("Portapapeles"), cc_portapapeles, 0, 5),
    CINTA_GRUPO(N_("Fuente"), cc_fuente, A_FONTE, 4),
    CINTA_GRUPO(N_("Párrafo"), cc_paragrafo, A_PARAGRAFO, 3),
    CINTA_GRUPO(N_("Estilos"), cc_estilos, 0, 1),
    CINTA_GRUPO(N_("Edición"), cc_edicion, 0, 2),
    CINTA_GRUPO(N_("Asistente"), cc_asistente, 0, 6),
};
static cinta_grupo_t cg_insertar[] = {
    CINTA_GRUPO(N_("Ilustraciones"), cc_ilustraciones, 0, 1),
    CINTA_GRUPO(N_("Texto"), cc_texto_ins, 0, 2),
};
static cinta_grupo_t cg_diseno[] = {
    CINTA_GRUPO(N_("Papel"), cc_papel, 0, 1),
    CINTA_GRUPO(N_("Formato"), cc_formato, 0, 2),
};
static cinta_grupo_t cg_vista[] = {
    CINTA_GRUPO(N_("Zoom"), cc_zoom, 0, 1),
    CINTA_GRUPO(N_("Mostrar"), cc_mostrar, 0, 2),
    CINTA_GRUPO(N_("Interfaz"), cc_interfaz, 0, 3),
};
static cinta_grupo_t cg_objeto[] = {
    CINTA_GRUPO(N_("Objeto"), cc_objeto, 0, 1),
};
#define PEST_OBJETO 4
static cinta_pest_t cp_pluma[] = {
    { .nome = N_("Inicio"), .kt = "I", .grupos = cg_inicio, .n = 6 },
    { .nome = N_("Insertar"), .kt = "N", .grupos = cg_insertar, .n = 2 },
    { .nome = N_("Diseño"), .kt = "D", .grupos = cg_diseno, .n = 2 },
    { .nome = N_("Vista"), .kt = "V", .grupos = cg_vista, .n = 3 },
    { .nome = N_("Objeto"), .kt = "O", .grupos = cg_objeto, .n = 1, .contextual = true },
};
