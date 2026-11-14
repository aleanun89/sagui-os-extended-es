/* ponte_sagui.h - la ponte entre MicroPython y libtls (D069).
 *
 * Los módulos ssl y hashlib (lado de MicroPython) no pueden incluir tls.h
 * ni cripto.h, que traen sagui.h (ver sagui_mp.h). ponte_sagui.c se
 * compila como un programa común de SaguiOS y ofrece solo lo necesario,
 * con tipos básicos de C y punteros opacos. Esta cabecera no incluye
 * nada, para servir a los dos lados. */
#ifndef PONTE_SAGUI_H
#define PONTE_SAGUI_H

/* ---- almacén de certificados raíz ---- */
/* El almacén del sistema (/etc/ssl/certs y /home/.certificados, D063),
 * cargado en la primera llamada. */
void *ponte_raizes_sistema(void);
/* Un almacén nuevo: vacío o con una copia de las raíces del sistema. */
void *ponte_raizes_novo(int com_sistema);
/* Suma los certificados de un texto PEM o de un DER; devuelve cuántos
 * entraron (0 = ningún certificado válido). */
int   ponte_raizes_somar(void *raizes, const char *dados, unsigned n);
/* Suma los de un archivo PEM; <0 = error de SaguiOS al leer el archivo. */
int   ponte_raizes_somar_arquivo(void *raizes, const char *caminho);
int   ponte_raizes_quantas(const void *raizes);
void  ponte_raizes_liberar(void *raizes);

/* ---- conexión TLS sobre un socket ya conectado ---- */
#define PONTE_VERIFICAR      1   /* validar la cadena con las raíces */
#define PONTE_CONFERIR_NOME  2   /* el certificado tiene que ser del host pedido */
/* Hace el handshake. Devuelve el objeto o NULL con el texto del error (en
 * español, Latin-1) en "erro"; *cert = 1 si el error fue del certificado. */
void *ponte_tls_abrir(int fd, const char *host, int opcoes, void *raizes, char *erro, unsigned max, int *cert);
int   ponte_tls_ler(void *t, void *buf, unsigned n);           /* bytes; 0 = fin; <0 = error */
int   ponte_tls_escrever(void *t, const void *buf, unsigned n);
int   ponte_tls_pendente(void *t);                             /* bytes ya descifrados a la espera */
void  ponte_tls_fechar(void *t);                               /* envía close_notify y libera (no cierra el fd) */
const char *ponte_tls_erro(void *t);
const char *ponte_tls_versao(void *t);
const char *ponte_tls_suite(void *t);
/* El certificado del servidor (DER) y su sujeto en texto. */
int   ponte_tls_certificado(void *t, const unsigned char **der, char *sujeito, unsigned max);

/* ---- SHA-384 y SHA-512 de libtls (para hashlib) ---- */
#define PONTE_HASH_CTX 256   /* bytes; cabe sha512_t */
void  ponte_sha512_iniciar(void *ctx, int bits);   /* 384 o 512 */
void  ponte_sha512_somar(void *ctx, const void *d, unsigned n);
void  ponte_sha512_fim(void *ctx, unsigned char *saida);

/* ---- hora (D080): UTC del kernel y hora local de la zona del proceso ----
 * c[9]: año, mes, día, hora, minuto, segundo, día de la semana (0 = lunes),
 * día del año (1..366), horario de verano (0/1). */
unsigned ponte_hora_utc(unsigned *ms);                 /* segundos UTC desde 1970; ms puede ser NULL */
void     ponte_hora_local(unsigned utc, int c[9]);
void     ponte_hora_utc_campos(unsigned utc, int c[9]);
unsigned ponte_local_para_utc(int ano, int mes, int dia, int hora, int minuto, int segundo);

/* ---- imágenes de libsagui (imagem.h, D054), para el módulo sagui (D083) ----
 * Píxeles 0x00RRGGBB, línea a línea. */
/* Lee BMP, PNM o PNG: *px se asigna con aloca (quien llama libera con
 * libera); 0 = ok, si no un error (texto en ponte_imagem_erro). */
int   ponte_imagem_ler(const char *caminho, int *larg, int *alt, unsigned **px, char *formato, unsigned max);
int   ponte_imagem_gravar(const char *caminho, int larg, int alt, const unsigned *px, int ppm);
/* imagem_desenhar: img (l_img x a_img) escalada a l x a en (ox, oy) del
 * destino, solo dentro del recorte (rx, ry, rl, ra). */
void  ponte_imagem_desenhar(unsigned *dst, int larg_dst, int rx, int ry, int rl, int ra,
                            int l_img, int a_img, const unsigned *px, int ox, int oy, int l, int a, int suave);
const char *ponte_imagem_erro(int erro);   /* en español, Latin-1 */

#endif
