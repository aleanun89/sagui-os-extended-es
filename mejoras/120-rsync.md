# 120 - rsync: sincronizar carpetas con otros ordenadores y copias de seguridad

- **Estado:** aprobada (**v8.4**, recomendación del coordinador del 2026-11-15; el mantenedor puede moverla)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** servidor SSH (D250), red, sistema de archivos (v8.4), copias de seguridad, permisos (D282)

## Motivación
Copiar y sincronizar carpetas entre SaguiOS y otros ordenadores (Linux, WSL, macOS, un NAS) mandando solo lo
que cambió: copias de seguridad de `/home`, llevar juegos y datos a la máquina, o sincronizar un pendrive.

## Propuesta
- **Base: openrsync** (OpenBSD, licencia **ISC**), no el rsync de Samba (GPL v3, compatible, pero hace `fork`
  para separar el generador y el receptor y comparte mucho estado global; SaguiOS no tiene `fork`). openrsync
  habla el protocolo de rsync (versión 27, la que entiende cualquier rsync moderno) con un receptor de un solo
  proceso. Donde haga `fork`+`exec`, se sustituye por `executar()` con tuberías o por hilos.
- **Fase 1 (servidor):** `rsync -av -e ssh carpeta/ root@saguios:/home/` desde Linux: el `sshd` (D250)
  ejecuta `rsync --server` con la entrada y la salida conectadas al canal. Lo más útil: copias y datos.
- **Fase 2 (cliente):** copia local con delta (`rsync -a /home/ /usb/copia/`) y cliente de demonios `rsync://`
  (NAS), sin necesidad de cliente SSH.
- **Fase 3:** cliente sobre SSH cuando exista el cliente `ssh` de SaguiOS (segunda fase de la D250).
- Conservar fechas, permisos, dueño y grupo (D282), enlaces y borrados (`--delete`); `--dry-run`, `--progress`.
- Opcional: una ventana sencilla de «Copia de seguridad» (origen, destino, programar con el cron).

## Criterios de aceptación
- [ ] `rsync -av` desde el anfitrión a SaguiOS por SSH, y de vuelta, con delta comprobado (solo se mandan los cambios)
- [ ] Copia local con `--delete` y `--dry-run`
- [ ] Cliente `rsync://` contra un demonio rsync del anfitrión
- [ ] Test automático en `tests/`; licencia en `docs/terceros.md`; decisión en `docs/decisoes.md`
