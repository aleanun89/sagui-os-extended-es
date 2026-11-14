# 065 - Framework web para Python (Microdot) y capa compatible con Bottle

- **Estado:** propuesta (**v8**, decidido por el mantenedor el 2026-11-08, junto con las mejoras de red)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 032 (bibliotecas de Python), D034 (MicroPython), D046/D069 (sockets, `rede_sagui.c`), 049 (HMKD y `servidor`), 064 (SQLite)

## Motivación

[Bottle](https://bottlepy.org/) es un microframework web de un solo archivo, con licencia MIT:
rutas con decoradores, plantillas, archivos estáticos y servidor propio. Sería ideal para hacer
pequeñas aplicaciones web en SaguiOS y abrirlas desde otro PC de la red o desde el Cipó.

## ¿Funciona Bottle tal cual en MicroPython?

**No, no sin cambios.** Bottle está escrito para CPython y usa mucho de su biblioteca estándar.
- **Módulos que nuestro MicroPython no tiene:** `http.cookies`, `email.utils`, `cgi`,
  `mimetypes`, `inspect`, `threading`, `hmac`, `pickle`, `tempfile`, `datetime` completo y
  `wsgiref` (su servidor por defecto).
- **Partes del lenguaje que MicroPython no cubre:** introspección de funciones, `sys._getframe`
  y algunos métodos especiales.

Sí hay base suficiente: sockets de servidor (`bind`/`listen`/`accept`, D046), `select`, `re`,
`json`, `binascii` y `time`. No hay `asyncio` (`MICROPY_PY_ASYNCIO = 0`), así que Microdot, el
microframework hecho para MicroPython, tampoco entra sin activarlo.

## Decisión (2026-11-08)

Va en la **v8**, con las mejoras de red (045 y *loopback*). Se usa el framework **específico de
MicroPython**:
1. **Activar `asyncio`** en MicroPython (`MICROPY_PY_ASYNCIO`), sobre `select` y los sockets no
   bloqueantes de `rede_sagui.c`.
2. **Microdot** (MIT, de Miguel Grinberg) en `/lib/python/microdot/`, registrado en
   `docs/terceros.md`: rutas, peticiones y respuestas, JSON, estáticos, plantillas sencillas,
   sesiones/cookies y, si el tamaño lo permite, WebSocket. Varias conexiones a la vez gracias
   a `asyncio`.
3. **Loopback** (127.0.0.1) en la pila de red, para abrir la aplicación desde el Cipó del propio
   SaguiOS.
4. La capa compatible con Bottle de abajo queda como **opcional**, encima de lo mismo, si hace
   falta para ejemplos de Bottle.

## Propuesta original (capa compatible con Bottle, opcional)

**`/lib/python/bottle.py` propio, compatible con la API básica de Bottle**: los ejemplos del
tutorial de Bottle funcionan sin cambios. No es el archivo original.
- `Bottle()`, `@route`/`@get`/`@post`/`@put`/`@delete`, `default_app()`
- Rutas con comodines: `<nombre>`, `<id:int>`, `<x:float>`, `<p:path>`, `<x:re:...>`
- `request`:
  - `method`, `path`, `query`, `forms`, `params`, `json`, `headers`, `cookies`, `body`;
  - formularios `application/x-www-form-urlencoded` y JSON; multipart y subida de archivos
    en una segunda fase.
- `response`: `status`, `content_type`, `set_header`, `set_cookie`. Devolver un `dict` produce
  JSON.
- `abort()`, `redirect()`, `HTTPError`/`HTTPResponse`, `@error(404)`
- `static_file(nombre, root=...)` con los tipos MIME comunes
- Plantillas **SimpleTemplate** (`template()`, `@view`, `{{ }}`, `% for`/`% if`, `%end`,
  `{{!sin_escapar}}`, `% include`/`% rebase` básicos)
- `run(host='0.0.0.0', port=8080, debug=...)`: servidor HTTP/1.1 propio sobre los sockets de
  SaguiOS, de **una petición a la vez**, con `select` y `keep-alive` sencillo. Sin hilos.
- Se rechaza lo que no exista, con un error claro («no disponible en SaguiOS»): plugins, WSGI
  externo y adaptadores de servidor.

Opcional: activar `asyncio` en MicroPython y ofrecer también Microdot (MIT) para quien lo
prefiera. Queda para una segunda fase.

## Pruebas

- Unidad en el anfitrión: el mismo `bottle.py` con CPython frente al Bottle original, mismas
  respuestas para un conjunto de rutas, plantillas y errores.
- QEMU:
  - aplicación de ejemplo (`/ejemplos/web/hola.py`: rutas, formulario, JSON, estático y
    plantilla) servida desde SaguiOS;
  - el anfitrión hace peticiones por el puerto redirigido de QEMU (`hostfwd`) y compara;
  - el Cipó de dentro no llega, porque no hay *loopback*. Se prueba con otra VM o desde el
    anfitrión.
- Con SQLite (064), cuando exista: un ejemplo de lista de tareas con base de datos.

## Criterios de aceptación

- [ ] Ejemplos del tutorial de Bottle (rutas, comodines, formularios, JSON, estáticos,
      plantillas, errores) funcionan sin cambios
- [ ] Servidor estable con peticiones seguidas y conexiones cortadas
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Licencia: no se copia Bottle (MIT, que también se podría), solo se imita su API.
- Falta *loopback* (127.0.0.1) en la pila de red. Sería útil añadirlo con esta mejora, para
  abrir la aplicación desde el Cipó del mismo SaguiOS.
- Modelo sugerido: **Sonnet medio**. Tamaño orientativo: unas 1.200-1.500 líneas de Python más
  las pruebas.
