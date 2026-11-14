# 089 - Multinúcleo: los procesadores secundarios se encienden

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-11

## Problema / motivación

El sistema tenía los cimientos del SMP (la D150: atómicas, cerrojo de espera
activa y datos por CPU) pero **no encendía ningún procesador secundario**: la
MADT del ACPI se leía y se contaba, y nada más. Con varios núcleos en la
máquina, el kernel usaba uno solo y todo el trabajo (el planificador, la
consola, la memoria) estaba hecho como si no pudiera haber otro núcleo
tocándolo.

## Propuesta

Encender los procesadores secundarios de verdad y que el sistema vivo con
ellos:

- **El arranque (INIT-SIPI).** El proceso principal lee la MADT (la tabla
  «APIC» del ACPI) y, por cada procesador secundario, copia un **trampolín** a
  la memoria baja (0x7000), le escribe lo que necesita (GDT propia, IDT, CR3,
  pila y función de entrada) y le manda un **INIT** seguido de una **SIPI**.
  El trampolín pasa a modo protegido, carga la GDT y la IDT de esa CPU, el GS
  (donde están sus datos), su TSS y su CR3, y salta al kernel.
  Ficheros: `kernel/arch/lapic.c`, `kernel/arch/ap_trampolim.asm`,
  `kernel/nucleo/smp.c`, `kernel/inc/lapic.h`, `kernel/inc/smp.h`.
- **El estado por CPU.** Cada CPU tiene su propia GDT (con su TSS y su
  descriptor de GS) y su propia pila de kernel. El «proceso actual» se lee por
  GS (`mov %gs:28`), como en Linux: el segmento ya dice en qué CPU se está.
  Ficheros: `kernel/arch/gdt.c`, `kernel/inc/cpu_dados.h`,
  `kernel/nucleo/cpu_dados.c`.
- **El planificador por CPU.** El proceso actual, la rebanada y el ocioso son
  de cada CPU; la tabla de procesos es compartida y se protege con un cerrojo
  para que dos CPUs no elijan el mismo proceso. Cada CPU tiene su propio
  ocioso. Ficheros: `kernel/nucleo/processos.c`.
- **El reloj por CPU.** El PIT sigue siendo el tic del principal; cada
  secundario tiene el temporizador de su APIC local, calibrado contra el TSC.
  Ficheros: `kernel/arch/lapic.c`, `kernel/dispositivos/relogio.c`.
- **Los cerrojos.** El de espera activa ahora sabe de quién es (para que una
  IRQ no espere por el cerrojo del código que interrumpió) y se usa en la
  tabla de procesos y en la consola. Ficheros: `kernel/inc/spinlock.h`,
  `kernel/nucleo/spinlock.c`, `kernel/nucleo/console.c`.
- **La información por núcleo.** La llamada nueva `INFO_NUCLEOS` (61) da una
  entrada por CPU (estado, tiques, tiques ociosos, el proceso que ejecuta, el
  núcleo físico) y `info_processo_t` gana `cpu` y `n_hilos`. El Administrador
  de tareas muestra los núcleos; el `infopc` los lista en su informe.
  Ficheros: `kernel/inc/abi.h`, `kernel/nucleo/chamadas_processos.c`.

## Criterios de aceptación

- [x] Con 4 CPUs arrancan los 3 secundarios y el kernel dice `nucleos=4`.
- [x] Con 2 CPUs arranca 1 secundario; con 1 CPU, nada cambia.
- [x] El proceso actual es por CPU (el GS) y la tabla de procesos no se rompe
      con dos CPUs eligiendo a la vez.
- [x] Cada CPU lleva su cuenta de tiques y de tiques ociosos.
- [x] `INFO_NUCLEOS` devuelve una entrada por CPU y `infopc -t sistema` las
      lista.
- [x] Test automático en `tests/h133_smp_multinucleo.py`.
- [x] Decisión registrada en `docs/decisoes.md` (D157).

## Notas

- **Los modos del ICR del QEMU no son los de la Intel:** allí el INIT es el
  5 y la SIPI el 6 (no el 4 y el 5). Con los valores del manual, el 4 es un
  NMI y el NPI reinicia la máquina. El INIT y la SIPI además tienen que ir
  seguidos, sin esperas: el procesador recién reiniciado entra en el vector de
  reset de la BIOS, y la BIOS reinicia la máquina entera.
- **La BIOS ya enciende los AP.** Con SeaBIOS, los secundarios arrancan solos
  durante el POST y quedan aparcados; por eso hace falta el INIT (que los
  reinicia) antes de la SIPI.
- **Queda fuera (la fase 2 del ROADMAP):** la migración de IRQ, la afinidad,
  los IPI de reparto de carga y los hilos de usuario. El campo `n_hilos` está
  preparado para ellos.
