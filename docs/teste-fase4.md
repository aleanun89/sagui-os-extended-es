# Guion de prueba: fase 4 (Gráficos y ratón)

**Archivos:** `entrega/fase4/saguios.vdi` (VirtualBox) y `entrega/fase4/saguios.img` (pendrive).
**Cómo grabar y configurar:** `docs/como-gravar.md` (para cambiar el `.vdi` de la VM, mira "Actualizar la VM en una fase nueva").

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Corrección después de la primera prueba en el G470 (H19-fix)

En la primera prueba de esta fase, el touchpad "no respondió" y **el teclado dejó de funcionar**. La culpa fue mía: la inicialización del touchpad tocaba el mismo chip que el teclado y podía dejar el teclado desactivado o con la cola bloqueada (detalles en el error 9 de `docs/erros.md`). Ahora:

- el teclado **ya no depende** del touchpad: aunque el touchpad no responda, el teclado tiene que funcionar;
- si el touchpad no responde, aparece un aviso con **números de diagnóstico**, por ejemplo `(cfg 47->45, F6=fff FF=fff F4=fff)`. **Fotografía esa línea**: me dice qué han respondido el chip y el touchpad.

## Qué ha cambiado desde la fase 3

- La pantalla ahora es **gráfica (1024x768)**, dibujada píxel a píxel por SaguiOS, con una **fuente propia** que tiene todos los acentos (`ã`, `õ`, `Ç`, `É`... aparecen bien).
- El **touchpad/ratón** mueve un **puntero** dibujado por SaguiOS.
- **Peticiones tuyas de la fase 3:**
  - las flechas **← →**, **Inicio**, **Fin** y **Supr** editan en mitad de la línea del shell;
  - el texto de un programa en segundo plano (`contador 10 &`) ya no se mezcla con lo que estás escribiendo: el shell vuelve a escribir el prompt y tu línea justo debajo.

Esta fase todavía no tiene ventanas ni escritorio: eso es la fase 5. El ratón ya se mueve y los clics se detectan, pero todavía no hay dónde hacer clic.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

1. Cambia el disco de la VM por el `saguios.vdi` nuevo y enciéndela.
   - Si la VM se abre en una ventana pequeña, usa **Ver > Pantalla completa** o agranda la ventana: la pantalla de SaguiOS es de 1024x768.
   - VirtualBox puede "capturar" el ratón al hacer clic en la ventana; la tecla **Ctrl derecha** lo libera.
2. **Pantalla inicial esperada** (mira `docs/capturas/H19-cursor-movido.png`):
   - fondo azul oscuro, franja verde arriba con `Ola! Eu sou o SaguiOS.` y `versão 0.4 (fase 4)`;
   - **ocho** líneas `[ ok ]`, incluidas **Vídeo: 1024x768, 32 bits, fuente propia** y **Ratón/touchpad PS/2**;
   - el prompt `sagui:/$` y un **puntero blanco** en el centro de la pantalla.
3. Prueba:

| Qué hacer | Qué debe pasar |
| --- | --- |
| Mover el ratón | El puntero lo sigue, sin dejar rastro, y se detiene en los bordes. |
| `echo Ação, vovô, pão, ÇÃO, à é í ó ú` | El texto aparece **con todos los acentos correctos** (incluidos `ã`, `õ`, `Ç`, `Ã`). |
| `cat /historia/linux.txt` | El texto se desplaza por la pantalla, con acentos. |
| Escribe `echo abc`, pulsa **←** dos veces, escribe `X`, pulsa **Fin**, escribe `!` e Intro | Sale `aXbc!` (la edición en mitad de la línea ha funcionado). |
| Escribe `cho oi`, pulsa **Inicio**, escribe `e` e Intro | Sale `oi`. |
| `contador 5 &` y empieza a escribir `echo fim` despacio | Cada segundo aparece `[contador N] ...` en una línea propia y el prompt se redibuja con lo que ya habías escrito. |
| `ls`, `ps`, `ajuda` | Como en la fase 3, ahora en pantalla gráfica. |
| `falha` | El proceso se termina con el mensaje, el shell sigue. |

---

## Etapa 2: Lenovo G470 (`saguios.img` en el pendrive)

1. Graba `saguios.img` en el pendrive y arranca (F12, **USB HDD**).
2. **Qué deberías ver en el G470:**
   1. Las líneas `[1/6]` a `[6/6]` en modo texto y, justo después, la **pantalla gráfica** con la franja verde, la fuente nueva y las ocho líneas `[ ok ]`.
   2. El **puntero del ratón** moviéndose con el **touchpad** (y con los botones del touchpad sin bloquear nada).
   3. `echo Ação, vovô, pão, ÇÃO` con los acentos correctos.
3. Atención especial en el G470:
   - **Vídeo:** es la primera vez que el portátil activa el modo gráfico VESA (la tarjeta Intel informó de 1024x768x32, modo 0x118, en la fase 1). Si la pantalla se queda **negra** después de `[6/6]`, o con los **colores cambiados** (por ejemplo, la franja azul en lugar de verde), fotografíala: eso dice si el problema es el modo o el orden de los colores.
   - **Touchpad:** mira si la velocidad del puntero es razonable (si es muy lenta o muy rápida, la ajusto).
   - **Velocidad:** mira si el texto aparece rápido al escribir y con `cat` (el vídeo del G470 puede ser más lento que el de la VM).
4. **Fotografía** la pantalla inicial con el puntero visible y la pantalla después del `echo` con acentos.

**Si sale mal**, fotografía la pantalla y dime:

- **El teclado no funciona:** fotografía la pantalla inicial entera (las líneas `[ ok ]` y cualquier `[aviso]`) y dime si la luz de Bloq Mayús se enciende al pulsarla.

- **Pantalla negra** después de `[6/6]`: el modo gráfico no se ha activado o el kernel se ha colgado antes de dibujar. Dime si la luz de Bloq Mayús se enciende al pulsar la tecla (eso muestra si el sistema sigue vivo).
- **Ha vuelto al modo texto con el aviso** `Modo gráfico indisponível`: la etapa 2 no ha podido activar VESA; el resto del sistema debería funcionar con normalidad.
- **El puntero no se mueve:** mira si aparece `[aviso] Mouse/touchpad PS/2 não respondeu` en la pantalla inicial.

Cuando lo pruebes, dime **"fase 4 ok"** o describe lo que ha pasado.
