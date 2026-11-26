# Cipó, el navegador de SaguiOS

Motor de páginas propio (`userland/web/`, ver `docs/cipo-compatibilidad.md`), QuickJS para JavaScript, HTTP/HTTPS (TLS 1.2 y 1.3).

## Ventana
Pestañas arriba; debajo, la barra de herramientas (atrás, adelante, recargar/parar, inicio, dirección con candado y estrella, menú ≡) y, si se quiere, la barra de marcadores.

- **Pestañas:** favicon o icono genérico, × para cerrar, «+» para otra, clic central cierra, arrastrar reordena, botón derecho = menú (recargar, duplicar, cerrar, cerrar las demás). Hasta 16; la rueda sobre las pestañas pasa de una a otra.
- **Candado:** verde = HTTPS válido, ámbar = sin verificar, rojo = HTTP. Un clic muestra el certificado.
- **Dirección:** sugerencias de marcadores e historial al escribir; texto suelto = búsqueda con el motor elegido.
- **Estrella:** marca la página; si ya está marcada, editar o quitar.

## Atajos
Ctrl+T nueva · Ctrl+W cerrar · Ctrl+Tab siguiente · Ctrl+L dirección · Ctrl+D marcar · Ctrl+B menú de marcadores · Ctrl+Mayús+B gestor · Ctrl+H historial · Ctrl+J descargas · Ctrl+F buscar · F5/Ctrl+R recargar · Alt+Izq/Der atrás/adelante · Alt+Inicio página de inicio · Ctrl++ / Ctrl+- / Ctrl+0 (o Ctrl+rueda) acercar, alejar y tamaño normal. Esc cierra el panel.

## Paneles (menú ≡)
Marcadores (carpetas, abrir/editar/borrar, importar y exportar HTML «Netscape bookmark file»), Historial (búsqueda), Descargas (progreso, pausa, cancelar, abrir carpeta), y los ajustes General, Privacidad y Rendimiento.

## Ajustes (`/home/.config/cipo/cipo.conf`, `clave=valor`)
`inicio_modo` (0 página propia, 1 en blanco, 2 dirección), `inicio_url`, `buscador` (DuckDuckGo, Wikipedia, Google, Bing), `hilos` (descargas simultáneas, 1-12, recortado al leer), `descargas` (carpeta), `js`, `imagenes`, `barra_marcadores`. Otros ficheros: `marcadores`, `historial`, `posicoes`.

## Zoom, bloques con desplazamiento y medios (D489)

- **Zoom:** Ctrl++ y Ctrl+- (25 %…500 %), Ctrl+0 vuelve al 100 %, Ctrl+rueda; también en el menú ≡. El texto se reparte otra vez (no se estira la imagen) y es **por sitio**
  (`~/.config/cipo/zoom`).
- **Bloques con `overflow: auto/scroll`:** barra con carril y pulgar; la rueda mueve el bloque bajo el puntero y, cuando llega al tope, la página; arrastra el pulgar o haz clic en el carril.
- **`<video controls>` y `<audio controls>`:** barra de reproducir/pausa, tiempo, progreso (un clic salta) y volumen. `<audio>` abre WAV, MP3 y FLAC; `<video>`, MP4 H.264/AAC. Solo un medio a la vez. Un clic en el vídeo lo pausa.

## Pintura sin parpadeo (D441)

La página se pinta en un búfer aparte y se copia a la ventana de una vez (nunca se ve el fondo vacío). Durante la carga la
maquetación y el repintado se agrupan (como mucho cada 80 ms, más uno final). Al desplazarse se mueven las filas ya
pintadas y solo se pinta la franja nueva (si hay `fixed`/`sticky` o un vídeo, se repinta entera de una vez).

## Descargas simultáneas
Con 1 todo se baja en serie; el valor por defecto es 4. Con N, hasta N hilos bajan por adelantado las imágenes y fuentes web de la página; el análisis, la decodificación y el pintado siguen en el hilo principal. Si la memoria libre no alcanza (equipos de 128-256 MB), el valor efectivo baja y el panel Rendimiento lo dice. `cipo --hilos N [dirección]` lo fija solo esa vez. Las conexiones HTTPS se serializan (la TLS no es reentrante). Medido (QEMU, 30 imágenes de 250 ms): 1 → 7800 ms, 4 → 2080 ms, 12 → 870 ms.

## Red (D487)
- **Cookies** completas (Domain, Path, Expires, Max-Age, Secure, HttpOnly, SameSite); las persistentes se guardan por usuario en `/home/.config/cipo/cookies`. Panel Privacidad: cuántas hay y «Borrar cookies y caché».
- **Formularios** con `method=post` (urlencoded, `multipart/form-data` o `text/plain`) y redirecciones 301/302/303/307/308 con el método correcto.
- **JavaScript**: `fetch`, `XMLHttpRequest`, `FormData`, `Blob`, `AbortController`, CORS sencillo y `WebSocket` (`ws://` y `wss://`); `localStorage` persistente (`/home/.config/cipo/storage/`, 1 MB por origen) y `sessionStorage`, por origen.
- Respuestas **gzip, deflate y brotli**; **caché HTTP** en memoria (`Cache-Control`, `ETag`, `Last-Modified`); User-Agent `Mozilla/5.0 (X11; SaguiOS i686) Cipo/8.5 (like Gecko)`.
- `sobre:red` muestra el estado de la red; `sobre:red?recargar` (escrito en la barra) relee cookies y almacenamiento del disco.

## JavaScript y DOM (D488)
DOM mutable con remaquetado incremental, eventos completos, bucle de eventos, medidas e historial: ver la ayuda «Cipó: JavaScript y DOM» (`rootfs/ayuda/cipo-javascript.md`).

## Pendiente
Modo de compatibilidad; HTTP/2; `<input type=file>` en formularios; CSS y JS en paralelo; Web Workers.

Pruebas: `tests/h373_cipo_ui.py` (`ui` o `hilos` para una parte), `tests/h487_rede.py` (red), `tests/h488_cipo_js_dom.py` (JavaScript y DOM), `tests/h489_cipo_css_medios.py` (zoom, overflow, tablas, CSS y medios). Decisiones: D373, D487, D488, D489.
