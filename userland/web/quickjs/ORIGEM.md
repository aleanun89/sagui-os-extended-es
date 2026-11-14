# QuickJS en el SaguiOS (D149)

Este directorio es el motor de JavaScript **QuickJS**, de terceros, con el
porte al SaguiOS.

## De dónde viene

- **Upstream:** <https://github.com/bellard/quickjs> (Bellard), rama `master`,
  versión `2026-06-04` (el fichero `VERSION`).
- **Licencia:** MIT (el fichero `LICENSE` upstream). No es la GPL: el
  repositorio es privado y no se publica, pero la procedencia queda
  registrada igual en `docs/terceros.md`.
- **Qué se ha cogido:** `quickjs.c`, `quickjs.h`, `cutils.c/h`,
  `libregexp.c/h`, `libregexp-opcode.h`, `libunicode.c/h`,
  `libunicode-table.h`, `quickjs-atom.h`, `quickjs-opcode.h`, `dtoa.c/h` y
  `list.h`. **No** se ha cogido `qjs.c` (el intérprete de línea de
  comandos), `qjsc.c` (el compilador a bytecode), `quickjs-libc.c` (la capa
  de sistema) ni `run-test262.c`.

## Lo que se ha cambiado del código original

Solo tres cosas, y las tres están marcadas con `PORT (D149)` en el propio
código:

1. **`CONFIG_ATOMICS` desligado.** El `Atomics.*` necesita `<pthread.h>` y
   `<stdatomic.h>`, y el SaguiOS no tiene hilos de usuario. Ahora solo se
   liga si `SAGUI_SEM_ATOMICS` está definido.
2. **La llamada a `js_atomics_free_waiters()`** (en `JS_FreeRuntime`) quedó
   dentro del `#ifdef CONFIG_ATOMICS`, porque su definición también.
3. Nada más: el resto del código es el del upstream, byte a byte.

## El porte

El QuickJS está escrito para un sistema con libc. Desde la D210 usa la **libc
común** del SaguiOS (`userland/libc/`, la misma de NetSurf, OpenTyrian, SQLite y
MicroPython): compila con `-nostdinc -I userland/libc` y enlaza sus objetos
(`LIBC_OBJ`). Esa libc ya da lo que el porte tenía aparte:

- **la memoria**: `malloc` de la libsagui (D209), alineado a 8 (lo que pide
  `JS_MALLOC_ALIGN`), y `malloc_usable_size` (`malloc.h`);
- **el printf y el strtod exactos** (`numeros.c`): las cifras exactas del doble
  y el redondeo correcto. Los números de JavaScript (`toString`, `toFixed`...)
  los sigue escribiendo el `js_dtoa` del propio QuickJS (`dtoa.c`);
- **el reloj** (`gettimeofday`, `clock_gettime`, `localtime_r` con
  `tm_gmtoff`), `fenv.h`, `alloca` y las mates que la libm de doble precisión
  no trae (`hypot`, `fmin`, `fmax`, `round`, `cbrt`, `lrint`);
- **la división de 64 bits**: la de la libsagui (D160).

Hasta la D210 todo esto eran unas cabeceras de reemplazo (`inc/`) y
`qjs_port.c`, con su propio printf sobre `js_dtoa`; se quitaron.

## El DOM mínimo

`qjs_bridge.c` le da al QuickJS lo que él no sabe: un `document` con `title`
y `body` (`innerHTML` y `textContent`), un `console.log`, un `alert` y los
temporizadores (`setTimeout` y `clearTimeout`). Y la ventana (D159): `window`
(es el propio objeto global, con `window.parent`, `window.top` y
`window.self`), `location` (`href`, `assign`, `replace`, `reload`),
`document.URL` y `navigator.userAgent`.

Desde la D160, además, el DOM **de verdad**: el navegador (el Cipó) le
manda al puente la página como un árbol de elementos (en texto; el formato
está en `qjs_bridge.h`) y el puente crea un objeto JavaScript por elemento.
Así el script puede consultar la página

```js
document.getElementById("id");
document.getElementsByTagName("a");
document.querySelector("#id");        // y ".clase" y "a.clase"
document.querySelectorAll("p");
```

leer cada elemento (`tagName`, `id`, `className`, `textContent`,
`getAttribute`, `children`, `parentElement`) y poner un tratador de clic:

```js
elemento.onclick = function (ev) { ... };
elemento.addEventListener("click", function (ev) { ... });
```

Al pulsar, el navegador pregunta al puente (`qjs_clique`) si hay tratador
para el elemento; el puente llama al `onclick` y a los escuchas con el
evento (`type`, `target` y `preventDefault`) y dice si quedó cancelado: si
quedó, el navegador no navega. Los tratadores se guardan en el puente; el
motor de la web solo sabe qué elementos los tienen (para que sean objetivo
del ratón), que es lo que le pregunta el navegador con `qjs_clique_ids`.

Lo que **no** hay todavía: escribir en el DOM (`createElement`,
`appendChild`, `setAttribute`, `remove()`), más eventos que el clic, ni
`canvas` en el JavaScript de la página.

Cuando el script pide otra página (`location.href = "..."`), el puente **no
navega**: avisa al navegador (el gancho `navegar`) y el Cipó lo hace fuera del
script, porque el motor se destruye y se recrea en cada página (D149) y
navegar a midstelo lo destruiría mientras corre.

## La pila

La pila de un programa del SaguiOS es de 64 KB, y el parser del QuickJS
recursa mucho. Por eso la ejecución corre en una pila grande de 2 MB (como
se hacía con el intérprete antiguo, en `js_nativo.h`).

## Las pruebas

- `tests/h125_quickjs.py`: el programa `/testes/quickjs` evalúa una serie de
  scripts y escribe en el log el valor de cada uno.
