// SPDX-License-Identifier: GPL-3.0-only
/* vfs.c - capa de archivos virtual con la capa escribible en la RAM (H14).
 *
 * En el arranque, el SGFS (sólo lectura) se convierte en un árbol de nodos en memoria:
 * cada carpeta tiene una lista de hijos. Un archivo empieza apuntando a los
 * bytes originales dentro del ramdisk; en la primera escritura obtiene una copia
 * propia en el heap del kernel ("copia al escribir"). Crear, borrar y renombrar
 * tocan sólo el árbol. El ramdisk nunca se altera; al apagar, todo lo que
 * se cambió se pierde (PRD 4.3), como en un sistema "live".
 *
 * A partir de la fase 6, las ramas del árbol pueden pertenecer a otros sistemas de
 * archivos (FAT32, ext2...): son los montajes (montagem.c, D040). Las
 * llamadas de archivos desvían las rutas hacia ellos antes de llegar aquí. */
#include "vfs.h"
#include "console.h"
#include "memoria.h"
#include "sgfs.h"
#include "texto.h"
#include "montagem.h"
#include "permisos.h"
#include "relogio.h"

static no_t raiz;
static uint32_t total_nos;

static no_t *novo_no(no_t *pai, const char *nome, size_t n, uint8_t tipo)
{
    no_t *x = kcalloc(sizeof(no_t));
    if (!x) return NULL;
    if (n >= NOME_MAX) n = NOME_MAX - 1;
    memcpy(x->nome, nome, n);
    x->nome[n] = 0;
    x->tipo = tipo;
    x->pai = pai;
    x->modificado = hora_utc();   /* D080: los del ramdisk quedan con la hora del arranque */
    x->modo = tipo == TIPO_ARQUIVO ? 0644 : 0755;   /* D282: de root; vfs_criar pone los de quien crea */
    /* Inserta en orden alfabético: el ls sale ordenado sin necesidad de ordenar. */
    no_t **p = &pai->filho;
    while (*p && txt_compara((*p)->nome, x->nome) < 0)
        p = &(*p)->irmao;
    x->irmao = *p;
    *p = x;
    total_nos++;
    return x;
}

static no_t *filho_chamado(no_t *pasta, const char *nome, size_t n)
{
    for (no_t *f = pasta->filho; f; f = f->irmao)
        if (txt_tamanho(f->nome) == n && txt_compara_n(f->nome, nome, n) == 0)
            return f;
    return NULL;
}

/* D282: los permisos del ramdisk. Todo es de root (carpetas y programas 0755, el
 * resto 0644) salvo: /home y lo de dentro, de sagui; /tmp, 1777 (todos crean,
 * cada uno borra lo suyo); /sistema, 0711 y lo de dentro 0700 (salvo /sistema/idiomas, D313, legible por todos);
 * la sombra, 0600. "c" es la ruta sin la barra inicial, de "n" bytes. */
static bool prefixo(const char *c, size_t n, const char *p)
{
    size_t k = txt_tamanho(p);
    return n >= k && txt_compara_n(c, p, k) == 0 && (n == k || c[k] == '/');
}

static void meta_inicial(no_t *x, const char *c, size_t n)
{
    if (prefixo(c, n, "home")) { x->uid = UID_SAGUI; x->gid = GID_SAGUI; }
    if (n == 3 && txt_compara_n(c, "tmp", 3) == 0) x->modo = 01777;
    if (n == 7 && txt_compara_n(c, "sistema", 7) == 0) x->modo = 0711;       /* D313: se atraviesa, no se lista el contenido... */
    else if (prefixo(c, n, "sistema") && !prefixo(c, n, "sistema/idiomas")) x->modo = 0700;   /* ...salvo los catalogos de idioma (0755/0644) */
    if (n >= 10 && txt_compara_n(c, "etc/sombra", 10) == 0) x->modo = 0600;
}

void vfs_iniciar(void)
{
    memset(&raiz, 0, sizeof raiz);
    raiz.tipo = TIPO_PASTA;
    raiz.modo = 0755;
    for (uint32_t i = 0; i < sgfs_quantidade(); i++) {
        const sgfs_entrada_t *e = sgfs_entrada(i);
        /* Recorre la ruta creando las carpetas intermedias que falten. */
        no_t *atual = &raiz;
        const char *c = e->caminho;
        while (*c) {
            const char *fim = c;
            while (*fim && *fim != '/') fim++;
            bool ultimo = !*fim;
            no_t *x = filho_chamado(atual, c, (size_t)(fim - c));
            if (!x) {
                uint8_t tipo = ultimo ? (e->tipo == SGFS_PASTA ? TIPO_PASTA :
                                         e->tipo == SGFS_PROGRAMA ? TIPO_PROGRAMA : TIPO_ARQUIVO) : TIPO_PASTA;
                x = novo_no(atual, c, (size_t)(fim - c), tipo);
                if (!x) return;
                meta_inicial(x, e->caminho, (size_t)(fim - e->caminho));
                if (ultimo && tipo != TIPO_PASTA) {
                    x->tamanho = e->tamanho;
                    x->original = e->tamanho ? sgfs_dados(e) : NULL;
                }
            }
            atual = x;
            c = ultimo ? fim : fim + 1;
        }
    }
    klog("[VFS] %u nos montados a partir do SGFS\n", total_nos);
}

no_t *vfs_raiz(void) { return &raiz; }

/* Ruta absoluta ya simplificada (ver caminho_resolver). */
no_t *vfs_achar(const char *caminho)
{
    no_t *atual = &raiz;
    const char *c = caminho;
    while (*c == '/') c++;
    while (*c) {
        const char *fim = c;
        while (*fim && *fim != '/') fim++;
        if (atual->tipo != TIPO_PASTA) return NULL;
        atual = filho_chamado(atual, c, (size_t)(fim - c));
        if (!atual) return NULL;
        c = *fim ? fim + 1 : fim;
    }
    return atual;
}

/* Separa "/a/b/c" en carpeta "/a/b" (encontrada) y nombre "c". */
static int separar(const char *caminho, no_t **pasta, const char **nome)
{
    const char *ultima = NULL;
    for (const char *p = caminho; *p; p++)
        if (*p == '/') ultima = p;
    if (!ultima || !ultima[1]) return ERRO_INVALIDO;   /* raíz o terminado en "/" */
    if (txt_tamanho(ultima + 1) >= NOME_MAX) return ERRO_INVALIDO;
    char pai[CAMINHO_MAX];
    size_t n = (size_t)(ultima - caminho);
    if (n >= sizeof pai) return ERRO_INVALIDO;
    memcpy(pai, caminho, n);
    pai[n] = 0;
    *pasta = vfs_achar(n ? pai : "/");
    if (!*pasta) return ERRO_NAO_EXISTE;
    if ((*pasta)->tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    *nome = ultima + 1;
    return 0;
}

int vfs_criar(const char *caminho, uint8_t tipo, no_t **saida)
{
    no_t *pasta;
    const char *nome;
    int r = separar(caminho, &pasta, &nome);
    if (r < 0) return r;
    if (filho_chamado(pasta, nome, txt_tamanho(nome))) return ERRO_JA_EXISTE;
    no_t *x = novo_no(pasta, nome, txt_tamanho(nome), tipo);
    if (!x) return ERRO_SEM_MEMORIA;
    permisos_dono_novo(tipo == TIPO_PASTA, &x->uid, &x->gid, &x->modo);   /* D282 */
    if (saida) *saida = x;
    return 0;
}

uint32_t vfs_ler(no_t *x, uint32_t desloc, void *destino, uint32_t n)
{
    if (x->tipo == TIPO_PASTA || desloc >= x->tamanho) return 0;
    if (n > x->tamanho - desloc) n = x->tamanho - desloc;
    const uint8_t *fonte = x->dados ? x->dados : x->original;
    if (fonte) memcpy(destino, fonte + desloc, n);
    else memset(destino, 0, n);
    return n;
}

/* Garantiza una copia propia con al menos 'cap' bytes (copia al escribir). */
static bool garantir_capacidade(no_t *x, uint32_t cap)
{
    if (x->dados && x->capacidade >= cap) return true;
    uint32_t nova = x->capacidade ? x->capacidade : 256;
    while (nova < cap) nova *= 2;
    uint8_t *d = kmalloc(nova);
    if (!d) return false;
    const uint8_t *velho = x->dados ? x->dados : x->original;
    if (velho && x->tamanho) memcpy(d, velho, x->tamanho);
    if (x->dados) kfree(x->dados);
    x->dados = d;
    x->capacidade = nova;
    x->original = NULL;
    return true;
}

int vfs_escrever(no_t *x, uint32_t desloc, const void *origem, uint32_t n)
{
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    if (n == 0) return 0;
    if (desloc > 64u * 1024 * 1024 || n > 64u * 1024 * 1024) return ERRO_SEM_MEMORIA;
    if (!garantir_capacidade(x, desloc + n)) return ERRO_SEM_MEMORIA;
    if (desloc > x->tamanho)   /* escribir después del fin: el hueco se llena de ceros */
        memset(x->dados + x->tamanho, 0, desloc - x->tamanho);
    memcpy(x->dados + desloc, origem, n);
    if (desloc + n > x->tamanho) x->tamanho = desloc + n;
    x->modificado = hora_utc();
    return (int)n;
}

void vfs_truncar(no_t *x)
{
    x->tamanho = 0;
    x->original = NULL;
    x->modificado = hora_utc();
}

/* Cambia el tama�o: aumentar rellena con ceros (D116). */
int vfs_redimensionar(no_t *x, uint32_t tam)
{
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    if (tam > 64u * 1024 * 1024) return ERRO_SEM_MEMORIA;
    if (tam > x->tamanho) {
        if (!garantir_capacidade(x, tam)) return ERRO_SEM_MEMORIA;
        memset(x->dados + x->tamanho, 0, tam - x->tamanho);
    } else if (tam < x->tamanho && x->dados) {
        /* un recorte posterior que vuelva a crecer no debe resucitar bytes viejos */
        memset(x->dados + tam, 0, x->tamanho - tam);
    }
    x->tamanho = tam;
    x->modificado = hora_utc();
    return 0;
}

static void destruir_no(no_t *x)
{
    if (x->dados) kfree(x->dados);
    kfree(x);
    total_nos--;
}

/* Saca el nodo de la lista de hijos del padre. */
static void desligar(no_t *x)
{
    no_t **p = &x->pai->filho;
    while (*p && *p != x) p = &(*p)->irmao;
    if (*p) *p = x->irmao;
    x->irmao = NULL;
}

int vfs_apagar(const char *caminho)
{
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    desligar(x);
    /* Archivo abierto por alguien: desaparece de la carpeta ya, pero los datos quedan hasta el
     * último cierre (vfs_soltar). */
    if (x->refs) x->removido = true;
    else destruir_no(x);
    return 0;
}

int vfs_remover_pasta(const char *caminho)
{
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (x == &raiz) return ERRO_ACESSO;
    if (x->tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    if (x->filho) return ERRO_NAO_VAZIA;
    desligar(x);
    destruir_no(x);
    return 0;
}

int vfs_renomear(const char *origem, const char *destino)
{
    no_t *x = vfs_achar(origem);
    if (!x) return ERRO_NAO_EXISTE;
    if (x == &raiz) return ERRO_ACESSO;
    char final[CAMINHO_MAX];
    txt_copia_n(final, destino, sizeof final);
    no_t *d = vfs_achar(destino);
    if (d && d->tipo == TIPO_PASTA) {
        /* "mv a.txt carpeta" = mover adentro de la carpeta, con el mismo nombre. */
        size_t n = txt_tamanho(final);
        if (n + 1 + txt_tamanho(x->nome) + 1 > sizeof final) return ERRO_INVALIDO;
        if (n > 1) final[n++] = '/';
        txt_copia_n(final + n, x->nome, sizeof final - n);
        d = vfs_achar(final);
    }
    if (d == x) return 0;
    if (d) {
        if (d->tipo == TIPO_PASTA) return ERRO_JA_EXISTE;
        vfs_apagar(final);   /* el destino es un archivo: se reemplaza */
    }
    no_t *pasta;
    const char *nome;
    int r = separar(final, &pasta, &nome);
    if (r < 0) return r;
    for (no_t *p = pasta; p; p = p->pai)   /* una carpeta no puede ir dentro de sí misma */
        if (p == x) return ERRO_INVALIDO;
    desligar(x);
    txt_copia_n(x->nome, nome, NOME_MAX);
    x->pai = pasta;
    no_t **p = &pasta->filho;
    while (*p && txt_compara((*p)->nome, x->nome) < 0) p = &(*p)->irmao;
    x->irmao = *p;
    *p = x;
    return 0;
}

int vfs_listar(const char *caminho, int indice, entrada_pasta_t *e)
{
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (x->tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    no_t *f = x->filho;
    for (int i = 0; f && i < indice; i++) f = f->irmao;
    if (!f) return 0;
    txt_copia_n(e->nome, f->nome, NOME_MAX);
    e->tipo = f->tipo;
    e->tamanho = f->tamanho;
    return 1;
}

void vfs_segurar(no_t *x) { x->refs++; }

void vfs_soltar(no_t *x)
{
    if (x->refs) x->refs--;
    if (!x->refs && x->removido) destruir_no(x);
}

/* D194: el men� Inicio pregunta si un programa de /apps est� (la partici�n puede faltar).
 * Como la llamada info_arquivo: dentro de un montaje, la operaci�n info de su sistema. */
bool vfs_existe_arquivo(const char *caminho)
{
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {
        info_arquivo_t inf;
        return *rel && m->fs->info(m->priv, rel, &inf) == 0 && inf.tipo != TIPO_PASTA;
    }
    no_t *x = vfs_achar(caminho);
    return x && x->tipo != TIPO_PASTA;
}

int vfs_ler_inteiro(const char *caminho, uint8_t **dados, uint32_t *tam)
{
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {   /* los programas también pueden ejecutarse desde un volumen montado */
        if (!*rel) return ERRO_E_PASTA;
        arq_montado_t *a = kcalloc(sizeof(arq_montado_t) + m->fs->tam_arq);
        if (!a) return ERRO_SEM_MEMORIA;
        uint32_t t = 0;
        int r = m->fs->abrir(m->priv, rel, ABRIR_LER, a->dados, &t);
        if (r < 0) { kfree(a); return r; }
        uint8_t *buf = t <= 64u * 1024 * 1024 ? kmalloc(t ? t : 1) : NULL;
        int n = buf ? m->fs->ler(m->priv, a->dados, 0, buf, t) : ERRO_SEM_MEMORIA;
        m->fs->fechar(m->priv, a->dados);
        kfree(a);
        if (n < 0) { if (buf) kfree(buf); return n; }
        *tam = (uint32_t)n;
        *dados = buf;
        return 0;
    }
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    uint8_t *buf = kmalloc(x->tamanho ? x->tamanho : 1);
    if (!buf) return ERRO_SEM_MEMORIA;
    *tam = vfs_ler(x, 0, buf, x->tamanho);
    *dados = buf;
    return 0;
}

/* ------------------------------------------------------------ D282: permisos */

static bool em_home(const char *c)
{
    return txt_compara_n(c, "/home", 5) == 0 && (c[5] == 0 || c[5] == '/');
}

/* Dueño, grupo y modo de una ruta. "olhar" = comprobar también que existe en un
 * sistema sin dueños (FAT32...), donde los permisos salen del montaje y son los
 * mismos para todo: para recorrer las carpetas de una ruta no hace falta leer el disco. */
static int meta_de(const char *caminho, meta_t *m, bool olhar)
{
    memset(m, 0, sizeof *m);
    const char *rel;
    montagem_t *mt = montagem_de(caminho, &rel);
    if (mt) {
        int r = montagem_meta(mt, rel, m, olhar);
        if (r < 0) return r;
    } else {
        no_t *x = vfs_achar(caminho);
        if (!x) return ERRO_NAO_EXISTE;
        m->uid = x->uid;
        m->gid = x->gid;
        m->modo = x->modo;
        m->tipo = x->tipo;
        m->com_donos = true;
    }
    /* Hasta la fase 3 hay un solo usuario: lo que es de root en /home se ve como
     * de sagui (lo que se creó antes de D282 en un ext2 era todo de root, y lo
     * que root crea ahí, como «zona» o un programa elevado, sigue siendo de la
     * persona). Lo de antes en un ext2 era 0644: se ve también ejecutable. */
    if (em_home(caminho)) {
        if (m->uid == UID_ROOT) {
            m->uid = UID_SAGUI;
            if (mt && m->com_donos && m->tipo != TIPO_PASTA) m->modo |= (uint16_t)((m->modo & 0444) >> 2);
        }
        if (m->gid == GID_ROOT) m->gid = GID_SAGUI;
    }
    return 0;
}

int vfs_meta(const char *caminho, meta_t *m) { return meta_de(caminho, m, true); }
int vfs_meta_rapida(const char *caminho, meta_t *m) { return meta_de(caminho, m, false); }

int vfs_fixar_meta(const char *caminho, int32_t uid, int32_t gid, int32_t modo)
{
    const char *rel;
    montagem_t *mt = montagem_de(caminho, &rel);
    if (mt) {
        if (!mt->fs->fixar_meta) return ERRO_NAO_SUPORTADO;
        if (mt->opcoes & MONTAR_SO_LEITURA) return ERRO_SO_LEITURA;
        return mt->fs->fixar_meta(mt->priv, rel, uid, gid, modo);
    }
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (uid >= 0) x->uid = (uint32_t)uid;
    if (gid >= 0) x->gid = (uint32_t)gid;
    if (modo >= 0) x->modo = (uint16_t)(modo & MODO_PERMISOS);
    return 0;
}

/* D341: en un volumen, su driver; en el árbol en RAM solo hay fecha de modificación (la de acceso
 * no se guarda: se acepta y se ignora). La raíz de un volumen FAT no tiene entrada: el driver
 * devuelve 0 sin hacer nada. */
int vfs_fixar_datas(const char *caminho, uint32_t modificado, uint32_t acessado)
{
    const char *rel;
    montagem_t *mt = montagem_de(caminho, &rel);
    if (mt) {
        if (mt->opcoes & MONTAR_SO_LEITURA) return ERRO_SO_LEITURA;
        if (!mt->fs->fixar_datas) return ERRO_NAO_SUPORTADO;
        return mt->fs->fixar_datas(mt->priv, rel, modificado, acessado);
    }
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (modificado) x->modificado = modificado;
    return 0;
}

