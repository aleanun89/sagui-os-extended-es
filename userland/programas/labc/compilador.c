/* SPDX-License-Identifier: GPL-3.0-only */
/* compilador.c - libtcc en memoria y ejecución en un hijo (D414). Ver compilador.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <libtcc.h>
#include "compilador.h"

extern char **environ;

typedef int (*func_main_t)(int, char **, char **);
static func_main_t programa;       /* lo último compilado (main o principal) */
static TCCState *estado;           /* se mantiene hasta lanzarlo: el código vive en su memoria */

typedef struct { lab_error_t *errs; int max, n; int errores; } ctx_t;

/* TCC avisa con «<string>:12: error: ...», «<string>:3: warning: ...» o «tcc: error: ...». */
static void al_error(void *opaco, const char *msg)
{
    ctx_t *c = opaco;
    int linea = 0, aviso = 0;
    const char *p = msg;
    if (strncmp(p, "<string>:", 9) == 0) {
        p += 9;
        while (*p >= '0' && *p <= '9') linea = linea * 10 + (*p++ - '0');
        if (*p == ':') p++;
        while (*p == ' ') p++;
    } else if (strncmp(p, "tcc: ", 5) == 0) {
        p += 5;
    }
    if (strncmp(p, "warning: ", 9) == 0) { aviso = 1; p += 9; }
    else if (strncmp(p, "error: ", 7) == 0) p += 7;
    if (!aviso) c->errores++;
    if (c->n < c->max) {
        lab_error_t *e = &c->errs[c->n++];
        e->linea = linea;
        e->aviso = aviso;
        snprintf(e->msg, sizeof e->msg, "%s", p);
    }
}

static int ahora_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

int lab_compilar(const char *fuente, int limites, lab_error_t *errs, int max, int *n, int *ms)
{
    ctx_t c = { errs, max, 0, 0 };
    int t0 = ahora_ms();
    if (estado) { tcc_delete(estado); estado = NULL; }
    programa = NULL;
    *n = 0;
    TCCState *s = tcc_new();
    if (!s) {
        snprintf(errs[0].msg, LAB_MSG_MAX, "no se ha podido crear el compilador");
        errs[0].linea = 0; errs[0].aviso = 0; *n = 1;
        return -1;
    }
    tcc_set_error_func(s, &c, al_error);
    if (limites) tcc_set_options(s, "-b -g");
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    int r = tcc_compile_string(s, fuente);
    if (r == 0 && c.errores == 0) r = tcc_relocate(s);
    if (r < 0 || c.errores) {
        if (c.errores == 0 && c.n < c.max) {
            c.errs[c.n].linea = 0; c.errs[c.n].aviso = 0;
            snprintf(c.errs[c.n++].msg, LAB_MSG_MAX, "no se ha podido compilar");
        }
        tcc_delete(s);
        *n = c.n;
        *ms = ahora_ms() - t0;
        return -1;
    }
    programa = (func_main_t)tcc_get_symbol(s, "main");
    if (!programa) programa = (func_main_t)tcc_get_symbol(s, "principal");
    if (!programa) {
        if (c.n < c.max) {
            c.errs[c.n].linea = 0; c.errs[c.n].aviso = 0;
            snprintf(c.errs[c.n++].msg, LAB_MSG_MAX, "el programa no tiene la función main");
        }
        tcc_delete(s);
        *n = c.n;
        *ms = ahora_ms() - t0;
        return -1;
    }
    estado = s;
    *n = c.n;
    *ms = ahora_ms() - t0;
    return 0;
}

static int fd_sal = -1, fd_ent = -1;

int lab_lanzar(void)
{
    int sal[2], ent[2];
    if (!programa) return -1;
    if (pipe(sal) < 0) return -1;
    if (pipe(ent) < 0) { close(sal[0]); close(sal[1]); return -1; }
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        close(sal[0]); close(sal[1]); close(ent[0]); close(ent[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(sal[1], 1);
        dup2(sal[1], 2);
        dup2(ent[0], 0);
        close(sal[0]); close(sal[1]); close(ent[0]); close(ent[1]);
        setvbuf(stdout, NULL, _IOLBF, 0);
        char *argv[] = { "programa", NULL };
        int r = programa(1, argv, environ);
        fflush(NULL);
        _exit(r);
    }
    close(sal[1]);
    close(ent[0]);
    fcntl(sal[0], F_SETFL, fcntl(sal[0], F_GETFL) | O_NONBLOCK);
    fd_sal = sal[0];
    fd_ent = ent[1];
    /* el código ya está copiado en el hijo: el padre suelta el suyo */
    tcc_delete(estado);
    estado = NULL;
    programa = NULL;
    return pid;
}

int lab_estado(int pid, int *codigo)
{
    int st = 0;
    pid_t r = waitpid(pid, &st, WNOHANG);
    if (r == 0) return 0;
    if (r < 0) { *codigo = 0; return 1; }
    if (WIFSIGNALED(st)) { *codigo = WTERMSIG(st); return 2; }
    *codigo = WEXITSTATUS(st);
    return 1;
}

void lab_detener(int pid)
{
    kill(pid, SIGKILL);
}

int lab_leer(char *b, int n)
{
    if (fd_sal < 0) return 0;
    int r = (int)read(fd_sal, b, (size_t)n);
    return r > 0 ? r : 0;
}

void lab_enviar(const char *s, int n)
{
    if (fd_ent >= 0) write(fd_ent, s, (size_t)n);
}

void lab_cerrar(void)
{
    if (fd_sal >= 0) close(fd_sal);
    if (fd_ent >= 0) close(fd_ent);
    fd_sal = fd_ent = -1;
}
