// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_unidad.c - pruebas de la pila Bluetooth en el PC (D279): la MISMA pila (kernel/bluetooth/) contra el
 * controlador HCI simulado y tres dispositivos remotos (tests/bt_sim.c). Se compila con ASan y UBSan.
 * Sale con 0 si todo bien. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt_sim.h"

static int fallos, pruebas;
#define CHECA(c, ...) do { pruebas++; if (!(c)) { fallos++; printf("FALLA (l.%d): ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void arrancar(const char *conf)
{
    sim_reiniciar();
    bt_g_iniciar();
    if (conf) bt_g_carregar(conf, (int)strlen(conf));
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
}

/* Reinicia la pila y el controlador conservando los remotos (y sus claves) y la configuración guardada. */
static void reiniciar_conservando(void)
{
    sim_remoto_t copia[SIM_MAX_REMOTOS];
    int n = sim_n_remotos;
    memcpy(copia, sim_remotos, sizeof copia);
    char conf[4096];
    strcpy(conf, sim_conf);
    sim_reiniciar();
    memcpy(sim_remotos, copia, sizeof copia);
    sim_n_remotos = n;
    for (int i = 0; i < n; i++) { sim_remotos[i].ligado = false; sim_remotos[i].ncid = 0; }
    bt_g_iniciar();
    bt_g_carregar(conf, (int)strlen(conf));
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
}

static bt_info_t info(void) { bt_info_t i; bt_g_info(&i); return i; }

static int indice_de(sim_remoto_t *r)
{
    for (int i = 0; i < bt_n_disp(); i++) if (!memcmp(bt_disp(i)->addr, r->addr, 6)) return i;
    return -1;
}

static const bt_disp_t nulo = { .estado = 255 };
static const bt_disp_t *de(sim_remoto_t *r) { int i = indice_de(r); return i < 0 ? &nulo : bt_disp(i); }

static const char *erro_de(sim_remoto_t *r) { return de(r)->erro; }

static int estado_de(sim_remoto_t *r) { return indice_de(r) < 0 ? -1 : de(r)->estado; }

static void buscar(void)
{
    CHECA(bt_buscar() == 0, "bt_buscar");
    sim_avanzar(100);
}


/* ------------------------------------------------------------------ pruebas */

static void t_inicio(void)
{
    arrancar(NULL);
    bt_info_t i = info();
    CHECA(i.estado == BT_PRONTO, "adaptador listo (%u)", i.estado);
    CHECA(i.addr[5] == 0x0A && i.addr[0] == 0x01, "BD_ADDR leida");
    CHECA(i.acl_mtu == 310 && i.acl_num == 8, "tamano de buferes ACL (%u x %u)", i.acl_mtu, i.acl_num);
    CHECA(i.hci_ver == 6 && i.lmp_ver == 6 && i.fabricante == 10, "version HCI/LMP y fabricante");
    CHECA(sim_cmds >= 10, "ordenes de inicio (%d)", sim_cmds);

    sim_reiniciar(); sim_cfg.falla_op = OP_RD_BUFFER; sim_cfg.falla_estado = 0x01;
    bt_g_iniciar(); bt_hci_iniciar(&sim_plat); sim_bombear();
    CHECA(info().estado == BT_FALHOU, "Read_Buffer_Size rechazada -> fallo");

    sim_reiniciar(); sim_cfg.falla_op = OP_WR_SSP; sim_cfg.falla_estado = 0x11;
    bt_g_iniciar(); bt_hci_iniciar(&sim_plat); sim_bombear();
    CHECA(info().estado == BT_PRONTO, "sin SSP el inicio sigue");

    sim_reiniciar(); sim_cfg.mudo = true;
    bt_g_iniciar(); bt_hci_iniciar(&sim_plat); sim_bombear();
    CHECA(info().estado == BT_INICIANDO, "controlador mudo: iniciando");
    sim_avanzar(6000);
    CHECA(info().estado == BT_FALHOU, "controlador mudo: fallo por tiempo (%u)", info().estado);

    sim_reiniciar(); sim_cfg.acl_mtu = 0;
    bt_g_iniciar(); bt_hci_iniciar(&sim_plat); sim_bombear();
    CHECA(info().estado == BT_FALHOU, "buffer ACL imposible -> fallo");
}

static void t_busqueda(void)
{
    for (int fmt = 0; fmt < 2; fmt++) {
        arrancar(NULL);
        sim_cfg.inq_formato = fmt;
        buscar();
        CHECA(bt_n_disp() == 3, "tres dispositivos vistos (%d)", bt_n_disp());
        CHECA(!info().buscando, "la busqueda acabo");
        const bt_disp_t *k = de(sim_teclado()), *m = de(sim_raton()), *c = de(sim_combo());
        CHECA(k && k->tipo == BT_T_TECLADO && !strcmp(k->nome, "Teclado BT"), "teclado con nombre de la EIR");
        CHECA(m && m->tipo == BT_T_RATO, "raton por la clase");
        CHECA(m && !strcmp(m->nome, "Raton BT"), "nombre del raton por Remote_Name_Request (%s)", m ? m->nome : "?");
        CHECA(c && c->tipo == BT_T_COMBO, "combo");
        CHECA(m && (fmt == 0 ? m->rssi == -60 : m->rssi == 0), "RSSI (%d)", m ? m->rssi : 0);
        CHECA(k && k->visto, "marcado como visto");
    }
    arrancar(NULL);
    sim_raton()->presente = false;
    buscar();
    CHECA(bt_n_disp() == 2, "un dispositivo ausente no sale");
    CHECA(bt_energia(false) == 0 && bt_buscar() < 0, "apagado no busca");
}

static void t_teclado_passkey(void)
{
    arrancar(NULL);
    buscar();
    sim_remoto_t *k = sim_teclado();
    int i = indice_de(k);
    CHECA(bt_conectar(i, false) == 0, "conectar");
    sim_avanzar(100);
    bt_info_t inf = info();
    CHECA(estado_de(k) == BT_D_AUTENTICANDO, "esperando al passkey (%d)", estado_de(k));
    CHECA(inf.conf_modo == BT_CONF_MOSTRAR && inf.conf_num == 123456, "passkey para escribir en el teclado (%u/%u)", inf.conf_modo, inf.conf_num);
    CHECA(bt_conectar(indice_de(sim_raton()), false) < 0, "un solo emparejamiento a la vez");
    sim_passkey_tecleada(k);
    sim_avanzar(100);
    CHECA(estado_de(k) == BT_D_CONECTADO, "teclado conectado (%d, %s)", estado_de(k), erro_de(k));
    CHECA(info().conf_modo == BT_CONF_NADA, "sin confirmacion pendiente");
    CHECA(k->protocolo_boot && k->n_set_protocol == 1, "SET_PROTOCOL(boot) enviado");
    CHECA(sim_hid_conectar_n == 1, "hid_conectar llamado");
    CHECA(sim_guardados >= 1 && strstr(sim_conf, "dispositivo 11:22:33:44:55:01 ") != NULL, "clave guardada:\n%s", sim_conf);
    char hex[40];
    bt_hex(hex, k->chave, 16);
    CHECA(strstr(sim_conf, hex) != NULL, "la clave del archivo es la del dispositivo");
    uint8_t inf1[8] = { 0, 0, 0x04, 0, 0, 0, 0, 0 };
    sim_teclear(k, inf1, 8);
    CHECA(sim_n_teclado == 1 && sim_rep_teclado[0][2] == 0x04, "tecla A recibida");
    uint8_t inf2[8] = { 2, 0, 0x04, 0x05, 0, 0, 0, 0 };
    sim_teclear(k, inf2, 8);
    CHECA(sim_n_teclado == 2 && sim_rep_teclado[1][0] == 2 && sim_rep_teclado[1][3] == 5, "Mayus + A + B");
    CHECA(de(k) && de(k)->tipo == BT_T_TECLADO && de(k)->hid_ok && de(k)->psm_ctl == 0x11 && de(k)->psm_int == 0x13, "SDP: PSM y tipo");
}

static void t_raton_justworks(void)
{
    for (int normal = 0; normal < 2; normal++) {
        arrancar(NULL);
        sim_hid_devuelve = normal;
        buscar();
        sim_remoto_t *m = sim_raton();
        CHECA(bt_conectar(indice_de(m), false) == 0, "conectar");
        sim_avanzar(200);
        CHECA(estado_de(m) == BT_D_CONECTADO, "raton conectado sin intervencion (%d: %s)", estado_de(m), erro_de(m));
        CHECA(sim_hid_desc_n == 52, "descriptor de informe del SDP entregado (%d)", sim_hid_desc_n);
        CHECA(m->n_set_protocol == (normal ? 0 : 1), "protocolo %s (%d ordenes)", normal ? "normal" : "boot", m->n_set_protocol);
        uint8_t rep[4] = { 1, 5, 0xFB, 0 };
        sim_teclear(m, rep, 4);
        CHECA(sim_n_raton == 1 && sim_rep_raton_boot[0] == !normal && sim_rep_raton_n[0] == 4 && sim_rep_raton[0][1] == 5, "informe de raton");
    }
}

static void t_combo_numerica(void)
{
    arrancar(NULL);
    buscar();
    sim_remoto_t *c = sim_combo();
    CHECA(bt_conectar(indice_de(c), false) == 0, "conectar combo");
    sim_avanzar(100);
    bt_info_t i = info();
    CHECA(i.conf_modo == BT_CONF_COMPARAR && i.conf_num == 123456 && i.conf_disp == indice_de(c), "confirmacion numerica");
    CHECA(bt_confirmar(true) == 0, "confirmar");
    sim_avanzar(200);
    CHECA(estado_de(c) == BT_D_CONECTADO, "combo conectado por SDP con continuacion (%d: %s)", estado_de(c), erro_de(c));
    CHECA(de(c)->tipo == BT_T_COMBO && de(c)->subclasse_hid == 0xC0, "subclase 0xC0 -> combo");
    CHECA(bt_confirmar(true) < 0, "sin confirmacion que dar");

    /* rechazar el numero */
    arrancar(NULL);
    buscar();
    c = sim_combo();
    bt_conectar(indice_de(c), false);
    sim_avanzar(100);
    CHECA(bt_confirmar(false) == 0, "rechazar");
    sim_avanzar(200);
    CHECA(estado_de(c) == BT_D_ERRO && erro_de(c)[0], "rechazo -> error (%s)", erro_de(c));
    CHECA(!de(c)->tem_chave && !c->ligado, "sin clave y sin enlace");
    CHECA(bt_conectar(indice_de(c), false) == 0, "se puede reintentar tras el error");
    sim_avanzar(100);
    bt_confirmar(true);
    sim_avanzar(200);
    CHECA(estado_de(c) == BT_D_CONECTADO, "reintento correcto");

    /* sin responder a la confirmacion: tiempo */
    arrancar(NULL);
    buscar();
    c = sim_combo();
    bt_conectar(indice_de(c), false);
    sim_avanzar(100);
    sim_avanzar(45000);
    CHECA(estado_de(c) == BT_D_ERRO, "sin contestar: error por tiempo (%d)", estado_de(c));
}

static void t_solo_parear(void)
{
    arrancar(NULL);
    buscar();
    sim_remoto_t *m = sim_raton();
    CHECA(bt_conectar(indice_de(m), true) == 0, "solo emparejar");
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_LIVRE && de(m)->tem_chave && !m->ligado, "emparejado y desconectado (%d)", estado_de(m));
    CHECA(sim_hid_conectar_n == 0, "no se abrio el HID");
    CHECA(strstr(sim_conf, "11:22:33:44:55:02") != NULL, "clave guardada");
}

static void t_persistencia_y_reconexion(void)
{
    arrancar(NULL);
    buscar();
    sim_remoto_t *k = sim_teclado();
    bt_conectar(indice_de(k), false);
    sim_avanzar(100);
    sim_passkey_tecleada(k);
    sim_avanzar(100);
    CHECA(estado_de(k) == BT_D_CONECTADO, "emparejado");
    reiniciar_conservando();
    CHECA(bt_n_disp() == 1 && de(k) && de(k)->tem_chave && !strcmp(de(k)->nome, "Teclado BT") && de(k)->tipo == BT_T_TECLADO,
          "el dispositivo vuelve del archivo con clave, nombre y tipo");
    int pk_antes = sim_guardados;
    CHECA(bt_conectar(indice_de(k), false) == 0, "conectar con clave guardada");
    sim_avanzar(200);
    CHECA(estado_de(k) == BT_D_CONECTADO, "conectado con la clave guardada, sin emparejar (%d: %s)", estado_de(k), erro_de(k));
    CHECA(info().conf_modo == BT_CONF_NADA && k->passkey_pendiente == 0, "sin pedir passkey");
    (void)pk_antes;
    CHECA(bt_desconectar(indice_de(k)) == 0, "desconectar");
    sim_avanzar(100);
    CHECA(estado_de(k) == BT_D_LIVRE && sim_soltar >= 1, "desconectado y teclas soltadas");
    sim_n_teclado = 0;
    /* el teclado reconecta solo */
    sim_remoto_conecta(k);
    sim_avanzar(200);
    CHECA(estado_de(k) == BT_D_CONECTADO, "reconexion iniciada por el teclado (%d: %s)", estado_de(k), erro_de(k));
    CHECA(k->protocolo_boot, "protocolo boot tambien al reconectar");
    uint8_t r[8] = { 0, 0, 0x1E, 0, 0, 0, 0, 0 };
    sim_teclear(k, r, 8);
    CHECA(sim_n_teclado == 1 && sim_rep_teclado[0][2] == 0x1E, "teclas tras reconectar");
    /* LEDs */
    int c = bt_g_por_addr(k->addr, false)->canal_int;
    CHECA(c >= 0 && bt_hid_leds(c, 2) == 0, "LEDs por el canal de interrupcion");
}

static void t_clave_vieja(void)
{
    arrancar(NULL);
    buscar();
    sim_remoto_t *k = sim_teclado();
    bt_conectar(indice_de(k), false);
    sim_avanzar(100);
    sim_passkey_tecleada(k);
    sim_avanzar(100);
    bt_desconectar(indice_de(k));
    sim_avanzar(100);
    k->tem_chave = false;   /* el teclado se reseteo y ya no la conoce */
    memset(k->chave, 0, 16);
    CHECA(bt_conectar(indice_de(k), false) == 0, "conectar");
    sim_avanzar(200);
    CHECA(estado_de(k) == BT_D_ERRO && strstr(erro_de(k), "clave rechazada"), "clave rechazada (%s)", erro_de(k));
    CHECA(!de(k)->tem_chave && !strstr(sim_conf, "\ndispositivo "), "la clave vieja se borro y el archivo se actualizo");
    CHECA(bt_conectar(indice_de(k), false) == 0, "reintentar");
    sim_avanzar(100);
    CHECA(info().conf_modo == BT_CONF_MOSTRAR, "ahora si empareja de nuevo");
    sim_passkey_tecleada(k);
    sim_avanzar(100);
    CHECA(estado_de(k) == BT_D_CONECTADO, "vuelve a conectar");
}

static void t_errores(void)
{
    sim_remoto_t *r;
    arrancar(NULL); buscar(); r = sim_raton();
    r->presente = false;   /* se apaga justo después de verlo */
    bt_conectar(indice_de(r), false);
    sim_avanzar(100);
    CHECA(estado_de(r) == BT_D_ERRO && strstr(erro_de(r), "no responde"), "no responde (%s)", erro_de(r));

    arrancar(NULL); buscar(); r = sim_raton();
    r->falha_conexao = 0x0D;
    bt_conectar(indice_de(r), false);
    sim_avanzar(100);
    CHECA(estado_de(r) == BT_D_ERRO && strstr(erro_de(r), "0x0d"), "conexion rechazada (%s)", erro_de(r));

    arrancar(NULL); buscar(); r = sim_raton();
    r->sdp_erro = true;
    bt_conectar(indice_de(r), false);
    sim_avanzar(300);
    CHECA(estado_de(r) == BT_D_CONECTADO, "SDP con error: se prueban los PSM de siempre (%d: %s)", estado_de(r), erro_de(r));

    arrancar(NULL); buscar(); r = sim_raton();
    r->sdp_sem_hid = true;
    bt_conectar(indice_de(r), false);
    sim_avanzar(300);
    CHECA(estado_de(r) == BT_D_CONECTADO, "SDP sin servicio HID: PSM de siempre (%d: %s)", estado_de(r), erro_de(r));

    arrancar(NULL); buscar(); r = sim_raton();
    r->ignora_sdp = true;
    bt_conectar(indice_de(r), false);
    sim_avanzar(200);
    CHECA(estado_de(r) == BT_D_SDP, "esperando al SDP");
    sim_avanzar(16000);
    CHECA(estado_de(r) == BT_D_ERRO && strstr(erro_de(r), "tiempo"), "SDP mudo: tiempo agotado (%s)", erro_de(r));
    CHECA(!r->ligado, "y se cierra el enlace");

    arrancar(NULL); buscar(); r = sim_raton();
    r->recusa_hid = true;
    bt_conectar(indice_de(r), false);
    sim_avanzar(300);
    CHECA(estado_de(r) == BT_D_ERRO && strstr(erro_de(r), "canal HID"), "canal HID rechazado (%s)", erro_de(r));

    /* se desconecta a medias */
    arrancar(NULL); buscar(); r = sim_raton();
    bt_conectar(indice_de(r), false);
    sim_avanzar(20);
    sim_desconectar_remoto(r, 0x08);
    sim_avanzar(100);
    CHECA(estado_de(r) == BT_D_ERRO || estado_de(r) == BT_D_CONECTADO || estado_de(r) == BT_D_LIVRE, "estado coherente tras corte (%d)", estado_de(r));
    CHECA(de(r)->manejador == 0xFFFF, "sin manejador tras el corte");

    /* conexion sin respuesta de la orden (tiempo en LIGANDO) */
    arrancar(NULL); buscar(); r = sim_raton();
    sim_cfg.mudo = true;
    bt_conectar(indice_de(r), false);
    sim_avanzar(25000);
    CHECA(estado_de(r) == BT_D_ERRO, "controlador mudo al conectar: error por tiempo (%d)", estado_de(r));
}

static void t_corte_conectado(void)
{
    arrancar(NULL); buscar();
    sim_remoto_t *m = sim_raton();
    bt_conectar(indice_de(m), false);
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "conectado");
    sim_desconectar_remoto(m, 0x08);
    sim_avanzar(100);
    CHECA(estado_de(m) == BT_D_LIVRE && sim_soltar == 1, "corte (fuera de alcance): libre y entrada soltada");
    int abiertos = 0;
    for (int i = 0; i < BT_MAX_CANAIS; i++) if (bt_l2cap_canal(i)) abiertos++;
    CHECA(abiertos == 0, "los canales L2CAP se liberaron (%d)", abiertos);
    CHECA(bt_conectar(indice_de(m), false) == 0, "se puede reconectar");
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "reconectado");
}

static void t_creditos_y_fragmentos(void)
{
    /* un solo buffer ACL y sin confirmaciones automaticas */
    arrancar(NULL);
    sim_cfg.acl_num = 1;
    sim_cfg.retener_acl = true;
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
    buscar();
    sim_remoto_t *c = sim_combo();
    bt_conectar(indice_de(c), false);
    sim_avanzar(100);
    bt_confirmar(true);
    for (int i = 0; i < 80 && estado_de(c) != BT_D_CONECTADO; i++) { sim_soltar_acl(); sim_avanzar(10); }
    CHECA(estado_de(c) == BT_D_CONECTADO, "un solo credito ACL basta para completar el flujo (%d: %s)", estado_de(c), erro_de(c));
    CHECA(sim_acl_creditos_vistos <= 1, "nunca mas de 1 ACL en vuelo (%d)", sim_acl_creditos_vistos);

    /* buferes de 27 bytes: se fragmenta lo que sale y se reensambla lo que entra */
    arrancar(NULL);
    sim_cfg.acl_mtu = 27;
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
    buscar();
    sim_remoto_t *m = sim_raton();
    bt_conectar(indice_de(m), false);
    sim_avanzar(500);
    CHECA(estado_de(m) == BT_D_CONECTADO && sim_hid_desc_n == 52, "con buferes de 27 bytes funciona (%d: %s, desc %d)", estado_de(m), erro_de(m), sim_hid_desc_n);
}

static void t_energia_olvidar(void)
{
    arrancar(NULL); buscar();
    sim_remoto_t *m = sim_raton(), *k = sim_teclado();
    bt_conectar(indice_de(m), false);
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "conectado");
    CHECA(bt_energia(false) == 0, "apagar");
    sim_avanzar(100);
    CHECA(estado_de(m) == BT_D_LIVRE && !m->ligado && sim_soltar >= 1, "apagar desconecta todo");
    CHECA(info().estado == BT_APAGADO && !info().energia, "estado apagado");
    CHECA(bt_conectar(indice_de(m), false) < 0, "apagado no conecta");
    CHECA(strstr(sim_conf, "energia 0") != NULL, "el apagado se guarda");
    CHECA(bt_energia(true) == 0, "encender");
    sim_avanzar(50);
    CHECA(info().estado == BT_PRONTO, "listo otra vez");
    CHECA(bt_conectar(indice_de(m), false) == 0, "conectar otra vez");
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "conectado de nuevo");

    int n = bt_n_disp();
    CHECA(bt_esquecer(indice_de(m)) == 0, "olvidar conectado");
    sim_avanzar(100);
    CHECA(bt_n_disp() == n - 1 && !m->ligado, "olvidado y desconectado");
    CHECA(!strstr(sim_conf, "11:22:33:44:55:02"), "ya no esta en el archivo");
    (void)k;
    CHECA(bt_esquecer(99) < 0 && bt_conectar(99, false) < 0 && bt_desconectar(99) < 0, "indices fuera de rango");
}

static void t_desconocido_y_unplug(void)
{
    arrancar(NULL);   /* sin buscar: el teclado es un desconocido */
    sim_remoto_t *k = sim_teclado();
    sim_remoto_conecta(k);
    sim_avanzar(200);
    CHECA(!k->ligado && bt_n_disp() == 0, "una conexion de un desconocido se rechaza");

    arrancar(NULL); buscar();
    k = sim_teclado();
    bt_conectar(indice_de(k), false);
    sim_avanzar(100);
    sim_passkey_tecleada(k);
    sim_avanzar(100);
    CHECA(estado_de(k) == BT_D_CONECTADO, "conectado");
    int n = bt_n_disp();
    sim_unplug(k);
    sim_avanzar(100);
    CHECA(bt_n_disp() == n - 1 && !strstr(sim_conf, "11:22:33:44:55:01"), "VIRTUAL_CABLE_UNPLUG: se olvida");
}

static void t_pin_antiguo(void)
{
    arrancar(NULL);
    sim_cfg.sin_ssp = true;
    bt_hci_iniciar(&sim_plat);
    sim_bombear();
    buscar();
    sim_remoto_t *m = sim_raton();
    bt_conectar(indice_de(m), false);
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "dispositivo anterior a SSP con PIN 0000 (%d: %s)", estado_de(m), erro_de(m));
}

static void t_configuracion(void)
{
    arrancar("# nada\nenergia 0\nbasura\ndispositivo 11:22:33:44:55:01 000102030405060708090a0b0c0d0e0f 05 1 Mi Teclado\n"
             "dispositivo XX:22:33:44:55:01 000102030405060708090a0b0c0d0e0f 05 1 mal\n"
             "dispositivo 11:22:33:44:55:09 00010203040506070809 05 1 corta\n");
    CHECA(bt_n_disp() == 1, "solo la linea valida (%d)", bt_n_disp());
    const bt_disp_t *d = bt_disp(0);
    CHECA(d && d->tem_chave && d->chave[15] == 0x0f && d->tipo_chave == 5 && d->tipo == BT_T_TECLADO && !strcmp(d->nome, "Mi Teclado"), "campos leidos");
    CHECA(!info().energia, "energia 0 leida");
    char t[2048];
    int n = bt_g_serializar(t, sizeof t);
    CHECA(n > 0 && strstr(t, "dispositivo 11:22:33:44:55:01 000102030405060708090a0b0c0d0e0f 05 1 Mi Teclado\n"), "serializa igual");
    CHECA(bt_g_serializar(t, 40) < 40, "serializar con poco sitio no se pasa");
}

static uint32_t rng = 12345;
static uint32_t azar(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void t_fuzz(void)
{
    arrancar(NULL); buscar();
    /* conexion normal en marcha mientras llegan basuras */
    sim_remoto_t *m = sim_raton();
    bt_conectar(indice_de(m), false);
    sim_avanzar(300);
    uint8_t b[300];
    for (int it = 0; it < 60000; it++) {
        int n = (int)(azar() % 40);
        for (int i = 0; i < n; i++) b[i] = (uint8_t)azar();
        if (azar() % 2 && n >= 2) { b[0] = (uint8_t)(azar() % 0x40); b[1] = (uint8_t)(n - 2); }   /* eventos con longitud buena */
        bt_hci_evento(b, n);
        if (azar() % 3 == 0) {
            if (n >= 4) {
                uint16_t hf = (uint16_t)((m->manejador) | ((azar() % 4) << 12));
                b[0] = (uint8_t)hf; b[1] = (uint8_t)(hf >> 8);
                b[2] = (uint8_t)(n - 4); b[3] = 0;
                if (n >= 8 && azar() % 2) { b[4] = (uint8_t)(n - 8); b[5] = 0; b[6] = (uint8_t)(azar() % 2 ? 1 : 0x50); b[7] = 0; }
            }
            bt_hci_acl_entrada(b, n);
        }
        if (it % 500 == 0) { sim_avanzar(30); }
    }
    /* analisis SDP con basura y con el registro bueno mutado */
    bt_sdp_hid_t r;
    for (int it = 0; it < 200000; it++) {
        int n = (int)(azar() % 200);
        for (int i = 0; i < n; i++) b[i] = (uint8_t)azar();
        if (n && azar() % 2) b[0] = 0x35;
        if (n > 2 && azar() % 2) b[1] = (uint8_t)(n - 2);
        bt_sdp_analisar(b, n, &r);
    }
    CHECA(true, "fuzz sin caidas");
    /* despues de todo, el sistema sigue siendo usable */
    arrancar(NULL); buscar();
    m = sim_raton();
    bt_conectar(indice_de(m), false);
    sim_avanzar(300);
    CHECA(estado_de(m) == BT_D_CONECTADO, "tras el fuzz se puede conectar");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    t_inicio();
    t_busqueda();
    t_teclado_passkey();
    t_raton_justworks();
    t_combo_numerica();
    t_solo_parear();
    t_persistencia_y_reconexion();
    t_clave_vieja();
    t_errores();
    t_corte_conectado();
    t_creditos_y_fragmentos();
    t_energia_olvidar();
    t_desconocido_y_unplug();
    t_pin_antiguo();
    t_configuracion();
    t_fuzz();
    printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
