# 014 - Información del PC (infopc, estilo CPU-Z)

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

No había forma de ver desde SaguiOS qué hardware detecta: modelo de CPU, frecuencia,
cachés e instrucciones, mapa de memoria, fabricante y BIOS de la placa, dispositivos PCI o
el modo de vídeo. Sirve para diagnosticar el G470 sin arrancar otro sistema, y para que los
tests comprueben que el kernel lee bien el E820, el SMBIOS, el PCI y el VBE.

## Propuesta

- Programa `/bin/infopc` (`userland/programas/infopc.c`): ventana con pestañas CPU, Memoria,
  Placa/BIOS, PCI, Vídeo y Sistema; `infopc -t [sección]` lo imprime en el terminal.
- Kernel (`kernel/nucleo/info_hardware.c`): copia del bloque VBE y de la tabla SMBIOS al
  arrancar (antes de la paginación), medida del TSC contra el PIT (100 ms) y los tipos
  `INFO_E820` (20), `INFO_SMBIOS` (21), `INFO_PCI` (22), `INFO_VIDEO` (23) e `INFO_CPU_MHZ`
  (24) de `SIS_INFO_SISTEMA` (`kernel/inc/abi.h`). `pci_listar` en `kernel/dispositivos/pci.c`.
- Entrada `infopc` en la ayuda de sagsh.
- Decisión D041.

## Criterios de aceptación

- [x] CPU: fabricante, nombre comercial, familia/modelo/stepping, microarquitectura,
      núcleos/hilos, cachés, instrucciones, hipervisor y frecuencia medida
- [x] Memoria: mapa E820, totales y nota del límite de ~892 MB (D008/D010, mejora 012)
- [x] SMBIOS: BIOS, sistema, placa, procesador y módulos de memoria (sin número de serie)
- [x] PCI con nombres de clase y de fabricante; vídeo VESA/VBE; resumen del sistema
- [x] Test automático en `tests/h39_infopc.py` (QEMU por defecto, `-cpu pentium2`,
      `-cpu core2duo -smp 2` y la ventana en el escritorio)
- [x] Decisión registrada en `docs/decisoes.md` (D041)
- [ ] Probado en el G470 (pendiente: ver notas)

## Notas

- En el G470 (Sandy Bridge, familia 6 modelo 0x2A) debería salir "Sandy Bridge" y la
  frecuencia nominal del TSC (no la del turbo).
- Riesgos en hardware real: la tabla SMBIOS puede estar por encima del área que se copia
  (se copian hasta 16 KB) o tener textos raros (se sustituyen por "?"); algunos BIOS
  dejan campos de los tipos 4/17 a cero o con "To Be Filled By O.E.M.". La lectura del PCI
  es la misma que ya hace el EHCI (solo configuración, sin escribir). Una BIOS solo UEFI no
  tendría el ancla en 0xF0000 (el G470 arranca en modo BIOS).
- Sin icono en el escritorio: la columna de iconos está llena (8 en 768 píxeles).
