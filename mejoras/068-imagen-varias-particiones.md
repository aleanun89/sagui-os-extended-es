# 068 - Imagen de disco con varias particiones (/home, /tmp, /logs) y particiones extendidas

- **Estado:** hecha (**v5.4**, rama `imagen-particiones`; D120)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** D040 (discos y montajes), D113 (apagado ACPI), 064 (SQLite), docs/imagenes.md

## Idea

El mantenedor pidió un generador de imágenes de disco para ejecutar en el PC y poder crear el disco
de `/home` con tamaño personalizado, y también uno para `/tmp` y `/logs`, con `make`, para grabarlo
con Rufus: una imagen con varias particiones.

## Qué se hizo

- `tools/mkimagem.py`: `--home-mb/--tmp-mb/--logs-mb` (y `--*-fs`, `--part-fs`) crean hasta tres particiones
  de datos (`SAGUI-DATOS`, `SAGUI-TMP`, `SAGUI-LOGS`), FAT32 o ext2, alineadas a 1 MB, en imagen dispersa
  (formateo directo en el archivo con desplazamiento). `--dados-mb/--dados-fs` siguen funcionando.
  `--extendida` y `--extra-mb` crean lógicas dentro de una extendida (`SAGUI-X1`...).
- `make imagem-particoes` (`HOME_MB`, `TMP_MB`, `LOGS_MB`, `PART_FS`, `PART_EXT`), `imagem-particoes-xz` y el
  asistente `tools/crear-disco.py`; imprimen la tabla de particiones y cómo grabar con Rufus o dd.
- Kernel: `/etc/fstab` monta por etiqueta `SAGUI-TMP` en `/tmp` y `SAGUI-LOGS` en `/logs`; solo en el
  disco que tiene la partición de SaguiOS; una FAT32 `SAGUI-*` ya no convierte el disco de arranque en un
  pendrive de datos.
- Registro del kernel en `/logs` (`kernel/fs/logs_disco.c`), con rotación de 10; `infopc --informe` copia el
  informe en `/logs`.
- Particiones extendidas y lógicas (`kernel/fs/discos.c`): cadena de EBR (0x05, 0x0F, 0x85), hasta 16 lógicas
  desde la 5, con defensas contra bucles, enlaces fuera del disco y tamaños absurdos.
- Pruebas: `tests/h91_particiones.py` y `tests/h93_extendidas.py`.
- Documentación: `docs/imagenes.md`.

## Pendiente

- GPT con más de cuatro particiones (el kernel solo lee las cuatro primeras).
- Un selector gráfico de tamaños (hoy: `make` o el asistente de texto).
