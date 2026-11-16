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
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "vfs.h"
#include "caminho.h"

#define L PERM_LIVRE
#define R PERM_ROOT
#define O PERM_OPERACAO

/* Cada llamada de abi.h. Ver la tabla comentada en docs/decisoes.md (D255). */
static const uint8_t classe[SIS_MAX] = {
    /* Procesos */
    [SIS_EXECUTAR] = L, [SIS_ESPERAR] = L, [SIS_SAIR] = L, [SIS_PID] = L,
    [SIS_MATAR] = O,                       /* los suyos (mismo uid) */
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
    [SIS_TELAS] = L,                       /* D290: varias pantallas, también preferencia del usuario */
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
        return a != INFO_ACEL2D_CTL && a != INFO_ACEL2D_PROVA && a != INFO_MSR_PROVA &&
               a != INFO_GPU3D_CTL && a != INFO_GPU3D_PROVA && a != INFO_CAB_OPC;   /* D284/D285: encender y probar el 3D tocan la GPU */
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
        return a == REDE_CONSULTAR;
    case SIS_WIFI:
        return a == WIFI_ESTADO || a == WIFI_REDES || a == WIFI_BUSCAR;
    case SIS_BLUETOOTH:
        return a == BT_ESTADO || a == BT_DISPOSITIVOS || a == BT_BUSCAR;
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
    if (op != PARQ_CONSULTAR && op != PARQ_MODO && op != PARQ_DONO) return ERRO_INVALIDO;
    char caminho[CAMINHO_MAX];
    int r = caminho_do_usuario(a, caminho);
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
