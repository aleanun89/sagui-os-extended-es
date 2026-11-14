# Guion de prueba: fase 2 (Kernel y terminal)

**Archivos:** `entrega/fase2/saguios.vdi` (VirtualBox) y `entrega/fase2/saguios.img` (pendrive).
**Cómo grabar y configurar:** `docs/como-gravar.md` (para cambiar el `.vdi` de la VM, mira "Actualizar la VM en una fase nueva").

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Qué ha cambiado desde la fase 1

- **Corrección (H04-fix):** el saludo ya no se sale por arriba. El mapa de memoria completo ha pasado al log; la pantalla muestra una línea de resumen.
- El kernel ahora gestiona interrupciones, cuenta el tiempo, controla la memoria, lee el teclado ABNT2 y los archivos del ramdisk, y abre un **terminal provisional** donde puedes escribir comandos.

> **Nota:** ahora el teclado por defecto es ES (español de España); el comando `teclado abnt2` cambia a ABNT2. Las pruebas de teclado de esta guía están pensadas para ABNT2.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

1. Cambia el disco de la VM por el `saguios.vdi` nuevo y enciéndela.
2. **Pantalla inicial esperada** (mira `docs/capturas/H09-tela-inicial-ide.png`):
   - Franja verde: `Ola! Eu sou o SaguiOS.` y, a la derecha, `versao 0.2 (fase 2)`.
   - `Memoria detectada: ... MB livres | usável (abaixo de 4 GB): ... MB`.
   - Cuatro líneas con `[ ok ]`: Interrupciones, Memoria física, Teclado, Archivos.
   - `Terminal provisório do kernel...` y el prompt `sagui:/$`.
3. **Escribe los comandos siguientes**, uno a uno, y pulsa Intro:

| Comando | Qué debe aparecer |
| --- | --- |
| `ajuda` | La lista de comandos con una línea de explicación cada uno. |
| `ls` | `arte`, `bin`, `historia`, `tmp` (como `<pasta>`, en azul), `leia-me.txt` y `sobre.txt` con su tamaño. |
| `cat leia-me.txt` | El texto de bienvenida. Como es más grande que la pantalla, se detiene en `-- mais --`: pulsa Espacio para continuar o `q` para salir. |
| `cd historia` | El prompt cambia a `sagui:/historia$`. |
| `ls` | `linux.txt` y `saguios.txt`. |
| `cat linux.txt` | La historia de Minix, Linux y GNU, con paginación. |
| `cd ../arte` | El prompt cambia a `sagui:/arte$`. |
| `cat sagui.txt` | La mascota en arte ASCII. |
| `cd /` | Vuelve a `sagui:/$`. |
| `mem` | Memoria total, gestionada, usada y libre. |
| `data` | Día de la semana, fecha y hora (VirtualBox usa la hora de tu PC). |
| `tempo` | Cuánto tiempo lleva encendido el sistema. Ejecútalo dos veces: el número aumenta. |
| `limpar` | La pantalla se queda vacía, solo con el prompt. |
| `reiniciar` | La VM se reinicia y SaguiOS vuelve a cargar. |

4. **Prueba el teclado ABNT2:** escribe (no hace falta pulsar Intro, o púlsalo y verás el "Comando desconhecido"):
   `ação vovô pão ÇÃO à é í ó ú ü / ? ; : [ ] \ |`
   - Los acentos usan las teclas de acento: `´` y luego la letra da `á`; `~` y luego `a` da `ã`; Shift + `~` (que es el `^`) y luego `o` da `ô`; Shift + `´` (que es el acento grave) y luego `a` da `à`; Shift + `6` (que es la diéresis) y luego `u` da `ü`.
   - **Limitación conocida y aceptada por el PRD:** en modo texto la fuente es la de la tarjeta de vídeo, que no tiene `ã`, `õ` ni las mayúsculas acentuadas (excepto `Ç` y `É`). Aparecen **sin el acento** en pantalla, pero el carácter correcto llega al sistema. La fuente propia (fase 4) lo resuelve.

---

## Etapa 2: Lenovo G470 (`saguios.img` en el pendrive)

1. Graba `saguios.img` en el pendrive (Rufus en modo DD, balenaEtcher o `dd`) y arranca (F12, **USB HDD**).
2. **Qué deberías ver en el G470:**
   1. La franja verde con `Ola! Eu sou o SaguiOS.` **arriba del todo** (la corrección de la fase 1).
   2. `Memoria detectada: ~8100 MB livres | usável (abaixo de 4 GB): ~3000 MB`, y la línea `Memória física: ~890 MB gerenciados...` (el kernel solo usa los primeros 896 MB a propósito; está explicado en `docs/decisoes.md`, D008).
   3. El prompt `sagui:/$` aceptando el **teclado del portátil**, con `ç` y acentos.
3. **Ejecuta la misma lista de comandos de la etapa 1.** Lo más importante en el G470:
   - **Teclado:** escribe `ação vovô ÇÃO / ? ; : [ ] \ |` y comprueba cada tecla, sobre todo `/` y `?` (tecla junto al Shift derecho), `ç`, `~`/`^` y `´`/`` ` ``. Si alguna tecla sale mal, dime **qué tecla has pulsado y qué ha aparecido** (una foto ayuda).
   - **Bloq Mayús:** la luz de Bloq Mayús debe encenderse y apagarse.
   - **`data`:** debe mostrar la fecha y la hora del reloj del portátil. La hora es la que tenga la BIOS (normalmente la hora local, si ya se ha usado Windows en él).
   - **`tempo`:** ejecútalo, espera unos 10 segundos contando con el reloj y ejecútalo otra vez. La diferencia debe ser de ~10 segundos.
   - **`reiniciar`:** el portátil debe reiniciarse.
4. **Fotografía** la pantalla inicial (justo después de encender) y la pantalla después de `mem` y `data`.

**Si sale mal**, fotografía la pantalla y dime:

- Una **pantalla roja** "O SaguiOS encontrou um erro e precisou parar": su foto muestra el nombre del error y los registros; es lo que necesito para encontrar la causa.
- El sistema **no responde al teclado**: dime si la luz de Bloq Mayús se enciende al pulsarla.
- El **reloj parado** (`tempo` siempre a 0): el temporizador no está generando interrupciones en el G470.

Cuando lo pruebes, dime **"fase 2 ok"** o describe lo que ha pasado.
