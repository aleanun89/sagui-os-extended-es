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
**llama.cpp**: `ia`, el chat con ventana y los asistentes de Pluma, Macaco y Correo lo usarán.

## Velocidad

`llm --info` mide la CPU y estima cuántos tokens por segundo dará y cuánto tardará en leer el
contexto elegido. Con varios núcleos (la imagen SMP) usa uno por núcleo (`--hilos N` lo cambia).

## Para no anunciarlo

    servicio desactivar llm
