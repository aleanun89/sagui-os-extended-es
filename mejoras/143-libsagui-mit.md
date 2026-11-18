# 143 - libsagui con licencia MIT

- **Estado:** idea del mantenedor (2026-11-18), sin versión asignada
- **Prioridad:** media
- **Alta:** 2026-11-18

## Problema / motivación

La libsagui mezcla licencias: 10 ficheros **GPL-3.0-only** heredados (`gui.c`, `widget.c`, `texto.c`, `entrada.c`,
`memoria.c`, `chamadas.c`, `formatar.c`, `aac.c`, `h264.c`, `mp4.c`, unas 5.800 líneas), 74 GPL-2.0-or-later y algunos
GPL-3.0-or-later. Por eso programas GPL-2.0-only (OpenTTD, D375) no pueden enlazarla, y lo que se compila con TCC
(D384) contra la libsagui queda bajo GPL-3.0.

## Propuesta

Una libsagui **MIT** (el sistema sigue siendo GPL v3; solo la biblioteca cambia):
1. Relicenciar a MIT lo que es obra del mantenedor y de los agentes del proyecto (el mantenedor es el titular):
   averiguarlo con el historial de git (ficheros que no vienen del SaguiOS original).
2. Reescribir en «sala limpia» (misma interfaz, sin copiar código) lo que venga del SaguiOS original (al menos los
   10 GPL-3.0-only), o pedir permiso a su autor.
3. Revisar las piezas de terceros incluidas y sustituir las que no sean compatibles con MIT.
4. Comprobación automática de licencias por fichero (auditar_licencias.py) para que no entre código GPL en ella.

## Criterios de aceptación

- [ ] Inventario de autoría por fichero de libsagui (git) y plan de relicencia/reescritura
- [ ] Reescritura limpia de los ficheros heredados, con las mismas pruebas pasando
- [ ] SPDX MIT en toda la libsagui y auditoría que lo vigile
- [ ] OpenTTD (D375) y programas de TCC pueden enlazarla sin cambiar de licencia
