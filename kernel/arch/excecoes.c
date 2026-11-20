// SPDX-License-Identifier: GPL-3.0-only
/* excecoes.c - la pantalla de error de SaguiOS.
 *
 * Cuando el kernel comete un error que la CPU detecta (dividir por cero, acceder
 * a memoria que no existe...), no hay forma de seguir con seguridad. La pantalla
 * muestra el nombre de la excepción en portugués y el estado de los registros, que es lo
 * que permite encontrar la causa a partir de una foto. */
#include "rastro.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "console.h"
#include "processo.h"
#include "abi.h"

static const char *const nomes[32] = {
    "División por cero", "Depuración", "Interrupción no enmascarable", "Punto de interrupción",
    "Desbordamiento (INTO)", "Índice fuera de límites (BOUND)", "Instrucción no válida", "Coprocesador no disponible",
    "Doble fallo", "Segmento del coprocesador", "TSS no válida", "Segmento ausente",
    "Fallo de pila", "Protección general", "Fallo de página", "Reservada (15)",
    "Error de coma flotante (x87)", "Comprobación de alineación", "Comprobación de máquina", "Error de coma flotante (SIMD)",
    "Virtualización", "Protección de control", "Reservada (22)", "Reservada (23)",
    "Reservada (24)", "Reservada (25)", "Reservada (26)", "Reservada (27)",
    "Inyección del hipervisor", "Comunicación VMM", "Seguridad", "Reservada (31)",
};

const char *nome_excecao(uint32_t vetor)
{
    return vetor < 32 ? nomes[vetor] : "Interrupción desconocida";
}

SEM_RETORNO void parar_maquina(void)
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

SEM_RETORNO void excecao_fatal(quadro_t *q)
{
    int_desliga();
    uint32_t cr2 = le_cr2();
    rastro_excecao(q->vetor, q->erro, q->eip);   /* depuración: sobrevive a un reinicio en caliente (rastro.h) */
    /* Primero el log (D164): si la pantalla se queda a medias (otra CPU
     * dibujando), la línea del puerto serie ya ha salido. */
    klog("\n[SAGUI] EXCECAO %u (%s) erro=%x eip=%x cr2=%x cpu=%u\n", q->vetor, nome_excecao(q->vetor),
         q->erro, q->eip, cr2, cpu_atual());
    console_esconder(false);   /* la pantalla de error aparece incluso con el escritorio activo */

    console_cor(COR_BRANCO, COR_VERMELHO);
    console_limpar();
    kprintf("\n  SaguiOS ha encontrado un error y ha tenido que detenerse.\n\n");
    console_cor(COR_AMARELO, COR_VERMELHO);
    kprintf("  Excepción %u: %s\n", q->vetor, nome_excecao(q->vetor));
    console_cor(COR_BRANCO, COR_VERMELHO);
    kprintf("  Código de error: 0x%08x\n\n", q->erro);
    kprintf("  EIP=%08x  CS=%04x  EFLAGS=%08x\n", q->eip, q->cs, q->eflags);
    kprintf("  EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n", q->eax, q->ebx, q->ecx, q->edx);
    kprintf("  ESI=%08x  EDI=%08x  EBP=%08x  ESP=%08x\n", q->esi, q->edi, q->ebp, q->esp_kernel + 20);
    kprintf("  DS=%04x  ES=%04x  FS=%04x  GS=%04x\n", q->ds, q->es, q->fs, q->gs);
    kprintf("  CR0=%08x  CR2=%08x  CR3=%08x\n", le_cr0(), cr2, le_cr3());
    if (q->vetor == 14) {
        /* Bits del código de error del fallo de página (manual de Intel):
         * 0 = página presente (violación de protección) o ausente,
         * 1 = escritura o lectura, 2 = en modo usuario o kernel. */
        kprintf("\n  Fallo de página: %s en la dirección 0x%08x,\n",
                (q->erro & 0x10) ? "ejecución (NX)" : q->erro & 2 ? "escritura" : "lectura", cr2);
        kprintf("  %s, en modo %s.\n", q->erro & 1 ? "página protegida" : "página no asignada",
                q->erro & 4 ? "usuario" : "kernel");
    }
    kprintf("\n  Haz una foto de esta pantalla para ayudar a encontrar el problema.\n");
    kprintf("  Apaga o reinicia el ordenador.\n");
    parar_maquina();
}

/* Error de un programa en el anillo 3: solo ese proceso termina. Es la protección
 * en acción: el kernel y los demás programas siguen normalmente. */
quadro_t *excecao_usuario(quadro_t *q)
{
    char motivo[96];
    snformatar(motivo, sizeof motivo, "%s en EIP=0x%08x", nome_excecao(q->vetor), q->eip);
    if (q->vetor == 14 && (q->erro & 0x11) == 0x11) {
        /* D349: bit 4 (I/D) = al buscar una instrucción, y la página estaba
         * presente: es NX (pila, montón o datos que no son código). */
        snformatar(motivo, sizeof motivo, "%s: ejecutar memoria no ejecutable (NX) en 0x%08x", nome_excecao(14), le_cr2());
        klog("[SAGUI] NX: proceso %d (%s) intento ejecutar 0x%08x (pila o datos)\n", processo_recursos()->pid,
             processo_atual()->nome, le_cr2());
    } else if (q->vetor == 14)
        snformatar(motivo, sizeof motivo, "%s: %s 0x%08x", nome_excecao(14),
                   q->erro & 2 ? "escritura" : "lectura", le_cr2());
    /* D401: el fallo pasa a ser una señal síncrona del hilo que falló. Sin tratador (o bloqueada,
     * o ignorada), el proceso termina como siempre, con este mismo mensaje. */
    int sinal = SINAL_SEGV, codigo = SINAL_SI_KERNEL;
    uint32_t endereco = q->eip;
    switch (q->vetor) {
    case 0:  sinal = SINAL_FPE; codigo = SINAL_FPE_INTDIV; break;
    case 16: case 19: sinal = SINAL_FPE; codigo = SINAL_FPE_FLTINV; break;
    case 6:  sinal = SINAL_ILL; codigo = SINAL_ILL_ILLOPC; break;
    case 1:  case 3: sinal = SINAL_TRAP; codigo = SINAL_TRAP_BRKPT; break;
    case 17: sinal = SINAL_BUS; codigo = SINAL_BUS_ADRALN; break;
    case 14: codigo = (q->erro & 1) ? SINAL_SEGV_ACCERR : SINAL_SEGV_MAPERR; endereco = le_cr2(); break;
    case 13: endereco = 0; break;
    }
    return sinais_excecao(q, sinal, codigo, endereco, motivo);
}
