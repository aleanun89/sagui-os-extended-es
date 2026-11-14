# 017 - Varios núcleos (SMP) e hilos de usuario

- **Estado:** propuesta, repartida en la **v9** (APIC, arranque de núcleos, cerrojo global) y la **v10** (cerrojos finos, planificador por núcleo, hilos de usuario) — ver `docs/ROADMAP.md`
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

SaguiOS usa un solo núcleo. El G470 (Core i3/i5 de 2.ª generación) tiene 2 núcleos y 4 hilos.

## Propuesta

Fase 1, SMP con gran cerrojo del kernel (~800-1.200 líneas):
- ACPI MADT para encontrar los núcleos (APIC ID); APIC local (reloj por núcleo e IPI).
- Arranque de los AP (INIT-SIPI-SIPI, trampolín en modo real por debajo de 1 MB).
- Datos por núcleo: `atual`, TSS, pila del kernel, dueño de la FPU y proceso ocioso.
- Gran cerrojo: un solo núcleo dentro del kernel a la vez (encaja con D010); los
  programas en anillo 3 corren en paralelo.
- Planificador multinúcleo; IPI para replanificar e invalidar la TLB.
- Opción de arranque para usar un solo núcleo si algo falla en el hardware real.

Fase 2, hilos de usuario (~400-600 líneas): crear hilo con pila propia en el mismo espacio de
direcciones, `join`, mutex; ejemplo `mandelbrot` multihilo para medir la ganancia.

## Criterios de aceptación

- [ ] QEMU `-smp 4`: los 4 núcleos arrancan y ejecutan procesos a la vez
- [ ] Batería completa en verde con `-smp 1` y `-smp 4`
- [ ] Hilos: un programa con 4 hilos usa los 4 núcleos (gestor de tareas por núcleo)
- [ ] Probado en el G470

## Riesgos

Fallos de concurrencia intermitentes; en hardware real, un fallo al arrancar los AP suele
acabar en reinicio sin mensaje. Choca con `fpu.c` (SSE2 en el kernel) y con la medida de CPU
del gestor de tareas: por eso va después de ellos.
