// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_ferramentas.h - la plantilla de chat de LFM3 con herramientas e imágenes y la lectura
 * de las llamadas a herramientas que escribe el modelo (D380, fase 2).
 *
 * El servidor (llm_servidor.c) recibe una petición de la API de OpenAI («messages», «tools»,
 * «tool_choice») y la convierte en el texto que el modelo espera, igual que la plantilla
 * jinja del GGUF (tokenizer.chat_template):
 *
 *   <|startoftext|><|im_start|>system\n SISTEMA\n\n<tools>\n{"name": ..., ...}\n</tools><|im_end|>\n
 *   <|im_start|>user\n ... <|im_end|>\n
 *   <|im_start|>assistant\n<think></think>TEXTO<tool_call>\n<function=NOMBRE>\n
 *       <parameter=CLAVE>\nVALOR\n</parameter>\n</function>\n</tool_call><|im_end|>\n
 *   <|im_start|>tool\n<tool_response>\nRESULTADO\n</tool_response><|im_end|>\n
 *   <|im_start|>assistant\n<think></think>
 *
 * y, al revés, lee los <tool_call> que genera el modelo y los devuelve como «tool_calls» con
 * los argumentos en JSON (los tipos, según el esquema de cada herramienta).
 * Todo el texto va en UTF-8. */
#ifndef LLM_FERRAMENTAS_H
#define LLM_FERRAMENTAS_H
#include "llm.h"
#ifdef LLM_ANFITRIAO
#define AG_ANFITRION
#endif
#include "../ia/json.h"

/* Un búfer de texto que crece. */
typedef struct { char *p; int n, cap; bool sem_memoria; } llm_buf_t;
void llm_buf_add(llm_buf_t *b, const char *s, int n);
void llm_buf_txt(llm_buf_t *b, const char *s);
void llm_buf_libera(llm_buf_t *b);

/* Valor JSON reescrito como json.dumps de Python (separadores ", " y ": ", sin \uXXXX para lo
 * que no es ASCII), que es lo que hace el filtro «tojson» de la plantilla. -> siguiente token */
int  llm_json_reemitir(const char *t, const json_tok_t *tk, int i, llm_buf_t *o);
/* Texto UTF-8 -> cadena JSON con comillas (como json.dumps, ensure_ascii=False). */
void llm_json_cadeia(llm_buf_t *o, const char *s, int n);

/* Trozo de la conversación: texto (con o sin tokens de control) o una imagen. */
enum { SEG_TEXTO = 0, SEG_CONTROLE = 1, SEG_IMAGEM = 2 };
typedef struct {
    void (*f)(void *ctx, int tipo, const char *s, int n);   /* SEG_IMAGEM: s = la URL de la imagen */
    void *ctx;
} llm_segs_t;

/* Lo que se sabe de la petición después de la plantilla. */
typedef struct {
    int  tools;            /* índice de «tools» (-1 si no hay o tool_choice es «none») */
    int  n_imagens;
    char prefixo[200];     /* lo que se le pone al principio de la respuesta (tool_choice forzado) */
} llm_plantilla_t;

/* Recorre la petición (tk[0] es el objeto) y saca la conversación por trozos. 0 = ok; si no, el
 * mensaje en erro. «sistema» (UTF-8, puede ser NULL) es el de --sistema si la petición no trae. */
int  llm_plantilla(const char *corpo, const json_tok_t *tk, const char *sistema, llm_segs_t *s,
                   llm_plantilla_t *info, char *erro, int max);

/* Las llamadas que hay en la respuesta del modelo (UTF-8, con el prefijo ya delante).
 * «conteudo» recibe el texto de antes de la primera (sin espacios al final); «chamadas» la
 * lista JSON de tool_calls (con «index» si delta) o nada si no hay. -> cuántas llamadas */
int  llm_ferramentas_ler(const char *corpo, const json_tok_t *tk, const llm_plantilla_t *info,
                         const char *txt, int n, llm_buf_t *conteudo, llm_buf_t *chamadas, bool delta);
/* Posición de «<tool_call>» en el texto, o -1. «guardar» recibe cuántos bytes del final podrían
 * ser el principio de la marca (no se mandan aún en el streaming). */
int  llm_ferramentas_marca(const char *txt, int n, int *guardar);

#endif
