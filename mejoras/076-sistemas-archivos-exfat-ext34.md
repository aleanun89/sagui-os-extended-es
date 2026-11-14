# 076 - Sistemas de archivos: exFAT y escritura en ext3/ext4

- **Estado:** propuesta (**v7**, decidido por el mantenedor el 2026-11-08: «el sistema de archivos nos enfocaremos en la v7»)
- **Prioridad:** media-alta
- **Alta:** 2026-11-08
- **Relacionadas:** D040 (discos y reglas de seguridad), D026 (FAT32), `kernel/fs/ext2.c`, 035 (raíz en disco, caché F1), 068 (particiones), 064/067 (SQLite y lite-monkey: sincronizar)

## Punto de partida

- **ext2:** lectura y escritura.
- **ext3/ext4:** **solo lectura**, incluidos los *extents* del ext4. Con diario,
  extents, 64 bits o sumas de verificación, el volumen se monta en solo lectura con un aviso.
- **exFAT:** se detecta y sale como «formato no soportado».

## 1. exFAT (lectura y escritura)

- Especificación publicada por Microsoft (2019): implementación propia, sin problemas de
  licencia.
- Útil para **pendrives de más de 32 GB y tarjetas SD de 64 GB o más**, que vienen así de
  fábrica.
- **Piezas:**
  - sector de arranque y su suma de verificación;
  - FAT y **mapa de bits** de clústeres libres; archivos contiguos sin cadena (`NoFatChain`);
  - **grupos de entradas** de directorio (archivo + flujo + nombres en UTF-16) con su suma;
  - tabla de mayúsculas (*up-case*) para comparar nombres;
  - fechas con zona horaria y etiqueta del volumen.
- Escritura con las mismas reglas que FAT32: caché de metadatos y grabación al final de cada
  operación; los discos internos solo con `rw` explícito (D040).
- Tamaño orientativo: unas 2.000-2.500 líneas.

## 2. ext3 con diario (escritura)

- **Al montar:** si el volumen quedó sin cerrar (`needs_recovery`), **reproducir el diario**
  (JBD2) antes de nada.
- **Escritura en modo «ordered»**, como Linux:
  - primero los datos;
  - luego los metadatos al diario, con un bloque de *commit*;
  - después los metadatos a su sitio;
  - y avanzar la cola del diario.
- Transacciones agrupadas por tiempo o tamaño, y `sincronizar` (D116) fuerza el *commit*.
- **Resultado:** un sistema de archivos que **no se corrompe si se va la luz**, ideal para
  `/home`, para la partición del sistema (035) y para bases de datos (SQLite, lite-monkey).
- Tamaño orientativo: unas 1.500-2.500 líneas.

## 3. Escritura en ext4

- Sobre lo anterior:
  - **árbol de extents** (crear, ampliar, partir y liberar);
  - tamaños y bloques de **64 bits**;
  - **flex_bg**;
  - **sumas CRC32c** en todos los metadatos (`metadata_csum`, de serie en `mkfs.ext4`): superbloque,
    descriptores de grupo, mapas de bits, inodos, extents, directorios y diario.
- Lo que no se entienda (cifrado, *inline data*, *casefold*…) mantiene el volumen en solo
  lectura, como hoy.
- Tamaño orientativo: unas 4.000-6.000 líneas.

## Orden propuesto dentro de la v7

1. exFAT (lo que más se nota con menos esfuerzo).
2. ext3 con diario.
3. Escritura en ext4.

Si el tiempo aprieta, la 3 puede pasar a una versión posterior.

## Pruebas

- **En el anfitrión:** volúmenes creados con `mkfs.exfat`, `mkfs.ext3` y `mkfs.ext4` (con y sin
  `metadata_csum`). SaguiOS escribe (archivos grandes y pequeños, carpetas, renombrar, borrar,
  llenar el disco) y el anfitrión comprueba con **`fsck.exfat` y `e2fsck -fn`** y compara el
  contenido.
- **Cortes de luz simulados en ext3/ext4:** se apaga QEMU a mitad de una escritura en distintos
  momentos, al volver a montar se reproduce el diario, y `e2fsck` debe quedar limpio, sin
  perder lo confirmado.
- **Real:** un pendrive de 64 GB en exFAT y una tarjeta SD.

## Criterios de aceptación

- [ ] exFAT de lectura y escritura, con `fsck.exfat` limpio
- [ ] ext3: reproducción del diario y escritura «ordered»; `e2fsck` limpio tras cortes simulados
- [ ] ext4: escritura con extents y CRC32c; `e2fsck` limpio
- [ ] Reglas de D040 intactas (discos internos)
- [ ] Tests automáticos en `tests/`
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

Modelo sugerido: **Opus** (sistemas de archivos y consistencia ante cortes: escribe discos del
usuario).
