# Bluetooth con un controlador USB (D279, mejora 059, v8.2)

SaguiOS maneja teclados y ratones Bluetooth con un **controlador USB estándar sin firmware**, como el
**CSR8510** (`0a12:0001`) que llevan casi todos los «pinchos» baratos. Esta es la primera fase de la mejora 059;
el audio (A2DP), los archivos (OBEX), el BLE y los controladores integrados con firmware (Intel AX210) van después.

## Qué hay

| Capa | Dónde | Qué hace |
| --- | --- | --- |
| Transporte USB | `kernel/usb/usbbt.c` | Reconoce la clase `0xE0/0x01/0x01`; órdenes HCI por control, eventos por interrupción (reensamblados por su longitud) y ACL por *bulk*; la tarea del kernel `bluetooth` bombea cada tic (10 ms) |
| HCI | `kernel/bluetooth/bt_hci.c` | Inicio (Reset, versión, BD_ADDR, búferes ACL, máscara de eventos, SSP, nombre, clase), cola de órdenes con créditos, créditos ACL, fragmentación, eventos |
| L2CAP | `bt_l2cap.c` | Señalización (conexión, configuración, desconexión, eco, información), canales en modo básico, reensamblado, tiempos |
| SDP | `bt_sdp.c` | Cliente: `ServiceSearchAttributeRequest` del servicio HID (0x1124) con continuación; lee PSM, subclase, dispositivo de arranque, nombre y **descriptor de informe** |
| HIDP | `bt_hid.c` | `SET_PROTOCOL`, informes de entrada, luces del teclado, `VIRTUAL_CABLE_UNPLUG` |
| Gestor | `bt_gestor.c` | Tabla de dispositivos, flujo emparejar/conectar, emparejamiento seguro simple, claves de enlace, reconexión |
| Entrada | `kernel/usb/hid.c` (`hid_bt_*`) | Los informes entran por el **mismo camino** que el HID USB: traducción a PS/2, teclas muertas, atajos, repetición, ratón relativo o por descriptor |
| Llamada | `SIS_BLUETOOTH` (135) | `BT_ESTADO`, `BT_BUSCAR`, `BT_DISPOSITIVOS`, `BT_EMPAREJAR`, `BT_CONECTAR`, `BT_DESCONECTAR`, `BT_OLVIDAR`, `BT_CONFIRMAR`, `BT_ENERGIA` |
| Programa | `userland/programas/bluetooth.c` | Orden `bluetooth` y ventana «Bluetooth» (menú Inicio > Sistema, o `bluetooth --ventana`) |

La pila (`kernel/bluetooth/`) no sabe nada del USB: todo lo de fuera entra por `bt_plat_t`, y por eso se prueba en el PC con un
controlador simulado (`tests/bt_sim.c`, `tests/bt_unidad.c`, `tests/h279_bluetooth.py`).

**Código:** todo escrito desde las especificaciones públicas (Bluetooth Core, HID Profile 1.1, HID Specification 1.11 con los
informes del protocolo *boot*, SDP, Assigned Numbers). **Nada** de BlueZ ni de Linux; no se copió código de ninguna pila.

## Cómo se usa

Ventana: **Inicio > Sistema > Bluetooth**. *Buscar* (unos 8 s), elegir el dispositivo y *Emparejar / Conectar*.
Terminal:

```
bluetooth                      estado del adaptador y lista
bluetooth buscar               busca y lista
bluetooth conectar 0           empareja (si hace falta) y conecta el dispositivo 0 (o su MAC)
bluetooth emparejar 0          solo guarda la clave y se desconecta
bluetooth desconectar 0
bluetooth olvidar 0            desconecta y borra la clave
bluetooth confirmar si|no      contesta a la comparación numérica
bluetooth encender | apagar
```

Lo que cambia el sistema (emparejar, conectar, desconectar, olvidar, confirmar, encender y apagar) pide el diálogo de
permisos de administrador (D269); buscar y mirar, no.

### Emparejamiento (Secure Simple Pairing)

El adaptador se declara **DisplayYesNo**; según lo que diga el dispositivo:

| Dispositivo | Método | Qué ves |
| --- | --- | --- |
| Ratón (sin pantalla ni teclado) | *Just Works* | nada: se acepta solo |
| Dos pantallas con sí/no (combos, móviles) | Comparación numérica | un código de 6 cifras: pulsa *Sí coincide* si es el mismo que el del dispositivo |
| Teclado | *Passkey* | un código de 6 cifras: escríbelo **en el teclado Bluetooth** y pulsa Intro |
| Dispositivos anteriores a SSP (2.0) | PIN | se prueba el PIN de fábrica `0000` |

La clave de enlace sale del controlador (Link Key Notification) y se guarda en `/etc/bluetooth.conf` y, si existe la
partición SAGUI-CONF (D255, D256), también en `SAGUI-CONF/etc/bluetooth.conf`, que es lo que sobrevive al apagado. Sin SAGUI-CONF
(modo en vivo) las claves duran hasta apagar (el registro lo dice). **El archivo da acceso a tus dispositivos:** solo lo lee root.

Los teclados reconectan solos al pulsar una tecla: llegan como *Connection Request*, y si tenemos su clave se aceptan, se
autentican y abren ellos los dos canales HID. Una clave rechazada por el dispositivo (se reseteó) se borra y el error dice que hay que
volver a emparejar.

## Límites conocidos

- **Solo HID** (teclados, ratones, combos). Sin audio, archivos, red ni **BLE** (teclados BLE como los de Apple/Logitech *Bluetooth Low
  Energy*: no); la mayoría de los teclados y ratones «Bluetooth» baratos son clásicos.
- **Controladores sin firmware.** Los Intel, Realtek y Broadcom que lo piden (los integrados de los portátiles, muchos pinchos
  modernos) aparecen en `usb` pero no inician. Quedan para la fase con firmware (D073).
- **Recepción ACL solo con EHCI y xHCI** (la recepción *bulk* continua del Wi-Fi). Con UHCI/OHCI (puertos USB 1.1) se puede buscar pero no
  emparejar ni conectar: la ventana lo dice. Con el EHCI comparte la recepción continua con el pincho Wi-Fi RTL8188EU.
- Un solo controlador y un solo emparejamiento a la vez; hasta 8 dispositivos conocidos y 4 enlaces.
- Teclados con informe distinto del *boot* (sin protocolo boot) y ratones cuyo descriptor no se entiende: no funcionan. Los ratones
  con descriptor entendible se usan en protocolo normal (con rueda y botones); los demás, en protocolo boot (3 o 4 bytes).
- No hay reconexión automática iniciada por nosotros: si se corta, se vuelve a pulsar *Conectar* (o el dispositivo reconecta si
  es un teclado/ratón que lo hace).

## Probar con el CSR8510 y un teclado o ratón Bluetooth reales

QEMU 8.2 no emula Bluetooth, así que lo que sigue **solo se puede comprobar con el pincho** en un PC real (con xHCI o EHCI). Anota
el resultado de cada paso y pega el registro (`dmesg | grep -E "BT|HID"` y la salida de `usb`).

1. **Reconocimiento.** Enchufa el pincho. `usb` debe listar `... (0a12:0001), clase inalámbrico` y una línea
   `Bluetooth: listo, ...`. `dmesg` debe traer `[BT] controlador ... iniciando`. `bluetooth` muestra HCI 6 (o el que sea), LMP, el
   BD_ADDR y `ACL 310 bytes x 8` (o parecido). Si dice «sin recepción ACL», el pincho está en un puerto UHCI/OHCI: cambia de puerto.
2. **Búsqueda.** Pon el teclado/ratón en modo de emparejamiento (suele ser una pulsación larga del botón Bluetooth). `bluetooth buscar` lo
   lista con nombre, tipo y señal. Si no sale con nombre, repite: los nombres se piden después.
3. **Ratón.** `bluetooth conectar N`: se conecta sin código. El puntero se mueve, los botones y la rueda funcionan.
   `dmesg`: `[BT] ... conectado (raton)` y `[HID] Bluetooth: raton (descriptor)` o `(boot)`.
4. **Teclado.** `bluetooth conectar N`: la ventana/terminal muestra un código; escríbelo en el teclado Bluetooth y pulsa Intro. Las teclas
   escriben en el terminal; Mayús, Ctrl, Alt, la repetición y Bloq Mayús (la luz del teclado) funcionan.
5. **Persistencia.** `bluetooth desconectar N` y `bluetooth conectar N` otra vez: **no** pide código. Reinicia el sistema (con SAGUI-CONF):
   `bluetooth lista` sigue mostrándolo como emparejado y `bluetooth conectar N` entra sin código.
6. **Reconexión del teclado.** Apaga y enciende el teclado, o pulsa una tecla tras un rato: debe volver solo (`[BT] ... conectado`).
7. **Cortes.** Aleja el dispositivo o quítale las pilas: el estado pasa a «emparejado» (sin conexión), no queda ninguna tecla pulsada.
   Retira el pincho con el dispositivo conectado: el sistema no se cuelga y el dispositivo desaparece de la lista.
8. **Olvidar y repetir.** `bluetooth olvidar N`, vuelve a buscar y a emparejar.
9. **Apagar/encender.** `bluetooth apagar` desconecta todo; `bluetooth encender` lo deja listo otra vez.

Si algo falla, lo más útil es: el modelo exacto del pincho y del dispositivo, `usb`, y el registro `[BT]`. Los controladores «CSR clónicos»
con firmware raro pueden rechazar órdenes de inicio: el registro dice `orden de inicio ... rechazada: se sigue`.
