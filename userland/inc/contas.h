// SPDX-License-Identifier: GPL-2.0-or-later
/* contas.h - las cuentas de usuario (D255, mejora 098, fase 1).
 *
 * Todo lo que lee o escribe las cuentas pasa por aquí (libsagui/contas.c): las
 * rutas están en abi.h (CONTAS_ARQ_*), el único sitio que hay que cambiar cuando
 * se decida dónde guarda el sistema su configuración persistente.
 *
 * /etc/usuarios, una línea por cuenta (como /etc/passwd, sin contraseña):
 *     nombre:uid:gid:carpeta:shell:descripción
 * /etc/sombra (solo root la lee: el kernel niega la ruta a los demás):
 *     nombre:hash
 * con hash = "" (sin contraseña todavía), "!" (no se puede entrar con
 * contraseña: cuentas de servicio) o "pbkdf2-sha256$..." (pbkdf2.h).
 *
 * Robustez: si /etc/usuarios falta o tiene líneas dañadas, se usan las cuentas
 * de serie (root, sagui y servicio) para lo que falte; si la sombra falta o una
 * línea no se entiende, esa cuenta queda «sin contraseña» (que solo se puede
 * fijar desde la sesión local) y nunca bloqueada. Nada de esto impide arrancar. */
#ifndef SAGUI_CONTAS_H
#define SAGUI_CONTAS_H
#include "sagui.h"

#define CONTA_NOME_MAX  32
#define CONTAS_MAX      16
#define SOMBRA_TEXTO_MAX 160

typedef struct {
    char     nome[CONTA_NOME_MAX];
    unsigned uid, gid;
    char     casa[64];
    char     shell[64];
    char     descricao[64];
} conta_t;

/* Las cuentas (las de serie añadidas si faltan). Devuelve cuántas; en "danado",
 * si no es NULL, true cuando el archivo faltaba o tenía líneas que no valen. */
int  contas_ler(conta_t *v, int max, bool *danado);
bool conta_por_uid(unsigned uid, conta_t *c);
bool conta_por_nome(const char *nome, conta_t *c);
/* El nombre de un uid ("1000" si no hay cuenta). */
const char *conta_nome(unsigned uid, char *buf, int max);

/* La sombra (solo root). sombra_ler: 1 y el texto en "hash" si la cuenta tiene
 * línea; 0 si no la tiene (= sin contraseña); ERRO_* si no se pudo leer (el
 * kernel la niega a quien no es root). */
int  sombra_ler(const char *nome, char *hash, int max);
/* Cambia (o añade) la línea de una cuenta. Escribe un temporal al lado y lo
 * renombra encima: un corte a medias deja la vieja o la nueva, nunca media. */
int  sombra_gravar(const char *nome, const char *hash);

/* La configuración del sistema (D255, partición SAGUI-CONF). "rel" es la ruta
 * dentro de /etc ("sombra", "ssh/authorized_keys", "servicios/activados"):
 * se escribe en CONF_SISTEMA_PONTO/etc/rel (si la partición está) y en /etc/rel
 * (la RAM, lo que leen los programas), cada uno con temporal + renombrar.
 * Solo root. conf_persistente: ¿hay SAGUI-CONF? (sin ella, «modo en vivo»). */
int  conf_gravar(const char *rel, const void *dados, int n);
int  conf_apagar(const char *rel);
bool conf_persistente(void);

/* Identidad del proceso (SIS_USUARIO). */
unsigned meu_uid(void);
unsigned meu_gid(void);
bool     sou_root(void);
/* Para los programas de administración: si no es root, escribe
 * «programa: hace falta ser administrador: usa «admin programa ...»» y devuelve true. */
bool     falta_admin(const char *programa);

/* Elevación sin setuid (D255): el gestor de servicios comprueba la contraseña.
 * ler_senha: pregunta sin eco (o lee una línea si la entrada no es un terminal);
 * devuelve la longitud o -1 (Ctrl+C, fin). pedir_conta: manda el pedido y espera
 * la respuesta (1 = hecho, o el error). elevar_interativo: pasa a ser uid_alvo
 * preguntando lo que haga falta (la contraseña, o elegir la primera si la cuenta
 * no tiene); 0 = ya lo es. */
int  ler_senha(const char *pergunta, char *buf, int max);
int  pedir_conta(pedido_conta_t *p);
int  elevar_interativo(unsigned uid_alvo, const char *programa);

#endif
