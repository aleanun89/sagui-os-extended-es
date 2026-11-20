/* sys/wait.h - esperar a los procesos hijos (D210; waitpid de verdad desde la D401).
 *
 * Parte de la libc común (userland/libc). El estado es el de Linux: (código & 0xff) << 8 si el
 * hijo salió; el número de la señal (| 0x80 si «core») si lo terminó una señal. Un proceso que
 * terminó el kernel sin señal (matar) cuenta como SIGKILL. Las funciones, en sinais.c. */
#ifndef NS_SYS_WAIT_H
#define NS_SYS_WAIT_H

#include <sys/types.h>
#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 8
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) != 0 && WTERMSIG(s) != 0x7f)
#define WCOREDUMP(s)   (((s) & 0x80) != 0)
#define WIFSTOPPED(s)  0
#define WSTOPSIG(s)    0
#define WIFCONTINUED(s) 0
pid_t waitpid(pid_t pid, int *estado, int opcoes);
pid_t wait(int *estado);
#endif
