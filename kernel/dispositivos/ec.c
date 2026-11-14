// SPDX-License-Identifier: GPL-2.0-or-later
/* ec.c - el controlador embebido (EC, ACPI PNP0C09) para el AML (D192).
 *
 * En casi todos los portátiles la batería, el cargador, la tapa y los
 * ventiladores viven en el EC: el AML los lee con campos de una
 * OperationRegion(…, EmbeddedControl, …), y uACPI necesita quien haga esas
 * lecturas. Aquí está el protocolo de la especificación ACPI (capítulo 12):
 * dos puertos, datos (0x62) y órdenes/estado (0x66), sacados del _CRS del
 * dispositivo.
 *
 *   leer:     esperar IBF=0; orden 0x80; esperar IBF=0; dirección; esperar OBF=1; dato
 *   escribir: esperar IBF=0; orden 0x81; esperar IBF=0; dirección; esperar IBF=0; dato; esperar IBF=0
 *   consulta: esperar IBF=0; orden 0x84; esperar OBF=1; número del evento (0 = ninguno)
 *
 * Por sondeo, sin interrupción: acpi.c deja todos los GPE desactivados (nadie
 * ejecutaría sus métodos) y el del EC también. Los eventos del EC (SCI_EVT:
 * el cargador enchufado, la batería que cambia, las teclas Fn...) se miran
 * cada segundo desde la tarea "acpi-aml" y se ejecuta su método _Qxx, que
 * suele avisar con Notify a la batería o al cargador.
 *
 * Todo corre en la tarea "acpi-aml": nunca hay dos transacciones a la vez.
 * Si el EC no responde (timeout), la lectura falla y el AML recibe un error;
 * el sistema sigue. */
#include "uacpi_sagui.h"
#include "console.h"
#include "portas.h"
#include "texto.h"
#include "relogio.h"

#define EC_OBF      0x01   /* el EC dejó un dato para el sistema */
#define EC_IBF      0x02   /* el EC todavía no leyó lo último que se le escribió */
#define EC_SCI_EVT  0x20   /* hay un evento esperando a la consulta 0x84 */
#define EC_LER      0x80
#define EC_ESCREVER 0x81
#define EC_CONSULTA 0x84

#define ESPERA_RAPIDA_US 2000   /* lo normal: unos microsegundos */
#define ESPERA_MAX_MS    250    /* después, a pasos de 10 ms (la tarea duerme) */

static uacpi_namespace_node *no_ec;
static uint16_t porta_dados = 0x62, porta_cmd = 0x66;
static bool ec_ok;          /* manejador instalado */
static bool usa_glk;        /* _GLK = 1: el cerrojo global para las consultas */
static uint32_t n_consultas, n_timeouts;
static bool avisou_timeout;

static uint8_t estado(void) { return porta_le8(porta_cmd); }

/* Espera a que (estado & mascara) == valor. Primero gira unos 2 ms (lo
 * normal es que el EC conteste en microsegundos) y después duerme a pasos de
 * 10 ms hasta 250 ms, para no tener la CPU 0 ocupada con un EC lento. */
static bool esperar(uint8_t mascara, uint8_t valor)
{
    for (uint32_t i = 0; i < ESPERA_RAPIDA_US; i++) {
        if ((estado() & mascara) == valor) return true;
        porta_pausa();
    }
    for (uint32_t ms = 0; ms < ESPERA_MAX_MS; ms += 10) {
        uacpi_sagui_dormir_ms(10);
        if ((estado() & mascara) == valor) return true;
    }
    n_timeouts++;
    if (!avisou_timeout) {
        avisou_timeout = true;
        klog("[EC] sin respuesta del EC (estado %x): la lectura falla\n", estado());
    }
    return false;
}

static bool esperar_ibf_livre(void) { return esperar(EC_IBF, 0); }
static bool esperar_obf_cheio(void) { return esperar(EC_OBF, EC_OBF); }

/* Un dato viejo en el búfer de salida (de una transacción cortada) se
 * confundiría con la respuesta: se descarta antes de empezar. */
static void descartar_velho(void)
{
    for (int i = 0; i < 8 && (estado() & EC_OBF); i++) {
        (void)porta_le8(porta_dados);
        porta_pausa();
    }
}

static bool ec_ler(uint8_t endereco, uint8_t *v)
{
    descartar_velho();
    if (!esperar_ibf_livre()) return false;
    porta_escreve8(porta_cmd, EC_LER);
    if (!esperar_ibf_livre()) return false;
    porta_escreve8(porta_dados, endereco);
    if (!esperar_obf_cheio()) return false;
    *v = porta_le8(porta_dados);
    return true;
}

static bool ec_escrever(uint8_t endereco, uint8_t v)
{
    descartar_velho();
    if (!esperar_ibf_livre()) return false;
    porta_escreve8(porta_cmd, EC_ESCREVER);
    if (!esperar_ibf_livre()) return false;
    porta_escreve8(porta_dados, endereco);
    if (!esperar_ibf_livre()) return false;
    porta_escreve8(porta_dados, v);
    return esperar_ibf_livre();
}

/* El manejador de la región EmbeddedControl: uACPI da la dirección absoluta
 * dentro del EC (0-255) y el ancho del campo; los anchos mayores que un byte
 * son bytes seguidos, el menos significativo primero (especificación 12.3). */
static uacpi_status regiao_ec(uacpi_region_op op, uacpi_handle dados)
{
    if (op == UACPI_REGION_OP_ATTACH || op == UACPI_REGION_OP_DETACH) return UACPI_STATUS_OK;
    if (op != UACPI_REGION_OP_READ && op != UACPI_REGION_OP_WRITE) return UACPI_STATUS_INVALID_ARGUMENT;
    uacpi_region_rw_data *rw = dados;
    if (rw->byte_width < 1 || rw->byte_width > 8 || rw->offset + rw->byte_width > 256)
        return UACPI_STATUS_INVALID_ARGUMENT;
    uint32_t base = (uint32_t)rw->offset;
    if (op == UACPI_REGION_OP_READ) {
        uint64_t v = 0;
        for (uint32_t i = 0; i < rw->byte_width; i++) {
            uint8_t b;
            if (!ec_ler((uint8_t)(base + i), &b)) return UACPI_STATUS_HARDWARE_TIMEOUT;
            v |= (uint64_t)b << (8 * i);
        }
        rw->value = v;
    } else {
        for (uint32_t i = 0; i < rw->byte_width; i++)
            if (!ec_escrever((uint8_t)(base + i), (uint8_t)(rw->value >> (8 * i))))
                return UACPI_STATUS_HARDWARE_TIMEOUT;
    }
    return UACPI_STATUS_OK;
}

/* Los dos puertos del _CRS: el primero es el de datos y el segundo el de
 * órdenes y estado (especificación 12.11). */
typedef struct { uint16_t porta[2]; int n; } portas_crs_t;

static uacpi_iteration_decision recurso(void *u, uacpi_resource *r)
{
    portas_crs_t *p = u;
    uint16_t porta = 0;
    if (r->type == UACPI_RESOURCE_TYPE_IO) porta = r->io.minimum;
    else if (r->type == UACPI_RESOURCE_TYPE_FIXED_IO) porta = r->fixed_io.address;
    else return UACPI_ITERATION_DECISION_CONTINUE;
    if (p->n < 2) p->porta[p->n++] = porta;
    return p->n < 2 ? UACPI_ITERATION_DECISION_CONTINUE : UACPI_ITERATION_DECISION_BREAK;
}

/* El primer dispositivo PNP0C09, sin evaluar el _STA de los demás (antes
 * de los _INI, el _STA de otros dispositivos puede leer el EC mismo). */
static uacpi_iteration_decision achar_ec(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)prof;
    static const char *const ids[] = { "PNP0C09", NULL };
    if (!uacpi_device_matches_pnp_id(no, ids)) return UACPI_ITERATION_DECISION_CONTINUE;
    *(uacpi_namespace_node **)u = no;
    return UACPI_ITERATION_DECISION_BREAK;
}

bool ec_iniciar(void)
{
    uacpi_namespace_node *no = NULL;
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar_ec, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, &no);
    if (!no) {
        klog("[EC] no hay controlador embebido (PNP0C09)\n");
        return false;
    }
    uint32_t sta = 0;
    if (uacpi_eval_sta(no, &sta) == UACPI_STATUS_OK && !(sta & 1)) {
        klog("[EC] el PNP0C09 dice que no esta presente (_STA %x)\n", sta);
        return false;
    }
    portas_crs_t p = { { 0, 0 }, 0 };
    uacpi_for_each_device_resource(no, "_CRS", recurso, &p);
    if (p.n == 2 && p.porta[0] && p.porta[1]) {
        porta_dados = p.porta[0];
        porta_cmd = p.porta[1];
    } else {
        klog("[EC] _CRS sin los dos puertos: se usan 0x62/0x66\n");
    }
    uacpi_u64 glk = 0;
    usa_glk = uacpi_eval_simple_integer(no, "_GLK", &glk) == UACPI_STATUS_OK && glk;
    /* Un EC que no existe en esos puertos lee 0xFF: IBF siempre a 1. */
    if (estado() == 0xFF) {
        klog("[EC] los puertos %x/%x leen 0xFF: no hay EC\n", porta_dados, porta_cmd);
        return false;
    }
    no_ec = no;
    /* Instalar el manejador ejecuta el _REG del EC (el AML anota que ya puede
     * usarlo: ECOK, OKEC...). */
    uacpi_status r = uacpi_install_address_space_handler(no, UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER, regiao_ec, NULL);
    if (r != UACPI_STATUS_OK) {
        klog("[EC] no se pudo instalar el manejador: %s\n", uacpi_status_to_string(r));
        return false;
    }
    ec_ok = true;
    const char *nome = uacpi_namespace_node_generate_absolute_path(no);
    klog("[EC] %s en los puertos %x/%x%s, estado %x\n", nome ? nome : "?", porta_dados, porta_cmd,
         usa_glk ? " (con el cerrojo global)" : "", estado());
    if (nome) uacpi_free_absolute_path(nome);
    return true;
}

/* Hasta 8 eventos por vuelta: un EC que no para de pedir no bloquea la tarea. */
void ec_atender_eventos(void)
{
    if (!ec_ok) return;
    for (int k = 0; k < 8 && (estado() & EC_SCI_EVT); k++) {
        uacpi_u32 seq = 0;
        bool cerrojo = false;
        if (usa_glk) {
            if (uacpi_acquire_global_lock(100, &seq) != UACPI_STATUS_OK) return;
            cerrojo = true;
        }
        uint8_t q = 0;
        bool ok = false;
        descartar_velho();
        if (esperar_ibf_livre()) {
            porta_escreve8(porta_cmd, EC_CONSULTA);
            if (esperar_obf_cheio()) {
                q = porta_le8(porta_dados);
                ok = true;
            }
        }
        if (cerrojo) uacpi_release_global_lock(seq);
        if (!ok || q == 0) return;
        static const char hex[] = "0123456789ABCDEF";
        char metodo[5] = { '_', 'Q', hex[q >> 4], hex[q & 15], 0 };
        n_consultas++;
        uacpi_status r = uacpi_execute_simple(no_ec, metodo);
        if (n_consultas <= 64 || r != UACPI_STATUS_OK)   /* el registro no se llena con las teclas Fn */
            klog("[EC] evento %x: %s %s\n", q, metodo, r == UACPI_STATUS_OK ? "ok" : uacpi_status_to_string(r));
        uacpi_sagui_trabalho_pendente();   /* los Notify que haya dejado el _Qxx */
    }
}

bool ec_presente(void) { return ec_ok; }
uint32_t ec_consultas(void) { return n_consultas; }
