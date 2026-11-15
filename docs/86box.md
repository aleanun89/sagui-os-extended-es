# 86Box: pruebas en PC antiguos emulados (D301)

86Box (GPL v2) emula PC de los años 90 y principios de los 2000. Sirve para probar SaguiOS en hardware de
la época que no tenemos: placas Pentium, Pentium II y K6, gráficas 3dfx, S3, ATI y Matrox, Sound Blaster,
NE2000, IDE de los chipsets PIIX y VIA.

**Se usa solo en pruebas concretas, a petición. NUNCA en la batería** (`rodar_todos.py` y `rodar_paralelo.py`
solo recogen `h<dos dígitos>*.py` y `b<dígito>_*.py`; los ficheros de 86Box se llaman `emu_*` / `emu86box.py`).
El foco del proyecto va de Pentium II/Athlon a i3-i7 de 5.ª-7.ª generación (`docs/arquitecturas.md`).

## Las ROM no se pueden subir NUNCA

Las BIOS de 86Box tienen copyright. **NO SE PUEDEN SUBIR NUNCA** (palabras del mantenedor). Por eso:

- viven solo en `~/.local/saguitools/86box/roms/` (fuera del repositorio); 86Box las lee de ahí con `-R`;
  `tools/86box.sh` y `tests/emu86box.py` no las copian a `build/` ni a ningún sitio del repositorio
  (la biblioteca aborta si la ruta de las ROM cae dentro del repositorio);
- `.gitignore` y `tools/publicar-excluir.txt` las excluyen (`roms/`, `*.rom`, `*.bin`/`*.nvr` bajo `tools/86box/`,
  `build/86box/`);
- `tools/86box_roms_prohibidas.sh --indice` es un gancho `pre-commit` (lo instala `tools/86box_instalar.sh`) que
  impide añadirlas a git, por nombre y por tamaño típico de BIOS;
- `tools/publicar.sh` ejecuta `tools/86box_roms_prohibidas.sh --arbol` sobre lo que va a publicar y **falla** si
  encuentra una.

## Instalación (sin sudo)

```
bash tools/86box_instalar.sh              # instala
bash tools/86box_instalar.sh --comprobar  # dice qué hay y qué falta
```

Deja en `~/.local/saguitools/86box/`: `app/` (86Box v6.0, compilación b9001, AppImage extraída con
`--appimage-extract` porque en WSL no hay FUSE; SHA-256 fijo), `roms/` (repositorio `86Box/roms`, etiqueta
v6.0, SHA-256 fijo), `deps/root/` (Xvfb, xdotool, xwd y libpcre3 sacados de los .deb sin instalar) y `bin/` con los
envoltorios `86box`, `xvfb`, `xdotool`, `xwd`. No toca el `PATH` ni `env.sh`. Xvfb necesita `/usr/bin/xkbcomp`: sin
sudo se resuelve con un espacio de nombres de usuario (`unshare -rm`, superposición sobre `/usr/bin`).
Si el anfitrión no permite espacios de nombres de usuario, el guion dice la orden `apt` para el mantenedor
(`sudo apt install xvfb xdotool x11-apps x11-xkb-utils libpcre3`) sin ejecutarla.

## Máquinas de referencia (`tools/86box/maquinas/*.cfg`)

Plantillas de configuración de 86Box, sin ROM. Llevan `@IMAGEN@`, `@PISTAS@` (cilindros del disco con 63 sectores y
16 cabezas), `@SERIE@` (pty de COM1) y `@CAPTURAS@`. Disco IDE maestro con la imagen raw, ratón PS/2, COM1 por
«serial passthrough» a un pty del anfitrión que la biblioteca vuelca a `serie.txt`.

| Máquina | Placa y chipset | CPU | RAM | Vídeo | Sonido | Red |
|---|---|---|---|---|---|---|
| `p1-s3` | ASUS P/I-P55T2P4, i430HX (PIIX3) | Pentium MMX 200 | 64 MB | S3 Trio64 PCI | SB16 ISA | NE2000 ISA |
| `p2-voodoo3` | ASUS P2B-LS, i440BX (PIIX4) | Pentium II 350 | 128 MB | 3dfx Voodoo3 2000 AGP | SB16 | NE2000 PCI (RTL8029) |
| `p3-rage` | ASUS P3B-F, i440BX | Pentium II 450 (Deschutes) | 256 MB | ATI Mach64 VT2 (Rage II) | ES1371 | NE2000 PCI |
| `k6-matrox` | AOpen AX59 Pro, VIA MVP3 (Super 7) | AMD K6-2 350 | 128 MB | Matrox Mystique 220 | SB16 | NE2000 PCI |

Límites de 86Box 6.0 (comprobados en su código): **no emula Pentium III** (ni Katmai ni Coppermine, así que
tampoco SSE; `p3-rage` es una placa de P3 con un Deschutes), **no tiene ATI Rage 128 ni Rage Pro** (Mach64 VT2/VT3
es lo más cercano), **no tiene 3Com 3c905** (solo 3c501/3c503 ISA: se usa NE2000) y **no tiene Socket A ni Slot A**
(no hay Athlon/Duron: el último AMD es el K6-III+ en Super 7). Con Matrox hay Millennium, Mystique, Millennium II y
Productiva G100.

## Lanzar una máquina a mano

```
tools/86box.sh MAQUINA IMAGEN [--sin-pantalla] [--segundos N] [--captura FICHERO.png] [--esperar TEXTO]
tools/86box.sh p2-voodoo3 build/saguios.img --sin-pantalla --segundos 60 --captura /tmp/p2.png
```

Copia la configuración y la imagen a `build/86box/<máquina>-<pid>/` (la imagen original no se toca), arranca 86Box
(con Xvfb propio si es `--sin-pantalla`), pulsa F1 en el aviso «CMOS checksum error» de la BIOS, guarda el serie en
`serie.txt`, captura y lo para todo por PID. Va envuelto en `tools/turno.sh`: cuenta como una máquina más.
Sin `--sin-pantalla` abre la ventana en el `DISPLAY` (WSLg). Las imágenes han de arrancar por **BIOS**, no UEFI.

## Escribir una prueba

`tests/emu86box.py` ofrece `Maquina86Box(maquina, imagen)` con una interfaz parecida a la de `qemu.py`:
`ligar/desligar` (también `with`), `pasar_bios()`, `esperar_serial(texto, tempo)`, `serial()`, `enviar_serial()`,
`tecla()`, `digitar()`, `comando()`, `captura(nome)` (a `build/86box/<nombre>/capturas/`; `guardar=True` también a
`docs/capturas/`). Las teclas van por xdotool (XTEST) con el puntero sobre la ventana de 86Box. Ejemplo completo:
`tests/emu_86box_arranque.py` (arranca la imagen consola en las máquinas que se pasen, espera el prompt, captura y
escribe `build/86box/informe-<máquina>.txt` con lo que detectó el kernel):

```
tools/turno.sh python3 tests/emu_86box_arranque.py p2-voodoo3 p1-s3
```

El nombre de la prueba debe empezar por `emu_` (nunca `hNN`) para que ninguna batería la recoja.

## Primera ejecución (2026-11-15, SaguiOS v8.3 de `main`, 86Box v6.0 b9001)

| Máquina | Resultado |
|---|---|
| `p2-voodoo3` | **Arranca hasta el escritorio** y el terminal. Etapa 2 con BIOS Award, VESA 1024x768x24 sobre la Voodoo3, FPU/MMX sin SSE (copias con `rep movsd`), PS/2 teclado y ratón, ACPI, PCI correcto. Capturas `86box-p2-voodoo3-escritorio.png` y `86box-p2-voodoo3-lspci.png`. |
| `p1-s3` | **No arranca**: la etapa 2 dice `ERROR: datos leidos corruptos (la suma no coincide)` al cargar el kernel (con y sin dynarec). `86box-p1-s3-error-etapa2.png`. |
| `p3-rage` | **No arranca**: `Cabecera invalida` en la etapa 2 (BIOS Award de la P3B-F, disco con UDMA 2). `86box-p3-rage-error-cabecera.png`. |
| `k6-matrox` | **Reinicio en bucle**: el kernel escribe `[SAGUI] kernel iniciado / compilacion: con GPL` una y otra vez y la pantalla queda negra. `86box-k6-matrox-pantalla-negra.png`. |

Detección en `p2-voodoo3`: CPU GenuineIntel con FPU y MMX (sin SSE: lo avisa); 127 MB; VGA 121a:0005 (Voodoo3);
IDE PIIX4 8086:7111 visto por PCI, **pero `[ATA] 1f0/0: sem LBA, ignorado` y 0 discos ATA** (por eso no se montan
SAGUI-TMP, /home, /apps...); USB UHCI 8086:7112 con `[UHCI] el reinicio no terminó` y 0 controladores; la tarjeta
Realtek 10ec:8029 (NE2000 PCI) aparece en `lspci` y `ifconfig` dice que no hay driver (solo e1000 y RTL8139); la SB16
es ISA y no aparece en PCI (no se ha probado el sonido). Con 86Box el ratón PS/2 funciona si se configura
`mouse_type = ps2` (las plantillas ya lo ponen).

### Pendientes (no se arregló el kernel en esta tarea)

Registrados como issues de GitHub (etiqueta `86box`):

- [#33](https://github.com/aleanun89/miniOS/issues/33) 86Box p1-s3: la etapa 2 no arranca el kernel (datos leídos corruptos) con la BIOS AMI del 430HX
- [#34](https://github.com/aleanun89/miniOS/issues/34) 86Box p3-rage: la etapa 2 dice «Cabecera invalida» con la BIOS Award de la P3B-F (440BX)
- [#35](https://github.com/aleanun89/miniOS/issues/35) 86Box k6-matrox: el kernel se reinicia en bucle tras «compilacion: con GPL» con un K6-2 en VIA MVP3
- [#36](https://github.com/aleanun89/miniOS/issues/36) 86Box p2-voodoo3: el driver ATA ignora el disco IDE del PIIX4 (sem LBA) y no se montan las particiones
- [#37](https://github.com/aleanun89/miniOS/issues/37) 86Box p2-voodoo3: UHCI del PIIX4 «el reinicio no terminó» y ningún controlador USB
- [#38](https://github.com/aleanun89/miniOS/issues/38) 86Box p2-voodoo3: sin driver para la NE2000 PCI (RTL8029) ni la NE2000 ISA

## Lo que 86Box no es

86Box emula con precisión los chipsets y las BIOS de la época, pero no sustituye al hardware real: el rendimiento
no es el de un Pentium II (sobre todo el de las gráficas 3D), no hay SSE ni Pentium III, y fallos que aparecen aquí
pueden ser de la emulación. Si algo falla solo en 86Box, comprobar primero con otra BIOS/placa de la misma familia.

## Arrancarlo a mano para trastear: `run_in_86box.sh`

En la raíz del proyecto, como `run_in_qemu.sh`:

```
./run_in_86box.sh                      # p2-voodoo3 con build/saguios.img
./run_in_86box.sh p1-s3                # otro preset
./run_in_86box.sh --imagen entrega/v8.3/saguios.img
./run_in_86box.sh --lista              # los presets
./run_in_86box.sh p2-voodoo3 --sin-f1  # para entrar tú en el SETUP de la BIOS
```

La imagen se copia a `build/86box/manual-<máquina>/` (la original no se toca) y el serie queda en
`build/86box/manual-<máquina>/serie.txt` (`tail -f` en otra terminal). Se termina cerrando la ventana.

### Con el 86Box de Windows: `--windows`

```
bash tools/86box_instalar.sh --windows            # una vez (o --windows-solo)
./run_in_86box.sh --windows [MAQUINA] [--imagen X] [--sin-f1]
```

Arranca `86Box.exe` (Windows x64, v6.0 b9001, zip portable con SHA-256 fijo) por la interoperabilidad de WSL
(`cmd.exe`/`powershell.exe` deben responder; si no, mira `[interop]` en `/etc/wsl.conf`). Todo vive en
`%LOCALAPPDATA%\saguitools\86box\` (`86Box.exe`, `roms\` y `vm\manual-<máquina>\`), **fuera del repositorio**: las
ROM tampoco se suben nunca. La imagen se copia a `vm\manual-<máquina>\disco.img` en cada arranque y el `86box.cfg`
sale del preset con rutas de Windows. La NVR de la BIOS se conserva entre sesiones.

- **Serie:** Windows no tiene pty, así que COM1 va al dispositivo «File (COM)» de 86Box, a `serie.txt` de esa carpeta
  (`tail -f /mnt/c/Users/<tú>/AppData/Local/saguitools/86box/vm/manual-<máquina>/serie.txt`; el script la imprime).
  86Box escribe con el búfer del sistema (bloques de 4 KB), así que el texto llega con retraso.
- **F1 de la BIOS:** PowerShell pulsa F1 hasta 30 s o hasta que el serie tenga texto (`--sin-f1` lo evita).
- Se queda en primer plano hasta cerrar la ventana. Uso manual del mantenedor: no pasa por `tools/turno.sh` ni cuenta
  como QEMU de pruebas. Para cerrarlo a la fuerza desde WSL: `taskkill.exe /F /PID <PID>` (el PID lo imprime el script).

## Ratón

86Box emula un ratón PS/2 **relativo** (`mouse_type = ps2`, el que usa SaguiOS): hay que *capturarlo*.

- **Capturar:** clic dentro de la ventana de 86Box (el cursor del anfitrión desaparece y el del invitado se mueve).
- **Soltar:** **Ctrl+End** o el **botón central** (el texto de la barra de 86Box 6.0: «Press Ctrl+End or middle button
  to release mouse»; no hay opción de configuración que lo cambie en la 6.0).
- **Por qué con WSLg va mal:** la captura necesita bloquear y recentrar el puntero, y WSLg (Weston/RDP) no lo admite
  bien: el cursor va a saltos o se desincroniza. No es un fallo de SaguiOS ni de los presets.
- **Recomendación:** para trastear con ratón, usa `./run_in_86box.sh --windows`: en Windows la captura funciona bien.
  En modo Linux el script avisa de esto al arrancar bajo WSL.

