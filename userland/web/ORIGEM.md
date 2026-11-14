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

## Licença e crédito

O repositório de onde veio **não tem ficheiro LICENSE**. O código foi escrito
por IA a partir do `prd.md` do mesmo projeto, e o próprio repositório diz
"sem código de outros sistemas" (as únicas exceções são tabelas de dados de
especificações). O que é de terceiros mesmo e está registado em
`docs/terceros.md`:

- as **fontes DejaVu** (`rootfs/fontes/`), com a `LICENCA-DejaVu.txt` ao lado;
- as **tabelas do Brotli** (RFC 7932) e da **quebra de linha** (UAX #14), que
  são dados das normas.

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

## Licença dos dados das normas

As tabelas do Brotli e da quebra de linha são dados factuais das normas
(ietf.org RFC 7932 e unicode.org UAX #14), não código de terceiros.

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
