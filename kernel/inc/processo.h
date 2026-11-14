// SPDX-License-Identifier: GPL-3.0-only
/* processo.h - procesos y planificador. */
#ifndef SAGUI_PROCESSO_H
#define SAGUI_PROCESSO_H
#include "tipos.h"
#include "cpu.h"
#include "paginacao.h"
#include "caminho.h"
#include "fpu.h"

#define MAX_PROCESSOS   64
#define MAX_ARQUIVOS    32
#define PILHA_KERNEL    8192     /* 2 páginas por proceso */
#define QUANTUM_TIQUES  2        /* 20 ms de CPU antes de ceder el turno */
#define BAR_MAPAS_MAX   8        /* mapas de dispositivos por proceso (D158) */
#define CASA_NENHUMA    0xFFu    /* D174: el proceso aún no tiene CPU propia */

typedef enum { P_LIVRE, P_PRONTO, P_EXECUTANDO, P_BLOQUEADO, P_ZUMBI } estado_t;

/* D251: programas que el kernel reconoce por la ruta de la que se cargaron
 * (processo_executar_env). Solo se usa para permisos: el supervisor de
 * servicios y el servidor de escritorio remoto (inyección de entrada). */
#define EXE_COMUM      0
#define EXE_SERVICIOS  1   /* /bin/servicios */
#define EXE_VNCD       2   /* /bin/vncd o /apps/bin/vncd */

struct arquivo;

typedef struct processo {
    int         pid;
    int         pai;
    int         grupo;            /* grupo de procesos: Ctrl+C termina el grupo en primer plano (D037) */
    estado_t    estado;
    char        nome[32];
    bool        usuario;          /* programa en el anillo 3 (o tarea del kernel) */
    uint8_t     exe_sistema;      /* D251: EXE_* si se cargó de una ruta del sistema que da permisos */
    /* D255: identidad. Vale la del líder (los hilos no la tienen propia):
     * se lee con processo_recursos() o processo_lider(). */
    uint32_t    uid, gid;
    int32_t     como_uid, como_gid;   /* USU_COMO: identidad del próximo hijo (-1 = la propia) */
    bool        de_servico;       /* desciende del gestor de servicios: no es la sesión local */
    espaco_t    espaco;
    uint32_t    pilha_kernel;     /* base (física = virtual) */
    quadro_t   *quadro;           /* donde quedaron guardados los registros */
    const void *espera;           /* "canal" en el que está bloqueado */
    uint32_t    acordar_em;       /* para dormir: tic en el que despierta (0 = no) */
    int         codigo_saida;
    char        motivo[64];       /* por qué terminó, si lo terminó el kernel */
    struct arquivo *arquivos[MAX_ARQUIVOS];
    char        pasta[CAMINHO_MAX];
    uint32_t    dados_inicio, dados_fim;   /* área que crece con aumentar_memoria */
    uint32_t    mmio_fim;          /* próxima dirección libre para los mapas de dispositivos (D158) */
    struct { uint32_t virt, bytes; } bar_mapas[BAR_MAPAS_MAX];   /* mapas de BAR vivos (D158) */
    uint32_t    tiques_cpu;
    uint32_t    cpu;          /* a CPU onde este processo corre (D157) */
    volatile bool na_cpu;     /* D165: una CPU lo tiene elegido (en modo usuario, o en el kernel sin el cerrojo) */
    bool        espaco_pendente; /* D165: terminado desde otra CPU; su espacio se destruye cuando esa CPU lo deje */
    /* D167: hilos. Un hilo es una entrada más de la tabla (su pila de kernel,
     * su cuadro, su FPU, su CPU) que comparte el espacio de su líder. Lo
     * compartido (ficheros, carpeta, área de datos, mapas, ventanas, pid
     * que ven los demás) se lee siempre del líder: processo_lider(). */
    struct processo *lider;       /* NULL en el hilo principal (el proceso) */
    uint32_t    futex;            /* D167: dirección física que espera en futex_esperar */
    uint64_t    vagas_hilos;      /* D186: en el líder, huecos de pila de hilos ocupados (bit n = hueco n) */
    uint8_t     vaga_pilha;       /* D186: en un hilo, su hueco de pila + 1 (0 = la pila es del programa) */
    /* D174: lo que usan los planificadores (planificadores.c). */
    uint32_t    mascara_cpu;      /* CPU permitidas (bit n = CPU n); 0 = todas */
    int         nice;             /* -20..19: solo cuenta en el planificador "prioridad" */
    uint32_t    casa;             /* la CPU de su cola (CASA_NENHUMA = ninguna todavía) */
    uint32_t    saiu_em;          /* tic en que dejó la CPU por última vez (caché caliente o fría) */
    uint32_t    migracoes;        /* veces que cambió de CPU */
    uint32_t    execucoes;        /* veces que una CPU lo tomó */
    uint32_t    recente;          /* uso reciente de CPU (decae a la mitad cada periodo) */
    uint32_t    tiques_vistos;    /* tiques_cpu en el último periodo de "recente" */
    uint64_t    pronto_tsc;       /* TSC cuando quedó listo al despertar (0 = no se mide) */
    uint32_t    lat_soma_kc, lat_max_kc, lat_n;   /* latencia de despertar a correr, en miles de ciclos */
    bool        fpu_usada;        /* ya usó FPU/MMX/SSE: "fpu" guarda sus registros */
    uint8_t     fpu[FPU_ESTADO] __attribute__((aligned(16)));   /* área del FXSAVE */
} processo_t;

processo_t *processo_atual(void);   /* o processo que esta CPU esta a executar (D157) */

/* D167: el proceso al que pertenece un hilo (el propio, si es el principal):
 * el que guarda los ficheros, la carpeta, el área de datos y el pid. */
static inline processo_t *processo_lider(processo_t *p) { return p && p->lider ? p->lider : p; }
static inline processo_t *processo_recursos(void) { return processo_lider(processo_atual()); }
bool        grupo_na_cpu(const processo_t *lider);   /* D167: ¿alguna CPU tiene un hilo suyo? */
int         hilos_contar(const processo_t *lider);   /* D167: hilos vivos, el principal incluido */

void        processos_iniciar(void);
processo_t *processo_novo(const char *nome, int pai);
processo_t *tarefa_kernel(const char *nome, void (*funcao)(void *), void *arg);
processo_t *processo_por_pid(int pid);
processo_t *processo_indice(int i);

quadro_t *escalonador_tique(quadro_t *q);     /* llamado por el reloj */
quadro_t *escalonar(quadro_t *q);             /* cede el turno ahora */
quadro_t *escalonador_tique_secundaria(quadro_t *q);   /* D165: el tic de un secundario, sin el cerrojo */
void      avisar_ociosa(processo_t *p);  /* D165: despertar una CPU ociosa que pueda tomarlo */
void      processo_tirar_da_cpu(processo_t *p);   /* D165: que la CPU donde corre deje su código */
void      espaco_sincronizar(espaco_t e);     /* D165: que ninguna otra CPU use traducciones viejas de "e" */
void      bloquear(const void *canal);
void      ceder(void);   /* tarea del kernel: cede el turno ahora (ej.: después de bloquear) */
void      acordar(const void *canal);
void      processo_ocioso_preparar(uint32_t cpu);   /* o ocioso de um processador secundario (D157) */
void      processo_encerrar(processo_t *p, int codigo, const char *motivo);
void      processo_liberar(processo_t *p);
const char *nome_estado(estado_t e);
void      processo_preparar_usuario(processo_t *p, uint32_t entrada, uint32_t pilha);
int       processo_executar(const char *caminho, int argc, char *const argv[], processo_t *pai);
int       processo_executar_como(const char *caminho, int argc, char *const argv[], uint32_t uid, uint32_t gid);   /* D255: sin padre */
int       processo_executar_env(const char *caminho, int argc, char *const argv[], int envc, char *const envp[],
                                processo_t *pai);

#define PILHA_USUARIO_TOPO  USUARIO_FIM
#define PILHA_USUARIO_TAM   (USUARIO_FIM - USUARIO_PILHA)   /* 8 MB virtuales (D186) */
#define PILHA_USUARIO_INICIAL (8u * 1024u)   /* lo que se asigna al cargar por debajo de argv/envp (D186) */
#define DADOS_LIMITE        USUARIO_HILOS     /* el área de datos termina antes de las pilas de los hilos (D049, D186) */

/* pilha.c (D186): pilas de usuario que crecen bajo demanda. */
typedef enum { PILHA_FORA, PILHA_CRESCER, PILHA_GUARDA } pilha_zona_t;
pilha_zona_t pilha_zona(const processo_t *p, uint32_t virt);       /* dónde cae "virt" para el proceso de "p" */
bool         pilha_crescer(processo_t *p, uint32_t virt);          /* asigna la página si es de pila; ¿ya está? */
int          pilha_vaga_reservar(processo_t *lider);               /* un hueco libre para un hilo, o -1 */
void         pilha_vaga_liberar(processo_t *hilo);
uint32_t     pilha_usada_kb(const processo_t *p);                  /* páginas de pila asignadas del hilo */
void         pilha_iniciar(void);                                  /* registra el fallo de página */

#endif
