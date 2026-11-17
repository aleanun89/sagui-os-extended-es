# 137 - Jabuti: programar con bloques para niños

- **Estado:** hecha (v8.5.4, D377)
- **Prioridad:** media
- **Alta:** 2026-11-17

## Problema / motivación

SaguiOS no tenía nada para enseñar a programar a los niños. El mantenedor pidió «una app sencilla inspirada en Scratch para enseñar fundamentos de programación a niños en mi SO». Scratch Desktop es Electron y su gato, su nombre y sus gráficos son marca de la Fundación Scratch, así que no se porta: se hace una aplicación original.

## Propuesta

`jabuti`: paleta de bloques por categorías con colores, área de programa con arrastrar y soltar y encaje por muescas, escenario con personajes que corren a la vez (intérprete propio de hilos verdes a 30 cuadros por segundo), editor de trajes y fondos en pixel art, biblioteca de personajes propia, importar imágenes, guardar proyectos como ZIP con texto legible, siete ejemplos, guía para maestros y familias. Todo en enteros (sin FPU) para ir bien en equipos antiguos (Pentium II/III, 128-256 MB, 800x600). Archivos: `userland/jabuti/`, `userland/programas/jabuti.c`, `userland/programas/jabuti_ops.h`, `rootfs/ejemplos/jabuti/`, `rootfs/ayuda/jabuti.md`, `docs/jabuti.md`.

## Criterios de aceptación

- [x] Paleta, área de programa y escenario; encaje por muescas; bloques en «C»; huecos de número, texto, condición y lista
- [x] Varios personajes con su programa y ejecución concurrente
- [x] Editor de trajes y fondos, biblioteca propia e importar PNG
- [x] Proyectos `.jbt` (ZIP + texto), ejemplos, asociación en Archivos, menú Inicio (Educación), ayuda y catálogos en/pt_BR
- [x] Test automático `tests/h379_jabuti.py`
- [x] Decisión D377 en `docs/decisoes.md`

## Notas

Pendiente: fuentes del sistema (D356) en los bloques, clones, bloques propios, monitor de listas en el escenario, exportar a una página web.
