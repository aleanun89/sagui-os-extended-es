# Dispositivos USB reales en QEMU (usbipd + WSL)

Para probar drivers con hardware de verdad (por ejemplo el pincho Wi-Fi RTL8188EU de la v7.3)
sin grabar un pendrive: Windows comparte el dispositivo con WSL por **usbipd-win** y QEMU se lo
pasa a SaguiOS en el controlador USB 2.0 (EHCI) con `usb-host`.

## Una vez

En Windows, en un PowerShell de administrador:

```powershell
usbipd list                       # buscar el BUSID (p. ej. 4-3 para 0bda:8179)
usbipd bind --busid 4-3
```

En WSL, para que QEMU abra el dispositivo sin ser root:

```bash
sudo tee /etc/udev/rules.d/99-saguios-usb.rules <<'REGLAS'
SUBSYSTEM=="usb", ATTR{idVendor}=="0bda", ATTR{idProduct}=="8179", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1111", ATTR{idProduct}=="1111", MODE="0666"
REGLAS
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## Cada vez

```powershell
usbipd attach --wsl --busid 4-3 --auto-attach     # dejar la ventana abierta
```

```bash
./run_in_qemu.sh --wifi-usb --serie               # o --usb-real VID:PID (se puede repetir)
./run_in_qemu.sh --xhci --usb-real VID:PID --serie  # D214: por un xHCI (USB 3.0) en vez del EHCI
```

Con `--xhci` el dispositivo va por el controlador USB 3.0 del QEMU (`qemu-xhci`): así se prueba el
driver xHCI con un teclado, un ratón o un pendrive de verdad (`[XHCI]` y `[HID]` en el log).

En el log: `[USB] conectado: Realtek 802.11n NIC (0bda:8179) classe ff`. Mientras está en WSL,
Windows no usa el dispositivo. El pincho puede aparecer como CD virtual `1111:1111` hasta que
se le mande el cambio de modo: `--wifi-usb` pasa los dos identificadores y `--auto-attach` lo
vuelve a enganchar cuando se reconecta con el otro.

## El Wi-Fi (D215)

Con el pincho pasado a QEMU, en SaguiOS:

```
wifi                     # adaptador, MAC de la eFuse, firmware y estado
wifi buscar              # redes cercanas
wifi conectar "Mi red" clave
ifconfig wlan0
```

La prueba `tests/h215_wifi.py` usa el pincho si está en `/sys/bus/usb/devices` (si no, salta esa
parte con un aviso). **Solo un QEMU a la vez** puede tener el pincho: no lances la prueba mientras
haya otro `./run_in_qemu.sh --wifi-usb` abierto.
