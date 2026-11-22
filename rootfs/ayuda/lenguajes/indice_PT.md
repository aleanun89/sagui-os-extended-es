<!-- categoria: Programación; orden: 1; titulo: Programar en SaguiOS -->
# Programar no SaguiOS

O SaguiOS traz várias linguagens de programação. Todas são usadas pelo **Terminal**, e quase todas podem ser escritas
com o editor **Mico** (Inicio > Accesorios, no menu Iniciar). Esta página compara as linguagens e diz qual escolher para
cada tarefa. Cada linguagem tem o seu guia com o primeiro programa passo a passo.

## Qual escolher

- **Nunca programou?** Comece por **Python**: é o mais fácil de ler, tem janelas simples e funciona bem no terminal. Se
  preferir blocos coloridos, experimente antes o [Jabuti](../jabuti.md).
- **Quer entender como o sistema funciona?** **C**. É a linguagem do SaguiOS: com `cc` você escreve programas de console
  e com `tcc` janelas e jogos.
- **Quer automatizar tarefas no terminal?** **sagsh** (a linguagem do shell): roteiros que chamam os programas do sistema,
  com variáveis, laços e funções.
- **Tem um arquivo de texto com colunas ou números?** **awk**: um único comando soma, filtra e reordena.
- **Aulas de Java na escola ou na faculdade?** **Java**: `java` e `javac` funcionam dentro do SaguiOS, e com `sagui.gui`
  você faz janelas.
- **Gosta de Go ou quer experimentar goroutines?** **Go** (TinyGo): os programas são compilados no PC, e o SaguiOS já tem
  exemplos prontos em `/apps/go/`.
- **Quer uma linguagem pequena para embutir nos seus programas?** **Lua**.
- **Quer aprender a programação do passado?** **Pascal**, com o comando `pascal` e a unidade `sagui` para gráficos.

## As linguagens num relance

| Linguagem | Para que serve | Como roda | Janelas | Rede | Guia |
|-----------|----------------|-----------|---------|------|------|
| **sagsh** | automação, roteiros do sistema | `sh guion.sh` | não | com comandos (`curl`, `ssh`) | [sagsh](sagsh_PT.md) |
| **C** (`cc`) | programas de console rápidos | `cc a.c -o a` e `./a` | não | não tratada aqui | [C](c_PT.md) |
| **C** (`tcc`) | janelas, jogos, erros de memória (`-b`) | `tcc -run a.c` | sim (libsagui, SDL 2, OpenGL ES) | não tratada aqui | [C](c_PT.md), [TCC](../tcc.md) |
| **Python** | roteiros, dados, janelas, rede | `python a.py` | sim (`sagui`, tkinter) | sim (`socket`, `requests`) | [Python](python_PT.md) |
| **Java** | programas com classes, janelas `sagui.gui` | `java A.java` | sim (`sagui.gui`) | sim (`java.net`) | [Java](java_PT.md) |
| **Go** (TinyGo) | programas com goroutines | compilado no PC; no SaguiOS, `/apps/go/` | não | ainda não | [Go](go_PT.md) |
| **Lua 5.4** | roteiros pequenos, embutível | `lua a.lua` | não | não | [Lua](../lua.md) |
| **awk** | texto por colunas | `awk '{ print $1 }' f` | não | não | [awk](../awk.md) |
| **Pascal** | programas no estilo Turbo Pascal | `pascal a.pas -o a` | sim (unidade `sagui`) | não | [Pascal](../pascal.md) |

O **make** não é uma linguagem para escrever programas, mas é a ferramenta que compila projetos inteiros com muitos
arquivos: [make](../make.md).

## Onde escrever o código

- **Mico** (o editor de código): colore C, Python, sh, Markdown e CHIP-8. Com **F5** executa o arquivo que você tem
  aberto (C com `cc`, Python com `python`, sh com `sh`). Veja [Mico](../mico.md).
- O **Pluma** é um processador de textos (salva em RTF por padrão): serve para anotações, não para programas.
- **Laboratório de C**: em Inicio > Educación. Um editor com exemplos e botão de executar, para começar em C.
- Para programas que rodam numa janela, o **VisualPythonS** deixa você desenhar o formulário com o mouse e escrever só as
  funções de cada botão: [VisualPythonS](../visualpythons.md).

## Outros caminhos para começar

- [Jabuti](../jabuti.md): programar arrastando blocos, sem escrever texto.
- [CHIP-8](../chip8.md): um montador e um emulador de uma máquina de 1977, para entender como um processador funciona por
  dentro.
- [Mico](../mico.md) e o [Terminal](../terminal.md): as ferramentas do dia a dia.

## Guias desta seção

- [C: seu primeiro programa](c_PT.md)
- [Go com TinyGo](go_PT.md)
- [Python com MicroPython](python_PT.md)
- [Java com miniJVM](java_PT.md)
- [sagsh: a linguagem do shell](sagsh_PT.md)
- Já na ajuda: [Lua](../lua.md), [awk](../awk.md), [Pascal](../pascal.md), [make](../make.md),
  [TCC](../tcc.md), [Laboratório de C](../laboratorio-c.md), [Janelas em Python](../python-tk.md).
