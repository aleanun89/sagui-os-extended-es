# 087 - Discos SATA (AHCI) y el principio del USB 3.0 (xHCI)

- **Estado:** en curso
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** 036 (los drivers que faltaban: AHCI, xHCI, DMA), 055 (AHCI en el ROADMAP), D040 (los discos internos, sólo lectura), D155

## Problema / motivación

Desde el principio el SaguiOS habla con los discos internos por el modo IDE
(ATA por PIO, con los puertos 0x1F0 y 0x170). Con el BIOS en modo
"IDE/compatible" funcionaba; pero en un PC de hoy la BIOS deja el controlador
Sata en modo **AHCI** (el modo nativo), y entonces:

- El kernel no ve ningún disco interno: el aviso era "Disco SATA en modo
  AHCI: todavía no soportado".
- El usuario tiene que cambiar una opción de la BIOS (o no tener SATA) para
  poder usar el disco del portátil.

Y el otro lado: el USB 3.0 (5 Gb/s) no existe en el SaguiOS. El driver EHCI
(H29) es del USB 2.0; un pendrive USB 3.0, o un PC cuyo único controlador USB
sea el xHCI, no lo ve. El USB 3.0 lleva otro controlador distinto (el xHCI),
con otro modelo de programación (tres anillos, TRB y contextos).

## Propuesta

Dos cosas, en este orden:

1. **Un driver AHCI de verdad** (`kernel/dispositivos/ahci.c`,
   `kernel/inc/ahci.h`), con el disco SATA entrando en la **misma capa de
   discos** que los IDE (`kernel/fs/discos.c`), sin duplicar la lógica de
   particiones (MBR y GPT, D120). Así el disco SATA se ve en el comando
   `discos`, se montan sus particiones y se lee de ellas con las mismas
   reglas de seguridad de la D040 (sólo lectura salvo `mount -o rw`).

2. **El esqueleto del xHCI** (`kernel/usb/xhci.c`, `kernel/inc/xhci.h`): el
   USB 3.0 es un driver entero y no cabe en una mejora, así que aquí queda lo
   que se puede hacer sin él: detectar el controlador en el PCI, leer sus
   registros de capacidad, habilitarlo y reservar en memoria del kernel las
   estructuras de anillo, diciendo por el log qué se ha encontrado y qué
   falta. El plan completo está en `docs/usb3.md`.

### El driver AHCI, en corto

- **Detección**: clase `0x01` (almacenamiento), subclase `0x06` (SATA). La
  barra del ABAR es la quinta, de 64 bits (registros 0x24 y 0x28 del PCI).
- **Arranque**: reinicio del HBA (GHC.HRST), habilitación (GHC.AE) y, por
  puerto, la secuencia de la especificación: parar el puerto (lo que dejó la
  BIOS con sus propias estructuras, que ya no valen), apuntar la lista de
  comandos (CLB) y el área de recepción de FIS (FB), habilitar la recepción
  de FIS, dar corriente, girar el disco y arrancar el motor de DMA.
- **Los comandos ATA** van en un FIS de registro H2D (32 bytes) colgado de
  una tabla de comandos, con su lista de descriptores de datos (PRDT) para el
  DMA. Se usa IDENTIFY (0xEC) para leer la identidad del disco y READ DMA EXT
  (0x25) para leer sectores (y WRITE DMA EXT, 0x35, para escribir, que sólo
  pasa si alguien monta el disco para escritura).
- **Espera por polling**, como el driver ATA por PIO: se espera a que el
  controlador baje el bit de "comando en curso" (PxCI). Las interrupciones
  del controlador (INTx o MSI-X) no hacen falta para leer y no se configuran
  todavía (GHC.IE se deja apagado).
- **La identidad del disco** sale del IDENTIFY: el modelo (palabras 27 a 46,
  con los bytes cambiados), si soporta LBA48 (palabra 83) y los sectores
  (palabras 60-61 y 100-103).
- El disco se registra con `disco_registrar("hd", "SATA", ...)` como los
  discos ATA, así que el nombre es el de siempre ("hda", "hdb"...), con sus
  particiones ("hdb1", "hdb2") y las mismas reglas de la D040.

### Lo que cambia en el kernel que ya había

- `kernel/dispositivos/ata.c`: deja de buscar los AHCI y llama al driver
  nuevo; `ata_ahci_sem_suporte()` pasa a contar los controladores AHCI que
  **no** se han podido usar (con el driver en marcha es 0), y se añade
  `ata_ahci_quantidade()`.
- `kernel/nucleo/principal.c`: el aviso del SATA cambia (ya no dice "todavía
  no soportado") y el número de discos internos cuenta los SATA.
- `kernel/usb/usb.c`: enciende el xHCI antes que el EHCI y dice por el log
  que el USB 3.0 todavía no se enumera.
- `userland/programas/infopc.c`: el AHCI pasa de "mejora 036 (AHCI)" a
  "ahci (SATA; D155)" y el xHCI a "esqueleto, sin enumerar".

### La prueba

`tests/h131_sata_ahci.py`: el QEMU lleva el disco de arranque en el IDE y un
segundo disco SATA en un `ich9-ahci` con dos particiones (un ext2 y un
FAT32). Se comprueba que el controlador se encuentra, que el disco responde
al IDENTIFY, que se registra como `hdb`, que **sus particiones se ven** (que
es lo importante: no basta con detectar el controlador), que se montan y que
se lee un fichero de cada una (la lectura va de verdad por el driver AHCI) y
que el disco no cambia ni un byte. La segunda parte arranca con un
`qemu-xhci` y comprueba que el kernel no se cuelga y dice qué falta.

## Criterios de aceptación

- [x] El kernel compila sin errores nuevos
- [x] El kernel arranca en el QEMU sin colgarse ni dar NEVERA
- [x] Un disco SATA en modo AHCI aparece en la capa de discos, con sus
      particiones, y se pueden montar y leer
- [x] `tests/h131_sata_ahci.py` en verde
- [x] `docs/usb3.md` con el plan del USB 3.0
- [x] `kernel/usb/xhci.c` compila, arranca y explica por el log qué falta
- [x] Decisión registrada en `docs/decisoes.md` (D155)

## Notas

- El driver AHCI va **por polling** y sin interrupciones: es lo más simple y
  es lo que hace ya el driver ATA por PIO. Cuando haga falta rendimiento
  (muchos discos, NCQ), la vía rápida es el NCQ y las MSI-X.
- El QEMU se aparta de la especificación del xHCI en un par de sitios (la
  tabla de segmentos del anillo de eventos y las unidades de DBOFF y
  RTSOFF). El esqueleto lo hace con el formato del QEMU y lo apunta con un
  TODO en `docs/usb3.md`.
- Falta probar el AHCI en un PC de verdad (el HP dv7 y el Toshiba L850 de
  `testes_hw/` tienen un AHCI de Intel y de AMD): ahí se verá si el IDENTIFY
  y el READ DMA EXT se portan bien con el hardware de verdad y no solo con
  el emulado.
