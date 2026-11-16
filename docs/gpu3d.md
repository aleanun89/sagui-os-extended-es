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
| SaguiGL | `userland/libsgl/sgl.c` | matrices, luces, niebla, recorte y descarte de caras con la CPU; triángulos en coordenadas de la ventana (`gpu3d_vert_t`) |
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
| Drivers (`gpu3d_intel.c`, `gpu3d_sim.c`; después R6xx/R7xx y R5xx) | `gpu3d_driver_t` y las piezas `g3d_*` de `kernel/inc/gpu3d.h` |
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

### Cómo añadir un driver (como se hizo con las Radeon R7xx, D284)

1. `kernel/dispositivos/gpu3d_<familia>.c` con un `const gpu3d_driver_t gpu3d_<familia>` (las diez
   funciones de `gpu3d.h`: `iniciar`, `alvo`, `limpar`, `textura`, `desenhar`, `esperar`, `ler`,
   `desligar`, `detalle`, `memoria`), declarado en `gpu3d.h`.
2. Una línea en `gpu3d_iniciar()` (`kernel/gui/gpu3d.c`) que lo elija para su tarjeta (la da
   `acel2d_placa()`: la que tiene la pantalla, con su familia y generación).
3. Devolver `G3D_NAO` para lo que no cabe o no se soporta y `G3D_FALHA` solo si la GPU no responde.
4. Nada más: la autoprueba, el vigilante, `gpu3d`, `infopc` y SaguiGL ya lo usan.

5. Probarlo en QEMU sin la tarjeta: `tests/h285*.py std` debe seguir pasando (la capa no toca nada si
   la tarjeta no es la suya), y su propia prueba de unidad de paquetes en el PC, como `gen3_3d_unidad.c`.

La interfaz es deliberadamente mínima (triángulos transformados, un destino, texturas ARGB) para que
cualquier GPU la cumpla; lo que una familia necesite de más (sombreadores de vértices propios, varias
texturas) queda dentro de su driver.

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
- **Memoria**: solo la que el VBE dejó en el GTT (la «robada»), entre la zona de prueba del 2D y el
  ring. **No se escribe el GTT**, ni registros de pantalla, relojes o energía. Con 8 MB robados y
  1024x768 quedan unos 4,5 MB: cabe un destino de 640x480 con Z de 24 bits y alguna textura; uno de
  800x600 solo con Z de 16 bits. Si no cabe, ese programa va por software.
- **Traer el cuadro**: la CPU lee la memoria de vídeo por la apertura (sin caché: lento).
  `gpu3d probar` lo mide aparte; si resulta ser lo que limita, el paso siguiente es mapear RAM
  normal en el GTT (ver «Pendiente»).

Fuentes: documentación de Intel del 3D de Gen3 y las cabeceras de Mesa (MIT), solo valores y
secuencias; código propio (`docs/terceros.md`). Nada del driver i915 de Linux con GPL.

### Qué no hace todavía

- Texturas que se repiten con lados que no son potencia de 2 (Gen3 no sabe): ese programa va por
  software. Filtro bilineal: el driver lo tiene, SaguiGL aún no lo pide.
- Los píxeles no son iguales bit a bit a los del motor por software (bordes, redondeo, luz y niebla
  por vértice): por eso los contextos en memoria (`sgl_criar_contexto_mem`), que usan las pruebas
  de píxeles, siguen por software salvo que se pida la GPU con `sgl_gpu_ligar()`.
- Las líneas van como rectángulos de un píxel; los puntos no están.

## Radeon R7xx (HD 4xxx: RV710, RV730, RV740, RV770) — D284

`kernel/dispositivos/gpu3d_r600.c` + `r600_3d_pacotes.c` / `kernel/inc/r600_3d.h` (paquetes PM4, tres
sombreadores a mano y valores de inicio: lógica pura, probada en el PC) + `r600_sondar`
(`kernel/inc/r600_base.h`, la sonda de solo lectura que comparte con el 2D por DMA de la D223).
**Sin probar en hardware.** La máquina es el HP dv7 (`1002:9553`, RV710). Las R6xx (HD 2xxx/3xxx) no
están: otro microcódigo y otro inicio.

### No se enciende sola

A diferencia de la Intel, **no se prueba con el primer uso**: arranca «apagado» y solo el
administrador la enciende con `gpu3d probar` (o `gpu3d encender`). El inicio carga microcódigo
(`/lib/firmware/radeon/RV710_pfp.bin` y `_me.bin`, de linux-firmware; no entran con
`SEM_FIRMWARE=1`) y reinicia el procesador de órdenes (CP) de una tarjeta en la que esto no se ha
ejecutado nunca, y el 2D de esa misma máquina tiene fallos abiertos (issues #1 y #2). Cuando esté
confirmada, `mkimagem.py --gpu3d=radeon` la deja como la Intel (se prueba con el primer uso).

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

### Cómo leer el log

`[GPU3D] r600: etapa N` es el último paso del inicio que terminó; después vienen las pruebas de la
autoprueba común (0 a 7).

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

R6xx; destinos de más de 1024x1024 y texturas de más de 1024x1024; presentar sin pasar por la RAM
(el cuadro se lee por la apertura de la CPU, que es lento); recuperarse de un bloqueo del motor.

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

### Qué probar en el Amilo (para el mantenedor)

Con la imagen normal (el 3D queda apagado; nada cambia hasta que se pida):

1. `dmesg | grep GPU3D` → tiene que decir `radeon-r500: Radeon R5xx 1002:7145: 3D disponible pero apagado`.
2. `acel2d` → el 2D debe estar activo («radeon») como hasta ahora. Si está en la CPU por un fallo, parar aquí.
3. `gpu3d probar --detalle` y después `dmesg > /home/r500.txt`. En el `dmesg` salen las **seis etapas**
   (`[GPU3D] r500: etapa 1` … `etapa 6`), la autoprueba (ocho pruebas) y la medida. Qué significa dónde se pare:

   | Se para en | Qué quiere decir | Qué mandar |
   | --- | --- | --- |
   | antes de la etapa 1, con un motivo | modelo fuera de la lista, memoria rara, pantalla en otro sitio, cursor en la zona | la línea del motivo y la `etapa 1` |
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

## Órdenes y diagnóstico

| Orden | Qué da |
| --- | --- |
| `gpu3d` | estado, driver, motivo, dueño del motor, memoria de GPU, contadores |
| `gpu3d probar` | enciende si hace falta, autoprueba (qué prueba falla y qué píxel) y medida |
| `gpu3d probar --detalle` | además, los registros de la GPU en el log (`[GPU3D]`) |
| `gpu3d apagar` / `gpu3d encender [anillo]` | software / volver a encender y probar (por lotes o en el anillo) |
| `sgldemo -g [N]` | las escenas de SaguiGL por software y por la GPU: píxeles que se apartan y fps de cada camino |
| `sgldemo` | el cubo en una ventana; el título de fps dice `(GPU)` o `(CPU)` |
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
7. `dmesg > /home/gpu3d.txt` y copiar el archivo (o foto de `dmesg | grep GPU3D`).

Si la máquina no arranca bien o la pantalla se estropea al probar (no debería: no se toca la
pantalla), arrancar con `--gpu3d=no` y decirlo.

## Pruebas automáticas

- `tests/gen3_3d_unidad.c` (en el PC): órdenes de Gen3 recorridas como lo haría la GPU, los dos
  programas de píxeles contra sus palabras escritas a mano, tamaño del vértice declarado, memoria
  de GPU y coma flotante con enteros.
- `tests/h285_gpu3d_intel.py` (QEMU): sin GPU todo sigue por software; con `--gpu3d=simulado` (un
  driver de mentira por la CPU, `gpu3d_sim.c`) se prueba el camino entero: autoprueba, `sgldemo -g`
  contra el software, dueño del motor, fallo de píxel y vigilante simulados, `gpu3d apagar/encender`.

`tests/h287_gpu3d_r500.py` (Radeon R5xx, D287): `unidad` (`tests/r500_3d_unidad.c`: órdenes del CP, formato del
vértice, programa de vértices y programas de fragmentos contra sus palabras a mano), `fuentes`, `firmware` (el
microcódigo en la imagen) y `std` (QEMU sin Radeon, con y sin `--gpu3d=radeon`: nada cambia).

## Pendiente

- Probarlo en hardware (todo lo anterior).
- Mapear RAM normal en el GTT para el destino y las texturas (traer el cuadro sin leer memoria de
  vídeo, y texturas grandes). Hay que escribir el GTT: irá detrás de una opción.
- Renderizador de la SDL 2 por GPU y varias pantallas (siguientes piezas de la v8.3, D257).
- Gen4 en adelante (X3100, GM45, HD Graphics): otra tubería.


## 2D por la GPU sin ser administrador (D346)

La 2D acelerada (`acel2d`, solo copiar y rellenar rectángulos) se usa al arrastrar ventanas y al desplazar la consola; el resto es software.
El estado y el reparto GPU/software (contadores en `INFO_ACEL2D`) los ve cualquier usuario: `acel2d`, `infopc` (sección GPU) y
Configuración (Pantalla > Aceleración gráfica). `infopc --bench-gpu` mide fuera de pantalla sin permisos; `--admin` (o el botón
«Medir con la GPU») añade la autoprueba del acelerador, que sigue siendo del administrador igual que `acel2d probar|apagar|encender`.
Detalle en `docs/decisoes.md` (D346).
