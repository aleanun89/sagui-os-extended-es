# Bluetooth con un controlador USB (D279, mejora 059, v8.2)

SaguiOS maneja teclados y ratones Bluetooth con un **controlador USB estándar sin firmware**, como el
**CSR8510** (`0a12:0001`) que llevan casi todos los «pinchos» baratos. Esta es la primera fase de la mejora 059;
el audio (A2DP), los archivos (OBEX), el BLE completo y los controladores Intel (AX210) van después; los de Broadcom, Realtek y
Atheros con firmware y la detección BLE llegaron en la 8.5.5.1 (D411, experimental).

## Qué hay

| Capa | Dónde | Qué hace |
| --- | --- | --- |
| Firmware | `kernel/bluetooth/bt_fw.c` | D411: parches de Broadcom (.hcd), Realtek (`rtl_bt/`) y Atheros (ath3k, por USB) durante el inicio; lógica pura probada en el PC |
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

- **Solo HID clásico** (teclados, ratones, combos). Sin audio, archivos ni red. El **BLE** solo se *detecta* (D411): teclados BLE
  como los de Apple/Logitech *Bluetooth Low Energy* salen en la lista pero no se emparejan (falta SMP y HOGP); la mayoría de los
  teclados y ratones «Bluetooth» baratos son clásicos.
- **Controladores con firmware: experimental (D411).** Broadcom, Realtek y Atheros se cargan desde la 8.5.5.1 (ver más abajo);
  **sin probar en hardware real**. Los Intel (AX200/AX210, `ibt`) siguen sin iniciar.
- **Recepción ACL con EHCI, xHCI y, desde D389, UHCI.** Por UHCI (puertos USB 1.1 de Intel/VIA, el pincho CSR es de velocidad
  completa y en los ICH con EHCI cae en el compañero) la recibe `uhci_rx_*`: 16 TD de 64 bytes encadenados en profundidad detrás de
  la cabeza de interrupción (cada 8 ms, hasta ~128 KB/s) y `usbbt.c` reconstruye los paquetes ACL por la longitud de la cabecera.
  **Sin probar aún en hardware real** (QEMU no emula Bluetooth). Con OHCI se puede buscar pero no emparejar ni conectar: la ventana
  lo dice. Con el EHCI comparte la recepción continua con el pincho Wi-Fi RTL8188EU.
- **Controladores Bluetooth 2.0 (clones CSR).** El inicio lee las características LMP (*Read_Local_Supported_Features*) y salta lo que
  el controlador no anuncia: sin EIR, la búsqueda va con RSSI (modo 1); sin SSP, no se pide *Simple Pairing* y el emparejamiento es
  con PIN (`0000`). Una orden de inicio opcional rechazada, contestada con *Command Status* o que nunca acaba (5 s) se salta y se anota
  en el registro con su nombre (D389).
- Un solo controlador y un solo emparejamiento a la vez; hasta 8 dispositivos conocidos y 4 enlaces.
- Teclados con informe distinto del *boot* (sin protocolo boot) y ratones cuyo descriptor no se entiende: no funcionan. Los ratones
  con descriptor entendible se usan en protocolo normal (con rueda y botones); los demás, en protocolo boot (3 o 4 bytes).
- No hay reconexión automática iniciada por nosotros: si se corta, se vuelve a pulsar *Conectar* (o el dispositivo reconecta si
  es un teclado/ratón que lo hace).

## Controladores que necesitan firmware (D411, experimental, sin probar en hardware)

Los controladores que no arrancan listos reciben un parche **durante el inicio**, antes de leer sus características. Todo lo de
esta sección está probado **solo contra un controlador HCI simulado** (`tests/h411_bluetooth_firmware.py`); si un chip real se
comporta distinto, el registro (`dmesg | grep BT`) lo dice y el inicio sigue sin parche.

| Fabricante | Chips | Cómo | Archivos (en `/lib/firmware/`) | Distribución |
| --- | --- | --- | --- | --- |
| **Broadcom** | BCM20702, BCM20703 y familia (`0a5c:21e*`, integrados de muchos portátiles) | HCI: *Download_Minidriver*, registros del `.hcd`, *Reset* | `brcm/<chip>-<vid>-<pid>.hcd` o `brcm/<chip>.hcd` | **No se distribuyen**: ponlos tú |
| **Realtek** | RTL8723B/BE/D, RTL8761B/BU, RTL8821A/C (`0bda:...`) | HCI vendor `0xFC20` en fragmentos de 252 bytes | `rtl_bt/<chip>_fw.bin` y `<chip>_config.bin` | De linux-firmware, en git y en la imagen |
| **Atheros** | AR3011, AR3012 (`0cf3:3000`, `0cf3:3004`, `04ca:300x`, `13d3:33xx`...) | USB: peticiones de fabricante y bulk (ath3k), antes del HCI | `ath3k-1.fw`; `ar3k/AthrBT_0x<rom>.dfu` y `ar3k/ramps_0x<rom>_<MHz>.dfu` | De linux-firmware, en git y en la imagen |

- **Broadcom.** El nombre del chip sale de *Read_Local_Name* («BCM20702A1»). Copia el `.hcd` que extraigas del controlador de
  Windows de tu equipo a `/lib/firmware/brcm/BCM20702A1-0a5c-21e8.hcd` (con el identificador USB de tu chip; `usb` lo muestra) o a
  `/lib/firmware/brcm/BCM20702A1.hcd`. Con la raíz en disco (D379) el archivo persiste; con el ramdisk, solo hasta apagar. Sin
  archivo: `[BT] Broadcom: sin parche (/lib/firmware/brcm/...hcd no existe): se sigue con lo que tenga la ROM`.
- **Realtek.** Se reconoce el chip por (`lmp_subversion`, revisión y versión HCI) y se elige el parche por la versión de ROM. Una
  `lmp_subversion` que no está en la tabla (por ejemplo un chip ya parcheado) no se toca. Registro: `[BT] Realtek RTL8761B en
  ROM ...: hace falta rtl_bt/rtl8761bu_fw.bin`, `ROM 1, parche 44470 bytes + config 6, 177 fragmentos` y, al acabar,
  `firmware: rtl_bt/rtl8761bu_fw.bin cargado, se relee la version`.
- **Atheros.** Pide al dispositivo su estado y su versión de ROM por USB y sube el parche y la configuración que le tocan; luego
  el dispositivo se reenumera (aparece otra vez en `usb`) y el HCI arranca ya con el parche. Un AR3011/AR3012 que enumera con
  clase de fabricante (0xFF) en vez de la de Bluetooth **todavía no se reconoce** (solo se atienden los de clase `0xE0/0x01/0x01`).
- **Lo que no hace:** Intel; el formato nuevo «RTBTCore» de Realtek (RTL8852 y siguientes); cambiar la dirección BD de los
  Realtek/Broadcom que vienen sin ella.
- Los firmware (`firmware/`, SHA-256 en `tools/firmware_bt.sh`) no entran con `make SEM_FIRMWARE=1`; licencias y origen en
  `docs/terceros.md`. Los controladores integrados que el mantenedor tiene en sus portátiles pueden estar apagados: no se tocan.

### Probar un controlador con firmware (guion)

1. Enchufa el pincho (o enciende el integrado) y mira `usb`: debe salir con clase inalámbrico y `[BT] controlador ... iniciando`.
2. `dmesg | grep -E "BT"` debe contar la carga (arriba) y terminar sin `rechazad`/`dejo de contestar`. `bluetooth` debe decir
   **listo** y mostrar la dirección y la versión (que **cambia** tras el parche: es buena señal).
3. Sigue como con el CSR (pasos 2 a 9 de la sección siguiente). Si falla, pega `usb`, `bluetooth` y `dmesg | grep BT`.

## Búsqueda de baja energía (BLE, D411, experimental)

Si el controlador anuncia LE (casi todos los de Bluetooth 4.0 en adelante), `bluetooth buscar` hace, tras la búsqueda clásica, una
**búsqueda LE activa de 4 s** (parámetros 60 ms / 30 ms, sin filtro de repetidos en el controlador) y añade a la lista los
anuncios que dicen su **nombre** o que son **HID** (apariencia 0x03C0 a 0x03C4 o servicio 0x1812). Salen como `BLE` o `BLE
(teclado/ratón)`; los anuncios anónimos (balizas, relojes sin nombre) se descartan para no llenar la tabla de 8 dispositivos.
`bluetooth conectar N` sobre uno de ellos da el error «baja energia (BLE): aun no se puede emparejar, solo se detecta».

**Plan para el BLE completo (pendiente):** canales fijos LE de L2CAP (ATT y SMP) y *LE Create Connection*; SMP (*Just Works* y
*Passkey*, claves LTK guardadas junto a las de enlace); cliente GATT (servicios, características, descriptores y notificaciones);
y HOGP (servicio HID 0x1812: mapa de informes y informes de entrada por notificación, que ya entrarían por `hid_bt_*`).

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
con firmware raro pueden rechazar órdenes de inicio: el registro dice `inicio: Write_Inquiry_Mode (0c45) rechazada (estado 11): se sigue`
o `inicio: Write_Simple_Pairing_Mode se salta`, y `[BT] caracteristicas LMP ...` dice qué anuncia el pincho. Por UHCI, `usb` añade la
línea `Bluetooth: ACL por UHCI, N paquetes USB, E errores` (D389).

### El clon CSR del Fujitsu AMILO Pi 1536 (D389)

Pincho `0a12:0001` («CSR», HCI/LMP 3 = Bluetooth 2.0, fabricante 10) por el UHCI del ICH7. Con la 8.5.4.10 se quedaba en «iniciando»:
*Write_Inquiry_Mode* 2 se rechazaba con `0x11` (bien, se seguía) y *Write_Simple_Pairing_Mode*, que un 2.0 no conoce, la contestaba con
*Command Status* `0x01` en vez de *Command Complete*: el inicio solo avanzaba con *Command Complete* y esperaba para siempre (7 eventos,
«errores 2» = esas dos órdenes). Guion para el mantenedor con la imagen nueva:

1. `bluetooth` debe decir **listo**, HCI 3, y `dmesg | grep BT` traer `caracteristicas LMP ...: RSSI si, EIR no, SSP no` y
   `Write_Simple_Pairing_Mode se salta`.
2. Con un **móvil visible** (Ajustes > Bluetooth abierto) o un teclado en modo de emparejamiento cerca: `bluetooth buscar` y
   `bluetooth lista` lo muestran (con RSSI).
3. Con un teclado o ratón Bluetooth: `bluetooth emparejar N` (con PIN `0000` si lo pide) y `usb` → la línea `ACL por UHCI` con
   paquetes > 0. Si el ratón/teclado no responde, pegar `bluetooth`, `usb` y `dmesg | grep -E "BT|UHCI"`.
