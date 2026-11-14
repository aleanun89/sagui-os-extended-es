// SPDX-License-Identifier: GPL-3.0-only
/* terminal_es.c - el terminal visto por los programas.
 *
 * Salida: el texto va a la pantalla (y al serial, que es el log de las pruebas).
 *
 * Entrada, modo línea (por defecto): el kernel monta la línea mientras la persona
 * escribe, con eco y Backspace, y solo la entrega al programa cuando llega el
 * Enter. Ctrl+D en una línea vacía significa "fin de archivo" (el "wc" leyendo
 * del teclado, por ejemplo, se detiene ahí).
 *
 * Entrada, modo en bruto: cada tecla se entrega al momento, sin eco. Teclas sin
 * carácter (flechas, Home...) se convierten en dos bytes: ESC y una letra (abi.h). El
 * shell usa este modo para tener historial y completar con Tab.
 *
 * Cuando no hay nada que entregar, el lector duerme en el canal del teclado y la
 * llamada se repite cuando llegue una tecla (D010). */
#include "terminal_es.h"
#include "abi.h"
#include "console.h"
#include "processo.h"
#include "sistema.h"
#include "teclado.h"

#define MAX_LINHA 256

static int modo = TERM_LINHA;
static char linha[MAX_LINHA];
static uint32_t tam_linha, entregue;
static bool linha_pronta;
static int pendente = -1;   /* segundo byte de una tecla especial en modo en bruto */

/* Quien puso el terminal en modo en bruto (el shell editando una línea). Si otro
 * proceso escribe en ese intervalo (un "contador &", por ejemplo), el texto
 * empieza en una línea nueva y el shell recibe ESC 'R' para redibujar el prompt y
 * lo que ya estaba escrito, en vez de que las dos cosas se mezclen. */
static int dono_bruto = -1;
static bool redesenhar;

static void eco(char c)
{
    console_char(c);
    serial_char(c);
}

int32_t terminal_escrever(const char *origem, uint32_t n)
{
    if (modo == TERM_BRUTO && processo_atual() && processo_recursos()->pid != dono_bruto && n > 0) {
        if (!redesenhar && console_coluna() != 0)
            eco('\n');
        redesenhar = true;
        acordar(teclado_canal());
    }
    for (uint32_t i = 0; i < n; i++)
        eco(origem[i]);
    console_mostrar();
    return (int32_t)n;
}

void terminal_modo(int m)
{
    modo = m;
    dono_bruto = (m == TERM_BRUTO && processo_atual()) ? processo_recursos()->pid : -1;
    redesenhar = false;
    tam_linha = entregue = 0;
    linha_pronta = false;
    pendente = -1;
}

void terminal_limpar(void)
{
    console_limpar();
    console_mostrar();
}

static char letra_especial(int t)
{
    switch (t) {
    case TECLA_CIMA: return TECLA_B_CIMA;       case TECLA_BAIXO: return TECLA_B_BAIXO;
    case TECLA_DIREITA: return TECLA_B_DIREITA; case TECLA_ESQUERDA: return TECLA_B_ESQUERDA;
    case TECLA_HOME: return TECLA_B_HOME;       case TECLA_END: return TECLA_B_END;
    case TECLA_DELETE: return TECLA_B_DELETE;   case TECLA_PGUP: return TECLA_B_PGUP;
    case TECLA_PGDN: return TECLA_B_PGDN;
    default: return 0;
    }
}

static int32_t ler_bruto(char *destino, uint32_t n)
{
    uint32_t k = 0;
    if (redesenhar && pendente < 0) {
        redesenhar = false;
        destino[k++] = 27;
        pendente = TECLA_B_REDESENHAR;
    }
    while (k < n) {
        if (pendente >= 0) { destino[k++] = (char)pendente; pendente = -1; continue; }
        int t = teclado_tentar();
        if (t < 0) break;
        if (t < 0x100) { destino[k++] = (char)t; continue; }
        char l = letra_especial(t);
        if (t == TECLA_ESC) { destino[k++] = 27; pendente = 27; continue; }   /* Esc sozinho = 27, 27 */
        if (!l) continue;
        destino[k++] = 27;
        pendente = (uint8_t)l;
    }
    if (k) return (int32_t)k;
    bloquear(teclado_canal());
    return SIS_REPETIR;
}

int32_t terminal_ler(char *destino, uint32_t n)
{
    if (n == 0) return 0;
    if (modo == TERM_BRUTO) return ler_bruto(destino, n);

    while (!linha_pronta) {
        int t = teclado_tentar();
        if (t < 0) {
            bloquear(teclado_canal());
            return SIS_REPETIR;
        }
        if (t == '\n') {
            eco('\n');
            linha[tam_linha++] = '\n';
            linha_pronta = true;
        } else if (t == '\b') {
            if (tam_linha) { tam_linha--; eco('\b'); }
        } else if (t == 4) {                 /* Ctrl+D */
            if (tam_linha == 0) return 0;    /* fin de archivo */
            linha_pronta = true;
        } else if (t == 3) {                 /* Ctrl+C: descarta la línea */
            terminal_escrever("^C\n", 3);
            tam_linha = 0;
        } else if (t >= 0x20 && t < 0x100 && t != 0x7F && tam_linha < MAX_LINHA - 1) {
            linha[tam_linha++] = (char)t;
            eco((char)t);
        }
    }
    uint32_t k = 0;
    while (k < n && entregue < tam_linha)
        destino[k++] = linha[entregue++];
    if (entregue == tam_linha) {
        tam_linha = entregue = 0;
        linha_pronta = false;
    }
    return (int32_t)k;
}
