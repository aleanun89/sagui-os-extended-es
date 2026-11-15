# 060 - Varias pantallas (extender y duplicar)

- **Estado:** fase 1 (escritorio extendido y espejo con salidas de QEMU) **hecha** en la v8.3 (D290, rama `v8.3-multipantalla`, prueba h290); v9 (Intel) y v10 (AMD) pendientes
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 026 (resoluciones, BGA, D057-D059), 055 (motor de pantalla de Intel,
  resolución nativa), 054 (GPU AMD), 058 (interfaz), 031 (barra de tareas)

## Problema / motivación

Hoy hay una sola pantalla: la que deja la BIOS por **VESA**, y VESA solo sabe de una salida.
Para usar un portátil con un monitor externo (VGA/HDMI) o un PC con dos monitores hacen falta
dos cosas:
- que el **escritorio** sepa trabajar con varias superficies;
- **drivers de pantalla nativos**, que encienden y configuran cada salida.

## Propuesta por fases

| Versión | Fase | Contenido |
| --- | --- | --- |
| **v8** | Escritorio multipantalla | El compositor pasa de una superficie a un **escritorio virtual** formado por N salidas, cada una con su posición, resolución, búfer y write-combining. Ventanas que se mueven entre pantallas, maximizar en la pantalla donde están, cursor que cruza, barra de tareas en la principal (y opcionalmente en todas), modos **extender** y **duplicar**. Aplicación «Pantallas» (colocar con el ratón, principal, resolución por salida), guardada en `/home`. **Primer driver: varias tarjetas o cabezas en QEMU** (varias `bochs-display`/`secondary-vga`, o `virtio-gpu` con varias salidas en modo 2D), para probarlo todo sin hardware |
| **v9** | Intel nativo | Motor de pantalla de Intel (base en la 055: resolución nativa del panel) con **varias *pipes***: panel interno (LVDS/eDP) más VGA/HDMI/DP externo en el G470 y similares. Lectura de **EDID** por DDC/I²C para conocer los modos del monitor. Detección de conexión y desconexión del monitor externo. Siempre con vuelta a VESA en una sola pantalla |
| **v10** | AMD nativo | Salidas de las Radeon (DCE) mediante las tablas **AtomBIOS** de la tarjeta, empezando por R6xx/R7xx (dv7). Es la parte más difícil: hay que interpretar las tablas de la BIOS de vídeo. Si resulta inviable, se documenta y se limita a duplicar lo que la BIOS deja encendido |

## Robustez

- Una salida que falla o un modo que el monitor rechaza vuelve a la configuración anterior
  tras unos segundos sin confirmación («¿Mantener esta configuración?»).
- Opción de arranque «una sola pantalla (VESA)», que siempre funciona.
- El resto del sistema (programas, `tkinter`, SaguiGL) no necesita saber cuántas pantallas
  hay: trabajan con ventanas. `EV_TELA` (D059) se amplía con la geometría del escritorio.

## Pruebas

- **QEMU** con dos o tres salidas:
  - colocar las pantallas;
  - mover una ventana de una a otra;
  - maximizar en cada una;
  - duplicar;
  - cambiar la resolución de una sola;
  - capturas de cada salida (`screendump` por cabeza).
- **Real:** G470 con un monitor externo (v9); dv7 (v10).

## Criterios de aceptación

- [x] v8.3: escritorio extendido y espejo en QEMU con varias salidas (segunda `bochs-display`); orden `pantalla` y página de Configuración (D290). Pendiente: izquierda/arriba, una barra por pantalla, «¿Mantener esta configuración?», `virtio-gpu`
- [ ] v9: panel interno más monitor externo en un portátil Intel, con EDID
- [ ] v10: lo mismo en AMD R6xx/R7xx, o documentado por qué no
- [ ] Decisiones registradas en `docs/decisoes.md`
