# 077 - Terminal con pestañas (varias instancias) y más opciones

- **Estado:** propuesta; reservada para una release posterior (v6.4)
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** 058 (explorador de archivos), 042 (componentes da libsagui),
  074 (servicios y `cron`), 003 (`cc`)

## Problema / motivación

El terminal del SaguiOS abre una sesión de comandos por ventana. Faltan dos
cosas que quien usa un terminal a diario echa de menos:

1. **Varias instancias a la vez.** Quien trabaja con el terminal (compilar,
   mirar un log, entrar por red, dejar un `tail` corriendo) acaba abriendo
   muchas ventanas, y las ventanas del SaguiOS no tienen forma de agruparse. Un
   terminal con **pestañas** (una por sesión) permite tener cuatro o cinco
   sesiones vivas en una sola ventana, cada una con su directorio y su
   historial.
2. **Un terminal más bonito y con más opciones.** Ahora mismo es un área de
   texto monoespaciada sobre el fondo del escritorio: sin colores, sin barra de
   estado y con pocas ayudas.

## Propuesta

El programa `terminal` (el de ahora), con las pestañas y el aspecto mejorados:

| Pieza | Qué hace |
| --- | --- |
| **Pestañas** | Una barra de pestañas encima del área de texto: una por sesión. Botón `+` para abrir otra, `×` para cerrar la de al lado, y arrastrar para reordenar. Cada sesión tiene su directorio, su historial y su salida. Cambiar de pestaña es `Ctrl+Tab` y `Ctrl+Shift+Tab` (y `Alt+1`..`Alt+9` para ir a una concreta) |
| **Aspecto** | Colores: la salida del shell puede traer color ANSI (rojo y verde para los errores, azul para las rutas), fondo propio del terminal, y un **tema** que se pueda cambiar desde Configuración |
| **Barra de estado** | Directorio actual, número de comandos de la sesión, y un indicador de si la sesión está ocupada |
| **Más opciones** | **Búsqueda en el scrollback** (`Ctrl+F`), **copiar y pegar** con el ratón, **selección** con arrastre, **scroll** con la rueda, y un **registro de la sesión** que se pueda guardar a fichero |

El motor de la terminal (el intérprete de comandos) no cambia: lo que se toca es
el programa de ventana y el modo de tener más de una sesión viva.

## Criterios de aceptación

- [ ] Abrir dos pestañas y que cada una tenga su directorio y su historial
- [ ] `Ctrl+Tab` alterna entre pestañas; `+` abre una nueva; `×` cierra
- [ ] Salida con colores ANSI y barra de estado con el directorio
- [ ] Buscar en el scrollback y copiar un texto seleccionado
- [ ] Test automático en `tests/` que lo comprueba
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Lo decide el usuario: entra en una **release posterior** (la v6.4), no en la
  entrega del visor de PDF ni en la del reproductor de MPEG-1.
- La terminal actual vive en `userland/programas/terminal.c` y el intérprete en
  el `sagsh`: conviene que las sesiones compartan el intérprete y no que cada
  pestaña tenga el suyo (la pila del programa es de 64 KB).
- El historial de cada pestaña tiene que poder guardarse aparte; el historial
  global del `sagsh` no debe mezclarse.
