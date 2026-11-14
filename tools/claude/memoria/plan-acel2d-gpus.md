---
name: plan-acel2d-gpus
description: Plan del mantenedor para la aceleración 2D por GPU (copiar/rellenar) por versiones y las GPU de sus portátiles
metadata:
  node_type: memory
  type: project
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-12T20:38:37.287Z
---

Decisión del mantenedor (2026-11-12): aceleración 2D por la GPU (copiar y rellenar rectángulos para el escritorio) sobre una **capa común** (D195, rama `v7.2-gpu2d`) con **detección automática por PCI y autoprueba al arrancar** (si la GPU no copia/rellena igual que la CPU o no responde a tiempo → CPU); `--sin-acel2d` y `--acel2d=...` para forzar. Nada de «desactivado por defecto»: debe funcionar solo en muchos PCs distintos.

Por versiones:
- **v7.2:** VMware SVGA (QEMU, probado aquí), Intel Gen6 (G470 HD 3000) y Gen3 (sin máquina de pruebas), **AMD R5xx**: el **Amilo (Core Duo T2400) lleva una Mobility Radeon X1400** dedicada (RV515: motor 2D por MMIO, sin firmware) — es la máquina de pruebas del acelerador Radeon (no tiene Intel 945GM, como se supuso al principio).
- **v7.4 (propuesta):** **AMD R7xx** (portátil con **HD 4550**, RV710: copias por el motor DMA, con microcódigo) y **NVIDIA solo GeForce Go 6xxx/7xxx** (NV4x/G7x; referencia `xf86-video-nv` de NVIDIA, MIT). El mantenedor **no tiene portátil con GeForce Go**, pero puede tener **placas base antiguas con GeForce 7025/7050/7100/7150 (o 6100/6150) integrada en el chipset nForce** (AMD e Intel): misma familia NV4x/G7x, sirven de banco de pruebas para la v7.4 (pedirle `infopc` o `lspci -nn | grep -i vga`).
- **Fase 8 o después:** AMD HD 7730M (GCN 1.0). NVIDIA desde la serie 8 y modernas: no (Maxwell 2+ exige firmware firmado).

**Why:** los portátiles antiguos con CPU lenta son donde más se nota; el mantenedor prioriza soportar muchos PCs distintos de forma automática y segura.

Relacionado: [[plan-v73-wifi-bt]], [[carga-wsl-concurrencia]]
