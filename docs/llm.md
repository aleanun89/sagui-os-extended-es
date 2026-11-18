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
| `llm_servidor.c` | `llm --servidor`: API compatible con OpenAI en 127.0.0.1 (usa el lector JSON de `userland/ia/json.c`) |

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

## Modelos

Nunca dentro del sistema: `/home/compartido/modelos`, `~/modelos` o la carpeta de `LLM_MODELOS`. El activo, en
`~/.config/llm/modelo`. En el PC del que compila, `input_share/modelos` (no va a la imagen); para QEMU, por disco
aparte o pendrive virtual.

## Servidor local

`llm --servidor [--puerto 8080]` carga el modelo y atiende `GET /v1/models` y `POST /v1/chat/completions` (con y sin
`stream`, `max_tokens`, `temperature`, `seed`) solo desde 127.0.0.1. En Configuración > IA, el proveedor «llama.cpp»
(`http://127.0.0.1:8080/v1`) ya apunta ahí: `ia`, `chatia` y los agentes lo usan sin cambios. Las herramientas
(`tools`) aún no: se contesta 400 con «tools» y el cliente repite sin ellas. Entre peticiones se conserva la caché si
la conversación nueva empieza por lo que ya está (las respuestas anteriores se escriben con su `<think></think>`
vacío, como el modelo las generó): en un chat solo se lee lo nuevo. Probado en el PC con el LFM3 real (dos turnos;
el segundo, 16 tokens nuevos y 34 reutilizados).

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

Pesos por ventanas (PAE) para 48k con cualquier modelo, descargador en
Configuración, VL (`mmproj`), perfil para equipos antiguos (hilos por defecto, bloques para la L2, estimación de
tiempos). Ver mejora 139.
