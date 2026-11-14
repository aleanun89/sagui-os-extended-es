# Guion de prueba: fase 3 (Procesos y programas)

**Archivos:** `entrega/fase3/saguios.vdi` (VirtualBox) y `entrega/fase3/saguios.img` (pendrive).
**Cómo grabar y configurar:** `docs/como-gravar.md` (para cambiar el `.vdi` de la VM, mira "Actualizar la VM en una fase nueva").

Prueba primero en la máquina virtual. Si hay algún error, envía la captura antes de pasar al G470.

---

## Qué ha cambiado desde la fase 2

Se ha eliminado el terminal provisional del kernel. Ahora:

- el kernel activa la **paginación** (cada programa tiene su propio espacio de memoria);
- el primer programa es el **shell `sagsh`**, que se ejecuta en el **anillo 3** (sin privilegios);
- cada comando (`ls`, `cat`, `cp`, `ps`...) es un **programa independiente** en `/bin`;
- varios programas se ejecutan **a la vez** (el reloj cambia de programa cada 20 ms);
- se pueden **crear, escribir y borrar** archivos y carpetas (en memoria: desaparecen al apagar);
- se pueden conectar programas con **pipes** (`ls bin | conta`) y **redirigir** a archivos (`>`, `>>`, `<`).

> **Nota:** ahora el teclado por defecto es ES (español de España); el comando `teclado abnt2` cambia a ABNT2. Las posiciones de teclas que se citan en esta guía son las del teclado ABNT2.

---

## Etapa 1: VirtualBox (`saguios.vdi`)

1. Cambia el disco de la VM por el `saguios.vdi` nuevo y enciéndela.
2. **Pantalla inicial esperada:** la franja verde con `Ola! Eu sou o SaguiOS.` y `versao 0.3 (fase 3)`, seis líneas `[ ok ]` (ahora con **Paginación** y **Procesos**), la línea `sagsh: o shell do SaguiOS (processo 1)` y el prompt `sagui:/$`.
3. **Escribe los comandos siguientes**, en este orden:

| Comando | Qué debe aparecer |
| --- | --- |
| `ajuda` | Dos columnas con todos los comandos y alias (`ls/listar`, `cat/ver`, ...). |
| `ajuda conta` | Explicación del comando `wc` (alias `conta`). |
| `ls` | Las carpetas (`arte/`, `bin/`, `historia/`, `testes/`, `tmp/`) y los archivos con su tamaño. |
| `listar bin` | Los 18 programas, cada uno marcado como `programa`. |
| `ls bin \| conta` | `18 linhas, ...` (la salida de `ls` pasa por el pipe y `wc` la cuenta). La barra vertical se hace con Shift + la tecla `\` junto al Shift izquierdo. |
| `echo ola > tmp/a.txt` | Nada (el texto ha ido al archivo). |
| `cat tmp/a.txt` | `ola` |
| `eco "mais uma" >> tmp/a.txt` y luego `ver tmp/a.txt` | `ola` y `mais uma`. |
| `cp tmp/a.txt tmp/b.txt` y luego `ls tmp` | `a.txt` y `b.txt`. |
| `conta < tmp/a.txt` | `2 linhas, 3 palavras, ...` |
| `mkdir tmp/pasta`, `ls tmp`, `rmdir tmp/pasta` | La carpeta aparece y luego desaparece. |
| `contador 10 &` | `[N] contador em segundo plano`. Durante los 10 segundos siguientes aparecen líneas `[contador N] 1 de 10 segundos`... **mientras sigues escribiendo**. |
| `ps` (justo después) | La lista de procesos: `ocioso`, `sagsh`, `contador` y el propio `ps`, con memoria y tiempo de CPU. |
| `falha` | `[SaguiOS] Processo N (falha) encerrado: Falta de página...` y **el shell sigue** (el prompt vuelve). |
| `falha instrucao` | `... encerrado: Proteção geral...` y el shell sigue. |
| `mem`, `data`, `sobre` | Memoria, fecha y hora, y los créditos. |
| Flecha arriba | Recupera el último comando. |
| `cat /his` y pulsa **Tab** | Se completa a `cat /historia/`. Escribe `li` y Tab otra vez: `linux.txt`. Intro. |
| `sair` | `O shell foi fechado. Abrindo um novo...` y un prompt nuevo. |
| `reiniciar` | La VM se reinicia. |

---

## Etapa 2: Lenovo G470 (`saguios.img` en el pendrive)

1. Graba `saguios.img` en el pendrive y arranca (F12, **USB HDD**).
2. **Qué deberías ver en el G470:**
   1. Las seis líneas `[ ok ]`, incluidas **Paginación** y **Procesos**, y el prompt `sagui:/$` del sagsh.
   2. `contador 10 &` contando solo **mientras** escribes `ps` y otros comandos (es la multitarea).
   3. `falha` terminando solo el programa, sin pantalla roja, y el shell siguiendo.
3. Ejecuta la misma lista de la etapa 1. En el G470, fíjate sobre todo en:
   - **paginación y procesos:** es la primera vez que el portátil funciona con la paginación activada y con programas en el anillo 3. Si aparece una **pantalla roja** nada más arrancar, su foto es esencial;
   - **teclado en el shell:** flechas (historial), Tab, Retroceso, `|` (Shift + `\`), `>` (Shift + `.`), `<` (Shift + `,`), `&` (Shift + `7`) y las **comillas** (Shift + la tecla `'` junto al 1);
   - **acentos:** si puedes, prueba ahora las teclas muertas que faltaron en la fase 2: `echo ação vovô pão à é ü` (recuerda que en modo texto `ã` y `õ` aparecen sin tilde; la fuente propia llega en la fase 4).
4. **Fotografía** la pantalla después de `ps` con el contador en marcha y la pantalla después de `falha`.

**Si sale mal**, fotografía la pantalla y dime:

- **Pantalla roja:** la foto muestra el nombre del error, la dirección y los registros.
- **El shell no aparece** (se detiene después de las líneas `[ ok ]`): dime cuál es la última línea de la pantalla.
- **El contador no cuenta** mientras escribes: dime qué muestra `ps`.

Cuando lo pruebes, dime **"fase 3 ok"** o describe lo que ha pasado.
