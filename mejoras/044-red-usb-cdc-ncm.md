# 044 - Red por USB: driver genérico CDC-NCM/ECM (Realtek RTL8156 y similares)

- **Estado:** aprobada; en cola, delante de la 042 por utilidad (red para los tres portátiles)
- **Prioridad:** alta
- **Alta:** 2026-11-07

## Problema / motivación

Ninguno de los portátiles de prueba tiene Ethernet con driver seguro:
- G470: Atheros, sin driver;
- HP dv7: Realtek 810x experimental, o Marvell sin driver;
- Amilo Pi 1536: chip desconocido.

Todos tienen USB 2.0 (EHCI), que SaguiOS ya maneja. El mantenedor tiene un adaptador **USB
2.5GbE Realtek RTL8156** (`0bda:8156`, bcdDevice 0x31F4, «USB 10/100/1G/2.5G LAN»), y como
este hay muchos.

## Descriptores del adaptador (capturados en Windows con USB Tree View, 2026-11-07)

- 3 configuraciones; `bMaxPacketSize0` 9 (porque estaba en SuperSpeed; en EHCI será
  High-Speed).
- **Configuración 1:** interfaz 0 de clase 0xFF/0xFF (modo propio de Realtek, el del driver
  `rtucx22x64` de Windows o `r8152` de Linux): bulk IN 0x81, bulk OUT 0x02 e interrupción
  IN 0x83 (2 bytes).
- **Configuración 2: CDC-NCM.**
  - Interfaz 0: clase 0x02, subclase 0x0D (NCM), protocolo 0, «CDC Communications Control».
    - Descriptores funcionales: Header (CDC 1.10), Union (control 0, subordinada 1),
      **Ethernet Networking** (0x0F: índice de la cadena con la MAC, tamaño máximo de
      segmento...) y NCM (0x1A).
    - Endpoint de interrupción IN 0x83 (16 bytes; notificaciones NETWORK_CONNECTION y
      CONNECTION_SPEED_CHANGE).
  - Interfaz 1: CDC-Data (0x0A), protocolo 0x01 (NTB). Ajuste alternativo 0 sin endpoints;
    **ajuste alternativo 1** con bulk IN 0x81 y bulk OUT 0x02.
- Configuración 3: no capturada; probablemente CDC-ECM.

## Propuesta

Driver **genérico de clase** (propio, a partir de las especificaciones públicas USB CDC 1.2,
ECM 1.2 y NCM 1.0 del USB-IF), no específico de Realtek. Sirve para el RTL8156/8153, muchos
adaptadores de otras marcas y el *USB tethering* de móviles que usen NCM o ECM.

- En la enumeración (`kernel/usb/usb.c`), recorrer **todas** las configuraciones y elegir la
  primera con una interfaz CDC NCM (0x02/0x0D) o ECM (0x02/0x06). Preferir NCM; ECM como
  alternativa. Requiere leer descriptores de configuración largos y `SET_CONFIGURATION`
  distinto de 1.
- **NCM:**
  - `GET_NTB_PARAMETERS`, `SET_NTB_INPUT_SIZE`;
  - formato **NTB16** (cabecera NTH16, NDP16 con índices de datagramas);
  - empaquetar uno o varios marcos por transferencia bulk OUT y desempaquetar varios por bulk
    IN;
  - ZLP y tamaños múltiplos de `wMaxPacketSize`.
- **ECM:** un marco Ethernet por transferencia bulk, más `SET_ETHERNET_PACKET_FILTER`.
- **MAC:** de la cadena indicada por el descriptor Ethernet Networking (12 dígitos
  hexadecimales).
- **Enlace:** notificaciones por el endpoint de interrupción. Reutilizar las transferencias
  por interrupción de la mejora 029 si ya existen; si no, añadir lo mínimo para EHCI
  (programación periódica o sondeo).
- **Integración con la pila de red** (D046) como interfaz `eth1` (o `eth0` si no hay otra):
  - conexión y desconexión en caliente (el EHCI ya la tiene para pendrives);
  - DHCP automático al conectar;
  - aparece en `ifconfig`, en la pestaña Red (038), en `infopc --informe` y en
    `docs/compatibilidad.md`.
- **Rendimiento en EHCI:** recepción con varias transferencias en cola (doble búfer como
  mínimo) para no perder marcos. Objetivo: >10 MB/s con `wget` en QEMU/KVM y en real.
- **Plan B** (si un adaptador no ofrece NCM ni ECM): adaptar el driver `r8152` de Linux bajo
  la D073 (GPL, aislado en `kernel/terceros/linux/r8152/`). No hace falta para este
  adaptador.

## Pruebas

- **QEMU:** `-device usb-ehci -device usb-net` emula **CDC-ECM** (y RNDIS); sirve para la
  prueba automática de ECM con la red de usuario: DHCP, ping, curl y wget con hash.
- **NCM:** QEMU no lo emula. Hay dos opciones:
  - pasar el adaptador real al WSL con **usbipd-win** (`usbipd bind/attach`) y de ahí a QEMU
    con `-device usb-host,vendorid=0x0bda,productid=0x8156`, si el QEMU local tiene soporte
    usb-host/libusb (comprobar);
  - pruebas unitarias del empaquetado NTB16 en el anfitrión contra tramas de referencia.
- **Real:** G470, dv7 y Amilo con el adaptador en un puerto USB 2.0, con `infopc --informe`.

## Criterios de aceptación

- [ ] ECM en QEMU (`usb-net`): DHCP, ping, curl/wget, conexión y desconexión en caliente
- [ ] NCM: empaquetado y desempaquetado NTB16 con pruebas unitarias; funcionando con el
      RTL8156 real (usbipd o portátil)
- [ ] Interfaz visible en ifconfig, pestaña Red e informe
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`
