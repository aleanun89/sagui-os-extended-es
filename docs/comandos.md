# Comandos del Terminal de SaguiOS

Auditoría y estado de los comandos de shell (mejora «terminal-comandos», decisión D097).
Los programas están en `userland/programas/` (van a `/bin`); los internos de sagsh, en
`userland/sagsh/internos.c`. La tabla de nombres, alias en español, secciones y ayuda (`ayuda`; antes
`ajuda`, que sigue valiendo, D385) es `userland/sagsh/ayuda.c`.

Convención: salida 0 = bien, 1 = error de ejecución, 2 = uso incorrecto. Los errores salen por
la salida de errores con el estilo `comando: archivo: no existe`.

## 1. Auditoría inicial (antes de la mejora)

Probado en QEMU (`ls -a`, `cp a b carpeta`, `cat -n`, `rm -r`, `mkdir -p`, `kill -l`, `cat -`...).

| Comando | Opciones que tenía | Faltaban | Errores o rarezas | Prioridad |
|---|---|---|---|---|
| `ls` | `-l` (fecha, tamaño) | `-a -h -R -1 -t -S -r`, comodines ya los expande el shell | `ls -a` da «uso»; con varios argumentos y un archivo, imprime el nombre de cabecera `x:` aunque sea un archivo; tamaño en el formato propio, no `ls -l` de Unix; en una tubería sale con tamaños (no sirve para `grep`/`xargs`) | alta |
| `cp` | ninguna | `-r -i -v -n -f`, varios orígenes a una carpeta | `cp a b carpeta` da «uso»; no copia carpetas; sobrescribe sin avisar | alta |
| `mv` | ninguna | varios orígenes, `-i -v -n -f` | sin varios orígenes; mover a una carpeta solo funciona si el kernel lo admite (no se comprueba que el destino sea carpeta) | alta |
| `rm` | pide confirmación en `/usb` | `-r -f -i -v` | `rm -r x` dice «-r: no existe» e intenta borrar `-r`; no borra carpetas | alta |
| `mkdir` | ninguna | `-p -v` | `mkdir -p a/b` falla: «no existe» | alta |
| `rmdir` | ninguna | `-p -v` | — | media |
| `cat` | varios archivos | `-n -b -s -A`, `-` = entrada estándar | `cat -n x` dice «-n: no existe»; `cat -` también | alta |
| `wc` | solo un archivo; formato propio «N líneas, N palabras, N letras» | `-l -w -c -m`, varios archivos con total | ignora los archivos de después del primero; el formato no es el de Unix (los tests de h38 usan el actual con la entrada estándar: se mantiene cuando no hay opciones) | alta |
| `echo` (interno) | `-n -e` | `-E`, más escapes (`\\`, `\c`, `\xHH`) | — | baja |
| `ps` | ninguna | `-e/-A`, `-f`, columnas, ordenar | solo lista todo, con formato propio | media |
| `kill` (interno) | `%trabajo`, pid | `-l`, `-s SEÑAL`, `-NÚMERO` útil | `kill -l` no hace nada; `-9`, `-TERM` se ignoran sin avisar (el kernel solo sabe terminar) | media |
| `env` | `env` y `env NOMBRE` | `env VAR=valor orden`, `-i`, `-u` | `env FOO=1 orden` no ejecuta la orden | media |
| `date`, `mem`, `ntp`, `zona`... | ver `ayuda` | — | `mem` no tiene `-h`; su salida no es la de `free` | baja |
| `history` (interno) | lista | `-c`, `N`, `!!`, `!N`, Ctrl+R | sin búsqueda incremental | media |
| edición de línea | flechas, Tab, Inicio/Fin, Ctrl+A/E/U/L/C | Ctrl+R, Ctrl+K, Ctrl+W | — | media |
| `clear` (interno) | sí | — | — | — |

Comandos que **no existían** (comprobado con `type`): `head tail grep find sort uniq touch du df
less more which tree cut tee diff xargs yes seq basename dirname stat uptime free whoami
hostname uname`. Sí existía el interno `history`, además de `type`, `command`, `test`, `printf`,
`read`, `alias`...

Notas de la plataforma:
- El kernel no tiene señales (solo `matar`), ni `utimes` (no se puede actualizar la fecha de un
  archivo existente), ni permisos, ni usuarios, ni enlaces.
- El terminal (tela cheia y ventana) entiende solo `ESC [ D/C/K/J`: **sin colores SGR**, así que
  `ls` y `grep` no colorean (pendiente de una mejora del terminal).

## 2. Estado final (D097)

Todos aceptan `--help`/`--ayuda`. «Entrada» = leen la entrada estándar si no se da archivo; `-` también.

| Comando (alias) | Opciones | Notas |
|---|---|---|
| `ls` (listar) | `-1 -a -d -h -l -R -r -S -t`, varios argumentos | formato antiguo en el terminal; solo nombres en pipes |
| `cp` (copiar) | `-r -R -i -n -v -f`, varios orígenes | evita copiar una carpeta dentro de sí misma |
| `mv` (mover) | `-i -n -v -f`, varios orígenes | entre RAM y pendrive copia y borra |
| `rm` (apagar) | `-r -R -f -i -v -d` | rechaza `/`, `.`, `..`; en `/usb` pregunta (salvo `-f`) |
| `mkdir`, `rmdir` | `-p -v` | |
| `cat` (ver) | `-n -b -s -E -T`, varios archivos, `-` | |
| `wc` (conta) | `-l -w -c -m -L`, varios archivos con total | sin opciones, el formato clásico |
| `head` (cabeca), `tail` (cauda) | `-n N`, `-N`, `-c N`, `-q -v`; `tail -n +N`, `tail -f` | |
| `grep` (procurar) | `-i -v -n -c -l -L -r -E -F -w -x -o -H -h -q -s -m -e -A -B -C` | regex propia; binarios: «Archivo binario ... coincide» |
| `find` (achar) | `-name -iname -path -type -size -empty -maxdepth -mindepth !`, `-print -print0 -delete -exec ... ;` | |
| `sort` (ordenar) | `-r -n -u -f -b -c -k N[nrf] -t -o` | |
| `uniq` (unico) | `-c -d -u -i` | |
| `cut` (cortar) | `-f -d -s -c` con listas `1,3-5,7-` | |
| `tee` | `-a` | |
| `seq` (sequencia) | `-s -w`, enteros | |
| `touch` (tocar) | `-c` | actualiza la fecha reescribiendo el primer byte |
| `du` (espaco) | `-a -h -s -c -b -d N` | |
| `df` | `-h -k`, carpetas | añade la raíz (RAM) a los montajes |
| `less` (paginar), `more` | `-n LÍNEAS`; teclas espacio f b Intro j k g G / n q | funciona también tras un pipe |
| `which` (qual) | `-a` | busca en `PATH` |
| `tree` (arvore) | `-a -d -L N` | ASCII |
| `basename`, `dirname` | `-a -s` (basename) | |
| `stat` (estado), `uptime` (ligado), `free` (livre), `uname`, `hostname`, `whoami` (quem) | `uptime -p`; `free -b -k -m -h`; `uname -asnrvmo` | |
| `yes` (sim), `sleep` (dormir) | `sleep 0.5`, sufijos s m h | |
| `xargs` | `-n -I -0 -t` | no ejecuta si no llega nada |
| `diff` (diferenca) | `-u -q -i -b` | LCS, hasta 3000 líneas distintas |
| `env` | `-i -u`, `VAR=valor orden` | `env NOMBRE` sigue mostrando el valor |
| `ps` (processos) | `-e -A -a -f -l` (aceptadas), `-u -n -p PID -s -c`, filtro por nombre | sin resumen en pipes |
| `kill` (interno) | `-l`, `-SEÑAL`, `-s SEÑAL`, `-0`, `%trabajo` | todas las señales terminan el proceso |
| `history` (interno) | `N`, `-c` | Ctrl+R, Ctrl+K, Ctrl+W en la edición de línea |
| `apagar-pc` (desligar, interno) | | graba discos y pendrives y apaga por ACPI (S5); si no puede, «Ya puedes apagar el ordenador» (D113) |
| `acpi` (interno) | `desactivar`, `activar` | estado del apagado ACPI (valores S5) y del botón de encendido (D113) |

### Pendiente

- Colores en `ls` y `grep` (el terminal no entiende SGR).
- `grep`: `{n,m}`, referencias hacia atrás, separador `--` entre grupos de contexto, `--include`.
- `find`: `-o`/`-a`, paréntesis, `-mtime`, `-newer`, `-exec ... +`.
- `sort`: `-k` con fin de campo (`-k2,2`), `-M`, `-h`; `uniq -f/-s`; `cut --complement`.
- `less`: retroceso real de pantalla (no hay cursor direccionable) y tamaño de pantalla automático.
- `ls -l` con permisos y propietario (el sistema no los tiene); `touch -d/-t` (falta `utimes`).
- `echo`: más escapes (`\xHH`, `\0NNN`); `history` con `!!` y `!N`.
- Señales reales (`kill -STOP/-CONT`) requieren el kernel.

## 3. Comandos añadidos en la v7.4.2 (D232)

Todos con `--help` y entrada en `ayuda`. Los marcados con (PC) tienen la lógica en un `*_logica.h` probada en el PC (`tests/comandos_unidad.c`); `tests/h232_comandos_nuevos.py` compara en QEMU lo estándar con el mismo comando del PC.

| Comando | Opciones | Notas |
|---|---|---|
| `sed` (PC) | `-n -e -f -E -r -i[SUF]`; `s///` con `g i p N`, `d p q = a i c`; direcciones `N $ /re/ N,M N,/re/ !` | sin `\1` (el motor no tiene subexpresiones), sin `y` ni llaves |
| `tr` (PC) | `-c -d -s`; conjuntos `a-z`, `\n`, `[:alpha:]`... | |
| `nl`, `rev`, `tac`, `fold`, `expand`, `paste`, `comm`, `cmp`, `column`, `split` | las habituales (`nl -b -n -w -s -v -i`; `fold -s -w`; `paste -s -d`; `comm -123`; `cmp -s -l`; `column -t -s -o`; `split -l -b -d`) | `column` sin `-t` reparte por filas |
| `od` (PC), `xxd` (PC) | `od -A -t -b -c -d -o -x -C -j -N -w -v`; `xxd -l -s -p -r` | `hexdump` es alias de `od` (`hexdump -C`) |
| `base64` (PC), `md5sum`, `sha1sum` (PC) | `base64 -d -i -w`; `-c` en los resúmenes | |
| `file` (PC), `realpath`, `mktemp`, `truncate` | `file -b`; `realpath -e`; `mktemp -d -u -p`; `truncate -s [+-]N[KMG] -c` | no hay enlaces: `ln`/`readlink` no existen |
| `expr` (PC), `bc` (PC), `cal` (PC) | `bc -l -s -e` (punto fijo, 6 decimales); `cal [-s] [[MES] AÑO]` (semana desde el lunes) | |
| `id`, `nproc`, `lscpu`, `lspci`, `dmesg` | `lspci -n -v`; `dmesg -n N [texto]` | `lsusb` es alias de `usb` |
| `time`, `timeout`, `watch`, `top` | `timeout [-k] DURACIÓN` (124 al agotarse); `watch -n -t -c`; `top -d -n -b -o` | solo programas; los internos del shell no se pueden lanzar así |

Descartados: `ln`, `readlink` (sin enlaces en el VFS), `nohup` (sin SIGHUP), `join`, `unexpand`.

## 4. `htop` (D366)

Monitor de procesos interactivo, clon propio (no usa código de htop): barras de CPU por núcleo, memoria y swap, árbol, hilos, ordenar, buscar, filtrar, `nice`, matar y ratón. Todas las teclas, en `docs/htop.md`.

## 5. `ifconfig` con varias interfaces y `ifconfig route` (D445)

Desde la v8.5.5.3 hay varias interfaces de red a la vez (`eth0`, `eth1`, `wlan0`; ver `docs/red.md`).
`ifconfig` (alias `ip`) las enseña todas y cada orden puede llevar el nombre de la interfaz; sin él, va
a la **principal** (la de la ruta por defecto), como antes con una sola tarjeta.

| Orden | Qué hace |
|---|---|
| `ifconfig` | todas las interfaces; con más de una, la métrica de cada una y la línea «Ruta por defecto: eth0 por 10.0.2.2 (métrica 100)» |
| `ifconfig eth1` | solo esa interfaz |
| `ifconfig route`, `ip route` | la tabla de rutas: la red local de cada interfaz, las rutas por defecto con su métrica (`<- en uso` en la elegida, `(sin enlace)`, `(desactivada)`) y el DNS del sistema |
| `ifconfig eth1 dhcp` | pide dirección por DHCP en esa interfaz |
| `ifconfig eth1 10.0.3.5/24 gw 10.0.3.1 [dns X]` | dirección fija en esa interfaz (no se puede repetir la de otra) |
| `ifconfig eth1 dns 1.1.1.1` | el DNS de esa interfaz (el del sistema es el de la principal) |
| `ifconfig eth1 metric 50` | prioridad de su ruta por defecto (0-9999; menor = preferida; cable 100, Wi-Fi 600) |
| `ifconfig eth1 down` / `up` | la desactiva o la activa; la ruta por defecto pasa a otra |
| `ifconfig -a` | además, las conexiones y la caché ARP (con la interfaz de cada entrada si hay varias) |

`ifconfig eth0 ...` con una sola interfaz se aplica a esa aunque se llame de otra forma (la `wlan0`
sola), como antes. En Configuración, **Red > Interfaces** lista todas y permite renovar el DHCP,
activar/desactivar y cambiar la prioridad («Preferir esta», «-», «+»).

## 6. `mount` y `montar` con XFS (D446)

- `mount [-t xfs] <disp> <carpeta>`: un XFS de Linux (v4 o v5) se reconoce solo (`auto`) y se monta **siempre en solo
  lectura**; con `-o rw` se monta igual en solo lectura y se avisa. `mount` sin argumentos lo lista como
  `tipo xfs (solo lectura, N MB libres de M MB)`.
- Un XFS que no se puede leer (funciones incompatibles desconocidas, `needsrepair`...) no se monta: `mount` dice el
  motivo (`mount: hdb4: XFS: funciones desconocidas 0x80000000`), y `discos` e `infopc` lo muestran como formato.
- Con el diario sucio (apagado incorrecto) se monta igual, con un aviso: lo último escrito puede no verse.
- Escribir, crear, borrar o renombrar dentro da «solo lectura» (`ERRO_SO_LEITURA`, el EROFS de SaguiOS).
- `montar` (la forma antigua, en `/usb`) dice «Pendrive montado en /usb (xfs, solo lectura)».
- Los enlaces simbólicos con destino relativo se siguen; uno con destino absoluto o roto se lee como un fichero con el
  destino de texto. Detalles en `docs/xfs.md`.
