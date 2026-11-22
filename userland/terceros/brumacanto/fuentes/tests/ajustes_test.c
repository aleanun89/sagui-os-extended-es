/* Prueba de src/ajustes.h (SaguiOS, D444): lectura y escritura de ajustes.txt y la eleccion de la distancia de dibujo. */
#include "../src/ajustes.h"
#include <stdio.h>
#include <stdlib.h>

static int fallos;
#define COMPROBAR(c) do { if (!(c)) { printf("FALLA %s:%d: %s\n",__FILE__,__LINE__,#c); ++fallos; } } while (0)

int main(void)
{
    char t[64];
    /* leer */
    COMPROBAR(ajuste_leer("vista=48\nescala=75\n","vista")==48);
    COMPROBAR(ajuste_leer("vista=48\nescala=75\n","escala")==75);
    COMPROBAR(ajuste_leer("vista=48\n","escala")==-1);
    COMPROBAR(ajuste_leer("","vista")==-1);
    COMPROBAR(ajuste_leer("vista=\n","vista")==-1);
    COMPROBAR(ajuste_leer("vista=abc\n","vista")==-1);
    COMPROBAR(ajuste_leer("mivista=3\nvista=104\n","vista")==104);   /* no confunde claves que terminan igual */
    COMPROBAR(ajuste_leer("vista=99999999999999\n","vista")==-1);
    COMPROBAR(ajuste_leer("vista=64","vista")==64);                  /* sin salto de linea al final */
    /* escribir y volver a leer */
    int n=ajuste_formatear(t,sizeof t,80,50);
    COMPROBAR(n>0 && ajuste_leer(t,"vista")==80 && ajuste_leer(t,"escala")==50);
    n=ajuste_formatear(t,sizeof t,32,0);
    COMPROBAR(n>0 && ajuste_leer(t,"vista")==32 && ajuste_leer(t,"escala")==-1);   /* sin escala (Linux) */
    /* la primera vez, segun el equipo */
    COMPROBAR(ajuste_vista_para("SaguiGL (software)")==48);
    COMPROBAR(ajuste_vista_para("SaguiGL (GPU)")==48);       /* GPU antigua sin v3 */
    COMPROBAR(ajuste_vista_para("SaguiGL (GPU v3)")==64);
    COMPROBAR(ajuste_vista_para("Mesa llvmpipe")==AJUSTE_VISTA_DEFECTO);   /* otro sistema: la de serie */
    COMPROBAR(ajuste_vista_para(NULL)==AJUSTE_VISTA_DEFECTO);
    COMPROBAR(AJUSTE_VISTA_DEFECTO==64);
    /* la medida corta */
    COMPROBAR(ajuste_vista_medida(3.5)==104);
    COMPROBAR(ajuste_vista_medida(8.0)==104);
    COMPROBAR(ajuste_vista_medida(8.1)==64);
    COMPROBAR(ajuste_vista_medida(40.0)==64);
    COMPROBAR(ajuste_vista_medida(0.0)==64);   /* sin medida valida, no se sube */
    printf(fallos?"ajustes_test: %d fallos\n":"ajustes_test BIEN\n",fallos);
    return fallos!=0;
}
