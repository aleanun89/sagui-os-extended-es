// SPDX-License-Identifier: GPL-3.0-only
/* tunel.h - la interfaz del túnel wg0 (kernel/rede/tunel.c, D363). */
#ifndef SAGUI_TUNEL_H
#define SAGUI_TUNEL_H
#include "tipos.h"
#include "abi.h"

/* Para la pila (rede.c) */
bool     tunel_ativo(void);
uint32_t tunel_ip(void);          /* 0 = sin túnel */
uint32_t tunel_dns(void);         /* 0 = sin DNS propio */
uint32_t tunel_mtu(void);
int      tunel_casamento(uint32_t destino);   /* bits de la ruta más larga del túnel; -1 = ninguna */
bool     tunel_enviar(const uint8_t *pacote, uint32_t n);
void     tunel_processar(void);   /* la tarea de la red mete lo que entró por el túnel */
void     tunel_descarte(uint32_t origem, uint32_t destino);   /* ip_chegou rechazó lo que escribió el demonio */
void     tunel_info_rede(info_rede_t *v);
int32_t  tunel_info(uint32_t ptr, uint32_t n);   /* INFO_WG */
bool     tunel_bandeja(bool *conectado, char *nome, uint32_t max);
int32_t  tunel_pedir_desligar(void);             /* REDE_TUNEL_DESLIGAR */

/* Para el socket del demonio (socket.c) */
int32_t  tunel_abrir(const void *dono);
void     tunel_fechar(const void *dono);
bool     tunel_dono(const void *s);
int32_t  tunel_disponivel(const void *dono);
int32_t  tunel_ler(const void *dono, char *buf, uint32_t n);
int32_t  tunel_escrever(const void *dono, const char *buf, uint32_t n);
int32_t  tunel_configurar(const void *dono, uint32_t ptr);
int32_t  tunel_publicar(const void *dono, uint32_t ptr);
const void *tunel_canal(void);

#endif
