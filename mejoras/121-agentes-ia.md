# 121 - Agentes de IA con herramientas, confirmación y registro

- **Estado:** hecha (v8.3.1, rama `v8.3.1-agentes`, D295)
- **Prioridad:** alta
- **Alta:** 2026-11-15

## Problema / motivación

El cliente de IA (D134, D145) conversa, pero no puede actuar: leer un fichero, mirar el estado del sistema o proponer un cambio. Un agente
(modelo + herramientas + bucle) lo hace, pero un modelo que escribe y ejecuta es un riesgo: la persona tiene que ver cada acción y el
contenido externo (ficheros, web, correo) no puede darle órdenes.

## Propuesta

- `ia_turno` (llamada a herramientas de la API OpenAI, streaming y degradación a chat), lector de JSON propio.
- Biblioteca de herramientas y registro para que cada aplicación añada las suyas (`ag_herr_registrar`).
- Política de seguridad separada y comentada (`agente_politica.c`): nunca root, confirmación en cada acción, rutas prohibidas, datos externos
  marcados, mancha por tarea, límites, respaldo y registro.
- Agentes como ficheros `.agente` (4 de serie en `/etc/agentes`), `ia --agente`, panel gráfico reutilizable (Chat IA y Mico).
- Guía `docs/agentes-ia.md`.

## Criterios de aceptación

- [x] `tool_calls` también en streaming, con argumentos partidos; sin herramientas se degrada a chat
- [x] Leer sin preguntar; escribir/ejecutar/web piden confirmación con la orden o el diff; denegar no hace nada
- [x] `admin` rechazado; rutas prohibidas rechazadas; límite de pasos; registro
- [x] `ia --agente`, Chat IA (modo agente), Mico (pestaña Agente), crear y editar agentes, ver el registro
- [x] Test automático `tests/h295_agentes_ia.py` (+ servidor falso y unidad en el PC) y capturas `H295-*.png`
- [x] Decisión D295 y guía
- [x] Panel de agente en Pluma, Macaco y Correo con herramientas propias (D298, prueba `h298_agentes_apps.py`, capturas `H298-*.png`)

## Notas

Revisión de seguridad pendiente (la hará Opus). Puntos a mirar: la mancha por tarea (se limpia con cada tarea nueva), la comprobación de
órdenes por palabras (mejor esfuerzo) y la sesión de administrador (se avisa pero no se rechaza).

## Integración en aplicaciones (D298)

- **Pluma:** leer documento o selección (texto y estructura), insertar, reemplazar, formato, WordArt; confirmación con el texto; un solo Ctrl+Z; acciones rápidas en el clic derecho.
- **Macaco:** leer rango (400 celdas), escribir (80 celdas) con lista de celdas y valores, formato, crear hoja, ordenar; Ctrl+Z de una vez; «Explicar estos datos» y «Crear fórmula para...».
- **Correo:** leer mensaje y carpeta, borrador (abre la ventana de redacción, solo al remitente), mover con confirmación; **sin envío**, contenido marcado como datos externos,
  agente sin `ejecutar_orden`/`escribir_fichero`/`buscar_web` y panel que solo ofrece agentes `correo_*`.
- Panel con **Ver > Asistente IA**, botón del robot y **F12**; agentes de serie `asistente-pluma`, `asistente-macaco` y `asistente-correo` en `/etc/agentes`.
- API nueva y compatible: `ag_panel_tarea`, `ag_panel_filtro`, `ag_panel_ocupado`, `ag_app_opciones`, `ag_arg_bool`. Detalle en `docs/agentes-ia.md` y D298.
Revisión de seguridad hecha (D295, «Revisión de seguridad»): la mancha dura la conversación; lo que se enseña es lo que se hace (sin
controles, líneas partidas, sin recortes callados, huella del fichero); nada oculto en `/home` y `/tmp`; `abrir_aplicacion` solo abre
aplicaciones; la web siempre pregunta; como administrador, lo lanzado corre como `sagui`; el entorno de las órdenes va sin secretos. El
filtro de órdenes por palabras sigue siendo de mejor esfuerzo (no es la barrera).
