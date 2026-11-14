// SPDX-License-Identifier: GPL-2.0-or-later
/* amd - el estado del motor 3D y la identificación de la GPU de AMD
 * (mejora 054, parte B2; D158).
 *
 * Con --ayuda:
 *   amd                 el estado del motor y la GPU que hay
 *   amd --tabla         la tabla de familias (B1 y B2)
 *   amd --bar b d f n o t   mapea una ventana de un BAR y lee los registros
 *                        (sirve para probar el kernel con un dispositivo de
 *                        mentira en el QEMU, y para diagnosticar en el dv7)
 *   amd --simular       finge una GPU que no responde: el vigilante tiene que
 *                        vencer y el motor volver al software
 */
#include "amd.h"

static void uso(void)
{
    escrever_texto(SAIDA,
        "uso: amd [--tabla] [--bar bus dispositivo funcion bar desplazamiento bytes] [--simular]\n"
        "\n"
        "Muestra el estado del motor 3D (el de software es el camino por defecto),\n"
        "la GPU de AMD que hay en el bus PCI con su familia y sus registros, y\n"
        "la tabla de familias del driver (mejora 054, partes B1 y B2).\n");
}

static void tabela_familias(void)
{
    int n;
    const amd_familia_info_t *const *t = amd_tabela(&n);
    imprimir("Tabla de familias del driver (mejora 054)\n");
    for (int i = 0; i < n; i++) {
        const amd_familia_info_t *f = t[i];
        imprimir("  %s (generacion %u, microcodo %s)\n", f->nome, f->geracao, f->microcodigo ? "si" : "no");
        imprimir("      %s; chips: %s\n", f->detalhe, f->chip);
        imprimir("      identificadores:");
        for (unsigned k = 0; k < f->n_faixas; k++)
            imprimir(" %04x-%04x", f->faixas[k].a, f->faixas[k].b);
        imprimir("\n");
        imprimir("      registros: MM_INDEX 0x%x, MM_DATA 0x%x", f->mm_index, f->mm_data);
        if (f->id_registro >= 0)
            imprimir(", DEVICE_ID 0x%x, REV_ID 0x%x, SUBSYS_ID 0x%x", f->id_registro, f->rev_registro, f->subsis_registro);
        else
            imprimir(", el identificador y la revision van por el PCI");
        if (f->grbm_indice >= 0) imprimir(", GRBM_STATUS 0x%x", f->grbm_indice);
        else imprimir(", el estado del bus maestro esta en otro indice (a confirmar)");
        imprimir(" (BAR %u)\n", f->bar_registros);
        imprimir("      ISA de los shaders: %s\n", f->isa);
        imprimir("      reutiliza de B1: %s\n", f->reutiliza);
        imprimir("      falta de B1: %s\n", f->falta);
    }
}

static void estado_motor(void)
{
    imprimir("Motor 3D\n");
    imprimir("  Motor en uso: %s\n", amd_motor_nome());
    imprimir("  Aceleracion: %s\n", amd_acel_nome());
    if (amd_motivo()[0]) imprimir("  Motivo: %s\n", amd_motivo());
    else imprimir("  Motivo: (ninguno)\n");
    imprimir("  Vigilante de la GPU: %u ms\n", amd_plazo_vigilante());
}

static void testar_bar(int bus, int disp, int func, int num, unsigned desloc, unsigned bytes)
{
    uint32_t mapa = 0;
    int r = amd_mapear_bar((unsigned)bus, (unsigned)disp, (unsigned)func, (unsigned)num,
                           desloc, bytes ? bytes : 4, 0, &mapa);
    if (r < 0) {
        imprimir_em(ERROS, "amd: no se pudo mapear el BAR %u de %02x:%02x.%u: %s (%d)\n",
                    num, bus, disp, func, nome_erro(r), r);
        return;
    }
    imprimir("BAR %u de %02x:%02x.%u mapeado en %p (%u bytes desde el desplazamiento %u)\n",
             num, bus, disp, func, (void *)mapa, bytes ? bytes : 4, desloc);
    volatile uint32_t *p = (volatile uint32_t *)mapa;
    unsigned n = (bytes ? bytes : 4) / 4;
    if (n > 16) n = 16;
    for (unsigned i = 0; i < n; i++)
        imprimir("  +%04u: %08x\n", desloc + i * 4, p[i]);
    amd_desejar_bar(mapa);
    imprimir("  mapa deshecho\n");
}

static void simular_falha(void)
{
    amd_gpu_t g;
    memset(&g, 0, sizeof g);
    g.tem_gpu = 1;
    g.modelo = 0x95C2;
    g.info = amd_buscar_familia(g.modelo);
    g.familia = g.info ? g.info->familia : AMD_FAMILIA_NENHUMA;
    imprimir("Simulacion: una GPU que nunca responde (plazo de 200 ms)\n");
    int r = amd_esperar(&g, 200, amd_ler_simulado, 0, 0);
    if (r == 0) {
        imprimir("  el vigilante: la GPU respondio a tiempo (no deberia pasar)\n");
        return;
    }
    imprimir("  el vigilante: se paso de plazo\n");
    imprimir("  motor tras el fallo: %s\n", amd_motor_nome());
    imprimir("  aceleracion tras el fallo: %s\n", amd_acel_nome());
    imprimir("  motivo: %s\n", amd_motivo());
}

/* La lectura de los registros de una familia, con el BAR de cualquier
 * dispositivo: sirve para probar la maquinaria de lectura (los registros
 * directos y el par MM_INDEX/MM_DATA) sin una GPU de AMD, y para diagnosticar
 * en el dv7. */
/* Copia un texto corto (con el final en cero). */
static void copiar_texto_proprio(char *dst, const char *orig, unsigned max)
{
    unsigned i = 0;
    for (; i + 1 < max && orig[i]; i++) dst[i] = orig[i];
    dst[i] = 0;
}

static void ler_registros(int bus, int disp, int func, const char *familia)
{
    const amd_familia_info_t *f = amd_familia_nome(familia);
    if (!f) {
        imprimir_em(ERROS, "amd: no conozco la familia %s\n", familia);
        return;
    }
    amd_gpu_t g;
    memset(&g, 0, sizeof g);
    g.barramento = (unsigned)bus;
    g.dispositivo = (unsigned)disp;
    g.funcao = (unsigned)func;
    g.info = f;
    g.familia = f->familia;
    g.bar = f->bar_registros;
    uint32_t mapa = 0;
    /* Leer los registros indirectos exige escribir el índice: el mapa va con
     * permiso de escritura cuando la familia los usa. */
    int escrever = f->grbm_indice >= 0 ? 1 : 0;
    int r = amd_mapear_bar((unsigned)bus, (unsigned)disp, (unsigned)func, f->bar_registros,
                           0, 64 * 1024, escrever, &mapa);
    if (r < 0) {
        imprimir_em(ERROS, "amd: no se pudo mapear el BAR %u de %02x:%02x.%u: %s (%d)\n",
                    f->bar_registros, bus, disp, func, nome_erro(r), r);
        return;
    }
    g.mapa = mapa;
    g.tamanho = 64 * 1024;
    g.mapeada = 1;
    amd_leer_registros(&g);
    imprimir("Registros de %s en el BAR %u de %02x:%02x.%u (mapeado en %p, %s)\n",
             f->nome, f->bar_registros, bus, disp, func, (void *)mapa,
             escrever ? "con permiso de escritura, para el indice" : "solo lectura");
    if (f->id_registro >= 0) {
        imprimir("  DEVICE_ID  %08x\n", g.device_id);
        imprimir("  REV_ID     %08x\n", g.rev_mmio);
        imprimir("  SUBSYS_ID  %08x  SUBSYS_VENDOR %08x\n", g.subsis_id, g.subsis_vendor);
    } else {
        imprimir("  (esta familia no tiene el identificador directo: va por el PCI)\n");
    }
    if (g.tem_grbm) imprimir("  GRBM_STATUS %08x (indice 0x%x, leido por MM_INDEX 0x%x)\n",
                             g.grbm_status, f->grbm_indice, f->mm_index);
    amd_desejar_bar(mapa);
    imprimir("  mapa deshecho\n");
}

int principal(int argc, char **argv)
{
    bool so_tabela = false, simular = false, testar = false, ler = false;
    int bus = 0, disp = 0, func = 0, num = 0;
    unsigned desloc = 0, bytes = 0;
    char nome_familia[48];
    int valores[6];
    nome_familia[0] = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "--help") == 0 || compara_texto(a, "-h") == 0) {
            uso();
            return 0;
        } else if (compara_texto(a, "--tabla") == 0) {
            so_tabela = true;
        } else if (compara_texto(a, "--simular") == 0) {
            simular = true;
        } else if (compara_texto(a, "--leer") == 0) {
            if (i + 4 >= argc) {
                imprimir_em(ERROS, "amd: --leer necesita bus, dispositivo, funcion y familia\n");
                return 2;
            }
            bool ok = true;
            for (int k = 0; k < 3; k++)
                if (!texto_para_numero(argv[i + 1 + k], &valores[k])) ok = false;
            if (!ok) {
                imprimir_em(ERROS, "amd: --leer espera tres numeros y el nombre de la familia\n");
                return 2;
            }
            bus = valores[0]; disp = valores[1]; func = valores[2];
            copiar_texto_proprio(nome_familia, argv[i + 4], sizeof nome_familia);
            ler = true;
            i += 4;
        } else if (compara_texto(a, "--bar") == 0) {
            if (i + 6 >= argc) {
                imprimir_em(ERROS, "amd: --bar necesita bus, dispositivo, funcion, BAR, desplazamiento y bytes\n");
                return 2;
            }
            bool ok = true;
            for (int k = 0; k < 6; k++)
                if (!texto_para_numero(argv[i + 1 + k], &valores[k])) ok = false;
            if (!ok) {
                imprimir_em(ERROS, "amd: --bar espera seis numeros\n");
                return 2;
            }
            bus = valores[0]; disp = valores[1]; func = valores[2];
            num = valores[3]; desloc = (unsigned)valores[4]; bytes = (unsigned)valores[5];
            testar = true;
            i += 6;
        } else {
            imprimir_em(ERROS, "amd: no conozco la opcion %s\n", a);
            uso();
            return 2;
        }
    }

    amd_iniciar();

    if (so_tabela) {
        tabela_familias();
        return 0;
    }
    if (testar) {
        testar_bar(bus, disp, func, num, desloc, bytes);
        return 0;
    }
    if (simular) {
        simular_falha();
        return 0;
    }
    if (ler) {
        ler_registros(bus, disp, func, nome_familia);
        return 0;
    }

    estado_motor();

    amd_gpu_t g;
    int r = amd_identificar(&g);
    if (r < 0) {
        imprimir("GPU\n  no se pudo identificar: %s\n", g.erro[0] ? g.erro : nome_erro(r));
        return 1;
    }
    if (!g.tem_gpu) {
        imprimir("GPU\n  no hay ninguna GPU AMD en el bus PCI\n");
        imprimir("  (el motor por software de SaguiGL es el camino por defecto)\n");
        return 0;
    }
    imprimir("GPU\n");
    imprimir("  PCI %02x:%02x.%u  device %04x  revision %02x\n",
             g.barramento, g.dispositivo, g.funcao, g.modelo, g.revisao);
    if (g.info) {
        imprimir("  Familia: %s\n", g.info->nome);
        imprimir("    %s (generacion %u, chips %s)\n", g.info->detalhe, g.info->geracao, g.info->chip);
        imprimir("    microcodo: %s\n", g.info->microcodigo ? "si" : "no");
        imprimir("    ISA de los shaders: %s\n", g.info->isa);
        imprimir("    reutiliza de B1: %s\n", g.info->reutiliza);
        imprimir("    falta de B1: %s\n", g.info->falta);
    } else {
        imprimir("  Familia: desconocida (no esta en la tabla del driver)\n");
    }
    if (g.mapeada) {
        imprimir("  Registros leidos (BAR %u, %u bytes):\n", g.bar, g.tamanho);
        if (g.info && g.info->id_registro >= 0) {
            imprimir("    DEVICE_ID  %08x\n", g.device_id);
            imprimir("    REV_ID     %08x\n", g.rev_mmio);
            imprimir("    SUBSYS_ID  %08x  SUBSYS_VENDOR %08x\n", g.subsis_id, g.subsis_vendor);
        } else {
            imprimir("    (esta familia no tiene el identificador directo: va por el PCI)\n");
        }
        if (g.tem_grbm) imprimir("    GRBM_STATUS %08x\n", g.grbm_status);
        if (g.erro[0]) imprimir("    aviso: %s\n", g.erro);
    } else if (g.erro[0]) {
        imprimir("  no se pudo mapear el BAR: %s\n", g.erro);
    }
    return 0;
}
