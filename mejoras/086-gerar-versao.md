# 086 - Un guion que genera la versión y deja la entrega montada

- **Estado:** hecho
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** D154

## Problema / motivación

Publicar una versión era una lista de comandos que había que recordar entera y en el
orden correcto:

```sh
make && make vdi && make iso && make imagem-persistente-fat32 && make entrega-sin-gpl
cp build/saguios.img build/saguios.vdi build/saguios.iso ... entrega/v6.4/
cd entrega/v6.4 && sha256sum saguios* > SHA256SUMS.txt
```

El riesgo no es que un comando falle (eso se ve), sino que **uno se olvide**: una entrega
sin la ISO, sin la variante sin GPL o sin la imagen persistente es fácil que pase
desapercibida, porque la imagen principal arranca igual. Además, cada versión cambia
algún detalle (el tamaño de la partición de datos, el nivel de optimización, si lleva
MicroPython) y no había forma de dejarlo escrito: la entrega no decía con qué se
compiló.

`run_in_qemu.sh` ya resolvía el problema hermano (arrancar el sistema con mil
opciones) con un guion en la raíz, con su `--help` y su forma de cargar el entorno
local. La versión merecía lo mismo.

## Propuesta

Un guion nuevo en la raíz, **`gerar_versao.sh`**, hermano de `run_in_qemu.sh`:

- **compila** la versión (con las variantes que ya existen en el Makefile);
- **arma** las imágenes que se entregan: el disco (`.img`), el VDI, la ISO de CD, la
  imagen persistente con FAT32 (comprimida) y la variante sin GPL;
- **deja la entrega** en `entrega/latest/` (sin versionar: siempre la última), con
  `SHA256SUMS.txt` y una hoja `VERSION.txt` con los datos de la compilación;
- con `--publicar`, la copia además a `entrega/v<versión>/`, que es donde vive cada
  versión congelada.

### La compilación parametrizada

Lo que se compila también se elige desde el guion, sin tocar el Makefile:

| Opción | Qué hace |
| --- | --- |
| `--variante gpl\|sem-gpl\|sem-firmware` | La variante de licencia de la imagen principal (D109). Cada una compila en su propia carpeta. |
| `--sin-python` | Compila sin MicroPython: una imagen bastante más pequeña. |
| `--otimizacao -O0\|-O1\|-O2\|-O3\|-Os` | El nivel de optimización del kernel y de los programas (`make OTIMIZACAO=...`). |
| `--jobs N` | Los trabajos en paralelo de `make`. |
| `--limpiar` | Borra la carpeta de compilación antes (compilación desde cero). |

Y lo que se entrega, con `--iso`, `--fat32 [MB]`, `--ext2 [MB]`, `--sin-gpl`,
`--destino DIR`, `--sin-compilar` (usar lo que ya hay compilado), `--sin-notas`,
`--notas FICHERO` y `--fijar-version vX.Y` (deja la versión en
`kernel/inc/versao.h` antes de compilar).

### La comprobación

`--probar` arranca la imagen recién hecha en el QEMU, sin ventana, y espera a que el
kernel llegue al escritorio (busca el «¡Hola! Soy SaguiOS…» en el log del puerto
serie). Si no llega en 60 segundos, enseña las últimas líneas del log y sale con
error: una entrega que no arranca no se publica.

## Criterios de aceptación

- [x] Un solo comando compila y deja la entrega en `entrega/latest/`
- [x] La entrega lleva `SHA256SUMS.txt`, que se comprueba con `sha256sum -c`
- [x] `VERSION.txt` dice la versión, el commit, la variante, el nivel de optimización y
      el contenido de la entrega
- [x] La compilación se puede parametrizar (variante de licencia, MicroPython,
      optimización) sin tocar el Makefile
- [x] `--probar` comprueba que la imagen arranca antes de darla por buena
- [x] `make versao` es atajo al guion
- [x] Decisión registrada en `docs/decisoes.md` (D154)

## Notas

- El guion **no toca el código** salvo con `--fijar-version`, que cambia la línea
  `#define SAGUI_VERSAO` de `kernel/inc/versao.h` (el resto del fichero no se toca, así
  que la codificación UTF-8 se conserva).
- La imagen persistente con FAT32 son 2 GB (dispersos); se entrega comprimida con `xz`,
  como en `entrega/v6.3/`. Si el `xz` no está instalado, se entrega sin comprimir.
- La variante sin GPL pasa antes la auditoría de licencias (`tools/auditar_licencias.py`),
  que es una puerta: si algo no cuadra, la entrega no sale.
- El guion carga `~/.local/saguitools/env.sh` si existe (nasm, gcc de 32 bits,
  `qemu-img`), igual que `run_in_qemu.sh`.
