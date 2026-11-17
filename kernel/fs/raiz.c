// SPDX-License-Identifier: GPL-2.0-or-later
/* raiz.c - la raíz del sistema en un disco (D379, prototipo de la mejora 035 F2).
 *
 * Hasta ahora el ramdisk SGFS era siempre la raíz: se copia entero a la RAM al
 * arrancar. Con este paso el ramdisk puede ser un *initramfs* pequeño (lo justo
 * para arrancar y para el modo rescate) que dice, en /etc/raiz.conf, dónde está la
 * raíz de verdad:
 *
 *   raiz=LABEL=SAGUI-RAIZ     el volumen (LABEL=, un nombre de «discos» o "arranque")
 *   modo=rw                   rw (por defecto) o ro
 *   espera=10                 segundos que se espera al volumen si aún no está (USB lento)
 *   capa=auto                 live: auto (SAGUI-PERSIST si está en el disco de arranque; si no,
 *                             la RAM), ram, o el volumen de la capa persistente
 *
 * Si el fichero no existe (las imágenes de siempre), no cambia nada: el árbol en
 * RAM sigue siendo la raíz. Si existe y el volumen se monta, "/" pasa a ser ese
 * volumen (montagem.c lo trata como el montaje de prefijo más corto) y todo lo
 * demás (fstab, programas, configuración) se lee de él. Si no se puede montar,
 * se sigue con el initramfs: es el modo rescate, con un aviso en pantalla.
 *
 * Seguridad: LABEL=SAGUI-* solo vale en el disco del que arrancó la máquina
 * (discos.c, irmao_de_boot: la cabecera del LBA 1 igual a la del arranque), así
 * que un pendrive ajeno con la misma etiqueta no se toma como raíz. */
#include "console.h"
#include "cpu.h"
#include "disco.h"
#include "memoria.h"
#include "montagem.h"
#include "relogio.h"
#include "texto.h"
#include "vfs.h"
#include "bootinfo.h"
#include "permisos.h"
#include "ramfs.h"
#include "sobreposicao.h"
#include "disco_ram.h"
#include "raiz.h"
#include "versao.h"

#define RAIZ_CONF "/etc/raiz.conf"
/* D379 (M1): la marca de versión que mkimagem.py (y el instalador, lo último que copia) dejan
 * en la raíz; y el punto donde se monta un momento, de solo lectura, para comprobarla. */
#define RAIZ_VERSAO     "/sistema/raiz/versao"
#define RAIZ_VERIFICAR  "/raiz-verificar"

static raiz_estado_t estado = RAIZ_NA_RAM;

static char capa_txt[48];   /* "capa=" de raiz.conf: "ram" o el volumen persistente (L2) */

/* Live (L1/L2): la raíz del medio de solo lectura con la capa escribible encima (sobreposicao.c).
 * Toram (T1): todavía no; se monta como la live. */
static int raiz_montar_especial(disco_t *d, const char *modo)
{
    if (txt_compara(modo, "toram") == 0) {
        /* T1: la raíz entera a un disco en RAM (ram0) y el mismo ext2 encima, con escritura (en la RAM). */
        console_cor(COR_CINZA, COR_PRETO);
        kprintf("Copiando el sistema a la RAM (%u MB)...\n", d->setores / 2048);
        int e;
        disco_t *r0 = disco_ram_copiar(d, &e);
        disco_t *v = r0 ? (r0->volume ? r0->volume : r0) : NULL;
        if (v && txt_comeca_com(v->tipo_fs, "ext")) {
            int r = montagem_montar_dono(v, "/", v->tipo_fs, MONTAR_RAIZ_INTERNO | MONTAR_LEITURA_ESCRITA, 0);
            if (r >= 0) { estado = RAIZ_TORAM; return r; }
            klog("[RAIZ] montar %s en / fallo (%d): se sigue como live desde el medio\n", v->nome, r);
        } else {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("[aviso] El sistema no cabe en la RAM (%s): se arranca leyendo del medio; no lo saques.\n",
                    e == ERRO_SEM_MEMORIA ? "poca memoria" : "error al copiarlo");
            console_cor(COR_CINZA, COR_PRETO);
        }
    }
    disco_t *p = NULL;
    if (capa_txt[0] && txt_compara(capa_txt, "ram") != 0) {
        /* D379 (M1): «auto» (las imágenes de serie): SAGUI-PERSIST si la imagen la lleva; si no, la RAM,
         * sin aviso (es lo normal en la live sin persistencia). */
        bool automatica = txt_compara(capa_txt, "auto") == 0;
        p = disco_resolver(automatica ? "LABEL=SAGUI-PERSIST" : capa_txt);
        if (p && !p->pai && p->volume != p) p = p->volume;
        if (!p || !txt_comeca_com(p->tipo_fs, "ext") || p->montagem || p == d) {
            if (automatica && !p) {
                klog("[RAIZ] sin SAGUI-PERSIST: la capa va en la RAM\n");
            } else {
                klog("[RAIZ] la capa %s no esta o no vale: la capa va en la RAM\n", capa_txt);
                console_cor(COR_AMARELO, COR_PRETO);
                kprintf("[aviso] No se encuentra la capa persistente (%s): lo que cambies se perderá al apagar.\n",
                        automatica ? "SAGUI-PERSIST" : capa_txt);
                console_cor(COR_CINZA, COR_PRETO);
            }
            p = NULL;
        }
    }
    sobreposicao_preparar(p);
    int r = montagem_montar_dono(d, "/", "sobreposicao", MONTAR_RAIZ_INTERNO | MONTAR_LEITURA_ESCRITA, 0);
    if (r >= 0) {
        estado = sobreposicao_persistente() ? RAIZ_LIVE_PERSISTENTE : RAIZ_LIVE;
        if (p && sobreposicao_persistente()) p->montagem = montagem_da_raiz();   /* nadie más la monta */
    }
    return r;
}

raiz_estado_t raiz_estado(void) { return estado; }

/* Fase R3: con la raíz en disco, /tmp en la RAM (si el fstab no puso SAGUI-TMP) y el
 * initramfs visible, de solo lectura, en /initrd (para el rescate y para mirar qué arrancó). */
void raiz_pos_fstab(void)
{
    if (!montagem_da_raiz()) return;
    const char *rel;
    if (montagem_de("/tmp", &rel) == montagem_da_raiz()) {
        int r = ramfs_montar("/tmp", "/tmp", "tmpfs", 0);
        klog("[RAIZ] /tmp en la RAM (ramfs): %d\n", r);
    }
    int r = ramfs_montar("/initrd", "/", "initrd", MONTAR_SO_LEITURA);
    klog("[RAIZ] el initramfs en /initrd: %d\n", r);
}

/* Valor de "chave=" en el texto (línea entera, sin espacios al final). */
static bool valor(const char *txt, const char *chave, char *saida, size_t max)
{
    size_t n = txt_tamanho(chave);
    for (const char *l = txt; *l; ) {
        while (*l == ' ' || *l == '\t') l++;
        if (txt_compara_n(l, chave, n) == 0 && l[n] == '=') {
            const char *v = l + n + 1;
            size_t k = 0;
            while (v[k] && v[k] != '\n' && v[k] != '\r' && v[k] != ' ' && v[k] != '\t' && v[k] != '#') k++;
            if (k >= max) k = max - 1;
            memcpy(saida, v, k);
            saida[k] = 0;
            return k > 0;
        }
        while (*l && *l != '\n') l++;
        if (*l) l++;
    }
    return false;
}

/* La raíz pedida no se puede usar: se sigue con el initramfs como en el modo rescate
 * (D255): consola de root, sin escritorio ni gestor de servicios (no están en el initramfs). */
static void aviso(const char *msg, const char *disp)
{
    console_cor(COR_AMARELO, COR_PRETO);
    kprintf("[aviso] %s (%s): se sigue con el sistema mínimo en RAM (modo rescate).\n", msg, disp);
    console_cor(COR_CINZA, COR_PRETO);
    info_boot.opcoes |= OPC_BOOT_RESCATE;
    permisos_iniciar();   /* vuelve a leer la opción: la sesión pasa a root */
}

/* D379 (M1): antes de usar un volumen como raíz se monta un momento, de solo lectura, y se mira que
 * tenga lo esencial (/bin/sagsh y /etc) y la marca de versión de este núcleo. Una raíz de otra versión
 * (un disco instalado con otra, una imagen mezclada) o a medias (una copia cortada: el instalador
 * escribe la marca lo último) no se usa: se sigue en el modo rescate, nunca con un sistema mezclado. */
static bool raiz_valida(disco_t *d, char *motivo, size_t max)
{
    int r = montagem_montar_dono(d, RAIZ_VERIFICAR, d->tipo_fs, MONTAR_SO_LEITURA, 0);
    if (r < 0 || !d->montagem) {
        snformatar(motivo, max, "no se puede leer (error %d)", r);
        return false;
    }
    bool ok = false;
    meta_t m;
    uint8_t *txt = NULL;
    uint32_t n = 0;
    if (!vfs_existe_arquivo(RAIZ_VERIFICAR "/bin/sagsh")) {
        snformatar(motivo, max, "le falta /bin/sagsh");
    } else if (vfs_meta(RAIZ_VERIFICAR "/etc", &m) < 0 || m.tipo != TIPO_PASTA) {
        snformatar(motivo, max, "le falta /etc");
    } else if (vfs_ler_inteiro(RAIZ_VERIFICAR RAIZ_VERSAO, &txt, &n) < 0) {
        snformatar(motivo, max, "no tiene la marca de versión (" RAIZ_VERSAO ")");
    } else {
        char v[32];
        uint32_t k = 0;
        while (k < n && k < sizeof v - 1 && txt[k] != '\n' && txt[k] != '\r' && txt[k] != ' ') { v[k] = (char)txt[k]; k++; }
        v[k] = 0;
        if (txt_compara(v, SAGUI_VERSAO) != 0)
            snformatar(motivo, max, "es de la versión %s y este núcleo es %s", v[0] ? v : "?", SAGUI_VERSAO);
        else
            ok = true;
    }
    if (txt) kfree(txt);
    montagem_desmontar(d->montagem, false);
    return ok;
}

bool raiz_montar(void)
{
    uint8_t forcado = cabecalho_boot[CAB_RAIZ_OPC];   /* D379: la opción de la cabecera manda sobre raiz.conf */
    /* Solo el initramfs trae este fichero; se lee del árbol en RAM, antes de montar nada en "/". */
    no_t *x = vfs_achar(RAIZ_CONF);
    if (!x || x->tipo == TIPO_PASTA || montagem_da_raiz()) {
        if (forcado != RAIZ_OPC_AUTO) klog("[RAIZ] modo %u pedido en la cabecera, pero el ramdisk no es un initramfs\n", forcado);
        return false;
    }
    char *txt = kmalloc(x->tamanho + 1);
    if (!txt) return false;
    uint32_t n = vfs_ler(x, 0, txt, x->tamanho);
    txt[n] = 0;
    char disp[48] = "", modo[8] = "rw", espera_txt[8] = "10";
    valor(txt, "raiz", disp, sizeof disp);
    valor(txt, "modo", modo, sizeof modo);
    valor(txt, "espera", espera_txt, sizeof espera_txt);
    valor(txt, "capa", capa_txt, sizeof capa_txt);
    kfree(txt);
    if (forcado == RAIZ_OPC_RAM) {
        klog("[RAIZ] la cabecera pide quedarse en el initramfs\n");
        aviso("Arranque pedido sin la raíz del disco", disp[0] ? disp : "-");
        return false;
    }
    if (forcado == RAIZ_OPC_DISCO) txt_copia_n(modo, "rw", sizeof modo);
    if (forcado == RAIZ_OPC_LIVE) txt_copia_n(modo, "live", sizeof modo);
    if (forcado == RAIZ_OPC_TORAM) txt_copia_n(modo, "toram", sizeof modo);
    if (!disp[0]) {
        klog("[RAIZ] %s sin \"raiz=\": la raiz sigue en la RAM\n", RAIZ_CONF);
        return false;
    }
    uint32_t espera = 0;
    for (const char *p = espera_txt; *p >= '0' && *p <= '9'; p++) espera = espera * 10 + (uint32_t)(*p - '0');
    if (espera > 60) espera = 60;

    /* El volumen puede tardar en aparecer (pendrive que se enumera tarde). */
    disco_t *d = disco_resolver(disp);
    uint32_t limite = relogio_tiques() + espera * TIQUES_POR_SEGUNDO;
    while (!d && (int32_t)(relogio_tiques() - limite) < 0) {
        cpu_espera();
        d = disco_resolver(disp);
    }
    estado = RAIZ_RESCATE;
    if (!d) {
        klog("[RAIZ] %s no encontrado tras %u s\n", disp, espera);
        aviso("No se encuentra la partición raíz", disp);
        return false;
    }
    if (!d->pai && d->volume != d) d = d->volume;
    if (!d || !txt_comeca_com(d->tipo_fs, "ext")) {
        klog("[RAIZ] %s no es ext2/3/4\n", disp);
        aviso("La partición raíz no es ext2", disp);
        return false;
    }
    char motivo[96];
    if (!raiz_valida(d, motivo, sizeof motivo)) {
        klog("[RAIZ] %s (%s) no vale como raiz: %s\n", d->nome, disp, motivo);
        char msg[160];
        snformatar(msg, sizeof msg, "La partición raíz no vale: %s", motivo);
        aviso(msg, disp);
        return false;
    }
    int r;
    if (txt_compara(modo, "live") == 0 || txt_compara(modo, "toram") == 0) {
        r = raiz_montar_especial(d, modo);
    } else {
        uint32_t opcoes = MONTAR_RAIZ_INTERNO | (txt_compara(modo, "ro") == 0 ? MONTAR_SO_LEITURA : MONTAR_LEITURA_ESCRITA);
        r = montagem_montar_dono(d, "/", d->tipo_fs, opcoes, 0);
    }
    if (r < 0) {
        klog("[RAIZ] montar %s (%s, %s) en / fallo (%d)\n", d->nome, disp, modo, r);
        aviso("No se ha podido montar la partición raíz", disp);
        return false;
    }
    montagem_t *m = montagem_da_raiz();
    bool so_leitura = m && (m->opcoes & MONTAR_SO_LEITURA);
    if (estado == RAIZ_RESCATE) estado = so_leitura ? RAIZ_DISCO_SO_LEITURA : RAIZ_DISCO;
    klog("[RAIZ] %s (%s, %s) montada en / [%s]%s; el initramfs queda debajo\n", d->nome, disp, d->tipo_fs, modo,
         so_leitura ? " en solo lectura" : "");
    return true;
}
