# Discos NVMe (D265)

SaguiOS lee y escribe discos NVMe (PCIe, clase 01h, subclase 08h, interfaz 02h) una vez arrancado el
sistema. Código: `kernel/dispositivos/nvme.c`. Decisión y detalles: D265 en `decisoes.md`.

- **Nombres:** `nvme0`, `nvme1`... (un namespace activo = un disco, hasta 4 por controlador); las particiones,
  `nvme0p1`, `nvme0p2`... Aparecen en `discos`, `infopc` (`[NVMe]`, modelo, capacidad y bloque) y se montan como
  cualquier otro disco (`mount nvme0p1 /datos`; es un disco interno: solo lectura salvo `mount -o rw`).
- **Tablas:** MBR (con extendidas) y GPT (las cuatro primeras particiones). En discos de bloque de 4096 las
  tablas cuentan en bloques de 4096 y el kernel las traduce a sectores de 512.
- **Sin interrupciones:** el driver sondea la cola de finalización con plazo (5 s por orden). No hay MSI en el
  kernel. Si el controlador no contesta, se apaga y el disco falla en lugar de colgar el sistema.
- **El número de serie no se lee ni se muestra.**
- **Límites:** sin TRIM, una sola cola de E/S con una orden a la vez, 2 TB como máximo (la capa de discos usa 32 bits), no se arranca desde NVMe (eso es del cargador UEFI).
- **Mirar en un PC real:** `dmesg`/log del kernel (`[NVME] controlador ..., versión ...`, `[NVME] namespace 1: ...`),
  `discos`, `infopc -t`.
- **Prueba:** `python3 tests/h265_nvme.py` (QEMU con `-device nvme`).

## D283 (v8.0.1)
- **BAR por encima de 4 GB:** el kernel la recoloca por debajo de 4 GB (`pci_realoc.c`, solo dispositivos del bus 0;
  aviso en el log si no hay hueco). Ya no hace falta apagar «Above 4G Decoding» para el NVMe.
- **Bloques de 4096:** se puede reparticionar (MBR y GPT, con `particiones`) e instalar (solo GPT, disco entero).
  Las particiones tienen que alinearse a 4096 (la alineación de 1 MB ya lo cumple).
- **Sigue sin:** MSI/INTx (sondeo) y varias órdenes en curso (un solo búfer de rebote).
