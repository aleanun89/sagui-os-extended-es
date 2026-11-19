// SPDX-License-Identifier: GPL-2.0-or-later
/* wmi.c - el puente WMI de ACPI (dispositivos PNP0C14) sobre uACPI (D405).
 *
 * Capa común para todos los fabricantes: HP (hp_wmi.c) es el primero que la usa; Acer,
 * ASUS, Dell, Lenovo... tienen el mismo dispositivo con otros GUID. Implementado desde el
 * documento público de Microsoft «Windows Instrumentation: WMI and ACPI»:
 *
 *   _WDG   un búfer con un bloque de 20 bytes por cada GUID que ofrece el dispositivo
 *          (análisis en kernel/lib/wmi_wdg.c, que se prueba en el anfitrión);
 *   WMxx   método del bloque «xx» (ObjectId): (instancia, id del método, búfer);
 *   WQxx   leer un bloque de datos (instancia); WCxx(1/0) antes y después si es «caro»;
 *   WExx   encender (1) o apagar (0) un evento (xx = NotifyId en hexadecimal);
 *   _WED   los datos del último evento: _WED(NotifyId), tras un Notify(dispositivo, NotifyId).
 *
 * Todo corre en la tarea "acpi-aml" (la única que llama a uACPI, D192): wmi_iniciar()
 * después de la batería y los Notify como trabajo diferido de esa tarea. Nada aquí
 * escribe en el hardware por su cuenta: solo ejecuta los métodos que pida quien la usa.
 * Si _WDG no tiene sentido, ese dispositivo se ignora y el resto del sistema sigue. */
#include "uacpi_sagui.h"
#include "wmi_wdg.h"
#include <uacpi/internal/namespace.h>   /* solo wmi_zerar_inteiro: el objeto de un Name */
#include "console.h"
#include "texto.h"

#define WMI_MAX_DISP     4
#define WMI_MAX_BLOCOS   48
#define WMI_MAX_ASSIN    8
#define WMI_MAX_SAIDA    4096

static wmi_bloco_t blocos[WMI_MAX_BLOCOS];
static uint8_t bloco_disp[WMI_MAX_BLOCOS];      /* de qué dispositivo es cada bloque */
static int nblocos;
static uacpi_namespace_node *disps[WMI_MAX_DISP];
static int ndisps;
static uint32_t n_eventos, n_eventos_sem_dono;

static struct { uint8_t guid[16]; wmi_evento_f f; } assin[WMI_MAX_ASSIN];
static int nassin;

static void nome_no(uacpi_namespace_node *no, char *s, uint32_t max)
{
    const char *n = uacpi_namespace_node_generate_absolute_path(no);
    txt_copia_n(s, n ? n : "?", max);
    if (n) uacpi_free_absolute_path(n);
}

/* ------------------------------------------------------------ eventos */

static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 valor)
{
    int d = (int)(uintptr_t)ctx;
    n_eventos++;
    int b = -1;
    for (int i = 0; i < nblocos; i++)
        if (bloco_disp[i] == d && (blocos[i].flags & WMI_EVENTO) && blocos[i].notificacao == (uint8_t)valor) { b = i; break; }
    if (b < 0) {
        if (n_eventos <= 32) klog("[WMI] Notify %x sin bloque de evento en _WDG\n", (uint32_t)valor);
        return UACPI_STATUS_OK;
    }
    /* Los datos del evento: _WED(NotifyId). Puede no existir (evento sin datos). */
    uacpi_object *arg = uacpi_object_create_integer(valor);
    uacpi_object_array args = { &arg, 1 };
    uacpi_object *ret = NULL;
    uacpi_status r = arg ? uacpi_eval(no, "_WED", &args, &ret) : UACPI_STATUS_OUT_OF_MEMORY;
    if (arg) uacpi_object_unref(arg);
    uint8_t dados[64];
    uint32_t n = 0;
    if (r == UACPI_STATUS_OK && ret) {
        uacpi_data_view v;
        uacpi_u64 x;
        if (uacpi_object_get_buffer(ret, &v) == UACPI_STATUS_OK) {
            n = v.length < sizeof dados ? (uint32_t)v.length : sizeof dados;
            memcpy(dados, v.const_bytes, n);
        } else if (uacpi_object_get_integer(ret, &x) == UACPI_STATUS_OK) {
            for (n = 0; n < 8; n++) dados[n] = (uint8_t)(x >> (8 * n));
        }
    } else if (r != UACPI_STATUS_NOT_FOUND && n_eventos <= 32) {
        klog("[WMI] _WED(%x): %s\n", (uint32_t)valor, uacpi_status_to_string(r));
    }
    if (ret) uacpi_object_unref(ret);
    bool entregue = false;
    for (int i = 0; i < nassin; i++)
        if (memcmp(assin[i].guid, blocos[b].guid, 16) == 0) { assin[i].f((uint8_t)valor, dados, n); entregue = true; }
    if (!entregue && ++n_eventos_sem_dono <= 16) {
        char g[37];
        wmi_guid_texto(blocos[b].guid, g);
        klog("[WMI] evento %x de %s (%u bytes) sin nadie que lo atienda\n", (uint32_t)valor, g, n);
    }
    return UACPI_STATUS_OK;
}

bool wmi_assinar_evento(const char *guid_txt, wmi_evento_f f)
{
    uint8_t g[16];
    if (!wmi_guid_ler(guid_txt, g) || nassin >= WMI_MAX_ASSIN) return false;
    int b = wmi_achar(blocos, nblocos, g);
    if (b < 0 || !(blocos[b].flags & WMI_EVENTO)) return false;
    memcpy(assin[nassin].guid, g, 16);
    assin[nassin].f = f;
    nassin++;
    /* WExx(1): que la BIOS empiece a mandar el evento (si el método existe). */
    char m[5];
    wmi_nome_metodo(&blocos[b], "WE", m);
    uacpi_object *um = uacpi_object_create_integer(1);
    uacpi_object_array args = { &um, 1 };
    uacpi_status r = um ? uacpi_execute(disps[bloco_disp[b]], m, &args) : UACPI_STATUS_OUT_OF_MEMORY;
    if (um) uacpi_object_unref(um);
    klog("[WMI] evento %s (Notify %x) atendido; %s(1): %s\n", guid_txt, blocos[b].notificacao, m,
         r == UACPI_STATUS_OK ? "ok" : r == UACPI_STATUS_NOT_FOUND ? "no existe" : uacpi_status_to_string(r));
    return true;
}

/* ------------------------------------------------------------ búsqueda */

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const ids[] = { "PNP0C14", NULL };
    if (ndisps >= WMI_MAX_DISP || !uacpi_device_matches_pnp_id(no, ids)) return UACPI_ITERATION_DECISION_CONTINUE;
    char nome[64];
    nome_no(no, nome, sizeof nome);
    uint32_t sta = 0;
    if (uacpi_eval_sta(no, &sta) == UACPI_STATUS_OK && !(sta & 1)) {
        klog("[WMI] %s: _STA %x, no presente\n", nome, sta);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    uacpi_object *o = NULL;
    uacpi_data_view v;
    if (uacpi_eval_simple_buffer(no, "_WDG", &o) != UACPI_STATUS_OK || uacpi_object_get_buffer(o, &v) != UACPI_STATUS_OK) {
        klog("[WMI] %s: sin _WDG legible\n", nome);
        if (o) uacpi_object_unref(o);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    int livres = WMI_MAX_BLOCOS - nblocos;
    int n = wmi_wdg_analisar(v.const_bytes, (uint32_t)v.length, &blocos[nblocos], livres);
    uacpi_object_unref(o);
    if (n < 0) {
        klog("[WMI] %s: _WDG de %u bytes sin sentido: se ignora\n", nome, (uint32_t)v.length);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }
    if (n > livres) {
        klog("[WMI] %s: %d bloques, solo caben %d\n", nome, n, livres);
        n = livres;
    }
    int d = ndisps++;
    disps[d] = no;
    klog("[WMI] %s: %d bloques en _WDG\n", nome, n);
    for (int i = 0; i < n; i++) {
        wmi_bloco_t *b = &blocos[nblocos + i];
        bloco_disp[nblocos + i] = (uint8_t)d;
        char g[37];
        wmi_guid_texto(b->guid, g);
        if (b->flags & WMI_EVENTO)
            klog("[WMI]   %s evento Notify %x, %u instancias, flags %x\n", g, b->notificacao, b->instancias, b->flags);
        else
            klog("[WMI]   %s %s %s, %u instancias, flags %x\n", g, (b->flags & WMI_METODO) ? "metodo" : "datos", b->id,
                 b->instancias, b->flags);
    }
    nblocos += n;
    uacpi_status r = uacpi_install_notify_handler(no, notificacao, (uacpi_handle)(uintptr_t)d);
    if (r != UACPI_STATUS_OK) klog("[WMI] %s: Notify sin manejador: %s\n", nome, uacpi_status_to_string(r));
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void wmi_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_DEVICE_BIT, UACPI_MAX_DEPTH_ANY, NULL);
    if (!ndisps) klog("[WMI] no hay puente WMI (PNP0C14)\n");
}

/* ------------------------------------------------------------ llamadas */

static int bloco_de(const char *guid_txt, uint8_t flag_necessaria)
{
    uint8_t g[16];
    if (!wmi_guid_ler(guid_txt, g)) return -1;
    int b = wmi_achar(blocos, nblocos, g);
    if (b < 0) return -1;
    if (flag_necessaria && !(blocos[b].flags & flag_necessaria)) return -1;
    return b;
}

bool wmi_tem(const char *guid_txt) { return bloco_de(guid_txt, 0) >= 0; }

/* Copia el resultado (búfer, cadena o entero) en saida; *nsaida = los bytes que había
 * (aunque no quepan). */
static void copiar_resultado(uacpi_object *ret, void *saida, uint32_t max, uint32_t *nsaida)
{
    *nsaida = 0;
    if (!ret) return;
    uacpi_data_view v;
    uacpi_u64 x;
    if (uacpi_object_get_string_or_buffer(ret, &v) == UACPI_STATUS_OK) {
        uint32_t n = v.length < max ? (uint32_t)v.length : max;
        memcpy(saida, v.const_bytes, n);
        *nsaida = (uint32_t)v.length;
    } else if (uacpi_object_get_integer(ret, &x) == UACPI_STATUS_OK) {
        uint8_t *s = saida;
        for (uint32_t i = 0; i < 8 && i < max; i++) s[i] = (uint8_t)(x >> (8 * i));
        *nsaida = 8;
    }
}

int wmi_metodo(const char *guid_txt, uint32_t instancia, uint32_t metodo, const void *entrada, uint32_t nentrada,
               void *saida, uint32_t max, uint32_t *nsaida)
{
    int b = bloco_de(guid_txt, WMI_METODO);
    if (b < 0) return WMI_ERRO_SEM_BLOCO;
    if (instancia >= blocos[b].instancias) return WMI_ERRO_ARG;
    char m[5];
    wmi_nome_metodo(&blocos[b], "WM", m);
    uacpi_object *a[3];
    a[0] = uacpi_object_create_integer(instancia);
    a[1] = uacpi_object_create_integer(metodo);
    uacpi_data_view dv = { .const_data = entrada, .length = nentrada };
    a[2] = uacpi_object_create_buffer(dv);
    uacpi_object_array args = { a, 3 };
    uacpi_object *ret = NULL;
    uacpi_status r = (a[0] && a[1] && a[2]) ? uacpi_eval(disps[bloco_disp[b]], m, &args, &ret) : UACPI_STATUS_OUT_OF_MEMORY;
    for (int i = 0; i < 3; i++) if (a[i]) uacpi_object_unref(a[i]);
    if (r != UACPI_STATUS_OK) {
        klog("[WMI] %s(%u, %u, %u bytes): %s\n", m, instancia, metodo, nentrada, uacpi_status_to_string(r));
        if (ret) uacpi_object_unref(ret);
        return WMI_ERRO_AML;
    }
    copiar_resultado(ret, saida, max, nsaida);
    if (ret) uacpi_object_unref(ret);
    return 0;
}

int wmi_consultar(const char *guid_txt, uint32_t instancia, void *saida, uint32_t max, uint32_t *nsaida)
{
    int b = bloco_de(guid_txt, 0);
    if (b < 0 || (blocos[b].flags & (WMI_METODO | WMI_EVENTO))) return WMI_ERRO_SEM_BLOCO;
    if (instancia >= blocos[b].instancias) return WMI_ERRO_ARG;
    uacpi_namespace_node *no = disps[bloco_disp[b]];
    char m[5], c[5];
    wmi_nome_metodo(&blocos[b], "WQ", m);
    wmi_nome_metodo(&blocos[b], "WC", c);
    bool caro = (blocos[b].flags & WMI_CARO) != 0;
    if (caro) {
        uacpi_object *um = uacpi_object_create_integer(1);
        uacpi_object_array a1 = { &um, 1 };
        if (um) { (void)uacpi_execute(no, c, &a1); uacpi_object_unref(um); }
    }
    uacpi_object *arg = uacpi_object_create_integer(instancia);
    uacpi_object_array args = { &arg, 1 };
    uacpi_object *ret = NULL;
    uacpi_status r = arg ? uacpi_eval(no, m, &args, &ret) : UACPI_STATUS_OUT_OF_MEMORY;
    if (arg) uacpi_object_unref(arg);
    if (caro) {
        uacpi_object *zero = uacpi_object_create_integer(0);
        uacpi_object_array a0 = { &zero, 1 };
        if (zero) { (void)uacpi_execute(no, c, &a0); uacpi_object_unref(zero); }
    }
    if (r != UACPI_STATUS_OK) {
        if (ret) uacpi_object_unref(ret);
        return WMI_ERRO_AML;
    }
    copiar_resultado(ret, saida, max, nsaida);
    if (ret) uacpi_object_unref(ret);
    return 0;
}

/* Para INFO_RADIOS: los bloques que se han visto (para identificar fabricantes nuevos). */
int wmi_blocos(const wmi_bloco_t **v)
{
    *v = blocos;
    return nblocos;
}

uint32_t wmi_eventos(void) { return n_eventos; }

/* Pone a 0 un Name entero del dispositivo WMI que tiene ese GUID (p. ej. la marca con la
 * que la BIOS de HP sabe que el sistema atiende el WMI). true si existía y era entero. */
bool wmi_zerar_inteiro(const char *guid_txt, const char *nome)
{
    int b = bloco_de(guid_txt, 0);
    if (b < 0) return false;
    uacpi_namespace_node *n = NULL;
    if (uacpi_namespace_node_find(disps[bloco_disp[b]], nome, &n) != UACPI_STATUS_OK || !n) return false;
    uacpi_object *o = uacpi_namespace_node_get_object(n);
    uacpi_u64 v;
    if (!o || uacpi_object_get_integer(o, &v) != UACPI_STATUS_OK) return false;
    return uacpi_object_assign_integer(o, 0) == UACPI_STATUS_OK;
}
