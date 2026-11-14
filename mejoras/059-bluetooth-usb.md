# 059 - Bluetooth por adaptador USB

- **Estado:** propuesta, repartida entre la **v9** y la **v11** (ver `docs/ROADMAP.md`)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Depende de:** 046 (xHCI) y 029 (HID USB), ambas en la v8; 041 (sonido) para el audio
- **Relacionadas:** 027 (libtls: P-256 para el emparejamiento), D073 (firmware de adaptadores)

## Propuesta por fases

| Versión | Fase | Contenido |
| --- | --- | --- |
| **v9** | Base y entrada | Driver USB de **clase Bluetooth** (0xE0/0x01/0x01: órdenes por control, eventos por interrupción, ACL por *bulk*); firmware de los adaptadores que lo piden (Intel, Realtek) desde linux-firmware (D073), opcional; **HCI**, **L2CAP** y **SDP**; buscar dispositivos; **emparejamiento** (SSP en clásico con ECDH P-256 de la libtls; SMP en BLE con AES-CMAC/CCM); claves guardadas en `/home`; **teclado y ratón Bluetooth** (HID clásico y BLE con GATT/ATT/HOGP) por el mismo camino que el HID USB (029); aplicación «Bluetooth» para buscar, emparejar y olvidar |
| **v10** | Audio | **A2DP** con códec **SBC** (codificador propio) y AVDTP, sobre el mezclador de la 041: auriculares y altavoces Bluetooth; AVRCP básico (volumen, reproducir/pausa) |
| **v11** | Datos | **OBEX** (enviar y recibir archivos, p. ej. del móvil); **PAN/BNEP** (red por Bluetooth, *tethering* del móvil) sobre la pila IPv4 |

## Pruebas

- QEMU no emula adaptadores Bluetooth. Se prueba con un **adaptador real**, pasado al WSL con
  usbipd y a QEMU con `usb-host` (si el QEMU local lo soporta), o directamente en los
  portátiles.
- Pruebas en el anfitrión de los protocolos con tramas grabadas (HCI, L2CAP, SMP con vectores
  de la especificación) y de SBC contra un decodificador de referencia.

## Criterios de aceptación

- [ ] v9: emparejar y usar un teclado y un ratón Bluetooth (clásico y BLE)
- [ ] v10: audio A2DP en unos auriculares Bluetooth
- [ ] v11: enviar/recibir un archivo por OBEX y navegar por PAN
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

- Tamaño orientativo: v9 ~6.000-8.000 líneas; v10 ~3.000; v11 ~2.500.
- Modelo sugerido: **Opus medio** (protocolos con criptografía y hardware real sin emulador).
