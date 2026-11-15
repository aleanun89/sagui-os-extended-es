// SPDX-License-Identifier: GPL-2.0-or-later
/* bluetooth - el Bluetooth con un controlador USB estándar, p. ej. el CSR8510 (D279, mejora 059).
 *
 *   bluetooth [estado]            estado del adaptador y de la pila
 *   bluetooth buscar              busca dispositivos (unos 8 s) y los lista
 *   bluetooth lista               los dispositivos conocidos
 *   bluetooth emparejar N|MAC     empareja (guarda la clave de enlace) sin dejarlo conectado
 *   bluetooth conectar N|MAC      conecta teclado o ratón (empareja antes si hace falta)
 *   bluetooth desconectar N|MAC
 *   bluetooth olvidar N|MAC       desconecta y borra la clave
 *   bluetooth confirmar si|no     contesta a la comparación numérica del emparejamiento
 *   bluetooth encender | apagar
 *   bluetooth --ventana           la ventana «Bluetooth» (la del menú Inicio)
 *
 * N es el número de la lista. Todo lo que cambia el sistema (emparejar, conectar, desconectar,
 * olvidar, confirmar, encender y apagar) pide permisos de administrador con el diálogo del
 * sistema (D269); buscar y mirar, no. Las claves se guardan en /etc/bluetooth.conf y en SAGUI-CONF. */
#include "sagui.h"
#include "gui.h"
#include "contas.h"

static int bt(int op, void *p, unsigned n)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_BLUETOOTH), "b"(op), "c"(p), "d"(n), "S"(0), "D"(0) : "memory");
    return r;
}

#define BT_ADM(motivo, op, a) COM_ADMIN(motivo, bt(op, (void *)(long)(a), 0))

static info_bt_t inf;
static disp_bt_t dd[8];
static int nd, r_estado;

static void atualizar(void)
{
    memset(&inf, 0, sizeof inf);
    r_estado = bt(BT_ESTADO, &inf, sizeof inf);
    nd = r_estado == 0 ? bt(BT_DISPOSITIVOS, dd, 8) : 0;
    if (nd < 0) nd = 0;
}

static const char *nome_adaptador(unsigned e)
{
    static const char *const n[] = { "sin controlador", "iniciando", "listo", "con fallo", "apagado" };
    return e < 5 ? n[e] : "?";
}

static const char *nome_tipo(unsigned t)
{
    static const char *const n[] = { "dispositivo", "teclado", "ratón", "teclado y ratón" };
    return t < 4 ? n[t] : "?";
}

static void texto_estado(const disp_bt_t *d, char *o, int max)
{
    switch (d->estado) {
    case BTD_LIVRE: copia_texto(o, d->pareado ? "emparejado" : d->visto ? "visible" : "sin conexión", (size_t)max); break;
    case BTD_LIGANDO: copia_texto(o, "conectando...", (size_t)max); break;
    case BTD_AUTENTICANDO: copia_texto(o, "emparejando...", (size_t)max); break;
    case BTD_CIFRANDO: copia_texto(o, "cifrando...", (size_t)max); break;
    case BTD_SDP: case BTD_HID: copia_texto(o, "abriendo canales...", (size_t)max); break;
    case BTD_CONECTADO: copia_texto(o, "conectado", (size_t)max); break;
    default: formatar_texto(o, (size_t)max, "error: %s", d->erro[0] ? d->erro : "falló la conexión"); break;
    }
}

static void mac(char *o, const unsigned char *a)
{
    formatar_texto(o, 18, "%02x:%02x:%02x:%02x:%02x:%02x", a[0], a[1], a[2], a[3], a[4], a[5]);
}

static int erro_op(const char *que, int r)
{
    if (r == ERRO_NAO_EXISTE) imprimir_em(ERROS, "bluetooth: no hay ningún controlador Bluetooth USB\n");
    else if (r == ERRO_ACESSO) imprimir_em(ERROS, "bluetooth: %s: hace falta ser administrador (usa «admin bluetooth ...»)\n", que);
    else if (r == ERRO_OCUPADO) imprimir_em(ERROS, "bluetooth: %s: el adaptador no está listo o está apagado\n", que);
    else if (r == ERRO_INVALIDO) imprimir_em(ERROS, "bluetooth: %s: dispositivo inexistente\n", que);
    else imprimir_em(ERROS, "bluetooth: %s: rechazado (%d)\n", que, r);
    return 1;
}

/* ------------------------------------------------------------ terminal */

static void mostrar_lista(void)
{
    if (!nd) { imprimir("(ningún dispositivo: «bluetooth buscar»)\n"); return; }
    for (int i = 0; i < nd; i++) {
        char m[20], e[80];
        mac(m, dd[i].addr);
        texto_estado(&dd[i], e, sizeof e);
        imprimir("%d  %s  %-24s %-16s %s\n", i, m, dd[i].nome[0] ? dd[i].nome : "(sin nombre)", nome_tipo(dd[i].tipo), e);
    }
}

static int estado(void)
{
    atualizar();
    if (r_estado != 0 && inf.estado == BTA_SEM_ADAPTADOR) {
        imprimir("Bluetooth: sin controlador (enchufa un adaptador USB Bluetooth)\n");
        return 0;
    }
    char m[20];
    mac(m, inf.addr);
    imprimir("Bluetooth: %s  %s  %s\n", nome_adaptador(inf.estado), inf.chip, m);
    imprimir("  HCI %u, LMP %u, fabricante %u; ACL %u bytes x %u; eventos %u, ACL recibidos %u, enviados %u, errores %u\n",
             inf.hci_ver, inf.lmp_ver, inf.fabricante, inf.acl_mtu, inf.acl_num, inf.eventos, inf.acl_rx, inf.acl_tx, inf.erros_hci);
    if (inf.erro[0]) imprimir("  Aviso: %s\n", inf.erro);
    if (inf.buscando) imprimir("  Buscando dispositivos...\n");
    mostrar_lista();
    return 0;
}

static int indice(const char *a)
{
    atualizar();
    if (a[0] >= '0' && a[0] <= '9' && !acha_char(a, ':')) {
        int n = texto_para_int(a);
        return n >= 0 && n < nd ? n : -1;
    }
    for (int i = 0; i < nd; i++) {
        char m[20];
        mac(m, dd[i].addr);
        if (compara_texto(m, a) == 0) return i;   /* minúsculas, como las muestra «lista» */
    }
    return -1;
}

/* Espera a que acabe un emparejamiento o conexión, atendiendo la confirmación numérica. */
static int esperar_fin(int idx, bool so_parear)
{
    bool visto_activo = false, dicho_modo = false;
    unsigned t0 = tempo_ms();
    for (;;) {
        atualizar();
        if (idx >= nd) return 1;
        const disp_bt_t *d = &dd[idx];
        if (inf.conf_modo == BTC_COMPARAR && inf.conf_disp == idx && !dicho_modo) {
            dicho_modo = true;
            char r[16];
            imprimir("Código: %06u. ¿Coincide con el que muestra %s? (s/n) ", inf.conf_num, d->nome[0] ? d->nome : "el dispositivo");
            if (ler_linha(0, r, sizeof r) < 0) r[0] = 'n';
            bool si = r[0] == 's' || r[0] == 'S' || r[0] == 'y' || r[0] == 'Y';
            int rc = BT_ADM("confirmar el emparejamiento Bluetooth", BT_CONFIRMAR, si ? 1 : 0);
            if (rc < 0) return erro_op("confirmar", rc);
        } else if (inf.conf_modo == BTC_MOSTRAR && inf.conf_disp == idx && !dicho_modo) {
            dicho_modo = true;
            imprimir("Escribe %06u en el teclado de %s y pulsa Intro.\n", inf.conf_num, d->nome[0] ? d->nome : "el dispositivo");
        } else if (inf.conf_modo == BTC_NADA) dicho_modo = false;
        if (d->estado != BTD_LIVRE) visto_activo = true;
        if (d->estado == BTD_CONECTADO) { imprimir("Conectado: %s\n", d->nome); return 0; }
        if (d->estado == BTD_ERRO) { imprimir_em(ERROS, "bluetooth: %s\n", d->erro[0] ? d->erro : "falló la conexión"); return 1; }
        if (d->estado == BTD_LIVRE && visto_activo) {
            if (so_parear && d->pareado) { imprimir("Emparejado: %s\n", d->nome); return 0; }
            imprimir_em(ERROS, "bluetooth: la conexión se cerró\n");
            return 1;
        }
        if (!visto_activo && tempo_ms() - t0 > 5000) { imprimir_em(ERROS, "bluetooth: no empezó (¿adaptador apagado?)\n"); return 1; }
        if (tempo_ms() - t0 > 120000) { imprimir_em(ERROS, "bluetooth: tiempo agotado\n"); return 1; }
        dormir(200);
    }
}

static int buscar_terminal(void)
{
    int r = bt(BT_BUSCAR, 0, 0);
    if (r < 0) return erro_op("buscar", r);
    imprimir("Buscando (unos 8 segundos)...\n");
    unsigned t0 = tempo_ms();
    dormir(500);
    do {
        dormir(300);
        atualizar();
    } while (inf.buscando && tempo_ms() - t0 < 25000);
    dormir(1500);   /* los nombres que faltan se piden después */
    atualizar();
    mostrar_lista();
    return 0;
}

static int terminal(int argc, char **argv)
{
    const char *c = argc > 1 ? argv[1] : "estado";
    if (compara_texto(c, "estado") == 0) return estado();
    if (compara_texto(c, "lista") == 0) { atualizar(); mostrar_lista(); return 0; }
    if (compara_texto(c, "buscar") == 0) return buscar_terminal();
    if (compara_texto(c, "encender") == 0 || compara_texto(c, "apagar") == 0) {
        bool on = c[0] == 'e';
        int r = BT_ADM(on ? "encender el Bluetooth" : "apagar el Bluetooth", BT_ENERGIA, on ? 1 : 0);
        return r < 0 ? erro_op(c, r) : 0;
    }
    if (compara_texto(c, "confirmar") == 0 && argc > 2) {
        bool si = argv[2][0] == 's' || argv[2][0] == 'y' || argv[2][0] == '1';
        int r = BT_ADM("confirmar el emparejamiento Bluetooth", BT_CONFIRMAR, si ? 1 : 0);
        return r < 0 ? erro_op(c, r) : 0;
    }
    int op = compara_texto(c, "emparejar") == 0 ? BT_EMPAREJAR : compara_texto(c, "conectar") == 0 ? BT_CONECTAR :
             compara_texto(c, "desconectar") == 0 ? BT_DESCONECTAR : compara_texto(c, "olvidar") == 0 ? BT_OLVIDAR : -1;
    if (op < 0 || argc < 3) {
        imprimir_em(ERROS, "uso: bluetooth [estado | buscar | lista | emparejar|conectar|desconectar|olvidar N|MAC | confirmar si|no | encender | apagar | --ventana]\n");
        return compara_texto(c, "--ayuda") == 0 || compara_texto(c, "-h") == 0 ? 0 : 1;
    }
    int i = indice(argv[2]);
    if (i < 0) { imprimir_em(ERROS, "bluetooth: no conozco «%s» (usa «bluetooth lista»)\n", argv[2]); return 1; }
    static const char *const motivo[] = { "", "", "", "emparejar un dispositivo Bluetooth", "conectar un dispositivo Bluetooth",
                                          "desconectar un dispositivo Bluetooth", "olvidar un dispositivo Bluetooth" };
    int r = BT_ADM(motivo[op], op, i);
    if (r < 0) return erro_op(c, r);
    if (op == BT_EMPAREJAR || op == BT_CONECTAR) return esperar_fin(i, op == BT_EMPAREJAR);
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define VL 600
#define VA 440
#define COR_AVISO 0xB02020

static jan_t *j;
static botao_t b_energia = { 0, 8, 100, 26, "Apagar", false }, b_buscar = { 0, 8, 100, 26, "Buscar", false };
static botao_t b_conectar = { 12, 0, 190, 26, "Emparejar / Conectar", false },
               b_desconectar = { 210, 0, 110, 26, "Desconectar", false }, b_olvidar = { 328, 0, 90, 26, "Olvidar", false },
               b_si = { 0, 0, 90, 26, "Sí coincide", false }, b_no = { 0, 0, 90, 26, "No", false };
static char itens[8][64];
static lista_t lista = { 12, 66, VL - 24, 200, itens, 0, -1, 0, 0, 0 };
static char mensaje[120];

static void desenhar_tudo(void)
{
    int L = j->larg, A = j->alt;
    g_retangulo(j, 0, 0, L, A, COR_FUNDO);
    char t[160];
    if (inf.estado == BTA_SEM_ADAPTADOR) {
        rotulo(j, 12, 14, "Bluetooth: no hay ningún controlador. Enchufa un adaptador USB Bluetooth (por ejemplo el CSR8510).", COR_AVISO);
    } else {
        char m[20];
        mac(m, inf.addr);
        formatar_texto(t, sizeof t, "Bluetooth: %s", nome_adaptador(inf.estado));
        rotulo(j, 12, 12, t, inf.estado == BTA_FALHOU ? COR_AVISO : COR_TEXTO);
        formatar_texto(t, sizeof t, "%s  %s", inf.chip, m);
        rotulo(j, 12, 34, t, COR_TEXTO_FRACO);
        b_energia.x = L - 12 - 100;
        b_buscar.x = L - 12 - 100 - 8 - 100;
        b_energia.texto = inf.estado == BTA_APAGADO ? "Encender" : "Apagar";
        b_buscar.texto = inf.buscando ? "Buscando..." : "Buscar";
        botao_desenhar(j, &b_energia);
        botao_desenhar(j, &b_buscar);
    }
    lista.l = L - 24;
    lista.a = A - 66 - 128;
    lista.qtd = nd;
    for (int i = 0; i < nd; i++) {
        char e[64];
        texto_estado(&dd[i], e, sizeof e);
        formatar_texto(itens[i], sizeof itens[i], "%-22s %-13s %s", dd[i].nome[0] ? dd[i].nome : "(sin nombre)", nome_tipo(dd[i].tipo), e);
    }
    if (lista.sel >= nd) lista.sel = nd - 1;
    lista_desenhar(j, &lista);
    if (!nd && inf.estado != BTA_SEM_ADAPTADOR) rotulo(j, 24, 80, "Pulsa «Buscar» con el dispositivo en modo de emparejamiento.", COR_TEXTO_FRACO);
    int y = A - 120;
    b_conectar.y = b_desconectar.y = b_olvidar.y = y;
    botao_desenhar(j, &b_conectar);
    botao_desenhar(j, &b_desconectar);
    botao_desenhar(j, &b_olvidar);
    y += 36;
    if (inf.conf_modo == BTC_COMPARAR) {
        formatar_texto(t, sizeof t, "Código de emparejamiento: %06u. ¿Es el mismo que muestra el dispositivo?", inf.conf_num);
        rotulo(j, 12, y, t, COR_DESTAQUE);
        b_si.x = L - 12 - 90 - 8 - 90; b_no.x = L - 12 - 90; b_si.y = b_no.y = y - 6;
        botao_desenhar(j, &b_si);
        botao_desenhar(j, &b_no);
    } else if (inf.conf_modo == BTC_MOSTRAR) {
        formatar_texto(t, sizeof t, "Escribe %06u en el teclado del dispositivo y pulsa Intro.", inf.conf_num);
        rotulo(j, 12, y, t, COR_DESTAQUE);
    } else if (lista.sel >= 0 && lista.sel < nd && dd[lista.sel].estado == BTD_ERRO && dd[lista.sel].erro[0]) {
        rotulo(j, 12, y, dd[lista.sel].erro, COR_AVISO);
    }
    y += 24;
    if (mensaje[0]) rotulo(j, 12, y, mensaje, COR_AVISO);
    else if (inf.erro[0]) rotulo(j, 12, y, inf.erro, COR_AVISO);
    else rotulo(j, 12, y, "Los teclados y ratones Bluetooth se usan como los USB.", COR_TEXTO_FRACO);
    jan_mostrar(j);
}

static void accion(int op, const char *motivo)
{
    mensaje[0] = 0;
    if (lista.sel < 0 || lista.sel >= nd) { copia_texto(mensaje, "Elige primero un dispositivo de la lista.", sizeof mensaje); return; }
    int r = BT_ADM(motivo, op, lista.sel);
    if (r == ERRO_ACESSO) copia_texto(mensaje, "Hacen falta permisos de administrador.", sizeof mensaje);
    else if (r == ERRO_OCUPADO) copia_texto(mensaje, "El adaptador no está listo o está apagado.", sizeof mensaje);
    else if (r < 0) formatar_texto(mensaje, sizeof mensaje, "No se pudo (%d).", r);
}

static int janela(void)
{
    j = jan_abrir("Bluetooth", VL, VA);
    if (!j) { imprimir("bluetooth: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    jan_estilo(j, JAN_REDIMENSIONAVEL, 520, 340);
    atualizar();
    desenhar_tudo();
    evento_t e;
    for (;;) {
        if (esperar_evento(j->id, &e, 400) != 1) { atualizar(); desenhar_tudo(); continue; }
        if (e.tipo == EV_FECHAR) break;
        if (jan_redimensionar(j, &e)) { atualizar(); desenhar_tudo(); continue; }
        if (e.tipo == EV_CLIQUE || e.tipo == EV_TECLA || e.tipo == EV_RODA) {
            if (lista_tratar(j, &lista, &e) != LISTA_NADA) { mensaje[0] = 0; desenhar_tudo(); continue; }
            if (botao_tratar(j, &b_buscar, &e)) {
                mensaje[0] = 0;
                int r = bt(BT_BUSCAR, 0, 0);
                if (r == ERRO_OCUPADO) copia_texto(mensaje, "El adaptador no está listo o está apagado.", sizeof mensaje);
                else if (r < 0) copia_texto(mensaje, "No hay controlador Bluetooth.", sizeof mensaje);
            } else if (botao_tratar(j, &b_energia, &e)) {
                mensaje[0] = 0;
                bool encender = inf.estado == BTA_APAGADO;
                int r = BT_ADM(encender ? "encender el Bluetooth" : "apagar el Bluetooth", BT_ENERGIA, encender ? 1 : 0);
                if (r == ERRO_ACESSO) copia_texto(mensaje, "Hacen falta permisos de administrador.", sizeof mensaje);
            } else if (botao_tratar(j, &b_conectar, &e)) accion(BT_CONECTAR, "emparejar y conectar un dispositivo Bluetooth");
            else if (botao_tratar(j, &b_desconectar, &e)) accion(BT_DESCONECTAR, "desconectar un dispositivo Bluetooth");
            else if (botao_tratar(j, &b_olvidar, &e)) {
                if (lista.sel >= 0 && lista.sel < nd && confirmar("Bluetooth", "¿Olvidar este dispositivo? Habrá que emparejarlo otra vez."))
                    accion(BT_OLVIDAR, "olvidar un dispositivo Bluetooth");
            } else if (inf.conf_modo == BTC_COMPARAR) {
                bool si = botao_tratar(j, &b_si, &e), no = !si && botao_tratar(j, &b_no, &e);
                if (si || no) {
                    int r = BT_ADM("confirmar el emparejamiento Bluetooth", BT_CONFIRMAR, si ? 1 : 0);
                    if (r < 0) copia_texto(mensaje, "No se pudo confirmar.", sizeof mensaje);
                }
            }
            atualizar();
            desenhar_tudo();
        }
    }
    elevacao_soltar();
    jan_fechar(j);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--ventana") == 0 || compara_texto(argv[1], "-v") == 0)) return janela();
    return terminal(argc, argv);
}
