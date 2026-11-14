// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores.c - monitor de hardware: temperatura y frecuencia de la CPU (D092,
 * mejora 050).
 *
 * Reglas (petición del usuario, D092):
 *   - solo lectura: ningún wrmsr, ninguna escritura en registro PCI, SMBus o
 *     Super I/O. La única escritura es el índice SMN de los Ryzen (00:00.0, registro
 *     0x60): el valor anterior se guarda y se restaura siempre, con las
 *     interrupciones desactivadas durante el acceso;
 *   - antes de leer, el CPUID tiene que confirmar fabricante, familia, modelo y el
 *     bit del recurso; y todo rdmsr pasa por msr_ler_seguro(), que absorbe un
 *     #GP y marca el MSR para no volver a intentarlo (kernel/arch/msr.c);
 *   - solo se lee cuando alguien lo pide (INFO_SENSORES), como máximo una vez por
 *     segundo; quien pregunte a mitad recibe la misma muestra;
 *   - la placa (Super I/O, SMBus/SPD) no se lee: queda pendiente y, cuando
 *     exista, se desactivará por defecto.
 * Un solo núcleo (el BSP) mientras no haya SMP (017). Las fórmulas están en
 * kernel/lib/sensores_conv.c, probadas en el anfitrión. */
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "info_hardware.h"
#include "msr.h"
#include "pci.h"
#include "portas.h"
#include "relogio.h"
#include "sensores_conv.h"
#include "sistema.h"
#include "texto.h"

/* MSRs (Intel SDM vol. 4; AMD BKDG/PPR). */
#define MSR_PLATFORM_ID      0x17
#define MSR_MICROCODIGO      0x8B
#define MSR_PLATFORM_INFO    0xCE
#define MSR_MPERF            0xE7
#define MSR_APERF            0xE8
#define MSR_EXT_CONFIG       0xEE    /* Core 2: bit 30 = TjMax de 85 °C */
#define MSR_PERF_STATUS      0x198
#define MSR_THERM_STATUS     0x19C
#define MSR_MISC_ENABLE      0x1A0
#define MSR_TEMP_TARGET      0x1A2
#define MSR_TURBO_LIMITE     0x1AD
#define MSR_PKG_THERM_STATUS 0x1B1
#define MSR_K8_FIDVID        0xC0010042u
#define MSR_AMD_HWCR         0xC0010015u
#define MSR_AMD_PSTATE_LIM   0xC0010061u
#define MSR_AMD_PSTATE_ST    0xC0010063u
#define MSR_AMD_PSTATE0      0xC0010064u
/* Un MSR que no existe en ninguna CPU (franja reservada de Intel y fuera de las
 * de los hipervisores): INFO_MSR_PROVA lo lee a propósito, junto con el 0x802. */
#define MSR_INEXISTENTE      0x0BADF00Du

typedef struct {
    bool     pronto;
    bool     intel, amd, hipervisor, tem_msr, tem_tsc;
    uint32_t fam, mod, step;
    uint32_t c6a, c6c;          /* CPUID 06H: EAX (sensores) e ECX (APERF/MPERF) */
    uint32_t x7d;               /* CPUID 80000007H: EDX */
    uint32_t c16[3];            /* CPUID 16H */
    char     marca[49];
} cpu_t;

static cpu_t cpu;
static info_sensores_t amostra;
static uint32_t tique_amostra;
static bool tem_amostra;
static uint64_t aperf_ant, mperf_ant;
static uint32_t tique_aperf;
static bool tem_aperf;
static bool registrado;

static inline void cpuid(uint32_t f, uint32_t s, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(s));
}

static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void detectar(void)
{
    uint32_t r[4], max, maxx;
    cpu.pronto = true;
    cpuid(0, 0, r);
    max = r[0];
    cpu.intel = r[1] == 0x756E6547 && r[3] == 0x49656E69 && r[2] == 0x6C65746E;   /* GenuineIntel */
    cpu.amd = r[1] == 0x68747541 && r[3] == 0x69746E65 && r[2] == 0x444D4163;     /* AuthenticAMD */
    if (max < 1) return;
    cpuid(1, 0, r);
    uint32_t bf = (r[0] >> 8) & 0xF, bm = (r[0] >> 4) & 0xF;
    cpu.fam = bf == 0xF ? bf + ((r[0] >> 20) & 0xFF) : bf;
    cpu.mod = (bf == 6 || bf == 0xF) ? (((r[0] >> 16) & 0xF) << 4) + bm : bm;
    cpu.step = r[0] & 0xF;
    cpu.tem_tsc = (r[3] >> 4) & 1;
    cpu.tem_msr = (r[3] >> 5) & 1;
    cpu.hipervisor = (r[2] >> 31) & 1;
    if (max >= 6) { cpuid(6, 0, r); cpu.c6a = r[0]; cpu.c6c = r[2]; }
    if (max >= 0x16) { cpuid(0x16, 0, r); cpu.c16[0] = r[0] & 0xFFFF; cpu.c16[1] = r[1] & 0xFFFF; cpu.c16[2] = r[2] & 0xFFFF; }
    cpuid(0x80000000u, 0, r);
    maxx = (r[0] >= 0x80000000u && r[0] < 0x8000FFFFu) ? r[0] : 0;
    if (maxx >= 0x80000007u) { cpuid(0x80000007u, 0, r); cpu.x7d = r[3]; }
    if (maxx >= 0x80000004u) {
        for (uint32_t i = 0; i < 3; i++) {
            cpuid(0x80000002u + i, 0, r);
            memcpy(cpu.marca + 16 * i, r, 16);
        }
        cpu.marca[48] = 0;
    }
}

/* Solo lee MSR si la CPU dice que tiene MSR (CPUID.01H:EDX[5]). */
static bool msr(uint32_t n, uint64_t *v)
{
    *v = 0;
    if (!cpu.tem_msr) return false;
    return msr_ler_seguro(n, v);
}

/* ------------------------------------------------------------ PCI e SMN */

static bool pci_dispositivo(uint8_t d, uint8_t f, uint16_t fabricante, pci_disp_t *p)
{
    *p = (pci_disp_t){ 0, d, f, 0, 0, 0, 0, 0 };
    uint32_t id = pci_le32(p, 0);
    if ((id & 0xFFFF) != fabricante) return false;
    p->fabricante = (uint16_t)id;
    p->modelo = (uint16_t)(id >> 16);
    return true;
}

/* El SMN de los Ryzen se lee por el par índice/dato 0x60/0x64 de la raíz (00:00.0).
 * La escritura del índice es la única del monitor: guarda el índice anterior y el
 * restaura, con las interrupciones desactivadas para que nadie se meta en medio
 * (ni en el 0xCF8 del mecanismo de configuración). */
static bool smn_ler(uint32_t endereco, uint32_t *v)
{
    pci_disp_t raiz;
    if (!pci_dispositivo(0, 0, 0x1022, &raiz)) return false;
    bool estavam = int_salva_desliga();
    uint32_t antes = pci_le32(&raiz, 0x60);
    pci_escreve32(&raiz, 0x60, endereco);
    *v = pci_le32(&raiz, 0x64);
    pci_escreve32(&raiz, 0x60, antes);
    int_restaura(estavam);
    return true;
}

/* ------------------------------------------------------------ temperatura */

static void por_temp(info_sensores_t *s, const char *nome, int mc)
{
    if (s->qtd_temp >= SENS_MAX_TEMP) return;
    txt_copia_n(s->temp[s->qtd_temp].nome, nome, sizeof s->temp[0].nome);
    s->temp[s->qtd_temp].mili_c = mc;
    s->qtd_temp++;
}

static void temp_intel(info_sensores_t *s)
{
    if (!(cpu.c6a & 1)) {   /* CPUID.06H:EAX[0] = sensor digital (DTS) */
        txt_copia_n(s->motivo, "la CPU no anuncia sensor digital (CPUID 06H)", sizeof s->motivo);
        return;
    }
    uint64_t v;
    int tj = 0;
    if (cpu.fam == 6 && !sconv_intel_core2(cpu.mod) && cpu.mod >= 0x1A && msr(MSR_TEMP_TARGET, &v))
        tj = sconv_intel_tjmax_msr(v);
    if (tj) {
        txt_copia_n(s->max_origem, "MSR 0x1A2", sizeof s->max_origem);
    } else {
        bool movel = false, ee85 = false;
        if (sconv_intel_core2(cpu.mod) && cpu.mod != 0x0E && !(cpu.mod == 0x0F && cpu.step < 4)) {
            movel = msr(MSR_PLATFORM_ID, &v) && ((v >> 28) & 1);
            if (!movel) ee85 = msr(MSR_EXT_CONFIG, &v) && ((v >> 30) & 1);
        }
        tj = sconv_intel_tjmax_tabela(cpu.mod, cpu.step, movel, ee85);
        txt_copia_n(s->max_origem, tj ? "tabla por modelo (aprox.)" : "no disponible", sizeof s->max_origem);
    }
    if (!tj) {
        /* Sin MSR 0x1A2 ni tabla por modelo (compilación sin GPL, D109). */
        txt_copia_n(s->motivo, "TjMax desconocido (sin MSR ni tablas por modelo: compilación sin GPL)",
                    sizeof s->motivo);
        return;
    }
    s->max_mili_c = tj;
    int mc;
    if (msr(MSR_THERM_STATUS, &v) && sconv_intel_temp(v, tj, &mc)) {
        por_temp(s, "Núcleo 0", mc);
        s->throttling |= sconv_intel_throttling(v, (cpu.c6a >> 4) & 1);
    }
    if (((cpu.c6a >> 6) & 1) && msr(MSR_PKG_THERM_STATUS, &v) && sconv_intel_temp(v, tj, &mc)) {
        por_temp(s, "Paquete", mc);
        s->throttling |= sconv_intel_throttling(v, (cpu.c6a >> 4) & 1);
    }
    if (s->qtd_temp) {
        s->fonte = SENS_FONTE_INTEL;
        txt_copia_n(s->fonte_txt, "MSR IA32_THERM_STATUS (0x19C)", sizeof s->fonte_txt);
    } else {
        txt_copia_n(s->motivo, "el MSR 0x19C no responde o no trae una lectura válida", sizeof s->motivo);
    }
}

static void temp_amd(info_sensores_t *s)
{
    pci_disp_t f3;
    uint32_t r;
    if (cpu.fam == 0x17 || cpu.fam == 0x19 || (cpu.fam == 0x15 && cpu.mod >= 0x60 && cpu.mod < 0x80)) {
        uint32_t end = cpu.fam == 0x15 ? 0xD8200CA4u : 0x00059800u;
        if (!smn_ler(end, &r) || r == 0xFFFFFFFFu) {
            txt_copia_n(s->motivo, "no hay raíz PCI de AMD en 00:00.0 (¿máquina virtual?)", sizeof s->motivo);
            return;
        }
        int t = cpu.fam == 0x15 ? sconv_amd_k10(r) : sconv_amd_zen(r);
        por_temp(s, "Tctl", t);
        int d = cpu.fam == 0x15 ? 0 : sconv_amd_zen_desvio(cpu.marca);
        if (d) por_temp(s, "Tdie", t - d);
        s->fonte = SENS_FONTE_AMD_ZEN;
        snformatar(s->fonte_txt, sizeof s->fonte_txt, "SMN 0x%x (00:00.0, índice 0x60)", end);
        s->max_mili_c = cpu.fam == 0x15 ? 0 : 95000;
        txt_copia_n(s->max_origem, cpu.fam == 0x15 ? "" : "Tctl máx. típico", sizeof s->max_origem);
        return;
    }
    if (cpu.fam < 0x0F || cpu.fam > 0x16 || !pci_dispositivo(0x18, 3, 0x1022, &f3)) {
        txt_copia_n(s->motivo, cpu.fam >= 0x0F && cpu.fam <= 0x16 ? "no hay función PCI 00:18.3 de AMD (¿máquina virtual?)"
                                                                  : "familia de AMD sin sensor conocido",
                    sizeof s->motivo);
        return;
    }
    if (cpu.fam == 0x0F) {
        r = pci_le32(&f3, 0xE4);
        /* El k8temp elige el núcleo y el sensor escribiendo en este registro;
         * aquí solo se lee lo que esté seleccionado (D092: nada de escritura). */
        por_temp(s, "CPU", sconv_amd_k8(r));
        s->fonte = SENS_FONTE_AMD_K8;
        txt_copia_n(s->fonte_txt, "PCI 00:18.3, registro 0xE4 (K8)", sizeof s->fonte_txt);
        return;
    }
    r = pci_le32(&f3, 0xA4);
    por_temp(s, "Tctl", sconv_amd_k10(r));
    s->fonte = SENS_FONTE_AMD_K10;
    txt_copia_n(s->fonte_txt, "PCI 00:18.3, registro 0xA4", sizeof s->fonte_txt);
    uint32_t htc = pci_le32(&f3, 0x64);
    int lim = sconv_amd_htc_limite(htc);
    if (lim) {
        s->max_mili_c = lim;
        txt_copia_n(s->max_origem, "límite HTC (F3x64)", sizeof s->max_origem);
        s->throttling = SENS_THR_SABE | ((htc >> 4) & 1 ? SENS_THR_TERMICO : 0) | ((htc >> 5) & 1 ? SENS_THR_REGISTRO : 0);
    }
}

/* ------------------------------------------------------------ frecuencia */

static void freq_real(info_sensores_t *s)
{
    if (!(cpu.c6c & 1) || !s->tsc_khz) return;   /* CPUID.06H:ECX[0] = APERF/MPERF */
    uint64_t a, m;
    if (!msr(MSR_APERF, &a) || !msr(MSR_MPERF, &m)) return;
    uint32_t agora = relogio_tiques(), dt = agora - tique_aperf;
    if (!tem_aperf || dt < TIQUES_POR_SEGUNDO / 10 || dt > 10 * TIQUES_POR_SEGUNDO) {
        /* Sin una muestra anterior razonable: un intervalo corto (10 ms
         * contados por el TSC, que no depende de las interrupciones). */
        uint64_t ini = ler_tsc(), fim = ini + (uint64_t)s->tsc_khz * 10;
        while (ler_tsc() < fim) __asm__ volatile("pause");
        aperf_ant = a; mperf_ant = m;
        if (!msr(MSR_APERF, &a) || !msr(MSR_MPERF, &m)) return;
        s->real_janela_ms = 10;
    } else {
        s->real_janela_ms = dt * (1000 / TIQUES_POR_SEGUNDO);
    }
    s->real_khz = sconv_freq_real_khz(s->tsc_khz, a - aperf_ant, m - mperf_ant);
    aperf_ant = a; mperf_ant = m;
    tique_aperf = relogio_tiques();
    tem_aperf = true;
}

static void freq_intel(info_sensores_t *s)
{
    uint64_t v;
    if (cpu.fam != 6 || cpu.mod < 0x0D) return;   /* PERF_STATUS con este formato: Pentium M en adelante */
    if (msr(MSR_PERF_STATUS, &v)) {
        s->mult_x10 = sconv_intel_mult_x10(cpu.mod, v);
        s->vid_mv = sconv_intel_vid_mv(cpu.mod, v);
        if (sconv_intel_core2(cpu.mod)) s->mult_max_x10 = sconv_intel_mult_max_core2_x10(v);
    }
    if (!sconv_intel_core2(cpu.mod) && cpu.mod >= 0x1A) {
        if (msr(MSR_PLATFORM_INFO, &v)) s->mult_max_x10 = (uint32_t)((v >> 8) & 0xFF) * 10;
        if (((cpu.c6a >> 1) & 1) && msr(MSR_TURBO_LIMITE, &v)) s->mult_turbo_x10 = (uint32_t)(v & 0xFF) * 10;
    }
    /* Turbo: con el bit 38 de MISC_ENABLE activado (la BIOS lo desactivó), el
     * CPUID.06H:EAX[1] pasa a decir 0. */
    if (msr(MSR_MISC_ENABLE, &v) && ((v >> 38) & 1)) s->turbo = SENS_TURBO_DESLIG;
    else s->turbo = ((cpu.c6a >> 1) & 1) ? SENS_TURBO_LIGADO : SENS_TURBO_NAO;
    /* Microcódigo: Linux escribe 0 en el 0x8B y ejecuta el CPUID antes de leer;
     * aquí no se escribe (D092), así que puede venir 0 = no disponible. */
    if (msr(MSR_MICROCODIGO, &v)) s->microcodigo = (uint32_t)(v >> 32);
}

static void freq_amd(info_sensores_t *s)
{
    uint64_t v;
    if (cpu.fam == 0x0F) {
        if ((cpu.x7d & 6) == 6 && msr(MSR_K8_FIDVID, &v)) {   /* 80000007H:EDX[1] FID, [2] VID */
            uint32_t khz = sconv_k8_khz(v);
            s->mult_x10 = khz / 10000;
            s->mult_max_x10 = sconv_k8_mult_max_x10(v);
            s->vid_mv = sconv_k8_vid_mv(v);
        }
        return;
    }
    if (cpu.fam >= 0x10 && ((cpu.x7d >> 7) & 1)) {   /* 80000007H:EDX[7] = HwPstate */
        uint32_t lim = 8;
        if (msr(MSR_AMD_PSTATE_LIM, &v)) lim = (uint32_t)((v >> 4) & 7) + 1;
        for (uint32_t i = 0; i < lim && i < SENS_MAX_PSTATES; i++) {
            if (!msr(MSR_AMD_PSTATE0 + i, &v)) break;
            uint32_t khz = sconv_amd_pstate_khz(cpu.fam, v);
            if (!khz) break;
            s->pstate_khz[s->qtd_pstates++] = khz;
        }
        if (s->qtd_pstates && msr(MSR_AMD_PSTATE_ST, &v)) {
            uint32_t p = (uint32_t)(v & 7);
            s->pstate = (int)p;
            if (p < s->qtd_pstates && msr(MSR_AMD_PSTATE0 + p, &v)) {
                s->mult_x10 = sconv_amd_pstate_khz(cpu.fam, v) / 10000;
                s->vid_mv = sconv_amd_vid_mv(cpu.fam, v);
            }
            s->mult_max_x10 = s->pstate_khz[0] / 10000;
        }
    }
    if ((cpu.x7d >> 9) & 1)   /* 80000007H:EDX[9] = Core Performance Boost */
        s->turbo = msr(MSR_AMD_HWCR, &v) && ((v >> 25) & 1) ? SENS_TURBO_DESLIG : SENS_TURBO_LIGADO;
    else
        s->turbo = SENS_TURBO_NAO;
    if (cpu.fam >= 0x10 && msr(MSR_MICROCODIGO, &v)) s->microcodigo = (uint32_t)v;
}

/* ------------------------------------------------------------ muestra */

static void ler_tudo(info_sensores_t *s)
{
    memset(s, 0, sizeof *s);
    s->versao = 1;
    s->pstate = -1;
    if (!cpu.pronto) detectar();
    s->hipervisor = cpu.hipervisor;
    s->tsc_khz = hw_tsc_khz();
    s->base_khz = cpu.c16[0] * 1000;
    s->max_khz = cpu.c16[1] * 1000;
    if (cpu.intel) temp_intel(s);
    else if (cpu.amd) temp_amd(s);
    else txt_copia_n(s->motivo, "fabricante sin sensor conocido", sizeof s->motivo);
    if (!s->qtd_temp && cpu.hipervisor)
        txt_copia_n(s->motivo, "máquina virtual: el hipervisor no emula los sensores", sizeof s->motivo);
    if (cpu.intel) freq_intel(s);
    else if (cpu.amd) freq_amd(s);
    freq_real(s);
    s->msr_falhas = msr_falhas();
    if (!registrado) {
        registrado = true;
        klog("[SENSORES] familia %x modelo %x%s: temperatura %s%s%s; freq. real %s; %u MSR sem resposta\n", cpu.fam,
             cpu.mod, cpu.hipervisor ? " (hipervisor)" : "", s->qtd_temp ? s->fonte_txt : "nao disponivel (",
             s->qtd_temp ? "" : s->motivo, s->qtd_temp ? "" : ")", s->real_khz ? "APERF/MPERF" : "nao disponivel",
             s->msr_falhas);
    }
}

int32_t sensores_info(uint32_t tipo, uint32_t ptr, uint32_t n)
{
    if (tipo == INFO_MSR_PROVA) {
        /* Dos lecturas que no pueden salir bien: un MSR que no existe y el
         * x2APIC ID (0x802) con el APIC en modo xAPIC. La segunda es la que el
         * QEMU sin KVM (TCG) rechaza con #GP; la primera la devuelve como 0
         * sin excepción, y el KVM y las CPUs reales rechazan las dos. */
        static const uint32_t provas[2] = { MSR_INEXISTENTE, 0x802 };
        int32_t r = 0;
        for (int i = 0; i < 2; i++) {
            uint64_t v;
            uint32_t antes = msr_falhas();
            bool leu = msr_ler_seguro(provas[i], &v);
            if (!leu) r |= 1 << i;
            klog("[MSR] prova: 0x%x -> %s (#GP absorvidos: %u, nesta leitura: %u)\n", provas[i],
                 leu ? "lido (o processador aceitou)" : "nao disponivel", msr_falhas(), msr_falhas() - antes);
        }
        return r;
    }
    if (tipo != INFO_SENSORES) return ERRO_INVALIDO;
    if (n < sizeof(info_sensores_t) || !usuario_ok((void *)ptr, sizeof(info_sensores_t), true)) return ERRO_ENDERECO;
    uint32_t agora = relogio_tiques();
    if (!tem_amostra || agora - tique_amostra >= TIQUES_POR_SEGUNDO) {
        ler_tudo(&amostra);
        tique_amostra = relogio_tiques();
        tem_amostra = true;
    }
    amostra.idade_ms = (relogio_tiques() - tique_amostra) * (1000 / TIQUES_POR_SEGUNDO);
    memcpy((void *)ptr, &amostra, sizeof amostra);
    return 0;
}

/* ------------------------------------------------------------ driver de frecuencia */

/* Temperatura de la CPU para el driver de frecuencia (D106): la mayor de las
 * lecturas (en AMD, el Tctl, que nunca es menor que el Tdie), sin la caché
 * de 1 s ni la medida de APERF/MPERF. Las mismas fuentes y reglas que el
 * monitor: solo lectura. false = no hay lectura. */
bool sensores_temp_cpu(int *mc)
{
    static info_sensores_t s;   /* estática: no cabe con holgura en la pila de una tarea del kernel */
    bool i0 = int_salva_desliga();
    if (!cpu.pronto) detectar();
    memset(&s, 0, sizeof s);
    if (cpu.intel) temp_intel(&s);
    else if (cpu.amd) temp_amd(&s);
    bool ok = s.qtd_temp > 0;
    int m = 0;
    for (uint32_t i = 0; i < s.qtd_temp; i++)
        if (i == 0 || s.temp[i].mili_c > m) m = s.temp[i].mili_c;
    int_restaura(i0);
    *mc = m;
    return ok;
}

/* Bandeja (D114): lectura barata para la barra, con la misma muestra de la syscall. */
bool sensores_temp_bandeja(int *mili_c, unsigned *throttling)
{
    uint32_t agora = relogio_tiques();
    if (!tem_amostra || (amostra.qtd_temp && agora - tique_amostra >= TIQUES_POR_SEGUNDO)) {
        ler_tudo(&amostra);
        tique_amostra = relogio_tiques();
        tem_amostra = true;
    }
    if (!amostra.qtd_temp) return false;   /* sin sensor: no se vuelve a preguntar al hardware */
    int m = amostra.temp[0].mili_c;
    for (unsigned i = 1; i < amostra.qtd_temp; i++) if (amostra.temp[i].mili_c > m) m = amostra.temp[i].mili_c;
    *mili_c = m;
    *throttling = (amostra.throttling & SENS_THR_SABE) ? amostra.throttling : 0;
    return true;
}
