# 011 - Shell más completo, tipo sh/bash

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-06

## Problema / motivación

sagsh solo separaba palabras con comillas y entendía `|`, `<`, `>`, `>>` y `&`. No había
variables, `$?`, `&&`/`||`, bloques, bucles, funciones ni scripts: para cualquier cosa algo
más larga que un comando había que escribir un programa en C o Python.

## Qué se hizo

Un shell tipo POSIX sh con lo esencial de bash, manteniendo el nombre `sagsh`, los alias en
portugués, `ajuda`, la edición de línea y los textos que esperan los tests. Detalle en
`docs/decisoes.md`, D037.

- `userland/sagsh/` (antes `userland/programas/sagsh.c`): léxico + analizador recursivo,
  expansión POSIX, aritmética, ejecución, internos, variables, ayuda, edición y bucle.
- `/bin/sh` es una copia de sagsh. `env` (nuevo programa) muestra el entorno.
- Kernel: `envp` en `SIS_EXECUTAR`, grupos de procesos (`SIS_GRUPO`), Ctrl+C a todo el grupo
  en primer plano y contador de Ctrl+C (`SIS_INTERRUPCOES`), `SIS_TIPO_DESCRITOR`,
  32 descriptores por proceso.
- libsagui: `environ`, `obter_env`, `executar_env`, `definir_grupo`, `interrupcoes`,
  `tipo_descritor`.
- Ejemplos: `/ejemplos/hola.sh`, `/ejemplos/funciones.sh`, `/ejemplos/adivina.sh`;
  arranque: `/etc/sagshrc`.

## Criterios de aceptación

- [x] Analizador de verdad: `;` `&&` `||` `|` `&` `( )` `{ }`, saltos de línea, `#`, `\`,
      comillas simples y dobles con las reglas de sh
- [x] Variables, `$?` `$$` `$!` `$#` `$0..$9` `$@` `$*`, `${x:-y}` y compañía, `export`,
      `unset`, `set`, `env`; entorno heredado por los programas (envp en el kernel)
- [x] `$( )`, `` ` ` ``, `$(( ))`, comodines `* ? [abc]`, `~`
- [x] `2>`, `2>&1`, `>&2`, `2>>`, `&>`, here-doc `<<FIN` / `<<-FIN`, `<<<`
- [x] `if/elif/else`, `while`, `until`, `for`, `case`, `break`, `continue`, `return`,
      funciones, `local`, `test`/`[ ]`, `true`, `false`, `:`, `exit`, `shift`, `read`,
      `echo -n/-e`, `printf`, `source`/`.`, `eval`, `alias`/`unalias`, `type`,
      `command -v`, `history`, `jobs`, `wait`, `fg`, `kill %n`
- [x] Scripts: `sagsh script.sh args`, archivos con `#!/bin/sh` ejecutados directamente,
      `/bin/sh`, ejemplos en `/ejemplos/*.sh`, `/etc/sagshrc`
- [x] Ctrl+C (ventana Terminal): termina todo el trabajo en primer plano, `$?` = 130;
      también corta bucles internos y `read`. "[1]+ Hecho comando" al terminar los trabajos
- [x] Sin límites fijos pequeños (líneas y listas dinámicas), errores de sintaxis claros,
      PS2 "> " para continuar, límite de recursión en vez de caída
- [x] Test automático `tests/h38_shell_completo.py`; h16, h18 y h24 actualizados
- [x] Decisión registrada en `docs/decisoes.md` (D037)

## Ideas siguientes

- Expansión de llaves `{a,b}` y `{1..5}`, `[[ ]]`, arrays, `getopts`, `select`, `$RANDOM`,
  `$LINENO`.
- Señales de verdad para `trap` (hoy solo `trap ... EXIT`) y `Ctrl+Z`/`bg` (detener
  trabajos): el kernel no tiene señales.
- Ctrl+C también en la consola del kernel (modo `--console`).
- Un `/dev/null` real en el kernel (hoy el shell lo emula).
- Completar con Tab nombres de variables y opciones; búsqueda en el historial (Ctrl+R) y
  guardarlo en un archivo.
- Una pila de usuario mayor (o configurable) para recursión más profunda.
- `/bin/sh` como enlace en lugar de copia si el SGFS llega a tenerlos (ahorraría 76 KB).
