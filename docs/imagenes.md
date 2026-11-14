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
| 1 | (tipo 0xDA) | arranque | SaguiOS (≈6 MB) |
| 2 | `SAGUI-DATOS` | `/home` | tus archivos |
| 3 | `SAGUI-TMP` | `/tmp` | temporales; sustituye al `/tmp` en RAM solo si la partición existe |
| 4 | `SAGUI-LOGS` | `/logs` | registros del kernel (ver abajo) |

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
