<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 13; titulo: Anexo, a biblioteca SDL no SaguiOS -->
# Anexo, a biblioteca SDL no SaguiOS

[Español](anexo-sdl.md) · [Português](anexo-sdl_PT.md) · [English](anexo-sdl_EN.md)

> O que é a SDL, por que o SaguiOS tem a sua própria, como ela se encaixa no sistema e o que foi preciso fazer, com os temas deste curso, para portar jogos e programas.

## 1. Teoria

### 1.1 O que é a SDL

A SDL (*Simple DirectMedia Layer*) é uma biblioteca que dá a um programa janelas, desenho, teclado, mouse, áudio, threads e temporizadores com uma interface
única, seja qual for o sistema. A SDL 1.2 é a versão clássica; a SDL 2 é a atual, com renderizador, texturas e eventos mais completos. No Linux ela se apoia
no X11 ou no Wayland; no Windows, nas suas próprias APIs.

### 1.2 Por que é preciso uma camada

Um programa que usa a SDL conhece apenas a interface; a implementação muda de um sistema para outro. Por isso o SaguiOS não depende do X11 nem do Wayland
(não os tem): implementa a interface da SDL sobre a sua própria biblioteca gráfica, a libsagui. O jogo é compilado sem mudanças.

### 1.3 Janela, lienzo, evento e quadro

- **Janela**: o que o sistema mostra.
- **Lienzo**: área de memória de 32 bits onde se desenha; é copiado para a janela ao apresentar.
- **Eventos**: uma fila preenchida pelo teclado, pelo mouse e pela janela; o programa a esvazia com `SDL_PollEvent`.
- **Quadro**: um ciclo de eventos, desenho, apresentação e espera. Um jogo repete o quadro dezenas de vezes por segundo.

### 1.4 Os temas do curso, aplicados

- **Threads**: criar uma thread e esperar que termine; o que cada thread pode fazer.
- **Travas e condições**: um mutex protege os dados e uma condição avisa de uma mudança; a espera deve ser repetida num laço.
- **Áudio**: o som é misturado numa chamada de retorno que roda numa thread própria; se essa thread não chegar a tempo, o som falha.
- **Prioridades**: uma thread de áudio pode pedir mais CPU ao escalonador.
- **Tempo**: o relógio do sistema e as esperas, sem temporizadores com chamada de retorno.

## 2. Como se encaixa no SaguiOS

### 2.1 O que existe

- **Janela e lienzo**: uma janela lógica do tamanho que o programa pediu; o lienzo é escalado ao tamanho da tela, com faixas pretas se for preciso, e em
  320 × 200 e 640 × 400 mantém-se a proporção 4:3 da VGA (`userland/libsdl2/sdl2_sagui.c`, decisão D204).
- **Teclado e mouse**: os códigos de tecla da SDL 2 vêm do mapa de teclas do núcleo, então há KEYDOWN e KEYUP também para Shift, Ctrl e Alt.
- **Áudio**: por chamada de retorno ou por fila (`SDL_QueueAudio`); a conversão de formato, canais e frequência está em `SDL_BuildAudioCVT` e
  `SDL_ConvertAudio` (decisão D383). `SDL_PauseAudio(0)` inicia uma **thread de áudio** própria que chama a chamada de retorno com a trava do dispositivo
  pega (decisão D393).
- **Renderizador por software e por GPU**: com `SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED)` o lienzo vai para a GPU se houver uma com 3D; se não,
  continua por software sem que o programa perceba (decisão D288, `docs/sdl-gpu.md`). `SDL_RenderGeometry` desenha triângulos (o AoEN usa isso pela D436).
- **Tela cheia sem passar pela RAM**: a GPU escreve o quadro na tela (decisão D432).
- **OpenGL ES 1.1**: `userland/libsdl2/sdl2_gl.c` implementa `SDL_GL_*` sobre EGL e SaguiGL (decisão D425).

### 2.2 Duas bibliotecas: SDL 1.2 e SDL 2

- `userland/libsdl12/` é a SDL 1.2 comum (`sdl_sagui.c`, decisão D228).
- `userland/libsdl2/` é a SDL 2 (`sdl2_sagui.c`, decisão D230), com seus cabeçalhos, `SDL_mixer`, `SDL_image` e `SDL_ttf` ao lado.
- Exemplos de portes que usam essas bibliotecas: Command & Conquer (D230 e D247), DOOM com Chocolate Doom (D383), Freeciv (D374), mGBA (D322),
  Brumacanto (D444) e AoEN (D393, D431, D436).

**O que não há.** Não há `SDL_AddTimer` (devolve erro) nem temporizadores com chamada de retorno, nem controles, nem mouse relativo, e os cursores próprios
são aceitos mas não desenhados. O comentário de cabeçalho de `userland/libsdl2/SDL.h` dizia que não havia threads nem OpenGL; já não é verdade (a D393 acrescentou as
threads e `sdl2_gl.c` implementa OpenGL ES) e o cabeçalho está em dia.

## 3. O que foi preciso fazer, e por quê

### 3.1 Threads: `SDL_CreateThread` sobre `thread_create`

Até a decisão D383, `SDL_CreateThread` falhava e as travas da SDL não faziam nada. A decisão D393 as ligou às threads reais do sistema (`thread_create`
da libsagui, decisão D167). Ficam duas regras:

- Uma thread de trabalho **não chama** vídeo, eventos nem áudio: só calcula e avisa a thread principal.
- Um programa que chamava a si mesmo com a trava tomada continua funcionando, porque a mesma thread pode tomá-la de novo.

### 3.2 Travas e condições

`SDL_CreateMutex`, `SDL_LockMutex` e `SDL_CondSignal` usam o `mutex_t` e o futex do sistema (decisão D150). Uma condição pode acordar sem que ninguém a
tenha avisado, então a espera sempre fica num laço que volta a verificar o dado (o exemplo `hilo_avisa.c` faz assim).

Uma espera de `SDL_CondWait` na thread principal é interrompida a cada 2 ms (para fazer avançar o OpenAL, que se move a partir da principal) e depois verifica de novo
(D393). Quem espera a chamada de retorno do áudio, como o Chocolate Doom, recebe o aviso de verdade a partir da thread de áudio.

### 3.3 Áudio: uma thread própria para que o som não falhe

No início a SDL do SaguiOS movia o áudio **a partir da thread principal**, uma vez por quadro, com cerca de 120 ms de antecipação: se um quadro demorava mais (o
Brumacanto tem quadros de 400 ms ao criar trechos novos), o som falhava. A decisão D393 (ampliação de 2026-11-22) deu à SDL a sua **thread de áudio**: `SDL_PauseAudio(0)`
(a primeira vez) a cria com `thread_create`, e a cada 5 ms olha quanto resta por tocar no fluxo do sistema; se é menos que a latência do núcleo mais 60 ms (no mínimo 120 ms),
chama a chamada de retorno do programa **a partir dessa thread** e lhe passa o bloco. Como a thread não depende dos quadros do jogo, um quadro lento já não corta o som.
`SDL_SAGUI_AUDIO_HILO=0` (variável de ambiente ou dica) volta ao modo antigo. O AoEN leva ainda o seu próprio misturador sem SDL (D431).

Consequência prática: `SDL_LockAudio` e `SDL_LockAudioDevice` são agora travas de verdade (recursivas), e a chamada de retorno roda com ela pega. Se o programa toca, a partir de outra thread,
o que compartilha com a chamada de retorno (um volume, um contador, a posição de uma música), deve pegá-la: são duas threads e sem ela haveria uma condição de corrida (capítulo 7).
É o padrão de `tono.c`.

### 3.4 Prioridades: `PLAN_AUDIO`

A prioridade de áudio do escalonador é o `PLAN_AUDIO`, decisão **D436** (a D431 é outra coisa: o áudio do AoEN independente dos FPS). Uma thread a pede com a chamada do sistema; a thread de áudio da
SDL **só a pede se lhe disserem** (`SDL_SAGUI_AUDIO_PRIO=1`, variável ou dica), porque com ela o Freeciv, com cliente, servidor e música na mesma CPU, deixava de responder aos pings do
servidor. É limitada por uso (`PLAN_AUDIO_MAX_USO`, 5 tiques a cada aprox. 250 ms, cerca de 20 % de uma CPU), então não serve para acaparar a CPU. É o tema do capítulo 5 na prática.

### 3.5 Temporizadores e relógio

Os programas usam `SDL_GetTicks` e `SDL_Delay`. O `SDL_AddTimer` não está disponível (devolve erro), então um programa que depende de um temporizador com
chamada de retorno deve fazer o seu próprio laço comparando o tempo, como faz o exemplo `tono.c`.

**Decisões e limites.**
- **Decisão: uma SDL própria sobre a libsagui.** Permite portar sem depender do X11 ou do Wayland (D230).
- **Decisão: renderizador por GPU opcional.** Se a GPU falhar, o programa continua por software (D288).
- **Decisão: uma thread de áudio própria na SDL.** A decisão D393 (ampliação) evita que um quadro lento corte o som, ao custo de a chamada de retorno rodar noutra thread e ter de ser
  protegida com `SDL_LockAudio`.
- **Limite: a prioridade de áudio não vem ativada por padrão.** Se a máquina está muito carregada, a thread de áudio compete com as demais como qualquer outra; pode-se pedir `PLAN_AUDIO`
  com `SDL_SAGUI_AUDIO_PRIO=1`.
- **Limite: não há temporizadores com chamada de retorno.** É preciso usar laços com o relógio.

## 4. Exemplos

Três programas pequenos estão em `/ejemplos/sdl/` (no repositório, `rootfs/ejemplos/sdl/`). Segundo o guia do TCC (`tcc.md`), são compilados dentro do
SaguiOS a partir de uma pasta sua, assim:

```
cd /tmp
tcc /ejemplos/sdl/ventana_mueve.c -o ventana_mueve -lSDL2
tcc /ejemplos/sdl/hilo_avisa.c -o hilo_avisa -lSDL2
tcc /ejemplos/sdl/tono.c -o tono -lSDL2
```

E são executados com `./ventana_mueve`, `./hilo_avisa` e `./tono`.

- `ventana_mueve.c`: uma janela de 320 × 200 com um retângulo que se move com as setas (eventos e renderizador). Escape sai.
- `hilo_avisa.c`: uma thread calcula uma soma e avisa a thread principal com uma condição (threads, mutex e condições).
- `tono.c`: um tom de 440 Hz gerado na chamada de retorno do áudio; no fim escreve quantas vezes foi chamada e se foi outra thread que o fez (áudio e relógio).

**Estado da verificação.** Os três foram compilados e executados dentro do SaguiOS no QEMU (`tests/curso_ejemplos.py`, com e sem KVM). O `hilo_avisa`
escreve `suma de 1 a 1000000 = 500000500000`. O `tono` diz que a chamada de retorno rodou umas 85 vezes em dois segundos e numa thread diferente da
principal (a imagem de teste não tem placa de som, então não se ouve nada: a thread de áudio marca o ritmo com o relógio). No `ventana_mueve`, seis
setas para a direita e três para baixo movem o retângulo 24 e 12 pontos, e Escape fecha a janela. O teste serviu para corrigir duas coisas: o
`hilo_avisa.c` somava com `long`, que no SaguiOS (32 bits) tem 32 bits e estourava (escrevia 1784293664 em vez de 500000500000; é o lembrete de que em
32 bits `long` e ponteiros cabem em 32 bits, capítulo 6), e o título «Rectángulo» saía como «RectAingulo» porque o TCC deixa as cadeias em UTF-8 e a janela
as desenha como Latin-1 (agora o título não tem acento).

## 5. Perguntas de revisão

1. Por que o SaguiOS implementa a sua própria SDL em vez de usar X11 ou Wayland? Que vantagem isso traz para um jogo já escrito?
2. Em `hilo_avisa.c`, por que a espera é um `while` e não um `if`? O que aconteceria se a thread de trabalho chamasse `SDL_PollEvent`?
3. Um jogo tem quadros de 150 ms e o som não falha com a SDL de fábrica. Por quê? O que aconteceria com `SDL_SAGUI_AUDIO_HILO=0`?
4. O que é o `PLAN_AUDIO`, quem o pede e por que tem um limite de uso?
5. Por que a chamada de retorno do áudio precisa agora de `SDL_LockAudio` para tocar um dado compartilhado com o programa?

## 6. Respostas

1. O X11 e o Wayland não existem no SaguiOS, então a camada própria é necessária. Para um jogo já escrito contra a SDL, a vantagem é que ele compila sem mudanças:
   a interface é a mesma e só muda a implementação.
2. Porque uma condição pode acordar sem um aviso, e o dado deve ser verificado de novo depois de cada despertar. E a thread de trabalho não deve chamar
   `SDL_PollEvent`, porque eventos e vídeo são tratados só na thread principal.
3. Porque desde a decisão D393 o áudio é movido por uma thread própria da SDL, que alimenta a placa com cerca de 120 ms de antecipação sem depender dos quadros do jogo. Com
   `SDL_SAGUI_AUDIO_HILO=0` voltaria ao modo antigo, em que a thread principal move o áudio uma vez por quadro: com quadros de 150 ms, o som falharia. (O AoEN leva ainda o seu próprio
   misturador: D431.)
4. O `PLAN_AUDIO` é a prioridade de áudio do escalonador (D436). Uma thread o pede com a chamada do sistema. Tem limite de uso para que uma thread não monopolize
   a CPU pedindo sempre essa prioridade.
5. Porque a chamada de retorno roda na thread de áudio, diferente da thread do programa: sem a trava, as duas poderiam tocar o mesmo dado ao mesmo tempo (condição de corrida,
   capítulo 7).
