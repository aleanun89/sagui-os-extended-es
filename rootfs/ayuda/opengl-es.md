# OpenGL ES 1.1 y EGL

SaguiOS trae **OpenGL ES 1.1** completo (perfil Common) con **EGL 1.4**, sobre SaguiGL: por software y, si el
driver de la tarjeta lo sabe hacer, por la GPU (D425). Los programas escritos para OpenGL ES 1.1 compilan sin
tocarlos con las cabeceras estándar:

    #include <GLES/gl.h>
    #include <GLES/glext.h>
    #include <EGL/egl.h>

## Probar

    es11demo            cuatro escenas (1 mapa de luz, 2 partículas, 3 sombra con esténcil, 4 reflejo);
                        1-4 o espacio cambian de escena, Esc cierra
    es11demo -n 100     100 cuadros de cada escena y sus fps
    es11conf            las pruebas de conformidad (deben salir «0 mal»)

## Compilar con TCC

    tcc mi_programa.c -o mi_programa -lGLESv1_CM -lEGL
    tcc juego_sdl.c -o juego -lSDL2 -lGLESv1_CM -lEGL       (con SDL_GL_CreateContext)

La «ventana nativa» de EGL es una ventana de la libsagui (`jan_t *`, de `jan_abrir` en `gui.h`); también hay
pbuffers (dibujar sin ventana). SaguiGL pide un procesador con **SSE2**.

## Qué hay

- Todo OpenGL ES 1.1: matrices (también de textura), luces (8, con focos y luz de dos caras), niebla, 4 unidades
  de textura con `GL_COMBINE` y DOT3, esténcil de 8 bits, prueba alfa, tijera, máscaras, desplazamiento de
  polígono, 6 planos de recorte, operación lógica, VBO, puntos y líneas anchas, texturas comprimidas con paleta,
  `glReadPixels`, `glCopyTexImage2D` y las variantes de coma fija (`...x`).
- Extensiones: sprites de puntos y tamaño por punto, `glDrawTex*OES`, mezcla separada y con resta, espejo, índices
  de 32 bits, `glMapBufferOES`, texturas de cualquier lado, S3TC (DXT1/3/5), anisótropo, FBO (dibujar en una
  textura) con Z de 16/24 y esténcil, cubos con REFLECTION_MAP y NORMAL_MAP, paleta de matrices (pieles con huesos)
  y `glQueryMatrixxOES`. `glGetString(GL_EXTENSIONS)` da la lista.
- No hay OpenGL ES 2 (sombreadores) ni OpenGL de escritorio.

## Por la GPU

`gpu3d` dice si hay 3D por la GPU y qué sabe hacer el driver. Lo que el driver no sabe se dibuja por software
sin que el programa haga nada (ese cuadro entero va por la CPU). `GL_RENDERER` dice «SaguiGL (GPU)» o
«SaguiGL (software)».
