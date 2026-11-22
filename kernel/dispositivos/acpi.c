// SPDX-License-Identifier: GPL-2.0-or-later
/* acpi.c - apagado por ACPI (estado S5) y botón de encendido (D113, mejora 030).
 *
 * Sin intérprete AML. Lo que se usa del ACPI:
 *   - la FADT ("FACP"): puertos PM1a/PM1b (control y eventos), SMI_CMD y
 *     ACPI_ENABLE para pasar al modo ACPI, los bloques GPE y la IRQ del SCI;
 *   - el objeto \_S5_ de la DSDT (o de una SSDT): un paquete con los valores
 *     SLP_TYPa/SLP_TYPb. Se busca byte a byte la secuencia NameOp "_S5_"
 *     PackageOp y se leen los dos primeros elementos si son constantes
 *     (ZeroOp, OneOp, BytePrefix, WordPrefix, DWordPrefix).
 *
 * Límites (documentados en D113):
 *   - no se ejecutan _PTS ni _GTS (algunas placas los necesitan para apagar
 *     del todo; entonces, a los 3 s, el sistema dice «Ya puedes apagar»);
 *   - un \_S5_ que no sea un paquete de constantes (un Method, una
 *     referencia a otro nombre, o dentro de un If con otra variante antes)
 *     no se entiende: ENERGIA_SEM_S5;
 *   - sólo registros en el espacio de E/S (lo habitual en x86); el botón de
 *     encendido sólo si es el «fijo» (FADT PWR_BUTTON = 0). Uno descrito como
 *     dispositivo AML (PNP0C0C) llega por un GPE, que aquí no se atiende;
 *   - en el modo ACPI todos los GPE quedan desactivados (nadie ejecutaría sus
 *     métodos _Lxx/_Exx): la tapa, la batería o las teclas especiales que
 *     dependen de ellos dejan de avisar. El ventilador lo controla el EC.
 *
 * Seguridad: nada se escribe si la FADT no tiene la suma correcta, si la
 * DSDT/SSDT de donde sale el \_S5_ no la tiene o si los valores no caben en
 * 3 bits. La opción de arranque --sem-acpi (OPC_BOOT_SEM_ACPI) y el comando
 * «acpi desactivar» dejan todo como antes.
 *
 * D192: el intérprete de AML (uACPI, uacpi_sagui.c) es opcional (--uacpi) y llega
 * después; este archivo sigue siendo el dueño del modo ACPI, del apagado y del
 * botón. uACPI nunca cambia el modo (UACPI_FLAG_NO_ACPI_MODE) y solo arranca
 * si este archivo ya dejó el sistema en modo ACPI (acpi_modo_ativo). */
#include "idioma.h"
#include "acpi.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "info_hardware.h"
#include "portas.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "montagem.h"
#include "supervisor.h"
#include "memoria.h"
#include "paginacao.h"

/* PM1_STS / PM1_EN */
#define PM1_TMR          0x0001
#define PM1_PWRBTN       0x0100
#define PM1_WAK          0x8000
/* PM1_CNT */
#define PM1_SCI_EN       0x0001
#define PM1_SLP_TYP(x)   ((uint16_t)((x) << 10))
#define PM1_SLP_TYP_MASK 0x1C00
#define PM1_SLP_EN       0x2000
/* FADT Flags */
#define FADT_PWR_BUTTON  (1u << 4)
#define FADT_HW_REDUCED  (1u << 20)

static info_energia_t e;           /* lo que INFO_ENERGIA devuelve */
static bool tabelas_ok;            /* FADT y \_S5_ válidos (independiente de desactivado) */
static bool desativado;            /* opción de arranque o "acpi desactivar" */
static bool por_arranque;          /* desactivado por la opción de arranque: no se reactiva */
static uint16_t gpe_blk[2];
static uint8_t gpe_len[2];
static uint16_t sleep_ctl;          /* ACPI 5+ hardware reduzido: SLEEP_CONTROL_REG (E/S) */
static volatile int pedido;        /* 0 = nada, 1 = menú/shell, 2 = botón, 3 = reiniciar (D216) */
static volatile int metodo_pedido; /* D450: con qué método empieza el reinicio (ENERGIA_RESET_*) */
static bool tarefa_pronta;         /* D450: la tarea "apagar" existe (antes, el reinicio va directo) */

/* D450: el registro de reinicio de la FADT (RESET_REG, ACPI 2.0+): una GAS de 12 bytes en el
 * desplazamiento 116 y el valor que hay que escribir en el 128. Solo si el firmware dice que vale
 * (RESET_REG_SUP, bit 10 de las Flags). Espacio 0 = memoria, 1 = E/S, 2 = configuración PCI. */
#define FADT_RESET_REG_SUP (1u << 10)
static struct {
    bool     ok;
    uint8_t  espaco, valor;
    uint64_t endereco;
    volatile uint8_t *mem;   /* espacio de memoria: la ventana, mapeada en acpi_iniciar */
} reset;
static uint16_t iapc_boot;   /* IAPC_BOOT_ARCH (byte 109): bit 1 = hay un 8042 */
static const char canal_pedido = 0;
static bool sci_ligado;

/* ------------------------------------------------------------ tablas (antes de la paginación) */

static bool soma_ok(const uint8_t *p, uint32_t n)
{
    uint8_t s = 0;
    for (uint32_t i = 0; i < n; i++) s = (uint8_t)(s + p[i]);
    return s == 0;
}

/* Una tabla con firma y suma de verificación correctas (tamaño razonable). */
static const uint8_t *tabela_valida(uint32_t endereco, const char *assinatura)
{
    if (endereco < 0x1000) return NULL;
    const uint8_t *t = (const uint8_t *)endereco;
    uint32_t tam = *(const uint32_t *)(t + 4);
    if (memcmp(t, assinatura, 4) != 0 || tam < 36 || tam > (4u << 20)) return NULL;
    return soma_ok(t, tam) ? t : NULL;
}

static uint32_t le32(const uint8_t *p) { return *(const uint32_t *)p; }

/* Generic Address Structure: solo el espacio de E/S (1), por debajo de 64 KB. */
static uint32_t gas_porta(const uint8_t *g)
{
    if (g[0] != 1 || le32(g + 8) || le32(g + 4) > 0xFFFF) return 0;
    return le32(g + 4);
}

/* Un elemento constante de un paquete AML; devuelve cuántos bytes ocupa (0 = no lo es). */
static int aml_constante(const uint8_t *p, const uint8_t *fim, uint32_t *v)
{
    if (p >= fim) return 0;
    switch (p[0]) {
    case 0x00: *v = 0; return 1;                         /* ZeroOp */
    case 0x01: *v = 1; return 1;                         /* OneOp */
    case 0x0A: if (p + 2 > fim) return 0; *v = p[1]; return 2;              /* BytePrefix */
    case 0x0B: if (p + 3 > fim) return 0; *v = p[1] | (p[2] << 8); return 3;   /* WordPrefix */
    case 0x0C: if (p + 5 > fim) return 0; *v = le32(p + 1); return 5;      /* DWordPrefix */
    default: return 0;
    }
}

/* Busca Name(\_S5_, Package(){a, b, ...}) en el cuerpo AML de la tabla. */
static bool achar_s5(const uint8_t *t, uint8_t *a, uint8_t *b)
{
    uint32_t tam = le32(t + 4);
    const uint8_t *fim = t + tam;
    for (const uint8_t *p = t + 36; p + 4 <= fim; p++) {
        if (p[0] != '_' || p[1] != 'S' || p[2] != '5' || p[3] != '_') continue;
        /* NameOp justo antes del nombre, o NameOp + '\' (nombre desde la raíz). */
        bool nome = (p - t >= 37 && p[-1] == 0x08) || (p - t >= 38 && p[-1] == '\\' && p[-2] == 0x08);
        if (!nome || p + 5 > fim || p[4] != 0x12) continue;   /* PackageOp */
        const uint8_t *q = p + 5;
        if (q >= fim) continue;
        q += 1 + (q[0] >> 6);         /* PkgLength: los bits 7-6 dicen cuántos bytes siguen */
        if (q >= fim) continue;
        uint8_t elementos = *q++;
        uint32_t va, vb;
        int n = aml_constante(q, fim, &va);
        if (!n) continue;
        vb = va;
        if (elementos >= 2 && !aml_constante(q + n, fim, &vb)) continue;
        if (va > 7 || vb > 7) continue;
        *a = (uint8_t)va;
        *b = (uint8_t)vb;
        return true;
    }
    return false;
}

static void motivo(const char *m)
{
    txt_copia_n(e.motivo, m, sizeof e.motivo);
}

/* Llamado desde hw_capturar_boot, antes de la paginación: toda la memoria física
 * (por debajo de 4 GB) está a la vista. Solo lee; guarda los puertos y el \_S5_. */
void acpi_capturar(void)
{
    e.estado = ENERGIA_SEM_ACPI;
    motivo("sin tablas ACPI (RSDP o FADT)");
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou) return;
    const uint8_t *f = NULL;
    for (uint32_t i = 0; i < a->qtd && i < ACPI_MAX_TABELAS; i++)
        if (memcmp(a->tabela[i].assinatura, "FACP", 4) == 0) { f = (const uint8_t *)a->tabela[i].endereco; break; }
    if (!f) return;
    e.estado = ENERGIA_TABELAS_MAS;
    if (!tabela_valida((uint32_t)f, "FACP") || le32(f + 4) < 116) {
        motivo("FADT con suma de verificación errónea");
        klog("[ACPI] FADT invalida (soma ou tamanho): sem apagado ACPI\n");
        return;
    }
    uint32_t tam = le32(f + 4);
    e.fadt_revisao = f[8];
    e.sci = *(const uint16_t *)(f + 46);
    e.smi_cmd = le32(f + 48);
    e.acpi_enable = f[52];
    e.pm1a_evt = le32(f + 56);
    e.pm1b_evt = le32(f + 60);
    e.pm1a_cnt = le32(f + 64);
    e.pm1b_cnt = le32(f + 68);
    for (int k = 0; k < 2; k++) {   /* fuera del espacio de E/S: no se toca */
        uint32_t g = le32(f + 80 + 4 * k);
        gpe_blk[k] = g <= 0xFFFF ? (uint16_t)g : 0;
    }
    e.pm1_evt_len = f[88];
    gpe_len[0] = f[92];
    gpe_len[1] = f[93];
    uint32_t flags = le32(f + 112);
    e.botao_metodo = (flags & FADT_PWR_BUTTON) ? 1 : 0;
    e.reduzido = (flags & FADT_HW_REDUCED) ? 1 : 0;
    /* ACPI 2.0+: los X_ (GAS de 64 bits) valen cuando los de 32 bits son 0. */
    if (tam >= 196) {
        if (!e.pm1a_evt) e.pm1a_evt = gas_porta(f + 148);
        if (!e.pm1b_evt) e.pm1b_evt = gas_porta(f + 160);
        if (!e.pm1a_cnt) e.pm1a_cnt = gas_porta(f + 172);
        if (!e.pm1b_cnt) e.pm1b_cnt = gas_porta(f + 184);
    }
    if (e.reduzido && tam >= 256) sleep_ctl = (uint16_t)gas_porta(f + 244);
    /* D450: el registro de reinicio (ACPI 2.0+, FADT de 129 bytes o más). Se apunta aunque no
     * haya \_S5_: reiniciar no lo necesita. */
    if (f[8] >= 2 && tam >= 129) {
        iapc_boot = *(const uint16_t *)(f + 109);
        uint64_t end = (uint64_t)le32(f + 120) | ((uint64_t)le32(f + 124) << 32);
        reset.espaco = f[116];
        reset.endereco = end;
        reset.valor = f[128];
        reset.ok = (flags & FADT_RESET_REG_SUP) && end && reset.espaco <= 2;
        if (reset.espaco == 1 && end > 0xFFFF) reset.ok = false;
        if (reset.espaco == 0 && end > 0xFFFFF000ull) reset.ok = false;   /* por encima de 4 GB no se ve */
        klog("[ACPI] RESET_REG: espacio %u direccion %x:%x valor %x%s; IAPC_BOOT_ARCH %x (8042 %s)\n",
             reset.espaco, (uint32_t)(end >> 32), (uint32_t)end, reset.valor,
             reset.ok ? "" : (flags & FADT_RESET_REG_SUP) ? " (no utilizable)" : " (RESET_REG_SUP=0)",
             iapc_boot, (iapc_boot & 2) ? "declarado" : "no declarado");
    }
    if (e.pm1a_cnt > 0xFFFF || e.pm1b_cnt > 0xFFFF || e.pm1a_evt > 0xFFFF || e.pm1b_evt > 0xFFFF) {
        motivo("registros PM1 fuera del espacio de E/S");
        return;
    }
    if (!e.pm1a_cnt && !sleep_ctl) {
        motivo(e.reduzido ? "ACPI de hardware reducido sin SLEEP_CONTROL" : "la FADT no tiene PM1a_CNT");
        return;
    }

    /* DSDT: la dirección de 32 bits, o el X_DSDT (si cabe en 32 bits). */
    uint32_t dsdt = le32(f + 40);
    if (!dsdt && tam >= 148 && !le32(f + 144)) dsdt = le32(f + 140);
    const uint8_t *d = tabela_valida(dsdt, "DSDT");
    e.estado = ENERGIA_SEM_S5;
    motivo("no se ha encontrado un \\_S5_ legible");
    if (d && achar_s5(d, &e.slp_typ_a, &e.slp_typ_b)) {
        memcpy(e.origem_s5, "DSDT", 5);
    } else {
        for (uint32_t i = 0; i < a->qtd && i < ACPI_MAX_TABELAS && !e.origem_s5[0]; i++) {
            if (memcmp(a->tabela[i].assinatura, "SSDT", 4) != 0) continue;
            const uint8_t *s = tabela_valida(a->tabela[i].endereco, "SSDT");
            if (s && achar_s5(s, &e.slp_typ_a, &e.slp_typ_b)) memcpy(e.origem_s5, "SSDT", 5);
        }
        if (!d) motivo("DSDT ausente o con suma errónea");
    }
    if (!e.origem_s5[0]) {
        klog("[ACPI] sem \\_S5_ legivel (DSDT %s): sem apagado ACPI\n", d ? "ok" : "invalida");
        return;
    }
    tabelas_ok = true;
    e.estado = ENERGIA_DISPONIVEL;
    e.motivo[0] = 0;
    klog("[ACPI] FADT rev %u: PM1a_CNT %x PM1b_CNT %x PM1a_EVT %x/%u SCI %u SMI_CMD %x ACPI_ENABLE %x%s%s\n",
         e.fadt_revisao, e.pm1a_cnt, e.pm1b_cnt, e.pm1a_evt, e.pm1_evt_len, e.sci, e.smi_cmd, e.acpi_enable,
         e.botao_metodo ? " botao-AML" : "", e.reduzido ? " hw-reduzido" : "");
    klog("[ACPI] \\_S5_ na %s: SLP_TYPa=%u SLP_TYPb=%u\n", e.origem_s5, e.slp_typ_a, e.slp_typ_b);
}

/* ------------------------------------------------------------ SCI y botón */

static uint16_t pm1_sts(void)
{
    uint16_t s = porta_le16((uint16_t)e.pm1a_evt);
    if (e.pm1b_evt) s |= porta_le16((uint16_t)e.pm1b_evt);
    return s;
}

static void pm1_limpar(uint16_t bits)   /* los bits de estado se borran escribiendo 1 */
{
    porta_escreve16((uint16_t)e.pm1a_evt, bits);
    if (e.pm1b_evt) porta_escreve16((uint16_t)e.pm1b_evt, bits);
}

static void pm1_habilitar(uint16_t bits)
{
    uint16_t meio = (uint16_t)(e.pm1_evt_len / 2);
    porta_escreve16((uint16_t)(e.pm1a_evt + meio), bits);
    if (e.pm1b_evt) porta_escreve16((uint16_t)(e.pm1b_evt + meio), bits);
}

/* Desactiva todos los GPE y borra los estados (nadie ejecutaría los métodos). */
static void gpe_desligar(void)
{
    for (int k = 0; k < 2; k++) {
        if (!gpe_blk[k] || gpe_len[k] < 2) continue;
        uint16_t meio = (uint16_t)(gpe_len[k] / 2);
        for (uint16_t i = 0; i < meio; i++) porta_escreve8((uint16_t)(gpe_blk[k] + meio + i), 0);
        for (uint16_t i = 0; i < meio; i++) porta_escreve8((uint16_t)(gpe_blk[k] + i), 0xFF);
    }
}

static bool atender_sci(void)
{
    if (!sci_ligado) return false;
    uint16_t s = pm1_sts();
    if (!(s & PM1_PWRBTN)) return false;
    pm1_limpar(PM1_PWRBTN);
    acpi_botao_evento();
    return true;
}

/* El botón ya se ha visto (y su bit de estado borrado): lo cuenta y despierta
 * la tarea "apagar". Con interrupciones desactivadas (en la IRQ del SCI). La
 * llama atender_sci() y, con uACPI (D192), su manejador del evento fijo del
 * botón: los dos miran el mismo bit y solo el primero que lo ve lo atiende. */
void acpi_botao_evento(void)
{
    e.sci_total++;
    if (desativado) return;
    e.pulsacoes++;
    if (!pedido) {
        pedido = 2;
        acordar(&canal_pedido);
    }
}

/* D192: ¿está el sistema en modo ACPI (SCI_EN = 1) y sin la opción de
 * arranque --sem-acpi? Solo entonces puede arrancar uACPI: en el modo
 * heredado el firmware (SMM) es el dueño del controlador embebido, y el
 * sistema no debe hablarle a la vez. El hardware reducido no tiene modo
 * heredado: siempre está en ACPI. */
bool acpi_modo_ativo(void)
{
    if (por_arranque) return false;
    if (e.reduzido) return true;
    if (!e.pm1a_cnt || e.pm1a_cnt > 0xFFFF) return false;
    return (porta_le16((uint16_t)e.pm1a_cnt) & PM1_SCI_EN) != 0;
}

bool acpi_botao_ativo(void) { return e.botao != 0; }

/* D192: vuelve a poner el bit del botón en PM1_EN sin tocar los demás
 * (leer, OR, escribir). uacpi_initialize() apaga todos los eventos fijos; si
 * uACPI no llega a instalar su manejador del botón, esto lo deja como estaba. */
void acpi_botao_rearmar(void)
{
    if (!e.botao || !sci_ligado) return;
    bool i0 = int_salva_desliga();
    uint16_t meio = (uint16_t)(e.pm1_evt_len / 2);
    uint16_t v = porta_le16((uint16_t)(e.pm1a_evt + meio));
    porta_escreve16((uint16_t)(e.pm1a_evt + meio), v | PM1_PWRBTN);
    if (e.pm1b_evt) {
        v = porta_le16((uint16_t)(e.pm1b_evt + meio));
        porta_escreve16((uint16_t)(e.pm1b_evt + meio), v | PM1_PWRBTN);
    }
    int_restaura(i0);
}

/* El apagado corre en esta tarea del kernel (y no dentro de la llamada al sistema,
 * que corre con las interrupciones desactivadas): el aviso necesita el gestor
 * de ventanas y de la espera. Peticiones: 1 = menú o shell, 2 = botón, 3 =
 * reiniciar con servicios en marcha (D216). Antes de grabar los discos, el
 * gestor de servicios para los suyos (como mucho 20 s: supervisor.c). */
static void tarefa_apagar(void *arg)
{
    (void)arg;
    for (;;) {
        int_desliga();
        if (!pedido) {
            bloquear(&canal_pedido);
            ceder();
        }
        int_liga();
        if (!pedido) continue;
        if (pedido == 2) {
            klog("[ACPI] botao de energia apertado: apagado ordenado\n");
            if (gui_ativa()) gui_aviso(_("Botón de encendido: apagando el ordenador..."));
            else {
                console_cor(COR_AMARELO, COR_PRETO);
                kprintf("\nBotón de encendido: apagando el ordenador...\n");
                console_cor(COR_CINZA, COR_PRETO);
            }
            relogio_dormir_ms(800);   /* para que se vea el aviso */
        }
        /* D450: todo el orden (servicios, discos, ACPI, procesadores, drivers, reinicio o S5) en energia.c. */
        energia_executar(pedido == 3 ? ENERGIA_REINICIAR : ENERGIA_APAGAR, metodo_pedido);
    }
}

/* D216: reiniciar con servicios en marcha pasa por esta tarea, que los para
 * antes (sis_reiniciar, si supervisor_ativo()). */
void acpi_pedir_reinicio(int metodo)
{
    bool i0 = int_salva_desliga();
    if (!pedido) { pedido = 3; metodo_pedido = metodo; }
    acordar(&canal_pedido);
    int_restaura(i0);
}

void acpi_pedir_apagado(void)
{
    bool i0 = int_salva_desliga();
    if (!pedido) pedido = 1;
    acordar(&canal_pedido);
    int_restaura(i0);
}

/* Espera corta sin interrupciones (puede llamarse con IF = 0). */
static void esperar_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 1000; i++) porta_pausa();   /* ~1 us por acceso al puerto 0x80 */
}

/* Pasa al modo ACPI si el firmware todavía no lo hizo (SCI_EN = 0). */
static bool modo_acpi(void)
{
    if (!e.pm1a_cnt) return false;
    if (porta_le16((uint16_t)e.pm1a_cnt) & PM1_SCI_EN) { e.modo_acpi = 1; return true; }
    if (!e.smi_cmd || !e.acpi_enable || e.smi_cmd > 0xFFFF) {
        klog("[ACPI] SCI_EN=0 e sem SMI_CMD/ACPI_ENABLE: fica no modo legado\n");
        return false;
    }
    porta_escreve8((uint16_t)e.smi_cmd, (uint8_t)e.acpi_enable);
    for (int i = 0; i < 300; i++) {   /* até 3 s */
        if (porta_le16((uint16_t)e.pm1a_cnt) & PM1_SCI_EN) {
            e.modo_acpi = 2;
            klog("[ACPI] modo ACPI ligado (SMI_CMD %x <- %x) em %d ms\n", e.smi_cmd, e.acpi_enable, i * 10);
            return true;
        }
        esperar_ms(10);
    }
    klog("[ACPI] o firmware nao ligou SCI_EN em 3 s\n");
    return false;
}

void acpi_iniciar(void)
{
    tarefa_pronta = tarefa_kernel("apagar", tarefa_apagar, NULL) != NULL;
    klog("[ACPI] tarefa \"apagar\" do kernel pronta\n");
    /* D450: el RESET_REG en memoria se mapea ya (después, con las interrupciones cerradas para
     * siempre, no se pide memoria). Sin caché: es un registro. */
    if (reset.ok && reset.espaco == 0) {
        uint32_t fis = (uint32_t)reset.endereco;
        uint32_t v = janela_mapear_tipo(fis & ~0xFFFu, PAGINA, PG_PCD | PG_PWT);
        if (v) reset.mem = (volatile uint8_t *)(v + (fis & 0xFFFu));
        else { reset.ok = false; klog("[ACPI] RESET_REG en memoria: sin ventana, no se usara\n"); }
    }
    if (info_boot.opcoes & OPC_BOOT_SEM_ACPI) {
        desativado = por_arranque = true;
        klog("[ACPI] desativado pela opcao de arranque (--sem-acpi)\n");
        if (tabelas_ok) e.estado = ENERGIA_DESATIVADO;
        return;
    }
    if (!tabelas_ok) {
        klog("[ACPI] apagado ACPI indisponivel: %s\n", e.motivo);
        return;
    }
    /* El botón necesita el modo ACPI, de un PM1_EVT y de ser el botón "fijo". */
    if (e.reduzido || !e.pm1a_evt || e.pm1_evt_len < 4 || e.sci >= 16 || e.botao_metodo) {
        klog("[ACPI] botao de energia nao atendido (%s)\n",
             e.botao_metodo ? "e um dispositivo AML" : e.reduzido ? "hardware reduzido" : "sem PM1_EVT/SCI");
        return;
    }
    if (!modo_acpi()) return;
    bool i0 = int_salva_desliga();
    gpe_desligar();
    pm1_habilitar(0);
    pm1_limpar(0xFFFF);
    pm1_habilitar(PM1_PWRBTN);
    sci_ligado = true;
    irq_compartilhar((uint8_t)e.sci, atender_sci);
    int_restaura(i0);
    e.botao = 1;
    klog("[ACPI] botao de energia ligado (SCI na IRQ %u)\n", e.sci);
}

bool acpi_disponivel(void) { return tabelas_ok && !desativado; }

/* Escribe SLP_TYP | SLP_EN. Si la máquina todavía funciona 3 s después, devuelve false. */
bool acpi_apagar(void)
{
    if (!acpi_disponivel()) {
        klog("[ACPI] apagado ACPI indisponivel (%s)\n", desativado ? "desativado" : e.motivo);
        return false;
    }
    klog("[ACPI] S5: SLP_TYPa=%u SLP_TYPb=%u\n", e.slp_typ_a, e.slp_typ_b);
    bool i0 = int_salva_desliga();
    gpe_desligar();   /* D450: ningún GPE (uACPI pudo encender alguno) despierta la máquina recién apagada */
    if (e.pm1a_cnt) {
        if (e.pm1a_evt) {
            pm1_habilitar(0);
            pm1_limpar(PM1_WAK | PM1_PWRBTN);
        }
        uint16_t va = porta_le16((uint16_t)e.pm1a_cnt) & (uint16_t)~(PM1_SLP_TYP_MASK | PM1_SLP_EN);
        uint16_t vb = e.pm1b_cnt ? porta_le16((uint16_t)e.pm1b_cnt) & (uint16_t)~(PM1_SLP_TYP_MASK | PM1_SLP_EN) : 0;
        /* Primero el tipo, después SLP_EN (como pide la especificación). */
        porta_escreve16((uint16_t)e.pm1a_cnt, va | PM1_SLP_TYP(e.slp_typ_a));
        if (e.pm1b_cnt) porta_escreve16((uint16_t)e.pm1b_cnt, vb | PM1_SLP_TYP(e.slp_typ_b));
        porta_escreve16((uint16_t)e.pm1a_cnt, va | PM1_SLP_TYP(e.slp_typ_a) | PM1_SLP_EN);
        if (e.pm1b_cnt) porta_escreve16((uint16_t)e.pm1b_cnt, vb | PM1_SLP_TYP(e.slp_typ_b) | PM1_SLP_EN);
    } else if (sleep_ctl) {
        porta_escreve8(sleep_ctl, (uint8_t)((e.slp_typ_a << 2) | 0x20));
    }
    esperar_ms(3000);
    int_restaura(i0);
    klog("[ACPI] a maquina nao desligou em 3 s\n");
    if (e.botao && e.pm1a_evt) pm1_habilitar(PM1_PWRBTN);
    return false;
}

bool acpi_tarefa_pronta(void) { return tarefa_pronta; }

/* D450: ¿hay un RESET_REG que el firmware declara utilizable? (también con --sem-acpi: es solo
 * una escritura, la que hace Linux primero.) Texto para las marcas del reinicio. */
bool acpi_reset_disponivel(void) { return reset.ok && !por_arranque; }

void acpi_reset_descrever(char *b, uint32_t n)
{
    static const char *const nomes[] = { "memoria", "E/S", "PCI" };
    if (!reset.ok) { snformatar(b, n, "no hay (FADT sin RESET_REG utilizable)"); return; }
    snformatar(b, n, "%s %x:%x <- %x", nomes[reset.espaco], (uint32_t)(reset.endereco >> 32),
               (uint32_t)reset.endereco, reset.valor);
}

bool acpi_8042_declarado(void) { return (iapc_boot & 2) != 0; }

/* D450: escribe el valor de reinicio (8 bits, como Linux y NT, sea cual sea el ancho de la GAS).
 * Con las interrupciones cerradas; si la máquina sigue aquí, vuelve. */
void acpi_reset_escrever(void)
{
    if (!acpi_reset_disponivel()) return;
    if (reset.espaco == 1) {
        porta_escreve8((uint16_t)reset.endereco, reset.valor);
    } else if (reset.espaco == 0) {
        if (reset.mem) *reset.mem = reset.valor;
    } else {
        /* Configuración PCI del bus 0: dispositivo en los bits 32-47, función en 16-31 y el
         * registro en 0-15 (ACPI 6.x, 5.2.3.2). Byte a byte por el mecanismo 1. */
        uint32_t disp = (uint32_t)(reset.endereco >> 32) & 0x1F;
        uint32_t func = (uint32_t)(reset.endereco >> 16) & 0x07;
        uint32_t reg = (uint32_t)reset.endereco & 0xFF;
        porta_escreve32(0xCF8, 0x80000000u | (disp << 11) | (func << 8) | (reg & 0xFC));
        porta_escreve8((uint16_t)(0xCFC + (reg & 3)), reset.valor);
    }
}

int32_t acpi_controle(uint32_t op)
{
    if (op == ACPI_OP_DESATIVAR) {
        desativado = true;
        if (tabelas_ok) e.estado = ENERGIA_DESATIVADO;
        klog("[ACPI] desativado por comando\n");
        return 0;
    }
    if (op == ACPI_OP_ATIVAR) {
        if (!tabelas_ok || por_arranque) return ERRO_NAO_EXISTE;
        desativado = false;
        e.estado = ENERGIA_DISPONIVEL;
        klog("[ACPI] reativado por comando\n");
        return 0;
    }
    return ERRO_INVALIDO;
}

int32_t acpi_info(uint32_t ptr, uint32_t n)
{
    if (n < sizeof e || !usuario_ok((void *)ptr, sizeof e, true)) return ERRO_ENDERECO;
    memcpy((void *)ptr, &e, sizeof e);
    return 0;
}
