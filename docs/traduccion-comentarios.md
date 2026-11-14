# Traducción de comentarios al español (D096)

Estado de la tarea: **en curso**. Regla: solo cambian comentarios y docstrings; cada archivo
conserva su número de líneas; `build/saguios.img` debe salir idéntica (`sha256sum -c
build/referencia.sha`) y `python3 tools/comparar_sin_comentarios.py <archivos>` debe pasar.

Comprobación de cada lote: `tools/comparar_sin_comentarios.py` (compara con `HEAD` sin comentarios).

## Hecho

- **tools/**: `mkfonte.py`, `mkimagem.py`, `mkimagens.py`, `mkiso.py`, `raices_ssl.py`,
  `status.py` (este ya estaba en español en su mayor parte).
- `tools/comparar_sin_comentarios.py` ya estaba en español (primer commit de la tarea).
- `tools/confirmar-hardware.sh`, `boot/mbr.asm`, `boot/cabecalho.inc`, `boot/bootinfo.inc`, `boot/estagio2.asm`.
- **kernel/lib/**: `caminho.c`, `formatar.c`, `fuso.c`, `rapido.c`, `sha256.c`, `sse2.c`, `texto.c`.
- **kernel/inc/** (pequeños): `autoteste.h`, `caminho.h`, `pipe.h`, `portas.h`, `sha256.h`, `som.h`, `texto.h`, `tipos.h`.
- **kernel/arch/**: `excecoes.c`, `fpu.c`, `gdt.c`. **kernel/mem/**: `fisica.c`, `heap.c`, `wc.c`.

## Pendiente

Siguiendo el orden de D096: resto de `tools/` (`prueba_sonido.sh`), `boot/estagio2.asm`, `kernel/lib/`,
`kernel/inc/`, `kernel/arch/`, `kernel/mem/`, `kernel/nucleo/`, `kernel/fs/`,
`kernel/dispositivos/`, `kernel/gui/`, `kernel/usb/`, `kernel/rede/`, `userland/libsagui/`,
`userland/inc/`, `userland/programas/`, `userland/sagsh/`, `userland/chip8/`,
`userland/libtls/`, `userland/micropython/*_sagui.c`, `tests/*.py`, `Makefile`.

## Excluido por las ramas en curso

Se calcula al empezar cada lote con:

    for b in sonido monitor-bench usb-rapido pascal; do git diff --name-only main...$b; done | sort -u

Ver la lista completa en el informe de cada sesión. Los archivos que tocan esas ramas se dejan
para después de fusionarlas.

## Excluido por ser de terceros (no se traduce)

`userland/micropython/micropython_embed/`, `userland/micropython/libm_dbl/`,
`userland/micropython/lib/`, `kernel/terceros/`.
