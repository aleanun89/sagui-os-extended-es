// SPDX-License-Identifier: GPL-3.0-only
/* discos.c - discos, particiones, formatos y avisos (H32, H34, D040).
 *
 * Cuando aparece un disco (pendrive USB o disco ATA), este módulo descubre
 * qué hay en él:
 *   - sector 0 que ya es un sector de arranque FAT32, o un ext2/3/4 en el sector 2
 *     ("superfloppy", sin tabla): el disco entero es el volumen;
 *   - tabla de particiones MBR (cuatro entradas) o GPT (las cuatro primeras):
 *     cada partición usada se convierte en un disco_t hijo ("usb0p1", "hda1"), con el
 *     formato reconocido por el contenido;
 *   - otros formatos (NTFS, exFAT, el propio SaguiOS...) aparecen con el
 *     nombre y "formato no soportado", y no se montan.
 * El disco entero ("usb0") sigue valiendo como nombre del primer volumen que
 * se puede montar, como antes de las particiones (compatibilidad con "montar").
 * Cada conexión, extracción y montaje de pendrive se convierte en un aviso, que
 * el área de trabajo muestra en una esquina de la pantalla. */
#include "disco.h"
#include "abi.h"
#include "bootinfo.h"
#include "caminho.h"
#include "cache_disco.h"
#include "console.h"
#include "fat32.h"
#include "exfat.h"
#include "iso9660.h"
#include "udf.h"
#include "memoria.h"
#include "montagem.h"
#include "permisos.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"

static disco_t discos[MAX_DISCOS];
static int contador_usb, contador_ata, contador_cd, contador_nvme, contador_ram;

#define MAX_AVISOS 8
static struct { int tipo; char texto[256]; char caminho[64]; } avisos[MAX_AVISOS];
static int av_ini, av_fim;

static void avisar(int tipo, const char *caminho, const char *fmt, va_list ap)
{
    formatar(avisos[av_fim].texto, sizeof avisos[av_fim].texto, fmt, ap);
    txt_copia_n(avisos[av_fim].caminho, caminho ? caminho : "", sizeof avisos[av_fim].caminho);
    avisos[av_fim].tipo = tipo;
    av_fim = (av_fim + 1) % MAX_AVISOS;
    if (av_fim == av_ini) av_ini = (av_ini + 1) % MAX_AVISOS;
    klog("[DISCO] aviso: %s\n", avisos[(av_fim + MAX_AVISOS - 1) % MAX_AVISOS].texto);
}

void disco_avisar(int tipo, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    avisar(tipo, NULL, fmt, ap);
    va_end(ap);
}

void disco_avisar_abrir(int tipo, const char *caminho, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    avisar(tipo, caminho, fmt, ap);
    va_end(ap);
}

bool disco_proximo_aviso(int *tipo, char *texto, int max, char *caminho, int max_caminho)
{
    if (av_ini == av_fim) return false;
    *tipo = avisos[av_ini].tipo;
    txt_copia_n(texto, avisos[av_ini].texto, (size_t)max);
    txt_copia_n(caminho, avisos[av_ini].caminho, (size_t)max_caminho);
    av_ini = (av_ini + 1) % MAX_AVISOS;
    return true;
}

disco_t *disco_indice(int i) { return (i >= 0 && i < MAX_DISCOS && discos[i].usado) ? &discos[i] : NULL; }

disco_t *disco_por_nome(const char *nome)
{
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].presente && txt_compara(discos[i].nome, nome) == 0) return &discos[i];
    return NULL;
}

disco_t *disco_raiz(disco_t *d) { return d->pai ? d->pai : d; }

bool disco_montado(disco_t *d)
{
    if (d->montagem) return true;
    if (d->pai) return false;
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].pai == d && discos[i].montagem) return true;
    return false;
}

disco_t *disco_pendrive_montado(void)
{
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = &discos[i];
        if (d->usado && d->presente && !d->pai && d->removivel && !d->de_boot && disco_montado(d)) return d;
    }
    return NULL;
}

/* Sectores relativos al volumen; la partición suma el inicio y no deja pasar
 * del fin (un sistema de archivos estropeado no puede escribir en la vecina). */
int disco_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    if (!d->presente || lba >= d->setores || n > d->setores - lba) return ERRO_ACESSO;
    if (d->pai) return disco_ler(d->pai, d->inicio + lba, n, buf);
    if (cache_disco_aplica(d)) return cache_disco_ler(d, lba, n, buf);   /* D281 */
    return d->ler(d, lba, n, buf) < 0 ? ERRO_ACESSO : 0;
}

static int escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf, bool ja);

int disco_escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf)
{
    return escrever(d, lba, n, buf, false);
}

/* D353: como disco_escrever, pero nunca diferido: llega al driver antes de volver y solo esos
 * sectores (la caché, si tiene el bloque, se pone al día). Para la cabecera de arranque: por la
 * caché diferida se grabaría al vaciar el bloque de 4 KB entero, con el MBR (LBA 0) y la etapa 2,
 * y un corte de luz en ese momento podría dejar mal un sector que no había cambiado. */
int disco_escrever_ja(disco_t *d, uint32_t lba, uint32_t n, const void *buf)
{
    return escrever(d, lba, n, buf, true);
}

static int escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf, bool ja)
{
    if (!d->presente || lba >= d->setores || n > d->setores - lba) return ERRO_ACESSO;
    /* Segundo bloqueo (D040): en un volumen montado sólo para lectura, o en un disco
     * interno que no se montó para escritura, nada se graba. */
    if (!d->rw_autorizado && !d->pai_gravando && (d->montagem || disco_raiz(d)->interno)) {
        klog("[DISCO] %s: gravacao recusada (so leitura)\n", d->nome);
        return ERRO_SO_LEITURA;
    }
    if (d->pai) {
        d->pai->pai_gravando = true;   /* la partición ya fue verificada */
        int r = escrever(d->pai, d->inicio + lba, n, buf, ja);
        d->pai->pai_gravando = false;
        return r;
    }
    if (!d->escrever) return ERRO_ACESSO;
    /* D281: por la caché; solo se difiere lo que escribe un sistema de archivos montado (un disco
     * reservado para el acceso en bruto -formateo, particiones, instalador- graba al instante). */
    /* D379: los pendrives también pasan por la caché (para leer la raíz de la live del medio), pero lo
     * que se les escribe va al instante: se pueden retirar sin avisar. */
    if (cache_disco_aplica(d)) return cache_disco_escrever(d, lba, n, buf, !ja && !disco_reservado(d) && !d->removivel);
    return d->escrever(d, lba, n, buf) < 0 ? ERRO_ACESSO : 0;
}

/* D281: graba lo que la caché tiene pendiente de ese disco (o de todos, con NULL). */
int disco_sincronizar(disco_t *d)
{
    return cache_disco_sincronizar(d ? disco_raiz(d) : NULL);
}

static void tamanho_legivel(uint32_t setores, char *s, size_t max)
{
    uint32_t mb = setores / 2048;
    if (mb >= 1024) snformatar(s, max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else snformatar(s, max, "%u MB", mb);
}

static void copiar_rotulo(disco_t *d, const uint8_t *r, int n)
{
    while (n > 0 && (r[n - 1] == ' ' || r[n - 1] == 0)) n--;
    if (n > (int)sizeof d->rotulo - 1) n = (int)sizeof d->rotulo - 1;
    memcpy(d->rotulo, r, (size_t)n);
    d->rotulo[n] = 0;
    if (txt_compara(d->rotulo, "NO NAME") == 0) d->rotulo[0] = 0;
}

static bool eh_fat32(const uint8_t *s)
{
    return s[510] == 0x55 && s[511] == 0xAA && memcmp(s + 82, "FAT32   ", 8) == 0 &&
           (s[11] | (s[12] << 8)) == 512 && s[13] != 0 && (s[22] | (s[23] << 8)) == 0;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* ext2/3/4: superbloque a 1024 bytes del inicio (sector 2), número mágico 0xEF53.
 * El nombre mostrado sigue las características: diario = ext3; extents, 64 bits,
 * flex_bg... = ext4 (D040). */
static bool reconhecer_ext(disco_t *d, const uint8_t *sb)
{
    if (sb[56] != 0x53 || sb[57] != 0xEF) return false;
    uint32_t compat = le32(sb + 92), incompat = le32(sb + 96), ro = le32(sb + 100);
    const char *tipo = "ext2";
    if (compat & 0x4) tipo = "ext3";
    if ((incompat & ~0x7u) || (ro & ~0x7u)) tipo = "ext4";
    if (le32(sb + 76) == 0) tipo = "ext2";   /* revisión 0 no tiene características */
    txt_copia_n(d->tipo_fs, tipo, sizeof d->tipo_fs);
    txt_copia_n(d->formato, tipo, sizeof d->formato);
    copiar_rotulo(d, sb + 120, 16);
    return true;
}

/* Descubre el formato de un volumen (disco entero sin tabla o partición). */
static bool reconhecer_volume(disco_t *d, const uint8_t *s0, uint8_t *buf)
{
    if (eh_fat32(s0)) {
        txt_copia_n(d->tipo_fs, "fat32", sizeof d->tipo_fs);
        txt_copia_n(d->formato, "FAT32", sizeof d->formato);
        copiar_rotulo(d, s0 + 71, 11);
        /* La etiqueta de verdad queda en una entrada especial en la carpeta raíz. */
        char r[12];
        if (fat_rotulo_raiz(d, r)) txt_copia_n(d->rotulo, r, sizeof d->rotulo);
        return true;
    }
    if (d->setores > 4 && disco_ler(d, 2, 2, buf) == 0 && reconhecer_ext(d, buf)) return true;
    if (exfat_reconhecer(s0)) {   /* D325: exFAT, con lectura y escritura */
        txt_copia_n(d->tipo_fs, "exfat", sizeof d->tipo_fs);
        txt_copia_n(d->formato, "exFAT", sizeof d->formato);
        d->rotulo[0] = 0;
        char r[12];
        if (exfat_rotulo_raiz(d, r)) txt_copia_n(d->rotulo, r, sizeof d->rotulo);
        return true;
    }
    if (memcmp(s0 + 3, "NTFS    ", 8) == 0) { txt_copia_n(d->formato, "NTFS: formato no soportado", sizeof d->formato); return false; }
    return false;
}

static disco_t *novo_registro(void)
{
    for (int i = 0; i < MAX_DISCOS; i++)
        if (!discos[i].usado) { memset(&discos[i], 0, sizeof discos[i]); discos[i].usado = true; return &discos[i]; }
    return NULL;
}

static void nome_particao(disco_t *d, int num, char *nome, size_t max)
{
    if (d->nome[0] == 'u' || d->nome[0] == 'n') snformatar(nome, max, "%sp%d", d->nome, num);
    else snformatar(nome, max, "%s%d", d->nome, num);
}

/* D265: en un disco con bloques reales de 4096 (NVMe), las tablas de particiones cuentan en bloques
 * de 4096 aunque la capa de discos trabaje en sectores de 512: hay que multiplicar por 8 todo LBA
 * que se lea de una tabla (MBR, EBR, cabecera y entradas de la GPT). Saturado a 32 bits. */
static uint32_t escala_tabla(const disco_t *d) { return d->bloco_fisico == 4096 ? 8 : 1; }
static uint32_t escalar(const disco_t *d, uint32_t v)
{
    uint32_t e = escala_tabla(d);
    return v > 0xFFFFFFFFu / e ? 0xFFFFFFFFu : v * e;
}

static disco_t *criar_particao(disco_t *d, int num, uint8_t tipo, uint32_t ini, uint32_t qtd, uint8_t *s, uint8_t *buf)
{
    if (!qtd || ini >= d->setores) return NULL;
    if (qtd > d->setores - ini) qtd = d->setores - ini;
    /* D213: al releer la tabla con particiones montadas (reserva con DBLOCO_CONF_MONTADO), una
     * montada que sigue igual conserva su registro (el montaje apunta a él); solo cambia su
     * número si las de delante cambiaron. */
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *v = &discos[i];
        if (v->usado && v->pai == d && v->montagem && v->inicio == ini && v->setores == qtd && v->tipo_mbr == tipo) {
            if (v->particao != num) {
                char antes[12];
                txt_copia_n(antes, v->nome, sizeof antes);
                v->particao = num;
                nome_particao(d, num, v->nome, sizeof v->nome);
                klog("[DISCO] %s (montada) pasa a llamarse %s\n", antes, v->nome);
            }
            return v;
        }
    }
    disco_t *p = novo_registro();
    if (!p) return NULL;
    p->presente = true;
    nome_particao(d, num, p->nome, sizeof p->nome);
    txt_copia_n(p->fabricante, d->fabricante, sizeof p->fabricante);
    txt_copia_n(p->produto, d->produto, sizeof p->produto);
    p->setores = qtd;
    p->tam_setor = d->tam_setor;
    p->pai = d;
    p->particao = num;
    p->inicio = ini;
    p->tipo_mbr = tipo;
    p->removivel = d->removivel;
    p->interno = d->interno;
    txt_copia_n(p->formato, "desconocido: formato no soportado", sizeof p->formato);
    if (tipo == 0xDA) {
        txt_copia_n(p->formato, "SaguiOS (arranque)", sizeof p->formato);
        return p;
    }
    if (disco_ler(p, 0, 1, s) < 0) { txt_copia_n(p->formato, "error de lectura", sizeof p->formato); return p; }
    if (reconhecer_volume(p, s, buf)) return p;
    if (p->formato[0] != 'd') return p;   /* exFAT/NTFS ya descritos */
    switch (tipo) {
    case 0x0B: case 0x0C: txt_copia_n(p->formato, "FAT32 dañado: no soportado", sizeof p->formato); break;
    case 0x01: case 0x04: case 0x06: case 0x0E:
        txt_copia_n(p->formato, "FAT12/16: formato no soportado", sizeof p->formato); break;
    case 0x07: txt_copia_n(p->formato, "NTFS: formato no soportado", sizeof p->formato); break;
    case 0x82: txt_copia_n(p->formato, "swap de Linux: no se monta", sizeof p->formato); break;
    case 0x83: txt_copia_n(p->formato, "partición Linux: formato no soportado", sizeof p->formato); break;
    case 0x05: case 0x0F: txt_copia_n(p->formato, "partición extendida: no soportada", sizeof p->formato); break;
    default:   snformatar(p->formato, sizeof p->formato, "partición tipo %x: no soportada", tipo);
    }
    return p;
}

/* D266: GUID de tipo de la partición de SaguiOS en una GPT (53414755-494F-4F53-8001-534147554953,
 * los tres primeros campos en little-endian, como va en la entrada). */
static const uint8_t GPT_TIPO_SAGUI[16] = { 0x55, 0x47, 0x41, 0x53, 0x4F, 0x49, 0x53, 0x4F,
                                            0x80, 0x01, 0x53, 0x41, 0x47, 0x55, 0x49, 0x53 };

/* GPT: cabecera en el sector 1 ("EFI PART"), entradas de 128 bytes. Sólo las
 * cuatro primeras usadas y sólo dentro de los primeros 2^32 sectores. */
static int ler_gpt(disco_t *d, disco_t **filhos, uint8_t *s, uint8_t *buf)
{
    int n = 0;
    if (disco_ler(d, escalar(d, 1), 1, s) < 0 || memcmp(s, "EFI PART", 8) != 0) return 0;
    uint32_t lba_ent = escalar(d, le32(s + 72)), qtd = le32(s + 80), tam = le32(s + 84);
    if (tam < 128 || tam > 512 || qtd > 128) return 0;
    uint8_t *e = kmalloc(512);
    if (!e) return 0;
    for (uint32_t i = 0; i < qtd && n < MAX_PRIMARIAS; i++) {
        uint32_t byte = i * tam;
        if (disco_ler(d, lba_ent + byte / 512, 1, e) < 0) break;
        const uint8_t *x = e + byte % 512;
        bool vazia = true;
        for (int k = 0; k < 16; k++) if (x[k]) vazia = false;
        if (vazia) continue;
        if (le32(x + 36) || le32(x + 44)) continue;   /* más allá de 2 TB */
        uint32_t ini = le32(x + 32), fim = le32(x + 40);
        if (fim < ini) continue;
        uint64_t qtd64 = ((uint64_t)fim - ini + 1) * escala_tabla(d);   /* D265 */
        /* D270: la partición de SaguiOS de un disco instalado con GPT (D266) es como la 0xDA del MBR:
         * no se monta, se protege en el disco de arranque y su primer sector es la cabecera. */
        uint8_t tipo = memcmp(x, GPT_TIPO_SAGUI, 16) == 0 ? 0xDA : 0xEE;
        disco_t *p = criar_particao(d, (int)i + 1, tipo, escalar(d, ini), qtd64 > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)qtd64, s, buf);
        if (p) filhos[n++] = p;
    }
    kfree(e);
    return n;
}

static bool tipo_extendida(uint8_t t) { return t == 0x05 || t == 0x0F || t == 0x85; }

/* Particiones lógicas de una extendida (D120): cadena de EBR. El primer EBR está al inicio de la
 * extendida; en cada uno, la entrada 0 es la lógica (su inicio es relativo a ese EBR) y la 1 el
 * enlace al siguiente EBR (relativo al inicio de la extendida). Defensas: máximo MAX_LOGICAS, el
 * enlace tiene que avanzar siempre (sin bucles), todo dentro de la extendida y del disco; una
 * lógica incoherente se ignora con un aviso y la cadena rota termina ahí. Devuelve cuántas dio de alta. */
static int ler_logicas(disco_t *d, disco_t *ext, disco_t **filhos, int n, uint8_t *s, uint8_t *buf)
{
    uint8_t *e = kmalloc(512);
    if (!e) return 0;
    uint32_t ini = ext->inicio, fim = ini + ext->setores;   /* ext->setores ya recortado al disco */
    uint32_t rel = 0;
    int dadas = 0;
    for (int k = 0; k < MAX_LOGICAS + 1; k++) {
        if (k == MAX_LOGICAS) { klog("[DISCO] %s: más de %d particiones lógicas, el resto se ignora\n", d->nome, MAX_LOGICAS); break; }
        uint32_t ebr = ini + rel;
        if (ebr < ini || ebr >= fim || ebr >= d->setores || disco_ler(d, ebr, 1, e) < 0 || e[510] != 0x55 || e[511] != 0xAA) {
            klog("[DISCO] %s: cadena de EBR rota en el sector %u, se ignora el resto\n", d->nome, ebr);
            break;
        }
        const uint8_t *l = e + 446, *enlace = e + 446 + 16;
        uint32_t lini = ebr + escalar(d, le32(l + 8)), lqtd = escalar(d, le32(l + 12));
        if (l[4] && !tipo_extendida(l[4])) {
            if (!lqtd || lini < ebr || lini >= fim || lqtd > fim - lini) {
                klog("[DISCO] %s: partición lógica incoherente (inicio %u, %u sectores), se ignora\n", d->nome, lini, lqtd);
            } else {
                disco_t *f = criar_particao(d, MAX_PRIMARIAS + 1 + dadas, l[4], lini, lqtd, s, buf);
                if (f) { filhos[n + dadas++] = f; }
            }
        }
        if (!enlace[4] || !tipo_extendida(enlace[4])) break;   /* fin de la cadena */
        uint32_t sig = escalar(d, le32(enlace + 8));
        if (sig <= rel) { klog("[DISCO] %s: bucle en la cadena de EBR, se ignora el resto\n", d->nome); break; }
        rel = sig;
    }
    kfree(e);
    return dadas;
}

/* Descubre el formato del disco entero y crea las particiones. */
static void reconhecer(disco_t *d)
{
    uint8_t *s = kmalloc(512), *buf = kmalloc(1024);
    if (!s || !buf) { if (s) kfree(s); if (buf) kfree(buf); return; }
    txt_copia_n(d->formato, "desconocido: formato no soportado", sizeof d->formato);
    if (disco_ler(d, 0, 1, s) < 0) { txt_copia_n(d->formato, "error de lectura", sizeof d->formato); goto fim; }
    bool tem_tabela = s[510] == 0x55 && s[511] == 0xAA && !eh_fat32(s) && !exfat_reconhecer(s);
    if (!tem_tabela && reconhecer_volume(d, s, buf)) {   /* sin tabla de particiones */
        d->volume = d;
        goto fim;
    }
    if (tem_tabela) {
        disco_t *filhos[MAX_PARTICOES];
        int n = 0;
        uint8_t tipos[4]; uint32_t inis[4], qtds[4]; bool ativas[4];
        for (int p = 0; p < 4; p++) {
            const uint8_t *e = s + 446 + p * 16;
            tipos[p] = e[4]; inis[p] = escalar(d, le32(e + 8)); qtds[p] = escalar(d, le32(e + 12));
            ativas[p] = e[4] && (e[0] == 0 || e[0] == 0x80);
        }
        d->gpt = ativas[0] && tipos[0] == 0xEE;   /* D179: una GPT no se toca */
        if (d->gpt) n = ler_gpt(d, filhos, s, buf);
        else
            for (int p = 0; p < 4; p++)
                if (ativas[p]) {
                    disco_t *f = criar_particao(d, p + 1, tipos[p], inis[p], qtds[p], s, buf);
                    if (!f) continue;
                    filhos[n++] = f;
                    if (tipo_extendida(tipos[p])) {   /* D120: las lógicas, numeradas desde 5 */
                        int l = ler_logicas(d, f, filhos, n, s, buf);
                        n += l;
                        f->tipo_fs[0] = 0;
                        snformatar(f->formato, sizeof f->formato, l == 1 ? "extendida (1 lógica)" : "extendida (%d lógicas)", l);
                    }
                }
        /* El volumen del disco entero: la primera partición que se puede montar
         * (FAT32 o ext; la del SaguiOS no cuenta, error 18). Sin ninguna, el
         * formato mostrado es el de la primera partición. */
        bool tem_saguios = false;
        for (int k = 0; k < n; k++) if (filhos[k]->tipo_mbr == 0xDA) tem_saguios = true;
        /* D181: la partición de juegos (SAGUI-JOGOS) no es el volumen del disco: el disco de
         * arranque sigue siendo «disco de arranque de SaguiOS» y "montar usb0" no la lleva a /usb.
         * D194: lo mismo con la de aplicaciones (SAGUI-APPS), que va en /apps. */
        for (int k = 0; k < n && !d->volume; k++)
            if (filhos[k]->tipo_fs[0] && txt_compara(filhos[k]->rotulo, "SAGUI-JOGOS") != 0 &&
                txt_compara(filhos[k]->rotulo, "SAGUI-APPS") != 0 && txt_compara(filhos[k]->rotulo, "SAGUI-CONF") != 0 &&
                txt_compara(filhos[k]->rotulo, "SAGUI-EFI") != 0 && txt_compara(filhos[k]->rotulo, "SAGUI-ESP") != 0) d->volume = filhos[k];   /* D264, D266: la ESP tampoco */
        /* Disco de arranque: tiene la partición del SaguiOS y ninguna FAT32 (una FAT32
         * lo convierte también en un pendrive de datos, error 18). Una partición ext de
         * datos en el disco de arranque (D040) no cambia eso, ni una FAT32 con etiqueta SAGUI-*
         * (DATOS, TMP, LOGS: las de las imágenes de "make imagem-particoes", D120). */
        bool tem_fat = false;
        for (int k = 0; k < n; k++)
            if (txt_compara(filhos[k]->tipo_fs, "fat32") == 0 && !txt_comeca_com(filhos[k]->rotulo, "SAGUI-")) tem_fat = true;
        d->de_boot = tem_saguios && !tem_fat;
        if (d->volume) {
            txt_copia_n(d->formato, d->volume->formato, sizeof d->formato);
            txt_copia_n(d->rotulo, d->volume->rotulo, sizeof d->rotulo);
            txt_copia_n(d->tipo_fs, d->volume->tipo_fs, sizeof d->tipo_fs);
        } else if (tem_saguios) {
            txt_copia_n(d->formato, d->removivel ? "pendrive de arranque de SaguiOS" : "disco de arranque de SaguiOS",
                        sizeof d->formato);
        } else if (n) {
            txt_copia_n(d->formato, filhos[0]->formato, sizeof d->formato);
        } else if (ativas[0] && tipos[0] == 0xEE) {
            txt_copia_n(d->formato, "GPT sin particiones utilizables", sizeof d->formato);
        }
    }
fim:
    kfree(s);
    kfree(buf);
}

/* D179: ¿es este el disco del que arrancó la máquina? Su LBA 1 tiene que ser la
 * cabecera que dejó el MBR en 0x7E00 (copiada al empezar el kernel). Se comparan los
 * 256 primeros bytes: llevan las sumas del kernel y del ramdisk y el identificador
 * que el instalador pone en cada disco instalado (CAB_ID_INSTALACAO), así una copia
 * instalada no se confunde con el pendrive del que salió. Desde un CD, el único disco
 * que puede serlo es la imagen El Torito de dentro del CD ("cd0img", D207): la BIOS
 * arrancó de ella y su LBA 1 es la cabecera que leyó la etapa 2. */
static void detectar_arranque(disco_t *d)
{
    d->arranque_real = false;
    d->cab_lba = 0;
    if (((info_boot.opcoes & OPC_BOOT_CD) && !d->imagem_cd) || d->setores < 2) return;
    uint8_t *s = kmalloc(512);
    if (!s) return;
    if (disco_ler(d, 1, 1, s) == 0 && memcmp(s, cabecalho_boot, 256) == 0) {
        d->arranque_real = true;
        d->cab_lba = 1;
        klog("[DISCO] %s: es el disco de arranque (cabecera igual a la del arranque)\n", d->nome);
    } else if (d->gpt) {
        /* D270: un disco instalado con GPT (D266) lleva la cabecera en el primer sector de la
         * partición de SaguiOS (el LBA 1 es la cabecera de la GPT); el cargador UEFI la lee de ahí. */
        for (int i = 0; i < MAX_DISCOS && !d->arranque_real; i++) {
            disco_t *p = &discos[i];
            if (!p->usado || p->pai != d || p->tipo_mbr != 0xDA || p->inicio < 2) continue;
            if (disco_ler(d, p->inicio, 1, s) == 0 && memcmp(s, cabecalho_boot, 256) == 0) {
                d->arranque_real = true;
                d->cab_lba = p->inicio;
                klog("[DISCO] %s: es el disco de arranque (GPT: cabecera en %s, sector %u)\n", d->nome, p->nome, p->inicio);
            }
        }
    }
    kfree(s);
}

disco_t *disco_arranque_real(void)
{
    disco_t *achado = NULL;
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = &discos[i];
        if (!d->usado || !d->presente || d->pai || !d->arranque_real) continue;
        if (achado) return NULL;   /* dos copias idénticas: no se sabe cuál es */
        achado = d;
    }
    return achado;
}

/* Hay algún disco que es (o puede ser) el de arranque de verdad. */
static bool ha_arranque_real(void)
{
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].presente && !discos[i].pai && discos[i].arranque_real) return true;
    return false;
}

/* D179: ¿puede ser el disco del que arrancó? Si se sabe cuál es, solo ese; si no
 * (CD, o dos copias idénticas), cualquiera con la partición de SaguiOS. */
bool disco_pode_ser_arranque(disco_t *d)
{
    d = disco_raiz(d);
    return d->arranque_real || (!ha_arranque_real() && d->de_boot);
}

bool disco_reservado(disco_t *d)
{
    d = disco_raiz(d);
    if (!d->reservado_pid) return false;
    processo_t *p = processo_por_pid(d->reservado_pid);
    if (!p || p->estado == P_ZUMBI) {   /* el dueño terminó sin soltarlo */
        klog("[DISCO] %s: la reserva del pid %d caduca (ya no existe)\n", d->nome, d->reservado_pid);
        d->reservado_pid = 0;
        d->reservado_flags = 0;
        return false;
    }
    return true;
}

/* D213: ¿esta partición no se puede tocar con una reserva DBLOCO_CONF_MONTADO? */
bool disco_particao_protegida(disco_t *p)
{
    if (!p->usado || !p->pai) return false;
    if (p->montagem) return true;
    return p->tipo_mbr == 0xDA && disco_pode_ser_arranque(p->pai);
}

disco_t *disco_toca_protegida(disco_t *d, uint32_t lba, uint32_t n)
{
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *p = &discos[i];
        if (p->pai != d || !disco_particao_protegida(p)) continue;
        if (lba < p->inicio + p->setores && p->inicio < lba + n) return p;
    }
    return NULL;
}

/* D338: los sectores que leyó la última comprobación que salió bien (el 0 y los EBR de la cadena,
 * también el que la cortó). Lo que dice la tabla depende solo de ellos: una escritura que no toca
 * ninguno deja la tabla igual, y no hace falta volver a leerla (en un pendrive, sin caché, eran
 * cuatro o cinco órdenes USB de un sector por cada escritura de datos al mover particiones). Se
 * olvida al reservar, al soltar, al releer la tabla, cuando una escritura en bruto toca alguno o
 * falla (disco_tabela_esquecer). */
static struct {
    disco_t *d;
    uint32_t n;
    uint32_t set[MAX_LOGICAS + 2];
} cadeia;

void disco_tabela_esquecer(disco_t *d, uint32_t lba, uint32_t n)
{
    if (!cadeia.d || (d && cadeia.d != d)) return;
    if (n) {
        bool toca = false;
        for (uint32_t i = 0; i < cadeia.n && !toca; i++) toca = cadeia.set[i] >= lba && cadeia.set[i] - lba < n;
        if (!toca) return;
    }
    cadeia.d = NULL;
}

/* Lee un sector del disco como quedaría tras la escritura [lba, lba+n) con el contenido k. */
static int ler_simulado(disco_t *d, uint32_t setor, uint32_t lba, uint32_t n, const uint8_t *k, uint8_t *s)
{
    if (cadeia.n < MAX_LOGICAS + 2) cadeia.set[cadeia.n++] = setor;
    if (setor >= lba && setor - lba < n) { memcpy(s, k + (setor - lba) * 512, 512); return 0; }
    return disco_ler(d, setor, 1, s);
}

/* D213: la tabla que quedaría (con las mismas reglas que reconhecer y ler_logicas) tiene que
 * describir cada partición protegida exactamente igual. Se usa en cada escritura de una reserva
 * con particiones montadas: si un programa (con un error, o a medias) fuese a cambiar o quitar de
 * la tabla una partición montada o la de SaguiOS, el kernel no deja escribir ese sector. */
bool disco_tabela_conserva(disco_t *d, uint32_t lba, uint32_t n, const uint8_t *k)
{
    struct { uint32_t ini, qtd; uint8_t tipo; } v[MAX_PARTICOES];
    int nv = 0;
    if (cadeia.d == d) {   /* D338: ¿toca la escritura algún sector de la cadena comprobada? */
        bool toca = false;
        for (uint32_t i = 0; i < cadeia.n && !toca; i++) toca = cadeia.set[i] >= lba && cadeia.set[i] - lba < n;
        if (!toca) return true;
    }
    cadeia.d = NULL;
    cadeia.n = 0;
    uint8_t *s = kmalloc(512);
    if (!s) return false;
    bool ok = true, erro = false;
    if (ler_simulado(d, 0, lba, n, k, s) < 0) { kfree(s); return false; }
    uint32_t ext_ini = 0, ext_qtd = 0;
    if (s[510] == 0x55 && s[511] == 0xAA)
        for (int p = 0; p < 4; p++) {
            const uint8_t *e = s + 446 + p * 16;
            if (!e[4] || (e[0] != 0 && e[0] != 0x80)) continue;
            uint32_t ini = escalar(d, le32(e + 8)), qtd = escalar(d, le32(e + 12));
            if (!qtd || ini >= d->setores) continue;
            if (qtd > d->setores - ini) qtd = d->setores - ini;
            if (tipo_extendida(e[4])) { if (!ext_qtd) { ext_ini = ini; ext_qtd = qtd; } continue; }
            v[nv].ini = ini; v[nv].qtd = qtd; v[nv].tipo = e[4]; nv++;
        }
    if (ext_qtd) {   /* la cadena de EBR, como ler_logicas */
        uint32_t fim = ext_ini + ext_qtd, rel = 0;
        for (int c = 0; c < MAX_LOGICAS && nv < MAX_PARTICOES; c++) {
            uint32_t ebr = ext_ini + rel;
            if (ebr < ext_ini || ebr >= fim) break;
            if (ler_simulado(d, ebr, lba, n, k, s) < 0) { erro = true; break; }
            if (s[510] != 0x55 || s[511] != 0xAA) break;
            const uint8_t *l = s + 446, *en = s + 462;
            uint32_t lini = ebr + escalar(d, le32(l + 8)), lqtd = escalar(d, le32(l + 12));
            if (l[4] && !tipo_extendida(l[4]) && lqtd && lini >= ebr && lini < fim && lqtd <= fim - lini) {
                v[nv].ini = lini; v[nv].qtd = lqtd; v[nv].tipo = l[4]; nv++;
            }
            if (!en[4] || !tipo_extendida(en[4])) break;
            uint32_t sig = escalar(d, le32(en + 8));
            if (sig <= rel) break;
            rel = sig;
        }
    }
    for (int i = 0; i < MAX_DISCOS && ok; i++) {
        disco_t *p = &discos[i];
        if (p->pai != d || !disco_particao_protegida(p)) continue;
        bool achada = false;
        for (int j = 0; j < nv && !achada; j++)
            achada = v[j].ini == p->inicio && v[j].qtd == p->setores && v[j].tipo == p->tipo_mbr;
        if (!achada) {
            klog("[BLOCOS] %s: la escritura en el sector %u cambiaría o quitaría %s (%s), no se escribe\n",
                 d->nome, lba, p->nome, p->montagem ? "montada" : "SaguiOS");
            ok = false;
        }
    }
    kfree(s);
    /* Solo se recuerda una cadena entera y bien leída (con sitio para todos sus sectores). */
    if (ok && !erro && cadeia.n < MAX_LOGICAS + 2) cadeia.d = d;
    return ok;
}

/* D179: la tabla cambió (particiones, formateo, instalador). Las particiones viejas
 * desaparecen y se vuelve a reconocer todo, como al conectar el disco. Quien llama
 * garantiza que no hay nada montado (la reserva lo exige), salvo con DBLOCO_CONF_MONTADO
 * (D213): las montadas siguen igual en la tabla (el kernel no deja escribir otra cosa) y
 * conservan su registro (criar_particao). */
void disco_reler(disco_t *d)
{
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].pai == d && !discos[i].montagem) discos[i].usado = discos[i].presente = false;
    disco_tabela_esquecer(d, 0, 0);   /* D338 */
    d->volume = NULL;
    d->de_boot = false;
    d->gpt = false;
    d->formato[0] = d->rotulo[0] = d->tipo_fs[0] = 0;
    reconhecer(d);
    detectar_arranque(d);
    klog("[DISCO] %s: tabla releida, %s\n", d->nome, d->formato);
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].pai == d)
            klog("[DISCO]   %s: inicio %u, %u setores, tipo %x, %s \"%s\"\n", discos[i].nome, discos[i].inicio,
                 discos[i].setores, discos[i].tipo_mbr, discos[i].formato, discos[i].rotulo);
}

/* D207: lo que hay en un CD: ISO 9660 (el disco entero es el volumen) o nada que montar.
 * D233: también UDF. Con los dos (UDF bridge, lo normal en un DVD de datos) se prefiere el
 * ISO 9660 con nombres largos (Rock Ridge o Joliet), que es el que se lee desde siempre; si
 * el ISO solo tiene nombres 8.3, el UDF (nombres largos). Un CD de audio no se monta. */
void disco_reconhecer_cd(disco_t *d)
{
    d->volume = NULL;
    d->formato[0] = d->rotulo[0] = d->tipo_fs[0] = 0;
    d->tem_iso = d->tem_udf = false;
    if (d->cd_audio) { snformatar(d->formato, sizeof d->formato, "CD de audio: las pistas en %saudio", d->nome); return; }
    if (!d->setores) { txt_copia_n(d->formato, "sin disco", sizeof d->formato); return; }
    char rot_iso[sizeof d->rotulo], fmt_iso[sizeof d->formato], rot_udf[sizeof d->rotulo], fmt_udf[sizeof d->formato];
    rot_iso[0] = fmt_iso[0] = rot_udf[0] = fmt_udf[0] = 0;
    d->tem_iso = iso_reconhecer(d, rot_iso, sizeof rot_iso, fmt_iso, sizeof fmt_iso);
    d->tem_udf = udf_reconhecer(d, rot_udf, sizeof rot_udf, fmt_udf, sizeof fmt_udf);
    bool iso_curto = d->tem_iso && txt_compara(fmt_iso, "ISO 9660") == 0;   /* sin Rock Ridge ni Joliet */
    if (d->tem_udf && (!d->tem_iso || iso_curto)) {
        txt_copia_n(d->tipo_fs, "udf", sizeof d->tipo_fs);
        txt_copia_n(d->rotulo, rot_udf, sizeof d->rotulo);
        if (d->tem_iso) snformatar(d->formato, sizeof d->formato, "%s + ISO 9660", fmt_udf);
        else txt_copia_n(d->formato, fmt_udf, sizeof d->formato);
    } else if (d->tem_iso) {
        txt_copia_n(d->tipo_fs, "iso9660", sizeof d->tipo_fs);
        txt_copia_n(d->rotulo, rot_iso, sizeof d->rotulo);
        if (d->tem_udf) snformatar(d->formato, sizeof d->formato, "%s + UDF", fmt_iso);
        else txt_copia_n(d->formato, fmt_iso, sizeof d->formato);
    } else {
        /* fmt_udf trae el motivo si hay un UDF que no se sabe leer (VAT, metadatos). */
        txt_copia_n(d->formato, fmt_udf[0] ? fmt_udf : "CD sin ISO 9660 ni UDF: no se monta", sizeof d->formato);
        return;
    }
    d->volume = d;
}

static void registrar_log(disco_t *d)
{
    char tam[24];
    tamanho_legivel(d->setores, tam, sizeof tam);
    klog("[DISCO] %s: %s %s, %s, %s, rotulo \"%s\"\n", d->nome, d->fabricante, d->produto, tam, d->formato, d->rotulo);
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].pai == d)
            klog("[DISCO]   %s: inicio %u, %u setores, %s \"%s\"\n", discos[i].nome, discos[i].inicio,
                 discos[i].setores, discos[i].formato, discos[i].rotulo);
}

/* D207: el lector de CD/DVD. Interno (no es un pendrive: sin avisos) y sin escritura. */
disco_t *disco_registrar_cd(const char *fabricante, const char *produto, uint32_t setores, void *dono,
                            int (*ler)(disco_t *, uint32_t, uint32_t, void *))
{
    disco_t *d = novo_registro();
    if (!d) return NULL;
    d->presente = true;
    snformatar(d->nome, sizeof d->nome, "cd%d", contador_cd++);
    txt_copia_n(d->fabricante, fabricante, sizeof d->fabricante);
    txt_copia_n(d->produto, produto, sizeof d->produto);
    d->setores = setores;
    d->tam_setor = 512;          /* la vista de atapi.c (el lector lee de 2048 en 2048) */
    d->dono = dono;
    d->ler = ler;
    d->interno = true;
    d->cd = true;
    disco_reconhecer_cd(d);
    registrar_log(d);
    return d;
}

/* D233: las pistas de audio del CD como un volumen "cdda" (cdda.c). Sin lectura de sectores
 * de 512 (ler devuelve error: el acceso en bruto no tiene nada que leer aquí). */
static int sem_setores(disco_t *d, uint32_t lba, uint32_t n, void *buf) { (void)d; (void)lba; (void)n; (void)buf; return -1; }

disco_t *disco_registrar_audio_cd(disco_t *cd, uint32_t setores, void *dono, struct cd_toc *toc,
                                  int (*ler_audio)(disco_t *, uint32_t, uint32_t, void *),
                                  const char *rotulo, const char *formato)
{
    disco_t *d = novo_registro();
    if (!d) return NULL;
    d->presente = true;
    snformatar(d->nome, sizeof d->nome, "%saudio", cd->nome);
    txt_copia_n(d->fabricante, cd->fabricante, sizeof d->fabricante);
    txt_copia_n(d->produto, "pistas de audio", sizeof d->produto);
    d->setores = setores;
    d->tam_setor = 512;
    d->dono = dono;
    d->ler = sem_setores;
    d->interno = true;
    d->audio_cd = true;
    d->toc = toc;
    d->ler_audio = ler_audio;
    txt_copia_n(d->tipo_fs, "cdda", sizeof d->tipo_fs);
    txt_copia_n(d->rotulo, rotulo, sizeof d->rotulo);
    txt_copia_n(d->formato, formato, sizeof d->formato);
    d->volume = d;
    registrar_log(d);
    return d;
}

/* D207: la imagen El Torito (emulación de disco duro) de dentro del CD: un disco entero
 * con su MBR, la cabecera de SaguiOS y la partición 0xDA, como el de un pendrive. Si se
 * arrancó de ese CD, es el disco de arranque de verdad (de él copia el instalador). */
disco_t *disco_registrar_imagem_cd(disco_t *cd, uint32_t setores, void *dono,
                                   int (*ler)(disco_t *, uint32_t, uint32_t, void *))
{
    disco_t *d = novo_registro();
    if (!d) return NULL;
    d->presente = true;
    snformatar(d->nome, sizeof d->nome, "%simg", cd->nome);
    txt_copia_n(d->fabricante, cd->fabricante, sizeof d->fabricante);
    txt_copia_n(d->produto, "imagen de arranque El Torito", sizeof d->produto);
    d->setores = setores;
    d->tam_setor = 512;
    d->dono = dono;
    d->ler = ler;
    d->interno = true;
    d->imagem_cd = true;
    reconhecer(d);
    detectar_arranque(d);
    registrar_log(d);
    return d;
}

static disco_t *registrar_comum(const char *prefixo, const char *fabricante, const char *produto, uint32_t setores,
                                uint32_t tam_setor, void *dono, bool removivel, uint16_t bloco_fisico,
                                int (*ler)(disco_t *, uint32_t, uint32_t, void *),
                                int (*escrever)(disco_t *, uint32_t, uint32_t, const void *))
{
    disco_t *d = novo_registro();
    if (!d) return NULL;
    d->presente = true;
    d->nvme = bloco_fisico != 0;        /* D265: solo el NVMe pasa su bloque real */
    d->bloco_fisico = bloco_fisico;
    d->ram = txt_compara(prefixo, "ram") == 0;   /* D379: el disco en RAM del toram (ram0) */
    bool rz = txt_compara(prefixo, "rz") == 0;  /* v8.5.4.2: la raíz comprimida del CD (disco_comprimido.c): sin caché */
    if (rz) { d->ram = true; snformatar(d->nome, sizeof d->nome, "rz0"); }
    else if (d->ram) snformatar(d->nome, sizeof d->nome, "ram%d", contador_ram++);
    else if (prefixo[0] == 'n') snformatar(d->nome, sizeof d->nome, "%s%d", prefixo, contador_nvme++);   /* D265: nvme0, nvme1... */
    else if (removivel) snformatar(d->nome, sizeof d->nome, "%s%d", prefixo, contador_usb++);
    else snformatar(d->nome, sizeof d->nome, "%s%c", prefixo, 'a' + contador_ata++);
    txt_copia_n(d->fabricante, fabricante, sizeof d->fabricante);
    txt_copia_n(d->produto, produto, sizeof d->produto);
    d->setores = setores;
    d->tam_setor = tam_setor;
    d->dono = dono;
    d->ler = ler;
    d->escrever = escrever;
    d->removivel = removivel;
    d->interno = !removivel;
    reconhecer(d);
    detectar_arranque(d);
    char tam[24];
    tamanho_legivel(setores, tam, sizeof tam);
    klog("[DISCO] %s: %s %s, %s, %s, rotulo \"%s\"\n", d->nome, fabricante, produto, tam, d->formato, d->rotulo);
    for (int i = 0; i < MAX_DISCOS; i++)
        if (discos[i].usado && discos[i].pai == d)
            klog("[DISCO]   %s: inicio %u, %u setores, %s \"%s\"\n", discos[i].nome, discos[i].inicio,
                 discos[i].setores, discos[i].formato, discos[i].rotulo);
    if (removivel && !d->de_boot) {   /* el pendrive desde el que arrancó el sistema: nada que avisar */
        if (d->volume) disco_avisar(AVISO_CONECTADO, "Pendrive conectado: %s (%s)", d->rotulo[0] ? d->rotulo : d->produto, tam);
        else disco_avisar(AVISO_CONECTADO, "Pendrive conectado: %s (%s), %s", d->produto, tam, d->formato);
    }
    fstab_aplicar(d);   /* después del arranque: lo que el /etc/fstab manda montar de este disco */
    return d;
}

disco_t *disco_registrar(const char *prefixo, const char *fabricante, const char *produto, uint32_t setores,
                         uint32_t tam_setor, void *dono, bool removivel,
                         int (*ler)(disco_t *, uint32_t, uint32_t, void *),
                         int (*escrever)(disco_t *, uint32_t, uint32_t, const void *))
{
    return registrar_comum(prefixo, fabricante, produto, setores, tam_setor, dono, removivel, 0, ler, escrever);
}

/* D265: un namespace NVMe ("nvme0"...), visto en sectores de 512 aunque su bloque real sea de 4096. */
disco_t *disco_registrar_nvme(const char *produto, uint32_t setores, uint16_t bloco_fisico, void *dono,
                              int (*ler)(disco_t *, uint32_t, uint32_t, void *),
                              int (*escrever)(disco_t *, uint32_t, uint32_t, const void *))
{
    return registrar_comum("nvme", "NVMe", produto, setores, 512, dono, false, bloco_fisico, ler, escrever);
}

void disco_removido(disco_t *d)
{
    bool perdeu = false;
    cache_disco_descartar(d);   /* D281: lo pendiente de un disco que ya no está no se puede grabar */
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *x = &discos[i];
        if (!x->usado || (x != d && x->pai != d)) continue;
        if (x->montagem) { montagem_desmontar(x->montagem, false); perdeu = true; }   /* ya salió: no se puede grabar */
    }
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *x = &discos[i];
        if (x->usado && x->pai == d) x->usado = x->presente = false;
    }
    d->presente = false;
    if (perdeu && d->removivel)
        disco_avisar(AVISO_REMOVIDO, "Se ha retirado el pendrive sin desmontarlo: pueden haberse perdido datos.");
    else if (d->removivel && !d->de_boot)
        disco_avisar(AVISO_REMOVIDO, "Pendrive retirado.");
    klog("[DISCO] %s removido\n", d->nome);
    d->usado = false;
}

/* Forma antigua (H32): "montar usb0" monta el primer volumen en /usb. */
int disco_montar(disco_t *d)
{
    if (!d->presente) return ERRO_NAO_EXISTE;
    disco_t *v = d->pai ? (d->tipo_fs[0] ? d : NULL) : d->volume;
    if (!v) return ERRO_FORMATO;
    if (v->montagem) return 0;
    return montagem_montar(v, "/usb", NULL, 0);
}

/* Desmonta todo lo que sea de este disco (o sólo esta partición). */
int disco_desmontar(disco_t *d)
{
    int r = 0;
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *x = &discos[i];
        if (!x->usado || !x->montagem || (x != d && x->pai != d)) continue;
        int k = montagem_desmontar(x->montagem, true);
        if (k < 0 && !r) r = k;
    }
    return r;
}

/* ------------------------------------------------------------ llamadas */

static void preencher_info(disco_t *d, info_disco_t *inf)
{
    memset(inf, 0, sizeof *inf);
    txt_copia_n(inf->nome, d->nome, sizeof inf->nome);
    txt_copia_n(inf->fabricante, d->fabricante, sizeof inf->fabricante);
    txt_copia_n(inf->produto, d->produto, sizeof inf->produto);
    txt_copia_n(inf->formato, d->formato, sizeof inf->formato);
    txt_copia_n(inf->rotulo, d->rotulo, sizeof inf->rotulo);
    txt_copia_n(inf->tipo_fs, d->tipo_fs, sizeof inf->tipo_fs);
    if (d->pai) txt_copia_n(inf->pai, d->pai->nome, sizeof inf->pai);
    inf->setores = d->setores;
    inf->particao = (unsigned)d->particao;
    inf->montado = disco_montado(d);
    inf->suportado = d->pai ? d->tipo_fs[0] != 0 : d->volume != NULL;
    disco_t *raiz = disco_raiz(d);
    inf->flags = (d->removivel ? DISCO_REMOVIVEL : 0) | (d->interno ? DISCO_INTERNO : 0) | (raiz->de_boot ? DISCO_BOOT : 0) |
                 (disco_pode_ser_arranque(raiz) ? DISCO_ARRANQUE_REAL : 0) | (raiz->trim == TRIM_SIM ? DISCO_TRIM : 0) |
                 (disco_reservado(raiz) ? DISCO_RESERVADO : 0) | ((raiz->cd || raiz->imagem_cd || raiz->audio_cd) ? DISCO_CD : 0) |
                 ((uint32_t)raiz->usb << 8) |
                 (raiz->nvme ? DISCO_NVME : 0) | (raiz->nvme && raiz->bloco_fisico == 4096 ? DISCO_BLOCO4K : 0);
    /* El punto: el del propio volumen o, en el disco entero, el del volumen por defecto
     * o el de cualquier partición montada. */
    montagem_t *m = d->montagem;
    if (!m && !d->pai && d->volume) m = d->volume->montagem;
    for (int i = 0; i < MAX_DISCOS && !m && !d->pai; i++)
        if (discos[i].usado && discos[i].pai == d && discos[i].montagem) m = discos[i].montagem;
    if (m) {
        txt_copia_n(inf->ponto, m->ponto, sizeof inf->ponto);
        if (m->opcoes & MONTAR_SO_LEITURA) inf->flags |= DISCO_SO_LEITURA;
    }
}

/* Orden: cada disco entero seguido de sus particiones. */
int32_t sis_listar_discos(uint32_t indice, uint32_t pinfo, uint32_t c, uint32_t dd, uint32_t e)
{
    (void)c; (void)dd; (void)e;
    info_disco_t *inf = (info_disco_t *)pinfo;
    if (!usuario_ok(inf, sizeof *inf, true)) return ERRO_ENDERECO;
    uint32_t k = 0;
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = &discos[i];
        if (!d->usado || !d->presente || d->pai) continue;
        if (k++ == indice) { preencher_info(d, inf); return 1; }
        for (int num = 1; num <= 128; num++)
            for (int j = 0; j < MAX_DISCOS; j++) {
                disco_t *p = &discos[j];
                if (!p->usado || p->pai != d || p->particao != num) continue;
                if (k++ == indice) { preencher_info(p, inf); return 1; }
            }
    }
    return 0;
}

/* montar(nombre, punto, opciones, dueño). punto 0: la forma antigua, en /usb.
 * D282: con MONTAR_DONO, "dd" es uid | gid << 16; con MONTAR_COM_UMASK, la máscara
 * va en las opciones. */
int32_t sis_montar(uint32_t pnome, uint32_t pponto, uint32_t opcoes, uint32_t dd, uint32_t e)
{
    (void)e;
    char nome[40];
    int r = usuario_texto((const char *)pnome, nome, sizeof nome);
    if (r < 0) return r;
    disco_t *d = disco_resolver(nome);
    if (!pponto) return d ? disco_montar(d) : ERRO_NAO_EXISTE;
    char bruto[CAMINHO_MAX], ponto[CAMINHO_MAX];
    r = usuario_texto((const char *)pponto, bruto, sizeof bruto);
    if (r < 0) return r;
    if (r == 0 || !caminho_resolver(processo_recursos()->pasta, bruto, ponto, sizeof ponto)) return ERRO_INVALIDO;
    if (!d) return ERRO_NAO_EXISTE;
    /* D207: un CD puede haberse cambiado desde la última vez: se vuelve a mirar antes de montarlo. */
    if (d->sondar && !disco_montado(d)) {
        trava_pegar(&trava_fs);
        d->sondar(d);
        trava_soltar(&trava_fs);
    }
    disco_t *v = d->pai || d->volume == d ? d : d->volume;
    if (!v || !v->tipo_fs[0]) return ERRO_FORMATO;
    static const char *const tipos[] = { "auto", "fat32", "ext2", "ext3", "ext4", "iso9660", "udf", "exfat" };
    uint32_t t = MONTAR_TIPO(opcoes);
    if (t > 7) return ERRO_INVALIDO;
    return montagem_montar_dono(v, ponto, tipos[t],
                                opcoes & (MONTAR_SO_LEITURA | MONTAR_LEITURA_ESCRITA | MONTAR_DONO | MONTAR_COM_UMASK | 0777u << 16),
                                dd);
}

/* desmontar(nombre del disco o carpeta montada). */
int32_t sis_desmontar(uint32_t pnome, uint32_t b, uint32_t c, uint32_t dd, uint32_t e)
{
    (void)b; (void)c; (void)dd; (void)e;
    char nome[CAMINHO_MAX];
    int r = usuario_texto((const char *)pnome, nome, sizeof nome);
    if (r < 0) return r;
    /* D281: "sync" (nombre "*") vacía la caché de bloques de todos los discos; "?cache" anota sus cifras. */
    if (txt_compara(nome, "*") == 0) return disco_sincronizar(NULL);
    if (txt_compara(nome, "?cache") == 0) { cache_disco_registar(); return 0; }
    /* D336: lo anterior es inofensivo y vale para cualquiera; desmontar de verdad sigue siendo de root
     * (la tabla de permisos.c deja pasar la llamada y el nombre se mira aqui, ya copiado al kernel). */
    {
        processo_t *eu = processo_recursos();
        if (eu->usuario && !permisos_root(eu)) {
            permisos_anotar_negacao("llamada", SIS_DESMONTAR, 0);
            return ERRO_ACESSO;
        }
    }
    disco_t *d = nome[0] != '/' && nome[0] != '.' ? disco_resolver(nome) : NULL;
    if (d) return disco_desmontar(d);
    char ponto[CAMINHO_MAX];
    if (!caminho_resolver(processo_recursos()->pasta, nome, ponto, sizeof ponto)) return ERRO_INVALIDO;
    for (int i = 0; i < MAX_MONTAGENS; i++) {
        montagem_t *m = montagem_indice(i);
        if (m && txt_compara(m->ponto, ponto) == 0) return montagem_desmontar(m, true);
    }
    return ERRO_NAO_EXISTE;
}

/* Un dispositivo por el nombre ("usb0p1"), "LABEL=etiqueta" o "arranque" (la
 * primera partición ext o FAT32 de un disco que tiene la partición del SaguiOS,
 * D040; la FAT32 sólo con la etiqueta SAGUI-DATOS, la de las imágenes "persistente-fat32"; nunca
 * SAGUI-TMP, SAGUI-LOGS, SAGUI-JOGOS ni SAGUI-APPS, que tienen su propia carpeta, D120, D181 y D194:
 * otra FAT32 en el mismo pendrive es un pendrive de datos común, en /usb, H34). */
static bool irmao_de_boot(disco_t *d)
{
    /* D179: si se sabe de qué disco arrancó la máquina, solo ese vale (un disco con
     * SaguiOS instalado no le quita el /home al pendrive del que se arrancó, ni al revés). */
    if (d->pai && ha_arranque_real()) return d->pai->arranque_real;
    for (int j = 0; j < MAX_DISCOS; j++)
        if (discos[j].usado && d->pai && discos[j].pai == d->pai && discos[j].tipo_mbr == 0xDA) return true;
    return false;
}

disco_t *disco_resolver(const char *nome)
{
    return disco_resolver_n(nome, 0);
}

/* v8.5.4.2 (D379): el n-ésimo volumen con esa etiqueta (n = 0 es el de disco_resolver). raiz.c los
 * recorre todos para quedarse con la SAGUI-RAIZ de esta instalación aunque haya otra que también valga
 * (dos discos SaguiOS y no se sabe de cuál se arrancó). Con un nombre o "arranque", solo n = 0. */
disco_t *disco_resolver_n(const char *nome, int n)
{
    if (txt_comeca_com(nome, "LABEL=")) {
        for (int i = 0; i < MAX_DISCOS; i++) {
            disco_t *d = &discos[i];
            if (d->usado && d->presente && d->tipo_fs[0] && (d->pai || d->volume == d) &&
                txt_compara(d->rotulo, nome + 6) == 0) {
                /* D120: SAGUI-DATOS/TMP/LOGS sólo valen en el disco que tiene la partición del
                 * SaguiOS: un pendrive cualquiera con esa etiqueta no se monta en /tmp ni /logs. */
                if (txt_comeca_com(d->rotulo, "SAGUI-") && !irmao_de_boot(d)) continue;
                if (n-- == 0) return d;
            }
        }
        return NULL;
    }
    if (n) return NULL;
    if (txt_compara(nome, "arranque") == 0) {
        for (int i = 0; i < MAX_DISCOS; i++) {
            disco_t *d = &discos[i];
            /* D179: ninguna SAGUI-* salvo SAGUI-DATOS es el /home (TMP, LOGS, JOGOS de D181, APPS...). */
            if (!d->usado || !d->presente || !d->pai ||
                (txt_comeca_com(d->rotulo, "SAGUI-") && txt_compara(d->rotulo, "SAGUI-DATOS") != 0) || (!txt_comeca_com(d->tipo_fs, "ext") &&
                 (txt_compara(d->tipo_fs, "fat32") != 0 || txt_compara(d->rotulo, "SAGUI-DATOS") != 0))) continue;
            if (irmao_de_boot(d)) return d;
        }
        return NULL;
    }
    return disco_por_nome(nome);
}
