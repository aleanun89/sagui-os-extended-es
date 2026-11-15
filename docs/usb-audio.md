# Audio USB y transferencias isócronas (v8.1, D275)

Esta página explica cómo suena un altavoz o unos auriculares USB en SaguiOS y cómo está hecha la base isócrona
de la pila USB. Lo decidido está en la D275 (`docs/decisoes.md`); la prueba, en `tests/h275_audio_usb.py`.

## Lo que ve el usuario

- Al enchufar un dispositivo de audio USB de clase Audio 1.0 (auriculares, altavoces, tarjetas de sonido USB) el kernel escribe
  `[UAC] ...: salida PCM 16 bits estereo a 48000 Hz, endpoint N` y la tarjeta pasa a ser la salida de sonido. Al retirarlo
  vuelve la tarjeta anterior (HDA o AC'97) o, si no hay otra, los programas siguen sin colgarse (el sonido se tira).
- `volumen salida` lista las tarjetas (la actual lleva `*`); `volumen salida N` cambia de salida con el sonido en marcha.
  El resto (`volumen`, `reproducir`, el mezclador, las aplicaciones, el ecualizador) funciona igual con cualquier tarjeta.
- `usb` muestra una línea `Audio USB:` con los paquetes puestos y completados, los huecos y los errores del flujo isócrono.
- El silencio también se manda a la unidad de función del dispositivo; el volumen lo hace el mezclador y el amplificador del
  dispositivo se deja al máximo.

## La API isócrona (`kernel/inc/usb.h`)

```c
usb_iso_cfg_t cfg = { .ep = 1, .entrada = false, .mps = 192, .intervalo = 1,
                      .anel_fis = fis, .anel_tam = bytes, .bytes_s = 48000 * 4, .tam_quadro = 4 };
int h = usb_iso_abrir(d, &cfg);      /* xHCI: Configure Endpoint; EHCI/UHCI/OHCI: reserva de ancho de banda */
usb_iso_ligar(d, h);                 /* el controlador recorre el anillo desde el principio */
uint32_t bytes = usb_iso_posicao(d, h);   /* completados; también reabastece la cola */
usb_iso_parar(d, h);  usb_iso_fechar(d, h);
```

Al abrir se debe haber activado ya la alternativa de la interfaz en el controlador xHCI (el Configure Endpoint va **antes**
del SET_INTERFACE). Los paquetes se reparten con `usb_iso_calcular` (cuadros enteros, acumulando el resto). La cola es de
`USB_ISO_COLA_MS` milisegundos; hay que llamar a `usb_iso_posicao` más a menudo que eso (el audio lo hace en cada tic).

| Controlador | Cómo | Estado |
| --- | --- | --- |
| xHCI | un TRB Isoch con SIA por milisegundo; evento cada 8 TRB (BEI) | probado en QEMU (h275 xhci, caliente, mezcla) |
| UHCI | 64 TD isócronos (bit IOS) en la lista de cuadros | probado en QEMU (h275 uhci, companero) |
| OHCI | ED isócrona + cola de TD de un paquete con su cuadro de inicio | probado en QEMU (h275 ohci) |
| EHCI | 64 iTD de 8 microcuadros en la lista periódica (o 64 siTD si el dispositivo es de plena velocidad tras un hub de alta velocidad, D277) | **sin probar** (el QEMU 8.2 no deja colgar el altavoz de un EHCI ni reparte siTD) |

## El driver UAC 1 (`kernel/usb/usbaudio.c`)

Recorre el descriptor de configuración (AudioControl y las alternativas de AudioStreaming), elige PCM de 16 bits y dos canales
con la frecuencia más cercana a 48 kHz, abre el flujo con un anillo de 32768 cuadros, activa la alternativa y se registra en
el mezclador con `audio_registrar`. Pendiente: mono y 24 bits, UAC 2, micrófonos y varias tarjetas USB a la vez.

## Qué falta

Hecho en la D277 (`docs/camara.md`): **entrada isócrona** en los cuatro controladores (casillas de `mps * mult` bytes y `usb_iso_ler`; el micrófono
USB, que usaría esa misma API, queda sin hacer) y el **siTD** del EHCI para plena velocidad tras un hub de alta velocidad (salida y entrada, sin probar).
Sigue sin probarse el iTD con un dispositivo de alta velocidad (el QEMU 8.2 no lo emula): se prueba con una cámara real.
