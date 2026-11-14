# 005 - Entorno de compilación sin sudo

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-06

## Problema / motivación

En el WSL no estaban `nasm`, `qemu`, `mtools`, `dosfstools` ni ImageMagick, y no hay sudo.
Además, los ficheros `*:Zone.Identifier` (restos de copiar desde Windows) rompían el Makefile
("multiple target patterns").

## Propuesta (aplicada)

- Se borraron los `*:Zone.Identifier` (solo tenían `ZoneId=3`).
- Paquetes descargados con `apt-get download` y extraídos en `~/.local/saguitools/root`.
- `~/.local/saguitools/bin/qemu-system-i386`: un wrapper que añade las rutas de la BIOS, de
  las ROM y de los módulos.
- `~/.local/saguitools/bin/convert` e `identify`: sustitutos mínimos en Python para los tres
  usos que hacen los tests.
- Uso: `. ~/.local/saguitools/env.sh && make && python3 tests/rodar_todos.py`

## Pendiente

- [ ] Llevar el script de instalación al repo (`tools/entorno-local.sh`) para que sea reproducible
- [ ] Añadir `*:Zone.Identifier` al `.gitignore`
