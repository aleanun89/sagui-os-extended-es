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
#include "kconf.h"
#include "disco_comprimido.h"

#define RAIZ_CONF "/etc/raiz.conf"
/* D379 (M1): la marca de versión que mkimagem.py (y el instalador, lo último que copia) dejan
 * en la raíz; y el punto donde se monta un momento, de solo lectura, para comprobarla. */
#define RAIZ_VERSAO     "/sistema/raiz/versao"
#define RAIZ_VERIFICAR  "/raiz-verificar"
/* v8.5.4.2: el identificador de la instalación en su raíz (lo escribe «instalar»; las imágenes no lo llevan). */
#define RAIZ_ID         "/sistema/raiz/id"
#define CAB_ID_INST     144   /* CAB_ID_INSTALACAO (boot/cabecalho.inc): "SGIN", aleatorio, hora */

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
        /* v8.5.4.2: la capa, del mismo disco que la raíz (con dos pendrives live, cada uno con la suya). */
        const char *nome_capa = automatica ? "LABEL=SAGUI-PERSIST" : capa_txt;
        p = NULL;
        for (int k = 0; k < MAX_DISCOS && !p; k++) {
            disco_t *c = disco_resolver_n(nome_capa, k);
            if (!c) break;
            if (!c->pai && c->volume != c) c = c->volume;
            if (c && (!txt_comeca_com(nome_capa, "LABEL=") || disco_raiz(c) == disco_raiz(d))) p = c;
        }
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
    if (estado == RAIZ_LIVE || estado == RAIZ_LIVE_PERSISTENTE) {
        /* v8.5.4.2: el sistema de serie (la capa inferior), para instalar sin los cambios de la sesión */
        r = sobreposicao_montar_serie("/sistema/serie");
        klog("[RAIZ] el sistema de serie en /sistema/serie: %d\n", r);
    }
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

/* v8.5.4.2: los discos y volúmenes que el núcleo sí ve, en pantalla y en el registro: con «no se encuentra
 * la partición raíz» es lo primero que hace falta saber (¿ve el disco de arranque? ¿con otra etiqueta?). */
static void listar_discos(void)
{
    int n = 0;
    kprintf("Discos que ve SaguiOS:\n");
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = disco_indice(i);
        if (!d || !d->usado || !d->presente || d->audio_cd) continue;
        n++;
        kprintf("  %-8s %5u MB  %-8s %-14s %s%s\n", d->nome, d->setores / 2048, d->tipo_fs[0] ? d->tipo_fs : "-",
                d->rotulo[0] ? d->rotulo : "", d->pai ? "" : d->formato, d->arranque_real ? " (arranque)" : "");
        klog("[RAIZ] disco visto: %s %u MB fs=%s rotulo=%s%s\n", d->nome, d->setores / 2048, d->tipo_fs,
             d->rotulo, d->arranque_real ? " (arranque)" : "");
    }
    if (!n) kprintf("  (ninguno)\n");
}

/* v8.5.4.2: no se encuentra la raíz: el aviso dice qué hacer. El caso típico (#36) es un PC cuyo
 * controlador de disco o USB el núcleo no maneja: arranca (la BIOS lee el núcleo y el initramfs) pero
 * después no ve el disco del que arrancó. La imagen «completo» lleva todo el sistema dentro y lo carga
 * en la RAM: no necesita volver a ver el disco. */
static void aviso_sin_raiz(const char *disp)
{
    aviso("No se encuentra la partición raíz", disp);
    listar_discos();
    console_cor(COR_AMARELO, COR_PRETO);
    kprintf("Si el disco o pendrive de arranque no está en la lista, este PC no deja a SaguiOS verlo después\n"
            "de arrancar. Prueba la imagen saguios-completo.img: carga todo el sistema en la RAM y no lo necesita.\n"
            "Para cambiar el modo de arranque, pulsa la tecla M al arrancar (ver /etc/rescate.txt).\n");
    console_cor(COR_CINZA, COR_PRETO);
}

/* v8.5.4.2 (D379): el identificador de la instalación (CAB_ID_INSTALACAO, que «instalar» pone en la
 * cabecera de cada disco instalado y, en el mismo momento, en /sistema/raiz/id de su raíz), como texto;
 * vacío en las imágenes (su cabecera no lo lleva y su raíz tampoco). */
static void id_da_cabecera(char *saida, size_t max)
{
    const uint8_t *c = cabecalho_boot + CAB_ID_INST;
    saida[0] = 0;
    if (c[0] != 'S' || c[1] != 'G' || c[2] != 'I' || c[3] != 'N') return;
    uint32_t a = (uint32_t)c[4] | (uint32_t)c[5] << 8 | (uint32_t)c[6] << 16 | (uint32_t)c[7] << 24;
    uint32_t b = (uint32_t)c[8] | (uint32_t)c[9] << 8 | (uint32_t)c[10] << 16 | (uint32_t)c[11] << 24;
    snformatar(saida, max, "%08x%08x", a, b);
}

/* La primera línea de un texto, sin los espacios, tabuladores y \r del final. false si no cabe. */
static bool primeira_linha(const uint8_t *txt, uint32_t n, char *v, size_t max)
{
    uint32_t k = 0;
    while (k < n && txt[k] != '\n') k++;
    while (k > 0 && (txt[k - 1] == ' ' || txt[k - 1] == '\t' || txt[k - 1] == '\r')) k--;
    bool cabe = k < max;
    if (!cabe) k = (uint32_t)max - 1;
    memcpy(v, txt, k);
    v[k] = 0;
    for (uint32_t i = 0; i < k; i++) if (v[i] == 0) cabe = false;   /* un 0 dentro tampoco casa */
    return cabe;
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
        /* v8.5.4.2: la primera línea ENTERA, quitando solo los espacios, tabuladores y \r del final
         * (una versión con espacios, «v6.0 beta 2», no se corta en el primer espacio: antes «v6.0 beta 2»
         * del núcleo no casaba nunca y una marca «v8.5.4.2 beta» casaba con «v8.5.4.2»). Una línea más
         * larga que el búfer no se recorta: no casa. */
        char v[64];
        bool cabe = primeira_linha(txt, n, v, sizeof v);
        if (!cabe || txt_compara(v, SAGUI_VERSAO) != 0) {
            snformatar(motivo, max, "es de la versión %s y este núcleo es %s", v[0] ? v : "?", SAGUI_VERSAO);
        } else {
            /* v8.5.4.2: y que sea la de ESTA instalación (dos discos SaguiOS de la misma versión: el
             * instalado y el pendrive live, o dos instalados). La cabecera con la que se arrancó dice
             * cuál: con identificador, la raíz tiene que llevar el mismo; sin él (una imagen), la raíz
             * no puede llevar ninguno (sería la de un disco instalado). */
            char esperado[24], tem[24] = "";
            id_da_cabecera(esperado, sizeof esperado);
            uint8_t *id = NULL;
            uint32_t ni = 0;
            if (vfs_ler_inteiro(RAIZ_VERIFICAR RAIZ_ID, &id, &ni) >= 0) {
                if (!primeira_linha(id, ni, tem, sizeof tem) || !tem[0]) txt_copia_n(tem, "?", sizeof tem);
                kfree(id);
            }
            if (txt_compara(esperado, tem) == 0)
                ok = true;
            else if (!esperado[0])
                snformatar(motivo, max, "es la de un sistema instalado (id %s), no la de este medio", tem);
            else if (!tem[0])
                snformatar(motivo, max, "no es la de esta instalación (no lleva identificador; esta es %s)", esperado);
            else
                snformatar(motivo, max, "es de otra instalación (id %s; esta es %s)", tem, esperado);
        }
    }
    if (txt) kfree(txt);
    montagem_desmontar(d->montagem, false);
    return ok;
}

bool raiz_montar(void)
{
    /* D379: la opción de la cabecera manda sobre raiz.conf; v8.5.4.2: encima, lo guardado con kconfig
     * (raiz_modo, salvo en el modo rescate) y, encima de todo, lo elegido con la tecla M al arrancar. */
    uint8_t forcado = cabecalho_boot[CAB_RAIZ_OPC];
    uint32_t guardado = kconf_valor(KC_RAIZ_MODO);
    if ((guardado == RAIZ_OPC_LIVE || guardado == RAIZ_OPC_TORAM) && forcado == RAIZ_OPC_DISCO) {
        /* v8.5.4.2 (revisión): en un disco instalado no se aplica (lo escrito se perdería sin darse cuenta): solo
         * la tecla M lo cambia para un arranque */
        klog("[RAIZ] modo %u guardado con kconfig: no se aplica a un disco instalado\n", guardado);
    } else if (guardado == RAIZ_OPC_LIVE || guardado == RAIZ_OPC_TORAM) {
        klog("[RAIZ] modo %u guardado con kconfig (la imagen dice %u)\n", guardado, forcado);
        forcado = (uint8_t)guardado;
    }
    uint32_t tecla = (info_boot.opcoes & OPC_BOOT_RAIZ_MODO) >> OPC_BOOT_RAIZ_MODO_DESL;
    if (tecla >= RAIZ_OPC_RAM && tecla <= RAIZ_OPC_TORAM) {
        static const char *const nomes[] = { "", "rescate (solo el initramfs)", "instalado", "live desde el medio", "todo a la RAM" };
        klog("[RAIZ] modo %u elegido al arrancar (tecla M): %s\n", tecla, nomes[tecla]);
        forcado = (uint8_t)tecla;
    }
    uint32_t pct = kconf_valor(KC_CAPA_RAM_PCT);
    if (pct) ramfs_fixar_pct(pct);
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

    /* El volumen puede tardar en aparecer (pendrive que se enumera tarde). v8.5.4.2: se miran TODOS los
     * volúmenes con esa etiqueta (disco_resolver_n), cada uno una vez, y se usa el primero que vale
     * (raiz_valida: versión e identificador de la instalación). Así, con dos discos SaguiOS, uno que no es
     * el nuestro (el instalado, cuando se arranca del pendrive; o al revés) no se toma por error, y se
     * sigue esperando por si el bueno es un pendrive que aún no ha salido. */
    disco_t *vistos[12];
    int n_vistos = 0, validas = 0;
    disco_t *d = NULL;
    char motivo[160] = "", disp_ruim[16] = "";
    uint32_t limite = relogio_tiques() + espera * TIQUES_POR_SEGUNDO;
    for (;;) {
        for (int k = -1; k < MAX_DISCOS; k++) {
            /* v8.5.4.2 (P5): primero, la raíz comprimida del CD (rz0, disco_comprimido.c), si la hay */
            disco_t *c = k < 0 ? (txt_comeca_com(disp, "LABEL=") ? disco_comprimido_achar(disp + 6) : NULL)
                               : disco_resolver_n(disp, k);
            if (!c && k < 0) continue;
            if (!c) break;
            if (!c->pai && c->volume != c) c = c->volume;
            if (!c) continue;
            bool ja = false;
            for (int j = 0; j < n_vistos; j++) if (vistos[j] == c) ja = true;
            if (ja) continue;
            if (n_vistos < (int)(sizeof vistos / sizeof vistos[0])) vistos[n_vistos++] = c;
            char mot[160];
            if (!txt_comeca_com(c->tipo_fs, "ext")) {
                snformatar(mot, sizeof mot, "no es ext2/3/4");
            } else if (raiz_valida(c, mot, sizeof mot)) {
                validas++;
                if (!d) d = c;
                continue;
            }
            klog("[RAIZ] %s (%s) no vale como raiz: %s\n", c->nome, disp, mot);
            if (!motivo[0]) { txt_copia_n(motivo, mot, sizeof motivo); txt_copia_n(disp_ruim, c->nome, sizeof disp_ruim); }
        }
        if (d || (int32_t)(relogio_tiques() - limite) >= 0 || n_vistos >= (int)(sizeof vistos / sizeof vistos[0])) break;
        cpu_espera();
    }
    estado = RAIZ_RESCATE;
    if (!d && !n_vistos) {
        klog("[RAIZ] %s no encontrado tras %u s\n", disp, espera);
        aviso_sin_raiz(disp);
        return false;
    }
    if (!d) {
        char msg[224];
        snformatar(msg, sizeof msg, "La partición raíz no vale: %s", motivo);
        aviso(msg, disp_ruim[0] ? disp_ruim : disp);
        return false;
    }
    /* v8.5.4.2: con escritura (instalado) solo si se sabe de verdad que es nuestra. Si hay dos que valen
     * (dos copias idénticas del mismo disco instalado), o no se sabe de qué disco se arrancó y la que vale
     * está en un medio extraíble, nunca se monta con escritura: se arranca como live (la raíz de solo
     * lectura con la capa en la RAM) y se avisa. */
    if (txt_compara(modo, "rw") == 0 && (validas > 1 || (!disco_arranque_real() && disco_raiz(d)->removivel))) {
        klog("[RAIZ] %s: %s: se monta como live (sin escribir en ella)\n", d->nome,
             validas > 1 ? "hay mas de una raiz que vale" : "no se sabe el disco de arranque y es extraible");
        console_cor(COR_AMARELO, COR_PRETO);
        kprintf("[aviso] %s: la raíz %s se usa sin escribir en ella (lo que cambies se perderá al apagar).\n",
                validas > 1 ? "Hay dos discos con este mismo SaguiOS instalado" : "No se sabe de qué disco se arrancó",
                d->nome);
        console_cor(COR_CINZA, COR_PRETO);
        txt_copia_n(modo, "live", sizeof modo);
        capa_txt[0] = 0;   /* la capa en la RAM: tampoco una SAGUI-PERSIST que no sepamos de quién es */
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
    /* el modo de verdad: un toram que no cabe en la RAM acaba en live (v8.5.4.2) */
    const char *real = estado == RAIZ_LIVE || estado == RAIZ_LIVE_PERSISTENTE ? "live" : estado == RAIZ_TORAM ? "toram" : modo;
    klog("[RAIZ] %s (%s, %s) montada en / [%s]%s; el initramfs queda debajo\n", d->nome, disp, d->tipo_fs, real,
         so_leitura ? " en solo lectura" : "");
    return true;
}
