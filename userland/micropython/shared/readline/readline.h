/* readline.h - substitui o shared/readline do MicroPython: o input() do
 * SaguiOS lê uma linha pelo modo de linha do terminal (mp_sagui.c). */
#ifndef SAGUI_MP_READLINE_H
#define SAGUI_MP_READLINE_H
#include "py/misc.h"
#define CHAR_CTRL_C (3)
#define CHAR_CTRL_D (4)
int sagui_readline(vstr_t *linha, const char *prompt);
#endif
