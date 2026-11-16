# 125 - Más RAM en 32 bits: PAE (hasta 64 GB), memoria alta y NX

- **Estado:** en curso: fases 1 y 2 hechas (D348, D349); falta la 3 (**v8.5**, propuesta del coordinador tras la decisión del mantenedor del 2026-11-15)
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
- [x] Memoria alta: más de 1,875 GB útiles sin PAE (hasta lo que deje el hueco PCI) — D348, h350
- [x] PAE: más de 4 GB en total con varios procesos; doble modo con y sin PAE — D349, h351
- [x] NX activo donde exista, sin romper DOSBox ni el JIT — D349, h351 (DOSBox pide su caché con mprotect)
- [ ] Caché de disco y swap comprimida usando la RAM alta (pendiente: D348 deja la caché en la baja)
- [x] Tests automáticos y decisión en `docs/decisoes.md` (D348/h350, D349/h351)

## Fase 1 hecha (v8.5, D348)
- Zona alta en `kernel/mem/fisica.c` (de 0x78000000 al fin de la RAM bajo 4 GB, según E820/UEFI), solo para
  páginas de programas (`memfisica_alocar_usuario`); kernel, tablas y DMA en la baja.
- Ventanas temporales por CPU (`kernel/mem/memalta.c`, estilo `kmap_atomic`) para poner a cero, cargar ELF y
  preparar pilas; auditor `tools/auditar_memalta.py`.
- `free`, `mem` e `infopc` muestran total, baja y alta. `SIS_MAPEAR_BAR` no deja mapear RAM.
- Prueba `tests/h350_memoria_alta.py` (3G, 4G, SMP, UEFI, 512M, 1G; TCG y KVM).

## Fase 2 hecha (v8.5, D349)
- Doble modo elegido al arrancar: PAE (3 niveles, PDPT por proceso con los directorios del kernel compartidos,
  mapa 1:1 en páginas de 2 MB) si la CPU lo anuncia; el modo clásico de 2 niveles si no, o con `mkimagem.py
  --sem-pae`. Un solo `paginacao.c` para los dos (geometría en variables, `pte_t`/`fis_t` de 64 bits, escritura de
  las entradas de 8 bytes en el orden seguro).
- Zona superior (de 4 GB a 64 GB) solo para programas; direcciones físicas de 64 bits en `memfisica_*`,
  `espaco_traduzir`/`tirar`, futex, ventanas de `memalta` y cargador.
- NX (`EFER.NXE` en todas las CPU, también en el trampolín SMP): pila, montón, datos ELF, ventanas, BAR y el mapa
  1:1 del kernel por encima de la imagen no ejecutables; `MEM_PROTEGER`/`mprotect`/`mmap(PROT_EXEC)` para el código
  generado (DOSBox `core=dynamic`). `--sem-nx` lo desactiva.
- `free`, `mem` e `infopc` dicen el modo y la memoria por encima de 4 GB. Prueba `tests/h351_pae_nx.py`.

## Falta (fase 3)
- Caché de disco y swap comprimida en la alta/superior (necesitan ventanas en todos los usos de sus bloques).
- W^X: los binarios C++ con `.init_array` tienen el primer segmento RWX (`programa_cxx.ld`).
- `usuario_ok` y la pila que crece mientras otra CPU duerme en el mismo proceso (hallazgo de la revisión, anterior a la D349).
- Probar en hardware el camino de XD_DISABLE (Intel con NX apagado en la BIOS) y el de una CPU secundaria sin NX.
- Hardware real con más de 4 GB (mini PC i3-1220P, Ryzen B450) y con PAE sin NX.
