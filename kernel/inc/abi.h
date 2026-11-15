// SPDX-License-Identifier: GPL-3.0-only
/* abi.h - o contrato entre o kernel e os programas do SaguiOS.
 *
 * Este arquivo é incluído pelo kernel e pela libsagui. Convenção (PRD 4.5):
 * "int 0x80", número da chamada em EAX, argumentos em EBX, ECX, EDX (e ESI,
 * EDI quando precisa de mais), resultado em EAX; negativo = erro.
 * Os números são agrupados como no livro do Tanenbaum. */
#ifndef SAGUI_ABI_H
#define SAGUI_ABI_H

/* Processos */
#define SIS_EXECUTAR        1   /* (caminho, argv, envp ou 0) -> pid; envp = "NOME=valor"... NULL (D037) */
#define SIS_ESPERAR         2   /* (pid ou -1, &codigo, sem_bloquear) -> pid (0 = ninguém terminou) */
#define SIS_SAIR            3   /* (codigo) */
#define SIS_PID             4   /* () -> pid */
#define SIS_MATAR           5   /* (pid) */
/* Arquivos */
#define SIS_ABRIR          10   /* (caminho, modo) -> fd */
#define SIS_LER            11   /* (fd, buf, n) -> lidos (0 = fim) */
#define SIS_ESCREVER       12   /* (fd, buf, n) -> escritos */
#define SIS_FECHAR         13   /* (fd) */
#define SIS_POSICIONAR     14   /* (fd, desloc, origem) -> nova posição */
#define SIS_INFO_ARQUIVO   15   /* (caminho, &info_arquivo) */
/* Diretórios */
#define SIS_CRIAR_PASTA    20   /* (caminho) */
#define SIS_REMOVER_PASTA  21   /* (caminho) */
#define SIS_APAGAR         22   /* (caminho) */
#define SIS_LISTAR         23   /* (caminho, indice, &entrada_pasta) -> 1 achou, 0 fim */
#define SIS_MUDAR_PASTA    24   /* (caminho) */
#define SIS_PASTA_ATUAL    25   /* (buf, n) */
#define SIS_RENOMEAR       26   /* (origem, destino) */
/* Comunicação */
#define SIS_PIPE           30   /* (int fds[2]) */
#define SIS_DUPLICAR       31   /* (fd_antigo, fd_novo) -> fd_novo */
#define SIS_DISPONIVEL     32   /* (fd) -> bytes num pipe sem bloquear; ERRO_PIPE_QUEBRADO = acabou */
/* Diversas */
#define SIS_DORMIR         40   /* (ms) */
#define SIS_TEMPO          41   /* () -> milissegundos desde o boot */
#define SIS_DATA           42   /* (&data_hora) */
#define SIS_INFO_SISTEMA   43   /* (tipo, buf, n) */
#define SIS_AUMENTAR_MEM   44   /* (bytes) -> fim antigo da área de dados */
#define SIS_REINICIAR      45   /* () */
#define SIS_MODO_TERMINAL  46   /* (TERM_LINHA ou TERM_BRUTO) */
#define SIS_LIMPAR_TELA    47   /* () */
#define SIS_REGISTRAR      48   /* (texto) -> escreve no log do sistema (serial), não na tela */
/* Janelas (fase 5) */
#define SIS_CRIAR_JANELA   50   /* (titulo, largura | altura << 16, &pixels) -> id */
#define SIS_FECHAR_JANELA  51   /* (id) */
#define SIS_ATUALIZAR_JANELA 52 /* (id, &retangulo ou 0 = tudo) */
#define SIS_PROXIMO_EVENTO 53   /* (id ou 0 = qualquer janela minha, &evento, esperar) -> 1 se houve; esperar =
                                   0 (não espera), 1 (sem prazo) ou EVENTO_PRAZO | instante em ms (D083) */
#define SIS_TITULO_JANELA  54   /* (id, titulo) */
/* Dispositivos (fase 6) */
#define SIS_LISTAR_DISCOS  55   /* (indice, &info_disco) -> 1 achou, 0 fim */
#define SIS_MONTAR         56   /* (nome, ponto, opcoes): ponto 0 = forma antiga, monta em /usb (D040) */
#define SIS_DESMONTAR      57   /* (nome do disco ou pasta montada) -> grava o que falta e libera */
/* Som (bônus B3) */
#define SIS_BIP            58   /* (&notas, quantidade, modo) -> volta na hora; o tique do relógio toca */
/* Jogos (bônus B4) */
#define SIS_TECLAS_SEGURADAS 59 /* (&mapa[32]) -> 1 se a janela do processo está em foco */
/* Teclado */
#define SIS_TECLADO        60   /* (TECLADO_ES, TECLADO_ABNT2 ou -1 = só consultar) -> layout atual */
/* Terminal virtual (D036) */
#define SIS_ABRIR_TV       61   /* (int fds[2]) -> fds[0] = mestre (tela), fds[1] = escravo (programas) */
#define SIS_PRIMEIRO_PLANO 62   /* (grupo ou 0) -> anterior; o Ctrl+C no terminal da entrada 0 encerra esse grupo */
/* Shell completo (D037) */
/* Aleatoriedade (D061): RDRAND/RDSEED se houver, senão TSC + jitter das IRQ, num reservatório SHA-256 */
#define SIS_ALEATORIO      63   /* (buf, n) -> n bytes imprevisíveis (no máximo 4096 por chamada) */
#define SIS_INFO_ALEATORIO 64   /* (&info_aleatorio_t) -> 0 */

#define SIS_GRUPO          70   /* (pid ou 0 = eu, grupo ou 0 = o próprio pid) -> põe o processo (eu ou um filho) num grupo */
#define SIS_INTERRUPCOES   71   /* () -> quantos Ctrl+C o terminal da entrada 0 já recebeu */
#define SIS_TIPO_DESCRITOR 72   /* (fd) -> TIPO_TERMINAL, TIPO_PIPE ou TIPO_ARQUIVO */
/* Janelas de tamanho variável e tela cheia (D044) */
#define SIS_ESTILO_JANELA  78   /* (id, estilo JAN_*, min_largura | min_altura << 16) */
#define SIS_REDIMENSIONAR_JANELA 79 /* (id, largura | altura << 16, &pixels) -> 0; os pixels ficam no mesmo endereço */
/* Administrador de tareas (D043) */
#define SIS_PEDIR_FECHAR   75   /* (id da janela) -> entrega EV_FECHAR ao dono, como o X da moldura */
/* Modos de vídeo (D058, D059) */
#define SIS_MODOS_VIDEO    76   /* (modo_video_t[], n) -> quantos modos copiou */
#define SIS_MUDAR_MODO     77   /* (largura | altura << 16, MUDAR_*) -> MUDAR_* do que foi feito, ou erro */
/* Rede (D046). Faixa 80-89 reservada para a rede. IPs como número na ordem
 * "humana": 10.0.2.15 = 0x0A00020F. */
#define SIS_SOCKET         80   /* (SOCK_TCP ou SOCK_UDP) -> fd (ler/escrever/fechar valem nele) */
#define SIS_CONECTAR       81   /* (fd, ip, porta) -> 0; TCP: abre a conexão; UDP: fixa o destino */
#define SIS_ESCUTAR        82   /* (fd, porta, fila) -> 0; TCP: espera conexões; UDP: recebe nessa porta */
#define SIS_ACEITAR        83   /* (fd, &endereco_rede_t ou 0) -> fd da conexão que chegou */
#define SIS_ENVIAR_PARA    84   /* (fd, buf, n, ip, porta) -> n (UDP) */
#define SIS_RECEBER_DE     85   /* (fd, buf, n, &endereco_rede_t ou 0) -> n (UDP: um datagrama) */
#define SIS_RESOLVER       86   /* (nome, &ip, servidor ou 0, porta ou 0) -> 0; pergunta ao DNS */
#define SIS_CONFIG_REDE    87   /* (REDE_*, &config_rede_t) -> 0 */
#define SIS_PING           88   /* (ip, &info_ping_t, prazo_ms) -> 0; eco ICMP */
#define SIS_OPCAO_SOCKET   89   /* (fd, OPC_*, valor) -> depende da opção */
/* Menu de contexto (D051) */
#define SIS_MENU_CONTEXTUAL 90  /* (id, x, y, itens, codigo) -> 0; mostra um menu sobre a janela (x, y relativos
                                   ao conteúdo). itens = textos separados por '\n'; "-" = separador; "~Texto" =
                                   desativado; "Texto\tAtalho" = atalho alinhado à direita; "*Texto" =
                                   marcado (a opção atual); ">Texto" abre um submenu com as linhas seguintes,
                                   até uma linha "<" (D064). O índice da resposta conta todas as linhas,
                                   ">" e "<" inclusive. A resposta chega depois como EV_MENU. */
/* Hora e fuso horário (D080, D081). O kernel guarda a hora em UTC. */
#define SIS_HORA           91   /* (HORA_*, arg, arg2): ver abaixo */
#define SIS_ZONA           92   /* (ZONA_LER ou ZONA_FIXAR, &config_zona_t) -> 0 */
#define SIS_DATAS_ARQUIVO  93   /* (caminho, &datas_arquivo_t) -> 0; datas em UTC (0 = desconhecida) */
/* Área de transferência (D074): um texto Latin-1 de até PORTAPAPEIS_MAX bytes, o último copiado */
#define SIS_COPIAR_TEXTO   94   /* (buf, n) -> bytes guardados (corta em PORTAPAPEIS_MAX; n = 0 esvazia) */
#define SIS_COLAR_TEXTO    95   /* (buf, max) -> tamanho do texto guardado; copia min(tamanho, max) bytes, sem zero */
#define PORTAPAPEIS_MAX    65536
/* Som de verdade: placa AC'97 ou Intel HDA e o misturador do kernel (D088-D090).
 * Um fluxo é um descritor: escrever() nele também toca (bloqueando) e
 * fechar() descarta o que faltava tocar. */
#define SIS_AUDIO_ABRIR    96   /* (taxa em Hz, canais | bits << 8) -> fd; ERRO_NAO_EXISTE = sem placa de som */
#define SIS_AUDIO_ESCREVER 97   /* (fd, buf, n, sem_bloquear) -> bytes aceitos (só quadros inteiros; 0 = cheio) */
#define SIS_AUDIO_CONTROLE 98   /* (fd ou -1, AUDIO_*, valor) -> depende da operação (abaixo) */
#define SIS_AUDIO_FECHAR   99   /* (fd, esperar) -> 0; esperar = 1: volta só depois de tocar tudo */
#define SIS_MOVER_JANELA  100   /* (id, x, y) -> 0; posição da moldura na tela (D102); x ou y < 0: relativo à borda
                                   direita/de baixo (-1 = encostada). Fora do desktop: ERRO_ACESSO */
#define SIS_TRUNCAR       111   /* (fd, novo tamanho) -> 0; reduz (libera) ou aumenta (rellena con ceros) (D116) */
#define SIS_SINCRONIZAR   112   /* (fd) -> 0; fuerza a disco lo pendiente del archivo (D116) */
#define SIS_FREQUENCIA    101   /* (FREQ_OP_*, arg) -> 0 o ERRO_*; control de la frecuencia de la CPU (D106).
                                   La consulta del estado es INFO_FREQUENCIA */
/* Diagnóstico y ajuste manual del códec HDA (D107): solo con el driver hda. */
#define SIS_AUDIO_HDA     102   /* (HDA_OP_*, a, b) -> depende de la operación (abajo); ERRO_NAO_EXISTE sin HDA */
/* Aparência do escritório (D112): fundo escolhido e cor de destaque. */
#define SIS_FUNDO         103   /* (FUNDO_*, ptr, y, n): imagem de fundo ja escalada para a tela, por faixas de linhas (abaixo) */
#define SIS_ACENTO        104   /* (rgb 0xRRGGBB ou -1 = so consultar) -> cor de destaque atual (barra de titulo e de tarefas) */
/* Apagado por ACPI e botão de energia (D113). */
#define SIS_DESLIGAR      105   /* () -> não volta: grava os discos, ACPI S5; sem ACPI, "Ya puedes apagar el PC" */
#define SIS_ACPI          106   /* (ACPI_OP_*) -> 0; ERRO_NAO_EXISTE = sem ACPI utilizável */
#define ACPI_OP_DESATIVAR   1   /* não usar mais o ACPI (apagar mostra a mensagem; o botão deixa de valer) */
#define ACPI_OP_ATIVAR      2   /* volta a usar (se as tabelas conferem e não foi a opção de arranque) */
/* Servidores con muchos sockets (lite-monkey, D117). */
#define SIS_ESPERAR_REDE  113   /* (visto, ms) -> contador de eventos de la red (31 bits); si sigue igual a
                                   "visto", duerme hasta que algún socket reciba algo o pasen ms */
#define SIS_CAPTURAR_TELA 114   /* (id|0=pantalla|-1=activa, &int[4] x,y,l,a (entra/sale), destino, max_pixels) -> pixels; la pantalla compuesta, sin cursor (D125) */
#define SIS_AVISAR        115   /* (texto, archivo|0) -> 0; notificación en la esquina; con archivo, al pulsarla se abre en el Visor (D125) */
#define SIS_COPIAR_IMAGEM 116   /* (ancho | alto << 16, píxeles 0x00RRGGBB) -> 0; portapapeles de imagen (D125); 0x0 vacía; máx. PORTAPAPEIS_IMG_PX */
#define SIS_COLAR_IMAGEM  117   /* (destino|0, max_pixels) -> ancho | alto << 16 (0 = no hay imagen); copia si destino != 0 y cabe */
#define PORTAPAPEIS_IMG_PX 2500000
/* Los BAR de un dispositivo PCI en el espacio del programa (D158). El kernel
 * elige qué páginas se mapean (las que el propio dispositivo declara en su BAR)
 * y devuelve la dirección virtual; el programa nunca ve una dirección física. */
#define SIS_MAPEAR_BAR    118   /* (&pedido_bar_t, &direccion) -> 0; la dirección virtual del mapa */
#define SIS_DESEJAR_BAR   119   /* (dirección virtual) -> 0; deshace el mapa de un BAR */
/* Hilos de usuario (D167). Un hilo comparte el espacio, los ficheros, la
 * carpeta y las ventanas del proceso; tiene su pila, sus registros, su FPU y
 * su número (tid), que no es el pid del proceso. */
#define SIS_HILO_CRIAR    120   /* (entrada, topo da pilha, arg) -> tid; con pilha != 0 la pila ya lleva el retorno
                                 * y los argumentos; con pilha = 0 el kernel da una de 1 MB con guarda y pone "arg" (D186) */
#define SIS_HILO_SAIR     121   /* (codigo) -> no vuelve; en el hilo principal, como SIS_SAIR */
#define SIS_HILO_ESPERAR  122   /* (tid, &codigo) -> 0; espera a que el hilo acabe y lo libera */
#define SIS_CEDER         123   /* () -> 0; cede el turno a otro hilo o proceso listo */
#define SIS_FUTEX_ESPERAR 124   /* (&palabra, valor, ms o 0) -> 0 despertado o plazo; 1 si la palabra no vale "valor" */
#define SIS_FUTEX_ACORDAR 125   /* (&palabra, cuantos) -> cuantos despertó */
#define SIS_HILO_ID       126   /* () -> tid del hilo que llama (el pid en el principal) */
/* D174: los planificadores. Una sola llamada con la operación en el primer
 * argumento (PLAN_*), para no gastar números. */
#define SIS_PLANIFICADOR  127   /* (PLAN_*, a, b, c) -> según la operación */
/* D179: acceso en bruto a los sectores de un disco (particiones, formateo, instalador).
 * Una sola llamada con la operación en el primer argumento (DBLOCO_*, abajo). */
#define SIS_DISCO_BLOCOS  128   /* (DBLOCO_*, nombre, a, b, c) -> según la operación */
/* D209: páginas del área de datos. Una sola llamada con la operación en el
 * primer argumento (MEM_*, abajo). Es lo que deja al malloc de la libsagui
 * devolver al sistema la memoria de los bloques grandes que se liberan. */
#define SIS_MEMORIA       129   /* (MEM_*, endereço, bytes) -> según la operación */
/* D215: el Wi-Fi (pincho RTL8188EU). Una llamada con la operación en el primer
 * argumento (WIFI_*, abajo). Buscar y conectar solo piden: la tarea «wifi» del
 * kernel lo hace y el programa consulta WIFI_ESTADO hasta que acaba. */
#define SIS_WIFI          130   /* (WIFI_*, ptr, n) -> según la operación */
/* D216: el gestor de servicios (/bin/servicios). Una sola llamada con la
 * operación en el primer argumento (SERV_*, abajo). */
#define SIS_SERVICOS      131   /* (SERV_*, arg) -> según la operación */

/* D218: el bloqueo de publicidad y rastreo por DNS. Una llamada con la operación
 * en el primer argumento (BLOQ_*, abajo). */
#define SIS_BLOQUEO       132   /* (BLOQ_*, a, b) -> según la operación */
/* Mejora 097 (v8.0b6, VNC): inyectar teclado y ratón como si vinieran del
 * hardware. Solo para el servicio de escritorio remoto (vncd); al resto,
 * error. Operación en el primer argumento (INY_*, abajo). */
#define SIS_INYECTAR      133   /* (INY_*, a, b) -> 0 o error */
/* D255 (mejora 098, fase 1): usuarios. Operación en el primer argumento (USU_*, abajo). */
#define SIS_USUARIO       134   /* (USU_*, a, b) -> según la operación */
/* D279 (v8.2, mejora 059): Bluetooth con un controlador USB estándar (p. ej. CSR8510). Operación en el
 * primer argumento (BT_*, abajo); las que cambian algo piden permisos de administrador (D255, D269). */
#define SIS_BLUETOOTH     135   /* (BT_*, ptr/indice, n) -> según la operación */
/* D290 (v8.3, mejora 060): varias pantallas. Operación en el primer argumento (TELAS_OP_*, abajo). */
#define SIS_TELAS         136   /* (TELAS_OP_*, a, b) -> según la operación */
#define SIS_MAX           137

/* SIS_USUARIO (D255). Cada proceso tiene un usuario (uid) y un grupo (gid);
 * los hijos y los hilos los heredan. Solo root (uid 0) los cambia. Lo que
 * puede hacer cada uno lo decide un solo módulo del kernel (permisos.c). */
#define UID_ROOT     0
#define GID_ROOT     0
#define UID_SAGUI    1000          /* el usuario del escritorio (entrada automática) */
#define GID_SAGUI    1000
#define UID_MAX      65535
#define USU_CONSULTAR  0   /* () -> uid | gid << 16 de quien llama */
#define USU_FIJAR      1   /* (uid, gid) -> 0; solo root: cambia la identidad del proceso (y de sus hilos) */
#define USU_COMO       2   /* (uid, gid) -> 0; solo root: el próximo executar() de este proceso crea el hijo
                            * con esa identidad (una sola vez); uid = -1 lo anula */
#define USU_DE_PID     3   /* (pid) -> uid | gid << 16 de ese proceso, o ERRO_SEM_PROCESSO */
#define USU_ESTADO     4   /* () -> USU_EST_* de quien llama y del arranque */
#define USU_EST_RESCATE  0x01   /* el sistema arrancó en modo rescate (shell de root en la consola) */
#define USU_EST_SERVICIO 0x02   /* el proceso viene de un servicio (sshd, vncd, cron...): no es la sesión local */
#define USU_EST_PRUEBA   0x04   /* imagen de prueba con la sesión como root (mkimagem.py --sesion-root) */
#define USU_EST_EN_VIVO  0x08   /* sin la partición SAGUI-CONF: la configuración del sistema no se guarda */

/* SIS_INYECTAR (mejora 097).
 * INY_TECLA:  a = código de tecla como en EV_TECLA (Latin-1, o TECLA_* desde 0x100);
 *             b = INY_PULSADA (o 0 si se suelta) | INY_MOD_* con los modificadores
 *             que el cliente tiene pulsados en ese momento. Con a = 0 solo cambian
 *             los modificadores (pulsar o soltar Shift, Ctrl o Alt solos: soltar
 *             Alt cierra el Alt+Tab). Ctrl+letra se convierte en el código de
 *             control, como en el teclado real. TECLA_SUPER pulsada/soltada es la
 *             tecla Windows (sola abre el menú Inicio al soltarla).
 * INY_RATON:  a = (x << 16) | y, posición absoluta en la pantalla;
 *             b = botones pulsados (INY_BOTON_*).
 * INY_RUEDA:  a = entalles con signo (positivo hacia abajo, como EV_RODA).
 * INY_ESTADO: a = 1 para activar el permiso en este proceso, 0 para soltarlo;
 *             devuelve error si el proceso no puede inyectar. Permisos (D251): solo
 *             /bin/vncd o /apps/bin/vncd lanzado por el gestor de servicios, y uno
 *             a la vez (ERRO_ACESSO, ERRO_OCUPADO); argumentos fuera de rango,
 *             ERRO_INVALIDO; la posición se recorta a la pantalla. */
#define INY_TECLA         1
#define INY_RATON         2
#define INY_RUEDA         3
#define INY_ESTADO        4
#define INY_PULSADA       0x01
#define INY_MOD_SHIFT     0x02
#define INY_MOD_CTRL      0x04
#define INY_MOD_ALT       0x08
#define INY_BOTON_IZQ     0x01
#define INY_BOTON_MEDIO   0x02
#define INY_BOTON_DER     0x04

/* SIS_SERVICOS (D216). El supervisor es el programa que el kernel lanza al
 * arrancar; solo puede haber uno vivo. Al apagar o reiniciar, el kernel le
 * avisa (SERV_APAGANDO deja de ser 0) y espera, como mucho 20 s, su SERV_PRONTO. */
#define SERV_REGISTRAR  1   /* () -> 0; quien llama pasa a ser el supervisor (ERRO_OCUPADO si hay otro vivo) */
#define SERV_APAGANDO   2   /* () -> 0, o SERV_MOTIVO_*: el sistema se está apagando o reiniciando */
#define SERV_PRONTO     3   /* () -> 0; solo el supervisor: ya paró los servicios, el apagado sigue */
#define SERV_ATIVOS     4   /* (cuántos) -> 0; solo el supervisor: servicios en marcha (0 = el apagado no espera) */
#define SERV_SUPERVISOR 5   /* () -> pid del supervisor vivo, o 0 */
/* D255: elevación sin setuid. Un programa (su, admin, passwd) deja en el kernel
 * un pedido con la contraseña; el supervisor (root, de /bin/servicios) lo recoge,
 * comprueba la contraseña en la sombra y responde. Si concede una elevación, el
 * kernel cambia la identidad del proceso que lo pidió, que sigue con su misma
 * entrada y salida. Un pedido por proceso; 8 a la vez como mucho. */
#define SERV_PEDIR      6   /* (&pedido_conta_t) -> id (> 0); ERRO_NAO_EXISTE sin supervisor, ERRO_OCUPADO lleno */
#define SERV_RESULTADO  7   /* (id) -> 0 aún sin respuesta, 1 hecho, o el error con que se negó */
#define SERV_PROXIMO    8   /* (&pedido_recebido_t) -> 1 si había uno, 0 si no; solo el supervisor */
#define SERV_RESPONDER  9   /* (id, resultado) -> 0; solo el supervisor; 1 = concedido (eleva si es PED_ELEVAR) */
#define PED_ELEVAR      1   /* pasar a ser uid_alvo (root, normalmente) con su contraseña */
#define PED_SENHA       2   /* cambiar la contraseña de uid_alvo (la actual en senha, la nueva en senha_nova) */
#define PED_SENHA_MAX   64
/* D269: con PED_GRAFICO (se suma a PED_ELEVAR) el kernel no espera una contraseña del programa: el
 * gestor de ventanas pinta el diálogo de permisos (atenúa el escritorio), pide la contraseña de
 * root a la persona y la entrega él mismo al supervisor. El programa solo espera el resultado
 * con SERV_RESULTADO (1 = ya es root; ERRO_ACESSO = «No» o contraseña mala; ERRO_ESPERA = bloqueo). */
#define PED_GRAFICO     0x100
#define PED_MOTIVO_MAX  80
typedef struct {
    unsigned int tipo;                   /* PED_* */
    unsigned int uid_alvo;
    char         senha[PED_SENHA_MAX];   /* la contraseña actual de uid_alvo */
    char         senha_nova[PED_SENHA_MAX];   /* PED_SENHA, o la primera contraseña de root si no tenía */
    char         motivo[PED_MOTIVO_MAX];      /* PED_GRAFICO: qué quiere hacer el programa (lo muestra el diálogo) */
} pedido_conta_t;
typedef struct {
    unsigned int id;
    unsigned int tipo, uid_alvo;
    int          pid;                    /* quien lo pidió */
    unsigned int uid, gid;               /* su identidad al pedirlo */
    unsigned int servicio;               /* 1 = viene de un servicio (sshd...), no de la sesión local */
    char         senha[PED_SENHA_MAX];
    char         senha_nova[PED_SENHA_MAX];
} pedido_recebido_t;
#define SERV_MOTIVO_APAGAR    1
#define SERV_MOTIVO_REINICIAR 2

/* SIS_MEMORIA (D209). Las direcciones y los tamaños van en páginas enteras
 * (múltiplos de 4096) y dentro del área de datos ([inicio, fim) de
 * aumentar_memoria): fuera de ella, ERRO_INVALIDO. */
#define MEM_RESERVAR  1   /* (0, bytes) -> fin antiguo del área; la alarga SIN asignar páginas */
#define MEM_DEVOLVER  2   /* (endereço, bytes) -> páginas devueltas; quedan sin asignar (tocarlas = fallo) */
#define MEM_ASIGNAR   3   /* (endereço, bytes) -> 0; asigna (a cero) las páginas que falten */

/* SIS_DISCO_BLOCOS (D179). El nombre es el de "discos" (hdb, usb0, hdb1...).
 * Leer no pide nada; escribir, descartar y releer la tabla solo valen para el
 * proceso que "reservó" el disco entero, y la reserva solo se da si no hay nada
 * montado de ese disco (o, con DBLOCO_CONF_MONTADO, protegiendo lo montado: D213). El disco del que arrancó el sistema pide además
 * DBLOCO_CONF_ARRANQUE (la confirmación doble de la interfaz). */
#define DBLOCO_RESERVAR   1   /* (nombre del disco entero, DBLOCO_CONF_*) -> 0 */
#define DBLOCO_LIBERAR    2   /* (nombre) -> 0; suelta la reserva y vuelve a leer la tabla */
#define DBLOCO_LER        3   /* (nombre, lba, sectores <= 128, destino) -> 0 */
#define DBLOCO_ESCREVER   4   /* (nombre, lba, sectores <= 128, origen) -> 0 */
#define DBLOCO_DESCARTAR  5   /* (nombre, lba, sectores) -> 0; TRIM; ERRO_NAO_SUPORTADO si no se puede */
#define DBLOCO_RELER      6   /* (nombre) -> 0; vuelve a leer la tabla sin soltar la reserva */
#define DBLOCO_INFO       7   /* (nombre, &info_bloco_t) -> 0 */
/* D233: CD/DVD. Desmonta lo que haya del disco y abre la bandeja (START STOP UNIT con
 * LoEj); con a = 1, la cierra. ERRO_OCUPADO si un programa lo tiene reservado (el
 * instalador copiando del CD) o no se puede desmontar; ERRO_NAO_SUPORTADO si no es un CD. */
#define DBLOCO_EXPULSAR   8   /* (nombre, cerrar) -> 0 */
#define DBLOCO_CONF_ARRANQUE 1   /* reservar el disco de arranque: la persona lo confirmó dos veces */
#define DBLOCO_CONF_GPT      2   /* reservar un disco GPT para borrarlo entero (el instalador) */
/* D213: reservar aunque haya particiones montadas (el disco de arranque con /home, /apps...).
 * Lo montado y, en el disco de arranque, la partición de SaguiOS (0xDA) quedan intocables: no
 * se escribe ni se descarta nada dentro de ellas, y cada escritura de la tabla (MBR o EBR) tiene
 * que dejarlas exactamente donde estaban (mismo inicio, tamaño y tipo). */
#define DBLOCO_CONF_MONTADO  4
#define DBLOCO_MAX_SETORES 128

typedef struct {
    unsigned int setores;          /* del volumen (disco o partición) */
    unsigned int inicio;           /* primer sector dentro del disco (0 = disco entero) */
    unsigned int trim;             /* TRIM_* abajo */
    unsigned int arranque_real;    /* 1 = es (o es parte de) el disco del que arrancó la máquina */
    unsigned int reservado_pid;    /* quién lo tiene reservado (0 = nadie) */
    unsigned int gpt;              /* 1 = el disco tiene tabla GPT (solo lectura) */
    unsigned int tipo_mbr;         /* tipo de la partición en la tabla (0 en el disco entero) */
    unsigned int cab_lba;          /* D270: en el disco de arranque, dónde está su cabecera: 1, o en un disco
                                    * instalado con GPT el primer sector de la partición de SaguiOS (0 = no lo es) */
    unsigned int bloco_fisico;     /* D283: 512, o 4096 en un NVMe de bloques de 4096 (las tablas MBR/GPT cuentan en ellos) */
    unsigned int reservado[7];
} info_bloco_t;
#define TRIM_NAO          0        /* el disco no lo admite (IDENTIFY palabra 169) */
#define TRIM_SIM          1        /* lo admite y el driver lo manda (AHCI) */
#define TRIM_SEM_DRIVER   2        /* lo admite, pero el driver no sabe mandarlo (IDE por PIO) */

/* SIS_MAPEAR_BAR: la petición va en un struct en el espacio del programa. */
typedef struct {
    unsigned int barramento, dispositivo, funcao;   /* donde está el dispositivo (PCI) */
    unsigned int indice;      /* 0..5: qué BAR del dispositivo */
    unsigned int desloc;      /* desde dónde dentro del BAR, en bytes */
    unsigned int tamanho;     /* cuántos bytes se quieren ver */
    unsigned int permissao;   /* BAR_LEITURA (0) o BAR_ESCRITA (1) */
} pedido_bar_t;

#define BAR_LEITURA 0
#define BAR_ESCRITA 1
#define BAR_TAMANHO_MAX (16u * 1024u * 1024u)   /* por petición */

/* Erros (sempre negativos) */
#define ERRO_INVALIDO      -1
#define ERRO_NAO_EXISTE    -2
#define ERRO_JA_EXISTE     -3
#define ERRO_SEM_MEMORIA   -4
#define ERRO_ENDERECO      -5   /* ponteiro inválido vindo do programa */
#define ERRO_NAO_E_PASTA   -6
#define ERRO_E_PASTA       -7
#define ERRO_NAO_VAZIA     -8
#define ERRO_LIMITE        -9   /* tabela cheia (arquivos, processos) */
#define ERRO_DESCRITOR    -10
#define ERRO_PIPE_QUEBRADO -11
#define ERRO_SEM_PROCESSO -12
#define ERRO_FORMATO      -13   /* não é um executável ELF válido */
#define ERRO_ACESSO       -14   /* operação não permitida */
#define ERRO_OCUPADO      -15
#define ERRO_SO_LEITURA   -16   /* sistema de arquivos montado só para leitura (D040) */
#define ERRO_NAO_SUPORTADO -17  /* el dispositivo o el driver no lo admite (D179: TRIM) */
#define ERRO_DISCO_ARRANQUE -18 /* es el disco de arranque: hace falta la confirmación doble (D179) */
#define ERRO_SEM_SENHA     -19  /* D255: la cuenta no tiene contraseña y hay que fijarla desde la sesión local */
#define ERRO_ESPERA        -20  /* D255: demasiados intentos fallidos: hay que esperar */

/* D255: la configuración del sistema. Los programas leen /etc como siempre; la
 * partición SAGUI-CONF (ext2, solo root) se monta en CONF_SISTEMA_PONTO y su
 * etc/ se copia sobre el /etc de la RAM al arrancar (kernel/fs/conf_sistema.c).
 * Quien cambia algo del sistema lo escribe en los dos sitios con
 * conf_gravar() (libsagui/conf_sistema.c, temporal + renombrar). Sin la
 * partición, «modo en vivo»: nada se guarda y root no tiene contraseña (solo
 * desde la sesión local). El kernel protege estas rutas a quien no es root:
 * la sombra y la clave privada del sshd no se leen; /etc, /bin y /sistema no se
 * escriben; /sistema tampoco se lee. */
#define CONF_SISTEMA_PONTO    "/sistema/conf"
#define CONTAS_ARQ_USUARIOS   "/etc/usuarios"
#define CONTAS_ARQ_SOMBRA     "/etc/sombra"
#define CONTAS_CLAVES_ROOT    "/etc/ssh/authorized_keys"   /* las claves SSH que entran como root */

/* Opções do SIS_MONTAR (D040). Sem nenhuma das duas primeiras: o padrão
 * (leitura e escrita nos pendrives; só leitura nos discos internos). */
#define MONTAR_SO_LEITURA      0x01
#define MONTAR_LEITURA_ESCRITA 0x02   /* pedido explícito: único jeito de gravar num disco interno */
#define MONTAR_TIPO(o)   (((o) >> 8) & 0xF)
#define MONTAR_AUTO      0x000
#define MONTAR_FAT32     0x100
#define MONTAR_EXT2      0x200
#define MONTAR_EXT3      0x300
#define MONTAR_EXT4      0x400
#define MONTAR_ISO9660   0x500   /* D207: CD/DVD, siempre solo lectura */
#define MONTAR_UDF       0x600   /* D233: DVD y CD grabados (UDF 1.02-2.01), solo lectura */
/* SIS_HORA (D080) */
#define HORA_UTC          0   /* (&hora_t) -> segundos UTC desde 1/1/1970 (ler como unsigned) */
#define HORA_AJUSTAR      1   /* (segundos UTC, HORA_*) -> 0: acerta o relógio do sistema */
#define HORA_LER_RTC      2   /* (&data_t) -> 0: o que está gravado no relógio da placa, sem conversão */
#define HORA_GRAVAR_RTC   1   /* opção de HORA_AJUSTAR: grava também o RTC (em UTC ou local, conforme a zona) */
/* SIS_ZONA (D080) */
#define ZONA_LER          0
#define ZONA_FIXAR        1   /* aplica já (a barra e as datas mudam); quem grava /etc/zona é o programa */
/* De onde veio a configuração (config_zona_t.origem) */
#define ZONA_PADRAO       0   /* nada configurado: UTC */
#define ZONA_ARQUIVO      1   /* /etc/zona (na RAM) */
#define ZONA_PERSISTENTE  2   /* /home/.zona (disco persistente, D040) */
#define ZONA_PROGRAMA     3   /* mudada com SIS_ZONA depois do boot */

/* Rede (D046): -40 a -44 */
#define ERRO_TEMPO        -40   /* a rede não respondeu a tempo */
#define ERRO_SEM_REDE     -41   /* sem placa de rede ou ainda sem endereço IP */
#define ERRO_RECUSADA     -42   /* o outro computador recusou a conexão */
#define ERRO_INALCANCAVEL -43   /* sem rota ou o ARP não achou o destino */
#define ERRO_CONEXAO_CAIU -44   /* o outro lado cortou a conexão (RST) ou parou de responder */

/* Layouts do teclado (SIS_TECLADO) */
#define TECLADO_ES    0
#define TECLADO_ABNT2 1

/* Modos de abrir */
#define ABRIR_LER       1
#define ABRIR_ESCREVER  2
#define ABRIR_CRIAR     4
#define ABRIR_TRUNCAR   8
#define ABRIR_ACRESCENTAR 16

/* Origem do posicionar */
#define POS_INICIO 0
#define POS_ATUAL  1
#define POS_FIM    2

/* Tipos de entrada */
#define TIPO_ARQUIVO  1
#define TIPO_PASTA    2
#define TIPO_PROGRAMA 3
#define TIPO_TERMINAL 4
#define TIPO_PIPE     5

#define NOME_MAX 64

typedef struct {
    unsigned int tipo;
    unsigned int tamanho;
} info_arquivo_t;

typedef struct {
    char         nome[NOME_MAX];
    unsigned int tipo;
    unsigned int tamanho;
} entrada_pasta_t;

/* SIS_INFO_ALEATORIO (D061) */
#define ALEA_RDRAND 1   /* a CPU tem RDRAND (e ele funciona) */
#define ALEA_RDSEED 2   /* a CPU tem RDSEED */
#define ALEA_TSC    4   /* o TSC entra nas medidas de tempo */
typedef struct {
    unsigned int fontes;         /* ALEA_* */
    unsigned int eventos;        /* interrupções misturadas desde o boot */
    unsigned int pedidos;        /* chamadas atendidas */
    unsigned int reservado;
} info_aleatorio_t;

/* SIS_DATA: a hora LOCAL da zona do sistema (desde D080; antes, o RTC "a secas"). */
typedef struct {
    unsigned short ano;
    unsigned char  mes, dia, hora, minuto, segundo, dia_semana;
} data_t;

/* SIS_HORA, HORA_UTC (D080) */
typedef struct {
    unsigned int segundos;    /* UTC desde 1/1/1970 */
    unsigned int ms;          /* 0..999 */
    int          desvio;      /* da zona do sistema agora: segundos a somar ao UTC */
    unsigned int verao;       /* 1 = horário de verão agora */
    unsigned int ajustes;     /* muda quando acertam a hora ou trocam a zona */
} hora_t;

/* SIS_ZONA (D080) */
typedef struct {
    char         nome[40];    /* "Europe/Madrid" ("UTC" sem configuração) */
    char         tz[64];      /* regra POSIX TZ: "CET-1CEST,M3.5.0,M10.5.0/3" */
    unsigned int rtc_local;   /* 1 = o relógio da placa guarda a hora local (Windows); 0 = UTC */
    unsigned int rtc_auto;    /* 1 = ninguém escolheu: o kernel decidiu (UTC numa máquina virtual, local num PC) */
    unsigned int formato_24h; /* 1 = 24 horas; 0 = 12 horas (a. m./p. m.) */
    unsigned int origem;      /* ZONA_* (só leitura) */
} config_zona_t;

/* SIS_DATAS_ARQUIVO (D080) */
typedef struct {
    unsigned int modificado;  /* UTC desde 1970; 0 = o sistema de arquivos não guarda */
    unsigned int criado;
    unsigned int acessado;
} datas_arquivo_t;

/* SIS_BIP: uma nota. freq 0 = pausa. Duração arredondada para 10 ms. */
typedef struct {
    unsigned short freq;   /* Hz */
    unsigned short ms;
} nota_t;
#define BIP_SUBSTITUIR 0   /* para o que estiver tocando e começa esta lista */
#define BIP_ENFILEIRAR 1   /* toca depois do que já está na fila */

/* Áudio (D088-D090). Formatos de SIS_AUDIO_ABRIR: PCM de 8 bits sem sinal ou
 * de 16 bits com sinal (little-endian), mono ou estéreo intercalado, de
 * AUDIO_TAXA_MIN a AUDIO_TAXA_MAX Hz; o kernel converte para a saída da placa. */
#define AUDIO_TAXA_MIN  4000
#define AUDIO_TAXA_MAX  96000
#define AUDIO_FLUXOS    8       /* fluxos abertos ao mesmo tempo, de todos os programas */
/* SIS_AUDIO_CONTROLE: valor -1 = só consultar */
#define AUDIO_VOLUME    1   /* (fd, _, 0-100): volume do fluxo; fd = -1: o mestre -> volume atual */
#define AUDIO_SILENCIO  2   /* (-1, _, 0 ou 1): cala tudo sem perder o volume -> 1 se calado */
#define AUDIO_PENDENTE  3   /* (fd) -> ms que ainda faltam tocar do que foi escrito */
#define AUDIO_LIVRE     4   /* (fd) -> bytes que cabem agora sem bloquear */
#define AUDIO_DESCARTAR 5   /* (fd) -> 0; joga fora o que ainda não tocou */
#define AUDIO_EQ        6   /* (-1, _, &eq_audio_t o 0): ecualizador (D201); 0 = consultar -> nº de etapas activas */
#define AUDIO_SALIDA    7   /* (-1, _, índice | -1): salida de sonido (D275) -> índice actual; ERRO_INVALIDO si no existe */

#define AUDIO_BALANCE   8   /* (-1, _, 0-200 | -1): balance (D292): 0 todo a la izquierda, 100 centro, 200 todo a la derecha -> valor actual */
#define AUDIO_LATENCIA  9   /* (-1, _, ms 20-300 | -1): cuánto escribe el mezclador por delante de la tarjeta (D292) -> ms actuales */

/* AUDIO_EQ (D201): cascada de hasta 5 biquads por canal, en punto fijo. Los coeficientes
 * (b0 b1 b2 a1 a2, ya divididos por a0) los calcula el userland en Q28; y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2.
 * La ganancia previa (margen) va ya incluida en los b de la primera etapa. etapas = 0: derivación total. */
#define EQ_ETAPAS 5
#define EQ_BANDAS 5
typedef struct {
    unsigned int etapas;                 /* 0-5 */
    int          coef[EQ_ETAPAS][5];     /* Q28 */
    signed char  db[EQ_BANDAS];          /* los realces pedidos (-12..+12), solo para informar */
} eq_audio_t;

/* SIS_AUDIO_HDA (D107). Los verbos van al códec que usa el driver. */
#define HDA_OP_VERBO   1   /* (op, no << 20 | verbo << 8 | carga, &resposta): verbo de 12 bits con carga
                              de 8, o verbo de 4 bits (<< 16) con carga de 16, como en la norma -> 0 */
#define HDA_OP_ESTADO  2   /* (op, &info_hda_t, sizeof) -> 0 */
#define HDA_OP_PINO    3   /* (op, nodo, 1 o 0): busca un camino al DAC y enciende el pin / lo apaga -> 0 */
#define HDA_OP_GPIO    4   /* (op, datos, máscara): los bits de la máscara pasan a salidas con esos datos;
                              máscara 0 = volver a escribir los GPIO del ajuste (no apaga otros) -> 0 */
#define HDA_OP_AUTOSILENCIO 5  /* (op, 1/0/-1): callar los altavoces con auriculares enchufados -> estado */
#define HDA_MAX_PINOS  8
typedef struct {
    unsigned char  codec, afg, primeiro_no, n_nos;   /* dirección del códec en el enlace; nodos */
    unsigned int   id, revisao;                      /* parámetros 0 y 2 del nodo 0 */
    unsigned int   ssid_pci, ssid_codec;             /* subsistema del controlador (PCI) y del códec (F20) */
    char           ajuste[24], modelo[24];           /* ajuste aplicado (vacío = ninguno) */
    unsigned char  n_gpio, gpio_mascara, gpio_direcao, gpio_dados;   /* lo que escribió el driver */
    unsigned char  autosilencio, fones_presentes, n_pinos, reservado;
    unsigned char  pino[HDA_MAX_PINOS];              /* pines encendidos por el driver */
    unsigned char  pino_papel[HDA_MAX_PINOS];        /* 0 línea, 1 altavoz, 2 auriculares */
    unsigned char  pino_dac[HDA_MAX_PINOS];
    unsigned int   pino_config[HDA_MAX_PINOS];       /* configuración usada (tras el ajuste) */
} info_hda_t;

/* SIS_INFO_SISTEMA */
#define INFO_MEMORIA   1
#define INFO_PROCESSOS 2
#define INFO_VERSAO    3
#define INFO_USB       4   /* relatório em texto: controladores, portas e dispositivos */
/* Informação do hardware para o infopc (D041). Faixa 20-29 reservada para isto. */
#define INFO_E820     20   /* (info_e820_t[], n) -> quantas regiões copiou */
#define INFO_SMBIOS   21   /* (buf, n bytes) -> bytes: info_smbios_t seguido da tabela SMBIOS crua */
#define INFO_PCI      22   /* (info_pci_t[], n) -> quantos dispositivos copiou */
#define INFO_VIDEO    23   /* (&info_video_t) */
#define INFO_CPU_MHZ  24   /* (&info_tsc_t) -> kHz do TSC medido contra o PIT no boot (0 = sem medida) */
/* Vídeo (D057-D059). Faixa 45-49. */
#define INFO_TELA     45   /* (&info_tela_t): modo atual, troca a quente, preferência, write-combining */
/* Informe de compatibilidade (D076). */
#define INFO_LOG      46   /* (buf, n) -> bytes: o fim do log do kernel (os últimos 32 KB, linhas inteiras) */
#define INFO_ACPI     47   /* (&info_acpi_t) -> 0: RSDP e assinaturas das tabelas (sem interpretar a AML) */
#define INFO_ARRANQUE 48   /* (&info_arranque_t) -> 0: de onde e como o carregador leu o sistema */
/* Rede (D046). Faixa 40-44 reservada para a rede. */
#define INFO_REDE      40   /* (&info_rede_t) -> 1 se há placa (suportada ou não), 0 se não há nenhuma */
#define INFO_CONEXOES  41   /* (info_conexao_t[], n) -> quantas copiou */
#define INFO_ARP       42   /* (info_arp_t[], n) -> quantas copiou */
/* Rede no Administrador de tareas (D079). Faixa 50-54. */
#define INFO_INTERFACES 50  /* (info_rede_t[], n) -> quantas: eth0 (se houver placa) e lo */
#define INFO_SOCKETS    51  /* (info_socket_t[], n) -> quantos: INFO_CONEXOES com o estado TCP detalhado */
#define INFO_TRAFEGO    52  /* (&info_trafego_t) -> 0: bytes por segundo da eth0 nos últimos 60 s */
/* Som (D088). */
#define INFO_AUDIO      53  /* (&info_audio_t) -> 0: placa, códec, volume e os fluxos abertos */
#define INFO_FREQUENCIA 56  /* (&info_frequencia_t, sizeof) -> 0: driver de frecuencia de la CPU (D106) */
/* Motor 3D por la GPU (D158): lo que el kernel sabe de la aceleración. La familia
 * y los registros los lee el programa (userland/libamd), no el kernel. */
#define INFO_MOTOR_GPU  57  /* (&info_motor_gpu_t, sizeof) -> 0 */

/* Estado del motor 3D (D158). El camino por defecto es el motor por software. */
#define MOTOR_GPU_SOFTWARE     0   /* ahora mismo: motor por software */
#define MOTOR_GPU_POR_HARDWARE 1   /* la GPU identificada puede acelerar (el motor aún no existe) */
#define MOTOR_GPU_DESATIVADO   2   /* la opción de arranque quitó la aceleración */
#define MOTOR_GPU_VIGILANTE_MS 2000   /* plazo máximo del vigilante de la GPU */
typedef struct {
    unsigned int versao;       /* 1 */
    unsigned int estado;        /* MOTOR_GPU_* */
    unsigned int aceleracao;    /* 1 = la aceleración por hardware está permitida */
    unsigned int desativado;    /* 1 = la opción de arranque la quitó (SGL_MOTOR=software) */
    unsigned int boot_opcoes;   /* las opciones de arranque, sin interpretar */
    unsigned int gpu_amd;       /* 1 = hay una GPU AMD en el bus PCI */
    unsigned int modelo;        /* device id del PCI (0 = ninguna) */
    unsigned int revisao;       /* revision id del PCI */
    unsigned int watchdog_ms;   /* el plazo máximo del vigilante de la GPU */
    char         motivo[64];    /* por qué no se acelera (vacío si sí se puede) */
    unsigned int reservado[8];
} info_motor_gpu_t;

/* 2D por la GPU para el escritorio (D195): qué acelerador se usa y por qué.
 * Los números 66 y 67 (el 64 y el 65 quedan libres para otras ramas). */
#define INFO_ACEL2D       66  /* (&info_acel2d_t, sizeof) -> 0 */
#define INFO_ACEL2D_PROVA 67  /* (&info_acel2d_prova_t, sizeof) -> 0: autoprueba completa y medida, ahora */
typedef struct {
    unsigned int versao;        /* 1 */
    unsigned int ativo;         /* 1 = la GPU copia y rellena; 0 = la CPU */
    unsigned int modo;          /* lo que pidió la cabecera (ACEL2D_* de acel2d.h) */
    unsigned int fabricante, modelo;   /* tarjeta elegida (0 = ninguna) */
    char         nome[16];      /* "vmware", "intel", "radeon" o "cpu" */
    char         motivo[160];   /* qué tarjeta y por qué (o por qué la CPU) */
    unsigned int copias, rellenos, falhas;   /* órdenes hechas por la GPU desde el arranque */
    unsigned int arrastos, quadros;          /* ventanas arrastradas y cuadros de arrastre medidos */
    unsigned int kciclos_quadro;             /* media de miles de ciclos (TSC) por cuadro de arrastre */
    unsigned int us_autoprova;               /* lo que tardó la autoprueba del arranque (0 = no se hizo) */
    unsigned int us_gpu, us_cpu;             /* la comparación de velocidad del arranque (0 = no se hizo) */
    unsigned int confirmada;                 /* D244: 1 = tarjeta confirmada; 0 = «por autoprueba» */
    unsigned int apagado;                    /* D244: 0 = no; 1 = lo apagó el vigilante; 2 = a mano */
    unsigned int prazo_ms;                   /* D244: plazo máximo de cada espera de la GPU en sesión */
    unsigned int reservado[2];
} info_acel2d_t;
#define INFO_ACEL2D_CTL   69  /* D244: (op, 0): op 0 = apagar, 1 = encender -> 0 / error */
/* Cámara (D277): INFO_CAMARA (&info_camara_t, sizeof) -> el campo "resultado" y el valor devuelto
 * (>= 0 bien, < 0 error). La orden va en el primer campo. Una sola cámara a la vez: la UVC integrada
 * o enchufada, o la fuente de prueba del kernel (un generador de patrón que pasa por el mismo montador
 * de cuadros, para probar sin cámara). Los servicios (de_servico) no pueden usarla.
 *   CAM_ESTADO  describe la fuente pedida (fuente 0 = la real, 2 = la de prueba): presente, nombre y modos.
 *   CAM_ABRIR   empieza a capturar con el modo "modo" (índice de ESTADO); la fuente 0 es la real y la 2 la de prueba.
 *   CAM_CUADRO  copia el último cuadro completo a buf si su seq es distinta de seq_visto (YUY2: larg*alt*2 bytes
 *               en orden Y0 U Y1 V; MJPEG: el JPEG tal cual); devuelve los bytes copiados, 0 = no hay cuadro nuevo.
 *   CAM_CERRAR  deja de capturar.
 *   CAM_AJUSTE  brillo/contraste de la unidad de procesamiento (control 0 o 1; escribir = 1 pone "valor"). */
#define INFO_CAMARA       70
#define CAM_ESTADO  0
#define CAM_ABRIR   1
#define CAM_CUADRO  2
#define CAM_CERRAR  3
#define CAM_AJUSTE  4
#define CAM_FUENTE_REAL   0
#define CAM_FUENTE_PRUEBA 2
#define CAM_FMT_YUY2   1
#define CAM_FMT_MJPEG  2
#define CAM_MODOS_MAX  24
#define CAM_CTL_BRILLO    0
#define CAM_CTL_CONTRASTE 1
typedef struct {
    unsigned short larg, alt;
    unsigned char  formato;     /* CAM_FMT_* */
    unsigned char  fps;
    unsigned char  reservado[2];
} info_camara_modo_t;
typedef struct {
    unsigned int   orden;       /* CAM_* */
    int            resultado;   /* lo mismo que devuelve la llamada */
    unsigned int   fuente;      /* CAM_FUENTE_* */
    unsigned int   modo;        /* ABRIR: índice del modo; ESTADO: el modo abierto */
    unsigned int   presente;    /* ESTADO: la fuente existe */
    unsigned int   abierta;     /* ESTADO: está capturando */
    unsigned int   n_modos;
    char           nome[48];
    info_camara_modo_t modos[CAM_MODOS_MAX];
    void          *buf;         /* CUADRO: donde copiar */
    unsigned int   buf_tam;
    unsigned int   seq_visto;   /* CUADRO: la seq del último cuadro que ya se tenía */
    unsigned int   seq;         /* CUADRO: seq del cuadro copiado; ESTADO: cuadros completos hasta ahora */
    unsigned short larg, alt;   /* CUADRO: tamaño del cuadro copiado */
    unsigned char  formato;     /* CUADRO: CAM_FMT_* del cuadro copiado */
    unsigned char  controles;   /* ESTADO: bit 0 brillo, bit 1 contraste disponibles */
    unsigned short reservado;
    unsigned int   control;     /* AJUSTE: CAM_CTL_* */
    unsigned int   escribir;    /* AJUSTE: 1 = poner "valor" */
    int            valor, minimo, maximo, defecto;   /* AJUSTE */
    unsigned int   descartados, paquetes, errores;   /* ESTADO: diagnóstico del flujo */
} info_camara_t;

/* 3D por la GPU (D285): la capa común (kernel/gui/gpu3d.c). SaguiGL manda los
 * triángulos ya recortados y en coordenadas del destino; el kernel los pasa al
 * driver de la tarjeta (Intel Gen3...) y devuelve el cuadro. Los números 71 a 74
 * (el 70 es INFO_CAMARA). */
#define INFO_GPU3D        71  /* (&info_gpu3d_t, sizeof) -> 0 */
#define INFO_GPU3D_OP     72  /* (&gpu3d_pedido_t, sizeof) -> 0, GPU3D_SEM_SUPORTE (> 0) o error (< 0) */
#define INFO_GPU3D_PROVA  73  /* (&info_gpu3d_prova_t, sizeof) -> 0: autoprueba y medida, ahora */
#define INFO_GPU3D_CTL    74  /* (op, 0): 0 = apagar, 1 = encender (lotes), 2 = encender con las órdenes en el anillo */

/* Captura de sonido / micrófono (D292). Una sola llamada, como la cámara: info_sistema(INFO_CAPTURA,
 * &info_captura_t, sizeof). El campo "orden" dice qué se pide; el resultado va también en "resultado".
 * Los servicios (de_servico) no pueden grabar (ERRO_ACESSO). Una captura abierta a la vez, de un proceso.
 * Las muestras son PCM de 16 bits con signo, entrelazadas; el kernel convierte desde la tasa nativa de
 * la tarjeta (48 kHz) a la pedida (8000 a 48000 Hz, p. ej. 44100) y a mono o estéreo.
 *   CAP_ESTADO    describe la fuente pedida (CAP_FUENTE_REAL / CAP_FUENTE_PRUEBA): presente, nombre, entradas,
 *                 entrada elegida, ganancia, silencio, nivel (pico y RMS de los últimos ~50 ms), cuadros perdidos.
 *   CAP_ABRIR     empieza a capturar (taxa, canais) -> 0 / ERRO_OCUPADO / ERRO_NAO_EXISTE.
 *   CAP_LER       copia como mucho buf_tam bytes ya convertidos a buf (no bloquea) -> bytes copiados (0 = aún no hay).
 *   CAP_FECHAR    para la captura.
 *   CAP_ENTRADA   escribir = 1: elige la entrada "entrada" (índice de ESTADO); vale con la captura cerrada.
 *   CAP_GANANCIA  escribir = 1: ganancia 0-100 (valor) y silencio (mudo); vale con la captura cerrada.
 * La fuente de prueba es un tono de 1 kHz (amplitud 8000) generado en el kernel con el mismo
 * canal de conversión: sirve para probar sin micrófono. */
#define INFO_CAPTURA      75  /* (&info_captura_t, sizeof) -> 0 / error (< 0) */
#define CAP_ESTADO    0
#define CAP_ABRIR     1
#define CAP_LER       2
#define CAP_FECHAR    3
#define CAP_ENTRADA   4
#define CAP_GANANCIA  5
#define CAP_FUENTE_REAL   0
#define CAP_FUENTE_PRUEBA 2
#define CAP_ENTRADAS_MAX  8
#define CAP_TONO_PRUEBA   1000   /* Hz */
#define CAP_TONO_AMPLITUD 8000
typedef struct {
    unsigned int   orden;        /* CAP_* */
    int            resultado;    /* lo mismo que devuelve la llamada */
    unsigned int   fuente;       /* CAP_FUENTE_* */
    unsigned int   taxa;         /* ABRIR: Hz pedidos; ESTADO: tasa nativa */
    unsigned int   canais;       /* ABRIR: 1 o 2 */
    unsigned int   presente;     /* ESTADO: la fuente existe */
    unsigned int   abierta;      /* ESTADO: está capturando */
    unsigned int   n_entradas;   /* ESTADO */
    unsigned int   entrada;      /* ESTADO: la elegida; ENTRADA: la que se pide */
    unsigned int   ganancia;     /* ESTADO: 0-100; GANANCIA: la que se pide */
    unsigned int   mudo;         /* ESTADO: silencio; GANANCIA: el que se pide */
    unsigned int   escribir;     /* ENTRADA y GANANCIA: 1 = cambiar */
    char           nome[48];     /* ESTADO: la tarjeta */
    char           entradas[CAP_ENTRADAS_MAX][24];   /* ESTADO: nombres de las entradas */
    unsigned int   pico, rms;    /* ESTADO: 0-32767, de los últimos ~50 ms */
    unsigned int   cuadros;      /* ESTADO: cuadros nativos capturados desde que se abrió */
    unsigned int   perdidos;     /* ESTADO: cuadros nativos perdidos (nadie leyó a tiempo) */
    void          *buf;          /* LER */
    unsigned int   buf_tam;      /* LER */
} info_captura_t;

/* Estados de la capa. */
#define GPU3D_SIN_PROBAR   0  /* hay driver; se enciende y se prueba con el primer uso */
#define GPU3D_ATIVO        1
#define GPU3D_FALHOU       2  /* la autoprueba falló o el vigilante lo apagó: software */
#define GPU3D_APAGADO      3  /* a mano («gpu3d apagar») */
#define GPU3D_SEM_GPU      4  /* ninguna tarjeta con driver 3D */
#define GPU3D_DESATIVADO   5  /* la opción de arranque (SGL_MOTOR=software) */

#define GPU3D_SEM_SUPORTE  1  /* la GPU no sabe hacer eso (textura grande, destino que no cabe): quien
                                 llama sigue por software, sin apagar el 3D para los demás */
#define GPU3D_TEXTURAS     8  /* huecos de textura por dueño */
#define GPU3D_MAX_LADO  2048  /* lado máximo del destino y de una textura */
#define GPU3D_MAX_VERT  3072  /* vértices por petición (múltiplo de 3) */

/* Un vértice: ocho palabras de 32 bits. x, y, z, rhw, u y v son los bits de un
 * float IEEE-754 (el kernel no usa coma flotante: los copia). x e y en píxeles
 * del destino (origen arriba a la izquierda, centros en .5), z de 0 a 1, rhw =
 * 1/w (corrección de perspectiva), cor = 0xAARRGGBB, espec = 0x00RRGGBB que se
 * suma al final (la niebla la calcula quien llama), u y v de 0 a 1. */
typedef struct { unsigned int x, y, z, rhw, cor, espec, u, v; } gpu3d_vert_t;

#define G3D_Z_TESTE      0x01
#define G3D_Z_ESCRITA    0x02
#define G3D_MEZCLA       0x04
#define G3D_TEXTURA      0x08
#define G3D_TEX_REPETE_S 0x10   /* si no, se sujeta al borde */
#define G3D_TEX_REPETE_T 0x20
#define G3D_TEX_LINEAR   0x40   /* si no, el vecino más próximo */
#define G3D_Z_MENOR       0
#define G3D_Z_MENOR_IGUAL 1
#define G3D_Z_SEMPRE      2
#define G3D_F_ZERO        0
#define G3D_F_UM          1
#define G3D_F_ALFA        2     /* alfa del origen */
#define G3D_F_UM_MENOS_ALFA 3
typedef struct {
    unsigned int  flags;        /* G3D_* */
    unsigned char func_z;       /* G3D_Z_MENOR... */
    unsigned char mez_orig, mez_dest;   /* G3D_F_* */
    unsigned char textura;      /* hueco */
} gpu3d_estado_t;

#define GPU3D_OP_ABRIR      1   /* a = ancho, b = alto, c = bits de Z (16 o 24): toma el motor para este proceso */
#define GPU3D_OP_FECHAR     2
#define GPU3D_OP_LIMPAR     3   /* a = 1 color | 2 Z, b = color 0x00RRGGBB (Z siempre al fondo) */
#define GPU3D_OP_TEXTURA    4   /* a = hueco, b = ancho, c = alto, ptr = píxeles 0xAARRGGBB (ptr 0: soltarla) */
#define GPU3D_OP_ESTADO     5   /* ptr = gpu3d_estado_t */
#define GPU3D_OP_TRIANGULOS 6   /* ptr = gpu3d_vert_t[], a = vértices (múltiplo de 3) */
#define GPU3D_OP_LER        7   /* ptr = destino 0x00RRGGBB, a = píxeles por fila del destino: espera a la GPU y copia */
typedef struct { unsigned int op, a, b, c, ptr, bytes; } gpu3d_pedido_t;

typedef struct {
    unsigned int versao;        /* 1 */
    unsigned int estado;        /* GPU3D_* */
    unsigned int fabricante, modelo;
    char         nome[16];      /* "intel-gen3", "simulado"... o "software" */
    char         placa[40];
    char         motivo[160];
    unsigned int dono;          /* pid que tiene el motor (0 = libre) */
    unsigned int larg, alt, zbits;   /* destino abierto */
    unsigned int triangulos, lotes, quadros, falhas, recusas;   /* desde el arranque */
    unsigned int us_autoprova;
    unsigned int memoria_kb, livre_kb;   /* memoria de GPU de la capa */
    unsigned int variante;      /* 0 = lotes (batch buffers), 1 = órdenes en el anillo */
    unsigned int prazo_ms;      /* plazo del vigilante */
    unsigned int pontos;        /* última puntuación 3D medida (0 = sin medir) */
    unsigned int tri_s, kpix_s; /* y sus medidas */
    unsigned int reservado[6];
} info_gpu3d_t;

#define GPU3D_N_PROVAS 8
typedef struct {
    unsigned int versao;        /* entrada: 2 = con detalle en el log; salida: 1 */
    int          resultado;     /* 1 = bien, 0 = mal, -1 = sin GPU con 3D */
    char         texto[160];
    unsigned int provas;        /* bit n = la prueba n pasó */
    unsigned int feitas;        /* bit n = la prueba n se hizo */
    int          falhou;        /* primera que falló (-1 = ninguna) */
    int          x, y;          /* píxel que no cuadra */
    unsigned int esperado, obtido;
    unsigned int us_autoprova;
    /* medida: triángulos pequeños (transformación y órdenes) y grandes (relleno) */
    unsigned int tri_pequenos, us_pequenos, tri_grandes, us_grandes, pix_grandes;
    unsigned int us_leitura, pix_leitura;   /* traer el cuadro a la RAM */
    unsigned int tri_s, kpix_s, pontos;
    unsigned int reservado[6];
} info_gpu3d_prova_t;
typedef struct {
    unsigned int versao;        /* 1 */
    int          resultado;     /* 1 = la GPU dio los mismos píxeles que la CPU; 0 = no; -1 = sin acelerador */
    unsigned int ordens;        /* copias y rellenos comprobados */
    unsigned int diferentes;    /* píxeles distintos */
    unsigned int soma_gpu, soma_cpu;   /* suma de los píxeles de la zona de prueba */
    unsigned int us_gpu, us_cpu;       /* medida: las mismas copias por la GPU y por la CPU */
    char         texto[160];    /* resumen para la orden */
    /* D243: medida por tipo de orden (infopc, pestaña GPU): 0 rellenos, 1 copias sin solape,
     * 2 copias solapadas. Las mismas órdenes por la GPU y por la CPU, con la suma de la zona. */
    unsigned int bn;            /* pruebas hechas (0 = no se midió) */
    struct { unsigned int ops, pixeis, us_gpu, us_cpu, soma_gpu, soma_cpu; } b[3];
} info_acel2d_prova_t;

/* La vía de cada IRQ (D211): por qué controlador y qué GSI llega cada
 * línea, con sus funciones PCI. Una fila por función PCI con interrupción y
 * una por cada línea ISA en uso que no tiene funciones PCI. El 68 (el 65
 * queda libre para otras ramas). */
#define INFO_IRQ_VIAS 68   /* (info_irq_via_t[], n) -> cuántas filas */
#define IRQ_VIA_PIC     0  /* sin IOAPIC: el 8259 de siempre */
#define IRQ_VIA_IOAPIC  1  /* por el IOAPIC */
#define IRQ_VIA_MISTA   2  /* con el IOAPIC activo, la línea entra por el 8259 (vuelta atrás de la autoprueba) */
#define IRQ_PROVA_NADA  0  /* sin autoprueba (el dispositivo no sabe provocar una interrupción) */
#define IRQ_PROVA_OK    1  /* la interrupción provocada llegó */
#define IRQ_PROVA_PIC   2  /* no llegó por el IOAPIC; por el 8259 sí */
#define IRQ_PROVA_FALHA 3  /* no llegó por ninguna vía: el driver sondea */
#define IRQ_PROVA_OCUPADO 4 /* no se pudo provocar (dispositivo ocupado) */
typedef struct {
    unsigned char irq;          /* la línea (vector 0x20 + irq) */
    unsigned char via;          /* IRQ_VIA_* */
    unsigned char apic;         /* el APIC de la CPU que la recibe */
    unsigned char prova;        /* IRQ_PROVA_* */
    unsigned char bus, disp, fun;   /* la función PCI; bus = 0xFF en una fila ISA */
    unsigned char pino;         /* 1-4 = INTA-INTD; 0 = ISA */
    unsigned int  gsi;          /* la GSI por la que llega (0xFFFFFFFF = ninguna: solo el 8259) */
    unsigned int  atendidas;    /* interrupciones atendidas en la línea */
    unsigned char prt;          /* 1 = la GSI la dio el _PRT; 0 = la línea de la BIOS (D166) */
    unsigned char nivel, baixo; /* forma de la entrada */
    unsigned char tormenta;     /* D244: 1 = enmascarada ahora por una tormenta */
    char          nome[12];     /* "EHCI", "e1000", "HDA"... si tiene autoprueba */
    unsigned int  por_segundo;  /* D244: interrupciones por segundo (última ventana) */
    unsigned int  tormentas;    /* D244: tormentas detectadas en la línea desde el arranque */
} info_irq_via_t;

/* Los núcleos (D157): uno por procesador, con su estado y su uso.
 * El número 61 (el 57 lo tiene INFO_MOTOR_GPU y el 58 INFO_APARENCIA). */
#define INFO_NUCLEOS  61  /* (info_nucleo_t[], n) -> quantas CPUs: uma por processador (D157) */
/* El driver de frecuencia por CPU (D175): una entrada por procesador. */
#define INFO_FREQ_NUCLEOS 62  /* (info_freq_nucleo_t[], n) -> cuántas CPU (ERRO_NAO_EXISTE sin driver) */
/* La pila que de verdad ocupa cada proceso (D186): las pilas crecen al
 * tocarlas, así que esto es lo que llegó a usar (no se devuelve al bajar). */
#define INFO_PILHAS   63  /* (info_pilha_t[], n) -> cuántos procesos */
typedef struct {
    int          pid;
    unsigned int pilha_kb;   /* la del hilo principal */
    unsigned int hilos_kb;   /* la suma de las de sus hilos con pila del kernel */
} info_pilha_t;

/* INFO_AUDIO */
#define AUDIO_SEM_PLACA   0   /* nenhuma placa de som no PCI */
#define AUDIO_FUNCIONANDO 1
#define AUDIO_SEM_DRIVER  2   /* há placa (fabricante/modelo), mas sem driver ou o driver falhou */
typedef struct {
    int           pid;
    unsigned int  taxa;
    unsigned char canais, bits, volume, reservado;
    unsigned int  pendente_ms;
} info_fluxo_audio_t;
#define AUDIO_SALIDAS 4
typedef struct {
    unsigned char usada, actual, reservado[2];   /* hay tarjeta en ese índice; es la salida actual */
    char          driver[8];                     /* "hda", "ac97" o "usb" */
    char          nombre[48];
} info_salida_audio_t;
typedef struct {
    unsigned char estado;            /* AUDIO_SEM_PLACA, AUDIO_FUNCIONANDO, AUDIO_SEM_DRIVER */
    unsigned char irq;               /* 0xFF = sondagem a cada tique do relógio */
    unsigned char volume, silencio;  /* volume mestre (0-100) e se está calado */
    char          driver[8];         /* "ac97" ou "hda" */
    char          placa[48];         /* o controlador */
    char          codec[64];         /* o códec e a saída usada */
    unsigned short fabricante, modelo;   /* PCI do controlador */
    unsigned int  taxa;              /* Hz da saída (48000) */
    unsigned int  tocando;           /* 1 = o DMA da placa está rodando */
    unsigned int  latencia_ms;       /* quanto o misturador escreve à frente da placa */
    unsigned int  quadros;           /* quadros tocados desde o boot (o relógio da placa) */
    unsigned int  interrupcoes, falhas;   /* IRQs da placa; vezes que o misturador se atrasou */
    unsigned int  nfluxos;
    info_fluxo_audio_t fluxo[AUDIO_FLUXOS];
    unsigned char eq_etapas;         /* 0 = ecualizador plano (derivación) */
    signed char   eq_db[EQ_BANDAS];  /* 60 Hz, 250 Hz, 1 kHz, 4 kHz, 12 kHz */
    info_salida_audio_t salidas[AUDIO_SALIDAS];   /* D275: las tarjetas conocidas (PCI y USB) */
    unsigned char balance;           /* D292: 0 izquierda, 100 centro, 200 derecha */
    unsigned char captura;           /* D292: 1 = hay entrada de sonido (micrófono) */
    unsigned short latencia_base_ms; /* D292: la latencia configurada (latencia_ms puede crecer tras un corte) */
} info_audio_t;

/* SIS_BLUETOOTH (D279) */
#define BT_ESTADO        0   /* (&info_bt_t, sizeof) -> 0; ERRO_NAO_EXISTE si no hay controlador */
#define BT_BUSCAR        1   /* () -> 0: empieza una búsqueda de ~8 s (info_bt_t.buscando) */
#define BT_DISPOSITIVOS  2   /* (disp_bt_t[], n) -> cuántos */
#define BT_EMPAREJAR     3   /* (indice) -> 0: solo emparejar (guarda la clave y se desconecta); pide administrador */
#define BT_CONECTAR      4   /* (indice) -> 0: conectar (y emparejar si hace falta); pide administrador */
#define BT_DESCONECTAR   5   /* (indice) -> 0; pide administrador */
#define BT_OLVIDAR       6   /* (indice) -> 0: desconecta y borra la clave; pide administrador */
#define BT_CONFIRMAR     7   /* (1 = coincide, 0 = no) -> 0: contesta a la comparación numérica; pide administrador */
#define BT_ENERGIA       8   /* (1 = encender, 0 = apagar) -> 0; pide administrador */
/* Estados del adaptador (info_bt_t.estado) */
#define BTA_SEM_ADAPTADOR 0
#define BTA_INICIANDO     1
#define BTA_PRONTO        2
#define BTA_FALHOU        3
#define BTA_APAGADO       4
/* Estados de un dispositivo (disp_bt_t.estado) */
#define BTD_LIVRE         0
#define BTD_LIGANDO       1
#define BTD_AUTENTICANDO  2
#define BTD_CIFRANDO      3
#define BTD_SDP           4
#define BTD_HID           5
#define BTD_CONECTADO     6
#define BTD_ERRO          7
/* Tipo de dispositivo (disp_bt_t.tipo) y modo de la confirmación pendiente (info_bt_t.conf_modo) */
#define BTT_OUTRO   0
#define BTT_TECLADO 1
#define BTT_RATO    2
#define BTT_COMBO   3
#define BTC_NADA     0
#define BTC_COMPARAR 1   /* ¿coincide el número con el del dispositivo? BT_CONFIRMAR 1 o 0 */
#define BTC_MOSTRAR  2   /* escribe el número en el teclado del dispositivo (sin respuesta) */

typedef struct info_bt {
    unsigned int   estado;         /* BTA_* */
    char           chip[40];       /* "CSR8510 A10 (0a12:0001)" */
    unsigned char  addr[6];        /* en orden de lectura: addr[0] es el primer byte de AA:BB:CC:DD:EE:FF */
    unsigned char  buscando;
    unsigned char  energia;
    unsigned char  conf_modo;      /* BTC_* */
    signed char    conf_disp;      /* índice del dispositivo que pide la confirmación (-1 = ninguno) */
    unsigned char  n_disp;
    unsigned int   conf_num;       /* el número de seis cifras de la comparación numérica o del passkey */
    unsigned char  hci_ver, lmp_ver;
    unsigned short fabricante, acl_mtu;
    unsigned char  acl_num, reservado[3];
    unsigned int   eventos, acl_rx, acl_tx, erros_hci;
    char           erro[64];       /* último problema del adaptador ("" = ninguno) */
} info_bt_t;

typedef struct disp_bt {
    unsigned char  addr[6];        /* en orden de lectura */
    unsigned char  estado;         /* BTD_* */
    unsigned char  tipo;           /* BTT_* */
    unsigned char  pareado;        /* hay clave de enlace guardada */
    unsigned char  visto;          /* salió en la última búsqueda */
    signed char    rssi;
    unsigned int   clase;
    char           nome[32];
    char           erro[48];       /* por qué falló el último intento ("" = ninguno) */
} disp_bt_t;

/* SIS_SOCKET */
#define SOCK_TCP 1
#define SOCK_UDP 2
/* SIS_OPCAO_SOCKET */
#define OPC_TEMPO_LIMITE 1   /* valor = ms para ler/aceitar/conectar (0 = sem limite) -> 0 */
#define OPC_FECHAR_ENVIO 2   /* TCP: manda o FIN, mas continua lendo -> 0 */
#define OPC_ENDERECOS    3   /* valor = &enderecos_socket_t -> 0 */
#define OPC_ESTADO       4   /* -> CON_* (TCP) */
/* SIS_WIFI (D215) */
#define WIFI_ESTADO      0   /* (&info_wifi_t, sizeof) -> 0; ERRO_NAO_EXISTE si no hay pincho */
#define WIFI_BUSCAR      1   /* (0) -> 0: empieza una búsqueda (unos 3 s; WIFI_ESTADO.buscando) */
#define WIFI_REDES       2   /* (rede_wifi_t[], n) -> cuántas redes de la última búsqueda */
#define WIFI_CONECTAR    3   /* (&pedido_wifi_t) -> 0: empieza; WIFI_ESTADO dice cómo acaba */
#define WIFI_DESCONECTAR 4   /* (0) -> 0 */

/* Estados (info_wifi_t.estado) */
#define WIFI_SEM_PINCHO   0   /* no hay ningún adaptador */
#define WIFI_INICIANDO    1   /* eFuse, firmware y tablas del chip */
#define WIFI_PRONTO       2   /* encendido, sin conectar */
#define WIFI_CONECTANDO   3   /* autenticación y asociación */
#define WIFI_SAUDACAO     4   /* saludo de cuatro pasos de WPA (WPA1, WPA2 o WPA3) */
#define WIFI_CONECTADO    5
#define WIFI_FALHOU       6   /* el chip no arrancó: ver «erro» */
#define WIFI_SEM_FIRMWARE 7   /* compilación sin firmware (SEM_FIRMWARE=1) o falta el archivo */

/* Seguridad de una red (rede_wifi_t.seguranca), la de kernel/inc/wpa.h */
#define WIFI_ABERTA    0
#define WIFI_WEP       1
#define WIFI_WPA1      2   /* WPA (normalmente TKIP) */
#define WIFI_WPA2      3
#define WIFI_OUTRA     4   /* Enterprise y demás: no se admite */
#define WIFI_WPA3      5   /* solo SAE (D220) */
#define WIFI_WPA2_WPA3 6   /* transición: PSK y SAE (D220) */
#define WIFI_WPA1_WPA2 7   /* mixto: WPA y WPA2 (D220) */
/* Detalles (rede_wifi_t.detalhe e info_wifi_t.detalhe, D220) */
#define WIFI_DET_TKIP   0x01   /* se usa TKIP (de par o de grupo) */
#define WIFI_DET_PMF    0x02   /* 802.11w (obligatoria en la red, o activa en la conexión) */
#define WIFI_DET_H2E    0x04   /* SAE hash-to-element */
#define WIFI_DET_SO_H2E 0x08   /* la red solo admite H2E */
#define WIFI_MAX_PWE    4      /* PWE de SAE recordados por red (uno por punto de acceso) */

typedef struct {
    char           ssid[33];
    unsigned char  bssid[6];
    unsigned char  canal;
    signed char    sinal;          /* dBm */
    unsigned char  seguranca;      /* WIFI_ABERTA... */
    unsigned char  suportada;      /* 1 = se puede conectar (con la clave que haga falta) */
    unsigned char  qualidade;      /* 0..100 */
    unsigned char  oculta;         /* SSID vacío */
    unsigned char  detalhe;        /* WIFI_DET_* (D220) */
    unsigned char  reservado[2];
} rede_wifi_t;

typedef struct {
    unsigned char  bssid[6];
    unsigned char  pwe[64];        /* x || y */
} pwe_wifi_t;

typedef struct {
    char           ssid[33];
    unsigned char  tem_pmk;        /* 1 = red WPA/WPA2: pmk vale; 0 = abierta */
    unsigned char  pmk[32];        /* PBKDF2(contraseña, SSID): la contraseña no entra en el kernel */
    unsigned char  reservado[2];
    /* D220: WEP y WPA3 (SAE). Nada de esto es la contraseña, salvo la clave
     * WEP, que es la clave misma (WEP no tiene derivada). */
    unsigned char  nwep;           /* 0, 5 o 13 bytes */
    unsigned char  wep[13];
    unsigned char  tem_pt;         /* SAE hash-to-element: PT de (contraseña, SSID) */
    unsigned char  npwe;           /* SAE por cazar y picotear: PWE de (contraseña, MAC, BSSID) */
    unsigned char  pt[64];
    pwe_wifi_t     pwe[WIFI_MAX_PWE];
} pedido_wifi_t;

typedef struct info_wifi {
    unsigned int   estado;         /* WIFI_* */
    char           chip[40];       /* "Realtek RTL8188EUS" */
    char           firmware[24];   /* versión del firmware cargado */
    unsigned char  mac[6];
    unsigned char  buscando;       /* 1 = búsqueda en curso */
    unsigned char  canal;
    char           ssid[33];
    unsigned char  bssid[6];
    signed char    sinal;          /* dBm de las últimas balizas */
    unsigned char  qualidade;      /* 0..100 */
    unsigned char  seguranca;
    unsigned char  taxa_mbps;      /* tasa de datos actual (Mb/s; 5 = 5,5) */
    unsigned char  detalhe;        /* WIFI_DET_* de la conexión (D220) */
    unsigned char  reservado[1];
    char           erro[80];       /* el último problema ("" = ninguno) */
    unsigned int   redes;          /* de la última búsqueda */
    unsigned int   rx_quadros, tx_quadros, balizas, erros_usb;
    unsigned int   conectado_s;    /* segundos desde la conexión */
    unsigned int   respostas_sondeo;  /* respuestas de sondeo dirigidas a nosotros: la transmisión funciona */
    /* D238: rendimiento (al final: los programas viejos piden menos y no los ven) */
    unsigned int   rx_bytes, tx_bytes;     /* bytes de datos (tramas Ethernet) entregados a la red y mandados */
    unsigned int   rx_usb, tx_usb;         /* transferencias USB de recepción leídas y tramas mandadas al USB */
    unsigned int   rx_agregadas_max;       /* el máximo de tramas que trajo una transferencia */
    unsigned int   tx_cola_llena, tx_perdidas, addba_rechazados;
    unsigned short taxa_dec;               /* tasa de envío en décimas de Mb/s (65 = 6,5) */
    unsigned short taxa_rx_dec;            /* la de la última trama de datos recibida */
    unsigned char  mcs;                    /* MCS de envío (0..7); 255 = tasa b/g */
    unsigned char  mcs_rx;                 /* MCS de la última trama de datos recibida; 255 = b/g */
    unsigned char  ht;                     /* 1 = conexión 802.11n (HT); 2 = la red es HT pero se conectó b/g */
    unsigned char  sgi;                    /* 1 = GI corto (400 ns) en el envío */
    unsigned char  rx_cola, tx_cola;       /* transferencias de cada cola USB (0 = una cada vez, D215) */
    unsigned char  rx_en_vuelo, tx_en_vuelo;   /* armadas o sin terminar ahora mismo */
} info_wifi_t;

/* SIS_BLOQUEO (D218). La tabla la compila el programa «bloqueo» en
 * /home/.config/bloqueo/tabla.bin (formato en kernel/inc/bloqueo_tabla.h) y el
 * kernel la carga al arrancar y con BLOQ_RECARGAR si bloqueo.conf dice «activo 1». */
#define BLOQ_ESTADO      0   /* (&info_bloqueo_t, sizeof) -> 0 */
#define BLOQ_RECARGAR    1   /* () -> 0; relee bloqueo.conf y tabla.bin (con error, se queda la de antes) */
#define BLOQ_ULTIMOS     2   /* (char[n][64], n) -> cuántos; el más reciente primero */
#define BLOQ_CONSULTAR   3   /* (nombre, BLOQC_*) -> BLOQ_LIBRE / BLOQ_BLOQUEADO / BLOQ_PERMITIDO */
#define BLOQ_CONTADORES  4   /* () -> 0; pone a cero las consultas, las bloqueadas y los últimos */
/* Opciones de BLOQ_CONSULTAR */
#define BLOQC_CONTAR     1   /* cuenta la consulta y, si se bloquea, la anota en los últimos */
#define BLOQC_SIEMPRE    2   /* mira la tabla aunque el bloqueo esté desactivado (si está cargada) */

typedef struct {
    unsigned int   activo;         /* 1 = filtrando (bloqueo.conf dice activo y la tabla está cargada) */
    unsigned int   pedido;         /* 1 = bloqueo.conf dice «activo 1» */
    unsigned int   subdominios;    /* 1 = un dominio de la lista bloquea también sus subdominios */
    unsigned int   bloqueados;     /* entradas de la tabla de bloqueo */
    unsigned int   permitidos;
    unsigned int   bytes;          /* memoria del kernel que ocupa la tabla */
    unsigned int   consultas;      /* nombres consultados desde el arranque (o desde BLOQ_CONTADORES) */
    unsigned int   bloqueadas;
    unsigned int   cargas;         /* veces que se cargó la tabla */
    int            erro;           /* la última carga: 0 = bien; ERRO_* */
    unsigned int   carga_us;       /* lo que tardó la última carga (microsegundos) */
    unsigned int   reservado[5];
} info_bloqueo_t;

/* SIS_CONFIG_REDE */
#define REDE_CONSULTAR    0   /* preenche config_rede_t */
#define REDE_FIXAR        1   /* endereço estático (para o DHCP) */
#define REDE_DHCP         2   /* pede endereço por DHCP e espera até 10 s */
#define REDE_DNS          3   /* só troca o servidor DNS */
#define REDE_EXPERIMENTAL 4   /* liga o driver experimental (RTL810x) */
#define REDE_RENOVAR      5   /* (0) renova (ou pede) o DHCP e volta na hora, sem esperar (D079) */
#define REDE_DESATIVAR    6   /* (0) desliga a eth0: nada entra nem sai; o endereço fica (D079) */
#define REDE_ATIVAR       7   /* (0) religa a eth0 (e renova o DHCP, se era o caso) */
#define REDE_FECHAR       8   /* (id de info_socket_t, no lugar do ponteiro) corta a conexão TCP com RST */
#define REDE_AJUSTAR_DRIVER 9 /* (&ajuste_driver_t) D124: ajustes en caliente del driver experimental (r8169) */
/* Como o endereço foi obtido */
#define REDE_SEM_IP   0
#define REDE_POR_DHCP 1
#define REDE_ESTATICO 2
/* Estados de uma conexão (INFO_CONEXOES e OPC_ESTADO) */
#define CON_FECHADA     0
#define CON_ESCUTANDO   1
#define CON_SYN_ENVIADO 2
#define CON_SYN_RECEBIDO 3
#define CON_ESTABELECIDA 4
#define CON_FECHANDO    5   /* um dos lados já mandou o FIN */
#define CON_UDP         6
/* Estado TCP detalhado (INFO_SOCKETS, D079): os do RFC 793 que a pilha usa
 * (sem TIME-WAIT, ver D046). */
#define TCP_FECHADA       0
#define TCP_ESCUTA        1
#define TCP_SYN_ENVIADO   2
#define TCP_SYN_RECEBIDO  3
#define TCP_ESTABELECIDA  4
#define TCP_FIN_ESPERA    5   /* FIN-WAIT: nós mandamos o FIN */
#define TCP_FECHAR_ESPERA 6   /* CLOSE-WAIT: o outro lado mandou o FIN */
#define TCP_ULTIMO_ACK    7   /* os dois mandaram: falta a última confirmação */

typedef struct {
    unsigned int ip, mascara, roteador, dns;
    unsigned int metodo;           /* REDE_SEM_IP, REDE_POR_DHCP, REDE_ESTATICO */
} config_rede_t;

/* D124: "ifconfig eth0 r8169 rxcfg=... rms=... multi=0|1 buf=... reiniciar-rx".
 * Solo cuenta lo marcado en "mascara" (AJUSTE_*). */
#define AJUSTE_RXCFG     0x01
#define AJUSTE_RMS       0x02
#define AJUSTE_MULTI     0x04
#define AJUSTE_BUF       0x08
#define AJUSTE_REINICIAR 0x10   /* reprograma la tarjeta y rellena los anillos */
typedef struct {
    unsigned int mascara;
    unsigned int rxcfg, rms, multi, buf;
} ajuste_driver_t;

typedef struct {
    unsigned int ip;
    unsigned int porta;
} endereco_rede_t;

typedef struct {
    unsigned int ip_local, porta_local, ip_remoto, porta_remota;
} enderecos_socket_t;

typedef struct {
    unsigned int seq;              /* entrada: número de sequência; tamanho: bytes de dados (0 = 56) */
    unsigned int tamanho;
    unsigned int us;               /* saída: tempo até a resposta, em microssegundos */
    unsigned int ttl;
} info_ping_t;

typedef struct {
    char           nome[8];        /* "eth0" */
    char           driver[12];     /* "e1000"; "" = placa sem driver */
    char           descricao[40];
    unsigned char  mac[6];
    unsigned char  cabo;           /* 1 = enlace ativo */
    unsigned char  ativa;          /* 1 = driver ligado; 0 = só detectada */
    unsigned char  experimental;
    unsigned char  irq;            /* 0xFF = sem interrupção (sondagem a cada 10 ms) */
    unsigned short fabricante, modelo;
    unsigned char  barramento, dispositivo, funcao, dhcp_estado;   /* 0 parado 1 descobrindo 2 pedindo 3 pronto 4 renovando */
    unsigned int   ip, mascara, roteador, dns, metodo;
    unsigned int   servidor_dhcp, concessao_s;   /* segundos que faltam da concessão */
    unsigned int   rx_pacotes, tx_pacotes, rx_bytes, tx_bytes;
    unsigned int   rx_erros, tx_erros, descartados, irqs;
    /* D079: campos novos no fim. INFO_REDE copia só os n bytes pedidos
     * (n = sizeof do programa), então quem foi compilado antes continua igual. */
    unsigned int   desativada;     /* 1 = desligada com REDE_DESATIVAR */
    unsigned int   pode_desativar; /* 1 = REDE_DESATIVAR/REDE_ATIVAR valem nela */
    unsigned int   loopback;       /* 1 = "lo" (127.0.0.1): só conta o tráfego para a própria máquina */
    /* D091: por que os quadros recebidos foram descartados e o diagnóstico
     * do driver experimental (linhas separadas por \n; "" = nada). */
    char           diagnostico[3072];
} info_rede_t;

typedef struct {
    unsigned int   tipo;           /* SOCK_TCP ou SOCK_UDP */
    unsigned int   estado;         /* CON_* */
    unsigned int   ip_local, porta_local, ip_remoto, porta_remota;
    unsigned int   na_fila_rx, na_fila_tx;
    int            pid;            /* dono (0 = sem descritor, terminando) */
} info_conexao_t;

/* INFO_SOCKETS (D079) */
typedef struct {
    info_conexao_t c;              /* o mesmo de INFO_CONEXOES (pid 0 = sem dono) */
    unsigned int   estado_tcp;     /* TCP_* (UDP: TCP_FECHADA) */
    unsigned int   id;             /* para REDE_FECHAR (0 = não dá para fechar daqui) */
    unsigned int   rx_bytes, tx_bytes;   /* dados recebidos e enviados nesta conexão */
} info_socket_t;

/* INFO_TRAFEGO (D079): o kernel guarda uma amostra por segundo, sempre (é só
 * uma subtração por segundo), para o gráfico já abrir com o último minuto. */
#define TRAFEGO_AMOSTRAS 60
typedef struct {
    unsigned int amostras;         /* quantas valem (cresce até 60 depois do boot) */
    unsigned int rx[TRAFEGO_AMOSTRAS], tx[TRAFEGO_AMOSTRAS];   /* bytes em cada segundo; a mais nova no fim */
} info_trafego_t;

typedef struct {
    unsigned int  ip;
    unsigned char mac[6];
    unsigned char completa, reservado;
    unsigned int  idade_s;
} info_arp_t;

typedef struct {
    unsigned long long base;
    unsigned long long tamanho;
    unsigned int       tipo;    /* 1 livre, 2 reservada, 3 ACPI, 4 ACPI NVS, 5 defeituosa */
    unsigned int       extra;   /* atributo estendido ACPI 3.0 (bit 0 = válida) */
} info_e820_t;

typedef struct {
    unsigned int  achou;          /* 0 = sem SMBIOS (o resto vale zero) */
    unsigned char versao_maior, versao_menor, entrada_64, reservado;
    unsigned int  endereco;       /* endereço físico da tabela */
    unsigned int  tamanho;        /* tamanho da tabela segundo a entrada */
    unsigned int  copiado;        /* bytes da tabela que vêm depois deste cabeçalho */
    unsigned int  estruturas;     /* quantidade anunciada (0 na entrada de 64 bits) */
} info_smbios_t;

typedef struct {
    unsigned char  barramento, dispositivo, funcao, revisao;
    unsigned char  classe, subclasse, interface, irq;
    unsigned short fabricante, modelo;
    unsigned short sub_fabricante, sub_modelo;
} info_pci_t;

typedef struct {
    unsigned int grafico;         /* 1 = o estágio 2 trocou para um modo VESA */
    unsigned int escritorio;      /* 1 = a área de trabalho está ativa (dá para abrir janelas) */
    unsigned int modo, largura, altura, bpp, pitch, framebuffer;
    unsigned char verm_tam, verm_pos, verde_tam, verde_pos, azul_tam, azul_pos, reservado[2];
    unsigned int vbe_versao;      /* 0x0300 = VBE 3.0; 0 = sem informação */
    unsigned int memoria_kb;      /* memória de vídeo anunciada pelo VBE */
    char         oem[48], fabricante[32], produto[32], revisao[16];
} info_video_t;

/* SIS_TELAS (D290, mejora 060): varias pantallas. Una "pantalla" es una salida de vídeo; la 0 es la del
 * arranque (la de INFO_TELA, SIS_MUDAR_MODO y el programa pantalla). */
#define TELAS_OP_INFO      0   /* (info_telas_t *, sizeof) -> número de pantallas */
#define TELAS_OP_CONFIG    1   /* (modo | lado << 8 | principal << 16) -> 0 o error; modo/lado: TELAS_MODO_*, TELAS_LADO_* */
#define TELAS_OP_RESOLUCAO 2   /* (pantalla, largura | altura << 16) -> 0 o error (la 0 usa SIS_MUDAR_MODO) */
#define TELAS_OP_GUARDAR   3   /* () -> 0 o error: /home/.config/pantallas.conf */
#define TELAS_MODO_EXTENDER 0
#define TELAS_MODO_ESPELHO  1
#define TELAS_LADO_DIREITA  0  /* las demás a la derecha de la principal */
#define TELAS_LADO_ABAIXO   1  /* las demás debajo de la principal */
#define TELAS_NUM_MAX       4
typedef struct {
    unsigned int qtd;              /* pantallas presentes */
    unsigned int modo, lado, principal;
    unsigned int desk_largura, desk_altura;   /* el escritorio entero */
    struct {
        int x, y;                  /* posición en el escritorio */
        unsigned int largura, altura, max_largura, max_altura;
        unsigned int a_quente;     /* 1 = cambia de resolución sin reiniciar */
        char nome[24];
    } t[TELAS_NUM_MAX];
} info_telas_t;

/* SIS_MODOS_VIDEO: um modo da lista (os do VBE do boot e, com a placa do
 * Bochs/QEMU, os que ela aceita a quente). */
typedef struct {
    unsigned short largura, altura;
    unsigned short modo;          /* número VBE (0 = só pela placa do Bochs/QEMU) */
    unsigned char  bpp;
    unsigned char  flags;         /* MODO_* */
} modo_video_t;
#define MODO_ATUAL     1          /* é o modo da tela agora */
#define MODO_A_QUENTE  2          /* dá para trocar sem reiniciar */
#define MODO_PREFERIDO 4          /* é a resolução gravada no disco de arranque */
#define MODO_VBE       8          /* a BIOS (VBE) oferece: vale para o próximo arranque */
/* SIS_MUDAR_MODO: opções e resultado */
#define MUDAR_AGORA    1          /* trocar já (só com troca a quente) */
#define MUDAR_GUARDAR  2          /* gravar no disco de arranque para os próximos (0x0 = automática) */

typedef struct {
    unsigned int largura, altura, bpp, pitch;
    unsigned int modo;            /* número VBE do modo ligado no boot (0 depois de uma troca a quente) */
    unsigned int a_quente;        /* 1 = placa do Bochs/QEMU/VirtualBox: troca sem reiniciar */
    unsigned int max_largura, max_altura;   /* limites da troca a quente */
    unsigned int pref_largura, pref_altura; /* preferência no disco de arranque (0 = automática) */
    unsigned int pref_gravavel;   /* 1 = achou o disco de arranque para gravar a preferência */
    unsigned int wc;              /* 1 = framebuffer em write-combining (D057) */
    char         wc_via[8];       /* "PAT" ou "MTRR" */
    char         wc_motivo[80];   /* por que não (vazio se ativo) */
    unsigned int memoria_kb;      /* memória de vídeo mapeada pelo kernel */
    unsigned int arranque_cd;     /* 1 = arrancou de um CD: a preferência não se grava (D077) */
    unsigned int wc_mbs;          /* D245: MB/s medidos ao escrever no framebuffer (0 = sem medida) */
    unsigned int fps_tela_cheia;  /* D245: quadros por segundo da tela cheia (0 = sem tela cheia) */
} info_tela_t;

/* INFO_ACPI (D076): só o que se lê sem intérprete de AML. */
#define ACPI_MAX_TABELAS 40
typedef struct {
    char           assinatura[5];  /* "FACP", "APIC"... e zero */
    char           oem[7];         /* OEM ID da tabela */
    unsigned int   endereco, tamanho;
    unsigned char  revisao, soma_ok, reservado[2];
} info_acpi_tabela_t;
typedef struct {
    unsigned int   achou;          /* 1 = RSDP encontrado */
    unsigned int   rsdp;           /* endereço físico do RSDP */
    unsigned int   revisao;        /* 0 = ACPI 1.0 (só RSDT); 2 = 2.0+ (XSDT) */
    char           oem[8];         /* OEM ID do RSDP */
    unsigned int   rsdt;
    unsigned int   xsdt_baixo, xsdt_alto;
    unsigned int   usou_xsdt;      /* a lista veio da XSDT (senão, da RSDT) */
    unsigned int   qtd;
    info_acpi_tabela_t tabela[ACPI_MAX_TABELAS];
} info_acpi_t;

/* INFO_ARRANQUE (D077). */
#define ARRANQUE_MAX_FASES 16
typedef struct {
    unsigned int   disco;          /* número do disco da BIOS (0x80 = primeiro disco ou o CD emulado) */
    unsigned int   cd;             /* 1 = arrancou de um CD (El Torito) */
    unsigned int   leitura_chs;    /* 1 = a BIOS não tinha as extensões do INT 13h (lido por CHS) */
    unsigned int   bpp_pref;       /* profundidade pedida no cabeçalho (0 = automática, D078) */
    unsigned int   res_pref;       /* largura | altura << 16 (D058) */
    unsigned int   opcoes;         /* CAB_OPCOES */
    /* D201: marcas de tiempo del arranque, en ms desde el encendido (TSC; la
     * primera incluye la BIOS y la lectura de la imagen por el INT 13h). */
    unsigned int   fases;
    unsigned int   fase_ms[ARRANQUE_MAX_FASES];
    char           fase_nome[ARRANQUE_MAX_FASES][16];
    unsigned int   firmware;       /* D270: 0 = BIOS, 1 = UEFI x64, 2 = UEFI IA32 (boot_info v2, D264) */
} info_arranque_t;

typedef struct {
    unsigned int       khz;       /* frequência do TSC; 0 = sem medida (CPU sem TSC) */
    unsigned int       janela_ms; /* duração da medida */
    unsigned long long ciclos;    /* ciclos do TSC contados na janela */
} info_tsc_t;
#define INFO_JANELAS  35   /* (info_janela_t[], n) -> quantas; de baixo para cima (D043) */
#define INFO_MONTAGENS 10  /* (info_montagem_t[n]) -> quantas montagens (D040) */

typedef struct {
    unsigned int ram_total_kb;
    unsigned int gerida_kb;
    unsigned int livre_kb;
    unsigned int usada_kb;
} info_memoria_t;

typedef struct {
    int          pid;
    int          pai;
    unsigned int estado;     /* 1 pronto, 2 executando, 3 bloqueado, 4 zumbi */
    unsigned int memoria_kb;
    unsigned int cpu_ms;
    unsigned int usuario;    /* 1 = programa no anel 3 */
    char         nome[32];
    unsigned int janelas;    /* janelas abertas pelo processo (D043) */
    unsigned int cpu;        /* a CPU onde este processo corre (D157) */
    unsigned int n_hilos;    /* quantos fios tem o processo (D157) */
} info_processo_t;

/* Uma entrada por processador (D157). O Administrador de tarefas usa isto
 * para mostrar os núcleos e o uso de cada um. */
typedef struct {
    unsigned int indice;         /* número da CPU (0 = o principal) */
    unsigned int estado;         /* 0 livre, 1 pronta, 2 ociosa */
    unsigned int apic_id;        /* identificador do APIC local */
    unsigned int nucleo;         /* núcleo físico (para agrupar hyperthreads) */
    unsigned int pid;            /* o processo que esta CPU está a executar (0 = ocioso) */
    unsigned int tiques;         /* tiques totais desta CPU */
    unsigned int tiques_ociosos; /* tiques em que esta CPU não tinha processo nenhum */
    unsigned int fatia;          /* tiques que ainda faltam ao processo atual */
    unsigned int n_processos;    /* quantos processos há no sistema todo */
    unsigned int hilo;           /* D174: qué hilo de su núcleo físico es (0, 1...) */
    unsigned int paquete;        /* D174: el paquete (zócalo) */
    unsigned int planificador;   /* D174: el planificador activo (PLANIFICADOR_*) */
} info_nucleo_t;

/* D174: los planificadores (SIS_PLANIFICADOR). Los números son también los
 * de mkimagem.py --planificador (bits 9-11 de las opciones de arranque). */
#define PLANIFICADOR_RR         1   /* round-robin global: el de siempre (por defecto) */
#define PLANIFICADOR_AFINIDAD   2   /* cada proceso vuelve a su CPU; robo solo si la CPU está libre */
#define PLANIFICADOR_EQUILIBRIO 3   /* una cola por CPU y reparto periódico de la carga */
#define PLANIFICADOR_PRIORIDAD  4   /* prioridad (nice) y preferencia por los interactivos */
#define PLANIFICADOR_MAX        4

/* Operaciones de SIS_PLANIFICADOR */
#define PLAN_CONSULTAR   0   /* (0, buf, n) -> el activo (PLANIFICADOR_*); su nombre en buf si buf != 0 */
#define PLAN_ESCOLHER    1   /* (numero) -> el anterior; cambia en caliente */
#define PLAN_NOME        2   /* (numero, buf, n) -> 0; el nombre de ese planificador (ERRO_INVALIDO si no existe) */
#define PLAN_AFINIDADE   3   /* (pid o 0, mascara o -1 para consultar) -> la máscara anterior; 0 = todas las CPU */
#define PLAN_PRIORIDADE  4   /* (pid o 0, nice -20..19) -> 0; se consulta con PLAN_INFO (un nice negativo
                                 * se confundiría con un error) */
#define PLAN_INFO        5   /* (pid/tid o 0, &info_planificacion_t) -> 0 */

/* Lo que el planificador sabe de un proceso o hilo (PLAN_INFO). */
typedef struct {
    int          pid;          /* el número de esta entrada (tid, si es un hilo) */
    unsigned int cpu;          /* la última CPU donde corrió */
    unsigned int mascara;      /* CPU permitidas (bit n = CPU n); 0 = todas */
    int          nice;         /* -20 (más prioridad) .. 19 */
    unsigned int migracoes;    /* veces que cambió de CPU */
    unsigned int execucoes;    /* veces que una CPU lo tomó */
    unsigned int recente;      /* uso reciente de CPU en tiques (el de "prioridad") */
    int          prioridade;   /* la prioridad efectiva ahora (menor = antes) */
    unsigned int lat_media_us; /* de despertar (listo) a correr: la media, en microsegundos */
    unsigned int lat_max_us;   /* y la peor */
    unsigned int despertares;  /* cuántas veces se midió */
    unsigned int lat_total_us; /* la suma (para medias de un tramo: diferencia de dos lecturas) */
} info_planificacion_t;

typedef struct {
    int          id;
    int          pid;        /* dono */
    int          x, y;       /* canto da moldura na tela */
    int          largura, altura;   /* área de conteúdo */
    unsigned int foco;       /* 1 = é a janela de cima */
    char         titulo[48];
} info_janela_t;

typedef struct {
    char         versao[32];
    unsigned int imagem_bytes;   /* tamanho útil da imagem de boot */
    unsigned int segundos_ligado;
    unsigned int largura, altura;
} info_versao_t;

typedef struct {
    char         nome[12];         /* "usb0", "usb0p1", "hda", "hda1" */
    char         fabricante[32];
    char         produto[48];
    char         formato[40];      /* "FAT32", "ext2"... ou "...: formato no soportado" */
    char         rotulo[20];
    unsigned int setores;          /* de 512 bytes */
    unsigned int montado;          /* ele (ou, no disco inteiro, alguma partição) está montado */
    unsigned int suportado;        /* tem um volume que dá para montar */
    /* D040 */
    char         pai[12];          /* "" = disco inteiro */
    char         tipo_fs[8];       /* "fat32", "ext2", "ext3", "ext4" ou "" */
    char         ponto[64];        /* onde está montado ("" = não está) */
    unsigned int particao;         /* 0 = disco inteiro */
    unsigned int flags;            /* DISCO_* abaixo */
} info_disco_t;
#define DISCO_REMOVIVEL   1        /* pendrive USB */
#define DISCO_INTERNO     2        /* disco ATA */
#define DISCO_BOOT        4        /* o disco de onde o SaguiOS ligou */
#define DISCO_SO_LEITURA  8        /* montado só para leitura */
#define DISCO_ARRANQUE_REAL 16     /* D179: es el disco del que arrancó (cabecera del LBA 1), o podría
                                      serlo si eso no se sabe: los cambios piden la confirmación doble */
#define DISCO_TRIM        32       /* D179: admite TRIM y el driver lo manda */
#define DISCO_RESERVADO   64       /* D179: un programa lo tiene reservado (particiones, instalador) */
#define DISCO_CD          128      /* D207: lector de CD/DVD ("cd0") o la imagen El Torito de su disco ("cd0img"): solo lectura */
#define DISCO_XHCI        256      /* D214: pendrive conectado por el controlador xHCI */
#define DISCO_USB3        512      /* D214: pendrive USB 3.x (SuperSpeed) */
#define DISCO_NVME        1024     /* D265: disco NVMe */
#define DISCO_BLOCO4K     2048     /* D265: su bloque real es de 4096 bytes (se presenta en sectores de 512) */

typedef struct {
    char         dispositivo[12];
    char         ponto[64];
    char         tipo[8];          /* "fat32", "ext2"... */
    unsigned int opcoes;           /* MONTAR_SO_LEITURA se só leitura */
    unsigned int total_kb, livre_kb;
} info_montagem_t;

/* SIS_TECLAS_SEGURADAS: posição de algumas teclas no mapa (código de
 * varredura do conjunto 1; 128 + código para as teclas estendidas E0). */
#define CV_ESC        0x01
#define CV_CTRL       0x1D
#define CV_SHIFT      0x2A
#define CV_ALT        0x38
#define CV_ESPACO     0x39
#define CV_B          0x30
#define CV_CTRL_DIR   (0x80 | 0x1D)
#define CV_CIMA       (0x80 | 0x48)
#define CV_BAIXO      (0x80 | 0x50)
#define CV_ESQUERDA   (0x80 | 0x4B)
#define CV_DIREITA    (0x80 | 0x4D)

/* Modos do terminal */
#define TERM_LINHA 0
#define TERM_BRUTO 1

/* No modo bruto, teclas sem caractere chegam como ESC (27) + uma letra; a
 * própria tecla Esc chega como 27, 27. */
#define TECLA_B_CIMA     'A'
#define TECLA_B_BAIXO    'B'
#define TECLA_B_DIREITA  'C'
#define TECLA_B_ESQUERDA 'D'
#define TECLA_B_HOME     'H'
#define TECLA_B_END      'F'
#define TECLA_B_DELETE   'X'
#define TECLA_B_PGUP     'P'
#define TECLA_B_PGDN     'N'
#define TECLA_B_REDESENHAR 'R'   /* não é tecla: outro processo escreveu na tela */

/* Sequências que o terminal entende na saída: ESC [ D (cursor à esquerda),
 * ESC [ C (à direita), ESC [ K (apaga até o fim da linha), ESC [ J (limpa a tela). */

/* Eventos de janela */
#define EV_CLIQUE    1   /* botão apertado; "cliques" = 2 no duplo clique */
#define EV_SOLTOU    2
#define EV_MOVER     3   /* movimento sobre a janela */
#define EV_TECLA     4   /* "tecla": caractere Latin-1, ou 0x100+ para setas etc. */
#define EV_FECHAR    5   /* clicaram no X da moldura */
#define EV_REDIMENSIONAR 10  /* pedem outro tamanho: x = largura, y = altura, tecla = JAN_ESTADO_*.
                                O programa responde com SIS_REDIMENSIONAR_JANELA (D044). */
#define EV_RODA      11  /* roda do mouse (D048): "cliques" = entalhes, como int com sinal
                            (+ para baixo, - para cima); x, y = posição do cursor. Vai para a
                            janela debaixo do cursor, mesmo sem foco (como no Windows 10+).
                            Quem não trata, ignora. Ler com (int)e.cliques. */
#define EV_MENU      12  /* resposta de SIS_MENU_CONTEXTUAL (D051): x = índice da linha escolhida
                            (contando separadores e desativadas, a partir de 0) ou -1 se cancelado;
                            tecla = o "codigo" passado na chamada. */
#define MENU_MAX_ITENS 96   /* linhas num menu de contexto, somando as dos submenus (eram 20 antes de D064;
                               64 hasta D229: el menú Inicio con todas las apps ya pasaba de 64; como mucho 127, menu.c guarda el panel en int8_t) */
#define MENU_MAX_TEXTO 40   /* letras por linha (o resto é cortado) */
#define RODA_LINHAS  3   /* linhas roladas por entalhe nos componentes da libsagui */
#define EV_TELA      16  /* a resolução da tela mudou (D059): x = largura, y = altura. Vai para
                            todas as janelas; as maximizadas e em tela cheia recebem também
                            EV_REDIMENSIONAR. Quem não trata, ignora. */

#define EV_MOVER_CRU 17  /* D242: con el ratón capturado (JAN_CAPTURA): x, y = desplazamiento crudo
                            (con signo, y hacia abajo), botoes = botones apretados. El cursor no se mueve. */
#define EV_CAPTURA_SOLTA 18  /* D242: el sistema soltó la captura (Ctrl+Alt, perdió el foco, menú...) */

/* SIS_PROXIMO_EVENTO: esperar até um instante (ms de SIS_TEMPO, 31 bits) e
 * devolver 0 se nada chegar (D083). */
#define EVENTO_PRAZO 0x80000000u

/* Estilos de janela (SIS_ESTILO_JANELA). Sem chamar, a janela tem tamanho fixo. */
#define JAN_REDIMENSIONAVEL 0x1   /* borda/canto, maximizar, duplo clique no título e F11 */
#define JAN_TELA_CHEIA_OK   0x2   /* aceita F11 mesmo com tamanho fixo (o kernel amplia) */
#define JAN_TELA_CHEIA      0x4   /* entrar em tela cheia agora (sem ele: sair) */
#define JAN_IMPRPANT        0x10  /* quiere recibir la tecla Impr Pant (D125) */
#define JAN_ESC_LIVRE       0x20  /* D204: en tela cheia el Esc llega al programa (sale solo con F11) */
#define JAN_DIALOGO         0x8   /* diálogo (D065): fica fora da barra de tarefas e não minimiza */
#define JAN_SEM_CURSOR      0x40  /* D230: con el foco, el puntero del sistema no se dibuja sobre el contenido
                                     (el programa dibuja el suyo); fuera de él, en menús y paneles, sí */
#define JAN_SO_CURSOR       0x80  /* D230: en SIS_ESTILO_JANELA, cambia solo JAN_SEM_CURSOR (lo demás,
                                     pantalla completa incluida, se queda como está) */
#define JAN_SO_CAPTURA      0x100 /* D242: en SIS_ESTILO_JANELA, cambia solo la captura del ratón:
                                     con JAN_CAPTURA la pide (solo la ventana con foco), sin él la suelta */
#define JAN_CAPTURA         0x200
/* Estado que acompanha EV_REDIMENSIONAR */
#define JAN_ESTADO_NORMAL      0
#define JAN_ESTADO_MAXIMIZADA  1
#define JAN_ESTADO_TELA_CHEIA  2

/* Teclas especiais nos eventos de janela. São os mesmos valores do enum do
 * kernel (teclado.h), que por isso não precisa destas definições. */
#ifndef SAGUI_KERNEL
#define TECLA_CIMA      0x100
#define TECLA_BAIXO     0x101
#define TECLA_ESQUERDA  0x102
#define TECLA_DIREITA   0x103
#define TECLA_HOME      0x104
#define TECLA_END       0x105
#define TECLA_PGUP      0x106
#define TECLA_PGDN      0x107
#define TECLA_INSERT    0x108
#define TECLA_DELETE    0x109
#define TECLA_F11       0x114   /* o gerenciador de janelas usa (tela cheia) */
#define TECLA_ESC       0x116
#define TECLA_SUPER     0x117   /* tecla Windows (E0 5B/5C): o gerenciador de janelas usa (menu Inicio) */
#endif

#define JANELA_LARG_MAX 1000
#define JANELA_ALT_MAX  700

typedef struct {
    unsigned int tipo;
    int          janela;
    int          x, y;        /* relativo à área de conteúdo da janela */
    unsigned int botoes;     /* 1 esquerdo, 2 direito */
    unsigned int cliques;
    unsigned int tecla;
    unsigned int modificadores;   /* 1 Shift, 2 Ctrl; nas teclas também 4 Alt (D075) */
} evento_t;

typedef struct { int x, y, largura, altura; } retangulo_t;

/* Código de saída de um processo encerrado pelo kernel (erro do programa). */
#define SAIDA_ENCERRADO_PELO_KERNEL -1000
#define SAIDA_INTERROMPIDO          130   /* encerrado com Ctrl+C (como 128 + SIGINT nos Unix) */

/* Monitor de hardware (D092, mejora 050). Faixa 54-55. Só leitura: o kernel
 * nunca escreve num MSR, no SMBus nem num Super I/O para isto; a única
 * escrita é o índice SMN dos Ryzen (salvo e restaurado, D092). A leitura
 * acontece só quando alguém pede, e no máximo uma vez por segundo (as
 * chamadas no meio recebem a mesma amostra, com idade_ms). */
#define INFO_SENSORES  54   /* (&info_sensores_t, sizeof) -> 0 */
/* SIS_FUNDO: FUNDO_LINHAS copia n linhas (largura da tela, 0x00RRGGBB) de ptr para a linha y da imagem
 * de fundo; FUNDO_FIM mostra o resultado; FUNDO_PADRAO volta ao degradê. Fora do desktop: ERRO_ACESSO. */
#define FUNDO_PADRAO 0
#define FUNDO_LINHAS 1
#define FUNDO_FIM    2
#define ACENTO_PADRAO 0x0B7A3E   /* o verde do SaguiOS */

#define INFO_APARENCIA 58   /* (&info_aparencia_t, sizeof) -> 0 (D112) */
typedef struct {
    unsigned int acento;         /* 0xRRGGBB */
    unsigned int fundo_imagem;   /* 1 = ha imagem de fundo escolhida */
    unsigned int largura, altura;/* a tela (o tamanho que FUNDO_LINHAS espera) */
} info_aparencia_t;
#define INFO_MSR_PROVA 55   /* (0, 0) -> bit 0: MSR inexistente deu "não disponível"; bit 1: idem o x2APIC ID (0x802) */

#define INFO_BANDEJA   60   /* (&info_bandeja_t, sizeof) -> 0: o que a bandeja da barra mostra agora (D114) */
typedef struct {
    unsigned int versao;          /* 1 */
    unsigned int som;             /* AUDIO_SEM_PLACA, AUDIO_FUNCIONANDO, AUDIO_SEM_DRIVER */
    unsigned int volume, silencio;
    unsigned int rede;            /* 0 sem placa, 1 sem conexão, 2 conectado */
    unsigned int ip;              /* eth0, como em info_rede_t (primeiro octeto nos bits altos) */
    unsigned int temp_ok;         /* 0 = a CPU não informa temperatura (ícone oculto) */
    int          temp_mili_c;
    unsigned int limitada;        /* 1 = marcada como limitada por temperatura */
    unsigned int painel;          /* 0 nenhum, 1 volume, 2 rede, 3 temperatura, 4 batería (D192) */
    unsigned int icones;          /* ícones visíveis */
    /* versión 2 (D192): la batería (bateria.c); sin batería el icono no aparece */
    unsigned int bateria;         /* 1 = hay batería puesta (icono visible) */
    int          bateria_pct;     /* 0-100; -1 = desconocido o sin batería */
    unsigned int bateria_estado;  /* BATERIA_* (cargando, descargando) */
} info_bandeja_t;

#define SENS_FONTE_NENHUMA 0
#define SENS_FONTE_INTEL   1   /* MSR IA32_THERM_STATUS (0x19C) e IA32_PACKAGE_THERM_STATUS (0x1B1) */
#define SENS_FONTE_AMD_K8  2   /* PCI 00:18.3, registro 0xE4 */
#define SENS_FONTE_AMD_K10 3   /* PCI 00:18.3, registro 0xA4 (famílias 10h-16h) */
#define SENS_FONTE_AMD_ZEN 4   /* SMN 0x59800 pelo índice/dado de 00:00.0 (17h/19h; 15h modelos 60h-7Fh: 0xD8200CA4) */
/* info_sensores_t.throttling */
#define SENS_THR_TERMICO   0x01   /* reduzindo agora por temperatura */
#define SENS_THR_REGISTRO  0x02   /* já reduziu desde o arranque (bit de registro) */
#define SENS_THR_PROCHOT   0x04   /* PROCHOT#/FORCEPR# (aviso externo) */
#define SENS_THR_ENERGIA   0x08   /* limite de potência */
#define SENS_THR_SABE      0x80   /* a fonte informa throttling (sem este bit: desconhecido) */
/* info_sensores_t.turbo */
#define SENS_TURBO_ND      0
#define SENS_TURBO_NAO     1   /* a CPU não tem */
#define SENS_TURBO_DESLIG  2   /* tem, mas a BIOS desligou */
#define SENS_TURBO_LIGADO  3
#define SENS_MAX_TEMP      4
#define SENS_MAX_PSTATES   8

typedef struct {
    char         nome[12];        /* "Núcleo 0", "Paquete", "Tctl", "Tdie" */
    int          mili_c;          /* milésimos de grau Celsius */
} info_temp_t;

typedef struct {
    unsigned int versao;          /* 1 */
    unsigned int hipervisor;      /* 1 = o CPUID diz que é uma máquina virtual */
    unsigned int fonte;           /* SENS_FONTE_* */
    char         fonte_txt[56];   /* de onde veio a temperatura */
    char         motivo[88];      /* sem temperatura: por quê */
    unsigned int qtd_temp;
    info_temp_t  temp[SENS_MAX_TEMP];
    int          max_mili_c;      /* TjMax ou limite (0 = desconhecido) */
    char         max_origem[40];  /* "MSR 0x1A2", "tabla por modelo"... */
    unsigned int throttling;      /* SENS_THR_* */
    unsigned int tsc_khz;         /* frequência nominal (TSC medido no arranque) */
    unsigned int real_khz;        /* frequência efetiva por APERF/MPERF (0 = n/d) */
    unsigned int real_janela_ms;  /* intervalo dessa medida */
    unsigned int base_khz, max_khz;   /* CPUID 0x16 (0 = n/d) */
    unsigned int mult_x10;        /* multiplicador atual x 10 (0 = n/d) */
    unsigned int mult_max_x10;    /* máximo sem turbo x 10 */
    unsigned int mult_turbo_x10;  /* turbo com 1 núcleo x 10 */
    int          pstate;          /* AMD: P-state atual (-1 = n/d) */
    unsigned int qtd_pstates;
    unsigned int pstate_khz[SENS_MAX_PSTATES];
    unsigned int vid_mv;          /* tensão pedida (VID), aproximada (0 = n/d) */
    unsigned int turbo;           /* SENS_TURBO_* */
    unsigned int microcodigo;     /* revisão (0 = n/d) */
    unsigned int msr_falhas;      /* #GP que o leitor protegido absorveu desde o arranque */
    unsigned int idade_ms;        /* idade desta amostra */
    unsigned int reservado[8];    /* placa (Super I/O, SMBus): pendente, desligado por padrão */
} info_sensores_t;

/* Control de la frecuencia de la CPU (D106, mejora 061). Driver aparte del
 * monitor: solo elige entre los P-states que la BIOS ya declaró (AMD 10h en
 * adelante: PStateCtl, MSR 0xC0010062, respetando PStateCurLim; Intel con
 * EIST desde Nehalem: el multiplicador de IA32_PERF_CTL). Nunca toca
 * tensiones (VID/FID). Límite de temperatura de 79 °C, que se puede bajar
 * pero no subir: al llegar, el estado más lento hasta bajar de límite - 7. */
#define FREQ_EST_NAO_DISPONIVEL 0   /* CPU sin mecanismo conocido, máquina virtual... (motivo) */
#define FREQ_EST_ATIVO          1
#define FREQ_EST_DESATIVADO     2   /* por la opción de arranque o "frecuencia --desactivar" */
#define FREQ_EST_FALHOU         3   /* una escritura dio #GP: desactivado hasta reiniciar */

#define FREQ_MEC_NENHUM   0
#define FREQ_MEC_AMD      1   /* PStateCtl (0xC0010062) */
#define FREQ_MEC_INTEL    2   /* IA32_PERF_CTL (0x199), multiplicador */
#define FREQ_MEC_SIMULADO 3   /* imagen de prueba (mkimagem.py --frecuencia-simulada): nada de hardware */

#define FREQ_POL_RENDIMENTO 0   /* por defecto: el estado más rápido */
#define FREQ_POL_ECONOMIA   1   /* el más lento */
#define FREQ_POL_DEMANDA    2   /* sube con carga y baja en reposo */

/* Operaciones de SIS_FREQUENCIA */
#define FREQ_OP_POLITICA  1   /* (FREQ_POL_*) */
#define FREQ_OP_LIMITE    2   /* (milésimas de °C): de 40000 a 79000; más de 79000 -> ERRO_INVALIDO */
#define FREQ_OP_DESATIVAR 3   /* deja la CPU en el estado de la BIOS y no escribe más */
#define FREQ_OP_ATIVAR    4   /* vuelve a activarlo (no después de un #GP) */
#define FREQ_OP_FORCAR    5   /* (0/1): subir aunque no se pueda leer la temperatura */
#define FREQ_OP_SIM_TEMP  6   /* solo simulado: (milésimas de °C, o FREQ_TEMP_ND) */
#define FREQ_OP_SIM_USO   7   /* solo simulado: uso de CPU en % para «a demanda» (-1 = el real) */
#define FREQ_OP_SIM_GP    8   /* solo simulado: (CPU) la próxima escritura de esa CPU va a un MSR inexistente (#GP) */
#define FREQ_OP_COORDENACAO 9 /* (FREQ_COORD_*): un estado por núcleo físico o el mismo en todo el paquete (D175) */
#define FREQ_OP_SIM_USO_CPU 10 /* solo simulado: (CPU << 16 | uso en %, 0xFFFF = el real) (D175) */

#define FREQ_COORD_NUCLEO  0   /* de serie: cada núcleo físico según su carga */
#define FREQ_COORD_PAQUETE 1   /* todas las CPU el mismo estado (el de la más cargada) */
#define FREQ_TEMP_ND      ((int)0x80000000)

#define FREQ_MAX_ESTADOS  8

typedef struct {
    unsigned int versao;           /* 1 */
    unsigned int estado;           /* FREQ_EST_* */
    unsigned int mecanismo;        /* FREQ_MEC_* */
    char         mecanismo_txt[48];
    char         motivo[88];       /* sin control: por qué */
    unsigned int politica;         /* FREQ_POL_* */
    unsigned int qtd_estados;
    unsigned int estado_khz[FREQ_MAX_ESTADOS];   /* 0 = el más rápido (0 kHz = desconocida) */
    char         estado_nome[FREQ_MAX_ESTADOS][8];   /* "P0", "x22"... */
    int          atual;            /* estado que dice la CPU ahora (-1 = n/d) */
    int          pedido;           /* último estado escrito (-1 = ninguno) */
    int          bios;             /* el que dejó la BIOS */
    int          rapido;           /* el más rápido que la plataforma permite (PStateCurLim) */
    int          limite_mc;        /* límite de temperatura (79000 de serie, nunca más) */
    int          retorno_mc;       /* vuelve a subir por debajo de esto */
    int          temp_mc;          /* última lectura (FREQ_TEMP_ND = sin lectura) */
    unsigned int temp_simulada;    /* 1 = la temperatura es la inyectada (simulado) */
    unsigned int limitada;         /* 1 = limitada por temperatura ahora */
    unsigned int vezes_limitada;   /* veces desde el arranque */
    unsigned int forcado;          /* 1 = sube sin lectura de temperatura */
    unsigned int uso_cpu;          /* % del último periodo (lo que ve «a demanda») */
    unsigned int periodo_ms;       /* cada cuánto decide */
    unsigned int escritas;         /* escrituras en la CPU desde el arranque */
    unsigned int origem_desativado; /* 1 = opción de arranque, 2 = /home/.frecuencia o /etc/frecuencia, 3 = comando */
    unsigned int cpus;             /* versión 2 (D175): CPU con el driver (las arrancadas; detalle en INFO_FREQ_NUCLEOS) */
    unsigned int coordenacao;      /* versión 2: FREQ_COORD_* */
    unsigned int reservado[6];
} info_frequencia_t;

/* INFO_FREQ_NUCLEOS (D175). Cada CPU escribe su propio registro de P-state;
 * «atual» es lo que dice el registro de estado de ESA CPU, que puede ser más
 * rápido que lo pedido si el hardware comparte el estado con otra (SMT,
 * módulo o paquete: manda el más rápido). */
typedef struct {
    unsigned int cpu;        /* número de la CPU (0 = la principal) */
    unsigned int ligada;     /* 1 = arrancada (con --smp); 0 = apagada: el resto no vale */
    unsigned int nucleo;     /* núcleo físico (dominio del P-state por núcleo) */
    int          atual;      /* estado según el registro de estado de esa CPU (-1 = aún sin leer) */
    int          pedido;     /* último estado escrito por esa CPU (-1 = ninguno) */
    int          alvo;       /* lo que la política decidió para ella (-1 = nada aún) */
    int          bios;       /* el que dejó la BIOS en esa CPU (-1 = aún sin leer) */
    unsigned int uso;        /* % del último periodo */
    unsigned int khz;        /* frecuencia del estado actual (0 = n/d) */
    unsigned int escritas;   /* escrituras de esa CPU desde el arranque */
    unsigned int pendente;   /* 1 = aún no aplicó lo último que se le pidió (lo hace en su tic) */
    unsigned int reservado[3];
} info_freq_nucleo_t;
/* INFO_ENERGIA (D113): apagado por ACPI e botão de energia. */
#define INFO_ENERGIA   59   /* (&info_energia_t, sizeof) -> 0 */
#define ENERGIA_SEM_ACPI     0   /* não há RSDP/FADT */
#define ENERGIA_TABELAS_MAS  1   /* FADT ou DSDT com soma errada, ou registros PM1 ausentes */
#define ENERGIA_SEM_S5       2   /* não achei um \_S5_ que se leia sem intérprete de AML */
#define ENERGIA_DISPONIVEL   3
#define ENERGIA_DESATIVADO   4   /* opção de arranque (mkimagem.py --sem-acpi) ou comando "acpi desactivar" */
typedef struct {
    unsigned int   estado;          /* ENERGIA_* */
    unsigned char  slp_typ_a, slp_typ_b;   /* do pacote \_S5_ */
    unsigned char  botao;           /* 1 = botão de energia fixo atendido pelo SCI */
    unsigned char  modo_acpi;       /* 0 = não ativado, 1 = já estava (SCI_EN), 2 = ativado pelo SaguiOS */
    char           origem_s5[5];    /* "DSDT" ou "SSDT" */
    unsigned char  reduzido;        /* FADT HW_REDUCED_ACPI */
    unsigned char  botao_metodo;    /* FADT PWR_BUTTON: botão como dispositivo AML (não atendido) */
    unsigned char  reservado;
    unsigned int   sci;             /* IRQ do SCI (FADT) */
    unsigned int   smi_cmd, acpi_enable;
    unsigned int   pm1a_evt, pm1b_evt, pm1a_cnt, pm1b_cnt;   /* portas de E/S (0 = não há) */
    unsigned int   pm1_evt_len;
    unsigned int   pulsacoes;       /* vezes que o botão foi apertado */
    unsigned int   sci_total;       /* SCIs atendidos */
    unsigned int   fadt_revisao;
    char           motivo[64];      /* por que não está disponível */
} info_energia_t;

/* INFO_BATERIA (D192): la batería del portátil, leída con el intérprete de AML
 * (uACPI) de los dispositivos ACPI PNP0C0A (batería) y ACPI0003 (cargador).
 * Con varias baterías, las capacidades y el consumo son la suma; los textos,
 * de la primera. Lo que no se sabe vale BATERIA_DESCONHECIDO (o -1). */
#define INFO_BATERIA   64   /* (&info_bateria_t, sizeof) -> 0 */
#define UACPI_DESLIGADO   0   /* imagen sin --uacpi (por defecto), con --sem-acpi o sin modo ACPI */
#define UACPI_INICIANDO   1   /* cargando el espacio de nombres (unos segundos tras el arranque) */
#define UACPI_ATIVO       2
#define UACPI_FALHOU      3   /* uACPI no pudo cargar: el sistema sigue sin él (motivo) */
#define BATERIA_DESCARREGANDO 1   /* bits de _BST */
#define BATERIA_CARREGANDO    2
#define BATERIA_CRITICA       4
#define BATERIA_DESCONHECIDO  0xFFFFFFFFu
typedef struct {
    unsigned int versao;          /* 1 */
    unsigned int uacpi;           /* UACPI_* */
    unsigned int baterias;        /* dispositivos PNP0C0A que hay (puestas o no) */
    unsigned int presente;        /* 1 = hay al menos una batería puesta (_STA bit 4) */
    unsigned int adaptador;       /* 0 = no hay ACPI0003; 1 = desenchufado; 2 = enchufado */
    unsigned int estado;          /* BATERIA_* de _BST (0 = ni carga ni descarga: llena y enchufada) */
    int          porcentagem;     /* 0-100; -1 = desconocido */
    int          minutos;         /* restantes (descargando) o hasta llenarse (cargando); -1 = desconocido */
    unsigned int unidade_ma;      /* 0 = mWh y mW; 1 = mAh y mA (_BIF/_BIX "Power Unit") */
    unsigned int projeto;         /* capacidad de diseño */
    unsigned int total;           /* capacidad de la última carga completa */
    unsigned int restante;        /* capacidad que queda (_BST) */
    unsigned int taxa;            /* consumo o corriente de carga (_BST) */
    unsigned int tensao_mv;       /* tensión actual (_BST) */
    unsigned int projeto_mv;      /* tensión de diseño */
    unsigned int aviso, baixo;    /* niveles de aviso y de batería baja de la BIOS */
    unsigned int ciclos;          /* ciclos de carga (_BIX; BATERIA_DESCONHECIDO con _BIF) */
    unsigned int leituras;        /* veces que se ha leído _BST */
    unsigned int notificacoes;    /* avisos Notify del firmware (0x80/0x81) */
    unsigned int idade_ms;        /* tiempo desde la última lectura */
    unsigned int ec;              /* 1 = hay controlador embebido (PNP0C09) atendido */
    unsigned int consultas_ec;    /* eventos del EC atendidos (_Qxx) */
    char         modelo[24], serie[16], tipo[12], fabricante[24];
    char         motivo[64];      /* por qué uACPI no está activo o la batería no se lee */
} info_bateria_t;

#endif
