<!-- categoria: Juegos y aplicaciones/Cipó; orden: 20; titulo: Cipó: JavaScript y DOM -->
# Cipó: JavaScript y DOM en las páginas

El navegador **Cipó** ejecuta el JavaScript de las páginas con **QuickJS**. Desde la versión 8.5.6.3 (D488) el
script ve un **DOM de verdad**, que puede cambiar de forma, recibe los eventos del ratón y del teclado, mide la
página y maneja el historial. Esta guía resume lo que funciona, para quien escribe páginas que se abren en SaguiOS.

## El árbol de la página

- **Consultar**: `getElementById`, `querySelector` y `querySelectorAll` con selectores completos (`#id`, `.clase`,
  etiqueta, `[atributo]` con `=`, `~=`, `|=`, `^=`, `$=`, `*=`; `>`, `+`, `~` y descendiente; `:first-child`,
  `:last-child`, `:nth-child(2n+1)`, `:nth-of-type`, `:not()`, `:is()`, `:where()`, `:has()`, `:empty`,
  `:checked`, `:disabled`, `:focus`...), `getElementsByTagName`, `getElementsByClassName`, `matches`, `closest`.
- **Cambiar**: `createElement`, `createTextNode`, `createDocumentFragment`, `appendChild`, `insertBefore`,
  `removeChild`, `replaceChild`, `remove`, `append`, `prepend`, `before`, `after`, `replaceWith`,
  `replaceChildren`, `cloneNode`, `innerHTML` y `outerHTML` de cualquier elemento, `insertAdjacentHTML`,
  `<template>.content`, `textContent`, `setAttribute`, `classList`, `style` (también `style.cssText`),
  `dataset`, `value`, `checked`, `hidden`.
- **Observar**: `MutationObserver` (hijos, atributos y texto, con `subtree`), `IntersectionObserver` (avisa al
  entrar y salir de la ventana al desplazar) y `ResizeObserver` (una vez).

## Los eventos

`addEventListener` y `removeEventListener` con **captura y burbuja** (`{ capture, once, passive, signal }`),
`preventDefault`, `stopPropagation`, `stopImmediatePropagation`, `dispatchEvent` y `CustomEvent`. Los manejadores
`onclick = ...` y los atributos `onclick="..."` también valen.

| Lo que hace la persona | Eventos |
|---|---|
| Clic con el botón izquierdo | `mousedown`, `mouseup`, `click` (y `dblclick` con doble clic) en el elemento bajo el puntero |
| Botón derecho | `contextmenu` (con `preventDefault` no sale el menú del Cipó) |
| Mover el ratón | `mousemove`, `mouseover`, `mouseout`, `mouseenter`, `mouseleave` (solo si la página los escucha) |
| Teclado | `keydown`, `keypress`, `keyup` en el elemento con foco (`key`, `code`, `keyCode`); con `preventDefault` en las flechas la página no se desplaza |
| Foco | `focus`, `blur`, `focusin`, `focusout` al pulsar un campo, un botón o un enlace |
| Escribir en un campo | `input` en cada tecla; `change` al pulsar Intro o al salir del campo |
| Enviar un formulario | `submit` (con `preventDefault` no se navega) |
| Carga | `DOMContentLoaded`, `load` |
| Ventana | `resize`, `scroll` |

## Temporizadores y animación

`setTimeout`, `setInterval`, `requestAnimationFrame`, `queueMicrotask` y las promesas siguen el orden de un
navegador: cada tarea corre con sus microtareas detrás, y los cuadros de `requestAnimationFrame` van todos juntos,
como mucho unos 60 por segundo. El Cipó no se queda colgado: en cada vuelta da al script unos 12 ms y luego
atiende al ratón y al teclado. Un script que no acaba en 20 s se para.

## Medidas

`getBoundingClientRect`, `offsetWidth/Height/Top/Left`, `clientWidth/Height`, `scrollWidth/Height`,
`getComputedStyle` (lo que calcula el motor: tamaños, márgenes, rellenos, bordes, colores, `display`,
`position`...), `innerWidth`, `innerHeight`, `scrollX`, `scrollY`, `scrollTo`, `scrollBy`, `scrollIntoView` y
`document.elementFromPoint`. Pedir una medida justo después de un cambio maqueta solo lo que cambió.

## Historial

`history.pushState`, `history.replaceState`, `history.state`, `history.back()` y el evento `popstate`: la
dirección de la barra cambia sin descargar la página, y el botón Atrás (o Retroceso) vuelve a la entrada anterior
avisando al script. `location.hash = "#parte"` y los enlaces a `#parte` de la misma página lanzan `hashchange` y
desplazan hasta el elemento.

## Rapidez

Cuando el script cambia algo, el Cipó no rehace la página entera: el estilo se vuelve a calcular solo en lo que
cambió, se conservan las medidas de lo demás y se vuelve a pintar solo la franja de la ventana afectada. Varios
cambios seguidos (en una misma vuelta del bucle) se maquetan una sola vez.

## Lo que aún no hay

Web Workers, `<canvas>` desde el JavaScript de la página, `Shadow DOM` y elementos personalizados, la selección
de texto (`getSelection`) y `:hover` en los selectores del script. `scrollTop`, `scrollLeft` y `scrollTo` de un
bloque con `overflow: auto` o `scroll` lo desplazan.

## Ver lo que hace el script

`console.log` escribe en el registro del sistema (Visor de registros, o el puerto serie), con el prefijo
`cipo: js:`. Allí salen también los errores (`qjs: erro ...`) y, al maquetar por un cambio del script, cuánto costó:
`cipo: js: layout N incremental: E estilos, M medidas guardadas, franja Y0-Y1, T ms`.
