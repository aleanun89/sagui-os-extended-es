<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 0; titulo: Fundamentos de sistemas operacionais aplicados ao SaguiOS -->
# Fundamentos de sistemas operacionais aplicados ao SaguiOS

[Español](indice.md) · [Português](indice_PT.md) · [English](indice_EN.md)

> Aula de fundamentos de sistemas operacionais para engenharia da computação, com o SaguiOS como caso real.
> Cada capítulo explica a teoria geral, como o SaguiOS resolve o mesmo problema (com os arquivos do código e as
> decisões `Dxxx` do projeto), um experimento para fazer dentro do sistema e perguntas de revisão.

Pode ser lido em ordem: cada capítulo usa conceitos dos anteriores. As referências a arquivos são caminhos do
repositório do SaguiOS (`kernel/`, `userland/`, `boot/`); as decisões (`D...`) estão em `docs/decisoes.md`.

## Capítulos

1. [O que é um sistema operacional](01-que-es-un-so_PT.md): funções, kernel monolítico frente a microkernel, onde está o SaguiOS.
2. [Inicialização](02-arranque_PT.md): BIOS e UEFI, MBR, estágio 2, modo protegido, passagem ao kernel, initramfs e serviços.
3. [Modo núcleo e modo usuário](03-modo-nucleo-usuario_PT.md): anéis do x86, interrupções, exceções, IDT e chamadas ao sistema (`int 0x80`).
4. [Processos e threads](04-procesos_PT.md): PCB, estados, `fork`, `execve`, `waitpid`, sinais e zumbis.
5. [Escalonamento](05-planificacion_PT.md): quantum, prioridades, filas múltiplas, preempção, prioridade de áudio e SMP com trava geral.
6. [Memória](06-memoria_PT.md): paginação de 32 bits, TLB, falhas de página, substituição, conjunto de trabalho, bibliotecas compartilhadas e ligação dinâmica, PAE.
7. [Concorrência e sincronização](07-concurrencia_PT.md): condições de corrida, seções críticas, travas, monitores, problemas clássicos, futex, a trava geral do kernel, impasses e inversão de prioridade.
8. [Sistemas de arquivos](08-sistemas-de-archivos_PT.md): VFS, inodos e blocos, ext2 e ext4 com diário, XFS somente leitura, FAT32, exFAT, SGFS, cache de blocos, diretórios e permissões.
9. [Entrada/saída e drivers](09-entrada-salida_PT.md): interrupções (PIC, APIC, MSI), DMA, sondagem frente a interrupções, drivers de disco e escalonamento do braço, RAID, USB, som e o modelo de drivers do SaguiOS.
10. [Redes](10-redes_PT.md): pilha TCP/IP, sockets, ARP, DHCP, várias interfaces e rotas, Ethernet USB, confiabilidade sobre UDP e sistemas distribuídos.
11. [Gráficos](11-graficos_PT.md): framebuffer, compositor de janelas, aceleração 2D e 3D (gpu3d v3) e volta à CPU.
12. [Segurança, energia e desligamento](12-seguridad-energia_PT.md): usuários, senhas, permissões, ACLs, ataques, ACPI, energia da CPU e reinício ordenado com prazos (D450).
13. [Anexo: a biblioteca SDL no SaguiOS](anexo-sdl_PT.md): SDL 2 própria sobre a libsagui, threads, travas, áudio em thread própria e prioridades, com exemplos em C.
14. [Anexo: assembler x86](anexo-ensamblador_PT.md): 16 e 32 bits, GDT, IDT, `int 0x80`, anel 3, paginação e assembly em linha, com exemplos.

## Correspondência com o livro de referência

| Livro (primeira edição) | Capítulos deste curso |
| --- | --- |
| 1. Introdução (o que é um SO, história, conceitos, estrutura) | 1, 3 |
| 2. Processos (modelo, comunicação, problemas clássicos, escalonamento) | 4, 5, 7 |
| 3. Gerenciamento de memória (troca, memória virtual, substituição, segmentação) | 6 |
| 4. Sistemas de arquivos (arquivos, diretórios, implementação, segurança e proteção) | 8, 12 |
| 5. Entrada/saída (hardware, software, discos, relógios, terminais) | 9, 11 |
| 6. Impasses | 7 |
| 7. Estudo do UNIX e 8. Estudo do MS-DOS | 3, 4, 8 (chamadas, processos e arquivos ao estilo Unix); comparações em 1 e 3 |
| Parte 2. Sistemas distribuídos (comunicação, multiprocessadores e multicomputadores) | 5, 7, 10 (seção «Sistemas distribuídos») |

O que o livro não traz e o curso sim: os anexos de assembly e de SDL, a energia e o desligamento, e a aceleração gráfica por GPU.

## Bibliografia de referência

- Tanenbaum, Andrew S.: *Sistemas operativos modernos* (edição em espanhol). Tradução de Oscar Alfredo Palmas Velasco; revisão técnica de
  Gabriel Guerrero. México: Prentice-Hall Hispanoamericana, 1993 (ISBN 968-880-323-5). É a tradução da primeira edição de *Modern
  Operating Systems* (Prentice Hall, 1992; ISBN 0-13-588187-0) e o livro de referência da disciplina. As edições posteriores (a quarta, de
  Tanenbaum e Bos, Pearson, 2015) ampliam temas como a virtualização; este curso explica os mais recentes com o SaguiOS. Esta aula não copia
  o texto, as figuras nem os exercícios do livro: os conceitos são explicados com exemplos próprios e com o código do SaguiOS.
- Para ler o código: o guia do kernel, em `/ayuda/guias/guia-kernel.md` (é o mesmo que `docs/guia-kernel.md` no repositório).
