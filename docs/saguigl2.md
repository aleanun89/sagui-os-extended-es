# SaguiGL 2.0: OpenGL ES 2.0 (D475)

SaguiGL 2.0 es **OpenGL ES 2.0** en la `libsgl`, con un compilador propio de **GLSL ES 1.00** y una
**representación intermedia (RI)** común que ejecuta la CPU y que traducirán los backends de cada GPU.
Plan aprobado en la Discusión #51 (camino A, propio, no Mesa). Esta es la **fase 1, la base por la CPU**,
que funciona en todas las máquinas (las mismas que la SaguiGL de siempre: SSE2).

| Fase | Qué | Estado |
| --- | --- | --- |
| 1 | API ES 2.0, compilador GLSL ES 1.00, RI, ejecución y rasterizado por la CPU | **hecha (D475)** |
| 2 | VirGL (QEMU y Proxmox): RI -> TGSI, gpu3d v4 | **hecha (D480)** |
| 3 | X1400 (R5xx) y HD 4500 (R7xx) | |
| 4 | UHD 620 (Gen9), HD 7600M (Evergreen), HD 4000 (Gen7) y Xe | |
| 5 | WebGL 1 del Cipó sobre SaguiGL 2.0 | |

## Cómo se usa

Lo mismo que en cualquier sistema con OpenGL ES 2.0: `<GLES2/gl2.h>`, `<GLES2/gl2ext.h>` y `<EGL/egl.h>`,
y un contexto con `EGL_CONTEXT_CLIENT_VERSION` = 2 (las configuraciones anuncian `EGL_OPENGL_ES2_BIT`).
El ejemplo completo es `userland/programas/es20demo.c` (en `/apps/bin/es20demo`).

```c
const EGLint cfg_at[] = { EGL_DEPTH_SIZE, 16, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
const EGLint ctx_at[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
eglInitialize(d, 0, 0);
eglChooseConfig(d, cfg_at, &cfg, 1, &n);
EGLSurface s = eglCreateWindowSurface(d, cfg, (EGLNativeWindowType)janela, 0);   /* una jan_t * */
EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, ctx_at);
eglMakeCurrent(d, s, s, c);
/* ... glCreateShader, glShaderSource, glCompileShader, glLinkProgram, glDrawArrays ... */
eglSwapBuffers(d, s);
```

En el `Makefile`, un programa de ES 2.0 enlaza `$(SGL2_OBJ)` y se compila con `$(SGL_FLAGS) -DSGL_ES2`
(ver `es20demo` y `es20conf`). Los programas de ES 1.1 (`SGL_OBJ`, Brumacanto, TCC) siguen con los objetos de
siempre, sin cambiar un bit.

Extensiones: `GL_OES_standard_derivatives` (`dFdx`, `dFdy`, `fwidth`), `GL_OES_rgb8_rgba8`,
`GL_OES_element_index_uint` y `GL_OES_texture_npot` (texturas NPOT con mipmaps y repetición, sin las
restricciones de ES 2.0), y desde la D486 `GL_OES_depth_texture`, `GL_OES_mapbuffer`, `GL_OES_depth24` y
`GL_OES_packed_depth_stencil` (sección «Extensiones de profundidad y mapbuffer (D486)»). Límites: 16 atributos, 256 vectores de uniformes por etapa, 8 varyings, 8 unidades
de textura por etapa (16 en total), texturas y renderbuffers de 2048.

## Cómo está hecho

```
programa ── gl*() ──> sgl2_despacho.c ──(contexto ES 2.0)──> sgl2.c (estado, objetos, programas)
                              └──────(contexto ES 1.1)──> sgl.c (es1_gl*, la SaguiGL de siempre)

glCompileShader/glLinkProgram ──> sgl_glsl.c (frontal) + sgl_glsl_ri.h (generación) ──> RI (sgl_ri.h)
                                                    sgl_ri.c: saltos, validación, optimizaciones
glDrawArrays/Elements ──> sgl2_desenho.c: atributos -> RI de vértices (sgl_ri_cpu.c, de 4 en 4) -> recorte
                          -> rasterizado (2x2) -> RI de fragmentos (sgl_ri_cpu.c) -> esténcil/Z/mezcla
```

- **Despacho.** ES 1.1 y ES 2.0 comparten 57 nombres. Con `-DSGL_ES2`, `sgl_es1_nomes.h` renombra las versiones
  de ES 1.1 a `es1_gl*` y `sgl2_despacho.c` pone los `gl*` de verdad (una rama según el contexto actual). Los
  tres ficheros los genera `tools/sgl/gerar_despacho.py` (la prueba comprueba que están al día).
- **Contexto.** Un contexto de ES 2.0 lleva debajo un `sgl_ctx_t` que solo da el destino (ventana, búfer
  propio, pbuffer) y `sgl_trocar()`; su GPU queda apagada. El Z (float) y el esténcil del framebuffer por
  defecto son del contexto de ES 2.0. Todos los framebuffers van de abajo arriba, como en GL.
- **Compilador** (`sgl_glsl.c`). El frontal viene del de Cipó (robson-apk, GPL-3.0-only; D141), copiado y
  endurecido: errores en español con su línea (`ERROR: 0:LÍNEA: mensaje`), sin mezclar int y float, sin
  escribir en uniformes, atributos, constantes ni entradas, condiciones bool, swizzles de un solo juego,
  índices constantes dentro del array, sin recursión. La generación (`sgl_glsl_ri.h`) es nueva. Cipó sigue con
  su copia hasta la fase 5.

## La representación intermedia (para quien escriba un backend)

La definición está en `userland/libsgl/sgl_ri.h`; `tests/sgl/es20_ri.c` compila un sombreador y escribe su RI
(`es20_ri f sombreador.glsl`). Está pensada para traducirse 1:1 a TGSI y no usa nada que una GPU de la época de
ES 2.0 no tenga.

**Registros.** Todos son vec4 de float. Ficheros: `TEMP` (temporales), `ENT` (entradas), `SAI` (salidas, solo
se escriben), `UNI` (uniformes), `IMED` (inmediatos, sus valores en `ri_prog_t.imed`), `END` (el registro de
dirección `a0`, solo lo escribe `ARL`) y `AMOST` (un sampler, solo como segunda fuente de `TEX/TXB/TXL`).
int y bool se guardan como float (0.0 y 1.0).

**Disposición.** Escalar y vector: un registro (componentes x, y, z, w en orden). `matN`: N registros
consecutivos (columnas). Cada elemento de un array y cada campo de una struct empieza en un registro.

**Instrucción** (`ri_ins_t`): `op`, destino (`arq`, `ind`, máscara `masc` de 4 bits, `sat` y `rel`) y hasta
tres fuentes (`arq`, `ind`, `sw` con 2 bits por componente, `mod` = `RI_NEG | RI_ABS` —primero el valor
absoluto— y `rel`). Con `rel` el índice es `ind + a0.x`. Las fuentes se leen todas antes de escribir el destino.

| Operaciones | Qué hacen |
| --- | --- |
| `MOV ADD MUL MAD MIN MAX FLR FRC TRUNC` | por componente (`MAD` = a·b + c) |
| `SLT SGE SEQ SNE` | 1.0 si a < b (>=, ==, !=), si no 0.0 |
| `CMP` | a < 0 ? b : c |
| `DP2 DP3 DP4` | producto escalar, a todas las componentes de la máscara |
| `RCP RSQ EX2 LG2 SIN COS POW` | escalares: leen la primera componente del swizzle y la replican (`RSQ` de \|x\|) |
| `DDX DDY` | derivadas (solo fragmentos; diferencias dentro del cuadrado de 2x2) |
| `ARL` | `a0 = floor(x)` |
| `TEX TXB TXL` | `f[0]` = coordenadas (s, t o la dirección del cubo; `w` = sesgo en TXB, nivel en TXL), `f[1]` = `AMOST[n]`, `tex` = 2D o cubo |
| `IF ELSE ENDIF` | entra si `f[0].x != 0` |
| `BGNLOOP ENDLOOP BRK CONT` | bucle sin condición; se sale con `BRK` (dentro de un `IF`) |
| `KILL` | descarta el fragmento (las vías activas) |
| `FIM` | fin |

No hay saltos libres ni llamadas: las funciones se expanden; un `return` antes del final es un `BRK` de un
bucle de una vuelta que envuelve la función (con una bandera si estaba dentro de otro bucle); `continue` en un
`for` lleva el incremento al principio del bucle. `atan`, `asin`, `acos`, `mod`, `sqrt` (= `RCP(RSQ(x))`),
`smoothstep`, `refract`, la división (`RCP`; la entera con corrección), `texture2DProj`, `matN * vecN`... llegan
ya bajados a estas operaciones. `ri_ligar()` rellena `alvo` (IF -> ELSE/ENDIF, ELSE -> ENDIF, BGNLOOP <->
ENDLOOP, BRK/CONT -> su ENDLOOP).

**Entradas y salidas** (`ri_prog_t.ent[]`, `.sai[]`, una por registro, con `sem` e `idx`): `RI_SEM_ATRIB`
(vértices: un atributo), `RI_SEM_POSICAO` (`gl_Position` / `gl_FragCoord`), `RI_SEM_PONTO_TAM`
(`gl_PointSize`), `RI_SEM_GENERICO` (varyings, `idx` = orden en ese sombreador), `RI_SEM_COR`
(`gl_FragColor`), `RI_SEM_FACE` (`gl_FrontFacing` en `.x`) y `RI_SEM_PONTO_COORD` (`gl_PointCoord`).
**Tablas** con nombre, tipo GL, tamaño de array y registro: `unif` (las structs campo a campo, como las ve GL;
los samplers apuntan a `AMOST`), `attr` y `vary`.

**El enlace** (`sgl2.c`, `ligar`) empareja los varyings por nombre (`s2_prog_t.mapa_vary`: entrada de
fragmentos -> salida de vértices), asigna las posiciones de los atributos (`atr_loc`), une los uniformes de
las dos etapas (los valores, en `uni[0]` y `uni[1]`, `float[4]` por registro) y guarda la unidad de textura de
cada sampler (`amost[etapa][sampler]`).

**Optimizaciones comunes** (`sgl_ri.c`, `ri_otimizar`): quita el código muerto, junta «P escribe t; MOV d, t»
en «P escribe d» y **compacta los temporales** por intervalos de vida (lo que se usa dentro de un bucle vive
todo el bucle salvo los temporales de una escritura que se escriben antes de leerse). Con índices dinámicos
sobre temporales no se compacta.

## La CPU

`sgl_ri_cpu.c` predecodifica la RI (registros absolutos, swizzles separados, uniformes e inmediatos replicados
en las 4 vías) y la ejecuta **en 4 vías a la vez**: un cuadrado de 2x2 píxeles o 4 vértices, con los
registros en SoA y vectores de 4 float (SSE2). El control de flujo divergente va por máscaras de vías.
Seno, coseno, 2^x y log2 son polinomios sobre las 4 vías (sin libm ni x87): **los mismos bits en el anfitrión y
en el SaguiOS** (las pruebas lo comprueban con sumas).

`sgl2_desenho.c`: los vértices que usa el dibujo, sombreados una vez y de 4 en 4; recorte contra los 6 planos;
triángulos en coma fija 28.4 con la regla de arriba-izquierda, recorridos en cuadrados de 2x2; varyings con
corrección de perspectiva; prueba de esténcil y Z **antes** del sombreador cuando no hay `discard`; tijera,
esténcil por cara, las 8 funciones de Z, `glPolygonOffset`, la mezcla de ES 2.0 completa y las máscaras.
Puntos con `gl_PointSize` (1 a 64) y `gl_PointCoord`; líneas como paralelogramos del ancho pedido (1 a 16).
Texturas 2D y cubo en `0xAARRGGBB` (todos los formatos de ES 2.0), los 6 filtros, mipmaps, `REPEAT`,
`CLAMP_TO_EDGE` y `MIRRORED_REPEAT`, nivel de detalle por las derivadas del cuadrado. FBO con color en textura
o renderbuffer, Z de 16 y esténcil de 8 en renderbuffer.

**Rendimiento** (`es20demo -m`, 320x240): en el PC del mantenedor (Ryzen 9 9900X) el triángulo 0,4 ms, el cubo
con luz por fragmento 1,6 ms, el FBO con efecto 5,1 ms y el plasma (8 senos y 3 cosenos por píxel) 14 ms. En
QEMU, ver la D475.

## Lo que necesita cada backend

Se enchufa en `sgl2_desenhar()` (`sgl2_desenho.c`): con el programa ya enlazado, en vez de llamar al intérprete
se traduce su RI (una vez, al enlazar) y se manda el dibujo a `gpu3d`.

- **VirGL (fase 2).** RI -> TGSI en texto: casi 1:1 (ficheros, swizzles, modificadores, `IF/ELSE/BGNLOOP/BRK/
  CONT`, `ARL` y `TEMP[ADDR[0].x+n]`, `TEX` con `SAMP`/`SVIEW`), `DCL IN/OUT` con las semánticas de la RI
  (`GENERIC` con el índice que da el enlace), constantes con `SET_CONSTANT_BUFFER`. En `gpu3d`, una **v4**: crear
  y usar un programa (los dos sombreadores ya traducidos), vértices genéricos (atributos con formato y paso),
  varios samplers con vistas, estado de fragmentos completo (esténcil por cara, mezcla separada y constante,
  máscaras, tijera) y destino FBO. La v3 (D457) ya guarda búferes.
- **R5xx (X1400).** Vértices por el PVS (ya genera sombreadores, D457) con `a0`; fragmentos por el US de R500
  (tiene control de flujo; sin direcciones: los arrays con índice dinámico en fragmentos no van). Reparto de
  registros (32 temporales: la compactación ya los deja así en lo normal).
- **R7xx (HD 4500) y Evergreen (HD 7600M).** ALU VLIW de 5 o 4 ranuras: empaquetar las operaciones
  independientes; cláusulas ALU/TEX y control de flujo por bloques CF; la RI estructurada se traduce sin
  reordenar saltos.
- **Gen9 (UHD 620), Gen7 (HD 4000) y Xe.** La RI se escalariza por componente (SIMD8/SIMD16 por píxeles);
  `IF/ELSE/ENDIF/WHILE/BREAK/CONT` existen en la EU; las derivadas, por las parejas del subspan.

## Fase 2: VirGL (D480)

Con gpu3d v4 (hoy solo `gpu3d_virgl`) `sgl2_gpu.c` traduce cada programa enlazado a TGSI en texto (`sgl_tgsi.c`; `SGL2_TGSI=fichero` en el PC lo vuelca al
enlazar) y manda los dibujos de triángulos a la GPU del anfitrión; puntos, líneas y lo que no cabe siguen por la CPU. La libsgl mantiene la coherencia con versiones por
objeto y lee el destino antes de cualquier uso por la CPU; la primera vez hace una autoprueba contra la CPU. Detalles de la interfaz en `abi.h` (`GPU3D_OP_V4_*`) y en la D480.
`es20demo --cpu` y `es20conf --cpu` fuerzan la CPU; `es20demo -d N` compara la escena N por las dos.
Medidas con QEMU TCG: por VirGL 3/1/3/0 ms por cuadro contra 12/48/139/269 ms por la CPU (triángulo, cubo, FBO, plasma).

## Qué falta (fase 1)

- Compilar la RI a código x86 (el intérprete ya va por 4 vías con SSE2; medir antes de complicarlo).
- `gl_DepthRange`, la comprobación de las reglas del apéndice A de GLSL ES (bucles con límites constantes,
  índices con expresiones de índice constante) y de la precisión por defecto en fragmentos.
- Líneas con la regla del rombo (ahora se dibujan como paralelogramos; las tiras repiten los extremos).
- `-lGLESv2` para TCC y `SDL_GL_*` con contexto de ES 2.0 en la SDL 2.
- Cipó (WebGL 1) sobre SaguiGL 2.0: fase 5.

## Pruebas

`tests/h475_saguigl2.py`: `unidad` (las 152 comprobaciones de `tests/sgl/es20_conformidad.c` en el PC, también
con ASan/UBSan; la RI de unos sombreadores válida y compacta), `es11` (es11conf y es11demo iguales con la
libsgl de ES 2.0), `fuentes`, `rendimiento`, `qemu` (es20conf y es20demo en el SaguiOS con las mismas sumas que
el PC, y ES 1.1 igual) y `capturas` (`docs/capturas/H475-*.png`).

## Extensiones de profundidad y mapbuffer (D486)

Cuatro extensiones de OpenGL ES 2.0, anunciadas en `glGetString(GL_EXTENSIONS)`, probadas por `tests/h486_ext_oes.py`
(programa `tests/sgl/es20_ext486.c`, `es20ext` en el SaguiOS):

- **`GL_OES_depth_texture`**: `glTexImage2D` con `GL_DEPTH_COMPONENT` y `GL_UNSIGNED_SHORT` o `GL_UNSIGNED_INT` (con o sin datos;
  también `glTexSubImage2D`). Se adjunta a `GL_DEPTH_ATTACHMENT` de un FBO (también sin textura de color) y se muestrea en el
  sombreador: `texture2D` da `(d, d, d, 1)` y la comparación (sombras) se hace a mano en GLSL, como en ES 2.0. Dentro, el nivel guarda
  la profundidad como floats (`z`), que es lo mismo que usa el rasterizador. No vale para cubos ni para `glGenerateMipmap`
  (`GL_INVALID_OPERATION`); una textura de profundidad como color, o una de color como profundidad, deja el FBO `INCOMPLETE_ATTACHMENT`.
- **`GL_OES_mapbuffer`**: `glMapBufferOES` (solo `GL_WRITE_ONLY_OES`), `glUnmapBufferOES`, `glGetBufferPointervOES`,
  `GL_BUFFER_MAPPED_OES`, `GL_BUFFER_ACCESS_OES`. Con un contexto de ES 2.0 actual las llamadas van a `sgl2.c`; con ES 1.1 siguen en
  `sgl_es.c`. El puntero es el del almacén del búfer, que tanto la CPU como la GPU leen en cada dibujo: no hay nada que subir al desmapear.
  `glBufferData` desmapea y `glBufferSubData` con el búfer mapeado da `GL_INVALID_OPERATION`.
- **`GL_OES_depth24`**: renderbuffers `GL_DEPTH_COMPONENT24_OES`. EGL ya ofrecía configuraciones con Z de 24 (con y sin esténcil, en
  ventana y en pbuffer); con esto `glmark2-es2 --off-screen` arranca sin `--visual-config`.
- **`GL_OES_packed_depth_stencil`**: `GL_DEPTH24_STENCIL8_OES` en renderbuffers y `GL_DEPTH_STENCIL_OES` +
  `GL_UNSIGNED_INT_24_8_OES` en texturas, adjuntables a `GL_DEPTH_ATTACHMENT` y `GL_STENCIL_ATTACHMENT` o de una vez a
  `GL_DEPTH_STENCIL_ATTACHMENT` (como Mesa). Muestrear la textura da la profundidad.

**Por VirGL.** Z de 24 (`DEPTH_COMPONENT24`), `DEPTH24_STENCIL8` y el mapbuffer los dibuja la GPU (el recurso Z/esténcil de v4 ya
era de 24 bits y con esténcil; el almacén mapeado es el que se manda con cada dibujo). **Lo que cae a la CPU**: cualquier dibujo
que escriba en un FBO cuya profundidad o esténcil sea una **textura** y cualquier dibujo que **muestree** una textura de profundidad
(el Z de la GPU es un recurso propio que no se puede leer ni muestrear desde la libsgl sin una ruta nueva en gpu3d; el color de esos
FBO se trae antes de la GPU como con cualquier otro dibujo por la CPU). Las escenas `shadow` y `refract` de glmark2 hacen así parte
de su trabajo por la CPU incluso con VirGL. La prueba compara cada escena GPU contra CPU píxel a píxel (0 píxeles distintos en las medidas).

## Medidas con glmark2 (D481)

`glmark2-es2` (glmark2 2023.01, el banco de pruebas de OpenGL ES 2.0 de Linaro, GPL-3.0-or-later) corre en SaguiOS sobre
SaguiGL 2.0 (`/apps/bin/glmark2-es2`, datos en `/apps/glmark2/`; cómo está hecho el port, en la D481). Sirve para medir
SaguiGL 2.0 y compararla con Linux + Mesa **con el mismo programa y las mismas escenas en el mismo ordenador**.

```
glmark2-es2                                     # las 33 escenas por defecto, ventana de 800x600 (10 s cada una)
glmark2-es2 -s 320x240 -f /apps/glmark2/benchmarks.txt     # la lista de la tabla de abajo (las 33 por defecto, 2 s cada una)
glmark2-es2 --fullscreen -b shading             # pantalla completa
glmark2-es2 --off-screen ...                    # en un FBO (renderbuffers de color y de Z de 24: D486)
tools/glmark2_pc.sh                             # el mismo glmark2 en el PC con Mesa (build/glmark2-pc/glmark2-es2)
```

**Condiciones.** Ryzen 9 9900X; SaguiOS en QEMU con KVM (una sola CPU; las columnas «CPU» sin GPU, la de «VirGL» con `-device virtio-vga-gl -display sdl,gl=on`, D480);
referencia: Mesa 25.2.8 con llvmpipe (renderizado por la CPU con LLVM, **20 hilos**, WSLg/X11; no hay GPU en ese
entorno), la misma fuente de glmark2 compilada con `tools/glmark2_pc.sh`. Ventana de 320x240, 2 s por escena (las 33
por defecto desde la D486; Mesa solo tiene las 29 de la medida anterior, `-` donde no se midió). FPS; la puntuación es la media de glmark2.
Las medidas en QEMU varían un 10-15% de una pasada a otra (otros procesos del anfitrión).

| Escena | SaguiOS CPU ventana | SaguiOS CPU FBO (`--off-screen`) | SaguiOS VirGL ventana | Mesa llvmpipe ventana | Mesa llvmpipe `--off-screen` |
| --- | ---: | ---: | ---: | ---: | ---: |
| `build:use-vbo=false` | 261 | 263 | 195 | 684 | 942 |
| `build:use-vbo=true` | 262 | 257 | 204 | 702 | 880 |
| `texture:texture-filter=nearest` | 1125 | 1191 | 287 | 909 | 1320 |
| `texture:texture-filter=linear` | 863 | 868 | 290 | 935 | 1367 |
| `texture:texture-filter=mipmap` | 605 | 614 | 293 | 850 | 1348 |
| `shading:shading=gouraud` | 136 | 135 | 148 | 569 | 706 |
| `shading:shading=blinn-phong-inf` | 122 | 122 | 168 | 545 | 662 |
| `shading:shading=phong` | 104 | 104 | 167 | 494 | 577 |
| `shading:shading=cel` | 98 | 95 | 171 | 500 | 600 |
| `bump:bump-render=high-poly` | 52 | 52 | 46 | 341 | 381 |
| `bump:bump-render=normals` | 612 | 652 | 291 | 885 | 1343 |
| `bump:bump-render=height` | 442 | 462 | 291 | 862 | 1351 |
| `effect2d:kernel=0,1,0;1,-4,1;0,1,0;` | 80 | 80 | 295 | 936 | 1370 |
| `effect2d:kernel=1,1,1,1,1;1,1,1,1,1;1,1,1,1,1;` | 50 | 51 | 288 | 946 | 1392 |
| `pulsar:quads=5:texture=false:light=false` | 928 | 1069 | 167 | 939 | 1398 |
| `desktop:windows=4:effect=blur:blur-radius=5:passes=1:separable=true` | 39 | 39 | 65 | 122 | 128 |
| `desktop:windows=4:effect=shadow` | 119 | 126 | 52 | 407 | 491 |
| `buffer:update-fraction=0.5:update-dispersion=0.9:columns=200:update-method=map:interleave=false` | 92 | 99 | 78 | - | - |
| `buffer:update-fraction=0.5:update-dispersion=0.9:columns=200:update-method=subdata:interleave=false` | 92 | 99 | 78 | 416 | 602 |
| `buffer:update-fraction=0.5:update-dispersion=0.9:columns=200:update-method=map:interleave=true` | 94 | 99 | 79 | - | - |
| `ideas:speed=duration` | 261 | 263 | 5 | 449 | 693 |
| `jellyfish` | 64 | 63 | 183 | 443 | 597 |
| `terrain` | 3 | 3 | 6 | 8 | 19 |
| `shadow` | 120 | 117 | 95 | - | - |
| `refract` | 13 | 12 | 13 | - | - |
| `conditionals:vertex-steps=0:fragment-steps=0` | 392 | 396 | 236 | 685 | 924 |
| `conditionals:vertex-steps=0:fragment-steps=5` | 192 | 190 | 247 | 700 | 936 |
| `conditionals:vertex-steps=5:fragment-steps=0` | 342 | 354 | 258 | 727 | 955 |
| `function:fragment-steps=5:fragment-complexity=low` | 212 | 226 | 258 | 656 | 954 |
| `function:fragment-steps=5:fragment-complexity=medium` | 137 | 131 | 258 | 532 | 901 |
| `loop:vertex-steps=5:fragment-steps=5:fragment-loop=false` | 247 | 252 | 230 | 694 | 931 |
| `loop:vertex-steps=5:fragment-steps=5:fragment-uniform=false` | 183 | 191 | 237 | 698 | 937 |
| `loop:vertex-steps=5:fragment-steps=5:fragment-uniform=true` | 187 | 192 | 245 | 696 | 932 |
| **glmark2 Score** | **257** | **268** | **178** | **631** | **883** |

Lectura rápida: por la CPU y con un solo hilo, SaguiGL 2.0 queda entre 2,5 y 3,5 veces por detrás de llvmpipe
con 20 hilos en la puntuación; en las escenas de píxeles pesados con luz por fragmento (`shading`, `effect2d`,
`jellyfish`, `bump:high-poly`) la diferencia es de 5 a 20 veces, y en las de pocos píxeles (`pulsar`, `texture`)
SaguiGL llega a la par o por delante. `terrain` (sombreadores de fragmentos muy largos: ruido simplex y desenfoque
sobre FBO) es la más cara: 3 FPS contra 8 de llvmpipe. Pantalla completa (1024x768) con `build` y `shading`: 112 de
puntuación.

**D486.** Las cuatro escenas que antes salían `Unsupported` (`shadow`, `refract` y `buffer` con `update-method=map`, dos veces) corren
con `GL_OES_depth_texture` y `GL_OES_mapbuffer`, y `--off-screen` arranca sin `--visual-config` con `GL_OES_depth24`: las 33 escenas
por defecto funcionan. Puntuación de las 33 (320x240): **257** por la CPU en ventana, **268** en FBO y **178** por VirGL en ventana.
VirGL va por delante en las escenas de píxeles pesados con luz (`shading` 148-171 contra 88-123, `effect2d` 288-295 contra 44-68,
`jellyfish` 183 contra 67) y casi todas topan con el límite de fotogramas de la presentación (~290 FPS); va por detrás en lo que
cae a la CPU con el destino en la GPU: **`ideas` (líneas y puntos) 5 FPS** contra 262 (cada dibujo trae el color de la GPU), `pulsar`
(167 contra 845), `desktop:effect=shadow` y `buffer` (78 contra 94), `refract` (13: textura de profundidad, por la CPU) y las
`desktop` con FBO. Es la lista de trabajo de una fase siguiente (puntos y líneas por la GPU, el Z de la GPU como textura).

**Lo que falta en SaguiGL 2.0 según glmark2:**

- **EGL sin configuraciones con alfa**: glmark2 pide por defecto `alpha=1` y no habría ninguna configuración de EGL; el port
  fija `alpha=0` por defecto (parche en `main.cpp`). Si algún día hay superficies RGBA, ese parche sobra.
- **FBO frente a ventana**: con Z de 24 (por defecto desde la D486) el FBO ya no es más lento que la ventana (268 contra 257); las medidas
  de la D481 (161 contra 234) eran de un FBO con Z de 16.
- Las 33 escenas por defecto funcionan y las capturas de las de `h481` no salen negras.

Capturas (datos de glmark2, GPL): `docs/capturas/H481-1-medusa.png` (`jellyfish`) y `docs/capturas/H481-2-terreno.png` (`terrain`),
ambas en SaguiOS con SaguiGL 2.0 por la CPU.
