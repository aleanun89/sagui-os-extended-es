// SPDX-License-Identifier: GPL-3.0-only
/* flujo_sagui.c - los flujos de red de libgit2 sobre los sockets y el TLS de SaguiOS (D400).
 *
 * libgit2 sin OpenSSL/mbedTLS: el transporte HTTP(S) pide un flujo con git_stream_register. Aquí van
 * dos: uno TCP plano (conectar_tcp de la libsagui, sin select/poll) y otro TLS 1.2/1.3 con la
 * biblioteca propia de SaguiOS (libtls: tls_novo/tls_conectar, la misma de curl y wget), que
 * verifica la cadena de certificados contra las raíces del sistema (/etc/ssl). La verificación se
 * hace al conectar (libgit2 solo la delega si el programa da una retrollamada).
 *
 * Variables de entorno, como en git: GIT_SSL_NO_VERIFY (no verifica) y GIT_SSL_CAINFO (un PEM con
 * las únicas raíces que valen). */
#include "tls.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <git2.h>
#include <git2/sys/stream.h>
#include <git2/sys/errors.h>
#include <string.h>

typedef struct {
    git_stream parent;
    char *host;
    int puerto;
    int fd;
    tls_t *tls;
    tls_config_t cfg;
    int cifrado;
} flujo_sagui_t;

static int flujo_conectar(git_stream *s)
{
    flujo_sagui_t *f = (flujo_sagui_t *)s;
    f->fd = conectar_tcp(f->host, f->puerto, 20000);
    if (f->fd < 0) {
        git_error_set(GIT_ERROR_NET, "no se pudo conectar a %s:%d: %s", f->host, f->puerto, nome_erro(f->fd));
        f->fd = -1;
        return -1;
    }
    if (!f->cifrado) return 0;

    char erro[200];
    const char *cacert = getenv("GIT_SSL_CAINFO");
    if (!tls_config_padrao(&f->cfg, cacert, erro, sizeof erro)) {
        git_error_set(GIT_ERROR_SSL, "%s", erro);
        return -1;
    }
    f->cfg.host = f->host;
    f->cfg.inseguro = getenv("GIT_SSL_NO_VERIFY") != NULL;
    f->tls = tls_novo(&f->cfg, f->fd);
    if (!f->tls) {
        git_error_set(GIT_ERROR_SSL, "sin memoria para TLS");
        return -1;
    }
    int r = tls_conectar(f->tls);
    if (r < 0) {
        git_error_set(GIT_ERROR_SSL, "%s: %s", f->host, tls_erro(f->tls));
        return r == TLS_ERRO_CERT ? GIT_ECERTIFICATE : -1;
    }
    return 0;
}

static int flujo_certificado(git_cert **out, git_stream *s)
{
    (void)s;
    *out = NULL;
    git_error_set(GIT_ERROR_SSL, "el certificado del servidor ya lo verifica el flujo TLS de SaguiOS");
    return GIT_ENOTFOUND;
}

static int flujo_proxy(git_stream *s, const git_proxy_options *p)
{
    (void)s; (void)p;
    git_error_set(GIT_ERROR_NET, "los proxies no están soportados en SaguiOS");
    return -1;
}

static ssize_t flujo_leer(git_stream *s, void *buf, size_t n)
{
    flujo_sagui_t *f = (flujo_sagui_t *)s;
    if (n > 16384) n = 16384;
    int r = f->tls ? tls_ler(f->tls, buf, (unsigned)n) : ler(f->fd, buf, (unsigned)n);
    if (r < 0) {
        git_error_set(GIT_ERROR_NET, "lectura de red fallida: %s", f->tls ? tls_erro(f->tls) : nome_erro(r));
        return -1;
    }
    return r;
}

static ssize_t flujo_escribir(git_stream *s, const char *data, size_t n, int flags)
{
    flujo_sagui_t *f = (flujo_sagui_t *)s;
    (void)flags;
    size_t hecho = 0;
    while (hecho < n) {
        unsigned k = n - hecho > 8192 ? 8192 : (unsigned)(n - hecho);
        int r = f->tls ? tls_escrever(f->tls, data + hecho, k) : escrever(f->fd, data + hecho, k);
        if (r <= 0) {
            git_error_set(GIT_ERROR_NET, "escritura de red fallida: %s", f->tls ? tls_erro(f->tls) : nome_erro(r));
            return -1;
        }
        hecho += (size_t)r;
    }
    return (ssize_t)hecho;
}

static int flujo_cerrar(git_stream *s)
{
    flujo_sagui_t *f = (flujo_sagui_t *)s;
    if (f->tls) { tls_fechar(f->tls); f->tls = NULL; }
    if (f->fd >= 0) { fechar(f->fd); f->fd = -1; }
    return 0;
}

static void flujo_liberar(git_stream *s)
{
    flujo_sagui_t *f = (flujo_sagui_t *)s;
    flujo_cerrar(s);
    free(f->host);
    free(f);
}

static int flujo_nuevo(git_stream **out, const char *host, const char *puerto, int cifrado)
{
    flujo_sagui_t *f = calloc(1, sizeof *f);
    if (!f) return -1;
    f->host = strdup(host);
    f->puerto = atoi(puerto);
    if (f->puerto <= 0) f->puerto = cifrado ? 443 : 80;
    f->fd = -1;
    f->cifrado = cifrado;
    f->parent.version = GIT_STREAM_VERSION;
    f->parent.encrypted = cifrado ? 1 : 0;
    f->parent.proxy_support = 0;
    f->parent.connect = flujo_conectar;
    f->parent.certificate = flujo_certificado;
    f->parent.set_proxy = flujo_proxy;
    f->parent.read = flujo_leer;
    f->parent.write = flujo_escribir;
    f->parent.close = flujo_cerrar;
    f->parent.free = flujo_liberar;
    *out = &f->parent;
    return 0;
}

static int nuevo_tcp(git_stream **out, const char *host, const char *puerto) { return flujo_nuevo(out, host, puerto, 0); }
static int nuevo_tls(git_stream **out, const char *host, const char *puerto) { return flujo_nuevo(out, host, puerto, 1); }

/* Se llama una vez al empezar (después de git_libgit2_init). */
int git_sagui_registrar_flujos(void)
{
    git_stream_registration reg;
    memset(&reg, 0, sizeof reg);
    reg.version = GIT_STREAM_VERSION;
    reg.init = nuevo_tcp;
    int r = git_stream_register(GIT_STREAM_STANDARD, &reg);
    if (r < 0) return r;
    reg.init = nuevo_tls;
    return git_stream_register(GIT_STREAM_TLS, &reg);
}
