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
 * Los campos usados son los de la especificación ELF; el código es de este proyecto. */
#include "sistema.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "processo.h"
#include "permisos.h"
#include "texto.h"
#include "vfs.h"

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

#define PT_LOAD 1
#define PF_W    2

/* Copia bytes dentro de otro espacio de direcciones. El kernel escribe en
 * las páginas por la dirección física (mapeo 1:1). Las de los segmentos ya
 * están asignadas; las de la pila se asignan aquí al llegar a ellas (D186). */
static bool copiar_para(espaco_t e, uint32_t virt, const void *origem, uint32_t n)
{
    const uint8_t *o = origem;
    while (n) {
        uint32_t fis = espaco_traduzir(e, virt, NULL);
        if (!fis) {
            if (!espaco_alocar(e, virt & ~(PAGINA - 1), PAGINA, PG_USUARIO | PG_ESCRITA)) return false;
            fis = espaco_traduzir(e, virt, NULL);
        }
        uint32_t cabe = PAGINA - (virt & 0xFFF);
        if (cabe > n) cabe = n;
        memcpy((void *)fis, o, cabe);
        virt += cabe; o += cabe; n -= cabe;
    }
    return true;
}

static int carregar_segmentos(espaco_t e, const uint8_t *img, uint32_t tam, uint32_t *entrada, uint32_t *fim)
{
    const elf_cabecalho_t *c = (const elf_cabecalho_t *)img;
    if (tam < sizeof *c || img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F')
        return ERRO_FORMATO;
    if (img[4] != 1 || img[5] != 1 || c->tipo != 2 || c->maquina != 3 || c->tam_ph != sizeof(elf_programa_t))
        return ERRO_FORMATO;   /* 32 bits, little-endian, ejecutable, Intel 386 */
    if (c->desloc_ph > tam || (uint32_t)c->qtd_ph * sizeof(elf_programa_t) > tam - c->desloc_ph)
        return ERRO_FORMATO;
    *fim = USUARIO_INICIO;
    bool algum = false;
    for (uint32_t i = 0; i < c->qtd_ph; i++) {
        const elf_programa_t *p = (const elf_programa_t *)(img + c->desloc_ph + i * sizeof(elf_programa_t));
        if (p->tipo != PT_LOAD || p->tam_memoria == 0) continue;
        if (p->virt < USUARIO_INICIO || p->virt >= DADOS_LIMITE || p->tam_memoria > DADOS_LIMITE - p->virt ||
            p->tam_arquivo > p->tam_memoria || p->desloc > tam || p->tam_arquivo > tam - p->desloc)
            return ERRO_FORMATO;   /* segmento fuera del área de los programas o más allá del archivo */
        uint32_t flags = PG_USUARIO | ((p->flags & PF_W) ? PG_ESCRITA : 0);
        if (!espaco_alocar(e, p->virt, p->tam_memoria, flags))
            return ERRO_SEM_MEMORIA;
        copiar_para(e, p->virt, img + p->desloc, p->tam_arquivo);
        if (p->virt + p->tam_memoria > *fim) *fim = p->virt + p->tam_memoria;
        algum = true;
    }
    if (!algum || c->entrada < USUARIO_INICIO || c->entrada >= *fim)
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
static uint32_t montar_pilha(espaco_t e, int argc, char *const argv[], int envc, char *const envp[])
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
    uint32_t c = (uint32_t)argc;
    sp -= 12;
    uint32_t cab[3] = { c, v_arg, v_env };
    if (!copiar_para(e, sp, cab, sizeof cab)) return 0;
    uint32_t margem = (sp & ~(PAGINA - 1)) - PILHA_USUARIO_INICIAL;
    if (!espaco_alocar(e, margem, sp - margem, PG_USUARIO | PG_ESCRITA)) return 0;
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
        if (l->como_uid >= 0) {
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
    } else if (identidade_kernel >= 0) {
        p->uid = (uint32_t)identidade_kernel & 0xFFFF;
        p->gid = (uint32_t)identidade_kernel >> 16;
    } else {
        p->uid = permisos_uid_inicial();
        p->gid = p->uid == UID_ROOT ? GID_ROOT : GID_SAGUI;
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

int processo_executar_env(const char *caminho, int argc, char *const argv[], int envc, char *const envp[],
                          processo_t *pai)
{
    uint32_t tam;
    uint8_t *img;
    int erro = vfs_ler_inteiro(caminho, &img, &tam);
    if (erro < 0) return erro;

    espaco_t e = espaco_criar();
    if (!e) { kfree(img); return ERRO_SEM_MEMORIA; }
    uint32_t entrada = 0, fim = 0;
    int r = carregar_segmentos(e, img, tam, &entrada, &fim);
    kfree(img);
    uint32_t pilha = 0;
    if (r == 0 && !(pilha = montar_pilha(e, argc, argv, envc, envp))) r = ERRO_SEM_MEMORIA;
    if (r < 0) { espaco_destruir(e); return r; }

    const char *nome = caminho;
    for (const char *p = caminho; *p; p++)
        if (*p == '/') nome = p + 1;
    processo_t *p = processo_novo(nome, pai ? pai->pid : 0);
    if (!p) { espaco_destruir(e); return ERRO_LIMITE; }
    p->espaco = e;
    p->exe_sistema = exe_do_caminho(caminho);   /* D251 */
    dar_identidade(p, pai);                     /* D255 */
    p->dados_inicio = p->dados_fim = (fim + PAGINA - 1) & ~(PAGINA - 1);
    p->grupo = pai ? pai->grupo : p->pid;   /* el hijo queda en el grupo del padre (D037) */
    if (pai) txt_copia_n(p->pasta, pai->pasta, sizeof p->pasta);
    arquivos_herdar(p, pai);
    processo_preparar_usuario(p, entrada, pilha);
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
