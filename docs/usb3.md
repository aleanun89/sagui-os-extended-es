# USB 3.0 (xHCI): lo que falta y el camino más corto

> **Estado (v7.3, D214):** el plan de abajo está hecho. `kernel/usb/xhci.c` enumera y transfiere
> (control, bulk e interrupción), `usb.c` reparte entre EHCI y xHCI, y `kernel/usb/hid.c` lleva el
> teclado y el ratón USB por los dos controladores. Lo que sigue queda como explicación del xHCI; lo
> decidido y lo que falta (isócronas, *streams*, MSI-X, energía) está en la D214. Prueba: `h214`.

Este documento es el plan del USB 3.0 para el SaguiOS. Está escrito a partir
de lo que ya hay en el repositorio (`kernel/usb/`), de lo que dice la
especificación xHCI y de cómo está emulado el controlador en el QEMU, que es
donde se puede probar sin hardware.

## 1. La situación

Hoy el SaguiOS tiene USB 2.0:

| Fichero | Qué hace |
| --- | --- |
| `kernel/usb/ehci.c` | Driver del controlador EHCI: los anillos (la lista periódica y la asíncrona), las cabezas de cola (QH), los descriptores de transferencia (qTD) y las transferencias por DMA. |
| `kernel/usb/usb.c` | La enumeración: los hubs, los descriptores, las direcciones de los dispositivos, la conexión y la retirada en caliente. |
| `kernel/usb/armazenamento.c` | La clase de almacenamiento: el pendrive que aparece como `usb0` en la capa de discos. |
| `kernel/inc/usb.h` | La API que comparten: `usb_controle()`, `ehci_bulk()`, `usb_disp()`, etc. |

El EHCI es el controlador del **USB 2.0** (480 Mb/s). El USB 3.0 (5 Gb/s, y
además USB 2.0 con más energía) lo lleva **otro** controlador distinto: el
**xHCI**. No vale el EHCI: son dos hardware diferentes, con registros
diferentes y un modelo de programación completamente distinto.

Los Miscellaneous los que se ven en los PC de verdad:

- Los Intel de 6ª generación en adelante (y los Ryzen) llevan un xHCI. En un
  PC con ese controlador, el pendrive USB 2.0 que se enchufe funciona (el
  xHCI también habla USB 2.0), pero un pendrive USB 3.0 no funciona: no hay
  quien lo hable.
- Los equipos con dos controladores (un xHCI y un EHCI, como el HP dv7 de las
  pruebas de hardware) tienen las dos cosas: el EHCI para el USB 2.0 y el
  xHCI, que hoy no tiene driver.

## 2. Qué hay que escribir: el xHCI por dentro

### 2.1 El bus PCI

El xHCI es un dispositivo del PCI:

- Clase `0x0C` (bus de serie), subclase `0x03` (USB). La interfaz (prog-if)
  es `0x04` en los xHCI de Intel y `0x30` en el de QEMU y el de Red Hat
  (`1b36:0001`, `1b36:000d`).
- La barra 0 es de **memoria** (MMIO), normalmente de 64 bits, y el
  espacio de registros es grande (de 64 KB a 1 MB). En el SaguiOS se mapea
  con `janela_mapear()` (D008: el kernel ve la memoria física 1:1, así que
  los punteros de DMA son las direcciones físicas tal cual).

### 2.2 Los registros

Los **registros de capacidad** están en el offset 0 de la barra:

| Offset | Registro | Qué dice |
| --- | --- | --- |
| 0x00 | CAPLENGTH (byte) y HCIVERSION | Longitud de los registros de capacidad (0x40) y versión de la especificación. Los **registros operacionales empiezan en CAPLENGTH**. |
| 0x04 | HCSPARAMS1 | Puertos (bits 31:24), interrupteurs (18:8) y ranuras de dispositivo (7:0). |
| 0x08 | HCSPARAMS2 | Tamaño máximo de la tabla de segmentos del anillo de eventos (bits 7:0). |
| 0x10 | HCCPARAMS1 | Si el controlador usa direcciones de 64 bits (bit 0). |
| 0x14 | DBOFF | Desplazamiento de la **matriz de campanas** (doorbell). |
| 0x18 | RTSOFF | Desplazamiento de los **registros de ejecución**. |

Los **registros operacionales** (offset CAPLENGTH = 0x40):

| Offset | Registro | Qué hace |
| --- | --- | --- |
| +0x00 | USBCMD | Bit 0: arrancar/parar; bit 1: reiniciar el controlador; bit 2: interrupciones globales. |
| +0x04 | USBSTS | Estado (bit 0: parado; bit 3: evento pendiente). |
| +0x14 | DNCTRL | Control de notificaciones de los dispositivos. |
| +0x18 | CRCR | El **anillo de comandos**: puntero y estado (bit de ciclo). |
| +0x30 | DCBAAP | Puntero a la **matriz de contextos de dispositivo** (DCBAA). |
| +0x38 | CONFIG | Cuántas ranuras de dispositivo están habilitadas. |
| +0x400 + 0x10*n | PORTSC | Un registro por puerto: hay algo conectado, velocidad, corriente, cambios. |

Los **registros de ejecución** (offset RTSOFF): el índice del tiempo
(MFINDEX, 0x00), la **tabla de interrupteurs** (0x20 + 0x20*n, con IMAN,
IMOD, ERSTSZ, ERSTBA y ERDP) y, en DBOFF, la **matriz de campanas**: una
campana de 32 bits por ranura y por endpoint.

### 2.3 Los tres anillos y los TRB

Todo en el xHCI gira alrededor de los **TRB** (Transfer Request Blocks), de
16 bytes: `parameter` (8 bytes), `status` (4) y `control` (4). El campo
`control` lleva el **tipo** (bits 31:10) y el **bit de ciclo** (bit 0), que
sirve para saber si un TRB es viejo o nuevo (el anillo es circular y el
controlador va dando la vuelta).

1. **Anillo de comandos** (CRCR): el host pone aquí las órdenes del
   controlador (habilitar ranura, dar dirección, configurar un endpoint,
   evaluar un contexto...). El controlador las ejecuta y escribe el
   resultado en el **anillo de eventos**. Los tipos que hacen falta:
   `Enable Slot`, `Address Device`, `Configure Endpoint`, `Evaluate Context`,
   `Reset Endpoint`, `Stop Endpoint`, `Set TR Dequeue Pointer`, `Reset
   Device`, `No-op` y `Link` (para encadenar segmentos).
2. **Anillo de eventos**: el controlador escribe aquí los eventos
   (`Command Completion`, `Port Status Change`, `Host Controller Event`...).
   La tabla de segmentos (ERST) dice dónde está cada segmento del anillo.
3. **Anillos de transferencia**: uno por endpoint (y por flujo). El host
   escribe ahí los TRB de la transferencia (`Setup`, `Data`, `Status` para
   control; `Normal` para el resto) y **toca la campana** (doorbell) del
   endpoint para que el controlador la ejecute.

Los **contextos** son la memoria que describe cada dispositivo: la DCBAA
(una entrada de 64 bits por ranura) apunta al contexto del dispositivo, que
tiene el contexto de la ranura y los contextos de los endpoints (con su
tamaño máximo de paquete, su tipo y su anillo de transferencia).

### 2.4 Las interrupciones

El controlador avisa de dos maneras:

- **INTx**: la línea de IRQ del PCI (la de siempre, `irq_registrar_extra()`).
- **MSI-X**: una tabla de vectores en la barra del PCI (una capacidad PCI
  opcional). Es lo que se usa en los PC de verdad y lo que menos IRQ
  compartidas deja.

Las dos se pueden empezar con INTx (más fácil) y pasar a MSI-X después.

## 3. Qué se puede reaprovechar y qué hay que escribir de cero

| Pieza | De dónde sale | Comentario |
| --- | --- | --- |
| Enumeración (hubs, descriptores, dirección, conexión en caliente) | `kernel/usb/usb.c` | La lógica de qué hacer con un dispositivo se reutiliza entera; lo que cambia es el transporte: en vez de `ehci_controle()` y `ehci_bulk()`, el xHCI hace lo mismo con TRB. |
| Clase de almacenamiento (el pendrive como `usb0`) | `kernel/usb/armazenamento.c` | Se reutiliza tal cual en cuanto el xHCI sepa hacer transferencias de masa: el `disco_registrar("usb", ...)` es el mismo. |
| Memoria para el DMA | `memfisica_alocar_seguidas()` y `janela_mapear()` | Igual que en el EHCI: el kernel ve la memoria física 1:1 (D008). |
| Tiempo (esperar sin spinning) | `ehci_esperar_ms()` (FRINDEX) y `relogio_tiques()` | El xHCI tiene su propio reloj, MFINDEX, en los registros de ejecución. |
| Interrupciones INTx | `irq_registrar_extra()` (como el EHCI) | MSI-X hay que hacerlo nuevo (capacidad PCI del MSI-X y su tabla). |
| El driver del xHCI | **nuevo, entero** | `kernel/usb/xhci.c`: anillos, TRB, contextos, campanas, tipos de transferencia. No hay nada reutilizable tal cual: el modelo es otro. |

Lo que **no** se puede reaprovejar es el núcleo del EHCI: allí el host
describe la transferencia en una QH con sus qTD y el controlador la ejecuta;
en el xHCI el host pone TRB en un anillo y toca una campana.

## 4. Las etapas, y qué se puede comprobar en cada una

**Etapa 1 - Detectar el controlador y leer sus capacidades** (hecho, D155).
`kernel/usb/xhci.c` busca el xHCI en el PCI, mapea la barra, lee CAPLENGTH,
HCSPARAMS1, DBOFF y RTSOFF, reserva en memoria del kernel la DCBAA, el
anillo de comandos, el anillo de eventos y la tabla de segmentos, programa
sus punteros y enciende el controlador (`USBCMD.Run`).
*Se comprueba*: el log dice qué controlador se ha encontrado, cuántos
puertos y ranuras tiene y qué falta (la enumeración). Prueba `h131`.

**Etapa 2 - El anillo de comandos y el anillo de eventos.** Escribir un TRB
`Enable Slot` en el anillo de comandos, tocar la campana 0 y esperar el
evento `Command Completion` en el anillo de eventos.
*Se comprueba*: el log dice "ranura 1 habilitada" (con el código de
completado y el identificador de la ranura que devolvió el controlador).

**Etapa 3 - Dar dirección al dispositivo.** Montar el contexto de entrada
(con el contexto de la ranura y el del endpoint de control), escribir el TRB
`Address Device` y, cuando el controlador avise, leer el contexto del
dispositivo (dirección, velocidad, tamaño máximo de paquete).
*Se comprueba*: el log dice la dirección que le ha dado el controlador y la
velocidad del dispositivo.

**Etapa 4 - Transferencias de control (leer los descriptores).** Con el
endpoint de control ya configurado, mandar los TRB `Setup`, `Data` y
`Status` de una petición de control y leer la respuesta.
*Se comprueba*: el log enseña los descriptores del dispositivo (el
fabricante, el producto, los endpoints). A partir de aquí, la enumeración
de `usb.c` (los hubs, los descriptores) se puede reutilizar.

**Etapa 5 - Los anillos de transferencia y el almacenamiento.** Configurar
el endpoint de masa (`Configure Endpoint`), poner TRB `Normal` en su anillo
de transferencia, tocar la campana del endpoint y leer los 16 sectores
iniciales del pendrive (el `READ CAPACITY`).
*Se comprueba*: el pendrive aparece en la capa de discos como `usb0` y sus
particiones se ven, exactamente igual que con el EHCI.

Después de la etapa 5 quedan: los hubs (el xHCI trae el hub dentro, pero
los hubs externos también hay que enumerarlos), la conexión en caliente, las
interrupciones MSI-X, los tipos isócronos (audio y vídeo por USB) y el USB
Type-C.

## 5. Cómo se prueba

En el QEMU, el controlador es el `qemu-xhci` (o `nec-usb-xhci`):

```
-device qemu-xhci,id=xhci
-drive if=none,id=pen,format=raw,file=IMAGEN
-device usb-storage,bus=xhci.0,drive=pen
```

La prueba sería `tests/h132_usb3_xhci.py`, siguiendo la de la etapa 1
(`tests/h131_sata_ahci.py`): arrancar, esperar al prompt y mirar el log.

## 6. Avisos y riesgos

- **El QEMU se aparta de la especificación en un par de sitios.** La tabla de
  segmentos del anillo de eventos (ERST) y las unidades de DBOFF y RTSOFF
  (bytes o unidades de 32 bytes) no son las que dice la especificación.
  El esqueleto (D155) lo hace con el formato del QEMU y lo apunta con un
  TODO: hay que comprobarlo en un xHCI de verdad antes de dar por buena la
  etapa 2.
- **Los anillos son circulares y hay que mirar el bit de ciclo** en cada TRB:
  si se olvida, el controlador lee TRB viejos y se cuelga.
- **El doorbell es lo único que despierta al controlador**: si no se toca la
  campana, el comando no se ejecuta nunca. Es el error más fácil de cometer.
- **Los endpoints de control son de un solo uso por cada dirección**: el
  `DATA` va en el endpoint de salida y la respuesta en el de entrada.
- Las interrupciones MSI-X necesitan la capacidad PCI del MSI-X y su tabla
  en la barra; el EHCI no la usa, así que no hay código de ese que copiar.
