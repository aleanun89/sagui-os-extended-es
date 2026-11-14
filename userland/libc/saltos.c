/* saltos.c - setjmp y longjmp de la libc común (D210), en ensamblador i386.
 *
 * jmp_buf: ebx, esi, edi, ebp, esp (el de quien llamó a setjmp) y la dirección
 * de vuelta. Viene del port de GNU Prolog (setjmp_gp.s, D189), que sale con
 * longjmp de los predicados que lanzan una excepción. Sin señales en el
 * SaguiOS, no hay máscara que guardar (sigsetjmp es setjmp, setjmp.h).
 */
#include "setjmp.h"

__asm__(
    ".section .text.setjmp,\"ax\",@progbits\n"
    ".globl setjmp\n"
    ".type setjmp, @function\n"
    "setjmp:\n"
    "    movl 4(%esp), %eax\n"          /* el jmp_buf */
    "    movl %ebx, 0(%eax)\n"
    "    movl %esi, 4(%eax)\n"
    "    movl %edi, 8(%eax)\n"
    "    movl %ebp, 12(%eax)\n"
    "    leal 4(%esp), %ecx\n"          /* esp tras volver de setjmp */
    "    movl %ecx, 16(%eax)\n"
    "    movl (%esp), %ecx\n"           /* la dirección de vuelta */
    "    movl %ecx, 20(%eax)\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    ".size setjmp, .-setjmp\n"
    "\n"
    ".section .text.longjmp,\"ax\",@progbits\n"
    ".globl longjmp\n"
    ".type longjmp, @function\n"
    "longjmp:\n"
    "    movl 4(%esp), %edx\n"          /* el jmp_buf */
    "    movl 8(%esp), %eax\n"          /* el valor (0 pasa a ser 1) */
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    incl %eax\n"
    "1:  movl 0(%edx), %ebx\n"
    "    movl 4(%edx), %esi\n"
    "    movl 8(%edx), %edi\n"
    "    movl 12(%edx), %ebp\n"
    "    movl 16(%edx), %esp\n"
    "    jmp *20(%edx)\n"
    ".size longjmp, .-longjmp\n"
    ".text\n");
