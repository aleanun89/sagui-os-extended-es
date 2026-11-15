/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Catálogo de funciones de Macaco (D311, «Asistente de funciones»).
 *
 * Es la ÚNICA fuente: el evaluador de fórmulas despacha por el `id` de esta tabla, el asistente
 * «Insertar función» saca de aquí las categorías, las listas y los argumentos, y el autocompletado y la
 * ayuda de argumentos al escribir a mano también. La documentación (docs/macaco.md) se genera con
 * tools/macaco_funciones_doc.py, que compila este fichero con el compilador del PC (por eso solo usa
 * <stddef.h>: nada de la libsagui).
 *
 * Los argumentos van en una cadena «nombre|descripción;nombre|descripción»:
 *   - «[nombre]»  es opcional;
 *   - «nombre+»   (o «[nombre]+») se repite: el asistente añade cuadros nuevos a medida que se rellenan.
 * Ni los nombres ni las descripciones llevan «;» ni «|».
 * `ejemplo` y `res` los usa la autoprueba (`macaco --fx`) sobre estos datos de la hoja:
 *   A1:A5 = 10, 20, 30, 40, 50   y   B1:B5 = a, b, a, c, a.
 * `res` NULL = no es determinista (HOY, ALEATORIO...): solo se comprueba que no dé error. */
#ifndef MACACO_FUNCIONES_H
#define MACACO_FUNCIONES_H

#include <stddef.h>

enum {   /* categorías */
    FXC_MAT, FXC_EST, FXC_LOG, FXC_TXT, FXC_FEC, FXC_BUS, FXC_N
};

enum {   /* identificadores (el evaluador despacha por ellos) */
    FX_SUMA, FX_PRODUCTO, FX_ABS, FX_ENTERO, FX_RAIZ, FX_POTENCIA, FX_RESIDUO, FX_PI, FX_ALEATORIO, FX_REDONDEAR,
    FX_PROMEDIO, FX_MAX, FX_MIN, FX_CONTAR, FX_CONTARA, FX_MEDIANA, FX_CONTARSI, FX_SUMARSI,
    FX_SI, FX_Y, FX_O, FX_NO, FX_SIERROR,
    FX_CONCATENAR, FX_LARGO, FX_IZQUIERDA, FX_DERECHA, FX_EXTRAE, FX_MAYUSC, FX_MINUSC, FX_ESPACIOS, FX_TEXTO,
    FX_HOY, FX_AHORA, FX_ANO, FX_MES, FX_DIA, FX_FECHA,
    FX_BUSCARV, FX_INDICE, FX_COINCIDIR,
    FX_NFUNC
};

typedef struct {
    const char *nome;      /* en español */
    const char *alias;     /* en inglés */
    int         cat;       /* FXC_* */
    int         id;        /* FX_* */
    const char *desc;      /* descripción corta (cabe en una línea del asistente) */
    const char *args;      /* ver arriba */
    const char *ejemplo;
    const char *res;
} fx_t;

static const char *const FX_CATS[FXC_N] = {
    "Matemáticas", "Estadísticas", "Lógicas", "Texto", "Fecha y hora", "Búsqueda y referencia"
};

static const fx_t FX_TABELA[] = {
    /* ---- Matemáticas */
    { "SUMA", "SUM", FXC_MAT, FX_SUMA, "Suma todos los números de los argumentos.",
      "número1|Primer número o rango que se suma.;[número2]+|Más números o rangos.",
      "=SUMA(1;2;3)", "6" },
    { "PRODUCTO", "PRODUCT", FXC_MAT, FX_PRODUCTO, "Multiplica todos los números de los argumentos.",
      "número1|Primer número o rango que se multiplica.;[número2]+|Más números o rangos.",
      "=PRODUCTO(2;3;4)", "24" },
    { "ABS", "ABS", FXC_MAT, FX_ABS, "Devuelve el valor absoluto de un número.",
      "número|Número del que se quiere el valor absoluto.",
      "=ABS(-5)", "5" },
    { "ENTERO", "INT", FXC_MAT, FX_ENTERO, "Redondea un número hacia abajo hasta el entero más próximo.",
      "número|Número que se redondea hacia abajo.",
      "=ENTERO(8.9)", "8" },
    { "RAIZ", "SQRT", FXC_MAT, FX_RAIZ, "Devuelve la raíz cuadrada de un número.",
      "número|Número positivo del que se quiere la raíz.",
      "=RAIZ(16)", "4" },
    { "POTENCIA", "POWER", FXC_MAT, FX_POTENCIA, "Eleva un número a una potencia entera (o 0,5).",
      "número|Número base.;potencia|Exponente: entero o 0.5 (raíz).",
      "=POTENCIA(2;3)", "8" },
    { "RESIDUO", "MOD", FXC_MAT, FX_RESIDUO, "Devuelve el resto de una división.",
      "número|Número que se divide.;divisor|Número por el que se divide.",
      "=RESIDUO(10;3)", "1" },
    { "PI", "PI", FXC_MAT, FX_PI, "Devuelve el valor de pi (3,14, con dos decimales).",
      "",
      "=PI()", "3.14" },
    { "ALEATORIO", "RAND", FXC_MAT, FX_ALEATORIO, "Devuelve un número aleatorio entre 0 y 1 (con dos decimales).",
      "",
      "=ALEATORIO()", NULL },
    { "REDONDEAR", "ROUND", FXC_MAT, FX_REDONDEAR, "Redondea un número a los decimales indicados (0 o 1).",
      "número|Número que se redondea.;[decimales]|Cuántos decimales se dejan (0 por omisión).",
      "=REDONDEAR(3.14;1)", "3.1" },

    /* ---- Estadísticas */
    { "PROMEDIO", "AVERAGE", FXC_EST, FX_PROMEDIO, "Devuelve la media aritmética de los argumentos.",
      "número1|Primer número o rango.;[número2]+|Más números o rangos.",
      "=PROMEDIO(2;4;9)", "5" },
    { "MAX", "MAX", FXC_EST, FX_MAX, "Devuelve el valor máximo de los argumentos.",
      "número1|Primer número o rango.;[número2]+|Más números o rangos.",
      "=MAX(3;9;5)", "9" },
    { "MIN", "MIN", FXC_EST, FX_MIN, "Devuelve el valor mínimo de los argumentos.",
      "número1|Primer número o rango.;[número2]+|Más números o rangos.",
      "=MIN(3;9;5)", "3" },
    { "CONTAR", "COUNT", FXC_EST, FX_CONTAR, "Cuenta las celdas que contienen números.",
      "valor1|Primer valor o rango.;[valor2]+|Más valores o rangos.",
      "=CONTAR(A1:A5)", "5" },
    { "CONTARA", "COUNTA", FXC_EST, FX_CONTARA, "Cuenta las celdas que no están vacías.",
      "valor1|Primer valor o rango.;[valor2]+|Más valores o rangos.",
      "=CONTARA(A1:B5)", "10" },
    { "MEDIANA", "MEDIAN", FXC_EST, FX_MEDIANA, "Devuelve la mediana (el valor central) de los números.",
      "número1|Primer número o rango.;[número2]+|Más números o rangos.",
      "=MEDIANA(A1:A5)", "30" },
    { "CONTAR.SI", "COUNTIF", FXC_EST, FX_CONTARSI, "Cuenta las celdas de un rango que cumplen un criterio.",
      "rango|Celdas que se examinan.;criterio|Número, texto o comparación como \">5\".",
      "=CONTAR.SI(B1:B5;\"a\")", "3" },
    { "SUMAR.SI", "SUMIF", FXC_EST, FX_SUMARSI, "Suma las celdas de un rango que cumplen un criterio.",
      "rango|Celdas que se examinan.;criterio|Número, texto o comparación como \">5\".;[rango_suma]|Celdas que se suman (por omisión, el rango).",
      "=SUMAR.SI(A1:A5;\">20\")", "120" },

    /* ---- Lógicas */
    { "SI", "IF", FXC_LOG, FX_SI, "Devuelve un valor si la condición es cierta y otro si no.",
      "prueba_lógica|Condición que se comprueba.;valor_si_verdadero|Valor si la condición es cierta.;[valor_si_falso]|Valor si la condición es falsa.",
      "=SI(A2>15;\"alto\";\"bajo\")", "alto" },
    { "Y", "AND", FXC_LOG, FX_Y, "Cierto si todos los argumentos son ciertos.",
      "valor_lógico1|Primera condición.;[valor_lógico2]+|Más condiciones.",
      "=Y(1<2;2<3)", "1" },
    { "O", "OR", FXC_LOG, FX_O, "Cierto si algún argumento es cierto.",
      "valor_lógico1|Primera condición.;[valor_lógico2]+|Más condiciones.",
      "=O(1>2;2<3)", "1" },
    { "NO", "NOT", FXC_LOG, FX_NO, "Invierte una condición: cierto pasa a falso y al revés.",
      "valor_lógico|Condición que se invierte.",
      "=NO(1>2)", "1" },
    { "SI.ERROR", "IFERROR", FXC_LOG, FX_SIERROR, "Devuelve un valor alternativo si el primero da error.",
      "valor|Expresión que se comprueba.;valor_si_error|Valor que se devuelve si hay error.",
      "=SI.ERROR(RAIZ(-1);0)", "0" },

    /* ---- Texto */
    { "CONCATENAR", "CONCATENATE", FXC_TXT, FX_CONCATENAR, "Une varios textos en uno (también con el operador &).",
      "texto1|Primer texto.;[texto2]+|Más textos.",
      "=CONCATENAR(\"Sa\";\"gui\";1)", "Sagui1" },
    { "LARGO", "LEN", FXC_TXT, FX_LARGO, "Devuelve el número de caracteres de un texto.",
      "texto|Texto cuya longitud se quiere.",
      "=LARGO(\"Sagui\")", "5" },
    { "IZQUIERDA", "LEFT", FXC_TXT, FX_IZQUIERDA, "Devuelve los primeros caracteres de un texto.",
      "texto|Texto del que se extrae.;[núm_de_caracteres]|Cuántos caracteres (1 por omisión).",
      "=IZQUIERDA(\"Sagui\";2)", "Sa" },
    { "DERECHA", "RIGHT", FXC_TXT, FX_DERECHA, "Devuelve los últimos caracteres de un texto.",
      "texto|Texto del que se extrae.;[núm_de_caracteres]|Cuántos caracteres (1 por omisión).",
      "=DERECHA(\"Sagui\";2)", "ui" },
    { "EXTRAE", "MID", FXC_TXT, FX_EXTRAE, "Devuelve caracteres del medio de un texto.",
      "texto|Texto del que se extrae.;posición_inicial|Posición del primer carácter (1 es el primero).;núm_de_caracteres|Cuántos caracteres.",
      "=EXTRAE(\"Sagui\";2;3)", "agu" },
    { "MAYUSC", "UPPER", FXC_TXT, FX_MAYUSC, "Convierte un texto a mayúsculas.",
      "texto|Texto que se convierte.",
      "=MAYUSC(\"sagui\")", "SAGUI" },
    { "MINUSC", "LOWER", FXC_TXT, FX_MINUSC, "Convierte un texto a minúsculas.",
      "texto|Texto que se convierte.",
      "=MINUSC(\"SAGUI\")", "sagui" },
    { "ESPACIOS", "TRIM", FXC_TXT, FX_ESPACIOS, "Quita los espacios sobrantes de un texto.",
      "texto|Texto que se limpia.",
      "=ESPACIOS(\"  a   b \")", "a b" },
    { "TEXTO", "TEXT", FXC_TXT, FX_TEXTO, "Convierte un número en texto con un formato (0, 0.00, 0%).",
      "valor|Número que se convierte.;formato|Formato: 0, 0.0, 0.00, #,##0 o 0%.",
      "=TEXTO(2.5;\"0.00\")", "2.50" },

    /* ---- Fecha y hora (números de serie como en Excel: 1 = 1900-01-01; 36526 = 2000-01-01) */
    { "HOY", "TODAY", FXC_FEC, FX_HOY, "Devuelve la fecha de hoy como número de serie.",
      "",
      "=HOY()", NULL },
    { "AHORA", "NOW", FXC_FEC, FX_AHORA, "Devuelve la fecha y la hora actuales como número de serie.",
      "",
      "=AHORA()", NULL },
    { "AÑO", "YEAR", FXC_FEC, FX_ANO, "Devuelve el año de una fecha (número de serie).",
      "núm_de_serie|Fecha como número de serie.",
      "=YEAR(36526)", "2000" },
    { "MES", "MONTH", FXC_FEC, FX_MES, "Devuelve el mes de una fecha (1 a 12).",
      "núm_de_serie|Fecha como número de serie.",
      "=MES(36526)", "1" },
    { "DIA", "DAY", FXC_FEC, FX_DIA, "Devuelve el día del mes de una fecha (1 a 31).",
      "núm_de_serie|Fecha como número de serie.",
      "=DIA(36556)", "31" },
    { "FECHA", "DATE", FXC_FEC, FX_FECHA, "Devuelve el número de serie de una fecha.",
      "año|Año (por ejemplo 2000).;mes|Mes (1 a 12).;día|Día del mes.",
      "=FECHA(2000;1;1)", "36526" },

    /* ---- Búsqueda y referencia */
    { "BUSCARV", "VLOOKUP", FXC_BUS, FX_BUSCARV, "Busca un valor en la primera columna y devuelve otra de la fila.",
      "valor_buscado|Valor que se busca en la primera columna.;matriz|Rango donde se busca.;indicador_columnas|Columna de la matriz que se devuelve (1 = la primera).;[ordenado]|0 = exacta, otro valor = aproximada.",
      "=BUSCARV(30;A1:B5;2;0)", "a" },
    { "INDICE", "INDEX", FXC_BUS, FX_INDICE, "Devuelve el valor de una posición de un rango.",
      "matriz|Rango del que se toma el valor.;núm_fila|Fila (o posición si el rango es una línea).;[núm_columna]|Columna del rango.",
      "=INDICE(A1:A5;3)", "30" },
    { "COINCIDIR", "MATCH", FXC_BUS, FX_COINCIDIR, "Devuelve la posición de un valor dentro de un rango.",
      "valor_buscado|Valor que se busca.;matriz_buscada|Rango (una fila o una columna).;[tipo_de_coincidencia]|0 = exacta, 1 = mayor valor menor o igual, -1 = menor valor mayor o igual.",
      "=COINCIDIR(40;A1:A5;0)", "4" },
};
#define FX_N ((int)(sizeof FX_TABELA / sizeof FX_TABELA[0]))

/* ---- lectura de la cadena de argumentos ---- */
typedef struct {
    char nome[32];       /* sin corchetes ni «+» */
    const char *desc;    /* dentro de la cadena original; termina en ';' o en el final */
    int  desc_n;         /* longitud de la descripción */
    int  opcional;       /* 1 = entre corchetes */
    int  repite;         /* 1 = se repite */
} fx_arg_t;

/* Devuelve cuántos argumentos tiene la declaración y rellena hasta `max` en `out`. */
static int fx_args_ler(const fx_t *f, fx_arg_t *out, int max)
{
    const char *p = f->args;
    int n = 0;
    while (*p) {
        fx_arg_t a;
        int k = 0;
        a.opcional = 0; a.repite = 0;
        if (*p == '[') { a.opcional = 1; p++; }
        while (*p && *p != '|' && *p != ';') {
            if (*p == ']') { p++; continue; }
            if (*p == '+') { a.repite = 1; p++; continue; }
            if (k < 31) a.nome[k++] = *p;
            p++;
        }
        a.nome[k] = 0;
        a.desc = "";
        a.desc_n = 0;
        if (*p == '|') {
            p++;
            a.desc = p;
            while (*p && *p != ';') { p++; a.desc_n++; }
        }
        if (*p == ';') p++;
        if (n < max) out[n] = a;
        n++;
    }
    return n;
}

#endif
