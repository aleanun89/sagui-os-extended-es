// SPDX-License-Identifier: GPL-2.0-or-later
/* documento.c - el texto de una pestaña del Mico.
 *
 * Gap buffer: el texto queda en un vector con un "agujero" en el lugar de la última
 * edición. Escribir solo llena el agujero; editar en otro lugar mueve el
 * agujero hasta allí (copia el trozo entre los dos puntos, no el archivo
 * entero). En un archivo de 20.000 líneas, escribir cuesta lo mismo que en uno
 * de diez.
 *
 * Índice de líneas: li[l] = posición del comienzo de la línea l (li[nli] = tam + 1,
 * una centinela, para que el fin de cualquier línea sea li[l + 1] - 1). Una
 * edición solo desplaza los números después de ella e inserta o quita los de las líneas
 * nuevas o borradas: nunca relee el texto.
 *
 * Realce: est[l] es el estado del realce al comienzo de la línea l (dentro de un
 * comentario de varias líneas, de una cadena de tres comillas...). Los estados
 * solo valen hasta est_ok; editar la línea l vuelve est_ok a l, y se
 * recalculan bajo demanda cuando la vista dibuja más abajo. Si la edición
 * no cambió el estado al final de la línea, el resto sigue valiendo (es el caso de
 * casi toda tecla), y la vista solo redibuja esa línea.
 *
 * Deshacer: una lista de operaciones (insertó / borró un texto en una posición).
 * Las letras escritas a continuación se juntan en una sola operación; un grupo (reemplazar
 * todos, mover líneas, comentar) se deshace de una vez. "Ilimitado", con un
 * tope de memoria: al pasar de LIMITE_DESFAZER bytes, las más antiguas se van.
 *
 * Archivos: Latin-1 en memoria, como en todo el sistema. Un archivo que vino
 * en UTF-8 vuelve en UTF-8 (D027). Guardar es atómico: escribe "x.nuevo" y solo
 * entonces lo renombra por encima de "x"; si la máquina se cae en el medio, el archivo
 * anterior sigue entero. */
#include "mico.h"

#define LIMITE_DESFAZER (4 * 1024 * 1024)
#define JUNTAR_MS 1500

static void sem_memoria(void)
{
    registrar("mico: sin memoria");
    mensagem("Mico", "No hay memoria suficiente.");
    sair(1);
}

static void *crescer(void *p, int *cap, int precisa, int tam_item)
{
    if (precisa <= *cap) return p;
    int nova = *cap ? *cap : 64;
    while (nova < precisa) nova *= 2;
    void *q = realoca(p, (size_t)nova * (size_t)tam_item);
    if (!q) sem_memoria();
    *cap = nova;
    return q;
}

uint32_t hash_texto(const char *s, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
    return h;
}

/* ------------------------------------------------------------ gap buffer */

static void mover_gap(doc_t *d, int pos)
{
    if (pos < d->g0) {
        int k = d->g0 - pos;
        memmove(d->buf + d->g1 - k, d->buf + pos, (size_t)k);
        d->g0 -= k; d->g1 -= k;
    } else if (pos > d->g0) {
        int k = pos - d->g0;
        memmove(d->buf + d->g0, d->buf + d->g1, (size_t)k);
        d->g0 += k; d->g1 += k;
    }
}

/* Garantiza un agujero de al menos n + 1 bytes (el 1 es el cero de doc_texto). */
static void garantir(doc_t *d, int n)
{
    if (d->g1 - d->g0 > n) return;
    int nova = d->cap * 2;
    if (nova < d->tam + n + 4096) nova = d->tam + n + 4096;
    char *b = aloca((size_t)nova);
    if (!b) sem_memoria();
    int depois = d->cap - d->g1;
    memcpy(b, d->buf, (size_t)d->g0);
    memcpy(b + nova - depois, d->buf + d->g1, (size_t)depois);
    libera(d->buf);
    d->buf = b;
    d->g1 = nova - depois;
    d->cap = nova;
}

char doc_car(doc_t *d, int pos)
{
    if (pos < 0 || pos >= d->tam) return 0;
    return pos < d->g0 ? d->buf[pos] : d->buf[pos + d->g1 - d->g0];
}

void doc_pedaco(doc_t *d, int pos, int n, char *dst)
{
    if (pos < 0) { n += pos; pos = 0; }
    if (pos + n > d->tam) n = d->tam - pos;
    if (n <= 0) return;
    if (pos < d->g0) {
        int a = d->g0 - pos < n ? d->g0 - pos : n;
        memcpy(dst, d->buf + pos, (size_t)a);
        dst += a; pos += a; n -= a;
    }
    if (n > 0) memcpy(dst, d->buf + pos + d->g1 - d->g0, (size_t)n);
}

const char *doc_texto(doc_t *d)
{
    mover_gap(d, d->tam);
    d->buf[d->tam] = 0;   /* entra: el agujero siempre tiene al menos 1 byte */
    return d->buf;
}

/* ------------------------------------------------------------ líneas */

static void reindexar(doc_t *d)
{
    const char *s = doc_texto(d);
    d->nli = 0;
    d->li = crescer(d->li, &d->cap_li, 2, sizeof(int));
    d->li[d->nli++] = 0;
    for (int i = 0; i < d->tam; i++)
        if (s[i] == '\n') {
            d->li = crescer(d->li, &d->cap_li, d->nli + 2, sizeof(int));
            d->li[d->nli++] = i + 1;
        }
    d->li[d->nli] = d->tam + 1;
    d->est = crescer(d->est, &d->cap_est, d->nli + 1, 1);
    d->est[0] = 0;
    d->est_ok = 0;
}

int doc_linha_de(doc_t *d, int pos)
{
    int a = 0, b = d->nli - 1;
    while (a < b) {
        int m = (a + b + 1) / 2;
        if (d->li[m] <= pos) a = m; else b = m - 1;
    }
    return a;
}

int doc_copiar_linha(doc_t *d, int l, char *dst, int max)
{
    int n = doc_tam_linha(d, l);
    if (n > max) n = max;
    doc_pedaco(d, d->li[l], n, dst);
    return n;
}

int doc_estado(doc_t *d, int l)
{
    static char lb[LINHA_MAX];
    if (l >= d->nli) l = d->nli - 1;
    while (d->est_ok < l) {
        int k = d->est_ok;
        int n = doc_copiar_linha(d, k, lb, LINHA_MAX);
        d->est[k + 1] = (uint8_t)realce_linha(d->ling, lb, n, d->est[k], NULL);
        d->est_ok = k + 1;
    }
    return d->est[l];
}

/* Después de editar la línea l (k líneas más o menos): decide hasta dónde
 * los estados del realce siguen valiendo. */
static void realce_apos(doc_t *d, int l, bool linhas_mudaram)
{
    static char lb[LINHA_MAX];
    d->est = crescer(d->est, &d->cap_est, d->nli + 1, 1);
    d->ed_lin = l;
    if (!linhas_mudaram && d->est_ok > l && l + 1 < d->nli) {
        int n = doc_copiar_linha(d, l, lb, LINHA_MAX);
        int novo = realce_linha(d->ling, lb, n, d->est[l], NULL);
        if (novo == d->est[l + 1]) { d->ed_so_linha = true; return; }
        d->est[l + 1] = (uint8_t)novo;
        d->est_ok = l + 1;
        d->ed_so_linha = false;
        return;
    }
    if (d->est_ok > l) d->est_ok = l;
    d->ed_so_linha = !linhas_mudaram;
}

/* ------------------------------------------------------------ deshacer */

static bool aplicando;          /* deshaciendo/rehaciendo: no registra */
static unsigned prox_grupo = 1;
static int prof_grupo;

static void op_tirar_primeira(doc_t *d)
{
    unsigned g = d->ops[0].grupo;
    int k = 1;
    while (g && k < d->feitos && d->ops[k].grupo == g) k++;
    for (int i = 0; i < k; i++) { d->mem_ops -= d->ops[i].n; libera(d->ops[i].t); }
    memmove(d->ops, d->ops + k, (size_t)(d->nops - k) * sizeof(op_t));
    d->nops -= k;
    d->feitos -= k;
    d->salvo = d->salvo >= k ? d->salvo - k : -1;
}

static void registrar_op(doc_t *d, bool apagou, int pos, const char *s, int n)
{
    if (aplicando) return;
    for (int i = d->feitos; i < d->nops; i++) { d->mem_ops -= d->ops[i].n; libera(d->ops[i].t); }
    d->nops = d->feitos;
    if (d->salvo > d->feitos) d->salvo = -1;   /* el estado guardado quedó en la rama descartada */
    unsigned agora = tempo_ms();
    if (!d->grupo && d->feitos > 0 && d->feitos != d->salvo && n == 1 && s[0] != '\n') {
        op_t *o = &d->ops[d->feitos - 1];
        bool perto = !o->grupo && agora - o->tempo < JUNTAR_MS && o->n < 256 && o->apagou == apagou;
        /* Juntar letras escritas, Backspaces y Supr seguidos; una palabra
         * nueva después de un espacio empieza otro paso. */
        bool espaco = o->n > 0 && o->t[o->n - 1] == ' ' && s[0] != ' ';
        if (perto && !espaco) {
            int onde = -1;
            if (!apagou && o->pos + o->n == pos) onde = o->n;          /* escribió después */
            else if (apagou && pos + 1 == o->pos) onde = 0;           /* Backspace */
            else if (apagou && pos == o->pos) onde = o->n;            /* Supr */
            if (onde >= 0) {
                char *t = realoca(o->t, (size_t)o->n + 1);
                if (!t) sem_memoria();
                memmove(t + onde + 1, t + onde, (size_t)(o->n - onde));
                t[onde] = s[0];
                o->t = t;
                o->n++;
                if (apagou && onde == 0) o->pos = pos;
                o->tempo = agora;
                d->mem_ops++;
                return;
            }
        }
    }
    d->ops = crescer(d->ops, &d->cap_ops, d->nops + 1, sizeof(op_t));
    op_t *o = &d->ops[d->nops++];
    o->apagou = apagou;
    o->pos = pos;
    o->n = n;
    o->t = aloca((size_t)n + 1);
    if (!o->t) sem_memoria();
    memcpy(o->t, s, (size_t)n);
    o->cur_antes = d->cursor;
    o->grupo = d->grupo;
    o->tempo = agora;
    d->feitos = d->nops;
    d->mem_ops += n;
    while (d->mem_ops > LIMITE_DESFAZER && d->feitos > 1) op_tirar_primeira(d);
}

void doc_grupo(doc_t *d, bool abrir)
{
    if (abrir) { if (prof_grupo++ == 0) d->grupo = prox_grupo++; }
    else if (prof_grupo > 0 && --prof_grupo == 0) d->grupo = 0;
}

/* ------------------------------------------------------------ editar */

void doc_inserir(doc_t *d, int pos, const char *s, int n)
{
    if (n <= 0) return;
    if (pos < 0) pos = 0;
    if (pos > d->tam) pos = d->tam;
    registrar_op(d, false, pos, s, n);
    int l = doc_linha_de(d, pos), k = 0;
    for (int i = 0; i < n; i++) if (s[i] == '\n') k++;
    d->li = crescer(d->li, &d->cap_li, d->nli + k + 1, sizeof(int));
    if (k) memmove(d->li + l + 1 + k, d->li + l + 1, (size_t)(d->nli - l) * sizeof(int));
    for (int i = l + 1 + k; i <= d->nli + k; i++) d->li[i] += n;
    for (int i = 0, m = 0; i < n; i++) if (s[i] == '\n') d->li[l + 1 + m++] = pos + i + 1;
    d->nli += k;
    garantir(d, n);
    mover_gap(d, pos);
    memcpy(d->buf + d->g0, s, (size_t)n);
    d->g0 += n;
    d->tam += n;
    d->versao++;
    realce_apos(d, l, k > 0);
}

void doc_apagar(doc_t *d, int pos, int n)
{
    if (pos < 0) { n += pos; pos = 0; }
    if (pos + n > d->tam) n = d->tam - pos;
    if (n <= 0) return;
    if (!aplicando) {
        char *t = aloca((size_t)n);
        if (!t) sem_memoria();
        doc_pedaco(d, pos, n, t);
        registrar_op(d, true, pos, t, n);
        libera(t);
    }
    int l1 = doc_linha_de(d, pos), l2 = doc_linha_de(d, pos + n), k = l2 - l1;
    if (k) memmove(d->li + l1 + 1, d->li + l2 + 1, (size_t)(d->nli - l2) * sizeof(int));
    d->nli -= k;
    for (int i = l1 + 1; i <= d->nli; i++) d->li[i] -= n;
    mover_gap(d, pos);
    d->g1 += n;
    d->tam -= n;
    d->versao++;
    realce_apos(d, l1, k > 0);
}

static void aplicar(doc_t *d, op_t *o, bool desfazer)
{
    aplicando = true;
    bool inserir = o->apagou == desfazer;
    if (inserir) doc_inserir(d, o->pos, o->t, o->n);
    else doc_apagar(d, o->pos, o->n);
    aplicando = false;
    d->cursor = desfazer ? o->cur_antes : (inserir ? o->pos + o->n : o->pos);
    d->ancora = -1;
}

bool doc_desfazer(doc_t *d)
{
    if (d->feitos == 0) return false;
    unsigned g = d->ops[d->feitos - 1].grupo;
    int l = d->nli;
    do { d->feitos--; aplicar(d, &d->ops[d->feitos], true); if (d->ed_lin < l) l = d->ed_lin; }
    while (g && d->feitos > 0 && d->ops[d->feitos - 1].grupo == g);
    d->ed_lin = l;
    d->ed_so_linha = false;
    return true;
}

bool doc_refazer(doc_t *d)
{
    if (d->feitos == d->nops) return false;
    unsigned g = d->ops[d->feitos].grupo;
    int l = d->nli;
    do { aplicar(d, &d->ops[d->feitos], false); d->feitos++; if (d->ed_lin < l) l = d->ed_lin; }
    while (g && d->feitos < d->nops && d->ops[d->feitos].grupo == g);
    d->ed_lin = l;
    d->ed_so_linha = false;
    return true;
}

/* ------------------------------------------------------------ crear, leer, guardar */

doc_t *doc_novo(void)
{
    static int prox_id = 1;
    doc_t *d = aloca_zerado(sizeof(doc_t));
    if (!d) sem_memoria();
    d->cap = 4096;
    d->buf = aloca((size_t)d->cap);
    if (!d->buf) sem_memoria();
    d->g0 = 0; d->g1 = d->cap; d->tam = 0;
    d->ancora = -1;
    d->col_desejada = -1;
    d->id = prox_id++;
    d->versao = d->versao_recup = 1;
    reindexar(d);
    return d;
}

void doc_liberar(doc_t *d)
{
    for (int i = 0; i < d->nops; i++) libera(d->ops[i].t);
    libera(d->ops); libera(d->li); libera(d->est); libera(d->buf);
    libera(d);
}

static void limpar_desfazer(doc_t *d)
{
    for (int i = 0; i < d->nops; i++) libera(d->ops[i].t);
    d->nops = d->feitos = d->salvo = 0;
    d->mem_ops = 0;
}

void doc_definir(doc_t *d, const char *s, int n)
{
    if (d->cap < n + 4096) {
        libera(d->buf);
        d->cap = n + 4096;
        d->buf = aloca((size_t)d->cap);
        if (!d->buf) sem_memoria();
    }
    memcpy(d->buf, s, (size_t)n);
    d->g0 = n; d->g1 = d->cap; d->tam = n;
    d->cursor = 0; d->ancora = -1; d->topo = 0; d->esq = 0; d->col_desejada = -1;
    limpar_desfazer(d);
    reindexar(d);
    d->versao++;
}

static uint32_t ult_hash, ult_bruto;   /* del último archivo leído (antes de convertir) */

char *ler_arquivo(const char *c, int *n_saida, int *erro, bool *utf8)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) { *erro = fd; return NULL; }
    int cap = 8192, n = 0, r;
    char *buf = aloca((size_t)cap);
    if (!buf) sem_memoria();
    for (;;) {
        if (n + 4096 > cap) {
            char *b = realoca(buf, (size_t)cap * 2);
            if (!b) sem_memoria();
            buf = b;
            cap *= 2;
        }
        r = ler(fd, buf + n, (unsigned)(cap - n - 1));
        if (r <= 0) break;
        n += r;
    }
    fechar(fd);
    ult_hash = hash_texto(buf, n);
    ult_bruto = (uint32_t)n;
    /* D027: los textos de afuera suelen venir en UTF-8. utf8 = NULL: el archivo es
     * nuestro y ya está en Latin-1 (copias de recuperación), no convierte. */
    int m = utf8 ? (int)utf8_para_latin1(buf, (size_t)n, NULL) : n;
    if (utf8) *utf8 = m < n;
    int k = 0;
    for (int i = 0; i < m; i++) if (buf[i] != '\r') buf[k++] = buf[i];
    buf[k] = 0;
    *n_saida = k;
    return buf;
}

bool doc_carregar(doc_t *d, const char *caminho, int *erro)
{
    int n;
    bool utf8 = false;
    char *b = ler_arquivo(caminho, &n, erro, &utf8);
    if (!b) return false;
    doc_definir(d, b, n);
    libera(b);
    d->utf8 = utf8;
    d->disco_tam = ult_bruto;
    d->disco_hash = ult_hash;
    d->disco_ok = true;
    copia_texto(d->caminho, caminho, sizeof d->caminho);
    const char *s = doc_texto(d);
    d->ling = ling_do_nome(caminho, s, d->tam < 64 ? d->tam : 64);
    d->est_ok = 0;
    d->versao_recup = d->versao;
    return true;
}

/* Escribe "camino.nuevo" y renombra por encima (D075). Si el sistema de archivos
 * no acepta el nombre temporal, escribe directo, como los otros programas. */
int gravar_atomico(const char *caminho, const char *s, int n)
{
    char tmp[CAMINHO_MAX + 8];
    formatar_texto(tmp, sizeof tmp, "%s.nuevo", caminho);
    int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) return fd;
        int r = escrever_tudo(fd, s, (unsigned)n);
        fechar(fd);
        return r == n ? 0 : (r < 0 ? r : ERRO_INVALIDO);
    }
    int r = escrever_tudo(fd, s, (unsigned)n);
    fechar(fd);
    if (r != n) { apagar(tmp); return r < 0 ? r : ERRO_INVALIDO; }
    r = renomear(tmp, caminho);
    if (r == ERRO_JA_EXISTE) { apagar(caminho); r = renomear(tmp, caminho); }
    if (r < 0) apagar(tmp);
    else if (!comeca_com(caminho, pasta_recup)) registrar("mico: escrito %s y renombrado a %s", tmp, caminho);
    return r < 0 ? r : 0;
}

int doc_salvar(doc_t *d)
{
    const char *s = doc_texto(d);
    char *out = NULL;
    int n = d->tam;
    if (d->utf8) {
        int m = 0;
        for (int i = 0; i < d->tam; i++) m += (unsigned char)s[i] >= 0x80 ? 2 : 1;
        out = aloca((size_t)m + 1);
        if (!out) sem_memoria();
        int k = 0;
        for (int i = 0; i < d->tam; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c >= 0x80) { out[k++] = (char)(0xC0 | (c >> 6)); out[k++] = (char)(0x80 | (c & 0x3F)); }
            else out[k++] = (char)c;
        }
        n = k;
    }
    int r = gravar_atomico(d->caminho, out ? out : s, n);
    if (r == 0) {
        d->disco_tam = (uint32_t)n;
        d->disco_hash = hash_texto(out ? out : s, n);
        d->disco_ok = true;
        d->salvo = d->feitos;
        d->versao_recup = d->versao;
    }
    if (out) libera(out);
    return r;
}

void doc_marcar_disco(doc_t *d)
{
    int n, erro;
    char *b = ler_arquivo(d->caminho, &n, &erro, NULL);
    if (!b) return;
    libera(b);
    d->disco_tam = ult_bruto;
    d->disco_hash = ult_hash;
    d->disco_ok = true;
}

const char *doc_nome(doc_t *d)
{
    if (!d->caminho[0]) return "sin nombre";
    const char *b = acha_ultimo_char(d->caminho, '/');
    return b ? b + 1 : d->caminho;
}

/* 0 = igual a lo que se leyó/guardó, 1 = cambió, -1 = desapareció. Solo compara el
 * contenido en archivos de hasta 256 KB (el sistema no guarda la fecha de
 * modificación); en los mayores, solo el tamaño. */
int doc_conferir_disco(doc_t *d)
{
    if (!d->caminho[0] || !d->disco_ok) return 0;
    info_arquivo_t inf;
    if (info_arquivo(d->caminho, &inf) < 0) return -1;
    if (inf.tamanho != d->disco_tam) return 1;
    if (inf.tamanho > 256 * 1024) return 0;
    int fd = abrir(d->caminho, ABRIR_LER);
    if (fd < 0) return 0;
    char *b = aloca(inf.tamanho + 1);
    if (!b) { fechar(fd); return 0; }
    int n = 0, r;
    while (n < (int)inf.tamanho && (r = ler(fd, b + n, inf.tamanho - (unsigned)n)) > 0) n += r;
    fechar(fd);
    uint32_t h = hash_texto(b, n);
    libera(b);
    return h != d->disco_hash ? 1 : 0;
}

/* ------------------------------------------------------------ recuperación */

/* Las copias quedan en /home/.mico si /home existe (la partición persistente,
 * D040: sobreviven a apagar la máquina) y en /tmp/.mico si no. Cada una
 * tiene una línea "MICO 1", el camino original (vacío = sin nombre) y el texto. */
char pasta_recup[64];

void recup_iniciar(void)
{
    info_arquivo_t inf;
    bool home = info_arquivo("/home", &inf) >= 0 && inf.tipo == TIPO_PASTA;
    copia_texto(pasta_recup, home ? "/home/.mico" : "/tmp/.mico", sizeof pasta_recup);
    if (criar_pasta(pasta_recup) < 0 && (info_arquivo(pasta_recup, &inf) < 0 || inf.tipo != TIPO_PASTA)) {
        copia_texto(pasta_recup, "/tmp/.mico", sizeof pasta_recup);
        criar_pasta("/tmp");
        criar_pasta(pasta_recup);
    }
}

static void nome_recup(doc_t *d, char *dst, int max)
{
    if (d->caminho[0]) formatar_texto(dst, (size_t)max, "%s/%x.rec", pasta_recup, hash_texto(d->caminho, (int)tam_texto(d->caminho)));
    else formatar_texto(dst, (size_t)max, "%s/sin-nombre-%d-%d.rec", pasta_recup, meu_pid(), d->id);
}

void recup_gravar(doc_t *d)
{
    char nome[CAMINHO_MAX];
    nome_recup(d, nome, sizeof nome);
    const char *s = doc_texto(d);
    int cab = (int)tam_texto(d->caminho) + 8;
    char *b = aloca((size_t)(cab + d->tam + 1));
    if (!b) return;
    int k = formatar_texto(b, (size_t)cab + 1, "MICO 1\n%s\n", d->caminho);
    memcpy(b + k, s, (size_t)d->tam);
    int r = gravar_atomico(nome, b, k + d->tam);
    libera(b);
    d->versao_recup = d->versao;
    registrar("mico: recuperacion %s (%d bytes, %s)", d->caminho[0] ? d->caminho : "sin nombre", d->tam, r == 0 ? "ok" : nome_erro(r));
}

void recup_apagar(doc_t *d)
{
    char nome[CAMINHO_MAX];
    nome_recup(d, nome, sizeof nome);
    apagar(nome);
}

char *recup_ler(const char *arquivo, char *caminho, int *n)
{
    int erro, m;
    char *b = ler_arquivo(arquivo, &m, &erro, NULL);
    if (!b) return NULL;
    if (!comeca_com(b, "MICO 1\n")) { libera(b); return NULL; }
    char *c = b + 7, *f = acha_char(c, '\n');
    if (!f) { libera(b); return NULL; }
    int k = (int)(f - c);
    if (k >= CAMINHO_MAX) k = CAMINHO_MAX - 1;
    memcpy(caminho, c, (size_t)k);
    caminho[k] = 0;
    int ini = (int)(f + 1 - b);
    memmove(b, b + ini, (size_t)(m - ini + 1));
    *n = m - ini;
    return b;
}

int recup_listar(char caminhos[][CAMINHO_MAX], char arquivos[][80], int max)
{
    int qtd = 0;
    entrada_pasta_t e;
    for (int i = 0; qtd < max && listar(pasta_recup, i, &e) > 0; i++) {
        int n = (int)tam_texto(e.nome);
        if (n < 5 || compara_texto(e.nome + n - 4, ".rec") != 0) continue;
        formatar_texto(arquivos[qtd], 80, "%s/%s", pasta_recup, e.nome);
        int fd = abrir(arquivos[qtd], ABRIR_LER);
        if (fd < 0) continue;
        char cab[CAMINHO_MAX + 8];
        int r = ler(fd, cab, sizeof cab - 1);
        fechar(fd);
        if (r < 8) continue;
        cab[r] = 0;
        if (!comeca_com(cab, "MICO 1\n")) continue;
        char *f = acha_char(cab + 7, '\n');
        if (!f) continue;
        *f = 0;
        copia_texto(caminhos[qtd], cab + 7, CAMINHO_MAX);
        qtd++;
    }
    return qtd;
}
