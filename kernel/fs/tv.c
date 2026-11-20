// SPDX-License-Identifier: GPL-2.0-or-later
/* tv.c - terminal virtual (pseudoterminal), D036.
 *
 * Hasta ahora, el Terminal en ventana conectaba el shell por dos pipes simples: las
 * teclas llegaban al programa sin eco y sin edición, porque el "modo línea"
 * (eco, Backspace, Enter, Ctrl+D) sólo existía en el terminal del kernel. El sagsh
 * se las arreglaba solo en el modo bruto, pero cualquier otro programa que leyera
 * una línea (python, input(), fgets de un programa de cc, "cat" sin
 * archivo) no mostraba nada de lo que se escribía.
 *
 * Un terminal virtual tiene dos extremos, como en Unix:
 *   - el maestro queda con quien dibuja (el Terminal): escribe las teclas y lee
 *     lo que debe aparecer en pantalla;
 *   - el esclavo pasa a ser la entrada 0 y las salidas 1 y 2 del shell y de los
 *     programas, y se comporta como el terminal del kernel: modo línea o modo bruto.
 * La disciplina de línea se ejecuta cuando la tecla LLEGA (al escribir en el maestro),
 * no cuando el programa lee: el eco aparece aunque nadie esté leyendo, y
 * Ctrl+C termina el programa en primer plano aunque esté atrapado
 * en un bucle sin leer nada. */
#include "tv.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "sistema.h"

tv_t *tv_criar(void)
{
    tv_t *t = kcalloc(sizeof(tv_t));
    if (!t) return NULL;
    t->entrada = pipe_criar();
    t->saida = pipe_criar();
    if (!t->entrada || !t->saida) {
        if (t->entrada) kfree(t->entrada);
        if (t->saida) kfree(t->saida);
        kfree(t);
        return NULL;
    }
    t->modo = TERM_LINHA;
    t->dono_bruto = -1;
    t->pendente = -1;
    t->coluna0 = true;
    t->mestre_aberto = t->escravo_aberto = true;
    return t;
}

/* Pone bytes en un pipe sin bloquear: lo que no quepa se pierde (teclas
 * de más con el programa detenido, o eco con la pantalla detenida). */
static void por(pipe_t *p, const char *s, uint32_t n)
{
    for (uint32_t i = 0; i < n && p->qtd < PIPE_TAM; i++) {
        p->buf[(p->ini + p->qtd) % PIPE_TAM] = (uint8_t)s[i];
        p->qtd++;
    }
    acordar(p);
}

static void para_tela(tv_t *t, const char *s, uint32_t n)
{
    if (!n || !t->mestre_aberto) return;
    por(t->saida, s, n);
    t->coluna0 = s[n - 1] == '\n';
}

/* ¿El proceso "pid" está en el grupo en primer plano? El grupo es el número del
 * proceso que lo lidera (D037); por compatibilidad, el propio pid también
 * cuenta. */
static bool no_primeiro_plano(tv_t *t, int pid)
{
    if (!t->primeiro_plano || pid <= 0) return false;
    if (pid == t->primeiro_plano) return true;
    processo_t *p = processo_por_pid(pid);
    return p && p->grupo == t->primeiro_plano;
}

/* Ctrl+C: termina todos los procesos del grupo en primer plano (todas las
 * etapas de un pipe, un script y los programas que ejecutó) y cuenta la
 * interrupción, para que el shell abandone un bucle que ejecuta él mismo. */
static void interromper(tv_t *t)
{
    para_tela(t, "^C\n", 3);
    t->tam = 0;
    t->interrupcoes++;
    if (!t->primeiro_plano) {
        /* Nadie que terminar: es el propio shell ejecutando un interno (un
         * "read", un bucle). La lectura pendiente vuelve como fin de archivo y el
         * shell ve la interrupción por el contador. */
        t->cancelar_leitura = true;
        acordar(t->entrada);
        return;
    }
    /* D401: SIGINT a cada proceso del grupo. Sin tratador termina como siempre (código 130,
     * «interrumpido con Ctrl+C»); con tratador, lo atiende el programa. */
    for (int i = 0; i < MAX_PROCESSOS; i++) {
        processo_t *p = processo_indice(i);
        if (!p || !p->usuario || p->lider || p->estado == P_ZUMBI || p->estado == P_LIVRE) continue;
        if (p->pid == t->primeiro_plano || p->grupo == t->primeiro_plano)
            sinais_enviar(p, SINAL_INT, 0, 0, SINAL_SI_KERNEL);
    }
}

/* Una tecla en modo línea. */
static void tecla_linha(tv_t *t, uint8_t c)
{
    if (t->esc) { t->esc = 0; return; }   /* tecla especial (ESC + letra): sin uso aquí */
    if (c == 27) { t->esc = 1; return; }
    if (c == '\n' || c == '\r') {
        t->linha[t->tam++] = '\n';
        para_tela(t, "\n", 1);
        por(t->entrada, t->linha, t->tam);
        t->tam = 0;
    } else if (c == '\b' || c == 0x7F) {
        if (t->tam) { t->tam--; para_tela(t, "\b", 1); }
    } else if (c == 4) {                       /* Ctrl+D */
        if (t->tam == 0) { t->fim_pendente = true; acordar(t->entrada); }
        else { por(t->entrada, t->linha, t->tam); t->tam = 0; }
    } else if (c == 3) {                       /* Ctrl+C */
        interromper(t);
    } else if ((c >= 0x20 || c == '\t') && t->tam < sizeof t->linha - 1) {
        t->linha[t->tam++] = (char)c;
        para_tela(t, (const char *)&c, 1);
    }
}

int32_t tv_mestre_escrever(tv_t *t, const char *origem, uint32_t n)
{
    if (!t->escravo_aberto) return ERRO_PIPE_QUEBRADO;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = (uint8_t)origem[i];
        if (t->modo == TERM_BRUTO) {
            /* En modo bruto, quien lee decide qué hacer con cada tecla, salvo el
             * Ctrl+C cuando el lector no es el primer plano (el shell editando
             * mientras un programa se ejecuta). */
            if (c == 3 && t->primeiro_plano && !no_primeiro_plano(t, t->dono_bruto)) interromper(t);
            else por(t->entrada, (const char *)&c, 1);
        } else {
            tecla_linha(t, c);
        }
    }
    return (int32_t)n;
}

int32_t tv_mestre_ler(tv_t *t, char *destino, uint32_t n)
{
    return pipe_ler(t->saida, destino, n);
}

int32_t tv_mestre_disponivel(tv_t *t)
{
    if (t->saida->qtd) return (int32_t)t->saida->qtd;
    return t->escravo_aberto ? 0 : ERRO_PIPE_QUEBRADO;
}

/* D317: cuánto puede leer el programa sin bloquearse (disponivel sobre el
 * esclavo). Lo usan el cliente ssh, que atiende a la vez el teclado y la red,
 * y el keypressed del Pascal. Mismo orden que tv_ler: si dice que hay algo, la
 * lectura vuelve en el acto (con datos o con fin de archivo). */
int32_t tv_escravo_disponivel(tv_t *t)
{
    if (t->pendente >= 0) return 1;
    if (t->modo == TERM_BRUTO && t->redesenhar && processo_atual() && processo_recursos()->pid == t->dono_bruto) return 2;
    if (t->entrada->qtd) return (int32_t)t->entrada->qtd;
    if (t->fim_pendente || t->cancelar_leitura) return 1;   /* la lectura devolverá 0 (fin de archivo) */
    return t->mestre_aberto ? 0 : ERRO_PIPE_QUEBRADO;
}

int32_t tv_ler(tv_t *t, char *destino, uint32_t n)
{
    if (n == 0) return 0;
    uint32_t k = 0;
    if (t->pendente >= 0) {   /* la 'R' que no cupo en la lectura anterior */
        destino[k++] = (char)t->pendente;
        t->pendente = -1;
        return (int32_t)k;
    }
    if (t->modo == TERM_BRUTO && t->redesenhar && processo_atual() && processo_recursos()->pid == t->dono_bruto) {
        /* El shell lee una tecla por vez: el ESC va ahora y la 'R' queda para la
         * próxima lectura. Devolver 0 aquí sería "fin de archivo". */
        t->redesenhar = false;
        destino[k++] = 27;
        if (n >= 2) destino[k++] = TECLA_B_REDESENHAR;
        else t->pendente = TECLA_B_REDESENHAR;
        return (int32_t)k;
    }
    if (t->entrada->qtd) return pipe_ler(t->entrada, destino, n);
    if (t->fim_pendente) { t->fim_pendente = false; return 0; }
    if (t->cancelar_leitura) { t->cancelar_leitura = false; return 0; }
    if (!t->mestre_aberto) return 0;
    bloquear(t->entrada);
    return SIS_REPETIR;
}

int32_t tv_escrever(tv_t *t, const char *origem, uint32_t n)
{
    if (!t->mestre_aberto) return ERRO_PIPE_QUEBRADO;
    if (n == 0) return 0;
    /* Otro proceso escribiendo mientras el shell edita la línea (D018): el texto
     * empieza en una línea nueva y el shell redibuja el prompt después. */
    if (t->modo == TERM_BRUTO && processo_atual() && processo_recursos()->pid != t->dono_bruto) {
        if (!t->redesenhar && !t->coluna0) para_tela(t, "\n", 1);
        t->redesenhar = true;
        acordar(t->entrada);
    }
    int32_t r = pipe_escrever(t->saida, origem, n);
    if (r > 0) t->coluna0 = origem[r - 1] == '\n';
    return r;
}

void tv_modo(tv_t *t, int modo)
{
    t->modo = modo;
    t->dono_bruto = (modo == TERM_BRUTO && processo_atual()) ? processo_recursos()->pid : -1;
    t->redesenhar = false;
    t->pendente = -1;
    t->cancelar_leitura = false;
    t->tam = 0;
    t->esc = 0;
}

void tv_limpar(tv_t *t)
{
    para_tela(t, "\x1b[J", 3);
    t->coluna0 = true;
}

/* Se cerró la última referencia a uno de los extremos. */
void tv_fechar(tv_t *t, bool mestre)
{
    if (mestre) t->mestre_aberto = false; else t->escravo_aberto = false;
    t->entrada->escritores = t->mestre_aberto ? 1 : 0;
    t->saida->escritores = t->escravo_aberto ? 1 : 0;
    acordar(t->entrada);
    acordar(t->saida);
    if (!t->mestre_aberto && !t->escravo_aberto) {
        kfree(t->entrada);
        kfree(t->saida);
        kfree(t);
    }
}
