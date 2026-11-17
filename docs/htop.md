# htop: monitor de procesos del terminal (D366)

`htop` es un clon propio (sin código de htop) para el Terminal, la consola por SSH y cualquier terminal xterm. Redibuja solo las
líneas que cambian y se actualiza cada 1,5 s por defecto.

```
htop [-d DÉCIMAS] [-u USUARIO] [-t] [-C] [-s COLUMNA]
```

`-d 30` actualiza cada 3 s; `-u sagui` solo ese usuario; `-t` empieza en vista de árbol; `-C` sin colores;
`-s cpu|mem|pid|tiempo|usuario|ni|res|orden` columna de orden inicial. Sin terminal (tubería) avisa y sale: para listas
en texto están `ps` y `top -b`.

## Pantalla

- **Cabecera:** una barra por CPU (verde = procesos de usuario, rojo = procesos del kernel; el reparto es el de los
  tiempos de CPU de los procesos, el kernel no separa usuario y sistema por núcleo), memoria (verde usada, amarillo caché
  de disco), swap comprimida en RAM (D352), `Tareas: N, M hilos; K en ejecución` y el tiempo encendido.
  Con más de 2 CPU se reparten en dos columnas (cuatro con más de 16 y 100 columnas).
- **Lista:** `PID USUARIO PRI NI RES S CPU% MEM% TIEMPO+ ORDEN`. `PRI` es 20 + nice. `RES` es la memoria del proceso
  (el kernel no da la virtual). `S`: `R` listo o ejecutando, `S` bloqueado, `Z` zombi (hasta que su padre lo recoge).
  Los procesos del kernel salen en azul, los hilos en verde, los marcados en amarillo; `CPU%` >= 50 en rojo.
- **Pie:** la barra de teclas F1-F10 (también con el ratón) o el último aviso.

## Teclas

| Tecla | Acción |
|---|---|
| Flechas, Re/Av Pág, Inicio, Fin | moverse (la rueda del ratón, de 3 en 3) |
| F1, `h`, `?` | ayuda |
| F2, `S` | configuración: columnas visibles, intervalo, colores, ocultar procesos del kernel, árbol y mostrar hilos por defecto |
| F3, `/` | buscar por nombre (se va seleccionando al teclear; F3 otra vez salta a la siguiente) |
| F4, `\` | filtrar por nombre (Esc lo quita) |
| F5, `t` | vista de árbol (padres e hijos, con sus hilos) |
| F6, `>`, `.` | elegir la columna de orden; repetirla invierte |
| `P` `M` `T` `N` | ordenar por CPU, memoria, tiempo o PID; `I` invierte |
| `H` | mostrar u ocultar los hilos de cada proceso |
| `K` | ocultar los procesos del kernel |
| F7 / `]` | nice - (más prioridad; el kernel solo deja bajarlo al administrador) |
| F8 / `[` | nice + (menos prioridad) |
| F9, `k` | matar el proceso seleccionado o los marcados (pide confirmar: SaguiOS no tiene señales, solo terminar) |
| `u` | mostrar solo los procesos de un usuario |
| Espacio / `U` | marcar el proceso (y bajar) / desmarcar todos; F7, F8 y F9 actúan sobre los marcados |
| `F` | seguir al proceso seleccionado aunque cambie de sitio al reordenar |
| Ctrl+L | redibujar |
| F10, `q` | salir |

Con el ratón: clic en un título de columna ordena por ella, clic en una línea la selecciona, clic en la barra de abajo
equivale a la tecla F, la rueda mueve la selección.

## Configuración

Se guarda en `~/.config/htoprc` (la carpeta personal de la cuenta; `/home/.config/htoprc` para sagui) al salir o al cerrar
F2, solo si algo cambió: `columnas`, `intervalo` (décimas), `orden`, `inverso`, `arbol`, `hilos`, `sin_kernel` y `mono`.
Sin partición de configuración ni `/home` escribible, htop sigue funcionando sin guardar nada.

## Cómo funciona

- Datos: `INFO_PROCESSOS` (procesos), `INFO_HILOS` (hilos, nuevo en esta versión, número 81 de `info_sistema`),
  `PLAN_INFO` (nice), `USU_DE_PID` (usuario), `INFO_NUCLEOS` (tiques por CPU), `INFO_MEMORIA` (memoria, caché, swap).
  `CPU%` es la parte del intervalo que el proceso estuvo ejecutando (100% = una CPU).
- Teclas y ratón: al arrancar, htop pone el Terminal en modo xterm (`CSI ? 7700 h`, el mismo que usa `ssh`) para recibir
  F1-F10 y activa el ratón con `CSI ? 1000 h` y `CSI ? 1006 h` (formato SGR). Al salir lo deja como estaba (y por SSH no toca
  el modo xterm, que es del cliente). Con la consola serie (sin `TERM`) se usan las teclas del modo bruto de SaguiOS:
  flechas, `q` y las letras; no hay F-teclas ni ratón.
- Tamaño: el Terminal contesta a `CSI 18 t`; por SSH se usan `LINES` y `COLUMNS` de la sesión.
- Pantalla alternativa (`CSI ? 1049`): al salir vuelve lo que había antes.

## Ratón en el Terminal (cambio de esta versión)

El Terminal reenvía clics y rueda a los programas que los piden con `CSI ? 1000 h` / `1002 h` y `1006 h` (SGR): clic y rueda
(`ESC [ < b ; x ; y M/m`), sin arrastre salvo con `1002`. Mayús+clic sigue seleccionando texto y la barra de pestañas sigue
siendo del Terminal. Solo vale en modo xterm (`CSI ? 7700 h`). Los demás modos de ratón se aceptaban y se ignoraban; ahora este
sí actúa. Lo probado: `tests/h368_htop.py`.

## Pruebas

`python3 tests/h368_htop.py` (Terminal del escritorio en QEMU: cabecera, lista, ordenar, ratón, árbol, buscar, filtrar,
nice, matar, ayuda, configuración y salida; capturas `docs/capturas/H368-*.png`).
