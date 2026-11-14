# Origem do motor da web (melhoria 081)

O motor que faz o Cipó mostrar páginas de verdade veio de outro repositório:
**https://github.com/robson-apk/sagui-os**, ramo **`cipo-web`** (commit `03906c8`,
"docs: Cipó — o que existe, capturas, limitações e problemas resolvidos").

Esse repositório é outra implementação do mesmo projeto (o `prd.md` do SaguiOS do
Eli Rigobeli), escrita por IA. **Não é um ramo deste repositório**: não tem
história comum connosco e o seu `sagui.h` é diferente do nosso. O que se fez aqui
foi trazer o motor, adaptationá-lo e ligá-lo ao nosso Cipó.

## O que trouxe

| Ficheiro | Linhas | Para que serve |
| --- | --- | --- |
| `motor.c` + `motor.h` | 5 964 | HTML em árvore, cascata CSS, caixas (bloco, inline, float, flex, grid, tabela, posicionadas), lista de pintura |
| `js.c` + `js_base.h` + `js_nativo.h` + `js_num.h` + `js_big.h` + `js_quadros.h` + `js_base.js` | 7 291 | Motor de JavaScript com DOM, eventos, Proxy, BigInt, iteradores |
| `canvas2d.c` | 1 800 | Canvas 2D (telas ARGB) |
| `svg.c` | 1 700 | SVG |
| `webgl.c` + `glsl.c` + `glsl.h` + `glsl_emb.h` + `glsl_gen.h` | 2 665 | WebGL 1 por software e compilador GLSL ES |
| `fonte.c` (desde la D196 en `userland/libfontes/`) | 1 700 | Fontes da web: TrueType (glyf), CFF, WOFF (zlib), WOFF2 (Brotli), fontes variáveis, rasterizador com antialias por área |
| `quebra.c` + `quebra_tab.h` | 400 | Quebra de linha (UAX #14) |
| `brotli.c` + `brotli_tab.h` | 1 100 | Descompressor Brotli (RFC 7932) para as fontes WOFF2 |
| `rootfs/fontes/*.ttf` | — | As fontes do sistema (DejaVu recortadas), com a `LICENCA-DejaVu.txt` |

O motor **não fala com a rede**: é um renderizador puro. A rede fica no nosso
Cipó, com o nosso `http.c` e o nosso TLS (`userland/libtls/`, que é melhor que o
deles: o deles nunca foi auditado). As imagens, o histórico, o buscar na página,
os formulários e o TLS são os nossos.

## O que mudou no código trazido

O código ficou byte a byte como estava, com estas mudanças mecânicas:

1. **O include.** Os ficheiros faziam `#include <sagui.h>`; agora fazem
   `#include "web.h"`, que é a nossa `sagui.h` mais a API que faltava (fontes
   da web, quebra de linha, UTF-8, Canvas 2D e JavaScript).
2. **Nomes que colidem com os nossos.**
   - `imagem_decodificar` (o deles devolve pixels ARGB; o nosso devolve uma
     `imagem_t`) → `web_imagem_decodificar`, em `web_compat.c`.
   - `sincronizar` e `caixa_t` (estáticos do `webgl.c`) → `sincronizar_tela` e
     `gl_caixa_t`.
3. **`g_sqrt` do `glsl.c`** usava `sqrtss` (SSE), que o nosso `x87` não aceita;
   passou a `fsqrt` do x87, para funcionar em qualquer CPU.
4. **`motor.h`** passou a incluir `web.h` (antes dependia de quem o incluía).
5. **Duas mudanças noiq de verdade** (commit `b176bbb`, melhoria 081), porque o hit-test
   do motor nao distinguia uma imagem sem link:
   - `MA_IMAGEM`: um alvo novo, com a URL da imagem, para o menu do botao direito a
     poder guardar a imagem no disco. As imagens que estao dentro de um link tambem
     expoem a URL.
   - `motor_rolagem()`: devolve o deslocamento vertical corrente da pagina. Sem ele,
     pintar a pagina por faixas deixava o hit-test com o deslocamento da ultima faixa
     e os cliques acertavam no sitio errado.

Ficaram também os nossos cabeçalhos: `web.h`, `js.h` e `canvas2d.h` (a API que o
motor espera), e `web_compat.c` (o que o motor precisa do browser: imagens em
ARGB, o pedido preparado do `fetch` e o pote de cookies).

## Licença e crédito (D262)

O motor é **GPL-3.0-only**. O repositório de onde veio não tinha ficheiro LICENSE, mas o
autor (robson-apk) propôs o código ao projeto original como o **PR #6 de
`erigobeli/sagui-os`** (aberto em 2026-11-10, sem fundir) e esse repositório é GPL-3.0:
pelos Termos do GitHub (secção D.6, «inbound = outbound») o que se contribui para um
repositório com licença fica sob essa licença. **Confirmado em 14/11/2026** (o código do PR #6 foi aceito para integração no SaguiOS original, GPL-3.0, por um canal interno; antes, pediu-se ao autor que o confirmasse)
explicitamente (rascunho em `docs/borradores/robson-licencia.md`). Os ficheiros levam
`SPDX-License-Identifier: GPL-3.0-only` (lista `ROBSON` de `tools/spdx.py`). Ficam sem
marca as tabelas de dados (`brotli_tab.h`, `quebra_tab.h`) e o `js_base.h` gerado.

O autor reconheceu em 2026-11-14 (commit `8c3b56b` de `robson-apk/sagui-os`,
`docs/cipo.md`) que **nem tudo foi escrito do zero**: há trechos que seguem de perto
projetos conhecidos. Os que estão na nossa cópia foram separados em
`userland/web/terceros/<projeto>/`, com a licença original e um `ORIGEM.md`, **sem mudar o
comportamento**, e estão em `docs/terceros.md`:

| Trecho | Onde estava | Projeto e licença | Agora |
| --- | --- | --- | --- |
| Rasterizador por área acumulada (linha do contorno, soma corrida, variante recortada e regra par-ímpar) | `linha()` de `fonte.c` (hoje `userland/libfontes/`) e `r_linha()` + soma de `cobrir()` em `canvas2d.c` | **font-rs** (Raph Levien, Google), **Apache-2.0** | `terceros/font-rs/fontrs.h` |
| `Number.prototype.toString(base)` | `n_numstr` de `js_nativo.h` | **V8** (`DoubleToRadixCString`), **BSD-3-Clause** | `terceros/v8-numeros/numero_base.h` |
| Tabelas da quebra de linha (UAX #14) | `quebra_tab.h` | **Unicode**, Unicode License v3 | dados no sítio; licença em `terceros/unicode/` |
| Dicionário, transformações e contextos do Brotli | `brotli_tab.h` | **RFC 7932** (IETF Trust; dicionário de Google, MIT) | dados no sítio; nota em `terceros/rfc7932-brotli/` |

**Não estão na nossa cópia** (só vieram no PR #6, que também tem o vídeo): o decodificador
VP8/WebP (libwebp, BSD), as tabelas CABAC/CAVLC do H.264 (FFmpeg, LGPL-2.1+) e os livros de
Huffman do AAC (FAAD2, GPL-2.0+). Se algum dia se integrar o vídeo, vão em
`userland/web/terceros/` com a licença e o crédito (o do FAAD2 exige a mensagem «Code from
FAAD2 is copyright (c) Nero AG, www.nero.com»). As **fontes DejaVu** (`rootfs/fontes/`) têm a
`LICENCA-DejaVu.txt` ao lado.

### O ficheiro original, em `referencia/`

O `referencia/cipo_robson.c` é o **programa** do Cipó tal como estava no
repositório de origem (ramo `cipo-web`, commit `03906c8`): o ponto de partida
da nossa adaptação. Fica guardado **só como referência**: o `Makefile` não o
compila (só pega nos `.c` de primeiro nível) e não faz parte da imagem. O
`LEIA-ME.md` desse directorio explica de onde vem e qual é o estado da sua
licença.

O motor do JavaScript tem uma base gerada (`js_base.h`) a partir de
`js_base.js` com `tools/web/mkjsbase.py`, que pode ser re-gerada se mudar o
origem.

## Como se usa

O motor é um renderizador: dá-lhe o HTML e ele pinta.

```c
motor_ganchos_t g = { baixar, imagem, tela };   /* o que o browser dá ao motor */
motor_t *m = motor_novo(&g);
motor_ler(m, html, tam_texto(html), url);
int alt = motor_layout(m, largura, altura_tela);
motor_pintar(m, px, largura, largura, y, a);     /* px: uint32_t da janela */
const motor_alvo_t *alvo = motor_alvo(m, x, y);  /* o que há sob o rato */
```

O programa de teste `userland/testes/cipo-motor.c` faz exactamente isto com um
HTML pequeno e diz no log quantos nós, comandos de pintura e pixels pintou.
