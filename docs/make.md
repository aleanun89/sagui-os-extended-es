# GNU make dentro de SaguiOS (D413)

Resumen de la decisión: [`decisoes.md`](decisoes.md) D413. Guía de uso: [`rootfs/ayuda/make.md`](../rootfs/ayuda/make.md).

    tools/make_fuentes.sh      # GNU make 4.4.1 a userland/terceros/make/fuentes (no va a git)
    make                       # /apps/bin/make y /apps/make (MAKE_DENTRO=0 lo deja fuera)
    python3 tests/h413_make.py # prueba (pc y saguios)

Estructura de `userland/terceros/make/`: `make.mk`, `plataforma/` (`config.h`, `glob.c`, `compat.c`, `ar.h`, `make_sagui.h`,
`principal_main.c`), `ejemplos/proyecto/`, `leeme.txt`. Cambios sobre el original (los hace `tools/make_fuentes.sh`):
`fcntl_bloqueo` en lugar de `fcntl(F_SETLKW)` en `posixos.c`; el getopt de make va con los símbolos renombrados (`-Dgetopt=make_getopt`...).
