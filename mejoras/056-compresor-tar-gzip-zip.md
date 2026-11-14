# 056 - Compresor y descompresor multiformato: tar, gzip (.tar.gz) y zip

- **Estado:** hecha (v5, D103): biblioteca, comandos con interoperabilidad probada y menú contextual de Archivos. Pendiente: visor del contenido de un archivo comprimido con doble clic y barra de progreso
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 024/D054 (ya hay un *inflate* propio para PNG en `userland/libsagui/imagem.c`),
  D097 (comandos del terminal), 023 (menú contextual de Archivos), 042 (barra de progreso)

## Problema / motivación

No hay forma de empaquetar ni de abrir archivos comprimidos dentro de SaguiOS: lo que se
descarga con `wget` (código fuente, juegos CHIP-8, imágenes, documentos) suele venir en
`.tar.gz` o `.zip`, y para mover carpetas entre SaguiOS y Windows (partición FAT32 de
`/home`) un `.zip` es lo más cómodo.

## Propuesta

**Biblioteca común en la libsagui** (`compressao.c` + cabecera), escrita aquí a partir de las
especificaciones:

- **DEFLATE** (RFC 1951):
  - **descompresión:** se reutiliza y se saca a la biblioteca común el *inflate* que ya existe
    para PNG, con una tabla de búsqueda para acelerarlo;
  - **compresión propia:** LZ77 con tabla hash y cadenas, bloques con Huffman fijo y dinámico,
    y niveles de 1 a 9 (velocidad o tamaño).
- **CRC-32** (gzip/zip) y **Adler-32** (zlib, para PNG).
- **gzip** (RFC 1952): cabecera, nombre original, CRC, tamaño y varios miembros seguidos.
- **tar** (formato POSIX **ustar**, más las extensiones habituales de GNU para nombres largos
  al leer): archivos, carpetas, permisos y fechas (D080).
- **zip** (especificación APPNOTE de PKWARE):
  - lectura y escritura con los métodos *stored* (0) y *deflate* (8);
  - directorio central, CRC y fechas DOS;
  - nombres en UTF-8 (bit 11), con conversión a Latin-1 para mostrarlos en SaguiOS;
  - **sin** cifrado ni ZIP64 en la primera versión, documentado; un archivo que los necesite
    da un error claro.

**Comandos** (con `--help` en español y la convención de errores de la D097):
- `tar` con `-c`, `-x`, `-t`, `-v`, `-f`, `-z` (gzip), `-C` (directorio) y listas de archivos;
- `gzip` y `gunzip` (y `zcat`), con `-k`, `-d`, `-1`..`-9` y `-c`;
- `zip -r archivo.zip carpeta...` (`-q`, `-0`..`-9`, `-u` si es barato);
- `unzip archivo.zip` con `-l`, `-d carpeta`, `-o` y `-t` (comprobar).
- Detección por contenido, no por extensión.
- Protección contra rutas peligrosas al extraer (`../`, rutas absolutas), extrayendo siempre
  dentro de la carpeta destino.

**Integración gráfica:**
- En **Archivos** (menú contextual): «Comprimir en .zip», «Comprimir en .tar.gz»,
  «Extraer aquí» y «Extraer en carpeta...».
- Doble clic en un `.zip`, `.tar`, `.tar.gz`/`.tgz` o `.gz` abre un **visor del contenido**
  sencillo (lista con tamaños y botón «Extraer»), o el propio Archivos navegando dentro.
- Barra de progreso (042) si ya existe; si no, un indicador sencillo.

**Rendimiento:** la compresión con nivel 6 debe ser razonable en un Pentium III, y la
descompresión rápida (la tabla de búsqueda del *inflate*). Se mide en MB/s.

## Pruebas

- Archivos de prueba **generados** (textos, binarios aleatorios, carpetas anidadas, archivo
  vacío, nombres largos y con acentos).
- **Interoperabilidad con las herramientas del anfitrión** (`tar`, `gzip`, `zip`, `unzip`, y
  `zipfile`/`tarfile`/`gzip` de Python):
  - lo que crea SaguiOS se abre en el anfitrión con el mismo contenido (sumas SHA-256);
  - lo que crea el anfitrión se extrae en SaguiOS igual.
- Archivos corruptos o truncados y rutas maliciosas (`../../etc`): error claro, nada fuera del
  destino.
- Menú contextual de Archivos con capturas.
- La batería completa debe pasar.

## Criterios de aceptación

- [x] DEFLATE (comprimir y descomprimir), CRC-32, gzip, tar y zip en la libsagui
- [x] `tar`, `gzip`/`gunzip`/`zcat`, `zip` y `unzip` con las opciones habituales
- [x] Interoperabilidad comprobada en los dos sentidos con las herramientas del anfitrión
- [x] Extracción segura (sin salir del destino)
- [x] Integración en Archivos (comprimir y extraer): menú contextual, `tests/h76b_archivos_comprimir.py`
- [x] Test automático en `tests/`
- [x] Decisión registrada en `docs/decisoes.md`

## Notas

- Unas 2.500-3.500 líneas. Modelo sugerido: **Sonnet medio** (bien especificado y con prueba
  objetiva contra el anfitrión).
- Conviene lanzarlo cuando termine el hilo de comandos del terminal (comparte el Makefile, la
  ayuda del shell y las utilidades comunes de la libsagui que ese hilo está creando).
