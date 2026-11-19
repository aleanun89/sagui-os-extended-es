// SPDX-License-Identifier: GPL-2.0-or-later
/* brillo [0-100 | + | - | +N | -N] - el brillo de la pantalla del portátil (D403).
 *
 * Sin argumentos dice el brillo y de dónde sale el control (ACPI Video, Fujitsu
 * o Toshiba). Un número pone ese porcentaje (el nivel más cercano que admite la
 * pantalla); "+" y "-" suben o bajan un nivel (+N/-N, N niveles), como las teclas.
 * Lo hace el kernel con el intérprete ACPI (imagen con --uacpi). */
#include "sagui.h"
#include "portatil.h"

static const char *fonte(unsigned f)
{
    switch (f) {
    case PORTATIL_FONTE_VIDEO:   return "ACPI Video";
    case PORTATIL_FONTE_FUJITSU: return "Fujitsu";
    case PORTATIL_FONTE_TOSHIBA: return "Toshiba";
    }
    return "ninguno";
}

static int sin_control(const info_portatil_t *p)
{
    if (p->uacpi != UACPI_ATIVO)
        imprimir_em(ERROS, "brillo: no disponible (%s)\n", p->motivo[0] ? p->motivo : "sin el intérprete ACPI");
    else
        imprimir_em(ERROS, "brillo: este equipo no deja cambiar el brillo (%s)\n",
                    p->motivo[0] ? p->motivo : "no hay dispositivo de brillo");
    return 1;
}

static int mostrar(void)
{
    info_portatil_t p;
    if (portatil_estado(&p) < 0) { imprimir_em(ERROS, "brillo: el kernel no responde\n"); return 1; }
    if (p.brilho_fonte == PORTATIL_FONTE_NADA || p.brilho_pct < 0) return sin_control(&p);
    imprimir("Brillo: %d %% (nivel %u de %u, %s %s)\n", p.brilho_pct, p.brilho_nivel + 1, p.brilho_niveis,
             fonte(p.brilho_fonte), p.brilho_fonte == PORTATIL_FONTE_VIDEO ? p.video_lcd :
             p.brilho_fonte == PORTATIL_FONTE_FUJITSU ? p.fujitsu_no : p.toshiba_no);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc < 2) return mostrar();
    const char *a = argv[1];
    if (compara_texto(a, "-h") == 0 || compara_texto(a, "--ayuda") == 0) {
        imprimir("uso: brillo [0-100 | + | - | +N | -N]\n");
        return 0;
    }
    int modo, v = 1;
    if (a[0] == '+' || a[0] == '-') {
        if (a[1] && !texto_para_numero(a + 1, &v)) { imprimir_em(ERROS, "uso: brillo [0-100 | + | - | +N | -N]\n"); return 2; }
        modo = PORTATIL_BRILHO_PASSO;
        if (a[0] == '-') v = -v;
    } else {
        if (!texto_para_numero(a, &v) || v > 100) { imprimir_em(ERROS, "uso: brillo [0-100 | + | - | +N | -N]\n"); return 2; }
        modo = PORTATIL_BRILHO_FIXAR;
    }
    int r = portatil_brilho(modo, v);
    if (r == ERRO_NAO_EXISTE) {
        info_portatil_t p;
        portatil_estado(&p);
        return sin_control(&p);
    }
    if (r == ERRO_ACESSO) { imprimir_em(ERROS, "brillo: no permitido (solo desde la sesión)\n"); return 1; }
    if (r < 0) { imprimir_em(ERROS, "brillo: no se pudo cambiar (%s)\n", nome_erro(r)); return 1; }
    return mostrar();
}
