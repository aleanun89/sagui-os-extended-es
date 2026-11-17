# Generador de imágenes con ventana (D370)

Herramienta del PC (no de SaguiOS) para generar imágenes sin recordar opciones. Abrir:

    tools/generador            # ventana (Tkinter; en WSL2 con WSLg)

## La ventana

Arriba, el **perfil**: *Pública* (lo de `make variantes`), *Privada completa* (lo de `make priv_variantes`:
`--con-input-share`, NetSurf, datos de OpenJazz y Quake) y *Personalizada*. A la derecha, perfiles guardados con
nombre (Guardar…, Cargar, Borrar). La última configuración se recuerda sola.

Pestañas:

- **Entrega**: las opciones de `gerar_versao.sh`: imágenes (todas, SMP, uACPI, completo, ISO, persistente FAT32/ext2
  con tamaño, particiones, sin GPL), `--variante` (gpl, sem-gpl, sem-firmware), `--sin-python`, `--otimizacao`,
  `--jobs`, `--limpiar`, `--sin-compilar`, `--probar`, `--fijar-version`, `--notas`, `--publicar` y las variables de
  tamaño (`HOME_MB`, `JOGOS_MB`, `APPS_MB`, `PART_FS`...).
- **Pantalla, Memoria, CPU, Aceleración, Disco, Sesión, USB y varios**: las opciones de `tools/mkimagem.py`, con una
  explicación corta cada una. Los valores por defecto son los de hoy (`--vesa`, y las carpetas `build/jogos/raiz`,
  `build/apps/raiz` y `build/boot/uefi` si existen). **Solo valen en el perfil Personalizada.**
- **Personalizada**: nombre del fichero, carpeta de compilación y la marca «lleva datos privados».

Las combinaciones imposibles se deshabilitan con el motivo debajo (p. ej. NX con «Sin PAE», GTT sin 3D Intel,
uACPI sin ACPI). Las reglas están en la tabla `REGLAS` de `tools/generador.py`.

**Carpeta de destino:** *Elegir…* abre el selector de carpetas; *Windows (/mnt/c)* empieza en `/mnt/c/Users`.
Por defecto `entrega/latest` (Pública), `entrega/privada` (Privada) o `entrega/personalizada`. Al terminar,
*Abrir carpeta* usa `explorer.exe` con `wslpath -w`.

## Qué ejecuta

- **Pública/Privada:** `./gerar_versao.sh --destino DIR …` (la Privada, antes, `make priv_descargas`, y con
  `--con-input-share`).
- **Personalizada:** `make … all` (o nada con *No compilar*) y `python3 tools/mkimagem.py --mbr build/boot/mbr.bin …
  --saida DIR/saguios-personalizada.img <opciones>` más `sha256sum`. No hay forma de pasar opciones arbitrarias de
  `mkimagem.py` a `gerar_versao.sh`, y no se ha tocado: por eso la imagen personalizada llama a `mkimagem.py` directamente.

La orden equivalente se muestra (y se copia con *Copiar*). Se ejecuta siempre con
`. ~/.local/saguitools/env.sh` y `SAGUI_TURNOS=6 tools/turno.sh`, en su propio grupo de procesos: *Cancelar* lo
para con todos sus hijos. La salida sale en vivo (errores en rojo), con barra de progreso aproximada por fases; al
final, la lista de imágenes con tamaño y SHA-256 y *Probar en QEMU* (`run_in_qemu.sh --imagen …`, en segundo plano).

## Seguridad

- La entrega privada (o con input_share/datos de juegos/NetSurf) muestra **«USO PROPIO: NO PUBLICAR»**, pide
  confirmación, nunca lleva `--publicar` y **solo** va a una carpeta fuera del repositorio o ignorada por git y sin
  ficheros versionados (`entrega/v*` no vale). `entrega/privada/` y `entrega/personalizada/` están en `.gitignore`;
  `entrega/` está excluida de `tools/publicar.sh`.
- Se rechaza cualquier destino en carpetas de 86Box: las ROM nunca se copian.

## Sin ventana (pruebas y guiones)

    tools/generador.py --orden --perfil privada
    tools/generador.py --orden --perfil personalizada --build build --set mk_smp=1 --set mk_resolucion=1280x720
    tools/generador.py --perfil publica --ejecutar --destino /mnt/c/Users/yo/SaguiOS
    tools/generador.py --claves          # claves de --set (g_*, mk_*, perfil, con_privados...)

`--perfil` acepta también el nombre de un perfil guardado. Sale con 2 si hay errores (destino inválido, faltan
ficheros de compilación...). `SAGUI_GENERADOR_CONFIG` cambia el fichero de configuración
(`~/.config/saguios-generador.json`).

## Captura

No hay `import`/PIL en el anfitrión, así que no se incluye captura: la ventana es clara (ttk, tema clam), con el
perfil y los perfiles guardados arriba, las pestañas, la carpeta de destino, la orden equivalente en una caja gris,
los botones Generar / Cancelar / Probar en QEMU / Abrir carpeta con la barra de progreso, y el registro en un panel
oscuro. Una entrega privada añade una franja roja de aviso encima de las pestañas.

## Prueba

`python3 tests/h372_generador.py` (sin pantalla; necesita un `build/` ya compilado solo para la parte de QEMU:
`SAGUI_GENERADOR_BUILD=ruta` si no está en esta copia ni en `../sagui-os-main`).
