# 073 - Cortafuegos (firewall)

- **Estado:** propuesta (**v8**, decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media (seguridad)
- **Alta:** 2026-11-08
- **Relacionadas:** red (D046, `kernel/rede/`), lite-monkey (067, puerto 7979), Python web (065),
  SSH (052), Wi-Fi (072), bandeja (D114), Configuración (D112), `/logs` (D120)

## Motivación

Hoy cualquier programa puede escuchar en un puerto (`servidor`, lite-monkey en el 7979, una app
Microdot, un futuro SSH), y cualquiera de la red local, o de una Wi-Fi pública con la 072,
puede conectarse. Un cortafuegos en el kernel decide qué entra y qué sale.

## Propuesta

### Filtro de paquetes en la pila IPv4 del kernel

- **Puntos de control:**
  - **entrada**, antes de entregar a TCP, UDP o ICMP;
  - **salida**, antes de enviar.

  Con una sola tabla de reglas, evaluada en orden y con política por defecto.
- **Reglas por:** dirección (entrada o salida), protocolo (TCP, UDP, ICMP), puerto o rango,
  IP o red de origen y destino (`192.168.1.0/24`), interfaz (`eth0`, `wlan0`) y acción
  (permitir, rechazar con RST o ICMP, descartar).
- **Con estado:** tabla de conexiones TCP y UDP; las respuestas a conexiones que el propio
  SaguiOS inició pasan solas.
- **Por programa** (lo que más se nota en el día a día): como los sockets del kernel saben qué
  proceso los abrió, se puede permitir o bloquear por nombre de programa
  («`litemonkey` puede escuchar en 7979 solo desde la red local»).
- **Política por defecto pensada para un escritorio:**
  - salida permitida;
  - entrada **bloqueada** salvo lo establecido;
  - al escuchar un programa en un puerto por primera vez, **preguntar** («`litemonkey` quiere
    aceptar conexiones en el puerto 7979: ¿permitir desde la red local / desde todas / no?»),
    y recordar la respuesta.
  - Sin escritorio, se aplica la política y se anota.
- **Protecciones básicas:**
  - límite de conexiones nuevas por segundo y por IP (contra escaneos e inundaciones de SYN);
  - descartar paquetes malformados;
  - no responder a pings desde fuera si se desea (modo «invisible»).
- **Registro:** lo bloqueado va al log del kernel y a `/logs/cortafuegos-*.log`, si hay partición
  de logs, con límite para no llenar el disco.

### Herramientas

- **`cortafuegos`** en el terminal: `estado`, `reglas`, `permitir`, `bloquear`, `borrar`,
  `politica`, `registro`, `activar`/`desactivar`, y `conexiones` (como `netstat`, con el
  programa de cada una).
- Reglas guardadas en `/home/.config/cortafuegos.conf` (o `/etc` como reserva), cargadas al
  arrancar **antes** de levantar la red.
- **Interfaz gráfica:** pestaña «Cortafuegos» en Configuración (activar, perfiles «Casa» y «Red
  pública», programas permitidos), aviso emergente al escuchar por primera vez, y un escudo en
  el icono de red de la bandeja con el número de bloqueos recientes.
- Pestaña Red del Administrador de tareas: conexiones activas con su programa.

## Robustez

- El cortafuegos nunca debe dejar el sistema sin red por error: opción de arranque
  «sin cortafuegos» y `cortafuegos desactivar` siempre a mano en el terminal local.
- La tabla de estado tiene límite de tamaño y caducidad; si se llena, se aplica la política sin
  colgarse.
- Coste medido: el reenvío por regla debe ser despreciable frente a la pila (pocas reglas,
  búsqueda lineal; más adelante, por puerto).

## Pruebas

- QEMU con la red de usuario y `hostfwd`:
  - un servidor en SaguiOS es alcanzable o no según las reglas;
  - se comprueba el estado (las respuestas pasan), el permiso por programa, la pregunta en el
    escritorio (capturas), el registro y el límite de conexiones (ráfaga desde el anfitrión).
- Las pruebas de red existentes (h44, h59 TLS, h88 lite-monkey...) pasan con el cortafuegos
  activo y su política por defecto.

## Criterios de aceptación

- [ ] Entrada bloqueada por defecto, salida permitida, respuestas con estado
- [ ] Reglas por puerto, IP/red, protocolo y programa; pregunta al escuchar por primera vez
- [ ] `cortafuegos` en el terminal y pestaña en Configuración; registro de bloqueos
- [ ] Batería de red completa con el cortafuegos activo
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Modelo sugerido: **Opus** (pila de red del kernel y seguridad).
- IPv6 no existe aún en SaguiOS; cuando llegue, el cortafuegos debe cubrirlo desde el principio.
