# 062 - Arranque UEFI

- **Estado:** en curso en la rama `v8.0-uefi` (D264: cargador, imagen híbrida y prueba h264 hechos; falta el PC real y la ISO). Plan: (**v8**: cargador UEFI x64/ia32, imagen híbrida, NVMe e instalador con GPT; **v9**: pulido). Adelantada de la v10 el 2026-11-13.
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 026 (vídeo, VBE), 030 (ACPI), 017 (varios núcleos), 046 (xHCI), 060 (pantallas), Ryzen B450

## Problema / motivación

SaguiOS arranca solo por **BIOS clásica** (MBR, cargador NASM en modo real, VBE para el vídeo).
Muchos PC modernos ya no traen el modo de compatibilidad (CSM), o lo traen pero desactivado,
y algunos portátiles recientes solo arrancan por UEFI. Sin UEFI, SaguiOS no arranca en esas
máquinas.

## Propuesta

- **Cargador UEFI propio** (`arranque/uefi/`), un `BOOTIA32.EFI` y un `BOOTX64.EFI` en
  `\EFI\BOOT\` de una partición del sistema EFI (ESP, FAT32, tipo GPT `EF00`, o `0xEF` en MBR).
  Usa los servicios del firmware para:
  - leer el kernel y el ramdisk de la ESP (Simple File System);
  - pedir el **mapa de memoria** y convertirlo al formato E820 que ya usa el kernel;
  - elegir un modo de vídeo con **GOP** (sustituto de VBE) y pasar el *framebuffer* lineal;
  - encontrar las tablas **ACPI** (RSDP) y **SMBIOS** en la tabla de configuración;
  - llamar a `ExitBootServices` y saltar al kernel en modo protegido de 32 bits, como hoy.
- **Firmware de 64 bits con kernel de 32 bits:** el `BOOTX64.EFI` tiene que salir del modo
  largo y volver a modo protegido antes de saltar (paso delicado, con pruebas propias).
- **Kernel:** recibe una estructura de arranque común («qué dejó el cargador»): BIOS o UEFI,
  mapa de memoria, *framebuffer* y RSDP. Sin servicios de la BIOS en tiempo de ejecución:
  todo lo que hoy pide a la BIOS (VBE, E820, discos por INT 13h en el arranque) debe venir
  del cargador.
- **Imagen híbrida:** el mismo pendrive o ISO arranca por BIOS (MBR) y por UEFI (ESP), con
  `tools/mkimagem.py --uefi`.
- **Secure Boot:** fuera del alcance. Hay que desactivarlo en la BIOS, y se documenta.
- **Reloj y apagado:** tiempo por RTC como hoy; apagado con ACPI (030), no con los servicios
  de ejecución de UEFI.

## Pruebas

- **QEMU con OVMF** (firmware UEFI libre): arranque de 32 y 64 bits, mapa de memoria,
  vídeo GOP a 1024x768 y otras resoluciones, y el escritorio completo.
- La batería completa por BIOS y una selección por UEFI.
- **Real:** el Ryzen B450 con CSM desactivado, y algún portátil reciente solo UEFI.

## Instalador con GPT y ESP (D266, rama `v8.0-gpt-esp`)

El instalador (texto y ventana) elige tabla **MBR (BIOS)** o **GPT (UEFI)**; con GPT crea ESP (FAT32), SaguiOS,
/home y SAGUI-CONF, y copia a la ESP lo que haya en **`/sistema/efi/`** (`EFI/BOOT/BOOTX64.EFI`...). `particiones`
y la ventana de Discos crean, borran y formatean en GPT. Detalle en D266 y `tests/h266_gpt_esp.py`.

## Criterios de aceptación

- [ ] Arranca en QEMU+OVMF (ia32 y x64) hasta el escritorio
- [ ] Imagen híbrida BIOS+UEFI que arranca de las dos formas
- [ ] Arranca en un PC real por UEFI sin CSM
- [x] Test automático en `tests/` (h264_uefi.py)
- [x] Decisión registrada en `docs/decisoes.md` (D264)

## Notas

- Modelo sugerido: **Opus medio** (cargador, cambio de modo de CPU y hardware real).
- Hecho sin `gnu-efi` ni EDK2: definiciones UEFI propias (`boot/uefi/uefi.h`) y el PE con
  `tools/mkefi.py` (D264). Guía para el PC real: `docs/uefi.md`.
- **USB 1.1 en PC antiguos (v8.0, D267, hecho):** los drivers UHCI y OHCI (`kernel/usb/uhci.c`, `ohci.c`) llevan teclado, ratón y pendrive de baja y plena velocidad en los PC sin xHCI, a partir de los compañeros del EHCI. No dependen del cargador UEFI; en un PC solo UEFI sirve el xHCI (D214). Ver `docs/usb11.md`.
