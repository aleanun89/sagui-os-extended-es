// SPDX-License-Identifier: GPL-2.0-or-later
/* dialogo-admin - ayudante de tests/h269_dialogo_admin.py (D269, issue #16).
 *
 *   dialogo-admin --pedir     pide ser administrador con el diálogo de permisos del sistema y anota
 *                             el resultado y el uid en el registro («dialogo: pedir r=0 uid=0»)
 *   dialogo-admin --avisos    dos avisos seguidos, el primero largo (varias líneas)
 *   dialogo-admin --mensaje   un mensaje y una pregunta con texto largo (ventanas que crecen)
 * Todo va al registro del sistema con «dialogo:». */
#include "sagui.h"
#include "gui.h"
#include "contas.h"

#define LARGO "Este es un aviso muy largo para comprobar que el texto sale completo y no se corta: "      \
              "primero hay que partirlo por palabras según el ancho real de la letra, después la caja crece " \
              "hacia abajo y al final aparece la palabra FINALDELAVISO."

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--pedir") == 0) {
        int r = elevar_grafico("probar el diálogo de permisos de la prueba h269");
        registrar("dialogo: pedir r=%d uid=%u", r, meu_uid());
        return r == 0 ? 0 : 1;
    }
    if (argc >= 2 && compara_texto(argv[1], "--avisos") == 0) {
        aviso_tela(LARGO, "");
        aviso_tela("Segundo aviso: SEGUNDOVISIBLE", "");
        registrar("dialogo: avisos enviados");
        dormir(500);
        return 0;
    }
    if (argc >= 2 && compara_texto(argv[1], "--mensaje") == 0) {
        mensagem("Mensaje largo", "Este mensaje tiene una línea larguísima que antes se salía por la derecha de la ventana "
                 "y se perdía: ahora la ventana crece y el texto se parte por palabras hasta FINALDELMENSAJE.");
        registrar("dialogo: mensaje cerrado");
        bool r = confirmar("Pregunta larga", "¿Seguro que quieres seguir? Esta pregunta también es larga y tiene que verse "
                           "entera, hasta la última palabra FINALDELAPREGUNTA, sin salirse de la ventana.");
        registrar("dialogo: confirmar %d", r);
        return 0;
    }
    imprimir("uso: dialogo-admin --pedir | --avisos | --mensaje\n");
    return 1;
}
