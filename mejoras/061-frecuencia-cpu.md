# 061 - Control de la frecuencia de la CPU (P-states) con límite de temperatura

- **Estado:** hecha en QEMU con el driver simulado (D106, **v5.1**); falta probarla en el HP dv7
- **Prioridad:** alta (en el HP dv7 SaguiOS va a la mitad de velocidad)
- **Alta:** 2026-11-08
- **Relacionadas:** 050 (monitor de hardware, solo lectura), 051 (SaguiMark), 030 (ACPI), D073

## Problema

En el **HP dv7** (Athlon X2 QL-64, AMD familia 11h, **2 P-states**) el monitor muestra la CPU
siempre en el P-state **bajo**. La BIOS arranca la CPU en el estado de ahorro y es el sistema
operativo quien debe pedir el alto. SaguiOS no tiene driver de frecuencia, y el monitor de la
050 es solo lectura por diseño. Resultado: el dv7 funciona a **la mitad de su velocidad**
(~1 GHz en vez de 2,1 GHz).

## Propuesta

Driver de frecuencia **separado del monitor** (`kernel/dispositivos/frequencia.c`), que solo
escribe en el registro estándar de «P-state deseado» y **solo elige entre los estados que la
BIOS ya definió**:

| CPU | Mecanismo | Soporte |
| --- | --- | --- |
| AMD familia 10h-17h y posteriores (dv7 = 11h; Ryzen = 17h) | `PStateCtl` (MSR 0xC0010062) = número de P-state; límites en `PStateCurLim` (0xC0010061); definiciones en 0xC0010064+ | sí |
| Intel con EIST (Core Duo/Core 2/Core i: Amilo, G470) | `IA32_PERF_CTL` (0x199) con el multiplicador máximo de la propia CPU (`IA32_PERF_STATUS`/`PLATFORM_INFO` 0xCE) | sí |
| AMD K8 (FID/VID a mano), CPU sin EIST, máquinas virtuales | — | no: «control de frecuencia no disponible» |

**Políticas** (comando `frecuencia`, monitor y Administrador de tareas):
- **«rendimiento»** (por defecto): P0, el máximo.
- **«ahorro»**: el mínimo.
- **«a demanda»**: sube con carga y baja en reposo, según el uso de CPU con histéresis.

## Seguridad (obligatoria)

- **Temperatura máxima: 79 °C** (decisión del mantenedor, 2026-11-08). Si la CPU llega a
  **79 °C**:
  - se baja al P-state **mínimo** **sin avisos emergentes** (serían molestos): solo un
    **indicador** en la pestaña Rendimiento del Administrador de tareas («limitada por
    temperatura», en color, junto a la gráfica de frecuencia), en `sensores` y en el registro
    del kernel;
  - no se vuelve a subir hasta que baje de **72 °C** (histéresis de 7 °C).
  - El límite se puede **bajar** en la configuración, pero **no subir** por encima de 79 °C.
  - Se lee con el monitor de la 050 al menos una vez por segundo mientras el P-state no sea el
    mínimo.
  - Si la temperatura **no se puede leer**, no se sube del P-state que dejó la BIOS (salvo que
    el usuario lo fuerce sabiendo el riesgo).
- **Nunca** se tocan voltajes, VID/FID ni se inventan frecuencias: solo se elige un P-state que
  la BIOS ya declaró válido, respetando `PStateCurLim` (el límite que pone la propia
  plataforma).
- CPUID antes de escribir; escrituras protegidas como las lecturas de la 050 (un #GP se absorbe
  y desactiva el driver).
- Opción de arranque y `frecuencia --desactivar` para dejar la CPU como la dejó la BIOS.
- En los dos núcleos del dv7, cuando haya SMP (017): el mismo P-state en todos.

## Ventiladores

**No se tocan.** En los portátiles como el dv7 los controla el **controlador embebido (EC)**
con su propia curva, gestionado por la BIOS e independiente del sistema operativo: siguen
funcionando igual con SaguiOS. Algunos portátiles esperan, además, que el sistema reduzca la
frecuencia cuando hace calor (refrigeración «pasiva» de ACPI); el límite de 79 °C cubre ese
papel sin necesitar ACPI.

## Pruebas

- **QEMU:** no hay P-states reales. Se prueban la lógica, las políticas y la histéresis con un
  monitor simulado (temperatura y P-state inyectados) y el «no disponible» en máquina virtual.
- **Real (dv7):**
  - `sensores` antes y después (P0 y frecuencia real por APERF/MPERF ~2,1 GHz);
  - **SaguiMark** antes y después (se espera casi el doble);
  - una carga larga (SaguiMark en bucle o Sagui 3D) vigilando que, si llega a 79 °C, baja al
    mínimo y vuelve por debajo de 72 °C.
  - Resultados y fotos en `testes_hw/`.

## Criterios de aceptación

- [x] AMD 10h-17h e Intel EIST: P-state máximo por defecto; políticas ahorro y a demanda
      (Intel desde Nehalem; los Core 2 quedan fuera: ver «Lo hecho»)
- [ ] Límite de 79 °C con histéresis a 72 °C, probado en simulación ✔ y en el dv7 (pendiente)
- [x] Sin lectura de temperatura no se sube; opción para desactivar el driver
- [ ] SaguiMark en el dv7 antes y después (pendiente del usuario)
- [x] Test automático en `tests/` (`h78_frecuencia.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D106)

## Notas

Modelo sugerido: **Opus medio** (escribe en registros de la CPU en hardware real).

## Lo hecho (2026-11-08)

- **Driver** `kernel/dispositivos/frequencia.c`, aparte del monitor; lógica (políticas, límite,
  histéresis) en `kernel/lib/frequencia_pol.c`, probada en el anfitrión.
- **AMD 10h en adelante:** `PStateCtl` (0xC0010062) con los P-states de 0xC0010064+ habilitados
  hasta `PstateMaxVal`, nunca más rápido que `CurPstateLimit` (0xC0010061, releído cada 250 ms).
- **Intel desde Nehalem con EIST** (y sin HWP): solo el multiplicador de `IA32_PERF_CTL` (0x199),
  entre el mínimo y el máximo sin turbo de `MSR_PLATFORM_INFO`. **Los Core 2 / Core Duo (el
  Amilo) quedan «no disponible»**: allí el mismo registro lleva el VID (la tensión) y la regla es
  no tocarla; se podrá hacer con las tablas `_PSS` de ACPI (030), que traen los valores de la BIOS.
- **Seguridad:** 79 °C → estado más lento hasta bajar de 72 °C (el límite se baja con
  `frecuencia --limite N`, de 40 a 79; nunca más de 79); temperatura leída cada 250 ms con las
  fuentes del monitor; sin lectura, no más que el estado de la BIOS (`--forzar` para saltárselo,
  sin guardarse); escrituras protegidas (`msr_escrever_seguro`: un #GP desactiva el driver);
  desactivable con `mkimagem.py --sin-frecuencia`, `desactivar=si` en `/home/.frecuencia` o
  `frecuencia --desactivar` (vuelve al estado de la BIOS).
- **Interfaz:** comando `frecuencia` (estado, `rendimiento|ahorro|demanda`, `--limite`,
  `--desactivar/--activar`, `--forzar`, `--guardar`, `-c`); `sensores` e `infopc` (y el informe)
  muestran el control y «LIMITADA POR TEMPERATURA»; en la pestaña Rendimiento, el gráfico de
  frecuencia pasa a rojo con una banda «limitada por temperatura», una línea con el estado del
  driver y un botón «Política: ...» que pasa a la siguiente. Sin avisos emergentes. Línea en el
  arranque: «Frecuencia de la CPU: ... límite 79 °C».
- **ABI:** `SIS_FREQUENCIA` (101), `INFO_FREQUENCIA` (56); byte 133 de la cabecera (`CAB_FREQ`).
- **Pruebas:** `tests/h78_frecuencia.py` con el driver simulado (`mkimagem.py
  --frecuencia-simulada`: P0 2100, P1 1600, P2 1050 MHz, BIOS en P2; temperatura y uso
  inyectados) y «no disponible» en QEMU; pasa con y sin KVM. Capturas `docs/capturas/H78-*.png`.
- **En el dv7:** ver `testes_hw/hp-dv7-frecuencia-pruebas.txt`.

### Pendiente

- Probar en el HP dv7 (P0 2100 MHz, SaguiMark antes/después, carga larga vigilando 79 °C), en el
  G470 (Sandy Bridge) y en el Ryzen B450 (17h).
- SMP (017): el mismo estado en todos los núcleos.
- Core 2 con `_PSS` de ACPI (030).

## Ampliación v6.1: turbo opcional (decidido el 2026-11-08)

En el Toshiba L850 (i7-3630QM, base 2,4 GHz, turbo hasta 3,4 GHz) el driver se queda, como
manda D106, en el estado normal más alto (x24). Se añade una opción **«turbo»**,
**desactivada de serie**:
- **Intel:**
  - pedir el multiplicador máximo de turbo (`MSR_TURBO_RATIO_LIMIT` 0x1AD, 1 núcleo activo,
    porque SaguiOS aún no usa SMP);
  - comprobar que el turbo no está desactivado en `IA32_MISC_ENABLE` (bit 38) ni por la BIOS;
  - la CPU decide sola cuánto sube según su temperatura y su consumo (TDP).
- **AMD:** «Core Performance Boost» (bit 25 de `HWCR` 0xC0010015) donde exista, como en el Ryzen.
  El dv7 no lo tiene.
- **Activarlo:** `frecuencia --turbo si|no` (con `--guardar`), y una casilla en la pestaña de
  frecuencia de Configuración y en el Administrador de tareas.
- **Ver la frecuencia real:** `sensores` y la gráfica de Rendimiento la muestran (APERF/MPERF),
  que es lo único que dice si de verdad sube.
- El **límite de 79 °C** sigue mandando: si se alcanza, se apaga el turbo y se baja al mínimo,
  como ahora.
- Pruebas: en QEMU, la lógica con la simulación (`--frecuencia-simulada` con un estado turbo);
  en el Toshiba, SaguiMark con y sin turbo y vigilando la temperatura.

