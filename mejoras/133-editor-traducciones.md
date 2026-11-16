# 133 — Editor de traducciones propio, al estilo de Poedit (v8.4.4)

- **Estado:** pendiente (v8.4.4)
- **Prioridad:** media-alta
- **Alta:** 2026-11-15

## Petición

El mantenedor preguntó si se podía tener Poedit (https://github.com/vslavik/poedit). Portarlo no es viable:
es C++ sobre wxWidgets, que en Linux dibuja con GTK (GLib, Cairo, Pango, HarfBuzz), y además usa Boost, ICU,
Lucene++ y gettext. Acordado el 2026-11-15: **una aplicación propia que haga lo útil de Poedit**.

## Propuesta

Una aplicación nativa (libsagui) o en VisualPythonS, lo que encaje mejor con la biblioteca de catálogos de D313:

- **Abrir** los `.po` y `.pot` del sistema (`idiomas/*/…`) y los de cualquier aplicación; recientes.
- **Lista de cadenas** con columnas «original» y «traducción», con filtros (sin traducir, dudosas/fuzzy,
  traducidas, todas) y búsqueda en el original, la traducción y el contexto.
- **Editor de la cadena:**
  - contexto (`msgctxt`), comentarios del traductor y de extracción, y referencias al código;
  - plurales (`msgid_plural`/`msgstr[n]` según `Plural-Forms`);
  - marcar o desmarcar como dudosa.
- **Validación**, al guardar y en vivo:
  - `%s`/`%d`/`%1$s`, `\n` y `\t`, atajos (`&` o `_`) y etiquetas, en el mismo número y orden que el original;
  - la longitud, con aviso si no cabe en la interfaz;
  - la cabecera del catálogo.
- **Actualizar desde el `.pot`**, como `msgmerge`: las cadenas nuevas, las que desaparecen (obsoletas) y las
  cambiadas como dudosas.
- **Generar** el catálogo que carga el sistema y recargarlo en caliente, si D313 lo permite, para ver la
  traducción sin reiniciar.
- **Estadísticas** por catálogo: porcentaje traducido, dudosas y sin traducir.
- **Sugerencias**: memoria de traducción (cadenas iguales o parecidas ya traducidas en otros catálogos) y,
  opcionalmente, el agente de IA del sistema (`ia`), siempre como propuesta que el traductor acepta.
- Su propia interfaz traducida (ES / PT-BR / EN). Puede usar la cinta de la mejora 132 cuando exista.

Sirve para el punto «multiidioma en el resto de aplicaciones» de la 8.4.4: traducir desde el propio SaguiOS.

## Pruebas

- Una prueba hNN que:
  - abra un `.po` de prueba, traduzca dos cadenas y marque una como dudosa;
  - provoque un error de `%s` que la validación detecte;
  - actualice desde un `.pot` con una cadena nueva y una obsoleta;
  - guarde y compruebe el fichero con `msgfmt --check` en el anfitrión, si existe.
- Capturas.
