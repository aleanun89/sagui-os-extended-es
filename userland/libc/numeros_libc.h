/* numeros_libc.h - conversión exacta entre reales y texto (D210, mejora 094 A).
 *
 * Interna de la libc común: la usan es.c (printf) y numeros.c (strtod). No es
 * una cabecera estándar; los programas no la incluyen.
 */
#ifndef LIBC_NUMEROS_H
#define LIBC_NUMEROS_H

/* La precisión de %e/%f/%g se limita a esto (más cifras serían ceros). */
#define LIBC_REAL_PREC_MAX 1100
/* Lo más largo que escribe __libc_real_texto: 309 cifras enteras, la coma y
 * LIBC_REAL_PREC_MAX decimales (%f de 1e308), más el 0 final. */
#define LIBC_REAL_TAM 1424

/* El cuerpo de un real para printf, sin signo: «conv» es f, F, e, E, g o G;
 * prec < 0 vale 6; alt es la bandera '#'. Las cifras son las exactas del
 * doble, redondeadas al par en los empates (como la glibc). Inf y NaN salen
 * como «inf»/«nan» (o en mayúsculas). Devuelve la longitud. */
int __libc_real_texto(char *buf, double v, char conv, int prec, int alt);

#endif
