// SPDX-License-Identifier: GPL-2.0-or-later
/* formatacao.c - formatear FAT32 y ext2, y hacer crecer un FAT32 (D179).
 *
 * FAT32 según la especificación de Microsoft (fatgen103): 32 sectores reservados,
 * dos FAT, raíz en el clúster 2, FSInfo en el sector 1 y copia del arranque en el 6;
 * el tamaño del clúster sale de la tabla de la especificación y el de la FAT de
 * su fórmula. ext2 "revisión 1" como el mke2fs de Linux con lo mínimo: inodos de
 * 128 bytes, superbloque disperso (sparse_super), tipo en las entradas de carpeta
 * (filetype) y archivos grandes (large_file); las tres las escribe el ext2.c del
 * kernel. La raíz y lost+found (e2fsck la pide).
 *
 * Orden de escritura: primero se ponen a cero los metadatos y lo que diga cómo era
 * el volumen anterior (sectores 0-3: un FAT32 viejo tapaba el ext2 nuevo), y el
 * superbloque o el sector de arranque van al final: un formateo cortado a medias no
 * parece un volumen válido. */
#include "particoes.h"

static void po16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void erro_txt(char *erro, int max, const char *fmt, ...)
{
    if (!erro || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(erro, (size_t)max, fmt, ap);
    va_end(ap);
}

static void avisar(fs_progresso_t p, void *ctx, int pc) { if (p) p(pc, ctx); }

static uint32_t aleatorio32(void)
{
    uint32_t v = 0;
    if (bytes_aleatorios(&v, 4) != 4) v = tempo_ms() * 2654435761u;
    return v;
}

/* Pone a cero n sectores avisando del progreso entre pc0 y pc1. */
static int zerar_com_progresso(const char *vol, uint32_t lba, uint32_t n, fs_progresso_t p, void *ctx, int pc0, int pc1)
{
    uint32_t feito = 0, passo = 2048;
    while (feito < n) {
        uint32_t q = n - feito > passo ? passo : n - feito;
        int r = blocos_zerar(vol, lba + feito, q);
        if (r < 0) return r;
        feito += q;
        avisar(p, ctx, pc0 + (int)((uint32_t)(pc1 - pc0) * (feito >> 8) / ((n >> 8) + 1)));
    }
    return 0;
}

/* ------------------------------------------------------------ FAT32 */

static void rotulo_fat(const char *r, char *s)
{
    memset(s, ' ', 11);
    for (int i = 0; r && r[i] && i < 11; i++) {
        char c = r[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) c = '_';
        s[i] = c;
    }
}

int fs_formatar_fat32(const char *vol, const char *rotulo, fs_progresso_t prog, void *ctx, char *erro, int max)
{
    info_bloco_t inf;
    int r = blocos_info(vol, &inf);
    if (r < 0) { erro_txt(erro, max, "%s: %s", vol, nome_erro(r)); return r; }
    uint32_t tot = inf.setores;
    if (tot < 67584) { erro_txt(erro, max, "FAT32 necesita al menos 33 MB (%s tiene %u MB)", vol, tot / 2048); return ERRO_INVALIDO; }
    uint32_t spc = tot <= 532480 ? 1 : tot <= 16777216 ? 8 : tot <= 33554432 ? 16 : tot <= 67108864 ? 32 : 64;
    const uint32_t rsvd = 32, nfats = 2;
    uint32_t fatsz = 0, clusters = 0;
    for (;;) {
        uint32_t t1 = tot - rsvd, t2 = (256 * spc + nfats) / 2;
        fatsz = (t1 + t2 - 1) / t2;
        clusters = (tot - rsvd - nfats * fatsz) / spc;
        if (clusters >= 65525 || spc == 1) break;
        spc /= 2;
    }
    if (clusters < 65525) { erro_txt(erro, max, "%s es demasiado pequeña para FAT32", vol); return ERRO_INVALIDO; }
    /* D213: la FAT, con sitio para que el volumen crezca sin mover nada (el doble; desde
     * 256 MB, el cuádruple), si con eso siguen saliendo clústeres de FAT32. */
    {
        uint64_t margem = tot >= 524288 ? 4 : 2;
        uint32_t fat_m = (uint32_t)(((uint64_t)tot * margem / spc + 2) * 4 / 512 + 1);
        if (fat_m > fatsz && tot > rsvd + nfats * fat_m) {
            uint32_t cl = (tot - rsvd - nfats * fat_m) / spc;
            if (cl >= 65525) { fatsz = fat_m; clusters = cl; }
        }
    }
    if (clusters > fatsz * 128 - 2) clusters = fatsz * 128 - 2;

    avisar(prog, ctx, 0);
    if (inf.trim == TRIM_SIM) blocos_descartar(vol, 1, tot - 1);   /* SSD: todo lo de antes, fuera */
    /* Los reservados (y con ellos cualquier superbloque ext viejo en el sector 2). */
    r = blocos_zerar(vol, 0, rsvd);
    if (r < 0) goto falha;
    for (uint32_t f = 0; f < nfats; f++) {
        r = zerar_com_progresso(vol, rsvd + f * fatsz, fatsz, prog, ctx, 2 + (int)f * 45, 47 + (int)f * 45);
        if (r < 0) goto falha;
    }
    uint32_t dados = rsvd + nfats * fatsz;
    r = blocos_zerar(vol, dados, spc);   /* la raíz */
    if (r < 0) goto falha;

    uint8_t s[512];
    char lab[11];
    rotulo_fat(rotulo, lab);
    /* Primer sector de cada FAT: medio, fin de cadena y la raíz (clúster 2) ocupada. */
    memset(s, 0, 512);
    po32(s, 0x0FFFFFF8); po32(s + 4, 0x0FFFFFFF); po32(s + 8, 0x0FFFFFFF);
    for (uint32_t f = 0; f < nfats; f++) if ((r = blocos_escrever(vol, rsvd + f * fatsz, 1, s)) < 0) goto falha;
    /* La etiqueta como entrada de la raíz (Windows la lee de aquí); sin etiqueta, "NO NAME". */
    if (rotulo && rotulo[0]) {
        memset(s, 0, 512);
        memcpy(s, lab, 11);
        s[11] = 0x08;
        if ((r = blocos_escrever(vol, dados, 1, s)) < 0) goto falha;
    } else memcpy(lab, "NO NAME    ", 11);
    /* FSInfo (1 y su copia en 7). */
    memset(s, 0, 512);
    po32(s, 0x41615252); po32(s + 484, 0x61417272);
    po32(s + 488, clusters - 1); po32(s + 492, 3);
    po32(s + 508, 0xAA550000);
    if ((r = blocos_escrever(vol, 1, 1, s)) < 0 || (r = blocos_escrever(vol, 7, 1, s)) < 0) goto falha;
    /* El sector de arranque (6 = copia; el 0, el último de todo). */
    memset(s, 0, 512);
    s[0] = 0xEB; s[1] = 0x58; s[2] = 0x90;
    memcpy(s + 3, "SAGUIOS ", 8);
    po16(s + 11, 512); s[13] = (uint8_t)spc; po16(s + 14, rsvd); s[16] = (uint8_t)nfats;
    s[21] = 0xF8; po16(s + 24, 63); po16(s + 26, 255);
    po32(s + 28, inf.inicio); po32(s + 32, tot);
    po32(s + 36, fatsz); po32(s + 44, 2); po16(s + 48, 1); po16(s + 50, 6);
    s[64] = 0x80; s[66] = 0x29; po32(s + 67, aleatorio32());
    memcpy(s + 71, lab, 11);
    memcpy(s + 82, "FAT32   ", 8);
    /* Código: "int 18h" (la BIOS prueba el siguiente disco) y parar. */
    s[90] = 0xCD; s[91] = 0x18; s[92] = 0xF4; s[93] = 0xEB; s[94] = 0xFD;
    s[510] = 0x55; s[511] = 0xAA;
    if ((r = blocos_escrever(vol, 6, 1, s)) < 0 || (r = blocos_escrever(vol, 0, 1, s)) < 0) goto falha;
    avisar(prog, ctx, 100);
    registrar("formatar: %s FAT32 \"%s\", %u sectores, %u por cluster, FAT de %u, %u clusters", vol, rotulo ? rotulo : "",
              tot, spc, fatsz, clusters);
    return 0;
falha:
    erro_txt(erro, max, "error al escribir en %s: %s (el volumen queda sin formato válido)", vol, nome_erro(r));
    return r;
}

/* Hacer crecer un FAT32 hasta el tamaño de su partición, si su FAT da para ello (D213: las
 * de las imágenes y las que formatea SaguiOS traen la FAT con sitio de sobra). No se amplía la
 * FAT moviéndola hacia los sectores reservados: con 32 reservados darían como mucho 12 sectores
 * más por FAT (1.536 clústeres), y un corte a medias dejaría las dos FAT estropeadas. */
int fs_crescer_fat32(const char *vol, char *msg, int max)
{
    info_bloco_t inf;
    uint8_t s[512];
    int r = blocos_info(vol, &inf);
    if (r < 0 || (r = blocos_ler(vol, 0, 1, s)) < 0) { erro_txt(msg, max, "%s: %s", vol, nome_erro(r)); return r; }
    if (s[510] != 0x55 || s[511] != 0xAA || memcmp(s + 82, "FAT32   ", 8) != 0) { erro_txt(msg, max, "%s no es FAT32", vol); return ERRO_FORMATO; }
    uint32_t spc = s[13], rsvd = le16(s + 14), nf = s[16], tot = le32(s + 32), fatsz = le32(s + 36);
    if (!spc || !nf || !fatsz) { erro_txt(msg, max, "%s: FAT32 con datos incoherentes", vol); return ERRO_FORMATO; }
    uint32_t dados = rsvd + nf * fatsz;
    if (inf.setores <= tot) { erro_txt(msg, max, "el sistema de archivos ya ocupa toda la partición"); return 0; }
    uint32_t velhos = (tot - dados) / spc, novos = (inf.setores - dados) / spc, maximo = fatsz * 128 - 2;
    if (novos > maximo) novos = maximo;
    if (novos <= velhos) {
        erro_txt(msg, max, "la FAT de %s no tiene sitio para más clústeres: el sistema de archivos se queda en %u MB", vol, tot / 2048);
        return 0;
    }
    /* Las entradas nuevas de la FAT tienen que estar libres (a cero). */
    uint8_t *f = aloca(64 * 512);
    if (!f) return ERRO_SEM_MEMORIA;
    uint32_t prim = (velhos + 2) * 4 / 512, ult = (novos + 2 - 1) * 4 / 512;
    for (uint32_t sec = prim; sec <= ult; sec += 64) {
        uint32_t q = ult - sec + 1 > 64 ? 64 : ult - sec + 1;
        if ((r = blocos_ler(vol, rsvd + sec, q, f)) < 0) { libera(f); return r; }
        for (uint32_t i = 0; i < q * 128; i++) {
            uint32_t ent = (sec * 128) + i;
            if (ent >= velhos + 2 && ent < novos + 2 && (le32(f + i * 4) & 0x0FFFFFFF)) {
                libera(f);
                erro_txt(msg, max, "la FAT de %s tiene basura después del final: no se toca", vol);
                return ERRO_FORMATO;
            }
        }
    }
    libera(f);
    uint32_t novo_tot = novos == maximo ? dados + novos * spc : inf.setores;
    po32(s + 32, novo_tot);
    if ((r = blocos_escrever(vol, 6, 1, s)) < 0 || (r = blocos_escrever(vol, 0, 1, s)) < 0) return r;
    uint32_t fsi = le16(s + 48);
    if (fsi && fsi < rsvd && blocos_ler(vol, fsi, 1, s) == 0 && le32(s) == 0x41615252 && le32(s + 488) != 0xFFFFFFFFu) {
        po32(s + 488, le32(s + 488) + (novos - velhos));
        blocos_escrever(vol, fsi, 1, s);
        if (fsi + 6 < rsvd) blocos_escrever(vol, fsi + 6, 1, s);
    }
    erro_txt(msg, max, "el FAT32 de %s ha crecido de %u a %u MB%s", vol, tot / 2048, novo_tot / 2048,
             novo_tot < inf.setores ? " (la FAT no da para más)" : "");
    registrar("formatar: %s FAT32 crece de %u a %u sectores", vol, tot, novo_tot);
    return 1;
}

uint32_t fs_tamanho(const char *vol, char *tipo, int max_tipo)
{
    uint8_t s[512 * 4];
    if (tipo && max_tipo) tipo[0] = 0;
    if (blocos_ler(vol, 0, 4, s) < 0) return 0;
    if (s[510] == 0x55 && s[511] == 0xAA && memcmp(s + 82, "FAT32   ", 8) == 0) {
        if (tipo) copia_texto(tipo, "FAT32", (size_t)max_tipo);
        return le32(s + 32);
    }
    const uint8_t *sb = s + 1024;
    if (sb[56] == 0x53 && sb[57] == 0xEF) {
        if (tipo) copia_texto(tipo, "ext", (size_t)max_tipo);
        return le32(sb + 4) * ((1024u << le32(sb + 24)) / 512);
    }
    return 0;
}

/* ------------------------------------------------------------ ext2 */

static bool potencia(uint32_t g, uint32_t b) { while (g > 1 && g % b == 0) g /= b; return g == 1; }
static bool tem_super(uint32_t g) { return g == 0 || g == 1 || potencia(g, 3) || potencia(g, 5) || potencia(g, 7); }

typedef struct {
    uint32_t bs, spb, blocos, fdb, bpg, grupos, ipg, itb, gdt, inodos;
} ext_geo_t;

static uint32_t sobrecarga(const ext_geo_t *g, uint32_t grupo)
{
    return (tem_super(grupo) ? 1 + g->gdt : 0) + 2 + g->itb;
}

static bool calcular_ext(uint32_t setores, ext_geo_t *g)
{
    g->bs = setores >= 1048576 ? 4096 : 1024;   /* 512 MB o más: bloques de 4 KB */
    g->spb = g->bs / 512;
    g->blocos = setores / g->spb;
    g->fdb = g->bs == 1024 ? 1 : 0;
    g->bpg = 8 * g->bs;
    for (int volta = 0; volta < 3; volta++) {
        g->grupos = (g->blocos - g->fdb + g->bpg - 1) / g->bpg;
        uint32_t por_bloco = g->bs / 128;
        uint32_t total = g->blocos / 4;                 /* un inodo cada 4 bloques de datos, como mke2fs con disco pequeño */
        g->ipg = (total + g->grupos - 1) / g->grupos;
        if (g->ipg < 16) g->ipg = 16;
        g->ipg = (g->ipg + por_bloco - 1) / por_bloco * por_bloco;
        g->ipg = (g->ipg + 7) / 8 * 8;
        if (g->ipg > g->bpg) g->ipg = g->bpg;
        g->itb = g->ipg * 128 / g->bs;
        g->gdt = (g->grupos * 32 + g->bs - 1) / g->bs;
        uint32_t ult = g->grupos - 1, ini = g->fdb + ult * g->bpg, tam = g->blocos - ini;
        if (g->grupos > 1 && tam < sobrecarga(g, ult) + 50) { g->blocos = ini; continue; }   /* el último grupo no da: fuera */
        break;
    }
    g->inodos = g->ipg * g->grupos;
    return g->grupos >= 1 && g->blocos - g->fdb >= sobrecarga(g, 0) + 16;
}

static int escrever_bloco(const char *vol, const ext_geo_t *g, uint32_t b, const void *buf)
{
    return blocos_escrever(vol, b * g->spb, g->spb, buf);
}

static void bit(uint8_t *m, uint32_t i) { m[i / 8] |= (uint8_t)(1u << (i % 8)); }

static void inodo_pasta(uint8_t *in, uint32_t modo, uint32_t links, uint32_t bloco, uint32_t bs, uint32_t agora)
{
    memset(in, 0, 128);
    po16(in, modo);
    po32(in + 4, bs);
    po32(in + 8, agora); po32(in + 12, agora); po32(in + 16, agora);
    po16(in + 26, links);
    po32(in + 28, bs / 512);
    po32(in + 40, bloco);
}

static void entrada_dir(uint8_t *e, uint32_t inodo, uint32_t rec, const char *nome)
{
    uint32_t n = (uint32_t)tam_texto(nome);
    po32(e, inodo); po16(e + 4, rec); e[6] = (uint8_t)n; e[7] = 2;   /* 2 = carpeta */
    memcpy(e + 8, nome, n);
}

int fs_formatar_ext2(const char *vol, const char *rotulo, fs_progresso_t prog, void *ctx, char *erro, int max)
{
    info_bloco_t inf;
    int r = blocos_info(vol, &inf);
    if (r < 0) { erro_txt(erro, max, "%s: %s", vol, nome_erro(r)); return r; }
    ext_geo_t g;
    if (inf.setores < 4096 || !calcular_ext(inf.setores, &g)) { erro_txt(erro, max, "%s es demasiado pequeña para ext2 (mínimo 2 MB)", vol); return ERRO_INVALIDO; }
    uint32_t agora = hora_utc(NULL);
    uint8_t *b = aloca(g.bs), *gdt = aloca_zerado(g.gdt * g.bs);
    if (!b || !gdt) { if (b) libera(b); if (gdt) libera(gdt); return ERRO_SEM_MEMORIA; }
    avisar(prog, ctx, 0);
    if (inf.trim == TRIM_SIM) blocos_descartar(vol, 4, inf.setores - 4);

    /* Lo que delataba al volumen anterior: los primeros 4 KB. */
    if ((r = blocos_zerar(vol, 0, 8)) < 0) goto falha;

    uint32_t raiz_blk = 0, lf_blk = 0, livres_tot = 0, inodos_livres_tot = 0;
    for (uint32_t gr = 0; gr < g.grupos; gr++) {
        uint32_t ini = g.fdb + gr * g.bpg, tam = g.blocos - ini > g.bpg ? g.bpg : g.blocos - ini;
        uint32_t meta = tem_super(gr) ? 1 + g.gdt : 0;
        uint32_t bb = ini + meta, ib = bb + 1, it = ib + 1, usados = sobrecarga(&g, gr);
        if (gr == 0) { raiz_blk = it + g.itb; lf_blk = raiz_blk + 1; usados += 2; }
        /* Tabla de inodos a cero. */
        if ((r = zerar_com_progresso(vol, it * g.spb, g.itb * g.spb, NULL, NULL, 0, 0)) < 0) goto falha;
        /* Mapa de bloques: la sobrecarga (y raíz y lost+found), y el relleno tras el final del grupo. */
        memset(b, 0, g.bs);
        for (uint32_t i = 0; i < usados; i++) bit(b, i);
        for (uint32_t i = tam; i < g.bs * 8; i++) bit(b, i);
        if ((r = escrever_bloco(vol, &g, bb, b)) < 0) goto falha;
        /* Mapa de inodos: 1..11 en el grupo 0; relleno tras ipg. */
        memset(b, 0, g.bs);
        if (gr == 0) for (uint32_t i = 0; i < 11; i++) bit(b, i);
        for (uint32_t i = g.ipg; i < g.bs * 8; i++) bit(b, i);
        if ((r = escrever_bloco(vol, &g, ib, b)) < 0) goto falha;
        uint32_t livres = tam - usados, ilivres = g.ipg - (gr == 0 ? 11 : 0);
        uint8_t *d = gdt + gr * 32;
        po32(d, bb); po32(d + 4, ib); po32(d + 8, it);
        po16(d + 12, livres); po16(d + 14, ilivres); po16(d + 16, gr == 0 ? 2 : 0);
        livres_tot += livres;
        inodos_livres_tot += ilivres;
        avisar(prog, ctx, 5 + (int)(80u * (gr + 1) / g.grupos));
    }
    /* Raíz (inodo 2) y lost+found (inodo 11): los dos en el primer bloque de la tabla del grupo 0. */
    uint32_t it0 = g.fdb + 1 + g.gdt + 2;
    memset(b, 0, g.bs);
    inodo_pasta(b + 1 * 128, 040755, 3, raiz_blk, g.bs, agora);
    if ((r = escrever_bloco(vol, &g, it0, b)) < 0) goto falha;
    memset(b, 0, g.bs);
    uint32_t bloco_lf = (11 - 1) * 128 / g.bs, desl_lf = (11 - 1) * 128 % g.bs;
    if (bloco_lf == 0) inodo_pasta(b + 1 * 128, 040755, 3, raiz_blk, g.bs, agora);
    inodo_pasta(b + desl_lf, 040700, 2, lf_blk, g.bs, agora);
    if ((r = escrever_bloco(vol, &g, it0 + bloco_lf, b)) < 0) goto falha;
    memset(b, 0, g.bs);
    entrada_dir(b, 2, 12, ".");
    entrada_dir(b + 12, 2, 12, "..");
    entrada_dir(b + 24, 11, g.bs - 24, "lost+found");
    if ((r = escrever_bloco(vol, &g, raiz_blk, b)) < 0) goto falha;
    memset(b, 0, g.bs);
    entrada_dir(b, 11, 12, ".");
    entrada_dir(b + 12, 2, g.bs - 12, "..");
    if ((r = escrever_bloco(vol, &g, lf_blk, b)) < 0) goto falha;

    /* Superbloque (y sus copias con la tabla de grupos en los grupos con copia). */
    uint8_t sb[1024];
    memset(sb, 0, sizeof sb);
    po32(sb + 0, g.inodos); po32(sb + 4, g.blocos); po32(sb + 8, 0);
    po32(sb + 12, livres_tot); po32(sb + 16, inodos_livres_tot);
    po32(sb + 20, g.fdb); po32(sb + 24, g.bs == 1024 ? 0 : 2); po32(sb + 28, g.bs == 1024 ? 0 : 2);
    po32(sb + 32, g.bpg); po32(sb + 36, g.bpg); po32(sb + 40, g.ipg);
    po32(sb + 44, 0); po32(sb + 48, agora);
    po16(sb + 52, 0); po16(sb + 54, 0xFFFF);
    po16(sb + 56, 0xEF53); po16(sb + 58, 1); po16(sb + 60, 1); po16(sb + 62, 0);
    po32(sb + 64, agora); po32(sb + 68, 0); po32(sb + 72, 0); po32(sb + 76, 1);
    po32(sb + 84, 11); po16(sb + 88, 128);
    po32(sb + 92, 0); po32(sb + 96, 0x0002); po32(sb + 100, 0x0001 | 0x0002);
    for (int i = 0; i < 4; i++) po32(sb + 104 + i * 4, aleatorio32());
    sb[104 + 6] = (uint8_t)((sb[104 + 6] & 0x0F) | 0x40);   /* UUID versión 4 */
    sb[104 + 8] = (uint8_t)((sb[104 + 8] & 0x3F) | 0x80);
    for (int i = 0; rotulo && rotulo[i] && i < 16; i++) sb[120 + i] = (uint8_t)rotulo[i];
    po32(sb + 264, agora);   /* s_mkfs_time */
    for (uint32_t gr = g.grupos; gr-- > 0; ) {   /* la del grupo 0 la última */
        if (!tem_super(gr)) continue;
        uint32_t ini = g.fdb + gr * g.bpg;
        po16(sb + 90, gr);
        if ((r = blocos_escrever(vol, (ini + 1) * g.spb, g.gdt * g.spb, gdt)) < 0) goto falha;
        if (gr == 0 && g.bs == 1024) r = blocos_escrever(vol, 2, 2, sb);
        else if (gr == 0) { memset(b, 0, g.bs); memcpy(b + 1024, sb, 1024); r = escrever_bloco(vol, &g, 0, b); }
        else { memset(b, 0, g.bs); memcpy(b, sb, 1024); r = escrever_bloco(vol, &g, ini, b); }
        if (r < 0) goto falha;
    }
    libera(b);
    libera(gdt);
    avisar(prog, ctx, 100);
    registrar("formatar: %s ext2 \"%s\", bloques de %u, %u bloques, %u grupos, %u inodos", vol, rotulo ? rotulo : "",
              g.bs, g.blocos, g.grupos, g.inodos);
    return 0;
falha:
    libera(b);
    libera(gdt);
    erro_txt(erro, max, "error al escribir en %s: %s (el volumen queda sin formato válido)", vol, nome_erro(r));
    return r;
}
