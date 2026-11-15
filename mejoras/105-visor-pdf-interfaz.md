# 105 - Interfaz gráfica completa para el visor de PDF

- **Estado:** hecha en la rama `v8.1-pdf-gui` (D274), pendiente de fusión; falta enchufar el diálogo de la mejora 103 en `escolher_arquivo()` y el arrastrar y soltar (no existe en el sistema)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** visor de PDF (D138, D205, `userland/programas/visorpdf.c`, motor `userland/pdf/`),
  diálogos de archivo (mejora 103), Archivos (abrir con)

## Propuesta

Que `visorpdf` sea una aplicación cómoda de escritorio:
- **Barra de herramientas** con iconos: Abrir (diálogo de la mejora 103), imprimir/exportar página a
  imagen, página anterior/siguiente y cuadro «página N de M», zoom (+, −, ajustar al ancho, a la página,
  porcentaje), girar, buscar.
- **Panel lateral** plegable con **miniaturas** de las páginas y el **índice** (marcadores/outline del PDF).
- **Buscar texto** (Ctrl+F) con resaltado y siguiente/anterior; seleccionar y copiar texto si el motor
  lo permite.
- Desplazamiento continuo entre páginas, rueda y teclado (RePág/AvPág, Inicio/Fin), pantalla completa
  (modo presentación).
- Recordar la última página y el zoom por documento; abrir desde Archivos con doble clic; arrastrar un
  PDF a la ventana.
- Contraseña de PDF cifrados con un diálogo (el motor ya descifra, `pdf_cripto.c`).

## Pruebas
- QEMU con PDF de prueba (incluidos los reales de D205): navegación, zoom, miniaturas, índice, búsqueda
  (capturas y comprobaciones por serie).

## Criterios de aceptación
- [x] Barra de herramientas, miniaturas e índice
- [x] Búsqueda, zoom y navegación completos
- [x] Test automático en `tests/` (`h274_visorpdf_gui.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D274)
