<!-- categoria: Programación/C; orden: 10; titulo: C -->
# C en SaguiOS: tu primer programa

**C** es el lenguaje con el que está escrito SaguiOS: el núcleo, las ventanas y casi todos los programas.
Es un lenguaje pequeño y rápido. Aprenderlo aquí es útil para entender el sistema por dentro, y para
escribir programas de consola, juegos y ventanas sin instalar nada más.

En SaguiOS hay **dos compiladores de C**:

| Orden | Qué es | Cuándo usarlo |
|-------|--------|---------------|
| `cc` | el compilador propio de SaguiOS: compila en una sola pasada, sin ensamblador | para empezar, programas de consola |
| `tcc` | el Tiny C Compiler: casi todo C y la **libsagui** (ventanas), la SDL 2 y OpenGL ES | ventanas, juegos, comprobar errores de memoria (`-b`) |

Las dos órdenes sacan un programa **ejecutable** que corre solo, sin más piezas. Para ventanas, usa `tcc`
(`cc` no las tiene). Guía de cada compilador: [TCC](../tcc.md). Otros lenguajes: [Programar en SaguiOS](indice.md).

## Antes de empezar

- Abre el **Terminal** (ahí escribes las órdenes).
- Para escribir el código usa **Mico** (el editor de código). Pluma es un procesador de textos: guarda en RTF
  por defecto y no es el mejor para programas. Mico colorea el C y, con **F5**, compila y ejecuta el programa
  (lo hace con `cc`). Mira [Mico](../mico.md).
- Crea una carpeta para tus programas, por ejemplo con `mkdir c` y entra con `cd c`.

## Tu primer programa, paso a paso

**1.** En el Terminal, dentro de la carpeta `c`, abre Mico con el archivo nuevo:

    mico hola.c

**2.** Escribe esto en Mico:

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

**3.** Guarda con **Ctrl+S**.

**4.** Compila y ejecuta. En el Terminal escribe:

    cc hola.c -o hola
    ./hola

Verás las dos líneas. Si no ves nada de error, `cc` ha creado el programa `hola` en tu carpeta. El `./` delante
del nombre es importante: dice «el programa de esta carpeta».

**5.** Alternativa sin crear archivo ejecutable: `tcc -run hola.c` compila en memoria y ejecuta.

**Atajo en Mico:** con el archivo abierto, **F5** hace lo mismo que los pasos 4 (compila con `cc` y ejecuta
en el panel de abajo).

## Lo básico del lenguaje

### Variables y tipos

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

En `cc`, `long` es lo mismo que `int`. `long long` no existe. Los números con decimales son `float` y `double`.
Para imprimirlos: `%d` (entero), `%f` o `%.2f` (decimales), `%s` (cadena), `%c` (carácter).

### Condiciones y bucles

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

`switch`, `break` y `continue` también funcionan. `goto` no existe en `cc`.

### Funciones

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

Una función debe estar escrita antes de usarla (o declarada arriba, como `int cuadrado(int x);`).

### Arrays y cadenas

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

Los índices empiezan en 0. C **no comprueba** que no te salgas del array: si escribes fuera, el programa
falla de forma rara. Con `tcc -b` (ver más abajo) te lo dice.

### Punteros y memoria

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

Regla de oro: lo que pides con `malloc` lo devuelves con `free`.

### Estructuras

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

En `cc` no se pasa una estructura **por valor** a una función: pasa su dirección (`struct sagui *`).

## Leer datos y archivos

**Teclado:** `fgets` lee una línea completa (`scanf` no existe en `cc`):

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

**Archivos:** `fopen`, `fprintf`, `fgets` y `fclose`:

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

Para los programas que trabajan con varios archivos o carpetas, mira los ejemplos de `/ejemplos`.

## Ventanas con la libsagui (con `tcc`)

La libsagui es la biblioteca de SaguiOS para dibujar en ventanas. Es la misma que usan los programas del
sistema. Con `tcc` basta con incluir sus cabeceras:

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

Compílalo y ejecútalo:

    tcc ventana.c -o ventana
    ./ventana

Las funciones principales son `jan_abrir` (abrir), `g_retangulo`, `g_linha`, `g_texto` (dibujar), `jan_mostrar`
(mostrar lo dibujado), `esperar_evento` (esperar al ratón o al teclado) y `jan_fechar`. El color es un número
`0xRRGGBB`. La lista completa está en las cabeceras `sagui.h` y `gui.h` de `/apps/tcc/include/sagui/`. El ejemplo se parece al de
[Laboratorio de C](../laboratorio-c.md), que además tiene un editor y el botón de ejecutar.

Las ventanas necesitan el escritorio (no funcionan en el modo de consola).

## Compilar varios archivos

Cuando un programa crece, se reparte en archivos (el `.c` y su `.h`). Con `tcc`:

    tcc -c calculos.c                          # por partes: calculos.o
    tcc -o programa principal.c calculos.o     # enlazar todo

## Errores típicos

| Lo que ves | Qué pasa | Qué hacer |
|------------|----------|-----------|
| `error: ... no está declarado` | escribiste mal un nombre o falta declararlo | revisa la ortografía y que la función esté antes de usarla |
| `error: expected ';'` o similar | falta un `;` o una llave `}` | mira la línea que indica el mensaje y la anterior |
| `./hola` no encuentra nada | el programa no se compiló, o estás en otra carpeta | mira si salió algún error del `cc`; comprueba con `ls` que `hola` está aquí |
| Los acentos salen como símbolos raros | la cadena está en UTF-8 y el terminal usa Latin-1 | escribe los textos sin acentos, o guarda el archivo en Latin-1 (Mico muestra el formato en la barra) |
| El programa se cae o da resultados absurdos sin mensaje | casi siempre: escribiste fuera de un array o usaste memoria liberada | compila con `tcc -b` y ejecútalo: te dice el archivo y la línea |
| `este programa usa SSE2 y la CPU no lo tiene` | el `cc` generó código SSE2 en una CPU antigua | compila con `cc -x87 hola.c -o hola` |
| `printf` imprime basura | el formato no coincide con el tipo (`%d` con un `double`) | usa `%f` para `double` y `%d` para `int` |

Para encontrar errores de memoria:

    tcc -b programa.c -o programa && ./programa

## Programas completos para estudiar

Cinco programas completos, sencillos, en `/ejemplos/tcc`: cada uno enseña una idea distinta y se compila con `tcc` dentro de
SaguiOS, sin instalar nada. Son la versión **original y sencilla**: léelos enteros (de 66 a 500 líneas), compílalos, cámbialos y
rómpelos a propósito. Snake y el Visor de registros del menú Inicio son versiones ampliadas de los dos últimos.

### Cómo compilarlos

Copia la carpeta a tu directorio, para poder modificarla (`/ejemplos` es de solo lectura), y compila con `make`:

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

Sin `make`, cada uno se compila con `tcc -o /tmp/huella huella.c` (y así con los demás). Con `tcc -b` te avisa de los errores
de memoria con su línea. El `LEEME.md` de la carpeta resume cómo probarlos.

### 1. `huella.c`: ficheros y CRC-32

Calcula el **CRC-32** (el de ZIP y PNG) de uno o varios archivos, o de la entrada estándar si no hay argumentos. Enseña a leer un
archivo **por bloques** de 4096 bytes en vez de entero, a preparar una **tabla** de 256 valores una sola vez y usarla por cada byte,
y a escribir un programa que funciona en una **tubería** (`-` es la entrada estándar). Lo esencial:

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

Prueba: `echo -n 123456789 | /tmp/huella` debe dar `CBF43926`, el valor de referencia del CRC-32. Para practicar: haz que muestre
también el tamaño del archivo; añade una opción `-v` que escriba el CRC en minúsculas; cambia el polinomio y mira cómo cambia
el resultado.

### 2. `visorppm.c`: imágenes y ventanas

Abre una imagen **PPM P6** (el formato de imagen más simple: una cabecera de texto y después los bytes R, G, B de cada píxel) en una
ventana. Enseña a **analizar una cabecera** de texto con comentarios, a comprobar los límites (640x480 como máximo) antes de
reservar memoria, y a **pasar de RGB a un píxel** de la libsagui, que es un entero `0xRRGGBB` en el búfer de la ventana. El
núcleo:

```
    for (size_t i = 0; i < bytes / 3; i++)
        pixeles[i] = ((uint32_t)rgb[3 * i] << 16) |
                     ((uint32_t)rgb[3 * i + 1] << 8) | rgb[3 * i + 2];

    atualizar_janela(ventana, NULL);
    while (esperar_evento(ventana, &evento, 100) != 1 || evento.tipo != EV_FECHAR)
        ;
```

Prueba con `/tmp/visorppm muestra.ppm` (Esc cierra; con un segundo argumento, `/tmp/visorppm muestra.ppm 3`, se cierra solo a
los 3 segundos). Para practicar: invierte los colores de cada píxel (`255 - valor`), pásalo a gris con
`(r + g + b) / 3`, o dibuja un marco blanco alrededor.

### 3. `snake.c`: el bucle de un juego y el teclado

El juego de la serpiente. Enseña la estructura de todo juego: un **estado** (la serpiente es un anillo de casillas, `cuerpo[]`, con
una tabla `ocupada[]` para saber al instante si un movimiento choca), un **paso** que avanza el mundo (`avanzar`), un **dibujo**
(`dibujar`) y un **bucle** que espera una tecla o el siguiente turno con `esperar_evento(..., espera)`. Fíjate en cómo `pedir_giro`
impide girar sobre el propio cuello, y en la velocidad, que sube con los puntos.

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

`/tmp/snake --prueba` ejecuta las reglas sin abrir ventana: es una **prueba automática** dentro del propio programa. Para practicar:
cambia `COLUMNAS` y `FILAS`; haz que atravesar la pared te lleve al lado contrario (en vez de perder); añade una segunda fruta; o
cambia `intervalo()` para que el juego acelere antes.

### 4. `visorlogs.c`: lectura de registros y filtros

Lee el **registro del kernel** (`info_sistema(INFO_LOG, ...)`) o un archivo, parte el texto en **líneas** sin copiarlas (guarda
solo dónde empieza cada una y cuánto mide) y las filtra por **categoría** (las etiquetas como `[GPU3D]` o `[USB]`) y por una
**búsqueda** sin distinguir mayúsculas. Enseña a trabajar con cadenas a mano, a limitar el tamaño (20 000 líneas, los últimos
2 MiB de un archivo) y a dibujar una lista con desplazamiento. Las dos funciones clave:

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

Prueba con `/tmp/visorlogs` (el registro del kernel) o `/tmp/visorlogs archivo.log`; Ctrl+F busca y Esc cierra. Para practicar:
añade una categoría nueva (por ejemplo `[SDL2]`), cuenta cuántas líneas tiene cada una, o pinta de rojo las líneas que
contengan «error». El Visor de registros del menú Inicio hace todo eso (y seguir el registro en vivo, exportar y copiar):
comparar las dos versiones es un buen ejercicio.

### 5. `lumen.c`: vídeo y audio con la libsagui

Un reproductor de vídeo y audio con **cola** de pistas, barra de progreso, volumen, **pantalla completa** y un **visualizador** del sonido. Enseña a abrir un archivo con `video_abrir` y `audio_arquivo_abrir`, a **decodificar cuadro a cuadro** (`video_quadro`), a mandar el sonido a la tarjeta **sin bloquear** (`audio_escrever_sem_bloquear`) y a **sincronizar**: el audio es el reloj y el cuadro que se dibuja es el que corresponde a las muestras ya entregadas. Lo esencial:

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

Prueba con `/tmp/lumen video.avi musica.wav` (AVI con MJPEG y PCM, o WAV; Espacio pausa, las flechas saltan 5 s, F11 cambia a pantalla completa, Esc cierra). Para practicar: añade una tecla que repita la pista, muestra el tiempo restante, o cambia el visualizador. SaguiOS ya trae el Reproductor de vídeo y Sintonía: compara cómo resuelven lo mismo.

## Qué sigue

- Compilar con `make` cuando el proyecto tiene varios archivos: [make](../make.md).
- Un laboratorio con ejemplos y botón de ejecutar: [Laboratorio de C](../laboratorio-c.md).
- Pascal, otro lenguaje compilado: [Pascal en SaguiOS](../pascal.md).
- Para ventanas en un lenguaje más sencillo: [Python](python.md).
