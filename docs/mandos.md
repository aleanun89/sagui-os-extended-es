# Mandos y joysticks (D307, v8.3.3)

SaguiOS reconoce los mandos y joysticks USB y el joystick analógico del puerto de juegos, y los
ofrece a los programas por una sola interfaz: la libsagui (`mandos.h`), las SDL 1.2 y 2
(`SDL_Joystick`, `SDL_GameController`) y DOSBox.

## Qué funciona

| Mando | Cómo se conecta | Qué se ve |
| --- | --- | --- |
| Mandos y joysticks USB **HID** genéricos (clase 3, aplicación Joystick, Game Pad o Multi-axis) | UHCI, OHCI, EHCI o xHCI; directo o detrás de un hub | hasta 8 ejes (X, Y, Z, Rx, Ry, Rz, deslizadores, rueda, timón, acelerador...), 4 hats y 32 botones, según el descriptor |
| **Logitech Dual Action**, RumblePad 2, **F310/F710 en modo D** (DirectInput) | USB HID | 4 ejes, 1 hat (cruceta), 12 botones; mapeo de `SDL_GameController` conocido |
| **Mando de Xbox 360 con cable** (y el F310 en modo X, y los compatibles con XInput por cable) | USB, clase propia 0xFF/0x5D/0x01 (no es HID) | 6 ejes (stick izquierdo, gatillo izquierdo, stick derecho, gatillo derecho), 11 botones, la cruceta como hat |
| **Joystick analógico** del puerto de juegos (2 o 4 ejes, 4 botones) | puerto 0x201 de la placa ISA o de la tarjeta de sonido (Sound Blaster...) | 2 o 4 ejes y 4 botones |

Varios mandos a la vez (hasta 8) y conexión y desconexión en caliente de los USB. **No** se reconocen
(todavía): los mandos Bluetooth (el HID Bluetooth solo lleva teclado y ratón, D279), el receptor
inalámbrico del Xbox 360 (otra clase, 0xFF/0x5D/0x81), los mandos de Xbox One/Series (protocolo GIP),
los DualShock 3/4 (HID con informes propios: se ven como mando genérico con el mapeo que dé su
descriptor), la vibración y los joysticks digitales del puerto de juegos (Gravis GrIP, Sidewinder).

## Probarlos

- **Configuración > Mandos**: la lista de mandos conectados; al elegir uno, la prueba en vivo dibuja
  cada par de ejes como un punto en un cuadro, los botones numerados (azules al pulsarlos) y los
  hats como una cruz. «Buscar joystick» vuelve a mirar el puerto de juegos.
- En el terminal, la orden `mandos`:

```
mandos                       # la lista: nombre, tipo, vía (USB xHCI, puerto 0x201...), ejes, botones, hats
mandos --probar 0 20         # el estado del mando 0 en vivo durante 20 s
mandos --estado 1            # una lectura del mando 1, en una línea
mandos --detectar            # volver a buscar el joystick del puerto de juegos
```

Los ejes van de -32768 a 32767, con X a la derecha e Y hacia abajo positivos (como en SDL); los
botones se numeran desde 1.

## El joystick del puerto de juegos

El puerto de juegos no tiene interrupción ni registros: cada eje es un potenciómetro que carga un
condensador, y el tiempo que tarda en bajar su bit (de ~24 us a ~1,1 ms) da la posición. SaguiOS lo
mide con el reloj del procesador (TSC) con las interrupciones apagadas durante la medida (como mucho
3 ms, normalmente ~1 ms), y solo cuando un programa lo lee, como mucho cada 20 ms.

- **Detección**: al arrancar y con «Buscar joystick» (o `mandos --detectar`). Sin puerto o sin joystick
  no hay ningún eje que baje, y no aparece ningún mando. Hay que enchufarlo antes de buscarlo: el
  puerto no avisa de la conexión. Si deja de responder (desenchufado), se retira solo.
- **Calibración automática** (por defecto): la posición al detectarlo es el centro; los extremos se
  amplían solos al mover el joystick hasta los topes. Basta con mover el joystick a las cuatro esquinas
  una vez después de arrancar.
- **Calibración a mano** (mejor para joysticks viejos con el centro desplazado):
  Configuración > Mandos > «Calibrar puerto»: mueve el joystick hasta las cuatro esquinas varias veces
  durante 6 s y después suéltalo en el centro. O en el terminal: `mandos --calibrar`.
  Se guarda en `/home/.config/sagui.conf` (clave `mando_puerto=`, mínimo, centro y máximo de cada eje
  en décimas de microsegundo, separados por `/`) si /home es persistente, y se aplica en cada arranque
  (`configuracion --aplicar`). «Calibración automática» (o `mandos --calibracion auto`) la quita.

```
mandos --calibracion                                  # la que hay
mandos --calibracion 300,5800,11500/300,5900,11600/0,0,0/0,0,0   # ponerla a mano
mandos --calibracion auto
```

## En los juegos

- **SDL 1.2** (DOSBox, OpenJazz): `SDL_NumJoysticks`, `SDL_JoystickOpen`, los ejes, botones y hats y los
  eventos `SDL_JOYAXISMOTION`, `SDL_JOYHATMOTION` y `SDL_JOYBUTTONDOWN/UP`.
- **SDL 2** (GNUBoy, Vanilla Conquer...): lo mismo con identificadores de instancia, conexión en caliente
  (`SDL_JOYDEVICEADDED/REMOVED`, `SDL_CONTROLLERDEVICEADDED/REMOVED`) y **`SDL_GameController`** con el
  mapeo estándar: el del Xbox 360, el de los Logitech en modo D y uno genérico para los demás (botones
  1-4 = A, B, X, Y; 5-6 hombros; 7-8 gatillos; 9 Back; 10 Start; 11-12 sticks; ejes 1-2 el stick
  izquierdo y 3-4 el derecho; el primer hat es la cruceta). Los mapeos no se pueden cambiar
  (`SDL_GameControllerAddMapping` no hace nada) y el mando no vibra.
- **DOSBox**: con `joysticktype=auto` en `[joystick]` (lo que escribe el `dosbox.conf` nuevo) emula el
  joystick del PC con el primer mando (4 ejes) o con los dos primeros (2 joysticks de 2 ejes). Un
  `dosbox.conf` anterior a la v8.3.3 tiene `joysticktype=none`: cambia esa línea. En **Recreativa**,
  Propiedades del juego > «Joystick»: auto, 2axis, 4axis, fcs (Thrustmaster), ch (CH Flightstick) o none.
- **GNUBoy**: el mando funciona solo (cruceta, A, B, Start, Back = Select).

## Para programar

```c
#include "mandos.h"
info_mando_t l[MANDOS_MAX];
unsigned gen;
int n = mandos_listar(l, MANDOS_MAX, &gen);   // l[i].nome, .tipo, .via, .n_eixos, .n_botoes, .n_hats, .mapa
estado_mando_t e;
if (mando_ler(0, &e) == 0 && e.id == l[0].id) {
    // e.eixos[k] de -32768 a 32767; (e.botoes >> b) & 1; e.hats[h] con MANDO_HAT_CIMA/DIREITA/BAIXO/ESQUERDA
}
```

Por debajo es la llamada `info_sistema(INFO_MANDOS, &info_mandos_t, sizeof)` (`kernel/inc/abi.h`,
número 77) con las órdenes `MANDOS_LISTA`, `MANDOS_ESTADO`, `MANDOS_DETECTAR`, `MANDOS_CALIBRAR` y
`MANDOS_INYECTAR` (solo en las imágenes de prueba). Se lee por sondeo, en cada cuadro: `gen` cambia al
conectar o desconectar un mando.

## Pruebas

`tests/h307_mandos.py`: la prueba de unidad del analizador de descriptores HID con descriptores reales
(Dual Action/F310, joystick genérico DragonRise, uno con Report ID) y de las cuentas del puerto de
juegos; y en el QEMU, que no emula ningún joystick USB, dos **mandos de prueba** del kernel
(`mkimagem.py --mandos-prueba`: un Dual Action cuyo descriptor pasa por el analizador y un Xbox 360)
a los que se mandan informes crudos con `mandos --inyectar`: la lista, el estado, la calibración, la
página de Configuración, GNUBoy (SDL 2) y DOSBox (SDL 1.2). El puerto de juegos se prueba en 86Box
(`tests/emu_86box_mandos.py`, fuera de la batería).

## Qué probar en hardware real

- Un mando USB genérico y un Logitech F310 (en modo D y en modo X) en un USB 1.1 (UHCI u OHCI), en un
  EHCI con hub y en un xHCI: `mandos`, Configuración > Mandos, conectar y desconectar con un juego
  abierto.
- Un mando de Xbox 360 con cable (original o compatible).
- Un joystick analógico en el puerto de juegos de una Sound Blaster o de la placa: la detección, que
  los ejes recorran todo el rango tras calibrar y que el sistema no dé tirones mientras se lee.
