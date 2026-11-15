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
`saguios.img`. La ESP es autosuficiente: el instalador con GPT (otro trabajo de la v8.0) puede
copiar estos cinco ficheros a la ESP del disco de destino. Cuando no hay LBA 1 de SaguiOS (un
disco GPT, cuyo LBA 1 es la cabecera GPT), el cargador usa `SAGUI/CABECERA.BIN`; en ese caso
`CABECERA.BIN` tiene que llevar los tamaños, sumas, direcciones y entrada del kernel y el ramdisk
que hay en la ESP (los LBA no se usan).

**GPT híbrido:** no. El LBA 1 es la cabecera de SaguiOS (el único LBA fijo del proyecto) y en GPT
es la cabecera GPT; la imagen híbrida va con MBR, que todo firmware UEFI acepta (la ESP con el
tipo 0xEF).

**ISO:** la ISO (D077) sigue arrancando solo por BIOS (El Torito con emulación de disco). La
entrada El Torito para UEFI (plataforma 0xEF con una imagen FAT) queda pendiente.

## Cómo funciona el cargador

1. Lee `SAGUI/CABECERA.BIN`; si el disco de la ESP tiene en el LBA 1 la misma cabecera (la
   imagen híbrida), usa la del disco: así el kernel reconoce su disco de arranque y
   `pantalla --guardar` sigue valiendo.
2. Lee el kernel y el ramdisk de la ESP y comprueba las sumas de la cabecera.
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
python3 tests/h264_uefi.py       # la prueba automática (x64, IA32, USB y BIOS)
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
