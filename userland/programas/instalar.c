// SPDX-License-Identifier: GPL-2.0-or-later
/* instalar - copia SaguiOS a un disco duro o a otro pendrive (D179).
 *
 *   instalar                         los discos donde se puede instalar
 *   instalar <disco> [--entero | --hueco MB | --particion N] [--sin-jogos] [--sin-apps]
 *            [--copiar-jogos] [--mbr | --gpt | --auto] [--esp] [--sin-home] [-s]
 *   instalar --ventana               el asistente gráfico (Inicio > Sistema)
 *
 * Qué queda en el disco de destino (arranca por BIOS):
 *   LBA 0      el MBR de SaguiOS (el mismo del disco de arranque) y la tabla
 *   LBA 1      la cabecera de arranque, con un identificador propio de esta instalación
 *   partición  SaguiOS (tipo 0xDA, activa): etapa 2, kernel y ramdisk, copiados del
 *              disco del que arrancó el sistema (y releídos para compararlos)
 *   extendida  con las lógicas de datos, que el sistema monta por ETIQUETA:
 *                SAGUI-DATOS (ext2)  -> /home
 *                SAGUI-JOGOS (ext2)  -> /jogos, lectura y escritura: igual que la partición de
 *                                       juegos de la imagen (D181, D228), un juego por carpeta;
 *                                       800 MB, o lo que quepa (la mitad de lo que queda)
 *                SAGUI-APPS  (ext2)  -> /apps, lectura y escritura: igual que la partición de
 *                                       aplicaciones de la imagen (D194), con lo que haya en /apps
 *                SAGUI-CONF  (ext2, 16 MB) la configuración del sistema (D256, mejora 098), la última;
 *                                       si el destino ya tenía una, su contenido se conserva
 *                                       (--formatear-conf la borra)
 * Con "disco entero" la partición de SaguiOS empieza en el LBA 1, como en las imágenes de
 * mkimagem.py; en un hueco libre empieza en el hueco y la cabecera del LBA 1 apunta allí.
 *
 * D266 (v8.0): tabla MBR (BIOS, como arriba) o GPT (UEFI). Con --esp, el MBR lleva además una partición
 * EFI (tipo 0xEF, FAT32) para la imagen híbrida. Con --gpt (solo disco entero) se crea una GPT con su MBR
 * protector, cabecera de respaldo y CRC, y cuatro particiones alineadas a 1 MiB:
 *   1 ESP (256 MB, FAT32 "SAGUI-ESP", tipo EFI)  con lo que haya en /sistema/efi (EFI/BOOT/BOOTX64.EFI...)
 *   2 SaguiOS (tipo propio): primer sector = la cabecera de arranque, luego etapa 2, kernel y ramdisk
 *   3 /home (ext2 "SAGUI-DATOS", opcional: --sin-home)   4 SAGUI-CONF (ext2, 16 MB, D256)
 * Sin juegos ni /apps (el kernel solo ve las cuatro primeras entradas de una GPT). --auto (por defecto) elige
 * GPT si el disco pasa de 2 TB o se arrancó por UEFI, y MBR si no. Una GPT con la BIOS clásica no arranca: hace
 * falta el cargador UEFI de la ESP. Si no hay /sistema/efi, un MBR se instala solo para BIOS (con aviso).
 *
 * Desde un CD (D207): el disco de origen es la imagen El Torito de dentro del CD ("cd0img",
 * que el kernel ve como un disco entero de solo lectura y reconoce por la cabecera); se
 * copia igual, y la cabecera escrita pierde el bit OPC_CD (el disco instalado no es un CD).
 *
 * Seguridad: nunca el disco del que arrancó el sistema; nada montado del destino; un
 * disco GPT solo en modo "disco entero"; antes de nada, el disco se marca como no
 * arrancable (cabecera borrada y un MBR que solo dice "no arrancable") y solo se hace
 * arrancable al final, después de verificarlo todo. Si algo falla a medias, el disco
 * queda no arrancable y se dice claramente. */
#include "sagui.h"
#include "contas.h"
#include "gui.h"
#include "particoes.h"
#include "gpt.h"

#define MB 2048u
#define CAB_ASSINATURA 0x44484753u
#define CAB_SIS_SETORES   136      /* sectores de las particiones del sistema: juegos y aplicaciones (D181, D194) */
#define CAB_ID_INSTALACAO 144      /* 'SGIN', aleatorio, hora (boot/cabecalho.inc) */
#define CAB_OPCOES        6        /* u16 (boot/cabecalho.inc) */
#define OPC_CD            8        /* imagen de CD: el disco de arranque es de solo lectura (D077) */
#define JOGOS_ORIGEM "/jogos"             /* la partición de juegos montada, o el /jogos del ramdisk (D181, D228) */
#define JOGOS_INSTALAR_MB 800u            /* D228: sitio para meter la versión completa de un juego */
#define APPS_ORIGEM  "/apps"               /* las aplicaciones fuera de la base (D194) */
#define EFI_ORIGEM "/sistema/efi"          /* D266: lo que se copia a la ESP (EFI/BOOT/BOOTX64.EFI...); lo pone la rama UEFI */
#define ESP_ROTULO "SAGUI-ESP"
#define ESP_PUNTO  "/mnt/instalar-esp"
#define CONF_MB 16u                        /* D256: la partición SAGUI-CONF (configuración del sistema, mejora 098) */
#define CONF_RESPALDO "/tmp/instalar-conf" /* donde se guarda (en RAM) la SAGUI-CONF vieja mientras se reparte el disco */
/* UTF-8 explícito (octal): el mismo texto y codificación que el LEEME de mkimagem.py */
#define CONF_LEEME \
    "SAGUI-CONF: la configuraci\303\263n del sistema de SaguiOS.\n\n" \
    "Esta partici\303\263n guarda lo que el sistema necesita recordar entre arranques:\n" \
    "usuarios y contrase\303\261as, claves de root y del servidor SSH, la contrase\303\261a\n" \
    "del VNC, los servicios cambiados y la red.\n\n" \
    "No la borres ni la formatees: sin ella SaguiOS arranca con los valores de\n" \
    "f\303\241brica y se pierden las contrase\303\261as y las claves.\n"

enum { M_ENTERO, M_HUECO, M_PARTICION };
enum { T_AUTO, T_MBR, T_GPT };   /* D266: la tabla que se crea */

typedef struct {
    char     destino[12];
    int      modo;
    uint32_t hueco_ini;          /* M_HUECO: cualquier sector dentro del hueco */
    int      particao;           /* M_PARTICION */
    bool     apps, jogos, copiar_jogos;
    bool     formatear_conf;     /* D256: formatear también la SAGUI-CONF vieja; si no, se conserva su contenido */
    char     conf_vieja[16];     /* el volumen de una SAGUI-CONF que ya hay en el destino ("hdb7"), o "" */
    /* calculado por planejar() */
    char     origem[12];
    uint8_t  cab[512], mbr_origem[512];
    uint32_t blob_src, blob_set;   /* etapa 2 + kernel + ramdisk en el origen */
    uint32_t blob_dst;
    uint32_t sys_ini, sys_set, ext_ini, ext_set;
    uint32_t home_set, jogos_set, apps_set, conf_set;
    bool     gpt;                /* el destino ya tiene una GPT */
    /* D266 */
    int      tabela;             /* T_AUTO (se resuelve en planejar), T_MBR o T_GPT */
    bool     esp;                /* crear la partición EFI (siempre con GPT; con MBR, si se pide y hay cargador) */
    bool     sin_home;           /* GPT: sin /home (la configuración va justo detrás del sistema) */
    bool     efi_ok;             /* hay algo que copiar en EFI_ORIGEM */
    uint32_t esp_ini, esp_set, home_ini, conf_ini;
    char     aviso[240];         /* lo que no será como se esperaba (p. ej. sin cargador UEFI) */
    bool     desde_cd;           /* D207: el origen es la imagen El Torito de un CD */
    char     perde[400];         /* qué se borra, dicho para la persona */
} plano_t;

/* ------------------------------------------------------------ utilidades */

static void txt(char *m, int max, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t alinhar_cima(uint32_t x) { return (x + MB - 1) / MB * MB; }

static bool info_de(const char *nome, info_disco_t *out)
{
    for (int i = 0; listar_discos(i, out) == 1; i++) if (compara_texto(out->nome, nome) == 0) return true;
    return false;
}

static void tamanho(uint32_t setores, char *s, int max)
{
    uint32_t mb = setores / MB;
    if (mb >= 1024) formatar_texto(s, (size_t)max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else formatar_texto(s, (size_t)max, "%u MB", mb);
}

/* El disco del que arrancó el sistema (el kernel lo sabe por la cabecera del LBA 1). */
static bool achar_origem(char *nome, int max)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (!d.particao && (d.flags & DISCO_ARRANQUE_REAL)) {
            info_bloco_t b;   /* el de verdad (por la cabecera), no uno que "podría serlo" */
            if (blocos_info(d.nome, &b) == 0 && b.arranque_real) { copia_texto(nome, d.nome, (size_t)max); return true; }
        }
    return false;
}

/* ¿Se puede instalar en este disco? Si no, por qué. */
static bool destino_valido(const info_disco_t *d, char *por, int max)
{
    if (d->particao) { txt(por, max, "es una partición, no un disco"); return false; }
    if (d->flags & DISCO_ARRANQUE_REAL) { txt(por, max, "es el disco de arranque (el sistema está corriendo desde él)"); return false; }
    if (d->flags & DISCO_CD) { txt(por, max, "es un CD/DVD (solo lectura)"); return false; }
    if (d->setores < 160u * MB) { txt(por, max, "es demasiado pequeño (hacen falta 160 MB)"); return false; }
    return true;
}

static bool existe_pasta(const char *c)
{
    info_arquivo_t i;
    return info_arquivo(c, &i) == 0 && i.tipo == TIPO_PASTA;
}

/* D266: ¿se arrancó por UEFI? La señal vendrá en bootinfo cuando exista el cargador UEFI (rama
 * v8.0-uefi); de momento, nunca: la tabla GPT se elige con --gpt (o con el disco de más de 2 TB). */
static bool arrancou_por_uefi(void) { return false; }

/* D266: «automático»: GPT si el disco pasa de 2 TB (el MBR no llega) o se arrancó por UEFI. */
static int tabela_automatica(uint32_t setores) { return setores >= 0xFFFF0000u || arrancou_por_uefi() ? T_GPT : T_MBR; }

/* KB que ocupan los archivos de una carpeta (y sus subcarpetas). */
static uint32_t kb_pasta(const char *c, int nivel)
{
    uint32_t kb = 0;
    entrada_pasta_t e;
    if (nivel > 12) return 0;
    for (int i = 0; listar(c, i, &e) > 0; i++) {
        if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0) continue;
        char f[256];
        caminho_juntar(f, sizeof f, c, e.nome);
        kb += e.tipo == TIPO_PASTA ? kb_pasta(f, nivel + 1) : e.tamanho / 1024 + 1;
    }
    return kb;
}

/* D228: lo que el ramdisk trae en /jogos (los juegos de la base). La partición de juegos
 * se monta encima de /jogos y los tapa, así que el instalador los copia SIEMPRE a la
 * partición nueva (como hace el make con la de la imagen); el resto de /jogos (los datos
 * del OpenTyrian, de OpenJazz, lo que haya metido el usuario) solo con --copiar-jogos. */
static const char *const jogos_base[] = { "chip8", "jclic", "sagui3d" };
#define N_JOGOS_BASE ((int)(sizeof jogos_base / sizeof jogos_base[0]))

static bool e_jogo_base(const char *nome)
{
    for (int i = 0; i < N_JOGOS_BASE; i++)
        if (compara_texto(nome, jogos_base[i]) == 0) return true;
    return false;
}

/* KB de /jogos: con «todo», todo; sin él, solo los juegos de la base. */
static uint32_t kb_jogos(bool todo)
{
    uint32_t kb = 0;
    entrada_pasta_t e;
    for (int i = 0; listar(JOGOS_ORIGEM, i, &e) > 0; i++) {
        if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0
            || compara_texto(e.nome, "lost+found") == 0) continue;
        if (!todo && !e_jogo_base(e.nome)) continue;
        char f[256];
        caminho_juntar(f, sizeof f, JOGOS_ORIGEM, e.nome);
        kb += e.tipo == TIPO_PASTA ? kb_pasta(f, 1) : e.tamanho / 1024 + 1;
    }
    return kb;
}

/* ¿Hay datos de juegos que copiar (algo más que los juegos de la base)? */
static bool ha_jogos(void) { return existe_pasta(JOGOS_ORIGEM) && kb_jogos(true) > kb_jogos(false); }
/* ¿Hay aplicaciones en /apps (la partición SAGUI-APPS o, con APPS=0, el ramdisk)? D194 */
/* D266: ¿hay un cargador UEFI que copiar a la ESP? (la ruta acordada con la rama UEFI: D266) */
static bool efi_disponivel(void) { return existe_pasta(EFI_ORIGEM) && kb_pasta(EFI_ORIGEM, 0) > 0; }
static bool ha_apps(void) { return existe_pasta(APPS_ORIGEM) && kb_pasta(APPS_ORIGEM, 0) > 0; }

/* ------------------------------------------------------------ el plan */

static bool planejar(plano_t *p, char *erro, int max)
{
    info_disco_t d;
    if (!achar_origem(p->origem, sizeof p->origem)) {
        txt(erro, max, "no se sabe de qué disco arrancó el sistema: no hay de dónde copiarlo "
                       "(si arrancó de un CD, su lector no se ha encontrado: mira \"discos\" y el log [ATAPI])");
        return false;
    }
    info_disco_t o;
    p->desde_cd = info_de(p->origem, &o) && (o.flags & DISCO_CD);
    if (!info_de(p->destino, &d)) { txt(erro, max, "no existe el disco %s", p->destino); return false; }
    char por[96];
    if (!destino_valido(&d, por, sizeof por)) { txt(erro, max, "no se instala en %s: %s", p->destino, por); return false; }
    if (compara_texto(p->origem, p->destino) == 0) { txt(erro, max, "%s es el disco de arranque", p->destino); return false; }
    /* La cabecera y el MBR del origen. */
    if (blocos_ler(p->origem, 1, 1, p->cab) < 0 || le32(p->cab) != CAB_ASSINATURA || blocos_ler(p->origem, 0, 1, p->mbr_origem) < 0) {
        txt(erro, max, "la cabecera de arranque de %s no se puede leer", p->origem);
        return false;
    }
    p->blob_src = le32(p->cab + 8);
    uint32_t total = le32(p->cab + 48);
    if (p->blob_src < 2 || total <= p->blob_src || total - p->blob_src > 1024u * MB) {
        txt(erro, max, "la cabecera de arranque de %s es incoherente", p->origem);
        return false;
    }
    p->blob_set = total - p->blob_src;
    uint32_t sys_mb = (p->blob_set * 2 + MB - 1) / MB;
    if (sys_mb < 64) sys_mb = 64;

    pt_tabela_t t;
    int r = pt_ler(p->destino, &t);
    if (r < 0) { txt(erro, max, "no se puede leer %s: %s", p->destino, nome_erro(r)); return false; }
    p->gpt = t.esquema == PT_GPT;
    p->perde[0] = 0;
    p->aviso[0] = 0;
    /* D266: la tabla que se creará, y la ESP. */
    if (p->tabela == T_AUTO) p->tabela = tabela_automatica(d.setores);
    if (p->tabela == T_GPT && p->modo != M_ENTERO) { txt(erro, max, "una GPT solo se crea en el disco entero (no en un hueco ni sustituyendo una partición)"); return false; }
    if (p->tabela == T_GPT) { p->jogos = p->apps = p->copiar_jogos = false; p->esp = true; }
    p->efi_ok = efi_disponivel();
    if (p->esp && !p->efi_ok) {
        if (p->tabela == T_MBR) {
            p->esp = false;
            txt(p->aviso, sizeof p->aviso, "no hay cargador UEFI en " EFI_ORIGEM ": se instala solo para BIOS, sin partición EFI");
        } else
            txt(p->aviso, sizeof p->aviso, "no hay cargador UEFI en " EFI_ORIGEM ": la ESP queda vacía y el disco GPT no arrancará hasta que copies BOOTX64.EFI en EFI/BOOT");
    }
    uint32_t esp_set = (p->tabela == T_GPT && d.setores >= 1024u * MB ? 256u : 64u) * MB;
    if (p->esp && p->efi_ok && kb_pasta(EFI_ORIGEM, 0) > esp_set / 2 - 4096) { txt(erro, max, "el cargador UEFI de " EFI_ORIGEM " no cabe en la partición EFI (%u MB)", esp_set / MB); return false; }
    uint32_t ini, fim;
    if (p->modo == M_ENTERO) {
        p->sys_ini = 1;
        p->blob_dst = p->blob_src;
        ini = 1;
        fim = t.setores / MB * MB;
        if (t.esquema == PT_MBR || (t.esquema == PT_GPT && !t.gpt_mala)) {
            /* D256: ¿ya hay una SAGUI-CONF (reinstalación)? Su contenido se conserva (también en una GPT, D266). */
            p->conf_vieja[0] = 0;
            for (int i = 0; i < 4 + t.nlog && !p->conf_vieja[0]; i++) {
                const pt_part_t *x = i < 4 ? &t.prim[i] : &t.log[i - 4];
                char n[16];
                info_disco_t q;
                if (!x->tipo || pt_tipo_extendida(x->tipo)) continue;
                pt_nome_particao(p->destino, i < 4 ? i + 1 : i + 1, n, sizeof n);
                if (info_de(n, &q) && q.suportado && compara_texto(q.rotulo, "SAGUI-CONF") == 0)
                    copia_texto(p->conf_vieja, n, sizeof p->conf_vieja);
            }
            for (int i = 0; i < 4; i++) if (t.prim[i].tipo) {
                char n[16], x[80], tm[16];
                info_disco_t q;
                pt_nome_particao(p->destino, i + 1, n, sizeof n);
                tamanho(t.prim[i].setores, tm, sizeof tm);
                if (info_de(n, &q) && q.suportado) txt(x, sizeof x, "%s%s (%s, %s \"%s\")", p->perde[0] ? ", " : "", n, tm, q.formato, q.rotulo);
                else txt(x, sizeof x, "%s%s (%s, %s)", p->perde[0] ? ", " : "", n, tm, pt_nome_tipo(t.prim[i].tipo));
                junta_texto(p->perde, x, sizeof p->perde);
            }
            if (t.nlog) { char x[40]; txt(x, sizeof x, " y %d lógica%s", t.nlog, t.nlog == 1 ? "" : "s"); junta_texto(p->perde, x, sizeof p->perde); }
            if (t.esquema == PT_GPT && t.ngpt > 4) { char x[48]; txt(x, sizeof x, " y %d más", t.ngpt - 4); junta_texto(p->perde, x, sizeof p->perde); }
            if (t.esquema == PT_GPT && p->perde[0]) { char x[48]; txt(x, sizeof x, " (todo en una GPT)"); junta_texto(p->perde, x, sizeof p->perde); }
        } else if (t.esquema == PT_GPT) txt(p->perde, sizeof p->perde, "toda la GPT (dañada) y lo que contenga");
        else if (t.esquema == PT_VOLUME) txt(p->perde, sizeof p->perde, "el volumen %s que ocupa todo el disco", d.formato);
        if (!p->perde[0]) copia_texto(p->perde, "nada (el disco está vacío)", sizeof p->perde);
    if (p->tabela == T_GPT) {   /* D266: ESP, SaguiOS, /home y SAGUI-CONF, alineadas a 1 MiB, dentro de la zona utilizable */
        uint32_t blob_total = p->blob_src - 1 + p->blob_set;
        uint32_t smb = (blob_total * 2 + MB - 1) / MB;
        if (smb < 64) smb = 64;
        p->esp_ini = MB; p->esp_set = esp_set;
        p->sys_ini = p->esp_ini + p->esp_set;
        p->sys_set = smb * MB;
        p->blob_dst = p->sys_ini + p->blob_src - 1;   /* la cabecera va en el primer sector de la partición */
        p->home_ini = p->sys_ini + p->sys_set;
        uint32_t fim_g = (t.setores - 33) / MB * MB;
        p->conf_set = CONF_MB * MB;
        if (fim_g < p->home_ini + p->conf_set + (p->sin_home ? 0u : 16u * MB)) { txt(erro, max, "no cabe: hacen falta al menos %u MB para ESP, sistema, configuración%s", (p->home_ini + p->conf_set) / MB + (p->sin_home ? 0u : 16u), p->sin_home ? "" : " y /home"); return false; }
        p->home_set = p->sin_home ? 0 : fim_g - p->home_ini - p->conf_set;
        if (p->home_set > 0xFFFFFFFFu - MB) p->home_set = 0xFFFFFFFFu - MB;
        p->home_set = p->home_set / MB * MB;
        p->conf_ini = p->home_ini + p->home_set;
            return true;
    }
        p->sys_set = sys_mb * MB - 1;
        p->ext_ini = sys_mb * MB;
        if (p->esp) { p->esp_ini = p->ext_ini; p->esp_set = esp_set; p->ext_ini += esp_set; }
    } else {
        if (t.esquema != PT_MBR && t.esquema != PT_SEM_TABELA) {
            txt(erro, max, "%s no tiene tabla MBR: solo se puede instalar en el disco entero", p->destino);
            return false;
        }
        if (p->modo == M_PARTICION) {
            pt_part_t *x = pt_achar(&t, p->particao);
            if (!x || p->particao > 4) { txt(erro, max, "elige una partición primaria (1 a 4)"); return false; }
            char n[16], tm[16];
            info_disco_t q;
            pt_nome_particao(p->destino, p->particao, n, sizeof n);
            tamanho(x->setores, tm, sizeof tm);
            if (pt_tipo_extendida(x->tipo)) txt(p->perde, sizeof p->perde, "%s (extendida, %s) y sus %d lógicas", n, tm, t.nlog);
            else if (info_de(n, &q) && q.suportado) txt(p->perde, sizeof p->perde, "%s (%s, %s \"%s\")", n, tm, q.formato, q.rotulo);
            else txt(p->perde, sizeof p->perde, "%s (%s, %s)", n, tm, pt_nome_tipo(x->tipo));
            p->hueco_ini = alinhar_cima(x->inicio);
            char e[120];
            if (pt_apagar(&t, p->particao, e, sizeof e) < 0) { txt(erro, max, "%s", e); return false; }
        }
        if (pt_extendida(&t) >= 0) {
            txt(erro, max, "%s ya tiene una partición extendida: instala en el disco entero o bórrala antes", p->destino);
            return false;
        }
        int libres = 0;
        for (int i = 0; i < 4; i++) if (!t.prim[i].tipo) libres++;
        if (libres < (p->esp ? 3 : 2)) { txt(erro, max, "hacen falta %s entradas libres en la tabla de %s (SaguiOS%s y la extendida)", p->esp ? "tres" : "dos", p->destino, p->esp ? ", EFI" : ""); return false; }
        for (int i = 0; i < 4; i++)
            if (t.prim[i].tipo && t.prim[i].inicio <= 1) { txt(erro, max, "la partición %d empieza en el sector 1, donde va la cabecera de arranque", i + 1); return false; }
        pt_livre_t v[24];
        int n = pt_livres(&t, v, 24), k = -1;
        for (int i = 0; i < n; i++)
            if (!v[i].logica && p->hueco_ini >= v[i].inicio && p->hueco_ini < v[i].inicio + v[i].setores) k = i;
        if (k < 0) { txt(erro, max, "no hay un hueco libre en esa posición de %s", p->destino); return false; }
        ini = v[k].inicio;
        fim = v[k].inicio + v[k].setores;
        p->sys_ini = ini;
        p->blob_dst = ini;
        p->sys_set = sys_mb * MB;
        p->ext_ini = ini + sys_mb * MB;
        if (p->esp) { p->esp_ini = p->ext_ini; p->esp_set = esp_set; p->ext_ini += esp_set; }
        if (!p->perde[0]) copia_texto(p->perde, "nada: solo se usa espacio libre", sizeof p->perde);
    }
    if (p->ext_ini >= fim) { txt(erro, max, "no cabe: SaguiOS necesita al menos %u MB más espacio para los datos", sys_mb); return false; }
    p->ext_set = fim - p->ext_ini;
    /* Los datos: /home se queda con lo que sobre; /jogos y /apps según el espacio (y lo pedido). */
    uint32_t disp = p->ext_set / MB;
    int nlog = 2 + (p->jogos ? 1 : 0) + (p->apps ? 1 : 0);   /* /home, SAGUI-CONF y las opcionales */
    if (disp < (uint32_t)nlog) { txt(erro, max, "no cabe"); return false; }
    disp -= (uint32_t)nlog;   /* un MB para el EBR de cada lógica */
    if (disp < CONF_MB + 16) { txt(erro, max, "no cabe: falta sitio para SAGUI-CONF (%u MB)", CONF_MB); return false; }
    disp -= CONF_MB;          /* D256: SAGUI-CONF, la última de las lógicas */
    p->conf_set = CONF_MB * MB;
    uint32_t jg = 0, ap = 0;
    if (p->apps && disp >= 16 + 40 + 40) {
        ap = disp * 15 / 100; if (ap < 40) ap = 40; if (ap > 4096) ap = 4096;
        /* D194: al menos el doble de lo que hay en /apps más 16 MB, para instalar otras. */
        uint32_t dados = ha_apps() ? kb_pasta(APPS_ORIGEM, 0) / 1024 + 1 : 0;
        if (ap < dados * 2 + 16) ap = dados * 2 + 16;
    } else if (p->apps) { p->apps = false; disp += 1; }
    /* Juegos (D181, D228): 800 MB, para meter desde el SaguiOS la versión completa de un
     * juego, o lo que quepa: como mucho la mitad de lo que queda tras /apps (el resto es
     * para /home). Nunca menos del doble de lo que se copia más 8 MB (o 32 MB). */
    if (p->jogos) {
        uint32_t dados = kb_jogos(p->copiar_jogos && ha_jogos()) / 1024 + 1;
        uint32_t minimo = dados * 2 + 8;
        if (minimo < 32) minimo = 32;
        uint32_t resto = disp > ap ? disp - ap : 0;
        jg = JOGOS_INSTALAR_MB;
        if (jg < minimo) jg = minimo;
        if (jg > resto / 2) jg = resto / 2;
        if (jg < minimo) { p->jogos = false; p->copiar_jogos = false; jg = 0; disp += 1; }
    }
    if (disp < jg + ap + 16) { txt(erro, max, "no cabe: hacen falta al menos %u MB", sys_mb + 16 + 2); return false; }
    p->apps_set = ap * MB;
    p->jogos_set = jg * MB;
    p->home_set = (disp - jg - ap) * MB;
    if (p->home_set > 0xFFFFFFFFu - MB) p->home_set = 0xFFFFFFFFu - MB;
    return true;
}

static void descrever_plano(const plano_t *p, char *s, int max)
{
    char a[16], h[16], j[16], x[16], e[16], parte[96];
    tamanho(p->sys_set + 1, a, sizeof a);
    tamanho(p->home_set, h, sizeof h);
    tamanho(p->jogos_set, j, sizeof j);
    tamanho(p->apps_set, x, sizeof x);
    tamanho(p->esp_set, e, sizeof e);
    txt(s, max, "SaguiOS %s", a);
    if (p->home_set) { txt(parte, sizeof parte, "  |  /home %s (ext2)", h); junta_texto(s, parte, (size_t)max); }
    if (p->jogos) { txt(parte, sizeof parte, "  |  juegos %s (ext2)", j); junta_texto(s, parte, (size_t)max); }
    if (p->apps) { txt(parte, sizeof parte, "  |  /apps %s (ext2)", x); junta_texto(s, parte, (size_t)max); }
    junta_texto(s, "  |  SAGUI-CONF 16 MB", (size_t)max);
    if (p->esp) { txt(parte, sizeof parte, "  |  EFI %s (FAT32)", e); junta_texto(s, parte, (size_t)max); }
    if (p->tabela == T_GPT) junta_texto(s, "  |  tabla GPT (UEFI)", (size_t)max);
}

/* ------------------------------------------------------------ la instalación */

typedef void (*informar_t)(int fase, int por_cento, const char *texto);
static informar_t informar;
static int fase_atual;
#define FASES 6
static const char *const nome_fase[FASES] = {
    "Preparar el disco (no arrancable mientras tanto)", "Crear y formatear las particiones", "Copiar el sistema",
    "Verificar lo copiado", "Datos: /home, juegos, /apps y SAGUI-CONF", "Hacer el disco arrancable" };

static void fase(int f, int pc, const char *t)
{
    fase_atual = f;
    if (informar) informar(f, pc, t);
}

static void prog_fmt(int pc, void *ctx) { (void)ctx; if (informar) informar(fase_atual, pc, NULL); }

/* Deja el destino sin cabecera: el MBR de SaguiOS dice "Cabecera invalida" y no sigue. */
static void marcar_nao_arrancavel(const char *disco)
{
    static const uint8_t zero[512];
    bool tinha = blocos_escrever(disco, 1, 1, zero) == 0;
    if (!tinha && blocos_reservar(disco, 0) == 0) { blocos_escrever(disco, 1, 1, zero); blocos_liberar(disco); }
    registrar("instalar: %s marcado como no arrancable", disco);
}

/* El código de arranque provisional: "int 18h" (la BIOS pasa al siguiente disco) y parar. */
/* D266: tras un fallo en una GPT no se toca el LBA 1 (es la cabecera de la GPT): se borra la cabecera
 * de arranque, que va en el primer sector de la partición de SaguiOS. */
static void marcar_nao_arrancavel_gpt(const plano_t *p, bool reservado)
{
    static const uint8_t zero[512];
    if (!reservado && blocos_reservar(p->destino, DBLOCO_CONF_GPT) < 0) return;
    blocos_escrever(p->destino, p->sys_ini, 1, zero);
    if (!reservado) blocos_liberar(p->destino);
    registrar("instalar: %s (GPT): cabecera de arranque borrada, no arrancable", p->destino);
}

static void mbr_provisional(uint8_t *s)
{
    memset(s, 0, 446);
    s[0] = 0xCD; s[1] = 0x18; s[2] = 0xF4; s[3] = 0xEB; s[4] = 0xFD;
}

static int copiar_blob(plano_t *p, bool verificar, char *erro, int max)
{
    enum { PASSO = 128 };
    uint8_t *a = aloca(PASSO * 512), *b = aloca(PASSO * 512);
    if (!a || !b) { if (a) libera(a); if (b) libera(b); return ERRO_SEM_MEMORIA; }
    int r = 0;
    for (uint32_t feito = 0; feito < p->blob_set && r == 0; ) {
        uint32_t q = p->blob_set - feito > PASSO ? PASSO : p->blob_set - feito;
        r = blocos_ler(p->origem, p->blob_src + feito, q, a);
        if (r < 0) { txt(erro, max, "error al leer %s (sector %u): %s", p->origem, p->blob_src + feito, nome_erro(r)); break; }
        if (!verificar) {
            r = blocos_escrever(p->destino, p->blob_dst + feito, q, a);
            if (r < 0) { txt(erro, max, "error al escribir %s (sector %u): %s", p->destino, p->blob_dst + feito, nome_erro(r)); break; }
        } else {
            r = blocos_ler(p->destino, p->blob_dst + feito, q, b);
            if (r < 0) { txt(erro, max, "error al releer %s (sector %u): %s", p->destino, p->blob_dst + feito, nome_erro(r)); break; }
            if (memcmp(a, b, q * 512) != 0) {
                txt(erro, max, "lo releído de %s no es igual al original (cerca del sector %u)", p->destino, p->blob_dst + feito);
                r = ERRO_FORMATO;
                break;
            }
        }
        feito += q;
        fase(fase_atual, (int)(feito / 64 * 100 / (p->blob_set / 64 + 1)), NULL);
    }
    libera(a);
    libera(b);
    return r;
}

/* ---- copia de archivos (los datos de juegos) ---- */
static uint32_t bytes_copiados, arquivos_copiados;

static int copiar_arquivo(const char *de, const char *para, char *erro, int max)
{
    int fi = abrir(de, ABRIR_LER);
    if (fi < 0) { txt(erro, max, "no se puede abrir %s: %s", de, nome_erro(fi)); return fi; }
    int fo = abrir(para, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fo < 0) { fechar(fi); txt(erro, max, "no se puede crear %s: %s", para, nome_erro(fo)); return fo; }
    static uint8_t buf[32768], buf2[32768];
    int r = 0;
    uint32_t total = 0;
    for (;;) {
        int n = ler(fi, buf, sizeof buf);
        if (n < 0) { r = n; txt(erro, max, "error al leer %s", de); break; }
        if (!n) break;
        if (escrever_tudo(fo, buf, (unsigned)n) < 0) { r = ERRO_LIMITE; txt(erro, max, "error al escribir %s (¿lleno?)", para); break; }
        total += (uint32_t)n;
    }
    fechar(fi);
    fechar(fo);
    if (r < 0) return r;
    /* Verificación: se relee la copia y se compara con el original. */
    fi = abrir(de, ABRIR_LER);
    fo = abrir(para, ABRIR_LER);
    if (fi < 0 || fo < 0) { if (fi >= 0) fechar(fi); if (fo >= 0) fechar(fo); txt(erro, max, "no se puede releer %s", para); return ERRO_ACESSO; }
    for (;;) {
        int n = ler(fi, buf, sizeof buf), m = ler(fo, buf2, sizeof buf2);
        if (n != m || (n > 0 && memcmp(buf, buf2, (unsigned)n) != 0)) { r = ERRO_FORMATO; txt(erro, max, "la copia de %s no es igual al original", de); break; }
        if (n <= 0) break;
    }
    fechar(fi);
    fechar(fo);
    bytes_copiados += total;
    arquivos_copiados++;
    return r;
}

static int copiar_pasta(const char *de, const char *para, char *erro, int max, int nivel)
{
    if (nivel > 12) return 0;
    criar_pasta(para);
    entrada_pasta_t e;
    for (int i = 0; listar(de, i, &e) > 0; i++) {
        if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0) continue;
        /* La lost+found del ext2 de origen no se copia: el formateo ya hace la suya. */
        if (nivel == 0 && compara_texto(e.nome, "lost+found") == 0) continue;
        char a[256], b[256];
        caminho_juntar(a, sizeof a, de, e.nome);
        caminho_juntar(b, sizeof b, para, e.nome);
        int r = e.tipo == TIPO_PASTA ? copiar_pasta(a, b, erro, max, nivel + 1) : copiar_arquivo(a, b, erro, max);
        if (r < 0) return r;
        fase(fase_atual, (int)(arquivos_copiados % 100), NULL);
    }
    return 0;
}

static int escrever_texto_arquivo(const char *caminho, const char *t)
{
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    return r < 0 ? r : 0;
}

/* Monta una partición del destino, la prepara y la desmonta. jogos: copia los datos del
 * juego (si se pidió); apps: copia lo que haya en /apps (D194). */
static uint32_t arquivos_jogos, arquivos_apps;
static bool conf_respaldada;   /* hay una copia de la SAGUI-CONF vieja en CONF_RESPALDO */
static int preparar_dados(plano_t *p, int num, const char *ponto, const char *leeme, bool jogos, bool apps,
                          char *erro, int max, bool conf)
{
    char vol[16];
    pt_nome_particao(p->destino, num, vol, sizeof vol);
    int r = montar_em(vol, ponto, MONTAR_LEITURA_ESCRITA);
    if (r < 0) { txt(erro, max, "no se puede montar %s (recién formateada): %s", vol, nome_erro(r)); return r; }
    char c[96];
    caminho_juntar(c, sizeof c, ponto, conf ? "LEEME" : "leeme.txt");   /* D256: en SAGUI-CONF se llama LEEME */
    r = leeme ? escrever_texto_arquivo(c, leeme) : 0;
    if (r == 0 && jogos) {
        /* D181, D228: un juego por carpeta, como en la partición de la imagen: todo /jogos
         * con --copiar-jogos; si no, solo los juegos de la base (el montaje los tapa). */
        uint32_t antes = arquivos_copiados;
        if (p->copiar_jogos && ha_jogos()) {
            fase(fase_atual, 0, "Copiando los datos de " JOGOS_ORIGEM "...");
            r = copiar_pasta(JOGOS_ORIGEM, ponto, erro, max, 0);
        } else {
            for (int i = 0; i < N_JOGOS_BASE && r == 0; i++) {
                char de[64], para[96];
                caminho_juntar(de, sizeof de, JOGOS_ORIGEM, jogos_base[i]);
                if (!existe_pasta(de)) continue;
                caminho_juntar(para, sizeof para, ponto, jogos_base[i]);
                r = copiar_pasta(de, para, erro, max, 1);
            }
        }
        arquivos_jogos += arquivos_copiados - antes;
    } else if (r == 0 && apps) {
        /* D194: las aplicaciones van con el sistema (con su leeme.txt, si lo traen). */
        if (ha_apps()) {
            fase(fase_atual, 0, "Copiando las aplicaciones de " APPS_ORIGEM "...");
            uint32_t antes = arquivos_copiados;
            r = copiar_pasta(APPS_ORIGEM, ponto, erro, max, 0);
            arquivos_apps += arquivos_copiados - antes;
        }
    } else if (r < 0) txt(erro, max, "no se puede escribir en %s: %s", vol, nome_erro(r));
    if (r == 0 && conf && conf_respaldada) {
        /* D256: reinstalación: se devuelve lo que había (contraseñas, claves), también su LEEME. */
        fase(fase_atual, 0, "Devolviendo el contenido de SAGUI-CONF...");
        r = copiar_pasta(CONF_RESPALDO, ponto, erro, max, 0);
    }
    int d = desmontar(ponto);
    if (r == 0 && d < 0) { txt(erro, max, "no se puede desmontar %s: %s", vol, nome_erro(d)); r = d; }
    return r;
}

/* D266: la ESP: se monta, se le copia lo que haya en EFI_ORIGEM (EFI/BOOT/BOOTX64.EFI...) y se desmonta. */
static int preparar_esp(plano_t *p, int num, char *erro, int max)
{
    if (!p->efi_ok) return 0;
    char vol[16];
    pt_nome_particao(p->destino, num, vol, sizeof vol);
    criar_pasta(ESP_PUNTO);
    int r = montar_em(vol, ESP_PUNTO, MONTAR_LEITURA_ESCRITA);
    if (r < 0) { txt(erro, max, "no se puede montar la partición EFI %s (recién formateada): %s", vol, nome_erro(r)); return r; }
    fase(fase_atual, 0, "Copiando el cargador UEFI a la partición EFI...");
    uint32_t antes = arquivos_copiados;
    r = copiar_pasta(EFI_ORIGEM, ESP_PUNTO, erro, max, 0);
    registrar("instalar: ESP: %u archivos copiados de " EFI_ORIGEM, arquivos_copiados - antes);
    int d = desmontar(ESP_PUNTO);
    if (r == 0 && d < 0) { txt(erro, max, "no se puede desmontar la partición EFI %s: %s", vol, nome_erro(d)); r = d; }
    return r;
}

static int instalar(plano_t *p, char *erro, int max)
{
    char e[200];
    int r;
    int num_sys = 0, num_ext = 0, num_home = 0, num_jogos = 0, num_apps = 0, num_conf = 0, num_esp = 0;
    bool gpt_novo = p->tabela == T_GPT;
    bool reservado = false, tocado = false;
    bytes_copiados = arquivos_copiados = arquivos_jogos = arquivos_apps = 0;
    registrar("instalar: de %s a %s, modo %d, tabla %s, sistema %u+%u, extendida %u+%u, home %u jogos %u apps %u, esp %u+%u",
              p->origem, p->destino, p->modo, gpt_novo ? "GPT" : "MBR", p->sys_ini, p->sys_set, p->ext_ini, p->ext_set, p->home_set, p->jogos_set, p->apps_set,
              p->esp_ini, p->esp_set);

    /* 0. D256: una SAGUI-CONF que ya hay en el destino se conserva: se copia a la RAM antes de
     * tocar nada (y se devuelve a la nueva); solo se pierde si se pidió formatear todo. */
    conf_respaldada = false;
    if (p->conf_vieja[0] && !p->formatear_conf) {
        fase(0, 0, "Guardando la configuración de SAGUI-CONF...");
        criar_pasta("/mnt/instalar-confvieja");
        criar_pasta(CONF_RESPALDO);
        r = montar_em(p->conf_vieja, "/mnt/instalar-confvieja", MONTAR_SO_LEITURA);
        if (r < 0) { txt(erro, max, "no se puede leer la SAGUI-CONF de %s (%s): no se ha tocado nada; "
                                    "usa --formatear-conf si no la quieres", p->conf_vieja, nome_erro(r)); return r; }
        r = copiar_pasta("/mnt/instalar-confvieja", CONF_RESPALDO, erro, max, 0);
        desmontar("/mnt/instalar-confvieja");
        if (r < 0) return r;
        conf_respaldada = true;
        registrar("instalar: SAGUI-CONF de %s guardada en %s", p->conf_vieja, CONF_RESPALDO);
    }

    /* 1. Reservar y dejar el disco no arrancable. */
    fase(0, 0, NULL);
    r = blocos_reservar(p->destino, (p->gpt || gpt_novo) && p->modo == M_ENTERO ? DBLOCO_CONF_GPT : 0);
    if (r < 0) {
        txt(erro, max, r == ERRO_OCUPADO ? "%s tiene particiones montadas o lo usa otro programa: no se ha tocado nada"
                                         : "no se puede usar %s: %s; no se ha tocado nada", p->destino, nome_erro(r));
        return r;
    }
    reservado = true;
    tocado = true;
    static const uint8_t zero[512];
    if ((r = blocos_escrever(p->destino, 1, 1, zero)) < 0) { txt(erro, max, "no se puede escribir en %s: %s", p->destino, nome_erro(r)); goto falha; }
    if (p->gpt && p->modo == M_ENTERO) {
        blocos_zerar(p->destino, 1, 33);
        info_bloco_t i;
        if (blocos_info(p->destino, &i) == 0) blocos_zerar(p->destino, i.setores - 33, 33);
    }
    fase(0, 100, NULL);

    /* 2. La tabla nueva y los sistemas de archivos. */
    fase(1, 0, NULL);
    pt_tabela_t t;
    if (gpt_novo) {   /* D266: la GPT entera, validada antes de escribir; respaldo, principal y MBR protector; CRC releídos */
        gpt_t *g = aloca(sizeof *g);
        if (!g) { txt(erro, max, "sin memoria"); r = ERRO_SEM_MEMORIA; goto falha; }
        uint8_t cod[512];
        mbr_provisional(cod);
        info_bloco_t ib;
        blocos_info(p->destino, &ib);
        gpt_nova(g, p->destino, ib.setores);
        int sl;
        if ((sl = gpt_adicionar(g, GPT_TIPO_ESP, p->esp_ini, p->esp_set, "EFI System", e, sizeof e)) < 0) { libera(g); txt(erro, max, "la GPT (ESP): %s", e); r = sl; goto falha; }
        num_esp = gpt_numero_kernel(g, sl);
        if ((sl = gpt_adicionar(g, GPT_TIPO_SAGUI, p->sys_ini, p->sys_set, "SaguiOS", e, sizeof e)) < 0) { libera(g); txt(erro, max, "la GPT (sistema): %s", e); r = sl; goto falha; }
        num_sys = gpt_numero_kernel(g, sl);
        if (p->home_set) {
            if ((sl = gpt_adicionar(g, GPT_TIPO_LINUX, p->home_ini, p->home_set, "SAGUI-DATOS", e, sizeof e)) < 0) { libera(g); txt(erro, max, "la GPT (/home): %s", e); r = sl; goto falha; }
            num_home = gpt_numero_kernel(g, sl);
        }
        if ((sl = gpt_adicionar(g, GPT_TIPO_LINUX, p->conf_ini, p->conf_set, "SAGUI-CONF", e, sizeof e)) < 0) { libera(g); txt(erro, max, "la GPT (SAGUI-CONF): %s", e); r = sl; goto falha; }
        num_conf = gpt_numero_kernel(g, sl);
        r = gpt_gravar(g, true, cod, e, sizeof e);
        libera(g);
        if (r < 0) { txt(erro, max, "la tabla GPT: %s", e); goto falha; }
        goto tabela_pronta;
    }
    if ((r = pt_ler(p->destino, &t)) < 0) { txt(erro, max, "no se puede leer la tabla: %s", nome_erro(r)); goto falha; }
    if (p->modo == M_ENTERO) { memset(t.prim, 0, sizeof t.prim); t.nlog = 0; t.esquema = PT_MBR; }
    else if (p->modo == M_PARTICION && pt_apagar(&t, p->particao, e, sizeof e) < 0) { txt(erro, max, "%s", e); r = ERRO_INVALIDO; goto falha; }
    if (t.esquema == PT_SEM_TABELA) t.esquema = PT_MBR;
    mbr_provisional(t.setor0);
    int slot = -1;
    for (int i = 0; i < 4; i++) { t.prim[i].ativa = false; if (!t.prim[i].tipo && slot < 0) slot = i; }
    if (slot < 0) { txt(erro, max, "no queda ninguna entrada libre en la tabla"); r = ERRO_LIMITE; goto falha; }
    t.prim[slot] = (pt_part_t){ 0xDA, true, p->sys_ini, p->sys_set, 0 };
    num_sys = slot + 1;
    if (p->esp) {   /* D266: partición EFI primaria (0xEF) entre SaguiOS y la extendida */
        num_esp = pt_criar(&t, p->esp_ini, p->esp_set, 0xEF, false, e, sizeof e);
        if (num_esp < 0) { txt(erro, max, "la partición EFI: %s", e); r = num_esp; goto falha; }
    }
    num_ext = pt_criar(&t, p->ext_ini, p->ext_set, 0x0F, false, e, sizeof e);
    if (num_ext < 0) { txt(erro, max, "la extendida: %s", e); r = num_ext; goto falha; }
    uint32_t pos = p->ext_ini + MB;
    num_home = pt_criar(&t, pos, p->home_set, 0x83, true, e, sizeof e);
    if (num_home < 0) { txt(erro, max, "/home: %s", e); r = num_home; goto falha; }
    pos += p->home_set + MB;
    if (p->jogos) {
        num_jogos = pt_criar(&t, pos, p->jogos_set, 0x83, true, e, sizeof e);
        if (num_jogos < 0) { txt(erro, max, "juegos: %s", e); r = num_jogos; goto falha; }
        pos += p->jogos_set + MB;
    }
    if (p->apps) {
        num_apps = pt_criar(&t, pos, p->apps_set, 0x83, true, e, sizeof e);   /* D194: ext2, como en la imagen */
        if (num_apps < 0) { txt(erro, max, "/apps: %s", e); r = num_apps; goto falha; }
        pos += p->apps_set + MB;
    }
    /* D256: SAGUI-CONF, ext2 de 16 MB, la última de las lógicas (como en las imágenes). */
    num_conf = pt_criar(&t, pos, p->conf_set, 0x83, true, e, sizeof e);
    if (num_conf < 0) { txt(erro, max, "SAGUI-CONF: %s", e); r = num_conf; goto falha; }
    if ((r = pt_gravar(&t, "instalar SaguiOS", e, sizeof e)) < 0) { txt(erro, max, "la tabla: %s", e); goto falha; }
tabela_pronta: ;
    char vol[16];
    if (num_esp) {   /* D266 */
        pt_nome_particao(p->destino, num_esp, vol, sizeof vol);
        fase(1, 0, "Formateando la partición EFI (FAT32)...");
        if ((r = fs_formatar_fat32(vol, ESP_ROTULO, prog_fmt, NULL, e, sizeof e)) < 0) { txt(erro, max, "%s", e); goto falha; }
    }
    if (num_home) {
        pt_nome_particao(p->destino, num_home, vol, sizeof vol);
        fase(1, 0, "Formateando /home (ext2)...");
        if ((r = fs_formatar_ext2(vol, "SAGUI-DATOS", prog_fmt, NULL, e, sizeof e)) < 0) { txt(erro, max, "%s", e); goto falha; }
    }
    if (num_jogos) {
        pt_nome_particao(p->destino, num_jogos, vol, sizeof vol);
        fase(1, 0, "Formateando la partición de juegos (ext2)...");
        if ((r = fs_formatar_ext2(vol, "SAGUI-JOGOS", prog_fmt, NULL, e, sizeof e)) < 0) { txt(erro, max, "%s", e); goto falha; }
    }
    if (num_apps) {
        pt_nome_particao(p->destino, num_apps, vol, sizeof vol);
        fase(1, 0, "Formateando /apps (ext2)...");
        if ((r = fs_formatar_ext2(vol, "SAGUI-APPS", prog_fmt, NULL, e, sizeof e)) < 0) { txt(erro, max, "%s", e); goto falha; }
    }
    pt_nome_particao(p->destino, num_conf, vol, sizeof vol);
    fase(1, 0, "Formateando SAGUI-CONF (ext2)...");
    if ((r = fs_formatar_ext2(vol, "SAGUI-CONF", prog_fmt, NULL, e, sizeof e)) < 0) { txt(erro, max, "%s", e); goto falha; }

    /* 3 y 4. El sistema: copiar y releer. */
    fase(2, 0, NULL);
    if ((r = copiar_blob(p, false, erro, max)) < 0) goto falha;
    fase(3, 0, NULL);
    if ((r = copiar_blob(p, true, erro, max)) < 0) goto falha;
    registrar("instalar: sistema copiado y verificado (%u sectores)", p->blob_set);

    /* 5. Los datos: se sueltan el disco y se montan las particiones nuevas. */
    fase(4, 0, NULL);
    blocos_liberar(p->destino);
    reservado = false;
    if (num_esp && (r = preparar_esp(p, num_esp, erro, max)) < 0) goto falha;
    if (num_home && (r = preparar_dados(p, num_home, "/mnt/instalar-home",
                            "Esta es la carpeta personal (/home) de SaguiOS instalado en este disco.\n"
                            "Lo que guardes aquí sigue aquí al reiniciar.\n", false, false, erro, max, false)) < 0) goto falha;
    if (num_jogos && (r = preparar_dados(p, num_jogos, "/mnt/instalar-jogos",
                            NULL, true, false, erro, max, false)) < 0) goto falha;
    if (num_apps && (r = preparar_dados(p, num_apps, "/mnt/instalar-apps",
                            "Aplicaciones de SaguiOS fuera de la RAM (/apps, D194): los ejecutables en /apps/bin\n"
                            "y los datos de cada una en /apps/<app>.\n", false, true, erro, max, false)) < 0) goto falha;
    if ((r = preparar_dados(p, num_conf, "/mnt/instalar-conf", CONF_LEEME, false, false, erro, max, true)) < 0) goto falha;
    fase(4, 100, NULL);

    /* 6. Arrancable: primero la cabecera, luego el código del MBR, y se relee todo. */
    fase(5, 0, NULL);
    if ((r = blocos_reservar(p->destino, gpt_novo ? DBLOCO_CONF_GPT : 0)) < 0) { txt(erro, max, "no se puede volver a reservar %s: %s", p->destino, nome_erro(r)); goto falha; }
    reservado = true;
    uint8_t cab[512], s[512];
    memcpy(cab, p->cab, 512);
    int32_t delta = (int32_t)(p->blob_dst - p->blob_src);
    po32(cab + 8, le32(cab + 8) + (uint32_t)delta);     /* etapa 2 */
    po32(cab + 16, le32(cab + 16) + (uint32_t)delta);   /* kernel */
    po32(cab + 32, le32(cab + 32) + (uint32_t)delta);   /* ramdisk */
    po32(cab + 48, le32(cab + 48) + (uint32_t)delta);   /* final */
    uint32_t id = 0;
    bytes_aleatorios(&id, 4);
    /* D207: la cabecera de una imagen de CD lleva OPC_CD; el disco instalado no es un CD. */
    {
        uint16_t op = (uint16_t)(cab[CAB_OPCOES] | cab[CAB_OPCOES + 1] << 8);
        op &= (uint16_t)~OPC_CD;
        cab[CAB_OPCOES] = (uint8_t)op;
        cab[CAB_OPCOES + 1] = (uint8_t)(op >> 8);
    }
    po32(cab + CAB_SIS_SETORES, (p->jogos ? p->jogos_set : 0) + (p->apps ? p->apps_set : 0) + p->conf_set);   /* para «Acerca de» (D181, D194, D256) */
    po32(cab + CAB_ID_INSTALACAO, 0x4E494753u);   /* "SGIN" */
    po32(cab + CAB_ID_INSTALACAO + 4, id ? id : tempo_ms());
    po32(cab + CAB_ID_INSTALACAO + 8, hora_utc(NULL));
    uint32_t cab_lba = gpt_novo ? p->sys_ini : 1;   /* D266: en una GPT, el LBA 1 es su cabecera */
    if ((r = blocos_escrever(p->destino, cab_lba, 1, cab)) < 0 || (r = blocos_ler(p->destino, cab_lba, 1, s)) < 0 || memcmp(s, cab, 512) != 0) {
        txt(erro, max, "la cabecera de arranque no se ha podido escribir");
        if (r >= 0) r = ERRO_FORMATO;
        goto falha;
    }
    fase(5, 50, NULL);
    if (gpt_novo) {   /* D266: la GPT, otra vez entera y con sus CRC, y el MBR protector tal como se escribió */
        gpt_t *g = aloca(sizeof *g);
        if (!g) { txt(erro, max, "sin memoria"); r = ERRO_SEM_MEMORIA; goto falha; }
        bool ok = gpt_ler(p->destino, g, e, sizeof e) == 0 && g->origem == GPT_PRINCIPAL && g->respaldo_ok &&
                  gpt_num_usadas(g) == (num_esp ? 1 : 0) + 1 + (num_home ? 1 : 0) + 1;
        libera(g);
        if (!ok) { txt(erro, max, "la GPT releída al final no es la esperada"); r = ERRO_FORMATO; goto falha; }
        blocos_liberar(p->destino);
        reservado = false;
        fase(5, 100, NULL);
        registrar("instalar: terminado en %s (GPT), %u archivos de /apps copiados", p->destino, arquivos_apps);
        return 0;
    }
    if ((r = pt_ler(p->destino, &t)) < 0) goto falha;
    memcpy(t.setor0, p->mbr_origem, 440);
    po32(t.setor0 + 440, id ^ 0x53414755u);   /* firma del disco propia */
    t.setor0[444] = t.setor0[445] = 0;
    if ((r = pt_gravar(&t, "código de arranque de SaguiOS", e, sizeof e)) < 0) { txt(erro, max, "el MBR: %s", e); goto falha; }
    if (blocos_ler(p->destino, 0, 1, s) < 0 || memcmp(s, p->mbr_origem, 440) != 0) { txt(erro, max, "el MBR releído no coincide"); r = ERRO_FORMATO; goto falha; }
    blocos_liberar(p->destino);
    fase(5, 100, NULL);
    (void)num_sys; (void)num_ext;
    registrar("instalar: terminado en %s (id %x), %u archivos de juegos y %u de /apps copiados", p->destino, id,
              arquivos_jogos, arquivos_apps);
    return 0;

falha:
    for (int i = 1; i <= 3; i++) {   /* las carpetas de trabajo, si quedó algo montado */
        static const char *const c[] = { NULL, "/mnt/instalar-home", "/mnt/instalar-jogos", "/mnt/instalar-apps" };
        desmontar(c[i]);
    }
    desmontar(ESP_PUNTO);
    if (tocado && gpt_novo) marcar_nao_arrancavel_gpt(p, reservado);
    else if (tocado) marcar_nao_arrancavel(p->destino);
    if (reservado) blocos_liberar(p->destino);
    registrar("instalar: FALLO en la fase %d: %s", fase_atual + 1, erro);
    return r < 0 ? r : ERRO_INVALIDO;
}

/* ------------------------------------------------------------ terminal */

static void inf_texto(int f, int pc, const char *t)
{
    static int ult_f = -1, ult_pc = -1;
    if (f != ult_f) { imprimir("%s[%d/%d] %s\n", ult_f >= 0 ? "\n" : "", f + 1, FASES, nome_fase[f]); ult_f = f; ult_pc = -1; }
    if (t) imprimir("      %s\n", t);
    if (pc / 20 != ult_pc / 20) { imprimir("  %d%%", pc); ult_pc = pc; }
}

static int listar_destinos(void)
{
    char origem[12] = "";
    bool tem = achar_origem(origem, sizeof origem);
    info_disco_t o;
    bool cd = tem && info_de(origem, &o) && (o.flags & DISCO_CD);
    imprimir("Disco de arranque (origen de la copia): %s%s\n", tem ? origem : "no se sabe: no se puede instalar",
             cd ? " (la imagen de arranque del CD)" : "");
    info_disco_t d;
    int n = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (d.particao) continue;
        char tm[16], por[96];
        tamanho(d.setores, tm, sizeof tm);
        bool ok = destino_valido(&d, por, sizeof por);
        imprimir("  %-6s %s %s, %s, %s: %s\n", d.nome, d.fabricante, d.produto, tm,
                 (d.flags & DISCO_CD) ? "CD/DVD" : (d.flags & DISCO_REMOVIVEL) ? "pendrive" : "disco interno", ok ? "se puede instalar" : por);
        n += ok;
    }
    if (!n) imprimir("No hay ningún disco donde instalar.\n");
    return 0;
}

static int terminal(int argc, char **argv)
{
    plano_t p;
    memset(&p, 0, sizeof p);
    p.modo = M_ENTERO;
    p.jogos = p.apps = true;
    bool sim = false;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--entero") == 0) p.modo = M_ENTERO;
        else if (compara_texto(argv[i], "--hueco") == 0 && i + 1 < argc) { int v; texto_para_numero(argv[++i], &v); p.modo = M_HUECO; p.hueco_ini = (uint32_t)v * MB; }
        else if (compara_texto(argv[i], "--particion") == 0 && i + 1 < argc) { int v; texto_para_numero(argv[++i], &v); p.modo = M_PARTICION; p.particao = v; }
        else if (compara_texto(argv[i], "--sin-jogos") == 0) p.jogos = false;
        else if (compara_texto(argv[i], "--sin-apps") == 0) p.apps = false;
        else if (compara_texto(argv[i], "--copiar-jogos") == 0) p.copiar_jogos = true;
        else if (compara_texto(argv[i], "--formatear-conf") == 0) p.formatear_conf = true;
        else if (compara_texto(argv[i], "--mbr") == 0) p.tabela = T_MBR;       /* D266 */
        else if (compara_texto(argv[i], "--gpt") == 0) p.tabela = T_GPT;
        else if (compara_texto(argv[i], "--auto") == 0) p.tabela = T_AUTO;
        else if (compara_texto(argv[i], "--esp") == 0) p.esp = true;
        else if (compara_texto(argv[i], "--sin-home") == 0) p.sin_home = true;
        else if (compara_texto(argv[i], "-s") == 0 || compara_texto(argv[i], "--si") == 0) sim = true;
        else if (argv[i][0] == '-') {
            imprimir_em(ERROS, "uso: instalar [disco [--entero | --hueco MB | --particion N] [--sin-jogos] [--sin-apps] [--copiar-jogos] [--formatear-conf]\n"
                               "                      [--mbr | --gpt | --auto] [--esp] [--sin-home] [-s]]\n"
                               "     instalar --ventana\n");
            return 2;
        } else copia_texto(p.destino, argv[i], sizeof p.destino);
    }
    if (!p.destino[0]) return listar_destinos();
    if (falta_admin("instalar")) return 1;   /* D255 */
    char e[300], d[300];
    if (!planejar(&p, e, sizeof e)) { imprimir_em(ERROS, "instalar: %s\n", e); return 1; }
    descrever_plano(&p, d, sizeof d);
    imprimir("Instalar SaguiOS de %s en %s\n  %s\n  Se borra: %s\n", p.origem, p.destino, d, p.perde);
    imprimir("  Tabla de particiones: %s\n", p.tabela == T_GPT ? "GPT (arranque UEFI; con la BIOS clásica no arranca)" : "MBR (arranque BIOS)");
    if (p.aviso[0]) imprimir("  AVISO: %s\n", p.aviso);
    if (p.conf_vieja[0])
        imprimir(p.formatear_conf ? "  AVISO: se BORRA también SAGUI-CONF (%s): contraseñas y claves del sistema\n"
                                  : "  Se conserva el contenido de SAGUI-CONF (%s): contraseñas y claves (--formatear-conf lo borra)\n", p.conf_vieja);
    if (p.copiar_jogos) imprimir("  Se copian también los datos de " JOGOS_ORIGEM "%s\n", ha_jogos() ? "" : " (no hay datos: nada que copiar)");
    if (!sim) {
        char r[16];
        imprimir("Escribe el nombre del disco (%s) para seguir: ", p.destino);
        if (ler_linha(ENTRADA, r, sizeof r) < 0 || compara_texto(r, p.destino) != 0) { imprimir("No se ha tocado nada.\n"); return 1; }
    }
    informar = inf_texto;
    int r = instalar(&p, e, sizeof e);
    imprimir("\n");
    if (r < 0) {
        imprimir_em(ERROS, "instalar: HA FALLADO: %s\n", e);
        imprimir_em(ERROS, "instalar: el disco %s ha quedado NO ARRANCABLE.\n", p.destino);
        return 1;
    }
    imprimir("SaguiOS instalado en %s y verificado.\n  %s\n", p.destino, d);
    if (p.copiar_jogos) imprimir("  Datos de juegos copiados: %u archivos\n", arquivos_jogos);
    if (p.apps) imprimir("  Aplicaciones de " APPS_ORIGEM " copiadas: %u archivos\n", arquivos_apps);
    imprimir("  SAGUI-CONF (configuración del sistema, %u MB)%s\n", CONF_MB, conf_respaldada ? ": contenido anterior conservado" : "");
    if (p.esp) imprimir("  Partición EFI (%u MB, FAT32): %s\n", p.esp_set / MB, p.efi_ok ? "con el cargador UEFI de " EFI_ORIGEM : "vacía (no había cargador UEFI)");
    if (p.tabela == T_GPT) imprimir("  Tabla GPT verificada (principal y respaldo, CRC releídos).\n");
    imprimir(p.desde_cd ? "Apaga, saca el CD y arranca desde %s.\n" : "Apaga, quita el disco de arranque y arranca desde %s.\n", p.destino);
    if (p.tabela == T_GPT) imprimir("Recuerda: el disco es GPT y arranca por UEFI (activa UEFI en la BIOS del PC).\n");
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define JL 660
#define JA 500
enum { P_DISCO, P_MODO, P_PROGRESSO, P_FIM };
static jan_t *j;
static int pagina;
static plano_t pl;
static char destinos[12][12];
static bool destino_ok[12];
static char destino_por[12][96];
static int ndest, sel_dest = -1;
typedef struct { int modo; uint32_t ini; int part; int tabela; char texto[120]; } opcao_t;
static opcao_t ops[12];
static int nops, sel_op;
static casilla_t cas_copiar = { 24, 0, true }, cas_apps = { 24, 0, true }, cas_esp = { 24, 0, false };   /* D266: cas_esp = ESP con MBR */
static char plano_txt[300], plano_erro[300], resultado[400];
static bool resultado_ok;
static int prog_fase, prog_pc;
static char prog_txt[96];

static void quebrar(const char *t, char *s, int max, int n)
{
    int k = 0, col = 0;
    while (*t && k < max - 1) {
        const char *p = t;
        while (*p && *p != ' ' && *p != '\n') p++;
        int len = (int)(p - t);
        if (col && col + 1 + len > n) { s[k++] = '\n'; col = 0; }
        else if (col && k < max - 1) { s[k++] = ' '; col++; }
        for (int i = 0; i < len && k < max - 1; i++) s[k++] = t[i];
        col += len;
        t = p;
        if (*t == '\n') { if (k < max - 1) s[k++] = '\n'; col = 0; t++; }
        else if (*t == ' ') t++;
    }
    s[k] = 0;
}

static int texto_linhas(int x, int y, const char *t, int n, uint32_t cor)
{
    char q[800];
    quebrar(t, q, sizeof q, n);
    const char *l = q;
    while (*l) {
        const char *f = acha_char(l, '\n');
        int c = f ? (int)(f - l) : (int)tam_texto(l);
        g_texto_n(j, x, y, l, c, cor);
        y += 18;
        l += c;
        if (*l == '\n') l++;
    }
    return y;
}

static void ler_destinos(void)
{
    ndest = 0;
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1 && ndest < 12; i++) {
        if (d.particao) continue;
        copia_texto(destinos[ndest], d.nome, 12);
        destino_ok[ndest] = destino_valido(&d, destino_por[ndest], 96);
        ndest++;
    }
    if (sel_dest >= ndest || (sel_dest >= 0 && !destino_ok[sel_dest])) sel_dest = -1;
    for (int i = 0; i < ndest && sel_dest < 0; i++) if (destino_ok[i]) sel_dest = i;
}

static void ler_opcoes(void)
{
    nops = 0;
    if (sel_dest < 0) return;
    pt_tabela_t t;
    if (pt_ler(destinos[sel_dest], &t) < 0) return;
    char tm[16];
    tamanho(t.setores, tm, sizeof tm);
    ops[nops].modo = M_ENTERO; ops[nops].tabela = T_MBR;
    txt(ops[nops].texto, sizeof ops[nops].texto, "Usar el disco entero (%s), tabla MBR (BIOS): se borra todo", tm);
    nops++;
    /* D266: el disco entero con GPT (UEFI): ESP + SaguiOS + /home + SAGUI-CONF */
    ops[nops].modo = M_ENTERO; ops[nops].tabela = T_GPT;
    txt(ops[nops].texto, sizeof ops[nops].texto, "Usar el disco entero (%s), tabla GPT (UEFI): se borra todo", tm);
    nops++;
    if (t.esquema == PT_MBR || t.esquema == PT_SEM_TABELA) {
        pt_livre_t v[24];
        int n = pt_livres(&t, v, 24);
        for (int i = 0; i < n && nops < 12; i++) {
            if (v[i].logica || v[i].setores < 160 * MB) continue;
            tamanho(v[i].setores, tm, sizeof tm);
            ops[nops].modo = M_HUECO; ops[nops].ini = v[i].inicio; ops[nops].tabela = T_MBR;
            txt(ops[nops].texto, sizeof ops[nops].texto, "Espacio libre de %s (desde %u MB): no se borra nada", tm, v[i].inicio / MB);
            nops++;
        }
        for (int i = 0; i < 4 && nops < 12; i++) {
            if (!t.prim[i].tipo || t.prim[i].setores < 160 * MB) continue;
            char n[16];
            info_disco_t q;
            pt_nome_particao(destinos[sel_dest], i + 1, n, sizeof n);
            tamanho(t.prim[i].setores, tm, sizeof tm);
            ops[nops].modo = M_PARTICION; ops[nops].part = i + 1; ops[nops].tabela = T_MBR;
            if (info_de(n, &q) && q.suportado) txt(ops[nops].texto, sizeof ops[nops].texto, "Sustituir %s (%s, %s \"%s\"): se borra", n, tm, q.formato, q.rotulo);
            else txt(ops[nops].texto, sizeof ops[nops].texto, "Sustituir %s (%s, %s): se borra", n, tm, pt_nome_tipo(t.prim[i].tipo));
            nops++;
        }
    }
    if (sel_op >= nops) sel_op = 0;
    if (tabela_automatica(t.setores) == T_GPT) sel_op = 1;   /* «automático»: GPT si pasa de 2 TB o se arrancó por UEFI */
}

static void recalcular(void)
{
    memset(&pl, 0, sizeof pl);
    plano_erro[0] = plano_txt[0] = 0;
    if (sel_dest < 0 || !nops) return;
    copia_texto(pl.destino, destinos[sel_dest], sizeof pl.destino);
    pl.modo = ops[sel_op].modo;
    pl.hueco_ini = ops[sel_op].ini;
    pl.particao = ops[sel_op].part;
    pl.tabela = ops[sel_op].tabela;
    pl.esp = pl.tabela == T_GPT || cas_esp.marcada;
    pl.jogos = true;
    pl.apps = cas_apps.marcada;
    pl.copiar_jogos = cas_copiar.marcada && ha_jogos();
    if (!planejar(&pl, plano_erro, sizeof plano_erro)) return;
    descrever_plano(&pl, plano_txt, sizeof plano_txt);
}

static void botao(int x, int y, int l, const char *t, bool ativo)
{
    botao_t b = { x, y, l, 28, t, false };
    botao_desenhar(j, &b);
    if (!ativo) for (int k = 0; k < 28; k += 2) g_retangulo(j, x + 2, y + k, l - 4, 1, 0xE2E6EA);
}

static void cabecalho(const char *titulo, const char *sub)
{
    g_retangulo(j, 0, 0, j->larg, 64, 0x0B7A3E);
    g_texto(j, 20, 10, titulo, 0xFFFFFF);
    g_texto(j, 20, 40, sub, 0xD8F0E0);
    static const char *const passos[] = { "1 Disco", "2 Dónde", "3 Copia", "4 Fin" };
    for (int i = 0; i < 4; i++) {
        int x = j->larg - 4 * 76 - 12 + i * 76;
        bool at = i == pagina;
        g_retangulo(j, x, 8, 70, 22, at ? 0xFFFFFF : 0x2E9E5F);
        g_texto(j, x + (70 - g_largura_texto(passos[i])) / 2, 11, passos[i], at ? 0x0B7A3E : 0xD8F0E0);
    }
}

static void desenhar(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    int bh = j->alt - 46;
    g_retangulo(j, 0, bh - 10, j->larg, 1, 0xDDE1E5);
    if (pagina == P_DISCO) {
        char o[12] = "", s[160];
        bool tem = achar_origem(o, sizeof o);
        txt(s, sizeof s, tem ? "Se copia el sistema que está corriendo ahora (de %s)." : "No se sabe de qué disco arrancó el sistema: no se puede instalar.", o);
        cabecalho("Instalar SaguiOS", s);
        g_texto(j, 20, 80, "¿En qué disco quieres instalarlo?", COR_TEXTO);
        int y = 106;
        for (int i = 0; i < ndest; i++, y += 46) {
            info_disco_t d;
            if (!info_de(destinos[i], &d)) continue;
            bool sel = i == sel_dest;
            g_retangulo(j, 20, y, j->larg - 40, 40, sel ? 0xDCE8FA : destino_ok[i] ? 0xFFFFFF : 0xF0F1F2);
            g_moldura(j, 20, y, j->larg - 40, 40, sel ? COR_SELECAO : 0xC9CED3);
            g_retangulo(j, 30, y + 8, 24, 24, destino_ok[i] ? ((d.flags & DISCO_REMOVIVEL) ? 0xE8891C : 0x3A7BD5) : 0xB0B6BC);
            g_texto(j, 34, y + 12, (d.flags & DISCO_REMOVIVEL) ? "U" : "D", 0xFFFFFF);
            char tm[16], l1[120];
            tamanho(d.setores, tm, sizeof tm);
            txt(l1, sizeof l1, "%s  -  %s %s  -  %s", d.nome, d.fabricante, d.produto, tm);
            g_texto(j, 66, y + 4, l1, destino_ok[i] ? COR_TEXTO : COR_TEXTO_FRACO);
            txt(l1, sizeof l1, "%s%s", destino_ok[i] ? ((d.flags & DISCO_REMOVIVEL) ? "pendrive USB" : "disco interno") : "no: ",
                destino_ok[i] ? "" : destino_por[i]);
            g_texto(j, 66, y + 21, l1, destino_ok[i] ? COR_TEXTO_FRACO : 0xC0392B);
        }
        botao(j->larg - 120, bh, 100, "Siguiente >", sel_dest >= 0 && tem);
        botao(20, bh, 100, "Cancelar", true);
    } else if (pagina == P_MODO) {
        char s[120];
        txt(s, sizeof s, "Disco de destino: %s", pl.destino[0] ? pl.destino : destinos[sel_dest]);
        cabecalho("¿Dónde, dentro del disco?", s);
        int y = 80;
        for (int i = 0; i < nops; i++, y += 28) {
            bool sel = i == sel_op;
            g_retangulo(j, 22, y + 3, 14, 14, 0xFFFFFF);
            g_moldura(j, 22, y + 3, 14, 14, COR_BORDA);
            if (sel) g_retangulo(j, 26, y + 7, 6, 6, COR_SELECAO);
            g_texto(j, 44, y + 2, ops[i].texto, ops[i].modo == M_HUECO ? COR_TEXTO : sel ? 0xC0392B : COR_TEXTO);
        }
        y += 8;
        bool gpt_sel = ops[sel_op].tabela == T_GPT;
        cas_apps.y = y;
        casilla_desenhar(j, &cas_apps);
        g_texto(j, 46, y, ha_apps() ? "Crear también la partición /apps, con las aplicaciones de este sistema"
                                    : "Crear también la partición /apps (para aplicaciones)", gpt_sel ? COR_TEXTO_FRACO : COR_TEXTO);
        y += 26;
        cas_copiar.y = y;
        bool hay = ha_jogos();
        casilla_desenhar(j, &cas_copiar);
        g_texto(j, 46, y, hay ? "Copiar también los datos de juegos de /jogos (OpenTyrian, OpenJazz...)"
                              : "Copiar los datos de juegos (no hay en este sistema)", hay && !gpt_sel ? COR_TEXTO : COR_TEXTO_FRACO);
        y += 26;
        cas_esp.y = y;
        casilla_desenhar(j, &cas_esp);
        g_texto(j, 46, y, gpt_sel ? "Con GPT, la partición EFI (ESP) y /home van siempre; sin juegos ni /apps"
                                  : "Crear también la partición EFI (para la imagen híbrida BIOS+UEFI)", gpt_sel ? COR_TEXTO_FRACO : COR_TEXTO);
        y += 30;
        g_retangulo(j, 20, y, j->larg - 40, bh - 20 - y, 0xFFFFFF);
        g_moldura(j, 20, y, j->larg - 40, bh - 20 - y, 0xC9CED3);
        if (plano_erro[0]) texto_linhas(32, y + 10, plano_erro, 74, 0xC0392B);
        else {
            g_texto(j, 32, y + 10, "Lo que va a quedar en el disco:", COR_TEXTO);
            /* Mini barra del plan. */
            int bx = 32, bw = j->larg - 64, by2 = y + 32;
            uint32_t tot = pl.sys_set + pl.ext_set + (pl.tabela == T_GPT ? pl.home_set + pl.conf_set : 0) + pl.esp_set;
            uint32_t partes[5] = { pl.esp ? pl.esp_set : 0, pl.sys_set, pl.home_set, pl.jogos ? pl.jogos_set : 0, pl.apps ? pl.apps_set : 0 };
            uint32_t cores[5] = { 0xC9A60C, 0xE8891C, 0x2E9E4F, 0x3A7BD5, 0x8E5CC2 };
            const char *nomes[5] = { "EFI", "SaguiOS", "/home", "juegos", "/apps" };
            int x = bx;
            for (int i = 0; i < 5; i++) {
                if (!partes[i]) continue;
                int l = (int)(partes[i] / MB * (uint32_t)bw / (tot / MB + 1));
                if (l < 40) l = 40;
                if (x + l > bx + bw) l = bx + bw - x;
                g_retangulo(j, x, by2, l, 30, cores[i]);
                g_moldura(j, x, by2, l, 30, 0xFFFFFF);
                if (l > g_largura_texto(nomes[i]) + 6) g_texto(j, x + 4, by2 + 7, nomes[i], 0xFFFFFF);
                x += l;
            }
            int yy = texto_linhas(32, by2 + 40, plano_txt, 74, COR_TEXTO);
            char p2[460];
            txt(p2, sizeof p2, "Se borra: %s", pl.perde);
            yy = texto_linhas(32, yy + 4, p2, 74, pl.modo == M_HUECO ? COR_TEXTO_FRACO : 0xC0392B);
            if (pl.aviso[0]) texto_linhas(32, yy + 2, pl.aviso, 74, 0xB9770E);
        }
        botao(20, bh, 100, "< Atrás", true);
        botao(j->larg - 120, bh, 100, "Instalar", !plano_erro[0]);
    } else if (pagina == P_PROGRESSO) {
        cabecalho("Instalando SaguiOS...", "No apagues el ordenador ni quites ningún disco.");
        int y = 84;
        for (int i = 0; i < FASES; i++, y += 26) {
            uint32_t c = i < prog_fase ? COR_DESTAQUE : i == prog_fase ? COR_SELECAO : 0xB0B6BC;
            g_retangulo(j, 24, y + 3, 12, 12, c);
            if (i < prog_fase) g_texto(j, 25, y, "v", 0xFFFFFF);
            g_texto(j, 46, y, nome_fase[i], i <= prog_fase ? COR_TEXTO : COR_TEXTO_FRACO);
        }
        int py = y + 16, pw = j->larg - 48;
        int total = (prog_fase * 100 + prog_pc) / FASES;
        g_retangulo(j, 24, py, pw, 22, 0xD5D9DD);
        g_retangulo(j, 24, py, pw * total / 100, 22, COR_DESTAQUE);
        g_moldura(j, 24, py, pw, 22, COR_BORDA);
        char t[48];
        txt(t, sizeof t, "%d %%", total);
        g_texto(j, 24 + pw / 2 - 16, py + 3, t, COR_TEXTO);
        if (prog_txt[0]) g_texto(j, 24, py + 32, prog_txt, COR_TEXTO_FRACO);
    } else {
        cabecalho(resultado_ok ? "SaguiOS está instalado" : "La instalación ha fallado",
                  resultado_ok ? "Todo lo copiado se ha releído y comparado con el original." : "El disco de destino ha quedado NO ARRANCABLE.");
        if (!resultado_ok) g_retangulo(j, 0, 0, j->larg, 4, 0xC0392B);
        texto_linhas(24, 84, resultado, 72, resultado_ok ? COR_TEXTO : 0xC0392B);
        botao(j->larg - 120, bh, 100, "Cerrar", true);
    }
    jan_mostrar(j);
}

static void inf_janela(int f, int pc, const char *t)
{
    if (f != prog_fase) prog_txt[0] = 0;
    prog_fase = f;
    prog_pc = pc > 100 ? 100 : pc;
    if (t) copia_texto(prog_txt, t, sizeof prog_txt);
    desenhar();
    registrar("instalar: fase %d %d%%", f + 1, prog_pc);
}

static void comecar(void)
{
    char t[700];
    txt(t, sizeof t, "Se va a instalar SaguiOS en %s.\nSe borra: %s.\n\n¿Seguir?", pl.destino, pl.perde);
    char q[800];
    quebrar(t, q, sizeof q, 42);
    if (!confirmar("Instalar SaguiOS", q)) return;
    if (pl.tabela == T_GPT) {
        txt(t, sizeof t, "El disco %s quedará con tabla GPT (arranque UEFI): ESP, SaguiOS, /home y configuración.%s%s\n\n¿Seguir?", pl.destino,
            pl.aviso[0] ? " AVISO: " : "", pl.aviso);
        quebrar(t, q, sizeof q, 42);
        if (!confirmar("Instalar SaguiOS (GPT)", q)) return;
    }
    if (pl.modo != M_HUECO) {
        txt(t, sizeof t, "Última confirmación: TODO lo que se ha dicho de %s se borra y no se puede recuperar.", pl.destino);
        quebrar(t, q, sizeof q, 42);
        if (!confirmar("Instalar SaguiOS", q)) return;
    }
    /* Lo montado del destino se desmonta (con permiso). */
    info_disco_t d;
    if (info_de(pl.destino, &d) && d.montado) {
        txt(t, sizeof t, "%s tiene particiones montadas. ¿Desmontarlas para instalar?", pl.destino);
        quebrar(t, q, sizeof q, 42);
        if (!confirmar("Instalar SaguiOS", q)) return;
        desmontar(pl.destino);
    }
    pagina = P_PROGRESSO;
    prog_fase = 0; prog_pc = 0; prog_txt[0] = 0;
    desenhar();
    informar = inf_janela;
    char e[300];
    registrar("instalar: comienza en %s", pl.destino);
    int r = instalar(&pl, e, sizeof e);
    resultado_ok = r >= 0;
    char d2[300];
    descrever_plano(&pl, d2, sizeof d2);
    if (resultado_ok)
        txt(resultado, sizeof resultado, "SaguiOS se ha instalado en %s.\n%s\n%s\nPara usarlo: apaga, %s y arranca desde %s.",
            pl.destino, d2, pl.copiar_jogos ? "Los datos de juegos se han copiado." : "",
            pl.desde_cd ? "saca el CD" : "quita el disco de arranque actual", pl.destino);
    else
        txt(resultado, sizeof resultado, "Error en \"%s\": %s.\n\nEl disco %s NO ARRANCA tal como ha quedado. Se puede repetir la instalación.",
            nome_fase[prog_fase], e, pl.destino);
    registrar("instalar: resultado %d: %s", r, resultado);
    pagina = P_FIM;
    desenhar();
}

static void registrar_layout(void)
{
    if (pagina == P_DISCO) for (int i = 0; i < ndest; i++) registrar("instalar: disco %s ok=%d y=%d", destinos[i], destino_ok[i], 106 + i * 46 + 20);
    if (pagina == P_MODO) {
        for (int i = 0; i < nops; i++) registrar("instalar: opcion %d y=%d %s", i, 80 + i * 28 + 10, ops[i].texto);
        registrar("instalar: casillas apps=%d copiar=%d esp=%d plan=%s erro=%s", cas_apps.y, cas_copiar.y, cas_esp.y, plano_txt, plano_erro);
    }
    registrar("instalar: pagina %d botones y=%d", pagina, j->alt - 46);
}

static int janela(void)
{
    if (!sou_root() && elevar_grafico("instalar SaguiOS: escribe y borra discos enteros") != 0) {   /* D255, D269: diálogo de permisos */
        mensagem("Instalar SaguiOS", "Instalar SaguiOS necesita permisos de administrador y no se concedieron.\n"
                 "Ábrelo otra vez y escribe la contraseña de administrador, o usa el Terminal:  admin instalar --ventana");
        return 1;
    }
    j = jan_abrir("Instalar SaguiOS", JL, JA);
    if (!j) { imprimir("instalar: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    ler_destinos();
    desenhar();
    registrar("instalar: ventana abierta");
    registrar_layout();
    evento_t e;
    for (;;) {
        if (esperar_evento(j->id, &e, 1000) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pagina != P_PROGRESSO) break; continue; }
        if (e.tipo != EV_CLIQUE && e.tipo != EV_TECLA) continue;
        int bh = j->alt - 46;
        bool b_dir = e.tipo == EV_CLIQUE && e.y >= bh && e.y < bh + 28 && e.x >= j->larg - 120 && e.x < j->larg - 20;
        bool b_esq = e.tipo == EV_CLIQUE && e.y >= bh && e.y < bh + 28 && e.x >= 20 && e.x < 120;
        if (e.tipo == EV_TECLA && e.tecla == '\n') b_dir = true;
        if (pagina == P_DISCO) {
            if (b_esq || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) break;
            if (e.tipo == EV_CLIQUE && e.y >= 106 && e.y < 106 + ndest * 46) {
                int i = (e.y - 106) / 46;
                if (i < ndest && destino_ok[i]) { sel_dest = i; desenhar(); }
            }
            if (b_dir && sel_dest >= 0) {
                sel_op = 0;
                ler_opcoes();
                recalcular();
                pagina = P_MODO;
                desenhar();
                registrar_layout();
            }
        } else if (pagina == P_MODO) {
            if (b_esq) { pagina = P_DISCO; ler_destinos(); desenhar(); registrar_layout(); continue; }
            if (e.tipo == EV_CLIQUE && e.y >= 80 && e.y < 80 + nops * 28) { sel_op = (e.y - 80) / 28; recalcular(); desenhar(); registrar_layout(); continue; }
            if (ops[sel_op].tabela != T_GPT && (casilla_tratar(&cas_apps, &e) || casilla_tratar(&cas_copiar, &e) || casilla_tratar(&cas_esp, &e))) { recalcular(); desenhar(); registrar_layout(); continue; }
            if (b_dir && !plano_erro[0]) comecar();
        } else if (pagina == P_FIM) {
            if (b_dir) break;
        }
    }
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && (compara_texto(argv[1], "--ventana") == 0 || compara_texto(argv[1], "-v") == 0)) return janela();
    return terminal(argc, argv);
}
