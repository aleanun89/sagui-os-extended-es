# IA local: el motor `llm` (D380, mejora 139, desde la v8.5.4.4)

Motor propio en C para ejecutar modelos de lenguaje GGUF dentro de SaguiOS. Ayuda para el usuario:
`rootfs/ayuda/llm.md` (`/ayuda/llm.md`). Decisión: D380 en `docs/decisoes.md`.

## Ficheros

| Fichero | Qué hace |
|---|---|
| `userland/llm/llm.h` | API del motor (GGUF, cuantización, modelo, tokenizador, muestreo) |
| `llm_plat.c` | plataforma: SaguiOS (libsagui) o el PC (`LLM_ANFITRIAO`, libc) |
| `llm_mat.c` | expf, sqrtf, senos... con la x87 (en el PC, la libm o la x87 con `LLM_MAT_X87`) |
| `llm_gguf.c` | lector GGUF v2/v3; la cabecera en memoria, los pesos en un mapa de páginas |
| `llm_quant.c` | bloques Q4_0/Q8_0/Q4_K/Q6_K: decuantizar, cuantizar la entrada (Q8_K, Q8_0), productos en C, elección de ruta |
| `llm_quant_sse2.c`, `llm_quant_ssse3.c` | los mismos productos con SSE2 (`pmaddwd`) y SSSE3 (`pmaddubsw`) |
| `llm_modelo.c` | la red LFM3, la caché K/V en Q8_0 y la multiplicación por lotes |
| `llm_token.c`, `llm_unicode.h` | BPE gpt2 con el pretokenizador de llama3; tablas Unicode de `tools/llm_unicode.py` |
| `llm_amostra.c` | temperatura, top-k, top-p, repetición, semilla (xorshift64*) |
| `llm_principal.c`, `llm_programa.h` | el programa: opciones, chat, `--info`, modelos, memoria |
| `llm_anuncio.c` | `llm --anunciar`: logo «IA Powered» (SIS_FUNDO `FUNDO_MARCA_IA`) |
| `llm_servidor.c` | `llm --servidor`: API compatible con OpenAI en 127.0.0.1, con herramientas e imágenes (fase 2) |
| `llm_ferramentas.c/.h` | fase 2: la plantilla de chat de LFM3 (herramientas, llamadas, respuestas, imágenes) y la lectura de los `<tool_call>` (compila `userland/ia/json.c`) |
| `llm_visao.c` | fase 2: el proyector de imágenes de LFM3-VL (`mmproj`, `--imagen`) |
| `userland/ia/modelos.c`, `userland/inc/ia_modelos.h` | fase 2: el descargador (`ia descargar`, Configuración > IA local); lista en `rootfs/etc/ia/modelos.lista` |

## La red LFM3

Referencia: llama.cpp (`src/models/lfm3.cpp` y `lfm-base.cpp`, b12088 o posterior). Por bloque: norma RMS; si la
capa no tiene cabezas K/V, **convolución corta** (`in_proj` da b, c, x; `y = c · conv1d(b · x)` con un núcleo de 7 por
canal y un estado de 6 posiciones; `out_proj`); si las tiene, **atención** GQA (16 cabezas de 128, 4 de K/V) con
normas RMS de Q y K por cabeza, RoPE NEOX (base 1e6 y factores `rope_freqs` en las completas; 1e4 en las de ventana
de 256), `o *= sigmoide(attn_gate · h)` por cabeza y `attn_output`; norma posterior; residuo; FFN SwiGLU con su norma
posterior; residuo. Salida atada al embedding (Q6_K).

## Caché y memoria

K y V se guardan cuantizados en Q8_0 (34 bytes por 32 valores). Las capas completas guardan `contexto` posiciones;
las de ventana un anillo de `256 + LLM_LOTE` (un lote entero cabe sin pisar lo que aún se mira). LFM3 1.7B:
1088 bytes por posición y capa: 79 MB a 12k, 155 MB a 24k, 308 MB a 48k. Antes de cargar se comprueba que pesos +
caché + trabajo + tokenizador caben en el hueco del proceso (~1,44 GB) y en la memoria libre.

## Rutas de cálculo

La entrada de cada multiplicación se cuantiza (Q8_K para filas K, Q8_0 para Q4_0/Q8_0) y el producto es de enteros:
C, SSE2 y SSSE3 dan los mismos enteros. La aritmética de float va por la x87 en todos los ficheros (también en el
SSE): así las tres rutas y el motor del PC dan los mismos tokens. En el PC (Ryzen 9900X, un hilo, LFM3 1.7B):
SSSE3 10,8 tok/s, SSE2 7,6 tok/s, C 1,9 tok/s.

## Comparar con llama.cpp (en el PC)

    python3 tools/llm_comparar.py --compilar                      # baja y compila llama.cpp fuera de git
    python3 tools/llm_comparar.py input_share/modelos/LFM3-1.7B-Q4_K_M.gguf --q8 -n 16
    python3 tools/llm_comparar.py --sintetico -n 30

llama.cpp (MIT) vive en `~/.local/saguitools/llama.cpp` (cmake de un venv, `~/.local/saguitools/llmvenv`).
`tools/llm_referencia.c` pasa los tokens uno a uno y vuelca los logits. Resultado con el LFM3 real y la caché q8:
los mismos tokens, top-1 igual en 38 de 40 posiciones, coseno mínimo 0,995.

## Seguridad del lector

Un `.gguf` es un fichero de fuera: el lector comprueba todo antes de usarlo (dimensiones, desplazamientos, textos
del vocabulario, hiperparámetros, vocabulario contra embedding; revisión de D380). La prueba de regresión en el
PC compila el motor con ASan+UBSan y le pasa GGUF mutados, textos largos y peticiones malas al servidor:

    python3 tests/llm_lector_asan.py

## Pruebas

- `tools/llm_modelo_prueba.py SALIDA.gguf [--grande]`: LFM3 sintético con semilla (8 bloques, ventana de 8, las
  cuatro cuantizaciones, BPE propio); `--grande` tiene una caché que no cabe a 48k.
- `make llm-pc`: el motor para el PC (`build/host/llm-pc`), con la x87 como en SaguiOS.
- `tests/h388_llm.py`: en QEMU, tokens iguales a los del PC por cada ruta, límites de contexto (y sin SSSE3 con
  `LLM_SIN_SSSE3=1`), error de memoria, logo con 2 GB (captura `H388-ia-powered`) y nada con 1 GB.
- `tools/llm_modelo_prueba.py SALIDA.gguf --mmproj`: proyector sintético de LFM3-VL para el modelo sintético.
- `tests/h412_ia_fase2.py`: la plantilla (contra la jinja del GGUF real si está), la lectura de llamadas, el
  descargador contra un servidor HTTPS del anfitrión (redirección larga, trozos, corte y reintento, `.parte`, suma
  mala, sin sitio), la página de Configuración (capturas `H412-ia-local-*`) y, en QEMU, la visión y el servidor con
  herramientas e imágenes iguales al PC.
- `tools/llm_visao_referencia.c`: la referencia de llama.cpp (libmtmd) para los vectores de una imagen (solo PC).

## Modelos

Nunca dentro del sistema: `/home/compartido/modelos`, `~/modelos` o la carpeta de `LLM_MODELOS`. El activo, en
`~/.config/llm/modelo`. En el PC del que compila, `input_share/modelos` (no va a la imagen); para QEMU, por disco
aparte o pendrive virtual.

## Servidor local

`llm --servidor [--puerto 8080]` carga el modelo y atiende `GET /v1/models` y `POST /v1/chat/completions` (con y sin
`stream`, `max_tokens`, `temperature`, `seed`) solo desde 127.0.0.1. En Configuración > IA, el proveedor «llama.cpp»
(`http://127.0.0.1:8080/v1`) ya apunta ahí: `ia`, `chatia` y los agentes lo usan sin cambios, **con herramientas**
(fase 2, ver abajo). Entre peticiones se conserva la caché si
la conversación nueva empieza por lo que ya está (las respuestas anteriores se escriben con su `<think></think>`
vacío, como el modelo las generó): en un chat solo se lee lo nuevo. Probado en el PC con el LFM3 real (dos turnos;
el segundo, 16 tokens nuevos y 34 reutilizados).

## Herramientas (fase 2)

Los agentes (D295/D298: `ia --agente`, Pluma, Macaco, Correo) mandan `tools` (formato de OpenAI), `tool_choice`
«auto» y, en la historia, mensajes `assistant` con `tool_calls` (argumentos como texto JSON) y mensajes `tool`.
`llm_ferramentas.c` escribe la conversación igual que la plantilla jinja del GGUF de LFM3 (`tokenizer.chat_template`):

- las herramientas, al final del sistema: `\n\n<tools>\n{...}\n{...}\n</tools>`, cada una como `json.dumps` de
  Python (separadores `", "` y `": "`, sin `\uXXXX` para lo que no es ASCII) y sin `defer_loading`;
- cada llamada: `<tool_call>\n<function=NOMBRE>\n<parameter=CLAVE>\nVALOR\n</parameter>\n...</function>\n</tool_call>`
  (varias, separadas por `\n\n`; los valores de texto tal cual, los demás en JSON);
- cada respuesta: `<|im_start|>tool\n<tool_response>\nTEXTO\n</tool_response><|im_end|>`.

Lo que genera el modelo se mira al vuelo: el texto se manda hasta `<tool_call>` (nunca la marca ni lo que podría ser
su principio) y al acabar cada `<tool_call>` vuelve como `tool_calls` (`finish_reason: "tool_calls"`; en streaming,
un delta con `index`, `id`, `name` y `arguments`). Los argumentos llevan el tipo del esquema: si el parámetro es
`string`, texto; si no, el valor si es JSON válido (número, booleano, objeto...). `tool_choice: "none"` quita las
herramientas; `"required"` o `{"type":"function","function":{"name":X}}` dejan empezada la llamada
(`<tool_call>\n<function=X>\n`) para que el modelo la siga. Los `<tool_call>` y demás marcas son tokens
«definidos por el usuario» (tipo 4) del vocabulario: no paran la generación. Probado en el PC con el LFM3 real: pide
`listar_carpeta` con `{"ruta": "/home/ana/documentos"}` y, con la respuesta de la herramienta, contesta con la lista
(el segundo turno reutiliza 157 tokens de la caché). `llm --probar-plantilla PETICION.json` escribe la conversación y
`llm --probar-respuesta PETICION.json RESPUESTA.txt` las llamadas que se leen (sin modelo; para las pruebas).

## Visión: LFM3-VL (fase 2)

El `mmproj` de LFM3-VL (arquitectura `clip`, proyector `lfm3vl`, 194 MB en Q8_0) **no es un SigLIP**: es un
codificador propio de 14 bloques que alterna convolución 3x3 por canal (como la convolución corta del texto, pero en
2D) y atención con RoPE 2D. La referencia es llama.cpp (`tools/mtmd/models/lfm3vl.cpp`, `mtmd-image.cpp`):

1. **Tamaño:** `smart_resize` de Qwen2-VL: lados múltiplos de 32 (parche 16 x fusión 2), entre `image_min_pixels`
   (65536) y el máximo; el modelo admite hasta 4096x4096, pero aquí el máximo de serie son **144 vectores**
   (~384x384, `--imagen-max N` hasta 1024) porque el coste crece con el cuadrado en la atención.
2. **Reescalado:** bicúbico con antialias (núcleo de Keys a=-0,5, ancho escalado al reducir), separable, en punto
   fijo de 22 bits con los pesos redondeados a 16 bits como torchvision; escrito aquí (no es código de Pillow).
3. **Normalización:** `(x/255 - 0,5)/0,5` y redondeo a f16 (como el im2col de ggml).
4. **Parches 16x16:** dos núcleos (`v.patch_embd.weight` y `.weight.1`, los dos cuadros de un vídeo); una foto llena
   los dos y se suman. Norma RMS (`v.embedding_norm`).
5. **Bloques:** norma RMS; convolución: `in_proj` -> `[a | m | b]`, `g = a·b`, convolución 3x3 por canal con ceros
   alrededor, `out_proj(m·g)`; atención: Q/K/V (16 cabezas de 64, 8 de K/V), norma RMS de Q y K por cabeza, RoPE 2D
   (parejas `(i, i+32)`: las 16 primeras giran con la fila y las 16 siguientes con la columna, base 1e6), atención
   completa, `attn_out`; FFN SwiGLU. Residuos.
6. **Proyector:** LayerNorm con sesgo (eps 1e-6), cada 2x2 parches juntos (`[p(2i,2j), p(2i,2j+1), p(2i+1,2j),
   p(2i+1,2j+1)]`, 4096), `mm.0` + sesgo, GELU (erf), `mm.2` + sesgo = un vector de 2048 por cada 2x2 parches.

Los vectores entran en la caché entre `<|vision_start|>` y `<|vision_end|>` (`llm_avaliar_emb`), en posiciones
seguidas. Las multiplicaciones van como las del texto (entrada en Q8_0, productos de enteros por la mejor ruta de la
CPU, también AVX2, D381): SaguiOS y `llm-pc` dan los mismos números. **Comparado con llama.cpp** (libmtmd,
`tools/llm_visao_referencia.c`): con el proyector real, 80 vectores de una imagen de 320x240, coseno medio 0,999 y
mínimo 0,972; con el sintético de las pruebas (otra forma: D=32, 234 vectores), mínimo 0,9997. Con el LFM3-VL 2B real
describe bien la imagen de prueba («un círculo rojo centrado sobre un fondo que transita del azul al verde»); en el PC
(AVX2, 4 hilos) codificarla lleva 3 s.

Uso: `llm --imagen FICHERO -p "..."` (o en el chat, con la primera pregunta) y, en el servidor, partes
`{"type":"image_url","image_url":{"url":"data:image/...;base64,..."}}` en el `content` (como mucho 4 imágenes; nada de
`file://`: el servidor no lee ficheros por encargo de otro). El `mmproj` se busca solo (`mmproj-BASE-*.gguf` junto al
modelo, BASE = el nombre del modelo sin la cuantización) o con `--mmproj`; `--mmproj no` no lo carga. En SaguiOS lee
todo lo que lee la libsagui (PNG, JPEG, BMP, PNM, GIF); `llm-pc` solo PPM y BMP de 24 bits. Memoria: los 194 MB del
proyector se suman a los del modelo (con 12k cabe).

## Descargador (fase 2)

`ia descargar` (y Configuración > IA > «IA local: modelos», que lanza `ia descargar ID --estado FICHERO` y pinta la
barra con lo que ese fichero dice) baja los modelos de `/etc/ia/modelos.lista`: `id | nombre | fichero | bytes |
sha256 | url | RAM | fichero que va con este | nota`. Tamaños y sumas (el `lfs.oid` de la API de Hugging Face,
comprobado el 2026-11-19, y con `sha256sum` los tres que había en el PC): LFM3 1.7B Q4_K_M, QAD-Q4_0 y Q8_0, LFM3-VL 2B
Q4_K_M, QAD-Q4_0 y Q8_0 (con su `mmproj` Q8_0). No hay Q4_0 «a secas» en los repositorios de Liquid AI: el Q4_0 es el
QAD. Los Q8_0 (1,8 GB) aún no caben en el proceso de `llm` (~1,44 GB, falta la fase de pesos por ventanas).

- Por **trozos de 32 MB con `Range`**: cada respuesta mide menos de 10^9 bytes, un corte solo pierde su trozo, se
  reintenta 5 veces (con pausa creciente) desde el último byte escrito y, de una sesión a otra, se sigue desde
  `NOMBRE.gguf.parte`.
- Hugging Face redirige a una URL firmada de más de 1 KB: la petición y la cabecera se leen con búferes propios (no
  caben en `url_t`); si la firma caduca (403/410), se vuelve a pedir la original; de HTTPS a HTTP no se sigue.
- Antes de empezar: el espacio libre del sistema de archivos de la carpeta (lo que falta + 1 MB) y un aviso si el
  equipo tiene menos RAM que la recomendada. Al acabar, SHA-256 del fichero entero; si no coincide, se borra.
- Ficheros de más de 2 GB: error claro (los `posicionar`/`info_arquivo` del sistema son de 32 bits).
- Probado contra Hugging Face de verdad en QEMU (2026-11-19): el `mmproj` del VL, con el TLS propio, la redirección a
  `us.aws.cdn.hf.co` (Location de ~1,1 KB) y los trozos con `Range` (206 con `Content-Range`): 64 MB en menos de
  90 s, hasta que el `/tmp` en RAM se llenó (ese ramfs tiene su propio tope, que el cálculo de espacio libre no ve;
  en `/home/compartido/modelos`, en disco, no pasa).

## Hilos y estimación

De serie, un hilo por CPU que trabaja (`INFO_NUCLEOS`; sin la imagen SMP, uno). Las filas de cada multiplicación y
las cabezas de la atención se reparten; el resultado es idéntico con cualquier número de hilos. En el PC: 1 hilo
10,8 tok/s al leer y 10,5 al generar; 4 hilos 29,4 y 19,1. `llm --info` mide 0,3 s de productos Q4_K y estima los
tok/s y el tiempo de leer el contexto elegido, y avisa si pasa de 5 minutos.

## Medidas

| Dónde | Ruta | Hilos | Lectura | Generación | Notas |
|---|---|---|---|---|---|
| PC (Ryzen 9 9900X), `llm-pc` | SSSE3 | 1 | 10,8 tok/s | 10,5 tok/s | LFM3 1.7B Q4_K_M |
| PC | SSSE3 | 4 | 29,4 tok/s | 19,1 tok/s | |
| PC | SSE2 | 1 | 7,6 tok/s | 7,6 tok/s | |
| PC | C (x87) | 1 | 1,9 tok/s | 1,9 tok/s | |
| SaguiOS en QEMU/KVM (2 GB, CPU de QEMU sin SSSE3) | SSE2 | 1 | 6,6 tok/s | 5,6 tok/s | contexto 12k; carga de 1,1 GB desde un pendrive USB virtual: ~70 s |

Leer el texto va casi a la misma velocidad que generar: con SSE las multiplicaciones están limitadas por el
cálculo, no por la memoria, y el lote solo ahorra lecturas de pesos. Leer 12k tokens enteros llevaría unos
30-40 minutos en un núcleo así: el contexto grande sirve para conversaciones que crecen poco a poco (el servidor y
el chat reutilizan la caché), no para pegar un documento de 12k de golpe. Lo que más acelera: varios núcleos
(imagen SMP) y AVX2 (D381).

## Pendiente

Pesos por ventanas (PAE) para 48k con cualquier modelo y para los Q8_0, perfil para equipos antiguos (bloques para
la L2). Visión: vídeo (dos cuadros por paso), imágenes grandes por mosaico, medir en hardware real; con
`--imagen-max` alto la atención (cuadrática) es lo que más pesa. Ver mejora 139 y D380.
