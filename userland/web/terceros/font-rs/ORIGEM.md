# font-rs: rasterizador por área acumulada (D262)

- **Origen:** el rasterizador de [font-rs](https://github.com/raphlinus/font-rs), de Raph Levien
  (Copyright 2015 Google Inc.), escrito en Rust.
- **Licencia:** Apache-2.0 (`LICENSE`, el texto de la licencia). No hay fichero NOTICE. Compatible con
  la GPL v3 del conjunto (no lo sería con «GPL v2 solo»; aquí el conjunto es v3).
- **Cómo llegó aquí:** el motor de la web del Cipó (código de robson-apk, D141) trae el algoritmo
  reproducido de memoria en C por una IA (`linha()` de `fonte.c` y `r_linha()` de `canvas2d.c`, más la
  suma corriente que da la cobertura). robson-apk lo reconoció el 2026-11-14 (commit `8c3b56b` de
  `robson-apk/sagui-os`, `docs/cipo.md`). Aquí se separó en esta carpeta, **sin cambiar el comportamiento**.
- **Qué hay:** `fontrs.h`, funciones `static inline`:
  `fontrs_linha` (fuentes, `float`), `fontrs_acumular` (suma corriente, regla «distinto de cero»),
  `fontrs_linha_recortada` (Canvas y SVG, `double`, recortada a la caja del dibujo) y
  `fontrs_acumular_regra` (suma corriente con «par o impar», `fill-rule: evenodd`).
- **Quién lo usa:** `userland/libfontes/fonte.c` (glifos TrueType y CFF) y `userland/web/canvas2d.c`
  (caminos del Canvas y del SVG).
- **Modificado respecto al original (Apache-2.0, sección 4b):** es C, no Rust; sin SIMD; con las
  variantes de recorte y de regla par-impar descritas arriba, que el original no tiene.
