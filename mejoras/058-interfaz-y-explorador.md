# 058 - Mejoras de interfaz y del explorador de archivos

- **Estado:** propuesta (v6); **el explorador de archivos está hecho** (D136, rama
  `v6.3-arquivos`: `userland/programas/arquivos.c` y `tests/h104_arquivos_pro.py`)
- **Prioridad:** media-alta
- **Alta:** 2026-11-07
- **Relacionadas:** 042 (componentes de la libsagui: tabla, árbol, pestañas, barra de progreso),
  047 (JPEG y temas), 023 (menú contextual), 031 (barra de tareas), 056 (compresor),
  Archivos (`userland/programas/arquivos.c`)

## Problema / motivación

Archivos es sencillo: una lista con iconos, doble clic y un menú contextual básico. Faltan
cosas que se dan por hechas en cualquier explorador (por ejemplo, **ver u ocultar los archivos
ocultos**), y la presentación del escritorio se puede pulir bastante.

## Propuesta: explorador de archivos

| Área | Contenido |
| --- | --- |
| **Ocultos** | **Ver / no ver ocultos** (Ctrl+H y menú Ver): ocultos = nombres que empiezan por `.` y archivos con el atributo *oculto* de FAT32. Por defecto, no se ven. La preferencia se recuerda (`/home/.escritorio/`). `ls -a` en el terminal hace lo mismo (D097) |
| **Vistas** | iconos grandes, lista, **detalles** (tabla con nombre, tamaño, tipo y fecha, ordenable por columna) y **miniaturas** de imágenes (BMP/PNG/JPEG, con caché) |
| **Navegación** | atrás, adelante y arriba; **barra de ruta** pulsable (`/` > `home` > `fotos`) y editable; **panel lateral** con lugares: `/`, `/home`, `/tmp`, pendrives (`/usb…`), montajes y favoritos |
| **Selección** | múltiple con Ctrl/Mayús y recuadro con el ratón; seleccionar todo |
| **Operaciones** | copiar, cortar y pegar archivos (portapapeles de archivos); arrastrar y soltar dentro de Archivos y hacia otra carpeta; renombrar en el sitio (F2); nueva carpeta y nuevo archivo; borrar con confirmación; **papelera** (`/home/.papelera`) con restaurar y vaciar, opcional; copias grandes con barra de progreso y cancelar |
| **Buscar** | por nombre (comodines) en la carpeta y subcarpetas, y filtro rápido al teclear |
| **Abrir con** | menú «Abrir con» (Editor, Mico, Visor, Paint, `reproducir`, Markdown…) y asociaciones por tipo |
| **Propiedades** | tamaño (también de carpetas), fechas, atributos (oculto, solo lectura), sistema de archivos y espacio libre del disco |
| **Comprimir y extraer** | entradas de la 056 |
| **Teclado** | todo manejable con teclado: flechas, Intro, Retroceso, Supr, F2, Ctrl+C/X/V/A, Ctrl+L para la ruta |

## Propuesta: interfaz y presentación

| Área | Contenido |
| --- | --- |
| **Coherencia** | todos los programas con los componentes comunes de la 042 y los temas de la 047: mismos botones, tablas, barras de desplazamiento y colores |
| **Ventanas** | **ajustar a media pantalla** arrastrando al borde (izquierda o derecha) y maximizar arrastrando arriba; **Alt+Tab** con miniaturas o iconos; doble clic en el título para maximizar (si falta); sombras suaves y bordes redondeados opcionales |
| **Barra y notificaciones** | **centro de notificaciones** (historial de avisos); bandeja con red, volumen y temperatura (050) **[hecho en la v5.2, D114, h85]**; reloj con calendario desplegable |
| **Escritorio** | iconos colocables, alineados a una rejilla, y recordados; selección múltiple en el escritorio |
| **Letra** | escala de la interfaz 1x/2x para pantallas grandes (p. ej. 1920x1080) y, si es barato, una fuente proporcional para los títulos |
| **Animaciones** | sutiles (abrir y minimizar), desactivables y apagadas en PC lentos o con «movimiento reducido» |
| **Accesibilidad** | tema de alto contraste (047), tamaños de cursor y foco del teclado visible |

Todo se mide en un PC lento: las animaciones y miniaturas no pueden hacer el escritorio más
torpe en un Pentium III (se desactivan solas si hace falta).

## Atajos de teclado (v6.0 beta)

Atajos básicos del escritorio, en la **v6** junto con el resto de la mejora (decisión del
mantenedor, 2026-11-08). No dependen de la 042 ni de la 047, así que pueden ir primero:
- **Alt+F4**: cierra la ventana activa. El programa recibe el mismo aviso que al pulsar la X,
  así que puede preguntar «¿Guardar cambios?». Sin ventana activa, no hace nada (el apagado
  seguirá en el menú).
- **Alt+Tab**: cambia a la siguiente ventana (Alt+Mayús+Tab, a la anterior). Primero con icono
  y título; después, con miniaturas.
- **Tecla Windows**: abre o cierra el menú. **Windows+D**: minimiza todo y muestra el escritorio.
- **Ctrl+Alt+Supr**: abre el Administrador de tareas.
- El gestor de ventanas del kernel atrapa estos atajos antes de entregar la tecla al programa.
- Test: abrir dos ventanas, Alt+Tab cambia el foco, Alt+F4 cierra la activa (y Editor con
  cambios pregunta), Windows abre el menú.
- **Hecho** en la rama `atajos-escritorio` (D119): `kernel/gui/atalhos.c`, sin syscalls nuevas;
  la tecla Windows abre el menú al soltarla (si no se pulsó otra); AltGr no cuenta como Alt.
  Ayuda: `ajuda atajos` y `/ayuda/atajos.md`. Prueba: `tests/h90_atajos.py`. Pendiente para la
  v6.1: miniaturas en el panel de Alt+Tab.

## Pruebas

- **Archivos:**
  - ocultos sí y no, con `.punto` y atributo FAT;
  - vistas, orden, barra de ruta y panel lateral;
  - copiar, cortar y pegar, y arrastrar;
  - renombrar en el sitio, papelera, buscar, «Abrir con» y propiedades.
  Todo con capturas y comprobación de los archivos en el disco.
  - **Hecho** en la rama `v6.3-arquivos` (D136): `tests/h104_arquivos_pro.py`
    (ocultos y preferencia, las tres vistas y el orden por columna, carpeta
    nueva, renombrar, apagar con confirmação, copiar y colar, buscar con
    curingas).
- **Ventanas:** ajuste a media pantalla, Alt+Tab y centro de notificaciones.
- Las pruebas existentes de Archivos, del menú contextual y de la barra (h28, h55, h60…),
  actualizadas con criterio.
- La batería completa debe pasar.

## Criterios de aceptación

- [x] Ver u ocultar ocultos (punto y atributo FAT), recordado entre sesiones
  — **hecho** (D136): Ctrl+H y el botón «Ocultos»; por defecto no se ven; la
  preferencia se guarda en `/home/.config/arquivos` (o `/etc/arquivos.conf` si
  no hay `/home`). El atributo de oculto del FAT32 no se puede mirar desde un
  programa (el `listar` del kernel ni siquiera lo lista), así que sólo cuenta
  el punto.
- [x] Vistas de detalles y miniaturas, barra de ruta, panel lateral, selección
  múltiple — **hecho** (D136) menos las miniaturas: las tres vistas (lista,
  detalles ordenable por columna e iconos), la barra de ruta con las migas de
  pan pulsables y editable (Ctrl+L), el panel lateral con los lugares y las
  acciones, y la selección múltiple con Ctrl, Mayús, Ctrl+A y recuadro.
- [x] Copiar, cortar y pegar, arrastrar, renombrar en el sitio, buscar, «Abrir
  con» y propiedades — **hecho** (D136) menos el arrastrar y soltar: hay
  portapapeles de archivos propio (copia y corte, con carpetas), renombrar con
  F2, buscar con comodines (filtro rápido y búsqueda en subcarpetas), «Abrir
  con…» y propiedades con el tamaño real de las carpetas.
- [ ] Ajuste de ventanas, Alt+Tab y centro de notificaciones (es del gestor de
  ventanas del kernel, no del explorador)
- [ ] Coherencia visual con los componentes (042) y los temas (047)
- [x] Test automático en `tests/` — `tests/h104_arquivos_pro.py` (D136)
- [x] Decisión registrada en `docs/decisoes.md` — D136

## Lo que falta de la parte del explorador

- **Papelera** (`/home/.papelera`) con restaurar y vaciar: es opcional y depende
  de que haya un disco montado en `/home`; la imagen normal no lo tiene.
- **Arrastrar y soltar**: el kernel no tiene protocolo de «soltar» entre
  ventanas.
- **Miniaturas** de imágenes: pedirían una caché en memoria que en un Pentium
  III no compensa.
- **Copias grandes con barra de progreso y cancelar**: la copia es síncrona y
  en el mismo proceso.

## Notas

- Depende de la 042 (componentes) y de la 047 (JPEG y temas); en la v6 conviene hacer esas dos
  primero.
- Modelo sugerido: **Sonnet medio** para Archivos y la mayor parte de la interfaz; las partes del
  gestor de ventanas del kernel (ajuste, Alt+Tab) con cuidado, y si se complican, con Opus.
