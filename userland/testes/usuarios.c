// SPDX-License-Identifier: GPL-2.0-or-later
/* usuarios - prueba de los usuarios y las llamadas privilegiadas (D255; tests/h254_usuarios.py).
 *
 *   usuarios --llamadas   como el usuario que lo ejecuta (sagui), recorre TODAS las
 *                         llamadas al sistema y las operaciones clasificadas en
 *                         kernel/nucleo/permisos.c: las de root deben dar ERRO_ACESSO
 *                         y las libres no (se llaman con argumentos inofensivos, que
 *                         suelen dar otro error). La tabla de aquí es independiente de
 *                         la del kernel: si alguien cambia una, la prueba lo ve.
 *   usuarios --rutas      las rutas protegidas (sombra, /etc, /bin, /sistema...).
 *   usuarios --servicio   lanzado por el gestor de servicios (no es la sesión local):
 *                         apagar y reiniciar se niegan.
 *   usuarios --pbkdf2     vectores de PBKDF2-HMAC-SHA256 (RFC 7914, 11).
 * Cada línea va también al registro del sistema (serie) con «usuarios:». */
#include "sagui.h"
#include "contas.h"
#include "pbkdf2.h"

static int ok, mal;

static int sc(unsigned n, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return r;
}

static void anota(bool bem, const char *fmt, ...)
{
    char t[200];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    if (bem) ok++; else mal++;
    if (!bem) {
        imprimir("usuarios: MAL %s\n", t);
        registrar("usuarios: MAL %s", t);
    }
}

/* Solo root: debe dar ERRO_ACESSO. */
static void root(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int r = sc(n, a, b, c, 0, 0);
    anota(r == ERRO_ACESSO, "llamada %u op %d: esperaba ERRO_ACESSO y dio %d", n, (int)a, r);
}

/* ¿El kernel anotó que nos negó esta llamada? Algunas libres dan ERRO_ACESSO por
 * su cuenta (ventanas sin escritorio...): solo cuenta si la negó permisos.c. Las
 * libres se prueban antes que las de root, así el límite del registro no tapa nada. */
static bool negada_por_permisos(unsigned n, unsigned a)
{
    static char log[32768];
    int t = info_sistema(INFO_LOG, log, sizeof log - 1);
    if (t <= 0) return true;   /* sin registro, no se puede saber: se da por mala */
    log[t] = 0;
    char marca[96];
    formatar_texto(marca, sizeof marca, "denegado a usuarios (pid %d, uid %u): llamada %u op %d\n", meu_pid(), meu_uid(),
                   n, (int)a);
    int nm = (int)tam_texto(marca);
    for (int i = 0; i + nm <= t; i++)
        if (memcmp(log + i, marca, (size_t)nm) == 0) return true;
    return false;
}

/* Libre: no debe dar ERRO_ACESSO por permisos (con estos argumentos, cualquier otra cosa vale). */
static void livre(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int r = sc(n, a, b, c, 0, 0);
    anota(r != ERRO_ACESSO || !negada_por_permisos(n, a), "llamada %u op %d: es libre y dio ERRO_ACESSO", n, (int)a);
}

#define NAO_EXISTE 999999u   /* un pid que no hay */

static void llamadas(void)
{
    static char buf[4096];
    static int fds[2];
    unsigned b = (unsigned)buf;
    /* --- libres --- */
    livre(SIS_EXECUTAR, 0, 0, 0);              /* ruta 0: ERRO_ENDERECO */
    livre(SIS_ESPERAR, NAO_EXISTE, 0, 1);
    livre(SIS_PID, 0, 0, 0);
    livre(SIS_MATAR, NAO_EXISTE, 0, 0);        /* no existe: ERRO_SEM_PROCESSO */
    livre(SIS_ABRIR, (unsigned)"/etc/usuarios", ABRIR_LER, 0);
    livre(SIS_LER, 99, b, 1);
    livre(SIS_ESCREVER, 99, b, 1);
    livre(SIS_FECHAR, 99, 0, 0);
    livre(SIS_POSICIONAR, 99, 0, 0);
    livre(SIS_INFO_ARQUIVO, (unsigned)"/etc", b, 0);
    livre(SIS_CRIAR_PASTA, (unsigned)"/tmp/usuarios.d", 0, 0);
    livre(SIS_REMOVER_PASTA, (unsigned)"/tmp/usuarios.d", 0, 0);
    livre(SIS_APAGAR, (unsigned)"/tmp/no-existe", 0, 0);
    livre(SIS_LISTAR, (unsigned)"/etc", 0, b);
    livre(SIS_MUDAR_PASTA, (unsigned)"/", 0, 0);
    livre(SIS_PASTA_ATUAL, b, sizeof buf, 0);
    livre(SIS_RENOMEAR, (unsigned)"/tmp/no-existe", (unsigned)"/tmp/otro", 0);
    livre(SIS_TRUNCAR, 99, 0, 0);
    livre(SIS_SINCRONIZAR, 99, 0, 0);
    livre(SIS_DATAS_ARQUIVO, (unsigned)"/etc", b, 0);
    livre(SIS_PIPE, (unsigned)fds, 0, 0);
    fechar(fds[0]); fechar(fds[1]);
    livre(SIS_DUPLICAR, 99, 98, 0);
    livre(SIS_DISPONIVEL, 99, 0, 0);
    livre(SIS_TIPO_DESCRITOR, 1, 0, 0);
    livre(SIS_DORMIR, 0, 0, 0);
    livre(SIS_TEMPO, 0, 0, 0);
    livre(SIS_DATA, b, 0, 0);
    livre(SIS_AUMENTAR_MEM, 0, 0, 0);
    livre(SIS_MEMORIA, 99, 0, 0);
    livre(SIS_REGISTRAR, (unsigned)"usuarios: registrar es libre", 0, 0);
    livre(SIS_MODO_TERMINAL, TERM_LINHA, 0, 0);
    livre(SIS_ABRIR_TV, 0, 0, 0);
    livre(SIS_GRUPO, NAO_EXISTE, 0, 0);
    livre(SIS_INTERRUPCOES, 0, 0, 0);
    livre(SIS_CRIAR_JANELA, 0, 0, 0);
    livre(SIS_FECHAR_JANELA, 9999, 0, 0);
    livre(SIS_ATUALIZAR_JANELA, 9999, 0, 0);
    livre(SIS_PROXIMO_EVENTO, 9999, 0, 0);
    livre(SIS_TITULO_JANELA, 9999, 0, 0);
    livre(SIS_ESTILO_JANELA, 9999, 0, 0);
    livre(SIS_REDIMENSIONAR_JANELA, 9999, 0, 0);
    livre(SIS_PEDIR_FECHAR, 9999, 0, 0);
    livre(SIS_MOVER_JANELA, 9999, 0, 0);
    livre(SIS_MENU_CONTEXTUAL, 9999, 0, 0);
    livre(SIS_CAPTURAR_TELA, 9999, 0, 0);
    livre(SIS_AVISAR, 0, 0, 0);
    livre(SIS_COPIAR_TEXTO, b, 0, 0);
    livre(SIS_COLAR_TEXTO, b, 0, 0);
    livre(SIS_COPIAR_IMAGEM, 0, 0, 0);
    livre(SIS_COLAR_IMAGEM, 0, 0, 0);
    livre(SIS_TECLAS_SEGURADAS, b, 0, 0);
    livre(SIS_TECLADO, (unsigned)-1, 0, 0);
    livre(SIS_MODOS_VIDEO, b, 0, 0);
    livre(SIS_MUDAR_MODO, 0, 0, 0);
    livre(SIS_FUNDO, 99, 0, 0);
    livre(SIS_ACENTO, (unsigned)-1, 0, 0);
    livre(SIS_ALEATORIO, b, 16, 0);
    livre(SIS_INFO_ALEATORIO, b, 0, 0);
    livre(SIS_BIP, 0, 0, 0);
    livre(SIS_AUDIO_ABRIR, 0, 0, 0);
    livre(SIS_AUDIO_ESCREVER, 99, 0, 0);
    livre(SIS_AUDIO_CONTROLE, 99, 99, 0);
    livre(SIS_AUDIO_FECHAR, 99, 0, 0);
    livre(SIS_LISTAR_DISCOS, 99, b, 0);
    livre(SIS_SOCKET, 99, 0, 0);
    livre(SIS_CONECTAR, 99, 0, 0);
    livre(SIS_ESCUTAR, 99, 0, 0);
    livre(SIS_ACEITAR, 99, 0, 0);
    livre(SIS_ENVIAR_PARA, 99, 0, 0);
    livre(SIS_RECEBER_DE, 99, 0, 0);
    livre(SIS_RESOLVER, 0, 0, 0);
    livre(SIS_PING, 0, 0, 0);
    livre(SIS_OPCAO_SOCKET, 99, 0, 0);
    livre(SIS_ESPERAR_REDE, 0, 0, 0);
    livre(SIS_DESEJAR_BAR, 0, 0, 0);
    livre(SIS_HILO_ESPERAR, NAO_EXISTE, 0, 0);
    livre(SIS_CEDER, 0, 0, 0);
    livre(SIS_FUTEX_ESPERAR, 0, 0, 0);
    livre(SIS_FUTEX_ACORDAR, 0, 0, 0);
    livre(SIS_HILO_ID, 0, 0, 0);
    livre(SIS_PONTE, 99, 0, 0);       /* D485: unidades de red (cada cuenta las suyas) */
    livre(SIS_CHAVEIRO, 99, 0, 0);    /* D485: la clave del llavero de la propia cuenta */
    /* No se llaman (son libres, pero tienen efectos): SIS_SAIR, SIS_HILO_SAIR, SIS_HILO_CRIAR,
     * SIS_PRIMEIRO_PLANO; SIS_REINICIAR y SIS_DESLIGAR se prueban desde un servicio. */

    /* --- solo root --- */
    root(SIS_MONTAR, 0, 0, 0);
    root(SIS_DESMONTAR, (unsigned)"disco-que-no-hay", 0, 0);   /* D336: el nombre se mira dentro; "*" (sync) es libre */
    root(SIS_ACPI, ACPI_OP_ATIVAR, 0, 0);
    root(SIS_FREQUENCIA, FREQ_OP_POLITICA, 0, 0);
    for (unsigned op = 1; op <= 10; op++) root(SIS_FREQUENCIA, op, 0, 0);
    root(SIS_MAPEAR_BAR, 0, 0, 0);
    root(SIS_INYECTAR, INY_ESTADO, 1, 0);
    root(SIS_INYECTAR, INY_TECLA, 'a', 0);
    int fora = sc(SIS_MAX, 0, 0, 0, 0, 0);       /* fuera de la tabla: nunca hace nada */
    anota(fora == ERRO_INVALIDO || fora == ERRO_ACESSO, "llamada %u (fuera de la tabla): %d", SIS_MAX, fora);

    /* --- por operación --- */
    /* SIS_MATAR: un proceso de otro usuario (el gestor de servicios es root) */
    int sup = servicos_kernel(SERV_SUPERVISOR, 0);
    if (sup > 0) root(SIS_MATAR, (unsigned)sup, 0, 0);
    else anota(false, "no hay gestor de servicios para probar matar a otro usuario");
    /* SIS_INFO_SISTEMA */
    static const unsigned info_livres[] = { INFO_MEMORIA, INFO_PROCESSOS, INFO_VERSAO, INFO_USB, INFO_E820, INFO_SMBIOS,
        INFO_PCI, INFO_VIDEO, INFO_CPU_MHZ, INFO_TELA, INFO_LOG, INFO_ACPI, INFO_ARRANQUE, INFO_REDE, INFO_CONEXOES,
        INFO_ARP, INFO_INTERFACES, INFO_SOCKETS, INFO_TRAFEGO, INFO_AUDIO, INFO_FREQUENCIA, INFO_MOTOR_GPU, INFO_ACEL2D,
        INFO_IRQ_VIAS, INFO_NUCLEOS, INFO_FREQ_NUCLEOS, INFO_PILHAS, INFO_JANELAS, INFO_MONTAGENS, INFO_SENSORES,
        INFO_APARENCIA, INFO_BANDEJA, INFO_ENERGIA, INFO_BATERIA };
    for (unsigned i = 0; i < sizeof info_livres / sizeof info_livres[0]; i++) livre(SIS_INFO_SISTEMA, info_livres[i], 0, 0);
    root(SIS_INFO_SISTEMA, INFO_ACEL2D_CTL, 1, 0);
    root(SIS_INFO_SISTEMA, INFO_ACEL2D_PROVA, 0, 0);
    root(SIS_INFO_SISTEMA, INFO_GPU3D_CTL, 1, 0);      /* D284/D285 */
    root(SIS_INFO_SISTEMA, INFO_GPU3D_PROVA, 0, 0);
    root(SIS_INFO_SISTEMA, INFO_MSR_PROVA, 0, 0);
    /* SIS_HORA */
    livre(SIS_HORA, HORA_UTC, b, 0);
    livre(SIS_HORA, HORA_LER_RTC, b, 0);
    root(SIS_HORA, HORA_AJUSTAR, 0, 0);
    root(SIS_HORA, 77, 0, 0);
    /* SIS_ZONA */
    livre(SIS_ZONA, ZONA_LER, b, 0);
    root(SIS_ZONA, ZONA_FIXAR, b, 0);
    /* SIS_AUDIO_HDA */
    livre(SIS_AUDIO_HDA, HDA_OP_ESTADO, b, 0);
    livre(SIS_AUDIO_HDA, HDA_OP_AUTOSILENCIO, (unsigned)-1, 0);
    livre(SIS_AUDIO_HDA, SB16_OP_ESTADO, b, 0);   /* D328 */
    livre(SIS_AUDIO_HDA, SB16_OP_CANAL, 0, (unsigned)-1);
    root(SIS_AUDIO_HDA, HDA_OP_AUTOSILENCIO, 1, 0);
    root(SIS_AUDIO_HDA, HDA_OP_VERBO, 0, 0);
    root(SIS_AUDIO_HDA, HDA_OP_PINO, 0, 0);
    root(SIS_AUDIO_HDA, HDA_OP_GPIO, 0, 0);
    /* SIS_DISCO_BLOCOS */
    livre(SIS_DISCO_BLOCOS, DBLOCO_INFO, (unsigned)"hda", b);
    for (unsigned op = DBLOCO_RESERVAR; op <= DBLOCO_EXPULSAR; op++)
        if (op != DBLOCO_INFO) root(SIS_DISCO_BLOCOS, op, (unsigned)"hda", 0);
    /* SIS_CONFIG_REDE */
    livre(SIS_CONFIG_REDE, REDE_CONSULTAR, b, 0);
    for (unsigned op = REDE_FIXAR; op <= REDE_AJUSTAR_DRIVER; op++) root(SIS_CONFIG_REDE, op, 0, 0);
    /* SIS_WIFI */
    livre(SIS_WIFI, WIFI_ESTADO, b, 0);
    livre(SIS_WIFI, WIFI_REDES, b, 0);
    livre(SIS_WIFI, WIFI_BUSCAR, 0, 0);
    /* D354: conectar y desconectar, la sesión local sí (sin adaptador: ERRO_NAO_EXISTE, no ERRO_ACESSO) */
    livre(SIS_WIFI, WIFI_CONECTAR, 0, 0);
    livre(SIS_WIFI, WIFI_DESCONECTAR, 0, 0);
    root(SIS_WIFI, 77, 0, 0);   /* operacion desconocida: cerrada */
    /* SIS_BLOQUEO */
    livre(SIS_BLOQUEO, BLOQ_ESTADO, b, 0);
    livre(SIS_BLOQUEO, BLOQ_ULTIMOS, b, 0);
    livre(SIS_BLOQUEO, BLOQ_CONSULTAR, (unsigned)"ejemplo.com", 0);
    root(SIS_BLOQUEO, BLOQ_RECARGAR, 0, 0);
    root(SIS_BLOQUEO, BLOQ_CONTADORES, 0, 0);
    /* SIS_PLANIFICADOR */
    livre(SIS_PLANIFICADOR, PLAN_CONSULTAR, 0, 0);
    livre(SIS_PLANIFICADOR, PLAN_NOME, 0, b);
    livre(SIS_PLANIFICADOR, PLAN_INFO, 0, b);
    livre(SIS_PLANIFICADOR, PLAN_AFINIDADE, 0, (unsigned)-1);
    livre(SIS_PLANIFICADOR, PLAN_AFINIDADE, 0, 0);          /* la suya */
    livre(SIS_PLANIFICADOR, PLAN_PRIORIDADE, 0, 5);         /* bajarse la prioridad */
    root(SIS_PLANIFICADOR, PLAN_PRIORIDADE, 0, (unsigned)-5);   /* subirla, no */
    root(SIS_PLANIFICADOR, PLAN_ESCOLHER, 0, 0);
    if (sup > 0) root(SIS_PLANIFICADOR, PLAN_AFINIDADE, (unsigned)sup, 1);   /* la de otro usuario */
    /* SIS_SERVICOS */
    livre(SIS_SERVICOS, SERV_APAGANDO, 0, 0);
    livre(SIS_SERVICOS, SERV_SUPERVISOR, 0, 0);
    livre(SIS_SERVICOS, SERV_PEDIR, 0, 0);                   /* puntero 0: ERRO_ENDERECO */
    livre(SIS_SERVICOS, SERV_RESULTADO, 12345, 0);
    root(SIS_SERVICOS, SERV_REGISTRAR, 0, 0);
    root(SIS_SERVICOS, SERV_PRONTO, 0, 0);
    root(SIS_SERVICOS, SERV_ATIVOS, 0, 0);
    root(SIS_SERVICOS, SERV_PROXIMO, b, 0);
    root(SIS_SERVICOS, SERV_RESPONDER, 1, 1);
    /* SIS_USUARIO */
    livre(SIS_USUARIO, USU_CONSULTAR, 0, 0);
    livre(SIS_USUARIO, USU_DE_PID, 0, 0);
    livre(SIS_USUARIO, USU_ESTADO, 0, 0);
    /* SIS_PERMISOS_ARQ (D282): libre; quién puede cambiar qué se mira dentro */
    livre(SIS_PERMISOS_ARQ, PARQ_UMASK, (unsigned)-1, 0);
    livre(SIS_PERMISOS_ARQ, PARQ_CONSULTAR, 0, 0);
    livre(SIS_PERMISOS_ARQ, PARQ_MODO, 0, 0);
    livre(SIS_PERMISOS_ARQ, PARQ_DONO, 0, 0);
    root(SIS_USUARIO, USU_FIJAR, 0, 0);
    root(SIS_USUARIO, USU_COMO, 0, 0);
    /* Y sigue siendo quien era. */
    anota(meu_uid() == UID_SAGUI, "después de todo, sigue siendo sagui (uid %u)", meu_uid());
}

static void rutas(void)
{
    static const struct { const char *c; int modo; } negadas[] = {
        { "/etc/sombra", ABRIR_LER }, { "/etc/sombra.tmp", ABRIR_LER }, { "/ETC/SOMBRA", ABRIR_LER },
        { "/etc/../etc/sombra", ABRIR_LER }, { "/etc/sombra", ABRIR_ESCREVER | ABRIR_CRIAR },
        { "/etc/usuarios", ABRIR_ESCREVER }, { "/etc/nuevo", ABRIR_ESCREVER | ABRIR_CRIAR },
        { "/etc/servicios/malo.conf", ABRIR_ESCREVER | ABRIR_CRIAR }, { "/bin/ls", ABRIR_ESCREVER },
        { "/bin/nuevo", ABRIR_ESCREVER | ABRIR_CRIAR }, { "/sistema/conf/etc/usuarios", ABRIR_LER },
        { "/sistema/conf/LEEME", ABRIR_LER }, { "/home/.ssh/ssh_host_ed25519_key", ABRIR_LER },
        { "/etc/ssh/ssh_host_ed25519_key", ABRIR_LER }, { "/etc/vnc/vncd.passwd", ABRIR_LER },
        { "/tmp/.servicios/pedidos/1.ped", ABRIR_ESCREVER | ABRIR_CRIAR },
    };
    for (unsigned i = 0; i < sizeof negadas / sizeof negadas[0]; i++) {
        int r = abrir(negadas[i].c, negadas[i].modo);
        if (r >= 0) fechar(r);
        anota(r == ERRO_ACESSO, "abrir %s (modo %d): esperaba ERRO_ACESSO y dio %d", negadas[i].c, negadas[i].modo, r);
    }
    int r = criar_pasta("/etc/nueva");
    anota(r == ERRO_ACESSO, "crear /etc/nueva: %d", r);
    r = criar_pasta("/sistema/x");
    anota(r == ERRO_ACESSO, "crear /sistema/x: %d", r);
    r = apagar("/etc/usuarios");
    anota(r == ERRO_ACESSO, "borrar /etc/usuarios: %d", r);
    r = apagar("/bin/ls");
    anota(r == ERRO_ACESSO, "borrar /bin/ls: %d", r);
    r = remover_pasta("/etc/servicios");
    anota(r == ERRO_ACESSO, "borrar la carpeta /etc/servicios: %d", r);
    r = renomear("/etc", "/etc2");
    anota(r == ERRO_ACESSO, "renombrar /etc: %d", r);
    int fd = abrir("/tmp/usuarios.x", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    anota(fd >= 0, "escribir en /tmp: %d", fd);
    if (fd >= 0) fechar(fd);
    r = renomear("/tmp/usuarios.x", "/etc/usuarios.x");
    anota(r == ERRO_ACESSO, "mover de /tmp a /etc: %d", r);
    r = renomear("/tmp/usuarios.x", "/bin/usuarios.x");
    anota(r == ERRO_ACESSO, "mover de /tmp a /bin: %d", r);
    apagar("/tmp/usuarios.x");
    /* Lo que sí se puede leer. */
    fd = abrir("/etc/usuarios", ABRIR_LER);
    anota(fd >= 0, "leer /etc/usuarios: %d", fd);
    if (fd >= 0) fechar(fd);
    fd = abrir("/etc/servicios/sshd.conf", ABRIR_LER);
    anota(fd >= 0, "leer /etc/servicios/sshd.conf: %d", fd);
    if (fd >= 0) fechar(fd);
    conta_t c;
    anota(conta_por_uid(meu_uid(), &c) && compara_texto(c.nome, "sagui") == 0, "la cuenta del uid %u es sagui", meu_uid());
}

static void servico(void)
{
    int est = usuario_op(USU_ESTADO, 0, 0);
    anota(est >= 0 && (est & USU_EST_SERVICIO), "el kernel sabe que viene de un servicio (%d)", est);
    int r = sc(SIS_DESLIGAR, 0, 0, 0, 0, 0);
    anota(r == ERRO_ACESSO, "apagar desde un servicio que no es root: %d", r);
    r = sc(SIS_REINICIAR, 0, 0, 0, 0, 0);
    anota(r == ERRO_ACESSO, "reiniciar desde un servicio que no es root: %d", r);
    /* D354: el Wi-Fi: mirar y buscar sí; conectar y desconectar, no (solo la sesión local o root) */
    r = sc(SIS_WIFI, WIFI_CONECTAR, 0, 0, 0, 0);
    anota(r == ERRO_ACESSO, "conectar el Wi-Fi desde un servicio que no es root: %d", r);
    r = sc(SIS_WIFI, WIFI_DESCONECTAR, 0, 0, 0, 0);
    anota(r == ERRO_ACESSO, "desconectar el Wi-Fi desde un servicio que no es root: %d", r);
    r = sc(SIS_WIFI, WIFI_BUSCAR, 0, 0, 0, 0);
    anota(r != ERRO_ACESSO, "buscar redes Wi-Fi desde un servicio es libre: %d", r);
    /* Desde un servicio, una cuenta sin contraseña no se puede fijar. */
    pedido_conta_t p;
    memset(&p, 0, sizeof p);
    p.tipo = PED_ELEVAR;
    p.uid_alvo = UID_ROOT;
    copia_texto(p.senha_nova, "desde-servicio", sizeof p.senha_nova);
    r = pedir_conta(&p);
    imprimir("usuarios: servicio elevar sin contraseña -> %d\n", r);
    registrar("usuarios: servicio elevar sin contrasena -> %d", r);
    anota(meu_uid() != UID_ROOT, "el servicio no pasó a root (uid %u)", meu_uid());
}

/* RFC 7914, 11: PBKDF2-HMAC-SHA256 ("passwd", "salt", 1 iteración). */
static void vetores(void)
{
    static const unsigned char esperado[32] = {
        0x55, 0xac, 0x04, 0x6e, 0x56, 0xe3, 0x08, 0x9f, 0xec, 0x16, 0x91, 0xc2, 0x25, 0x44, 0xb6, 0x05,
        0xf9, 0x41, 0x85, 0x21, 0x6d, 0xde, 0x04, 0x65, 0xe6, 0x8b, 0x9d, 0x57, 0xc2, 0x0d, 0xac, 0xbc };
    unsigned char h[32];
    pbkdf2_sha256("passwd", 6, (const unsigned char *)"salt", 4, 1, h);
    anota(memcmp(h, esperado, 32) == 0, "PBKDF2-HMAC-SHA256 (RFC 7914, 11)");
    char t[PBKDF2_TEXTO_MAX];
    anota(pbkdf2_gerar("clave-de-prueba", t, sizeof t) == 0, "generar un hash");
    anota(pbkdf2_conferir("clave-de-prueba", t), "la buena se acepta");
    anota(!pbkdf2_conferir("clave-de-prueba!", t), "la mala no");
    anota(!pbkdf2_conferir("x", "pbkdf2-sha256$1$zz$00"), "un hash dañado no vale");
    anota(!pbkdf2_conferir("", ""), "vacío no vale");
}

int principal(int argc, char **argv)
{
    const char *o = argc > 1 ? argv[1] : "";
    if (compara_texto(o, "--llamadas") == 0) llamadas();
    else if (compara_texto(o, "--rutas") == 0) rutas();
    else if (compara_texto(o, "--servicio") == 0) servico();
    else if (compara_texto(o, "--pbkdf2") == 0) vetores();
    else { imprimir_em(ERROS, "uso: usuarios --llamadas | --rutas | --servicio | --pbkdf2\n"); return 2; }
    imprimir("usuarios: %s ok=%d mal=%d\n", o + 2, ok, mal);
    registrar("usuarios: %s ok=%d mal=%d", o + 2, ok, mal);
    return mal ? 1 : 0;
}
