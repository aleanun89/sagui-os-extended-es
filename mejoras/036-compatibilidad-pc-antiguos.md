# 036 - Compatibilidad con PC x86 antiguos («resucitar» PC de forma educativa) y SaguiGL

- **Estado:** en curso. Fase A hecha salvo MicroPython sin SSE2 (rama `compat-antiguos`,
  D076-D078); fases B, C y D, hoja de ruta.
- **Prioridad:** alta (es la dirección elegida para el proyecto)
- **Alta:** 2026-11-07
- **Relacionadas:**
  - 029 (USB 1.1 y HID), 030 (ACPI), 035 (almacenamiento y memoria), 026 (vídeo y
    resoluciones);
  - D073 (código de drivers de Linux permitido, aislado y registrado).

## Problema / motivación

Objetivo: que SaguiOS sirva para devolver a la vida, de forma educativa, PC x86 de 1997-2011
(Pentium II/III, Athlon, Pentium 4, Pentium M, Core Duo, Core 2, y los portátiles G470 y HP
dv7), mejorando la compatibilidad de hardware poco a poco y con un registro de máquinas
probadas.

Lo que hoy lo impide o lo limita:

| Hallazgo | Efecto |
| --- | --- |
| MicroPython se compila con SSE2 fijo (`MPCFLAGS ... -msse2 -mfpmath=sse`, también `libc_sagui.o`) | en Pentium III y Athlon/Athlon XP (SSE sin SSE2) `python` muere con «instrucción inválida» al usar `float` |
| `cc` genera SSE2 por defecto (D039; `-x87` es opcional) | los programas compilados fallan en esas CPU |
| El kernel exige i686 con CMOV (`-march=i686`) | no arranca en Pentium/Pentium MMX ni en AMD K6 |
| Solo hay imagen de disco (`.img`/`.vdi`), no ISO | los PC de 1997-2003 casi nunca arrancan desde USB |
| Vídeo VESA solo en 24/32 bits | muchas tarjetas de 2-8 MB (ATI Rage, S3, i810, TNT) solo ofrecen 1024x768 a 16 bits |
| USB solo EHCI | en PC con USB 1.1 (UHCI/OHCI) no hay pendrive ni teclado o ratón USB |
| Red solo e1000, RTL8139 y r8169 experimental | faltan las más comunes de la época: Intel e100, 3Com 3c905, VIA Rhine |
| Disco ATA solo PIO | lento en máquinas donde IDE es lo único que hay |
| Sin apagado (ni ACPI ni APM) | hay que mantener pulsado el botón |
| Sin sonido real (solo el altavoz del PC) | juegos y CHIP-8 sin sonido |

## Fase A: «que arranque y no se cuelgue en cualquier PC» (esfuerzo medio-bajo)

1. **Sin SSE2, de forma automática:**
   - MicroPython en dos versiones (`python` SSE2 y x87), y el lanzador elige según el CPUID;
     o una sola versión x87, si la diferencia de rendimiento lo permite (medir con `bench`).
     **Hecho** (mejora 033, D082): dos binarios, `/bin/python` ejecuta `/bin/python-x87` si no
     hay SSE2; la x87 a 53 bits da los mismos resultados; probado con `-cpu pentium3` (h68) y
     `-cpu pentium2` (h35);
   - `cc`: x87 por defecto si la CPU no tiene SSE2 (`-sse2` y `-x87` para forzar);
   - revisar cualquier otro objeto compilado con `-msse2` (libtls `aesni.o` ya va aparte y
     comprueba el CPUID; `simd.c` es una prueba).
2. **ISO arrancable desde CD** (El Torito con emulación de disco duro sobre la imagen actual,
   o sin emulación con un cargador de CD propio). `make iso`, y `run_in_qemu.sh --cd`.
3. **Vídeo a 16 y 15 bits** (RGB565/RGB555) además de 24/32: conversión en la copia del búfer
   trasero a la tarjeta (`rapido.c`), con el resto del sistema a 32 bits; reserva automática
   640x480 si no hay otro modo.
4. **Informe de compatibilidad:** `infopc --informe` escribe en el pendrive, el disquete o
   `/home` un `.txt` con:
   - CPU y recursos;
   - memoria (E820);
   - dispositivos PCI (fabricante:modelo, clase, driver o «sin driver»);
   - modos VESA;
   - discos, USB y tablas ACPI;
   - y el log del arranque.
5. **`docs/compatibilidad.md`:** tabla de máquinas probadas con lo que funciona y lo que no
   (✓, ⚠, ✗), enlazando su informe.

### Fase A: qué se hizo y qué falta (rama `compat-antiguos`)

| Punto | Estado | Dónde |
| --- | --- | --- |
| 1. `cc` sin SSE2 | **hecho**: x87 por defecto si la CPU no tiene SSE2; `-sse2` (con aviso) y `-x87` fuerzan | D076, `cc.c` |
| 1. Otros objetos con `-msse2` | **revisado**: `sse2.o` del kernel y de la libsagui y `aesni.o` ya comprobaban el CPUID; `simd.o` ya no lleva `-msse2` en todo el archivo (atributos `target` por función) | D076 |
| 1. Función para el lanzador de Python | **hecho**: `cpu_tem_sse2()` (libsagui, ya existía; documentada en `sagui.h`) | D076 |
| 1. MicroPython sin SSE2 | **pendiente** (tras fusionar el hilo `python-libs`): hoy `python` sale con «este procesador no tiene SSE2» | — |
| 2. ISO arrancable | **hecho**: El Torito con emulación de disco duro, `tools/mkiso.py`, `make iso`, `run_in_qemu.sh --cd`; el cargador lee por CHS si no hay extensiones del INT 13h; `pantalla --guardar` explica que el CD es de solo lectura | D077 |
| 3. Vídeo a 16 y 15 bits | **hecho**: el cargador acepta 32 > 24 > 16 > 15, `mkimagem.py --bpp`, reserva 640x480 (VESA 1.2); conversión al copiar (C o SSE2), cursor, consola y cambio en caliente (BGA) | D078 |
| 4. `infopc --informe` | **hecho**: CPU, E820, SMBIOS, PCI con tabla de lo que se sabe, modos VESA, arranque, discos, USB, ACPI (RSDP/RSDT/XSDT), log del kernel (anillo de 32 KB); `/home` > `/usb` > `/tmp` | D076 |
| 5. `docs/compatibilidad.md` | **hecho**: QEMU con varias CPU, CD y 16/15 bits, VirtualBox, G470 y HP dv7 (pendientes de informe) | — |
| Pruebas | `tests/h65_compat_antiguos.py`; `/testes/sse2` compara también la conversión SSE2 a 16 bits (`h40`) | — |

Pendiente de la fase A, además de MicroPython: probar la ISO y los 16 bits en máquinas reales
(la emulación de disco duro del El Torito es la menos probada en BIOS reales; si falla, añadir
una entrada «sin emulación» al catálogo) y recoger los informes del G470 y del HP dv7.

## Fase B: periféricos de la época (de uno en uno, según los informes)

Con la D073 se pueden adaptar drivers de Linux o los BSD; cada uno va en `kernel/terceros/` si
lleva código ajeno, con su prueba en QEMU, 86Box o PCem.

| Pieza | Por qué | Prueba |
| --- | --- | --- |
| USB 1.1 (UHCI y OHCI) + teclado y ratón USB | mejora 029, fase 1 | QEMU |
| Red **Intel e100** (8255x) | la más común en PC de oficina de 1998-2005 | QEMU (`i82557b`) |
| Red 3Com 3c905 y VIA Rhine | muy comunes en clónicos | 86Box o real |
| **IDE con DMA** (bus master) | el disco va varias veces más rápido | QEMU (PIIX) |
| ACPI básico (030), y **APM** para apagar Pentium II/III sin ACPI útil | apagar de verdad | QEMU |
| Sonido **AC'97** (Intel ICH, VIA), luego Sound Blaster 16 y **HDA** (Core Duo) | sonido real para juegos y CHIP-8 | QEMU (`AC97`, `sb16`, `intel-hda`) |
| AHCI | discos SATA de los Core Duo, Core 2 y el G470 | QEMU (`ich9-ahci`) |

## Fase C: más atrás (opcional)

- Compilación **i586** (sin CMOV) para Pentium y Pentium MMX y AMD K6: `make CPU=i586`.
- Modo de 8 bits con paleta para VGA y SVGA muy antiguas.

## Fase D: «SaguiGL», una mini biblioteca 3D inspirada en OpenGL ES 1.1

**Nombre:** «OpenGL» es marca de Khronos y exige pasar sus pruebas de conformidad, así que la
biblioteca se llama **SaguiGL** (`libsgl`). Su API está **inspirada** en OpenGL ES 1.1, con
nombres `gl*` parecidos, y la implementación es propia salvo lo que permita la D073 en los
motores de hardware.

### D.1 El subconjunto («recorte»)

| Incluido | Fuera |
| --- | --- |
| Triángulos (sueltos, tiras, abanicos), líneas y puntos sencillos | shaders visibles al programador (GLSL) |
| Arrays de vértices (`glVertexPointer`, `glColorPointer`, `glTexCoordPointer`, `glDrawArrays`, `glDrawElements`) y `glBegin`/`glEnd` de conveniencia | listas de visualización; VBO en la primera versión |
| Matrices de modelo/vista y proyección con pila, `glFrustum`, `glOrtho`, rotar, trasladar, escalar | doble precisión |
| Búfer Z de 16 bits, recorte de caras traseras | stencil, acumulación, multimuestreo |
| Sombreado plano y Gouraud | iluminación por píxel |
| Una textura 2D (RGB565, RGBA8888; potencias de 2; vecino o bilineal; repetir o recortar; *modulate*/*replace*) | multitextura, mipmaps automáticos y compresión (quizá más adelante) |
| Mezcla alfa y niebla lineal | — |
| Iluminación fija: 1-2 luces direccionales (ambiente y difusa; especular opcional) | luces puntuales con atenuación |
| Contexto ligado a una ventana de SaguiOS (`sgl_criar_contexto(janela)`, `sgl_trocar()`), pantalla completa | varios contextos compartidos |

### D.2 Arquitectura: una API y varios motores, con alternativa garantizada

```
  programa / Sagui 3D / Python (sagui.gl, futuro)
                 │  SaguiGL (libsgl)
  ┌──────────────┼──────────────┬───────────────┬──────────────┬───────────────┐
software     Radeon R300/R420  Radeon R5xx    Radeon R6xx/R7xx  Radeon Evergreen  (opc.) 3dfx Voodoo
(referencia) X300-X850         X1300-X1950    HD 2xxx/3xxx/4xxx HD 5xxx
```

- **El motor por software es obligatorio y el primero:**
  - SSE si la CPU lo tiene; x87 o coma fija si no (Pentium II);
  - es la referencia de las pruebas: píxeles comparados con imágenes esperadas.
- **Elección del motor:**
  - SaguiGL elige el motor por el identificador PCI de la tarjeta;
  - si el motor no se reconoce o falla al iniciar, usa el de software sin que el programa se
    entere;
  - `SGL_MOTOR=software|r300|r500|r600|evergreen` lo fuerza.
- **El kernel da lo mínimo:**
  - mapear al proceso los registros (BAR) y una zona de VRAM de la tarjeta, con permiso;
  - un búfer de DMA en memoria física contigua;
  - un **vigilante**: si la GPU no termina un trabajo en un tiempo límite, se reinicia el
    motor 3D o se abandona y se pasa a software, y el programa recibe un error, nunca un
    cuelgue del sistema.
  La lógica va en espacio de usuario: si un motor falla, cae el programa, no el kernel.
- **Pantalla:**
  - se sigue usando el modo VESA para la salida de vídeo; programar modos en Radeon exige
    interpretar AtomBIOS, que es otro proyecto;
  - el 3D dibuja en un búfer de la VRAM y se copia o se muestra en la zona que la tarjeta ya
    está enseñando;
  - el motor enciende a mano el bloque 3D o sus relojes, si la BIOS los deja apagados.

### D.3 Familias Radeon soportadas (escritorio y Mobility)

| Serie | Familia | Ejemplos de escritorio | Ejemplos Mobility e integradas | Programación | Documentación | Firmware |
| --- | --- | --- | --- | --- | --- | --- |
| **X300-X850** | R300/R350/R360, RV370/RV380, R420/R423/R480, RV410 | X300, X550, X600, X700, X800, X850 | Mobility Radeon X300, X600, X700, X800; Radeon Xpress 200M (RS480, núcleo tipo RV370) | shaders SM 2.0 (ISA de R300) | sin manual oficial completo; muy parecida a R5xx; drivers `radeon`/Mesa r300 de referencia (MIT, D073) | ninguno obligatorio: se puede programar por registros (MMIO); el CP con microcódigo es opcional |
| **X1300-X1950** | R5xx (RV515, RV530, RV560/570, R520, R580) | X1300, X1600, X1650, X1800, X1900, X1950 | Mobility Radeon X1300, X1400, X1600, X1700, X1800, X1900; Radeon X1200/X1250 (RS690, integrada) | shaders SM 3.0 (ISA nuevo de píxel) | **oficial de AMD**: guía de aceleración y registros 3D de R5xx | igual que arriba |
| **HD 2xxx/3xxx** y **HD 4xxx** | R6xx (RV610/630/670) y R7xx (RV710/730/770) | HD 2400, 2600, 3450, 3650, 3850, 4350, 4550, 4670, 4850 | Mobility Radeon HD 2400, 3450, 3650, 4330, 4530, 4650, 4670; **HD 3200/3300** (RS780) y HD 4200/4250 (RS880), integradas | VLIW5, shaders unificados, órdenes por el procesador de comandos (CP) | **oficial de AMD**: ISA y registros 3D de R6xx/R7xx | **microcódigo CP (PFP/ME) obligatorio**, de linux-firmware (D073) |
| **HD 5xxx** | Evergreen (Cedar, Redwood, Juniper, Cypress) | HD 5450, 5570, 5670, 5770, 5870 | Mobility Radeon HD 5430, 5470, 5650, 5730, 5850 | VLIW5 (cambios de ISA y registros respecto a R7xx) | **oficial de AMD**: ISA y registros de Evergreen | **CP + RLC obligatorios**, de linux-firmware |

**Particularidades de las versiones Mobility e integradas** (portátiles):

- Mismo núcleo que la de escritorio, pero otros identificadores PCI: la tabla de motores
  agrupa por familia, no por modelo.
- La BIOS del portátil suele dejar la GPU a **relojes reducidos** (ahorro de energía). Se
  respetan; no se tocan voltajes. Como mucho se suben relojes a los valores de la tabla de la
  BIOS, y solo si se documenta y prueba.
- La pantalla interna va por LVDS y la configura la BIOS; como seguimos con VESA, no hay que
  tocarla.
- Las **integradas** (Xpress 200M, X1200/X1250, HD 3200/4200) usan la RAM del sistema como
  «VRAM» (zona reservada por la BIOS, visible en el E820): el gestor de memoria de la GPU
  debe respetarla.
- **Vigilar la temperatura:** sin ACPI completo no hay control térmico propio; con la GPU a
  carga alta durante mucho rato, confiar en la protección del hardware y documentarlo.
- El **HP dv7** (HD 3200 integrada o HD 3450/4530) es la máquina de pruebas real de la
  familia R6xx/R7xx.

**Cómo se generan los shaders:** ninguna de estas familias tiene la tubería fija de OpenGL 1.x
en hardware. SaguiGL traduce cada combinación de estado (textura sí/no, *modulate*/*replace*,
niebla, alfa, número de luces...) a un pequeño programa de vértices y de píxeles en el ISA de
cada familia:
- un generador en `tools/` produce las variantes **precompiladas** en el build (decenas, no
  miles);
- una caché en tiempo de ejecución elige la variante;
- no hay compilador de shaders en el sistema.

**Robustez de los motores por hardware:**
- detectar familia y revisión antes de tocar nada;
- comprobar que el bloque 3D responde (un registro de prueba, un trabajo vacío con su marca
  de fin) y, si no, motor por software;
- el vigilante de la sección D.2;
- nunca escribir en registros de pantalla, memoria de vídeo ajena ni energía fuera de lo
  documentado;
- una opción de arranque (menú de la mejora 035) para desactivar toda aceleración 3D.

### D.4 Pruebas

- **Motor por software:** en QEMU, escenas de prueba comparadas píxel a píxel (o con una
  tolerancia mínima documentada).
- **3dfx Voodoo** (opcional): 86Box y PCem la emulan con Pentium II/III.
- **Radeon: ningún emulador emula su 3D**, así que la prueba es en hardware real con un
  programa `sgltest`:
  - dibuja las escenas de referencia con el motor de hardware y con el de software;
  - compara las imágenes y escribe un informe;
  - se adjunta a `docs/compatibilidad.md` con la tarjeta y la máquina.
- Demo `sgldemo`: cubo texturizado, terreno y el sagüí en 3D con modelos propios.

### D.5 Orden y esfuerzo orientativo

| Paso | Contenido | Tamaño aprox. | Dónde se prueba |
| --- | --- | --- | --- |
| D1 | API SaguiGL + motor por software + `sgldemo` | ~3.000 líneas | QEMU |
| D2 | **R6xx/R7xx** (HD 2xxx-4xxx, incluidas Mobility y HD 3200/4200): CP con firmware, GART, generador de shaders VLIW5 | ~6.000-8.000 | **HP dv7** |
| D3 | Evergreen (HD 5xxx y Mobility HD 5xxx): reutiliza mucho de D2 | +2.500-3.500 | una HD 5xxx |
| D4 | R5xx (X1xxx y Mobility X1xxx, X1200/X1250) | ~5.000 | una X1xxx |
| D5 | R300/R420 (X300-X850, Mobility X300-X800, Xpress 200M): reutiliza parte de D4 | +2.500 | una X300-X850 |
| (opc.) | 3dfx Voodoo 1/2/3 | ~1.200 | 86Box/PCem y real |

D2 va primero porque el dv7 permite probarla. La D073 reduce bastante los tamaños de
D2-D5: las definiciones de registros y partes de la inicialización se pueden tomar del driver
`radeon` de Linux y de Mesa, que en buena parte son MIT, aisladas y registradas en
`docs/terceros.md`.

## Criterios de aceptación

- [ ] Fase A: `python` y los programas de `cc` funcionan con `-cpu pentium3` y `-cpu athlon`
      en QEMU; ISO arrancable; modos de 16 bits; `infopc --informe`; `docs/compatibilidad.md`
      (todo hecho y probado en `h65`; `python` sin SSE2 con la variante x87: mejora 033, D082;
      el `athlon` de QEMU 8.2 tiene SSE2, así que la prueba usa `athlon,-sse2`)
- [ ] Fase B: cada driver con su prueba y su fila en `docs/compatibilidad.md` y, si lleva
      código ajeno, en `docs/terceros.md`
- [ ] Fase D1: SaguiGL por software con pruebas de píxeles en QEMU y `sgldemo`
- [ ] Fase D2-D5: `sgltest` coincide con el motor por software en cada familia probada; un
      fallo de GPU nunca cuelga el sistema
- [ ] Decisiones registradas en `docs/decisoes.md` (una por driver o motor)

## Notas

- Las pruebas en máquinas reales las hace el mantenedor con `infopc --informe`; cada informe
  decide la siguiente pieza de la fase B.
- Emuladores útiles para la época: QEMU (`-cpu pentium2/pentium3/athlon`, PIIX, e100, AC'97,
  SB16, UHCI/OHCI), y 86Box/PCem para tarjetas que QEMU no tiene (Voodoo, S3, Rage, 3Com).
