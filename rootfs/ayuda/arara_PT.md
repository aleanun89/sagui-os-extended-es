<!-- categoria: Juegos y aplicaciones; orden: 30; titulo: Arara -->
# Arara: diagramas

O **Arara** (`/apps/bin/arara`, em **Iniciar > Acessórios**) faz diagramas no estilo do Microsoft Visio, mas mais simples: fluxogramas,
redes, organogramas e esquemas feitos de figuras ligadas por conectores. Os arquivos são `.arara` (um JSON de texto legível); o diagrama
pode ser exportado para **PNG**, **SVG** e **PDF**.

    arara                       abre um diagrama vazio
    arara /ejemplos/arara/flujo.arara
    arara --abrir f.arara --png f.png --svg f.svg --pdf f.pdf     converte sem abrir a janela

## A tela

- **Esquerda, a paleta**: quatro paletas escolhidas na lista de cima. *Básicas* (retângulo, retângulo arredondado, elipse, losango,
  triângulo e texto), *Fluxograma* (início ou fim, processo, decisão, entrada ou saída, documento e banco de dados), *Rede* (PC,
  servidor, roteador, nuvem e impressora) e *Organograma* (caixa de pessoa).
- **Centro, a área de desenho**, com as abas das **páginas** em cima e a grade ao fundo.
- **Direita, o inspetor**: as propriedades do que está selecionado (texto, posição, tamanho, preenchimento, linha e espessura; nos
  conectores, o tipo e as setas). Sem seleção mostra as da página.
- **Embaixo, a barra de estado**: zoom, página, quantas coisas estão selecionadas e uma dica.

## Desenhar

1. **Arraste uma figura** da paleta até a área de desenho, ou clique nela e depois no lugar desejado. Com o ajuste à grade ligado ela cai alinhada.
2. **Clique duplo** numa figura para escrever dentro dela. *Enter* quebra a linha; *Esc* ou um clique fora termina.
3. Escolha a ferramenta **Reto** ou **Ângulo** e arraste de uma figura a outra: o conector gruda no ponto de conexão mais próximo (em
   cima, direita, embaixo ou esquerda). Com essas ferramentas os pontos de conexão aparecem. *Esc* volta ao ponteiro.
   **Ao mover uma figura, os conectores dela a acompanham.** O menu *Conector* define as setas dos conectores novos.

## Selecionar e editar

| Ação | Como |
| --- | --- |
| Selecionar uma figura ou um conector | clique; **Shift**+clique adiciona ou tira |
| Selecionar várias | arraste um retângulo no vazio, ou **Ctrl+A** |
| Mover | arraste (com ajuste à grade) ou use as setas do teclado |
| Mudar o tamanho | arraste as alças da caixa da seleção |
| Cor de preenchimento, de linha, espessura | no inspetor (o seletor de cor tem transparência) |
| Apagar | **Del** (os conectores de uma figura apagada ficam soltos onde estavam) |
| Copiar, recortar, colar, duplicar | **Ctrl+C**, **Ctrl+X**, **Ctrl+V**, **Ctrl+D** |
| Desfazer e refazer | **Ctrl+Z**, **Ctrl+Y** (até 60 passos) |

O menu **Organizar** traz **trazer para a frente** e **enviar para trás**, **alinhar** (esquerda, centro, direita, topo, meio, base)
e **distribuir** na horizontal ou na vertical (três figuras ou mais).

## Zoom, grade e páginas

- A **roda** do mouse aproxima e afasta em volta do ponteiro; para rolar, arraste com o botão do meio ou com a barra de espaço.
  *Exibir > Ajustar à janela* enquadra o diagrama inteiro.
- *Grade* mostra ou oculta a quadrícula e *Ajuste* faz o que você move saltar para ela.
- Um diagrama tem até **8 páginas**: a aba **+** cria uma, o **x** da aba a elimina e as abas podem ser reordenadas arrastando.
  *Página > Mudar o nome* ou o inspetor a renomeiam.

## Exportar

*Arquivo > Exportar para PNG* salva a página atual como imagem (em dupla resolução se não for enorme), *Exportar para SVG* como desenho
vetorial e *Exportar para PDF* todas as páginas, uma por folha A4 paisagem.

## O formato `.arara`

JSON com uma lista de páginas; cada uma traz suas `figuras` (`tipo`, `x`, `y`, `l`, `a`, `relleno`, `linea`, `grosor`, `texto`) e seus
`conectores` (as figuras que ligam e o ponto de cada uma: 0 em cima, 1 direita, 2 embaixo, 3 esquerda). As cores são `#RRGGBB` ou
`#AARRGGBB`. Pode ser lido e editado à mão. Exemplos em `/ejemplos/arara/`: `flujo.arara`, `red.arara` e `organigrama.arara`.
