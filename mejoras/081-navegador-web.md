# 081 - El navegador con motor de verdad: CSS, JavaScript, Canvas, SVG y WebGL

- **Estado:** en curso
- **Prioridad:** alta
- **Alta:** 2026-11-11
- **Relacionadas:** 043 (el cliente de IA), 048 (el vídeo), D140 (el asistente del Mico), D141 (el motor de la web), D142 (la hoja Macaco)

## Problema / motivación

El Cipó era un **lector de HTML en texto**: parseaba etiquetas, cortaba el texto en
trozos y los dibujaba con la fuente de 8x16. No entendía CSS, no maquetaba nada, no
había imágenes con su tamaño real, no había tablas ni flex ni grid, y las páginas de
verdad salían como un amasijo de líneas.

El proyecto tenía además una decisión pendiente: **el sistema web del SaguiOS**. No
existía un motor de página. Existía el plan de escribir uno, y mientras tanto el
navegador se quedaba en el lector de texto.

## Propuesta

Traer el motor de página que ya existe escrito en otro repositorio de la misma
familia, adaptarlo y **cambiar solo el renderizador del Cipó**, conservando la
interfaz, el historial, el buscar en la página, los formularios y la red que ya
teníamos.

### Lo que se trajo

De `https://github.com/robson-apk/sagui-os`, ramo `cipo-web` (commit `03906c8`):

| Módulo | Qué da |
| --- | --- |
| `motor.c` + `motor.h` | HTML en árbol, cascata CSS, cajas de bloque, inline, float, flex, grid, tabla y posicionadas |
| `js.c` y sus cabeceras | Motor de JavaScript con DOM, eventos, Proxy, BigInt, iteradores |
| `canvas2d.c` | Canvas 2D (telas ARGB) |
| `svg.c` | SVG |
| `webgl.c` + `glsl.c` | WebGL 1 por software y compilador GLSL ES |
| `fonte.c` | Fuentes de la web: TrueType, CFF, WOFF, WOFF2, variables, con antialias por área |
| `quebra.c` | Ruptura de línea (UAX #14) |
| `brotli.c` | Descompresor Brotli (RFC 7932) para las fuentes WOFF2 |

Vive en `userland/web/`, se compila con la FPU x87 y **fuera de la libsagui común**
(que va sin coma flotante, D076): solo lo ligan el Cipó y el programa de prueba,
como el motor de PDF (D138).

### Lo que se queda nuestro

El motor es un renderizador puro: **no habla con la red**. La red la sigue haciendo
`userland/libsagui/http.c` y `userland/libtls/`, que validan los certificados (el TLS
del repositorio de origen nunca se auditó). También son nuestros el historial, el
buscar, los formularios, las imágenes, la barra de direcciones y todo el resto de la
interfaz. El motor recibe la página por un gancho (`baixar`, `imagem`, `tela`) que el
Cipó da.

La capa de adaptación es nuestra: `userland/web/web.h`, `js.h` y `canvas2d.h` (la API
que el motor espera) y `userland/web/web_compat.c` (lo que el motor necesita del
browser: imágenes en ARGB, el pedido preparado del `fetch` y el pote de cookies).

### El crédito

El repositorio de origen **no tiene fichero LICENSE**. El código queda registrado en
`docs/terceros.md` (D141) junto con las fuentes DejaVu y las tablas de las normas, y
la procedencia está detallada en `userland/web/ORIGEM.md`.

### Segunda parte: por trozos, botón derecho y robustez

Lo que faltaba para que el navegador se pueda usar de verdad:

- **El render va por bandas.** El bucle de eventos pinta una franja de la página por
  vuelta (hasta 64 px) y luego espera al siguiente evento, así que la ventana nunca se
  queda congelada con una página larga. Para que el hit-test (qué hay bajo el ratón)
  siga siendo cierto con la página a medio pintar, el motorCalcula la posición de
  cada banda al pintar (el `motor_rolagem()` del motor de origen, tocado por
  nosotros; está anotado en `userland/web/ORIGEM.md`).
- **Los recursos se piden de uno en uno.** Las imágenes y las hojas de estilo se
  descargan en vueltas sueltas del bucle, no todas de golpe: el aviso de «Cargando…»
  se enciende antes y se apaga cuando no queda nada por pedir.
- **El botón derecho abre un menú contextual** con «Guardar imagen» y «Guardar enlace».
  Lo que se guarda va a `/home` si esa carpeta existe (la partición de datos, que es
  persistente) y a `/tmp` si no existe: antes la descarga fallaba siempre en un sistema
  sin partición de datos.
- **Los nombres se sanean**: el nombre del fichero sale del final de la URL y solo
  quedan letras, números, punto, guion y guion bajo; el resto se cambia por `_`
  (`/foto uno.png` → `foto_uno.png`).
- **Las URL se codifican**: `url_separar()` ya no rechaza las direcciones con espacios,
  las codifica en `%XX` (`/foto uno.png` → `/foto%20uno.png`). Esto arregla también el
  `wget` y el `curl` con nombres de fichero con espacios.

La prueba es `tests/h114_cipo_descargas.py`.

### Tercera parte: filtro CSS y animaciones (D148)

El motor pinta como se le pide, y ahora também sabe **filtrar** e **animar**:

- **`filter`**: `grayscale(v)`, `brightness(v)`, `opacity(v)` e `blur(px)`
  aplicam-se ao pintar o fundo, a borda e o texto. O texto herda o filtro do
  contentor (o do antepassado mais proximo que declara `filter`). O desfoque
  é uma aproximacao por caixa.
- **`@keyframes` com `animation`**: anima a cor, a cor de fundo, as cores da
  borda e a opacidade, com interpolacao entre os quadros. O motor web é um
  renderizador puro — nao sabe que horas são — por isso ganha a funcao nova
  `bool motor_tick(motor_t *m, unsigned ms)`, que o Cipo chama em cada volta do
  laço de eventos com a hora do SO. Devolve `true` quando alguma animacao
  mudou (e então basta repintar a faixa visivel, sem refazer o layout).

As provas sao `tests/h120_motor_filtro_anim.py` e
`tests/h122_motor_img_tam.py` (esta ultima tambem cobre o `width`/`height` do
HTML na caixa da imagem).

### Quarta parte: o motor de JavaScript é o QuickJS (D149)

O motor de JavaScript que veio do outro repositorio (com DOM, eventos, Canvas e
WebGL) **nao era usado pelo navegador**: so pela copia de referencia. As paginas
nao executavam nada. Agora o motor de JavaScript e de terceiros: o **QuickJS**
(bellard/quickjs, MIT), portado ao SaguiOS, e cada `<script>` da pagina corre
com um **DOM minimo**:

- `document.title` (ler e escrever): muda o titulo da pagina e da aba;
- `document.body.innerHTML` e `document.body.textContent`: o corpo da pagina
  (ao escrever, monta-se um documento novo e volta a maquetar);
- `console.log`/`info`/`warn`/`error`: ao registo do sistema;
- `alert`: ao registo e ao pe da janela;
- `setTimeout`/`clearTimeout`: com o relogio do SO.

Cada pagina nova tem o seu motor (as variaveis de uma pagina nao passam para a
outra). O porte ao SaguiOS (sem libc) esta em `userland/web/quickjs/`: as
cabeceras de reemplazo (`inc/`), `qjs_port.c` (a memoria, as cadeias, o
relogio, as mates e o formatador) e `qjs_bridge.c` (o DOM). O SO tem de
executar o contexto do QuickJS numa **pilha de 2 MB**: a pila do programa (64
KB) nao chega para o parser.

As provas sao `tests/h125_quickjs.py` (o motor: 18 scripts) e
`tests/h128_cipo_js.py` (o navegador: uma pagina servida por HTTP com um
`<script>`).

## Criterios de aceptación

- [x] El motor compila y enlaza dentro del SaguiOS
- [x] `tests/h112_motor_web.py`: carga un HTML, lo maqueta y lo pinta (18 nodos, 16 comandos de pintura, 158 px de alto, 60 689 píxeles pintados)
- [x] El Cipó usa el motor: `tests/h113_cipo_real.py` (texto real, enlace azul y subrayado, `motor_alvo` sigue el enlace)
- [x] Las fuentes del sistema (DejaVu recortadas) en `rootfs/fontes/`, con su licencia
- [x] Decisión registrada en `docs/decisoes.md`: D141
- [x] Que el navegador no se congele con una página grande: trabajo por trozos y proceso ayudante para las descargas
- [x] Botón derecho para guardar una imagen o un enlace al disco
- [x] Robustez: ninguna página puede tumbar el navegador (memoria, tiempos de espera, HTML disparatado)
- [x] `tests/h114_cipo_descargas.py`: los recursos se cargan de uno en uno, el aviso de carga sale antes, el botón derecho ofrece «Guardar imagen» y «Guardar enlace», y lo guardado lleva el nombre saneado
- [x] `tests/h120_motor_filtro_anim.py`: o `filter` pinta (cinza, brilho, opacidade, desfoque) e a animacao muda com o instante
- [x] `tests/h122_motor_img_tam.py`: o `width`/`height` do HTML mandam na caixa da imagem (e o CSS manda sobre o atributo)
- [x] `tests/h125_quickjs.py`: o QuickJS roda no SO (aritmetica, cadeias, mates, objetos, arrays, setas, fechos, classes, JSON, regex, data, console, alert, document e temporizadores)
- [x] `tests/h128_cipo_js.py`: as paginas do Cipo correm JavaScript (o console, o `document.title`, o `document.body.innerHTML` e o `setTimeout` da pagina)

## Notas

**Los hilos no caben aquí.** El SaguiOS no tiene hilos de usuario ni SMP (el SMP está
planeado para la v9), así que el multiproceso se resuelve con el trabajo por trozos: el
bucle de eventos nunca se queda sin responder y las descargas van a un proceso hijo,
con lo que ya tiene el SO (`executar`, `criar_pipe`, `disponivel`). Los hilos de usuario
de verdad (`pthread` sencillo) siguen siendo una mejora del SO para la v10.

**Lo que el motor de origen todavía no hace** (y por tanto el Cipó tampoco): sin
HTTP/2 ni compresión, sem WebSocket, sem `transition` (só `animation`), sem
animacao multipla, com o filtro fora dos gradientes, das imagens e do SVG, e
com um motor de JavaScript que e um interprete de arvore (lento sem KVM).
Seu TLS e seu cliente HTTP nao foram trouxidos: os nossos sao melhores.
