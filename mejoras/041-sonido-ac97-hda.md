# 041 - Sonido: drivers AC'97 e Intel HDA, mezclador y reproducción de WAV

- **Estado:** hecha en QEMU (rama `sonido`); falta probarla en un portátil real
- **Prioridad:** alta
- **Alta:** 2026-11-07
- **Relacionadas:** 036 fase B, 034 (CHIP-8: patrón XO-CHIP), 048 (vídeo), D073

## Problema / motivación

SaguiOS solo tiene el pitido del altavoz del PC (B3). Los juegos, CHIP-8 (cuyo patrón de
sonido XO-CHIP hoy solo se aproxima) y el futuro reproductor de vídeo necesitan sonido de
verdad. Casi todos los PC de 1999-2006 tienen **AC'97** y los de 2005 en adelante **Intel
HDA**: el dv7, el Amilo y el Ryzen son HDA.

## Propuesta

- **Drivers, solo reproducción:**
  - **AC'97**: Intel ICH y compatibles (QEMU `-device AC97`), con *bus master* PCM out,
    lista de descriptores y volumen maestro/PCM;
  - **Intel HDA**: controlador estándar (QEMU `-device intel-hda -device hda-duplex` o
    `hda-output`), CORB/RIRB, recorrido de los widgets del códec, ruta DAC → mezclador →
    pin de salida (altavoz/auriculares), *stream* de salida con BDL y volumen/silencio por
    amplificador.
  - Formato: PCM de 16 bits estéreo a 48 kHz (y 44,1 kHz si el códec lo admite).
  - Interrupciones con reserva por sondeo.
  - Con la D073 se pueden usar *quirks* de pines por códec tomados de Linux, aislados y
    registrados.
- **Mezclador en el kernel:**
  - varios programas suenan a la vez;
  - conversión de frecuencia (8-48 kHz) y de mono a estéreo;
  - volumen por flujo y maestro;
  - búfer circular con latencia de ~50-100 ms.
- **API:**
  - syscalls (**96-99**, con `SIS_MAX` subido) para abrir un flujo, escribir muestras,
    volumen/estado y cerrar;
  - envoltorios en la libsagui (`audio_abrir`, `audio_escrever`...);
  - `/dev/audio` si encaja en el VFS;
  - `INFO_AUDIO` (53) para `infopc` y el Administrador de tareas.
- **Programas:**
  - `reproducir archivo.wav` (PCM 8/16 bits, mono/estéreo, cualquier frecuencia razonable);
  - `volumen [0-100|silencio]`;
  - control de volumen en la barra si es barato.
- **Usos inmediatos:**
  - CHIP-8 con el **patrón XO-CHIP real**;
  - efectos en Banana y Sagui 3D;
  - `sagui.sonido` en Python.
  - El pitido del altavoz (B3) se mantiene como reserva.

## Pruebas

- **QEMU:** `-audiodev wav,id=a0,path=salida.wav` con `AC97` y con `intel-hda` + `hda-duplex`.
  Se reproduce un WAV de prueba generado y se compara el `.wav` grabado (frecuencia
  dominante, duración y nivel); también la mezcla de dos flujos y el silencio.
- **`run_in_qemu.sh --sonido`:** añade la tarjeta (HDA por defecto) con salida por PulseAudio
  (WSLg).
- **Real:** HP dv7, Amilo y Ryzen B450 (códec según `infopc --informe`).

## Criterios de aceptación

- [x] AC'97 y HDA en QEMU, con audio grabado y comprobado
- [x] Mezcla de varios flujos y volumen
- [x] `reproducir` y `volumen`; CHIP-8 con el patrón XO-CHIP
- [ ] Probado en al menos un portátil real
- [x] Test automático en `tests/` (`h70_sonido.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D088-D090)

## Lo hecho

- **Núcleo:** `kernel/dispositivos/audio.c` (mezclador, flujos, syscalls 96-99, `INFO_AUDIO`),
  `ac97.c` (Intel ICH, SiS 7012, NVIDIA, AMD), `hda.c` (CORB/RIRB o comando inmediato,
  recorrido de los *widgets*, todas las salidas analógicas con EAPD), `irq_compartilhar()` en
  `idt.c` para las líneas PCI compartidas; el mezclador corre en la IRQ de la tarjeta y en cada
  tic del reloj (sondeo). Los flujos son descriptores (`A_AUDIO`): se callan al cerrar o al
  terminar el programa. `SIS_MAX` = 128.
- **Programas:** `reproducir` (WAV PCM 8/16 bits, mono/estéreo, 4-96 kHz, en streaming: empieza
  con ~0,3 s leídos y sigue leyendo mientras suena), `volumen`, `infopc` (la tarjeta, el códec, la IRQ y los cortes), la ayuda del
  shell, CHIP-8 con el patrón XO-CHIP real, efectos de Banana y Sagui 3D (`efeito_tocar`) y
  `sagui.Sonido` / `sagui.volumen` en Python.
- **Pruebas:** `tests/h70_sonido.py` (AC'97, HDA, sin tarjeta y CHIP-8 en el escritorio),
  con el audio grabado por QEMU en un `.wav` y analizado (frecuencia, duración, nivel, mezcla,
  volumen y silencio). `tests/qemu.py` acepta `Maquina(som="ac97"|"hda"|"hda-output")`.
- **`run_in_qemu.sh --sonido`:** altavoz y tarjeta HDA por PulseAudio (`--ac97` para la otra).
- **Ajustes por códec y subsistema (D107, rama `sonido-dv7`):** el HP dv7 (IDT 92HD75B
  `111d:7603`, subsistema `103c:3063`) solo daba sonido por el subwoofer. Con los datos de Linux
  (`patch_sigmatel.c`, aislados en `kernel/terceros/linux/hda/`) el driver aplica a la familia
  IDT 92HD71B/75B «GPIO0 = EAPD» (el amplificador de los altavoces) y el ajuste de la tabla de
  subsistemas (dv7 `103c:3063` → «cualquier HP»: tomas A y F como auriculares; dv4/dv5/dv6:
  además GPIO5 de graves y el pin 0x0d). Elección pura en `hda_ajustes.c`, probada en el
  anfitrión. **Autosilencio:** al empezar a sonar, con auriculares enchufados (detección de
  presencia) se callan los altavoces.
- **Diagnóstico:** `sonido` (syscall 102, `SIS_AUDIO_HDA`): resumen, `--codec` (volcado de todos
  los nodos), `--tono`, y `--gpio`, `--pin`, `--eapd`, `--auto`, `--verbo` para probar a mano en
  el portátil sin recompilar. Instrucciones para el dv7: `testes_hw/hp-dv7-sonido-pruebas.md`.
  Prueba: `tests/h79_sonido_hp.py`.

## Pendiente

- El pendrive (EHCI) lee a ~48 KB/s reales en QEMU (con y sin KVM): un WAV de 44,1 kHz
  estéreo desde el pendrive empieza enseguida pero corta a menudo; desde `/tmp` o un disco
  suena entero. Es la velocidad del USB (mejora 045), no del sonido.
- Probar en el HP dv7, el Amilo y el Ryzen B450 (`reproducir` con un WAV y el informe).
- **HP dv7:** probar el ajuste `idt71-hp` (D107) con `testes_hw/hp-dv7-sonido-pruebas.md`; si
  no basta, con el volcado de `sonido --codec` y las pruebas a mano se añade su fila propia.
- Detección de auriculares en vivo (hoy solo al empezar a sonar, D107) y control de volumen en
  la barra de tareas.
- Grabación (micrófono) y salida digital (HDMI): fuera del alcance de esta mejora.

- **Reproductor de música gráfico** (v5.2, D115): programa `musica` sobre el mezclador (escritura no
  bloqueante, `AUDIO_DESCARTAR` para pausa y salto); prueba `tests/h86_musica.py`.
