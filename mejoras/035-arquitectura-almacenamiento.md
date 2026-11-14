# 035 - Arquitectura de almacenamiento y memoria: raíz en disco, caché, carga bajo demanda y bibliotecas compartidas

- **Estado:** propuesta (documento de diseño). No empezar hasta que terminen los hilos en curso
  que tocan el kernel; cada fase va **sola**, sin otros hilos de kernel en paralelo, y con la
  batería completa entre fases.
- **Prioridad:** alta
- **Alta:** 2026-11-07
- **Relacionadas:** [012](012-memoria-alta.md) (kmap y caché de bloques),
  [013](013-sistemas-de-archivos.md) (montaje, ext2, ATA), [017](017-smp-multinucleo.md) (SMP),
  [029](029-usb-teclado-raton.md) (USB), [030](030-acpi-basico.md) (ACPI: apagado limpio)

---

## 1. Problema

Hoy el cargador lee **toda la imagen** al arrancar: el kernel (~215 KB) y un ramdisk SGFS
(~2 MB) con todos los programas y datos. El ramdisk se queda en RAM para siempre, con una capa
de copia en escritura encima (`vfs.c`), y cada `executar` copia el ELF entero en el espacio
del proceso. Funciona, pero no escala:

| Síntoma | Dato actual (2026-11-07) |
| --- | --- |
| El arranque se alarga con cada mejora (lectura por INT 13h, lenta en USB) | 2,2 MB y subiendo: Game Boy, Tk, etc. lo llevarán a 4-6 MB |
| Todo ocupa RAM aunque no se use | imágenes de ejemplo, juegos, raíces SSL, ayuda, fuentes de Python… |
| Código duplicado por el enlace estático | `curl` 82 KB, `wget` 78 KB y `certificados` 78 KB llevan cada uno la libtls; `sh` y `sagsh` son el mismo binario dos veces (82 KB ×2); 56 programas suman 1,56 MB |
| Ejecutar copia el programa entero | `python` (164 KB) se copia completo en cada ejecución; dos procesos iguales no comparten nada |
| Sin caché de disco | ext2 y FAT32 van al disco en cada acceso (solo FAT32 tiene una caché de 16 sectores) |
| La meta de 6 MB (D050) es artificial | condiciona lo que se puede añadir |

## 2. Objetivos

1. **Arranque de tiempo constante:** no depende del tamaño del sistema.
2. **La RAM solo guarda lo que se usa**, y lo que se puede releer del disco se descarta cuando
   falta memoria. Es "carga y descarga a RAM" sin swap.
3. **Código compartido:** una copia en memoria por biblioteca y por programa, la usen cuantos
   procesos la usen.
4. **Caché de disco** que aproveche la RAM libre, incluida la memoria alta (~1,1 GB más en el
   G470).
5. **Robustez primero:** ningún cambio puede dejar el sistema sin arrancar ni corromper datos
   del usuario. Ver la sección 5.
6. Sin código de terceros. Compatibilidad con los programas existentes, recompilándolos, y con
   los dos portátiles (G470, HP dv7) y QEMU/VirtualBox.

**No son objetivos:** swap a disco, journaling completo, multiusuario y permisos, ni módulos
del kernel cargables.

## 3. Arquitectura propuesta

```
 Arranque                          En marcha
 ────────                          ─────────
 MBR + etapa 2                     programas ──► páginas bajo demanda ─┐
   │ lee SOLO kernel + initrd       bibliotecas compartidas (/lib) ────┤
   ▼                                                                   ▼
 kernel ──► initrd (RAM, ~400 KB) ──► monta la raíz ext2 ──► caché de páginas/bloques
            (sh, mount, rescate)       del medio de arranque        (RAM libre + memoria alta)
                                        │                            │
                                        ▼                            ▼
                                     /  (ext2: /bin /lib /usr /etc /home /var)   ATA / USB
                                     /tmp (RAM)   /usb, /mnt (otros discos)
```

### 3.1 Disposición del medio de arranque

```
 LBA 0      MBR (etapa 1) + tabla de particiones
 LBA 1      cabecero SaguiOS (CAB_*): versión, sumas, opciones, resolución (D058)
            + NUEVO: UUID del sistema (16 bytes) y LBA/tamaño del initrd
 part. 0xDA etapa 2 + kernel + initrd           (solo lectura en uso normal)
 part. 0x83 raíz ext2 "SAGUI-RAIZ" con el mismo UUID en el superbloque (s_uuid)
 resto      libre (el usuario puede crear una FAT32 de datos, como en H34)
```

- `tools/mkimagem.py` genera las dos particiones. La raíz ext2 se crea con nuestro propio
  código (ya escribimos ext2, D040), no con `mke2fs`, para que el build sea reproducible.
- **Identificación del disco de arranque:** el kernel recorre los discos (ATA, USB) y monta
  como `/` la partición ext2 cuyo **UUID coincide con el del cabecero** con el que arrancó.
  No usa nombres ni orden de detección (el pendrive puede ser `usb0` o `usb1`, el disco
  `ata0`...).

### 3.2 initrd (lo mínimo para llegar a la raíz)

- Es un SGFS como el ramdisk actual, pero pequeño (~300-500 KB): `sh`, `mount`, `ls`, `cat`,
  `discos`, `fsck` (nuevo), `infopc` y los mensajes de rescate.
- Los drivers ATA, EHCI y almacenamiento USB y el código de ext2 ya están en el kernel; el
  initrd no los necesita.
- **Arranque normal:** kernel → montar raíz (UUID) → `pivot`: el initrd pasa a `/initrd`, o se
  libera → `/sbin/inicio` de la raíz.
- **Modo rescate:** ver la sección 5.2.

### 3.3 Caché unificada de páginas y bloques (base de todo lo demás)

- Unidad: la página de 4 KB, con índice `(dispositivo, bloque)` para metadatos y
  `(inodo, desplazamiento)` para datos de archivo, en una tabla hash. Reemplazo LRU de dos
  listas (activa e inactiva), para que leer un archivo grande una vez no expulse lo útil.
- **Lectura anticipada** secuencial: ventana adaptativa de 16 a 128 KB.
- **Escritura diferida** con límites: un archivo modificado se marca *sucio* y se escribe a
  los 5 s, o antes si los sucios superan el 10 % de la caché; `sync` y el desmontaje lo fuerzan
  todo. Las escrituras de metadatos van **ordenadas** (ver 5.3).
- Usa la memoria alta mediante `kmap` (parte pendiente de 012): las páginas de la caché pueden
  estar por encima de 0x78000000 y el kernel las mapea al vuelo en una ventana.
- Tamaño: toda la RAM libre, menos una reserva mínima para el kernel (p. ej. 8 MB, o el 5 %).
  Se reduce sola cuando los procesos piden memoria.

### 3.4 Carga bajo demanda de programas

- `executar` ya no copia el ELF: crea **regiones** (VMA) en el espacio del proceso que
  apuntan al archivo (`.text`/`.rodata` de solo lectura y compartidas; `.data` privada con
  copia en escritura; `.bss` y la pila anónimas a cero).
- **Fallo de página** en una región de archivo: busca la página en la caché (3.3), la lee si
  no está y la mapea. El código de solo lectura se mapea **la misma página física** en todos
  los procesos que ejecutan ese programa.
- **Expulsión:** con poca memoria, las páginas limpias respaldadas por archivo se quitan de los
  procesos (se borra la PTE) y vuelven a la caché inactiva. Se recargan solas si se vuelven a
  tocar. Las páginas anónimas no se expulsan (no hay swap): si no queda nada que liberar, falla
  el `malloc`/`sbrk` del proceso que pide memoria, **no el kernel**.
- El archivo de un programa en uso no se puede sobrescribir en el sitio (`ERRO_OCUPADO`,
  "texto ocupado"); sí se puede borrar o renombrar. Instalar un programa nuevo = escribir
  aparte y renombrar.

### 3.5 Bibliotecas compartidas (versión sencilla)

- `libsagui`, `libtls`, `imagem` (y más adelante MicroPython como biblioteca) en
  `/lib/*.so`, cada una **enlazada en una dirección fija** de una zona reservada (p. ej.
  0xD0000000-0xDFFFFFFF, una ranura de 16 MB por biblioteca, tabla en `tools/bibliotecas.txt`).
  Sin reubicación en tiempo de carga ni PIC: el ELF del programa declara qué bibliotecas
  necesita y el kernel las mapea como en 3.4 (código compartido, datos privados).
- Las llamadas entre programa y biblioteca resuelven por una **tabla de saltos** al principio
  de cada biblioteca, con índices estables. Una versión nueva puede añadir entradas al final,
  pero nunca reordenar las existentes.
- Versión ABI en la cabecera de la biblioteca: si el programa pide una mayor que la instalada,
  `executar` falla con un mensaje claro ("libtls 2 necesaria, instalada 1").
- El enlace estático **sigue siendo posible** y se mantiene para el initrd y el modo rescate,
  que no deben depender de `/lib`.
- El compilador `cc` interno genera por ahora programas estáticos; el soporte de `.so` en `cc`
  queda para más adelante.

### 3.6 Organización de carpetas

| Ruta | Contenido | Dónde vive |
| --- | --- | --- |
| `/bin` | programas | raíz ext2 |
| `/lib` | bibliotecas compartidas, `/lib/python` | raíz ext2 |
| `/usr/share/{ayuda,imagenes,jogos,ejemplos,ssl}` | datos de solo lectura | raíz ext2 |
| `/etc` | configuración (`fstab`, teclado, resolución, red) | raíz ext2 |
| `/home` | archivos del usuario | raíz ext2 (o un disco aparte por `fstab`) |
| `/var/log` | registro del sistema (copia de la serie, rotado) | raíz ext2 |
| `/tmp` | temporales | RAM (se borra al reiniciar) |
| `/dev` | dispositivos (`urandom`…) | virtual |
| `/initrd` | el initrd tras el arranque, para rescate | RAM |
| `/usb`, `/mnt/*` | otros discos | montajes |

Se mantienen enlaces o rutas de compatibilidad para lo antiguo (`/jogos`, `/imagenes`,
`/ejemplos`, `/ayuda`) durante una versión, para no romper scripts ni pruebas de golpe.

### 3.7 Instalación en disco duro (`instalar`)

- `instalar ata0` copia la etapa 1, el cabecero y las dos particiones a un disco ATA, con
  confirmación escrita ("escribe SI para borrar ata0") y comprobando que el disco no es el de
  arranque. Hace falta que la BIOS ponga el SATA en modo IDE, o un driver AHCI (mejora futura).
- Por defecto el sistema sigue sin escribir en discos ATA al arrancar (regla de D040).

## 4. Fases

| Fase | Contenido | Esfuerzo | Riesgo | Se puede probar en QEMU |
| --- | --- | --- | --- | --- |
| **F0** | **Partición del sistema** (ver abajo): lo grande y poco usado sale del ramdisk a una partición `SAGUI-SIS` en la compilación con particiones | bajo-medio | bajo | sí |
| **F1** | Caché de páginas y bloques (3.3) + `kmap` (012) + `sync`, sin cambiar aún el arranque | medio | bajo | sí |
| **F2** | Raíz en ext2 + initrd + UUID + reorganización de carpetas + modo rescate (3.1, 3.2, 3.6, 5.2) | medio-alto | medio | sí (USB y ATA) |
| **F3** | Carga bajo demanda y código compartido (3.4) | medio-alto | medio | sí |
| **F4** | Bibliotecas compartidas (3.5) | medio | medio | sí |
| **F5** | `instalar` en disco duro (3.7) | bajo-medio | medio (escribe discos) | sí; en el portátil con SATA en IDE |

- **F1 + F2** quitan el límite de crecimiento; **F3 + F4** dan el ahorro de memoria y la
  velocidad de arranque de los programas.
- Cada fase se fusiona sola, con la batería completa y probada en el G470 antes de la
  siguiente, como en las fases originales del PRD.
- Tras F2, la meta de tamaño de D050 se sustituye por dos: **initrd ≤ 512 KB** y **arranque
  hasta el escritorio ≤ N s en el G470** (medir antes de F2 para fijar N).

### F0: partición del sistema (paso intermedio, propuesto el 2026-11-08)

Antes de la raíz en disco completa, un paso pequeño que ya quita RAM: **no todo el sistema
tiene que vivir en el ramdisk**. En la imagen con particiones (068) hay una opción
(`make imagem-particoes SISTEMA_EN_DISCO=1 SIS_MB=...`) que:
- deja en el **ramdisk solo el núcleo** del sistema: shell, escritorio, Archivos, Terminal,
  Editor, utilidades básicas, `/etc` y la ayuda mínima. Es lo necesario para arrancar y
  reparar;
- pone en una partición **`SAGUI-SIS`** (ext2, montada **solo lectura** en `/sistema`) lo grande
  y poco usado:
  - `python` y `python-x87` con `/lib/python`;
  - `sqlite3`, `litemonkey`, `sagui3d` y sus fases, `sgldemo`;
  - las imágenes de ejemplo, `/ejemplos` y la ayuda extensa;
  - más adelante, el vídeo y los modelos de la IA.
- El cargador de programas ya lee ELF de cualquier sistema de archivos montado, así que basta con:
  - añadir `/sistema/bin` al PATH y `/sistema/lib/python` a la búsqueda de Python;
  - poner las entradas del menú y del escritorio con la ruta de `/sistema`.
- **Si falta la partición** (imagen de una sola pieza, CD, otro PC): esas aplicaciones no
  aparecen y el sistema lo dice, sin fallar. La imagen normal sigue siendo completa en RAM.
- **Ganancia:** el ramdisk baja a unos 2-3 MB y la RAM libre sube en lo mismo. Los programas
  grandes tardan algo más en abrirse la primera vez, por la lectura del disco; la caché de F1
  lo compensa después.
- **Robustez:** `SAGUI-SIS` solo lectura y con suma de comprobación por archivo (`MANIFIESTO`
  con SHA-256) comprobada en segundo plano. El ramdisk siempre puede arrancar solo.

## 5. Robustez

Este es el apartado que manda: ante la duda, se elige la opción que no puede dejar el sistema
sin arrancar ni perder datos.

### 5.1 Principios

1. **El arranque nunca depende de algo escribible.** La etapa 2, el kernel y el initrd van en
   una partición que en uso normal solo se lee, y llevan sumas de verificación (ya existen
   para kernel y ramdisk; se extienden al initrd).
2. **Siempre hay un camino de rescate** que no necesita la raíz ext2 (5.2).
3. **Fallar de forma cerrada y visible:** ante una inconsistencia, montar en solo lectura y
   avisar, nunca "arreglar en silencio" escribiendo.
4. **Nada escribe en un disco que no se ha identificado sin ambigüedad** (UUID), y nada
   escribe en discos ATA sin que el usuario lo pida (D040).
5. **Errores de E/S propagados:** un fallo de lectura de disco llega al programa como error
   (`ERRO_ES`), o como terminación del proceso si es en un fallo de página. Nunca como pánico
   del kernel ni como datos a cero en silencio.

### 5.2 Arranque y rescate

| Situación | Comportamiento |
| --- | --- |
| No se encuentra la partición raíz con el UUID (pendrive en otro puerto, controlador no soportado, AHCI…) | espera hasta 10 s a que aparezcan discos USB (enumeración lenta), reintenta y, si no, **modo rescate**: shell del initrd con un aviso claro y `discos`/`mount` para montar a mano |
| La raíz no se desmontó limpiamente (`s_state` de ext2 ≠ limpio) | `fsck` automático en modo de **solo comprobación**; si encuentra errores, monta en **solo lectura** y avisa en el escritorio ("ejecuta `fsck -r /`") |
| Superbloque dañado | prueba las copias de respaldo de ext2 (grupo 1, 3, 5…) en solo lectura; si ninguna sirve, rescate |
| Suma del initrd incorrecta | el cargador lo dice (como hoy con kernel y ramdisk) y no salta |
| `/sbin/inicio` o `/lib` dañados | el kernel cae al shell estático del initrd (rescate), no a una pantalla de error |
| Tecla al arrancar (p. ej. mantener Mayús o F8) | menú: normal / rescate / solo lectura / resolución segura 1024x768 / sin write-combining |

- El **modo rescate es el sistema actual, más pequeño**: todo en RAM, sin depender del disco.
  Se prueba en la batería en cada versión (5.6).

### 5.3 Consistencia del sistema de archivos sin journal

ext2 no tiene journal; se compensa con disciplina:

- **Escrituras ordenadas:** primero los datos, luego el inodo y por último las entradas de
  directorio y los mapas de bits. Así, un corte deja como mucho bloques "perdidos" (espacio
  ocupado sin usar), que `fsck` recupera, y nunca un directorio apuntando a basura.
- **Actualizaciones atómicas por renombrado** para la configuración (`/etc/*`, preferencias,
  certificados añadidos): escribir `x.nuevo`, `sync` del archivo y `renombrar` sobre `x`. La
  biblioteca da una función `guardar_atomico()` y la usan todos los programas del sistema.
- **`s_state` = sucio** mientras está montado en escritura; limpio al desmontar. El apagado
  limpio (ACPI, 030) y el reinicio desmontan siempre (ya lo hace `montagem_desmontar_tudo`).
- **`fsck` propio** con dos modos:
  - comprobar (solo lectura): superbloque, grupos, mapas, inodos, directorios y contadores;
  - reparar (`-r`): solo arreglos seguros, es decir, contadores, bloques e inodos huérfanos a
    `lost+found` y mapas de bits. Ante algo dudoso, se detiene y lo explica.
- Opción futura, si ext2 se queda corto: journal de metadatos compatible con ext3, que ya
  leemos. No entra en estas fases.

### 5.4 Memoria

- **Reservas:** el kernel mantiene un mínimo de páginas libres (marca baja/alta) solo para él
  (manejadores de interrupción, red, escritura de la caché), para no quedarse sin memoria al
  intentar liberar memoria.
- **Orden de recuperación:** primero caché inactiva limpia, luego páginas de archivo de
  procesos, y luego se fuerza la escritura de sucias y se reintenta. Si aun así no hay, falla
  la petición del proceso (no el kernel).
- **Sin sobrecompromiso para memoria anónima:** `sbrk` reserva de verdad (o falla en el
  acto), así que un proceso no muere más tarde por un fallo de página sin memoria.
- **Carreras con archivos:** una página de archivo bajo E/S está bloqueada; dos fallos de
  página sobre la misma esperan al mismo bloque, sin leerlo dos veces ni mapear medio leído.
  Esto prepara SMP (017).
- `mem` e `infopc` muestran la caché (total, sucia, aciertos/fallos) y las páginas
  compartidas.

### 5.5 Compatibilidad y migración

- Los ELF estáticos actuales siguen funcionando (3.5). Los generados por `cc` antes de esta
  mejora también.
- **Pendrives grabados con versiones antiguas:** arrancan como siempre (ramdisk completo). El
  cargador nuevo reconoce el formato antiguo del cabecero y no busca raíz.
- **`/home` del usuario en un disco aparte** (fstab, D040): se respeta; la raíz nueva no lo
  pisa.
- **Actualizar un pendrive** de versión antigua a nueva: regrabar la imagen. Copiar antes
  `/home` si estaba en el mismo pendrive; la documentación lo dirá en `docs/como-gravar.md`.

### 5.6 Pruebas de robustez (obligatorias para cerrar cada fase)

| Prueba | Fase | Cómo |
| --- | --- | --- |
| Corte de energía durante escrituras | F1-F2 | en QEMU: escribir muchos archivos, matar QEMU en un instante aleatorio (varias semillas), `fsck` del host (`e2fsck -n`) y el nuestro: sin corrupción de directorios y como mucho bloques perdidos |
| `guardar_atomico` | F1 | cortar entre escribir y renombrar: el archivo viejo o el nuevo, completos; nunca medio |
| Errores de E/S inyectados | F1-F3 | un disco de prueba que devuelve error en ciertos sectores (QEMU `blkdebug`): el programa recibe error, el kernel sigue vivo |
| Raíz no encontrada / UUID distinto / superbloque roto | F2 | el sistema entra en rescate o monta en solo lectura, con el mensaje esperado |
| Arranque desde USB en otro puerto o con dos pendrives SaguiOS distintos | F2 | monta la raíz del UUID correcto, no la del otro |
| Memoria baja | F3 | QEMU con 64 MB: abrir muchos programas grandes; el sistema expulsa páginas limpias y sigue; un proceso que pide de más recibe error; el kernel no entra en pánico |
| Código compartido | F3 | dos `python` a la vez comparten las páginas de código (`mem` lo muestra) y cada uno funciona |
| Biblioteca de versión incompatible | F4 | mensaje claro, sin fallo raro |
| Batería completa | todas | las ~65 pruebas actuales siguen pasando en modo normal **y** las de consola en modo rescate |

## 6. Impacto en el hardware real

- **G470:** arranque desde USB por EHCI (ya funciona, fase 6). La raíz ext2 en el mismo
  pendrive se lee con el mismo driver. **Riesgo:** la enumeración USB puede tardar; de ahí la
  espera con reintento de 5.2. El disco interno solo con SATA en IDE.
- **HP dv7:** igual por EHCI. Si el pendrive está en un puerto que la BIOS maneja como OHCI
  (USB 1.1), el kernel no lo ve hasta tener la mejora 029: en ese caso, rescate con un
  mensaje que lo explique.
- **VirtualBox/QEMU:** sin cambios relevantes. Se añaden pruebas con disco IDE y con USB.

## 7. Criterios de aceptación globales

- [ ] Arranque: el cargador lee solo kernel + initrd (≤ 512 KB); tiempo hasta el escritorio
      medido en el G470 y sin crecer al añadir programas
- [ ] La RAM ocupada en reposo no crece con el tamaño de `/usr/share`
- [ ] Dos instancias de un programa comparten su código; las bibliotecas se cargan una vez
- [ ] Caché de disco con lectura anticipada y escritura diferida; `sync` y desmontaje seguros
- [ ] Modo rescate, solo lectura automática ante una raíz sucia o dañada, y `fsck` propio
- [ ] Todas las pruebas de robustez de 5.6 en verde
- [ ] Decisiones registradas en `docs/decisoes.md` (una por fase)
- [ ] `docs/como-gravar.md` actualizado (particiones, rescate, actualizar desde una versión
      antigua)

## 8. Riesgos principales y mitigación

| Riesgo | Mitigación |
| --- | --- |
| No encontrar la raíz en hardware real (USB lento, controlador no soportado) | UUID + reintentos + rescate siempre disponible; probar en el G470 al cerrar F2 |
| Corrupción de ext2 por cortes de corriente | escrituras ordenadas, renombrado atómico, `fsck`, solo lectura ante la duda y pruebas de corte |
| Errores sutiles en fallos de página y caché (datos de otro archivo, páginas compartidas modificadas) | F3 separada de F1, pruebas de estrés y comprobaciones internas (`ASSERT`) activadas en la batería |
| Romper programas existentes | estáticos siguen funcionando, rutas de compatibilidad, batería completa en cada fase |
| Tiempo de desarrollo | cada fase aporta valor sola; se puede parar tras F2 |

## Notas

- F1 cierra la parte pendiente de la mejora 012.
- Esta mejora y SMP (017) se tocan: la caché y los fallos de página deben diseñarse con
  bloqueos desde el principio, aunque hoy haya un solo núcleo.
