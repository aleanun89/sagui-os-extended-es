# 030 - ACPI básico: apagar, botón de encendido y tablas

- **Estado:** parte básica hecha (D113, rama `acpi-apagar`): apagado S5 y botón de encendido; faltan RESET_REG, MADT y probar en hardware real
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

SaguiOS no tiene nada de ACPI: no sabe **apagar** la máquina, solo reiniciar
(`SIS_REINICIAR` → `maquina_reiniciar`). En un portátil hay que mantener pulsado el botón de
encendido, y pulsarlo sin más no hace nada; si hay un disco ext2/FAT montado, apagar así
puede perder lo pendiente.

## Propuesta (sin intérprete AML)

- `kernel/dispositivos/acpi.c`:
  - buscar el RSDP en la EBDA y en 0xE0000-0xFFFFF; recorrer la RSDT (la XSDT solo si hace
    falta) y comprobar las sumas de verificación;
  - **FADT**: puertos PM1a/PM1b_CNT y PM1_EVT, SCI_INT, SMI_CMD/ACPI_ENABLE y registro de
    reinicio (RESET_REG);
  - **\_S5**: buscar el objeto `_S5_` en la DSDT byte a byte y leer SLP_TYPa/SLP_TYPb del
    paquete (no hace falta interpretar AML; es lo habitual en SO pequeños);
  - activar el modo ACPI (escribir ACPI_ENABLE en SMI_CMD y esperar SCI_EN);
  - **apagar**: desmontar todo (`montagem_desmontar_tudo`, como al reiniciar) y escribir
    SLP_TYP | SLP_EN en PM1a/PM1b_CNT;
  - **botón de encendido** (*fixed power button*, PWRBTN_STS/EN en PM1_EVT) por la IRQ del
    SCI: apagado limpio, o aviso en el escritorio con "¿Apagar?";
  - **reinicio** por RESET_REG de la FADT, y si no hay, los métodos actuales.
- **MADT**: número de procesadores/núcleos y APIC, para `infopc` y como preparación de SMP
  ([017](017-smp-multinucleo.md)). También anotar si hay HPET y MCFG.
- Syscall `SIS_APAGAR` (número libre en ese momento), comando `apagar`, y entradas "Apagar" y
  "Reiniciar" en el escritorio / menú contextual ([023](023-menu-contextual.md)).
- `infopc`: sección ACPI (tablas presentes, OEM, núcleos).

## Criterios de aceptación

- [x] `apagar-pc` (y Inicio > «Apagar...») cierra QEMU tras desmontar los discos (h84)
- [x] `system_powerdown` por QMP (botón de encendido) produce un apagado limpio (h84)
- [ ] Reinicio por RESET_REG
- [x] Sin ACPI (o con tablas rotas, o `--sem-acpi`) el sistema sigue arrancando y lo dice en el log
- [ ] `infopc` muestra los núcleos de la MADT
- [x] Test automático en `tests/` (h84_acpi)
- [x] Decisión registrada en `docs/decisoes.md` (D113)
- [ ] Probado en el G470 y en el HP dv7

## Notas

- Esfuerzo bajo-medio: ~500-700 líneas. Todo se puede probar en QEMU.
- **Fuera de alcance** (necesitan un intérprete AML, miles de líneas): batería, tapa,
  brillo, temperatura, suspensión (S3) y eventos de la EC. Si algún día se quiere, sería una
  mejora aparte.
