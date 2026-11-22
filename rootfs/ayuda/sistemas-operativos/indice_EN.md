<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 0; titulo: Operating systems fundamentals applied to SaguiOS -->
# Operating systems fundamentals applied to SaguiOS

[Español](indice.md) · [Português](indice_PT.md) · [English](indice_EN.md)

> A fundamentals lesson on operating systems for computer engineering, with SaguiOS as a real case.
> Each chapter explains the general theory, how SaguiOS solves the same problem (with the files of the code and the
> project's `Dxxx` decisions), an experiment to do inside the system, and review questions.

You can read it in order: each chapter uses concepts from the previous ones. File references are paths in the SaguiOS
repository (`kernel/`, `userland/`, `boot/`); the decisions (`D...`) are in `docs/decisoes.md`.

## Chapters

1. [What an operating system is](01-que-es-un-so_EN.md): functions, monolithic kernel versus microkernel, where SaguiOS is.
2. [Boot](02-arranque_EN.md): BIOS and UEFI, MBR, stage 2, protected mode, handover to the kernel, initramfs and services.
3. [Kernel mode and user mode](03-modo-nucleo-usuario_EN.md): x86 rings, interrupts, exceptions, IDT and system calls (`int 0x80`).
4. [Processes and threads](04-procesos_EN.md): PCB, states, `fork`, `execve`, `waitpid`, signals and zombies.
5. [Scheduling](05-planificacion_EN.md): quantum, priorities, multilevel queues, preemption, audio priority and SMP with a kernel-wide lock.
6. [Memory](06-memoria_EN.md): 32-bit paging, TLB, page faults, replacement, working set, shared libraries and dynamic linking, PAE.
7. [Concurrency and synchronization](07-concurrencia_EN.md): race conditions, critical sections, locks, monitors, classic problems, futex, the kernel-wide lock, deadlocks and priority inversion.
8. [File systems](08-sistemas-de-archivos_EN.md): VFS, inodes and blocks, ext2 and ext4 with journal, read-only XFS, FAT32, exFAT, SGFS, block cache, directories and permissions.
9. [Input/output and drivers](09-entrada-salida_EN.md): interrupts (PIC, APIC, MSI), DMA, polling versus interrupts, disk drivers and arm scheduling, RAID, USB, sound and the SaguiOS driver model.
10. [Networking](10-redes_EN.md): TCP/IP stack, sockets, ARP, DHCP, several interfaces and routes, USB Ethernet, reliability over UDP and distributed systems.
11. [Graphics](11-graficos_EN.md): framebuffer, window compositor, 2D and 3D acceleration (gpu3d v3), fallback to the CPU.
12. [Security, power and shutdown](12-seguridad-energia_EN.md): users, passwords, permissions, ACLs, attacks, ACPI, CPU power and orderly reboot with deadlines (D450).
13. [Appendix: the SDL library in SaguiOS](anexo-sdl_EN.md): SaguiOS's own SDL 2 over libsagui, threads, locks, audio on its own thread and priorities, with C examples.
14. [Appendix: x86 assembler](anexo-ensamblador_EN.md): 16 and 32 bits, GDT, IDT, `int 0x80`, ring 3, paging and inline assembly, with examples.

## Correspondence with the reference book

| Book (first edition) | Chapters of this course |
| --- | --- |
| 1. Introduction (what an OS is, history, concepts, structure) | 1, 3 |
| 2. Processes (model, communication, classic problems, scheduling) | 4, 5, 7 |
| 3. Memory management (swapping, virtual memory, replacement, segmentation) | 6 |
| 4. File systems (files, directories, implementation, security and protection) | 8, 12 |
| 5. Input/output (hardware, software, disks, clocks, terminals) | 9, 11 |
| 6. Deadlocks | 7 |
| 7. Case study of UNIX and 8. Case study of MS-DOS | 3, 4, 8 (Unix-style calls, processes and files); comparisons in 1 and 3 |
| Part 2. Distributed systems (communication, multiprocessors and multicomputers) | 5, 7, 10 (section "Distributed systems") |

What the book does not have and the course does: the assembly and SDL appendices, power and shutdown, and GPU graphics
acceleration.

## Reference bibliography

- Tanenbaum, Andrew S.: *Sistemas operativos modernos* (Spanish edition). Translated by Oscar Alfredo Palmas Velasco; technical
  review by Gabriel Guerrero. Mexico: Prentice-Hall Hispanoamericana, 1993 (ISBN 968-880-323-5). It is the translation of the first
  edition of *Modern Operating Systems* (Prentice Hall, 1992; ISBN 0-13-588187-0) and the reference book of the course. Later
  editions (the fourth, by Tanenbaum and Bos, Pearson, 2015) extend topics such as virtualization; this course explains the more
  recent ones with SaguiOS. This lesson does not copy the book's text, figures or exercises: the concepts are explained with our
  own examples and with the SaguiOS code.
- To read the code: the kernel guide, at `/ayuda/guias/guia-kernel.md` (it is the same as `docs/guia-kernel.md` in the
  repository).
