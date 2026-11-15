// SPDX-License-Identifier: GPL-2.0-or-later
/* particoes.c - la tabla de particiones MBR (primarias, extendida y lógicas), la
 * GPT (lectura aquí, escritura en gpt.c: D266) y el deshacer (D179). Ver particoes.h.
 *
 * Reglas que se cumplen siempre antes de escribir (pt_validar):
 *   - todo dentro del disco, nada en el sector 0, sin solapes entre primarias;
 *   - una sola extendida, y cada lógica (con su EBR delante) dentro de ella, sin
 *     solapes y en orden (la cadena de EBR solo avanza, como pide el kernel);
 *   - un disco GPT o un volumen sin tabla no se tocan (el instalador, que borra el
 *     disco entero, cambia el esquema a propósito).
 * Orden de escritura: primero los EBR y después el MBR (si se corta a medias, el MBR
 * viejo sigue describiendo el disco); los EBR, del último al primero (D213): así cada
 * enlace apunta a un EBR que ya está escrito, y en ningún momento la cadena pierde una
 * lógica que no cambia (el kernel lo exige si hay particiones montadas en el disco).
 * Luego se relee todo y se compara.
 * Deshacer: antes de cada cambio se guardan los sectores que se van a pisar (y cómo
 * quedan) en /tmp/particiones/<disco>.deshacer; deshacer solo restaura si el disco
 * sigue exactamente como lo dejó ese cambio. */
#include "particoes.h"
#include "gpt.h"

/* ------------------------------------------------------------ acceso en bruto */

static inline int chamar_blocos(unsigned op, const char *nome, unsigned a, unsigned b, unsigned c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_DISCO_BLOCOS), "b"(op), "c"(nome), "d"(a), "S"(b), "D"(c) : "memory");
    return r;
}

int blocos_info(const char *nome, info_bloco_t *i) { return chamar_blocos(DBLOCO_INFO, nome, (unsigned)i, 0, 0); }
int blocos_reservar(const char *disco, int conf) { return chamar_blocos(DBLOCO_RESERVAR, disco, (unsigned)conf, 0, 0); }
int blocos_liberar(const char *disco) { return chamar_blocos(DBLOCO_LIBERAR, disco, 0, 0, 0); }
int blocos_reler(const char *disco) { return chamar_blocos(DBLOCO_RELER, disco, 0, 0, 0); }

int blocos_ler(const char *nome, uint32_t lba, uint32_t n, void *buf)
{
    uint8_t *p = buf;
    while (n) {
        uint32_t q = n > DBLOCO_MAX_SETORES ? DBLOCO_MAX_SETORES : n;
        int r = chamar_blocos(DBLOCO_LER, nome, lba, q, (unsigned)p);
        if (r < 0) return r;
        lba += q; n -= q; p += q * 512;
    }
    return 0;
}

int blocos_escrever(const char *nome, uint32_t lba, uint32_t n, const void *buf)
{
    const uint8_t *p = buf;
    while (n) {
        uint32_t q = n > DBLOCO_MAX_SETORES ? DBLOCO_MAX_SETORES : n;
        int r = chamar_blocos(DBLOCO_ESCREVER, nome, lba, q, (unsigned)p);
        if (r < 0) return r;
        lba += q; n -= q; p += q * 512;
    }
    return 0;
}

int blocos_descartar(const char *nome, uint32_t lba, uint32_t n)
{
    return chamar_blocos(DBLOCO_DESCARTAR, nome, lba, n, 0);
}

int blocos_zerar(const char *nome, uint32_t lba, uint32_t n)
{
    static uint8_t zeros[DBLOCO_MAX_SETORES * 512];
    while (n) {
        uint32_t q = n > DBLOCO_MAX_SETORES ? DBLOCO_MAX_SETORES : n;
        int r = chamar_blocos(DBLOCO_ESCREVER, nome, lba, q, (unsigned)zeros);
        if (r < 0) return r;
        lba += q; n -= q;
    }
    return 0;
}

/* ------------------------------------------------------------ utilidades */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void   po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void erro_txt(char *erro, int max, const char *fmt, ...)
{
    if (!erro || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(erro, (size_t)max, fmt, ap);
    va_end(ap);
}

bool pt_tipo_extendida(uint8_t t) { return t == 0x05 || t == 0x0F || t == 0x85; }

const char *pt_nome_tipo(uint8_t t)
{
    switch (t) {
    case 0x00: return "libre";
    case 0x01: return "FAT12";
    case 0x04: case 0x06: case 0x0E: return "FAT16";
    case 0x05: case 0x0F: case 0x85: return "extendida";
    case 0x07: return "NTFS/exFAT";
    case 0x0B: case 0x0C: return "FAT32";
    case 0x82: return "swap de Linux";
    case 0x83: return "Linux";
    case 0xDA: return "SaguiOS (arranque)";
    case 0xEE: return "GPT (protectora)";
    case 0xEF: return "EFI";
    default:   return "otro";
    }
}

void pt_nome_particao(const char *disco, int numero, char *s, int max)
{
    /* Como el kernel (discos.c, nome_particao): usb0p1 y, D265/D270, nvme0p1. */
    if (disco[0] == 'u' || disco[0] == 'n') formatar_texto(s, (size_t)max, "%sp%d", disco, numero);
    else formatar_texto(s, (size_t)max, "%s%d", disco, numero);
}

int pt_extendida(const pt_tabela_t *t)
{
    for (int i = 0; i < 4; i++) if (t->prim[i].tipo && pt_tipo_extendida(t->prim[i].tipo)) return i;
    return -1;
}

pt_part_t *pt_achar(pt_tabela_t *t, int numero)
{
    if (numero >= 1 && numero <= 4) return t->prim[numero - 1].tipo ? &t->prim[numero - 1] : NULL;
    if (numero >= 5 && numero - 5 < t->nlog) return &t->log[numero - 5];
    return NULL;
}

static uint32_t alinhar_cima(uint32_t x) { return (x + PT_MB - 1) / PT_MB * PT_MB; }
static uint32_t alinhar_baixo(uint32_t x) { return x / PT_MB * PT_MB; }

/* ------------------------------------------------------------ lectura */

static bool eh_fat32(const uint8_t *s)
{
    return s[510] == 0x55 && s[511] == 0xAA && memcmp(s + 82, "FAT32   ", 8) == 0 && s[11] == 0 && s[12] == 2;
}

static void ler_gpt(pt_tabela_t *t)
{
    t->ngpt = 0;
    memset(t->prim, 0, sizeof t->prim);   /* la entrada del MBR protector no es una partición */
    gpt_t *g = aloca(sizeof *g);
    if (!g) { t->gpt_mala = true; copia_texto(t->gpt_aviso, "sin memoria", sizeof t->gpt_aviso); return; }
    if (gpt_ler(t->disco, g, t->gpt_aviso, sizeof t->gpt_aviso) < 0) { t->gpt_mala = true; libera(g); return; }
    t->gpt_respaldo = g->origem == GPT_RESPALDO;
    t->gpt_sem_respaldo = g->origem == GPT_PRINCIPAL && !g->respaldo_ok;
    for (int i = 0; i < (int)GPT_ENTRADAS && t->ngpt < PT_MAX_GPT; i++) {
        if (!gpt_usada(g, i)) continue;
        pt_gpt_t *x = &t->gpt[t->ngpt++];
        x->slot = i;
        x->inicio = gpt_inicio(g, i);
        x->setores = gpt_setores_ent(g, i);
        x->numero = gpt_numero_kernel(g, i);
        x->tipo_mbr = gpt_tipo_mbr(gpt_tipo(g, i));
        const char *n = gpt_nome_tipo(gpt_tipo(g, i));
        if (n) copia_texto(x->tipo, n, sizeof x->tipo);
        else gpt_guid_texto(gpt_tipo(g, i), x->tipo, sizeof x->tipo);
        gpt_nome_ent(g, i, x->nome, sizeof x->nome);
        if (i < 4 && x->numero == i + 1 && x->setores)
            t->prim[i] = (pt_part_t){ x->tipo_mbr, false, x->inicio, x->setores, 0 };
    }
    libera(g);
}

/* La cadena de EBR, como la lee el kernel (discos.c, ler_logicas). */
static void ler_logicas(pt_tabela_t *t, const pt_part_t *ext)
{
    uint8_t e[512];
    uint32_t ini = ext->inicio, fim = ext->inicio + ext->setores, rel = 0;
    t->nlog = 0;
    for (int k = 0; k <= PT_MAX_LOG; k++) {
        uint32_t ebr = ini + rel;
        if (ebr < ini || ebr >= fim || blocos_ler(t->disco, ebr, 1, e) < 0 || e[510] != 0x55 || e[511] != 0xAA) break;
        const uint8_t *l = e + 446, *en = e + 462;
        if (l[4] && !pt_tipo_extendida(l[4]) && t->nlog < PT_MAX_LOG) {
            pt_part_t *p = &t->log[t->nlog++];
            p->tipo = l[4];
            p->ativa = l[0] == 0x80;
            p->inicio = ebr + le32(l + 8) * (t->esc ? t->esc : 1);
            p->setores = le32(l + 12) * (t->esc ? t->esc : 1);
            p->ebr = ebr;
        }
        if (!en[4] || !pt_tipo_extendida(en[4])) break;
        uint32_t sig = le32(en + 8) * (t->esc ? t->esc : 1);
        if (sig <= rel) break;
        rel = sig;
    }
}

int pt_ler(const char *disco, pt_tabela_t *t)
{
    memset(t, 0, sizeof *t);
    copia_texto(t->disco, disco, sizeof t->disco);
    info_bloco_t inf;
    int r = blocos_info(disco, &inf);
    if (r < 0) return r;
    if (inf.inicio) return ERRO_INVALIDO;   /* una partición no tiene tabla */
    t->setores = inf.setores;
    t->esc = inf.bloco_fisico == 4096 ? 8 : 1;   /* D283 */
    r = blocos_ler(disco, 0, 1, t->setor0);
    if (r < 0) return r;
    const uint8_t *s = t->setor0;
    if (s[510] != 0x55 || s[511] != 0xAA) {
        uint8_t sb[1024];
        t->esquema = PT_SEM_TABELA;
        if (t->setores > 4 && blocos_ler(disco, 2, 2, sb) == 0 && sb[56] == 0x53 && sb[57] == 0xEF) t->esquema = PT_VOLUME;
        return 0;
    }
    if (eh_fat32(s)) { t->esquema = PT_VOLUME; return 0; }
    t->esquema = PT_MBR;
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = s + 446 + i * 16;
        if (!e[4]) continue;
        if (e[4] == 0xEE) t->esquema = PT_GPT;
        t->prim[i].tipo = e[4];
        t->prim[i].ativa = e[0] == 0x80;
        t->prim[i].inicio = le32(e + 8) * t->esc;
        t->prim[i].setores = le32(e + 12) * t->esc;
    }
    if (t->esquema == PT_GPT) { ler_gpt(t); return 0; }
    int x = pt_extendida(t);
    if (x >= 0) ler_logicas(t, &t->prim[x]);
    return 0;
}

/* ------------------------------------------------------------ validación */

static bool solapa(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) { return a0 < b1 && b0 < a1; }

bool pt_validar(const pt_tabela_t *t, char *erro, int max)
{
    if (t->esquema == PT_GPT) { erro_txt(erro, max, "el disco %s tiene tabla GPT: aquí solo se muestra, no se cambia", t->disco); return false; }
    if (t->esquema == PT_VOLUME) { erro_txt(erro, max, "%s es un volumen sin tabla de particiones: crea antes una tabla nueva", t->disco); return false; }
    int next = 0, ativas = 0;
    for (int i = 0; i < 4; i++) {
        const pt_part_t *p = &t->prim[i];
        if (!p->tipo) continue;
        if (p->tipo == 0xEE) { erro_txt(erro, max, "no se crean tablas GPT"); return false; }
        if (!p->setores || p->inicio < 1 || p->inicio >= t->setores || p->setores > t->setores - p->inicio) {
            erro_txt(erro, max, "la partición %d se sale del disco", i + 1); return false;
        }
        if (pt_tipo_extendida(p->tipo)) { if (next++) { erro_txt(erro, max, "solo puede haber una partición extendida"); return false; } }
        else if (p->ativa) ativas++;
        if (p->ativa && pt_tipo_extendida(p->tipo)) { erro_txt(erro, max, "una extendida no puede ser la activa"); return false; }
        for (int j = i + 1; j < 4; j++) {
            const pt_part_t *q = &t->prim[j];
            if (q->tipo && solapa(p->inicio, p->inicio + p->setores, q->inicio, q->inicio + q->setores)) {
                erro_txt(erro, max, "las particiones %d y %d se solapan", i + 1, j + 1); return false;
            }
        }
    }
    if (ativas > 1) { erro_txt(erro, max, "hay más de una partición activa"); return false; }
    int x = pt_extendida(t);
    if (t->nlog && x < 0) { erro_txt(erro, max, "hay lógicas sin partición extendida"); return false; }
    if (t->nlog > PT_MAX_LOG) { erro_txt(erro, max, "como mucho %d lógicas", PT_MAX_LOG); return false; }
    if (x >= 0) {
        uint32_t ei = t->prim[x].inicio, ef = ei + t->prim[x].setores;
        for (int k = 0; k < t->nlog; k++) {
            const pt_part_t *l = &t->log[k];
            if (!l->tipo || pt_tipo_extendida(l->tipo)) { erro_txt(erro, max, "la lógica %d tiene un tipo no válido", k + 5); return false; }
            if (!l->setores || l->ebr < ei || l->ebr >= l->inicio || l->inicio >= ef || l->setores > ef - l->inicio) {
                erro_txt(erro, max, "la lógica %d no cabe dentro de la extendida", k + 5); return false;
            }
            if (k && l->ebr <= t->log[k - 1].ebr) { erro_txt(erro, max, "las lógicas no están en orden"); return false; }
            for (int j = 0; j < k; j++) {
                const pt_part_t *m = &t->log[j];
                if (solapa(l->ebr, l->inicio + l->setores, m->ebr, m->inicio + m->setores)) {
                    erro_txt(erro, max, "las lógicas %d y %d se solapan", j + 5, k + 5); return false;
                }
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------ huecos libres */

static int ordenar_intervalos(uint32_t (*v)[2], int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && v[j][0] < v[j - 1][0]; j--) {
            uint32_t a = v[j][0], b = v[j][1];
            v[j][0] = v[j - 1][0]; v[j][1] = v[j - 1][1];
            v[j - 1][0] = a; v[j - 1][1] = b;
        }
    return n;
}

/* Huecos entre intervalos ocupados dentro de [ini, fim). */
static int huecos(uint32_t (*occ)[2], int n, uint32_t ini, uint32_t fim, bool logica, pt_livre_t *v, int k, int max)
{
    ordenar_intervalos(occ, n);
    uint32_t pos = ini;
    for (int i = 0; i <= n && k < max; i++) {
        uint32_t ate = i < n ? occ[i][0] : fim;
        if (ate > fim) ate = fim;
        uint32_t a = alinhar_cima(pos), b = alinhar_baixo(ate);
        if (logica) a += PT_MB;   /* el EBR va en el primer MB del hueco */
        if (b > a && b - a >= PT_MB) { v[k].inicio = a; v[k].setores = b - a; v[k].logica = logica; k++; }
        if (i < n && occ[i][1] > pos) pos = occ[i][1];
    }
    return k;
}

int pt_livres(const pt_tabela_t *t, pt_livre_t *v, int max)
{
    if (t->esquema == PT_GPT && !t->gpt_mala) {   /* D266: lo libre entre las entradas, dentro de lo utilizable */
        uint32_t occ[PT_MAX_GPT][2];
        int n = 0;
        for (int i = 0; i < t->ngpt; i++)
            if (t->gpt[i].setores) { occ[n][0] = t->gpt[i].inicio; occ[n][1] = t->gpt[i].inicio + t->gpt[i].setores; n++; }
        return huecos(occ, n, PT_MB, t->setores > 34 ? t->setores - 34 : 0, false, v, 0, max);
    }
    if (t->esquema != PT_MBR && t->esquema != PT_SEM_TABELA) return 0;
    uint32_t occ[4 + PT_MAX_LOG][2];
    int n = 0, k;
    for (int i = 0; i < 4; i++)
        if (t->prim[i].tipo) { occ[n][0] = t->prim[i].inicio; occ[n][1] = t->prim[i].inicio + t->prim[i].setores; n++; }
    k = huecos(occ, n, PT_MB, t->setores, false, v, 0, max);
    int x = pt_extendida(t);
    if (x >= 0) {
        n = 0;
        for (int i = 0; i < t->nlog; i++) { occ[n][0] = t->log[i].ebr; occ[n][1] = t->log[i].inicio + t->log[i].setores; n++; }
        k = huecos(occ, n, t->prim[x].inicio, t->prim[x].inicio + t->prim[x].setores, true, v, k, max);
    }
    return k;
}

/* ------------------------------------------------------------ operaciones en memoria */

static void ordenar_logicas(pt_tabela_t *t)
{
    for (int i = 1; i < t->nlog; i++)
        for (int j = i; j > 0 && t->log[j].ebr < t->log[j - 1].ebr; j--) {
            pt_part_t a = t->log[j]; t->log[j] = t->log[j - 1]; t->log[j - 1] = a;
        }
}

/* Aplica el cambio en una copia; si la tabla resultante no vale, todo queda como estaba. */
static pt_tabela_t copia;

static int confirmar_mudanca(pt_tabela_t *t, int resultado, char *erro, int max)
{
    if (resultado < 0) { *t = copia; return resultado; }
    if (!pt_validar(t, erro, max)) { *t = copia; return ERRO_INVALIDO; }
    return resultado;
}

int pt_criar(pt_tabela_t *t, uint32_t inicio, uint32_t setores, uint8_t tipo, bool logica, char *erro, int max)
{
    copia = *t;
    if (t->esquema == PT_SEM_TABELA) t->esquema = PT_MBR;
    if (!tipo) { erro_txt(erro, max, "falta el tipo"); return ERRO_INVALIDO; }
    inicio = alinhar_cima(inicio);
    setores = alinhar_baixo(setores);
    if (setores < PT_MB) { erro_txt(erro, max, "la partición tiene que tener al menos 1 MB"); *t = copia; return ERRO_INVALIDO; }
    if (!logica) {
        int slot = -1;
        for (int i = 0; i < 4 && slot < 0; i++) if (!t->prim[i].tipo) slot = i;
        if (slot < 0) { erro_txt(erro, max, "ya hay cuatro primarias: crea las nuevas como lógicas dentro de una extendida"); *t = copia; return ERRO_LIMITE; }
        if (pt_tipo_extendida(tipo) && pt_extendida(t) >= 0) { erro_txt(erro, max, "ya hay una partición extendida"); *t = copia; return ERRO_JA_EXISTE; }
        t->prim[slot] = (pt_part_t){ tipo, false, inicio, setores, 0 };
        return confirmar_mudanca(t, slot + 1, erro, max);
    }
    if (pt_extendida(t) < 0) { erro_txt(erro, max, "no hay partición extendida para la lógica"); *t = copia; return ERRO_NAO_EXISTE; }
    if (pt_tipo_extendida(tipo)) { erro_txt(erro, max, "una lógica no puede ser extendida"); *t = copia; return ERRO_INVALIDO; }
    if (t->nlog >= PT_MAX_LOG) { erro_txt(erro, max, "como mucho %d lógicas", PT_MAX_LOG); *t = copia; return ERRO_LIMITE; }
    if (inicio < PT_MB) { *t = copia; return ERRO_INVALIDO; }
    t->log[t->nlog++] = (pt_part_t){ tipo, false, inicio, setores, inicio - PT_MB };
    ordenar_logicas(t);
    int num = -1;
    for (int i = 0; i < t->nlog; i++) if (t->log[i].inicio == inicio) num = i + 5;
    return confirmar_mudanca(t, num, erro, max);
}

int pt_apagar(pt_tabela_t *t, int numero, char *erro, int max)
{
    copia = *t;
    pt_part_t *p = pt_achar(t, numero);
    if (!p) { erro_txt(erro, max, "no existe la partición %d", numero); return ERRO_NAO_EXISTE; }
    if (numero <= 4) {
        if (pt_tipo_extendida(p->tipo)) t->nlog = 0;   /* con la extendida se van sus lógicas */
        memset(p, 0, sizeof *p);
    } else {
        for (int i = numero - 5; i + 1 < t->nlog; i++) t->log[i] = t->log[i + 1];
        t->nlog--;
    }
    return confirmar_mudanca(t, numero, erro, max);
}

int pt_redimensionar(pt_tabela_t *t, int numero, uint32_t setores, char *erro, int max)
{
    copia = *t;
    pt_part_t *p = pt_achar(t, numero);
    if (!p) { erro_txt(erro, max, "no existe la partición %d", numero); return ERRO_NAO_EXISTE; }
    /* El final queda en un límite de 1 MB del disco (así la siguiente sigue alineada). */
    uint32_t fim = alinhar_baixo(p->inicio + setores);
    if (fim <= p->inicio) { erro_txt(erro, max, "el tamaño nuevo es demasiado pequeño"); return ERRO_INVALIDO; }
    if (numero <= 4 && pt_tipo_extendida(p->tipo))
        for (int i = 0; i < t->nlog; i++)
            if (t->log[i].inicio + t->log[i].setores > fim) {
                erro_txt(erro, max, "la extendida no puede quedar más pequeña que sus lógicas (la %d acaba más allá)", i + 5);
                return ERRO_INVALIDO;
            }
    p->setores = fim - p->inicio;
    return confirmar_mudanca(t, numero, erro, max);
}

int pt_ativar(pt_tabela_t *t, int numero, char *erro, int max)
{
    copia = *t;
    if (numero < 1 || numero > 4 || !t->prim[numero - 1].tipo) { erro_txt(erro, max, "solo una primaria puede ser la activa"); return ERRO_INVALIDO; }
    for (int i = 0; i < 4; i++) t->prim[i].ativa = i == numero - 1;
    return confirmar_mudanca(t, numero, erro, max);
}

int pt_mudar_tipo(pt_tabela_t *t, int numero, uint8_t tipo, char *erro, int max)
{
    copia = *t;
    pt_part_t *p = pt_achar(t, numero);
    if (!p) { erro_txt(erro, max, "no existe la partición %d", numero); return ERRO_NAO_EXISTE; }
    if (!tipo || pt_tipo_extendida(tipo) != pt_tipo_extendida(p->tipo)) { erro_txt(erro, max, "ese tipo no vale para esta partición"); return ERRO_INVALIDO; }
    p->tipo = tipo;
    return confirmar_mudanca(t, numero, erro, max);
}

/* ------------------------------------------------------------ escritura */

#define GEOM_C 255
#define GEOM_S 63
static void chs(uint8_t *e, uint32_t lba)
{
    uint32_t c = lba / (GEOM_C * GEOM_S), h = (lba / GEOM_S) % GEOM_C, s = lba % GEOM_S + 1;
    if (c > 1023) { c = 1023; h = 254; s = 63; }
    e[0] = (uint8_t)h; e[1] = (uint8_t)(s | ((c >> 2) & 0xC0)); e[2] = (uint8_t)c;
}

/* D283: las entradas del MBR van en bloques reales; con bloques de 4096, los sectores se dividen entre 8
 * (el alineado a 1 MB de pt_* ya lo garantiza; montar_setores lo comprueba antes). */
static uint32_t esc_montaje = 1;

static void entrada(uint8_t *e, uint8_t tipo, bool ativa, uint32_t inicio_abs, uint32_t rel, uint32_t qtd)
{
    memset(e, 0, 16);
    if (!tipo) return;
    inicio_abs /= esc_montaje; rel /= esc_montaje; qtd /= esc_montaje;
    e[0] = ativa ? 0x80 : 0;
    chs(e + 1, inicio_abs);
    e[4] = tipo;
    chs(e + 5, inicio_abs + qtd - 1);
    po32(e + 8, rel);
    po32(e + 12, qtd);
}

#define MAX_SETORES_TAB (PT_MAX_LOG + 2)
typedef struct { uint32_t lba; uint8_t depois[512]; } setor_novo_t;

/* Los sectores que describen la tabla t: los EBR (por orden) y, al final, el MBR. */
static int montar_setores(const pt_tabela_t *t, setor_novo_t *v)
{
    int n = 0, x = pt_extendida(t);
    esc_montaje = t->esc ? t->esc : 1;
    if (esc_montaje > 1) {   /* todo tiene que caer en un límite de bloque real */
        for (int i = 0; i < 4; i++)
            if (t->prim[i].tipo && ((t->prim[i].inicio | t->prim[i].setores) % esc_montaje)) return -1;
        for (int k = 0; k < t->nlog; k++)
            if ((t->log[k].ebr | t->log[k].inicio | t->log[k].setores) % esc_montaje) return -1;
    }
    if (x >= 0) {
        uint32_t ei = t->prim[x].inicio;
        if (!t->nlog || t->log[0].ebr != ei) {   /* EBR solo de enlace al principio de la extendida */
            memset(v[n].depois, 0, 512);
            v[n].lba = ei;
            if (t->nlog) {
                const pt_part_t *s = &t->log[0];
                entrada(v[n].depois + 462, 0x05, false, s->ebr, s->ebr - ei, s->inicio + s->setores - s->ebr);
            }
            v[n].depois[510] = 0x55; v[n].depois[511] = 0xAA;
            n++;
        }
        for (int k = 0; k < t->nlog; k++) {
            const pt_part_t *l = &t->log[k];
            memset(v[n].depois, 0, 512);
            v[n].lba = l->ebr;
            entrada(v[n].depois + 446, l->tipo, false, l->inicio, l->inicio - l->ebr, l->setores);
            if (k + 1 < t->nlog) {
                const pt_part_t *s = &t->log[k + 1];
                entrada(v[n].depois + 462, 0x05, false, s->ebr, s->ebr - ei, s->inicio + s->setores - s->ebr);
            }
            v[n].depois[510] = 0x55; v[n].depois[511] = 0xAA;
            n++;
        }
    }
    memcpy(v[n].depois, t->setor0, 446);   /* el código de arranque y la firma del disco se conservan */
    for (int i = 0; i < 4; i++) {
        const pt_part_t *p = &t->prim[i];
        entrada(v[n].depois + 446 + i * 16, p->tipo, p->ativa, p->inicio, p->inicio, p->setores);
    }
    v[n].depois[510] = 0x55; v[n].depois[511] = 0xAA;
    v[n].lba = 0;
    return n + 1;
}

/* ---- el archivo de deshacer ---- */
#define DH_MAGICO 0x48444753u   /* "SGDH" */
#define DH_PASSOS 16
typedef struct { uint32_t magico, n, setores_disco, hora; char descricao[48]; } dh_cab_t;
typedef struct { uint32_t lba; uint8_t antes[512], depois[512]; } dh_setor_t;

static void caminho_deshacer(const char *disco, char *s, int max)
{
    formatar_texto(s, (size_t)max, "/tmp/particiones/%s.deshacer", disco);
}

/* Todo el archivo en memoria (como mucho DH_PASSOS pasos de unos 20 KB). */
static uint8_t *ler_deshacer(const char *disco, unsigned *tam)
{
    char c[64];
    caminho_deshacer(disco, c, sizeof c);
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) { *tam = 0; return NULL; }
    char *d = ler_todo_fd(fd, tam);
    fechar(fd);
    return (uint8_t *)d;
}

static int gravar_deshacer(const char *disco, const uint8_t *dados, unsigned tam)
{
    char c[64];
    caminho_deshacer(disco, c, sizeof c);
    criar_pasta("/tmp/particiones");
    if (!tam) { apagar(c); return 0; }
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, dados, tam);
    fechar(fd);
    return r < 0 ? r : 0;
}

/* Los pasos del archivo: devuelve cuántos y el desplazamiento de cada uno. */
static int passos(const uint8_t *d, unsigned tam, unsigned *ini, int max)
{
    int n = 0;
    unsigned p = 0;
    while (p + sizeof(dh_cab_t) <= tam && n < max) {
        const dh_cab_t *c = (const dh_cab_t *)(d + p);
        unsigned t = sizeof(dh_cab_t) + c->n * sizeof(dh_setor_t);
        if (c->magico != DH_MAGICO || c->n > MAX_SETORES_TAB || p + t > tam) break;
        ini[n++] = p;
        p += t;
    }
    return n;
}

int pt_passos_deshacer(const char *disco)
{
    unsigned tam, ini[DH_PASSOS + 1];
    uint8_t *d = ler_deshacer(disco, &tam);
    int n = d ? passos(d, tam, ini, DH_PASSOS + 1) : 0;
    if (d) libera(d);
    return n;
}

static int guardar_passo(const pt_tabela_t *t, const char *descr, const dh_setor_t *v, int n)
{
    unsigned tam, ini[DH_PASSOS + 1];
    uint8_t *d = ler_deshacer(t->disco, &tam);
    int k = d ? passos(d, tam, ini, DH_PASSOS + 1) : 0;
    unsigned desde = 0, usado = k ? tam : 0;
    if (k >= DH_PASSOS) { desde = ini[k - DH_PASSOS + 1]; }   /* se olvida el más antiguo */
    unsigned novo = sizeof(dh_cab_t) + (unsigned)n * sizeof(dh_setor_t);
    uint8_t *saida = aloca(usado - desde + novo);
    if (!saida) { if (d) libera(d); return ERRO_SEM_MEMORIA; }
    if (usado > desde) memcpy(saida, d + desde, usado - desde);
    dh_cab_t *c = (dh_cab_t *)(saida + usado - desde);
    memset(c, 0, sizeof *c);
    c->magico = DH_MAGICO; c->n = (uint32_t)n; c->setores_disco = t->setores; c->hora = hora_utc(NULL);
    copia_texto(c->descricao, descr ? descr : "cambio", sizeof c->descricao);
    memcpy(c + 1, v, (unsigned)n * sizeof(dh_setor_t));
    int r = gravar_deshacer(t->disco, saida, usado - desde + novo);
    libera(saida);
    if (d) libera(d);
    return r;
}

static bool mesma_tabela(const pt_tabela_t *a, const pt_tabela_t *b)
{
    if (a->nlog != b->nlog) return false;
    for (int i = 0; i < 4; i++) {
        const pt_part_t *p = &a->prim[i], *q = &b->prim[i];
        if (p->tipo != q->tipo || (p->tipo && (p->inicio != q->inicio || p->setores != q->setores || p->ativa != q->ativa))) return false;
    }
    for (int i = 0; i < a->nlog; i++) {
        const pt_part_t *p = &a->log[i], *q = &b->log[i];
        if (p->tipo != q->tipo || p->inicio != q->inicio || p->setores != q->setores) return false;
    }
    return true;
}

int pt_gravar(pt_tabela_t *t, const char *descricao, char *erro, int max)
{
    ordenar_logicas(t);
    if (!pt_validar(t, erro, max)) return ERRO_INVALIDO;
    pt_tabela_t atual;
    int r = pt_ler(t->disco, &atual);
    if (r < 0) { erro_txt(erro, max, "no se puede leer %s: %s", t->disco, nome_erro(r)); return r; }
    if (atual.setores != t->setores) { erro_txt(erro, max, "el disco ha cambiado de tamaño"); return ERRO_INVALIDO; }
    if (atual.esquema == PT_GPT && t->esquema != PT_MBR) { erro_txt(erro, max, "el disco es GPT"); return ERRO_ACESSO; }

    static setor_novo_t novos[MAX_SETORES_TAB];
    static dh_setor_t passo[MAX_SETORES_TAB];
    int n = montar_setores(t, novos);
    if (n < 0) { erro_txt(erro, max, "el disco tiene bloques de %u bytes: las particiones tienen que empezar y acabar en múltiplos de ese bloque", t->esc * 512); return ERRO_INVALIDO; }
    for (int i = 0; i < n; i++) {   /* lo que había antes, para deshacer */
        passo[i].lba = novos[i].lba;
        r = blocos_ler(t->disco, novos[i].lba, 1, passo[i].antes);
        if (r < 0) { erro_txt(erro, max, "no se puede leer el sector %u: %s", novos[i].lba, nome_erro(r)); return r; }
        memcpy(passo[i].depois, novos[i].depois, 512);
    }
    /* El deshacer se guarda antes de tocar nada: si se corta a medias, se puede volver. */
    r = guardar_passo(t, descricao, passo, n);
    if (r < 0) { erro_txt(erro, max, "no se puede guardar la copia para deshacer: %s", nome_erro(r)); return r; }
    for (int k = 0; k < n; k++) {   /* EBR primero (del último al primero), MBR al final */
        int i = k < n - 1 ? n - 2 - k : n - 1;
        r = blocos_escrever(t->disco, novos[i].lba, 1, novos[i].depois);
        if (r < 0) {
            erro_txt(erro, max, "error al escribir el sector %u (%s): usa \"deshacer\" para volver a la tabla anterior",
                     novos[i].lba, nome_erro(r));
            return r;
        }
    }
    uint8_t s[512];
    for (int i = 0; i < n; i++)
        if (blocos_ler(t->disco, novos[i].lba, 1, s) < 0 || memcmp(s, novos[i].depois, 512) != 0) {
            erro_txt(erro, max, "la tabla releída no coincide con la escrita (sector %u): usa \"deshacer\"", novos[i].lba);
            return ERRO_INVALIDO;
        }
    pt_tabela_t lida;
    if (pt_ler(t->disco, &lida) < 0 || !mesma_tabela(&lida, t)) {
        erro_txt(erro, max, "la tabla releída no es la esperada: usa \"deshacer\"");
        return ERRO_INVALIDO;
    }
    memcpy(t->setor0, novos[n - 1].depois, 512);
    t->esquema = PT_MBR;
    blocos_reler(t->disco);
    registrar("particiones: %s: tabla escrita (%s), %d sectores", t->disco, descricao ? descricao : "", n);
    return 0;
}

int pt_deshacer(const char *disco, char *msg, int max)
{
    unsigned tam, ini[DH_PASSOS + 1];
    uint8_t *d = ler_deshacer(disco, &tam);
    int k = d ? passos(d, tam, ini, DH_PASSOS + 1) : 0;
    if (!k) { if (d) libera(d); erro_txt(msg, max, "no hay ningún cambio que deshacer en %s", disco); return ERRO_NAO_EXISTE; }
    const dh_cab_t *c = (const dh_cab_t *)(d + ini[k - 1]);
    const dh_setor_t *v = (const dh_setor_t *)(c + 1);
    info_bloco_t inf;
    int r = blocos_info(disco, &inf);
    if (r < 0 || inf.setores != c->setores_disco) {
        libera(d);
        erro_txt(msg, max, "%s no es el mismo disco (otro tamaño): no se deshace", disco);
        return ERRO_INVALIDO;
    }
    uint8_t s[512];
    for (uint32_t i = 0; i < c->n; i++)
        if (blocos_ler(disco, v[i].lba, 1, s) < 0 || memcmp(s, v[i].depois, 512) != 0) {
            libera(d);
            erro_txt(msg, max, "la tabla de %s ha cambiado después de \"%s\": no se deshace (se perdería ese cambio)", disco, c->descricao);
            return ERRO_OCUPADO;
        }
    for (uint32_t k = 0; k < c->n; k++) {   /* como pt_gravar: EBR del último al primero, MBR al final */
        uint32_t i = k < c->n - 1 ? c->n - 2 - k : c->n - 1;
        r = blocos_escrever(disco, v[i].lba, 1, v[i].antes);
        if (r < 0) { libera(d); erro_txt(msg, max, "error al restaurar el sector %u: %s", v[i].lba, nome_erro(r)); return r; }
    }
    for (uint32_t i = 0; i < c->n; i++)
        if (blocos_ler(disco, v[i].lba, 1, s) < 0 || memcmp(s, v[i].antes, 512) != 0) {
            libera(d);
            erro_txt(msg, max, "el sector %u restaurado no coincide", v[i].lba);
            return ERRO_INVALIDO;
        }
    erro_txt(msg, max, "deshecho: \"%s\" (la tabla vuelve a como estaba; los datos de un formateo no vuelven)", c->descricao);
    gravar_deshacer(disco, d, ini[k - 1]);
    libera(d);
    blocos_reler(disco);
    registrar("particiones: %s: deshecho un cambio", disco);
    return 0;
}
