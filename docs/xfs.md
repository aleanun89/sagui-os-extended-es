# XFS en SaguiOS: plan (solo lectura en la v8.5.5.3, escritura a futuro)

Pedido del mantenedor (2026-11-20): leer discos XFS en la próxima revisión, en **solo lectura**, y dejar
planteados los ajustes necesarios para que más adelante se pueda **escribir**.

Decisión **D446** (`docs/decisoes.md`). **Fase 1 hecha** el 2026-11-21 en la rama `v8.5.5.3-xfs`; la fase 2 sigue
pendiente.

## Por qué

XFS es el sistema de archivos por defecto de Fedora Server, RHEL, Rocky y Alma, y es habitual en discos de datos
y NAS. Leerlo permite abrir y copiar desde discos de Linux, igual que ya se hace con ext2/3/4.

## Licencia

El XFS de Linux (`fs/xfs`) y `xfsprogs` son **GPL-2.0-only** (en parte, LGPL-2.1), y eso no se puede mezclar con
la GPL v3 del proyecto. Se escribe **desde cero**, como se hizo con ext4 y exFAT, a partir de la especificación
pública **«XFS Algorithms & Data Structures»** (kernel.org, `Documentation/filesystems/xfs/`, documentación
publicada aparte).

- No se copia código del kernel ni de `xfsprogs`.
- Las constantes y los formatos del disco no son código y se pueden usar.

## Fase 1 (v8.5.5.3): solo lectura — HECHA (D446)

Fichero `kernel/fs/xfs.c` (más `kernel/inc/xfs.h`), registrado en el VFS como `ext2.c` y `exfat.c` (`fs_xfs`, tipo
`xfs`, `mount -t xfs`).

**Estado al terminar la fase 1** (lo que sigue de esta sección era el plan; se cumplió entero salvo lo anotado):
- todo lo de «Qué se lee», «Funciones del disco», «Diario sucio» e «Integración» está hecho y probado (`tests/h446_xfs.py`);
- los enlaces simbólicos con destino relativo se siguen al resolver rutas; los de destino absoluto, rotos o en bucle se
  leen como un fichero con el destino de texto (el VFS no tiene enlaces);
- la barra lateral de Archivos pone un candado a toda unidad de solo lectura;
- `infopc` y `discos` enseñan el formato «XFS (solo lectura)», o el motivo si no se puede leer;
- sin hacer (para la fase 2): la tabla CRC32C «slice-by-8» o con la instrucción `crc32` (punto 5); la lectura sigue byte
  a byte, que basta para leer;
- probado solo en QEMU y con imágenes de mkfs.xfs 6.6: falta un disco XFS de verdad escrito por Linux (Fedora, RHEL) en
  una de las máquinas del mantenedor.

### Qué se lee
- **Superbloque** del grupo de asignación 0: magia `XFSB`, versión 4 o 5, tamaño de bloque (512 B-64 KB),
  tamaño de inodo, `agblocks`/`agcount`, inodo raíz y bits de funciones.
- **Sumas CRC32C** (v5): se comprueban en el superbloque, los inodos, los bloques de directorio y los árboles B+.
  Si una suma falla, ese objeto da error de E/S y no se sigue leyendo basura.
- **Inodos** (`xfs_dinode`): formatos *local* (datos dentro del inodo), *extents* (lista de extents) y *btree*
  (árbol B+ de extents, `bmbt`).
  - Los números de inodo son de 64 bits: grupo de asignación, bloque e índice.
  - Las fechas pueden ser «bigtime» (posteriores a 2038).
- **Directorios** en sus cinco formas:
  - *shortform*, dentro del inodo;
  - *block*, un solo bloque;
  - *leaf*;
  - *node*;
  - *btree*, para directorios enormes.

  La búsqueda por nombre usa el hash de XFS (`xfs_da_hashname`). El listado recorre los bloques de datos.
- **Ficheros normales**: se traducen los desplazamientos con los extents. Los huecos (*sparse*) y los extents
  «sin escribir» (*unwritten*) se leen como ceros.
- **Enlaces simbólicos**, locales o en bloques remotos (con su cabecera v5).
- **Atributos extendidos**: no hacen falta para leer. Solo se ignoran sin fallar.

### Funciones del disco (`sb_features_incompat` y `ro_compat`)
- **Aceptadas en lectura**: `ftype`, `spinodes`, `meta_uuid`, `bigtime`, `nrext64` (contadores de extents de
  64 bits) y `inobtcount`. `reflink`, `rmapbt` y `finobt` son de escritura y no estorban para leer.
- **Rechazadas, con un mensaje claro**: cualquier bit incompatible desconocido, como `needsrepair`, `exchrange`,
  `parent` y `metadir` (XFS 6.x). El disco no se monta, en vez de leerse mal.

### Diario sucio (apagado incorrecto)
Si el diario no está limpio, Linux se niega a montar en solo lectura sin `norecovery`. En la fase 1:

- Se detecta (cabecera del diario y `sb_` de estado) y se monta **igualmente en solo lectura** con un aviso:
  «XFS: el diario no está vacío (apagado incorrecto): lo último escrito puede no verse; repáralo en Linux con
  xfs_repair o móntalo allí una vez».
- No se reproduce el diario. Eso queda para la fase 2.

### Integración
- Detección automática por la magia `XFSB` en `discos.c`/`montagem.c`, también en particiones GPT (tipo Linux)
  y MBR (0x83).
- `montar` muestra «xfs (solo lectura)». Archivos y la barra lateral lo enseñan como unidad con el candado de
  solo lectura.
- Cualquier escritura devuelve `EROFS`.
- `infopc` y `df` muestran el tipo.

### Pruebas
- Las imágenes de prueba se crean en el PC con `mkfs.xfs -p` (fichero «proto»: sin montar ni root). `xfsprogs` lo trae
  `tools/entorno-local.sh` sin sudo (el paquete de Ubuntu extraído en `~/.local/saguitools/root`).
- `tests/xfs_lib.py` hace lo que mkfs no hace solo: fragmentar un fichero en cientos de extents (árbol bmbt), con huecos y
  extents sin escribir, sin tocar el espacio libre (con los bloques que ya eran del fichero); ensuciar el diario; y poner
  la cabecera v5 de los enlaces en bloques, que el mkfs.xfs 6.6 no escribe (xfs_repair -n lo marca). Que `xfs_repair -n`
  quede limpio y que `xfs_db` lea los mismos mapas comprueba esas estructuras sin el driver de SaguiOS.
- `tools/xfs_anfitriao.c` corre el mismo `kernel/fs/xfs.c` en el PC (32 bits) sobre una imagen: la prueba lo usa antes
  de arrancar QEMU, y sirve para depurar.
- Variantes:
  - v5 con CRC;
  - v4 antigua;
  - bloque de 4 KB y de 1 KB;
  - un directorio con 10 000 ficheros (formas *node* y *btree*);
  - un fichero fragmentado en más de 100 extents (*btree*);
  - huecos, enlaces simbólicos largos y nombres UTF-8;
  - diario sucio;
  - un bit incompatible desconocido, que debe rechazarse.
- La prueba compara sumas de ficheros leídos en QEMU con las del PC y comprueba que escribir da `EROFS`.

### Esfuerzo
Medio: unas 2.000-3.000 líneas, en la línea de exFAT más el diario de ext4. Los árboles B+ y las cinco formas
de directorio son lo más largo.

## Fase 2 (a futuro): escritura. Ajustes que hay que preparar ya

Para que la fase 1 no haya que rehacerla, se escribe pensando en esto:

1. **Capa de metadatos con búferes**: leer los bloques de metadatos (inodos, directorios, árboles) a través de
   una caché de búferes con marca de «sucio» y referencia, aunque en la fase 1 nunca se ensucien.
   - La escritura necesita esa capa para ordenar y registrar los cambios en el diario.
   - Aprovechar `cache_disco.c`, añadiendo el *pin* de búferes mientras están en una transacción.
2. **Diario lógico de XFS**: XFS no registra bloques enteros como jbd2, sino **elementos lógicos**: cambios de
   inodo, de búfer por regiones, de extents libres e intentos (EFI/EFD).
   - Fase 2a: **reproducir** el diario al montar, para leer bien los discos con apagado incorrecto.
   - Fase 2b: escribir transacciones propias. Se puede empezar con una transacción por operación y sin CIL,
     sin el *delayed logging* de Linux; es más lento, pero correcto.
3. **Asignación de espacio**: los árboles de espacio libre por grupo (`bnobt` por bloque, `cntbt` por tamaño),
   la lista libre del grupo (AGFL) y los árboles de inodos (`inobt`/`finobt`). Las rutinas de árbol B+ de la
   fase 1 deben ser **genéricas** (un solo recorrido con callbacks por tipo de árbol), para añadir después la
   inserción y la división de nodos sin duplicar código.
4. **Funciones que bloquean la escritura**: con `reflink`, `rmapbt` o atributos de *parent pointers*, escribir
   exige mantener árboles extra. Plan: la fase 2 escribe solo si el disco no tiene `reflink`/`rmapbt`, y si no,
   lo monta en solo lectura con un aviso. Más adelante se pueden ir sumando.
5. **CRC32C al escribir**: la función de la fase 1 se usa también para calcular sumas. Que sea rápida (tabla
   «slice-by-8», o la instrucción `crc32` de SSE4.2 si la CPU la tiene).
6. **Desplazamientos de 64 bits**: el VFS y la caché de bloques ya manejan discos grandes por ext4. Hay que
   comprobar que los números de bloque de XFS (`xfs_fsblock_t`, de 64 bits) no se truncan en ningún punto del
   camino, sobre todo en el kernel de 32 bits sin PAE.
7. **Fechas, propietarios y permisos**: traducir uid/gid, modos y fechas «bigtime» en las dos direcciones, igual
   que se hace con ext4 (D-permisos).
8. **Herramienta de comprobación**: antes de dar la escritura por buena, comprobar cada imagen escrita por
   SaguiOS con `xfs_repair -n` en el PC, dentro de la prueba automática.

Esfuerzo de la fase 2: alto. La reproducción del diario (2a) es lo más útil y lo primero que conviene hacer.
