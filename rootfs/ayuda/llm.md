# IA local: `llm`

`llm` ejecuta **modelos de lenguaje en este equipo**, sin red ni servidores: el motor es
propio de SaguiOS (D380, desde la v8.5.4.4) y lee modelos GGUF. El primero que sabe ejecutar es **LFM3 1.7B**
de Liquid AI (Q4_K_M, ~1 GB).

## Qué hace falta

- **Más de 1,8 GB de memoria.** Con menos, `llm` lo dice y no carga nada.
- **Un modelo** (`.gguf`) en `/home/compartido/modelos` o en `~/modelos`. Los modelos nunca
  vienen con el sistema ni se guardan dentro de él: se bajan en Configuración > IA o se copian
  ahí desde un pendrive.

Cuando las dos cosas se cumplen, el escritorio muestra el logo **IA Powered** abajo a la izquierda.

## Uso

    llm                          chat con el modelo (/nuevo empieza otra conversación, /salir sale)
    llm -p "¿Qué es un sagui?"   una sola pregunta
    llm --info                   el modelo, su cuantización y cuánta memoria necesita
    llm --modelos                los modelos que hay (* = el activo)
    llm --activar FICHERO.gguf   el que se usa sin -m
    llm -m FICHERO.gguf ...      otro modelo solo esta vez

## Bajar modelos

En **Configuración > IA > IA local: modelos** está la lista de modelos que se pueden bajar
(LFM3 1.7B y LFM3-VL 2B en Q4_K_M, Q4_0 y Q8_0, y el proyector de imágenes del VL): elige uno y
pulsa **Descargar**; la barra enseña el avance. **Usar este modelo** lo deja como el activo. Lo
mismo en el terminal:

    ia descargar                     la lista (y lo que ya está bajado o a medias)
    ia descargar lfm3-1.7b-q4km      baja ese modelo a /home/compartido/modelos
    ia descargar lfm3-vl-2b-q4km --activar   el VL con su proyector, y lo deja activo

La descarga va por HTTPS desde Hugging Face, comprueba antes que haya sitio en el disco y al
final la **suma SHA-256** (si no coincide, borra lo bajado). Si se corta (o la cancelas), lo bajado
se queda en `NOMBRE.gguf.parte` y la próxima vez **sigue desde ahí**. Los Q8_0 aún no caben en la
memoria de `llm` (hará falta otra fase). La lista está en `/etc/ia/modelos.lista`.

## Imágenes (modelos VL)

Con un modelo **LFM3-VL** y su proyector (`mmproj-...gguf`, en la misma carpeta) `llm` ve imágenes:

    llm -m /home/compartido/modelos/LFM3-VL-2B-Q4_K_M.gguf --imagen foto.png -p "¿Qué hay en la foto?"

Lee PNG, JPEG, BMP, GIF y PPM. La imagen se reduce a unos 384x384 (144 vectores; `--imagen-max N`
cambia cuántos, hasta 1024, que es más lento): en un equipo antiguo tarda de unos segundos a un
par de minutos. El servidor (`llm --servidor`) también acepta imágenes en los mensajes.

## Contexto

El contexto es cuánto texto recuerda el modelo (pregunta, respuestas y lo anterior):

    llm --contexto 24k

- Con un procesador con **SSSE3** (casi todos desde 2006-2011): de **12k** (de serie) a **48k**.
- **Sin SSSE3** (Core Duo, Pentium 4, Athlon 64): de **4k** (de serie) a **48k**; se recomienda
  4k-8k y modelos Q4_0, porque leer mucho texto será lento.

Más contexto necesita más memoria (`llm --info` lo calcula: con LFM3 1.7B, 1,1 GB a 12k y
1,3 GB a 48k) y tarda más en leer textos largos.

## Otras opciones

    --pensar              deja que el modelo razone antes de contestar (más lento)
    --sistema TEXTO       instrucciones para toda la conversación
    -n N                  como mucho N tokens de respuesta (512 de serie)
    --temp T --top-k K --top-p P --repeticion R --semilla S
    --hilos N             varios hilos (sirve con la imagen SMP y varios núcleos)
    --crudo               sin plantilla de chat: continúa el texto de -p

Al terminar, `llm` dice cuántos tokens por segundo ha leído y generado.

## Para los demás programas

    llm --servidor

deja el modelo cargado y lo ofrece como un servidor compatible con OpenAI en
`http://127.0.0.1:8080/v1` (solo para este equipo). En Configuración > IA, elige el proveedor
**llama.cpp**: `ia`, el chat con ventana y los asistentes de Pluma, Macaco y Correo lo usarán,
**también con sus herramientas** (leer ficheros, buscar, proponer cambios...: el modelo pide la
herramienta y el asistente la ejecuta, con tu permiso como siempre).

## Velocidad

`llm --info` mide la CPU y estima cuántos tokens por segundo dará y cuánto tardará en leer el
contexto elegido. Con varios núcleos (la imagen SMP) usa uno por núcleo (`--hilos N` lo cambia).

## Para no anunciarlo

    servicio desactivar llm
