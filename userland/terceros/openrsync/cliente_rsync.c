// SPDX-License-Identifier: GPL-3.0-only
/* cliente_rsync.c - rsync de SaguiOS como cliente por ssh (D314, v8.4.2).
 *
 *   rsync -av carpeta/ usuario@host:ruta/      (subir)
 *   rsync -av usuario@host:ruta/ carpeta/      (bajar)
 *
 * Es codigo propio (no de openrsync). Hace lo que el client.c y el fargs.c de openrsync, que
 * usan fork y socketpair (SaguiOS no los tiene): lanza /bin/ssh con executar() y dos tuberias
 * como entrada y salida del hijo (como hace el sshd con sus procesos), manda el saludo del
 * protocolo 27 y entrega las dos tuberias al emisor o al receptor de openrsync. La lectura y la
 * escritura de esas tuberias pasan por rs_read/rs_write (compat_sagui.c), que evitan el
 * interbloqueo de las tuberias de 512 bytes del kernel.
 *
 * ssh se lanza con -T -q; la clave es /home/.ssh/id_ed25519 salvo -i (en -e), y una maquina
 * desconocida se rechaza a menos que se pase -o StrictHostKeyChecking=accept-new: con la
 * entrada ocupada por el protocolo ssh no puede preguntar la huella (conectar antes a mano con
 * "ssh maquina" la guarda en known_hosts). */
#include "sagui.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rsync_sagui.h"
#include "cliente_rsync.h"
#undef read
#undef write

#define ORDEN_MAX 1536
#define ARGS_MAX  48

int rsync_es_demonio(const char *a)
{
    if (strncmp(a, "rsync://", 8) == 0) return 1;
    const char *d = strstr(a, "::");   /* maquina::modulo/ruta */
    const char *b = strchr(a, '/');
    return d && d > a && (!b || d < b);
}

int rsync_es_remota(const char *a)
{
    if (rsync_es_demonio(a)) return 0;   /* ese tipo de remoto es otro (demonio) */
    const char *d = strchr(a, ':');
    const char *b = strchr(a, '/');
    return d && d > a && (!b || d < b);
}

/* "usuario@host:ruta" -> host (con el usuario) y ruta. false si no cabe. */
static bool partir(const char *a, char *host, size_t hmax, const char **ruta)
{
    const char *d = strchr(a, ':');
    size_t n = (size_t)(d - a);
    if (n == 0 || n >= hmax) return false;
    memcpy(host, a, n);
    host[n] = 0;
    *ruta = d + 1;
    if (!**ruta) *ruta = ".";   /* "host:" = la carpeta de entrada */
    return true;
}

/* Añade "t" a la orden remota, entre comillas simples si lleva algo que el shell interpretaria. */
static bool anadir(char *orden, size_t max, const char *t)
{
    size_t l = strlen(orden);
    bool cita = !*t;
    for (const char *p = t; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              strchr("_./~=+,:@%-", *p)))
            cita = true;
    if (l + 4 + strlen(t) * 4 >= max) return false;
    if (l) orden[l++] = ' ';
    if (!cita) { strcpy(orden + l, t); return true; }
    orden[l++] = '\'';
    for (const char *p = t; *p; p++) {
        if (*p == '\'') { memcpy(orden + l, "'\\''", 4); l += 4; }
        else orden[l++] = *p;
    }
    orden[l++] = '\'';
    orden[l] = 0;
    return true;
}

/* Vacia lo que ssh aun mande tras terminar el protocolo, para que pueda cerrar sin bloquearse. */
static void vaciar_hasta_fin(int fd, unsigned ms)
{
    unsigned t0 = tempo_ms();
    char b[256];
    while (tempo_ms() - t0 < ms) {
        int d = disponivel(fd);
        if (d == ERRO_PIPE_QUEBRADO) return;
        if (d > 0) { ler(fd, b, (unsigned)d < sizeof b ? (unsigned)d : sizeof b); continue; }
        dormir(10);
    }
}

/* Las opciones de la orden "rsync --server ..." del otro lado: -vlogDtpre.iLsfxC segun lo pedido. */
static void armar_flags(const struct opts *opts, char flags[24])
{
    size_t nf = 0;
    flags[nf++] = '-';
    for (int i = 0; i < opts->verbose && nf < 8; i++) flags[nf++] = 'v';
    if (opts->dry_run) flags[nf++] = 'n';
    if (opts->preserve_links) flags[nf++] = 'l';
    if (opts->preserve_uids) flags[nf++] = 'o';
    if (opts->preserve_gids) flags[nf++] = 'g';
    flags[nf++] = 'D';
    if (opts->preserve_times) flags[nf++] = 't';
    if (opts->preserve_perms) flags[nf++] = 'p';
    if (opts->recursive) flags[nf++] = 'r';
    strcpy(flags + nf, "e.iLsfxC");
}

int rsync_cliente_ssh(const struct opts *opts, const char *ssh_orden, int argc, char **argv)
{
    char host[256], orden[ORDEN_MAX], ssh_copia[256];
    const char *ruta_remota = NULL;
    bool subir = false;   /* el destino es remoto: nosotros emitimos */
    int nrem = 0;
    for (int i = 0; i < argc; i++) if (rsync_es_remota(argv[i])) nrem++;
    if (nrem == 0 || argc < 2) { fprintf(stderr, "rsync: falta el origen o el destino remoto\n"); return 1; }
    if (rsync_es_remota(argv[argc - 1])) {
        if (nrem != 1) { fprintf(stderr, "rsync: no se copia de remoto a remoto\n"); return 1; }
        subir = true;
        if (!partir(argv[argc - 1], host, sizeof host, &ruta_remota)) { fprintf(stderr, "rsync: maquina no valida\n"); return 1; }
    } else {
        /* bajar: todos los origenes son remotos y de la misma maquina */
        if (nrem != argc - 1) { fprintf(stderr, "rsync: no se mezclan origenes locales y remotos\n"); return 1; }
        char h2[256];
        const char *r2;
        if (!partir(argv[0], host, sizeof host, &r2)) { fprintf(stderr, "rsync: maquina no valida\n"); return 1; }
        for (int i = 1; i < argc - 1; i++)
            if (!partir(argv[i], h2, sizeof h2, &r2) || strcmp(h2, host) != 0) {
                fprintf(stderr, "rsync: todos los origenes remotos deben ser de la misma maquina\n");
                return 1;
            }
    }

    /* La orden que ejecutara el otro lado: rsync --server [--sender] -vlogDtpre.iLsfxC [--delete] . rutas */
    char flags[24];
    armar_flags(opts, flags);
    orden[0] = 0;
    bool ok = anadir(orden, sizeof orden, opts->rsync_path ? opts->rsync_path : "rsync") &&
              anadir(orden, sizeof orden, "--server") &&
              (subir || anadir(orden, sizeof orden, "--sender")) &&
              anadir(orden, sizeof orden, flags) &&
              (!opts->del || anadir(orden, sizeof orden, "--delete")) &&
              anadir(orden, sizeof orden, ".");
    if (subir) ok = ok && anadir(orden, sizeof orden, ruta_remota);
    else for (int i = 0; ok && i < argc - 1; i++) {
        char h2[256];
        const char *r2;
        partir(argv[i], h2, sizeof h2, &r2);
        ok = anadir(orden, sizeof orden, r2);
    }
    if (!ok) { fprintf(stderr, "rsync: la orden remota es demasiado larga\n"); return 1; }

    /* Los argumentos de ssh: lo de -e (palabras separadas por espacios) y la orden. */
    char *args[ARGS_MAX];
    int na = 0;
    snprintf(ssh_copia, sizeof ssh_copia, "%s", ssh_orden && *ssh_orden ? ssh_orden : "ssh");
    for (char *p = ssh_copia; *p && na < ARGS_MAX - 6;) {
        while (*p == ' ') p++;
        if (!*p) break;
        args[na++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    const char *prog = na ? args[0] : "ssh";
    if (strcmp(prog, "ssh") != 0 && strcmp(prog, "/bin/ssh") != 0) {
        fprintf(stderr, "rsync: -e solo admite el ssh de SaguiOS (ssh [opciones])\n");
        return 1;
    }
    args[0] = "ssh";
    args[na++] = "-T";
    args[na++] = "-q";
    args[na++] = host;
    args[na++] = orden;
    args[na] = NULL;

    int pin[2] = { -1, -1 }, pout[2] = { -1, -1 };   /* pin: rsync -> ssh; pout: ssh -> rsync */
    if (criar_pipe(pin) != 0 || criar_pipe(pout) != 0) {
        fprintf(stderr, "rsync: no se pudieron crear las tuberias\n");
        return 1;
    }
    int g0 = duplicar(ENTRADA, 28), g1 = duplicar(SAIDA, 29);
    duplicar(pin[0], ENTRADA);
    duplicar(pout[1], SAIDA);
    int pid = executar("/bin/ssh", args);
    duplicar(g0, ENTRADA);
    duplicar(g1, SAIDA);
    fechar(g0);
    fechar(g1);
    fechar(pout[1]);
    if (pid < 0) {
        fprintf(stderr, "rsync: no se pudo lanzar ssh: %s\n", nome_erro(pid));
        fechar(pin[0]); fechar(pin[1]); fechar(pout[0]);
        return 1;
    }
    int fdin = pout[0], fdout = pin[1];
    rs_cliente_tuberias(fdin, fdout, pin[0]);

    /* Saludo del protocolo (lado cliente de rsync_client de openrsync). */
    struct sess sess;
    memset(&sess, 0, sizeof sess);
    sess.opts = opts;
    sess.lver = RSYNC_PROTOCOL;
    int rc = 0;
    if (!io_write_int(&sess, fdout, sess.lver) || !io_read_int(&sess, fdin, &sess.rver) ||
        !io_read_int(&sess, fdin, &sess.seed)) {
        fprintf(stderr, "rsync: no hubo saludo del otro lado (¿ssh no pudo conectar o no hay rsync en la maquina remota?)\n");
        goto fim;
    }
    if (sess.rver < sess.lver) {
        fprintf(stderr, "rsync: el protocolo remoto (%d) es mas viejo que el nuestro (%d)\n", sess.rver, sess.lver);
        goto fim;
    }
    sess.mplex_reads = 1;
    if (subir) rc = rsync_sender(&sess, fdin, fdout, (size_t)(argc - 1), argv) ? 1 : 0;
    else rc = rsync_receiver(&sess, fdin, fdout, argv[argc - 1]) ? 1 : 0;

fim:
    fechar(pin[1]);     /* fin de la entrada de ssh: cierra el canal */
    vaciar_hasta_fin(pout[0], 5000);
    fechar(pin[0]);
    fechar(pout[0]);
    int cod = 255;
    esperar(pid, &cod);
    if (!rc) {
        if (cod != 0) fprintf(stderr, "rsync: ssh termino con codigo %d\n", cod);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ demonio rsync:// (D336)
 *
 *   rsync rsync://maquina[:puerto]/                   lista los modulos
 *   rsync -av rsync://maquina/modulo/ruta/ destino/   baja
 *   rsync -av carpeta/ rsync://maquina/modulo/ruta/   sube
 *   (tambien maquina::modulo/ruta)
 *
 * El protocolo del demonio (puerto 873): primero lineas de texto ("@RSYNCD: 27", el nombre del
 * modulo, "@RSYNCD: OK", los argumentos del servidor terminados en salto de linea porque el
 * protocolo es el 27, menor que el 30) y despues el mismo protocolo binario que por ssh, con
 * el socket como entrada y salida. Sin autenticacion (un modulo con "auth users" se rechaza
 * con un mensaje) y sin cifrar: es el demonio sin mas, pensado para la red local. */
#define DEMONIO_PUERTO 873

struct url_demonio {
    char host[256];
    int  puerto;
    char modulo[128];
    char ruta[512];   /* lo que sigue al modulo, sin la barra inicial; puede ser "" */
    int  hay_modulo;
};

static bool partir_demonio(const char *a, struct url_demonio *u)
{
    const char *p;
    memset(u, 0, sizeof *u);
    u->puerto = DEMONIO_PUERTO;
    if (strncmp(a, "rsync://", 8) == 0) p = a + 8;
    else p = a;
    const char *fin;
    if (p == a) { fin = strstr(a, "::"); if (!fin) return false; }
    else { fin = strchr(p, '/'); if (!fin) fin = p + strlen(p); }
    char h[256];
    size_t n = (size_t)(fin - p);
    if (n == 0 || n >= sizeof h) return false;
    memcpy(h, p, n);
    h[n] = 0;
    char *arroba = strchr(h, '@');
    char *maq = arroba ? arroba + 1 : h;       /* el usuario se ignora: no hay autenticacion */
    char *dp = strchr(maq, ':');
    if (dp) { *dp = 0; u->puerto = atoi(dp + 1); if (u->puerto <= 0 || u->puerto > 65535) return false; }
    if (!*maq || strlen(maq) >= sizeof u->host) return false;
    strcpy(u->host, maq);
    const char *r = p == a ? fin + 2 : fin;     /* tras :: o tras la primera barra */
    if (*r == '/') r++;
    if (!*r) return true;                       /* solo la maquina: listar */
    const char *b = strchr(r, '/');
    size_t m = b ? (size_t)(b - r) : strlen(r);
    if (m == 0 || m >= sizeof u->modulo) return false;
    memcpy(u->modulo, r, m);
    u->modulo[m] = 0;
    u->hay_modulo = 1;
    if (b) {
        if (strlen(b + 1) >= sizeof u->ruta) return false;
        strcpy(u->ruta, b + 1);
    }
    return true;
}

/* Una linea del socket (sin el salto de linea). -1 si se cerro o no cabe. */
static int leer_linea(int fd, char *l, size_t max)
{
    size_t n = 0;
    for (;;) {
        char c;
        int r = ler(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n') break;
        if (c == '\r') continue;
        if (n + 1 < max) l[n++] = c;
    }
    l[n] = 0;
    return (int)n;
}

static bool escribir_texto(int fd, const char *t)
{
    return escrever_tudo(fd, t, (unsigned)strlen(t)) >= 0;
}

int rsync_cliente_demonio(const struct opts *opts, int argc, char **argv)
{
    struct url_demonio u, u2;
    bool subir = false, listar = false;
    int nsrc = argc - 1;
    if (argc == 1) {
        if (!partir_demonio(argv[0], &u)) { fprintf(stderr, "rsync: direccion rsync:// no valida: %s\n", argv[0]); return 1; }
        listar = !u.hay_modulo;
        if (!listar) { fprintf(stderr, "rsync: falta el destino\n"); return 1; }
    } else if (rsync_es_demonio(argv[argc - 1])) {
        subir = true;
        if (!partir_demonio(argv[argc - 1], &u) || !u.hay_modulo) { fprintf(stderr, "rsync: destino rsync:// sin modulo\n"); return 1; }
        for (int i = 0; i < nsrc; i++)
            if (rsync_es_demonio(argv[i]) || rsync_es_remota(argv[i])) { fprintf(stderr, "rsync: no se copia de remoto a remoto\n"); return 1; }
    } else {
        if (!partir_demonio(argv[0], &u) || !u.hay_modulo) { fprintf(stderr, "rsync: origen rsync:// sin modulo\n"); return 1; }
        for (int i = 0; i < nsrc; i++)
            if (!rsync_es_demonio(argv[i]) || !partir_demonio(argv[i], &u2) || !u2.hay_modulo ||
                strcmp(u2.host, u.host) != 0 || u2.puerto != u.puerto || strcmp(u2.modulo, u.modulo) != 0) {
                fprintf(stderr, "rsync: los origenes deben ser todos del mismo modulo del mismo demonio\n");
                return 1;
            }
    }

    int fd = conectar_tcp(u.host, u.puerto, 15000);
    if (fd < 0) { fprintf(stderr, "rsync: no se pudo conectar con %s:%d: %s\n", u.host, u.puerto, nome_erro(fd)); return 1; }

    char l[1024];
    int rc = 1;
    if (!escribir_texto(fd, "@RSYNCD: 27\n") || leer_linea(fd, l, sizeof l) < 0 || strncmp(l, "@RSYNCD:", 8) != 0) {
        fprintf(stderr, "rsync: %s:%d no responde como un demonio rsync\n", u.host, u.puerto);
        goto fim;
    }
    if (!escribir_texto(fd, listar ? "#list\n" : u.modulo) || (!listar && !escribir_texto(fd, "\n"))) goto fim;

    if (listar) {
        while (leer_linea(fd, l, sizeof l) >= 0) {
            if (strncmp(l, "@RSYNCD: EXIT", 13) == 0) { rc = 0; break; }
            if (strncmp(l, "@ERROR", 6) == 0) { fprintf(stderr, "rsync: %s\n", l); break; }
            printf("%s\n", l);
        }
        goto fim;
    }

    for (;;) {    /* el mensaje del dia y, al final, @RSYNCD: OK */
        if (leer_linea(fd, l, sizeof l) < 0) { fprintf(stderr, "rsync: el demonio cerro la conexion\n"); goto fim; }
        if (strncmp(l, "@RSYNCD: OK", 11) == 0) break;
        if (strncmp(l, "@RSYNCD: AUTHREQD", 17) == 0) {
            fprintf(stderr, "rsync: el modulo '%s' pide usuario y clave y este cliente no autentica (use ssh)\n", u.modulo);
            goto fim;
        }
        if (strncmp(l, "@ERROR", 6) == 0 || strncmp(l, "@RSYNCD: EXIT", 13) == 0) { fprintf(stderr, "rsync: %s\n", l); goto fim; }
        if (opts->verbose) printf("%s\n", l);
    }

    /* Los argumentos del servidor, uno por linea (protocolo < 30), y una linea vacia. */
    char flags[24];
    armar_flags(opts, flags);
    bool ok = escribir_texto(fd, "--server\n") && (subir || escribir_texto(fd, "--sender\n")) &&
              escribir_texto(fd, flags) && escribir_texto(fd, "\n") &&
              (!opts->del || escribir_texto(fd, "--delete\n")) && escribir_texto(fd, ".\n");
    if (subir) {
        snprintf(l, sizeof l, "%s/%s\n", u.modulo, u.ruta);
        ok = ok && escribir_texto(fd, l);
    } else for (int i = 0; ok && i < nsrc; i++) {
        partir_demonio(argv[i], &u2);
        snprintf(l, sizeof l, "%s/%s\n", u2.modulo, u2.ruta);
        ok = escribir_texto(fd, l);
    }
    ok = ok && escribir_texto(fd, "\n");
    if (!ok) { fprintf(stderr, "rsync: no se pudieron mandar los argumentos\n"); goto fim; }

    struct sess sess;
    memset(&sess, 0, sizeof sess);
    sess.opts = opts;
    sess.lver = RSYNC_PROTOCOL;
    sess.rver = RSYNC_PROTOCOL;      /* ya negociado en texto: el servidor usa el nuestro */
    if (!io_read_int(&sess, fd, &sess.seed)) { fprintf(stderr, "rsync: el demonio no mando la semilla (¿ruta o modulo no valido?)\n"); goto fim; }
    sess.mplex_reads = 1;
    rs_cliente_socket(fd);
    if (subir) rc = rsync_sender(&sess, fd, fd, (size_t)nsrc, argv) ? 0 : 1;
    else rc = rsync_receiver(&sess, fd, fd, argv[argc - 1]) ? 0 : 1;
fim:
    fechar(fd);
    return rc;
}
