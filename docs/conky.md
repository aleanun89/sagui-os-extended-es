# Conky de SaguiOS

`conky` es un widget de escritorio (D365, v8.5.3) que lee **`/home/.config/conky.conf`** y pinta en una ventana sin
marco, bajo las ventanas normales, los datos del sistema que pida el texto. Es un clon propio: solo imita el formato del
fichero del Conky clásico (`.conkyrc`: opciones, una línea `TEXT` y el texto con `${variables}`), no usa su código.

Se activa desde **Añadir widgets...** (menú del escritorio) o **Configuración > Escritorio > Widgets de escritorio...**;
al activarlo arranca solo al iniciar la sesión. La primera vez crea un `conky.conf` de ejemplo. **Al guardar el fichero
el widget se recarga solo** (en unos 5 s). Se mueve con Alt + arrastrar (o «Editar widgets» en el menú del escritorio).

```
# conky.conf
update_interval 1.0
alignment top_left
gap_x 20
gap_y 40
minimum_size 300 0
own_window_transparent no
default_color E8F2EA
color1 5CD08A
color2 E8C65A
color3 5AA6E8

TEXT
${color1}${nodename}${color}${alignr}SaguiOS ${kernel}
${color3}${hr 1}${color}
${color2}Tiempo en marcha${color}${alignr}${uptime}
${color2}CPU${color}  ${cpu}%${alignr}${cpugraph 18,150 5CD08A}
${color2}Memoria${color}  ${mem} / ${memmax}${alignr}${memperc}%
${membar 8}
${color2}Disco${color}  ${fs_used /} / ${fs_size /}
${fs_bar 8 /}
${color2}Bajada${color}  ${downspeed eth0}/s${alignr}${downspeedgraph eth0 18,110 5AA6E8}
${top name 1}${alignr}${top cpu 1}%
${alignc}${time %H:%M:%S}
```

## Opciones de la cabecera

| Opción | Efecto |
|---|---|
| `update_interval S` | segundos entre lecturas (mínimo 0,25; `1.0` por defecto) |
| `alignment` | `top_left`, `top_right`, `top_middle`, `bottom_*`, `middle_left`, `middle_right`, `middle_middle` (y `tl`, `tr`, `bl`, `br`...) |
| `gap_x`, `gap_y` | separación del borde de la pantalla |
| `minimum_size W [H]`, `maximum_width W` | tamaño de la ventana (se ajusta al texto; no pasa de 900x640) |
| `own_window_transparent yes` | fondo transparente (solo se ve el texto sobre el fondo del escritorio) |
| `own_window_argb_value N` | opacidad del panel (0-255) |
| `default_color`, `color0`...`color9` | colores: `RRGGBB`, `#RRGGBB` o un nombre (`white`, `red`, `green`, `blue`, `yellow`, `orange`, `grey`, `cyan`...) |
| `default_bar_size W H`, `default_graph_size W H` | alto de barras y gráficas (el ancho es el que quede de la línea) |
| `draw_borders yes` | filete de acento dentro del panel |
| `use_xft`, `font`, `own_window*`, `double_buffer`, `border_*`, `draw_shades`... | se aceptan y se **ignoran** (la letra es la de 8x16 de SaguiOS) |

Una opción desconocida se ignora y se anota una vez en el registro (`[CONKY] opción no soportada: x`).

## Variables

| Variable | Muestra |
|---|---|
| `${cpu}` · `${cpubar [h]}` · `${cpugraph [h,w] [color]}` | uso total de CPU en % · barra · gráfica |
| `${cpu_count}` | número de CPU |
| `${mem}` · `${memmax}` · `${memperc}` · `${memeasyfree}` | memoria usada, total, % y libre (`1.2GiB`) |
| `${membar [h]}` · `${memgraph [h,w]}` | barra y gráfica de memoria |
| `${swap}` · `${swapmax}` · `${swapperc}` · `${swapbar}` | la swap comprimida en RAM |
| `${fs_used /}` · `${fs_size /}` · `${fs_free /}` | disco del punto de montaje (`/` es la RAM del sistema) |
| `${fs_used_perc /}` · `${fs_free_perc /}` · `${fs_bar [h] /}` | porcentajes y barra |
| `${downspeed eth0}` · `${upspeed eth0}` | bajada y subida de la eth0 (`3.4KiB`; `lo` da 0) |
| `${downspeedgraph eth0 [h,w] [color]}` · `${upspeedgraph ...}` | gráficas de red |
| `${uptime}` · `${uptime_short}` | tiempo encendido (`1d 2h 3m 4s`) |
| `${time %H:%M}` | la hora local con un formato de `data` (`%Y %m %d %H %M %S %A %B %Z`...) |
| `${nodename}` · `${hostname}` · `${kernel}` · `${sysname}` · `${machine}` | nombre del equipo, versión de SaguiOS, `SaguiOS`, `i686` |
| `${top name N}` · `${top cpu N}` · `${top mem N}` · `${top pid N}` · `${top mem_res N}` | el proceso N-ésimo por CPU |
| `${top_mem name N}` ... | lo mismo ordenado por memoria |
| `${processes}` · `${running_processes}` | procesos en total y ejecutándose |
| `${acpitemp}` · `${hwmon ...}` | temperatura (°C) de la primera sonda del monitor de hardware |
| `${battery_percent}` · `${battery_bar [h]}` | batería (`n/d` si no hay) |
| `${color [nombre\|#rrggbb\|N]}` · `${color0}`...`${color9}` | cambia el color del texto que sigue (`${color}` vuelve al de por defecto) |
| `${alignr [n]}` · `${alignc}` | lo que sigue en la línea va a la derecha o al centro |
| `${hr [alto]}` · `${stippled_hr}` | línea horizontal |
| `${voffset N}` · `${offset N}` · `${goto N}` | desplazamientos |
| `${font ...}` · `${tab}` | se ignoran |
| `$$` | un `$` |

Lo que no está en la tabla (`${exec}`, `${execi}`, `${image}`, `${loadavg}`, `${wireless_*}`...) se **muestra tal cual** y se
anota una vez en el registro (`[CONKY] variable no soportada: x`); no rompe nada. Tampoco hay `exec`: lanzar programas desde
un fichero de texto del escritorio sería un riesgo.

## Consumo

Una lectura del sistema por ciclo y solo de lo que el texto usa; si el texto y los valores no cambiaron respecto al ciclo
anterior, no se repinta. Con `update_interval 2` o más baja aún.
