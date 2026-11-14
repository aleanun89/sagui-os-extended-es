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
| 1 | **Elegir la salida** | Altavoces, auriculares, salida de línea y **HDMI** (el audio HDMI de la gráfica es otro códec HDA: AMD en el dv7 y el Toshiba L850, Intel en el G470). Lista de salidas con su nombre, la elegida por defecto recordada, y cambio en caliente |
| 2 | **Detección de auriculares en marcha** | Respuestas no solicitadas del códec (*unsolicited responses*, *jack sense*): al enchufar, se callan los altavoces y al quitar vuelven, mientras suena. Aviso en pantalla opcional |
| 7 | **Sonidos del sistema** | Arranque, aviso, error y apagado, generados por el sistema (sin material ajeno), con interruptor y volumen propio |
| 8 | **Teclas multimedia** | Subir, bajar y silenciar con las teclas del portátil (códigos extendidos E0 20/2E/30 y las de cada fabricante), con indicador en pantalla |
| 9 | **Ajustes de conectores guardados** | Lo que hoy se prueba con `sonido --pin/--gpio/--eapd/--verbo` se puede **guardar** por modelo (códec + subsistema) en `sonido.conf` y se aplica en cada arranque, al estilo de `hda-jack-retask` de Linux: arreglar un portátil sin recompilar |

## v6.2 (con el vídeo)

| # | Mejora | Detalle |
| --- | --- | --- |
| 3 | **Micrófono y grabación** | Flujo de captura del HDA (y AC'97): elegir la entrada (micrófono interno, externo, línea), ganancia, medidor de nivel, `grabar archivo.wav [segundos]`, y la API en la libsagui y en Python. La usarán Mico, la IA (043) y el vídeo |
| 4 | **Volumen por aplicación** | Un volumen por flujo en el mezclador, con la ventana «Mezclador» (barra por programa) y un acceso desde la bandeja |
| 5 | **Balance y ecualizador** (ecualizador de 5 bandas hecho: D201, `ecualizador`, Configuración > Sonido; el balance sigue pendiente) | Balance izquierda/derecha, graves/agudos o ecualizador de 5 bandas (biquad en coma fija o SSE2), perfiles («altavoces del portátil», «auriculares») y uno por salida |
| 6 | **Calidad y latencia** | Conversión de frecuencia de mejor calidad, salida nativa a 48 kHz y 24 bits si el códec la admite, latencia configurable (hoy 100 ms fija) con aviso de cortes |

## v8 o más adelante

| # | Mejora | Detalle |
| --- | --- | --- |
| 10 | **Ahorro de energía** | Apagar el códec (estados D3) tras unos segundos de silencio, sin chasquidos al volver |
| 11 | **Audio USB** (USB Audio Class 1/2) | Auriculares, altavoces y micrófonos USB, sobre el EHCI y el xHCI de la 046; aparecen como una salida o entrada más |

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
- [ ] v8: ahorro de energía y audio USB
- [ ] Tests automáticos en `tests/`
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

Modelo sugerido: **Sonnet** para la mayoría (mezclador, ecualizador, grabación, Configuración);
**Opus** para las respuestas no solicitadas y el HDMI en hardware real, y para el audio USB.
