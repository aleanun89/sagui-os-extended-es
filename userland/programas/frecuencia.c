// SPDX-License-Identifier: GPL-2.0-or-later
/* frecuencia - control de la frecuencia de la CPU (D106, mejora 061).
 *
 *   frecuencia                      estado: mecanismo, P-states, política y
 *                                   límite de temperatura
 *   frecuencia rendimiento|ahorro|demanda
 *                                   cambia la política (rendimiento = de serie)
 *   frecuencia --limite N           límite de temperatura en °C, de 40 a 79
 *                                   (se puede bajar, nunca subir de 79)
 *   frecuencia --desactivar         deja la CPU como la dejó la BIOS y no la toca
 *   frecuencia --activar            vuelve a activarlo
 *   frecuencia --forzar | --no-forzar
 *                                   subir aunque no se pueda leer la temperatura
 *   frecuencia --guardar            guarda política, límite y desactivado en
 *                                   /home/.frecuencia (se lee al arrancar)
 *   frecuencia --por-nucleo | --paquete
 *                                   con varias CPU (D175): cada núcleo físico
 *                                   según su carga (de serie) o todas el mismo
 *                                   estado, el de la más cargada
 *   frecuencia -c [-n N]            una línea por segundo
 * Solo en la imagen de prueba (driver simulado):
 *   frecuencia --simular-temp N|nd, --simular-uso N|real,
 *              --simular-uso-cpu C N|real, --prueba-gp [C]
 *
 * El nombre del archivo va en español porque es el del comando (interfaz). */
#include "sagui.h"
#include "sensores.h"

static int erro(const char *msg, int r)
{
    if (r == ERRO_NAO_EXISTE) imprimir_em(ERROS, "frecuencia: control de frecuencia no disponible en esta CPU\n");
    else if (r == ERRO_ACESSO) imprimir_em(ERROS, "frecuencia: %s: no permitido ahora\n", msg);
    else imprimir_em(ERROS, "frecuencia: %s: rechazado (%d)\n", msg, r);
    return 1;
}

static void linha(const info_frequencia_t *f)
{
    data_t d;
    char t[24], m[24];
    memset(&d, 0, sizeof d);
    ler_data(&d);
    int a = f->atual;
    imprimir("%02u:%02u:%02u  %-4s %-9s temp. %-11s uso %3u%%  %s%s\n", d.hora, d.minuto, d.segundo,
             a >= 0 ? f->estado_nome[a] : "?", a >= 0 && f->estado_khz[a] ? sensores_mhz(m, sizeof m, f->estado_khz[a]) : "?",
             f->temp_mc != FREQ_TEMP_ND ? sensores_grau(t, sizeof t, f->temp_mc) : "sin lectura", f->uso_cpu,
             frequencia_politica(f->politica), f->limitada ? "  LIMITADA POR TEMPERATURA" : "");
    if (f->versao >= 2 && f->cpus > 1) {   /* D175: el estado de cada CPU */
        info_freq_nucleo_t v[8];
        int n = frequencia_nucleos(v, 8);
        imprimir("          ");
        for (int i = 0; i < n; i++)
            if (v[i].ligada)
                imprimir(" CPU%u %s", v[i].cpu, v[i].atual >= 0 && v[i].atual < (int)f->qtd_estados ? f->estado_nome[v[i].atual] : "?");
        imprimir("\n");
    }
}

static int guardar(const info_frequencia_t *f)
{
    char buf[160];
    int n = formatar_texto(buf, sizeof buf,
                           "# Driver de frecuencia (D106): se lee al arrancar\npolitica=%s\nlimite=%d\ndesactivar=%s\n"
                           "coordinacion=%s\n",
                           f->politica == FREQ_POL_ECONOMIA ? "ahorro" : f->politica == FREQ_POL_DEMANDA ? "demanda" : "rendimiento",
                           f->limite_mc / 1000, f->estado == FREQ_EST_DESATIVADO ? "si" : "no",
                           f->coordenacao == FREQ_COORD_PAQUETE ? "paquete" : "nucleo");
    int fd = abrir("/home/.frecuencia", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        if (fd == ERRO_NAO_EXISTE)
            imprimir_em(ERROS, "frecuencia: no hay /home (sin partición de datos persistente): no se puede guardar\n");
        else
            imprimir_em(ERROS, "frecuencia: no se puede escribir /home/.frecuencia (%d)\n", fd);
        return 1;
    }
    int w = escrever_tudo(fd, buf, (unsigned)n);
    fechar(fd);
    if (w < 0) return erro("guardar", w);
    imprimir("Guardado en /home/.frecuencia (se aplica al arrancar).\n");
    return 0;
}

int principal(int argc, char **argv)
{
    info_frequencia_t f;
    if (!frequencia_ler(&f)) {
        imprimir_em(ERROS, "frecuencia: el kernel no tiene el driver de frecuencia\n");
        return 1;
    }
    bool continuo = false, mudou = false;
    int vezes = 0, r;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int pol = compara_texto(a, "rendimiento") == 0 ? FREQ_POL_RENDIMENTO
                : compara_texto(a, "ahorro") == 0 ? FREQ_POL_ECONOMIA
                : compara_texto(a, "demanda") == 0 || compara_texto(a, "a-demanda") == 0 ? FREQ_POL_DEMANDA : -1;
        if (pol >= 0) {
            if ((r = frequencia_controlar(FREQ_OP_POLITICA, pol)) < 0) return erro(a, r);
            imprimir("Política: %s\n", frequencia_politica((unsigned)pol));
            mudou = true;
        } else if (compara_texto(a, "--limite") == 0 && i + 1 < argc) {
            int g = texto_para_int(argv[++i]);
            if (g > 79) {
                imprimir_em(ERROS, "frecuencia: el límite no puede pasar de 79 °C (solo se puede bajar)\n");
                return 1;
            }
            if ((r = frequencia_controlar(FREQ_OP_LIMITE, g * 1000)) < 0) return erro("--limite (de 40 a 79 °C)", r);
            imprimir("Límite de temperatura: %d °C (vuelve a subir por debajo de %d °C)\n", g, g - 7);
            mudou = true;
        } else if (compara_texto(a, "--desactivar") == 0) {
            if ((r = frequencia_controlar(FREQ_OP_DESATIVAR, 0)) < 0) return erro(a, r);
            imprimir("Driver desactivado: la CPU queda en el estado de la BIOS.\n");
            mudou = true;
        } else if (compara_texto(a, "--activar") == 0) {
            if ((r = frequencia_controlar(FREQ_OP_ATIVAR, 0)) < 0) return erro(a, r);
            imprimir("Driver activado.\n");
            mudou = true;
        } else if (compara_texto(a, "--forzar") == 0 || compara_texto(a, "--no-forzar") == 0) {
            bool sim = a[2] == 'f';
            if ((r = frequencia_controlar(FREQ_OP_FORCAR, sim)) < 0) return erro(a, r);
            if (sim)
                imprimir("AVISO: sin lectura de temperatura, la CPU subirá igualmente y el límite de 79 °C\n"
                         "no podrá actuar. Solo si sabes que la refrigeración es buena. No se guarda.\n");
            else
                imprimir("Sin forzar: sin temperatura no pasa del estado de la BIOS.\n");
            mudou = true;
        } else if (compara_texto(a, "--guardar") == 0) {
            frequencia_ler(&f);
            if (guardar(&f)) return 1;
        } else if (compara_texto(a, "--simular-temp") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            int mc = compara_texto(v, "nd") == 0 ? FREQ_TEMP_ND : texto_para_int(v) * 1000;
            if ((r = frequencia_controlar(FREQ_OP_SIM_TEMP, mc)) < 0) return erro("--simular-temp (solo simulado)", r);
            mudou = true;
        } else if (compara_texto(a, "--simular-uso") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if ((r = frequencia_controlar(FREQ_OP_SIM_USO, compara_texto(v, "real") == 0 ? -1 : texto_para_int(v))) < 0)
                return erro("--simular-uso (solo simulado)", r);
            mudou = true;
        } else if (compara_texto(a, "--simular-uso-cpu") == 0 && i + 2 < argc) {
            int c = texto_para_int(argv[++i]);
            const char *v = argv[++i];
            int u = compara_texto(v, "real") == 0 ? 0xFFFF : texto_para_int(v);
            if ((r = frequencia_controlar(FREQ_OP_SIM_USO_CPU, (c << 16) | (u & 0xFFFF))) < 0)
                return erro("--simular-uso-cpu (solo simulado)", r);
            mudou = true;
        } else if (compara_texto(a, "--prueba-gp") == 0) {
            int c = 0;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') c = texto_para_int(argv[++i]);
            if ((r = frequencia_controlar(FREQ_OP_SIM_GP, c)) < 0) return erro("--prueba-gp (solo simulado)", r);
            mudou = true;
        } else if (compara_texto(a, "--paquete") == 0 || compara_texto(a, "--por-nucleo") == 0) {
            bool paq = a[2] == 'p' && a[3] == 'a';
            if ((r = frequencia_controlar(FREQ_OP_COORDENACAO, paq ? FREQ_COORD_PAQUETE : FREQ_COORD_NUCLEO)) < 0)
                return erro(a, r);
            imprimir("Coordinación: %s\n", paq ? "por paquete (todas las CPU el mismo estado)" : "por núcleo físico");
            mudou = true;
        } else if (compara_texto(a, "-c") == 0) {
            continuo = true;
        } else if (compara_texto(a, "-n") == 0 && i + 1 < argc) {
            vezes = texto_para_int(argv[++i]);
        } else {
            imprimir("Uso: frecuencia [rendimiento|ahorro|demanda] [--limite N] [--desactivar|--activar]\n"
                     "                  [--forzar|--no-forzar] [--guardar] [-c [-n N]]\n"
                     "  sin opciones: mecanismo, P-states, política y límite de temperatura\n"
                     "  rendimiento   el estado más rápido (de serie); ahorro: el más lento;\n"
                     "  demanda       sube con carga y baja en reposo\n"
                     "  --limite N    límite de temperatura (40-79 °C; nunca más de 79): al llegar,\n"
                     "                el estado más lento hasta bajar 7 °C\n"
                     "  --desactivar  la CPU como la dejó la BIOS\n"
                     "  --guardar     en /home/.frecuencia (también: desactivar=si para arrancar sin él)\n"
                     "  --por-nucleo  con varias CPU, cada núcleo físico según su carga (de serie);\n"
                     "  --paquete     todas el mismo estado (el de la CPU más cargada)\n"
                     "  -c            una línea por segundo (-n N: solo N líneas)\n");
            return compara_texto(a, "-h") == 0 || compara_texto(a, "--help") == 0 ? 0 : 1;
        }
    }
    if (continuo) {
        for (int k = 0; vezes <= 0 || k < vezes; k++) {
            if (k) dormir(1000);
            frequencia_ler(&f);
            linha(&f);
        }
        return 0;
    }
    if (mudou) return 0;
    static char buf[1024];
    frequencia_ler(&f);
    int n = frequencia_texto(&f, buf, sizeof buf);
    escrever_tudo(SAIDA, buf, (unsigned)n);
    registrar("frecuencia: estado %u mec %u pol %u atual %d bios %d temp %d limite %d limitada %u", f.estado, f.mecanismo,
              f.politica, f.atual, f.bios, f.temp_mc, f.limite_mc, f.limitada);
    if (f.versao >= 2 && f.cpus > 1) {   /* D175: una línea por CPU para las pruebas */
        info_freq_nucleo_t v[8];
        int q = frequencia_nucleos(v, 8);
        for (int i = 0; i < q; i++)
            if (v[i].ligada)
                registrar("frecuencia: cpu %u atual %d pedido %d alvo %d bios %d uso %u escritas %u pendente %u coord %u",
                          v[i].cpu, v[i].atual, v[i].pedido, v[i].alvo, v[i].bios, v[i].uso, v[i].escritas, v[i].pendente,
                          f.coordenacao);
    }
    return 0;
}
