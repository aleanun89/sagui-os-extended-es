// SPDX-License-Identifier: GPL-3.0-only
/* janela.h - la interfaz de janela.c (C) para el C++ de glmark2 (D481). */
#ifndef GLMARK2_JANELA_H_
#define GLMARK2_JANELA_H_
#ifdef __cplusplus
extern "C" {
#endif
void *gm_jan_abrir(const char *titulo, int largura, int altura);
void gm_jan_fechar(void *j);
void gm_jan_mostrar(void *j);
int gm_jan_larg(void *j);
int gm_jan_alt(void *j);
int gm_jan_completa(void *j);
int gm_jan_eventos(void *j);
unsigned gm_ms(void);
#ifdef __cplusplus
}
#endif
#endif
