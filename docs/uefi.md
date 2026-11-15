# Arranque UEFI (D264, v8.0)

SaguiOS arranca por **BIOS** (MBR, como siempre) y por **UEFI** (x64 e IA32) con **la misma
imagen** `build/saguios.img`. El kernel sigue siendo de 32 bits: el cargador UEFI hace con el
firmware lo que la etapa 2 hace con la BIOS y le entrega la misma `boot_info`.

## Qué hay en la imagen

```
LBA 0          MBR (etapa 1) y tabla de particiones
LBA 1          cabecera de arranque (boot/cabecalho.inc)
LBA 2...       etapa 2, kernel, ramdisk                 <- partición 1 (0xDA, activa): BIOS
alineada 1 MB  ESP: FAT32 «SAGUI-EFI», tipo 0xEF        <- partición 3 (primaria): UEFI
               extendida con SAGUI-APPS, SAGUI-JOGOS, SAGUI-CONF...  <- partición 2 (D213)
```

La ESP lleva:

| Fichero | Qué es | Cómo se genera |
|---|---|---|
| `EFI/BOOT/BOOTX64.EFI` | cargador para firmware de 64 bits (PE32+) | `make uefi` (`boot/uefi/`, `tools/mkefi.py`) |
| `EFI/BOOT/BOOTIA32.EFI` | cargador para firmware de 32 bits (PE32) | ídem |
| `SAGUI/CABECERA.BIN` | la cabecera de arranque (512 bytes, igual que el LBA 1) | `tools/mkimagem.py` |
| `SAGUI/KERNEL.BIN` | el kernel (`build/kernel.bin`) | ídem |
| `SAGUI/RAMDISK.SGF` | el ramdisk SGFS (`build/ramdisk.sgfs`) | ídem |

`tools/mkimagem.py --uefi build/boot/uefi` la añade (función `montar_esp`); `make` ya hace así
`saguios.img` y, desde D270, todas las imágenes de disco (multinúcleo, uACPI, persistentes,
particiones, sin GPL). La ISO arranca por UEFI con su propio `EFI.IMG` (abajo).

**Discos instalados (D270).** El ramdisk lleva el cargador en `/sistema/efi/EFI/BOOT/` y el
instalador lo copia a la ESP del disco de destino; la ESP de un disco instalado **solo lleva el
cargador**. El cargador busca SaguiOS en el disco de la ESP: el LBA 1 (imagen híbrida y discos
instalados con MBR) o, en una GPT, el primer sector de la partición de tipo SaguiOS
(`53414755-494F-4F53-8001-534147554953`), donde el instalador pone la cabecera; y lee el kernel y el
ramdisk de los sectores que dice esa cabecera. Solo si la ESP lleva `SAGUI/CABECERA.BIN` del
mismo sistema (la imagen híbrida) los lee de los ficheros de la ESP.

**GPT híbrido:** no. El LBA 1 es la cabecera de SaguiOS (el único LBA fijo del proyecto) y en GPT
es la cabecera GPT; la imagen híbrida va con MBR, que todo firmware UEFI acepta (la ESP con el
tipo 0xEF).

**ISO (D270):** además de la entrada de la BIOS (emulación de disco duro, D077), el catálogo de
El Torito tiene una sección UEFI (plataforma 0xEF) con `EFI.IMG`, un FAT12 de 1,44 MB con solo
`EFI/BOOT/BOOTX64.EFI` y `BOOTIA32.EFI` (`tools/mkiso.py --efi`). El cargador encuentra la imagen
de disco de la entrada de la BIOS en el catálogo y lee de ella la cabecera, el kernel y el ramdisk.

## Cómo funciona el cargador

1. Lee la cabecera del disco de la ESP (LBA 1, partición GPT de SaguiOS o imagen El Torito del
   CD); así el kernel reconoce su disco de arranque y `pantalla --guardar` (que graba esa misma
   cabecera) vale para el próximo arranque. Sin ella, `SAGUI/CABECERA.BIN` de la ESP.
2. Lee el kernel y el ramdisk (de la ESP si es el mismo sistema; si no, del disco) y comprueba
   las sumas de la cabecera.
3. Busca el RSDP de ACPI y el SMBIOS en la tabla de configuración de UEFI (sin BIOS no están en
   0xE0000-0xFFFFF) y, de la MADT y la HPET, los IOAPIC y el HPET.
4. Elige el modo de vídeo con **GOP**: la resolución guardada en la cabecera, la nativa del panel
   (EDID, hasta 1920x1200), 1024x768, 800x600, 640x480 u otra; siempre 32 bits (BGRx, RGBx o con
   máscaras: el kernel ya pinta con cualquier posición de los colores).
5. Convierte el mapa de memoria de UEFI a E820 (los tipos que dice ACPI), sale de los servicios
   de arranque (`ExitBootServices`) y salta al **trampolín**, que pasa a modo protegido de 32 bits
   sin paginación (en x64, saliendo del modo largo), deja el APIC local en «cable virtual», los
   IOAPIC enmascarados y el HPET sin la ruta heredada (como los deja la BIOS: el PIC 8259 y el PIT
   funcionan), mueve el kernel a 1 MB si hacía falta y salta al kernel con `EAX = BOOT_MAGICO`.
   El ramdisk no se mueve: queda donde el firmware dio memoria y el kernel lo encuentra por la
   `boot_info` (en OVMF, por ejemplo, hay memoria del firmware en 8 MB).

Después del arranque el kernel no usa nada de la BIOS ni de UEFI: ni servicios de ejecución, ni
VBE, ni INT 13h. Las resoluciones de la lista son las de la GOP; cambiar de resolución en caliente
solo se puede con una tarjeta con driver (Bochs/QEMU, VMware...); en las demás, `pantalla --guardar`
y reiniciar (como por BIOS).

**Rescate:** Ctrl al arrancar funciona si el firmware entrega la tecla con el estado de Ctrl
(Simple Text Input Ex); si no, la imagen con `--rescate`.

## Probar en QEMU

```
./run_in_qemu.sh --uefi          # OVMF de 64 bits
./run_in_qemu.sh --uefi ia32     # OVMF de 32 bits
./run_in_qemu.sh --uefi --cd     # la ISO por UEFI (make iso antes)
python3 tests/h264_uefi.py       # la prueba automática (x64, IA32, USB y BIOS)
python3 tests/h270_uefi_gpt.py   # instalar en GPT/NVMe por UEFI y arrancar el disco instalado, SMP, ISO
```

OVMF: `sudo apt install ovmf ovmf-ia32` o `bash tools/entorno-local.sh` (sin sudo).

## Probar en el Ryzen / ASUS B450

Graba `saguios.img` en un pendrive como siempre (Rufus en modo DD, o `dd`).

1. **Secure Boot desactivado.** El cargador no está firmado. En la BIOS del ASUS: *Boot → Secure
   Boot → OS Type: «Other OS»* (con «Windows UEFI mode» no arranca nada sin firmar).
2. **Sin CSM (UEFI puro):** *Boot → CSM (Compatibility Support Module) → Launch CSM: Disabled*.
   En el menú de arranque (F8) elige la entrada **«UEFI: <nombre del pendrive>»**.
3. **Con CSM:** *Launch CSM: Enabled*. El menú (F8) ofrece dos entradas para el mismo pendrive:
   la que empieza por «UEFI:» arranca por UEFI; la otra, por BIOS (la etapa 2 de siempre). Prueba
   las dos.
4. **Above 4G Decoding / Resizable BAR:** si están activados, la GOP de la tarjeta gráfica puede
   quedar por encima de 4 GB, donde un kernel de 32 bits no llega. El cargador lo avisa («el
   framebuffer está por encima de 4 GB»); desactívalos.

### Qué foto mandar si falla

- **Se para en el texto del cargador** («SaguiOS - cargador UEFI x64», líneas `[1/6]`..`[6/6]`):
  foto de la pantalla entera; la última línea dice la etapa y el `ERROR:` el motivo.
- **Franja roja arriba y nada más:** falló justo después de salir de los servicios de arranque
  (el destino del kernel ocupado): foto y modelo de la placa y versión de la BIOS.
- **Pantalla negra después de `[6/6]`:** el kernel no llegó a pintar. Si se puede, arranca por
  BIOS (con CSM) e infórmalo; prueba también el otro puerto de vídeo de la tarjeta.
- **Llega al escritorio:** `infopc --informe` (o «Información del PC») dice `firmware=uefi-x64`
  en el registro; manda el informe y una foto, y prueba teclado, ratón, red y apagado.

## Instalar en el NVMe del Ryzen / ASUS B450 por UEFI (D270)

El instalador, arrancado por UEFI, deja el disco con **GPT** (modo automático): partición EFI
(256 MB, FAT32 `SAGUI-ESP`, con el cargador), SaguiOS, `/home` (`SAGUI-DATOS`) y `SAGUI-CONF`.
**Se borra todo el disco de destino.** Si el NVMe tiene Windows u otros datos, no lo hagas: usa
otro disco.

1. Prepara la BIOS como arriba (Secure Boot en «Other OS»; CSM como quieras) y arranca el pendrive
   con la entrada **«UEFI: <pendrive>»**. Comprueba en «Información del PC» (o `infopc --informe`)
   que dice `firmware=uefi-x64` y que se ve el NVMe (`[NVMe] nvme0 ...`, o `discos` en el terminal).
2. Abre **Inicio > Sistema > Instalar SaguiOS** y elige `nvme0`. Con el arranque por UEFI la
   opción preseleccionada es «Usar el disco entero (...), tabla GPT (UEFI)». O en el terminal:
   `admin` y `instalar nvme0` (dice «Tabla de particiones: GPT ... [automática: se arrancó por
   UEFI]»; escribe `nvme0` para confirmar).
3. Al terminar dice «SaguiOS instalado en nvme0 y verificado». Apaga, quita el pendrive y
   arranca: en el menú de arranque (F8) el NVMe aparece como **«UEFI OS»** o con su modelo
   (el firmware arranca `EFI/BOOT/BOOTX64.EFI` de la ESP). Si no aparece, en *Boot → Boot Option
   #1* elige el NVMe; algunas placas solo lo listan si CSM está apagado.
4. En el texto del cargador sale `[1/6] cabecera: la de la particion de SaguiOS (GPT, sector
   ...); sistema leido del disco`. Ya en el escritorio, el registro dice
   `[DISCO] nvme0: es el disco de arranque (GPT: cabecera en nvme0p2 ...)` y `[CONF] SAGUI-CONF en ...`.
5. `pantalla 1920x1080 --guardar` (o la resolución que quieras) graba la resolución en el disco
   instalado; el cargador la usa en el arranque siguiente.

Si `discos` no lista `nvme0` y el registro dice `[NVME] ... BAR0 inexistente o por encima de 4 GB`,
apaga *Above 4G Decoding* (y *Resizable BAR*) en la BIOS: el kernel de 32 bits no llega por encima
de 4 GB (D270).

**Qué mandar si falla:** foto del texto del cargador (`ERROR: no se encuentra SaguiOS...` si no
ve la partición de SaguiOS: di si el NVMe lleva bloques de 512 o 4096, `discos` lo dice en el
pendrive), o el informe de `infopc` arrancando desde el pendrive. Recuerda: el NVMe es un disco
interno; SaguiOS solo lo escribe al instalar, al guardar la resolución o si se monta con
`mount -o rw`. Un NVMe con bloques de 4096 todavía no se puede reparticionar (D265): el
instalador lo rechaza.
