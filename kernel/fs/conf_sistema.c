// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_sistema.c - la configuración persistente del sistema (D255, opción A del mantenedor).
 *
 * La partición SAGUI-CONF (ext2, 16 MB, en el disco de arranque) se monta por
 * el /etc/fstab en CONF_PONTO (/sistema/conf). Lo que tiene en etc/ se copia al
 * arrancar sobre el /etc de la RAM (el del ramdisk): usuarios, sombra, claves
 * SSH de root y del servidor, contraseña del VNC, .conf de servicios... Así
 * todos los programas siguen leyendo /etc como siempre, y quien cambia algo
 * del sistema lo escribe en los dos sitios (libsagui/conf_sistema.c).
 *
 * Copiar (y no montar encima de /etc) es lo más robusto: si la partición falta,
 * está dañada o tiene un archivo raro, /etc sigue siendo el del ramdisk y el
 * sistema arranca igual («modo en vivo»: root sin contraseña y solo desde la
 * sesión local). Límites: 8 niveles de carpetas, 256 KB por archivo, 4 MB en
 * total; lo que se pase se salta y se anota.
 *
 * Quien no es root no puede leer ni escribir nada bajo /sistema (permisos.c). */
#include "conf_sistema.h"
#include "console.h"
#include "memoria.h"
#include "montagem.h"
#include "texto.h"
#include "vfs.h"
#include "gui.h"
#include "bootinfo.h"
#include "raiz.h"

#define ARQ_MAX    (256u * 1024u)
#define TOTAL_MAX  (4u * 1024u * 1024u)
#define NIVEIS_MAX 8

static uint32_t copiados_bytes;
static int copiados, saltados;

static bool listar(const char *caminho, int i, entrada_pasta_t *e)
{
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) return m->fs->listar(m->priv, rel, i, e) == 1;
    return vfs_listar(caminho, i, e) == 1;
}

/* D282: lo copiado a /etc es siempre de root; de SAGUI-CONF solo se toman los
 * permisos, sin escritura para el grupo ni los otros (un archivo de otro dueño
 * en la partición no puede dar a nadie la configuración del sistema). Los
 * secretos, 0600 aunque en la partición estén mal. */
static uint16_t modo_de(bool pasta, const char *de, const char *para)
{
    meta_t m;
    uint16_t modo = pasta ? 0755 : 0644;
    if (vfs_meta(de, &m) == 0) modo = (uint16_t)(m.modo & 0755);
    if (pasta) modo |= 0700;
    if (txt_comeca_com(para, "/etc/sombra") || txt_comeca_com(para, "/etc/ssh/ssh_host_") ||
        txt_comeca_com(para, "/etc/vnc/vncd.passwd"))
        modo &= 0700;
    return modo;
}

static void fixar_meta(no_t *x, const char *de, const char *para)
{
    x->uid = UID_ROOT;
    x->gid = GID_ROOT;
    x->modo = modo_de(x->tipo == TIPO_PASTA, de, para);
}

/* D379: en la live la raíz es la sobreposición (escribible): la copia va a su /etc, no al árbol en RAM. */
static bool na_raiz_montada(void)
{
    montagem_t *r = montagem_da_raiz();
    return r && !(r->opcoes & MONTAR_SO_LEITURA);
}

static void copiar_arquivo(const char *de, const char *para)
{
    uint8_t *dados;
    uint32_t tam;
    if (vfs_ler_inteiro(de, &dados, &tam) < 0) { saltados++; klog("[CONF] no se puede leer %s\n", de); return; }
    if (tam > ARQ_MAX || copiados_bytes + tam > TOTAL_MAX) {
        kfree(dados);
        saltados++;
        klog("[CONF] %s: demasiado grande (%u bytes): se queda el del ramdisk\n", de, tam);
        return;
    }
    if (na_raiz_montada()) {
        char tmp[CAMINHO_MAX];
        snformatar(tmp, sizeof tmp, "%s.conf-tmp", para);
        int r = escrever_atomico(para, tmp, dados, tam);
        kfree(dados);
        if (r < 0) { saltados++; klog("[CONF] no se puede escribir %s (%d)\n", para, r); return; }
        vfs_fixar_meta(para, UID_ROOT, GID_ROOT, modo_de(false, de, para));
        copiados++;
        copiados_bytes += tam;
        return;
    }
    no_t *x = vfs_achar(para);
    if (x && x->tipo == TIPO_PASTA) { kfree(dados); saltados++; return; }
    if (!x && vfs_criar(para, TIPO_ARQUIVO, &x) < 0) { kfree(dados); saltados++; return; }
    vfs_truncar(x);
    if (tam) vfs_escrever(x, 0, dados, tam);
    fixar_meta(x, de, para);
    kfree(dados);
    copiados++;
    copiados_bytes += tam;
}

static void copiar_pasta(const char *de, const char *para, int nivel)
{
    if (nivel > NIVEIS_MAX) return;
    if (na_raiz_montada()) {   /* D379: la live */
        meta_t m;
        if (vfs_meta(para, &m) < 0) {
            const char *rel;
            montagem_t *mt = montagem_de(para, &rel);
            if (!mt || mt->fs->criar_pasta(mt->priv, rel) < 0) return;
        } else if (m.tipo != TIPO_PASTA) return;
        vfs_fixar_meta(para, UID_ROOT, GID_ROOT, modo_de(true, de, para));
    } else {
        no_t *p = vfs_achar(para);
        if (!p && vfs_criar(para, TIPO_PASTA, &p) < 0) return;
        if (p->tipo != TIPO_PASTA) return;
        fixar_meta(p, de, para);
    }
    entrada_pasta_t e;
    for (int i = 0; i < 512 && listar(de, i, &e); i++) {
        if (txt_compara(e.nome, ".") == 0 || txt_compara(e.nome, "..") == 0 || txt_compara(e.nome, "lost+found") == 0)
            continue;
        if (txt_tamanho(de) + txt_tamanho(e.nome) + 2 > CAMINHO_MAX ||
            txt_tamanho(para) + txt_tamanho(e.nome) + 2 > CAMINHO_MAX) continue;
        size_t nt = txt_tamanho(e.nome);
        if (nt > 4 && txt_compara(e.nome + nt - 4, ".tmp") == 0) continue;   /* una escritura a medias */
        char a[CAMINHO_MAX], b[CAMINHO_MAX];
        txt_copia_n(a, de, sizeof a);
        txt_copia_n(a + txt_tamanho(a), "/", sizeof a - txt_tamanho(a));
        txt_copia_n(a + txt_tamanho(a), e.nome, sizeof a - txt_tamanho(a));
        txt_copia_n(b, para, sizeof b);
        txt_copia_n(b + txt_tamanho(b), "/", sizeof b - txt_tamanho(b));
        txt_copia_n(b + txt_tamanho(b), e.nome, sizeof b - txt_tamanho(b));
        if (e.tipo == TIPO_PASTA) copiar_pasta(a, b, nivel + 1);
        else copiar_arquivo(a, b);
    }
}

/* D379: con la raíz en un disco con escritura (instalado), SAGUI-CONF se funde en ella:
 * /etc y /root ya son persistentes y CONF_PONTO es una carpeta más de la raíz. */
bool conf_sistema_na_raiz(void)
{
    const char *rel;
    montagem_t *m = montagem_de(CONF_PONTO, &rel);
    return m && m == montagem_da_raiz() && raiz_estado() == RAIZ_DISCO && !(m->opcoes & MONTAR_SO_LEITURA);
}

static bool conf_na_raiz(void) { return conf_sistema_na_raiz(); }

bool conf_sistema_montada(void)
{
    const char *rel;
    montagem_t *m = montagem_de(CONF_PONTO, &rel);
    if (m && m == montagem_da_raiz()) return conf_na_raiz();
    return m && !*rel && !(m->opcoes & MONTAR_SO_LEITURA);
}

bool conf_sistema_separada(void)
{
    return conf_sistema_montada() && vfs_existe_arquivo("/etc/sistema.conf");
}

int conf_sistema_gravar(const char *rel, const void *dados, uint32_t n)
{
    char ram[CAMINHO_MAX], tmp[CAMINHO_MAX];
    snformatar(ram, sizeof ram, "/etc/%s", rel);
    snformatar(tmp, sizeof tmp, "/etc/%s.nuevo", rel);
    int r = escrever_atomico(ram, tmp, dados, n);
    if (conf_sistema_montada() && !conf_na_raiz()) {   /* D379: en la raíz en disco, /etc ya es la copia buena */
        snformatar(ram, sizeof ram, CONF_PONTO "/etc/%s", rel);
        snformatar(tmp, sizeof tmp, CONF_PONTO "/etc/%s.nuevo", rel);
        int rd = escrever_atomico(ram, tmp, dados, n);
        if (rd < 0) r = rd;
    }
    return r;
}

static bool raiz_persistente;

bool conf_sistema_raiz_persistente(void) { return raiz_persistente; }

void conf_sistema_raiz_preparar(void)
{
    /* D379: en la live sin SAGUI-CONF, /root está en la capa escribible de la raíz. */
    if (na_raiz_montada() && !conf_na_raiz() && !conf_sistema_montada()) {
        meta_t m;
        if (vfs_meta("/root", &m) < 0) {
            const char *rel;
            montagem_t *mt = montagem_de("/root", &rel);
            if (mt) mt->fs->criar_pasta(mt->priv, rel);
        }
        vfs_fixar_meta("/root", 0, 0, 0700);
        raiz_persistente = false;
        klog("[CONF] /root en la capa de la raiz (sin SAGUI-CONF)\n");
        return;
    }
    /* D379: con la raíz en disco, /root es una carpeta de verdad de ese disco (sin alias). */
    if (conf_na_raiz()) {
        meta_t m;
        if (vfs_meta("/root", &m) < 0) {
            const char *rel;
            montagem_t *mt = montagem_de("/root", &rel);
            if (mt && mt->fs->criar_pasta) mt->fs->criar_pasta(mt->priv, rel);
        }
        vfs_fixar_meta("/root", 0, 0, 0700);
        raiz_persistente = false;
        klog("[CONF] /root en la raiz del disco (SAGUI-CONF fundida en la raiz)\n");
        return;
    }
    no_t *x = vfs_achar("/root");
    if (!x && vfs_criar("/root", TIPO_PASTA, &x) < 0) x = NULL;
    if (x) { x->uid = 0; x->gid = 0; x->modo = 0700; }
    raiz_persistente = false;
    if (!conf_sistema_montada()) { klog("[CONF] /root en la RAM (sin SAGUI-CONF)\n"); return; }
    meta_t m;
    if (vfs_meta(CONF_RAIZ, &m) < 0) {
        const char *rel;
        montagem_t *mt = montagem_de(CONF_RAIZ, &rel);
        if (mt && mt->fs->criar_pasta) mt->fs->criar_pasta(mt->priv, rel);
        vfs_fixar_meta(CONF_RAIZ, 0, 0, 0700);
    }
    raiz_persistente = vfs_meta(CONF_RAIZ, &m) == 0 && m.tipo == TIPO_PASTA;
    klog("[CONF] /root en SAGUI-CONF (%s): %s\n", CONF_RAIZ, raiz_persistente ? "si" : "no, se queda en la RAM");
}

bool conf_sistema_inicio_automatico(void)
{
    if (!conf_sistema_montada() || (cabecalho_boot[CAB_USU_OPC] & CAB_USU_SIN_PEDIR_ROOT)) return true;
    uint8_t *d;
    uint32_t n;
    if (vfs_ler_inteiro("/etc/sistema.conf", &d, &n) < 0) return true;
    bool sim = true;
    const char *p = (const char *)d, *fim = p + n;
    while (p < fim) {
        const char *l = p;
        while (p < fim && *p != '\n') p++;
        if (p - l >= 20 && txt_compara_n(l, "inicio_automatico=no", 20) == 0) sim = false;
        if (p < fim) p++;
    }
    kfree(d);
    return sim;
}

void conf_sistema_home_reavaliar(void)
{
    uint8_t *d;
    uint32_t n;
    bool feita = false;
    if (conf_sistema_montada() && vfs_ler_inteiro("/etc/migracion-home", &d, &n) == 0) {
        feita = n >= 5 && txt_compara_n((const char *)d, "hecha", 5) == 0;
        kfree(d);
    }
    if (feita != vfs_home_e_de_root()) klog("[CONF] /home: %s\n", feita ? "de root (cada cuenta en /home/<nombre>)" : "de sagui");
    vfs_home_de_root(feita);
}

void conf_sistema_superpor(void)
{
    if (!conf_sistema_montada()) {
        klog("[CONF] sin la particion SAGUI-CONF: modo en vivo (la configuracion del sistema no se guarda)\n");
        return;
    }
    copiar_pasta(CONF_PONTO "/etc", "/etc", 0);
    klog("[CONF] SAGUI-CONF en " CONF_PONTO ": %d archivo(s) (%u bytes) sobre /etc%s\n", copiados, copiados_bytes,
         saltados ? ", algunos saltados" : "");
}
