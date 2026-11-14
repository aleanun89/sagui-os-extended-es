# 095 - Arrancar del CD/DVD e instalar en el disco interno

- **Estado:** hecha en `v7.2-cd-instalar` (D207), pendiente de mezclar y de probar en el Amilo
- **Prioridad:** alta
- **Alta:** 2026-11-13
- **Relacionadas:** D077 (la ISO El Torito), D155/D199 (AHCI), D179 (instalador), D194 (/apps del CD)

## Problema / motivación

El Fujitsu Amilo Pi 1536 del mantenedor no arranca por USB, pero sí arranca `saguios.iso`.
Desde el CD SaguiOS funcionaba en RAM, pero no se podía instalar: el kernel ignoraba los
lectores de CD/DVD (ATAPI) y el instalador no sabía de dónde copiar el sistema.

## Propuesta

- Lectores ATAPI de solo lectura en el IDE (PACKET por PIO) y en el AHCI (SATAPI), con log
  `[ATAPI]` (modelo, capacidad, sense key/ASC de cada error) y esperas por tiempo (TSC).
- ISO 9660 de solo lectura (Rock Ridge, Joliet, 8.3), montable en `/cd`; el CD de arranque
  se monta solo.
- La imagen El Torito de dentro del CD como un disco más (`cd0img`), que es el disco de
  arranque cuando se arrancó del CD: el instalador copia de él y quita `OPC_CD` de la cabecera.

## Criterios de aceptación

- [x] `discos`, `infopc` y `mount` muestran el CD; `ls /cd` y `sha256sum` leen la ISO
- [x] `instalar hda` desde el CD (IDE y AHCI) y el disco instalado arranca solo, con `/home`
- [x] Test automático: `tests/h207_instalar_cd.py`
- [x] Decisión registrada: D207
- [ ] Probado en el Amilo (líneas `[ATAPI]` del log)

## Notas

Sin archivos de más de 4 GB en ISO 9660 (varias extensiones), sin continuaciones CE de
Rock Ridge ni directorios relocalizados. El disco instalado lleva el ramdisk del CD (con
`/apps` dentro), que la partición SAGUI-APPS tapa.
