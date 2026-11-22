<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 1; titulo: What an operating system is -->
# What an operating system is

[Español](01-que-es-un-so.md) · [Português](01-que-es-un-so_PT.md) · [English](01-que-es-un-so_EN.md)

> Goal: understand what an operating system does, why it is organized in layers, and where SaguiOS fits.

## 1. Theory

### 1.1 A program that manages the hardware

A computer is hardware: processor, memory, disks, network, screen. Application programs want to use that hardware
without knowing its details, and several programs want to use it at the same time. The **operating system** is the
software that acts as the intermediary. It has four classic functions:

- **Abstract.** It offers simpler models than the hardware: a *process* instead of a CPU, a *file* instead of disk
  sectors, a *socket* instead of a network controller.
- **Share.** It divides the CPU, memory, disk and network among programs, with rules of fairness, priority and quotas.
- **Protect.** It prevents a program from overwriting another program's memory, or a user from reading another user's
  files without permission.
- **Coordinate.** It lets programs communicate and wait for each other (pipes, signals, locks).

That set is called the **kernel**: the part that runs with all the privileges of the processor. Everything else (shell,
editor, browser) is an ordinary program that asks it for services.

### 1.2 Monolithic kernel versus microkernel

There are two large families of design:

| Design | What goes inside the kernel | Advantages | Disadvantages | Examples |
| --- | --- | --- | --- | --- |
| **Monolithic** | Scheduler, memory, file systems, drivers and network stack, all in one privileged space. | Calls between parts are direct functions: fast. A single binary is easier to debug. | A driver failure can bring the whole system down. The privileged code is huge. | Linux, FreeBSD, OpenBSD |
| **Microkernel** | Only the minimum: processes, address spaces, IPC. Drivers and file systems are user processes. | More isolation: a failing driver is restarted on its own. The kernel is small and auditable. | Each service needs messages between processes: more costly unless designed carefully. | MINIX 3, QNX, L4 |

In practice, almost all real systems are **hybrid** or monolithic with modules. The debate about which is better is about
reliability versus speed, and about how much third-party code one wants to isolate.

### 1.3 System calls and privileged mode

A program does not touch the hardware directly. It asks for services with **system calls**: `read`, `write`, `fork`,
`open`. The CPU enforces the boundary with two execution modes (we will see them in chapter 3). That boundary is what
makes it possible to protect the system.

### 1.4 Other systems, for comparison

- **Linux**: monolithic with loadable modules; the whole kernel runs in privileged mode.
- **Windows NT**: hybrid kernel, with layers (HAL, executive, I/O manager) and many services outside the kernel.
- **BSD (FreeBSD, OpenBSD)**: monolithic, with very readable code and documentation; OpenBSD prioritizes security.
- **MINIX 3**: microkernel with drivers as processes that restart by themselves. It is the classic example in this course.

### 1.5 A simple calculation: the cost of separating the kernel from the services

Suppose a service costs **0.8 µs** inside a monolithic kernel (enter, do the work and leave). In a microkernel the same
service needs a message to the server: a process switch there and back, 2 µs each, plus copying the message (0.3 µs).
Total: 2 × 2 + 0.3 = **4.3 µs**, about five times more. The numbers are illustrative (it is an exercise, not a
measurement), but the reasoning is real: isolation has a price measured in time, and microkernels reduce it by avoiding
unnecessary process switches.

### 1.6 What goes inside the kernel: comparison

| System | Model | Where the drivers live | Can be loaded at run time |
| --- | --- | --- | --- |
| Linux | monolithic with modules | in the kernel, as modules | yes |
| FreeBSD | monolithic with kernel modules | in the kernel | yes |
| Windows NT | hybrid, layered | in kernel mode, with the driver model | yes |
| MINIX 3 | microkernel | user processes | yes (they are restarted) |
| SaguiOS | monolithic in a single image | inside `kernel/dispositivos/`, compiled into the kernel | not the goal of the course |

### 1.7 Other structures: layers, virtual machines and client-server

Besides the monolithic kernel and the microkernel there are other ways to organize a system:

- **Layers.** The system is split into levels and each one only uses the services of the one below (hardware, memory, I/O,
  user programs...). Each level can be verified on its own, and it is the model of network stacks (chapter 10). The
  problem: real functions cross layers (virtual memory needs the disk, and the disk needs memory) and going through many
  layers takes time.
- **Virtual machines.** A **hypervisor** (the first was IBM's VM/370) multiplies one physical machine into several virtual
  machines that behave like the hardware, and each one runs its own "guest" operating system. QEMU and KVM are today's PC
  version. SaguiOS is almost always tested as a QEMU guest (with KVM or with QEMU's emulated CPU, called TCG). That
  shapes the design: a guest does not know when the hypervisor takes its virtual CPU away, which is why the kernel-wide
  lock changes how it hands out turns when it detects a hypervisor (chapter 7).
- **Client-server.** It is the microkernel idea taken to the whole system: a client asks a server process for a service
  with messages, and it does not matter whether the server is on the same machine or on another (chapter 10).

The structure decides what can be isolated and what each service costs. None is best at everything, and real systems mix
several.

### 1.8 A short history: from batches to the PC

- **Batches** (1950s and 60s): an operator loads jobs and the machine runs them one at a time; the CPU waits while a tape
  is read.
- **Multiprogramming**: several jobs in memory at once; while one waits for I/O, the CPU serves another (chapters 5 and 6).
  It requires memory protection.
- **Time sharing**: many interactive users on one machine, with short CPU slices. Unix was born there.
- **Personal computer**: small single-task systems (like MS-DOS) that over the years won back the multitasking, virtual
  memory and protection of the big ones.

SaguiOS is a PC system of the last stage: preemptive multitasking, protected memory, users and network.

## 2. How SaguiOS does it

SaguiOS-Extended is an **educational 32-bit (i686) system**. Its kernel is **monolithic**: the scheduler, memory, file
systems, drivers and network stack are compiled into a single image that runs with kernel privileges. The tree shows
it: `kernel/nucleo/` (processes), `kernel/mem/` (memory), `kernel/fs/` (files), `kernel/dispositivos/` (drivers) and
`kernel/rede/` (network) are folders of the same kernel.

Some concrete points:

- **A single privileged space.** The code of `kernel/fs/vfs.c` calls `kernel/fs/ext2.c` or `kernel/fs/fat32.c` directly,
  without messages. It is the monolithic advantage that shows along the path of a call (chapter 3).
- **The user libraries are separate.** The libc (`userland/libc/`) and libsagui (`userland/libsagui/`) are program code,
  not kernel code. They translate functions like `write` into the `int 0x80` call of chapter 3. Since decision **D462**
  almost every program in `/bin` loads them as shared libraries (`/lib/libsagui.so.1`, `/lib/libc.so.1`; chapter 6), and
  only what is essential to boot and repair the system is linked statically.
- **How it is built.** The system is compiled on the PC with `make` (GCC 13 or GCC 15: decision **D464** fixed the few
  places that failed with GCC 15's C23) and tested as a QEMU guest.
- **Third-party code isolated.** Drivers taken from other projects live in `kernel/terceros/`, with their license
  recorded (decisions D034 and D073). This isolates what is not from the project, although it remains part of the kernel.
- **The kernel is documented inside the system.** The kernel guide is in `/ayuda/guias/guia-kernel.md`; the repository
  keeps it in `docs/guia-kernel.md`.

**Where things are in the repository:**

| Part | Folder | Example file |
| --- | --- | --- |
| Boot | `boot/` | `boot/mbr.asm` |
| Kernel (processes, calls) | `kernel/nucleo/` | `kernel/nucleo/processos.c` |
| Memory | `kernel/mem/` | `kernel/mem/paginacao.c` |
| File systems | `kernel/fs/` | `kernel/fs/vfs.c` |
| Drivers | `kernel/dispositivos/` | `kernel/dispositivos/ahci.c` |
| Network | `kernel/rede/` | `kernel/rede/tcp.c` |
| C and graphics libraries of user space | `userland/libc/`, `userland/libsagui/` | `userland/libc/posix.c` |
| Programs | `userland/programas/` | `userland/programas/ps.c` |

> **Relevant decision.** D157, D164, D165 and D169 (SMP): the kernel is still one program and, with several CPUs, each one
> serves its own system calls, but they **enter the kernel one at a time**, under a kernel-wide lock that hands out turns
> in order of arrival. We will see this in chapters 5 and 7.

### 2.1 Decisions, alternatives and current limits

- **Decision: a monolithic kernel in a single 32-bit binary.** It lets you read the whole system, follow a call from end
  to end (chapter 3) and avoid the cost of messages on old machines. The kernel guide explains it from its first section
  (`docs/guia-kernel.md`).
- **Limit: only i686.** The 64-bit modes are not used (kernel guide, section 1). Extending to 64 bits is in the roadmap,
  not in this version.
- **Limit: a driver failure affects everything.** SaguiOS mitigates serious failures with an exception screen
  (`kernel/arch/excecoes.c`), not with process isolation. What it does isolate is code from other projects, but by
  **license** (decisions D034 and D073), not by privilege.
- **Limit: the kernel was written for one processor.** Decision **D165** puts a kernel-wide lock in place: each CPU serves
  its calls, but only one is inside the kernel at a time (chapter 7). It is simpler and safer, at the cost of scalability.

## 3. Experiment in SaguiOS

1. Open the **Terminal** (Start > Accessories) and type `ps`. You will see the processes with their PID, state and memory.
   Each line is a **process** from the theory.
2. Type `uname` and `uptime`. They are user programs that ask the kernel for data with system calls.
3. Open the **Task manager** (Start > System > Task manager). Compare what it shows with `ps`.
4. Type `ayuda ps` to see the program's documentation inside the system itself.

![Terminal with the ps command showing system processes](img/cap-terminal-ps.png)

**What to observe:** kernel processes appear in the list next to user programs, because both are part of the same system,
but kernel processes do not use user memory.

**Additional practical exercise.** In the repository (on the PC), run `du -sh kernel/*` and sort the folders by size.
Compare `kernel/dispositivos/` with `kernel/nucleo/`. Explain why drivers take so much space in a monolithic kernel and
what that means for the reliability of the system.

## 4. Review questions

1. What is the difference between the **kernel** and a user program? Give one example of each in SaguiOS.
2. A sound driver fails and causes an exception in the kernel. What happens in a monolithic kernel and in a microkernel?
3. Why can a system be monolithic and still have isolated parts, such as `kernel/terceros/`?
4. Name two functions of an operating system that a command interpreter such as `sagsh` performs (not the kernel).

5. Using the numbers in section 1.5, what cost per process switch would make the message-based call cost the same as the
   monolithic one (0.8 µs)? Is it realistic?
6. A disk driver inside the kernel corrupts the memory used by the scheduler. List the consequences in a monolithic and in
   a microkernel design.
7. A hypervisor shares one machine between two guest systems. Give two advantages and two drawbacks compared with installing
   each system on its own machine.
8. Why does multiprogramming need memory protection while a system that runs only one program at a time does not?

## 5. Answers

1. The kernel runs with hardware privileges and manages resources (for example `kernel/nucleo/processos.c`). A user
   program (for example `ps`, in `userland/programas/ps.c`) only asks for services.
2. In a monolithic kernel, the failure happens in the privileged space and can bring the whole system down. In a
   microkernel, the driver is a process: it can be restarted without stopping the rest.
3. Isolation can be of **code and license**, not only of privilege: `kernel/terceros/` separates foreign code to manage
   its license, although it is still compiled into the kernel.
4. For example: running commands in sequence and with pipes (it uses `fork`, `execve` and `pipe`), and providing a work
   environment with variables and redirections. The interpreter uses the same calls as any program.

### Answers to the additional questions

5. Solve 2c + 0.3 = 0.8, so c = 0.25 µs per process switch. With 2 µs switches it is impossible to match: the message
   will always be more expensive unless process switches are avoided. That is why modern microkernels reduce switches and
   copy less.
6. In the monolithic design, the driver and the scheduler share one space: the whole system can crash or misbehave. In the
   microkernel, the driver is a process: its failure does not touch the scheduler (if the minimal kernel is correct) and
   the service can be restarted.
7. Advantages: the hardware is better used (several logical machines on one physical machine) and it is easy to test, copy
   or restore a system without touching the rest (this is how SaguiOS is tested in QEMU). Drawbacks: there is one more layer
   that costs performance (the guest's privileged instructions are handled by the hypervisor) and the guest does not control
   when its virtual CPU runs.
8. With a single program there is nobody to harm except the system itself; with several, one program's bug could write into
   another's memory or the kernel's. Hardware that prevents it is needed (kernel mode and paging; chapters 3 and 6).
