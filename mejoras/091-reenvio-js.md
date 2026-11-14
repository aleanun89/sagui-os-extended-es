# 091 - El navegador sigue los reenvíos de las páginas

- **Estado:** propuesta
- **Prioridad:** alta
- **Alta:** 2026-11-11

## Problema / motivación

Al buscar en DuckDuckGo y pulsar un resultado, la página se quedaba **en
blanco**. La causa eran dos cosas distintas:

1. La página de reenvío de DuckDuckGo (`https://duckduckgo.com/l/?uddg=...`)
   es una página diminuta que sólo lleva esto:

   ```html
   <script language='JavaScript'>window.parent.location.replace("https://destino/");</script>
   ```

   El QuickJS del Cipó no conocía `window` (ni `location`), así que el script
   moría con `ReferenceError: 'window' is not defined` y no passaba nada: la
   página se quedaba con su esqueleto vacío (2 líneas, 0 enlaces).

2. Eveno después de arreglar lo anterior, la página de destino (GitHub) no
   cargaba: el servidor manda cabeceras de 5,7 KB (la `content-security-policy`
   es enorme) y el buffer del cliente HTTP era de 4 KB, así que la respuesta
   se rechazaba entera con «Respuesta no válida del servidor».

## Propuesta

- **En el puente del QuickJS** (`userland/web/quickjs/qjs_bridge.c`): la
  ventana y la navegación.
  - `window` es el propio objeto global; `window.parent`, `window.top` y
    `window.self` también (la página de DuckDuckGo usa `window.parent`).
  - `location` con `href` (leer y escribir), `assign()`, `replace()` y
    `reload()`; `document.location` es el mismo objeto.
  - `document.URL` y `navigator.userAgent`.
  - Un gancho nuevo en `qjs_ganchos_t`: `navegar`, el último de la lista
    (quien los ponía, los pones todos: la lista es posicional).
- **En el Cipó** (`userland/programas/cipo.c`): la navegación es **diferida**.
  El script pide la página, pero el Cipó no navega con el motor en marcha
  (navegar a midstelo destruye el motor que está corriendo el script): el
  pedido se guarda y el bucle principal lo hace cuando el script ya acabó.
  Los direcciones relativos se resuelven contra la página actual.
- **El `<meta http-equiv="refresh">`**: el navegador lo entiende y sigue el
  reenvío inmediato (`content="0;url=..."`), que es la otra forma que usan
  las páginas de reenvío.
- **En el cliente HTTP** (`userland/libsagui/http.c`): la cabecera se lee
  línea a línea y las líneas que no caben (o que son más largas que el
  buffer) se **deitan fuera** en vez de tumbar la respuesta entera. Así una
  cabecera gigante no impide ver el cuerpo.

## Criterios de aceptación

- [x] `window`, `window.parent`, `location.href`, `location.assign()`,
      `location.replace()`, `document.URL` y `navigator.userAgent` existen en
      el QuickJS del SO (prueba h125).
- [x] Una página que reenvía con JavaScript lleva el navegador a la página de
      destino (prueba h132).
- [x] Una página que reenvía con `<meta http-equiv="refresh">` también
      (prueba h132).
- [x] El endereço pedido por el script se resuelve contra la página actual.
- [x] El navegador no se cae con el reenvio.
- [x] Buscar en DuckDuckGo, pulsar un resultado y llegar a la página de
      destino: comprobado con el DuckDuckGo de verdad (la página de GitHub se
      dibuja entera).
- [x] Decisión registrada en `docs/decisoes.md` (D159).

## Notas

El DOM del QuickJS sigue siendo mínimo (D149): no hay `querySelector`, ni
eventos, ni `canvas`. Las webs que dependan de eso siguen sin funcionar; lo
que se arregla aquí es el reenvío, que es lo que hace que un enlace «no
resuelva» y deje la pantalla en blanco.
