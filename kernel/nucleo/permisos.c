// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos.c - quién puede hacer qué (D255, mejora 098, fase 1).
 *
 * SaguiOS tiene dos clases de procesos: los de root (uid 0: el kernel, el
 * gestor de servicios y lo que este lanza como root, lo elevado con la
 * contraseña y todo en el modo rescate) y los demás (sagui, 1000, el
 * escritorio; y los usuarios de servicio). Aquí está, en una sola tabla, qué
 * llamadas al sistema son libres y cuáles solo de root. El despachador
 * (sistema.c) pregunta antes de cada llamada.
 *
 * Reglas:
 *   - lo que no está en la tabla como libre se niega a quien no es root (una
 *     llamada nueva sin clasificar queda cerrada, no abierta);
 *   - en las llamadas con la operación en el primer argumento, la operación
 *     decide (operacao_livre); los argumentos que se miran son números, no
 *     punteros, así que otro hilo no puede cambiarlos entre la comprobación y
 *     el uso;
 *   - las rutas se comprueban dentro de cada llamada, ya copiadas al kernel y
 *     resueltas (permisos_ruta), por la misma razón.
 * Cada negativa se anota en el registro del kernel, con un límite para no
 * llenarlo (y con él /logs). */
#include "permisos.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define L PERM_LIVRE
#define R PERM_ROOT
#define O PERM_OPERACAO

/* Cada llamada de abi.h. Ver la tabla comentada en docs/decisoes.md (D255). */
static const uint8_t classe[SIS_MAX] = {
    /* Procesos */
    [SIS_EXECUTAR] = L, [SIS_ESPERAR] = L, [SIS_SAIR] = L, [SIS_PID] = L,
    [SIS_MATAR] = O,                       /* los suyos (mismo uid) */
    /* Ficheros: las rutas protegidas se miran dentro (permisos_ruta) */
    [SIS_ABRIR] = L, [SIS_LER] = L, [SIS_ESCREVER] = L, [SIS_FECHAR] = L, [SIS_POSICIONAR] = L,
    [SIS_INFO_ARQUIVO] = L, [SIS_CRIAR_PASTA] = L, [SIS_REMOVER_PASTA] = L, [SIS_APAGAR] = L,
    [SIS_LISTAR] = L, [SIS_MUDAR_PASTA] = L, [SIS_PASTA_ATUAL] = L, [SIS_RENOMEAR] = L,
    [SIS_TRUNCAR] = L, [SIS_SINCRONIZAR] = L, [SIS_DATAS_ARQUIVO] = L,
    [SIS_PIPE] = L, [SIS_DUPLICAR] = L, [SIS_DISPONIVEL] = L, [SIS_TIPO_DESCRITOR] = L,
    /* Tiempo, memoria, registro */
    [SIS_DORMIR] = L, [SIS_TEMPO] = L, [SIS_DATA] = L, [SIS_AUMENTAR_MEM] = L, [SIS_MEMORIA] = L,
    [SIS_INFO_SISTEMA] = O,                /* consultar sí; las que tocan la GPU o los MSR, no */
    [SIS_REGISTRAR] = L,
    [SIS_HORA] = O,                        /* leer sí; ajustar el reloj, no */
    [SIS_ZONA] = O,                        /* leer sí; cambiar la zona del sistema, no */
    /* Apagar y reiniciar: la sesión local sí (como el botón de encendido); un servicio o SSH, no */
    [SIS_REINICIAR] = O, [SIS_DESLIGAR] = O,
    [SIS_ACPI] = R,
    /* Terminal y ventanas: cosas de la sesión del usuario */
    [SIS_MODO_TERMINAL] = L, [SIS_LIMPAR_TELA] = L, [SIS_ABRIR_TV] = L, [SIS_PRIMEIRO_PLANO] = L,
    [SIS_GRUPO] = L, [SIS_INTERRUPCOES] = L,
    [SIS_CRIAR_JANELA] = L, [SIS_FECHAR_JANELA] = L, [SIS_ATUALIZAR_JANELA] = L,
    [SIS_PROXIMO_EVENTO] = L, [SIS_TITULO_JANELA] = L, [SIS_ESTILO_JANELA] = L,
    [SIS_REDIMENSIONAR_JANELA] = L, [SIS_PEDIR_FECHAR] = L, [SIS_MOVER_JANELA] = L,
    [SIS_MENU_CONTEXTUAL] = L, [SIS_CAPTURAR_TELA] = L, [SIS_AVISAR] = L,
    [SIS_COPIAR_TEXTO] = L, [SIS_COLAR_TEXTO] = L, [SIS_COPIAR_IMAGEM] = L, [SIS_COLAR_IMAGEM] = L,
    [SIS_TECLAS_SEGURADAS] = L, [SIS_TECLADO] = L,   /* la distribución del teclado: preferencia del usuario */
    [SIS_MODOS_VIDEO] = L, [SIS_MUDAR_MODO] = L,      /* la resolución: preferencia del usuario */
    [SIS_FUNDO] = L, [SIS_ACENTO] = L,
    [SIS_ALEATORIO] = L, [SIS_INFO_ALEATORIO] = L,
    /* Sonido */
    [SIS_BIP] = L, [SIS_AUDIO_ABRIR] = L, [SIS_AUDIO_ESCREVER] = L, [SIS_AUDIO_CONTROLE] = L,
    [SIS_AUDIO_FECHAR] = L,
    [SIS_AUDIO_HDA] = O,                   /* el estado sí; verbos, pines y GPIO del códec, no */
    /* Discos y montajes */
    [SIS_LISTAR_DISCOS] = L,
    [SIS_MONTAR] = R, [SIS_DESMONTAR] = R,
    [SIS_DISCO_BLOCOS] = O,                /* solo DBLOCO_INFO; leer en crudo saltaría todas las protecciones */
    /* Red: usar la red es libre; configurarla, no */
    [SIS_SOCKET] = L, [SIS_CONECTAR] = L, [SIS_ESCUTAR] = L, [SIS_ACEITAR] = L,
    [SIS_ENVIAR_PARA] = L, [SIS_RECEBER_DE] = L, [SIS_RESOLVER] = L, [SIS_PING] = L,
    [SIS_OPCAO_SOCKET] = L, [SIS_ESPERAR_REDE] = L,
    [SIS_CONFIG_REDE] = O,                 /* solo REDE_CONSULTAR */
    [SIS_WIFI] = O,                        /* estado, redes y buscar; conectar y desconectar, no */
    [SIS_BLOQUEO] = O,                     /* estado, últimos y consultar; recargar y contadores, no */
    /* Hardware directo */
    [SIS_FREQUENCIA] = R,
    [SIS_MAPEAR_BAR] = R, [SIS_DESEJAR_BAR] = L,   /* deshacer un mapa propio, sí */
    [SIS_INYECTAR] = R,                    /* y además el permiso del servicio (D251) */
    /* Hilos y planificador */
    [SIS_HILO_CRIAR] = L, [SIS_HILO_SAIR] = L, [SIS_HILO_ESPERAR] = L, [SIS_CEDER] = L,
    [SIS_FUTEX_ESPERAR] = L, [SIS_FUTEX_ACORDAR] = L, [SIS_HILO_ID] = L,
    [SIS_PLANIFICADOR] = O,                /* consultar; afinidad y nice >= 0 de los suyos */
    /* Servicios y usuarios */
    [SIS_SERVICOS] = O,                    /* consultar y pedir; ser el supervisor, no */
    [SIS_USUARIO] = O,                     /* consultar; cambiar de identidad, no */
};

int permisos_classe(uint32_t n)
{
    return n < SIS_MAX ? classe[n] : PERM_SEM_CLASSE;
}

/* ---------------------------------------------------------------- identidad al arrancar */

static uint32_t uid_inicial = UID_SAGUI;
static bool rescate, sesion_raiz;

void permisos_iniciar(void)
{
    rescate = (info_boot.opcoes & OPC_BOOT_RESCATE) != 0;
    sesion_raiz = (info_boot.opcoes & OPC_BOOT_SESION_RAIZ) != 0;
    uid_inicial = rescate || sesion_raiz ? UID_ROOT : UID_SAGUI;
    klog("[PERMISOS] sesion como %s%s%s\n", uid_inicial == 0 ? "root" : "sagui",
         rescate ? " (MODO RESCATE)" : "", sesion_raiz ? " (imagen de prueba)" : "");
    /* Autoprueba barata: una llamada de la tabla del despachador sin clase
     * quedaría cerrada para todos menos root; se avisa en el registro. */
    for (uint32_t n = 1; n < SIS_MAX; n++)
        if (sistema_tem_chamada(n) && classe[n] == PERM_SEM_CLASSE)
            klog("[PERMISOS] aviso: la llamada %u no esta clasificada (solo root)\n", n);
}

uint32_t permisos_uid_inicial(void) { return uid_inicial; }
bool permisos_modo_rescate(void) { return rescate; }
bool permisos_sesion_raiz(void) { return sesion_raiz; }

/* ---------------------------------------------------------------- registro con límite */

#define ANOTAR_JANELA_S 10    /* como mucho ANOTAR_MAX líneas cada 10 s */
#define ANOTAR_MAX      20

static uint32_t janela_ini, anotadas, caladas;

/* ¿Se puede anotar otra línea en esta ventana de tiempo? */
static bool pode_anotar(void)
{
    uint32_t agora = relogio_tiques();
    if (agora - janela_ini >= ANOTAR_JANELA_S * TIQUES_POR_SEGUNDO) {
        if (caladas) klog("[PERMISOS] %u denegaciones mas sin anotar\n", caladas);
        janela_ini = agora;
        anotadas = caladas = 0;
    }
    if (anotadas >= ANOTAR_MAX) { caladas++; return false; }
    anotadas++;
    return true;
}

void permisos_anotar_negacao(const char *que, uint32_t n, uint32_t op)
{
    if (!pode_anotar()) return;
    processo_t *p = processo_recursos();
    klog("[PERMISOS] denegado a %s (pid %d, uid %u): %s %u op %d\n", p->nome, p->pid, p->uid, que, n, (int)op);
}

/* ---------------------------------------------------------------- operaciones */

/* ¿Es "pid" (0 = yo) un proceso del mismo usuario? Si no existe, sí: la
 * llamada dará el error de siempre. */
static bool do_mesmo_usuario(uint32_t pid, const processo_t *eu)
{
    if (pid == 0) return true;
    processo_t *p = processo_por_pid((int)pid);
    if (!p) return true;
    return processo_lider(p)->uid == eu->uid;
}

static bool operacao_livre(uint32_t n, uint32_t a, uint32_t b, const processo_t *eu)
{
    switch (n) {
    case SIS_MATAR:
        return do_mesmo_usuario(a, eu);
    case SIS_INFO_SISTEMA:
        return a != INFO_ACEL2D_CTL && a != INFO_ACEL2D_PROVA && a != INFO_MSR_PROVA;
    case SIS_HORA:
        return a == HORA_UTC || a == HORA_LER_RTC;
    case SIS_ZONA:
        return a == ZONA_LER;
    case SIS_REINICIAR:
    case SIS_DESLIGAR:
        return !eu->de_servico;
    case SIS_AUDIO_HDA:
        return a == HDA_OP_ESTADO || (a == HDA_OP_AUTOSILENCIO && (int32_t)b == -1);
    case SIS_DISCO_BLOCOS:
        return a == DBLOCO_INFO;
    case SIS_CONFIG_REDE:
        return a == REDE_CONSULTAR;
    case SIS_WIFI:
        return a == WIFI_ESTADO || a == WIFI_REDES || a == WIFI_BUSCAR;
    case SIS_BLOQUEO:
        return a == BLOQ_ESTADO || a == BLOQ_ULTIMOS || a == BLOQ_CONSULTAR;
    case SIS_PLANIFICADOR:
        /* b es el pid (o 0) en AFINIDADE y PRIORIDADE: aquí llega como "b"
         * porque el primer argumento es la operación. */
        if (a == PLAN_CONSULTAR || a == PLAN_NOME || a == PLAN_INFO) return true;
        return false;
    case SIS_SERVICOS:
        return a == SERV_APAGANDO || a == SERV_SUPERVISOR || a == SERV_PEDIR || a == SERV_RESULTADO;
    case SIS_USUARIO:
        return a == USU_CONSULTAR || a == USU_DE_PID || a == USU_ESTADO;
    }
    return false;
}

int permisos_chamada(uint32_t n, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    processo_t *eu = processo_recursos();
    if (eu->uid == 0 || !eu->usuario) return 0;   /* root y las tareas del kernel */
    int k = n < SIS_MAX ? classe[n] : PERM_SEM_CLASSE;
    if (k == PERM_LIVRE) return 0;
    if (k == PERM_OPERACAO) {
        /* El planificador mira también el pid y el valor: los suyos, y sin
         * subir la prioridad (nice negativo), como en Unix. */
        if (n == SIS_PLANIFICADOR && (a == PLAN_AFINIDADE || a == PLAN_PRIORIDADE)) {
            if (a == PLAN_AFINIDADE && (int32_t)c == -1) return 0;   /* solo consultar */
            if (!do_mesmo_usuario(b, eu)) goto nega;
            if (a == PLAN_AFINIDADE) return 0;
            if ((int32_t)c >= 0) return 0;
            goto nega;
        }
        if (operacao_livre(n, a, b, eu)) return 0;
    }
nega:
    permisos_anotar_negacao("llamada", n, a);
    return ERRO_ACESSO;
}

/* ---------------------------------------------------------------- rutas */

/* ¿"c" es "pre" o está dentro? Sin distinguir mayúsculas (FAT32 no las distingue). */
static bool dentro(const char *c, const char *pre)
{
    size_t i = 0;
    for (; pre[i]; i++) {
        char x = c[i], y = pre[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return c[i] == 0 || c[i] == '/' || pre[i - 1] == '/';
}

/* Lo mismo, pero vale cualquier nombre que empiece por "pre" (el temporal de
 * una escritura atómica, "sombra.tmp", cuenta como la sombra). */
static bool comeca(const char *c, const char *pre)
{
    for (size_t i = 0; pre[i]; i++) {
        char x = c[i], y = pre[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

/* Provisional (fase 1): sin permisos de ficheros, unas pocas rutas fijas.
 *   - no se leen: la sombra (contraseñas), la clave privada del sshd, la
 *     contraseña del VNC y nada de /sistema (la partición SAGUI-CONF);
 *   - no se escriben: /etc (la configuración del sistema, también la de los
 *     servicios que corren como root) y /bin (sus programas), y las órdenes
 *     al gestor de servicios (/tmp/.servicios/pedidos).
 * Las rutas llegan resueltas: sin "..", "." ni barras repetidas. */
int permisos_ruta(const char *caminho, int acao)
{
    processo_t *eu = processo_recursos();
    if (eu->uid == 0 || !eu->usuario) return 0;
    if (comeca(caminho, CONTAS_ARQ_SOMBRA) || dentro(caminho, "/home/.ssh/ssh_host_ed25519_key") ||
        dentro(caminho, "/etc/ssh/ssh_host_ed25519_key") || dentro(caminho, "/etc/vnc/vncd.passwd") ||
        dentro(caminho, "/home/.config/vncd.passwd") || dentro(caminho, "/sistema"))
        goto nega;
    if (acao & PERM_ESCREVER) {
        if (dentro(caminho, "/etc") || dentro(caminho, "/bin") || dentro(caminho, "/tmp/.servicios/pedidos"))
            goto nega;
    }
    return 0;
nega:
    if (pode_anotar())
        klog("[PERMISOS] denegado a %s (pid %d, uid %u): %s %s\n", eu->nome, eu->pid, eu->uid,
             acao & PERM_ESCREVER ? "escribir" : "leer", caminho);
    return ERRO_ACESSO;
}
