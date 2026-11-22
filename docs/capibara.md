# Capibara: motor de juegos 2D (D452)

Capibara es un motor de juegos 2D **propio y sencillo** para SaguiOS (y Linux): un árbol de nodos, escenas en un fichero de texto
legible, señales entre nodos y guiones en **Lua 5.4**, con todos los nombres en español. Se inspira en las ideas de motores como
Godot (nodos, escenas, señales, guiones) pero tiene nombre, formato y API propios. Código **MIT** (`userland/capibara/LICENSE`), en C11,
sobre la SDL 2 de SaguiOS. Tiene también un **editor gráfico** (`capibara-editor`, D460; ver «El editor»).

    userland/capibara/
      motor/            el motor: nodo.c, texto.c, props.c, escena.c, recursos.c, fisica.c, guion.c, entrada.c, audio.c, dibujo.c, mundo.c
                        (núcleo, sin SDL) y dibujo_sdl.c, plataforma_sdl.c (la SDL)
      ejemplos/         plataformas/ y disparos/ (proyectos completos con gráficos CC0 de Kenney)
      motor/escritor.c  edición de escenas línea a línea (D460: lo usa el editor)
      editor/           el editor: editor.c, raster.c (rasterizador del lienzo), imagen_sdl.c
      pruebas/          prueba_nucleo.c (sin SDL), ejemplos.sh
      tools/            generar_sonidos.py, generar_nivel_plataformas.py
      Makefile          compilación para Linux   ·   capibara.mk   compilación para SaguiOS (/apps/bin/capibara)

## Cómo se usa

    capibara proyecto/                 una carpeta con capibara.cfg   (en SaguiOS: /apps/bin/capibara /apps/capibara/plataformas)
    capibara juego.capi                un paquete
    capibara --empaquetar proyecto/ juego.capi
    opciones: --pantalla-completa  --gpu=auto|si|no  --hud  --audio-prio=auto|si|no
              --cuadros N [--volcar f.ppm]   N cuadros de paso fijo sin esperar (determinista; imprime CAPI_HASH)
              --bench N                      N cuadros a toda velocidad; imprime CAPI_BENCH (cuadros/s)
              --entrada "12:derecha+;40:saltar+;41:saltar-"   (o CAPI_ENTRADA) pulsa y suelta acciones en cuadros dados
              --grabar-audio f.wav           graba lo que mezcla el audio
    teclas del motor: F3 HUD de rendimiento · F4 cajas de colisión · F11 o Alt+Intro pantalla completa · Esc salir

En SaguiOS están en Inicio > Juegos («Capibara: plataformas» y «Capibara: escuadrón») y en `/apps/capibara/`.

## Cómo se hace un juego

Un proyecto es una carpeta (o un `.capi`) con `capibara.cfg`, escenas `.esc`, guiones `.lua` y recursos (PNG, WAV). Las rutas de los ficheros son
**siempre relativas a la raíz del proyecto** (en `.esc`, en `.lua` y en las funciones de la API).

**capibara.cfg**

    [proyecto]
    nombre = "Mi juego"
    escena_principal = "escenas/nivel1.esc"
    semilla = 11                      ; semilla de math.random (el juego es reproducible)
    [pantalla]
    ancho = 320                       ; tamaño lógico: el juego se dibuja a esta resolución y se escala (bandas negras si no cabe)
    alto = 180
    escala_entera = no                ; si sí, el factor de escala es un entero
    fondo = "#5fb4ee"
    [fisica]
    gravedad = 900                    ; píxeles por segundo al cuadrado
    paso = 60                         ; pasos de física por segundo
    [acciones]                        ; nombre = ligaduras: tecla:Nombre  raton:izq|central|der  mando:boton  eje:leftx-  eje:lefty+
    saltar = tecla:Space tecla:Up mando:a
    izquierda = tecla:Left tecla:A mando:izquierda eje:leftx-

Las teclas llevan el nombre de la SDL (`Left`, `Space`, `A`, `Left Shift`...; sin distinguir mayúsculas) y hay alias en español (`izquierda`, `derecha`, `arriba`,
`abajo`, `espacio`, `entrar`). Botones de mando: `a b x y atras inicio lb rb arriba abajo izquierda derecha`; ejes: `leftx lefty rightx righty`.

**Una escena** (`.esc`) es un árbol de nodos en texto. Lo mejor es leer los ejemplos (`ejemplos/plataformas/escenas/`).

    # comentario (también ; )
    [Nodo2D Nivel]                                  [Tipo Nombre]: el primer nodo es la raíz de la escena
    guion = "guiones/nivel.lua"                     propiedades: clave = valores (números, "textos", si/no, listas)
    var.monedas_total = 24                          variable propia del nodo: en Lua será self.monedas_total
    [Sprite Dibujo padre=Mundo/Jugador]             padre = ruta desde la raíz de la escena (por defecto, la raíz)
    textura = "img/personajes.png"
    cuadricula = 9 3                                columnas y filas de fotogramas de la textura
    animacion = "andar" 9 si 0 1                    nombre, fotogramas por segundo, bucle, lista de fotogramas
    animar = "andar"
    [MapaCasillas Terreno padre=Mundo]
    textura = "img/tilemap.png"
    casilla = 18 18
    tamano = 80 14                                  celdas
    solidas = 0 1 2 3                               qué casillas son sólidas (sin esto, todas las no vacías de las capas sólidas)
    [capa suelo solida=si]                          cada capa son tantas filas como «tamano»; «.» es vacío, un número es el índice de casilla
    . . . . 5 5 . .
    [Instancia Moneda3 escena="escenas/moneda.esc" padre=Mundo]    otra escena dentro de ésta
    posicion = 100 50                               las propiedades que siguen se aplican a la raíz de la instancia
    [Conexion desde=Reiniciar senal=presionado hacia=. metodo=_reiniciar]    conecta una señal a un método de un guion

Los avisos de carga (propiedad desconocida, padre no encontrado...) salen en el registro con el fichero y la línea, y el juego sigue.

**Un guion** es un `.lua` que **devuelve una tabla de métodos**. Dentro de ellos, `self` es el nodo.

    local Jugador = {}
    function Jugador:_listo() self.vidas = 3 end                 -- cuando el nodo y sus hijos están en el árbol
    function Jugador:_fisica(dt)                                  -- a paso fijo (por defecto 60 por segundo)
      local dir = Entrada.eje("izquierda", "derecha")
      self.vx, self.vy = self:mover_y_deslizar(dir * 90, self.vy + Fisica.gravedad() * dt)
    end
    function Jugador:_proceso(dt) end                             -- una vez por cuadro dibujado
    function Jugador:_entrada(ev) end                             -- ev.tipo: "accion", "tecla", "raton", "raton_mov"
    return Jugador

El fichero se ejecuta **una sola vez** y la tabla la comparten todos los nodos que lo usan; lo propio de cada nodo va en `self` (`self.vidas`). Un error de Lua
se escribe con su traza (`[CAPI] error en ...`) y el juego sigue.

## Nodos

| Nodo | Para qué | Propiedades de la escena (además de las de Nodo2D) |
| --- | --- | --- |
| `Nodo` | agrupar, sin posición | `visible`, `procesar`, `guion`, `grupo` |
| `Nodo2D` | posición, giro y escala; la jerarquía los compone | `posicion` (o `x`,`y`), `rotacion` (grados), `escala` (o `escala_x`,`escala_y`), `z` (relativo al padre), `color` (`"#rrggbb[aa]"` o 3-4 números; se multiplica por los hijos), `interpolar` |
| `Sprite` | un fotograma de una textura con animación | `textura`, `cuadricula c f`, `fotograma`, `region x y w h`, `espejo_h`, `espejo_v`, `centrado`, `desplazamiento`, `animacion` (se repite), `animar`, `velocidad_anim` |
| `MapaCasillas` | rejilla de casillas por trozos de 16x16 y capas (hasta 6) | `textura`, `casilla w h`, `separacion`, `tamano celdas_x celdas_y`, `solidas`, `capa` y `mascara` (capas de colisión, como Cuerpo); secciones `[capa nombre solida=si]` |
| `Camara2D` | qué parte del mundo se ve | `actual`, `zoom`, `suavizado` (velocidad de seguimiento por segundo; 0 = instantáneo), `seguir` (ruta a un nodo), `limites izq arriba der abajo` + `usar_limites`, `desplazamiento` |
| `Etiqueta` | texto (UTF-8; fuente 5x7 con á é í ó ú ñ ü ¿ ¡ ç...) | `texto`, `escala_texto`, `alineado` (`izquierda`/`centro`/`derecha`), `ancho` (ajuste de línea), `sombra`, `fijo` (en la pantalla, sin cámara) |
| `Boton` | botón sencillo | `texto`, `tamano w h`, `fijo`, `deshabilitado`, `color_normal`/`color_encima`/`color_pulsado`; señal `presionado` |
| `Area` | detecta solapes, sin chocar | `forma` (`"caja" w h` o `"circulo" r`), `desplazamiento_forma`, `capa`, `mascara`, `vigilar`; señales `cuerpo_entro(c)`, `cuerpo_salio(c)`, `area_entro(a)`, `area_salio(a)` |
| `Cuerpo` | choca con casillas sólidas y cuerpos estáticos | `forma`, `desplazamiento_forma`, `capa`, `mascara`, `modo` (`estatico`, `cinematico`, `dinamico`), `vx`, `vy`, `escala_gravedad`, `rebote`, `friccion` |
| `Particulas` | emisor de partículas simples | `emitiendo`, `una_vez`, `cantidad`, `maximo`, `vida`, `vida_azar`, `velocidad min max`, `angulo`, `dispersion`, `gravedad`, `tamano_part ini fin`, `color_inicial`, `color_final`, `aditivo`, `textura`+`cuadricula`+`fotograma`, `semilla` |
| `ReproductorAudio` | efectos y música | `sonido` (WAV PCM de 8 o 16 bits), `volumen`, `bucle`, `musica`, `autoreproducir` |
| `Temporizador` | cuenta atrás | `tiempo`, `una_vez`, `autoinicio`; señal `fin` |

**Colisiones.** Cada `Cuerpo` y `Area` tiene `capa` (qué es) y `mascara` (con qué choca o qué detecta), como máscaras de bits (`capa = 2`, `mascara = 5`). Un `Cuerpo`
choca con las casillas sólidas de un `MapaCasillas` cuya `capa` coincida con su `mascara` y con los `Cuerpo` en modo `estatico`. Un `Area` detecta los
`Cuerpo` cuya `capa` esté en su `mascara` y las `Area` igual. Cajas (alineadas con los ejes, sin giro) y círculos. Un cuerpo `cinematico` lo mueve el guion con
`mover_y_deslizar` (se desliza por paredes y suelos); uno `dinamico` recibe gravedad y velocidad solo.

**Dibujo.** Los nodos se dibujan por `z` (acumulado por la jerarquía) y, a igual `z`, en orden de árbol. Los `Cuerpo` y la cámara se dibujan **interpolados** entre
dos pasos de física (`interpolar`; con `teletransportar` no hay salto). Un nodo invisible oculta a sus hijos. `Etiqueta` y `Boton` con `fijo = si` se dibujan
sobre todo, en coordenadas de pantalla (la pantalla lógica de `capibara.cfg`).

## API de Lua

Todo con nombres en español. Un nodo se usa con `:` (métodos) y `.` (propiedades y variables). Los campos que no son propiedades del nodo se guardan en el propio
nodo (`self.vidas = 3`). `tipo` e `id` son de solo lectura (no se pueden usar como variables propias).

**Funciones globales** (`Capibara`, `Entrada`, `Audio`, `Fisica`, `Tiempo`)

| Función | Qué hace |
| --- | --- |
| `Capibara.cargar_escena(ruta)` | instancia una escena (sin colgarla): `nodo` o `nil, error` |
| `Capibara.cambiar_escena(ruta)` | cambia de escena al final del cuadro |
| `Capibara.raiz()` | la raíz de la escena actual |
| `Capibara.nuevo(tipo, nombre)` | crea un nodo (`"Sprite"`, `"Area"`...); luego `agregar_hijo` |
| `Capibara.grupo(nombre)` | tabla con los nodos de un grupo |
| `Capibara.valido(x)` | `true` si `x` es un nodo que sigue vivo |
| `Capibara.pantalla()` | ancho y alto lógicos |
| `Capibara.hud(si)` / `Capibara.salir()` | HUD de rendimiento / salir del juego |
| `Entrada.presionada(a)`, `recien_presionada(a)`, `recien_soltada(a)` | estado de la acción `a` (en `_fisica` valen por paso; en `_proceso`, por cuadro) |
| `Entrada.fuerza(a)`, `Entrada.eje(neg, pos)` | 0..1 / -1..1 (teclas, mando analógico) |
| `Entrada.raton()`, `Entrada.raton_mundo()` | posición del ratón en la pantalla lógica / en el mundo |
| `Entrada.declarar(a, ligaduras)` | declara una acción desde el juego |
| `Audio.sonido(ruta, volumen, pan)` | dispara un efecto (devuelve si cupo) |
| `Audio.musica(ruta, volumen, bucle)`, `Audio.detener_musica()`, `Audio.volumen(v)` | música (una) y volumen general |
| `Fisica.gravedad([g])` | consulta o cambia la gravedad |
| `Fisica.punto_solido(x, y)` | ¿hay una casilla sólida o un cuerpo estático en ese punto? |
| `Tiempo.cuadros()`, `Tiempo.segundos()` | cuadros dibujados y segundos de juego |

**Métodos de todos los nodos**

`obtener_nodo(ruta)` (relativa: `"Hijo/Nieto"`, `".."`, o `"/Raiz/..."`) · `buscar(nombre)` (recursiva) · `hijos()` · `agregar_hijo(n)` · `quitar_hijo(n)` ·
`liberar()` (se borra al final del cuadro) · `instanciar(ruta_escena)` (`nodo` o `nil, error`; sin colgar) · `en_grupo(g)`, `agregar_a_grupo(g)`, `quitar_de_grupo(g)` ·
`conectar(senal, funcion)` o `conectar(senal, objeto, "metodo")` · `desconectar(senal)` · `emitir(senal, ...)` ·
`mover(dx, dy)` · `posicion_global()` → x, y · `poner_posicion_global(x, y)` · `teletransportar(x, y)` (sin interpolar) · `poner(propiedad, valores...)` (para las de varios valores:
`n:poner("forma", "circulo", 8)`).
Campos: `nombre`, `tipo`, `padre`, `id` y todas las propiedades de las tablas (`x`, `y`, `rotacion`, `visible`, `texto`...).

**Por tipo**

| Nodo | Métodos y campos |
| --- | --- |
| `Sprite` | `animar(nombre)`, `detener_anim()`, `animacion`, `fotograma`; señal `animacion_terminada(nombre)` (animaciones sin bucle) |
| `Cuerpo` | `mover_y_deslizar(vx, vy)` → vx, vy que quedan (0 en el eje bloqueado); mueve `v*paso` · `en_suelo`, `en_techo`, `en_pared` · `ultimo_choque()` → nodo, nx, ny · `vx`, `vy` |
| `Area` | `solapados()` → tabla con lo que hay dentro ahora |
| `Camara2D` | `hacer_actual()`, `seguir(nodo)`, `sacudir(amplitud, segundos)` |
| `MapaCasillas` | `celda(cx, cy[, capa])` → índice o `nil` · `poner_celda(cx, cy, capa, indice_o_nil)` · `mundo_a_celda(x, y)` · `celda_a_mundo(cx, cy)` (centro) · `tamano_mapa()`; la capa es un nombre o un número desde 1 |
| `Temporizador` | `iniciar([tiempo])`, `detener()`, `activo` |
| `ReproductorAudio` | `reproducir()`, `detener()` |
| `Particulas` | `emitir_particulas([n])` |

**Señales.** `n:conectar("senal", f)` llama a `f(args...)`; `n:conectar("senal", obj, "metodo")` llama a `obj:metodo(args...)`; `n:emitir("mi_senal", 1, 2)` emite una propia.
Las que emite el motor están en la tabla de nodos. Las de la escena (`[Conexion ...]`) llaman a un método del nodo destino con los argumentos de la señal.

**Eventos de `_entrada(ev)`:** `{tipo="accion", accion="saltar", presionada=true}`, `{tipo="tecla", tecla="space", presionada=true}`,
`{tipo="raton", boton=1, presionada=true, x=, y=}`, `{tipo="raton_mov", x=, y=}`.

## Rendimiento: lo aprendido, aplicado

| Idea | En Capibara |
| --- | --- |
| Atlas y lotes grandes (D436) | los nodos emiten cuadriláteros; se agrupan por textura y mezcla y van en una sola llamada `SDL_RenderGeometry` indexada (hasta 500 cuadriláteros por lote, 2000 vértices; el máximo de gpu3d es 3072). Un nivel entero son 1-3 lotes. Las texturas de un juego deben ser hojas (atlas): en la GPU de SaguiOS hay 32 huecos de textura y cada subida es entera |
| Por la GPU solo cuando conviene (D432) | `--gpu=auto` (por defecto) pide el renderizador acelerado solo a pantalla completa, donde la GPU presenta sin volver a la RAM; en ventana, traer el cuadro de la GPU a la RAM puede ser muy lento. `--gpu=si` lo fuerza (también en ventana; lo sirve la SDL de SaguiOS si hay 3D, por ejemplo `--gpu3d=simulado`) y `--gpu=no` fuerza el software |
| Vuelta a la CPU | el motor no sabe si hay GPU: pide el acelerado y la SDL de SaguiOS vuelve sola al software si no hay 3D o algo falla (el HUD, F3, dice el renderizador y el número de lotes) |
| Poco que dibujar | el lienzo es el tamaño lógico del juego (320x180...) y el sistema lo escala: se dibujan 57.600 píxeles, no 1280x1024. Las casillas se recortan a lo que ve la cámara |
| Audio independiente de los FPS (D431) | mezclador sin cerrojos y un hilo propio (en SaguiOS, el de la tarjeta con adelanto de la latencia del kernel y `PLAN_AUDIO` de alta prioridad, automática con la GPU; en el PC, el de la SDL) |
| Paso fijo | la física no depende de los cuadros: con pocos fps se hacen hasta 5 pasos por cuadro; con muchos, el dibujo interpola |
| Escenas e instancias | los ficheros `.esc` se leen en cada instancia: para muchos objetos (balas, partículas) conviene instanciar de una escena pequeña y reciclar nodos |

F3 enseña: fps y tiempo de cuadro, renderizador y lotes, cuadriláteros, texturas, nodos, pares de física, audio y, en SaguiOS, CPU y RAM. Medidas con el motor
dentro de QEMU (`tests/h452_capibara.py`; el QEMU emula la CPU y la «GPU» simulada dibuja con ella, así que **no dicen nada del hardware real**; medidas del 2026-11-21):

| Ejemplo | Dónde | Cuadros por segundo |
| --- | --- | --- |
| plataformas (130 cuadriláteros, 86 nodos) | PC, SDL «dummy», software | unos 7.000 |
| disparos (hasta 370 cuadriláteros) | PC, SDL «dummy», software | unos 5.800 |
| plataformas | QEMU, software (renderizador `saguios`) | 238-250 (peor cuadro 10-20 ms) |
| disparos, desde un paquete `.capi` | QEMU, software | 72-74 |
| plataformas | QEMU, `--gpu3d=simulado`, `--gpu=si` (`saguios-gpu`, 3 lotes y 704 triángulos por cuadro) | 7-9 (el driver simulado dibuja con la CPU del QEMU y trae cada cuadro a la RAM en ventana) |

A pantalla completa, en una máquina con 3D de verdad, el cuadro no vuelve a la RAM (D432): ahí es donde se espera que la GPU gane (aún sin medir en hardware).

## Compilar

- **SaguiOS:** `make` lo incluye (`userland/capibara/capibara.mk`; necesita las fuentes de Lua, `tools/lua_fuentes.sh`; `CAPIBARA=0` lo deja fuera). Va a `/apps/bin/capibara`,
  los ejemplos a `/apps/capibara/`.
- **El editor** (`capibara-editor`, `userland/capibara/editor/`) solo se compila para SaguiOS (usa la libsagui para la ventana y los componentes); en Linux se compila el motor, y el escritor de escenas (`motor/escritor.c`) se prueba en `prueba_nucleo`.
- **Linux:** `make -C userland/capibara` (SDL2, SDL2_image y las fuentes de Lua 5.4; `LUA_SRC=` o `LUA=sistema`). `make -C userland/capibara prueba` pasa las pruebas sin ventana.
  Con eso se puede publicar la carpeta como repositorio aparte (MIT).

## Pruebas

`tests/h452_capibara.py [nucleo] [ejemplos] [fuentes] [saguios] [gpu]`: el núcleo sin SDL (`prueba_nucleo.c`: texto, escenas, instancias, señales, física, guiones, entrada,
mapas, animación, partículas, mezclador, paquete y determinismo), los ejemplos con la SDL «dummy» (hash del último cuadro igual en dos ejecuciones y desde el
paquete, eventos del juego, audio grabado), licencias y documentación, y QEMU (software y `--gpu3d=simulado`, con capturas `H452-*` y cuadros por segundo).

## El editor (D460, v8.5.6.1)

`/apps/bin/capibara-editor` (Inicio > Programación, icono propio; ayuda en `rootfs/ayuda/capibara-editor.md`) es el editor visual de
proyectos Capibara, hecho con los componentes de editor de la libsagui (D454: paneles divisibles, inspector, selector de color, lienzo con
zoom, paleta, editor de código con resaltado, deshacer, arrastrar y soltar, consola). `capibara-editor [carpeta] [--ejemplo=plataformas|disparos]`.

**Qué hace.** Árbol de la escena | lienzo | inspector, y abajo recursos, paleta de casillas y consola; el editor de Lua se despliega bajo el
lienzo al abrir un guion. Menús y barra de herramientas (guardar, abrir, deshacer/rehacer, mover/girar/escalar, pintar y goma de casillas,
rejilla e imán, jugar, parar, exportar). Árbol: añadir nodo por tipo, borrar, duplicar, renombrar, reordenar y cambiar de padre arrastrando
(antes, dentro o después), instanciar otra escena (`[Instancia]`; también soltando un `.esc` o una textura sobre el lienzo). Lienzo:
selección con clic (lo de dentro de una instancia se elige como la instancia) y con rectángulo, asas para mover (con imán a la rejilla),
girar y escalar, zoom y desplazamiento, cajas de colisión; con un `MapaCasillas` elegido, pintar con la paleta (un bloque entero a la vez) y
borrar casillas por capas. Inspector: generado de la tabla de propiedades del motor, más variables `var.*` y señales (`[Conexion]`).
Guiones: editor de Lua con resaltado, plantilla (`_listo`, `_proceso`) para el guion de un nodo, comprobación de sintaxis al guardar. **Jugar**
lanza `capibara` en otro proceso (con la escena abierta si se quiere, `--escena`) y vuelca su salida en la consola; un error de Lua se marca
con su línea y el doble clic abre el guion en ella. **Exportar** crea un `.capi`. Proyecto nuevo con escena y guion de plantilla; los dos
ejemplos se copian a la carpeta del usuario y se editan ahí.

**Cómo está hecho (decisiones de diseño).**
- **El documento es el texto de la escena** (`motor/escritor.c`, `EscDoc`): una lista de líneas más un análisis de secciones (`[Tipo Nombre
  padre=...]`, `[capa ...]`, `[Conexion ...]`, con su primera y última línea y los comentarios pegados encima). Cada orden (poner o quitar una
  propiedad, añadir, borrar, duplicar, renombrar o mover un nodo, pintar una casilla, añadir una capa o una conexión) cambia solo las líneas que le
  tocan: guardar sin cambios es byte a byte igual, y el resto del fichero (comentarios, orden, espacios, finales de línea CRLF) se conserva. El
  cargador no tuvo que cambiar. Deshacer es una instantánea del texto (las escenas son pequeñas), con fusión de las ediciones seguidas del mismo
  campo y un solo paso por trazo o gesto.
- **El lienzo es el motor**: de ese texto se instancia la escena con `escena_instanciar_texto` en *modo editor* (`g_modo_editor`: sin guiones ni
  sonidos) y se dibuja con `dib_empezar_cuadro` y una **cámara libre** (`dib_camara_libre`, que sustituye a la del juego); los cuadriláteros se
  pasan a un rasterizador por software (`editor/raster.c`) y se copian a la ventana. No se duplica el dibujo, la cámara ni las casillas. Elegir
  nodos usa `nodo_caja_local` y las matrices del motor; mover, girar y escalar cambian el nodo vivo mientras dura el gesto y al soltar se escriben las
  propiedades al texto (`posicion`, `rotacion`, `escala`) y se recarga la escena.
- **El inspector sale de `props.c`** sin código por nodo: `prop_listar` da cada propiedad con su tipo (`PK_*`), límites, etiquetas de los
  componentes y opciones, y `prop_texto` el valor por omisión en el formato del fichero; lo que ya está en la escena se lee del texto.
- **Jugar** usa el propio ejecutable del motor en otro proceso (tubería para la salida y los errores), así el juego es el de verdad.

**Límites del editor.** Sin editor de animaciones (`animacion = ...` se edita a mano; el inspector la oculta), sin depurador paso a paso ni
pausa del juego, una sola escena abierta a la vez, las texturas de un mapa con `separacion` se enseñan sin separar en la paleta, los
nodos dentro de una instancia se editan en la escena de esa instancia, y los textos largos (más de 90 caracteres) del inspector son de solo lectura.
La prueba es `tests/h460_capibara_editor.py` (el núcleo del escritor en el PC y el editor en QEMU con ratón y teclado).

## Límites actuales del motor

Límites: sin giro en mapas y formas, sin plataformas móviles o de un solo sentido, los cuerpos cinemáticos no chocan entre sí, una música a la vez y solo WAV
(PCM 8/16 bits; el decodificador Vorbis de SaguiOS podría añadirse), una única cámara actual, fuente 5x7 incorporada (sin TrueType), sin pausa de árbol ni
capas de interfaz con anclajes, sin red.

Pendiente para más adelante: editor de animaciones, depurador (pausa y paso a paso: el bucle ya separa los pasos), plataformas móviles y giro en formas.
