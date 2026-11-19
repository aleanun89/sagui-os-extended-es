// SPDX-License-Identifier: GPL-3.0-only
/* proceso.c - git_process de libgit2 sobre los procesos de SaguiOS (D400).
 *
 * Sustituye a src/util/unix/process.c (que usa fork/exec). Lo usa el transporte SSH «exec» de
 * libgit2 (transports/ssh_exec.c): lanza /bin/ssh con la entrada y la salida por tuberías
 * (ponte.c) y habla el protocolo de git por ellas. La línea de órdenes se parte como un shell
 * mínimo (comillas dobles y simples, \ para escapar). La salida de errores (2) la hereda. */
#include <stdio.h>
#include <git2.h>

#include "git2_util.h"
#include "vector.h"
#include "process.h"
#include "fs_path.h"

int sg_lanzar(const char *ruta, char *const argv[], int *entrada, int *salida);
int sg_leer(int fd, void *buf, unsigned n);
int sg_escribir(int fd, const void *buf, unsigned n);
int sg_cerrar(int fd);
int sg_esperar(int pid, int *codigo);

struct git_process {
	char **args;
	size_t nargs;
	char *cwd;

	unsigned int capture_in  : 1,
	             capture_out : 1,
	             capture_err : 1;

	int pid;
	int child_in;
	int child_out;
	int status;
	int exitcode;
};

static void liberar_args(char **a, size_t n)
{
	for (size_t i = 0; a && i < n; i++)
		git__free(a[i]);
	git__free(a);
}

int git_process_new(
	git_process **out,
	const char **args,
	size_t args_len,
	const char **env,
	size_t env_len,
	git_process_options *opts)
{
	git_process *p;
	size_t i;

	GIT_UNUSED(env);
	GIT_UNUSED(env_len);
	GIT_ASSERT_ARG(out && args && args_len > 0);

	p = git__calloc(1, sizeof(git_process));
	GIT_ERROR_CHECK_ALLOC(p);
	p->args = git__calloc(args_len + 1, sizeof(char *));
	GIT_ERROR_CHECK_ALLOC(p->args);
	for (i = 0; i < args_len; i++) {
		p->args[i] = git__strdup(args[i]);
		GIT_ERROR_CHECK_ALLOC(p->args[i]);
	}
	p->nargs = args_len;
	p->child_in = p->child_out = -1;
	p->pid = -1;
	if (opts) {
		p->capture_in = opts->capture_in;
		p->capture_out = opts->capture_out;
		p->capture_err = opts->capture_err;
	}
	*out = p;
	return 0;
}

/* Parte la línea en palabras. Devuelve un vector de cadenas (a liberar con git_vector_dispose_deep). */
int git_process_new_from_cmdline(
	git_process **out,
	const char *cmdline,
	const char **env,
	size_t env_len,
	git_process_options *opts)
{
	git_vector v = GIT_VECTOR_INIT;
	git_str w = GIT_STR_INIT;
	const char *c = cmdline;
	int error = 0;

	GIT_ASSERT_ARG(out && cmdline);
	git_vector_init(&v, 8, NULL);

	for (;;) {
		bool hay = false;
		while (*c == ' ' || *c == '\t') c++;
		if (!*c) break;
		git_str_clear(&w);
		while (*c && *c != ' ' && *c != '\t') {
			if (*c == '"' || *c == '\'') {
				char q = *c++;
				hay = true;
				while (*c && *c != q) {
					if (q == '"' && *c == '\\' && c[1]) c++;
					git_str_putc(&w, *c++);
				}
				if (*c == q) c++;
			} else if (*c == '\\' && c[1]) {
				c++;
				hay = true;
				git_str_putc(&w, *c++);
			} else {
				hay = true;
				git_str_putc(&w, *c++);
			}
		}
		if (hay) {
			char *d = git__strdup(w.ptr ? w.ptr : "");
			GIT_ERROR_CHECK_ALLOC(d);
			if ((error = git_vector_insert(&v, d)) < 0) goto fin;
		}
	}
	if (v.length == 0) {
		git_error_set(GIT_ERROR_INVALID, "línea de órdenes vacía");
		error = -1;
		goto fin;
	}
	error = git_process_new(out, (const char **)v.contents, v.length, env, env_len, opts);
fin:
	git_str_dispose(&w);
	git_vector_dispose_deep(&v);
	return error;
}

/* Sin barra, se busca en /bin y /apps/bin (SaguiOS no tiene PATH para executar()). */
static int resolver(git_str *out, const char *nombre)
{
	static const char *dirs[] = { "/bin/", "/apps/bin/" };
	size_t i;
	if (strchr(nombre, '/'))
		return git_str_puts(out, nombre);
	for (i = 0; i < ARRAY_SIZE(dirs); i++) {
		git_str_clear(out);
		git_str_puts(out, dirs[i]);
		git_str_puts(out, nombre);
		if (git_fs_path_isfile(out->ptr))
			return 0;
	}
	git_str_clear(out);
	git_str_puts(out, "/bin/");
	git_str_puts(out, nombre);
	return 0;
}

int git_process_start(git_process *p)
{
	git_str ruta = GIT_STR_INIT;
	int pid, *pin = NULL, *pout = NULL, fin = -1, fout = -1;

	GIT_ASSERT_ARG(p);
	if (p->capture_in) pin = &fin;
	if (p->capture_out) pout = &fout;
	resolver(&ruta, p->args[0]);
	pid = sg_lanzar(ruta.ptr, p->args, pin, pout);
	if (pid < 0) {
		git_error_set(GIT_ERROR_OS, "no se pudo lanzar '%s'", ruta.ptr);
		git_str_dispose(&ruta);
		return -1;
	}
	git_str_dispose(&ruta);
	p->pid = pid;
	p->child_in = fin;
	p->child_out = fout;
	p->status = GIT_PROCESS_STATUS_NONE;
	return 0;
}

int git_process_id(p_pid_t *out, git_process *p)
{
	GIT_ASSERT_ARG(out && p);
	if (p->pid < 0) {
		git_error_set(GIT_ERROR_INVALID, "el proceso no se ha lanzado");
		return -1;
	}
	*out = p->pid;
	return 0;
}

ssize_t git_process_read(git_process *p, void *buf, size_t count)
{
	int n;
	GIT_ASSERT_ARG(p && buf && count > 0);
	if (!p->capture_out || p->child_out < 0) {
		git_error_set(GIT_ERROR_INVALID, "la salida del proceso no se captura");
		return -1;
	}
	if (count > 32768) count = 32768;
	n = sg_leer(p->child_out, buf, (unsigned)count);
	if (n < 0) {
		git_error_set(GIT_ERROR_OS, "lectura del proceso fallida (%d)", n);
		return -1;
	}
	return n;
}

ssize_t git_process_read_err(git_process *p, void *buf, size_t count)
{
	GIT_UNUSED(p); GIT_UNUSED(buf); GIT_UNUSED(count);
	git_error_set(GIT_ERROR_INVALID, "la salida de errores del proceso no se captura");
	return -1;
}

ssize_t git_process_write(git_process *p, const void *buf, size_t count)
{
	const char *b = buf;
	size_t hecho = 0;
	GIT_ASSERT_ARG(p && buf);
	if (!p->capture_in || p->child_in < 0) {
		git_error_set(GIT_ERROR_INVALID, "la entrada del proceso no se captura");
		return -1;
	}
	while (hecho < count) {
		size_t k = count - hecho > 16384 ? 16384 : count - hecho;
		int n = sg_escribir(p->child_in, b + hecho, (unsigned)k);
		if (n <= 0) {
			git_error_set(GIT_ERROR_OS, "escritura al proceso fallida (%d)", n);
			return -1;
		}
		hecho += (size_t)n;
	}
	return (ssize_t)hecho;
}

int git_process_close_in(git_process *p)
{
	if (p->child_in >= 0) { sg_cerrar(p->child_in); p->child_in = -1; }
	return 0;
}

int git_process_close_out(git_process *p)
{
	if (p->child_out >= 0) { sg_cerrar(p->child_out); p->child_out = -1; }
	return 0;
}

int git_process_close_err(git_process *p)
{
	GIT_UNUSED(p);
	return 0;
}

int git_process_close(git_process *p)
{
	git_process_close_in(p);
	git_process_close_out(p);
	return 0;
}

int git_process_wait(git_process_result *result, git_process *p)
{
	int cod = 255, r;
	GIT_ASSERT_ARG(p);
	if (p->pid < 0) {
		git_error_set(GIT_ERROR_INVALID, "el proceso no se ha lanzado");
		return -1;
	}
	git_process_close_in(p);   /* sin entrada abierta el hijo no se queda esperando */
	r = sg_esperar(p->pid, &cod);
	p->pid = -1;
	if (result) {
		result->status = r < 0 ? GIT_PROCESS_STATUS_ERROR : GIT_PROCESS_STATUS_NORMAL;
		result->exitcode = cod;
		result->signal = 0;
	}
	return 0;
}

int git_process_result_msg(git_str *msg, git_process_result *result)
{
	if (result->status == GIT_PROCESS_STATUS_NORMAL)
		return git_str_printf(msg, "el proceso terminó con código %d", result->exitcode);
	return git_str_puts(msg, "el proceso no terminó con normalidad");
}

void git_process_free(git_process *p)
{
	if (!p) return;
	git_process_close(p);
	if (p->pid >= 0) {
		int cod;
		sg_esperar(p->pid, &cod);
	}
	liberar_args(p->args, p->nargs);
	git__free(p->cwd);
	git__free(p);
}
