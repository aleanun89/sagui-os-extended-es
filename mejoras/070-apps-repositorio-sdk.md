# 070 - Aplicaciones extra en /app, repositorio en red y SDK para crear apps (también con IA)

- **Estado:** idea para el **futuro** (sin versión; planteada por el mantenedor el 2026-11-08)
- **Prioridad:** media-alta (es lo que convierte SaguiOS en una plataforma)
- **Alta:** 2026-11-08
- **Relacionadas:** 068 (particiones), 035 (bibliotecas compartidas), 027 (TLS), 064/067 (SQLite, lite-monkey), 065 (web en Python), 043 (IA)

## Objetivo

1. **`/app`**: carpeta de **aplicaciones extra**, fuera de la imagen del sistema.
   - Con partición persistente (`SAGUI-APPS`, o `/home/.apps` como reserva), lo instalado se
     conserva.
   - Sin disco, `/app` vive en RAM y se pierde al reiniciar, y el sistema lo avisa.
2. **Repositorio de apps en red**: un servidor (en Python, que corre en cualquier PC o en la
   nube) del que SaguiOS descarga, instala, actualiza y desinstala apps con un comando y una
   ventana.
3. **SDK de compilación cruzada**: crear apps en un PC normal, probarlas en QEMU y publicarlas.
   Pensado para que **otra IA** pueda hacer apps de forma autónoma y segura.

## 1. Paquete de aplicación (`.sap`)

- Un `.tar.gz` (la 056 ya lo lee) con:
  - **`app.toml`**, el manifiesto:
    - `id` (p. ej. `es.aanun.notas`), `nombre`, `version` (semver), `descripcion`, `autor`,
      `licencia`;
    - `tipo`: `nativa` (ELF de SaguiOS), `python` (MicroPython) o `mixta`;
    - `requiere` (versión mínima de SaguiOS y del **ABI**, módulos de Python, otras apps);
    - `ejecutable`, icono, entradas de menú y asociaciones de tipos de archivo;
    - `permisos` declarados: red, disco fuera de su carpeta, audio, servidor en un puerto.
      Al principio solo informativos y mostrados al instalar; más adelante, aplicados.
  - `bin/`, `lib/` (Python), `share/` (datos, iconos, ayuda en Markdown).
- **Instalación:** `/app/<id>/<version>/` y un enlace `actual`, que permite volver a la versión
  anterior. Los datos del usuario van en `/home/.app-datos/<id>/`, nunca dentro de `/app`.
- **ABI:**
  - hoy los programas enlazan la libsagui de forma estática, así que una app nativa es
    autónoma;
  - se fija un **número de versión del ABI** (llamadas al sistema y estructuras de `abi.h`) y
    el instalador rechaza apps de un ABI incompatible;
  - con bibliotecas compartidas (035), las apps se harán más pequeñas.

## 2. Gestor en SaguiOS: `apps`

- **Terminal:** `apps buscar`, `apps info`, `apps instalar`, `apps quitar`, `apps actualizar`,
  `apps lista`, `apps volver` (versión anterior) y `apps repos` (añadir o quitar repositorios).
- **Ventana «Tienda de apps»:** catálogo con iconos, capturas, búsqueda, instalar y quitar,
  y actualizaciones pendientes en la bandeja.
- **Integración:** PATH con `/app/*/actual/bin`, entradas en el menú Inicio («Apps»), iconos y
  asociaciones con Archivos («Abrir con»).
- **Seguridad:**
  - descarga por **HTTPS** (libtls);
  - **índice firmado** por el repositorio con Ed25519 (que habría que añadir a la libtls) o
    RSA-PSS (que ya está para verificar certificados), con la clave pública en `/etc/apps/`;
  - SHA-256 de cada paquete en el índice;
  - nunca se instala nada sin firma válida salvo con `--sin-firma` y un aviso rojo.
- **Instalación atómica:** se descomprime en una carpeta temporal y se mueve con
  `renombrar`. Si falla a medias, no queda nada roto.

## 3. Servidor de repositorio (Python, en otro equipo)

- `tools/repo-apps/servidor.py`: CPython con la biblioteca estándar, sin dependencias.
  Incluye:
  - un índice JSON (`indice.json` y su firma);
  - los paquetes como archivos estáticos;
  - una **API de subida** con token por autor;
  - una página web sencilla con el catálogo.
- Al subir un paquete: se valida el manifiesto y el formato, se **prueba automáticamente en
  QEMU** (arranca SaguiOS, instala la app, la ejecuta, comprueba que no se cuelga y hace una
  captura) y, si pasa, se firma el índice y se publica.
- Se puede montar en el PC de casa, en una Raspberry o en la nube. El repositorio oficial es
  uno más de la lista de `apps repos`.
- Más adelante podría servirse desde el propio SaguiOS con Microdot (065).

## 4. SDK de compilación cruzada (`sagui-sdk`)

- Paquete para Linux/WSL (y un contenedor Docker con todo listo) que contiene:
  - las **cabeceras** de `userland/inc`, la **libsagui** compilada (`libsagui.a`), el
    `programa.ld` y los flags exactos (`-m32 -march=i686 -ffreestanding ...`): el mismo gcc
    del sistema hace de compilador cruzado;
  - el entorno de **MicroPython** para apps Python: los módulos de SaguiOS y un validador de
    lo que no existe en MicroPython;
  - **plantillas**: app de terminal en C, app de ventana en C (gui.h), app Python con
    `sagui`/`tkinter` y servidor de red;
  - **`sagui-sdk crear | compilar | probar | empaquetar | comprobar | publicar`**:
    - `probar` arranca SaguiOS en QEMU con la app instalada y, si se le pide, ejecuta un
      guion de prueba como los `tests/` del proyecto, con QMP y capturas;
    - `comprobar` valida: ELF de 32 bits correcto, sin instrucciones que la CPU mínima no
      tenga (SSE2 opcional), tamaño, manifiesto, licencia y llamadas al sistema de un ABI
      conocido.
- **Versionado:** cada SDK corresponde a una versión de SaguiOS y del ABI. Se genera
  automáticamente al publicar cada versión (`entrega/vX/sagui-sdk-vX.tar.gz`).
- **Compilar dentro de SaguiOS** también: `cc` y Pascal ya existen; con varios `.c` y
  bibliotecas (v6.2), `sagui-sdk` podría tener también una versión mínima dentro del propio
  sistema.

## 5. Que otra IA pueda hacer apps (en serio)

Lo que hace falta para que un agente de IA cree apps solo y bien:
- **Documentación pensada para agentes** en el SDK (`AGENTES.md`):
  - la API de la libsagui y de gui.h, con ejemplos que compilan;
  - las limitaciones: 32 bits, sin hilos, MicroPython en vez de CPython, Latin-1 en la
    interfaz;
  - las convenciones del proyecto;
  - una lista de «no hacer».
  - La referencia de la API se **genera desde las cabeceras**, así nunca se desactualiza.
- **Bucle cerrado sin humanos:** el agente escribe el código, `sagui-sdk probar` le devuelve el
  resultado (salida, capturas, fallos) y repite hasta que pasa. El mismo método con el que se
  ha construido SaguiOS con `tests/`.
- **Guardarraíles:**
  - la app solo ve su carpeta y la API pública;
  - el repositorio exige que pase las pruebas automáticas en QEMU antes de publicar;
  - revisión humana opcional para el canal «estable» y canal «pruebas» para lo no revisado;
  - límites de tamaño, y prohibido red o disco no declarados en `permisos` (cuando se
    apliquen).
- **Plantilla de encargo:** un prompt o ficha estándar («haz una app que...») con criterios de
  aceptación comprobables, como las fichas de `mejoras/`.

## Fases propuestas

1. **Formato `.sap`, `/app` y `apps instalar` desde archivo local**, con la partición
   `SAGUI-APPS` en el generador de imágenes (068).
2. **SDK** (cabeceras, `libsagui.a`, plantillas, `compilar`/`probar`/`empaquetar`) y su
   generación con cada versión.
3. **Servidor de repositorio** en Python e índice firmado, con `apps buscar/instalar/actualizar`
   por HTTPS.
4. **Tienda gráfica**, integración con el menú y Archivos, actualizaciones en la bandeja.
5. **Pruebas automáticas en el servidor** y `AGENTES.md` para la creación de apps con IA.
6. **Permisos aplicados por el kernel**: necesita una raíz por proceso y jails, que se
   acercan a la 052 y la 035.

## Criterios de aceptación

- [ ] Instalar, actualizar, volver y quitar una app nativa y una Python, con y sin disco
      persistente
- [ ] Índice firmado y paquetes verificados; un paquete manipulado se rechaza
- [ ] SDK: una plantilla se compila en el PC, se prueba en QEMU y se publica en un repositorio local
- [ ] Una IA crea una app a partir de una ficha usando solo el SDK y `AGENTES.md`
- [ ] Tests automáticos en `tests/`
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

- Modelo sugerido: **Opus** para el diseño, la firma y el ABI; **Sonnet** para el servidor, el
  SDK, la tienda y las plantillas.
- Sin código de terceros: firmas con la libtls propia. Si se añade Ed25519, propio o con una
  implementación de dominio público registrada en `docs/terceros.md`.
