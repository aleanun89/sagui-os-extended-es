# 054 - Motor 3D con aceleración por software (SaguiGL) y plan de drivers 3D de AMD

- **Estado:** **v5 hecha** (A1 base e identificación de la GPU, D104; ver «Estado de la v5») y **v6 hecha** (A1-A2: texturas, mezcla, niebla, luces y ruta SIMD, D130; ver «Estado de la v6»); **B2 con la estructura hecha** (D158, mejora 090: la tabla de familias, la identificación y el vigilante; ver «Estado de la B2»); el resto, propuesta, repartida por versiones: **v6** el grupo «3D» de SaguiMark; **v7** Sagui 3D sobre SaguiGL (A5) y 2D por la GPU (B1.1-B1.4; **en la v7.2, D195, la capa común `acel2d` y el motor 2D por registros de R100-R500 —la X1400—, sin probar en hardware; el 2D de R6xx/R7xx sigue pendiente porque necesita el CP con microcódigo**); **v8** 3D por la GPU (B1.5-B1.6) y A3 si hay SMP. Concreta la fase D de la [036](036-compatibilidad-pc-antiguos.md)
  en un plan de trabajo por pasos.
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 036 (fase D: API SaguiGL y familias Radeon), D073 (firmware y código de
  Linux/Mesa), 051 (SaguiMark), 015 (SSE2), B4 (Sagui 3D)

## Parte A: SaguiGL con motor por software optimizado

Subconjunto de OpenGL ES 1.1 descrito en la 036 (D.1): triángulos, matrices, búfer Z,
Gouraud, una textura, mezcla alfa, niebla y luces fijas. Biblioteca `libsgl`, en espacio de
usuario.

| Paso | Contenido |
| --- | --- |
| A1 | Tubería por software de referencia: transformación y recorte (planos de frustum), proyección, *setup* de triángulos con aritmética de punto fijo (aristas incrementales y regla *top-left*), interpolación perspectiva-correcta de color y UV, Z de 16/32 bits, texturas vecino o bilineal, mezcla y niebla |
| A2 | **Aceleración por software:** rasterización por **bloques (tiles) de 8x8** con descarte rápido, *spans* horizontales, interpolación en SIMD (**SSE/SSE2**: 4 píxeles a la vez; MMX en Pentium II; ruta en C sin SIMD), texturas en orden *swizzled* para la caché, Z jerárquico sencillo por bloque, escritura en el búfer trasero del compositor sin copias extra |
| A3 | **Varios núcleos** cuando exista SMP (017): bloques repartidos entre núcleos. Opcional, después de la 017 |
| A4 | Integración: contexto en una ventana o a pantalla completa; demo `sgldemo` (cubo texturizado, terreno, el sagüí en 3D con modelos propios); módulo `sagui.gl` en Python si es barato |
| A5 | **Sagui 3D sobre SaguiGL** como motor alternativo (el actual 2,5D se mantiene), para comparar rendimiento y aspecto |

**Medida:**
- prueba de píxeles contra imágenes de referencia (determinista);
- grupo **«3D»** en SaguiMark: triángulos por segundo, *fill rate* y fps de escenas fijas, en
  QEMU/KVM y en los portátiles.

**Objetivo orientativo:** escenas sencillas a 640x480 a 30 fps en un Pentium III con SSE, y a
1024x768 en un Core Duo.

## Parte B: plan de drivers 3D de AMD (Radeon)

Por qué AMD:
- el **HP dv7** tiene una Radeon **HD 3200 (RS780, R6xx)** o una **HD 3450/4530**;
- el **Amilo** tiene una **X1400 (R5xx)**;
- AMD publicó documentación oficial de R5xx, R6xx/R7xx y Evergreen;
- con la D073 se pueden usar como referencia (y copiar definiciones de) los drivers `radeon` de
  Linux y `r300`/`r600` de Mesa, en buena parte MIT, y el microcódigo de linux-firmware.

**Lo que no se hará:** el **Ryzen B450** lleva una **RX 6400 (Navi 24, RDNA2)**. Su driver
(`amdgpu`) necesita cargar firmware en el PSP, gestionar el SMU y programar GFX10.3 y su
gestión de memoria. Es decenas de veces más grande y se deja **fuera del plan**: ahí se usa el
motor por software.

### B1. R6xx/R7xx (HD 2xxx-4xxx, incluidas Mobility y HD 3200/4200): primera familia, máquina de pruebas: HP dv7

| Paso | Contenido | Prueba (en el dv7) |
| --- | --- | --- |
| B1.1 | **Solo lectura:** detectar la familia por PCI, mapear los registros (MMIO), volcar identificación, relojes y estado en `infopc --informe` | el informe |
| B1.2 | **Procesador de comandos (CP):** cargar el microcódigo PFP/ME (linux-firmware, `firmware/amd/`, D073), anillo de comandos, `NOP` y escritura de una marca (*fence*). Vigilante: si no responde en X ms, se reinicia o se abandona | la marca vuelve; sin cuelgues |
| B1.3 | **Memoria:** GART para que la GPU lea memoria del sistema (en las integradas, la «VRAM» reservada por la BIOS en el E820), y gestor simple de VRAM | copias CPU→GPU→CPU idénticas |
| B1.4 | **2D por la GPU (primer beneficio real):** copias y rellenos de rectángulos con el motor de la GPU para el compositor de ventanas: mover ventanas y redibujar sin gastar CPU. Alternativa siempre lista: la ruta SSE2 actual (`rapido.c`) | capturas idénticas a la ruta por software; uso de CPU menor |
| B1.5 | **3D:** estado de la tubería de R600, búferes de vértices, superficie de color y Z, y **shaders VLIW5 precompilados** para las combinaciones de estado de SaguiGL (generados por una herramienta en `tools/` al compilar; sin compilador de shaders en el sistema) | `sgltest` coincide con el motor por software; fps en SaguiMark |
| B1.6 | Motor `r600` de SaguiGL, con caída automática al motor por software ante cualquier fallo | `sgldemo` y Sagui 3D en el dv7 |

### B2. Siguientes familias (después de B1, según el hardware disponible)

- **Evergreen (HD 5xxx):** reutiliza mucho de B1 (CP+RLC, cambios de ISA y registros).
- **R5xx (X1xxx; Amilo):** sin microcódigo obligatorio, se puede programar por registros; ISA
  de shaders distinto; documentación oficial.
- **R300/R420 (X300-X850):** reutiliza parte de R5xx.

**Estado de B2: hecha la estructura (D158, mejora 090).** B1 sigue sin hacerse, así que B2 no
se puede dar por hecha: lo que hay es el **armazón** de las tres familias (la tabla, la
identificación, el vigilante y la opción de arranque), más la pieza mínima del kernel para
leer los registros MMIO de un BAR. Ver «Estado de la B2» más abajo y la mejora 090.

### Seguridad y robustez (comunes)

- Todo en espacio de usuario salvo lo mínimo en el kernel:
  - mapear BAR y VRAM con permiso;
  - búfer de DMA;
  - IRQ;
  - **vigilante de la GPU**.
  Un fallo de la GPU termina el programa o vuelve al motor por software, **nunca cuelga el
  sistema**.
- No se tocan modos de vídeo (se sigue con VESA), voltajes ni relojes salvo los valores que la
  BIOS deja. Se vigila la temperatura con el monitor de la 050 durante las pruebas.
- Opción de arranque para **desactivar toda aceleración** (y `SGL_MOTOR=software`).
- Sin emulador posible: cada paso se valida en el dv7 con un programa de prueba que escribe un
  informe en `/home` (persistencia FAT32) para traerlo al PC de desarrollo.

## Estado de la v5 (hecha, D104)

- **`libsgl`** (`userland/libsgl/`: `sgl.h`, `sgl.c`, `cenas.c`): contexto ligado a una ventana
  (`sgl_criar_contexto`, `sgl_trocar`) o a memoria (`sgl_criar_contexto_mem`); `glMatrixMode`,
  `glLoadIdentity`, `glPushMatrix`/`glPopMatrix`, `glTranslatef`, `glRotatef`, `glScalef`,
  `glFrustumf`, `glOrthof`, `glViewport`, `glClear` (color y Z), `glClearColor`,
  `glEnable`/`glDisable` (`GL_DEPTH_TEST`, `GL_CULL_FACE`), `glCullFace`, `glFrontFace`,
  `glShadeModel`, `glDepthFunc`; matrices de vértices y colores con `glDrawArrays` y
  `glDrawElements` (`GL_TRIANGLES`, `_STRIP`, `_FAN`, `GL_LINES`, `_LINE_STRIP`, `_LINE_LOOP`) y
  `glBegin`/`glEnd`. Búfer Z de 16 o 32 bits. Sombreado plano (color del último vértice) y
  Gouraud (en espacio de pantalla, sin corrección de perspectiva). Recorte contra los 6 planos
  del frustum y descarte de caras traseras.
- **Rasterización** con enteros: coordenadas 28.4, funciones de arista incrementales con la regla
  *top-left*, gradientes de Z y color en Q16 (una división de 64 bits por triángulo, propia, sin
  libgcc). Determinista: sin libc ni libm (seno y coseno propios, `sqrtss`), compilada con
  `-ffp-contract=off`; el anfitrión da los mismos bits que el SaguiOS.
- **`sgldemo`**: cubo girando 320x240 con fps; `-t N` (sin ventana), `-z 16|32`, `-c escena`.
  Requiere SSE2 (comprueba el CPUID); el motor aún no tiene ruta x87 para el Pentium III.
- **`tests/h77_sgl.py`**: sumas FNV-1a de tres escenas fijas (triángulo Gouraud+plano, cubo con
  Z y cull, Z cruzado con líneas y abanico) y de 60 cuadros del demo, con Z de 16 y de 32 bits,
  contra el motor compilado en el anfitrión (`tests/sgl/anfitriao.c`); más la tabla de GPU.
- **Identificar la GPU:** `userland/inc/gpuid.h` (tabla de rangos de AMD R100-posteriores e
  Intel Gen2-posteriores; si no está, «desconocida») usada en la pestaña Vídeo de `infopc`
  (`infopc -t video`): familia, chips y si SaguiGL tendrá motor por hardware (R6xx/R7xx primero,
  Evergreen/R5xx/R300 después, Intel Gen3 y Gen6; el resto, solo software).
- **Falta de la parte A:** grupo «3D» de SaguiMark (v6), ruta x87 para CPU sin SSE2,
  Sagui 3D sobre SaguiGL (v7).

## Estado de la v6 (hecha, D130)

- **Texturas** (`glGenTextures`, `glBindTexture`, `glTexImage2D`, `glTexParameteri`,
  `glDeleteTextures`, `glTexCoord2f`, `glTexCoordPointer`): RGBA de 8 bits, del vecino más
  próximo, con repetición (`GL_REPEAT`) o borde (`GL_CLAMP_TO_EDGE`), hasta 1024x1024 y
  cuatro texturas vivas a la vez.
- **Mezcla** (`glEnable(GL_BLEND)`, `glBlendFunc`): `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`
  y `GL_ZERO`, después de la prueba de Z.
- **Niebla** (`glFogf`, `glFogfv`, `GL_FOG_START`, `GL_FOG_END`, `GL_FOG_COLOR`): por la
  profundidad normalizada.
- **Luces** (`glLightfv`, `GL_LIGHT0`, `GL_LIGHT_POSITION`, `GL_LIGHT_DIFFUSE`,
  `GL_LIGHT_AMBIENT`): una direccional, con la luz por cara (`GL_FLAT`).
- **Ruta SIMD** (SSE2): el sombreado va de cuatro píxeles en cuatro y el recorrido por
  bloques de 8x8; la ruta de referencia da **el mismo píxel bit a bit**. Con el `cc` de
  dentro del SaguiOS (sin intrínsecos) va la de referencia, así que el ejemplo sigue
  compilándose con el `cc` (D127).
- **Sin tocar la v5:** si no se activa ninguna de las cuatro cosas, el motor sigue por la
  ruta de siempre y las sumas de la prueba h77 no cambian.
- **`tests/h101_sagui_gl_v6.py`**: el programa `tests/sgl/sglv6.c`, compilado en el
  anfitrión y dentro del SaguiOS (`/bin/sglv6`), comprueba que con solo la textura se ven
  los 64 texels, que la escena completa tiene 9216 píxeles pintados y muchos colores, y
  que la ruta SIMD no se diferencia de la de referencia en ningún píxel.
- **Falta de la parte A:** grupo «3D» de SaguiMark, ruta x87 para CPU sin SSE2, Sagui 3D
  sobre SaguiGL (v7) y el 2D por la GPU (v7, B1).

## Estado de la B2 (hecha la estructura, D158; mejora 090)

B1 **no está hecha** (el acelerador por la GPU sigue siendo un plan), así que B2 no se puede
dar por hecha: lo que hay es la **estructura completa y verificable** de las tres familias, con
lo que B1 traiga después.

- **La tabla de familias** (`userland/libamd/`): una entrada por familia con los
  identificadores de dispositivo, la generación, los registros de identificación que se leen,
  la ISA de los shaders, **qué partes de B1 reutiliza y cuáles no**, y si hace falta
  microcódigo. Las familias de B2 son **Evergreen** (HD 5xxx, `MM_INDEX` 0xE000),
  **R5xx** (X1xxx, el Amilo, `MM_INDEX` 0x4000, sin microcódigo) y **R300/R420** (X300-X850,
  `MM_INDEX` 0x4000, sin microcódigo); la de **R6xx/R7xx** (`MM_INDEX` 0xD00, con microcódigo)
  es la base de B1.
- **La pieza mínima del kernel** (`kernel/dispositivos/bar.c`): `SIS_MAPEAR_BAR` mapea una
  ventana de un BAR de un dispositivo PCI en el espacio del programa. El kernel elige las
  páginas (solo las que el dispositivo declara), sondea el tamaño del BAR, las mapea sin caché
  y rehúsa lo que se solape con la RAM del sistema; el programa nunca ve una dirección física.
- **La identificación** (`/bin/amd`): busca la GPU de AMD en el PCI, mapea su BAR 0 y lee los
  registros de la familia (`DEVICE_ID`, `REV_ID`, `SUBSYS_ID` y el estado del bus maestro de
  registros). Sin GPU de AMD lo dice claro.
- **Las reglas de seguridad**: el motor por software es el camino por defecto; el **vigilante**
  de la GPU tiene un plazo máximo (2 s) y, si la GPU no responde, **el motor vuelve al
  software**; la opción de arranque **`SGL_MOTOR=software`** (`mkimagem.py --sgl-motor
  software`) desactiva toda la aceleración. No se tocan los modos de vídeo, los voltajes ni los
  relojes.
- **La prueba** `tests/h134_amd_b2.py`: el programa arranca y funciona, la tabla está, sin GPU
  de AMD el motor es el de software, el vigilante vence y hace caer al software, el motor por
  software da las mismas escenas que en el anfitrión, la opción de arranque desactiva la
  aceleración y el kernel mapea de verdad un BAR (con una e1000 de mentira en el QEMU).
- **Lo que falta:** el motor por hardware (B1), el microcódigo (PFP/ME) y la comprobación en
  el dv7 y en el Amilo. El QEMU no emula ninguna GPU de AMD: la lectura de los registros solo
  se ha probado con un dispositivo de mentira.

## Esfuerzo orientativo

| Bloque | Tamaño | Modelo sugerido |
| --- | --- | --- |
| A1-A2 (motor por software + SIMD) | ~3.000-4.000 líneas | Sonnet medio (medible y probado por píxeles) |
| A4-A5 (demo, Python, Sagui 3D) | ~1.000-1.500 | Sonnet medio |
| B1.1-B1.4 (detección, CP, memoria, 2D) | ~3.000 | **Opus medio** (hardware real, sin emulador) |
| B1.5-B1.6 (3D R600, shaders) | ~4.000-5.000 | **Opus medio** |

## Criterios de aceptación

- [ ] SaguiGL por software con pruebas de píxeles (**hecho en la v5**, h77) y grupo 3D en SaguiMark; ruta SIMD
      comprobada contra la de referencia (v6)
- [ ] `sgldemo` (**hecho en la v5**) y Sagui 3D sobre SaguiGL (v7)
- [ ] B1.1-B1.4 en el dv7: informe, CP con marca, GART y 2D por la GPU en el compositor
- [ ] B1.5-B1.6: 3D por la GPU en el dv7 igual que el motor por software
- [ ] B2: la estructura de las familias está hecha (D158, mejora 090); falta el motor por
      hardware de cada familia, que depende de B1
- [ ] Ningún fallo de la GPU cuelga el sistema (prueba forzando el vigilante) — el vigilante y
      la caída al software ya están (D158); falta probarlos con la GPU real
- [ ] Decisiones registradas en `docs/decisoes.md`; terceros en `docs/terceros.md`
