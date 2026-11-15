# 075 - Mejoras del sonido configurables desde el sistema

- **Estado:** propuesta, repartida entre la **v6.1**, la **v6.2** y la **v8** (decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 041 (sonido: HDA, AC'97, mezclador), D107 (ajustes de códec IDT y `sonido`), D112 (Configuración), D114 (bandeja), 048 (vídeo), 043 (IA), 046 (xHCI)

## Punto de partida

Drivers HDA y AC'97, mezclador, volumen general (bandeja y Configuración), ajuste por modelo
de los códecs IDT de HP y el programa `sonido` con diagnóstico y pruebas a mano (`--gpio`,
`--pin`, `--eapd`). Todo lo nuevo se configura desde la pestaña **Sonido** de Configuración, el
comando `sonido` y la bandeja, y se guarda en `/home/.config/sonido.conf`.

## v6.1 (lo pequeño y más útil)

| # | Mejora | Detalle |
| --- | --- | --- |
| 1 | **Elegir la salida** (la elección de tarjeta se **recuerda desde la D292**: `configuracion salida N`, botón en Configuración > Sonido; falta la elección de salida dentro del códec y el HDMI) | Altavoces, auriculares, salida de línea y **HDMI** (el audio HDMI de la gráfica es otro códec HDA: AMD en el dv7 y el Toshiba L850, Intel en el G470). Lista de salidas con su nombre, la elegida por defecto recordada, y cambio en caliente |
| 2 | **Detección de auriculares en marcha** | Respuestas no solicitadas del códec (*unsolicited responses*, *jack sense*): al enchufar, se callan los altavoces y al quitar vuelven, mientras suena. Aviso en pantalla opcional |
| 7 | **Sonidos del sistema** | Arranque, aviso, error y apagado, generados por el sistema (sin material ajeno), con interruptor y volumen propio |
| 8 | **Teclas multimedia** | Subir, bajar y silenciar con las teclas del portátil (códigos extendidos E0 20/2E/30 y las de cada fabricante), con indicador en pantalla |
| 9 | **Ajustes de conectores guardados** (**hecho en la D292**: `sonido --guardar/--aplicar/--guardados/--olvidar`, `/home/.config/sonido.conf`) | Lo que hoy se prueba con `sonido --pin/--gpio/--eapd/--verbo` se puede **guardar** por modelo (códec + subsistema) en `sonido.conf` y se aplica en cada arranque, al estilo de `hda-jack-retask` de Linux: arreglar un portátil sin recompilar |

## v6.2 (con el vídeo)

| # | Mejora | Detalle |
| --- | --- | --- |
| 3 | **Micrófono y grabación** (**hecho en la D292** para HDA y AC'97: `grabar`, Grabadora, `INFO_CAPTURA`, Configuración > Sonido, `docs/microfono.md`; sin probar en hardware; falta el micrófono USB) | Flujo de captura del HDA (y AC'97): elegir la entrada (micrófono interno, externo, línea), ganancia, medidor de nivel, `grabar archivo.wav [segundos]`, y la API en la libsagui y en Python. La usarán Mico, la IA (043) y el vídeo |
| 4 | **Volumen por aplicación** | Un volumen por flujo en el mezclador, con la ventana «Mezclador» (barra por programa) y un acceso desde la bandeja |
| 5 | **Balance y ecualizador** (ecualizador de 5 bandas hecho: D201; **balance hecho en la D292**: `configuracion balance`, barra en Configuración > Sonido) | Balance izquierda/derecha, graves/agudos o ecualizador de 5 bandas (biquad en coma fija o SSE2), perfiles («altavoces del portátil», «auriculares») y uno por salida |
| 6 | **Calidad y latencia** | Conversión de frecuencia de mejor calidad, salida nativa a 48 kHz y 24 bits si el códec la admite, latencia configurable (**hecha en la D292**: 20 a 300 ms, `configuracion latencia`; falta el aviso de cortes, 48 kHz/24 bits y la mejor conversión de tasa) |

## v8 o más adelante

| # | Mejora | Detalle |
| --- | --- | --- |
| 10 | **Ahorro de energía** | Apagar el códec (estados D3) tras unos segundos de silencio, sin chasquidos al volver |
| 11 | **Audio USB** (USB Audio Class 1/2) | Auriculares, altavoces y micrófonos USB, sobre el EHCI y el xHCI de la 046; aparecen como una salida o entrada más. **Hecho en parte (v8.1, D275):** salida UAC 1 (PCM 16 bits estéreo) por xHCI, UHCI y OHCI, varias tarjetas en el mezclador, `volumen salida`, retirada en caliente; faltan UAC 2, mono y 24 bits, micrófonos (entrada isócrona) y el siTD del EHCI |

## Pruebas

- QEMU (intel-hda con `hda-duplex`/`hda-micro`, AC'97):
  - elegir la salida;
  - grabación (QEMU puede inyectar un WAV como micrófono);
  - volumen por aplicación con dos flujos;
  - ecualizador comparado con una referencia en el anfitrión;
  - teclas multimedia por QMP;
  - ajustes guardados que se aplican tras reiniciar.
- Real:
  - auriculares enchufados en marcha en el dv7, el Toshiba L850 (Realtek ALC269) y el G470;
  - HDMI a una televisión;
  - micrófono interno.

## Criterios de aceptación

- [ ] v6.1: salida elegible (incluido HDMI), auriculares en marcha, sonidos del sistema, teclas multimedia, ajustes de conectores guardados
- [ ] v6.2: grabación, volumen por aplicación, ecualizador, 48 kHz/24 bits y latencia configurable
- [ ] v8: ahorro de energía y audio USB (audio USB UAC 1 de salida hecho en la v8.1, D275, `tests/h275_audio_usb.py`; falta el ahorro de energía)
- [x] D292: micrófono (HDA, AC'97), balance, latencia, salida recordada y conectores guardados; pruebas en `tests/h292_microfono.py`; pendientes: micrófono USB, volumen por aplicación, sonidos del sistema, 24 bits
- [ ] Tests automáticos en `tests/`
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

Modelo sugerido: **Sonnet** para la mayoría (mezclador, ecualizador, grabación, Configuración);
**Opus** para las respuestas no solicitadas y el HDMI en hardware real, y para el audio USB.
