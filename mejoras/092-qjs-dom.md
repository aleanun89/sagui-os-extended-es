# 092 - El JavaScript de las páginas ve un DOM de verdad

- **Estado:** propuesta
- **Prioridad:** alta
- **Alta:** 2026-11-11

## Problema / motivación

El JavaScript del Cipó (el QuickJS de la D149) tenía un DOM **de mentira**:
`document.title`, `document.body.innerHTML`, `document.body.textContent`,
`document.URL`, `console`, `alert` y `setTimeout`. Nada más.

Pero casi todas las páginas que hacen algo con JavaScript empiezan igual:

```html
<script>
  var menu = document.getElementById('menu');
  menu.onclick = function (ev) { ev.preventDefault(); abrir(); };
</script>
```

Con el DOM de mentira, ese script moría en la **primera línea** con un
`ReferenceError: getElementById is not defined`, y la página se quedaba
**en blanco**: el mismo síntoma que arregló la D159 para el reenvío del
DuckDuckGo, pero ahora en la puerta de entrada de casi todo el JavaScript de
la web. Y sin `preventDefault` no se puede hacer el caso típico: un menú que
se abre al pulsar y que **no** debe seguir el enlace.

## Propuesta

- **En el motor de la web** (`userland/web/motor.c`, `motor.h`): que exponga
  el árbol de la página, que ya tiene (la etiqueta, los atributos, el texto,
  el padre y los hijos de cada nodo), con un identificador por nodo
  (`motor_dom_lista`, `motor_dom_corpo`, `motor_dom_documento`). Y que un
  elemento con un tratador de clic del JavaScript sea **objetivo del ratón**
  (`motor_marcar_clique`), sin duplicar el código de pintados: el objetivo se
  crea en el layout, como el de un enlace.
- **En el puente del QuickJS** (`userland/web/quickjs/qjs_bridge.c`): que
  reciba el árbol (como texto, con el formato de `qjs_bridge.h`) y cree un
  objeto JavaScript por elemento, con `getElementById`,
  `getElementsByTagName`, `querySelector` y `querySelectorAll`; y con
  `tagName`, `id`, `className`, `textContent`, `getAttribute`, `children` y
  `parentElement` en cada elemento. Que guarde los tratadores de clic
  (`onclick` y `addEventListener("click", ...)`) y que sepa llamar a ellos
  con un evento (`type`, `target` y `preventDefault`).
- **En el Cipó** (`userland/programas/cipo.c`): que pase el árbol del motor al
  puente antes de correr los scripts, que marque en el motor los elementos
  con tratador (y vuelva a maquetar), y que al pulsar pregunte al puente si
  hay tratador: si el script cancela el evento, no navega.

## Criterios de aceptación

- [x] `document.getElementById("id")`, `document.getElementsByTagName("a")`,
      `document.querySelector("#id")`, `document.querySelector(".clase")` y
      `document.querySelectorAll(...)` existen y devuelven lo que toca.
- [x] Del elemento: `tagName`, `id`, `className`, `textContent`,
      `getAttribute("x")`, `children` y `parentElement`.
- [x] `document.body` y `document.documentElement` son elementos de verdad.
- [x] `elemento.onclick = function(){...}` y
      `elemento.addEventListener("click", function(){...})` se guardan.
- [x] Al pulsar con el ratón, el tratador corre y recibe el evento (con
      `type` y `target`).
- [x] `event.preventDefault()` cancela la navegación del enlace.
- [x] Test automático en `tests/` que lo comprueba (`tests/h135_qjs_dom.py`).
- [x] Decisión registrada en `docs/decisoes.md` (D160).

## Notas

- **Lo que falta** (la parte 3): la **escritura** en el DOM
  (`document.createElement`, `appendChild`, `setAttribute`,
  `elemento.textContent = "..."`, `elemento.remove()`), con el efecto en la
  página. Y también: más eventos (el que hay es el clic), `canvas` en el
  JavaScript de la página, `document.forms` y los selectores complejos
  (`:first-child`, `[attr=valor]`, `>`).
- **Topes**: los 256 primeros nodos de la página, 8 atributos por elemento y
  4 escuchas por elemento.
- El camino viejo del `data-sagui-js` (del motor de JavaScript que hubo antes
  del QuickJS) sigue intacto: el objetivo del ratón lleva su propio campo
  (`dom`) para no mezclar los dos espacios de nombres.
