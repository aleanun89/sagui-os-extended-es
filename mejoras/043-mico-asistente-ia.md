# 043 - Cliente de chat con IA y asistente en Mico (OpenRouter u otro endpoint configurable)

- **Estado:** cliente `ia` de terminal hecho y probado (D134). El panel «Asistente»
  de Mico está implementado y probado en H111 (D140). La **aplicación de chat con
  ventana** está hecha y probada en H116 (D143) y **ampliada en la v7.4.3 (D239,
  H239)**: lista lateral de conversaciones guardadas (abrir, renombrar, borrar),
  Markdown en las respuestas con bloques de código y botón «Copiar», también en
  streaming, Preferencias en la ventana (servidor, clave, modelo del chat), mensaje de
  sistema editable por conversación, «Regenerar» y «Adjuntar». Sigue pendiente, en el
  Mico, mostrar las propuestas de cambio como diferencias aplicables.
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Depende de:** Mico (037), HTTPS/libtls (027), red (020); conviene tener antes la 042 (componentes)

## La aplicación de chat, primera versión (D143, 2026-11-11)

Lo primero de la ampliación está hecho: `/bin/chatia` es una aplicación con
ventana (`userland/programas/chatia.c`), con su icono en el menú Inicio.

- **Interfaz:** la conversación arriba (con «Tú:», «Asistente:» y «Sistema:»),
  la caja de texto abajo y los botones «Enviar», «Detener» y «Nueva
  conversación». La respuesta llega en streaming y se ve a medida que llega.
- **Contexto:** el cliente de IA (`userland/ia/ia.c`) manda la conversación
  entera en el array `messages` (hasta las últimas doce mensajes), para que el
  modelo sepa de qué se habla. La API nueva es `ia_chat_conversa()`.
- **Castellano:** los rótulos de la ventana y la instrucción al modelo
  («responde siempre en castellano»). La respuesta llega en UTF-8 y se pasa a
  Latin-1 para la pantalla.

La configuración sigue siendo la del cliente: `IA_URL`, `IA_MODELO` e
`IA_CHAVE` en el entorno, o `/home/.config/ia/provedor` y
`/home/.config/ia/chave`. El programa acepta además `--url` y `--modelo` en la
línea de órdenes.

**Lo que quedaba de la ampliación** se hizo en la v7.4.3 (D239): ver la sección
siguiente.

## La ampliación del chat (D239, v7.4.3)

- **Conversaciones:** columna a la izquierda con las guardadas, la más nueva arriba;
  «Nueva conversación», «Renombrar» y «Borrar» (con confirmación), y el menú del botón
  derecho. Título automático con las primeras palabras de la primera pregunta. Cada una
  es un fichero de texto legible, `/home/.config/ia/conversas/conversa-NNNN.txt`
  (título, `--- system`, `--- user`, `--- assistant`), escrito primero a un `.tmp` y
  puesto en su sitio después. Sin `/home` persistente van a `/tmp/ia/conversas` y la
  ventana lo avisa.
- **Markdown** con el componente nuevo de la libsagui `rico_*`
  (`userland/libsagui/texto_rico.c`, el «visor de texto con estilos»): negrita, cursiva,
  títulos, listas, citas, enlaces, código en línea y bloques de código con fondo y botón
  «Copiar» al portapapeles (D074). Se formatea mientras la respuesta llega.
- **Preferencias** dentro de la ventana: servidor (con los proveedores con nombre de
  D145), clave (con asteriscos; vacía = no cambia), modelo del chat (con «Cargar
  modelos» del servidor) y el mensaje de sistema de la conversación (con el de
  castellano por omisión). El servidor, la clave y `modelo_chatia` van a la
  configuración compartida de D145.
- **«Regenerar»** la última respuesta (con el mismo contexto) y **«Adjuntar»** un
  fichero de texto de hasta 16 KB, que va como bloque de código en la pregunta.
- El cliente de IA gana `ia_chat_conversa_ref()` (mensajes de cualquier tamaño),
  `ia_config_carregar_arquivo()` e `ia_pasta()`, sin cambiar lo que ya había.
- Prueba: `tests/h239_chatia_conversas.py`; capturas `docs/capturas/H239-*.png`.

## Problema / motivación

Mico ya edita, compila y muestra errores. Un asistente de IA permitiría:
- explicar código;
- proponer correcciones a partir de los errores de «Problemas»;
- generar funciones y pequeños programas.

Todo desde dentro de SaguiOS, en un PC antiguo.

El modelo **no corre en SaguiOS**: se usa por red un servidor con API **compatible con la de
OpenAI** (`POST /v1/chat/completions`, `GET /v1/models`). Así sirven tanto un
**llama.cpp server** (`llama-server`) en otro PC de la red local, sin Internet y sin coste,
como cualquier proveedor compatible por HTTPS.

## Ampliación (2026-11-08): cliente de chat y proveedores configurables

- **Aplicación de chat «Asistente IA»**, independiente de Mico:
  - **conversaciones**: lista lateral, nueva, renombrar, borrar y guardar en `/home/.ia/conversaciones/`;
  - respuestas **en streaming**, con **Markdown** (el motor del visor de la 019/049), bloques de código con botón «Copiar» y portapapeles (D074);
  - selector de **proveedor y modelo** en la propia ventana;
  - «Detener» y «Regenerar»;
  - mensaje de sistema editable por conversación;
  - adjuntar el contenido de un archivo de texto.
- **Proveedores configurables** (ventana «Configuración» y `/home/.ia/proveedores`):
  - **OpenRouter** (`https://openrouter.ai/api/v1`, API compatible con OpenAI, con su lista de modelos por `GET /models` y las cabeceras recomendadas `HTTP-Referer`/`X-Title`);
  - **cualquier endpoint compatible con OpenAI**: llama.cpp `llama-server`, Ollama (`/v1`), LM Studio, vLLM u otros proveedores;
  - opcional: la API nativa de Anthropic (`/v1/messages`), que tiene otro formato y se añade como adaptador si da tiempo.
  - Por proveedor: nombre, URL base, clave de API, modelo por defecto, tiempo límite y parámetros (temperatura, máximo de tokens).
  - Botón **«Probar conexión»**.
- **Clave de API:** se guarda en `/home` **en claro** y se avisa de ello. Nunca se muestra en pantalla completa ni en el log, y el informe de `infopc` no la incluye.
- **Terminal:** `ia "pregunta"` (y `ia -m modelo`, `ia --proveedor X`), con salida en streaming, para usarla en *pipes* (`cat error.txt | ia "explícame esto"`).
- **Mico:** el panel «Asistente» y las acciones de abajo usan la misma configuración y la misma biblioteca.
- **Biblioteca común** en la libsagui: cliente HTTP/HTTPS (libtls), **JSON** propio y **SSE** (streaming), compartida por el chat, `ia`, Mico y Python (`/lib/python/ia.py`).
- **Pruebas:** un **servidor falso compatible con OpenAI** en el anfitrión (Python), con streaming, lista de modelos, errores 401/429/500 y cortes de conexión. Sin Internet, sin claves reales y sin coste.
- **Privacidad:** la primera vez que se usa un proveedor externo, aviso de que la conversación (y el código, en Mico) se envía a terceros. Con llama.cpp/Ollama en la red local no sale nada de casa.

## Propuesta original (asistente en Mico)

- **Configuración** (`/home/.mico/ia`, o en el `.mico` del proyecto):
  - URL base (p. ej. `http://192.168.1.10:8080/v1`);
  - modelo, temperatura y tamaño máximo de respuesta;
  - clave de API opcional. Se guarda **en claro**: avisar y no mostrarla nunca en pantalla ni
    en el log.
  - Botón «Probar conexión», que llama a `/v1/models`.
- **Cliente en la libsagui** (reutilizable por otros programas):
  - HTTP y HTTPS con la libtls;
  - un **parser/generador JSON propio** pequeño;
  - **respuestas en streaming** (`stream: true`, *server-sent events*), para ver el texto según
    llega;
  - tiempo límite y botón «Detener».
- **Panel «Asistente» en Mico** (lateral o inferior):
  - chat con el contexto del archivo o la selección;
  - acciones rápidas en el menú contextual del editor: «Explicar», «Corregir con los errores
    de Problemas», «Añadir comentarios», «Escribir pruebas», «Generar aquí...»;
  - las propuestas de cambio se muestran como **diferencias** (antes/después) y solo se aplican
    al pulsar «Aplicar» (deshacer normal después);
  - el contexto se recorta para caber en la ventana del modelo: archivo actual, selección,
    errores y, opcionalmente, otros archivos abiertos.
- **Modo agente (más adelante, opcional):** herramientas (*tool calls*) que el modelo puede
  pedir: leer un archivo del proyecto, buscar en la carpeta, proponer la escritura de un
  archivo y ejecutar una orden en el terminal (compilar, probar). **Cada escritura y cada orden
  necesita la confirmación del usuario**, y solo dentro de la carpeta del proyecto. Nada se
  ejecuta solo.
- **Python:** el mismo cliente expuesto en `/lib/python` (p. ej. un módulo `ia`), usando
  `requests` (032).

## Criterios de aceptación

- [x] Configurar la URL y probar la conexión contra un `llama-server` de la red local
      → `ia --url http://host:porta/v1` (o `IA_URL`), `ia --provar` (comprueba la
      conexión con `GET /v1/models`) e `ia --modelos` (lista los modelos)
- [x] Chat con respuestas en streaming y botón «Detener»
      → en el terminal: `ia "pregunta"` escribe cada pedazo según llega y
      `--limite N` (o el Ctrl+C) la corta; lo que ya llegó se queda escrito. En Mico,
      H111 comprueba que la respuesta se muestra parcialmente durante el streaming.
- [x] «Explicar» y «Corregir con los errores» sobre una selección, con diferencias y «Aplicar»
      → Mico incluye las acciones «Explicar», «Corregir Problemas» y «Generar función»;
      se envía contexto limitado y la respuesta es una propuesta, nunca se aplica automáticamente.
      **D240 (v7.4.3):** la propuesta se pide con el código completo, se compara por líneas
      (`libsagui/difer.c`) y se ve en rojo/verde con contexto; «Aplicar» (un paso de
      Ctrl+Z), «Aplicar este bloque» y «Descartar»; si el archivo cambió, avisa y recalcula.
      Prueba: `tests/h240_mico_diferencias.py`.
- [x] Funciona también por HTTPS con un proveedor compatible (clave de API)
      → el cliente usa la `libtls` (como el `curl`) cuando la URL es `https://`, y
      manda la clave en la cabecera `Authorization`. La clave se lee del entorno
      (`IA_CHAVE`) o de `/home/.config/ia/chave`; nunca se escribe en el
      repositorio ni en la imagen, y `ia --config` no la muestra. La prueba
      automática lo comprueba contra el servidor falso por HTTPS (con `-k`, que
      acepta el certificado autoasignado de la prueba)
- [x] Ningún cambio en archivos ni orden ejecutada sin confirmación
      → el cliente `ia` no escribe ningún archivo ni ejecuta ninguna orden: solo
      pregunta y escribe la respuesta en la pantalla. El modo agente (escrituras y
      órdenes del modelo) queda para más adelante y llevará confirmación
- [x] Test automático con un **servidor falso** compatible en el anfitrión (Python), sin
      Internet ni modelo real
      → `tests/servidor_falso_ia.py` (responde como un servidor de la API de
      OpenAI: modelos, streaming, `[DONE]`, errores 401/429/500) y
      `tests/h105_ia_v62.py`
- [x] Aplicación de chat con ventana, con la conversación como contexto y en castellano
      → `/bin/chatia` (`userland/programas/chatia.c`): la conversación en la ventana,
      el contexto son las últimas doce mensajes (la API nueva del cliente es
      `ia_chat_conversa()`) y todo en castellano, con la instrucción al modelo
      incluida. Prueba: `tests/h116_chatia.py`
- [x] Conversaciones guardadas, Markdown con «Copiar», proveedor y modelo en la
      ventana, sistema editable, «Regenerar» y adjuntos
      → D239 (v7.4.3). Prueba: `tests/h239_chatia_conversas.py`
- [x] Decisión registrada en `docs/decisoes.md`
      → D134 (cliente), D140 (Asistente en Mico), D143 (la aplicación de chat) y D239
      (la ampliación del chat)

## Lo hecho hasta ahora (v6.2, D134 y v6.4, D140)

### Asistente en Mico (D140)

- El panel inferior añade la pestaña «Asistente», con entrada de pregunta,
  respuesta en streaming y botones «Enviar»/«Detener».
- Acciones «Explicar», «Corregir Problemas» y «Generar función» en el panel;
  «Explicar» y «Generar función» también están disponibles en el menú contextual.
- El contexto se limita a la selección o línea actual y, para corregir, hasta
  ocho errores. Las propuestas no modifican el documento.
- Se reutilizan `ia_config_carregar` y `ia_chat`; el aviso de proveedor externo
  aparece antes de enviar la conversación o código. La prueba H111 usa servidor
  falso local y verifica streaming, pantalla y registro.

## Lo hecho hasta ahora (v6.2, D134)

- `userland/inc/ia.h` y `userland/ia/ia.c`: la biblioteca común. Lee la
  configuración (entorno o `/home/.config/ia/provedor` y `/home/.config/ia/chave`),
  monta el pedido de `POST /v1/chat/completions` y lee la respuesta pedazo a
  pedazo (SSE), por HTTP o por HTTPS. El JSON es un lector pequeño y propio.
- `userland/programas/ia.c`: el comando `ia` del terminal.
- La biblioteca queda lista para el panel del Mico (el «Detener» ya es un gancho
  `deve_parar` que el botón usaría) y para el modo agente.

## Notas

- En la petición original se dijo «OpenAPI»; se entiende **API compatible con OpenAI**, que
  es la que ofrecen llama.cpp server, Ollama (`/v1`), LM Studio y otros.
- Privacidad: con un proveedor externo, el código se envía a terceros. Avisar la primera
  vez; con llama.cpp en la red local no sale nada de casa.
- En QEMU la red de usuario llega al anfitrión por `10.0.2.2`: un `llama-server` en el WSL o
  en Windows se alcanza así.
