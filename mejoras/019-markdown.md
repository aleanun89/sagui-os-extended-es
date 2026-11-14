# 019 - Visor y editor de Markdown

- **Estado:** hecha
- **Prioridad:** baja
- **Alta:** 2026-11-07

## Problema / motivación

Los textos de ayuda y los ejemplos del sistema son texto plano. Markdown es el formato
habitual para documentación (README, notas) y SaguiOS no tenía forma de verlo formateado.

## Propuesta (aplicada)

Programa gráfico nuevo `/bin/markdown` (`userland/programas/markdown.c`): visor de
CommonMark básico + tablas GFM, tareas y tachado; editor con resaltado de sintaxis; vista
dividida; modo texto `markdown -t` para el terminal. Documentos `rootfs/ejemplos/ejemplo.md`
y `rootfs/ayuda/markdown.md`. Archivos abre los `.md` con él y `ajuda markdown` lo explica.
Detalle en `docs/decisoes.md`, D045.

## Criterios de aceptación

- [x] Títulos `#`-`######` (H1 a 2×, negrita simulada, línea bajo H1/H2), párrafos con ajuste
      al ancho, **negrita**, *cursiva*, `código`, bloques de código, citas, listas anidadas,
      tareas, reglas, enlaces, imágenes como «[imagen: alt]», tablas alineadas y escapes
- [x] Desplazamiento con flechas, RePág/AvPág, Inicio/Fin y barra arrastrable
- [x] Editor con resaltado, Ctrl+S, guardar como, nuevo, abrir y aviso al cerrar con cambios
- [x] Ctrl+E / F5 alternan visor y editor; vista dividida
- [x] Clic en un enlace a otro `.md` lo abre
- [x] `markdown -t` imprime el documento maquetado
- [x] Test automático `tests/h43_markdown.py`
- [x] Decisión registrada en `docs/decisoes.md` (D045)
- [x] Ventana redimensionable y maximizable (D048, mejora 021: `EV_REDIMENSIONAR` →
      `maquetar(nuevo_ancho)`, mínimo 560x240)
- [x] Rueda del ratón (D048, mejora 021)

## Notas

Capturas: `docs/capturas/H43-visor.png`, `H43-editor.png`, `H43-visor-prueba.png`,
`H43-dividido.png`, `H43-aviso.png`. Las constantes `LARG`/`ALT` y la función `paineis()`
concentran la geometría de la ventana.
