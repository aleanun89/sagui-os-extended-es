// SPDX-License-Identifier: GPL-3.0-only
/* processo.h - procesos y planificador. */
#ifndef SAGUI_PROCESSO_H
#define SAGUI_PROCESSO_H
#include "tipos.h"
#include "cpu.h"
#include "paginacao.h"
#include "caminho.h"
#include "fpu.h"
#include "abi.h"

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
#define EXE_BIN        3   /* D269: cualquier otro programa de /bin (solo root escribe ahí): el diálogo de permisos recuerda su autorización */

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
    uint16_t    umask;            /* D282: lo que se quita a los permisos de lo que crea (022) */
    char        casa_conta[40];   /* D340: la carpeta personal de la cuenta («/home/ana»): lo que pide en /home va
                                   * ahí; vacía = /home tal cual (sagui, root, servicios). Heredada */
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
    uint32_t    dados_teto;       /* D378: hasta dónde puede crecer (0 = DADOS_LIMITE; menos con intérprete) */
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
    uint64_t    futex;            /* D167: dirección física que espera en futex_esperar (64 bits con PAE, D349) */
    uint64_t    vagas_hilos;      /* D186: en el líder, huecos de pila de hilos ocupados (bit n = hueco n) */
    uint8_t     vaga_pilha;       /* D186: en un hilo, su hueco de pila + 1 (0 = la pila es del programa) */
    /* D351: la barrera del espacio compartido (espaco_pode_tirar). */
    bool        em_chamada;       /* el hilo está dentro de una llamada al sistema (puede dormir en ella) */
    bool        na_barreira;      /* el hilo durmió en la barrera o en su puerta (hay que limpiar acordar_em) */
    int         tirando;          /* en el líder: tid del hilo que espera para quitar páginas o escritura (0 = nadie) */
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
    /* D401: señales. Las acciones, las pendientes del proceso, el origen de cada una y la alarma
     * valen en el líder; la máscara, las pendientes del hilo, la pila alternativa y lo de
     * sigsuspend, en cada hilo. */
    acao_sinal_t sinais_acoes[SINAIS_N];   /* en el líder */
    struct { int pid; uint16_t uid, codigo; } sinais_origem[SINAIS_N];   /* en el líder: quién mandó cada pendiente */
    uint32_t    sinais_pendentes; /* en el líder: dirigidas al proceso */
    uint32_t    sinais_hilo;      /* dirigidas a este hilo */
    uint32_t    sinais_mascara;   /* bloqueadas en este hilo */
    uint32_t    sinais_mascara_suspensa;   /* sigsuspend: la máscara de antes (vale si sinais_suspenso) */
    bool        sinais_suspenso;  /* dentro de sigsuspend */
    pilha_sinal_t sinais_pilha;   /* sigaltstack (base 0 = ninguna) */
    uint32_t    alarme_em;        /* en el líder: tic del SIGALRM (0 = ninguna) */
    int         sinal_fim;        /* la señal que terminó el proceso (0 = salió por sí mismo) */
    /* D401: la llamada al sistema en curso. "repetindo" lo pone el despachador cuando la llamada
     * devuelve SIS_REPETIR (1 = una espera que una señal puede interrumpir con EINTR; 2 = se
     * repite siempre); "repetida" es lo que había al entrar (la llamada es una repetición). */
    uint8_t     repetindo;
    bool        chamada_repetida;
    uint32_t    sondar_prazo;     /* poll: tic en que vence (0 = sin plazo), mientras se repite */
    quadro_t   *quadro_chamada;   /* el cuadro de la llamada en curso (fork, exec) */
    uint32_t    fd_fechar_exec;   /* en el líder: bit n = FD_CLOEXEC del descriptor n */
    bool        exec_sem_volta;   /* revisión D401: execve ya mató los demás hilos; si falla ahora, el proceso termina */
    bool        fpu_usada;        /* ya usó FPU/MMX/SSE: "fpu" guarda sus registros */
    /* área del FXSAVE o del XSAVE (D381): XSAVE pide 64 bytes de alineación y la cabecera
     * (bytes 512-575) a cero hasta el primer guardado; memset al crear el proceso lo deja así. */
    uint8_t     fpu[FPU_ESTADO] __attribute__((aligned(FPU_ESTADO_ALINHA)));
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
bool      espaco_pode_tirar(void);            /* D351: ¿ningún otro hilo dormido en una llamada? si no, bloquea */
bool      espaco_porta(void);                 /* D351: ¿puede entrar esta llamada? si no, bloquea */
void      espaco_chamada_fim(void);           /* D351: el hilo sale de una llamada al sistema */
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

/* D401: fork, exec y wait (bifurcar.c, carregador.c) y señales (sinais.c). */
void      processo_preparar_copia(processo_t *p, const quadro_t *q);   /* el hijo del fork empieza en "q" con eax = 0 */
int32_t   espera_estado(const processo_t *p);   /* el estado de waitpid de un proceso terminado */
void      sinais_iniciar_processo(processo_t *p, const processo_t *de, bool fork);   /* acciones de un proceso nuevo */
void      sinais_exec(processo_t *l);           /* exec: los tratadores vuelven a SIG_DFL */
int       sinais_enviar(processo_t *p, int sinal, int pid_origem, uint32_t uid_origem, int codigo);   /* al proceso */
int       sinais_enviar_hilo(processo_t *t, int sinal, int pid_origem, uint32_t uid_origem, int codigo);
void      sinais_filho_terminou(processo_t *filho);   /* SIGCHLD al padre (o que no quede zombi) */
quadro_t *sinais_ao_sair(quadro_t *q);          /* al volver al anillo 3: entrega una señal pendiente */
quadro_t *sinais_excecao(quadro_t *q, int sinal, int codigo, uint32_t endereco, const char *motivo);
void      sinais_tique(uint32_t agora);         /* las alarmas (tic de la CPU 0) */
bool      sinais_chamada_interrompivel(uint32_t n);   /* ¿una señal puede cortar esta llamada con EINTR? */
quadro_t *sinais_voltar(quadro_t *q);           /* SIS_SINAL_VOLTAR (sigreturn) */
void      sondar_esperar(void);                 /* poll: dormir hasta cualquier acordar() o el plazo */

#define PILHA_USUARIO_TOPO  USUARIO_FIM
#define PILHA_USUARIO_TAM   (USUARIO_FIM - USUARIO_PILHA)   /* 8 MB virtuales (D186) */
#define PILHA_USUARIO_INICIAL (8u * 1024u)   /* lo que se asigna al cargar por debajo de argv/envp (D186) */
#define DADOS_LIMITE        USUARIO_HILOS     /* el área de datos termina antes de las pilas de los hilos (D049, D186) */
/* D378: los programas con PT_INTERP llevan su intérprete (/lib/ld-sagui.so.1, un ELF a dirección fija) en
 * [INTERP_BASE, DADOS_LIMITE): para ellos el área de datos acaba en INTERP_BASE. */
#define INTERP_BASE         0xD8000000u
static inline uint32_t dados_teto(const processo_t *p) { return p->dados_teto ? p->dados_teto : DADOS_LIMITE; }

/* pilha.c (D186): pilas de usuario que crecen bajo demanda. */
typedef enum { PILHA_FORA, PILHA_CRESCER, PILHA_GUARDA } pilha_zona_t;
pilha_zona_t pilha_zona(const processo_t *p, uint32_t virt);       /* dónde cae "virt" para el proceso de "p" */
bool         pilha_crescer(processo_t *p, uint32_t virt);          /* asigna la página si es de pila; ¿ya está? */
int          pilha_vaga_reservar(processo_t *lider);               /* un hueco libre para un hilo, o -1 */
void         pilha_vaga_liberar(processo_t *hilo);
uint32_t     pilha_usada_kb(const processo_t *p);                  /* páginas de pila asignadas del hilo */
void         pilha_iniciar(void);                                  /* registra el fallo de página */

#endif
