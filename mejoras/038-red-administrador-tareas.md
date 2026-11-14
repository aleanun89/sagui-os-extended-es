# 038 - Pestaña «Red» en el Administrador de tareas

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

El Administrador de tareas (D043, mejora 016) enseña procesos, rendimiento y ventanas, pero
nada de la red: para ver si hay IP, cuánto tráfico pasa o qué programa tiene una conexión
abierta había que ir al Terminal (`ifconfig -a`). Tampoco se podía renovar el DHCP ni cortar
una conexión desde la interfaz gráfica.

## Propuesta (aplicada)

Detalle en `docs/decisoes.md`, **D079**.

- `userland/programas/tareas.c`: cuarta pestaña **Red** y ventana redimensionable (D044):
  - por interfaz (`eth0`, `lo`): estado, driver, MAC, IPv4/máscara, puerta de enlace, DNS,
    DHCP o estática, bytes y paquetes, errores, descartes y velocidad de envío y recepción;
  - gráfico del tráfico de los últimos 60 s (enviado y recibido);
  - conexiones: protocolo, local, remoto, estado TCP, PID y proceso; ordenables por columna;
  - acciones: «Renovar DHCP», «Desactivar/Activar interfaz», «Cerrar conexión» (con
    confirmación), con botones y con el menú contextual (D051);
  - solo consulta la red con la pestaña visible, una vez por segundo.
- Kernel (`kernel/rede/`): `INFO_INTERFACES` 50, `INFO_SOCKETS` 51 (estado TCP del RFC 793,
  bytes, id), `INFO_TRAFEGO` 52 (60 s guardados por el kernel), `REDE_RENOVAR`,
  `REDE_DESATIVAR`/`REDE_ATIVAR` (en la pila, valen para cualquier driver), `REDE_FECHAR`
  (RST); `info_rede_t` crece por el final e `INFO_REDE` copia solo los bytes pedidos.
- **Loopback** `127.0.0.1` (interfaz `lo`), que la mejora 020 tenía pendiente.
- `ifconfig` enseña `lo` y acepta `ifconfig eth0 down|up`.

## Criterios de aceptación

- [x] Pestaña Red con interfaces, contadores, velocidad, gráfico y conexiones con PID
- [x] Renovar DHCP sin bloquear la ventana; desactivar y activar la interfaz
- [x] Cerrar una conexión con confirmación (el programa recibe el error)
- [x] La ventana se adapta al tamaño y no consulta la red fuera de la pestaña
- [x] `ifconfig`/`nc` siguen funcionando (h44)
- [x] Test automático en `tests/` que lo comprueba (`h66_red_tareas.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D079)

## Ideas siguientes

- Tráfico por proceso (sumar los bytes de sus conexiones) en la pestaña Procesos.
- Icono de red en la barra (con el estado y la velocidad al pasar el ratón).
- Gráfico guardado por el kernel también para `lo` (hoy lo acumula el programa).
- TIME-WAIT y un estado CLOSING propio cuando la pila lo tenga (D046).
