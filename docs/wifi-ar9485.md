# Wi-Fi integrada Atheros AR9485 (Toshiba Satellite L850)

Guía para probar el driver `kernel/rede/ar9485.c` (D280, mejoras 072 y 079) en el portátil real. QEMU no emula
esta tarjeta: lo que sigue solo se puede comprobar en el L850 (`168c:0032`, PCIe, 802.11b/g/n 1x1, 2,4 GHz).

## Qué hace y qué no

- Sin firmware: el chip se programa por registros. La MAC y la calibración salen de su EEPROM/OTP.
- Mismos mandatos que con el pincho USB: `wifi`, `wifi buscar`, `wifi conectar SSID clave`, `wifi desconectar`,
  `wifi recordadas`, `wifi olvidar`, `ifconfig wlan0`. WPA2-PSK, WPA3 (SAE), WPA/TKIP, WEP y redes abiertas, como
  con el RTL8188EU (D215, D220).
- Se asocia como 802.11 b/g (sin 802.11n): velocidades de hasta 54 Mb/s. 5 GHz no existe en este chip.
- El CCMP va por software; para probarlo por hardware, `cifrado hw` en `/home/.config/wifi-desempenho.conf`.
- Un solo chip Wi-Fi a la vez (si hay un pincho USB, se usa el que arranque primero).

## Pasos

1. **Interruptor de radio.** Si el L850 tiene el interruptor físico o la tecla Fn de la Wi-Fi, que esté **encendido**; el
   driver lo lee por el GPIO que dice la ROM y, si está apagado, lo dice y no arranca.
2. **Arrancar** la imagen y abrir el Terminal. Mirar el registro: `dmesg | grep -E "AR9485|WIFI"`. Debe salir, por este orden:
   - `[AR9485] tarjeta Atheros AR9485 (168c:0032) en BB:DD.F`
   - `[AR9485] SREV xxxxxxxx: versión 240 revisión N` (la versión **tiene** que ser `240`; si es otra, el chip no es un AR9485)
   - `[AR9485] EEPROM de 4096 bytes` (o `OTP`)
   - `[AR9485] ROM vN, plantilla M, MAC xx:xx:xx:xx:xx:xx, 1T1R (máscaras 1/1), 11g sí, 11n sí`
   - `[AR9485] chip listo, canal 1 (2412 MHz)` y una línea de diagnóstico con registros y contadores
   - `[WIFI] Atheros AR9485 (revisión N), MAC ... (ROM), CCMP por software`
3. **Buscar redes:** `wifi buscar`. Deben aparecer las redes de 2,4 GHz cercanas (los SSID ajenos se ofuscan). Si no sale
   ninguna, mirar el diagnóstico (`wifi` y el registro): `rx` a 0 quiere decir que el anillo de recepción no recibe nada.
4. **Conectar:** `wifi conectar "SSID" clave`, luego `wifi`, `ifconfig wlan0`, `ping 8.8.8.8`, `wget`/`curl` por HTTPS.
   Si falla, el registro (`[WIFI] ...`) dice el paso: autenticación, asociación (con su estado), «la contraseña no es
   correcta» si el saludo de cuatro pasos no termina en 8 s.

## Qué mirar si algo no va

| Síntoma en `dmesg` | Probable causa |
| --- | --- |
| no sale `[AR9485]` | el PCI no ve `168c:0032` (otro modelo: mirar `infopc`) |
| `el chip no contesta (lecturas 0xFFFFFFFF)` | la tarjeta está apagada o el enlace PCIe caído (interruptor de radio, ASPM) |
| `versión de MAC distinta` | no es un AR9485 |
| `no se encuentra la EEPROM ni la OTP` / `ningún bloque con suma buena` | lectura de la ROM; mandar el registro |
| `el reinicio del chip falló (-N)` | -1 despertar, -2 reinicio, -3 interruptor de radio, -4 sintetizador, -5 calibración del AGC |
| `wifi buscar` sin redes, `rx 0` | no recibe: la ROM, las tablas o el anillo; mandar el diagnóstico |
| ve redes pero no asocia | autenticación y asociación **sí** usan la transmisión: mirar `tx` y `llena`/`underrun` |
| asocia pero el saludo no termina | el cifrado: probar `cifrado hw` en `wifi-desempenho.conf` (o quitarla) |

Mandar al mantenedor el `dmesg` completo de `[AR9485]` y `[WIFI]`; con eso se corrige sin repetir pruebas.

## Cómo está hecho

Ver D280 en `docs/decisoes.md` y `kernel/rede/ar9485.c`. La fuente es el athn(4) de OpenBSD (ISC), con sus registros y tablas
en `kernel/terceros/bsd/athn/` (`docs/terceros.md`).
