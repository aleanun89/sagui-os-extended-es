# Brumacanto

Juego de exploración, minería y construcción en primera persona para un jugador.
Está escrito en C11, usa SDL 2.0 y dibuja con OpenGL ES-CM 1.1, búferes de
vértices y el pipeline fijo. Incluye el código, los gráficos y las instrucciones
de compilación.

## Jugar

Descomprime el ZIP completo. Los ejecutables necesitan la carpeta `assets` en el
mismo directorio.

- **Linux x86-64 (glibc 2.34 o posterior).** Ejecuta `./brumacanto` desde una terminal. Necesitas SDL2 y
  un proveedor EGL/GLES con soporte para el perfil **ES-CM 1.1**. En Debian o
  Ubuntu, los paquetes del lado de la aplicación son
  `libsdl2-2.0-0 libegl1 libgles1`.
- **Windows x64.** Ejecuta `brumacanto.exe`. El paquete incluye `SDL2.dll`.
  Para el contexto gráfico hace falta un proveedor EGL con OpenGL ES 1.1, por
  ejemplo, una instalación por aplicación de Mesa3D para Windows. Véanse las
  [instrucciones de Mesa3D para Windows](https://github.com/pal1000/mesa-dist-win/).
  Usa DLL x64 y conserva los archivos del proveedor junto a `brumacanto.exe`.
  El ejecutable se ha compilado para Windows x64. La prueba de ejecución
  se ha realizado en Linux.

En el menú puedes editar la semilla y las tres dimensiones haciendo clic en
cada campo o pasando entre ellos con **Tab**. La semilla admite números o
texto. La anchura (X) y el fondo (Z) se eligen por separado entre **160 y
32 768 bloques**, en múltiplos de 32. La altura (Y) admite valores entre
**48 y 128**. El tamaño inicial es de **4096 × 4096 × 48**.

La partida depende de la semilla y de las dimensiones. Al guardar, el juego
recuerda la última configuración para que puedas continuar desde el menú.
También puedes abrir un mundo desde la terminal:

```sh
./brumacanto --seed Sierra --size 8192x4096x96
```

`--size 8192` crea una planta cuadrada de 8192 × 8192 con la altura por defecto.
`--size 8192x4096` fija anchura y fondo con esa misma altura.

### Controles

| Tecla o botón | Acción |
|---|---|
| WASD y ratón | Desplazarse y mirar |
| Espacio y Mayús | Saltar o nadar hacia arriba, y correr |
| Clic izquierdo | Mantener para minar. Pulsar para atacar a una criatura |
| Clic derecho | Colocar el bloque seleccionado o comer fruta |
| 1–9 o rueda | Elegir un objeto de la barra |
| E | Abrir la mochila y la fabricación |
| F | Comerciar mirando a un aldeano |
| F2 | Alternar entre supervivencia y creativo |
| F5 | Guardar |
| Esc | Pausa con controles y opciones de guardado |

En la mochila puedes elegir un objeto con las flechas o el ratón y asignarlo a
un hueco con **1–9**. **Tab** cambia a las recetas y **Enter** fabrica la
seleccionada. Con ratón, pulsa una receta para seleccionarla y vuelve a pulsar
para fabricarla. En creativo tienes acceso a los materiales de construcción
desde la mochila.

### Qué encontrarás

- **3 animales.** Zorro, jabalí y liebre. Este último huye cuando te acercas.
- **4 tipos de NPC.** Los aldeanos cambian tablas por fruta o ámbar por faroles.
  El Umbrío te persigue, el Coloso golpea con más fuerza y el Candil Errante
  lanza proyectiles.
- **28 tipos de bloque**, incluidos terreno, minerales, dos maderas, cristal,
  agua, banco de trabajo, faroles y roca madre. El agua se puede colocar en
  creativo y la roca madre es indestructible.
- **4 herramientas.** Pico para roca y minerales, hacha para madera, pala para
  terreno suelto y espada para combatir. Hay **12 recetas**.
- Cuevas, lagos, vegetación, nieve, ciclo día/noche y sonido sintetizado. La
  aldea está al norte del inicio. Su mina está al oeste, cerca de **X 68, Z 87**.
  Los faroles iluminan las caras cercanas. Los enemigos detectan desde más lejos
  en la oscuridad.

El terreno se genera en sectores de 16 × 16 conforme exploras. El juego
mantiene en memoria los sectores cercanos y conserva los que has modificado.
Solo escribe esos cambios de terreno en el guardado, junto con la posición,
la salud, la hora, la mochila y la barra rápida. Las criaturas vuelven a
generarse al cargar.

Para ampliar una partida de la versión de **160 × 160 × 48**, introduce su
semilla, elige las dimensiones nuevas y pulsa **Continuar mundo**. Se
conservan los bloques originales y la posición. Al guardar, se crea una
partida con las dimensiones elegidas y se mantiene el archivo anterior.

La ubicación
predeterminada en Linux es
`~/.local/share/Brumacanto/Brumacanto/` y en Windows
`%APPDATA%\Brumacanto\Brumacanto\`.

## Compilar

### Linux

En Debian o Ubuntu:

```sh
sudo apt install build-essential pkg-config libsdl2-dev libegl1 libgles1
make -B
make test
./brumacanto
```

El repositorio incluye las cabeceras de OpenGL ES 1.1 de Khronos. Los símbolos
gráficos se resuelven en ejecución a través de SDL, así que el enlace del juego
solo necesita SDL2 y la biblioteca matemática del sistema.

### CMake

Con SDL2 2.x instalada, o indicando con `-DSDL2_DIR=...` la carpeta que
contiene `SDL2Config.cmake` o `sdl2-config.cmake`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake copia el atlas junto al binario. Puedes regenerarlo con
`python3 tools/build_atlas.py` si tienes Pillow y editas los PNG.

Para identificar el proveedor gráfico, ejecuta `./brumacanto --smoke-test`.
La prueba abre un contexto, imprime la versión y dibuja el menú, el mundo, la
mochila, la pausa y una zona lejana. Admite `--size 32768x32768x128`.
En una sesión sin monitor, SDL 2.32 permite seleccionar
`SDL_VIDEODRIVER=offscreen`. El render se ha probado en Linux con Mesa 22.3.6
(llvmpipe). Algunas compilaciones de Mesa omiten GLES 1.1 y pueden devolver
`EGL_BAD_ALLOC` aunque funcionen GLES 2 y 3. En ese caso se necesita un
proveedor compilado con `gles1=enabled`.
[Documentación de Mesa](https://docs.mesa3d.org/opengles.html) ·
[Incidencia de Debian #1073812](https://bugs.debian.org/1073812).

## Licencias y procedencia

- Código de Brumacanto: MIT. Véase `LICENSE.txt`.
- Texturas e iconos de **Voxel Pack**, de Kenney Vleugels, CC0 1.0. Los PNG
  utilizados y la licencia original están en `assets/kenney/`. Se han
  redimensionado las piezas para el atlas y ajustado el canal alfa de cristal
  y agua. Los modelos de las criaturas, el farol y el sonido son propios.
  [Ficha de Kenney](https://kenney.nl/assets/voxel-pack/) ·
  [Descarga de OpenGameArt](https://opengameart.org/content/voxel-pack).
- Cabeceras GLES de Khronos: MIT. Véanse `third_party/khronos/` y el
  [registro de OpenGL](https://github.com/KhronosGroup/OpenGL-Registry).
- SDL2 para Windows: licencia zlib. Véanse `third_party/sdl2/LICENSE.txt` y
  [SDL 2.32.10](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10).
