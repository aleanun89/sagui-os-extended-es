// SPDX-License-Identifier: GPL-2.0-or-later
/* permisos.c - quién puede hacer qué (D255 y D282, mejora 098, fases 1 y 2).
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
 *     resueltas, por la misma razón: desde D282 con los permisos de cada archivo
 *     (dueño, grupo, rwx y el pegajoso; permisos_acesso, permisos_criar,
 *     permisos_remover) y, además, unos pocos secretos por su ruta (permisos_ruta).
 * Cada negativa se anota en el registro del kernel, con un límite para no
 * llenarlo (y con él /logs). */
#include "permisos.h"
#include "ocio.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "caminho.h"
#include "conf_sistema.h"
#include "memoria.h"
#include "processo.h"

#define L PERM_LIVRE
#define R PERM_ROOT
#define O PERM_OPERACAO

/* Cada llamada de abi.h. Ver la tabla comentada en docs/decisoes.md (D255). */
static const uint8_t classe[SIS_MAX] = {
    /* Procesos */
    [SIS_EXECUTAR] = L, [SIS_ESPERAR] = L, [SIS_SAIR] = L, [SIS_PID] = L,
    [SIS_MATAR] = O,                       /* los suyos (mismo uid) */
    /* D401: fork, exec, waitpid, señales (kill mira el uid dentro), poll y fcntl */
    [SIS_BIFURCAR] = L, [SIS_SUBSTITUIR] = L, [SIS_ESPERAR_PID] = L, [SIS_SINAL] = L,
    [SIS_SINAL_VOLTAR] = L, [SIS_SONDAR] = L, [SIS_DESCRITOR] = L,
    /* Ficheros: los permisos de cada ruta se miran dentro (D282) */
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
    [SIS_CRIAR_WIDGET] = L,                /* D365: ventana de escritorio, como cualquier ventana */
    [SIS_IDIOMA] = L,                      /* D313: el idioma es preferencia del usuario */
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
    [SIS_PORTATIL] = O,                    /* D403: estado, brillo y acciones; los avisos simulados y las tablas, root */
    [SIS_TELAS] = O,                       /* D290: varias pantallas; D371: guardarlas es de la máquina */
    [SIS_FUNDO] = L, [SIS_ACENTO] = L, [SIS_ESQUEMA] = L, [SIS_CURSOR] = L,
    [SIS_PONTE] = L,                       /* D485: cada cuenta sus unidades de red; quién puede qué se mira dentro */
    [SIS_CHAVEIRO] = L,                    /* D485: la clave del llavero de la propia cuenta (root, la de cualquiera) */
    [SIS_ALEATORIO] = L, [SIS_INFO_ALEATORIO] = L,
    /* Sonido */
    [SIS_BIP] = L, [SIS_AUDIO_ABRIR] = L, [SIS_AUDIO_ESCREVER] = L, [SIS_AUDIO_CONTROLE] = L,
    [SIS_AUDIO_FECHAR] = L,
    [SIS_AUDIO_HDA] = O,                   /* el estado sí; verbos, pines y GPIO del códec, no */
    /* Discos y montajes */
    [SIS_LISTAR_DISCOS] = L,
    [SIS_MONTAR] = R,
    [SIS_DESMONTAR] = L,                   /* D336: "sync" (vaciar cachés) es libre; desmontar se niega dentro (discos.c) */
    [SIS_DISCO_BLOCOS] = O,                /* solo DBLOCO_INFO; leer en crudo saltaría todas las protecciones */
    /* Red: usar la red es libre; configurarla, no */
    [SIS_SOCKET] = O,                      /* D363: TCP y UDP libres; SOCK_TUN (la VPN), solo root (D372) */
    [SIS_CONECTAR] = L, [SIS_ESCUTAR] = L, [SIS_ACEITAR] = L,
    [SIS_ENVIAR_PARA] = L, [SIS_RECEBER_DE] = L, [SIS_RESOLVER] = L, [SIS_PING] = L,
    [SIS_OPCAO_SOCKET] = L, [SIS_ESPERAR_REDE] = L,
    [SIS_CONFIG_REDE] = O,                 /* solo REDE_CONSULTAR */
    [SIS_WIFI] = O,                        /* estado, redes y buscar, libres; conectar y desconectar, solo la sesión local (D354) */
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
    [SIS_BLUETOOTH] = O,                   /* D279: estado, dispositivos y buscar; el resto, no */
    [SIS_PERMISOS_ARQ] = L,                /* D282: quién puede cambiar qué se mira dentro */
};

int permisos_classe(uint32_t n)
{
    return n < SIS_MAX ? classe[n] : PERM_SEM_CLASSE;
}

/* ---------------------------------------------------------------- identidad al arrancar */

static uint32_t uid_inicial = UID_SAGUI;
static bool rescate, sesion_raiz;
/* D340: la sesión local (quién está delante del escritorio). Al arrancar, la de uid_inicial; el inicio de
 * sesión (kernel/gui/sessao.c) la cambia. sessao_casa vacía = /home tal cual (sagui, root). */
static uint32_t sessao_uid = UID_SAGUI, sessao_gid = GID_SAGUI;
static char sessao_casa[40];

void permisos_iniciar(void)
{
    rescate = (info_boot.opcoes & OPC_BOOT_RESCATE) != 0;
    sesion_raiz = (info_boot.opcoes & OPC_BOOT_SESION_RAIZ) != 0;
    uid_inicial = rescate || sesion_raiz ? UID_ROOT : UID_SAGUI;
    sessao_uid = uid_inicial;
    sessao_gid = uid_inicial == UID_ROOT ? GID_ROOT : GID_SAGUI;
    sessao_casa[0] = 0;
    klog("[PERMISOS] sesion como %s%s%s\n", uid_inicial == 0 ? "root" : "sagui",
         rescate ? " (MODO RESCATE)" : "", sesion_raiz ? " (imagen de prueba)" : "");
    /* Autoprueba barata: una llamada de la tabla del despachador sin clase
     * quedaría cerrada para todos menos root; se avisa en el registro. */
    for (uint32_t n = 1; n < SIS_MAX; n++)
        if (sistema_tem_chamada(n) && classe[n] == PERM_SEM_CLASSE)
            klog("[PERMISOS] aviso: la llamada %u no esta clasificada (solo root)\n", n);
}

uint32_t permisos_uid_inicial(void) { return sessao_uid; }
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
        if (a == INFO_OCIO_CTL) return ocio_op_libre(b);   /* D422: recargar, no molestar, actividad y probar; simular y batería simulada, solo el administrador */
        return a != INFO_ACEL2D_CTL && a != INFO_ACEL2D_PROVA && a != INFO_MSR_PROVA &&
               a != INFO_GPU3D_CTL && a != INFO_GPU3D_PROVA && a != INFO_CAB_OPC &&
               a != INFO_KCONF_GRAVAR && a != INFO_GPU_ENERGIA_CTL;   /* D399: cambiar relojes de la GPU */   /* D353: kconfig solo lee sin ser administrador */   /* D284/D285: encender y probar el 3D tocan la GPU */
    case SIS_HORA:
        return a == HORA_UTC || a == HORA_LER_RTC;
    case SIS_ZONA:
        return a == ZONA_LER;
    case SIS_REINICIAR:
    case SIS_DESLIGAR:
        return !eu->de_servico;
    case SIS_AUDIO_HDA:
        /* D328: con la SB16 la operacion 0x40 es solo lectura y 0x41 es el mezclador (niveles 0-100):
         * sin ellas el usuario normal no podia ver la tarjeta ni cambiar el volumen. */
        return a == HDA_OP_ESTADO || a == SB16_OP_ESTADO || a == SB16_OP_CANAL ||
               (a == HDA_OP_AUTOSILENCIO && (int32_t)b == -1);
    case SIS_DISCO_BLOCOS:
        return a == DBLOCO_INFO;
    case SIS_CONFIG_REDE:
        /* D363/D372: apagar el túnel (wg-quick down) sí lo puede la sesión local sin elevar: apagar
         * devuelve el tráfico a la red física, que es lo seguro (encender, en cambio, es de root). */
        return a == REDE_CONSULTAR || (a == REDE_TUNEL_DESLIGAR && !eu->de_servico);
    case SIS_SOCKET:
        /* D372: crear la interfaz del túnel (encender la VPN) es solo de root: desvía el tráfico y el
         * DNS de todo el sistema. La sesión local lo pide con el diálogo de administrador (wg-quick,
         * la ventana de la VPN) y el demonio corre como root. TCP y UDP siguen libres. */
        return a != SOCK_TUN;
    case SIS_WIFI:
        /* D354: conectar y desconectar el Wi-Fi, como apagar: la sesión local sí (sin ser root);
         * un servicio o una sesión SSH que no es de root, no. */
        return a == WIFI_ESTADO || a == WIFI_REDES || a == WIFI_BUSCAR ||
               ((a == WIFI_CONECTAR || a == WIFI_DESCONECTAR) && !eu->de_servico);
    case SIS_BLUETOOTH:
        return a == BT_ESTADO || a == BT_DISPOSITIVOS || a == BT_BUSCAR;
    case SIS_PORTATIL:
        /* D403: el brillo y las acciones (volumen, radio) son como las teclas: de la sesión, no de un
         * servicio; simular avisos del firmware y leer las tablas ACPI, solo root. */
        return a == PORTATIL_ESTADO || ((a == PORTATIL_BRILHO || a == PORTATIL_ACAO) && !eu->de_servico);
    case SIS_TELAS:
        /* D371 (P6): las pantallas son de la máquina: cambiarlas para la sesión, cualquiera; guardarlas en
         * /etc (con la configuración de la máquina separada), el administrador. */
        return a != TELAS_OP_GUARDAR || !conf_sistema_separada();
    case SIS_BLOQUEO:
        return a == BLOQ_ESTADO || a == BLOQ_ULTIMOS || a == BLOQ_CONSULTAR;
    case SIS_PLANIFICADOR:
        /* b es el pid (o 0) en AFINIDADE y PRIORIDADE: aquí llega como "b"
         * porque el primer argumento es la operación. */
        /* D436: PLAN_AUDIO, sí: solo vale para el hilo que llama y el planificador la quita a quien gasta mucho */
        if (a == PLAN_CONSULTAR || a == PLAN_NOME || a == PLAN_INFO || a == PLAN_AUDIO) return true;
        return false;
    case SIS_SERVICOS:
        return a == SERV_APAGANDO || a == SERV_SUPERVISOR || a == SERV_PEDIR || a == SERV_RESULTADO;
    case SIS_USUARIO:
        return a == USU_CONSULTAR || a == USU_DE_PID || a == USU_ESTADO || a == USU_SESSAO ||
               a == USU_CERRAR_SESION;   /* D340: cerrar la sesión lo comprueba usuarios.c (solo la local) */
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

/* Secretos que se protegen también por la ruta (D255, y D282 los mantiene): en un
 * /home FAT32 todo es de sagui y no se pueden expresar con permisos, y así una
 * escritura atómica a medias ("sombra.tmp") o un archivo creado con una máscara
 * equivocada tampoco se leen.
 *   - no se leen: la sombra (contraseñas), la clave privada del sshd y la
 *     contraseña del VNC (en /etc o, sin SAGUI-CONF, en /home);
 *   - no se escriben: las órdenes al gestor de servicios (/tmp/.servicios/pedidos),
 *     que pueden estar en un /tmp FAT32 donde todos escriben. */
int permisos_ruta(const char *caminho, int acao)
{
    processo_t *eu = processo_recursos();
    if (!eu || eu->uid == 0 || !eu->usuario || !acao) return 0;   /* ver sus datos (stat) sí */
    if (comeca(caminho, CONTAS_ARQ_SOMBRA) || dentro(caminho, "/home/.ssh/ssh_host_ed25519_key") ||
        dentro(caminho, "/etc/ssh/ssh_host_ed25519_key") || dentro(caminho, "/etc/vnc/vncd.passwd") ||
        dentro(caminho, "/home/.config/vncd.passwd"))
        goto nega;
    if ((acao & PERM_ESCREVER) && dentro(caminho, "/tmp/.servicios/pedidos"))
        goto nega;
    /* D313: /sistema es 0711 (se atraviesa para llegar a /sistema/idiomas, legible por todos); la
     * configuración montada (/sistema/conf: sombra, claves) sigue cerrada a quien no es root */
    if (dentro(caminho, "/sistema/conf"))
        goto nega;
    return 0;
nega:
    if (pode_anotar())
        klog("[PERMISOS] denegado a %s (pid %d, uid %u): %s %s (secreto)\n", eu->nome, eu->pid, eu->uid,
             acao & PERM_ESCREVER ? "escribir" : "leer", caminho);
    return ERRO_ACESSO;
}

/* ---------------------------------------------------------------- D340: varias cuentas
 *
 * La carpeta personal de una cuenta que no es la principal (sagui) es /home/<nombre>, pero sus
 * programas la ven en /home: todo lo que piden dentro de /home (también /home/.config, que usan más
 * de cien rutas fijas de los programas y de los ports) va a su carpeta. Así cada cuenta tiene su
 * configuración sin cambiar los programas, y no puede ni nombrar los archivos de las demás. Root
 * (también un programa elevado) ve el árbol real. La carpeta actual se guarda como la ve el
 * programa: se traduce en cada llamada. */

static bool nome_casa_valido(const char *c)
{
    if (txt_compara_n(c, "/home/", 6) != 0) return false;
    const char *n = c + 6;
    size_t k = 0;
    for (; n[k]; k++)
        if (!((n[k] >= 'a' && n[k] <= 'z') || (n[k] >= '0' && n[k] <= '9') || n[k] == '_' || n[k] == '-')) return false;
    return k > 0 && k < 32;
}

bool permisos_casa_valida(const char *casa) { return !casa[0] || nome_casa_valido(casa); }

void permisos_fixar_sessao(uint32_t uid, uint32_t gid, const char *casa)
{
    sessao_uid = uid;
    sessao_gid = gid;
    sessao_casa[0] = 0;
    if (casa && nome_casa_valido(casa)) txt_copia_n(sessao_casa, casa, sizeof sessao_casa);
    klog("[SESION] sesion local: uid %u gid %u, carpeta %s\n", uid, gid, sessao_casa[0] ? sessao_casa : "/home");
}

uint32_t permisos_sessao_gid(void) { return sessao_gid; }
const char *permisos_sessao_casa(void) { return sessao_casa; }

/* La ruta real de "rel" (que empieza por '/', o "") dentro de la carpeta de la sesión. */
void permisos_sessao_caminho(char *dst, size_t n, const char *rel)
{
    snformatar(dst, n, "%s%s", sessao_casa[0] ? sessao_casa : "/home", rel);
}

static bool em_home_c(const char *c) { return txt_compara_n(c, "/home", 5) == 0 && (c[5] == 0 || c[5] == '/'); }

/* D371 (P8): /home/compartido, la carpeta común (root, 1777), es la misma para todas las cuentas: no se traduce
 * a la carpeta de cada una. Solo cuando /home ya es de root (sagui en /home/sagui): antes, era una más de sagui. */
#define CASA_COMUM "/home/compartido"
static bool em_comum(const char *c)
{
    size_t n = sizeof CASA_COMUM - 1;
    return vfs_home_e_de_root() && txt_compara_n(c, CASA_COMUM, n) == 0 && (c[n] == 0 || c[n] == '/');
}

/* "/home/x" -> casa + "/x" (en el sitio; CAMINHO_MAX). false si no cabe. */
static bool traduzir_com(const char *casa, char *caminho)
{
    if (!casa[0] || !em_home_c(caminho) || em_comum(caminho)) return true;
    size_t nc = txt_tamanho(casa), nr = txt_tamanho(caminho + 5);
    if (nc + nr + 1 > CAMINHO_MAX) return false;
    memmove(caminho + nc, caminho + 5, nr + 1);
    memcpy(caminho, casa, nc);
    return true;
}

/* D371 (P14): /root se guarda en SAGUI-CONF: "/root/x" -> CONF_RAIZ "/x" para todos los procesos (el permiso lo
 * dan los de la carpeta, 0700 de root). Sin SAGUI-CONF, /root es la carpeta de la RAM. */
static bool traduzir_raiz(char *caminho)
{
    if (!conf_sistema_raiz_persistente() || txt_compara_n(caminho, "/root", 5) != 0 || (caminho[5] && caminho[5] != '/'))
        return true;
    size_t nc = sizeof CONF_RAIZ - 1, nr = txt_tamanho(caminho + 5);
    if (nc + nr + 1 > CAMINHO_MAX) return false;
    memmove(caminho + nc, caminho + 5, nr + 1);
    memcpy(caminho, CONF_RAIZ, nc);
    return true;
}

bool permisos_casa_traduzir(const processo_t *p, char *caminho)
{
    if (!p || !p->usuario) return true;
    if (!traduzir_raiz(caminho)) return false;
    const processo_t *l = processo_lider((processo_t *)p);
    if (l->uid == 0) {
        /* D371 (P2 b): un programa elevado con el diálogo o «admin» desde una cuenta ve el árbol real (para
         * administrar, /home/ana es /home/ana), pero su configuración (/home/.algo: .config, .ssh...) sigue
         * siendo la de esa cuenta. «su» suelta la carpeta (USU_CASA_SOLTAR): root del todo, con /root. */
        if (l->casa_conta[0] && txt_compara_n(caminho, "/home/.", 7) == 0) return traduzir_com(l->casa_conta, caminho);
        return true;
    }
    return traduzir_com(l->casa_conta, caminho);
}

/* D371 (fase 4): la carpeta de una cuenta según /etc/usuarios ("" si es /home tal cual, root, o no se sabe). Así
 * todo proceso de una cuenta (también los que lanza un servicio, cron, SSH o «su ana») ve su carpeta en /home,
 * no solo los de la sesión local. */
void permisos_casa_de_uid(uint32_t uid, char *casa, size_t n)
{
    casa[0] = 0;
    if (uid == 0 || n < 2) return;
    uint8_t *d;
    uint32_t tam;
    if (vfs_ler_inteiro("/etc/usuarios", &d, &tam) < 0) return;
    const char *p = (const char *)d, *fim = p + tam;
    while (p < fim) {
        const char *l = p, *le = p;
        while (le < fim && *le != '\n') le++;
        p = le < fim ? le + 1 : le;
        if (le > l && le[-1] == '\r') le--;
        if (l >= le || *l == '#') continue;
        const char *c[5];
        size_t k[5] = { 0 };
        int nc = 0;
        for (const char *q = l; q <= le && nc < 5;) {   /* nombre:uid:gid:carpeta:... */
            const char *e = q;
            while (e < le && *e != ':') e++;
            c[nc] = q; k[nc] = (size_t)(e - q); nc++;
            q = e + 1;
        }
        if (nc < 4 || !k[1]) continue;
        uint32_t u = 0;
        bool ok = true;
        for (size_t i = 0; i < k[1]; i++) { if (c[1][i] < '0' || c[1][i] > '9') ok = false; else u = u * 10 + (uint32_t)(c[1][i] - '0'); }
        if (!ok || u != uid) continue;
        char x[40];
        if (k[3] >= sizeof x) break;
        memcpy(x, c[3], k[3]);
        x[k[3]] = 0;
        if (nome_casa_valido(x) && txt_tamanho(x) < n) txt_copia_n(casa, x, n);
        break;
    }
    kfree(d);
}

/* Una carpeta real a como la ve un proceso con la carpeta "casa". */
static void real_a_vista(const char *casa, char *pasta, size_t n)
{
    if (!casa[0]) return;
    size_t nc = txt_tamanho(casa);
    char c[CAMINHO_MAX];
    if (txt_compara_n(pasta, casa, nc) == 0 && (pasta[nc] == 0 || pasta[nc] == '/')) {
        snformatar(c, sizeof c, "/home%s", pasta + nc);
        txt_copia_n(pasta, c, n);
    } else if (em_home_c(pasta) && !em_comum(pasta)) {
        txt_copia_n(pasta, "/", n);   /* otra carpeta de /home: la cuenta no la ve */
    }
}

void permisos_pasta_herdada(processo_t *p, const processo_t *pai)
{
    const processo_t *lp = processo_lider((processo_t *)pai);
    if (txt_compara(p->casa_conta, lp->casa_conta) == 0) return;
    char c[CAMINHO_MAX];
    txt_copia_n(c, p->pasta, sizeof c);
    if (lp->uid != 0 && !traduzir_com(lp->casa_conta, c)) txt_copia_n(c, "/", sizeof c);   /* la vista del padre, a la real */
    real_a_vista(p->casa_conta, c, sizeof c);
    txt_copia_n(p->pasta, c, sizeof p->pasta);
}

/* El proceso (líder) pasa de su uid a "novo": su carpeta actual sigue siendo la misma carpeta real
 * (root ve el árbol real; la cuenta, su /home). */
void permisos_casa_mudar_uid(processo_t *l, uint32_t novo)
{
    if (novo != 0) {   /* D371 (fase 4): a una cuenta, la carpeta de esa cuenta */
        char casa[sizeof l->casa_conta];
        permisos_casa_de_uid(novo, casa, sizeof casa);
        if (casa[0] && txt_compara(casa, l->casa_conta) != 0) {
            char c[CAMINHO_MAX];
            txt_copia_n(c, l->pasta, sizeof c);
            if (l->uid != 0 && !traduzir_com(l->casa_conta, c)) txt_copia_n(c, "/", sizeof c);
            txt_copia_n(l->casa_conta, casa, sizeof l->casa_conta);
            real_a_vista(casa, c, sizeof c);
            txt_copia_n(l->pasta, c, sizeof l->pasta);
            return;
        }
    }
    if (!l->casa_conta[0] || (l->uid == 0) == (novo == 0)) return;
    char c[CAMINHO_MAX];
    size_t nc = txt_tamanho(l->casa_conta);
    if (novo == 0) {   /* a root: la vista de la cuenta pasa a la real */
        txt_copia_n(c, l->pasta, sizeof c);
        if (traduzir_com(l->casa_conta, c)) txt_copia_n(l->pasta, c, sizeof l->pasta);
        else txt_copia_n(l->pasta, "/", sizeof l->pasta);
    } else if (txt_compara_n(l->pasta, l->casa_conta, nc) == 0 && (l->pasta[nc] == 0 || l->pasta[nc] == '/')) {
        snformatar(c, sizeof c, "/home%s", l->pasta + nc);
        txt_copia_n(l->pasta, c, sizeof l->pasta);
    } else if (em_home_c(l->pasta) && !em_comum(l->pasta)) {
        txt_copia_n(l->pasta, "/", sizeof l->pasta);   /* otra carpeta de /home: la cuenta no la ve */
    }
}

/* ---------------------------------------------------------------- D282: permisos de archivos */

void permisos_dono_novo(bool pasta, uint32_t *uid, uint32_t *gid, uint16_t *modo)
{
    processo_t *eu = processo_recursos();
    bool kernel = !eu || !eu->usuario;
    *uid = kernel ? UID_ROOT : eu->uid;
    *gid = kernel ? GID_ROOT : eu->gid;
    uint16_t mascara = kernel ? UMASK_PADRAO : eu->umask;
    *modo = (uint16_t)((pasta ? 0777 : 0666) & ~mascara);
}

/* ¿Tiene "eu" los permisos "quer" (PERM_*) sobre algo con estos dueños y modo? */
static bool pode(const processo_t *eu, const meta_t *m, int quer)
{
    uint32_t bits = eu->uid == m->uid ? (uint32_t)m->modo >> 6 : eu->gid == m->gid ? (uint32_t)m->modo >> 3 : m->modo;
    return ((bits & 7) & (uint32_t)quer) == (uint32_t)quer;
}

static bool livre_de_controle(const processo_t *eu) { return !eu || eu->uid == 0 || !eu->usuario; }

static int negar(const processo_t *eu, const char *que, const char *caminho)
{
    if (pode_anotar())
        klog("[PERMISOS] denegado a %s (pid %d, uid %u): %s %s\n", eu->nome, eu->pid, eu->uid, que, caminho);
    return ERRO_ACESSO;
}

/* Entrar (x) en "/" y en cada carpeta de la ruta hasta "fim" bytes (sin incluir
 * el último nombre). */
static int recorrer(const processo_t *eu, const char *caminho, size_t fim)
{
    char parcial[CAMINHO_MAX];
    meta_t m;
    for (size_t i = 0; i < fim; i++) {
        if (caminho[i] != '/') continue;
        size_t n = i ? i : 1;   /* "/" para la raíz */
        if (n >= sizeof parcial) return ERRO_INVALIDO;
        memcpy(parcial, caminho, n);
        parcial[n] = 0;
        if (vfs_meta_rapida(parcial, &m) < 0) return 0;   /* no existe: la operación fallará sola */
        if (!pode(eu, &m, PERM_EXECUTAR)) return negar(eu, "entrar en", parcial);
    }
    return 0;
}

/* Bytes de la carpeta de "caminho" ("/a/b" -> 2, "/a" -> 0: la raíz). */
static size_t fim_da_pasta(const char *caminho)
{
    size_t ult = 0;
    for (size_t i = 0; caminho[i]; i++)
        if (caminho[i] == '/') ult = i;
    return ult;
}

int permisos_acesso(const char *caminho, int quer)
{
    processo_t *eu = processo_recursos();
    if (livre_de_controle(eu)) return 0;
    int r = permisos_ruta(caminho, quer);
    if (r < 0) return r;
    if (caminho[0] == '/' && caminho[1]) {
        if ((r = recorrer(eu, caminho, fim_da_pasta(caminho) + 1)) < 0) return r;
    }
    if (!quer) return 0;
    meta_t m;
    if (vfs_meta(caminho, &m) < 0) return 0;
    if (!pode(eu, &m, quer))
        return negar(eu, quer & PERM_ESCREVER ? "escribir" : quer & PERM_LER ? "leer" : "ejecutar o entrar en", caminho);
    return 0;
}

/* La carpeta de "caminho" en "pasta" ("/" para la raíz). */
static void pasta_de(const char *caminho, char *pasta)
{
    size_t n = fim_da_pasta(caminho);
    if (n >= CAMINHO_MAX) n = CAMINHO_MAX - 1;
    if (!n) { pasta[0] = '/'; pasta[1] = 0; return; }
    memcpy(pasta, caminho, n);
    pasta[n] = 0;
}

int permisos_criar(const char *caminho)
{
    processo_t *eu = processo_recursos();
    if (livre_de_controle(eu)) return 0;
    int r = permisos_ruta(caminho, PERM_ESCREVER);
    if (r < 0) return r;
    char pasta[CAMINHO_MAX];
    pasta_de(caminho, pasta);
    return permisos_acesso(pasta, PERM_ESCREVER | PERM_EXECUTAR);
}

int permisos_remover(const char *caminho)
{
    processo_t *eu = processo_recursos();
    if (livre_de_controle(eu)) return 0;
    int r = permisos_criar(caminho);
    if (r < 0) return r;
    char pasta[CAMINHO_MAX];
    pasta_de(caminho, pasta);
    meta_t mp, mx;
    /* D371: /home es de sagui (D340), así que sin esto sagui podía renombrar o quitar la carpeta de otra
     * cuenta (/home/ana, aunque sea 0700: basta escribir en /home) y dejar en su sitio una suya con lo que
     * quisiera dentro. Lo que cuelga directamente de /home solo lo borra o renombra su dueño (o root). */
    if (dentro(pasta, "/home") && !pasta[5] && vfs_meta(caminho, &mx) == 0 && mx.uid != eu->uid)
        return negar(eu, "borrar o renombrar (carpeta de otra cuenta)", caminho);
    if (vfs_meta(pasta, &mp) < 0 || !(mp.modo & MODO_PEGAJOSO) || mp.uid == eu->uid) return 0;
    if (vfs_meta(caminho, &mx) < 0 || mx.uid == eu->uid) return 0;
    return negar(eu, "borrar (carpeta pegajosa)", caminho);
}

/* ---------------------------------------------------------------- SIS_PERMISOS_ARQ */

static int caminho_do_usuario(uint32_t ptr, char *saida)
{
    char bruto[CAMINHO_MAX];
    int r = usuario_texto((const char *)ptr, bruto, sizeof bruto);
    if (r < 0) return r;
    if (r == 0) return ERRO_INVALIDO;
    if (!caminho_resolver(processo_recursos()->pasta, bruto, saida, CAMINHO_MAX)) return ERRO_INVALIDO;
    if (!permisos_casa_traduzir(processo_recursos(), saida)) return ERRO_INVALIDO;   /* D340 */
    return 0;
}

int32_t sis_permisos_arq(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)d;
    processo_t *eu = processo_recursos();
    if (op == PARQ_UMASK) {
        uint16_t antes = eu->umask;
        if ((int32_t)a != -1) eu->umask = (uint16_t)(a & 0777);
        return antes;
    }
    if (op != PARQ_CONSULTAR && op != PARQ_MODO && op != PARQ_DONO && op != PARQ_DATAS && op != PARQ_DATAS_FD)
        return ERRO_INVALIDO;
    char caminho[CAMINHO_MAX];
    int r = op == PARQ_DATAS_FD ? arquivos_caminho(a, caminho, sizeof caminho) : caminho_do_usuario(a, caminho);
    if (r < 0) return r;

    if ((r = permisos_acesso(caminho, 0)) < 0) return r;   /* entrar en sus carpetas */
    meta_t m;
    if ((r = vfs_meta(caminho, &m)) < 0) return r;
    bool root = livre_de_controle(eu);
    if (op == PARQ_CONSULTAR) {
        info_permisos_t *v = (info_permisos_t *)b;
        if (!usuario_ok(v, sizeof *v, true)) return ERRO_ENDERECO;
        memset(v, 0, sizeof *v);
        v->uid = m.uid;
        v->gid = m.gid;
        v->modo = m.modo;
        v->tipo = m.tipo;
        v->com_donos = m.com_donos;
        return 0;
    }
    if (op == PARQ_DATAS || op == PARQ_DATAS_FD) {
        /* D341 (como utimensat): una fecha concreta, solo el dueño o root; «ahora» o «no cambiar»
         * bastan con poder escribir el archivo. Las dos «no cambiar» no hacen nada. */
        uint32_t mod = b, ace = c;
        if (mod == PARQ_DATA_MANTER && ace == PARQ_DATA_MANTER) return 0;
        bool concreta = (mod != PARQ_DATA_AGORA && mod != PARQ_DATA_MANTER) ||
                        (ace != PARQ_DATA_AGORA && ace != PARQ_DATA_MANTER);
        if (!root && m.uid != eu->uid) {
            if (concreta) return negar(eu, "cambiar las fechas de", caminho);
            if ((r = permisos_acesso(caminho, PERM_ESCREVER)) < 0) return r;
        }
        uint32_t agora = hora_utc();
        mod = mod == PARQ_DATA_AGORA ? agora : mod == PARQ_DATA_MANTER ? 0 : mod;
        ace = ace == PARQ_DATA_AGORA ? agora : ace == PARQ_DATA_MANTER ? 0 : ace;
        return vfs_fixar_datas(caminho, mod, ace);
    }
    if (op == PARQ_MODO) {

        if (!root && m.uid != eu->uid) return negar(eu, "cambiar los permisos de", caminho);
        return vfs_fixar_meta(caminho, -1, -1, (int32_t)(b & MODO_PERMISOS));
    }
    /* PARQ_DONO: el dueño solo lo cambia root; el grupo, root o el dueño (al suyo). */
    int32_t uid = (int32_t)b, gid = (int32_t)c;
    if (uid < -1 || uid > UID_MAX || gid < -1 || gid > UID_MAX) return ERRO_INVALIDO;
    if (!root) {
        if (uid != -1 && (uint32_t)uid != m.uid) return negar(eu, "cambiar el dueño de", caminho);
        if (gid != -1 && (m.uid != eu->uid || ((uint32_t)gid != eu->gid && (uint32_t)gid != m.gid)))
            return negar(eu, "cambiar el grupo de", caminho);
    }
    return vfs_fixar_meta(caminho, uid, gid, -1);
}
