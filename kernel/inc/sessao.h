// SPDX-License-Identifier: GPL-2.0-or-later
/* sessao.h - inicio de sesión gráfico, cerrar sesión y cambiar de usuario (D340, mejora 098, fase 3).
 *
 * La pantalla de inicio de sesión la pinta el gestor de ventanas (como el diálogo de permisos, D269):
 * ningún programa ve la contraseña. Si solo hay una cuenta de persona y no tiene contraseña, se entra
 * directo, como antes. La contraseña la comprueba el gestor de servicios (D255); una cuenta sin
 * contraseña entra sin preguntarle. */
#ifndef SAGUI_SESSAO_H
#define SAGUI_SESSAO_H
#include "tipos.h"
#include "desenho.h"

/* principal.c, con el escritorio ya en pantalla. true = se entra directo (una sola cuenta sin
 * contraseña, imagen de prueba o modo rescate): la sesión ya está puesta. false = la pantalla de inicio
 * de sesión está abierta y la sesión empezará cuando alguien entre. */
bool sessao_arrancar(void);
/* El proceso 0 (principal.c), en su espera. inicio: empezó una sesión (aplicar su configuración y abrir
 * su Terminal); restaurar: se cerró una (volver a la configuración de la máquina detrás de la pantalla). */
bool sessao_tomar_inicio(void);
bool sessao_tomar_restaurar(void);
/* USU_CERRAR_SESION: cierra todos los programas de la sesión local y abre la pantalla. */
int  sessao_pedir_fim(int modo, int pid);

/* Gestor de ventanas: la pantalla es modal y va por encima de todo. */
bool sessao_ativa(void);
void sessao_tique(void);
void sessao_desenhar(superficie_t *t);
bool sessao_mouse(int x, int y, int botoes, int mudaram);
bool sessao_tecla(int tecla);

#endif
