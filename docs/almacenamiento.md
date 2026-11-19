# Almacenamiento y arranque: la raíz en disco, el modo live y el modo «todo a RAM» (D379)

- **Estado: DECIDIDO (2026-11-17)**: el mantenedor acepta todas las recomendaciones (P1-P7 = A, sección 6).
  **Versión: v8.5.3.1** (rama `v8.5.3.1-almacenamiento`): imprescindibles R1-R3 (instalado) y L1-L2 (live desde
  el pendrive con capa escribible); T1 (toram del CD) si llega a tiempo. **M1 también en la v8.5.3.1** (el mantenedor:
  no se publica hasta que las imágenes por defecto lo usen; §7); F queda para la v8.5.4.2. Es la continuación de la [mejora 035](../mejoras/035-arquitectura-almacenamiento.md),
  que se actualiza para apuntar aquí.
- **Pedido del mantenedor (2026-11-17):** «No limites el ramdisk: abordar su funcionamiento como un
  Linux/Unix. Instalado en disco: el sistema de archivos en el disco (a RAM solo lo fundamental).»
  **Corrección del mismo día:** el live normal (USB, imagen de disco) **también funciona desde el medio**,
  no copiando todo a la RAM: raíz del medio (de solo lectura si conviene) con una capa escribible (en RAM
  o persistente en el propio medio), leyendo bajo demanda con la caché. **«Todo a RAM» (toram) es solo un
  modo específico**: el CD/ISO o una live que lo pida de forma explícita, nunca la regla general ni una
  decisión automática por la cantidad de RAM.

---

## 1. Estado real medido (v8.5.3, `main` b68aa924, 2026-11-17)

### 1.1 Qué hay en el ramdisk

Compilación `make all INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0`:

| Pieza | Tamaño |
| --- | --- |
| `build/ramdisk.sgfs` (perfil «base», D324) | **12.521.168 B (11,94 MiB)**: a **60 KB** del límite de 12 MiB que vigila h324 |
| Árbol del ramdisk (`build/rootfs`, 749 nodos del VFS) | 14,0 MiB en disco del anfitrión (textos en UTF-8) |
| `/bin` (181 programas, todos estáticos) | 7,3 MiB: `configuracion` 303 KB, `instalar` 196 KB, `infopc` 152 KB, `discos` 144 KB, `sagsh` **y** `sh` 140 KB cada uno (el mismo binario dos veces), `wg` 137 KB, `fuentes`, `reproducir`, `midi`, `litemonkey`, `editor`, `markdown`, `terminal`, `tareas` (115-130 KB cada uno) |
| `/testes` (programas de prueba en el ramdisk de producción) | 1,9 MiB |
| `/lib` (`/lib/python`, firmware) | 1,3 MiB |
| `/fontes` (DejaVu) | 1,3 MiB |
| `/sistema` (cargadores UEFI, MBR, catálogos de idioma) | 0,8 MiB |
| `/ejemplos` | 0,7 MiB |
| `/etc`, `/imagenes`, `/ayuda`, `/jogos` (chip8, jclic) | 0,3 + 0,2 + 0,1 + 0,1 MiB |
| `kernel.bin` | 1,39 MiB |
| `/apps` (partición SAGUI-APPS, fuera del ramdisk desde D324) | 21,7 MiB de contenido, partición de 52 MB |
| `/jogos` (SAGUI-JOGOS) | 6,9 MiB de contenido, partición de 9 MB (800 MB en las imágenes «grandes») |

La rama `v8.5.4.1-dinamico` (D378) ya pone `libsagui.so.1`, `libc.so.1` y `libsagui_medios.so.1` en
`/apps/lib` y sus pruebas en `/apps/testes` porque el ramdisk no tenía sitio.

### 1.2 Qué se copia a la RAM al arrancar y cuánto cuesta

- La **etapa 2** (BIOS, INT 13h) o el **cargador UEFI** leen **kernel + ramdisk entero** (13,9 MiB) antes de
  saltar al kernel. El ramdisk se queda en la RAM **para siempre**; el VFS (`vfs.c`) monta encima un árbol de
  nodos con copia al escribir (lo que se escribe, en el heap del kernel; se pierde al apagar salvo lo que vaya a
  una partición montada).
- **Coste medido** (QEMU, 256 MB, consola, `mem`): con el ramdisk de siempre el kernel gestiona **240 MB** y
  quedan **235 MB** libres; con el initramfs del prototipo, **251 MB** y **245 MB** libres: el ramdisk cuesta
  **≈ 11-12 MB de RAM** en cualquier máquina, se use o no lo que lleva. En una de 32 MB es más de un tercio de
  la memoria (D324: h324 exige 4 MB libres con escritorio).
- Cada `executar` lee **el ELF entero** (`vfs_ler_inteiro`) y lo copia en el espacio del proceso: no hay páginas
  compartidas (D378 fase 3 pendiente).

### 1.3 Cómo arranca hoy cada medio

| Medio | Qué lee el cargador | Raíz | Particiones montadas (fstab) |
| --- | --- | --- | --- |
| **Imagen de disco / USB** (`saguios.img`, BIOS) | MBR → cabecera LBA 1 → etapa 2 → kernel + ramdisk por INT 13h | ramdisk en RAM | SAGUI-JOGOS `/jogos`, SAGUI-APPS `/apps`, SAGUI-CONF `/sistema/conf`; `/home` solo si hay SAGUI-DATOS («arranque» en fstab), si no, `/home` en RAM |
| **Disco instalado** (`instalar`, D179/D318) | igual: el instalador copia la partición 0xDA (etapa 2, kernel y ramdisk) byte a byte | ramdisk en RAM | además SAGUI-DATOS `/home` (y TMP/LOGS si se crean) |
| **UEFI** (imagen híbrida D264, instalado con GPT D266) | `BOOTX64.EFI`/`BOOTIA32.EFI` lee `\SAGUI\KERNEL.BIN` y `\SAGUI\RAMDISK.SGF` de la ESP (o del disco, D270) | ramdisk en RAM | igual que BIOS |
| **CD/ISO** (El Torito, emulación de disco, D077) | la BIOS emula un disco con la imagen `--cd`; la etapa 2 lee kernel + **`ramdisk-cd.sgfs`** (base + `/apps`, ≈ 34 MB estimados hoy) | ramdisk en RAM | ninguna: la ISO no lleva particiones de datos |

**Corrección a un supuesto del encargo:** desde **D207** el kernel **sí lee el CD** (ATAPI por IDE y SATAPI por
AHCI, ISO 9660 con Rock Ridge/Joliet, UDF D233) y monta el CD de arranque en `/cd`; además ve la imagen El
Torito de dentro como un disco (`cd0img`), que es lo que usa el instalador. Lo que no hace es tomar del CD la
raíz ni `/apps` (por eso el ramdisk del CD lleva `/apps` dentro).

**Identificación del disco de arranque** (D179, `discos.c`): el kernel compara los 256 primeros bytes del LBA 1
de cada disco (o del primer sector de la partición 0xDA en GPT) con la cabecera con la que arrancó (lleva las
sumas del kernel y del ramdisk y el identificador de instalación). Las etiquetas `LABEL=SAGUI-*` del fstab **solo
valen en ese disco** (`irmao_de_boot`): es el equivalente al UUID de la 035, ya hecho.

### 1.4 Particiones de hoy (imagen `saguios.img`, `sfdisk -d`)

| N.º | Tipo | Tamaño | Etiqueta | Dónde |
| --- | --- | --- | --- | --- |
| 1 | 0xDA, activa | 13,3 MB | — | etapa 2, kernel, ramdisk (sin sistema de archivos) |
| 3 | 0xEF | 40 MB | SAGUI-EFI | ESP FAT32 con copia del kernel y del ramdisk |
| 5 | 0x83 lógica | 9 MB | SAGUI-JOGOS | `/jogos` rw |
| 6 | 0x83 lógica | 52 MB | SAGUI-APPS | `/apps` rw |
| 7 | 0x83 lógica | 16 MB | SAGUI-CONF | `/sistema/conf` (la superpone sobre `/etc` al arrancar, D255) |

El instalador añade SAGUI-DATOS (`/home`). La caché de bloques (D281/D350) **solo cachea discos internos**
(ATA/SATA/NVMe): pendrives y CD van sin caché (se pueden retirar sin avisar).

### 1.5 Tiempos de arranque (`[ARRANQUE]` del registro, QEMU con KVM, 512 MB)

| Medio | kernel cargado | discos | escritorio | configuración | Tiempo real hasta el escritorio |
| --- | --- | --- | --- | --- | --- |
| IDE, BIOS, ramdisk de siempre | **2.759 ms** | 3.816 | 5.466 | **5.696 ms** | 5,8 s |
| USB (EHCI), BIOS | ≈ 1.160 ms | 2.681 | 4.495 | 4.721 ms | — |
| IDE, UEFI x64 (OVMF) | 5.025 ms | 6.096 | 7.761 | 7.985 ms | 8,0 s |
| **IDE, BIOS, prototipo (initramfs 640 KB + raíz en disco)** | **469 ms** | 1.543 | 3.212 | **3.445 ms** | — |

La diferencia del prototipo está casi toda en la carga: la BIOS ya no lee 12 MB por INT 13h. El resto del arranque
(USB, red, sonido) no cambia. (Con KVM la lectura INT 13h es rápida; en el hardware real, sobre todo por USB, la
diferencia será mayor.)

---

## 2. Principios del diseño

1. **Como un Unix:** el núcleo + un **initramfs mínimo** en RAM; la raíz `/` es un sistema de archivos de verdad
   (ext2) en un medio, leído **bajo demanda**, con la caché de disco (D281/D350) haciendo el resto.
2. **Tres modos, elegidos de forma explícita, nunca por la RAM que haya:**
   - **instalado**: raíz `rw` en el disco;
   - **live** (USB, imagen de disco): raíz del **medio**, de solo lectura, con una **capa escribible** encima
     (en RAM o persistente en el propio medio);
   - **toram**: la raíz se copia entera a la RAM; solo para el **CD/ISO** y para una live que lo pida.
3. **Robustez primero** (035 §5): el arranque nunca depende de algo escribible; siempre hay rescate (el initramfs
   es un sistema mínimo autosuficiente); ante la duda, solo lectura y aviso.
4. **Sin romper lo que hay:** una imagen sin `/etc/raiz.conf` en su ramdisk arranca exactamente como hoy (el
   prototipo ya lo garantiza, h387 D); los discos instalados con versiones anteriores siguen arrancando.

---

## 3. Diseño propuesto

### 3.1 El initramfs (lo que siempre va a la RAM)

- Es un **SGFS como el ramdisk de hoy** (mismo formato, mismo cargador, mismas sumas): no hay que tocar la etapa 2
  ni el cargador UEFI. Lo hace `tools/mkinitramfs.py` desde `build/rootfs`.
- Contenido: `sagsh`/`sh`, `ls cat cp mv rm mkdir echo mount umount montar desmontar discos dmesg mem ps sync`
  (estáticos: el initramfs **nunca** depende de `/lib`, D378), carpetas vacías, `/etc/raiz.conf` (qué raíz montar)
  y `/etc/rescate.txt`. Más adelante: `fsck` propio y `instalar` (para reparar o reinstalar sin la raíz).
- **Tamaño: 640 KB** en el prototipo (la meta de la 035 era ≤ 512 KB; con `sh` como enlace al mismo binario que
  `sagsh` baja ~140 KB). **El límite de 12 MB de h324 deja de tener sentido** y se sustituye por dos: initramfs
  ≤ 1 MB (h387 A) y la raíz sin límite propio (el que dé la partición).
- Los drivers de disco (ATA, AHCI, NVMe, EHCI/xHCI/UHCI/OHCI, ATAPI) y los sistemas de archivos están **en el
  kernel**, como hoy: el initramfs no lleva módulos.
- **`/etc/raiz.conf`** (el equivalente al `root=` de la línea de órdenes de Linux):
  ```
  raiz=LABEL=SAGUI-RAIZ     # LABEL=, un nombre de «discos» o "arranque"
  modo=rw                   # instalado: rw; live: ro + capa; toram
  espera=10                 # segundos que se espera al volumen (USB lento)
  capa=ram                  # live: ram | LABEL=SAGUI-PERSIST  (fase L2)
  ```
  Se puede **forzar desde el cargador**: un byte nuevo de la cabecera (`CAB_RAIZ_OPC`, offset 162, hoy libre) con
  «auto (lo que diga raiz.conf) / disco / live / toram / rescate», que la etapa 2 cambia con una tecla al arrancar
  (como ya hace Ctrl para rescate y Mayús para la aceleración segura) y que `kconfig` (D353) puede guardar.

### 3.2 Instalado en disco físico

```
 disco:  [MBR][cab][0xDA: etapa2 + kernel + initramfs][ESP?][ext: SAGUI-RAIZ (/) | SAGUI-DATOS (/home) | SAGUI-JOGOS?]
 RAM:    kernel + initramfs (≈ 2 MB)  ->  monta SAGUI-RAIZ en "/" (rw)  ->  fstab  ->  escritorio
```

- `SAGUI-RAIZ` (ext2) lleva **todo lo que hoy es ramdisk + `/apps`**: `/bin`, `/lib` (las `.so` de D378),
  `/apps`, `/etc`, `/root`, `/sistema`, `/fontes`, `/ayuda`, `/ejemplos`, `/usr/share` a medio plazo, `/var/log`.
- `/home` en **SAGUI-DATOS** aparte (como Linux con `/home` separado): reinstalar el sistema no toca los datos.
- `/tmp`: en RAM (el árbol del initramfs montado como tmpfs, fase R3) o en SAGUI-TMP si se pide.
- **SAGUI-CONF se funde en la raíz** en el instalado: `/etc` y `/root` (D371) ya son persistentes en la raíz, la
  copia de D255 sobre `/etc` sobra (el prototipo ya la salta con la raíz en disco).
- **Rescate** si la raíz no monta (no está, no es ext2, superbloque roto, error de E/S): se sigue con el initramfs,
  consola de root, aviso en pantalla y `dmesg` con las líneas `[RAIZ]` (h387 C). Con una raíz **sucia** (no se
  desmontó bien) se monta en **solo lectura** con aviso, hasta tener `fsck` propio (035 §5.3).
- Seguridad: la raíz es de root (`/bin`, `/lib`, `/apps`, `/etc` 0755/0644, `/etc/sombra` 0600, `/sistema` 0711,
  `/tmp` 1777, `/home` de sagui: mkimagem ya los pone igual que `meta_inicial` del VFS); las reglas de D282/D371
  pasan a ser las del ext2 sin casos especiales del árbol en RAM.

### 3.3 Live (USB, imagen de disco): desde el medio, con capa escribible

```
 USB:    [MBR][cab][0xDA: kernel + initramfs][SAGUI-RAIZ (ro)][SAGUI-PERSIST? | SAGUI-DATOS?][SAGUI-CONF]
 RAM:    kernel + initramfs + caché + la capa escribible (solo lo que se cambia)
 "/" = capa(RAM o SAGUI-PERSIST) sobre SAGUI-RAIZ(ro)
```

- La raíz del medio se monta **de solo lectura**: el sistema de serie no se puede estropear (un corte con el
  pendrive no deja una raíz rota) y la imagen grabada sigue siendo la misma (sumas comprobables).
- Encima, una **capa escribible** al estilo overlayfs (sistema de archivos nuevo `sobreposicao`, un
  `sistema_arquivos_t` más): busca primero en la capa y luego en la raíz; escribir un fichero de la raíz lo
  **copia a la capa** (copy-up); borrar deja una **marca de borrado** (whiteout) en la capa; los directorios se
  mezclan al listar. La capa puede ser:
  - **en RAM** (por defecto): el árbol del VFS de hoy, generalizado como `ramfs`. Se pierde al apagar, como hoy;
  - **persistente** (`capa=LABEL=SAGUI-PERSIST`): una partición ext2 del propio pendrive, como las live «con
    persistencia» de las distribuciones.
- `/home` y SAGUI-CONF siguen como hoy en la live: si hay SAGUI-DATOS y SAGUI-CONF en el pendrive, son
  persistentes; si no, en RAM («modo en vivo» de D255).
- **Lectura bajo demanda:** hace falta que la caché de bloques **cachee también los pendrives** (hoy solo los
  discos internos): solo lectura de bloques limpios, descartada al retirar el medio (`disco_removido` ya
  descarta). Es requisito de la fase L1.
- Retirar el medio en live: como en cualquier live que no es toram, **no se puede** (el sistema lo avisa y lo
  que no está en caché da error de E/S al programa, nunca pánico, 035 §5.1).

### 3.4 Toram (CD/ISO, o una live que lo pida)

- La raíz entera se **copia a la RAM** al arrancar y el medio se puede quitar. Nunca es automático por la RAM:
  - el **CD/ISO** lo usa por defecto (el CD es lento, gira y hace ruido, y es el caso para el que se pidió);
  - una **live USB** solo si se elige (tecla en el cargador, `CAB_RAIZ_OPC`, o `modo=toram` en `raiz.conf`).
- **Mecanismo propuesto:** el kernel (no la etapa 2) lee la imagen de la raíz del medio y la vuelca en un **disco
  en RAM** (`ram0`, un `disco_t` cuyo almacén es memoria alta, D348) y monta encima el **mismo ext2** de siempre.
  Ventajas: el cargador sigue leyendo solo kernel + initramfs (arranque igual de rápido hasta el escritorio
  mínimo), el formato es uno solo (ext2) en los tres modos y el instalador copia lo mismo. La imagen puede ir
  **comprimida** (bloques comprimidos de 64 KB con el compresor de la swap D352 o el inflate propio), para que el CD
  lea menos.
- En el **CD**: como el kernel ya lee ISO 9660 (D207), la imagen de la raíz puede ser un **fichero** de la ISO
  (`/SAGUI/RAIZ.IMG`) o una partición dentro de la imagen El Torito (`cd0img`). Así la ISO deja de necesitar el
  `ramdisk-cd.sgfs` gigante (que la BIOS tiene que leer entero por la emulación de disco, lento en lectores viejos).
- **Memoria:** toram necesita la raíz entera en RAM (hoy ≈ 36 MB con `/apps`, más los juegos si se incluyen). Si
  no cabe, **no se decide solo**: se avisa («no hay RAM para toram: se arranca leyendo del CD/medio») y se sigue en
  modo live del medio (en el CD, live desde el CD por ATAPI). Es una vuelta atrás segura, no un umbral de elección.

### 3.5 BIOS y UEFI

- BIOS: la etapa 2 no cambia (lee lo que diga la cabecera; ahora un initramfs pequeño).
- UEFI: el cargador lee `\SAGUI\RAMDISK.SGF` de la ESP, que pasa a ser el initramfs: la **ESP adelgaza** de 40 MB a
  lo justo para FAT32 (≈ 33 MB por el mínimo de FAT32, o FAT16 si se quiere menos). El kernel monta la raíz igual
  (el disco de arranque se reconoce por la cabecera, también en GPT, D270).
- El ramdisk «completo» de hoy queda como **formato de compatibilidad** (imágenes antiguas, `RAMDISK=completo`) hasta
  que el toram por `ram0` lo sustituya.

### 3.6 Dónde va cada cosa

| Ruta | Instalado | Live (USB/imagen) | Toram (CD, o pedido) | initramfs / rescate |
| --- | --- | --- | --- | --- |
| `/bin`, `/sbin` | SAGUI-RAIZ (rw, de root) | SAGUI-RAIZ ro + capa | `ram0` (copia) | 18 programas estáticos |
| `/lib` (`ld-sagui.so.1`, `.so` de D378, `/lib/python`, firmware) | SAGUI-RAIZ | ídem | ídem | no hay (todo estático) |
| `/apps` | **dentro de SAGUI-RAIZ** (deja de ser partición) | ídem | ídem | — |
| `/etc` | SAGUI-RAIZ (persistente; absorbe SAGUI-CONF) | capa (RAM o PERSIST); SAGUI-CONF superpuesta como hoy | RAM; SAGUI-CONF si hay un disco con ella | `raiz.conf`, `rescate.txt` |
| `/root` (D371) | SAGUI-RAIZ, 0700 | capa / SAGUI-CONF (`raiz/`) | RAM | RAM |
| `/home` | SAGUI-DATOS | SAGUI-DATOS del pendrive o RAM | RAM | RAM |
| `/jogos` | SAGUI-JOGOS (grande, aparte) | ídem | no (o del CD) | — |
| `/tmp` | RAM (tmpfs) o SAGUI-TMP | RAM | RAM | RAM |
| `/var/log`, `/logs` | SAGUI-RAIZ o SAGUI-LOGS | capa | RAM | RAM |

### 3.7 Cómo pasa el instalador de live a instalado

1. Crea la tabla (como hoy): 0xDA con etapa 2 + kernel + **initramfs** (`modo=rw`), ESP si UEFI, SAGUI-RAIZ,
   SAGUI-DATOS y SAGUI-JOGOS opcional.
2. **Copia la raíz por ficheros** desde la raíz **de serie** (la ro del medio, o `ram0` en toram), no desde la capa:
   lo instalado es el sistema limpio. Opción «llevar mis cambios» para copiar también la capa.
3. Escribe el `/etc/fstab` del destino (DATOS en `/home`, JOGOS en `/jogos`) y pasa al `/etc` del destino lo que
   hoy guarda SAGUI-CONF (usuarios, sombra, claves) en vez de crear SAGUI-CONF.
4. Comprueba (sumas por fichero, como hoy compara byte a byte) y marca la cabecera con su identificador (D179).

### 3.8 Rendimiento, memoria, fiabilidad

- **Arranque:** −2,3 s medidos con KVM (más en hardware real); el arranque deja de crecer con el sistema.
- **RAM:** +11 MB libres en reposo; lo que se usa entra en la caché y se descarta cuando hace falta (D350 ya cede
  memoria). En máquinas de 32-64 MB el instalado y el live desde el medio son los modos naturales (toram no).
- **Coste:** cada programa se lee del disco la primera vez (la caché lo compensa; la 035 F3, carga bajo demanda y
  páginas compartidas, lo mejora de verdad).
- **Cerrojo único de los sistemas de archivos** (`trava_fs`, D095): con todo en ext2, todas las operaciones de
  ficheros pasan por él; hay que medirlo con el escritorio y pasar a un cerrojo por volumen si molesta.
- **Fiabilidad:** escritura diferida con orden y barreras (D281), desmontaje de la raíz **la última** al apagar
  (hecho en el prototipo), raíz sucia → solo lectura, `fsck` propio pendiente; en live la raíz ro no se corrompe.

---

## 4. El prototipo (hecho, rama `v8.5.3.1-almacenamiento`)

**Qué demuestra:** arranque con un initramfs de 640 KB que monta una raíz ext2 de una partición de la imagen y
sigue el arranque desde ella **hasta el escritorio**, con `/apps` y SAGUI-CONF montadas dentro, escritura
persistente, apagado limpio (e2fsck limpio) y rescate si la raíz no está. Las imágenes normales no cambian.

- `kernel/fs/raiz.c` + `kernel/inc/raiz.h`: lee `/etc/raiz.conf` del árbol en RAM; resuelve el volumen
  (`disco_resolver`, que con `LABEL=SAGUI-*` solo acepta el disco de arranque); espera hasta `espera` s; monta en
  `/` con la opción interna `MONTAR_RAIZ_INTERNO` (no llega desde los programas: `discos.c` filtra las opciones);
  si falla, `RAIZ_RESCATE` y aviso.
- `kernel/fs/montagem.c`: la raíz es el montaje de prefijo más corto (toda ruta absoluta que no cae en otro);
  montajes **dentro** de la raíz permitidos (el punto se crea en el disco si falta); `umount /` se niega;
  `montagem_desmontar_tudo` suelta la raíz la última; `MAX_MONTAGENS` 8 → 12.
- `kernel/nucleo/principal.c`: `raiz_montar()` tras detectar discos y antes del fstab (marca `[ARRANQUE] raiz`);
  con la raíz en disco no se superpone SAGUI-CONF sobre el `/etc` en RAM (no se vería).
- `tools/mkinitramfs.py` (nuevo) y `tools/mkimagem.py --raiz PASTA [--raiz-mb N]`: partición SAGUI-RAIZ (ext2,
  UUID fijo) con el árbol del ramdisk, textos a Latin-1 como `mksgfs`, permisos del VFS y los puntos de montaje.
- `tests/h387_raiz_disco.py`: A (anfitrión: tamaño, e2fsck, permisos), B (raíz montada, `/bin` y `/etc` del disco,
  `uname` solo en el disco, `/apps` dentro, `umount /` negado, persistencia tras reiniciar, e2fsck limpio), C
  (rescate), D (la imagen normal no cambia). Pasa con KVM y con TCG. Captura `H387-escritorio-raiz.png`.

**Lo que el prototipo NO resuelve (y por qué no se hizo aquí, es el cambio grande):**
1. ~28 sitios del kernel usan el árbol en RAM **directamente** (`vfs_achar`/`vfs_criar`: `conf_sistema`,
   `sessao.preparar_casa`, `wifi`, `bloqueo`, `usbbt`, `area_trabalho`, `telas`, `frequencia`, `gpu3d`...): con la
   raíz en disco escriben en el árbol oculto del initramfs. Hay que pasarlos todos a la API por rutas.
2. El árbol en RAM no es un `sistema_arquivos_t`: para montarlo como `/tmp`, `/initrd` o capa de la live hay que
   convertirlo en un driver `ramfs`.
3. En rescate se lanza el gestor de servicios, que no existe en el initramfs (avisos repetidos en la serie): el
   rescate por raíz no encontrada debe comportarse como `OPC_RESCATE`.
4. Caché solo para discos internos; overlay, `ram0`/toram, `fsck`, instalador y migración: fases de abajo.

---

## 5. Plan por fases

Cada fase va sola (hilo de kernel sin otros en paralelo), con su prueba y la batería reducida + las afectadas; el
cambio de modo por defecto de las imágenes, con la batería completa.

| Fase | Contenido | Prueba | Riesgo |
| --- | --- | --- | --- |
| **R0** (hecha) | prototipo: initramfs + raíz ext2 en `/` + rescate | h387 | bajo: solo con `raiz.conf` |
| **R1** | los ~28 accesos directos al árbol en RAM pasan a la API por rutas; rescate = `OPC_RESCATE`; `CAB_RAIZ_OPC` y tecla en la etapa 2 | h387 ampliada (escritorio, sesión de sagui, Wi-Fi/config guardada), batería reducida | medio: toca sesión y configuración |
| **R2** | **instalado**: el instalador crea SAGUI-RAIZ (absorbe `/apps` y SAGUI-CONF), copia por ficheros, fstab; imagen «instalable» | h149/h381 nuevas variantes; arranque solo del disco | medio (escribe discos; ya hay instalador probado) |
| **R3** | árbol en RAM como driver `ramfs` (`/tmp`, `/initrd` tras el arranque) | h387, h14 (VFS) | medio |
| **L1** | caché de lectura para pendrives; **live**: raíz ro del medio + capa en RAM (driver `sobreposicao`: copy-up, whiteouts) | h387 live: escribir/borrar ficheros de serie, reiniciar = limpio; e2fsck de la raíz intacto | medio-alto (VFS nuevo) |
| **L2** | capa persistente SAGUI-PERSIST | cortes de luz (035 §5.6), reinicios | medio |
| **T1** | **toram**: `ram0` + copia de la raíz (comprimida opcional); ISO con la raíz como fichero o partición; cargador con la opción | h207 con la ISO nueva, quitar el CD con el sistema en marcha | medio |
| **M1** | imágenes por defecto con initramfs; límite de h324 sustituido (initramfs ≤ 1 MB, RAM libre); migración/actualización de instalados (P6) | batería completa | medio |
| **F** | `fsck` propio, raíz sucia → solo lectura con aviso; luego 035 F3 (carga bajo demanda) | 035 §5.6 | — |

---

## 6. Preguntas para el mantenedor (contestadas el 2026-11-17: **todas A**)

**P1. Formato de la raíz instalada.**
- **A. ext2 (recomendada)**: es lo que más hemos probado, escribimos con orden y barreras (D281) y con arnés de
  cortes; sin diario, pero con raíz sucia → solo lectura y `fsck` propio.
- B. ext4 con diario (D337 activó la escritura ext3/ext4 tras el arnés de cortes): más resistente a cortes, más
  código en el camino de cada escritura; podría ser el paso siguiente sin cambiar nada más.
- C. ext2 ahora y la opción ext4 en el instalador cuando el diario lleve una versión entera sin fallos.

**P2. Particiones del instalado.**
- **A. SAGUI-RAIZ única para el sistema (incluye `/apps` y lo de SAGUI-CONF) + SAGUI-DATOS `/home` + SAGUI-JOGOS
  opcional (recomendada)**: como un Linux con `/home` aparte; reinstalar no toca los datos.
- B. Todo en una sola partición (raíz con `/home` dentro): lo más simple, pero reinstalar es más delicado.
- C. Mantener SAGUI-APPS y SAGUI-CONF separadas además de la raíz: menos cambio, más particiones y más reglas.

**P3. La capa escribible de la live (USB/imagen).**
- **A. En RAM por defecto, y persistente (SAGUI-PERSIST) si la imagen se hace así o el usuario la crea
  (recomendada)**: el pendrive de serie no se estropea; persistencia opcional como en las distribuciones.
- B. Siempre persistente en el pendrive (la live se comporta como un instalado en USB).
- C. Sin capa: la raíz del pendrive montada `rw` directamente (lo más sencillo; un corte puede dañar el sistema).

**P4. Qué medios usan toram y cómo se elige.**
- **A. El CD/ISO por defecto (con una tecla para arrancar «desde el CD» sin copiar), y en USB/imagen solo con la
  tecla del cargador o `modo=toram` (recomendada).**
- B. Solo el CD; en USB, nunca.
- C. Cualquier medio, siempre a elección en un menú del cargador en cada arranque (más lento de usar).

**P5. Formato de lo que se carga en RAM en toram.**
- **A. La misma imagen ext2 de la raíz copiada a un disco en RAM (`ram0`), comprimida por bloques en el CD
  (recomendada)**: un único formato en los tres modos.
- B. Seguir con el SGFS «completo» (el ramdisk de hoy) para el CD: cero trabajo, pero la ISO sigue siendo la
  excepción y la BIOS lee decenas de MB.
- C. Un formato comprimido propio (tipo squashfs): menos lectura del CD, mucho código nuevo.

**P6. Instalaciones ya existentes (≤ 8.5).**
- **A. Siguen arrancando como hoy (sin `raiz.conf` no cambia nada); para pasar al modo nuevo, reinstalar
  conservando `/home` y la configuración (el instalador ya guarda SAGUI-CONF) (recomendada).**
- B. Una herramienta `actualizar` que convierta en sitio SAGUI-APPS en SAGUI-RAIZ (copiar la base dentro, mover
  `/apps`, cambiar la etiqueta y el initramfs): sin reinstalar, pero escribe sobre el sistema en marcha.
- C. Las dos: A como camino oficial y B como experimental.

**P7. El CD: ¿toram o también «desde el CD»?**
- **A. Toram por defecto y live desde el CD como alternativa (ya hay ATAPI/ISO 9660, D207) (recomendada).**
- B. Solo toram (si no cabe en RAM, no arranca el CD: hay que usar la imagen de disco).

---

## 7. Estado de las fases (v8.5.3.1 y v8.5.4.2)

| Fase | Estado | Qué se hizo | Prueba |
| --- | --- | --- | --- |
| R0 | hecha | prototipo (sección 4) | h387 A-D |
| R1 | hecha | **cuelgue arreglado**: el ocioso (proceso 0) esperaba un bloqueo cediendo con las interrupciones apagadas mientras el dueño dormía esperando la IRQ de su DMA: con la raíz en disco el shell se quedaba sin prompt ~1 de cada 4 arranques (`trava.c`: el ocioso espera un tic con interrupciones); los accesos del kernel que aún iban al árbol en RAM (icono por fecha, OpenTyrian en el menú, claves Bluetooth) pasan por rutas; SAGUI-CONF fundida en la raíz (`conf_sistema_na_raiz`: `/etc` y `/root` de la raíz, sin copia); rescate por raíz no encontrada = modo rescate (root, sin escritorio ni servicios); `CAB_RAIZ_OPC` (byte 163; 162 ya era de D371) | h387, 20 arranques seguidos sin cuelgue |
| R3 | hecha | `ramfs.c`: una carpeta del árbol en RAM como volumen; con la raíz en disco, `/tmp` en la RAM y el initramfs en `/initrd` (solo lectura); `mkimagem.py --raiz` mete `/apps` dentro de SAGUI-RAIZ y no crea SAGUI-CONF (P2 A) | h387 B |
| R2 | hecha | instalador: con la raíz en disco (`/initrd/etc/raiz.conf`), SAGUI-RAIZ ocupa el sitio de SAGUI-CONF (MBR y GPT), se copia la raíz por ficheros (comparados uno a uno), la SAGUI-CONF vieja del destino se funde en `/etc` y `/root` (P6 A) y la cabecera lleva raíz rw | h387 E |
| L1 | hecha | `sobreposicao.c` (copy-up con dueño, permisos y fecha; marcas `.wh.<nombre>`; carpetas opacas `.wh..opq`; renombrar carpetas de la inferior no se admite, como EXDEV), caché de lectura para pendrives (su escritura sigue directa), modo `live` en `raiz.conf`; en «mount» sale como `tipo live`. La SAGUI-CONF de un pendrive se superpone sobre el `/etc` de la capa | h387 F (pendrive EHCI: escribir, cambiar, borrar, carpetas, renombrar; al reiniciar, todo de serie y SAGUI-RAIZ byte a byte igual) |
| L2 | hecha | capa persistente SAGUI-PERSIST (`capa=LABEL=SAGUI-PERSIST`, `mkimagem.py --persist-mb N`); si falta o no vale, la capa va en la RAM con aviso | h387 G (cambios tras reiniciar, raíz intacta, los dos ext2 limpios) |
| T1 | hecha (sin compresión) | `disco_ram.c`: la raíz copiada a `ram0` (páginas altas si las hay; si no cabe dejando 32 MB, aviso y live desde el medio) y el mismo ext2 encima, `rw`; `mkimagem.py --cd --raiz` mete SAGUI-RAIZ en la imagen El Torito (el kernel la ve en `cd0img`); 61 MB en ~4 s (QEMU) | h387 H (ISO, copia a RAM, se saca el CD y sigue) |
| M1 | **hecha** (la 8.5.3.1 no se publica sin ella, decisión del mantenedor) | todas las imágenes de `make all`/`make variantes` con initramfs + SAGUI-RAIZ: disco/USB/VDI en live (capa en RAM; las persistentes en SAGUI-PERSIST, `capa=auto`), SAGUI-CONF como siempre; la ISO en toram con Freeciv; «completo» = el ramdisk de siempre; `RAIZ_DISCO=0` = como la 8.5.3. Marca de versión y sumas del `/etc` de serie en la raíz (rescate si no cuadra); reinstalar conserva SAGUI-DATOS en su sitio y lo cambiado de `/etc` y `/root` (de SAGUI-RAIZ o SAGUI-CONF). h324: initramfs ≤ 1 MB | h387 D, I, J, K; h119, h149, h159, h207, h254, h270, h324, h381 adaptadas |
| T1+P5 | **hecha (v8.5.4.2)** | la raíz del CD comprimida por bloques de 64 KB (DEFLATE, formato SGRZ, partición 0x5A); el núcleo la ve como `rz0` (`disco_comprimido.c`, `kernel/lib/inflar.c`): toram la copia a `ram0`; sin RAM, live desde el CD descomprimiendo lo leído. ISO 94,2 → 47,1 MB | h387 H |
| P4/P7 | **hecha (v8.5.4.2)** | tecla **M** al arrancar (etapa 2 y UEFI): live / todo a la RAM / instalado / rescate, en `boot_info`; `kconfig raiz_modo` y `capa_ram_pct` | h387 M, N |
| F | **básico hecho (v8.5.4.2)** | ext2 sin el bit de limpio: fsck básico al montar con escritura (mapas y contadores rehechos de los inodos; si no se puede, solo lectura con aviso). Falta: directorios y enlaces (pasos 2-4 de e2fsck) y la carga bajo demanda (035 F3) | h387 O |

**Pendiente de la 8.5.3.1 (pequeño) o para la 8.5.3.2:**
- T1: la imagen de la raíz **comprimida por bloques** en el CD (P5); hoy va sin comprimir.
- La **tecla en el cargador** para elegir toram/live/rescate (P4): el byte `CAB_RAIZ_OPC` y `mkimagem.py --modo-raiz` ya
  existen; falta que la etapa 2 lo cambie con una tecla y que `kconfig` lo guarde.
- En la live, el instalador copia la raíz que se ve (con la capa); la opción «solo el sistema de serie» (leer la
  inferior) queda para M1.
- h324 vigila ya el initramfs (≤ 1 MB, M1); el ramdisk completo solo es el formato de compatibilidad.
- Desde la live, el instalador copia la raíz que se ve (con la capa: lo cambiado en la sesión va también); leer solo la
  inferior («el sistema de serie») necesita exponer la capa inferior de `sobreposicao.c`: sigue pendiente.
- El cerrojo único `trava_fs` (D095): con la raíz en disco todo pasa por él; medir con el escritorio.

**Hecho en la v8.5.4.2** (detalle en D379): marca de versión por línea entera; el rescate lista los discos y recomienda
`saguios-completo.img`; identificador de la instalación en la cabecera y en `/sistema/raiz/id` (con dos discos
SaguiOS cada uno usa su raíz; un instalado dudoso nunca se monta con escritura); `mkimagem.py` falla sin `debugfs`
para SAGUI-RAIZ; instalar desde la live copia el sistema de serie (`/sistema/serie`; `--con-cambios` para lo de la
sesión); tope de lo escrito en la RAM con ENOSPC; renombrar en la live como en el ext2; fsck básico.

## 8. Orden de las particiones (D388, v8.5.4.10r2)

Desde la v8.5.4.10r2 todas las imágenes de disco (y el instalador) dejan **`/home` la última**, para que al grabar
la imagen en un disco grande se pueda ampliar sin mover nada, también en la live:

| N.º (MBR) | Tipo | Etiqueta | Qué es |
| --- | --- | --- | --- |
| 1 | 0xDA, activa | — | arranque: etapa 2, núcleo e initramfs |
| 2 | 0xEF | `SAGUI-EFI` | la ESP (arranque UEFI), delante de la extendida |
| 3 | 0x0F | — | la extendida, la **última** partición, hasta el final de la imagen |
| 5 | lógica | `SAGUI-CONF` | la configuración del sistema |
| 6 | lógica | `SAGUI-RAIZ` | el sistema de serie (en la live, la base de `/`, solo lectura) |
| 7 | lógica | `SAGUI-PERSIST` | (persistentes) tus cambios a `/`: la capa de la live |
| … | lógica | `SAGUI-JOGOS`, `SAGUI-APPS`, `SAGUI-TMP`, `SAGUI-LOGS`, `SAGUI-X*` | las que lleve la imagen |
| la última | lógica | `SAGUI-DATOS` | `/home` |

Los números dependen de qué particiones lleve cada imagen: SaguiOS las monta **por etiqueta**, nunca por número.
El primer arranque en un disco con 512 MB o más libres ofrece «Ampliar /home» (`ampliar-home`): crecen solo la
extendida y `/home` (y su ext2), comprobado antes y después. En el terminal: `ampliar-home` (dice cuánto) y
`particiones ampliar /home`.

**Imágenes grabadas antes de la v8.5.4.10r2** (`/home` la primera lógica, la ESP en la entrada 3): siguen
arrancando igual. Ampliar `/home` obligaría a mover lo de detrás; si ahí están la raíz y su capa en uso (la live
desde la v8.5.3.1, o un sistema instalado), SaguiOS **no lo intenta** y explica que hay que grabar la imagen nueva
o instalar SaguiOS en el disco con `instalar` desde un pendrive. Ojo: si el instalador conserva una SAGUI-DATOS que
está la primera, la conserva en su sitio (con la disposición de antes); para la nueva, copia antes lo de `/home` a
otro disco e instala con `--formatear-home`. Con una
imagen antigua sin raíz en disco, se mueve como antes (copiado y releído antes de cambiar la tabla).

En la live, `/` son dos particiones: `discos` las marca como «sistema de serie (base de /, solo lectura)» y «tus
cambios (capa de /)», y `df` y `mount` dan una sola línea `/` de tipo `live` con el espacio de la capa.

## 9. Un ext2 que no se desmontó bien: «reparar» (D397, v8.5.4.10r2)

ext2 no tiene diario. Al montar un ext2 que no se desmontó bien, el kernel rehace los mapas de bits y los contadores
(`fsck_basico`, D379 F); si encuentra algo más grave (un inodo que apunta fuera del volumen, un bloque que usan dos
sitios), lo monta **de solo lectura** y dice qué ejecutar:

    hda9: no se desmontó bien y no se puede comprobar (un inodo apunta fuera del volumen): solo lectura; ejecuta «reparar hda9»

`reparar` (como root) lo arregla desde el propio SaguiOS: `reparar -n hda9` solo lo dice, `reparar hda9` pregunta,
`reparar -y hda9` no pregunta. Si la partición está montada de solo lectura, la desmonta, la repara, la comprueba otra
vez y la vuelve a montar donde estaba, con escritura. Antes de escribir guarda los bloques que cambian
(`/tmp/reparar/…copia`, o `--copia` en otro disco) y `reparar --deshacer COPIA hda9` lo deja como estaba. Ayuda:
`/ayuda/reparar.md`.

| Volumen | Cómo repararlo |
| --- | --- |
| `/home` (SAGUI-DATOS), `/jogos` (SAGUI-JOGOS), `/apps`, un pendrive | `reparar -y hdaN` (se desmonta y se vuelve a montar sola) |
| La capa de la live (SAGUI-PERSIST) | en uso como `/`: arrancar con **M → «2 todo a la RAM»** y entonces `reparar -y hdaN` |
| La raíz instalada | desde la live o el modo rescate (no está montada), o reinstalar (se conservan `/home` y la configuración) |

Solo ext2 (ni ext3 ni ext4, que llevan diario: para esos, e2fsck en Linux).
