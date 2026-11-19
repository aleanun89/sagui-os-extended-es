// SPDX-License-Identifier: GPL-3.0-only
/* console.c - la salida de texto del kernel: kprintf, klog y la consola.
 *
 * La consola tiene dos motores: el modo texto de la tarjeta VGA (fases 1 a 3, y
 * reserva si el modo gráfico no se activa) y el terminal gráfico dibujado con la
 * fuente propia (fase 4 en adelante). Quien escribe no necesita saber cuál. */
#include "console.h"
#include "cpu.h"
#include "texto.h"
#include "spinlock.h"

void textovga_iniciar(void);
void textovga_limpar(void);
void textovga_cor(uint8_t frente, uint8_t fundo);
void textovga_char(char c);

bool termgraf_iniciar(void);
void termgraf_limpar(void);
void termgraf_cor(uint8_t frente, uint8_t fundo);
void termgraf_char(char c);
void termgraf_mostrar(void);
int  termgraf_colunas(void);
int  termgraf_coluna(void);
int  textovga_coluna(void);
void termgraf_redimensionar(bool desenhar);

static bool grafico;
static bool escondido;   /* con el escritorio en marcha, el texto del kernel va solo al serie */

void console_esconder(bool sim) { escondido = sim; }

void console_iniciar(void)
{
    textovga_iniciar();
}

/* Se llama después de que la memoria está lista (el terminal gráfico necesita el
 * kmalloc para el búfer de atrás). Devuelve si el modo gráfico se activó. */
bool console_grafico(void)
{
    grafico = termgraf_iniciar();
    return grafico;
}

bool console_em_grafico(void) { return grafico; }

/* El modo de vídeo cambió (D059): el terminal rehace la cuadrícula; escondido (con el
 * escritorio en marcha) no dibuja nada. */
void console_tela_mudou(void)
{
    if (grafico) termgraf_redimensionar(!escondido);
}
int  console_colunas(void)     { return grafico ? termgraf_colunas() : 80; }
int  console_coluna(void)      { return grafico ? termgraf_coluna() : textovga_coluna(); }

void console_limpar(void)
{
    if (escondido) return;
    if (grafico) termgraf_limpar(); else textovga_limpar();
}

void console_cor(uint8_t frente, uint8_t fundo)
{
    if (grafico) termgraf_cor(frente, fundo); else textovga_cor(frente, fundo);
}

void console_char(char c)
{
    if (escondido) return;
    if (grafico) termgraf_char(c); else textovga_char(c);
}

/* En modo gráfico el dibujo queda en el búfer de atrás hasta que se llame a esto. */
void console_mostrar(void)
{
    if (grafico && !escondido) termgraf_mostrar();
}

/* Últimos 128 KB del log del kernel, en anillo (D076; 32 KB hasta el 2026-11-19: en el Amilo, las líneas de
 * sshd echaban del anillo el diagnóstico de la GPU en pocos minutos, D387): el infopc --informe lo
 * copia al archivo, porque en un PC sin puerto serie ese log se perdería
 * perdido. Solo lo que pasa por kprintf y klog (el kernel y el registrar() de los
 * programas); la salida común de los programas no entra. */
#define LOG_TAM (128 * 1024)
static char log_anel[LOG_TAM];
static uint32_t log_fim;      /* bytes ya escritos desde el arranque (la posición es log_fim % LOG_TAM) */

static void log_guardar(const char *s)
{
    bool i = int_salva_desliga();   /* la IRQ del mouse y la red también registran */
    for (; *s; s++) log_anel[log_fim++ % LOG_TAM] = *s;
    int_restaura(i);
}

/* Copia el fin del log (hasta max bytes, terminados en cero) a dst; devuelve
 * cuántos bytes copió, sin el cero. Empieza al inicio de una línea. */
uint32_t console_log_copiar(char *dst, uint32_t max)
{
    if (!max) return 0;
    bool i = int_salva_desliga();
    uint32_t total = log_fim < LOG_TAM ? log_fim : LOG_TAM, n = total < max - 1 ? total : max - 1;
    uint32_t ini = log_fim - n;
    for (uint32_t k = 0; k < n; k++) dst[k] = log_anel[(ini + k) % LOG_TAM];
    int_restaura(i);
    dst[n] = 0;
    uint32_t pulo = 0;
    if (n < log_fim) {   /* la primera línea puede estar cortada */
        while (pulo < n && dst[pulo] != '\n') pulo++;
        if (pulo < n) pulo++;
        memmove(dst, dst + pulo, n - pulo + 1);
    }
    return n - pulo;
}

/* A consola e partilhada: com varios processadores, dois podem escrever ao
 * mesmo tempo (D157). O cerrojo sabe de quem e, para uma interrupcao nao
 * ficar a espera dele proprio. D165: se toma con las interrupciones
 * apagadas. Si el reloj cambiaba de proceso con el cerrojo tomado (el
 * proceso 0 escribe con las interrupciones puestas), el código que lo tenía
 * quedaba parado y otra CPU, ya con el cerrojo general, esperaba la consola
 * para siempre: la CPU 0 no podía volver a entrar en el kernel para soltarla. */
static spinlock_t console_lock = SPINLOCK_INICIAL("consola");

void kprintf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    formatar(buf, sizeof buf, fmt, ap);
    va_end(ap);
    bool i = int_salva_desliga();
    spin_lock(&console_lock);
    for (char *p = buf; *p; p++) {
        console_char(*p);
        serial_char(*p);
    }
    log_guardar(buf);
    console_mostrar();
    spin_unlock(&console_lock);
    int_restaura(i);
}

void klog(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    formatar(buf, sizeof buf, fmt, ap);
    va_end(ap);
    /* La línea entera sin interrupciones: si no, otro proceso puede escribir en
     * la serie en medio de ella (h16 veía «[SAGUI] exec» + texto del shell). */
    bool i = int_salva_desliga();
    spin_lock(&console_lock);
    serial_texto(buf);
    log_guardar(buf);
    spin_unlock(&console_lock);
    int_restaura(i);
}
