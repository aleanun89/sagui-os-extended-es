# 059 - Visor de PDF

- **Estado:** feita (v6.3); rama `v6.3-pdf`
- **Prioridad:** media-alta
- **Alta:** 2026-11-10
- **Relacionadas:** 047 (JPEG e imagens), 042 (componentes da libsagui), 048 (vídeo),
  049 (áudio), 056 (compressor), 064 (SQLite), 082 (Pentium III sem SSE2),
  003 (`cc`, que já compila com a x87)

## Problema / motivação

O SaguiOS não tinha forma nenhuma de ler um PDF. Havia um visor de imagens (BMP, PNG,
JPEG) e o `reproductor` para vídeo, mas o PDF é o formato de documento que toda a
gente tem por aí: manuais, relatórios, livros. Um PDF é também um formatoacetado
pelo PDF/A (ISO 19005), que é o que se guarda em arquivos e bibliotecas.

O problema não é só "abrir um PDF": é abrir os de verdade, com as fontes que
traem dentro (para que o texto saia com a forma certa) e os que estão protegidos
com palavra-passe.

## Proposta

Um motor de PDF escrito do zero em `userland/pdf/` e um programa `visorpdf` que o
mostra numa janela.

| Peça | Onde | O que faz |
| --- | --- | --- |
| **Analisador** | `userland/pdf/pdf_lex.c` | léxico, objetos, dicionários, *streams* com filtros (Flate, LZW, ASCIIHex, ASCII85, RunLength), a tabela de referências cruzadas (a clássica e a de *stream*), a reconstrução por varrimento quando a xref está estragada, os *object streams* e a árvore de páginas com o `/MediaBox` e o `/Rotate` herdados |
| **Criptografia** | `userland/pdf/pdf_cripto.c` | o *standard security handler* da ISO 32000-1 §7.6: RC4 (V1/V2), AES-128 em CBC (V4) e AES-256 (V5/R6, com o hash 2.B da norma). MD5 e RC4 próprios; o AES inverso, calculado em GF(2^8) |
| **Tipografias** | `userland/pdf/pdf_fonte.c` | TrueType (`glyf`, glifos simples e compostos), CFF (charstrings tipo 2) e Type 1 (charstrings tipo 1, com `eexec` e RC4). As 14 fontes básicas (Helvetica, Times, Courier…) vêm com contornos próprios |
| **Geometria** | `userland/pdf/pdf_geom.c` | caminhos, aplanado de curvas e matrizes 2x3 |
| **Renderizador** | `userland/pdf/pdf_render.c` | o intérprete do conteúdo: caminhos, texto, imagens, recortes, sombras e transparência; preenche com a regra *non-zero* e *even-odd*, com cobertura antisserrilhada na horizontal |
| **Programa** | `userland/programas/visorpdf.c` | janela com barra de botões (página anterior/seguinte, zoom, ajustar, 1:1), barra de estado, rolagem com a roda e as setas, F11 e Ctrl+O |

### Onde fica e porquê

O motor **não** entra na `libsagui` comum: usa números com vírgula flutuante, e a
`libsagui` vai sem eles (`-mgeneral-regs-only`, D076, para o sistema arrancar num
Pentium III ou num Athlon XP sem SSE2). Fica numa pasta própria, como o `sagui3d` e
o `cc`, e é compilado com a **x87** (`-mfpmath=387`): assim funciona em todo o
hardware suportado, e não só nos que têm SSE2. O kernel emula a FPU por processo.

O motor não sabe nada de ventanas: escribe los píxeles en un búfer RGBA de 4 bytes
por píxel y el programa los compone encima de la ventana. Así el motor se puede
probar sin pantalla (`visorpdf --ppm` escribe un PPM para comparar).

## Critérios de aceitação

- [x] Abre un PDF y dice cuántas páginas tiene (`visorpdf -t`)
- [x] Muestra la página en una ventana, con scroll, zoom y salto de páginas
- [x] Dibuja el texto con los contornos de la fuente incrustada (TrueType, CFF o Type 1)
- [x] Dibuja imágenes (JPEG y sin filtro), con su máscara de alfa
- [x] Abre un PDF cifrado (RC4, AES-128 y AES-256) con su contraseña
- [x] Abre un PDF/A (es un PDF: no hace falta nada más)
- [x] Descomprime los streams con FlateDecode (el envoltorio zlib de verdad)
- [x] Test automático en `tests/h107_visorpdf.py` que lo comprueba
- [x] Decisión registrada en `docs/decisoes.md` (D138)

## Notas

- *Sin código de terceros.* Todo sale de la norma ISO 32000-1 (el formato) y de la
  ISO 19005 (el PDF/A), más el RFC 1321 (el MD5). No hay código de pdf.js, de
  poppler ni de ningún motor de PDF: ni copied, ni adapted, ni studied.
- *Las fuentes incrustadas* se dibujan como vectores, con sus contornos. Las 14
  fuentes básicas no van incrustadas en el PDF: el sistema dibuja sus formas.
- *Lo que no está:* los formularios rellenables, los enlaces, las anotaciones, el
  texto con expresiones regulares (OpenType `GSUB`/`GPOS`), los colores de espacio
  Lab y los sombreados que no sean axiales.

## Como se comprobó

- *Los vectores de la norma.* El SHA-256, el SHA-384 e o SHA-512 se comparam com os
  vectores do FIPS 180-4 e o AES (bloco e CBC) com os do FIPS 197 e do NIST SP
  800-38A. O RC4, com o vector do RFC 6229.
- *A revisão 6 (AES-256).* O "hash 2.B" da ISO 32000-2 foi conferido contra o
  ficheiro de teste do PDFium (`encrypted_hello_world_r6.pdf`): com a palavra-passe
  "hôtel" tem de dar exatamente o `/U` que está no ficheiro. Quando se descobre
  que o hash dá outro valor, é porque o bloco K cresce (32, 48 ou 64 bytes) e o
  comprimento da repetição tem de mudar com ele.
- *A prova no sistema.* `tests/h107_visorpdf.py` gera quatro PDF (o simples, o
  comprimido com FlateDecode, o cifrado com RC4 e o cifrado com AES-256), escreve-os
  num pendrive e abre-os no SaguiOS dentro do QEMU: conta as páginas, lê o título,
  escreve o PPM e compara os píxeis (o rectângulo azul no sítio, o texto com tinta,
  as margens brancas), e confirma que um PDF cifrado não abre sem a palavra-passe e
  abre com ela.
- *O AES dos PDF de teste.* Para gerar o PDF com AES-256 sem meter código de
  terceiros, `tests/aes256.py` traz um AES escrito à mão (a tabela S sai da
  inversa multiplicativa em GF(2^8)); foi conferido contra uma biblioteca de
  referência e contra o vector do PDFium.
- *PDF reales (v7.1.1, D205).* `tests/h205_pdf_reales.py` abre seis PDF de
  verdad (OpenOffice, Adobe Designer, pdfTeX, cairo, un escaneado CCITT y
  Word/Acrobat, en `tests/recursos/pdf/`) y compara cada página con la que
  dibuja MuPDF, reducida a una rejilla de grises. Con ella salieron y se
  arreglaron el `cm` concatenado al revés (la causa de la «basura»), las
  fuentes Type 1 y CFF, la xref en flujo con predictor, la `/SMask`, el
  JPEG con reinicio y el filtro CCITT, entre otros (ver D205).
