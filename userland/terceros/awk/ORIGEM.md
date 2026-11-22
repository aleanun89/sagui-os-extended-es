# awk (The One True AWK) en SaguiOS (D440)

- **Origen:** https://github.com/onetrueawk/awk, etiqueta `20261106` (la «segunda edición» de Aho, Kernighan y Weinberger,
  que mantiene Brian Kernighan). Tarball `https://github.com/onetrueawk/awk/archive/refs/tags/20261106.tar.gz`,
  SHA-256 `1b354fe6fa0089a959ef912085e7a064236d15a7d3c9e6ce86456c3b87d3f464`.
- **Licencia:** permisiva de Lucent Technologies (aviso y exención de garantía, sin cláusula copyleft; equivalente a HPND),
  compatible con la GPL v3. El texto va en `fuentes/LICENSE` y se copia a `/apps/awk/LICENCIA-AWK.txt`.
- **Qué no va a git:** `fuentes/` (lo baja `tools/awk_fuentes.sh`, con el SHA-256 fijo; solo se copian los `.c`, `.h`, `awkgram.y`,
  `LICENSE`, `README.md` y `awk.1`). **Sin parches:** el código del upstream se compila tal cual.
- **Qué va a git (nuestro):** `awk.mk` (cómo se compila), `plataforma/` (`awk_sagui.h`: constantes `FPE_*` que faltan;
  `wctype.h` y `awk_compat.c`: `towupper`, `mbtowc` y `wctomb` de un byte, porque la libc no tiene caracteres anchos y su
  `MB_CUR_MAX` es 1, así que awk trabaja por bytes en Latin-1; `principal_main.c`: `principal()` llama a `main`).
- **La gramática (`gerado/`):** `awkgram.tab.c` y `awkgram.tab.h` salen de `awkgram.y` con GNU bison 3.8.2
  (`bison -d -b awkgram awkgram.y`). Se guardan en git porque el anfitrión no trae bison; `tools/awk_fuentes.sh` comprueba
  el SHA-256 de `awkgram.y` y avisa si cambia (entonces hay que regenerarlos). La salida de bison lleva su excepción de
  la GPL (puede usarse con cualquier licencia). `proctab.c` lo genera en el PC `maketab` (del upstream) al compilar.
- **Instalado en:** `/bin/awk` (herramienta básica de los guiones; unos 140 KB en el ramdisk), `/apps/awk` (licencia, léeme
  y página de manual) y `/ejemplos/awk` (un guion con su caso de prueba).
- **Cómo se comprueba:** `tests/h440_awk_lua.py`.
