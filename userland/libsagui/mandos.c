// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos.c - mandos y joysticks en la libsagui (D307). Ver mandos.h. */
#include "mandos.h"
#include "conf_maquina.h"

static int llamar(info_mandos_t *i) { return info_sistema(INFO_MANDOS, i, sizeof *i); }

int mandos_listar(info_mando_t *l, int max, unsigned *geracao)
{
    static info_mandos_t i;   /* ~900 bytes: fuera de la pila de los hilos de audio de SDL */
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_LISTA;
    int r = llamar(&i);
    if (r < 0) return r;
    int n = (int)i.n;
    if (n > max) n = max;
    for (int k = 0; k < n; k++) l[k] = i.lista[k];
    if (geracao) *geracao = i.geracao;
    return n;
}

int mando_ler_crudo(int indice, estado_mando_t *e, unsigned short crudo[4])
{
    static info_mandos_t i;
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_ESTADO;
    i.indice = (unsigned)indice;
    int r = llamar(&i);
    if (r < 0) return r;
    *e = i.estado;
    if (crudo) for (int k = 0; k < 4; k++) crudo[k] = i.crudo[k];
    return 0;
}

int mando_ler(int indice, estado_mando_t *e) { return mando_ler_crudo(indice, e, NULL); }

int mandos_detectar(void)
{
    static info_mandos_t i;
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_DETECTAR;
    return llamar(&i);
}

int mandos_calibracao(unsigned short cal[4][3], int *manual)
{
    static info_mandos_t i;
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_CALIBRAR;
    int r = llamar(&i);
    if (r < 0) return r;
    for (int e = 0; e < 4; e++) for (int k = 0; k < 3; k++) cal[e][k] = i.cal[e][k];
    if (manual) *manual = (int)i.calibrado;
    return 0;
}

int mandos_calibrar(const unsigned short cal[4][3])
{
    static info_mandos_t i;
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_CALIBRAR;
    i.escribir = cal ? 1 : 2;
    if (cal) for (int e = 0; e < 4; e++) for (int k = 0; k < 3; k++) i.cal[e][k] = cal[e][k];
    return llamar(&i);
}

int mandos_inyectar(int indice, const void *informe, unsigned n)
{
    static info_mandos_t i;
    if (n > sizeof i.informe) return ERRO_INVALIDO;
    memset(&i, 0, sizeof i);
    i.orden = MANDOS_INYECTAR;
    i.indice = (unsigned)indice;
    i.informe_n = n;
    memcpy(i.informe, informe, n);
    return llamar(&i);
}

const char *mando_nome_tipo(unsigned tipo)
{
    switch (tipo) {
    case MANDO_TIPO_HID: return "USB HID";
    case MANDO_TIPO_XBOX360: return "Xbox 360";
    case MANDO_TIPO_PUERTO: return "puerto de juegos";
    case MANDO_TIPO_PRUEBA: return "prueba";
    }
    return "?";
}

int mandos_cal_texto(const unsigned short cal[4][3], char *s, unsigned max)
{
    return formatar_texto(s, max, "%u,%u,%u/%u,%u,%u/%u,%u,%u/%u,%u,%u",
                          cal[0][0], cal[0][1], cal[0][2], cal[1][0], cal[1][1], cal[1][2],
                          cal[2][0], cal[2][1], cal[2][2], cal[3][0], cal[3][1], cal[3][2]);
}

int mandos_cal_de_texto(const char *s, unsigned short cal[4][3])
{
    for (int e = 0; e < 4; e++) {
        for (int k = 0; k < 3; k++) {
            if (*s < '0' || *s > '9') return ERRO_INVALIDO;
            unsigned v = 0;
            while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s - '0'); if (v > 65535) return ERRO_INVALIDO; s++; }
            cal[e][k] = (unsigned short)v;
            char sep = k < 2 ? ',' : e < 3 ? '/' : 0;
            if (*s != sep) return ERRO_INVALIDO;
            if (sep) s++;
        }
    }
    return 0;
}

/* ------------------------------------------------------------ sagui.conf (clave mando_puerto) */

#define MANDOS_DIR_CONF "/home/.config"
#define MANDOS_ARQ_CONF "/home/.config/sagui.conf"
#define CHAVE "mando_puerto="

static bool home_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

int mandos_conf_ler(unsigned short cal[4][3])
{
    static char t[2048];
    int fd = abrir(maquina_separada() ? MAQ_ARQ_CONF : MANDOS_ARQ_CONF, ABRIR_LER);   /* D371: es de la máquina */
    if (fd < 0) return -1;
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return -1;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        size_t l = tam_texto(p);
        while (l && (p[l - 1] == '\r' || p[l - 1] == ' ')) p[--l] = 0;
        if (comeca_com(p, CHAVE)) return mandos_cal_de_texto(p + tam_texto(CHAVE), cal);
        if (!fim) break;
        p = fim + 1;
    }
    return -1;
}

int mandos_conf_gravar(const unsigned short cal[4][3])
{
    static char t[2048], o[2200];
    int n = 0, no = 0;
    if (maquina_separada()) {   /* D371: la calibración es de la máquina: /etc/sistema.conf (administrador) */
        char v[96] = "", l[128];
        if (cal) mandos_cal_texto(cal, v, sizeof v);
        formatar_texto(l, sizeof l, CHAVE "%s\n", v);
        return maquina_gravar_linhas(l, "guardar la calibración del mando");
    }
    if (!home_persistente()) return 1;
    int fd = abrir(MANDOS_ARQ_CONF, ABRIR_LER);
    if (fd >= 0) { n = ler(fd, t, sizeof t - 1); fechar(fd); if (n < 0) n = 0; }
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        if (*p && !comeca_com(p, CHAVE)) no += formatar_texto(o + no, sizeof o - (unsigned)no, "%s\n", p);
        if (!fim) break;
        p = fim + 1;
    }
    if (cal) {
        char v[96];
        mandos_cal_texto(cal, v, sizeof v);
        no += formatar_texto(o + no, sizeof o - (unsigned)no, CHAVE "%s\n", v);
    }
    criar_pasta(MANDOS_DIR_CONF);
    fd = abrir(MANDOS_ARQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, o, (unsigned)no);
    fechar(fd);
    return r < 0 ? r : 0;
}
