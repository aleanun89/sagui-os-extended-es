# Soporte de tarjetas gráficas (GPU) por versión

Estado a 2026-11-15 (v8.0). **Leyenda:** ✅ funciona y probado · 🟡 hecho, falta confirmarlo en hardware real ·
🔧 en desarrollo · 📅 planificado · — no previsto. El 3D por software (SaguiGL) funciona en **todas** las tarjetas.

## Lo que hay hoy y lo que viene

| Familia (ejemplos) | Imagen (pantalla) | 2D acelerado | 3D por GPU | Vulkan | Máquina de prueba |
| --- | --- | --- | --- | --- | --- |
| **Cualquier GPU** con VESA (BIOS) o GOP (UEFI) | ✅ VBE desde siempre; GOP desde v8.0 | — (copia por la CPU, optimizada) | — (SaguiGL por software) | — | todas |
| **QEMU estándar** (Bochs/BGA), virtio | ✅ cambio de resolución en caliente | — | — | — | QEMU |
| **VMware SVGA II** (VMware, `-device vmware-svga`) | ✅ | ✅ v7.2 (D195) | — | — | QEMU/VMware |
| **Intel Gen3** GMA 900/950, 3100 (945, G33) | ✅ | ✅ v7.2, blitter (D195) | 🔧 **v8.3** (D285) | — | portátiles con 945 |
| **Intel Gen4-Gen5** GM45, HD de 1.ª gen. Core (Ironlake) | ✅ | ✅ v7.2, blitter (D195) | 📅 v11 (111) | — | Toshiba L635 (Gen5) |
| **Intel Gen6** HD 2000/3000 (Sandy Bridge) | ✅ | ✅ v7.2, blitter por BCS (D195) | 📅 v11 (111) | — | Lenovo G470 |
| **Intel Gen7** HD 4000 (Ivy Bridge), Haswell | ✅ (VESA/GOP) | 📅 v11 (111) | 📅 v11 (111) | 📅 v11, hasvk (64 bits) | **Toshiba L850** |
| **Intel Gen8-Gen12** Broadwell…Alder Lake (UHD, Iris Xe) | ✅ (GOP) | 📅 v11 (111) | 📅 v11 (111) | 📅 v11, ANV (64 bits) | **mini PC i3-1220P** (Gen12) |
| **Radeon R5xx** X1300-X1950 (Mobility X1400) | ✅ | 🟡 v7.2 (issue #3) | 🟡 **v8.3** (D287): escrito sobre `gpu3d`, apagado hasta `gpu3d encender`; falta probarlo en el Amilo | — | Fujitsu Amilo |
| **Radeon R6xx/R7xx** HD 2000-4000 (HD 4530/4550) | ✅ | 🟡 v7.4, motor DMA (D223, D246; issues #1, #2) | 🔧 **v8.3** (D284) | — | **HP dv7** |
| **Radeon Evergreen/NI** HD 5000-6000 (Cedar…Hemlock, Barts, Turks, Caicos, Cayman; sin las APU) | ✅ (VESA/GOP) | 🟡 **v8.3**, motor DMA (D286), sin probar en hardware | — | — | Toshiba L635 (Mobility HD 5000, `1002:68e0`) |
| **Radeon GCN** HD 7000, R7/R9, RX 400/500 (Polaris), Vega | ✅ (GOP) | 📅 v11 por SDMA (110) | 📅 v11 (110) | 📅 v11, RADV (64 bits) (108) | (por definir) |
| **Radeon RDNA 1/2/3** RX 5000-7000 | ✅ (GOP) | 📅 v11 (110, tras GCN) | 📅 v11 (110) | 📅 v11, RADV (64 bits) (108) | (por definir) |
| **NVIDIA GeForce 6/7**, nForce 6100-7050 | ✅ | 🟡 v7.4 (D226), sin probar en hardware | — | — | — |
| **NVIDIA posteriores** (8000 en adelante) | ✅ (VESA/GOP) | — | — | — | — |

## Otras piezas gráficas por versión

| Versión | Qué trae |
| --- | --- |
| v7.2 | Capa 2D común con autoprueba (D195); Intel Gen3-Gen6, X1400 y VMware |
| v7.4 | Radeon R6xx/R7xx por DMA (D223), GeForce 6/7 (D226) |
| v8.0b-v8.0b4 | Pestaña GPU en Información del PC con benchmark 2D (D243), vigilante del 2D y tormentas de IRQ (D244), write-combining (D245), R7xx revisado (D246) |
| v8.0 | Arranque UEFI con **GOP**: imagen en cualquier GPU moderna sin BIOS (D264) |
| **v8.3** (en desarrollo) | **3D por GPU** en R5xx, R6xx/R7xx e Intel Gen3, **2D en Radeon Evergreen/NI** con una **capa 3D común** (pensada ya para amdgpu/Vulkan), renderizador de la SDL 2 por GPU, varias pantallas (fase 1), puntuación 3D |
| v8.3.1 | Ejemplos y benchmarks 3D, monitor de uso de la GPU |
| v10.0 | SaguiOS de **64 bits** (base para Vulkan), sin quitar los 32 bits |
| **v11** | **GPU modernas, Intel primero**: Intel Gen5-Gen12 (111) y luego AMD GCN/RDNA (107, 110): pantalla nativa, 2D, 3D en 32 bits y **Vulkan** en 64 bits (ANV/hasvk, RADV) (108) |

## Qué probar en el Toshiba L635 (Radeon Evergreen, D286)

1. Arrancar normal y guardar el registro: `dmesg > /home/evg.txt` (o una foto de las líneas `[ACEL2D]`).
2. Si pone `[ACEL2D] activo: Radeon HD 5400 (Cedar) …`: `acel2d`, `acel2d probar --detalle`, otro `dmesg`, y la pestaña GPU de Información del PC (benchmark 2D). Mover ventanas, desplazar texto en el terminal, abrir un juego.
3. Si pone `[ACEL2D] CPU: …`, el motivo dice qué comprobación no cuadró (controlador de pantalla, memoria, DMA ocupado…): enviar el registro entero. Si la pantalla es de la Intel del portátil (`es de la tarjeta 8086:…`), el driver de la Radeon ni se intenta.
4. Ante cualquier problema de imagen: `acel2d apagar`, o arrancar con `--sin-acel2d`.

Más detalle: mejoras 054, 055, 060, 106-111 y la discusión de GitHub sobre v9.x/v10/v11.
