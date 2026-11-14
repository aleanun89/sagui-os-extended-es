/* sys/wait.h - esperar a los procesos hijos (D210).
 *
 * Parte de la libc común (userland/libc). El estado va como en Linux
 * (código << 8) para que valgan las macros de siempre; un proceso que mató el
 * kernel o kill() sale como la señal 9. Las funciones, en posix.c. */
#ifndef NS_SYS_WAIT_H
#define NS_SYS_WAIT_H

#include <sys/types.h>
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s)   (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFSTOPPED(s)  0
pid_t waitpid(pid_t pid, int *estado, int opcoes);
pid_t wait(int *estado);
#endif
