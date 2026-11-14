# SaguiOS: resumen final

Un sistema operativo para PC x86 de 32 bits, escrito desde cero por una IA (Claude, de Anthropic) para el proyecto de Eli Rigobeli, siguiendo el `prd.md`. Todo lo que va dentro de la imagen se ha escrito en este proyecto: cargador de arranque, kernel, drivers, biblioteca de los programas (`libsagui`), shell, programas gráficos, fuente y dibujos.

## Qué se ha hecho

| Fase | Entrega | Historias |
| --- | --- | --- |
| 1. Arranque | Cargador propio en dos etapas (MBR + etapa 2), A20, mapa E820, copia por encima de 1 MB en *unreal mode*, suma de verificación, modo protegido, kernel en C | H01 a H04 |
| 2. Kernel y terminal | GDT/IDT/TSS, pantalla de error, PIC, PIT a 100 Hz, CMOS, bitmap de páginas, `kmalloc`, teclado ABNT2 con teclas muertas, SGFS, terminal provisional | H05 a H09 |
| 3. Procesos y programas | Paginación (un espacio por proceso), planificador round-robin expropiativo, anillo 3, `int 0x80` con validación de punteros, ELF, `libsagui`, archivos escribibles en la RAM, pipes, shell `sagsh` con 18 comandos | H10 a H16 |
| 4. Gráficos y ratón | VESA 1024x768x32 con doble búfer, fuente 8x16 propia con acentos, terminal gráfico, touchpad PS/2 | H17 a H19 |
| 5. Ventanas y programas | Escritorio, gestor de ventanas en el kernel, kit gráfico, Archivos, Terminal, Editor, Calculadora, Sagui Atrapa Bananas, Acerca de | H20 a H28 |
| 6. Pendrive de datos | PCI, driver EHCI, enumeración USB con hubs y conexión en caliente, Bulk-Only/SCSI, FAT32 con lectura y escritura, notificación e icono del pendrive | H29 a H34 |
| Bonus | Pitido por el altavoz del PC (cola de notas reproducida por el reloj); **Sagui 3D**, juego en primera persona 2,5D con tres niveles (B1 y B2 cancelados por Eli) | B3, B4 |

## Cifras

| Elemento | Valor |
| --- | --- |
| Imagen (bytes útiles) | **575.488 bytes = 562 KB (39 % de 1,44 MB)**, con Sagui 3D y sus tres niveles incluidos. El archivo `.img` ocupa 1 MB porque se completa con ceros (D004). |
| Líneas de código | boot 903, kernel 9.614, userland 6.317 (total 16.834), más 3.591 de scripts de build y pruebas |
| Programas | 31 en `/bin` (shell, 22 comandos y 8 programas gráficos) y 6 de prueba en `/testes` |
| Pruebas automáticas | 37, una por historia (más una de dos controladores USB), todas en QEMU (IDE y pendrive USB cuando la historia toca el arranque) |
| Commits | ~55, uno por historia, más las entregas y confirmaciones de cada fase |
| Decisiones registradas | 30, en `docs/decisoes.md` (el razonamiento de cada parte central) |
| Errores registrados | 19, en `docs/erros.md` (síntoma, causa, corrección) |
| Capturas de pantalla | más de 70, en `docs/capturas/` |

## Tiempo de cada fase (trabajo del agente)

Desde el inicio de la fase hasta la entrega para prueba, según las horas de los commits del 03/11/2026:

| Fase | Tiempo | Ciclos de corrección después de la prueba en el G470 |
| --- | --- | --- |
| 1. Arranque | ~15 min | 1: el mapa de memoria del G470 (8 GB, 21 regiones) empujaba el saludo fuera de la pantalla (H04-fix) |
| 2. Kernel y terminal | ~30 min | 0 |
| 3. Procesos y programas | ~40 min | 0 (dos peticiones de Eli, las flechas laterales y el texto en segundo plano, entraron en la fase 4) |
| 4. Gráficos y ratón | ~20 min + ~45 min de la corrección | 1: la inicialización del touchpad bloqueaba el teclado (H19-fix) |
| 5. Ventanas y programas | ~47 min | 0 (un detalle visual en el juego, corregido al principio de la fase 6) |
| 6. Pendrive de datos | ~40 min | 2: el EHCI del G470 es de 64 bits y pide estructuras más grandes; la FAT32 situada después de la partición de SaguiOS no se veía; los dos controladores del G470 compartían el mismo anillo de transferencias (errores 17 a 19) |

## Qué funcionó en el G470

Confirmado por Eli, con fotos en `testes_eli/`:

- **Fase 1:** arranque desde el pendrive con el cargador propio; A20 ya activada por la BIOS; VESA 1024x768x32 (modo 0x118) disponible.
- **Fase 2:** saludo, interrupciones, memoria (8135 MB en total, 3017 MB por debajo de 4 GB, 892 MB gestionados), teclado ABNT2 (`/`, `?`, `ç`), terminal.
- **Fase 3:** shell en el anillo 3, multitarea (`contador &` con `ps`), historial, acentos con teclas muertas.
- **Fase 4:** modo gráfico, fuente propia con acentos, touchpad, después de la corrección H19-fix.
- **Fase 5:** escritorio con cinco programas abiertos a la vez.
- **Fase 6:** en las dos primeras pruebas el pendrive no aparecía (errores 17 a 19, corregidos). Aprobada en la tercera prueba. Puntos que solo pone a prueba el hardware real: el EHCI de 64 bits, el hub interno del chipset (D024) y el traspaso del controlador USB de la BIOS al sistema.

## Qué se ha quedado fuera

- **Bonus B1 y B2** (disco duro interno de solo lectura, arranque UEFI): cancelados por Eli. B1: SaguiOS nunca escribe en el disco duro, así que listar las particiones no aporta nada. B2: el objetivo es funcionar en ordenadores antiguos, con BIOS tradicional.
- **Hub USB probado en QEMU:** imposible en QEMU 8.2, que no tiene hub de alta velocidad y se niega a conectar el hub USB 1.1 al EHCI. El código sigue la especificación, pero solo se valida en el G470 (D024).
- **Dispositivos USB de velocidad completa o baja** (que necesitarían *split transactions* detrás del hub): no soportados. Los pendrives son de alta velocidad.
- **Teclado y ratón USB:** fuera del alcance del PRD (el G470 usa PS/2 internamente).
- **Fuera del alcance del PRD** (sección 1.1): hilos, varios procesadores, memoria virtual con intercambio a disco, usuarios y permisos.
- **Limitaciones conocidas:** el kernel usa solo los primeros 896 MB de RAM (D008); un solo pendrive montado a la vez; los nombres de archivo de más de 63 letras en el pendrive aparecen cortados; los textos en UTF-8 que vienen de fuera se convierten a Latin-1 al mostrarlos (D027).

## La tabla de Tanenbaum (PRD 1.1), rellenada

| Concepto del libro | Qué implementa SaguiOS | Historias | Estado | Dónde verlo |
| --- | --- | --- | --- | --- |
| Modo kernel y modo usuario | Kernel en el anillo 0; todos los programas en el anillo 3, con IOPL 0 | H03, H12 | Hecho | `falha instrucao` tumba solo el programa |
| Procesos | PID, padre e hijo, estados, `executar`, `esperar`, `sair`, `matar`, zombis recogidos | H11, H13 | Hecho | `ps` |
| Planificación | Round-robin expropiativo, cuanto de 20 ms en el tic del PIT | H11 | Hecho | prueba A/B alternándose; `contador &` |
| Comunicación entre procesos | Pipes con bloqueo, fin de archivo, `duplicar` | H15 | Hecho | `ls bin \| conta`; Terminal en ventana |
| Espacios de direcciones | Paginación de 2 niveles, un directorio por proceso, kernel compartido | H10 | Hecho | prueba de dos espacios |
| Gestión de memoria | Bitmap de páginas, `kmalloc`, `aumentar_memoria` + `aloca` en los programas | H06, H10, H13 | Hecho | `mem` |
| Archivos y directorios | Árbol en la RAM con copia en escritura sobre el SGFS; FAT32 en el pendrive con lectura y escritura | H08, H14, H32, H33 | Hecho | Archivos; `fsck.vfat` limpio |
| Entrada y salida | Drivers propios por interrupción y por sondeo: teclado, touchpad, vídeo, reloj, PCI, USB EHCI, almacenamiento | H05, H07, H17, H19, H29 a H31 | Hecho | `usb`, `discos` |
| Protección | Páginas del kernel sin el bit de usuario; punteros validados en cada llamada | H12, H16 | Hecho | `falha kernel` |
| Llamadas al sistema | ~45 llamadas: procesos, archivos, carpetas, comunicación, varias, ventanas, dispositivos | H12 | Hecho | `kernel/inc/abi.h` |
| El shell | `sagsh` en el anillo 3: redirección, pipes, segundo plano, historial, Tab, edición con flechas | H16 | Hecho | Terminal |
| Carga de programas | Cargador ELF32 propio | H13 | Hecho | todos los programas en `/bin` |
| Interfaz gráfica (extra) | Gestor de ventanas en el kernel, programas gráficos en el anillo 3 con memoria compartida | H20 a H28 | Hecho | el escritorio |

## Fase bonus

| Elemento | Qué es | Dónde verlo |
| --- | --- | --- |
| B3 | Comando `bip`, sonidos en Sagui Atrapa Bananas y en Sagui 3D, sin bloquear a quien los reproduce | D029, `tests/b3_bip.py` |
| B4 | Sagui 3D: motor 2,5D propio (salas convexas con alturas, rendijas por columna, coma fija), tres niveles en texto, monos capuchinos, tirachinas de jabuticaba, bananas, cáscaras, verja y racimo dorado | sección "Sagui 3D" en `docs/decisoes.md`, `tests/b4_sagui3d.py` |
