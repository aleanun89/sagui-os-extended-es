#ifndef BRUMA_AJUSTES_H
#define BRUMA_AJUSTES_H
/* Ajustes que se guardan junto a las partidas (SaguiOS, D444): la distancia de dibujo (F3) y, en SaguiOS, la escala de
 * dibujo (F4). Un solo fichero, ajustes.txt, con una clave por linea ("vista=64", "escala=75"); si no existe, la escala se
 * lee del escala.txt de antes (D451). Funciones puras, sin SDL: tests/ajustes_test.c las prueba en el PC. */
#include <stdio.h>
#include <string.h>

#define AJUSTE_VISTA_MIN 32
#define AJUSTE_VISTA_MAX 104
#define AJUSTE_VISTA_DEFECTO 64

/* Valor de una clave en un texto de lineas "clave=numero"; -1 si no esta o no es un numero. */
static inline int ajuste_leer(const char *texto,const char *clave)
{
    const size_t n=strlen(clave);
    for (const char *p=texto; *p; ) {
        if (strncmp(p,clave,n)==0 && p[n]=='=') {
            const char *v=p+n+1;
            if (*v<'0' || *v>'9') return -1;
            int valor=0;
            for (; *v>='0' && *v<='9'; ++v) { valor=valor*10+(*v-'0'); if (valor>100000) return -1; }
            return valor;
        }
        while (*p && *p!='\n') ++p;
        if (*p=='\n') ++p;
    }
    return -1;
}

/* Escribe el texto del fichero. escala<=0: sin la linea de la escala (programa que no la tiene). */
static inline int ajuste_formatear(char *salida,size_t capacidad,int vista,int escala)
{
    return escala>0 ? snprintf(salida,capacidad,"vista=%d\nescala=%d\n",vista,escala)
                    : snprintf(salida,capacidad,"vista=%d\n",vista);
}

/* D476, limitador de FPS: 30, 60 o 0 (ilimitado); cualquier otro valor (o falta de clave, -1) = 60. Clave "fps". */
static inline int ajuste_fps_valido(int fps) { return fps==30 ? 30 : fps==0 ? 0 : 60; }
/* Siguiente en el ciclo de F6: 60 -> 30 -> ilimitado -> 60. */
static inline int ajuste_fps_siguiente(int fps) { fps=ajuste_fps_valido(fps); return fps==60 ? 30 : fps==30 ? 0 : 60; }
/* Como ajuste_formatear, y añade la línea "fps=N" (fps<0: no la escribe). */
static inline int ajuste_formatear_fps(char *salida,size_t capacidad,int vista,int escala,int fps)
{
    int n=ajuste_formatear(salida,capacidad,vista,escala);
    if (n<0 || fps<0 || (size_t)n>=capacidad) return n;
    int m=snprintf(salida+n,capacidad-(size_t)n,"fps=%d\n",fps);
    return m<0 || (size_t)(n+m)>=capacidad ? -1 : n+m;
}

/* Distancia de dibujo de la primera vez segun lo que anuncia el 3D de SaguiOS (glGetString(GL_RENDERER)):
 *   sin 3D por la GPU ("SaguiGL (software)") o con una GPU antigua sin gpu3d v3 ("SaguiGL (GPU)")  -> 48;
 *   con gpu3d v3 ("SaguiGL (GPU v3)")                                                              -> 64.
 * Un renderizador que no es de SaguiOS (NULL, o cualquier otro) -> el de serie (64). */
static inline int ajuste_vista_para(const char *renderizador)
{
    if (!renderizador || strncmp(renderizador,"SaguiGL",7)!=0) return AJUSTE_VISTA_DEFECTO;
    if (strstr(renderizador,"GPU v3")) return 64;
    return 48;
}

/* La medida corta (milisegundos por cuadro, de media, con la vista de 64 y una GPU con v3): si va sobrada, 104. */
#define AJUSTE_MEDIDA_SOBRADA_MS 8.0
static inline int ajuste_vista_medida(double ms_por_cuadro)
{
    return ms_por_cuadro>0 && ms_por_cuadro<=AJUSTE_MEDIDA_SOBRADA_MS ? AJUSTE_VISTA_MAX : 64;
}

#endif
