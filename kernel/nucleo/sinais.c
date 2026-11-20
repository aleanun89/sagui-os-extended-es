// SPDX-License-Identifier: GPL-3.0-only
/* sinais.c - señales POSIX (D401).
 *
 * Qué hay en cada sitio (processo.h):
 *   - en el líder (el proceso): las 32 acciones (tratador, máscara, banderas, restaurador), las
 *     pendientes dirigidas al proceso, quién mandó cada una y la alarma;
 *   - en cada hilo: su máscara, sus pendientes (las síncronas y las de SINOP_HILO), su pila
 *     alternativa y lo de sigsuspend.
 *
 * Enviar (sinais_enviar): una ignorada se descarta; una que termina por defecto y que ningún hilo
 * bloquea termina el proceso ya (como el Ctrl+C de siempre, que también mata a un proceso
 * dormido dentro del kernel); las demás quedan pendientes y se despierta a un hilo que no la
 * bloquee: si duerme en una llamada que se repite (SIS_REPETIR, su cuadro guardado es de
 * usuario) pasa a listo; si corre en otra CPU, una IPI lo hace entrar en el kernel. Un hilo
 * dormido DENTRO del kernel (un disco, una trava: su cuadro es del anillo 0) no se toca: recibe
 * la señal cuando su llamada termina.
 *
 * Entregar (sinais_ao_sair): solo al volver al anillo 3, con el cerrojo general, desde el final
 * de tratar_interrupcao. Si la llamada se había bloqueado con SIS_REPETIR, con SA_RESTART se
 * repetirá al volver del tratador y sin él devuelve ERRO_INTERROMPIDO (EINTR). El marco va en la
 * pila del programa (o en la alternativa), de arriba abajo: el área de la FPU alineada a 64 (si
 * el hilo la usó), el contexto (contexto_sinal_t, el ucontext_t de Linux i386), el info
 * (info_sinal_t, el siginfo_t de 128 bytes) y cuatro palabras: la vuelta (el restaurador de la
 * libc, que llama a SIS_SINAL_VOLTAR), la señal, &info y &contexto; (esp + 4) queda alineado a
 * 16, como al entrar en una función. Nada se ejecuta en la pila (NX, D349).
 *
 * Volver (sinais_voltar, sigreturn): el contexto se restaura saneado (selectores de usuario,
 * solo los bits de EFLAGS que un programa puede cambiar, el área de la FPU sin nada que dé #GP
 * al cargarla). No da más poder que poner los registros a mano. */
#include "processo.h"
#include "sistema.h"
#include "console.h"
#include "texto.h"
#include "relogio.h"
#include "smp.h"
#include "cpu_dados.h"
#include "fpu.h"

#define BIT(s)          (1u << (s))
#define NAO_BLOQUEAVEIS (BIT(SINAL_KILL) | BIT(SINAL_STOP))
#define EFLAGS_USUARIO  0x40DD5u   /* CF PF AF ZF SF TF DF OF AC */
#define EFLAGS_TF       0x100u
#define EFLAGS_DF       0x400u
#define EFLAGS_IF       0x200u
#define PILHA_ALT_MIN   2048u      /* MINSIGSTKSZ */

static const char canal_sinais = 0;   /* sigsuspend */

enum { DFL_TERMINAR, DFL_CORE, DFL_IGNORAR };

static int padrao(int s)
{
    switch (s) {
    case SINAL_CHLD: case SINAL_URG: case SINAL_WINCH: case SINAL_CONT:
    /* Parar y continuar no existen aún (D401): se ignoran. */
    case SINAL_STOP: case SINAL_TSTP: case SINAL_TTIN: case SINAL_TTOU:
        return DFL_IGNORAR;
    case SINAL_QUIT: case SINAL_ILL: case SINAL_TRAP: case SINAL_ABRT: case SINAL_BUS:
    case SINAL_FPE: case SINAL_SEGV: case SINAL_XCPU: case SINAL_XFSZ: case SINAL_SYS:
        return DFL_CORE;
    }
    return DFL_TERMINAR;
}

static const char *nome_sinal(int s)
{
    static const char *const n[SINAIS_N] = {
        "0", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
        "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT",
        "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF",
        "SIGWINCH", "SIGIO", "SIGPWR", "SIGSYS",
    };
    return s > 0 && s < SINAIS_N ? n[s] : "?";
}

/* ¿Ignorada (por la acción o por defecto)? Se descarta al generarla. */
static bool ignorada(const processo_t *l, int s)
{
    uint32_t t = l->sinais_acoes[s].tratador;
    return t == SINAL_TRATADOR_IGN || (t == SINAL_TRATADOR_DFL && padrao(s) == DFL_IGNORAR);
}

/* ------------------------------------------------------------------ procesos nuevos */

void sinais_iniciar_processo(processo_t *p, const processo_t *de, bool fork)
{
    if (fork && de) {
        const processo_t *l = processo_lider((processo_t *)de);
        memcpy(p->sinais_acoes, l->sinais_acoes, sizeof p->sinais_acoes);
        p->sinais_mascara = de->sinais_mascara;   /* la del hilo que llama */
        p->sinais_pilha = de->sinais_pilha;
        p->sinais_pilha.flags &= ~(uint32_t)SINAL_PILHA_EM_USO;
        return;   /* pendientes, alarma y lo demás empiezan a cero (processo_novo) */
    }
    /* Lanzado con executar(): todo por defecto, salvo SIGPIPE, ignorada: los programas de SaguiOS
     * esperan el error de tubería rota (servicios, sshd, vncd, cron...). Ver D401. */
    p->sinais_acoes[SINAL_PIPE].tratador = SINAL_TRATADOR_IGN;
}

void sinais_exec(processo_t *l)
{
    for (int s = 1; s < SINAIS_N; s++) {
        acao_sinal_t *a = &l->sinais_acoes[s];
        if (a->tratador != SINAL_TRATADOR_IGN) a->tratador = SINAL_TRATADOR_DFL;
        a->mascara = 0;
        a->flags = 0;
        a->restaurador = 0;
    }
    memset(&l->sinais_pilha, 0, sizeof l->sinais_pilha);
    l->sinais_suspenso = false;
}

/* ------------------------------------------------------------------ enviar */

/* Termina el proceso por la señal "s" (su acción por defecto, o SIGKILL). */
static void terminar_por(processo_t *l, int s, const char *motivo)
{
    char m[64];
    if (!motivo) {
        if (s == SINAL_INT) motivo = "interrumpido con Ctrl+C";
        else { snformatar(m, sizeof m, "terminado por la señal %s", nome_sinal(s)); motivo = m; }
    }
    l->sinal_fim = s;
    processo_encerrar(l, s == SINAL_INT ? SAIDA_INTERROMPIDO : SAIDA_ENCERRADO_PELO_KERNEL, motivo);
}

static bool hilo_vivo(const processo_t *t)
{
    return t->estado != P_LIVRE && t->estado != P_ZUMBI;
}

/* ¿La bloquean todos los hilos vivos del proceso? */
static bool bloqueada_por_todos(processo_t *l, int s)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = processo_indice(i);
        if ((t == l || t->lider == l) && hilo_vivo(t) && !(t->sinais_mascara & BIT(s))) return false;
    }
    return true;
}

/* Que "t" mire sus señales pronto: si duerme en una llamada que se repite, pasa a listo; si corre
 * en otra CPU, una IPI la hace entrar en el kernel (y la entrega es al salir). */
static void despertar(processo_t *t)
{
    if (t->estado == P_BLOQUEADO && t->quadro && (t->quadro->cs & 3) == 3) {
        t->estado = P_PRONTO;
        t->espera = NULL;
        t->acordar_em = 0;
        t->futex = 0;
        avisar_ociosa(t);
    } else if (t->na_cpu && t->cpu != cpu_atual()) {
        smp_acordar(t->cpu);
    }
}

/* El hilo que mejor puede tomar "s": uno que no la bloquee, mejor el que ya corre. */
static processo_t *hilo_para(processo_t *l, int s)
{
    processo_t *melhor = NULL;
    int nota = -1;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = processo_indice(i);
        if (!(t == l || t->lider == l) || !hilo_vivo(t) || (t->sinais_mascara & BIT(s))) continue;
        int n = t->estado == P_EXECUTANDO ? 3 : t->estado == P_PRONTO ? 2 :
                (t->quadro && (t->quadro->cs & 3) == 3) ? 1 : 0;
        if (n > nota) { nota = n; melhor = t; }
    }
    return melhor;
}

static int enviar(processo_t *p, int s, int pid_origem, uint32_t uid_origem, int codigo, bool pode_terminar)
{
    processo_t *l = processo_lider(p);
    if (!l || !l->usuario || l->pid == 0 || l->estado == P_ZUMBI || l->estado == P_LIVRE) return ERRO_SEM_PROCESSO;
    if (s == 0) return 0;
    if (s < 1 || s >= SINAIS_N) return ERRO_INVALIDO;
    if (s == SINAL_KILL) {
        if (pode_terminar) terminar_por(l, s, NULL);
        else l->sinais_pendentes |= BIT(s);
        return 0;
    }
    if (ignorada(l, s)) return 0;
    if (pode_terminar && l->sinais_acoes[s].tratador == SINAL_TRATADOR_DFL && !bloqueada_por_todos(l, s)) {
        terminar_por(l, s, NULL);
        return 0;
    }
    l->sinais_pendentes |= BIT(s);
    l->sinais_origem[s].pid = pid_origem;
    l->sinais_origem[s].uid = (uint16_t)uid_origem;
    l->sinais_origem[s].codigo = (uint16_t)codigo;
    processo_t *t = hilo_para(l, s);
    if (t) despertar(t);
    return 0;
}

int sinais_enviar(processo_t *p, int s, int pid_origem, uint32_t uid_origem, int codigo)
{
    return enviar(p, s, pid_origem, uid_origem, codigo, true);
}

int sinais_enviar_hilo(processo_t *t, int s, int pid_origem, uint32_t uid_origem, int codigo)
{
    processo_t *l = processo_lider(t);
    if (!l->usuario || !hilo_vivo(t)) return ERRO_SEM_PROCESSO;
    if (s == 0) return 0;
    if (s < 1 || s >= SINAIS_N) return ERRO_INVALIDO;
    if (s == SINAL_KILL) { terminar_por(l, s, NULL); return 0; }
    if (ignorada(l, s)) return 0;
    if (l->sinais_acoes[s].tratador == SINAL_TRATADOR_DFL && !(t->sinais_mascara & BIT(s))) {
        terminar_por(l, s, NULL);
        return 0;
    }
    t->sinais_hilo |= BIT(s);
    l->sinais_origem[s].pid = pid_origem;
    l->sinais_origem[s].uid = (uint16_t)uid_origem;
    l->sinais_origem[s].codigo = (uint16_t)codigo;
    if (t != processo_atual()) despertar(t);
    return 0;
}

void sinais_filho_terminou(processo_t *f)
{
    processo_t *pai = processo_por_pid(f->pai);
    if (!pai || pai->lider || !pai->usuario || pai->estado == P_ZUMBI || pai->estado == P_LIVRE) return;
    const acao_sinal_t *a = &pai->sinais_acoes[SINAL_CHLD];
    if (a->tratador == SINAL_TRATADOR_IGN || (a->flags & SINAL_SA_NOCLDWAIT)) {
        /* Como Linux: nadie lo va a esperar; lo recoge el proceso 0. Un waitpid dormido mira de nuevo. */
        f->pai = 0;
        acordar(pai);
        if (a->tratador == SINAL_TRATADOR_IGN) return;
    }
    int codigo = f->sinal_fim ? SINAL_CLD_KILLED : SINAL_CLD_EXITED;
    enviar(pai, SINAL_CHLD, f->pid, f->uid, codigo, true);
}

void sinais_tique(uint32_t agora)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *l = processo_indice(i);
        if (l->lider || !l->alarme_em || !hilo_vivo(l) || (int32_t)(agora - l->alarme_em) < 0) continue;
        l->alarme_em = 0;
        enviar(l, SINAL_ALRM, 0, 0, SINAL_SI_KERNEL, false);   /* en el tic no se cierra nada: se entrega al salir */
    }
}

int32_t espera_estado(const processo_t *p)
{
    if (p->sinal_fim) return (p->sinal_fim & 0x7F) | (padrao(p->sinal_fim) == DFL_CORE ? 0x80 : 0);
    if (p->codigo_saida == SAIDA_ENCERRADO_PELO_KERNEL) return SINAL_KILL;   /* lo terminó el kernel (matar) */
    return (p->codigo_saida & 0xFF) << 8;
}

/* ------------------------------------------------------------------ entregar */

static bool na_pilha_alt(const processo_t *t, uint32_t sp)
{
    const pilha_sinal_t *p = &t->sinais_pilha;
    return p->base && sp > p->base && sp <= p->base + p->tamanho;
}

/* Monta el marco del tratador de "s" en la pila del hilo y deja el cuadro listo para entrar en
 * él. false si la memoria no vale (la pila desbordada, sin pila alternativa). */
static bool montar_marco(processo_t *t, quadro_t *q, int s, const acao_sinal_t *a, const info_sinal_t *info,
                         uint32_t mascara_uc)
{
    uint32_t topo = q->esp_usuario;
    const pilha_sinal_t *alt = &t->sinais_pilha;
    if ((a->flags & SINAL_SA_ONSTACK) && alt->base && !(alt->flags & SINAL_PILHA_DESLIGADA) &&
        !na_pilha_alt(t, q->esp_usuario))
        topo = alt->base + alt->tamanho;
    uint32_t sp = topo, fpu = 0, formato = 0;
    if (t->fpu_usada && fpu_formato()) {
        fpu_guardar(t);   /* los registros, al área (el próximo #NM la vuelve a cargar) */
        sp = (sp - FPU_ESTADO) & ~(uint32_t)(FPU_ESTADO_ALINHA - 1);
        fpu = sp;
        formato = fpu_formato();
    }
    sp = (sp - sizeof(contexto_sinal_t)) & ~15u;
    uint32_t uc = sp;
    sp -= sizeof(info_sinal_t);
    uint32_t inf = sp;
    sp = ((sp - 16u) & ~15u) - 4u;   /* cuatro palabras; (esp + 4) alineado a 16 */
    if (sp < USUARIO_INICIO || sp > topo || !usuario_ok((void *)sp, topo - sp, true)) return false;

    if (fpu) {
        memcpy((void *)fpu, t->fpu, FPU_ESTADO);
        /* Revisión D401: el tratador empieza con la FPU limpia, como en Linux. Con la del código
         * interrumpido, un SIGFPE de la x87 (#MF) dejaba la excepción pendiente y la primera
         * instrucción x87 del tratador volvía a fallar (SIGFPE bloqueada: el proceso moría).
         * sigreturn la restaura desde fpregs. */
        t->fpu_usada = false;
    }
    contexto_sinal_t *c = (contexto_sinal_t *)uc;
    memset(c, 0, sizeof *c);
    c->pilha_base = alt->base;
    c->pilha_flags = alt->base ? (na_pilha_alt(t, q->esp_usuario) ? SINAL_PILHA_EM_USO : alt->flags) : SINAL_PILHA_DESLIGADA;
    c->pilha_tamanho = alt->tamanho;
    uint32_t g[19] = { q->gs, q->fs, q->es, q->ds, q->edi, q->esi, q->ebp, q->esp_usuario, q->ebx, q->edx, q->ecx,
                       q->eax, q->vetor, q->erro, q->eip, q->cs, q->eflags, q->esp_usuario, q->ss_usuario };
    memcpy(c->gregs, g, sizeof g);
    c->fpregs = fpu;
    c->oldmask = mascara_uc;
    c->cr2 = info->u.endereco;
    c->mascara = mascara_uc;
    c->fpu_formato = formato;
    memcpy((void *)inf, info, sizeof *info);
    uint32_t palavras[4] = { a->restaurador, (uint32_t)s, inf, uc };
    memcpy((void *)sp, palavras, sizeof palavras);

    q->esp_usuario = sp;
    q->eip = a->tratador;
    q->eax = (uint32_t)s;   /* también en registros, para los tratadores regparm */
    q->edx = inf;
    q->ecx = uc;
    q->eflags = (q->eflags & ~(EFLAGS_TF | EFLAGS_DF)) | EFLAGS_IF;
    return true;
}

/* La llamada que dormía (SIS_REPETIR) y que ahora interrumpe una señal con tratador. Si el cuadro
 * se rebobinó para repetirla (repetindo != 0), el eip apunta al «int 0x80»: con EINTR se salta. */
static void interromper_chamada(processo_t *t, quadro_t *q, const acao_sinal_t *a)
{
    if (!t->repetindo) {
        /* No dormía (o ya volvió): solo sigsuspend, que encontró la señal al entrar, da EINTR. */
        if (t->sinais_suspenso) q->eax = (uint32_t)ERRO_INTERROMPIDO;
        return;
    }
    bool eintr = t->sinais_suspenso ||   /* sigsuspend vuelve siempre con EINTR */
                 (t->repetindo == 1 && (!(a->flags & SINAL_SA_RESTART) || q->eax == SIS_SONDAR));   /* poll nunca se repite */
    /* repetindo == 2: una espera interna (barrera D351): se repite siempre. */
    if (!eintr) return;
    q->eax = (uint32_t)ERRO_INTERROMPIDO;
    q->eip += 2;
}

/* Entrega "s" a "t" (que corre en esta CPU y vuelve al anillo 3 con "q"). Devuelve el cuadro a
 * reanudar (otro, si el proceso terminó). */
static quadro_t *entregar(processo_t *t, quadro_t *q, int s, const info_sinal_t *info)
{
    processo_t *l = processo_lider(t);
    acao_sinal_t a = l->sinais_acoes[s];
    if (s == SINAL_KILL) { terminar_por(l, s, NULL); return escalonar(q); }
    if (a.tratador == SINAL_TRATADOR_IGN) return q;
    if (a.tratador == SINAL_TRATADOR_DFL) {
        if (padrao(s) == DFL_IGNORAR) return q;
        terminar_por(l, s, NULL);
        return escalonar(q);
    }
    uint32_t mascara_uc = t->sinais_suspenso ? t->sinais_mascara_suspensa : t->sinais_mascara;
    uint32_t base = t->sinais_mascara;
    quadro_t guardado = *q;
    interromper_chamada(t, q, &a);
    if (!a.restaurador || !montar_marco(t, q, s, &a, info, mascara_uc)) {
        *q = guardado;
        char m[64];
        snformatar(m, sizeof m, "no se pudo entregar la señal %s (pila)", nome_sinal(s));
        kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: %s\n", l->pid, t->nome, m);
        terminar_por(l, SINAL_SEGV, m);
        return escalonar(q);
    }
    t->sinais_suspenso = false;
    t->repetindo = 0;
    t->sinais_mascara = (base | a.mascara | ((a.flags & SINAL_SA_NODEFER) ? 0 : BIT(s))) & ~NAO_BLOQUEAVEIS;
    if (a.flags & SINAL_SA_RESETHAND) {
        l->sinais_acoes[s].tratador = SINAL_TRATADOR_DFL;
        l->sinais_acoes[s].flags &= ~(uint32_t)SINAL_SA_SIGINFO;
    }
    return q;
}

quadro_t *sinais_ao_sair(quadro_t *q)
{
    for (int volta = 0; volta < 4; volta++) {
        processo_t *t = processo_atual();
        if (!t || t->pid == 0 || !t->usuario || (q->cs & 3) != 3 || t->estado != P_EXECUTANDO) return q;
        processo_t *l = processo_lider(t);
        uint32_t pend = (t->sinais_hilo | l->sinais_pendentes) & ~(t->sinais_mascara & ~NAO_BLOQUEAVEIS);
        if (!pend) return q;
        int s = __builtin_ctz(pend);
        info_sinal_t info;
        memset(&info, 0, sizeof info);
        info.signo = s;
        info.codigo = l->sinais_origem[s].codigo;
        info.u.proc.pid = l->sinais_origem[s].pid;
        info.u.proc.uid = l->sinais_origem[s].uid;
        if (t->sinais_hilo & BIT(s)) t->sinais_hilo &= ~BIT(s);
        else l->sinais_pendentes &= ~BIT(s);
        quadro_t *r = entregar(t, q, s, &info);
        if (r != q) return r;   /* terminó: otro proceso; sus señales, en su próxima salida */
    }
    return q;
}

/* Un fallo del procesador en el programa: la señal síncrona al hilo que falló. Si no tiene
 * tratador (o la bloquea, o la ignora), el proceso termina como siempre, con el mismo mensaje. */
quadro_t *sinais_excecao(quadro_t *q, int s, int codigo, uint32_t endereco, const char *motivo)
{
    processo_t *t = processo_atual(), *l = processo_lider(t);
    const acao_sinal_t *a = &l->sinais_acoes[s];
    if (a->tratador > SINAL_TRATADOR_IGN && !(t->sinais_mascara & BIT(s))) {
        info_sinal_t info;
        memset(&info, 0, sizeof info);
        info.signo = s;
        info.codigo = codigo;
        info.u.endereco = endereco;
        t->repetindo = 0;
        t->sinais_suspenso = false;
        quadro_t *r = entregar(t, q, s, &info);
        return r;
    }
    if (motivo) kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: %s\n", l->pid, t->nome, motivo);
    l->sinal_fim = s;
    processo_encerrar(t, SAIDA_ENCERRADO_PELO_KERNEL, motivo);
    return escalonar(q);
}

/* sigreturn: el restaurador de la libc llama aquí con el contexto que dejó montar_marco (o el
 * que el tratador cambió, como hace TCC con -bt o un siglongjmp que no vuelve). */
quadro_t *sinais_voltar(quadro_t *q)
{
    processo_t *t = processo_atual(), *l = processo_lider(t);
    contexto_sinal_t c;
    uint32_t ptr = q->ebx;
    if ((ptr & 3) || !usuario_ok((void *)ptr, sizeof c, false)) {
        kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: contexto de señal no valido\n", l->pid, t->nome);
        l->sinal_fim = SINAL_SEGV;
        processo_encerrar(t, SAIDA_ENCERRADO_PELO_KERNEL, "contexto de señal no valido");
        return escalonar(q);
    }
    memcpy(&c, (const void *)ptr, sizeof c);
    if (c.fpregs) {
        if (c.fpu_formato != fpu_formato() || (c.fpregs & (FPU_ESTADO_ALINHA - 1)) ||
            !usuario_ok((void *)c.fpregs, FPU_ESTADO, false)) {
            kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: area de la FPU de la señal no valida\n", l->pid, t->nome);
            l->sinal_fim = SINAL_SEGV;
            processo_encerrar(t, SAIDA_ENCERRADO_PELO_KERNEL, "area de la FPU de la señal no valida");
            return escalonar(q);
        }
        fpu_guardar(t);
        memcpy(t->fpu, (const void *)c.fpregs, FPU_ESTADO);
        fpu_area_sanear(t->fpu);
        t->fpu_usada = true;
    } else if (t->fpu_usada) {
        /* Antes del tratador no había usado la FPU: vuelve a la limpia. */
        fpu_guardar(t);
        memset(t->fpu, 0, FPU_ESTADO);
        t->fpu_usada = false;
    }
    const uint32_t *g = c.gregs;
    q->gs = q->fs = q->es = q->ds = SEL_UDADOS;
    q->cs = SEL_UCODIGO;
    q->ss_usuario = SEL_UDADOS;
    q->edi = g[4]; q->esi = g[5]; q->ebp = g[6];
    q->ebx = g[8]; q->edx = g[9]; q->ecx = g[10]; q->eax = g[11];
    q->esp_usuario = g[7];
    q->eip = g[14];
    q->eflags = (q->eflags & ~EFLAGS_USUARIO) | (g[16] & EFLAGS_USUARIO) | EFLAGS_IF;
    t->sinais_mascara = c.mascara & ~NAO_BLOQUEAVEIS;
    t->repetindo = 0;
    t->sinais_suspenso = false;
    return q;
}

/* ------------------------------------------------------------------ la llamada */

bool sinais_chamada_interrompivel(uint32_t n)
{
    switch (n) {
    case SIS_LER: case SIS_ESCREVER: case SIS_ESPERAR: case SIS_ESPERAR_PID: case SIS_SONDAR:
    case SIS_SINAL: case SIS_ACEITAR: case SIS_CONECTAR: case SIS_RECEBER_DE: case SIS_ENVIAR_PARA:
        return true;
    /* Revisión D401: SIS_HILO_ESPERAR no: pthread_join nunca da EINTR (POSIX); se repite siempre. */
    }
    return false;
}

/* ¿Puede "eu" mandar señales a "alvo"? El mismo usuario, o root (D255). */
static bool pode_sinalizar(const processo_t *eu, const processo_t *alvo)
{
    if (!alvo->usuario || alvo->pid == 0 || alvo->lider) return false;
    return eu->uid == 0 || eu->uid == alvo->uid;
}

static int32_t op_enviar(int pid, int s)
{
    processo_t *eu = processo_recursos();
    if (s < 0 || s >= SINAIS_N) return ERRO_INVALIDO;
    if (pid > 0) {
        processo_t *p = processo_por_pid(pid);
        if (!p || p->estado == P_ZUMBI || p->estado == P_LIVRE) return ERRO_SEM_PROCESSO;
        p = processo_lider(p);
        if (!pode_sinalizar(eu, p)) return ERRO_ACESSO;
        return sinais_enviar(p, s, eu->pid, eu->uid, SINAL_SI_USER);
    }
    /* 0: mi grupo; -1: todos los que puedo (menos yo); < -1: el grupo -pid. */
    int grupo = pid == 0 ? eu->grupo : -pid;
    int achados = 0, negados = 0;
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado == P_LIVRE || p->estado == P_ZUMBI || p->lider || !p->usuario) continue;
        if (pid == -1 ? p == eu : (p->grupo != grupo && p->pid != grupo)) continue;
        if (!pode_sinalizar(eu, p)) { negados++; continue; }
        achados++;
        sinais_enviar(p, s, eu->pid, eu->uid, SINAL_SI_USER);
    }
    return achados ? 0 : negados ? ERRO_ACESSO : ERRO_SEM_PROCESSO;
}

int32_t sis_sinal(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)d;
    processo_t *t = processo_atual(), *l = processo_lider(t);
    switch (op) {
    case SINOP_ACAO: {
        int s = (int)a;
        if (s < 1 || s >= SINAIS_N) return ERRO_INVALIDO;
        acao_sinal_t *nova = (acao_sinal_t *)b, *velha = (acao_sinal_t *)c;
        if (nova && !usuario_ok(nova, sizeof *nova, false)) return ERRO_ENDERECO;
        if (velha && !usuario_ok(velha, sizeof *velha, true)) return ERRO_ENDERECO;
        acao_sinal_t n;
        if (nova) {
            n = *nova;
            if (s == SINAL_KILL || s == SINAL_STOP) return ERRO_INVALIDO;
            if (n.tratador > SINAL_TRATADOR_IGN && (n.tratador < USUARIO_INICIO || n.restaurador < USUARIO_INICIO))
                return ERRO_INVALIDO;   /* un tratador necesita el restaurador de la libc */
        }
        if (velha) *velha = l->sinais_acoes[s];
        if (nova) {
            n.mascara &= ~NAO_BLOQUEAVEIS;
            l->sinais_acoes[s] = n;
            if (ignorada(l, s)) {   /* POSIX: las pendientes de una que pasa a ignorada se descartan */
                l->sinais_pendentes &= ~BIT(s);
                for (int i = 1; i < MAX_PROCESSOS; i++) {
                    processo_t *h = processo_indice(i);
                    if (h == l || h->lider == l) h->sinais_hilo &= ~BIT(s);
                }
            }
        }
        return 0;
    }
    case SINOP_MASCARA: {
        uint32_t *nova = (uint32_t *)b, *velha = (uint32_t *)c;
        if (nova && !usuario_ok(nova, 4, false)) return ERRO_ENDERECO;
        if (velha && !usuario_ok(velha, 4, true)) return ERRO_ENDERECO;
        uint32_t antes = t->sinais_mascara;
        if (nova) {
            uint32_t m = *nova;
            if (a == SINOP_BLOQUEAR) t->sinais_mascara |= m;
            else if (a == SINOP_DESBLOQUEAR) t->sinais_mascara &= ~m;
            else if (a == SINOP_FIXAR) t->sinais_mascara = m;
            else return ERRO_INVALIDO;
            t->sinais_mascara &= ~NAO_BLOQUEAVEIS;
        }
        if (velha) *velha = antes;
        return 0;   /* lo que se desbloqueó y estaba pendiente se entrega al salir */
    }
    case SINOP_ENVIAR:
        return op_enviar((int)a, (int)b);
    case SINOP_HILO: {
        processo_t *h = a ? processo_por_pid((int)a) : t;
        if (!h || processo_lider(h) != l || !hilo_vivo(h)) return ERRO_SEM_PROCESSO;
        return sinais_enviar_hilo(h, (int)b, l->pid, l->uid, SINAL_SI_USER);
    }
    case SINOP_PENDENTES: {
        uint32_t *m = (uint32_t *)a;
        if (!usuario_ok(m, 4, true)) return ERRO_ENDERECO;
        *m = t->sinais_hilo | l->sinais_pendentes;
        return 0;
    }
    case SINOP_SUSPENDER: {
        if (!t->sinais_suspenso) {
            uint32_t *m = (uint32_t *)a;
            if (!usuario_ok(m, 4, false)) return ERRO_ENDERECO;
            t->sinais_mascara_suspensa = t->sinais_mascara;
            t->sinais_mascara = *m & ~NAO_BLOQUEAVEIS;
            t->sinais_suspenso = true;
        }
        /* Sale con EINTR cuando se entrega una con tratador (sinais_ao_sair); las ignoradas no la
         * despiertan del todo: la llamada se repite y vuelve a dormir. */
        if ((t->sinais_hilo | l->sinais_pendentes) & ~t->sinais_mascara) return ERRO_INTERROMPIDO;
        bloquear(&canal_sinais);
        return SIS_REPETIR;
    }
    case SINOP_ALARME: {
        uint32_t agora = relogio_tiques(), resta = 0;
        if (l->alarme_em && (int32_t)(l->alarme_em - agora) > 0)
            resta = (l->alarme_em - agora) * (1000 / TIQUES_POR_SEGUNDO);
        else if (l->alarme_em)
            resta = 1;
        if (a > 0x7FFFFFFFu / TIQUES_POR_SEGUNDO) a = 0x7FFFFFFFu / TIQUES_POR_SEGUNDO;
        l->alarme_em = a ? agora + (a * TIQUES_POR_SEGUNDO + 999) / 1000 : 0;
        if (a && !l->alarme_em) l->alarme_em = 1;
        return (int32_t)resta;
    }
    case SINOP_PILHA: {
        pilha_sinal_t *nova = (pilha_sinal_t *)a, *velha = (pilha_sinal_t *)b;
        if (nova && !usuario_ok(nova, sizeof *nova, false)) return ERRO_ENDERECO;
        if (velha && !usuario_ok(velha, sizeof *velha, true)) return ERRO_ENDERECO;
        bool em_uso = t->quadro_chamada && na_pilha_alt(t, t->quadro_chamada->esp_usuario);
        pilha_sinal_t antes = t->sinais_pilha;
        if (!antes.base) antes.flags = SINAL_PILHA_DESLIGADA;
        else if (em_uso) antes.flags = SINAL_PILHA_EM_USO;
        if (nova) {
            pilha_sinal_t n = *nova;
            if (em_uso) return ERRO_ACESSO;   /* EPERM: no se cambia desde dentro de ella */
            if (n.flags & SINAL_PILHA_DESLIGADA) memset(&t->sinais_pilha, 0, sizeof t->sinais_pilha);
            else {
                if (n.flags & ~(uint32_t)SINAL_PILHA_EM_USO) return ERRO_INVALIDO;
                if (n.tamanho < PILHA_ALT_MIN) return ERRO_SEM_MEMORIA;   /* ENOMEM, como POSIX */
                if (n.base < USUARIO_INICIO || n.base + n.tamanho < n.base || n.base + n.tamanho > USUARIO_FIM)
                    return ERRO_INVALIDO;
                t->sinais_pilha.base = n.base;
                t->sinais_pilha.tamanho = n.tamanho;
                t->sinais_pilha.flags = 0;
            }
        }
        if (velha) *velha = antes;
        return 0;
    }
    case SINOP_PAI:
        return l->pai;
    case SINOP_GRUPO: {   /* getpgid: de cualquiera, como en Linux */
        processo_t *p = a ? processo_por_pid((int)a) : l;
        if (!p || p->estado == P_LIVRE || p->estado == P_ZUMBI) return ERRO_SEM_PROCESSO;
        return processo_lider(p)->grupo;
    }
    }
    return ERRO_INVALIDO;
}
