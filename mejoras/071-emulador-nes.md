# 071 - Emulador de NES

- **Estado:** propuesta (**v6.2**, decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** emulador CHIP-8 (`userland/chip8/`, D123 `chip8-roms`), 041 (sonido y mezclador), 029/046 (HID USB: mandos), 058 (pantalla completa)

## Objetivo

Un emulador de NES propio, en C, con la misma filosofía que el CHIP-8:
- ventana o pantalla completa (256x240 ampliado ×2/×3);
- sonido por el mezclador;
- teclado ahora y mando USB cuando haya HID.

**Sin ROMs comerciales:** el emulador es código propio y no se incluye ninguna ROM. Se usan
*homebrew* con licencia libre y ROMs de prueba descargadas solo al probar.

## Piezas

| Pieza | Contenido | Validación |
| --- | --- | --- |
| **CPU 6502 (2A03)** | Todas las instrucciones oficiales, ciclos exactos, NMI/IRQ, y las no oficiales más usadas | `nestest.nes` comparando su registro instrucción a instrucción (se descarga al probar, no va en el repositorio) |
| **PPU** | Dibujado por línea de pantalla: fondo con desplazamiento fino, *nametables* con *mirroring*, paletas, sprites 8x8/8x16, prioridades, límite de 8 sprites por línea, *sprite 0 hit*, VBlank/NMI | ROMs de prueba de la PPU y capturas comparadas con referencias |
| **APU** | 2 canales de pulso, triángulo, ruido y DMC, contador de tramas e IRQ, mezcla no lineal aproximada, salida a 44,1 kHz por el mezclador | Comparación del audio grabado por QEMU |
| **Mappers** | 0 NROM, 1 MMC1, 2 UxROM, 3 CNROM y 4 MMC3 (IRQ por línea): la gran mayoría del catálogo | Una ROM *homebrew* o de prueba por mapper |
| **Entrada** | Teclado configurable (2 jugadores); mando USB con la HID (v8) | Prueba por QMP |
| **Extras** | Pausa, reinicio, guardar y cargar partida en cualquier punto (*save states*), batería de la SRAM en `/home`, velocidad ×2, captura | — |

## Formato y rendimiento

- iNES y NES 2.0. La NES va a 1,79 MHz (la PPU, a 5,37 MHz); con un dibujado por línea de
  pantalla sobra en un Pentium III, y en el dv7 o el G470 va a 60 fps holgados.
- Unas 6.000-9.000 líneas de C, de las que el ejecutable ocupa unos 100 KB.
- La sincronización CPU-PPU es la parte delicada: avanzar la PPU 3 ciclos por cada ciclo de
  CPU, o por bloques con puntos de sincronización en los accesos a sus registros.

## Fases

1. CPU 6502 + NROM + PPU (fondo y sprites): ya funcionan juegos sencillos y *homebrew*.
2. APU (sonido).
3. Mappers MMC1, UxROM, CNROM y MMC3.
4. *Save states*, SRAM, ajustes de teclado, pantalla completa y mando USB.

## Pruebas

- `nestest` (registro de la CPU) y ROMs de prueba de la PPU y la APU, descargadas en el
  momento de probar y nunca dentro del repositorio.
- Un *homebrew* propio mínimo (ensamblado en el anfitrión) para cada mapper.
- Capturas en QEMU y audio grabado.
- Rendimiento: fps con y sin KVM.

## Criterios de aceptación

- [ ] `nestest` coincide con el registro de referencia
- [ ] Juegos NROM y de los mappers 1-4 se ven y suenan bien
- [ ] 60 fps en el dv7 y en QEMU con KVM
- [ ] Sin ROMs comerciales en el repositorio ni en la imagen
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Objetivo a largo plazo: compilarlo con el `cc` de SaguiOS

Como los demás programas del sistema, el emulador se compila con el **gcc del PC** al hacer
`make`. **Objetivo apuntado (mantenedor, 2026-11-08):** que en algún momento lo compile el **propio
`cc` de SaguiOS**, como prueba de fuego de su madurez, y como paso hacia que el sistema se compile
a sí mismo. Requisitos:
- en `cc`: varios `.c` y enlazado, `long long`, `stdarg`, `goto`, más biblioteca (v6.2) y variables
  en registros, para llegar a 60 fps;
- el código del emulador se escribe desde el principio en un C «sencillo», sin extensiones de
  gcc, para que sea compilable por los dos. Cuando `cc` llegue, se añade una prueba que lo
  compile dentro de SaguiOS y compare su salida y sus fps con la versión de gcc.

## Notas

- Modelo sugerido: **Opus** para la CPU, la PPU y la sincronización; **Sonnet** para la APU,
  los mappers y los extras.
- Se puede añadir más adelante un descargador de *homebrew* con licencia libre, como
  `chip8-roms`, si hay un archivo con licencias claras.
