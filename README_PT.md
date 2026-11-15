<!-- Traducción de README.md en el commit 36ef0600 (sincronía: tools/readme_sincronia.py) -->
<div align="center">

# 🐒 SaguiOS-Extended

[Español](README.md) · [Português](README_PT.md) · [English](README_EN.md)

*Baseado no [SaguiOS](https://github.com/erigobeli/sagui-os) de Eli Rigobeli.*

**Um sistema operacional escrito do zero por IA**
para PC x86 de 32 bits, funcionando de verdade em um notebook de 2011.

Feito pelo Claude (Anthropic) para o projeto de **Eli Rigobeli**,
e mantido por **Alejandro Anuncibay**, com melhorias escritas com o Claude
(Anthropic) e com um agente de IA (MiniMax Code).

`562 KB` · `sem GRUB` · `sem libc` · `sem código de outros sistemas (exceto drivers, D073)` · `mais de 130 testes automáticos`

<img src="docs/capturas/B4-3-fase1-corredor.png" width="49%" alt="Sagui 3D, o jogo em primeira pessoa do SaguiOS">
<img src="docs/capturas/H28-tres-programas.png" width="49%" alt="Área de trabalho do SaguiOS com três programas abertos">

</div>

> A documentação detalhada (`docs/`, `mejoras/`) está em espanhol.

---

## O que é

> **Nota sobre esta versão:** esta é uma tradução para o espanhol do projeto original, escrito em
> português do Brasil. A interface do sistema está em espanhol («Archivos», «Configuración», «Terminal»…)
> e o teclado aceita as distribuições **ES** (padrão) e **ABNT2**, que se escolhem com o programa `teclado`
> (`teclado es` / `teclado abnt2`). Os identificadores do código, os comentários, os nomes de arquivo e os comandos
> são mantidos em português.

O SaguiOS é um sistema operacional completo, desde o carregador de boot até o jogo em 3D, escrito
inteiramente neste projeto, seguindo a especificação de [`prd.md`](prd.md). Inicia a partir de um pendrive em um
**Lenovo G470** (2011), no VirtualBox e no QEMU, e cabe em **28.959.232 bytes**: 12,2 MB
de boot (kernel e ramdisk) e 16 MB das partições do sistema, a de jogos e a de aplicativos
(a meta da imagem é de 38 MB desde a v7.0b, D181; antes eram 16 MB; a versão original, sem as melhorias, ocupava 575.488 bytes, os
39% de um disquete de 1,44 MB, que era a meta do PRD até a D050).

Tudo o que está dentro da imagem foi escrito aqui, exceto o que é de terceiros: carregador de boot em dois estágios, kernel,
drivers (teclado ES e ABNT2, touchpad, vídeo VESA, USB 2.0, pendrive), sistema de arquivos FAT32,
gerenciador de janelas, a biblioteca dos programas (`libsagui`), o shell, os programas, a fonte
tipográfica de 8x16 e os desenhos, exceto o que de terceiros lista
[`docs/terceros.md`](docs/terceros.md) (as faces DejaVu do sistema e os ícones Fluent, MIT). Cada decisão de projeto está explicada em
[`docs/decisoes.md`](docs/decisoes.md) e cada erro encontrado, com sua causa e sua correção, em
[`docs/erros.md`](docs/erros.md).

## Funcionando no hardware real

Fotos do Eli em cada fase, no Lenovo G470:

| Fase 1: boot | Fase 2: kernel e terminal | Fase 3: processos e shell |
| :---: | :---: | :---: |
| <img src="testes_eli/fase_1_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_2_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_3_no_notebook.jpg" width="260"> |
| **Fase 4: gráficos e mouse** | **Fase 5: janelas e programas** | **Extra: Sagui 3D (VM)** |
| <img src="testes_eli/fase_4_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_5_no_notebook.jpg" width="260"> | <img src="testes_eli/fase_bonus_vm.png" width="260"> |

## As fases

| Fase | O que foi construído | Na tela |
| --- | --- | :---: |
| **1. Boot** | Carregador próprio em dois estágios (MBR + estágio 2), linha A20, mapa de memória E820, cópia acima de 1 MB em *unreal mode*, soma de verificação, modo protegido, kernel em C | <img src="docs/capturas/H04-boot-vdi.png" width="200"> |
| **2. Kernel e terminal** | GDT, IDT, TSS, tela de erro, PIC, relógio a 100 Hz, memória física, `kmalloc`, teclado ABNT2 com acentos (e ES nesta versão), sistema de arquivos SGFS | <img src="docs/capturas/H09-ls-cd.png" width="200"> |
| **3. Processos e programas** | Paginação com um espaço por processo, escalonador preemptivo, anel 3, chamadas de sistema, carregador ELF, pipes, shell `sagsh` | <img src="docs/capturas/H18-edicao-e-segundo-plano.png" width="200"> |
| **4. Gráficos e mouse** | VESA 1024x768x32 com buffer duplo, fonte 8x16 própria com acentos, terminal gráfico, touchpad PS/2 | <img src="docs/capturas/H18-fonte-acentos.png" width="200"> |
| **5. Janelas e programas** | Área de trabalho, gerenciador de janelas, Archivos, Terminal, Editor, Calculadora, Sagui Atrapa Bananas | <img src="docs/capturas/H28-programas-abertos.png" width="200"> |
| **6. Pendrive de dados** | PCI, driver USB 2.0 (EHCI) com hubs e conexão a quente, armazenamento USB, FAT32 com leitura e escrita | <img src="docs/capturas/H34-2-arquivos-no-pendrive.png" width="200"> |
| **Extra** | B3: bipe pelo alto-falante do PC. B4: **Sagui 3D**. (B1 e B2 cancelados pelo Eli.) | <img src="docs/capturas/B4-6-macaco-tonto.png" width="200"> |

## Sagui 3D

O sagui ficou preso em um labirinto de pedra em plena Mata Atlântica. Ele precisa juntar bananas
para abrir a grade de cada fase, enquanto uns macacos-prego guardiões tentam tirar as bananas
do bolso dele. A funda atira sementes de jabuticaba (com três, o macaco fica atordoado e foge),
cada banana comida deixa uma casca que faz o macaco escorregar, e cada fase esconde um cacho
dourado atrás de uma parede de cipós.

<div align="center">
<img src="docs/capturas/B4-1-tela-de-inicio.png" width="32%">
<img src="docs/capturas/B4-7-macaco-escorregou.png" width="32%">
<img src="docs/capturas/B4-5-vitoria.png" width="32%">
</div>

O motor 2,5D foi projetado a partir da geometria, sem consultar nenhum código, tutorial nem
repositório de jogos: salas convexas com altura própria de chão e de teto, desenho coluna a
coluna com uma «fenda» que se estreita a cada passo, perspectiva sem olho de peixe, ponto fixo
(os programas não usam a FPU), chão e teto texturizados e luz por sala. São três fases em
arquivos de texto, com texturas e personagens desenhados no próprio código. Explicação completa
na seção «Sagui 3D» de [`docs/decisoes.md`](docs/decisoes.md).

## Extras: GNU Prolog (código de terceiros)

- **GNU Prolog 1.5.0** (D189, v7.2): `gprolog` (ou `prolog`) abre o interpretador com a biblioteca ISO;
  `['/ejemplos/familia.pl'].` carrega o exemplo de parentesco e listas. LGPL-3/GPL-2, isolado em
  `userland/terceros/gprolog/` e fora da variante sem GPL. Teste: `tests/h156_gprolog.py`.

## Extras: OpenTyrian (código de terceiros)

- **OpenTyrian** (D156, v6.6): o shoot 'em up vertical de 1995 com o motor do projeto original (GPL-2.0),
  isolado em `userland/terceros/opentyrian/` e com uma plataforma própria que faz o papel do SDL 1.2 sobre as
  janelas do SaguiOS. Não entra na variante sem GPL, nem o jogo nem os seus dados.
- Desde a v7.0b está em **Início > Jogos > OpenTyrian** (D180) e seus dados (Tyrian 2.1) ficam em uma
  partição ext2 da própria imagem, `SAGUI-JOGOS`, que o sistema monta ao iniciar em `/jogos` (de
  leitura e escrita desde a v7.4, D228), com os dados em `/jogos/opentyrian` (D181): não ocupam RAM
  (o ramdisk não cresce).
- O jogo **encontra os dados sozinho**: em `/jogos/opentyrian` e em `jogos/opentyrian` de cada disco
  montado (`/usb`, `/datos`…). No CD, ou com uma imagem sem a partição, eles são colocados em um pendrive
  (`tools/imagem_tyrian.sh` monta isso a partir do hospedeiro). Se não os encontrar, uma janela explica como
  consegui-los e oferece «Buscar otra vez» («Procurar de novo»). Testes: `tests/h136_opentyrian.py`, `tests/h150_opentyrian_menu.py`.

## Extras: OpenJazz (código de terceiros, C++)

- **OpenJazz** (D228, v7.4): o motor livre de *Jazz Jackrabbit* (1994), versão 20260301 do projeto
  original (GPL-2.0), em `userland/terceros/openjazz/` sem alterar uma linha. É **o primeiro programa de C++**
  do sistema: traz um runtime de C++ próprio (`userland/libcxx/`: `new`/`delete`, exceções com um
  desenrolador DWARF nosso, alguns cabeçalhos mínimos da biblioteca padrão) e o SDL 1.2 comum
  (`userland/libsdl12/`). Fica em `/apps/bin/openjazz` e em **Início > Jogos > OpenJazz** (também em tela cheia);
  não entra na variante sem GPL.
- **Os dados do jogo não são livres** e não vêm com o SaguiOS. O OpenJazz os procura em qualquer pasta de
  `/jogos` (ou em `jogos/` de um pendrive) e prefere a versão completa à shareware. Para tê-los:
  `tools/openjazz_datos.sh` baixa a shareware (episódio 1) antes de compilar, ou coloca-se o jogo completo em
  `input_share/games/` ([docs/input_share.md](docs/input_share.md)); ou copiam-se para `/jogos` a partir do SaguiOS.
  Sem os dados, uma janela explica como consegui-los. A configuração e as partidas vão para `/home/openjazz`.
  Teste: `tests/h228_openjazz.py`.

## Extras: DOSBox e Recreativa (jogos de DOS)

- **DOSBox 0.74-3** (D294, v8.3.1): o emulador de PC e de MS-DOS clássico (GPL-2.0 ou posterior), em
  `userland/terceros/dosbox/` com um patch de três linhas. Núcleo normal e **recompilador dinâmico de x86**
  (cerca de 9 vezes mais rápido em um laço de 32 bits, medido com KVM), Sound Blaster 16, AdLib, alto-falante,
  mouse capturado e tela cheia. Para o porte, o runtime de C++ ganhou **RTTI** (`dynamic_cast`, `typeid`),
  `<map>`, fluxos (`<iostream>`, `<sstream>`, `<fstream>`) e mais. Fica em `/apps/bin/dosbox` e em
  **Início > Jogos**; um `.exe`, `.com` ou `.bat` abre-se com ele a partir de Arquivos; não entra na variante sem GPL.
- **Recreativa** é o lançador: uma lista de jogos com a aparência do Windows 98 (grade com capa ou detalhes),
  que encontra sozinha as pastas de `/home/dos` e `/jogos` com programas de DOS e guarda, para cada jogo, o
  núcleo, os ciclos, a memória, a placa de som, um CD (pasta ou imagem ISO/CUE) e os comandos prévios.
  **Os jogos não vêm com o sistema** nem vão para o git. Guia: [docs/dosbox.md](docs/dosbox.md). Teste:
  `tests/h294_dosbox.py`.

## Extras: Quake (código de terceiros)

- **Quake** (D229, v7.4.1): o motor de *Quake* (1996) que a id Software publicou com licença GPL-2.0, o
  WinQuake com seu **renderizador por software**, em `userland/terceros/quake/` sem alterar uma linha (C puro,
  sem o assembly). Sobre o SDL 1.2 comum: 320x200 em uma janela ampliada ao dobro (ou 640x480), tela cheia em
  4:3, som a 22050 Hz. Fica em `/apps/bin/quake` e em **Início > Jogos > Quake** (também em tela cheia);
  não entra na variante sem GPL. Sem rede nem música de CD.
- **Os dados do jogo não são livres** e não vêm com o SaguiOS. O Quake os procura em qualquer pasta de
  `/jogos` (ou em `jogos/` de um pendrive): `id1/pak0.pak`, ou o `pak0.pak` solto; prefere a versão
  registrada. Para tê-los: `tools/quake_datos.sh` baixa a shareware 1.06 (episódio 1) antes de compilar, ou
  coloca-se o jogo em `input_share/games/Quake/` ([docs/input_share.md](docs/input_share.md)). Sem os dados,
  uma janela explica como consegui-los. A configuração e as partidas vão para `/home/quake/id1`.
  Teste: `tests/h229_quake.py`.

## Extras: Game Boy (GNUBoy, código de terceiros, C)

- **SDL2-GNUBoy** (D235, v7.4.2): o emulador de **Game Boy e Game Boy Color** (GPL-2.0, commit `c8642d1`),
  em `userland/terceros/gnuboy/` sem alterar uma linha, sobre o SDL 2 próprio (`userland/libsdl2/`). Fica em
  `/apps/bin/gnuboy` e em **Início > Jogos > Game Boy (GNUBoy)**; não entra na variante sem GPL. Janela
  de 480x432 (160x144 vezes 3), tela cheia com a proporção e as faixas (F11), som, setas, Z/X
  (A/B), Intro (Start), Maiúsc (Select), F5/F7 para salvar e carregar o estado; a bateria do cartucho
  (`.sav`) e os estados vão para `/home/gnuboy`.
- **As ROMs não vêm com o SaguiOS.** São procuradas em `/jogos/gb`, `/home/gnuboy/roms` e nos discos montados, e
  uma janela deixa escolher (nome do cabeçalho do cartucho); sem ROM, outra explica onde colocá-las.
  `input_share/games/gb/` as coloca na imagem ([docs/input_share.md](docs/input_share.md));
  `tools/gnuboy_roms_libres.sh` baixa uma ROM livre (Libbet, zlib) para testar. Teste:
  `tests/h235_gnuboy.py` (a parte «unidade» roda no PC; ROM de teste própria em
  `tools/gnuboy_rom_teste.py`).

## Extras: programar dentro do SaguiOS

- **`cc`, um compilador de C escrito neste projeto** (B5): compila dentro do próprio
  SaguiOS um subconjunto amplo de C (int/char/short/unsigned, ponteiros, vetores, struct,
  union, enum, typedef, ponteiros para função, `switch`, macros com parâmetros,
  `#include "..."`, vários arquivos de origem (D133), `long long` (D128), `float`/`double` com SSE2 ou, em CPU sem SSE2, com a x87) diretamente para um executável ELF,
  sem assembler nem linker, com `printf`, `malloc`, `<math.h>` e companhia incluídos.
  `cc /ejemplos/hola.c -o /tmp/hola` e depois `/tmp/hola`; teste também
  `/ejemplos/mandelbrot.c`. Como foi pensado: D033 e D039 em [`docs/decisoes.md`](docs/decisoes.md).
- **`pascal`, um compilador de Pascal também próprio** (melhoria 040): Pascal no estilo do Turbo Pascal (inteiros, `real` com SSE2 ou x87, string, conjuntos, registros com variante,
  ponteiros, arquivos, procedimentos aninhados, `{$R+}`) direto para um executável ELF, com o mesmo
  gerador de código do `cc`. Unidades `crt` (`clrscr`, `gotoxy`, `readkey`...) e
  `sagui` (gráficos em uma janela). `pascal /ejemplos/pascal/hola.pas -o /tmp/hola`; mais
  exemplos em `/ejemplos/pascal` e o manual em `/ayuda/pascal.md`. Detalhes: D085-D087.
- **libsagui** (v6.1, D126): componentes novos, tela com imagens, barra de progresso e caixa de seleção;
  e a biblioteca de ícones (`icones`, D135), com 112 ícones de 16x16.
- **`qjs`, JavaScript no terminal** (D188, v7.2): o QuickJS do navegador (D149) como interpretador:
  `qjs arquivo.js [args]`, `qjs -e 'código'` e um REPL com histórico. `console`, `print`, temporizadores,
  módulos ES locais e os módulos `std`/`os` (arquivos, ambiente, `sleep`). Erros com traceback e código de saída 1.
- **`python`, MicroPython 1.24** (B6): REPL no terminal, `python arquivo.py` e
  `python -c`. **É código de terceiros** (licença MIT), a única exceção à regra de
  autoria, isolado em `userland/micropython/` e opcional: `make MICROPYTHON=0` gera a
  imagem sem ele (D034). Traz `float` em double por SSE2 ou x87 (D038, D082) e uma biblioteca padrão
  (melhoria 032, D067-D069):
  - arquivos com `open()` (texto em Latin-1, como o Editor, ou binário), `io.StringIO`/`BytesIO`,
    `sys.stdin`/`stdout`/`stderr`; `str` unicode (`len("ñ") == 1`) com acentos no terminal;
  - `import` de `.py` e pacotes: a pasta do script e `/lib/python` estão em `sys.path`;
  - módulos `os` e `os.path`, `sys`, `io`, `json`, `re`, `random`, `time` (com `localtime`,
    `strftime`), `struct`, `collections` (`deque`, `namedtuple`, `OrderedDict`,
    `defaultdict`), `heapq`, `binascii`, `hashlib` (md5, sha1, sha256, sha384, sha512),
    `errno`, `array`, `select`, `math`, `cmath`, `gc`, `micropython`, e `sqlite3` (SQLite 3.54.1, subconjunto da
    DB-API 2.0; melhoria 064, D116, [`docs/sqlite.md`](docs/sqlite.md));
  - rede: `socket` (TCP cliente e servidor, UDP, DNS), `ssl` (a libtls, com os certificados de
    `/etc/ssl/certs`) e, em `/lib/python`, `requests`, `urllib.parse` e `litemonkey`;
  - o heap começa com 4 MB e cresce sozinho (`-X heap=16M` o fixa). Exemplos:
    `/ejemplos/archivos.py`, `/ejemplos/red.py` e `/ejemplos/json_http.py`;
  - **janelas** (melhoria 033, D083-D084): o módulo `sagui` (em C: `sagui.Ventana` com
    retângulos, elipses, polígonos, texto, imagens BMP/PNG, eventos do mouse e do teclado,
    o menu de contexto e um laço que dorme até o próximo evento) e, por cima, um
    **`tkinter`** escrito em Python com a aparência do SaguiOS: `Tk`, `Button`, `Entry`, `Text`,
    `Listbox`, `Canvas`, `Scale`, `Menu`, `pack`/`grid`/`place`, `StringVar`, `bind`, `after`,
    `messagebox`, `simpledialog`, `filedialog` e um `ttk` mínimo. Teste
    `python /ejemplos/tk/calculadora.py` ou `/ejemplos/tk/juego.py`; o que há, em
    `/ayuda/python-tk.md`;
  - sem SSE2 (Pentium III, Athlon XP), o `python` usa só `/bin/python-x87`, com os mesmos
    resultados (D082).
- **`simd`**: os programas podem usar a FPU x87, MMX e SSE; o kernel guarda o estado
  de cada processo (D032).
- **`pantalla`** (tela): lista os modos de vídeo e muda a resolução; no QEMU, Bochs e
  VirtualBox sem reiniciar (`pantalla 1280x1024`), em um PC de verdade no próximo boot
  (`--guardar` a deixa no disco de boot). O framebuffer usa *write-combining* quando
  a CPU permite (D057-D059).

## Extras: PCs antigos (melhoria 036, fase A)

- **Sem SSE2 automaticamente** (Pentium III, Athlon XP): o kernel e a libsagui copiam
  com `rep movsd`, o `cc` gera código x87 (`-sse2`/`-x87` forçam) e o `simd` testa só
  o que há (D076). O Python com números reais ainda precisa de SSE2.
- **CD inicializável:** `make iso` → `build/saguios.iso` (El Torito, feita por
  `tools/mkiso.py`); `./run_in_qemu.sh --cd`. A partir do CD não se salva nada no disco de
  boot (D077).
- **Vídeo a 16 e 15 bits** (RGB565/RGB555) para placas de 2-8 MB: a área de trabalho segue a 32
  bits e se converte ao copiar para a placa (em C ou com SSE2); `mkimagem.py --bpp 16` o
  prefere (D078).
- **`infopc --informe`**: um `.txt` com a CPU, a memória, cada dispositivo PCI com seu driver
  (ou a melhoria que o prevê), os modos VESA, os discos, as tabelas ACPI e o log do kernel,
  para enviar. Máquinas testadas: [`docs/compatibilidad.md`](docs/compatibilidad.md).
- **Monitor de hardware** (D092, melhoria 050): `sensores` e `temp` mostram a temperatura da
  CPU (Intel por MSR; AMD K8, 10h-16h e Ryzen por PCI/SMN), a frequência real (APERF/MPERF),
  o multiplicador, os P-states, a tensão pedida, o turbo e o microcódigo; também na
  aba CPU do `infopc`, no relatório e na aba Desempenho do Gerenciador de tarefas (gráficos de 60 s, em amarelo perto do máximo e em vermelho se a CPU limita). É
  **somente leitura**: um MSR que não existe não derruba o sistema (o #GP é absorvido), e a placa-mãe
  (ventiladores, SPD) não é tocada. Em uma máquina virtual, «não disponível».
- **SaguiMark** (D093, melhoria 051): `saguimark [--rapido]` ou a aba Benchmark do `infopc`
  medem inteiros, FPU x87, SIMD (SSE2) e memória com testes verificados (uma CPU que calcula
  errado dá «ERROR») e dão uma pontuação; 1000 = referência, por enquanto provisória (QEMU/KVM; a
  definitiva será o HP dv7). O resultado fica salvo em `/home/saguimark-AAAAMMDD-HHMM.txt` e
  é comparado com `/lib/saguimark/referencias.txt`.
- **`benchmark`** (D176): `benchmark [--mono | --multi [N]] [--rapido]` mede oito cargas com um núcleo e com todos ao mesmo tempo (processos e threads), com pontuação (1000 = QEMU com KVM e uma CPU), fator de escala e eficiência; também na aba Desempenho do `infopc`. O histórico fica em `/home/.benchmark`.
- **SaguiGL** (D104, melhoria 054 v5): `libsgl`, API de OpenGL ES 1.1 por software (matrizes com
  pilha, `glFrustumf`/`glOrthof`, `glDrawArrays`/`glDrawElements`/`glBegin`, buffer Z de 16 ou
  32 bits, Gouraud e flat, recorte contra o frustum, descarte de faces traseiras; desde a v6, além disso, texturas,
  mistura, névoa e luzes, com caminho SIMD (D130)). O `sgldemo` mostra um cubo girando com os
  fps (`-t N` mede sem janela). O teste `tests/h77_sgl.py` compara o resultado, pixel a
  pixel (soma), com o mesmo motor compilado no hospedeiro. O `infopc` (aba Vídeo)
  identifica a família da GPU pelo seu PCI (AMD R300-Evergreen, Intel Gen2-Gen6) e diz se
  o SaguiGL teria motor por hardware.
- **Armação dos drivers 3D da AMD** (D158, melhoria 054 B2, `/bin/amd`): a tabela de famílias
  (Evergreen, R5xx e R300/R420, e R6xx/R7xx), os registradores de identificação e a leitura dos
  registradores MMIO do BAR. É a armação e o mínimo do kernel para lê-los, não o motor 3D por GPU.
- **Discos SATA em modo AHCI** (D155, melhoria 087): entram na mesma camada de discos que os IDE,
  com as mesmas regras de somente leitura. O USB 3.0 (xHCI) fica como esqueleto, conforme
  [`docs/usb3.md`](docs/usb3.md).

## Extras: lite-monkey, o banco de dados de documentos

- **`litemonkey`** (melhoria 067, D117): banco de dados **documental** próprio, em C, inspirado no
  MongoDB mas mais básico, na **porta 7979**. O mesmo código funciona no PC
  (`make -C litemonkey`) e no SaguiOS (`litemonkey &`; dados em `/home/litemonkey`). Documentos
  BSON, CRUD com operadores de consulta e atualização, índices (com `unique` e multichave),
  agregação básica (`$match`, `$group`, `$sort`...), cursores, arquivo somente de acréscimo com CRC
  que se recupera depois de um corte de luz e se compacta renomeando, e autenticação HMAC-SHA256
  opcional.
- **Cliente Python** `litemonkey` com API parecida com a do pymongo, o mesmo no CPython e no
  MicroPython do SaguiOS: `c = litemonkey.Cliente("127.0.0.1"); c.loja.produtos.find({...})`.
  Console **`lmk`** (`show dbs`, `use`, `db.col.find(...)`, `it`). Do PC ao SaguiOS com o redirecionamento de portas do QEMU; dentro do SaguiOS por 127.0.0.1. Manual:
  [`docs/litemonkey.md`](docs/litemonkey.md).

## Extras: Mico, o editor de código

- **Mico** (`mico pasta`, ou Início > Acessórios): explorador da pasta, abas, cores para C, Python, sh, Markdown e assembly de CHIP-8, desfazer ilimitado,
  recuo automático, comentar com Ctrl+/, mover linhas com Alt+↑/↓, buscar e substituir,
  buscar em toda a pasta, ir para a linha, paleta de comandos (Ctrl+Maiúsc+P) e temas claro e
  escuro. **F5** compila e executa no terminal de baixo (C, Pascal, Python, sh); os erros do `cc` e do `pascal` e os tracebacks
  do Python saem em **Problemas** e um clique leva à linha. Salva de forma atômica e faz
  cópias de recuperação a cada 5 s. Teste com `mico /ejemplos/mico`; manual em
  `/ayuda/mico.md`. Detalhes: D075.
- **Área de transferência do sistema:** copiar e colar texto entre o Mico, o Editor, o Markdown e
  o Terminal (no Terminal, Ctrl+Maiúsc+C/V ou o botão direito). Detalhes: D074.

## Extras: planilha e e-mail

- **Macaco** (D142, v6.3): planilha com grade, fórmulas típicas (soma, média, mínimo,
  máximo, contar, se, arredondar…), gráficos de barras e de pizza, e arquivos próprios `.mxs`
  (Macaco eXcel Sheet).
- **Pluma** (D291, v8.3): processador de textos com a aparência de um Word 97 simplificado (menus, barras de
  ferramentas com ícones, régua, visualização de página, fontes, parágrafos, localizar e substituir). Lê e grava **RTF**
  e texto, e **exporta para PDF**. Sem macros.
- **Cliente de e-mail** (D146, v6.3 e v6.6): recebe por POP3 ou IMAP e envia por SMTP, com janela,
  lista de mensagens, anexos e corpo em texto puro ou em HTML (convertido em texto).

## Extras: barra de tarefas e menu Início

- **Botão Início** (ou a tecla Windows, ou Ctrl+Esc): os programas mais usados em cima, os
  demais agrupados em submenus (Acessórios, Gráficos, Internet, Jogos, Sistema), e embaixo
  «Ejecutar...» («Executar...»), «Configuración de pantalla» («Configuração de tela»), «Acerca de SaguiOS» («Sobre o SaguiOS») e «Reiniciar...» («Reiniciar...»)
  («Apagar» («Desligar») aparece desativada: ainda não há ACPI).
- **Um botão por janela** na barra superior: clique para trazê-la para frente ou minimizá-la,
  botão direito para o menu da janela; se não couberem, encolhem-se e as que sobram vão para
  «»». Clique no relógio: a data completa; botão direito: «Ajustar fecha y hora...» («Ajustar data e hora...»).
- **Botão direito na área de trabalho:** «Configuración de pantalla...» e o submenu
  «Resolución» («Resolução»), que muda o modo na hora. Detalhes: D064-D066.
- **Bandeja** à esquerda do relógio: volume (clique: controle deslizante e «Silenciar»; a
  roda sobre o ícone sobe e desce), rede (o IP ao passar o mouse; clique: IP, gateway,
  DNS e «Renovar DHCP») e temperatura da CPU (só se puder ser lida: não no QEMU; laranja
  a partir de 70 °C e vermelha a partir de 79 °C). Detalhes: D114, teste `tests/h85_bandeja.py`.
- **Atalhos da área de trabalho** (v6.0 beta): Alt+F4 fecha a janela ativa (o programa recebe o aviso,
  como com o X), Alt+Tab troca de janela (Alt+Maiúsc+Tab, para a anterior), a tecla Windows abre o
  menu, Windows+D mostra a área de trabalho e Ctrl+Alt+Del abre o Gerenciador de tarefas.
- **Capturas de tela** (beta 3, D125): Impr Pant, Alt+Impr Pant, Windows+Maiúsc+S, o programa
  Recortes e o comando `captura`.
- **Utilitário «Discos»** (v6.0 beta): cada disco e partição com uma barra de espaço ocupado e livre,
  sistema de arquivos, rótulo e ponto de montagem; montar, desmontar e abrir em Archivos. O comando
  `discos` do terminal mostra também o uso.
- **Explorador de arquivos** (v6.3, D136): ver os ocultos (Ctrl+H), visualizações de detalhes e miniaturas,
  barra de caminho e painel lateral, seleção múltipla, copiar, recortar, colar e arrastar, renomear,
  buscar, «Abrir con» («Abrir com»), propriedades e lixeira. O Alt+Tab mostra miniaturas.

## Extras: imagens e desenho

- **Visualizador de imagens** (`visor`, ícone «Visor»): abre BMP, PPM/PGM/PBM, **PNG** (com um
  descompressor *deflate* escrito aqui) e **JPEG** de linha de base (fotos de câmeras e celulares; o progressivo e o de 12 bits
  não são lidos; melhoria 047), ajustado à janela ou no tamanho real, com zoom
  (roda, `+`/`-`), arrastar, imagem anterior/seguinte da pasta (AvPág/RePág) e
  tela cheia (F11). Archivos abre as imagens com ele. Há exemplos em `/imagenes`,
  desenhados por `tools/mkimagens.py`. Detalhes: D054 e D055.
- **Paint** (`paint`, ícone «Paint»): lápis, pincel, borracha, aerógrafo, linhas, retângulos e
  elipses (contorno ou preenchidos), balde de preenchimento, conta-gotas e texto; cor principal (clique
  esquerdo) e secundária (direito); desfazer de vários níveis (Ctrl+Z); salva em BMP
  (Ctrl+S). Detalhes: D056.
- **Vídeo MJPEG em AVI** (ícone «Reproductor de vídeo» («Reprodutor de vídeo»), v6.2, D129), com o decodificador JPEG e
  `tools/convertir-video.sh` para preparar os vídeos.
- **Visualizador de PDF** (`visorpdf`, D138): motor próprio em `userland/pdf/`, com tipografias, filtros e
  cifras RC4 e AES-256.

## Extras: fontes vetoriais (v7.2, D196)

- **libfontes** (`userland/libfontes/`, `userland/inc/fontes.h`): TrueType, OpenType com contornos CFF,
  WOFF e WOFF2, com antisserrilhado por cobertura exata. É o motor do navegador Cipó, agora compartilhado:
  `fonte_abrir_archivo`, `fonte_medir`, `fonte_texto` (sobre uma janela ou um buffer, com cor e tamanho em
  pixels), `fonte_nombre` e o catálogo de `/apps/fuentes` e `/home/fuentes`.
- **Fontes da Typodermic** (Raymond Larabie, **CC0**): 28 em `/apps/fuentes` (partição `SAGUI-APPS`) e
  `tools/fuentes_typodermic.py` para extraí-las do zip (`NEW_FONTS/`, fora do git) ou empacotar as 731 em
  uma pasta ou um `.tar.gz` para copiar para um disco instalado.
- **Configuração > Fontes:** lista, pré-visualização e fonte padrão para documentos e títulos. O **Paint**
  a usa na sua ferramenta de texto, e o **Editor** tem um botão «Vista» («Vista») que mostra o documento com ela.
  A área de trabalho segue com a fonte de mapa de bits 8x16 (próximo passo).

## Extras: atividades educativas JClic

- **Início > Educación > Actividades JClic** («Início > Educação > Atividades JClic») (D185, v7.1) abre os projetos do repositório do Clic do XTEC
  (https://projectes.xtec.cat/clic/es/repo/): `.jclic.zip` ou `.jclic`, também com duplo clique no Archivos.
  Jogam-se quebra-cabeças (duplo, de troca e de buraco), associações simples e complexas, memória, explorar,
  identificar, caça-palavras e palavras cruzadas, com contadores de acertos, tentativas e tempo. As atividades
  de texto e o antigo Clic 3.0 (`.pac`) ainda não são lidos: são pulados com um aviso.
- **Início > Educación > Actividades JClic (bajar)** («Início > Educação > Atividades JClic (baixar)») lista o repositório (cerca de 2.600 projetos, filtro de
  idioma e busca), baixa os que se quiserem para `/home/jclic/` e os abre. Sem rede usa-se o índice salvo.
  **Nenhum projeto do repositório vem com o SaguiOS** (cada um tem sua licença, normalmente Creative
  Commons); só um exemplo próprio em `/jogos/jclic`. Teste: `tests/h152_jclic.py`.

## Extras: Paciência Spider

- **Paciência Spider** (`spider`, D173; Início > Jogos): o Spider do Windows Me com 104 cartas, 10 colunas,
  níveis de 1, 2 e 4 naipes, arrastar e soltar, clique para mover para a melhor coluna, dica (H), desfazer
  (Ctrl+Z), partida nova (F2) e pontuação do Windows; cartas, naipes e figuras desenhados em código, com
  animação ao distribuir, ao retirar uma escada e ao ganhar. `spider --autoprueba` testa as regras sem janela.

<div align="center">
<img src="docs/capturas/H143-2-tapete.png" width="32%">
<img src="docs/capturas/H143-4-arrastre.png" width="32%">
<img src="docs/capturas/H143-9b-pantalla-completa.png" width="32%">
</div>

## Extras: CHIP-8

- **Emulador `chip8`** (ícone «CHIP-8», D070): CHIP-8, **SUPER-CHIP 1.1** e **XO-CHIP**
  (128x64, rolagem, sprites de 16x16, 64 KB, quatro cores e o padrão de som real
  pela placa de som; sem placa, aproximado pelo alto-falante), escrito a partir da documentação pública. Os *quirks* de cada máquina são escolhidos pela
  extensão (`.ch8`, `.sc8`, `.xo8`) ou um a um (`-q`); janela redimensionável com
  tela cheia, paletas (uma com as cores do SaguiOS), pausa, passo a passo,
  depurador (F10) e `chip8 -t` para executar sem janela.
- **Montador `chip8asm`** (D071): rótulos, constantes, `db`/`dw`, `include` e erros com
  arquivo e linha; `-d` desmonta. O mesmo código monta no build as ROMs incluídas.
- **Quatro jogos próprios** em `/jogos/chip8`, com sua fonte em assembly (D072): RAMAS (o sagui coleta bananas
  e desvia de paus), SUMAS (um quebra-cabeça de 4x4 com o teclado hexadecimal), CANOA (descer o rio em SUPER-CHIP) e AURORA (demonstração XO-CHIP com 4 cores e melodia).
  Manual: `/ayuda/chip8.md` (`markdown /ayuda/chip8.md`).

<div align="center">
<img src="docs/capturas/H63-chip8-ramas-jogo.png" width="32%">
<img src="docs/capturas/H63-chip8-depurador.png" width="32%">
<img src="docs/capturas/H63-chip8-aurora.png" width="32%">
</div>

## Extras: assistente de IA

- **Cliente `ia`** (D134, v6.2.1): comando do terminal com uma API compatível com a da OpenAI e
  resposta em *streaming*. Provedores: **OpenRouter** ou qualquer endpoint compatível com a OpenAI
  (llama.cpp, Ollama, LM Studio…).
- **Configuração compartilhada** (D145, v6.6): em Configuração, aba «IA», escolhem-se o provedor, o endereço, a chave e os modelos (geral, terminal, chat e Mico). Cada programa usa seu modelo; a
  chave é salva à parte e não é exibida.
- **Aplicativo de chat** com janela, contexto e em espanhol (D143), e o **painel Asistente** («Assistente») do Mico
  (D140). Segundo o ROADMAP, seguem pendentes a lista lateral de conversas, o Markdown nas
  respostas e o seletor de provedor e modelo dentro da janela do chat.

## Extras: som

- **Placas de som AC'97 e Intel HDA** (D088-D090, melhoria 041): AC'97 da Intel (ICH),
  SiS, NVIDIA e AMD, e qualquer controlador HDA, com o percurso dos *widgets* do codec
  até o alto-falante, os fones e a saída de linha. PCM de 16 bits estéreo a 48 kHz, com
  interrupções e, se elas não chegarem, por sondagem. O alto-falante do PC continua como reserva.
- **Mixer no núcleo**: vários programas tocam ao mesmo tempo, cada um na sua frequência
  (4-96 kHz, 8 ou 16 bits, mono ou estéreo), com volume por fluxo e geral.
  - `reproducir archivo.wav|mp3|flac` («reproduzir») e `volumen [0-100|silencio|activar]`; `infopc` mostra a
    placa e o codec.
  - **Formatos de áudio com decodificadores próprios** (D098-D100, melhoria 053): MP3 (MPEG-1 e 2
    Layer III, VBR, tags ID3), FLAC sem perdas e WAV IMA ADPCM, reconhecidos pelo conteúdo e tocados em *streaming*; `reproducir` mostra formato, frequência, canais,
    duração e tags; o duplo clique no Archivos abre `.wav`, `.mp3` e `.flac`. Um MP3 estéreo
    custa ~18 Mciclos por segundo de áudio (tempo real com folga em um Pentium III). Ogg
    Vorbis, pendente. Preparar áudio: `tools/convertir-audio.sh`.
  - **Reprodutor de música gráfico** (`musica`, menu Início > Acessórios > «Música», D115): lista
    de reprodução (arquivos ou uma pasta inteira), anterior/seguinte, reproduzir/pausa/parar,
    barra de progresso com tempo e salto com um clique, volume, formato e tags
    (ID3, comentários FLAC); espaço, setas ±5 s, N, P. Não bloqueia a interface. O duplo clique em
    `.wav`, `.mp3`, `.flac` ou `.ogg` no Archivos o abre. Teste `h86_musica.py`.
  - **Reprodutor de MIDI** (`midi`, Início > Acessórios > «MIDI», D190): toca `.mid`, `.midi` e `.kar` com um
    sintetizador próprio da libsagui (`audio_midi.c`, sem arquivos de dados); visão dos 16 canais com o instrumento de GM, teclado de 88 notas e medidor, letra karaokê e lista da pasta. `musica` e
    `reproducir` também tocam MIDI. Exemplos em `/ejemplos/musica`. Teste `h157_midi.py`.
  - O CHIP-8 toca o padrão de áudio XO-CHIP de verdade; o Banana e o Sagui 3D, seus efeitos; em
    Python, `sagui.Sonido(22050).escribir(datos)`.
  - No QEMU: `./run_in_qemu.sh --sonido` (HDA pelo PulseAudio; `--ac97` para a outra).

## Extras: data, hora e fuso horário

- **O núcleo guarda a hora em UTC** e o relógio da placa-mãe pode estar em UTC ou na hora
  local, como nos PCs com Windows (por padrão, local em um PC e UTC em uma máquina
  virtual). Os fusos usam regras **POSIX TZ** com um interpretador escrito aqui e uma tabela de
  84 zonas («España (Madrid, península)», «Brasil (São Paulo, Brasilia, Río)»...): a barra
  muda sozinha para o horário de verão. Detalhes: D080 e D081, melhoria 039.
  - `zona` (a atual, `zona lista`, `zona Europe/Madrid`, `zona --rtc local`), a janela
    **«Fecha y hora»** («Data e hora») (Início → Sistema, ou botão direito no relógio), `date` com a
    abreviação (CET/CEST) e `-u`, a variável `TZ` por programa, `ls -l` com datas e
    `ntp` (SNTP) para acertar a hora. Com `/home` persistente o fuso é salvo em
    `/home/.zona`.

## Rede e HTTPS

- **Rede cabeada** (D046, D047): drivers Intel e1000 e Realtek RTL8139, pilha IPv4 própria
  (ARP, ICMP, UDP, DHCP, DNS, TCP), loopback `127.0.0.1` e sockets como descritores.
  Comandos `ifconfig`, `dhcp`, `ping`, `nslookup`, `nc` e o navegador de texto **Cipó**
  (`cipo`).
- **Aba «Red» («Rede») do Gerenciador de tarefas** (D079, melhoria 038): interfaces com estado,
  endereços, contadores e velocidade, gráfico do último minuto, conexões com seu processo,
  e «Renovar DHCP», «Desactivar interfaz» («Desativar interface») e «Cerrar conexión» («Fechar conexão»).
- **HTTPS com criptografia escrita aqui** (D060-D063, melhoria 027): TLS 1.3 e TLS 1.2 (ECDHE com
  AES-GCM ou ChaCha20-Poly1305), X25519, P-256/P-384, certificados RSA e ECDSA, validação X.509
  completa (cadeia, datas, nome, usos) com mensagens claras («certificado caducado» («certificado vencido»), «nombre no coincide» («nome não confere»), «emisor desconocido» («emissor desconhecido»)). Os números aleatórios saem de um gerador do núcleo
  (RDRAND se houver; se não, o relógio das interrupções), também como `/dev/urandom`.
  - `curl https://...` (`-v` mostra a conexão e o certificado, `-k` não o verifica,
    `--cacert ca.pem`), `wget` com barra de progresso, `sha256sum`, e o Cipó abre páginas HTTPS.
  - `certificados` gerencia as raízes de confiança de `/etc/ssl/certs` (32 de fábrica:
    Let's Encrypt, DigiCert, GlobalSign, Google, Amazon...): `lista`, `ver`, `añadir` («adicionar»),
    `quitar` («remover») e `probar host` (conecta e explica a validação). Com `/home` persistente as
    mudanças sobrevivem ao reinício.
  - Sem auditoria e sem revogação (OCSP/CRL): é um sistema educativo; os limites estão em
    D063.

## Extras: o navegador Cipó

- **Motor web próprio** (D141, v6.3): CSS, JavaScript, Canvas, SVG e WebGL por software, com o desenho
  por faixas; downloads um a um e botão direito para salvar.
- **Abas, favoritos e busca** (D148, D152): cada aba com seu cache e sua posição de leitura,
  favoritos, copiar, busca sem acentos e imagens JPEG reduzidas. Ctrl+clique abre o link em outra
  aba, e a posição de leitura de cada site é lembrada.
- **JavaScript** (D149, D160): o motor QuickJS, portado para o SO. Cada `<script>` vê um DOM que pode
  consultar (`getElementById`, `getElementsByTagName`, `querySelector`), mudar atributos e
  `textContent`, e colocar tratadores de clique.
- **Redirecionamentos** (D159): `window`, `location` e `meta refresh`, com a navegação adiada até que o
  script termine.
- **Transições CSS** (D153) e **pseudo-elementos** sem quebrar o DOM (D161): `https://www.amd.com/`
  carrega inteiro.

## Extras: compressão (melhoria 056)

- **`tar`, `gzip`/`gunzip`/`zcat`, `zip` e `unzip`** com DEFLATE próprio (LZ77 + Huffman
  dinâmico/fixo, níveis 0-9), CRC-32, gzip de vários membros, tar ustar/GNU/pax e zip stored e
  deflate (sem cifra nem ZIP64). A extração nunca sai da pasta de destino. Testado nos dois sentidos com `tar`, `gzip`, `zip`, `unzip`, `tarfile` e `zipfile` do hospedeiro
  (`tests/h76_compresor.py`). Detalhes e limites: D103.

## Discos: partições e instalador (v7.0b, D179)

- **Partições:** `particiones` («partições») no terminal e o botão **Particiones** da janela Discos
  (`discos --particiones [disco]`): tabela MBR com primárias, **estendida e lógicas**, alinhadas a
  1 MB. Criar, apagar, mudar o tamanho (um FAT32 cresce com a partição se a sua FAT permitir;
  encolher um sistema de arquivos não é feito), formatar em **FAT32 ou ext2**, marcar a ativa,
  **TRIM** (em discos SATA/AHCI que o suportam) e **desfazer** a última mudança. A GPT é mostrada
  mas não é tocada.
- **Instalador:** Início > Sistema > **Instalar SaguiOS** (ou `instalar <disco>`). Copia o sistema
  que está rodando para um disco rígido ou para outro pendrive (o disco inteiro, um espaço livre ou no lugar de
  uma partição) e o deixa inicializável por BIOS, com `/home`, os jogos (em `/jogos/opentyrian`) e os
  aplicativos (em `/apps`, com o que houver no `/apps` do sistema) em partições ext2 próprias que se
  montam por rótulo, como na imagem. Opcionalmente copia os dados dos jogos de `/jogos`.
- **Segurança:** o kernel só deixa escrever em um disco **reservado**, e só o reserva se nada estiver montado dele; o disco de boot pede **duas confirmações**; cada diálogo diz o que é apagado; a tabela é validada antes de ser escrita (EBR primeiro, MBR por último) e relida; o instalador
  deixa o disco **não inicializável** até ter verificado tudo. Detalhes em
  [`docs/decisoes.md`](docs/decisoes.md#d179-gestión-de-particiones-trim-y-un-instalador-de-saguios-a-disco-duro-o-pendrive-v70b).

## Partições do sistema: jogos e aplicativos (v7.0b e v7.1, D181 e D194)

O ramdisk, que é copiado inteiro para a RAM ao iniciar, leva só **a base** do sistema (o shell, a área de trabalho, Archivos, Discos, o instalador, Configuração e as bibliotecas). O que não é essencial fica em
partições da própria imagem, montadas pelo rótulo, que não ocupam RAM:

| Partição | Formato | Monta em | O que leva | Variáveis |
| --- | --- | --- | --- | --- |
| `SAGUI-JOGOS` | ext2, leitura e escrita | `/jogos` | um jogo por pasta: `/jogos/opentyrian` (D181), `/jogos/openjazz`, `/jogos/quake` (D229) e o que for colocado, como a versão completa de um jogo (D228); também os jogos da base do ramdisk (chip8, jclic, sagui3d), que a montagem cobre | `JOGOS_MB` (800 nas persistentes, a de partições e o instalador), `JOGOS_IMG_MB` (0 = o justo em `saguios.img`), `JOGOS=0` |
| `SAGUI-APPS` | ext2, leitura e escrita | `/apps` | os aplicativos não fundamentais: `/apps/bin` (no `PATH`: NetSurf, `sqlite3`) e os dados de cada um em `/apps/<app>` (D194) | `APPS_MB` (0 = o justo mais margem), `APPS=0` |
| `SAGUI-CONF` | ext2, 16 MB, só root | montada pelo kernel (melhoria 098) | a configuração do sistema (melhoria 098, D256): usuários, senhas, rede; por enquanto só um `LEEME`. Fica por último, em todas as imagens de disco ou pendrive e nas instalações; **a ISO não a leva** (modo ao vivo). O instalador preserva a que já existir (`--formatear-conf` a apaga); `mkimagem.py --sin-conf` a remove | (fixa) |

- Se faltar uma partição, o sistema **inicia igual**: sem `SAGUI-APPS`, o NetSurf não aparece no menu
  Início e o `netsurf` não é encontrado.
- `make APPS=0` deixa os aplicativos **dentro do ramdisk** (em `/apps`, com os mesmos caminhos): uma
  imagem de um só bloco. O **CD** não lê partições, então a ISO sempre os leva no seu ramdisk.
- Para instalar outro aplicativo basta copiar seu executável para `/apps/bin` e seus dados para `/apps/<app>`;
  o instalador copia tudo o que está em `/apps` para o disco instalado. Teste: `tests/h159_sagui_apps.py`.

## Multinúcleo e threads

O SaguiOS inicia com um processador, mas já pode usar vários. Cada peça tem sua opção ao criar a imagem.

- **SMP opcional** (D164, v6.7; trava geral do kernel em D165, v6.8): com `--smp` a imagem liga os
  processadores secundários. Uma trava geral do kernel (`kernel/nucleo/trava_geral.c`) deixa que cada CPU
  atenda suas chamadas de sistema, suas falhas e suas exceções, então vários programas rodam ao mesmo tempo.
  As tarefas do kernel seguem na CPU 0.
- **IOAPIC opcional** (D166, v6.8): com `--ioapic` as IRQ dos dispositivos chegam pelo IOAPIC e não pelo
  8259, com volta atrás ao PIC se o tique não chegar.
- **Interrupções distribuídas** (D168, v6.8): com `--ioapic --smp` as IRQ 1-15 se distribuem por turnos entre
  as CPU secundárias; o PIT (IRQ 0) fica na CPU 0. Nos testes, o teclado vai para a CPU 1 e a rede
  ou o som (IRQ 11), para outra CPU.
- **Threads de usuário** (D167, v6.8): uma thread compartilha o espaço, os arquivos, a pasta e as janelas do
  seu processo. A libsagui traz `thread_create`, `thread_join`, `thread_yield`, `thread_sair`, `mutex_t` e
  `semaforo_t` (que dormem no kernel, por futex), e o `malloc` é seguro entre threads. A mesma API está na
  biblioteca do `cc`; exemplo: `/ejemplos/hilos.c`. Python e Pascal ainda não têm threads
  ([`docs/multihilo.md`](docs/multihilo.md)).

**Por padrão a imagem inicia com uma só CPU.** Para criar a imagem com vários processadores, depois do `make`:

```bash
python3 tools/mkimagem.py --mbr build/boot/mbr.bin --estagio2 build/boot/estagio2.bin \
    --kernel build/kernel.bin --kernel-elf build/kernel.elf --ramdisk build/ramdisk.sgfs \
    --saida build/saguios.img --vesa --smp --ioapic
```

As duas opções são independentes: `--smp` liga os secundários, `--ioapic` muda o controlador de
interrupções, e com as duas as IRQ se distribuem. O comando substitui `build/saguios.img`, que é a que
inicia `./run_in_qemu.sh --nucleos 4` (4 CPU na máquina); o próximo `make` que mudar algo a recria
sem estas opções. Sem `--smp` na imagem, as CPU extras não são usadas.

**Medições** (KVM, 4 CPU, [`docs/multihilo.md`](docs/multihilo.md)): o cálculo com `bench 4 c` ganha entre 2,7x
e 3,2x, e quatro threads (`bench 4 h`) entre 3,8x e 4,1x.

**Limites:** há uma só trava geral, então uma IRQ atendida em uma CPU secundária espera que a CPU
que está no kernel a solte; a distribuição de IRQ é fixa por turnos; e o IOAPIC e as linhas PCI sem `_PRT`
não foram testados em hardware real (D166, D168).

## Números

| | |
| --- | --- |
| Imagem de boot | **28.959.232 bytes** úteis: 12.182.016 de boot (11.740.144 de ramdisk) e 16 MB das partições SAGUI-JOGOS e SAGUI-APPS (meta de 38 MB desde a v7.0b, D181 e D194; a original, 575.488) |
| Linhas de código | boot 903, kernel 9.614, programas 6.317 (mais 3.591 de ferramentas e testes) |
| Programas | dezenas em `/bin`: shell, comandos tipo Unix (`ls`, `grep`, `find`, `sort`, `head`, `tail`... ver `docs/comandos.md`, D097) e programas gráficos |
| Testes automáticos | mais de 130 (`tests/h*.py`, um por história), no QEMU |
| Decisões registradas | 163 entradas (D001 a D168), com o raciocínio de cada uma |
| Erros registrados | 25, com sintoma, causa e correção |
| Commits | 711 em `main` (`git log`) |

## Testar

### Quais imagens há no repositório e quais são geradas

Desde a v7.4 (decisão do mantenedor, 2026-11-13), **no GitHub só são enviadas as duas imagens
básicas** de cada versão, em `entrega/vX.Y[.Z]/`: `saguios.img` (um núcleo) e `saguios-smp.img`
(multinúcleo), com `SHA256SUMS.txt` e `VERSION.txt`. Assim o repositório não cresce com centenas de MB
por versão (o GitHub rejeita arquivos de mais de 100 MB). Nessas duas a partição de jogos tem
o tamanho justo do que leva.

**Todas as outras são geradas no seu computador** depois de baixar o repositório:

```
make variantes DESTINO=entrega/mia      # todas: persistentes ext2 e FAT32, com e sem multinúcleo, com uACPI,
                                        # a de partições, a sem GPL, a ISO e o .vdi
make imagem-persistente-smp             # ou só uma (veja «make help»)
make imagem-persistente-smp-uacpi       # a de notebooks: persistente, multinúcleo e com
                                        # o indicador de bateria (uACPI, D192)
```

As persistentes, a de partições e o instalador criam a **partição de jogos de 800 MB, de
leitura e escrita** (`/jogos`, uma pasta por jogo), onde você pode copiar depois a versão
completa de um jogo a partir do próprio SaguiOS. Os tamanhos são mudados com variáveis
(`JOGOS_MB`, `APPS_MB`, `DATOS_MB`…).

**Conteúdo próprio:** o que não pode ir no repositório nem ser baixado por um script na hora de compilar
(por exemplo, a versão completa de um jogo que você tenha) é colocado na pasta `input_share/`
(ignorada pelo git: nunca é enviada) e entra nas imagens que você gerar. Por exemplo,
`input_share/games/Jazz/` acaba em `/jogos/Jazz` (com `make` normal; em `make variantes` é preciso
pedir com `make variantes GERAR_OPC=--con-input-share`, para que uma entrega publicada nunca a leve por descuido). Detalhes em
[`docs/input_share.md`](docs/input_share.md).

### Como usá-las

As imagens prontas estão em [`entrega/final/`](entrega/final/) e em `entrega/vX.Y[.Z]/` (as básicas):

- **VirtualBox:** `saguios.vdi` (máquina "Other/Unknown", 32 bits, EFI desativado).
- **PC de verdade:** grave `saguios.img` em um pendrive em modo DD (Rufus, balenaEtcher ou `dd`)
  e inicie com a BIOS tradicional; em PCs que não iniciam pela USB, grave `saguios.iso`
  (`make iso`) em um CD.
- **QEMU com janela:** `./run_in_qemu.sh` (compila se for preciso e inicia `build/saguios.img`;
  `--usb` inicia como pendrive, `--cd` a partir da ISO, `--nucleos N` com N processadores (veja «Multinúcleo e threads»), `--mem MB`
  (2048 por padrão), `--cpu pentium3` sem SSE2, `--sonido` («--sonido») ativa o alto-falante e uma placa Intel HDA e `--help` mostra o resto).
- **Gerar uma versão:** `./gerar_versao.sh` (o irmão do `run_in_qemu.sh`): compila, monta as imagens de boot e deixa a entrega em `entrega/latest/` com suas somas SHA-256 e uma folha `VERSION.txt`; `--help` mostra as opções, que incluem a variante de licença, se leva MicroPython, o nível de otimização e `--probar` (iniciar a imagem antes de publicá-la).

O passo a passo está em [`docs/como-gravar.md`](docs/como-gravar.md).

## Compilar e testar

Requisitos (Ubuntu): `nasm gcc gcc-multilib binutils make python3 qemu-system-x86 qemu-utils imagemagick mtools dosfstools`.

**Guia completo para compilar suas próprias imagens** (requisitos, todas as opções, downloads, jogos próprios e problemas frequentes), em espanhol, inglês e português: [`compilar_versiones.md`](compilar_versiones.md).

```bash
make            # build/saguios.img
make vdi        # build/saguios.vdi (VirtualBox)
make iso        # build/saguios.iso (CD inicializável, D077)
make test       # todos os testes no QEMU (sem janela)
make rodar      # abre no QEMU com janela
make versao     # gera a versão e deixa a entrega (./gerar_versao.sh)
```

O que quem compila quiser colocar na sua imagem e não pode ir no repositório (por exemplo a versão
completa de um jogo) vai em `input_share/` (ignorada pelo git): `input_share/games/<pasta>` acaba em
`/jogos/<pasta>` e `input_share/apps/` em `/apps`. Veja [docs/input_share.md](docs/input_share.md).
Os programas de C++ (OpenJazz) pedem o `cc1plus` do g++-13, que o `tools/entorno-local.sh` instala.

Os testes automáticos estão em `tests/`, um por história. Um só: `python3 tests/h101_sagui_gl_v6.py`.
A bateria completa: `python3 tests/rodar_todos.py` (em série) ou `python3 tests/rodar_paralelo.py --help`
(várias de uma vez, com `-j N` e `--kvm` se houver KVM). Em paralelo, **`-j 2` ou `-j 3` no máximo**: cada QEMU
ocupa CPU e memória, e com mais o computador pode travar ou reiniciar.

## Estrutura

```
boot/        estágio 1 (MBR) e estágio 2 do carregador, em NASM
kernel/      kernel em C freestanding + NASM: núcleo, memória, drivers, USB, arquivos, janelas
userland/    libsagui e os programas; userland/sagui3d/ é o jogo do bônus B4;
             userland/micropython/ é o MicroPython (terceiros, MIT, opcional)
             userland/web/ (motor do Cipó e QuickJS), userland/pdf/ (visualizador de PDF), userland/ia/,
             userland/mail/ (cliente de e-mail) e userland/terceros/ (OpenTyrian, D156)
rootfs/      arquivos que vão para o disco na RAM (textos, fases do Sagui 3D)
tools/       build (mksgfs.py, mkimagem.py, mkiso.py), painel (status.py), verificador de fases
tests/       testes automáticos no QEMU, um por história
docs/        histórias, decisões, diário, erros, roteiros de teste e capturas de tela
dashboard/   painel do projeto (abra dashboard/index.html no navegador)
entrega/     imagens .img e .vdi de cada fase, com SHA-256
testes_eli/  fotos dos testes do Eli no G470
```

## Documentação

> **Roadmap de versões**: [`docs/ROADMAP.md`](docs/ROADMAP.md) · versão visual: [`docs/roadmap.html`](docs/roadmap.html).
>
> **Para continuar o trabalho em outro PC**: [`docs/CONTINUAR.md`](docs/CONTINUAR.md) (estado, branches pela metade, testes pendentes e fila de melhorias).

| Documento | O que contém |
| --- | --- |
| [`docs/resumo-final.md`](docs/resumo-final.md) | Resumo do projeto, tempo de cada fase e a tabela de conceitos do livro do Tanenbaum completada |
| [`docs/decisoes.md`](docs/decisoes.md) | As 163 entradas de decisões de projeto (D001 a D168) e a seção do Sagui 3D |
| [`docs/erros.md`](docs/erros.md) | Os 25 erros da tabela, incluindo os encontrados no G470 |
| [`docs/compatibilidad.md`](docs/compatibilidad.md) | Máquinas testadas (✓, ⚠, ✗) e como enviar um relatório (`infopc --informe`) |
| [`docs/diario.md`](docs/diario.md) | O diário do projeto, fase a fase |
| [`docs/multihilo.md`](docs/multihilo.md) | Multithread e multinúcleo: o que há, o que falta e como se mede o ganho |
| [`docs/licencia.md`](docs/licencia.md) | A licença GPL v3 explicada: camadas, SPDX, compatibilidades, o que se pode colocar e o que não |
| [`docs/terceros.md`](docs/terceros.md) | O código de terceiros: o que é, de onde vem, com qual licença e em qual decisão foi aceito |
| [`docs/historias/`](docs/historias/) | Uma página por história |
| [`status.md`](status.md) e [`dashboard/`](dashboard/index.html) | Estado de cada fase e história |

## Licença

O SaguiOS-Extended é software livre sob a **GNU General Public License, versão 3** (texto completo
em [`LICENSE`](LICENSE)). É um fork do **[SaguiOS](https://github.com/erigobeli/sagui-os)**, o
sistema operacional criado por **Eli Rigobeli** (GPL v3), que autorizou expressamente traduzi-lo,
testá-lo e publicar esta versão enquanto seguir sendo GPL v3 e com o código-fonte disponível
(ver [`AUTORES.md`](AUTORES.md)). **Se este projeto te for útil, visite e apoie o original:
https://github.com/erigobeli/sagui-os**

O esquema, por camadas (explicado com calma em [`docs/licencia.md`](docs/licencia.md)):

| Camada | Licença |
| --- | --- |
| O conjunto (o repositório e as imagens) | GPL v3 |
| Arquivos que já existiam no SaguiOS original (commit `dba0fffc`) e os derivados deles | `GPL-3.0-only` |
| Arquivos novos deste fork | `GPL-2.0-or-later` (que pode ser usada sob a GPL v3) |
| Terceiros (MicroPython, uACPI, SQLite, os jogos portados...) | a sua, registrada em [`docs/terceros.md`](docs/terceros.md) |

Cada arquivo próprio traz sua linha `SPDX-License-Identifier`. NetSurf, GNUBoy e o início moderno do driver de rede Realtek r8169 (GPL-2.0 sem «ou posterior») **não vão nas imagens nem no código publicados**: só quem compila pode incluí-los, para uso privado. Detalhes e a decisão: D258 em [`docs/decisoes.md`](docs/decisoes.md).

## As regras do projeto

SaguiOS nasceu de uma ideia simples (ver [`prd.md`](prd.md), a especificação original de Eli
Rigobeli): **um sistema operacional escrito do zero**. Este fork mantém esse espírito com estas regras:

- **O essencial é próprio:** o carregador de boot (sem GRUB), o kernel, a libc, a biblioteca
  gráfica, a área de trabalho e os aplicativos do sistema estão escritos no projeto.
- **O código de terceiros é a exceção, e está sempre isolado e registrado** em
  [`docs/terceros.md`](docs/terceros.md), com sua origem e sua licença: intérpretes e bibliotecas
  (MicroPython, QuickJS, SQLite, uACPI), os jogos portados (Quake, OpenTyrian, OpenJazz, Command &
  Conquer…), tabelas de hardware tiradas de drivers livres (FreeBSD, OpenBSD, Mesa e algumas do Linux com
  licença «GPL v2 ou posterior»), ícones e fontes.
- **Só licenças compatíveis com a GPL v3** no que se publica. O que não é (NetSurf, GNUBoy e o início
  moderno do driver de rede Realtek r8169, que são «só GPL v2») fica fora das imagens e do código
  publicados; cada um pode compilá-lo para uso próprio
  ([`compilar_versiones.md`](compilar_versiones.md)).
- **Um teste automático por funcionalidade**, que inicia o sistema no QEMU e verifica o resultado
  ([`tests/`](tests/)).
- **As decisões ficam escritas** em [`docs/decisoes.md`](docs/decisoes.md) (D001, D002…) e as propostas em
  [`mejoras/`](mejoras/README.md).
- **Testado em hardware real** antes de dar uma versão por boa, não só em máquinas virtuais.
- Os identificadores e nomes de arquivo vêm do original, em português; os comentários, a documentação e a
  interface estão em espanhol.

