// SPDX-License-Identifier: GPL-2.0-or-later
/* kconfig - opciones del kernel que se guardan desde el sistema y valen al reiniciar (D353).
 *
 *   kconfig                      la lista: valor de este arranque, de la imagen, guardado y si hace
 *                                falta reiniciar
 *   kconfig OPCION=VALOR ...     guarda (solo administrador: «admin kconfig ...»); VALOR = si|no, un
 *                                número, auto, un nombre (gobernador) o «defecto» (vuelve el de la imagen)
 *   kconfig --por-defecto        borra todo lo guardado: al reiniciar, los valores de la imagen
 *   kconfig -h                   ayuda, con lo que hace cada opción
 *
 * Lo guardado va en la cabecera de arranque del disco del que arrancó SaguiOS (como «pantalla
 * --guardar»); desde un CD no se puede. Si lo guardado no deja arrancar: Ctrl al arrancar (modo
 * rescate, que no lo aplica) y «kconfig --por-defecto».
 * El nombre del archivo es el de la orden (interfaz). */
#include "sagui.h"
#include "opcoes_kernel.h"

static info_kconf_t ops[KCONF_MAX];
static int n_ops;
static unsigned est;

static int ler_ops(void)
{
    n_ops = kconf_ler(ops, KCONF_MAX, &est);
    if (n_ops < 0) {
        imprimir_em(ERROS, _("kconfig: este kernel no tiene opciones guardables (%d)\n"), n_ops);
        return -1;
    }
    return 0;
}

static const info_kconf_t *achar(const char *nome)
{
    for (int i = 0; i < n_ops; i++)
        if (compara_texto(ops[i].nome, nome) == 0) return &ops[i];
    return NULL;
}

static void listar_ops(void)
{
    char a[24], b[24], c[24];
    imprimir(_("Opciones del kernel (se aplican al reiniciar):\n"));
    imprimir("%-20s %-13s %-13s %-13s %s\n", _("OPCIÓN"), _("AHORA"), _("IMAGEN"), _("GUARDADO"), _("AL REINICIAR"));
    bool falta = false;
    for (int i = 0; i < n_ops; i++) {
        const info_kconf_t *o = &ops[i];
        kconf_texto_valor(o, o->efetivo, a, sizeof a);
        kconf_texto_valor(o, o->imagem, b, sizeof b);
        if (o->guardado) kconf_texto_valor(o, o->valor_guardado, c, sizeof c);
        else copia_texto(c, "-", sizeof c);
        bool r = kconf_pide_reinicio(o);
        falta |= r;
        const char *nota = o->ignorada == KCONF_IGN_RANGO ? _("(guardado fuera de rango: ignorado)")
                         : o->ignorada == KCONF_IGN_RESCATE ? _("(no aplicado: modo rescate)")
                         : r ? _("cambia al reiniciar") : "";
        imprimir("%-20s %-13s %-13s %-13s %s\n", o->nome, a, b, c, nota);
    }
    if (est & KCONF_EST_RESCATE)
        imprimir(_("Modo rescate: lo guardado no se aplica en este arranque. «kconfig --por-defecto» lo borra.\n"));
    if (est & KCONF_EST_DANADA)
        imprimir(_("Aviso: lo guardado está dañado y no se aplica; al guardar algo se empieza de cero.\n"));
    if (est & KCONF_EST_CD)
        imprimir(_("Arranque desde CD: no se puede guardar nada.\n"));
    if (falta) imprimir(_("Hay cambios guardados que se aplicarán al reiniciar.\n"));
}

static void ajuda(void)
{
    imprimir(_("uso: kconfig [OPCION=VALOR ...] | kconfig --por-defecto\n"));
    imprimir(_("Valores: si|no, un número, auto, o «defecto» (vuelve al de la imagen).\n"));
    for (int i = 0; i < n_ops; i++) {
        const info_kconf_t *o = &ops[i];
        char rango[48];
        if (o->tipo == KCONF_SINO) copia_texto(rango, "si|no", sizeof rango);
        else if (compara_texto(o->nome, "gobernador") == 0) copia_texto(rango, "auto|rendimiento|ahorro|demanda", sizeof rango);
        else if (compara_texto(o->nome, "raiz_modo") == 0) copia_texto(rango, "auto|live|toram", sizeof rango);
        else formatar_texto(rango, sizeof rango, "auto|%u-%u", o->min, o->max);
        imprimir("  %-20s %-32s %s\n", o->nome, rango, kconf_rotulo(o->nome));
        imprimir("  %-20s %s\n", "", kconf_ayuda(o->nome));
    }
    imprimir(_("Si lo guardado impide arrancar: Ctrl al arrancar (modo rescate) y «kconfig --por-defecto».\n"));
}

static int erro_gravar(const char *que, int r)
{
    if (r == ERRO_ACESSO)
        imprimir_em(ERROS, _("kconfig: hace falta ser administrador (usa: admin kconfig %s)\n"), que);
    else if (r == ERRO_SO_LEITURA)
        imprimir_em(ERROS, _("kconfig: SaguiOS ha arrancado desde un CD, que es de solo lectura: no se puede guardar\n"));
    else if (r == ERRO_NAO_EXISTE)
        imprimir_em(ERROS, _("kconfig: no se encontró el disco de arranque de SaguiOS para guardar\n"));
    else imprimir_em(ERROS, _("kconfig: %s: no se pudo guardar: %s\n"), que, nome_erro(r));
    return 1;
}

int principal(int argc, char **argv)
{
    if (ler_ops() < 0) return 1;
    if (argc < 2) { listar_ops(); return 0; }
    if (compara_texto(argv[1], "-h") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0) {
        ajuda();
        return 0;
    }
    if (compara_texto(argv[1], "--por-defecto") == 0) {
        int r = kconf_quitar_todo();
        if (r < 0) return erro_gravar("--por-defecto", r);
        imprimir(_("Configuración del kernel borrada: al reiniciar, los valores de la imagen.\n"));
        return 0;
    }
    /* Primero se comprueba todo y después se guarda: un error no deja la mitad guardada. */
    int huecos[KCONF_MAX], quitar[KCONF_MAX];
    unsigned valores[KCONF_MAX];
    int n = 0;
    for (int i = 1; i < argc && n < KCONF_MAX; i++) {
        char nome[32];
        const char *ig = argv[i];
        while (*ig && *ig != '=') ig++;
        if (*ig != '=' || ig == argv[i] || (size_t)(ig - argv[i]) >= sizeof nome) {
            imprimir_em(ERROS, _("kconfig: «%s»: se espera OPCION=VALOR (kconfig -h)\n"), argv[i]);
            return 2;
        }
        copia_texto(nome, argv[i], (size_t)(ig - argv[i]) + 1);
        const info_kconf_t *o = achar(nome);
        if (!o) { imprimir_em(ERROS, _("kconfig: no hay ninguna opción «%s» (kconfig -h)\n"), nome); return 2; }
        unsigned v = 0;
        int q = kconf_interpretar(o, ig + 1, &v);
        if (q < 0) {
            if (o->tipo == KCONF_SINO) imprimir_em(ERROS, _("kconfig: %s: «%s» no vale (si o no)\n"), nome, ig + 1);
            else imprimir_em(ERROS, _("kconfig: %s: «%s» fuera de rango (auto o de %u a %u)\n"), nome, ig + 1, o->min, o->max);
            return 2;
        }
        huecos[n] = o->hueco;
        quitar[n] = q == 1;
        valores[n] = v;
        n++;
    }
    /* D367: dependencias entre opciones: nunca se guarda una combinación que no tiene sentido */
    kconf_cambio_t ped[KCONF_MAX], fin[KCONF_MAX];
    for (int i = 0; i < n; i++) { ped[i].hueco = huecos[i]; ped[i].valor = valores[i]; ped[i].quitar = quitar[i]; ped[i].aviso = NULL; }
    char err[120];
    int nf = kconf_resolver(ops, n_ops, ped, n, fin, KCONF_MAX, err, sizeof err);
    if (nf < 0) { imprimir_em(ERROS, _("kconfig: combinación no válida (%s); no se guarda nada\n"), err); return 2; }
    for (int i = 0; i < nf; i++) {
        int r = fin[i].quitar ? kconf_quitar(fin[i].hueco) : kconf_fijar(fin[i].hueco, fin[i].valor);
        if (r < 0) return erro_gravar(i < n ? argv[i + 1] : "(ajuste)", r);
    }
    for (int i = n; i < nf; i++)
        for (int k = 0; k < n_ops; k++)
            if (ops[k].hueco == fin[i].hueco) imprimir(_("%s: ajustado a %s solo (%s)\n"), ops[k].nome, fin[i].valor ? "si" : "no", fin[i].aviso);
    ler_ops();
    for (int i = 0; i < n; i++) {
        const info_kconf_t *o = NULL;
        for (int k = 0; k < n_ops; k++)
            if (ops[k].hueco == huecos[i]) o = &ops[k];
        if (!o) continue;
        char t[24];
        kconf_texto_valor(o, o->proximo, t, sizeof t);
        if (quitar[i]) imprimir(_("%s: vuelve al de la imagen (%s)"), o->nome, t);
        else imprimir(_("%s: guardado %s"), o->nome, t);
        imprimir("%s\n", kconf_pide_reinicio(o) ? _("; se aplica al reiniciar") : "");
    }
    return 0;
}
