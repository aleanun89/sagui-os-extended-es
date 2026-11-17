# Imágenes de disco con varias particiones (D120)

`make imagem-particoes` crea un disco de SaguiOS listo para grabar, con **/home**, **/tmp** y **/logs**
en particiones propias y del tamaño que quieras. Una sola imagen (`.img`), varias particiones MBR.

## Crear la imagen

```
. ~/.local/saguitools/env.sh        # solo si las herramientas están sin sudo (mejora 005)
make imagem-particoes HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32
```

- `HOME_MB`, `TMP_MB`, `LOGS_MB`: tamaño en MB de cada partición; **0 = sin esa partición**.
  Por defecto 2048, 256 y 128. FAT32 necesita al menos 33 MB; ext2, 2 MB.
- `PART_FS`: `fat32` (por defecto; Windows lee y escribe los archivos) o `ext2` (Linux).
- Resultado: `build/saguios-particiones.img`. La imagen es **dispersa**: aunque mida varios GB,
  ocupa en el PC solo unos MB (lo que de verdad se ha escrito).
- `make imagem-particoes-xz` crea además `build/saguios-particiones.img.xz`.
- Asistente: `python3 tools/crear-disco.py -i` pregunta los tamaños y el sistema y llama a `mkimagem.py`.
- Al terminar se imprime la tabla de particiones y los pasos para grabarla.

| Partición | Etiqueta | Se monta en | Contenido |
| --- | --- | --- | --- |
| 1 | (tipo 0xDA) | arranque | núcleo e initramfs (≈2 MB) |
| 2 | `SAGUI-DATOS` | `/home` | tus archivos |
| 3 | `SAGUI-TMP` | `/tmp` | temporales; sustituye al `/tmp` en RAM solo si la partición existe |
| 4 | `SAGUI-LOGS` | `/logs` | registros del kernel (ver abajo) |
| — | `SAGUI-JOGOS`, `SAGUI-RAIZ`, `SAGUI-CONF` | `/jogos`, `/`, `/sistema/conf` | juegos, el sistema (D379) y su configuración |

Las tres están alineadas a 1 MB. SaguiOS las monta solo al arrancar (`/etc/fstab`, por etiqueta) y
**solo en el disco que tiene la partición de SaguiOS**: un pendrive cualquiera con una etiqueta
`SAGUI-TMP` no se monta en `/tmp`. Los discos internos (ATA) se siguen montando solo para lectura
si el fstab no dice `rw` (D040).

### Más de cuatro particiones: extendida y lógicas

El MBR admite cuatro primarias. Con `PART_EXT=--extendida` (o con `--extra-mb`) la primera partición de
datos (`/home`) es primaria y el resto son **lógicas** dentro de una **extendida** (tipo 0x0F), numeradas
desde 5 como en Linux (`usb0p5`, `usb0p6`...):

```
make imagem-particoes PART_EXT="--extendida --extra-mb 512 --extra-mb 1024"
```

Las extras se llaman `SAGUI-X1`, `SAGUI-X2`... y no tienen carpeta fija (se montan con
`mount LABEL=SAGUI-X1 /datos` o añadiendo una línea a `/etc/fstab`). Máximo 16 lógicas.
El kernel reconoce cualquier extendida (0x05, 0x0F, 0x85), también las creadas con otras herramientas;
una cadena rota, en bucle o con tamaños absurdos se ignora con un aviso en el registro, sin colgar nada.

## Cómo arrancan las imágenes desde la v8.5.3.1 (D379)

Todas las imágenes de serie (`saguios.img`, `-smp`, `-uacpi`, las persistentes, la de particiones, la VDI y
la ISO) arrancan como un Unix: la BIOS o el cargador UEFI solo leen el núcleo y un **initramfs** de menos de
1 MB; el sistema entero (programas, `/lib`, `/apps` con las aplicaciones, `/etc`) está en la partición
**SAGUI-RAIZ** (ext2) y se lee del medio según hace falta. Por eso arranca antes y deja más RAM libre.

| Imagen | Cómo se usa la raíz | Qué se guarda al apagar |
| --- | --- | --- |
| `saguios.img`, `-smp`, `-uacpi`, `-particiones`, VDI | **live desde el medio**: SAGUI-RAIZ de solo lectura y los cambios en la RAM | la configuración del sistema (SAGUI-CONF, como siempre), `/home` si hay SAGUI-DATOS, `/jogos` |
| `saguios-persistente*.img` | live con **SAGUI-PERSIST**: los cambios al sistema (programas instalados, `/etc`, `/root`) se guardan en esa partición | todo: además `/home` en SAGUI-DATOS |
| `saguios.iso` | **toram**: la raíz (comprimida en el CD desde la v8.5.4.2: la ISO pesa la mitad) se descomprime a la RAM al arrancar y el CD se puede sacar; si no hay RAM bastante (unos 32 MB más que la raíz, ~80 MB), avisa y se usa leyendo del CD | nada (CD) |
| disco instalado (`instalar`) | SAGUI-RAIZ con escritura (como un Linux instalado) | todo; `/home` va aparte, en SAGUI-DATOS |

La partición SAGUI-RAIZ del pendrive **nunca se escribe** en la live (un corte de luz no la estropea). Si la
raíz falta, es de otra versión o está incompleta, SaguiOS sigue con el sistema mínimo del initramfs (modo
rescate, consola de root) y dice por qué; las líneas `[RAIZ]` de `dmesg` dan el detalle.

**Elegir el modo al arrancar (v8.5.4.2):** con la tecla **M** pulsada al encender sale un menú: 1 live desde
el medio, 2 todo a la RAM, 3 instalado (la raíz con escritura), 4 rescate (solo el sistema mínimo); Intro deja
el de siempre. `kconfig raiz_modo=live` o `raiz_modo=toram` lo deja guardado (`kconfig raiz_modo=defecto` lo
quita). En la live, lo que se escribe en la RAM tiene un tope (la mitad de la RAM; `kconfig capa_ram_pct`):
al llenarse, «no queda espacio» y un aviso, sin que se caiga el sistema.

**Instalar desde la live** copia por defecto el sistema **de serie** (sin lo cambiado en la sesión);
`instalar ... --con-cambios` (o la casilla del asistente) lleva también lo cambiado. Cada disco instalado lleva
su identificador: con dos discos SaguiOS conectados (el instalado y un pendrive live), cada uno usa su raíz.

**Si no encuentra la raíz** (el PC no deja a SaguiOS ver el disco o el pendrive del que arrancó, #36), el modo
rescate lista los discos que ve y recomienda **`saguios-completo.img`**. Un ext2 que no se desmontó bien
(corte de luz) pasa una comprobación básica al arrancar (mapas y contadores, como `e2fsck`); si no se puede
comprobar, se usa de solo lectura y se avisa.

**Reinstalar** sobre un disco con SaguiOS (de cualquier versión) conserva la partición de `/home`
(SAGUI-DATOS) tal cual y la configuración (contraseñas, claves de ssh y vnc, lo cambiado en `/etc` y `/root`);
`instalar ... --formatear-home` y `--formatear-conf` las borran si se pide. Los discos instalados con la
v8.5.3 o anteriores siguen arrancando como siempre; para pasar al sistema nuevo, se reinstala encima.

## `saguios-completo.img` (D324)

**`saguios-completo.img`** lleva todo el sistema y las aplicaciones **en la memoria** (el ramdisk de
siempre, unos 34 MB), como hasta la v8.4.2, y no necesita leer el disco después de arrancar. Úsala en los
**PC donde la controladora de disco no localiza bien el disco de arranque**: en algunos PC de época (IDE sin
LBA, #36) o con controladoras que SaguiOS aún no maneja bien, el sistema arranca por la BIOS pero luego no ve
el disco; con las imágenes normales se queda entonces en el modo rescate («No se encuentra la partición
raíz»). Pide más RAM: unos 64 MB más que la normal, y no lleva Freeciv ni OpenTTD (sus rutas no caben en el
formato del ramdisk).

Para generarla: `make RAMDISK=completo` (o `RAMDISK=completo ./gerar_versao.sh`). Con `RAIZ_DISCO=0`, todas
las imágenes salen con el ramdisk de siempre (como hasta la v8.5.3).

Si pasa con tu PC, apunta el modelo (placa y controladora de disco) y abre una issue con el registro
(`/logs`): así se arregla el driver.

## Grabarla con Rufus (Windows)

1. Abre Rufus y conecta el pendrive o disco USB de destino (**se borra todo lo que tenga**).
2. En *Dispositivo* elige el pendrive. Pulsa **SELECCIÓN** y escoge `saguios-particiones.img`
   (o el `.img.xz`; Rufus lo descomprime).
3. Pulsa **EMPEZAR**. Cuando pregunte, elige **«Grabar en modo Imagen DD»** (no «modo ISO»).
4. Cuando termine, arranca el PC desde ese pendrive. Con FAT32, Windows puede ver las particiones de
   datos del pendrive desde *Administración de discos* (a veces solo muestra la primera: asígnale letra
   a las demás).

Un pendrive necesita al menos el tamaño total de la imagen (la tabla impresa lo indica).

## Grabarla con dd (Linux/WSL)

```
sudo dd if=build/saguios-particiones.img of=/dev/sdX bs=4M conv=fsync status=progress
```

Comprueba bien `/dev/sdX` con `lsblk` antes: dd no pregunta.

## /logs: el registro del kernel

Si `/logs` está montado para escribir, el kernel guarda el registro de `klog` (los últimos 32 KB)
en `/logs/kernel-AAAAMMDD-HHMM.log`:

- al montar la partición (lo acumulado desde el arranque) y
- al apagar o reiniciar (antes de desmontar todo; también en el apagado por ACPI de D113).

Es un archivo por arranque (si hay dos en el mismo minuto, el segundo lleva `-2`). Se conservan los 10
más recientes. Sin partición `SAGUI-LOGS` no se escribe nada. Además, `infopc --informe` deja una copia
del informe de compatibilidad en `/logs/informe-AAAAMMDD-HHMM.txt`.
