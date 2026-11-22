<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 12; titulo: Security, power and shutdown -->
# Security, power and shutdown

[Español](12-seguridad-energia.md) · [Português](12-seguridad-energia_PT.md) · [English](12-seguridad-energia_EN.md)

> Goal: understand what an operating system protects and against what, how users and permissions are used, what role ACPI plays in
> power management, and why shutting a system down well is a problem of order and deadlines.

## 1. Theory

### 1.1 What security means in an operating system

Security is not a single function. It rests on three ideas:

- **Identity**: knowing who does each thing. Each process and each file has an owner (user and group).
- **Authorization**: deciding what that identity may do. These are file permissions and privileged calls.
- **Isolation**: so that a fault or a malicious program cannot touch what does not belong to it. The separate address space (chapter 6)
  and kernel mode (chapter 3) are the foundation.

The **principle of least privilege** says each process should have only the permissions it needs. The administrator (`root`) is the
extreme case: it can do almost everything, and that is why it is used with care.

### 1.2 Users and permissions

Unix and its descendants use `uid` and `gid` to identify each process and each file. The administrator is uid 0. File permissions are
read, write and execute for the owner, the group and the rest. Other models, such as the access control lists (ACL) of Windows, give
finer permissions at the cost of complexity.

### 1.3 Privileged calls

Not all calls are open to everyone: changing the time, rebooting the machine or mounting disks are operations the kernel allows only to
an administrator. The kernel checks it in the call itself, before carrying it out.

### 1.4 Power and ACPI

**ACPI** (*advanced configuration and power interface*) is the specification by which the firmware describes the board: power sources,
sensors, shutdown, reboot and batteries. The operating system runs methods of an internal language called **AML** to use them. A correct
shutdown uses the **S5** state (power off).

### 1.5 Shutting down in order

Shutting down badly can lose data: files half-written in the disk cache, a service that has not closed its database, a processor still
running code while power falls. An orderly shutdown follows these steps:

1. Ask programs and services to finish, with a deadline.
2. Flush the disk cache and unmount the file systems.
3. Stop the other processors and the drivers, in the reverse order of boot.
4. Power off or reboot with the most reliable mechanism available.

Each step needs a **deadline**: if a step hangs, the system cannot wait forever. The failure is noted and the next step proceeds.

### 1.6 Alternatives

- Linux: `systemd` coordinates the shutdown of services and the `shutdown` command.
- Windows: fast startup keeps part of the state on disk (hybrid hibernation).
- BSD: `shutdown` and the `rc` manager stop the services in order.

### 1.7 Calculation: the worst-case deadline of a reboot

The shutdown of SaguiOS (decision D450) has bounded deadlines. Adding the worst case of each step with a fixed deadline, with one NVMe
device, a reboot takes at most: services 20 s + ACPI 3 s + processors 0.1 s + NVMe 5 s + `RESET_REG` 1 s + 8042 0.5 s + `0xCF9` 0.5 s =
**30.1 s**. The disk deadlines are left out, since they depend on the disk's response (they are measured with the TSC clock). With two NVMe
devices, the worst case rises by 5 s more. The bounded deadline is what stops the system from hanging forever: without one, a blocked step
would delay the shutdown indefinitely.

### 1.8 Comparison: shutdown in other systems

- **Linux** with `systemd` for services: each unit has a stop timeout (90 s by default) and is then killed.
- **Windows** gives each service time to close (5 s by default in the registry) and then forces termination.
- **FreeBSD** uses `shutdown` and the `rc` script system, which stops services in the reverse order of start.
- All share the idea: each step has a maximum time, and the system goes on even if a service does not respond.

### 1.9 Passwords: hash, salt and iterations

A system must not store passwords, but their **fingerprint** (*hash*): a one-way function. On login the hash of what was typed is
computed and compared. Two more defenses: the **salt** (a random value different for each account, which forces attacking each
account separately and makes precomputed tables useless) and **iterations** (repeating the computation many times so that each
guess is expensive). Calculation: an 8-character password of 62 symbols has 62^8 approx. 2.2 × 10^14 possibilities. With a simple hash at
10^9 guesses per second it is exhausted in about 2.5 days; with 20,000 iterations each guess costs 20,000 times more (5 × 10^4 per
second) and it takes about **140 years** (illustrative figures). The system can also slow down whoever guesses online (a wait after
each failure and a lockout after several).

### 1.10 Protection domains: ACLs and capabilities

Protection can be seen as a **matrix** of domains (users or processes) by objects (files, devices), with the rights in each cell.
Storing it by **columns** gives an **access control list** (ACL): each object says who can do what; it easily answers "who can
read this file?". Storing it by **rows** gives **capabilities**: each process carries a list of what it can use, like an open file
descriptor; it answers "what can this process touch?" and is easily passed between processes. Unix permissions (`rwx` for owner,
group and others) are an ACL reduced to three entries; an open file descriptor is a capability.

### 1.11 Principles of secure design

- **Least privilege**: each process only with what it needs.
- **Fail-safe defaults**: what is not expressly allowed is denied.
- **Complete mediation**: every access is checked, not just the first.
- **Economy of mechanism**: as small and simple as possible, so that it can be reviewed entirely.
- **Open design**: security does not depend on the attacker not knowing how it works (it does depend on not knowing the key).
- **Psychological acceptability**: if it is annoying, people turn it off.

### 1.12 Typical attacks and defenses

- **Buffer overflow.** A program copies data without checking the size and overwrites what lies behind, such as the return address
  on the stack. Defenses: checking sizes, non-executable data pages (**NX**, chapter 6), a guard page next to the stack and
  randomizing addresses (**ASLR**). The 1988 Internet worm spread, among other things, with a buffer overflow in a network service.
- **Malware.** A **virus** attaches itself to another program; a **worm** copies itself over the network; a **trojan** is a program
  that looks useful but does something else. Defense: least privilege, and what runs on behalf of a user must not be able to
  change the system.
- **Covert channels.** Two processes that cannot talk can communicate through something they do share, such as the time each one
  takes to get the CPU. They are very hard to close.

### 1.13 Energy: CPU states

The CPU spends energy in two ways that can be tuned. With **performance states** (P-states) the frequency *f* and the voltage *V*
are lowered together: dynamic power is proportional to *C* × *V*² × *f*. With **idle states** (C-states) parts of the chip are
switched off when there is no work; the simplest is the `hlt` instruction, which stops the CPU until the next interrupt. Lowering
the frequency by 25 % and the voltage by 15 % leaves the power at 0.75 × 0.85² approx. **0.54**: a 46 % saving, while the work takes a
third longer.

## 2. How SaguiOS does it

- **Users and permissions.** `kernel/nucleo/usuarios.c` (which user each process belongs to, its uid and gid) and `kernel/nucleo/permisos.c`
  (which calls each one may make), with decisions D255 (privileged calls) and D282 (owner, group and `rwx`). The accounts and the administrator
  have their design in decision D371 (`/root`, own folders and system configuration apart from the person's).
- **Session lock.** `kernel/gui/sessao.c` manages login and logout; `kernel/gui/ocio.c` the lock and the screensaver. The taskbar menu
  (`kernel/gui/barra.c`) offers "Bloquear" (lock) when there are accounts.
- **Isolation.** Each process has its own address space (chapter 6), calls validate the program's pointers with `usuario_ok` (chapter 3),
  and permissions are checked on each `open`.
- **Passwords.** Accounts store a **PBKDF2-SHA256** hash with a 16-byte salt and 20,000 iterations (`userland/inc/pbkdf2.h`); it
  is checked by `login` and the service manager, and only root can read the shadow file. After a failure the answer takes 2 s,
  and after five in a row from the same user there is a one-minute lockout.
- **Elevation without `setuid`.** `admin` and `su` are not programs with privileges of their own: they ask the service manager for
  the elevation (decision D255), which looks at the shadow file and, if it grants it, the kernel changes the identity of the
  process that asked.
- **Closed by default.** In `kernel/nucleo/permisos.c`, a system call that nobody has classified is left **for root only**:
  forgetting to classify it denies, it does not allow.
- **System programs and dynamic linking.** A `/bin` program is loaded with `AT_SECURE` (decision D378): it does not read the
  `SAGUI_LIB_CAMINHO` variable and only loads libraries owned by root without write permission for others, so that nobody
  changes what runs with privileges.
- **CPU energy.** `kernel/dispositivos/frequencia.c` chooses among the P-states the BIOS declared (decision D106, and per CPU with
  SMP in D175), with a temperature limit of 79 °C; an idle CPU sleeps with `hlt` (chapter 7).
- **Services.** `kernel/nucleo/supervisor.c` starts, restarts and stops services (decision D216). The `servicios` command shows them.
- **ACPI.** `kernel/dispositivos/uacpi_sagui.c` is the kernel's layer for the uACPI library (third-party code, MIT, in
  `kernel/terceros/uacpi/`), and `kernel/dispositivos/acpi.c` does the S5 shutdown (decision D113). `bateria.c` reads the battery state.
- **Orderly shutdown and reboot.** `kernel/nucleo/energia.c` (decision D450) has a single path to reboot and to shut down, in the kernel's
  shutdown task. Its fixed order is:
  1. start;
  2. services (at most 20 s, in `supervisor.c`);
  3. disks: write what is pending and unmount (each disk command has its deadline);
  4. ACPI: `_TTS(5)` and, when shutting down, `_PTS(5)` (at most 3 s); then interrupts are closed;
  5. secondary processors stopped (at most 100 ms) and drivers shut down in the reverse order of boot;
  6. reboot through the FADT `RESET_REG` (1 s), then through the keyboard controller, port 0xCF9 and, if everything fails, a triple fault;
     power off through `SLP_TYP|SLP_EN` of `\_S5_` (3 s).

  Each step leaves a mark in the serial log and in a trace that survives a warm reboot.
- **Commands.** `apagar-pc` and `reiniciar` are internal commands of the shell (`userland/sagsh/internos.c`).

### 2.1 Decisions, alternatives and current limits

- **Decision: users and privileged calls.** Decision **D255** reserves to an administrator the calls that change the system; decision
  **D282** gives file permissions by owner, group and `rwx`.
- **Decision: accounts and `/root`.** Decision **D371** separates system configuration from each person's.
- **Decision: supervised services.** Decision **D216** starts, watches and restarts services (`kernel/nucleo/supervisor.c`).
- **Decision: a single shutdown path with order and deadlines.** Decision **D450** (`kernel/nucleo/energia.c`).
- **Limit: isolation is that of memory and of calls.** A program cannot touch another's memory, but there are no jails or containers:
  an administrator user has access to everything.
- **Limit: the deadlines are fixed in the code.** Changing them requires recompiling; there is no configuration file for them.

![Event viewer with the shutdown and power marks](img/cap-visorlogs.png)

## 3. Experiment in SaguiOS

1. Type `whoami` and `id`. Compare the uid and groups of your user with those of `root`.
2. Type `ls -l /ayuda` and observe the permissions of the files. Try `cat` on a file you have no read permission for, if you have one, and
   observe the error.
3. Type `servicios` and look at the services the supervisor keeps alive.
4. Open the **Visor de registros** (Event viewer, Spanish name in the menu), choose the category **Energía** (Energy) and search for `apagado`
   or `reinicio`. If the log keeps marks of an earlier shutdown, you will see them in that category.
5. **Do not run `apagar-pc` or `reiniciar` for this experiment**: they are the commands that close the system. Read them in `energia.c` and
   in decision D450, and compare the order with the theory.

**Additional practical exercise.** Read the header of `kernel/nucleo/energia.c` and draw in your own words the sequence of shutdown steps, with
the deadline of each. Then open the Event viewer, choose the Energy category and explain what each mark means if there is one. Do not run
`apagar-pc` to do it.

## 4. Review questions

1. What is the principle of least privilege? Give an example of a service that meets it and one that does not.
2. Why must the shutdown unmount the disks before cutting power, and what happens if it does not?
3. Why does each step of the shutdown have a deadline? What problem does it prevent?
4. A network driver takes 30 seconds to close during shutdown. What must the system do according to SaguiOS's order?
5. If a shutdown step takes 40 s and has no deadline, what happens to the system? And with a 5 s deadline?
6. Why can a user program not read another user's password even if it knows its memory address?
7. What is the difference between an ACPI shutdown (S5) and a reboot through the keyboard controller?
8. A 6-lowercase-letter password is tried at 5 × 10^4 guesses per second (with iterations). How long does it take to exhaust the
   search space?
9. What is the difference between an ACL and a capability? Which one answers "who can read this file?" better?
10. If the frequency drops by 50 % and the voltage by 20 %, what fraction of the dynamic power is left?
11. Why does `permisos.c` leave system calls that nobody has classified closed?

## 5. Answers

1. A process should have only the permissions it needs. A printing service that only writes to its queue meets the principle. A program
   that always runs as administrator does not, even if that is convenient.
2. The disk cache holds data not yet written. If power is cut before flushing it, the data is lost or inconsistent. Unmounting forces the
   write and leaves the file system in a clean state.
3. Because a step can hang (an unresponsive disk, a stuck driver). Without a deadline, the shutdown would wait forever. With one, the
   failure is noted and the system goes on.
4. According to D450, each step has a deadline: when it passes, the failure is noted and the shutdown continues with the next step. The
   driver does not block the rest of the shutdown, and the log keeps the mark of the step that failed.
5. Without a deadline, the shutdown waits 40 s or more, and if the step hangs, it waits forever. With a 5 s deadline, the failure is
   noted at 5 s and the shutdown continues with the next step.
6. Because each process has a different address space: its address does not exist in the other's space. In addition, the kernel validates
   the pointers of each call (`usuario_ok`).
7. An ACPI shutdown asks the firmware to enter the S5 (off) state; a reboot through the keyboard controller (0xFE on the 8042) asks the
   keyboard controller to reset the processor. They are different mechanisms: one turns off, the other restarts.
8. 26^6 = 308,915,776 possibilities / 50,000 per second approx. 6178 s, that is, **1.7 hours**.
9. An ACL lives on each object and says who can use it (it answers "who can read this file?" better); a capability is carried by
   each process and says what it can use (it answers "what can this process touch?").
10. 0.5 × 0.8² = 0.5 × 0.64 = **0.32**: 32 % is left (68 % is saved).
11. By the fail-safe defaults principle: if someone adds a call and forgets to classify it, what happens is that nobody except root
    can use it (a visible failure with no risk), rather than everybody being able to use it without noticing.
