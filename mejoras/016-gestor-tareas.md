# 016 - Administrador de tareas gráfico

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

Solo existían `ps` y `kill` en el shell: no se veía cuánto CPU gastaba cada programa, ni el
uso total de CPU y memoria a lo largo del tiempo, y no había forma gráfica de cerrar un
programa colgado o una ventana.

## Propuesta (aplicada)

Programa `/bin/tareas` (`userland/programas/tareas.c`) con pestañas Procesos, Rendimiento y
Ventanas; icono "Tareas" en el escritorio y atajo global Ctrl+Mayús+Esc
(`kernel/gui/janelas.c`). En el kernel: campo `janelas` al final de `info_processo_t`,
`INFO_JANELAS` (35) y `SIS_PEDIR_FECHAR` (75). Corrección del terminal virtual: un programa
en segundo plano que escribía en el Terminal cerraba el shell. Detalle en
`docs/decisoes.md`, D043.

## Criterios de aceptación

- [x] Tabla de procesos con PID, nombre, padre, estado, % CPU, tiempo de CPU, memoria y ventana
- [x] Refresco cada segundo, con pausa; ordenar al hacer clic en la cabecera
- [x] "Finalizar proceso" con confirmación; no deja finalizar el ocioso ni tareas del kernel;
      avisa en sagsh, Terminal y el propio administrador
- [x] Gráficas de CPU y memoria de los últimos 60 s, procesos y tiempo encendido
- [x] Lista de ventanas y "Cerrar ventana" (como el botón X)
- [x] `tareas -t`: instantánea en modo texto
- [x] Icono en el escritorio y Ctrl+Mayús+Esc
- [x] Test automático `tests/h41_tareas.py`
- [x] Decisión registrada en `docs/decisoes.md` (D043)
- [ ] Desplazamiento con la rueda del ratón (el kernel aún no entrega eventos de rueda)
- [ ] Prioridades / "cambiar prioridad" (el planificador es round-robin sin prioridades)

## Notas

- La resolución del % de CPU es la del reloj (10 ms): con muestras de 1 s, ±1 %.
- La memoria por proceso es la de sus páginas de usuario más la pila del kernel; las
  ventanas (memoria del gestor) no se cuentan en el proceso.
- `tareas -t` va en segundo plano (el shell trata `tareas` como programa gráfico), así que
  su salida aparece tras el prompt.
