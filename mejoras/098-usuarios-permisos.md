# 098 - Usuarios y permisos

- **Estado:** fase 1 en **v8.0b7** (D255), fase 2 en **v8.4** (D282), fase 3 en **v8.4.4** (D340); falta SSH y crontab por cuenta
- **Prioridad:** alta (seguridad; hay `sshd` y `vncd`)
- **Alta:** 2026-11-14
- **Relacionadas:** servicios (D216), SSH (D250, D253, 052), VNC (097, D251, D252), sistemas de archivos
  (013), montajes (D120), almacenamiento (035), cortafuegos (073)

## Motivación

SaguiOS no tiene usuarios ni permisos: cualquier programa puede formatear, configurar la red, inyectar
entrada o leer cualquier fichero. Con acceso remoto (SSH, VNC) hace falta una barrera **sencilla, útil y
sobre todo robusta**.

## Decisiones del mantenedor (2026-11-14)

1. **Modelo:** un usuario normal (`sagui`, 1000) con entrada automática al escritorio y **root** con
   contraseña para administrar (como Ubuntu o macOS); varios usuarios en la fase 3.
2. **Elevación sin setuid:** la contraseña la comprueba el **gestor de servicios** (ya de confianza,
   corre como root), que lanza la orden como root. `su` / `admin orden` en el Terminal y un diálogo de
   contraseña en el escritorio.
3. **Dónde se guarda (2026-11-14):** partición pequeña **`SAGUI-CONF`** (ext2, 16 MB, solo root) para
   la configuración persistente del sistema (usuarios, sombra, claves de root y del servidor SSH,
   contraseña del VNC, `.conf` de servicios cambiados, red); se superpone al `/etc` de RAM al arrancar.
   La crean `mkimagem`, `gerar_versao` y el instalador. Sin ella (imagen en vivo, ISO): root sin
   contraseña solo en la consola local, con aviso. Con la raíz ext2 de la 035, `/etc` irá en la raíz.
4. **Versiones:** fase 1 en la v8.0b7; fases 2 y 3 en la v8.4 (antes v8.5) (después de la v8.0).

## Propuesta

### Fase 1 (v8.0b7): identidades y llamadas privilegiadas
- `uid`/`gid` en cada proceso, heredados; solo root los cambia. Usuarios: `root` (0), `sagui` (1000) y
  usuarios de servicio sin escritorio (`sshd`, `vncd`...).
- `/etc/usuarios` (cuentas) y `/etc/sombra` (PBKDF2-SHA256 con sal, solo root).
- **Las 133 llamadas al sistema clasificadas**; lo que no está marcado como libre se deniega a quien no
  es root (montar, formatear, disco crudo, particiones, red, Wi-Fi, cortafuegos, servicios, apagar,
  hora, acelerador 2D, `SIS_INYECTAR`...). Un solo módulo del kernel decide.
- Elevación por el gestor de servicios; `su`, `admin`, diálogo gráfico; registro en `/logs`.
- **Robustez:** «modo rescate» en el arranque (shell de root en la consola local); si `/etc/sombra`
  falta o está dañado, aviso y nunca bloqueo del arranque.
- `sshd` y `vncd`: `authorized_keys` por usuario; `SIS_INYECTAR` exige root + permiso del servicio.

### Fase 2 (v8.4): permisos de ficheros
- Dueño, grupo y `rwx` (9 bits, sin ACL), comprobados en el VFS (abrir, crear, borrar, renombrar,
  mkdir, ejecutar). ext2/ext4: los suyos. SGFS: todo de root. FAT32/ISO/UDF: `uid=`, `gid=`,
  `umask=` al montar.

### Fase 3 (v8.4): varios usuarios
- Pantalla de inicio, `/home/<usuario>` (separados de verdad solo en ext2), «Usuarios» en
  Configuración, SSH por usuario.

## Pruebas
- Una prueba recorre **todas** las llamadas como usuario normal: las privilegiadas dan `ERRO_ACESSO`,
  las libres funcionan. Elevación con contraseña buena y mala; modo rescate; `/etc/sombra` dañado.
- Batería completa sin regresiones (los programas del escritorio siguen funcionando como `sagui`).

## Criterios de aceptación
- [x] Fase 1: identidades, llamadas privilegiadas, elevación, modo rescate (v8.0b7, D255; falta el diálogo gráfico de contraseña)
- [x] Fase 2: permisos de ficheros (v8.4, D282, `h282_permisos.py`)
- [x] Fase 3: varios usuarios (v8.4.4, D340, `h340_varios_usuarios.py`; faltan SSH y crontab por cuenta)
- [x] Test automático en `tests/` (`h254_usuarios.py`, fase 1)
- [x] Decisión registrada en `docs/decisoes.md` (D255, fase 1)

## Notas
- Fase 1 hecha (D255, `docs/usuarios.md`): configuración persistente en la partición SAGUI-CONF (opción A del
  mantenedor), montada en `/sistema/conf` y copiada sobre `/etc` al arrancar.
- Modelo: kernel y seguridad con Opus; revisión con otro agente; userland (su, diálogos) con Sonnet.
- Fase 2 hecha (D282, rama `v8.5-permisos`): dueño, grupo, `rwx` y pegajoso comprobados en el VFS;
  ext2 con los suyos, ramdisk de root (`/home` de sagui, `/tmp` 1777, `/sistema` 0700), FAT32/ISO/UDF
  con `uid=`/`gid=`/`umask=`; `chmod`, `chown`, `chgrp`, `umask`, `ls -l` y Propiedades en Archivos;
  las rutas de D255 pasan a permisos, salvo unos pocos secretos que se protegen además por la ruta.
- Fase 3 hecha (D340, rama `v8.4.4-usuarios`): pantalla de inicio de sesión del gestor de ventanas (directo con
  una sola cuenta sin contraseña), Configuración > Usuarios (crear, borrar, renombrar, contraseña, grupo admin con
  su propia contraseña), cerrar sesión y cambiar de usuario en Inicio. Cada cuenta nueva tiene `/home/<nombre>`
  (0700) y sus programas lo ven como `/home` (el kernel traduce las rutas de quien no es root), así que la
  configuración es por cuenta sin cambiar los programas; `sagui` conserva `/home`. Pendiente: SSH y crontab por
  cuenta, inicio de sesión en la consola de texto.
