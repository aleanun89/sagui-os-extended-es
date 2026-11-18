# `input_share/`: lo que quien compila mete en su imagen

`input_share/` es una carpeta en la raíz del repositorio para el contenido **propio de quien compila
la distribución** que no puede ir en el repositorio ni bajarse con un guion: la versión completa de
un juego que ha comprado, una aplicación con una licencia que no permite redistribuirla, sus propios
datos. Está en `.gitignore`: **nada de lo que haya ahí va nunca a git**. Si no existe, la compilación
es la de siempre. Decisión: D228.

## Estructura

```
input_share/
  games/
    <carpeta>/      ->  /jogos/<carpeta>   (la partición de juegos SAGUI-JOGOS)
  apps/             ->  /apps              (la partición de aplicaciones SAGUI-APPS)
  modelos/          ->  (no va a la imagen) modelos de IA locales (D380)
```

- **`modelos/`** (D380) guarda los modelos de lenguaje (`.gguf`) del que compila, para medir el motor `llm`
  en el PC (`tools/llm_comparar.py`) o en QEMU **como disco aparte o pendrive virtual**. El `make` no la
  copia nunca: los modelos no van dentro del sistema (en SaguiOS viven en `/home/compartido/modelos` o
  `~/modelos`, ver docs/llm.md). Tampoco se ponen en `apps/`: inflarían SAGUI-APPS y SAGUI-RAIZ.

- **`games/<carpeta>/`** se copia, tal cual y con sus subcarpetas, a `/jogos/<carpeta>/` de la
  partición `SAGUI-JOGOS` (ext2 de lectura y escritura, montada en `/jogos`). Por ejemplo, la versión
  completa de Jazz Jackrabbit (la de GOG) en `input_share/games/Jazz/` acaba en `/jogos/Jazz`, y
  OpenJazz la encuentra sola (busca en todas las carpetas de `/jogos` y prefiere la versión completa
  al shareware). Si la carpeta se llama como una del sistema (`opentyrian`, `openjazz`), su contenido
  se pone encima del de serie.
- **Juegos de DOS (D294):** cualquier carpeta de `input_share/games/` con un `.exe`, `.com` o `.bat` en su raíz
  (`input_share/games/DOOM/DOOM.EXE`) acaba en `/jogos/DOOM` y **Recreativa la muestra sola** (con su programa, que
  puede cambiarse en Propiedades); DOSBox la monta como C:. Otras carpetas conocidas del sistema (`chip8`, `sagui3d`,
  `jclic`, `opentyrian`, `openjazz`, `quake`, `conquer`, `gnuboy`) no cuentan. Los datos son tuyos: no van a git ni a las
  imágenes que se publican (`input_share/` está ignorada y la compilación publicable la deja vacía). Para tenerlos
  en el C: por defecto (`/home/dos`), cópialos a mano o con Archivos; la lista de Recreativa los guarda en
  `/home/dos/juegos.conf`. Guía: `docs/dosbox.md`.
- **ROM de consolas (D321):** `input_share/games/nes`, `sms`, `gg` (y los sistemas que se vayan registrando) acaban en `/jogos/nes|sms|gg`
  y Recreativa las muestra solas por extensión (`.nes`, `.sms`, `.sg`, `.gg`). Nunca van a git ni a las imágenes publicadas. Guía:
  `docs/recreativa.md`.
- **Quake (D229):** los datos van en `input_share/games/Quake/id1/pak0.pak` (y `pak1.pak` de la
  versión registrada), la estructura de Quake, y acaban en `/jogos/Quake/id1/`. También vale el
  `pak0.pak` suelto, `input_share/games/Quake/pak0.pak`: el lanzador toma la carpeta como el `id1`.
  Los nombres pueden ir en mayúsculas (`ID1/PAK0.PAK`, como en el CD de DOS). Las expansiones van
  al lado, en su carpeta (`input_share/games/Quake/hipnotic/`, `.../rogue/`) y se arrancan con
  `quake -hipnotic` o `-rogue`. La configuración y las partidas no van ahí: Quake las escribe en
  `/home/quake/id1`. El shareware se baja aparte con `tools/quake_datos.sh` (va a `/jogos/quake`).
- **`apps/`** se copia encima de `/apps` de la partición `SAGUI-APPS`: un ejecutable en
  `input_share/apps/bin/` sale en `/apps/bin` (que está en el `PATH`) y sus datos en
  `input_share/apps/<app>/`. Con `make APPS=0` va dentro del ramdisk, en las mismas rutas.
- Las marcas de descarga de Windows (`*:Zone.Identifier`) no se copian.

## `games/gb/`: ROM de Game Boy y Game Boy Color

Las ROM (`.gb` y `.gbc`) para el emulador **GNUBoy** (D235) no vienen con el sistema: son de quien las
pone. Las de `input_share/games/gb/` van, como todo `games/<carpeta>/`, a **`/jogos/gb`**, que es donde
el emulador las busca primero (también en `/home/gnuboy/roms` y en `jogos/gb`, `gb` o `roms` de los
discos montados). Al abrir **Inicio > Juegos > Game Boy (GNUBoy)** sale una lista con el nombre que
lleva cada cartucho en su cabecera; sin ROM, una ventana explica dónde ponerlas. Las partidas (`.sav`,
la batería del cartucho) y los estados quedan en `/home/gnuboy`. En **Archivos**, abrir un `.gb` o un
`.gbc` lo arranca directamente.

```
input_share/games/gb/juego.gb      ->  /jogos/gb/juego.gb
```

Solo con ROM de juegos que se tenga derecho a usar. Para probar, `tools/gnuboy_roms_libres.sh` baja a
`userland/terceros/gnuboy/roms-libres/` (ignorada por git) una ROM libre (Libbet, licencia zlib) que se
puede copiar ahí; la prueba usa una ROM propia, generada por `tools/gnuboy_rom_teste.py`.

## `games/CnC/`: Command & Conquer (las ISO de C&C Gold)

Una excepción a «se copia tal cual»: si `input_share/games/CnC/` tiene **ISO**, no se copian (son
1,2 GB y casi todo es de Windows) sino que `tools/cnc_extraer.py` saca de ellas lo que usa Vanilla
Conquer (D230). Las ISO son las de **Command & Conquer Gold para Windows 95** que EA regaló en
2007, una por disco; se reconocen por su volumen, no por su nombre:

```
input_share/games/CnC/
  CnC_GDI95.iso     (volumen GDI95, 608 MB)
  CnC_NOD95.iso     (volumen NOD95, 592 MB; opcional: sin ella, solo la campaña de GDI)
```

Lo que acaba en `/jogos/CnC` (63 MB):

- de la raíz de la ISO: `CONQUER.MIX`, `DESERT.MIX`, `TEMPERAT.MIX`, `WINTER.MIX`, `SOUNDS.MIX` y
  `SCORES.MIX` (la música);
- del instalador (`INSTALL/SETUP.Z`, un archivo de InstallShield 3 que la herramienta
  descomprime): `UPDATE.MIX`, `UPDATEC.MIX`, `CCLOCAL.MIX`, `LOCAL.MIX`, `SPEECH.MIX`,
  `TRANSIT.MIX`, `DESEICNH.MIX`, `TEMPICNH.MIX` y `WINTICNH.MIX`;
- de cada disco, sus misiones: `gdi/GENERAL.MIX` y `nod/GENERAL.MIX`;
- con `make CNC_VIDEOS=1`, también las películas: `gdi/MOVIES.MIX` y `nod/MOVIES.MIX` (840 MB más:
  súbase la partición, `JOGOS_MB=2000`). Sin ellas el juego se salta las escenas;
- un `LEEME-SAGUIOS.TXT` con la lista.

El resto (el instalador, los `.EXE` y `.DLL`, DirectX, Acrobat Reader, los vídeos de presentación,
los manuales) no se copia. La extracción se guarda en `build/jogos/cnc` y no se repite si las ISO no
cambian. A mano: `tools/cnc_extraer.py --buscar input_share/games/CnC /ruta/destino` (`--listar
ISO` enseña lo que hay dentro). Sin las ISO, también vale copiar a `/jogos/CnC` la carpeta de un
C&C95 instalado (con `GENERAL.MIX` en la raíz o en `gdi/` y `nod/`).

## Compilar

Basta con poner los ficheros y hacer `make` (o la imagen que se quiera): el `Makefile` vigila lo que
hay en `input_share/games` y `input_share/apps` (los nombres, tamaños y fechas, con
`tools/marca_carpeta.py`) y rehace las particiones cuando algo cambia. Otra carpeta:

```bash
make INPUT_SHARE=/ruta/a/mi_input_share
make INPUT_SHARE=          # sin input_share, aunque exista
```

En una copia de trabajo (`miniOS_master_os_*`) se puede usar la del árbol principal con
`INPUT_SHARE=../miniOS_master/input_share` (no hace falta copiarla ni enlazarla).

El tamaño de la partición de juegos: `saguios.img` (y `-smp`, `-uacpi`) la llevan del tamaño justo
para lo que contiene (`JOGOS_IMG_MB=0`); las persistentes, la de particiones y el instalador, de
800 MB (`JOGOS_MB=800`), con sitio para meter más juegos desde el propio SaguiOS.

## Licencias: es responsabilidad de quien compila

- Lo que se mete en `input_share/` **no es del proyecto**: su licencia es la que sea, y es
  responsabilidad de quien lo pone. El proyecto no lo distribuye, ni en el repositorio ni en las
  versiones que publica.
- Una imagen compilada con `input_share/` es **para uso propio**: si contiene un juego comprado, no se
  puede compartir. Por eso `gerar_versao.sh` (las entregas de `entrega/`, que van a git y a GitHub)
  compila siempre **sin** `input_share/` y sin los datos de OpenJazz ni de Quake (`INPUT_SHARE=`,
  `OPENJAZZ_DATOS=0` y `QUAKE_DATOS=0`); `--con-input-share` los mete, con un aviso de que esa entrega no se puede
  publicar.
- Lo que sí se puede bajar con un guion no va aquí: tiene su propio guion y su carpeta ignorada (por
  ejemplo `tools/openjazz_datos.sh` para el shareware de Jazz Jackrabbit, `tools/quake_datos.sh`
  para el de Quake, `tools/netsurf_fuentes.sh`
  para las fuentes de NetSurf). Ver [terceros.md](terceros.md).
