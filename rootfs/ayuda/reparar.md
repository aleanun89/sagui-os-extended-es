# Reparar un disco ext2 («reparar»)

Un ext2 no tiene diario: si el ordenador se cuelga o se va la luz con un volumen montado, pueden quedar
mapas de bits y contadores que no cuadran, archivos que comparten un bloque, entradas de carpeta que
apuntan a archivos borrados o archivos sin carpeta. Al arrancar, SaguiOS comprueba lo sencillo (mapas y
contadores) y lo arregla solo; si encuentra algo más, monta el volumen **de solo lectura** y avisa:

    hda9: no se desmontó bien y no se puede comprobar (un inodo apunta fuera del volumen): solo lectura; ejecuta «reparar hda9»

Lo que hay se puede leer y copiar. Para arreglarlo, como root, en el Terminal:

| Orden | Qué hace |
| --- | --- |
| `reparar -n hda9` | Solo mira: dice todo lo que está mal y qué haría, sin escribir nada. |
| `reparar hda9` | Lo mismo, y pregunta antes de arreglarlo. |
| `reparar -y hda9` | Lo arregla sin preguntar. |
| `reparar --deshacer COPIA hda9` | Deja hda9 como estaba antes de repararla. |

Si la partición está montada de solo lectura, `reparar` la desmonta, la repara, la comprueba otra vez y
la vuelve a montar donde estaba, **ya con escritura**. Una partición montada con escritura no se toca
(desmóntala antes con `umount`).

## Qué arregla

- **Punteros a bloques fuera del volumen**: se quitan (el archivo queda con un hueco o más corto). Si un
  inodo tiene más de 12, es basura y se borra.
- **Bloques que usan dos archivos**: no se sabe de cuál era de verdad, así que cada archivo se queda con
  una copia (no se pierde ninguna de las dos versiones). Al final se listan para que los revises.
- **Inodos estropeados** (tipo desconocido, enlaces simbólicos imposibles): se borran.
- **Carpetas**: entradas rotas, entradas a archivos borrados, «.» y «..».
- **Archivos y carpetas sin carpeta**: van a `/lost+found` con el nombre `#<inodo>`.
- **Número de enlaces, tamaños, mapas de bits y contadores**, y la marca de «desmontado bien».

## Reparación automática al arrancar

El servicio `reparar-arranque` (`/etc/servicios/reparar-arranque.conf`, encendido por defecto) mira al
arrancar los ext2 que el kernel montó de solo lectura por no haberse desmontado bien (por ejemplo
`/home` tras un cuelgue) y les pasa `reparar -p` (modo «preen»). Ese modo **solo** arregla lo que no
pierde nada: mapas de bits, contadores, enlaces, la marca de «desmontado bien», punteros a bloques que
no existen y bloques compartidos (cada archivo se queda una copia). Si queda limpio, vuelve a montar el
volumen con escritura y avisa en pantalla. Si hace falta algo arriesgado (borrar inodos, descartar
entradas de carpeta, soltar bloques, rehacer el superbloque con su copia), **no toca nada**: el volumen
sigue de solo lectura y el aviso dice «ejecuta `reparar hdaN`». La raíz y la capa de la live en uso no se
tocan, solo se avisa.

Para apagarlo: `servicio desactivar reparar-arranque`, o crear el archivo
`/sistema/conf/reparar-arranque.no`. A mano: `reparar -p hdaN` (lo mismo que hace el servicio con un
volumen) o `reparar --arranque` (todos los que toquen).

## Copia de seguridad

Antes de escribir, `reparar` guarda los bloques que va a cambiar tal y como están, en
`/tmp/reparar/<partición>-<fecha>.copia` (en la memoria: se pierde al apagar). Para guardarla en otro
disco: `reparar --copia /usb/hda9.copia hda9`. Con esa copia, `reparar --deshacer` lo deja todo como
estaba.

## La capa de la live (SAGUI-PERSIST)

Mientras SaguiOS arranca desde la live persistente, su capa de cambios está en uso como raíz y no se
puede reparar. Arranca con **M** en el menú de arranque y elige **«2 todo a la RAM»**: la capa no se usa
y entonces `reparar -y hdaN` la arregla. Después, reinicia normalmente.

## Límites

Solo ext2 (lo que crean SaguiOS y `mke2fs -t ext2`): ni ext3 ni ext4 (con diario, extents...); para
esos, `e2fsck` desde Linux. Si la carpeta raíz del volumen está destruida, tampoco: `e2fsck`.
