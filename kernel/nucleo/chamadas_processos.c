// SPDX-License-Identifier: GPL-3.0-only
/* chamadas_processos.c - llamadas de los grupos "procesos" y "diversas". */
#include "radios.h"
#include "painel.h"
#include "imagens.h"
#include "irq_rotas.h"
#include "aparencia.h"
#include "acel2d.h"
#include "gpu3d.h"
#include "gpu_energia.h"
#include "ocio.h"
#include "mandos.h"
#include "audio.h"
#include "sistema.h"
#include "console.h"
#include "memoria.h"
#include "cache_disco.h"
#include "memalta.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "bootinfo.h"
#include "info_hardware.h"
#include "gui.h"
#include "montagem.h"
#include "cpu_dados.h"
#include "planificador.h"

#include "acpi.h"
#include "bateria.h"
#include "supervisor.h"
#include "troca.h"

int32_t sis_sair(uint32_t codigo, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    processo_encerrar(processo_atual(), (int)codigo, NULL);
    return 0;
}

int32_t sis_pid(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return processo_recursos()->pid;   /* D167: en un hilo, el pid del proceso (el tid lo da SIS_HILO_ID) */
}

int32_t sis_matar(uint32_t pid, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    processo_t *p = processo_por_pid((int)pid);
    if (!p || p->estado == P_ZUMBI) return ERRO_SEM_PROCESSO;
    if (!p->usuario) return ERRO_ACESSO;   /* las tareas del kernel y el ocioso no */
    processo_encerrar(p, SAIDA_ENCERRADO_PELO_KERNEL, "encerrado pelo comando matar");
    return 0;
}

/* esperar(pid o -1, &codigo, sem_bloquear): recoge un hijo que terminó.
 * Si ninguno terminó todavía, se bloquea y la llamada se repite cuando un hijo
 * termine (processo_encerrar despierta al padre); con sem_bloquear, devuelve 0. */
int32_t sis_esperar(uint32_t pid, uint32_t ptr_codigo, uint32_t sem_bloquear, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    int *codigo = (int *)ptr_codigo;
    if (codigo && !usuario_ok(codigo, sizeof(int), true)) return ERRO_ENDERECO;
    bool tem_filho = false;
    processo_t *eu = processo_recursos();   /* D167: los hijos son del proceso, no del hilo */
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado == P_LIVRE || p->lider || p->pai != eu->pid) continue;   /* los hilos no son hijos */
        if ((int)pid != -1 && p->pid != (int)pid) continue;
        tem_filho = true;
        /* D165: un hijo terminado desde otra CPU no se recoge hasta que esa
         * CPU lo deja (sigue sobre su pila); entonces despierta al padre. */
        if (p->estado == P_ZUMBI && !grupo_na_cpu(p)) {   /* D167: ni un hilo suyo en una CPU */
            int r = p->pid;
            if (codigo) *codigo = p->codigo_saida;
            processo_liberar(p);
            return r;
        }
    }
    if (!tem_filho) return ERRO_SEM_PROCESSO;
    if (sem_bloquear) return 0;   /* nadie terminó todavía; quien llamó no quiere esperar */
    bloquear(eu);   /* el hijo despierta a su padre: el proceso, aunque espere un hilo */
    return SIS_REPETIR;
}

static const char canal_relogio = 0;

int32_t sis_dormir(uint32_t ms, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    uint32_t tiques = (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    if (tiques == 0) tiques = 1;
    processo_atual()->acordar_em = relogio_tiques() + tiques;
    bloquear(&canal_relogio);
    return 0;   /* resultado ya listo: no se repite al despertar */
}

int32_t sis_tempo(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return (int32_t)(relogio_tiques() * (1000 / TIQUES_POR_SEGUNDO));
}

int32_t sis_data(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    data_t *dt = (data_t *)ptr;
    if (!usuario_ok(dt, sizeof *dt, true)) return ERRO_ENDERECO;
    /* D080: la hora local de la zona del sistema (el RTC puede estar en UTC). */
    fuso_data_t h;
    hora_local(&h);
    dt->ano = h.ano; dt->mes = h.mes; dt->dia = h.dia;
    dt->hora = h.hora; dt->minuto = h.minuto; dt->segundo = h.segundo;
    dt->dia_semana = h.dia_semana;
    return 0;
}

int32_t sis_info_sistema(uint32_t tipo, uint32_t ptr, uint32_t n, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (tipo == INFO_MEMORIA) {
        info_memoria_t *m = (info_memoria_t *)ptr;
        /* D348/D349: los campos de la memoria alta y la superior solo si el programa los pide (n). */
        bool v2 = n >= INFO_MEMORIA_V2, v3 = n >= INFO_MEMORIA_V3, v4 = n >= INFO_MEMORIA_V4, v5 = n >= sizeof *m;
        if (!usuario_ok(m, v5 ? sizeof *m : v4 ? INFO_MEMORIA_V4 : v3 ? INFO_MEMORIA_V3 : v2 ? INFO_MEMORIA_V2 : INFO_MEMORIA_V1, true))
            return ERRO_ENDERECO;
        estat_memoria_t est;
        memoria_estatisticas(&est);
        m->ram_total_kb = (uint32_t)(est.ram_total >> 10);
        m->gerida_kb = est.paginas_geridas * 4;
        m->livre_kb = est.paginas_livres * 4;
        m->usada_kb = m->gerida_kb - m->livre_kb;
        if (v2) {
            m->alta_gerida_kb = est.altas_geridas * 4;
            m->alta_livre_kb = est.altas_livres * 4;
            m->janelas_usos = memalta_usos();
            m->modo = (paginacao_com_pae ? MEM_MODO_PAE : 0) | (paginacao_nx() ? MEM_MODO_NX : 0) |
                      (paginacao_pae_cpu() ? MEM_MODO_CPU_PAE : 0) | (paginacao_nx_cpu() ? MEM_MODO_CPU_NX : 0);
        }
        if (v3) {
            m->superior_gerida_kb = est.superiores_geridas * 4;
            m->superior_livre_kb = est.superiores_livres * 4;
            cache_disco_est_t c;   /* D350 */
            cache_disco_estatisticas(&c);
            m->cache_kb = c.blocos * 4;
            m->cache_alta_kb = c.altos * 4;
        }
        if (v4) {   /* D352: la swap comprimida */
            troca_est_t t;
            troca_estatisticas(&t);
            m->troca_teto_kb = t.teto_paginas * 4;
            m->troca_kb = t.paginas * 4;
            m->troca_comprimida_kb = t.bytes / 1024;
            m->troca_armazem_kb = t.armazem_paginas * 4;
            m->troca_saidas = t.saidas;
            m->troca_entradas = t.entradas;
        }
        if (v5) {   /* D378 (fase 3): páginas compartidas de las bibliotecas */
            imagens_est_t im;
            imagens_estatisticas(&im);
            m->compartida_kb = im.paginas * 4;
            m->compartida_mapeada_kb = im.mapeadas * 4;
            m->compartida_objetos = im.objetos;
            m->compartida_reutilizadas = im.reutilizadas;
        }
        return 0;
    }
    if (tipo == INFO_PROCESSOS) {
        info_processo_t *v = (info_processo_t *)ptr;
        if (n > MAX_PROCESSOS) n = MAX_PROCESSOS;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t k = 0;
        for (int i = 0; i < MAX_PROCESSOS && k < n; i++) {
            processo_t *p = processo_indice(i);
            if (p->estado == P_LIVRE || p->lider) continue;   /* D167: un hilo cuenta en su proceso */
            v[k].pid = p->pid;
            v[k].pai = p->pai;
            v[k].estado = p->estado;
            /* D378 (fase 3): de las páginas compartidas de las bibliotecas, solo su parte (las de un
             * objeto entre los procesos que lo usan): así la suma de todos no las repite. */
            v[k].memoria_kb = (p->usuario && p->estado != P_ZUMBI ?
                               (espaco_paginas_usuario(p->espaco) - imagens_paginas_alheias(p->espaco) -
                                espaco_paginas_cow_alheias(p->espaco)) * 4 : 0)   /* D401: idem con las del fork */
                              + (p->pid ? PILHA_KERNEL / 1024 : 0);
            uint32_t tiques = p->tiques_cpu;   /* D167: el de todos sus hilos */
            for (int h = 1; h < MAX_PROCESSOS; h++) {
                processo_t *t = processo_indice(h);
                if (t->lider == p && t->estado != P_LIVRE) tiques += t->tiques_cpu;
            }
            v[k].cpu_ms = tiques * (1000 / TIQUES_POR_SEGUNDO);
            v[k].usuario = p->usuario;
            txt_copia_n(v[k].nome, p->nome, sizeof v[k].nome);
            v[k].janelas = (uint32_t)janelas_contar(p->pid);
            v[k].cpu = p->cpu;          /* a CPU onde este processo corre (D157) */
            v[k].n_hilos = p->estado == P_ZUMBI ? 0 : p->pid == 0 ? 1 : (uint32_t)hilos_contar(p);   /* D167 */
            k++;
        }
        return (int32_t)k;
    }
    if (tipo == INFO_HILOS) {   /* D366 */
        info_hilo_t *v = (info_hilo_t *)ptr;
        if (n > MAX_PROCESSOS) n = MAX_PROCESSOS;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t k = 0;
        for (int i = 1; i < MAX_PROCESSOS && k < n; i++) {
            processo_t *t = processo_indice(i);
            if (t->estado == P_LIVRE || !t->lider) continue;
            v[k].tid = t->pid;
            v[k].pid = t->lider->pid;
            v[k].estado = t->estado;
            v[k].cpu_ms = t->tiques_cpu * (1000 / TIQUES_POR_SEGUNDO);
            v[k].cpu = t->cpu;
            k++;
        }
        return (int32_t)k;
    }
    if (tipo == INFO_DISCO_CACHE) {   /* D368 */
        if (n < sizeof(info_disco_cache_t)) return ERRO_INVALIDO;
        info_disco_cache_t *d = (info_disco_cache_t *)ptr;
        if (!usuario_ok(d, sizeof *d, true)) return ERRO_ENDERECO;
        cache_disco_est_t e;
        cache_disco_estatisticas(&e);
        info_disco_cache_t r;
        memset(&r, 0, sizeof r);
        r.versao = 1;
        r.ativa = e.max_blocos != 0;
        r.blocos = e.blocos; r.max_blocos = e.max_blocos; r.sujos = e.sujos;
        r.acertos = e.acertos; r.falhas = e.falhas; r.anticipados = e.anticipados;
        r.gravados = e.gravados; r.vaciados = e.vaciados; r.diretos = e.diretos; r.erros = e.erros;
        *d = r;
        return 0;
    }
    if (tipo == INFO_PILHAS) {   /* D186 */
        info_pilha_t *v = (info_pilha_t *)ptr;
        if (n > MAX_PROCESSOS) n = MAX_PROCESSOS;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t k = 0;
        for (int i = 0; i < MAX_PROCESSOS && k < n; i++) {
            processo_t *p = processo_indice(i);
            if (p->estado == P_LIVRE || p->lider) continue;
            v[k].pid = p->pid;
            v[k].pilha_kb = p->estado == P_ZUMBI ? 0 : pilha_usada_kb(p);
            v[k].hilos_kb = 0;
            for (int h = 1; h < MAX_PROCESSOS && p->estado != P_ZUMBI; h++) {
                processo_t *t = processo_indice(h);
                if (t->lider == p && t->estado != P_LIVRE) v[k].hilos_kb += pilha_usada_kb(t);
            }
            k++;
        }
        return (int32_t)k;
    }
    if (tipo == INFO_NUCLEOS) {   /* D157: os processadores, para o Administrador de tarefas */
        info_nucleo_t *v = (info_nucleo_t *)ptr;
        if (n > CPU_MAX) n = CPU_MAX;
        if (!usuario_ok(v, n * sizeof *v, true)) return ERRO_ENDERECO;
        uint32_t quantos = cpu_numero();
        if (quantos > n) quantos = n;
        uint32_t processos = 0;
        for (int i = 0; i < MAX_PROCESSOS; i++) {
            processo_t *p = processo_indice(i);
            if (p->estado != P_LIVRE && !p->lider) processos++;
        }
        for (uint32_t i = 0; i < quantos; i++) {
            cpu_dados_t *d = cpu_dados(i);
            if (!d) break;
            v[i].indice = i;
            v[i].estado = d->estado;
            v[i].apic_id = d->apic_id;
            /* El núcleo físico, el hilo dentro de él y el paquete, por
             * CPUID (D174): dos hilos hermanos dan el mismo núcleo. */
            v[i].nucleo = cpu_nucleo_fisico(i);
            v[i].hilo = cpu_hilo_do_nucleo(i);
            v[i].paquete = cpu_pacote(i);
            v[i].planificador = planificador_ativo();
            v[i].pid = d->atual ? (uint32_t)processo_lider(d->atual)->pid : 0;   /* D167: el proceso del hilo */
            v[i].tiques = d->tiques;
            v[i].tiques_ociosos = d->tiques_ociosos;
            v[i].fatia = d->fatia;
            v[i].n_processos = processos;
        }
        return (int32_t)quantos;
    }
    if (tipo == INFO_TELA) {   /* D057-D059 */
        int32_t modos_info_tela(uint32_t ptr);
        return modos_info_tela(ptr);
    }
    if (tipo == INFO_APARENCIA) return aparencia_info(ptr, n);   /* D112 */
    if (tipo == INFO_BARRA) return barra_info(ptr, n);   /* D361 */
    if (tipo == INFO_BARRA_GRAVAR) return barra_gravar(ptr, n);   /* D361 */
    if (tipo == INFO_PAINEL) return painel_info(ptr, n);           /* D407 */
    if (tipo == INFO_PAINEL_GRAVAR) return painel_gravar(ptr, n);  /* D407 */
    if (tipo == INFO_JANELAS) {   /* D043: el administrador de tareas lista las ventanas */
        if (n > 64) n = 64;
        if (!usuario_ok((void *)ptr, n * sizeof(info_janela_t), true)) return ERRO_ENDERECO;
        return janelas_listar((void *)ptr, (int)n);
    }
    if (tipo == INFO_USB) {
        if (!usuario_ok((void *)ptr, n, true) || n < 2) return ERRO_ENDERECO;
        int usb_relatorio(char *s, int max);
        return usb_relatorio((char *)ptr, (int)n);
    }
    if (tipo == INFO_MONTAGENS) {
        int32_t montagem_info(info_montagem_t *v, uint32_t n);
        return montagem_info((info_montagem_t *)ptr, n);
    }
    if (tipo == INFO_VERSAO) {
        info_versao_t *v = (info_versao_t *)ptr;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        extern const char *sagui_versao;
        extern uint32_t imagem_bytes;
        txt_copia_n(v->versao, sagui_versao, sizeof v->versao);
        v->imagem_bytes = imagem_bytes;
        v->segundos_ligado = relogio_segundos();
        v->largura = info_boot.vesa_ativo ? info_boot.vesa_largura : 80;
        v->altura = info_boot.vesa_ativo ? info_boot.vesa_altura : 25;
        return 0;
    }
    if (tipo == INFO_ISA) {   /* D302 */
        int32_t isa_info(uint32_t ptr, uint32_t n);
        return isa_info(ptr, n);
    }
    if (tipo == INFO_AUDIO) {   /* D088 */
        int32_t audio_info(uint32_t ptr, uint32_t n);
        return audio_info(ptr, n);
    }
    if (tipo == INFO_WG) {   /* D363: el túnel WireGuard (sin secretos) */
        int32_t tunel_info(uint32_t ptr, uint32_t n);
        return tunel_info(ptr, n);
    }
    if ((tipo >= INFO_REDE && tipo <= INFO_ARP) || (tipo >= INFO_INTERFACES && tipo <= INFO_TRAFEGO)) {
        int32_t rede_info(uint32_t tipo, uint32_t ptr, uint32_t n);
        return rede_info(tipo, ptr, n);   /* D046, D079 */
    }
    if ((tipo >= INFO_E820 && tipo <= INFO_CPU_MHZ) || (tipo >= INFO_LOG && tipo <= INFO_ARRANQUE))
        return hw_info(tipo, ptr, n);   /* infopc (D041) y el informe de compatibilidad (D076) */
    if (tipo == INFO_FREQUENCIA) {   /* driver de frecuencia (D106) */
        int32_t frequencia_info(uint32_t ptr, uint32_t n);
        return frequencia_info(ptr, n);
    }
    if (tipo == INFO_FREQ_NUCLEOS) {   /* D175: el estado de cada CPU */
        int32_t frequencia_info_nucleos(uint32_t ptr, uint32_t n);
        return frequencia_info_nucleos(ptr, n);
    }
    if (tipo == INFO_ACEL2D) return acel2d_info(ptr, n);         /* D195 */
    if (tipo == INFO_ACEL2D_CTL)   /* D244; D387: 2-4, el volcado por GPU; D391: 5-7, la vuelta */
        return ptr == 0 ? acel2d_apagar() : ptr == 1 ? acel2d_acender() : ptr <= 35 || (ptr >= 50 && ptr <= 53) ? acel2d_volcado_ctl((int)ptr) : ERRO_INVALIDO;   /* D424: 50-53, prioridad */
    if (tipo == INFO_ACEL2D_PROVA) return acel2d_prova(ptr, n);  /* D195: acel2d probar */
    if (tipo == INFO_GPU3D) return gpu3d_info(ptr, n);           /* D285: 3D por la GPU */
    if (tipo == INFO_GPU3D_OP) return gpu3d_op(ptr, n);
    if (tipo == INFO_GPU3D_PROVA) return gpu3d_prova(ptr, n);
    if (tipo == INFO_GPU3D_CTL) return gpu3d_ctl(ptr);
    if (tipo == INFO_OCIO) return ocio_info(ptr, n);   /* D422 */
    if (tipo == INFO_OCIO_CTL) return ocio_ctl(ptr);
    if (tipo == INFO_GPU_ENERGIA) return gpu_energia_info(ptr, n);   /* D399 */
    if (tipo == INFO_GPU_ENERGIA_CTL) return gpu_energia_ctl(ptr);
    if (tipo == INFO_CAB_OPC) {   /* D323: configuración de aceleración y disco en la cabecera de arranque */
        int32_t cab_opc_gravar(uint32_t pedido);
        return cab_opc_gravar(ptr);
    }
    if (tipo == INFO_KCONF) {   /* D353: kconfig */
        int32_t kconf_info(uint32_t ptr, uint32_t n);
        return kconf_info(ptr, n);
    }
    if (tipo == INFO_KCONF_GRAVAR) {
        int32_t kconf_gravar(uint32_t pedido);
        return kconf_gravar(ptr);
    }
    if (tipo == INFO_MOTOR_GPU) {   /* motor 3D por la GPU (D158) */
        int32_t motor_gpu_info(uint32_t ptr, uint32_t n);
        return motor_gpu_info(ptr, n);
    }
    if (tipo == INFO_CAMARA) {   /* cámara UVC y fuente de prueba (D277) */
        int32_t camara_info(uint32_t ptr, uint32_t n);
        return camara_info(ptr, n);
    }
    if (tipo == INFO_CAPTURA) return captura_info(ptr, n);   /* D292: micrófono */
    if (tipo == INFO_MANDOS) return mandos_info(ptr, n);     /* D307: mandos y joysticks */
    if (tipo == INFO_BANDEJA) return barra_info_bandeja(ptr, n);   /* D114 */
    if (tipo == INFO_ENERGIA) return acpi_info(ptr, n);   /* D113 */
    if (tipo == INFO_BATERIA) return bateria_info(ptr, n);   /* D192 */
    if (tipo == INFO_RADIOS) return radios_info(ptr, n);   /* D405: radios y panel del HP */
    if (tipo == INFO_RADIOS_MUDAR) return radios_mudar(ptr);
    if (tipo == INFO_ACPI_TABELA) return acpi_tabela_ler(ptr, n);
    if (tipo == INFO_IRQ_VIAS) return irq_rotas_info(ptr, n);   /* D211 */
    if (tipo == INFO_SENSORES || tipo == INFO_MSR_PROVA) {   /* monitor de hardware (D092) */
        int32_t sensores_info(uint32_t tipo, uint32_t ptr, uint32_t n);
        return sensores_info(tipo, ptr, n);
    }
    return ERRO_INVALIDO;
}

/* aumentar_memoria(n): extiende el área de datos del programa en n bytes y
 * devuelve dónde terminaba. Es la base del malloc de la libsagui. */
int32_t sis_aumentar_memoria(uint32_t n, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    processo_t *l = processo_recursos();   /* D167: el área de datos es del proceso */
    uint32_t antigo = l->dados_fim;
    if (n == 0) return (int32_t)antigo;
    if (antigo > dados_teto(l) || n > dados_teto(l) - antigo) return ERRO_SEM_MEMORIA;   /* D378 */
    /* Solo se añaden páginas: las otras CPU no tienen nada que olvidar en
     * su TLB (una entrada no presente no se guarda en ella). */
    if (!espaco_alocar(l->espaco, antigo, n, PG_ESCRITA | PG_USUARIO | PG_NX)) return ERRO_SEM_MEMORIA;   /* D349 */
    l->dados_fim = antigo + n;
    return (int32_t)antigo;
}

/* memoria(op, endereço, bytes) (D209): páginas del área de datos para el malloc.
 *
 *   MEM_RESERVAR  alarga el área como aumentar_memoria, pero sin asignar
 *                 páginas: el malloc reserva direcciones de golpe y asigna solo
 *                 lo que usa. Devuelve el principio (alineado a página).
 *   MEM_DEVOLVER  quita y libera las páginas de [endereço, endereço+bytes): lo
 *                 que el malloc libera vuelve al sistema sin que el programa
 *                 acabe. Las direcciones siguen siendo del área: tocarlas sin
 *                 volver a asignarlas es un fallo de página.
 *   MEM_ASIGNAR   asigna (a cero) las páginas que falten en el rango.
 *
 * Solo dentro de [dados_inicio, dados_fim) y en páginas enteras, así que no
 * puede tocar el código del programa, las pilas, las ventanas ni los BAR.
 * SMP (D165, D167): otro hilo del proceso puede estar corriendo en otra CPU
 * con esas páginas en su TLB; se le saca de ella (espaco_sincronizar) después
 * de quitarlas y antes de liberarlas, como al cerrar una ventana.
 * D351: MEM_DEVOLVER y quitar la escritura esperan antes a que ningún otro
 * hilo duerma a mitad de una llamada que use esas páginas (espaco_pode_tirar). */
#define MEM_LOTE 64   /* páginas que se quitan antes de sincronizar y liberar */

static bool rango_de_dados(const processo_t *l, uint32_t ini, uint32_t n)
{
    if ((ini | n) & (PAGINA - 1)) return false;
    if (ini < l->dados_inicio || ini > l->dados_fim) return false;
    return n <= l->dados_fim - ini;
}

int32_t sis_memoria(uint32_t op, uint32_t ini, uint32_t n, uint32_t d, uint32_t e)
{
    processo_t *l = processo_recursos();
    if (op == MEM_RESERVAR) {
        uint32_t base = (l->dados_fim + PAGINA - 1) & ~(PAGINA - 1);
        if (n & (PAGINA - 1)) return ERRO_INVALIDO;
        uint32_t teto = dados_teto(l);   /* D378: con intérprete, antes de él */
        if (base < l->dados_fim || base > teto || n > teto - base) return ERRO_SEM_MEMORIA;
        l->dados_fim = base + n;
        return (int32_t)base;
    }
    /* D378: el intérprete de los programas dinámicos (ld-sagui) deja en solo lectura la zona RELRO
     * del ejecutable (su GOT) después de relocarlo: en la imagen del programa solo se puede pedir
     * eso, solo lectura y sin ejecución; nunca dar escritura ni ejecución fuera del área de datos. */
    bool so_ler_imagem = op == MEM_PROTEGER && d == 0 && !((ini | n) & (PAGINA - 1)) &&
                         ini >= USUARIO_INICIO && ini < l->dados_inicio && n <= l->dados_inicio - ini;
    if (!so_ler_imagem && !rango_de_dados(l, ini, n)) return ERRO_INVALIDO;
    if (op == MEM_ASIGNAR)
        return espaco_alocar(l->espaco, ini, n, PG_ESCRITA | PG_USUARIO | PG_NX) ? 0 : ERRO_SEM_MEMORIA;   /* D349 */
    if (op == MEM_PROTEGER) {
        /* D349: como mprotect, sobre lo ya asignado. Quitar permisos (escritura,
         * ejecución) y también darlos se sincroniza con las demás CPU: un hilo
         * en otra CPU con la entrada vieja en su TLB fallaría (o seguiría
         * escribiendo) con los permisos de antes. */
        if (d & ~(uint32_t)(MEM_PROT_ESCRITA | MEM_PROT_EXEC | 1)) return ERRO_INVALIDO;
        /* D351: quitar la escritura espera a que ningún otro hilo duerma en
         * una llamada que vaya a escribir ahí (dar permisos o quitar la
         * ejecución no le afecta: el kernel no ejecuta código del programa). */
        if (!(d & MEM_PROT_ESCRITA) && !espaco_pode_tirar()) return SIS_REPETIR;
        /* Como mprotect (ENOMEM): todas las páginas del rango tienen que estar
         * asignadas. Si no, unas quedarían con los permisos nuevos y las que se
         * asignaran después (MEM_ASIGNAR) con NX: mejor un error claro. */
        for (uint32_t v = ini; v < ini + n; v += PAGINA)
            if (!espaco_existe(l->espaco, v)) return ERRO_SEM_MEMORIA;   /* D352: en la swap también vale */
        uint32_t por = ((d & MEM_PROT_ESCRITA) ? PG_ESCRITA : 0) | ((d & MEM_PROT_EXEC) ? 0 : PG_NX);
        uint32_t tirar = ((d & MEM_PROT_ESCRITA) ? 0 : PG_ESCRITA) | ((d & MEM_PROT_EXEC) ? PG_NX : 0);
        espaco_ajustar(l->espaco, ini, n, por, tirar);
        espaco_sincronizar(l->espaco);
        return 0;
    }
    if (op == MEM_SACAR)   /* D352 */
        return (int32_t)troca_sacar_faixa(l->espaco, ini, ini + n);
    if (op == MEM_COMPARTIR) {   /* D378 (fase 3): d = fd << 8 | MEM_PROT_EXEC o 0; e = desplazamiento */
        if (d & 0xFF & ~(uint32_t)MEM_PROT_EXEC) return ERRO_INVALIDO;   /* nunca escritura */
        if (d & MEM_PROT_EXEC) {   /* D485: ni código de un montaje sin ejecución (bibliotecas en /red) */
            char cam[CAMINHO_MAX];
            if (arquivos_caminho(d >> 8, cam, sizeof cam) == 0 && montagem_sem_exec(cam)) return ERRO_ACESSO;
        }
        if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351: va a cambiar marcos del rango */
        return imagens_mapear(l, ini, n, d >> 8, (d & MEM_PROT_EXEC) != 0, e);
    }
    if (op != MEM_DEVOLVER) return ERRO_INVALIDO;
    if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351 */
    /* D378 (fase 3): las páginas compartidas del rango (espaco_tirar no quita las prestadas). */
    uint32_t compartidas = imagens_desmapear(l->espaco, ini, ini + n);
    fis_t fisicas[MEM_LOTE];
    uint32_t devolvidas = 0;
    uint32_t v = ini, fim = ini + n;
    while (v < fim) {
        uint32_t k = 0;
        for (; v < fim && k < MEM_LOTE; v += PAGINA) {
            /* Una página prestada (de otro dueño) no se quita: espaco_tirar
             * devuelve 0. En el área de datos no debería haber ninguna. */
            fis_t f = espaco_tirar(l->espaco, v);
            if (f) fisicas[k++] = f;
        }
        if (!k) continue;
        espaco_sincronizar(l->espaco);
        for (uint32_t i = 0; i < k; i++) memfisica_liberar(fisicas[i]);
        devolvidas += k;
    }
    return (int32_t)(devolvidas + compartidas);
}

/* D216: con servicios en marcha, el reinicio lo hace la tarea "apagar" del
 * kernel, que primero los para (puede tardar unos segundos): la llamada vuelve
 * y el programa solo espera (reiniciar() de la libsagui ya no vuelve).
 * D450: ahora siempre por esa tarea, con o sin servicios: corre en la CPU 0 (la
 * llamada puede venir de un secundario), con su orden y sus plazos (energia.c).
 * "a" elige el primer método (ENERGIA_RESET_*; 0 = el orden normal). Solo si la
 * tarea todavía no existe se reinicia aquí mismo. */
int32_t sis_reiniciar(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    int metodo = a <= ENERGIA_RESET_MAX ? (int)a : ENERGIA_RESET_AUTO;
    if (acpi_tarefa_pronta()) {
        klog("[SAGUI] reinicio pedido por %s (pid %d), metodo %d\n", processo_atual()->nome, processo_atual()->pid, metodo);
        acpi_pedir_reinicio(metodo);
        return 0;
    }
    montagem_desmontar_tudo();   /* graba lo que esté pendiente (D040) */
    maquina_reiniciar();
}

/* desligar(): apagado ordenado (D113). Lo hace la tarea "apagar" del kernel;
 * la llamada vuelve enseguida y el programa sólo tiene que esperar. */
int32_t sis_desligar(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    klog("[SAGUI] apagado pedido por %s (pid %d)\n", processo_atual()->nome, processo_atual()->pid);
    acpi_pedir_apagado();
    return 0;
}

int32_t sis_acpi(uint32_t op, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    return acpi_controle(op);
}

/* registrar(texto): una línea en el log del sistema (la serial), con el nombre y el
 * PID del programa. Los programas gráficos registran lo que hacen por aquí,
 * sin ensuciar la terminal de quien los abrió. */
int32_t sis_registrar(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    char t[256];
    int r = usuario_texto((const char *)ptr, t, sizeof t);
    if (r < 0) return r;
    klog("[%s %d] %s\n", processo_atual()->nome, processo_recursos()->pid, t);
    return 0;
}

/* sis_executar está en kernel/nucleo/carregador.c (H13). */

/* grupo(pid, grupo): pone un proceso (él mismo, pid 0, o un hijo) en un
 * grupo de procesos; grupo 0 = un grupo nuevo, liderado por él. El shell
 * interactivo pone cada trabajo (todas las etapas de un pipe) en un grupo y lo
 * declara en primer plano: el Ctrl+C termina el grupo entero (D037). */
int32_t sis_grupo(uint32_t pid, uint32_t grupo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    processo_t *eu = processo_recursos();
    processo_t *p = processo_lider(pid ? processo_por_pid((int)pid) : eu);
    if (!p || p->estado == P_ZUMBI || !p->usuario) return ERRO_SEM_PROCESSO;
    if (p != eu && p->pai != eu->pid) return ERRO_ACESSO;
    p->grupo = grupo ? (int)grupo : p->pid;
    /* D167: los hilos van con su proceso (el Ctrl+C mira el grupo). */
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *t = processo_indice(i);
        if (t->lider == p && t->estado != P_LIVRE) t->grupo = p->grupo;
    }
    return 0;
}
