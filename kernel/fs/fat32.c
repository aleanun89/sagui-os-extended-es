// SPDX-License-Identifier: GPL-3.0-only
/* fat32.c - el sistema de archivos FAT32 del pendrive de datos (H32, H33).
 *
 * Estructura de un volumen FAT32 (especificación de Microsoft):
 *   sector 0 del volumen: sector de arranque con el BPB (tamaños y posiciones);
 *   sectores reservados (incluye el FSInfo, con la cuenta de libres);
 *   dos copias de la FAT: una entrada de 32 bits (28 usados) por cluster,
 *     diciendo cuál es el próximo cluster del archivo (o "fin", o "libre");
 *   área de datos, en clusters, empezando en el cluster 2. La carpeta raíz es un
 *     archivo como los demás, a partir del cluster indicado en el BPB.
 * Una carpeta es una lista de entradas de 32 bytes. Los nombres largos quedan en
 * entradas especiales (atributo 0x0F) antes de la entrada corta 8.3. Ver D026. */
#include "fat32.h"
#include "montagem.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "caminho.h"
#include "texto.h"
#include "trava.h"
#include "processo.h"
#include "cpu.h"

#define FIM_CADEIA   0x0FFFFFF8u
#define MARCA_FIM    0x0FFFFFFFu
#define ATR_SOMENTE  0x01
#define ATR_OCULTO   0x02
#define ATR_SISTEMA  0x04
#define ATR_ROTULO   0x08
#define ATR_PASTA    0x10
#define ATR_ARQUIVO  0x20
#define ATR_LFN      0x0F
#define LIVRE_E5     0xE5

typedef struct {
    disco_t *disco;
    uint32_t inicio;            /* sector 0 del volumen */
    uint32_t setores_cluster;
    uint32_t bytes_cluster;
    uint32_t reservados, n_fats, setores_fat;
    uint32_t inicio_fat, inicio_dados;
    uint32_t raiz;
    uint32_t total_clusters;
    uint32_t setor_fsinfo;
    uint32_t livres, dica;
    bool     montado;
    /* Caché de sectores: una por volumen (D040). */
    struct { uint32_t setor; bool valido, sujo; uint32_t uso; uint8_t *dados; } cache_[16];
    uint32_t relogio_cache;
    int      erro_es;     /* primer error de E/S desde la última operación */
    /* Lectura anticipada (D095): quien lee un archivo en trozos de 1 KB no
     * puede pagar un comando USB por trozo. En una lectura secuencial, el
     * primer trozo pequeño trae una ventana de JANELA_SETORES sectores y los
     * siguientes salen de la memoria. */
    uint8_t *ra_dados;
    uint32_t ra_setor, ra_n;      /* la ventana guarda los sectores [ra_setor, ra_setor + ra_n) */
    uint32_t ra_prox;             /* el sector que vendría después de la última lectura pequeña */
} volume_t;
#define JANELA_SETORES 256        /* 128 KB: un comando USB entero */
#define DIRETO_SETORES 64         /* pedidos desde 32 KB van directo al disco, sin ventana */
#define RACHA_MAX      (128u * 1024u)   /* más que eso por pedido no compensa: el USB divide en comandos de 128 KB */

/* Cada montaje tiene su propio volume_t. Las operaciones empiezan apuntando "vp"
 * al volumen del montaje; el resto del código sigue escribiendo "v.". Las
 * llamadas al sistema se ejecutan con las interrupciones desactivadas (D007),
 * así que ninguna otra operación cambia el "vp" a mitad de una. */
static volume_t *vp;
#define v (*vp)
#define cache (vp->cache_)
#define relogio_cache (vp->relogio_cache)
#define erro_es (vp->erro_es)

/* ------------------------------------------------------------ caché de sectores */

#define CACHE 16

static int gravar_setor_cache(int i)
{
    if (!cache[i].sujo) return 0;
    int r = disco_escrever(v.disco, cache[i].setor, 1, cache[i].dados);
    /* Sector de la FAT: graba también en las otras copias. */
    if (r == 0 && cache[i].setor >= v.inicio_fat && cache[i].setor < v.inicio_fat + v.setores_fat)
        for (uint32_t k = 1; k < v.n_fats && r == 0; k++)
            r = disco_escrever(v.disco, cache[i].setor + k * v.setores_fat, 1, cache[i].dados);
    if (r < 0) { if (!erro_es) erro_es = ERRO_ACESSO; return r; }
    cache[i].sujo = false;
    return 0;
}

/* Graba los sectores sucios de la caché. Sectores vecinos salen en un solo comando (un
 * comando USB cuesta tres transferencias, no importa el tamaño), y la FAT se
 * graba también en las otras copias. */
static int descarregar(void)
{
    int ordem[CACHE], q = 0, r = 0;
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].sujo) ordem[q++] = i;
    if (!q) return 0;
    for (int a = 1; a < q; a++) {   /* inserción por número de sector */
        int x = ordem[a], b = a;
        while (b > 0 && cache[ordem[b - 1]].setor > cache[x].setor) { ordem[b] = ordem[b - 1]; b--; }
        ordem[b] = x;
    }
    uint8_t *lote = q > 1 ? kmalloc(CACHE * 512) : NULL;
    for (int a = 0; a < q; ) {
        uint32_t s0 = cache[ordem[a]].setor;
        bool na_fat = s0 >= v.inicio_fat && s0 < v.inicio_fat + v.setores_fat;
        int b = a + 1;
        while (lote && b < q && cache[ordem[b]].setor == s0 + (uint32_t)(b - a) &&
               (cache[ordem[b]].setor >= v.inicio_fat && cache[ordem[b]].setor < v.inicio_fat + v.setores_fat) == na_fat) b++;
        int k = b - a, ok;
        if (k == 1) {
            ok = gravar_setor_cache(ordem[a]) == 0;
        } else {
            for (int j = 0; j < k; j++) memcpy(lote + j * 512, cache[ordem[a + j]].dados, 512);
            int w = disco_escrever(v.disco, s0, (uint32_t)k, lote);
            if (w == 0 && na_fat)
                for (uint32_t c = 1; c < v.n_fats && w == 0; c++) w = disco_escrever(v.disco, s0 + c * v.setores_fat, (uint32_t)k, lote);
            ok = w == 0;
            if (ok) for (int j = 0; j < k; j++) cache[ordem[a + j]].sujo = false;
            else if (!erro_es) erro_es = ERRO_ACESSO;
        }
        if (!ok) r = ERRO_ACESSO;
        a = b;
    }
    if (lote) kfree(lote);
    return r;
}

static void esquecer_cache(void)
{
    for (int i = 0; i < CACHE; i++) cache[i].valido = cache[i].sujo = false;
    vp->ra_n = 0;
}

/* Devuelve el contenido de un sector (absoluto) desde la caché; NULL si falla. */
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
    cache[i].setor = s;
    cache[i].valido = true;
    cache[i].sujo = false;
    cache[i].uso = ++relogio_cache;
    return cache[i].dados;
}

static void sujar(uint32_t s)
{
    if (s >= vp->ra_setor && s < vp->ra_setor + vp->ra_n) vp->ra_n = 0;   /* la ventana quedaría vieja */
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].setor == s) cache[i].sujo = true;
}

/* Graba sectores enteros directamente, manteniendo la caché coherente. */
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
    /* Los sectores sucios en la caché son más nuevos que los del pendrive. */
    int r = disco_ler(v.disco, s, n, dados);
    if (r < 0) { if (!erro_es) erro_es = ERRO_ACESSO; return r; }
    for (int i = 0; i < CACHE; i++)
        if (cache[i].valido && cache[i].sujo && cache[i].setor >= s && cache[i].setor < s + n)
            memcpy(dados + (cache[i].setor - s) * 512, cache[i].dados, 512);
    return 0;
}

/* Lectura de sectores de archivo. Las peticiones grandes van directo al disco (un
 * comando USB por hasta 128 KB); las pequeñas, cuando siguen a la anterior, vienen
 * de una ventana de 128 KB leída de una vez. */
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
    if (ler_direto(s, k, vp->ra_dados) < 0) return ler_direto(s, n, dados);   /* repite sólo la petición: el error correcto */
    vp->ra_setor = s;
    vp->ra_n = k;
    memcpy(dados, vp->ra_dados, n * 512);
    return 0;
}

/* ------------------------------------------------------------ FAT */

static uint32_t setor_do_cluster(uint32_t c) { return v.inicio_dados + (c - 2) * v.setores_cluster; }

static uint32_t fat_ler_entrada(uint32_t c)
{
    uint8_t *s = setor(v.inicio_fat + c * 4 / 512);
    if (!s) return MARCA_FIM;
    return *(uint32_t *)(s + (c * 4) % 512) & 0x0FFFFFFFu;
}

static void fat_gravar_entrada(uint32_t c, uint32_t valor)
{
    uint32_t sn = v.inicio_fat + c * 4 / 512;
    uint8_t *s = setor(sn);
    if (!s) return;
    uint32_t *e = (uint32_t *)(s + (c * 4) % 512);
    *e = (*e & 0xF0000000u) | (valor & 0x0FFFFFFFu);   /* los 4 bits altos están reservados */
    sujar(sn);
}

static bool cluster_valido(uint32_t c) { return c >= 2 && c < v.total_clusters + 2; }

/* Busca un cluster libre desde la sugerencia, lo marca como fin de cadena y
 * (opcionalmente) lo enlaza con el anterior. Pone a cero el contenido si se pide. */
static uint32_t alocar_cluster(uint32_t anterior, bool zerar)
{
    uint32_t inicio = cluster_valido(v.dica) ? v.dica : 2;
    for (uint32_t k = 0; k < v.total_clusters; k++) {
        uint32_t c = 2 + (inicio - 2 + k) % v.total_clusters;
        if (fat_ler_entrada(c) != 0) continue;
        fat_gravar_entrada(c, MARCA_FIM);
        if (anterior) fat_gravar_entrada(anterior, c);
        if (v.livres != 0xFFFFFFFFu && v.livres) v.livres--;
        v.dica = c + 1;
        if (zerar) {
            uint8_t *z = kcalloc(v.bytes_cluster);
            if (z) { gravar_direto(setor_do_cluster(c), v.setores_cluster, z); kfree(z); }
        }
        return c;
    }
    return 0;   /* pendrive lleno */
}

static void liberar_cadeia(uint32_t c)
{
    for (int guarda = 0; cluster_valido(c) && guarda < 0x1000000; guarda++) {
        uint32_t prox = fat_ler_entrada(c);
        fat_gravar_entrada(c, 0);
        if (v.livres != 0xFFFFFFFFu) v.livres++;
        if (prox >= FIM_CADEIA || prox == 0) break;
        c = prox;
    }
}

/* El n-ésimo cluster de una cadena (0 si no existe). */
static uint32_t cluster_n(uint32_t c, uint32_t n)
{
    for (uint32_t i = 0; i < n && cluster_valido(c); i++) {
        c = fat_ler_entrada(c);
        if (c >= FIM_CADEIA) return 0;
    }
    return cluster_valido(c) ? c : 0;
}

/* ------------------------------------------------------------ nombres */

static uint8_t minuscula(uint8_t c)
{
    if (c >= 'A' && c <= 'Z') return (uint8_t)(c + 32);
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return (uint8_t)(c + 32);   /* À-Þ, menos × */
    return c;
}

static bool nomes_iguais(const char *a, const char *b)
{
    while (*a && *b)
        if (minuscula((uint8_t)*a++) != minuscula((uint8_t)*b++)) return false;
    return *a == *b;
}

static uint8_t soma_curta(const uint8_t *nome11)
{
    uint8_t s = 0;
    for (int i = 0; i < 11; i++) s = (uint8_t)(((s & 1) << 7) + (s >> 1) + nome11[i]);
    return s;
}

/* Nombre corto "NOME    EXT" -> "nome.ext" (respetando las marcas de minúsculas). */
static void nome_curto_para_texto(const uint8_t *e, char *saida)
{
    int k = 0;
    bool min_base = e[12] & 0x08, min_ext = e[12] & 0x10;
    for (int i = 0; i < 8 && e[i] != ' '; i++) {
        uint8_t c = (i == 0 && e[0] == 0x05) ? 0xE5 : e[i];
        saida[k++] = (char)(min_base ? minuscula(c) : c);
    }
    if (e[8] != ' ') {
        saida[k++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++) saida[k++] = (char)(min_ext ? minuscula(e[i]) : e[i]);
    }
    saida[k] = 0;
}

/* Una entrada leída de una carpeta, ya con el nombre armado. */
typedef struct {
    char     nome[NOME_MAX];
    uint8_t  atributos;
    uint32_t primeiro, tamanho;
    uint32_t setor, desloc;           /* dónde está la entrada corta */
    uint32_t setor_lfn, desloc_lfn;   /* la primera entrada de nombre largo (o la corta) */
    uint32_t qtd_lfn;
    uint16_t cria_data, cria_hora, acesso_data, mod_data, mod_hora;   /* D080: hora local, formato FAT */
} entrada_t;

typedef struct {
    uint32_t cluster;     /* cluster actual de la carpeta */
    uint32_t setor_rel;   /* sector dentro del cluster */
    uint32_t idx;         /* entrada dentro del sector (0..15) */
    bool     acabou;
} cursor_t;

static void cursor_iniciar(cursor_t *c, uint32_t pasta)
{
    c->cluster = pasta; c->setor_rel = 0; c->idx = 0; c->acabou = !cluster_valido(pasta);
}

/* Devuelve la siguiente entrada cruda (32 bytes) y dónde está. */
static uint8_t *cursor_proxima(cursor_t *c, uint32_t *sn, uint32_t *desl)
{
    if (c->acabou) return NULL;
    if (c->idx == 16) {
        c->idx = 0;
        if (++c->setor_rel == v.setores_cluster) {
            c->setor_rel = 0;
            uint32_t prox = fat_ler_entrada(c->cluster);
            if (prox >= FIM_CADEIA || !cluster_valido(prox)) { c->acabou = true; return NULL; }
            c->cluster = prox;
        }
    }
    *sn = setor_do_cluster(c->cluster) + c->setor_rel;
    *desl = c->idx * 32;
    c->idx++;
    uint8_t *s = setor(*sn);
    if (!s) { c->acabou = true; return NULL; }
    return s + *desl;
}

/* Siguiente entrada válida (archivo, carpeta o etiqueta) de la carpeta. */
static bool ler_entrada(cursor_t *c, entrada_t *e)
{
    uint16_t longo[260];
    int partes = 0;
    uint8_t soma = 0;
    uint32_t sn, desl, lfn_sn = 0, lfn_desl = 0, qtd = 0;
    for (;;) {
        uint8_t *x = cursor_proxima(c, &sn, &desl);
        if (!x || x[0] == 0x00) { c->acabou = true; return false; }   /* 0x00: fin de la carpeta */
        if (x[0] == LIVRE_E5) { partes = 0; qtd = 0; continue; }
        if (x[11] == ATR_LFN) {
            int ordem = x[0] & 0x1F;
            if (x[0] & 0x40) {               /* la última parte viene primero */
                partes = ordem; soma = x[13]; memset(longo, 0, sizeof longo);
                lfn_sn = sn; lfn_desl = desl; qtd = 0;
            }
            if (ordem < 1 || ordem > 20 || x[13] != soma) { partes = 0; continue; }
            static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
            for (int i = 0; i < 13; i++) longo[(ordem - 1) * 13 + i] = (uint16_t)(x[pos[i]] | (x[pos[i] + 1] << 8));
            qtd++;
            continue;
        }
        memset(e, 0, sizeof *e);
        e->atributos = x[11];
        e->primeiro = (uint32_t)(x[26] | (x[27] << 8)) | ((uint32_t)(x[20] | (x[21] << 8)) << 16);
        e->tamanho = *(uint32_t *)(x + 28);
        e->setor = sn; e->desloc = desl;
        e->cria_hora = *(uint16_t *)(x + 14); e->cria_data = *(uint16_t *)(x + 16);
        e->acesso_data = *(uint16_t *)(x + 18);
        e->mod_hora = *(uint16_t *)(x + 22); e->mod_data = *(uint16_t *)(x + 24);
        if (partes && soma == soma_curta(x)) {
            int k = 0;
            for (int i = 0; i < partes * 13 && longo[i] && longo[i] != 0xFFFF && k < NOME_MAX - 1; i++)
                e->nome[k++] = longo[i] < 0x100 ? (char)longo[i] : '_';
            e->nome[k] = 0;
            e->setor_lfn = lfn_sn; e->desloc_lfn = lfn_desl; e->qtd_lfn = qtd;
        } else {
            nome_curto_para_texto(x, e->nome);
            e->setor_lfn = sn; e->desloc_lfn = desl; e->qtd_lfn = 0;
        }
        return true;
    }
}

/* Busca un nombre en una carpeta (ignora "." y ".." y la etiqueta). */
static bool achar_na_pasta(uint32_t pasta, const char *nome, size_t n, entrada_t *e)
{
    char alvo[NOME_MAX];
    if (n >= NOME_MAX) n = NOME_MAX - 1;
    memcpy(alvo, nome, n);
    alvo[n] = 0;
    cursor_t c;
    cursor_iniciar(&c, pasta);
    while (ler_entrada(&c, e)) {
        if (e->atributos & ATR_ROTULO) continue;
        if (nomes_iguais(e->nome, alvo)) return true;
    }
    return false;
}

/* Sigue una ruta relativa. Devuelve la entrada; para la raíz, rellena una
 * entrada de carpeta con el cluster de la raíz. */
static bool achar(const char *rel, entrada_t *e)
{
    memset(e, 0, sizeof *e);
    e->atributos = ATR_PASTA;
    e->primeiro = v.raiz;
    const char *p = rel;
    while (*p == '/') p++;
    while (*p) {
        const char *fim = p;
        while (*fim && *fim != '/') fim++;
        if (!(e->atributos & ATR_PASTA)) return false;
        uint32_t pasta = e->primeiro ? e->primeiro : v.raiz;
        if (!achar_na_pasta(pasta, p, (size_t)(fim - p), e)) return false;
        if ((e->atributos & ATR_PASTA) && e->primeiro == 0) e->primeiro = v.raiz;   /* ".." hacia la raíz */
        p = *fim ? fim + 1 : fim;
    }
    return true;
}

/* Separa "a/b/c" en carpeta "a/b" (entrada) y nombre "c". */
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
    if (!(pasta->atributos & ATR_PASTA)) return ERRO_NAO_E_PASTA;
    return 0;
}

/* ------------------------------------------------------------ creación de entradas */

/* El FAT guarda la hora LOCAL, sin zona (es lo que hacen el DOS y Windows):
 * la hora de la zona del sistema (D080). */
static uint16_t data_fat(void)
{
    fuso_data_t d;
    hora_local(&d);
    return (uint16_t)(((d.ano - 1980) << 9) | (d.mes << 5) | d.dia);
}

static uint16_t hora_fat(void)
{
    fuso_data_t d;
    hora_local(&d);
    return (uint16_t)((d.hora << 11) | (d.minuto << 5) | (d.segundo / 2));
}

/* Fecha y hora del FAT (locales) -> UTC; 0 = sin fecha. */
static uint32_t fat_para_utc(uint16_t data, uint16_t hora)
{
    if (!data) return 0;
    int ano = 1980 + (data >> 9), mes = (data >> 5) & 15, dia = data & 31;
    if (mes < 1 || mes > 12 || dia < 1) return 0;
    uint32_t local = fuso_compor(ano, mes, dia, hora >> 11, (hora >> 5) & 63, (hora & 31) * 2);
    return hora_local_para_utc(local);
}

/* D341: un instante UTC -> fecha y hora del FAT (locales, segundos de 2 en 2, por abajo, como
 * Linux y mtools). Fuera del rango del FAT (1980-2107) se queda en el extremo. */
static void utc_para_fat(uint32_t utc, uint16_t *data, uint16_t *hora)
{
    fuso_data_t d;
    hora_local_de(utc, &d);
    if (d.ano < 1980) { *data = (uint16_t)((1 << 5) | 1); *hora = 0; return; }
    if (d.ano > 2107) { *data = (uint16_t)((127 << 9) | (12 << 5) | 31); *hora = (uint16_t)((23 << 11) | (59 << 5) | 29); return; }
    *data = (uint16_t)(((d.ano - 1980) << 9) | (d.mes << 5) | d.dia);
    *hora = (uint16_t)((d.hora << 11) | (d.minuto << 5) | (d.segundo / 2));
}

static bool valido_8_3(uint8_t c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    return c == '-' || c == '_' || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '(' || c == ')' ||
           c == '@' || c == '^' || c == '{' || c == '}' || c == '~' || c == '\'';
}

static bool nome_curto_existe(uint32_t pasta, const uint8_t *n11)
{
    cursor_t c;
    cursor_iniciar(&c, pasta);
    uint32_t sn, desl;
    uint8_t *x;
    while ((x = cursor_proxima(&c, &sn, &desl)) && x[0]) {
        if (x[0] != LIVRE_E5 && x[11] != ATR_LFN && memcmp(x, n11, 11) == 0) return true;
    }
    return false;
}

/* Genera un nombre corto único: "RELATO~1TXT". */
static void gerar_nome_curto(uint32_t pasta, const char *nome, uint8_t *n11)
{
    const char *ponto = NULL;
    for (const char *p = nome; *p; p++) if (*p == '.') ponto = p;
    if (ponto == nome) ponto = NULL;
    uint8_t base[8], ext[3];
    int nb = 0, ne = 0;
    for (const char *p = nome; *p && (!ponto || p < ponto) && nb < 8; p++) {
        uint8_t c = (uint8_t)*p;
        if (c == ' ' || c == '.') continue;
        if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
        base[nb++] = valido_8_3(c) ? c : '_';
    }
    if (ponto)
        for (const char *p = ponto + 1; *p && ne < 3; p++) {
            uint8_t c = (uint8_t)*p;
            if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
            ext[ne++] = valido_8_3(c) ? c : '_';
        }
    if (nb == 0) base[nb++] = '_';
    for (int k = 1; k < 1000000; k++) {
        char sufixo[10];
        int ns = snformatar(sufixo, sizeof sufixo, "~%d", k);
        memset(n11, ' ', 11);
        int cabe = nb < 8 - ns ? nb : 8 - ns;
        memcpy(n11, base, (size_t)cabe);
        memcpy(n11 + cabe, sufixo, (size_t)ns);
        memcpy(n11 + 8, ext, (size_t)ne);
        if (!nome_curto_existe(pasta, n11)) return;
    }
}

/* Busca "n" entradas libres seguidas en la carpeta (extendiéndola si hace falta). */
static bool achar_espaco(uint32_t pasta, uint32_t n, uint32_t *sns, uint32_t *desls)
{
    for (int volta = 0; volta < 2; volta++) {
        cursor_t c;
        cursor_iniciar(&c, pasta);
        uint32_t seguidas = 0, sn, desl, ultimo = pasta;
        uint8_t *x;
        while ((x = cursor_proxima(&c, &sn, &desl))) {
            ultimo = c.cluster;
            if (x[0] == 0x00 || x[0] == LIVRE_E5) {
                sns[seguidas] = sn; desls[seguidas] = desl;
                if (++seguidas == n) return true;
            } else seguidas = 0;
        }
        /* Sin espacio: un cluster más (en cero = entradas libres) al final de la carpeta. */
        while (cluster_valido(fat_ler_entrada(ultimo)) && fat_ler_entrada(ultimo) < FIM_CADEIA) ultimo = fat_ler_entrada(ultimo);
        if (!alocar_cluster(ultimo, true)) return false;
    }
    return false;
}

static int criar_entrada(uint32_t pasta, const char *nome, uint8_t atributos, uint32_t primeiro, uint32_t tamanho,
                         uint32_t *setor_out, uint32_t *desl_out)
{
    uint8_t n11[11];
    gerar_nome_curto(pasta, nome, n11);
    uint32_t tam = (uint32_t)txt_tamanho(nome);
    uint32_t partes = (tam + 12) / 13;
    uint32_t sns[22], desls[22];
    if (!achar_espaco(pasta, partes + 1, sns, desls)) return ERRO_SEM_MEMORIA;
    uint8_t soma = soma_curta(n11);
    static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    for (uint32_t k = 0; k < partes; k++) {
        uint32_t ordem = partes - k;             /* grabadas de atrás para adelante */
        uint8_t *x = setor(sns[k]);
        if (!x) return ERRO_ACESSO;
        x += desls[k];
        memset(x, 0, 32);
        x[0] = (uint8_t)(ordem | (k == 0 ? 0x40 : 0));
        x[11] = ATR_LFN;
        x[13] = soma;
        for (int i = 0; i < 13; i++) {
            uint32_t j = (ordem - 1) * 13 + (uint32_t)i;
            uint16_t ch = j < tam ? (uint8_t)nome[j] : (j == tam ? 0x0000 : 0xFFFF);
            x[pos[i]] = (uint8_t)ch;
            x[pos[i] + 1] = (uint8_t)(ch >> 8);
        }
        sujar(sns[k]);
    }
    uint8_t *x = setor(sns[partes]);
    if (!x) return ERRO_ACESSO;
    x += desls[partes];
    memset(x, 0, 32);
    memcpy(x, n11, 11);
    x[11] = atributos;
    uint16_t d = data_fat(), h = hora_fat();
    *(uint16_t *)(x + 14) = h; *(uint16_t *)(x + 16) = d;   /* creación */
    *(uint16_t *)(x + 18) = d;                              /* último acceso */
    *(uint16_t *)(x + 22) = h; *(uint16_t *)(x + 24) = d;   /* modificación */
    *(uint16_t *)(x + 20) = (uint16_t)(primeiro >> 16);
    *(uint16_t *)(x + 26) = (uint16_t)primeiro;
    *(uint32_t *)(x + 28) = tamanho;
    sujar(sns[partes]);
    if (setor_out) { *setor_out = sns[partes]; *desl_out = desls[partes]; }
    return 0;
}

/* Marca la entrada (corta y largas) como borrada. */
static void apagar_entrada(entrada_t *e)
{
    /* Recorre desde la primera parte del nombre largo hasta la corta. */
    uint32_t sn = e->setor_lfn, desl = e->desloc_lfn;
    for (uint32_t k = 0; k <= e->qtd_lfn; k++) {
        uint8_t *x = setor(sn);
        if (!x) return;
        x[desl] = LIVRE_E5;
        sujar(sn);
        if (sn == e->setor && desl == e->desloc) break;
        desl += 32;
        if (desl == 512) {
            desl = 0;
            /* Siguiente sector de la carpeta: puede estar en otro cluster. */
            uint32_t rel = sn - v.inicio_dados;
            uint32_t cl = 2 + rel / v.setores_cluster;
            if ((rel % v.setores_cluster) + 1 < v.setores_cluster) sn++;
            else { uint32_t prox = fat_ler_entrada(cl); if (!cluster_valido(prox)) return; sn = setor_do_cluster(prox); }
        }
    }
}

static void atualizar_entrada(uint32_t sn, uint32_t desl, uint32_t primeiro, uint32_t tamanho)
{
    uint8_t *x = setor(sn);
    if (!x) return;
    x += desl;
    *(uint16_t *)(x + 20) = (uint16_t)(primeiro >> 16);
    *(uint16_t *)(x + 26) = (uint16_t)primeiro;
    *(uint32_t *)(x + 28) = tamanho;
    *(uint16_t *)(x + 22) = hora_fat();
    *(uint16_t *)(x + 24) = data_fat();
    x[11] |= ATR_ARQUIVO;
    sujar(sn);
}

static int terminar(int r)
{
    int d = descarregar();
    if (erro_es) { r = r < 0 ? r : erro_es; erro_es = 0; }
    return r < 0 ? r : (d < 0 ? d : r);
}

/* ------------------------------------------------------------ montaje */

/* Lee el BPB y el FSInfo para "v". "oficial" = montaje de verdad (cuenta
 * como nueva generación y aparece en el log); la lectura de la etiqueta usa la otra. */
static int ler_volume(disco_t *d, bool oficial)
{
    uint8_t *s = kmalloc(512);
    if (!s) return ERRO_SEM_MEMORIA;
    if (disco_ler(d, 0, 1, s) < 0) { kfree(s); return ERRO_ACESSO; }
    v.disco = d;
    v.inicio = 0;
    v.setores_cluster = s[13];
    v.bytes_cluster = v.setores_cluster * 512;
    v.reservados = (uint32_t)(s[14] | (s[15] << 8));
    v.n_fats = s[16];
    v.setores_fat = *(uint32_t *)(s + 36);
    v.raiz = *(uint32_t *)(s + 44);
    v.setor_fsinfo = v.inicio + (uint32_t)(s[48] | (s[49] << 8));
    uint32_t total = (uint32_t)(s[19] | (s[20] << 8));
    if (!total) total = *(uint32_t *)(s + 32);
    v.inicio_fat = v.inicio + v.reservados;
    v.inicio_dados = v.inicio_fat + v.n_fats * v.setores_fat;
    if (!v.setores_cluster || !v.n_fats || !v.setores_fat || total <= v.inicio_dados - v.inicio) { kfree(s); return ERRO_FORMATO; }
    v.total_clusters = (total - (v.inicio_dados - v.inicio)) / v.setores_cluster;
    v.livres = 0xFFFFFFFFu;
    v.dica = 2;
    if (disco_ler(d, v.setor_fsinfo, 1, s) == 0 && *(uint32_t *)s == 0x41615252u && *(uint32_t *)(s + 484) == 0x61417272u) {
        v.livres = *(uint32_t *)(s + 488);
        v.dica = *(uint32_t *)(s + 492);
        if (v.livres > v.total_clusters) v.livres = 0xFFFFFFFFu;
    }
    kfree(s);
    esquecer_cache();
    v.montado = true;
    if (oficial) {
        klog("[FAT32] montado: %u clusters de %u bytes, raiz no cluster %u, %u livres\n",
             v.total_clusters, v.bytes_cluster, v.raiz, v.livres);
    }
    return 0;
}

static void liberar_volume(volume_t *x)
{
    for (int i = 0; i < CACHE; i++) if (x->cache_[i].dados) kfree(x->cache_[i].dados);
    if (x->ra_dados) kfree(x->ra_dados);
    kfree(x);
}

static int fat_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    volume_t *x = kcalloc(sizeof(volume_t));
    if (!x) return ERRO_SEM_MEMORIA;
    vp = x;
    int r = ler_volume(d, true);
    if (r < 0) { liberar_volume(x); return r; }
    /* D281: comprobación ligera. La entrada 1 de la FAT lleva en el bit 27 la marca de "desmontado
     * limpio" (la que usa Windows). Con escritura se mira y se pone "en uso"; al desmontar se vuelve
     * a poner limpia. Si al montar no estaba limpia, el volumen no se desmontó bien (corte de luz,
     * pendrive arrancado): se anota y se avisa. Como la caché de bloques graba en orden, esta marca
     * llega al disco antes que cualquier cambio posterior. */
    if (!(*opcoes & MONTAR_SO_LEITURA)) {
        uint32_t e1 = fat_ler_entrada(1);
        if (!(e1 & 0x08000000u)) {
            klog("[FAT32] %s: no se desmonto limpio la ultima vez (la marca de la FAT no esta puesta)\n", d->nome);
            disco_avisar(AVISO_INFO, "%s: no se desmontó bien la última vez; conviene comprobarlo (fsck.vfat) en otro equipo.", d->nome);
        }
        if (e1 & 0x08000000u) {
            fat_gravar_entrada(1, e1 & ~0x08000000u);
            if (descarregar() < 0) { liberar_volume(x); return ERRO_ACESSO; }
        }
    }
    *priv = x;
    return 0;
}

static int fat_desmontar(void *priv, bool gravar)
{
    vp = priv;
    int r = 0;
    if (gravar) {
        r = descarregar();
        uint8_t *s = kmalloc(512);
        if (s && disco_ler(v.disco, v.setor_fsinfo, 1, s) == 0 && *(uint32_t *)s == 0x41615252u) {
            if (v.livres == 0xFFFFFFFFu) {   /* no lo sabíamos: cuenta ahora */
                uint32_t n = 0;
                for (uint32_t c = 2; c < v.total_clusters + 2; c++) if (fat_ler_entrada(c) == 0) n++;
                v.livres = n;
            }
            *(uint32_t *)(s + 488) = v.livres;
            *(uint32_t *)(s + 492) = v.dica;
            if (disco_escrever(v.disco, v.setor_fsinfo, 1, s) < 0) r = ERRO_ACESSO;
        }
        if (s) kfree(s);
        fat_gravar_entrada(1, fat_ler_entrada(1) | 0x08000000u);   /* D281: desmontado limpio */
        if (descarregar() < 0) r = ERRO_ACESSO;
    }
    esquecer_cache();
    v.montado = false;
    klog("[FAT32] desmontado%s\n", gravar ? " (tudo gravado)" : " sem gravar");
    liberar_volume(vp);
    vp = NULL;
    return r;
}

bool fat_rotulo_raiz(disco_t *d, char *rotulo)
{
    /* Un volumen provisional, sólo para leer la carpeta raíz. */
    volume_t *guarda, *x = kcalloc(sizeof(volume_t));
    if (!x) return false;
    trava_pegar(&trava_fs);      /* "vp" es de todos: nadie más toca el FAT32 mientras esto duerme en el USB */
    guarda = vp;
    vp = x;
    bool achou = false;
    if (ler_volume(d, false) == 0) {
        cursor_t c;
        cursor_iniciar(&c, v.raiz);
        uint32_t sn, desl;
        uint8_t *e;
        while ((e = cursor_proxima(&c, &sn, &desl)) && e[0]) {
            if (e[0] != LIVRE_E5 && e[11] == ATR_ROTULO) {
                int n = 11;
                while (n > 0 && e[n - 1] == ' ') n--;
                memcpy(rotulo, e, (size_t)n);
                rotulo[n] = 0;
                achou = true;
                break;
            }
        }
    }
    liberar_volume(x);
    vp = guarda;
    trava_soltar(&trava_fs);
    return achou;
}

/* ------------------------------------------------------------ operaciones */

/* El n-ésimo cluster del archivo, continuando desde el último buscado (lecturas y
 * escrituras seguidas no recorren la cadena desde el principio, D040). */
static uint32_t cluster_de(fat_arq_t *f, uint32_t n)
{
    uint32_t c = (f->cache_cl && n >= f->cache_n) ? cluster_n(f->cache_cl, n - f->cache_n) : cluster_n(f->primeiro, n);
    if (c) { f->cache_cl = c; f->cache_n = n; }
    return c;
}

static int fat_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    vp = priv;
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    inf->tipo = (e.atributos & ATR_PASTA) ? TIPO_PASTA : TIPO_ARQUIVO;
    inf->tamanho = (e.atributos & ATR_PASTA) ? 0 : e.tamanho;
    return terminar(0);
}

static int fat_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    vp = priv;
    entrada_t e;
    if (!*rel) { memset(d, 0, sizeof *d); return 0; }   /* la raíz no tiene entrada */
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    d->modificado = fat_para_utc(e.mod_data, e.mod_hora);
    d->criado = fat_para_utc(e.cria_data, e.cria_hora);
    d->acessado = fat_para_utc(e.acesso_data, 0);
    return terminar(0);
}

static int fat_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    vp = priv;
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (!(e.atributos & ATR_PASTA)) return terminar(ERRO_NAO_E_PASTA);
    cursor_t c;
    cursor_iniciar(&c, e.primeiro ? e.primeiro : v.raiz);
    int k = 0;
    while (ler_entrada(&c, &e)) {
        if (e.atributos & (ATR_ROTULO | ATR_OCULTO | ATR_SISTEMA)) continue;
        if (txt_compara(e.nome, ".") == 0 || txt_compara(e.nome, "..") == 0) continue;
        if (k++ != indice) continue;
        txt_copia_n(saida->nome, e.nome, NOME_MAX);
        saida->tipo = (e.atributos & ATR_PASTA) ? TIPO_PASTA : TIPO_ARQUIVO;
        saida->tamanho = (e.atributos & ATR_PASTA) ? 0 : e.tamanho;
        return terminar(1);
    }
    return terminar(0);
}

static int fat_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    vp = priv;
    fat_arq_t *f = arq;
    entrada_t e;
    memset(f, 0, sizeof *f);
    *tamanho = 0;
    if (achar(rel, &e)) {
        if (e.atributos & ATR_PASTA) return terminar(ERRO_E_PASTA);
        if ((modo & ABRIR_ESCREVER) && (e.atributos & ATR_SOMENTE)) return terminar(ERRO_ACESSO);
        f->setor_entrada = e.setor; f->desloc_entrada = e.desloc;
        f->primeiro = e.primeiro; f->tamanho = e.tamanho;
        if (modo & ABRIR_TRUNCAR) {
            if (f->primeiro) liberar_cadeia(f->primeiro);
            f->primeiro = 0; f->tamanho = 0;
            atualizar_entrada(f->setor_entrada, f->desloc_entrada, 0, 0);
        }
        *tamanho = f->tamanho;
        return terminar(0);
    }
    if (!(modo & ABRIR_CRIAR)) return terminar(ERRO_NAO_EXISTE);
    entrada_t pasta;
    const char *nome;
    int r = separar(rel, &pasta, &nome);
    if (r < 0) return terminar(r);
    r = criar_entrada(pasta.primeiro ? pasta.primeiro : v.raiz, nome, ATR_ARQUIVO, 0, 0, &f->setor_entrada, &f->desloc_entrada);
    return terminar(r);
}

/* Cuántos bytes, desde "no_cluster" dentro del cluster "cl", quedan en
 * clusters SEGUIDOS en el disco (cada uno justo después del anterior): es lo que una
 * única lectura o grabación consigue llevar en un comando. Los pendrives
 * formateados de fábrica o los archivos grabados de una vez casi siempre
 * ocupan franjas largas; con clusters de 512 bytes, el límite antiguo (un
 * cluster por petición) costaba un comando USB por cada 512 bytes. "ult" recibe el
 * último cluster de la franja. */
static uint32_t racha(uint32_t cl, uint32_t no_cluster, uint32_t quer, uint32_t *ult)
{
    uint32_t disp = v.bytes_cluster - no_cluster;
    *ult = cl;
    while (disp < quer && disp < RACHA_MAX) {
        uint32_t prox = fat_ler_entrada(*ult);
        if (prox != *ult + 1 || !cluster_valido(prox)) break;
        *ult = prox;
        disp += v.bytes_cluster;
    }
    return disp < quer ? disp : quer;
}

/* Lee "parte" bytes desde el byte "d0" del sector "s0": los extremos que no
 * son sectores enteros pasan por la caché; el medio va directo, de una vez. */
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

/* Lo mismo para grabar: los extremos por lectura-modificación en la caché. */
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

static int fat_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    vp = priv;
    fat_arq_t *f = arq;
    if (pos >= f->tamanho) return 0;
    if (n > f->tamanho - pos) n = f->tamanho - pos;
    uint8_t *destino = buf;
    uint32_t feito = 0;
    uint32_t cl = cluster_de(f, pos / v.bytes_cluster);
    bool ok = true;
    while (feito < n && cl) {
        uint32_t no_cluster = (pos + feito) % v.bytes_cluster, ult;
        uint32_t parte = racha(cl, no_cluster, n - feito, &ult);
        if (ler_bytes(setor_do_cluster(cl) + no_cluster / 512, no_cluster % 512, parte, destino + feito) < 0) { ok = false; break; }
        feito += parte;
        uint32_t ate = no_cluster + parte;                       /* bytes de la franja ya consumidos */
        uint32_t ultimo = cl + (ate + v.bytes_cluster - 1) / v.bytes_cluster - 1;
        if (ate % v.bytes_cluster == 0) {
            uint32_t prox = fat_ler_entrada(ultimo);
            cl = (prox >= FIM_CADEIA || !cluster_valido(prox)) ? 0 : prox;
        } else {
            cl = ultimo;
        }
    }
    if (ok && cl) { f->cache_cl = cl; f->cache_n = (pos + feito) / v.bytes_cluster; }
    return terminar((int)feito);
}

static int fat_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho)
{
    vp = priv;
    fat_arq_t *f = arq;
    if (n == 0) return 0;
    const uint8_t *origem = buf;
    /* Garantiza clusters hasta el fin de lo que se va a escribir. */
    uint32_t precisa = (pos + n + v.bytes_cluster - 1) / v.bytes_cluster;
    if (!f->primeiro) {
        f->primeiro = alocar_cluster(0, pos > 0);
        if (!f->primeiro) return terminar(ERRO_SEM_MEMORIA);
        f->mudou = true;
    }
    uint32_t cl = f->primeiro, qtd = 1;
    if (f->cache_cl && f->cache_n < precisa) { cl = f->cache_cl; qtd = f->cache_n + 1; }
    while (qtd < precisa) {
        uint32_t prox = fat_ler_entrada(cl);
        if (prox >= FIM_CADEIA || !cluster_valido(prox)) {
            /* Cluster nuevo: se pone a cero sólo si va a quedar un hueco antes de lo que se escribirá. */
            prox = alocar_cluster(cl, qtd * v.bytes_cluster < pos);
            if (!prox) return terminar(ERRO_SEM_MEMORIA);
        }
        cl = prox;
        qtd++;
    }
    uint32_t feito = 0;
    cl = cluster_de(f, pos / v.bytes_cluster);
    while (feito < n && cl) {
        uint32_t no_cluster = (pos + feito) % v.bytes_cluster, ult;
        uint32_t parte = racha(cl, no_cluster, n - feito, &ult);
        if (gravar_bytes(setor_do_cluster(cl) + no_cluster / 512, no_cluster % 512, parte, origem + feito) < 0) break;
        feito += parte;
        uint32_t ate = no_cluster + parte;
        uint32_t ultimo = cl + (ate + v.bytes_cluster - 1) / v.bytes_cluster - 1;
        if (ate % v.bytes_cluster == 0 && feito < n) {
            uint32_t prox = fat_ler_entrada(ultimo);
            cl = (prox >= FIM_CADEIA || !cluster_valido(prox)) ? 0 : prox;
        } else {
            cl = ultimo;
        }
    }
    if (pos + feito > f->tamanho) f->tamanho = pos + feito;
    atualizar_entrada(f->setor_entrada, f->desloc_entrada, f->primeiro, f->tamanho);
    *tamanho = f->tamanho;
    return terminar((int)feito);
}

static void fat_fechar(void *priv, void *arq)
{
    vp = priv;
    (void)arq;
    terminar(0);
}

/* Cambia el tamaño: reducir libera los clusters que sobran; aumentar graba ceros. */
static int fat_truncar(void *priv, void *arq, uint32_t tam)
{
    vp = priv;
    fat_arq_t *f = arq;
    if (tam > f->tamanho) {
        uint8_t *z = kcalloc(4096);
        if (!z) return ERRO_SEM_MEMORIA;
        uint32_t t;
        while (f->tamanho < tam) {
            uint32_t parte = tam - f->tamanho > 4096 ? 4096 : tam - f->tamanho;
            int r = fat_escrever(priv, arq, f->tamanho, z, parte, &t);
            if (r <= 0) { kfree(z); return r < 0 ? r : ERRO_ACESSO; }
        }
        kfree(z);
        return 0;
    }
    uint32_t manter = (tam + v.bytes_cluster - 1) / v.bytes_cluster;
    if (manter == 0) {
        f->cache_cl = 0;
        if (f->primeiro) liberar_cadeia(f->primeiro);
        f->primeiro = 0;
    } else {
        f->cache_cl = 0;
        uint32_t ultimo = cluster_n(f->primeiro, manter - 1);
        if (ultimo) {
            uint32_t prox = fat_ler_entrada(ultimo);
            if (cluster_valido(prox) && prox < FIM_CADEIA) liberar_cadeia(prox);
            fat_gravar_entrada(ultimo, MARCA_FIM);
        }
    }
    /* El resto del �ltimo cluster queda a cero, para que un hueco posterior no resucite datos viejos. */
    if (manter && tam % v.bytes_cluster && tam < f->tamanho) {
        uint32_t resto = v.bytes_cluster - tam % v.bytes_cluster, t;
        uint8_t *z = kcalloc(resto);
        if (z) { f->tamanho = tam; fat_escrever(priv, arq, tam, z, resto, &t); kfree(z); }
    }
    f->tamanho = tam;
    atualizar_entrada(f->setor_entrada, f->desloc_entrada, f->primeiro, f->tamanho);
    return terminar(0);
}

static void fat_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    vp = priv;
    /* Con clusters de 512 bytes, bytes_cluster / 1024 daba 0: se divide al final (D121). */
    *total_kb = v.total_clusters * (v.bytes_cluster / 512) / 2;
    if (v.livres == 0xFFFFFFFFu) {   /* sin dato en el FSInfo: cuenta una vez y lo guarda */
        uint32_t n = 0;
        for (uint32_t c = 2; c < v.total_clusters + 2; c++) if (fat_ler_entrada(c) == 0) n++;
        v.livres = n;
    }
    *livre_kb = v.livres * (v.bytes_cluster / 512) / 2;
}

static int fat_criar_pasta(void *priv, const char *rel)
{
    vp = priv;
    entrada_t e, pasta;
    if (achar(rel, &e)) return terminar(ERRO_JA_EXISTE);
    const char *nome;
    int r = separar(rel, &pasta, &nome);
    if (r < 0) return terminar(r);
    uint32_t pai = pasta.primeiro ? pasta.primeiro : v.raiz;
    uint32_t c = alocar_cluster(0, true);
    if (!c) return terminar(ERRO_SEM_MEMORIA);
    /* "." apunta a sí misma; ".." al padre (0 si el padre es la raíz). */
    uint8_t *x = setor(setor_do_cluster(c));
    if (!x) return terminar(ERRO_ACESSO);
    uint16_t d = data_fat(), h = hora_fat();
    for (int k = 0; k < 2; k++) {
        uint8_t *y = x + k * 32;
        memset(y, ' ', 11);
        y[0] = '.';
        if (k == 1) y[1] = '.';
        y[11] = ATR_PASTA;
        uint32_t aponta = k == 0 ? c : (pai == v.raiz ? 0 : pai);
        *(uint16_t *)(y + 20) = (uint16_t)(aponta >> 16);
        *(uint16_t *)(y + 26) = (uint16_t)aponta;
        *(uint16_t *)(y + 14) = h; *(uint16_t *)(y + 16) = d; *(uint16_t *)(y + 22) = h; *(uint16_t *)(y + 24) = d;
    }
    sujar(setor_do_cluster(c));
    r = criar_entrada(pai, nome, ATR_PASTA, c, 0, NULL, NULL);
    if (r < 0) liberar_cadeia(c);
    return terminar(r);
}

static bool pasta_vazia(uint32_t c)
{
    cursor_t cur;
    cursor_iniciar(&cur, c);
    entrada_t e;
    while (ler_entrada(&cur, &e))
        if (txt_compara(e.nome, ".") != 0 && txt_compara(e.nome, "..") != 0 && !(e.atributos & ATR_ROTULO)) return false;
    return true;
}

static int fat_remover_pasta(void *priv, const char *rel)
{
    vp = priv;
    entrada_t e;
    if (!*rel) return terminar(ERRO_ACESSO);
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (!(e.atributos & ATR_PASTA)) return terminar(ERRO_NAO_E_PASTA);
    if (!pasta_vazia(e.primeiro)) return terminar(ERRO_NAO_VAZIA);
    apagar_entrada(&e);
    liberar_cadeia(e.primeiro);
    return terminar(0);
}

static int fat_apagar(void *priv, const char *rel)
{
    vp = priv;
    entrada_t e;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    if (e.atributos & ATR_PASTA) return terminar(ERRO_E_PASTA);
    apagar_entrada(&e);
    if (e.primeiro) liberar_cadeia(e.primeiro);
    return terminar(0);
}

static int fat_renomear(void *priv, const char *de, const char *para)
{
    vp = priv;
    entrada_t e, alvo, pasta;
    if (!*de) return terminar(ERRO_ACESSO);
    if (!achar(de, &e)) return terminar(ERRO_NAO_EXISTE);
    char final[CAMINHO_MAX];
    txt_copia_n(final, para, sizeof final);
    if (achar(para, &alvo)) {
        if (alvo.atributos & ATR_PASTA) {          /* mover adentro de la carpeta */
            size_t n = txt_tamanho(final);
            if (n) final[n++] = '/';
            const char *nome_de = de;
            for (const char *p = de; *p; p++) if (*p == '/') nome_de = p + 1;
            txt_copia_n(final + n, nome_de, sizeof final - n);
            if (achar(final, &alvo)) return terminar(ERRO_JA_EXISTE);
        } else {
            if (e.atributos & ATR_PASTA) return terminar(ERRO_JA_EXISTE);
            apagar_entrada(&alvo);                 /* el archivo de destino se reemplaza */
            if (alvo.primeiro) liberar_cadeia(alvo.primeiro);
        }
    }
    const char *nome;
    int r = separar(final, &pasta, &nome);
    if (r < 0) return terminar(r);
    uint32_t nova_pai = pasta.primeiro ? pasta.primeiro : v.raiz;
    /* Una carpeta no puede ir dentro de sí misma. */
    if (e.atributos & ATR_PASTA) {
        uint32_t c = nova_pai;
        for (int guarda = 0; c != v.raiz && guarda < 64; guarda++) {
            if (c == e.primeiro) return terminar(ERRO_INVALIDO);
            entrada_t pp;
            if (!achar_na_pasta(c, "..", 2, &pp)) break;
            c = pp.primeiro ? pp.primeiro : v.raiz;
        }
    }
    uint32_t sn_novo = 0, desl_novo = 0;
    r = criar_entrada(nova_pai, nome, e.atributos, e.primeiro, e.tamanho, &sn_novo, &desl_novo);
    if (r < 0) return terminar(r);
    /* D341: renombrar no cambia las fechas (mv, y rsync, que escribe en un temporal y lo renombra) */
    uint8_t *xn = setor(sn_novo);
    if (xn) {
        xn += desl_novo;
        *(uint16_t *)(xn + 14) = e.cria_hora; *(uint16_t *)(xn + 16) = e.cria_data;
        *(uint16_t *)(xn + 18) = e.acesso_data;
        *(uint16_t *)(xn + 22) = e.mod_hora; *(uint16_t *)(xn + 24) = e.mod_data;
        sujar(sn_novo);
    }
    apagar_entrada(&e);
    if (e.atributos & ATR_PASTA) {                 /* el ".." de la carpeta movida apunta al nuevo padre */
        entrada_t ponto2;
        if (achar_na_pasta(e.primeiro, "..", 2, &ponto2)) {
            uint8_t *x = setor(ponto2.setor);
            if (x) {
                uint32_t aponta = nova_pai == v.raiz ? 0 : nova_pai;
                *(uint16_t *)(x + ponto2.desloc + 20) = (uint16_t)(aponta >> 16);
                *(uint16_t *)(x + ponto2.desloc + 26) = (uint16_t)aponta;
                sujar(ponto2.setor);
            }
        }
    }
    return terminar(0);
}

/* D341: fecha de modificación (con su hora) y de acceso (solo el día) de la entrada. La raíz no
 * tiene entrada: no se guarda nada y no es un error (rsync fija la de la carpeta de destino). */
static int fat_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{
    vp = priv;
    entrada_t e;
    if (!*rel) return 0;
    if (!achar(rel, &e)) return terminar(ERRO_NAO_EXISTE);
    uint8_t *x = setor(e.setor);
    if (!x) return terminar(ERRO_ACESSO);
    x += e.desloc;
    uint16_t d, h;
    if (mod) { utc_para_fat(mod, &d, &h); *(uint16_t *)(x + 22) = h; *(uint16_t *)(x + 24) = d; }
    if (acesso) { utc_para_fat(acesso, &d, &h); *(uint16_t *)(x + 18) = d; }
    sujar(e.setor);
    return terminar(0);
}

/* El FAT32 vac�a su cach� al final de cada operaci�n (terminar); aqu� se repite por si acaso. */
static int fat_sincronizar(void *priv, void *arq)
{
    vp = priv;
    (void)arq;
    return terminar(0);
}

const sistema_arquivos_t fs_fat32 = {
    .nome = "fat32", .tam_arq = sizeof(fat_arq_t),
    .montar = fat_montar, .desmontar = fat_desmontar, .info = fat_info, .datas = fat_datas, .listar = fat_listar,
    .abrir = fat_abrir, .ler = fat_ler, .escrever = fat_escrever, .truncar = fat_truncar, .fechar = fat_fechar, .sincronizar = fat_sincronizar,
    .criar_pasta = fat_criar_pasta, .remover_pasta = fat_remover_pasta, .apagar = fat_apagar,
    .renomear = fat_renomear, .espaco = fat_espaco,
    .fixar_datas = fat_fixar_datas,   /* D341 */
};

