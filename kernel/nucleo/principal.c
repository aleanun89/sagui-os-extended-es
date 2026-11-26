// SPDX-License-Identifier: GPL-3.0-only
/* principal.c - punto de entrada en C del kernel del SaguiOS.
 *
 * Comprueba que el cargador hizo su trabajo (firma y boot_info),
 * muestra el saludo y la memoria, y enciende las partes del kernel en orden:
 * interrupciones y reloj, memoria física y asignador, teclado, sistema de
 * archivos y, por último, la terminal. Todo cabe en pocas líneas de pantalla: en
 * el G470 el mapa de memoria grande empujaba el saludo hacia afuera (error 1). */
#include "rastro.h"
#include "kconf.h"
#include "bootinfo.h"
#include "acel2d.h"
#include "gpu3d.h"
#include "virgl.h"
#include "virtio_gpu.h"
#include "gpu_energia.h"
#include "console.h"
#include "texto.h"
#include "cpu.h"
#include "relogio.h"
#include "audio.h"
#include "autoteste.h"
#include "memoria.h"
#include "teclado.h"
#include "fpu.h"
#include "sgfs.h"
#include "paginacao.h"
#include "memalta.h"
#include "processo.h"
#include "rapido.h"
#include "sistema.h"
#include "vfs.h"
#include "mouse.h"
#include "i8042.h"
#include "gui.h"
#include "telas.h"
#include "usb.h"
#include "mandos.h"
#include "xhci.h"
#include "info_hardware.h"
#include "montagem.h"
#include "raiz.h"
#include "ata.h"
#include "cache_disco.h"
#include "atapi.h"
#include "rede.h"
#include "wc.h"
#include "modos_video.h"
#include "aleatorio.h"
#include "msr.h"
#include "versao.h"
#include "frequencia.h"
#include "eist_antiguo.h"
#include "acpi.h"
#include "bateria.h"
#include "smp.h"
#include "trava_geral.h"
#include "isa.h"
#include "ioapic.h"
#include "irq_rotas.h"
#include "cpu_dados.h"
#include "planificador.h"
#include "bloqueo.h"
#include "supervisor.h"
#include "permisos.h"
#include "sessao.h"
#include "conf_sistema.h"
#include "troca.h"

/* El nombre y la versión vienen de versao.h (D101). */

info_boot_t info_boot;
modo_vbe_t modos_vbe[MODOS_MAX];
uint8_t cabecalho_boot[512];
const char *sagui_versao = SAGUI_VERSAO;
uint32_t imagem_bytes;   /* tamaño útil de la imagen, leído del encabezado de boot (sectores x 512) */
regiao_e820_t mapa_memoria[MAPA_MAX];

static const char *nome_tipo_e820(uint32_t t)
{
    switch (t) {
    case 1: return "livre";
    case 2: return "reservada";
    case 3: return "ACPI (recuperavel)";
    case 4: return "ACPI NVS";
    case 5: return "defeituosa";
    default: return "outra";
    }
}

static void parar_com_erro(const char *msg)
{
    console_cor(COR_BRANCO, COR_VERMELHO);
    kprintf("\n " SAGUI_NOME ": %s \n", msg);
    for (;;)
        __asm__ volatile("cli; hlt");
}

/* Suma la RAM libre (tipo 1) del mapa E820. Las entradas con el atributo extendido
 * presente pero con el bit 0 apagado deben ignorarse (ACPI 3.0).
 * Con abaixo_de_4g, cuenta sólo la parte que un kernel de 32 bits sin PAE
 * consigue direccionar. */
static uint64_t memoria_livre(bool abaixo_de_4g)
{
    const uint64_t limite = 0x100000000ull;
    uint64_t total = 0;
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != E820_LIVRE || !(r->extra & 1))
            continue;
        uint64_t ini = r->base, fim = r->base + r->tamanho;
        if (abaixo_de_4g) {
            if (ini >= limite) continue;
            if (fim > limite) fim = limite;
        }
        total += fim - ini;
    }
    return total;
}

/* El mapa completo va sólo a la serial. En el G470 tiene ~20 regiones, y
 * mostrarlo en la pantalla de 25 líneas empujaba el saludo hacia afuera (error 1). */
static void registrar_mapa(void)
{
    klog("Mapa de memoria (E820), %u regioes:\n", info_boot.mapa_qtd);
    for (uint32_t i = 0; i < info_boot.mapa_qtd; i++) {
        regiao_e820_t *r = &mapa_memoria[i];
        uint64_t fim = r->base + r->tamanho - 1;
        klog("  %08x%08x - %08x%08x  %s\n",
             (uint32_t)(r->base >> 32), (uint32_t)r->base,
             (uint32_t)(fim >> 32), (uint32_t)fim, nome_tipo_e820(r->tipo));
    }
}

/* Comprueba el encabezado del SGFS (el lector completo viene en la H08). */
static void conferir_ramdisk(void)
{
    const uint8_t *rd = (const uint8_t *)info_boot.ramdisk_inicio;
    if (info_boot.ramdisk_bytes < 32 || memcmp(rd, "SGFS", 4) != 0) {
        kprintf("Ramdisk: NO encontrado en %p\n", rd);
        return;
    }
    uint32_t entradas = *(const uint32_t *)(rd + 8);
    klog("Ramdisk SGFS: %u bytes em %p, %u entradas\n", info_boot.ramdisk_bytes, rd, entradas);
}

/* Las imágenes de prueba pueden pedir un programa inicial con argumentos, ej.:
 * "/bin/ola uno dos". El kernel lo ejecuta y espera a que termine. */
static void rodar_programa_de_teste(char *linha)
{
    char *argv[16];
    int argc = 0;
    for (char *p = linha; *p && argc < 15;) {
        while (*p == ' ') *p++ = 0;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }
    argv[argc] = NULL;
    int pid = processo_executar(argv[0], argc, argv, NULL);
    klog("[TESTE] programa %s iniciado: %d\n", argv[0], pid);
    if (pid < 0) return;
    while (processo_por_pid(pid))
        cpu_espera();
    klog("[TESTE] programa terminou\n");
}

/* Una línea de estado del arranque: "[ ok ] texto". */
/* D112: aplica /home/.config/sagui.conf (teclado, volumen, fondo, acento). Es un programa común
 * ("configuracion --aplicar"): sin archivo no hace nada. Se espera a que termine para que el
 * primer programa ya vea el teclado y el escritorio elegidos. */
static void aplicar_configuracao(bool sessao)
{
    /* D323: la aceleración (2D, 3D, cabecera del disco) la cambia solo root: con la sesión como sagui, el programa
     * arranca como root, lo aplica y baja él mismo a la identidad de la sesión (le pasa su uid y gid) antes de
     * aplicar lo demás, que sigue corriendo con los mismos permisos de siempre.
     * D340: al empezar (o cerrar) una sesión después del arranque, solo lo de la cuenta («--sesion»: ni la
     * aceleración ni los conectores), lanzado ya con la identidad y la carpeta de la sesión. */
    char suid[12], sgid[12];
    snformatar(suid, sizeof suid, "%u", permisos_uid_inicial());
    snformatar(sgid, sizeof sgid, "%u", permisos_sessao_gid());
    char *argv[] = { "configuracion", "--aplicar", suid, sgid, sessao ? "--sesion" : NULL, NULL };
    int pid = sessao ? processo_executar("/bin/configuracion", 5, argv, NULL)
            : permisos_uid_inicial() == UID_ROOT ? processo_executar("/bin/configuracion", 2, argv, NULL)
                                                 : processo_executar_como("/bin/configuracion", 4, argv, UID_ROOT, GID_ROOT);
    klog("[SAGUI] configuracion --aplicar: %d\n", pid);
    if (pid < 0) return;
    uint32_t limite = relogio_tiques() + 15 * TIQUES_POR_SEGUNDO;
    while (processo_por_pid(pid) && (int32_t)(relogio_tiques() - limite) < 0)
        cpu_espera();
}

static void abrir_terminal(void)
{
    char *argv_t[] = { "terminal", NULL };
    int pid = processo_executar("/bin/terminal", 1, argv_t, NULL);
    klog("[SAGUI] terminal: %d\n", pid);
}

/* D365: los widgets de escritorio que la persona dejó activos (widgets.conf de su cuenta): el programa
 * «widgets --arrancar» los lanza y termina. */
static void abrir_widgets(void)
{
    char *argv_w[] = { "widgets", "--arrancar", NULL };
    int pid = processo_executar("/bin/widgets", 2, argv_w, NULL);
    klog("[SAGUI] widgets: %d\n", pid);
}

static void etapa(const char *texto)
{
    console_cor(COR_VERDE_CLARO, COR_PRETO);
    kprintf("[ ok ] ");
    console_cor(COR_CINZA, COR_PRETO);
    kprintf("%s\n", texto);
}

void inicio_kernel(uint32_t magico, info_boot_t *bi)
{
    arranque_marca("kernel");   /* D201: lo primero, antes de que pase el tiempo */
    serial_iniciar();
    console_iniciar();
    klog("\n[SAGUI] kernel iniciado\n");
    klog("[SAGUI] compilacion: " SAGUI_VARIANTE "\n");
#ifdef SAGUI_SEM_GPL
    klog("[SAGUI] sin GPL: r8169 solo con el inicio clasico, sensores sin tablas por modelo\n");
#endif

    if (magico != BOOT_MAGICO || bi->assinatura != BOOTINFO_ASSIN)
        parar_com_erro("no me ha cargado la etapa 2 de " SAGUI_NOME);

    /* La boot_info y el mapa están en la memoria baja, que más tarde se convierte en área
     * libre. Copiamos hacia dentro del kernel antes de cualquier otra cosa. */
    info_boot = *bi;
    /* D264: el cargador UEFI deja la cabecera en su zona y lo dice en cab_end; la etapa 2
     * por BIOS deja cab_end a cero y la cabecera en 0x7E00, como siempre. */
    if (info_boot.versao < 2) { info_boot.firmware = FIRMWARE_BIOS; info_boot.cab_end = 0; info_boot.rsdp = 0; info_boot.smbios = 0; }
    const uint8_t *cab = (const uint8_t *)CABECALHO_BOOT_END(&info_boot);
    imagem_bytes = *(const uint32_t *)(cab + 48) * 512;   /* CAB_IMG_SETORES, todavía en la memoria baja */
    /* D181: más las particiones del sistema (la de juegos), CAB_SIS_SETORES. Una imagen
     * anterior tiene ahí un cero (la cabecera se rellena con ceros). */
    uint32_t sis = *(const uint32_t *)(cab + 136);
    if (sis < 0x01000000u) imagem_bytes += sis * 512;
    info_boot.teste[63] = 0;
    if (info_boot.mapa_qtd > MAPA_MAX)
        info_boot.mapa_qtd = MAPA_MAX;
    memcpy(mapa_memoria, (const void *)info_boot.mapa_end, info_boot.mapa_qtd * sizeof(regiao_e820_t));
    if (info_boot.modos_qtd > MODOS_MAX || !info_boot.modos_end)
        info_boot.modos_qtd = 0;
    memcpy(modos_vbe, (const void *)info_boot.modos_end, info_boot.modos_qtd * sizeof(modo_vbe_t));
    memcpy(cabecalho_boot, cab, sizeof cabecalho_boot);
    kconf_iniciar();      /* D353: lo que guardó kconfig, encima de la imagen (no en el modo rescate) */
    hw_capturar_boot();   /* VBE y SMBIOS para el infopc (D041), con toda la memoria física a la vista */
    cpu_lapic_capturar();   /* D157: los APIC locales de la MADT, antes de la paginación */
    ioapic_capturar();      /* D166: los IOAPIC y las overrides de la MADT, también antes */

    /* H05, H06, H10: interrupciones, reloj, memoria y paginación. Van antes
     * de cualquier texto porque la terminal gráfica (H18) necesita el kmalloc y
     * la ventana del kernel para el framebuffer. Hasta entonces, sólo la serial registra. */
    gdt_iniciar();
    idt_iniciar();
    msr_iniciar();       /* un #GP de un rdmsr del monitor de hardware no derriba el kernel (D092) */
    pic_iniciar();
    relogio_iniciar();
    hora_iniciar();      /* D080: UTC a partir del RTC; la zona viene después del fstab */
    int_liga();
    hw_medir_tsc();       /* 100 ms: TSC contra el PIT (D041) */
    paginacao_escolher_modo();   /* D349: PAE o no y NX, antes de las zonas de memoria (la superior solo con PAE) */
    memfisica_iniciar();
    isa_dma_reservar();   /* D302: 128 KB bajo 16 MB para el DMA ISA, antes de que se gaste la memoria baja */
    paginacao_iniciar();
    memalta_iniciar();    /* D348: ventanas temporales para las páginas altas de los programas */
    troca_iniciar();      /* D352: swap comprimida en RAM (tope, reserva baja; la tarea, más tarde) */
    wc_iniciar();         /* PAT con una entrada write-combining, antes de mapear el framebuffer (D057) */

    /* H17/H18: terminal gráfica con la fuente propia. Si no hay modo
     * gráfico, la consola sigue en modo texto y avisa. */
    bool grafico = console_grafico();
    if (grafico) modos_iniciar();   /* lista de modos y cambio en caliente en el log (D058, D059) */

    int largura = console_colunas();
    console_cor(COR_AMARELO, COR_VERDE);
    kprintf("%*s", largura, "");
    kprintf("  %-*s%20s  ", largura - 24, "¡Hola! Soy " SAGUI_NOME ".", "versión " SAGUI_VERSAO);
    kprintf("%*s", largura, "");
    console_cor(COR_CINZA, COR_PRETO);

    uint64_t livre = memoria_livre(false);
    uint64_t usavel = memoria_livre(true);
    console_cor(COR_BRANCO, COR_PRETO);
    kprintf("Memoria detectada: %u MB libres", (uint32_t)(livre >> 20));
    console_cor(COR_CINZA, COR_PRETO);
    kprintf(" | usable (por debajo de 4 GB): %u MB\n", (uint32_t)(usavel >> 20));
    registrar_mapa();
    static const char *metodos_a20[] = { "ya activa", "BIOS", "puerto 0x92", "teclado" };
    kprintf("E820: %u regiones | A20: %s | disco 0x%x | VESA: ", info_boot.mapa_qtd,
            info_boot.a20_metodo < 4 ? metodos_a20[info_boot.a20_metodo] : "?", info_boot.disco);
    if (info_boot.vesa_ok)
        kprintf("%ux%ux%u\n", info_boot.vesa_largura, info_boot.vesa_altura, info_boot.vesa_bpp);
    else
        kprintf("ninguno\n");
    klog("[SAGUI] memoria_mb=%u usavel_mb=%u\n", (uint32_t)(livre >> 20), (uint32_t)(usavel >> 20));
    kconf_registrar();    /* D353 */
    /* D077: desde el CD (El Torito, la imagen emulada como disco 0x80) el disco de
     * arranque es sólo lectura; y sin las extensiones del INT 13h el cargador leyó
     * por CHS. Las dos cosas ayudan a entender un informe (infopc --informe). */
    if (info_boot.opcoes & OPC_BOOT_CD)
        kprintf("Arranque desde CD (El Torito): el disco de arranque es de solo lectura.\n");
    if (info_boot.firmware != FIRMWARE_BIOS) {
        kprintf("Arranque UEFI (%s): video GOP %ux%u, sin servicios de la BIOS.\n",
                info_boot.firmware == FIRMWARE_UEFI_X64 ? "x64" : "IA32", info_boot.vesa_largura, info_boot.vesa_altura);
        klog("[BOOT] firmware=uefi-%s gop=%ux%u pitch=%u fb=%x rsdp=%x smbios=%x modos=%u\n",
             info_boot.firmware == FIRMWARE_UEFI_X64 ? "x64" : "ia32", info_boot.vesa_largura, info_boot.vesa_altura,
             info_boot.vesa_pitch, info_boot.vesa_fb, info_boot.rsdp, info_boot.smbios, info_boot.modos_qtd);
    } else {
        klog("[BOOT] firmware=bios\n");
    }
    klog("[BOOT] cd=%u leitura_chs=%u bpp_pref=%u\n", (info_boot.opcoes & OPC_BOOT_CD) ? 1 : 0,
         info_boot.leitura_chs, info_boot.bpp_pref);
    if (info_boot.opcoes & OPC_BOOT_KERNEL_PRUEBA)   /* D392: lo arrancó la etapa 2 desde la ranura de prueba */
        klog("[BOOT] kernel de prueba (arranque probar, D392): %s; %u bytes\n",
             (info_boot.opcoes & OPC_BOOT_KERNEL_FIXO) ? "el ultimo bueno (confirmado)" : "esta vez, el siguiente arranque es el ultimo bueno",
             info_boot.kernel_bytes);

    etapa("Interrupciones: GDT e IDT propias, PIC reasignado, reloj a 100 Hz");
    {
        char linha[112];
        if (memfisica_geridas_alta())   /* D348 */
            snformatar(linha, sizeof linha, "Memoria física: %u MB gestionados (%u MB altos, para programas), kmalloc listo",
                       (memfisica_livres() + memfisica_livres_alta()) / 256, memfisica_livres_alta() / 256);
        else
            snformatar(linha, sizeof linha, "Memoria física: %u MB gestionados en páginas de 4 KB, kmalloc listo",
                       memfisica_livres() / 256);
        etapa(linha);
    }
    {
        char linha[96];
        snformatar(linha, sizeof linha, "Paginación%s%s: 1:1 hasta %u MB%s, un espacio por proceso",
                   paginacao_com_pae ? " PAE" : "", paginacao_nx() ? " con NX" : "", paginacao_fim_mapa() >> 20,
                   !paginacao_pse() ? "" : paginacao_com_pae ? " en páginas de 2 MB" : " en páginas de 4 MB");
        etapa(linha);
    }
    rastro_iniciar();   /* depuración: las marcas del arranque anterior (si hubo reinicio en caliente) y empieza */
    fpu_iniciar();
    aleatorio_iniciar();   /* después del TSC medido y antes de la red (ISN del TCP, D061) */
    rapido_iniciar();   /* rep movsd o SSE2 para los gráficos: mide y elige (D042) */
    /* D195: 2D por la GPU (copiar y rellenar rectángulos): elige el acelerador por
     * el PCI, lo pone a prueba contra la CPU y solo entonces lo usa. Después del
     * rapido (la CPU de referencia) y de la paginación (el MMIO). */
    if (grafico) {
        acel2d_iniciar();
        char linha[128];
        snformatar(linha, sizeof linha, "Aceleración 2D: %s (%s)", acel2d_ativo() ? acel2d_nome() : "CPU", acel2d_motivo());
        etapa(linha);
        acel2d_volcado_iniciar();   /* D387: el volcado del búfer de atrás por la GPU (GART), si el 2D lo sabe hacer */
    }
    /* D285: 3D por la GPU: aquí solo se mira si hay driver para la tarjeta; se
     * enciende y se prueba con el primer uso (un fallo no retrasa el arranque). */
    gpu3d_iniciar();
    /* D473: VirGL (3D de la virtio-gpu): autoprueba del triángulo, solo si el dispositivo lo ofrece. */
    if (grafico && vg_virgl_ofrecido()) virgl_autoprueba();
    /* D399: energía de la GPU: solo lee las tablas de datos de la BIOS de vídeo (nada se ejecuta ni se
     * cambia en una tarjeta sin confirmar); kconfig gpu_energia lo aplica unos segundos después. */
    gpu_energia_iniciar();
    {
        char linha[96], rec[64];
        fpu_descrever(rec, sizeof rec);
        snformatar(linha, sizeof linha, "CPU %s: %s", cpu_fabricante(), rec[0] ? rec : "sin FPU");
        etapa(linha);
    }
    /* D166: las IRQ por el IOAPIC, solo con la opción de arranque
     * (mkimagem.py --ioapic). Necesita la paginación (el MMIO) y el TSC (para
     * comprobar que el tic llega; si no, vuelve al PIC). Va antes de los
     * drivers, que habilitan su IRQ en el controlador que esté activo, y
     * antes del SMP. */
    if (info_boot.opcoes & OPC_BOOT_IOAPIC) {
        if (ioapic_ativar())
            etapa("Interrupciones: IOAPIC activo, el 8259 enmascarado");
    } else {
        klog("[IOAPIC] desactivado (opcion de arranque --ioapic para activarlo)\n");
    }
    /* D157/D164: los procesadores secundarios. Necesitan la paginación (sus
     * tablas son las del kernel), el reloj (la espera entre el INIT y la SIPI)
     * y la FPU ya preparada en el principal (copian su CR0 y su CR4, y usan sus
     * recursos de CPUID). Solo con la opción de arranque (mkimagem.py --smp):
     * sin ella, un procesador, como antes, porque el kernel aún no tiene un
     * cerrojo general para varias CPU a la vez. */
    uint32_t nucleos = 1;
    cpu_topologia_ler(0);   /* D174: núcleo físico e hilo de la CPU 0 (cada secundario lee el suyo) */
    if (info_boot.opcoes & OPC_BOOT_SMP)
        nucleos = smp_iniciar();
    else
        klog("[SAGUI] SMP desactivado (opcion de arranque --smp para activarlo)\n");
    klog("[SAGUI] nucleos=%u\n", nucleos);
    /* D174: la topología (hilos hermanos de un mismo núcleo físico). */
    for (uint32_t c = 0; c < nucleos; c++)
        klog("[SAGUI] topologia (%s): CPU %u nucleo %u hilo %u paquete %u\n", cpu_topologia_metodo(), c,
             cpu_nucleo_fisico(c), cpu_hilo_do_nucleo(c), cpu_pacote(c));
    /* D174: el planificador elegido al arrancar (mkimagem.py --planificador). */
    planificador_iniciar((info_boot.opcoes & OPC_BOOT_PLANIFICADOR) >> OPC_BOOT_PLANIFICADOR_DESL);
    /* D169: con varias CPU, el cerrojo general por turnos o sin cola. */
    if (nucleos > 1)
        trava_geral_escolher((info_boot.opcoes & OPC_BOOT_TURNOS) != 0);
    /* D168: con varias CPU y el IOAPIC, las IRQ de los dispositivos se
     * reparten entre los secundarios (el cerrojo general, D165, deja que
     * cualquier CPU las atienda). */
    if (nucleos > 1 && ioapic_ativo())
        ioapic_repartir();
    if (grafico) {
        char linha[96];
        snformatar(linha, sizeof linha, "Vídeo: %ux%u, %u bits, fuente propia 8x16 con acentos",
                   info_boot.vesa_largura, info_boot.vesa_altura, info_boot.vesa_bpp);
        etapa(linha);
    } else {
        console_cor(COR_AMARELO, COR_PRETO);
        kprintf("[aviso] Modo gráfico no disponible: el sistema sigue en modo texto.\n");
        console_cor(COR_CINZA, COR_PRETO);
        klog("[SAGUI] sem modo grafico, modo texto\n");
    }

    /* H11: a partir de aquí el reloj cambia de proceso (round-robin). */
    processos_iniciar();
    sistema_iniciar();
    permisos_iniciar();   /* D255: la identidad de la sesión (sagui, o root en el modo rescate) */
    etapa("Procesos: round-robin cada 20 ms, programas en el anillo 3, int 0x80");
    arranque_marca("cpu+procesos");

    /* H07: teclado. */
    teclado_iniciar();
    etapa("Teclado PS/2: distribución ES y ABNT2 (acentos con teclas muertas, ñ, ç)");

    /* H19: ratón / touchpad en el puerto auxiliar del mismo controlador. */
    if (mouse_iniciar())
        etapa("Ratón/touchpad PS/2: cursor dibujado por " SAGUI_NOME ", IRQ 12");
    else {
        /* Los números ayudan a entender el hardware a partir de una foto:
         * configuración del controlador y respuestas del touchpad (FA = ok). */
        const i8042_diag_t *d = i8042_diagnostico();
        console_cor(COR_AMARELO, COR_PRETO);
        kprintf("[aviso] El ratón/touchpad PS/2 no responde (cfg %x->%x, F6=%x FF=%x F4=%x). El teclado sigue normal.\n",
                (uint32_t)d->config_lida & 0xFFF, (uint32_t)d->config_final, (uint32_t)d->resposta_padrao & 0xFFF,
                (uint32_t)d->resposta_reset & 0xFFF, (uint32_t)d->resposta_ligar & 0xFFF);
        console_cor(COR_CINZA, COR_PRETO);
    }

    /* H08: sistema de archivos del ramdisk. */
    conferir_ramdisk();
    if (sgfs_iniciar((const void *)info_boot.ramdisk_inicio, info_boot.ramdisk_bytes)) {
        char linha[96];
        vfs_iniciar();
        snformatar(linha, sizeof linha, "Archivos: SGFS con %u entradas (%u KB) + capa escribible en RAM",
                   sgfs_quantidade(), (info_boot.ramdisk_bytes + 1023) / 1024);
        etapa(linha);
    } else {
        console_cor(COR_VERMELHO_CLARO, COR_PRETO);
        kprintf("[ERROR] ramdisk SGFS no válido: el sistema sigue sin archivos\n");
        console_cor(COR_CINZA, COR_PRETO);
    }
    arranque_marca("ramdisk");
    /* H29-H31: USB 2.0 (EHCI), hubs y pendrives de datos. */
    usb_iniciar();
    arranque_marca("usb");
    mandos_iniciar();   /* D307: el puerto de juegos (y los mandos de prueba); los USB llegan por hid.c */
    {
        char linha[96];
        int n = ehci_quantidade(), nx = xhci_quantidade(), nu = uhci_quantidade(), no = ohci_quantidade();
        if (!n && !nx && (nu || no)) {
            snformatar(linha, sizeof linha, "USB 1.1: %d UHCI y %d OHCI; pendrives en /usb, teclado y ratón USB", nu, no);
            etapa(linha);
        } else if (n || nx) {
            if (nx)
                snformatar(linha, sizeof linha, "USB: %d xHCI (USB 3.0) y %d EHCI; pendrives en /usb, teclado y ratón USB", nx, n);
            else
                snformatar(linha, sizeof linha, "USB: %d controlador(es) EHCI, pendrives en /usb (comandos discos, montar)", n);
            etapa(linha);
        } else {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("[aviso] No se ha encontrado ningún controlador USB (ni xHCI ni EHCI).\n");
            console_cor(COR_CINZA, COR_PRETO);
        }
    }
    /* D040: discos internos ATA (IDE), sólo para lectura salvo "mount -o rw".
     * D155: también los SATA en modo AHCI. */
    {
        int n = ata_iniciar();
        arranque_marca("discos");
        char linha[96];
        snformatar(linha, sizeof linha, "Discos internos ATA: %d (IDE y SATA; solo lectura salvo \"mount -o rw\"; comandos discos, mount)", n);
        etapa(linha);
        if (ata_ahci_sem_suporte()) {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("[aviso] Hay un controlador SATA AHCI que el driver no ha podido usar (mira el log del kernel).\n");
            console_cor(COR_CINZA, COR_PRETO);
        }
        /* D207: lectores de CD/DVD; si se arrancó de un CD, ese CD queda en /cd. */
        bool cd_montado = atapi_montar_cd_arranque();
        atapi_iniciar_vigilancia();   /* D233: el disco que se mete o se saca con el sistema en marcha */
        if (atapi_quantidade()) {
            snformatar(linha, sizeof linha, "Lectores de CD/DVD: %d (%s)", atapi_quantidade(),
                       cd_montado ? "el CD de arranque en /cd" : "solo lectura; comandos discos, mount");
            etapa(linha);
        }
    }
    /* D379: si el ramdisk es un initramfs con /etc/raiz.conf, la raíz pasa a ser la
     * partición del disco que dice (o, si no se puede, se sigue en RAM: modo rescate).
     * Sin ese fichero (las imágenes de siempre) no hace nada. */
    if (raiz_montar()) {
        etapa("Raíz del sistema en el disco (initramfs mínimo en RAM)");
        arranque_marca("raiz");
    }
    /* D040: lo que el /etc/fstab manda montar (por ejemplo, la partición de
     * datos del disco de arranque en /home). De aquí en adelante, un disco conectado
     * en caliente también consulta el fstab. */
    {
        bool i0 = int_salva_desliga();
        fstab_liberar();
        raiz_pos_fstab();   /* D379: /tmp en la RAM e /initrd, con la raíz en disco */
        /* D255: la configuración del sistema de SAGUI-CONF sobre /etc. D379: con la raíz en
         * disco, /etc ya es persistente y esta copia sobre el árbol en RAM no se vería. */
        if (raiz_estado() != RAIZ_DISCO && raiz_estado() != RAIZ_DISCO_SO_LEITURA)
            conf_sistema_superpor();
        conf_sistema_raiz_preparar();   /* D371: /root, en SAGUI-CONF si está (D379: o en la raíz en disco) */
        conf_sistema_home_reavaliar();  /* D371: /home de root si sagui ya está en /home/sagui */
        hora_carregar_config();   /* D080: /home/.zona (persistente) o /etc/zona */
        int_restaura(i0);
    }
    /* D218: el bloqueo de publicidad por DNS, si se activó (lee /home/.config/bloqueo). */
    bloqueo_iniciar();
    /* D106: driver de frecuencia de la CPU (después del fstab: lee
     * /home/.frecuencia). Solo una línea si hay control. */
    frequencia_iniciar();
    {
        info_frequencia_t f;
        if (frequencia_info_kernel(&f) && f.estado == FREQ_EST_ATIVO) {
            char linha[96];
            snformatar(linha, sizeof linha, "Frecuencia de la CPU: %s, límite %d °C", f.mecanismo_txt, f.limite_mc / 1000);
            etapa(linha);
        }
    }
    /* D177: EIST de los Intel antiguos (Pentium M a Core 2), solo lectura:
     * qué FID/VID tiene la CPU y si el _PSS de la BIOS es fiable. */
    eist_antiguo_iniciar();
    {
        eist_antiguo_t ea;
        if (eist_antiguo_info(&ea) && ea.estado == EIST_ANT_DETECTADO) {
            char linha[128];
            snformatar(linha, sizeof linha, "EIST antiguo (%s): %s; sin cambios (solo lectura)", ea.nome, ea.motivo);
            etapa(linha);
        }
    }
    /* D046: placa de red y DHCP. El arranque espera como máximo 2 s por la
     * dirección (en QEMU tarda milisegundos); después el DHCP sigue solo. */
    rede_iniciar();
    arranque_marca("red");
    {
        placa_t *pl = rede_placa();
        uint32_t limite = relogio_tiques() + 2 * TIQUES_POR_SEGUNDO;
        while (pl && pl->cabo() && !rede_dhcp_terminou() && (int32_t)(relogio_tiques() - limite) < 0)
            cpu_espera();
        char linha[96];
        bool aviso;
        rede_linha_boot(linha, sizeof linha, &aviso);
        if (linha[0] && !aviso) etapa(linha);
        else if (linha[0]) {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("[aviso] %s\n", linha);
            console_cor(COR_CINZA, COR_PRETO);
        }
    }
    /* D088: placa de sonido (HDA o AC'97). Sin placa, ninguna línea. */
    audio_iniciar();
    arranque_marca("sonido");
    {
        char linha[96];
        bool aviso;
        audio_linha_boot(linha, sizeof linha, &aviso);
        if (linha[0] && !aviso) etapa(linha);
        else if (linha[0]) {
            console_cor(COR_AMARELO, COR_PRETO);
            kprintf("[aviso] %s\n", linha);
            console_cor(COR_CINZA, COR_PRETO);
        }
    }
    /* D113: apagado por ACPI (S5) y botón de encendido por el SCI. Después de
     * la red (D113). */
    acpi_iniciar();
    arranque_marca("acpi");
    /* D192: el intérprete de AML (uACPI) para la batería, en su propia tarea
     * y un segundo después: nunca retrasa ni tumba el arranque. Solo si
     * la imagen lleva --uacpi (desactivado por defecto) y acpi.c dejó el
     * sistema en modo ACPI. */
    uacpi_sagui_iniciar();
    klog("[SAGUI] boot ok\n");
    /* D095: de aquí en adelante hay planificador e IRQ: la E/S del pendrive duerme
     * esperando el DMA en vez de girar con el sistema detenido. Antes (y en el
     * pánico) la espera sigue siendo síncrona. */
    ehci_modo_assincrono(true);
    ata_modo_assincrono(true);    /* D281: el DMA del IDE duerme esperando su IRQ */
    cache_disco_tarefa_iniciar(); /* D281: vaciado periódico de la escritura diferida */
    troca_tarefa_iniciar();       /* D352: mantiene memoria libre sacando páginas a la swap */
    xhci_modo_assincrono(true);   /* D214: la IRQ del xHCI, con su sonda para la autoprueba */
    /* D211: con el IOAPIC, cada driver que sabe provocar una interrupción de
     * su dispositivo comprueba que llega; si no, esa línea vuelve al 8259. */
    irq_rotas_arranque();

    /* D255: el modo rescate es la consola de texto con un shell de root y sin
     * servicios: lo mínimo que tiene que funcionar para arreglar el sistema. */
    bool sem_gui = (info_boot.opcoes & (OPC_BOOT_CONSOLE | OPC_BOOT_RESCATE)) != 0;
    bool vai_ter_gui = grafico && !sem_gui;
    if (info_boot.teste[0] == '/' && !vai_ter_gui)
        rodar_programa_de_teste(info_boot.teste);
    else if (info_boot.teste[0] && info_boot.teste[0] != '/')
        autoteste_executar(info_boot.teste);

    /* H20: con video y sin la opción --console, el escritorio toma la
     * pantalla; el shell pasa a ejecutarse dentro del programa Terminal (H24). */
    if (grafico && !sem_gui && gui_iniciar()) {
        console_esconder(true);
        arranque_marca("escritorio");
        klog("[SAGUI] area de trabalho no ar\n");
        telas_iniciar();   /* D290: otras salidas de vídeo (varias pantallas), si las hay */
        aplicar_configuracao(false);
        arranque_marca("configuracion");
        if (info_boot.teste[0] == '/') {
            rodar_programa_de_teste(info_boot.teste);   /* pruebas de la fase 5: programa gráfico */
        } else if (sessao_arrancar()) {   /* D340: una sola cuenta sin contraseña: directo, como siempre */
            abrir_terminal();
            abrir_widgets();
        }
        /* D216: el gestor de servicios, después del Terminal (no lo retrasa) y
         * sin esperar a nada: si falta o falla, se anota y se sigue. D340: con la
         * pantalla de inicio de sesión, antes de que alguien entre (comprueba las contraseñas). */
        supervisor_lancar();
        for (;;) {
            cpu_espera();   /* el proceso 0 sólo descansa; el resto son procesos y la tarea de las ventanas */
            supervisor_vigiar();   /* D216: lo relanza si murió (mira una vez por segundo) */
            /* D340: tras cerrar una sesión, la configuración de la máquina detrás de la pantalla; al
             * entrar, la de la cuenta y su Terminal. */
            if (sessao_tomar_restaurar()) aplicar_configuracao(true);
            if (sessao_tomar_inicio()) {
                aplicar_configuracao(true);
                abrir_terminal();
                abrir_widgets();
            }
        }
    }

    /* H16: el primer proceso es el shell, un programa común en el anillo 3. La
     * terminal provisoria de la H09 fue quitada. De aquí en adelante este código
     * es sólo el proceso 0 (ocioso). */
    aplicar_configuracao(false);
    sessao_consola_preparar();   /* D371: la carpeta de la cuenta (tras la migración de sagui, /home/sagui) */
    bool servicos_lancados = false;
    if (permisos_modo_rescate()) {
        console_cor(COR_AMARELO, COR_PRETO);
        kprintf("\n*** MODO RESCATE: shell de root sin contrasena, sin escritorio ni servicios. ***\n"
                "    passwd cambia la contrasena de root; reiniciar vuelve al arranque normal.\n"
                "    La configuracion del kernel guardada (kconfig) no se aplica; kconfig --por-defecto la borra.\n\n");
        console_cor(COR_CINZA, COR_PRETO);
        klog("[SAGUI] MODO RESCATE\n");
    }
    /* D371 (fase 6, P12): con la configuración guardada y sin el inicio automático, la consola pide la cuenta
     * («login», como root: comprueba la contraseña y pasa a la cuenta). El modo rescate y las pruebas, no. */
    bool pede_login = !permisos_modo_rescate() && !permisos_sesion_raiz() && !conf_sistema_inicio_automatico();
    for (;;) {
        char *argv[] = { pede_login ? "login" : "sagsh", NULL };
        int pid = pede_login ? processo_executar_como("/bin/login", 1, argv, UID_ROOT, GID_ROOT)
                             : processo_executar("/bin/sagsh", 1, argv, NULL);
        if (pid < 0) {
            console_cor(COR_VERMELHO_CLARO, COR_PRETO);
            kprintf("No se pudo iniciar /bin/sagsh (error %d).\n", pid);
            console_cor(COR_CINZA, COR_PRETO);
            for (;;) cpu_espera();
        }
        klog("[SAGUI] shell iniciado: processo %d\n", pid);
        /* D216: el gestor de servicios, después del primer shell (que sigue
         * siendo el primer programa, con el PID de siempre). */
        if (!servicos_lancados) {
            supervisor_lancar();
            servicos_lancados = true;
        }
        while (processo_por_pid(pid)) {
            cpu_espera();
            supervisor_vigiar();   /* D216 */
        }
        kprintf("\n[SaguiOS] Se ha cerrado el shell. Abriendo uno nuevo...\n");
    }
}
