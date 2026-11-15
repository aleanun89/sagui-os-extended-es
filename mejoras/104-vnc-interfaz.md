# 104 - Interfaz gráfica para el VNC (cliente y servidor)

- **Estado:** hecha (**v8.1**, D273, pendiente de integrar)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** VNC (mejora 097: `vncd` D252, `vnc` D254, `ssh -L` D253), Configuración (D112),
  diálogo de permisos (D269), diálogos de archivo (mejora 103)

## Propuesta

- **Cliente («Escritorio remoto»):** al abrirlo sin argumentos, una ventana de conexión: equipo y
  puerto, contraseña (recordable), calidad y colores (8/16/32 bpp, JPEG), «solo ver», pantalla completa,
  y una **lista de conexiones guardadas** (`/home/.config/vnc.conf`) con «Conectar», «Editar», «Borrar».
  Barra de herramientas durante la sesión: Ctrl+Alt+Supr, pantalla completa, 1:1/escalar, desconectar.
- **Servidor:** pestaña «Escritorio remoto» en Configuración: activar/desactivar el servicio `vncd`,
  fijar la contraseña, «solo ver», escuchar en «solo esta máquina (por SSH)» o «red local», ver quién
  está conectado y desconectarlo; con el diálogo de permisos (D269) porque es de root. Indicador en la
  bandeja mientras hay alguien conectado.

## Pruebas
- QEMU: la ventana de conexión con una conexión guardada contra el `vncd` local; la pestaña del servidor
  activa el servicio y cambia la contraseña (capturas).

## Criterios de aceptación
- [x] Ventana de conexión con conexiones guardadas
- [x] Pestaña del servidor en Configuración
- [x] Test automático en `tests/` (`h273_vnc_gui.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D273)
