# 046 - USB 3 (xHCI): pendrives, teclado y ratón en PC modernos

- **Estado:** propuesta (prioridad alta tras la prueba en el Ryzen)
- **Prioridad:** alta
- **Alta:** 2026-11-07
- **Relacionadas:** 029 (HID, OHCI/UHCI), 044 (red USB), 035 (raíz en disco), D073

## Problema / motivación

En el PC con Ryzen 5 4500 y placa ASUS PRIME B450M-A (`docs/compatibilidad.md`, fotos
`testes_hw/ryzen-b450-*.jpg`), el chipset y el SoC **solo tienen controladores xHCI** (USB 3,
`1022:43d5` y los del SoC), sin EHCI. SaguiOS solo maneja EHCI, así que:

- el **pendrive de arranque desaparece** al arrancar el kernel (la BIOS lo leyó por INT 13h):
  no se monta la partición FAT32 de `/home` ni ningún otro disco USB;
- el **ratón USB no funciona** (el teclado sí, por la emulación PS/2 de la BIOS, que se pierde
  si se toca el controlador);
- el adaptador de red USB (044) tampoco tendría dónde conectarse.

Esto pasa en casi cualquier PC de ~2013 en adelante.

## Propuesta

- **Driver xHCI** (`kernel/usb/xhci.c`), a partir de la especificación pública de Intel (xHCI
  1.1/1.2). Con la D073 se permite mirar la implementación de Linux como referencia y copiar
  definiciones, aislado y registrado.
  - Traspaso desde la BIOS (*USB Legacy Support* en las capacidades extendidas), reset e
    inicialización.
  - DCBAA, anillos de comandos y de eventos y, por dispositivo, contexto de *slot* y de
    *endpoints*.
  - `Enable Slot`, `Address Device`, `Configure Endpoint`.
  - Transferencias de control, *bulk* e interrupción. Puertos USB 2 y USB 3 del mismo
    controlador (protocolo por puerto en *Supported Protocol*).
  - Hubs (*route string*, TT para dispositivos lentos detrás de hubs USB 2) y conexión en
    caliente.
  - Interrupciones con MSI si el PCI las tiene; si no, INTx o consulta.
  - Direcciones de 64 bits: la memoria de SaguiOS está por debajo de 4 GB.
- **Capa común de USB** (`usb.c`): que almacenamiento, HID (029) y red (044) funcionen igual
  sobre EHCI y sobre xHCI.
- **HID en protocolo de arranque** (parte de la 029, si no está ya): teclado y ratón USB, con
  rueda; el teclado por el mismo camino que el PS/2 (ES/ABNT2, teclas muertas, repetición).
- **Raíz y persistencia:** con xHCI, el pendrive de arranque vuelve a aparecer y `arranque` en
  `/etc/fstab` monta la FAT32 de `/home` como en los equipos con EHCI.

## Pruebas

- **QEMU:** `-device qemu-xhci` (o `nec-usb-xhci`) con `usb-storage`, `usb-kbd`, `usb-mouse`,
  `usb-tablet` y un hub. Cubre:
  - arrancar desde el pendrive en xHCI;
  - montar `/home` en FAT32;
  - teclado y ratón USB;
  - desconectar y volver a conectar.
- Las pruebas EHCI existentes (h29-h34, h52...) siguen pasando.
- **Real:** el PC Ryzen B450 (pendrive, `/home`, ratón) y, si se puede, el G470 (que también
  tiene xHCI en algunos modelos).

## Criterios de aceptación

- [ ] Pendrive en un puerto xHCI: visible tras arrancar, `/home` en FAT32 montado
- [ ] Teclado y ratón USB funcionando sin la emulación de la BIOS
- [ ] Conexión en caliente y hubs
- [ ] Test automático en `tests/` con `qemu-xhci`
- [ ] Probado en el PC Ryzen B450
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Esfuerzo grande: ~3.000-4.000 líneas (xHCI + capa común), más HID si no está hecho.
- Mientras tanto, en ese PC: ratón PS/2 en el puerto combinado de la placa.
