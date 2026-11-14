# Ejemplo de Markdown

Este documento muestra lo que sabe dibujar el visor **Markdown** de SaguiOS:
texto con *cursiva*, **negrita**, ***las dos a la vez***, ~~tachado~~ y
`código en línea`. Los acentos (á, é, í, ó, ú, ñ, ¿?, ¡!) se ven bien porque el
texto se convierte de UTF-8 a Latin-1 al abrir el archivo.

Una línea que termina con dos espacios  
fuerza un salto de línea.

## Listas

- Primer elemento
- Segundo elemento, con una lista dentro:
  - anidada con sangría
  - otra más
    + y un tercer nivel
- Tercer elemento

1. Uno
2. Dos
3. Tres

### Tareas

- [x] Escribir el visor
- [x] Escribir el editor
- [ ] Ventanas redimensionables

## Código

Un bloque entre vallas:

```c
int principal(int argc, char **argv)
{
    imprimir("hola, %s\n", argv[0]);
    return 0;
}
```

Y otro con sangría de cuatro espacios:

    $ markdown -t /ejemplos/ejemplo.md

## Citas

> Una cita puede tener **énfasis** y ocupar
> varias líneas.
>
> > Y también puede haber citas dentro de citas.

## Tabla

| Elemento   | Sintaxis      | Nivel |
|:-----------|:-------------:|------:|
| Título     | `# Título`    |     1 |
| Subtítulo  | `## Sub`      |     2 |
| Lista      | `- elemento`  |     - |

## Enlaces e imágenes

Consulta la [ayuda del programa](../ayuda/markdown.md) o visita
<https://commonmark.org> (los enlaces externos solo se muestran).

![El sagui en la rama](sagui.png)

---

Escapes: \*esto no es cursiva\* y \# esto no es un encabezado.

#### Encabezado de nivel 4
##### Encabezado de nivel 5
###### Encabezado de nivel 6

Fin del ejemplo.
