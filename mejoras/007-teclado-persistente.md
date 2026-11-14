# 007 - Recordar la distribución de teclado entre arranques

- **Estado:** hecha (rama `configuracion`, D112)
- **Prioridad:** baja
- **Alta:** 2026-11-06

## Problema / motivación

`teclado abnt2` dura solo hasta reiniciar; el sistema siempre arranca en ES. El ramdisk es
de solo lectura en cada arranque, así que no hay dónde guardarlo.

## Propuesta

Opciones:

1. Un byte en el cabecero de boot (`boot/cabecalho.inc`) elegido al generar la imagen
   (`make TECLADO=abnt2`).
2. Guardarlo en el pendrive FAT32 de datos (`/usb/sagui.cfg`) y leerlo al montarlo.
3. Un selector en el Escritorio.

## Criterios de aceptación

- [x] La distribución elegida sobrevive a un reinicio
- [x] Test automático (`tests/h83_configuracion.py`)

## Cómo se hizo (D112)

Se eligió una variante de la opción 2 con el archivo `/home/.config/sagui.conf` (clave `teclado=es|abnt2`),
escrito por la aplicación **Configuración** o por `configuracion teclado abnt2`. Al arrancar, el kernel
ejecuta `configuracion --aplicar`, que lee el archivo y llama a `SIS_TECLADO`. Requiere un `/home`
persistente; sin él el cambio vale hasta apagar y el programa lo avisa.
