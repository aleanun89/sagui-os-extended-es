---
name: plan-v73-wifi-bt
description: Plan del mantenedor para la v7.3 (WiFi/Bluetooth solo con pinchos USB concretos; integrados en la fase 8) y el hardware que tiene
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T20:17:12.947Z
---

Decisión del mantenedor (2026-11-12): la **v7.3** trae USB 3.0 (xHCI) y el soporte **inicial** de WiFi **solo con el pincho USB concreto**; las tarjetas **integradas** quedan para la **fase 8 (v8)**. **Bluetooth entero (pincho CSR8510 e integrados) pasa a la v8** (2026-11-13). v7.3.1: servicios + cron (074) y bloqueo DNS tipo Pi-hole, desactivado por defecto.

Hardware que tiene en su PC (Windows, `usbipd list`):
- **WiFi:** pincho **Realtek RTL8188EU** `0bda:8179` (BUSID 4-3). Conectado en frío aparece como CD virtual `1111:1111` con el instalador `.exe` (ZeroCD): SaguiOS tendrá que mandarle la orden de cambio de modo (como `usb_modeswitch`). Referencia: driver `rtl8xxxu` de Linux; firmware `rtl8188eufw.bin` (linux-firmware, redistribuible, no libre); el chip cifra CCMP por hardware; nos toca el MLME/WPA2.
- **Bluetooth:** solo el integrado **Intel AX210** `8087:0032` (necesita firmware de Intel) → fase 8. Para la v7.3 se recomendó comprar un pincho **CSR8510** (HCI estándar, sin firmware).
- Tiene **usbipd-win** instalado: se puede pasar el pincho a WSL y a QEMU (`usb-host` sobre EHCI) para probar el driver con hardware real; comprobar antes que el QEMU de `~/.local/saguitools` tenga soporte `usb-host` (libusb).

Relacionado: [[carga-wsl-concurrencia]], [[base-vs-apps-particiones]]

**Estado (2026-11-13):** el driver del RTL8188EU (D215, v7.3; seguridad D220, v7.3.2) **funciona con el router real del mantenedor**, aunque lento (probado por usbipd en QEMU). Rendimiento (802.11n, recepción por interrupción, menos copias) previsto para la v7.4.2.
