// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_sim.h - controlador HCI simulado y dispositivos Bluetooth remotos para probar la pila en el PC (D279).
 * Habla HCI de verdad (órdenes, eventos y paquetes ACL con créditos) y, detrás, emula periféricos con su
 * propia L2CAP, un servidor SDP con respuesta fragmentada y HIDP: un teclado, un ratón y un combo. */
#ifndef BT_SIM_H
#define BT_SIM_H
#include <stdint.h>
#include <stdbool.h>

#define SIM_MAX_REMOTOS 6

typedef struct {
    uint8_t  addr[6];
    char     nome[32];
    uint32_t cod;
    int      io;                 /* capacidad de E/S: 0 DisplayOnly, 1 DisplayYesNo, 2 KeyboardOnly, 3 NoInputNoOutput */
    bool     tem_chave;
    uint8_t  chave[16];
    bool     presente;           /* responde a la búsqueda y a las conexiones */
    int      falha_conexao;      /* estado de Connection Complete si no es 0 */
    bool     sdp_erro;           /* contesta al SDP con una respuesta de error */
    bool     sdp_sem_hid;        /* contesta al SDP con una lista vacía */
    bool     sdp_fragmentado;    /* respuesta en dos trozos con estado de continuación */
    bool     recusa_hid;         /* rechaza los canales HID (seguridad) */
    bool     ignora_sdp;         /* no contesta al SDP */
    bool     boot_so;            /* en el SDP, subclase de teclado/ratón */
    uint8_t  subclasse;
    bool     descritor_raton;    /* el SDP trae un descriptor de informe de ratón */
    bool     inquiry_ext;        /* sale como resultado extendido (con nombre) */
    /* estado interno */
    uint16_t manejador;
    bool     ligado;
    uint16_t cid[8], hcid[8], psm[8];
    int      ncid;
    bool     config_ok[8];
    uint8_t  cont[2];
    bool     protocolo_boot;
    int      passkey_pendiente;
    int      n_set_protocol;
    uint8_t  id_sinal;
} sim_remoto_t;

typedef struct {
    int      acl_num;            /* búferes ACL del controlador */
    int      acl_mtu;
    int      falla_op;           /* opcode que responde con error (0 = ninguno) */
    int      falla_estado;
    bool     mudo;               /* no contesta a nada */
    bool     sin_ssp;
    int      inq_formato;        /* 0 = RSSI, 1 = clásico */
    bool     retener_acl;        /* no confirma los ACL hasta sim_soltar_acl() */
    bool     clon_csr;           /* como el 0a12:0001 del Amilo: BT 2.0 (HCI/LMP 3), sin EIR ni SSP; las
                                    órdenes desconocidas acaban con Command Status 0x01 y el modo de
                                    búsqueda 2 se rechaza con 0x11 */
    bool     sin_features;       /* Read_Local_Supported_Features desconocida */
    int      inq_modo;           /* último Write_Inquiry_Mode aceptado */
    int      cs_mudo_op;         /* opcode que solo recibe Command Status 0 y nunca acaba */
} sim_cfg_t;

extern sim_cfg_t sim_cfg;
extern sim_remoto_t sim_remotos[SIM_MAX_REMOTOS];
extern int sim_n_remotos;
extern uint32_t sim_ms;
extern int sim_acl_creditos_vistos;      /* mayor número de ACL en vuelo visto */
extern int sim_cmds, sim_acl_tx;

void sim_reiniciar(void);                       /* controlador y remotos por defecto */
sim_remoto_t *sim_teclado(void);
sim_remoto_t *sim_raton(void);
sim_remoto_t *sim_combo(void);
void sim_bombear(void);                         /* entrega a la pila todo lo pendiente */
void sim_avanzar(uint32_t ms);                  /* reloj + tics de la pila + bombear */
void sim_remoto_conecta(sim_remoto_t *r);       /* el dispositivo reconecta solo (Connection Request) */
void sim_teclear(sim_remoto_t *r, const uint8_t *informe, int n);   /* informe de entrada por el canal de interrupción */
void sim_desconectar_remoto(sim_remoto_t *r, uint8_t motivo);
void sim_evento_crudo(const uint8_t *p, int n); /* inyecta un evento tal cual */
void sim_acl_crudo(const uint8_t *p, int n);
void sim_passkey_tecleada(sim_remoto_t *r);     /* el usuario escribió el passkey en el teclado remoto */
void sim_unplug(sim_remoto_t *r);
void sim_soltar_acl(void);               /* el dispositivo manda VIRTUAL_CABLE_UNPLUG */

/* plataforma para la pila */
#include "../kernel/bluetooth/bt.h"
extern bt_plat_t sim_plat;
extern char sim_conf[4096];
extern int sim_conf_n, sim_guardados;
extern uint8_t sim_rep_teclado[64][8];
extern int sim_n_teclado;
extern uint8_t sim_rep_raton[64][8];
extern int sim_rep_raton_n[64];
extern bool sim_rep_raton_boot[64];
extern int sim_n_raton;
extern int sim_soltar, sim_hid_conectar_n;
extern int sim_hid_devuelve;                    /* lo que devuelve hid_conectar para un ratón */
extern int sim_hid_desc_n;
extern char sim_log[8192];
#endif
