# 3D por la GPU (v8.3, D285; Radeon R5xx: D287)

SaguiGL (`userland/libsgl`) dibuja por software desde la v5. Desde la v8.3 puede mandar los
triángulos a la GPU cuando hay un driver 3D para la tarjeta, y vuelve al software si no lo hay o
si la GPU falla. Este documento describe la **capa común** (la comparten todos los drivers 3D), el
driver de **Intel Gen3** y **qué probar en una máquina real**.

> Ningún driver 3D está probado todavía en hardware real: QEMU no emula el 3D de estas tarjetas.
> Todo lo que se dice aquí del hardware está **sin confirmar** hasta que el mantenedor lo pruebe.

## Reparto del trabajo

| Capa | Dónde | Qué hace |
| --- | --- | --- |
| SaguiGL | `userland/libsgl/sgl.c` | matrices, luces, niebla, recorte y descarte de caras con la CPU; triángulos en coordenadas de la ventana (`gpu3d_vert_t`); con un driver **v3** (D457), los dibujos desde VBO los transforma la GPU |
| Capa común | `kernel/gui/gpu3d.c`, `kernel/gui/gpu3d_mem.c`, `kernel/inc/gpu3d.h` | elige el driver, autoprueba, dueño del motor, vigilante, medida, llamadas `INFO_GPU3D*` |
| Driver | `kernel/dispositivos/gpu3d_*.c` | traduce a las órdenes de su GPU |

El kernel no usa coma flotante: los vértices llevan los **bits** de sus `float` y el kernel los
copia (y los valida con enteros).

### Las llamadas (`kernel/inc/abi.h`)

`info_sistema(INFO_GPU3D_OP, &gpu3d_pedido_t, …)` con `op`:

| Operación | Qué hace |
| --- | --- |
| `GPU3D_OP_ABRIR` (ancho, alto, bits de Z) | toma el motor para el proceso y crea el destino; la primera vez enciende el driver y pasa la autoprueba |
| `GPU3D_OP_LIMPAR` | color y/o Z |
| `GPU3D_OP_TEXTURA` | carga (o suelta) una textura `0xAARRGGBB` en un hueco (8 huecos) |
| `GPU3D_OP_ESTADO` | Z (prueba, función, escritura), mezcla (factores), textura (hueco, repetir o sujetar, filtro) |
| `GPU3D_OP_TRIANGULOS` | hasta 3072 vértices (de 3 en 3) con el estado actual |
| `GPU3D_OP_LER` | espera a la GPU y copia el cuadro (`0x00RRGGBB`) a la memoria del programa |
| `GPU3D_OP_APRESENTAR` (D432) | la GPU copia el destino (o una textura) a la pantalla, sin volver a la RAM; solo a pantalla completa (ver «Presentar sin volver a la RAM») |
| `GPU3D_OP_FECHAR` | suelta el motor |

Devuelven 0, `GPU3D_SEM_SUPORTE` (> 0: «eso no lo hago, sigue por software»; el 3D no se apaga
para los demás) o un error (< 0: la GPU falló y la capa ya la apagó). En los dos casos SaguiGL
sigue por software sin que el programa haga nada.

Un vértice son ocho palabras: `x, y` (píxeles del destino, origen arriba a la izquierda, centros en
.5), `z` (0 a 1), `rhw` (1/w, para la corrección de perspectiva), `cor` (`0xAARRGGBB`, ya con la
luz), `espec` (`0x00RRGGBB`, se suma al final: así llega la niebla) y `u, v`.
El color de un píxel es `saturar(textura × cor + espec)` (sin textura, `cor + espec`).

### El motor es de un proceso cada vez

El destino y las texturas viven en la memoria de la GPU, que es poca. El primer programa que abre
el motor se lo queda hasta que lo cierra o termina; mientras, los demás van por software. `gpu3d`
dice quién lo tiene.

### Autoprueba y vigilante

- Al **arrancar** la capa solo mira qué tarjeta tiene la pantalla y si hay driver: no toca la GPU
  (un fallo del 3D no puede retrasar ni estropear el arranque).
- La **primera vez** que un programa abre el motor (o con `gpu3d probar` / `gpu3d encender`) se
  enciende el driver y se pasa la **autoprueba**: ocho pruebas que dibujan en un destino de 128x96
  y leen píxeles concretos: 0 limpiar, 1 rectángulo de color, 2 Z, 3 mezcla, 4 textura (orden de
  canales y orientación), 5 color interpolado, 6 especular y 7 Z de 16 bits.
- Si alguna falla, o si después una orden no termina en `GPU3D_PRAZO_MS` (2 s), el 3D se apaga
  (`[GPU3D] apagado: <motivo>` en el log, con los registros de la GPU) y todo sigue por software
  hasta `gpu3d encender`.
- `mkimagem.py --sgl-motor software` (D158) o `--gpu3d=no` lo quitan desde el arranque.

### Piezas comunes para los drivers (pensadas para durar)

El plan del mantenedor es SaguiOS de 64 bits en la v10.0 y, después, amdgpu sobre una capa de
compatibilidad (LinuxKPI) y Mesa (RADV/Vulkan). La capa de hoy es pequeña, pero sus piezas tienen
la forma de las que hará falta entonces, para que los drivers no haya que reescribirlos enteros:

| Pieza | Hoy (`gpu3d.h`) | A qué corresponde después |
| --- | --- | --- |
| **Memoria de GPU** | `g3d_arena_t`: una zona con dirección de GPU propia (no la de la CPU), repartida en bloques con alineación | gestor de memoria de la GPU (GEM/TTM): varias zonas (VRAM, GTT/GART), desalojo |
| **Búferes** | `g3d_buffer_t`: bloque con su uso (destino, Z, textura, lote), dirección de GPU y, si la CPU lo ve, ventana | objetos de búfer con mapa en la CPU y dominio de uso |
| **Colas de órdenes** | `g3d_fila_t`: palabras preparadas en RAM y entregadas de una vez (lote o anillo) | *command buffers* y su entrega al planificador de la GPU |
| **Vallas** | `g3d_vala_t`: número de secuencia de cada entrega; se espera a una valla con plazo | *fences* / `dma_fence`, sincronización entre colas |
| **Estado + dibujo juntos** | `desenhar(estado, vértices)`: cada entrega lleva su estado completo | *pipelines* y *draws* de Vulkan |

Decisiones para no cerrarse puertas: las direcciones de GPU y los tamaños son `uint64_t`; las
estructuras de la ABI solo tienen enteros de 32 bits de tamaño fijo (valen igual en 64 bits); el
driver no guarda estado entre entregas; la memoria de GPU nunca se confunde con punteros de la CPU.

Lo que **falta** y habrá que añadir cuando toque: varias colas y contextos por proceso (hoy un
dueño), vallas con aviso por interrupción (hoy se sondea), memoria de GPU fuera de la que deja el
VBE (programar el GTT/GART), búferes compartidos con el compositor (hoy el cuadro se copia a la
RAM) y sombreadores programables.

### La capa común única y su interfaz estable

Decisión del coordinador (2026-11-15): **la capa 3D común es esta (`gpu3d`)**. Las ramas de Radeon
(`v8.3-gpu3d-r600`, `v8.3-gpu3d-r500`) la fusionan y entran como drivers suyos; la capa «acel3d» de la
rama R600 no se queda. Desde el commit que lleva este texto la interfaz es **estable**: no se cambia
de forma incompatible sin anotarlo aquí (tabla de cambios, abajo) y en la D285.

Quién la usa:

| Quién | Por dónde |
| --- | --- |
| SaguiGL (`userland/libsgl/sgl.c`) | `INFO_GPU3D_OP` (abrir, limpiar, textura, estado, triángulos, leer, cerrar) |
| `gpu3d`, `infopc`, `sgldemo -g` | `INFO_GPU3D`, `INFO_GPU3D_PROVA`, `INFO_GPU3D_CTL` |
| Drivers (`gpu3d_intel.c`, `gpu3d_sim.c`; después R6xx/R7xx, R5xx y VirGL, D474) | `gpu3d_driver_t` y las piezas `g3d_*` de `kernel/inc/gpu3d.h` |
| Más adelante: renderizador de la SDL 2 por GPU | las mismas llamadas (rectángulos con textura = dos triángulos) |

Lo que es estable (contrato):

- **ABI del programa** (`kernel/inc/abi.h`): `INFO_GPU3D` = 71, `INFO_GPU3D_OP` = 72, `INFO_GPU3D_PROVA`
  = 73, `INFO_GPU3D_CTL` = 74; `gpu3d_vert_t` (8 palabras), `gpu3d_estado_t`, `gpu3d_pedido_t`, las
  `GPU3D_OP_*` 1 a 7, los `G3D_*` de estado y `info_gpu3d_t` / `info_gpu3d_prova_t` (con campos
  `reservado` para crecer sin romper). Se puede **añadir** (operaciones, bits de estado, factores) pero no
  cambiar el significado de lo que hay.
- **Interfaz del driver** (`gpu3d_driver_t`, por orden): `nome`, `iniciar(d, geracao, tela, variante,
  motivo, n)`, `alvo(larg, alt, zbits)`, `limpar(mascara, cor)`, `textura(vaga, larg, alt, argb)`,
  `desenhar(estado, vertices, n, &vala)`, `esperar(vala, ms)`, `ler(dest, passo_px)`, `desligar()`,
  `detalle(fase)`, `memoria(&total_kb, &livre_kb)`. Devuelven `G3D_OK`, `G3D_NAO` o `G3D_FALHA`.
  Funciones nuevas se añaden **al final** de la estructura y pueden ser `NULL`.
- **Qué garantiza la capa al driver:** `iniciar` antes que nada y nunca al arrancar; `alvo` antes de
  dibujar; vértices finitos, dentro del destino (margen de 2 píxeles) y con z de 0 a 1; `n` múltiplo de 3 y
  como mucho `GPU3D_MAX_VERT`; punteros del programa ya comprobados; un solo dueño; tras un `G3D_FALHA`
  llama a `detalle` y a `desligar` y no vuelve a llamar hasta otro `iniciar`.
- **Qué debe cumplir el driver:** comprobar el modelo exacto antes de escribir un registro; toda espera
  con plazo (`GPU3D_PRAZO_MS` como mucho); `desenhar` no depende de estado anterior; `ler` devuelve
  `0x00RRGGBB` con el origen arriba a la izquierda; el color es `saturar(textura × cor + espec)`; pasar las
  ocho pruebas de la autoprueba (son el criterio de «funciona»); `desligar` deja la pantalla como estaba.
- **Piezas `g3d_*`:** `g3d_arena_iniciar/livre`, `g3d_buffer_criar/soltar`, `g3d_fila_iniciar`, `g3d_por`,
  `g3d_float_de_fixo`, `g3d_fixo_de_float`. Usarlas no es obligatorio, pero es lo que se quiere compartir.

Números repartidos que hay que respetar al fusionar: byte **142** de la cabecera de arranque
(`CAB_GPU3D`; modos 0 auto, 1 no, 2 simulado, 3 intel; **4 y 5 quedan para radeon r600 y r500**, el 0x08 es
la variante del driver) e `INFO_*` 71 a 74.

| Fecha | Cambio de la interfaz |
| --- | --- |
| 2026-11-15 | Primera versión estable (D285) |
| 2026-11-15 | Al fusionar main: `INFO_GPU3D*` pasan de 70-73 a **71-74** (el 70 es `INFO_CAMARA`, D277). Solo cambia el número: quien use los nombres de `abi.h` recompila y ya |
| 2026-11-15 | D284 (sin cambio de interfaz): driver `gpu3d_r600`, que arranca en `GPU3D_APAGADO`; modo 4 de `CAB_GPU3D` (`--gpu3d=radeon`); `INFO_GPU3D_CTL` e `INFO_GPU3D_PROVA` solo para el administrador |
| 2026-11-16 | D358 (se amplía, nada incompatible): bit `0x40` de `CAB_GPU3D` (`GPU3D_CAB_GTT`, RAM del sistema en el GTT) que llega al driver en `variante` (los que no lo usan lo ignoran); `INFO_GPU3D_CTL` acepta 3 (lotes + GTT) y 4 (anillo + GTT); `info_gpu3d_t.variante` pasa a bits `GPU3D_VAR_*` de `abi.h` (el 1 sigue siendo «anillo») con lo que de verdad quedó, y `reservado[0]` = KB de RAM en el GTT; `gpu3d_intel_estado()` en `gpu3d.h`. `gpu3d_driver_t` no cambia |
| 2026-11-20 | D432 (se amplía, nada incompatible): **presentar sin volver a la RAM**: operación 14 (`GPU3D_OP_APRESENTAR`, `gpu3d_apresentar_t`), bit `G3D_CAP_APRESENTAR` (0x00400000), una función más al final de `gpu3d_driver_t` (`apresentar`, puede ser `NULL`), `info_gpu3d_t.reservado[3..5]`. Ver «Presentar sin volver a la RAM» |
| 2026-11-21 | D438 (sin cambio de interfaz): driver `gpu3d_intel9` (Intel Gen9 por el RCS con execlists), apagado hasta `gpu3d probar`; `--gpu3d=intel` (modo 3) también lo deja «sin probar» en estas tarjetas |
| 2026-11-22 | D457 (se amplía, nada incompatible): **gpu3d v3** (operaciones 15-18: búferes de vértices residentes, transformación por la GPU), `gpu3d_capas_t.bits3`, dos funciones más al final de `gpu3d_driver_t` (`vbuf`, `desenhar3`, pueden ser `NULL`). Ver «gpu3d v3» |
| 2026-11-20 | D425 (se amplía, nada incompatible): **gpu3d v2** (operaciones 8-13, estado de varias unidades, esténcil, DXT, cubos, destino = textura) y bits de capacidad; cinco funciones nuevas al final de `gpu3d_driver_t` que pueden ser `NULL`. Ver «gpu3d v2» |

### Cómo añadir un driver (como se hizo con las Radeon R7xx, D284)

1. `kernel/dispositivos/gpu3d_<familia>.c` con un `const gpu3d_driver_t gpu3d_<familia>` (las diez
   funciones de `gpu3d.h`: `iniciar`, `alvo`, `limpar`, `textura`, `desenhar`, `esperar`, `ler`,
   `desligar`, `detalle`, `memoria`), declarado en `gpu3d.h`.
2. Una línea en `gpu3d_iniciar()` (`kernel/gui/gpu3d.c`) que lo elija para su tarjeta (la da
   `acel2d_placa()`: la que tiene la pantalla, con su familia y generación).
3. Devolver `G3D_NAO` para lo que no cabe o no se soporta y `G3D_FALHA` solo si la GPU no responde.
4. Los cuatro campos del diagnóstico común (D390): `conf` (el nombre de `/sistema/conf/gpu3d-<conf>.conf`), `faixas` y
   `n_faixas` (los registros que ese fichero puede tocar, por la cola; `NULL, 0` si ninguno) y `vistas()` (lo que la CPU ve
   del color y del Z del destino; si no se ve, se deja vacío). El driver escribe `gpu3d_ajustes` en su cola: los
   `G3D_AJ_INICIO` tras el estado fijo y los `G3D_AJ_DIBUJO` tras el estado de cada dibujo. Las sentinelas, el análisis
   y `gpu3d probar --ver` los hace la capa.
5. Nada más: la autoprueba, el vigilante, `gpu3d`, `infopc` y SaguiGL ya lo usan.

6. Probarlo en QEMU sin la tarjeta: `tests/h285*.py std` debe seguir pasando (la capa no toca nada si
   la tarjeta no es la suya), y su propia prueba de unidad de paquetes en el PC, como `gen3_3d_unidad.c`.

La interfaz es deliberadamente mínima (triángulos transformados, un destino, texturas ARGB) para que
cualquier GPU la cumpla; lo que una familia necesite de más (sombreadores de vértices propios, varias
texturas) queda dentro de su driver.

## gpu3d v2: OpenGL ES 1.1 completo por la GPU (D425)

Desde la v8.5.5.2 SaguiGL es OpenGL ES 1.1 completo (multitextura, `GL_COMBINE`, esténcil, prueba alfa,
tijera, máscaras, mezcla separada, cubos, DXT, dibujar en una textura...). La interfaz v1 no puede llevar
eso, así que hay una **v2 que se añade a la v1 sin cambiarla**:

- un driver que no sabe nada de v2 **sigue igual**: la capa le da `G3D_CAP_*` = 0 y SaguiGL le manda solo
  lo que cabe en v1 (una textura con MODULATE o REPLACE, mezcla de v1, Z de v1); lo demás lo dibuja por
  software;
- un driver que implementa v2 **anuncia lo que sabe** con bits de capacidad, y SaguiGL le manda un dibujo
  solo si **todo** lo que el dibujo necesita está anunciado. Si falta una sola cosa, ese cuadro va por
  software (ver «Qué hace SaguiGL cuando un dibujo no cabe», abajo).

Desde la D426 los drivers de **R5xx** (`gpu3d_r500.c`) y **R6xx/R7xx** (`gpu3d_r600.c`) implementan v2 y anuncian lo
que pasa sus pruebas en la máquina (`kernel/dispositivos/g3d_v2_provas.c`; ver sus secciones). Evergreen lo irá
añadiendo; cada bit se anuncia solo cuando pasa su prueba. El driver simulado de QEMU (`gpu3d_sim.c`) todavía no tiene v2: el camino
v2 de SaguiGL se prueba en el PC con `tests/sgl/es11_gpu_falsa.c` (una GPU falsa por la CPU que anuncia todo y
compara cada escena por v2, por v1 y por software; h424 «unidad»).

### Lo que ve el programa (`kernel/inc/abi.h`)

| Operación | Qué hace |
| --- | --- |
| `GPU3D_OP_CAPACIDADES` (8) | `ptr` = `gpu3d_capas_t`. Se puede pedir sin ser dueño. Con el motor sin probar devuelve lo de v1 |
| `GPU3D_OP_ESTADO2` (9) | `ptr` = `gpu3d_estado2_t`. `GPU3D_SEM_SUPORTE` si pide algo no anunciado; error si tiene valores fuera de rango |
| `GPU3D_OP_TRIANGULOS2` (10) | `ptr` = `gpu3d_vert2_t[]`, `a` = vértices (múltiplo de 3, hasta 3072) con el último `ESTADO2` aceptado |
| `GPU3D_OP_TEXTURA2` (11) | `ptr` = `gpu3d_textura2_t`: un nivel de una cara (ARGB o DXT1/3/5; 2D o cubo). `dados` = 0 suelta el hueco |
| `GPU3D_OP_LIMPAR2` (12) | `ptr` = `gpu3d_limpar2_t`: color/Z/esténcil con valor de Z y de esténcil, máscaras y tijera |
| `GPU3D_OP_DESTINO` (13) | `a` = hueco de textura (su nivel 0, cara 0) o `GPU3D_DESTINO_PRINCIPAL`; `b` = bits de Z (0/16/24); `c` = esténcil (0/8) |

`GPU3D_OP_LER` lee **el destino actual** (el principal o la textura). Las operaciones v1 siguen valiendo
mezcladas con las v2 (`TEXTURA` v1 = `TEXTURA2` ARGB 2D de un nivel).

`info_gpu3d_t.reservado[1]` = bits `G3D_CAP_*` del driver encendido y `reservado[2]` = sus unidades de
textura (para `gpu3d` e `infopc`).

Tamaños fijos (comprobados con `_Static_assert` en `gpu3d.c`): `gpu3d_capas_t` 64 bytes,
`gpu3d_vert2_t` 88, `gpu3d_unidade_t` 28, `gpu3d_estado2_t` 168, `gpu3d_textura2_t` 48,
`gpu3d_limpar2_t` 36. Solo enteros de 8, 16 y 32 bits: valen igual en 64 bits.

### Bits de capacidad (`gpu3d_capas_t.bits`)

| Bit | Qué significa | Qué lo usa en GL |
| --- | --- | --- |
| `G3D_CAP_V2` | entiende `gpu3d_estado2_t` y `gpu3d_vert2_t` (`desenhar2`) | todo lo de abajo |
| `G3D_CAP_COMBINAR` | todas las `G3D_CB_*` salvo DOT3, con escala 1/2/4 | `GL_COMBINE`, `GL_ADD`, `GL_BLEND`, `GL_DECAL` |
| `G3D_CAP_DOT3` | `G3D_CB_DOT3_RGB/RGBA` | mapas de normales |
| `G3D_CAP_ALFA_TESTE` | prueba alfa con las 8 funciones | `glAlphaFunc` |
| `G3D_CAP_ESTENCIL` | esténcil de 8 bits (`estencil_bits` = 8) | `glStencil*`, sombras |
| `G3D_CAP_TIJERA` | recorte a un rectángulo | `glScissor` |
| `G3D_CAP_MASCARAS` | máscara de color por canal | `glColorMask` |
| `G3D_CAP_Z_FUNCS` | las 8 funciones del Z (v1 solo tiene <, <= y siempre) | `glDepthFunc` |
| `G3D_CAP_MEZCLA_EXT` | factores `G3D_F_*` 4 a 10 (color de origen/destino, alfa de destino, saturado) | `glBlendFunc` |
| `G3D_CAP_MEZCLA_SEP` | factores y ecuación distintos para el color y el alfa | OES_blend_func_separate / equation_separate |
| `G3D_CAP_MEZCLA_EQ` | resta y resta invertida | OES_blend_subtract |
| `G3D_CAP_LOGICA` | operación lógica (en vez de la mezcla) | `glLogicOp` |
| `G3D_CAP_NEBLINA` | niebla con el factor de cada vértice y `cor_neblina` | `GL_FOG` con varias texturas o COMBINE |
| `G3D_CAP_TEX_ESPELHO` | `G3D_ENV_ESPELHO` | OES_texture_mirrored_repeat |
| `G3D_CAP_TEX_NPOT` | lados que no son potencia de 2 con cualquier envoltura, **sin** mipmaps | OES_texture_npot |
| `G3D_CAP_TEX_NPOT_MIP` | además, lados que no son potencia de 2 **con** mipmaps (pide también `TEX_NPOT` y `TEX_MIPMAP`) | OES_texture_npot con mipmaps |
| `G3D_CAP_TEX_MIPMAP` | niveles > 0 en `TEXTURA2` y filtros `G3D_FIL_MIP_*` | mipmaps (v1 usa siempre el nivel 0) |
| `G3D_CAP_TEX_CUBO` | texturas de cubo (`G3D_TEX_CUBO`, seis caras cuadradas) | OES_texture_cube_map |
| `G3D_CAP_TEX_ANISO` | `aniso` > 1, hasta `max_aniso` | EXT_texture_filter_anisotropic |
| `G3D_CAP_TEX_PROJ` | coordenadas proyectivas en 2D (s/q, t/q con q ≠ 1) | matriz de textura proyectiva |
| `G3D_CAP_DESTINO_TEX` | `GPU3D_OP_DESTINO` | OES_framebuffer_object |
| `G3D_CAP_LIMPAR2` | `GPU3D_OP_LIMPAR2` | `glClear` con máscaras, tijera, `glClearDepthf`, esténcil |

Además: `unidades` (1 a 4), `max_lado`, `max_aniso`, `formatos` (bits `1 << G3D_FMT_*`: ARGB8888,
DXT1, DXT3, DXT5), `zbits` (1 = 16, 2 = 24) y `estencil_bits`.

La capa **quita** los bits que dependen de funciones que el driver no tiene: sin `desenhar2` no hay v2;
sin `textura2`, solo ARGB y ni cubos ni mipmaps; sin `destino`, no hay `DESTINO_TEX`; sin `limpar2`, ni
`LIMPAR2` ni esténcil. Y `ESTADO2` comprueba otra vez cada cosa pedida contra los bits, así que el driver
**nunca** recibe algo que no anunció.

### El estado v2 (`gpu3d_estado2_t`)

Va completo en cada dibujo (como en v1, el driver no guarda estado entre entregas). Por orden de la
tubería de un píxel:

1. **Unidades de textura** `u[0..unidades-1]`. Cada una: `ativa`, `vaga` (hueco), `tipo` (2D o cubo),
   `filtro` (`G3D_FIL_*`), `env_s/env_t` (repetir, borde, espejo), `aniso`, `coord` (qué juego de
   coordenadas del vértice; hoy siempre el de su número) y la **combinación**:
   - `rgb_func` y `alfa_func` (`G3D_CB_*`): sustituir, modular, sumar, sumar con signo, interpolar, restar,
     DOT3 (solo color);
   - tres argumentos por canal: `rgb_fonte[k]` / `alfa_fonte[k]` (`G3D_FT_TEXTURA`, `CONSTANTE`,
     `PRIMARIA`, `ANTERIOR`) con su operando `rgb_opd[k]` (color, 1-color, alfa, 1-alfa) / `alfa_opd[k]`
     (alfa, 1-alfa);
   - `rgb_escala`, `alfa_escala` (1, 2, 4) y `constante` (0xAARRGGBB).

   Resultado = `saturar(escala × func(arg0, arg1, arg2))`. `ANTERIOR` en la unidad 0 es la primaria (el
   color del vértice). Una unidad no `ativa` deja pasar lo anterior. Los modos clásicos de GL los convierte
   SaguiGL (tabla de la especificación de ES 1.1, §3.7.12): el driver **solo ve combinaciones**, que es
   justo lo que necesita para escribir su programa de píxeles. Por ejemplo, MODULATE con una textura RGBA es
   `rgb_func = MODULAR (TEXTURA·COR, ANTERIOR·COR)`, `alfa_func = MODULAR (TEXTURA·ALFA, ANTERIOR·ALFA)`.
   DOT3_RGBA pone el mismo valor en el alfa y la `alfa_func` de esa unidad se ignora.
2. **Especular**: el RGB de `espec` del vértice se suma tras la última unidad (saturando), como en v1.
3. **Niebla** (`G3D2_NEBLINA`): `color = f × color + (1 - f) × cor_neblina`, con `f` = byte alto de `espec`
   / 255 (interpolado). SaguiGL calcula `f` por vértice (lineal, EXP o EXP2) con la CPU.
4. **Prueba alfa** (`G3D2_ALFA_TESTE`, `func_alfa`, `ref_alfa`).
5. **Tijera** (`G3D2_TIJERA`, `tijera_x/y/l/a`, en píxeles del destino, origen arriba a la izquierda).
6. **Esténcil** (`G3D2_ESTENCIL`): `est_func`, `est_ref`, `est_mascara` (lectura), `est_escrita`, y qué hacer
   si falla el esténcil, si falla el Z o si pasan los dos (`G3D_EST_*`).
7. **Z**: `G3D2_Z_TESTE`, `G3D2_Z_ESCRITA` (la máscara de Z) y `func_z` (`G3D_CMP_*`, el orden de GL).
8. **Mezcla** (`G3D2_MEZCLA`): factores para color y alfa (`mez_orig_rgb`, `mez_dest_rgb`,
   `mez_orig_alfa`, `mez_dest_alfa`; los `G3D_F_*` 0-3 son los de v1) y ecuaciones (`G3D_EQ_*`). O bien
   **operación lógica** (`G3D2_LOGICA`, `op_logica` en el orden de GL), que la sustituye.
9. **Máscara de color** (`mascara_cor`, bits R, G, B, A).

El destino principal **no tiene alfa** (es la ventana, `0x00RRGGBB`): SaguiGL ya cambia los factores de
alfa de destino por 1 o 0 antes de mandar el estado; en una textura de destino el alfa sí se guarda.

Lo que **no** llega al driver porque SaguiGL lo hace con la CPU: matrices (también la de textura y la paleta
de OES_matrix_palette), luces, generación de coordenadas (REFLECTION_MAP/NORMAL_MAP), planos de recorte
(`glClipPlanef`), recorte al frustum, descarte de caras, desplazamiento del polígono (`glPolygonOffset`, ya
sumado al z de cada vértice), puntos y sprites (llegan como dos triángulos con sus coordenadas), líneas
anchas (rectángulos) y `glDrawTexOES` (un rectángulo).

### Vértices v2 (`gpu3d_vert2_t`)

`x, y, z, rhw, cor, espec` como en v1 (bits de float; `espec` lleva además el factor de niebla en el byte
alto) y `tc[4][4]`: (s, t, r, q) por unidad. En 2D se muestrea en (s/q, t/q) (SaguiGL ya divide cuando
q = 1 no basta y el driver no tiene `G3D_CAP_TEX_PROJ`); en un cubo, la dirección (s, t, r) sin normalizar
(cara por el eje mayor, como en GL). La capa comprueba que todo sea finito y que |coordenada| < 2^15.

### Texturas v2 (`gpu3d_textura2_t`)

- Un nivel de una cara por llamada. El **nivel 0 de la cara 0** (re)crea el hueco: `tipo`, `formato`,
  `larg × alt` y cuántos `niveis` tendrá (para reservar la memoria de todos de una vez).
- Formatos: ARGB8888 (`0xAARRGGBB`, como v1) o **DXT1/DXT3/DXT5** tal cual vienen (bloques de 4x4, 8 o 16
  bytes, el orden de S3TC). `bytes` debe ser exacto (la capa lo comprueba).
- Cubo: caras 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z, cuadradas.
- Sin `G3D_CAP_TEX_NPOT`, SaguiGL solo manda lados potencia de 2 (si no, ese dibujo va por software). Una textura
  de lado no potencia de 2 con filtro de mipmaps pide además `G3D_CAP_TEX_NPOT_MIP` (la capa lo comprueba con el
  tamaño del hueco).

### Dibujar en una textura (`GPU3D_OP_DESTINO`)

El hueco debe tener ya su nivel 0 (con `TEXTURA` o `TEXTURA2`, ARGB, al menos 8x8). El driver crea un Z
(y esténcil) propio para ese destino si se pide. Mientras dure, `TRIANGULOS*`, `LIMPAR*` y `LER` van a la
textura, y la capa no deja leer de ese mismo hueco (`ESTADO2` con esa `vaga` da error) ni cargarlo. Al
volver al principal (`GPU3D_DESTINO_PRINCIPAL`) el contenido queda en el hueco para muestrearlo. `FECHAR`
vuelve solo al principal.

### Lo que debe hacer un driver para v2

Añadir al final de su `gpu3d_driver_t` (después de `vistas`; las que falten quedan `NULL`):

```c
void (*capacidades)(gpu3d_capas_t *c);   /* tras iniciar: cambia lo que sepa hacer */
int  (*desenhar2)(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n, g3d_vala_t *vala);
int  (*textura2)(const gpu3d_textura2_t *t);
int  (*limpar2)(const gpu3d_limpar2_t *l);
int  (*destino)(int vaga, int zbits, int estencil);   /* vaga < 0: el principal */
```

Reglas (las de v1 siguen valiendo):

1. **Anunciar solo lo probado.** Mejor pocos bits ciertos que muchos dudosos: un bit sin anunciar solo
   cuesta velocidad (va por software); uno mal anunciado da píxeles malos. La capa llama a `capacidades`
   tras cada `iniciar` y el modelo exacto ya se conoce (una R5xx y una R7xx no saben lo mismo).
2. **La combinación es la que manda.** El programa de píxeles se genera de `u[]` en cada `desenhar2` (o se
   guarda en una caché por la combinación). El orden de la tubería y las fórmulas son las de arriba.
3. **Lo que no se pueda en un caso concreto** (un DOT3 con escala 4 que el hardware no tiene, más
   instrucciones de las que caben...) se contesta con `G3D_NAO` en `desenhar2`: SaguiGL lo repite por
   software y la GPU sigue encendida.
4. **El resultado debe parecerse al de software** (no bit a bit): las pruebas de conformidad del PC
   (`tests/sgl/es11_conformidad.c`) dan los píxeles de referencia de cada modo.
5. Probar sin la tarjeta lo que se pueda: un generador de paquetes con su prueba de unidad en el PC (como
   `gen3_3d_unidad.c`), y que `tests/h285*.py` siga pasando.

### Qué hace SaguiGL cuando un dibujo no cabe

Antes de cada dibujo SaguiGL calcula qué necesita (bits, unidades, formatos). Si cabe en v1 o en lo
anunciado, va a la GPU. Si no:

- el contexto trae el cuadro de la GPU (`LER`) y **sigue por software hasta el final del cuadro** (el Z de
  la GPU no se puede traer, así que en ese primer cuadro mezclado el Z de lo dibujado antes por la GPU no
  cuenta para lo de software);
- los cuadros siguientes empiezan ya por software (desde el `glClear`), sin mezcla;
- cuando un cuadro entero por software ha cabido todo en la GPU, el siguiente vuelve a la GPU.

Así un programa que siempre usa algo no anunciado va entero por software (sin parpadeo) y uno que lo usa
de vez en cuando vuelve solo a la GPU.

### Cambios en la interfaz

| Fecha | Cambio |
| --- | --- |
| 2026-11-20 | Primera versión de gpu3d v2 (D425): operaciones 8 a 13, `gpu3d_capas_t`, `gpu3d_estado2_t`, `gpu3d_unidade_t`, `gpu3d_vert2_t`, `gpu3d_textura2_t`, `gpu3d_limpar2_t`, bits `G3D_CAP_*`, cinco funciones al final de `gpu3d_driver_t`, `info_gpu3d_t.reservado[1..2]`. Ningún driver la usa todavía; nada de v1 cambia |
| 2026-11-20 | (a petición de los drivers, D426) `G3D_CAP_TEX_NPOT` pasa a ser «sin mipmaps» y hay un bit nuevo, `G3D_CAP_TEX_NPOT_MIP` (0x00200000), para lados no potencia de 2 con mipmaps; la capa lo comprueba en `ESTADO2` con el tamaño del hueco. `GPU3D_OP_ABRIR`: `c` = bits de Z \| bits de esténcil << 8 (SaguiGL pide 8 salvo que EGL diga 0); la capa quita el esténcil antes de `alvo()` y, con esténcil, pide Z de 24 (Z24S8). Ningún driver cambia |

## gpu3d v3: la GPU guarda los vértices y los transforma (D457)

Hasta la v8.5.5.2 SaguiGL transformaba cada vértice con la CPU (matrices, niebla, recorte, paso a la pantalla) y mandaba al
kernel vértices ya en píxeles: en un mundo de 55 000 vértices por cuadro (Brumacanto) eso, y copiarlos a la cola de la tarjeta, es
lo que limita los fps en la X1400 y en la HD 4500. Con **v3**, que **se añade a v2 sin cambiarla**, los vértices viven en la
memoria de la GPU y la GPU hace la transformación:

- un driver sin v3 **sigue igual** (`gpu3d_capas_t.bits3` = 0) y SaguiGL le manda lo de siempre;
- un driver con v3 anuncia lo que sabe en `bits3` y **la capa lo pone a prueba contra la CPU** antes de dejar que se use;
- la parte de los **píxeles no cambia**: un dibujo v3 usa el último `ESTADO2` aceptado (texturas, combinación, prueba alfa,
  Z, mezcla...), así que el programa de píxeles de v2 de cada driver sirve tal cual. v3 pide `G3D_CAP_V2`.

### Lo que ve el programa (`kernel/inc/abi.h`)

| Operación | Qué hace |
| --- | --- |
| `GPU3D_OP_VBUF` (15) | `ptr` = `gpu3d_vbuf_t` (32 bytes): crea (`G3D_VB_NOVO`), sube un trozo o suelta (`bytes` = 0) un **búfer residente** de vértices (`G3D_VB_VERTICES`) o de índices de 16 bits (`G3D_VB_INDICES16`) en un hueco (`GPU3D_VBUFS` = 512) |
| `GPU3D_OP_TRANSF` (16) | `ptr` = `gpu3d_transf_t` (580 bytes): la transformación de los dibujos siguientes |
| `GPU3D_OP_DESENHAR3` (17) | `ptr` = `gpu3d_desenho3_t` (80 bytes): dibuja desde los búferes con el último `ESTADO2` y la última `TRANSF` |
| `GPU3D_OP_LUZES` (18) | `ptr` = `gpu3d_luzes_t` (880 bytes): luces y material de ES 1.1 para `G3D3_LUZ`. **La ABI está definida y ningún driver la anuncia todavía** |

Devuelven 0, `GPU3D_SEM_SUPORTE` (ese dibujo, por la CPU), **`GPU3D_EXPULSO`** (2: «ese búfer no está en la GPU: súbelo otra vez
entero y repite»; no es un fallo) o un error (< 0).

**`gpu3d_transf_t`** (todo lo que es coma flotante va como bits de `float`; matrices por columnas, como en GL):

| Campo | Qué es |
| --- | --- |
| `mvp[16]` | recorte = `mvp` × posición. La GPU **recorta** a −w ≤ x, y, z ≤ w y divide por w |
| `mv[16]` | ojo = `mv` × posición: para la niebla (distancia al ojo) y, cuando las haya, las luces |
| `vp_escala[3]`, `vp_desloc[3]` | la vista: `x = ndc.x × escala[0] + desloc[0]` en píxeles del destino con el origen arriba a la izquierda (`escala[1]` < 0), `z` de 0 a 1. La vista tiene que caer **dentro del destino** (la capa lo comprueba) |
| `descarte` | caras que no se dibujan, por el **signo del área en la pantalla** `(x1−x0)(y2−y0) − (x2−x0)(y1−y0)` con la y hacia abajo: `G3D_DESC_POS` (> 0) o `G3D_DESC_NEG` (< 0). SaguiGL lo saca de `glCullFace`/`glFrontFace` |
| `flags` | `G3D3_NEBLINA` (niebla por vértice: `f = saturar(neb_a − d × neb_b)` con d = distancia al ojo; `G3D_NEB_EXP/EXP2` con `G3D_C3_NEB_EXP`), `G3D3_NEB_MEZCLADA` (la niebla ya mezclada en el vértice: `color.rgb × f` y `espec.rgb = neb_cor × (1 − f)`, para un `ESTADO2` sin `G3D2_NEBLINA`, D451; sin el bit, `f` va en el byte alto de `espec`, como en v2), `G3D3_TEX_MAT(u)` (la unidad u multiplica su coordenada por `tex[u]`), `G3D3_LUZ` |
| `cor`, `tc_const[4][4]`, `normal_const` | el valor de un atributo que el dibujo no trae en un array |

**`gpu3d_desenho3_t`**: `modo` (`G3D_PRIM_TRIANGULOS`, `TIRA`, `ABANICO`), `primeiro`, `n` (hasta `GPU3D_MAX_VERT3` = 65535),
`indices` (un hueco `G3D_VB_INDICES16` o `GPU3D_SEM_INDICES`) y los **atributos** (`gpu3d_atrib_t`: hueco, formato, paso y
desplazamiento, múltiplos de 4): `pos` (`G3D_AT_FLOAT2/3/4`), `cor_a` (`G3D_AT_UBYTE4N`, cuatro bytes R, G, B, A, o
`G3D_AT_FLOAT4`), `normal_a` y `tc[4]` (`FLOAT2/3/4`). Los atributos van **con el dibujo y no con la transformación**: un mundo
de cientos de búferes con el mismo formato es una sola `TRANSF` y **un `DESENHAR3` (80 bytes) por búfer**.

### Bits de capacidad (`gpu3d_capas_t.bits3`)

| Bit | Qué significa |
| --- | --- |
| `G3D_C3_V3` | búferes residentes, transformación, recorte, vista, descarte, color (bytes o constante) y coordenadas |
| `G3D_C3_NEBLINA` | niebla lineal por vértice, en las dos salidas |
| `G3D_C3_NEB_EXP` | niebla exponencial |
| `G3D_C3_TEX_MAT` | matriz de textura |
| `G3D_C3_INDICES` | dibujos con índices de 16 bits |
| `G3D_C3_TIRAS` | tiras y abanicos |
| `G3D_C3_COR_FLOAT` | color en `G3D_AT_FLOAT4` |
| `G3D_C3_LUZ`, `G3D_C3_LUZ_ESPEC`, `G3D_C3_LUZ_FOCO` | luces por vértice (`max_luzes`): **nadie las anuncia todavía** |

`gpu3d_capas_t` sigue midiendo 64 bytes (`bits3`, `vbufs` y `max_luzes` salen de los reservados). `reservado[0..4]` de la respuesta
lleva, para `gpu3d` e `infopc`: el resultado de la autoprueba v3 (0 sin hacer, 1 bien, 2 | escena << 8) | lo que anunció el driver
<< 16, los búferes en la GPU, sus KB, las expulsiones y los dibujos desde búferes.

### Lo que hace la capa (`kernel/gui/gpu3d.c`)

- **Contabilidad**: de cada hueco, su capacidad, cuánto se ha subido (un prefijo sin huecos: no se dibuja desde memoria sin
  escribir), el mayor índice (búferes de índices) y cuándo se usó por última vez.
- **Expulsión**: si el driver no tiene sitio para un búfer nuevo (`G3D_NAO`), la capa suelta el que lleva más tiempo sin
  usarse y repite, hasta que quepa o no quede ninguno. El programa lo nota porque el siguiente dibujo con el búfer expulsado
  devuelve `GPU3D_EXPULSO`; SaguiGL lo sube otra vez y repite. El registro avisa de la primera expulsión.
- **Validación antes del driver**: cada atributo tiene que caber en lo subido a su búfer para todos los vértices del dibujo
  (con índices, para el mayor índice del búfer); matrices finitas; la vista dentro del destino; nada que el driver no anunciara.
- Las subidas grandes se copian en trozos de 64 KB con `gpu3d_respirar` entre ellos (el sonido no se corta, D436).
- **Autoprueba v3** (al encender, tras la de v1, y con `gpu3d probar`): tres escenas en el destino de 128x96 (perspectiva con
  recorte por los lados y por el plano cercano, color por vértice y Z; índices, tira y descarte de caras; textura con matriz y
  niebla). Cada una se dibuja por el `desenhar3` del driver y **por la CPU** (`g3d_v3_cpu` + su `desenhar2`) y se comparan las
  dos imágenes (se toleran 28 por canal y un 3 % de píxeles de borde). Si no cuadran, **v3 queda apagado y el 3D sigue encendido
  por v2**; en `dmesg`: `[GPU3D] v3: escena N (nombre): FALLO; M pixeles distintos...` y el peor píxel con lo que dio la GPU y
  lo que dio la CPU. Solo si la GPU deja de responder se apaga todo, como siempre. `gpu3d probar --detalle` lo dice de las tres.

### La transformación por la CPU (`kernel/gui/gpu3d_v3.c`)

Es v3 hecho con enteros: coma flotante IEEE de 32 bits **por software** (multiplicar, sumar, dividir y raíz con redondeo al más
próximo: iguales bit a bit a las del procesador, comprobado con millones de valores en `tests/g3d_v3_unidad.c`), búsqueda de los
atributos con comprobación de límites, recorte, proyección y descarte; entrega triángulos v2. La usan el **driver simulado** de
QEMU (su v3 es esto más su v2) y la autoprueba de la capa. No hace luces ni niebla exponencial (`G3D_V3_CPU_BITS3`).

### Lo que debe hacer un driver

Dos funciones más al final de `gpu3d_driver_t` (las dos o ninguna) y `bits3` en `capacidades()`:

```c
int (*vbuf)(int vaga, uint32_t bytes, uint32_t desloc, const void *dados, uint32_t n);
int (*desenhar3)(const g3d_desenho3_t *d, g3d_vala_t *vala);
```

`vbuf`: con `dados` NULL y `bytes` > 0 crea el hueco (`G3D_NAO` si no cabe: la capa expulsa y repite); con `dados`, copia; con
`bytes` 0, suelta. Para repartir la memoria hay una pieza común, **`g3d_vbmonte_t`** (`gpu3d_mem.c`): el driver pide un bloque
grande a su arena (que solo tiene 48 bloques) y el montón lo reparte por huecos. `desenhar3` recibe el `ESTADO2`, la `TRANSF`,
el dibujo ya validado y el mayor vértice que puede leer. Lo que no pueda en un caso concreto, `G3D_NAO` (SaguiGL lo repite por
la CPU). Las salidas de la etapa de vértices son las de un vértice v2 (posición, color, especular con el factor de niebla en el
alfa, y las coordenadas de cada unidad).

### Qué hace SaguiGL (`userland/libsgl/sgl_es.c`, `es_v3_desenhar`)

Un `glDrawArrays`/`glDrawElements` va por v3 si **todos los arrays activos están en búferes (VBO)** y: triángulos (o tiras y
abanicos con `G3D_C3_TIRAS`), posición `GL_FLOAT`, color de 4 `GL_UNSIGNED_BYTE` (o 4 `GL_FLOAT`) o constante, coordenadas
`GL_FLOAT`, índices `GL_UNSIGNED_SHORT` en un búfer de elementos, sin luces, sin planos de recorte, sin `glPolygonOffset`, sin
sombreado plano, sin generación de coordenadas ni paleta de matrices, niebla de GL lineal, la vista dentro de la ventana, y el
estado de píxeles cabe en v2. Si no, ese dibujo va como siempre (la CPU transforma, v2/v1/software): las dos formas pintan en
el mismo destino. Cada VBO se sube **una vez** (`glBufferData`), `glBufferSubData` sube **solo el trozo**, y un `GPU3D_EXPULSO`
se resuelve subiendo otra vez (con los huecos donde caigan). Si lo de un cuadro no cabe en la memoria de la GPU (tres cuadros seguidos
con más de 24 subidas por expulsión), v3 se deja en ese contexto y se sueltan sus búferes: expulsar en cada dibujo es mucho más lento
que transformar con la CPU. `ESTADO2` y `TRANSF` solo se mandan cuando cambian. `SAGUI_GL_V3=0` lo quita (para comparar).

> Un mismo objeto dibujado en dos pasadas, una por v3 y otra por la CPU, puede tener el Z distinto en el último bit (dos
> aritméticas): si aparece «lucha de Z» en una pasada de detalle, es eso.

### Estado por driver

| Driver | v3 | Estado |
| --- | --- | --- |
| **simulado** (`gpu3d_sim.c`) | `G3D_V3_CPU_BITS3` (todo menos luces y niebla exponencial) | ✅ en QEMU (`tests/h457_gpu3d_v3.py`). «Registros» de prueba en `gpu3d-simulado.conf`: `inicio 1014 N` (N KB de memoria para búferes, en hexadecimal), `inicio 1018 1` (transforma mal: la autoprueba v3 lo nota), `inicio 101c 1` (sin v3) |
| **Radeon R5xx** (`gpu3d_r500.c`, X1400 del Amilo) | `V3`, `NEBLINA`, `TEX_MAT`, `TIRAS`, `COR_FLOAT` (sin índices) | ✅ **probado en el Amilo** (2026-11-22, revisión de D457). Se anuncia solo y decide la autoprueba v3; `inicio 4ff0 4000` en `/sistema/conf/gpu3d-r500.conf` lo quita. Medidas abajo |
| **Radeon R6xx/R7xx** (`gpu3d_r600.c`, HD 4500 del HP) | `V3`, `NEBLINA`, `TEX_MAT`, `INDICES`, `TIRAS`, `COR_FLOAT` | ✅ **probado en el HP** (RV710 `1002:9553`, 2026-11-22): autoprueba v3 con 0 píxeles distintos en las tres escenas. Solo se anuncia sola en esa tarjeta; en otra R6xx/R7xx, `inicio 28ffc 2` en `/sistema/conf/gpu3d-r600.conf` (y decide la autoprueba). Ver abajo |
| **Intel Gen9** (`gpu3d_intel9.c`, UHD 620 del Dell) | `V3`, `NEBLINA`, `TEX_MAT`, `INDICES`, `TIRAS`, `COR_FLOAT` (`0x23b`) | ✅ **probado en el Dell** (2026-11-22): autoprueba v3 con 0 píxeles distintos en las tres escenas. Ver «Intel Gen9: cómo está hecho» abajo |
| Evergreen, Intel Gen3 | — | sin hacer: siguen con v2 o v1 (la CPU transforma) |

### Radeon R5xx: cómo está hecho y qué probar en el Amilo

- Los búferes van en un **montón de la mitad de lo libre de la zona, hasta 64 MB** (en el Amilo, 58 MB) dentro de la zona de la VRAM
  del 3D (se pide con el primer búfer y se devuelve cuando no queda ninguno, para no quitar sitio a las texturas de quien no usa VBO;
  con los 4 MB de la primera versión el mundo de Brumacanto no cabía y cada dibujo expulsaba otro búfer). Se escriben por la ventana móvil, en
  write-combining si se puede (como las texturas, D432), tras esperar a que la GPU termine lo pendiente.
- El dibujo: `VAP_VF_MAX/MIN_VTX_INDX`, `3D_LOAD_VBPNTR` (un array por atributo, con la dirección en el mapa de la tarjeta, el
  tamaño y el paso en palabras) y `3D_DRAW_VBUF_2`.
- La etapa de vértices: `PROG_STREAM_CNTL` por array (floats de 2, 3 o 4; el color como 4 bytes normalizados), un **programa del
  PVS generado** (`r5_vs_gerar`, `r500_pacotes.h`): 4 `DOT` con las filas de la MVP para la posición; para la niebla, 3 `DOT` con
  la modelo-vista, `DOT`, `RSQ`, `MUL`, `MAD`, `MAX`, `MIN`; 2 `DOT` por juego con matriz de textura), `VAP_VTE_CNTL` con la w
  de verdad y las escalas de la vista (`SE_VPORT_*`), `VAP_CLIP_CNTL` con el recorte encendido y `SU_CULL_MODE`. El
  rasterizador y el programa de píxeles son los de v2. Valores comprobados contra `r300_reg.h`, `r300_emit.c`, `r300_render.c`,
  `r300_state.c` y `compiler/r3xx_vertprog.c` de Mesa 23.3.6, incluida la regla de que una instrucción no lee dos entradas ni dos
  constantes distintas.
- Tras un dibujo v3, el siguiente de v1 o v2 **vuelve a poner** su etapa de vértices (sin recorte, sin vista, sin descarte) con
  el vaciado y la espera de D451 delante. Cambiar de programa de vértices o de constantes lleva ese mismo vaciado.
- En el PC: `tests/r500_v3_unidad.c` ejecuta el programa generado con un simulador del PVS y lo compara con la cuenta de ES 1.1
  (240 combinaciones), y revisa las órdenes. **No puede saber si la X1400 entiende esas palabras igual**: eso es la autoprueba v3.

**Probado en el Amilo** (X1400 `1002:7145`, 2026-11-22, kernel de prueba con `arranque probar`, sin confirmar; revisión de
D457 por `depurador-drivers`). Primera vez: la GPU no se colgaba, pero la autoprueba v3 fallaba en la escena 0 (quads negros al
azar, **también en la referencia por v2**: dibujada dos veces daba 1751 píxeles distintos) y en la escena 1 (pintaba la tira
contraria). Causas y arreglos: `US_PIXSIZE` no cubría el registro que escribe el RS cuando no hay texturas (D457, revisión) y el
convenio de `SU_CULL_MODE` era el contrario. Después: **autoprueba v3 BIEN, las tres escenas con 0 píxeles distintos**, en cuatro
encendidos seguidos; `es11demo -g 5` con 1160 píxeles distintos del software (por v2, 1154: los bordes), 25 dibujos desde 25
búferes, sin expulsiones.

| Medida (vista 64 en Brumacanto, 300 cuadros) | sin v3 (`SAGUI_GL_V3=0`) | con v3 |
| --- | --- | --- |
| `es11demo -n 300`: lightmap / partículas / sombra / reflejo | 27 / 297 / 58 / 25 | 27 / 297 / 59 / 25 (no usan VBO) |
| `es11demo -n 300`: **mundo** (61 488 vértices en 25 VBO) | 46 | **206** |
| Brumacanto en ventana (1000x562), escala 100 / 50 | 8,4 / 9,4 | **9,7 / 21,3** |
| Brumacanto a pantalla completa (1024x768), escala 100 / 50 | 8,5 / 10,9 | **13,4 / 26,2** |

El peor cuadro de Brumacanto sigue en 410-470 ms con y sin v3 (al hacer la malla de un trozo nuevo: es de la CPU). Con la escala
al 100 % el cuello pasa a ser rellenar los píxeles (y en ventana, traer el cuadro a la RAM); con la escala al 50 %, v3 da ×2,3-2,4.

| Qué se ve | Qué quiere decir | Qué probar |
| --- | --- | --- |
| `[GPU3D] v3 ...: autoprueba BIEN` y `gpu3d` dice «vertices por la GPU (v3, D457): si» | funciona | — |
| `v3: escena 1 (...): FALLO` y las escenas 0 y 2 bien | el descarte de caras está al revés en esa tarjeta | `inicio 4ff0 1000` (la otra convención) |
| `v3: escena N: FALLO` | lo que sea: mirarlo | `gpu3d probar --detalle --guardar --ver 0`: en `dmesg` salen los **mapas** de las dos imágenes (`v3 mapa N`, CPU a la izquierda y GPU a la derecha) y si la referencia es estable (`leida otra vez`, `dibujada otra vez`) |
| `apagado: la GPU no respondio a tiempo en la autoprueba de gpu3d v3` | el CP se quedó parado (como en D451) | `inicio 4ff0 2802` (vaciado y espera delante de cada dibujo v3, sincronizado) y `gpu3d encender` |

`inicio 4ff0 4000` quita v3 (la CPU transforma, como antes de la D457). El antiguo `800` ya no hace falta y no hace nada.

### Radeon R6xx/R7xx: cómo está hecho y qué probar en el HP

- **Búferes:** un montón de hasta 64 MB (la mitad de lo libre) en la parte grande de la zona del 3D, sin ventana fija de la CPU;
  se escribe por la ventana móvil, a trozos de 4 MB, en write-combining si hay PAT (`20` en las banderas lo quita), tras esperar a
  que la GPU termine lo pendiente. Tras una subida, el siguiente dibujo vacía la caché de vértices (en la RV710 y la RV610, la de
  texturas) sobre el montón (`SURFACE_SYNC`). En el HP: 57 MB, el mundo de Brumacanto (vista 64) ocupa 10 MB en 96 búferes, sin
  expulsiones.
- **El sombreador de vértices** se genera por formato (`r63d_vs3_gerar`, `r600_3d_pacotes.c`; caché de 8 en la página de
  sombreadores, `0x9000`-`0xCFFF`): una cláusula de lecturas (un recurso por atributo, el `160 + k`, con el índice en `R0.x`) y el
  ALU: 4 `DOT4` con las filas de la MVP; para la niebla, 3 `DOT4` con la modelo-vista, `DOT4` del ojo consigo mismo, `SQRT_IEEE`
  (en la ranura t) y `MULADD` saturado; 4 `DOT4` por juego con matriz de textura; el color de floats saturado o el constante.
  Las salidas son las del de v2 (`PARAM0` color, `PARAM1` especular con la niebla en el alfa, `PARAM2-5` los juegos), así que el
  programa de píxeles de v2 no cambia. Las constantes (30) van al «constant file» del sombreador de vértices (`SET_ALU_CONST` desde
  la 256; el estado base ya pone `SQ_CONFIG.DX9_CONSTS`) y solo se mandan cuando cambia la `TRANSF`.
- **El rasterizador** (al pasar a v3; se deshace al volver a v1/v2 o al presentar): `PA_CL_CLIP_CNTL` con el recorte encendido
  (espacio de GL, atributos interpolados en el espacio de recorte), `PA_CL_VTE_CNTL` con las seis escalas de la vista y la w de
  verdad, `PA_CL_VPORT_*` de la `TRANSF` y `PA_SU_SC_MODE_CNTL` con `CULL_FRONT` para `G3D_DESC_POS` (cara de delante: la de las
  agujas del reloj en la pantalla; `8` en las banderas, la otra convención). El dibujo: `DRAW_INDEX_AUTO` o, con índices,
  `DRAW_INDEX` desde el búfer (16 bits); `VGT_PRIMITIVE_TYPE` 4/6/5.
- Contrastado con Mesa 23.3.6 (`r600_sq.h`, `r600d.h`, `r600_isa.c`, `r600_asm.c`: las reglas de los puertos de lectura y del
  «constant file», que en las R7xx son 2 parejas de canales por grupo; `r600_state.c`, `r600_state_common.c`, `r600_formats.h`) y
  xf86-video-ati (`r600_shader.c`, `r600_exa.c`, `r600_reg_r6xx.h`: las constantes del sombreador de vértices desde la 256 y el
  origen `ALU_SRC_CFILE_BASE`); todo MIT, solo valores y reglas.
- En el PC: `tests/r600_v3_unidad.c` (en `h457 unidad`) genera 600 programas al azar (formatos, niebla, unidades, matrices; R6xx y
  R7xx), los lee con un decodificador propio, los ejecuta con un simulador del ALU y los compara con la cuenta de ES 1.1; revisa
  también los puertos, la ranura t y las órdenes.

**Banderas** (`/sistema/conf/gpu3d-r600.conf`, `inicio 28ffc N`, se suman; `28ffc` no es un registro y no se escribe): `1` sin v3;
`2` v3 en una tarjeta sin confirmar; `4` cada dibujo v3 se espera y se dice (`r600 v3 sinc`); `8` el descarte al revés; `10` sin
índices; `20` los búferes sin write-combining. `gpu3d probar --detalle` añade la línea `r600 detalle (...): v3 (D457): ...`
(dibujos, transformaciones, subidas, montón, sombreadores generados).

**Medido en el HP dv7** (HD 4530, RV710; kernel de esta rama con `arranque probar`, 2026-11-22):

| Prueba | Sin v3 (`SAGUI_GL_V3=0`) | Con v3 |
| --- | --- | --- |
| `es11demo -n 300`: lightmap, partículas, sombra, reflejo | 714, 526, 750, 25 fps | 714-731, 526-535, 731-750, 26 fps (no usan VBO: igual) |
| `es11demo -n 300`: mundo (61 488 vértices en 25 VBO) | **45 fps** | **461-476 fps** |
| `es11demo -g 5` (mundo, GPU frente a software) | — | 791 píxeles distintos de 76 800 (bordes) |
| Brumacanto `--bench 300 --view 64`, en ventana | 9,3 y 9,3 fps | 35,5-38,0 fps con la CPU a 2100 MHz (P0); 19,2-22,6 a 1050 MHz (P1) |
| Brumacanto `--bench 300 --view 64 --fullscreen` | 10,4 y 5,4 fps | 46,1-46,2 fps en P0; 24,1-31,4 si la CPU baja a P1 |

**Por qué Brumacanto daba 19 o 37 fps** (revisión de `depurador-drivers`, 2026-11-22): no es el driver, es la temperatura del HP.
Con v3 el cuadro lo limita la CPU (la espera de la valla es ~1,3 ms de ~26 ms por cuadro, `r600 detalle`), y el HP baja la CPU
a P1 (1050 MHz) al pasar de 77 °C (`frecuencia`: «LIMITADA POR TEMPERATURA»; los `Notify(\_PR_.CPU0, 0x80)` del registro). Con
`frecuencia` antes y después de cada medida: en P0 siempre 35,5-38 fps (ventana) y 46 (pantalla completa) con 96 subidas; en P1,
19,2-22,6 con 102 subidas. Las 6 subidas de más son consecuencia, no causa: con cuadros más lentos el paseo del bench llega a
generar más trozos del mundo; se suben una vez cada uno (ninguna subida repetida, 0 expulsiones, 0 peticiones no atendidas).
**Para medir, mirar `frecuencia` antes y después y descartar las medidas en P1.**

Con la GART apagada (`gpu3d encender sin-gtt`, 2026-11-22): autoprueba v3 BIEN, `es11demo -g 5` 791 píxeles distintos (lo mismo),
`es11demo -n 300` «mundo» 46 fps (lo limita leer el cuadro de la VRAM sin caché, no v3), Brumacanto en ventana 15,8 fps (en P1).

**Qué ejecutar en el HP** (o en otra R6xx/R7xx; ahí, antes, `echo 'inicio 28ffc 2' > /sistema/conf/gpu3d-r600.conf`):

```
volumen 10
gpu3d probar --detalle; gpu3d                 # «vertices por la GPU (v3, D457): si (bits 0x23b)»
dmesg | grep -E 'v3' > /home/v3.txt
es11demo -g 5; es11demo -n 300; SAGUI_GL_V3=0 es11demo -n 300
brumacanto --seed Prueba --size 160x160x48 --view 64 --bench 300     # BRUMA_BENCH en dmesg
```

| Qué se ve | Qué quiere decir | Qué probar |
| --- | --- | --- |
| `v3: escena 1 (...): FALLO` y las otras bien | el descarte de caras al revés en esa tarjeta | `inicio 28ffc 8` (con `2` si no está confirmada: `a`) |
| `v3: escena 0: FALLO` con casi todos los píxeles distintos | no lee los arrays o no ejecuta el programa | `inicio 28ffc 4` (cada dibujo se espera y se dice) y mandar `dmesg` |
| solo falla la escena 1 por los índices | `DRAW_INDEX` | `inicio 28ffc 10` |
| `apagado: la GPU no respondio a tiempo en la autoprueba de gpu3d v3` | el CP se quedó parado | `inicio 28ffc 1` (sin v3) y mandar `dmesg` |

### Intel Gen9: cómo está hecho y qué probar en el Dell

- **Sombreador de vértices SIMD8 generado** (`i9_vs_gerar`, `intel9_3d_pacotes.c`) con el mismo ensamblador que los de
  píxeles: cada canal es un vértice; entrada g0, g1 (asideros de la URB), las constantes empujadas desde g2 y detrás los
  atributos del VF (4 registros cada uno); 4 filas de la MVP con MUL/ADD, la niebla con 3 filas de la modelo-vista, `MATH SQRT`
  y `sat(neb_a - d·neb_b)` (en el alfa del especular o mezclada en el color), dos filas de la matriz de textura por unidad;
  sale la misma VUE que escribía el VF en v1/v2 (cabecera, posición, color, especular, (s, t) de cuatro unidades) por
  mensajes URB SIMD8 de dos ranuras, la cabecera copiada de g1 con NoMask y el último con EOT desde g112. Caché de 8
  sombreadores de 4 KB en la zona (`Z_KV`). Hasta 149 instrucciones.
- **Constantes** por el búfer 3 de `3DSTATE_CONSTANT_VS` (dirección entera; en Gen9 valen con el
  `3DSTATE_BINDING_TABLE_POINTERS_VS` de detrás), solo cuando cambian. `DispatchGRFStartRegisterForURBData` = 2: las
  constantes y los atributos van juntos («URB portion (Constant + Vertices)»); con 2 + nc no salía nada en el Dell.
- **Recorte, división por w, vista y descarte de caras por la GPU**: `3DSTATE_CLIP` (ClipEnable, ViewportXYClipTest, sin banda
  de guarda: lo que se sale de la vista se recorta), `3DSTATE_SF` (ViewportTransformEnable), `SF_CLIP_VIEWPORT` con la vista
  de la `TRANSF`, `3DSTATE_RASTER` con el Z cercano y lejano y `CullMode` (área > 0 en la pantalla = sentido horario = FRONT
  con FrontWinding CW, como anv). Al volver a un dibujo v1/v2 se apagan con una espera del cauce delante.
- **Búferes residentes en RAM con caché**: un montón de 16 MB (o lo que haya seguido) mapeado en la PPGTT en `0x3C000000` con
  el índice 4 del PAT (WB en la LLC, como el destino de D448); la CPU sube con `memcpy` y el VF lee con MOCS 1. Se crea con el
  primer búfer y se queda hasta apagar el 3D. Un búfer por atributo en `3DSTATE_VERTEX_BUFFERS` (con su tamaño: el VF no lee
  fuera), índices de 16 bits con `3DSTATE_INDEX_BUFFER`, tiras y abanicos con `3DSTATE_VF_TOPOLOGY`.
- Banderas nuevas de `/sistema/conf/gpu3d-intel9.conf` (`inicio fff0 N`): `100000` no anuncia v3, `200000` descarte de caras al
  revés (diagnóstico), `400000` cada dibujo v3 en su tanda esperada (las 8 primeras, al registro: `intel9 v3 sinc`); `200` también
  vacía de la caché de la CPU cada subida. `gpu3d probar --detalle` da los dibujos v3, las subidas y el montón.

**Qué ejecutar en el Dell** (kernel con `arranque probar`; el 3D arranca apagado: `gpu3d probar` primero; `volumen 10`):

```
gpu3d probar --detalle; dmesg | grep "v3" > /home/v3.txt
es11demo -g 5; es11demo -n 300; SAGUI_GL_V3=0 es11demo -n 300
brumacanto --seed Prueba --size 160x160x48 --bench 300 --view 64 [--fullscreen]   (y con SAGUI_GL_V3=0)
```

Medido el 2026-11-22 (kernel k457, programas de la rama en `/home/v3`): `es11demo` mundo 119 → 1666 fps (otra vez, más caliente:
86 → 1153), `-g 5` 736 píxeles distintos de 76 800; Brumacanto vista 64 en ventana 28,9 → 84,9 fps, a pantalla completa 13,8 →
30,6 (ahí manda presentar: leer el cuadro de 1920x1080 3 ms, copiarlo a la ventana 2 ms y el compositor; el driver no tiene
`apresentar`). Ojo: el Dell se calienta (paquete a 98 °C) y baja la CPU a x4; con el 3D apagado todo va por software (3 fps).

Revisión de `depurador-drivers` (2026-11-22, kernel `k457r1` = la rama con dos arreglos del 2D y del gobernador, nada de v3):
los paquetes de `3DSTATE_VS`, `3DSTATE_CLIP`, `3DSTATE_SF`, `3DSTATE_RASTER`, `SF_CLIP_VIEWPORT` y `3DSTATE_CONSTANT_VS`
coinciden campo a campo con `gen80.xml`/`gen90.xml` de Mesa; banda de guarda apagada; NoMask en la cabecera del mensaje URB
como `emit_urb_writes`. Medido otra vez en frío: autoprueba v3 BIEN (`0x23b`), mundo 1875 fps (v2 116), `-g 5` 736 píxeles
distintos (v2 733: es de la escena), Brumacanto 88,7 fps en ventana (v2 28,6) y 42,4 a pantalla completa (v2 22,7, ya
caliente). La bandera `200` no hace falta. Detalle en D457, «Revisión de v3 en Intel».

### Cambios en la interfaz

| Fecha | Cambio |
| --- | --- |
| 2026-11-22 | Intel Gen9 anuncia v3 (D457, «v3 en Intel»): sin cambios en la interfaz |
| 2026-11-22 | Primera versión de gpu3d v3 (D457): operaciones 15 a 18, `gpu3d_vbuf_t`, `gpu3d_atrib_t`, `gpu3d_transf_t`, `gpu3d_luz_t`, `gpu3d_luzes_t`, `gpu3d_desenho3_t`, `gpu3d_capas_t.bits3/vbufs/max_luzes` (de los reservados), `GPU3D_EXPULSO`, dos funciones al final de `gpu3d_driver_t` (`vbuf`, `desenhar3`), `G3D_BUF_VERTICES` y `g3d_vbmonte_t`. Nada de v1 ni de v2 cambia |

## Presentar sin volver a la RAM (D432)

Hasta la v8.5.5.1 un programa que dibujaba por la GPU (el renderizador «saguios-gpu» de la SDL 2, D288) tenía que
**traer cada cuadro a la RAM** (`GPU3D_OP_LER`) para que el compositor lo copiara a su búfer de atrás y de ahí otra vez a
la memoria de vídeo: la lectura por la apertura de la CPU (sin caché, ~10 MB/s en las R7xx) se comía toda la ganancia.
Desde la D432 la capa tiene una operación más, **presentar**: la GPU copia (escalando) el destino del 3D, o una textura,
**a la superficie visible de la pantalla**, y el cuadro no vuelve a la RAM.

### La operación (`kernel/inc/abi.h`)

`GPU3D_OP_APRESENTAR` (14), `ptr` = `gpu3d_apresentar_t` (40 bytes, solo enteros):

| Campo | Qué es |
| --- | --- |
| `fonte` | `GPU3D_DESTINO_PRINCIPAL` (el destino del 3D) o un hueco de textura ARGB (la textura va **directa** a la pantalla) |
| `janela` | la ventana del programa: tiene que ser suya, estar a pantalla completa y tener el foco |
| `ox, oy, ol, oa` | rectángulo de la fuente, en sus píxeles |
| `dx, dy, dl, da` | rectángulo de destino, en píxeles **del contenido de la ventana** (la capa lo lleva a la pantalla con la misma cuenta que la pantalla completa del compositor); lo que queda fuera se pinta de negro (bandas) |
| `flags` | `G3D_APRES_LINEAR` (filtro bilineal; sin él, vecino más próximo, como el software) y `G3D_APRES_VSYNC` (esperar al retrazado vertical) |

Devuelve 0 (presentado; la GPU ya terminó), `GPU3D_SEM_SUPORTE` («**ahora no**»: el compositor no cede la pantalla, o
el driver no puede: ese cuadro se presenta como siempre, con `LER` y la ventana) o un error (< 0: la GPU falló y la capa
la apagó, como en cualquier otra operación). El bit de capacidad es **`G3D_CAP_APRESENTAR`** (`GPU3D_OP_CAPACIDADES`); no
depende de `G3D_CAP_V2` (Evergreen lo tiene siendo v1). `info_gpu3d_t.reservado[3..5]` = cuadros directos, cuadros que
no se pudieron y la ventana que tiene la pantalla + 1 (lo enseña `gpu3d`).

### Quién tiene la pantalla: la exclusiva del compositor (`kernel/gui/janelas.c`)

Dos que escriben a la vez en la pantalla se pisan, así que **solo presenta quien tiene la pantalla completa en exclusiva**:

- La capa pregunta al compositor antes de **cada** cuadro (`gui_exclusiva_apresentar`). Se la cede a la ventana si es del
  proceso que llama, está **a pantalla completa y con el foco** y no hay nada que deba verse encima.
- Mientras la tiene, el compositor **no pinta**: `gui_mostrar` descarta lo marcado y `video_mostrar` tampoco escribe (ni la
  flecha: la pone la capa 3D encima de cada cuadro, con la zona «sin cursor» de la ventana respetada, D230).
- **La recupera** (y repinta todo) en cuanto: la ventana pierde el foco o la pantalla completa (**Alt+Tab**, otra ventana,
  Esc, F11, se cierra, el programa muere), aparece un **aviso**, un menú, el panel de la bandeja, el diálogo de permisos,
  la pantalla de **sesión o de bloqueo**, el **salvapantallas** o la pantalla apagada (D422), hay más de una pantalla
  (D290), el programa vuelve a actualizar su ventana por el camino normal, suelta el motor (`FECHAR`), el 3D se apaga
  (`gpu3d apagar`, el vigilante) o pasan 3 s sin presentar. La comprobación se repite en cada vuelta del gestor de ventanas
  (cada tic), no solo cuando el programa presenta.
- Al recuperarla, **el último cuadro de la pantalla se trae a los píxeles de la ventana** (una lectura de la memoria de
  vídeo, lenta pero una sola vez): lo que se ve detrás del aviso o del panel de Alt+Tab es ese cuadro aunque el programa
  esté en pausa. El programa lo nota porque su siguiente `APRESENTAR` devuelve «ahora no»; cuando el motivo desaparece (el
  aviso caduca), el siguiente cuadro vuelve a ir directo. En el log: `[GUI] pantalla exclusiva: cedida a la ventana N` y
  `... la ventana N la devuelve tras M cuadros (motivo)`.
- Solo con la pantalla de **32 bits** (`0x00RRGGBB`) y **una** pantalla. Una captura de pantalla (`SIS_CAPTURAR_TELA`, Impr
  Pant) hecha mientras dura ve el búfer del compositor, no el cuadro de la GPU (pendiente).

### Lo que hace un driver (`int (*apresentar)(const g3d_apres_t *p)`, al final de `gpu3d_driver_t`; puede ser `NULL`)

- `p == NULL`: «¿puedes escribir en la pantalla de ahora?» (`G3D_OK` / `G3D_NAO`), sin tocar la GPU. La capa lo pregunta
  al encender y de ahí sale el bit de capacidad.
- Si no: copia el rectángulo de la fuente al de la pantalla (ya recortados y en píxeles de pantalla), pinta antes las
  bandas negras si lleva `G3D_APRES_BANDAS` (la capa lo pone al tomar la pantalla, si cambia el rectángulo o si la flecha
  cayó sobre una banda), **espera a que la GPU termine, con plazo**, y deja el destino de dibujo como estaba.
- La pantalla vive en la misma VRAM que el 3D en las tres familias, así que **el destino de la copia es la superficie del
  CRTC, con su paso**: no hay copia intermedia. Es un dibujo normal (dos triángulos con la fuente como textura, sin Z ni
  mezcla) con el destino de color cambiado mientras dura. Piezas comunes en `gpu3d_mem.c`: `g3d_apres_bandas`,
  `g3d_apres_quad`, `g3d_quad_cor`.
- **Seguridad:** de la pantalla solo se **leen** registros (superficie, paso y modo del controlador, para comprobar que
  siguen como al encender, y el bit de retrazado); **no se escribe ninguno** (ni la base del CRTC: no hay doble búfer).
  Si la pantalla cambió, está en mosaico o su paso no sirve, el driver contesta «no puedo» y todo sigue como antes.

| Familia | Dónde está la pantalla | Cómo | Estado |
| --- | --- | --- | --- |
| **R5xx** (`gpu3d_r500.c`, X1400 del Amilo) | `mc_tela` (MC `c0000000` en el Amilo), ARGB8888 lineal | `r5_vaciar` (vaciar las cachés **antes** de esperar al 3D limpio, D426) → `RB3D_COLOROFFSET0/COLORPITCH0` a la pantalla → bandas y rectángulo con `desenhar_int` (v1) → la valla de siempre. La ventana móvil de la CPU no interviene en la copia; las texturas se suben por ella en write-combining | ✅ **probado en el Amilo** (abajo) |
| **R6xx/R7xx** (`gpu3d_r600.c`, HD 4500 del dv7) | `base.mc_tela` (32 bits) | valla → `r63d_alvo_modo` (color = pantalla con el `ARRAY_MODE` de `D1GRPH_CONTROL`, sin Z) → sombreadores de v1, `r63d_textura`, `r63d_estado` → valla con `SURFACE_SYNC` sobre la pantalla → `r63d_alvo` del programa. Exige el paso de la pantalla alineado al grupo de la tarjeta (D402) y fuente de hasta 1024x1024 | **sin probar en hardware** (el dv7 estaba apagado) |
| **Evergreen/NI** (`gpu3d_evergreen.c`, HD 7600M del Toshiba) | `mc_tela` de **40 bits** (D408: `f:00000000`); `CB_COLOR0_BASE` lleva los bits 39:8 | como las R7xx con `eg3d_alvo_modo` (modo de `GRPH_CONTROL`; el VBE del Toshiba deja lineal alineada). La **prioridad de la pantalla** (D424) la pone y mantiene el 2D (`acel_r600.c`); presentar no la toca y el log dice cómo está | **sin probar en hardware** (el Toshiba estaba apagado) |
| **Simulado** (`gpu3d_sim.c`, QEMU) | el framebuffer de la tarjeta (`video_placa`) | la CPU escala y escribe ahí (no en el búfer del compositor): una captura del QEMU lo ve. `inicio 1008 1` en `gpu3d-simulado.conf`: «no puedo» | probado (h432) |
| **VirGL** (`gpu3d_virgl.c`, QEMU/Proxmox con `virtio-vga-gl`, D474) | el respaldo del escritorio de la virtio-gpu (`video_placa`) | el cuadro vuelve del anfitrión (`TRANSFER_FROM_HOST_3D` con valla), la CPU lo escala al respaldo y `vg_atualizar`/`vg_descargar` lo llevan al scanout | probado en QEMU con WSLg (h474) |
| Intel Gen3 | — | sin la operación: sigue como hasta ahora | — |

**Probado en el Amilo (X1400 `1002:7145`, pantalla de 1024x768, 2026-11-20; kernel de prueba con `arranque probar`, sin
confirmar).** La imagen que deja la GPU es la correcta (el patrón de `sdl2presentar`, traído de la memoria de vídeo y
capturado: colores, orientación, escalado y bandas), 0 fallos de la GPU y el 2D sigue activo. Medidas (cuadros por
segundo; «textura» = una textura por cuadro generada por la CPU, 200 cuadros):

| | GPU directo (D432) | GPU con vuelta a la RAM (antes) | Software |
| --- | --- | --- | --- |
| `sdl2presentar` 320x200 | **333** | 46 | 122 |
| `sdl2presentar` 640x480 | **177** | 32 | 83 |
| `sdl2presentar` 800x600 | **123** | 29 | 67 |
| `sdl2presentar` 1024x768 (1:1) | **73** | — | 53 |
| `sdl2presentar -m dibujo` 640x480 | **238** | — | — |
| DOOM (Freedoom, `-timedemo demo1 -fullscreen`, lienzo 800x600) | **109** | — | 82 |

La primera tanda, con la textura subida por la ventana **sin caché** de la CPU (~100 MB/s), perdía contra el software
salvo a 320x200 (147 / 68 / 61 fps, y DOOM a ~23): por eso `r5_textura` sube ahora con **write-combining** por la ventana
móvil cuando el framebuffer ya lo usa por la PAT (`inicio 4ff0 40` en `gpu3d-r500.conf` lo quita). **Falta ver con los
ojos** (nadie miraba la pantalla): desgarro, parpadeo de la flecha, Alt+Tab y avisos en la máquina.

### Desgarro

La copia va a la superficie que se está mostrando (búfer frontal): si cruza el barrido, ese cuadro se ve partido por una
línea. Con **`G3D_APRES_VSYNC`** el driver espera al principio del retrazado vertical antes de avisar al procesador de
órdenes: **solo lee** el bit de estado del CRTC (`D1CRTC_STATUS` 0x609C en R5xx-R7xx, `CRTC_STATUS` 0x6E8C en Evergreen),
con un plazo de 20 ms (si no llega, presenta igual y lo cuenta: «sin retrazado a tiempo» en el detalle). Es una espera
activa de hasta un cuadro, por eso **no va por defecto** (`SDL_SAGUI_GPU_VSYNC=1`). El doble búfer cambiando la base del
CRTC (sin desgarro y sin espera activa) **no se ha hecho**: obliga a escribir registros de pantalla (`D1GRPH_PRIMARY_SURFACE_
ADDRESS` y el bloqueo de actualización), que hasta ahora ningún driver 3D toca, y a devolverlos bien en cada salida
(Alt+Tab, fallo, apagado); queda anotado para cuando el camino directo esté confirmado en las tres máquinas.

### La SDL 2 (`userland/libsdl2/sdl2gpu.h`, `sdl2_sagui.c`; detalles y cómo se enciende en cada juego: `docs/sdl-gpu.md`)

- Con el renderizador por GPU y la ventana **a pantalla completa**, `SDL_RenderPresent` pide `APRESENTAR` en vez de `LER`.
  Si el kernel dice «ahora no», ese cuadro va como antes (y el lienzo se presenta entero al compositor).
- **Una textura por cuadro** (DOOM, AoEN con `render2d`, emuladores): el `SDL_RenderCopy` opaco que cubre todo el lienzo
  **se aplaza**; si nada se dibuja encima, la fuente de la presentación es **la textura** y la GPU la lleva escalada a la
  pantalla: la subida va a la VRAM, una sola copia por la GPU, y el escalado deja de hacerlo la CPU.
- **Superficie de la ventana** (`SDL_UpdateWindowSurface`: Tzar, OpenTTD), experimental y solo con `SDL_SAGUI_GPU=1`: el
  lienzo se sube a una textura cuando cambia y se presenta igual.
- El renderizador por GPU **sigue siendo opcional** (D288, D394): nada cambia para quien no lo pide.

### Cómo leerlo en el log y en `gpu3d`

- Al encender: `[GPU3D] v2: capacidades ...; presentar sin volver a la RAM (D432): si|no` (en Evergreen, `[GPU3D] v1:
  presentar ...`) y, en R7xx/Evergreen, `[GPU3D] r600|evergreen presentar (D432): se puede | <por qué no> (pantalla MC ...,
  paso, tamaño, modo de superficie, alineación[, prioridad de la pantalla])`.
- La primera vez que un programa presenta: `[GPU3D] presentar (D432): fuente textura|destino LxA+x+y de LxA -> pantalla
  LxA+x+y de LxA` y la del driver: `[GPU3D] r500|r600|evergreen presentar (D432): primera copia a la pantalla: fuente MC
  ... -> pantalla MC ...`.
- `gpu3d`: `presentar en la pantalla (D432): si; N cuadros directos, M por la RAM; pantalla en exclusiva: ...`.
- `gpu3d probar --detalle`: la línea `presentar (D432)` del detalle de cada driver (copias hechas y cuántas sin retrazado
  a tiempo). Si la GPU no termina una copia, es un fallo como cualquier otro: `[GPU3D] apagado: la GPU no respondio a
  tiempo al presentar en la pantalla`, el compositor recupera la pantalla y todo sigue por software.

### Qué ejecutar en cada máquina (para el mantenedor y `depurador-drivers`)

Con un kernel de esta rama (`arranque probar`, sin confirmar) y `/apps/bin/sdl2presentar` de esta rama (un juego sintético
a pantalla completa; vale copiarlo a `/home`). En las tres, con el escritorio:

```
gpu3d probar --detalle           (BIEN y «presentar sin volver a la RAM (D432): si»)
sdl2presentar -n 300 -l 640x480                              (GPU, directo: fps y «directos 300, por la RAM 0»)
SDL_SAGUI_GPU_PRESENTAR=0 sdl2presentar -n 300 -l 640x480    (GPU trayendo el cuadro a la RAM: lo de antes)
SDL_SAGUI_GPU=0 sdl2presentar -n 300 -l 640x480              (software)
sdl2presentar -n 300 -l 640x480 -m dibujo                    (la fuente es el destino del 3D)
sdl2presentar -n 60 -l 320x180 -e 20 -a                      (patrón fijo: rojo, verde, azul, blanco; bandas negras
                                                              arriba y abajo; a los 10 s un aviso; Alt+Tab y F11)
SDL_SAGUI_GPU_VSYNC=1 sdl2presentar -n 300 -l 640x480        (con espera al retrazado: como mucho los Hz de la pantalla)
dmesg > /home/d432.txt
```

Qué mirar: que el patrón se ve entero, con los colores en su sitio (rojo arriba a la izquierda, verde arriba a la derecha,
azul abajo a la izquierda, blanco abajo a la derecha) y las bandas negras; que no parpadea ni deja basura; que el aviso y
Alt+Tab devuelven el escritorio bien y que F11 vuelve al juego; que `gpu3d` sigue «activo» con «fallos 0»; y los fps de
las tres primeras órdenes. Después, un juego: `SDL_SAGUI_GPU=1 chocolate-doom` (o `AOEN_RENDER=1 aoen`) a pantalla
completa. Si la pantalla se queda negra o con basura: Alt+Tab (o Ctrl+Esc) la devuelve al compositor; `gpu3d apagar`
quita el 3D; nada queda grabado, un reinicio vuelve al kernel bueno.

## OpenGL ES por la GPU en un juego de verdad: Brumacanto (D451)

- **Niebla en el vértice para v2:** sin `G3D_CAP_NEBLINA`, con una unidad en MODULATE o ninguna, SaguiGL mezcla la niebla
  en el color y el especular (como v1); un dibujo con prueba alfa y niebla (el mundo de Brumacanto) ya cabe en la X1400.
- **Búfer de atrás propio** en las superficies de ventana de EGL (la ventana solo ve cuadros enteros) y **presentar directo**
  (`GPU3D_OP_APRESENTAR`) desde `sgl_trocar` cuando el kernel lo acepta (pantalla completa con el foco).
- **Escala de dibujo:** `SDL_SAGUI_GL_ESCALA=25..100` (pista o entorno) o `sgl_escala()`; se amplía al presentar.
- **Lotes de 3072 vértices**, descarte de caras antes de recortar, recorte sin copias.
- **R5xx:** espera al 3D antes de cada orden del 2D por el anillo (sin ella, una copia del compositor entre dos dibujos v2
  colgaba la X1400), cola de 256 KB en write-combining; la vuelta por la GART (D391) se enciende sola en la X1400.
- **Perfil:** `SAGUI_GL_PERFIL=1 programa` escribe en el log cada 2 s lotes y vértices por cuadro, ms en SaguiGL y en el
  kernel, vuelta a la RAM, copia a la ventana, presentar y cuántos cuadros fueron directos, por la RAM o por software.
  Para aislar un fallo de un driver: `SAGUI_GL_LOTE=N`, `SAGUI_GL_NEBLINA_CPU=0`, `SAGUI_GL_PRESENTAR=0`,
  `SAGUI_GL_DIAG=1` (forzar v2) o `2` (sin prueba alfa en la GPU); en R5xx, `inicio 4ff0 N` con los bits 80, 100, 200, 400.

Medidas (Amilo, vista 64): por la GPU 8,3 cuadros/s en ventana y 8,4 a pantalla completa (antes 0,9); el límite son los
vértices (unos 55.000 por cuadro: ~40 ms en el kernel y ~55 ms en SaguiGL).

## Intel Gen3 (915, 945, G33, Pineview)

`kernel/dispositivos/gpu3d_intel.c` + `gen3_pacotes.h`. Modelos (PCI `8086:`): `2582` 915G, `2592`
915GM, `2772` 945G, `27A2` 945GM (Amilo), `27AE` 945GME, `29B2` Q35, `29C2` G33, `29D2` Q33, `A001`
y `A011` Pineview. Las X3100 (GM965) y posteriores son Gen4: otra tubería (sombreadores en la EU);
**no** las cubre este driver.

- **Anillo**: el mismo ring (RCS) que usa el blitter 2D (`acel_intel.c`, D195). Si el 2D lo dejó
  apagado por ser más lento que la CPU, lo enciende el 3D; si el 2D **falló**, el 3D no se intenta
  (salvo `--gpu3d=intel`).
- **Órdenes**: por **lotes** (`MI_BATCH_BUFFER_START` a un búfer de 64 KB) o escritas **en el
  anillo** (`gpu3d encender anillo`, `--gpu3d-anillo`). Cada entrega termina en `MI_FLUSH`.
- **Estado**: fijo (reglas de rasterizado de OpenGL, tijera apagada, sin estado indirecto), destino
  (`3DSTATE_BUF_INFO` de color y Z, `DST_BUF_VARS`, `DRAW_RECT`), `S2/S4/S5/S6` (formato del
  vértice, Z, mezcla), mapa y amostrador de la textura y el **programa de píxeles**.
- **Programa de píxeles** (formato i915, instrucciones de tres palabras), escrito a mano:
  - color: `dcl difuso; dcl especular; add_sat oC, difuso, especular.xyz0`
  - textura: `dcl difuso; dcl especular; dcl t0; dcl s0; texld r0, s0, t0; mad_sat oC, r0, difuso, especular.xyz0`
- **Vértices** dentro de la orden (`3DPRIMITIVE` con lista de triángulos): XYZW + color + especular
  + (u, v) = 8 palabras, que es `gpu3d_vert_t` tal cual.
- **Limpiar**: con el blitter (`XY_COLOR_BLT`), que es lo que ya prueba el 2D.
- **Memoria**: la que el VBE dejó en el GTT (la «robada»), entre la zona de prueba del 2D y el
  ring. Sin la opción `gtt` **no se escribe el GTT**, ni registros de pantalla, relojes o energía
  (nunca). Con 8 MB robados y 1024x768 quedan unos 4,5 MB: cabe un destino de 640x480 con Z de 24
  bits y alguna textura; uno de 800x600 solo con Z de 16 bits. Si no cabe, ese programa va por software.
- **RAM del sistema en el GTT** (D358, **solo a mano**: `gpu3d encender gtt`, `gpu3d encender anillo
  gtt` o `mkimagem.py --gpu3d-gtt`): 16 MB de RAM normal (8 MB si hay poca libre) en una ventana del
  GTT libre (comprobado leyendo: todas sus entradas iguales) al final de los primeros 128 MB de
  direcciones gráficas. Ahí va el **destino de color** (traer el cuadro es leer RAM con caché, no la
  apertura sin caché) y, si la BIOS dejó encendida la página de vaciado del chipset y la GPU lee bien
  la RAM, **las texturas**. El Z sigue en la memoria del VBE (solo lo ve la GPU) salvo que no quepa.
  Antes de usarla se comprueba que la CPU ve cada página por la apertura y que la GPU escribe en la
  RAM y solo donde debe. Las páginas no se liberan nunca (hasta reiniciar), para que una GPU colgada
  no pueda escribir en memoria del kernel; si algo falla, las entradas vuelven a lo que tenían (si la
  GPU se colgó, se quedan apuntando a esas páginas reservadas) y no se repite hasta reiniciar.
- **Traer el cuadro**: sin la RAM en el GTT, la CPU lee la memoria de vídeo por la apertura (sin
  caché: lento); con ella, RAM con caché tras invalidarla. `gpu3d probar` lo mide aparte
  («traer N píxeles a la RAM»): es la cifra a comparar entre `gpu3d encender` y `gpu3d encender gtt`.
- **Robustez** (D358): cada entrega empieza invalidando las cachés de lectura de la GPU (texturas
  recargadas en el mismo sitio); tras cada espera se lee EIR y un error nuevo (orden no válida, tabla
  de páginas) apaga el 3D; si el lote termina pero no hace lo que lleva, se sigue en el anillo.

### Encendido por etapas (D358) y cómo leer el log

`[GPU3D] intel: etapa N` es el último paso del encendido que se hizo; después vienen la autoprueba
común (0 a 7) y la medida.

| Última etapa en el log | Qué significa | Qué mirar |
| --- | --- | --- |
| ninguna (`no es una Intel Gen3 de la lista`) | el modelo no está en la lista | el id PCI |
| `etapa 1` y `el ring no arranca` | el ring (del 2D o propio) no se pone en marcha | las líneas `[ACEL2D] intel` |
| `etapa 2` y nada más / `no avanza con una orden vacia` | el ring está parado o colgado | `intel al iniciar`: CTL, HEAD, TAIL |
| `etapa 3: ... no guarda lo escrito` | la memoria que anuncia el VBE no está toda en el GTT | la dirección; probar otra resolución |
| `etapa 3` y `etapa 4: el blitter no termina/no escribe bien` | el blitter no llega a la zona 3D | la palabra que salió; `intel etapa 4` |
| `etapa 4` y `etapa 5: el lote no termina` | `MI_BATCH_BUFFER_START` cuelga el ring | reiniciar y `gpu3d encender anillo` |
| `etapa 5: el lote termina pero no hace lo que lleva` | lotes sin efecto: sigue solo en el anillo | `EIR`, `IPEHR` de esa línea |
| `etapa 6: RAM en el GTT no: ...` | la RAM en el GTT no se usa; el 3D sigue con la del VBE | el motivo (ventana en uso, GTTADR, página de la apertura...) |
| `etapa 6: la GPU no termina o marca un error` | la GPU se colgó o marcó error con la RAM | `PGTBL_ER`, `EIR`; no usar `gtt` en esa máquina |
| `etapa 7: la GPU no termina el estado 3D` | una orden del estado 3D cuelga la GPU | `IPEHR` (la orden), `INSTDONE` |
| `etapa 7` con `pixel ... no coincide` | el estado se acepta pero el triángulo no sale bien | la autoprueba dice qué pieza |
| `la GPU marco un error ... (EIR ...)` | orden no válida (bit 0) o fallo de la tabla de páginas (bit 4) | `IPEHR`, `PGTBL_ER` |

Fuentes: documentación de Intel del 3D de Gen3 y las cabeceras de Mesa (MIT), solo valores y
secuencias; código propio (`docs/terceros.md`). Nada del driver i915 de Linux con GPL.

### Qué no hace todavía

- Texturas que se repiten con lados que no son potencia de 2 (Gen3 no sabe): ese programa va por
  software. Filtro bilineal: el driver lo tiene, SaguiGL aún no lo pide.
- Los píxeles no son iguales bit a bit a los del motor por software (bordes, redondeo, luz y niebla
  por vértice): por eso los contextos en memoria (`sgl_criar_contexto_mem`), que usan las pruebas
  de píxeles, siguen por software salvo que se pida la GPU con `sgl_gpu_ligar()`.
- Las líneas van como rectángulos de un píxel; los puntos no están.

## Intel Gen9-12 (3D) — Skylake, Kaby Lake, Coffee/Whiskey/Comet Lake (D438)

`kernel/dispositivos/gpu3d_intel9.c` + `intel9_3d_pacotes.c` / `kernel/inc/intel9_3d.h` (ensamblador mínimo de las EU, los dos
sombreadores, las órdenes de una tanda y los workarounds: lógica pura, probada en el PC con `tests/intel9_3d_unidad.c`) + la imagen del
contexto del RCS en `acel_intel_logica.h` (la de i915). **Probado en el Dell con UHD 620 (`8086:3ea0`, 2026-11-21): las 8 pruebas
BIEN, 330.943 triángulos/s, 1951 puntos, `sgldemo -g` BIEN en las 7 escenas** (D438, «Resultado en hardware»). Solo arranca en **Gen9** (la lista exacta de D434: Skylake,
Kaby Lake, Coffee, Whiskey, Amber y Comet Lake; la UHD 620 `8086:3ea0` del portátil de pruebas). Ice Lake (Gen11) y Tiger/Alder Lake (Gen12)
contestan «todavía no»: el codificador ya sabe sus formatos, faltan sus workarounds (D438).

- **No se enciende sola**, como las Radeon: `gpu3d` la muestra «apagado»; `gpu3d probar` o `gpu3d encender` la ponen en marcha con la
  autoprueba; `--gpu3d=intel` al grabar la imagen la deja «sin probar» (se prueba con el primer programa que abra el motor).
- **Cómo dibuja:** por el **motor de render (RCS)** con execlists (el mismo envío que el 2D de D434, que va por el BCS: motores distintos).
  Por **lotes** (D448): cada tanda lleva el estado fijo una vez (`PIPELINE_SELECT`, `STATE_BASE_ADDRESS`, URB, L3, `3DSTATE_*`), tantos
  dibujos como quepan (de cada uno solo el estado que cambia) y acaba con un PIPE_CONTROL que vacía color, Z y datos y escribe la marca.
  Se manda al leer el cuadro, al esperar, al cambiar destino o textura o al llenarse; la CPU monta la siguiente mientras la GPU dibuja.
  Al mandar trabajo pide al PCU la frecuencia máxima (RP0; sin esto la GPU se queda en 100 MHz) y vuelve a la del firmware tras 1 s
  sin trabajo. El sombreador de vértices va apagado (los vértices llegan ya en píxeles, como blorp de Mesa) y los de
  píxeles son dos, SIMD8, escritos a mano en la ISA de las EU: `color + especular` y `textura x color + especular`.
- **Memoria:** de 4 a 16 MB de la memoria robada detrás de la pantalla y de las zonas del 2D. La CPU la ve por la apertura, en WC lo
  que solo escribe (estado, anillo, vértices, texturas) y sin caché lo que lee. El **destino de color va en RAM del sistema con caché**
  (hasta 8 MB, mapeado solo en la PPGTT con WB en la LLC, D448): leer el cuadro cuesta lo que copiar memoria. El Z va en mosaico Y (la
  CPU no lo ve).
- **Si algo falla:** `STOP_RING`, los registros del motor al log y todo sigue por software. Si una tanda no terminó, el 3D no se vuelve a
  encender hasta reiniciar. Nunca se reinicia la GPU ni se toca la pantalla.

### Encendido por etapas y cómo leer el log

`[GPU3D] intel9: etapa N` es el paso que se está haciendo; si el encendido falla, `gpu3d probar` dice `etapa N (nombre): motivo`. Después
vienen la autoprueba común (0 a 7) y la medida.

| Etapa / línea | Qué significa | Qué mirar |
| --- | --- | --- |
| `etapa 0 (sonda de solo lectura)`: `el modelo ... no esta en la lista` / `Gen11/12 todavia no` | no es una Gen9 de la lista | el id PCI |
| `etapa 0`: `GTT de la pantalla: ...`, `no caben 4 MB de zona 3D en la memoria robada` | el GOP no dejó la pantalla en páginas seguidas, o la memoria robada (DVMT) es pequeña | las líneas `memoria robada ... MB`, `reservada`; subir la DVMT en la BIOS |
| `etapa 0`: `el plano N del pipe X ... usa la memoria de la zona 3D` | otra superficie del firmware está donde iría la zona | la línea del motivo |
| `etapa 1 (zona y GTT)`: `no quedo mapeada` / `no guarda lo escrito` | las entradas del GTT nuevas no valen o la apertura no llega a esa memoria | `zona 3D de N KB en la grafica ... (fisica ...)` |
| `etapa 2 (forcewake)`: `forcewake ... sin respuesta` | la GPU no despierta el dominio de render o el del GT | los acuses |
| `etapa 3 (RCS sin uso)`: `el RCS ya esta en uso` | alguien (firmware, otro sistema) dejó el RCS en marcha | `EXECLIST_STATUS`, `RING_CTL` |
| `etapa 4`: líneas `wa REG: antes -> despues` | los workarounds de Gen9; `MOCS 0 ... L3CC 0 ...` | si se cuelga aquí o después, probar las banderas 1, 2 y 20 |
| `etapa 5 (contexto y tanda vacia)`: `la tanda ... no termina` | el RCS no acepta el contexto (imagen, PPGTT, ELSP) o no escribe la marca | las líneas `tras el plazo` (HEAD, ACTHD, EXECLIST, CSB) |
| `etapa 6 (estado 3D sin dibujo)`: `no termina la tanda del estado 3D` | una orden del estado 3D cuelga el RCS | `IPEHR` (la orden), `INSTDONE`, `EIR` |
| `el motor marco un error (EIR ..., RING_FAULT ...)` | orden no válida o fallo de página (dirección fuera de la PPGTT) | `TLB` (las dos palabras del fallo) |
| autoprueba, prueba 0 o 1 | no salen píxeles: el VF, el SBE o el sombreador de píxeles | `--ver` (D390): ¿la sentinela `a1b2c3` sigue intacta? |
| prueba 2 o 7 (Z) | el Z en mosaico Y (`3DSTATE_DEPTH_BUFFER`) | |
| prueba 3 (mezcla) | `BLEND_STATE` y `3DSTATE_PS_BLEND` | |
| prueba 4 (textura) | la superficie de la textura o el muestreador; o la coherencia de lo que escribe la CPU | bandera 20 (MOCS) |
| prueba 5 o 6 (interpolado, especular) | las baricéntricas o los datos de los atributos (g4-g9) | |

Ajustes (D390) en `/sistema/conf/gpu3d-intel9.conf`: `inicio REG VALOR` para los registros 0x7000-0x73FC, 0xB000-0xB1FC y 0xE000-0xE8FC
(por LRI en cada tanda) y `inicio fff0 N` con banderas (no es un registro): `1` sin los workarounds del RCS, `2` sin los del GT, `4` sin los
de contexto, `8` sin el inhibidor de la carga del contexto, `10` las primeras 16 tandas al log, `20` sin tocar el MOCS 0 y 1. D448: `40` un
lote por dibujo, `80` la zona sin caché, `100` el destino en la zona, `200` vaciar la caché de la CPU antes de leer el destino en RAM,
`400` sin guardar el contexto, `800` marcas de tiempo de la GPU en cada tanda (`[GPU3D] intel9 perfil: ...`), `1000` sin pedir RP0,
`2000`/`4000`/`8000` pruebas (R_PWR_CLK_STATE, L3 con caché, L3CNTLREG una vez). `11c0` = el comportamiento de D438, para comparar.

Medidas (D448): cada 600 cuadros, `[GPU3D] intel9 cada 600 cuadros: ...` dice por cuadro las tandas, los dibujos y los microsegundos
esperando a la GPU, copiando el cuadro y montando; `gpu3d probar --detalle` añade la frecuencia (`frecuencia N MHz ... en RP0`), el PAT,
el MOCS y los totales. Si el cuadro sale con píxeles viejos o mezclados, probar `inicio fff0 200` (y después `100`).

### Qué probar en el portátil UHD 620 (Whiskey Lake, `8086:3ea0`, UEFI)

Con un kernel de esta rama **que lleve también if_ure** (D433: si no, se pierde la red por la Ethernet USB y el SSH). El 2D (D434) y el 3D
son independientes: el 3D se prueba aunque el 2D esté en la CPU. Por SSH o en una terminal, como administrador:

```
acel2d
gpu3d                      (debe decir «apagado» y «Intel Gen9 8086:3ea0»)
gpu3d probar --detalle     (las 7 etapas y las 8 pruebas)
dmesg > /home/intel9.txt
```

Si dice **BIEN**: `sgldemo -g` (SaguiGL por software contra la GPU, escena a escena), `sgldemo -n 600 -s` (fps por la GPU),
`sgldemo -p 300 --mem --gpu` (el paisaje) y `gpu3d apagar; sgldemo -n 600 -s; sgldemo -p 300 --mem` (por software), para comparar con
lo de D448 (4285 y 70-75 fps por la GPU, 2857 y 25 por software); con `inicio fff0 11c0` se vuelve a D438; `infopc --bench-gpu`, `gpu3d encender` (tiene que volver a encender sin reiniciar) y otro
`dmesg > /home/intel9-2.txt`. Mover ventanas mientras gira el cubo de `sgldemo` (con el escritorio). Al final, reiniciar con normalidad.

Si **falla**: la línea `etapa N` o `prueba N` y el `dmesg` (se trae con `scp`). Si se cuelga en una etapa, probar una vez con
`/sistema/conf/gpu3d-intel9.conf` = `inicio fff0 23` (sin los workarounds por MMIO ni el MOCS) y otra con `inicio fff0 8`, reiniciando
entre una y otra (tras un cuelgue el 3D no se vuelve a encender hasta reiniciar). Si la máquina deja de responder (no debería: solo se usa el
RCS), reiniciar: nada queda grabado.

### gpu3d v2 en Gen9 (D453)

`desenhar2`, `textura2`, `limpar2` y `capacidades` (sin `destino`). El sombreador de píxeles se **genera de la combinación** de las
unidades (`i9_ps2_gerar`: hasta 4 unidades con todas las `G3D_CB_*`, DOT3, escalas 1/2/4, constantes como inmediatos, especular y niebla)
y se guarda en una caché de 32 huecos de 4 KB en la zona (un hueco que usa el lote abierto no se pisa; si lo usa la tanda en la GPU, se
espera). El resto es estado de función fija de Gen9: prueba alfa (`BLEND_STATE` + `COLOR_CALC_STATE`, con `PixelShaderKillsPixel`),
esténcil de 8 bits aparte del Z (mosaico W, `3DSTATE_STENCIL_BUFFER`), las 8 funciones del Z, tijera (`SCISSOR_RECT`, siempre encendida
con el destino entero si no se pide), máscara de canales, mezcla con factores ampliados, aparte para el alfa y con resta, operación
lógica, espejo, NPOT y **mipmaps** (la disposición «2D» de Gen4-9, en mosaico Y o lineal con la bandera `20000`, `MIP Count`,
`Mip Tail Start LOD` 15 como isl, filtro de mipmaps y `Max LOD`). El sombreador va con **Vector Mask Enable** (`3DSTATE_PS`, como
Mesa antes de XeHP): sin él los píxeles «ayudantes» de cada 2x2 no calculan sus coordenadas y el LOD de los 2x2 a medio cubrir sale de
basura (en el Dell se leía otro nivel). El esténcil en mosaico W se programa con el paso al doble del ancho (como isl).
Un lote puede mezclar dibujos de v1 (32 bytes por vértice) y de v2 (88): al cambiar de formato se pone otro búfer de vértices.

Se anuncia solo lo que pasa las pruebas comunes en la máquina (`g3d_v2_provas.c`, al encender): en el Dell UHD 620, todo lo candidato
(`gpu3d probar` y `dmesg | grep "intel9 v2"`: «se anuncian ... de ...»). La bandera `10000` de `gpu3d-intel9.conf` no anuncia v2; `40000` baja el LOD un nivel y `80000` (diagnóstico) llena las texturas con mipmaps
de un patrón con su (x, y) de la disposición, para ver en el log de la prueba 15 dónde lee la GPU.
No hace todavía: cubos, anisótropo, DXT, coordenadas proyectivas ni dibujar en una textura (SaguiGL los hace por software).

### Qué no hace todavía

Cubos, anisótropo, DXT, coordenadas proyectivas y destino en textura de gpu3d v2 (D453); Gen11 y Gen12; destinos y
texturas de más de 2048x2048; filtro de mipmaps; el destino en mosaico Y (más relleno, pero habría que deshacer el mosaico al leer); un
gobernador de frecuencia común con el 2D y RC6.

## Radeon R6xx y R7xx (HD 2000-4000: R600, RV610-RV670; RV710, RV730, RV740, RV770) — D284, D357

`kernel/dispositivos/gpu3d_r600.c` + `r600_3d_pacotes.c` / `kernel/inc/r600_3d.h` (paquetes PM4, tres
sombreadores a mano y valores de inicio: lógica pura, probada en el PC) + `r600_sondar`
(`kernel/inc/r600_base.h`, la sonda de solo lectura que comparte con el 2D por DMA de la D223).
**Probado en el HP dv7** (`1002:9553`, RV710; D402, 2026-11-19): las 8 pruebas BIEN, `sgldemo -g` BIEN y 5 minutos
de `sgldemo` en ventana sin fallos, también con el 2D por DMA a la vez. Dos fallos corregidos entonces (`SURFACE_SYNC`
que no cubría el final de un rango sin alinear; paso de las superficies alineado al grupo de `GB_TILING_CONFIG`). La
zona va hasta 128 MB (16 con ventana fija de la CPU; el Z y las texturas en el resto, las texturas escritas por una
ventana móvil). Lo lento es **traer el cuadro** (la CPU lee la VRAM sin caché por el PCIe: ~10 MB/s con SSE2), así que
en ventanas pequeñas el software es más rápido; el detalle dice cuánto se va en la valla y cuánto en la copia. Ninguna
R6xx probada.

**GART de las R7xx (D406, sin probar todavía):** `gpu3d encender gtt` enciende la memoria virtual de la GPU (ventana
de 16 MB detrás de la VRAM, como Linux) y el cuadro vuelve a la RAM copiado por la propia GPU (CP_DMA) en vez de
leerlo la CPU. Por pasos con `/sistema/conf/gart-r700.paso` (0 solo lee ... 4 la GPU lee de la RAM); las líneas
`[GPU3D] r600 GART paso N` del log dicen hasta dónde llegó. `gpu3d encender` (sin `gtt`) vuelve a lo de antes.

**R6xx (D357, v8.5.2):** el mismo driver lleva desde la D357 las HD 2000/3000 (R600, RV610, RV620, RV630,
RV635, RV670; no las integradas RS780/RS880). Cambia el microcódigo (`R600_*`, `RV610_*` también para la
RV620, `RV630_*` también para la RV635, `RV670_*`: PFP de 2304 bytes y ME de 21504, cargados primero el ME
como `r600_cp_load_microcode`), los valores por defecto del motor (`r6_inicio_motor`, de `r600_gpu_init`),
`ME_INITIALIZE`, `START_3D_CMDBUF` al principio del estado base, la codificación de las operaciones de los
sombreadores, el paquete `SURFACE_BASE_UPDATE` tras poner las bases del color y del Z (rodeo de las RV6xx) y
la mezcla de la R600 (sin «por superficie»). Ninguna máquina del mantenedor tiene una R6xx: queda a la espera
de quien la tenga. El 2D por DMA de las R6xx sigue solo con `--acel2d=radeon` (D223); el 3D no lo necesita
(basta con que la capa 2D reconozca la tarjeta).

**Arreglo de la D357:** al fusionar la rama de las R5xx (`02bae96d`) se perdieron las líneas que ponían el
estado inicial de las R7xx: en el dv7 `gpu3d` decía «sin GPU con driver 3D» y `gpu3d probar` no encendía
nada. Vuelven a estar (y `tests/h284` y `tests/h359` lo vigilan).

### gpu3d v2 en R6xx/R7xx (D426, sin probar en hardware)

| Qué | Cómo | Bit |
| --- | --- | --- |
| Hasta 4 unidades con COMBINE y DOT3 | sombreador de píxeles generado (`r63d_ps2_gerar`): cláusula TEX (una `SAMPLE` por unidad) y cláusulas ALU; caché de 8 en la VRAM (`0x1000`-`0x8FFF` de la zona de sombreadores) | `COMBINAR`, `DOT3`, `unidades` = 4 |
| Niebla con el factor del vértice | en el sombreador (alfa del especular, color como literal) | `NEBLINA` |
| Esténcil de 8 bits | el Z de 24 bits es `DEPTH_8_24` (también en v1); `DB_DEPTH_CONTROL`, `DB_STENCILREFMASK(_BF)` | `ESTENCIL`, `LIMPAR2` |
| Prueba alfa | `SX_ALPHA_TEST_CONTROL` / `SX_ALPHA_REF`, con el Z tarde (`DB_SHADER_CONTROL`, como Mesa) | `ALFA_TESTE` |
| Tijera, máscara de color, 8 funciones de Z | `PA_SC_GENERIC_SCISSOR`, `CB_TARGET_MASK`, `DB_DEPTH_CONTROL` | `TIJERA`, `MASCARAS`, `Z_FUNCS` |
| Mezcla ampliada, separada, resta | `CB_BLEND(0)_CONTROL` con `SEPARATE_ALPHA_BLEND` | `MEZCLA_EXT/SEP/EQ` |
| Operación lógica | `ROP3` de `CB_COLOR_CONTROL` (sin mezcla) | `LOGICA` |
| DXT1/3/5 | `FMT_BC1/2/3` en 1D en mosaico (en lineal la tarjeta no las lee bien; D426, «DXT en la HD 4500»), paso de 8 bloques | `formatos` |
| NPOT con repetición, espejo, anisótropo | el muestreador (`WRAP`, `MIRROR`, `ANISO_*` y `MAX_ANISO_RATIO`) | `TEX_NPOT`, `TEX_ESPELHO`, `TEX_ANISO` |
| Dibujar en una textura | `r63d_alvo` sobre el hueco (ARGB, su paso), con un Z propio; `LER` lee la textura | `DESTINO_TEX` |

Va por software: mipmaps, cubos, coordenadas proyectivas y lados de más de 1024. Los vértices v2 van tal cual
(`gpu3d_vert2_t`, 88 bytes) con un sombreador de vértices v2 fijo; al volver a un dibujo v1 se restauran el SPI
(3 parámetros), los sombreadores de v1, la prueba alfa y el orden del Z.

**Cómo leerlo en el log** (al encender, tras la autoprueba común): `[GPU3D] r600 v2: pruebas: pedidas 0x..., superadas
0x... (faltan 0x...); 4 unidades; formatos 0xf` y, por cada prueba que falla, `[GPU3D] r600 v2: prueba N (<qué>) MAL:
pixel (x,y) esperado ... obtenido ...` (las pruebas: 0 base, 1 combinar, 2 DOT3, 3 alfa, 4 esténcil y LIMPAR2, 5 tijera y
máscara, 6 funciones del Z, 7 mezcla, 8 lógica, 9 niebla, 10 espejo, 11 DXT, 12 NPOT, 13 anisótropo, 14 destino textura, 15 mipmaps).
`gpu3d probar --detalle` añade `r600 detalle (...): v2 (D426): capacidades ...; sombreadores de pixeles: N generados, M de
la cache`. Si la prueba 0 falla, no hay v2 y todo sigue como en la v8.5.5.1.

### No se enciende sola

A diferencia de la Intel, **no se prueba con el primer uso**: arranca «apagado» y solo el
administrador la enciende con `gpu3d probar` (o `gpu3d encender`). El inicio carga microcódigo
(`/lib/firmware/radeon/RV710_pfp.bin` y `_me.bin`, de linux-firmware; no entran con
`SEM_FIRMWARE=1`) y reinicia el procesador de órdenes (CP) de una tarjeta en la que esto no se ha
ejecutado nunca, y el 2D de esa misma máquina tiene fallos abiertos (issues #1 y #2). Cuando esté
confirmada, `mkimagem.py --gpu3d=radeon` la deja como la Intel (se prueba con el primer uso). `gpu3d` la
muestra como «apagado (software; «gpu3d probar» o «gpu3d encender» lo enciende)».

### Qué toca y qué no

- **Memoria:** solo VRAM, detrás de todo lo que puede usar el 2D por DMA, alineada a 1 MB: cola de
  órdenes (64 KB), sombreadores y una arena (`g3d_arena_t`) con vértices, destino, Z y texturas (4 a
  16 MB en total). Sin GART, sin bus maestro, sin interrupciones. Antes se comprueba (leyendo) que
  la zona cae en `MC_VM_FB_LOCATION`, en la apertura de la CPU y en `CONFIG_MEMSIZE`, que la memoria
  virtual de la GPU está apagada, que no hay un cursor por hardware allí y que guarda lo escrito.
- **Registros que escribe:** los del CP (`CP_ME_CNTL`, `CP_RB_*`, carga del microcódigo), el
  reinicio **solo del CP** (`GRBM_SOFT_RESET` bit 0), unos 50 valores por defecto del motor 3D
  (`0x8xxx`-`0xAxxx`; la lista sale de `r7_inicio_motor`) y, por la cola, el contexto 3D.
- **No toca:** controladores de pantalla (`0x6xxx`), HDP, relojes, energía, DMA, `SRBM_SOFT_RESET`.
  De la secuencia de inicio de referencia se dejaron fuera a propósito el borrado de las superficies
  del HDP (`0x2C14...`) y `DCP_TILING_CONFIG`/`HDP_TILING_CONFIG`/`DMA_TILING_CONFIG`. Si el 3D no
  funciona, es lo primero que mirar.
- **Si algo no responde:** se para el CP, `[GPU3D] apagado: ...`, y todo sigue por software.
  **Nunca** se reinicia el motor gráfico entero (no se ha probado con la pantalla del VBE encendida):
  si quedó bloqueado puede no volver hasta reiniciar la máquina.
- El 3D (CP) y el 2D (motor DMA) son independientes: que el 2D haya fallado no impide probar el 3D.
  Hace falta que el 2D haya reconocido la tarjeta (no vale una imagen con `--sin-acel2d`); después
  se puede apagar con `acel2d apagar`.

### Qué ejecutar en el HP dv7

Con la imagen normal (lleva el firmware). La primera vez, con el 2D apagado para no mezclar problemas:

```
acel2d apagar
gpu3d
gpu3d probar --detalle
dmesg > /home/gpu3d.txt
```

Si dice **BIEN** (las ocho pruebas), el 3D queda activo:

```
gpu3d
sgldemo -g               (SaguiGL por software contra la GPU, escena a escena)
sgldemo                  (el cubo en una ventana, ya por la GPU)
infopc --bench-gpu
dmesg > /home/gpu3d-2.txt
```

Traer `/home/gpu3d*.txt` (por SSH, D250: `scp`) o una foto. `gpu3d apagar` lo para.

Si `gpu3d` dice «sin GPU con driver 3D» en el dv7, la imagen es anterior a la D357 (v8.5.2): el estado se
perdía al fusionar y el 3D no se podía encender.

### Cómo leer el log

Desde la D357 **la propia salida de `gpu3d probar` dice la etapa**: si el encendido falla, el motivo empieza
por `etapa N (nombre): ...` (`etapa 0 (sonda de solo lectura)` si la tarjeta no pasó las comprobaciones sin
tocar nada), y con `--detalle` el log lleva `[GPU3D] r600 detalle (...): ultima etapa del encendido: N de 6`.
Las etapas: 0 sonda, 1 leer el microcódigo, 2 parar el CP y cargarlo, 3 valores por defecto del motor, 4 cola
y arranque del CP, 5 prueba de la cola, 6 sombreadores y estado base. Si el encendido pasa, lo que falle es de
la autoprueba común (`prueba N`, 0 a 7). En el log, `[GPU3D] r600: etapa N` es el último paso que terminó.

| Última línea | Qué significa | Dónde mirar |
| --- | --- | --- |
| `no se pudo iniciar el 3D: el modelo ... no esta en la lista` u otro motivo de la sonda | La sonda (solo lectura) rechazó la tarjeta | El motivo: modelo, controlador de pantalla, VRAM, cursor |
| `falta el microcodigo` | No está `/lib/firmware/radeon/RV710_*.bin` | Imagen `sem-firmware` |
| `la VRAM de la zona 3D no guarda lo escrito` | La zona elegida no es memoria utilizable | `zona 3D: ... en MC ...`, `CONFIG_MEMSIZE`, apertura |
| `etapa 2` y nada más | El CP no arranca tras cargar el microcódigo | `CP_STAT`, `CP_ME_CNTL` del detalle |
| `el CP no ejecuta la prueba de la cola` | El CP no lee la cola de la VRAM | `RPTR` (¿avanza?), `CP_RB_BASE`, vaciado del HDP |
| `el CP no termina el estado base` | Un paquete del estado base lo bloquea | `GRBM_STATUS`, `RPTR` (en qué palabra se quedó) |
| autoprueba: prueba 0 (limpiar) | No dibuja, o dibuja otra cosa | Formato del destino, sombreadores, vaciado de cachés antes de leer |
| prueba 1 o 5 (rectángulo, color interpolado) | Borrar va; fallan los vértices o la interpolación | Lectura de vértices del sombreador, `SPI_*` |
| prueba 2 o 7 (Z) | El búfer Z (mosaico 1D) | `DB_DEPTH_INFO`, `GB_TILING_CONFIG` de la etapa 3 |
| prueba 3 (mezcla) | `CB_BLEND*`, `CB_COLOR_CONTROL` | |
| prueba 4 (textura) | Recurso de textura o muestreador | Modo lineal alineado, orden de los canales |
| prueba 6 (especular) | El tercer parámetro o la suma del sombreador | `SPI_VS_OUT_ID_0`, sombreador de color |

### Qué no hace todavía

Las integradas RS780/RS880; destinos de más de 1024x1024 y texturas de más de 1024x1024 (la SDL 2 por GPU y
SaguiGL siguen por software en esos casos); recuperarse de un bloqueo del motor. Presentar sin pasar por la RAM: desde
la D432, a pantalla completa (sin probar en hardware; SaguiGL en ventana sigue leyendo el cuadro).

## Radeon Evergreen/NI (HD 5000-6000, HD 7500M/7600M: Cedar…Hemlock, Barts, Turks, Caicos) — D427

`kernel/dispositivos/gpu3d_evergreen.c` + `evergreen_3d_pacotes.c` / `kernel/inc/evergreen_3d.h` (paquetes PM4, tres
sombreadores a mano en el ISA de Evergreen y valores de inicio: lógica pura, probada en el PC con
`tests/evergreen_3d_unidad.c`) + la sonda de solo lectura del 2D (`r600_sondar`, D286/D408). **Cayman (HD 6900) no**:
otra arquitectura de sombreadores (VLIW4); se queda en software.

Mismo esquema que las R7xx (sección anterior): el CP con su microcódigo (`/lib/firmware/radeon/<FAMILIA>_pfp.bin` y
`_me.bin`, de linux-firmware) lee la cola en la VRAM; un sombreador de vértices que copia y dos de píxeles (color y
textura) que interpolan los parámetros con `INTERP_XY/ZW`; el cuadro vuelve a la RAM leído por la CPU. Lo propio:

- **VRAM por encima de 4 GB en el MC (D408):** la zona usa las mismas direcciones de 40 bits que el 2D (en el Toshiba,
  `f:xxxxxxxx`); el log las escribe como `alto:bajo`.
- **Zona 3D** detrás del 2D, de las superficies de los otros controladores (hasta 6) y de los cursores, dentro de los
  **primeros 64 MB del BAR 0** (sin registro del tamaño de la apertura, D286): de 4 a unos 50 MB.
- **No se enciende sola** (como las R7xx): `gpu3d` la muestra «apagado»; `gpu3d probar` / `gpu3d encender` la ponen en
  marcha con la autoprueba; `gpu3d apagar` para el CP. `--gpu3d=radeon` al grabar la imagen la deja «sin probar» (se
  prueba con el primer programa que abra el motor). «gtt» no hace nada aquí (sin GART en Evergreen todavía).
- **Registros:** los del CP, `GRBM_SOFT_RESET` del CP y de los bloques del 3D que dependen de él (PA, SPI, SH, SX,
  VGT), unos 40 valores por defecto del motor (`eg3d_inicio_motor`, de `evergreen_gpu_init`) y el contexto por la cola.
  **No toca** pantalla, DMIF, HDP (salvo su vaciado), MC, relojes, energía, DMA ni `SRBM_SOFT_RESET`; `GB_ADDR_CONFIG` y
  `GB_BACKEND_MAP` se dejan como los dejó la BIOS (solo se escriben si están a cero).

### Cómo leer el log

`[GPU3D] evergreen: etapa N` es el último paso que terminó; si el encendido falla, `gpu3d probar` dice `etapa N
(nombre): motivo`. Etapas: 0 sonda, 1 microcódigo y VRAM de la zona, 2 parar el CP y cargarlo, 3 valores por defecto
del motor, 4 cola y arranque del CP, 5 prueba de la cola, 6 sombreadores y estado base.

| Última línea | Qué significa | Dónde mirar |
| --- | --- | --- |
| `etapa 0 (sonda de solo lectura): ...` | la sonda del 2D o la zona rechazaron la tarjeta (modelo, pantalla, VRAM, cursor, Cayman) | el motivo y las líneas `[GPU3D] evergreen: controlador N` |
| `etapa 1: falta el microcodigo` | no está `/lib/firmware/radeon/TURKS_*.bin` (imagen de antes de la D427 o `sem-firmware`) | copiar los dos ficheros |
| `etapa 1: la VRAM ... no guarda lo escrito` | la zona no es memoria utilizable por la CPU | `zona 3D: ... en MC`, `CONFIG_MEMSIZE` |
| `etapa 3` con `secuenciador del MC PARADO` | la BIOS no dejó el MC en marcha (raro): el 3D no puede ir | `MC_SEQ_SUP_CNTL` |
| `etapa 4` y nada más / `etapa 5: el CP no ejecuta la prueba de la cola` | el CP no lee la cola de la VRAM o el microcódigo no arranca | `RPTR` (¿avanza?), `CP_RB_BASE`, `CP_STAT` |
| `etapa 6: el CP no termina el estado base` | un paquete del estado base (preámbulo, `CLEAR_STATE`, SQ) lo bloquea | `RPTR` (en qué palabra), `GRBM_STATUS` |
| autoprueba: prueba 0 o 1 | no salen triángulos o salen mal: sombreadores, `SPI_*`, formato del destino | `--ver` (D390), `CB_COLOR0_INFO` |
| prueba 2 o 7 (Z) | el Z en mosaico 1D (`DB_Z_INFO`, `DB_DEPTH_SIZE`) | `GB_ADDR_CONFIG` del detalle |
| prueba 3 (mezcla) | `CB_BLEND0_CONTROL` (bit 30) | |
| prueba 4 (textura) | recurso o muestreador (palabras 4 y 7, `NUM_BANKS`) | paso alineado del detalle |
| prueba 5 o 6 (interpolado, especular) | `INTERP_XY/ZW`, `SPI_BARYC_CNTL`, semánticas | |

### Qué probar en el Toshiba (HD 7600M Turks `1002:6840`)

Con una imagen de la v8.5.5.2 o posterior (lleva el microcódigo `TURKS_*`), con el escritorio:

```
gpu3d                      (debe decir «apagado» y «Radeon Evergreen/NI 1002:6840»)
gpu3d probar --detalle     (las 6 etapas y las 8 pruebas)
dmesg > /home/evg3d.txt
```

Si dice **BIEN**: `sgldemo -g` (SaguiGL por software contra la GPU, escena a escena: debe decir BIEN), `sgldemo` (el cubo
en ventana: el título dice `(GPU)` y los fps), `sgldemo -n 300 -s` (fps sin pausas, GPU) y `gpu3d apagar; sgldemo -n 300
-s` (lo mismo por software), `infopc --bench-gpu`, y otro `dmesg > /home/evg3d-2.txt`. Mover la ventana del cubo y abrir
el menú mientras gira (el 2D por DMA sigue a la vez). `gpu3d apagar` y `gpu3d encender` (tiene que volver a encender sin
reiniciar). Reiniciar con normalidad al final.

Si algo sale mal: la línea `etapa N` o `prueba N` y el `dmesg`; `gpu3d apagar` deja todo como estaba (la pantalla no se
toca). Si la máquina se queda colgada (no debería: solo se toca el CP), reiniciar: nada queda grabado.

### Qué no hace todavía

Cayman; GART (el cuadro vuelve por la CPU, que es lo lento: ~10 MB/s por el PCIe en las R7xx); destinos y texturas de
más de 1024x1024; plantilla, varias texturas y el resto de la interfaz v2 (fase B de la D427).

## Radeon R5xx (X1300 a X1950; la Mobility X1400 del Amilo) (D287)

`kernel/dispositivos/gpu3d_r500.c` y `kernel/dispositivos/r500_pacotes.h`. **Sin probar en hardware.**

- **No se enciende solo.** Al arrancar la capa dice `[GPU3D] radeon-r500: Radeon R5xx 1002:7145: 3D disponible
  pero apagado…` y `gpu3d` lo muestra «apagado». Se enciende con `gpu3d encender` o `gpu3d probar` (los dos
  hacen la autoprueba), o con una imagen hecha con `--gpu3d=radeon` (entonces se prueba con el primer programa
  que abra el motor, como en Intel). Si el 2D de la tarjeta falló o lo apagó su vigilante, no se enciende.
- **Cómo dibuja:** por el procesador de órdenes (CP) con el microcódigo `/lib/firmware/radeon/R520_cp.bin`
  (no está con `SEM_FIRMWARE=1`). La cola (64 KB), el destino, el Z y las texturas van en una zona de hasta
  16 MB de la VRAM detrás de la pantalla; **sin bus maestro**: la tarjeta no toca la memoria del sistema. Los
  vértices van dentro de las órdenes; un programa de vértices que copia y dos programas de fragmentos
  (`color + especular` y `color * textura + especular`).
- **Convive con el 2D por MMIO** (`acel_radeon.c`): el CP solo trabaja mientras la capa espera a la valla.
- **Si algo falla** se para el CP y se sigue por software; el motor no se reinicia nunca.

Desde la D357, si el encendido falla, `gpu3d probar` dice `etapa N (nombre): ...` en su propia salida, sin
mirar el log; `--detalle` añade `[GPU3D] r500 ...: ultima etapa del encendido: N de 6`.

### gpu3d v2 en R5xx (D426; probado en la X1400 del Amilo el 2026-11-20)

**La regresión de la X1400 y su arreglo** (D426, «Amilo»): con el kernel de la v8.5.5.2, la autoprueba de v1 salía
bien, la prueba v2 0 (una unidad) también, y en la prueba 1 (combinar, cuatro unidades) el motor se quedaba ocupado
para siempre (`RBBM_STATUS 9401c100`, GA ocupado; RPTR fijo; ninguna valla). Después fallaban la medida y el 2D («la
FIFO no se vacia», GART aparcada). Causa: al cambiar el número de juegos de coordenadas, `r5_estado_vertices` mandaba
`WAIT_UNTIL` con «3D parado **y limpio**» sin pedir antes el vaciado de las cachés de color y de Z; tras un dibujo la
espera no termina nunca. Ahora va `r5_vaciar` delante (vaciado y espera, como la valla), y `r500_3d_unidad` lo vigila.

**Qué anuncia la X1400** (lo que pasa las pruebas en la máquina): v2 con **una unidad**, prueba alfa, tijera,
máscaras, 8 funciones de Z, mezcla ampliada/separada/resta, operación lógica, espejo, anisótropo, dibujar en una
textura y DXT1/3/5 (`capacidades 0xa2fe9`, formatos `0xf`). **No**: COMBINAR y DOT3 con varias unidades (la prueba 1
da negro en (4,4): sin cuelgue, pero mal), niebla (la prueba 9 sale sin niebla), y **esténcil y LIMPAR2**, que se
quitaron de los candidatos: pasan la prueba pequeña pero en la escena «sombra» de `es11demo` la X1400 dejaba rayas
negras en el fondo y la sombra con huecos. Todo eso va por software, como antes de v2.

**Ajustes de depuración** en `/sistema/conf/gpu3d-r500.conf`, línea `inicio 4ff0 N` (no es un registro: no se
escribe; N en hexadecimal, suma de): `1` sin v2; `2` cada parte del estado de un dibujo v2 (espera y `PVS_FLUSH`, PSC,
programa de vértices, formato, RS, destino, unidades, constantes, programa del US, fragmento, triángulos) y el primer
dibujo v1 tras uno v2, cada uno con su valla y una línea `r500 v2 sinc: <parte>: bien|CUELGA`; `4` solo las pruebas de
una unidad; `8` los semáforos de textura en todas las lecturas (como el primer generador; ahora van como en Mesa, solo
en la última); `20` pedir también esténcil y LIMPAR2. Al agotarse el tiempo, el log lleva la cola alrededor del RPTR
(`r500 cola ...`). Un cuelgue obliga a reiniciar: el motor no se reinicia nunca.

| Qué | Cómo | Bit |
| --- | --- | --- |
| Hasta 4 unidades con COMBINE y DOT3 | programa del US generado (`r5_fp_gerar`), con las constantes de las unidades en las del US; caché de 8 (se vuelve a cargar en el US solo si cambia) | `COMBINAR`, `DOT3`, `unidades` = 4 |
| Niebla con el factor del vértice | en el programa (alfa del especular; color en la constante 4) | `NEBLINA` |
| Esténcil de 8 bits | el Z de 24 bits ya era `24_8`; `ZB_CNTL`, `ZB_ZSTENCILCNTL`, `ZB_STENCILREFMASK` | `ESTENCIL`, `LIMPAR2` |
| Prueba alfa | fija, en el FG (`FG_ALPHA_FUNC`, valor de 8 bits) | `ALFA_TESTE` |
| Tijera, máscara de color, 8 funciones de Z | `SC_SCISSOR0/1`, `RB3D_COLOR_CHANNEL_MASK`, `ZB_ZSTENCILCNTL` | `TIJERA`, `MASCARAS`, `Z_FUNCS` |
| Mezcla ampliada, separada, resta | `RB3D_CBLEND`/`ABLEND` con `SEPARATE_ALPHA_ENABLE` y `COMB_FCN` | `MEZCLA_EXT/SEP/EQ` |
| Operación lógica | `RB3D_ROPCNTL` | `LOGICA` |
| DXT1/3/5 | `TX_FORMAT_DXT1/3/5` con la mezcla de canales de las R500 (la de ARGB8888), paso de 16 texels | `formatos` |
| Espejo, anisótropo | `TX_FILTER0` (`MIRRORED`, `ANISO`, `MAX_ANISO`) y `R500_TX_MAX_ANISO` | `TEX_ESPELHO`, `TEX_ANISO` |
| Dibujar en una textura | el destino del RB3D sobre el hueco (ARGB, su paso), con un Z propio; `LER` lee la textura | `DESTINO_TEX` |

Va por software: **NPOT con repetición o espejo** (con borde sí; como la limitación de Gen3: el paso propio de la
textura no deja repetir), mipmaps, cubos y coordenadas proyectivas. El vértice v2 va en la orden con 6 + 2 palabras por
juego de coordenadas usado (con uno es el de v1); al cambiar de número de juegos se vuelve a cargar el programa de
vértices (con el motor parado), y al volver a v1 se restaura lo que v1 no pone (mezcla del alfa, ROP, prueba alfa).

**En el log:** igual que en R7xx, con `r500` (`[GPU3D] r500 v2: pruebas: ...`, `prueba N (...) MAL ...`) y en el
detalle `r500 ...: v2 (D426): capacidades ...; programas: N generados, M de la cache, K cargas`.

### Qué probar en el Amilo (para el mantenedor)

Con la imagen normal (el 3D queda apagado; nada cambia hasta que se pida):

1. `dmesg | grep GPU3D` → tiene que decir `radeon-r500: Radeon R5xx 1002:7145: 3D disponible pero apagado`.
2. `acel2d` → el 2D debe estar activo («radeon») como hasta ahora. Si está en la CPU por un fallo, parar aquí.
3. `gpu3d probar --detalle` y después `dmesg > /home/r500.txt`. En el `dmesg` salen las **seis etapas**
   (`[GPU3D] r500: etapa 1` … `etapa 6`), la autoprueba (ocho pruebas) y la medida. Qué significa dónde se pare:

   | Se para en | Qué quiere decir | Qué mandar |
   | --- | --- | --- |
   | antes de la etapa 1, con un motivo | modelo fuera de la lista, memoria rara, pantalla en otro sitio, cursor en la zona | la línea del motivo y la `etapa 1` |
   | etapa 1, «no caben 2 MB ... detrás de la pantalla y de las superficies activas» | no hay hueco detrás de lo que muestran D1, D2 y los cursores (D386; antes, con D2 activo, «la superficie de D2 ... está cerca de la zona 3D») | todas las líneas `etapa 1` (dicen CRTC, fin, ventana y KB de cada controlador) |
   | etapa 2 | falta el microcódigo o la VRAM de la zona no guarda lo escrito | el motivo |
   | etapa 4 («el puntero de lectura no quedó a cero») | el CP no aceptó la cola | las líneas `r500 al iniciar` |
   | **etapa 5 («el CP no ejecuta la prueba de la cola»)** | **el CP no lee la cola desde la VRAM** (la hipótesis de la D287, punto 3) o el microcódigo no arranca | las líneas `r500 prueba de la cola` (RPTR, WPTR, RBBM_STATUS, CP_CSQ_STAT) |
   | etapa 6 («no termina el estado fijo») | alguna orden del estado 3D cuelga el motor | las líneas `r500 estado fijo` |
   | autoprueba, prueba 0 (limpiar) | no llegan triángulos al destino (programa de vértices, formato del vértice) o no se lee donde se dibuja | la línea `autoprueba: FALLO` con el píxel y el `detalle` |
   | prueba 2 / 7 (Z) | formato o paso del Z | lo mismo |
   | prueba 4 (textura) | orden de canales, orientación o paso de la textura | lo mismo |
   | prueba 5 / 6 (interpolado, especular) | colores del vértice (orden D3D) o el segundo color | lo mismo |

4. Después de la prueba (vaya bien o mal): mover una ventana y abrir el menú. **El escritorio tiene que seguir
   igual de fluido** (el 2D comparte el motor). `acel2d` debe seguir diciendo «radeon». Si el 2D se apagó solo
   tras la prueba 3D, decirlo: es que el motor quedó ocupado.
5. Si la autoprueba fue bien: `gpu3d` (estado «activo», puntuación), `sgldemo -g` (SaguiGL por software contra
   la GPU en cuatro escenas: dice cuántos píxeles difieren), `sgldemo` (el cubo en ventana, ya por la GPU) y
   la pestaña **GPU** de Información del PC (puntuación 3D). Una foto del cubo sirve para ver bordes y texturas.
6. `gpu3d apagar` → todo vuelve al software; `gpu3d encender` → repite el encendido (tiene que funcionar una
   segunda vez sin reiniciar).
7. Reiniciar y apagar la máquina con normalidad (el 3D no debe dejar nada colgado).

Cosas a mirar si funciona: bordes de los triángulos comparados con el software (regla de relleno y centro del
píxel), texturas no potencia de 2 con repetición (van por software), y la velocidad de `sgldemo` (traer el
cuadro de la VRAM a la RAM es lento: es lo siguiente a mejorar).

### Qué probar de gpu3d v2 (D426) en el Amilo (X1400) y en el HP dv7 (HD 4500)

Con un kernel de esta rama (la raíz de la máquina puede quedarse: SaguiGL de antes no usa v2 y sigue igual):

1. `gpu3d probar --detalle` y `dmesg > /home/v2.txt`. Lo que importa: la línea `v2: pruebas: pedidas ..., superadas
   ...` y las de `prueba N (...) MAL`. La autoprueba de v1 (las 8 de siempre) tiene que seguir BIEN: en R7xx el Z de 24
   bits cambió a `DEPTH_8_24` (con esténcil) también para v1.
2. `gpu3d` (bits en `infopc --informe` y la pestaña GPU cuando SaguiGL ES 1.1 lo enseñe) y, con la SaguiGL de la rama
   `v8.5.5.2-sgl-es11`, sus demos de ES 1.1 (`sgldemo -g`, la escena de multitextura) por GPU y por software.
3. Mover ventanas mientras gira: el 2D sigue por su camino (R5xx por el anillo del CP, R7xx por DMA).

Si algo cuelga la GPU durante las pruebas v2, el vigilante apaga el 3D y todo sigue por software (como con v1).

## Qué probar en cada máquina Radeon (resumen, D357)

| Máquina | Tarjeta | Qué ejecutar | Qué mandar |
| --- | --- | --- | --- |
| **HP dv7** | HD 4530/4570 `1002:9553` (RV710, R7xx) | `acel2d probar --detalle`; `acel2d apagar`; `gpu3d`; `gpu3d probar --detalle`; si BIEN: `sgldemo -g`, `sgldemo`, `infopc --bench-gpu`; `sdl2gpu` (D288) | `dmesg > /home/dv7-radeon.txt` y la salida de las órdenes (o foto) |
| **Fujitsu Amilo** | Mobility X1400 `1002:7145` (R5xx) | `acel2d`; `gpu3d probar --detalle`; la lista de «Qué probar en el Amilo» | `dmesg > /home/r500.txt` |
| **Toshiba L635** | Mobility HD 5000 `1002:68e0` (Cedar, Evergreen) | 2D (D286): `dmesg > /home/evg.txt`; `acel2d`; `acel2d probar --detalle`. 3D (D427): `gpu3d probar --detalle` y, si BIEN, `sgldemo -g` | `/home/evg.txt` y la salida de `acel2d probar --detalle` y `gpu3d probar --detalle` |
| **Toshiba (HD 7600M)** | Turks `1002:6840` (NI, VRAM del MC en 0xF_0000_0000) | la lista de «Qué probar en el Toshiba» de la sección Evergreen/NI | `dmesg > /home/evg3d.txt` y la salida de `gpu3d probar --detalle` |

En las tres: si `acel2d probar` falla al iniciar, el texto empieza por `etapa N:` (1 modelo y sonda, 2 memoria y
zona del DMA, 3 estado previo del DMA, 4 encender el anillo, 5 prueba del anillo, 6 valla); si pasa el inicio y
falla después, es la autoprueba de la capa 2D. Ante cualquier cosa rara: `acel2d apagar`, `gpu3d apagar`, o
arrancar con Mayús pulsado (modo seguro, D323).

## Órdenes y diagnóstico

| Orden | Qué da |
| --- | --- |
| `gpu3d` | estado, driver, motivo, dueño del motor, memoria de GPU, contadores |
| `gpu3d probar` | enciende si hace falta, autoprueba (qué prueba falla y qué píxel) y medida |
| `gpu3d probar --detalle` | además, los registros de la GPU en el log (`[GPU3D]`) y, tras cada etapa, lo que dejó la GPU en el destino (sentinelas `a1b2c3`/`5a5a5a5a`, D390) |
| `gpu3d probar --ver [s] [--guardar]` | además, una ventana con lo que dibujó la tarjeta en cada etapa (tecla o `s` segundos; sin escritorio, `/home/gpu3d-etapaN.bmp`) (D390) |
| `/sistema/conf/gpu3d-<driver>.conf` | ajustes de registros para depurar (`inicio`/`dibujo REG VALOR`), solo en los rangos del driver (D386, D390; `docs/gpus.md`) |
| `gpu3d apagar` / `gpu3d encender [anillo] [gtt]` | software / volver a encender y probar (por lotes o en el anillo; `gtt`: RAM del sistema en el GTT, D358) |
| `sgldemo -g [N]` | las escenas de SaguiGL por software y por la GPU: píxeles que se apartan y fps de cada camino |
| `sgldemo` | el cubo en una ventana; el título de fps dice `(GPU)` o `(CPU)` |
| `sgldemo -n N [-s]` | D391: la ventana del cubo, N cuadros y se cierra: ms, fps reales, camino y suma del último cuadro (`-s`: sin ninguna pausa) |
| `acel2d vuelta [apagar\|encender\|medir]` | D391: el cuadro vuelve a la RAM por la GPU (R5xx con PCI Express): estado, cuadros por GPU y por CPU, medida |
| `infopc` (pestaña GPU) e `infopc --informe` | estado del 3D y última puntuación 3D |
| `infopc --bench-gpu` | añade la medida 3D a la 2D y la guarda en `/home/saguimark-gpu-*.txt` |

Líneas del log (`dmesg`), todas con `[GPU3D]`: elección al arrancar, encendido, zona de memoria,
cada destino creado, resultado de la autoprueba (pruebas hechas y superadas en hexadecimal), la
medida, `apagado: <motivo>` y, con un fallo o `--detalle`, los registros del ring y de error
(`IPEIR`, `IPEHR`, `INSTDONE`, `ACTHD`, `EIR`, `ESR`, `INSTPM`, `PGTBL_ER`).

**Puntuación 3D** (provisional): `triángulos/s ÷ 200 + kpíxeles/s ÷ 200`; 100 000 triángulos
pequeños por segundo y 100 Mpíxeles/s con mezcla dan 1000 puntos. Se ajustará con medidas reales.

## Qué probar en una máquina con 945/G33 (para el mantenedor)

Imagen normal (sin opciones). Con el administrador, en un terminal, y guardando el log al final:

1. `acel2d` — anotar si el 2D de Intel quedó activo, en la CPU por lento, o falló. (Si falló, el 3D
   no se intenta: mandar el `dmesg` igualmente.)
2. `gpu3d` — debe decir «sin probar» y `Intel Gen3 8086:xxxx`.
3. `gpu3d probar --detalle` — lo importante. Resultado posible:
   - **BIEN**: anotar triángulos/s, kpix/s, lo que tarda en traer el cuadro y los puntos.
   - **MAL en una prueba**: anotar cuál (0 a 7) y el píxel (`esperado`/`obtenido`): dice qué pieza
     falla (1 = estado o programa de color; 2 = Z; 3 = mezcla; 4 = textura; 7 = Z de 16 bits).
   - **Tiempo agotado**: la GPU se quedó con una orden. El 2D también pasará a la CPU (comparten
     ring) y el escritorio sigue. Anotar las líneas `[GPU3D] intel tras el fallo`.
4. Si falló: reiniciar y probar `gpu3d encender anillo` y luego `gpu3d probar --detalle` (el otro
   camino de las órdenes). Anotar si cambia.
5. Si fue bien: `sgldemo -g` (compara con SaguiGL por software: «BIEN» en las cuatro escenas y los
   fps de cada camino) y `sgldemo` (el cubo en ventana: debe verse igual que siempre y decir `(GPU)`).
   Mover la ventana y abrir otras mientras gira: el 2D y el 3D comparten el ring.
6. `infopc --bench-gpu` y la pestaña GPU de Información del PC.
7. `dmesg > /home/gpu3d.txt` y copiar el archivo (o foto de `dmesg | grep GPU3D`). La última línea
   `[GPU3D] intel: etapa N` dice hasta dónde llegó el encendido (tabla «Encendido por etapas»).
8. **RAM en el GTT (D358), solo si lo anterior fue BIEN**: `gpu3d encender gtt`, después
   `gpu3d probar --detalle` y `gpu3d`. Anotar las líneas `etapa 6` (ventana, entradas «todas
   iguales: libres», «la CPU ve las N paginas», «la GPU escribe en la RAM», texturas sí/no), si la
   autoprueba sigue BIEN y el tiempo de «traer N píxeles a la RAM» comparado con el del paso 3. Luego
   `sgldemo -g` y `sgldemo` otra vez (mover la ventana del cubo). `dmesg > /home/gpu3d-gtt.txt`.
   Si la máquina hace algo raro tras esto (programas que fallan, archivos dañados), reiniciar, **no**
   volver a usar `gtt` en ella y decirlo: es la parte que escribe en el GTT.

Si la máquina no arranca bien o la pantalla se estropea al probar (no debería: no se toca la
pantalla), arrancar con `--gpu3d=no` y decirlo.

## Pruebas automáticas

- `tests/gen3_3d_unidad.c` (en el PC): órdenes de Gen3 recorridas como lo haría la GPU, los dos
  programas de píxeles contra sus palabras escritas a mano, tamaño del vértice declarado, memoria
  de GPU y coma flotante con enteros.
- `tests/h285_gpu3d_intel.py` (QEMU): sin GPU todo sigue por software; con `--gpu3d=simulado` (un
  driver de mentira por la CPU, `gpu3d_sim.c`) se prueba el camino entero: autoprueba, `sgldemo -g`
  contra el software, dueño del motor, fallo de píxel y vigilante simulados, `gpu3d apagar/encender`.
- `tests/gen3_gtt_unidad.c` (en el PC, D358): memoria robada de `GMCH_CTRL`, ventana del GTT, entradas,
  un GTT de BIOS simulado (la ventana nunca pisa lo usado) y la copia del blitter.
- `tests/h360_gpu3d_intel_gtt.py` (D358): las dos pruebas de unidad, las reglas del driver (etapas, el GTT
  solo en la ventana comprobada, páginas nunca liberadas, EIR, invalidar cachés) y, en QEMU con el driver
  simulado, `gpu3d encender gtt` / `anillo gtt` e imágenes `--gpu3d-gtt`.

`tests/h287_gpu3d_r500.py` (Radeon R5xx, D287): `unidad` (`tests/r500_3d_unidad.c`: órdenes del CP, formato del
vértice, programa de vértices y programas de fragmentos contra sus palabras a mano; y `tests/gpu3d_diag_unidad.c`, el
diagnóstico común de la D390), `fuentes`, `firmware` (el microcódigo en la imagen) y `std` (QEMU sin Radeon, con y sin
`--gpu3d=radeon`: nada cambia).

`tests/h432_presentar_gpu.py` (D432): `unidad` (`tests/g3d_apres_unidad.c`: bandas, vértices de la copia y la pantalla
como destino en R5xx; `tests/g3d_apres_r600_unidad.c`: lo mismo en R6xx/R7xx y Evergreen con direcciones de 40 bits, y
que los destinos de siempre no cambian), `fuentes`, `std` (sin GPU nada cambia) y `simulado` (QEMU: el cuadro llega a la
pantalla escalado y con bandas, el compositor no pinta encima, y la recupera con Alt+Tab, un aviso, al salir y con
`gpu3d apagar`; en ventana, con la opción apagada o con un driver que no puede, como antes).

`tests/h405_saguigl.py` (D404): unidad del filtro, mipmaps, perspectiva, luces, formatos y OBJ en el anfitrión; sumas del
paisaje en QEMU; medida por software; con el simulado, `sgldemo -g` (siete escenas) y la caché de texturas.

`tests/h285_gpu3d_intel.py diag` (D390): con el driver simulado, `gpu3d probar --ver` en una ventana que avanza sola, el
análisis por etapas en el log y los ajustes de `gpu3d-simulado.conf` (una GPU que no escribe deja `a1b2c3`; un registro
fuera de rango no se aplica; `--guardar` deja los BMP).

## SaguiGL realista: filtro, mipmaps, luces y caché de texturas (D404)

Para probar el 3D de las tarjetas con algo más parecido a un juego, la SaguiGL (`userland/libsgl`) tiene una ruta
«realista» que solo se usa cuando el programa la pide (filtro distinto de `GL_NEAREST`, `glHint(GL_PERSPECTIVE_
CORRECTION_HINT, GL_NICEST)`, normales, `glMaterial`, `glEnable(GL_LIGHTn)` o `glFogf(GL_FOG_MODE, ...)`): los programas
de antes dan los mismos píxeles (lo comprueba `h405 unidad` contra el `sgl.c` anterior).

| | Software | GPU (capa `gpu3d`) |
|---|---|---|
| Perspectiva de texturas y colores | sí (q = 1/w por píxel) | sí (el rasterizador con `rhw`; el simulado también desde la D404) |
| Bilineal (`GL_LINEAR`) | sí | `G3D_TEX_LINEAR` (R5xx: `TX_MAG/MIN_LINEAR`; simulado: desde la D404) |
| Mipmaps (`*_MIPMAP_*`, nivel por píxel, trilineal) | sí; niveles generados (caja 2x2) o dados | **no**: se sube el nivel 0 (o uno menor si no cabe) |
| Luz por vértice (8 luces, ambiente, difusa, especular, atenuación, materiales) | sí | sí (la calcula la CPU: color + especular del vértice) |
| Niebla por distancia (`GL_LINEAR`, `GL_EXP`, `GL_EXP2`) | por vértice, con perspectiva | por vértice (en el especular, como la de v6) |
| Formatos | RGBA, RGB, L, LA, A de 8 bits, RGB565, RGBA4444, RGBA5551 (guardados en RGBA8) | ARGB8888 |
| Texturas | hasta 2048x2048, 512 por contexto | 32 huecos por dueño (8 con un kernel de antes) |

**Caché de texturas en la GPU.** Cada hueco guarda qué textura tiene y su *generación* (cambia con `glTexImage2D` o al
borrarla): una textura se sube una sola vez y se reutiliza en los cuadros siguientes; si no hay hueco, se reemplaza el que
lleva más tiempo sin usarse; si la GPU contesta que no cabe, se sueltan otras y, al final, se sube un nivel de mipmap más
pequeño. La biblioteca pregunta cuántos huecos hay subiendo una textura de 1x1 al hueco 31 (un kernel de antes contesta
error sin apagar nada y se usan 8). `GPU3D_TEXTURAS` pasó de 8 a 32 en un commit aparte (la SDL 2 sigue con 8).

**Lo que necesitaría el driver (descrito, no hecho):** mipmaps en la GPU. La ABI se ampliaría por el final con
`GPU3D_OP_TEXTURA` + `c` = alto | (niveles << 16) y los niveles seguidos en `ptr`; en R5xx, `TX_FORMAT0` lleva el número
de niveles (bits 26-29 en `r300_reg.h`) y `TX_FILTER0` el filtro entre niveles (`MIP_NEAREST`/`MIP_LINEAR`, bits 13-14),
con los niveles uno detrás de otro en la VRAM y **sin** `TX_PASSO_PROPRIO` (el paso propio no vale con mipmaps), así que el
ancho tendría que ser potencia de 2 o múltiplo de lo que pida el muestreador; en R6xx/R7xx, `SQ_TEX_RESOURCE_WORD4/5`
(`BASE_LEVEL`, `LAST_LEVEL`) y `MIP_ADDRESS` en la palabra 3, y el filtro en `SQ_TEX_SAMPLER_WORD0` (`MIP_FILTER`). Hasta
entonces, `sgldemo -g` compara el paisaje sin mipmaps.

**Escena y medida** (`sgldemo`, todo generado por código):
- `sgldemo -c paisaje` / `-c avion`: sumas (cuadros 0 y 40, 160x120) que `h405 somas` compara con el anfitrión.
- `sgldemo -p [N]`: el paisaje en una ventana de 640x480 (terreno de 64x64 celdas con hierba, roca y arena mezcladas,
  agua con olas, unos 50 árboles, rocas, casas, nubes, niebla EXP2, sol y luz del cielo, un avión que vuela) durante N
  cuadros, y al final: fps, triángulos por cuadro y por segundo, texturas subidas (KB y por cuadro), reutilizadas, huecos
  y memoria de la GPU usada. `--texturas MB` añade MB de texturas de 256x256 distintas en cada cuadro (carteles en corro):
  con más de las que caben en los huecos, mide la subida por cuadro. `--mem` sin ventana, `--sw` por software, `--sin-mip`,
  `--detalle N`, `--avion` (cámara detrás del avión), `--modelo F.obj --giro G` (un OBJ con su MTL y texturas).
- `sgldemo -g`: además de las cuatro escenas de antes, `bilineal`, `luces` y `paisaje` (sin mipmaps).

**Qué probar en el Amilo (X1400) y el HP (HD 4500)**, con `sgldemo` de `/home/aanun/SO/private/saguigl/` (no hace falta
otro kernel; con el de antes salen 8 huecos): `sgldemo -g 30`, `sgldemo -p 300` (con escritorio), `sgldemo -p 300 --sw`
para comparar, y `sgldemo -p 300 --texturas 16`, `--texturas 64` para ver cuándo deja de caber en la VRAM.

## Pendiente

- Probarlo en hardware (todo lo anterior).
- ~~Mapear RAM normal en el GTT~~: hecho en la D358, detrás de la opción `gtt`; si va bien en las
  máquinas, que sea lo normal.
- ~~Renderizador de la SDL 2 por GPU~~: D288 (sobre la misma capa, vale para Intel). Varias pantallas
  (mejora 060), pendiente.
- Intel: destino y Z con mosaico (Y) si el Z lineal diera problemas en alguna máquina; presentar sin
  copiar el cuadro (la operación de la D432 solo la tienen las Radeon y el simulado).
- D432: probar `APRESENTAR` en las tres Radeon; doble búfer con cambio de base del CRTC (sin desgarro); presentar en
  ventana (hoy solo a pantalla completa); que SaguiGL/EGL la use; subida parcial de texturas para los programas que
  pintan en la superficie de la ventana; que la captura de pantalla vea el cuadro de la GPU.
- Gen4 a Gen8 (X3100, GM45, HD Graphics hasta Broadwell): otra tubería. Gen9: D438 (`gpu3d_intel9`); Gen11 y Gen12, después.


## 2D por la GPU sin ser administrador (D346)

La 2D acelerada (`acel2d`, solo copiar y rellenar rectángulos) se usa al arrastrar ventanas y al desplazar la consola; el resto es software.
El estado y el reparto GPU/software (contadores en `INFO_ACEL2D`) los ve cualquier usuario: `acel2d`, `infopc` (sección GPU) y
Configuración (Pantalla > Aceleración gráfica). `infopc --bench-gpu` mide fuera de pantalla sin permisos; `--admin` (o el botón
«Medir con la GPU») añade la autoprueba del acelerador, que sigue siendo del administrador igual que `acel2d probar|apagar|encender`.
Detalle en `docs/decisoes.md` (D346).
