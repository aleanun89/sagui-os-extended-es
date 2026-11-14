# 039 - Zona horaria, hora UTC en el kernel y «Fecha y hora»

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

El reloj de la placa (RTC) se usaba tal cual: SaguiOS no sabía si guardaba UTC (máquinas
virtuales, Linux) o la hora local (PC con Windows), no tenía zonas ni horario de verano, el
TLS comparaba los certificados con una hora que podía estar desplazada una o dos horas y
`ls` no enseñaba fechas.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, **D080** y **D081**.

- Kernel: hora del sistema en **UTC** (`kernel/nucleo/hora.c`), RTC en UTC o local (por
  defecto local en un PC real y UTC en una máquina virtual), escritura del RTC,
  `SIS_HORA` 91, `SIS_ZONA` 92, `SIS_DATAS_ARQUIVO` 93; `SIS_DATA` da la hora local.
- Intérprete **POSIX TZ** propio (`kernel/lib/fuso.c`), compilado en el kernel y en la
  libsagui; tabla de 84 zonas con nombre en español (`userland/libsagui/zonas.c`).
- Variable **`TZ`** por proceso; `hora_local()`, `agora_local()`, `local_para_utc()` y
  `formatar_data()` en la libsagui.
- `/etc/zona` y, con `/home` persistente, **`/home/.zona`**, leídos al arrancar.
- Programas: `zona` (consola y `--ventana` «Fecha y hora»), `date` (alias de `data`) con
  `-u`, `ls -l`, `ntp` (SNTP).
- Barra: hora local (cambia sola al horario de verano), formato de 12 o 24 h, botón derecho en
  el reloj → «Ajustar fecha y hora...»; «Fecha y hora» en Inicio → Sistema.
- ext2 guarda UTC y FAT32 hora local; Archivos enseña la fecha en Propiedades.
- TLS con el UTC del kernel (D081).

## Criterios de aceptación

- [x] Intérprete comparado con `zoneinfo` y la glibc (h67, en el anfitrión), con la hora
      repetida y la inexistente
- [x] RTC en UTC y en local; `date`, `date -u`, `TZ=... date`; barra en invierno y verano y
      el cambio de horario sin reiniciar
- [x] Fechas de archivos en RAM, FAT32 (hora local) y ext2 (UTC)
- [x] Persistencia en `/home/.zona` tras reiniciar; aviso sin disco persistente
- [x] Ventana «Fecha y hora» (búsqueda, aplicar, RTC, formato, ajuste manual)
- [x] `ntp` contra un servidor SNTP del anfitrión, con y sin `--rtc`
- [x] `h59_tls` pasa con el RTC en hora local
- [x] Test automático en `tests/` que lo comprueba (`h67_zona_horaria.py`)
- [x] Decisiones registradas en `docs/decisoes.md` (D080, D081)

## Ideas siguientes

- Probar en el G470 el valor por defecto «local» y la escritura del RTC (BCD, 12 h).
- `ntp` automático al arrancar (si hay red) y cada cierto tiempo.
- Fechas en la lista de Archivos (columna «Modificado»).
- Las fechas de `ls -l` en un sistema de archivos con zona distinta de la del sistema (FAT32
  escrito por otro PC con otra zona) no se pueden corregir: el FAT no guarda la zona.
