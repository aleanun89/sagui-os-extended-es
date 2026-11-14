/* processos_gp.c - lanzar programas desde GNU Prolog en SaguiOS (D189).
 *
 * GNU Prolog lanza procesos con fork + execvp (spawn/3, system/1, exec/5 y,
 * sobre todo, consult/1, que compila el fichero con el programa pl2wam). En
 * SaguiOS se lanza con executar(), y las redirecciones se hacen como en el
 * sagsh: se guardan los descriptores 0/1/2, se ponen los de las tuberías, se
 * lanza el hijo (que los hereda) y se restauran.
 *
 * Límite: el hijo también hereda el otro extremo de las tuberías (SaguiOS no
 * tiene «cerrar al ejecutar»): si se le da la entrada por tubería (exec/5),
 * no verá el fin de fichero aunque el padre la cierre. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "processos_gp.h"

char **Pl_M_Cmd_Line_To_Argv(char *cmd, int *argc);
int executar(const char *caminho, char *const argv[]);
int info_arquivo(const char *caminho, void *info);

/* La ruta del programa: tal cual si lleva '/', si no en el PATH y en /bin. */
static char *achar_programa(const char *nome)
{
  static char c[512];
  unsigned info[2];
  const char *path, *p;

  if (strchr(nome, '/'))
    return (char *) nome;
  path = getenv("PATH");
  if (path == NULL || *path == '\0')
    path = "/bin";
  for (p = path;;)
    {
      const char *fim = strchr(p, ':');
      size_t n = fim ? (size_t) (fim - p) : strlen(p);

      if (n == 0)
	snprintf(c, sizeof c, "./%s", nome);
      else
	snprintf(c, sizeof c, "%.*s/%s", (int) n, p, nome);
      if (info_arquivo(c, info) >= 0)
	return c;
      if (!fim)
	break;
      p = fim + 1;
    }
  snprintf(c, sizeof c, "/bin/%s", nome);
  return c;
}

/* Lanza arg[0] con los descriptores 0/1/2 que haya ahora. */
static int lancar(char *arg[])
{
  int pid;

  if (arg[1] == (char *) 1)
    arg = Pl_M_Cmd_Line_To_Argv(arg[0], NULL);
  pid = executar(achar_programa(arg[0]), arg);
  if (pid < 0)
    {
      errno = __ns_errno_de(pid);
      return -1;
    }
  return pid;
}

int
Pl_SG_Spawn(char *arg[])
{
  int pid, st;

  fflush(NULL);
  pid = lancar(arg);
  if (pid < 0)
    return -1;
  if (waitpid(pid, &st, 0) < 0)
    return -1;
  return WIFEXITED(st) ? WEXITSTATUS(st) : st;
}

/* Pone «novo» en el descriptor fd guardando el de antes en *guarda. */
static int trocar(int novo, int fd, int *guarda)
{
  *guarda = dup(fd);
  if (*guarda < 0 || dup2(novo, fd) < 0)
    return -1;
  return 0;
}

static void restaurar(int fd, int guarda)
{
  if (guarda >= 0)
    {
      dup2(guarda, fd);
      close(guarda);
    }
}

int
Pl_SG_Spawn_Redirect(char *arg[], int detach, FILE **f_in, FILE **f_out, FILE **f_err)
{
  int p_in[2] = { -1, -1 }, p_out[2] = { -1, -1 }, p_err[2] = { -1, -1 };
  int g0 = -1, g1 = -1, g2 = -1;
  int pid, erro = 0;

  fflush(NULL);
  if ((f_in && pipe(p_in)) || (f_out && pipe(p_out)) ||
      (f_err && f_err != f_out && pipe(p_err)))
    return -1;

  if (f_in && trocar(p_in[0], 0, &g0))
    erro = 1;
  if (!erro && f_out && trocar(p_out[1], 1, &g1))
    erro = 1;
  if (!erro && f_err && trocar(f_err == f_out ? p_out[1] : p_err[1], 2, &g2))
    erro = 1;
  /* el hijo ve las tuberías en 0/1/2: las copias originales de su lado se cierran */
  if (f_in)
    close(p_in[0]);
  if (f_out)
    close(p_out[1]);
  if (f_err && f_err != f_out)
    close(p_err[1]);

  pid = erro ? -1 : lancar(arg);
  erro = errno;

  restaurar(0, g0);
  restaurar(1, g1);
  restaurar(2, g2);

  if (pid < 0)
    {
      if (f_in)
	close(p_in[1]);
      if (f_out)
	close(p_out[0]);
      if (f_err && f_err != f_out)
	close(p_err[0]);
      errno = erro;
      return -1;
    }

  if (f_in && (*f_in = fdopen(p_in[1], "w")) == NULL)
    return -1;
  if (f_out && (*f_out = fdopen(p_out[0], "r")) == NULL)
    return -1;
  if (f_err && f_err != f_out && (*f_err = fdopen(p_err[0], "r")) == NULL)
    return -1;

  return detach ? 0 : pid;
}
