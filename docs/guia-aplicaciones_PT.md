# Guia de aplicações do SaguiOS-Extended

[Español](guia-aplicaciones.md) · [Português](guia-aplicaciones_PT.md) · [English](guia-aplicaciones_EN.md)

> Para quem quer saber **quais programas o SaguiOS traz** e **onde está o código** de cada um.
> As aplicações estão organizadas como o menu **Iniciar**. Para cada uma indica-se o nome que
> aparece, o comando ou o arquivo na imagem, uma linha sobre o que faz, o caminho do seu código e
> a decisão de projeto (`D…`) mais importante, que está em [`decisoes.md`](decisoes.md).
> O kernel está explicado no [guia do kernel](guia-kernel_PT.md).

**Índice**

1. [Como os programas estão distribuídos: `/bin` e `/apps`](#1-como-os-programas-estão-distribuídos-bin-e-apps)
2. [Menu Iniciar, por categorias](#2-menu-iniciar-por-categorias)
3. [Desenvolvimento: compiladores e ferramentas](#3-desenvolvimento-compiladores-e-ferramentas)
4. [Comandos de console: `sagsh` e `/bin`](#4-comandos-de-console-sagsh-e-bin)
5. [Bibliotecas](#5-bibliotecas)
6. [Ports de terceiros](#6-ports-de-terceiros)
7. [Onde ler mais](#7-onde-ler-mais)

---

## 1. Como os programas estão distribuídos: `/bin` e `/apps`

O SaguiOS tem dois lugares para os executáveis:

| Pasta | O que há | Como chega lá |
| --- | --- | --- |
| `/bin` | Os comandos básicos e as aplicações pequenas | Na base do sistema, a que é carregada ao iniciar |
| `/apps/bin` | As aplicações grandes: o navegador, os jogos, os compiladores | Na partição **SAGUI-APPS**, que é montada em `/apps` (decisões D194 e D324) |

A lista do que vai para `/apps` está no `Makefile` (variável `APPS_MOVER_FIJO` e
`APPS_PROGRAMAS`). Assim a inicialização na RAM pesa menos. As demais regras de compilação de cada
programa estão na sua pasta, em um arquivo `.mk` (por exemplo, `userland/terceros/netsurf/netsurf.mk`).

> **Regra do menu:** uma entrada de `/apps` só aparece no menu Iniciar se o seu executável existir
> (função `programa_disponivel()` de `kernel/gui/barra.c`, decisão D194). Por isso as entradas dos
> jogos grandes não aparecem em uma imagem que não os traz.

**Símbolo `†`:** os programas com esta marca não estão na variante **sem GPL** (`make SEM_GPL=1`,
decisão D109): seu código é GPL de terceiros e fica isolado. Ver [`terceros.md`](terceros.md).

---

## 2. Menu Iniciar, por categorias

O menu Iniciar é definido na tabela `programas[]` de `kernel/gui/barra.c`. Há sete grupos.
As tabelas desta seção saem dessa tabela: cada linha é uma entrada do menu, na mesma ordem.

### 2.1 Acessórios

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Archivos (Arquivos) | `/bin/arquivos` | explorador de arquivos e pastas | `userland/programas/arquivos.c` | — |
| Calculadora | `/bin/calculadora` | calculadora | `userland/programas/calculadora.c` | — |
| Editor de texto | `/bin/editor` | editor de texto simples | `userland/programas/editor.c` | — |
| Recortes | `/bin/recortes` | recortes da tela | `userland/programas/recortes.c` | D125 |
| Iconos (Ícones) | `/bin/icones` | biblioteca de ícones à vista | `userland/programas/icones.c` | D135 |
| Música | `/apps/bin/musica` | reprodutor de música simples | `userland/programas/musica.c` | — |
| MIDI | `/bin/midi` | reprodutor de arquivos MIDI | `userland/programas/midi.c` | D190 |
| Markdown | `/bin/markdown` | visualizador e editor de Markdown | `userland/programas/markdown.c` | — |
| Mico (editor de código) | `/apps/bin/mico` | editor de código (Mico) | `userland/programas/mico/` | — |
| VisualPythonS (diseñador) (designer) | `/apps/bin/python /lib/python/vps/disenador.py` | designer visual com Python (roda com o MicroPython) | `rootfs/lib/python/vps/disenador.py` | D297 |
| Explorador SQLite (explorador SQLite) | `/apps/bin/python /lib/python/basedatos/explorador_sqlite.py` | explorador de bancos de dados SQLite (com o MicroPython) | `rootfs/lib/python/basedatos/explorador_sqlite.py` | D333 |
| Explorador lite-monkey (explorador lite-monkey) | `/apps/bin/python /lib/python/basedatos/explorador_litemonkey.py` | explorador do banco de dados lite-monkey (com o MicroPython) | `rootfs/lib/python/basedatos/explorador_litemonkey.py` | D333 |
| Pluma (processador de textos) (Pluma) | `/apps/bin/pluma` | processador de textos | `userland/programas/pluma.c` | D291 |
| Traductor (Tradutor) | `/apps/bin/traductor` | editor de traduções (no estilo do Poedit) | `userland/programas/traductor.c` | D335 |
| Macaco (planilha) (Macaco) | `/apps/bin/macaco` | planilha eletrônica | `userland/programas/macaco.c` | D142 |
| Tucán (apresentações) (Tucán) | `/apps/bin/tucan` | editor de apresentações | `userland/programas/tucan.c` | D369 |
| Visor de PDF (Visualizador de PDF) | `/apps/bin/visorpdf` | visualizador de PDF (motor em userland/pdf/) | `userland/programas/visorpdf.c` | D139 |
| IA (chat) (IA (chat)) | `/apps/bin/ia` | chat com IA na linha de comando (biblioteca em userland/ia/) | `userland/programas/ia.c` | D139 |
| Chat IA | `/apps/bin/chatia` | chat com IA em janela (biblioteca em userland/ia/) | `userland/programas/chatia.c` | D143 |

### 2.2 Gráficos

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Paint | `/apps/bin/paint` | programa de desenho | `userland/programas/paint.c` | — |
| Visor de imágenes (Visualizador de imagens) | `/bin/visor` | visualizador de imagens | `userland/programas/visor.c` | — |
| PlanoMini (CAD 2D) (PlanoMini (CAD 2D)) | `/apps/bin/planomini` | mini CAD 2D (plantas) | `userland/planomini/` | D312 |
| Reproductor de vídeo (Reprodutor de vídeo) | `/apps/bin/reproductor` | reprodutor de vídeo | `userland/programas/reproductor.c` | D139 |
| Cámara (Câmera) | `/apps/bin/camara` | webcam (UVC) | `userland/programas/camara.c` | D277 |

### 2.3 Internet

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Cipó (navegador) (Cipó (navegador)) | `/apps/bin/cipo` | navegador web Cipó (motor em userland/web/) | `userland/programas/cipo.c` | — |
| NetSurf (navegador) (NetSurf (navegador)) | `/apps/bin/netsurf` | navegador NetSurf (código de terceiros) | `userland/terceros/netsurf/` | D184 † |
| Correo (Correio) | `/apps/bin/correo` | cliente de correio (biblioteca em userland/mail/) | `userland/programas/correo.c` | D146 |
| Escritorio remoto (VNC) (Área de trabalho remota (VNC)) | `/bin/vnc` | cliente de área de trabalho remota VNC | `userland/programas/vnc.c` | D254 |

### 2.4 Multimídia

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Sintonía (reproductor) (Sintonia (reprodutor)) | `/apps/bin/sintonia` | reprodutor de música com listas (m3u, pls) e emissoras por HTTPS | `userland/programas/sintonia.c` | D225 |
| Grabadora de sonido (Gravador de som) | `/apps/bin/grabadora` | gravador de som pelo microfone | `userland/programas/grabadora.c` | D292 |
| Conversor de audio (Conversor de áudio) | `/apps/bin/convertir-audio` | conversor entre MP3, FLAC e WAV (o codificador MP3 é o Shine, em userland/terceros/shine/) | `userland/programas/convertir-audio.c` | D339 |

### 2.5 Educação

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| GebraMini (geometría dinámica) (GebraMini (geometria dinâmica)) | `/apps/bin/gebramini` | geometria dinâmica | `userland/gebramini/` | D309 |
| Jabuti (programar brincando) (Jabuti (programar brincando)) | `/apps/bin/jabuti` | programação com blocos para crianças | `userland/jabuti/` | D377 |
| Laboratorio de C (Laboratório de C) | `/apps/bin/labc` | laboratório de C: editor, compilador (libtcc) e exemplos | `userland/programas/labc/` | D414 |
| Actividades JClic (Atividades JClic) | `/apps/bin/jclic` | visualizador de atividades educativas JClic | `userland/programas/jclic.c` | D185 |
| Actividades JClic (bajar) (Atividades JClic (baixar)) | `/apps/bin/jclicbajar` | baixador de atividades JClic do repositório | `userland/programas/jclicbajar.c` | D185 |

### 2.6 Jogos

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Sagui Atrapa Bananas (Sagui Pega Bananas) | `/apps/bin/banana` | jogo Sagui Pega Banana | `userland/programas/banana.c` | — |
| Solitario Spider (Paciência Spider) | `/apps/bin/spider` | paciência Spider | `userland/programas/spider.c` | D173 |
| Buscaminas (Campo minado) | `/apps/bin/buscaminas` | jogo Campo minado (Buscaminas) | `userland/programas/buscaminas.c` | D248 |
| Snake (Cobrinha) | `/apps/bin/snake` | o jogo da cobrinha, com recorde guardado no perfil do usuário (uma versão simples para estudar está em `/ejemplos/tcc/snake.c`) | `userland/programas/snake.c` | D463 |
| AoEN | `/apps/bin/aoen` | AoEN, jogo de estratégia em tempo real | `userland/juegos/aoen/` | D382 |
| OpenTyrian | `/apps/bin/opentyrian` | OpenTyrian (shoot-em-up, código de terceiros) | `userland/terceros/opentyrian/` | D180 † |
| OpenTyrian (pantalla completa) (tela cheia) | `/apps/bin/opentyrian --pantalla-completa` | OpenTyrian (shoot-em-up, código de terceiros) (em tela cheia) | `userland/terceros/opentyrian/` | D204 † |
| Freeciv | `/apps/bin/freeciv-sdl2` | Freeciv, estratégia por turnos (cliente SDL 2) | `userland/terceros/freeciv/` | D374 † |
| OpenTTD | `/apps/bin/openttd` | OpenTTD. Seu código não está nesta branch: a entrada só aparece se existir `/apps/bin/openttd` | `—` | D375 † |
| OpenJazz | `/apps/bin/openjazz` | OpenJazz, jogo de plataforma (Jazz Jackrabbit) | `userland/terceros/openjazz/` | D228 † |
| OpenJazz (pantalla completa) (tela cheia) | `/apps/bin/openjazz --pantalla-completa` | OpenJazz, jogo de plataforma (Jazz Jackrabbit) (em tela cheia) | `userland/terceros/openjazz/` | D228 † |
| Command & Conquer | `/apps/bin/conquer` | Command & Conquer (Vanilla Conquer) | `userland/terceros/conquer/` | D230 † |
| Quake | `/apps/bin/quake` | Quake (motor livre da id Software) | `userland/terceros/quake/` | D229 † |
| Quake (pantalla completa) (tela cheia) | `/apps/bin/quake --pantalla-completa` | Quake (motor livre da id Software) (em tela cheia) | `userland/terceros/quake/` | D229 † |
| DOOM | `/apps/bin/chocolate-doom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Freedoom | `/apps/bin/chocolate-doom --freedoom` | DOOM (Chocolate Doom) | `userland/terceros/chocolatedoom/` | D383 † |
| Tzar | `/apps/bin/tzar` | Tzar, jogo de estratégia (port em C do ReTzar) | `userland/juegos/tzar/` | D417 † |
| Game Boy (GNUBoy) | `/apps/bin/gnuboy` | emulador de Game Boy e Game Boy Color (GNUBoy) | `userland/terceros/gnuboy/` | D235 † |
| Recreativa (juegos y consolas) (Fliperama (jogos e consoles)) | `/apps/bin/recreativa` | lançador de jogos de DOS | `userland/terceros/recreativa/` | D294 † |
| DOSBox | `/apps/bin/dosbox` | emulador de MS-DOS (DOSBox 0.74) | `userland/terceros/dosbox/` | D294 † |
| DOSBox (pantalla completa) (tela cheia) | `/apps/bin/dosbox --pantalla-completa` | emulador de MS-DOS (DOSBox 0.74) (em tela cheia) | `userland/terceros/dosbox/` | D294 † |
| Sagui 3D | `/apps/bin/sagui3d` | jogo 3D próprio | `userland/sagui3d/` | — |
| CHIP-8 | `/apps/bin/chip8` | emulador de CHIP-8 | `userland/chip8/` | — |
| ROM de CHIP-8 (bajar) (ROM de CHIP-8 (baixar)) | `/apps/bin/python /apps/bin/chip8-roms-gui` | ROM de CHIP-8 (download) | `userland/apps/bin/chip8-roms-gui` | D123 |

### 2.7 Sistema

| Nome no menu | Comando (caminho) | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Terminal | `/bin/terminal` | terminal em janela | `userland/programas/terminal.c` | — |
| Visor de registros (Visualizador de registros) | `/bin/visorlogs` | o registro do kernel e arquivos de registro: filtros, cores por nível, acompanhamento ao vivo, exportar e copiar (uma versão simples para estudar está em `/ejemplos/tcc/visorlogs.c`) | `userland/programas/visorlogs.c` | D463 |
| Administrador de tareas (Gerenciador de tarefas) | `/bin/tareas` | gerenciador de tarefas | `userland/programas/tareas.c` | — |
| Información del PC (Informações do PC) | `/bin/infopc` | informações do PC | `userland/programas/infopc.c` | — |
| Pantalla (Tela) | `/bin/pantalla --ventana` | resolução e telas | `userland/programas/pantalla.c` | — |
| Fecha y hora (Data e hora) | `/bin/zona --ventana` | data, hora e fuso horário | `userland/programas/zona.c` | — |
| Configuración (Configuração) | `/bin/configuracion` | configuração do sistema | `userland/programas/configuracion.c` | — |
| Discos | `/bin/discos --ventana` | discos e partições | `userland/programas/discos.c` | D121 |
| Bluetooth | `/bin/bluetooth --ventana` | Bluetooth | `userland/programas/bluetooth.c` | D279 |
| Instalar SaguiOS (Instalar o SaguiOS) | `/bin/instalar --ventana` | instalador do SaguiOS em um disco ou pendrive | `userland/programas/instalar.c` | D179 |
| DEBUG-APP (depuración remota) (DEBUG-APP (depuração remota)) | `/bin/debug-app` | depuração remota por SSH | `userland/programas/debug-app.c` | D362 |

---

## 3. Desenvolvimento: compiladores e ferramentas

O menu não tem um grupo "Desenvolvimento". Estas ferramentas estão em `/apps/bin` (ou em `/bin`) e
são usadas a partir do terminal. O **Laboratório de C** (em Educação) e o **Mico** (em Acessórios)
também servem para programar.

| Ferramenta | Comando | O que faz | Código | Decisão |
| --- | --- | --- | --- | --- |
| Compilador de C do SaguiOS | `/apps/bin/cc` | compila programas em C para executáveis do SaguiOS (gera código x86) | `userland/programas/cc.c`, `userland/programas/cc_biblioteca.h` | — |
| Compilador de Pascal | `/apps/bin/pascal` | compila programas em Pascal | `userland/programas/pascal.c`, `userland/programas/pascal_biblioteca.h` | D085-D087 |
| TCC (Tiny C Compiler) | `/apps/bin/tcc` | compilador de C rápido, no próprio sistema | `userland/terceros/tcc/` | — |
| GNU make | `/apps/bin/make` | constrói projetos segundo um `Makefile` | `userland/terceros/make/` | — |
| git | `/apps/bin/git` | controle de versões, sobre o libgit2 | `userland/terceros/git/` | D400 |
| MicroPython | `/apps/bin/python` | Python 3 reduzido (o comando `python` do SaguiOS) | `userland/micropython/` | D034 |
| miniJVM (`java`, `javac`) | `/apps/bin/java`, `/apps/bin/javac` | intérprete de Java e seu compilador | `userland/terceros/minijvm/` | D376 |
| GNU Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | intérprete de Prolog e seu compilador | `userland/terceros/gprolog/` | D189 |
| TinyGo (exemplos) | — | exemplos em Go; são compilados com o TinyGo | `userland/go/` (ver `docs/go.md`) | D364 |
| JavaScript (`qjs`) | `/apps/bin/qjs` | intérprete de JavaScript no terminal | `userland/programas/qjs.c` | D188 |
| SQLite (`sqlite3`) | `/apps/bin/sqlite3` | intérprete de comandos SQL sobre o SQLite | `userland/programas/sqlite3.c`, `userland/terceros/sqlite/` | D116 |
| Shell `sagsh` | `/bin/sagsh` (cópia: `/bin/sh`) | o intérprete de comandos; ver a seção 4 | `userland/sagsh/` | D037 |
| `debug-app` | `/bin/debug-app` | depuração remota por SSH (entra outro computador da rede) | `userland/programas/debug-app.c` | D362 |

---

## 4. Comandos de console: `sagsh` e `/bin`

**`sagsh`** é o intérprete de comandos do SaguiOS (a *shell*). Lê o que você escreve, procura o
programa (em `/bin`, `/apps/bin` e no `PATH`) e o executa. `/bin/sh` é uma cópia do `sagsh`. Seu código
está em `userland/sagsh/` (analisador em `analise.c`, execução em `executar.c`, a ajuda em
`ayuda.c`). Decisão D037.

Cada arquivo `.c` de `userland/programas/` é compilado como um comando. O nome do arquivo é o
nome do comando. A tabela seguinte sai do cabeçalho de cada arquivo, então a descrição é a que o
próprio código diz. A coluna **Comando** indica onde fica o executável na imagem padrão
(`/apps/bin` para os da lista `APPS_MOVER_FIJO` do `Makefile`, `/bin` para o resto).

| Comando | O que faz | Código |
| --- | --- | --- |
| `/bin/acel2d` | o 2D pela GPU da área de trabalho (D195) | `userland/programas/acel2d.c` |
| `/bin/admin` | executa um comando como administrador (root) (D255, melhoria 098) | `userland/programas/admin.c` |
| `/bin/afinidad` | as CPU onde um processo pode rodar (D174) | `userland/programas/afinidad.c` |
| `/bin/amd` | o estado do motor 3D e a identificação da GPU da AMD (melhoria 054, parte B2; D158) | `userland/programas/amd.c` |
| `/bin/ampliar-home` | "Ampliar /home para usar todo o espaço livre" (D318, v8.4.3) | `userland/programas/ampliar-home.c` |
| `/bin/arquivos` | o explorador de arquivos do SaguiOS (H23; melhoria 058, D136; renovado na v8.5.5.2, D420) | `userland/programas/arquivos.c` |
| `/bin/arranque` | testar um kernel iniciando-o UMA única vez, e trocar o estágio 2 do disco (D392) | `userland/programas/arranque.c` |
| `/apps/bin/banana` | Sagui Pega Banana, o jogo do SaguiOS (H27) | `userland/programas/banana.c` |
| `/bin/base64` | codifica ou decodifica Base64 (D232) | `userland/programas/base64.c` |
| `/bin/basename` | tira a pasta (e um sufixo) de um caminho (D097) | `userland/programas/basename.c` |
| `/bin/bc` | calculadora de linha de comando, versão simples (D232) | `userland/programas/bc.c` |
| `/bin/bench` | mede a velocidade da memória e da interface gráfica (D042) | `userland/programas/bench.c` |
| `/bin/benchmark` | benchmark de monoNúcleo e multiNúcleo com pontuação (D176) | `userland/programas/benchmark.c` |
| `/bin/bip` | bip [frequência [duração]] - toca uma nota no alto-falante do PC (B3) | `userland/programas/bip.c` |
| `/bin/bloqueo` | o bloqueio de publicidade e rastreamento por DNS, no estilo do Pi-hole (D218) | `userland/programas/bloqueo.c` |
| `/bin/bluetooth` | o Bluetooth com um controlador USB padrão, p. ex. o CSR8510 | `userland/programas/bluetooth.c` |
| `/bin/brillo` | o brilho da tela do notebook (D403) | `userland/programas/brillo.c` |
| `/apps/bin/buscaminas` | o Campo minado do SaguiOS (D248) | `userland/programas/buscaminas.c` |
| `/bin/cal` | calendário (D232): o mês de hoje, um mês qualquer ou um ano inteiro | `userland/programas/cal.c` |
| `/bin/calculadora` | a Calculadora do SaguiOS (H26) | `userland/programas/calculadora.c` |
| `/apps/bin/camara` | camara [-p] [--modo N] [--info] [--foto arquivo [--cuadros N]] - o aplicativo Câmera (D277) | `userland/programas/camara.c` |
| `/bin/captura` | captura [arquivo] [-v/--ventana] [-r x,y,largura,altura] [-t segundos] (D125) Salva a tela já composta (sem cursor) como PNG | `userland/programas/captura.c` |
| `/bin/cat` | mostra arquivos; sem argumentos (ou com "-") copia a entrada para a saída (D097) | `userland/programas/cat.c` |
| `/apps/bin/cc` | o compilador de C do SaguiOS | `userland/programas/cc.c` |
| `/bin/certificados` | gerencia o armazenamento de raízes do TLS (D063) | `userland/programas/certificados.c` |
| `/apps/bin/chatia` | o chat com IA com janela (D143, D239) | `userland/programas/chatia.c` |
| `/bin/chgrp` | muda o grupo de arquivos e pastas (D282, melhoria 098): root, ou o dono para o seu próprio grupo | `userland/programas/chgrp.c` |
| `/bin/chmod` | muda as permissões de arquivos e pastas (D282, melhoria 098) | `userland/programas/chmod.c` |
| `/bin/chown` | muda o dono (e o grupo) de arquivos e pastas (D282, melhoria 098) | `userland/programas/chown.c` |
| `/apps/bin/cipo` | cipo [endereço] (alias do navegador) - o Cipó, navegador do SaguiOS (D047) | `userland/programas/cipo.c` |
| `/bin/cmp` | compara dois arquivos byte a byte (D232) | `userland/programas/cmp.c` |
| `/bin/column` | formata um texto em colunas (D232) | `userland/programas/column.c` |
| `/bin/comm` | compara dois arquivos ordenados linha a linha (D232) | `userland/programas/comm.c` |
| `/bin/componentes` | demonstração do kit gráfico da libsagui (H22): rótulo, botão, caixa de texto, área de texto com rolagem, lista e diálogos | `userland/programas/componentes.c` |
| `/bin/configuracion` | o aplicativo "Configuração" do SaguiOS (D112, melhorias 007 e 047) | `userland/programas/configuracion.c` |
| `/bin/conky` | clone próprio (não um port) do Conky para a área de trabalho do SaguiOS (D365) | `userland/programas/conky.c` |
| `/bin/contador` | conta segundos, para mostrar a multitarefa | `userland/programas/contador.c` |
| `/apps/bin/convertir-audio` | o Conversor de áudio (D339) | `userland/programas/convertir-audio.c` |
| `/apps/bin/correo` | cliente de correio com janela (082) e aparência do Outlook clássico (113, D289) | `userland/programas/correo.c` |
| `/bin/cp` | copia arquivos e pastas (D097) | `userland/programas/cp.c` |
| `/bin/cron` | executa tarefas em horas fixas (D216, melhoria 074) | `userland/programas/cron.c` |
| `/bin/crontab` | a tabela de cron do usuário, /home/.config/crontab (D216) | `userland/programas/crontab.c` |
| `/bin/curl` | download por HTTP/1.1 ou HTTPS (D046, D062) | `userland/programas/curl.c` |
| `/bin/cut` | extrai campos ou colunas de cada linha (D097) | `userland/programas/cut.c` |
| `/bin/data` | data e hora (D080) | `userland/programas/data.c` |
| `/bin/debug-app` | DEBUG-APP: deixa entrar outro computador da rede local, por SSH e por um tempo, para depurar este (D362) | `userland/programas/debug-app.c` |
| `/bin/desmontar` | grava tudo o que estiver pendente e libera o CD ou o pendrive | `userland/programas/desmontar.c` |
| `/bin/df` | espaço livre nos sistemas de arquivos montados (D097) | `userland/programas/df.c` |
| `/bin/dhcp` | pede um endereço IP por DHCP e mostra o resultado (D046) | `userland/programas/dhcp.c` |
| `/bin/diff` | compara dois arquivos linha a linha (D097) | `userland/programas/diff.c` |
| `/bin/dirname` | tira o último componente de um caminho (D097) | `userland/programas/dirname.c` |
| `/bin/discos` | os discos e o espaço que ocupa cada um (H34, D040, D121) | `userland/programas/discos.c` |
| `/bin/dmesg` | o registro do kernel (D232): o que escreveu ao iniciar e depois | `userland/programas/dmesg.c` |
| `/bin/dnsbloqueo` | servidor DNS mínimo com o filtro do bloqueio de publicidade (D218): o "Pi-hole" da casa | `userland/programas/dnsbloqueo.c` |
| `/bin/du` | espaço que ocupam arquivos e pastas (D097) | `userland/programas/du.c` |
| `/bin/echo` | escreve os argumentos separados por espaço | `userland/programas/echo.c` |
| `/bin/ecualizador` | equalizador de 5 bandas para todo o som do sistema (D201) | `userland/programas/ecualizador.c` |
| `/bin/editor` | o Editor de texto do SaguiOS (H25) | `userland/programas/editor.c` |
| `/bin/eject` | eject [-t] [leitor] - desmonta o CD/DVD e abre a bandeja (D233) | `userland/programas/eject.c` |
| `/bin/ejecutar` | os diálogos do menu Iniciar (D065) | `userland/programas/ejecutar.c` |
| `/bin/energia` | protetor de tela, bloqueio e desligamento da tela por inatividade, bateria e "não perturbe" (D422) | `userland/programas/energia.c` |
| `/bin/env` | mostra o ambiente ou executa um comando com um ambiente modificado (D097) | `userland/programas/env.c` |
| `/apps/bin/es11demo` | demonstração de OpenGL ES 1.1 com EGL (D425) | `userland/programas/es11demo.c` |
| `/bin/escaner` | escâner de rede próprio do SaguiOS (D398) | `userland/programas/escaner.c` |
| `/bin/expand` | troca tabulações por espaços (D232) | `userland/programas/expand.c` |
| `/bin/expr` | avalia uma expressão a partir de seus argumentos (D232), como o do Linux | `userland/programas/expr.c` |
| `/bin/falha` | provoca um erro de propósito para mostrar a proteção: só este processo é encerrado pelo kernel; o shell e o sistema continuam | `userland/programas/falha.c` |
| `/bin/file` | diz de que tipo é cada arquivo, olhando seu conteúdo (D232) | `userland/programas/file.c` |
| `/bin/find` | procura arquivos e pastas por nome, tipo, tamanho | `userland/programas/find.c` |
| `/bin/fold` | quebra as linhas longas (D232) | `userland/programas/fold.c` |
| `/bin/frecuencia` | controle da frequência da CPU (D106, melhoria 061) | `userland/programas/frecuencia.c` |
| `/bin/free` | memória total, usada e livre (D097) | `userland/programas/free.c` |
| `/bin/fuentes` | instalar, remover e ver as fontes do sistema (D356) | `userland/programas/fuentes.c` |
| `/apps/bin/gebramini` | geometria dinâmica e funções do SaguiOS (D309, melhoria 128) | `userland/programas/gebramini.c` |
| `/bin/gpu3d` | o 3D pela GPU (D285) | `userland/programas/gpu3d.c` |
| `/apps/bin/grabadora` | o Gravador de som (D292) | `userland/programas/grabadora.c` |
| `/bin/grabar` | grabar arquivo.wav [segundos] [opções] - grava o microfone em um WAV (D292) | `userland/programas/grabar.c` |
| `/bin/grep` | procura linhas que correspondam a um padrão (D097) | `userland/programas/grep.c` |
| `/bin/gunzip` | gunzip = gzip -d (melhoria 056): ver gzip.c | `userland/programas/gunzip.c` |
| `/bin/gzip` | gzip, gunzip e zcat (melhoria 056, D103): uma única fonte; gunzip.c e zcat.c a incluem com MODO_GZ diferente | `userland/programas/gzip.c` |
| `/bin/head` | primeiras linhas (ou bytes) de arquivos ou da entrada (D097) | `userland/programas/head.c` |
| `/bin/hostname` | nome da máquina (D097) | `userland/programas/hostname.c` |
| `/bin/htop` | monitor de processos interativo para o terminal (D366) | `userland/programas/htop.c` |
| `/apps/bin/ia` | cliente de chat com IA na linha de comando (D134) | `userland/programas/ia.c` |
| `/bin/icones` | a biblioteca de ícones do SaguiOS, à vista (D135) | `userland/programas/icones.c` |
| `/bin/id` | identidade do processo (D232; de verdade desde D255): o uid e o gid que o kernel lhe dá, com os nomes de /etc/usuarios | `userland/programas/id.c` |
| `/bin/ifconfig` | mostra e configura a placa de rede (D046) | `userland/programas/ifconfig.c` |
| `/bin/infopc` | informações do PC, no estilo do CPU-Z (D041) | `userland/programas/infopc.c` |
| `/bin/instalar` | copia o SaguiOS para um disco rígido ou para outro pendrive (D179) | `userland/programas/instalar.c` |
| `/apps/bin/jabuti` | programação com blocos para crianças (D377, melhoria 137) | `userland/programas/jabuti.c` |
| `/apps/bin/jclic` | visualizador de atividades educativas JClic (D185) | `userland/programas/jclic.c` |
| `/apps/bin/jclicbajar` | "Atividades JClic (baixar)": o baixador do repositório Clic do XTEC (D185) | `userland/programas/jclicbajar.c` |
| `/bin/kconfig` | opções do kernel que são guardadas pelo sistema e valem ao reiniciar (D353) | `userland/programas/kconfig.c` |
| `/bin/kill` | encerra um processo pelo seu PID | `userland/programas/kill.c` |
| `/bin/less` | less / more (paginador) - mostra um arquivo ou a entrada de tela na tela (D097) | `userland/programas/less.c` |
| `/bin/login` | início de sessão no console de texto (D371, fase 6, P12) | `userland/programas/login.c` |
| `/bin/ls` | lista arquivos e pastas (D097; D385 os põe em colunas) | `userland/programas/ls.c` |
| `/bin/lscpu` | informações da CPU no formato do Linux (D232), sobre o CPUID e o que dá o kernel | `userland/programas/lscpu.c` |
| `/bin/lspci` | lista os dispositivos PCI no formato do Linux (D232), sobre o que dá o kernel | `userland/programas/lspci.c` |
| `/apps/bin/macaco` | a planilha eletrônica do SaguiOS (melhoria 078, D140) | `userland/programas/macaco.c` |
| `/bin/mandos` | controles e joysticks pelo terminal (D307; docs/mandos.md) | `userland/programas/mandos.c` |
| `/bin/markdown` | markdown [-t] [arquivo.md] - visualizador e editor de Markdown do SaguiOS (D045) | `userland/programas/markdown.c` |
| `/bin/md5sum` | resumo MD5 de arquivos, como no Linux (D232) | `userland/programas/md5sum.c` |
| `/bin/mem` | memória total, usada e livre | `userland/programas/mem.c` |
| `/bin/midi` | midi [arquivo / pasta] | `userland/programas/midi.c` |
| `/bin/mkdir` | cria pastas (D097) | `userland/programas/mkdir.c` |
| `/bin/mktemp` | cria um arquivo ou pasta temporário com nome único e escreve seu caminho (D232) | `userland/programas/mktemp.c` |
| `/bin/montar` | monta o pendrive em /usb (sem nome: o primeiro disponível para montar) | `userland/programas/montar.c` |
| `/bin/more` | paginador simples: o mesmo programa que o less (D097) | `userland/programas/more.c` |
| `/bin/mount` | monta sistemas de arquivos (D040) | `userland/programas/mount.c` |
| `/apps/bin/musica` | musica [arquivo / pasta] | `userland/programas/musica.c` |
| `/bin/mv` | move ou renomeia arquivos e pastas (D097) | `userland/programas/mv.c` |
| `/bin/nc` | nc (netcat) básico - conexões TCP pela linha de comando (D046) | `userland/programas/nc.c` |
| `/bin/nl` | numera as linhas (D232) | `userland/programas/nl.c` |
| `/bin/nproc` | quantas CPU há (D232) | `userland/programas/nproc.c` |
| `/bin/nslookup` | nslookup (alias host) nome [servidor[:porta]] - pergunta ao DNS (D046) | `userland/programas/nslookup.c` |
| `/bin/ntp` | acerta o relógio por SNTP (RFC 4330), sobre UDP (D080) | `userland/programas/ntp.c` |
| `/bin/od` | despeja um arquivo em octal, hexadecimal ou caracteres (D232); com -C dá o formato de "hexdump -C" | `userland/programas/od.c` |
| `/bin/ola` | imprime os argumentos recebidos (teste do carregador, H13) | `userland/programas/ola.c` |
| `/apps/bin/paint` | o programa de desenho do SaguiOS (D056) | `userland/programas/paint.c` |
| `/bin/panel` | o painel tátil (touchpad) pelo console (D407) | `userland/programas/panel.c` |
| `/bin/pantalla` | resolução da tela (D058, D059) | `userland/programas/pantalla.c` |
| `/bin/particiones` | a tabela de partições dos discos, pelo terminal (D179) | `userland/programas/particiones.c` |
| `/apps/bin/pascal` | o compilador de Pascal do SaguiOS (D085-D087) | `userland/programas/pascal.c` |
| `/bin/passwd` | muda a senha de uma conta (D255, melhoria 098) | `userland/programas/passwd.c` |
| `/bin/paste` | junta as linhas de vários arquivos lado a lado (D232) | `userland/programas/paste.c` |
| `/bin/ping` | ping [-c N] [-s bytes] [-W ms] host - eco ICMP (D046) | `userland/programas/ping.c` |
| `/bin/planificador` | o escalonador de processos entre as CPU (D174) | `userland/programas/planificador.c` |
| `/apps/bin/planomini` | mini CAD 2D do SaguiOS (D312, melhoria 129) | `userland/programas/planomini.c` |
| `/apps/bin/pluma` | o processador de textos do SaguiOS (melhoria 116, D291), com a aparência de um Word 97 simplificado: barra de menus, barras de ferramentas Padrão e Formatação com ícones, régua com margens e recuos, vista de página sobre fundo cinza e barra de status | `userland/programas/pluma.c` |
| `/bin/portatil` | o próprio dos notebooks pelo ACPI (D403) | `userland/programas/portatil.c` |
| `/bin/ps` | lista os processos (D097) | `userland/programas/ps.c` |
| `/apps/bin/qjs` | o intérprete de JavaScript no terminal (D188) | `userland/programas/qjs.c` |
| `/bin/radios` | estado dos rádios (Wi-Fi, Bluetooth, WWAN) dos notebooks HP pelo WMI, e ligá-los ou desligá-los (D405) | `userland/programas/radios.c` |
| `/bin/realpath` | caminho absoluto e limpo de um arquivo ou pasta (D232) | `userland/programas/realpath.c` |
| `/bin/recortes` | recortes [--recorte] [-t segundos] - recortes de tela (D125) | `userland/programas/recortes.c` |
| `/bin/reparar` | repara um ext2 que ficou mal depois de um corte ou de um travamento (D397) | `userland/programas/reparar.c` |
| `/bin/reproducir` | reproducir [-v volume] arquivo | `userland/programas/reproducir.c` |
| `/apps/bin/reproductor` | reproductor [arquivo / pasta / lista.m3u] | `userland/programas/reproductor.c` |
| `/bin/rev` | escreve cada linha ao contrário (D232) | `userland/programas/rev.c` |
| `/bin/rm` | apaga arquivos e, com -r, pastas (D097) | `userland/programas/rm.c` |
| `/bin/rmdir` | apaga pastas vazias (D097) | `userland/programas/rmdir.c` |
| `/bin/saguimark` | o benchmark do SaguiOS em modo texto (D093, melhoria 051) | `userland/programas/saguimark.c` |
| `/bin/scp` | copiar um arquivo para outra máquina, ou trazê-lo, por SSH (D317) | `userland/programas/scp.c` |
| `/bin/sed` | editor de fluxo, o básico (D232): s///, d, p, q, =, a, i, c com endereços | `userland/programas/sed.c` |
| `/bin/sensores` | o monitor de hardware em modo texto (D092, melhoria 050) | `userland/programas/sensores.c` |
| `/bin/seq` | escreve uma sequência de números inteiros (D097) | `userland/programas/seq.c` |
| `/bin/servicio` | gerencia os serviços em segundo plano pelo terminal (D216) | `userland/programas/servicio.c` |
| `/bin/servicios` | o supervisor do gerenciador de serviços (D216, melhoria 074) | `userland/programas/servicios.c` |
| `/apps/bin/sgldemo` | cubo de cores girando com o SaguiGL (melhoria 054, v5; D104) | `userland/programas/sgldemo.c` |
| `/bin/sha1sum` | resumo SHA-1 de arquivos, como no Linux (D232) | `userland/programas/sha1sum.c` |
| `/bin/sha256sum` | resumo SHA-256 de arquivos (D060), como no Linux | `userland/programas/sha256sum.c` |
| `/bin/simd` | mostra e testa a FPU x87, MMX, SSE e SSE2 (D032) | `userland/programas/simd.c` |
| `/apps/bin/sintonia` | sintonia [arquivo / pasta / lista.m3u / lista.pls / http://emissora] | `userland/programas/sintonia.c` |
| `/bin/sleep` | espera alguns segundos (D097) | `userland/programas/sleep.c` |
| `/apps/bin/snake` | o jogo Snake do SaguiOS: cobrinha com recorde no perfil, `--prueba` e `--semilla` (D463) | `userland/programas/snake.c` |
| `/bin/sobre` | versão e créditos do SaguiOS-Extended (D101) | `userland/programas/sobre.c` |
| `/bin/sobre-o-sistema` | a janela "Sobre o SaguiOS-Extended" (H28, D101): versão, tamanho da imagem, processos ativos, memória e créditos | `userland/programas/sobre-o-sistema.c` |
| `/bin/sonido` | diagnóstico e testes do codec de som HDA (D107) | `userland/programas/sonido.c` |
| `/bin/sonidos` | os sons do sistema (D421): toca o de um evento e vigia o USB e a bateria | `userland/programas/sonidos.c` |
| `/bin/sort` | ordena as linhas de arquivos ou da entrada (D097) | `userland/programas/sort.c` |
| `/apps/bin/spider` | a paciência Spider do SaguiOS (D173) | `userland/programas/spider.c` |
| `/bin/split` | divide um arquivo em pedaços (D232) | `userland/programas/split.c` |
| `/apps/bin/sqlite3` | intérprete de comandos SQL sobre o SQLite de userland/terceros/sqlite (D116) | `userland/programas/sqlite3.c` |
| `/bin/ssh` | cliente SSH (D317): entrar a partir do SaguiOS em outra máquina | `userland/programas/ssh.c` |
| `/bin/ssh-keygen` | chaves ed25519 para o cliente ssh (D317) | `userland/programas/ssh-keygen.c` |
| `/bin/sshd` | servidor SSH de administração (D250, melhoria 052) | `userland/programas/sshd.c` |
| `/bin/stat` | dados de um arquivo ou pasta (D097) | `userland/programas/stat.c` |
| `/bin/su` | abre um shell com outra conta, root por padrão (D255, melhoria 098) | `userland/programas/su.c` |
| `/bin/sync` | grava o cache de blocos de todos os discos (D281) | `userland/programas/sync.c` |
| `/bin/tablasacpi` | despeja as tabelas ACPI (DSDT, SSDT, FACP...) da máquina em arquivos .dat, para estudar sua AML em outro computador (iasl -d) ou com tools/uacpi_volcado (D405). Só lê: não muda nada do sistema | `userland/programas/tablasacpi.c` |
| `/bin/tac` | escreve as linhas em ordem inversa: a última primeiro (D232) | `userland/programas/tac.c` |
| `/bin/tail` | últimas linhas (ou bytes) de arquivos ou da entrada (D097) | `userland/programas/tail.c` |
| `/bin/tar` | empacota e desempacota arquivos (melhoria 056, D103) | `userland/programas/tar.c` |
| `/bin/tareas` | o Gerenciador de tarefas (D043, D079, D216) | `userland/programas/tareas.c` |
| `/bin/teclado` | mostra ou muda o layout do teclado (ES ou ABNT2) | `userland/programas/teclado.c` |
| `/bin/tee` | copia a entrada para a saída e para arquivos (D097) | `userland/programas/tee.c` |
| `/bin/temp` | a temperatura da CPU em uma linha (D092, melhoria 050) | `userland/programas/temp.c` |
| `/bin/terminal` | o Terminal em janela (H24), com abas e preferências (D231) | `userland/programas/terminal.c` |
| `/bin/time` | executa um comando e mede quanto tempo leva (D232) | `userland/programas/time.c` |
| `/bin/timeout` | executa um comando e o encerra se passar de um tempo (D232) | `userland/programas/timeout.c` |
| `/bin/top` | os processos ao vivo, no terminal (D232) | `userland/programas/top.c` |
| `/bin/touch` | cria arquivos vazios se não existirem e define suas datas (D097, D341) | `userland/programas/touch.c` |
| `/bin/touchpad` | touchpad = painel (D407): ver panel.c | `userland/programas/touchpad.c` |
| `/bin/tr` | traduz, apaga ou comprime caracteres da entrada (D232) | `userland/programas/tr.c` |
| `/apps/bin/traductor` | editor de traduções do SaguiOS, no estilo do Poedit (D335, melhoria 133) | `userland/programas/traductor.c` |
| `/bin/tree` | mostra pastas e arquivos como uma árvore (D097) | `userland/programas/tree.c` |
| `/bin/truncate` | muda o tamanho de um arquivo (D232) | `userland/programas/truncate.c` |
| `/apps/bin/tucan` | o editor de apresentações do SaguiOS (D369), com a faixa da suíte (Pluma, Macaco) | `userland/programas/tucan.c` |
| `/bin/umount` | umount <pasta/dispositivo> - grava o que falta e desmonta (D040) | `userland/programas/umount.c` |
| `/bin/uname` | nome e versão do sistema (D097) | `userland/programas/uname.c` |
| `/bin/uniq` | tira (ou conta) linhas repetidas consecutivas (D097) | `userland/programas/uniq.c` |
| `/bin/unzip` | lista, verifica e extrai arquivos .zip (melhoria 056, D103) | `userland/programas/unzip.c` |
| `/bin/uptime` | hora atual, há quanto tempo o sistema está ligado e quantos processos existem (D097) | `userland/programas/uptime.c` |
| `/bin/usb` | diagnóstico: controladores USB 2.0, portas com dispositivo e dispositivos reconhecidos (H29, H30) | `userland/programas/usb.c` |
| `/bin/visor` | o Visualizador de imagens do SaguiOS (D055) | `userland/programas/visor.c` |
| `/bin/visorlogs` | o Visualizador de registros: o registro do kernel ou um arquivo, com filtros, cores e acompanhamento ao vivo (D463) | `userland/programas/visorlogs.c` |
| `/apps/bin/visorpdf` | o visualizador de PDF do SaguiOS (melhoria 059, D138; interface de área de trabalho completa na melhoria 105, D274) | `userland/programas/visorpdf.c` |
| `/bin/vnc` | cliente de área de trabalho remota VNC (melhoria 097 fase 2, D254) | `userland/programas/vnc.c` |
| `/bin/vncd` | servidor de área de trabalho remota VNC (melhoria 097, D252) | `userland/programas/vncd.c` |
| `/bin/volumen` | volumen [0-100 / +N / -N / silencio / activar / salida [N]] - volume geral do som (D088) | `userland/programas/volumen.c` |
| `/bin/watch` | repete um comando a cada certo tempo e mostra sua saída (D232) | `userland/programas/watch.c` |
| `/bin/wc` | conta linhas, palavras e letras (bytes) de arquivos ou da entrada (D097) | `userland/programas/wc.c` |
| `/bin/wg` | a VPN WireGuard do SaguiOS (D363) | `userland/programas/wg.c` |
| `/bin/wg-quick` | liga e desliga a VPN WireGuard (D363): "wg-quick up NOME", "wg-quick down" | `userland/programas/wg-quick.c` |
| `/bin/wget` | baixa um arquivo por HTTP ou HTTPS e o grava com o nome da URL (D062) | `userland/programas/wget.c` |
| `/bin/which` | diz qual programa seria executado para um nome (D097) | `userland/programas/which.c` |
| `/bin/whoami` | nome do usuário (D097; de verdade desde D255): a conta do uid do processo | `userland/programas/whoami.c` |
| `/bin/widget-calendario` | o mês em curso, com o dia de hoje marcado (D365) | `userland/programas/widget-calendario.c` |
| `/bin/widget-notas` | um bilhete adesivo na área de trabalho (D365) | `userland/programas/widget-notas.c` |
| `/bin/widget-relogio` | o relógio de área de trabalho (D365): analógico ou digital (um clique muda o modo; a escolha é guardada em relogio.modo de widgets.conf). Só se repinta quando muda o segundo | `userland/programas/widget-relogio.c` |
| `/bin/widget-sistema` | pequeno monitor do sistema (D365): CPU com seu gráfico, memória e rede (download e upload com o gráfico do último minuto). Uma leitura por segundo; não se repinta se os números não mudaram | `userland/programas/widget-sistema.c` |
| `/bin/widgets` | o gerenciador de widgets da área de trabalho (D365): lista os disponíveis, ativa ou desativa cada um e muda suas opções; o que fica ativo é guardado em /home/.config/widgets.conf e inicia sozinho ao começar a sessão | `userland/programas/widgets.c` |
| `/bin/wifi` | o Wi-Fi com o pendrive Realtek RTL8188EU (D215, D220) | `userland/programas/wifi.c` |
| `/bin/xargs` | constrói e executa comandos com o que chega pela entrada (D097) | `userland/programas/xargs.c` |
| `/bin/xxd` | despejo hexadecimal de um arquivo, ou o seu inverso com -r (D232) | `userland/programas/xxd.c` |
| `/bin/yes` | repete um texto (por padrão "y") até que a saída seja fechada (D097) | `userland/programas/yes.c` |
| `/bin/zcat` | zcat = gzip -dc (melhoria 056): ver gzip.c | `userland/programas/zcat.c` |
| `/bin/zip` | cria arquivos .zip (melhoria 056, D103) | `userland/programas/zip.c` |
| `/bin/zona` | fuso horário, relógio da placa e a janela "Data e hora" (D080) | `userland/programas/zona.c` |

> **Comandos que o Makefile nomeia e cujo código não está nesta branch:** `sglv6` e `es11conf`.
> Não se documentam aqui até que existam suas fontes.

---

## 5. Bibliotecas

Uma **biblioteca** é código que vários programas reutilizam. No SaguiOS há dois grupos: as próprias do
sistema (a *libc* e a *libsagui*) e as de cada tipo de aplicação.

| Biblioteca | Pasta | Para que serve |
| --- | --- | --- |
| **libsagui** | `userland/libsagui/` | Biblioteca própria do SaguiOS. Seu arquivo `chamadas.c` envolve cada chamada ao sistema (`int 0x80`, ver o guia do kernel). Traz também janelas e o kit gráfico (`gui_*.c`), áudio (`audio_*.c`), imagens (JPEG, PNG, GIF), MPEG, compressão, rede (`http.c`) e documentos (`pdf_*.c`, `presentacion_*.c`). |
| **libc** | `userland/libc/` | A biblioteca C comum: entrada e saída (`es.c`), memória (`memoria.c`), texto (`texto.c`) e a parte POSIX que o SaguiOS tem (`posix.c`: `write`, `read`, `open`...). Assim os ports de terceiros compilam quase sem mudanças. |
| **libcxx** | `userland/libcxx/` | O suporte de C++ (`rt/`) para os programas escritos em C++ (OpenJazz, DOSBox, Gearsystem...). |
| **SDL 2** | `userland/libsdl2/` | Uma SDL 2 escrita para o SaguiOS: seus cabeçalhos são próprios e o código chama as janelas e o áudio do SaguiOS (D230). Serve para portar jogos. |
| **SDL 1.2** | `userland/libsdl12/` | O mesmo para a SDL 1.2, que alguns jogos usam (D228). |
| **OpenAL** | `userland/libopenal/` | Um subconjunto próprio do OpenAL 1.1 para o som 3D dos jogos (D230). |
| **SaguiGL** (`libsgl`) | `userland/libsgl/` | O 3D próprio do SaguiOS. Desenha por software e, desde a v8.3, pode enviar os triângulos à GPU (ver [`gpu3d.md`](gpu3d.md)). Inclui cenas fixas (`cenas.c`). |
| **libamd** | `userland/libamd/` | O arcabouço do driver 3D das GPU AMD (D158). |
| **libtls** | `userland/libtls/` | Criptografia e TLS: AES, SHA, GCM, números grandes e as conexões HTTPS. Quem as usa, entre outros, são `curl`, `wget` e o navegador. |
| **libdl** | `userland/libdl/` | `dlopen`, `dlsym` e `dlclose` dentro do `libsagui.so` (D378, [`enlace-dinamico.md`](enlace-dinamico.md)). |
| **libfontes** | `userland/libfontes/` | O motor de fontes: famílias, carregamento de fontes, subconjuntos e texto. O Pluma o usa como provedor de fontes de documento (D319). |
| **web** | `userland/web/` | O motor da web do Cipó: CSS, JavaScript com DOM, Canvas 2D, SVG, WebGL por software e o descompressor Brotli para as fontes WOFF2 (D141). |
| **pdf** | `userland/pdf/` | O motor de PDF do visualizador (D138). |
| **llm** | `userland/llm/` | Um motor próprio para modelos de linguagem locais (formato GGUF), com o comando `llm` em `/apps/bin` (D380). |
| **IA e correio** | `userland/ia/`, `userland/mail/` | As bibliotecas do cliente de IA (`userland/ia/`) e do cliente de correio (`userland/mail/`). São compartilhadas, entre outros, por `ia`, `chatia`, `pluma`, `macaco`, `tucan` e `correo`. |
| **MicroPython** | `userland/micropython/` | Python, com a "cola" do SaguiOS (`arquivos_sagui.c`, `janela_sagui.c`). A parte do MicroPython é de terceiros (D034). |
| **litemonkey** | `litemonkey/` | Banco de dados documental próprio, em C (D117). Fica fora de `userland/`. |
| **Ligadores** | `userland/programa.ld`, `userland/programa_cxx.ld` | Dizem como o código de um programa em C ou C++ é posicionado. |
| **Cabeçalhos da API** | `userland/inc/` | Os cabeçalhos OpenGL ES e EGL, além dos da libc. |

---

## 6. Ports de terceiros

Um **port** é um programa livre de outro autor que foi adaptado para rodar no SaguiOS. Cada pasta
tem um arquivo `leeme.txt` com as mudanças. O registro de tudo o que é alheio, com licença e
origem, está em [`terceros.md`](terceros.md).

| Port | Pasta | O que é | Comando | Decisão |
| --- | --- | --- | --- | --- |
| NetSurf | `userland/terceros/netsurf/` | navegador web leve | `/apps/bin/netsurf` | D184 † |
| Freeciv 3.1.6 | `userland/terceros/freeciv/` | estratégia por turnos; servidor e cliente SDL 2 | `/apps/bin/freeciv-server`, `/apps/bin/freeciv-sdl2` | D374 † |
| OpenTTD | — (não está nesta branch) | simulador de transporte | `/apps/bin/openttd` (se existir) | D375 † |
| Chocolate Doom 3.1.1 | `userland/terceros/chocolatedoom/` | DOOM, Heretic, Hexen e Strife; com o conteúdo livre Freedoom | `/apps/bin/chocolate-doom` | D383 † |
| Quake | `userland/terceros/quake/` | o motor do Quake, com seu render por software | `/apps/bin/quake` | D229 † |
| Vanilla Conquer | `userland/terceros/conquer/` | Command & Conquer (Tiberian Dawn e Red Alert) | `/apps/bin/conquer` | D230 † |
| OpenJazz | `userland/terceros/openjazz/` | Jazz Jackrabbit | `/apps/bin/openjazz` | D228 † |
| OpenTyrian | `userland/terceros/opentyrian/` | Tyrian 2.1 (os dados ficam na partição SAGUI-JOGOS) | `/apps/bin/opentyrian` | D180, D181 † |
| Tzar | `userland/juegos/tzar/` | port em C do ReTzar | `/apps/bin/tzar` | D417 † |
| AoEN | `userland/juegos/aoen/` | jogo de estratégia em tempo real deste projeto | `/apps/bin/aoen` | D382 |
| DOSBox 0.74-3 | `userland/terceros/dosbox/` | emulador de MS-DOS | `/apps/bin/dosbox` | D294 † |
| Recreativa | `userland/terceros/recreativa/` | lançador de jogos de DOS (código próprio) | `/apps/bin/recreativa` | D294 † |
| GNUBoy | `userland/terceros/gnuboy/` | Game Boy e Game Boy Color | `/apps/bin/gnuboy` | D235 † |
| mGBA 0.10.5 | `userland/terceros/mgba/` | Game Boy Advance | `/apps/bin/mgba` | D322 |
| Gearsystem | `userland/terceros/gearsystem/` | Master System, Game Gear e SG-1000 | `/apps/bin/gearsystem` | D321 |
| LaiNES | `userland/terceros/laines/` | NES (Famicom) | `/apps/bin/laines` | D321 |
| Entrada comum de emuladores | `userland/terceros/emucomun/` | o ponto de entrada que os emuladores de consoles compartilham (código próprio) | — | D321 |
| TCC 0.9.28rc | `userland/terceros/tcc/` | compilador de C; também é a libtcc que usa o Laboratório de C | `/apps/bin/tcc` | — |
| GNU make 4.4.1 | `userland/terceros/make/` | construção de projetos | `/apps/bin/make` | — |
| git (libgit2) | `userland/terceros/git/` | controle de versões | `/apps/bin/git` | D400 |
| MicroPython | `userland/micropython/` | Python | `/apps/bin/python` | D034 |
| GNU Prolog 1.5.0 | `userland/terceros/gprolog/` | Prolog | `/apps/bin/gprolog`, `/apps/bin/pl2wam` | D189 |
| miniJVM | `userland/terceros/minijvm/` | Java | `/apps/bin/java`, `/apps/bin/javac` | D376 |
| SQLite | `userland/terceros/sqlite/` | banco de dados | `/apps/bin/sqlite3` | D116 |
| Shine | `userland/terceros/shine/` | codificador MP3 de ponto fixo (usado pelo Conversor de áudio) | — (biblioteca) | — |
| openrsync | `userland/terceros/openrsync/` | `rsync` como cliente por SSH | `/apps/bin/rsync` | D314 |
| zlib 1.3.1 | `userland/terceros/zlib/` | biblioteca de compressão de dados | — (biblioteca) | — |
| Sons "Kenney" | `userland/terceros/kenney-sonidos/` | sons da interface (dados, com sua licença) | — (dados) | D421 |
| Ícones "Fluent UI" | `userland/terceros/fluentui/` | ícones da biblioteca do sistema (dados, com sua licença) | — (dados) | D135 |

Os jogos deste projeto (Sagui Atrapa Bananas, Solitario Spider, Buscaminas, Sagui 3D e CHIP-8)
não são ports: estão em `userland/programas/`, `userland/sagui3d/` e `userland/chip8/`.

---

## 7. Onde ler mais

| Tema | Documento |
| --- | --- |
| O kernel e as chamadas ao sistema | [`guia-kernel_PT.md`](guia-kernel_PT.md) |
| Todas as decisões de projeto | [`decisoes.md`](decisoes.md) |
| Código de terceiros, licenças e origem | [`terceros.md`](terceros.md), [`licencia.md`](licencia.md) |
| Os ports de jogos, um a um | [`doom.md`](doom.md), [`freeciv.md`](freeciv.md), [`tzar.md`](tzar.md), [`aoen.md`](aoen.md), [`dosbox.md`](dosbox.md), [`mgba.md`](mgba.md), [`openttd.md`](openttd.md), [`carencias-netsurf.md`](carencias-netsurf.md) |
| Compilar ferramentas e programas | [`make.md`](make.md), [`tcc.md`](tcc.md), [`git.md`](git.md), [`go.md`](go.md) |
| Aplicações de área de trabalho | [`pluma.md`](pluma.md), [`cipo.md`](cipo.md), [`macaco.md`](macaco.md), [`visualpythons.md`](visualpythons.md), [`litemonkey.md`](litemonkey.md), [`camara.md`](camara.md), [`microfono.md`](microfono.md) |
| Jogos e emuladores de DOS, Game Boy e mais | [`recreativa.md`](recreativa.md), [`mandos.md`](mandos.md) |

---

> *Este guia foi gerado a partir do código em 2026-11-21; se algo não bater, confie no código.*
