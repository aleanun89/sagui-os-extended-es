# Más Wi-Fi integradas PCIe (D410, experimental)

Estado: **reconocimiento y lógica pura**; solo el Atheros AR9485 (`docs/wifi-ar9485.md`) tiene driver. Lo demás está
marcado como experimental, **sin probar en hardware**.

## Qué hace hoy

Al arrancar, si no hay AR9485 (ni pincho USB), SaguiOS recorre el PCI y escribe en el registro (`dmesg | grep WPCI`) cada
tarjeta conocida:

```
[WPCI] Atheros AR9285 (168c:002b rev 01, subsistema 103c:xxxx) en 03:00.0: Atheros AR5008/AR9002 (ath9k, sin firmware), 1 cadena(s), 2,4 GHz, ROM: EEPROM 4K
[WPCI] experimental: reconocida pero sin driver todavia (firmware: ninguno); ver docs/wifi-pcie.md
```

Solo lee el espacio de configuración PCI; no toca la tarjeta. Sirve para saber con certeza qué lleva cada portátil.

## Tarjetas del catálogo (`kernel/rede/wifi_pcie_logica.h`)

| Chip | PCI | Familia | Cadenas | ROM | Firmware |
| --- | --- | --- | --- | --- | --- |
| AR9285 | 168c:002b | AR5008 | 1 (2,4 GHz) | EEPROM «4K» (64 palabras, 376 bytes) | ninguno |
| AR2427 | 168c:002c | AR5008 (como AR9285) | 1 | 4K | ninguno |
| AR9287 / AR9227 | 168c:002e / 002d | AR5008 | 2 | AR9287 (128 palabras, 727 bytes) | ninguno |
| AR9280/AR9281 | 168c:002a | AR5008 | 2, dos bandas | AR5416 (256 palabras, 3256 bytes) | ninguno |
| AR9380/AR9382/AR9580 | 168c:0030 / 0033 | AR9003 | 2-3 | AR9300 (bloques) | ninguno |
| **AR9485** | 168c:0032 | AR9003 | 1 | AR9300 | ninguno (**driver activo**, L850) |
| AR9462 / AR9565 | 168c:0034 / 0036 | AR9003 | 2 / 1 | AR9300 | ninguno |
| BCM4313 | 14e4:4727 | brcmsmac | 1 | SPROM/OTP | `bcm43xx-0.fw` y `bcm43xx_hdr-0.fw` |
| RT3090 | 1814:3090 | rt2800 PCI | 1 | EEPROM/eFuse | `rt2860.bin` |

## Plan por máquina

- **Toshiba L850 (AR9485):** sin cambios, sigue con `ar9485.c`.
- **Toshiba L635 (BCM4313, Broadcom):** brcmsmac (Linux, ISC) necesita el microcódigo `bcm43xx-0.fw` + `bcm43xx_hdr-0.fw`
  (610.812, linux-firmware). Licencia **LICENCE.broadcom_bcm43xx**: redistribuible completo y sin modificar, solo para chips Broadcom,
  con el acuerdo al lado; irá en `firmware/broadcom/` con su licencia (D073) cuando haya driver. El driver es un MAC propio
  (decenas de miles de líneas en brcmsmac con el PHY N): es una pieza aparte de varias semanas, no cabe aquí.
- **HP dv7 (probablemente AR9285):** con la radio apagada desaparece del PCI (el WMI de HP la enciende, D405). Con la imagen
  de esta rama, tras encender la radio, `dmesg | grep WPCI` dice qué tarjeta es. Si es AR9285, el siguiente paso es el driver
  AR5008: DMA de `ar5008.c`, tablas `ar9285_1_2`, calibración PA/IQ/ADC, ROM 4K (ya con su lógica y prueba aquí).
- **RT3090 (1814:3090):** `rt2800` es común con el RT3070 USB que hace otro agente en `v8.5.5.1-wifi-usb`. **No se duplica**: cuando
  esa rama aterrice, la lógica común (tablas de registros, formato de la EEPROM, mapa de canales) debe quedar en un
  `rt2800_logica.h` que use también la variante PCIe, y aquí solo se añade el acceso por MMIO y los anillos de DMA.
  Firmware `rt2860.bin` (v40), licencia `LICENCE.ralink-firmware.txt` (redistribuible).

## Qué falta para el driver AR9285/AR9287 (los más probables)

1. Sustituir las macros `AR_SREV_*` (hoy fijas al 9485 en `ar9485.c`) por la versión leída (`wpci_ath_srev`).
2. DMA del AR5008 (descriptores `ar_tx_desc`/`ar_rx_desc` de 20 palabras), distinto del de 128 bytes del AR9003.
3. Tablas de inicio del AR9285 1.2 (`ar9285_1_2_*`) y la rutina `ar9280_set_synth` (la palabra ya está calculada y probada).
4. ROM 4K: lectura por palabras (`AR_EEPROM_OFFSET`), magia, suma, y la tabla de potencias (la estructura ya está medida:
   376 bytes) con la interpolación de `ar9485_logica.h`.
5. Calibración (PA offset del AR9285, IQ, ruido) y el ciclo de vida igual que el del AR9485.

## Probar en las máquinas

Los portátiles pueden estar apagados: las órdenes están en `/home/aanun/SO/private/wifi-pcie/<máquina>/ORDENES.txt`
junto a un kernel de prueba.
