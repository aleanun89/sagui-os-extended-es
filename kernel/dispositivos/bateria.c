// SPDX-License-Identifier: GPL-2.0-or-later
/* bateria.c - la batería del portátil por ACPI (D192).
 *
 * La batería es un dispositivo PNP0C0A ("Control Method Battery") con tres
 * métodos que ejecuta uACPI:
 *   _STA  bit 4: la batería está puesta;
 *   _BIX  (ACPI 4+) o _BIF: unidad (mW o mA), capacidad de diseño y de la
 *         última carga completa, tensión de diseño, niveles de aviso, ciclos,
 *         modelo, número de serie, tipo y fabricante;
 *   _BST  estado (descargando, cargando, crítica), consumo, capacidad que
 *         queda y tensión.
 * El cargador es un ACPI0003 con _PSR (1 = enchufado).
 *
 * Se leen cada 5 segundos desde la tarea "acpi-aml" y enseguida si el
 * firmware avisa con Notify (0x80: cambió el estado; 0x81: cambió la
 * información, por ejemplo otra batería). _BIF/_BIX solo al principio, al
 * poner la batería o con el aviso 0x81: en muchos portátiles son lentos
 * (el G570 duerme 20 ms entre lectura y lectura del EC). Lo leído se guarda
 * en "pub": la barra de tareas y INFO_BATERIA solo copian, nunca llaman a
 * uACPI. Los cálculos (porcentaje, tiempo) están en kernel/lib/bateria_conv.c. */
#include "uacpi_sagui.h"
#include "bateria.h"
#include "bateria_conv.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define PERIODO_TIQUES   (5 * TIQUES_POR_SEGUNDO)
#define PERIODO_FALHA    (60 * TIQUES_POR_SEGUNDO)   /* tras 3 lecturas fallidas seguidas: el registro no se llena */
#define MAX_ADAPTADORES  2
#define AVISO_BAIXA      10   /* % : aviso en la consola (la barra avisa por su cuenta) */
#define AVISO_REARMAR    12

typedef struct {
    uacpi_namespace_node *no;
    bool info_lida;
    char modelo[24], serie[16], tipo[12], fabricante[24];
    bateria_bruta_t b;
} bat_t;

static bat_t bats[BATERIA_MAX];
static int nbats;
static uacpi_namespace_node *adapt[MAX_ADAPTADORES];
static int nadapt;
static uint32_t adaptador;          /* 0 no hay, 1 desenchufado, 2 enchufado */
static volatile bool refrescar, reler_info;
static uint32_t ultima_leitura;     /* tique */
static bool avisou_baixa;
static uint32_t falhas_seguidas;   /* lecturas de _BST que fallaron (EC que no contesta...) */

/* Lo que ven los demás (barra, INFO_BATERIA). Se escribe con las
 * interrupciones desactivadas y se copia igual: nunca a medias. */
static info_bateria_t pub = {
    .versao = 1, .uacpi = UACPI_DESLIGADO, .porcentagem = -1, .minutos = -1,
};

/* ------------------------------------------------------------ paquetes */

static uint32_t pkg_int(const uacpi_object_array *a, uint32_t i)
{
    if (i >= a->count) return BATERIA_DESCONHECIDO;
    uacpi_u64 v;
    if (uacpi_object_get_integer(a->objects[i], &v) != UACPI_STATUS_OK) return BATERIA_DESCONHECIDO;
    return v > 0xFFFFFFFFull ? BATERIA_DESCONHECIDO : (uint32_t)v;
}

/* Un texto del paquete (cadena o búfer): solo ASCII imprimible, sin los
 * espacios del final (las BIOS rellenan con espacios o ceros). */
static void pkg_txt(const uacpi_object_array *a, uint32_t i, char *dst, uint32_t max)
{
    dst[0] = 0;
    if (i >= a->count) return;
    uacpi_data_view v;
    if (uacpi_object_get_string_or_buffer(a->objects[i], &v) != UACPI_STATUS_OK) return;
    uint32_t k = 0;
    for (uint32_t j = 0; j < v.length && k + 1 < max; j++) {
        uint8_t c = v.const_bytes[j];
        if (c == 0) break;
        dst[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    while (k && dst[k - 1] == ' ') k--;
    dst[k] = 0;
}

/* _BIX (ACPI 4.0+: revisión, unidad, diseño, completa, tecnología, tensión,
 * aviso, bajo, ciclos, ..., modelo en 16) o, si no lo hay, _BIF (unidad,
 * diseño, completa, tecnología, tensión, aviso, bajo, ..., modelo en 9). */
static bool ler_info(bat_t *x)
{
    uacpi_object *o = NULL;
    uacpi_object_array a;
    if (uacpi_eval_simple_package(x->no, "_BIX", &o) == UACPI_STATUS_OK &&
        uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 20) {
        x->b.unidade = pkg_int(&a, 1);
        x->b.projeto = pkg_int(&a, 2);
        x->b.total = pkg_int(&a, 3);
        x->b.projeto_mv = pkg_int(&a, 5);
        x->b.aviso = pkg_int(&a, 6);
        x->b.baixo = pkg_int(&a, 7);
        x->b.ciclos = pkg_int(&a, 8);
        pkg_txt(&a, 16, x->modelo, sizeof x->modelo);
        pkg_txt(&a, 17, x->serie, sizeof x->serie);
        pkg_txt(&a, 18, x->tipo, sizeof x->tipo);
        pkg_txt(&a, 19, x->fabricante, sizeof x->fabricante);
        uacpi_object_unref(o);
        return true;
    }
    if (o) uacpi_object_unref(o);
    o = NULL;
    if (uacpi_eval_simple_package(x->no, "_BIF", &o) == UACPI_STATUS_OK &&
        uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 13) {
        x->b.unidade = pkg_int(&a, 0);
        x->b.projeto = pkg_int(&a, 1);
        x->b.total = pkg_int(&a, 2);
        x->b.projeto_mv = pkg_int(&a, 4);
        x->b.aviso = pkg_int(&a, 5);
        x->b.baixo = pkg_int(&a, 6);
        x->b.ciclos = BATERIA_DESCONHECIDO;
        pkg_txt(&a, 9, x->modelo, sizeof x->modelo);
        pkg_txt(&a, 10, x->serie, sizeof x->serie);
        pkg_txt(&a, 11, x->tipo, sizeof x->tipo);
        pkg_txt(&a, 12, x->fabricante, sizeof x->fabricante);
        uacpi_object_unref(o);
        return true;
    }
    if (o) uacpi_object_unref(o);
    return false;
}

static bool ler_estado(bat_t *x)
{
    uacpi_object *o = NULL;
    uacpi_object_array a;
    bool ok = false;
    if (uacpi_eval_simple_package(x->no, "_BST", &o) == UACPI_STATUS_OK &&
        uacpi_object_get_package(o, &a) == UACPI_STATUS_OK && a.count >= 4) {
        x->b.estado = pkg_int(&a, 0);
        x->b.taxa = pkg_int(&a, 1);
        x->b.restante = pkg_int(&a, 2);
        x->b.tensao_mv = pkg_int(&a, 3);
        if (x->b.estado == BATERIA_DESCONHECIDO) x->b.estado = 0;
        ok = true;
    }
    if (o) uacpi_object_unref(o);
    return ok;
}

/* ------------------------------------------------------------ avisos del firmware */

/* Notify(BATx, 0x80/0x81) o Notify(ADP, 0x80): corre como trabajo diferido
 * en la tarea "acpi-aml" (nunca en la IRQ); solo pide otra lectura. */
static uacpi_status notificacao(uacpi_handle ctx, uacpi_namespace_node *no, uacpi_u64 valor)
{
    (void)ctx; (void)no;
    if (valor == 0x81) reler_info = true;
    refrescar = true;
    pub.notificacoes++;
    if (pub.notificacoes <= 16)   /* las primeras: el registro no se llena */
        klog("[BATERIA] aviso del firmware %x\n", (uint32_t)valor);
    return UACPI_STATUS_OK;
}

/* ------------------------------------------------------------ búsqueda */

static uacpi_iteration_decision achar(void *u, uacpi_namespace_node *no, uacpi_u32 prof)
{
    (void)u; (void)prof;
    static const char *const bat_ids[] = { "PNP0C0A", NULL };
    static const char *const ac_ids[] = { "ACPI0003", NULL };
    const char *tipo = NULL;
    if (nbats < BATERIA_MAX && uacpi_device_matches_pnp_id(no, bat_ids)) {
        bats[nbats].no = no;
        bats[nbats].b.existe = true;
        nbats++;
        tipo = "bateria (PNP0C0A)";
    } else if (nadapt < MAX_ADAPTADORES && uacpi_device_matches_pnp_id(no, ac_ids)) {
        adapt[nadapt++] = no;
        tipo = "cargador (ACPI0003)";
    }
    if (tipo) {
        uacpi_install_notify_handler(no, notificacao, NULL);
        const char *nome = uacpi_namespace_node_generate_absolute_path(no);
        klog("[BATERIA] %s: %s\n", tipo, nome ? nome : "?");
        if (nome) uacpi_free_absolute_path(nome);
    }
    return UACPI_ITERATION_DECISION_CONTINUE;
}

void bateria_iniciar(void)
{
    uacpi_namespace_for_each_child(uacpi_namespace_root(), achar, NULL, UACPI_OBJECT_DEVICE_BIT,
                                   UACPI_MAX_DEPTH_ANY, NULL);
    bool i0 = int_salva_desliga();
    pub.baterias = (uint32_t)nbats;
    pub.adaptador = nadapt ? 1 : 0;
    int_restaura(i0);
    adaptador = nadapt ? 1 : 0;
    if (!nbats && !nadapt) klog("[BATERIA] no hay bateria ni cargador ACPI\n");
    refrescar = true;
}

/* ------------------------------------------------------------ lectura */

static const char *texto_estado(const info_bateria_t *s)
{
    if (!s->presente) return "sin bateria";
    if (s->estado & BATERIA_CARREGANDO) return "cargando";
    if (s->estado & BATERIA_DESCARREGANDO) return "descargando";
    return s->adaptador == 2 ? "cargada" : "en reposo";
}

static void ler_tudo(void)
{
    /* El cargador: el primero que responda. */
    for (int i = 0; i < nadapt; i++) {
        uacpi_u64 v;
        if (uacpi_eval_simple_integer(adapt[i], "_PSR", &v) == UACPI_STATUS_OK) {
            adaptador = v ? 2 : 1;
            break;
        }
    }
    bateria_bruta_t brutas[BATERIA_MAX];
    for (int i = 0; i < nbats; i++) {
        bat_t *x = &bats[i];
        uint32_t sta = 0;
        if (uacpi_eval_sta(x->no, &sta) != UACPI_STATUS_OK) sta = 0;
        bool presente = (sta & 0x10) != 0;
        if (!presente) {
            x->b.presente = x->b.info_ok = x->b.estado_ok = false;
            x->info_lida = false;   /* al volver a ponerla, se lee otra vez */
        } else {
            x->b.presente = true;
            if (!x->info_lida || reler_info) {
                x->b.info_ok = ler_info(x);
                x->info_lida = x->b.info_ok;
            }
            x->b.estado_ok = ler_estado(x);
        }
        brutas[i] = x->b;
    }
    reler_info = false;
    bool falhou = false;
    for (int i = 0; i < nbats; i++) if (brutas[i].presente && !brutas[i].estado_ok) falhou = true;
    if (!falhou) falhas_seguidas = 0;
    else if (++falhas_seguidas == 3) klog("[BATERIA] _BST falla una y otra vez: se lee cada 60 s\n");

    info_bateria_t n = pub;
    bateria_calcular(brutas, nbats, adaptador, &n);
    n.adaptador = adaptador;
    n.baterias = (uint32_t)nbats;
    n.leituras = pub.leituras + 1;
    n.ec = ec_presente() ? 1 : 0;
    n.consultas_ec = ec_consultas();
    const bat_t *p = NULL;
    for (int i = 0; i < nbats; i++) if (bats[i].b.presente) { p = &bats[i]; break; }
    txt_copia_n(n.modelo, p ? p->modelo : "", sizeof n.modelo);
    txt_copia_n(n.serie, p ? p->serie : "", sizeof n.serie);
    txt_copia_n(n.tipo, p ? p->tipo : "", sizeof n.tipo);
    txt_copia_n(n.fabricante, p ? p->fabricante : "", sizeof n.fabricante);

    bool mudou = n.presente != pub.presente || n.estado != pub.estado || n.porcentagem != pub.porcentagem ||
                 n.adaptador != pub.adaptador || pub.leituras == 0;
    bool i0 = int_salva_desliga();
    n.notificacoes = pub.notificacoes;   /* puede haber cambiado mientras se leía */
    pub = n;
    ultima_leitura = relogio_tiques();
    int_restaura(i0);

    if (mudou) {
        char t[24];
        bateria_texto_tempo(n.minutos, t, sizeof t);
        klog("[BATERIA] %d %% %s, %u/%u %s, consumo %u, %s, cargador %u%s\n", n.porcentagem, texto_estado(&n),
             n.restante, n.total, n.unidade_ma ? "mAh" : "mWh", n.taxa, t[0] ? t : "tiempo ?", n.adaptador,
             n.presente ? "" : " (sin bateria)");
    }
    /* Aviso de batería baja en la consola. En el escritorio avisa la barra
     * de tareas (una notificación), no esta tarea. */
    bool baja = n.presente && (n.estado & BATERIA_DESCARREGANDO) && n.porcentagem >= 0 && n.porcentagem < AVISO_BAIXA;
    if (baja && !avisou_baixa) {
        avisou_baixa = true;
        klog("[BATERIA] aviso: bateria baja (%d %%)\n", n.porcentagem);
        if (!gui_ativa()) {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("\nBatería baja: %d %%. Conecta el cargador.\n", n.porcentagem);
            console_cor(COR_CINZA, COR_PRETO);
        }
    } else if (!n.presente || !(n.estado & BATERIA_DESCARREGANDO) || n.porcentagem >= AVISO_REARMAR) {
        avisou_baixa = false;
    }
}

void bateria_ciclo(void)
{
    if (!nbats && !nadapt) return;
    uint32_t agora = relogio_tiques();
    uint32_t periodo = falhas_seguidas >= 3 ? PERIODO_FALHA : PERIODO_TIQUES;
    if (refrescar || pub.leituras == 0 || agora - ultima_leitura >= periodo) {
        refrescar = false;
        ler_tudo();
    }
}

/* ------------------------------------------------------------ para el resto del kernel */

void bateria_uacpi_estado(uint32_t estado, const char *motivo)
{
    bool i0 = int_salva_desliga();
    pub.uacpi = estado;
    txt_copia_n(pub.motivo, motivo ? motivo : "", sizeof pub.motivo);
    int_restaura(i0);
}

bool bateria_estado(info_bateria_t *b)
{
    bool i0 = int_salva_desliga();
    *b = pub;
    b->idade_ms = pub.leituras ? (relogio_tiques() - ultima_leitura) * (1000 / TIQUES_POR_SEGUNDO) : 0;
    int_restaura(i0);
    return b->presente != 0;
}

int32_t bateria_info(uint32_t ptr, uint32_t n)
{
    if (n < sizeof(info_bateria_t) || !usuario_ok((void *)ptr, sizeof(info_bateria_t), true)) return ERRO_ENDERECO;
    info_bateria_t b;
    bateria_estado(&b);
    memcpy((void *)ptr, &b, sizeof b);
    return 0;
}
