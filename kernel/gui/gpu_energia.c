// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu_energia.c - energía de la GPU: la capa común (D399). Ver kernel/inc/gpu_energia.h.
 *
 * Modos (INFO_GPU_ENERGIA_CTL, «acel2d energia», kconfig gpu_energia):
 *   bios    no se toca nada; si se había cambiado, vuelve a los relojes y la tensión del arranque;
 *   bajo    el estado de menos reloj que vale con las pantallas encendidas;
 *   alto    el de por defecto de la BIOS (nunca por encima: los relojes se recortan a los de FirmwareInfo);
 *   auto    el gobernador (cada 100 ms, como el dynpm de Linux): sube un estado si el 3D o el 2D/volcado
 *           trabajan de forma sostenida (300 ms), baja uno tras 2 s en reposo, y no cambia dos veces en
 *           menos de 1 s;
 *   manual  un estado concreto de la tabla (a mano).
 * Reglas de seguridad:
 *   - en hardware real sin confirmar (lista «confirmadas» del driver), el arranque solo LEE las tablas
 *     de datos de la BIOS: ni un reloj leído por la BIOS ni un cambio hasta que el administrador lo
 *     pida (o kconfig gpu_energia lo diga expresamente); el modo rescate y Mayús al arrancar no aplican
 *     kconfig;
 *   - antes del primer cambio se leen los relojes del arranque (GetEngineClock/GetMemoryClock): sin
 *     ellos no hay vuelta y no se cambia nada;
 *   - cada cambio: interrupciones cerradas, el 2D sincronizado y el motor parado (RBBM y el CP del 3D),
 *     el cambio y su comprobación por la BIOS; después la autoprueba corta del 2D y la del 3D si está
 *     encendido (en «auto», solo la primera vez que se entra en cada estado);
 *   - si algo falla: vuelta a los relojes y la tensión del arranque, modo bios y bloqueado para el
 *     resto del arranque (se desbloquea a mano: «acel2d energia desbloquear»); si tras volver el 2D
 *     sigue fallando, el vigilante lo apaga (la CPU dibuja).
 * El registro va con [GPUENERGIA]. */
#include "gpu_energia.h"
#include "abi.h"
#include "acel2d.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "kconf.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "video.h"

static const gpu_en_driver_t *drv;
static gpu_en_placa_t placa;
static bool iniciado;
static uint32_t modo = GPU_EN_MODO_BIOS, modo_kc, modo_arranque;
static int estado_atual = -1;
static uint32_t sclk, mclk, fonte, lido_s;            /* 10 kHz; fonte: 0 no se sabe, 1 leídos, 2 por defecto, 3 puestos */
static uint32_t sclk_arr, mclk_arr;
static bool arr_lido, bloqueado, kc_pendente, tarefa_ligada;
static uint32_t n_cambios, n_fallos, ultimo_us, provados;
static char ultimo[96] = "";
static gpu_en_auto_t aut;
static uint32_t ant2d, ant3d;
volatile uint32_t gpu_energia_pedido_s;              /* última vez que alguien miró (las zonas térmicas) */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint32_t soporte(void)
{
    if (!drv) return GPU_EN_SIN;
    return placa.pode_mudar ? GPU_EN_CONTROL : GPU_EN_LECTURA;
}

static void anotar(const char *m)
{
    txt_copia_n(ultimo, m, sizeof ultimo);
    klog("[GPUENERGIA] %s\n", m);
}

static const char *nome_modo(uint32_t m)
{
    static const char *const n[] = { "?", "bios", "bajo", "alto", "auto", "manual" };
    return m <= 5 ? n[m] : "?";
}

/* ------------------------------------------------------------ leer y cambiar */

static bool ler_agora(char *m, int nm)
{
    if (!drv || !placa.pode_ler) { snformatar(m, (size_t)nm, "esta tarjeta no da sus relojes"); return false; }
    uint32_t s = 0, k = 0;
    bool i0 = int_salva_desliga();
    bool ok = drv->ler(&s, &k, m, nm);
    int_restaura(i0);
    if (!ok) return false;
    sclk = s; mclk = k; fonte = 1; lido_s = relogio_segundos();
    if (!arr_lido && estado_atual < 0) {
        arr_lido = true; sclk_arr = s; mclk_arr = k;
        klog("[GPUENERGIA] relojes del arranque (por la BIOS): motor %u kHz, memoria %u kHz\n", s * 10, k * 10);
    }
    snformatar(m, (size_t)nm, "relojes leidos por la BIOS: motor %u MHz, memoria %u MHz", s / 100, k / 100);
    return true;
}

/* Pone el estado "idx" (-1: los relojes del arranque). ensaio: sin escribir nada. */
static bool cambiar(int idx, bool ensaio, bool probar_siempre, const char *porque, char *m, int nm);

static void volver_bios(const char *causa)
{
    char mm[96] = "";
    n_fallos++;
    bloqueado = true;
    modo = GPU_EN_MODO_BIOS;
    bool ok = true;
    if (arr_lido) {
        const gpu_en_estado_t *e = NULL;
        bool i0 = int_salva_desliga();
        acel_sincronizar();
        drv->parar_motor(500, mm, sizeof mm);
        /* «ahora» a 0: se repiten SetEngineClock y SetMemoryClock aunque el fallo parezca haberlos dejado igual */
        ok = drv->aplicar(e, sclk_arr, mclk_arr, 0, 0, false, mm, sizeof mm);
        int_restaura(i0);
        if (ok) { sclk = sclk_arr; mclk = mclk_arr; fonte = 1; lido_s = relogio_segundos(); }
        estado_atual = -1;
    }
    char t[96];
    int r2 = acel2d_energia_autoprova(t, sizeof t);
    if (r2 < 0) acel2d_desligar("la autoprueba fallo tras volver a los relojes de la BIOS (D399)");
    /* «ultimo» es corto (96): lo que importa va delante; el registro lleva todo */
    snformatar(ultimo, sizeof ultimo, "vuelta a la BIOS %s tras un fallo (bloqueado): %s", ok ? "bien" : "FALLIDA", causa);
    klog("[GPUENERGIA] fallo (%s): vuelta a la BIOS %s%s%s; bloqueado hasta reiniciar; 2D tras volver: %s\n", causa,
         ok ? "bien" : "FALLIDA", ok ? "" : ": ", ok ? "" : mm,
         r2 > 0 ? "igual que la CPU" : r2 == 0 ? "sin 2D por la GPU" : "FALLO (se apaga el 2D)");
}

static bool cambiar(int idx, bool ensaio, bool probar_siempre, const char *porque, char *m, int nm)
{
    if (!drv || !placa.pode_mudar) { snformatar(m, (size_t)nm, "esta tarjeta no deja cambiar los relojes (%s)", placa.motivo); return false; }
    if (bloqueado && !ensaio) { snformatar(m, (size_t)nm, "bloqueado tras un fallo (acel2d energia desbloquear)"); return false; }
    const gpu_en_estado_t *e = NULL;
    uint32_t s, k;
    if (idx < 0) {
        if (!arr_lido || (estado_atual < 0 && !ensaio)) { snformatar(m, (size_t)nm, "ya estan los relojes de la BIOS"); return true; }
        s = sclk_arr; k = mclk_arr;
    } else {
        if (idx >= placa.tab.n) { snformatar(m, (size_t)nm, "no hay estado %d (la tabla tiene %d)", idx, placa.tab.n); return false; }
        int ec = drv->ecras();
        if (!gpu_en_estado_serve(&placa.tab, idx, ec)) {
            snformatar(m, (size_t)nm, "el estado %d no vale con %d pantalla(s) encendida(s)", idx, ec);
            return false;
        }
        e = &placa.tab.e[idx];
        gpu_en_relojes_alvo(e, &placa.fw, &s, &k);
    }
    /* sin los relojes del arranque no hay vuelta: se leen antes del primer cambio */
    if (!arr_lido && !ler_agora(m, nm)) return false;
    if (!ensaio && idx == estado_atual) { snformatar(m, (size_t)nm, "ya esta en el estado %d", idx); return true; }
    char mm[96] = "";
    bool i0 = int_salva_desliga();
    acel_sincronizar();
    uint64_t t0 = tsc();
    bool ok = drv->parar_motor(500, mm, sizeof mm);
    if (ok) ok = drv->aplicar(e, s, k, sclk, mclk, ensaio, mm, sizeof mm);
    uint32_t khz = hw_tsc_khz();
    uint32_t us = khz ? (uint32_t)((tsc() - t0) * 1000u / khz) : 0;
    int_restaura(i0);
    if (ensaio) {
        snformatar(m, (size_t)nm, "%s estado %d (%u/%u MHz): %s", ok ? "ensayo del" : "ensayo FALLIDO del", idx, s / 100, k / 100, mm);
        klog("[GPUENERGIA] %s\n", m);
        return ok;
    }
    ultimo_us = us;
    if (ok) {
        sclk = s; mclk = k; fonte = 1; lido_s = relogio_segundos();
        estado_atual = idx;
        n_cambios++;
        uint32_t bit = 1u << (idx < 0 ? 31 : idx);
        if (probar_siempre || !(provados & bit)) {
            char t[96] = "";
            int r2 = acel2d_energia_autoprova(t, sizeof t);
            int r3 = r2 >= 0 ? gpu3d_energia_comprobar(t, sizeof t) : 0;
            if (r2 < 0 || r3 < 0) { ok = false; snformatar(mm, sizeof mm, "%s tras el cambio: %s", r2 < 0 ? "2D" : "3D", t); }
            else {
                provados |= bit;
                klog("[GPUENERGIA] comprobacion tras el cambio: 2D %s, 3D %s\n", r2 > 0 ? "igual que la CPU" : "sin 2D por la GPU",
                     r3 > 0 ? "responde" : "apagado");
            }
        }
    }
    if (!ok) {
        snformatar(m, (size_t)nm, "estado %d (%u/%u MHz): %s", idx, s / 100, k / 100, mm);
        klog("[GPUENERGIA] FALLO al cambiar a %s\n", m);
        volver_bios(mm);
        return false;
    }
    snformatar(m, (size_t)nm, "%s: estado %d, motor %u MHz, memoria %u MHz (%u us con el motor parado)", porque, idx, s / 100, k / 100, us);
    klog("[GPUENERGIA] %s\n", m);
    txt_copia_n(ultimo, m, sizeof ultimo);
    return true;
}

static int estado_de_modo(uint32_t md)
{
    int ec = drv ? drv->ecras() : 1;
    if (md == GPU_EN_MODO_BAJO) return gpu_en_estado_baixo(&placa.tab, ec);
    if (md == GPU_EN_MODO_ALTO) return gpu_en_estado_alto(&placa.tab);
    return -1;
}

static void tarefa(void *arg);

static int32_t por_modo(uint32_t md, const char *porque)
{
    char m[120];
    if (md < GPU_EN_MODO_BIOS || md > GPU_EN_MODO_AUTO) return ERRO_INVALIDO;
    if (md == GPU_EN_MODO_BIOS) {
        modo = md;
        bool ok = cambiar(-1, false, true, porque, m, sizeof m);
        anotar(m);
        return ok ? 0 : ERRO_INVALIDO;
    }
    if (!drv || !placa.pode_mudar) { snformatar(m, sizeof m, "esta tarjeta no deja cambiar los relojes: %s", drv ? placa.motivo : "sin driver"); anotar(m); return ERRO_NAO_SUPORTADO; }
    if (bloqueado) { anotar("bloqueado tras un fallo en este arranque (acel2d energia desbloquear)"); return ERRO_OCUPADO; }
    if (md == GPU_EN_MODO_AUTO) {
        if (!tarefa_ligada) { anotar("sin la tarea del gobernador: no hay modo auto"); return ERRO_NAO_SUPORTADO; }
        if (!arr_lido && !ler_agora(m, sizeof m)) { anotar(m); return ERRO_INVALIDO; }
        modo = md;
        aut = (gpu_en_auto_t){ 0, 0, GPU_EN_AUTO_PAUSA };
        ant2d = acel2d_ordens_total(); ant3d = gpu3d_lotes_total();
        snformatar(m, sizeof m, "%s: modo auto (empieza en %s)", porque, estado_atual < 0 ? "los relojes de la BIOS" : "el estado puesto");
        anotar(m);
        return 0;
    }
    int idx = estado_de_modo(md);
    if (idx < 0) { anotar("la tabla de la BIOS no tiene ese estado"); return ERRO_NAO_SUPORTADO; }
    bool ok = cambiar(idx, false, true, porque, m, sizeof m);
    if (ok) modo = md;
    anotar(m);
    return ok ? 0 : ERRO_INVALIDO;
}

/* ------------------------------------------------------------ la tarea (auto y lo de kconfig) */

static void tarefa(void *arg)
{
    (void)arg;
    static const char canal = 0;
    uint32_t inicio = relogio_segundos();
    for (;;) {
        int_desliga();
        if (kc_pendente && relogio_segundos() - inicio >= 5) {
            kc_pendente = false;
            klog("[GPUENERGIA] modo del arranque (kconfig gpu_energia): %s\n", nome_modo(modo_arranque));
            por_modo(modo_arranque, "kconfig");
        }
        uint32_t espera = 1000;
        if (modo == GPU_EN_MODO_AUTO && !bloqueado && drv) {
            espera = GPU_EN_AUTO_JANELA_MS;
            uint32_t a2 = acel2d_ordens_total(), a3 = gpu3d_lotes_total();
            int dir = gpu_en_auto_passo(&aut, a2 - ant2d, a3 - ant3d);
            ant2d = a2; ant3d = a3;
            if (dir) {
                int ref = estado_atual >= 0 ? estado_atual : placa.tab.padrao;
                int viz = gpu_en_vizinho(&placa.tab, ref, dir, drv->ecras());
                if (viz >= 0 && viz != estado_atual) {
                    char m[120];
                    cambiar(viz, false, false, dir > 0 ? "auto: sube" : "auto: baja", m, sizeof m);
                }
            }
        }
        processo_atual()->acordar_em = relogio_tiques() + espera * TIQUES_POR_SEGUNDO / 1000;
        bloquear(&canal);
        ceder();
        int_liga();
    }
}

/* ------------------------------------------------------------ arranque */

void gpu_energia_iniciar(void)
{
    if (iniciado) return;
    iniciado = true;
    memset(&placa, 0, sizeof placa);
    placa.temp_mili_c = GPU_EN_SIN_SENSOR;
    static const gpu_en_driver_t *const drivers[] = { &gpu_en_sim, &gpu_en_radeon };
    for (unsigned i = 0; i < sizeof drivers / sizeof drivers[0] && !drv; i++) {
        gpu_en_placa_t p;
        memset(&p, 0, sizeof p);
        p.temp_mili_c = GPU_EN_SIN_SENSOR;
        if (drivers[i]->sondar(&p)) { drv = drivers[i]; placa = p; }
    }
    if (!drv) {
        txt_copia_n(placa.motivo, "sin driver de energia para esta tarjeta (solo Radeon R5xx, por ahora)", sizeof placa.motivo);
        txt_copia_n(placa.temp_fuente, "sin driver para leerla", sizeof placa.temp_fuente);
        klog("[GPUENERGIA] %s\n", placa.motivo);
        return;
    }
    if (placa.fw.sclk_def) { sclk = placa.fw.sclk_def; mclk = placa.fw.mclk_def; fonte = 2; }
    modo_kc = kconf_valor(KC_GPU_ENERGIA);
    modo_arranque = modo_kc ? modo_kc : placa.confirmada ? GPU_EN_MODO_AUTO : GPU_EN_MODO_BIOS;
    bool seguro = (info_boot.opcoes & OPC_BOOT_ACEL_SEGURO) != 0;
    klog("[GPUENERGIA] %s: %s; %s; kconfig gpu_energia=%u -> %s%s\n", drv->nome, placa.motivo,
         placa.pode_mudar ? "puede cambiar los relojes" : placa.pode_ler ? "solo lectura" : "sin relojes",
         modo_kc, nome_modo(modo_arranque), seguro ? " (modo seguro: no se aplica)" : "");
    /* La tarea se crea ya aquí (y no al pedir «auto» desde una llamada al sistema): duerme 1 s cada vuelta
     * mientras no haya nada que hacer. */
    if (placa.pode_mudar) {
        kc_pendente = modo_arranque != GPU_EN_MODO_BIOS && !seguro;
        tarefa_ligada = tarefa_kernel("gpu-energia", tarefa, NULL) != NULL;
        if (!tarefa_ligada) klog("[GPUENERGIA] sin la tarea gpu-energia: no habra modo auto ni el de kconfig\n");
    }
}

/* ------------------------------------------------------------ llamadas */

int32_t gpu_energia_info(uint32_t ptr, uint32_t n)
{
    info_gpu_energia_t *v = (info_gpu_energia_t *)ptr;
    if (n < sizeof *v || !usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
    gpu_energia_pedido_s = relogio_segundos();
    memset(v, 0, sizeof *v);
    v->versao = 1;
    v->soporte = soporte();
    v->fabricante = placa.fabricante; v->modelo = placa.modelo;
    txt_copia_n(v->driver, drv ? drv->nome : "", sizeof v->driver);
    txt_copia_n(v->motivo, placa.motivo, sizeof v->motivo);
    txt_copia_n(v->bios, placa.bios, sizeof v->bios);
    v->modo = modo; v->modo_kconfig = kconf_valor(KC_GPU_ENERGIA);
    v->confirmada = placa.confirmada; v->bloqueado = bloqueado;
    v->estado = estado_atual;
    v->sclk_khz = sclk * 10; v->mclk_khz = mclk * 10;
    v->relojes_fuente = fonte;
    v->relojes_edad_s = fonte == 1 ? relogio_segundos() - lido_s : 0;
    v->sclk_def_khz = placa.fw.sclk_def * 10; v->mclk_def_khz = placa.fw.mclk_def * 10;
    v->sclk_max_khz = placa.fw.sclk_max * 10; v->mclk_max_khz = placa.fw.mclk_max * 10;
    v->sclk_arranque_khz = arr_lido ? sclk_arr * 10 : 0; v->mclk_arranque_khz = arr_lido ? mclk_arr * 10 : 0;
    v->ref_khz = placa.fw.ref * 10;
    v->tensao_mv = estado_atual < 0 ? placa.fw.vddc_arranque_mv : 0;
    if (estado_atual >= 0 && placa.tab.e[estado_atual].tensao == GPU_EN_V_GPIO)
        v->tensao_txt_baixa = (placa.tab.e[estado_atual].misc & ATOM_PM_TENSAO_GPIO) != 0;
    v->n_estados = (uint32_t)placa.tab.n;
    for (int i = 0; i < placa.tab.n && i < GPU_EN_MAX_ESTADOS_ABI; i++) {
        const gpu_en_estado_t *e = &placa.tab.e[i];
        info_gpu_estado_t *o = &v->estados[i];
        o->sclk_khz = e->sclk * 10; o->mclk_khz = e->mclk * 10;
        o->misc = e->misc; o->misc2 = e->misc2;
        o->tipo = e->tipo; o->tensao = e->tensao; o->sem_ecra = e->sem_ecra; o->so_um_ecra = e->so_um_ecra;
        o->pcie_lanes = e->pcie_lanes; o->indice_bios = e->indice_bios;
    }
    v->estado_bajo = drv ? estado_de_modo(GPU_EN_MODO_BAJO) : -1;
    v->estado_alto = drv ? estado_de_modo(GPU_EN_MODO_ALTO) : -1;
    v->temp_mili_c = placa.temp_mili_c;
    txt_copia_n(v->temp_fuente, placa.temp_fuente, sizeof v->temp_fuente);
    v->n_zonas = (uint32_t)zonas_termicas_ler(v->zona_mili_c, v->zona_nome, GPU_EN_MAX_ZONAS);
    v->vram_kb = placa.vram_kb ? placa.vram_kb : video_memoria_mapeada() >> 10;
    uint32_t t3, l3;
    gpu3d_memoria_uso(&t3, &l3);
    v->vram_usada_kb = (video_pitch() * video_altura()) / 1024 + (t3 - l3) + (acel2d_volcado_em_uso() ? 16 : 0);
    v->cambios = n_cambios; v->fallos = n_fallos; v->ultimo_us = ultimo_us;
    v->actividad = (uint32_t)aut.ocupadas;
    txt_copia_n(v->ultimo, ultimo, sizeof v->ultimo);
    return 0;
}

int32_t gpu_energia_ctl(uint32_t pedido)
{
    uint32_t op = pedido >> 16, arg = pedido & 0xFFFFu;
    char m[120];
    switch (op) {
    case GPU_EN_OP_MODO:
        return por_modo(arg, "a mano");
    case GPU_EN_OP_LEER: {
        bool ok = ler_agora(m, sizeof m);
        anotar(m);
        return ok ? 0 : ERRO_INVALIDO;
    }
    case GPU_EN_OP_ESTADO: {
        bool ok = cambiar((int)arg, false, true, "a mano", m, sizeof m);
        if (ok) modo = GPU_EN_MODO_MANUAL;
        anotar(m);
        return ok ? 0 : bloqueado ? ERRO_OCUPADO : ERRO_INVALIDO;
    }
    case GPU_EN_OP_SIMULAR: {
        bool ok = cambiar(arg == 0xFFFFu ? -1 : (int)arg, true, false, "ensayo", m, sizeof m);
        anotar(m);
        return ok ? 0 : ERRO_INVALIDO;
    }
    case GPU_EN_OP_DETALLE:
        if (!drv) { anotar("sin driver de energia"); return ERRO_NAO_SUPORTADO; }
        {
            bool i0 = int_salva_desliga();
            drv->detalle();
            int_restaura(i0);
        }
        klog("[GPUENERGIA] modo %s, estado %d, motor %u kHz, memoria %u kHz (fuente %u), arranque %u/%u kHz, cambios %u, fallos %u%s\n",
             nome_modo(modo), estado_atual, sclk * 10, mclk * 10, fonte, sclk_arr * 10, mclk_arr * 10, n_cambios, n_fallos,
             bloqueado ? ", BLOQUEADO" : "");
        anotar("detalle en dmesg ([GPUENERGIA])");
        return 0;
    case GPU_EN_OP_DESBLOQUEAR:
        bloqueado = false;
        anotar("desbloqueado a mano: se puede volver a cambiar en este arranque");
        return 0;
    }
    return ERRO_INVALIDO;
}
