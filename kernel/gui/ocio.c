// SPDX-License-Identifier: GPL-2.0-or-later
/* ocio.c - energía y pantalla: salvapantallas, bloqueo y apagado de la pantalla por inactividad, aviso de batería
 * y «no molestar» (D422).
 *
 * La inactividad es el tiempo desde la última tecla o movimiento del ratón (incluidos los del escritorio remoto y
 * el panel táctil, que llegan por las mismas colas). Con «no molestar» (un programa lo pide con
 * INFO_OCIO_CTL, o hay una ventana a pantalla completa) la cuenta no avanza: ni salvapantallas, ni bloqueo, ni
 * apagado de la pantalla.
 *
 * Orden de lo que pasa al ir pasando los minutos: salvapantallas (efecto de kernel/lib/ocio_efectos.c) ->
 * bloqueo (la pantalla de D371, solo si la cuenta tiene contraseña) -> pantalla apagada (en negro: sin
 * VBE/PM ni llamadas a la BIOS en modo real no hay DPMS de verdad, ver D422). La primera tecla o movimiento
 * solo despierta: no llega a ningún programa.
 *
 * La configuración es de la máquina (/etc/energia.conf, clave=valor). Sin ese archivo: salvapantallas y bloqueo
 * apagados; la pantalla se apaga a los 10 min con batería y a los 30 enchufado; aviso de batería al 15 % y
 * crítico al 5 % (solo avisa). */
#include "ocio.h"
#include "abi.h"
#include "acpi.h"
#include "bateria.h"
#include "console.h"
#include "frequencia.h"
#include "gui.h"
#include "idioma.h"
#include "memoria.h"
#include "ocio_efectos.h"
#include "processo.h"
#include "relogio.h"
#include "fuso.h"
#include "sessao.h"
#include "sistema.h"
#include "telas.h"
#include "texto.h"
#include "vfs.h"
#include "video.h"

#define ARQ_CONF   "/etc/energia.conf"
#define NM_MAX     8
#define QUADRO_TIQUES 8        /* ~12 cuadros por segundo: suficiente y ligero en equipos antiguos */
#define MATAR_ESPERA_S 30      /* aviso antes de apagar por batería crítica */

typedef struct {
    int salva_efecto, salva_min;
    int bloqueo_modo, bloqueo_min;
    int pantalla_bat_min, pantalla_red_min;
    int bat_aviso, bat_critica, bat_accion;
    int gob_bat, gob_red;
} cfg_t;

static const cfg_t PREDET = { 0, 10, 0, 15, 10, 30, 15, 5, 0, 3, 3 };

static cfg_t cfg = { 0, 10, 0, 15, 10, 30, 15, 5, 0, 3, 3 };
static bool cargado;
static uint32_t t_inicio;

static int estado;                       /* OCIO_ESTADO_* */
static uint32_t ult_actividad;           /* tique */
static uint32_t extra_s;                 /* segundos simulados */
static bool bloqueo_hecho, salva_vista;
static bool quadro_pedido;               /* el próximo ocio_desenhar() dibuja un cuadro */
static uint32_t t_quadro;
static bool nm_pantalla_cheia;
static int nm_pid[NM_MAX];
static uint32_t veces_salva, veces_bloqueo, veces_apagada, avisos_bat;

static ocio_ef_t ef;
static int ef_actual = -1;

/* batería */
static bool bat_sim, bat_sim_con;
static int bat_sim_pct;
static bool bat_sim_ac;
static uint32_t t_bat;
static int bat_pct = -1;
static bool bat_con, bat_ac;
static bool avisou_baixa, avisou_critica;
static uint32_t critica_desde;           /* tique en que se avisó del apagado (0 = no) */
static int gob_aplicado = -2;

/* ------------------------------------------------------------ configuración */

static int numero(const char *s, int def, int max)
{
    if (!*s) return def;
    int v = 0;
    for (; *s >= '0' && *s <= '9'; s++) { v = v * 10 + (*s - '0'); if (v > max) return max; }
    return v;
}

static void ler_config(void)
{
    cfg = PREDET;
    cargado = false;
    uint8_t *d = NULL;
    uint32_t tam = 0;
    if (vfs_ler_inteiro(ARQ_CONF, &d, &tam) < 0 || !d) {
        klog("[OCIO] sin %s: valores por defecto\n", ARQ_CONF);
        return;
    }
    cargado = true;
    const char *p = (const char *)d, *fim = p + tam;
    while (p < fim) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        char k[40], v[16];
        const char *eq = l;
        while (eq < le && *eq != '=') eq++;
        if (l >= le || *l == '#' || eq >= le) continue;
        size_t nk = (size_t)(eq - l), nv = (size_t)(le - eq - 1);
        if (nk >= sizeof k || nv >= sizeof v) continue;
        txt_copia_n(k, l, nk + 1); k[nk] = 0;
        txt_copia_n(v, eq + 1, nv + 1); v[nv] = 0;
        if (!txt_compara(k, "salvapantallas")) cfg.salva_efecto = numero(v, 0, OCIO_EF_N - 1);
        else if (!txt_compara(k, "salvapantallas_min")) cfg.salva_min = numero(v, 10, 60);
        else if (!txt_compara(k, "bloqueo")) cfg.bloqueo_modo = numero(v, 0, 2);
        else if (!txt_compara(k, "bloqueo_min")) cfg.bloqueo_min = numero(v, 15, 120);
        else if (!txt_compara(k, "pantalla_bateria_min")) cfg.pantalla_bat_min = numero(v, 10, 240);
        else if (!txt_compara(k, "pantalla_red_min")) cfg.pantalla_red_min = numero(v, 30, 240);
        else if (!txt_compara(k, "bateria_aviso")) cfg.bat_aviso = numero(v, 15, 50);
        else if (!txt_compara(k, "bateria_critica")) cfg.bat_critica = numero(v, 5, 30);
        else if (!txt_compara(k, "bateria_accion")) cfg.bat_accion = numero(v, 0, 1);
        else if (!txt_compara(k, "gobernador_bateria")) cfg.gob_bat = numero(v, 3, 3);
        else if (!txt_compara(k, "gobernador_red")) cfg.gob_red = numero(v, 3, 3);
    }
    kfree(d);
    if (cfg.bat_critica >= cfg.bat_aviso && cfg.bat_aviso) cfg.bat_critica = cfg.bat_aviso > 1 ? cfg.bat_aviso - 1 : 1;
    gob_aplicado = -2;   /* se vuelve a aplicar el perfil */
    klog("[OCIO] configuracion: salvapantallas=%d/%d min, bloqueo=%d/%d min, pantalla bateria=%d red=%d min, "
         "bateria aviso=%d critica=%d accion=%d, gobernador bat=%d red=%d\n", cfg.salva_efecto, cfg.salva_min, cfg.bloqueo_modo,
         cfg.bloqueo_min, cfg.pantalla_bat_min, cfg.pantalla_red_min, cfg.bat_aviso, cfg.bat_critica, cfg.bat_accion,
         cfg.gob_bat, cfg.gob_red);
}

/* ------------------------------------------------------------ no molestar */

static bool nm_vivo(int pid)
{
    processo_t *p = processo_por_pid(pid);
    return p && p->estado != P_LIVRE && p->estado != P_ZUMBI;
}

static int nm_contar(void)
{
    int n = 0;
    for (int i = 0; i < NM_MAX; i++) {
        if (!nm_pid[i]) continue;
        if (!nm_vivo(nm_pid[i])) { klog("[OCIO] no molestar: el pid %d acabo y lo suelta\n", nm_pid[i]); nm_pid[i] = 0; continue; }
        n++;
    }
    return n;
}

/* ------------------------------------------------------------ batería y perfil */

static void aplicar_perfil(void)
{
    int quiere = bat_con && !bat_ac ? cfg.gob_bat : cfg.gob_red;
    if (quiere == 3 || quiere == gob_aplicado) return;
    static const char *const nomes[] = { "rendimiento", "ahorro", "a demanda" };
    int r = sis_frequencia(FREQ_OP_POLITICA, (uint32_t)quiere, 0, 0, 0);
    klog("[OCIO] gobernador: perfil %s (%s) -> %d\n", nomes[quiere], bat_con && !bat_ac ? "con bateria" : "enchufado", r);
    gob_aplicado = r == 0 ? quiere : -2;
    if (r != 0) cfg.gob_bat = cfg.gob_red = 3;   /* sin driver de frecuencia: no insistir */
}

static void bateria_tique(uint32_t agora)
{
    if (agora - t_bat < 2 * TIQUES_POR_SEGUNDO) return;
    t_bat = agora;
    bool antes_con = bat_con, antes_ac = bat_ac;
    if (bat_sim) {
        bat_con = bat_sim_con; bat_pct = bat_sim_pct; bat_ac = bat_sim_ac;
    } else {
        info_bateria_t b;
        bat_con = bateria_estado(&b);
        bat_pct = bat_con ? b.porcentagem : -1;
        bat_ac = !bat_con || b.adaptador == 2 || (b.adaptador == 0 && !(b.estado & BATERIA_DESCARREGANDO));
    }
    if (bat_con != antes_con || bat_ac != antes_ac) gob_aplicado = -2;
    aplicar_perfil();
    if (!bat_con || bat_ac) {
        if (critica_desde) klog("[OCIO] apagado por bateria cancelado\n");
        avisou_baixa = avisou_critica = false;
        critica_desde = 0;
        return;
    }
    if (bat_pct < 0) return;
    char m[96];
    if (cfg.bat_critica && bat_pct <= cfg.bat_critica) {
        if (!avisou_critica) {
            avisou_critica = true; avisos_bat++;
            klog("[OCIO] bateria critica: %d %%\n", bat_pct);
            snformatar(m, sizeof m, cfg.bat_accion ? _("Batería crítica (%d %%): el equipo se apagará en 30 s si no lo enchufas.")
                                                   : _("Batería crítica (%d %%): enchufa el cargador."), bat_pct);
            gui_aviso(m);
            if (cfg.bat_accion) critica_desde = agora;
        }
    } else if (cfg.bat_aviso && bat_pct <= cfg.bat_aviso) {
        if (!avisou_baixa) {
            avisou_baixa = true; avisos_bat++;
            klog("[OCIO] bateria baja: %d %%\n", bat_pct);
            snformatar(m, sizeof m, _("Batería baja: %d %%."), bat_pct);
            gui_aviso(m);
        }
    }
    if (cfg.bat_aviso && bat_pct > cfg.bat_aviso + 3) avisou_baixa = false;
    if (bat_pct > cfg.bat_critica + 3) { avisou_critica = false; if (critica_desde) { critica_desde = 0; klog("[OCIO] apagado por bateria cancelado\n"); } }
    if (critica_desde && cfg.bat_accion && agora - critica_desde >= MATAR_ESPERA_S * TIQUES_POR_SEGUNDO) {
        critica_desde = 0;
        klog("[OCIO] bateria critica: apagado ordenado\n");
        acpi_pedir_apagado();
    }
}

/* ------------------------------------------------------------ estados */

static void tela_toda(void) { gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura()); }

static void empezar(int nuevo, int efecto)
{
    estado = nuevo;
    quadro_pedido = true;
    t_quadro = relogio_tiques();
    if (nuevo == OCIO_ESTADO_SALVA) {
        veces_salva++;
        salva_vista = true;
        ef_actual = efecto;
        ocio_ef_iniciar(&ef, efecto, relogio_tiques() * 2654435761u + 12345u, (int)video_largura(), (int)video_altura(),
                        relogio_tiques() * 10u);
        klog("[OCIO] salvapantallas: efecto %d\n", efecto);
    } else {
        veces_apagada++;
        klog("[OCIO] pantalla apagada (en negro)\n");
    }
    tela_toda();
}

static void despertar(void)
{
    bool vista = salva_vista;
    klog("[OCIO] despierta (%s)\n", estado == OCIO_ESTADO_APAGADA ? "pantalla apagada" : "salvapantallas");
    estado = OCIO_ESTADO_ACTIVO;
    salva_vista = false;
    quadro_pedido = false;
    bloqueo_hecho = false;
    if (vista && cfg.bloqueo_modo == 2 && !sessao_bloqueada() && sessao_bloquear_inatividade()) {
        veces_bloqueo++;
        klog("[OCIO] bloqueo al salir del salvapantallas\n");
    }
    tela_toda();
}

void ocio_iniciar(void)
{
    ler_config();
    ult_actividad = t_inicio = relogio_tiques();
    extra_s = 0;
    estado = OCIO_ESTADO_ACTIVO;
}

bool ocio_entrada(void)
{
    ult_actividad = relogio_tiques();
    extra_s = 0;
    if (estado == OCIO_ESTADO_ACTIVO) { bloqueo_hecho = false; salva_vista = false; return false; }
    despertar();
    return true;
}

bool ocio_cobre(void) { return estado != OCIO_ESTADO_ACTIVO; }

void ocio_tique(void)
{
    uint32_t agora = relogio_tiques();
    static bool relido;
    if (!cargado && !relido && agora - t_inicio > 10 * TIQUES_POR_SEGUNDO) { relido = true; ler_config(); }
    bateria_tique(agora);
    bool nm = nm_contar() > 0;
    nm_pantalla_cheia = gui_tela_cheia_ativa() && !sessao_ativa();
    if (nm || nm_pantalla_cheia) {
        ult_actividad = agora;
        extra_s = 0;
        if (estado != OCIO_ESTADO_ACTIVO) despertar();
        return;
    }
    uint32_t ocioso = (agora - ult_actividad) / TIQUES_POR_SEGUNDO + extra_s;
    int pmin = bat_con && !bat_ac ? cfg.pantalla_bat_min : cfg.pantalla_red_min;
    if (cfg.bloqueo_modo == 1 && cfg.bloqueo_min && !bloqueo_hecho && ocioso >= (uint32_t)cfg.bloqueo_min * 60) {
        bloqueo_hecho = true;
        if (sessao_bloquear_inatividade()) { veces_bloqueo++; klog("[OCIO] bloqueo por inactividad (%d min)\n", cfg.bloqueo_min); }
    }
    if (pmin && ocioso >= (uint32_t)pmin * 60) {
        if (estado != OCIO_ESTADO_APAGADA) empezar(OCIO_ESTADO_APAGADA, 0);
    } else if (cfg.salva_efecto && cfg.salva_min && ocioso >= (uint32_t)cfg.salva_min * 60) {
        if (estado == OCIO_ESTADO_ACTIVO) empezar(OCIO_ESTADO_SALVA, cfg.salva_efecto);
    }
    if (estado == OCIO_ESTADO_SALVA && agora - t_quadro >= QUADRO_TIQUES) {
        t_quadro = agora;
        quadro_pedido = true;
        tela_toda();
    }
}

/* El compositor lo llama en lugar de dibujar las ventanas: el búfer de la pantalla conserva el último cuadro, así
 * que si no toca cuadro nuevo (algo de detrás pidió repintar) no se dibuja nada y se muestra lo que hay. */
void ocio_desenhar(superficie_t *t)
{
    if (!quadro_pedido) return;
    quadro_pedido = false;
    sup_retangulo(t, 0, 0, t->larg, t->alt, 0);
    if (estado != OCIO_ESTADO_SALVA) return;
    int h = 0, m = 0, s = 0;
    if (ef_actual == OCIO_EF_RELOJ) {
        fuso_data_t d;
        hora_local(&d);
        h = d.hora; m = d.minuto; s = d.segundo;
    }
    ocio_ef_cuadro(&ef, t->px, t->larg, t->alt, t->larg, relogio_tiques() * 10u, 0, h, m, s);
}

/* ------------------------------------------------------------ llamadas al sistema */

int32_t ocio_info(uint32_t ptr, uint32_t n)
{
    info_ocio_t *v = (info_ocio_t *)ptr;
    if (n < sizeof *v || !usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
    info_ocio_t r;
    memset(&r, 0, sizeof r);
    r.versao = 1;
    r.estado = (unsigned)estado;
    r.inactivo_s = (relogio_tiques() - ult_actividad) / TIQUES_POR_SEGUNDO + extra_s;
    r.no_molestar = (unsigned)nm_contar() + (nm_pantalla_cheia ? 1u : 0u);
    r.bloqueada = sessao_bloqueada();
    r.cargado = cargado;
    r.salva_efecto = (unsigned)cfg.salva_efecto; r.salva_min = (unsigned)cfg.salva_min;
    r.bloqueo_modo = (unsigned)cfg.bloqueo_modo; r.bloqueo_min = (unsigned)cfg.bloqueo_min;
    r.pantalla_bat_min = (unsigned)cfg.pantalla_bat_min; r.pantalla_red_min = (unsigned)cfg.pantalla_red_min;
    r.bat_aviso = (unsigned)cfg.bat_aviso; r.bat_critica = (unsigned)cfg.bat_critica; r.bat_accion = (unsigned)cfg.bat_accion;
    r.gob_bat = (unsigned)cfg.gob_bat; r.gob_red = (unsigned)cfg.gob_red;
    r.con_bateria = bat_con; r.bat_pct = bat_pct; r.enchufado = bat_ac; r.simulada = bat_sim;
    r.veces_salva = veces_salva; r.veces_bloqueo = veces_bloqueo; r.veces_apagada = veces_apagada; r.avisos_bateria = avisos_bat;
    memcpy(v, &r, sizeof r);
    return 0;
}

bool ocio_op_libre(uint32_t pedido)
{
    uint32_t op = pedido >> 16;
    return op == OCIO_OP_RECARGAR || op == OCIO_OP_NO_MOLESTAR || op == OCIO_OP_ACTIVIDAD || op == OCIO_OP_PROBAR;
}

int32_t ocio_ctl(uint32_t pedido)
{
    uint32_t op = pedido >> 16, arg = pedido & 0xFFFFu;
    switch (op) {
    case OCIO_OP_RECARGAR:
        ler_config();
        return 0;
    case OCIO_OP_NO_MOLESTAR: {
        int pid = processo_recursos()->pid, libre = -1;
        for (int i = 0; i < NM_MAX; i++) {
            if (nm_pid[i] == pid) { if (!arg) { nm_pid[i] = 0; klog("[OCIO] no molestar: suelta el pid %d\n", pid); } return 0; }
            if (!nm_pid[i] && libre < 0) libre = i;
        }
        if (!arg) return 0;
        if (libre < 0) return ERRO_SEM_MEMORIA;
        nm_pid[libre] = pid;
        ult_actividad = relogio_tiques();
        klog("[OCIO] no molestar: pide el pid %d\n", pid);
        return 0;
    }
    case OCIO_OP_ACTIVIDAD:
        ocio_entrada();
        return 0;
    case OCIO_OP_SIMULAR:
        extra_s += arg;
        klog("[OCIO] simulado: +%u s de inactividad\n", arg);
        return 0;
    case OCIO_OP_PROBAR:
        if (arg >= OCIO_EF_N) return ERRO_INVALIDO;
        if (estado == OCIO_ESTADO_ACTIVO) empezar(OCIO_ESTADO_SALVA, (int)arg);
        return 0;
    case OCIO_OP_BATERIA_SIM:
        if (arg == 0xFFFF) { bat_sim = false; klog("[OCIO] bateria: la real\n"); }
        else {
            bat_sim = true;
            bat_sim_con = !(arg & 0x200);
            bat_sim_ac = (arg & 0x100) != 0;
            bat_sim_pct = (int)(arg & 0xFF);
            klog("[OCIO] bateria simulada: %s %d %% %s\n", bat_sim_con ? "con bateria" : "sin bateria", bat_sim_pct,
                 bat_sim_ac ? "enchufada" : "desenchufada");
        }
        t_bat = 0;
        return 0;
    default:
        return ERRO_INVALIDO;
    }
}
