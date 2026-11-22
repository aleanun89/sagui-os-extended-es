<!-- categoria: Programación/C; orden: 10; titulo: C -->
# C no SaguiOS: seu primeiro programa

O **C** é a linguagem em que o próprio SaguiOS é escrito: o núcleo, as janelas e quase todos os programas.
É uma linguagem pequena e rápida. Aprendê-la aqui ajuda a entender o sistema por dentro e permite escrever
programas de console, jogos e programas com janelas sem instalar mais nada.

O SaguiOS tem **dois compiladores de C**:

| Comando | O que é | Quando usar |
|---------|---------|-------------|
| `cc` | o compilador próprio do SaguiOS: compila em uma só passagem, sem montador | para começar, programas de console |
| `tcc` | o Tiny C Compiler: quase todo o C, mais a **libsagui** (janelas), a SDL 2 e o OpenGL ES | janelas, jogos, verificar erros de memória (`-b`) |

Os dois comandos geram um programa **executável** que roda sozinho, sem peças extras. Para janelas, use `tcc`
(o `cc` não as tem). Guia de cada compilador: [TCC](../tcc.md). Outras linguagens: [Programar no SaguiOS](indice_PT.md).

## Antes de começar

- Abra o **Terminal** (é ali que você digita os comandos).
- Para escrever o código, use o **Mico** (o editor de código). O Pluma é um processador de textos: salva em RTF por
  padrão e não é a melhor escolha para programas. O Mico colore o C e, com **F5**, compila e executa o programa
  (usa o `cc`). Veja [Mico](../mico.md).
- Crie uma pasta para os seus programas, por exemplo com `mkdir c` e entre nela com `cd c`.

## Seu primeiro programa, passo a passo

**1.** No Terminal, dentro da pasta `c`, abra o Mico com o arquivo novo:

    mico hola.c

**2.** Escreva isto no Mico:

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

**3.** Salve com **Ctrl+S**.

**4.** Compile e execute. No Terminal digite:

    cc hola.c -o hola
    ./hola

Você verá as duas linhas. Se não aparecer nenhum erro, o `cc` criou o programa `hola` na sua pasta. O `./` antes
do nome é importante: significa «o programa desta pasta».

**5.** Alternativa sem criar um arquivo executável: `tcc -run hola.c` compila na memória e executa.

**Atalho no Mico:** com o arquivo aberto, **F5** faz o mesmo que os passos 4 (compila com `cc` e executa o
programa no painel de baixo).

## O básico da linguagem

### Variáveis e tipos

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

No `cc`, `long` é o mesmo que `int`. `long long` não existe. Os números com decimais são `float` e `double`.
Para imprimi-los: `%d` (inteiro), `%f` ou `%.2f` (decimais), `%s` (cadeia), `%c` (caractere).

### Condições e laços

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

`switch`, `break` e `continue` também funcionam. `goto` não existe no `cc`.

### Funções

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

Uma função tem de estar escrita antes de ser usada (ou declarada acima, como `int cuadrado(int x);`).

### Arrays e cadeias

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

Os índices começam em 0. O C **não verifica** se você fica dentro de um array: se escrever para fora, o programa
falha de forma estranha. Com `tcc -b` (veja abaixo) ele avisa.

### Ponteiros e memória

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

Regra de ouro: o que você pede com `malloc` devolve com `free`.

### Estruturas

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

No `cc`, uma estrutura não é passada **por valor** para uma função: passe o endereço dela (`struct sagui *`).

## Ler dados e arquivos

**Teclado:** `fgets` lê uma linha completa (o `scanf` não existe no `cc`):

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

**Arquivos:** `fopen`, `fprintf`, `fgets` e `fclose`:

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

Para programas que trabalham com vários arquivos ou pastas, veja os exemplos em `/ejemplos`.

## Janelas com a libsagui (com `tcc`)

A libsagui é a biblioteca do SaguiOS para desenhar em janelas. É a mesma que os programas do sistema usam. Com o
`tcc` basta incluir os cabeçalhos:

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

Compile e execute:

    tcc ventana.c -o ventana
    ./ventana

As funções principais são `jan_abrir` (abrir), `g_retangulo`, `g_linha`, `g_texto` (desenhar), `jan_mostrar`
(mostrar o que foi desenhado), `esperar_evento` (esperar o mouse ou o teclado) e `jan_fechar` (fechar). A cor é um
número `0xRRGGBB`. A lista completa está nos cabeçalhos `sagui.h` e `gui.h` em `/apps/tcc/include/sagui/`. O exemplo
é parecido com o do [Laboratório de C](../laboratorio-c.md), que também tem um editor e um botão de executar.

As janelas precisam da área de trabalho (não funcionam no modo de console).

## Compilar vários arquivos

Quando um programa cresce, ele se divide em arquivos (o `.c` e o seu `.h`). Com o `tcc`:

    tcc -c calculos.c                          # por partes: calculos.o
    tcc -o programa principal.c calculos.o     # enlazar todo

## Erros típicos

As mensagens abaixo são as que os compiladores mostram (em espanhol, como aparecem no sistema).

| O que aparece | O que acontece | O que fazer |
|---------------|----------------|-------------|
| `error: ... no está declarado` | você escreveu um nome errado, ou não o declarou | confira a ortografia e se a função está escrita antes de ser usada |
| `error: expected ';'` ou parecido | falta um `;` ou um `}` | olhe a linha que a mensagem indica e a anterior |
| `./hola` não encontra nada | o programa não foi compilado, ou você está em outra pasta | veja se o `cc` mostrou algum erro; use `ls` para confirmar que `hola` está aqui |
| Os acentos saem como símbolos estranhos | a cadeia está em UTF-8 e o terminal usa Latin-1 | escreva os textos sem acentos, ou salve o arquivo em Latin-1 (o Mico mostra o formato na barra) |
| O programa cai ou dá resultados absurdos sem mensagem | quase sempre: você escreveu fora de um array ou usou memória já liberada | compile com `tcc -b` e execute: ele mostra o arquivo e a linha |
| `este programa usa SSE2 y la CPU no lo tiene` | o `cc` gerou código SSE2 para uma CPU que não o tem | compile com `cc -x87 hola.c -o hola` |
| O `printf` imprime lixo | o formato não corresponde ao tipo (`%d` com um `double`) | use `%f` para `double` e `%d` para `int` |

Para encontrar erros de memória:

    tcc -b programa.c -o programa && ./programa

## Programas completos para estudar

Cinco programas completos e simples ficam em `/ejemplos/tcc`: cada um ensina uma ideia diferente e é compilado com `tcc` dentro do
SaguiOS, sem instalar nada. São as versões **originais e simples**: leia-os inteiros (de 66 a 500 linhas), compile-os, mude-os e
quebre-os de propósito. O Snake e o Visualizador de registros do menu Iniciar são versões ampliadas dos dois últimos.

### Como compilá-los

Copie a pasta para o seu diretório, para poder modificá-la (`/ejemplos` é somente leitura), e compile com `make`:

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

Sem `make`, cada um se compila com `tcc -o /tmp/huella huella.c` (e assim com os demais). Com `tcc -b` ele avisa os erros de
memória com a sua linha. O `LEEME.md` da pasta resume como testá-los.

### 1. `huella.c`: arquivos e CRC-32

Calcula o **CRC-32** (o do ZIP e do PNG) de um ou mais arquivos, ou da entrada padrão se não houver argumentos. Ensina a ler um
arquivo **por blocos** de 4096 bytes em vez de inteiro, a preparar uma **tabela** de 256 valores uma única vez e usá-la a cada byte,
e a escrever um programa que funciona num **pipe** (`-` é a entrada padrão). O essencial:

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

Teste: `echo -n 123456789 | /tmp/huella` deve dar `CBF43926`, o valor de referência do CRC-32. Para praticar: faça com que mostre
também o tamanho do arquivo; acrescente uma opção `-v` que escreva o CRC em minúsculas; mude o polinômio e veja como o resultado muda.

### 2. `visorppm.c`: imagens e janelas

Abre uma imagem **PPM P6** (o formato de imagem mais simples: um cabeçalho de texto e depois os bytes R, G, B de cada pixel) numa
janela. Ensina a **analisar um cabeçalho** de texto com comentários, a verificar os limites (640x480 no máximo) antes de reservar
memória, e a **passar de RGB para um pixel** da libsagui, que é um inteiro `0xRRGGBB` no buffer da janela. O núcleo:

```
    for (size_t i = 0; i < bytes / 3; i++)
        pixeles[i] = ((uint32_t)rgb[3 * i] << 16) |
                     ((uint32_t)rgb[3 * i + 1] << 8) | rgb[3 * i + 2];

    atualizar_janela(ventana, NULL);
    while (esperar_evento(ventana, &evento, 100) != 1 || evento.tipo != EV_FECHAR)
        ;
```

Teste com `/tmp/visorppm muestra.ppm` (Esc fecha; com um segundo argumento, `/tmp/visorppm muestra.ppm 3`, fecha sozinho aos 3
segundos). Para praticar: inverta as cores de cada pixel (`255 - valor`), passe-a para cinza com `(r + g + b) / 3`, ou desenhe uma
moldura branca em volta.

### 3. `snake.c`: o laço de um jogo e o teclado

O jogo da cobrinha. Ensina a estrutura de todo jogo: um **estado** (a cobra é um anel de casas, `cuerpo[]`, com uma tabela
`ocupada[]` para saber na hora se um movimento bate), um **passo** que avança o mundo (`avanzar`), um **desenho** (`dibujar`) e um
**laço** que espera uma tecla ou o próximo turno com `esperar_evento(..., espera)`. Repare como `pedir_giro` impede de virar sobre o
próprio pescoço, e na velocidade, que aumenta com os pontos.

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

`/tmp/snake --prueba` executa as regras sem abrir janela: é um **teste automático** dentro do próprio programa. Para praticar:
mude `COLUMNAS` e `FILAS`; faça atravessar a parede levar ao lado oposto (em vez de perder); acrescente uma segunda fruta; ou mude
`intervalo()` para o jogo acelerar antes.

### 4. `visorlogs.c`: leitura de registros e filtros

Lê o **registro do kernel** (`info_sistema(INFO_LOG, ...)`) ou um arquivo, divide o texto em **linhas** sem copiá-las (guarda só
onde cada uma começa e quanto mede) e as filtra por **categoria** (as etiquetas como `[GPU3D]` ou `[USB]`) e por uma **busca** sem
distinguir maiúsculas. Ensina a trabalhar com textos à mão, a limitar o tamanho (20 000 linhas, os últimos 2 MiB de um arquivo) e a
desenhar uma lista com rolagem. As duas funções-chave:

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

Teste com `/tmp/visorlogs` (o registro do kernel) ou `/tmp/visorlogs arquivo.log`; Ctrl+F busca e Esc fecha. Para praticar:
acrescente uma categoria nova (por exemplo `[SDL2]`), conte quantas linhas cada uma tem, ou pinte de vermelho as linhas que
contenham «error». O Visualizador de registros do menu Iniciar faz tudo isso (e também acompanha o registro ao vivo, exporta e
copia): comparar as duas versões é um bom exercício.

### 5. `lumen.c`: vídeo e áudio com a libsagui

Um reprodutor de vídeo e áudio com **fila** de faixas, barra de progresso, volume, **tela cheia** e um **visualizador** do som. Ensina a abrir um arquivo com `video_abrir` e `audio_arquivo_abrir`, a **decodificar quadro a quadro** (`video_quadro`), a mandar o som para a placa **sem bloquear** (`audio_escrever_sem_bloquear`) e a **sincronizar**: o áudio é o relógio e o quadro desenhado é o que corresponde às amostras já entregues. O essencial:

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

Teste com `/tmp/lumen video.avi musica.wav` (AVI com MJPEG e PCM, ou WAV; Espaço pausa, as setas saltam 5 s, F11 alterna a tela cheia, Esc fecha). Para praticar: acrescente uma tecla que repita a faixa, mostre o tempo restante, ou mude o visualizador. O SaguiOS já traz o Reprodutor de vídeo e o Sintonía: compare como resolvem o mesmo problema.

## O que vem depois

- Compilar com `make` quando o projeto tem vários arquivos: [make](../make.md).
- Um laboratório com exemplos e botão de executar: [Laboratório de C](../laboratorio-c.md).
- Pascal, outra linguagem compilada: [Pascal no SaguiOS](../pascal.md).
- Janelas em uma linguagem mais simples: [Python](python_PT.md).
