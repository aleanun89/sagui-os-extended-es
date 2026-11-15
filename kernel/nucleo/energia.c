// SPDX-License-Identifier: GPL-3.0-only
/* energia.c - reiniciar y apagar la máquina. */
#include "idioma.h"
#include "acpi.h"
#include "cpu.h"
#include "console.h"
#include "gui.h"
#include "montagem.h"
#include "portas.h"
#include "relogio.h"

SEM_RETORNO void maquina_reiniciar(void)
{
    klog("[SAGUI] reiniciando\n");
    int_desliga();
    /* El controlador de teclado tiene una línea conectada al reset de la CPU: el
     * comando 0xFE da un pulso en ella. */
    for (int i = 0; i < 100000 && (porta_le8(0x64) & 2); i++) ;
    porta_escreve8(0x64, 0xFE);
    for (volatile int i = 0; i < 1000000; i++) ;
    /* Si no funcionó: IDT vacía + interrupción = falta triple, que también reinicia. */
    struct EMPACOTADO { uint16_t limite; uint32_t base; } vazia = { 0, 0 };
    __asm__ volatile("lidt %0; int3" : : "m"(vazia));
    parar_maquina();
}

/* Apagado ordenado (D113). Corre en la tarea "apagar" del kernel (acpi.c):
 * graba lo pendiente de los discos y pendrives, como al reiniciar, y pide el
 * estado S5 por ACPI. Si no hay ACPI utilizable, o la máquina sigue encendida
 * 3 s después, avisa de que ya se puede apagar con el botón y se queda parada. */
SEM_RETORNO void maquina_apagar(void)
{
    klog("[SAGUI] apagando\n");
    bool i0 = int_salva_desliga();   /* como en sis_reiniciar: nadie escribe mientras tanto */
    montagem_desmontar_tudo();       /* graba lo que esté pendiente (D040) */
    int_restaura(i0);
    acpi_apagar();                   /* sólo vuelve si no se apagó */
    klog("[SAGUI] ya se puede apagar el PC\n");
    if (gui_ativa()) {
        gui_aviso(_("Ya puedes apagar el ordenador."));
        int_liga();
        relogio_dormir_ms(500);      /* el gestor de ventanas dibuja el aviso */
    } else {
        console_cor(COR_BRANCO, COR_AZUL);
        kprintf("\n Ya puedes apagar el ordenador. \n");
        console_cor(COR_CINZA, COR_PRETO);
        console_mostrar();
    }
    parar_maquina();
}
