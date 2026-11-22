// SPDX-License-Identifier: GPL-2.0-or-later
/* acel2d - el 2D por la GPU del escritorio (D195).
 *
 *   acel2d            estado: qué acelerador se usa (o por qué la CPU), cuántas
 *                     copias y rellenos ha hecho la GPU, el reparto con el software
 *                     (D346) y la medida del arrastre. Sin ser administrador.
 *   acel2d probar     autoprueba completa ahora: copias solapadas en todas las
 *                     direcciones, rellenos, los bordes y muchas órdenes al azar,
 *                     comparadas píxel a píxel con la CPU, y una medida de la GPU
 *                     contra la CPU. Si el acelerador quedó apagado en el
 *                     arranque, se vuelve a iniciar solo para la prueba (sirve
 *                     para mirarlo en el hardware: el detalle va al log del kernel,
 *                     líneas [ACEL2D]).
 *
 *   acel2d probar --detalle   lo mismo y, además, copias sueltas de varios tamaños con el tiempo de
 *                     cada una (GPU y CPU) y los registros del motor antes y después, volcados al log
 *                     [ACEL2D] (D246): para sacar una foto o hacer «dmesg > /home/acel.txt»
 *
 *   acel2d apagar     apaga el acelerador (todo pasa a la CPU) sin regrabar (D244)
 *   acel2d encender   lo vuelve a elegir y a poner a prueba, como al arrancar (D244)
 *
 *   acel2d volcado [apagar | encender | medir]   (D387) el volcado del búfer de atrás a la pantalla por
 *                     la GPU (GART de las Radeon R5xx): sin más, su estado; apagar y encender (repite
 *                     la autoprueba; a mano se enciende aunque sea más lento); medir copia 10 veces la
 *                     pantalla entera por la GPU y por la CPU (líneas [ACEL2D] volcado medir en el log)
 *   acel2d volcado paso N   (depuración en hardware real, 0-6) la puesta en marcha de la GART solo hasta
 *                     el paso N, con los registros al log, y deshecha: 0 solo lee; 1 tabla; 2 registros
 *                     sin encender; 3 encendida; 4 bus maestro; 5 copia de una página; 6 copia de 1024x64.
 *                     Puede colgar el equipo: guardar el log (dmesg > /home/x.txt; sync) entre paso y paso
 *                     7-17 (dbg5): experimentos, la pantalla entera por la GART cambiando una cosa cada vez
 *                     (lista en gv_experimento, kernel/dispositivos/acel_radeon.c); 20 borra las opciones de
 *                     depuración y 21-25 cambian una (RELOJ, AGP, SNOOP, CIEGO, MARCAS); 26 (D387) el 2D por MMIO
 *                     aunque el CP del 3D esté en marcha (solo para comparar); 27-35 (D387) la valla del 3D
 *                     y su espera: 27 VRAM, 28 RBBM, 29 HDP, 30 PAUSA, 31 TIJERA, 32 LINUX, 33 SINESPERA,
 *                     34 LEERCOLA, 35 2DSINESPERA, 36 COLAENTERA (D436) (ver gpu3d_r500.c; funcionan también con el 2D apagado)
 *
 *   acel2d vuelta [apagar | encender | medir]   (D391) la vuelta del cuadro del 3D a la RAM por la GPU
 *                     (la misma GART): sin más, su estado y cuántos cuadros ha traído; apagar y encender
 *                     (repite la autoprueba); medir trae 8 veces 320x32 píxeles de la VRAM por la GPU y por
 *                     la CPU (línea [ACEL2D] vuelta medir en el log)
 *
 *   acel2d energia [leer | bios | bajo | alto | auto | estado N | simular N|bios | detalle | desbloquear]
 *                     (D399) la energía de la GPU: sin más (y sin ser administrador), relojes del motor y de
 *                     la memoria, los de la BIOS, la tensión, la temperatura (o «sin sensor») con las zonas
 *                     térmicas del ACPI aparte, la VRAM y los estados de la tabla de la BIOS. leer: los
 *                     relojes de ahora por la BIOS (GetEngineClock); bios/bajo/alto/auto: el modo, ahora;
 *                     estado N: ese estado a mano; simular: ensaya el cambio sin escribir nada (lo que haría
 *                     la BIOS, al log [GPUENERGIA]); detalle: registros de los PLL al log; desbloquear: tras
 *                     un fallo, deja volver a intentarlo. El modo al arrancar: kconfig gpu_energia.
 *
 * El nombre del archivo es el del comando. */
#include "sagui.h"
#include "contas.h"

/* ---- D399: energía de la GPU ---- */
static const char *texto_tipo(unsigned t)
{
    static const char *const n[] = { "", "ahorro", "bateria", "equilibrado", "rendimiento", "por defecto" };
    return t < 6 ? n[t] : "?";
}

static void mhz(char *t, unsigned n, unsigned khz)
{
    if (!khz) copia_texto(t, "?", n);
    else formatar_texto(t, n, "%u MHz", (khz + 500) / 1000);
}

static int energia_estado(void)
{
    static info_gpu_energia_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_GPU_ENERGIA, &v, sizeof v) != 0 || v.versao != 1) {
        imprimir_em(ERROS, "acel2d energia: el kernel no lo sabe\n");
        return 1;
    }
    static const char *const sop[] = { "sin driver", "solo lectura", "control" };
    static const char *const modos[] = { "?", "bios", "bajo", "alto", "auto", "manual" };
    static const char *const fuentes[] = { "no se sabe", "leidos por la BIOS", "los de por defecto de la BIOS, sin leer", "los puestos" };
    char a[24], b[24], c[24], d[24];
    imprimir("Energia de la GPU: %s (%s)\n", v.driver[0] ? v.driver : "-", sop[v.soporte < 3 ? v.soporte : 0]);
    imprimir("  %s\n", v.motivo);
    if (v.bios[0]) imprimir("  BIOS de video: %s\n", v.bios);
    if (!v.soporte) return 0;
    imprimir("  modo: %s (kconfig gpu_energia: %s)%s%s\n", modos[v.modo <= 5 ? v.modo : 0],
             v.modo_kconfig <= 4 ? (v.modo_kconfig ? modos[v.modo_kconfig] : "automatico") : "?",
             v.confirmada ? "; tarjeta confirmada" : "; tarjeta sin confirmar", v.bloqueado ? "; BLOQUEADO tras un fallo" : "");
    mhz(a, sizeof a, v.sclk_khz); mhz(b, sizeof b, v.mclk_khz);
    imprimir("  motor %s, memoria %s (%s", a, b, fuentes[v.relojes_fuente < 4 ? v.relojes_fuente : 0]);
    if (v.relojes_fuente == 1) imprimir(" hace %u s", v.relojes_edad_s);
    imprimir(")\n");
    mhz(a, sizeof a, v.sclk_def_khz); mhz(b, sizeof b, v.mclk_def_khz); mhz(c, sizeof c, v.sclk_max_khz); mhz(d, sizeof d, v.mclk_max_khz);
    imprimir("  por defecto de la BIOS: %s / %s; maximos del chip: %s / %s; referencia %u kHz\n", a, b, c, d, v.ref_khz);
    if (v.sclk_arranque_khz) {
        mhz(a, sizeof a, v.sclk_arranque_khz); mhz(b, sizeof b, v.mclk_arranque_khz);
        imprimir("  al arrancar (a lo que vuelve «bios»): %s / %s\n", a, b);
    }
    if (v.tensao_mv) imprimir("  tension: %u mV (la de arranque de la BIOS)\n", v.tensao_mv);
    else if (v.estado >= 0) imprimir("  tension: %s\n", v.tensao_txt_baixa ? "baja (pin GPIO del estado)" : "la del estado (sin valor en mV)");
    if (v.temp_mili_c != GPU_EN_SIN_SENSOR) imprimir("  temperatura: %d C (%s)\n", v.temp_mili_c / 1000, v.temp_fuente);
    else imprimir("  temperatura: %s\n", v.temp_fuente);
    if (v.n_zonas) {
        imprimir("  zonas termicas del ACPI (no son de la tarjeta):");
        for (unsigned i = 0; i < v.n_zonas && i < GPU_EN_MAX_ZONAS; i++) imprimir(" %s %d C", v.zona_nome[i], v.zona_mili_c[i] / 1000);
        imprimir("\n");
    }
    if (v.vram_kb) imprimir("  VRAM: %u MB; SaguiOS usa unos %u MB (pantalla, 3D y tablas)\n", v.vram_kb / 1024, (v.vram_usada_kb + 1023) / 1024);
    imprimir("  estados de la BIOS (bajo = %d, alto = %d con las pantallas de ahora):\n", v.estado_bajo, v.estado_alto);
    for (unsigned i = 0; i < v.n_estados && i < GPU_EN_MAX_ESTADOS_ABI; i++) {
        const info_gpu_estado_t *e = &v.estados[i];
        mhz(a, sizeof a, e->sclk_khz); mhz(b, sizeof b, e->mclk_khz);
        imprimir("  %c %u: %s / %s %s%s%s%s\n", (int)i == v.estado ? '>' : ' ', i, a, b, texto_tipo(e->tipo),
                 e->tensao == 1 ? ", tension por GPIO" : e->tensao == 2 ? ", tension por VDDC" : "",
                 e->sem_ecra ? ", sin pantalla" : "", e->so_um_ecra ? ", una pantalla" : "");
    }
    imprimir("  cambios %u, fallos %u, el ultimo con el motor parado %u us\n", v.cambios, v.fallos, v.ultimo_us);
    if (v.ultimo[0]) imprimir("  ultimo: %s\n", v.ultimo);
    imprimir("acel2d energia: modo %s estado %d motor %u memoria %u fallos %u%s\n", modos[v.modo <= 5 ? v.modo : 0], v.estado,
             v.sclk_khz, v.mclk_khz, v.fallos, v.bloqueado ? " bloqueado" : "");
    return 0;
}

static int energia(int argc, char **argv)
{
    if (argc < 3) return energia_estado();
    unsigned op = 0, arg = 0;
    const char *s = argv[2];
    if (compara_texto(s, "leer") == 0) op = GPU_EN_OP_LEER;
    else if (compara_texto(s, "bios") == 0) { op = GPU_EN_OP_MODO; arg = GPU_EN_MODO_BIOS; }
    else if (compara_texto(s, "bajo") == 0) { op = GPU_EN_OP_MODO; arg = GPU_EN_MODO_BAJO; }
    else if (compara_texto(s, "alto") == 0) { op = GPU_EN_OP_MODO; arg = GPU_EN_MODO_ALTO; }
    else if (compara_texto(s, "auto") == 0 || compara_texto(s, "dinamico") == 0) { op = GPU_EN_OP_MODO; arg = GPU_EN_MODO_AUTO; }
    else if (compara_texto(s, "detalle") == 0) op = GPU_EN_OP_DETALLE;
    else if (compara_texto(s, "desbloquear") == 0) op = GPU_EN_OP_DESBLOQUEAR;
    else if ((compara_texto(s, "estado") == 0 || compara_texto(s, "simular") == 0) && argc > 3) {
        op = compara_texto(s, "estado") == 0 ? GPU_EN_OP_ESTADO : GPU_EN_OP_SIMULAR;
        if (op == GPU_EN_OP_SIMULAR && compara_texto(argv[3], "bios") == 0) arg = 0xFFFF;
        else if (argv[3][0] >= '0' && argv[3][0] <= '9') arg = (unsigned)texto_para_int(argv[3]);
        else op = 0;
    }
    if (!op) {
        imprimir_em(ERROS, "uso: acel2d energia [leer | bios | bajo | alto | auto | estado N | simular N|bios | detalle | desbloquear]\n");
        return 1;
    }
    int r = info_sistema(INFO_GPU_ENERGIA_CTL, (void *)(long)GPU_EN_PEDIDO(op, arg), 0);
    static info_gpu_energia_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_GPU_ENERGIA, &v, sizeof v);
    imprimir("acel2d energia %s: %s%s%s\n", s, r == 0 ? "bien" : "NO", v.ultimo[0] ? " - " : "", v.ultimo);
    energia_estado();
    return r == 0 ? 0 : 1;
}

int principal(int argc, char **argv)
{
    /* D255: probar, apagar y encender tocan la GPU: solo el administrador. */
    /* D399: «acel2d energia» sin más solo lee (como «acel2d»). */
    bool so_ler = argc == 2 && compara_texto(argv[1], "energia") == 0;
    if (argc > 1 && !so_ler && compara_texto(argv[1], "-h") != 0 && compara_texto(argv[1], "--ayuda") != 0 && falta_admin("acel2d"))
        return 1;
    if (argc > 1 && compara_texto(argv[1], "energia") == 0) return energia(argc, argv);
    if (argc > 1 && (compara_texto(argv[1], "probar") == 0 || compara_texto(argv[1], "-p") == 0)) {
        info_acel2d_prova_t r;
        memset(&r, 0, sizeof r);
        int detalle = argc > 2 && (compara_texto(argv[2], "--detalle") == 0 || compara_texto(argv[2], "-d") == 0);
        r.versao = detalle ? 2 : 0;
        if (info_sistema(INFO_ACEL2D_PROVA, &r, sizeof r) != 0) {
            imprimir_em(ERROS, "acel2d: el kernel no tiene la prueba\n");
            return 1;
        }
        imprimir("acel2d probar: %s\n", r.texto);
        if (r.resultado >= 0)
            imprimir("acel2d probar: %u ordenes, %u pixeles distintos, suma GPU %08x CPU %08x\n",
                     r.ordens, r.diferentes, r.soma_gpu, r.soma_cpu);
        if (r.resultado == 1)
            imprimir("acel2d probar: medida: GPU %u us, CPU %u us\n", r.us_gpu, r.us_cpu);
        if (detalle) imprimir("acel2d probar: detalle volcado al log del kernel (lineas [ACEL2D]; dmesg > /home/acel.txt)\n");
        imprimir("acel2d probar: %s\n", r.resultado == 1 ? "BIEN" : r.resultado == 0 ? "MAL" : "sin acelerador");
        return r.resultado == 1 ? 0 : 1;
    }
    if (argc > 1 && (compara_texto(argv[1], "apagar") == 0 || compara_texto(argv[1], "encender") == 0)) {
        int enc = compara_texto(argv[1], "encender") == 0;
        int r = info_sistema(INFO_ACEL2D_CTL, (void *)(long)enc, 0);
        info_acel2d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_ACEL2D, &v, sizeof v);
        imprimir("acel2d %s: %s (%s)\n", argv[1], v.ativo ? "acelerador activo" : "la CPU hace todo", v.motivo);
        return enc ? (r == 0 ? 0 : 1) : 0;
    }
    if (argc > 1 && compara_texto(argv[1], "prioridad") == 0) {   /* D424: la prioridad de la pantalla (Evergreen/NI) */
        int op = 50;
        if (argc > 2) {
            if (compara_texto(argv[2], "leer") == 0) op = 50;
            else if (compara_texto(argv[2], "alta") == 0) op = 51;
            else if (compara_texto(argv[2], "bios") == 0) op = 52;
            else if (compara_texto(argv[2], "estres") == 0) op = 53;
            else { imprimir_em(ERROS, "uso: acel2d prioridad [leer | alta | bios | estres]\n"); return 1; }
        }
        int r = info_sistema(INFO_ACEL2D_CTL, (void *)(long)op, 0);
        imprimir("acel2d prioridad %s: %s (detalle: dmesg | grep ACEL2D)\n", argc > 2 ? argv[2] : "leer",
                 r == 0 ? "BIEN" : "no disponible (solo Radeon Evergreen/NI con el 2D en marcha)");
        return r == 0 ? 0 : 1;
    }
    if (argc > 1 && compara_texto(argv[1], "volcado") == 0) {
        int op = 0;
        if (argc > 2) {
            if (compara_texto(argv[2], "apagar") == 0) op = 2;
            else if (compara_texto(argv[2], "encender") == 0) op = 3;
            else if (compara_texto(argv[2], "medir") == 0) op = 4;
            else if (compara_texto(argv[2], "paso") == 0 && argc > 3 && argv[3][0] >= '0' && argv[3][0] <= '9' &&
                     (!argv[3][1] || (argv[3][1] >= '0' && argv[3][1] <= '9' && !argv[3][2])) &&
                     (argv[3][1] ? (argv[3][0] - '0') * 10 + argv[3][1] - '0' : argv[3][0] - '0') <= 36) {
                /* Depuración en hardware real: la puesta en marcha de la GART hasta el paso N, y deshecha (0-6);
                 * 7-17, experimentos de copia de la pantalla entera; 20-25, opciones de depuración; 26 (D387),
                 * el 2D por MMIO aunque el CP del 3D esté en marcha (solo para comparar). */
                int np = argv[3][1] ? (argv[3][0] - '0') * 10 + argv[3][1] - '0' : argv[3][0] - '0';
                int r = info_sistema(INFO_ACEL2D_CTL, (void *)(long)(10 + np), 0);
                imprimir("acel2d volcado paso %s: %s (detalle: dmesg | grep GART)\n", argv[3], r == 0 ? "BIEN" : "FALLO o no disponible");
                return r == 0 ? 0 : 1;
            }
            else { imprimir_em(ERROS, "uso: acel2d volcado [apagar | encender | medir | paso 0-36]\n"); return 1; }
        }
        int r = op ? info_sistema(INFO_ACEL2D_CTL, (void *)(long)op, 0) : 0;
        info_acel2d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_ACEL2D, &v, sizeof v);
        static const char *const est[] = { "no", "activo", "apagado (kconfig)", "apagado a mano", "fallo", "mas lento que la CPU" };
        imprimir("acel2d volcado%s%s: %s (%s)\n", op ? " " : "", op ? argv[2] : "", v.volcado_estado < 6 ? est[v.volcado_estado] : "?",
                 v.volcado_motivo);
        imprimir("acel2d volcado: GPU %u volcados (%u kpx), CPU %u pequenos; medida GPU %u us, CPU %u us%s\n", v.volcado_ops,
                 v.volcado_kpx, v.volcado_cpu_ops, v.volcado_us_gpu, v.volcado_us_cpu, v.volcado_simulado ? " (simulado)" : "");
        if (op == 4) imprimir("acel2d volcado medir: %s\n", r == 0 ? "detalle en el log del kernel (dmesg)" : "no se pudo (volcado apagado?)");
        return r == 0 ? 0 : 1;
    }
    if (argc > 1 && compara_texto(argv[1], "vuelta") == 0) {
        int op = 0;
        if (argc > 2) {
            if (compara_texto(argv[2], "apagar") == 0) op = 5;
            else if (compara_texto(argv[2], "encender") == 0) op = 6;
            else if (compara_texto(argv[2], "medir") == 0) op = 7;
            else { imprimir_em(ERROS, "uso: acel2d vuelta [apagar | encender | medir]\n"); return 1; }
        }
        int r = op ? info_sistema(INFO_ACEL2D_CTL, (void *)(long)op, 0) : 0;
        info_acel2d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_ACEL2D, &v, sizeof v);
        static const char *const est[] = { "no", "activa", "apagada (kconfig)", "apagada a mano", "fallo", "mas lenta que la CPU" };
        imprimir("acel2d vuelta%s%s: %s (%s)\n", op ? " " : "", op ? argv[2] : "", v.retorno_estado < 6 ? est[v.retorno_estado] : "?",
                 v.retorno_motivo);
        imprimir("acel2d vuelta: GPU %u cuadros (%u kpx), CPU %u; medida GPU %u us, CPU %u us%s%s\n", v.retorno_ops, v.retorno_kpx,
                 v.retorno_cpu_ops, v.retorno_us_gpu, v.retorno_us_cpu, v.retorno_mascara ? "" : "; con el alfa",
                 v.volcado_simulado ? " (simulada)" : "");
        if (op == 7) imprimir("acel2d vuelta medir: %s\n", r == 0 ? "detalle en el log del kernel (dmesg)" : "no se pudo (vuelta apagada?)");
        return r == 0 ? 0 : 1;
    }
    if (argc > 1) {
        imprimir_em(ERROS, "uso: acel2d [probar [--detalle] | apagar | encender | volcado [apagar | encender | medir | paso 0-6] |"
                           " vuelta [apagar | encender | medir] | prioridad [leer | alta | bios | estres] | energia [leer | bios | bajo | alto | auto | estado N | simular N | detalle]]\n");
        return 1;
    }
    info_acel2d_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_ACEL2D, &v, sizeof v) != 0) {
        imprimir_em(ERROS, "acel2d: el kernel no lo sabe\n");
        return 1;
    }
    imprimir("Aceleracion 2D: %s\n", v.ativo ? v.nome : "CPU");
    imprimir("  %s\n", v.motivo);
    if (v.modelo) imprimir("  tarjeta: %s; plazo de la GPU en sesion: %u ms\n",
                           v.confirmada ? "confirmada" : "por autoprueba", v.prazo_ms);
    if (v.apagado) imprimir("  apagado %s: hasta «acel2d encender» todo lo hace la CPU\n",
                            v.apagado == 2 ? "a mano" : "por el vigilante (la GPU no respondio)");
    if (v.us_autoprova)
        imprimir("  autoprueba del arranque: %u us\n", v.us_autoprova);
    if (v.us_gpu || v.us_cpu)
        imprimir("  velocidad al arrancar: GPU %u us, CPU %u us\n", v.us_gpu, v.us_cpu);
    if (v.modelo) imprimir("  tarjeta %04x:%04x\n", v.fabricante, v.modelo);
    imprimir("  copias %u, rellenos %u por la GPU, fallos %u\n", v.copias, v.rellenos, v.falhas);
    /* D346: el reparto GPU / software, de solo lectura (sin ser administrador) */
    imprimir("  GPU: copias %u (%u kpx), rellenos %u (%u kpx)\n", v.copias, v.kpx_gpu_copias, v.rellenos, v.kpx_gpu_rellenos);
    imprimir("  software: mover %u (%u kpx), consola %u (%u kpx), rellenos %u (%u kpx), volcados %u (%u kpx)\n",
             v.sw_ops[0], v.sw_kpx[0], v.sw_ops[1], v.sw_kpx[1], v.sw_ops[2], v.sw_kpx[2], v.sw_ops[3], v.sw_kpx[3]);
    /* D387 */
    imprimir("  volcado por la GPU: %s; %u volcados (%u kpx)\n", v.volcado_estado == 1 ? "activo" : "no", v.volcado_ops, v.volcado_kpx);
    imprimir("    %s\n", v.volcado_motivo);
    /* D391 */
    imprimir("  vuelta del 3D por la GPU: %s; %u cuadros (%u kpx), %u por la CPU\n", v.retorno_estado == 1 ? "activa" : "no",
             v.retorno_ops, v.retorno_kpx, v.retorno_cpu_ops);
    imprimir("    %s\n", v.retorno_motivo);
    if (v.quadros)
        imprimir("  arrastre: %u ventanas, %u cuadros, %u kciclos por cuadro\n", v.arrastos, v.quadros, v.kciclos_quadro);
    return 0;
}
