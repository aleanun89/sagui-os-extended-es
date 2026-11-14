// SPDX-License-Identifier: GPL-2.0-or-later
/* correo.h - protocolos de correo (SMTP, POP3, IMAP) y MIME para el cliente `correo` (082).
 *
 * Todo en userland/mail/. Sin libc, sin coma flotante: toda la aritmética es entera.
 * Las cadenas van en UTF-8 tal como llegan de la red; la conversión a Latin-1 (la
 * pantalla) la hace quien dibuja. Los búferes grandes van con aloca, nunca en la pila
 * (que es de 64 KB). */
#ifndef SAGUI_CORREO_H
#define SAGUI_CORREO_H
#include "sagui.h"
#include "tls.h"

/* ---- sesión: TCP y, si hace falta, TLS. Todos los protocolos son de líneas. ---- */
typedef struct {
    conexao_t con;          /* fd y tls (NULL = sin cifrar) */
    char      erro[200];   /* último error, en español */
    char      rec[4096];   /* lectura con búfer: sobra de la última lectura de red */
    unsigned  rec_n, rec_pos;
} sessao_t;

/* Abre la conexión. tls = 1: TLS directo (SMTPS 465, POP3S 995, IMAPS 993).
 * Devuelve 0 o un error (<0); el texto queda en s->erro. */
int  sessao_abrir(sessao_t *s, const char *host, int porta, bool tls, const tls_config_t *cfg);
/* Lee una línea (sin el CRLF) en buf. Devuelve la longitud, o <0 con error. */
int  sessao_linha(sessao_t *s, char *buf, unsigned max);
/* Escribe la línea más CRLF. */
int  sessao_escrever_linha(sessao_t *s, const char *linha);
/* Escribe n bytes en crudo (para el cuerpo y los literales). */
int  sessao_escrever(sessao_t *s, const void *buf, unsigned n);
/* Pasa a TLS sobre la conexión abierta (STARTTLS). 0 = ok. */
int  sessao_starttls(sessao_t *s, const char *host, const tls_config_t *cfg);
void sessao_fechar(sessao_t *s);

/* ---- SMTP ---- */
typedef struct {
    const char *host;
    int         porta;
    bool        tls;            /* TLS directo (465) */
    bool        starttls;       /* pedir STARTTLS en la conexión de texto (587) */
    const char *usuario;        /* NULL = sin AUTH */
    const char *clave;
    const char *de;             /* MAIL FROM */
    const char *para;           /* RCPT TO (uno solo, por sencillez) */
    const char *mensaje;        /* mensaje ya en MIME, con sus cabeceras; el DATA lo punta */
    unsigned    nmensaje;
    const tls_config_t *cfg;
} smtp_envio_t;
/* 0 = aceptado; <0 = error (texto en erro). */
int smtp_enviar(const smtp_envio_t *e, char *erro, unsigned max);

/* ---- POP3 y IMAP: descarga de todos los mensajes en una sola sesión ----
 * Por cada mensaje llama a f(ctx, i, dados, n) con i desde 1; "dados" es el mensaje
 * entero (cabeceras y cuerpo) y solo vale durante la llamada. Así no hace falta tener
 * en memoria todo el buzón. f devuelve 0 para seguir o <0 para parar (error del
 * llamante). Devuelve cuántos mensajes se entregaron, o <0 con error (en erro). */
typedef struct {
    const char *host;
    int         porta;
    bool        tls;            /* TLS directo (995 / 993) */
    const char *usuario;
    const char *clave;
    const tls_config_t *cfg;
} buzon_cfg_t;
typedef int (*mensagem_fn)(void *ctx, int i, const char *dados, unsigned n);
int pop3_receber(const buzon_cfg_t *b, mensagem_fn f, void *ctx, char *erro, unsigned max);
int imap_receber(const buzon_cfg_t *b, mensagem_fn f, void *ctx, char *erro, unsigned max);

/* ---- MIME (mime.c) ---- */
/* Base64 (RFC 4648, alfabeto estándar, relleno "="). Devuelve el tamaño escrito en dst. */
unsigned base64_codificar(const uint8_t *src, unsigned n, char *dst, unsigned max);
/* Descodifica; ignora espacios y CRLF. Devuelve bytes escritos o <0 si hay un carácter inválido. */
int      base64_descodificar(const char *src, unsigned n, uint8_t *dst, unsigned max);
/* Quoted-printable (RFC 2045): descodifica en dst; ignora los "=" de fin de línea. */
int      quoted_printable_descodificar(const char *src, unsigned n, uint8_t *dst, unsigned max);
/* HTML a texto plano básico: etiquetas de bloque -> saltos, entidades comunes, sin <script>/<style>. */
unsigned html_para_texto(const char *html, unsigned n, char *dst, unsigned max);

/* Mensaje analizado. Los punteros apuntan dentro del mensaje original (no son copias)
 * salvo "texto" y "html", que son búferes propios (aloca) ya descodificados. */
#define MIME_MAX_ADJUNTOS 16
typedef struct {
    char     de[256], para[256], assunto[256], data[64];
    char    *texto;          /* text/plain descodificado (aloca), o NULL */
    char    *html;           /* text/html descodificado (aloca), o NULL */
    int      n_anexos;
    struct {
        char     nome[128];
        char     tipo[64];
        unsigned tamanho;   /* bytes descodificados */
    } anexos[MIME_MAX_ADJUNTOS];
} mensagem_t;

/* Analiza el mensaje bruto. Rellena m; libera con mensagem_analisar_liberar. */
int  mensagem_analisar(const char *bruta, unsigned n, mensagem_t *m);
void mensagem_analisar_liberar(mensagem_t *m);

/* Construcción de un mensaje para enviar. */
typedef struct {
    const char *de, *para, *assunto;
    const char *texto;             /* cuerpo en texto plano (UTF-8 o Latin-1 limpio) */
    int         n_anexos;
    struct { const char *nome; const uint8_t *dados; unsigned n; } anexos[MIME_MAX_ADJUNTOS];
} mensagem_envio_t;
/* Escribe el mensaje MIME en un búfer aloca'd. Devuelve tamaño; *out (liberar con libera). */
int  mensagem_construir(const mensagem_envio_t *e, char **out, unsigned *n);

#endif
