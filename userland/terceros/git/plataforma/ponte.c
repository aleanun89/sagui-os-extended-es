// SPDX-License-Identifier: GPL-3.0-only
/* ponte.c - lo de la libsagui que libgit2 necesita, sin mezclar sus cabeceras con las de libgit2 (D400).
 *
 * SaguiOS no tiene fork: executar() lanza un programa nuevo que hereda solo las entradas 0, 1 y 2.
 * Para darle tuberías se redirigen 0 y 1 del propio proceso (como hace el sagsh en sus tuberías),
 * se lanza y se restauran. Los otros extremos de las tuberías no se heredan (solo se heredan 0-2),
 * así que cerrar el extremo de escritura da el EOF al hijo. */
#include "sagui.h"

/* Lanza ruta con argv. Con entrada/salida != NULL crea una tubería para esa corriente y devuelve el
 * extremo del padre; si es NULL, el hijo hereda la del padre. Devuelve el pid o <0. */
int sg_lanzar(const char *ruta, char *const argv[], int *entrada, int *salida)
{
    int pin[2] = { -1, -1 }, pout[2] = { -1, -1 };
    int g0 = -1, g1 = -1, pid;
    if (entrada && criar_pipe(pin) < 0) return -1;
    if (salida && criar_pipe(pout) < 0) {
        if (entrada) { fechar(pin[0]); fechar(pin[1]); }
        return -1;
    }
    /* Copias libres de 0 y 1: un descriptor que no esté en uso. */
    int libres[2], nl = 0;
    for (int fd = 3; fd < 64 && nl < 2; fd++) {
        info_arquivo_t inf;
        (void)inf;
        if (fd == pin[0] || fd == pin[1] || fd == pout[0] || fd == pout[1]) continue;
        if (tipo_descritor(fd) < 0) libres[nl++] = fd;
    }
    if (nl < 2) pid = -1;
    else {
        g0 = libres[0]; g1 = libres[1];
        duplicar(ENTRADA, g0);
        duplicar(SAIDA, g1);
        if (entrada) { duplicar(pin[0], ENTRADA); fechar(pin[0]); }
        if (salida) { duplicar(pout[1], SAIDA); fechar(pout[1]); }
        pid = executar(ruta, argv);
        duplicar(g0, ENTRADA);
        duplicar(g1, SAIDA);
        fechar(g0);
        fechar(g1);
    }
    if (pid < 0) {
        if (entrada) { fechar(pin[1]); if (pin[0] >= 0) fechar(pin[0]); }
        if (salida) { fechar(pout[0]); }
        return pid < 0 ? pid : -1;
    }
    if (entrada) *entrada = pin[1];
    if (salida) *salida = pout[0];
    return pid;
}

int sg_leer(int fd, void *buf, unsigned n) { return ler(fd, buf, n); }
int sg_escribir(int fd, const void *buf, unsigned n) { return escrever(fd, buf, n); }
int sg_cerrar(int fd) { return fechar(fd); }
int sg_esperar(int pid, int *codigo) { return esperar(pid, codigo); }
