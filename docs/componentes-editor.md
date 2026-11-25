# Componentes de editor de la libsagui (D454, v8.5.6)

Componentes **generales** para programas de tipo editor (los necesita el editor del motor de juegos Capibara, pero sirven a
cualquier aplicación). Declarados en `userland/inc/gui_editor.h` (incluido por `gui.h`), implementados en
`userland/libsagui/gui_*.c` (lista base de `libsagui.so.1`). Siguen el patrón de siempre: una estructura con la posición y el
estado, `*_desenhar` y `*_tratar`; usan `mod_tema` (D423) y el esquema vigente (D428), así que cambian con el estilo del
sistema. **Sin coma flotante**: enteros y punto fijo. Demo y banco de pruebas: `/bin/demoeditor` y `tests/h454_componentes_editor.py`.

Convenciones: coordenadas de ventana en píxeles; `*_tratar` devuelve qué pasó (constantes `*_NADA`, `*_MUDOU`...) y se
redibuja solo si cambió algo; los eventos de teclado los debe recibir **solo el componente con el foco** (el programa lo
decide: `demoeditor` guarda el último componente donde se hizo clic). Los textos de los componentes se traducen con
`idioma_traducir_sl` (catálogo `libsagui`).

## 1. Paneles divisibles (`gui_paineis.c`)

Un árbol de nodos: hojas (con el `id` de la aplicación) y divisiones `PD_LADO` (a | b) o `PD_PILHA` (a sobre b).

```c
paineis_t pn; paineis_iniciar(&pn, x, y, l, a);
int n1 = paineis_folha(&pn, H_ARBOL), n2 = paineis_folha(&pn, H_VISTA);
paineis_dividir(&pn, PD_LADO, n1, n2, 200 /* milésimas */, 100 /* mín. a */, 200 /* mín. b */, 1 /* plegable: lado a */);
paineis_calcular(&pn);                           // o paineis_mover(&pn, x, y, l, a) al redimensionar
paineis_rect(&pn, H_ARBOL, &x, &y, &l, &a);      // false = plegada (tamaño 0)
paineis_desenhar(j, &pn);                        // solo los separadores (7 px); el contenido es de la aplicación
int r = paineis_tratar(j, &pn, &e);              // PAINEIS_ARRASTA / SOLTOU / PLEGOU: repintar los contenidos
```

- El último nodo que se crea es la raíz (`paineis_raiz` para cambiarla). Hasta `PAINEIS_MAX` (24) nodos.
- **Tamaños mínimos** en píxeles de cada lado; si no caben los dos mínimos se reparte en proporción.
- **Plegar**: `plegavel` = 1 pliega el lado a, 2 el lado b. La pastilla central del separador (o doble clic) pliega y despliega;
  arrastrar un separador plegado lo despliega. `paineis_plegar(&pn, nodo, true)` lo hace por programa.
- **Guardar y restaurar**: `paineis_guardar(&pn, buf, max)` da `"pos,plegado;..."` (una entrada por división, en el orden de
  creación) y `paineis_restaurar(&pn, txt)` lo repone (false si el número de divisiones no coincide).

## 2. Inspector de propiedades (`gui_inspetor.c`)

Rejilla nombre/valor con secciones plegables. El programa rellena un vector `ins_prop_t` con los constructores:

| constructor | editor |
|---|---|
| `inspetor_secao(p, nome, aberta)` | cabecera plegable (las propiedades siguientes cuelgan de ella) |
| `inspetor_int(p, nome, v, min, max, paso)` | campo numérico: flechas, arrastre horizontal (4 px = un paso), teclear |
| `inspetor_fixo(p, nome, v, decimais, min, max, paso)` | punto fijo: el valor va multiplicado por 10^decimais (1250 con 2 = 12,50) |
| `inspetor_bool(p, nome, v)` | casilla (clic o espacio) |
| `inspetor_enum(p, nome, sel, opcoes, n)` | desplegable (menú contextual del sistema) |
| `inspetor_deslizador(p, nome, v, min, max)` | deslizador |
| `inspetor_texto(p, nome, texto)` | texto |
| `inspetor_cor(p, nome, 0xAARRGGBB, com_alfa)` | muestra + selector de color |
| `inspetor_arquivo(p, nome, caminho, filtro)` | campo + botón «..» (`dialogo_abrir`) |

```c
inspetor_t ins; inspetor_iniciar(&ins, x, y, l, a, props, n);
ins.mudou = ao_mudar;          // void (*)(void *ctx, inspetor_t *, int indice): el valor ya está en props[indice]
int r = inspetor_tratar(j, &ins, &e);   // INSPETOR_MUDOU (ins.ultimo = propiedad) / INSPETOR_SECAO
```

Límites con `min < max` (en `INT`, `FIXO`, `DESLIZ`). Teclado con el foco (`ins.foco`): arriba/abajo cambian de propiedad,
izquierda/derecha suman o restan un paso, Intro edita o abre el editor del tipo, Esc cancela el texto. `ins.codigo` es el código
de `menu_contextual` de los desplegables (único en la ventana). `inspetor_definir_int` cambia un valor por programa y avisa;
`inspetor_valor_texto` da el texto de un valor («12,50», «Sí»...).

## 3. Selector de color (`gui_selcor.c`)

`seletorcor_t` (260 x 208) incrustable: paleta de 30 colores, cuadro de saturación/brillo, barra de tono, barra de alfa
(`com_alfa`) y tres deslizadores RGB, con vista previa y hexadecimal. `seletorcor_iniciar/definir/cor/desenhar/tratar`
(`SELCOR_MUDOU`). `seletor_cor_dialogo(titulo, &argb, com_alfa)` lo pone en una ventana modal con Aceptar y Cancelar (lo usa
el inspector). `cor_rgb_para_hsv` y `cor_hsv_para_rgb` son enteras (tono 0-359, S y V 0-255).

## 4. Lienzo con zoom y desplazamiento (`gui_vista.c`)

Vista 2D genérica que **no guarda objetos**: la aplicación dibuja en coordenadas del mundo.

```c
edvista_t v; edvista_iniciar(&v, x, y, l, a);       // zoom 1000 (milésimas), rejilla de 16, origen en el centro
v.desenhar = cb_dibujar;   // void (*)(void *ctx, jan_t *, edvista_t *): usar edvista_ret / edvista_moldura / edvista_linha / edvista_texto
v.acertar  = cb_acertar;   // int (*)(void *ctx, int wx, int wy): objeto bajo el punto del mundo o -1
v.aviso    = cb_aviso;     // void (*)(ctx, ev, a, b, c, d, fim)
```

- **Zoom** con la rueda, centrado en el ratón (x1,25 por muesca; de `EDVISTA_ZOOM_MIN` 62 a `EDVISTA_ZOOM_MAX` 16000). `edvista_zoom`.
- **Desplazar**: botón central, o Espacio (con el ratón encima y el foco) y arrastrar con el izquierdo. `edvista_centrar`.
- **Rejilla** configurable (`v.grade`, 0 = ninguna; se agranda sola si queda demasiado fina) e **imán** (`v.iman`, `edvista_iman`).
- **Selección por rectángulo**: arrastrar en vacío. Avisos: `EDVISTA_E_CLIQUE` (a = id o -1, b/c = punto del mundo),
  `EDVISTA_E_RET` (a,b,c,d = x0,y0,x1,y1 del mundo).
- **Asas** de un rectángulo seleccionado (`edvista_selecionar(&v, x, y, l, a, rot)`): arrastrar el cuerpo lo **mueve** (imán),
  las 8 asas lo **escalan** (con el borde contrario fijo, también girado) y el asa de arriba lo **gira** (grados; Mayús = de 15 en
  15). Avisos `EDVISTA_E_MOVER` / `ESCALAR` (a,b,c,d = x,y,l,a) y `GIRAR` (a = grados); `fim` = true al soltar (momento de
  registrar el deshacer). También `EDVISTA_E_ZOOM` y `EDVISTA_E_DESLOCAR`.
- Conversión: `edvista_para_tela` / `edvista_para_mundo`. Límite: coordenadas del mundo de hasta ±60000 con el zoom máximo.

## 5. Paleta de casillas (`gui_paleta.c`)

Una imagen (`imagem_t`) dividida en celdas (`edpaleta_iniciar(&p, x, y, l, a, img, cel_l, cel_a)`; `p.escala` 1-4) o celdas
dibujadas por la aplicación (`desenhar_celda`). Clic elige una, arrastrar un bloque rectangular, Mayús + clic lo amplía, la rueda
desplaza, las flechas mueven la selección con el foco. `edpaleta_selecao` (normalizada), `edpaleta_selecao_indices` (fila a fila),
`edpaleta_definir_selecao`. `EDPALETA_ESCOLHEU` al soltar.

## 6. Editor de código con resaltado (`gui_codigo.c`)

- **Resaltador por tabla** (`edrealce_regras_t`): comentarios de línea y de bloque, cadenas (con escapes) y cadenas largas, palabras
  clave, tipos, funciones, constantes, números, preprocesador (`#` en C), llamadas `nombre(`. Tablas incluidas: `edrealce_lua` y
  `edrealce_c`; otro lenguaje es otra tabla. `edrealce_linha(r, linea, n, estado, clases)` clasifica una línea (`RC_*`) y devuelve el
  estado de la siguiente (0, comentario de bloque, cadena larga); `edrealce_cor(clase)` da el color del esquema vigente.
- **Componente** `codigo_t` (`codigo_criar` ... `codigo_liberar`): texto Latin-1 (hasta 4 MB), números de línea, línea actual,
  selección con ratón y teclado, portapapeles (Ctrl+A/C/X/V), **sangría automática** (conserva la de la línea, la aumenta tras
  `then`/`do`/`else`/`repeat`/`function ...`/`{` y la reduce al escribir `end`/`else`/`until`/`}`; Tab inserta hasta el siguiente
  múltiplo), **deshacer y rehacer** (Ctrl+Z / Ctrl+Y, los tecleos seguidos se funden), **buscar** (Ctrl+F: incremental, Intro
  siguiente, Mayús+Intro anterior, Esc cierra) y **error marcado**: `codigo_marcar_erro(c, linha, msg)` pinta la línea y su mensaje.
- API: `codigo_definir/texto/regras/buscar/ir_linha/linha/coluna/linhas/desfazer/refazer/modificado/inserir/selecao`.
- Pluma es un procesador de textos con formato (RTF) y no tiene resaltado; el resaltador de código que había era el de **Mico**
  (editor de terminal), que sigue con el suyo. Esta pieza es la versión por tabla para programas con ventana.

## 7. Deshacer y rehacer genérico (`gui_desfazer.c`)

Pila de órdenes (`pilha_desfazer_t`). Se registra una orden **después** de ejecutarla, con la función que la deshace y la rehace:

```c
ed_desfazer_registrar(&p, "Mover", aplicar /* void (*)(void *dados, bool refazer) */, liberar, dados);
ed_desfazer_agrupar_inicio(&p, "Pintar"); ...varios registrar...; ed_desfazer_agrupar_fim(&p);   // un solo paso
ed_desfazer(&p); ed_refazer(&p);   // bool; desfazer_pode/refazer_pode; desfazer_nome/refazer_nome (para el menú)
```
Registrar borra lo rehacible; la pila recuerda `max` pasos. `desfazer_topo` da los datos de la última orden (para fundir tecleos).

## 8. Arrastrar y soltar (`gui_desfazer.c`)

Entre componentes del mismo programa. Quien empieza llama a `arrsol_preparar(&item, x, y)` en el clic (elemento: `tipo` > 0,
hasta 64 bytes de `dados`, `rotulo` que se ve junto al puntero); se activa al moverse más de 4 px. Los destinos se registran
en cada repintado con `arrsol_alvos_limpar()` y `arrsol_alvo(x, y, l, a, cb, ctx)`; el callback recibe `ARRSOL_ENTRA`,
`MOVE`, `SAI` y `SOLTA` y devuelve si **acepta** el elemento (el rótulo se pone verde y el destino puede resaltarse).
`arrsol_tratar(j, &e)` va primero con cada evento: si no devuelve `ARRSOL_NADA` el evento es suyo (`ARRSOL_ATIVO` mientras dura,
`ARRSOL_FIM` al soltar, `ARRSOL_CANCELOU` con Esc). El rótulo se dibuja guardando y restaurando lo de debajo.

## 9. Consola de salida (`gui_consola.c`)

`consola_t`: anillo de 256 líneas con nivel `CON_MSG` / `CON_AVISO` / `CON_ERRO` (colores del tema), cabecera con tres filtros
con contador, **Copiar** (lo visible al portapapeles) y **Limpiar**, y seguimiento del final. `consola_escrever(c, nivel, texto)`;
`consola_escrever_linha(c, nivel, linha, texto)` añade una remisión a una línea de código: el doble clic devuelve `CONSOLA_IR`
con `c->ir_linha`. `consola_boton` da el rectángulo de cada botón (para pruebas).

## Demo y pruebas

`/bin/demoeditor`: árbol | lienzo y código | inspector y, abajo, recursos y consola (la paleta con una hoja de casillas dibujada
por el propio programa). Escribe «demoeditor: ...» en la serie con cada acción. `tests/h454_componentes_editor.py` lo maneja con el
ratón y el teclado: separador, mínimos, plegar y guardar/restaurar; clic, mover, escalar, girar y rectángulo; deshacer, rehacer y
grupos; arrastrar y soltar; zoom y desplazamiento (Espacio y botón central); todos los editores del inspector; el selector de
color; la paleta; el código (resaltado, sangría, error marcado, buscar, deshacer); y los filtros de la consola. Capturas
`H454-*` en `docs/capturas/`.

## Gráficos (D469)

El componente de gráficos (`grafico.h`, `gui_grafico.c`) no es de editor pero sigue el mismo patrón de estructura con estado; usa coma
flotante (x87) y dibuja en un búfer de píxeles. Su API está en `docs/grafico.md`.
