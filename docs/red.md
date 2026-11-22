# La pila de red: recepción, TCP y cómo medir la velocidad

La pila IPv4 está en `kernel/rede/` (`rede.c`: Ethernet, ARP, IP, ICMP, UDP, DHCP, DNS y la
tarea «rede»; `tcp.c`: TCP; `socket.c`: los sockets; `tunel.c`: WireGuard). Corre siempre con
las interrupciones apagadas: en la interrupción de la tarjeta (`ao_irq`) o en la tarea «rede».
Los drivers que necesitan esperar (Wi-Fi USB, Ethernet USB) nunca entran en la pila: dejan las
tramas en una cola y su propia tarea las mueve (D215, D433). La elección de la ruta tampoco espera
ni cede la CPU: solo mira el estado de las interfaces que la tarea «rede» ya leyó.

## Varias interfaces a la vez (D445)

Desde la v8.5.5.3 la pila tiene **varias interfaces activas a la vez** (hasta 4): las tarjetas PCI
(`eth0`, `eth1`... en el orden en que se buscan: e1000, RTL8139, NE2000 PCI, RTL8169/Atheros),
el Ethernet USB (`ure`, D433: `eth0` si no hay tarjeta PCI, si no la siguiente) y la Wi-Fi (`wlan0`,
D215, cuando está conectada). Antes había una sola: la Wi-Fi conectada aparcaba la `eth0` y el
USB solo se usaba sin tarjeta PCI.

- **Cada interfaz** (`iface_t` en `rede.c`) tiene su tarjeta, su dirección, máscara, puerta de
  enlace y DNS, su DHCP (estado, xid, concesión) y su desactivación (`ifconfig eth1 down`). Las
  entradas ARP y los paquetes que esperan respuesta ARP son de una interfaz; un ARP solo se
  contesta por la dirección de la interfaz por la que llegó.
- **Rutas** (sin tabla aparte, salen de las interfaces, como en Linux con una dirección por
  interfaz; `ifconfig route` o `ip route` las enseña):
  1. la **red local** de cada interfaz con dirección (`ip & máscara`, sin puerta de enlace); si
     dos interfaces están en la misma red, gana la de máscara más larga y, a igualdad, la mejor por
     métrica;
  2. una **ruta por defecto** por cada interfaz con puerta de enlace, con su **métrica** (cable 100,
     Wi-Fi 600; `ifconfig eth1 metric 50` o Configuración > Red > Interfaces): gana la de métrica
     menor **entre las que tienen enlace y no están desactivadas**; si ninguna lo tiene, la de métrica
     menor igualmente (con una sola tarjeta, sin cable, se intenta como siempre). A igualdad de
     métrica, la de índice menor (`eth0` antes que `eth1`).
  3. **El origen manda**: un paquete cuya dirección de origen es de una interfaz que puede llevarlo
     (está en su red local, o tiene puerta de enlace) sale por esa interfaz. El TCP guarda la dirección
     local de cada conexión (la que usó al abrirla, o a la que llegó el SYN): las conexiones siguen por
     su interfaz aunque cambie la ruta por defecto, y las respuestas a una conexión entrante salen con
     la dirección a la que se llamó. Las respuestas a un ping, igual.
- **Al caer una interfaz** (sin enlace, desactivada, el USB retirado o la Wi-Fi desconectada), la ruta
  por defecto pasa sola a la siguiente; la tarea «rede» mira el enlace de cada tarjeta en cada vuelta
  (`atualizar_enlaces`) y, con más de una interfaz, deja en el registro `[REDE] eth0: enlace caído` y
  `[REDE] ruta por defecto: eth1 (puerta 10.0.3.2)`. Las conexiones TCP que iban por la que cayó se
  pierden (su dirección ya no sale por ningún sitio que la conozca); al cambiar de red por defecto se
  vacía la caché de DNS.
- **La interfaz principal** es la de la ruta por defecto (o, si no hay, la primera con dirección): la
  que ven las llamadas sin nombre de interfaz (`REDE_CONSULTAR`, `INFO_REDE`, la bandeja de la barra,
  la línea del arranque), y **su DNS es el del sistema**. `SIS_CONFIG_REDE` lleva en el tercer argumento
  el nombre de la interfaz (`"eth1"`; 0 = la principal: todo lo compilado antes sigue igual).
- **ABI**: `info_rede_t` gana `metrica`, `rota_padrao`, `usb` y `wifi` sacados de los últimos 8 bytes de
  `diagnostico` (el tamaño no cambia); `info_arp_t.interface` (antes `reservado`) dice de qué interfaz es
  cada entrada; `INFO_INTERFACES` da hasta 6 (las físicas en orden, `wg0` y `lo`); `REDE_METRICA` (11).
- **IRQ**: una sola función (`ao_irq`) para todas las líneas de las tarjetas; dos tarjetas pueden
  compartir línea. Una tormenta en una línea sin dueño pasa todas a sondeo.
- **Límites**: cada driver lleva una sola tarjeta (dos e1000: solo la primera); sin enrutamiento entre
  interfaces (SaguiOS no reenvía paquetes de una red a otra); la dirección fija y el DNS a mano se
  ponen con `ifconfig` (Configuración solo renueva, activa/desactiva y cambia la prioridad); la métrica
  no se guarda entre arranques.
- **Con una sola tarjeta nada cambia**: mismos nombres, mismos mensajes del registro y de `ifconfig`
  (la métrica y la ruta por defecto solo se enseñan con más de una interfaz).

## Recepción (D437)

- Cada vuelta (`processar_chegadas`) vacía la tarjeta hasta **64 tramas** (`RX_POR_VOLTA`, un
  anillo). Si llega al tope, la tarea «rede» vuelve enseguida, cediendo antes el turno, hasta 4
  vueltas seguidas (`VOLTAS_SEGUIDAS`) en vez de esperar al tic siguiente (10 ms). Desde la
  interrupción, con el tope alcanzado, se despierta a la tarea.
- Con IRQ, cada interrupción hace una vuelta; sin IRQ («sin IRQ (sondeo cada 10 ms)» en
  `ifconfig`), la tarea hace una vuelta por tic.
- Lo que la vuelta no puede arreglar es lo que la tarjeta no ha entregado aún: el anillo de la
  RTL8169 (32 descriptores) o de la RTL8139 (8 KB) se llena antes en sondeo a 1 Gb/s, y el
  Ethernet USB (`ure`) trae una sola transferencia por tic. Eso es de la v8.5.5.3 (IRQ/MSI,
  varias lecturas USB en vuelo).

## TCP (D046, D437)

| | Antes (hasta la 8.5.5.1) | Desde la 8.5.5.2 (D437) |
|---|---|---|
| Anillo de recepción (ventana) | 32 KB | 128 KB (32 KB si no hay memoria seguida) |
| Anillo de envío | 16 KB | 64 KB (16 KB si no hay memoria) |
| Escala de ventana (RFC 7323) | no | sí: escala 2, solo si el otro lado también la ofrece |
| ACK | uno por segmento | retrasado: uno cada dos segmentos llenos o a los 40 ms |
| Tarjeta llena al enviar | el segmento se perdía y salía al vencer el RTO (≥ 200 ms) | el envío espera al siguiente ACK o tic |

- **Escala de ventana**: el SYN lleva MSS, NOP y escala 2; el SYN+ACK solo la lleva si el SYN
  del otro lado la trajo. Sin la opción de los dos lados no hay escala y la ventana anunciada se
  queda en 64 KB como mucho (el slirp de QEMU es así). La ventana del SYN nunca va escalada.
- **ACK retrasado**: solo para un segmento lleno (el mayor visto, a partir de 536 bytes), nuevo,
  en orden, que cupo entero y sin FIN, y como mucho uno pendiente. Lo pequeño (SSH interactivo,
  el final de una respuesta), lo repetido o fuera de orden, el FIN y el SYN llevan el ACK en el
  acto. Cualquier segmento que salga (datos, ventana) lleva el ACK y quita el pendiente.
- **Envío detenido**: `ip_enviar` sigue devolviendo `true` cuando la trama «sale» al cable; el TCP
  pregunta a `rede_envio_recusado()` si la tarjeta no tenía hueco (la cola del `lo` o del túnel
  llena ya hacía devolver `false` a `ip_enviar`) y, en los dos casos, no da el segmento por enviado.
- Sigue sin haber control de congestión, SACK ni cola de segmentos fuera de orden (un segmento
  fuera de orden se tira y el ACK repetido hace reenviar al otro lado).

## Medir

- En una máquina real: `dmesg | grep REDE`. Cada conexión de las 8 primeras deja
  `[REDE] TCP ip:puerto: MSS 1460, ventana 128 KB con escala de ventana (escala 2, la del otro lado 7)...`,
  y cada 10 s con tráfico sale
  `[REDE] rx N KB/s, tx M KB/s; T tramas en V vueltas (media, hasta X por vuelta; tope 64: K veces)`
  (con «, por sondeo» si no hay IRQ). Una media cerca de 1 trama por vuelta con mucho tráfico
  quiere decir que la tarjeta entrega de una en una.
- En QEMU: `tests/h437_velocidad_red.py` (con `SAGUI_VAZAO_MEDIR=1` solo mide y saca la tabla) y
  el programa `/testes/vazao baixar|subir|escutar`.
