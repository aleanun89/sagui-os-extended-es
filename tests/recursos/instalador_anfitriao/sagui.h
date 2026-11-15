// SPDX-License-Identifier: GPL-2.0-or-later
/* sagui.h de mentira para compilar en el anfitrión la libsagui de particiones (particoes.c,
 * particoes_acoes.c, particoes_tamanho.c, gpt.c, redimensao.c y formatacao.c) sobre un archivo
 * que hace de disco entero (h318, D318). Solo lo que usan esos archivos, con la libc del PC. */
#ifndef SAGUI_H
#define SAGUI_H
#define SAGUI_ANFITRIAO 1
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned int setores, inicio, trim, arranque_real, reservado_pid, gpt, tipo_mbr, cab_lba, bloco_fisico, reservado[7];
} info_bloco_t;
typedef struct {
    char nome[12], fabricante[32], produto[48], formato[40], rotulo[20];
    unsigned int setores, montado, suportado;
    char pai[12], tipo_fs[8], ponto[64];
    unsigned int particao, flags;
} info_disco_t;

#define ERRO_INVALIDO      -1
#define ERRO_NAO_EXISTE    -2
#define ERRO_JA_EXISTE     -3
#define ERRO_SEM_MEMORIA   -4
#define ERRO_LIMITE        -9
#define ERRO_FORMATO      -13
#define ERRO_ACESSO       -14
#define ERRO_OCUPADO      -15
#define ERRO_NAO_SUPORTADO -17
#define ERRO_DISCO_ARRANQUE -18
#define DBLOCO_CONF_ARRANQUE 1
#define DBLOCO_CONF_GPT      2
#define DBLOCO_CONF_MONTADO  4
#define DISCO_REMOVIVEL   1
#define DISCO_SO_LEITURA  8
#define DISCO_ARRANQUE_REAL 16
#define TRIM_NAO 0
#define TRIM_SIM 1
#define TRIM_SEM_DRIVER 2
#define ABRIR_LER       1
#define ABRIR_ESCREVER  2
#define ABRIR_CRIAR     4
#define ABRIR_TRUNCAR   8
#define MONTAR_SO_LEITURA      0x01
#define MONTAR_LEITURA_ESCRITA 0x02

static inline void *aloca(size_t n) { return malloc(n); }
static inline void *aloca_zerado(size_t n) { return calloc(1, n); }
static inline void libera(void *p) { free(p); }
static inline int formatar_lista(char *s, size_t n, const char *f, va_list ap) { return vsnprintf(s, n, f, ap); }
#define formatar_texto snprintf
static inline void copia_texto(char *d, const char *s, size_t n) { snprintf(d, n, "%s", s); }
static inline int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
static inline bool comeca_com(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }   /* D282: formatacao.c */
static inline const char *nome_erro(int r) { (void)r; return "error de E/S"; }
static inline size_t tam_texto(const char *s) { return strlen(s); }
static inline uint32_t hora_utc(void *p) { (void)p; return 1762905600u; }
void registrar(const char *fmt, ...);
int bytes_aleatorios(void *p, unsigned n);
uint32_t tempo_ms(void);
int listar_discos(int i, info_disco_t *d);
int desmontar(const char *ponto);
int montar_em(const char *vol, const char *ponto, int op);
int criar_pasta(const char *c);
int abrir(const char *c, int modo);
int fechar(int fd);
int apagar(const char *c);
int escrever_tudo(int fd, const void *buf, unsigned n);
char *ler_todo_fd(int fd, unsigned *tam);
#endif
