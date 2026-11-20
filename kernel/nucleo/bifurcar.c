// SPDX-License-Identifier: GPL-3.0-only
/* bifurcar.c - fork con copia al escribir y waitpid (D401).
 *
 * fork: todo con el cerrojo general y sin dormir, salvo la barrera del espacio compartido (D351),
 * que va primero: si otro hilo del proceso duerme dentro de una llamada con punteros ya
 * comprobados (o a mitad de MEM_COMPARTIR, que escribe en páginas por su dirección física), la
 * llamada se repite cuando sale. Después: entrada nueva de la tabla, espacio nuevo, las páginas
 * compartidas con copia al escribir (espaco_clonar) y las de las bibliotecas (imagens_clonar),
 * las TLB de las demás CPU sincronizadas (al padre se le quitó W), y la copia de lo demás. El
 * hijo es un solo hilo: el que llamó, con eax = 0. Si algo falla, se deshace entero: destruir el
 * espacio del hijo baja las cuentas de las páginas compartidas. */
#include "processo.h"
#include "sistema.h"
#include "console.h"
#include "texto.h"
#include "imagens.h"
#include "vfs.h"
#include "memoria.h"

void arquivos_herdar_todos(processo_t *filho, processo_t *pai);   /* arquivos.c */

/* Las páginas que son la palabra de un futex con un hilo del padre esperando se copian ya para
 * el hijo: compartidas, el padre las cambiaría de marco al escribir y el que espera se quedaría
 * dormido en el viejo (la clave del futex es el marco, D167). */
static processo_t *pai_futex;

static bool e_futex_do_pai(fis_t f)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = processo_indice(i);
        if ((t == pai_futex || t->lider == pai_futex) && t->estado == P_BLOQUEADO && t->futex &&
            (t->futex & ~(uint64_t)(PAGINA - 1)) == f)
            return true;
    }
    return false;
}

int32_t sis_bifurcar(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    processo_t *t = processo_atual(), *l = processo_lider(t);
    quadro_t *q = t->quadro_chamada;
    if (!q || !l->usuario || l->estado == P_ZUMBI) return ERRO_ACESSO;
    if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351: vamos a quitar W a las páginas del padre */
    processo_t *f = processo_novo(l->nome, l->pid);
    if (!f) return ERRO_LIMITE;
    espaco_t novo = espaco_criar();
    if (!novo) { processo_liberar(f); return ERRO_SEM_MEMORIA; }
    pai_futex = l;
    int r = espaco_clonar(l->espaco, novo, e_futex_do_pai);
    if (r == 0 && !imagens_clonar(l->espaco, novo)) r = ERRO_SEM_MEMORIA;
    espaco_sincronizar(l->espaco);   /* las páginas que perdieron W: ninguna CPU con la traducción vieja */
    if (r < 0) {
        espaco_destruir(novo);       /* las cuentas de lo ya compartido bajan solas */
        processo_liberar(f);
        klog("[SAGUI] fork de %d (%s) fallo: sin memoria\n", l->pid, l->nome);
        return r;
    }
    f->espaco = novo;
    f->usuario = true;
    f->exe_sistema = l->exe_sistema;
    f->uid = l->uid;
    f->gid = l->gid;
    f->como_uid = f->como_gid = -1;
    f->de_servico = l->de_servico;
    f->umask = l->umask;
    txt_copia_n(f->casa_conta, l->casa_conta, sizeof f->casa_conta);
    txt_copia_n(f->pasta, l->pasta, sizeof f->pasta);
    f->grupo = l->grupo;
    f->dados_inicio = l->dados_inicio;
    f->dados_fim = l->dados_fim;
    f->dados_teto = l->dados_teto;
    /* Revisión D401: de los huecos de pila de los hilos, solo el del hilo que llamó sigue ocupado
     * (el hijo corre en él). Los de los demás quedan libres para los hilos del hijo: su memoria
     * copiada sigue ahí y un hilo nuevo la reutiliza (pilha_de_hilo). Antes se heredaban todos
     * ocupados y nadie los soltaba. Los mapas de BAR y las ventanas no se heredan. */
    f->vagas_hilos = t->vaga_pilha ? (uint64_t)1 << (t->vaga_pilha - 1) : 0;
    f->mmio_fim = USUARIO_MMIO;
    f->fd_fechar_exec = l->fd_fechar_exec;
    sinais_iniciar_processo(f, t, true);
    arquivos_herdar_todos(f, l);
    fpu_guardar(t);   /* sus registros, a su área (el XSAVE entero, D381) */
    memcpy(f->fpu, t->fpu, FPU_ESTADO);
    f->fpu_usada = t->fpu_usada;
    processo_preparar_copia(f, q);
    klog("[SAGUI] fork %d (%s) -> %d\n", l->pid, l->nome, f->pid);
    return f->pid;
}

/* waitpid(pid, &estado, opciones): como sis_esperar (que no cambia), con los grupos y el estado
 * de Linux. */
int32_t sis_esperar_pid(uint32_t upid, uint32_t ptr_estado, uint32_t opcoes, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    int pid = (int)upid;
    int *estado = (int *)ptr_estado;
    if (opcoes & ~(uint32_t)ESPERA_SEM_BLOQUEAR) return ERRO_INVALIDO;
    if (estado && !usuario_ok(estado, sizeof(int), true)) return ERRO_ENDERECO;
    processo_t *eu = processo_recursos();
    bool tem_filho = false;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado == P_LIVRE || p->lider || p->pai != eu->pid) continue;
        if (pid > 0 && p->pid != pid) continue;
        if (pid == 0 && p->grupo != eu->grupo) continue;
        if (pid < -1 && p->grupo != -pid) continue;
        tem_filho = true;
        if (p->estado == P_ZUMBI && !grupo_na_cpu(p)) {   /* D165, D167: ninguna CPU sobre su pila */
            int r = p->pid;
            if (estado) *estado = espera_estado(p);
            processo_liberar(p);
            return r;
        }
    }
    if (!tem_filho) return ERRO_SEM_PROCESSO;
    if (opcoes & ESPERA_SEM_BLOQUEAR) return 0;
    bloquear(eu);   /* processo_encerrar despierta al padre (el proceso) */
    return SIS_REPETIR;
}
