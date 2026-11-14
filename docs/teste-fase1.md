# Guion de prueba: fase 1 (Arranque)

**Archivos:** `entrega/fase1/saguios.vdi` (VirtualBox) y `entrega/fase1/saguios.img` (pendrive).
**Cómo grabar y configurar:** `docs/como-gravar.md`.

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

1. Crea la VM como en `docs/como-gravar.md`, sección 2 (tipo **Other**, sin EFI, disco `saguios.vdi`).
2. Enciende la VM.

**Qué debe aparecer** (rápido, menos de 1 segundo cada cosa):

1. La pantalla de la BIOS de VirtualBox y, por un instante, las líneas del cargador:
   ```
   SaguiOS: carregando...
   SaguiOS - estagio 2 do carregador
   [1/6] A20 ok
   [2/6] memoria ok: N regioes, M MB livres
   [3/6] kernel . ok
   [4/6] ramdisk . ok
   [5/6] video: 1024x768x32
   [6/6] entrando no modo protegido
   ```
2. Justo después, la pantalla del kernel (mira `docs/capturas/H03-kernel-ide.png`):
   - Franja verde arriba con **`Ola! Eu sou o SaguiOS.`** en amarillo.
   - **`Memoria detectada: N MB livres`**, con N cercano a la memoria que le hayas dado a la VM.
   - El mapa de memoria, la línea de A20, el disco de arranque, el modo de vídeo VESA disponible y `Ramdisk SGFS: ... 9 entradas`.
   - En verde: **`Fase 1 concluida: o kernel esta no controle da maquina.`**
3. El sistema se queda parado en esa pantalla. Es lo esperado en la fase 1 (el teclado no llega hasta la fase 2).

**Apúntame:** la cantidad de memoria mostrada y la línea `Video VESA disponivel`.

---

## Etapa 2: Lenovo G470 (`saguios.img` en el pendrive)

1. Graba `saguios.img` en el pendrive (Rufus en modo DD, balenaEtcher o `dd`), como en `docs/como-gravar.md`, sección 3.
2. Arranca desde el pendrive: **F2** para comprobar el modo Legacy en la BIOS, luego **F12** y elige **USB HDD**.

**Qué deberías ver en el G470:**

1. Las líneas `[1/6]` a `[6/6]` pasando rápido.
2. La franja verde con **`Ola! Eu sou o SaguiOS.`**
3. **`Memoria detectada: ... MB livres`** con un valor cercano a la RAM del portátil (p. ej.: ~1900 MB si tiene 2 GB, ~3800 MB si tiene 4 GB; parte de la memoria queda reservada para el vídeo integrado y la BIOS).

**Fotografía**, aunque salga bien, la pantalla final del kernel entera: muestra el mapa de memoria real del G470, el método de A20 que ha funcionado y el modo VESA que ofrece la tarjeta Intel. Eso me ayuda a preparar las fases siguientes.

**Si sale mal, fotografía la pantalla** y dime:

- Si la pantalla se ha quedado negra sin ningún mensaje: el pendrive ni siquiera se eligió como arranque, o la BIOS no aceptó el MBR. Comprueba el modo Legacy y si el pendrive aparece en el F12.
- Si se ha detenido en una de las líneas `[n/6]`: la foto muestra la etapa exacta.
- Si ha aparecido un mensaje `Erro`/`ERRO`: la tabla al final de `docs/como-gravar.md` explica cada uno.

Cuando lo pruebes, dime **"fase 1 ok"** o describe lo que ha pasado.
