# 101 - Ordenar los iconos del escritorio

- **Estado:** hecha en la rama `v8.0-iconos` (D268, prueba h268); pendiente de fusionar (v8.0)
- **Prioridad:** media
- **Alta:** 2026-11-14
- **Relacionadas:** escritorio y sus iconos (`kernel/gui/janelas.c`, `kernel/gui/arte.c`), menú
  contextual del escritorio (D051, D064), iconos (D135), personalización (047), Configuración (D112),
  varios usuarios (098, fase 3: preferencias por usuario)

## Motivación

Los iconos del escritorio salen en un orden fijo. El mantenedor quiere poder ordenarlos.

## Propuesta

- **Mover a mano:** arrastrar un icono a cualquier sitio, con **rejilla** (encaje a la casilla más
  cercana) y sin solaparse; si se suelta encima de otro, se intercambian o se busca la casilla libre
  más próxima.
- **Ordenar automáticamente** desde el menú contextual del escritorio, submenú «Ordenar iconos»: por
  **nombre**, por **tipo** (programas, carpetas, documentos), por **fecha**, y «**Alinear a la
  rejilla**»; opción «**Organización automática**» (mantener siempre ordenados).
- **Selección múltiple** (rectángulo con el ratón y Ctrl+clic) para mover varios a la vez.
- **Persistente:** posiciones y criterio en `/home/.config/escritorio.conf` (por usuario cuando llegue
  la fase 3 de la 098); si cambia la resolución (D059), recolocar sin perder el orden relativo.
- Teclado: flechas para moverse entre iconos, Intro para abrir (accesibilidad).

## Pruebas
- QEMU: arrastrar un icono y comprobar su casilla (captura y registro), ordenar por nombre y por
  tipo, reiniciar y comprobar que se conserva, cambiar de resolución y comprobar que nada queda fuera.

## Criterios de aceptación
- [x] Arrastrar con rejilla, sin solapes
- [x] Ordenar por nombre, tipo y fecha; alinear; organización automática
- [x] Posiciones guardadas entre arranques y tras cambiar de resolución
- [x] Test automático en `tests/`
- [x] Decisión registrada en `docs/decisoes.md`

## Notas
- Modelo sugerido: Sonnet (escritorio en el gestor de ventanas, con pruebas medibles).
