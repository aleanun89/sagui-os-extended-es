# Ethernet USB Realtek (RTL8152, RTL8153, RTL8153B, RTL8156 y RTL8156B) — D433

> **Experimental y sin probar en hardware real.** Está escrito sin el adaptador delante, a partir del driver `if_ure` de
> OpenBSD (ver [`terceros.md`](terceros.md)). QEMU no emula estos chips: la prueba de verdad la hace quien tenga el adaptador
> (sección «Cómo probarlo»). Decisión: [`decisoes.md`](decisoes.md), D433.

## Qué hay

| Pieza | Fichero | Qué hace |
| --- | --- | --- |
| Driver | `kernel/rede/ure.c` | Reconoce el adaptador por VID:PID, arranca el chip, lo enlaza con la pila y envía y recibe tramas. |
| Lógica | `kernel/rede/ure_logica.h` | Secuencias de registros de cada familia, recepción agregada, trama de envío y velocidades. Probada en el PC. |
| Identificadores | `kernel/rede/ure_ids.h` | 60 VID:PID de la tabla de OpenBSD (Realtek y OEM). Generado por `tools/gerar_ure_ids.py`. |
| Registros | `kernel/terceros/bsd/ure/ureg.h` | Los `#define` de registros de OpenBSD, con su aviso de copyright. Generado por `tools/gerar_ureg.py`. |
| Prueba | `tests/h433_ure.py` | Lógica con ASan/UBSan, fuentes y QEMU sin adaptador. |

Chips soportados: **RTL8152** (`4c00`, `4c10`), **RTL8153** (`5c00`..`5c30`), **RTL8153B** (`6000`, `6010`), **RTL8156** (`7020`,
`7030`, con 2500 Mb/s) y **RTL8156B** (`7410`). Ningún chip de estos necesita firmware. Fuera de alcance: RTL8157/8159, RTL8153D
(`7420`) y las versiones desconocidas: se reconocen y el registro dice «sin soporte».

## Cómo se integra en la pila

- La pila tiene **una** interfaz activa. El adaptador USB se llama **`ure`** en `ifconfig` y ocupa **eth0** solo si **no hay**
  tarjeta PCI activa (`kernel/rede/rede.c`, `rede_usb_eth_registrar`).
- Con el Wi-Fi activo, el USB queda **aparcado** (como la eth0 PCI) y vuelve al desconectar el Wi-Fi.
- Con una tarjeta PCI activa, el USB queda **sin usar** y el registro lo dice.
- Sin IRQ: la pila lee en cada tic la transferencia de recepción ya terminada (`usb_rx_ler` no espera).
- **La pila nunca hace transferencias USB** (como con el Wi-Fi USB): una transferencia cede la CPU mientras espera y la pila
  corre con las interrupciones apagadas, así que otro proceso podría entrar en ella a medias. El envío deja la trama en una
  cola de 16 que vacía la tarea del kernel **«ure»**, y el enlace lo lee esa tarea cada 250 ms. La tarea es una sola: arranca
  el chip del adaptador enchufado, manda lo de la cola y mira el enlace; al retirarlo, espera al siguiente.
- Si el adaptador llega después del arranque (en caliente), se activa en el momento; si se retira, la pila se queda sin placa.
- **`make URE_PREFERIR=1`** (solo para probar): si al arrancar hay un adaptador enchufado, la pila no busca tarjetas PCI ni
  ISA y el USB queda de `eth0` aunque el equipo tenga red integrada. Al cambiarlo, `touch kernel/rede/rede.c` antes de `make`.

## Qué se ve en el registro

Con el adaptador enchufado, `dmesg` debe mostrar, en este orden (los valores son ejemplos):

```
[USB] conectado: Realtek USB 10/100/1000/2.5G LAN (0bda:8156) ...
[URE] adaptador Ethernet USB 0bda:8156 (...) - EXPERIMENTAL, sin probar en hardware
[URE] chip RTL8156 (versión 0x7020), velocidad USB alta, errores de control hasta ahora 0
[URE] RTL8156 lista: MAC xx:xx:xx:xx:xx:xx (rx por bulk de 16384 bytes)
[REDE] placa ure (Realtek RTL8156 (USB, experimental)), por sondeo (USB)
[URE] enlace arriba: 2500 Mb/s full-duplex
```

Si algo falla, el registro dice el paso: `[URE] no responde en el paso «…» (errores USB N): sin usar`. Los pasos son:
«identificar el chip», «autocarga del chip» (la más lenta: hasta 10 s, como OpenBSD), «carga de la flash», «reinicio del controlador»,
«control OOB», «inicio del PHY», «dirección MAC», «filtro de recepción», «anuncio de velocidades» y «transferencias de control
(el USB no contesta)». Otras líneas útiles: `[URE] recepción: trama descartada (código -N)`, `[URE] envío: error USB N`,
`[URE] enlace abajo`, `[URE] la configuración N no tiene los endpoints bulk del modo Realtek` (el adaptador arrancó en modo
CDC-ECM: no se usa).

En `ifconfig` (con la interfaz activa) el texto de diagnóstico dice el chip, la versión de chip, el dúplex y los contadores
de rx, tx, errores, cola de envío llena y recepciones casi llenas (tráfico alto).

## Cómo probarlo

1. Usa la imagen de prueba que te pase el coordinador, compilada con **`make URE_PREFERIR=1`**: el Amilo tiene la red
   integrada RTL8169 activa y, sin esa opción, el USB se queda «sin usar» (la pila tiene una sola interfaz). Su versión dice
   `v8.5.5.1` a propósito, para no pisar la del Amilo (la compilación sale de la rama `v8.5.5.2-ure`). Arranca el Amilo con el
   RTL8156 **ya enchufado en un puerto USB 2.0** (EHCI) y el cable de red en el adaptador, no en la toma del portátil. El
   registro debe decir `[REDE] URE_PREFERIR: hay un Ethernet USB Realtek; no se buscan tarjetas PCI ni ISA`.
2. Cuando haya arrancado, en el shell: `dmesg` (o `dmesg > /home/usuario/ure.txt` si quieres guardarlo) y busca las
   líneas `[URE]` y `[REDE]`. Copia también la línea `[USB] conectado` del adaptador.
3. `ifconfig`: la interfaz debe aparecer como `eth0` con el driver `ure`. Después `dhcp` y `ping 8.8.8.8`.
4. Si el adaptador no sale: saca el registro entero y dime en qué **paso** se queda (el texto `«…»`), y el número de errores USB.
5. Prueba de carga: descarga algo grande (`wget` de un fichero de decenas de MB) y mira en `ifconfig` que los errores de rx no
   suban y que la descarga termine.
6. Saca el adaptador **en mitad de la descarga**: debe salir `[URE] adaptador retirado` y `[REDE] Ethernet USB retirada`, sin
   que el sistema se congele. Vuélvelo a meter: con `URE_PREFERIR=1` la red PCI no estaba arrancada, así que el USB vuelve a
   ser la `eth0` (`[REDE] placa ure`); `dhcp` otra vez.
7. Con la imagen normal (sin `URE_PREFERIR`), el registro debe decir que el USB queda «sin usar» porque la RTL8169 está
   activa: es lo esperado hasta las varias interfaces de la 8.5.5.3.

## Límites conocidos (no probados)

- **Recepción de 16 KB.** `usb_rx_abrir` admite 16 KB por transferencia; OpenBSD usa 32 KB en el RTL8153 y el RTL8156. No se
  pierden ni se cortan tramas: el tamaño del búfer se le dice al chip (`USB_RX_EARLY_SIZE` se calcula con los 16 KB, como hace
  OpenBSD con los suyos), así que cierra cada transferencia antes de pasarse. Lo que cuesta es **velocidad**: una sola
  transferencia en vuelo, leída a cada tic de la pila (100 Hz), da del orden de 1-2 MB/s; con más tráfico el chip retiene y
  acaba descartando en el lado Ethernet (el TCP lo reenvía). Mejora pendiente: la cola de varias lecturas de D238
  (`usb_rx_abrir_fila`, como el Wi-Fi) y despertar la pila con la IRQ del xHCI.
- Una sola interfaz (eth0) y un solo adaptador Realtek a la vez.
- Sin descarga de sumas de control, sin VLAN, sin Wi-Fi ni EEE/ALDPS ajustados a mano; la tarea «ure» manda una transferencia
  por trama y espera a cada una (con el plazo de `usb_bulk`); si la cola de 16 se llena, la trama se pierde y se cuenta.
- Sin firmware: no hace falta, pero tampoco se han probado los chips más nuevos (RTL8157/8159, RTL8153D).
