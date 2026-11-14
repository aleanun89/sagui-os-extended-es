# 013 - Sistemas de archivos permanentes: montajes, ext2/ext4, persistencia y disco ATA

- **Estado:** hecha (pasos 1 a 4; `mkfs` dentro de SaguiOS queda como idea)
- **Prioridad:** alta
- **Alta:** 2026-11-07

## Problema / motivación

Todo lo que se guardaba en SaguiOS se perdía al apagar, salvo en un único pendrive FAT32 en
`/usb`. No se podían leer discos de Linux (ext2/ext3/ext4) ni el disco interno del portátil.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, D040.

- Capa de montaje genérica (`kernel/fs/montagem.c`): varios montajes en cualquier carpeta,
  interfaz de sistema de archivos (FAT32 migrado), particiones `usb0p1`/`hda1`, `/etc/fstab`
  (arranque y conexión en caliente), programas `mount`/`umount`, `discos` con particiones.
- `kernel/fs/ext2.c`: ext2 lectura y escritura; ext3/ext4 solo lectura (extents, 64bit,
  flex_bg, htree como lista).
- Persistencia: `make imagem-persistente` añade una partición ext2 que se monta en `/home`.
- `kernel/dispositivos/ata.c`: discos IDE por PIO (LBA28/48), solo lectura por defecto.

## Criterios de aceptación

- [x] Varios volúmenes montados a la vez en carpetas cualesquiera; `montar`/`desmontar` y el
      icono del pendrive siguen funcionando (H30-H34)
- [x] ext2 (bloques de 1 y 4 KB, inodos de 128 y 256) leído y escrito; `e2fsck -fn` limpio (h50)
- [x] ext4 con extents fragmentados leído; montar `rw` lo deja en solo lectura; el disco no
      cambia (h51)
- [x] `/home` persistente tras reiniciar, en USB e IDE (h52)
- [x] Disco ATA detectado, solo lectura por defecto, `mount -o rw` explícito para escribir (h53)
- [x] fstab con pendrive conectado en caliente (h54)
- [x] Decisión D040 y errores 23-25
- [ ] `mkfs.ext2` dentro de SaguiOS (opcional, no hecho)

## Ideas siguientes

- **AHCI**: driver SATA nativo; muchos portátiles vienen con AHCI y sin opción IDE.
- **Diario de ext3/ext4**: reproducir un diario pendiente (recovery) y, después, escribir con
  diario; hoy esos volúmenes son solo lectura.
- **Escritura en ext4**: extents, sumas de verificación (crc32c, metadata_csum), flex_bg.
- **Instalador**: copiar SaguiOS al disco interno con una partición de datos (exige escritura
  segura en ATA/AHCI y confirmación explícita).
- `mkfs` (ext2 y FAT32) dentro de SaguiOS.
- Particiones lógicas (extendidas) del MBR y GPT con más de cuatro particiones.
- Montar dentro de otro volumen; UUID= en el fstab; fstab persistente en `/home`.
- htree al escribir (directorios enormes) y nombres de más de 63 caracteres.
