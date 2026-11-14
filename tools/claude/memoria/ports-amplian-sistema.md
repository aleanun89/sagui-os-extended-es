---
name: ports-amplian-sistema
description: "Portar juegos libres sirve también para completar las capacidades del sistema (C++, SDL, libc) hacia compilar apps grandes dentro de SaguiOS"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 70ab6765-4570-4f0d-a587-96a40f1d4263
  modified: 2026-11-13T14:16:13.411Z
---

El mantenedor (2026-11-13): «portar juegos es una manera de completar también las capacidades que luego permitirán compilar desde dentro del propio OS aplicaciones más completas». Juegos en marcha: OpenTyrian (D156), OpenJazz (v7.4, D228, runtime C++ con excepciones y SDL 1.2 común), Quake 1 (v7.4.1, D229), Vanilla Conquer (v7.4.2, D230, SDL2 mínima). Los datos no distribuibles van por `input_share/games/<juego>/` (Jazz completo, `Quake/pak0.pak`, `CnC/CnC_GDI95.iso` y `CnC_NOD95.iso`).

**Why:** cada port obliga a añadir piezas (runtime de C++, capas SDL, funciones de libc, cabeceras) que luego sirven para otros programas y para el compilador dentro del sistema.

**How to apply:** en los encargos de ports, pedir que lo que falte se haga general y reutilizable (`userland/libcxx`, `userland/libsdl12`, `userland/libsdl2`, la libc común D210), nunca parches dentro del juego. Juegos siempre fuera de la variante sin GPL. Motores que exigen .NET u OpenGL con shaders (OpenRA) no son viables; buscar el motor original en C/C++ con render por software (Vanilla Conquer).

Relacionado: [[imagenes-en-github]], [[base-vs-apps-particiones]]
