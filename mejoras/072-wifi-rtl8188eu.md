# 072 - Wi-Fi: Realtek RTL8188EU (USB), Atheros AR9485 (PCIe) y pila 802.11 con WPA2

- **Estado:** **RTL8188EU hecho en la v7.3 (D215, rama `v7.3-wifi`)**: firmware, eFuse, búsqueda y
  transmisión probados con el pincho real; la conexión y WPA2, listos y probados contra un punto de acceso
  simulado, a falta de la red del mantenedor. El AR9485 sigue para la v8.
  **v7.3.2 (D220, rama `v7.3.2-wifi-seguridad`)**: WEP, WPA/TKIP (y WPA/WPA2 mixto) y WPA3-Personal (SAE)
  con 802.11w, probados en el anfitrión con los vectores de las normas y contra un punto de acceso simulado.
- **Antes:** propuesta (**v8**, decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media-alta
- **Alta:** 2026-11-08
- **Relacionadas:** 046 (xHCI, para el Ryzen), 044 (red por USB CDC), 027 (libtls: SHA-1/SHA-256/AES), D073 (código y firmware de Linux), 057 (variante sin GPL), 059 (Bluetooth USB), bandeja (D114)

## Objetivo

Conectar SaguiOS a redes Wi-Fi con el adaptador USB del mantenedor, **Realtek RTL8188EU
Wireless LAN 802.11n USB 2.0** (2,4 GHz), abiertas y **WPA2-PSK** (la de casa).

## Adaptador del mantenedor (datos tomados en Windows el 2026-11-08)

| Dato | Valor |
| --- | --- |
| Nombre | Realtek RTL8188EU Wireless LAN 802.11n USB 2.0 Network Adapter |
| **VID:PID** | **`0bda:8179`**, revisión `0000` (`USB\VID_0BDA&PID_8179&REV_0000`) |
| Descripción USB | «802.11n NIC», fabricante Realtek Semiconductor Corp. |
| Driver en Windows | `netrtwlanu.inf`, versión 1030.38.712.2019 (de Microsoft) |
| Radios | 802.11b/g/n, 2,4 GHz |
| Seguridad que admite | WPA2-Personal con CCMP (lo que hace falta), WPA/WPA2 con TKIP y Enterprise |

**Descriptores USB** (volcado de USB Device Tree Viewer del mantenedor):
- Dispositivo: USB 2.0 de alta velocidad (480 Mbit/s), `bcdDevice` 0x0000, `bMaxPacketSize0` 64,
  una sola configuración, alimentado por el bus (500 mA), con *remote wakeup*.
- **Una interfaz** (0), clase **0xFF/0xFF/0xFF** (del fabricante), con **3 endpoints *bulk* de 512
  bytes**:
  - `0x81` **IN**: recepción (tramas recibidas e informes del firmware);
  - `0x02` **OUT** y `0x03` **OUT**: dos colas de transmisión. En `rtl8xxxu`, con 2 OUT se
    reparten las colas de prioridad alta, normal y baja; hay que reproducir ese reparto.
- Sin endpoint de interrupción: todo va por *bulk* y control (registros con peticiones de
  fabricante `0x05`, como en Linux).
- **Dónde está conectado:** en el PC del mantenedor cuelga de un controlador **xHCI** (ACPI
  `XH00`, puerto 12, que admite USB 1.1 y 2.0). En ese PC hace falta el xHCI de la 046 (también
  en la v8). En el dv7, el G470 y el Amilo va por el EHCI que ya existe.

- `0bda:8179` es el ID del **RTL8188EU/8188EUS** en `rtl8xxxu` de Linux. El 8188ETV comparte ID
  y se distingue en tiempo de ejecución por el registro de versión del chip (`REG_SYS_CFG`). El
  driver debe leerlo, anotarlo en el log y en `infopc`, y elegir las tablas.
- Para pasarlo a QEMU: `usbipd attach --wsl --busid <bus>` en Windows y
  `-device usb-host,vendorid=0x0bda,productid=0x8179` en QEMU. Mientras está en el WSL, Windows
  lo pierde.

## Segundo adaptador: Atheros AR9485 (PCIe) del Toshiba L850 (añadido el 2026-11-08)

El portátil Toshiba Satellite L850-1JR del mantenedor (`testes_hw/toshiba-l850-informe-20261108.txt`)
lleva un **Atheros AR9485** (`168c:0032`, PCIe, 802.11b/g/n 1x1, 2,4 GHz), hoy sin driver. Se
añade como **segundo driver sobre la misma pila 802.11 y WPA2**:
- En Linux es **`ath9k`**, con licencia **ISC/BSD** (permisiva): se puede usar como referencia o
  traer código y tablas sin problemas de licencia, **también en la variante sin GPL**. Se
  registra igual en `docs/terceros.md`.
- **No necesita firmware**: el MAC 802.11 corre en el anfitrión (*softMAC*). A cambio, el driver
  hace más trabajo que con el Realtek:
  - cargar las **tablas de inicio** del AR9485 (`ar9485_initvals.h`);
  - leer la **EEPROM/OTP** (MAC, calibración y potencia por canal);
  - calibrar el PHY al cambiar de canal y en marcha (ANI, ruido);
  - anillos DMA de TX y RX por PCIe (MSI no hace falta, basta INTx);
  - la tabla de claves por hardware (*keycache*) para AES-CCMP;
  - el control de tasas, propio y sencillo (como `ath9k_rate_control` o *minstrel* simplificado).
- **Pruebas:** QEMU no lo emula y el portátil es la única máquina. El driver se escribe con
  mucho diagnóstico (`wifi diag`: registros, EEPROM y calibración) y una imagen de prueba por
  fases:
  1. detección y lectura de la EEPROM;
  2. recepción de balizas (`wifi buscar`);
  3. asociación a una red abierta;
  4. WPA2.
- **Orden:** primero el RTL8188EU, que se puede probar en QEMU pasándolo por USB, y con él la
  pila 802.11. Después el AR9485 reutiliza todo lo que no es el driver.

## Piezas

| Parte | Contenido | Dificultad |
| --- | --- | --- |
| **Driver USB** | Clase específica del fabricante: endpoints *bulk* de entrada y salida y control para registros. Carga del **firmware** `rtl8188eufw.bin` (linux-firmware, D073; fuera con `SEM_FIRMWARE=1`). Tablas de inicio MAC, BB y RF del driver `rtl8xxxu` de Linux (GPL, en `kernel/terceros/linux/rtl8xxxu/`, fuera con `SEM_GPL=1`). Descriptores de TX y RX | media-alta |
| **Pila 802.11 (cliente)** | Búsqueda activa y pasiva por los 13 canales, lista de redes con señal; autenticación abierta y asociación; tramas de datos con LLC/SNAP ↔ Ethernet hacia la pila IPv4 de siempre; balizas para detectar la pérdida de conexión y reconexión; tasas con ayuda del firmware | alta |
| **WPA2-PSK** | PMK = PBKDF2-HMAC-SHA1(contraseña, SSID, 4096); saludo de 4 pasos EAPOL (PTK, GTK, MIC con HMAC-SHA1); instalación de claves en la **CAM** del chip (AES-CCMP por hardware); renovación de la GTK | alta |
| **Herramientas** | `wifi buscar`, `wifi conectar "SSID"` (pide la contraseña), `wifi estado`, `wifi olvidar`; redes conocidas en `/home/.config/wifi.conf` (contraseña derivada como PMK, no en claro); icono de la bandeja con la lista de redes y la señal; DHCP automático al conectar | media |

**Fuera del primer alcance:** WPA3/SAE, WPA-Enterprise (802.1X), WEP (obsoleto e
inseguro), modo punto de acceso, modo monitor, 5 GHz (este chip no lo tiene) y ahorro de
energía. (WEP, TKIP y WPA3/SAE llegaron en la v7.3.2, D220; Enterprise sigue fuera.)

## Pruebas

- **QEMU no emula este chip.** Se prueba con el adaptador real:
  - Windows → WSL con `usbipd`, y WSL → QEMU con `-device usb-host,vendorid=0x0bda,productid=0x8179`
    (detrás del EHCI emulado);
  - o directamente en el dv7 o el G470.
- **Pruebas en el anfitrión sin hardware:**
  - PBKDF2, PTK y MIC con los vectores de la norma 802.11i (anexo H);
  - el saludo de 4 pasos contra un punto de acceso simulado en Python;
  - el analizador de balizas con tramas grabadas.
- **Real:** buscar redes, conectar a una abierta y a la WPA2 de casa, `ping`, `wget`/`curl`
  por HTTPS, reconexión tras apagar el router, y quitar y poner el adaptador.

## Criterios de aceptación

- [x] `wifi buscar` lista las redes cercanas con la señal (pincho real en QEMU, `h215`)
- [ ] Conectar a una red WPA2-PSK, DHCP, `ping` y HTTPS (listo; **falta probarlo con la red del mantenedor**)
- [x] Reconexión automática y redes recordadas (`/home/.config/wifi.conf` con la PMK; sin probar con red real)
- [x] Variante sin firmware: el adaptador sale con su MAC y «esta compilación no lleva firmware», sin fallar;
      sin GPL entra entero (la referencia es ISC)
- [x] Pruebas en el anfitrión de la parte criptográfica con los vectores de la norma (y el saludo de cuatro
      pasos contra un punto de acceso simulado)
- [x] Decisión registrada en `docs/decisoes.md` (D215)

## Hecho en la v7.3 (D215)

- **Cambio de modo** del CD virtual `1111:1111` (StandardEject de usb_modeswitch) en `kernel/rede/wifi.c`.
- **Driver** `kernel/rede/rtl8188eu.c` (propio, leyendo el urtwn de OpenBSD, ISC; las tablas en
  `kernel/terceros/bsd/rtwn/`): eFuse (MAC, potencia, cristal), encendido, LLT y colas, firmware
  (`/lib/firmware/rtl8188eufw.bin`, de `firmware/realtek/`), MAC/BB/AGC/RF, calibración LC, canal y potencia,
  descriptores de TX/RX, CAM para el CCMP, LED, informes de TX para la tasa.
- **USB**: recepción bulk continua en el EHCI (QH fija en el anillo asíncrono) y en el xHCI, sin el bloqueo
  de las transferencias (un pendrive en el mismo EHCI sigue a su velocidad).
- **802.11 y WPA2** (`kernel/rede/wifi.c`, `kernel/lib/wpa.c`): búsqueda activa, autenticación abierta,
  asociación b/g, saludo de cuatro pasos, GTK y su renovación, protección contra repeticiones, pérdida de
  balizas y reconexión.
- **wlan0** en la pila IP (una interfaz activa: la eth0 queda aparcada mientras el Wi-Fi está conectado),
  `wifi`, `ifconfig wlan0`, `infopc`, icono en la bandeja.
- **Pendiente:** la conexión con una red real; la calibración IQ; 802.11n; ~~WPA2 con grupo TKIP (modo mixto)~~
  (v7.3.2, D220).
  En el Ryzen del mantenedor el pincho cuelga del xHCI: por el xHCI de QEMU (`qemu-xhci`) el firmware y la
  búsqueda ya funcionan con el pincho real (`h215 pincho-xhci`).

## Hecho en la v7.3.2 (D220)

- **Primitivas** (`kernel/lib/wpa_cifra.c`, para el kernel, el programa `wifi` y el anfitrión): MD5 y HMAC-MD5,
  RC4, CRC-32, Michael, la mezcla de TKIP, el encapsulado TKIP y WEP de una trama, HMAC-SHA256, el KDF de la
  802.11 y HKDF, AES-CMAC, CCMP por software (tramas de gestión de la 802.11w) y BIP-CMAC-128.
- **P-256 y SAE** (`kernel/lib/p256.c`, `kernel/lib/sae.c`): aritmética de Montgomery de 8 miembros, escalera
  de Montgomery, raíz cuadrada y SSWU; PWE por cazar y picotear y por hash-to-element (PT), Commit, Confirm,
  KCK, PMK y PMKID.
- **Saludo** (`kernel/lib/wpa.c`): WPA1 (descriptor 254), descriptores 1, 2, 3 y 0, PSK-SHA256 y SAE, IGTK,
  RSNXE, PMKID en el mensaje 1 y el aviso de fallo de Michael; elección de lo que se negocia
  (`wpa_escolher`: SAE si se puede, si no PSK; 802.11w solo si la red la exige o con SAE).
- **Pila** (`kernel/rede/wifi.c`): TKIP y WEP por software (el CCMP de datos sigue en el chip),
  clave compartida, SAE con anti-clogging token, 802.11w (CCMP de gestión, BIP, consulta SA), contramedidas
  de TKIP, transición WPA2/WPA3 y vuelta a WPA2-PSK si el SAE no sale.
- **Programa `wifi`**: seguridad exacta en `buscar`, avisos de WEP y TKIP, PT y PWE de SAE calculados en el
  programa y guardados en `wifi.conf` (la contraseña no se guarda; la clave WEP sí, porque es la clave
  misma). Bandeja e `infopc` con la seguridad de la conexión.
- **Pendiente:** probar cada modo con un router real (WEP, WPA-TKIP, mixto, WPA3, transición); caché de
  PMKSA; SAE-PK e identificadores de contraseña; otros grupos de SAE; protección de balizas.

## Notas

- Modelo sugerido: **Opus** (hardware real sin emulador, criptografía y pila 802.11).
- Tamaño orientativo: unas 6.000-9.000 líneas, más el firmware (~15 KB).
- Una vez hecha la pila 802.11, otros chips USB (RTL8192EU, RTL8188FTV, Atheros AR9271 con
  firmware abierto) son sobre todo drivers nuevos sobre la misma pila.
