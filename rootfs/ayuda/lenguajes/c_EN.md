<!-- categoria: Programación/C; orden: 10; titulo: C -->
# C on SaguiOS: your first program

**C** is the language SaguiOS itself is written in: the kernel, the windows and almost all the programs.
It is a small and fast language. Learning it here helps you understand the system from the inside, and lets you
write console programs, games and windowed programs without installing anything else.

SaguiOS has **two C compilers**:

| Command | What it is | When to use it |
|---------|------------|----------------|
| `cc` | SaguiOS's own compiler: one pass, no assembler | to start, console programs |
| `tcc` | the Tiny C Compiler: almost all of C, plus the **libsagui** (windows), SDL 2 and OpenGL ES | windows, games, memory error checks (`-b`) |

Both commands produce an **executable** program that runs by itself, with no extra pieces. For windows, use `tcc`
(`cc` does not have them). Guide to each compiler: [TCC](../tcc.md). Other languages: [Programming on SaguiOS](indice_EN.md).

## Before you start

- Open the **Terminal** (that is where you type commands).
- To write the code, use **Mico** (the code editor). Pluma is a word processor: it saves RTF by default and is not
  the best choice for programs. Mico colors C and, with **F5**, compiles and runs the program (it uses `cc`).
  See [Mico](../mico.md).
- Create a folder for your programs, for example with `mkdir c` and go into it with `cd c`.

## Your first program, step by step

**1.** In the Terminal, inside the `c` folder, open Mico with the new file:

    mico hola.c

**2.** Type this in Mico:

```c
/* hola.c: el primer programa en C */
#include <stdio.h>

int main(void)
{
    printf("Hola, mundo desde C en SaguiOS\n");
    printf("2 + 3 = %d\n", 2 + 3);
    return 0;
}
```

**3.** Save with **Ctrl+S**.

**4.** Compile and run it. In the Terminal type:

    cc hola.c -o hola
    ./hola

You will see the two lines. If you see no error, `cc` has created the program `hola` in your folder. The `./` in
front of the name matters: it means "the program in this folder".

**5.** To compile and run in one step, without creating an executable file: `tcc -run hola.c`.

**Shortcut in Mico:** with the file open, **F5** does the same as steps 4 (it compiles with `cc` and runs the
program in the panel below).

## The basics of the language

### Variables and types

```c
/* tipos.c */
#include <stdio.h>

int main(void)
{
    int edad = 12;              /* número entero */
    double altura = 1.45;       /* número con decimales */
    char letra = 'S';           /* un carácter */
    char nombre[] = "Tito";     /* una cadena: un array de caracteres */

    printf("%s tiene %d anos y mide %.2f m\n", nombre, edad, altura);
    printf("su inicial es %c\n", letra);
    return 0;
}
```

In `cc`, `long` is the same as `int`. `long long` does not exist. Numbers with decimals are `float` and `double`.
To print them: `%d` (integer), `%f` or `%.2f` (decimals), `%s` (string), `%c` (character).

### Conditions and loops

```c
/* bucles.c */
#include <stdio.h>

int main(void)
{
    int i;
    for (i = 1; i <= 5; i++)
        printf("%d x 2 = %d\n", i, i * 2);

    int n = 3;
    while (n > 0) {
        printf("cuenta atras: %d\n", n);
        n--;
    }

    if (n == 0)
        printf("despegue!\n");
    else
        printf("todavia no\n");
    return 0;
}
```

`switch`, `break` and `continue` also work. `goto` does not exist in `cc`.

### Functions

```c
/* funciones.c */
#include <stdio.h>

int cuadrado(int x)
{
    return x * x;
}

int main(void)
{
    printf("el cuadrado de 7 es %d\n", cuadrado(7));
    return 0;
}
```

A function must be written before it is used (or declared above, like `int cuadrado(int x);`).

### Arrays and strings

```c
/* arrays.c */
#include <stdio.h>
#include <string.h>

int main(void)
{
    int notas[4] = { 7, 9, 5, 10 };
    int suma = 0;
    for (int i = 0; i < 4; i++)
        suma += notas[i];
    printf("media: %d\n", suma / 4);

    char saludo[32];
    strcpy(saludo, "hola");
    strcat(saludo, " amigo");
    printf("%s (%d letras)\n", saludo, (int)strlen(saludo));
    return 0;
}
```

Indexes start at 0. C **does not check** that you stay inside an array: if you write outside, the program fails in
strange ways. With `tcc -b` (see below) it tells you.

### Pointers and memory

```c
/* punteros.c */
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int x = 10;
    int *p = &x;          /* p guarda la dirección de x */
    *p = 20;              /* cambia x a través del puntero */
    printf("x vale %d\n", x);

    int *datos = malloc(5 * sizeof(int));   /* memoria pedida en tiempo de ejecución */
    if (datos == NULL)
        return 1;
    for (int i = 0; i < 5; i++)
        datos[i] = i * i;
    printf("el cuadrado de 4 es %d\n", datos[4]);
    free(datos);                             /* devuelve la memoria */
    return 0;
}
```

Golden rule: what you ask for with `malloc` you give back with `free`.

### Structures

```c
/* estructuras.c */
#include <stdio.h>

struct sagui {
    char nombre[16];
    int bananas;
};

int main(void)
{
    struct sagui s = { "Tito", 3 };
    s.bananas += 2;
    printf("%s tiene %d bananas\n", s.nombre, s.bananas);
    return 0;
}
```

In `cc` a structure is not passed **by value** to a function: pass its address (`struct sagui *`) instead.

## Reading input and files

**Keyboard:** `fgets` reads a full line (`scanf` does not exist in `cc`):

```c
/* eco.c */
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char linea[128];
    printf("Escribe un numero: ");
    if (fgets(linea, sizeof linea, stdin) == NULL)
        return 1;
    int n = atoi(linea);
    printf("el doble es %d\n", n * 2);
    return 0;
}
```

**Files:** `fopen`, `fprintf`, `fgets` and `fclose`:

```c
/* notas.c */
#include <stdio.h>

int main(void)
{
    FILE *f = fopen("notas.txt", "w");      /* "w": escribir (borra lo anterior) */
    if (f == NULL) {
        printf("no puedo crear notas.txt\n");
        return 1;
    }
    fprintf(f, "primera linea\n");
    fprintf(f, "segunda linea\n");
    fclose(f);

    f = fopen("notas.txt", "r");            /* "r": leer */
    char linea[128];
    while (fgets(linea, sizeof linea, f) != NULL)
        printf("> %s", linea);
    fclose(f);
    return 0;
}
```

For programs that work with several files or folders, look at the examples in `/ejemplos`.

## Windows with the libsagui (with `tcc`)

The libsagui is SaguiOS's library for drawing in windows. It is the same one the system programs use. With `tcc`
you only need to include its headers:

```c
/* ventana.c: una ventana con un degradado */
#include "sagui.h"
#include "gui.h"

int principal(int argc, char **argv)
{
    jan_t *v = jan_abrir("Mi ventana", 320, 200);
    if (!v) return 1;
    for (int y = 0; y < 200; y++)
        g_retangulo(v, 0, y, 320, 1, 0x103060 + y);
    g_texto(v, 20, 20, "Hola desde mi ventana", 0xFFFFFF);
    jan_mostrar(v);

    evento_t e;
    for (;;) {
        if (esperar_evento(v->id, &e, 100) == 1 && e.tipo == EV_FECHAR)
            break;                          /* el usuario cierra la ventana */
    }
    jan_fechar(v);
    return 0;
}
```

Compile and run it:

    tcc ventana.c -o ventana
    ./ventana

The main functions are `jan_abrir` (open), `g_retangulo`, `g_linha`, `g_texto` (draw), `jan_mostrar` (show what was
drawn), `esperar_evento` (wait for the mouse or the keyboard) and `jan_fechar` (close). A color is a number
`0xRRGGBB`. The full list is in the headers `sagui.h` and `gui.h` in `/apps/tcc/include/sagui/`. The example is
similar to the one in the [C Laboratory](../laboratorio-c.md), which also has an editor and a run button.

Windows need the desktop (they do not work in console mode).

## Compiling several files

When a program grows, it is split into files (the `.c` and its `.h`). With `tcc`:

    tcc -c calculos.c                          # por partes: calculos.o
    tcc -o programa principal.c calculos.o     # enlazar todo

## Typical errors

The messages below are the ones the compilers print (in Spanish, as they appear on the system).

| What you see | What happens | What to do |
|--------------|--------------|------------|
| `error: ... no está declarado` | you wrote a name wrong, or did not declare it | check the spelling and that the function is written before it is used |
| `error: expected ';'` or similar | a `;` or a `}` is missing | look at the line the message points to, and the one before it |
| `./hola` finds nothing | the program was not compiled, or you are in another folder | check whether `cc` printed an error; use `ls` to check that `hola` is here |
| Accents come out as strange symbols | the string is UTF-8 and the terminal uses Latin-1 | write the texts without accents, or save the file in Latin-1 (Mico shows the format in its bar) |
| The program crashes or gives absurd results without a message | almost always: you wrote outside an array or used freed memory | compile with `tcc -b` and run it: it tells you the file and the line |
| `este programa usa SSE2 y la CPU no lo tiene` | `cc` generated SSE2 code for a CPU that does not have it | compile with `cc -x87 hola.c -o hola` |
| `printf` prints garbage | the format does not match the type (`%d` with a `double`) | use `%f` for `double` and `%d` for `int` |

To find memory errors:

    tcc -b programa.c -o programa && ./programa

## Complete programs to study

Five complete, simple programs live in `/ejemplos/tcc`: each one teaches a different idea and is compiled with `tcc` inside
SaguiOS, with nothing to install. They are the **original, simple** versions: read them whole (66 to 500 lines), compile them,
change them and break them on purpose. Snake and the Log viewer in the Start menu are extended versions of the last two.

### How to compile them

Copy the folder to your directory, so you can modify it (`/ejemplos` is read-only), and compile with `make`:

```
cp -r /ejemplos/tcc ~/tcc
cd ~/tcc
make SALIDA=/tmp
/tmp/huella muestra.ppm
/tmp/visorppm muestra.ppm
/tmp/snake --prueba
/tmp/visorlogs
/tmp/lumen video.avi
```

Without `make`, each one is compiled with `tcc -o /tmp/huella huella.c` (and likewise for the others). With `tcc -b` it reports
memory errors with their line. The `LEEME.md` in the folder summarises how to try them.

### 1. `huella.c`: files and CRC-32

It computes the **CRC-32** (the one used by ZIP and PNG) of one or more files, or of the standard input when there are no
arguments. It teaches how to read a file **in blocks** of 4096 bytes instead of all at once, how to prepare a **table** of 256
values once and use it for every byte, and how to write a program that works in a **pipe** (`-` is the standard input). The essentials:

```
    for (unsigned i = 0; i < 256; i++) {
        uint32_t resto = i;
        for (int bit = 0; bit < 8; bit++)
            resto = (resto >> 1) ^ ((resto & 1u) ? UINT32_C(0xedb88320) : 0u);
        tabla[i] = resto;
    }
    /* ... y por cada byte leído: */
    crc = (crc >> 8) ^ tabla[(crc ^ bloque[i]) & 0xffu];
```

Try it: `echo -n 123456789 | /tmp/huella` must print `CBF43926`, the reference value of CRC-32. To practise: make it also show
the size of the file; add a `-v` option that prints the CRC in lower case; change the polynomial and see how the result changes.

### 2. `visorppm.c`: images and windows

It opens a **PPM P6** image (the simplest image format: a text header followed by the R, G, B bytes of every pixel) in a window.
It teaches how to **parse a text header** with comments, how to check the limits (640x480 at most) before reserving memory, and
how to **turn RGB into a libsagui pixel**, which is an integer `0xRRGGBB` in the window buffer. The core:

```
    for (size_t i = 0; i < bytes / 3; i++)
        pixeles[i] = ((uint32_t)rgb[3 * i] << 16) |
                     ((uint32_t)rgb[3 * i + 1] << 8) | rgb[3 * i + 2];

    atualizar_janela(ventana, NULL);
    while (esperar_evento(ventana, &evento, 100) != 1 || evento.tipo != EV_FECHAR)
        ;
```

Try `/tmp/visorppm muestra.ppm` (Esc closes it; with a second argument, `/tmp/visorppm muestra.ppm 3`, it closes by itself after 3
seconds). To practise: invert the colours of every pixel (`255 - value`), turn it grey with `(r + g + b) / 3`, or draw a white
frame around it.

### 3. `snake.c`: the loop of a game and the keyboard

The snake game. It teaches the structure of every game: a **state** (the snake is a ring of cells, `cuerpo[]`, with an `ocupada[]`
table to know at once whether a move collides), a **step** that advances the world (`avanzar`), a **drawing** function
(`dibujar`) and a **loop** that waits for a key or for the next turn with `esperar_evento(..., espera)`. Look at how `pedir_giro`
prevents turning onto the snake's own neck, and at the speed, which grows with the score.

```
static void pedir_giro(partida_t *p, enum direccion giro)
{
    if (p->giro == SIN_GIRO && giro != p->direccion &&
        giro != (p->direccion ^ 1))
        p->giro = giro;
}

    /* en el bucle principal: esperar una tecla, o hasta el próximo paso */
    int32_t falta = (int32_t)(turno - tempo_ms());
    if (falta <= 0) {
        avanzar(&p);
        turno = tempo_ms() + intervalo(&p);
    }
```

`/tmp/snake --prueba` runs the rules without opening a window: it is an **automatic test** inside the program itself. To practise:
change `COLUMNAS` and `FILAS`; make crossing a wall take you to the opposite side (instead of losing); add a second fruit; or
change `intervalo()` so the game speeds up sooner.

### 4. `visorlogs.c`: reading logs and filters

It reads the **kernel log** (`info_sistema(INFO_LOG, ...)`) or a file, splits the text into **lines** without copying them (it
only stores where each one starts and how long it is) and filters them by **category** (labels such as `[GPU3D]` or `[USB]`) and by a
case-insensitive **search**. It teaches how to handle strings by hand, how to limit sizes (20,000 lines, the last 2 MiB of a file)
and how to draw a scrolling list. The two key functions:

```
static bool contiene(const char *texto, size_t tam, const char *patron)
{
    size_t n = strlen(patron);
    for (size_t i = 0; i + n <= tam; i++) {
        size_t j = 0;
        while (j < n && plegar(texto[i + j]) == plegar(patron[j]))
            j++;
        if (j == n)
            return true;
    }
    return false;
}

static const char *const gpu[] = { "[gpu", "[acel2d]", "[video]", NULL };
if (alguno(texto, tam, gpu))
    resultado |= 1u << GPU;
```

Try `/tmp/visorlogs` (the kernel log) or `/tmp/visorlogs file.log`; Ctrl+F searches and Esc closes. To practise: add a new
category (for example `[SDL2]`), count how many lines each one has, or paint in red the lines that contain "error". The Log viewer
in the Start menu does all that (and also follows the log live, exports and copies): comparing the two versions is a good exercise.

### 5. `lumen.c`: video and audio with the libsagui

A video and audio player with a track **queue**, progress bar, volume, **full screen** and a sound **visualiser**. It teaches how to open a file with `video_abrir` and `audio_arquivo_abrir`, how to **decode frame by frame** (`video_quadro`), how to send sound to the card **without blocking** (`audio_escrever_sem_bloquear`) and how to **synchronise**: the audio is the clock and the frame drawn is the one matching the samples already delivered. The essentials:

```
static void avanzar_audio(lumen_t *p)
{
    /* mantener lleno el búfer de sonido: el audio manda, el vídeo lo sigue */
    while (audio_pendente(p->flujo) < 160) {
        int n = audio_arquivo_ler(p->audio, p->pcm, PCM);
        if (n <= 0)
            break;
        audio_escrever_sem_bloquear(p->flujo, p->pcm, n * p->canales * 2);
    }
}

    /* el cuadro que toca es el de las muestras ya entregadas */
    int cuadro = (int)((uint64_t)p->entregados * p->fps / p->tasa);
    if (cuadro != p->cuadro)
        video_quadro(p->video, &p->imagen);
```

Try `/tmp/lumen video.avi music.wav` (AVI with MJPEG and PCM, or WAV; Space pauses, arrows skip 5 s, F11 toggles full screen, Esc closes). To practise: add a key that repeats the track, show the remaining time, or change the visualiser. SaguiOS already ships the Video player and Sintonía: compare how they solve the same problem.

## What comes next

- Compiling with `make` when a project has several files: [make](../make.md).
- A lab with examples and a run button: [C Laboratory](../laboratorio-c.md).
- Pascal, another compiled language: [Pascal on SaguiOS](../pascal.md).
- For windows in a simpler language: [Python](python_EN.md).
