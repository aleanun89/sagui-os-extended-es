# Guion de prueba: fase 6 (Pendrive de datos: USB y FAT32)

**Archivos:** `entrega/fase6/saguios.vdi` (VirtualBox) y `entrega/fase6/saguios.img` (pendrive). Las mismas imágenes están en `entrega/final/`.
**Cómo grabar y configurar:** `docs/como-gravar.md`.

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Corrección después de la primera prueba en el G470

El pendrive no aparecía por dos motivos (errores 17 y 18):

1. El controlador USB del G470 trabaja con **direcciones de 64 bits** y exige estructuras más grandes que las que usaba el driver. QEMU usa un controlador de 32 bits, por eso las pruebas de aquí pasaban. **Esta corrección solo se puede confirmar en el G470.**
2. Solo se examinaba la **primera partición** del pendrive. Ahora la FAT32 se encuentra en cualquiera de las cuatro, así que el pendrive con la partición de SaguiOS + la FAT32 de 3 GB que creaste funciona (probado en QEMU con esa misma disposición).

**Segunda corrección:** la foto de `usb` mostró que la primera corrección funcionó (el segundo controlador ya reconocía el hub y el lector de tarjetas), pero los dos controladores del G470 compartían la misma cola de transferencias y uno estorbaba al otro (error 19). Ahora cada uno tiene la suya. Se ha reproducido y probado en QEMU con dos controladores.

Si aún no aparece, ejecuta **`usb`** en el Terminal y fotografía la salida: ahora dice si el controlador es de 64 bits, si la BIOS lo ha soltado y **en qué paso** se ha detenido la conversación con el pendrive en cada puerto.

## Qué ha cambiado desde la fase 5

- SaguiOS tiene un **driver USB 2.0 (EHCI) propio** y lee y escribe **pendrives en FAT32**.
- Al **conectar** un pendrive con el sistema encendido: aparece la notificación `Pendrive conectado: <nome> (<tamanho>)` en la esquina y el **icono del pendrive** en el escritorio.
- **Doble clic en el icono:** lo monta en `/usb` y abre Archivos allí. El Editor guarda y abre archivos en `/usb`.
- **Botón derecho en el icono** (o **Extraer con seguridad** en el panel lateral de Archivos): lo graba todo y muestra `Pode remover o pendrive.`
- Desde el Terminal: `discos`, `montar`, `desmontar`, y todos los comandos (`ls`, `cat`, `cp`, `mv`, `rm`...) funcionan en `/usb`. El `rm` en el pendrive pide confirmación.
- Sagui Atrapa Bananas guarda el récord en `/usb/sagui-recorde.txt` cuando hay un pendrive montado.
- Corrección: los corazones de las vidas aparecen en la pantalla de inicio del juego.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

SaguiOS solo lee pendrives **USB** (un segundo disco virtual normal no sirve). En VirtualBox, el pendrive de verdad del PC se pasa a la VM:

1. **Configuración > USB**: marca **Habilitar controlador USB** y elige **USB 2.0 (EHCI)**. (En VirtualBox 6/7 el USB 2.0 necesita el **Extension Pack**; si no está instalado, sáltate esta etapa y prueba directamente en el G470.)
2. Enciende la VM. Conecta un pendrive **FAT32** al PC y, en el menú de la VM, **Dispositivos > USB**, marca el pendrive. Pasa a la VM como si se hubiera conectado a ella.
3. Sigue la lista de la etapa 2 de abajo (es la misma).

---

## Etapa 2: Lenovo G470 (con un solo pendrive)

Vas a usar el **mismo pendrive** para arrancar y para los datos (el sistema se ejecuta entero desde la RAM):

1. Graba `saguios.img` en el pendrive (Rufus en modo DD, balenaEtcher o `dd`) y arranca el G470 (F12, **USB HDD**).
2. En la pantalla inicial, comprueba la línea **`[ ok ] USB: N controlador(es) EHCI`** (el G470 debería tener **2**).
3. Con el escritorio en marcha, **quita el pendrive**. El sistema sigue funcionando.
4. En el PC, **formatea el pendrive en FAT32** (hasta 32 GB desde Windows; por encima, con Rufus: "No arrancable", sistema de archivos FAT32). Si quieres, copia en él algún archivo de texto y una carpeta con nombre acentuado.
5. **Vuelve a conectar el pendrive al G470**, con SaguiOS encendido.

**Qué deberías ver:**

| Qué hacer | Qué debe pasar |
| --- | --- |
| Conectar el pendrive | Notificación `Pendrive conectado: <nome> (<tamanho>)` en la esquina superior y el icono del pendrive abajo, en la columna de iconos. |
| Doble clic en el icono del pendrive | Notificación `Pendrive montado em /usb` y Archivos se abre mostrando el contenido del pendrive (nombres largos y acentos correctos). |
| Abrir un `.txt` que venga del PC | Se abre en el Editor con los acentos correctos. |
| Doble clic en **Editor**, escribir una nota, **Ctrl+S**, borrar el `/tmp/` y escribir `/usb/nota.txt` | Se guarda en el pendrive. |
| En Archivos, **Nueva carpeta** y **Borrar** dentro del pendrive | Crea y borra (con "¿Seguro?"). |
| En el Terminal: `discos` | `usb0  <fabricante> <produto>  <tamanho>  FAT32  "<nome>"  (montado em /usb)`. |
| En el Terminal: `cp /leia-me.txt /usb/` y `ls /usb` | El archivo aparece. |
| Jugar una partida de **Banana** | El récord queda guardado en `/usb/sagui-recorde.txt`. |
| **Botón derecho** en el icono del pendrive (o **Extraer con seguridad** en Archivos) | `Pode remover o pendrive.` |
| Quitar el pendrive | `Pendrive removido.` y el icono desaparece. |
| En el PC: abrir el pendrive | `nota.txt`, `leia-me.txt` y `sagui-recorde.txt` están ahí. Windows no debería pedir "examinar y reparar" el pendrive. |
| Conectarlo de nuevo, montarlo (doble clic) y **sacarlo sin desmontar** | Aviso `O pendrive foi removido sem desmontar: dados podem ter se perdido.` |

**Atención especial en el G470:**

- **El hub interno del chipset.** En el G470, todo dispositivo USB está detrás de un hub de alta velocidad que existe dentro del chipset. QEMU no tiene un hub equivalente, así que **es la primera vez que el camino del hub se ejecuta de verdad** (D024). Si el pendrive no se reconoce, ejecuta `usb` en el Terminal y **fotografía la salida**: muestra los controladores, los puertos y los dispositivos (el hub debería aparecer como "clase hub").
- **El traspaso del controlador de la BIOS a SaguiOS** también solo ocurre de verdad en el G470. Si el teclado o el touchpad dejan de funcionar justo después de la línea `USB` en el arranque, dímelo.
- **Puertos:** prueba más de un puerto USB del portátil.

**Si sale mal**, fotografía la pantalla y dime:

- **Ninguna notificación al conectarlo:** ejecuta `usb` y `discos` en el Terminal y fotografía la salida.
- **Aparece, pero no se monta:** el mensaje dice el motivo (por ejemplo, "formato no soportado" si está en exFAT o NTFS).
- **Pantalla roja:** la foto muestra el error y los registros.

Cuando lo pruebes, dime **"fase 6 ok"** o describe lo que ha pasado.
