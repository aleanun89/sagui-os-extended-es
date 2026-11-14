# Guion de prueba: fase 5 (Ventanas y programas)

**Archivos:** `entrega/fase5/saguios.vdi` (VirtualBox) y `entrega/fase5/saguios.img` (pendrive).
**Cómo grabar y configurar:** `docs/como-gravar.md` (para cambiar el `.vdi` de la VM, mira "Actualizar la VM en una fase nueva").

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Qué ha cambiado desde la fase 4

SaguiOS ahora arranca directamente en un **escritorio**:

- fondo en degradado verde, amarillo y azul, con el **sagui** en pixel art en la esquina;
- **barra superior** con el nombre del sistema y el reloj;
- **iconos** de Archivos, Terminal, Editor, Calculadora, Banana y Acerca de (se abren con **doble clic**);
- **ventanas** con barra de título: arrastra por la barra, haz clic para traerlas al frente, **X** para cerrar;
- el **Terminal** se abre solo al encender, con el `sagsh` dentro (todos los comandos de la fase 3 siguen ahí).

Todos los programas gráficos son programas normales en el anillo 3: el kernel solo dibuja los marcos y entrega los clics y las teclas.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

1. Cambia el disco de la VM por el `saguios.vdi` nuevo y enciéndela. Deja la ventana de la VM a **1024x768 o más** (o a pantalla completa).
2. **Pantalla esperada** (mira `docs/capturas/H24-terminal-ps-conta.png`): el escritorio con los iconos a la izquierda, el sagui en la esquina y la ventana **Terminal** abierta con el prompt `sagui:/$`.
3. Prueba:

| Qué hacer | Qué debe pasar |
| --- | --- |
| Arrastrar el Terminal por la barra de título | La ventana sigue al ratón, sin dejar rastro. |
| En el Terminal: `ps` y `ls bin \| conta` | Los procesos (incluidos `janelas (kernel)` y `terminal`) y el recuento de los programas. |
| Doble clic en el icono **Archivos** | Abre el gestor de archivos con las carpetas y los archivos con iconos. |
| En Archivos: doble clic en `historia` y luego en `linux.txt` | Entra en la carpeta y abre el texto en el **Editor**. |
| En el Editor: escribir algo | El título gana un `*`. |
| En el Editor: **Ctrl+S** | Pide la ruta (sugiere `/tmp/`): escribe `nota.txt` e Intro. El `*` desaparece. |
| Cerrar el Editor con la **X** después de cambiar el texto | Pregunta "¿Guardar antes?". |
| En Archivos: **Volver**, entrar en `tmp`, **Nueva carpeta**, seleccionarla y **Borrar** | Crea y borra la carpeta, con "¿Seguro?". |
| Doble clic en **Calculadora** | `12,5 × 4 =` da `50`; `7 ÷ 2 =` da `3,5`; `1 ÷ 0 =` muestra el aviso. Funciona con clic y con el teclado. |
| Doble clic en **Banana** | Pantalla de inicio de **Sagui Atrapa Bananas**. Intro empieza; las **flechas** mueven el sagui; atrapa las bananas; 3 vidas; récord de la sesión. |
| Doble clic en **Acerca de** | Versión, tamaño de la imagen, procesos activos y créditos. |
| En el Terminal: `editor`, `calculadora`, `banana`, `arquivos` | Los mismos programas se abren desde el shell. |
| En el Terminal: `sair` | El Terminal se cierra. Para volver a abrirlo, doble clic en el icono **Terminal**. |

---

## Etapa 2: Lenovo G470 (`saguios.img` en el pendrive)

1. Graba `saguios.img` en el pendrive y arranca (F12, **USB HDD**).
2. **Qué deberías ver en el G470:**
   1. El **escritorio** con el degradado, el sagui, los iconos, el reloj en la barra y el Terminal abierto.
   2. El **touchpad** arrastrando ventanas y el **doble clic** abriendo los programas desde los iconos.
   3. **Sagui Atrapa Bananas** jugable con las flechas.
3. Atención especial en el G470:
   - **Doble clic en el touchpad:** si cuesta acertar (dos toques en menos de medio segundo), dímelo: ajusto el tiempo.
   - **Velocidad:** arrastrar ventanas grandes redibuja bastante pantalla. Dime si va lento o "a tirones" al arrastrar.
   - **Botones del touchpad:** clic izquierdo para todo; el derecho todavía no tiene función (queda para el pendrive, en la fase 6).
   - **Reloj en la barra:** debe coincidir con la hora de la BIOS.
4. **Fotografía** el escritorio con tres programas abiertos (por ejemplo Archivos, Calculadora y Banana) y una pantalla del juego.

**Si sale mal**, fotografía la pantalla y dime:

- **Pantalla roja** "O SaguiOS encontrou um erro": la foto muestra el error y los registros.
- **El escritorio aparece, pero el ratón o el teclado no responden:** dime si el puntero se mueve y si la luz de Bloq Mayús se enciende.
- **Una ventana no se abre** desde el icono: prueba el mismo programa desde el Terminal (`calculadora`, por ejemplo) y dime qué aparece.

Cuando lo pruebes, dime **"fase 5 ok"** o describe lo que ha pasado.
