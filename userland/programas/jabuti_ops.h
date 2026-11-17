// SPDX-License-Identifier: GPL-3.0-or-later
/* jabuti_ops.h - la tabla de bloques de «Bloques» (D377). Lo incluye solo userland/jabuti/jb_modelo.c;
 * está aquí, junto al programa, para que las marcas N_() entren en el catálogo de traducción «jabuti».
 *
 * Rótulos: «#» = ranura de número, «$» = ranura de texto, «^» = ranura de condición (hexágono),
 * «@» = lista desplegable (el tipo de la lista lo da el campo «menu»). */
#ifndef JABUTI_OPS_H
#define JABUTI_OPS_H

const char *const jb_nombre_cat[NCAT] = {
    N_("Eventos"), N_("Movimiento"), N_("Apariencia"), N_("Sonido"), N_("Lápiz"), N_("Control"), N_("Sensores"), N_("Operadores"), N_("Variables"),
};
const uint32_t jb_color_cat[NCAT] = {
    0xFFBF00, 0x4C97FF, 0x9966FF, 0xCF63CF, 0x0FBD8C, 0xFFAB19, 0x5CB1D6, 0x59C059, 0xFF8C1A,
};

/* clave, categoría, tipo, menú, rótulo, valores por defecto */
const op_t jb_ops[O_FIN] = {
    [O_BANDERA]  = { "bandera",   C_EVENTOS, K_HAT,   M_NADA,    N_("al pulsar la bandera verde"), { 0 } },
    [O_TECLA]    = { "tecla",     C_EVENTOS, K_HAT,   M_TECLA,   N_("al pulsar la tecla @"), { "espacio" } },
    [O_CLIC]     = { "clic",      C_EVENTOS, K_HAT,   M_NADA,    N_("al hacer clic en este personaje"), { 0 } },
    [O_RECIBIR]  = { "recibir",   C_EVENTOS, K_HAT,   M_MENSAJE, N_("al recibir @"), { "mensaje1" } },
    [O_ENVIAR]   = { "enviar",    C_EVENTOS, K_STACK, M_MENSAJE, N_("enviar @"), { "mensaje1" } },

    [O_MOVER]    = { "mover",     C_MOV, K_STACK, M_NADA,     N_("mover # pasos"), { "10" } },
    [O_GIRAR_D]  = { "girar-der", C_MOV, K_STACK, M_NADA,     N_("girar a la derecha # °"), { "15" } },
    [O_GIRAR_I]  = { "girar-izq", C_MOV, K_STACK, M_NADA,     N_("girar a la izquierda # °"), { "15" } },
    [O_IR_XY]    = { "ir-xy",     C_MOV, K_STACK, M_NADA,     N_("ir a x: # y: #"), { "0", "0" } },
    [O_IR_A]     = { "ir-a",      C_MOV, K_STACK, M_OBJETIVO, N_("ir a @"), { "raton" } },
    [O_DESLIZAR] = { "deslizar",  C_MOV, K_STACK, M_NADA,     N_("deslizar # s a x: # y: #"), { "1", "0", "0" } },
    [O_APUNTAR]  = { "apuntar",   C_MOV, K_STACK, M_NADA,     N_("apuntar en dirección #"), { "90" } },
    [O_APUNTAR_A]= { "apuntar-a", C_MOV, K_STACK, M_OBJETIVO, N_("apuntar hacia @"), { "raton" } },
    [O_SUMAR_X]  = { "sumar-x",   C_MOV, K_STACK, M_NADA,     N_("sumar # a x"), { "10" } },
    [O_FIJAR_X]  = { "fijar-x",   C_MOV, K_STACK, M_NADA,     N_("fijar x a #"), { "0" } },
    [O_SUMAR_Y]  = { "sumar-y",   C_MOV, K_STACK, M_NADA,     N_("sumar # a y"), { "10" } },
    [O_FIJAR_Y]  = { "fijar-y",   C_MOV, K_STACK, M_NADA,     N_("fijar y a #"), { "0" } },
    [O_REBOTAR]  = { "rebotar",   C_MOV, K_STACK, M_NADA,     N_("rebotar si toca un borde"), { 0 } },
    [O_GIRO]     = { "giro",      C_MOV, K_STACK, M_ROT,      N_("estilo de giro: @"), { "todo" } },
    [O_POS_X]    = { "pos-x",     C_MOV, K_REP,   M_NADA,     N_("posición x"), { 0 } },
    [O_POS_Y]    = { "pos-y",     C_MOV, K_REP,   M_NADA,     N_("posición y"), { 0 } },
    [O_DIRECCION]= { "direccion", C_MOV, K_REP,   M_NADA,     N_("dirección"), { 0 } },

    [O_DECIR_T]  = { "decir-t",   C_APAR, K_STACK, M_NADA,   N_("decir $ durante # segundos"), { "¡Hola!", "2" } },
    [O_DECIR]    = { "decir",     C_APAR, K_STACK, M_NADA,   N_("decir $"), { "¡Hola!" } },
    [O_PENSAR_T] = { "pensar-t",  C_APAR, K_STACK, M_NADA,   N_("pensar $ durante # segundos"), { "Hmm...", "2" } },
    [O_PENSAR]   = { "pensar",    C_APAR, K_STACK, M_NADA,   N_("pensar $"), { "Hmm..." } },
    [O_TRAJE]    = { "traje",     C_APAR, K_STACK, M_TRAJE,  N_("cambiar el traje a @"), { "" } },
    [O_SIG_TRAJE]= { "sig-traje", C_APAR, K_STACK, M_NADA,   N_("siguiente traje"), { 0 } },
    [O_FONDO]    = { "fondo",     C_APAR, K_STACK, M_FONDO,  N_("cambiar el fondo a @"), { "" } },
    [O_SUMAR_TAM]= { "sumar-tam", C_APAR, K_STACK, M_NADA,   N_("cambiar el tamaño en #"), { "10" } },
    [O_FIJAR_TAM]= { "fijar-tam", C_APAR, K_STACK, M_NADA,   N_("fijar el tamaño a # %"), { "100" } },
    [O_MOSTRAR]  = { "mostrar",   C_APAR, K_STACK, M_NADA,   N_("mostrar"), { 0 } },
    [O_OCULTAR]  = { "ocultar",   C_APAR, K_STACK, M_NADA,   N_("ocultar"), { 0 } },
    [O_N_TRAJE]  = { "n-traje",   C_APAR, K_REP,   M_NADA,   N_("número de traje"), { 0 } },
    [O_TAMANO]   = { "tamano",    C_APAR, K_REP,   M_NADA,   N_("tamaño"), { 0 } },

    [O_NOTA]     = { "nota",      C_SONIDO, K_STACK, M_NADA,   N_("tocar la nota # durante # pulsos"), { "60", "0.5" } },
    [O_SONIDO]   = { "sonido",    C_SONIDO, K_STACK, M_SONIDO, N_("tocar el sonido @"), { "pitido" } },
    [O_CALLAR]   = { "callar",    C_SONIDO, K_STACK, M_NADA,   N_("parar los sonidos"), { 0 } },

    [O_LAPIZ_BAJAR]  = { "lapiz-bajar",  C_LAPIZ, K_STACK, M_NADA,  N_("bajar el lápiz"), { 0 } },
    [O_LAPIZ_SUBIR]  = { "lapiz-subir",  C_LAPIZ, K_STACK, M_NADA,  N_("subir el lápiz"), { 0 } },
    [O_LAPIZ_BORRAR] = { "lapiz-borrar", C_LAPIZ, K_STACK, M_NADA,  N_("borrar todo"), { 0 } },
    [O_LAPIZ_COLOR]  = { "lapiz-color",  C_LAPIZ, K_STACK, M_COLOR, N_("poner el color del lápiz en @"), { "rojo" } },
    [O_LAPIZ_GROSOR] = { "lapiz-grosor", C_LAPIZ, K_STACK, M_NADA,  N_("poner el grosor del lápiz en #"), { "2" } },
    [O_LAPIZ_SELLAR] = { "lapiz-sellar", C_LAPIZ, K_STACK, M_NADA,  N_("sellar"), { 0 } },
    [O_LAPIZ_SIG]    = { "lapiz-sig",    C_LAPIZ, K_STACK, M_NADA,  N_("cambiar al siguiente color del lápiz"), { 0 } },

    [O_ESPERAR]      = { "esperar",       C_CONTROL, K_STACK, M_NADA,    N_("esperar # segundos"), { "1" } },
    [O_REPETIR]      = { "repetir",       C_CONTROL, K_C,     M_NADA,    N_("repetir #"), { "10" } },
    [O_SIEMPRE]      = { "siempre",       C_CONTROL, K_CAP,   M_NADA,    N_("por siempre"), { 0 } },
    [O_SI]           = { "si",            C_CONTROL, K_C,     M_NADA,    N_("si ^ entonces"), { 0 } },
    [O_SI_NO]        = { "si-no",         C_CONTROL, K_C,     M_NADA,    N_("si ^ entonces"), { 0 } },
    [O_ESPERAR_HASTA]= { "esperar-hasta", C_CONTROL, K_STACK, M_NADA,    N_("esperar hasta que ^"), { 0 } },
    [O_REPETIR_HASTA]= { "repetir-hasta", C_CONTROL, K_C,     M_NADA,    N_("repetir hasta que ^"), { 0 } },
    [O_DETENER]      = { "detener",       C_CONTROL, K_CAP,   M_DETENER, N_("detener @"), { "todo" } },

    [O_TOCANDO]       = { "tocando",       C_SENS, K_BOOL, M_TOCANDO,   N_("¿tocando @?"), { "borde" } },
    [O_TOCANDO_COLOR] = { "tocando-color", C_SENS, K_BOOL, M_COLOR,     N_("¿tocando el color @?"), { "rojo" } },
    [O_TECLA_PULSADA] = { "tecla-pulsada", C_SENS, K_BOOL, M_TECLA,     N_("¿tecla @ pulsada?"), { "espacio" } },
    [O_RATON_PULSADO] = { "raton-pulsado", C_SENS, K_BOOL, M_NADA,      N_("¿ratón pulsado?"), { 0 } },
    [O_RATON_X]       = { "raton-x",       C_SENS, K_REP,  M_NADA,      N_("ratón x"), { 0 } },
    [O_RATON_Y]       = { "raton-y",       C_SENS, K_REP,  M_NADA,      N_("ratón y"), { 0 } },
    [O_PREGUNTAR]     = { "preguntar",     C_SENS, K_STACK, M_NADA,     N_("preguntar $ y esperar"), { "¿Cómo te llamas?" } },
    [O_RESPUESTA]     = { "respuesta",     C_SENS, K_REP,  M_NADA,      N_("respuesta"), { 0 } },
    [O_CRONO]         = { "crono",         C_SENS, K_REP,  M_NADA,      N_("cronómetro"), { 0 } },
    [O_CRONO_REINICIAR] = { "crono-reiniciar", C_SENS, K_STACK, M_NADA, N_("reiniciar el cronómetro"), { 0 } },
    [O_DISTANCIA]     = { "distancia",     C_SENS, K_REP,  M_DISTANCIA, N_("distancia a @"), { "raton" } },

    [O_SUMA]     = { "suma",     C_OPER, K_REP,  M_NADA, N_("# + #"), { "", "" } },
    [O_RESTA]    = { "resta",    C_OPER, K_REP,  M_NADA, N_("# - #"), { "", "" } },
    [O_MULT]     = { "mult",     C_OPER, K_REP,  M_NADA, N_("# × #"), { "", "" } },
    [O_DIV]      = { "div",      C_OPER, K_REP,  M_NADA, N_("# ÷ #"), { "", "" } },
    [O_AZAR]     = { "azar",     C_OPER, K_REP,  M_NADA, N_("número al azar entre # y #"), { "1", "10" } },
    [O_MAYOR]    = { "mayor",    C_OPER, K_BOOL, M_NADA, N_("# > #"), { "", "50" } },
    [O_MENOR]    = { "menor",    C_OPER, K_BOOL, M_NADA, N_("# < #"), { "", "50" } },
    [O_IGUAL]    = { "igual",    C_OPER, K_BOOL, M_NADA, N_("# = #"), { "", "50" } },
    [O_Y]        = { "y",        C_OPER, K_BOOL, M_NADA, N_("^ y ^"), { 0 } },
    [O_O]        = { "o",        C_OPER, K_BOOL, M_NADA, N_("^ o ^"), { 0 } },
    [O_NO]       = { "no",       C_OPER, K_BOOL, M_NADA, N_("no ^"), { 0 } },
    [O_UNIR]     = { "unir",     C_OPER, K_REP,  M_NADA, N_("unir $ y $"), { "hola ", "mundo" } },
    [O_LETRA]    = { "letra",    C_OPER, K_REP,  M_NADA, N_("letra # de $"), { "1", "mundo" } },
    [O_LONGITUD] = { "longitud", C_OPER, K_REP,  M_NADA, N_("longitud de $"), { "mundo" } },
    [O_RESTO]    = { "resto",    C_OPER, K_REP,  M_NADA, N_("# resto # "), { "", "" } },
    [O_REDONDEAR]= { "redondear",C_OPER, K_REP,  M_NADA, N_("redondear #"), { "" } },
    [O_ABS]      = { "abs",      C_OPER, K_REP,  M_NADA, N_("valor absoluto de #"), { "" } },
    [O_RAIZ]     = { "raiz",     C_OPER, K_REP,  M_NADA, N_("raíz cuadrada de #"), { "" } },

    [O_FIJAR_VAR]   = { "fijar-var",   C_VARS, K_STACK, M_VAR, N_("fijar @ a #"), { "", "0" } },
    [O_CAMBIAR_VAR] = { "cambiar-var", C_VARS, K_STACK, M_VAR, N_("cambiar @ por #"), { "", "1" } },
    [O_MOSTRAR_VAR] = { "mostrar-var", C_VARS, K_STACK, M_VAR, N_("mostrar la variable @"), { "" } },
    [O_OCULTAR_VAR] = { "ocultar-var", C_VARS, K_STACK, M_VAR, N_("ocultar la variable @"), { "" } },
    [O_VAR]         = { "var",         C_VARS, K_REP,   M_VAR, N_("@"), { "" } },
    [O_LISTA_AGREGAR] = { "lista-agregar", C_VARS, K_STACK, M_LISTA, N_("agregar $ a la lista @"), { "cosa", "" } },
    [O_LISTA_VACIAR]  = { "lista-vaciar",  C_VARS, K_STACK, M_LISTA, N_("vaciar la lista @"), { "" } },
    [O_LISTA_ELEM]    = { "lista-elem",    C_VARS, K_REP,   M_LISTA, N_("elemento # de la lista @"), { "1", "" } },
    [O_LISTA_LONG]    = { "lista-long",    C_VARS, K_REP,   M_LISTA, N_("longitud de la lista @"), { "" } },
};

/* Los textos de los menús y de la biblioteca, para el catálogo de traducción (no se usan aquí). */
const char *const jb_textos_extra[] = {
    N_("espacio"), N_("flecha arriba"), N_("flecha abajo"), N_("flecha izquierda"), N_("flecha derecha"), N_("Intro"), N_("cualquiera"),
    N_("puntero del ratón"), N_("posición al azar"), N_("borde"), N_("todo"), N_("izquierda-derecha"), N_("ninguno"),
    N_("pitido"), N_("salto"), N_("moneda"), N_("bocina"), N_("aplauso"), N_("este guion"), N_("mensaje1"), N_("mensaje2"), N_("mensaje3"), N_("mensaje4"), N_("mensaje5"),
    N_("rojo"), N_("naranja"), N_("amarillo"), N_("verde"), N_("turquesa"), N_("azul"), N_("morado"), N_("rosa"), N_("negro"), N_("blanco"), N_("gris"), N_("marron"),
    N_("Sagui"), N_("Pelota"), N_("Cohete"), N_("Estrella"), N_("Manzana"), N_("Cesta"), N_("Pez"), N_("Flor"),
    N_("Blanco"), N_("Cielo"), N_("Pasto"), N_("Noche"), N_("Espacio"), N_("Cancha"), N_("Mar"),
    N_("¡Hola!"), N_("Hmm..."), N_("¿Cómo te llamas?"), N_("cosa"), N_("hola "), N_("mundo"),
    N_("No se puede dividir entre cero: el resultado es 0"), N_("No existe la raíz de un número negativo: el resultado es 0"),
    N_("Demasiados bloques metidos unos dentro de otros"), N_("La variable «%s» no existe todavía"), N_("La lista «%s» no existe todavía"),
    N_("No encuentro el traje «%s»"), N_("No encuentro el fondo «%s»"),
    NULL,
};
#endif
