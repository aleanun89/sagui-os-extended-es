# Diario de SaguiOS

Registro cronológico de lo que se ha hecho. Una entrada por bloque de trabajo.

## 2026-11-03

- **H01.** Lectura del PRD. Instalados NASM 2.16, ImageMagick 6.9 y `gcc-multilib`;
  QEMU 8.2.2, Python 3 y mtools ya estaban en el VPS. Validado el GCC del sistema con
  `-m32 -ffreestanding` (genera ELF32 i386). Creada la estructura de carpetas de la sección 5,
  el repositorio git, el `status.md`, el panel y la herramienta `tools/status.py`.
- **H02.** MBR de 512 bytes: salto + BPB falso reservado, lectura por LBA con
  3 intentos, cabecera de arranque en el LBA 1 como única posición fija, tabla con
  una partición activa. Probado aislado (una etapa 2 de mentira que solo se detiene) en IDE
  y en USB/EHCI: el mensaje `SaguiOS: carregando...` aparece en los dos.
- **H03.** Etapa 2 con las 6 etapas en pantalla, unreal mode para copiar por encima de
  1 MB, suma de verificación del kernel y del ramdisk, búsqueda del modo VESA. Kernel
  en C: BSS, pila, copia de la boot_info, saludo y memoria. La memoria mostrada
  sigue a la RAM de la VM (511 MB con 512, 127 MB con 128), prueba de que viene del E820.
- **H04.** `.vdi` mediante `qemu-img`; la prueba arranca desde el propio `.vdi` y comprueba
  que es idéntico al `.img`. Imagen mínima de 1 MB (D004). `como-gravar.md`
  y `teste-fase1.md` escritos. Ningún error de ejecución en esta fase: todas las
  pruebas pasaron en la primera ronda completa (ver `docs/erros.md`).
- **Fin de la fase 1.** Todas las pruebas (H01 a H04) pasan. Imágenes en
  `entrega/fase1/`. Parada obligatoria: esperando la prueba de Eli.

## 2026-11-03 15:22

- **Confirmación de Eli en el hardware:** "fase 1 ok". Fase 1 validada en VirtualBox y en el G470.
- Foto de Eli en el G470: `testes_eli/fase_1_no_notebook.jpg` (copia en
  `docs/capturas/G470-fase1-tela-final.png`). Arranque desde el pendrive (disco 0x80),
  22 sectores leídos, A20 ya activada por la BIOS, VESA 1024x768x32 (modo 0x118),
  ramdisk íntegro. Mapa E820 con ~20 regiones y ~8 GB de RAM.
- **Problema visto en la foto:** el mapa E820 del G470 es grande y desplazó la pantalla, y el
  saludo y la línea de memoria se salieron por arriba. Corrección acordada con Eli:
  hacerla al principio de la fase 2 (H04-fix).
- **H04-fix.** Mapa E820 completo solo en el puerto serie; la pantalla muestra el resumen y,
  aparte, la memoria que puede usar un kernel de 32 bits. Probado con
  6 GB en QEMU (6143 MB en total, 3071 MB por debajo de 4 GB).
- **Fase 2 iniciada.** Mecanismo de autopruebas mediante la cabecera de arranque (D006).
- **H05.** GDT y TSS propias, 256 vectores generados con `%rep`, despachador que
  devuelve el marco que hay que reanudar (D007), pantalla de error en portugués, PIC
  remapeado, PIT a 100 Hz comprobado contra el reloj CMOS. El kernel pasó a
  compilarse con cadenas en Latin-1, traducidas a la fuente de la tarjeta VGA.
- **H06.** Bitmap de páginas hasta 896 MB y `kmalloc` por clases de tamaño
  (D008). Fallo encontrado en la revisión antes de la primera prueba: el enlace de la lista libre
  sobrescribía la firma del bloque; corregido usando el campo de la cabecera.
  La autoprueba con 450 bloques pasó a la primera con 128 MB y 1 GB.
- **H07.** Teclado ABNT2 completo (D009). La prueba teclea 25 grupos de teclas y
  compara byte a byte. Descubrimiento: en QEMU el AltGr es `alt_r`, no `altgr`.
  Una captura reveló que la franja del saludo pasaba de 80 columnas (error 2).
- **H08.** Lector del SGFS y resolución de rutas. Una captura reveló que
  `formatar()` no aceptaba anchura `*` (error 3).
- **H09.** Terminal provisional con paginación. El harness tecleaba `/` con la
  tecla estadounidense (error 4, de la prueba, no del kernel).
- **Fin de la fase 2.** 9/9 pruebas pasan (H01 a H09). Imágenes en
  `entrega/fase2/`. Parada obligatoria: esperando la prueba de Eli.

## 2026-11-03 16:05

- **Confirmación de Eli en el hardware:** "fase 2 ok". Fase 2 validada en VirtualBox y en el G470.
- Foto de Eli en el G470: `testes_eli/fase_2_no_notebook.jpg` (copia en
  `docs/capturas/G470-fase2-tela-inicial.png`). Saludo arriba del todo (corrección
  confirmada), 8135 MB en total, 3017 MB por debajo de 4 GB, 892 MB gestionados,
  21 regiones en el E820. Todos los comandos funcionaron, sin pantalla roja; `/`,
  `?` y `ç` comprobados. Los acentos con teclas muertas aún no se han probado
  por separado en el portátil (Eli avisará si hay algún error).
- **Fase 3 iniciada** (tag `pre-fase3`). Plan de arquitectura en D010: mapa de
  direcciones, llamadas reiniciables, herencia solo de 0-2.
- **H10.** Paginación de 2 niveles, tablas del kernel compartidas, ventanas
  para dispositivos, pantalla de fallo de página explicada.
- **H11.** Planificador round-robin: `AABBAABB...` con dos tareas que nunca
  ceden el turno. Truco usado: el marco inicial de una tarea del kernel guarda
  la dirección de retorno y el argumento en los campos que el `iret` del anillo 0 no lee.
- **H12.** Anillo 3, TSS e `int 0x80` con validación de punteros; `cli` y
  la lectura del kernel en el anillo 3 terminan solo el proceso.
- **H13.** Cargador ELF, libsagui, build de la userland; `/bin/ola um dois três`.
- **H14.** Árbol de archivos en la RAM con copia en escritura, descriptores,
  terminal como archivo (modos línea y bruto). Regresión encontrada en la H12 y
  corregida (error 5); el commit se había colado porque el script encadenaba el
  commit después de un `grep` (pasé a comprobar PASSOU explícitamente).
- **H15.** Pipes: 20.000 bytes en un búfer de 512, en los dos sentidos.
- **H16.** sagsh con redirección, pipes, `&`, historial y Tab; 18
  programas con los alias; terminal provisional eliminado; prueba de la H09
  adaptada al shell. Hallazgo: GCC no divide en `1 / x` (error 6).
- **Fin de la fase 3.** 16/16 pruebas pasan. Imagen con 223 KB útiles. Imágenes
  en `entrega/fase3/`. Parada obligatoria: esperando la prueba de Eli.

## 2026-11-03 17:21

- **Confirmación de Eli en el hardware:** "fase 3 ok". Fase 3 validada en VirtualBox y en el G470.
- Foto de Eli en el G470: `testes_eli/fase_3_no_notebook.jpg` (copia en
  `docs/capturas/G470-fase3-contador-ps.png`): `contador 10 &` contando
  mientras se ejecuta `ps`, cuatro procesos, shell en el anillo 3. Flechas arriba y
  abajo (historial) y acentos con teclas muertas comprobados. Peticiones de
  Eli: las flechas laterales no editan la línea (no estaba implementado:
  va en la H18, con el terminal gráfico) y el texto de un programa en segundo
  plano se mezcla con la línea que se está escribiendo (también en la H18).
- **Fase 4 iniciada** (tag `pre-fase4`). Peticiones de Eli registradas.
- **H17.** Consola separada en una capa (`console_*`) con dos motores. Vídeo
  VESA con doble búfer de 32 bits, copia solo de lo que ha cambiado, primitivas.
- **H18.** Fuente propia dibujada en texto (`tools/fonte/sagui8x16.txt`),
  letras acentuadas compuestas por el generador; terminal gráfico 128x48; arranque
  reordenado para que la pantalla pase a modo gráfico justo después de activar la memoria. El harness
  pasó a leer la pantalla gráfica reconociendo las celdas por la propia fuente
  (sin OCR), y todas las pruebas antiguas siguieron siendo válidas. Peticiones de Eli:
  edición con flechas en el shell y redibujado de la línea cuando otro proceso
  escribe.
- **H19.** Ratón PS/2 (IRQ 12) con cursor propio. Corregido un problema
  latente: el manejador del teclado descartaba bytes del ratón.
- **Fin de la fase 4.** 19/19 pruebas pasan. Imágenes en `entrega/fase4/`.
  Parada obligatoria: esperando la prueba de Eli.
- **Prueba de Eli en el G470 (fase 4):** el touchpad no respondió y el teclado
  dejó de funcionar. **H19-fix:** módulo `i8042.c` único, cola vaciada también por el
  reloj, configuración comprobada, puerto del ratón desactivado si no
  responde, diagnóstico en pantalla (error 9, D020). Una prueba nueva con la IRQ 12
  bloqueada reproduce el defecto en QEMU con el código antiguo y pasa con el
  nuevo. En la regresión aparecieron dos condiciones de carrera en pruebas (error 10),
  corregidas. 19/19 pasan. Nueva imagen en `entrega/fase4/`.

## 2026-11-03 18:37

- **Confirmación de Eli en el hardware:** "fase 4 ok". Fase 4 validada en VirtualBox y en el G470.
- Foto de Eli en el G470 después de la corrección: `testes_eli/fase_4_no_notebook.jpg`
  (copia en `docs/capturas/G470-fase4-grafico-touchpad.png`). Modo gráfico
  1024x768x32 activado, fuente propia con acentos (`Ação, vovô, pão, ÇÃO`),
  touchpad respondiendo (línea `[ ok ] Mouse/touchpad PS/2`) y puntero en pantalla,
  teclado funcionando. La corrección H19-fix lo resolvió.
- **Fase 5 iniciada** (tag `pre-fase5`). Plan en D021.
- **H20.** Escritorio: degradado verde-amarillo-azul, mascota e iconos en
  pixel art en el código, barra con reloj. Opción de build `--console` para las
  pruebas de las fases anteriores. Hallazgo: una tarea del kernel siempre lista dejaba
  al proceso 0 sin CPU; creada la interrupción 0x81 para ceder el turno (error 11).
- **H21.** Gestor de ventanas en el kernel con píxeles compartidos con el
  programa (páginas "prestadas"), eventos por ventana, arrastrar, foco,
  cerrar. Una captura reveló rastros al arrastrar (error 12), corregido.
- **H22.** Kit gráfico en la libsagui (botón, cuadro, área de texto, lista,
  diálogos) y programa de demostración.
- **H24.** Terminal en ventana con el sagsh por pipes; llamada `disponivel`;
  secuencia ESC [ J; comandos que abren programas gráficos.
- **H25.** Editor con Ctrl+S, título con `*` y aviso al cerrar; llamada
  `registrar` para que los programas gráficos no ensucien el terminal.
- **H23.** Archivos con iconos; abre en el Editor; crea y borra carpetas. La prueba
  también vuelve a abrir desde Archivos la nota creada en el Editor (criterio de la H25).
- **H26.** Calculadora en coma fija con división larga propia (D023).
- **H27.** Sagui Atrapa Bananas; la prueba juega de verdad, guiada por el log.
- **H28.** Iconos que abren los programas y pantalla Acerca de; imagen con 428.544 bytes
  útiles (29 % de la meta).
- **Fin de la fase 5.** 28/28 pruebas pasan. Imágenes en `entrega/fase5/`.
  Parada obligatoria: esperando la prueba de Eli.

## 2026-11-03 20:00

- **Confirmación de Eli en el hardware:** "fase 5 ok". Fase 5 validada en VirtualBox y en el G470.
- Foto de Eli en el G470: `testes_eli/fase_5_no_notebook.jpg` (copia en
  `docs/capturas/G470-fase5-janelas.png`). Escritorio con Calculadora,
  Terminal, Editor, Sagui Atrapa Bananas y Acerca de abiertos a la vez;
  Acerca de muestra 419,0 KB de imagen, 8 procesos y 879 MB libres de 892 MB.
  Detalle visto en la foto: en la pantalla de inicio del juego los corazones de las vidas
  no aparecen (corregido al principio de la fase 6).
- **Fase 6 iniciada** (tag `pre-fase6`). Investigación: el hub de QEMU 8.2 es
  USB 1.1 y no se conecta al EHCI; el camino del hub queda para el G470 (D024).
  Corregidos los corazones de las vidas en la pantalla de inicio del juego (error 13).
- **H29-H31.** PCI, EHCI tomado de la BIOS, transferencias síncronas medidas con el
  FRINDEX, enumeración, hubs, conexión en caliente por sondeo, Bulk-Only/SCSI.
  El primer arranque con el driver ya enumeró el pendrive y leyó la etiqueta del FAT32.
  Sectores comprobados byte a byte en el VPS.
- **H32.** FAT32 con nombres largos y acentos montado en `/usb` mediante la capa de
  archivos virtual. Hallazgo: los textos de fuera vienen en UTF-8; conversión a
  Latin-1 en `cat` y en el Editor (D027, error 14).
- **H33.** Escritura comprobada con `mtools` y `fsck.vfat` (íntegro).
- **H34.** Notificación, icono del pendrive, Archivos con panel lateral y "Extraer
  con seguridad", aviso de extracción sin desmontar (error 15: notificación
  cortada). La regresión mostró que el propio pendrive de arranque generaba
  notificación e icono: ahora se ignora en esos dos sitios.
- A petición de Eli, Acerca de dice "para el proyecto de Eli Rigobeli". Versión 1.0.
- **Fin de la fase 6 (final).** Resumen en `docs/resumo-final.md`. Imágenes en
  `entrega/fase6/` y `entrega/final/`. Esperando la prueba de Eli.

### Corrección de la fase 6 (G470)

- **Informe de Eli:** el pendrive conectado no aparecía; `usb` mostraba el puerto
  ocupado incluso sin el pendrive; `montar` no encontraba FAT32. El pendrive tenía la
  partición de SaguiOS y una FAT32 de ~3 GB creada después.
- **Causa 1 (error 17):** el EHCI del G470 es de 64 bits y exige qTD/QH más grandes;
  el de QEMU es de 32 bits y no muestra el defecto. Corregido (D028), sin forma de
  validarlo en QEMU: la prueba real es en el G470. `usb` ganó un diagnóstico por puerto.
- **Causa 2 (error 18):** solo se examinaba la primera partición. Corregido y
  probado en QEMU con la misma disposición que el pendrive de Eli.
- **Segunda prueba (error 19):** el nuevo diagnóstico de `usb` mostró que la
  corrección de 64 bits funcionó (el segundo controlador enumeró el hub y el
  lector de tarjetas), pero el primero fallaba en el hub. Causa: los dos
  controladores compartían el mismo anillo de transferencias. Reproducido en
  QEMU con dos EHCI (`h29_dois_controladores`) y corregido.

## 2026-11-03 22:57

- **Confirmación de Eli en el hardware:** "fase 6 ok". Fase 6 validada en VirtualBox y en el G470.

### Fase bonus (replanificada por Eli)

- Fase 6 aprobada en el G470 después de la tercera imagen (error 19).
- Eli canceló el B1 (disco duro interno) y el B2 (UEFI), mantuvo el B3 (pitido) y creó
  el B4 (Sagui 3D).
- **B3.** Cola de notas reproducida con el tic del reloj (D029). La primera
  comprobación del canal 2 vio 0 cambios en QEMU porque medía el tiempo por
  número de vueltas; pasó a medirlo con el canal 0.
- **B4.** El primer motor del juego funcionó a la primera para las paredes y el cielo;
  dos ajustes vistos en las capturas: una franja de la derecha sin dibujar (una cuenta
  que desbordaba los 32 bits con una pared pegada al ojo) y el mono que
  resbalaba se deslizaba por detrás del sagui. En la jugabilidad, el mono que
  cogía una banana volvía al instante y quitaba las tres vidas en segundos: ahora
  sale corriendo con la banana.

## 2026-11-04 00:19

- **Confirmación de Eli:** "Funcionou tudo perfeitamente" (ha funcionado todo perfectamente). Fase bonus (B3 pitido y B4 Sagui 3D) aprobada; la foto enviada es de la VM (`testes_eli/fase_bonus_vm.png`).

## 2026-11-06

- **Mejoras pedidas por el mantenedor** (seguimiento en `mejoras/`):
  - **Traducción al español** de la interfaz y la documentación, y **teclado ES**
    (por defecto) con ABNT2 seleccionable con `teclado` (D031).
  - **FPU x87, MMX y SSE** para los programas, con estado por proceso y cambio
    perezoso; programa `simd` y prueba h35 (D032). Corregida de paso la alineación
    de la pila en `inicio.asm`.
  - **B5 (`cc`).** Compilador propio de una pasada (D033): tokens con el
    preprocesador integrado, descenso recursivo que genera bytes i386 al momento,
    lvalues con el truco de borrar la última carga, saltos parcheados mediante listas
    dentro del código, ELF con tres segmentos de dirección fija. La biblioteca
    estándar (printf, malloc, str*...) es C que se compila junto con el programa.
    Para probar rápido, cc tiene un modo `-linux` que genera ELF para Linux: los
    programas de prueba se compararon con la salida de GCC. Tres errores
    registrados (20 a 22).
  - **B6 (`python`).** MicroPython 1.24.1 mediante el "embed port" oficial, sin coma
    flotante (D034). Es la primera excepción a la regla de autoría (código de
    terceros, licencia MIT), aislada y opcional (`make MICROPYTHON=0`). Funcionó a la
    primera en QEMU.

## 2026-11-07

- **Shell completo (mejora 011, D037).** sagsh deja de ser un separador de palabras con
  `| < > &` y pasa a ser un shell tipo sh/bash: analizador recursivo a un árbol, variables
  y entorno, `$( )`, `$(( ))`, comodines, here-docs, `if/while/for/case`, funciones,
  trabajos y scripts con `#!`. Sin fork: lo interno corre en el propio proceso (los
  subshells guardan y restauran el estado) y las etapas internas de un pipe que no pueden
  correr en él se lanzan como un sagsh hijo que recibe funciones y variables en un
  preludio de texto.
  - Kernel: `envp` en el 3er argumento de `SIS_EXECUTAR` (pila `[esp+8]`), grupos de
    procesos para que Ctrl+C termine un pipe entero o un script con lo que ejecuta, un
    contador de Ctrl+C para cortar bucles internos del shell y `SIS_TIPO_DESCRITOR`.
  - Para probar rápido, el shell se compiló también en Linux con una capa que imita la
    libsagui con llamadas POSIX (fuera del repositorio): ~160 casos comparados con la
    salida de bash, más entradas aleatorias con AddressSanitizer. En QEMU, h16 pasó a la
    primera; h38 necesitó dos ajustes del propio test (el eco de la línea tecleada
    contiene el texto buscado, y un prompt redibujado en mitad de la escritura engaña a
    `m.comando`) y un cambio en la tabla (`sh` era el nombre y `sagsh` el alias, y el
    proceso aparecía como "sh").
  - Un `$( )` dentro de una función recursiva desbordó la pila de 64 KB del programa
    (fallo de página en 0xbffefff4). Ahora hay un límite que avisa "demasiada recursión"
    y las estructuras de redirecciones ya no ocupan la pila.
