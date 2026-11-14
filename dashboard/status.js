// Generado por tools/status.py a partir de status.json. No editar.
window.SAGUI_STATUS = {
  "titulo": "SaguiOS: un sistema operativo escrito desde cero por IA",
  "atualizado": "2026-11-04T00:20:45",
  "situacao": "Fase 7 confirmada en el hardware. Pasando a la fase 8.",
  "fase_atual": 7,
  "historia_atual": null,
  "bloqueio": "",
  "mensagem_fase": "",
  "fases": [
    {
      "numero": 1,
      "nome": "Arranque",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T15:22:25"
    },
    {
      "numero": 2,
      "nome": "Kernel y terminal",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T16:05:16"
    },
    {
      "numero": 3,
      "nome": "Procesos y programas",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T17:21:51"
    },
    {
      "numero": 4,
      "nome": "Gráficos y ratón",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T18:37:28"
    },
    {
      "numero": 5,
      "nome": "Ventanas y programas",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T20:00:46"
    },
    {
      "numero": 6,
      "nome": "Pendrive de datos",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-03T22:57:09"
    },
    {
      "numero": 7,
      "nome": "Bonus (B3 pitido, B4 Sagui 3D)",
      "estado": "feito",
      "progresso": 100.0,
      "confirmado": "2026-11-04T00:19:30"
    }
  ],
  "historias": [
    {
      "id": "H01",
      "fase": 1,
      "titulo": "Preparar el entorno",
      "objetivo": "Instalar NASM, QEMU, ImageMagick, Python; validar el GCC freestanding; crear carpetas, git, status.md y dashboard.",
      "aceite": "`make` se ejecuta sin errores con un objetivo vacío; el dashboard se abre en el navegador.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T14:12:34",
      "fim": "2026-11-03T14:14:17",
      "minutos": 2,
      "resultado": "Entorno listo: NASM, GCC -m32, QEMU, ImageMagick; estructura, git y panel creados"
    },
    {
      "id": "H02",
      "fase": 1,
      "titulo": "Etapa 1 (MBR)",
      "objetivo": "Código de 512 bytes con firma 0x55AA, tabla de particiones válida con partición activa y área de BPB reservada.",
      "aceite": "En QEMU (IDE y USB) aparece el mensaje `SaguiOS: carregando...`.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T14:14:29",
      "fim": "2026-11-03T14:22:05",
      "minutos": 8,
      "resultado": "MBR de 512 bytes con BPB reservado y partición activa; el mensaje aparece en IDE y USB"
    },
    {
      "id": "H03",
      "fase": 1,
      "titulo": "Etapa 2 y modo protegido",
      "objetivo": "A20, mapa E820, lectura del kernel y del ramdisk, GDT, modo protegido, salto al kernel con boot_info.",
      "aceite": "El kernel en C escribe en modo texto VGA `Ola! Eu sou o SaguiOS.` y la memoria detectada.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T14:22:05",
      "fim": "2026-11-03T14:22:52",
      "minutos": 1,
      "resultado": "Etapa 2 completa y kernel en C en modo protegido; probado en IDE y USB con 512 y 128 MB"
    },
    {
      "id": "H04",
      "fase": 1,
      "titulo": "Imagen para pendrive y VirtualBox",
      "objetivo": "El build genera build/saguios.img con SHA-256 y build/saguios.vdi; docs/como-gravar.md.",
      "aceite": "Arranque en los dos modos de QEMU; .vdi generado; archivos en entrega/fase1/; como-gravar.md con Rufus (DD) y balenaEtcher.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T14:22:52",
      "fim": "2026-11-03T14:27:10",
      "minutos": 4,
      "resultado": "Imagen .img y .vdi con SHA-256; arranque del .img (IDE y USB) y del .vdi en QEMU; como-gravar.md"
    },
    {
      "id": "H05",
      "fase": 2,
      "titulo": "Interrupciones y reloj",
      "objetivo": "IDT, excepciones de la CPU con nombre y registros, remapeo del PIC, PIT a 100 Hz, contador de tiempo.",
      "aceite": "La división por cero muestra la pantalla de error de SaguiOS; el contador de segundos avanza.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T15:24:57",
      "fim": "2026-11-03T15:28:28",
      "minutos": 4,
      "resultado": "IDT con 256 vectores, pantalla de error, PIC remapeado, PIT a 100 Hz, CMOS"
    },
    {
      "id": "H06",
      "fase": 2,
      "titulo": "Memoria física",
      "objetivo": "Bitmap de páginas de 4 KB a partir del E820 y asignador kmalloc/kfree.",
      "aceite": "La prueba asigna y libera bloques de varios tamaños sin corromper nada; el diagnóstico muestra la memoria total y libre.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T15:28:28",
      "fim": "2026-11-03T15:32:06",
      "minutos": 4,
      "resultado": "Bitmap de páginas a partir del E820 y kmalloc por clases; autoprueba de 450 bloques sin corrupción"
    },
    {
      "id": "H07",
      "fase": 2,
      "titulo": "Teclado PS/2 con ABNT2",
      "objetivo": "Driver en IRQ1, búfer, Shift, Bloq Mayús, Ctrl, teclas muertas y el mapa ABNT2.",
      "aceite": "Las teclas enviadas por el monitor de QEMU aparecen correctas, incluida la ç.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T15:32:06",
      "fim": "2026-11-03T15:37:48",
      "minutos": 6,
      "resultado": "Driver PS/2 con ABNT2 completo, teclas muertas, AltGr, Bloq Mayús; 25 grupos de teclas comprobados byte a byte"
    },
    {
      "id": "H08",
      "fase": 2,
      "titulo": "Sistema de archivos SGFS",
      "objetivo": "tools/mksgfs.py y lector en el kernel: listar carpeta, abrir y leer archivo.",
      "aceite": "Una prueba interna lista las entradas de rootfs/ y lee leia-me.txt entero.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T15:37:48",
      "fim": "2026-11-03T15:39:25",
      "minutos": 2,
      "resultado": "mksgfs.py + lector en el kernel; listado igual a rootfs/ y leia-me.txt idéntico"
    },
    {
      "id": "H09",
      "fase": 2,
      "titulo": "Terminal provisional del kernel",
      "objetivo": "Prompt sagui:/$ con ajuda, ls, cd, cat, limpar, mem, data, reiniciar.",
      "aceite": "Un guion teclea cada comando en QEMU y lo comprueba por la captura y por el puerto serie.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T15:39:25",
      "fim": "2026-11-03T15:41:51",
      "minutos": 2,
      "resultado": "Terminal provisional con ajuda, ls, cd, cat (paginado), limpar, mem, data, tempo, reiniciar"
    },
    {
      "id": "H10",
      "fase": 3,
      "titulo": "Paginación",
      "objetivo": "Directorio y tablas de páginas, kernel en todos los espacios, fallo de página con mensaje claro.",
      "aceite": "Dos espacios de direcciones con valores distintos en la misma dirección virtual; un acceso no válido genera la pantalla de fallo de página.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:07:26",
      "fim": "2026-11-03T16:10:05",
      "minutos": 3,
      "resultado": "Paginación de 2 niveles, espacios por proceso, pantalla de fallo de página"
    },
    {
      "id": "H11",
      "fase": 3,
      "titulo": "Procesos y planificador",
      "objetivo": "PCB, cambio de contexto en ensamblador, round-robin expropiativo en el PIT, proceso ocioso.",
      "aceite": "Dos tareas imprimen A y B alternadas en el puerto serie sin cooperar.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:10:05",
      "fim": "2026-11-03T16:12:46",
      "minutos": 3,
      "resultado": "Planificador round-robin expropiativo; las tareas A y B se alternan cada 20 ms sin cooperar"
    },
    {
      "id": "H12",
      "fase": 3,
      "titulo": "Ring 3 y llamadas al sistema",
      "objetivo": "TSS, segmentos de usuario, ring 3, int 0x80, validación de punteros.",
      "aceite": "Un programa en ring 3 escribe `Ola do ring 3!` y termina; `cli` en ring 3 termina solo ese proceso.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:12:46",
      "fim": "2026-11-03T16:16:35",
      "minutos": 4,
      "resultado": "Programas en el anillo 3, int 0x80 con validación de punteros; cli y el acceso al kernel terminan solo el proceso"
    },
    {
      "id": "H13",
      "fase": 3,
      "titulo": "Cargador ELF y libsagui",
      "objetivo": "Lector de ELF32 estático, pila de usuario con argumentos, libsagui con printf.",
      "aceite": "El kernel carga bin/ola del SGFS y este imprime los argumentos.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:16:35",
      "fim": "2026-11-03T16:21:08",
      "minutos": 5,
      "resultado": "Cargador ELF32 y libsagui; /bin/ola imprime los argumentos"
    },
    {
      "id": "H14",
      "fase": 3,
      "titulo": "Archivos y carpetas escribibles",
      "objetivo": "Capa escribible en la RAM sobre el SGFS, tabla de archivos por proceso, terminal como 0 y 1.",
      "aceite": "Una prueba en ring 3 crea, escribe, relee y borra tmp/teste.txt; lo mismo con una carpeta.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:21:08",
      "fim": "2026-11-03T16:25:17",
      "minutos": 4,
      "resultado": "Árbol en la RAM con copia en escritura, descriptores por proceso, terminal como 0/1; 28 comprobaciones en el anillo 3"
    },
    {
      "id": "H15",
      "fase": 3,
      "titulo": "Pipes",
      "objetivo": "pipe y duplicar, búfer en el kernel, bloqueo de lector y escritor, EOF.",
      "aceite": "Dos procesos transfieren por pipe un texto mayor que el búfer sin pérdidas ni duplicados.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:25:17",
      "fim": "2026-11-03T16:27:11",
      "minutos": 2,
      "resultado": "Pipes con bloqueo y fin de archivo; 20.000 bytes por un pipe de 512, en los dos sentidos"
    },
    {
      "id": "H16",
      "fase": 3,
      "titulo": "Shell y comandos como programas",
      "objetivo": "bin/sagsh como primer proceso, redirección, pipe, historial, Tab, comandos y alias.",
      "aceite": "Un guion ejecuta todos los comandos y alias; un fallo tumba solo el proceso; capturas en docs/capturas/.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T16:27:11",
      "fim": "2026-11-03T16:41:25",
      "minutos": 14,
      "resultado": "sagsh en el anillo 3 con redirección, pipes, historial y Tab; 18 programas con alias; terminal provisional eliminado"
    },
    {
      "id": "H17",
      "fase": 4,
      "titulo": "Framebuffer VESA",
      "objetivo": "Cambio al modo VESA, doble búfer y primitivas de dibujo.",
      "aceite": "Captura con fondo de color y formas; si VESA falla, vuelve al modo texto y avisa.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T17:22:03",
      "fim": "2026-11-03T17:24:26",
      "minutos": 2,
      "resultado": "Framebuffer VESA 1024x768x32 con doble búfer y primitivas de dibujo"
    },
    {
      "id": "H18",
      "fase": 4,
      "titulo": "Fuente propia",
      "objetivo": "Fuente bitmap 8x16 propia con ASCII y acentos Latin-1; terminal gráfico.",
      "aceite": "La captura muestra `Ação, vovô, pão, ÇÃO, à é í ó ú` correctamente.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T17:24:26",
      "fim": "2026-11-03T17:32:50",
      "minutos": 8,
      "resultado": "Fuente 8x16 propia con acentos, terminal gráfico de 128x48, edición con flechas"
    },
    {
      "id": "H19",
      "fase": 4,
      "titulo": "Ratón / touchpad PS/2",
      "objetivo": "Puerto auxiliar del i8042, IRQ12, paquetes de 3 bytes, cursor propio, botones.",
      "aceite": "Los movimientos del monitor de QEMU mueven el cursor; el clic se registra en el puerto serie.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T17:32:50",
      "fim": "2026-11-03T18:27:00",
      "minutos": 54,
      "resultado": "Ratón/touchpad PS/2; H19-fix: módulo i8042 único, teclado independiente del touchpad"
    },
    {
      "id": "H20",
      "fase": 5,
      "titulo": "Escritorio",
      "objetivo": "Degradado con los colores de Brasil, mascota sagui en pixel art, barra superior con reloj.",
      "aceite": "Captura del escritorio en docs/capturas/.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T18:38:00",
      "fim": "2026-11-03T18:46:46",
      "minutos": 9,
      "resultado": "Escritorio con degradado de Brasil, mascota en pixel art, barra con reloj e iconos"
    },
    {
      "id": "H21",
      "fase": 5,
      "titulo": "Gestor de ventanas",
      "objetivo": "Ventanas con marco, arrastrar, foco; llamadas de ventana para programas en ring 3.",
      "aceite": "Dos programas abren ventanas; la prueba arrastra una, cierra la otra y el programa termina.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T18:46:46",
      "fim": "2026-11-03T18:53:31",
      "minutos": 7,
      "resultado": "Gestor de ventanas en el kernel, memoria compartida, eventos por ventana; arrastrar y cerrar probados"
    },
    {
      "id": "H22",
      "fase": 5,
      "titulo": "Kit gráfico de la libsagui",
      "objetivo": "Texto, botón, etiqueta, cuadro de texto, área de texto, lista, confirmación.",
      "aceite": "Demostración con todos los componentes funcionando; captura.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T18:53:31",
      "fim": "2026-11-03T18:56:46",
      "minutos": 3,
      "resultado": "Kit gráfico en la libsagui: botón, cuadro, área de texto con desplazamiento, lista, diálogos"
    },
    {
      "id": "H23",
      "fase": 5,
      "titulo": "Archivos",
      "objetivo": "Gestor de archivos en ring 3 con iconos, doble clic, volver, crear y borrar carpeta.",
      "aceite": "Navega hasta historia/, abre linux.txt en el Editor, crea y borra una carpeta en tmp/.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T19:03:23",
      "fim": "2026-11-03T19:07:26",
      "minutos": 4,
      "resultado": "Gestor de archivos con iconos; abre en el Editor, crea y borra carpetas"
    },
    {
      "id": "H24",
      "fase": 5,
      "titulo": "Terminal en ventana",
      "objetivo": "Ventana que ejecuta el sagsh conectado por pipes; comandos que abren los programas gráficos.",
      "aceite": "Captura del terminal en ventana ejecutando ps y ls bin | conta.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T18:56:46",
      "fim": "2026-11-03T19:00:05",
      "minutos": 3,
      "resultado": "Terminal en ventana ejecutando el sagsh por pipes; comandos que abren los programas gráficos"
    },
    {
      "id": "H25",
      "fase": 5,
      "titulo": "Editor de texto",
      "objetivo": "Nuevo, abrir, guardar, guardar como, acentos, flechas, Ctrl+S, asterisco, aviso al cerrar.",
      "aceite": "Crea tmp/nota.txt con `Olá, SaguiOS! Ação e coração.`, lo vuelve a abrir desde Archivos y lo comprueba.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T19:00:05",
      "fim": "2026-11-03T19:03:23",
      "minutos": 3,
      "resultado": "Editor con Nuevo/Abrir/Guardar, Ctrl+S, título con *, aviso al cerrar"
    },
    {
      "id": "H26",
      "fase": 5,
      "titulo": "Calculadora",
      "objetivo": "Botones y teclado, coma fija, avisos de división por cero y número demasiado grande.",
      "aceite": "12,5 × 4 = 50; 7 ÷ 2 = 3,5; 1 ÷ 0 muestra un aviso; captura.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T19:07:26",
      "fim": "2026-11-03T19:09:30",
      "minutos": 2,
      "resultado": "Calculadora en coma fija con botones y teclado; avisos de división por cero y número demasiado grande"
    },
    {
      "id": "H27",
      "fase": 5,
      "titulo": "Sagui Atrapa Bananas",
      "objetivo": "Juego de autor: el sagui atrapa bananas, 3 vidas, velocidad creciente, récord.",
      "aceite": "La prueba juega con las flechas desde el monitor; capturas del juego y de la pantalla de fin de partida.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T19:09:30",
      "fim": "2026-11-03T19:11:50",
      "minutos": 2,
      "resultado": "Juego Sagui Atrapa Bananas con pixel art, vidas, velocidad creciente y récord"
    },
    {
      "id": "H28",
      "fase": 5,
      "titulo": "Acabado",
      "objetivo": "Iconos en el escritorio y pantalla Acerca de SaguiOS.",
      "aceite": "Captura con tres programas abiertos; tamaño de la imagen medido.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T19:11:50",
      "fim": "2026-11-03T19:14:10",
      "minutos": 2,
      "resultado": "Iconos que abren los programas, pantalla Acerca de; imagen de 428 KB (29 % de la meta)"
    },
    {
      "id": "H29",
      "fase": 6,
      "titulo": "PCI y controlador EHCI",
      "objetivo": "Escaneo PCI, traspaso de control desde la BIOS, reset del EHCI, puertos.",
      "aceite": "El diagnóstico lista los controladores EHCI y los puertos con dispositivo en QEMU.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:16:48",
      "fim": "2026-11-03T20:16:48",
      "minutos": 1,
      "resultado": "PCI, EHCI tomado de la BIOS, transferencias; comando usb"
    },
    {
      "id": "H30",
      "fase": 6,
      "titulo": "Enumeración USB, hubs y conexión en caliente",
      "objetivo": "Transferencias de control, descriptores, direcciones, hubs, conexión y extracción.",
      "aceite": "Con un hub virtual, lista fabricante, producto y clase; eventos de conexión y extracción en el puerto serie.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:16:48",
      "fim": "2026-11-03T20:16:48",
      "minutos": 1,
      "resultado": "Enumeración, hubs (validación en el G470), conexión y extracción en caliente"
    },
    {
      "id": "H31",
      "fase": 6,
      "titulo": "Almacenamiento masivo",
      "objetivo": "Bulk-Only Transport y SCSI: identificar, capacidad, leer y escribir.",
      "aceite": "Lee y escribe sectores en una imagen de prueba y los comprueba byte a byte en el VPS.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:16:48",
      "fim": "2026-11-03T20:16:48",
      "minutos": 1,
      "resultado": "Bulk-Only + SCSI; sectores leídos y escritos comprobados byte a byte en el VPS"
    },
    {
      "id": "H32",
      "fase": 6,
      "titulo": "FAT32: lectura",
      "objetivo": "Particiones MBR, FAT32 con nombres cortos y largos, montaje en /usb.",
      "aceite": "Un pendrive de prueba con carpetas, acentos y nombres largos se lee con ls y cat.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:16:48",
      "fim": "2026-11-03T20:18:39",
      "minutos": 2,
      "resultado": "FAT32 con nombres largos y acentos montado en /usb; ls y cat funcionando"
    },
    {
      "id": "H33",
      "fase": 6,
      "titulo": "FAT32: escritura",
      "objetivo": "Crear, sobrescribir, ampliar, borrar; carpetas; dos FAT; FSInfo; desmontar lo graba todo.",
      "aceite": "Tras desmontar, mtools y fsck.vfat confirman la integridad y el contenido.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:18:39",
      "fim": "2026-11-03T20:20:50",
      "minutos": 2,
      "resultado": "Escritura FAT32 comprobada con mtools y fsck.vfat"
    },
    {
      "id": "H34",
      "fase": 6,
      "titulo": "Integración del pendrive",
      "objetivo": "Notificación, icono Pendrive, montar y extraer con seguridad, flujo por el terminal.",
      "aceite": "Guion completo por la interfaz y por el terminal; el VPS lo comprueba con mtools; capturas.",
      "estado": "feito",
      "bonus": false,
      "inicio": "2026-11-03T20:20:50",
      "fim": "2026-11-03T20:25:28",
      "minutos": 5,
      "resultado": "Notificación, icono del pendrive, montar/extraer por la interfaz y por el terminal; comprobado con mtools y fsck"
    },
    {
      "id": "B1",
      "fase": 7,
      "titulo": "Bonus: ver el disco duro interno",
      "objetivo": "Driver SATA de solo lectura para listar el disco duro y sus particiones.",
      "aceite": "discos muestra el disco duro y sus particiones sin montar ni escribir.",
      "estado": "cancelado",
      "bonus": true,
      "inicio": null,
      "fim": null,
      "minutos": null,
      "resultado": "Cancelado por Eli: SaguiOS nunca escribe en el disco duro, así que listar las particiones no aporta nada al proyecto."
    },
    {
      "id": "B2",
      "fase": 7,
      "titulo": "Bonus: arranque por UEFI",
      "objetivo": "Cargador UEFI con GOP.",
      "aceite": "Arranque en una máquina UEFI.",
      "estado": "cancelado",
      "bonus": true,
      "inicio": null,
      "fim": null,
      "minutos": null,
      "resultado": "Cancelado por Eli: el objetivo de SaguiOS es funcionar en ordenadores antiguos, con BIOS tradicional."
    },
    {
      "id": "B3",
      "fase": 7,
      "titulo": "Bonus: pitido",
      "objetivo": "Altavoz del PC por el canal 2 del PIT.",
      "aceite": "Pitido en el juego y comando bip.",
      "estado": "feito",
      "bonus": true,
      "inicio": "2026-11-03T22:57:35",
      "fim": "2026-11-03T23:02:23",
      "minutos": 5,
      "resultado": "Comando bip, sonidos en Sagui Atrapa Bananas, cola de notas reproducida con el tic del reloj (D029); canal 2 comprobado en QEMU"
    },
    {
      "id": "B4",
      "fase": 7,
      "titulo": "Bonus: Sagui 3D",
      "objetivo": "Juego de autor en primera persona, aspecto 2,5D (salas con alturas propias de suelo y techo, paredes en cualquier ángulo), monos capuchinos guardianes, tirachinas de jabuticaba, bananas, cáscaras, verja de la banana y racimo dorado, sonidos con el pitido.",
      "aceite": "El juego se abre, el mapa se carga, el sagui anda, la semilla alcanza al enemigo, el mono resbala con la cáscara, la salida solo se abre con el mínimo de bananas, el nivel termina en la salida; capturas; tres niveles en archivos de texto.",
      "estado": "feito",
      "bonus": true,
      "inicio": "2026-11-03T23:02:29",
      "fim": "2026-11-03T23:45:56",
      "minutos": 43,
      "resultado": "Sagui 3D: motor 2,5D propio, tres niveles en texto, monos capuchinos, semillas, cáscaras, verja de la banana, racimo dorado, sonidos con el pitido; prueba automática y capturas"
    }
  ],
  "tanenbaum": [
    {
      "conceito": "Modo kernel y modo usuario",
      "implementa": "Kernel en ring 0, programas en ring 3",
      "historias": "H03, H12",
      "estado": "feito"
    },
    {
      "conceito": "Procesos",
      "implementa": "Creación, terminación, estados, PID, padre e hijo",
      "historias": "H11, H13",
      "estado": "feito"
    },
    {
      "conceito": "Planificación",
      "implementa": "Round-robin expropiativo con el reloj",
      "historias": "H11",
      "estado": "feito"
    },
    {
      "conceito": "Comunicación entre procesos",
      "implementa": "Pipes entre programas (ls | conta)",
      "historias": "H15",
      "estado": "feito"
    },
    {
      "conceito": "Espacios de direcciones",
      "implementa": "Paginación con un espacio por proceso",
      "historias": "H10",
      "estado": "feito"
    },
    {
      "conceito": "Gestión de memoria",
      "implementa": "Memoria física, asignador del kernel, memoria de los procesos",
      "historias": "H06, H10, H13",
      "estado": "feito"
    },
    {
      "conceito": "Archivos y directorios",
      "implementa": "Abrir, leer, escribir, crear, borrar, en la RAM y en el pendrive FAT32",
      "historias": "H08, H14, H32, H33",
      "estado": "feito"
    },
    {
      "conceito": "Entrada y salida",
      "implementa": "Interrupciones y drivers propios: teclado, ratón, vídeo, reloj, USB",
      "historias": "H05, H07, H17, H19, H29-H31",
      "estado": "feito"
    },
    {
      "conceito": "Protección",
      "implementa": "Un proceso no accede a la memoria de otro ni al kernel",
      "historias": "H12, H16",
      "estado": "feito"
    },
    {
      "conceito": "Llamadas al sistema",
      "implementa": "Procesos, archivos, directorios y varias",
      "historias": "H12",
      "estado": "feito"
    },
    {
      "conceito": "El shell",
      "implementa": "sagsh en ring 3, con redirección y pipes",
      "historias": "H16",
      "estado": "feito"
    },
    {
      "conceito": "Carga de programas",
      "implementa": "Ejecutables ELF leídos del sistema de archivos",
      "historias": "H13",
      "estado": "feito"
    },
    {
      "conceito": "Interfaz gráfica (extra)",
      "implementa": "Gestor de ventanas en el kernel; programas gráficos en ring 3",
      "historias": "H20-H28",
      "estado": "feito"
    }
  ],
  "acoes": [
    {
      "data": "2026-11-04T00:19:30",
      "texto": "Eli confirmó: ha funcionado todo perfectamente (bonus)"
    },
    {
      "data": "2026-11-03T23:45:56",
      "texto": "Fase 7 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T23:45:56",
      "texto": "B4 concluida: Sagui 3D: motor 2,5D propio, tres niveles en texto, monos capuchinos, semillas, cáscaras, verja de la banana, racimo dorado, sonidos con el pitido; prueba automática y capturas"
    },
    {
      "data": "2026-11-03T23:32:48",
      "texto": "B4 Sagui 3D: motor 2,5D, tres niveles, monos capuchinos, bananas, cáscaras, verja y racimo dorado; prueba automática superada"
    },
    {
      "data": "2026-11-03T23:02:29",
      "texto": "Inicio de la B4: Bonus: Sagui 3D"
    },
    {
      "data": "2026-11-03T23:02:23",
      "texto": "B3 concluida: Comando bip, sonidos en Sagui Atrapa Bananas, cola de notas reproducida con el tic del reloj (D029); canal 2 comprobado en QEMU"
    },
    {
      "data": "2026-11-03T22:57:35",
      "texto": "Inicio de la B3: Bonus: pitido"
    },
    {
      "data": "2026-11-03T22:57:35",
      "texto": "Fase 6 aprobada por Eli. Fase bonus replanificada: B1 y B2 cancelados por Eli, B3 se mantiene, B4 (Sagui 3D) nuevo."
    },
    {
      "data": "2026-11-03T22:57:09",
      "texto": "Eli confirmó: fase 6 ok"
    },
    {
      "data": "2026-11-03T21:56:30",
      "texto": "Fase 6 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T21:55:57",
      "texto": "Error corregido: G470: con dos pendrives, el de datos no aparece; usb muestra el hub del EHCI 0 detenido en la configuración"
    },
    {
      "data": "2026-11-03T21:36:41",
      "texto": "Fase 6 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T21:26:31",
      "texto": "Error corregido: una FAT32 creada después de la partición de SaguiOS no se ve"
    },
    {
      "data": "2026-11-03T21:26:31",
      "texto": "Error corregido: G470: el pendrive no aparece; usb muestra un puerto ocupado sin pendrive"
    },
    {
      "data": "2026-11-03T21:21:11",
      "texto": "Bloqueo: G470: el pendrive no aparece (fase 6). Corrección en curso: H30-fix"
    },
    {
      "data": "2026-11-03T20:50:19",
      "texto": "Fase 6 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T20:50:19",
      "texto": "Regresión final: 34/34 pruebas superadas"
    },
    {
      "data": "2026-11-03T20:25:28",
      "texto": "H34 concluida: Notificación, icono del pendrive, montar/extraer por la interfaz y por el terminal; comprobado con mtools y fsck"
    },
    {
      "data": "2026-11-03T20:25:27",
      "texto": "Error corregido: acentos cambiados en textos UTF-8 del pendrive"
    },
    {
      "data": "2026-11-03T20:20:50",
      "texto": "Inicio de la H34: Integración del pendrive"
    },
    {
      "data": "2026-11-03T20:20:50",
      "texto": "H33 concluida: Escritura FAT32 comprobada con mtools y fsck.vfat"
    },
    {
      "data": "2026-11-03T20:18:39",
      "texto": "Inicio de la H33: FAT32: escritura"
    },
    {
      "data": "2026-11-03T20:18:39",
      "texto": "H32 concluida: FAT32 con nombres largos y acentos montado en /usb; ls y cat funcionando"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "Inicio de la H32: FAT32: lectura"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "H31 concluida: Bulk-Only + SCSI; sectores leídos y escritos comprobados byte a byte en el VPS"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "H30 concluida: Enumeración, hubs (validación en el G470), conexión y extracción en caliente"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "H29 concluida: PCI, EHCI tomado de la BIOS, transferencias; comando usb"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "Inicio de la H31: Almacenamiento masivo"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "Inicio de la H30: Enumeración USB, hubs y conexión en caliente"
    },
    {
      "data": "2026-11-03T20:16:48",
      "texto": "Inicio de la H29: PCI y controlador EHCI"
    },
    {
      "data": "2026-11-03T20:04:57",
      "texto": "Inicio de la H29: PCI y controlador EHCI"
    },
    {
      "data": "2026-11-03T20:00:46",
      "texto": "Eli confirmó: fase 5 ok"
    },
    {
      "data": "2026-11-03T19:24:19",
      "texto": "Fase 5 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T19:24:19",
      "texto": "Todas las pruebas superadas (H01 a H28, 28/28)"
    },
    {
      "data": "2026-11-03T19:14:10",
      "texto": "H28 concluida: Iconos que abren los programas, pantalla Acerca de; imagen de 428 KB (29 % de la meta)"
    },
    {
      "data": "2026-11-03T19:11:50",
      "texto": "Inicio de la H28: Acabado"
    },
    {
      "data": "2026-11-03T19:11:50",
      "texto": "H27 concluida: Juego Sagui Atrapa Bananas con pixel art, vidas, velocidad creciente y récord"
    },
    {
      "data": "2026-11-03T19:09:30",
      "texto": "Inicio de la H27: Sagui Atrapa Bananas"
    },
    {
      "data": "2026-11-03T19:09:30",
      "texto": "H26 concluida: Calculadora en coma fija con botones y teclado; avisos de división por cero y número demasiado grande"
    },
    {
      "data": "2026-11-03T19:07:26",
      "texto": "Inicio de la H26: Calculadora"
    },
    {
      "data": "2026-11-03T19:07:26",
      "texto": "H23 concluida: Gestor de archivos con iconos; abre en el Editor, crea y borra carpetas"
    },
    {
      "data": "2026-11-03T19:03:23",
      "texto": "Inicio de la H23: Archivos"
    },
    {
      "data": "2026-11-03T19:03:23",
      "texto": "H25 concluida: Editor con Nuevo/Abrir/Guardar, Ctrl+S, título con *, aviso al cerrar"
    },
    {
      "data": "2026-11-03T19:00:05",
      "texto": "Inicio de la H25: Editor de texto"
    },
    {
      "data": "2026-11-03T19:00:05",
      "texto": "H24 concluida: Terminal en ventana ejecutando el sagsh por pipes; comandos que abren los programas gráficos"
    },
    {
      "data": "2026-11-03T18:56:46",
      "texto": "Inicio de la H24: Terminal en ventana"
    },
    {
      "data": "2026-11-03T18:56:46",
      "texto": "H22 concluida: Kit gráfico en la libsagui: botón, cuadro, área de texto con desplazamiento, lista, diálogos"
    },
    {
      "data": "2026-11-03T18:53:31",
      "texto": "Inicio de la H22: Kit gráfico de la libsagui"
    },
    {
      "data": "2026-11-03T18:53:31",
      "texto": "H21 concluida: Gestor de ventanas en el kernel, memoria compartida, eventos por ventana; arrastrar y cerrar probados"
    },
    {
      "data": "2026-11-03T18:53:30",
      "texto": "Error corregido: rastros al arrastrar ventanas"
    },
    {
      "data": "2026-11-03T18:46:46",
      "texto": "Inicio de la H21: Gestor de ventanas"
    },
    {
      "data": "2026-11-03T18:46:46",
      "texto": "H20 concluida: Escritorio con degradado de Brasil, mascota en pixel art, barra con reloj e iconos"
    },
    {
      "data": "2026-11-03T18:46:46",
      "texto": "Error corregido: el arranque no terminaba con el escritorio en marcha"
    },
    {
      "data": "2026-11-03T18:38:00",
      "texto": "Inicio de la H20: Escritorio"
    },
    {
      "data": "2026-11-03T18:37:28",
      "texto": "Eli confirmó: fase 4 ok"
    },
    {
      "data": "2026-11-03T18:27:00",
      "texto": "Fase 4 entregada. Esperando la prueba de Eli."
    },
    {
      "data": "2026-11-03T18:27:00",
      "texto": "H19 concluida: Ratón/touchpad PS/2; H19-fix: módulo i8042 único, teclado independiente del touchpad"
    },
    {
      "data": "2026-11-03T18:12:39",
      "texto": "Error corregido: G470: el teclado dejó de funcionar y el touchpad no respondió (fase 4)"
    },
    {
      "data": "2026-11-03T18:08:12",
      "texto": "Bloqueo: G470: el teclado no responde en la fase 4 y el touchpad no respondió (corrección en curso: H19-fix)"
    },
    {
      "data": "2026-11-03T17:40:18",
      "texto": "Fase 4 entregada. Esperando la prueba de Eli."
    }
  ],
  "erros": [
    {
      "data": "2026-11-03T21:55:57",
      "sintoma": "G470: con dos pendrives, el de datos no aparece; usb muestra el hub del EHCI 0 detenido en la configuración",
      "causa": "los dos controladores EHCI compartían el mismo anillo de transferencias",
      "correcao": "una página de estructuras por controlador; prueba con dos EHCI en QEMU"
    },
    {
      "data": "2026-11-03T21:26:31",
      "sintoma": "Una FAT32 creada después de la partición de SaguiOS no se ve",
      "causa": "solo se examinaba la primera partición del MBR",
      "correcao": "se examinan las cuatro particiones; vale la primera FAT32"
    },
    {
      "data": "2026-11-03T21:26:31",
      "sintoma": "G470: el pendrive no aparece; usb muestra un puerto ocupado sin pendrive",
      "causa": "el EHCI de 64 bits exige qTD/QH más grandes (apéndice B); el driver usaba el formato de 32 bits",
      "correcao": "qTD de 64 bytes y QH de 96 con las palabras altas a cero; diagnóstico en el comando usb (D028)"
    },
    {
      "data": "2026-11-03T20:25:27",
      "sintoma": "Acentos cambiados en textos UTF-8 del pendrive",
      "causa": "SaguiOS usa Latin-1; los archivos de fuera vienen en UTF-8",
      "correcao": "Conversión UTF-8 -> Latin-1 en cat y en el Editor"
    },
    {
      "data": "2026-11-03T18:53:30",
      "sintoma": "Rastros al arrastrar ventanas",
      "causa": "Contorno de 1 px fuera del área redibujada",
      "correcao": "El área redibujada incluye el contorno"
    },
    {
      "data": "2026-11-03T18:46:46",
      "sintoma": "El arranque no terminaba con el escritorio en marcha",
      "causa": "Tarea del gestor siempre lista; proceso 0 sin CPU",
      "correcao": "Interrupción 0x81 para que la tarea del kernel ceda el turno"
    },
    {
      "data": "2026-11-03T18:12:39",
      "sintoma": "G470: el teclado dejó de funcionar y el touchpad no respondió (fase 4)",
      "causa": "La inicialización del ratón podía escribir una configuración errónea en el controlador y dejar un byte del touchpad bloqueando la cola del teclado",
      "correcao": "Módulo i8042 único, cola vaciada también por el reloj, configuración comprobada, puerto del ratón desactivado si no responde"
    },
    {
      "data": "2026-11-03T16:41:24",
      "sintoma": "falha divisao no dividía",
      "causa": "GCC optimiza 1/x sin la instrucción div",
      "correcao": "Dividendo 12345"
    },
    {
      "data": "2026-11-03T16:25:45",
      "sintoma": "Regresión: 'Ola do ring 3!' desapareció en la prueba de la H12",
      "causa": "Los programas creados sin el cargador se quedaron sin descriptores tras la H14",
      "correcao": "La autoprueba da el terminal como 0, 1 y 2"
    },
    {
      "data": "2026-11-03T15:41:51",
      "sintoma": "cd ../arte llegaba como cd ..;arte",
      "causa": "El harness tecleaba / con la tecla estadounidense (en ABNT2 es ;)",
      "correcao": "El harness usa posiciones ABNT2"
    },
    {
      "data": "2026-11-03T15:39:25",
      "sintoma": "El saludo y la memoria partían la línea",
      "causa": "Textos de más de 80 columnas",
      "correcao": "Anchos fijos que suman 80"
    },
    {
      "data": "2026-11-03T15:39:25",
      "sintoma": "El árbol del SGFS salía como %*s en pantalla",
      "causa": "formatar() no aceptaba anchura con *",
      "correcao": "Soporte de * en formatar()"
    }
  ],
  "capturas": [
    {
      "arquivo": "docs/capturas/H23-confirmar-apagar.png",
      "nome": "H23-confirmar-apagar.png",
      "data": "2026-11-03T23:45:38"
    },
    {
      "arquivo": "docs/capturas/H23-nota-reaberta.png",
      "nome": "H23-nota-reaberta.png",
      "data": "2026-11-03T23:45:30"
    },
    {
      "arquivo": "docs/capturas/H23-linux-no-editor.png",
      "nome": "H23-linux-no-editor.png",
      "data": "2026-11-03T23:45:22"
    },
    {
      "arquivo": "docs/capturas/H23-arquivos-raiz.png",
      "nome": "H23-arquivos-raiz.png",
      "data": "2026-11-03T23:45:18"
    },
    {
      "arquivo": "docs/capturas/H24-terminal-ps-conta.png",
      "nome": "H24-terminal-ps-conta.png",
      "data": "2026-11-03T23:44:16"
    },
    {
      "arquivo": "docs/capturas/H16-tab-historico.png",
      "nome": "H16-tab-historico.png",
      "data": "2026-11-03T23:43:39"
    },
    {
      "arquivo": "docs/capturas/H16-falha-protecao.png",
      "nome": "H16-falha-protecao.png",
      "data": "2026-11-03T23:43:34"
    },
    {
      "arquivo": "docs/capturas/H16-contador-e-ps.png",
      "nome": "H16-contador-e-ps.png",
      "data": "2026-11-03T23:43:22"
    },
    {
      "arquivo": "docs/capturas/H16-arquivos-redirecionamento-pipe.png",
      "nome": "H16-arquivos-redirecionamento-pipe.png",
      "data": "2026-11-03T23:43:19"
    },
    {
      "arquivo": "docs/capturas/H16-ajuda.png",
      "nome": "H16-ajuda.png",
      "data": "2026-11-03T23:43:02"
    },
    {
      "arquivo": "docs/capturas/B4-7-macaco-escorregou.png",
      "nome": "B4-7-macaco-escorregou.png",
      "data": "2026-11-03T23:42:31"
    },
    {
      "arquivo": "docs/capturas/B4-6-macaco-tonto.png",
      "nome": "B4-6-macaco-tonto.png",
      "data": "2026-11-03T23:42:28"
    },
    {
      "arquivo": "docs/capturas/B4-5-vitoria.png",
      "nome": "B4-5-vitoria.png",
      "data": "2026-11-03T23:42:20"
    },
    {
      "arquivo": "docs/capturas/B4-4-porteira-aberta.png",
      "nome": "B4-4-porteira-aberta.png",
      "data": "2026-11-03T23:42:15"
    },
    {
      "arquivo": "docs/capturas/B4-3-fase1-corredor.png",
      "nome": "B4-3-fase1-corredor.png",
      "data": "2026-11-03T23:42:07"
    },
    {
      "arquivo": "docs/capturas/B4-2-fase1-clareira.png",
      "nome": "B4-2-fase1-clareira.png",
      "data": "2026-11-03T23:42:04"
    },
    {
      "arquivo": "docs/capturas/B4-1-tela-de-inicio.png",
      "nome": "B4-1-tela-de-inicio.png",
      "data": "2026-11-03T23:42:02"
    },
    {
      "arquivo": "docs/capturas/B3-comando-bip.png",
      "nome": "B3-comando-bip.png",
      "data": "2026-11-03T23:41:42"
    },
    {
      "arquivo": "docs/capturas/H34-8-um-pendrive-so.png",
      "nome": "H34-8-um-pendrive-so.png",
      "data": "2026-11-03T23:41:29"
    },
    {
      "arquivo": "docs/capturas/H34-7-terminal.png",
      "nome": "H34-7-terminal.png",
      "data": "2026-11-03T23:41:19"
    },
    {
      "arquivo": "docs/capturas/H34-6-removido-sem-desmontar.png",
      "nome": "H34-6-removido-sem-desmontar.png",
      "data": "2026-11-03T23:41:09"
    },
    {
      "arquivo": "docs/capturas/H34-5-pendrive-removido.png",
      "nome": "H34-5-pendrive-removido.png",
      "data": "2026-11-03T23:41:03"
    },
    {
      "arquivo": "docs/capturas/H34-4-pode-remover.png",
      "nome": "H34-4-pode-remover.png",
      "data": "2026-11-03T23:41:02"
    },
    {
      "arquivo": "docs/capturas/H34-3-editor-salvou-no-pendrive.png",
      "nome": "H34-3-editor-salvou-no-pendrive.png",
      "data": "2026-11-03T23:40:58"
    },
    {
      "arquivo": "docs/capturas/H34-2-arquivos-no-pendrive.png",
      "nome": "H34-2-arquivos-no-pendrive.png",
      "data": "2026-11-03T23:40:52"
    },
    {
      "arquivo": "docs/capturas/H34-1-pendrive-conectado.png",
      "nome": "H34-1-pendrive-conectado.png",
      "data": "2026-11-03T23:40:49"
    },
    {
      "arquivo": "docs/capturas/H33-pendrive-gravado.png",
      "nome": "H33-pendrive-gravado.png",
      "data": "2026-11-03T23:40:37"
    },
    {
      "arquivo": "docs/capturas/H32-ls-pendrive.png",
      "nome": "H32-ls-pendrive.png",
      "data": "2026-11-03T23:39:47"
    },
    {
      "arquivo": "docs/capturas/H30-conexao-a-quente.png",
      "nome": "H30-conexao-a-quente.png",
      "data": "2026-11-03T23:39:21"
    },
    {
      "arquivo": "docs/capturas/H29-diagnostico-usb.png",
      "nome": "H29-diagnostico-usb.png",
      "data": "2026-11-03T23:39:05"
    },
    {
      "arquivo": "docs/capturas/H29-dois-controladores.png",
      "nome": "H29-dois-controladores.png",
      "data": "2026-11-03T23:38:58"
    },
    {
      "arquivo": "docs/capturas/H28-tres-programas.png",
      "nome": "H28-tres-programas.png",
      "data": "2026-11-03T23:38:49"
    },
    {
      "arquivo": "docs/capturas/H28-programas-abertos.png",
      "nome": "H28-programas-abertos.png",
      "data": "2026-11-03T23:38:42"
    },
    {
      "arquivo": "docs/capturas/H27-banana-fim-de-jogo.png",
      "nome": "H27-banana-fim-de-jogo.png",
      "data": "2026-11-03T23:38:25"
    },
    {
      "arquivo": "docs/capturas/H27-banana-jogando.png",
      "nome": "H27-banana-jogando.png",
      "data": "2026-11-03T23:38:15"
    },
    {
      "arquivo": "docs/capturas/H27-banana-inicio.png",
      "nome": "H27-banana-inicio.png",
      "data": "2026-11-03T23:38:13"
    },
    {
      "arquivo": "docs/capturas/H26-calculadora-divisao-por-zero.png",
      "nome": "H26-calculadora-divisao-por-zero.png",
      "data": "2026-11-03T23:38:02"
    },
    {
      "arquivo": "docs/capturas/H26-calculadora-3-5.png",
      "nome": "H26-calculadora-3-5.png",
      "data": "2026-11-03T23:37:55"
    },
    {
      "arquivo": "docs/capturas/H26-calculadora-50.png",
      "nome": "H26-calculadora-50.png",
      "data": "2026-11-03T23:37:54"
    },
    {
      "arquivo": "docs/capturas/H25-aviso-ao-fechar.png",
      "nome": "H25-aviso-ao-fechar.png",
      "data": "2026-11-03T23:37:34"
    },
    {
      "arquivo": "docs/capturas/H25-editor-reaberto.png",
      "nome": "H25-editor-reaberto.png",
      "data": "2026-11-03T23:37:31"
    },
    {
      "arquivo": "docs/capturas/H25-editor-digitando.png",
      "nome": "H25-editor-digitando.png",
      "data": "2026-11-03T23:37:25"
    },
    {
      "arquivo": "docs/capturas/H22-dialogo-confirmar.png",
      "nome": "H22-dialogo-confirmar.png",
      "data": "2026-11-03T23:36:51"
    },
    {
      "arquivo": "docs/capturas/H22-componentes-em-uso.png",
      "nome": "H22-componentes-em-uso.png",
      "data": "2026-11-03T23:36:49"
    },
    {
      "arquivo": "docs/capturas/H22-componentes.png",
      "nome": "H22-componentes.png",
      "data": "2026-11-03T23:36:39"
    },
    {
      "arquivo": "docs/capturas/H21-janela-fechada.png",
      "nome": "H21-janela-fechada.png",
      "data": "2026-11-03T23:36:33"
    },
    {
      "arquivo": "docs/capturas/H21-janela-arrastada.png",
      "nome": "H21-janela-arrastada.png",
      "data": "2026-11-03T23:36:29"
    },
    {
      "arquivo": "docs/capturas/H21-duas-janelas.png",
      "nome": "H21-duas-janelas.png",
      "data": "2026-11-03T23:36:27"
    },
    {
      "arquivo": "docs/capturas/H20-area-de-trabalho.png",
      "nome": "H20-area-de-trabalho.png",
      "data": "2026-11-03T23:36:13"
    },
    {
      "arquivo": "docs/capturas/H19-cursor-movido.png",
      "nome": "H19-cursor-movido.png",
      "data": "2026-11-03T23:35:56"
    },
    {
      "arquivo": "docs/capturas/H18-edicao-e-segundo-plano.png",
      "nome": "H18-edicao-e-segundo-plano.png",
      "data": "2026-11-03T23:35:40"
    },
    {
      "arquivo": "docs/capturas/H18-fonte-acentos.png",
      "nome": "H18-fonte-acentos.png",
      "data": "2026-11-03T23:35:35"
    },
    {
      "arquivo": "docs/capturas/H17-formas-ide.png",
      "nome": "H17-formas-ide.png",
      "data": "2026-11-03T23:35:12"
    },
    {
      "arquivo": "docs/capturas/H15-pipes.png",
      "nome": "H15-pipes.png",
      "data": "2026-11-03T23:34:57"
    },
    {
      "arquivo": "docs/capturas/H14-arquivos-gravaveis.png",
      "nome": "H14-arquivos-gravaveis.png",
      "data": "2026-11-03T23:34:51"
    },
    {
      "arquivo": "docs/capturas/H13-ola-argumentos.png",
      "nome": "H13-ola-argumentos.png",
      "data": "2026-11-03T23:34:39"
    },
    {
      "arquivo": "docs/capturas/H12-anel3-protecao.png",
      "nome": "H12-anel3-protecao.png",
      "data": "2026-11-03T23:34:33"
    },
    {
      "arquivo": "docs/capturas/H11-tarefas-A-B.png",
      "nome": "H11-tarefas-A-B.png",
      "data": "2026-11-03T23:34:27"
    },
    {
      "arquivo": "docs/capturas/H10-tela-falta-de-pagina.png",
      "nome": "H10-tela-falta-de-pagina.png",
      "data": "2026-11-03T23:34:20"
    },
    {
      "arquivo": "docs/capturas/H10-dois-espacos.png",
      "nome": "H10-dois-espacos.png",
      "data": "2026-11-03T23:34:14"
    },
    {
      "arquivo": "docs/capturas/H09-tela-inicial-usb.png",
      "nome": "H09-tela-inicial-usb.png",
      "data": "2026-11-03T23:34:08"
    },
    {
      "arquivo": "docs/capturas/H09-mem-data.png",
      "nome": "H09-mem-data.png",
      "data": "2026-11-03T23:34:05"
    },
    {
      "arquivo": "docs/capturas/H09-cat-sagui.png",
      "nome": "H09-cat-sagui.png",
      "data": "2026-11-03T23:34:03"
    },
    {
      "arquivo": "docs/capturas/H09-ls-cd.png",
      "nome": "H09-ls-cd.png",
      "data": "2026-11-03T23:34:01"
    },
    {
      "arquivo": "docs/capturas/H09-ajuda.png",
      "nome": "H09-ajuda.png",
      "data": "2026-11-03T23:33:59"
    },
    {
      "arquivo": "docs/capturas/H09-tela-inicial-ide.png",
      "nome": "H09-tela-inicial-ide.png",
      "data": "2026-11-03T23:33:59"
    },
    {
      "arquivo": "docs/capturas/H08-sgfs-lista.png",
      "nome": "H08-sgfs-lista.png",
      "data": "2026-11-03T23:33:52"
    },
    {
      "arquivo": "docs/capturas/H07-teclado-abnt2.png",
      "nome": "H07-teclado-abnt2.png",
      "data": "2026-11-03T23:33:46"
    },
    {
      "arquivo": "docs/capturas/H06-autoteste-memoria.png",
      "nome": "H06-autoteste-memoria.png",
      "data": "2026-11-03T23:33:36"
    },
    {
      "arquivo": "docs/capturas/H05-tela-de-erro-divisao.png",
      "nome": "H05-tela-de-erro-divisao.png",
      "data": "2026-11-03T23:33:16"
    },
    {
      "arquivo": "docs/capturas/H04-boot-vdi.png",
      "nome": "H04-boot-vdi.png",
      "data": "2026-11-03T23:33:10"
    },
    {
      "arquivo": "docs/capturas/H04-fix-memoria-6g.png",
      "nome": "H04-fix-memoria-6g.png",
      "data": "2026-11-03T23:32:53"
    },
    {
      "arquivo": "docs/capturas/H03-kernel-usb.png",
      "nome": "H03-kernel-usb.png",
      "data": "2026-11-03T23:32:41"
    },
    {
      "arquivo": "docs/capturas/H03-kernel-ide.png",
      "nome": "H03-kernel-ide.png",
      "data": "2026-11-03T23:32:28"
    },
    {
      "arquivo": "docs/capturas/H03-estagio2-usb.png",
      "nome": "H03-estagio2-usb.png",
      "data": "2026-11-03T23:32:22"
    },
    {
      "arquivo": "docs/capturas/H03-estagio2-ide.png",
      "nome": "H03-estagio2-ide.png",
      "data": "2026-11-03T23:32:16"
    },
    {
      "arquivo": "docs/capturas/H02-mbr-usb.png",
      "nome": "H02-mbr-usb.png",
      "data": "2026-11-03T23:32:09"
    },
    {
      "arquivo": "docs/capturas/H02-mbr-ide.png",
      "nome": "H02-mbr-ide.png",
      "data": "2026-11-03T23:32:03"
    },
    {
      "arquivo": "docs/capturas/G470-fase5-janelas.png",
      "nome": "G470-fase5-janelas.png",
      "data": "2026-11-03T20:00:46"
    },
    {
      "arquivo": "docs/capturas/G470-fase4-grafico-touchpad.png",
      "nome": "G470-fase4-grafico-touchpad.png",
      "data": "2026-11-03T18:37:28"
    },
    {
      "arquivo": "docs/capturas/G470-fase3-contador-ps.png",
      "nome": "G470-fase3-contador-ps.png",
      "data": "2026-11-03T17:21:50"
    },
    {
      "arquivo": "docs/capturas/G470-fase2-tela-inicial.png",
      "nome": "G470-fase2-tela-inicial.png",
      "data": "2026-11-03T16:05:16"
    },
    {
      "arquivo": "docs/capturas/G470-fase1-tela-final.png",
      "nome": "G470-fase1-tela-final.png",
      "data": "2026-11-03T15:22:25"
    }
  ],
  "linhas": {
    "boot": 903,
    "kernel": 9614,
    "userland": 6317,
    "ferramentas": 3591
  },
  "imagem": {
    "bytes_util": 575488,
    "bytes_arquivo": 1048576,
    "meta_bytes": 1474560
  },
  "progresso_geral": 100.0
};
