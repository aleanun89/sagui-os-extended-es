# Micrófono y grabación de sonido (D292, mejora 075 punto 3)

Hasta la v8.2 el sistema no grababa nada. Desde la D292 hay una entrada de sonido en el **Intel HDA** y en el **AC'97**,
una API en la libsagui, la orden `grabar`, la **Grabadora** y los ajustes de micrófono, balance y latencia en
**Configuración > Sonido**. Lo decidido está en la D292 (`docs/decisoes.md`); la prueba, en `tests/h292_microfono.py`.

## Lo que ve el usuario

- `grabar --info`: la tarjeta, las entradas (la elegida lleva `*`) y la ganancia.
- `grabar nota.wav [segundos]`: graba a 44100 Hz, mono, 16 bits (por defecto 5 s). Opciones: `-r HZ` (8000 a 48000), `-e` estéreo,
  `-t MS` (milisegundos), `-i N` elige la entrada, `-g N` la ganancia (0-100), `--silencio` / `--activar`, `-p` la fuente de prueba.
- **Grabadora** (menú Multimedia): Grabar/Parar, Reproducir, Guardar (diálogo común D272, `/home/grabaciones`), medidor de nivel y
  un historial del nivel; `<` `>` cambian de entrada y «Prueba» usa la fuente de prueba. Graba en memoria hasta 120 s.
- **Configuración > Sonido**: a la derecha, el micrófono (entrada, ganancia, silencio y un medidor en vivo: «habla para
  probar»); a la izquierda, balance, «Probar izq./der.» (1 kHz medio segundo por un solo canal), latencia (20 a 300 ms) y, si hay
  varias tarjetas, la salida elegida. Todo se guarda en `/home/.config/sagui.conf` (`balance=`, `latencia=`, `micro_entrada=`,
  `micro_ganancia=`, `micro_silencio=`, `salida=`) y `configuracion --aplicar` lo restablece en cada arranque. También por
  línea de órdenes: `configuracion balance -100..100`, `latencia MS`, `microfono entrada N|ganancia N|silencio|activar`, `salida N`.
- **Ajustes de conectores por modelo** (mejora 075 punto 9): `sonido --guardar --pin 1a on` (también `--gpio`, `--eapd`, `--auto`,
  `--verbo`) hace la orden y la guarda en `/home/.config/sonido.conf` bajo `[codec:subsistemaPCI:subsistemaCodec]`; `sonido --aplicar`
  (lo lanza `configuracion --aplicar`) las repite, `sonido --guardados` las lista y `sonido --olvidar` las borra.
- Los **servicios** (`de_servico`: cron, sshd...) no pueden grabar (`ERRO_ACESSO`), como no pueden usar la cámara (D277).

## La API (`userland/inc/captura.h`)

Una sola llamada del sistema ya existente: `info_sistema(INFO_CAPTURA = 75, &info_captura_t, sizeof)` (no se añade número de llamada
nuevo: `SIS_MAX` sigue en 136). El campo `orden` elige `CAP_ESTADO`, `CAP_ABRIR`, `CAP_LER`, `CAP_FECHAR`, `CAP_ENTRADA`, `CAP_GANANCIA`
(`kernel/inc/abi.h`).

```c
#include "captura.h"
info_captura_t i;
captura_estado(CAP_FUENTE_REAL, &i);        // i.presente, i.nome, i.n_entradas, i.entradas[k], i.entrada, i.ganancia, i.pico, i.rms
captura_abrir(CAP_FUENTE_REAL, 44100, 1);   // PCM 16 bits, 8000 a 48000 Hz, 1 o 2 canales
n = captura_ler(buf, tam);                  // bytes ya convertidos; 0 = aún no hay (no bloquea); captura_ler_espera(buf, tam, ms)
captura_cerrar();
captura_entrada(k);  captura_ganancia(pct, mudo);   // valen con la captura cerrada
captura_gravar_wav("a.wav", CAP_FUENTE_REAL, 44100, 1, 5000, NULL);   // todo junto
```

Una captura abierta en todo el sistema (`ERRO_OCUPADO` para el segundo); si el dueño muere, el tic la cierra. Otra fuente: `CAP_FUENTE_PRUEBA`,
un tono de 1 kHz (amplitud 8000) generado en el kernel que pasa por la misma cola y la misma conversión (sirve para probar sin micrófono).
Los números nuevos: `INFO_CAPTURA` = 75; en `SIS_AUDIO_CONTROLE`, `AUDIO_BALANCE` = 8 (0 izquierda, 100 centro, 200 derecha) y `AUDIO_LATENCIA` = 9 (ms).

## Cómo está hecho

- `kernel/dispositivos/captura.c`: capa común. En cada tic vacía el anillo de la tarjeta (16384 cuadros de 48 kHz estéreo) a una cola
  del kernel de 340 ms, mide pico y RMS y convierte al leer (interpolación lineal en punto fijo 16.16; mono = media). Sin IRQ.
- `hda.c`: el primer descriptor de flujo de entrada (`0x80`), etiqueta de flujo 2, sin IOC. Entradas: pines de entrada analógicos (micrófono
  interno/externo según la ubicación de su configuración por defecto, línea, auxiliar) con camino hasta un conversor de entrada (ADC) por selectores
  y mezcladores; elegir una activa el pin (VREF 80 % o 50 % en los micrófonos), selecciona la conexión en cada selector y calla las demás entradas
  de los mezcladores. La ganancia va en el primer amplificador de entrada del camino con pasos.
- `ac97.c`: canal PCM IN (`NABM+0x00`), 32 descriptores de 512 cuadros; entradas micrófono, línea y auxiliar por el selector de grabación
  (`0x1A`); ganancia en `0x1C` (pasos de 1,5 dB).
- `audio.c`: balance (atenúa el canal del lado contrario antes del ecualizador; centrado no toca nada, bit a bit) y latencia base configurable.

## Límites

- **QEMU no inyecta audio de entrada**: `-audiodev wav` solo graba la salida (con él la tarjeta emulada ni siquiera avanza su posición de
  entrada) y `-audiodev none` hace que grabe silencio avanzando de verdad. La prueba comprueba el DMA (≈24 000 cuadros en 500 ms, silencio exacto)
  y todo lo demás con la fuente de prueba. El nivel real del micrófono y la ganancia del códec solo se ven en hardware.
- **Micrófono USB (UAC 1)**: sin hacer. La entrada isócrona ya existe en los cuatro controladores (D277), pero QEMU no emula un micrófono USB para
  probarla; queda como siguiente paso (analizar la alternativa de AudioStreaming de entrada, `usb_iso_abrir` con `entrada = true` y rellenar el
  anillo de `captura_hw_t`).
- Volumen por aplicación y sonidos del sistema (arranque, aviso, error): sin hacer (no existen sonidos del sistema que conmutar; la única marca
  actual, el clic de las capturas, ya tiene su interruptor).
- Mono: media de los dos canales; si un micrófono solo llena el izquierdo, el nivel baja 6 dB.

## Qué probar en hardware real

1. **dv7 (IDT 92HD7x, micrófono interno)**: `grabar --info` (¿aparece «Micrófono interno»?), `grabar -g 70 prueba.wav 5` hablando y abrir el WAV; si sale
   silencio, mirar con `sonido --codec` qué pin es el micrófono y si el ADC conecta con él, y si hace falta GPIO/EAPD (`sonido --guardar --gpio ...`).
2. **Toshiba L850 (Realtek ALC269)**: pin 0x12 (micrófono interno) y 0x18/0x19 (externo); ¿el micrófono necesita refuerzo (+20/30 dB)? Probar ganancia 100.
3. **G470 (Realtek/Conexant)**: igual; y el micrófono externo con la clavija de auriculares (retasking).
4. En todas: Configuración > Sonido: el medidor debe moverse al hablar; `grabar` con la fuente de prueba (`-p`) debe dar un tono limpio de 1 kHz.
5. Posición del anillo: algunos HDA reales dan un `LPIB` de entrada poco exacto; si la grabación sale con saltos, pasar a la zona de posición en memoria (DPLBASE).
