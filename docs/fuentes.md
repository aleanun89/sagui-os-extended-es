# Fuentes: instalarlas y usarlas (D356)

SaguiOS dibuja fuentes TrueType y OpenType (CFF) con la libfontes. Pluma y Macaco ofrecen **todas** las instaladas en su selector de fuente
(con buscador, cada nombre dibujado con la suya), las usan en pantalla, las incrustan en PDF y Macaco las guarda por su nombre en XLSX y ODS.

## Dónde viven

| Carpeta | Quién | Notas |
|---|---|---|
| `/fontes` | la base (DejaVu Sans, Serif, Mono) | en el ramdisk; no se quitan |
| `/apps/fuentes` | **todas las cuentas** | partición SAGUI-APPS (disco, persistente); aquí van las 28 Typodermic; hace falta ser administrador |
| `/home/fuentes` (o `$HOME/fuentes`) | **la cuenta** | sin root; persiste si `/home` tiene partición |

## Instalar

- **Archivos:** doble clic en un `.ttf` o `.otf` abre la vista previa con el botón **Instalar** (para tu cuenta) o **Instalar para todos**.
- **Configuración > Fuentes > Instalar fuentes...** abre la ventana de fuentes.
- **Ventana:** `fuentes --ventana`: lista con muestra, Instalar..., Carpeta..., Quitar, Actualizar y «Para todas las cuentas».
- **Terminal:**
  - `fuentes` — las familias instaladas y su origen
  - `fuentes instalar ARCHIVO.ttf|.otf|CARPETA...` — para tu cuenta
  - `admin fuentes instalar --sistema ARCHIVO...` — para todas
  - `fuentes quitar "Familia"` y `admin fuentes quitar --sistema "Familia"`
  - `fuentes actualizar`

Antes de copiar, `fuentes` valida el archivo con la libfontes: uno dañado, truncado o que no sea una fuente se rechaza con el motivo y no se copia nada.

## Los programas abiertos

Los selectores vuelven a leer las carpetas cada vez que se abren: instala con Pluma o Macaco abiertos y la fuente sale en el siguiente
despliegue del combo. Si quitas una fuente, deja de salir en las listas y el texto que la usaba se dibuja como la clásica de su tipo.

## La colección completa de Typodermic

La imagen lleva una selección de 28 (`tools/fuentes_typodermic.py seleccion`). Para las 731: `python3 tools/fuentes_typodermic.py completa
carpeta` y `fuentes instalar --sistema carpeta` (caben hasta 247 familias distintas). El zip (`NEW_FONTS/typodermic-public-domain-*.zip`) no va en
git: sin él (o sin `FUENTES_RED=1`) la imagen no trae ninguna extra.
