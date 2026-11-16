// SPDX-License-Identifier: GPL-2.0-or-later
/* exfat.c - el sistema de archivos exFAT de los pendrives y tarjetas grandes (D325).
 *
 * Implementación propia, hecha con la especificación pública de exFAT de Microsoft
 * (2019); no se copió código de Linux ni de otro sistema. Estructura de un volumen:
 *   región de arranque (sectores 0-11: arranque y sectores extendidos, con una suma de
 *     verificación en el 11; sectores 12-23 son su copia);
 *   FAT: entradas de 32 bits por clúster (0 = libre, 0xFFFFFFFF = fin de cadena). Los
 *     archivos pueden ir *sin cadena* (NoFatChain): clústeres seguidos que la FAT no menciona;
 *   montón de clústeres, desde el 2: la raíz (una cadena en la FAT), el mapa de bits de
 *     asignación (un bit por clúster, LSB primero), la tabla de mayúsculas y los datos.
 * Una carpeta es una lista de entradas de 32 bytes. Cada archivo es un grupo: File (0x85) +
 * Stream Extension (0xC0, con el primer clúster, tamaños y NameHash) + File Name (0xC1, 15
 * caracteres UTF-16 cada una), y el File lleva la suma de verificación de todo el grupo.
 *
 * Los nombres se pasan a Latin-1 (la codificación del sistema): lo que no cabe sale como '_',
 * igual que en FAT32 (fat32.c). Las comparaciones no distinguen mayúsculas, con la tabla de
 * mayúsculas del propio volumen. Límites: el VFS cuenta posiciones en 32 bits, así que un
 * archivo de 4 GB o más se puede LEER hasta 4 GB - 1 pero no escribir (se rechaza al abrir
 * para escribir y nada se corrompe), y los nombres de más de NOME_MAX - 1 caracteres salen
 * recortados. Solo se escribe con una FAT (TexFAT no) y con el mapa de bits en memoria. */
#include "exfat.h"
#include "montagem.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "fuso.h"
#include "caminho.h"
#include "texto.h"
#include "trava.h"
#include "processo.h"
#include "cpu.h"

#define EOC          0xFFFFFFFFu
#define T_BITMAP     0x81
#define T_MAIUSC     0x82
#define T_ROTULO     0x83
#define T_ARQ        0x85
#define T_FLUXO      0xC0
#define T_NOME       0xC1
#define A_SOMENTE    0x01
#define A_OCULTO     0x02
#define A_SISTEMA    0x04
#define A_PASTA      0x10
#define A_ARQ        0x20
#define SEM_ENTRADA  0xFFFFFFFFu
#define GRUPO_MAX    32            /* entradas de un grupo que se leen de una vez (File + 31) */
#define BITMAP_MAX   (4u * 1024 * 1024)
#define VOL_SUJO     0x0002        /* VolumeFlags: VolumeDirty */

typedef exfat_fluxo_t fluxo_t;

typedef struct {
    disco_t *disco;
    uint32_t bps_sh;               /* log2 de los bytes por sector del volumen (9-12) */
    uint32_t spc, bpc;             /* sectores de 512 y bytes por clúster */
    uint32_t inicio_fat, tam_fat;  /* en sectores de 512 */
    uint32_t inicio_dados;
    uint32_t total_clusters;
    uint32_t raiz;
    uint16_t flags;
    bool     so_leitura;           /* montado así (a petición o porque el volumen no es de fiar) */
    bool     pus_sujo;             /* este montaje puso VolumeDirty: lo quita al desmontar */
    bool     montado;
    uint32_t livres, dica;
    /* Mapa de bits en memoria; solo se graba lo que cambió ([bm_ini, bm_fim) en bytes). */
    uint8_t *bitmap;
    uint32_t bm_bytes, bm_setor;
    uint32_t bm_ini, bm_fim;
    /* Mayúsculas de la tabla del volumen (identidad donde no dice nada). */
    uint16_t *maiusc;
    bool     maiusc_ok;
    char     rotulo[24];
    uint32_t erros_soma;
    uint8_t *zeros;                /* 64 KB a cero, para poner a cero clústeres de carpetas */
    struct { uint32_t setor; bool valido, sujo; uint32_t uso; uint8_t *dados; } cache_[16];
    uint32_t relogio_cache;
    int      erro_es;
    uint8_t *ra_dados;
    uint32_t ra_setor, ra_n, ra_prox;
} volume_t;
#define JANELA_SETORES 256
#define DIRETO_SETORES 64
#define RACHA_MAX      (128u * 1024u)
#define BLOCO_ZEROS    65536u

static volume_t *vp;
#define v (*vp)
#define cache (vp->cache_)
#define relogio_cache (vp->relogio_cache)
#define erro_es (vp->erro_es)
#define CACHE 16

/* ------------------------------------------------------------ caché de sectores (como la del FAT32) */

static bool na_fat(uint32_t s) { return s >= v.inicio_fat && s < v.inicio_fat + v.tam_fat; }

static int gravar_setor_cache(int i)
{
    if (!cache[i].sujo) return 0;
    int r = disco_escrever(v.disco, cache[i].setor, 1, cache[i].dados);
    if (r < 0) { if (!erro_es) erro_es = ERRO_ACESSO; return r; }
    cache[i].sujo = false;
    return 0;
}

/* Graba el mapa de bits que cambió (sectores enteros). */
static int gravar_bitmap(void)
{
    if (!v.bitmap || v.bm_ini >= v.bm_fim) return 0;
    uint32_t s0 = v.bm_ini / 512, s1 = (v.bm_fim + 511) / 512;
    uint32_t total = (v.bm_bytes + 511) / 512;
    if (s1 > total) s1 = total;
    int r = 0;
    for (uint32_t s = s0; s < s1; ) {
        uint32_t n = s1 - s > 128 ? 128 : s1 - s;
        uint8_t *tmp = NULL;
        const uint8_t *orig = v.bitmap + s * 512;
        if (s + n == total && v.bm_bytes % 512) {   /* el último sector, completado con ceros */
            tmp = kcalloc(n * 512);
            if (!tmp) return ERRO_SEM_MEMORIA;
            memcpy(tmp, orig, v.bm_bytes - s * 512);
            orig = tmp;
        }
        int w = disco_escrever(v.disco, v.bm_setor + s, n, orig);
        if (tmp) kfree(tmp);
        if (w < 0) { r = ERRO_ACESSO; if (!erro_es) erro_es = ERRO_ACESSO; break; }
        s += n;
    }
    if (r == 0) { v.bm_ini = 0xFFFFFFFFu; v.bm_fim = 0; }
    return r;
}

/* Graba los sectores sucios de la caché en orden de número; los vecinos, de un golpe. "so_fat":
 * solo la FAT (1), solo lo que no es FAT (2) o todo (0). */
static int gravar_cache(int que)
{
    int ordem[CACHE], q = 0, r = 0;
    for (int i = 0; i < CACHE; i++) {
        if (!cache[i].valido || !cache[i].sujo) continue;
        bool f = na_fat(cache[i].setor);
        if ((que == 1 && !f) || (que == 2 && f)) continue;
        ordem[q++] = i;
    }
    for (int a = 1; a < q; a++) {
        int x = ordem[a], b = a;
        while (b > 0 && cache[ordem[b - 1]].setor > cache[x].setor) { ordem[b] = ordem[b - 1]; b--; }
        ordem[b] = x;
    }
    uint8_t *lote = q > 1 ? kmalloc(CACHE * 512) : NULL;
    for (int a = 0; a < q; ) {
        uint32_t s0 = cache[ordem[a]].setor;
        int b = a + 1;
        while (lote && b < q && cache[ordem[b]].setor == s0 + (uint32_t)(b - a)) b++;
        int k = b - a, ok;
        if (k == 1) ok = gravar_setor_cache(ordem[a]) == 0;
        else {
            for (int j = 0; j < k; j++) memcpy(lote + j * 512, cache[ordem[a + j]].dados, 512);
            ok = disco_escrever(v.disco, s0, (uint32_t)k, lote) == 0;
            if (ok) for (int j = 0; j < k; j++) cache[ordem[a + j]].sujo = false;
            else if (!erro_es) erro_es = ERRO_ACESSO;
        }
        if (!ok) r = ERRO_ACESSO;
        a = b;
    }
    if (lote) kfree(lote);
    return r;
}

/* Orden de grabación (para que un corte no deje el volumen peor que FAT32): si la operación
 * reservó espacio, primero la FAT y el mapa de bits y después las entradas de carpeta; si solo
 * liberó, primero las entradas y después la FAT y el mapa (un corte deja clústeres perdidos,
 * nunca una entrada apuntando a un clúster libre). */
static int descarregar(bool alocacao_primeiro)
{
    int r = 0, x;
    if (alocacao_primeiro) {
        if ((x = gravar_cache(1)) < 0) r = x;
        if ((x = gravar_bitmap()) < 0) r = x;
        if ((x = gravar_cache(2)) < 0) r = x;
    } else {
        if ((x = gravar_cache(2)) < 0) r = x;
        if ((x = gravar_cache(1)) < 0) r = x;
        if ((x = gravar_bitmap()) < 0) r = x;
    }
    return r;
}

static void esquecer_cache(void)
{
    for (int i = 0; i < CACHE; i++) cache[i].valido = cache[i].sujo = false;
    vp->ra_n = 0;
}

static uint8_t *setor(uint32_t s)
{
    int livre = -1, velho = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].valido && cache[i].setor == s) { cache[i].uso = ++relogio_cache; return cache[i].dados; }
        if (!cache[i].valido && livre < 0) livre = i;
        if (cache[i].uso < cache[velho].uso) velho = i;
    }
    int i = livre >= 0 ? livre : velho;
    if (cache[i].valido && gravar_setor_cache(i) < 0) return NULL;
    if (!cache[i].dados && !(cache[i].dados = kmalloc(512))) return NULL;
    if (disco_ler(v.disco, s, 1, cache[i].dados) < 0) { cache[i].valido = false; if (!erro_es) erro_es = ERRO_ACESSO; return NULL; }
    cache[i].setor = s; cache[i].valido = true; cache[i].sujo = false; cache[i].uso = ++relogio_cache;
    return cache[i].dados;
}

static void sujar(uint32_t s)
{
    if (s >= vp->ra_setor && s < vp->ra_setor + vp->ra_n) vp->ra_n = 0;
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].setor == s) cache[i].sujo = true;
}

static int gravar_direto(uint32_t s, uint32_t n, const uint8_t *dados)
{
    if (vp->ra_n && s < vp->ra_setor + vp->ra_n && s + n > vp->ra_setor) vp->ra_n = 0;
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].setor >= s && cache[i].setor < s + n) {
            memcpy(cache[i].dados, dados + (cache[i].setor - s) * 512, 512);
            cache[i].sujo = false;
        }
    int r = disco_escrever(v.disco, s, n, dados);
    if (r < 0 && !erro_es) erro_es = ERRO_ACESSO;
    return r;
}

static int ler_direto(uint32_t s, uint32_t n, uint8_t *dados)
{
    int r = disco_ler(v.disco, s, n, dados);
    if (r < 0) { if (!erro_es) erro_es = ERRO_ACESSO; return r; }
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].sujo && cache[i].setor >= s && cache[i].setor < s + n)
            memcpy(dados + (cache[i].setor - s) * 512, cache[i].dados, 512);
    return 0;
}

/* Lectura de datos de archivo: lo grande va directo; lo pequeño y seguido, por una ventana de 128 KB. */
static int ler_dados(uint32_t s, uint32_t n, uint8_t *dados)
{
    if (n >= DIRETO_SETORES) return ler_direto(s, n, dados);
    if (vp->ra_n && s >= vp->ra_setor && s + n <= vp->ra_setor + vp->ra_n) {
        memcpy(dados, vp->ra_dados + (s - vp->ra_setor) * 512, n * 512);
        vp->ra_prox = s + n;
        return 0;
    }
    bool sequencial = s == vp->ra_prox;
    vp->ra_prox = s + n;
    uint32_t k = JANELA_SETORES;
    if (v.disco->setores > s && v.disco->setores - s < k) k = v.disco->setores - s;
    if (!sequencial || k < n || (!vp->ra_dados && !(vp->ra_dados = kmalloc(JANELA_SETORES * 512))))
        return ler_direto(s, n, dados);
    vp->ra_n = 0;
    if (ler_direto(s, k, vp->ra_dados) < 0) return ler_direto(s, n, dados);
    vp->ra_setor = s; vp->ra_n = k;
    memcpy(dados, vp->ra_dados, n * 512);
    return 0;
}

static int ler_bytes(uint32_t s0, uint32_t d0, uint32_t parte, uint8_t *dest)
{
    if (d0) {
        uint32_t q = 512 - d0 > parte ? parte : 512 - d0;
        uint8_t *s = setor(s0);
        if (!s) return -1;
        memcpy(dest, s + d0, q);
        dest += q; parte -= q; s0++;
    }
    uint32_t n = parte / 512;
    if (n) {
        if (ler_dados(s0, n, dest) < 0) return -1;
        dest += n * 512; s0 += n; parte -= n * 512;
    }
    if (parte) {
        uint8_t *s = setor(s0);
        if (!s) return -1;
        memcpy(dest, s, parte);
    }
    return 0;
}

static int gravar_bytes(uint32_t s0, uint32_t d0, uint32_t parte, const uint8_t *orig)
{
    if (d0) {
        uint32_t q = 512 - d0 > parte ? parte : 512 - d0;
        uint8_t *s = setor(s0);
        if (!s) return -1;
        memcpy(s + d0, orig, q);
        sujar(s0);
        orig += q; parte -= q; s0++;
    }
    uint32_t n = parte / 512;
    if (n) {
        if (gravar_direto(s0, n, orig) < 0) return -1;
        orig += n * 512; s0 += n; parte -= n * 512;
    }
    if (parte) {
        uint8_t *s = setor(s0);
        if (!s) return -1;
        memcpy(s, orig, parte);
        sujar(s0);
    }
    return 0;
}

/* ------------------------------------------------------------ FAT y mapa de bits */

static bool cluster_valido(uint32_t c) { return c >= 2 && c < v.total_clusters + 2; }
static uint32_t setor_do_cluster(uint32_t c) { return v.inicio_dados + (c - 2) * v.spc; }

static uint32_t fat_ler(uint32_t c)
{
    uint8_t *s = setor(v.inicio_fat + c * 4 / 512);
    if (!s) return EOC;
    return *(uint32_t *)(s + (c * 4) % 512);
}

static void fat_gravar(uint32_t c, uint32_t valor)
{
    uint32_t sn = v.inicio_fat + c * 4 / 512;
    uint8_t *s = setor(sn);
    if (!s) return;
    *(uint32_t *)(s + (c * 4) % 512) = valor;
    sujar(sn);
}

static bool bm_usado(uint32_t c) { uint32_t i = c - 2; return v.bitmap[i >> 3] & (1u << (i & 7)); }

static void bm_sujar(uint32_t byte)
{
    if (byte < v.bm_ini) v.bm_ini = byte;
    if (byte + 1 > v.bm_fim) v.bm_fim = byte + 1;
}

static void bm_por(uint32_t c, bool usado)
{
    uint32_t i = c - 2;
    bool antes = v.bitmap[i >> 3] & (1u << (i & 7));
    if (antes == usado) return;
    if (usado) { v.bitmap[i >> 3] |= (uint8_t)(1u << (i & 7)); if (v.livres) v.livres--; }
    else       { v.bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); v.livres++; }
    bm_sujar(i >> 3);
}

/* Primer clúster libre desde "ini" (con vuelta); 0 si no hay. */
static uint32_t bm_achar_livre(uint32_t ini)
{
    uint32_t n = v.total_clusters;
    uint32_t i = cluster_valido(ini) ? ini - 2 : 0;
    for (uint32_t feitos = 0; feitos < n; ) {
        if (i >= n) i = 0;
        if ((i & 7) == 0 && v.bitmap[i >> 3] == 0xFF) { i += 8; feitos += 8; continue; }
        if (!(v.bitmap[i >> 3] & (1u << (i & 7)))) return i + 2;
        i++; feitos++;
    }
    return 0;
}

/* Reserva un clúster: lo marca en el mapa de bits (la FAT la toca quien lo enlaza). */
static uint32_t alocar_perto(uint32_t quero)
{
    if (!v.bitmap) return 0;
    uint32_t c = bm_achar_livre(quero ? quero : v.dica);
    if (!c) return 0;
    bm_por(c, true);
    v.dica = c + 1;
    return c;
}

/* ------------------------------------------------------------ flujos (archivos y carpetas) */

static uint32_t fl_proximo(const fluxo_t *fl, uint32_t c)
{
    if (fl->sem_cadeia) return cluster_valido(c + 1) ? c + 1 : 0;
    uint32_t p = fat_ler(c);
    return cluster_valido(p) ? p : 0;
}

/* El clúster número n del flujo (0 si no existe). */
static uint32_t fl_cluster(const fluxo_t *fl, uint32_t n)
{
    if (n >= fl->ncl || !cluster_valido(fl->primeiro)) return 0;
    if (fl->sem_cadeia) return fl->primeiro + n;
    uint32_t c = fl->primeiro;
    for (uint32_t i = 0; i < n; i++) { c = fl_proximo(fl, c); if (!c) return 0; }
    return c;
}

static uint32_t fl_ultimo(fluxo_t *fl)
{
    if (!fl->ncl) return 0;
    if (fl->sem_cadeia) return fl->primeiro + fl->ncl - 1;
    if (!fl->ultimo) fl->ultimo = fl_cluster(fl, fl->ncl - 1);
    return fl->ultimo;
}

/* Pasa un flujo contiguo a cadena: escribe sus enlaces en la FAT. */
static void fl_converter(fluxo_t *fl)
{
    if (!fl->sem_cadeia) return;
    for (uint32_t i = 0; i < fl->ncl; i++)
        fat_gravar(fl->primeiro + i, i + 1 < fl->ncl ? fl->primeiro + i + 1 : EOC);
    fl->sem_cadeia = false;
    fl->ultimo = fl->ncl ? fl->primeiro + fl->ncl - 1 : 0;
}

/* Libera los clústeres desde el número "de" en adelante (los datos no se tocan). */
static void fl_liberar_de(fluxo_t *fl, uint32_t de)
{
    if (de >= fl->ncl) return;
    if (fl->sem_cadeia) {
        for (uint32_t i = de; i < fl->ncl; i++) bm_por(fl->primeiro + i, false);
    } else {
        uint32_t c = fl_cluster(fl, de), ant = de ? fl_cluster(fl, de - 1) : 0;
        for (uint32_t g = de; c && g < fl->ncl; g++) {
            uint32_t prox = fat_ler(c);
            fat_gravar(c, 0);
            bm_por(c, false);
            c = cluster_valido(prox) ? prox : 0;
        }
        if (ant) fat_gravar(ant, EOC);
        fl->ultimo = ant;
    }
    fl->ncl = de;
    if (!de) { fl->primeiro = 0; fl->sem_cadeia = false; fl->ultimo = 0; }
}

/* Amplía el flujo hasta "novo" clústeres. Si falta sitio, deshace lo que reservó. */
static int fl_ampliar(fluxo_t *fl, uint32_t novo)
{
    uint32_t antes = fl->ncl;
    while (fl->ncl < novo) {
        uint32_t ult = fl_ultimo(fl);
        uint32_t quer = ult ? ult + 1 : 0;
        /* Si el siguiente está libre, se toma (sigue contiguo); si no, el primero libre. */
        uint32_t c = (quer && cluster_valido(quer) && !bm_usado(quer)) ? quer : 0;
        if (c) { bm_por(c, true); v.dica = c + 1; } else c = alocar_perto(0);
        if (!c) { fl_liberar_de(fl, antes); return ERRO_SEM_MEMORIA; }
        if (!fl->ncl) {
            fl->primeiro = c; fl->sem_cadeia = true; fl->ultimo = c;
        } else {
            if (fl->sem_cadeia && c != ult + 1) fl_converter(fl);
            if (!fl->sem_cadeia) { fat_gravar(ult, c); }
            fl->ultimo = c;
        }
        if (!fl->sem_cadeia) fat_gravar(c, EOC);
        fl->ncl++;
    }
    return 0;
}

static void zerar_cluster(uint32_t c)
{
    if (!v.zeros && !(v.zeros = kcalloc(BLOCO_ZEROS))) { if (!erro_es) erro_es = ERRO_SEM_MEMORIA; return; }
    uint32_t s = setor_do_cluster(c), n = v.spc;
    while (n) {
        uint32_t k = n > BLOCO_ZEROS / 512 ? BLOCO_ZEROS / 512 : n;
        gravar_direto(s, k, v.zeros);
        s += k; n -= k;
    }
}

/* ------------------------------------------------------------ nombres */

static uint16_t maiuscula(uint16_t c) { return v.maiusc ? v.maiusc[c] : c; }

static bool prefixo_igual(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!a[i] || maiuscula((uint8_t)a[i]) != maiuscula((uint8_t)b[i])) return false;
    return true;
}

static bool nomes_iguais(const char *a, const char *b)
{
    while (*a && *b)
        if (maiuscula((uint8_t)*a++) != maiuscula((uint8_t)*b++)) return false;
    return *a == *b;
}

/* NameHash: suma rotativa de 16 bits sobre los bytes de los caracteres en mayúsculas. */
static uint16_t hash_nome(const char *nome)
{
    uint16_t h = 0;
    for (; *nome; nome++) {
        uint16_t c = maiuscula((uint8_t)*nome);
        h = (uint16_t)(((h & 1) ? 0x8000 : 0) + (h >> 1) + (uint8_t)c);
        h = (uint16_t)(((h & 1) ? 0x8000 : 0) + (h >> 1) + (uint8_t)(c >> 8));
    }
    return h;
}

static uint16_t soma_grupo(const uint8_t *g, uint32_t n)
{
    uint16_t s = 0;
    for (uint32_t i = 0; i < n * 32; i++) {
        if (i == 2 || i == 3) continue;
        s = (uint16_t)(((s & 1) ? 0x8000 : 0) + (s >> 1) + g[i]);
    }
    return s;
}

static bool nome_valido(const char *n)
{
    size_t t = txt_tamanho(n);
    if (!t || t > 255) return false;
    if (n[t - 1] == ' ' || n[t - 1] == '.') return false;
    for (size_t i = 0; i < t; i++) {
        uint8_t c = (uint8_t)n[i];
        if (c < 0x20 || c == '"' || c == '*' || c == '/' || c == ':' || c == '<' || c == '>' || c == '?' || c == '\\' || c == '|')
            return false;
    }
    return true;
}

/* ------------------------------------------------------------ fechas */

static uint32_t ts_agora(uint8_t *desvio)
{
    fuso_data_t d;
    hora_local(&d);
    int o = d.desvio / 900;
    *desvio = (uint8_t)(0x80 | (o & 0x7F));
    return ((uint32_t)(d.ano - 1980) << 25) | ((uint32_t)d.mes << 21) | ((uint32_t)d.dia << 16) |
           ((uint32_t)d.hora << 11) | ((uint32_t)d.minuto << 5) | (d.segundo / 2);
}

static uint32_t ts_para_utc(uint32_t ts, uint8_t desvio)
{
    if (!ts) return 0;
    int ano = 1980 + (int)(ts >> 25), mes = (ts >> 21) & 15, dia = (ts >> 16) & 31;
    if (mes < 1 || mes > 12 || dia < 1) return 0;
    uint32_t local = fuso_compor(ano, mes, dia, (ts >> 11) & 31, (ts >> 5) & 63, (int)(ts & 31) * 2);
    if (desvio & 0x80) {
        int o = desvio & 0x7F;
        if (o & 0x40) o -= 128;
        return (uint32_t)((int32_t)local - o * 900);
    }
    return hora_local_para_utc(local);
}

/* D341: un instante UTC -> sello exFAT en la hora local de la zona del sistema, con su desvío (en
 * cuartos de hora, marcado con el bit 7, como Windows y Linux) y las centésimas del segundo impar.
 * Fuera del rango (1980-2107) se queda en el extremo. */
static uint32_t utc_para_ts(uint32_t utc, uint8_t *desvio, uint8_t *cs)
{
    fuso_data_t d;
    hora_local_de(utc, &d);
    int o = d.desvio / 900;
    *desvio = (uint8_t)(0x80 | (o & 0x7F));
    *cs = 0;
    if (d.ano < 1980) return (1u << 21) | (1u << 16);
    if (d.ano > 2107) return (127u << 25) | (12u << 21) | (31u << 16) | (23u << 11) | (59u << 5) | 29u;
    *cs = (uint8_t)((d.segundo & 1) ? 100 : 0);
    return ((uint32_t)(d.ano - 1980) << 25) | ((uint32_t)d.mes << 21) | ((uint32_t)d.dia << 16) |
           ((uint32_t)d.hora << 11) | ((uint32_t)d.minuto << 5) | (d.segundo / 2);
}

/* ------------------------------------------------------------ carpetas */

typedef struct {
    char     nome[NOME_MAX];
    uint16_t attr;
    fluxo_t  fl;                 /* datos */
    uint32_t vdl;
    bool     grande;             /* DataLength >= 4 GB */
    fluxo_t  pai;                /* la carpeta que lo contiene */
    uint32_t idx, qtd;           /* SEM_ENTRADA en la raíz */
    uint32_t t_cria, t_mod, t_acesso;
    uint8_t  d_cria, d_mod, d_acesso;
    uint8_t  cs_cria, cs_mod;    /* D341: centésimas (0..199) que se suman a los 2 s del sello */
} entrada_t;

typedef struct { fluxo_t *fl; uint32_t cn, cc; } cursor_t;

/* El puntero a la entrada "idx" de la carpeta (y su sector); NULL si no cabe en la carpeta. */
static uint8_t *cur_ptr(cursor_t *c, uint32_t idx, uint32_t *sn)
{
    uint32_t byte = idx * 32, n = byte / v.bpc;
    if (idx >= 0x07FFFFFFu || n >= c->fl->ncl) return NULL;
    if (!c->cc || n < c->cn) { c->cn = 0; c->cc = c->fl->primeiro; }
    while (c->cn < n) {
        c->cc = fl_proximo(c->fl, c->cc);
        if (!c->cc) return NULL;
        c->cn++;
    }
    if (!cluster_valido(c->cc)) return NULL;
    uint32_t dentro = byte % v.bpc;
    uint32_t s = setor_do_cluster(c->cc) + dentro / 512;
    uint8_t *p = setor(s);
    if (!p) return NULL;
    if (sn) *sn = s;
    return p + dentro % 512;
}

static bool grupo_ler(fluxo_t *fl, uint32_t idx, uint32_t n, uint8_t *buf)
{
    cursor_t c = { fl, 0, 0 };
    for (uint32_t k = 0; k < n; k++) {
        uint8_t *p = cur_ptr(&c, idx + k, NULL);
        if (!p) return false;
        memcpy(buf + k * 32, p, 32);
    }
    return true;
}

static bool grupo_gravar(fluxo_t *fl, uint32_t idx, uint32_t n, const uint8_t *buf)
{
    cursor_t c = { fl, 0, 0 };
    for (uint32_t k = 0; k < n; k++) {
        uint32_t sn;
        uint8_t *p = cur_ptr(&c, idx + k, &sn);
        if (!p) return false;
        memcpy(p, buf + k * 32, 32);
        sujar(sn);
    }
    return true;
}

static void raiz_fluxo(fluxo_t *fl)
{
    memset(fl, 0, sizeof *fl);
    fl->primeiro = v.raiz;
    uint32_t c = v.raiz;
    for (uint32_t g = 0; cluster_valido(c) && g < v.total_clusters; g++) {
        fl->ncl++;
        fl->ultimo = c;
        c = fl_proximo(fl, c);
        if (!c) break;
    }
    fl->tam = fl->ncl * v.bpc;
}

static void raiz_entrada(entrada_t *e)
{
    memset(e, 0, sizeof *e);
    e->attr = A_PASTA;
    raiz_fluxo(&e->fl);
    e->vdl = e->fl.tam;
    e->idx = SEM_ENTRADA;
}

static uint32_t n_clusters(uint32_t tam) { return tam / v.bpc + (tam % v.bpc ? 1 : 0); }

/* Lee el grupo que empieza en la entrada File "idx" de la carpeta "fl". Falso si no es un grupo
 * entero y bueno (suma mala, falta el flujo o los nombres). */
static bool ler_grupo(fluxo_t *pai, uint32_t idx, const uint8_t *prim, entrada_t *e)
{
    uint32_t sec = prim[1], n = 1 + sec;
    if (sec < 2 || n > GRUPO_MAX) return false;
    uint8_t g[GRUPO_MAX * 32];
    if (!grupo_ler(pai, idx, n, g)) return false;
    if (g[32] != T_FLUXO) return false;
    if (*(uint16_t *)(g + 2) != soma_grupo(g, n)) { v.erros_soma++; return false; }
    uint32_t nl = g[32 + 3];
    if (!nl || (nl + 14) / 15 > sec - 1) return false;
    memset(e, 0, sizeof *e);
    e->attr = *(uint16_t *)(g + 4);
    e->t_cria = *(uint32_t *)(g + 8);  e->t_mod = *(uint32_t *)(g + 12);  e->t_acesso = *(uint32_t *)(g + 16);
    e->d_cria = g[22]; e->d_mod = g[23]; e->d_acesso = g[24];
    e->cs_cria = g[20] < 200 ? g[20] : 0; e->cs_mod = g[21] < 200 ? g[21] : 0;
    uint8_t *s = g + 32;
    uint64_t vdl = *(uint32_t *)(s + 8) | ((uint64_t)*(uint32_t *)(s + 12) << 32);
    uint64_t tam = *(uint32_t *)(s + 24) | ((uint64_t)*(uint32_t *)(s + 28) << 32);
    e->fl.primeiro = *(uint32_t *)(s + 20);
    e->fl.sem_cadeia = (s[1] & 2) != 0;
    e->grande = tam > 0xFFFFFFFFull;
    e->fl.tam = e->grande ? 0xFFFFFFFFu : (uint32_t)tam;
    e->vdl = vdl > tam ? e->fl.tam : (vdl > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)vdl);
    e->fl.ncl = e->fl.primeiro ? n_clusters(e->fl.tam) : 0;
    if (e->attr & A_PASTA) { e->fl.ncl = e->fl.primeiro ? n_clusters(e->fl.tam) : 0; }
    int k = 0;
    for (uint32_t i = 0; i < nl; i++) {
        const uint8_t *nm = g + 64 + (i / 15) * 32;
        if (nm[0] != T_NOME) return false;
        uint16_t ch = *(const uint16_t *)(nm + 2 + (i % 15) * 2);
        if (k < NOME_MAX - 1) e->nome[k++] = ch < 0x100 ? (char)ch : '_';
    }
    e->nome[k] = 0;
    e->pai = *pai;
    e->idx = idx; e->qtd = n;
    return true;
}

/* Siguiente grupo bueno de la carpeta a partir de *idx. Falso al llegar al final. */
static bool ler_entrada(fluxo_t *pai, cursor_t *c, uint32_t *idx, entrada_t *e)
{
    for (;;) {
        uint8_t *p = cur_ptr(c, *idx, NULL);
        if (!p || p[0] == 0x00) return false;
        if (p[0] == T_ARQ) {
            uint32_t ini = *idx, n = 1 + (uint32_t)p[1];
            uint8_t prim[32];
            memcpy(prim, p, 32);
            *idx += n;
            if (ler_grupo(pai, ini, prim, e)) return true;
            continue;
        }
        (*idx)++;
    }
}

static bool achar_na_pasta(const fluxo_t *pasta, const char *nome, size_t n, entrada_t *e)
{
    char alvo[NOME_MAX];
    if (n >= NOME_MAX) n = NOME_MAX - 1;
    memcpy(alvo, nome, n);
    alvo[n] = 0;
    fluxo_t pai = *pasta;
    cursor_t c = { &pai, 0, 0 };
    uint32_t idx = 0;
    while (ler_entrada(&pai, &c, &idx, e))
        if (nomes_iguais(e->nome, alvo)) return true;
    return false;
}

static bool achar(const char *rel, entrada_t *e)
{
    raiz_entrada(e);
    const char *p = rel;
    while (*p == '/') p++;
    while (*p) {
        const char *fim = p;
        while (*fim && *fim != '/') fim++;
        if (!(e->attr & A_PASTA)) return false;
        fluxo_t dir = e->fl;
        if (!achar_na_pasta(&dir, p, (size_t)(fim - p), e)) return false;
        p = *fim ? fim + 1 : fim;
        while (*p == '/') p++;
    }
    return true;
}

static int separar(const char *rel, entrada_t *pasta, const char **nome)
{
    const char *barra = NULL;
    for (const char *p = rel; *p; p++) if (*p == '/') barra = p;
    char tmp[CAMINHO_MAX];
    if (barra) {
        size_t n = (size_t)(barra - rel);
        if (n >= sizeof tmp) return ERRO_INVALIDO;
        memcpy(tmp, rel, n);
        tmp[n] = 0;
        *nome = barra + 1;
    } else {
        tmp[0] = 0;
        *nome = rel;
    }
    if (!**nome || txt_tamanho(*nome) >= NOME_MAX) return ERRO_INVALIDO;
    if (!achar(tmp, pasta)) return ERRO_NAO_EXISTE;
    if (!(pasta->attr & A_PASTA)) return ERRO_NAO_E_PASTA;
    return 0;
}

/* ------------------------------------------------------------ escribir entradas */

/* Reescribe el grupo de "e" con su flujo, tamaño y fechas actuales (y la suma). */
static int gravar_entrada(const entrada_t *e, bool tocar_fechas)
{
    if (e->idx == SEM_ENTRADA) return 0;   /* la raíz no tiene entrada */
    uint8_t g[GRUPO_MAX * 32];
    if (e->qtd > GRUPO_MAX || !grupo_ler((fluxo_t *)&e->pai, e->idx, e->qtd, g)) return ERRO_ACESSO;
    *(uint16_t *)(g + 4) = e->attr;
    if (tocar_fechas) {
        uint8_t d;
        uint32_t t = ts_agora(&d);
        *(uint32_t *)(g + 12) = t; g[23] = d; g[21] = 0;   /* modificación */
        *(uint32_t *)(g + 16) = t; g[24] = d;              /* último acceso */
    }
    uint8_t *s = g + 32;
    s[1] = (uint8_t)(1 | (e->fl.sem_cadeia ? 2 : 0));
    *(uint32_t *)(s + 8) = e->vdl;  *(uint32_t *)(s + 12) = 0;
    *(uint32_t *)(s + 20) = e->fl.primeiro;
    *(uint32_t *)(s + 24) = e->fl.tam;  *(uint32_t *)(s + 28) = 0;
    *(uint16_t *)(g + 2) = soma_grupo(g, e->qtd);
    return grupo_gravar((fluxo_t *)&e->pai, e->idx, e->qtd, g) ? 0 : ERRO_ACESSO;
}

/* Amplía una carpeta en un clúster (a cero) y actualiza su entrada. */
static int pasta_ampliar(entrada_t *dir)
{
    if (dir->fl.ncl && !fl_ultimo(&dir->fl)) return ERRO_ACESSO;
    int r = fl_ampliar(&dir->fl, dir->fl.ncl + 1);
    if (r < 0) return r;
    zerar_cluster(dir->fl.ultimo);
    dir->fl.tam = dir->fl.ncl * v.bpc;
    dir->vdl = dir->fl.tam;
    return gravar_entrada(dir, false);
}

/* Crea un grupo en la carpeta "dir"; "e" devuelve cómo quedó. Los datos (flujo, tamaño) ya los
 * trae "e"; aquí se ponen nombre, atributos y fechas. */
static int criar_entrada(entrada_t *dir, const char *nome, uint16_t attr, entrada_t *e)
{
    if (!nome_valido(nome)) return ERRO_INVALIDO;
    uint32_t tam = (uint32_t)txt_tamanho(nome);
    uint32_t partes = (tam + 14) / 15, n = 2 + partes;
    /* Un hueco de "n" entradas libres seguidas (las de después de la marca de fin también lo son). */
    uint32_t idx = 0, corrida = 0, ini = 0;
    {
        fluxo_t pai = dir->fl;
        cursor_t c = { &pai, 0, 0 };
        bool fim = false;
        for (;;) {
            uint8_t *p = cur_ptr(&c, idx, NULL);
            if (!p) break;
            if (p[0] == 0x00) { fim = true; if (!corrida) ini = idx; break; }
            if (!(p[0] & 0x80)) { if (!corrida) ini = idx; if (++corrida == n) break; }
            else corrida = 0;
            idx++;
        }
        if (!fim && !corrida) ini = idx;
    }
    /* Que quepa: si el hueco acaba pasando del final de la carpeta, se amplía. */
    while ((ini + n) * 32 > dir->fl.ncl * v.bpc) {
        int r = pasta_ampliar(dir);
        if (r < 0) return r;
    }
    uint8_t g[GRUPO_MAX * 32];
    if (n > GRUPO_MAX) return ERRO_INVALIDO;
    memset(g, 0, n * 32);
    uint8_t d;
    uint32_t t = ts_agora(&d);
    g[0] = T_ARQ; g[1] = (uint8_t)(n - 1);
    *(uint16_t *)(g + 4) = attr;
    *(uint32_t *)(g + 8) = t; *(uint32_t *)(g + 12) = t; *(uint32_t *)(g + 16) = t;
    g[22] = d; g[23] = d; g[24] = d;
    g[32] = T_FLUXO; g[33] = (uint8_t)(1 | (e->fl.sem_cadeia ? 2 : 0));
    g[35] = (uint8_t)tam;
    *(uint16_t *)(g + 36) = hash_nome(nome);
    *(uint32_t *)(g + 40) = e->vdl;
    *(uint32_t *)(g + 52) = e->fl.primeiro;
    *(uint32_t *)(g + 56) = e->fl.tam;
    for (uint32_t i = 0; i < partes; i++) {
        uint8_t *nm = g + 64 + i * 32;
        nm[0] = T_NOME;
        for (uint32_t k = 0; k < 15 && i * 15 + k < tam; k++) nm[2 + k * 2] = (uint8_t)nome[i * 15 + k];
    }
    *(uint16_t *)(g + 2) = soma_grupo(g, n);
    if (!grupo_gravar(&dir->fl, ini, n, g)) return ERRO_ACESSO;
    txt_copia_n(e->nome, nome, sizeof e->nome);
    e->attr = attr;
    e->pai = dir->fl;
    e->idx = ini; e->qtd = n;
    e->t_cria = e->t_mod = e->t_acesso = t;
    e->d_cria = e->d_mod = e->d_acesso = d;
    return 0;
}

/* Marca el grupo como borrado (se quita el bit "en uso" de cada entrada). */
static void apagar_entrada(const entrada_t *e)
{
    if (e->idx == SEM_ENTRADA) return;
    cursor_t c = { (fluxo_t *)&e->pai, 0, 0 };
    for (uint32_t k = 0; k < e->qtd; k++) {
        uint32_t sn;
        uint8_t *p = cur_ptr(&c, e->idx + k, &sn);
        if (!p) return;
        p[0] &= 0x7F;
        sujar(sn);
    }
}

static int terminar_com(int r, bool aloc)
{
    int d = descarregar(aloc);
    if (erro_es) { r = r < 0 ? r : erro_es; erro_es = 0; }
    return r < 0 ? r : (d < 0 ? d : r);
}
static int terminar(int r) { return terminar_com(r, true); }
static int terminar_liberando(int r) { return terminar_com(r, false); }

/* ------------------------------------------------------------ montaje */

/* Reconocimiento barato por el sector 0 (también lo usa discos.c). */
bool exfat_reconhecer(const uint8_t *s)
{
    if (memcmp(s + 3, "EXFAT   ", 8) != 0 || s[510] != 0x55 || s[511] != 0xAA) return false;
    for (int i = 11; i < 64; i++) if (s[i]) return false;
    return s[108] >= 9 && s[108] <= 12 && s[109] <= 25 - s[108] && s[105] == 1;
}

/* Suma de verificación de la región de arranque: sectores 0-10 menos VolumeFlags y PercentInUse. */
static uint32_t soma_arranque(const uint8_t *b, uint32_t bytes)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < bytes; i++) {
        if (i == 106 || i == 107 || i == 112) continue;
        s = ((s & 1) ? 0x80000000u : 0) + (s >> 1) + b[i];
    }
    return s;
}

/* Lee el sector de arranque (y, si "oficial", comprueba su suma) y rellena "v". */
static int ler_volume(disco_t *d, bool oficial)
{
    uint8_t *s = kmalloc(512);
    if (!s) return ERRO_SEM_MEMORIA;
    if (disco_ler(d, 0, 1, s) < 0) { kfree(s); return ERRO_ACESSO; }
    if (!exfat_reconhecer(s)) { kfree(s); return ERRO_FORMATO; }
    v.disco = d;
    v.bps_sh = s[108];
    uint32_t sh = v.bps_sh - 9;
    uint32_t spc_sh = s[109];
    v.spc = 1u << (sh + spc_sh);
    v.bpc = v.spc * 512;
    uint32_t fat_off = *(uint32_t *)(s + 80), fat_len = *(uint32_t *)(s + 84);
    uint32_t heap_off = *(uint32_t *)(s + 88), count = *(uint32_t *)(s + 92);
    v.raiz = *(uint32_t *)(s + 96);
    v.flags = *(uint16_t *)(s + 106);
    uint64_t compr = *(uint32_t *)(s + 72) | ((uint64_t)*(uint32_t *)(s + 76) << 32);
    uint8_t n_fats = s[110];
    kfree(s);
    if (n_fats != 1 && n_fats != 2) return ERRO_FORMATO;
    if (!fat_len || !count || count > 0xFFFFFFF5u || heap_off < fat_off + fat_len) return ERRO_FORMATO;
    if (v.raiz < 2 || v.raiz >= count + 2) return ERRO_FORMATO;
    if (fat_off > 0x7FFFFFFFu >> sh || fat_len > 0x7FFFFFFFu >> sh || heap_off > 0x7FFFFFFFu >> sh) return ERRO_FORMATO;
    v.inicio_fat = fat_off << sh;
    v.tam_fat = fat_len << sh;
    v.inicio_dados = heap_off << sh;
    v.total_clusters = count;
    if ((uint64_t)v.inicio_fat + (uint64_t)n_fats * v.tam_fat > v.inicio_dados) return ERRO_FORMATO;
    if ((uint64_t)v.inicio_dados + (uint64_t)count * v.spc > d->setores) {
        klog("[EXFAT] %s: el volumen dice tener mas sectores que el disco\n", d->nome);
        return ERRO_FORMATO;
    }
    if ((uint64_t)count + 2 > ((uint64_t)v.tam_fat * 512) / 4) return ERRO_FORMATO;
    (void)compr;
    if (n_fats == 2) v.so_leitura = true;   /* TexFAT: no se escribe */
    if (oficial) {
        uint32_t bps = 1u << v.bps_sh;
        uint8_t *r = kmalloc(12 * bps);
        if (!r) return ERRO_SEM_MEMORIA;
        if (disco_ler(d, 0, 12u << sh, r) < 0) { kfree(r); return ERRO_ACESSO; }
        uint32_t soma = soma_arranque(r, 11 * bps), ok = 1;
        for (uint32_t i = 0; i < bps / 4; i++) if (*(uint32_t *)(r + 11 * bps + i * 4) != soma) ok = 0;
        kfree(r);
        if (!ok) { klog("[EXFAT] %s: la suma de la region de arranque no coincide\n", d->nome); return ERRO_FORMATO; }
    }
    v.livres = 0;
    v.dica = 2;
    esquecer_cache();
    v.montado = true;
    return 0;
}

static void liberar_volume(volume_t *x)
{
    for (int i = 0; i < CACHE; i++) if (x->cache_[i].dados) kfree(x->cache_[i].dados);
    if (x->ra_dados) kfree(x->ra_dados);
    if (x->bitmap) kfree(x->bitmap);
    if (x->maiusc) kfree(x->maiusc);
    if (x->zeros) kfree(x->zeros);
    kfree(x);
}

/* Lee "bytes" de datos contiguos que empiezan en el clúster "c" (mapa de bits y tabla de mayúsculas). */
static int ler_contiguo(uint32_t c, uint32_t bytes, uint8_t *dest)
{
    uint32_t s = setor_do_cluster(c), n = (bytes + 511) / 512, feitos = 0;
    uint8_t *tmp = NULL;
    while (feitos < n) {
        uint32_t k = n - feitos > 128 ? 128 : n - feitos;
        if ((feitos + k) * 512 > bytes) {   /* el último trozo, sin pasarse del búfer */
            if (!tmp && !(tmp = kmalloc(128 * 512))) return ERRO_SEM_MEMORIA;
            if (ler_direto(s + feitos, k, tmp) < 0) { kfree(tmp); return ERRO_ACESSO; }
            memcpy(dest + feitos * 512, tmp, bytes - feitos * 512);
        } else if (ler_direto(s + feitos, k, dest + feitos * 512) < 0) { if (tmp) kfree(tmp); return ERRO_ACESSO; }
        feitos += k;
    }
    if (tmp) kfree(tmp);
    return 0;
}

/* Recorre la raíz buscando el mapa de bits, la tabla de mayúsculas y la etiqueta. */
static int ler_raiz(bool com_estruturas)
{
    fluxo_t raiz;
    raiz_fluxo(&raiz);
    cursor_t c = { &raiz, 0, 0 };
    uint32_t bm_c = 0, bm_len = 0, mx_c = 0, mx_len = 0, mx_soma = 0;
    bool tem_bm = false, tem_mx = false;
    v.rotulo[0] = 0;
    for (uint32_t i = 0; ; i++) {
        uint8_t *p = cur_ptr(&c, i, NULL);
        if (!p || p[0] == 0x00) break;
        if (p[0] == T_ROTULO && p[1] <= 11) {
            int k = 0;
            for (int j = 0; j < p[1]; j++) {
                uint16_t ch = *(uint16_t *)(p + 2 + j * 2);
                v.rotulo[k++] = ch < 0x100 ? (char)ch : '_';
            }
            v.rotulo[k] = 0;
        } else if (p[0] == T_BITMAP && !tem_bm && !(p[1] & 1)) {
            bm_c = *(uint32_t *)(p + 20); bm_len = *(uint32_t *)(p + 24); tem_bm = true;
        } else if (p[0] == T_MAIUSC && !tem_mx) {
            mx_soma = *(uint32_t *)(p + 4); mx_c = *(uint32_t *)(p + 20); mx_len = *(uint32_t *)(p + 24); tem_mx = true;
        }
    }
    if (!com_estruturas) return 0;
    if (!tem_bm || !cluster_valido(bm_c) || bm_len < (v.total_clusters + 7) / 8 || bm_len > BITMAP_MAX) {
        klog("[EXFAT] mapa de bits ausente o demasiado grande: solo lectura y sin cuenta de libres\n");
        v.so_leitura = true;
    } else {
        v.bm_bytes = (v.total_clusters + 7) / 8;
        v.bitmap = kmalloc(v.bm_bytes);
        if (!v.bitmap) return ERRO_SEM_MEMORIA;
        v.bm_setor = setor_do_cluster(bm_c);
        if (ler_contiguo(bm_c, v.bm_bytes, v.bitmap) < 0) return ERRO_ACESSO;
        uint32_t usados = 0;
        for (uint32_t i = 0; i < v.total_clusters; i++) if (v.bitmap[i >> 3] & (1u << (i & 7))) usados++;
        v.livres = v.total_clusters - usados;
        v.bm_ini = 0xFFFFFFFFu; v.bm_fim = 0;
    }
    /* Tabla de mayúsculas: identidad, salvo lo que diga la tabla (comprimida o no). */
    v.maiusc = kmalloc(65536 * 2);
    if (!v.maiusc) return ERRO_SEM_MEMORIA;
    for (uint32_t i = 0; i < 65536; i++) v.maiusc[i] = (uint16_t)i;
    v.maiusc_ok = false;
    if (tem_mx && cluster_valido(mx_c) && mx_len >= 2 && mx_len <= 131072 && !(mx_len & 1)) {
        uint8_t *t = kmalloc(mx_len);
        if (t && ler_contiguo(mx_c, mx_len, t) == 0) {
            uint32_t soma = 0;
            for (uint32_t i = 0; i < mx_len; i++) soma = ((soma & 1) ? 0x80000000u : 0) + (soma >> 1) + t[i];
            if (soma == mx_soma) {
                const uint16_t *e = (const uint16_t *)t;
                uint32_t n = mx_len / 2, pos = 0;
                for (uint32_t i = 0; i < n && pos < 65536; i++) {
                    if (e[i] == 0xFFFF && i + 1 < n) { pos += e[i + 1]; i++; }
                    else v.maiusc[pos++] = e[i];
                }
                v.maiusc_ok = true;
            }
        }
        if (t) kfree(t);
    }
    if (!v.maiusc_ok) {
        /* Sin tabla fiable: las mayúsculas básicas de Latin-1, y sin escribir (los NameHash dependen de ella). */
        for (uint32_t i = 'a'; i <= 'z'; i++) v.maiusc[i] = (uint16_t)(i - 32);
        for (uint32_t i = 0xE0; i <= 0xFE; i++) if (i != 0xF7) v.maiusc[i] = (uint16_t)(i - 32);
        klog("[EXFAT] tabla de mayusculas ausente o con suma mala: solo lectura\n");
        v.so_leitura = true;
    }
    return 0;
}

static int gravar_flags(uint16_t flags)
{
    uint8_t *s = kmalloc(512);
    if (!s) return ERRO_SEM_MEMORIA;
    int r = disco_ler(v.disco, 0, 1, s);
    if (r == 0) {
        *(uint16_t *)(s + 106) = flags;
        r = disco_escrever(v.disco, 0, 1, s);
    }
    kfree(s);
    if (r < 0) return ERRO_ACESSO;
    v.flags = flags;
    return 0;
}

static int ex_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    volume_t *x = kcalloc(sizeof(volume_t));
    if (!x) return ERRO_SEM_MEMORIA;
    vp = x;
    int r = ler_volume(d, true);
    if (r == 0) r = ler_raiz(true);
    if (r < 0) { liberar_volume(x); vp = NULL; return r; }
    if (v.so_leitura) *opcoes |= MONTAR_SO_LEITURA;
    /* Volumen marcado sucio (no se desmontó bien): solo lectura con aviso, salvo que se pida
     * «rw» a propósito. Los discos internos ya exigen ese «rw» (D040). */
    if ((v.flags & VOL_SUJO) && !(*opcoes & MONTAR_SO_LEITURA)) {
        if (*opcoes & MONTAR_LEITURA_ESCRITA) {
            klog("[EXFAT] %s: volumen sucio, se monta para escritura a peticion\n", d->nome);
            disco_avisar(AVISO_INFO, "%s: no se desmontó bien la última vez; montado con escritura a petición. Conviene comprobarlo (fsck.exfat) en otro equipo.", d->nome);
            v.pus_sujo = false;
        } else {
            klog("[EXFAT] %s: volumen sucio, se monta solo para lectura\n", d->nome);
            disco_avisar(AVISO_INFO, "%s: no se desmontó bien la última vez; montado solo para lectura. Para escribir, mount -o rw tras comprobarlo (fsck.exfat) en otro equipo.", d->nome);
            *opcoes |= MONTAR_SO_LEITURA;
        }
    }
    if (!(*opcoes & MONTAR_SO_LEITURA)) {
        v.so_leitura = false;
        if (!(v.flags & VOL_SUJO)) {
            if (gravar_flags(v.flags | VOL_SUJO) < 0) { liberar_volume(x); vp = NULL; return ERRO_ACESSO; }
            v.pus_sujo = true;
        }
    } else v.so_leitura = true;
    klog("[EXFAT] montado: %u clusters de %u bytes, raiz no cluster %u, %u livres, etiqueta '%s'%s\n",
         v.total_clusters, v.bpc, v.raiz, v.livres, v.rotulo, v.so_leitura ? ", so leitura" : "");
    *priv = x;
    return 0;
}

static int ex_desmontar(void *priv, bool gravar)
{
    vp = priv;
    int r = 0;
    if (gravar && !v.so_leitura) {
        r = descarregar(true);
        if (v.pus_sujo && gravar_flags(v.flags & ~VOL_SUJO) < 0) r = ERRO_ACESSO;
    }
    esquecer_cache();
    v.montado = false;
    klog("[EXFAT] desmontado%s\n", gravar ? " (tudo gravado)" : " sem gravar");
    liberar_volume(vp);
    vp = NULL;
    return r;
}

bool exfat_rotulo_raiz(disco_t *d, char *rotulo)
{
    volume_t *guarda, *x = kcalloc(sizeof(volume_t));
    if (!x) return false;
    trava_pegar(&trava_fs);
    guarda = vp;
    vp = x;
    bool achou = false;
    if (ler_volume(d, false) == 0 && ler_raiz(false) == 0 && v.rotulo[0]) {
        txt_copia_n(rotulo, v.rotulo, 12);
        achou = true;
    }
    liberar_volume(x);
    vp = guarda;
    trava_soltar(&trava_fs);
    return achou;
}

/* ------------------------------------------------------------ operaciones */

#define SOLEITURA() do { if (v.so_leitura) return ERRO_SO_LEITURA; } while (0)

static uint32_t cluster_de(exfat_arq_t *f, uint32_t n)
{
    if (f->fl.sem_cadeia) return fl_cluster(&f->fl, n);
    uint32_t c;
    if (f->cache_cl && n >= f->cache_n) {
        c = f->cache_cl;
        for (uint32_t i = f->cache_n; i < n && c; i++) c = fl_proximo(&f->fl, c);
    } else c = fl_cluster(&f->fl, n);
    if (c) { f->cache_cl = c; f->cache_n = n; }
    return c;
}

static int ex_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    vp = priv;
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    inf->tipo = (e.attr & A_PASTA) ? TIPO_PASTA : TIPO_ARQUIVO;
    inf->tamanho = (e.attr & A_PASTA) ? 0 : e.fl.tam;
    return terminar(0);
}

static int ex_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    vp = priv;
    entrada_t e;
    if (!*rel) { memset(d, 0, sizeof *d); return 0; }
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    d->modificado = ts_para_utc(e.t_mod, e.d_mod);
    d->criado = ts_para_utc(e.t_cria, e.d_cria);
    if (d->modificado) d->modificado += e.cs_mod / 100u;   /* D341: el segundo impar */
    if (d->criado) d->criado += e.cs_cria / 100u;
    d->acessado = ts_para_utc(e.t_acesso, e.d_acesso);
    return terminar(0);
}

static int ex_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    vp = priv;
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (!(e.attr & A_PASTA)) return terminar(ERRO_NAO_E_PASTA);
    fluxo_t dir = e.fl;
    cursor_t c = { &dir, 0, 0 };
    uint32_t idx = 0;
    int k = 0;
    while (ler_entrada(&dir, &c, &idx, &e)) {
        if (e.attr & (A_OCULTO | A_SISTEMA)) continue;
        if (k++ != indice) continue;
        txt_copia_n(saida->nome, e.nome, NOME_MAX);
        saida->tipo = (e.attr & A_PASTA) ? TIPO_PASTA : TIPO_ARQUIVO;
        saida->tamanho = (e.attr & A_PASTA) ? 0 : e.fl.tam;
        return terminar(1);
    }
    return terminar(0);
}

static void abrir_de(exfat_arq_t *f, const entrada_t *e)
{
    memset(f, 0, sizeof *f);
    f->fl = e->fl; f->pai = e->pai; f->idx = e->idx; f->qtd = e->qtd; f->vdl = e->vdl;
}

static entrada_t entrada_do_arq(const exfat_arq_t *f, uint16_t attr)
{
    entrada_t e;
    memset(&e, 0, sizeof e);
    e.fl = f->fl; e.pai = f->pai; e.idx = f->idx; e.qtd = f->qtd; e.vdl = f->vdl; e.attr = attr;
    return e;
}

/* Reescribe el grupo del archivo con lo que lleva "f" (el atributo de archivo y las fechas se tocan). */
static int atualizar_arq(exfat_arq_t *f)
{
    uint8_t g[GRUPO_MAX * 32];
    if (f->qtd > GRUPO_MAX || !grupo_ler(&f->pai, f->idx, f->qtd, g)) return ERRO_ACESSO;
    entrada_t e = entrada_do_arq(f, *(uint16_t *)(g + 4) | A_ARQ);
    return gravar_entrada(&e, true);
}

static int ex_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    vp = priv;
    exfat_arq_t *f = arq;
    entrada_t e;
    memset(f, 0, sizeof *f);
    *tamanho = 0;
    bool escreve = (modo & (ABRIR_ESCREVER | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) != 0;
    if (achar(rel, &e)) {
        if (e.attr & A_PASTA) return terminar(ERRO_E_PASTA);
        if (escreve) {
            SOLEITURA();
            if (e.attr & A_SOMENTE) return terminar(ERRO_ACESSO);
            if (e.grande) return terminar(ERRO_ACESSO);   /* 4 GB o más: el VFS no cuenta tanto */
        }
        abrir_de(f, &e);
        if (modo & ABRIR_TRUNCAR) {
            fl_liberar_de(&f->fl, 0);
            f->fl.tam = 0; f->vdl = 0;
            int r = atualizar_arq(f);
            if (r < 0) return terminar_liberando(r);
            *tamanho = 0;
            return terminar_liberando(0);
        }
        *tamanho = f->fl.tam;
        return terminar(0);
    }
    if (!(modo & ABRIR_CRIAR)) return terminar(ERRO_NAO_EXISTE);
    SOLEITURA();
    entrada_t pasta;
    const char *nome;
    int r = separar(rel, &pasta, &nome);
    if (r < 0) return terminar(r);
    memset(&e, 0, sizeof e);
    r = criar_entrada(&pasta, nome, A_ARQ, &e);
    if (r < 0) return terminar(r);
    abrir_de(f, &e);
    return terminar(0);
}

/* Bytes, desde "no" dentro del clúster "cl" (número "ci" del archivo), que están en clústeres
 * seguidos en el disco: lo que una lectura o grabación lleva en un comando. */
static uint32_t racha(exfat_arq_t *f, uint32_t cl, uint32_t ci, uint32_t no, uint32_t quer, uint32_t *ult, uint32_t *n_ult)
{
    uint32_t disp = v.bpc - no;
    *ult = cl; *n_ult = ci;
    while (disp < quer && disp < RACHA_MAX) {
        uint32_t prox;
        if (*n_ult + 1 >= f->fl.ncl) break;
        if (f->fl.sem_cadeia) prox = *ult + 1;
        else { prox = fat_ler(*ult); if (prox != *ult + 1) break; }
        if (!cluster_valido(prox)) break;
        *ult = prox; (*n_ult)++;
        disp += v.bpc;
    }
    return disp < quer ? disp : quer;
}

static int ex_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    vp = priv;
    exfat_arq_t *f = arq;
    if (pos >= f->fl.tam) return 0;
    if (n > f->fl.tam - pos) n = f->fl.tam - pos;
    uint8_t *destino = buf;
    /* Lo que hay después del ValidDataLength son ceros. */
    uint32_t reais = pos >= f->vdl ? 0 : (f->vdl - pos < n ? f->vdl - pos : n);
    uint32_t feito = 0;
    bool ok = true;
    while (feito < reais) {
        uint32_t p = pos + feito, ci = p / v.bpc, no = p % v.bpc, ult, nult;
        uint32_t cl = cluster_de(f, ci);
        if (!cl) { ok = false; break; }
        uint32_t parte = racha(f, cl, ci, no, reais - feito, &ult, &nult);
        if (ler_bytes(setor_do_cluster(cl) + no / 512, no % 512, parte, destino + feito) < 0) { ok = false; break; }
        feito += parte;
        if (!f->fl.sem_cadeia) { f->cache_cl = ult; f->cache_n = nult; }
    }
    if (ok && reais < n) { memset(destino + reais, 0, n - reais); feito = n; }
    return terminar((int)feito);
}

/* Graba datos en clústeres que ya existen. */
static int gravar_dados(exfat_arq_t *f, uint32_t pos, const uint8_t *orig, uint32_t n)
{
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t p = pos + feito, ci = p / v.bpc, no = p % v.bpc, ult, nult;
        uint32_t cl = cluster_de(f, ci);
        if (!cl) return ERRO_ACESSO;
        uint32_t parte = racha(f, cl, ci, no, n - feito, &ult, &nult);
        if (gravar_bytes(setor_do_cluster(cl) + no / 512, no % 512, parte, orig + feito) < 0) return ERRO_ACESSO;
        feito += parte;
        if (!f->fl.sem_cadeia) { f->cache_cl = ult; f->cache_n = nult; }
    }
    return 0;
}

/* Pone a cero [a, b) de un archivo cuyos clústeres ya existen. */
static int zerar_intervalo(exfat_arq_t *f, uint32_t a, uint32_t b)
{
    if (!v.zeros && !(v.zeros = kcalloc(BLOCO_ZEROS))) return ERRO_SEM_MEMORIA;
    while (a < b) {
        uint32_t k = b - a > BLOCO_ZEROS ? BLOCO_ZEROS : b - a;
        int r = gravar_dados(f, a, v.zeros, k);
        if (r < 0) return r;
        a += k;
    }
    return 0;
}

static int ex_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho)
{
    vp = priv;
    exfat_arq_t *f = arq;
    SOLEITURA();
    if (n == 0) return 0;
    if ((uint64_t)pos + n > 0xFFFFFFFFull) return terminar(ERRO_INVALIDO);
    uint32_t precisa = n_clusters(pos + n);
    if (precisa > f->fl.ncl) {
        f->cache_cl = 0;
        if (fl_ampliar(&f->fl, precisa) < 0) return terminar(ERRO_SEM_MEMORIA);
    }
    if (pos > f->vdl) {
        int r = zerar_intervalo(f, f->vdl, pos);
        if (r < 0) return terminar(r);
    }
    int r = gravar_dados(f, pos, buf, n);
    uint32_t feito = r < 0 ? 0 : n;
    if (feito) {
        if (pos + feito > f->vdl) f->vdl = pos + feito;
        if (pos + feito > f->fl.tam) f->fl.tam = pos + feito;
    }
    int u = atualizar_arq(f);
    *tamanho = f->fl.tam;
    if (r < 0) return terminar(r);
    return terminar(u < 0 ? u : (int)feito);
}

static void ex_fechar(void *priv, void *arq)
{
    vp = priv;
    (void)arq;
    terminar(0);
}

static int ex_truncar(void *priv, void *arq, uint32_t tam)
{
    vp = priv;
    exfat_arq_t *f = arq;
    SOLEITURA();
    uint32_t quer = n_clusters(tam);
    f->cache_cl = 0;
    if (tam >= f->fl.tam) {
        if (quer > f->fl.ncl && fl_ampliar(&f->fl, quer) < 0) return terminar(ERRO_SEM_MEMORIA);
        f->fl.tam = tam;   /* lo nuevo está detrás del VDL: se lee como ceros */
        int r = atualizar_arq(f);
        return terminar(r);
    }
    f->fl.tam = tam;
    if (f->vdl > tam) f->vdl = tam;
    fl_liberar_de(&f->fl, quer);
    int r = atualizar_arq(f);
    return terminar_liberando(r);
}

static int ex_sincronizar(void *priv, void *arq)
{
    vp = priv;
    (void)arq;
    return terminar(0);
}

static void ex_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    vp = priv;
    uint32_t kb = v.bpc >= 1024 ? v.bpc / 1024 : 0;
    if (kb) { *total_kb = v.total_clusters * kb; *livre_kb = v.livres * kb; }
    else { *total_kb = v.total_clusters / 2; *livre_kb = v.livres / 2; }
}

static int ex_criar_pasta(void *priv, const char *rel)
{
    vp = priv;
    SOLEITURA();
    entrada_t e, pasta;
    if (achar(rel, &e)) return terminar(ERRO_JA_EXISTE);
    const char *nome;
    int r = separar(rel, &pasta, &nome);
    if (r < 0) return terminar(r);
    memset(&e, 0, sizeof e);
    if (fl_ampliar(&e.fl, 1) < 0) return terminar(ERRO_SEM_MEMORIA);
    zerar_cluster(e.fl.primeiro);
    e.fl.tam = v.bpc;
    e.vdl = v.bpc;
    r = criar_entrada(&pasta, nome, A_PASTA, &e);
    if (r < 0) fl_liberar_de(&e.fl, 0);
    return terminar(r);
}

static bool pasta_vazia(const fluxo_t *fl)
{
    fluxo_t dir = *fl;
    cursor_t c = { &dir, 0, 0 };
    for (uint32_t i = 0; ; i++) {
        uint8_t *p = cur_ptr(&c, i, NULL);
        if (!p || p[0] == 0x00) return true;
        if (p[0] == T_ARQ) return false;
    }
}

static int ex_remover_pasta(void *priv, const char *rel)
{
    vp = priv;
    SOLEITURA();
    entrada_t e;
    if (!*rel) return terminar(ERRO_ACESSO);
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (!(e.attr & A_PASTA)) return terminar(ERRO_NAO_E_PASTA);
    if (!pasta_vazia(&e.fl)) return terminar(ERRO_NAO_VAZIA);
    apagar_entrada(&e);
    fl_liberar_de(&e.fl, 0);
    return terminar_liberando(0);
}

static int ex_apagar(void *priv, const char *rel)
{
    vp = priv;
    SOLEITURA();
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (e.attr & A_PASTA) return terminar(ERRO_E_PASTA);
    apagar_entrada(&e);
    fl_liberar_de(&e.fl, 0);
    return terminar_liberando(0);
}

static bool mesma_entrada(const entrada_t *a, const entrada_t *b)
{
    return a->idx == b->idx && a->pai.primeiro == b->pai.primeiro && a->idx != SEM_ENTRADA;
}

static int ex_renomear(void *priv, const char *de, const char *para)
{
    vp = priv;
    SOLEITURA();
    entrada_t e, alvo, pasta;
    if (!*de) return terminar(ERRO_ACESSO);
    if (!achar(de, &e)) return terminar(ERRO_NAO_EXISTE);
    char final[CAMINHO_MAX];
    txt_copia_n(final, para, sizeof final);
    bool substitui = false;
    if (achar(para, &alvo) && mesma_entrada(&e, &alvo)) {
        const char *a = de, *b = para;
        while (*a == '/') a++;
        while (*b == '/') b++;
        if (txt_compara(a, b) == 0) return terminar(0);   /* mismo nombre: nada que hacer */
    } else if (achar(para, &alvo)) {
        if (alvo.attr & A_PASTA) {          /* mover dentro de la carpeta */
            size_t n = txt_tamanho(final);
            if (n) final[n++] = '/';
            const char *nome_de = de;
            for (const char *p = de; *p; p++) if (*p == '/') nome_de = p + 1;
            txt_copia_n(final + n, nome_de, sizeof final - n);
            if (achar(final, &alvo)) return terminar(ERRO_JA_EXISTE);
        } else {
            if (e.attr & A_PASTA) return terminar(ERRO_JA_EXISTE);
            substitui = true;
        }
    }
    const char *nome;
    int r = separar(final, &pasta, &nome);
    if (r < 0) return terminar(r);
    /* Una carpeta no puede ir dentro de sí misma (exFAT no tiene "..": se mira la ruta). */
    if (e.attr & A_PASTA) {
        size_t n = txt_tamanho(de);
        while (n && de[n - 1] == '/') n--;
        char a[CAMINHO_MAX];
        txt_copia_n(a, de, n + 1 < sizeof a ? n + 1 : sizeof a);
        a[n] = 0;
        size_t m = txt_tamanho(final);
        if (m > n && final[n] == '/' && prefixo_igual(final, a, n)) return terminar(ERRO_INVALIDO);
    }
    entrada_t nova = e;
    r = criar_entrada(&pasta, nome, e.attr, &nova);
    if (r < 0) return terminar(r);
    /* Las fechas del grupo viejo pasan al nuevo (renombrar no cambia la creación). */
    {
        uint8_t g[GRUPO_MAX * 32];
        if (grupo_ler(&nova.pai, nova.idx, nova.qtd, g)) {
            *(uint32_t *)(g + 8) = e.t_cria;  *(uint32_t *)(g + 12) = e.t_mod;  *(uint32_t *)(g + 16) = e.t_acesso;
            g[22] = e.d_cria; g[23] = e.d_mod; g[24] = e.d_acesso;
            g[20] = e.cs_cria; g[21] = e.cs_mod;   /* D341 */
            *(uint16_t *)(g + 2) = soma_grupo(g, nova.qtd);
            grupo_gravar(&nova.pai, nova.idx, nova.qtd, g);
        }
    }
    /* Primero queda grabada la entrada nueva; luego se borra la vieja (un corte entre las dos
     * deja el archivo duplicado, nunca perdido). */
    int d = descarregar(true);
    if (d < 0) { erro_es = 0; return d; }
    apagar_entrada(&e);
    if (substitui) { apagar_entrada(&alvo); fl_liberar_de(&alvo.fl, 0); }
    return terminar_liberando(0);
}

/* D341: modificación (con sus centésimas: resolución de 1 s) y último acceso del grupo. La raíz no
 * tiene grupo: no se guarda nada y no es un error. */
static int ex_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{
    vp = priv;
    SOLEITURA();
    entrada_t e;
    if (!*rel) return 0;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (e.idx == SEM_ENTRADA) return terminar(0);
    uint8_t g[GRUPO_MAX * 32];
    if (e.qtd > GRUPO_MAX || !grupo_ler(&e.pai, e.idx, e.qtd, g)) return terminar(ERRO_ACESSO);
    uint8_t d, cs;
    if (mod) { *(uint32_t *)(g + 12) = utc_para_ts(mod, &d, &cs); g[23] = d; g[21] = cs; }
    if (acesso) { *(uint32_t *)(g + 16) = utc_para_ts(acesso, &d, &cs); g[24] = d; }
    *(uint16_t *)(g + 2) = soma_grupo(g, e.qtd);
    return terminar(grupo_gravar(&e.pai, e.idx, e.qtd, g) ? 0 : ERRO_ACESSO);
}

const sistema_arquivos_t fs_exfat = {
    .nome = "exfat", .tam_arq = sizeof(exfat_arq_t),
    .montar = ex_montar, .desmontar = ex_desmontar, .info = ex_info, .datas = ex_datas, .listar = ex_listar,
    .abrir = ex_abrir, .ler = ex_ler, .escrever = ex_escrever, .truncar = ex_truncar, .fechar = ex_fechar,
    .sincronizar = ex_sincronizar, .criar_pasta = ex_criar_pasta, .remover_pasta = ex_remover_pasta,
    .apagar = ex_apagar, .renomear = ex_renomear, .espaco = ex_espaco,
    .fixar_datas = ex_fixar_datas,   /* D341 */
};

