// SPDX-License-Identifier: GPL-3.0-only
/* chaveiro.h - el llavero de la cuenta: secretos cifrados en ~/.config/llavero (D485).
 *
 * El fichero (0600) lleva una cabecera en claro y el contenido (pares servicio=valor) cifrado con AES-256-GCM.
 * La clave del fichero es HMAC-SHA256(K, sal del fichero) y K = PBKDF2-HMAC-SHA256(contraseña, "SaguiOS-llavero:"
 * + nombre de la cuenta, 20 000). K no se guarda en disco: la deja en el kernel (SIS_CHAVEIRO) el gestor de
 * servicios al comprobar la contraseña al entrar, o la propia cuenta al escribir la contraseña del llavero.
 * Sin K en el kernel se prueba la de la contraseña vacía (cuenta sin contraseña: «protege poco»).
 *
 * chaveiro_derivar es solo cabecera (la usa también el gestor de servicios, que no enlaza la libtls). El resto
 * está en userland/libtls/chaveiro.c. */
#ifndef SAGUI_CHAVEIRO_H
#define SAGUI_CHAVEIRO_H
#include "sagui.h"
#include "sha256.h"

#define CHAVEIRO_ARQ        "/home/.config/llavero"   /* /home es la carpeta de la cuenta (D340) */
#define CHAVEIRO_ITER       20000
#define CHAVEIRO_SERVICO_MAX 64
#define CHAVEIRO_VALOR_MAX   256
#define CHAVEIRO_ENTRADAS    32

/* Cómo está cifrado el llavero. */
#define CHAV_MODO_VAZIA   0   /* con la contraseña vacía: protege poco */
#define CHAV_MODO_CONTA   1   /* con la contraseña de la cuenta */
#define CHAV_MODO_PROPIA  2   /* con una contraseña propia del llavero */

/* Resultado de abrir. */
#define CHAV_ABERTO   0       /* abierto (o no existía: vacío y listo para guardar) */
#define CHAV_FECHADO  -1      /* existe pero ninguna clave lo abre: hace falta la contraseña */
#define CHAV_DANADO   -2      /* el fichero no se entiende */

/* K a partir de una contraseña y el nombre de la cuenta (PBKDF2-HMAC-SHA256, 32 bytes). */
static inline void chaveiro_derivar(const char *senha, const char *nome, unsigned char k[32])
{
    /* PBKDF2 con un solo bloque de salida, como en pbkdf2.h (que no se incluye aquí: sus funciones son static). */
    unsigned char kb[SHA256_BLOCO], ip[SHA256_BLOCO], op[SHA256_BLOCO], u[SHA256_TAM], meio[SHA256_TAM];
    unsigned ns = (unsigned)tam_texto(senha);
    memset(kb, 0, sizeof kb);
    if (ns > SHA256_BLOCO) sha256(senha, ns, kb);
    else memcpy(kb, senha, ns);
    for (int i = 0; i < SHA256_BLOCO; i++) { ip[i] = (unsigned char)(kb[i] ^ 0x36); op[i] = (unsigned char)(kb[i] ^ 0x5C); }
    sha256_t hi, ho, c;
    sha256_iniciar(&hi); sha256_somar(&hi, ip, SHA256_BLOCO);
    sha256_iniciar(&ho); sha256_somar(&ho, op, SHA256_BLOCO);
    char sal[96];
    formatar_texto(sal, sizeof sal, "SaguiOS-llavero:%s", nome);
    static const unsigned char um[4] = { 0, 0, 0, 1 };
    c = hi; sha256_somar(&c, sal, (unsigned)tam_texto(sal)); sha256_somar(&c, um, 4); sha256_fim(&c, meio);
    c = ho; sha256_somar(&c, meio, SHA256_TAM); sha256_fim(&c, u);
    memcpy(k, u, SHA256_TAM);
    for (unsigned it = 1; it < CHAVEIRO_ITER; it++) {
        c = hi; sha256_somar(&c, u, SHA256_TAM); sha256_fim(&c, meio);
        c = ho; sha256_somar(&c, meio, SHA256_TAM); sha256_fim(&c, u);
        for (int i = 0; i < SHA256_TAM; i++) k[i] ^= u[i];
    }
    volatile unsigned char *z;
    z = kb; for (unsigned i = 0; i < sizeof kb; i++) z[i] = 0;
    z = ip; for (unsigned i = 0; i < sizeof ip; i++) z[i] = 0;
    z = op; for (unsigned i = 0; i < sizeof op; i++) z[i] = 0;
    z = u;  for (unsigned i = 0; i < sizeof u; i++) z[i] = 0;
    memset(&hi, 0, sizeof hi); memset(&ho, 0, sizeof ho); memset(&c, 0, sizeof c);
}

typedef struct {
    char servico[CHAVEIRO_SERVICO_MAX];
    char valor[CHAVEIRO_VALOR_MAX];
} chaveiro_entrada_t;

typedef struct {
    int  modo;                    /* CHAV_MODO_* */
    bool existia;                 /* había fichero */
    unsigned char k[32];          /* la K con que se abrió (o se guardará) */
    int  n;
    chaveiro_entrada_t e[CHAVEIRO_ENTRADAS];
} chaveiro_t;

/* Abre el llavero de la cuenta con la K del kernel o, sin ella, la de la contraseña vacía. */
int  chaveiro_abrir(chaveiro_t *c);
/* Abre con la K de "senha" (la de la cuenta de antes de cambiarla, o la propia del llavero) y la deja en el
 * kernel para el resto de la sesión. */
int  chaveiro_abrir_com(chaveiro_t *c, const char *senha);
const char *chaveiro_ler(const chaveiro_t *c, const char *servico);   /* NULL si no está */
int  chaveiro_por(chaveiro_t *c, const char *servico, const char *valor);   /* y guarda */
int  chaveiro_tirar(chaveiro_t *c, const char *servico);                    /* y guarda */
int  chaveiro_guardar(chaveiro_t *c);
/* Cambia a una contraseña propia del llavero ("" = volver a la de la cuenta si la hay, o a la vacía). */
int  chaveiro_mudar_senha(chaveiro_t *c, const char *nova);
void chaveiro_fechar(chaveiro_t *c);   /* borra la memoria */
const char *chaveiro_modo_texto(int modo);
bool chaveiro_tem_chave_sessao(void);   /* ¿dejó el gestor de servicios (o la cuenta) una K en el kernel? */

#endif
