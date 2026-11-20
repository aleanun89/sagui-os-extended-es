// SPDX-License-Identifier: GPL-3.0-only
/* carregador.c - crea procesos a partir de ejecutables ELF32 (H13).
 *
 * Un ejecutable ELF trae una lista de "segmentos para cargar" (PT_LOAD):
 * cada uno dice la dirección virtual, cuántos bytes vienen del archivo y cuántos
 * ocupa en la memoria (la diferencia se pone en cero: es la BSS). El cargador:
 *   1. lee el archivo entero a un buffer del kernel;
 *   2. comprueba el encabezado (ELF, 32 bits, little-endian, ejecutable, i386);
 *   3. crea un espacio de direcciones nuevo y, para cada segmento, asigna las
 *      páginas en la dirección pedida y copia los bytes del archivo;
 *   4. crea la pila del programa en el tope del área del usuario y pone en ella
 *      los argumentos y el entorno (argc, argv, envp y los textos; D037);
 *   5. arma el cuadro inicial para el anillo 3 y deja el proceso listo.
 * D378: si el ejecutable trae PT_INTERP (programa con bibliotecas compartidas),
 * se carga además su intérprete (/lib/ld-sagui.so.1, un ELF a dirección fija en
 * [INTERP_BASE, DADOS_LIMITE)), se añade un cuarto valor a la pila ([esp+12]: el
 * vector auxiliar con las cabeceras de programa, la entrada y AT_SECURE) y el
 * proceso empieza en el intérprete, que carga las bibliotecas y salta a la
 * entrada del programa. Los programas sin PT_INTERP no cambian.
 * Los campos usados son los de la especificación ELF; el código es de este proyecto. */
#include "sistema.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "memalta.h"
#include "processo.h"
#include "permisos.h"
#include "texto.h"
#include "vfs.h"
#include "relogio.h"
#include "gui.h"

/* Límites de argumentos y entorno (D037): los textos van a la pila del
 * programa, así que el total queda en 16 KB. */
#define MAX_ARGS       256
#define MAX_ENV        256
#define MAX_ARGS_BYTES (16u * 1024u)

typedef struct EMPACOTADO {
    uint8_t  ident[16];
    uint16_t tipo, maquina;
    uint32_t versao, entrada, desloc_ph, desloc_sh, flags;
    uint16_t tam_cabecalho, tam_ph, qtd_ph, tam_sh, qtd_sh, indice_nomes;
} elf_cabecalho_t;

typedef struct EMPACOTADO {
    uint32_t tipo, desloc, virt, fis, tam_arquivo, tam_memoria, flags, alinhamento;
} elf_programa_t;

#define PT_LOAD   1
#define PT_INTERP 3

/* D378: vector auxiliar (los números son los del System V). */
#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_BASE   7
#define AT_ENTRY  9
#define AT_SECURE 23
#define AT_EXECFN 31
#define INTERP_MAX 64
#define AUX_PARES  9
#define PH_MAX     32

/* Lo que el intérprete necesita saber del programa (D378). */
typedef struct {
    uint32_t entrada;               /* la del programa, no la del intérprete */
    const char *caminho;            /* la ruta resuelta del programa (AT_EXECFN): el $ORIGIN de confianza */
    uint32_t base_interp;
    uint32_t qtd_ph;
    elf_programa_t ph[PH_MAX];
    uint32_t end_seguro;            /* dónde quedó el valor de AT_SECURE en la pila (se pone al final) */
} auxiliar_t;
#define PF_X    1
#define PF_W    2

/* Copia bytes dentro de otro espacio de direcciones. El kernel escribe en
 * las páginas por su dirección física, a través de una ventana temporal
 * (pueden ser de la memoria alta, D348). Las de los segmentos ya están
 * asignadas; las de la pila se asignan aquí al llegar a ellas (D186). */
static bool copiar_para(espaco_t e, uint32_t virt, const void *origem, uint32_t n)
{
    const uint8_t *o = origem;
    while (n) {
        fis_t fis = espaco_traduzir(e, virt, NULL);
        if (!fis) {
            if (!espaco_alocar(e, virt & ~(PAGINA - 1), PAGINA, PG_USUARIO | PG_ESCRITA | PG_NX)) return false;
            fis = espaco_traduzir(e, virt, NULL);
            if (!fis) return false;   /* D352: estaba en la swap y no hay memoria para traerla */
        }
        uint32_t cabe = PAGINA - (virt & 0xFFF);
        if (cabe > n) cabe = n;
        fisica_escrever(fis, o, cabe);
        virt += cabe; o += cabe; n -= cabe;
    }
    return true;
}

/* "interp": NULL al cargar el propio intérprete (que no puede traer PT_INTERP);
 * si no, recibe la ruta del intérprete o "" si el programa es estático. Los
 * segmentos tienen que caer en [ini, lim) (lim = 0: según haya intérprete). */
/* D378: el intérprete corre dentro de todos los programas dinámicos, también los de root y los
 * del sistema: tiene que ser de root y que nadie más pueda cambiarlo ni cambiar las carpetas
 * por las que se llega a él (desde /lib). Sin dueños (FAT, ISO 9660), no vale. */
static bool interp_confiavel(const char *interp)
{
    char c[INTERP_MAX];
    txt_copia_n(c, interp, sizeof c);
    for (;;) {
        meta_t m;
        if (vfs_meta(c, &m) < 0 || !m.com_donos || m.uid != 0 || (m.modo & 022)) {
            klog("[SAGUI] interprete %s rechazado: %s no es solo de root\n", interp, c);
            return false;
        }
        if (txt_compara(c, "/lib") == 0) return true;
        char *barra = NULL;
        for (char *q = c; *q; q++)
            if (*q == '/') barra = q;
        if (!barra || barra == c) return false;   /* no debería pasar: empieza por «/lib/» */
        *barra = 0;
    }
}

static int carregar_segmentos(espaco_t e, const uint8_t *img, uint32_t tam, uint32_t ini, uint32_t lim,
                              uint32_t *entrada, uint32_t *fim, char *interp)
{
    const elf_cabecalho_t *c = (const elf_cabecalho_t *)img;
    if (tam < sizeof *c || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F')
        return ERRO_FORMATO;
    if (img[4] != 1 || img[5] != 1 || c->tipo != 2 || c->maquina != 3 || c->tam_ph != sizeof(elf_programa_t))
        return ERRO_FORMATO;   /* 32 bits, little-endian, ejecutable, Intel 386 */
    if (c->desloc_ph > tam || (uint32_t)c->qtd_ph * sizeof(elf_programa_t) > tam - c->desloc_ph)
        return ERRO_FORMATO;
    if (interp) interp[0] = 0;
    for (uint32_t i = 0; i < c->qtd_ph; i++) {   /* D378: ¿con intérprete? */
        const elf_programa_t *p = (const elf_programa_t *)(img + c->desloc_ph + i * sizeof(elf_programa_t));
        if (p->tipo != PT_INTERP) continue;
        if (!interp || p->desloc > tam || p->tam_arquivo > tam - p->desloc || p->tam_arquivo < 2 ||
            p->tam_arquivo > INTERP_MAX || img[p->desloc + p->tam_arquivo - 1] != 0)
            return ERRO_FORMATO;
        txt_copia_n(interp, (const char *)img + p->desloc, INTERP_MAX);
        /* Solo intérpretes de /lib (que solo root escribe), sin «..». */
        if (interp[0] != '/' || interp[1] != 'l' || interp[2] != 'i' || interp[3] != 'b' || interp[4] != '/')
            return ERRO_FORMATO;
        for (const char *q = interp; q[0]; q++)
            if (q[0] == '.' && q[1] == '.') return ERRO_FORMATO;
    }
    if (!lim) lim = interp && interp[0] ? INTERP_BASE : DADOS_LIMITE;
    *fim = ini;
    bool algum = false;
    for (uint32_t i = 0; i < c->qtd_ph; i++) {
        const elf_programa_t *p = (const elf_programa_t *)(img + c->desloc_ph + i * sizeof(elf_programa_t));
        if (p->tipo != PT_LOAD || p->tam_memoria == 0) continue;
        if (p->virt < ini || p->virt >= lim || p->tam_memoria > lim - p->virt ||
            p->tam_arquivo > p->tam_memoria || p->desloc > tam || p->tam_arquivo > tam - p->desloc)
            return ERRO_FORMATO;   /* segmento fuera del área de los programas o más allá del archivo */
        /* D349: sin PF_X, no ejecutable (con NX). Si dos segmentos comparten una
         * página, la página queda con la suma de los permisos: espaco_alocar no
         * toca lo que ya existe, así que se ajusta después. */
        uint32_t flags = PG_USUARIO | ((p->flags & PF_W) ? PG_ESCRITA : 0) | ((p->flags & PF_X) ? 0 : PG_NX);
        if (!espaco_alocar(e, p->virt, p->tam_memoria, flags))
            return ERRO_SEM_MEMORIA;
        espaco_ajustar(e, p->virt, p->tam_memoria, flags & PG_ESCRITA, (p->flags & PF_X) ? PG_NX : 0);
        copiar_para(e, p->virt, img + p->desloc, p->tam_arquivo);
        if (p->virt + p->tam_memoria > *fim) *fim = p->virt + p->tam_memoria;
        algum = true;
    }
    if (!algum || c->entrada < ini || c->entrada >= *fim)
        return ERRO_FORMATO;
    *entrada = c->entrada;
    return 0;
}

/* Copia un vector de textos a la pila (de arriba hacia abajo): primero los
 * textos, después el vector de punteros terminado en NULL. Devuelve el nuevo
 * tope; "vetor" recibe la dirección del vector. */
static uint32_t empilhar_vetor(espaco_t e, uint32_t sp, int n, char *const v[], uint32_t *ponteiros, uint32_t *vetor)
{
    for (int i = n - 1; i >= 0; i--) {
        uint32_t t = txt_tamanho(v[i]) + 1;
        sp -= t;
        if (!copiar_para(e, sp, v[i], t)) return 0;
        ponteiros[i] = sp;
    }
    ponteiros[n] = 0;
    sp &= ~3u;
    sp -= (uint32_t)(n + 1) * 4;
    if (!copiar_para(e, sp, ponteiros, (uint32_t)(n + 1) * 4)) return 0;
    *vetor = sp;
    return sp;
}

/* Pila inicial, de arriba hacia abajo: los textos y el vector del entorno, los
 * textos y el vector argv (los dos terminados en NULL), y por último argc, el
 * puntero a argv y el puntero a envp, que la función _inicio de la
 * libsagui lee en [esp], [esp+4] y [esp+8].
 *
 * D186: la pila tiene 8 MB de direcciones, pero aquí solo se asignan las
 * páginas que ocupan esos datos y PILHA_USUARIO_INICIAL más por debajo; el
 * resto llega con el fallo de página (pilha.c) cuando el programa lo toca. */
static uint32_t montar_pilha(espaco_t e, int argc, char *const argv[], int envc, char *const envp[],
                             auxiliar_t *aux)
{
    uint32_t sp = PILHA_USUARIO_TOPO;
    int maior = argc > envc ? argc : envc;
    uint32_t *ponteiros = kmalloc((uint32_t)(maior + 1) * 4);
    if (!ponteiros) return 0;
    uint32_t v_env = 0, v_arg = 0;
    sp = empilhar_vetor(e, sp, envc, envp, ponteiros, &v_env);
    if (sp) sp = empilhar_vetor(e, sp, argc, argv, ponteiros, &v_arg);
    kfree(ponteiros);
    if (!sp) return 0;
    uint32_t v_aux = 0;
    if (aux) {
        /* D378: la ruta del programa (AT_EXECFN), la copia de las cabeceras de programa y, debajo,
         * el vector auxiliar. La ruta la pone el kernel, no argv[0] (que elige quien lanza): de ella
         * sale el $ORIGIN, que un proceso seguro también usa. */
        uint32_t t = txt_tamanho(aux->caminho) + 1;
        sp -= t;
        if (!copiar_para(e, sp, aux->caminho, t)) return 0;
        uint32_t v_fn = sp;
        uint32_t tam_ph = aux->qtd_ph * sizeof(elf_programa_t);
        sp = (sp - tam_ph) & ~15u;
        uint32_t v_ph = sp;
        if (!copiar_para(e, sp, aux->ph, tam_ph)) return 0;
        uint32_t par[AUX_PARES * 2] = {
            AT_PHDR, v_ph, AT_PHENT, sizeof(elf_programa_t), AT_PHNUM, aux->qtd_ph, AT_PAGESZ, PAGINA,
            AT_BASE, aux->base_interp, AT_ENTRY, aux->entrada, AT_EXECFN, v_fn, AT_SECURE, 1, AT_NULL, 0,
        };
        sp = (sp - sizeof par) & ~15u;
        if (!copiar_para(e, sp, par, sizeof par)) return 0;
        v_aux = sp;
        for (int i = 0; i < AUX_PARES; i++)   /* el valor de AT_SECURE: seguro hasta saber la identidad */
            if (par[2 * i] == AT_SECURE) aux->end_seguro = sp + (uint32_t)(2 * i + 1) * 4;
    }
    uint32_t c = (uint32_t)argc;
    sp -= aux ? 16 : 12;
    uint32_t cab[4] = { c, v_arg, v_env, v_aux };
    if (!copiar_para(e, sp, cab, aux ? 16 : 12)) return 0;
    uint32_t margem = (sp & ~(PAGINA - 1)) - PILHA_USUARIO_INICIAL;
    if (!espaco_alocar(e, margem, sp - margem, PG_USUARIO | PG_ESCRITA | PG_NX)) return 0;
    return sp;
}

int processo_executar(const char *caminho, int argc, char *const argv[], processo_t *pai)
{
    return processo_executar_env(caminho, argc, argv, 0, NULL, pai);
}

/* D255: la identidad del proceso nuevo. Con padre, la suya (o la que pidió
 * para su próximo hijo con USU_COMO, que se gasta aquí); sin padre (lo lanza el
 * kernel: Terminal, menú Inicio, escritorio), la de la sesión, salvo que quien
 * lo lanza pida otra (el gestor de servicios, root). */
static int32_t identidade_kernel = -1;   /* uid | gid << 16 para el próximo sin padre; -1 = la de la sesión */

int processo_executar_como(const char *caminho, int argc, char *const argv[], uint32_t uid, uint32_t gid)
{
    identidade_kernel = (int32_t)(uid | gid << 16);
    int r = processo_executar_env(caminho, argc, argv, 0, NULL, NULL);
    identidade_kernel = -1;
    return r;
}

static void dar_identidade(processo_t *p, processo_t *pai)
{
    if (pai) {
        processo_t *l = processo_lider(pai);
        bool como = l->como_uid >= 0;
        if (como) {
            p->uid = (uint32_t)l->como_uid;
            p->gid = (uint32_t)l->como_gid;
            l->como_uid = l->como_gid = -1;   /* una sola vez */
        } else {
            p->uid = l->uid;
            p->gid = l->gid;
        }
        /* Lo que lanza el gestor de servicios (y sus descendientes) no es la
         * sesión local: SSH, cron, VNC... Se hereda aunque el padre muera. */
        p->de_servico = l->de_servico || l->exe_sistema == EXE_SERVICIOS;
        p->umask = l->umask;   /* D282 */
        txt_copia_n(p->casa_conta, l->casa_conta, sizeof p->casa_conta);   /* D340 */
        /* D371 (fase 4): un hijo que nace con otra cuenta (un servicio con usuario=sagui) ve la carpeta de esa cuenta. */
        if (como && p->uid != 0) {
            char casa[sizeof p->casa_conta];
            permisos_casa_de_uid(p->uid, casa, sizeof casa);
            if (casa[0]) txt_copia_n(p->casa_conta, casa, sizeof p->casa_conta);
        }
    } else if (identidade_kernel >= 0) {
        p->uid = (uint32_t)identidade_kernel & 0xFFFF;
        p->gid = (uint32_t)identidade_kernel >> 16;
        if (p->uid != 0) permisos_casa_de_uid(p->uid, p->casa_conta, sizeof p->casa_conta);   /* D371 */
        /* D340: lo que el kernel lanza con otra identidad (el gestor de servicios) ve /home tal cual. */
    } else {
        /* D340: la sesión local (la cuenta que entró en el escritorio) con su carpeta personal. */
        p->uid = permisos_uid_inicial();
        p->gid = permisos_sessao_gid();
        txt_copia_n(p->casa_conta, permisos_sessao_casa(), sizeof p->casa_conta);
    }
}

/* D251: la ruta (ya resuelta, absoluta) de la que se carga el programa dice
 * si es uno de los que dan permisos. Comparación exacta: otra ruta, otro
 * nombre o una copia en otra carpeta es un programa común. */
static uint8_t exe_do_caminho(const char *caminho)
{
    if (txt_compara(caminho, "/bin/servicios") == 0) return EXE_SERVICIOS;
    if (txt_compara(caminho, "/bin/vncd") == 0 || txt_compara(caminho, "/apps/bin/vncd") == 0) return EXE_VNCD;
    if (caminho[0] == '/' && caminho[1] == 'b' && caminho[2] == 'i' && caminho[3] == 'n' && caminho[4] == '/') {   /* D269 */
        for (const char *c = caminho; c[0]; c++)
            if (c[0] == '.' && c[1] == '.') return EXE_COMUM;   /* «/bin/../home/...» no es de /bin */
        return EXE_BIN;
    }
    return EXE_COMUM;
}

/* D401: lo que deja cargado un programa en un espacio nuevo, sin proceso todavía (lo usan
 * executar, que crea uno, y exec, que cambia el espacio del que llama). */
typedef struct {
    espaco_t e;
    uint32_t entrada, pilha, fim;
    bool     dinamico;       /* con intérprete (D378): el área de datos acaba en INTERP_BASE */
    uint32_t end_seguro;     /* dónde va el valor de AT_SECURE (0 = sin vector auxiliar) */
} carga_t;

static int carregar_programa(const char *caminho, int argc, char *const argv[], int envc, char *const envp[],
                             carga_t *cg)
{
    uint32_t tam;
    uint8_t *img;
    memset(cg, 0, sizeof *cg);
    int erro = vfs_ler_inteiro(caminho, &img, &tam);
    if (erro < 0) return erro;

    espaco_t e = espaco_criar();
    if (!e) { kfree(img); return ERRO_SEM_MEMORIA; }
    uint32_t entrada = 0, fim = 0;
    char interp[INTERP_MAX];
    auxiliar_t *aux = NULL;
    int r = carregar_segmentos(e, img, tam, USUARIO_INICIO, 0, &entrada, &fim, interp);
    if (r == 0 && interp[0]) {
        /* D378: el intérprete, en su zona; el proceso empieza en él. */
        const elf_cabecalho_t *c = (const elf_cabecalho_t *)img;
        aux = kcalloc(sizeof *aux);
        if (!aux) r = ERRO_SEM_MEMORIA;
        else if (c->qtd_ph > PH_MAX) r = ERRO_FORMATO;
        else {
            aux->entrada = entrada;
            aux->caminho = caminho;
            aux->base_interp = INTERP_BASE;
            aux->qtd_ph = c->qtd_ph;
            for (uint32_t i = 0; i < c->qtd_ph; i++)
                aux->ph[i] = *(const elf_programa_t *)(img + c->desloc_ph + i * sizeof(elf_programa_t));
            uint32_t ti;
            uint8_t *ii;
            r = interp_confiavel(interp) ? vfs_ler_inteiro(interp, &ii, &ti) : ERRO_ACESSO;
            if (r >= 0) {
                uint32_t fim_interp = 0;
                r = carregar_segmentos(e, ii, ti, INTERP_BASE, DADOS_LIMITE, &entrada, &fim_interp, NULL);
                kfree(ii);
            }
        }
    }
    kfree(img);
    uint32_t pilha = 0;
    if (r == 0 && !(pilha = montar_pilha(e, argc, argv, envc, envp, aux))) r = ERRO_SEM_MEMORIA;
    if (r < 0) { espaco_destruir(e); kfree(aux); return r; }
    cg->e = e;
    cg->entrada = entrada;
    cg->pilha = pilha;
    cg->fim = fim;
    cg->dinamico = aux != NULL;
    cg->end_seguro = aux ? aux->end_seguro : 0;
    kfree(aux);
    return 0;
}

/* D378: AT_SECURE = 0 solo para una cuenta normal que ejecuta un programa sin permisos
 * especiales (no root, no de /bin). */
static void fixar_seguro(const carga_t *cg, const processo_t *p)
{
    if (!cg->end_seguro) return;
    uint32_t seguro = (p->uid == 0 || p->exe_sistema != EXE_COMUM) ? 1 : 0;
    copiar_para(cg->e, cg->end_seguro, &seguro, 4);
}

static const char *nome_de(const char *caminho)
{
    const char *nome = caminho;
    for (const char *p = caminho; *p; p++)
        if (*p == '/') nome = p + 1;
    return nome;
}

int processo_executar_env(const char *caminho, int argc, char *const argv[], int envc, char *const envp[],
                          processo_t *pai)
{
    carga_t cg;
    int r = carregar_programa(caminho, argc, argv, envc, envp, &cg);
    if (r < 0) return r;
    processo_t *p = processo_novo(nome_de(caminho), pai ? pai->pid : 0);
    if (!p) { espaco_destruir(cg.e); return ERRO_LIMITE; }
    p->espaco = cg.e;
    p->exe_sistema = exe_do_caminho(caminho);   /* D251 */
    dar_identidade(p, pai);                     /* D255 */
    p->dados_inicio = p->dados_fim = (cg.fim + PAGINA - 1) & ~(PAGINA - 1);
    if (cg.dinamico) p->dados_teto = INTERP_BASE;   /* D378: área de datos hasta el intérprete */
    fixar_seguro(&cg, p);
    p->grupo = pai ? pai->grupo : p->pid;   /* el hijo queda en el grupo del padre (D037) */
    if (pai) {
        txt_copia_n(p->pasta, pai->pasta, sizeof p->pasta);
        permisos_pasta_herdada(p, pai);   /* D371: si nace con otra carpeta que su padre */
    }
    arquivos_herdar(p, pai);
    sinais_iniciar_processo(p, NULL, false);   /* D401: todo por defecto (SIGPIPE ignorada) */
    processo_preparar_usuario(p, cg.entrada, cg.pilha);
    klog("[SAGUI] executar %s -> processo %d\n", caminho, p->pid);
    return p->pid;
}

/* Copia un vector de textos del programa (terminado en NULL) al buffer
 * "textos" del kernel. Devuelve cuántos, o un error. */
static int copiar_vetor(const char *const *uv, char **v, int max, char *textos, uint32_t *usado)
{
    int n = 0;
    while (uv) {
        if (!usuario_ok(&uv[n], sizeof(char *), false)) return ERRO_ENDERECO;
        const char *a = uv[n];
        if (!a) break;
        if (n == max) return ERRO_LIMITE;
        if (*usado >= MAX_ARGS_BYTES) return ERRO_LIMITE;
        int r = usuario_texto(a, textos + *usado, MAX_ARGS_BYTES - *usado);
        if (r < 0) return r == ERRO_INVALIDO ? ERRO_LIMITE : r;
        v[n++] = textos + *usado;
        *usado += (uint32_t)r + 1;
    }
    return n;
}

/* executar(caminho, argv, envp): argv y envp son vectores de textos
 * terminados en NULL, en el espacio del programa (envp puede ser 0: entorno
 * vacío). Todo se copia al kernel antes de crear el proceso. */
int32_t sis_executar(uint32_t ptr_caminho, uint32_t ptr_argv, uint32_t ptr_envp, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    char bruto[CAMINHO_MAX], caminho[CAMINHO_MAX];
    int r = usuario_texto((const char *)ptr_caminho, bruto, sizeof bruto);
    if (r < 0) return r;
    if (!caminho_resolver(processo_recursos()->pasta, bruto, caminho, sizeof caminho)) return ERRO_INVALIDO;
    if (!permisos_casa_traduzir(processo_recursos(), caminho)) return ERRO_INVALIDO;   /* D340 */
    if ((r = permisos_acesso(caminho, PERM_LER | PERM_EXECUTAR)) < 0) return r;   /* D282 */

    char **argv = kmalloc((MAX_ARGS + 1 + MAX_ENV + 1) * sizeof(char *));
    char *textos = kmalloc(MAX_ARGS_BYTES);
    if (!argv || !textos) { kfree(argv); kfree(textos); return ERRO_SEM_MEMORIA; }
    char **envp = argv + MAX_ARGS + 1;
    uint32_t usado = 0;
    int argc = copiar_vetor((const char *const *)ptr_argv, argv, MAX_ARGS, textos, &usado);
    int envc = argc < 0 ? 0 : copiar_vetor((const char *const *)ptr_envp, envp, MAX_ENV, textos, &usado);
    if (argc < 0) r = argc;
    else if (envc < 0) r = envc;
    else {
        if (argc == 0) argv[argc++] = bruto;   /* sin argv: el nombre del programa */
        argv[argc] = NULL;
        envp[envc] = NULL;
        r = processo_executar_env(caminho, argc, argv, envc, envp, processo_recursos());   /* D167: el padre es el proceso */
        processo_recursos()->como_uid = processo_recursos()->como_gid = -1;   /* D255: USU_COMO vale para un intento */
    }
    kfree(textos);
    kfree(argv);
    return r;
}

/* Revisión D401: un error de execve. Si ya se mataron los demás hilos (la llamada se repitió
 * esperando a que dejaran su CPU) no se puede volver al programa como si nada: el proceso termina,
 * como en Linux tras el punto de no retorno. */
static int32_t exec_falhou(processo_t *l, int32_t r)
{
    if (!l->exec_sem_volta) return r;
    l->exec_sem_volta = false;
    kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: execve fallo (%d) tras terminar sus hilos\n", l->pid, l->nome, r);
    l->sinal_fim = SINAL_KILL;
    processo_encerrar(l, SAIDA_ENCERRADO_PELO_KERNEL, "execve fallo tras terminar sus hilos");
    return r;
}

/* D401: execve(caminho, argv, envp). El proceso que llama pasa a ejecutar otro programa con el
 * mismo pid, padre, grupo, identidad, carpeta y descriptores (menos los FD_CLOEXEC).
 *   1. Todo lo que puede fallar va antes de tocar nada: la ruta, los permisos (D282) y la carga
 *      entera del programa (y de su intérprete, D378) en un espacio NUEVO. Si falla, vuelve con
 *      el error y el proceso sigue igual.
 *   2. Los demás hilos terminan. Si alguno sigue aún en otra CPU (sacándolo de su código), se
 *      suelta lo cargado y la llamada se repite en el tic siguiente: el espacio viejo no se puede
 *      destruir mientras una CPU lo tenga en su CR3 (D165, D167).
 *   3. Sin vuelta atrás: se cierran las ventanas (sus píxeles están en el espacio viejo), se
 *      cambia el espacio y se destruye el viejo, se cierran los FD_CLOEXEC, los tratadores de
 *      señales vuelven a SIG_DFL, la FPU queda limpia y el cuadro empieza en la entrada nueva.
 * Desde un hilo que no es el principal: ERRO_NAO_SUPORTADO (carencia anotada en D401). */
int32_t sis_substituir(uint32_t ptr_caminho, uint32_t ptr_argv, uint32_t ptr_envp, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    processo_t *t = processo_atual(), *l = processo_lider(t);
    quadro_t *q = t->quadro_chamada;
    if (!q || !l->usuario) return ERRO_ACESSO;
    if (t != l) return ERRO_NAO_SUPORTADO;
    char bruto[CAMINHO_MAX], caminho[CAMINHO_MAX];
    int r = usuario_texto((const char *)ptr_caminho, bruto, sizeof bruto);
    if (r < 0) return exec_falhou(l, r);
    if (!caminho_resolver(l->pasta, bruto, caminho, sizeof caminho)) return exec_falhou(l, ERRO_INVALIDO);
    if (!permisos_casa_traduzir(l, caminho)) return exec_falhou(l, ERRO_INVALIDO);   /* D340 */
    if ((r = permisos_acesso(caminho, PERM_LER | PERM_EXECUTAR)) < 0) return exec_falhou(l, r);   /* D282 */

    char **argv = kmalloc((MAX_ARGS + 1 + MAX_ENV + 1) * sizeof(char *));
    char *textos = kmalloc(MAX_ARGS_BYTES);
    if (!argv || !textos) { kfree(argv); kfree(textos); return exec_falhou(l, ERRO_SEM_MEMORIA); }
    char **envp = argv + MAX_ARGS + 1;
    uint32_t usado = 0;
    int argc = copiar_vetor((const char *const *)ptr_argv, argv, MAX_ARGS, textos, &usado);
    int envc = argc < 0 ? 0 : copiar_vetor((const char *const *)ptr_envp, envp, MAX_ENV, textos, &usado);
    carga_t cg;
    if (argc < 0) r = argc;
    else if (envc < 0) r = envc;
    else {
        if (argc == 0) argv[argc++] = bruto;
        argv[argc] = NULL;
        envp[envc] = NULL;
        r = carregar_programa(caminho, argc, argv, envc, envp, &cg);
    }
    kfree(textos);
    kfree(argv);
    if (r < 0) return exec_falhou(l, r);

    /* 2. Los demás hilos. Primero la barrera D351 (ninguno dormido dentro de una llamada con
     * punteros del espacio viejo); los que corren en otra CPU se sacan de su código; quedan
     * zombis sin padre (los recoge escalonar). */
    if (!espaco_pode_tirar()) { espaco_destruir(cg.e); return SIS_REPETIR; }
    bool ainda = false;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *h = processo_indice(i);
        if (h->lider != l || h->estado == P_LIVRE) continue;
        if (h->estado != P_ZUMBI) {
            l->exec_sem_volta = true;   /* desde aquí, un fallo de la llamada repetida termina el proceso */
            if (h->na_cpu) processo_tirar_da_cpu(h);
            fpu_esquecer(h);
            h->estado = P_ZUMBI;
            h->espera = NULL;
            h->acordar_em = 0;
        }
        h->pai = 0;
        /* Revisión D401: su hueco de pila ya no es suyo (vagas_hilos vuelve a 0 abajo). Si no, al
         * recogerlo escalonar() más tarde soltaría el hueco de un hilo NUEVO del programa cargado y
         * dos hilos acabarían con la misma pila. */
        h->vaga_pilha = 0;
        if (h->na_cpu) ainda = true;
    }
    if (ainda) {
        espaco_destruir(cg.e);
        t->acordar_em = relogio_tiques() + 1;
        bloquear(&l->espaco);
        return SIS_REPETIR;
    }

    /* 3. Sin vuelta atrás. */
    l->exec_sem_volta = false;
    janelas_do_processo_fechar(l);
    espaco_t velho = l->espaco;
    l->espaco = cg.e;
    espaco_ativar(cg.e);
    espaco_destruir(velho);
    l->exe_sistema = exe_do_caminho(caminho);   /* D251: como al lanzarlo */
    txt_copia_n(l->nome, nome_de(caminho), sizeof l->nome);
    l->dados_inicio = l->dados_fim = (cg.fim + PAGINA - 1) & ~(PAGINA - 1);
    l->dados_teto = cg.dinamico ? INTERP_BASE : 0;
    fixar_seguro(&cg, l);
    l->mmio_fim = USUARIO_MMIO;
    memset(l->bar_mapas, 0, sizeof l->bar_mapas);
    l->vagas_hilos = 0;
    l->tirando = 0;
    arquivos_fechar_exec(l);
    sinais_exec(l);
    fpu_guardar(l);
    fpu_esquecer(l);
    memset(l->fpu, 0, FPU_ESTADO);   /* la cabecera del XSAVE a cero (D381) */
    l->fpu_usada = false;
    memset(q, 0, sizeof *q);
    q->ds = q->es = q->fs = q->gs = SEL_UDADOS;
    q->cs = SEL_UCODIGO;
    q->ss_usuario = SEL_UDADOS;
    q->esp_usuario = cg.pilha;
    q->eip = cg.entrada;
    q->eflags = 0x202;
    q->vetor = VETOR_SISTEMA;
    klog("[SAGUI] exec %s en el proceso %d\n", caminho, l->pid);
    return 0;
}
