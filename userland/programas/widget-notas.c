// SPDX-License-Identifier: GPL-3.0-only
/* widget-notas - una nota adhesiva en el escritorio (D365). El texto se guarda en
 * /home/.config/widget-notas.txt. Un clic abre una ventana pequeña para editarla (el widget no
 * toma el teclado); al cerrarla o pulsar «Guardar» se guarda y la nota se repinta. */
#include "widget.h"

#define L 200
#define A 184
#define NOTA_ARQ "/home/.config/widget-notas.txt"
#define NOTA_MAX 3000
#define COLS 22

static char nota[NOTA_MAX + 1];

static void cargar(void)
{
    nota[0] = 0;
    int fd = abrir(NOTA_ARQ, ABRIR_LER);
    if (fd < 0) { copia_texto(nota, _("Nota adhesiva: haz clic para escribir."), sizeof nota); return; }
    int n = ler(fd, nota, NOTA_MAX);
    fechar(fd);
    nota[n > 0 ? n : 0] = 0;
}

static void guardar(void)
{
    criar_pasta("/home/.config");
    int fd = abrir(NOTA_ARQ, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever(fd, nota, (unsigned)tam_texto(nota));
    fechar(fd);
}

static bool tique(jan_t *j)
{
    /* La nota: papel amarillo con la esquina de arriba más oscura. */
    for (int y = 0; y < A; y++)
        g_retangulo(j, 0, y, L, 1, y < 22 ? 0xE0B83C : 0xF6E27A);
    g_moldura(j, 0, 0, L, A, 0x9A7A1C);
    g_texto(j, 8, 3, _("Nota"), 0x5A430A);
    /* El texto, partido en líneas de COLS letras (por palabras si se puede). */
    int y = 28, x = 0;
    const char *p = nota;
    char linha[COLS + 1];
    while (*p && y + 16 <= A - 4) {
        int n = 0;
        while (p[n] && p[n] != '\n' && n < COLS) n++;
        if (p[n] && p[n] != '\n') {   /* cortó en medio de una palabra: retrocede hasta un espacio */
            int k = n;
            while (k > 0 && p[k] != ' ') k--;
            if (k > 0) n = k;
        }
        for (int i = 0; i < n; i++) linha[i] = p[i];
        linha[n] = 0;
        g_texto(j, 8 + x, y, linha, 0x3A2E08);
        y += 16;
        p += n;
        if (*p == '\n' || *p == ' ') p++;
    }
    return true;
}

static void editar(jan_t *j)
{
    jan_t *e = jan_abrir(_("Nota adhesiva"), 340, 280);
    if (!e) return;
    area_t a;
    if (!area_iniciar(&a, 8, 8, 324, 222)) { jan_fechar(e); return; }
    area_definir(&a, nota, (int)tam_texto(nota));
    a.foco = true;
    botao_t guardar_b = { 230, 240, 100, 28, _("Guardar"), false };
    area_desenhar(e, &a);
    botao_desenhar(e, &guardar_b);
    jan_mostrar(e);
    evento_t ev;
    for (;;) {
        if (proximo_evento(e->id, &ev, true) != 1) continue;
        if (ev.tipo == EV_FECHAR) break;
        if (botao_tratar(e, &guardar_b, &ev)) break;
        if (area_tratar(e, &a, &ev)) { area_desenhar(e, &a); jan_mostrar(e); }
    }
    int n = a.tam < NOTA_MAX ? a.tam : NOTA_MAX;
    for (int i = 0; i < n; i++) nota[i] = a.buf[i];
    nota[n] = 0;
    guardar();
    jan_fechar(e);
    tique(j);
}

static bool evento(jan_t *j, const evento_t *e)
{
    if (e->tipo != EV_CLIQUE || !(e->botoes & 1)) return false;
    editar(j);
    return true;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    cargar();
    jan_t *j = widget_abrir("notas", L, A, 260, 40, false, 100);
    if (!j) return 1;
    widget_ejecutar(j, "notas", 3600000, tique, evento);   /* sin temporizador útil: solo eventos */
    return 0;
}
