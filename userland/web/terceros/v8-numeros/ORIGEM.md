# V8: número en otra base, Number.prototype.toString(base) (D262)

- **Origen:** la función `DoubleToRadixCString` del motor de JavaScript [V8](https://v8.dev)
  (Copyright 2006-2011, the V8 project authors).
- **Licencia:** BSD de 3 cláusulas (`LICENSE`, el LICENSE.v8 de V8). Compatible con la GPL v3.
- **Cómo llegó aquí:** `n_numstr` de `js_nativo.h` (motor de JavaScript de robson-apk, D141) seguía el
  algoritmo del V8 (su propio comentario decía «mesmo algoritmo do V8»); robson-apk lo reconoció el
  2026-11-14. Aquí el algoritmo vive en `numero_base.h` y `js_nativo.h` solo lo llama, con el mismo
  resultado que antes.
- **Qué cambió respecto al original:** es C sin la biblioteca de V8; `floor` y `%` de JavaScript llegan
  como punteros a función (no hay libm); el texto queda al principio del búfer de quien llama.
- **Prueba:** `tests/h112*`-`h128*` (JavaScript); `(255).toString(16)`, `(0.5).toString(2)`,
  `(-10.25).toString(8)`, `(0.1).toString(3)`.
