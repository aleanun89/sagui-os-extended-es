# 084 - El navegador: abrir un enlace en otra pestaña y recordar la posición de lectura

- **Estado:** en curso
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** 063 y 081 (el navegador y su motor), D148 (las mejoras del navegador de la v6.3), D152 (esta mejora)

## Problema / motivación

Al Cipó (D148) le faltaban dos cosas de todos los navegadores:

1. **Un enlace siempre se abría en la pestaña actual.** En cualquier navegador,
   el Ctrl+clic (o el menú del botón derecho) abre el enlace en una pestaña
   nueva, para poder ir a otra página y volver a la primera sin perderla. En el
   Cipó no había manera: el enlace navegaba la pestaña actual y la página
   anterior solo se recuperaba con «atrás».
2. **La posición de lectura solo se guardaba dentro del historial.** El array
   `historico_pos[]` guardaba dónde se estaba leyendo *dentro del historial de
   navegación* (voltar/avancar), pero al volver a un sitio por otra vía
   (navegando otra vez a la misma dirección, recargando, o abriendo el enlace
   otra vez) la página salía desde arriba. En una página larga —un artículo,
   una lista de noticias— hay que volver a bajar hasta el párrafo que se
   estaba leyendo.

## Propuesta

Solo toca al programa del navegador, `userland/programas/cipo.c`: ni el motor
de la web ni el kernel cambian.

### 1. El enlace en otra pestaña

- **Ctrl+clic**: al hacer clic izquierdo sobre un enlace con el Ctrl apretado
  (`e.modificadores & 2`), el enlace se abre en una pestaña nueva. La pestaña
  actual se queda como estaba (con su página en memoria) y la nueva queda
  **activa**, con la página del enlace ya cargada. Un clic normal sigue
  navegando en la pestaña actual, igual que antes.
- **El menú del botón derecho** gana una línea más, **«Abrir en pestaña
  nueva»**, que hace lo mismo. Las líneas que ya había («Guardar imagen» y
  «Guardar enlace») siguen donde estaban y hacen lo mismo.

El menú del botón derecho tenía el código `63`; ahora tiene el suyo propio,
el **`65`** (el `64` es el de los marcadores), y **cada línea del menú guarda
su acción**, para que el `EV_MENU` sepa qué hacer con la línea elegida.

Para abrir la pestaña, `nova_aba()` se parte en dos: `nova_aba_com_url(url)`
abre una pestaña con la página indicada y `nova_aba()` (Ctrl+T y el botón «+»)
sigue abrindo la página de inicio.

### 2. La posición de leitura de cada sitio

La posición (en píxeles de la página) se guarda **por dirección**, no dentro
del historial:

- **Fichero**: `/home/.config/cipo/posicoes`, en la misma carpeta del Cipó
  que los marcadores (`/home/.config/cipo/marcadores`). Si no hay partición de
  datos, `/tmp/.config/cipo/posicoes`. Las carpetas se crean se falta.
- **Formato**: texto simples, una línea por sitio, `url<TAB>pixels`. El
  separador es la tabulación, el mismo que usan los marcadores: una dirección
  no la trae codificada, porque el espacio y los caracteres especiales van en
  `%XX` (`/foto uno.png` → `/foto%20uno.png`). Al leer, la posición es lo que
  viene **después de la última tabulación** y el resto es la dirección.
- **Robustez**: si el fichero no existe o está estropeado, el Cipó arranca
  igual (sin posiciones). Una línea que no tenga el formato `url<TAB>pixels`
  (o cuya posición no sea un número) se ignora. El fichero se escribe entero
  cada vez que se guarda una posición.
- **Límite**: 64 sitios. Cuando la lista se llena, sale el más antiguo; un
  sitio que se vuelve a visitar pasa al final de la lista (es el más reciente).

**Cuándo se guarda y cuándo se repone.** Al salir de un sitio (navegar a
otro, recargar o cerrar el programa) se guarda la posición en la que se
quedó; al entrar en un sitio se repone la que tenga guardada, si la hay. Al
volver atrás con «atrás» manda la posición del historial (que es la misma
que se acaba de guardar), así que los dos caminos dan el mismo resultado.

## Criterios de aceptación

- [x] El Ctrl+clic sobre un enlace abre una pestaña nueva con la página del
      enlace, y la pestaña actual no se mueve
- [x] El menú del botón derecho ofrece «Abrir en pestaña nueva» y al elegirla
      se abre la pestaña
- [x] «Guardar imagen» y «Guardar enlace» siguen en su sitio y funcionan
- [x] Un clic normal (sin Ctrl) sigue navegando en la pestaña actual
- [x] La posición de lectura se guarda al salir de un sitio y se repone al
      volver a él (navegando otra vez o recargando)
- [x] La posición sobrevive a reiniciar el programa (fichero
      `/home/.config/cipo/posicoes`)
- [x] Un fichero de posiciones estropeado no impide que el Cipó arranque
- [x] Test automático: `tests/h127_navegador_extra.py`

## Notas

**El texto del menú va en castellano.** El resto de la interfaz del Cipó está
en castellano («Guardar imagen», «Guardar enlace», «Atrás», «Adelante»,
«Recargar»), así que la línea nueva es «Abrir en pestaña nueva» y no
«Abrir en aba nova».

**El Ctrl+clic se prueba por el QMP, no por el HMP.** El monitor de HMP del
QEMU manda «sendkey ctrl» y **solta la tecla en la siguiente orden**, así que
no sirve para un Ctrl+clic (el modificador llega a zero antes del clic). La
prueba `h127` abre un segundo monitor en **QMP** e usa `input-send-event`
(tecla apretada, botón apretado, botón soltado, tecla largada).

**Lo que não se toca.** El motor web (`userland/web/`) no cambia: la posición
de leitura é del programa, no del renderizador. El historial (`historico[]`,
`historico_pos[]`) queda como estaba.
