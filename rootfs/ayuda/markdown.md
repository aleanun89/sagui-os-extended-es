# Markdown: visor y editor

`markdown` abre documentos escritos en **Markdown**, un formato de texto
sencillo en el que unos pocos signos (`#`, `*`, `-`, `>`...) indican títulos,
listas o énfasis. El programa los muestra ya formateados y también permite
editarlos.

## Cómo abrirlo

- Desde el terminal: `markdown archivo.md`
- Desde **Archivos**: doble clic en cualquier archivo `.md`
- Sin archivo (`markdown`) empieza un documento nuevo en el editor.

Para ver un documento directamente en el terminal, ya maquetado:

    markdown -t archivo.md [columnas]

## Teclas

| Tecla              | Qué hace                                  |
|--------------------|-------------------------------------------|
| Ctrl+E o F5        | Alterna entre el visor y el editor        |
| Ctrl+S             | Guarda (pide la ruta si no tiene nombre)  |
| Ctrl+O             | Abre otro archivo                         |
| Flechas, RePág, AvPág | Desplazan el documento                 |
| Inicio, Fin        | Principio o final (en el editor, de la línea; con Ctrl, del texto) |

El botón **Dividir** muestra el editor a la izquierda y la vista previa a la
derecha, que se actualiza mientras escribes. La barra de la derecha se puede
arrastrar o pulsar para pasar de página, y la **rueda del ratón** desplaza el
panel que esté debajo del cursor (tres líneas por muesca).

La ventana se puede **redimensionar** arrastrando el borde o la esquina, y
**maximizar** con el botón de la barra de título o con un doble clic en ella:
el documento se vuelve a maquetar con el ancho nuevo.

## Enlaces

Un clic en un enlace a otro archivo `.md` (por ejemplo,
[el ejemplo](../ejemplos/ejemplo.md)) lo abre en el visor. Los enlaces a
páginas web solo se muestran: SaguiOS no tiene red.

## Qué Markdown entiende

1. Títulos `#` a `######` (y los subrayados con `===` o `---`)
2. Párrafos, **negrita**, *cursiva*, ~~tachado~~ y `código`
3. Listas con `-`, `*`, `+` o números, anidadas por sangría, y tareas `- [ ]`
4. Bloques de código con ```` ``` ```` o con cuatro espacios
5. Citas con `>`, reglas `---`, tablas con `|` y escapes con `\`
6. Imágenes `![texto](archivo)`, que se muestran como *[imagen: texto]*

Al guardar, el archivo se graba en UTF-8, el formato habitual de Markdown.
