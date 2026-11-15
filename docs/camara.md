# Cámaras web (UVC) en SaguiOS (v8.1, D277)

## Lo que ve el usuario

- Al enchufar (o al arrancar con) una cámara UVC, el kernel escribe `[UVC] ...: camara UVC 1.10, N formato(s), M tamano(s), K alternativa(s)
  isocrona(s)`; la cámara **no se enciende** hasta que una aplicación la abre. `usb` añade la línea `Camara UVC: ...` con cuadros, descartados, paquetes y errores.
- **Inicio > Gráficos > Cámara**: vista previa en vivo, **Foto** (diálogo de guardar; por defecto `/home/fotos/foto-AAAAMMDD-HHMMSS.png`; PNG siempre, JPEG solo
  con cámaras MJPEG), `<` y `>` para cambiar de tamaño y formato, `B-`/`B+` y `C-`/`C+` (brillo y contraste, si la cámara los tiene) y **Prueba** para usar la
  fuente de prueba (un patrón generado por el kernel) cuando no hay cámara. Espacio o `F` sacan la foto, `P` alterna la fuente de prueba.
- En el terminal: `camara --info` (cámara y modos), `camara -p --info` (la fuente de prueba),
  `camara [-p] [--modo N] --foto archivo.png [--cuadros N]` (sin ventana: descarta los primeros cuadros y guarda el último).

## La interfaz para otros programas

Una sola llamada: `info_sistema(INFO_CAMARA, &info, sizeof info)` con un `info_camara_t` (`kernel/inc/abi.h`); el campo `orden` dice qué se pide. La capa
`userland/inc/camara.h` (libsagui) la envuelve:

```c
#include "camara.h"
info_camara_t i;
camara_estado(CAM_FUENTE_REAL, &i);            // i.presente, i.nome, i.n_modos, i.modos[k] = { larg, alt, formato (CAM_FMT_YUY2 / CAM_FMT_MJPEG), fps }
camara_abrir(CAM_FUENTE_REAL, modo, &i);       // empieza a capturar con ese modo (CAM_FUENTE_PRUEBA = la fuente de prueba)
unsigned seq = 0;
n = camara_cuadro(&i, buf, tam, seq);          // n > 0: cuadro nuevo en buf; i.seq, i.larg, i.alt, i.formato; 0: aún no hay otro; < 0: error
camara_a_imagem(buf, n, i.larg, i.alt, i.formato, &imagem);   // YUY2 o MJPEG a 0x00RRGGBB
camara_cerrar();
```

- YUY2 son `larg * alt * 2` bytes en el orden Y0 U Y1 V; MJPEG es el JPEG entero. `buf` debe medir al menos `larg * alt * 2` bytes (más de lo que pesa un MJPEG).
- **Una cámara a la vez y de un solo proceso**: otro proceso recibe `ERRO_OCUPADO` mientras el dueño siga vivo; los servicios (procesos de `de_servico`) reciben `ERRO_ACESSO`.
- `camara_ajuste(CAM_CTL_BRILLO | CAM_CTL_CONTRASTE, escribir, valor, &i)` lee o fija el control (`i.minimo`, `i.maximo`, `i.defecto`, `i.valor`); `ERRO_NAO_SUPORTADO` si la cámara no lo tiene.
- La fuente de prueba da 320x240 y 640x480 en YUY2 a 15 cuadros por segundo: barras de color (blanco, amarillo, cian, verde, magenta, rojo, azul, negro), un cuadrado blanco
  que se mueve y, abajo, el número de cuadro en 16 bloques. Cada quinto cuadro pierde su último paquete y se descarta (se ven en `descartados`).

## Cómo está hecho

- `usb_iso_*` con entrada (`kernel/usb/isoent.h`): las casillas, la cola de recibidos y `usb_iso_ler`. Ver `docs/usb-audio.md` para la API.
- `kernel/usb/uvc_nucleo.c`: descriptores, PROBE/COMMIT y montaje de cuadros, sin dependencias del kernel (se prueba en el PC).
- `kernel/usb/uvc.c`: conexión, apertura, `uvc_tique()` (llamado desde el reloj: recoge los paquetes y monta los cuadros) y `camara_info`.
- Anillo de datos: 384 casillas en alta velocidad y 128 en plena velocidad, cada una de `mps * mult` bytes (hasta 1,2 MB contiguos; si no hay memoria, se reduce).

## Qué probar con cámara real (Toshiba L635, L850)

Todo lo que sigue es lo que QEMU no puede ejercitar. Cada punto, con el resultado y la línea del registro (`dmesg | grep -i uvc`, `usb`):

1. **Reconocimiento:** al arrancar, `[UVC] ...: camara UVC` con los formatos y la cantidad de alternativas; `usb` muestra «Camara UVC: ... en reposo». Anotar el controlador
   (EHCI o xHCI) y el VID:PID.
2. **Vista previa:** Inicio > Gráficos > Cámara. Debe verse la imagen y el pie de ventana contar cuadros; `usb` debe mostrar `cuadros` creciendo con pocos `descartados` y `errores`.
   Probar los dos formatos (`<` y `>`): 640x480 YUY2 y MJPEG (las cámaras integradas suelen dar MJPEG a 720p y YUY2 solo a baja resolución; si YUY2 640x480 no llega
   por ancho de banda, es lo esperado en plena velocidad).
3. **Foto** a PNG (y a JPEG con MJPEG); abrir el archivo con el Visor.
4. **Brillo y contraste** (si la cámara los tiene: aparecen los botones); comprobar que cambia la imagen.
5. **EHCI (iTD, la L635/L850 con USB 2.0):** es el camino que más importa y **nunca se ejecutó**: iTD de entrada con `mult` 1, 2 y 3 (alternativas de 1024x2 y 1024x3
   bytes; en `uvc.c` se elige la menor que cabe en la carga útil negociada). Síntomas posibles: imagen rota o cuadros descartados todo el rato (revisar que los iTD
   se activan: los `huecos` del flujo en `usb`; el sentido de entrada, bit 11 de la palabra 1; el multiplicador, bits 1:0 de la palabra 2; las páginas).
6. **xHCI** (si la máquina usa xHCI): lo mismo, con ráfagas (`Max Burst = mult - 1`, TLBPC) y los eventos por TRB.
7. **Retirar/enchufar** (cámaras externas con la cámara abierta): el kernel no se cuelga, la aplicación sigue viva (puede quedarse sin cuadros nuevos), `[UVC] camara retirada`.
8. **Con la cámara abierta, audio USB** (h275 en máquina real, si hay altavoz USB): los dos flujos isócronos a la vez (EHCI: hoy solo un flujo por controlador; si falla, es el límite
   conocido de `ehci.c`).
9. **siTD:** solo con una cámara de plena velocidad detrás de un hub de alta velocidad (no hay cámara integrada así); sin probar.

Lo que **no** hay: cámaras USB externas (D257), vídeo grabado, lector de QR, H.264 de cámara, varias cámaras, balance de blancos y exposición.
