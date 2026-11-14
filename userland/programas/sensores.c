// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores - el monitor de hardware en modo texto (D092, mejora 050).
 *
 *   sensores           temperatura, frecuencias, multiplicador, P-states,
 *                      VID, turbo y microcódigo (las mismas líneas del informe);
 *                      al final, el driver de frecuencia (D106) y, si lo
 *                      está, «LIMITADA POR TEMPERATURA»
 *   sensores -c [-n N] una línea por segundo, hasta Ctrl+C (o N líneas)
 *   sensores --prova-msr
 *                      lee a propósito un MSR que no existe: el kernel debe
 *                      responder "no disponible" y seguir (prueba de la H71)
 *
 * El kernel sólo lee los sensores cuando alguien lo pide y como máximo una vez por
 * segundo; no se escribe nada en el hardware (D092). La placa (ventiladores, SPD)
 * queda pendiente: no hay opción --placa todavía. */
#include "sagui.h"
#include "sensores.h"

static void linha_continua(const info_sensores_t *s)
{
    char t[24], f[24];
    data_t d;
    memset(&d, 0, sizeof d);
    ler_data(&d);
    int m = sensores_temp_maior(s);
    const char *orig;
    unsigned khz = sensores_freq_atual(s, &orig);
    imprimir("%02u:%02u:%02u  temp. %-14s  frec. %-10s%s%s%s", d.hora, d.minuto, d.segundo,
             m >= -100000 && s->qtd_temp ? sensores_grau(t, sizeof t, m) : "no disponible",
             khz ? sensores_mhz(f, sizeof f, khz) : "n/d", khz ? " (" : "", khz ? orig : "", khz ? ")" : "");
}

/* Indicador del driver de frecuencia (D106) al final de la línea continua. */
static void sufixo_frequencia(void)
{
    info_frequencia_t fq;
    if (frequencia_ler(&fq) && fq.limitada) imprimir("  LIMITADA POR TEMPERATURA");
}

int principal(int argc, char **argv)
{
    bool continuo = false;
    int vezes = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-c") == 0) continuo = true;
        else if (compara_texto(argv[i], "-n") == 0 && i + 1 < argc) vezes = texto_para_int(argv[++i]);
        else if (compara_texto(argv[i], "--prova-msr") == 0) {
            int r = info_sistema(INFO_MSR_PROVA, NULL, 0);
            if (r < 0) { imprimir_em(ERROS, "sensores: el kernel no tiene la prueba\n"); return 1; }
            imprimir("MSR 0x0BADF00D (no existe):     %s\n", r & 1 ? "no disponible" : "leído (esta CPU o emulador lo acepta)");
            imprimir("MSR 0x802 (x2APIC desactivado): %s\n", r & 2 ? "no disponible" : "leído (x2APIC activo)");
            imprimir("El sistema sigue funcionando: la lectura protegida ha absorbido los fallos.\n");
            return 0;
        } else {
            imprimir("Uso: sensores [-c [-n N]]\n"
                     "  sin opciones: temperatura, frecuencias, multiplicador, VID, turbo y microcódigo\n"
                     "  -c            una línea por segundo hasta Ctrl+C (-n N: solo N líneas)\n");
            return compara_texto(argv[i], "-h") == 0 ? 0 : 1;
        }
    }
    info_sensores_t s;
    if (!sensores_ler(&s)) {
        imprimir_em(ERROS, "sensores: el kernel no tiene el monitor de hardware\n");
        return 1;
    }
    if (!continuo) {
        static char buf[3072];
        int n = sensores_texto(&s, buf, sizeof buf);
        info_frequencia_t fq;
        if (frequencia_ler(&fq)) n += frequencia_texto(&fq, buf + n, (int)sizeof buf - n);   /* D106 */
        escrever_tudo(SAIDA, buf, (unsigned)n);
        registrar("sensores: fonte %u, %u temperaturas, real %u kHz, msr falhas %u", s.fonte, s.qtd_temp, s.real_khz,
                  s.msr_falhas);
        return 0;
    }
    for (int k = 0; vezes <= 0 || k < vezes; k++) {
        if (k) { dormir(1000); sensores_ler(&s); }
        linha_continua(&s);
        sufixo_frequencia();
        imprimir("\n");
    }
    return 0;
}
