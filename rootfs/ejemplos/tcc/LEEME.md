# Cinco programas completos para compilar con tcc

Esta carpeta (`/ejemplos/tcc`) tiene cinco programas en C, sencillos y completos, para estudiarlos,
compilarlos dentro de SaguiOS y cambiarlos. Son la versión **original y sencilla**; Snake y el Visor de
registros del menú Inicio son versiones ampliadas de dos de ellos (están en `userland/programas/`).

| Archivo | Qué es | Qué enseña |
|---------|--------|------------|
| `huella.c` | CRC-32 de archivos o de la entrada estándar | ficheros, bloques, tablas, tuberías |
| `visorppm.c` (+ `muestra.ppm`) | abre una imagen PPM en una ventana | formatos de imagen, píxeles, la libsagui |
| `snake.c` | el juego de la serpiente | bucle de juego, teclado, temporizador |
| `visorlogs.c` | lee el registro del kernel o un archivo y lo filtra | lectura de registros, texto, filtros |
| `lumen.c` | reproductor de vídeo y audio con cola y visualizador | decodificar vídeo y audio con la libsagui, sincronizar el sonido con los cuadros |

## Compilar

Copia la carpeta a tu directorio (así puedes modificarla) y compila:

    cp -r /ejemplos/tcc ~/tcc
    cd ~/tcc
    make SALIDA=/tmp

o uno a uno, con `tcc`:

    tcc -o /tmp/huella huella.c
    tcc -o /tmp/visorppm visorppm.c
    tcc -o /tmp/snake snake.c
    tcc -o /tmp/visorlogs visorlogs.c
    tcc -o /tmp/lumen lumen.c

## Probarlos

    echo -n 123456789 | /tmp/huella      # CBF43926
    /tmp/huella muestra.ppm              # C36CC40B
    /tmp/visorppm muestra.ppm            # Esc cierra; un segundo argumento cierra tras N segundos
    /tmp/snake --prueba                  # comprueba las reglas sin abrir ventana
    /tmp/snake                           # Espacio empieza, flechas o WASD mueven, P pausa, Esc sale
    /tmp/visorlogs                       # el registro del kernel; /tmp/visorlogs archivo.log abre un archivo

    /tmp/lumen video.avi musica.wav      # AVI MJPEG con PCM, WAV...; Espacio pausa, F11 pantalla completa

Cada programa tiene un guion de prácticas en la guía de C de la Ayuda (Programación > C > «Programas
completos para estudiar»).

## Licencia

GPL v3 (`SPDX-License-Identifier: GPL-3.0-only`), como el resto de SaguiOS.
