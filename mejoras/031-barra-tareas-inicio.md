# 031 - Barra de tareas, menú Inicio y submenús

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

La barra superior solo tenía el nombre, el lema y el reloj; las ventanas minimizadas
aparecían como botones sueltos en ella, y para abrir un programa había que buscar su icono
o usar el Terminal. Cambiar la resolución (mejora 026) pedía el comando `pantalla`. Quien
viene de Windows o de un escritorio Linux espera un botón Inicio, un botón por ventana abierta
y llegar a la configuración de la pantalla con el botón derecho en el escritorio.

## Propuesta (aplicada)

- **Submenús en el menú contextual** (`kernel/gui/menu.c`, D064): `>Texto` abre un submenú
  con las líneas siguientes hasta `<`; `*Texto` sale marcada. Se abren al pasar el ratón, con
  clic, con la flecha derecha o Enter; la izquierda y Esc cierran solo el último. Hasta 4
  paneles. Los programas los usan con la misma `SIS_MENU_CONTEXTUAL` (`MENU_MAX_ITENS` pasa
  de 20 a 64). Dentro del kernel, además, iconos de 16x16 por línea y `menu_lista_t`.
- **Barra de tareas** (`kernel/gui/barra.c`, nuevo, D065), en la misma barra de 24 píxeles:
  botón Inicio con el sagüí, un botón por ventana (foco resaltado, minimizadas apagadas, clic
  para traer delante o minimizar, botón derecho con el menú de la ventana, «»» cuando no
  caben), lema que se acorta o desaparece y reloj (clic: fecha completa). Sustituye a
  `desenhar_minimizadas`. Los diálogos (`JAN_DIALOGO`, nuevo estilo) no salen en ella.
- **Menú Inicio:** más usados, grupos en submenús, «Ejecutar...» y «Reiniciar...» (programa
  `ejecutar`, con los diálogos de la libsagui), «Configuración de pantalla», «Acerca de
  SaguiOS» y «Apagar» desactivada (sin ACPI). Tecla Windows (`TECLA_SUPER`, E0 5B/5C) y
  Ctrl+Esc.
- **Escritorio** (D066): «Configuración de pantalla...» (`pantalla --ventana`) y submenú
  «Resolución» con los modos en caliente y el actual marcado. La ventana Pantalla muestra en
  líneas propias por qué no hay write-combining.

## Criterios de aceptación

- [x] Botón Inicio con icono propio y menú colgando de la barra, con grupos y más usados
- [x] Submenús en `menu.c` (ratón y teclado), compatibles con el formato anterior y documentados
- [x] Tecla Windows y Ctrl+Esc abren y cierran el menú Inicio
- [x] Un botón por ventana: foco resaltado, minimizadas con otro estilo, clic alterna, botón derecho con el menú de la ventana
- [x] Botones que se estrechan y «»» con las que no caben; diálogos fuera de la barra
- [x] Reloj a la derecha (clic: fecha), lema acortado u oculto
- [x] Funciona en 640x480, 800x600, 1024x768 y 1280x1024 y se recoloca al cambiar de modo en caliente; oculta en pantalla completa
- [x] «Configuración de pantalla...» y submenú «Resolución» en el menú del escritorio
- [x] Test automático `tests/h60_barra_tareas.py` (capturas `docs/capturas/H60-*.png`);
      `h42_ventanas.py` y `h55_menu_contextual.py` adaptados
- [x] Decisiones registradas en `docs/decisoes.md` (D064, D065, D066)
- [x] **Bandeja** (v5.2, D114): volumen, red y temperatura a la izquierda del reloj, con ayuda al pasar el ratón y paneles al hacer clic; prueba `tests/h85_bandeja.py`

## Notas

- «Apagar» espera a la mejora 030 (ACPI); hasta entonces sale desactivada con «(no disponible)».
- Los «más usados» se cuentan en memoria y se pierden al reiniciar: no hay dónde guardar
  preferencias de la sesión gráfica.
- «Ejecutar...» lanza programas de `/bin` sin shell: los comandos de texto (`ls`...) correrían
  sin terminal. Para ellos está el Terminal.
- Sin hacer: tecla «Menú»/Mayús+F10, Windows+R para «Ejecutar», vista previa de la ventana al
  pasar el ratón por su botón, arrastrar botones para reordenarlos, fijar programas en la barra.
- Las capturas antiguas que salen con la barra (H20, H28, H42...) no se regeneraron: muestran
  la barra de antes.
