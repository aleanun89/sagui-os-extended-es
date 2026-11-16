// SPDX-License-Identifier: GPL-2.0-or-later
/* touch (tocar) - crea archivos vacíos si no existen y fija sus fechas (D097, D341).
 * Las fechas las fija el kernel (PARQ_DATAS): la hora actual, o la de -t, -d o -r. */
#include "sagui.h"

static const char *AYUDA =
    "uso: touch [-c] [-a] [-m] [-t [[SS]AA]MMDDhhmm[.ss]] [-d FECHA] [-r ARCHIVO] archivo...\n"
    "  -c  no crea los archivos que no existan\n"
    "  -a  cambia solo la fecha de acceso      -m  cambia solo la de modificación\n"
    "  -t  fecha en hora local: 202611161230.45 = 16/11/2026 12:30:45\n"
    "  -d  fecha en hora local: \"2026-11-16 12:30:45\", \"2026-11-16\" o @segundos (UTC desde 1970)\n"
    "  -r  usa las fechas de ARCHIVO\n"
    "Sin -t, -d ni -r pone la hora actual. En FAT la hora se guarda de 2 en 2 segundos.\n";

/* Lee "n" cifras de *p. -1 si no lo son. */
static int cifras(const char **p, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') return -1;
        v = v * 10 + (c - '0');
    }
    *p += n;
    return v;
}

static bool hora_valida(int ano, int mes, int dia, int h, int m, int s)
{
    return ano >= 1970 && ano <= 2105 && mes >= 1 && mes <= 12 && dia >= 1 && dia <= fuso_dias_mes(ano, mes) &&
           h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 60;
}

/* -t [[SS]AA]MMDDhhmm[.ss], en hora local. 0 = inválida. */
static uint32_t ler_t(const char *t)
{
    size_t n = 0;
    while (t[n] && t[n] != '.') n++;
    fuso_data_t hoje;
    agora_local(&hoje);
    int ano = hoje.ano, s = 0;
    const char *p = t;
    if (n == 12) ano = cifras(&p, 4);
    else if (n == 10) { int aa = cifras(&p, 2); ano = aa < 0 ? -1 : (aa >= 69 ? 1900 + aa : 2000 + aa); }
    else if (n != 8) return 0;
    int mes = cifras(&p, 2), dia = cifras(&p, 2), h = cifras(&p, 2), m = cifras(&p, 2);
    if (*p == '.') { p++; s = cifras(&p, 2); }
    if (*p || ano < 0 || mes < 0 || dia < 0 || h < 0 || m < 0 || s < 0 || !hora_valida(ano, mes, dia, h, m, s)) return 0;
    return local_para_utc(fuso_compor(ano, mes, dia, h, m, s), NULL);
}

/* -d "AAAA-MM-DD[ hh:mm[:ss]]" (también con 'T'), en hora local, o "@segundos" en UTC. 0 = inválida. */
static uint32_t ler_d(const char *t)
{
    const char *p = t;
    if (*p == '@') {
        uint32_t v = 0;
        if (!*++p) return 0;
        for (; *p; p++) { if (*p < '0' || *p > '9') return 0; v = v * 10 + (uint32_t)(*p - '0'); }
        return v;
    }
    int ano = cifras(&p, 4);
    if (ano < 0 || *p++ != '-') return 0;
    int mes = cifras(&p, 2);
    if (mes < 0 || *p++ != '-') return 0;
    int dia = cifras(&p, 2), h = 0, m = 0, s = 0;
    if (dia < 0) return 0;
    if (*p == ' ' || *p == 'T') {
        p++;
        h = cifras(&p, 2);
        if (h < 0 || *p++ != ':') return 0;
        m = cifras(&p, 2);
        if (m < 0) return 0;
        if (*p == ':') { p++; s = cifras(&p, 2); }
    }
    if (*p || s < 0 || !hora_valida(ano, mes, dia, h, m, s)) return 0;
    return local_para_utc(fuso_compor(ano, mes, dia, h, m, s), NULL);
}

int principal(int argc, char **argv)
{
    bool no_crear = false, so_acesso = false, so_mod = false;
    unsigned mod = PARQ_DATA_AGORA, ace = PARQ_DATA_AGORA;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "camft:d:r:", &o)) >= 0) {
        if (r == 'c') no_crear = true;
        else if (r == 'a') so_acesso = true;
        else if (r == 'm') so_mod = true;
        else if (r == 'f') continue;
        else if (r == 't' || r == 'd') {
            uint32_t v = r == 't' ? ler_t(o.valor) : ler_d(o.valor);
            if (!v) { imprimir_em(ERROS, "touch: fecha no válida: %s\n", o.valor); return 2; }
            mod = ace = v;
        } else if (r == 'r') {
            datas_arquivo_t d;
            int e = datas_arquivo(o.valor, &d);
            if (e < 0) { imprimir_em(ERROS, "touch: %s: %s\n", o.valor, nome_erro(e)); return 1; }
            mod = d.modificado ? d.modificado : PARQ_DATA_AGORA;
            ace = d.acessado ? d.acessado : mod;
        }
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("touch", r, &o);
    }
    if (so_acesso && !so_mod) mod = PARQ_DATA_MANTER;
    if (so_mod && !so_acesso) ace = PARQ_DATA_MANTER;
    if (o.indice >= argc) { imprimir_em(ERROS, "touch: falta el nombre del archivo (touch --help)\n"); return 2; }
    int erros = 0;
    for (int i = o.indice; i < argc; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) < 0) {
            if (no_crear) continue;
            int fd = abrir(argv[i], ABRIR_ESCREVER | ABRIR_CRIAR);
            if (fd < 0) { imprimir_em(ERROS, "touch: %s: %s\n", argv[i], nome_erro(fd)); erros++; continue; }
            fechar(fd);
            if (mod == PARQ_DATA_AGORA && ace == PARQ_DATA_AGORA) continue;   /* recién creado: ya es ahora */
        }
        r = fixar_datas(argv[i], mod, ace);
        if (r < 0) { imprimir_em(ERROS, "touch: %s: %s\n", argv[i], nome_erro(r)); erros++; }
    }
    return erros ? 1 : 0;
}
