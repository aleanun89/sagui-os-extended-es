# 012 - Usar la RAM por encima de 896 MB

- **Estado:** en curso (primera parte hecha: frontera en 2 GB, D049)
- **Prioridad:** baja
- **Alta:** 2026-11-06

## Problema / motivación

El kernel solo gestiona ~892 MB aunque el G470 tiene ~8 GB (3017 MB por debajo de 4 GB).
Es una decisión de diseño (D008/D010): el kernel accede a la memoria física por su propia
dirección (mapeo 1:1 del primer GB virtual) y los programas empiezan en 0x40000000; de ese
GB, los últimos 128 MB (0x38000000-0x3FFFFFFF) son "ventanas" para el framebuffer VESA y el
EHCI. El límite es `MEMFIS_LIMITE` (`kernel/inc/memoria.h`).

## Primera parte, hecha (D049): mover la frontera a 2 GB

La opción más sencilla: el kernel se queda con los 2 primeros GB virtuales en lugar de 1, y
los programas empiezan en 0x80000000.

- Mapeo 1:1 hasta el final real de la RAM (redondeado a 4 MB), con tope en 0x78000000;
  ventanas del kernel en 0x78000000-0x7FFFFFFF; `MEMFIS_LIMITE` = 0x78000000.
- Páginas de 4 MB (PSE) para el mapeo del kernel, globales con PGE; tablas de 4 KB solo en
  los bloques que tocan regiones reservadas (y en los primeros 4 MB).
- Resultado en QEMU (`mem`): ~1,9 GB gestionados con 2 GB o más (1917 MB; en el G470,
  1920 MB menos lo reservado por debajo de 0x78000000); con 512 MB y 256 MB, toda la RAM,
  como antes (~509 MB y 253 MB).
- Cambio de ABI: los ELF se enlazan en 0x80000000 (programa.ld y el `cc`); hay que
  recompilar los ejecutables que el `cc` generó antes.
- Prueba: `tests/h46_memoria.py` (256 MB, 512 MB, 2 GB con un programa de 1300 MB, 3 GB).

## Pendiente

- **Memoria alta con `kmap`** (abajo, "Propuesta"): la RAM entre 0x78000000 y 4 GB (en el
  G470, ~1,1 GB más) solo se puede usar con un asignador aparte y una ventana temporal.
  Ahora el límite está en 1920 MB en lugar de 896 MB.
- **Caché de bloques de disco** que aproveche la memoria que sobra.

## Propuesta

Opción recomendada ("highmem" solo para usuario):

- Asignador de páginas altas (896 MB-4 GB) separado del bajo (`kernel/mem/fisica.c`).
- Ventana temporal `kmap`/`kunmap` en la franja de ventanas (`paginacao.c`).
- Las páginas de los programas salen de la reserva alta (`espaco_alocar`), y quien escribe
  en ellas desde el kernel (`carregador.c: copiar_para`, autotest) pasa por `kmap`.
- Las tablas de páginas, `kmalloc` y los búferes DMA siguen en memoria baja.
- ~100-200 líneas; gana ~2,1 GB en el G470, todos para programas.

Descartadas: higher-half (no gana memoria por sí sola y rompe física = virtual en todo el
kernel) y PAE (>500 líneas y riesgo alto, para procesos que igualmente tienen 2 GB
virtuales).

## Criterios de aceptación

- [x] `mem` muestra ~1,9 GB gestionados con `-m 2G` y `-m 3G` (primera parte, D049)
- [ ] `mem` muestra ~3 GB gestionados en QEMU con `-m 3G` y en el G470
- [ ] Autotest que fuerza páginas altas (carga de ELF, `aumentar_memoria`)
- [ ] Batería completa en verde
- [ ] Decisión en `docs/decisoes.md`

## Swap y caché de disco (consulta del 2026-11-07)

- **Swap sobre la RAM sobrante (tipo zram) o en disco:** descartado por ahora. Necesita la misma
  ventana `kmap` que esta mejora, y además un manejador de fallos de página, una política de
  expulsión y copias de 4 KB en cada ida y vuelta (~600-1.000 líneas). Con la memoria alta
  usada directamente, las páginas no tienen que moverse.
- **Caché de bloques de disco en memoria alta:** sí, como segunda parte de esta mejora,
  cuando estén ext2 y el disco ATA (013). Es memoria que se puede vaciar en cualquier momento,
  sin la complejidad del swap.
- **Cuándo:** después de integrar la rama `sistema-archivos` (013), porque toca
  `paginacao.c`, `fisica.c` y el cargador de programas.

## Opción más sencilla: mover la frontera a 2 GB (consulta del 2026-11-07)

Ampliar el mapeo 1:1 del kernel de 1 GB a 2 GB moviendo `USUARIO_INICIO` de 0x40000000 a
0x80000000 (cada programa queda con 1 GB virtual, de 0x80000000 a 0xBFFFFFFF):

- El kernel gestiona ~1,9 GB sin `kmap`: solo cambian constantes (`MEMFIS_LIMITE`,
  `KERNEL_JANELAS`, `USUARIO_*`), el mapeo de arranque (con páginas de 4 MB, PSE),
  `userland/programa.ld` y las direcciones fijas del `cc` (D033).
- Es un cambio de ABI: todo se recompila desde el código fuente; los ejecutables generados
  antes con `cc` dentro de SaguiOS habría que recompilarlos. Necesita decisión nueva (el PRD
  fija 0x40000000).
- ~50-100 líneas, riesgo bajo. Por encima de 2 GB no conviene ampliar el 1:1: entre 3 y 4 GB
  está el MMIO de PCI (framebuffer).
- **Recomendado como primer paso**; la memoria alta con `kmap` (hasta ~3 GB) y la caché de
  bloques quedan para después y son compatibles.
- **Cuándo:** después de integrar la rama `shell-completo`, que cambia el cargador
  (`carregador.c`) para el entorno de los programas.

- [ ] Decisión en `docs/decisoes.md` (la primera parte está en D049)
