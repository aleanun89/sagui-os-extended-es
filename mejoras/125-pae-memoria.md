# 125 - Más RAM en 32 bits: PAE (hasta 64 GB), memoria alta y NX

- **Estado:** aprobada (**v8.5**, propuesta del coordinador tras la decisión del mantenedor del 2026-11-15)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** 069 (OOM, swap comprimida en RAM), 035 (almacenamiento), caché de disco (D281, v8.4), 106 (64 bits, v10), SMP (D164), DOSBox (D294, recompilador)

## Motivación
El mantenedor quiere **centrar SaguiOS en equipos antiguos**: muchos portátiles i3/i5/AMD de 2010-2016 tienen
gráficas modestas pero **8 o 16 GB de RAM**. Hoy el kernel solo usa **1,875 GB** (`MEMFIS_LIMITE`, D008/D049)
porque accede a la memoria física por una correspondencia directa dentro de su espacio de 4 GB. Es mejor
aprovechar esa RAM con **PAE en 32 bits** que obligar a 64 bits: se mantiene la retrocompatibilidad y todo lo
demás igual (la v10 x86-64 sigue como camino aparte).

## Propuesta
1. **Memoria alta (base de todo):** el kernel deja de necesitar toda la RAM mapeada. Lo que pasa del límite
   («highmem») es para páginas de programas, caché de disco y swap comprimida; el kernel la toca por **ventanas
   temporales** (estilo `kmap`) al ponerla a cero, copiarla, cargar ejecutables o hacer E/S. El DMA y las
   estructuras del kernel siguen en memoria baja.
2. **PAE:** tablas de páginas de 64 bits y tres niveles (PDPT, directorio, tabla); direcciones físicas de hasta
   36 bits (**64 GB**). Cada proceso sigue con su espacio de usuario de 32 bits, pero entre todos usan toda la RAM.
   - **Doble modo:** se elige al arrancar según CPUID (PAE presente, y no en la lista negra). Sin PAE (algunos
     Pentium M antiguos), el modo actual de siempre.
   - **NX** (bit 63, con `EFER.NXE` si la CPU lo tiene): pila y montón no ejecutables; la memoria que se
     ejecute de verdad (recompilador de DOSBox, JIT) la pide explícitamente con `PROT_EXEC`.
   - SMP: cada CPU carga el mismo CR3/PDPT; arranque BIOS y UEFI.
3. **Usar la RAM extra aunque los programas no la pidan:** caché de disco grande (v8.4) y **swap comprimida en
   RAM** (069) por encima de lo que usan los procesos.
4. **Información:** `free`, `infopc` y la bandeja muestran la RAM total, la usada por programas, caché y swap.

## Pruebas
- QEMU con `-m 6G`, `-m 12G` y `-m 16G` (TCG y KVM) y CPU sin PAE (`-cpu pentium2` o `-cpu ...,-pae`).
- Programa de prueba que reserva y escribe 2 GB por proceso en varios procesos a la vez; la suma pasa de 4 GB.
- NX: un programa que salta a la pila muere con un error claro; DOSBox con `core=dynamic` sigue funcionando.
- Batería completa en los dos modos (con y sin PAE).
- Hardware real: dv7 (4 GB), L850, mini PC i3-1220P y el Ryzen (más de 4 GB).

## Quién lo hace
**Opus** (núcleo: memoria y paginación), con pruebas en hardware real; revisión de otro Opus antes de fusionar.

## Criterios de aceptación
- [ ] Memoria alta: más de 1,875 GB útiles sin PAE (hasta lo que deje el hueco PCI)
- [ ] PAE: más de 4 GB en total con varios procesos; doble modo con y sin PAE
- [ ] NX activo donde exista, sin romper DOSBox ni el JIT
- [ ] Caché de disco y swap comprimida usando la RAM alta
- [ ] Tests automáticos y decisión en `docs/decisoes.md`
