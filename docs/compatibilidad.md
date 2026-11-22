# Compatibilidad: máquinas probadas

SaguiOS quiere funcionar en PC x86 de 1997-2011 (mejora 036). Esta página recoge en qué
máquinas se ha probado, qué funciona y qué no, y cómo enviar un informe de una máquina nueva.

## Leyenda

| Símbolo | Significado |
| :---: | --- |
| ✓ | Funciona y está comprobado (prueba automática en QEMU o foto en la máquina real) |
| ⚠ | Funciona en parte, es experimental o no se ha comprobado en esa máquina |
| ✗ | No funciona todavía (entre paréntesis, la mejora que lo prevé) |

## Máquinas

### Emuladas (pruebas automáticas)

| Máquina | Arranque | Escritorio y gráficos | `cc` (float/double) | `python` con float | Red | Sonido | Notas |
| --- | :---: | :---: | :---: | :---: | :---: | :---: | --- |
| QEMU `qemu32` (por defecto, con SSE2) | ✓ disco, USB y CD | ✓ 32, 24, 16 y 15 bits | ✓ SSE2 | ✓ | ✓ e1000 | ✓ AC'97 e Intel HDA (`h70`) | Toda la batería (`tests/`) |
| QEMU `-cpu pentium3` (SSE sin SSE2) | ✓ | ✓ sin SSE2 (`rep movsd`) | ✓ x87 automático | ✗ (036) | ✓ | — | `tests/h65_compat_antiguos.py` |
| QEMU `-cpu athlon,-sse2` (como un Athlon XP) | ✓ | ✓ | ✓ x87 automático | ✗ (036) | ✓ | — | El `athlon` de QEMU 8.2 tiene SSE2: para imitar un Athlon XP hay que quitarlo |
| QEMU `-cpu athlon` | ✓ | ✓ | ✓ SSE2 | ✓ | ✓ | — | |
| QEMU `-cpu pentium2` (sin SSE) | ✓ | ✓ | ⚠ x87 automático (el mismo camino que en `pentium3`, sin prueba propia) | ✗ (036) | ✓ | — | `tests/h40_sse2.py` |
| QEMU con `-cdrom saguios.iso` | ✓ El Torito, lectura CHS | ✓ | ✓ | ✓ | ✓ | — | `pantalla --guardar` no puede guardar (CD de solo lectura) |
| QEMU con `mkimagem.py --bpp 16` / `--bpp 15` | ✓ | ✓ RGB565 / RGB555, también el cambio en caliente | ✓ | ✓ | ✓ | — | Colores dentro del error de 16 bits |
| VirtualBox (IDE, BIOS, VBoxVGA) | ✓ | ✓ 32 bits, cambio en caliente | ✓ | ✓ | ⚠ | ⚠ sin probar (AC'97 de ICH o HDA) | Fases 1-6 probadas; CD, 16 bits e informe **pendientes de informe** |

### Reales

| Máquina | CPU / chipset | Arranque | Vídeo | Entrada | Almacenamiento | Red | Sonido | Estado |
| --- | --- | :---: | :---: | :---: | :---: | :---: | :---: | --- |
| **Lenovo G470** (2011) | Sandy Bridge (Core i3/i5 o Pentium B9xx), HM65, Intel HD Graphics | ✓ USB (EHCI) | ✓ VESA 1024x768x32; ⚠ WC y 1366x768 sin medir | ✓ teclado y touchpad PS/2 | ✓ pendrive FAT32; ⚠ disco SATA solo en modo IDE (AHCI: 036) | ⚠ RTL810x experimental; ✗ Wi-Fi AR9285 | ⚠ HDA sin probar | Fotos de las fases 1-5 en `testes_eli/`. **Pendiente de informe** (`infopc --informe`) |
| **HP dv7** (2008-2009) | Athlon X2 QL-64 (SSE2), AMD RS780 + SB700, Radeon HD 3200 o HD 3450/4530 | ✓ USB (pendrive) | ✓ VESA; **3D Radeon R7xx** (HD 4530/4570 `1002:9553`, D402) con la **GART** (D406) encendida sola con el 3D desde la v8.5.5 (945 fps frente a 50, 15.000 cuadros sin fallos, 2026-11-19; el 3D sigue apagado por defecto); gpu3d v2 (D426) con las 15 pruebas en la máquina, también DXT1/3/5 en 1D en mosaico (formatos `0xf`, 2026-11-21) | ✓ teclado y touchpad PS/2 (ES) | ⚠ | ✓ **Realtek RTL8111/8168** (`10ec:8168`, PCI 08:00.0) con el driver r8169 experimental: `ifconfig eth0 experimental`, DHCP y `ping 8.8.8.8` (~7,5 ms) correctos (2026-11-07) | ⚠ HDA IDT 92HD75B: solo el subwoofer (2026-11-08); ajuste `idt71-hp` (GPIO0, D107) pendiente de probar con `sonido` | **Probado** con la imagen de `main` del 2026-11-07 (foto: `testes_hw/hp-dv7-red.jpg`). Pendiente de informe completo. D091: su revisión sigue con el inicio **clásico** (sin cambios); probar `entrega/prueba-realtek/` y fotografiar `ip` (línea «Chip:», XID) para confirmarla como probada |
| **Toshiba Satellite L850-1JR** (2012) | Core i7-3630QM (Ivy Bridge), PCH Intel serie 7 (`8086:1e..`), Intel HD 4000 | ✓ USB | ✓ VESA 1024x768x32 | ✓ | ⚠ | ⚠ **Realtek RTL8168evl/8111evl** (`10ec:8168` rev 07, XID 2c9, versión MAC 34): enlace y envío bien, la recepción solo ve una vuelta del anillo con datos viejos y el DHCP no responde (v6.0 beta 2, inicio clásico). D124: inicio **8168e** de Linux, DMA sin caché y volcado de las primeras tramas en `ip`; probar `entrega/prueba-realtek-8168e/`. ✗ Wi-Fi Atheros AR9485 (`168c:0032`) sin driver | ⚠ HDA `8086:1e20`, codec Realtek `10ec:0269` sin probar | **Probado** 2026-11-08 (`testes_hw/toshiba-l850-*.txt`: informe, `ip` y SaguiMark) |
| **Portátil Dell con Intel UHD 620** (Whiskey Lake-U, puente `8086:3e34`) | Core de 8.ª gen. (Whiskey Lake), 16 GB, Intel UHD 620 `8086:3ea0` | ✓ UEFI x64 (pendrive) | ✓ GOP 1920x1080x32 (WC por PAT); 2D por execlists: autoprueba bien pero las copias más lentas que la CPU (quedan en la CPU; desde D434 el motor queda para el volcado del búfer de atrás, a mano, igual que la CPU); **3D por GPU confirmado** con `gpu3d probar` (D438, D448, D453: autoprueba 8 de 8, 27 978 puntos; gpu3d v2 con 17 de 18 capacidades, mipmaps incluidos desde la revisión del 2026-11-21; **gpu3d v3 (D457) confirmado** el 2026-11-22 por el especialista y la revisión: autoprueba v3 0 píxeles distintos, `es11demo` mundo 1875 fps, Brumacanto 88,7 fps en ventana); apagado hasta `gpu3d probar` | — | ✓ pendrive | ✓ Ethernet USB Realtek (`if_ure`, D433; SSH) | — | **Probado** 2026-11-21 y 2026-11-22 por SSH con kernels de prueba (`arranque probar`): reinicia bien con D450 |
| **Fujitsu Amilo** (2006) | Core Duo T2400 (Yonah, SSE3), Intel 945PM/ICH7-M, Radeon Mobility X1400 (RV515, `1002:7145`, 128 MB) | ✓ disco SATA instalado | ✓ VESA 1024x768x32; **2D por GPU confirmado** (`acel2d probar --detalle`: 176 órdenes con los mismos píxeles que la CPU, 60 copias en 6,7 ms frente a 226 ms, 2026-11-18); ✓ **3D por GPU** (D287): autoprueba 8 de 8 tras D386 (348 mil triángulos/s, puntuación 2504); **volcado por GPU** (D387) confirmado y encendido solo desde la v8.5.5 (más de 20.000 volcados, 60.000 cuadros 3D con volcado, 2026-11-19) | ✓ | ✓ SATA (Patriot Burst) | ✓ Realtek `10ec:8169` (DEBUG-APP por SSH) | ✓ HDA `8086:27d8` (detectado) | **Probado** 2026-11-18 por DEBUG-APP (D362); 3D: 2026-11-20 con kernel de prueba, regresión de gpu3d v2 (D426) corregida: autoprueba y medida BIEN (puntuación 2455), `sgldemo` 91-99 fps, paisaje 13,7 fps por GPU frente a 3,0 por software, `es11demo` en ventana; v2 con una unidad (sin esténcil, niebla ni varias unidades). **gpu3d v3** (vértices por la GPU, D457): probado 2026-11-22 con kernel de prueba, autoprueba v3 BIEN y encendido de serie; mundo de `es11demo` 46 → 206 fps, Brumacanto (vista 64) 8,4 → 9,7 en ventana y 10,9 → 26,2 a pantalla completa con escala 50. Bluetooth: pincho clon CSR `0a12:0001` (BT 2.0) por UHCI se quedaba en «iniciando» (Command Status a SSP): arreglado en D389 junto con la recepción ACL por UHCI, falta probarlo |
| **PC sobremesa ASUS PRIME B450M-A** (2022) | Ryzen 5 4500 (Zen 2), AMD B450, Radeon RX 6400/6500 (Navi 24, `1002:743f`), BIOS AMI 3802 en modo CSM | ✓ USB por la BIOS (CSM) | ✓ VESA 1024x768x32 (VBE 3.0, AtomBIOS), WC por PAT | ✓ teclado (emulación PS/2 de la BIOS); ✗ ratón USB: **solo hay xHCI** (sin EHCI) | ✗ el pendrive desaparece tras arrancar (xHCI) → `/home` no se monta; SATA en AHCI | ⚠ Realtek `10ec:8168` (revisión moderna, ¿RTL8111H?): enlace y envío bien, **recepción descarta los paquetes** (RX 32, 31 descartados) y DHCP no responde. D091: inicio **moderno** para 8111g/h (sale del modo fuera de banda, desbloquea la recepción, RxConfig del Linux, ASPM apagado) y diagnóstico en `ip`. Probar `entrega/prueba-realtek/saguios-correccion.img`: `ifconfig eth0 experimental`, `dhcp`, foto de `ip`; si falla, también `saguios.img` (solo diagnóstico) | ⚠ HDA sin probar | **Probado** 2026-11-07 (fotos `testes_hw/ryzen-b450-*.jpg`). Necesita: driver xHCI + HID (046) y ajustar el driver Realtek a las revisiones nuevas. Con ratón PS/2 en el puerto combinado debería haber ratón |

En la columna «Sonido», «—» es que la combinación no tiene prueba propia (el driver no depende de
la CPU). En una máquina real, `infopc --informe` dice qué tarjeta y qué códec hay y qué salidas
encendió el driver (D089); `reproducir` con un WAV cualquiera confirma que se oye.

Cuando llegue un informe, su fila se completa aquí y el archivo se guarda en
`docs/informes/<maquina>.txt`, enlazado desde la tabla.

### Sensores y SaguiMark (mejoras 050 y 051)

| Máquina | Sensores (`sensores`) | SaguiMark 2 (total; no se compara con la 1) | Notas |
| --- | --- | ---: | --- |
| QEMU con KVM (`-cpu qemu32`) | «no disponible» (máquina virtual), sin errores | **1000** (referencia provisional) | La lectura de un MSR inexistente da #GP y se absorbe (`tests/h71_sensores.py`) |
| QEMU sin KVM (TCG) | «no disponible», sin errores | ~76 (no representativo; grupo 2D ~165) | El TCG devuelve 0 en los MSR desconocidos, sin #GP |
| QEMU `-cpu pentium3` | «no disponible» | parcial, SIMD «n/d» (y 2D «n/d» sin escritorio) | `tests/h72_saguimark.py` |
| Lenovo G470 (Sandy Bridge) | ⚠ previsto: MSR 0x19C con TjMax del 0x1A2, APERF/MPERF, turbo | pendiente | |
| HP dv7 (Athlon X2 QL-64, familia 11h) | ⚠ previsto: PCI 00:18.3 0xA4 (Tctl), límite HTC, P-states | pendiente: **será la referencia (1000)** | |
| ASUS B450 (Ryzen 5 4500, 17h) | ⚠ previsto: SMN 0x59800 (Tctl), P-states, CPB | pendiente | |
| Amilo (Core / Core 2) | ⚠ previsto: MSR 0x19C con TjMax por tabla (aprox.) | pendiente | |

### Control de frecuencia (mejora 061, D106)

| Máquina | Mecanismo | Estado | Notas |
| --- | --- | --- | --- |
| QEMU (con y sin KVM) | — | «no disponible (máquina virtual)», sin errores | Lógica probada con el driver simulado (`tests/h78_frecuencia.py`) |
| HP dv7 (Athlon X2 QL-64, 11h) | `PStateCtl` (0xC0010062), P0 2100 / P1 1050 MHz | ⚠ previsto: P0 al arrancar (la BIOS deja P1) | Pruebas en `testes_hw/hp-dv7-frecuencia-pruebas.txt` |
| Lenovo G470 (Sandy Bridge) | `IA32_PERF_CTL` (multiplicador, sin turbo) | ⚠ previsto | |
| ASUS B450 (Ryzen 5 4500, 17h) | `PStateCtl` | ⚠ previsto | |
| Amilo (Core 2) | — | «no disponible»: su `PERF_CTL` lleva el VID (tensión), no se toca | Con `_PSS` de ACPI (030) |

Si algo va mal: `frecuencia --desactivar` (al momento), `desactivar=si` en `/home/.frecuencia`
(desde el arranque siguiente) o una imagen con `mkimagem.py --sin-frecuencia`.

Para medir un PC: `saguimark` (modo preciso) con `/home` persistente o un pendrive, y enviar
`/home/saguimark-AAAAMMDD-HHMM.txt` junto con el informe; su última línea va a
`rootfs/lib/saguimark/referencias.txt`. El informe (`infopc --informe`) ya incluye los
sensores y el último SaguiMark.

## Lo que SaguiOS hace solo en un PC antiguo (fase A de la mejora 036)

- **CPU sin SSE2** (Pentium III, Athlon/Athlon XP): el kernel y la libsagui copian con
  `rep movsd`; `cc` genera código x87 (`-sse2` lo fuerza y avisa); `simd` no prueba SSE2 (D076).
- **CD arrancable** para los PC que no arrancan desde USB: `make iso`, grabar
  `saguios.iso` (D077, `docs/como-gravar.md`, sección 6).
- **Vídeo a 16 y 15 bits** si la tarjeta no ofrece 24 ni 32, con 640x480 como reserva (D078).
- **BIOS sin lectura LBA**: el cargador lee por CHS (D077).
- **Pendrive USB sin congelar el sistema** (D095): la espera del DMA duerme hasta la IRQ del
  EHCI (la línea PCI que deja la BIOS). Si en un PC real esa IRQ no llega, el controlador pasa
  solo a ceder la CPU y volver a mirar (más lento y con más CPU, pero sin congelar nada); el
  comando `usb` dice en qué modo está cada controlador y cuántas interrupciones llegaron. En
  QEMU con KVM: ~5,6 MB/s de lectura y ~2,9 MB/s de escritura (antes, 50 y 20 KB/s). **Sin
  medir en el G470** ni en otro PC real.

Pendiente: `python` con números reales en CPU sin SSE2 (falla con «este procesador no tiene
SSE2»), procesadores sin CMOV (Pentium, K6: fase C) y los drivers de la fase B (USB 1.1,
e100, 3Com, VIA Rhine, IDE con DMA, AC'97, AHCI, ACPI).

## Cómo generar y enviar un informe

1. Arranca SaguiOS en la máquina (pendrive o CD).
2. Si arrancas desde CD o quieres llevarte el informe, conecta un **pendrive FAT32**: aparece
   en `/usb`.
3. Abre el **Terminal** (o, sin escritorio, en la pantalla de texto) y escribe:

   ```
   infopc --informe
   ```

   SaguiOS escribe `informe-AAAAMMDD-HHMM.txt`:
   - en `/home`, si la imagen tiene partición de datos (`make imagem-persistente`);
   - si no, en el pendrive (`/usb`);
   - si no hay ninguno, en `/tmp`, que está en la RAM, y avisa: conecta un pendrive y repite.
   - con una partición `SAGUI-LOGS` (`make imagem-particoes`, docs/imagenes.md), deja además una copia en `/logs`, junto al registro del kernel de cada arranque.

   `infopc --informe /usb/mipc.txt` elige el nombre.
4. Antes de quitar el pendrive: `desmontar` (o «Extraer» en Archivos).
5. Envía el `.txt` (adjunto en una incidencia o por correo) con:
   - el modelo de la máquina;
   - cómo arrancó (USB, CD, disco);
   - qué funcionó y qué no;
   - si se colgó antes de poder escribir el informe, una **foto de la pantalla**: los
     mensajes `[1/6]` a `[6/6]` del cargador dicen dónde se detuvo.

### Qué contiene el informe

Es texto en ISO-8859-1 (Latin-1), sin datos personales (el número de serie de SMBIOS se omite):

| Sección | Contenido |
| --- | --- |
| Resumen | SSE2 sí/no, modo de vídeo, dispositivos PCI con y sin driver |
| CPU | fabricante, familia/modelo, microarquitectura, frecuencia, cachés, recursos (FPU, PAE, MTRR, PGE, CMOV, PAT, MMX, SSE, SSE2...) |
| Memoria | el mapa E820 de la BIOS y lo que gestiona el kernel |
| Placa/BIOS | SMBIOS: BIOS, sistema, procesador, módulos de memoria |
| PCI | cada función: `bus:disp.fun`, `fabricante:modelo`, subsistema, clase, revisión, IRQ y lo que se sabe: `driver: X (soportado/experimental)`, `sin driver`, `no necesita driver` o `sin driver, previsto: mejora 029/030/036` |
| Vídeo | modo actual, VBE, write-combining, cambio en caliente y **todos los modos VESA** aceptados, con su profundidad |
| Sistema | versión, memoria, discos USB |
| Arranque | disco de la BIOS, si fue desde CD, si la BIOS leyó por LBA o por CHS, preferencias |
| Discos y USB | discos ATA y USB con particiones, montajes y el estado de los controladores USB |
| ACPI | el RSDP y las tablas (firma, dirección, tamaño, OEM), sin interpretarlas |
| CPU: sensores | temperatura y su fuente, límite (TjMax o HTC), throttling, frecuencia real y nominal, multiplicador, P-states, VID, turbo y microcódigo (D092; «no disponible» lo que no exista) |
| SaguiMark | el resumen del último resultado guardado en `/home` o `/tmp` (D093) |
| Log del kernel | las últimas 200 líneas (lo que saldría por el puerto serie) |

### SaguiMark en hardware real

| Máquina | Versión | Total | Enteros | x87 | SIMD | Memoria | Notas |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| HP dv7 (Athlon X2 QL-64, 2,1 GHz) | SaguiMark 1, rápido, 2026-11-08 | 64 | 81 | 161 | 45 | 29 | CPU en el **P-state bajo** (sin la 061); en esta familia (K8/Griffin) el SSE2 rinde menos que x87. Referencia 1000 = QEMU/KVM en Ryzen 9 9900X |

### Informe completo del HP dv7 (2026-11-08)

Resumen en `testes_hw/hp-dv7-informe-20261108.txt`. Lo más relevante:
- **Red** RTL8168d/8111d: **funciona y se activa de serie** (D091, versiones 25/26 confirmadas).
- **Sonido** HDA con códec IDT 111d:7603 (92HD75B): **solo suena el subwoofer**; el driver eligió
  los pines 0x0d y 0x0f. Causa probable: falta el GPIO0 del códec, que en los HP con IDT enciende
  el amplificador de los altavoces. **Ajuste aplicado (D107, rama `sonido-dv7`, pendiente de
  probar):** `idt71-hp` de Linux (GPIO0 = 1, tomas 0x0a y 0x0f como auriculares, altavoces
  callados con auriculares enchufados). Qué probar y qué mandar:
  `testes_hw/hp-dv7-sonido-pruebas.md` (programa `sonido`).
- **CPU** en P1 (1050 MHz) de 2: mejora 061 (v5.1).
- **Disco** SATA en AHCI (v6), **USB 3** Fresco Logic (xHCI, v8), 4 OHCI sin driver (029).

### Toshiba Satellite L850-1JR (Core i7-3630QM, 16 GB) - 2026-11-08

- **Red** Realtek RTL8168evl/8111evl (10ec:8168 rev 07, XID 2c9, versión 34): **funciona y se activa de
  serie** desde la v6.0 (D124; perfil 8168e, RxConfig 0x870E). DHCP y ping a Internet en ~7 ms.
- Wi-Fi Atheros AR9485 (168c:0032): sin driver (mejora 072, v8). USB 3 (xHCI): sin driver (046, v8).
- Sonido HDA con códec Realtek ALC269: con driver. Frecuencia: control Intel (EIST) activo, sin turbo
  (turbo opcional en la v6.1).
- **SaguiMark 2: 311** (KVM/QEMU de referencia = 1000; HP dv7 = 64).

## Ethernet USB Realtek RTL8152, RTL8153 y RTL8156 (D433, experimental)

El driver `kernel/rede/ure.c` reconoce los adaptadores USB de Realtek de la tabla de OpenBSD (60 VID:PID, por ejemplo
`0bda:8152`, `0bda:8153` y `0bda:8156`) y los pone como la interfaz `eth0` (`ure`) si no hay tarjeta PCI ni Wi-Fi activos
(para probarlo en un equipo con red integrada, como el Amilo: `make URE_PREFERIR=1`).
**Sin probar en hardware real** (el RTL8156 del Amilo está pendiente). Detalle y cómo probarlo, en [`ure.md`](ure.md).
