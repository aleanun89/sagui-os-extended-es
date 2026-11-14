# 018 - Ventanas redimensionables y pantalla completa

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

Todas las ventanas tenían el tamaño que eligió el programa al abrirlas. El Terminal se quedaba
en 94x28 aunque sobrara pantalla, no se podía maximizar ni apartar una ventana sin cerrarla, y
Sagui 3D se jugaba en una ventana de 640x448.

## Propuesta (aplicada)

El gestor de ventanas del kernel (`kernel/gui/janelas.c`) atiende el borde y la esquina, los
botones de maximizar/restaurar y minimizar, el doble clic en el título y F11/Esc; pide el tamaño
nuevo al programa con `EV_REDIMENSIONAR` y el programa cambia sus píxeles con
`SIS_REDIMENSIONAR_JANELA` (mismo puntero). Las ventanas son de tamaño fijo salvo que el programa
lo pida con `SIS_ESTILO_JANELA`. Pantalla completa sin marco, ampliada por el kernel si el programa
no cambia de tamaño (`kernel/gui/escala.c`). Detalle en `docs/decisoes.md`, D044.

## Criterios de aceptación

- [x] Arrastrar la esquina del Terminal lo agranda y recalcula columnas x líneas sin perder el texto
- [x] Maximizar y restaurar con botón y con doble clic en el título
- [x] Minimizar; restaurar desde la barra superior
- [x] Editor y Archivos redimensionables y maximizables
- [x] Programas existentes de tamaño fijo sin cambios
- [x] F11 pone Sagui 3D en pantalla completa; Esc o F11 vuelven
- [x] Un programa puede pedir la pantalla completa con una llamada
- [x] Test automático `tests/h42_ventanas.py`
- [x] Decisión registrada en `docs/decisoes.md` (D044)
- [ ] Cursor con forma de flecha doble sobre el borde (toca `video.c`)
- [ ] Redimensionar desde los bordes izquierdo y superior
- [ ] Avisar al shell del tamaño del terminal (`TIOCSWINSZ`) para programas a pantalla completa

## Notas

- Sagui 3D se amplía (x1,6, vecino más próximo); renderizarlo a 1024x768 exigiría cambiar su
  motor (vista de 320x200 y HUD en posiciones fijas).
- Memoria: cada cambio de tamaño reserva páginas contiguas nuevas antes de liberar las antiguas;
  una ventana a pantalla completa ocupa 3 MB.
