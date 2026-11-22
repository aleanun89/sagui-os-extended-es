// SPDX-License-Identifier: GPL-2.0-or-later
/* casa_migrar.c - la cuenta principal (sagui) pasa de tener /home entero a su carpeta /home/sagui (D371, fase 4).
 *
 * Lo hace «configuracion --aplicar» como root al arrancar, una vez, solo con SAGUI-CONF y un /home persistente que
 * guarda dueños (ext2). Sin copiar nada: cada archivo y carpeta de /home se RENOMBRA (en el mismo sistema de
 * archivos) dentro de /home/sagui.nuevo, que al final se renombra a /home/sagui; luego /etc/usuarios dice
 * /home/sagui. Nada se borra.
 *
 * Reanudable: /etc/migracion-home (en SAGUI-CONF) dice «empezada» antes del primer renombrado y «hecha» al
 * final. Si se corta a medias, el siguiente arranque ve «empezada» y sigue moviendo lo que quede (los
 * renombrados ya hechos no se repiten: lo que ya está dentro no está en /home). Las carpetas de las demás
 * cuentas, lost+found y /home/compartido no se tocan. Con /etc/migracion-home «hecha», el kernel pasa /home a root
 * (conf_sistema_home_reavaliar). */
#include "sagui.h"
#include "contas.h"
#include "conf_maquina.h"

#define MARCA     "/etc/migracion-home"
#define MAX_ENTR  512

static int ler_marca(char *t, int max)
{
    int fd = abrir(MARCA, ABRIR_LER);
    if (fd < 0) { t[0] = 0; return -1; }
    int n = ler(fd, t, (unsigned)(max - 1));
    fechar(fd);
    t[n > 0 ? n : 0] = 0;
    return n;
}

/* ¿/home es un montaje con escritura que guarda dueños (ext2)? En la RAM o en FAT32, no se migra. */
static bool home_persistente_com_donos(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    bool montado = false;
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) montado = true;
    info_permisos_t p;
    return montado && permisos_arquivo("/home", &p) == 0 && p.com_donos;
}

/* ¿"nome" (en /home) es la carpeta de otra cuenta? */
static bool casa_de_otra(const conta_t *v, int n, const char *nome, unsigned eu)
{
    char c[96];
    formatar_texto(c, sizeof c, "/home/%s", nome);
    for (int i = 0; i < n; i++)
        if (v[i].uid != eu && compara_texto(v[i].casa, c) == 0) return true;
    return false;
}

/* Las claves de la máquina que iban en /home (la del servidor SSH y la contraseña del VNC). sshd y vncd las copian a
 * SAGUI-CONF la primera vez (D255) pero dejaban la de /home, y al pasar /home a la carpeta de sagui, sagui las
 * leería (la protección de permisos.c es por nombre: /home/.ssh/...). Antes de mover nada: si no están ya en /etc
 * se copian (0600, conf_gravar) y la de /home se borra. false = no se pudo copiar (no se sigue: se reintenta). */
static bool segredo_fora(const char *de, const char *rel)
{
    info_arquivo_t ia;
    if (info_arquivo(de, &ia) != 0) return true;
    char etc[96];
    formatar_texto(etc, sizeof etc, "/etc/%s", rel);
    if (info_arquivo(etc, &ia) != 0) {
        static char t[512];
        int fd = abrir(de, ABRIR_LER), n = -1;
        if (fd >= 0) { n = ler(fd, t, sizeof t); fechar(fd); }
        int r = n > 0 && n < (int)sizeof t ? conf_gravar(rel, t, n) : ERRO_INVALIDO;
        memset(t, 0, sizeof t);
        if (r < 0) { registrar("casa_migrar: no se pudo pasar %s a %s (%s): no se migra", de, etc, nome_erro(r)); return false; }
    }
    int r = apagar(de);
    registrar("casa_migrar: %s quitado de /home (la de la máquina está en %s): %d", de, etc, r);
    return true;
}

static bool segredos_fora(const char *base)
{
    char c[128];
    formatar_texto(c, sizeof c, "%s/.ssh/ssh_host_ed25519_key", base);
    if (!segredo_fora(c, "ssh/ssh_host_ed25519_key")) return false;
    formatar_texto(c, sizeof c, "%s/.config/vncd.passwd", base);
    return segredo_fora(c, "vnc/vncd.passwd");
}

bool casa_real_de_uid(unsigned uid, char *casa, int max)
{
    conta_t c;
    if (!conta_por_uid(uid, &c) || !c.casa[0]) { copia_texto(casa, uid == 0 ? "/root" : "/home", (size_t)max); return false; }
    copia_texto(casa, c.casa, (size_t)max);
    return true;
}

int casa_migrar_sagui(void)
{
    if (!sou_root() || !conf_persistente()) return 0;
    if (usuario_op(USU_ESTADO, 0, 0) & USU_EST_SIN_MIGRAR) return 0;
    char marca[32];
    ler_marca(marca, sizeof marca);
    if (comeca_com(marca, "hecha")) return 0;
    bool empezada = comeca_com(marca, "empezada");
    static conta_t v[CONTAS_MAX];
    int nv = contas_ler(v, CONTAS_MAX, NULL);
    int ip = -1;
    for (int i = 0; i < nv; i++) if (v[i].uid == UID_SAGUI) ip = i;
    if (ip < 0) return 0;
    /* Ya tiene su carpeta (el instalador con otro nombre que sagui): no hay nada que mover, pero /home pasa igual a
     * root, con /home/compartido, como tras migrar (si no, /home seguiría siendo de la cuenta principal). */
    bool ja_tem = compara_texto(v[ip].casa, "/home") != 0 && !empezada;
    if (!home_persistente_com_donos()) {
        if (!ja_tem) registrar("casa_migrar: /home no es persistente o no guarda dueños: %s sigue con /home entero", v[ip].nome);
        return 0;
    }
    info_arquivo_t ia;
    char dest[96], novo[104];
    int r;
    formatar_texto(dest, sizeof dest, "/home/%s", v[ip].nome);
    formatar_texto(novo, sizeof novo, "%s.nuevo", dest);
    if (ja_tem) {
        copia_texto(dest, v[ip].casa, sizeof dest);
        registrar("casa_migrar: %s ya tiene su carpeta (%s): solo /home pasa a root", v[ip].nome, dest);
        goto final;
    }
    if (!empezada) {
        if (info_arquivo(dest, &ia) == 0) {
            registrar("casa_migrar: ya existe %s y no la creó la migración: no se toca nada", dest);
            return ERRO_JA_EXISTE;
        }
        r = conf_gravar("migracion-home", "empezada\n", 9);
        if (r < 0) { registrar("casa_migrar: no se puede escribir la marca en SAGUI-CONF (%s)", nome_erro(r)); return r; }
    }
    registrar("casa_migrar: %s pasa de /home a %s%s", v[ip].nome, dest, empezada ? " (se sigue lo empezado)" : "");
    if (info_arquivo(dest, &ia) != 0) {   /* si ya existe, el corte fue después del último renombrado */
        if (!segredos_fora("/home") || !segredos_fora(novo)) return ERRO_ACESSO;   /* las claves de la máquina, fuera */
        if (info_arquivo(novo, &ia) != 0) {
            r = criar_pasta(novo);
            if (r < 0) { registrar("casa_migrar: no se puede crear %s (%s)", novo, nome_erro(r)); return r; }
        } else if (ia.tipo != TIPO_PASTA) {   /* un archivo con ese nombre (lo pudo poner sagui): no se toca nada */
            registrar("casa_migrar: %s existe y no es una carpeta: no se migra", novo);
            return ERRO_JA_EXISTE;
        }
        mudar_dono(novo, UID_SAGUI, GID_SAGUI);
        mudar_permisos(novo, 0700);
        const char *bn = novo + 6, *bd = dest + 6;
        int movidos = 0, erros = 0;
        /* D462: 32 KB del malloc solo mientras se migra (antes en la BSS, que pagaba cada programa que usa
         * libsagui.so.1 aunque nunca migre). */
        char (*nomes)[NOME_MAX] = aloca(sizeof(char[MAX_ENTR][NOME_MAX]));
        if (!nomes) { registrar("casa_migrar: sin memoria para la lista de /home"); return ERRO_SEM_MEMORIA; }
        for (int vuelta = 0; vuelta < 64; vuelta++) {   /* los nombres primero: renombrar cambia la lista */
            int k = 0;
            entrada_pasta_t e;
            for (int i = 0; k < MAX_ENTR && listar("/home", i, &e) > 0; i++) {
                if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0 || compara_texto(e.nome, "lost+found") == 0 ||
                    compara_texto(e.nome, bn) == 0 || compara_texto(e.nome, bd) == 0 ||
                    casa_de_otra(v, nv, e.nome, UID_SAGUI))
                    continue;
                if (e.tipo == TIPO_PASTA && e.nome[0] != '.') {   /* la carpeta conservada de una cuenta borrada (D340) */
                    char c[96];
                    info_permisos_t p;
                    formatar_texto(c, sizeof c, "/home/%s", e.nome);
                    if (permisos_arquivo(c, &p) == 0 && p.uid != UID_SAGUI && p.uid != UID_ROOT) continue;
                }
                copia_texto(nomes[k++], e.nome, NOME_MAX);
            }
            if (!k) break;
            int antes = movidos;
            for (int i = 0; i < k; i++) {
                char de[NOME_MAX + 8], para[NOME_MAX + 112];
                formatar_texto(de, sizeof de, "/home/%s", nomes[i]);
                formatar_texto(para, sizeof para, "%s/%s", novo, nomes[i]);
                r = renomear(de, para);
                if (r < 0) { erros++; registrar("casa_migrar: no se pudo mover %s (%s): se queda en /home", de, nome_erro(r)); }
                else movidos++;
            }
            if (movidos == antes) break;   /* nada más se puede mover */
        }
        libera(nomes);
        registrar("casa_migrar: %d entrada(s) movidas a %s, %d no", movidos, novo, erros);
        r = renomear(novo, dest);
        if (r < 0) { registrar("casa_migrar: no se pudo renombrar %s (%s): se reintenta al arrancar", novo, nome_erro(r)); return r; }
    }
    mudar_dono(dest, UID_SAGUI, GID_SAGUI);
    mudar_permisos(dest, 0700);
    copia_texto(v[ip].casa, dest, sizeof v[ip].casa);
    r = contas_gravar(v, nv);
    if (r < 0) { registrar("casa_migrar: no se pudo escribir /etc/usuarios (%s): se reintenta al arrancar", nome_erro(r)); return r; }
final:
    /* P8: una carpeta común para compartir entre cuentas (como /tmp: cada uno borra lo suyo). Se crea después de
     * mover (una «compartido» que tuviera sagui antes es suya y pasó a su carpeta). */
    if (info_arquivo("/home/compartido", &ia) != 0) criar_pasta("/home/compartido");
    if (info_arquivo("/home/compartido", &ia) == 0 && ia.tipo == TIPO_PASTA) {
        mudar_dono("/home/compartido", UID_ROOT, GID_ROOT);
        mudar_permisos("/home/compartido", 01777);
    } else registrar("casa_migrar: /home/compartido no es una carpeta: no se crea la común");
    mudar_dono("/home", UID_ROOT, GID_ROOT);
    mudar_permisos("/home", 0755);
    r = conf_gravar("migracion-home", "hecha\n", 6);
    registrar("casa_migrar: hecha: %s en %s (%d)", v[ip].nome, dest, r);
    return r < 0 ? r : 1;
}

/* D380: la carpeta de los modelos de IA local, /home/compartido/modelos, de root con 1777 (pegajosa, como /tmp):
 * cualquier cuenta deja ahí sus modelos, pero nadie puede borrar ni sustituir los de otro. La crea root al
 * arrancar (Configuración --aplicar), solo si /home/compartido existe y es de root; si ya hay una
 * «modelos» que no es de root (la creó otra cuenta antes), se deja y se avisa en el registro. */
int casa_pasta_modelos(void)
{
    if (!sou_root()) return 0;
    info_permisos_t p;
    if (permisos_arquivo("/home/compartido", &p) != 0 || p.tipo != TIPO_PASTA || p.uid != UID_ROOT) return 0;
    if (permisos_arquivo("/home/compartido/modelos", &p) != 0) {
        int r = criar_pasta("/home/compartido/modelos");
        if (r < 0) return r;
        mudar_dono("/home/compartido/modelos", UID_ROOT, GID_ROOT);
        mudar_permisos("/home/compartido/modelos", 01777);
        return 1;
    }
    if (p.tipo != TIPO_PASTA || p.uid != UID_ROOT) {
        registrar("casa_pasta_modelos: /home/compartido/modelos no es de root: no se toca (la IA local no elige solos sus modelos ajenos)");
        return 0;
    }
    if ((p.modo & 01777) != 01777) mudar_permisos("/home/compartido/modelos", 01777);
    return 0;
}
