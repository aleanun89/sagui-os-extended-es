// SPDX-License-Identifier: GPL-2.0-or-later
/* info_hardware.c - lo que el infopc (D041) necesita y sólo el kernel alcanza.
 *
 * Tres cosas se guardan al inicio, antes de encender la paginación, cuando
 * el kernel todavía ve toda la memoria física:
 *   - el bloque de información del VBE (0x6000), que la etapa 2 dejó en la
 *     memoria baja y que después se convierte en área libre;
 *   - la tabla SMBIOS/DMI: el ancla "_SM_" o "_SM3_" queda en el segmento de la
 *     BIOS (0xF0000-0xFFFFF, alineada a 16 bytes) y apunta a la tabla,
 *     que en la PC real suele quedar cerca del tope de la RAM baja (área
 *     reservada que el kernel no mapea después);
 *   - la frecuencia del TSC, medida contra el PIT (el reloj de 100 Hz);
 *   - las tablas ACPI (D076): el RSDP (en la EBDA o en 0xE0000-0xFFFFF) y las
 *     firmas que la RSDT/XSDT lista, más la DSDT apuntada por la FADT. Sólo
 *     los encabezados: nada de AML.
 * Las llamadas INFO_E820..INFO_CPU_MHZ (abi.h, rango 20-29) copian eso al
 * programa, comprobando siempre el puntero con usuario_ok. Todo es sólo lectura:
 * ningún registro de hardware se altera. */
#include "info_hardware.h"
#include "eist_antiguo.h"
#include "acpi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "gui.h"
#include "pci.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define SMBIOS_MAX (16 * 1024)

static info_smbios_t smbios;
static uint8_t smbios_tabela[SMBIOS_MAX];
static info_video_t video;
static info_tsc_t tsc;
static info_acpi_t acpi;

/* ------------------------------------------------------------ VBE */

/* Los textos del VBE son "punteros largos" del modo real (segmento:desplazamiento). */
static void texto_vbe(char *dst, uint32_t max, uint32_t longe)
{
    const char *s = (const char *)(((longe >> 16) << 4) + (longe & 0xFFFF));
    uint32_t i = 0;
    if (longe == 0 || (uint32_t)s >= 0x100000) { dst[0] = 0; return; }
    for (; i < max - 1 && s[i]; i++)
        dst[i] = (s[i] >= 0x20 && s[i] < 0x7F) ? s[i] : '?';
    dst[i] = 0;
    while (i > 0 && dst[i - 1] == ' ') dst[--i] = 0;
}

static void capturar_vbe(void)
{
    const uint8_t *vi = (const uint8_t *)0x6000;   /* VBE_INFO de boot/bootinfo.inc */
    video.grafico = info_boot.vesa_ativo;
    video.modo = info_boot.vesa_modo;
    video.largura = info_boot.vesa_largura;
    video.altura = info_boot.vesa_altura;
    video.bpp = info_boot.vesa_bpp;
    video.pitch = info_boot.vesa_pitch;
    video.framebuffer = info_boot.vesa_fb;
    video.verm_tam = info_boot.verm_tam;   video.verm_pos = info_boot.verm_pos;
    video.verde_tam = info_boot.verde_tam; video.verde_pos = info_boot.verde_pos;
    video.azul_tam = info_boot.azul_tam;   video.azul_pos = info_boot.azul_pos;
    if (!info_boot.vesa_ok || memcmp(vi, "VESA", 4) != 0) return;
    video.vbe_versao = *(const uint16_t *)(vi + 4);
    video.memoria_kb = (uint32_t)*(const uint16_t *)(vi + 18) * 64;
    texto_vbe(video.oem, sizeof video.oem, *(const uint32_t *)(vi + 6));
    if (video.vbe_versao >= 0x0200) {
        texto_vbe(video.fabricante, sizeof video.fabricante, *(const uint32_t *)(vi + 22));
        texto_vbe(video.produto, sizeof video.produto, *(const uint32_t *)(vi + 26));
        texto_vbe(video.revisao, sizeof video.revisao, *(const uint32_t *)(vi + 30));
    }
}

uint32_t hw_video_memoria_kb(void) { return video.memoria_kb; }

/* Después de un cambio de modo (D059): el infopc muestra el modo actual. */
void hw_video_atualizar(uint32_t modo)
{
    video.modo = modo;
    video.largura = info_boot.vesa_largura;
    video.altura = info_boot.vesa_altura;
    video.bpp = info_boot.vesa_bpp;
    video.pitch = info_boot.vesa_pitch;
    video.verm_tam = info_boot.verm_tam;   video.verm_pos = info_boot.verm_pos;
    video.verde_tam = info_boot.verde_tam; video.verde_pos = info_boot.verde_pos;
    video.azul_tam = info_boot.azul_tam;   video.azul_pos = info_boot.azul_pos;
}

/* ------------------------------------------------------------ SMBIOS */

static bool soma_ok(const uint8_t *p, uint32_t n)
{
    uint8_t s = 0;
    for (uint32_t i = 0; i < n; i++) s = (uint8_t)(s + p[i]);
    return s == 0;
}

/* Copia la tabla y cuenta las estructuras que cupieron enteras en el buffer. */
static void copiar_tabela(uint32_t endereco, uint32_t tamanho)
{
    uint32_t n = tamanho < SMBIOS_MAX ? tamanho : SMBIOS_MAX;
    memcpy(smbios_tabela, (const void *)endereco, n);
    /* Corta en la última estructura completa (encabezado + textos + dos ceros). */
    uint32_t pos = 0, bom = 0;
    while (pos + 4 <= n) {
        uint32_t f = pos + smbios_tabela[pos + 1];
        if (smbios_tabela[pos + 1] < 4) break;
        while (f + 1 < n && !(smbios_tabela[f] == 0 && smbios_tabela[f + 1] == 0)) f++;
        if (f + 1 >= n) break;
        uint8_t tipo = smbios_tabela[pos];
        pos = f + 2;
        bom = pos;
        if (tipo == 127) break;   /* fin de la tabla */
    }
    smbios.achou = 1;
    smbios.endereco = endereco;
    smbios.tamanho = tamanho;
    smbios.copiado = bom;
}

static void capturar_smbios(void)
{
    const uint8_t *achou2 = NULL, *achou3 = NULL;
    for (uint32_t a = 0xF0000; a < 0x100000; a += 16) {
        const uint8_t *p = (const uint8_t *)a;
        if (!achou3 && memcmp(p, "_SM3_", 5) == 0 && p[6] >= 0x18 && p[6] <= 0x20 && soma_ok(p, p[6]))
            achou3 = p;
        else if (!achou2 && memcmp(p, "_SM_", 4) == 0 && p[5] >= 0x1F && p[5] <= 0x20 && soma_ok(p, p[5])
                 && memcmp(p + 0x10, "_DMI_", 5) == 0 && soma_ok(p + 0x10, 15))
            achou2 = p;
    }
    if (achou3) {
        uint64_t end = *(const uint64_t *)(achou3 + 0x10);
        uint32_t max = *(const uint32_t *)(achou3 + 0x0C);
        if (end < 0x100000000ull && end != 0 && max != 0) {
            smbios.versao_maior = achou3[7];
            smbios.versao_menor = achou3[8];
            smbios.entrada_64 = 1;
            copiar_tabela((uint32_t)end, max);
            return;
        }
    }
    if (achou2) {
        uint32_t end = *(const uint32_t *)(achou2 + 0x18);
        uint16_t tam = *(const uint16_t *)(achou2 + 0x16);
        if (end == 0 || tam == 0) return;
        smbios.versao_maior = achou2[6];
        smbios.versao_menor = achou2[7];
        smbios.estruturas = *(const uint16_t *)(achou2 + 0x1C);
        copiar_tabela(end, tam);
    }
}

/* ------------------------------------------------------------ ACPI */

static const uint8_t *achar_rsdp(uint32_t ini, uint32_t fim)
{
    for (uint32_t a = ini; a + 20 <= fim; a += 16)
        if (memcmp((const void *)a, "RSD PTR ", 8) == 0 && soma_ok((const uint8_t *)a, 20))
            return (const uint8_t *)a;
    return NULL;
}

static void acpi_tabela(uint32_t endereco)
{
    if (acpi.qtd >= ACPI_MAX_TABELAS || endereco < 0x1000) return;
    const uint8_t *t = (const uint8_t *)endereco;
    for (int i = 0; i < 4; i++)
        if (t[i] < 0x20 || t[i] > 0x7E) return;   /* no parece un encabezado de tabla */
    info_acpi_tabela_t *e = &acpi.tabela[acpi.qtd++];
    memcpy(e->assinatura, t, 4);
    e->assinatura[4] = 0;
    for (int i = 0; i < 6; i++) e->oem[i] = (t[10 + i] >= 0x20 && t[10 + i] < 0x7F) ? (char)t[10 + i] : ' ';
    e->oem[6] = 0;
    e->endereco = endereco;
    e->tamanho = *(const uint32_t *)(t + 4);
    e->revisao = t[8];
    e->soma_ok = e->tamanho >= 36 && e->tamanho < (1u << 20) && soma_ok(t, e->tamanho);
}

/* Antes de la paginación: toda la memoria física (hasta 4 GB) está a la vista. Tablas
 * por encima de 4 GB (XSDT con dirección de 64 bits alta) quedan fuera. */
static void capturar_acpi(void)
{
    /* El segmento de la EBDA queda en el área de datos de la BIOS (0x40E). El asm vacío
     * oculta al GCC que la dirección es baja (él la tomaría por NULL + 0x40E). */
    const uint16_t *bda;
    __asm__("" : "=r"(bda) : "0"(0x40E));
    uint32_t ebda = (uint32_t)*bda << 4;
    const uint8_t *r = (ebda >= 0x80000 && ebda < 0xA0000) ? achar_rsdp(ebda, ebda + 1024) : NULL;
    if (!r) r = achar_rsdp(0xE0000, 0x100000);
    if (!r) return;
    acpi.achou = 1;
    acpi.rsdp = (uint32_t)r;
    acpi.revisao = r[15];
    memcpy(acpi.oem, r + 9, 6);
    acpi.oem[6] = 0;
    acpi.rsdt = *(const uint32_t *)(r + 16);
    if (acpi.revisao >= 2) {
        acpi.xsdt_baixo = *(const uint32_t *)(r + 24);
        acpi.xsdt_alto = *(const uint32_t *)(r + 28);
    }
    uint32_t raiz = acpi.rsdt, passo = 4;
    if (acpi.xsdt_baixo && !acpi.xsdt_alto) { raiz = acpi.xsdt_baixo; passo = 8; acpi.usou_xsdt = 1; }
    if (raiz < 0x1000) return;
    acpi_tabela(raiz);
    uint32_t tam = acpi.qtd ? acpi.tabela[0].tamanho : 0;
    if (tam < 36 || tam > 4096) return;
    for (uint32_t o = 36; o + passo <= tam; o += passo) {
        const uint8_t *q = (const uint8_t *)raiz + o;
        if (passo == 8 && *(const uint32_t *)(q + 4)) continue;   /* por encima de 4 GB */
        uint32_t end = *(const uint32_t *)q;
        uint32_t antes = acpi.qtd;
        acpi_tabela(end);
        /* La DSDT no está en la lista: la que apunta a ella es la FADT ("FACP"). */
        if (acpi.qtd > antes && memcmp(acpi.tabela[antes].assinatura, "FACP", 4) == 0 && acpi.tabela[antes].tamanho >= 44)
            acpi_tabela(*(const uint32_t *)((const uint8_t *)end + 40));
    }
}

const info_acpi_t *hw_acpi(void) { return &acpi; }

void hw_capturar_boot(void)
{
    capturar_vbe();
    capturar_smbios();
    capturar_acpi();
    acpi_capturar();   /* D113: FADT y \_S5_, mientras toda la memoria física está a la vista */
    eist_pss_capturar();   /* D177: _PSS/_PSD/_PCT de la DSDT y las SSDT (también las cargadas con Load), solo lectura */
    if (acpi.achou)
        klog("[INFO] ACPI: RSDP em %p, revisao %u, OEM \"%s\", %u tabelas\n", (void *)acpi.rsdp, acpi.revisao,
             acpi.oem, acpi.qtd);
    else
        klog("[INFO] ACPI: RSDP nao encontrado\n");
    if (smbios.achou)
        klog("[INFO] SMBIOS %u.%u em %p: %u bytes (%u copiados)\n", smbios.versao_maior, smbios.versao_menor,
             (void *)smbios.endereco, smbios.tamanho, smbios.copiado);
    else
        klog("[INFO] SMBIOS nao encontrado\n");
}

/* ------------------------------------------------------------ TSC */

static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* División de 64 por 32 bits sin la libgcc (el kernel no la enlaza). */
static uint64_t div64(uint64_t n, uint32_t d)
{
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

/* Cuenta los ciclos del TSC entre dos flancos del reloj de 100 Hz, separados
 * por TSC_TIQUES tics. El "hlt" despierta justo después de la IRQ 0, así que el error de
 * cada flanco es la latencia de la interrupción (microsegundos), pequeño frente a la
 * ventana de 100 ms. El período real del tic es DIVISOR/1193182 s (el divisor
 * entero 11932 da 99,998 Hz, no 100 Hz exactos), y eso se tiene en cuenta. */
#define TSC_TIQUES 10
#define PIT_HZ     1193182u
#define PIT_DIV    ((PIT_HZ + TIQUES_POR_SEGUNDO / 2) / TIQUES_POR_SEGUNDO)


/* D201: marcas de tiempo del arranque. El TSC cuenta desde el encendido, así
 * que la primera marca (la entrada al kernel) dice cuánto tardaron la BIOS y la
 * lectura de la imagen. Antes de medir el TSC se guarda el valor y se escribe en
 * el log después, al medirlo. */
static struct { uint64_t tsc; char nome[16]; } marcas[ARRANQUE_MAX_FASES];
static uint32_t qtd_marcas, marcas_escritas;

static uint32_t marca_ms(uint32_t i) { return tsc.khz ? (uint32_t)div64(marcas[i].tsc, tsc.khz) : 0; }

static void marcas_escrever(void)
{
    for (; marcas_escritas < qtd_marcas; marcas_escritas++) {
        uint32_t i = marcas_escritas, ms = marca_ms(i);
        klog("[ARRANQUE] %s: %u ms desde el encendido (+%u ms)\n", marcas[i].nome, ms,
             i ? ms - marca_ms(i - 1) : ms);
    }
}

void arranque_marca(const char *fase)
{
    if (qtd_marcas >= ARRANQUE_MAX_FASES) return;
    marcas[qtd_marcas].tsc = ler_tsc();
    int k = 0;
    for (; fase[k] && k < 15; k++) marcas[qtd_marcas].nome[k] = fase[k];
    marcas[qtd_marcas].nome[k] = 0;
    qtd_marcas++;
    if (tsc.khz) marcas_escrever();
}

uint32_t hw_tsc_khz(void) { return tsc.khz; }

void hw_medir_tsc(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1) return;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (!(d & (1u << 4))) return;   /* sin TSC */
    uint32_t t = relogio_tiques();
    while (relogio_tiques() == t) cpu_espera();
    uint64_t ini = ler_tsc();
    t = relogio_tiques();
    while (relogio_tiques() - t < TSC_TIQUES) cpu_espera();
    uint64_t fim = ler_tsc();
    tsc.ciclos = fim - ini;
    tsc.janela_ms = TSC_TIQUES * 1000 / TIQUES_POR_SEGUNDO;
    /* Hz = ciclos * PIT_HZ / (PIT_DIV * TSC_TIQUES); em kHz, divide por 1000. */
    tsc.khz = (uint32_t)div64(div64(tsc.ciclos * PIT_HZ, PIT_DIV * TSC_TIQUES), 1000);
    klog("[INFO] TSC: %u kHz (%u ms)\n", tsc.khz, tsc.janela_ms);
    marcas_escrever();   /* D201: las marcas de antes de medir */
}

/* ------------------------------------------------------------ llamadas */

int32_t hw_info(uint32_t tipo, uint32_t ptr, uint32_t n)
{
    if (tipo == INFO_E820) {
        info_e820_t *v = (info_e820_t *)ptr;
        if (n > info_boot.mapa_qtd) n = info_boot.mapa_qtd;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        for (uint32_t i = 0; i < n; i++) {
            v[i].base = mapa_memoria[i].base;
            v[i].tamanho = mapa_memoria[i].tamanho;
            v[i].tipo = mapa_memoria[i].tipo;
            v[i].extra = mapa_memoria[i].extra;
        }
        return (int32_t)n;
    }
    if (tipo == INFO_SMBIOS) {
        if (n < sizeof smbios || !usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
        uint32_t k = n - sizeof smbios;
        if (k > smbios.copiado) k = smbios.copiado;
        info_smbios_t cab = smbios;
        cab.copiado = k;
        memcpy((void *)ptr, &cab, sizeof cab);
        memcpy((uint8_t *)ptr + sizeof cab, smbios_tabela, k);
        return (int32_t)(sizeof cab + k);
    }
    if (tipo == INFO_ACPI) {
        if (n < sizeof acpi || !usuario_ok((void *)ptr, sizeof acpi, true)) return ERRO_ENDERECO;
        memcpy((void *)ptr, &acpi, sizeof acpi);
        return 0;
    }
    if (tipo == INFO_LOG) {
        if (!n || !usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
        return (int32_t)console_log_copiar((char *)ptr, n);
    }
    if (tipo == INFO_ARRANQUE) {
        info_arranque_t a;
        memset(&a, 0, sizeof a);
        a.disco = info_boot.disco;
        a.cd = (info_boot.opcoes & OPC_BOOT_CD) ? 1u : 0u;
        a.leitura_chs = info_boot.leitura_chs;
        a.bpp_pref = info_boot.bpp_pref;
        a.res_pref = info_boot.res_pref;
        a.opcoes = info_boot.opcoes;
        a.fases = qtd_marcas;
        for (uint32_t i = 0; i < qtd_marcas; i++) {
            a.fase_ms[i] = marca_ms(i);
            memcpy(a.fase_nome[i], marcas[i].nome, sizeof a.fase_nome[i]);
        }
        if (n < sizeof a || !usuario_ok((void *)ptr, sizeof a, true)) return ERRO_ENDERECO;
        memcpy((void *)ptr, &a, sizeof a);
        return 0;
    }
    if (tipo == INFO_PCI) {
        static pci_disp_t lista[64];
        info_pci_t *v = (info_pci_t *)ptr;
        if (n > 64) n = 64;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        int q = pci_listar(lista, (int)n);
        for (int i = 0; i < q; i++) {
            pci_disp_t *p = &lista[i];
            uint32_t r08 = pci_le32(p, 0x08), r0c = pci_le32(p, 0x0C), sub = 0, r3c = 0;
            if (((r0c >> 16) & 0x7F) == 0) {   /* encabezado tipo 0: tiene subsistema e IRQ */
                sub = pci_le32(p, 0x2C);
                r3c = pci_le32(p, 0x3C);
            }
            v[i] = (info_pci_t){ p->barramento, p->dispositivo, p->funcao, (uint8_t)r08,
                                 p->classe, p->subclasse, p->interface, (uint8_t)r3c,
                                 p->fabricante, p->modelo, (uint16_t)sub, (uint16_t)(sub >> 16) };
        }
        return q;
    }
    if (tipo == INFO_VIDEO) {
        info_video_t *v = (info_video_t *)ptr;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        *v = video;
        v->escritorio = gui_ativa();
        return 0;
    }
    if (tipo == INFO_CPU_MHZ) {
        info_tsc_t *v = (info_tsc_t *)ptr;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        *v = tsc;
        return (int32_t)tsc.khz;
    }
    return ERRO_INVALIDO;
}
