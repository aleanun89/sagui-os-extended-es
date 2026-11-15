# Idiomas: cómo marcar los textos y cómo traducir (D313, mejora 118)

SaguiOS-Extended se muestra en **español** (el original), **portugués de Brasil** (`pt_BR`) y **inglés** (`en`). Los textos
están en español dentro del código y se traducen con catálogos; si falta una traducción sale el español.

## Elegir el idioma

- **Configuración > Teclado > Idioma de la interfaz**: Español, Português (Brasil) o English. Vale en el acto para los programas que
  abras después y para el escritorio (barra, menú Inicio, iconos); las ventanas ya abiertas conservan el suyo hasta cerrarlas.
  Se guarda en `/home/.config/sagui.conf` (`idioma=en`) y se aplica en cada arranque.
- Línea de comandos: `configuracion idioma en` (o `pt_BR`, `es`); como root, `configuracion idioma pt_BR --sistema` fija el del
  sistema (`/etc/idioma`), que usa quien no tiene uno propio.
- `LANG` en el entorno manda sobre todo lo demás para ese programa: `LANG=en arquivos`.
- `LANG=xx` (o `configuracion idioma xx`): **pseudo-idioma** de pruebas, cada cadena entre corchetes y un 30 % más larga.

## Marcar los textos en el código

```c
g_texto(j, 24, 40, _("Guardar como"), COR_TEXTO);                 // se traduce en el momento
formatar_texto(msg, sizeof msg, _("%d elementos"), n);             // con formato: se traduce la plantilla
static const char *const rot[] = { N_("Abrir"), N_("Cerrar") };    // tabla estática: solo se marca...
g_texto(j, x, y, _(rot[i]), cor);                                  // ...y se traduce al usarla
```

Reglas:

- La cadena en español **es** la clave: si se cambia una letra, la traducción se pierde (queda en el `.po` viejo hasta que se borra).
- Marca la **frase entera** (con sus `%d`, `%s`), no trozos que se peguen luego: el orden de las palabras cambia de un idioma a otro.
  Para elegir entre dos frases, marca las dos (`cond ? _("Sí") : _("No")`), no `"%s"` con una palabra suelta.
- Texto con menús del kernel: las marcas especiales (`>`, `<`, `-`, `~`, `*`, `\t`) van **fuera** del `_()`, salvo el atajo tras `\t`,
  que forma parte de la frase.
- No marques lo que no es texto de la interfaz: rutas, órdenes, claves de configuración, nombres de archivo, `klog`/`registrar`
  (el `dmesg` y los registros se quedan en español).
- Los botones (`botao_t`), `confirmar`, `mensagem` y `pedir_texto` traducen su texto por sí mismos; aun así escribe `_()` en el literal
  (o `N_()` en una inicialización estática) para que el extractor lo vea.
- Ancho: las traducciones suelen ser más largas. Prueba con `LANG=xx` y con `SAGUI_GUI_REVISAR=1` (D300) para ver lo que no cabe.
- Números y fechas: `formatar_decimal_regional`, `formatar_inteiro_regional`, `formatar_data_regional`, `regional_decimal()`
  (libsagui, `regional.c`).

En el kernel: `#include "idioma.h"` y lo mismo (`_()`, `N_()`).

## Dónde va cada texto (qué catálogo)

| Fuente | Catálogo |
|---|---|
| `userland/programas/X.c` | `X` (`/sistema/idiomas/<idioma>/X.cat`, el nombre del ejecutable) |
| `userland/libsagui/*.c`, `userland/inc/*.h` | `libsagui` (los lee todo programa, tras el suyo) |
| `kernel/gui/*.c` y los avisos de `acpi.c`, `supervisor.c`, `energia.c` | `kernel` |

Un programa nuevo en otra carpeta o con otro nombre de ejecutable se añade en `catalogo_de()` y `BUSCAR` de
`tools/extraer_textos.py`.

## Traducir

Los `.po` están en `idiomas/<idioma>/<catálogo>.po` (UTF-8, formato estándar; `idiomas/plantilla/*.pot` es la plantilla).

1. `python3 tools/extraer_textos.py extraer` busca las marcas y añade lo nuevo a cada `.po` con `msgstr ""` (conserva lo ya traducido y
   quita lo que ya no está en el código).
2. Traduce con **Poedit** (o Weblate, o un editor de texto): abre `idiomas/pt_BR/arquivos.po`, escribe cada `msgstr`. Una traducción
   vacía o marcada `fuzzy` cuenta como pendiente. Mantén los `%d`, `%s`, `\n` y los espacios del principio y del final.
   Criterio: la interfaz de Windows en cada idioma («Arquivo, Editar, Exibir» / «File, Edit, View»).
3. `python3 tools/extraer_textos.py estadisticas` (cuántas hay por idioma y catálogo) y `python3 tools/extraer_textos.py comprobar
   configuracion arquivos` (lista lo que falta; sale con 1 si hay algo).
4. `make` genera los `.cat` (`build/idiomas/`) y los pone en el ramdisk (`/sistema/idiomas`). No hace falta nada más.

Un idioma nuevo: crea `idiomas/<código>/`, añade el código a `IDIOMAS` en `tools/extraer_textos.py`, ejecuta `extraer`, traduce y
añade su botón en Configuración (`b_idi` en `configuracion.c`). Los textos van en **Latin-1**: caben es, pt, en, fr, de e it;
cirílico, griego o CJK necesitan la fase 3 (UTF-8 interno).

## Pruebas

`python3 tests/h315_idiomas.py` (lector de catálogos, `.po` completos, capturas en `en`, `pt_BR` y `xx`, cambio de idioma desde la
ventana, reinicio). Con `LANG=xx` y los registros: lo marcado que falta sale como `[IDIOMA] sin traducir (idioma): texto` (una vez
por cadena) en el puerto serie.
