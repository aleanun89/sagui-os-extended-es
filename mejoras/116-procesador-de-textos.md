# 116 - Procesador de textos «Pluma» (aspecto Word 97), RTF y exportación a PDF

- **Estado:** hecha (v8.3, rama `v8.3-procesador`)
- **Prioridad:** media
- **Alta:** 2026-11-15

## Problema / motivación

SaguiOS no tiene un procesador de textos: el Editor es de texto plano y el Visor de PDF solo lee. Para la temática retro del sistema (Windows 98 y Office 97) hace falta un programa que se le parezca, con formato de caracteres y párrafos, vista de página y salida a PDF, y de paso una biblioteca de componentes de escritorio más capaz que otras aplicaciones puedan reutilizar.

## Propuesta

**Pluma** (`userland/programas/pluma.c`, `/bin/pluma`): menús Archivo/Edición/Ver/Insertar/Formato/Herramientas/Ayuda, barras de herramientas Estándar y Formato, regla, vista de página con zoom, diálogos Fuente, Párrafo y Buscar/Reemplazar, fecha, salto de página, recuento de palabras, papel A4 o Carta, exportar a PDF. Formatos: RTF (leer y escribir), texto plano (leer y escribir) y PDF (solo escribir).

Componentes nuevos en la libsagui (`gui.h`): `barra_menu`, `barra_ferr`, `combo`, `regua`, `barra_estado`, `rte` (control de texto con formato), diálogos `dialogo_fonte`/`dialogo_paragrafo`/`dialogo_buscar`; formatos `rtf_ler`/`rtf_escrever` (`rtf.h`) y escritor de PDF `pdfw_*` (`pdf_escribir.h`). Detalle en `docs/pluma.md` y D291.

## Criterios de aceptación

- [x] Leer y escribir RTF con fuentes, tamaños, N/C/S, color, alineación, sangrías, viñetas y saltos de página; ida y vuelta estable
- [x] Exportar a PDF (14 fuentes estándar, Flate, A4/Carta) que abre el Visor de PDF del sistema y valida el anfitrión
- [x] La paginación en pantalla y en el PDF salen de la misma maquetación
- [x] Test automático `tests/h291_pluma.py` con capturas `H291-*.png`
- [x] Decisión D291 en `docs/decisoes.md`

## Notas

No incluye imágenes, tablas, estilos, cabeceras, corrector ni macros. Los glifos de pantalla son la fuente de mapa de bits escalada (no hay fuentes Helvetica/Times en el sistema); el PDF sí usa las fuentes estándar. Pendiente para otra mejora: insertar imágenes, tablas sencillas y vista previa de impresión.
