# 045 - Rendimiento de la red: ventana grande, control de congestión, ACK retardado, SACK

- **Estado:** propuesta (en cola, sin prisa)
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Relacionadas:** 020 (red), 038 (pestaña Red), 044 (red por USB)

## Problema / motivación

La pila TCP (`kernel/rede/tcp.c`, D046) es correcta pero conservadora:

| Limitación actual | Efecto |
| --- | --- |
| Anillo de recepción de **32 KB**; ventana anunciada ≤ 65.535 sin *window scaling*; solo la opción MSS | velocidad máxima ≈ búfer ÷ RTT: con 50 ms de RTT (Internet), **~640 KB/s** aunque el enlace dé mucho más; en red local apenas se nota |
| Sin control de congestión («manda lo que la ventana del otro lado permite») | en Internet, ráfagas que provocan pérdidas y retransmisiones |
| Sin SACK; solo retransmisión rápida con 3 ACK duplicados | recuperación lenta cuando se pierden varios segmentos |
| Un ACK por segmento; sin Nagle | más paquetes de los necesarios |
| Copias driver → pila → anillo → programa | más CPU por byte, que importa en PC antiguos |
| EHCI por consulta y una transferencia cada vez (`kernel/usb/ehci.c`) | limita al adaptador USB (044) y a los pendrives |

## Propuesta (por orden de efecto/esfuerzo)

1. **Búferes grandes + escalado de ventana** (RFC 7323):
   - anillos de recepción y envío configurables (256 KB-1 MB por conexión, con un límite
     global);
   - opción *Window Scale* en el SYN;
   - opcionalmente *Timestamps* (RTTM y PAWS).
2. **Control de congestión**: arranque lento, evitación de congestión, retransmisión rápida
   y recuperación **NewReno** (RFC 5681, 6582), con `cwnd`/`ssthresh` por conexión y reinicio
   tras inactividad.
3. **ACK retardado** (cada 2 segmentos o 40-200 ms) y **Nagle** (RFC 1122/896), con
   `TCP_NODELAY` en `SIS_OPCAO_SOCKET` para quien lo necesite (terminal remoto, `nc`
   interactivo).
4. **SACK** (RFC 2018/6675): recepción con huecos y retransmisión selectiva.
5. **Menos copias**: recepción directa al anillo y lecturas grandes desde el programa.
   Medir antes de complicar.
6. **EHCI**: interrupciones y varias transferencias en cola; beneficia a la 044 y a los
   discos USB.
   **Hecho (2026-11-07, D095, rama `usb-rapido`)**, con otro alcance del que se pensó: lo que
   frenaba al pendrive no era la falta de colas sino los pedidos de 1 KB y las escrituras de
   metadatos sector a sector. Ahora: comandos de hasta 128 KB, FAT32 por rachas de clústeres
   seguidos con lectura anticipada, espera por IRQ del EHCI (con IF activo: el proceso
   duerme y los demás siguen), trabas para que otro proceso pueda entrar en el FS y en el
   driver mientras el primero espera. Medido en QEMU con KVM (`tests/h73_usb_rapido.py`):
   lectura 52 → **5.640 KB/s**, escritura 21 → **2.900 KB/s**, y el escritorio sigue
   respondiendo durante la copia. **Sigue pendiente** lo que el punto decía de «varias
   transferencias en cola»: encadenar CBW + datos + CSW (una espera por comando en lugar de
   tres) y tener varios comandos en vuelo; ver D095 («Lo que NO se hizo»).
7. **Estadísticas para la pestaña Red** (038) e `infopc`: retransmisiones, `cwnd`, RTT
   suavizado y ventana por conexión.

## Medida y pruebas

- **Banco de pruebas en el anfitrión, sin Internet:**
  - un servidor HTTP con archivos grandes;
  - un **proxy TCP en Python que añade latencia y pérdidas** configurables (p. ej. 50 ms y
    1 %), porque `tc netem` necesita sudo.
- **Programa o prueba de rendimiento:** `wget` de 20-50 MB con hash, en tres escenarios:
  - red local (0 ms);
  - 50 ms de latencia;
  - 50 ms + 1 % de pérdidas.
  Se registran MB/s antes y después de cada paso, con TCG y con KVM (`SAGUI_KVM=1`).
- **Corrección:**
  - las pruebas h44, h47, h59, h62 y h66 siguen pasando;
  - la descarga con pérdidas termina con el hash correcto;
  - `nc` interactivo sin retrasos con `TCP_NODELAY`.

## Criterios de aceptación

- [ ] Escalado de ventana y búferes grandes: con 50 ms de latencia, al menos 4 veces más
      rápido que ahora
- [ ] NewReno: con 1 % de pérdidas la descarga es estable y correcta
- [ ] ACK retardado y Nagle con `TCP_NODELAY`
- [ ] (Opcional) SACK y menos copias, con medida
- [ ] Banco de pruebas y test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`
