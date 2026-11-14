---
name: maquinas-prueba-fisicas
description: Máquinas físicas en las que el mantenedor prueba SaguiOS y lo que se ha visto en cada una
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T23:16:48.349Z
---

- **Fujitsu Amilo Pi 1536** (Core Duo T2400, ICH7-M, SATA AHCI `8086:27c5`, Mobility Radeon X1400, RTL8169SB, EHCI + 4 UHCI). v7.1: el disco SATA no respondía al IDENTIFY y el arranque esperaba 20–40 s → D199. Con IOAPIC el USB iba lentísimo (las imágenes SMP van sin IOAPIC). Fecha 01/01/2007: pila CMOS. USB sin funcionar (¿dispositivo USB 1.x al compañero UHCI sin driver?).
- **Toshiba Satellite L635** (Core i3 M370, 2 núcleos/4 hilos; PCH serie 5 `8086:3b..`; Mobility Radeon HD 5000 Evergreen `1002:68e0`; WiFi Broadcom; HDA codec Conexant `14f1:5069`; 2 EHCI con hub interno, sin compañeros; SSD Patriot Burst 120 GB). v7.1 SMP (2026-11-13): arranca bien con el disco en **modo compatibilidad (IDE)**, los 4 hilos arrancan, pendrive USB y particiones /home, /jogos, /apps montadas, EIST activo. Pendiente: probar el modo AHCI con la imagen de D199. **Red sin driver:** Ethernet **Atheros AR8152 v1.1 `1969:2060`** (Fast Ethernet, familia atl1c) → propuesto driver `alc` para la v7.2 tomando como referencia `alc(4)` de FreeBSD (BSD, vale para la compilación sin GPL), experimental como el r8169 porque el QEMU no lo emula; WiFi **Broadcom BCM4313 `14e4:4727`** (integrada, firmware) → fase 8. Su informe (`informe-20100102-0021.txt`) destapó que `discos` llenaba el log del kernel.

**How to apply:** al tocar AHCI, USB o arranque, pedirle prueba en estas máquinas; distinguir modo IDE/AHCI de la BIOS.

Relacionado: [[plan-acel2d-gpus]], [[carga-wsl-concurrencia]]
