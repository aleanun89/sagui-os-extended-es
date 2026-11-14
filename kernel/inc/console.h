// SPDX-License-Identifier: GPL-3.0-only
/* console.h - salida de texto del kernel (pantalla + espejo en el serial). */
#ifndef SAGUI_CONSOLE_H
#define SAGUI_CONSOLE_H
#include "tipos.h"

/* Colores del modo texto VGA (4 bits). */
enum {
    COR_PRETO, COR_AZUL, COR_VERDE, COR_CIANO, COR_VERMELHO, COR_MAGENTA, COR_MARROM, COR_CINZA,
    COR_CINZA_ESCURO, COR_AZUL_CLARO, COR_VERDE_CLARO, COR_CIANO_CLARO, COR_VERMELHO_CLARO,
    COR_MAGENTA_CLARO, COR_AMARELO, COR_BRANCO
};

void console_iniciar(void);     /* modo texto, al arrancar */
bool console_grafico(void);     /* intenta pasar al terminal gráfico (fase 4) */
bool console_em_grafico(void);
void console_tela_mudou(void);  /* después de cambiar el modo de vídeo (D059) */
void console_limpar(void);
void console_cor(uint8_t frente, uint8_t fundo);
void console_char(char c);
void console_mostrar(void);
void console_esconder(bool sim);
int  console_colunas(void);
int  console_coluna(void);   /* columna del cursor */

void serial_iniciar(void);
void serial_char(char c);
void serial_texto(const char *s);

/* kprintf escribe en la pantalla y en el serial; klog solo en el serial (log de pruebas). */
void kprintf(const char *fmt, ...);
void klog(const char *fmt, ...);
uint32_t console_log_copiar(char *dst, uint32_t max);   /* fin del log del kernel (INFO_LOG, D076) */
int  formatar(char *dst, size_t max, const char *fmt, va_list ap);
int  snformatar(char *dst, size_t max, const char *fmt, ...);

#endif
