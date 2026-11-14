# 026 - Vídeo: write-combining, resolución al arrancar y cambio de modo en caliente

- **Estado:** hecha (WC real y modos VESA del G470 sin probar)
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

- La memoria de vídeo quedaba como la deja la BIOS: sin caché (UC). Cada escritura de la copia
  del buffer de trás a la tarjeta era una transacción del tamaño de la instrucción.
- El cargador elegía siempre 1024x768 (o 800x600, 640x480). En el G470 la pantalla es de
  1366x768 y no había forma de pedir otra resolución.
- Cambiar de resolución exigía recompilar; el escritorio, las ventanas, el ratón y varios
  programas suponían 1024x768.

## Propuesta (aplicada)

**C. Write-combining** (`kernel/mem/wc.c`, D057). Con PAT (CPUID.1:EDX.16) la entrada 1 del
MSR IA32_PAT pasa de WT a WC y las páginas del framebuffer se mapean con PWT
(`janela_mapear_tipo`, siempre en tablas de 4 KB: nunca una página grande con tipos mezclados).
Sin PAT, un MTRR variable libre con tamaño potencia de 2 y base alineada, programado con la
secuencia del manual de Intel (CD=1, `wbinvd`, TLB vacía sin PGE, MTRR apagados mientras se
escribe); si una MTRR de la BIOS solapa el framebuffer se desiste (UC gana a WC). Se desactiva
con `tools/mkimagem.py --sem-wc` (bit `OPC_SEM_WC` del cabecero). El log dice
`[VIDEO] WC ativo via PAT|MTRR` o `[VIDEO] WC inativo: <motivo>`; `infopc` y `pantalla` lo
muestran. La medida de `rapido.c` corre después del mapeo, y otra vez (solo la copia a la
tarjeta) tras cada cambio de modo.

**A. Resolución al arrancar** (`boot/estagio2.asm`, D058). El cargador guarda en la boot_info
la lista de modos VBE de 24/32 bits con framebuffer lineal (hasta 64, en 0x5700) y prefiere la
resolución de `CAB_RES_LARG`/`CAB_RES_ALT` (offset 128 del cabecero, LBA 1). Sin preferencia
(0x0, el valor de toda imagen nueva) todo sigue igual: 1024x768. El programa `pantalla` la
cambia; `--guardar` reescribe solo esos 4 bytes en la partición del SaguiOS del disco de
arranque, tras comprobar que el sector es el mismo cabecero con el que arrancó la máquina.

**B. Cambio en caliente** (`kernel/dispositivos/bga.c`, `modos_video.c`, D059). Con la tarjeta
de Bochs/QEMU (PCI 1234:1111) o VBoxVGA (80EE:BEEF) y los puertos 0x1CE/0x1CF (ID
0xB0C0-0xB0C5) se cambia de modo sin BIOS. El framebuffer se mapea entero (16 MB en QEMU) al
arrancar, así que solo se rehace el buffer de trás. Se adaptan el terminal del kernel, el
fondo y los iconos (columnas según la altura), la barra superior, el ratón (también el
vmmouse) y las ventanas: las maximizadas y a pantalla completa reciben `EV_REDIMENSIONAR` con
el tamaño nuevo; las demás se meten dentro de la pantalla; todas reciben `EV_TELA` (16).

**ABI.** `SIS_MODOS_VIDEO` (76): `(modo_video_t[], n)`; `SIS_MUDAR_MODO` (77):
`(ancho | alto << 16, MUDAR_AGORA | MUDAR_GUARDAR)`; `INFO_TELA` (45): `info_tela_t`. Las
ventanas de usuario pasan a 8 MB cada una (0xE0000000-0xEFFFFFFF): la pantalla completa cabe
hasta 1920x1080; los datos del programa terminan en 0xE0000000.

**Programa** `pantalla` (`userland/programas/pantalla.c`): lista modos, cambia, `--guardar`,
`auto`, y `--ventana` abre la ventana "Pantalla" (lista + Aplicar / Aplicar y guardar).

Otros: el Terminal admite hasta 320x100 letras; la barra superior oculta el lema si no cabe
junto al reloj.

## Criterios de aceptación

- [x] WC por PAT en QEMU (`-cpu` por defecto, qemu32, tiene PAT): `[VIDEO] WC ativo via PAT`
- [x] Sin PAT (`-cpu qemu32,-pat,+mtrr`): el MTRR 0 de SeaBIOS (UC) solapa el framebuffer y se
      desiste con el motivo en el log; con `--sem-wc`, desactivado
- [x] Arranque por defecto en 1024x768 (las capturas de la batería no cambian)
- [x] `pantalla` lista los modos; `pantalla 1280x1024` y `pantalla 800x600` cambian en caliente
      con `-vga std`; ventana maximizada y pantalla completa se adaptan; el ratón llega al
      área nueva
- [x] `pantalla 1280x1024 --guardar` + reiniciar: el cargador arranca en 1280x1024
- [x] Test automático `tests/h58_resoluciones.py` (capturas `docs/capturas/H58-*.png`)
- [x] Decisiones en `docs/decisoes.md` (D057, D058, D059)
- [ ] Probado en el G470: WC real y su efecto en `[RAPIDO]` (y en `bench`)
- [ ] Probado en el G470: qué modos VESA da la BIOS de Intel (¿1366x768?) y `pantalla --guardar`
      en el pendrive

## Notas

- En QEMU sin KVM el tipo de memoria no cambia nada: la tarjeta es emulada. La prueba imprime
  la medida de la copia a la tarjeta con y sin WC solo como registro: entre ejecuciones varía
  más que entre los dos casos (p. ej. con WC rep 1369 / sse2 1090 / nt 1150 ciclos/1024, sin WC
  1362 / 1123 / 1143).
- El camino del MTRR no se puede ejercitar en QEMU (SeaBIOS deja una MTRR UC sobre todo el
  agujero PCI); en el G470 tampoco se usará, porque tiene PAT.
- La BGA de QEMU exige anchos múltiplos de 8: 1366x768 no existe en caliente (sí 1360x768).
  En el G470 la BIOS de vídeo de Intel suele no ofrecer 1366x768 por VBE; si no aparece en
  `pantalla`, habrá que usar 1280x768/1024x768 o un driver nativo (fuera de alcance).
- Sin BGA (G470) `pantalla ANCHOxALTO` guarda la preferencia para el siguiente arranque.
- Un icono "Pantalla" en el escritorio quedó fuera para no chocar con el hilo `menu-contextual`
  (que toca `area_trabalho.c`); se abre con `pantalla --ventana`.
- Con SMP (mejora 017), cada núcleo tendría que programar la misma PAT.
