# Cipó, el navegador de SaguiOS

Motor de páginas propio (`userland/web/`, ver `docs/cipo-compatibilidad.md`), QuickJS para JavaScript, HTTP/HTTPS (TLS 1.2 y 1.3).

## Ventana
Pestañas arriba; debajo, la barra de herramientas (atrás, adelante, recargar/parar, inicio, dirección con candado y estrella, menú ≡) y, si se quiere, la barra de marcadores.

- **Pestañas:** favicon o icono genérico, × para cerrar, «+» para otra, clic central cierra, arrastrar reordena, botón derecho = menú (recargar, duplicar, cerrar, cerrar las demás). Hasta 16; la rueda sobre las pestañas pasa de una a otra.
- **Candado:** verde = HTTPS válido, ámbar = sin verificar, rojo = HTTP. Un clic muestra el certificado.
- **Dirección:** sugerencias de marcadores e historial al escribir; texto suelto = búsqueda con el motor elegido.
- **Estrella:** marca la página; si ya está marcada, editar o quitar.

## Atajos
Ctrl+T nueva · Ctrl+W cerrar · Ctrl+Tab siguiente · Ctrl+L dirección · Ctrl+D marcar · Ctrl+B menú de marcadores · Ctrl+Mayús+B gestor · Ctrl+H historial · Ctrl+J descargas · Ctrl+F buscar · F5/Ctrl+R recargar · Alt+Izq/Der atrás/adelante · Alt+Inicio página de inicio. Esc cierra el panel.

## Paneles (menú ≡)
Marcadores (carpetas, abrir/editar/borrar, importar y exportar HTML «Netscape bookmark file»), Historial (búsqueda), Descargas (progreso, pausa, cancelar, abrir carpeta), y los ajustes General, Privacidad y Rendimiento.

## Ajustes (`/home/.config/cipo/cipo.conf`, `clave=valor`)
`inicio_modo` (0 página propia, 1 en blanco, 2 dirección), `inicio_url`, `buscador` (DuckDuckGo, Wikipedia, Google, Bing), `hilos` (descargas simultáneas, 1-12, recortado al leer), `descargas` (carpeta), `js`, `imagenes`, `barra_marcadores`. Otros ficheros: `marcadores`, `historial`, `posicoes`.

## Pintura sin parpadeo (D441)

La página se pinta en un búfer aparte y se copia a la ventana de una vez (nunca se ve el fondo vacío). Durante la carga la
maquetación y el repintado se agrupan (como mucho cada 80 ms, más uno final). Al desplazarse se mueven las filas ya
pintadas y solo se pinta la franja nueva (si hay `fixed`/`sticky` o un vídeo, se repinta entera de una vez).

## Descargas simultáneas
Con 1 todo se baja en serie; el valor por defecto es 4. Con N, hasta N hilos bajan por adelantado las imágenes y fuentes web de la página; el análisis, la decodificación y el pintado siguen en el hilo principal. Si la memoria libre no alcanza (equipos de 128-256 MB), el valor efectivo baja y el panel Rendimiento lo dice. `cipo --hilos N [dirección]` lo fija solo esa vez. Las conexiones HTTPS se serializan (la TLS no es reentrante). Medido (QEMU, 30 imágenes de 250 ms): 1 → 7800 ms, 4 → 2080 ms, 12 → 870 ms.

## Pendiente
Zoom, user-agent y modo de compatibilidad; cookies (el Cipó no las tiene); CSS y JS en paralelo.

Prueba: `tests/h373_cipo_ui.py` (`ui` o `hilos` para una parte). Decisión: D373.
