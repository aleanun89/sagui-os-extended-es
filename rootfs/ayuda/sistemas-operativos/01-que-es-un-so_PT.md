<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 1; titulo: O que é um sistema operacional -->
# O que é um sistema operacional

[Español](01-que-es-un-so.md) · [Português](01-que-es-un-so_PT.md) · [English](01-que-es-un-so_EN.md)

> Objetivo: entender o que faz um sistema operacional, por que ele se organiza em camadas e onde o SaguiOS se encaixa.

## 1. Teoria

### 1.1 Um programa que gerencia o hardware

Um computador é hardware: processador, memória, discos, rede, tela. Os programas de aplicação querem usar esse hardware
sem conhecer seus detalhes, e vários programas querem usá-lo ao mesmo tempo. O **sistema operacional** é o software que
faz de intermediário. Tem quatro funções clássicas:

- **Abstrair.** Oferece modelos mais simples que o hardware: um *processo* em vez de uma CPU, um *arquivo* em vez de setores
  de disco, um *socket* em vez de um controlador de rede.
- **Repartir.** Divide a CPU, a memória, o disco e a rede entre os programas, com regras de justiça, prioridade e cotas.
- **Proteger.** Impede que um programa sobrescreva a memória de outro, ou que um usuário leia os arquivos de outro sem permissão.
- **Coordenar.** Permite que os programas se comuniquem e se esperem (pipes, sinais, travas).

Esse conjunto se chama **núcleo** (*kernel*): a parte que roda com todos os privilégios do processador. O resto (shell, editor,
navegador) são programas comuns que pedem serviços a ele.

### 1.2 Kernel monolítico frente a microkernel

Há duas grandes famílias de projeto:

| Projeto | O que fica dentro do núcleo | Vantagens | Desvantagens | Exemplos |
| --- | --- | --- | --- | --- |
| **Monolítico** | Escalonador, memória, sistemas de arquivos, drivers e pilha de rede, tudo num mesmo espaço privilegiado. | As chamadas entre as partes são funções diretas: rápido. É mais fácil de depurar com um único binário. | Uma falha num driver pode derrubar todo o sistema. O código privilegiado é enorme. | Linux, FreeBSD, OpenBSD |
| **Microkernel** | Só o mínimo: processos, espaços de endereçamento, IPC. Drivers e sistemas de arquivos são processos de usuário. | Mais isolamento: um driver com defeito é reiniciado sozinho. O núcleo é pequeno e auditável. | Cada serviço exige mensagens entre processos: mais custo, a menos que seja bem projetado. | MINIX 3, QNX, L4 |

Na prática, quase todos os sistemas reais são **híbridos** ou monolíticos com módulos. O debate sobre qual é melhor diz respeito
à confiabilidade frente à velocidade, e a quanto código de terceiros se quer isolar.

### 1.3 Chamadas ao sistema e modo privilegiado

Um programa não toca o hardware diretamente. Pede serviços com **chamadas ao sistema** (*system calls*): `read`, `write`,
`fork`, `open`. A CPU impõe a fronteira com dois modos de execução (veremos no capítulo 3). Essa fronteira é o que torna possível
proteger o sistema.

### 1.4 Outros sistemas, para comparar

- **Linux**: monolítico com módulos carregáveis; todo o núcleo roda em modo privilegiado.
- **Windows NT**: núcleo híbrido, com camadas (HAL, executivo, gerenciador de E/S) e muitos serviços fora do núcleo.
- **BSD (FreeBSD, OpenBSD)**: monolíticos, com código e documentação muito legíveis; o OpenBSD prioriza a segurança.
- **MINIX 3**: microkernel com drivers como processos que se reiniciam sozinhos. É o exemplo clássico da disciplina.

### 1.5 Um cálculo simples: o custo de separar o núcleo dos serviços

Suponha que um serviço custe **0,8 µs** dentro de um núcleo monolítico (entrar, fazer o trabalho e sair). Em um microkernel, o mesmo
serviço exige uma mensagem ao servidor: uma troca de processo de ida e outra de volta, de 2 µs cada, mais a cópia da mensagem
(0,3 µs). Total: 2 × 2 + 0,3 = **4,3 µs**, cerca de cinco vezes mais. Os números são ilustrativos (é um exercício, não uma medição),
mas o raciocínio é real: o isolamento tem um preço medido em tempo, e os microkernels o reduzem evitando trocas de processo
desnecessárias.

### 1.6 O que entra no núcleo: comparação

| Sistema | Modelo | Onde ficam os drivers | Podem ser carregados em tempo de execução |
| --- | --- | --- | --- |
| Linux | monolítico com módulos | no núcleo, como módulos | sim |
| FreeBSD | monolítico com módulos do núcleo | no núcleo | sim |
| Windows NT | híbrido em camadas | em modo núcleo, com o modelo de controladores | sim |
| MINIX 3 | microkernel | processos de usuário | sim (são reiniciados) |
| SaguiOS | monolítico em uma única imagem | dentro de `kernel/dispositivos/`, compilados no núcleo | não é o objetivo do curso |

### 1.7 Outras estruturas: camadas, máquinas virtuais e cliente-servidor

Além do monolítico e do micronúcleo, há outras formas de organizar um sistema:

- **Camadas.** O sistema é dividido em níveis e cada um só usa os serviços do inferior (hardware, memória, E/S, programas de
  usuário...). Cada nível pode ser verificado separadamente e é o modelo das pilhas de rede (capítulo 10). O problema: as
  funções reais cruzam camadas (a memória virtual precisa do disco, e o disco precisa de memória) e atravessar muitas camadas
  custa tempo.
- **Máquinas virtuais.** Um **hipervisor** (o primeiro foi o VM/370, da IBM) multiplica uma máquina física em várias máquinas
  virtuais que se comportam como o hardware, e cada uma executa o seu próprio sistema operacional «convidado». QEMU e KVM são
  hoje a versão para PC. O SaguiOS é quase sempre testado como convidado do QEMU (com KVM ou com a CPU emulada do QEMU,
  chamada TCG). Isso condiciona o projeto: um convidado não sabe quando o hipervisor lhe tira a CPU virtual, e por isso a trava
  geral do kernel muda a forma de distribuir turnos se detecta um hipervisor (capítulo 7).
- **Cliente-servidor.** É a ideia do micronúcleo levada a todo o sistema: um cliente pede um serviço a um processo servidor com
  mensagens, e tanto faz o servidor estar na mesma máquina ou em outra (capítulo 10).

A estrutura decide o que pode ser isolado e quanto custa cada serviço. Nenhuma é a melhor em tudo, e os sistemas reais misturam várias.

### 1.8 Breve história: dos lotes ao PC

- **Lotes** (anos 50 e 60): um operador carrega trabalhos e a máquina os executa um de cada vez; a CPU espera enquanto uma fita é lida.
- **Multiprogramação**: vários trabalhos ao mesmo tempo na memória; enquanto um espera a E/S, a CPU atende outro (capítulos 5 e 6).
  Obriga a ter proteção de memória.
- **Tempo compartilhado**: muitos usuários interativos numa mesma máquina, com fatias curtas de CPU. Dali nasceu o Unix.
- **Computador pessoal**: sistemas pequenos de uma só tarefa (como o MS-DOS) que, com os anos, recuperaram a multitarefa, a
  memória virtual e a proteção dos grandes.

O SaguiOS é um sistema de PC da última etapa: multitarefa preemptiva, memória protegida, usuários e rede.

## 2. Como o SaguiOS faz

O SaguiOS-Extended é um **sistema educativo de 32 bits (i686)**. Seu núcleo é **monolítico**: o escalonador, a memória, os
sistemas de arquivos, os drivers e a pilha de rede são compilados numa única imagem que roda com privilégios de núcleo. A árvore
do projeto mostra isso: `kernel/nucleo/` (processos), `kernel/mem/` (memória), `kernel/fs/` (arquivos), `kernel/dispositivos/`
(drivers) e `kernel/rede/` (rede) são pastas do mesmo núcleo.

Alguns pontos concretos:

- **Um único espaço privilegiado.** O código de `kernel/fs/vfs.c` chama diretamente `kernel/fs/ext2.c` ou `kernel/fs/fat32.c`, sem
  mensagens. É a vantagem do monolítico que aparece no caminho de uma chamada (capítulo 3).
- **As bibliotecas de usuário são separadas.** A libc (`userland/libc/`) e a libsagui (`userland/libsagui/`) são código de programa,
  não do núcleo. Traduzem funções como `write` para a chamada `int 0x80` do capítulo 3. Desde a decisão **D462**, quase todos os
  programas de `/bin` as carregam como bibliotecas compartilhadas (`/lib/libsagui.so.1`, `/lib/libc.so.1`; capítulo 6), e só o
  essencial para iniciar e reparar o sistema é ligado de forma estática.
- **Como é construído.** O sistema é compilado no PC com `make` (GCC 13 ou GCC 15: a decisão **D464** corrigiu os poucos pontos
  que falhavam com o C23 do GCC 15) e testado como convidado do QEMU.
- **Código de outros projetos isolado.** Os drivers tomados de outros projetos ficam em `kernel/terceros/`, com a licença registrada
  (decisões D034 e D073). Isso isola o que não é do projeto, embora continue fazendo parte do núcleo.
- **O núcleo é documentado dentro do próprio sistema.** O guia do kernel está em `/ayuda/guias/guia-kernel.md`; o repositório o
  mantém em `docs/guia-kernel.md`.

**Onde fica cada coisa no repositório:**

| Parte | Pasta | Arquivo de exemplo |
| --- | --- | --- |
| Inicialização | `boot/` | `boot/mbr.asm` |
| Núcleo (processos, chamadas) | `kernel/nucleo/` | `kernel/nucleo/processos.c` |
| Memória | `kernel/mem/` | `kernel/mem/paginacao.c` |
| Sistemas de arquivos | `kernel/fs/` | `kernel/fs/vfs.c` |
| Drivers | `kernel/dispositivos/` | `kernel/dispositivos/ahci.c` |
| Rede | `kernel/rede/` | `kernel/rede/tcp.c` |
| Biblioteca C e gráfica de usuário | `userland/libc/`, `userland/libsagui/` | `userland/libc/posix.c` |
| Programas | `userland/programas/` | `userland/programas/ps.c` |

> **Decisão relevante.** D157, D164, D165 e D169 (SMP): o núcleo continua sendo um único programa e, com várias CPUs, cada uma
> atende as suas próprias chamadas ao sistema, mas elas **entram no núcleo uma de cada vez**, sob uma trava geral que distribui
> os turnos por ordem de chegada. Veremos isso nos capítulos 5 e 7.

### 2.1 Decisões, alternativas e limites atuais

- **Decisão: núcleo monolítico em um único binário de 32 bits.** Permite ler o sistema inteiro, seguir uma chamada do início ao fim
  (capítulo 3) e não pagar o custo das mensagens em máquinas antigas. O guia do kernel explica isso desde a sua primeira seção
  (`docs/guia-kernel.md`).
- **Limite: só i686.** Os modos de 64 bits não são usados (guia do kernel, seção 1). Estender para 64 bits está no roteiro, não nesta versão.
- **Limite: uma falha num driver afeta tudo.** O SaguiOS atenua falhas graves com uma tela de exceção (`kernel/arch/excecoes.c`), não
  com isolamento de processos. O que ele isola é o código de outros projetos, mas por **licença** (decisões D034 e D073), não por privilégio.
- **Limite: o núcleo foi escrito para um processador.** A decisão **D165** coloca uma trava geral: cada CPU atende as suas
  chamadas, mas só uma está dentro do núcleo de cada vez (capítulo 7). É mais simples e seguro, ao custo de escalar.

## 3. Experimento no SaguiOS

1. Abra o **Terminal** (Início > Acessórios) e digite `ps`. Você verá os processos com o PID, o estado e a memória. Cada linha é um
   **processo** da teoria.
2. Digite `uname` e `uptime`. São programas de usuário que pedem dados ao núcleo com chamadas ao sistema.
3. Abra o **Administrador de tarefas** (Início > Sistema > Administrador de tarefas). Compare o que ele mostra com `ps`.
4. Digite `ayuda ps` para ver a documentação do programa dentro do próprio sistema.

![Terminal com o comando ps mostrando processos do sistema](img/cap-terminal-ps.png)

**O que observar:** os processos do núcleo aparecem na lista ao lado dos programas de usuário, porque ambos fazem parte do mesmo
sistema, mas os do núcleo não ocupam memória de usuário.

**Exercício prático adicional.** No repositório (no PC), execute `du -sh kernel/*` e ordene as pastas por tamanho. Compare
`kernel/dispositivos/` com `kernel/nucleo/`. Explique por que os drivers ocupam tanto espaço em um núcleo monolítico e o que isso
significa para a confiabilidade do sistema.

## 4. Perguntas de revisão

1. Qual a diferença entre o **núcleo** e um programa de usuário? Dê um exemplo de cada um no SaguiOS.
2. Um driver de som falha e provoca uma exceção no núcleo. O que acontece num monolítico e num microkernel?
3. Por que um sistema pode ser monolítico e ainda ter partes isoladas, como `kernel/terceros/`?
4. Cite duas funções que um interpretador de comandos como o `sagsh` cumpre (não o núcleo).
5. Com os números da seção 1.5, qual custo por troca de processo faria a chamada por mensagens custar o mesmo que a monolítica (0,8 µs)? É realista?
6. Um driver de disco dentro do núcleo corrompe a memória usada pelo escalonador. Liste as consequências num projeto monolítico e num microkernel.
7. Um hipervisor divide uma máquina entre dois sistemas convidados. Indique duas vantagens e dois inconvenientes em relação a instalar
   cada sistema na sua própria máquina.
8. Por que a multiprogramação precisa de proteção de memória e um sistema que executa um só programa por vez não?

## 5. Respostas

1. O núcleo roda com privilégios de hardware e gerencia recursos (por exemplo `kernel/nucleo/processos.c`). Um programa de usuário
   (por exemplo `ps`, em `userland/programas/ps.c`) apenas pede serviços.
2. Num monolítico, a falha ocorre no espaço privilegiado e pode derrubar todo o sistema. Num microkernel, o driver é um processo: pode
   ser reiniciado sem parar o resto.
3. O isolamento pode ser de **código e de licença**, não só de privilégio: `kernel/terceros/` separa o código alheio para gerenciar
   a sua licença, embora continue compilado dentro do núcleo.
4. Por exemplo: executar comandos em sequência e com pipes (usa `fork`, `execve` e `pipe`), e oferecer um ambiente de trabalho com
   variáveis e redirecionamentos. O interpretador usa as mesmas chamadas que qualquer programa.
5. Basta igualar 2c + 0,3 = 0,8, logo c = 0,25 µs por troca de processo. Com trocas de 2 µs é impossível igualar: a mensagem sempre
   será mais cara, a menos que se evitem trocas de processo. Por isso os microkernels modernos reduzem as trocas e copiam menos.
6. No monolítico, o driver e o escalonador compartilham o espaço: o sistema inteiro pode cair ou se comportar de forma incorreta. No
   microkernel, o driver é um processo: a falha não toca o escalonador (se o núcleo mínimo estiver correto) e o serviço pode ser reiniciado.
7. Vantagens: aproveita-se melhor o hardware (várias máquinas lógicas em uma física) e é fácil testar, copiar ou restaurar um
   sistema sem mexer no resto (assim se testa o SaguiOS no QEMU). Inconvenientes: há uma camada a mais que custa desempenho (as
   instruções privilegiadas do convidado são atendidas pelo hipervisor) e o convidado não controla quando a sua CPU virtual roda.
8. Com um só programa não há quem prejudicar além do próprio sistema; com vários, um erro de um poderia escrever na memória de outro
   ou do núcleo. É preciso hardware que o impeça (modo núcleo e paginação; capítulos 3 e 6).
