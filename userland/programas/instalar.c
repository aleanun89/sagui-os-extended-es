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
#include "pbkdf2.h"

#define MB 2048u
#define CAB_ASSINATURA 0x44484753u
#define CAB_SIS_SETORES   136      /* sectores de las particiones del sistema: juegos y aplicaciones (D181, D194) */
#define CAB_ID_INSTALACAO 144      /* 'SGIN', aleatorio, hora (boot/cabecalho.inc) */
#define CAB_OPCOES        6        /* u16 (boot/cabecalho.inc) */
#define OPC_CD            8        /* imagen de CD: el disco de arranque es de solo lectura (D077) */
#define JOGOS_ORIGEM "/jogos"             /* la partición de juegos montada, o el /jogos del ramdisk (D181, D228) */
#define JOGOS_INSTALAR_MB 800u            /* D228: sitio para meter la versión completa de un juego */
#define APPS_ORIGEM  "/apps"               /* las aplicaciones fuera de la base (D194) */
#define EFI_ORIGEM "/sistema/efi"          /* D266: lo que se copia a la ESP (EFI/BOOT/BOOTX64.EFI...); D270: va en el ramdisk */
#define MBR_ORIGEM "/sistema/arranque/mbr.bin"   /* D270: el MBR de SaguiOS (BIOS), para instalar con MBR desde un disco GPT */
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
    uint32_t cab_origem;         /* D270: sector de la cabecera en el origen: 1, o el de su partición GPT de SaguiOS */
    bool     tabela_auto;        /* D270: la tabla la eligió el modo automático */
    char     perde[400];         /* qué se borra, dicho para la persona */
    /* D318: tamaños elegidos (MB; 0 = automático: /home, lo que sobre), sistema de archivos de cada
     * partición de datos (PA_FS_EXT2 o PA_FS_FAT32; 0 = ext2) y /tmp y /logs en disco */
    uint32_t home_mb, apps_mb, jogos_mb, tmp_mb, logs_mb;
    bool     tmp, logs;
    int      fs_home, fs_apps, fs_jogos, fs_tmp, fs_logs;
    uint32_t tmp_set, logs_set;
    /* D318: «junto a lo que hay»: antes de instalar, la partición reduzir_part se encoge a
     * reduzir_set sectores (primero su sistema de archivos) y se instala en el hueco que deja */
    bool     reduzir;
    int      reduzir_part;
    uint32_t reduzir_set, reduzir_antes;
    char     reduzir_txt[160];
    /* D318: opciones del sistema instalado (van a SAGUI-CONF y a /home) */
    char     nome_equipo[32];
    char     zona[40];
    int      teclado;            /* -1 = el de ahora; TECLADO_ES o TECLADO_ABNT2 */
    char     senha_root[64], senha_usuario[64];
    char     conta[32];          /* D371: la cuenta de la persona (uid 1000); "" = sagui */
    bool     conta_admin;        /* D371 (P1 b): la cuenta es administradora (grupo admin, su propia contraseña) */
    bool     inicio_auto;        /* D371 (P5): entrar sin pedir la cuenta (de serie, no) */
    bool     sshd, vncd;
    unsigned copiar_conf;        /* CC_* */
    bool     sem_copiar_apps;    /* crear /apps vacía (sin copiar las aplicaciones de este sistema) */
} plano_t;

/* D318: lo que se copia de la sesión en vivo al /home instalado */
enum { CC_PREFS = 1, CC_WIFI = 2, CC_SSH = 4, CC_CORREO = 8, CC_IA = 16, CC_IA_CHAVE = 32, CC_DOCS = 64 };

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

/* D318: ¿aparece "palavra" como elemento de una lista separada por comas ("home,apps")? */
static bool acha_texto(const char *lista, const char *palavra)
{
    int n = (int)tam_texto(palavra);
    for (const char *l = lista; *l; ) {
        const char *f = acha_char(l, ',');
        int len = f ? (int)(f - l) : (int)tam_texto(l);
        if (len == n && memcmp(l, palavra, (unsigned)n) == 0) return true;
        l += len;
        if (*l == ',') l++;
    }
    return false;
}

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

/* D266, D270: ¿se arrancó por UEFI? El cargador UEFI lo dice en la boot_info (campo firmware, D264) y
 * el kernel lo devuelve en INFO_ARRANQUE. */
static bool arrancou_por_uefi(void)
{
    info_arranque_t a;
    memset(&a, 0, sizeof a);
    return info_sistema(INFO_ARRANQUE, &a, sizeof a) == 0 && a.firmware != 0;
}

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

/* KB de /jogos: con «todo», todo; sin él, solo los juegos de la base. D318: se recuerda (el
 * asistente rehace el plan a cada tecla y recorrer /jogos entero cada vez sería lento). */
static uint32_t kb_jogos_de_verdade(bool todo);
static uint32_t kb_jogos(bool todo)
{
    static uint32_t cache[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
    if (cache[todo] == 0xFFFFFFFFu) cache[todo] = kb_jogos_de_verdade(todo);
    return cache[todo];
}

static uint32_t kb_jogos_de_verdade(bool todo)
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
static uint32_t kb_apps(void)   /* D318: recordado, como kb_jogos */
{
    static uint32_t kb = 0xFFFFFFFFu;
    if (kb == 0xFFFFFFFFu) kb = existe_pasta(APPS_ORIGEM) ? kb_pasta(APPS_ORIGEM, 0) : 0;
    return kb;
}
static bool ha_apps(void) { return kb_apps() > 0; }

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
    /* La cabecera y el MBR del origen. D270: si el sistema arrancó de un disco instalado con GPT, su
     * cabecera está en el primer sector de la partición de SaguiOS (el kernel dice cuál) y el LBA 0
     * es el MBR protector: el código de arranque por BIOS se toma del ramdisk. */
    info_bloco_t bo;
    p->cab_origem = blocos_info(p->origem, &bo) == 0 && bo.cab_lba ? bo.cab_lba : 1;
    if (blocos_ler(p->origem, p->cab_origem, 1, p->cab) < 0 || le32(p->cab) != CAB_ASSINATURA) {
        txt(erro, max, "la cabecera de arranque de %s no se puede leer", p->origem);
        return false;
    }
    bool mbr_ok = false;
    if (p->cab_origem == 1) mbr_ok = blocos_ler(p->origem, 0, 1, p->mbr_origem) == 0;
    else {
        int fd = abrir(MBR_ORIGEM, ABRIR_LER);
        if (fd >= 0) { mbr_ok = ler(fd, p->mbr_origem, 512) == 512 && p->mbr_origem[510] == 0x55 && p->mbr_origem[511] == 0xAA; fechar(fd); }
    }
    p->blob_src = le32(p->cab + 8);
    uint32_t total = le32(p->cab + 48);
    if (p->blob_src <= p->cab_origem || total <= p->blob_src || total - p->blob_src > 1024u * MB) {
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
    p->tabela_auto = p->tabela == T_AUTO;
    if (p->tabela == T_AUTO) p->tabela = p->modo == M_ENTERO ? tabela_automatica(d.setores) : T_MBR;   /* D270: una GPT, solo en el disco entero */
    /* D283: en un NVMe de bloques de 4096 el LBA 1 del arranque por BIOS no existe (la BIOS no arranca de NVMe) y el cargador
     * UEFI busca la cabecera en la partición de SaguiOS de una GPT: allí solo se instala con GPT. */
    if (d.flags & DISCO_BLOCO4K) {
        if (p->tabela_auto && p->modo == M_ENTERO) p->tabela = T_GPT;
        if (p->tabela != T_GPT) { txt(erro, max, "%s tiene bloques de 4096 bytes: solo se instala con GPT, en el disco entero (--gpt)", p->destino); return false; }
    }
    if (p->tabela == T_MBR && !mbr_ok) {
        txt(erro, max, "no se encuentra el código de arranque por BIOS (%s): instala con --gpt (arranque UEFI)", MBR_ORIGEM);
        return false;
    }
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
        p->blob_dst = 1 + (p->blob_src - p->cab_origem);   /* la cabecera en el LBA 1 y el resto detrás, como en el origen */
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
        uint32_t blob_total = p->blob_src - p->cab_origem + p->blob_set;
        uint32_t smb = (blob_total * 2 + MB - 1) / MB;
        if (smb < 64) smb = 64;
        p->esp_ini = MB; p->esp_set = esp_set;
        p->sys_ini = p->esp_ini + p->esp_set;
        p->sys_set = smb * MB;
        p->blob_dst = p->sys_ini + (p->blob_src - p->cab_origem);   /* la cabecera va en el primer sector de la partición */
        p->home_ini = p->sys_ini + p->sys_set;
        uint32_t fim_g = (t.setores - ((d.flags & DISCO_BLOCO4K) ? 41u : 33u)) / MB * MB;   /* D283: en 4096 la GPT final ocupa 5 bloques de 4096 */
        p->conf_set = CONF_MB * MB;
        if (fim_g < p->home_ini + p->conf_set + (p->sin_home ? 0u : 16u * MB)) { txt(erro, max, "no cabe: hacen falta al menos %u MB para ESP, sistema, configuración%s", (p->home_ini + p->conf_set) / MB + (p->sin_home ? 0u : 16u), p->sin_home ? "" : " y /home"); return false; }
        p->home_set = p->sin_home ? 0 : fim_g - p->home_ini - p->conf_set;
        if (!p->sin_home && p->home_mb && p->home_mb * MB < p->home_set) p->home_set = p->home_mb * MB;   /* D318 */
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
        if (p->reduzir) {   /* D318: «junto a lo que hay»: el hueco sale de encoger una partición */
            pt_part_t *x = pt_achar(&t, p->reduzir_part);
            char n[16], e[160], tp[16];
            info_disco_t q;
            if (!x || pt_tipo_extendida(x->tipo) || x->tipo == 0xDA || x->tipo == 0xEF) { txt(erro, max, "esa partición no se puede reducir"); return false; }
            pt_nome_particao(p->destino, p->reduzir_part, n, sizeof n);
            if (x->tipo == 0x07) { txt(erro, max, "%s es NTFS: SaguiOS no escribe en NTFS y no la reduce (redúcela desde Windows y usa el espacio libre)", n); return false; }
            uint32_t ocupa = fs_tamanho(n, tp, sizeof tp);
            if (!ocupa) { txt(erro, max, "%s no es FAT32 ni ext2: SaguiOS no sabe reducirla", n); return false; }
            uint32_t min = fs_minimo(n);
            if (!min || p->reduzir_set < min) { txt(erro, max, "%s no se puede reducir por debajo de %u MB sin mover datos", n, min / MB); return false; }
            p->reduzir_antes = x->setores;
            if (p->reduzir_set >= x->setores) { txt(erro, max, "el tamaño nuevo de %s tiene que ser menor que el de ahora (%u MB)", n, x->setores / MB); return false; }
            if (pt_redimensionar(&t, p->reduzir_part, p->reduzir_set, e, sizeof e) < 0) { txt(erro, max, "%s: %s", n, e); return false; }
            x = pt_achar(&t, p->reduzir_part);
            p->reduzir_set = x->setores;
            p->hueco_ini = alinhar_cima(x->inicio + x->setores);
            if (p->modo == M_ENTERO || p->modo == M_PARTICION) p->modo = M_HUECO;
            tamanho(p->reduzir_antes, e, 16);
            char d2[16];
            tamanho(p->reduzir_set, d2, sizeof d2);
            txt(p->reduzir_txt, sizeof p->reduzir_txt, "%s (%s%s%s%s) se reduce de %s a %s: sus archivos se conservan", n, tp,
                info_de(n, &q) && q.rotulo[0] ? " \"" : "", info_de(n, &q) ? q.rotulo : "", info_de(n, &q) && q.rotulo[0] ? "\"" : "", e, d2);
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
    /* Los datos: /home se queda con lo que sobre (o lo pedido); /jogos y /apps según el espacio (o lo
     * pedido); /tmp y /logs (D318) con lo pedido. Cada lógica gasta 1 MB de EBR. */
    uint32_t disp = p->ext_set / MB;
    if (disp < 2 + CONF_MB + 16) { txt(erro, max, "no cabe: falta sitio para SAGUI-CONF (%u MB) y /home", CONF_MB); return false; }
    disp -= 2 + CONF_MB;      /* los EBR de /home y SAGUI-CONF, y D256: SAGUI-CONF, la última de las lógicas */
    p->conf_set = CONF_MB * MB;
    uint32_t tm = 0, lg = 0;
    if (p->tmp) { tm = p->tmp_mb ? p->tmp_mb : 256; disp = disp > tm + 1 ? disp - tm - 1 : 0; }
    if (p->logs) { lg = p->logs_mb ? p->logs_mb : 128; disp = disp > lg + 1 ? disp - lg - 1 : 0; }
    uint32_t jg = 0, ap = 0;
    uint32_t dados_apps = p->apps && ha_apps() ? kb_apps() / 1024 + 1 : 0;
    if (p->apps && p->apps_mb) {
        ap = p->apps_mb;
        if (ap < dados_apps + 16) { txt(erro, max, "/apps necesita al menos %u MB (lo que ocupan las aplicaciones más 16)", dados_apps + 16); return false; }
        disp = disp > 1 ? disp - 1 : 0;
    } else if (p->apps && disp >= 16 + 40 + 40) {
        disp -= 1;
        ap = disp * 15 / 100; if (ap < 40) ap = 40; if (ap > 4096) ap = 4096;
        /* D194: al menos el doble de lo que hay en /apps más 16 MB, para instalar otras. */
        if (ap < dados_apps * 2 + 16) ap = dados_apps * 2 + 16;
    } else p->apps = false;
    /* Juegos (D181, D228): 800 MB, para meter desde el SaguiOS la versión completa de un
     * juego, o lo que quepa: como mucho la mitad de lo que queda tras /apps (el resto es
     * para /home). Nunca menos del doble de lo que se copia más 8 MB (o 32 MB). */
    if (p->jogos) {
        uint32_t dados = kb_jogos(p->copiar_jogos && ha_jogos()) / 1024 + 1;
        uint32_t minimo = dados * 2 + 8;
        if (minimo < 32) minimo = 32;
        if (p->jogos_mb) {
            jg = p->jogos_mb;
            if (jg < dados + 8) { txt(erro, max, "la partición de juegos necesita al menos %u MB", dados + 8); return false; }
            disp = disp > 1 ? disp - 1 : 0;
        } else {
            uint32_t resto = disp > ap + 1 ? disp - ap - 1 : 0;
            jg = JOGOS_INSTALAR_MB;
            if (jg < minimo) jg = minimo;
            if (jg > resto / 2) jg = resto / 2;
            if (jg < minimo) { p->jogos = false; p->copiar_jogos = false; jg = 0; }
            else disp -= 1;
        }
    }
    /* Un FAT32 de SaguiOS necesita al menos 34 MB (formatacao.c). */
    struct { bool usa; int fs; uint32_t mb; const char *nome; } fat[] = {
        { p->apps, p->fs_apps, ap, "/apps" }, { p->jogos, p->fs_jogos, jg, "juegos" },
        { p->tmp, p->fs_tmp, tm, "/tmp" }, { p->logs, p->fs_logs, lg, "/logs" } };
    for (int i = 0; i < 4; i++)
        if (fat[i].usa && fat[i].fs == PA_FS_FAT32 && fat[i].mb < 34) { txt(erro, max, "%s en FAT32 necesita al menos 34 MB", fat[i].nome); return false; }
    uint32_t fixo = jg + ap;
    if (disp < fixo + 16) { txt(erro, max, "no cabe: hacen falta al menos %u MB más", fixo + 16 - disp); return false; }
    uint32_t hm = p->home_mb ? p->home_mb : disp - fixo;
    if (hm > disp - fixo) { txt(erro, max, "no cabe: /home puede tener como mucho %u MB con lo demás que se ha pedido", disp - fixo); return false; }
    if (hm < 16 || (p->fs_home == PA_FS_FAT32 && hm < 34)) { txt(erro, max, "/home necesita al menos %u MB", p->fs_home == PA_FS_FAT32 ? 34 : 16); return false; }
    p->apps_set = ap * MB;
    p->jogos_set = jg * MB;
    p->tmp_set = tm * MB;
    p->logs_set = lg * MB;
    p->home_set = hm * MB;
    if (p->home_set > 0xFFFFFFFFu - MB) p->home_set = 0xFFFFFFFFu - MB;
    return true;
}

static void descrever_plano(const plano_t *p, char *s, int max)
{
    char a[16], h[16], e[16], parte[96];
    tamanho(p->sys_set + 1, a, sizeof a);
    tamanho(p->home_set, h, sizeof h);
    tamanho(p->esp_set, e, sizeof e);
    txt(s, max, "SaguiOS %s", a);
    /* D318: cada partición de datos con su tamaño y su sistema de archivos */
    struct { bool usa; uint32_t set; int fs; const char *nome; } v[] = {
        { p->home_set != 0, p->home_set, p->fs_home, "/home" }, { p->tmp, p->tmp_set, p->fs_tmp, "/tmp" },
        { p->logs, p->logs_set, p->fs_logs, "/logs" }, { p->jogos, p->jogos_set, p->fs_jogos, "juegos" },
        { p->apps, p->apps_set, p->fs_apps, "/apps" } };
    for (int i = 0; i < 5; i++) {
        if (!v[i].usa) continue;
        tamanho(v[i].set, h, sizeof h);
        txt(parte, sizeof parte, "  |  %s %s (%s)", v[i].nome, h, v[i].fs == PA_FS_FAT32 ? "FAT32" : "ext2");
        junta_texto(s, parte, (size_t)max);
    }
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

/* D282: la copia conserva dueño, grupo y permisos (el instalador corre como root).
 * En FAT32 (la ESP) no se puede y no importa. */
static void copiar_permisos(const char *de, const char *para)
{
    info_permisos_t p;
    if (permisos_arquivo(de, &p) < 0 || !p.com_donos) return;
    mudar_dono(para, (int)p.uid, (int)p.gid);
    mudar_permisos(para, p.modo);
}

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
    if (r == 0) copiar_permisos(de, para);
    bytes_copiados += total;
    arquivos_copiados++;
    return r;
}

static int copiar_pasta(const char *de, const char *para, char *erro, int max, int nivel)
{
    if (nivel > 12) return 0;
    criar_pasta(para);
    copiar_permisos(de, para);
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

/* ---- D318: copiar la configuración de la sesión en vivo y escribir las opciones ---- */
static int escrever_texto_arquivo(const char *caminho, const char *t);

/* ¿Existe la ruta? */
static bool existe(const char *c) { info_arquivo_t i; return info_arquivo(c, &i) == 0; }

/* KB de un archivo o una carpeta (0 si no existe). */
static uint32_t kb_de(const char *c)
{
    info_arquivo_t i;
    if (info_arquivo(c, &i) < 0) return 0;
    return i.tipo == TIPO_PASTA ? kb_pasta(c, 0) : i.tamanho / 1024 + 1;
}

/* Lo que va en «preferencias»: /home/.config sin lo que tiene su propia casilla. */
static bool conf_propia(const char *nome)
{
    static const char *const v[] = { "wifi.conf", "correo", "correo.libreta", "agentes", "ia" };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) if (compara_texto(nome, v[i]) == 0) return true;
    return false;
}

/* KB que se copiarían con cada casilla (para la ventana y el resumen). */
static uint32_t kb_copia(unsigned cc)
{
    uint32_t kb = 0;
    entrada_pasta_t e;
    char c[256];
    if (cc & CC_PREFS)
        for (int i = 0; listar("/home/.config", i, &e) > 0; i++) {
            if (e.nome[0] == '.' && (!e.nome[1] || e.nome[1] == '.')) continue;
            if (conf_propia(e.nome)) continue;
            caminho_juntar(c, sizeof c, "/home/.config", e.nome);
            kb += kb_de(c);
        }
    if (cc & CC_WIFI) kb += kb_de("/home/.config/wifi.conf");
    if (cc & CC_SSH) kb += kb_de("/home/.ssh");
    if (cc & CC_CORREO) kb += kb_de("/home/.config/correo") + kb_de("/home/.config/correo.libreta") + kb_de("/home/correo");
    if (cc & CC_IA) {
        kb += kb_de("/home/.config/agentes") + kb_de("/home/.config/ia");
        if (!(cc & CC_IA_CHAVE)) { uint32_t k = kb_de("/home/.config/ia/chave"); kb = kb > k ? kb - k : 0; }
    }
    if (cc & CC_DOCS)
        for (int i = 0; listar("/home", i, &e) > 0; i++) {
            if (e.nome[0] == '.' || compara_texto(e.nome, "lost+found") == 0 || compara_texto(e.nome, "correo") == 0) continue;
            caminho_juntar(c, sizeof c, "/home", e.nome);
            kb += kb_de(c);
        }
    return kb;
}

/* Copia un archivo o una carpeta de /home (rel = ".ssh", ".config/wifi.conf"...) al /home nuevo. */
static int copiar_rel(const char *ponto, const char *rel, char *erro, int max)
{
    char de[160], para[160];
    caminho_juntar(de, sizeof de, "/home", rel);
    caminho_juntar(para, sizeof para, ponto, rel);
    info_arquivo_t i;
    if (info_arquivo(de, &i) < 0) return 0;
    return i.tipo == TIPO_PASTA ? copiar_pasta(de, para, erro, max, 1) : copiar_arquivo(de, para, erro, max);
}

static uint32_t arquivos_conf;
static int copiar_configuracao(plano_t *p, const char *ponto, char *erro, int max)
{
    if (!p->copiar_conf) return 0;
    uint32_t antes = arquivos_copiados;
    int r = 0;
    char c[160];
    caminho_juntar(c, sizeof c, ponto, ".config");
    criar_pasta(c);
    entrada_pasta_t e;
    if (p->copiar_conf & CC_PREFS) {
        fase(fase_atual, 0, "Copiando las preferencias (/home/.config)...");
        for (int i = 0; r == 0 && listar("/home/.config", i, &e) > 0; i++) {
            if ((e.nome[0] == '.' && (!e.nome[1] || e.nome[1] == '.')) || conf_propia(e.nome)) continue;
            char rel[96];
            txt(rel, sizeof rel, ".config/%s", e.nome);
            r = copiar_rel(ponto, rel, erro, max);
        }
    }
    if (r == 0 && (p->copiar_conf & CC_WIFI)) r = copiar_rel(ponto, ".config/wifi.conf", erro, max);
    if (r == 0 && (p->copiar_conf & CC_SSH)) { fase(fase_atual, 0, "Copiando las claves SSH (/home/.ssh)..."); r = copiar_rel(ponto, ".ssh", erro, max); }
    if (r == 0 && (p->copiar_conf & CC_CORREO)) {
        fase(fase_atual, 0, "Copiando las cuentas de correo...");
        if ((r = copiar_rel(ponto, ".config/correo", erro, max)) == 0 && (r = copiar_rel(ponto, ".config/correo.libreta", erro, max)) == 0)
            r = copiar_rel(ponto, "correo", erro, max);
    }
    if (r == 0 && (p->copiar_conf & CC_IA)) {
        fase(fase_atual, 0, "Copiando los agentes de IA y su configuración...");
        if ((r = copiar_rel(ponto, ".config/agentes", erro, max)) == 0) {
            caminho_juntar(c, sizeof c, ponto, ".config/ia");
            criar_pasta(c);
            for (int i = 0; r == 0 && listar("/home/.config/ia", i, &e) > 0; i++) {
                if (e.nome[0] == '.' && (!e.nome[1] || e.nome[1] == '.')) continue;
                if (compara_texto(e.nome, "chave") == 0 && !(p->copiar_conf & CC_IA_CHAVE)) continue;   /* la clave de la API, solo si se pide */
                char rel[96];
                txt(rel, sizeof rel, ".config/ia/%s", e.nome);
                r = copiar_rel(ponto, rel, erro, max);
            }
        }
    }
    if (r == 0 && (p->copiar_conf & CC_DOCS)) {
        fase(fase_atual, 0, "Copiando los documentos de /home...");
        for (int i = 0; r == 0 && listar("/home", i, &e) > 0; i++) {
            if (e.nome[0] == '.' || compara_texto(e.nome, "lost+found") == 0 || compara_texto(e.nome, "correo") == 0
                || compara_texto(e.nome, "leeme.txt") == 0) continue;
            r = copiar_rel(ponto, e.nome, erro, max);
        }
    }
    arquivos_conf = arquivos_copiados - antes;
    registrar("instalar: configuración copiada (%x): %u archivos", p->copiar_conf, arquivos_conf);
    return r;
}

/* Cambia (o añade) "clave=valor" en un archivo de líneas; el resto se conserva. */
static int fixar_linha(const char *caminho, const char *chave, const char *valor, char sep)
{
    char novo[4096];
    int k = 0;
    bool feito = false;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd >= 0) {
        unsigned tam;
        char *b = ler_todo_fd(fd, &tam);
        fechar(fd);
        if (b) {
            int n = (int)tam_texto(chave);
            for (char *l = b; *l && k < (int)sizeof novo - 200; ) {
                char *f = acha_char(l, '\n');
                int len = f ? (int)(f - l) : (int)tam_texto(l);
                bool esta = len > n && memcmp(l, chave, (unsigned)n) == 0 && l[n] == sep;
                if (esta && !feito) { k += formatar_texto(novo + k, sizeof novo - (size_t)k, "%s%c%s\n", chave, sep, valor); feito = true; }
                else if (!esta && len < (int)sizeof novo - 200 - k) { memcpy(novo + k, l, (unsigned)len); k += len; novo[k++] = '\n'; }
                l += len;
                if (*l == '\n') l++;
            }
            libera(b);
        }
    }
    if (!feito) k += formatar_texto(novo + k, sizeof novo - (size_t)k, "%s%c%s\n", chave, sep, valor);
    fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, novo, (unsigned)k);
    fechar(fd);
    return r < 0 ? r : 0;
}

/* El texto de /etc/zona para una zona de la tabla. */
static bool texto_zona(const char *nome, char *t, int max)
{
    const zona_info_t *z = zona_achar(nome);
    if (!z) return false;
    txt(t, max, "# Zona horaria de SaguiOS (la escribió el instalador; se cambia con \"zona\", D080).\n"
                "zona=%s\ntz=%s\nformato=24\n", z->nome, z->tz);
    return true;
}

/* D371: la cuenta de la persona (la principal, uid 1000): sagui salvo que se elija otro nombre. */
static const char *conta_de(const plano_t *p) { return p->conta[0] ? p->conta : "sagui"; }
static bool conta_e_sagui(const plano_t *p) { return compara_texto(conta_de(p), "sagui") == 0; }

/* D371: ¿nombre válido para la cuenta? [a-z0-9_-]{1,31}, que no sea root, servicio ni admin. */
static bool nome_conta_valido(const char *n)
{
    int k = 0;
    for (; n[k]; k++)
        if (!((n[k] >= 'a' && n[k] <= 'z') || (n[k] >= '0' && n[k] <= '9') || n[k] == '_' || n[k] == '-')) return false;
    return k > 0 && k < 32 && compara_texto(n, "root") != 0 && compara_texto(n, "servicio") != 0 &&
           compara_texto(n, "admin") != 0;
}

/* D371: /etc/usuarios con la cuenta elegida como la principal (uid 1000, /home/<nombre>): sin la línea de
 * sagui ni otra con el uid 1000; el resto (de una SAGUI-CONF conservada) se queda. */
static int usuarios_com_conta(const plano_t *p, const char *arq)
{
    static char novo[4096];
    int k = formatar_texto(novo, sizeof novo, "# /etc/usuarios - las cuentas de SaguiOS (D255, D371). nombre:uid:gid:carpeta:shell:descripción\n");
    bool tem_root = false, tem_serv = false;
    int fd = abrir(arq, ABRIR_LER);
    if (fd >= 0) {
        unsigned tam;
        char *b = ler_todo_fd(fd, &tam);
        fechar(fd);
        for (char *l = b; b && *l && k < (int)sizeof novo - 200; ) {
            char *f = acha_char(l, '\n');
            int len = f ? (int)(f - l) : (int)tam_texto(l);
            char lin[200];
            int ln = len < (int)sizeof lin - 1 ? len : (int)sizeof lin - 1;
            memcpy(lin, l, (unsigned)ln);
            lin[ln] = 0;
            char *c1 = acha_char(lin, ':');
            bool fora = lin[0] == '#' || !c1;
            if (!fora) {
                *c1 = 0;
                fora = compara_texto(lin, "sagui") == 0 || compara_texto(lin, conta_de(p)) == 0 || comeca_com(c1 + 1, "1000:");
                if (compara_texto(lin, "root") == 0) tem_root = true;
                if (compara_texto(lin, "servicio") == 0) tem_serv = true;
            }
            if (!fora && len < (int)sizeof novo - 200 - k) { memcpy(novo + k, l, (unsigned)len); k += len; novo[k++] = '\n'; }
            l += len;
            if (*l == '\n') l++;
        }
        if (b) libera(b);
    }
    if (!tem_root) k += formatar_texto(novo + k, sizeof novo - (size_t)k, "root:0:0:/root:/bin/sagsh:Administrador\n");
    if (!tem_serv) k += formatar_texto(novo + k, sizeof novo - (size_t)k, "servicio:100:100:/::Servicios sin escritorio\n");
    k += formatar_texto(novo + k, sizeof novo - (size_t)k, "%s:1000:1000:/home/%s:/bin/sagsh:Usuario del escritorio\n",
                        conta_de(p), conta_de(p));
    return escrever_texto_arquivo(arq, novo);
}

/* D282: todo el /home nuevo es de sagui (como en las imágenes, mkimagem.py), sin cambiar los modos: lo
 * que escribe el instalador lo crea root, y una clave SSH copiada sigue con su 0600. */
static void dono_sagui(const char *c, int nivel)
{
    if (nivel > 16) return;
    mudar_dono(c, UID_SAGUI, GID_SAGUI);
    entrada_pasta_t e;
    for (int i = 0; listar(c, i, &e) > 0; i++) {
        if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0 || compara_texto(e.nome, "lost+found") == 0) continue;
        char f[256];
        caminho_juntar(f, sizeof f, c, e.nome);
        if (e.tipo == TIPO_PASTA) dono_sagui(f, nivel + 1);
        else mudar_dono(f, UID_SAGUI, GID_SAGUI);
    }
}

/* En el /home nuevo: el teclado (configuración del escritorio, D112) y la zona (D080). */
static int opcoes_home(plano_t *p, const char *ponto)
{
    char c[160], t[300];
    caminho_juntar(c, sizeof c, ponto, ".config");
    criar_pasta(c);
    if (p->teclado >= 0) {
        caminho_juntar(c, sizeof c, ponto, ".config/sagui.conf");
        int r = fixar_linha(c, "teclado", p->teclado == TECLADO_ABNT2 ? "abnt2" : "es", '=');
        if (r < 0) return r;
    }
    if (p->zona[0] && texto_zona(p->zona, t, sizeof t)) {
        caminho_juntar(c, sizeof c, ponto, ".zona");
        int r = escrever_texto_arquivo(c, t);
        if (r < 0) return r;
    }
    return 0;
}

/* En SAGUI-CONF nueva (su etc/ se copia sobre /etc al arrancar, D255): contraseñas, nombre del
 * equipo, zona y servicios. La sombra solo lleva hashes PBKDF2 (pbkdf2.h); la contraseña en claro
 * no se escribe en ningún sitio y se borra de la memoria al acabar. */
static int opcoes_conf(plano_t *p, const char *ponto, char *erro, int max)
{
    char c[160], t[300];
    caminho_juntar(c, sizeof c, ponto, "etc");
    criar_pasta(c);
    caminho_juntar(c, sizeof c, ponto, "etc/servicios");
    criar_pasta(c);
    int r = 0;
    (void)pbkdf2_conferir;   /* pbkdf2.h trae también la comprobación, que aquí no hace falta */
    struct { const char *conta, *senha; } s[] = { { "root", p->senha_root }, { conta_de(p), p->senha_usuario } };
    for (int i = 0; i < 2 && r == 0; i++) {
        if (!s[i].senha[0]) continue;
        char h[PBKDF2_TEXTO_MAX];
        if (pbkdf2_gerar(s[i].senha, h, sizeof h) < 0) { txt(erro, max, "no se puede preparar la contraseña de %s", s[i].conta); return ERRO_INVALIDO; }
        caminho_juntar(c, sizeof c, ponto, "etc/sombra");
        r = fixar_linha(c, s[i].conta, h, ':');
        if (r == 0) mudar_permisos(c, 0600);   /* D282: la sombra, solo root */
        memset(h, 0, sizeof h);
    }
    if (r == 0 && !conta_e_sagui(p)) {   /* D371: la cuenta elegida es la principal (uid 1000) */
        caminho_juntar(c, sizeof c, ponto, "etc/usuarios");
        r = usuarios_com_conta(p, c);
    }
    if (r == 0 && p->conta_admin) {   /* D371 (P1 b): «la misma contraseña»: la cuenta es administradora */
        char lin[64];
        caminho_juntar(c, sizeof c, ponto, "etc/grupos");
        txt(lin, sizeof lin, "10:%s", conta_de(p));
        r = fixar_linha(c, "admin", lin, ':');
    }
    if (r == 0) {   /* D371: la configuración de la máquina ya va en /etc (sistema.conf es además la marca) */
        caminho_juntar(c, sizeof c, ponto, "etc/sistema.conf");
        info_arquivo_t ia;
        /* P5: en lo instalado, sin inicio automático de serie (la pantalla o «login» piden la cuenta). */
        if (info_arquivo(c, &ia) < 0)
            r = escrever_texto_arquivo(c, p->inicio_auto ? "# Configuración de la máquina (D371): solo la cambia el administrador.\ninicio_automatico=si\n"
                                                         : "# Configuración de la máquina (D371): solo la cambia el administrador.\ninicio_automatico=no\n");
        if (r == 0 && p->teclado >= 0) r = fixar_linha(c, "teclado", p->teclado == TECLADO_ABNT2 ? "abnt2" : "es", '=');
    }
    if (r == 0 && p->nome_equipo[0]) {
        caminho_juntar(c, sizeof c, ponto, "etc/hostname");
        txt(t, sizeof t, "%s\n", p->nome_equipo);
        r = escrever_texto_arquivo(c, t);
    }
    if (r == 0 && p->zona[0] && texto_zona(p->zona, t, sizeof t)) {
        caminho_juntar(c, sizeof c, ponto, "etc/zona");
        r = escrever_texto_arquivo(c, t);
    }
    if (r == 0) {   /* D250: el sshd, apagado salvo que se pida; lo mismo el VNC */
        caminho_juntar(c, sizeof c, ponto, "etc/servicios/activados");
        if ((r = fixar_linha(c, "sshd", p->sshd ? "si" : "no", '=')) == 0) r = fixar_linha(c, "vncd", p->vncd ? "si" : "no", '=');
    }
    if (r < 0) txt(erro, max, "no se pueden escribir las opciones en SAGUI-CONF: %s", nome_erro(r));
    registrar("instalar: opciones en SAGUI-CONF: claves %s/%s, cuenta '%s'%s, equipo '%s', zona '%s', sshd %d, vncd %d", p->senha_root[0] ? "root" : "-",
              p->senha_usuario[0] ? conta_de(p) : "-", conta_de(p), p->conta_admin ? " (administradora)" : "", p->nome_equipo, p->zona, p->sshd, p->vncd);
    return r;
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
        if (r == 0) mudar_dono(ponto, UID_SAGUI, GID_SAGUI);   /* D282: la persona añade juegos ahí */
    } else if (r == 0 && apps) {
        /* D194: las aplicaciones van con el sistema (con su leeme.txt, si lo traen). */
        if (ha_apps()) {
            fase(fase_atual, 0, "Copiando las aplicaciones de " APPS_ORIGEM "...");
            uint32_t antes = arquivos_copiados;
            r = copiar_pasta(APPS_ORIGEM, ponto, erro, max, 0);
            arquivos_apps += arquivos_copiados - antes;
        }
        if (r == 0) {   /* D282: /apps es de root, pero /apps/vps es de sagui (VisualPythonS guarda ahí, D297), también vacía */
            char v[96];
            caminho_juntar(v, sizeof v, ponto, "vps");
            criar_pasta(v);
            mudar_dono(v, UID_SAGUI, GID_SAGUI);
            mudar_dono(ponto, UID_ROOT, GID_ROOT);
            mudar_permisos(ponto, 0755);
        }
    } else if (r < 0) txt(erro, max, "no se puede escribir en %s: %s", vol, nome_erro(r));
    if (r == 0 && conf && conf_respaldada) {
        /* D256: reinstalación: se devuelve lo que había (contraseñas, claves), también su LEEME. */
        fase(fase_atual, 0, "Devolviendo el contenido de SAGUI-CONF...");
        r = copiar_pasta(CONF_RESPALDO, ponto, erro, max, 0);
    }
    if (r == 0 && conf) r = opcoes_conf(p, ponto, erro, max);   /* D318 */
    if (r == 0 && compara_texto(ponto, "/mnt/instalar-home") == 0) {
        /* D371: con otra cuenta que sagui, lo de la persona va a su carpeta /home/<cuenta> (0700). */
        char casa[96];
        if (conta_e_sagui(p)) copia_texto(casa, ponto, sizeof casa);
        else { caminho_juntar(casa, sizeof casa, ponto, conta_de(p)); criar_pasta(casa); }
        if ((r = copiar_configuracao(p, casa, erro, max)) == 0 && (r = opcoes_home(p, casa)) < 0)
            txt(erro, max, "no se pueden escribir el teclado y la zona en /home: %s", nome_erro(r));
        if (r == 0) dono_sagui(ponto, 0);   /* D282: todo de la cuenta principal (uid 1000) */
        if (r == 0 && !conta_e_sagui(p)) mudar_permisos(casa, 0700);
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
    int num_sys = 0, num_ext = 0, num_home = 0, num_jogos = 0, num_apps = 0, num_conf = 0, num_esp = 0, num_tmp = 0, num_logs = 0;
    bool gpt_novo = p->tabela == T_GPT;
    bool reservado = false, tocado = false;
    bytes_copiados = arquivos_copiados = arquivos_jogos = arquivos_apps = 0;
    registrar("instalar: de %s a %s, modo %d, tabla %s, sistema %u+%u, extendida %u+%u, home %u jogos %u apps %u, esp %u+%u",
              p->origem, p->destino, p->modo, gpt_novo ? "GPT" : "MBR", p->sys_ini, p->sys_set, p->ext_ini, p->ext_set, p->home_set, p->jogos_set, p->apps_set,
              p->esp_ini, p->esp_set);

    /* 0a. D318: «junto a lo que hay»: primero se encoge la partición elegida (pt_executar comprueba su
     * sistema de archivos, lo encoge y después cambia la tabla; si algo falla, el disco queda como
     * estaba). Hasta aquí el disco no se ha marcado como no arrancable: su otro sistema sigue igual. */
    if (p->reduzir) {
        fase(0, 0, p->reduzir_txt);
        pt_acao_t a;
        memset(&a, 0, sizeof a);
        a.op = PA_REDIMENSIONAR;
        copia_texto(a.disco, p->destino, sizeof a.disco);
        a.numero = p->reduzir_part;
        a.setores = p->reduzir_set;
        a.progresso = prog_fmt;
        r = pt_executar(&a, e, sizeof e);
        if (r < 0) { txt(erro, max, "no se ha podido reducir la partición %d: %s. No se ha tocado nada más", p->reduzir_part, e); return r; }
        registrar("instalar: %s", e);
        fase(0, 0, e);
    }

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
            if ((sl = gpt_adicionar(g, p->fs_home == PA_FS_FAT32 ? GPT_TIPO_DADOS_MS : GPT_TIPO_LINUX, p->home_ini, p->home_set, "SAGUI-DATOS", e, sizeof e)) < 0) { libera(g); txt(erro, max, "la GPT (/home): %s", e); r = sl; goto falha; }
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
    num_home = pt_criar(&t, pos, p->home_set, p->fs_home == PA_FS_FAT32 ? 0x0C : 0x83, true, e, sizeof e);
    if (num_home < 0) { txt(erro, max, "/home: %s", e); r = num_home; goto falha; }
    pos += p->home_set + MB;
    if (p->tmp) {   /* D318: el orden de las imágenes (mkimagem): DATOS, TMP, LOGS, JOGOS, APPS, CONF */
        num_tmp = pt_criar(&t, pos, p->tmp_set, p->fs_tmp == PA_FS_FAT32 ? 0x0C : 0x83, true, e, sizeof e);
        if (num_tmp < 0) { txt(erro, max, "/tmp: %s", e); r = num_tmp; goto falha; }
        pos += p->tmp_set + MB;
    }
    if (p->logs) {
        num_logs = pt_criar(&t, pos, p->logs_set, p->fs_logs == PA_FS_FAT32 ? 0x0C : 0x83, true, e, sizeof e);
        if (num_logs < 0) { txt(erro, max, "/logs: %s", e); r = num_logs; goto falha; }
        pos += p->logs_set + MB;
    }
    if (p->jogos) {
        num_jogos = pt_criar(&t, pos, p->jogos_set, p->fs_jogos == PA_FS_FAT32 ? 0x0C : 0x83, true, e, sizeof e);
        if (num_jogos < 0) { txt(erro, max, "juegos: %s", e); r = num_jogos; goto falha; }
        pos += p->jogos_set + MB;
    }
    if (p->apps) {
        num_apps = pt_criar(&t, pos, p->apps_set, p->fs_apps == PA_FS_FAT32 ? 0x0C : 0x83, true, e, sizeof e);   /* D194: ext2, como en la imagen */
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
    /* D318: cada partición de datos con el sistema de archivos elegido (ext2 por defecto). */
    struct { int num, fs; const char *rotulo, *nome; } dados[] = {
        { num_home, p->fs_home, "SAGUI-DATOS", "/home" }, { num_tmp, p->fs_tmp, "SAGUI-TMP", "/tmp" },
        { num_logs, p->fs_logs, "SAGUI-LOGS", "/logs" }, { num_jogos, p->fs_jogos, "SAGUI-JOGOS", "la partición de juegos" },
        { num_apps, p->fs_apps, "SAGUI-APPS", "/apps" } };
    for (int i = 0; i < 5; i++) {
        if (!dados[i].num) continue;
        char t2[80];
        bool fat = dados[i].fs == PA_FS_FAT32;
        pt_nome_particao(p->destino, dados[i].num, vol, sizeof vol);
        txt(t2, sizeof t2, "Formateando %s (%s)...", dados[i].nome, fat ? "FAT32" : "ext2");
        fase(1, 0, t2);
        r = fat ? fs_formatar_fat32(vol, dados[i].rotulo, prog_fmt, NULL, e, sizeof e) : fs_formatar_ext2(vol, dados[i].rotulo, prog_fmt, NULL, e, sizeof e);
        if (r < 0) { txt(erro, max, "%s", e); goto falha; }
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
                            "y los datos de cada una en /apps/<app>.\n", false, !p->sem_copiar_apps, erro, max, false)) < 0) goto falha;
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
    desmontar("/mnt/instalar-conf");
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
    p.teclado = -1;
    bool sim = false;
    for (int i = 1; i < argc; i++) {
        /* D318: tamaños, sistemas de archivos, reducir, opciones y configuración que se copia */
        int v;
        if (i + 1 < argc && (compara_texto(argv[i], "--home-mb") == 0 || compara_texto(argv[i], "--apps-mb") == 0 ||
                             compara_texto(argv[i], "--jogos-mb") == 0 || compara_texto(argv[i], "--tmp-mb") == 0 ||
                             compara_texto(argv[i], "--logs-mb") == 0)) {
            if (!texto_para_numero(argv[i + 1], &v) || v < 0) { imprimir_em(ERROS, "instalar: %s: número no válido\n", argv[i]); return 2; }
            char o = argv[i][2];
            if (o == 'h') p.home_mb = (uint32_t)v;
            else if (o == 'a') p.apps_mb = (uint32_t)v;
            else if (o == 'j') p.jogos_mb = (uint32_t)v;
            else if (o == 't') { p.tmp_mb = (uint32_t)v; p.tmp = v > 0; }
            else { p.logs_mb = (uint32_t)v; p.logs = v > 0; }
            i++;
            continue;
        }
        if (compara_texto(argv[i], "--fat32") == 0 && i + 1 < argc) {   /* --fat32 home,apps,jogos,tmp,logs */
            const char *l = argv[++i];
            if (acha_texto(l, "home")) p.fs_home = PA_FS_FAT32;
            if (acha_texto(l, "apps")) p.fs_apps = PA_FS_FAT32;
            if (acha_texto(l, "jogos") || acha_texto(l, "juegos")) p.fs_jogos = PA_FS_FAT32;
            if (acha_texto(l, "tmp")) p.fs_tmp = PA_FS_FAT32;
            if (acha_texto(l, "logs")) p.fs_logs = PA_FS_FAT32;
            continue;
        }
        if (compara_texto(argv[i], "--reducir") == 0 && i + 1 < argc) {   /* --reducir N:MB */
            const char *l = argv[++i], *dp = acha_char(l, ':');
            int n2, mb;
            char a[8];
            copia_texto(a, l, dp && dp - l < 7 ? (size_t)(dp - l + 1) : sizeof a);
            if (!dp || !texto_para_numero(a, &n2) || !texto_para_numero(dp + 1, &mb) || n2 < 1 || mb < 1) {
                imprimir_em(ERROS, "instalar: --reducir N:MB (la partición N se reduce a MB y se instala en el hueco)\n");
                return 2;
            }
            p.reduzir = true; p.reduzir_part = n2; p.reduzir_set = (uint32_t)mb * MB; p.modo = M_HUECO;
            continue;
        }
        if (compara_texto(argv[i], "--nombre") == 0 && i + 1 < argc) { copia_texto(p.nome_equipo, argv[++i], sizeof p.nome_equipo); continue; }
        if (compara_texto(argv[i], "--zona") == 0 && i + 1 < argc) {
            if (!zona_achar(argv[i + 1])) { imprimir_em(ERROS, "instalar: zona desconocida: %s (mira \"zona lista\")\n", argv[i + 1]); return 2; }
            copia_texto(p.zona, zona_achar(argv[++i])->nome, sizeof p.zona);
            continue;
        }
        if (compara_texto(argv[i], "--teclado") == 0 && i + 1 < argc) { p.teclado = compara_texto(argv[++i], "abnt2") == 0 ? TECLADO_ABNT2 : TECLADO_ES; continue; }
        if (compara_texto(argv[i], "--clave-root") == 0 && i + 1 < argc) { copia_texto(p.senha_root, argv[++i], sizeof p.senha_root); continue; }
        if (compara_texto(argv[i], "--clave-usuario") == 0 && i + 1 < argc) { copia_texto(p.senha_usuario, argv[++i], sizeof p.senha_usuario); continue; }
        if (compara_texto(argv[i], "--cuenta") == 0 && i + 1 < argc) {   /* D371 */
            if (!nome_conta_valido(argv[i + 1])) { imprimir_em(ERROS, "instalar: nombre de cuenta no válido: %s (a-z, 0-9, _ y -)\n", argv[i + 1]); return 2; }
            copia_texto(p.conta, argv[++i], sizeof p.conta);
            continue;
        }
        if (compara_texto(argv[i], "--cuenta-admin") == 0) { p.conta_admin = true; continue; }
        if (compara_texto(argv[i], "--inicio-automatico") == 0) { p.inicio_auto = true; continue; }
        if (compara_texto(argv[i], "--sshd") == 0) { p.sshd = true; continue; }
        if (compara_texto(argv[i], "--vncd") == 0) { p.vncd = true; continue; }
        if (compara_texto(argv[i], "--copiar-config") == 0 && i + 1 < argc) {   /* prefs,wifi,ssh,correo,ia,ia-clave,docs | todo */
            const char *l = argv[++i];
            bool todo = compara_texto(l, "todo") == 0;
            if (todo || acha_texto(l, "prefs")) p.copiar_conf |= CC_PREFS;
            if (todo || acha_texto(l, "wifi")) p.copiar_conf |= CC_WIFI;
            if (todo || acha_texto(l, "ssh")) p.copiar_conf |= CC_SSH;
            if (todo || acha_texto(l, "correo")) p.copiar_conf |= CC_CORREO;
            if (todo || acha_texto(l, "ia")) p.copiar_conf |= CC_IA;
            if (acha_texto(l, "ia-clave")) p.copiar_conf |= CC_IA | CC_IA_CHAVE;
            if (todo || acha_texto(l, "docs")) p.copiar_conf |= CC_DOCS;
            continue;
        }
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
            imprimir_em(ERROS, "uso: instalar [disco [--entero | --hueco MB | --particion N | --reducir N:MB] [--sin-jogos] [--sin-apps]\n"
                               "                      [--copiar-jogos] [--formatear-conf] [--mbr | --gpt | --auto] [--esp] [--sin-home]\n"
                               "                      [--home-mb N] [--apps-mb N] [--jogos-mb N] [--tmp-mb N] [--logs-mb N] [--fat32 home,apps,...]\n"
                               "                      [--nombre EQUIPO] [--zona Z] [--teclado es|abnt2] [--clave-root X] [--clave-usuario X]\n"
                               "                      [--sshd] [--vncd] [--copiar-config prefs,wifi,ssh,correo,ia,ia-clave,docs|todo] [-s]]\n"
                               "     instalar --ventana   (el asistente; también «instalar» sin nada fuera del Terminal)\n");
            return 2;
        } else copia_texto(p.destino, argv[i], sizeof p.destino);
    }
    if (!p.destino[0]) return listar_destinos();
    if (falta_admin("instalar")) return 1;   /* D255 */
    char e[300], d[300];
    if (!planejar(&p, e, sizeof e)) { imprimir_em(ERROS, "instalar: %s\n", e); return 1; }
    descrever_plano(&p, d, sizeof d);
    imprimir("Instalar SaguiOS de %s en %s\n  %s\n  Se borra: %s\n", p.origem, p.destino, d, p.perde);
    imprimir("  Tabla de particiones: %s%s\n", p.tabela == T_GPT ? "GPT (arranque UEFI; con la BIOS clásica no arranca)" : "MBR (arranque BIOS)",
             !p.tabela_auto ? "" : arrancou_por_uefi() ? " [automática: se arrancó por UEFI]" : " [automática: se arrancó por BIOS]");
    if (p.aviso[0]) imprimir("  AVISO: %s\n", p.aviso);
    if (p.conf_vieja[0])
        imprimir(p.formatear_conf ? "  AVISO: se BORRA también SAGUI-CONF (%s): contraseñas y claves del sistema\n"
                                  : "  Se conserva el contenido de SAGUI-CONF (%s): contraseñas y claves (--formatear-conf lo borra)\n", p.conf_vieja);
    if (p.copiar_jogos) imprimir("  Se copian también los datos de " JOGOS_ORIGEM "%s\n", ha_jogos() ? "" : " (no hay datos: nada que copiar)");
    if (p.reduzir) imprimir("  Antes: %s\n", p.reduzir_txt);
    if (p.copiar_conf) imprimir("  Se copia de esta sesión a /home: %u KB de configuración y documentos\n", kb_copia(p.copiar_conf));
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
    if (p.copiar_conf) imprimir("  Configuración de la sesión copiada: %u archivos\n", arquivos_conf);
    if (p.copiar_jogos) imprimir("  Datos de juegos copiados: %u archivos\n", arquivos_jogos);
    if (p.apps) imprimir("  Aplicaciones de " APPS_ORIGEM " copiadas: %u archivos\n", arquivos_apps);
    imprimir("  SAGUI-CONF (configuración del sistema, %u MB)%s\n", CONF_MB, conf_respaldada ? ": contenido anterior conservado" : "");
    if (p.esp) imprimir("  Partición EFI (%u MB, FAT32): %s\n", p.esp_set / MB, p.efi_ok ? "con el cargador UEFI de " EFI_ORIGEM : "vacía (no había cargador UEFI)");
    if (p.tabela == T_GPT) imprimir("  Tabla GPT verificada (principal y respaldo, CRC releídos).\n");
    imprimir(p.desde_cd ? "Apaga, saca el CD y arranca desde %s.\n" : "Apaga, quita el disco de arranque y arranca desde %s.\n", p.destino);
    if (p.tabela == T_GPT) imprimir("Recuerda: el disco es GPT y arranca por UEFI (activa UEFI en la BIOS del PC).\n");
    return 0;
}

/* ------------------------------------------------------------ ventana (D318: el asistente) */

#include "instalar_asistente.h"

int principal(int argc, char **argv)
{
    if (argc >= 2 && (compara_texto(argv[1], "--ventana") == 0 || compara_texto(argv[1], "-v") == 0 ||
                      compara_texto(argv[1], "--asistente") == 0)) return janela();
    /* D318: sin nada y fuera del Terminal (Inicio, el icono del escritorio), el asistente, como «discos» (D121). */
    if (argc == 1 && !eh_terminal(SAIDA)) return janela();
    return terminal(argc, argv);
}
