# 050 - Monitor de hardware: temperatura, frecuencias y más en el Administrador de tareas e infopc

- **Estado:** hecha en QEMU (D092, D094); falta probarla en PC reales. Placa (Super I/O, SMBus): pendiente
- **Prioridad:** baja-media
- **Alta:** 2026-11-07
- **Relacionadas:** 016 (Administrador de tareas), 014 (infopc), 030 (ACPI), D073

## Problema / motivación

No se ve ninguna temperatura. En PC antiguos (pasta térmica seca, ventiladores sucios) es un
dato útil, y en los portátiles de prueba ayuda a saber si una carga (Sagui 3D, compilar, el
vídeo de la 048) los calienta demasiado. No hace falta ACPI: la temperatura del procesador se
lee de registros de la propia CPU o del chipset.

## Propuesta

Módulo `kernel/dispositivos/temperatura.c`, con detección por CPUID (fabricante, familia y
modelo):

| CPU | Fuente | Notas |
| --- | --- | --- |
| Intel Core/Core 2/Core i (Amilo, G470) | MSR `IA32_THERM_STATUS` (0x19C): lectura digital por núcleo; `MSR_TEMPERATURE_TARGET` (0x1A2) para el TjMax desde Nehalem; en Core Duo/Core 2, TjMax por modelo (tabla) | CPUID.06H:EAX bit 0 = sensor digital; lectura en cada núcleo (con un solo núcleo activo, el BSP; con SMP, 017, todos) |
| AMD K8 / familia 10h-16h (dv7: familia 11h) | PCI 00:18.3, registro 0xA4 (*Reported Temperature Control*); en K8, 0xE4 | por paquete |
| AMD familia 17h/19h (Ryzen) | SMN 0x00059800 (Tctl) por el índice/dato del PCI 00:00.0 (0x60/0x64); restar el desplazamiento de los modelos que lo tienen (Tctl → Tdie) | por paquete (y por CCD si es fácil) |
| Otros / máquinas virtuales | — | «no disponible», sin errores |

- **API:** `INFO_TEMPERATURA` (número libre en ese momento): temperatura actual por sensor,
  máxima del modelo (TjMax o límite) y si está marcando *throttling* (Intel: bit PROCHOT del
  MSR).
- **Administrador de tareas**, pestaña Rendimiento:
  - temperatura por núcleo y paquete;
  - gráfica de 60 s;
  - color amarillo cerca del máximo y rojo con *throttling*.
  Solo se consulta con la pestaña visible.
- **`infopc`** (pestaña CPU) e **informe** (036): la temperatura y la fuente.
- **Comando `temp`** (`temp -c` para ver en continuo).
- Opcional: aviso en la barra si se supera un umbral.

## Ampliación: frecuencias y otros datos (petición del 2026-11-07)

Además de la temperatura, en `infopc` (pestaña CPU) y en Rendimiento del Administrador:

| Dato | Fuente | Notas |
| --- | --- | --- |
| **Frecuencia real por núcleo** | MSR `IA32_APERF`/`IA32_MPERF` (Intel y AMD desde K10): frecuencia efectiva = TSC × ΔAPERF/ΔMPERF en un intervalo | en vivo, con gráfica; refleja turbo y ahorro |
| Frecuencia nominal y del TSC | medida con el PIT y RDTSC (ya existe, `INFO_CPU_MHZ`) y CPUID 0x16 si está | — |
| Multiplicador y P-state actual | Intel: MSR `IA32_PERF_STATUS` (0x198); AMD: MSR `0xC0010063`/`0xC0010064+` (P-states) | — |
| Turbo/Boost activo y máximos | Intel: MSR `0x1AD`/CPUID; AMD: CPB en `0xC0010015` | — |
| Voltaje del núcleo (VID) | Intel (algunas generaciones) en `IA32_PERF_STATUS`; AMD en el P-state actual | aproximado: es el voltaje pedido, no el medido |
| Cachés L1/L2/L3 (tamaño, vías, línea) | CPUID 4 (Intel) / 0x8000001D o 0x80000005-6 (AMD) | — |
| Microcódigo | MSR `0x8B` (Intel) / `0x8B` (AMD) | — |
| *Throttling* térmico y por energía | Intel: bits del `IA32_THERM_STATUS`/`PACKAGE_THERM_STATUS` | — |
| Memoria: módulos, tipo, tiempos (SPD) | EEPROM SPD por SMBus (Intel ICH/PCH, AMD FCH/PIIX4) | opcional; SMBIOS ya da tipo y velocidad |
| Ventiladores y otras temperaturas de la placa | chip Super I/O (ITE IT87xx, Nuvoton NCT67xx, Winbond) por puertos LPC | **opcional y con cuidado**: cada chip es distinto (D073: lm-sensors como referencia), y una escritura mal hecha puede parar un ventilador; solo lectura |

- El kernel expone todo en una consulta (`INFO_SENSORES`) que se lee una vez por segundo y
  solo cuando alguien mira.
- Comando `sensores` (`sensores -c` en continuo), con las mismas líneas que el informe.

## Pruebas

- QEMU no emula sensores: la prueba comprueba el «no disponible» y la lógica de conversión
  con valores simulados (pruebas en el anfitrión de las fórmulas, por familia).
- **Real:** G470 o Amilo (Intel), HP dv7 (AMD 11h) y Ryzen B450 (17h): comparar con lo que
  muestra la BIOS o Windows.

## Criterios de aceptación

- [x] Temperatura en Intel (MSR) y AMD (10h-16h y 17h) con la tabla de modelos
- [x] Pestaña Rendimiento con gráfica; `infopc`, informe y `temp`
- [x] «No disponible» en máquinas virtuales, sin errores
- [ ] Probado al menos en un Intel y un AMD reales
- [x] Test automático en `tests/` (`h71_sensores.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D092, D094)

## Lo hecho (2026-11-07)

Requisito principal del usuario: el monitor es **siempre seguro** (D092).

- **Solo lectura.** Ningún `wrmsr` ni escritura en PCI, SMBus o Super I/O. Única excepción: el
  índice SMN de los Ryzen (00:00.0, registro 0x60), que se guarda y se restaura siempre, con
  las interrupciones desactivadas.
- **Lectura segura de MSR** (`kernel/arch/msr.c`): CPUID antes de cada lectura (fabricante,
  familia, modelo, bit de capacidad) y, además, un manejador del #GP que, si el `rdmsr`
  protegido falla, devuelve «no disponible» y marca ese MSR para no reintentarlo. Probado con
  `sensores --prova-msr` (MSR 0x0BADF00D y x2APIC 0x802): con KVM, dos #GP absorbidos y el
  sistema sigue; sin KVM el QEMU devuelve 0 sin excepción.
- **Fuentes** (`kernel/dispositivos/sensores.c`, fórmulas en `kernel/lib/sensores_conv.c`):
  - Intel: `IA32_THERM_STATUS` (0x19C) y `IA32_PACKAGE_THERM_STATUS` (0x1B1); TjMax del MSR 0x1A2
    desde Nehalem o por tabla (Core Duo, Core 2 con los MSR 0x17 y 0xEE, Atom antiguos);
    throttling térmico, PROCHOT y límite de potencia;
  - AMD K8: PCI 00:18.3, 0xE4; 10h-16h: 0xA4 (Tctl), con el límite HTC de F3x64;
  - AMD 17h/19h: SMN 0x59800 con el desvío Tctl → Tdie por modelo; 15h modelos 60h-7Fh: SMN
    0xD8200CA4.
- **Frecuencias y más:** real por APERF/MPERF (diferencial entre lecturas o 10 ms medidos con el
  TSC), nominal (TSC), base y máxima (CPUID 16H), multiplicador (Intel 0x198, 0xCE, 0x1AD),
  P-states de AMD (0xC0010061-0xC001006B; 10h, 11h, 15h, 16h, 17h, 19h) y K8 (0xC0010042), VID
  aproximado, turbo/CPB (0x1A0, 0xC0010015) y microcódigo (0x8B, sin escribir: puede salir «no
  disponible» en Intel). Las cachés ya salían por CPUID en `infopc` (hoja 4 o 0x80000005-6).
- **`INFO_SENSORES` (54)** con caché de 1 s; **`INFO_MSR_PROVA` (55)** para la prueba.
- **Comandos** `sensores` (`-c [-n N]`) y `temp` (`-c [-n N]`); la pestaña CPU de `infopc` y el
  informe muestran las mismas líneas.
- **Administrador de tareas**, pestaña Rendimiento: cuatro gráficas de 60 s (CPU, memoria,
  temperatura y frecuencia); temperatura en verde, amarilla a menos de 10 °C del máximo y roja
  con throttling; multiplicador, turbo y VID debajo. Los sensores solo se consultan con la
  pestaña visible.
- **Pruebas:** `tests/h71_sensores.py` (fórmulas en el anfitrión con 39 casos simulados por
  familia, «no disponible» en QEMU, `sensores`, `temp`, `-c`, la prueba del MSR, infopc, el
  informe y la pestaña Rendimiento con captura).

### Pendiente

- Probar en el G470 (Sandy Bridge), el Amilo (Core 2), el HP dv7 (11h) y el Ryzen B450 (17h) y
  comparar con la BIOS o Windows.
- Por núcleo y por CCD cuando haya SMP (017).
- **Placa** (prioridad baja): SPD por SMBus y ventiladores/temperaturas del Super I/O. Solo
  tras una opción explícita (`sensores --placa`), con aviso, en solo lectura y con tiempos
  límite (D092, punto 5).
- Aviso en la barra de tareas al pasar de un umbral (opcional).
