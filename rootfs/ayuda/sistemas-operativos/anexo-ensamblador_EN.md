<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 92; titulo: Appendix: x86 assembler -->
# Appendix: x86 assembler in SaguiOS

Assembly is the language closest to the machine: each instruction is an operation the processor executes
directly. This appendix explains what is needed to understand how SaguiOS boots, how it moves from one processor
mode to another, and how the kernel and the programs share the work. It follows the historical order of the x86
architecture: first the 16-bit real mode, then the 32-bit protected mode, then an overview of 64 bits. The 16 and
32 bits are explained in detail, with references to the real files of the system; the 64 bits only as concepts.

References have the form `file:line`, with the path from the root of the repository. The lines can be checked by
opening the file on the PC.

## 1. What assembly is and why an operating system needs it

A program in C or Pascal is translated into machine instructions by a compiler. The assembler does the same with
an almost one-to-one step: each line is a **mnemonic** (`mov`, `cli`, `int`) with its operands, and it becomes one
or several bytes. Assembly is specific to each processor family. The 32-bit x86 instructions run the same on a
Pentium II as on an Athlon or on a 7th-generation i7, which is the basis for SaguiOS running on such different
machines.

An operating system needs assembly (or, in practice, a few lines of assembly) because there are things that the C
language cannot express:

| What | Why C is not enough | Where it appears in SaguiOS |
|------|---------------------|-----------------------------|
| **Boot** | When powered on, the CPU is in real mode and there is no stack or C | `boot/mbr.asm`, `boot/estagio2.asm` |
| **Mode change** | Processor tables (GDT) must be loaded and control registers (CR0) written | `boot/estagio2.asm:294-300` |
| **Interrupts** | The processor pushes a specific frame and the code must return with `iret` | `kernel/arch/vetores.asm` |
| **Context switch** | Switching stacks and page tables between processes | `kernel/arch/vetores.asm:51-53`, `kernel/mem/paginacao.c:490-492` |
| **Privileged instructions** | Only the kernel may use `cli`, `lgdt` or write CR3 | `kernel/inc/cpu.h:81-82`, `kernel/arch/anel3_testes.asm:46` |

### Privilege rings

The x86 processor has four privilege levels, called **rings** (0 to 3). SaguiOS uses only two: ring 0 for the kernel
and ring 3 for user programs. The current privilege level (CPL) is held in the selector of the code segment. In
ring 3, the instructions that change the global state of the machine (`cli`, `lgdt`, writing CR0 or CR3) raise a
general protection exception (#GP), and the kernel ends the program that caused it. This way a program cannot turn
interrupts off or read the kernel's memory.

## 2. 16-bit real mode

When the processor is switched on, it is in **real mode**, the mode of the 8086 from 1978. It is the mode the BIOS
uses and the one in which the first sector of the disk is seen.

### Registers

- Four general registers of 16 bits that can be split into two 8-bit halves: **AX** (AH and AL), **BX**, **CX** and
  **DX**.
- Two index registers and two pointer registers: **SI**, **DI**, **BP** and **SP**. SP is the stack pointer.
- The program counter **IP**, and the flags (**FLAGS**), with bits such as CF (carry, which the BIOS uses to signal
  errors) or IF (interrupts enabled).
- Four **segment registers**: **CS** (code), **DS** (data), **SS** (stack) and **ES** (extra data).

### Segment:offset addressing

A 16-bit register reaches only 64 KB. To reach 1 MB, the processor adds two values:

    dirección física = segmento × 16 + desplazamiento

For example, `0x07C0:0x0005` is `0x07C0 × 16 + 0x0005 = 0x7C05`. Many combinations of segment and offset point to the
same physical address, and this is why the boot code sets CS to zero as soon as it can (see `boot/mbr.asm:39`: the
long jump `jmp 0x0000:.cs_zerado` leaves CS = 0 so that the addresses of `org 0x7C00` always hold).

With 20 address lines, addresses above 1 MB "wrap around" to zero on the 8086. Modern machines have a gate called
the **A20 line** that can enable or disable it; SaguiOS enables it in stage 2 (`boot/estagio2.asm`, section
`[1/6] A20`, line 132, and the method through port 0x92 on lines 413-418).

### The BIOS and its interrupts

The BIOS offers low-level services called with `int n`. They are the only "operating system" available before the
kernel. SaguiOS uses, among others:

| Interrupt | Function (AH) | Use in SaguiOS |
|-----------|---------------|----------------|
| `int 0x10` | `0x0E`: write a character in teletype mode | Loader messages (`boot/mbr.asm:112`) |
| `int 0x13` | `0x41`: are there LBA extensions? | Choosing how to read the disk (`boot/mbr.asm:52-55`) |
| `int 0x13` | `0x08`: disk geometry | CHS reading if there is no LBA (`boot/mbr.asm:60-64`) |
| `int 0x13` | `0x42`: read sectors by LBA | Reading stage 2 and the kernel (`boot/mbr.asm:150-152`) |
| `int 0x15` | `0xE820`: memory map | How much RAM there is (`boot/estagio2.asm:508-512`) |

BIOS interrupts are called in real mode and leave their results in the registers (the carry CF signals an error).
When the system moves to protected mode there is no BIOS any more, which is why the loader makes all its calls
before the jump.

### The MBR: 512 bytes and the 0x55AA signature

When booting from a disk, the BIOS reads the **first sector** (512 bytes) of the disk to address `0x7C00`, checks
that its last two bytes are `0x55 0xAA`, and jumps to it. That sector is the **MBR** (*Master Boot Record*). Since
only 512 bytes fit, the MBR can do only one thing: load the next stage, which has no such limit.

The file `boot/mbr.asm` describes it in its header (lines 10-17):

- `0x000-0x002`: short jump and NOP, the format some BIOSes expect.
- `0x003-0x059`: the BPB area (the disk geometry of a FAT) set to zero. Some BIOSes write the geometry here when
  booting from USB, which is why there is no code in that area.
- `0x05A-0x1B7`: code and messages.
- `0x1BE-0x1FD`: partition table, filled in by `tools/mkimagem.py`.
- `0x1FE-0x1FF`: the signature. In the file it is `dw 0xAA55` (line 191), and since x86 is little-endian, the bytes
  `55 AA` appear on disk.

The steps of the SaguiOS MBR, in order:

1. **Known state** (`boot/mbr.asm:31-36`): disable interrupts, set DS, ES and SS to zero and the stack to `0x7C00`,
   which grows downwards, in the free low memory.

        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00

2. **Remember the disk**: the BIOS leaves the number of the boot disk in DL; the MBR saves it in `disco`
   (`boot/mbr.asm:43`) because all the reads and stage 2 will need it.

3. **Choose the read mode**: the BIOS is asked whether it understands the LBA extensions (`AH=0x41`, line 55). If not,
   the geometry is requested (`AH=0x08`, lines 60-64) and reading will be done by cylinder, head and sector.

4. **Read the boot header** from LBA sector 1 and check its signature (lines 76-79). The header says at which sector
   stage 2 starts and how many sectors it occupies.

5. **Read stage 2 to `0x8000`** (lines 82-91) and jump to it with the disk in DL (lines 93-94):

        mov dl, [disco]                 ; la etapa 2 recibe el disco en DL, como nosotros
        jmp 0x0000:ESTAGIO2_ENDERECO

If something fails, the MBR writes a message with the BIOS and stops in a `hlt` (the label `parar`, in
`boot/mbr.asm:165-169`). The routine that writes that text (`escrever`, `boot/mbr.asm:106-115`) walks the string
byte by byte with `lodsb` and calls `int 0x10` for each character:

    escrever:
            lodsb                       ; AL = [DS:SI], SI se incrementa
            test al, al
            jz .fim
            mov ah, 0x0E
            mov bx, 0x0007
            int 0x10
            jmp escrever

### Stage 2 in real mode

`boot/estagio2.asm` has no 512-byte limit, and it does in real mode whatever the BIOS still allows: it checks the
CPU (`conferir_cpu`, which requires CPUID and CMOV), enables A20, reads the memory map with `int 0x15`, loads the
kernel and the ramdisk above 1 MB and chooses the VESA video mode with `int 0x10`. To copy above 1 MB without leaving
real mode, it uses the **unreal mode** trick (`entrar_unreal`, line 1161): it enters protected mode only for an
instant to load a 4 GB descriptor into DS and ES, and goes back to real mode. The descriptor limit stays in the
processor's cache and 32-bit addresses keep working.

## 3. Switch to 32-bit protected mode

**Protected mode** is SaguiOS's working mode. Several things change at once:

- Registers become 32 bits. The general ones are **EAX, EBX, ECX, EDX, ESI, EDI, EBP** and **ESP** (the stack). The
  program counter is **EIP** and the flags are **EFLAGS** (IF is still bit 9).
- The address of a segment stops being `segment × 16`. Now the selector is an index into a table of
  **descriptors**, and each descriptor states base, limit, type and **privilege level** (DPL).
- **Protected interrupts** can be used (an IDT table with gates that say which ring may call them) and **paging**
  (virtual memory).

### The GDT

The **global descriptor table** (GDT) is a list of 8-byte descriptors. A code or data descriptor has:

- the **base** (32 bits) and the **limit** (20 bits, in bytes or in 4 KB pages);
- the **access byte**: present bit, DPL (two bits, ring 0 to 3), type (code or data, read or write);
- the **granularity bits**: the 4 KB flag and the 32-bit mode.

The loader defines a minimal GDT with a null descriptor (mandatory), and 32-bit code and data with base 0 and a
limit of 4 GB (`boot/estagio2.asm:1787-1795`):

    gdt:
            dq 0                            ; descriptor nulo (obligatorio)
            dw 0xFFFF, 0x0000               ; 0x08 código: límite 0-15, base 0-15
            db 0x00, 0x9A, 0xCF, 0x00       ;      base 16-23, presente/anillo 0/código, 4 KB + 32 bits, base 24-31
            dw 0xFFFF, 0x0000               ; 0x10 datos
            db 0x00, 0x92, 0xCF, 0x00

The selectors are offsets inside the table: `0x08` is the ring 0 code and `0x10` the data. With base 0 and a limit of
4 GB, the **flat model** is equivalent to having no segmentation: all addresses are linear. This is how the kernel
works, and why the C code does not need to think about segments.

### The jump into protected mode

The step is done with six instructions, in `boot/estagio2.asm:294-300`:

    .entrar_pm:
            cli                             ; ninguna interrupción de la BIOS durante el cambio
            lgdt [gdt_descritor]            ; carga la GDT (dirección y tamaño)
            mov eax, cr0
            or eax, 1                       ; bit 0 = PE, protección activada
            mov cr0, eax
            jmp SEL_CODIGO:modo_protegido   ; salto largo: recarga CS con el selector de 32 bits

Two things are worth noticing:

- **The long jump is mandatory.** When PE is set, the processor still has the real-mode CS in its cache. A far jump
  with selector `0x08` reloads CS with the 32-bit code descriptor and empties the instruction queue that the
  processor had already read in real mode.
- **The other segment registers are reloaded afterwards.** `modo_protegido` (`boot/estagio2.asm:1735`) loads DS,
  ES, FS, GS and SS with `0x10` and puts the provisional stack at `0x7C00` (lines 1736-1742). Until that load, DS and
  SS still hold real-mode values.

### The A20 line and the memory map

The A20 line is not a detail of protected mode: without it, addresses from 1 MB upwards wrap around. The loader
enables it before the jump (section `[1/6]`, line 132), and if it cannot be enabled it stops with a message
(`msg_falha_a20`). The E820 memory map tells the kernel which ranges are usable RAM and which are reserved for the
BIOS or the hardware.

### From the loader to the kernel

`kernel/arch/entrada.asm` is the first instruction of the kernel. It receives in EAX the loader's signature and in EBX
the address of `boot_info`, and before any C code it does three things:

1. It sets the BSS section to zero, because the flat binary does not contain it and the C code assumes variables set
   to zero (lines 17-24: `rep stosb`).
2. It switches to its own stack, inside the BSS (line 25).
3. It calls `inicio_kernel` with the two values as arguments (lines 27-29).

### The cdecl calling convention

In 32-bit protected mode, SaguiOS (like most C of the time) uses the **cdecl** convention:

- The arguments are pushed onto the stack from right to left. That is why `push edx; push esi; call inicio_kernel`
  (`kernel/arch/entrada.asm:27-29`) leaves `boot_info` and the signature as second and first argument.
- The return value goes in **EAX** (and in EDX:EAX if it is 64 bits).
- The caller **cleans up the stack** after the call (`add esp, 4` in `kernel/arch/vetores.asm:60`).
- EBX, ESI, EDI, EBP and ESP are preserved by the called function; EAX, ECX and EDX may change.

The stack frame of a typical function looks like this (illustrative text, not code to assemble):

```text
  [ebp+12]   segundo argumento
  [ebp+8]    primer argumento
  [ebp+4]    dirección de retorno (la mete el "call")
  [ebp]      EBP anterior (la función lo guarda con "push ebp")
  [ebp-4]    primera variable local
```

The C compiler generates this on its own. What matters for reading SaguiOS's assembly is that the frame of each
interrupt has a fixed format that the C code knows (see the next section).

### Interrupts and exceptions: the IDT

The **interrupt descriptor table** (IDT) has 256 entries, one per vector. Each entry gives the address of the handler,
the code selector and the **DPL**, which decides whether a ring 3 program may call it with `int n`:

- `kernel/arch/idt.c:70` sets all the entries with `0x8E`: a 32-bit interrupt gate, DPL 0. A user program that runs
  `int 0x20`, for example, receives #GP.
- `kernel/arch/idt.c:72` sets the system call, `VETOR_SISTEMA` (`kernel/inc/cpu.h:27`, value `0x80`), with `0xEE`:
  DPL 3, so that ring 3 can use it.

When the processor receives an interrupt, it pushes the frame that the entry code must respect, and if there was a
ring change it also pushes the user's ESP and SS. The C code sees it as a structure (`quadro_t`, in
`kernel/inc/cpu.h:36-42`). Since not every exception pushes an error code, each vector has its own entry stub that
makes the frame always the same size:

- `kernel/arch/vetores.asm:27-36` is a `%rep` loop of 256 iterations that pushes a 0 if the processor did not push an
  error code (the `%if` condition, lines 30-32), pushes the vector number and jumps to the common stub.
- `entrada_comum` (lines 38-59) saves all the registers with `pushad`, loads the kernel segments and calls
  `tratar_interrupcao` (`kernel/arch/idt.c:297`).
- `retomar_quadro` (lines 63-70) restores the registers and ends with `iretd`, which pops EIP, CS and EFLAGS, and if
  the frame belongs to another ring, also ESP and SS.

### System calls with int 0x80

A user program asks the kernel for services with `int 0x80`. The call number goes in EAX and the arguments in EBX,
ECX, EDX (and ESI, EDI); the comment in `kernel/inc/abi.h:5` explains it. An example from SaguiOS
(`kernel/arch/anel3_testes.asm:19-27`) writes a text with the write call:

    mov eax, SIS_ESCREVER               ; número de la llamada
    mov ebx, 1                          ; descriptor 1 (la salida)
    lea ecx, [esi + (.msg - .aqui)]     ; dirección del texto
    mov edx, .msg_fim - .msg            ; longitud
    int 0x80                            ; el kernel toma el control

The same file tests protection (lines 42-50): `cli` in ring 3 makes the kernel end the process, and an attempt to read
`0x00100000` (kernel memory, `anel3_testes.asm:55-59`) causes a page fault, because kernel pages do not have the user
bit.

### Ring 3: how a program starts

`processo_preparar_usuario` (`kernel/nucleo/processos.c:714-728`) prepares the frame of a new program as if it came
from a ring 3 interrupt. It sets the user selectors (`SEL_UCODIGO` 0x1B and `SEL_UDADOS` 0x23,
`kernel/inc/cpu.h:11-12`), the program's stack and the entry point. EFLAGS is `0x202`: IF set and IOPL 0, so the program
cannot use `cli`, `sti` or the I/O ports. Then `retomar_quadro` runs the `iretd` and the CPU goes to ring 3 without
any special "lower the privilege" instruction.

### Paging: CR3, CR0 and CR4

Paging gives each process its own address space. The CPU controls it with three registers:

- **CR3** points to the directory table (the page tables). Changing CR3 changes the address space. This is what
  `espaco_ativar` does (`kernel/mem/paginacao.c:490-492`): `mov %0, %%cr3`.
- **CR0.PG** (bit 31) turns paging on. The SaguiOS loader turns it on in the secondary processors
  (`kernel/arch/ap_trampolim.asm:145`, `mov cr0, eax`, with the table already in CR3 from line 136).
- **CR4** enables extensions: PSE (4 MB pages, used by the kernel map), PAE (64-bit entries) and PGE (global pages).
  `ap_trampolim.asm:97` copies the CR4 of the main processor before turning paging on, because the order matters.

The context switch in the kernel brings all this together: when `tratar_interrupcao` returns the frame of another
process, `entrada_comum` loads its stack (`mov esp, eax`, `vetores.asm:53`). When changing process, the kernel calls
`espaco_ativar` with the address space of the new process (`kernel/nucleo/processos.c:603`), that is, it loads CR3.
This way an interrupt can end in another program with other memory.

### Secondary processors: from 16 to 32 bits again

The SMP mode (optional, `mkimagem.py --smp`) starts the secondary processors with an INIT-SIPI sequence. Each
secondary starts in real mode on a page that the main processor has copied to `0x7000`, and the trampoline repeats
the loader's path: local GDT, CR0.PE, long jump to the 32-bit code and then paging. It is a good example of the mode
switch being repeated on each CPU:

    protegido:                          ; kernel/arch/ap_trampolim.asm:80, ya en modo de 32 bits
            mov ax, 0x10                ; (extracto: se omiten los comentarios de las líneas 81-83)
            mov ds, ax
            mov es, ax
            mov fs, ax
            mov gs, ax
            mov ss, ax

The header of the file says that the trampoline does the minimum to reach C: the GDT, the TSS and the IDT of each CPU
are prepared afterwards in C, with paging already on (`kernel/arch/ap_trampolim.asm:16-21`).

### The UEFI loader: from 64 to 32 bits

The UEFI x64 loader (`boot/uefi/trampolim.asm`) does the opposite: the firmware leaves the CPU in long mode, and the
trampoline comes down to 32 bits for the kernel. It has two entries, `entrada64` (line 48) and `entrada32` (line 73).
In the 64-bit one, the code loads its own GDT, does a far return (`o64 retf`, line 57) to get into the compatibility
segment, turns paging off (CR0, line 65) and clears the LME and NXE bits of EFER (line 68). Section 6 explains 64 bits.

## 4. Inline assembly in C

Many parts of the kernel cannot be written in C, but there are single instructions that are worth using from C: reading
a port, querying CPUID, changing the interrupt flag. The compiler lets you write **inline** assembly with
`__asm__ volatile("...")`.

### Two syntaxes

SaguiOS assembly is written in two forms, depending on the compiler:

| | Intel (NASM, used for this appendix's exercises) | AT&T (GCC and TCC inline) |
|--|---------------------------------------------|---------------------------|
| Order | destination, source: `mov eax, ebx` | source, destination: `movl %ebx, %eax` |
| Registers | no `%`: `eax` | with `%`: `%eax` |
| Constants | no dollar sign: `mov eax, 5` | with `$`: `movl $5, %eax` |
| Size | implicit or `dword` | suffix: `movl`, `movw`, `movb` |

SaguiOS's C (kernel and programs) is compiled by GCC on the PC, and the kernel also contains `.asm` files in NASM.
That is why two syntaxes appear in the same code: the `__asm__` blocks are in AT&T and the `.asm` files in Intel.

### Operand constraints

In `__asm__`, the C operands are connected to registers with constraints. An example from `kernel/inc/portas.h`:

    static inline uint8_t porta_le8(uint16_t porta)
    {
        uint8_t v;
        __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(porta));
        return v;
    }

It means: the output `v` goes in AL (`"=a"`); the input `porta` goes in DX or is an 8-bit constant (`"Nd"`). So `inb`
reads the given port and leaves the value in `v`. `porta_escreve8` (lines 7-9) does the same with `outb`.
`volatile` stops the compiler from removing or moving the instruction; if the instruction also touches memory,
`"memory"` is added so that the compiler does not reorder the accesses.

### Examples from SaguiOS

| Instruction | What it does | Where it is used |
|-------------|--------------|------------------|
| `cpuid` | Queries the CPU's capabilities (leaf in EAX) | `kernel/arch/msr.c:105-107`, `kernel/dispositivos/sensores.c:74`, `userland/programas/simd.c:26` |
| `rdtsc` | Reads the cycle counter (TSC) into EDX:EAX | `kernel/dispositivos/ahci.c:167`, `kernel/dispositivos/sensores.c:80` |
| `in` / `out` | Read and write an I/O port | `kernel/inc/portas.h:7-15` (`outb`, `inb`) |
| `cli` / `sti` | Disable and enable interrupts | `kernel/inc/cpu.h:81-82` (`int_liga`, `int_desliga`) |
| `cli; hlt` | Halt the CPU for good | `kernel/arch/excecoes.c:34` (after a fatal fault) |
| `sti; nop; nop; cli` | Let an interrupt in through a specific window | `kernel/dispositivos/ata.c:594` |
| `rep insw` | Read a block of words from a port | `kernel/dispositivos/ata.c:317` |
| `wbinvd` | Write back and flush the CPU cache | `kernel/dispositivos/acel_radeon.c:825` |

Two examples deserve attention. `cpuid` has many leaves, and the result comes in four registers: `sensores.c:72-74`
wraps it in a function with four outputs. `ata.c:594` shows a trick: the instruction that follows `sti` runs before
an interrupt is accepted, so the `nop` leaves a window for the interrupt to come in before the next `cli`.

## 5. SIMD: MMX, SSE and AVX in concept

**SIMD** (*Single Instruction, Multiple Data*) means that one instruction works on several data items at once,
packed into a large register. Examples:

- **MMX** (nineties): 64-bit registers (MM0 to MM7) that share the x87 FPU space. Adding four 16-bit integers in one
  instruction.
- **SSE** (Pentium III, 1999): 128-bit registers (XMM0 to XMM7 in 32 bits, XMM0 to XMM15 in 64 bits). Floating-point
  operations on 4 numbers of 32 bits at a time. **SSE2** (Pentium 4, 2001) adds 128-bit integers and double-precision
  numbers.
- **AVX** (Sandy Bridge, 2011): 256-bit registers (YMM), with a new encoding prefix (VEX). It is what SaguiOS's
  process state uses when the CPU has it (see below).

To use them, the program first asks the CPU with `cpuid`. `userland/programas/simd.c` defines the bits it checks: MMX
in bit 23 of EDX of leaf 1 (`R_MMX`, line 19), FXSR in bit 24 (`R_FXSR`, line 20), SSE in bit 25 and SSE2 in bit 26
(lines 21-22). The same program tests each set with one instruction of it, in inline assembly:

- x87 with `fsqrt` (line 42);
- MMX with `paddw` and `emms` (line 52), which empties the MMX state;
- SSE with `movaps` and `mulps` (line 64);
- the SSE configuration with `ldmxcsr` (line 138).

### Where SaguiOS uses SIMD

- **Large copies and fills in the graphical interface**: `kernel/lib/sse2.c` (a 16-byte copy per instruction; the
  header comment explains why the destination is aligned). The accelerator example is `kernel/dispositivos/gpu3d_r600.c:1193`,
  which reads the card's window 16 bytes at a time: that window is uncached memory
  (`kernel/dispositivos/gpu3d_evergreen.c:775`) and each read is slow. The function `sse2_copiar` is declared in
  `kernel/inc/sse2.h:13`.
- **The FPU and SSE state per process**: each process has its own x87, MMX and XMM registers, and the kernel does not
  save them on every process change because that would be expensive. Instead, the switch is **lazy**
  (`kernel/arch/fpu.c:9-20`): the scheduler sets CR0.TS and the first floating-point instruction of the new process
  raises exception 7 (#NM); the handler saves the state of the previous owner and loads the new one.
- **Enabling SSE**: `kernel/arch/fpu.c:295` sets CR4.OSFXSR, without which the processor raises #UD on any SSE
  instruction, and `fpu.c:296` adds CR4.OSXMMEXCPT (SSE errors go to exception 19). The state is saved with `fxsave`
  (`kernel/arch/fpu.c:107`), or with `xsave` if there is AVX (D381: 832 bytes, the whole YMM registers,
  `kernel/arch/fpu.c:28-33`).
- **Kernel rules**: the kernel is compiled with `-mgeneral-regs-only` (`kernel/arch/fpu.c:18-19`), so normal C does
  not use XMM; `sse2.c` does, and that is why it is only called between `fpu_kernel_inicio()` and `fpu_kernel_fim()`,
  which save and restore the state of the owning process (`kernel/lib/sse2.c` says so in its header, lines 5-10).

That is why the FPU and SSE state belongs to each process: if the kernel did not save it, a program would see another
program's registers.

## 6. 64-bit x86-64: concepts only

Version 10 of SaguiOS will be 64-bit (improvement 106, `mejoras/106-saguios-64-bits.md`). Today the kernel and the
programs are 32-bit, and `docs/arquitecturas.md` summarizes it: 32-bit x86 is kept after v10 for PCs without 64-bit
support, and v10 adds x86-64. This section explains what changes, without going into the detail of each instruction.

### Registers and long mode

- The general registers become **64 bits** (RAX to RSP, RBP, RSI, RDI, and RIP), and **R8 to R15** are added:
  16 general registers instead of 8. An instruction prefix (REX) indicates the 64-bit size.
- The processor is in **long mode** when paging is on and the LME bit of the EFER register (MSR `0xC0000080`, bit 8)
  is set. The UEFI trampoline of section 3 does the opposite: `entrada64` (`boot/uefi/trampolim.asm:48`) turns paging
  off (line 65) and clears LME and NXE (line 68) to return to 32-bit mode.
- In long mode, segmentation is almost gone: the code and data segments are treated as flat and their limits are not
  checked. The code descriptor has an L bit that says it is 64-bit.

### Four-level paging

With 64 bits, the virtual address has 48 useful bits and paging uses four levels of tables: **PML4**, **PDPT**, **PD**
and **PT**. Each table has 512 entries of 8 bytes; the four form a chain of indexes taken from bits 39-47, 30-38,
21-29 and 12-20 of the address, and the page offset is the low 12 bits (4 KB pages). CR3 points to the PML4. There are
2 MB pages (they stop at the PD level) and 1 GB pages (at the PDPT). Intel adds a fifth level (LA57) for 57 bits, which
is not needed here.

### The System V calling convention

On Linux and on almost all 64-bit systems, the convention is **System V AMD64**: the first arguments go in registers
(RDI, RSI, RDX, RCX, R8, R9), the return value in RAX, and the stack must be aligned to 16 bytes at the call. This
changes the frame of section 3: there are no arguments on the stack any more (except those that do not fit in
registers), and local variables can live below RSP in a 128-byte area called the *red zone*, without moving RSP.

### System calls: syscall and sysret

In 32 bits, SaguiOS enters the kernel with `int 0x80`, which goes through the IDT, pushes the frame and checks
permissions. In 64 bits the instructions **syscall** and **sysret** exist, which change ring without going through the
IDT: the entry address is in an MSR (`LSTAR`), and the call's registers go in RAX (number), RDI, RSI, RDX, R10, R8 and
R9. It is faster because it does not go through the IDT. SaguiOS's `int 0x80` (`kernel/inc/abi.h:5`) will keep working
in the 32-bit version.

### What would change in SaguiOS

According to `docs/arquitecturas.md` (x86-64 rows), v10 asks for the code to be separated by architecture into
`kernel/arch/<arquitectura>/` (boot, context switch, paging, interrupts, system calls) and for our own compilers with
another target. With what has been seen above, the most visible changes are:

- the IDT has 16-byte entries, not 8, and the return from an interrupt is `iretq`;
- the boot code of the secondary processors and `entrada.asm` go through long mode, with the four-level table, and the
  frame of `vetores.asm` changes size (RSP and SS are always pushed);
- driver pointers become 64 bits, and so do the DMA and the 64-bit BARs of modern GPUs, which is the reason for
  improvement 106;
- the compiler has to generate 64-bit code (v10 is a new target, not an option).

## 7. Exercises

### Exercise 1: a boot sector in NASM (on the PC)

**Goal:** write a 512-byte MBR that shows a text with `int 0x10` and stops, and check that the assembler puts the
signature on it. It is assembled on the PC with NASM (`sudo apt install nasm`, as in `docs/CONTINUAR.md`).

Save this code as `sector.asm`:

<!-- nasm: sector.asm -->
```nasm
; sector.asm - sector de arranque de 512 bytes que escribe un texto con la BIOS.
bits 16
org 0x7C00

inicio:
        cli
        xor ax, ax
        mov ds, ax
        mov ss, ax
        mov sp, 0x7C00
        sti
        mov si, mensaje
.bucle:
        lodsb                           ; AL = [DS:SI], y SI avanza
        test al, al
        jz .fin
        mov ah, 0x0E                    ; teletipo de la BIOS
        mov bx, 0x0007
        int 0x10
        jmp .bucle
.fin:
.parar:
        hlt
        jmp .parar

mensaje: db "Hola desde NASM", 13, 10, 0

        times 510 - ($ - $$) db 0       ; relleno hasta el byte 510
        dw 0xAA55                       ; firma del MBR (bytes 55 AA en el disco)
```

Assemble it and check the result:

    nasm -f bin sector.asm -o sector.bin
    ls -l sector.bin
    od -A d -t x1 sector.bin | tail -2

The size must be 512 bytes, and the last two, `55 aa`.

**Solution (what each part does):**

- `bits 16` and `org 0x7C00` tell NASM to generate real-mode code and that the labels start at the address where the
  BIOS loads the sector.
- The first steps are those of the SaguiOS MBR: interrupts off, segments at zero, stack at `0x7C00`, and interrupts
  are enabled again with `sti`, as the SaguiOS MBR does (`boot/mbr.asm:41`).
- `lodsb` reads the byte at `[DS:SI]` and advances SI. As the text ends in 0, `test al, al` and `jz` leave the loop.
- `int 0x10` with AH = `0x0E` writes the character in AL. BL is the colour, which only matters in graphics modes.
- `times 510 - ($ - $$) db 0` pads up to byte 510, and `dw 0xAA55` writes the signature in the last two bytes. If the
  code were longer than 510 bytes, NASM would give a negative "times" error: that is how the limit is checked.

To see it run on the PC, QEMU can be used: `qemu-system-i386 -drive format=raw,file=sector.bin`.

### Exercise 2: CPUID inside SaguiOS with TCC

**Goal:** read the CPU vendor from a SaguiOS program.

SaguiOS does not ship NASM, but **TCC** (`/apps/bin/tcc`) accepts inline `__asm__` with AT&T syntax, and it can also
assemble `.s` files (TCC does the assembling itself; `userland/terceros/tcc/fuentes/libtcc.c` handles it on lines
1204-1206). Save this code as `cpuid.c` inside SaguiOS (it is also ready-made in `/ejemplos/asm/cpuid.c`), and compile or run it with `tcc -run cpuid.c`:

<!-- tcc: cpuid.c -->
```c
#include <stdio.h>

/* Lee CPUID con la hoja 0: EAX = hoja máxima, EBX/EDX/ECX = el fabricante. */
int main(void)
{
    unsigned int a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    printf("hoja maxima: %u\n", a);
    printf("ebx=%08x edx=%08x ecx=%08x\n", b, d, c);
    return 0;
}
```

**Solution:** `cpuid` with EAX = 0 returns in EAX the highest supported leaf, and the vendor name is in EBX, EDX and ECX,
in that order. That is why they are printed in that order. The constraints `"=a"`, `"=b"`... tie each register to a C
variable, and `"a"(0), "c"(0)` pass the input values.

**Status:** tested in QEMU with TCC (`tests/curso_ejemplos.py`): with `tcc -run` and compiled to an executable it prints the highest leaf and the three
registers. In QEMU without KVM it prints `GenuineIntel` (EBX = `756e6547`, EDX = `49656e69`, ECX = `6c65746e`); with KVM, the host's vendor
(on AMD, `AuthenticAMD`).

### Review questions

1. **What physical address is `0x1234:0x0010`?**
   Solution: `0x1234 × 16 + 0x0010 = 0x12340 + 0x10 = 0x12350`.

2. **Why does the SaguiOS MBR not run the full loader itself, but reads a stage 2?**
   Solution: the sector has 512 bytes and the BIOS only loads that. The MBR code has to fit in the gap between the BPB
   and the partition table (`boot/mbr.asm:10-17`), so it only knows how to read the header and stage 2, and jump to it.

3. **Why does the switch to protected mode need a far jump after setting PE?**
   Solution: when PE is set, the CPU still runs with the real-mode CS in its cache and with the instructions already
   read. The far jump reloads CS with the 32-bit code selector and empties the instruction queue.

4. **What happens if a ring 3 program runs `cli`?**
   Solution: the CPU raises #GP because `cli` is not allowed in ring 3. The kernel ends only that process
   (`kernel/arch/anel3_testes.asm:42-43` describes it).

5. **Why does `int 0x80` have DPL 3 (`0xEE`) and `int 0x20` does not?**
   Solution: the DPL of the gate says which ring may call it with `int`. System calls must be callable by programs,
   and IRQs are fired by the hardware (`kernel/arch/idt.c:66-72`).

6. **Who cleans up the stack in cdecl?**
   Solution: the caller, with an `add esp, N` after the `call` (`kernel/arch/vetores.asm:60` is an example).

7. **Why is the FPU state saved lazily?**
   Solution: saving and restoring 512 bytes on every process change is expensive, and almost no process uses the FPU.
   With CR0.TS, the first use raises #NM and only then is the switch done (`kernel/arch/fpu.c:12-16`).

8. **How many 64-bit general registers does x86-64 have, and what are they called?**
   Solution: 16: RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP and R8 to R15.

## 8. Related course examples

`rootfs/ejemplos/mico/parpadeo.asm` is an example program written in assembly, but it is **not x86**. It is the
assembler of the CHIP-8 virtual machine, assembled with `chip8asm` (the comment in its first lines says so). It serves
as a contrast: the same concept (mnemonics, labels, jumps), on another machine. The Mico editor colours that language;
the machine is explained in [the CHIP-8 guide](../chip8.md), and the editor in [the Mico guide](../mico.md).

For compiling C inside SaguiOS, see [TCC](../tcc.md) and [C in SaguiOS](../lenguajes/c.md).

## 9. Bibliography

- A. S. Tanenbaum and H. Bos, *Modern Operating Systems*. A general reference book for boot, interrupts and memory
  management, with a more theoretical approach than this appendix.
- Intel, *Intel 64 and IA-32 Architectures Software Developer's Manual*, volumes 1 to 3: the reference for the
  instructions, the GDT, the IDT, paging and the control registers.
- AMD, *AMD64 Architecture Programmer's Manual*: the same information for x86-64, with long mode and the calling
  convention.
- Documentation of the INT 13h extensions (LBA, EDD) and of the INT 15h E820 service, which the SaguiOS loader uses
  as described above.

The Intel and AMD manuals are public. This appendix does not copy text, figures or exercises from any book.
