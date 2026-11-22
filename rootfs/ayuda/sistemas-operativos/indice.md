<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 0; titulo: Fundamentos de sistemas operativos aplicados a SaguiOS -->
# Fundamentos de sistemas operativos aplicados a SaguiOS

[Español](indice.md) · [Português](indice_PT.md) · [English](indice_EN.md)

> Lección de fundamentos de sistemas operativos para ingeniería informática, con SaguiOS como caso real.
> Cada capítulo explica la teoría general, cómo lo resuelve SaguiOS (con los ficheros del código y las
> decisiones `Dxxx` del proyecto), un experimento para hacer dentro del sistema y preguntas de repaso.

Se puede leer en orden: cada capítulo usa conceptos de los anteriores. Las referencias a ficheros son rutas del
repositorio de SaguiOS (`kernel/`, `userland/`, `boot/`); las decisiones (`D...`) están en `docs/decisoes.md`.

## Capítulos

1. [Qué es un sistema operativo](01-que-es-un-so.md): funciones, núcleo monolítico frente a micronúcleo, dónde está SaguiOS.
2. [Arranque](02-arranque.md): BIOS y UEFI, MBR, etapa 2, modo protegido, paso al kernel, initramfs y servicios.
3. [Modo núcleo y modo usuario](03-modo-nucleo-usuario.md): anillos de x86, interrupciones, excepciones, IDT y `int 0x80`.
4. [Procesos e hilos](04-procesos.md): PCB, estados, `fork`, `execve`, `waitpid`, señales y zombis.
5. [Planificación](05-planificacion.md): quantum, prioridades, colas múltiples, apropiación, prioridad de audio y SMP con cerrojo general.
6. [Memoria](06-memoria.md): paginación, TLB, fallos de página, reemplazo, conjunto de trabajo, bibliotecas compartidas, enlace dinámico y PAE.
7. [Concurrencia y sincronización](07-concurrencia.md): condiciones de carrera, cerrojos, monitores, problemas clásicos, futex, cerrojo general, interbloqueos e inversión de prioridad.
8. [Sistemas de archivos](08-sistemas-de-archivos.md): VFS, inodos, ext2 y ext4 con diario, XFS de solo lectura, FAT32, exFAT, SGFS, caché, directorios y permisos.
9. [Entrada/salida y drivers](09-entrada-salida.md): interrupciones, DMA, sondeo, discos y su planificación, RAID, USB, sonido y el modelo de drivers.
10. [Redes](10-redes.md): pila TCP/IP, sockets, ARP, DHCP, varias interfaces y rutas, Ethernet USB, fiabilidad sobre UDP y sistemas distribuidos.
11. [Gráficos](11-graficos.md): framebuffer, compositor, aceleración 2D y 3D por GPU (gpu3d v3), y vuelta a la CPU.
12. [Seguridad, energía y apagado](12-seguridad-energia.md): usuarios, contraseñas, permisos, ACL, ataques, ACPI, energía de la CPU y reinicio ordenado.
13. [Anexo: la biblioteca SDL en SaguiOS](anexo-sdl.md): SDL 2 propia sobre la libsagui, hilos, cerrojos, audio en hilo propio y prioridades, con ejemplos en C.
14. [Anexo: ensamblador x86](anexo-ensamblador.md): 16 y 32 bits, GDT, IDT, `int 0x80`, anillo 3, paginación y ensamblador en línea, con ejemplos.

## Correspondencia con el libro de referencia

| Libro (primera edición) | Capítulos de este curso |
| --- | --- |
| 1. Introducción (qué es un SO, historia, conceptos, estructura) | 1, 3 |
| 2. Procesos (modelo, comunicación, problemas clásicos, planificación) | 4, 5, 7 |
| 3. Administración de la memoria (intercambio, memoria virtual, reemplazo, segmentación) | 6 |
| 4. Sistemas de archivo (archivos, directorios, implantación, seguridad y protección) | 8, 12 |
| 5. Entrada/salida (hardware, software, discos, relojes, terminales) | 9, 11 |
| 6. Bloqueos | 7 |
| 7. Estudio de UNIX y 8. Estudio de MS-DOS | 3, 4, 8 (llamadas, procesos y archivos tipo Unix); comparativas en 1 y 3 |
| Parte 2. Sistemas distribuidos (comunicación, multiprocesadores y multicomputadoras) | 5, 7, 10 (apartado «Sistemas distribuidos») |

Lo que el libro no trae y el curso sí: los anexos de ensamblador y de SDL, la energía y el apagado, y la aceleración
gráfica por GPU.

## Bibliografía de referencia

- Tanenbaum, Andrew S.: *Sistemas operativos modernos*. Traducción de Oscar Alfredo Palmas Velasco; revisión técnica de
  Gabriel Guerrero. México: Prentice-Hall Hispanoamericana, 1993 (ISBN 968-880-323-5). Es la traducción de la primera edición
  de *Modern Operating Systems* (Prentice Hall, 1992; ISBN 0-13-588187-0) y el libro de referencia de la asignatura. Las
  ediciones posteriores (la cuarta, de Tanenbaum y Bos, Pearson, 2015) amplían temas como la virtualización; este curso explica
  los más recientes con SaguiOS. Esta lección no copia el texto, las figuras ni los ejercicios del libro: los conceptos se
  explican con ejemplos propios y con el código de SaguiOS.
- Para leer el código: la guía del kernel, en `/ayuda/guias/guia-kernel.md` (es la misma que `docs/guia-kernel.md` del
  repositorio).
