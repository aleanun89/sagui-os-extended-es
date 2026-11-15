// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos - prueba de los permisos de archivos (D282, mejora 098 fase 2; tests/h282_permisos.py).
 *
 *   permisos --matriz   lanzado como root: prepara archivos y carpetas de root con
 *                       permisos conocidos en cada sistema (RAM, ext2 en /home y /logs,
 *                       FAT32 en /fatroot y /fatsagui), comprueba lo de root (todo se
 *                       puede, chown, chmod, el FAT32 no guarda permisos), se convierte
 *                       en sagui (USU_FIJAR) y recorre la matriz de operaciones: leer,
 *                       escribir, crear, borrar, renombrar, mkdir, rmdir, listar, entrar,
 *                       ejecutar, chmod, chown, chgrp, umask y el pegajoso de /tmp.
 *   permisos --ver RUTA  imprime "permisos: RUTA modo uid gid com_donos" (lo usa la prueba
 *                       tras reiniciar para ver que el ext2 los guarda).
 * Cada fallo va a la salida y al registro con «permisos: MAL»; al final,
 * «permisos: matriz ok=N mal=M». */
#include "sagui.h"
#include "contas.h"

static int ok, mal;

static void anota(bool bem, const char *fmt, ...)
{
    char t[200];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (bem) ok++; else mal++;
    if (!bem) {
        imprimir("permisos: MAL %s\n", t);
        registrar("permisos: MAL %s", t);
    }
}

static int criar(const char *c, const char *texto)
{
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = texto ? escrever(fd, texto, (unsigned)tam_texto(texto)) : 0;
    fechar(fd);
    return r < 0 ? r : 0;
}

static int ler_arq(const char *c)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return fd;
    char b[16];
    int r = ler(fd, b, sizeof b);
    fechar(fd);
    return r < 0 ? r : 0;
}

static int escrever_arq(const char *c)
{
    int fd = abrir(c, ABRIR_ESCREVER);
    if (fd < 0) return fd;
    int r = escrever(fd, "x", 1);
    fechar(fd);
    return r < 0 ? r : 0;
}

static int listar_pasta(const char *c)
{
    entrada_pasta_t e;
    int r = listar(c, 0, &e);
    return r < 0 ? r : 0;
}

static bool meta(const char *c, unsigned *modo, unsigned *uid, unsigned *gid, unsigned *donos)
{
    info_permisos_t p;
    if (permisos_arquivo(c, &p) < 0) return false;
    if (modo) *modo = p.modo;
    if (uid) *uid = p.uid;
    if (gid) *gid = p.gid;
    if (donos) *donos = p.com_donos;
    return true;
}

/* Copia /bin/echo a "para" (un programa: su primer bloque es la cabecera ELF). */
static int copiar_programa(const char *para)
{
    int fi = abrir("/bin/echo", ABRIR_LER);
    if (fi < 0) return fi;
    int fo = abrir(para, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fo < 0) { fechar(fi); return fo; }
    static char b[4096];
    int n, r = 0;
    while ((n = ler(fi, b, sizeof b)) > 0)
        if (escrever(fo, b, (unsigned)n) != n) { r = ERRO_LIMITE; break; }
    fechar(fi);
    fechar(fo);
    return r;
}

static int rodar(const char *c)
{
    char *argv[] = { (char *)c, (char *)"permisos-ok", NULL };
    int pid = executar(c, argv);
    if (pid < 0) return pid;
    int cod = -1;
    esperar(pid, &cod);
    return cod;
}

#define ESPERA(expr, esperado, txt) do { int _r = (expr); anota(_r == (esperado), "%s: esperaba %d y dio %d", txt, (int)(esperado), _r); } while (0)
#define PODE(expr, txt) do { int _r = (expr); anota(_r >= 0, "%s: esperaba poder y dio %d", txt, _r); } while (0)
#define NEGA(expr, txt) ESPERA(expr, ERRO_ACESSO, txt)

/* ---------------------------------------------------------------- como root */

/* Lo que root deja preparado en "base" (una carpeta donde todos escriben o la de
 * pruebas): un archivo 0600, uno 0640, uno 0604, uno 0666, una carpeta 0700 con un
 * archivo y una 0755. */
static void preparar(const char *base)
{
    char c[160];
    static const struct { const char *n; unsigned modo; } arqs[] = {
        { "r600", 0600 }, { "r640", 0640 }, { "r604", 0604 }, { "r666", 0666 },
    };
    for (unsigned i = 0; i < sizeof arqs / sizeof arqs[0]; i++) {
        caminho_juntar(c, sizeof c, base, arqs[i].n);
        PODE(criar(c, "de root\n"), c);
        PODE(mudar_permisos(c, arqs[i].modo), c);
    }
    caminho_juntar(c, sizeof c, base, "p700");
    criar_pasta(c);
    PODE(mudar_permisos(c, 0700), c);
    caminho_juntar(c, sizeof c, base, "p700/dentro");
    PODE(criar(c, "x"), c);
    caminho_juntar(c, sizeof c, base, "p755");
    criar_pasta(c);
    PODE(mudar_permisos(c, 0755), c);
    caminho_juntar(c, sizeof c, base, "p755/dentro");
    PODE(criar(c, "x"), c);
}

static void como_root(void)
{
    anota(meu_uid() == UID_ROOT, "empieza como root (uid %u)", meu_uid());
    unsigned modo = 0, uid = 0, gid = 0, donos = 0;
    /* El ramdisk: de root; /tmp, 1777; /sistema, 0711 (D313: se atraviesa hasta /sistema/idiomas). */
    anota(meta("/bin", &modo, &uid, &gid, &donos) && modo == 0755 && uid == 0 && gid == 0 && donos, "/bin root 0755 (%o %u)", modo, uid);
    anota(meta("/bin/ls", &modo, &uid, NULL, NULL) && modo == 0755 && uid == 0, "/bin/ls root 0755 (%o)", modo);
    anota(meta("/etc/usuarios", &modo, &uid, NULL, NULL) && modo == 0644 && uid == 0, "/etc/usuarios root 0644 (%o)", modo);
    anota(meta("/tmp", &modo, &uid, NULL, NULL) && modo == 01777 && uid == 0, "/tmp 1777 (%o)", modo);
    anota(meta("/sistema", &modo, &uid, NULL, NULL) && modo == 0711 && uid == 0, "/sistema 0711 (%o)", modo);
    anota(meta("/sistema/idiomas", &modo, &uid, NULL, NULL) && modo == 0755 && uid == 0, "/sistema/idiomas 0755 (%o)", modo);
    anota(meta("/sistema/arranque", &modo, &uid, NULL, NULL) && modo == 0700 && uid == 0, "/sistema/arranque 0700 (%o)", modo);
    /* /home (ext2): de sagui; /logs (ext2): 1777. */
    anota(meta("/home", &modo, &uid, &gid, &donos) && modo == 0755 && uid == UID_SAGUI && gid == GID_SAGUI && donos,
          "/home de sagui 0755 (%o %u %u %u)", modo, uid, gid, donos);
    anota(meta("/logs", &modo, &uid, NULL, &donos) && modo == 01777 && uid == 0 && donos, "/logs ext2 1777 (%o %u)", modo, uid);
    /* FAT32: los del montaje. */
    anota(meta("/fatroot", &modo, &uid, &gid, &donos) && modo == 0755 && uid == 0 && gid == 0 && !donos,
          "/fatroot: uid=root,umask=022 (%o %u %u %u)", modo, uid, gid, donos);
    anota(meta("/fatsagui", &modo, &uid, &gid, &donos) && modo == 0755 && uid == UID_SAGUI && gid == GID_SAGUI && !donos,
          "/fatsagui: de sagui por defecto (%o %u %u)", modo, uid, gid);
    /* Root puede todo, también lo de 0000. */
    PODE(criar("/tmp/cero", "root lee igual"), "root crea /tmp/cero");
    PODE(mudar_permisos("/tmp/cero", 0), "chmod 0 /tmp/cero");
    PODE(ler_arq("/tmp/cero"), "root lee un 0000");
    PODE(escrever_arq("/tmp/cero"), "root escribe un 0000");
    apagar("/tmp/cero");
    /* Lo que root crea con su umask (022). */
    PODE(criar("/tmp/r-umask", "x"), "root crea /tmp/r-umask");
    anota(meta("/tmp/r-umask", &modo, &uid, &gid, NULL) && modo == 0644 && uid == 0 && gid == 0, "lo de root nace 0644 (%o %u)", modo, uid);
    apagar("/tmp/r-umask");
    /* El FAT32 no guarda permisos: chmod y chown no se pueden. */
    PODE(criar("/fatroot/f", "fat"), "root crea en /fatroot");
    ESPERA(mudar_permisos("/fatroot/f", 0600), ERRO_NAO_SUPORTADO, "chmod en FAT32");
    ESPERA(mudar_dono("/fatroot/f", UID_SAGUI, -1), ERRO_NAO_SUPORTADO, "chown en FAT32");
    /* Los preparados. */
    preparar("/tmp");
    criar_pasta("/home/permisos");
    preparar("/logs");
    /* chown y chgrp de root (en RAM y en ext2). */
    PODE(criar("/tmp/r-chown", "x"), "crear /tmp/r-chown");
    PODE(mudar_dono("/tmp/r-chown", UID_SAGUI, GID_SAGUI), "chown sagui:sagui (RAM)");
    anota(meta("/tmp/r-chown", NULL, &uid, &gid, NULL) && uid == UID_SAGUI && gid == GID_SAGUI, "chown en RAM (%u %u)", uid, gid);
    PODE(criar("/logs/r-chgrp", "x"), "crear /logs/r-chgrp");
    PODE(mudar_permisos("/logs/r-chgrp", 0640), "chmod 640 (ext2)");
    PODE(mudar_dono("/logs/r-chgrp", -1, GID_SAGUI), "chgrp sagui (ext2)");
    anota(meta("/logs/r-chgrp", &modo, &uid, &gid, &donos) && modo == 0640 && uid == 0 && gid == GID_SAGUI && donos,
          "chgrp en ext2 (%o %u %u)", modo, uid, gid);
    /* En /home, lo de root se ve de sagui (un solo usuario hasta la fase 3). */
    PODE(criar("/home/permisos/de-root", "x"), "root crea en /home");
    anota(meta("/home/permisos/de-root", NULL, &uid, &gid, NULL) && uid == UID_SAGUI && gid == GID_SAGUI,
          "lo de root en /home se ve de sagui (%u %u)", uid, gid);
    /* El ext2 lo guarda: queda para después de reiniciar. */
    PODE(criar("/home/persiste", "x"), "crear /home/persiste");
    PODE(mudar_permisos("/home/persiste", 0751), "chmod 751 /home/persiste");
    PODE(criar("/logs/persiste", "x"), "crear /logs/persiste");
    PODE(mudar_permisos("/logs/persiste", 0640), "chmod 640 /logs/persiste");
    PODE(mudar_dono("/logs/persiste", UID_SAGUI, 100), "chown 1000:100 /logs/persiste");
}

/* ---------------------------------------------------------------- como sagui */

/* La matriz sobre lo que root preparó en "base". "pegajosa": la carpeta lo es. */
static void matriz(const char *base, bool pegajosa)
{
    char c[160], d[160];
    caminho_juntar(c, sizeof c, base, "r600");
    NEGA(ler_arq(c), "leer r600 de root");
    NEGA(escrever_arq(c), "escribir r600 de root");
    caminho_juntar(c, sizeof c, base, "r640");
    NEGA(ler_arq(c), "leer r640 (grupo root)");
    caminho_juntar(c, sizeof c, base, "r604");
    PODE(ler_arq(c), "leer r604 (otros r)");
    NEGA(escrever_arq(c), "escribir r604");
    NEGA(mudar_permisos(c, 0666), "chmod de un archivo de root");
    NEGA(mudar_dono(c, UID_SAGUI, -1), "chown a sí mismo de uno de root");
    NEGA(mudar_dono(c, -1, GID_SAGUI), "chgrp de uno de root");
    caminho_juntar(c, sizeof c, base, "r666");
    PODE(escrever_arq(c), "escribir r666 (otros w)");
    if (pegajosa) {
        NEGA(apagar(c), "borrar r666 de root en carpeta pegajosa");
        caminho_juntar(d, sizeof d, base, "movido");
        NEGA(renomear(c, d), "renombrar r666 de root en carpeta pegajosa");
    }
    caminho_juntar(c, sizeof c, base, "p700");
    NEGA(listar_pasta(c), "listar carpeta 0700 de root");
    NEGA(mudar_pasta(c), "entrar en carpeta 0700 de root");
    caminho_juntar(c, sizeof c, base, "p700/dentro");
    NEGA(ler_arq(c), "leer dentro de carpeta 0700 (no se puede entrar)");
    info_arquivo_t inf;
    NEGA(info_arquivo(c, &inf), "info dentro de carpeta 0700");
    caminho_juntar(c, sizeof c, base, "p755");
    PODE(listar_pasta(c), "listar carpeta 0755 de root");
    PODE(mudar_pasta(c), "entrar en carpeta 0755");
    mudar_pasta("/");
    caminho_juntar(c, sizeof c, base, "p755/nuevo");
    NEGA(criar(c, "x"), "crear dentro de carpeta 0755 de root");
    caminho_juntar(c, sizeof c, base, "p755/sub");
    NEGA(criar_pasta(c), "mkdir dentro de carpeta 0755 de root");
    caminho_juntar(c, sizeof c, base, "p755/dentro");
    PODE(ler_arq(c), "leer dentro de carpeta 0755");
    NEGA(apagar(c), "borrar dentro de carpeta 0755 de root");
    caminho_juntar(c, sizeof c, base, "p755");
    NEGA(remover_pasta(c), "rmdir de carpeta de root");
    /* Lo propio: crear, umask, chmod, chgrp, renombrar, borrar. */
    caminho_juntar(c, sizeof c, base, "mio");
    PODE(criar(c, "mío"), "crear lo mío");
    unsigned modo = 0, uid = 0, gid = 0;
    anota(meta(c, &modo, &uid, &gid, NULL) && uid == UID_SAGUI && gid == GID_SAGUI && modo == 0644,
          "%s: de sagui 0644 (%o %u %u)", c, modo, uid, gid);
    PODE(mudar_permisos(c, 0600), "chmod 600 de lo mío");
    anota(meta(c, &modo, NULL, NULL, NULL) && modo == 0600, "chmod quedó (%o)", modo);
    PODE(mudar_permisos(c, 0), "chmod 0 de lo mío");
    NEGA(ler_arq(c), "leer lo mío con 0000");
    PODE(mudar_permisos(c, 0644), "el dueño siempre puede volver a cambiarlo");
    NEGA(mudar_dono(c, UID_ROOT, -1), "regalar lo mío a root");
    PODE(mudar_dono(c, -1, GID_SAGUI), "chgrp a mi grupo");
    NEGA(mudar_dono(c, -1, GID_ROOT), "chgrp a otro grupo");
    caminho_juntar(d, sizeof d, base, "mio2");
    PODE(renomear(c, d), "renombrar lo mío");
    PODE(apagar(d), "borrar lo mío");
    caminho_juntar(c, sizeof c, base, "pmia");
    PODE(criar_pasta(c), "mkdir propio");
    anota(meta(c, &modo, &uid, NULL, NULL) && modo == 0755 && uid == UID_SAGUI, "la carpeta nace 0755 (%o)", modo);
    PODE(remover_pasta(c), "rmdir propio");
    int antes = mascara_criacao(077);
    anota(antes == 022, "umask de serie 022 (%o)", (unsigned)antes);
    caminho_juntar(c, sizeof c, base, "privado");
    PODE(criar(c, "x"), "crear con umask 077");
    anota(meta(c, &modo, NULL, NULL, NULL) && modo == 0600, "con umask 077 nace 0600 (%o)", modo);
    apagar(c);
    caminho_juntar(c, sizeof c, base, "pprivada");
    PODE(criar_pasta(c), "mkdir con umask 077");
    anota(meta(c, &modo, NULL, NULL, NULL) && modo == 0700, "con umask 077 la carpeta nace 0700 (%o)", modo);
    remover_pasta(c);
    mascara_criacao(antes);
    /* Ejecutar: un programa copiado recibe la x; un 0644, no se ejecuta. */
    caminho_juntar(c, sizeof c, base, "prog");
    PODE(copiar_programa(c), "copiar /bin/echo");
    anota(meta(c, &modo, NULL, NULL, NULL) && modo == 0755, "un programa recién copiado nace 0755 (%o)", modo);
    ESPERA(rodar(c), 0, "ejecutar el programa copiado");
    PODE(mudar_permisos(c, 0644), "chmod -x");
    NEGA(rodar(c), "ejecutar un 0644");
    apagar(c);
    caminho_juntar(c, sizeof c, base, "datos");
    PODE(criar(c, "no es un programa"), "crear datos");
    anota(meta(c, &modo, NULL, NULL, NULL) && modo == 0644, "un archivo de datos no recibe la x (%o)", modo);
    apagar(c);
}

static void como_sagui(void)
{
    anota(usuario_op(USU_FIJAR, UID_SAGUI, GID_SAGUI) == 0, "root pasa a sagui");
    anota(meu_uid() == UID_SAGUI, "ahora es sagui (uid %u)", meu_uid());
    /* El ramdisk: /, /etc, /bin, /sistema. */
    NEGA(criar("/nuevo", "x"), "crear en /");
    NEGA(criar_pasta("/nueva"), "mkdir en /");
    NEGA(criar("/etc/nuevo", "x"), "crear en /etc");
    NEGA(escrever_arq("/etc/usuarios"), "escribir /etc/usuarios");
    PODE(ler_arq("/etc/usuarios"), "leer /etc/usuarios");
    NEGA(apagar("/bin/ls"), "borrar /bin/ls");
    NEGA(renomear("/bin/ls", "/tmp/ls"), "mover /bin/ls");
    NEGA(remover_pasta("/etc/servicios"), "rmdir /etc/servicios");
    NEGA(renomear("/etc", "/etc2"), "renombrar /etc");
    ESPERA(rodar("/bin/echo"), 0, "ejecutar /bin/echo");
    NEGA(listar_pasta("/sistema"), "listar /sistema");
    /* D313: /sistema se atraviesa (0711) para leer los catalogos de idioma; lo demas sigue cerrado */
    PODE(mudar_pasta("/sistema/idiomas"), "entrar en /sistema/idiomas");
    mudar_pasta("/");
    PODE(listar_pasta("/sistema/idiomas"), "listar /sistema/idiomas");
    NEGA(listar_pasta("/sistema/arranque"), "listar /sistema/arranque");
    NEGA(mudar_pasta("/sistema/conf"), "entrar en /sistema/conf");
    NEGA(ler_arq("/sistema/conf/LEEME"), "leer /sistema/conf/LEEME (SAGUI-CONF)");
    NEGA(criar("/sistema/idiomas/nuevo", "x"), "crear en /sistema/idiomas");
    NEGA(ler_arq("/etc/sombra"), "leer la sombra");
    PODE(listar_pasta("/etc"), "listar /etc");
    /* Matriz en RAM (/tmp, pegajosa), ext2 (/home/permisos, de sagui; /logs, pegajosa). */
    matriz("/tmp", true);
    matriz("/logs", true);
    /* /home: lo que root creó ahí es de sagui. */
    PODE(escrever_arq("/home/permisos/de-root"), "escribir en /home lo que creó root");
    PODE(apagar("/home/permisos/de-root"), "borrar en /home lo que creó root");
    PODE(criar("/home/mio", "x"), "crear en /home (ext2)");
    unsigned modo = 0, uid = 0, gid = 0, donos = 0;
    anota(meta("/home/mio", &modo, &uid, &gid, &donos) && modo == 0644 && uid == UID_SAGUI && donos,
          "/home/mio: sagui 0644 en ext2 (%o %u %u)", modo, uid, donos);
    apagar("/home/mio");
    /* El grupo: r640 con grupo sagui se lee, no se escribe. */
    PODE(ler_arq("/logs/r-chgrp"), "leer 0640 con mi grupo");
    NEGA(escrever_arq("/logs/r-chgrp"), "escribir 0640 con mi grupo");
    PODE(escrever_arq("/tmp/r-chown"), "escribir lo que root me dio con chown");
    PODE(apagar("/tmp/r-chown"), "borrar lo que root me dio en /tmp");
    /* FAT32 con uid=root,umask=022: se lee, no se escribe. */
    PODE(ler_arq("/fatroot/f"), "leer en /fatroot");
    NEGA(escrever_arq("/fatroot/f"), "escribir en /fatroot");
    NEGA(criar("/fatroot/g", "x"), "crear en /fatroot");
    NEGA(apagar("/fatroot/f"), "borrar en /fatroot");
    NEGA(criar_pasta("/fatroot/d"), "mkdir en /fatroot");
    /* FAT32 por defecto: de sagui. */
    PODE(criar("/fatsagui/mio", "x"), "crear en /fatsagui");
    anota(meta("/fatsagui/mio", &modo, &uid, NULL, &donos) && modo == 0755 && uid == UID_SAGUI && !donos,
          "/fatsagui/mio (%o %u %u)", modo, uid, donos);
    PODE(criar_pasta("/fatsagui/carpeta"), "mkdir en /fatsagui");
    PODE(renomear("/fatsagui/mio", "/fatsagui/carpeta/mio"), "mover en /fatsagui");
    PODE(apagar("/fatsagui/carpeta/mio"), "borrar en /fatsagui");
    PODE(remover_pasta("/fatsagui/carpeta"), "rmdir en /fatsagui");
    ESPERA(mudar_permisos("/fatsagui", 0700), ERRO_NAO_SUPORTADO, "chmod en FAT32 (como dueño)");
    /* De vuelta a root no se puede. */
    NEGA(usuario_op(USU_FIJAR, UID_ROOT, GID_ROOT), "sagui no vuelve a root");
}

int principal(int argc, char **argv)
{
    if (argc >= 3 && compara_texto(argv[1], "--ver") == 0) {
        info_permisos_t p;
        int r = permisos_arquivo(argv[2], &p);
        if (r < 0) { imprimir("permisos: %s error %d\n", argv[2], r); return 1; }
        imprimir("permisos: %s %04o %u %u %u\n", argv[2], p.modo, p.uid, p.gid, p.com_donos);
        return 0;
    }
    if (argc < 2 || compara_texto(argv[1], "--matriz") != 0) {
        imprimir_em(ERROS, "uso: permisos --matriz | --ver RUTA\n");
        return 2;
    }
    como_root();
    como_sagui();
    imprimir("permisos: matriz ok=%d mal=%d\n", ok, mal);
    registrar("permisos: matriz ok=%d mal=%d", ok, mal);
    return mal ? 1 : 0;
}
