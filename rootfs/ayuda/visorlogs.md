<!-- categoria: Juegos y aplicaciones; orden: 14; titulo: Visor de registros -->
# Visor de registros

El **Visor de registros** (Inicio > Sistema > Visor de registros, o `visorlogs` en el Terminal) muestra el registro del
kernel (lo que `dmesg` imprime) o cualquier archivo de texto, con búsqueda, filtros y colores. Sirve para saber qué ha
pasado en el sistema: qué driver se ha cargado, por qué falla una unidad, qué ha escrito un programa con `registrar()`.

## Qué se ve

- A la izquierda, las **categorías** con su recuento: Todas, Incidencias (avisos y errores), GPU, USB, Disco, Red, Energía,
  Audio, Aplicaciones y Sistema. Cada una reúne las etiquetas reales del kernel: `[GPU3D]`, `[ACEL2D]`, `[USB]`, `[XHCI]`,
  `[AHCI]`, `[REDE]`, `[WIFI]`, `[ENERGIA]`, `[OCIO]`, `[HDA]`, `[AOEN]`, `[SDL2]`, `[RASTRO]`, `[GUI]`...
- En el centro, las líneas, con su número. Los **colores** indican el nivel: texto normal, **ámbar** para avisos
  (`[aviso]`, `timeout`...), **rojo** para errores (`error:`, `fallo`, `denegado`...) y rojo en negrita con fondo rosado para
  lo crítico (pánico, excepciones, fallos de página). Los colores salen del esquema de Configuración > Estilos, también con
  los esquemas oscuros.
- Arriba, el cuadro **Buscar** (sin distinguir mayúsculas), que se combina con la categoría y el proceso elegidos.
- Abajo, el estado: líneas visibles y si el registro se sigue en vivo.

## Seguir en vivo

El visor sigue el registro del kernel (y un archivo que crece, como `/logs/kernel-....log` o el de un servidor) cada medio
segundo: las líneas nuevas aparecen solas y, si estás al final, la lista baja con ellas. Si subes a leer, se queda donde
estás. El botón **Pausa** detiene el seguimiento (y pasa a llamarse **Seguir**); también F6.

## Filtrar por proceso

Las líneas que escriben los programas con `registrar()` empiezan por `[nombre pid]`. El botón **Proceso** abre la lista de
procesos que hay en el registro: elige uno para ver solo sus líneas, o «Todos los procesos» para quitar el filtro.

## Copiar y exportar

- Un **clic** selecciona una línea; **Mayús+clic** una serie; **Ctrl+A** todas las visibles. **Copiar** (o Ctrl+C) las lleva
  al portapapeles.
- **Exportar** (Ctrl+E) guarda en un archivo las líneas que se ven (con la categoría, la búsqueda y el proceso aplicados).
- **Abrir** (Ctrl+O) elige un archivo; **/logs** abre el selector en la carpeta de registros del disco (si hay una partición
  SAGUI-LOGS montada); **Kernel** vuelve al registro del kernel; **Recargar** (F5) lo lee de nuevo.

## Atajos

| Tecla | Acción |
|-------|--------|
| Ctrl+F | ir al cuadro Buscar (Intro vuelve a la lista) |
| F5 / F6 | recargar / pausar o seguir |
| Ctrl+C, Ctrl+A | copiar la selección, seleccionarlo todo |
| Ctrl+E, Ctrl+O | exportar, abrir |
| Flechas, RePág, AvPág, Inicio, Fin | recorrer; izquierda y derecha desplazan las líneas largas |
| Rueda | desplaza de tres en tres líneas |
| Esc | cierra |

El visor conserva hasta 20 000 líneas y lee hasta los últimos 2 MiB de un archivo.

## Desde el Terminal

    visorlogs                              abre el registro del kernel
    visorlogs /logs/kernel-20261122.log    abre un archivo
    visorlogs --filtro gpu                 empieza con la categoría GPU (todas, incidencias, gpu, usb, disco,
                                           red, energia, audio, apps, sistema)
    visorlogs --proceso buscaminas         solo las líneas de ese proceso
    visorlogs --buscar error --exportar /tmp/errores.txt
                                           sin ventana: escribe en el archivo las líneas filtradas

## Para aprender

La primera versión, sencilla, de este programa está en `/ejemplos/tcc/visorlogs.c` (unas 500 líneas de C, para compilar con
`tcc`). La guía de C de la Ayuda (Programación > C) la explica en «Programas completos para estudiar».
