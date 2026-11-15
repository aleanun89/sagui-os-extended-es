// SPDX-License-Identifier: GPL-2.0-or-later
/* scp - copiar un archivo a otra máquina, o traerlo, por SSH (D317).
 *
 *   scp [-P PUERTO] [-i CLAVE] [-q] [-o OPCIÓN] ORIGEN DESTINO
 *
 * Uno de los dos es remoto: [usuario@]máquina:ruta. Es un scp sencillo sobre
 * el cliente ssh en modo tubería: para traer, «cat ruta» en el otro lado y la
 * salida a un archivo local; para llevar, el archivo local como entrada de
 * «cat > ruta». No necesita scp ni sftp en el servidor (vale para OpenSSH y
 * para el sshd de SaguiOS) y no convierte nada: los bytes llegan iguales.
 *
 * Lo que se trae se escribe primero en ARCHIVO.scp-parcial y solo se renombra
 * si la orden remota terminó bien: un corte no deja un archivo a medias con
 * el nombre bueno. Un archivo cada vez; sin -r. */
#include "sagui.h"

static const char *AYUDA =
    "uso: scp [-P PUERTO] [-i CLAVE] [-q] [-o OPCIÓN] ORIGEN DESTINO\n"
    "Copia un archivo entre esta máquina y otra por SSH; uno de los dos es [usuario@]máquina:ruta.\n"
    "  scp root@192.168.1.10:/etc/hostname .        traer\n"
    "  scp informe.txt usuario@pc:/home/usuario/    llevar\n"
    "Usa el cliente ssh (las mismas claves y la misma lista de máquinas conocidas).\n";

/* ¿Es remoto? «máquina:ruta» con los dos puntos antes de cualquier barra. */
static char *remoto(char *s)
{
    for (char *p = s; *p; p++) {
        if (*p == '/') return NULL;
        if (*p == ':') return p;
    }
    return NULL;
}

static const char *base(const char *s)
{
    const char *b = s;
    for (const char *p = s; *p; p++) if (*p == '/' && p[1]) b = p + 1;
    return b;
}

static bool eh_pasta(const char *s)
{
    info_arquivo_t i;
    return info_arquivo(s, &i) == 0 && i.tipo == TIPO_PASTA;
}

/* La ruta remota entre comillas simples; no se admiten comillas ni controles
 * (para que la orden no se pueda partir en otra). */
static bool citar(const char *ruta, char *dst, unsigned max)
{
    unsigned k = 0;
    if (!ruta[0]) ruta = ".";
    if (k + 1 >= max) return false;
    dst[k++] = '\'';
    for (const char *p = ruta; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\'' || c < 0x20 || c == 0x7F) return false;
        if (k + 2 >= max) return false;
        dst[k++] = (char)c;
    }
    dst[k++] = '\'';
    dst[k] = 0;
    return true;
}

int principal(int argc, char **argv)
{
    char *args[24];
    int na = 0;
    args[na++] = "ssh";
    args[na++] = "-T";
    bool quieto = false;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "--help") == 0 || compara_texto(a, "-h") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        }
        if ((compara_texto(a, "-P") == 0 || compara_texto(a, "-i") == 0 || compara_texto(a, "-o") == 0) && i + 1 < argc) {
            if (na > 16) { escrever_texto(ERROS, AYUDA); return 2; }
            args[na++] = compara_texto(a, "-P") == 0 ? "-p" : (char *)a;
            args[na++] = argv[++i];
            continue;
        }
        if (compara_texto(a, "-q") == 0) { quieto = true; continue; }
        escrever_texto(ERROS, AYUDA);
        return 2;
    }
    if (argc - i != 2) { escrever_texto(ERROS, AYUDA); return 2; }
    char *origem = argv[i], *destino = argv[i + 1];
    char *ro = remoto(origem), *rd = remoto(destino);
    if ((ro != NULL) == (rd != NULL)) {
        imprimir_em(ERROS, "scp: uno (y solo uno) de los dos tiene que ser remoto (máquina:ruta)\n");
        return 2;
    }
    static char ordem[600], ruta_r[520], local[400], parcial[420];
    char *maquina;
    bool trazer = ro != NULL;
    if (trazer) {
        *ro = 0;
        maquina = origem;
        if (!citar(ro + 1, ruta_r, sizeof ruta_r)) { imprimir_em(ERROS, "scp: ruta remota no admitida: %s\n", ro + 1); return 1; }
        if (eh_pasta(destino)) {
            formatar_texto(local, sizeof local, "%s%s%s", destino,
                           destino[tam_texto(destino) - 1] == '/' ? "" : "/", base(ro + 1));
        } else copia_texto(local, destino, sizeof local);
        formatar_texto(ordem, sizeof ordem, "cat %s", ruta_r);
        formatar_texto(parcial, sizeof parcial, "%s.scp-parcial", local);
        args[na++] = "-n";
        args[na++] = "--salida-a";
        args[na++] = parcial;
    } else {
        *rd = 0;
        maquina = destino;
        info_arquivo_t inf;
        if (info_arquivo(origem, &inf) < 0 || inf.tipo == TIPO_PASTA) {
            imprimir_em(ERROS, "scp: %s no es un archivo (sin -r: un archivo cada vez)\n", origem);
            return 1;
        }
        char r[520];
        const char *rr = rd + 1;
        if (!rr[0] || rr[tam_texto(rr) - 1] == '/') formatar_texto(r, sizeof r, "%s%s", rr, base(origem));
        else copia_texto(r, rr, sizeof r);
        if (!citar(r, ruta_r, sizeof ruta_r)) { imprimir_em(ERROS, "scp: ruta remota no admitida: %s\n", r); return 1; }
        formatar_texto(ordem, sizeof ordem, "cat > %s", ruta_r);
        args[na++] = "--entrada-de";
        args[na++] = origem;
    }
    if (quieto) args[na++] = "-q";
    args[na++] = maquina;
    args[na++] = ordem;
    args[na] = NULL;
    int pid = executar("/bin/ssh", args);
    if (pid < 0) { imprimir_em(ERROS, "scp: no se pudo lanzar ssh: %s\n", nome_erro(pid)); return 1; }
    int cod = 255;
    esperar(pid, &cod);
    if (trazer) {
        if (cod != 0) {
            apagar(parcial);
            imprimir_em(ERROS, "scp: no se pudo traer %s (código %d)\n", ro + 1, cod);
            return 1;
        }
        apagar(local);
        if (renomear(parcial, local) < 0) { imprimir_em(ERROS, "scp: no se pudo escribir %s\n", local); return 1; }
        if (!quieto) imprimir("%s -> %s\n", ro + 1, local);
        return 0;
    }
    if (cod != 0) { imprimir_em(ERROS, "scp: no se pudo llevar %s (código %d)\n", origem, cod); return 1; }
    if (!quieto) imprimir("%s -> %s:%s\n", origem, maquina, rd + 1);
    return 0;
}
