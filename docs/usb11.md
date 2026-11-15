# USB 1.1: controladores UHCI y OHCI (D267)

Los PC antiguos con chipset Intel (UHCI + EHCI), VIA (UHCI), AMD, NVIDIA o SiS (OHCI) llevan los
dispositivos de **baja y plena velocidad** (teclados, ratones, hubs USB 1.1, pendrives viejos) por un
controlador *compañero* del EHCI. Sin driver para él, el sistema veía el dispositivo, lo cedía y nadie lo
recogía: ni teclado, ni ratón, ni pendrive. Desde la v8.0 hay drivers propios, escritos desde las
especificaciones públicas (UHCI Design Guide 1.1 de Intel y OpenHCI 1.0a), sin código de otros sistemas.

## Qué hace

| | UHCI (`kernel/usb/uhci.c`) | OHCI (`kernel/usb/ohci.c`) |
|---|---|---|
| Registros | puertos de E/S (BAR 4) | MMIO (BAR 0) |
| Lista | 1024 cuadros -> QH/TD de 32 bytes | HCCA + ED/TD de 16 bytes (listas de control, bulk y 32 periódicas) |
| Control, bulk | sí (una transferencia por vez, TD en profundidad, paquete corto con SPD) | sí (una ED de trabajo por lista, TD con redondeo) |
| Interrupción (HID) | QH colgada de la cabeza "int", se consulta cada 8 ms | ED colgada de las 32 entradas periódicas, cada cuadro |
| Isócrono | no (v8.1) | no (v8.1) |
| Traspaso BIOS | para el controlador, borra SMI/avisos de `USBLEGSUP` (PCI 0xC0), reinicio global | `HcControl.IR` -> `OwnershipChangeRequest`, espera a que suelte; después `HCR` |
| Esperas | sin IRQ: consulta del bit "activo" cediendo la CPU; tiempo por TSC | sin IRQ: código de condición del TD; tiempo por TSC |

Las dos exponen la misma interfaz que el EHCI (`*_controle`, `*_bulk`, `*_int_abrir/ler/fechar`), y
`usb.c` las despacha por `tipo_ctl` (`USB_UHCI`, `USB_OHCI`). Los drivers de clase (hubs, HID,
almacenamiento) no cambian: el HID usa `usb_int_abrir/ler/fechar` para todos los controladores menos el xHCI.
No hay recepción continua (el Wi-Fi sigue pidiendo EHCI o xHCI).

## Compañeros del EHCI

El EHCI mira cada puerto al conectarse algo: alta velocidad se queda; baja velocidad (línea en estado K) o
plena velocidad (el puerto no se habilita tras el reset) se ceden con `PORTSC.PORT_OWNER` y el puerto pasa
a manos del compañero. El UHCI/OHCI ve entonces la conexión en su propio puerto y lo enumera con la
secuencia normal (reset de 50 ms, SET_ADDRESS, descriptores, SET_CONFIGURATION), con hubs USB 1.1 detrás.
Solo se cede si hay un driver UHCI/OHCI que lo recoja (`companheiro_usavel`): un chipset que declara
compañeros sin que tengamos controlador para ellos deja el puerto sin ceder (como antes). Al desenchufar,
el puerto vuelve solo al EHCI (EHCI 4.2.2) y el siguiente dispositivo de alta velocidad se enumera allí.
El orden de arranque es xHCI, EHCI, UHCI, OHCI (el EHCI tiene que tomar los puertos antes).

## Qué probar en el Fujitsu Amilo Pi 1536 (Core Duo T2400, chipset Intel 945)

El 945 tiene un EHCI y cuatro UHCI (2 puertos cada uno, 8 puertos en total).

1. Arrancar SaguiOS con el ratón y el teclado USB externos (los del portátil son PS/2 internos) y mirar el
   log del arranque (`dmesg`/serie): debe haber `[EHCI] controlador ...`, cuatro `[UHCI] controlador ...
   8086:27c8..27cb` y `[USB] 4 controlador(es) UHCI, 0 OHCI`.
2. Con un ratón o teclado USB de baja velocidad: `[EHCI] porta N: dispositivo USB 1.x de baixa velocidade ->
   companheiro UHCI/OHCI`, `[USB] porta N (EHCI 0): USB 1.x, cedido al controlador companero` y después
   `[USB] conectado: ... UHCI, 1,5 Mb/s` y `[HID] teclado USB: ... por UHCI`.
3. `usb` en el terminal: sección «Controladores USB 1.1», estado de cada puerto, `HID: ... por UHCI`.
4. Un pendrive viejo (USB 1.1) o un hub USB 1.1 con un pendrive: `discos`, `montar usbN`, leer y escribir un
   fichero y `desmontar`. Un pendrive USB 2.0 sigue por el EHCI a 480 Mb/s (compruébese que no se ha
   estropeado).
5. Enchufar y desenchufar el ratón y el pendrive en caliente varias veces; el sistema no debe congelarse.
6. Si el teclado USB emulado por la BIOS (SMI) interfería, el log del UHCI dice `BIOS lo usaba`; al
   arrancar debe seguir respondiendo el teclado PS/2 interno y el USB externo.
7. Si algo falla, copiar de `usb` y del log las líneas `[UHCI]`, `[OHCI]` y `[USB] porta`. Lo no verificable
   en QEMU: dispositivos de baja velocidad (QEMU no los emula), el `USBLEGSUP` y la SMI de una BIOS real y
   los errores de hardware (CRC, babble).

## En QEMU

`run_in_qemu.sh --uhci` / `--ohci` ponen un `piix3-usb-uhci` (2 puertos) o un `pci-ohci` (3) para el
pendrive (`--pendrive`) y el teclado/tableta (`--hid-usb`). La prueba es `tests/h267_usb11.py`
(`uhci`, `ohci`, `companero`).
