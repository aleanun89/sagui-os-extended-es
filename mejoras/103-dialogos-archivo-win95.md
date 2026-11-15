# 103 - Diálogos de abrir, guardar y elegir carpeta (estilo Windows 95)

- **Estado:** aprobada (**v8.1**, pedido del mantenedor el 2026-11-15)
- **Prioridad:** alta (los usan casi todas las aplicaciones)
- **Alta:** 2026-11-15
- **Relacionadas:** componentes de la libsagui (`userland/libsagui/gui.c`, D065 diálogos), explorador
  Archivos (D136, D139), iconos (D135), usuarios (D255: carpetas sin permiso)

## Motivación

Hoy las aplicaciones piden rutas escribiéndolas (`pedir_texto`) o con soluciones propias. Hacen falta
componentes comunes y cómodos, con el aspecto clásico de Windows 95.

## Propuesta

Tres diálogos modales en la libsagui, iguales para todas las aplicaciones:
- **Abrir archivo:** lista de la carpeta (vista de lista y de detalles: nombre, tamaño, tipo, fecha),
  desplegable «Buscar en» con la ruta y las unidades (/, /home, /usb, /jogos, /apps, particiones montadas),
  botón «Subir un nivel», «Nueva carpeta», campo «Nombre de archivo» y desplegable «Tipo» con filtros
  (`*.txt`, `*.pdf`, `Imágenes`…), botones «Abrir» y «Cancelar». Doble clic en carpeta = entrar.
- **Guardar como:** igual, con nombre propuesto, confirmación al sobrescribir y extensión según el tipo.
- **Seleccionar carpeta:** árbol de carpetas desplegable (como «Buscar carpeta» de Windows 95) y «Nueva
  carpeta».
- Teclado completo (Tab, flechas, Intro, Esc, escribir para saltar a un nombre), carpetas recientes, y
  rutas sin permiso (D255) mostradas sin poder entrar.
- API sencilla: `dialogo_abrir(titulo, filtros, ruta_inicial, salida)`, `dialogo_guardar(...)`,
  `dialogo_carpeta(...)`; y adoptarla en Editor, Paint, Visor, visorpdf, Mico, Reproductor, Cipó
  (guardar página), Captura y los demás que pidan rutas.

## Pruebas
- QEMU: abrir, guardar con sobrescritura, elegir carpeta y filtros en dos o tres aplicaciones (capturas).

## Criterios de aceptación
- [ ] Tres diálogos comunes con aspecto Windows 95 y teclado completo
- [ ] Adoptados por las aplicaciones principales
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`
