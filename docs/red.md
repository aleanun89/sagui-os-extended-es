# La pila de red: recepción, TCP y cómo medir la velocidad

La pila IPv4 está en `kernel/rede/` (`rede.c`: Ethernet, ARP, IP, ICMP, UDP, DHCP, DNS y la
tarea «rede»; `tcp.c`: TCP; `socket.c`: los sockets; `tunel.c`: WireGuard). Corre siempre con
las interrupciones apagadas: en la interrupción de la tarjeta (`ao_irq`) o en la tarea «rede».
Los drivers que necesitan esperar (Wi-Fi USB, Ethernet USB) nunca entran en la pila: dejan las
tramas en una cola y su propia tarea las mueve (D215, D433).

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
