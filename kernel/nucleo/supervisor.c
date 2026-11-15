// SPDX-License-Identifier: GPL-2.0-or-later
/* supervisor.c - el gestor de servicios visto desde el kernel (D216).
 *
 * Lo mínimo para que /bin/servicios pueda hacer su trabajo:
 *   - lanzarlo al arrancar como un proceso más, sin padre (como el Terminal);
 *   - relanzarlo si muere: el proceso 0 lo mira una vez por segundo en su
 *     espera; espera 1, 2, 4, 8 y 16 s antes de cada intento y, tras 5 muertes
 *     seguidas (sin llegar a vivir 60 s), se rinde y lo deja anotado;
 *   - SIS_SERVICOS: el programa se registra como supervisor, dice cuántos
 *     servicios tiene en marcha y pregunta si el sistema se está apagando;
 *   - al apagar o reiniciar, la tarea "apagar" (acpi.c) le avisa y espera a que
 *     diga «listo» antes de desmontar los discos. El plazo es de 20 s: un
 *     supervisor colgado nunca impide apagar.
 *
 * Nada de esto puede impedir que el sistema arranque: si /bin/servicios no
 * existe o no carga, se anota y se sigue. */
#include "supervisor.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define ESPERA_APAGADO_S 20   /* lo máximo que el apagado espera al supervisor */
#define RELANCAR_MAX      5   /* muertes seguidas antes de rendirse */
#define VIDA_ESTAVEL_S   60   /* vivo tanto tiempo: las muertes vuelven a contar desde cero */

static int sup_pid;               /* el que se registró con SERV_REGISTRAR */
static int lancado_pid;           /* el último que lanzó el kernel */
static bool ligado;               /* el kernel se encarga de lanzarlo y relanzarlo */
static volatile int apagando;     /* SERV_MOTIVO_*: una vez puesto, ya no se quita */
static volatile bool pronto;      /* el supervisor ya paró los servicios */
static volatile int ativos;       /* servicios en marcha según el supervisor */
static uint32_t proxima_vista, lancado_em, relancar_em;
static bool esperando_relancar;
static int mortes;

static bool vivo(int pid)
{
    if (pid <= 0) return false;
    processo_t *p = processo_por_pid(pid);
    return p && p->estado != P_ZUMBI && p->estado != P_LIVRE;
}

static bool antes(uint32_t limite)
{
    return (int32_t)(relogio_tiques() - limite) < 0;
}

/* Con las interrupciones apagadas: el proceso 0 no debe ceder el turno a medio
 * crear el proceso (otro programa podría estar creando uno a la vez). El
 * programa está en el ramdisk: leerlo no duerme. */
static void lancar(void)
{
    char *argv[] = { "servicios", NULL };
    bool i0 = int_salva_desliga();
    int pid = processo_executar_como("/bin/servicios", 1, argv, UID_ROOT, GID_ROOT);   /* D255: root */
    int_restaura(i0);
    lancado_em = relogio_tiques();
    lancado_pid = pid > 0 ? pid : 0;
    if (pid < 0) klog("[SERVICOS] no se pudo lanzar /bin/servicios (erro %d)\n", pid);
    else klog("[SERVICOS] supervisor lanzado: pid %d\n", pid);
}

void supervisor_lancar(void)
{
    if (info_boot.opcoes & OPC_BOOT_RESCATE) {
        klog("[SERVICOS] no se lanzan en el modo rescate\n");
        return;
    }
    if (info_boot.opcoes & OPC_BOOT_SEM_SERVICOS) {
        klog("[SERVICOS] desactivados por la opcion de arranque (--sin-servicios)\n");
        return;
    }
    if (info_boot.teste[0]) {
        klog("[SERVICOS] no se lanzan en una autoprueba (%s)\n", info_boot.teste);
        return;
    }
    ligado = true;
    proxima_vista = relogio_tiques() + TIQUES_POR_SEGUNDO;
    lancar();
}

void supervisor_vigiar(void)
{
    if (!ligado || apagando || antes(proxima_vista)) return;
    proxima_vista = relogio_tiques() + TIQUES_POR_SEGUNDO;
    if (vivo(lancado_pid) || vivo(sup_pid)) return;   /* también vale uno lanzado a mano */
    if (!esperando_relancar) {
        uint32_t vida = (relogio_tiques() - lancado_em) / TIQUES_POR_SEGUNDO;
        if (vida >= VIDA_ESTAVEL_S) mortes = 0;
        mortes++;
        if (mortes > RELANCAR_MAX) {
            klog("[SERVICOS] el supervisor murio %d veces seguidas: no se relanza mas\n", RELANCAR_MAX);
            ligado = false;
            return;
        }
        uint32_t espera = 1u << (mortes - 1);   /* 1, 2, 4, 8, 16 s */
        klog("[SERVICOS] el supervisor (pid %d) murio tras %u s: se relanza en %u s\n", lancado_pid, vida, espera);
        relancar_em = relogio_tiques() + espera * TIQUES_POR_SEGUNDO;
        esperando_relancar = true;
        return;
    }
    if (antes(relancar_em)) return;
    esperando_relancar = false;
    lancar();
}

/* D251: el supervisor registrado, si vive y se cargó de /bin/servicios (un
 * programa cualquiera que se registre cuando no hay otro no cuenta). */
int supervisor_pid_confiavel(void)
{
    if (!vivo(sup_pid)) return 0;
    processo_t *p = processo_por_pid(sup_pid);
    return p && p->exe_sistema == EXE_SERVICIOS ? sup_pid : 0;
}

bool supervisor_ativo(void)
{
    return vivo(sup_pid) && ativos > 0 && !apagando;
}

bool supervisor_parar_servicos(int motivo)
{
    if (apagando) return true;   /* ya se hizo (un segundo pedido mientras se apaga) */
    apagando = motivo;           /* desde aquí, el supervisor ya no se relanza */
    if (!vivo(sup_pid) || ativos <= 0) return true;
    pronto = false;
    klog("[SERVICOS] %s: parando %d servicio(s) (supervisor pid %d)\n",
         motivo == SERV_MOTIVO_REINICIAR ? "reinicio" : "apagado", ativos, sup_pid);
    if (gui_ativa()) gui_aviso("Parando los servicios...");
    else kprintf("Parando los servicios...\n");
    uint32_t limite = relogio_tiques() + ESPERA_APAGADO_S * TIQUES_POR_SEGUNDO;
    while (!pronto && vivo(sup_pid) && antes(limite))
        relogio_dormir_ms(100);
    if (pronto) klog("[SERVICOS] servicios parados\n");
    else klog("[SERVICOS] el supervisor no termino (%s): el apagado sigue igual\n",
              vivo(sup_pid) ? "plazo agotado" : "ha muerto");
    return pronto;
}

/* ---------------------------------------------------------------- elevación (D255)
 *
 * Los pedidos de su, admin y passwd esperan aquí a que el supervisor los
 * recoja. El kernel guarda quién pidió (pid, uid y si viene de un servicio) y
 * la contraseña, que borra en cuanto se entrega; el supervisor solo ve lo que
 * el kernel le dice de quien pidió, no lo que el programa afirme. Al conceder
 * una elevación, el kernel cambia la identidad del proceso que la pidió, si
 * sigue vivo y es el mismo (los pid no se reutilizan). */
#define PEDIDOS_MAX   8
#define PEDIDO_VIDA_S 120   /* un pedido que nadie atiende ni recoge caduca */

/* D269: en un pedido gráfico (PED_GRAFICO) el kernel hace de intermediario entre el diálogo de permisos y
 * el supervisor: primero lo sondea sin contraseña (¿tiene root contraseña?, ¿está bloqueado?), luego
 * el diálogo pregunta y entrega la contraseña, y un fallo vuelve al diálogo sin contestar al programa. */
typedef enum { PD_LIVRE, PD_ESPERANDO, PD_ENTREGUE, PD_RESPONDIDO, PD_DIALOGO } pd_estado_t;
#define GF_SONDA       1   /* esperando la respuesta del sondeo sin contraseña */
#define GF_PREGUNTA    2   /* el diálogo pide la contraseña */
#define GF_COMPROBANDO 3   /* contraseña entregada al supervisor */
#define PEDIDO_DIALOGO_S 300   /* un diálogo abierto sin atender caduca a los 5 min */
#define LEMBRAR_S      300   /* autorización recordada por programa: 5 min */
typedef struct {
    pd_estado_t       estado;
    pedido_recebido_t r;
    int               resultado;
    uint32_t          desde;
    bool              graf;
    uint8_t           fase;
    int               erro;               /* último fallo mostrado por el diálogo */
    bool              primeira;           /* root no tiene contraseña: el diálogo pide elegirla */
    char              nome[32];           /* el nombre lo pone el kernel (processo_t), no el programa */
    char              motivo[PED_MOTIVO_MAX];
} pedido_t;

/* Autorizaciones recordadas (solo programas de /bin, por nombre y uid). */
static struct { char nome[32]; uint32_t uid, ate; bool usada; } lembradas[8];

static bool lembrada(const processo_t *p)
{
    if (p->exe_sistema != EXE_BIN) return false;
    for (int i = 0; i < 8; i++)
        if (lembradas[i].usada && lembradas[i].uid == p->uid && txt_compara(lembradas[i].nome, p->nome) == 0)
            return (int32_t)(relogio_tiques() - lembradas[i].ate) < 0;
    return false;
}

static void lembrar(const processo_t *p, uint32_t uid_antes)
{
    if (p->exe_sistema != EXE_BIN) return;
    int k = 0;
    for (int i = 0; i < 8; i++) {
        if (lembradas[i].usada && lembradas[i].uid == uid_antes && txt_compara(lembradas[i].nome, p->nome) == 0) { k = i; goto pronto; }
        if (!lembradas[i].usada) k = i;
        else if ((int32_t)(relogio_tiques() - lembradas[i].ate) >= 0) k = i;
    }
pronto:
    txt_copia_n(lembradas[k].nome, p->nome, sizeof lembradas[k].nome);
    lembradas[k].uid = uid_antes;
    lembradas[k].ate = relogio_tiques() + LEMBRAR_S * TIQUES_POR_SEGUNDO;
    lembradas[k].usada = true;
}
static pedido_t pedidos[PEDIDOS_MAX];
static uint32_t proximo_id = 1;

static void pedido_limpar(pedido_t *p)
{
    memset(p, 0, sizeof *p);   /* también la contraseña */
}

static void pedido_limpar_senhas(pedido_t *p)
{
    memset(p->r.senha, 0, PED_SENHA_MAX);
    memset(p->r.senha_nova, 0, PED_SENHA_MAX);
}

static void pedidos_caducar(void)
{
    for (int i = 0; i < PEDIDOS_MAX; i++) {
        pedido_t *p = &pedidos[i];
        if (p->estado == PD_LIVRE) continue;
        uint32_t vida = p->estado == PD_DIALOGO ? PEDIDO_DIALOGO_S : PEDIDO_VIDA_S;
        bool velho = relogio_tiques() - p->desde > vida * TIQUES_POR_SEGUNDO;
        if (velho || !vivo(p->r.pid)) pedido_limpar(p);
    }
}

static int pedir(uint32_t ptr)
{
    pedido_conta_t *u = (pedido_conta_t *)ptr;
    if (!usuario_ok(u, sizeof *u, false)) return ERRO_ENDERECO;
    if (!supervisor_pid_confiavel()) return ERRO_NAO_EXISTE;
    bool graf = (u->tipo & PED_GRAFICO) != 0;
    uint32_t tipo = u->tipo & ~(uint32_t)PED_GRAFICO;
    if (tipo != PED_ELEVAR && tipo != PED_SENHA) return ERRO_INVALIDO;
    if (u->uid_alvo > UID_MAX) return ERRO_INVALIDO;
    pedidos_caducar();
    processo_t *eu = processo_recursos();
    if (graf && (tipo != PED_ELEVAR || u->uid_alvo != UID_ROOT || !gui_ativa())) return ERRO_NAO_SUPORTADO;
    if (graf && eu->de_servico) return ERRO_ACESSO;   /* un servicio (SSH) no tiene a nadie delante del escritorio */
    pedido_t *livre = NULL;
    for (int i = 0; i < PEDIDOS_MAX; i++) {
        if (pedidos[i].estado != PD_LIVRE && pedidos[i].r.pid == eu->pid) return ERRO_OCUPADO;   /* uno por proceso */
        if (!livre && pedidos[i].estado == PD_LIVRE) livre = &pedidos[i];
    }
    if (!livre) return ERRO_OCUPADO;
    pedido_t *p = livre;
    pedido_limpar(p);
    p->r.id = proximo_id++;
    if (proximo_id > 0x7FFFFFFF) proximo_id = 1;
    p->r.tipo = tipo;
    p->r.uid_alvo = u->uid_alvo;
    p->r.pid = eu->pid;
    p->r.uid = eu->uid;
    p->r.gid = eu->gid;
    p->r.servicio = eu->de_servico;
    if (!graf) {
        memcpy(p->r.senha, u->senha, PED_SENHA_MAX);
        memcpy(p->r.senha_nova, u->senha_nova, PED_SENHA_MAX);
        p->r.senha[PED_SENHA_MAX - 1] = p->r.senha_nova[PED_SENHA_MAX - 1] = 0;
    }   /* gráfico: la contraseña no la da el programa, solo el diálogo del sistema */
    p->desde = relogio_tiques();
    p->estado = PD_ESPERANDO;
    if (graf) {
        p->graf = true;
        p->fase = GF_SONDA;
        txt_copia_n(p->nome, eu->nome, sizeof p->nome);
        for (int i = 0; i < PED_MOTIVO_MAX - 1 && u->motivo[i]; i++)
            p->motivo[i] = (unsigned char)u->motivo[i] < 32 ? ' ' : u->motivo[i];   /* sin controles: el texto es del programa */
        if (lembrada(eu)) {   /* la persona ya lo autorizó hace menos de 5 min: sin diálogo */
            uint32_t uid_antes = eu->uid;
            processo_lider(eu)->uid = UID_ROOT;
            processo_lider(eu)->gid = GID_ROOT;
            p->resultado = 1;
            p->estado = PD_RESPONDIDO;
            klog("[USUARIOS] ELEVACION recordada: %s (pid %d) pasa de uid %u a root sin diálogo (5 min)\n", eu->nome, eu->pid, uid_antes);
            return (int)p->r.id;
        }
    }
    klog("[USUARIOS] pedido %u de %s (pid %d, uid %u): %s uid %u%s%s\n", p->r.id, eu->nome, eu->pid, eu->uid,
         tipo == PED_ELEVAR ? "elevar a" : "contrasena de", u->uid_alvo, eu->de_servico ? " (desde un servicio)" : "",
         graf ? " (con diálogo)" : "");
    return (int)p->r.id;
}

static pedido_t *achar_pedido(uint32_t id)
{
    for (int i = 0; i < PEDIDOS_MAX; i++)
        if (pedidos[i].estado != PD_LIVRE && pedidos[i].r.id == id) return &pedidos[i];
    return NULL;
}

static int resultado(uint32_t id)
{
    pedido_t *p = achar_pedido(id);
    if (!p || p->r.pid != processo_recursos()->pid) return ERRO_NAO_EXISTE;
    if (p->estado != PD_RESPONDIDO) {
        if (!supervisor_pid_confiavel()) { pedido_limpar(p); return ERRO_NAO_EXISTE; }   /* murió sin responder */
        return 0;
    }
    int r = p->resultado;
    pedido_limpar(p);
    return r;
}

static int proximo(uint32_t ptr)
{
    pedido_recebido_t *dst = (pedido_recebido_t *)ptr;
    if (!usuario_ok(dst, sizeof *dst, true)) return ERRO_ENDERECO;
    pedidos_caducar();
    pedido_t *melhor = NULL;
    for (int i = 0; i < PEDIDOS_MAX; i++)
        if (pedidos[i].estado == PD_ESPERANDO && (!melhor || pedidos[i].r.id < melhor->r.id)) melhor = &pedidos[i];
    if (!melhor) return 0;
    memcpy(dst, &melhor->r, sizeof *dst);
    memset(melhor->r.senha, 0, PED_SENHA_MAX);   /* el kernel no se queda con las contraseñas */
    memset(melhor->r.senha_nova, 0, PED_SENHA_MAX);
    melhor->estado = PD_ENTREGUE;
    return 1;
}

static void elevar_pedido(pedido_t *p, processo_t *alvo)
{
    alvo->uid = p->r.uid_alvo;
    alvo->gid = p->r.uid_alvo == UID_ROOT ? GID_ROOT : p->r.uid_alvo;
    klog("[USUARIOS] ELEVACION concedida: %s (pid %d) pasa de uid %u a uid %u\n", alvo->nome, alvo->pid,
         p->r.uid, p->r.uid_alvo);
}

static int responder(uint32_t id, int32_t res)
{
    pedido_t *p = achar_pedido(id);
    if (!p || p->estado != PD_ENTREGUE) return ERRO_NAO_EXISTE;
    processo_t *alvo = processo_por_pid(p->r.pid);
    if (!alvo || alvo->estado == P_ZUMBI) { pedido_limpar(p); return ERRO_SEM_PROCESSO; }
    alvo = processo_lider(alvo);
    if (p->graf && res != 1) {
        /* D269: un fallo vuelve al diálogo (que lo enseña y deja reintentar); al programa solo llega el definitivo. */
        bool sondeo = p->fase == GF_SONDA;
        bool reintentable = res == ERRO_ACESSO || res == ERRO_ESPERA || res == ERRO_INVALIDO;
        if (sondeo && res == ERRO_SEM_SENHA) { p->primeira = true; p->erro = 0; }
        else if (sondeo && res == ERRO_ACESSO) { p->primeira = false; p->erro = 0; }   /* «falta la contraseña»: pregunta */
        else if (reintentable) p->erro = res;
        else {
            klog("[USUARIOS] pedido %u de pid %d: NEGADO (%d)\n", p->r.id, p->r.pid, res);
            p->resultado = res < 0 ? res : ERRO_ACESSO;
            p->estado = PD_RESPONDIDO;
            p->desde = relogio_tiques();
            return 0;
        }
        p->fase = GF_PREGUNTA;
        p->estado = PD_DIALOGO;
        p->desde = relogio_tiques();
        return 0;
    }
    if (res == 1 && p->r.tipo == PED_ELEVAR) {
        if (alvo->uid != p->r.uid) { pedido_limpar(p); return ERRO_SEM_PROCESSO; }   /* ya no es el mismo */
        elevar_pedido(p, alvo);
        if (p->graf) lembrar(alvo, p->r.uid);
    } else {
        klog("[USUARIOS] pedido %u de pid %d: %s (%d)\n", p->r.id, p->r.pid, res == 1 ? "hecho" : "NEGADO", res);
    }
    p->resultado = res == 1 ? 1 : res < 0 ? res : ERRO_ACESSO;
    p->estado = PD_RESPONDIDO;
    p->desde = relogio_tiques();
    return 0;
}

/* ---------------------------------------------------------------- diálogo de permisos (D269)
 * Lo usa kernel/gui/permisos_dialogo.c. El programa que pide nunca ve la contraseña: la escribe la
 * persona en el diálogo del gestor de ventanas y pasa directamente de ahí al supervisor. */
bool supervisor_dialogo_vista(dialogo_vista_t *v)
{
    pedidos_caducar();
    pedido_t *m = NULL;
    for (int i = 0; i < PEDIDOS_MAX; i++) {
        pedido_t *p = &pedidos[i];
        if (!p->graf || p->estado == PD_LIVRE || p->estado == PD_RESPONDIDO || p->fase == GF_SONDA) continue;
        if (!m || p->r.id < m->r.id) m = p;
    }
    if (!m) return false;
    v->id = m->r.id;
    v->pid = m->r.pid;
    txt_copia_n(v->nome, m->nome, sizeof v->nome);
    txt_copia_n(v->motivo, m->motivo, sizeof v->motivo);
    v->comprobando = m->fase == GF_COMPROBANDO;
    v->erro = m->erro;
    v->primeira = m->primeira;
    return true;
}

void supervisor_dialogo_enviar(uint32_t id, const char *senha, const char *nova)
{
    pedido_t *p = achar_pedido(id);
    if (!p || !p->graf || p->estado != PD_DIALOGO) return;
    txt_copia_n(p->r.senha, senha, PED_SENHA_MAX);
    txt_copia_n(p->r.senha_nova, nova, PED_SENHA_MAX);
    p->fase = GF_COMPROBANDO;
    p->estado = PD_ESPERANDO;
    p->desde = relogio_tiques();
}

void supervisor_dialogo_negar(uint32_t id)
{
    pedido_t *p = achar_pedido(id);
    if (!p || !p->graf || p->estado != PD_DIALOGO) return;
    klog("[USUARIOS] pedido %u de %s (pid %d): la persona dijo NO\n", p->r.id, p->nome, p->r.pid);
    pedido_limpar_senhas(p);
    p->resultado = ERRO_ACESSO;
    p->estado = PD_RESPONDIDO;
    p->desde = relogio_tiques();
}

/* servicos(SERV_*, arg). Cualquiera puede preguntar; solo el supervisor
 * registrado puede decir cuántos servicios tiene y que ya terminó. */
int32_t sis_servicos(uint32_t op, uint32_t arg, uint32_t c, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    int eu = processo_recursos()->pid;
    switch (op) {
    case SERV_REGISTRAR:
        if (processo_recursos()->uid != 0) return ERRO_ACESSO;   /* D255 (permisos.c ya lo niega) */
        if (vivo(sup_pid) && sup_pid != eu) return ERRO_OCUPADO;
        if (apagando) return ERRO_ACESSO;
        sup_pid = eu;
        ativos = 0;
        klog("[SERVICOS] supervisor registrado: pid %d\n", eu);
        return 0;
    case SERV_APAGANDO:
        return apagando;
    case SERV_SUPERVISOR:
        return vivo(sup_pid) ? sup_pid : 0;
    case SERV_PRONTO:
        if (eu != sup_pid) return ERRO_ACESSO;
        pronto = true;
        return 0;
    case SERV_ATIVOS:
        if (eu != sup_pid) return ERRO_ACESSO;
        ativos = (int)arg;
        return 0;
    case SERV_PEDIR:
        return pedir(arg);
    case SERV_RESULTADO:
        return resultado(arg);
    case SERV_PROXIMO:
        if (eu != supervisor_pid_confiavel() || processo_recursos()->uid != 0) return ERRO_ACESSO;
        return proximo(arg);
    case SERV_RESPONDER:
        if (eu != supervisor_pid_confiavel() || processo_recursos()->uid != 0) return ERRO_ACESSO;
        return responder(arg, (int32_t)c);
    }
    return ERRO_INVALIDO;
}
