# uACPI en SaguiOS (D192)

Intérprete de AML y subsistema ACPI pequeño, pensado para kernels de aficionado y
sistemas embebidos. Lo usa SaguiOS para ejecutar los métodos de la batería
(`_STA`, `_BIF`/`_BIX`, `_BST`) y del cargador (`_PSR`), que casi siempre leen
el controlador embebido (EC).

- **Origen:** <https://github.com/uACPI/uACPI>
- **Versión:** etiqueta `7.0.0` (commit `686a9a52d9ad72690406d7cada82801377d5e2fa`,
  publicada el 2026-10-16)
- **Archivo descargado:** `https://github.com/uACPI/uACPI/archive/refs/tags/7.0.0.tar.gz`,
  SHA-256 `4ccdb4c06d06c54116920eb861adf3a87efd3285c762e2e6f3529b52c6d20fa5`
- **Licencia:** MIT (Copyright (c) 2022-2026 Daniil Tatianin), texto en `LICENSE`.
  No es GPL: entra también en la variante `SEM_GPL=1` (D109).

## Qué se copió

Solo `source/` (sin `files.cmake`), `include/` y `LICENSE`, **byte a byte**. No se
copiaron las pruebas (`tests/`), ni `meson.build`, `meson.options`,
`uacpi.cmake` ni el `README.md` del proyecto.

## Cambios

**Ninguno en el código de uACPI.** Todo lo propio está fuera de esta carpeta:

- `kernel/inc/uacpi_types.h`: los tipos de uACPI sobre los del kernel
  (`UACPI_OVERRIDE_TYPES`), para no mezclar `<stdint.h>` con `tipos.h`;
- `kernel/dispositivos/uacpi_sagui.c`: la capa del kernel (`uacpi_kernel_*`);
- `kernel/dispositivos/ec.c` y `kernel/dispositivos/bateria.c`: el controlador
  embebido y el driver de la batería;
- las opciones de compilación del `Makefile` (`UACPI_PHYS_ADDR_IS_32BITS`,
  `UACPI_OVERRIDE_TYPES`).

Para actualizar: bajar la etiqueta nueva, comprobar el SHA-256, sustituir
`source/` e `include/` enteros, actualizar esta ficha y la fila de
`docs/terceros.md`, y repasar `kernel_api.h` por si cambió la capa del kernel.
