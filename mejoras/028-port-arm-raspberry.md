# 028 - Port a ARM (Raspberry Pi)

- **Estado:** opción futura. **NO HACER POR AHORA.** Queda solo apuntada; no lanzar ningún
  hilo ni tocar código para esto sin que se pida de forma explícita.
- **Versión:** se decide después de la v10 (64 bits y código separado por arquitectura; mantenedor, 2026-11-15)
- **Prioridad:** baja
- **Alta:** 2026-11-07

## Problema / motivación

SaguiOS solo funciona en PC x86 de 32 bits. Llevarlo a una Raspberry Pi permitiría usarlo en
hardware barato y actual. No es una recompilación: aproximadamente la mitad del sistema se
porta casi tal cual y la otra mitad, la que habla con el hardware, hay que reescribirla.

## Qué se porta casi tal cual (C sin dependencias del PC)

- Gestor de ventanas, escritorio y dibujo: ya trabajan sobre un framebuffer en memoria.
- Programas de usuario: shell, Archivos, Editor, Markdown, visor, Paint, tareas, Sagui 3D…
- Sistemas de archivos: SGFS, FAT32, ext2/ext4.
- Red de IPv4 hacia arriba (ARP, TCP, DNS…) y TLS.
- MicroPython, que ya tiene soporte para ARM.

## Qué hay que reescribir

| Parte | En x86 | En la Raspberry Pi |
| --- | --- | --- |
| Arranque | MBR + etapa 2, BIOS, VBE | el firmware carga `kernel8.img` desde la SD (más sencillo) |
| Modo de CPU | GDT, IDT, TSS, int 0x80, anillo 3 | EL1/EL0, `svc`, tabla de vectores de excepción |
| Memoria | paginación de 2 niveles, frontera en 2 GB (D049) | MMU AArch64 de 4 niveles; caché y barreras a mano |
| Interrupciones y reloj | PIC, PIT, RTC | GIC / controlador de la Pi, temporizador genérico ARM; sin RTC (hora por red) |
| Vídeo | VESA | framebuffer pedido a la GPU por el *mailbox* (lo más fácil) |
| Teclado, ratón | PS/2 | **USB** (no hay PS/2) |
| Disco | ATA, USB EHCI | controlador SD/EMMC |
| Red | e1000, RTL8139 | Ethernet LAN9514/LAN7515 colgada del USB (Pi 3) |
| FPU/SIMD | x87, SSE2 (D032) | NEON |
| Ensamblador | NASM | GNU as AArch64 |
| `cc` | genera código x86 | generador de código ARM64 nuevo |

La parte más dura es el **USB**: sin él no hay ni teclado. El driver EHCI actual no sirve. La
Pi 3 usa DWC2, un controlador USB incómodo; la Pi 4 usa xHCI detrás de PCIe, con su propio
firmware.

## Propuesta (cuando se decida hacerlo)

1. **Separar lo dependiente de la arquitectura** dentro de x86, sin cambiar comportamiento:
   arranque, paginación, interrupciones, cambio de contexto y E/S por puertos van a
   `kernel/arch/x86/`, detrás de una interfaz común. Este paso también ayudaría a SMP
   ([017](017-smp-multinucleo.md)).
2. **Primero en QEMU** (`qemu-system-aarch64 -M raspi3b`), que emula bien la Pi 3: consola
   serie, procesos y shell; después framebuffer y ventanas; después USB (teclado/ratón) y SD.
3. **Después, la Pi real.** Recomendada: Raspberry Pi 3B/3B+ (mejor documentada, bien
   emulada y con USB más sencillo que la Pi 4).

## Criterios de aceptación

- [ ] El kernel x86 sigue pasando toda la batería tras separar `kernel/arch/`
- [ ] Arranca en `qemu-system-aarch64 -M raspi3b` hasta el shell por consola serie
- [ ] Escritorio y ventanas en el framebuffer de la Pi
- [ ] Teclado y ratón USB, tarjeta SD
- [ ] Probado en una Raspberry Pi 3 real
- [ ] Tests automáticos en `tests/` para la versión ARM (QEMU)
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Esfuerzo orientativo: de 2 a 4 veces el hilo más grande hecho hasta ahora (red o shell).
  El USB y la SD se llevan la mayor parte.
- Habría que decidir entre AArch64 (64 bits, recomendado) o ARMv7 de 32 bits, que se parece
  más al modelo actual pero es menos útil a futuro.
- Riesgo: mantener dos arquitecturas duplica el trabajo de pruebas de cada mejora posterior.
