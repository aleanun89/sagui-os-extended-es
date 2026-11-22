<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 92; titulo: Anexo: assembler x86 -->
# Anexo: assembler x86 no SaguiOS

O assembler é a linguagem mais próxima da máquina: cada instrução é uma operação que o processador executa
diretamente. Este anexo explica o que é preciso para entender como o SaguiOS inicializa, como passa de um modo de
trabalho do processador para outro e como o kernel e os programas dividem o trabalho. Ele segue a ordem histórica
da arquitetura x86: primeiro o modo real de 16 bits, depois o modo protegido de 32 bits, depois uma visão geral dos
64 bits. Os 16 e 32 bits são explicados em detalhe, com referências aos arquivos reais do sistema; os 64 bits, apenas
como conceitos.

As referências têm a forma `arquivo:linha`, com o caminho a partir da raiz do repositório. As linhas podem ser
conferidas abrindo o arquivo no PC.

## 1. O que é o assembler e por que um sistema operacional precisa dele

Um programa em C ou em Pascal é traduzido em instruções de máquina por um compilador. O assembler faz o mesmo com um
passo quase um a um: cada linha é um **mnemônico** (`mov`, `cli`, `int`) com seus operandos, e vira um ou vários
bytes. O assembler é próprio de cada família de processadores. As instruções x86 de 32 bits rodam igual num Pentium II
e num Athlon ou num i7 de 7.ª geração, e essa é a base de o SaguiOS funcionar em máquinas tão diferentes.

Um sistema operacional precisa de assembler (ou, na prática, de algumas linhas de assembler) porque há coisas que a
linguagem C não consegue expressar:

| O quê | Por que o C não basta | Onde aparece no SaguiOS |
|-------|-----------------------|-------------------------|
| **Inicialização** | Ao ligar, a CPU está em modo real e não há pilha nem C | `boot/mbr.asm`, `boot/estagio2.asm` |
| **Troca de modo** | É preciso carregar as tabelas do processador (GDT) e escrever nos registradores de controle (CR0) | `boot/estagio2.asm:294-300` |
| **Interrupções** | O processador empilha um quadro concreto e o código tem de voltar com `iret` | `kernel/arch/vetores.asm` |
| **Troca de contexto** | Trocar de pilha e de tabelas de página entre processos | `kernel/arch/vetores.asm:51-53`, `kernel/mem/paginacao.c:490-492` |
| **Instruções privilegiadas** | Só o kernel pode usar `cli`, `lgdt` ou escrever em CR3 | `kernel/inc/cpu.h:81-82`, `kernel/arch/anel3_testes.asm:46` |

### Anéis de privilégio

O processador x86 tem quatro níveis de privilégio, chamados **anéis** (0 a 3). O SaguiOS usa apenas dois: o anel 0 para
o kernel e o anel 3 para os programas de usuário. O nível de privilégio atual (CPL) fica no seletor do segmento de
código. No anel 3, as instruções que mudam o estado global da máquina (`cli`, `lgdt`, escrever em CR0 ou CR3) geram uma
exceção de proteção geral (#GP), e o kernel encerra o programa que a provocou. Assim um programa não consegue desligar
as interrupções nem ler a memória do kernel.

## 2. Modo real de 16 bits

Quando o processador é ligado, ele está em **modo real**, o modo do 8086 de 1978. É o modo que a BIOS usa e no qual
se vê o primeiro setor do disco.

### Registradores

- Quatro registradores gerais de 16 bits que podem ser divididos em duas metades de 8 bits: **AX** (AH e AL), **BX**,
  **CX** e **DX**.
- Dois registradores de índice e dois de ponteiro: **SI**, **DI**, **BP** e **SP**. SP é o ponteiro da pilha.
- O contador de programa **IP**, e os indicadores (**FLAGS**), com bits como CF (vai-um, que a BIOS usa para sinalizar
  erros) ou IF (interrupções habilitadas).
- Quatro **registradores de segmento**: **CS** (código), **DS** (dados), **SS** (pilha) e **ES** (dados extras).

### Endereçamento segmento:deslocamento

Um registrador de 16 bits alcança só 64 KB. Para chegar a 1 MB, o processador soma dois valores:

    dirección física = segmento × 16 + desplazamiento

Por exemplo, `0x07C0:0x0005` é `0x07C0 × 16 + 0x0005 = 0x7C05`. Muitas combinações de segmento e deslocamento apontam
para o mesmo endereço físico, e é por isso que o código de inicialização zera CS assim que pode (veja `boot/mbr.asm:39`:
o salto longo `jmp 0x0000:.cs_zerado` deixa CS = 0 para que os endereços do `org 0x7C00` valham sempre).

Com 20 linhas de endereço, os endereços acima de 1 MB "dão a volta" e voltam a zero no 8086. As máquinas modernas têm
uma porta chamada **linha A20** que a pode ativar ou desativar; o SaguiOS a ativa no estágio 2 (`boot/estagio2.asm`,
seção `[1/6] A20`, linha 132, e o método pela porta 0x92 nas linhas 413-418).

### A BIOS e suas interrupções

A BIOS oferece serviços de baixo nível chamados com `int n`. São o único "sistema operacional" disponível antes do
kernel. O SaguiOS usa, entre outras:

| Interrupção | Função (AH) | Uso no SaguiOS |
|-------------|-------------|----------------|
| `int 0x10` | `0x0E`: escrever um caractere em modo teletipo | Mensagens do carregador (`boot/mbr.asm:112`) |
| `int 0x13` | `0x41`: há extensões LBA? | Escolher como ler o disco (`boot/mbr.asm:52-55`) |
| `int 0x13` | `0x08`: geometria do disco | Leitura CHS se não há LBA (`boot/mbr.asm:60-64`) |
| `int 0x13` | `0x42`: ler setores por LBA | Ler o estágio 2 e o kernel (`boot/mbr.asm:150-152`) |
| `int 0x15` | `0xE820`: mapa de memória | Quanta RAM há (`boot/estagio2.asm:508-512`) |

As interrupções da BIOS são chamadas em modo real e deixam os resultados nos registradores (o vai-um CF indica erro).
Quando o sistema passa ao modo protegido, já não há BIOS: por isso o carregador faz todas as chamadas antes do salto.

### O MBR: 512 bytes e a assinatura 0x55AA

Ao iniciar a partir de um disco, a BIOS lê o **primeiro setor** (512 bytes) do disco no endereço `0x7C00`, verifica que
seus dois últimos bytes são `0x55 0xAA` e salta para ele. Esse setor é o **MBR** (*Master Boot Record*). Como cabem
apenas 512 bytes, o MBR só pode fazer uma coisa: carregar o estágio seguinte, que não tem esse limite.

O arquivo `boot/mbr.asm` o descreve no seu cabeçalho (linhas 10-17):

- `0x000-0x002`: salto curto e NOP, o formato que algumas BIOS esperam.
- `0x003-0x059`: área do BPB (a geometria do disco de um FAT) zerada. Algumas BIOS escrevem a geometria aqui ao iniciar
  pelo USB, por isso não há código nessa zona.
- `0x05A-0x1B7`: código e mensagens.
- `0x1BE-0x1FD`: tabela de partições, preenchida por `tools/mkimagem.py`.
- `0x1FE-0x1FF`: a assinatura. No arquivo é `dw 0xAA55` (linha 191), e como o x86 é little-endian, os bytes `55 AA`
  aparecem no disco.

Os passos do MBR do SaguiOS, em ordem:

1. **Estado conhecido** (`boot/mbr.asm:31-36`): desativar as interrupções, pôr DS, ES e SS em zero e a pilha em
   `0x7C00`, que cresce para baixo, na memória baixa livre.

        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

2. **Lembrar o disco**: a BIOS deixa em DL o número do disco de inicialização; o MBR o guarda em `disco`
   (`boot/mbr.asm:43`) porque todas as leituras e o estágio 2 vão precisar dele.

3. **Escolher o modo de leitura**: pergunta-se à BIOS se ela entende as extensões LBA (`AH=0x41`, linha 55). Se não,
   pede-se a geometria (`AH=0x08`, linhas 60-64) e a leitura será feita por cilindro, cabeça e setor.

4. **Ler o cabeçalho de inicialização** do setor LBA 1 e verificar sua assinatura (linhas 76-79). O cabeçalho diz em
   qual setor começa o estágio 2 e quantos setores ele ocupa.

5. **Ler o estágio 2 em `0x8000`** (linhas 82-91) e saltar para ele com o disco em DL (linhas 93-94):

        mov dl, [disco]                 ; la etapa 2 recibe el disco en DL, como nosotros
        jmp 0x0000:ESTAGIO2_ENDERECO

Se algo falhar, o MBR escreve uma mensagem com a BIOS e para num `hlt` (o rótulo `parar`, em `boot/mbr.asm:165-169`).
A rotina que escreve esse texto (`escrever`, `boot/mbr.asm:106-115`) percorre a cadeia byte a byte com `lodsb` e chama
`int 0x10` para cada caractere:

    escrever:
            lodsb                       ; AL = [DS:SI], SI se incrementa
            test al, al
            jz .fim
            mov ah, 0x0E
            mov bx, 0x0007
            int 0x10
            jmp escrever

### O estágio 2 em modo real

O `boot/estagio2.asm` não tem limite de 512 bytes, e faz em modo real tudo o que a BIOS ainda permite: verifica a CPU
(`conferir_cpu`, que exige CPUID e CMOV), ativa A20, lê o mapa de memória com `int 0x15`, carrega o kernel e o ramdisk
acima de 1 MB e escolhe o modo de vídeo VESA com `int 0x10`. Para copiar acima de 1 MB sem sair do modo real, usa o
truque do **modo irreal** (`entrar_unreal`, linha 1161): entra no modo protegido só por um instante para carregar em DS
e ES um descritor de 4 GB, e volta ao modo real. O limite do descritor fica na cache do processador e os endereços de
32 bits continuam funcionando.

## 3. Passagem ao modo protegido de 32 bits

O **modo protegido** é o modo de trabalho do SaguiOS. Mudam várias coisas ao mesmo tempo:

- Os registradores passam a ter 32 bits. Os gerais são **EAX, EBX, ECX, EDX, ESI, EDI, EBP** e **ESP** (a pilha). O
  contador de programa é **EIP** e os indicadores são **EFLAGS** (IF continua no bit 9).
- O endereço de um segmento deixa de ser `segmento × 16`. Agora o seletor é um índice numa tabela de **descritores**, e
  cada descritor diz base, limite, tipo e **nível de privilégio** (DPL).
- Podem-se usar **interrupções** protegidas (uma tabela IDT com portas que dizem qual anel pode chamá-las) e
  **paginação** (a memória virtual).

### A GDT

A **tabela global de descritores** (GDT) é uma lista de descritores de 8 bytes. Um descritor de código ou de dados tem:

- a **base** (32 bits) e o **limite** (20 bits, em bytes ou em páginas de 4 KB);
- o **byte de acesso**: bit de presente, DPL (dois bits, anel 0 a 3), tipo (código ou dados, leitura ou escrita);
- os **bits de granularidade**: o de 4 KB e o modo de 32 bits.

O carregador define uma GDT mínima com um descritor nulo (obrigatório), código e dados de 32 bits com base 0 e limite de
4 GB (`boot/estagio2.asm:1787-1795`):

    gdt:
            dq 0                            ; descriptor nulo (obligatorio)
            dw 0xFFFF, 0x0000               ; 0x08 código: límite 0-15, base 0-15
            db 0x00, 0x9A, 0xCF, 0x00       ;      base 16-23, presente/anillo 0/código, 4 KB + 32 bits, base 24-31
            dw 0xFFFF, 0x0000               ; 0x10 datos
            db 0x00, 0x92, 0xCF, 0x00

Os seletores são deslocamentos dentro da tabela: `0x08` é o código do anel 0 e `0x10` os dados. Com base 0 e limite de
4 GB, o **modelo plano** equivale a não ter segmentação: todos os endereços são lineares. É assim que o kernel funciona,
e por isso o código C não precisa pensar em segmentos.

### O salto ao modo protegido

A passagem é feita com seis instruções, em `boot/estagio2.asm:294-300`:

    .entrar_pm:
            cli                             ; ninguna interrupción de la BIOS durante el cambio
            lgdt [gdt_descritor]            ; carga la GDT (dirección y tamaño)
            mov eax, cr0
            or eax, 1                       ; bit 0 = PE, protección activada
            mov cr0, eax
            jmp SEL_CODIGO:modo_protegido   ; salto largo: recarga CS con el selector de 32 bits

Duas coisas merecem atenção:

- **O salto longo é obrigatório.** Ao ativar PE, o processador ainda tem na cache o CS do modo real. Um salto distante com
  o seletor `0x08` recarrega CS com o descritor de código de 32 bits e esvazia a fila de instruções que o processador já
  tinha lido no modo real.
- **Os demais registradores de segmento são recarregados depois.** `modo_protegido` (`boot/estagio2.asm:1735`) carrega DS,
  ES, FS, GS e SS com `0x10` e põe a pilha provisória em `0x7C00` (linhas 1736-1742). Até essa carga, DS e SS ainda têm
  valores do modo real.

### A linha A20 e o mapa de memória

A linha A20 não é um detalhe do modo protegido: sem ela, os endereços a partir de 1 MB dão a volta. O carregador a ativa
antes do salto (seção `[1/6]`, linha 132), e se não for possível ativá-la, para com uma mensagem (`msg_falha_a20`). O
mapa de memória E820 diz ao kernel quais intervalos são RAM utilizável e quais estão reservados para a BIOS ou o hardware.

### Do carregador ao kernel

`kernel/arch/entrada.asm` é a primeira instrução do kernel. Recebe em EAX a assinatura do carregador e em EBX o endereço
de `boot_info`, e antes de qualquer código C faz três coisas:

1. Zera a seção BSS, porque o binário plano não a contém e o código C supõe variáveis em zero (linhas 17-24: `rep stosb`).
2. Passa para uma pilha própria, dentro da BSS (linha 25).
3. Chama `inicio_kernel` com os dois valores como argumentos (linhas 27-29).

### A convenção de chamada cdecl

No modo protegido de 32 bits, o SaguiOS (como a maior parte do C da época) usa a convenção **cdecl**:

- Os argumentos são empilhados da direita para a esquerda. Por isso `push edx; push esi; call inicio_kernel`
  (`kernel/arch/entrada.asm:27-29`) deixa `boot_info` e a assinatura como segundo e primeiro argumento.
- O valor de retorno vai em **EAX** (e em EDX:EAX se for de 64 bits).
- Quem chama **limpa a pilha** depois da chamada (`add esp, 4` em `kernel/arch/vetores.asm:60`).
- EBX, ESI, EDI, EBP e ESP são preservados pela função chamada; EAX, ECX e EDX podem mudar.

O quadro de pilha de uma função típica fica assim (texto ilustrativo, não é código para montar):

```text
  [ebp+12]   segundo argumento
  [ebp+8]    primer argumento
  [ebp+4]    dirección de retorno (la mete el "call")
  [ebp]      EBP anterior (la función lo guarda con "push ebp")
  [ebp-4]    primera variable local
```

O compilador de C gera isso sozinho. O que importa para ler o assembler do SaguiOS é que o quadro de cada interrupção
tem um formato fixo que o código C conhece (veja a próxima seção).

### Interrupções e exceções: a IDT

A **tabela de descritores de interrupção** (IDT) tem 256 entradas, uma por vetor. Cada entrada diz o endereço do
tratador, o seletor de código e o **DPL**, que decide se um programa do anel 3 pode chamá-la com `int n`:

- `kernel/arch/idt.c:70` põe todas as entradas com `0x8E`: porta de interrupção de 32 bits, DPL 0. Um programa de usuário
  que execute `int 0x20`, por exemplo, recebe #GP.
- `kernel/arch/idt.c:72` põe a chamada ao sistema, `VETOR_SISTEMA` (`kernel/inc/cpu.h:27`, valor `0x80`), com `0xEE`:
  DPL 3, para que o anel 3 possa usá-la.

Ao receber uma interrupção, o processador empilha o quadro que o código de entrada deve respeitar, e se houve troca de
anel também o ESP e o SS do usuário. O C o vê como uma estrutura (`quadro_t`, em `kernel/inc/cpu.h:36-42`). Como nem
toda exceção empilha um código de erro, cada vetor tem seu próprio trecho de entrada que faz o quadro ter sempre o
mesmo tamanho:

- `kernel/arch/vetores.asm:27-36` é um laço `%rep` de 256 voltas que empilha um 0 se o processador não empilhou código de
  erro (a condição `%if`, linhas 30-32), empilha o número do vetor e salta para o trecho comum.
- `entrada_comum` (linhas 38-59) guarda todos os registradores com `pushad`, carrega os segmentos do kernel e chama
  `tratar_interrupcao` (`kernel/arch/idt.c:297`).
- `retomar_quadro` (linhas 63-70) restaura os registradores e termina com `iretd`, que desempilha EIP, CS e EFLAGS e, se
  o quadro for de outro anel, também ESP e SS.

### Chamadas ao sistema com int 0x80

Um programa de usuário pede serviços ao kernel com `int 0x80`. O número da chamada vai em EAX e os argumentos em EBX,
ECX, EDX (e ESI, EDI); o comentário de `kernel/inc/abi.h:5` o explica. Um exemplo do SaguiOS
(`kernel/arch/anel3_testes.asm:19-27`) escreve um texto com a chamada de escrita:

    mov eax, SIS_ESCREVER               ; número de la llamada
    mov ebx, 1                          ; descriptor 1 (la salida)
    lea ecx, [esi + (.msg - .aqui)]     ; dirección del texto
    mov edx, .msg_fim - .msg            ; longitud
    int 0x80                            ; el kernel toma el control

O mesmo arquivo testa a proteção (linhas 42-50): `cli` no anel 3 faz o kernel encerrar o processo, e uma tentativa de
ler `0x00100000` (memória do kernel, `anel3_testes.asm:55-59`) provoca uma falha de página, porque as páginas do kernel
não têm o bit de usuário.

### Anel 3: como um programa começa

`processo_preparar_usuario` (`kernel/nucleo/processos.c:714-728`) prepara o quadro de um programa novo como se viesse de
uma interrupção do anel 3. Põe os seletores de usuário (`SEL_UCODIGO` 0x1B e `SEL_UDADOS` 0x23, `kernel/inc/cpu.h:11-12`),
a pilha do programa e o ponto de entrada. O EFLAGS é `0x202`: IF ligado e IOPL 0, de modo que o programa não pode usar
`cli`, `sti` nem as portas de E/S. Depois, `retomar_quadro` executa o `iretd` e a CPU passa ao anel 3 sem nenhuma
instrução especial de "baixar o privilégio".

### Paginação: CR3, CR0 e CR4

A paginação dá a cada processo o seu próprio espaço de endereços. A CPU a controla com três registradores:

- **CR3** aponta para a tabela de diretório (as tabelas de páginas). Mudar CR3 muda o espaço de endereços. É o que faz
  `espaco_ativar` (`kernel/mem/paginacao.c:490-492`): `mov %0, %%cr3`.
- **CR0.PG** (bit 31) liga a paginação. O carregador do SaguiOS a liga nos processadores secundários
  (`kernel/arch/ap_trampolim.asm:145`, `mov cr0, eax`, com a tabela já em CR3 da linha 136).
- **CR4** ativa extensões: PSE (páginas de 4 MB, que o mapa do kernel usa), PAE (entradas de 64 bits) e PGE (páginas
  globais). `ap_trampolim.asm:97` copia o CR4 do processador principal antes de ligar a paginação, porque a ordem importa.

A troca de contexto no kernel reúne tudo isso: quando `tratar_interrupcao` devolve o quadro de outro processo,
`entrada_comum` carrega a pilha dele (`mov esp, eax`, `vetores.asm:53`). Ao trocar de processo, o núcleo chama
`espaco_ativar` com o espaço do processo novo (`kernel/nucleo/processos.c:603`), ou seja, carrega CR3. Assim uma
interrupção pode terminar em outro programa, com outra memória.

### Processadores secundários: de 16 a 32 bits outra vez

O modo SMP (opcional, `mkimagem.py --smp`) acende os processadores secundários com uma sequência INIT-SIPI. Cada
secundário começa em modo real numa página que o principal copiou para `0x7000`, e o trampolim repete o caminho do
carregador: GDT local, CR0.PE, salto longo para o código de 32 bits e, depois, a paginação. É um bom exemplo de que a
troca de modo se repete em cada CPU:

    protegido:                          ; kernel/arch/ap_trampolim.asm:80, ya en modo de 32 bits
            mov ax, 0x10                ; (extracto: se omiten los comentarios de las líneas 81-83)
            mov ds, ax
            mov es, ax
            mov fs, ax
            mov gs, ax
            mov ss, ax

O cabeçalho do arquivo diz que o trampolim faz o mínimo para chegar ao C: a GDT, a TSS e a IDT de cada CPU são preparadas
depois, em C, com a paginação já ligada (`kernel/arch/ap_trampolim.asm:16-21`).

### O carregador UEFI: de 64 para 32 bits

O carregador UEFI x64 (`boot/uefi/trampolim.asm`) faz o caminho contrário: o firmware deixa a CPU em modo longo, e o
trampolim desce para 32 bits para o kernel. Tem duas entradas, `entrada64` (linha 48) e `entrada32` (linha 73). Na de
64 bits, o código carrega uma GDT própria, faz um retorno distante (`o64 retf`, linha 57) para entrar no segmento de
compatibilidade, desliga a paginação (CR0, linha 65) e limpa os bits LME e NXE do EFER (linha 68). A seção 6 explica os
64 bits.

## 4. Assembler em linha no C

Muitas partes do kernel não podem ser escritas em C, mas há instruções isoladas que vale a pena usar a partir do C: ler
uma porta, consultar o CPUID, mudar as interrupções. O compilador permite escrever assembler **em linha** com
`__asm__ volatile("...")`.

### Duas sintaxes

O assembler do SaguiOS é escrito de duas formas, conforme o compilador:

| | Intel (NASM, usado nos exercícios deste anexo) | AT&T (GCC e TCC em linha) |
|--|---------------------------------------------|---------------------------|
| Ordem | destino, origem: `mov eax, ebx` | origem, destino: `movl %ebx, %eax` |
| Registradores | sem `%`: `eax` | com `%`: `%eax` |
| Constantes | sem cifrão: `mov eax, 5` | com `$`: `movl $5, %eax` |
| Tamanho | implícito ou `dword` | sufixo: `movl`, `movw`, `movb` |

O C do SaguiOS (kernel e programas) é compilado pelo GCC no PC, e o kernel também contém arquivos `.asm` em NASM. Por
isso aparecem duas sintaxes no mesmo código: os blocos `__asm__` estão em AT&T e os arquivos `.asm` em Intel.

### Restrições dos operandos

Em `__asm__`, os operandos do C se ligam aos registradores com restrições. Um exemplo de `kernel/inc/portas.h`:

    static inline uint8_t porta_le8(uint16_t porta)
    {
        uint8_t v;
        __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(porta));
        return v;
    }

Quer dizer: a saída `v` vai em AL (`"=a"`); a entrada `porta` vai em DX ou é uma constante de 8 bits (`"Nd"`). Assim o
`inb` lê a porta indicada e deixa o valor em `v`. `porta_escreve8` (linhas 7-9) faz o mesmo com `outb`. `volatile` impede
que o compilador remova ou mova a instrução; se a instrução também tocar a memória, acrescenta-se `"memory"` para que o
compilador não reordene os acessos.

### Exemplos do SaguiOS

| Instrução | O que faz | Onde se usa |
|-----------|-----------|-------------|
| `cpuid` | Consulta as capacidades da CPU (folha em EAX) | `kernel/arch/msr.c:105-107`, `kernel/dispositivos/sensores.c:74`, `userland/programas/simd.c:26` |
| `rdtsc` | Lê o contador de ciclos (TSC) em EDX:EAX | `kernel/dispositivos/ahci.c:167`, `kernel/dispositivos/sensores.c:80` |
| `in` / `out` | Ler e escrever uma porta de E/S | `kernel/inc/portas.h:7-15` (`outb`, `inb`) |
| `cli` / `sti` | Desligar e ligar as interrupções | `kernel/inc/cpu.h:81-82` (`int_liga`, `int_desliga`) |
| `cli; hlt` | Parar a CPU para sempre | `kernel/arch/excecoes.c:34` (após uma falha fatal) |
| `sti; nop; nop; cli` | Deixar entrar uma interrupção numa janela específica | `kernel/dispositivos/ata.c:594` |
| `rep insw` | Ler um bloco de palavras de uma porta | `kernel/dispositivos/ata.c:317` |
| `wbinvd` | Escrever e esvaziar a cache da CPU | `kernel/dispositivos/acel_radeon.c:825` |

Dois exemplos merecem atenção. O `cpuid` tem muitas folhas, e o resultado vem em quatro registradores: `sensores.c:72-74`
o envolve numa função com quatro saídas. `ata.c:594` mostra um truque: a instrução que segue o `sti` executa antes de
uma interrupção ser aceita, então o `nop` deixa uma janela para a interrupção entrar antes do próximo `cli`.

## 5. SIMD: MMX, SSE e AVX em conceito

**SIMD** (*Single Instruction, Multiple Data*) significa que uma instrução opera sobre vários dados ao mesmo tempo,
empacotados num registrador grande. Exemplos:

- **MMX** (anos noventa): registradores de 64 bits (MM0 a MM7) que compartilham o espaço da FPU x87. Somar quatro
  inteiros de 16 bits numa só instrução.
- **SSE** (Pentium III, 1999): registradores de 128 bits (XMM0 a XMM7 em 32 bits, XMM0 a XMM15 em 64 bits). Operações de
  ponto flutuante em 4 números de 32 bits de cada vez. **SSE2** (Pentium 4, 2001) acrescenta inteiros de 128 bits e
  números de dupla precisão.
- **AVX** (Sandy Bridge, 2011): registradores de 256 bits (YMM), com um prefixo de codificação novo (VEX). É o que o
  estado da FPU do SaguiOS usa quando a CPU o tem (veja mais abaixo).

Para usá-los, o programa primeiro pergunta à CPU com `cpuid`. O `userland/programas/simd.c` define os bits que ele
verifica: MMX no bit 23 de EDX da folha 1 (`R_MMX`, linha 19), FXSR no bit 24 (`R_FXSR`, linha 20), SSE no bit 25 e SSE2
no bit 26 (linhas 21-22). O mesmo programa testa cada conjunto com uma instrução sua, em assembler em linha:

- x87 com `fsqrt` (linha 42);
- MMX com `paddw` e `emms` (linha 52), que esvazia o estado MMX;
- SSE com `movaps` e `mulps` (linha 64);
- a configuração de SSE com `ldmxcsr` (linha 138).

### Onde o SaguiOS usa SIMD

- **Cópias e preenchimentos grandes na interface gráfica**: `kernel/lib/sse2.c` (uma cópia de 16 bytes por instrução;
  o comentário do cabeçalho explica por que o destino é alinhado). O exemplo do acelerador é `kernel/dispositivos/gpu3d_r600.c:1193`,
  que lê a janela da placa de 16 em 16 bytes: essa janela é memória sem cache (`kernel/dispositivos/gpu3d_evergreen.c:775`)
  e cada leitura é lenta. A função `sse2_copiar` está declarada em `kernel/inc/sse2.h:13`.
- **O estado da FPU e do SSE por processo**: cada processo tem seus próprios registradores x87, MMX e XMM, e o kernel não
  os salva a cada troca de processo porque isso seria caro. Em vez disso, a troca é **preguiçosa**
  (`kernel/arch/fpu.c:9-20`): o escalonador ativa CR0.TS e a primeira instrução de ponto flutuante do processo novo
  provoca a exceção 7 (#NM); o tratador salva o estado do dono anterior e carrega o do novo.
- **Ativar o SSE**: `kernel/arch/fpu.c:295` põe CR4.OSFXSR, sem o qual o processador gera #UD em qualquer instrução SSE,
  e `fpu.c:296` acrescenta CR4.OSXMMEXCPT (erros de SSE vão para a exceção 19). O estado é salvo com `fxsave`
  (`kernel/arch/fpu.c:107`), ou com `xsave` se houver AVX (D381: 832 bytes, os YMM inteiros, `kernel/arch/fpu.c:28-33`).
- **Regras do kernel**: o kernel é compilado com `-mgeneral-regs-only` (`kernel/arch/fpu.c:18-19`), então o C normal não
  usa XMM; o `sse2.c` usa, e por isso só é chamado entre `fpu_kernel_inicio()` e `fpu_kernel_fim()`, que salvam e
  restauram o estado do processo dono (`kernel/lib/sse2.c` diz isso no cabeçalho, linhas 5-10).

Por isso o estado da FPU e do SSE pertence a cada processo: se o kernel não o salvasse, um programa veria os registradores
de outro.

## 6. 64 bits (x86-64): apenas conceitos

A versão 10 do SaguiOS será de 64 bits (melhoria 106, `mejoras/106-saguios-64-bits.md`). Hoje o kernel e os programas são
de 32 bits, e `docs/arquitecturas.md` resume: o x86 de 32 bits se mantém depois da v10 para os PC sem suporte a 64 bits,
e a v10 acrescenta o x86-64. Esta seção explica o que muda, sem entrar no detalhe de cada instrução.

### Registradores e modo longo

- Os registradores gerais passam a ter **64 bits** (RAX até RSP, RBP, RSI, RDI, e RIP), e acrescentam-se **R8 a R15**:
  16 registradores gerais em vez de 8. Um prefixo de instrução (REX) indica o tamanho de 64 bits.
- O processador está no **modo longo** quando a paginação está ativa e o bit LME do registrador EFER (MSR `0xC0000080`,
  bit 8) está ligado. O trampolim UEFI da seção 3 faz o contrário: `entrada64` (`boot/uefi/trampolim.asm:48`) desliga a
  paginação (linha 65) e limpa LME e NXE (linha 68) para voltar ao modo de 32 bits.
- No modo longo, a segmentação quase desaparece: os segmentos de código e dados são tratados como planos e seus limites
  não são verificados. O descritor de código tem um bit L que indica que ele é de 64 bits.

### Paginação de quatro níveis

Com 64 bits, o endereço virtual tem 48 bits úteis e a paginação usa quatro níveis de tabelas: **PML4**, **PDPT**, **PD**
e **PT**. Cada tabela tem 512 entradas de 8 bytes; as quatro formam uma cadeia de índices tirados dos bits 39-47, 30-38,
21-29 e 12-20 do endereço, e o deslocamento dentro da página são os 12 bits baixos (páginas de 4 KB). CR3 aponta para a
PML4. Há páginas de 2 MB (param no nível PD) e de 1 GB (no PDPT). A Intel acrescenta um quinto nível (LA57) para 57 bits,
que aqui não é necessário.

### A convenção de chamada System V

No Linux e na quase totalidade dos sistemas de 64 bits, a convenção é a **System V AMD64**: os primeiros argumentos vão
em registradores (RDI, RSI, RDX, RCX, R8, R9), o valor de retorno em RAX, e a pilha deve estar alinhada a 16 bytes na
chamada. Isso muda o quadro da seção 3: já não há argumentos na pilha (exceto os que não cabem nos registradores), e as
variáveis locais podem ficar abaixo de RSP numa área de 128 bytes chamada *zona vermelha* (*red zone*), sem mover RSP.

### Chamadas ao sistema: syscall e sysret

Em 32 bits, o SaguiOS entra no kernel com `int 0x80`, que passa pela IDT, empilha o quadro e verifica permissões. Em 64
bits existem as instruções **syscall** e **sysret**, que mudam de anel sem passar pela IDT: o endereço de entrada está
num MSR (`LSTAR`), e os registradores da chamada vão em RAX (número), RDI, RSI, RDX, R10, R8 e R9. É mais rápido porque não
passa pela IDT. O `int 0x80` do SaguiOS (`kernel/inc/abi.h:5`) continuará funcionando na versão de 32 bits.

### O que mudaria no SaguiOS

Segundo `docs/arquitecturas.md` (linhas do x86-64), a v10 pede separar o código por arquitetura em
`kernel/arch/<arquitetura>/` (inicialização, troca de contexto, paginação, interrupções, chamadas ao sistema) e
compiladores próprios com outro destino. Com o que foi visto acima, as mudanças mais visíveis são:

- a IDT tem entradas de 16 bytes, não de 8, e o retorno de uma interrupção é `iretq`;
- o código de inicialização dos secundários e o `entrada.asm` passam pelo modo longo, com a tabela de quatro níveis, e o
  quadro de `vetores.asm` muda de tamanho (RSP e SS são sempre empilhados);
- os ponteiros dos drivers passam a ter 64 bits, e também o DMA e os BAR de 64 bits das GPU modernas, que é o motivo da
  melhoria 106;
- o compilador tem de gerar código de 64 bits (a v10 é um destino novo, não uma opção).

## 7. Exercícios

### Exercício 1: um setor de inicialização em NASM (no PC)

**Objetivo:** escrever um MBR de 512 bytes que mostre um texto com `int 0x10` e pare, e verificar que o assembler põe
a assinatura nele. Monta-se no PC com o NASM (`sudo apt install nasm`, como em `docs/CONTINUAR.md`).

Salve este código como `sector.asm`:

<!-- nasm: sector.asm -->
```nasm
; sector.asm - sector de arranque de 512 bytes que escribe un texto con la BIOS.
bits 16
org 0x7C00

inicio:
        cli
        xor ax, ax
        mov ds, ax
        mov ss, ax
        mov sp, 0x7C00
        sti
        mov si, mensaje
.bucle:
        lodsb                           ; AL = [DS:SI], y SI avanza
        test al, al
        jz .fin
        mov ah, 0x0E                    ; teletipo de la BIOS
        mov bx, 0x0007
        int 0x10
        jmp .bucle
.fin:
.parar:
        hlt
        jmp .parar

mensaje: db "Hola desde NASM", 13, 10, 0

        times 510 - ($ - $$) db 0       ; relleno hasta el byte 510
        dw 0xAA55                       ; firma del MBR (bytes 55 AA en el disco)
```

Monte-o e verifique o resultado:

    nasm -f bin sector.asm -o sector.bin
    ls -l sector.bin
    od -A d -t x1 sector.bin | tail -2

O tamanho deve ser 512 bytes, e os dois últimos, `55 aa`.

**Solução (o que faz cada parte):**

- `bits 16` e `org 0x7C00` dizem ao NASM que gere código de modo real e que os rótulos começam no endereço onde a BIOS
  carrega o setor.
- Os primeiros passos são os do MBR do SaguiOS: interrupções desligadas, segmentos em zero, pilha em `0x7C00`, e as
  interrupções são religadas com `sti`, como faz o MBR do SaguiOS (`boot/mbr.asm:41`).
- `lodsb` lê o byte de `[DS:SI]` e avança SI. Como o texto termina em 0, `test al, al` e `jz` saem do laço.
- `int 0x10` com AH = `0x0E` escreve o caractere em AL. BL é a cor, que só importa em modos gráficos.
- `times 510 - ($ - $$) db 0` preenche até o byte 510, e `dw 0xAA55` escreve a assinatura nos dois últimos bytes. Se o
  código passasse de 510 bytes, o NASM daria um erro de "times" negativo: é assim que se verifica o limite.

Para vê-lo funcionando no PC, pode-se usar o QEMU: `qemu-system-i386 -drive format=raw,file=sector.bin`.

### Exercício 2: CPUID dentro do SaguiOS com TCC

**Objetivo:** ler o fabricante da CPU a partir de um programa do SaguiOS.

O SaguiOS não traz o NASM, mas o **TCC** (`/apps/bin/tcc`) aceita `__asm__` em linha com sintaxe AT&T, e também pode
montar arquivos `.s` (quem os monta é o próprio TCC; `userland/terceros/tcc/fuentes/libtcc.c` trata disso nas linhas
1204-1206). Salve este código como `cpuid.c` dentro do SaguiOS (também está pronto em `/ejemplos/asm/cpuid.c`) e compile-o ou execute-o com `tcc -run cpuid.c`:

<!-- tcc: cpuid.c -->
```c
#include <stdio.h>

/* Lee CPUID con la hoja 0: EAX = hoja máxima, EBX/EDX/ECX = el fabricante. */
int main(void)
{
    unsigned int a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    printf("hoja maxima: %u\n", a);
    printf("ebx=%08x edx=%08x ecx=%08x\n", b, d, c);
    return 0;
}
```

**Solução:** o `cpuid` com EAX = 0 devolve em EAX a folha mais alta suportada, e o nome do fabricante está em EBX, EDX e
ECX, nessa ordem. Por isso são impressos nessa ordem. As restrições `"=a"`, `"=b"`... ligam cada registrador a uma
variável do C, e `"a"(0), "c"(0)` passam os valores de entrada.

**Estado:** testado no QEMU com o TCC (`tests/curso_ejemplos.py`): com `tcc -run` e compilado para um executável escreve a folha máxima e os três
registradores. No QEMU sem KVM sai `GenuineIntel` (EBX = `756e6547`, EDX = `49656e69`, ECX = `6c65746e`); com KVM, o fabricante do hospedeiro
(em AMD, `AuthenticAMD`).

### Perguntas de revisão

1. **Qual é o endereço físico de `0x1234:0x0010`?**
   Solução: `0x1234 × 16 + 0x0010 = 0x12340 + 0x10 = 0x12350`.

2. **Por que o MBR do SaguiOS não executa ele mesmo o carregador completo, mas lê um estágio 2?**
   Solução: o setor tem 512 bytes e a BIOS só carrega isso. O código do MBR tem de caber na lacuna entre o BPB e a
   tabela de partições (`boot/mbr.asm:10-17`), então só sabe ler o cabeçalho e o estágio 2, e saltar para ele.

3. **Por que a passagem ao modo protegido precisa de um salto distante depois de pôr PE?**
   Solução: ao ativar PE, a CPU continua executando com o CS do modo real em sua cache e com as instruções já lidas. O
   salto distante recarrega CS com o seletor do código de 32 bits e esvazia a fila de instruções.

4. **O que acontece se um programa do anel 3 executar `cli`?**
   Solução: a CPU gera #GP porque `cli` não é permitido no anel 3. O kernel encerra só esse processo
   (`kernel/arch/anel3_testes.asm:42-43` o descreve).

5. **Por que `int 0x80` tem DPL 3 (`0xEE`) e `int 0x20` não?**
   Solução: o DPL da porta diz qual anel pode chamá-la com `int`. As chamadas ao sistema precisam ser chamáveis pelos
   programas, e as IRQ são disparadas pelo hardware (`kernel/arch/idt.c:66-72`).

6. **Quem limpa a pilha na convenção cdecl?**
   Solução: quem chama, com um `add esp, N` depois do `call` (`kernel/arch/vetores.asm:60` é um exemplo).

7. **Por que o estado da FPU é salvo de forma preguiçosa?**
   Solução: salvar e restaurar 512 bytes a cada troca de processo é caro, e quase nenhum processo usa a FPU. Com
   CR0.TS, o primeiro uso provoca #NM e só então se faz a troca (`kernel/arch/fpu.c:12-16`).

8. **Quantos registradores gerais de 64 bits tem o x86-64, e como se chamam?**
   Solução: 16: RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP e R8 a R15.

## 8. Exemplos relacionados do curso

`rootfs/ejemplos/mico/parpadeo.asm` é um programa de exemplo escrito em assembler, mas **não é x86**. É o assembler da
máquina virtual CHIP-8, montado com o `chip8asm` (o comentário das primeiras linhas diz isso). Serve de contraste: o
mesmo conceito (mnemônicos, rótulos, saltos), em outra máquina. O editor Mico colore essa linguagem; a máquina está
explicada no [guia do CHIP-8](../chip8.md), e o editor no [guia do Mico](../mico.md).

Para a compilação de C dentro do SaguiOS, veja [TCC](../tcc.md) e [C no SaguiOS](../lenguajes/c.md).

## 9. Bibliografia

- A. S. Tanenbaum e H. Bos, *Sistemas operacionais modernos*. Livro geral de referência sobre inicialização, interrupções
  e gerência de memória, com uma abordagem mais teórica do que a deste anexo.
- Intel, *Intel 64 and IA-32 Architectures Software Developer's Manual*, volumes 1 a 3: a referência das instruções, da
  GDT, da IDT, da paginação e dos registradores de controle.
- AMD, *AMD64 Architecture Programmer's Manual*: a mesma informação para o x86-64, com o modo longo e a convenção de
  chamada.
- Documentação das extensões INT 13h (LBA, EDD) e do serviço E820 da INT 15h, que o carregador do SaguiOS usa como
  descrito acima.

Os manuais da Intel e da AMD são públicos. Este anexo não copia texto, figuras nem exercícios de nenhum livro.
