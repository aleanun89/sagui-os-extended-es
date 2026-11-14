# Cómo grabar y probar SaguiOS

Cada fase entrega dos archivos en `entrega/faseX/` (y la versión final en `entrega/final/`):

| Archivo | Para qué |
| --- | --- |
| `saguios.img` | Imagen en bruto del disco. Es lo que va, byte a byte, al pendrive de arranque del G470. |
| `saguios.vdi` | La misma imagen convertida al formato de VirtualBox. |
| `saguios.iso` | CD arrancable (`make iso`, D077), para los PC que no arrancan desde USB. Ver la sección 6. |
| `SHA256SUMS.txt` | Huellas digitales de los dos archivos, para comprobar que la descarga no ha corrompido nada. |

**Orden de prueba:** primero en VirtualBox (`.vdi`). Si funciona, en el G470 (`.img`).

> **Atención:** grabar la imagen **borra todo** lo que haya en el pendrive. Usa un pendrive sin nada importante.
> SaguiOS nunca escribe en el disco interno del portátil.

---

## 1. Comprobar el archivo descargado (opcional, recomendado)

- **Windows (PowerShell):** `Get-FileHash .\saguios.img -Algorithm SHA256`
- **Linux:** `sha256sum saguios.img`

El resultado tiene que ser igual al que aparece en `SHA256SUMS.txt`.

---

## 2. Probar en VirtualBox (`saguios.vdi`)

### Crear la VM (solo la primera vez)

1. **Máquina > Nueva**.
2. Nombre: `SaguiOS`. Tipo: **Other**. Versión: **Other/Unknown** (32 bits).
3. Memoria: **512 MB** (sirve cualquier valor de 64 MB a 2 GB).
4. Disco duro: **Usar un archivo de disco duro virtual existente** y elegir `saguios.vdi`.
5. Antes de encenderla, en **Configuración > Sistema > Placa base**, deja **Habilitar EFI desmarcado**. SaguiOS arranca por la BIOS tradicional.
6. En **Configuración > Almacenamiento**, el disco debe estar en el controlador **IDE** (es el predeterminado para "Other").

### Actualizar la VM en una fase nueva

VirtualBox recuerda cada `.vdi` por su identificador interno, y cada fase genera un archivo nuevo:

1. Con la VM apagada, en **Configuración > Almacenamiento**, selecciona el disco antiguo y haz clic en **Quitar conexión**.
2. **Archivo > Herramientas > Administrador de medios virtuales**: selecciona el `saguios.vdi` antiguo y haz clic en **Eliminar** (elige **Conservar** el archivo, si te lo pregunta; o bórralo, da igual).
3. Vuelve a **Almacenamiento**, haz clic en el controlador IDE, **Añadir disco duro** y elige el `saguios.vdi` nuevo.

### Ver el log del kernel en VirtualBox (opcional)

El kernel escribe un log en el puerto serie. Para verlo: **Configuración > Puertos serie > Puerto 1**, marca **Habilitar**, modo **Archivo sin formato** y elige una ruta, por ejemplo `C:\temp\sagui-serial.txt`. Después de encender la VM, abre ese archivo con el Bloc de notas.

---

## 3. Grabar el pendrive de arranque (`saguios.img`)

Sirve cualquier pendrive (SaguiOS ocupa menos de 3 MB). Elige **uno** de los programas siguientes.

### Opción A: Rufus (Windows)

1. Abre Rufus y elige el pendrive en **Dispositivo**.
2. En **Elección de arranque**, haz clic en **SELECCIONAR** y elige `saguios.img`.
   (Si el filtro de archivos muestra solo `.iso`, cambia el filtro a **Todos los archivos** o **\*.img**.)
3. Rufus reconoce el `.img` como imagen de disco en bruto y desactiva las opciones de partición. Es lo esperado.
4. Haz clic en **EMPEZAR**. **Si pregunta el modo de escritura, elige "Escribir en modo Imagen DD".**
5. Confirma el aviso de que se borrarán los datos del pendrive.

> En el proyecto anterior Rufus falló con una ISO. Con el `.img` en bruto, en modo DD, copia la imagen sin alterar nada.
> Si aun así hay problemas, usa balenaEtcher.

### Opción B: balenaEtcher (Windows, macOS, Linux)

1. **Flash from file** y elige `saguios.img`.
2. **Select target** y elige el pendrive.
3. **Flash!** Etcher graba y comprueba la grabación al final.

### Opción C: `dd` (Linux)

```bash
lsblk                          # descubra qual é o pendrive, ex.: /dev/sdb (NÃO uma partição como /dev/sdb1)
sudo dd if=saguios.img of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

> Comprueba tres veces el `/dev/sdX`. Equivocarse aquí borra el disco equivocado.

### Después de grabar

Windows puede decir que el pendrive "hay que formatearlo" o que no reconoce la partición. **No lo formatees**: es normal, la partición de SaguiOS tiene un formato propio. Para usar el pendrive para otra cosa después, basta con volver a formatearlo.

---

## 4. Arrancar el Lenovo G470

1. Apaga el portátil. Conecta el pendrive a un puerto USB.
2. Enciéndelo y pulsa **F2** repetidamente para entrar en el **Setup de la BIOS**.
   - En **Boot**, comprueba que el **Boot Mode** es **Legacy Support** / **Legacy** (si existe la opción).
   - Si hay **Secure Boot**, déjalo en **Disabled** (el G470, de 2011, normalmente ni siquiera tiene esa opción).
   - **Save & Exit** (F10).
3. Enciéndelo de nuevo y pulsa **F12** repetidamente para abrir el **menú de arranque**.
4. Elige el pendrive (aparece como **USB HDD: <nombre del pendrive>**).
5. SaguiOS debería empezar a cargar.

Si se cuelga o aparece un error, **fotografía la pantalla entera**. Los mensajes `[1/6]` a `[6/6]` muestran en qué etapa se ha detenido.

### Mensajes de error del cargador

| Mensaje | Qué significa |
| --- | --- |
| `Cabecera invalida` | El LBA 1 no tiene la cabecera de SaguiOS: grabación incompleta o hecha en modo ISO. Vuelve a grabar en modo DD. |
| `Error de lectura` / `ERROR: fallo de lectura del disco (INT 13h)` | La BIOS no ha podido leer el pendrive (o el CD). Prueba otro puerto USB u otro pendrive. |
| `ERRO: dados lidos estao corrompidos` | Lo ha leído, pero el contenido no coincide con la suma del build. Vuelve a grabarlo; si se repite, prueba otro pendrive. |
| `ERRO: nao consegui ligar a linha A20` | Ninguno de los tres métodos de A20 ha funcionado en ese hardware. |
| `ERRO: BIOS nao deu o mapa de memoria` | La BIOS no ha respondido a E820. |

---

## 5. Pendrive de datos (fase 6)

SaguiOS lee y escribe pendrives **FAT32** en `/usb`. Se puede usar el **mismo pendrive** del arranque, porque el sistema se ejecuta entero en la RAM:

1. Arranca desde el pendrive con `saguios.img`.
2. Con el escritorio en marcha, quita el pendrive (el sistema sigue funcionando).
3. En un PC, formatea el pendrive en **FAT32**:
   - **Windows, hasta 32 GB:** clic derecho en el pendrive > **Formatear** > Sistema de archivos **FAT32**.
   - **Más de 32 GB:** Windows no ofrece FAT32. Usa **Rufus**: en "Elección de arranque" elige **No arrancable** y en "Sistema de archivos" elige **FAT32**.
   - **Linux:** `sudo mkfs.vfat -F 32 -n MEUPEN /dev/sdX1` (la partición).
4. Vuelve a conectar el pendrive con SaguiOS encendido: aparece en el escritorio.

> Formatear borra SaguiOS del pendrive: para el siguiente arranque, vuelve a grabar `saguios.img`.
>
> **Alternativa sin borrar el arranque:** después de grabar `saguios.img`, crea una partición **FAT32** en el espacio libre del pendrive (en Windows, **Administración de discos** > clic derecho en el espacio no asignado > **Nuevo volumen simple** > FAT32; en Linux, `gparted`). SaguiOS arranca desde la primera partición y monta la FAT32 en `/usb`.
> Antes de quitar el pendrive, usa **Extraer con seguridad** (o `desmontar` en el Terminal).

---

## 6. CD arrancable (`saguios.iso`)

Los PC de 1997-2005 casi nunca arrancan desde USB, pero sí desde CD. `make iso` genera
`build/saguios.iso` (con su `.sha256`), hecha por `tools/mkiso.py` (sin programas externos).

- **Grabar:** como **imagen de disco** («Grabar imagen» en Windows, «Grabar imagen» en Brasero o
  `wodim dev=/dev/sr0 saguios.iso` en Linux), **no** como un archivo dentro de un CD de datos.
  Basta un CD-R (ocupa 8 MB).
- **Arrancar:** en el Setup de la BIOS, pon el **CD-ROM** antes que el disco duro (o usa el menú
  de arranque, normalmente F12, F11 o Esc).
- **En QEMU:** `./run_in_qemu.sh --cd` (usa `-cdrom` y `-boot d`). En VirtualBox: añade un lector
  óptico con `saguios.iso` y ponlo primero en el orden de arranque.
- **Cómo funciona (D077):** el CD usa El Torito con *emulación de disco duro*: la BIOS presenta
  la imagen de SaguiOS de dentro del CD como el primer disco. Como muchas BIOS no ofrecen la
  lectura por LBA en ese modo, el cargador lee por CHS. El sistema se carga entero en la RAM, así
  que el CD se puede sacar después de arrancar.
- **Solo lectura:** desde el CD no se puede guardar nada en el disco de arranque. En particular,
  `pantalla --guardar` (la resolución para el próximo arranque) responde «no se puede guardar:
  SaguiOS ha arrancado desde un CD». Los pendrives FAT32 (`/usb`) funcionan como siempre.
- Dentro de la ISO también está `SAGUIOS.IMG`, la imagen de disco (completada hasta 7,8 MB),
  por si quieres copiarla a un pendrive desde otro sistema.

---

## 7. PC antiguos (Pentium II/III, Athlon, Pentium 4...)

Lo que SaguiOS hace solo (mejora 036, fase A):

| Situación | Qué pasa |
| --- | --- |
| CPU sin SSE2 (Pentium III, Athlon XP) | El kernel y las bibliotecas usan `rep movsd` en lugar de SSE2; `cc` genera código para la FPU x87 (`-sse2` lo fuerza, con aviso); `simd` solo prueba lo que hay. **Python con números reales necesita SSE2** (pendiente). |
| Tarjeta de vídeo que solo da 16 o 15 bits | El cargador acepta 32, 24, 16 y 15 bits (en ese orden a igual resolución) y el sistema convierte al copiar a la pantalla. Si no hay 1024x768, 800x600 o 640x480. |
| Sin arranque por USB | Usa el CD (sección 6). |
| BIOS sin lectura LBA | El cargador lee por CHS. |

**Profundidad de color a mano:** una imagen hecha con `tools/mkimagem.py ... --bpp 16` (o 15, 24,
32) prefiere esa profundidad. Sirve si la tarjeta es lenta o tiene poca memoria: 16 bits
mueve la mitad de datos que 32.

**Si algo no funciona, envía un informe:** en el Terminal, `infopc --informe`. Escribe un
`.txt` en `/home`, en el pendrive (`/usb`) o, si no hay ninguno, en `/tmp` (avisa: conecta un
pendrive FAT32 y repite). Cómo enviarlo y qué hay dentro: [`compatibilidad.md`](compatibilidad.md).

## Pendrive con persistencia en FAT32 (2 GB)

`entrega/v3/saguios-persistente-fat32.img.xz` (o `make imagem-persistente-fat32
[DATOS_FAT32_MB=2048]`, que deja `build/saguios-persistente-fat32.img`) es la misma imagen
con una segunda partición **FAT32 de 2 GB** (etiqueta `SAGUI-DATOS`). SaguiOS la monta en
**`/home`** al arrancar (`/etc/fstab`: `arranque /home auto rw`): lo que guardes allí
(documentos, `/home/.zona`, certificados añadidos...) sigue después de reiniciar, y la
partición **también se lee y se escribe desde Windows**.

- Necesita un pendrive de **4 GB o más**.
- **Rufus** y **balenaEtcher** graban el `.img.xz` directamente (sin descomprimir): con Rufus,
  elige el archivo, acepta el modo **DD** y empieza. Se escriben ~2,1 GB, así que tarda unos
  minutos.
- Desde Windows, la partición de datos aparece como unidad `SAGUI-DATOS`. La partición de
  SaguiOS sigue sin formato que Windows entienda: **no la formatees**.
- La imagen con ext2 (`make imagem-persistente`) sigue existiendo; la FAT32 tiene la ventaja
  de Windows y el inconveniente de que FAT32 no guarda permisos ni archivos de más de 4 GB.
