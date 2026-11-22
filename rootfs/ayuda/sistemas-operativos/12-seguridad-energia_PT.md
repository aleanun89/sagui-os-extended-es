<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 12; titulo: Segurança, energia e desligamento -->
# Segurança, energia e desligamento

[Español](12-seguridad-energia.md) · [Português](12-seguridad-energia_PT.md) · [English](12-seguridad-energia_EN.md)

> Objetivo: entender o que um sistema operacional protege e contra o quê, como são usados os usuários e as permissões, qual papel o ACPI tem no gerenciamento de
> energia e por que desligar bem um sistema é um problema de ordem e de prazos.

## 1. Teoria

### 1.1 O que é segurança num sistema operacional

Segurança não é uma função única. Apoia-se em três ideias:

- **Identidade**: saber quem faz cada coisa. Cada processo e cada arquivo têm um dono (usuário e grupo).
- **Autorização**: decidir o que essa identidade pode fazer. São as permissões de arquivo e as chamadas privilegiadas.
- **Isolamento**: para que uma falha ou um programa malicioso não consiga tocar o que não lhe pertence. O espaço de endereçamento separado (capítulo 6) e o modo
  núcleo (capítulo 3) são a base.

O **princípio do privilégio mínimo** diz que cada processo deve ter apenas as permissões de que precisa. O administrador (`root`) é o caso extremo: pode quase tudo,
e por isso é usado com cuidado.

### 1.2 Usuários e permissões

O Unix e seus descendentes usam `uid` e `gid` para identificar cada processo e cada arquivo. O administrador é o uid 0. As permissões de arquivo são leitura, escrita
e execução para o dono, o grupo e os demais. Outros modelos, como as listas de controle de acesso (ACL) do Windows, dão permissões mais finas, ao custo de complexidade.

### 1.3 Chamadas privilegiadas

Nem todas as chamadas estão abertas a todos: mudar a hora, reiniciar a máquina ou montar discos são operações que o núcleo só permite a um administrador. O núcleo
verifica isso na própria chamada, antes de executá-la.

### 1.4 Energia e ACPI

O **ACPI** (*advanced configuration and power interface*) é a especificação pela qual o firmware descreve a placa: fontes de energia, sensores, desligamento, reinício
e baterias. O sistema operacional executa métodos de uma linguagem interna chamada **AML** para usá-los. Um desligamento correto usa o estado **S5** (desligado).

### 1.5 Desligar com ordem

Desligar mal pode perder dados: arquivos pela metade no cache de disco, um serviço que não fechou seu banco de dados, um processador que continua executando código
enquanto a energia cai. Um desligamento ordenado segue estes passos:

1. Pedir aos programas e serviços que terminem, com um prazo.
2. Descarregar o cache de disco e desmontar os sistemas de arquivos.
3. Parar os demais processadores e os drivers, na ordem inversa à do boot.
4. Desligar ou reiniciar pelo mecanismo mais confiável disponível.

Cada passo precisa de um **prazo**: se um passo trava, o sistema não pode esperar para sempre. A falha é anotada e o passo seguinte prossegue.

### 1.6 Alternativas

- Linux: o `systemd` coordena o desligamento dos serviços e o comando `shutdown`.
- Windows: o início rápido guarda parte do estado no disco (hibernação híbrida).
- BSD: o `shutdown` e o gerenciador `rc` param os serviços em ordem.

### 1.7 Cálculo: o prazo do pior caso de um reinício

O desligamento do SaguiOS (decisão D450) tem prazos limitados. Somando o pior caso de cada passo com prazo fixo, com um NVMe, um reinício leva no máximo:
serviços 20 s + ACPI 3 s + processadores 0,1 s + NVMe 5 s + `RESET_REG` 1 s + 8042 0,5 s + `0xCF9` 0,5 s = **30,1 s**. Ficam de fora os prazos dos discos, que dependem da
resposta de cada um (são medidos pelo relógio TSC). Com dois NVMe, o pior caso sobe 5 s a mais. O prazo limitado é o que impede o sistema de travar para sempre: sem
prazo, um passo bloqueado atrasaria o desligamento indefinidamente.

### 1.8 Comparação: desligamento em outros sistemas

- **Linux** com o `systemd` para os serviços: cada unidade tem um tempo de parada (90 s por padrão) e depois é encerrada.
- **Windows** dá a cada serviço um tempo para fechar (5 s por padrão no registro) e depois força o término.
- **FreeBSD** usa o `shutdown` e o sistema de scripts `rc`, que para os serviços na ordem inversa à do início.
- Todos compartilham a ideia: cada passo tem um tempo máximo, e o sistema segue mesmo que um serviço não responda.

### 1.9 Senhas: *hash*, sal e iterações

Um sistema não deve guardar as senhas, mas a sua **impressão** (*hash*): uma função de mão única. Ao entrar se calcula a impressão do que
foi digitado e se compara. Duas defesas a mais: o **sal** (um valor aleatório diferente por conta, que obriga a atacar cada conta
separadamente e deixa inúteis as tabelas pré-calculadas) e as **iterações** (repetir o cálculo muitas vezes para que cada tentativa seja
cara). Cálculo: uma senha de 8 caracteres de 62 símbolos tem 62^8 aprox. 2,2 × 10^14 possibilidades. Com um hash simples a 10^9 tentativas por
segundo se esgota em cerca de 2,5 dias; com 20 000 iterações cada tentativa custa 20 000 vezes mais (5 × 10^4 por segundo) e são cerca de
**140 anos** (números ilustrativos). O sistema também pode frear quem tenta online (uma espera após cada falha e um bloqueio após várias).

### 1.10 Domínios de proteção: ACLs e capacidades

A proteção pode ser vista como uma **matriz** de domínios (usuários ou processos) por objetos (arquivos, dispositivos), com os direitos em
cada casa. Guardá-la por **colunas** dá uma **lista de controle de acesso** (ACL): cada objeto diz quem pode fazer o quê; responde
facilmente «quem pode ler este arquivo?». Guardá-la por **linhas** dá **capacidades**: cada processo leva uma lista do que pode usar, como
um descritor de arquivo aberto; responde a «o que este processo pode tocar?» e se passa facilmente entre processos. As permissões do Unix
(`rwx` para dono, grupo e demais) são uma ACL reduzida a três entradas; um descritor de arquivo aberto é uma capacidade.

### 1.11 Princípios de projeto seguro

- **Privilégio mínimo**: cada processo só com o que precisa.
- **Padrões seguros**: o que não está expressamente permitido, nega-se.
- **Mediação completa**: verifica-se cada acesso, não só o primeiro.
- **Mecanismo econômico**: o menor e mais simples possível, para poder revisá-lo inteiro.
- **Projeto aberto**: a segurança não depende de o atacante ignorar o funcionamento (depende, sim, de ele ignorar a chave).
- **Aceitação psicológica**: se é incômodo, as pessoas o desativam.

### 1.12 Ataques típicos e defesas

- **Estouro de buffer.** Um programa copia dados sem verificar o tamanho e sobrescreve o que há atrás, como o endereço de retorno da
  pilha. Defesas: verificar tamanhos, páginas de dados não executáveis (**NX**, capítulo 6), uma página de guarda junto à pilha e
  aleatorizar os endereços (**ASLR**). O verme da Internet de 1988 se propagou, entre outras coisas, com um estouro de buffer num
  serviço de rede.
- **Malware.** Um **vírus** se cola a outro programa; um **verme** se copia sozinho pela rede; um **cavalo de Troia** é um programa
  aparentemente útil que faz outra coisa. Defesa: privilégio mínimo, e o que roda em nome de um usuário não deve poder alterar o sistema.
- **Canais ocultos.** Dois processos que não podem conversar podem se comunicar através de algo que compartilham, como o tempo que cada
  um leva para obter a CPU. São muito difíceis de fechar.

### 1.13 Energia: estados da CPU

A CPU gasta energia de duas formas que podem ser ajustadas. Com **estados de desempenho** (P-states) baixam-se ao mesmo tempo a
frequência *f* e a tensão *V*: a potência dinâmica é proporcional a *C* × *V*² × *f*. Com **estados de repouso** (C-states) desligam-se
partes do chip quando não há trabalho; o mais simples é a instrução `hlt`, que para a CPU até a próxima interrupção. Baixar a frequência
25 % e a tensão 15 % deixa a potência em 0,75 × 0,85² aprox. **0,54**: uma economia de 46 %, enquanto o trabalho leva um terço a mais.

## 2. Como o SaguiOS faz

- **Usuários e permissões.** `kernel/nucleo/usuarios.c` (a qual usuário pertence cada processo, seu uid e gid) e `kernel/nucleo/permisos.c` (quais chamadas cada um
  pode fazer), com as decisões D255 (chamadas privilegiadas) e D282 (dono, grupo e `rwx`). As contas e o administrador têm seu desenho na decisão D371 (`/root`, pastas
  próprias e configuração do sistema separada da pessoa).
- **Bloqueio de sessão.** `kernel/gui/sessao.c` gerencia o início e o fim da sessão; `kernel/gui/ocio.c` o bloqueio e o protetor de tela. O menu da barra de tarefas
  (`kernel/gui/barra.c`) oferece "Bloquear" quando há contas.
- **Isolamento.** Cada processo tem seu espaço de endereçamento (capítulo 6), as chamadas validam os ponteiros do programa com `usuario_ok` (capítulo 3) e as permissões são
  verificadas em cada `open`.
- **Senhas.** As contas guardam uma impressão **PBKDF2-SHA256** com sal de 16 bytes e 20 000 iterações (`userland/inc/pbkdf2.h`); quem a
  verifica é o `login` e o gerenciador de serviços, e só o root pode ler o arquivo de sombra. Após uma falha, a resposta demora 2 s, e
  após cinco seguidas do mesmo usuário há um bloqueio de um minuto.
- **Elevação sem `setuid`.** `admin` e `su` não são programas com privilégios próprios: pedem a elevação ao gerenciador de serviços
  (decisão D255), que consulta a sombra e, se a concede, o núcleo troca a identidade do processo que a pediu.
- **Fechado por padrão.** Em `kernel/nucleo/permisos.c`, uma chamada ao sistema que ninguém classificou fica **só para o root**: esquecer
  de classificá-la nega, não permite.
- **Programas do sistema e ligação dinâmica.** Um programa de `/bin` é carregado com `AT_SECURE` (decisão D378): não lê a variável
  `SAGUI_LIB_CAMINHO` e só carrega bibliotecas do root sem escrita de outros, para que ninguém altere o que roda com privilégios.
- **Energia da CPU.** `kernel/dispositivos/frequencia.c` escolhe entre os P-states que a BIOS declarou (decisão D106, e por CPU com SMP
  na D175), com um limite de temperatura de 79 °C; uma CPU ociosa dorme com `hlt` (capítulo 7).
- **Serviços.** `kernel/nucleo/supervisor.c` inicia, reinicia e para os serviços (decisão D216). O comando `servicios` os mostra.
- **ACPI.** `kernel/dispositivos/uacpi_sagui.c` é a camada do núcleo para a biblioteca uACPI (código de terceiros, MIT, em `kernel/terceros/uacpi/`), e
  `kernel/dispositivos/acpi.c` faz o desligamento S5 (decisão D113). `bateria.c` lê o estado da bateria.
- **Desligamento e reinício ordenados.** `kernel/nucleo/energia.c` (decisão D450) tem um único caminho para reiniciar e para desligar, na tarefa de desligamento do núcleo.
  Sua ordem fixa é:
  1. início;
  2. serviços (no máximo 20 s, em `supervisor.c`);
  3. discos: gravar o pendente e desmontar (cada comando de disco tem o seu prazo);
  4. ACPI: `_TTS(5)` e, ao desligar, `_PTS(5)` (no máximo 3 s); depois as interrupções ficam fechadas;
  5. processadores secundários parados (no máximo 100 ms) e drivers desligados na ordem inversa à do boot;
  6. reinício pelo `RESET_REG` da FADT (1 s), depois pelo controlador de teclado, pela porta 0xCF9 e, se tudo falhar, um triplo erro; desligamento pelo
     `SLP_TYP|SLP_EN` de `\_S5_` (3 s).

  Cada passo deixa uma marca no registro serial e num rastro que sobrevive a um reinício a quente.
- **Comandos.** `apagar-pc` e `reiniciar` são comandos internos do shell (`userland/sagsh/internos.c`).

![Visor de registros com as marcas do desligamento e da energia](img/cap-visorlogs.png)

### 2.1 Decisões, alternativas e limites atuais

- **Decisão: usuários e chamadas privilegiadas.** A decisão **D255** reserva a um administrador as chamadas que mudam o sistema; a decisão **D282** dá permissões de
  arquivo por dono, grupo e `rwx`.
- **Decisão: contas e `/root`.** A decisão **D371** separa a configuração do sistema da configuração de cada pessoa.
- **Decisão: serviços supervisionados.** A decisão **D216** inicia, vigia e reinicia os serviços (`kernel/nucleo/supervisor.c`).
- **Decisão: um único caminho de desligamento com ordem e prazos.** A decisão **D450** (`kernel/nucleo/energia.c`).
- **Limite: o isolamento é o da memória e o das chamadas.** Um programa não pode tocar a memória de outro, mas não há jaulas (*jails*) nem contêineres: um
  usuário administrador tem acesso a tudo.
- **Limite: os prazos estão fixados no código.** Mudá-los exige recompilar; não há um arquivo de configuração para isso.

## 3. Experimento no SaguiOS

1. Digite `whoami` e `id`. Compare o uid e os grupos do seu usuário com os de `root`.
2. Digite `ls -l /ayuda` e observe as permissões dos arquivos. Tente o `cat` de um arquivo sem permissão de leitura, se você tiver um, e observe o erro.
3. Digite `servicios` e veja os serviços que o supervisor mantém vivos.
4. Abra o **Visor de registros** (nome em espanhol no menu), escolha a categoria **Energía** (Energia) e procure `apagado` ou `reinicio`. Se o registro guardar marcas de um desligamento anterior, você as verá nessa categoria.
5. **Não execute `apagar-pc` nem `reiniciar` para fazer o experimento**: são os comandos que fecham o sistema. Leia-os em `energia.c` e na decisão D450, e compare a ordem com a teoria.

**O que observar:** um desligamento é mais um programa, com passos, prazos e registros. Por isso pode ser depurado quando falha.

**Exercício prático adicional.** Leia o cabeçalho de `kernel/nucleo/energia.c` e desenhe, com suas palavras, a sequência de passos do desligamento, com o prazo de cada um. Depois abra o Visor de registros,
escolha a categoria Energia e explique o que significa cada marca, se houver alguma. Não execute `apagar-pc` para fazer isso.

## 4. Perguntas de revisão

1. O que é o princípio do privilégio mínimo? Dê um exemplo de um serviço que o cumpre e de um que não o cumpre.
2. Por que o desligamento deve desmontar os discos antes de cortar a energia, e o que acontece se não o fizer?
3. Por que cada passo do desligamento tem um prazo? Que problema isso evita?
4. Um driver de rede leva 30 segundos para fechar durante o desligamento. O que o sistema deve fazer, segundo a ordem do SaguiOS?
5. Se um passo do desligamento leva 40 s e não tem prazo, o que acontece com o sistema? E com um prazo de 5 s?
6. Por que um programa de usuário não consegue ler a senha de outro usuário mesmo conhecendo o endereço de memória dela?
7. Qual a diferença entre o desligamento pelo ACPI (S5) e um reinício pelo controlador de teclado?
8. Uma senha de 6 letras minúsculas é testada a 5 × 10^4 tentativas por segundo (com iterações). Quanto demora para esgotar o espaço de
   busca?
9. Qual a diferença entre uma ACL e uma capacidade? Qual responde melhor a «quem pode ler este arquivo?»?
10. Se a frequência cai 50 % e a tensão 20 %, que fração da potência dinâmica sobra?
11. Por que `permisos.c` deixa fechadas as chamadas que ninguém classificou?

## 5. Respostas

1. Um processo deve ter apenas as permissões de que precisa. Um serviço de impressão que só escreve na sua fila cumpre o princípio. Um programa que sempre roda como administrador não cumpre, mesmo que seja cômodo.
2. O cache de disco guarda dados ainda não gravados. Se a energia for cortada antes de descarregá-lo, os dados se perdem ou ficam inconsistentes. Desmontar força a gravação e deixa o sistema de arquivos num estado limpo.
3. Porque um passo pode travar (um disco que não responde, um driver preso). Sem prazo, o desligamento esperaria para sempre. Com prazo, a falha é anotada e o sistema segue.
4. Segundo a D450, cada passo tem um prazo: passado o tempo, a falha é anotada e o desligamento continua com o passo seguinte. O driver não bloqueia o resto do desligamento, e o registro guarda a marca do passo que falhou.
5. Sem prazo, o desligamento espera 40 s ou mais, e se o passo travar, espera para sempre. Com prazo de 5 s, a falha é anotada aos 5 s e o desligamento continua com o próximo passo.
6. Porque o espaço de endereçamento de cada processo é diferente: o endereço não existe no espaço do outro. Além disso, o núcleo valida os ponteiros de cada chamada (`usuario_ok`).
7. O desligamento pelo ACPI pede ao firmware que entre no estado S5 (desligado); um reinício pelo controlador de teclado (0xFE no 8042) pede ao controlador que reinicie o processador. São mecanismos diferentes: um desliga, o outro reinicia.
8. 26^6 = 308 915 776 possibilidades / 50 000 por segundo aprox. 6178 s, ou seja, **1,7 hora**.
9. Uma ACL fica em cada objeto e diz quem pode usá-lo (responde melhor a «quem pode ler este arquivo?»); uma capacidade é levada por cada
   processo e diz o que pode usar (responde a «o que este processo pode tocar?»).
10. 0,5 × 0,8² = 0,5 × 0,64 = **0,32**: sobram 32 % (economiza-se 68 %).
11. Pelo princípio de padrões seguros: se alguém acrescenta uma chamada e esquece de classificá-la, o que acontece é que ninguém além do
    root pode usá-la (uma falha visível e sem risco), e não que todos possam usá-la sem perceber.
