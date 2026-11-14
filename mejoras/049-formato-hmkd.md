# 049 - HMKD: un «estándar web» propio, Markdown con imágenes y vídeo

- **Estado:** propuesta
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Depende de:** 019/021 (visor Markdown), 047 (JPEG), 048 (vídeo), 020/027 (HTTP/HTTPS)
- **Relacionadas:** D047 (Cipó)

## Idea

**HMKD** («Hiper-Markdown», extensión `.hmkd`, tipo `text/hmkd`) es un formato de páginas
pensado para SaguiOS: **Markdown** (lo que ya entiende el visor de la 019), más **imágenes**,
**vídeo embebido** y **enlaces entre páginas** locales o por la red. Es la «web» de SaguiOS:
- ligera, para PC antiguos;
- legible como texto plano;
- servible con cualquier servidor HTTP;
- y se escribe con Mico o el Editor.

## Especificación (borrador; la versión final irá en `docs/hmkd.md`)

- **Base:** el subconjunto de CommonMark que ya soporta el visor (títulos, párrafos, énfasis,
  listas, citas, código, tablas y enlaces). Codificación **UTF-8** en el archivo; el visor
  convierte a Latin-1 para la pantalla.
- **Cabecera opcional** (*front matter*):
  ```
  ---
  titulo: Mi página
  idioma: es
  ---
  ```
- **Imágenes:** `![texto alternativo](foto.jpg)`, con atributos opcionales entre llaves:
  `![sagüí](sagui.png){ancho=320 alinear=centro}`. Formatos: BMP, PNG, PPM y JPEG (047).
- **Vídeo** (048):
  ```
  ::video[Descripción](clip.mpg){ancho=426 bucle silencio auto}
  ```
  Se muestra un recuadro con el primer cuadro y un botón ▶. `auto` reproduce al
  mostrarse (por defecto, no). Formatos `.mpg` (MPEG-1) y `.avi` (MJPEG).
- **Enlaces:**
  - relativos (`otra.hmkd`, `../indice.hmkd#seccion`);
  - absolutos (`http://` y `https://`, que se abren en el mismo visor si son `.hmkd`, o en
    Cipó si son HTML);
  - `archivo:/home/...` para archivos locales.
- **Bloques extra, opcionales y bien acotados:**
  - `::nota[texto]`, `::aviso[texto]`: recuadros de color;
  - `::galeria` ... `::` : una rejilla de imágenes.
  - Nada de scripts ni CSS: el formato es **estático y seguro** por diseño.
- **Robustez:**
  - todo lo desconocido se muestra como texto, sin errores;
  - límites de tamaño por recurso;
  - por defecto, los recursos remotos solo por HTTPS validado (027);
  - una página no puede leer archivos locales salvo que esté guardada en local.

## Implementación

- **Visor/navegador:**
  - el visor Markdown (`markdown`) aprende HMKD: imágenes en línea, el recuadro de vídeo con
    la API de la 048, y la navegación con atrás/adelante e historial;
  - **Cipó** abre `.hmkd` (por extensión o por `Content-Type: text/hmkd`) con el mismo motor;
  - un **componente de la libsagui** (`hmkd_vista`) para que cualquier programa
    (ayuda, Mico, tkinter) muestre HMKD.
- **Editor:** Mico con resaltado de HMKD y vista previa dividida, como el Markdown actual.
- **Servir:**
  - **`servidor`**, un servidor HTTP estático mínimo en SaguiOS (`servidor /home/web 8080`),
    para publicar una carpeta HMKD en la red local;
  - en el PC basta con `python3 -m http.server`.
- **Ayuda del sistema en HMKD:** `/ayuda` puede migrar a HMKD con capturas y vídeos cortos
  de ejemplo.
- **Herramientas en el PC:** `tools/hmkd2html.py` (opcional) para ver las páginas en un
  navegador normal (`<img>` y `<video>`).

## Criterios de aceptación

- [ ] `docs/hmkd.md` con la especificación y ejemplos
- [ ] Visor: imágenes (con JPEG), vídeo embebido reproducible, enlaces locales y remotos,
      atrás y adelante
- [ ] Cipó abre `.hmkd` por HTTP y HTTPS; `servidor` publica una carpeta
- [ ] Páginas de ejemplo propias en `/ejemplos/hmkd/` (con imágenes y un vídeo propios)
- [ ] Test automático: páginas de prueba renderizadas (capturas) y navegación, contra un
      servidor del anfitrión
- [ ] Decisión registrada en `docs/decisoes.md`
