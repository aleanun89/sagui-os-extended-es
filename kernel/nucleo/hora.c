// SPDX-License-Identifier: GPL-2.0-or-later
/* hora.c - la hora del sistema y la zona horaria (D080).
 *
 * Modelo de los Unix: el kernel guarda la hora en UTC y sólo convierte a la hora
 * local al mostrarla (reloj de la barra, SIS_DATA, fechas del FAT32).
 *
 *   - El RTC (reloj de la placa) puede guardar UTC o la hora local, como hacen
 *     las PC con Windows. Sin configuración, el kernel elige: en una máquina
 *     virtual (CPUID.1:ECX bit 31), UTC, que es el estándar de QEMU y de casi
 *     todas las VM que no corren Windows; en una PC de verdad, local, que es lo
 *     que la BIOS y Windows dejan grabado en las PC de la época del G470.
 *   - La zona es una regla POSIX TZ (fuso.c), leída de /home/.zona (disco
 *     persistente, D040) o de /etc/zona (en la RAM). Quien graba esos
 *     archivos es el programa "zona"; el kernel sólo lee en el arranque y aplica lo que
 *     SIS_ZONA mande.
 *   - El reloj avanza con los tics del PIT a partir de una lectura del RTC y
 *     vuelve a comprobar el RTC cada 10 minutos (el PIT a ~99,998 Hz adelanta
 *     ~1,3 s por día). Sólo se reajusta si la diferencia pasa de 1 s, para no
 *     estar saltando por la fracción de segundo que el RTC no informa.
 *   - Un ajuste sin grabar el RTC (ntp sin --rtc) queda como "corrección":
 *     segundos sumados a lo que dice el RTC. */
#include "relogio.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "conf_sistema.h"

#define RESINCRONIZAR (600 * TIQUES_POR_SEGUNDO)

static fuso_t fuso;
static config_zona_t cfg;
static uint32_t base_utc, base_tique, proxima_sinc;
static int32_t correcao;
static uint32_t ajustes;

static bool sob_hipervisor(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (c >> 31) & 1;
}

static uint32_t rtc_para_utc(const data_hora_t *d)
{
    uint32_t t = fuso_compor(d->ano, d->mes, d->dia, d->hora, d->minuto, d->segundo);
    return cfg.rtc_local ? fuso_para_utc(&fuso, t, NULL) : t;
}

static uint32_t rtc_agora(void)
{
    data_hora_t d;
    cmos_ler_data(&d);
    return rtc_para_utc(&d) + (uint32_t)correcao;
}

static uint32_t pelos_tiques(void)
{
    return base_utc + (relogio_tiques() - base_tique) / TIQUES_POR_SEGUNDO;
}

static void sincronizar(bool forcar)
{
    bool i0 = int_salva_desliga();
    uint32_t rtc = rtc_agora(), previsto = pelos_tiques();
    int32_t dif = (int32_t)(rtc - previsto);
    if (forcar || dif > 1 || dif < -1) {
        base_utc = rtc;
        base_tique = relogio_tiques();
    }
    proxima_sinc = relogio_tiques() + RESINCRONIZAR;
    int_restaura(i0);
}

/* La tarea de las ventanas (barra) lee la hora con las interrupciones activadas; una
 * llamada al sistema puede cambiar la base o la zona en medio. Las lecturas son
 * cortas y se hacen con las interrupciones desactivadas, para nunca ver la mitad. */
uint32_t hora_utc(void)
{
    bool i0 = int_salva_desliga();
    if ((int32_t)(relogio_tiques() - proxima_sinc) >= 0) sincronizar(false);
    uint32_t t = pelos_tiques();
    int_restaura(i0);
    return t;
}

void hora_local_de(uint32_t utc, fuso_data_t *d)
{
    bool i0 = int_salva_desliga();
    fuso_local(&fuso, utc, d);
    int_restaura(i0);
}

void hora_local(fuso_data_t *d) { hora_local_de(hora_utc(), d); }

uint32_t hora_local_para_utc(uint32_t local)
{
    bool i0 = int_salva_desliga();
    uint32_t t = fuso_para_utc(&fuso, local, NULL);
    int_restaura(i0);
    return t;
}
bool hora_24h(void) { return cfg.formato_24h != 0; }
uint32_t hora_ajustes(void) { return ajustes; }

static void registrar_hora(const char *porque)
{
    fuso_data_t u, l;
    uint32_t t = hora_utc();
    fuso_decompor(t, &u);
    fuso_local(&fuso, t, &l);
    klog("[HORA] %s: zona %s (%s), rtc %s%s, %04u-%02u-%02u %02u:%02u:%02u UTC = %02u:%02u:%02u %s\n", porque,
         cfg.nome, cfg.tz, cfg.rtc_local ? "local" : "utc", cfg.rtc_auto ? " (auto)" : "", u.ano, u.mes, u.dia,
         u.hora, u.minuto, u.segundo, l.hora, l.minuto, l.segundo, l.abrev);
}

static void padrao(void)
{
    memset(&cfg, 0, sizeof cfg);
    txt_copia_n(cfg.nome, "UTC", sizeof cfg.nome);
    txt_copia_n(cfg.tz, "UTC0", sizeof cfg.tz);
    cfg.rtc_auto = 1;
    cfg.rtc_local = !sob_hipervisor();
    cfg.formato_24h = 1;
    fuso_utc(&fuso);
}

void hora_iniciar(void)
{
    padrao();
    sincronizar(true);
    registrar_hora("inicio");
}

/* ------------------------------------------------------------ /etc/zona */

/* Líneas "clave=valor"; "#" comenta. Claves: zona, tz, rtc (utc|local),
 * formato (24|12). Una clave desconocida se ignora (versiones futuras). */
static bool aplicar_texto(const char *s, config_zona_t *c)
{
    bool achou_tz = false;
    while (*s) {
        const char *fim = s;
        while (*fim && *fim != '\n') fim++;
        const char *ig = s;
        while (ig < fim && *ig != '=') ig++;
        if (*s != '#' && ig < fim) {
            char chave[16], valor[64];
            uint32_t nc = (uint32_t)(ig - s), nv = (uint32_t)(fim - ig - 1);
            while (nv && (ig[nv] == '\r' || ig[nv] == ' ')) nv--;
            if (nc < sizeof chave && nv < sizeof valor) {
                memcpy(chave, s, nc); chave[nc] = 0;
                memcpy(valor, ig + 1, nv); valor[nv] = 0;
                if (txt_compara(chave, "zona") == 0) txt_copia_n(c->nome, valor, sizeof c->nome);
                else if (txt_compara(chave, "tz") == 0) { txt_copia_n(c->tz, valor, sizeof c->tz); achou_tz = true; }
                else if (txt_compara(chave, "rtc") == 0 && txt_compara(valor, "local") == 0) { c->rtc_local = 1; c->rtc_auto = 0; }
                else if (txt_compara(chave, "rtc") == 0 && txt_compara(valor, "utc") == 0) { c->rtc_local = 0; c->rtc_auto = 0; }
                else if (txt_compara(chave, "formato") == 0) c->formato_24h = txt_compara(valor, "12") != 0;
            }
        }
        s = *fim ? fim + 1 : fim;
    }
    return achou_tz;
}

static bool ler_arquivo(const char *caminho, config_zona_t *c)
{
    uint8_t *txt;
    uint32_t tam;
    if (vfs_ler_inteiro(caminho, &txt, &tam) < 0) return false;
    char *s = kmalloc(tam + 1);
    if (!s) { kfree(txt); return false; }
    memcpy(s, txt, tam);
    s[tam] = 0;
    kfree(txt);
    bool ok = aplicar_texto(s, c);
    kfree(s);
    return ok;
}

static int aplicar(const config_zona_t *nova)
{
    fuso_t f;
    if (!fuso_ler(nova->tz, &f)) return ERRO_INVALIDO;
    bool i0 = int_salva_desliga();
    /* Con el RTC en hora local, la zona nueva cambia la lectura de él: el RTC es la
     * referencia (es lo que dejaron la BIOS y Windows), así que el UTC del
     * sistema se recalcula. Con el RTC en UTC nada cambia en el UTC. */
    cfg = *nova;
    fuso = f;
    sincronizar(true);
    ajustes++;
    int_restaura(i0);
    return 0;
}

void hora_carregar_config(void)
{
    config_zona_t c = cfg;
    /* D371: con la configuración de la máquina separada, la zona es solo la de /etc (SAGUI-CONF). */
    bool sep = conf_sistema_separada();
    if (sep && ler_arquivo("/etc/zona", &c)) c.origem = ZONA_PERSISTENTE;
    else if (!sep && ler_arquivo("/home/.zona", &c)) c.origem = ZONA_PERSISTENTE;
    else if (!sep && ler_arquivo("/etc/zona", &c)) c.origem = ZONA_ARQUIVO;
    else { registrar_hora("sem /etc/zona"); return; }
    if (aplicar(&c) < 0) {
        klog("[HORA] regra TZ invalida em %s: \"%s\" (fica UTC)\n",
             c.origem == ZONA_PERSISTENTE && !sep ? "/home/.zona" : "/etc/zona", c.tz);
        return;
    }
    registrar_hora(c.origem == ZONA_PERSISTENTE && !sep ? "/home/.zona" : "/etc/zona");
}

/* ------------------------------------------------------------ llamadas */

int32_t sis_hora(uint32_t op, uint32_t arg, uint32_t arg2, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (op == HORA_UTC) {
        hora_t *h = (hora_t *)arg;
        uint32_t t = hora_utc();
        if (h) {
            if (!usuario_ok(h, sizeof *h, true)) return ERRO_ENDERECO;
            bool v;
            h->segundos = t;
            h->ms = (relogio_tiques() - base_tique) % TIQUES_POR_SEGUNDO * (1000 / TIQUES_POR_SEGUNDO);
            h->desvio = fuso_desvio(&fuso, t, &v);
            h->verao = v;
            h->ajustes = ajustes;
        }
        return (int32_t)t;
    }
    if (op == HORA_AJUSTAR) {
        if (arg < 946684800u) return ERRO_INVALIDO;   /* antes de 2000: error seguro */
        bool i0 = int_salva_desliga();
        if (arg2 & HORA_GRAVAR_RTC) {
            fuso_data_t l;
            if (cfg.rtc_local) fuso_local(&fuso, arg, &l);
            else fuso_decompor(arg, &l);
            data_hora_t r = { l.ano, l.mes, l.dia, l.hora, l.minuto, l.segundo };
            correcao = 0;
            cmos_gravar_data(&r);
        } else {
            correcao = 0;
            correcao = (int32_t)(arg - rtc_agora());
        }
        base_utc = arg;
        base_tique = relogio_tiques();
        proxima_sinc = base_tique + RESINCRONIZAR;
        ajustes++;
        int_restaura(i0);
        registrar_hora(arg2 & HORA_GRAVAR_RTC ? "acertada (sistema e RTC)" : "acertada (so o sistema)");
        return 0;
    }
    if (op == HORA_LER_RTC) {
        data_t *dt = (data_t *)arg;
        if (!usuario_ok(dt, sizeof *dt, true)) return ERRO_ENDERECO;
        data_hora_t h;
        cmos_ler_data(&h);
        dt->ano = h.ano; dt->mes = h.mes; dt->dia = h.dia;
        dt->hora = h.hora; dt->minuto = h.minuto; dt->segundo = h.segundo;
        dt->dia_semana = (uint8_t)dia_da_semana(h.ano, h.mes, h.dia);
        return 0;
    }
    return ERRO_INVALIDO;
}

int32_t sis_zona(uint32_t op, uint32_t ptr, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    config_zona_t *z = (config_zona_t *)ptr;
    if (!usuario_ok(z, sizeof *z, op == ZONA_LER)) return ERRO_ENDERECO;
    if (op == ZONA_LER) { *z = cfg; return 0; }
    if (op != ZONA_FIXAR) return ERRO_INVALIDO;
    config_zona_t nova = *z;
    nova.nome[sizeof nova.nome - 1] = 0;
    nova.tz[sizeof nova.tz - 1] = 0;
    nova.rtc_local = nova.rtc_local != 0;
    nova.formato_24h = nova.formato_24h != 0;
    if (nova.rtc_auto) nova.rtc_local = !sob_hipervisor();
    nova.origem = ZONA_PROGRAMA;
    int r = aplicar(&nova);
    if (r == 0) registrar_hora("SIS_ZONA");
    return r;
}
