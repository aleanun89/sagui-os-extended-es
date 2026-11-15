# 126 - Drivers de la época

- **Estado:** en curso (**v8.3.3**: la Sound Blaster 16, D302; mandos y joysticks, D307)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** D088-D090 (mezclador de sonido), D275 (varias tarjetas), D292 (captura), D301 (86Box), issues #36 y #38 (IDE PIIX)

## Problema / motivación

SaguiOS se centra en equipos antiguos, del Pentium II/Athlon a los i3-i7 de 5.ª-7.ª generación. Los drivers que tiene
son los de los chipsets de 2002 en adelante (HDA, AC'97, e1000, RTL8139/8169, AHCI): un PC de 1997-2001 con tarjetas
ISA o PCI de la época se queda sin sonido, sin red y con el disco IDE por PIO. 86Box (D301) permite probarlos sin
tener el hardware.

## Propuesta

Una serie de drivers pequeños, cada uno con su prueba en QEMU (cuando lo emula) y una comprobación en 86Box:

1. **Sound Blaster 16** (ISA, DSP 4.xx): hecho en la v8.3.3 (D302, `docs/sb16.md`). Incluye el **controlador de
   DMA 8237** (`kernel/dispositivos/isa.c`, reutilizable), la **memoria baja reservada** al arrancar, el **ISA PnP**
   (Vibra16/AWE PnP) y la lista de **dispositivos ISA** (`INFO_ISA`, `lspci`, `infopc`).
2. **Ensoniq ES1370/ES1371** (AudioPCI, Sound Blaster PCI 64/128): PCI, DMA propio; QEMU emula la ES1370.
3. **NE2000 PCI (RTL8029) e ISA**: la red de 86Box y de muchos PC; QEMU emula las dos (`ne2k_pci`, `ne2k_isa`).
4. **AMD PCnet** (Am79C970A, PCnet-PCI II): QEMU `pcnet`, VirtualBox y VMware antiguos.
5. **Intel e100** (82557/8/9, EtherExpress PRO/100): QEMU `i82557b` y similares.
6. **3Com 3c905** (Boomerang/Cyclone): muy común en PC de marca; ni QEMU ni 86Box 6.0 lo emulan (habrá que probarlo
   en hardware).
7. **DMA y LBA del IDE PIIX** (issues #36 y #38): Bus Master IDE con interrupción (sin congelar el tic) y LBA48/LBA28
   bien en los PIIX3/PIIX4.

8. **Mandos y joysticks** (encargo del mantenedor, 2026-11-15): hecho en la v8.3.3 (D307, `docs/mandos.md`). USB HID
   (joystick y mando, con un analizador general de descriptores de informe), el mando de Xbox 360 con cable (clase
   propia 0xFF/0x5D/0x01) y el joystick analógico del puerto de juegos 0x201 (temporización RC con el TSC,
   calibración); `INFO_MANDOS` (77), la API de la libsagui, `SDL_Joystick` en las SDL 1.2 y 2 y `SDL_GameController`,
   DOSBox (`joysticktype=auto`), Configuración > Mandos y la orden `mandos`.

Después, si hace falta: Sound Blaster Pro/2.0 (DSP 2-3, solo 8 bits), síntesis FM por el OPL3 para MIDI, MPU-401.

## Criterios de aceptación

- [x] SB16: detección (puertos fijos e ISA PnP), reproducción a 44,1 kHz, mezclador CT1745, captura y su prueba
  (`tests/h302_sb16.py`); 86Box `p2-voodoo3` la detecta
- [x] Mandos: USB HID y Xbox 360 por UHCI/OHCI/EHCI/xHCI con conexión en caliente, puerto de juegos con calibración,
  SDL 1.2/2 y DOSBox (`tests/h307_mandos.py`, con mandos de prueba en el kernel porque el QEMU no emula ninguno)
- [ ] Mandos: comprobar en 86Box el joystick del puerto de juegos y en hardware real un mando USB y un Xbox 360
- [ ] ES1370/ES1371
- [ ] NE2000 PCI e ISA
- [ ] PCnet
- [ ] e100
- [ ] 3c905
- [ ] IDE PIIX: DMA y LBA (#36, #38)
- [ ] Decisión registrada en `docs/decisoes.md` para cada driver

## Notas

- Las sondas de puertos ISA no deben colgar ni cambiar nada en máquinas sin la tarjeta: leer antes de escribir, con
  esperas acotadas, y solo cuando no hay ya un driver PCI para lo mismo (la D302 lo hace así).
- Código propio: como referencia, la documentación pública de los fabricantes y los drivers de FreeBSD (BSD); nada
  de copiar código GPL-2.0-only de Linux.
