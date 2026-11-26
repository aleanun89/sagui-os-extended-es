// SPDX-License-Identifier: GPL-2.0-or-later
/* montagem.c - tabla de montajes y /etc/fstab (D040).
 *
 * Hasta la fase 6 había un solo caso especial: el pendrive en /usb. Ahora cualquier
 * volumen (partición FAT32 o ext2/3/4 de un pendrive o de un disco ATA) puede
 * montarse en cualquier carpeta del árbol en RAM, varios a la vez. La
 * ruta se desvía al montaje de prefijo más largo.
 *
 * Reglas de seguridad:
 *   - disco interno (ATA): sólo lectura, salvo que la petición diga "rw";
 *   - ext3/ext4: siempre sólo lectura (el ext2.c decide y avisa);
 *   - el punto de montaje tiene que estar en el árbol en RAM (no dentro de
 *     otro montaje) y no puede ser la raíz. */
#include "montagem.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "trava.h"
#include "vfs.h"

static montagem_t tabela[MAX_MONTAGENS];
static uint32_t geracao = 1;

montagem_t *montagem_indice(int i) { return (i >= 0 && i < MAX_MONTAGENS && tabela[i].usado) ? &tabela[i] : NULL; }

/* D379: el montaje de la raíz ("/"), si la raíz está en un disco. Sin él (lo de
 * siempre) el árbol en RAM es la raíz y esto vale NULL. */
static montagem_t *montagem_raiz;
static bool desmontando_tudo;   /* solo al apagar se puede soltar la raíz */

montagem_t *montagem_de(const char *caminho, const char **rel)
{
    montagem_t *melhor = NULL;
    for (int i = 0; i < MAX_MONTAGENS; i++) {
        montagem_t *m = &tabela[i];
        if (!m->usado || m == montagem_raiz || txt_compara_n(caminho, m->ponto, m->tam_ponto) != 0) continue;
        char c = caminho[m->tam_ponto];
        if (c != 0 && c != '/') continue;                 /* "/usbx" no es /usb */
        if (!melhor || m->tam_ponto > melhor->tam_ponto) melhor = m;
    }
    if (melhor && rel) {
        const char *r = caminho + melhor->tam_ponto;
        *rel = *r ? r + 1 : r;
    }
    /* D379: lo que no cae en otro montaje es de la raíz en disco (toda ruta absoluta). */
    if (!melhor && montagem_raiz && caminho[0] == '/') {
        melhor = montagem_raiz;
        if (rel) {
            const char *r = caminho;
            while (*r == '/') r++;
            *rel = r;
        }
    }
    return melhor;
}

montagem_t *montagem_da_raiz(void) { return montagem_raiz; }

bool montagem_viva(const arq_montado_t *a) { return a->m->usado && a->m->geracao == a->geracao; }

bool montagem_abaixo(const char *caminho)
{
    size_t n = txt_tamanho(caminho);
    if (n == 1) n = 0;   /* "/" */
    for (int i = 0; i < MAX_MONTAGENS; i++) {
        montagem_t *m = &tabela[i];
        if (m == montagem_raiz) continue;   /* D379: la raíz está "debajo" de todo; no cuenta */
        if (m->usado && txt_compara_n(m->ponto, caminho, n) == 0 && (m->ponto[n] == 0 || m->ponto[n] == '/')) return true;
    }
    return false;
}

static const sistema_arquivos_t *fs_do_tipo(const char *tipo)
{
    if (txt_compara(tipo, "fat32") == 0) return &fs_fat32;
    if (txt_compara(tipo, "exfat") == 0) return &fs_exfat;       /* D325 */
    if (txt_compara(tipo, "xfs") == 0) return &fs_xfs;           /* D446: solo lectura */
    if (txt_comeca_com(tipo, "ext")) return &fs_ext2;
    if (txt_compara(tipo, "iso9660") == 0) return &fs_iso9660;   /* D207: CD/DVD */
    if (txt_compara(tipo, "udf") == 0) return &fs_udf;           /* D233: DVD y CD grabados */
    if (txt_compara(tipo, "cdda") == 0) return &fs_cdda;         /* D233: CD de audio (pistas en WAV) */
    if (txt_compara(tipo, "ramfs") == 0) return &fs_ramfs;       /* D379: una carpeta del árbol en RAM */
    if (txt_compara(tipo, "sobreposicao") == 0) return &fs_sobreposicao;   /* D379: la raíz de la live */
    if (txt_compara(tipo, "serie") == 0) return &fs_serie;                 /* v8.5.4.2: su capa inferior */
    return NULL;
}

/* D233: un CD con UDF y un ISO 9660 a la vez (UDF bridge) se puede montar de las
 * dos formas; el resto de volúmenes, solo con el sistema que se reconoció. */
static bool tipo_valido(disco_t *d, const sistema_arquivos_t *fs)
{
    if (fs == fs_do_tipo(d->tipo_fs)) return true;
    if (fs == &fs_sobreposicao) return txt_comeca_com(d->tipo_fs, "ext");   /* D379: un ext2 como capa inferior */
    return d->cd && ((fs == &fs_iso9660 && d->tem_iso) || (fs == &fs_udf && d->tem_udf));
}


/* ------------------------------------------------------------ bloqueo de los sistemas de archivos (D095)
 *
 * Los drivers FAT32 y ext2 guardan el volumen en variables estáticas y no
 * se escribieron para dos operaciones a la vez. Antes el kernel nunca
 * cedía la CPU en medio de una llamada al sistema, así que eso nunca pasaba;
 * ahora una lectura del pendrive duerme esperando al DMA y otro proceso puede
 * llamar al sistema de archivos. Cada montaje recibe una copia de la tabla de
 * operaciones cuyas funciones toman un bloqueo global antes de entrar en el driver.
 * (Un solo bloqueo, y no uno por volumen, porque el estado estático es de todos
 * los volúmenes del mismo tipo.) */
trava_t trava_fs;

typedef struct {
    sistema_arquivos_t tabela;          /* lo que ve el resto del kernel (m->fs) */
    const sistema_arquivos_t *orig;     /* el driver de verdad */
    void *priv;                         /* el volumen del driver */
    bool  ativo;                        /* falso después de desmontar: las operaciones atrasadas se rechazan */
    bool  sem_trava;                    /* D485: el puente (espera a un proceso de usuario): sin trava_fs */
} envolto_t;

static envolto_t envoltos[MAX_MONTAGENS];

#define E ((envolto_t *)priv)
#define ENTRAR() do { if (!E->sem_trava) trava_pegar(&trava_fs); } while (0)
#define SAIR()   do { if (!E->sem_trava) trava_soltar(&trava_fs); } while (0)

static int t_info(void *priv, const char *rel, info_arquivo_t *inf)
{ ENTRAR(); int r = E->ativo ? E->orig->info(E->priv, rel, inf) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_datas(void *priv, const char *rel, datas_arquivo_t *d)
{ ENTRAR(); int r = E->ativo ? E->orig->datas(E->priv, rel, d) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_listar(void *priv, const char *rel, int i, entrada_pasta_t *e)
{ ENTRAR(); int r = E->ativo ? E->orig->listar(E->priv, rel, i, e) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tam)
{ ENTRAR(); int r = E->ativo ? E->orig->abrir(E->priv, rel, modo, arq, tam) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{ ENTRAR(); int r = E->ativo ? E->orig->ler(E->priv, arq, pos, buf, n) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{ ENTRAR(); int r = E->ativo ? E->orig->escrever(E->priv, arq, pos, buf, n, tam) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_truncar(void *priv, void *arq, uint32_t tam)
{ ENTRAR(); int r = E->ativo ? E->orig->truncar(E->priv, arq, tam) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_sincronizar(void *priv, void *arq)
{ ENTRAR(); int r = E->ativo ? E->orig->sincronizar(E->priv, arq) : ERRO_NAO_EXISTE; SAIR(); return r; }
/* Ctrl+C cierra los archivos del proceso dentro de la IRQ del teclado: ahí no se
 * puede esperar ni dormir. Si el bloqueo está tomado por otro, el cierre se
 * omite (el FAT32 no guarda nada pendiente después de cada operación). */
static void t_fechar(void *priv, void *arq)
{
    if (E->sem_trava) { if (E->ativo) E->orig->fechar(E->priv, arq); return; }   /* D485: el puente no espera al cerrar */
    if (contexto_irq() ? !trava_tentar(&trava_fs) : (trava_pegar(&trava_fs), false)) return;
    if (E->ativo) E->orig->fechar(E->priv, arq);
    SAIR();
}
static int t_criar_pasta(void *priv, const char *rel)
{ ENTRAR(); int r = E->ativo ? E->orig->criar_pasta(E->priv, rel) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_remover_pasta(void *priv, const char *rel)
{ ENTRAR(); int r = E->ativo ? E->orig->remover_pasta(E->priv, rel) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_apagar(void *priv, const char *rel)
{ ENTRAR(); int r = E->ativo ? E->orig->apagar(E->priv, rel) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_renomear(void *priv, const char *de, const char *para)
{ ENTRAR(); int r = E->ativo ? E->orig->renomear(E->priv, de, para) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_meta(void *priv, const char *rel, meta_t *m)
{ ENTRAR(); int r = E->ativo ? E->orig->meta(E->priv, rel, m) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_fixar_meta(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo)
{ ENTRAR(); int r = E->ativo ? E->orig->fixar_meta(E->priv, rel, uid, gid, modo) : ERRO_NAO_EXISTE; SAIR(); return r; }
static int t_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{ ENTRAR(); int r = E->ativo ? E->orig->fixar_datas(E->priv, rel, mod, acesso) : ERRO_NAO_EXISTE; SAIR(); return r; }
static void t_espaco(void *priv, uint32_t *total, uint32_t *livre)
{ ENTRAR(); if (E->ativo) E->orig->espaco(E->priv, total, livre); SAIR(); }
static int t_desmontar(void *priv, bool gravar)
{
    ENTRAR();
    int r = 0;
    if (E->ativo) { r = E->orig->desmontar(E->priv, gravar); E->ativo = false; }
    SAIR();
    return r;
}
#undef E

/* Prepara el envoltorio del montaje "i" alrededor del volumen "priv" del driver. */
static void envolver(int i, const sistema_arquivos_t *fs, void *priv)
{
    envolto_t *e = &envoltos[i];
    e->orig = fs;
    e->priv = priv;
    e->ativo = true;
    e->sem_trava = false;
    e->tabela = *fs;
    e->tabela.desmontar = t_desmontar;
    e->tabela.info = t_info;
    e->tabela.listar = t_listar;
    e->tabela.abrir = t_abrir;
    e->tabela.ler = t_ler;
    e->tabela.escrever = t_escrever;
    e->tabela.truncar = t_truncar;
    e->tabela.fechar = t_fechar;
    e->tabela.sincronizar = fs->sincronizar ? t_sincronizar : NULL;
    e->tabela.criar_pasta = t_criar_pasta;
    e->tabela.remover_pasta = t_remover_pasta;
    e->tabela.apagar = t_apagar;
    e->tabela.renomear = t_renomear;
    e->tabela.espaco = fs->espaco ? t_espaco : NULL;
    e->tabela.datas = fs->datas ? t_datas : NULL;
    e->tabela.meta = fs->meta ? t_meta : NULL;
    e->tabela.fixar_meta = fs->fixar_meta ? t_fixar_meta : NULL;
    e->tabela.fixar_datas = fs->fixar_datas ? t_fixar_datas : NULL;   /* D341 */

}

/* D379: con la raíz en disco, el punto de montaje es una carpeta de ese disco. Si
 * falta y la raíz se puede escribir, se crea (como «mkdir -p»); si no, el montaje
 * vale igual (el desvío es por la ruta), solo que la carpeta no sale en el listado. */
static bool criar_ponto_na_raiz(const char *ponto)
{
    montagem_t *r = montagem_raiz;
    if (r->opcoes & MONTAR_SO_LEITURA) return false;
    char parcial[CAMINHO_MAX];
    for (size_t i = 1; ; i++) {
        if (ponto[i] != '/' && ponto[i] != 0) continue;
        memcpy(parcial, ponto, i);
        parcial[i] = 0;
        info_arquivo_t inf;
        bool criou = false;
        if (r->fs->info(r->priv, parcial + 1, &inf) < 0) {
            if (r->fs->criar_pasta(r->priv, parcial + 1) < 0) return false;
            criou = true;
        }
        if (!ponto[i]) return criou;   /* D379 (M1): si se creó, se quita al desmontar (como en la RAM) */
    }
}

/* Crea las carpetas que falten en la ruta (en la RAM). Devuelve si creó la última. */
static int criar_ponto(const char *ponto, bool *criou)
{
    char parcial[CAMINHO_MAX];
    *criou = false;
    if (montagem_raiz) { *criou = criar_ponto_na_raiz(ponto); return 0; }
    for (size_t i = 1; ; i++) {
        if (ponto[i] != '/' && ponto[i] != 0) continue;
        memcpy(parcial, ponto, i);
        parcial[i] = 0;
        no_t *x = vfs_achar(parcial);
        if (!x) {
            int r = vfs_criar(parcial, TIPO_PASTA, NULL);
            if (r < 0) return r;
            if (!ponto[i]) *criou = true;
        } else if (x->tipo != TIPO_PASTA) {
            return ERRO_NAO_E_PASTA;
        }
        if (!ponto[i]) return 0;
    }
}

/* D282: /tmp y /logs son de todos (cada uno escribe lo suyo); /home, de sagui. */
static bool ponto_comum(const char *ponto)
{
    return txt_compara(ponto, "/tmp") == 0 || txt_compara(ponto, "/logs") == 0;
}

int montagem_meta(montagem_t *m, const char *rel, meta_t *s, bool olhar)
{
    memset(s, 0, sizeof *s);
    if (m->fs->meta) {
        int r = m->fs->meta(m->priv, rel, s);
        if (r < 0) return r;
        s->com_donos = true;
    } else {
        /* FAT32, ISO 9660, UDF, CD de audio: todo con los permisos del montaje. */
        if (olhar && *rel) {
            info_arquivo_t inf;
            int r = m->fs->info(m->priv, rel, &inf);
            if (r < 0) return r;
            s->tipo = (uint8_t)inf.tipo;
        } else {
            s->tipo = TIPO_PASTA;
        }
        s->uid = m->uid;
        s->gid = m->gid;
        s->modo = (uint16_t)(0777 & ~m->umask);
    }
    /* D485: en un montaje sin ejecución los ficheros no llevan la «x» (las carpetas sí: es entrar). */
    if ((m->opcoes & MONTAR_SEM_EXEC) && *rel && s->tipo != TIPO_PASTA) s->modo &= (uint16_t)~0111u;
    if (!*rel) {   /* la carpeta del montaje */
        s->tipo = TIPO_PASTA;
        if (txt_compara(m->ponto, "/home") == 0) {   /* D371: de root cuando cada cuenta tiene su carpeta */
            bool r = vfs_home_e_de_root();
            s->uid = r ? UID_ROOT : UID_SAGUI; s->gid = r ? GID_ROOT : GID_SAGUI; s->modo = 0755;
        }
        else if (ponto_comum(m->ponto) && s->com_donos) { s->uid = UID_ROOT; s->gid = GID_ROOT; s->modo = 01777; }
    }
    return 0;
}

int montagem_montar(disco_t *d, const char *ponto, const char *tipo, uint32_t opcoes)
{
    return montagem_montar_dono(d, ponto, tipo, opcoes & ~(uint32_t)(MONTAR_DONO | MONTAR_RAIZ_INTERNO), 0);
}

int montagem_montar_dono(disco_t *d, const char *ponto, const char *tipo, uint32_t opcoes, uint32_t dono)
{
    if (!d->presente) return ERRO_NAO_EXISTE;
    if (!d->tipo_fs[0]) return ERRO_FORMATO;
    if (d->montagem) return ERRO_OCUPADO;
    if (disco_reservado(d)) {   /* D179: alguien cambia la tabla o formatea este disco */
        klog("[MONTAGEM] %s: disco reservado (particiones o instalador), no se monta\n", d->nome);
        return ERRO_OCUPADO;
    }
    bool e_raiz = (opcoes & MONTAR_RAIZ_INTERNO) != 0;   /* D379: solo raiz.c monta "/" */
    opcoes &= ~(uint32_t)MONTAR_RAIZ_INTERNO;
    if (ponto[0] != '/' || txt_tamanho(ponto) >= CAMINHO_MAX) return ERRO_INVALIDO;
    if (e_raiz ? (ponto[1] || montagem_raiz) : !ponto[1]) return ERRO_INVALIDO;
    /* Ya montado ahí, o dentro de otro montaje. D379: dentro de la raíz en disco sí se puede. */
    montagem_t *dentro = montagem_de(ponto, NULL);
    if (dentro && dentro != montagem_raiz) return ERRO_OCUPADO;
    if (!tipo || txt_compara(tipo, "auto") == 0) tipo = d->tipo_fs;
    const sistema_arquivos_t *fs = fs_do_tipo(tipo);
    if (!fs || !tipo_valido(d, fs)) return ERRO_FORMATO;
    int livre = -1;
    for (int i = 0; i < MAX_MONTAGENS && livre < 0; i++) if (!tabela[i].usado) livre = i;
    if (livre < 0) return ERRO_LIMITE;

    /* Disco interno: sólo graba con pedido explícito (D040). */
    disco_t *raiz = disco_raiz(d);
    if (raiz->interno && !(opcoes & MONTAR_LEITURA_ESCRITA)) opcoes |= MONTAR_SO_LEITURA;
    if (opcoes & MONTAR_SO_LEITURA) opcoes &= ~(uint32_t)MONTAR_LEITURA_ESCRITA;

    bool criou;
    int r = criar_ponto(ponto, &criou);
    if (r < 0) return r;
    void *priv = NULL;
    d->rw_autorizado = !(opcoes & MONTAR_SO_LEITURA);
    trava_pegar(&trava_fs);
    r = fs->montar(d, &opcoes, &priv);
    trava_soltar(&trava_fs);
    d->rw_autorizado = r >= 0 && !(opcoes & MONTAR_SO_LEITURA);
    if (fs == &fs_sobreposicao) d->rw_autorizado = false;   /* D379: la capa inferior de la live nunca se escribe */
    if (r < 0) {
        if (criou && montagem_raiz) montagem_raiz->fs->remover_pasta(montagem_raiz->priv, ponto + 1);   /* D379 */
        else if (criou) vfs_remover_pasta(ponto);
        klog("[MONTAGEM] %s em %s falhou (%d)\n", d->nome, ponto, r);
        return r;
    }
    montagem_t *m = &tabela[livre];
    memset(m, 0, sizeof *m);
    m->usado = true;
    txt_copia_n(m->ponto, ponto, sizeof m->ponto);
    m->tam_ponto = (uint32_t)txt_tamanho(m->ponto);
    m->disco = d;
    envolver(livre, fs, priv);
    m->fs = &envoltos[livre].tabela;
    m->priv = &envoltos[livre];
    m->opcoes = opcoes;
    m->geracao = ++geracao;
    m->criou_ponto = criou;
    /* D282: el dueño de lo que hay en un sistema sin dueños. Por defecto, de sagui
     * (los pendrives y el /home FAT32); /tmp y /logs, de todos. */
    if (opcoes & MONTAR_DONO) { m->uid = dono & 0xFFFF; m->gid = dono >> 16; }
    else if (ponto_comum(ponto)) { m->uid = UID_ROOT; m->gid = GID_ROOT; }
    else { m->uid = UID_SAGUI; m->gid = GID_SAGUI; }
    m->umask = (opcoes & MONTAR_COM_UMASK) ? MONTAR_UMASK(opcoes) : ponto_comum(ponto) ? 0 : UMASK_PADRAO;
    txt_copia_n(m->tipo, d->cd ? fs->nome : d->tipo_fs, sizeof m->tipo);   /* D233: el que se usó */
    if (fs == &fs_sobreposicao) txt_copia_n(m->tipo, "live", sizeof m->tipo);   /* D379: la sobreposición de la live */
    d->montagem = m;
    if (e_raiz) {   /* D379: dueños de verdad (ext2), sin máscara de montaje */
        m->uid = UID_ROOT; m->gid = GID_ROOT; m->umask = 022;
        montagem_raiz = m;
    }
    bool so_leitura = (opcoes & MONTAR_SO_LEITURA) != 0;
    klog("[MONTAGEM] %s (%s) montado em %s, %s\n", d->nome, m->tipo, ponto, so_leitura ? "so leitura" : "leitura e escrita");
    if (txt_compara(ponto, "/logs") == 0 && !so_leitura) logs_disco_gravar("montaje");   /* D120 */
    if (raiz->removivel && !raiz->de_boot)
        disco_avisar(AVISO_MONTADO, so_leitura ? "Pendrive montado en %s (solo lectura)" : "Pendrive montado en %s", ponto);
    return 0;
}

/* D485: ver montagem.h. Lo usa el puente (ponte.c) para /red y para cada unidad de red. */
montagem_t *montagem_montar_virtual(disco_t *d, const char *ponto, const sistema_arquivos_t *fs, void *priv,
                                    uint32_t opcoes, uint32_t uid, uint32_t gid, uint32_t umask, int *erro)
{
    *erro = ERRO_INVALIDO;
    if (ponto[0] != '/' || !ponto[1] || txt_tamanho(ponto) >= CAMINHO_MAX) return NULL;
    for (int i = 0; i < MAX_MONTAGENS; i++)
        if (tabela[i].usado && txt_compara(tabela[i].ponto, ponto) == 0) { *erro = ERRO_JA_EXISTE; return NULL; }
    int livre = -1;
    for (int i = 0; i < MAX_MONTAGENS && livre < 0; i++) if (!tabela[i].usado) livre = i;
    if (livre < 0) { *erro = ERRO_LIMITE; return NULL; }
    montagem_t *m = &tabela[livre];
    memset(m, 0, sizeof *m);
    m->usado = true;
    txt_copia_n(m->ponto, ponto, sizeof m->ponto);
    m->tam_ponto = (uint32_t)txt_tamanho(m->ponto);
    m->disco = d;
    envolver(livre, fs, priv);
    envoltos[livre].sem_trava = true;
    m->fs = &envoltos[livre].tabela;
    m->priv = &envoltos[livre];
    m->opcoes = opcoes;
    m->geracao = ++geracao;
    m->uid = uid;
    m->gid = gid;
    m->umask = umask;
    txt_copia_n(m->tipo, fs->nome, sizeof m->tipo);
    d->montagem = m;
    *erro = 0;
    klog("[MONTAGEM] %s (%s) montado em %s\n", d->nome, m->tipo, ponto);
    return m;
}

bool montagem_sem_exec(const char *caminho)
{
    montagem_t *m = montagem_de(caminho, NULL);
    return m && (m->opcoes & MONTAR_SEM_EXEC);
}

int montagem_desmontar(montagem_t *m, bool gravar)
{
    if (!m->usado) return 0;
    /* D379: la raíz en disco solo se suelta al apagar o si el disco ya no está. */
    if (m == montagem_raiz && !desmontando_tudo && m->disco->presente) return ERRO_OCUPADO;
    /* Nadie puede quedarse con la carpeta actual dentro del volumen. */
    for (int i = 0; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado != P_LIVRE && p->estado != P_ZUMBI && txt_compara_n(p->pasta, m->ponto, m->tam_ponto) == 0 &&
            (p->pasta[m->tam_ponto] == 0 || p->pasta[m->tam_ponto] == '/'))
            txt_copia_n(p->pasta, "/", sizeof p->pasta);
    }
    /* Sólo lectura: nada que grabar (ni el FSInfo del FAT32). */
    int r = m->fs->desmontar(m->priv, gravar && !(m->opcoes & MONTAR_SO_LEITURA));
    disco_t *d = m->disco;
    d->montagem = NULL;
    d->rw_autorizado = false;
    m->usado = false;
    m->geracao = 0;
    if (m == montagem_raiz) montagem_raiz = NULL;
    if (m->criou_ponto && montagem_raiz && m != montagem_raiz) {
        /* D379 (M1): el punto se creó en la raíz en disco (un pendrive en /usb): se quita si quedó vacío. */
        montagem_raiz->fs->remover_pasta(montagem_raiz->priv, m->ponto + 1);
    } else if (m->criou_ponto) {
        no_t *x = vfs_achar(m->ponto);
        if (x && !x->filho) vfs_remover_pasta(m->ponto);
    }
    disco_t *raiz = disco_raiz(d);
    if (gravar) disco_sincronizar(raiz);   /* D281: lo diferido de la caché de bloques, antes de dar el volumen por libre */
    klog("[MONTAGEM] %s desmontado de %s%s\n", d->nome, m->ponto, gravar ? "" : " sem gravar");
    if (gravar && raiz->removivel && !raiz->de_boot && !disco_montado(raiz))
        disco_avisar(AVISO_DESMONTADO, "Ya puedes retirar el pendrive.");
    return r;
}

void montagem_desmontar_tudo(void)
{
    logs_disco_gravar("apagado o reinicio");   /* D120: antes de soltar nada */
    desmontando_tudo = true;
    montagem_t *r = montagem_raiz;   /* D379: la raíz, la última */
    for (int i = 0; i < MAX_MONTAGENS; i++)
        if (tabela[i].usado && &tabela[i] != r) montagem_desmontar(&tabela[i], tabela[i].disco->presente);
    if (r && r->usado) montagem_desmontar(r, r->disco->presente);
    desmontando_tudo = false;
    disco_sincronizar(NULL);   /* D281: apagar o reiniciar: nada diferido se queda en la caché */
}

/* ------------------------------------------------------------ /etc/fstab */

static bool fstab_ativo;   /* falso durante el arranque: el arranque aplica todo de una vez al final */

static bool proxima_palavra(const char **p, char *saida, size_t max)
{
    while (**p == ' ' || **p == '\t') (*p)++;
    if (!**p || **p == '\n' || **p == '#') return false;
    size_t n = 0;
    while (**p && **p != ' ' && **p != '\t' && **p != '\n' && **p != '\r') {
        if (n + 1 < max) saida[n++] = **p;
        (*p)++;
    }
    saida[n] = 0;
    return true;
}

/* D282: el valor de "uid=1000", "gid=sagui", "umask=022" (base 10 u 8). -1 si no se entiende. */
static int32_t valor_opcao(const char *v, size_t n, int base)
{
    if (n == 4 && txt_compara_n(v, "root", 4) == 0) return 0;
    if (n == 5 && txt_compara_n(v, "sagui", 5) == 0) return UID_SAGUI;
    if (!n || n > 7) return -1;
    int32_t x = 0;
    for (size_t i = 0; i < n; i++) {
        if (v[i] < '0' || v[i] >= '0' + base) return -1;
        x = x * base + (v[i] - '0');
    }
    return x;
}

/* Formato de cada línea: dispositivo carpeta tipo opciones. El dispositivo es un
 * nombre ("usb0p1", "hda1"), "LABEL=etiqueta" o "arranque". Opciones separadas
 * por coma: ro, rw, defaults, noauto (ignora la línea). "#" = comentario. */
void fstab_aplicar(disco_t *so_este)
{
    if (so_este && !fstab_ativo) return;
    uint8_t *txt;
    uint32_t tam;
    if (vfs_ler_inteiro("/etc/fstab", &txt, &tam) < 0) return;
    char *s = kmalloc(tam + 1);
    if (!s) { kfree(txt); return; }
    memcpy(s, txt, tam);
    s[tam] = 0;
    kfree(txt);
    for (const char *linha = s; *linha; ) {
        const char *p = linha;
        char disp[40], ponto[CAMINHO_MAX], tipo[16] = "auto", opc[64] = "defaults";
        if (proxima_palavra(&p, disp, sizeof disp) && proxima_palavra(&p, ponto, sizeof ponto)) {
            proxima_palavra(&p, tipo, sizeof tipo);
            proxima_palavra(&p, opc, sizeof opc);
            uint32_t opcoes = 0, dono = UID_SAGUI | GID_SAGUI << 16;
            bool pular = false;
            for (const char *o = opc; *o; ) {
                const char *fim = o;
                while (*fim && *fim != ',') fim++;
                size_t n = (size_t)(fim - o);
                if (n == 2 && txt_compara_n(o, "ro", 2) == 0) opcoes |= MONTAR_SO_LEITURA;
                if (n == 2 && txt_compara_n(o, "rw", 2) == 0) opcoes |= MONTAR_LEITURA_ESCRITA;
                if (n == 6 && txt_compara_n(o, "noauto", 6) == 0) pular = true;
                if (n == 6 && txt_compara_n(o, "noexec", 6) == 0) opcoes |= MONTAR_SEM_EXEC;   /* D485 */
                /* D282: uid=, gid=, umask= (FAT32, ISO 9660, UDF; el ext2 tiene los suyos). */
                int32_t v;
                if (n > 4 && txt_compara_n(o, "uid=", 4) == 0 && (v = valor_opcao(o + 4, n - 4, 10)) >= 0 && v <= UID_MAX) {
                    if (!(opcoes & MONTAR_DONO)) dono = (uint32_t)v | (uint32_t)v << 16;   /* el grupo, el suyo si no se dice */
                    else dono = (dono & 0xFFFF0000u) | (uint32_t)v;
                    opcoes |= MONTAR_DONO;
                }
                if (n > 4 && txt_compara_n(o, "gid=", 4) == 0 && (v = valor_opcao(o + 4, n - 4, 10)) >= 0 && v <= UID_MAX) {
                    if (!(opcoes & MONTAR_DONO)) dono = (uint32_t)v << 16 | UID_SAGUI;
                    else dono = (dono & 0xFFFF) | (uint32_t)v << 16;
                    opcoes |= MONTAR_DONO;
                }
                if (n > 6 && txt_compara_n(o, "umask=", 6) == 0 && (v = valor_opcao(o + 6, n - 6, 8)) >= 0)
                    opcoes |= MONTAR_COM_MASCARA(v);
                o = *fim ? fim + 1 : fim;
            }
            disco_t *d = disco_resolver(disp);
            if (d && !d->pai && d->volume != d) d = d->volume;   /* "usb0" = el primer volumen */
            if (!d && !pular && !so_este && txt_comeca_com(disp, "LABEL=SAGUI-"))   /* D179: el arranque sigue sin ella */
                klog("[FSTAB] %s (%s): no esta en el disco de arranque, se sigue sin ella\n", disp, ponto);
            if (!pular && d && !d->montagem && (!so_este || disco_raiz(d) == so_este) && ponto[0] == '/') {
                int r = montagem_montar_dono(d, ponto, tipo, opcoes, dono);
                klog("[FSTAB] %s -> %s (%s, %s): %d\n", disp, ponto, tipo, opc, r);
            }
        }
        while (*linha && *linha != '\n') linha++;
        if (*linha) linha++;
    }
    kfree(s);
}

void fstab_liberar(void)
{
    fstab_aplicar(NULL);
    fstab_ativo = true;
}

/* ------------------------------------------------------------ información */

int32_t montagem_info(info_montagem_t *v, uint32_t n)
{
    if (n > MAX_MONTAGENS) n = MAX_MONTAGENS;
    if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
    uint32_t k = 0;
    for (int i = 0; i < MAX_MONTAGENS && k < n; i++) {
        montagem_t *m = &tabela[i];
        if (!m->usado || m->oculto) continue;
        /* D485: una unidad de red solo la ve su cuenta (y root). */
        if (m->privado && processo_recursos()->uid != UID_ROOT && processo_recursos()->uid != m->uid) continue;
        memset(&v[k], 0, sizeof v[k]);
        txt_copia_n(v[k].dispositivo, m->disco->nome, sizeof v[k].dispositivo);
        txt_copia_n(v[k].ponto, m->ponto, sizeof v[k].ponto);
        txt_copia_n(v[k].tipo, m->tipo, sizeof v[k].tipo);
        v[k].opcoes = m->opcoes;
        if (m->fs->espaco) m->fs->espaco(m->priv, &v[k].total_kb, &v[k].livre_kb);
        k++;
    }
    return (int32_t)k;
}
