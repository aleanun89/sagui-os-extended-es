// SPDX-License-Identifier: GPL-3.0-only
/* processos.c - tabla de procesos y planificador round-robin apropiativo.
 *
 * Cada proceso tiene su propia pila del kernel. Cuando lo interrumpen,
 * el código de vetores.asm guarda los registros en esa pila (el "cuadro") y
 * el despachador devuelve el cuadro a retomar (D007). Cambiar de proceso es,
 * entonces, sólo elegir otro cuadro, cambiar el directorio de páginas (CR3) y
 * avisar a la TSS de qué pila del kernel usar cuando el siguiente programa en el
 * anillo 3 sea interrumpido.
 *
 * Política: en cada tic del reloj el proceso actual gasta un poco de su
 * rebanada; cuando se acaba, el planificador elige el próximo listo. QUÉ
 * proceso toma cada CPU lo decide la política activa (planificadores.c,
 * D174: round-robin, afinidad, equilibrio o prioridad); aquí queda el
 * mecanismo. El proceso 0 ("ocioso") sólo corre cuando nadie más está listo. */
#include "processo.h"
#include "smp.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "texto.h"
#include "vfs.h"
#include "gui.h"
#include "cpu_dados.h"
#include "trava_geral.h"
#include "planificador.h"

void tss_definir_pilha(uint32_t esp0);


static processo_t tabela[MAX_PROCESSOS];
/* El ocioso de cada CPU (D157). El de la CPU 0 es el propio arranque del
 * kernel (tabela[0]); el de cada procesador secundario es un cuadro y una
 * pila propios, para que dos CPUs puedan estar ociosas a la vez. */
static processo_t ociosos[CPU_MAX];
/* La tabla de procesos es compartida entre las CPU: la protege el cerrojo
 * general del kernel (D165), que toda CPU tiene mientras está en el kernel. */
static int proximo_pid = 1;
quadro_t *ao_ceder(quadro_t *q);

/* El proceso que esta CPU está a ejecutar. Con el GS montado, cada CPU
 * lee su propia entrada de la tabla; antes de eso (el arranque) se usa el
 * valor global, que todavía no hay. */
static processo_t *atual_global;

processo_t *processo_atual(void)
{
    if (cpu_gs_lista()) return cpu_processo_gs();
    return atual_global;
}

/* El ocioso de una CPU: el de la CPU 0 es el proceso 0 de la tabla; los
 * de los secundarios tienen su propia entrada (D157). */
static processo_t *ocioso_da_cpu(uint32_t cpu)
{
    return cpu == 0 ? &tabela[0] : &ociosos[cpu];
}

const char *nome_estado(estado_t e)
{
    static const char *n[] = { "livre", "pronto", "executando", "bloqueado", "zumbi" };
    return e <= P_ZUMBI ? n[e] : "?";
}

/* Prepara el ocioso de un procesador secundario: es su propio contexto (el
 * que tiene al arrancar), y su cuadro se rellena en la primera
 * interrupción de su reloj (D157). */
void processo_ocioso_preparar(uint32_t cpu)
{
    if (cpu == 0 || cpu >= CPU_MAX) return;
    processo_t *p = &ociosos[cpu];
    memset(p, 0, sizeof *p);
    p->pid = 0;
    p->estado = P_EXECUTANDO;
    p->cpu = cpu;
    txt_copia_n(p->nome, "ocioso", sizeof p->nome);
    p->espaco = espaco_kernel();
    txt_copia_n(p->pasta, "/", sizeof p->pasta);
    cpu_dados(cpu)->atual = p;
    cpu_dados(cpu)->fatia = QUANTUM_TIQUES;
    cpu_dados(cpu)->ultimo = 0;
}

void processos_iniciar(void)
{
    /* El código que está corriendo ahora (el fin de la inicialización del kernel) pasa a ser
     * el proceso 0, el "ocioso". Usa la pila del arranque y su cuadro se
     * rellena en la primera interrupción del reloj. */
    processo_t *p = &tabela[0];
    memset(p, 0, sizeof *p);
    p->pid = 0;
    p->estado = P_EXECUTANDO;
    txt_copia_n(p->nome, "ocioso", sizeof p->nome);
    p->espaco = espaco_kernel();
    txt_copia_n(p->pasta, "/", sizeof p->pasta);
    atual_global = p;
    cpu_dados(0)->atual = p;
    cpu_dados(0)->fatia = QUANTUM_TIQUES;
    cpu_dados(0)->ultimo = 0;
    idt_registrar(VETOR_CEDER, ao_ceder);
    pilha_iniciar();   /* D186: el fallo de página hace crecer las pilas */
}

processo_t *processo_indice(int i) { return (i >= 0 && i < MAX_PROCESSOS) ? &tabela[i] : NULL; }

processo_t *processo_por_pid(int pid)
{
    for (int i = 0; i < MAX_PROCESSOS; i++)
        if (tabela[i].estado != P_LIVRE && tabela[i].pid == pid)
            return &tabela[i];
    return NULL;
}

processo_t *processo_novo(const char *nome, int pai)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = &tabela[i];
        if (p->estado != P_LIVRE) continue;
        uint32_t pilha = memfisica_alocar_seguidas(PILHA_KERNEL / PAGINA);
        if (!pilha) return NULL;
        memset(p, 0, sizeof *p);
        p->pid = proximo_pid++;
        p->pai = pai;
        p->estado = P_BLOQUEADO;   /* sólo queda listo cuando el cuadro inicial esté armado */
        p->espera = p;
        txt_copia_n(p->nome, nome, sizeof p->nome);
        p->pilha_kernel = pilha;
        p->espaco = espaco_kernel();
        p->mmio_fim = USUARIO_MMIO;   /* los mapas de dispositivos empiezan aquí (D158) */
        txt_copia_n(p->pasta, "/", sizeof p->pasta);
        p->como_uid = p->como_gid = -1;   /* D255: sin identidad pendiente para los hijos */
        p->umask = 022;                   /* D282: UMASK_PADRAO; los hijos heredan la del padre */
        planificador_herdar(p, pai);   /* D174: máscara de CPU y nice del padre */
        return p;
    }
    return NULL;
}

/* Una tarea del kernel que termina cae aquí. Se vuelve zombi y espera el próximo
 * tic, que nunca más la elige. */
static void fim_tarefa(void)
{
    int_desliga();
    processo_encerrar(processo_atual(), 0, NULL);
    int_liga();
    for (;;) cpu_espera();
}

processo_t *tarefa_kernel(const char *nome, void (*funcao)(void *), void *arg)
{
    processo_t *p = processo_novo(nome, 0);
    if (!p) return NULL;
    /* En el anillo 0 el iret sólo desapila EIP, CS y EFLAGS. Los dos campos
     * siguientes del cuadro (esp_usuario y ss_usuario) quedan en el tope de la pila
     * cuando empieza la función: se convierten en la dirección de retorno y el argumento. */
    quadro_t *q = (quadro_t *)(p->pilha_kernel + PILHA_KERNEL - sizeof(quadro_t));
    memset(q, 0, sizeof *q);
    /* O GS de uma tarefa do kernel e o da CPU: e assim que a tarefa sabe
     * que processo e (D157). */
    q->ds = q->es = q->fs = SEL_KDADOS;
    q->gs = SEL_GS_CPU;
    q->cs = SEL_KCODIGO;
    q->eip = (uint32_t)funcao;
    q->eflags = 0x202;                     /* IF=1: la tarea puede ser interrumpida */
    q->esp_usuario = (uint32_t)fim_tarefa; /* "dirección de retorno" de la función */
    q->ss_usuario = (uint32_t)arg;         /* primer argumento */
    p->quadro = q;
    p->estado = P_PRONTO;
    p->espera = NULL;
    planificador_pronto_novo(p);   /* D174 */
    avisar_ociosa(p);
    return p;
}

/* D167: ¿alguna CPU tiene elegido un hilo del proceso "l" (el principal
 * incluido)? Mientras la tenga, su pila de kernel y su espacio siguen en
 * uso. */
bool grupo_na_cpu(const processo_t *l)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        const processo_t *t = &tabela[i];
        if (t->estado != P_LIVRE && (t == l || t->lider == l) && t->na_cpu) return true;
    }
    return false;
}

/* D167: ¿alguna CPU que no es esta corre un hilo de "l"? Esta CPU solo
 * puede tener al que llama (processo_atual), así que basta mirar los demás. */
static bool grupo_noutra_cpu(const processo_t *l)
{
    processo_t *eu = processo_atual();
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        const processo_t *t = &tabela[i];
        if (t->estado != P_LIVRE && (t == l || t->lider == l) && t->na_cpu && t != eu) return true;
    }
    return false;
}

/* D167: los hilos vivos del proceso "l", el principal incluido. */
int hilos_contar(const processo_t *l)
{
    int n = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        const processo_t *t = &tabela[i];
        if ((t == l || t->lider == l) && t->estado != P_LIVRE && t->estado != P_ZUMBI) n++;
    }
    return n;
}

/* D167: el espacio del proceso "l" ya se destruyó: nadie del grupo lo
 * apunta más (los hilos guardan una copia del valor). */
static void grupo_sem_espaco(processo_t *l)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = &tabela[i];
        if (t->estado != P_LIVRE && (t == l || t->lider == l)) t->espaco = espaco_kernel();
    }
    l->espaco_pendente = false;
}

void processo_liberar(processo_t *p)
{
    if (!p->lider) {
        /* D167: con el proceso se van los hilos que quedaban: todos zombis y
         * fuera de toda CPU (quien llama comprobó grupo_na_cpu). */
        for (int i = 1; i < MAX_PROCESSOS; i++) {
            processo_t *t = &tabela[i];
            if (t->lider != p || t->estado == P_LIVRE) continue;
            if (t->na_cpu || t->estado != P_ZUMBI) {
                klog("[SAGUI] hilo %d de %d todavia en uso al liberar el proceso\n", t->pid, p->pid);
                continue;
            }
            processo_liberar(t);
        }
    } else if (p->lider->estado != P_LIVRE) {
        p->lider->tiques_cpu += p->tiques_cpu;   /* el tiempo del hilo cuenta en el proceso */
    }
    fpu_esquecer(p);
    if (!p->lider && p->espaco_pendente && p->espaco != espaco_kernel()) {
        espaco_destruir(p->espaco);   /* lo terminó otra CPU y nadie lo destruyó aún (D165) */
        p->espaco = espaco_kernel();
    }
    p->espaco_pendente = false;
    pilha_vaga_liberar(p);   /* D186: el hueco de su pila queda para otro hilo */
    p->lider = NULL;
    if (p->pilha_kernel)
        memfisica_liberar_seguidas(p->pilha_kernel, PILHA_KERNEL / PAGINA);
    p->pilha_kernel = 0;
    p->estado = P_LIVRE;
}

/* D167: termina los hilos del proceso "l" (no el principal). Los que corren
 * en otra CPU se sacan antes de su código, como un proceso (D165); el que
 * llama, si es uno de ellos, queda zombi y lo deja escalonar() al volver.
 * Ninguno se queda esperando que lo recojan: pai = 0. */
static void hilos_encerrar(processo_t *l, int codigo)
{
    processo_t *eu = processo_atual();
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = &tabela[i];
        if (t->lider != l || t->estado == P_LIVRE) continue;
        if (t->estado != P_ZUMBI) {
            if (t != eu && t->na_cpu) processo_tirar_da_cpu(t);
            fpu_esquecer(t);
            t->codigo_saida = codigo;
            t->estado = P_ZUMBI;
            t->espera = NULL;
            t->acordar_em = 0;
        }
        t->pai = 0;
    }
}

/* Termina un proceso (él mismo u otro). La memoria del programa y los
 * archivos se liberan ya; la pila del kernel y la entrada en la tabla quedan
 * hasta que el padre recoja el código de salida con "esperar".
 * D167: terminar un hilo (salir, un fallo, matar su número) termina el
 * proceso entero, con todos sus hilos; un hilo solo sale con hilo_sair. */
void processo_encerrar(processo_t *p, int codigo, const char *motivo)
{
    p = processo_lider(p);
    if (p->estado == P_ZUMBI || p->estado == P_LIVRE || p->pid == 0) return;
    hilos_encerrar(p, codigo);
    /* D165: si corre en otra CPU (en modo usuario, o en el kernel esperando
     * el cerrojo o durmiendo en cpu_espera), primero se la saca de su código:
     * después ya no toca su memoria y se puede cerrar todo. Su espacio de
     * direcciones se destruye cuando esa CPU lo deje (su CR3 aún lo usa). */
    bool noutra = p != processo_atual() && p->na_cpu;
    if (noutra) processo_tirar_da_cpu(p);
    fpu_esquecer(p);
    p->codigo_saida = codigo;
    if (motivo) txt_copia_n(p->motivo, motivo, sizeof p->motivo);
    /* Matar a otro proceso: deja de existir ahora, antes de cerrar los
     * archivos. Cerrar puede esperar un bloqueo (D095) y, mientras tanto, el
     * proceso todavía vivo, durmiendo en medio de una lectura del pendrive,
     * terminaría la operación, volvería a correr y cerraría los mismos archivos. */
    if (p != processo_atual()) { p->estado = P_ZUMBI; p->espera = NULL; p->acordar_em = 0; }
    arquivos_fechar_todos(p);
    janelas_do_processo_fechar(p);   /* antes de destruir el espacio: las páginas de las ventanas son del gestor */
    if (p->usuario && p->espaco != espaco_kernel()) {
        /* D167: otra CPU con un hilo suyo aún tiene el espacio en el CR3. */
        if (grupo_noutra_cpu(p)) {
            p->espaco_pendente = true;    /* lo destruye escalonar() al dejarlo la última CPU */
        } else {
            espaco_destruir(p->espaco);   /* cambia el CR3 al del kernel si es el actual */
            grupo_sem_espaco(p);
        }
    }
    p->estado = P_ZUMBI;
    p->espera = NULL;
    /* Los hijos quedan huérfanos: pasan al proceso 0, que los recoge. */
    for (int i = 1; i < MAX_PROCESSOS; i++)
        if (tabela[i].estado != P_LIVRE && tabela[i].pai == p->pid)
            tabela[i].pai = 0;
    acordar(processo_por_pid(p->pai));   /* el padre puede estar en "esperar" */
    klog("[SAGUI] processo %u (%s) terminou com codigo %d%s%s\n", p->pid, p->nome, codigo,
         motivo ? ": " : "", motivo ? motivo : "");
}

/* Una tarea del kernel no pasa por "int 0x80"; para dormir de verdad se
 * bloquea y cede el turno por esta interrupción de software, que va directo al
 * planificador. Sin eso, una tarea que despierta en cada tic quedaría siempre
 * lista y el proceso 0 nunca volvería a correr. */
quadro_t *ao_ceder(quadro_t *q)
{
    return escalonar(q);
}

void ceder(void)
{
    __asm__ volatile("int %0" : : "i"(VETOR_CEDER) : "memory");
}

void bloquear(const void *canal)
{
    processo_t *atual = processo_atual();
    if (!atual) return;
    atual->estado = P_BLOQUEADO;
    atual->espera = canal;
}

void acordar(const void *canal)
{
    if (!canal) return;
    for (int i = 0; i < MAX_PROCESSOS; i++)
        if (tabela[i].estado == P_BLOQUEADO && tabela[i].espera == canal) {
            tabela[i].estado = P_PRONTO;
            tabela[i].espera = NULL;
            avisar_ociosa(&tabela[i]);
        }
}

/* D165: hay trabajo para "p": si hay una CPU ociosa que puede tomarlo, se
 * la despierta con una IPI al salir del kernel (kernel_saida), cuando el
 * cerrojo ya está libre. Cuál, lo dice el planificador (D174): las que su
 * máscara permite (las tareas del kernel, la CPU 0), la suya primero. */
static void acordar_ociosa(const processo_t *p)
{
    if (!smp_arrancadas() || !p || !cpu_gs_lista()) return;
    uint32_t eu = cpu_atual();
    cpu_dados_t *d = cpu_dados(eu);
    int c = planificador_ociosa_para(p, eu, d->acordar);   /* las ya avisadas, para otro */
    if (c >= 0) d->acordar |= 1u << c;
}

/* "p" quedó listo (se despertó, se creó o lo soltaron). Además de despertar
 * una CPU ociosa, el planificador "prioridad" puede quitarle la CPU a uno
 * peor (D174). */
void avisar_ociosa(processo_t *p)
{
    if (!p) return;
    planificador_despertou(p);
    acordar_ociosa(p);
}

/* D165: saca a "p" de la CPU donde corre (otra): una IPI hace que esa CPU
 * entre en el kernel, y allí espera el cerrojo, que tenemos nosotros. En
 * cuanto espera, ya no ejecuta código del programa. Se llama con el
 * cerrojo. Por si la IPI se perdiera, la espera tiene un límite. */
void processo_tirar_da_cpu(processo_t *p)
{
    uint32_t c = p->cpu;
    if (!p->na_cpu || c >= CPU_MAX || c == cpu_atual()) return;
    cpu_dados_t *d = cpu_dados(c);
    smp_acordar(c);
    for (uint32_t i = 0; d->atual == p && !d->esperando_trava; i++) {
        __asm__ volatile("pause" ::: "memory");
        if (i == 200000000u) {
            klog("[SMP] la CPU %u no respondio al sacar el proceso %u\n", c, p->pid);
            break;
        }
    }
}

/* D165: otra CPU puede tener en su TLB páginas de "e" que acabamos de
 * quitar (una ventana que se cierra mientras su programa corre en otro
 * núcleo). Se la saca del programa y, al tomar el cerrojo, recarga el CR3
 * antes de seguir. Se llama con el cerrojo, después de quitar las páginas
 * y antes de liberarlas. */
void espaco_sincronizar(espaco_t e)
{
    if (!smp_arrancadas()) return;
    for (uint32_t c = 0; c < CPU_MAX; c++) {
        if (c == cpu_atual()) continue;
        processo_t *p = cpu_dados(c)->atual;
        if (!p || p->pid == 0 || !p->na_cpu || p->espaco != e) continue;
        cpu_dados(c)->recarregar_cr3 = 1;
        processo_tirar_da_cpu(p);
    }
}

/* Qué puede tomar cada CPU (D165, ahora en planificadores.c):
 *   - listo, y que no lo tenga otra CPU (na_cpu);
 *   - las tareas del kernel solo en la CPU 0: hablan con dispositivos cuyas
 *     IRQ llegan a la CPU 0 y algunas esperan con las interrupciones puestas;
 *   - un programa, en las CPU de su máscara (D174), también si quedó parado
 *     dentro del kernel: la CPU que lo retoma tiene el cerrojo. */

/* Elige el proceso siguiente de esta CPU. Siempre con el cerrojo general
 * (D165): dos CPUs no eligen el mismo proceso. */
quadro_t *escalonar(quadro_t *q)
{
    uint32_t cpu = cpu_atual();
    cpu_dados_t *d = cpu_dados(cpu);
    processo_t *atual = processo_atual();
    if (!atual) return q;   /* todavía no hay procesos */

    atual->quadro = q;
    if (atual->estado == P_EXECUTANDO)
        atual->estado = P_PRONTO;

    /* Recoge zombis que nadie va a esperar (huérfanos y tareas del kernel):
     * ninguno que una CPU tenga todavía (D165). */
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *z = &tabela[i];
        if (z->estado == P_ZUMBI && z->pai == 0 && z != atual && !z->na_cpu &&
            (z->lider || !grupo_na_cpu(z)))   /* D167: un proceso, cuando ningún hilo suyo está en una CPU */
            processo_liberar(z);
    }

    /* El siguiente: lo elige la política activa (D174). */
    processo_t *prox = planificador_escolher(cpu, atual);
    if (prox) {
        planificador_tomou(prox, cpu);   /* D174: migraciones, su cola, la latencia */
        prox->estado = P_EXECUTANDO;
        prox->cpu = cpu;
        prox->na_cpu = true;
        d->ultimo = (int)(prox - tabela);
    }

    if (!prox) {
        /* Nadie listo: esta CPU se queda en su propio ocioso (D157). O
         * quadro e o do ocioso (o que ficou guardado quando o ocioso foi
         * interrompido), e nao o do processo que acabou de ceder: se fosse
         * este, o processo voltava a ser escolhido de imediato. */
        processo_t *ocioso = ocioso_da_cpu(cpu);
        if (ocioso != atual) {
            ocioso->estado = P_EXECUTANDO;
            ocioso->cpu = cpu;
        }
        d->estado = CPU_OCIOSA;
        d->fatia = QUANTUM_TIQUES;
        d->atual = ocioso;
        /* Con varias CPU, una CPU ociosa no se queda con el espacio de un
         * programa: otra CPU puede destruirlo (D164, D165). */
        if (smp_arrancadas() && espaco_atual() != espaco_kernel())
            espaco_ativar(espaco_kernel());
        /* D178: también al quedarse ociosa se guarda la FPU del proceso que
         * deja: si no, sus registros se quedan en esta CPU y otra CPU que lo
         * recoja carga un área vieja (o a ceros: MXCSR 0, #XM en la primera
         * instrucción SSE, como vio el benchmark de la D176). */
        fpu_trocar(ocioso);
    } else {
        d->estado = CPU_PRONTA;
        d->fatia = planificador_fatia(prox);   /* D174: la de su prioridad */
        if (prox->usuario)
            tss_definir_pilha(prox->pilha_kernel + PILHA_KERNEL);
        /* Con varias CPU el CR3 se recarga siempre: otra CPU pudo cambiar las
         * tablas del programa mientras corría allí, y la TLB de esta no se
         * entera (D164). */
        if (prox->espaco != espaco_atual() || smp_arrancadas())
            espaco_ativar(prox->espaco);
        fpu_trocar(prox);
        d->atual = prox;
    }

    /* Esta CPU deja "atual": otra puede tomarlo. Con el cerrojo general eso
     * no puede pasar hasta que esta CPU lo suelte en kernel_saida, ya sobre
     * la pila del marco que reanuda: la pila de "atual" ya no se usa (D165). */
    if (prox != atual && atual->pid != 0) {
        atual->na_cpu = false;
        atual->saiu_em = relogio_tiques();   /* D174: desde aquí su caché se enfría */
        if (atual->estado == P_ZUMBI) {
            /* Lo terminó otra CPU mientras corría aquí: su espacio se destruye
             * ahora, que el CR3 de esta CPU ya es otro, si ninguna otra CPU
             * corre un hilo suyo (D167); y el padre puede recogerlo. */
            processo_t *l = processo_lider(atual);
            if (l->espaco_pendente && !grupo_na_cpu(l)) {
                espaco_destruir(l->espaco);
                grupo_sem_espaco(l);
            }
            if (atual->lider) acordar(atual);   /* D167: quien espera este hilo (hilo_esperar) */
            acordar(processo_por_pid(l->pai));
        } else if (atual->estado == P_PRONTO) {
            acordar_ociosa(atual);   /* otra CPU ociosa puede seguir con él */
        }
    }
    if (!prox) {
        processo_t *ocioso = ocioso_da_cpu(cpu);
        return ocioso->quadro ? ocioso->quadro : q;
    }
    return prox->quadro;
}

quadro_t *escalonador_tique(quadro_t *q)
{
    uint32_t agora = relogio_tiques();
    uint32_t cpu = cpu_atual();
    cpu_dados_t *d = cpu_dados(cpu);
    processo_t *atual = processo_atual();
    if (!atual) return q;
    atual->tiques_cpu++;
    d->tiques++;
    if (atual->pid == 0)
        d->tiques_ociosos++;   /* esta CPU no tenía ningún proceso en marcha (D157) */
#ifdef TRAVA_MEDIR
    trava_geral_relatorio(agora);   /* D169: diagnóstico del cerrojo general */
#endif

    /* Los que estaban dormidos y ya les toca (con el cerrojo general, D165). */
    for (int i = 0; i < MAX_PROCESSOS; i++) {
        processo_t *p = &tabela[i];
        if (p->estado == P_BLOQUEADO && p->acordar_em && (int32_t)(agora - p->acordar_em) >= 0) {
            p->acordar_em = 0;
            p->estado = P_PRONTO;
            p->espera = NULL;
            avisar_ociosa(p);
        }
    }

    planificador_tique0(agora);   /* D174: el uso reciente y el reparto de la carga */

    /* Cambia cuando se acaba la rebanada, o al instante si el ocioso está corriendo y
     * alguien quedó listo, o si el planificador lo pidió (D174). */
    if (atual->estado != P_EXECUTANDO || --d->fatia <= 0 || atual->pid == 0 ||
        planificador_troca_pendente(cpu))
        return escalonar(q);   /* bloqueado (ej.: tarea del kernel durmiendo), fin de la rebanada, u ocioso */
    return q;
}

/* El tic de un procesador secundario (D165), sin el cerrojo general: casi
 * siempre solo cuenta el tiempo, que es de esta CPU. Solo si hay que
 * planificar (se acabó la rebanada, el proceso ya no corre o la CPU está
 * ociosa) intenta tomarlo; si otra CPU lo tiene, se vuelve a intentar en el
 * tic siguiente en vez de parar aquí. Los dormidos los despierta el tic de
 * la CPU 0. */
quadro_t *escalonador_tique_secundaria(quadro_t *q)
{
    cpu_dados_t *d = cpu_dados(cpu_atual());
    processo_t *atual = processo_atual();
    if (!atual) return q;
    atual->tiques_cpu++;
    d->tiques++;
    if (atual->pid == 0)
        d->tiques_ociosos++;
    bool planificar = atual->pid == 0 || atual->estado != P_EXECUTANDO;
    if (d->fatia > 0) d->fatia--;
    if (d->fatia <= 0) planificar = true;
    if (planificador_troca_pendente(cpu_atual())) planificar = true;   /* D174 */
    if (!planificar || !trava_geral_tentar())
        return q;
    return escalonar(q);
}

/* Cuadro inicial de un programa en el anillo 3: segmentos de usuario, la pila
 * del programa y el punto de entrada. El iret con cambio de anillo desapila
 * también ESP y SS, y la CPU pasa a ejecutar el programa sin privilegios. */
void processo_preparar_usuario(processo_t *p, uint32_t entrada, uint32_t pilha)
{
    quadro_t *q = (quadro_t *)(p->pilha_kernel + PILHA_KERNEL - sizeof(quadro_t));
    memset(q, 0, sizeof *q);
    q->ds = q->es = q->fs = q->gs = SEL_UDADOS;
    q->cs = SEL_UCODIGO;
    q->ss_usuario = SEL_UDADOS;
    q->esp_usuario = pilha;
    q->eip = entrada;
    q->eflags = 0x202;   /* IF=1, IOPL=0: el programa no puede usar cli/sti ni puertos */
    p->quadro = q;
    p->usuario = true;
    p->estado = P_PRONTO;
    p->espera = NULL;
    planificador_pronto_novo(p);   /* D174: equilibrio la pone en la CPU menos cargada */
    avisar_ociosa(p);   /* D165: una CPU ociosa puede empezarlo ya */
}
