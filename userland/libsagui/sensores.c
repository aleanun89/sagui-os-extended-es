// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores.c - textos del monitor de hardware (D092, mejora 050).
 *
 * Lo que no existe en la máquina (casi todo en una máquina virtual) sale como
 * "no disponible", nunca como error. La placa (ventiladores, temperaturas del
 * Super I/O y el SPD de la memoria por el SMBus) no se lee: queda pendiente y, si
 * algún día existe, será solo con una opción explícita (D092). */
#include "sensores.h"

bool sensores_ler(info_sensores_t *s)
{
    memset(s, 0, sizeof *s);
    s->pstate = -1;
    return info_sistema(INFO_SENSORES, s, sizeof *s) == 0;
}

const char *sensores_grau(char *b, int max, int mc)
{
    int g = mc >= 0 ? (mc + 500) / 1000 : -((-mc + 500) / 1000);
    formatar_texto(b, (size_t)max, "%d °C", g);
    return b;
}

const char *sensores_mhz(char *b, int max, unsigned khz)
{
    formatar_texto(b, (size_t)max, "%u MHz", (khz + 500) / 1000);
    return b;
}

int sensores_temp_maior(const info_sensores_t *s)
{
    int m = -1;
    for (unsigned i = 0; i < s->qtd_temp && i < SENS_MAX_TEMP; i++)
        if (s->temp[i].mili_c > m) m = s->temp[i].mili_c;
    return m;
}

unsigned sensores_freq_atual(const info_sensores_t *s, const char **origem)
{
    const char *o = "";
    unsigned khz = 0;
    if (s->real_khz) { khz = s->real_khz; o = "APERF/MPERF"; }
    else if (s->pstate >= 0 && (unsigned)s->pstate < s->qtd_pstates) { khz = s->pstate_khz[s->pstate]; o = "P-state"; }
    else if (s->mult_x10 && s->mult_max_x10 && s->tsc_khz) {
        /* El TSC va al multiplicador máximo sin turbo: el actual es proporcional. */
        khz = s->tsc_khz / 10 * s->mult_x10 / s->mult_max_x10 * 10;   /* sin división de 64 bits (no hay libgcc) */
        o = "multiplicador";
    }
    if (origem) *origem = o;
    return khz;
}

typedef struct { char *p; int n, max; } saida_t;

static void ad(saida_t *o, const char *fmt, ...)
{
    if (o->n >= o->max - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int k = formatar_lista(o->p + o->n, (size_t)(o->max - o->n), fmt, ap);
    va_end(ap);
    if (k > 0) o->n += k;
    if (o->n > o->max - 1) o->n = o->max - 1;
}

static void mult(char *b, int max, unsigned x10)
{
    if (x10 % 10) formatar_texto(b, (size_t)max, "x%u,%u", x10 / 10, x10 % 10);
    else formatar_texto(b, (size_t)max, "x%u", x10 / 10);
}

int sensores_texto(const info_sensores_t *s, char *buf, int max)
{
    saida_t o = { buf, 0, max };
    char a[24], b[24];
    if (max > 0) buf[0] = 0;
    if (s->qtd_temp) {
        ad(&o, "Temperatura:       ");
        for (unsigned i = 0; i < s->qtd_temp && i < SENS_MAX_TEMP; i++)
            ad(&o, "%s%s %s", i ? ", " : "", s->temp[i].nome, sensores_grau(a, sizeof a, s->temp[i].mili_c));
        if (s->max_mili_c) ad(&o, " (máx. %s, %s)", sensores_grau(a, sizeof a, s->max_mili_c), s->max_origem);
        ad(&o, "\n");
        ad(&o, "Fuente:            %s%s\n", s->fonte_txt,
           s->fonte == SENS_FONTE_AMD_K10 || s->fonte == SENS_FONTE_AMD_K8 ? " (escala de AMD, aprox.)" : "");
    } else {
        ad(&o, "Temperatura:       no disponible (%s)\n", s->motivo[0] ? s->motivo : "sin sensor conocido");
    }
    if (!(s->throttling & SENS_THR_SABE)) ad(&o, "Limitación:        no disponible\n");
    else if (s->throttling & (SENS_THR_TERMICO | SENS_THR_PROCHOT | SENS_THR_ENERGIA))
        ad(&o, "Limitación:        SÍ, ahora%s%s%s\n", s->throttling & SENS_THR_TERMICO ? " por temperatura" : "",
           s->throttling & SENS_THR_PROCHOT ? " (PROCHOT)" : "", s->throttling & SENS_THR_ENERGIA ? " por potencia" : "");
    else ad(&o, "Limitación:        no%s\n", s->throttling & SENS_THR_REGISTRO ? " (pero la hubo desde el arranque)" : "");

    if (s->real_khz) ad(&o, "Frecuencia real:   %s (APERF/MPERF en %u ms)\n", sensores_mhz(a, sizeof a, s->real_khz),
                        s->real_janela_ms);
    else ad(&o, "Frecuencia real:   no disponible (sin APERF/MPERF)\n");
    if (s->tsc_khz) ad(&o, "Frec. nominal:     %u,%02u MHz (TSC)\n", s->tsc_khz / 1000, s->tsc_khz % 1000 / 10);
    if (s->base_khz || s->max_khz)
        ad(&o, "Base / máxima:     %s / %s (CPUID 16H)\n", sensores_mhz(a, sizeof a, s->base_khz),
           sensores_mhz(b, sizeof b, s->max_khz));
    if (s->mult_x10) {
        mult(a, sizeof a, s->mult_x10);
        ad(&o, "Multiplicador:     %s", a);
        if (s->mult_max_x10) { mult(a, sizeof a, s->mult_max_x10); ad(&o, " (máximo %s", a); }
        if (s->mult_turbo_x10) { mult(a, sizeof a, s->mult_turbo_x10); ad(&o, ", turbo %s", a); }
        ad(&o, "%s\n", s->mult_max_x10 ? ")" : "");
    } else {
        ad(&o, "Multiplicador:     no disponible\n");
    }
    if (s->qtd_pstates) {
        ad(&o, "P-states:          ");
        for (unsigned i = 0; i < s->qtd_pstates && i < SENS_MAX_PSTATES; i++)
            ad(&o, "%sP%u %u MHz%s", i ? ", " : "", i, s->pstate_khz[i] / 1000, (int)i == s->pstate ? " (actual)" : "");
        ad(&o, "\n");
    }
    if (s->vid_mv) ad(&o, "Tensión (VID):     %u,%03u V (pedida al regulador, aprox.)\n", s->vid_mv / 1000, s->vid_mv % 1000);
    else ad(&o, "Tensión (VID):     no disponible\n");
    static const char *const turbo[] = { "no disponible", "la CPU no tiene", "desactivado por la BIOS", "activo" };
    ad(&o, "Turbo / Boost:     %s\n", turbo[s->turbo <= 3 ? s->turbo : 0]);
    if (s->microcodigo) ad(&o, "Microcódigo:       0x%x\n", s->microcodigo);
    else ad(&o, "Microcódigo:       no disponible\n");
    ad(&o, "Placa:             ventiladores y SPD sin leer (Super I/O y SMBus: pendiente, desactivado)\n");
    return o.n;
}

/* ------------------------------------------------------------ frecuencia (D106) */

bool frequencia_ler(info_frequencia_t *f)
{
    memset(f, 0, sizeof *f);
    f->atual = f->pedido = -1;
    return info_sistema(INFO_FREQUENCIA, f, sizeof *f) == 0;
}

int frequencia_nucleos(info_freq_nucleo_t *v, int max)
{
    int n = info_sistema(INFO_FREQ_NUCLEOS, v, (unsigned)max);
    return n < 0 ? 0 : n;
}

const char *frequencia_politica(unsigned p)
{
    static const char *const n[] = { "rendimiento", "ahorro", "a demanda" };
    return p <= 2 ? n[p] : "?";
}

int frequencia_texto(const info_frequencia_t *f, char *buf, int max)
{
    saida_t o = { buf, 0, max };
    char a[24];
    if (max > 0) buf[0] = 0;
    if (f->estado == FREQ_EST_NAO_DISPONIVEL) {
        ad(&o, "Control de frec.:  no disponible (%s)\n", f->motivo[0] ? f->motivo : "sin mecanismo conocido");
        return o.n;
    }
    static const char *const est[] = { "no disponible", "activo", "desactivado", "desactivado por un fallo" };
    ad(&o, "Control de frec.:  %s, política %s (%s)\n", est[f->estado <= 3 ? f->estado : 0],
       frequencia_politica(f->politica), f->mecanismo_txt);
    if (f->estado == FREQ_EST_FALHOU) ad(&o, "                   %s\n", f->motivo);
    ad(&o, "Estados:           ");
    for (unsigned i = 0; i < f->qtd_estados && i < FREQ_MAX_ESTADOS; i++) {
        ad(&o, "%s%s ", i ? ", " : "", f->estado_nome[i]);
        if (f->estado_khz[i]) ad(&o, "%s", sensores_mhz(a, sizeof a, f->estado_khz[i]));
        else ad(&o, "? MHz");
        if ((int)i == f->atual) ad(&o, " (actual)");
        if ((int)i == f->bios) ad(&o, " (BIOS)");
    }
    ad(&o, "\n");
    if (f->limitada)
        ad(&o, "Límite de temp.:   LIMITADA POR TEMPERATURA (%s; vuelve a subir por debajo de %d °C)\n",
           f->temp_mc != FREQ_TEMP_ND ? sensores_grau(a, sizeof a, f->temp_mc) : "sin lectura", f->retorno_mc / 1000);
    else
        ad(&o, "Límite de temp.:   %d °C (ahora %s%s; %u veces desde el arranque)\n", f->limite_mc / 1000,
           f->temp_mc != FREQ_TEMP_ND ? sensores_grau(a, sizeof a, f->temp_mc) : "sin lectura",
           f->temp_simulada ? ", simulada" : "", f->vezes_limitada);
    if (f->temp_mc == FREQ_TEMP_ND && f->estado == FREQ_EST_ATIVO)
        ad(&o, "                   %s\n", f->forcado ? "FORZADO: sube sin poder leer la temperatura"
                                                     : "sin temperatura: no pasa del estado de la BIOS");
    /* D175: con varias CPU, el estado de cada una (lo lee su propio registro). */
    if (f->versao >= 2 && f->cpus > 1) {
        static info_freq_nucleo_t v[8];
        int n = frequencia_nucleos(v, 8);
        ad(&o, "Por CPU:           coordinación %s\n", f->coordenacao == FREQ_COORD_PAQUETE ? "por paquete"
                                                                                       : "por núcleo físico");
        for (int i = 0; i < n; i++) {
            if (!v[i].ligada) continue;
            int at = v[i].atual, p = v[i].alvo;
            ad(&o, "  CPU %u (núcleo %u): %s ", v[i].cpu, v[i].nucleo,
               at >= 0 && at < (int)f->qtd_estados ? f->estado_nome[at] : "?");
            if (v[i].khz) ad(&o, "%s", sensores_mhz(a, sizeof a, v[i].khz));
            ad(&o, ", uso %u %%", v[i].uso);
            if (p >= 0 && p != at && p < (int)f->qtd_estados) ad(&o, ", pide %s", f->estado_nome[p]);
            if (v[i].bios >= 0 && v[i].bios < (int)f->qtd_estados) ad(&o, ", BIOS %s", f->estado_nome[v[i].bios]);
            ad(&o, "%s\n", v[i].pendente ? " (aplicando)" : "");
        }
    }
    return o.n;
}
