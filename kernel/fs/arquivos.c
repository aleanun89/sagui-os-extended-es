// SPDX-License-Identifier: GPL-3.0-only
/* arquivos.c - archivos abiertos y las llamadas de archivos y carpetas (H14).
 *
 * Cada proceso tiene una tabla de 16 descriptores; cada descriptor apunta
 * a un "archivo abierto", que guarda lo que es (un nodo del VFS, la terminal o
 * un extremo de pipe), el modo y la posición. Varios descriptores (de uno o de
 * varios procesos) pueden apuntar al mismo archivo abierto, después de un
 * duplicar o de un ejecutar; él cuenta las referencias y sólo se cierra de
 * verdad cuando la última desaparece. */
#include "permisos.h"
#include "abi.h"
#include "console.h"
#include "caminho.h"
#include "memoria.h"
#include "pipe.h"
#include "processo.h"
#include "sistema.h"
#include "terminal_es.h"
#include "texto.h"
#include "vfs.h"
#include "montagem.h"
#include "conf_sistema.h"
#include "tv.h"
#include "rede.h"
#include "aleatorio.h"
#include "audio.h"

enum { A_NO = 1, A_TERMINAL, A_PIPE_LER, A_PIPE_ESCREVER, A_MONTADO, A_TV_MESTRE, A_TV_ESCRAVO, A_SOCKET, A_ALEATORIO, A_AUDIO };

typedef struct arquivo {
    int      tipo;
    int      modo;
    uint32_t pos;
    no_t    *no;
    pipe_t  *pipe;
    arq_montado_t *mnt;  /* archivo en un volumen montado (D040) */
    tv_t    *tv;         /* terminal virtual (D036) */
    socket_t *sock;      /* socket de red (D046) */
    struct fluxo *fluxo; /* flujo de audio (D088) */
    char    *criado;     /* D282: ruta, si esta apertura lo creó y aún no se escribió el principio */
    char    *caminho;    /* D341: la ruta con que se abrió (A_NO, A_MONTADO), para futimens */
    int      refs;
} arquivo_t;

/* Un archivo de volumen montado sólo vale mientras la misma montura continúe. */
static bool mnt_vivo(arquivo_t *a) { return a->mnt && montagem_viva(a->mnt); }

/* Operaciones que cambian el volumen: rechazadas si está sólo para lectura. */
static bool so_leitura(montagem_t *m) { return (m->opcoes & MONTAR_SO_LEITURA) != 0; }

static arquivo_t *arquivo_novo(int tipo, int modo)
{
    arquivo_t *a = kcalloc(sizeof(arquivo_t));
    if (a) { a->tipo = tipo; a->modo = modo; a->refs = 1; }
    return a;
}

static void arquivo_soltar(arquivo_t *a)
{
    if (--a->refs > 0) return;
    if (a->tipo == A_NO) vfs_soltar(a->no);
    else if (a->tipo == A_MONTADO) { if (mnt_vivo(a)) a->mnt->m->fs->fechar(a->mnt->m->priv, a->mnt->dados); kfree(a->mnt); }
    else if (a->tipo == A_PIPE_LER) pipe_fechar_ponta(a->pipe, false);
    else if (a->tipo == A_PIPE_ESCREVER) pipe_fechar_ponta(a->pipe, true);
    else if (a->tipo == A_TV_MESTRE || a->tipo == A_TV_ESCRAVO) tv_fechar(a->tv, a->tipo == A_TV_MESTRE);
    else if (a->tipo == A_SOCKET) socket_fechar(a->sock);
    else if (a->tipo == A_AUDIO) audio_fluxo_fechar(a->fluxo);
    if (a->criado) kfree(a->criado);
    if (a->caminho) kfree(a->caminho);
    kfree(a);
}

/* D282: un programa recién creado (compilado, copiado, descargado) se puede
 * ejecutar sin chmod: si lo primero que se escribe en un archivo que creó esta
 * misma apertura es la cabecera ELF, recibe la x donde tenga la r. Solo una
 * vez, y solo en lo creado aquí (nunca en un archivo que ya existía). */
static void talvez_executavel(arquivo_t *a, const uint8_t *buf, uint32_t n)
{
    if (!a->criado) return;
    char *c = a->criado;
    a->criado = NULL;
    if (a->pos == 0 && n >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') {
        meta_t m;
        if (vfs_meta(c, &m) == 0 && m.com_donos)
            vfs_fixar_meta(c, -1, -1, m.modo | ((m.modo & 0444) >> 2));
    }
    kfree(c);
}

static char *copiar_caminho(const char *c)
{
    size_t n = txt_tamanho(c) + 1;
    char *x = kmalloc(n);
    if (x) memcpy(x, c, n);
    return x;
}

static int fd_livre(processo_t *p)
{
    for (int i = 0; i < MAX_ARQUIVOS; i++)
        if (!p->arquivos[i]) return i;
    return ERRO_LIMITE;
}

static arquivo_t *pegar(uint32_t fd)
{
    return fd < MAX_ARQUIVOS ? processo_recursos()->arquivos[fd] : NULL;
}

/* D341: la ruta con la que se abrió el archivo del descriptor "fd" (para fijar sus fechas). Si se
 * renombró después, es la de antes: futimens actúa sobre la ruta, no sobre el archivo. */
int arquivos_caminho(uint32_t fd, char *saida, size_t max)
{
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if ((a->tipo != A_NO && a->tipo != A_MONTADO) || !a->caminho) return ERRO_NAO_SUPORTADO;
    if (txt_tamanho(a->caminho) >= max) return ERRO_INVALIDO;
    txt_copia_n(saida, a->caminho, max);
    return 0;
}

/* Los flujos de audio son descriptores (D088): cerrar el programa, o el

 * descriptor, silencia el flujo. */
int arquivos_novo_audio(struct fluxo *f)
{
    int fd = fd_livre(processo_recursos());
    if (fd < 0) return fd;
    arquivo_t *a = arquivo_novo(A_AUDIO, ABRIR_ESCREVER);
    if (!a) return ERRO_SEM_MEMORIA;
    a->fluxo = f;
    processo_recursos()->arquivos[fd] = a;
    return fd;
}

struct fluxo *arquivos_fluxo(uint32_t fd)
{
    arquivo_t *a = pegar(fd);
    return a && a->tipo == A_AUDIO ? a->fluxo : NULL;
}

/* El hijo hereda sólo la entrada, la salida y la salida de errores (D010). El
 * primer programa, sin padre, recibe la terminal en las tres. */
void arquivos_herdar(processo_t *filho, processo_t *pai)
{
    if (!pai) {
        arquivo_t *t = arquivo_novo(A_TERMINAL, ABRIR_LER | ABRIR_ESCREVER);
        if (!t) return;
        t->refs = 3;
        for (int i = 0; i < 3; i++) filho->arquivos[i] = t;
        return;
    }
    for (int i = 0; i < 3; i++)
        if (pai->arquivos[i]) {
            filho->arquivos[i] = pai->arquivos[i];
            pai->arquivos[i]->refs++;
        }
}

void arquivos_fechar_todos(processo_t *p)
{
    for (int i = 0; i < MAX_ARQUIVOS; i++)
        if (p->arquivos[i]) {
            arquivo_soltar(p->arquivos[i]);
            p->arquivos[i] = NULL;
        }
}

/* Texto del programa -> ruta absoluta simplificada, como la ve el programa ("vista") y la real
 * ("saida"): D340, lo que una cuenta pide en /home va a su carpeta personal. "vista" puede ser NULL. */
static int caminho_do_usuario_v(uint32_t ptr, char *saida, char *vista)
{
    char bruto[CAMINHO_MAX];
    int r = usuario_texto((const char *)ptr, bruto, sizeof bruto);
    if (r < 0) return r;
    if (r == 0) return ERRO_INVALIDO;
    if (!caminho_resolver(processo_recursos()->pasta, bruto, saida, CAMINHO_MAX)) return ERRO_INVALIDO;
    if (vista) txt_copia_n(vista, saida, CAMINHO_MAX);
    if (!permisos_casa_traduzir(processo_recursos(), saida)) return ERRO_INVALIDO;
    return 0;
}

static int caminho_do_usuario(uint32_t ptr, char *saida) { return caminho_do_usuario_v(ptr, saida, NULL); }

int32_t sis_abrir(uint32_t ptr, uint32_t modo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    if (!(modo & (ABRIR_LER | ABRIR_ESCREVER))) modo |= ABRIR_LER;
    if (modo & (ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) modo |= ABRIR_ESCREVER;
    int quer = (modo & ABRIR_LER ? PERM_LER : 0) | (modo & ABRIR_ESCREVER ? PERM_ESCREVER : 0);
    /* /dev/urandom y /dev/random (D061): el generador del kernel como archivo,
     * para scripts y programas portados. Los dos son lo mismo (no bloquean). */
    if (txt_compara(caminho, "/dev/urandom") == 0 || txt_compara(caminho, "/dev/random") == 0) {
        int fd = fd_livre(processo_recursos());
        if (fd < 0) return fd;
        arquivo_t *a = arquivo_novo(A_ALEATORIO, (int)modo & (ABRIR_LER | ABRIR_ESCREVER));
        if (!a) return ERRO_SEM_MEMORIA;
        processo_recursos()->arquivos[fd] = a;
        return fd;
    }
    /* D282: si existe, sus permisos; si no y se crea, escribir en su carpeta. */
    meta_t mt;
    bool existia = !permisos_root(processo_recursos()) ? vfs_meta(caminho, &mt) == 0 : true;
    r = existia || !(modo & ABRIR_CRIAR) ? permisos_acesso(caminho, quer) : permisos_criar(caminho);
    if (r < 0) return r;
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {
        if (!*rel) return ERRO_E_PASTA;
        if ((modo & ABRIR_ESCREVER) && so_leitura(m)) return ERRO_SO_LEITURA;
        int fd = fd_livre(processo_recursos());
        if (fd < 0) return fd;
        arquivo_t *a = arquivo_novo(A_MONTADO, (int)modo);
        arq_montado_t *f = kcalloc(sizeof(arq_montado_t) + m->fs->tam_arq);
        if (!a || !f) { if (a) kfree(a); if (f) kfree(f); return ERRO_SEM_MEMORIA; }
        f->m = m;
        f->geracao = m->geracao;
        r = m->fs->abrir(m->priv, rel, (int)modo, f->dados, &f->tamanho);
        if (r < 0) { kfree(a); kfree(f); return r; }
        a->mnt = f;
        /* D282: el ext2 dice 1 si lo creó (con el dueño de quien llama). Si no, y no
         * existía al comprobarlo, otro lo creó mientras tanto: sus permisos mandan. */
        if (r != 1 && !existia && permisos_acesso(caminho, quer) < 0) {
            m->fs->fechar(m->priv, f->dados);
            kfree(a); kfree(f);
            return ERRO_ACESSO;
        }
        if (r == 1 && m->fs->fixar_meta) a->criado = copiar_caminho(caminho);
        a->caminho = copiar_caminho(caminho);   /* D341: sin memoria, futimens no podrá */
        processo_recursos()->arquivos[fd] = a;
        return fd;
    }
    no_t *x = vfs_achar(caminho);
    bool criou = false;
    if (!x) {
        if (!(modo & ABRIR_CRIAR)) return ERRO_NAO_EXISTE;
        r = vfs_criar(caminho, TIPO_ARQUIVO, &x);
        if (r < 0) return r;
        criou = true;
    }
    if (x->tipo == TIPO_PASTA) return ERRO_E_PASTA;
    int fd = fd_livre(processo_recursos());
    if (fd < 0) return fd;
    arquivo_t *a = arquivo_novo(A_NO, (int)modo);
    if (!a) return ERRO_SEM_MEMORIA;
    if (modo & ABRIR_TRUNCAR) vfs_truncar(x);
    if (criou) a->criado = copiar_caminho(caminho);
    a->caminho = copiar_caminho(caminho);
    a->no = x;
    vfs_segurar(x);
    processo_recursos()->arquivos[fd] = a;
    return fd;
}

static int32_t ler_de(arquivo_t *a, uint32_t buf, uint32_t n)
{
    switch (a->tipo) {
    case A_NO: {
        if (!(a->modo & ABRIR_LER)) return ERRO_ACESSO;
        uint32_t r = vfs_ler(a->no, a->pos, (void *)buf, n);
        a->pos += r;
        return (int32_t)r;
    }
    case A_MONTADO: {
        if (!(a->modo & ABRIR_LER)) return ERRO_ACESSO;
        if (!mnt_vivo(a)) return ERRO_NAO_EXISTE;   /* volumen desmontado o eliminado */
        int r = a->mnt->m->fs->ler(a->mnt->m->priv, a->mnt->dados, a->pos, (void *)buf, n);
        if (r > 0) a->pos += (uint32_t)r;
        return r;
    }
    case A_TERMINAL: {
        int32_t r = terminal_ler((char *)buf, n);
        console_mostrar();   /* el eco de lo escrito aparece ya */
        return r;
    }
    case A_PIPE_LER:  return pipe_ler(a->pipe, (char *)buf, n);
    case A_TV_MESTRE: return tv_mestre_ler(a->tv, (char *)buf, n);
    case A_TV_ESCRAVO: return tv_ler(a->tv, (char *)buf, n);
    case A_SOCKET:    return socket_ler(a->sock, (char *)buf, n);
    case A_ALEATORIO:
        if (n > 65536) n = 65536;
        aleatorio_bytes((void *)buf, n);
        return (int32_t)n;
    default:          return ERRO_ACESSO;
    }
}

/* D167: leer y escribir pueden dormir dentro del kernel (un disco, cpu_espera
 * suelta el cerrojo general) y, mientras tanto, otro hilo del mismo proceso
 * puede cerrar el descriptor. La operación se queda con una referencia al
 * fichero hasta acabar, como el fget/fput de Linux. */
int32_t sis_ler(uint32_t fd, uint32_t buf, uint32_t n, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (!usuario_ok((void *)buf, n, true)) return ERRO_ENDERECO;
    a->refs++;
    int32_t r = ler_de(a, buf, n);
    arquivo_soltar(a);
    return r;
}

/* D371 (P14): /root se guarda en SAGUI-CONF (16 MB), junto a las contrase�as y las claves. Lo que se escribe en
 * /root no puede dejarla con menos de RAIZ_RESERVA_KB libres: una contrase�a nueva siempre tiene sitio. */
#define RAIZ_RESERVA_KB 4096u
static bool raiz_sem_sitio(const arquivo_t *a, uint32_t n)
{
    if (!a->caminho || txt_compara_n(a->caminho, CONF_RAIZ "/", sizeof CONF_RAIZ) != 0 || !a->mnt->m->fs->espaco) return false;
    uint32_t total, livre;
    a->mnt->m->fs->espaco(a->mnt->m->priv, &total, &livre);
    if (livre >= RAIZ_RESERVA_KB + n / 1024u + 1) return false;
    klog("[CONF] /root: sin sitio (quedan %u KB en SAGUI-CONF, reserva %u KB)\n", livre, RAIZ_RESERVA_KB);
    return true;
}

static int32_t escrever_em(arquivo_t *a, uint32_t buf, uint32_t n)
{
    switch (a->tipo) {
    case A_NO: {
        if (!(a->modo & ABRIR_ESCREVER)) return ERRO_ACESSO;
        if (a->modo & ABRIR_ACRESCENTAR) a->pos = a->no->tamanho;
        talvez_executavel(a, (const uint8_t *)buf, n);
        int r = vfs_escrever(a->no, a->pos, (const void *)buf, n);
        if (r > 0) a->pos += (uint32_t)r;
        return r;
    }
    case A_MONTADO: {
        if (!(a->modo & ABRIR_ESCREVER)) return ERRO_ACESSO;
        if (!mnt_vivo(a)) return ERRO_NAO_EXISTE;
        if (so_leitura(a->mnt->m)) return ERRO_SO_LEITURA;
        if (a->modo & ABRIR_ACRESCENTAR) a->pos = a->mnt->tamanho;
        if (raiz_sem_sitio(a, n)) return ERRO_LIMITE;
        talvez_executavel(a, (const uint8_t *)buf, n);
        int r = a->mnt->m->fs->escrever(a->mnt->m->priv, a->mnt->dados, a->pos, (const void *)buf, n, &a->mnt->tamanho);
        if (r > 0) a->pos += (uint32_t)r;
        return r;
    }
    case A_TERMINAL:      return terminal_escrever((const char *)buf, n);
    case A_PIPE_ESCREVER: return pipe_escrever(a->pipe, (const char *)buf, n);
    case A_TV_MESTRE:     return tv_mestre_escrever(a->tv, (const char *)buf, n);
    case A_TV_ESCRAVO:    return tv_escrever(a->tv, (const char *)buf, n);
    case A_SOCKET:        return socket_escrever(a->sock, (const char *)buf, n);
    case A_ALEATORIO:     aleatorio_misturar((const void *)buf, n); return (int32_t)n;   /* como en Linux: mezcla, no se fía */
    case A_AUDIO:         return audio_fluxo_escrever(a->fluxo, (const uint8_t *)buf, n, false);
    default:              return ERRO_ACESSO;
    }
}

int32_t sis_escrever(uint32_t fd, uint32_t buf, uint32_t n, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (!usuario_ok((void *)buf, n, false)) return ERRO_ENDERECO;
    a->refs++;   /* D167: ver sis_ler */
    int32_t r = escrever_em(a, buf, n);
    arquivo_soltar(a);
    return r;
}

int32_t sis_fechar(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    processo_recursos()->arquivos[fd] = NULL;
    arquivo_soltar(a);
    return 0;
}

int32_t sis_posicionar(uint32_t fd, uint32_t desloc, uint32_t origem, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (a->tipo != A_NO && a->tipo != A_MONTADO) return ERRO_ACESSO;
    uint32_t tam = a->tipo == A_NO ? a->no->tamanho : a->mnt->tamanho;
    int32_t base = origem == POS_INICIO ? 0 : origem == POS_ATUAL ? (int32_t)a->pos :
                   origem == POS_FIM ? (int32_t)tam : -1;
    if (base < 0) return ERRO_INVALIDO;
    int32_t nova = base + (int32_t)desloc;
    if (nova < 0) return ERRO_INVALIDO;
    a->pos = (uint32_t)nova;
    return nova;
}

/* truncar(fd, tamanho) (D116): reduce el archivo (libera clusters/bloques) o lo
 * aumenta rellenando con ceros. No mueve la posici�n. */
int32_t sis_truncar(uint32_t fd, uint32_t tam, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (!(a->modo & ABRIR_ESCREVER)) return ERRO_ACESSO;
    if (a->tipo == A_NO) return vfs_redimensionar(a->no, tam);
    if (a->tipo != A_MONTADO) return ERRO_ACESSO;
    if (!mnt_vivo(a)) return ERRO_NAO_EXISTE;
    if (so_leitura(a->mnt->m)) return ERRO_SO_LEITURA;
    if (tam == a->mnt->tamanho) return 0;
    int r = a->mnt->m->fs->truncar(a->mnt->m->priv, a->mnt->dados, tam);
    if (r < 0) return r;
    a->mnt->tamanho = tam;
    return 0;
}

/* sincronizar(fd) (D116): lo pendiente del archivo llega al disco. En la RAM no hay nada. */
int32_t sis_sincronizar(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (a->tipo == A_NO) return 0;
    if (a->tipo != A_MONTADO) return ERRO_ACESSO;
    if (!mnt_vivo(a)) return ERRO_NAO_EXISTE;
    if (so_leitura(a->mnt->m) || !a->mnt->m->fs->sincronizar) return 0;
    return a->mnt->m->fs->sincronizar(a->mnt->m->priv, a->mnt->dados);
}

int32_t sis_info_arquivo(uint32_t ptr, uint32_t pinfo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    info_arquivo_t *info = (info_arquivo_t *)pinfo;
    if (!usuario_ok(info, sizeof *info, true)) return ERRO_ENDERECO;
    if ((r = permisos_acesso(caminho, 0)) < 0) return r;   /* D282: entrar en sus carpetas */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {
        if (!*rel) { info->tipo = TIPO_PASTA; info->tamanho = 0; return 0; }
        return m->fs->info(m->priv, rel, info);
    }
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    info->tipo = x->tipo;
    info->tamanho = x->tamanho;
    return 0;
}

/* SIS_DATAS_ARQUIVO (D080): fechas en UTC. En el árbol en RAM sólo existe la de
 * modificación; ext2 y FAT32 guardan las tres (el FAT32 en hora local, que el
 * fat32.c convierte). */
int32_t sis_datas_arquivo(uint32_t ptr, uint32_t pdatas, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    datas_arquivo_t *v = (datas_arquivo_t *)pdatas;
    if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
    datas_arquivo_t t = { 0, 0, 0 };
    if ((r = permisos_acesso(caminho, 0)) < 0) return r;   /* D282 */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {
        if (*rel && m->fs->datas) {
            r = m->fs->datas(m->priv, rel, &t);
            if (r < 0) return r;
        }
    } else {
        no_t *x = vfs_achar(caminho);
        if (!x) return ERRO_NAO_EXISTE;
        t.modificado = x->modificado;
    }
    *v = t;
    return 0;
}

int32_t sis_criar_pasta(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    if ((r = permisos_criar(caminho)) < 0) return r;   /* D282 */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) return !*rel ? ERRO_JA_EXISTE : so_leitura(m) ? ERRO_SO_LEITURA : m->fs->criar_pasta(m->priv, rel);
    return vfs_criar(caminho, TIPO_PASTA, NULL);
}

int32_t sis_remover_pasta(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    if ((r = permisos_remover(caminho)) < 0) return r;   /* D282 */
    /* No deja eliminar la carpeta en la que algún proceso está. */
    for (int i = 0; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado != P_LIVRE && p->estado != P_ZUMBI && txt_compara(p->pasta, caminho) == 0)
            return ERRO_OCUPADO;
    }
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) return !*rel ? ERRO_OCUPADO : so_leitura(m) ? ERRO_SO_LEITURA : m->fs->remover_pasta(m->priv, rel);
    return vfs_remover_pasta(caminho);
}

int32_t sis_apagar(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    if ((r = permisos_remover(caminho)) < 0) return r;   /* D282 */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) return !*rel ? ERRO_E_PASTA : so_leitura(m) ? ERRO_SO_LEITURA : m->fs->apagar(m->priv, rel);
    return vfs_apagar(caminho);
}

int32_t sis_listar(uint32_t ptr, uint32_t indice, uint32_t pent, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(ptr, caminho);
    if (r < 0) return r;
    if (!usuario_ok((void *)pent, sizeof(entrada_pasta_t), true)) return ERRO_ENDERECO;
    if ((r = permisos_acesso(caminho, PERM_LER)) < 0) return r;   /* D282: leer la carpeta */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) return m->fs->listar(m->priv, rel, (int)indice, (entrada_pasta_t *)pent);
    return vfs_listar(caminho, (int)indice, (entrada_pasta_t *)pent);
}

int32_t sis_mudar_pasta(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    char caminho[CAMINHO_MAX], vista[CAMINHO_MAX];
    int r = caminho_do_usuario_v(ptr, caminho, vista);
    if (r < 0) return r;
    if ((r = permisos_acesso(caminho, PERM_EXECUTAR)) < 0) return r;   /* D282: entrar */
    const char *rel;
    montagem_t *m = montagem_de(caminho, &rel);
    if (m) {
        if (*rel) {
            info_arquivo_t inf;
            r = m->fs->info(m->priv, rel, &inf);
            if (r < 0) return r;
            if (inf.tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
        }
        txt_copia_n(processo_recursos()->pasta, vista, sizeof processo_recursos()->pasta);   /* D340: como la ve */
        return 0;
    }
    no_t *x = vfs_achar(caminho);
    if (!x) return ERRO_NAO_EXISTE;
    if (x->tipo != TIPO_PASTA) return ERRO_NAO_E_PASTA;
    txt_copia_n(processo_recursos()->pasta, vista, sizeof processo_recursos()->pasta);
    return 0;
}

int32_t sis_pasta_atual(uint32_t buf, uint32_t n, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    uint32_t precisa = txt_tamanho(processo_recursos()->pasta) + 1;
    if (n < precisa) return ERRO_LIMITE;
    if (!usuario_ok((void *)buf, precisa, true)) return ERRO_ENDERECO;
    memcpy((void *)buf, processo_recursos()->pasta, precisa);
    return (int32_t)precisa - 1;
}

int32_t sis_renomear(uint32_t po, uint32_t pd, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    char origem[CAMINHO_MAX], destino[CAMINHO_MAX];
    int r = caminho_do_usuario(po, origem);
    if (r < 0) return r;
    r = caminho_do_usuario(pd, destino);
    if (r < 0) return r;
    /* D282: quitarlo de su carpeta (y si es pegajosa, ser su dueño) y ponerlo en la
     * otra: dentro de "destino" si es una carpeta, o en su lugar (reemplazándolo). */
    if ((r = permisos_remover(origem)) < 0) return r;
    meta_t mdest;
    if (vfs_meta(destino, &mdest) == 0 && mdest.tipo == TIPO_PASTA) {
        if ((r = permisos_acesso(destino, PERM_ESCREVER | PERM_EXECUTAR)) < 0) return r;
        /* "mv a carpeta" = "carpeta/a": si ya hay uno, se reemplaza (y puede ser un secreto). */
        char final[CAMINHO_MAX];
        const char *base = origem;
        for (const char *p = origem; *p; p++) if (*p == '/') base = p + 1;
        size_t n = txt_tamanho(destino);
        if (n + 1 + txt_tamanho(base) + 1 > sizeof final) return ERRO_INVALIDO;
        txt_copia_n(final, destino, sizeof final);
        if (n > 1) final[n++] = '/';
        txt_copia_n(final + n, base, sizeof final - n);
        r = vfs_meta(final, &mdest) == 0 ? permisos_remover(final) : permisos_ruta(final, PERM_ESCREVER);
    } else {
        r = vfs_meta(destino, &mdest) == 0 ? permisos_remover(destino) : permisos_criar(destino);
    }
    if (r < 0) return r;
    const char *ro, *rd;
    montagem_t *mo = montagem_de(origem, &ro), *md = montagem_de(destino, &rd);
    if (mo && mo == md) {
        if (!*ro) return ERRO_OCUPADO;
        return so_leitura(mo) ? ERRO_SO_LEITURA : mo->fs->renomear(mo->priv, ro, rd);
    }
    if (mo || md) return ERRO_ACESSO;   /* entre la RAM y un volumen, o entre dos: el mv copia y borra */
    if (montagem_abaixo(origem)) return ERRO_OCUPADO;   /* carpeta con un punto de montaje dentro */
    return vfs_renomear(origem, destino);
}

int32_t sis_pipe(uint32_t pfds, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    int *fds = (int *)pfds;
    if (!usuario_ok(fds, 2 * sizeof(int), true)) return ERRO_ENDERECO;
    int fl = fd_livre(processo_recursos());
    if (fl < 0) return fl;
    processo_recursos()->arquivos[fl] = (arquivo_t *)1;   /* reserva mientras busca el segundo */
    int fe = fd_livre(processo_recursos());
    processo_recursos()->arquivos[fl] = NULL;
    if (fe < 0) return fe;
    pipe_t *p = pipe_criar();
    arquivo_t *al = arquivo_novo(A_PIPE_LER, ABRIR_LER);
    arquivo_t *ae = arquivo_novo(A_PIPE_ESCREVER, ABRIR_ESCREVER);
    if (!p || !al || !ae) {
        if (p) kfree(p);
        if (al) kfree(al);
        if (ae) kfree(ae);
        return ERRO_SEM_MEMORIA;
    }
    al->pipe = ae->pipe = p;
    processo_recursos()->arquivos[fl] = al;
    processo_recursos()->arquivos[fe] = ae;
    fds[0] = fl;
    fds[1] = fe;
    return 0;
}

/* duplicar(antigo, novo): el descriptor "novo" pasa a apuntar al mismo
 * archivo abierto que "antigo" (cerrando lo que había en él). Así es como el
 * shell conecta la salida de un programa a un archivo o a un pipe. */
int32_t sis_duplicar(uint32_t antigo, uint32_t novo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(antigo);
    if (!a || novo >= MAX_ARQUIVOS) return ERRO_DESCRITOR;
    if (antigo == novo) return (int32_t)novo;
    a->refs++;
    if (processo_recursos()->arquivos[novo]) arquivo_soltar(processo_recursos()->arquivos[novo]);
    processo_recursos()->arquivos[novo] = a;
    return (int32_t)novo;
}

/* disponivel(fd): cuánto se puede leer de un pipe sin bloquear. La Terminal en
 * ventana usa esto para mirar la salida del shell y los eventos de la ventana en
 * el mismo bucle, sin quedarse atrapada en ninguno de los dos. */
int32_t sis_disponivel(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    if (a->tipo == A_TV_MESTRE) return tv_mestre_disponivel(a->tv);
    if (a->tipo == A_TV_ESCRAVO) return tv_escravo_disponivel(a->tv);   /* D317 */
    if (a->tipo == A_TERMINAL) return terminal_disponivel();            /* D317 */
    if (a->tipo == A_SOCKET) return socket_disponivel(a->sock);
    if (a->tipo != A_PIPE_LER) return ERRO_INVALIDO;
    if (a->pipe->qtd) return (int32_t)a->pipe->qtd;
    return a->pipe->escritores ? 0 : ERRO_PIPE_QUEBRADO;
}

int32_t sis_modo_terminal(uint32_t modo, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    if (modo != TERM_LINHA && modo != TERM_BRUTO) return ERRO_INVALIDO;
    arquivo_t *t = pegar(0);
    if (t && t->tipo == A_TV_ESCRAVO) tv_modo(t->tv, (int)modo);
    else terminal_modo((int)modo);
    return 0;
}

int32_t sis_limpar_tela(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    arquivo_t *t = pegar(1);
    if (t && t->tipo == A_TV_ESCRAVO) tv_limpar(t->tv);
    else terminal_limpar();
    return 0;
}

/* abrir_tv(fds): crea un terminal virtual; fds[0] es el maestro (quien dibuja
 * lee la salida y escribe las teclas), fds[1] el esclavo (pasa a ser la entrada y las
 * salidas de los programas). Ver D036. */
int32_t sis_abrir_tv(uint32_t pfds, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    int *fds = (int *)pfds;
    if (!usuario_ok(fds, 2 * sizeof(int), true)) return ERRO_ENDERECO;
    int fm = fd_livre(processo_recursos());
    if (fm < 0) return fm;
    processo_recursos()->arquivos[fm] = (arquivo_t *)1;   /* reserva mientras busca el segundo */
    int fe = fd_livre(processo_recursos());
    processo_recursos()->arquivos[fm] = NULL;
    if (fe < 0) return fe;
    tv_t *t = tv_criar();
    arquivo_t *am = arquivo_novo(A_TV_MESTRE, ABRIR_LER | ABRIR_ESCREVER);
    arquivo_t *ae = arquivo_novo(A_TV_ESCRAVO, ABRIR_LER | ABRIR_ESCREVER);
    if (!t || !am || !ae) {
        if (t) { tv_fechar(t, true); tv_fechar(t, false); }
        if (am) kfree(am);
        if (ae) kfree(ae);
        return ERRO_SEM_MEMORIA;
    }
    am->tv = ae->tv = t;
    processo_recursos()->arquivos[fm] = am;
    processo_recursos()->arquivos[fe] = ae;
    fds[0] = fm;
    fds[1] = fe;
    return 0;
}

/* primeiro_plano(grupo): el shell avisa qué grupo de procesos está en
 * primer plano en el terminal de su entrada 0; el Ctrl+C termina todos los
 * procesos de ese grupo (D037). 0 = ninguno. Devuelve el grupo anterior, para
 * que un shell dentro de otro pueda restaurarlo. En el terminal del kernel no tiene
 * efecto (allí el Ctrl+C sólo descarta la línea). */
int32_t sis_primeiro_plano(uint32_t pid, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    arquivo_t *t = pegar(0);
    if (!t || t->tipo != A_TV_ESCRAVO) return 0;
    int antes = t->tv->primeiro_plano;
    t->tv->primeiro_plano = (int)pid;
    return antes;
}

/* interrupcoes(): cuántos Ctrl+C ya recibió el terminal de la entrada 0. El shell
 * compara antes y después para abandonar un bucle que ejecuta él mismo (D037). */
int32_t sis_interrupcoes(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    arquivo_t *t = pegar(0);
    if (!t || t->tipo != A_TV_ESCRAVO) return 0;
    return (int32_t)(t->tv->interrupcoes & 0x7FFFFFFF);
}

/* tipo_descritor(fd): lo que hay detrás de un descriptor. El shell lo usa para
 * saber si la entrada es una terminal (modo interactivo, edición de la línea). */
int32_t sis_tipo_descritor(uint32_t fd, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    arquivo_t *a = pegar(fd);
    if (!a) return ERRO_DESCRITOR;
    switch (a->tipo) {
    case A_TERMINAL: case A_TV_ESCRAVO: return TIPO_TERMINAL;
    case A_PIPE_LER: case A_PIPE_ESCREVER: case A_TV_MESTRE: return TIPO_PIPE;
    default: return TIPO_ARQUIVO;
    }
}

/* Un descriptor nuevo para un socket de red (D046). */
int arquivos_novo_socket(socket_t *s)
{
    int fd = fd_livre(processo_recursos());
    if (fd < 0) return fd;
    arquivo_t *a = arquivo_novo(A_SOCKET, ABRIR_LER | ABRIR_ESCREVER);
    if (!a) return ERRO_SEM_MEMORIA;
    a->sock = s;
    processo_recursos()->arquivos[fd] = a;
    return fd;
}

socket_t *arquivos_socket(uint32_t fd)
{
    arquivo_t *a = pegar(fd);
    return a && a->tipo == A_SOCKET ? a->sock : NULL;
}
