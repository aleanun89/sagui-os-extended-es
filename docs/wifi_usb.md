# Pinchos Wi-Fi USB (D409)

SaguiOS lleva Wi-Fi con pinchos USB sobre una sola pila 802.11/WPA (`kernel/rede/wifi.c`, D215, D220, D238).
**Solo el Realtek RTL8188EU está probado en hardware.** Los demás son **experimentales: se escribieron sin tener el
dispositivo** (el mantenedor solo tiene el RTL8188EU y los Wi-Fi integrados) y puede que no funcionen a la
primera. Este documento explica qué hay, cómo probarlo si tienes uno de esos pinchos y qué contar después.

Un solo chip Wi-Fi a la vez (el primero que se enciende; los demás se ignoran). Todos usan los mismos mandatos:
`wifi`, `wifi buscar`, `wifi conectar SSID [clave]`, `wifi desconectar`, `ifconfig wlan0`, y el icono de la bandeja.

## Qué pinchos reconoce

| Familia | Chips | VID:PID (ejemplos) | Driver | Firmware (`/lib/firmware`) | Estado |
| --- | --- | --- | --- | --- | --- |
| Realtek RTL8188EU | RTL8188EUS, RTL8188ETV | `0bda:8179` | `kernel/rede/rtl8188eu.c` | `rtl8188eufw.bin` | **probado** (D215, D220, D238) |
| Realtek «C» | RTL8188CUS, RTL8188CU, RTL8188RU, RTL8192CU (2T2R) | `0bda:8176`, `0bda:8178`, `0bda:817a`, `0bda:817b`... (80 en total) | el mismo `rtl8188eu.c`, rama de la familia C | `rtl8192cufw_A.bin` (UMC corte A 1T1R) o `rtl8192cufw_TMSC.bin` | experimental |
| Ralink | RT2870, RT3070, RT3071/RT3072, RT5370 | `148f:2870`, `148f:3070`, `148f:3072`, `148f:5370`... (248 en total) | `kernel/rede/rt2870.c` | `rt2870.bin` | experimental |
| Atheros AR9271 | AR9271 (ath9k_htc) | `0cf3:9271`, `0cf3:1006`, `13d3:3327`... (13 en total) | **ninguno todavía** | (`htc_9271.fw`, libre, sin integrar) | solo reconocido |

Los chips Ralink RT3572, RT3593, RT5392 y RT5592 comparten VID:PID con los anteriores o con sus parientes: el
sistema los reconoce, enciende el pincho, lee su identificador y dice «todavía sin soporte».

Para saber si tu pincho está en la lista, antes de enchufarlo:

```
wifi compatibles                (las familias y cuántos dispositivos tiene cada una)
wifi compatibles 148f:5370      (de qué familia es un VID:PID)
```

El VID:PID de lo que tienes enchufado lo da el programa `usb` (o `lsusb` en Linux).

## Qué hace y qué no (los experimentales)

- Solo **2,4 GHz a 20 MHz**; tasas **b/g fijas** (OFDM 24 Mb/s para los datos; las tramas de gestión van a 1 Mb/s):
  sin 802.11n, sin A-MPDU, sin control automático de la tasa. Es decir: **más lento que el RTL8188EU**, a propósito,
  para no depender de partes sin probar.
- Seguridad: redes abiertas, WEP, WPA, WPA2 y WPA3 como con el 8188EU. En los **Realtek «C» el CCMP lo hace el chip**
  (claves en la CAM, como el EU); en los **Ralink todo el cifrado va por software** (el kernel cifra y descifra; el
  chip pasa las tramas tal cual).
- Sin calibración IQ del radio, sin ahorro de energía; el firmware se carga desde `/lib/firmware/` (con
  `make SEM_FIRMWARE=1` no entra y el driver lo dice).
- El aviso «experimental» sale en el registro del kernel al enchufar el pincho, en el nombre del adaptador
  (`Realtek RTL8192CU (experimental)`) y en `wifi`.

## Cómo probarlo

1. Arranca SaguiOS (la imagen de la versión) y entra con un usuario. **Antes de enchufar nada**, mira los mensajes:
   `dmesg | grep WIFI` no debe decir nada.
2. Enchufa el pincho **en un puerto USB 2.0 o 3.0** y espera unos segundos. Algunos Realtek aparecen primero como un
   CD virtual con el driver de Windows (`1111:1111`): el sistema lo cambia de modo solo.
3. `dmesg | grep WIFI`. Líneas esperadas (ejemplo de un Ralink):

   ```
   [WIFI] pincho Ralink RT2870/RT3070/RT5370 (148f:3070) EXPERIMENTAL: sin probar en hardware (D409), se enciende en la tarea wifi
   [WIFI] RT3070 (rev 0201), RF RT3020, 1T1R, ROM EEPROM
   [WIFI] Ralink RT3070 (experimental), MAC aa:bb:cc:dd:ee:ff (EEPROM/eFuse); EXPERIMENTAL: sin probar en hardware (D409)
   [WIFI] firmware rt2870.bin +0 cargado; chip listo (0 errores USB)
   ```

   Si no sale ni la primera línea, el VID:PID no está en la lista (`wifi compatibles VID:PID`).
   Si sale «no soportado», «el chip no se encendió», «el microcódigo no arrancó»... es un fallo del driver:
   guarda todo el `dmesg`.
4. `wifi`: debe poner el adaptador, la MAC, el firmware y «Estado: listo». `ifconfig wlan0`: la interfaz.
5. `wifi buscar`: la lista de redes cercanas con su señal. Sin redes pero sin errores: probablemente la recepción o
   la antena (prueba cerca del router).
6. `wifi conectar MI_RED mi-clave` y luego `ping 8.8.8.8` / `curl http://neverssl.com`. Con una red abierta o WPA2
   se prueba lo más sencillo; con WPA3 y WEP, después.
7. Retira y vuelve a enchufar el pincho: debe volver a salir.

## Qué contar si falla

Abre una issue (etiqueta del pincho, p. ej. `wifi-usb`) con:

- el VID:PID, el modelo del pincho y el chip (si lo sabes);
- `dmesg | grep -i -E "wifi|usb"` completo (sin tu SSID ni las MAC de otros: ofúscalos);
- `wifi` y `wifi buscar`;
- hasta dónde llegó: ¿se reconoce?, ¿carga el firmware?, ¿ve redes?, ¿se asocia?, ¿hay tráfico?;
- si usas máquina virtual (usbipd, VirtualBox), dilo: el USB virtual cambia mucho.

Con eso se puede corregir el driver con el pincho en la mesa de otro (el depurador de drivers trabaja con informes así).

## Cómo está hecho

- `kernel/rede/wifi_usb_ids.h` (generado de las listas de OpenBSD por `tools/gerar_ids_wifi_usb.py`) y
  `wifi_usb_logica.h`: qué familia es cada VID:PID.
- Realtek «C»: `rtl8188eu.c` con ramas `fc(r)` (encendido, LLT, DMA, firmware, MAC/BB/RF, potencia, descriptor de TX) y la
  lógica pura en `rtl8192cu_logica.h`; tablas de OpenBSD en `kernel/terceros/bsd/rtwn/r92creg_92c.h`.
- Ralink: `rt2870.c` y `rt2870_logica.h`; registros y tablas de `run(4)` en `kernel/terceros/bsd/run/rt2860reg.h`.
- Pruebas: `python3 tests/h409_wifi_usb.py` (lógica en el anfitrión con ASan/UBSan contra firmware reales; fuentes;
  QEMU sin pincho con `wifi compatibles`); `tests/h215_wifi.py` sigue siendo la prueba del 8188EU real.
- Referencias (ISC): `if_urtwn.c`/`rtwn.c`/`r92creg.h` y `if_run.c`/`rt2860reg.h` de OpenBSD. **No se copió nada de
  los drivers de Linux** (rtl8xxxu, rt2800usb: GPL). Los firmware son de linux-firmware (`docs/terceros.md`).

## Atheros AR9271 (descrito, sin hacer)

El AR9271 (`0cf3:9271` y los otros 12 VID:PID de `athn(4)`) es un USB con el MAC/PHY ath9k y un 8051 (ath9k_htc) que
corre un firmware que **sí es libre** (open-ath9k-htc-firmware, ClearBSD/ISC; `htc_9271.fw` de linux-firmware, 50 KB).
Lo que falta, y por qué no entró en esta versión:

1. Carga del firmware por el endpoint de control del chip (comando de escritura a la dirección 0x501000 y salto), como
   `athn_usb_load_firmware` de OpenBSD.
2. El **protocolo HTC** (conexión de servicios por los endpoints bulk) y **WMI** (órdenes: canal, claves, filtros,
   estaciones), con su propio formato de tramas.
3. El inicio del PHY y de los registros `ar9285`-like (tablas `ar9271` de athn, muy largas) y la calibración.
4. Transmisión y recepción con las cabeceras HTC (`ath9k_htc_tx_hdr`, `rx_status`).

Es un trabajo del tamaño de los dos drivers anteriores juntos. Hasta entonces el sistema reconoce el pincho, lo
anota en el registro («reconocido, todavía sin driver») y lo deja quieto.
