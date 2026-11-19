// SPDX-License-Identifier: GPL-2.0-or-later
/* hp_wmi.c - radios y teclas del panel de los portátiles HP por WMI (D405).
 *
 * Los HP de consumo (Pavilion dv/G, Compaq Presario...) llevan en la DSDT un dispositivo
 * PNP0C14 (normalmente \_SB.WMID) con dos GUID que interesan aquí:
 *   5FB7F034-2C63-45E9-BE91-3D44E2C707E4  método (WMAD o WMAA): consultas a la BIOS;
 *   95F24279-4D7B-4334-9387-ACCDC67EF61C  evento (Notify 0x80): _WED da (evento, dato).
 * La consulta es un búfer «SECU», orden (1 leer, 2 escribir), tipo, tamaño y datos; la
 * respuesta, firma, código (0 = bien) y datos (kernel/lib/wmi_wdg.c). Es lo que dice el AML
 * de las propias BIOS (HWMC/WHCM): no se ha copiado código de ningún otro driver.
 *
 * Radios (tipo 5): el primer byte dice qué radios hay; cada radio tiene su byte con
 * 0x02 = encendida por software y 0x08 = no la bloquea el botón. Una radio funciona si
 * tiene las dos. En estas BIOS, la primera lectura avisa a la BIOS de que el sistema
 * atiende el WMI: a partir de ahí el botón inalámbrico cambia el bloqueo «por hardware» y
 * manda el evento 5, y el sistema solo tiene que leer y avisar.
 *
 * Tarjeta Wi-Fi que aparece: en estos HP la tarjeta desaparece del PCI con la radio
 * apagada. Al encenderla, se vuelve a mirar el bus (el PCI de SaguiOS se lee siempre del
 * hardware, no hay lista guardada que refrescar) a los 3, 6 y 12 s; si aparece una función
 * nueva se avisa de que hay que reiniciar para usarla (los drivers se eligen al arrancar y
 * la BIOS no le dio recursos); si no aparece, también. La BIOS de HP guarda el estado de la
 * radio: tras reiniciar sigue encendida.
 *
 * Todo corre en la tarea "acpi-aml". Los programas piden (INFO_RADIOS_MUDAR) con un hueco
 * estático: nada que pueda quedar apuntando a la pila de quien pidió. */
#include "uacpi_sagui.h"
#include "radios.h"
#include "acoes.h"
#include "bateria.h"
#include "console.h"
#include "cpu.h"
#include "pci.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define RESPOSTA_MAX     (HPWMI_RESPOSTA_CAB + 128)
#define ENTRADA_DADOS    128          /* las BIOS esperan el búfer entero aunque los datos sean menos */
#define PCI_MAX          96
#define ESPERA_PEDIDO_MS 5000

static const char *const nomes_radio[HPWMI_RADIOS] = { "Wi-Fi", "Bluetooth", "WWAN" };

static bool hp_ok;
static bool botao_da_bios;   /* ver devolver_botao() */
static uint8_t estado[4];
static hpwmi_radio_t radios[HPWMI_RADIOS];
static bool estado_lido;

/* Lo que ven los programas (INFO_RADIOS): se escribe con las interrupciones apagadas. */
static info_radios_t pub = { .versao = 1 };

/* El pedido de un programa: un único hueco. */
static volatile bool pedido_pendente, pedido_feito;
static volatile uint32_t pedido_valor;
static volatile int32_t pedido_resultado;
static const char canal_espera = 0;   /* solo para dormir con el reloj de red */

/* La vuelta por el PCI tras encender la Wi-Fi. */
static pci_disp_t pci_inicio[PCI_MAX];
static int n_pci_inicio;
static uint32_t pci_mirar_em;   /* tique; 0 = nada pendiente */
static int pci_vueltas;

static void nota(const char *t)
{
    bool i0 = int_salva_desliga();
    txt_copia_n(pub.nota, t, sizeof pub.nota);
    int_restaura(i0);
}

static void publicar(void)
{
    info_radios_t n = pub;
    n.hp = hp_ok;
    n.lido = estado_lido;
    n.presentes = n.sw = n.hw = 0;
    for (int i = 0; i < HPWMI_RADIOS; i++) {
        if (radios[i].presente) n.presentes |= 1u << i;
        if (radios[i].sw) n.sw |= 1u << i;
        if (radios[i].hw) n.hw |= 1u << i;
    }
    n.eventos = wmi_eventos();
    const wmi_bloco_t *v;
    int nb = wmi_blocos(&v);
    n.n_wmi = (uint32_t)nb;
    for (int i = 0; i < nb && i < RADIOS_WMI_MAX; i++) {
        wmi_guid_texto(v[i].guid, n.wmi[i].guid);
        n.wmi[i].id[0] = v[i].id[0];
        n.wmi[i].id[1] = v[i].id[1];
        n.wmi[i].id[2] = 0;
        n.wmi[i].flags = v[i].flags;
        n.wmi[i].instancias = v[i].instancias;
        n.wmi[i].notificacao = v[i].notificacao;
    }
    bool i0 = int_salva_desliga();
    txt_copia_n(n.nota, pub.nota, sizeof n.nota);   /* puede haber cambiado */
    pub = n;
    int_restaura(i0);
}

/* ------------------------------------------------------------ consultas a la BIOS */

/* Una consulta: devuelve el código de la BIOS (0 = bien) o, si ni siquiera se pudo
 * llamar, un valor negativo. "resp" recibe los datos de la respuesta. */
static int consultar(uint32_t ordem, uint32_t tipo, const void *dados, uint32_t n, uint8_t *resp, uint32_t max,
                     uint32_t *nresp)
{
    uint8_t ent[HPWMI_CABECALHO + ENTRADA_DADOS];
    uint8_t d[ENTRADA_DADOS];
    memset(d, 0, sizeof d);
    if (n > sizeof d) return WMI_ERRO_ARG;
    if (n) memcpy(d, dados, n);
    uint32_t nent = hpwmi_montar(ent, sizeof ent, ordem, tipo, d, sizeof d);
    /* El tamaño declarado es el de los datos de verdad, no el del relleno. */
    ent[12] = (uint8_t)n; ent[13] = (uint8_t)(n >> 8); ent[14] = 0; ent[15] = 0;
    uint8_t saida[RESPOSTA_MAX];
    uint32_t nsaida = 0;
    int r = wmi_metodo(HPWMI_GUID_BIOS, 0, hpwmi_id_metodo(max), ent, nent, saida, sizeof saida, &nsaida);
    if (r < 0) return r;
    if (nsaida > sizeof saida) nsaida = sizeof saida;
    uint32_t codigo;
    const uint8_t *rd;
    uint32_t nrd;
    if (!hpwmi_resposta(saida, nsaida, &codigo, &rd, &nrd)) return WMI_ERRO_AML;
    if (nresp) *nresp = nrd < max ? nrd : max;
    if (resp && nrd) memcpy(resp, rd, nrd < max ? nrd : max);
    bool i0 = int_salva_desliga();
    pub.codigo = codigo;
    int_restaura(i0);
    return (int)codigo;
}

static bool ler_radios(void)
{
    uint8_t r[4] = { 0, 0, 0, 0 };
    uint32_t n = 0;
    int c = consultar(HPWMI_LER, HPWMI_T_RADIOS, NULL, 0, r, sizeof r, &n);
    if (c != 0 || n < 4) {
        klog("[HPWMI] leer las radios: codigo %d, %u bytes\n", c, n);
        estado_lido = false;
        return false;
    }
    memcpy(estado, r, 4);
    hpwmi_radios_ler(estado, radios);
    estado_lido = true;
    return true;
}

static void registrar_radios(const char *quando)
{
    klog("[HPWMI] radios (%s): estado %x %x %x %x\n", quando, estado[0], estado[1], estado[2], estado[3]);
    for (int i = 0; i < HPWMI_RADIOS; i++)
        if (radios[i].presente)
            klog("[HPWMI]   %s: software %s, hardware %s\n", nomes_radio[i], radios[i].sw ? "encendida" : "apagada",
                 radios[i].hw ? "libre" : "bloqueada (boton)");
}

/* ------------------------------------------------------------ el PCI tras encender */

static bool pci_igual(const pci_disp_t *a, const pci_disp_t *b)
{
    return a->barramento == b->barramento && a->dispositivo == b->dispositivo && a->funcao == b->funcao &&
           a->fabricante == b->fabricante && a->modelo == b->modelo;
}

static void pci_mirar(void)
{
    static pci_disp_t agora[PCI_MAX];
    int n = pci_listar(agora, PCI_MAX);
    uint32_t novos = 0;
    for (int i = 0; i < n; i++) {
        bool velho = false;
        for (int k = 0; k < n_pci_inicio && !velho; k++) velho = pci_igual(&agora[i], &pci_inicio[k]);
        if (velho) continue;
        novos++;
        char t[160];
        snformatar(t, sizeof t, "Ha aparecido una tarjeta nueva en el PCI (%x:%x.%x, %04x:%04x). Reinicia para usarla: "
                   "los drivers se eligen al arrancar.", agora[i].barramento, agora[i].dispositivo, agora[i].funcao,
                   agora[i].fabricante, agora[i].modelo);
        klog("[HPWMI] PCI: nueva %x:%x.%x %04x:%04x clase %x.%x\n", agora[i].barramento, agora[i].dispositivo,
             agora[i].funcao, agora[i].fabricante, agora[i].modelo, agora[i].classe, agora[i].subclasse);
        if (!pub.pci_novos) acoes_aviso(t);
    }
    bool i0 = int_salva_desliga();
    if (novos) { pub.pci_novos = novos; pub.reiniciar = 1; }
    int_restaura(i0);
    if (novos) { pci_mirar_em = 0; nota("tarjeta nueva en el PCI: reinicia para usarla"); return; }
    static const uint32_t esperas_s[] = { 3, 6 };
    if (pci_vueltas < 2) {
        pci_mirar_em = relogio_tiques() + esperas_s[pci_vueltas++] * TIQUES_POR_SEGUNDO;
        return;
    }
    pci_mirar_em = 0;
    if (botao_da_bios) {   /* no se sabe si el botón la encendió o la apagó: sin aviso */
        klog("[HPWMI] PCI: ninguna tarjeta nueva tras el boton\n");
        return;
    }
    klog("[HPWMI] PCI: la tarjeta no ha aparecido tras encender la radio\n");
    i0 = int_salva_desliga();
    pub.reiniciar = 1;
    int_restaura(i0);
    nota("la tarjeta no aparece en el PCI: reinicia (la BIOS recuerda la radio)");
    acoes_aviso("La Wi-Fi está encendida pero la tarjeta no aparece en el PCI: reinicia para que aparezca "
                "(la BIOS recuerda el estado de la radio).");
}

static void pci_programar(void)
{
    if (!pub.reiniciar) {
        pci_vueltas = 0;
        pci_mirar_em = relogio_tiques() + 3 * TIQUES_POR_SEGUNDO;
    }
}

/* ------------------------------------------------------------ cambios */

static bool funciona(int i) { return radios[i].presente && radios[i].sw && radios[i].hw; }

/* Tras releer: avisa de lo que cambió. "botao" = vino del botón inalámbrico. */
static void comparar(const hpwmi_radio_t antes[HPWMI_RADIOS], bool botao)
{
    for (int i = 0; i < HPWMI_RADIOS; i++) {
        if (!radios[i].presente) continue;
        bool a = antes[i].presente && antes[i].sw && antes[i].hw, d = funciona(i);
        if (a == d && antes[i].hw == radios[i].hw && antes[i].sw == radios[i].sw) continue;
        char t[120];
        if (d) snformatar(t, sizeof t, "%s encendida%s.", nomes_radio[i], botao ? " (botón inalámbrico)" : "");
        else if (!radios[i].hw) snformatar(t, sizeof t, "%s apagada por el botón inalámbrico.", nomes_radio[i]);
        else snformatar(t, sizeof t, "%s apagada.", nomes_radio[i]);
        acoes_aviso(t);
        nota(t);
        klog("[HPWMI] %s\n", t);
        if (i == HPWMI_RADIO_WIFI && d && !a) pci_programar();
        if (i == HPWMI_RADIO_WIFI && !d) pci_mirar_em = 0;   /* se apagó antes de mirar: nada que esperar */
    }
}

static int32_t mudar(uint32_t valor)
{
    int radio = (int)(valor & 0xFF);
    bool ligar = ((valor >> 8) & 0xFF) != 0;
    if (radio < 0 || radio >= HPWMI_RADIOS) return ERRO_INVALIDO;
    if (!hp_ok) return ERRO_NAO_EXISTE;
    if (estado_lido && !radios[radio].presente) return ERRO_NAO_EXISTE;
    hpwmi_radio_t antes[HPWMI_RADIOS];
    memcpy(antes, radios, sizeof antes);
    uint32_t pedido = hpwmi_radios_pedido(radio, ligar);
    uint8_t d[4] = { (uint8_t)pedido, (uint8_t)(pedido >> 8), (uint8_t)(pedido >> 16), (uint8_t)(pedido >> 24) };
    int c = consultar(HPWMI_ESCREVER, HPWMI_T_RADIOS, d, sizeof d, NULL, 0, NULL);
    klog("[HPWMI] %s %s: escribir %x -> codigo %d\n", ligar ? "encender" : "apagar", nomes_radio[radio], pedido, c);
    if (c != 0) {
        char t[96];
        snformatar(t, sizeof t, "la BIOS no cambió la radio (código %d)", c);
        nota(t);
        publicar();
        return ERRO_INVALIDO;
    }
    ler_radios();
    comparar(antes, false);
    registrar_radios("tras el cambio");
    if (ligar && radio == HPWMI_RADIO_WIFI && radios[radio].sw && !radios[radio].hw) {
        nota("Wi-Fi encendida por software, pero el botón inalámbrico la bloquea: púlsalo");
        acoes_aviso("La Wi-Fi sigue bloqueada por el botón inalámbrico: púlsalo para encenderla.");
    }
    publicar();
    return 0;
}

/* ------------------------------------------------------------ eventos */

static void tecla(void)
{
    uint8_t r[4] = { 0, 0, 0, 0 };
    uint32_t n = 0;
    int c = consultar(HPWMI_LER, HPWMI_T_TECLA, NULL, 0, r, sizeof r, &n);
    if (c != 0 || n < 2) {
        klog("[HPWMI] tecla del panel: la consulta da codigo %d (%u bytes)\n", c, n);
        return;
    }
    uint32_t cod = (uint32_t)r[0] | (uint32_t)r[1] << 8;
    if (n >= 4) cod |= (uint32_t)r[2] << 16 | (uint32_t)r[3] << 24;
    bool i0 = int_salva_desliga();
    pub.teclas++;
    pub.ultima_tecla = cod;
    int_restaura(i0);
    const hpwmi_tecla_t *t = hpwmi_tecla(cod);
    klog("[HPWMI] tecla del panel %x: %s\n", cod, t ? t->nome : "desconocida");
    if (!t) return;
    /* Las acciones comunes del sistema (acoes.c, D403): las mismas que las teclas del teclado, con su aviso. */
    if (t->acao == HPWMI_ACAO_VOL_MAIS) acao_sistema(ACAO_VOLUME_MAIS, ORIGEM_ACAO_HP);
    else if (t->acao == HPWMI_ACAO_VOL_MENOS) acao_sistema(ACAO_VOLUME_MENOS, ORIGEM_ACAO_HP);
    else if (t->acao == HPWMI_ACAO_SILENCIO) acao_sistema(ACAO_SILENCIO, ORIGEM_ACAO_HP);
    else if (t->acao == HPWMI_ACAO_AVISO) {
        char a[96];
        snformatar(a, sizeof a, "HP: %s.", t->nome);
        acoes_aviso(a);
    }
}

static void evento(uint8_t notificacao, const uint8_t *dados, uint32_t n)
{
    uint32_t ev = 0, dado = 0;
    if (!hpwmi_evento_ler(dados, n, &ev, &dado)) {
        klog("[HPWMI] evento (Notify %x) con %u bytes de datos: no se entiende\n", notificacao, n);
        return;
    }
    bool i0 = int_salva_desliga();
    pub.ultimo_evento = ev;
    pub.ultimo_dado = dado;
    int_restaura(i0);
    static const char *const nomes[] = { "?", "acople", "aparcar el disco", "cargador", "tecla del panel",
                                         "radios", "CPU y bateria", "bloqueo", "tapa" };
    klog("[HPWMI] evento %x (%s), dato %x\n", ev, ev < sizeof nomes / sizeof nomes[0] ? nomes[ev] : "?", dado);
    if (ev == HPWMI_EV_RADIOS) {
        hpwmi_radio_t antes[HPWMI_RADIOS];
        memcpy(antes, radios, sizeof antes);
        bool lido = botao_da_bios ? false : ler_radios();   /* releer volvería a poner la marca */
        if (lido) {
            comparar(antes, true);
            registrar_radios("evento");
        } else if (botao_da_bios) {
            /* La BIOS no cuenta las radios: no se sabe si se encendió o apagó; se mira el
             * PCI por si aparece la tarjeta. */
            acoes_aviso("Botón inalámbrico pulsado: si la tarjeta Wi-Fi aparece en el PCI, se avisará para reiniciar.");
            nota("botón inalámbrico pulsado; mirando el PCI");
            pci_programar();
        }
    } else if (ev == HPWMI_EV_TECLA) {
        tecla();
    }
    publicar();
}

/* La primera lectura de las radios le dice a la BIOS que el sistema atiende el WMI (la
 * marca WMIF en el dv7, WAOK en el G62). Desde ahí el botón inalámbrico solo enciende la
 * radio si el sistema la encendió «por software». Pero si la BIOS dice que no hay ninguna
 * radio (en el dv7: la tarjeta no estaba en el bus al arrancar y la BIOS lo apuntó así en
 * la CMOS), el sistema no puede encenderla por WMI y el botón quedaría inútil: se quita la
 * marca para que el botón vuelva a mandar como sin sistema (enciende la radio y la tarjeta
 * aparece). Solo se toca un Name entero del propio dispositivo WMI, nunca el hardware. */
static void devolver_botao(void)
{
    static const char *const marcas[] = { "WMIF", "WAOK" };
    for (unsigned i = 0; i < sizeof marcas / sizeof marcas[0]; i++)
        if (wmi_zerar_inteiro(HPWMI_GUID_BIOS, marcas[i])) {
            botao_da_bios = true;
            klog("[HPWMI] la BIOS no ve ninguna radio: %s = 0, el boton inalambrico vuelve a encenderla por su cuenta\n",
                 marcas[i]);
            nota("la BIOS no ve la tarjeta: pulsa el botón inalámbrico y reinicia si aparece");
            return;
        }
    klog("[HPWMI] la BIOS no ve ninguna radio (sin marca conocida que devolver)\n");
}

/* ------------------------------------------------------------ la tarea */

void hp_wmi_iniciar(void)
{
    n_pci_inicio = pci_listar(pci_inicio, PCI_MAX);
    if (!wmi_tem(HPWMI_GUID_BIOS)) {
        nota("no hay WMI de HP en este equipo");
        publicar();
        return;
    }
    hp_ok = true;
    klog("[HPWMI] WMI de HP encontrado\n");
    if (!wmi_assinar_evento(HPWMI_GUID_EVENTO, evento)) klog("[HPWMI] sin el GUID de eventos: no llegaran las teclas\n");
    if (ler_radios()) {
        registrar_radios("al arrancar");
        nota("radios leídas");
        if (!(estado[0] & 7)) devolver_botao();
    } else {
        nota("la BIOS no dio el estado de las radios");
    }
    publicar();
}

void hp_wmi_ciclo(void)
{
    if (pedido_pendente) {
        int32_t r = mudar(pedido_valor);
        bool i0 = int_salva_desliga();
        pedido_resultado = r;
        pedido_feito = true;
        pedido_pendente = false;
        int_restaura(i0);
    }
    if (pci_mirar_em && (int32_t)(relogio_tiques() - pci_mirar_em) >= 0) pci_mirar();
}

/* ------------------------------------------------------------ para los programas */

int32_t radios_info(uint32_t ptr, uint32_t n)
{
    if (n < sizeof(info_radios_t) || !usuario_ok((void *)ptr, sizeof(info_radios_t), true)) return ERRO_ENDERECO;
    info_bateria_t b;
    bateria_estado(&b);
    info_radios_t r;
    bool i0 = int_salva_desliga();
    r = pub;
    int_restaura(i0);
    r.versao = 1;
    r.uacpi = b.uacpi;
    if (b.uacpi != UACPI_ATIVO && !r.nota[0])
        txt_copia_n(r.nota, b.uacpi == UACPI_DESLIGADO ? "uACPI desactivado (imagen sin --uacpi)" : b.motivo, sizeof r.nota);
    memcpy((void *)ptr, &r, sizeof r);
    return 0;
}

int32_t radios_mudar(uint32_t valor)
{
    info_bateria_t b;
    bateria_estado(&b);
    if (b.uacpi != UACPI_ATIVO) return ERRO_NAO_EXISTE;
    if ((valor & 0xFF) >= HPWMI_RADIOS) return ERRO_INVALIDO;
    bool i0 = int_salva_desliga();
    if (pedido_pendente) { int_restaura(i0); return ERRO_OCUPADO; }
    pedido_valor = valor;
    pedido_feito = false;
    pedido_pendente = true;
    int_restaura(i0);
    uacpi_sagui_acordar();
    /* Una llamada al sistema corre con las interrupciones apagadas: se cede el turno
     * como en ata.c (bloquear + ceder con el reloj de red), no con uacpi_sagui_dormir_ms. */
    processo_t *eu = processo_atual();
    if (!eu || eu->pid == 0 || contexto_irq()) return ERRO_OCUPADO;   /* se hará; quien pidió mira INFO_RADIOS */
    uint32_t fim = relogio_tiques() + ESPERA_PEDIDO_MS * TIQUES_POR_SEGUNDO / 1000;
    while (!pedido_feito && (int32_t)(relogio_tiques() - fim) < 0) {
        bloquear(&canal_espera);
        eu->acordar_em = relogio_tiques() + 2;
        ceder();
        eu->acordar_em = 0;
    }
    return pedido_feito ? pedido_resultado : ERRO_OCUPADO;
}
