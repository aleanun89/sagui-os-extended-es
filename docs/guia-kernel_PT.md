# Guia do kernel do SaguiOS-Extended

[Español](guia-kernel.md) · [Português](guia-kernel_PT.md) · [English](guia-kernel_EN.md)

> Para quem quer aprender como um sistema operacional é feito, lendo código real.
> Este guia explica o **kernel** (o núcleo): o que faz cada parte, como ela se encaixa com as outras
> e em qual arquivo ela está. As aplicações estão no [guia de aplicações](guia-aplicaciones_PT.md).

**Índice**

1. [O que é o SaguiOS](#1-o-que-é-o-saguios)
2. [Como ele inicia](#2-como-ele-inicia)
3. [Mapa do kernel: as pastas](#3-mapa-do-kernel-as-pastas)
4. [Os blocos, um por um](#4-os-blocos-um-por-um)
5. [Como se faz uma chamada ao sistema, passo a passo](#5-como-se-faz-uma-chamada-ao-sistema-passo-a-passo)
6. [Tabelas de arquivos](#6-tabelas-de-arquivos)
7. [Onde ler mais](#7-onde-ler-mais)

---

## 1. O que é o SaguiOS

O **SaguiOS-Extended** é um sistema operacional **educativo**, escrito em espanhol e pensado para
aprender. Seus traços principais:

| Traço | O que significa |
| --- | --- |
| **i686, 32 bits** | Foi pensado para computadores antigos de 32 bits (processadores i686 e compatíveis). Por isso o código usa ponteiros de 32 bits e assembly i386. Os modos de 64 bits não são usados. |
| **Próprio** | O carregador de inicialização, o kernel, a biblioteca C (a *libc*) e a biblioteca gráfica (a *libsagui*) foram escritos neste projeto. Não usa GRUB. Alguns drivers vêm de outros projetos (Linux, FreeBSD, uACPI): estão isolados e marcados em `kernel/terceros/`. |
| **GPL v3** | É software livre. O código que vem de outros projetos está marcado e registrado em [`docs/terceros.md`](terceros.md). |

> **O que é um kernel.** É o programa que está sempre em execução, por baixo de todos os outros.
> Ele reparte a CPU entre os programas, lhes dá memória, cuida dos discos e da rede, e impede que
> se sobrescrevam uns aos outros. Os programas (um editor, um navegador) pedem coisas a ele por meio
> de **chamadas ao sistema**. Ver a seção 5.

O kernel do SaguiOS é escrito em **C** (com um pouco de **assembly** para o que o C não consegue
fazer, como carregar a tabela de interrupções). É compilado sem a biblioteca padrão do PC
(*freestanding*): não há `printf` nem `malloc` vindos de fora, tudo está em `kernel/`.

---

## 2. Como ele inicia

Quando o PC é ligado, nada do SaguiOS está em execução. A inicialização é uma **cadeia**: cada
estágio carrega o seguinte.

```
 Ligar
    │
    ▼
 BIOS (ou UEFI)             o firmware do PC; lê o primeiro setor do disco
    │
    ▼
 Estágio 1: MBR             boot/mbr.asm · 512 bytes; procura o cabeçalho (LBA 1)
    │
    ▼
 Estágio 2                  boot/estagio2.asm · passa ao modo protegido de 32 bits, A20,
    │                       mapa de memória (E820), copia tudo acima de 1 MB
    ▼
 Kernel                     kernel/nucleo/principal.c · liga as partes em ordem
    │
    ▼
 initramfs (ramdisk)        o mínimo indispensável na RAM: o formato SGFS (kernel/fs/sgfs.c)
    │
    ▼
 Raiz SAGUI-RAIZ            o sistema de arquivos real em um disco (decisão D379)
    │
    ▼
 Sessão gráfica             login, área de trabalho e barra de tarefas (kernel/gui/)
```

### 2.1 O que é cada passo

- **BIOS ou UEFI.** O firmware do computador. Copia o primeiro setor do disco para a memória
  (`0x7C00`) e salta para lá. Com UEFI, o SaguiOS usa um carregador próprio (`boot/uefi/cargador.c`,
  decisão D264) que faz, com os serviços do firmware, o mesmo que o estágio 2 faz com a BIOS.
  Ver [`docs/uefi.md`](uefi.md).
- **Estágio 1 (MBR).** Um setor de 512 bytes. É tão pequeno que só sabe ler o **cabeçalho de
  inicialização** (LBA 1 da imagem), que diz onde está cada parte. Ver `boot/cabecalho.inc`.
- **Estágio 2.** É a parte grande do carregador. Como, ao ligar o PC, o processador está no *modo
  real* de 16 bits, o estágio 2 faz tudo o que só a BIOS sabe fazer: ativa a linha A20 (para poder
  usar mais de 1 MB de memória), pede o **mapa de memória** (E820), copia o kernel acima de 1 MB e,
  por fim, passa ao **modo protegido** de 32 bits. Deixa todas as informações em uma estrutura
  (`boot/bootinfo.inc`) que o kernel recebe.
- **Kernel.** `kernel/nucleo/principal.c` é o ponto de entrada em C. Verifica se o carregador fez
  seu trabalho (a assinatura e a estrutura de inicialização), e liga as peças em ordem: interrupções
  e relógio, memória física, teclado, sistema de arquivos e, por fim, o terminal.
- **initramfs (no SaguiOS, *ramdisk*).** Um pequeno sistema de arquivos carregado na RAM junto com
  o kernel, com o indispensável para montar o disco. No SaguiOS está no formato **SGFS** (lido por
  `kernel/fs/sgfs.c`). Ver [`docs/almacenamiento.md`](almacenamiento.md).
- **Raiz SAGUI-RAIZ.** A decisão **D379** separa "o que é preciso para iniciar" de "o sistema". O
  sistema vive em uma partição ext2 chamada `SAGUI-RAIZ`, que é montada como `/`. Assim pode-se
  iniciar a partir de um pendrive no modo *live*, instalado no disco ou com tudo na RAM.
- **Sessão gráfica.** `kernel/gui/sessao.c` mostra o login e, depois, a área de trabalho
  (`kernel/gui/area_trabalho.c`) com sua barra de tarefas (`kernel/gui/barra.c`).

> **Para testar a inicialização:** o `make` gera `build/saguios.img` e `./run_in_qemu.sh` a inicia
> em uma máquina virtual. Ver o README, seção "Compilar y probar" (Compilar e testar).

---

## 3. Mapa do kernel: as pastas

O kernel está na pasta `kernel/`. Cada subpasta é um **bloco** com uma responsabilidade.

| Pasta | Para que serve | Arquivos que mais se leem primeiro |
| --- | --- | --- |
| `kernel/arch/` | O que depende do processador: tabelas GDT e IDT, interrupções, APIC, SMP | `idt.c`, `gdt.c`, `pic.c`, `lapic.c`, `ioapic.c` |
| `kernel/mem/` | Memória: páginas físicas, paginação, PAE, swap comprimida, cópia na escrita | `fisica.c`, `paginacao.c`, `heap.c`, `copia_escrita.c`, `troca.c` |
| `kernel/nucleo/` | Processos, threads, escalonador, sinais, chamadas ao sistema, usuários e permissões | `processos.c`, `sistema.c`, `bifurcar.c`, `carregador.c`, `principal.c` |
| `kernel/fs/` | Sistema de arquivos: VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, cache, raiz | `vfs.c`, `arquivos.c`, `ext2.c`, `cache_disco.c`, `raiz.c` |
| `kernel/dispositivos/` | Drivers: discos, som, vídeo, 2D e 3D pela GPU, ACPI, notebooks, sensores | `ahci.c`, `hda.c`, `video.c`, `gpu3d.c`, `uacpi_sagui.c` |
| `kernel/rede/` | Pilha TCP/IP, drivers de placas de rede, Wi-Fi | `rede.c`, `tcp.c`, `socket.c`, `wifi.c` |
| `kernel/usb/` | Controladores USB (EHCI, OHCI, UHCI, xHCI), dispositivos e classes | `usb.c`, `xhci.c`, `hid.c` |
| `kernel/bluetooth/` | Pilha Bluetooth clássica (HCI, L2CAP, SDP, HID) | `bt_hci.c`, `bt_l2cap.c`, `bt_gestor.c` |
| `kernel/gui/` | Compositor, janelas, área de trabalho, barra de tarefas, menus, estilos | `janelas.c`, `barra.c`, `inicio.c`, `sessao.c` |
| `kernel/lib/` | Funções puras do kernel (sem tocar no hardware): formatos, criptografia, cópias rápidas | `formatar.c`, `sha256.c`, `rapido.c`, `wpa.c` |
| `kernel/inc/` | Os **cabeçalhos** (`.h`) que as partes do kernel compartilham e, em parte, os programas | `abi.h`, `processo.h`, `vfs.h` |
| `kernel/terceros/` | Código **de outros projetos** (uACPI, FreeBSD, Linux, OpenBSD, X.org): o que não é deste projeto | `uacpi/` |

> **Como se relacionam.** As pastas não são ilhas. Um programa pede um arquivo (`nucleo`), o
> sistema de arquivos (`fs`) procura o bloco no disco (`dispositivos`), que o pede ao controlador
> (`dispositivos/ahci.c`), que fala com o hardware usando o mapa de memória e as interrupções
> (`arch`, `mem`). Esse percurso aparece bem na seção 5.

---

## 4. Os blocos, um por um

### 4.1 `kernel/arch/`: o que depende do processador

- **GDT** (`gdt.c`): a tabela de **segmentos**. Define quais código e dados são do kernel
  (anel 0) e quais são dos programas (anel 3). Tem também a **TSS**, a estrutura que guarda a pilha
  do kernel de cada CPU (decisão D157).
- **IDT** (`idt.c`): a tabela de **interrupções**. Diz qual função é executada para cada número
  de interrupção: falhas do processador, o relógio, o teclado, os discos e a porta `0x80` das
  chamadas ao sistema. `tratar_interrupcao()` é o despachante geral.
- **Interrupções de hardware** (`pic.c`, `ioapic.c`, `lapic.c`, `irq_rotas.c`): o 8259
  (controlador antigo), o **APIC local** de cada CPU e o **IOAPIC**, que distribui as linhas
  dos dispositivos (decisão D166). `irq_rotas.c` decide por qual linha chega cada IRQ de PCI
  (com a tabela `_PRT` do ACPI, decisão D211).
- **Tela de erro** (`excecoes.c`): quando um programa falha de forma grave, mostra-se o estado do
  processador em vez de travar o sistema.
- **FPU, MMX e SSE** (`fpu.c`): salva e restaura o estado da vírgula flutuante de cada processo.
- **MSR** (`msr.c`): leitura dos registros especiais do processador que não derruba o kernel se o
  MSR não existir (decisão D092).
- **SMP**: a inicialização dos processadores secundários está em `kernel/nucleo/smp.c`. É opcional
  (decisão D164): só é ligada com a imagem `--smp`. O código do kernel continua rodando na
  CPU 0 (modelo mestre e escravo); as demais CPUs executam processos e atendem chamadas ao sistema
  (decisões D157 e D165).

### 4.2 `kernel/mem/`: a memória

- **Memória física** (`fisica.c`): quantas páginas de 4 KB estão livres. Mantém um **mapa de bits**
  (um bit por página: 1 = ocupada).
- **Alocador do kernel** (`heap.c`): `kmalloc` e `kfree`. Os blocos pequenos saem de classes
  de tamanho; os grandes, de páginas inteiras.
- **Paginação** (`paginacao.c`): cada processo tem seu **espaço de endereços**, com seus
  diretórios de páginas. Há dois modos: o clássico de 2 níveis e o **PAE** de 3 níveis, que
  permite mais de 4 GB de memória física e o bit **NX** (a página com dados não pode ser executada;
  decisões D010, D049 e D349).
- **Memória alta** (`memalta.c`): janelas temporárias para escrever em páginas físicas que não
  estão mapeadas (decisão D348).
- **Cópia na escrita** (`copia_escrita.c`): quando o `fork` duplica um processo, as páginas são
  compartilhadas e só são copiadas quando alguém escreve nelas (decisão D401).
- **Swap comprimida na RAM** (`troca.c`, `compressor.c`): quando falta memória, em vez de gravar
  no disco, a página é comprimida na própria RAM (decisão D352).
- **Write-combining** (`wc.c`): o framebuffer de vídeo é escrito em blocos, o que é mais rápido.
- **Páginas compartilhadas das bibliotecas** (`imagens.c`): o código de uma biblioteca
  compartilhada vive em uma única cópia (decisão D378, [`docs/enlace-dinamico.md`](enlace-dinamico.md)).

### 4.3 `kernel/nucleo/`: processos, threads e chamadas ao sistema

- **Processos** (`processos.c`): a tabela de processos e o **escalonador** *round-robin*
  preemptivo: cada processo recebe uma fatia de tempo e o relógio lhe tira a CPU ao terminá-la.
- **Escalonadores entre CPUs** (`planificadores.c`, `afinidad.c`): em qual processador cada
  processo é executado (decisão D174).
- **Threads** (`hilos.c`): threads dentro de um processo, com espera por endereço (*futex*, D167).
- **Travas** (`trava.c`, `trava_geral.c`, `spinlock.c`, `atomicas.c`): a exclusão mútua.
  Com SMP, o kernel tem uma trava geral (D165) e primitivas atômicas (D150).
- **Carga de programas** (`carregador.c`): lê um executável **ELF32** e cria o processo com seu
  código, seus dados e sua pilha.
- **`fork` e `exec`** (`bifurcar.c`): o `fork` cria uma cópia do processo (com cópia na escrita) e o
  `exec` o substitui por outro programa. É o modelo do Unix; o shell os usa para cada comando.
- **Sinais** (`sinais.c`): avisos entre processos (decisão D401).
- **Chamadas ao sistema** (`sistema.c`): a porta de entrada `int 0x80`. Ver a seção 5.
- **Usuários e permissões** (`usuarios.c`, `permisos.c`): quem é cada processo (seu uid e gid) e
  quais chamadas cada um pode fazer (decisões D255 e D282).
- **Hora** (`hora.c`) e **energia** (`energia.c`): o relógio do sistema, o fuso horário (D080),
  reiniciar e desligar.
- **Pilhas que crescem** (`pilha.c`): a pilha de um programa cresce quando é preciso, com uma
  guarda que detecta o estouro (decisão D186).
- **Gerenciador de serviços** (`supervisor.c`): os serviços em segundo plano, vistos a partir do
  kernel (decisão D216).
- **Inicialização e configuração**: `principal.c` (entrada do kernel), `kconf.c` (opções que são
  guardadas para a próxima inicialização, D353), `info_hardware.c` (dados para o `infopc`, D041).

### 4.4 `kernel/fs/`: os sistemas de arquivos

A ideia-chave é o **VFS** (*Virtual File System*, `vfs.c`): os programas usam sempre as mesmas
chamadas (`abrir`, `ler`, `escrever`), e o VFS as envia ao sistema de arquivos correspondente.

```
 programa → chamada ao sistema → arquivos.c → vfs.c → ext2 / FAT32 / exFAT / ISO 9660 / UDF / SGFS
                                                   → pipe.c (tubulações)  → tv.c (terminal)
                                                                       ↓
                                                   cache_disco.c (cache de blocos) → blocos.c → discos
```

- **Descritores de arquivo** (`arquivos.c`): a tabela de arquivos abertos de cada processo.
- **Montagens** (`montagem.c`): a tabela de montagens e o `/etc/fstab` (decisão D040).
- **Discos e partições** (`discos.c`, `blocos.c`): acesso aos setores, tabela de partições
  e avisos de disco (decisões D040 e D179).
- **Cache de blocos** (`cache_disco.c`): leitura antecipada e gravação adiada (decisão D281).
- **ext2, ext3 e ext4** (`ext2.c`, `jbd2.c`): leitura e gravação, com o diário JBD2 (D326).
  É o sistema da raiz `SAGUI-RAIZ`.
- **FAT32** (`fat32.c`) e **exFAT** (`exfat.c`): pendrives e cartões de memória.
- **CD e DVD**: `iso9660.c` (ISO 9660), `udf.c` (UDF, D233) e `cdda.c` (um CD de áudio visto como
  uma pasta de arquivos WAV).
- **Sobreposição** (`sobreposicao.c`): a raiz da versão *live*. O sistema do meio é somente
  leitura e uma camada gravável fica por cima, como o *overlayfs* do Linux (decisão D379).
- **Raiz em disco e na RAM** (`raiz.c`, `ramfs.c`, `disco_ram.c`, `disco_comprimido.c`): formas
  diferentes de montar `/` (decisão D379).
- **Tubulações** (`pipe.c`): comunicação entre processos por um buffer circular dentro do kernel.
- **Terminal virtual** (`tv.c`): a pseudoterminal que usam o Terminal de janela e o `ssh`.
- **Configuração e registro** (`conf_sistema.c`, `logs_disco.c`): a partição `SAGUI-CONF` e o
  registro do kernel em `SAGUI-LOGS`.

### 4.5 `kernel/dispositivos/`: os drivers

É o bloco maior (83 arquivos). Agrupa-se assim:

- **Discos**: `ata.c` (IDE por PIO), `ahci.c` (SATA), `nvme.c` (NVMe), `atapi.c` (leitores de CD/DVD).
- **Barramento**: `pci.c` (o barramento PCI), `bar.c` (as janelas de memória de cada dispositivo),
  `pci_realoc.c` (realocar uma BAR de 64 bits abaixo de 4 GB), `isa.c` e `isapnp.c` (o barramento
  ISA e as placas ISA *Plug and Play*).
- **ACPI**: `uacpi_sagui.c` é a camada do kernel para a biblioteca **uACPI**, que interpreta a
  linguagem AML do firmware. Sobre ela estão `acpi.c` (desligamento), `bateria.c`, `ec.c`, `prt.c`
  e `wmi.c`.
- **Notebooks**: `portatil.c`, `toshiba.c`, `fujitsu.c`, `hp_wmi.c`, `video_acpi.c` (brilho e teclas).
- **Sensores e energia**: `sensores.c` (temperatura e frequência), `frequencia.c` (limite de
  temperatura), `eist_antiguo.c` (SpeedStep), `zonas_termicas.c`.
- **Teclado, mouse e controles**: `teclado.c`, `i8042.c`, `mouse.c`, `painel.c` (touchpads PS/2),
  `mandos.c` e `portajogos.c`.
- **Vídeo**: `video.c` (VESA com buffer duplo), `vga_texto.c` (modo texto), `bga.c` (Bochs/QEMU),
  `modos_video.c` e `telas.c` (várias telas).
- **Som**: `audio.c` (mixador), `hda.c` e `ac97.c` (placas), `sb16.c`, `captura.c` (microfone),
  `som.c` (o alto-falante do PC).
- **GPU**: as camadas comuns `acel2d.h`, `gpu3d.h` e `motor_gpu.c`, e um driver por família
  (`acel_intel.c`, `acel_radeon.c`, `acel_r600.c`, `gpu3d_r600.c`, `gpu3d_evergreen.c`...).
  Quais placas têm suporte e com qual versão está em [`docs/gpus.md`](gpus.md); a 3D pela GPU
  está explicada em [`docs/gpu3d.md`](gpu3d.md).
- **Tempo e registro**: `relogio.c` (PIT e CMOS), `serial.c` (o "log" do kernel pela COM1),
  `terminal_es.c` e `terminal_grafico.c`.

### 4.6 `kernel/rede/`: a rede e o Wi-Fi

- **Pilha de rede** (`rede.c`): Ethernet, ARP, IPv4, ICMP (`ping`), UDP, DHCP e DNS.
- **TCP** (`tcp.c`) e **sockets** (`socket.c`): a conexão confiável e a interface que os programas veem.
- **Drivers de placas**: `e1000.c`, `r8169.c`, `rtl8139.c`, `ne2000.c`, `alc.c` (Atheros AR813x).
- **Wi-Fi**: `wifi.c` (cliente 802.11 com o pendrive RTL8188EU), `rtl8188eu.c`, `rt2870.c` (Ralink),
  `ar9485.c` (Atheros integrada) e `wifi_pcie.c`. A parte que não toca no hardware (criptografia
  WPA2 e WPA3) está em `kernel/lib/wpa.c` e `kernel/lib/sae.c`. Ver [`docs/wifi_usb.md`](wifi_usb.md)
  e [`docs/wifi-ar9485.md`](wifi-ar9485.md).
- **Túnel e VPN**: `tunel.c` (a interface `wg0` do WireGuard, [`docs/wireguard.md`](wireguard.md)).
- **Bloqueio de publicidade por DNS** (`bloqueo.c`, decisão D218).

Os arquivos com sufixo `_logica.h` são a **parte pura** de um driver: cálculos e decodificação
que não tocam no hardware. Assim podem ser testados no PC, sem placa. É um padrão que se repete
em todo o kernel.

### 4.7 `kernel/usb/` e `kernel/bluetooth/`

- **USB** (`usb.c`): enumeração dos dispositivos, hubs e conexão a quente.
  Os controladores são `xhci.c` (USB 3.0), `ehci.c` (2.0), `ohci.c` e `uhci.c` (1.1).
  As classes de dispositivo: `armazenamento.c` (pendrives, com SCSI), `hid.c` (teclados e mouses),
  `usbaudio.c` (som), `uvc.c` (câmeras). Ver [`docs/usb3.md`](usb3.md).
- **Bluetooth** (`kernel/bluetooth/`): a pilha clássica se divide em camadas, como no livro:
  `bt_hci.c` (comandos e eventos do controlador), `bt_l2cap.c` (canais), `bt_sdp.c` (buscar
  serviços), `bt_hid.c` (teclados e mouses sem fio) e `bt_gestor.c` (dispositivos e
  emparelhamento). `bt_fw.c` carrega o firmware de alguns chips. Ver [`docs/bluetooth.md`](bluetooth.md).

### 4.8 `kernel/gui/`: a área de trabalho

- **Compositor e janelas** (`janelas.c`): o gerenciador de janelas. Decide qual janela fica por cima,
  a arrasta, a minimiza e a fecha.
- **Desenho** (`desenho.c`, `escala.c`, `rapido.c` de `kernel/lib`): desenho com recorte, ampliação
  e cópias rápidas.
- **Área de trabalho** (`area_trabalho.c`): fundo, mascote, ícones e menu do botão direito (`menu.c`).
- **Barra de tarefas** (`barra.c`): botão Iniciar, botões das janelas, bandeja e relógio.
- **Menu Iniciar** (`inicio.c`): os menus de colunas. As entradas de cada categoria estão na
  tabela `programas[]` de `barra.c`. A lista completa está no
  [guia de aplicações](guia-aplicaciones_PT.md).
- **Estilos e cores** (`aparencia.c`, `arte.c`, `idioma.c`): cor de destaque, desenhos em pixel
  art e o idioma da interface (decisão D313).
- **Avisos, energia e sessão**: `avisos.c` (notificações), `ocio.c` (protetor de tela e bloqueio),
  `sessao.c` (início e encerramento de sessão), `permisos_dialogo.c` (senha de administrador).

### 4.9 `kernel/lib/`: funções puras

São funções que **não tocam no hardware**: podem ser entendidas e testadas sem máquina real.

- Formatos e texto: `formatar.c` (como `printf`), `texto.c`, `caminho.c` (caminhos `/`, `.`, `..`),
  `catalogo.c` (traduções).
- Criptografia: `sha256.c`, `p256.c`, `wpa.c`, `wpa_cifra.c`, `sae.c`.
- Cópias rápidas: `rapido.c`, `sse1.c`, `sse2.c`, `sse3.c` e `ssse3.c`. Cada um usa a instrução
  que a CPU tem.
- Conversões de sensores e energia: `sensores_conv.c`, `bateria_conv.c`, `brilho_conv.c`,
  `frequencia_pol.c`.
- Tempo: `fuso.c` (fusos horários com as regras POSIX TZ).
- Descompressão: `inflar.c` (DEFLATE, para a raiz comprimida do CD).

### 4.10 `kernel/terceros/`: código de outros projetos

Aqui está o que **não** foi escrito neste projeto. Cada origem tem sua pasta e sua licença:

- `uacpi/`: a biblioteca **uACPI** (MIT), que interpreta as tabelas e a linguagem AML do ACPI.
- `bsd/` (FreeBSD e OpenBSD, licença BSD) e `linux/` (Linux, GPL-2.0), com dados de registros e
  tabelas de inicialização de placas.
- `mit/` (X.org e o interpretador ATOM das Radeon, MIT).

O que vem de fora está explicado em [`docs/terceros.md`](terceros.md) e a regra de autoria
(decisões D034 e D073) em [`docs/decisoes.md`](decisoes.md). As partes GPL são isoladas em
`kernel/terceros/linux/` para que a variante sem GPL (`make SEM_GPL=1`) possa ser compilada sem elas.

---

## 5. Como se faz uma chamada ao sistema, passo a passo

Um programa não pode tocar no disco nem na tela por conta própria: o processador lhe impede de
usar certas instruções (está no **anel 3**). Para pedir algo ao kernel, usa uma **chamada ao
sistema**. Como exemplo, seguimos o **`write`**, a chamada que escreve dados em um arquivo, uma
tubulação ou no terminal.

```
 programa (anillo 3)
   write(fd, buf, n)                      userland/libc/posix.c
     └─ escrever(fd, buf, n)              userland/libsagui/chamadas.c
          └─ EAX=12, EBX=fd, ECX=buf, EDX=n;  int $0x80
 ───────────────────────────── cambio de anillo (la CPU entra al kernel) ─────────
 kernel (anillo 0)
   IDT, puerta 0x80                       kernel/arch/idt.c  → tratar_interrupcao()
     └─ tratar_sistema(cuadro)            kernel/nucleo/sistema.c
          ├─ ¿número válido? ¿punteros del usuario correctos? (usuario_ok)
          ├─ ¿permitido para este usuario? (kernel/nucleo/permisos.c)
          └─ tabela[SIS_ESCREVER]  →  sis_escrever()      kernel/fs/arquivos.c
               └─ escrever_em()  →  vfs_escrever()        kernel/fs/vfs.c
                    └─ fichero: driver del sistema (ext2, FAT32...) + caché de bloques
                       tubería: kernel/fs/pipe.c
                       terminal: kernel/fs/tv.c
 ───────────────────────────── vuelta al programa ───────────────────────────────
   EAX = bytes escritos (o negativo si hay error)
   write() devuelve ese valor
```

### 5.1 Os passos, um por um

1. **O programa chama o `write`.** É a função da *libc* (`userland/libc/posix.c`). As funções no
   estilo POSIX vêm da libc; as específicas do SaguiOS, da libsagui.
2. **O `escrever` coloca os números nos registradores.** `userland/libsagui/chamadas.c` coloca o
   número da chamada em `EAX` (`SIS_ESCREVER` vale **12**, segundo `kernel/inc/abi.h`), os
   argumentos em `EBX`, `ECX` e `EDX`, e executa `int $0x80`. Esse é o contrato que descreve o
   cabeçalho `abi.h`: "int 0x80, número em EAX, argumentos em EBX, ECX, EDX, resultado em EAX".
3. **A CPU passa ao kernel.** A instrução `int 0x80` procura a entrada 0x80 da **IDT**
   (`kernel/arch/idt.c`). Como essa porta permite chamá-la a partir do anel 3, a CPU muda para a
   pilha do kernel (a TSS de `gdt.c`) e salta para o kernel.
4. **O despachante de chamadas.** `sistema_iniciar()` (`kernel/nucleo/sistema.c`) registra
   `tratar_sistema` para esse vetor. A função lê `EAX` e consulta a **tabela** `tabela[]`, que
   aponta cada número para sua função (`[SIS_ESCREVER] = sis_escrever`).
5. **Verificações.** Antes de executar qualquer coisa, o kernel confere se o número existe, se os
   ponteiros (como `buf`) estão dentro da memória do programa (`usuario_ok`) e se o usuário pode
   fazer essa chamada (a tabela de classes de `permisos.c`).
6. **O trabalho real.** `sis_escrever` (`kernel/fs/arquivos.c`) procura o descritor `fd` na tabela
   do processo. Depois, `vfs_escrever` (`kernel/fs/vfs.c`) decide a qual sistema de arquivos ou
   dispositivo esse descritor corresponde.
7. **Se for preciso esperar.** Se a chamada tiver que esperar (por exemplo, ler uma tubulação
   vazia), o kernel põe o processo para dormir e **deixa o ponteiro de instrução apontando para o
   `int 0x80`**. Quando o processo acorda, a chamada é repetida (`sistema.c`, cabeçalho e linha
   `eip -= 2`).
8. **A volta.** O resultado é guardado em `EAX`: bytes escritos, ou um número negativo se falhar.
   O kernel restaura os registradores do programa e retorna (`iret`). A libc recebe o valor e o
   `write` o devolve.

> **Para praticar:** procure `SIS_ESCREVER` no código. Você verá a constante em `abi.h`, seu uso em
> `chamadas.c`, a tabela em `sistema.c` e a permissão em `permisos.c`. É o percurso completo de
> qualquer outra chamada, mudando só o nome.

---

## 6. Tabelas de arquivos

Cada tabela tem **todos** os arquivos `.c` e `.h` de sua pasta, com uma linha sobre o que fazem.
A linha sai do comentário de cabeçalho de cada arquivo (quase todos começam com
`/* nombre.c - qué hace ... */`). As referências `(D…)` são decisões de
[`docs/decisoes.md`](decisoes.md) e as `H…` e "mejora" são a numeração histórica do projeto.
O caminho é relativo à pasta da tabela.

### 6.1 kernel/arch/ (GDT, IDT, interrupções, APIC, SMP)

| Arquivo | O que faz |
| --- | --- |
| `excecoes.c` | a tela de erro do SaguiOS |
| `fpu.c` | FPU x87, MMX e SSE para os programas |
| `gdt.c` | tabela de segmentos do kernel e a TSS (D157: uma por CPU) |
| `idt.c` | tabela de interrupções e o despachante |
| `ioapic.c` | o IOAPIC: as IRQ dos dispositivos sem o 8259 (D166) |
| `irq_rotas.c` | o caminho de cada IRQ PCI: autoteste na inicialização, _PRT com uACPI e volta ao 8259 por linha (D211) |
| `lapic.c` | o APIC local de cada processador (D157) |
| `msr.c` | leitura de MSR que não derruba o kernel (D092) |
| `pic.c` | os dois controladores de interrupção 8259 (mestre e escravo) |


### 6.2 kernel/mem/ (memória física, paginação, PAE/NX, swap comprimida, cópia na escrita)

| Arquivo | O que faz |
| --- | --- |
| `compressor.c` | compressão de páginas para a swap comprimida (D352) |
| `copia_escrita.c` | a contagem de referências das páginas compartilhadas pelo fork (D401) |
| `fisica.c` | gerenciador de páginas físicas (mapas de bits) |
| `heap.c` | kmalloc/kfree: classes de tamanho para blocos pequenos e páginas inteiras para os grandes |
| `imagens.c` | páginas somente leitura das bibliotecas compartilhadas entre processos (D378, fase 3; projeto em docs/enlace-dinamico.md §15 e em D378) |
| `memalta.c` | janelas temporárias para páginas físicas (D348) |
| `paginacao.c` | paginação em dois modos: clássica de 2 níveis ou PAE de 3 (D010, D049, D349) |
| `troca.c` | swap comprimida na RAM (D352) |
| `wc.c` | write-combining (WC) para o framebuffer |


### 6.3 kernel/nucleo/ (processos, threads, escalonador, sinais, chamadas, usuários e permissões)

| Arquivo | O que faz |
| --- | --- |
| `acoes.c` | ações comuns do sistema (volume, silêncio, brilho, rádio) e seu aviso na tela (D403) |
| `aleatorio.c` | números imprevisíveis para o TLS, o TCP e o DNS (D061) |
| `atomicas.c` | a parte de 64 bits das primitivas atômicas (D150) |
| `autoteste.c` | testes que o kernel executa sobre si mesmo |
| `bifurcar.c` | fork com cópia na escrita e waitpid (D401) |
| `carregador.c` | cria processos a partir de executáveis ELF32 (H13) |
| `chamadas_processos.c` | chamadas dos grupos "processos" e "diversas" |
| `console.c` | a saída de texto do kernel: kprintf, klog e o console |
| `cpu_dados.c` | a área de dados por CPU (D150, D157) |
| `energia.c` | reiniciar e desligar a máquina |
| `hilos.c` | threads de usuário e espera por endereço (futex) (D167) |
| `hora.c` | a hora do sistema e o fuso horário (D080) |
| `info_hardware.c` | o que o infopc (D041) precisa e só o kernel alcança |
| `kconf.c` | opções do kernel que são guardadas pelo SaguiOS e valem na próxima inicialização (D353) |
| `permisos.c` | quem pode fazer o quê (D255 e D282, melhoria 098, fases 1 e 2) |
| `pilha.c` | pilhas de usuário que crescem sob demanda, com guarda (D186) |
| `planificadores.c` | as políticas do escalonador entre as CPU (D174) |
| `portapapeis.c` | a área de transferência do sistema (D074) |
| `principal.c` | ponto de entrada em C do kernel do SaguiOS |
| `processos.c` | tabela de processos e escalonador round-robin preemptivo |
| `rastro.c` | marcas que sobrevivem a um reinício a quente (ver rastro.h) |
| `sinais.c` | sinais POSIX (D401) |
| `sistema.c` | porta de entrada das chamadas ao sistema (int 0x80) |
| `smp.c` | a inicialização dos processadores secundários (D157, D164) |
| `spinlock.c` | travas de espera ativa do kernel (D150) |
| `supervisor.c` | o gerenciador de serviços visto a partir do kernel (D216) |
| `trava.c` | exclusão mútua entre processos e tarefas do kernel (D095) |
| `trava_geral.c` | a trava geral do kernel (D165) |
| `usuarios.c` | SIS_USUARIO: a identidade dos processos (D255, melhoria 098, fase 1) |


### 6.4 kernel/fs/ (VFS, ext2/3/4, FAT32, exFAT, ISO 9660, UDF, cache de disco, sobreposição e raiz)

| Arquivo | O que faz |
| --- | --- |
| `arquivos.c` | arquivos abertos e as chamadas de arquivos e pastas (H14) |
| `blocos.c` | acesso bruto aos setores de um disco (D179) |
| `cache_disco.c` | cache de blocos com leitura antecipada e gravação adiada (D281) |
| `cdda.c` | o sistema de arquivos "cdda": um CD de áudio como uma pasta de WAV (D233) |
| `cdda_logica.c` | o que não toca no hardware do CD de áudio (D233): a tabela de conteúdo, o CD-TEXT, os nomes dos arquivos, o cabeçalho WAV e a lista |
| `conf_sistema.c` | a configuração persistente do sistema (D255, opção A do mantenedor) |
| `disco_comprimido.c` | a raiz do CD comprimida por blocos, vista como um disco ("rz0"; v8.5.4.2, D379 P5) |
| `disco_ram.c` | um disco na RAM com a cópia de um volume ("ram0"; D379, fase T1: toram) |
| `discos.c` | discos, partições, formatos e avisos (H32, H34, D040) |
| `exfat.c` | o sistema de arquivos exFAT dos pendrives e cartões grandes (D325) |
| `ext2.c` | ext2, ext3 e ext4: leitura e gravação (D040, D326) |
| `fat32.c` | o sistema de arquivos FAT32 do pendrive de dados (H32, H33) |
| `iso9660.c` | leitura dos CD/DVD com ISO 9660 (D207) |
| `jbd2.c` | reprodução e gravação do diário JBD2 (D326) |
| `logs_disco.c` | o registro do kernel na partição SAGUI-LOGS (D120) |
| `montagem.c` | tabela de montagens e /etc/fstab (D040) |
| `pipe.c` | comunicação entre processos por um buffer circular no kernel |
| `raiz.c` | a raiz do sistema em um disco (D379, protótipo da melhoria 035 F2) |
| `ramfs.c` | um ramo da árvore na RAM montado como mais um volume (D379, fase R3) |
| `sgfs.c` | leitura do ramdisk SGFS |
| `sobreposicao.c` | a raiz da live: o sistema do meio, somente leitura, com uma camada gravável por cima (D379, fase L1; no estilo do overlayfs do Linux) |
| `tv.c` | terminal virtual (pseudoterminal), D036 |
| `udf.c` | leitura dos CD/DVD com UDF (D233), somente leitura |
| `vfs.c` | camada de arquivos virtual com a camada gravável na RAM (H14) |


### 6.5 kernel/dispositivos/ (discos, som, vídeo, 2D e 3D pela GPU, ACPI/uACPI, notebooks, sensores)

| Arquivo | O que faz |
| --- | --- |
| `ac97.c` | controladores AC'97 da Intel (ICH) e compatíveis (D089) |
| `acel_gcn.c` | copiar e preencher retângulos com o motor DMA das Radeon GCN 1.0 (Southern Islands: Tahiti, Pitcairn, Cape Verde, Oland; HD 7000, R7/R9 200 e 300 dessas famílias) (D360) |
| `acel_intel.c` | o blitter das GPU Intel, da Gen3 à Gen8 (D195, D359) |
| `acel_intel_logica.h` | a parte pura do blitter Intel (D195, D359): os modelos da Gen7 (Ivy Bridge), Gen7.5 (Haswell) e Gen8 (Broadwell) com identificador exato, e as famílias que são reconhecidas mas não têm 2D |
| `acel_mach64.c` | o motor 2D das ATI Mach64 e Rage (GX, CT, VT, Rage II, Rage Pro, Rage LT/XL/Mobility) (D308) |
| `acel_nv.c` | o motor 2D das NVIDIA GeForce 6/7 e Go 6xxx/7xxx (NV4x/G7x) e das integradas nos chipsets nForce (GeForce 6100/6150 e 7025/7050/7100/7150: C51, C61, C67, C68 e C73) (D224) |
| `acel_nv_logica.h` | a parte pura do acelerador 2D das GeForce 6/7 (D224): os modelos (identificadores PCI exatos e o chip que tem que dizer PMC_BOOT_0), o formato das ordens do buffer (cabeçalhos, pontos, tamanhos, saltos), a tabela hash do PRAMIN e o pitch da superfície |
| `acel_r600.c` | copiar e preencher retângulos com o motor DMA das Radeon R6xx/R7xx (D223; a camada comum é a da D195) |
| `acel_radeon.c` | o motor 2D clássico das Radeon R100 a R500 (D195) |
| `acel_s3.c` | o 2D das S3 Trio64 e ViRGE (D308) |
| `acel_tdfx.c` | o motor 2D das 3dfx Banshee, Voodoo3 e Voodoo4/5 (VSA-100) (D305) |
| `acel_vmware.c` | acelerador 2D da VMware SVGA II (PCI 15ad:0405, D195) |
| `acpi.c` | desligamento pelo ACPI (estado S5) e botão de ligar (D113, melhoria 030) |
| `ahci.c` | driver do controlador SATA AHCI (D155) |
| `ata.c` | discos internos ATA (IDE) por PIO, sem DMA nem interrupções (D040) |
| `atapi.c` | leitores de CD/DVD (ATAPI), somente leitura (D207) |
| `atom_sintetica.h` | uma BIOS ATOM de brinquedo, construída à mão, para testar sem placa (D399) |
| `audio.c` | o mixador de som do kernel e as chamadas de áudio (D088-D090) |
| `bar.c` | as BAR (as janelas de endereços) de um dispositivo PCI (D158) |
| `bateria.c` | a bateria do notebook pelo ACPI (D192) |
| `bga.c` | placa de vídeo do Bochs/QEMU ("dispi", BGA) |
| `bga_extra.c` | segundas (e terceiras) placas do Bochs/QEMU como saídas de vídeo extras (D290) |
| `captura.c` | entrada de som (microfone) do kernel (D292) |
| `ec.c` | o controlador embutido (EC, ACPI PNP0C09) para a AML (D192) |
| `eist_antiguo.c` | Enhanced SpeedStep dos Intel antigos (D177, fases A e B) |
| `energia_radeon.c` | relógios e estados de energia das Radeon R5xx pela BIOS ATOM (D399) |
| `energia_sim.c` | driver de energia de mentira para os testes no QEMU (D399, mkimagem.py --gpu-energia-simulada) |
| `evergreen_3d_pacotes.c` | pacotes PM4, shaders e valores de início do motor 3D das Radeon Evergreen e Northern Islands sem Cayman (D427) |
| `evergreen_dma.c` | lista de modelos, pacote de cópia e verificação da tela das Radeon Evergreen e Northern Islands (D286) |
| `frequencia.c` | driver de frequência da CPU com limite de temperatura (D106, melhoria 061) |
| `fujitsu.c` | os dispositivos ACPI próprios da Fujitsu (e Fujitsu Siemens) (D403) |
| `g3d_v2_provas.c` | testes das capacidades gpu3d v2 de um driver (D426) |
| `gart_r700_logica.h` | a GART (memória virtual, contexto 0) das Radeon R7xx (RV710, RV730, RV740, RV770), D406 |
| `gcn_dma.c` | lista de modelos GCN, pacotes do DMA das Southern Islands, verificação da tela (DCE 6) e preenchimento com CONSTANT_FILL (D360) |
| `gen3_gtt.h` | RAM do sistema na GTT da Intel Gen3 (915, 945, G33, Pineview), D358 |
| `gen3_pacotes.h` | os comandos 3D das GPU Intel Gen3 (915, 945, G33, Pineview), D285 |
| `gpu3d_evergreen.c` | 3D pela GPU nas Radeon Evergreen (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) e Northern Islands sem Cayman (HD 6000 e HD 7500M/7600M: Barts, Turks, Caicos), sob a camada comum gpu3d (D427) |
| `gpu3d_intel.c` | 3D pela GPU na Intel Gen3: 915, 945, G33 e Pineview (D285, D358) |
| `gpu3d_r500.c` | 3D pela GPU nas Radeon R5xx (X1300 a X1950: RV515, R520, RV530, RV560, RV570, R580), sob a camada comum gpu3d (D285, D287) |
| `gpu3d_r600.c` | 3D pela GPU nas Radeon R7xx (RV710, RV730, RV740, RV770) e, desde a D357, R6xx (R600, RV610, RV620, RV630, RV635, RV670): o processador de ordens (CP) com seu microcódigo, o estado do motor 3D e os desenhos, sob a camada comum gpu3d |
| `gpu3d_sim.c` | um driver 3D de mentira, pela CPU, para testar a camada comum (D285) |
| `gpu_energia_logica.h` | lógica pura da energia da GPU (D399): o que se pode testar no PC |
| `hda.c` | Intel High Definition Audio (D089) |
| `hda_ajustes.c` | escolha do ajuste de um codec HDA (D107) |
| `hp_wmi.c` | rádios e teclas do painel dos notebooks HP pelo WMI (D405) |
| `i8042.c` | o controlador do teclado e do touchpad (H19-fix) |
| `injecao.c` | entrada injetada para a área de trabalho remota (D251, melhoria 097) |
| `isa.c` | o barramento ISA (D302): o controlador de DMA 8237, a memória baixa para ele e a lista de dispositivos ISA detectados (para o infopc e o lspci) |
| `isapnp.c` | ISA Plug and Play (D302): encontrar as placas ISA PnP e configurar uma Sound Blaster da Creative (Vibra16, SB16 PnP, AWE32/64 PnP) que a BIOS deixou sem ativar |
| `mandos.c` | a camada comum de controles e joysticks (D307) |
| `modos_video.c` | lista de modos, troca a quente e resolução preferida |
| `motor_gpu.c` | o que o kernel sabe do motor 3D pela GPU (D158) |
| `mouse.c` | driver do mouse PS/2 (o touchpad do G470 fala o mesmo protocolo) |
| `nvme.c` | driver do controlador NVMe (D265), escrito a partir da especificação pública NVM Express 1.4 / 2.0 (sem código de outros sistemas) |
| `painel.c` | painéis táteis PS/2 (touchpads) em modo absoluto, com gestos (D407) |
| `painel_logica.h` | a parte pura dos painéis táteis PS/2 (D407): reconhecer o modelo pelas suas respostas, decodificar os pacotes absolutos de Synaptics, ALPS e Elantech e converter os toques em movimento, cliques e roda (os gestos) |
| `pci.c` | o barramento PCI (H29) |
| `pci_realoc.c` | baixa abaixo de 4 GB uma BAR de 64 bits que o firmware deixou no alto (D283) |
| `portajogos.c` | o joystick analógico da porta de jogos (0x201) (D307) |
| `portatil.c` | o próprio dos notebooks pelo ACPI: o brilho, os avisos do firmware, SIS_PORTATIL e o despejo das tabelas (D403) |
| `prt.c` | o roteamento das interrupções PCI do firmware (_PRT) com o uACPI (D211) |
| `r500_pacotes.h` | as ordens 3D das Radeon R5xx (RV515, R520, RV530, RV560, RV570, R580) para o processador de ordens (CP): estado do pipeline (VAP, RS, US, FG, RB3D, ZB), o programa de vértices e os dois programas de fragmentos |
| `r600_3d_pacotes.c` | pacotes PM4, shaders e valores de início do motor 3D das Radeon R6xx/R7xx (D284) |
| `r600_dma_pacotes.c` | pacotes do motor DMA das Radeon R6xx/R7xx e a divisão dos retângulos em cópias lineares (D223) |
| `radeon2d_ordens.h` | as escritas de registradores de uma ordem do motor 2D das Radeon R100-R500 (D195, D387, D391), sem efeitos: as mesmas, na mesma ordem, vão por MMIO (acel_radeon.c) ou, com o CP em marcha, pelo anel como PACKET0 (gpu3d_r500.c) |
| `relogio.c` | o PIT (temporizador programável) e o relógio CMOS |
| `sb16.c` | a Sound Blaster 16 da Creative, no barramento ISA (D302) |
| `sensores.c` | monitor de hardware: temperatura e frequência da CPU (D092, melhoria 050) |
| `serial.c` | porta serial COM1 (0x3F8), o "log" do kernel |
| `som.c` | bipe pelo alto-falante do PC (bônus B3) |
| `teclado.c` | driver do teclado PS/2 (controlador i8042, IRQ 1), layouts espanhol (ES, o padrão) e ABNT2 |
| `telas.c` | várias telas: registro de saídas, disposição da área de trabalho e configuração (D290) |
| `terminal_es.c` | o terminal visto pelos programas |
| `terminal_grafico.c` | o terminal desenhado em modo gráfico com a fonte própria (H18) |
| `toshiba.c` | a interface HCI dos notebooks Toshiba (D403) |
| `uacpi_sagui.c` | a camada do kernel para o uACPI e a tarefa "acpi-aml" (D192) |
| `vga_texto.c` | console em modo texto VGA (80x25), usado nas fases 1 a 3 |
| `video.c` | framebuffer VESA com buffer duplo |
| `video_acpi.c` | o ACPI Video padrão: brilho da tela do notebook e avisos das teclas de brilho e de tela (D403; especificação ACPI, apêndice B «Video Extensions») |
| `volcado_logica.h` | a parte pura do despejo do buffer de trás para a tela pela GPU (D387): entradas da GART PCIE das Radeon R300-R5xx, onde pôr sua janela no mapa do controlador de memória (MC) e a divisão de um retângulo em faixas para o motor 2D |
| `vram_zona_logica.h` | onde pôr uma zona de trabalho da GPU (fila de ordens, buffers do 3D) na memória de vídeo sem sobrescrever nada do que a placa mostra (D386) |
| `wmi.c` | a ponte WMI do ACPI (dispositivos PNP0C14) sobre o uACPI (D405) |
| `zonas_termicas.c` | as zonas térmicas do ACPI (ThermalZone, _TMP), para mostrá-las junto à GPU (D399) |


### 6.6 kernel/rede/ (pilha TCP/IP, drivers de rede, Wi-Fi)

| Arquivo | O que faz |
| --- | --- |
| `alc.c` | driver EXPERIMENTAL das Ethernet Atheros AR813x/AR815x (família L1C/L2C, o "atl1c" do Linux), D200 |
| `alc_logica.h` | a parte pura do driver alc (Atheros AR813x/AR815x, D200): modelos, MAC, descritores, estado do PHY e valores dos registradores de configuração |
| `ar9485.c` | Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, 802.11b/g/n 1x1, 2,4 GHz), D280 |
| `ar9485_logica.h` | a parte pura do driver do Atheros AR9485 (D280): leitura e restauração da imagem da EEPROM/OTP (blocos com soma de controle e modelos), tabelas de canais e a interpolação das potências |
| `bloqueo.c` | bloqueio de publicidade e rastreamento por DNS (D218) |
| `e1000.c` | driver da Intel PRO/1000 (82540EM e parecidas) |
| `ne2000.c` | driver das placas NE2000 (D329): o DP8390 com 16 KB de RAM própria, em PCI (Realtek RTL8029 e clones, 10ec:8029) e em ISA (portas 0x300/0x280/0x320/0x340) |
| `r8169.c` | driver EXPERIMENTAL da família Realtek RTL8101E/8102E/8105E e RTL8168/8169 |
| `r8169_ajustes.h` | ajustes a quente do driver r8169 (D124): RxConfig, RxMaxSize, tamanho do buffer no descritor e "multi" (bit 14 de RxConfig), pedidos com "ifconfig eth0 r8169 ..." para testar variantes no hardware real sem recompilar |
| `r8169_logica.h` | parte do driver r8169 que não toca no hardware (D091): revisão do chip, perfil de inicialização e decodificação |
| `r8169_sem_gpl.h` | o mínimo do driver r8169 para a compilação sem GPL (`make SEM_GPL=1`, melhoria 057, D109) |
| `rede.c` | a pilha de rede: Ethernet, ARP, IPv4, ICMP (ping), UDP, DHCP e DNS |
| `rt2870.c` | os pendrives Wi-Fi Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409) |
| `rt2870_logica.h` | a parte pura do suporte aos pendrives Wi-Fi Ralink RT2870 / RT3070 / RT3071 / RT3072 / RT5370 (D409, experimental, sem testar em hardware) |
| `rtl8139.c` | driver da Realtek RTL8139 (QEMU: -nic user,model=rtl8139) |
| `rtl8188eu.c` | o chip do pendrive Wi-Fi Realtek RTL8188EU (USB 0bda:8179), D215 |
| `rtl8192cu_logica.h` | a parte pura do suporte aos pendrives Wi-Fi Realtek RTL8188CU / RTL8192CU / RTL8188RU (D409, experimental, sem testar em hardware) |
| `socket.c` | sockets: a rede vista pelos programas |
| `tcp.c` | TCP: cliente e servidor simples |
| `tunel.c` | a interface do túnel, wg0 (D363) |
| `wifi.c` | Wi-Fi cliente (802.11 b/g) com o pendrive Realtek RTL8188EU (D215) |
| `wifi_pcie.c` | identificação de mais Wi-Fi integradas PCIe (D410), experimental |
| `wifi_pcie_logica.h` | a parte pura do suporte a mais Wi-Fi integradas PCIe (D410): catálogo de placas por identificador PCI, famílias Atheros (ath9k/athn), decodificação do SREV, geometria e leitura da ROM da família AR5008 |
| `wifi_usb_ids.h` | VID:PID dos pendrives Wi-Fi USB (D409) |
| `wifi_usb_logica.h` | qual família de pendrive Wi-Fi USB é um VID:PID (D409) |


### 6.7 kernel/usb/ (controladores USB e classes de dispositivo)

| Arquivo | O que faz |
| --- | --- |
| `armazenamento.c` | pendrives: Bulk-Only Transport + SCSI (H31) |
| `ehci.c` | driver do controlador USB 2.0 (EHCI) (H29) |
| `hid.c` | teclados e mouses USB (classe HID) pelo xHCI, EHCI, UHCI e OHCI (D214, D267) |
| `isoent.h` | fila de pacotes recebidos de um fluxo isócrono de entrada (D277) |
| `ohci.c` | driver do controlador USB 1.1 OHCI (AMD, NVIDIA, SiS, ALi, Apple...) (D267) |
| `uhci.c` | driver do controlador USB 1.1 UHCI (Intel PIIX/ICH, VIA...) (D267) |
| `usb.c` | dispositivos USB: enumeração, hubs e conexão a quente (H30, D214) |
| `usbaudio.c` | áudio USB, classe Audio 1.0 (UAC 1), saída de som (D275) |
| `usbbt.c` | transporte USB da pilha Bluetooth e a chamada SIS_BLUETOOTH (D279, melhoria 059) |
| `uvc.c` | câmeras UVC 1.1 (classe de vídeo USB) e a fonte de teste (D277) |
| `uvc_nucleo.c` | a parte pura do driver UVC 1.1 (D277) |
| `uvc_nucleo.h` | a parte pura do driver UVC 1.1 (classe de vídeo USB, D277) |
| `xhci.c` | driver do controlador USB 3.0 (xHCI) (D155, D214) |


### 6.8 kernel/bluetooth/ (pilha Bluetooth clássica)

| Arquivo | O que faz |
| --- | --- |
| `bt.h` | pilha Bluetooth clássico (BR/EDR) do SaguiOS (D279, melhoria 059, v8.2) |
| `bt_fw.c` | carregamento de firmware dos controladores Bluetooth Broadcom, Realtek e Atheros (D411) |
| `bt_fw.h` | carregamento de firmware e correções dos controladores Bluetooth que não iniciam prontos (D411) |
| `bt_gestor.c` | o gerenciador Bluetooth (D279): tabela de dispositivos, fluxo de conexão e emparelhamento, chaves de enlace guardadas e as operações que usam a chamada SIS_BLUETOOTH e o terminal |
| `bt_hci.c` | camada HCI da pilha Bluetooth (D279): comandos, eventos, tráfego ACL com créditos, início do controlador e o registro do adaptador |
| `bt_hid.c` | HIDP (D279): o perfil HID do Bluetooth sobre dois canais L2CAP, controle (PSM 0x11) e interrupção (PSM 0x13) |
| `bt_l2cap.c` | L2CAP básico (D279): sinalização pelo canal 1, canais dinâmicos em modo básico, remontagem dos quadros ACL fragmentados |
| `bt_sdp.c` | cliente SDP (D279): busca o serviço HID de um dispositivo com uma única requisição ServiceSearchAttributeRequest e lê da resposta os PSM dos canais, a subclasse, se é dispositivo de arranque e o descritor de relatório |


### 6.9 kernel/gui/ (compositor, janelas, área de trabalho, barra, menus, estilos)

| Arquivo | O que faz |
| --- | --- |
| `acel2d.c` | 2D pela GPU para a área de trabalho: a camada comum (D195) |
| `aparencia.c` | cor de destaque e fundo escolhidos na Configuração (D112) |
| `area_trabalho.c` | o fundo, a mascote, os ícones e o menu da área de trabalho (H20, H28, D051, D066) |
| `arte.c` | os desenhos da área de trabalho, em pixel art feito neste projeto |
| `atalhos.c` | atalhos de teclado da área de trabalho (v6.0 beta, D119) |
| `avisos.c` | notificações emergentes com o texto completo (issue #16, D269) |
| `barra.c` | a barra de tarefas: botão Iniciar, botões das tarefas, bandeja e relógio (D065, D361) |
| `desenho.c` | rotinas de desenho com recorte, usadas pela área de trabalho e pelo gerenciador de janelas |
| `escala.c` | cópia ampliada ou reduzida de pixels (tela cheia, D044) |
| `gpu3d.c` | 3D pela GPU: a camada comum (D285) |
| `gpu3d_mem.c` | partes comuns dos drivers 3D: memória da GPU por blocos, buffers e ponto flutuante com inteiros (D285) |
| `gpu_energia.c` | energia da GPU: a camada comum (D399) |
| `idioma.c` | idioma da interface do kernel e SIS_IDIOMA (D313, melhoria 118) |
| `inicio.c` | os menus Iniciar de colunas: "Dois colunas" e "Moderno" (D361) |
| `janelas.c` | o gerenciador de janelas (H21) |
| `menu.c` | menu de contexto do botão direito (D051) e submenus (D064) |
| `ocio.c` | energia e tela: protetor de tela, bloqueio e desligamento da tela por inatividade, aviso de bateria |
| `permisos_dialogo.c` | o diálogo de permissões de administrador (D269, melhoria 102, issue #16) |
| `sessao.c` | início de sessão gráfico, encerrar sessão e trocar de usuário (D340, melhoria 098, fase 3) |


### 6.10 kernel/lib/ (funções puras do kernel)

| Arquivo | O que faz |
| --- | --- |
| `acpi_pss.c` | leitor mínimo de _PSS, _PSD e _PCT sem interpretador AML (D177) |
| `bateria_conv.c` | porcentagem e tempo restante da bateria (D192) |
| `brilho_conv.c` | níveis de brilho dos notebooks (D403) |
| `caminho.c` | resolução de caminhos |
| `catalogo.c` | leitor de catálogos de tradução (D313) |
| `crc32c.c` | CRC32c (polinômio 0x1EDC6F41, em ordem refletida 0x82F63B78) e CRC16 (polinômio 0x8005, refletido 0xA001) |
| `div64.c` | as rotinas de 64 bits que o GCC chama em 32 bits (D192) |
| `formatar.c` | formatação de texto no estilo printf, escrita para o SaguiOS |
| `frequencia_pol.c` | políticas e limite de temperatura do driver de frequência (D106, melhoria 061) |
| `fuso.c` | interpretador de cadeias POSIX TZ e calendário (D080) |
| `hid_informe.c` | analisador de descritores de relatório HID e decodificação de controles (D307) |
| `inflar.c` | descompressão DEFLATE (RFC 1951) no núcleo (v8.5.4.2, D379: a raiz comprimida do CD) |
| `ocio_efectos.c` | os efeitos do protetor de tela (D422) |
| `p256.c` | a curva NIST P-256 para o SAE do WPA3 (D220) |
| `portajogos_conv.c` | as contas da porta de jogos (D307) |
| `rapido.c` | cópias e preenchimentos grandes da interface gráfica (D042) |
| `sae.c` | SAE do WPA3-Personal com P-256 (D220) |
| `sensores_conv.c` | fórmulas do monitor de hardware (D092) |
| `sha256.c` | SHA-256 escrito a partir da FIPS 180-4 (seções 4.1.2, 4.2.2, 5.3.3 e 6.2), D060 |
| `sse1.c` | cópias e preenchimentos só com SSE, sem SSE2 (D208) |
| `sse2.c` | cópias e preenchimentos com SSE2 (D042) |
| `sse3.c` | cópias com SSE3: lddqu para as leituras desalinhadas (D208) |
| `ssse3.c` | reordenar canais de pixels com pshufb (D208) |
| `texto.c` | rotinas de memória e de texto do kernel |
| `wifi_n.c` | desempenho do Wi-Fi do pendrive RTL8188EU (D238) |
| `wmi_wdg.c` | a análise de _WDG e os buffers da HP, sem hardware (D405) |
| `wpa.c` | criptografia e formatos do Wi-Fi cliente (D215) |
| `wpa_cifra.c` | primitivas do Wi-Fi para WEP, TKIP, o descritor 1 do EAPOL-Key, os AKM com SHA-256 (SAE, PSK-SHA256), o CCMP por software e o BIP (D220, v7.3.2) |


### 6.11 kernel/inc/ (cabeçalhos compartilhados)

| Arquivo | O que faz |
| --- | --- |
| `abi.h` | contrato entre o kernel e os programas: números das chamadas ao sistema e estruturas comuns |
| `acel2d.h` | 2D pela GPU para a área de trabalho: copiar e preencher retângulos (D195) |
| `acel_intel.h` | o anel de comandos das GPU Intel, compartilhado pelo blitter 2D (acel_intel.c, D195) e pelo 3D da Gen3 (gpu3d_intel.c, D285) |
| `acoes.h` | ações comuns do sistema: volume, silêncio, brilho, rádio (D403) |
| `acpi.h` | desligamento pelo ACPI (S5) e botão de ligar (D113) |
| `acpi_pss.h` | leitor mínimo de _PSS, _PSD e _PCT das tabelas ACPI (D177) |
| `ahci.h` | controlador SATA AHCI (D155) |
| `aleatorio.h` | gerador de números aleatórios do kernel (D061) |
| `aparencia.h` | cor de destaque da área de trabalho (D112) |
| `ar9485.h` | Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, família AR9003, sem firmware), D280 |
| `arte.h` | desenhos em pixel art (letras = cores da paleta) |
| `ata.h` | discos internos ATA/IDE por PIO (D040) e SATA/AHCI (D155) |
| `atapi.h` | leitores de CD/DVD (ATAPI), somente leitura (D207) |
| `atomicas.h` | primitivas atômicas do kernel (D150, base do SMP) |
| `audio.h` | placas de som (AC'97 e Intel HDA) e o mixador do kernel (D088-D090) |
| `autoteste.h` | testes internos do kernel, ativados só nas imagens de teste |
| `avisos.h` | notificações emergentes (H34, D125, D233; issue #16, D269) |
| `bateria.h` | uACPI, o controlador embutido e a bateria, vistos do resto do kernel (D192) |
| `bateria_conv.h` | dos números de _BIF/_BIX/_BST para porcentagem e tempo (D192) |
| `bga.h` | placa de vídeo do Bochs/QEMU/VirtualBox ("dispi"): troca de modo sem a BIOS (D059) |
| `bloqueo.h` | bloqueio de publicidade e rastreamento por DNS no resolvedor do kernel, no estilo de um sumidouro DNS (D218) |
| `bloqueo_tabla.h` | o formato da tabela do bloqueio de publicidade por DNS e a busca nela (D218) |
| `bluetooth.h` | Bluetooth com um controlador USB padrão (D279, melhoria 059, v8.2) |
| `bootinfo.h` | o que a etapa 2 entrega ao kernel (espelho de boot/bootinfo.inc) |
| `brilho_conv.h` | dos níveis de brilho que o firmware dá para índices e porcentagens (D403) |
| `cache_disco.h` | cache de blocos comum aos discos internos (D281) |
| `caminho.h` | montar e simplificar caminhos ("/", ".", "..") |
| `catalogo.h` | catálogos de tradução (D313, melhoria 118) |
| `cdda.h` | as faixas de um CD de áudio como arquivos WAV, somente leitura (D233) |
| `compressor.h` | compressão rápida de uma página de 4 KB para a swap (D352) |
| `conf_sistema.h` | a partição SAGUI-CONF: configuração persistente do sistema (D255) |
| `console.h` | saída de texto do kernel (tela + espelho na serial) |
| `copia_escrita.h` | contagem de referências das páginas físicas que compartilham vários espaços de endereços (cópia na escrita do fork, D401) |
| `cpu.h` | segmentos, interrupções e o quadro guardado na pilha em cada interrupção |
| `cpu_dados.h` | dados por CPU: a base do SMP (D150) e seu uso real (D157) |
| `crc32c.h` | CRC32c (Castagnoli) e CRC16 (o das somas de grupo do ext4 sem metadata_csum) |
| `desenho.h` | desenho com recorte sobre uma superfície de pixels (fase 5) |
| `disco.h` | discos e partições vistos pelo sistema de arquivos (fase 6, D040) |
| `disco_comprimido.h` | a raiz do CD comprimida por blocos (v8.5.4.2, D379 P5) |
| `disco_ram.h` | "ram0": a cópia na RAM de um volume (D379, toram) |
| `eist_antiguo.h` | Enhanced SpeedStep dos Intel antigos (D177) |
| `eq_fixo.h` | o filtro do equalizador em ponto fixo (D201) |
| `esquemas.h` | esquemas de aparência de todo o sistema (v8.5.5.2, D428) |
| `evergreen_3d.h` | pacotes PM4, shaders e valores de início do motor 3D das Radeon Evergreen (HD 5000: Cedar, Redwood, Juniper, Cypress, Hemlock) e Northern Islands sem Cayman (HD 6000 e HD 7500M/7600M: Barts, Turks, Caicos) (D427) |
| `evergreen_dma.h` | o próprio das Radeon Evergreen (HD 5000) e Northern Islands (HD 6000) para o 2D pelo motor DMA (D286): a lista exata de modelos, o pacote de cópia e a verificação do controlador de tela |
| `exfat.h` | leitura e gravação de exFAT (D325) |
| `fat32.h` | leitura e gravação de FAT32 (H32, H33) |
| `fontes_sistema.h` | fontes do sistema: tabela de tipografias embutidas no kernel |
| `fpu.h` | FPU x87, MMX e SSE para os programas (estado guardado por processo) |
| `frequencia.h` | driver de frequência da CPU (D106, melhoria 061) |
| `frequencia_pol.h` | a decisão do driver de frequência (D106, melhoria 061) |
| `fuso.h` | fusos horários com regras POSIX TZ (D080) |
| `g3d_v2_provas.h` | testes das capacidades gpu3d v2 de um driver (D426) |
| `gcn_dma.h` | o próprio das Radeon GCN para o 2D pela GPU (D360): a lista exata de modelos (GCN 1.0 a 5), os pacotes do motor DMA das Southern Islands (GCN 1.0) e a verificação do controlador de tela |
| `gpu3d.h` | 3D pela GPU: a camada comum e o que seus drivers compartilham (D285) |
| `gpu3d_diag.h` | diagnóstico do 3D comum a todos os drivers (D390; generaliza o que a D386 fez para a R5xx do Amilo) |
| `gpu_energia.h` | energia da GPU: relógios, estados, temperatura e VRAM (D399) |
| `gui.h` | área de trabalho e gerenciador de janelas (fase 5) |
| `hda_ajustes.h` | ajustes ("quirks") de codecs HDA por codec e subsistema (D107) |
| `hid_informe.h` | analisador de descritores de relatório HID e decodificação de controles (D307) |
| `i8042.h` | controlador do teclado e do touchpad |
| `idioma.h` | o idioma da interface do kernel (D313, melhoria 118) |
| `imagens.h` | páginas somente leitura das bibliotecas compartilhadas entre processos (D378, fase 3; docs/enlace-dinamico.md §4 e §15) |
| `inflar.h` | descompressão DEFLATE e CRC-32 no núcleo (v8.5.4.2, D379) |
| `info_hardware.h` | dados do hardware para o infopc (D041) |
| `ioapic.h` | o IOAPIC: o controlador de interrupções dos dispositivos que substitui o par de 8259 (D166) |
| `irq_rotas.h` | qual caminho segue cada IRQ dos dispositivos PCI e o autoteste que o verifica (D211) |
| `isa.h` | o barramento ISA: o controlador de DMA 8237, a memória baixa para ele e a lista de dispositivos ISA detectados (D302) |
| `iso9660.h` | sistema de arquivos dos CD/DVD (ISO 9660), somente leitura (D207) |
| `jbd2.h` | o diário JBD2 do ext3/ext4 (D326): reproduzir o pendente ao montar e gravar transações |
| `kconf.h` | opções do kernel que são alteradas pelo SaguiOS e valem na próxima inicialização (D353) |
| `lapic.h` | o APIC local (LAPIC) de cada processador (D157) |
| `mandos.h` | a camada comum de controles e joysticks (D307): USB HID, Xbox 360 com fio e a porta de jogos |
| `memalta.h` | janelas temporárias do kernel para páginas físicas (D348) |
| `memoria.h` | páginas físicas e o alocador do kernel |
| `modos_video.h` | lista de modos, troca a quente e resolução preferida (D058, D059) |
| `montagem.h` | tabela de montagens e a interface dos sistemas de arquivos (D040) |
| `mouse.h` | mouse/touchpad PS/2 (H19) |
| `msr.h` | leitura protegida de MSR (D092) |
| `nvme.h` | controlador NVMe (D265) |
| `ocio.h` | energia e tela: protetor de tela, bloqueio e desligamento da tela por inatividade, aviso de bateria e «não perturbe» (D422) |
| `ocio_efectos.h` | os efeitos do protetor de tela (D422) |
| `p256.h` | a curva elíptica NIST P-256 (FIPS 186-4, D.1.2.3; SEC 1) para o SAE do WPA3 (D220, v7.3.2) |
| `paginacao.h` | espaços de endereços (diretórios de páginas) em modo clássico ou PAE |
| `painel.h` | painéis táteis PS/2: Synaptics, ALPS e Elantech em modo absoluto, com gestos (D407) |
| `pci.h` | acesso ao espaço de configuração PCI (mecanismo 1, portas 0xCF8/0xCFC) |
| `pci_realoc.h` | realocar abaixo de 4 GB uma BAR de 64 bits que o firmware deixou no alto (D283) |
| `permisos.h` | quem pode fazer o quê: usuários e chamadas privilegiadas (D255, melhoria 098, fase 1) |
| `permisos_dialogo.h` | diálogo de permissões de administrador (D269, melhoria 102) |
| `pipe.h` | tubulações entre processos (H15) |
| `planificador.h` | os escalonadores de processos entre as CPU (D174) |
| `portajogos_conv.h` | as contas da porta de jogos (D307), sem tocar no hardware |
| `portas.h` | acesso às portas de E/S do x86 (instruções in/out) |
| `portatil.h` | o próprio dos notebooks visto do resto do kernel (D403) |
| `prazo.h` | esperas com limite de tempo real (D199, D207) |
| `processo.h` | processos e escalonador |
| `prt.h` | o roteamento das interrupções PCI do firmware (_PRT) com o uACPI (D211) |
| `r600_3d.h` | pacotes PM4, shaders e valores de início do motor 3D das Radeon R6xx/R7xx (D284) |
| `r600_base.h` | o que compartilham o 2D por DMA (acel_r600.c, D223) e o 3D pelo CP (gpu3d_r600.c, D284) das Radeon R6xx/R7xx: a sonda somente leitura da placa (modelo exato, BAR, MMIO e onde fica a tela no controlador) |
| `r600_dma.h` | pacotes do motor DMA das Radeon R6xx/R7xx e a divisão de um retângulo em cópias lineares (D223) |
| `radios.h` | os rádios e o painel dos HP (hp_wmi.c) e o despejo de tabelas ACPI, vistos a partir das chamadas ao sistema (D405) |
| `raiz.h` | a raiz do sistema em um disco (D379) |
| `ramfs.h` | uma pasta da árvore na RAM montada como volume (D379) |
| `rapido.h` | cópias e preenchimentos grandes da interface gráfica (D042) |
| `rastro.h` | marcas de depuração que sobrevivem a um reinício a quente (Amilo, dbg5, 2026-11-19) |
| `rede.h` | placas de rede, pilha IPv4 (ARP, IP, ICMP, UDP, DHCP, DNS, TCP) e sockets |
| `relogio.h` | tempo desde a inicialização (PIT) e data/hora (CMOS) |
| `sae.h` | SAE (Simultaneous Authentication of Equals) do WPA3-Personal com o grupo 19 (P-256), IEEE 802.11-2020 12.4 (D220, v7.3.2) |
| `sb16.h` | a Sound Blaster 16 (ISA) e o ISA PnP que as Vibra/AWE PnP precisam (D302) |
| `sensores.h` | o que o kernel pergunta ao monitor de hardware (D092) sem passar pela syscall |
| `sensores_conv.h` | as fórmulas do monitor de hardware (D092) |
| `sessao.h` | início de sessão gráfico, encerrar sessão e trocar de usuário (D340, melhoria 098, fase 3) |
| `sgfs.h` | leitor do sistema de arquivos SGFS (o formato é gerado por tools/mksgfs.py) |
| `sha256.h` | SHA-256 (FIPS 180-4), D060 |
| `sistema.h` | chamadas ao sistema por "int 0x80" (D007) |
| `smp.h` | início dos processadores secundários (D157) |
| `sobreposicao.h` | a raiz da live: ext2 somente leitura com uma camada gravável (D379) |
| `som.h` | o alto-falante do PC (bônus B3) |
| `spinlock.h` | travas de espera ativa do kernel (D150) |
| `sse2.h` | cópias e preenchimentos com SSE2 (D042) |
| `supervisor.h` | o gerenciador de serviços visto a partir do kernel (D216) |
| `teclado.h` | teclado PS/2 com layouts ES (padrão) e ABNT2 |
| `telas.h` | várias telas (saídas de vídeo) e área de trabalho estendida ou espelhada (melhoria 060, D290) |
| `terminal_es.h` | o terminal como arquivo (entrada 0 e saída 1/2 dos processos) |
| `texto.h` | funções de memória e de texto escritas para o kernel |
| `tipos.h` | tipos básicos do kernel. Sem libc: definem-se os próprios |
| `trava.h` | trava de exclusão mútua do kernel (D095) |
| `trava_geral.h` | a trava geral do kernel (D165) |
| `troca.h` | swap comprimida na RAM (D352, melhoria 125 fase 3b, melhoria 069 fase 3) |
| `tunel.h` | a interface do túnel wg0 (kernel/rede/tunel.c, D363) |
| `tv.h` | terminal virtual (pseudoterminal): um terminal para o Terminal em janela (D036) |
| `uacpi_sagui.h` | o que compartilham a camada do uACPI (uacpi_sagui.c), o controlador embutido (ec.c) e a bateria (bateria.c) |
| `uacpi_types.h` | os tipos do uACPI sobre os do kernel (D192) |
| `udf.h` | sistema de arquivos UDF dos CD/DVD, somente leitura (D233) |
| `usb.h` | controladores EHCI e xHCI, dispositivos USB, hubs, armazenamento e HID (fase 6; xHCI e HID na v7.3, D214) |
| `versao.h` | nome e versão do sistema, em um só lugar (D101) |
| `vfs.h` | camada de arquivos virtual: um caminho só para tudo (PRD 4.3) |
| `video.h` | modo gráfico VESA com buffer duplo (H17) |
| `wc.h` | write-combining no framebuffer (D057) |
| `wifi.h` | Wi-Fi cliente com o pendrive USB Realtek RTL8188EU (D215) |
| `wifi_n.h` | desempenho do Wi-Fi do pendrive RTL8188EU (D238): o detalhamento de uma transferência de recepção com vários quadros (agregação do chip), os elementos do 802.11n (HT) e do WMM, a resposta a um ADDBA e a escada de taxas |
| `wifi_pcie.h` | mais Wi-Fi integradas PCIe de notebooks (D410): reconhecimento por PCI da família Atheros ath9k (AR9285, AR9287, AR9380...), Broadcom BCM4313 e Ralink RT3090 (experimental: só identifica e registra) |
| `wmi_wdg.h` | a ponte WMI do ACPI (PNP0C14) sem hardware nem uACPI: a análise de _WDG e os formatos dos buffers da HP (D405) |
| `wpa.h` | criptografia e formatos do Wi-Fi cliente (D215): SHA-1, HMAC-SHA1, PBKDF2 (a PMK), o PRF do 802.11i, AES-128 com seu envoltório de chaves |
| `wpa_cifra.h` | as primitivas criptográficas do Wi-Fi que não são as do WPA2-CCMP (D220, v7.3.2): MD5 e HMAC-MD5 (descritor 1 do EAPOL-Key), RC4, CRC-32, Michael e a mistura de chave do TKIP, o encapsulamento TKIP e WEP |
| `xhci.h` | controlador USB 3.0 (xHCI) (D155, D214) |


### 6.12 kernel/terceros/ (código de outros projetos: uACPI, BSD, Linux, MIT)

| Arquivo | O que faz |
| --- | --- |
| `bsd/alc/alc_regs.h` | registradores e descritores das Ethernet Atheros AR813x/AR815x (do FreeBSD) |
| `bsd/athn/ar9003reg.h` | registradores da família Wi-Fi Atheros AR9003 (do BSD) |
| `bsd/athn/ar9485_datos.h` | tabelas de início do Atheros AR9485 (do BSD) |
| `bsd/athn/athnreg.h` | registradores comuns dos Atheros AR9003 (do BSD) |
| `bsd/coretemp/coretemp_tjmax.h` | TjMax por modelo de CPU Intel (do coretemp do FreeBSD) |
| `bsd/rtwn/r92creg_88e.h` | registradores e tabelas de início dos Realtek RTL8188E (do OpenBSD) |
| `bsd/rtwn/r92creg_92c.h` | registradores e tabelas de início dos Realtek RTL8188CU/8192CU (do OpenBSD) |
| `bsd/run/rt2860reg.h` | registradores e descritores dos Ralink RT2870/RT3070/RT5370 (do OpenBSD) |
| `linux/hda/idt_92hd71bxx.h` | dados do codec IDT/SigmaTel 92HD71Bxx (do Linux) |
| `linux/hwmon/sensores_tabelas.h` | tabela de desvio Tctl/Tdie dos Ryzen (do k10temp do Linux) |
| `linux/r8169/r8169_inicio.h` | sequências de inicialização das Realtek RTL8168/8111 (do Linux) |
| `linux/r8169/r8169_versoes.h` | versões e nomes de chip das Realtek RTL8168/8169 (do Linux) |
| `mit/atom/atom-bits.h` | bits e constantes do interpretador da BIOS ATOM das Radeon (MIT) |
| `mit/atom/atom.c` | interpretador das tabelas de comandos da BIOS ATOM das Radeon (MIT) |
| `mit/atom/atom.h` | interface do interpretador ATOM das Radeon (MIT) |
| `mit/atom/atom_compat.h` | compatibilidade do interpretador ATOM com o kernel (MIT) |
| `mit/nv/nv_2d.h` | métodos 2D e gravações de PGRAPH/PFIFO das NVIDIA GeForce 6/7 (do xf86-video-nv) |
| `uacpi/include/uacpi/acpi.h` | cabeçalho principal das estruturas ACPI |
| `uacpi/include/uacpi/context.h` | contexto global do uACPI |
| `uacpi/include/uacpi/event.h` | API pública de eventos ACPI (GPE e eventos fixos) |
| `uacpi/include/uacpi/helpers.h` | funções auxiliares do uACPI |
| `uacpi/include/uacpi/internal/compiler.h` | macros do compilador para o uACPI (uso interno) |
| `uacpi/include/uacpi/internal/context.h` | cópia local da FADT já verificada e convertida |
| `uacpi/include/uacpi/internal/dynamic_array.h` | arrays dinâmicos do uACPI (uso interno) |
| `uacpi/include/uacpi/internal/event.h` | eventos ACPI: interface interna (GPE e eventos fixos) |
| `uacpi/include/uacpi/internal/helpers.h` | funções auxiliares internas do uACPI |
| `uacpi/include/uacpi/internal/interpreter.h` | interpretador de AML: interface interna |
| `uacpi/include/uacpi/internal/io.h` | gravação de campos AML (uso interno) |
| `uacpi/include/uacpi/internal/log.h` | mensagens de depuração do uACPI |
| `uacpi/include/uacpi/internal/mutex.h` | mutex do uACPI sobre a API do kernel |
| `uacpi/include/uacpi/internal/namespace.h` | espaço de nomes ACPI: interface interna |
| `uacpi/include/uacpi/internal/notify.h` | notificações ACPI: interface interna |
| `uacpi/include/uacpi/internal/opcodes.h` | códigos de operação AML que o interpretador entende |
| `uacpi/include/uacpi/internal/opregion.h` | regiões de operação (acesso ao hardware a partir da AML): interface interna |
| `uacpi/include/uacpi/internal/osi.h` | suporte a _OSI: interface interna |
| `uacpi/include/uacpi/internal/registers.h` | registradores de hardware do ACPI: interface interna |
| `uacpi/include/uacpi/internal/resources.h` | recursos ACPI (IRQ, E/S, memória): conversão a partir da AML |
| `uacpi/include/uacpi/internal/shareable.h` | objetos compartilhados do interpretador (uso interno) |
| `uacpi/include/uacpi/internal/stdlib.h` | funções de libc mínimas usadas pelo uACPI |
| `uacpi/include/uacpi/internal/tables.h` | tabelas ACPI: interface interna |
| `uacpi/include/uacpi/internal/types.h` | objetos comuns do interpretador AML |
| `uacpi/include/uacpi/internal/utilities.h` | utilidades internas (nomes, ids EISA) |
| `uacpi/include/uacpi/io.h` | API pública de acesso às portas e à memória |
| `uacpi/include/uacpi/kernel_api.h` | API que o kernel do SaguiOS implementa para o uACPI |
| `uacpi/include/uacpi/log.h` | API pública das mensagens do uACPI |
| `uacpi/include/uacpi/namespace.h` | API pública do espaço de nomes ACPI |
| `uacpi/include/uacpi/notify.h` | API pública de notificações |
| `uacpi/include/uacpi/opregion.h` | API pública de regiões de operação |
| `uacpi/include/uacpi/osi.h` | API pública de _OSI |
| `uacpi/include/uacpi/platform/arch_helpers.h` | identificador de thread da plataforma (adapta-se a cada sistema) |
| `uacpi/include/uacpi/platform/atomic.h` | operações atômicas para o uACPI |
| `uacpi/include/uacpi/platform/compiler.h` | atributos e macros do compilador |
| `uacpi/include/uacpi/platform/config.h` | opções de compilação do uACPI (contexto, tamanhos) |
| `uacpi/include/uacpi/platform/libc.h` | funções de libc usadas pelo uACPI (com implementação padrão) |
| `uacpi/include/uacpi/platform/types.h` | tipos específicos da plataforma |
| `uacpi/include/uacpi/registers.h` | API pública dos registradores ACPI |
| `uacpi/include/uacpi/resources.h` | API pública de recursos (_CRS, _PRS) |
| `uacpi/include/uacpi/sleep.h` | API pública de estados de suspensão e desligamento |
| `uacpi/include/uacpi/status.h` | códigos de estado do uACPI |
| `uacpi/include/uacpi/tables.h` | API pública das tabelas ACPI |
| `uacpi/include/uacpi/types.h` | tipos públicos do uACPI |
| `uacpi/include/uacpi/uacpi.h` | cabeçalho principal do uACPI: inicialização e desligamento |
| `uacpi/include/uacpi/utilities.h` | API pública de utilidades (avaliar métodos, nomes) |
| `uacpi/source/default_handlers.c` | manipuladores padrão das regiões de operação (memória, E/S, PCI) |
| `uacpi/source/event.c` | eventos ACPI: GPE (eventos de finalidade geral) e eventos fixos |
| `uacpi/source/interpreter.c` | interpretador de AML: a máquina virtual que executa os métodos ACPI |
| `uacpi/source/io.c` | leitura e gravação dos campos AML (fields) sobre o hardware |
| `uacpi/source/mutex.c` | mutex do interpretador sobre a API do kernel |
| `uacpi/source/namespace.c` | espaço de nomes ACPI: árvore de objetos (a raiz `\`, `_SB`, etc.) |
| `uacpi/source/notify.c` | notificações (Notify) aos dispositivos |
| `uacpi/source/opcodes.c` | tabela de códigos AML e sua análise |
| `uacpi/source/opregion.c` | regiões de operação: acesso ao hardware a partir dos métodos AML |
| `uacpi/source/osi.c` | resposta a _OSI (que sistema o kernel diz ser) |
| `uacpi/source/registers.c` | leitura e gravação dos registradores de hardware do ACPI |
| `uacpi/source/resources.c` | recursos ACPI (_CRS, _PRS): IRQ, E/S e memória |
| `uacpi/source/sleep.c` | estados de suspensão e desligamento (S5) |
| `uacpi/source/stdlib.c` | funções de libc mínimas usadas pelo uACPI |
| `uacpi/source/tables.c` | tabelas ACPI: busca e instalação (DSDT, SSDT, FADT...) |
| `uacpi/source/types.c` | objetos do interpretador: inteiros, buffers, cadeias e pacotes |
| `uacpi/source/uacpi.c` | inicialização, carregamento de tabelas e desligamento do uACPI |
| `uacpi/source/utilities.c` | utilidades: nomes ACPI, ids EISA e avaliação de métodos |


---

## 7. Onde ler mais

| Tema | Documento |
| --- | --- |
| Arquitetura por camadas e o que há em cada uma | [`docs/arquitecturas.md`](arquitecturas.md) (processadores) e o [README](../README_PT.md) |
| Armazenamento: raiz em disco, live e "tudo na RAM" (D379) | [`docs/almacenamiento.md`](almacenamiento.md) |
| Inicialização com UEFI | [`docs/uefi.md`](uefi.md) |
| Vários processadores e threads | [`docs/multihilo.md`](multihilo.md) |
| Enlace dinâmico e bibliotecas compartilhadas | [`docs/enlace-dinamico.md`](enlace-dinamico.md) |
| Placas gráficas e 3D pela GPU | [`docs/gpus.md`](gpus.md), [`docs/gpu3d.md`](gpu3d.md) |
| Código de terceiros e licenças | [`docs/terceros.md`](terceros.md), [`docs/licencia.md`](licencia.md) |
| Todas as decisões de projeto (D001 a D4xx) | [`docs/decisoes.md`](decisoes.md) |
| Como se compila e se testa | [`docs/make.md`](make.md) e o README |

---

> *Este guia foi gerado a partir do código em 2026-11-21; se algo não bater, confie no código.*
