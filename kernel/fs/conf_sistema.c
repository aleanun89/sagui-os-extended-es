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
    no_t *x = vfs_achar(para);
    if (x && x->tipo == TIPO_PASTA) { kfree(dados); saltados++; return; }
    if (!x && vfs_criar(para, TIPO_ARQUIVO, &x) < 0) { kfree(dados); saltados++; return; }
    vfs_truncar(x);
    if (tam) vfs_escrever(x, 0, dados, tam);
    kfree(dados);
    copiados++;
    copiados_bytes += tam;
}

static void copiar_pasta(const char *de, const char *para, int nivel)
{
    if (nivel > NIVEIS_MAX) return;
    no_t *p = vfs_achar(para);
    if (!p && vfs_criar(para, TIPO_PASTA, NULL) < 0) return;
    if (p && p->tipo != TIPO_PASTA) return;
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

bool conf_sistema_montada(void)
{
    const char *rel;
    montagem_t *m = montagem_de(CONF_PONTO, &rel);
    return m && !*rel && !(m->opcoes & MONTAR_SO_LEITURA);
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
