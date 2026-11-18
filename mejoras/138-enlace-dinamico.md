# 138 - Enlace dinámico de bibliotecas (.so, dlopen, ld-sagui)

- **Estado:** fases 1 y 2 hechas (publicadas en la v8.5.4.3); fase 3 (páginas compartidas en el kernel) hecha en la rama
  `v8.5.4.5-paginas` (2026-11-18, pendiente de revisión); fases 3b-7 por hacer
- **Prioridad:** media-alta
- **Alta:** 2026-11-17

## Problema / motivación

Todos los programas se enlazan estáticos: la libsagui, la libtls, la libfontes, la SDL2... se repiten en cada ELF
(≈3,9 MB de los 6,7 MB de ELF del ramdisk, que se copia entero a la RAM al arrancar, y ≈5,3 MB en `/apps/bin`), un fallo
de la libsagui obliga a recompilar todo, y no hay `dlopen` para módulos nativos (Java JNI, Python, complementos). El
mantenedor pide enlace dinámico bien planteado y que el `cc`, Pascal y TinyGo puedan generar programas que lo usen.

## Propuesta

Diseño completo en [`docs/enlace-dinamico.md`](../docs/enlace-dinamico.md): intérprete `ld-sagui.so` en espacio de
usuario cargado por el kernel, enlace inmediato + RELRO, bibliotecas `-fPIC` con `soname`, páginas compartidas de solo
lectura en el kernel (sin copia al escribir), `/apps` primero y `/bin` después salvo lo esencial, `cc` fases A y B,
Pascal, TinyGo, miniJVM y Python con `dlopen`.

Fases: 1 `libdl` en usuario (hecha) · 2a `PT_INTERP` + `ld-sagui.so` · 2b `.so` del sistema · 3 páginas compartidas ·
3b `/bin` dinámico · 4 `cc` fase A · 5 `cc` fase B y Pascal · 6 TinyGo/Java/Python · 7 PIE/ASLR (opcional).

## Criterios de aceptación

- [x] Fase 1: `dlopen/dlsym/dlclose/dlerror` en espacio de usuario con relocaciones i386, RELRO y W^X (`tests/h385_dlopen.py`)
- [x] Medida del coste del PIC con un caso real (SHA-256: +2,2 %, +0,9 % con `-Bsymbolic-functions`)
- [x] Decisión registrada en `docs/decisoes.md` (D378, propuesta)
- [x] Respuestas del mantenedor a las preguntas de `enlace-dinamico.md` §11 (todas las recomendadas; 9: GPL-3.0)
- [x] Fase 2: intérprete `ld-sagui.so.1` y `.so` del sistema (`tests/h386_dinamico.py`, h247)
- [x] Fase 3: páginas de solo lectura de las bibliotecas compartidas entre procesos en el kernel (`MEM_COMPARTIR`,
  `kernel/mem/imagens.c`; `tests/h390_paginas_compartidas.py`: 8 hijos, sustitución en disco, W^X, sin fugas, `--smp`,
  PAE con 6 GB, `--sem-pae`, swap). Medido: un hijo dinámico gasta 1150 KB en vez de 2450 KB
- [ ] Fases 3b-7, cada una con su prueba

## Notas

El núcleo de memoria (fase 3, v8.5.4.5) es la parte delicada: va en su propia fase, con pruebas de estrés con `--smp`, swap y
memoria alta. El enlace dinámico no cambia la situación legal de los programas GPL-2.0-only (D375).
