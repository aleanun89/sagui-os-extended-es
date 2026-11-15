// SPDX-License-Identifier: GPL-3.0-only
/* internos.c - comandos internos de sagsh.
 *
 * Son internos los que necesitan cambiar el propio shell (cd, export, read,
 * exit...) y los muy usados en scripts, que serían lentos como programa
 * (echo, test, printf, true). Los textos están en español; los nombres
 * en portugués (entrar, onde, eco, limpar, matar, sair) llegan aquí por la
 * tabla de ajuda.c. */
#include "sagsh.h"

static int fontes;   /* profundidad de "source": return vale allí dentro */

static void sai(const char *s) { escrever_saida(SAIDA, s, (int)tam_texto(s)); }

/* ------------------------------------------------------------ simples */

static int i_verdade(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int i_falso(int argc, char **argv) { (void)argc; (void)argv; return 1; }

static int i_cd(int argc, char **argv)
{
    const char *destino = argc > 1 ? argv[1] : var_obter("HOME");
    bool mostrar = false;
    if (!destino || !*destino) destino = "/";
    if (compara_texto(destino, "-") == 0) {
        destino = var_obter("OLDPWD");
        if (!destino) { erro("cd: OLDPWD no está definida"); return 1; }
        mostrar = true;
    }
    char antes[256];
    if (pasta_atual(antes, sizeof antes) < 0) antes[0] = 0;
    char *d = sh_dup(destino);
    int r = mudar_pasta(d);
    if (r < 0) { erro("cd: %s: %s", d, nome_erro(r)); libera(d); return 1; }
    libera(d);
    char agora[256];
    if (pasta_atual(agora, sizeof agora) < 0) agora[0] = 0;
    var_definir("OLDPWD", antes, V_EXPORTADA);
    var_definir("PWD", agora, V_EXPORTADA);
    if (mostrar) { sai(agora); sai("\n"); }
    return 0;
}

static int i_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char p[256];
    if (pasta_atual(p, sizeof p) < 0) return 1;
    sai(p);
    sai("\n");
    return 0;
}

/* D282: la máscara de creación del shell (la heredan los programas que lanza). */
static int i_umask(int argc, char **argv)
{
    if (argc < 2) {
        char t[8];
        int m = mascara_criacao(-1);
        if (m < 0) return 1;
        formatar_texto(t, sizeof t, "%04o\n", (unsigned)m);
        sai(t);
        return 0;
    }
    unsigned m = 0;
    for (const char *p = argv[1]; *p; p++) {
        if (*p < '0' || *p > '7' || m > 0777) { imprimir_em(ERROS, "umask: máscara no válida (en octal: 022, 077)\n"); return 1; }
        m = m * 8 + (unsigned)(*p - '0');
    }
    if (m > 0777) { imprimir_em(ERROS, "umask: máscara no válida (en octal: 022, 077)\n"); return 1; }
    return mascara_criacao((int)m) < 0 ? 1 : 0;
}

static int i_limpar(int argc, char **argv)
{
    (void)argc; (void)argv;
    sai("\x1b[H\x1b[2J");   /* funciona en el terminal de pantalla completa y en el Terminal en ventana */
    return 0;
}

static int i_reiniciar(int argc, char **argv)
{
    (void)argc; (void)argv;
    sai("Reiniciando...\n");
    dormir(200);
    int r = reiniciar();   /* D255: solo vuelve si se negó (servicio o SSH sin ser root) */
    imprimir_em(ERROS, "reiniciar: %s\n", nome_erro(r));
    return 1;
}

/* apagar-pc (D113): apagado ordenado; el kernel graba los discos y apaga por
 * ACPI. Sin ACPI deja el aviso «Ya puedes apagar el ordenador». */
static int i_apagar_pc(int argc, char **argv)
{
    (void)argc; (void)argv;
    sai("Apagando...\n");
    dormir(200);
    int r = desligar();   /* D255: solo vuelve si se negó */
    imprimir_em(ERROS, "apagar-pc: %s\n", nome_erro(r));
    return 1;
}

/* acpi [desactivar|activar]: estado del apagado por ACPI y del botón. */
static int i_acpi(int argc, char **argv)
{
    char t[160];
    if (argc > 1) {
        int op = compara_texto(argv[1], "desactivar") == 0 ? ACPI_OP_DESATIVAR
               : compara_texto(argv[1], "activar") == 0 ? ACPI_OP_ATIVAR : 0;
        if (!op) { sai("uso: acpi [desactivar|activar]\n"); return 2; }
        if (acpi_controle(op) < 0) { sai("acpi: no hay ACPI utilizable (o se desactivó al arrancar)\n"); return 1; }
    }
    static info_energia_t e;
    if (info_sistema(INFO_ENERGIA, &e, sizeof e) < 0) { sai("acpi: sin información\n"); return 1; }
    static const char *const estados[] = { "sin ACPI", "tablas erróneas", "sin \\_S5_ legible", "disponible", "desactivado" };
    formatar_texto(t, sizeof t, "Apagado ACPI: %s", e.estado <= ENERGIA_DESATIVADO ? estados[e.estado] : "?");
    sai(t);
    if (e.origem_s5[0]) { formatar_texto(t, sizeof t, " (S5 = %u/%u, %s)", e.slp_typ_a, e.slp_typ_b, e.origem_s5); sai(t); }
    if (e.motivo[0] && e.estado != ENERGIA_DISPONIVEL && e.estado != ENERGIA_DESATIVADO) {
        formatar_texto(t, sizeof t, ": %s", e.motivo); sai(t);
    }
    formatar_texto(t, sizeof t, "\nBotón de encendido: %s (SCI en la IRQ %u, %u pulsaciones)\n",
                   e.botao ? "atendido" : e.botao_metodo ? "es un dispositivo AML: no atendido" : "no atendido",
                   e.sci, e.pulsacoes);
    sai(t);
    return 0;
}

/* bateria [-v] (D192): la batería del portátil, leída por el ACPI (uACPI). */
static void tempo_bat(char *t, int max, int min)
{
    if (min < 0) formatar_texto(t, max, "-");
    else if (min >= 60) formatar_texto(t, max, "%d h %02d min", min / 60, min % 60);
    else formatar_texto(t, max, "%d min", min);
}

static int i_bateria(int argc, char **argv)
{
    bool detalhe = argc > 1 && (compara_texto(argv[1], "-v") == 0 || compara_texto(argv[1], "--detalles") == 0);
    if (argc > 1 && !detalhe) { sai("uso: bateria [-v]\n"); return 2; }
    static info_bateria_t b;
    char t[160], tm[24];
    if (info_sistema(INFO_BATERIA, &b, sizeof b) < 0) { sai("bateria: el sistema no informa de la batería\n"); return 1; }
    if (!b.presente) {
        if (b.uacpi == UACPI_DESLIGADO)
            formatar_texto(t, sizeof t, "Batería: no disponible (%s)\n", b.motivo[0] ? b.motivo : "sin ACPI");
        else if (b.uacpi == UACPI_INICIANDO)
            formatar_texto(t, sizeof t, "Batería: leyendo el ACPI, prueba dentro de unos segundos\n");
        else if (b.uacpi == UACPI_FALHOU)
            formatar_texto(t, sizeof t, "Batería: no disponible (el intérprete ACPI no pudo cargar: %s)\n", b.motivo);
        else if (!b.baterias)
            formatar_texto(t, sizeof t, "No hay batería (el ACPI de este equipo no describe ninguna)\n");
        else
            formatar_texto(t, sizeof t, "No hay batería puesta\n");
        sai(t);
        if (b.adaptador) { sai(b.adaptador == 2 ? "Cargador: enchufado\n" : "Cargador: desenchufado\n"); }
        return 1;
    }
    bool carga = (b.estado & BATERIA_CARREGANDO) != 0;
    const char *est = carga ? "cargando" : (b.estado & BATERIA_DESCARREGANDO) ? "descargando"
                    : b.adaptador == 2 ? "cargada, enchufado" : "en reposo";
    if (b.porcentagem >= 0) formatar_texto(t, sizeof t, "Batería: %d %% (%s%s)\n", b.porcentagem, est,
                                           (b.estado & BATERIA_CRITICA) ? ", CRÍTICA" : "");
    else formatar_texto(t, sizeof t, "Batería: nivel desconocido (%s)\n", est);
    sai(t);
    tempo_bat(tm, sizeof tm, b.minutos);
    formatar_texto(t, sizeof t, "%s: %s\n", carga ? "Tiempo para llenarse" : "Tiempo restante", tm);
    sai(t);
    const char *u = b.unidade_ma ? "mAh" : "mWh";
    if (b.restante != BATERIA_DESCONHECIDO && b.total != BATERIA_DESCONHECIDO) {
        formatar_texto(t, sizeof t, "Capacidad: %u de %u %s", b.restante, b.total, u);
        sai(t);
        if (b.projeto != BATERIA_DESCONHECIDO) { formatar_texto(t, sizeof t, " (diseño %u %s)", b.projeto, u); sai(t); }
        sai("\n");
    }
    if (b.tensao_mv != BATERIA_DESCONHECIDO) {
        formatar_texto(t, sizeof t, "Tensión: %u.%u V\n", b.tensao_mv / 1000, (b.tensao_mv % 1000) / 100);
        sai(t);
    }
    if (b.taxa != BATERIA_DESCONHECIDO) {
        formatar_texto(t, sizeof t, "%s: %u %s\n", carga ? "Corriente de carga" : "Consumo", b.taxa, b.unidade_ma ? "mA" : "mW");
        sai(t);
    }
    formatar_texto(t, sizeof t, "Cargador: %s\n", b.adaptador == 2 ? "enchufado" : b.adaptador == 1 ? "desenchufado" : "no se sabe");
    sai(t);
    if (b.modelo[0] || b.fabricante[0]) {
        formatar_texto(t, sizeof t, "Modelo: %s (%s%s%s%s%s)\n", b.modelo[0] ? b.modelo : "?",
                       b.fabricante[0] ? b.fabricante : "fabricante ?", b.tipo[0] ? ", " : "", b.tipo,
                       b.serie[0] ? ", serie " : "", b.serie);
        sai(t);
    }
    if (detalhe) {
        if (b.ciclos != BATERIA_DESCONHECIDO) { formatar_texto(t, sizeof t, "Ciclos de carga: %u\n", b.ciclos); sai(t); }
        formatar_texto(t, sizeof t, "Aviso de la BIOS: %u %s; batería baja: %u %s\n", b.aviso, u, b.baixo, u);
        sai(t);
        formatar_texto(t, sizeof t, "uACPI: activo; EC: %s (%u eventos); lecturas %u; avisos %u; hace %u ms\n",
                       b.ec ? "sí" : "no", b.consultas_ec, b.leituras, b.notificacoes, b.idade_ms);
        sai(t);
    }
    return 0;
}

/* ------------------------------------------------------------ echo y printf */

static int escape(const char *s, texto_t *t, bool *parar)
{
    /* s apunta al carácter después de la "\"; devuelve cuántos consumió */
    switch (s[0]) {
    case 'n': tx_car(t, '\n'); return 1;
    case 't': tx_car(t, '\t'); return 1;
    case 'r': tx_car(t, '\r'); return 1;
    case 'a': tx_car(t, 7); return 1;
    case 'b': tx_car(t, '\b'); return 1;
    case 'f': tx_car(t, '\f'); return 1;
    case 'v': tx_car(t, '\v'); return 1;
    case 'e': case 'E': tx_car(t, 27); return 1;
    case '\\': tx_car(t, '\\'); return 1;
    case 'c': if (parar) *parar = true; return 1;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
        int v = 0, k = 0;
        if (s[0] == '0') k = 1;   /* \0nnn */
        int ini = k;
        while (k < ini + 3 && s[k] >= '0' && s[k] <= '7') v = v * 8 + (s[k++] - '0');
        tx_car(t, (char)v);
        return k ? k : 1;
    }
    case 'x': {
        int v = 0, k = 1;
        while (k < 3) {
            char c = s[k];
            int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if (d < 0) break;
            v = v * 16 + d;
            k++;
        }
        if (k == 1) { tx_junta(t, "\\x"); return 1; }
        tx_car(t, (char)v);
        return k;
    }
    case 0: tx_car(t, '\\'); return 0;
    default: tx_car(t, '\\'); tx_car(t, s[0]); return 1;
    }
}

static int i_echo(int argc, char **argv)
{
    bool nova_linha = true, escapes = false;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        bool ok = true;
        for (const char *c = argv[i] + 1; *c; c++)
            if (*c != 'n' && *c != 'e' && *c != 'E') ok = false;
        if (!ok) break;
        for (const char *c = argv[i] + 1; *c; c++) {
            if (*c == 'n') nova_linha = false;
            else if (*c == 'e') escapes = true;
            else escapes = false;
        }
    }
    texto_t t = {0};
    bool parar = false;
    int primeiro = i;
    for (; i < argc && !parar; i++) {
        if (i > primeiro) tx_car(&t, ' ');
        if (!escapes) { tx_junta(&t, argv[i]); continue; }
        for (const char *c = argv[i]; *c && !parar; c++) {
            if (*c == '\\') c += escape(c + 1, &t, &parar);
            else tx_car(&t, *c);
        }
    }
    if (nova_linha && !parar) tx_car(&t, '\n');
    int r = t.n ? escrever_saida(SAIDA, t.s, t.n) : 0;
    tx_liberar(&t);
    return r < 0 ? 1 : 0;
}

static void numero_texto(texto_t *t, unsigned v, int base, bool maiuscula)
{
    char buf[16];
    int n = 0;
    const char *dig = maiuscula ? "0123456789ABCDEF" : "0123456789abcdef";
    do { buf[n++] = dig[v % (unsigned)base]; v /= (unsigned)base; } while (v);
    while (n) tx_car(t, buf[--n]);
}

static int valor_printf(const char *a, bool *erro_num)
{
    if (!a) return 0;
    if (a[0] == '\'' || a[0] == '"') return (unsigned char)a[1];
    const char *p = a;
    while (*p == ' ') p++;
    bool neg = false;
    if (*p == '-' || *p == '+') { neg = *p == '-'; p++; }
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (p[0] == '0' && p[1]) base = 8;
    unsigned v = 0;
    bool algum = false;
    for (; *p; p++) {
        int d = (*p >= '0' && *p <= '9') ? *p - '0' : (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10 : (*p >= 'A' && *p <= 'F') ? *p - 'A' + 10 : 99;
        if (d >= base) { *erro_num = true; erro("printf: %s: número no válido", a); break; }
        v = v * (unsigned)base + (unsigned)d;
        algum = true;
    }
    if (!algum && *a) { *erro_num = true; }
    return neg ? -(int)v : (int)v;
}

static int i_printf(int argc, char **argv)
{
    if (argc < 2) { erro("printf: uso: printf formato [argumentos]"); return 2; }
    const char *fmt = argv[1];
    int ai = 2;
    texto_t t = {0};
    bool erro_num = false, parar = false;
    do {
        int usados_antes = ai;
        for (const char *c = fmt; *c && !parar; c++) {
            if (*c == '\\') { c += escape(c + 1, &t, &parar); continue; }
            if (*c != '%') { tx_car(&t, *c); continue; }
            c++;
            if (*c == '%') { tx_car(&t, '%'); continue; }
            bool esq = false, zero = false, mais = false, espaco = false;
            for (;; c++) {
                if (*c == '-') esq = true;
                else if (*c == '0') zero = true;
                else if (*c == '+') mais = true;
                else if (*c == ' ') espaco = true;
                else if (*c == '#') ;
                else break;
            }
            int larg = 0, prec = -1;
            if (*c == '*') { larg = valor_printf(ai < argc ? argv[ai++] : NULL, &erro_num); c++; if (larg < 0) { esq = true; larg = -larg; } }
            else while (*c >= '0' && *c <= '9') larg = larg * 10 + (*c++ - '0');
            if (*c == '.') {
                c++;
                prec = 0;
                if (*c == '*') { prec = valor_printf(ai < argc ? argv[ai++] : NULL, &erro_num); c++; }
                else while (*c >= '0' && *c <= '9') prec = prec * 10 + (*c++ - '0');
            }
            while (*c == 'l' || *c == 'h') c++;
            const char *arg = ai < argc ? argv[ai++] : NULL;
            texto_t campo = {0};
            bool numerico = false;
            switch (*c) {
            case 's':
                tx_junta(&campo, arg ? arg : "");
                if (prec >= 0 && campo.n > prec) { campo.n = prec; campo.s[prec] = 0; }
                break;
            case 'b':
                for (const char *b = arg ? arg : ""; *b && !parar; b++) {
                    if (*b == '\\') b += escape(b + 1, &campo, &parar);
                    else tx_car(&campo, *b);
                }
                break;
            case 'c':
                if (arg && *arg) tx_car(&campo, arg[0]);
                break;
            case 'd': case 'i': {
                numerico = true;
                int v = valor_printf(arg, &erro_num);
                unsigned u = v < 0 ? (unsigned)(-(v + 1)) + 1u : (unsigned)v;
                texto_t dig = {0};
                numero_texto(&dig, u, 10, false);
                while (prec >= 0 && dig.n < prec) { tx_junta(&campo, "0"); prec--; }
                if (v < 0) { texto_t s = {0}; tx_car(&s, '-'); tx_junta(&s, campo.s ? campo.s : ""); tx_junta(&s, dig.s); tx_liberar(&campo); campo = s; }
                else { texto_t s = {0}; if (mais) tx_car(&s, '+'); else if (espaco) tx_car(&s, ' '); tx_junta(&s, campo.s ? campo.s : ""); tx_junta(&s, dig.s); tx_liberar(&campo); campo = s; }
                tx_liberar(&dig);
                break;
            }
            case 'u': case 'x': case 'X': case 'o': {
                numerico = true;
                unsigned v = (unsigned)valor_printf(arg, &erro_num);
                texto_t dig = {0};
                numero_texto(&dig, v, *c == 'o' ? 8 : *c == 'u' ? 10 : 16, *c == 'X');
                while (prec >= 0 && dig.n < prec) { tx_car(&campo, '0'); prec--; }
                tx_junta(&campo, dig.s);
                tx_liberar(&dig);
                break;
            }
            default:
                erro("printf: %%%c: conversión no válida", *c ? *c : ' ');
                tx_liberar(&campo);
                tx_liberar(&t);
                return 1;
            }
            tx_texto(&campo);
            int falta = larg - campo.n;
            if (!esq && falta > 0) {
                if (zero && numerico) {
                    int k = (campo.s[0] == '-' || campo.s[0] == '+' || campo.s[0] == ' ') ? 1 : 0;
                    tx_junta_n(&t, campo.s, k);
                    while (falta-- > 0) tx_car(&t, '0');
                    tx_junta(&t, campo.s + k);
                } else {
                    while (falta-- > 0) tx_car(&t, ' ');
                    tx_junta(&t, campo.s);
                }
            } else {
                tx_junta(&t, campo.s);
                while (falta-- > 0) tx_car(&t, ' ');
            }
            tx_liberar(&campo);
            if (!*c) break;
        }
        if (ai == usados_antes) break;   /* el formato no usa argumentos */
    } while (ai < argc && !parar);
    if (t.n) escrever_saida(SAIDA, t.s, t.n);
    tx_liberar(&t);
    return erro_num ? 1 : 0;
}

/* ------------------------------------------------------------ test y [ */

static bool test_erro;

static bool inteiro(const char *s, int *v)
{
    const char *p = s;
    while (*p == ' ') p++;
    if (!eh_numero(p)) { erro("test: %s: se esperaba un número entero", s); test_erro = true; *v = 0; return false; }
    *v = para_int(p);
    return true;
}

static bool eh_unario(const char *s)
{
    static const char *ops = "efdsrwxznhLtpbcSgGkuO";
    return s[0] == '-' && s[1] && !s[2] && acha_char(ops, s[1]);
}

static bool eh_binario(const char *s)
{
    static const char *bin[] = { "=", "==", "!=", "<", ">", "-eq", "-ne", "-lt", "-le", "-gt", "-ge", "-nt", "-ot", "-ef", NULL };
    for (int i = 0; bin[i]; i++) if (compara_texto(s, bin[i]) == 0) return true;
    return false;
}

static bool unario(const char *op, const char *a)
{
    info_arquivo_t inf;
    switch (op[1]) {
    case 'z': return !*a;
    case 'n': return *a != 0;
    case 't': return tipo_descritor(para_int(a)) == TIPO_TERMINAL;
    case 'p': return info_arquivo(a, &inf) == 0 && inf.tipo == TIPO_PIPE;
    case 'h': case 'L': case 'b': case 'c': case 'S': case 'g': case 'u': case 'k': return false;
    }
    if (info_arquivo(a, &inf) < 0) return false;
    switch (op[1]) {
    case 'd': return inf.tipo == TIPO_PASTA;
    case 'f': return inf.tipo != TIPO_PASTA;
    case 's': return inf.tamanho > 0;
    case 'x': return inf.tipo == TIPO_PROGRAMA || inf.tipo == TIPO_PASTA;
    default: return true;   /* -e -r -w -G -O: existe */
    }
}

static bool binario(const char *a, const char *op, const char *b)
{
    if (compara_texto(op, "=") == 0 || compara_texto(op, "==") == 0) return compara_texto(a, b) == 0;
    if (compara_texto(op, "!=") == 0) return compara_texto(a, b) != 0;
    if (compara_texto(op, "<") == 0) return compara_texto(a, b) < 0;
    if (compara_texto(op, ">") == 0) return compara_texto(a, b) > 0;
    if (op[1] == 'n' && op[2] == 't') return false;
    if (op[1] == 'o' && op[2] == 't') return false;
    if (op[1] == 'e' && op[2] == 'f') return compara_texto(a, b) == 0;
    int x, y;
    if (!inteiro(a, &x) || !inteiro(b, &y)) return false;
    if (op[1] == 'e' && op[2] == 'q') return x == y;
    if (op[1] == 'n') return x != y;
    if (op[1] == 'l' && op[2] == 't') return x < y;
    if (op[1] == 'l') return x <= y;
    if (op[1] == 'g' && op[2] == 't') return x > y;
    return x >= y;
}

typedef struct { char **a; int n, i; } tst_t;
static bool t_ou(tst_t *t);

static bool t_primario(tst_t *t)
{
    if (t->i >= t->n) { erro("test: falta un argumento"); test_erro = true; return false; }
    char *x = t->a[t->i];
    if (compara_texto(x, "!") == 0) { t->i++; return !t_primario(t); }
    if (compara_texto(x, "(") == 0) {
        t->i++;
        bool r = t_ou(t);
        if (t->i < t->n && compara_texto(t->a[t->i], ")") == 0) t->i++;
        else { erro("test: falta ')'"); test_erro = true; }
        return r;
    }
    if (t->i + 2 < t->n + 0 && eh_binario(t->a[t->i + 1])) {
        bool r = binario(x, t->a[t->i + 1], t->a[t->i + 2]);
        t->i += 3;
        return r;
    }
    if (eh_unario(x) && t->i + 1 < t->n) {
        bool r = unario(x, t->a[t->i + 1]);
        t->i += 2;
        return r;
    }
    t->i++;
    return *x != 0;
}

static bool t_e(tst_t *t)
{
    bool r = t_primario(t);
    while (t->i < t->n && compara_texto(t->a[t->i], "-a") == 0) { t->i++; bool d = t_primario(t); r = r && d; }
    return r;
}

static bool t_ou(tst_t *t)
{
    bool r = t_e(t);
    while (t->i < t->n && compara_texto(t->a[t->i], "-o") == 0) { t->i++; bool d = t_e(t); r = r || d; }
    return r;
}

static int avaliar_test(char **a, int n)
{
    test_erro = false;
    bool r;
    if (n == 0) return 1;
    if (n == 1) r = a[0][0] != 0;
    else if (n == 2 && compara_texto(a[0], "!") == 0) r = a[1][0] == 0;
    else if (n == 2 && eh_unario(a[0])) r = unario(a[0], a[1]);
    else if (n == 3 && eh_binario(a[1])) r = binario(a[0], a[1], a[2]);
    else if (n == 3 && compara_texto(a[0], "!") == 0) r = !(avaliar_test(a + 1, 2) == 0);
    else {
        tst_t t = { a, n, 0 };
        r = t_ou(&t);
        if (t.i < n && !test_erro) { erro("test: %s: se esperaba un operador", a[t.i]); test_erro = true; }
    }
    if (test_erro) return 2;
    return r ? 0 : 1;
}

static int i_test(int argc, char **argv) { return avaliar_test(argv + 1, argc - 1); }

static int i_colchete(int argc, char **argv)
{
    if (argc < 2 || compara_texto(argv[argc - 1], "]") != 0) { erro("[: falta ']'"); return 2; }
    return avaliar_test(argv + 1, argc - 2);
}

/* ------------------------------------------------------------ variables */

static int i_export(int argc, char **argv)
{
    int i = 1, st = 0;
    bool tirar = false;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (compara_texto(argv[i], "-n") == 0) tirar = true;
        else if (compara_texto(argv[i], "-p") == 0) ;
        else if (compara_texto(argv[i], "--") == 0) { i++; break; }
        else { erro("export: %s: opción no válida", argv[i]); return 2; }
    }
    if (i >= argc) { var_listar(true); return 0; }
    for (; i < argc; i++) {
        char *igual = acha_char(argv[i], '=');
        char *nome = igual ? sh_dup_n(argv[i], (int)(igual - argv[i])) : sh_dup(argv[i]);
        if (!eh_nome(nome)) { erro("export: '%s': no es un nombre válido", argv[i]); st = 1; }
        else if (tirar) { var_t *v = var_achar(nome); if (v) v->flags &= ~V_EXPORTADA; }
        else if (igual) { if (!var_definir(nome, igual + 1, V_EXPORTADA)) st = 1; }
        else var_exportar(nome);
        libera(nome);
    }
    return st;
}

static int i_readonly(int argc, char **argv)
{
    int i = 1, st = 0;
    if (i < argc && compara_texto(argv[i], "-p") == 0) i++;
    if (i >= argc) {
        var_listar(false);
        return 0;
    }
    for (; i < argc; i++) {
        char *igual = acha_char(argv[i], '=');
        char *nome = igual ? sh_dup_n(argv[i], (int)(igual - argv[i])) : sh_dup(argv[i]);
        if (!eh_nome(nome)) { erro("readonly: '%s': no es un nombre válido", argv[i]); st = 1; }
        else if (igual && !var_definir(nome, igual + 1, V_SO_LEITURA)) st = 1;
        else if (!igual) var_definir(nome, NULL, V_SO_LEITURA);
        libera(nome);
    }
    return st;
}

static int i_unset(int argc, char **argv)
{
    int i = 1;
    bool func = false, so_var = false;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (compara_texto(argv[i], "-f") == 0) func = true;
        else if (compara_texto(argv[i], "-v") == 0) so_var = true;
        else if (compara_texto(argv[i], "--") == 0) { i++; break; }
    }
    for (; i < argc; i++) {
        if (func) { funcao_remover(argv[i]); continue; }
        if (var_achar(argv[i]) || so_var) var_remover(argv[i]);
        else funcao_remover(argv[i]);
    }
    return 0;
}

static int i_local(int argc, char **argv)
{
    if (sh.funcoes == 0) { erro("local: solo se puede usar dentro de una función"); return 1; }
    int st = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        char *igual = acha_char(argv[i], '=');
        char *nome = igual ? sh_dup_n(argv[i], (int)(igual - argv[i])) : sh_dup(argv[i]);
        if (!eh_nome(nome)) { erro("local: '%s': no es un nombre válido", argv[i]); st = 1; libera(nome); continue; }
        var_local(nome);
        if (igual) { if (!var_definir(nome, igual + 1, 0)) st = 1; }
        else if (!var_achar(nome)) var_definir(nome, "", 0);
        libera(nome);
    }
    return st;
}

static bool opcao_longa(const char *nome, bool valor)
{
    if (compara_texto(nome, "errexit") == 0) sh.opt_e = valor;
    else if (compara_texto(nome, "nounset") == 0) sh.opt_u = valor;
    else if (compara_texto(nome, "xtrace") == 0) sh.opt_x = valor;
    else if (compara_texto(nome, "noglob") == 0) sh.opt_f = valor;
    else if (compara_texto(nome, "allexport") == 0) sh.opt_a = valor;
    else if (compara_texto(nome, "noexec") == 0) sh.opt_n = valor;
    else return false;
    return true;
}

static int i_set(int argc, char **argv)
{
    if (argc == 1) { var_listar(false); return 0; }
    int i = 1;
    for (; i < argc; i++) {
        char *a = argv[i];
        if (compara_texto(a, "--") == 0) { i++; params_definir(argv + i, argc - i); return 0; }
        if (compara_texto(a, "-") == 0) { i++; break; }
        if ((a[0] != '-' && a[0] != '+') || !a[1]) break;
        bool liga = a[0] == '-';
        for (char *c = a + 1; *c; c++) {
            switch (*c) {
            case 'e': sh.opt_e = liga; break;
            case 'u': sh.opt_u = liga; break;
            case 'x': sh.opt_x = liga; break;
            case 'f': sh.opt_f = liga; break;
            case 'a': sh.opt_a = liga; break;
            case 'n': sh.opt_n = liga; break;
            case 'm': case 'b': case 'h': case 'C': case 'v': break;   /* aceptadas e ignoradas */
            case 'o':
                if (i + 1 >= argc) {
                    imprimir("errexit\t%s\nnounset\t%s\nxtrace\t%s\nnoglob\t%s\nallexport\t%s\n",
                             sh.opt_e ? "on" : "off", sh.opt_u ? "on" : "off", sh.opt_x ? "on" : "off",
                             sh.opt_f ? "on" : "off", sh.opt_a ? "on" : "off");
                    return 0;
                }
                if (!opcao_longa(argv[++i], liga)) { erro("set: %s: opción no válida", argv[i]); return 2; }
                goto proximo;
            default:
                erro("set: -%c: opción no válida", *c);
                return 2;
            }
        }
    proximo:;
    }
    if (i < argc) params_definir(argv + i, argc - i);
    return 0;
}

static int i_shift(int argc, char **argv)
{
    int n = argc > 1 ? para_int(argv[1]) : 1;
    if (n < 0 || n > params.n) return 1;
    for (int i = 0; i < n; i++) libera(params.v[i]);
    for (int i = n; i <= params.n; i++) params.v[i - n] = params.v[i];
    params.n -= n;
    return 0;
}

static int i_read(int argc, char **argv)
{
    bool cru = false;
    const char *prompt = NULL;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (compara_texto(argv[i], "-r") == 0) cru = true;
        else if (compara_texto(argv[i], "-p") == 0 && i + 1 < argc) prompt = argv[++i];
        else if (compara_texto(argv[i], "--") == 0) { i++; break; }
        else if (compara_texto(argv[i], "-s") == 0) ;
        else { erro("read: %s: opción no válida", argv[i]); return 2; }
    }
    if (prompt) escrever_texto(ERROS, prompt);
    /* lee un byte por vez: no consume lo que viene después de la línea */
    texto_t linha = {0};
    texto_t marca = {0};   /* 1 = escapado con \ (no divide) */
    bool fim = false, algum = false;
    for (;;) {
        char c;
        int r = ler(ENTRADA, &c, 1);
        if (r <= 0) { fim = true; break; }
        algum = true;
        if (c == '\n') break;
        if (c == '\\' && !cru) {
            r = ler(ENTRADA, &c, 1);
            if (r <= 0) { fim = true; break; }
            if (c == '\n') continue;
            tx_car(&linha, c);
            tx_car(&marca, 1);
            continue;
        }
        tx_car(&linha, c);
        tx_car(&marca, 0);
    }
    tx_texto(&linha);
    tx_texto(&marca);
    if (i >= argc) {
        var_definir("REPLY", linha.s, 0);
    } else {
        const char *ifs = var_obter("IFS");
        if (!ifs) ifs = " \t\n";
        int nvars = argc - i, p = 0, n = linha.n;
        #define EH_IFS(k) (!marca.s[k] && *ifs && acha_char(ifs, (unsigned char)linha.s[k]))
        #define EH_BRANCO(k) (EH_IFS(k) && (linha.s[k] == ' ' || linha.s[k] == '\t' || linha.s[k] == '\n'))
        while (p < n && EH_BRANCO(p)) p++;
        for (int v = 0; v < nvars; v++) {
            texto_t campo = {0};
            if (v == nvars - 1) {   /* la última se lleva el resto, sin los blancos del final */
                int e = n;
                while (e > p && EH_BRANCO(e - 1)) e--;
                tx_junta_n(&campo, linha.s + p, e - p);
                p = n;
            } else {
                while (p < n && !EH_IFS(p)) tx_car(&campo, linha.s[p++]);
                while (p < n && EH_BRANCO(p)) p++;
                if (p < n && EH_IFS(p)) { p++; while (p < n && EH_BRANCO(p)) p++; }
            }
            if (!var_definir(argv[i + v], tx_texto(&campo), 0)) { tx_liberar(&campo); tx_liberar(&linha); tx_liberar(&marca); return 2; }
            tx_liberar(&campo);
        }
        #undef EH_IFS
        #undef EH_BRANCO
    }
    tx_liberar(&linha);
    tx_liberar(&marca);
    (void)algum;
    return fim ? 1 : 0;
}

/* ------------------------------------------------------------ flujo */

static int i_exit(int argc, char **argv)
{
    if (argc > 1) {
        if (!eh_numero(argv[1])) { erro("exit: %s: se esperaba un número", argv[1]); sh.status = 2; }
        else sh.status = para_int(argv[1]) & 0xFF;
    }
    sh.fluxo = F_SAIR;
    return sh.status;
}

static int i_return(int argc, char **argv)
{
    if (sh.funcoes == 0 && fontes == 0) { erro("return: solo se puede usar en una función o en un script cargado con source"); return 1; }
    if (argc > 1) sh.status = para_int(argv[1]) & 0xFF;
    sh.fluxo = F_VOLTAR;
    return sh.status;
}

static int laco(int argc, char **argv, int tipo)
{
    int n = argc > 1 ? para_int(argv[1]) : 1;
    if (n < 1) { erro("%s: %s: el número debe ser 1 o mayor", argv[0], argv[1]); return 1; }
    if (sh.lacos == 0) return 0;   /* fuera de un bucle: no hace nada (como bash) */
    if (n > sh.lacos) n = sh.lacos;
    sh.nivel = n;
    sh.fluxo = tipo;
    return 0;
}

static int i_break(int argc, char **argv) { return laco(argc, argv, F_PARAR); }
static int i_continue(int argc, char **argv) { return laco(argc, argv, F_CONTINUAR); }

/* ------------------------------------------------------------ source y eval */

char *ler_arquivo_texto(const char *caminho, int *erro_r)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) { *erro_r = fd; return NULL; }
    texto_t t = {0};
    char buf[512];
    int n;
    while ((n = ler(fd, buf, sizeof buf)) > 0) tx_junta_n(&t, buf, n);
    fechar(fd);
    tx_texto(&t);
    t.n = (int)utf8_para_latin1(t.s, (size_t)t.n, NULL);   /* scripts escritos fuera (UTF-8) */
    t.s[t.n] = 0;
    for (int i = 0; i < t.n; i++) if (!t.s[i]) t.s[i] = ' ';
    *erro_r = 0;
    return t.s;
}

static int i_source(int argc, char **argv)
{
    if (argc < 2) { erro("%s: falta el nombre del archivo", argv[0]); return 2; }
    char *caminho = sh_dup(argv[1]);
    info_arquivo_t inf;
    if (!acha_char(argv[1], '/') && info_arquivo(argv[1], &inf) < 0) {
        char *c = achar_programa(argv[1]);
        if (c) { libera(caminho); caminho = c; }
    }
    int e;
    char *texto = ler_arquivo_texto(caminho, &e);
    if (!texto) { erro("%s: %s: %s", argv[0], argv[1], nome_erro(e)); libera(caminho); return 1; }
    lista_t antigos = {0}, novos = {0};
    bool trocar = argc > 2;
    if (trocar) {
        for (int i = 2; i < argc; i++) lista_poe(&novos, sh_dup(argv[i]));
        params_trocar(&novos, &antigos);
    }
    char *script_antes = sh.script;
    int linha_antes = sh.linha;
    sh.script = caminho;
    fontes++;
    int st = executar_texto(texto, caminho);
    fontes--;
    sh.script = script_antes;
    sh.linha = linha_antes;
    if (sh.fluxo == F_VOLTAR) { sh.fluxo = F_NORMAL; st = sh.status; }
    if (trocar) { lista_liberar(&params); params = antigos; }
    libera(texto);
    libera(caminho);
    return st;
}

static int i_eval(int argc, char **argv)
{
    texto_t t = {0};
    for (int i = 1; i < argc; i++) {
        if (i > 1) tx_car(&t, ' ');
        tx_junta(&t, argv[i]);
    }
    int st = executar_texto(tx_texto(&t), NULL);
    tx_liberar(&t);
    return st;
}

static int i_let(int argc, char **argv)
{
    int v = 0;
    for (int i = 1; i < argc; i++)
        if (!avaliar_aritmetica(argv[i], &v)) return 2;
    return v == 0;
}

/* ------------------------------------------------------------ alias y tipos */

static int i_alias(int argc, char **argv)
{
    if (argc == 1 || (argc == 2 && compara_texto(argv[1], "-p") == 0)) { apelidos_listar(); return 0; }
    int st = 0;
    for (int i = 1; i < argc; i++) {
        char *igual = acha_char(argv[i], '=');
        if (igual) {
            char *nome = sh_dup_n(argv[i], (int)(igual - argv[i]));
            apelido_definir(nome, igual + 1);
            libera(nome);
        } else {
            apelido_t *a = apelido_achar(argv[i]);
            if (!a) { erro("alias: %s: no encontrado", argv[i]); st = 1; continue; }
            texto_t t = {0};
            tx_formatar(&t, "alias %s=", a->nome);
            aspas_simples(&t, a->valor);
            tx_car(&t, '\n');
            sai(t.s);
            tx_liberar(&t);
        }
    }
    return st;
}

static int i_unalias(int argc, char **argv)
{
    int st = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-a") == 0) {
            apelidos_limpar();
            continue;
        }
        if (!apelido_remover(argv[i])) { erro("unalias: %s: no encontrado", argv[i]); st = 1; }
    }
    return st;
}

/* modo 0: type; 1: command -v; 2: command -V */
static int descrever(const char *nome, int modo)
{
    apelido_t *a = apelido_achar(nome);
    texto_t t = {0};
    int st = 0;
    if (a) {
        if (modo == 1) { tx_formatar(&t, "alias %s=", nome); aspas_simples(&t, a->valor); }
        else { tx_formatar(&t, "%s es un alias de ", nome); aspas_simples(&t, a->valor); }
    } else if (eh_palavra_reservada(nome)) {
        if (modo == 1) tx_junta(&t, nome);
        else tx_formatar(&t, "%s es una palabra reservada del shell", nome);
    } else if (funcao_achar(nome)) {
        if (modo == 1) tx_junta(&t, nome);
        else {
            tx_formatar(&t, "%s es una función\n%s () ", nome, nome);
            desanalisar(funcao_achar(nome)->corpo, &t, false);
        }
    } else if (interno_achar(nome)) {
        if (modo == 1) tx_junta(&t, nome);
        else if (compara_texto(nome_canonico(nome), nome) != 0)
            tx_formatar(&t, "%s es un comando interno del shell (alias en portugués de %s)", nome, nome_canonico(nome));
        else tx_formatar(&t, "%s es un comando interno del shell", nome);
    } else {
        char *c = achar_programa(nome);
        info_arquivo_t inf;
        if (c && acha_char(nome, '/') && info_arquivo(c, &inf) < 0) { libera(c); c = NULL; }
        if (!c) {
            if (modo != 1) erro("type: %s: no encontrado", nome);
            tx_liberar(&t);
            return 1;
        }
        if (modo == 1) tx_junta(&t, c);
        else if (compara_texto(nome_canonico(nome), nome) != 0)
            tx_formatar(&t, "%s es %s (alias en portugués de %s)", nome, c, nome_canonico(nome));
        else tx_formatar(&t, "%s es %s", nome, c);
        libera(c);
    }
    tx_car(&t, '\n');
    sai(t.s);
    tx_liberar(&t);
    return st;
}

static int i_type(int argc, char **argv)
{
    int st = 0;
    for (int i = 1; i < argc; i++) if (argv[i][0] != '-') st |= descrever(argv[i], 0);
    return st;
}

static int i_command(int argc, char **argv)
{
    /* "command programa ..." ya se trató en executar.c; aquí solo -v y -V */
    int modo = 0, i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (compara_texto(argv[i], "-v") == 0) modo = 1;
        else if (compara_texto(argv[i], "-V") == 0) modo = 2;
        else if (compara_texto(argv[i], "-p") == 0) ;
        else break;
    }
    if (!modo) return 0;
    int st = 0;
    for (; i < argc; i++) st |= descrever(argv[i], modo);
    return st;
}

/* ------------------------------------------------------------ trabajos */

/* history [N] lista (las últimas N); history -c borra. */
static int i_history(int argc, char **argv)
{
    int n = 0;
    if (argc > 1) {
        if (compara_texto(argv[1], "-c") == 0) { historico_limpar(); return 0; }
        if (!eh_numero(argv[1])) { erro("history: uso: history [N | -c]"); return 2; }
        n = para_int(argv[1]);
    }
    historico_listar(n);
    return 0;
}
static int i_jobs(int argc, char **argv) { (void)argc; (void)argv; listar_trabalhos(); return 0; }
static int i_wait(int argc, char **argv) { return esperar_trabalhos(argc, argv); }
static int i_fg(int argc, char **argv) { return trazer_trabalho(argc, argv); }

static int i_bg(int argc, char **argv)
{
    (void)argc; (void)argv;
    erro("bg: en SaguiOS los trabajos no se detienen: los que lanzas con & ya siguen en segundo plano");
    return 0;
}

/* Las señales de SaguiOS: el kernel solo sabe terminar un proceso, así que todas
 * las señales que "matan" hacen lo mismo. */
static const struct { const char *nome; int num; } sinais[] = {
    { "HUP", 1 }, { "INT", 2 }, { "QUIT", 3 }, { "KILL", 9 }, { "TERM", 15 } };

static int num_sinal(const char *s)
{
    if (comeca_com(s, "SIG")) s += 3;
    if (eh_numero(s)) return para_int(s);
    for (unsigned k = 0; k < sizeof sinais / sizeof sinais[0]; k++)
        if (compara_texto(s, sinais[k].nome) == 0) return sinais[k].num;
    return -1;
}

static bool pid_existe(int pid)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    for (int i = 0; i < n; i++) if (v[i].pid == pid) return true;
    return false;
}

static int i_kill(int argc, char **argv)
{
    if (argc < 2) { erro("kill: uso: kill [-SEÑAL] <pid | %%trabajo>... | kill -l"); return 1; }
    int st = 0, i = 1, sinal = 15;
    if (compara_texto(argv[1], "-l") == 0 || compara_texto(argv[1], "-L") == 0) {
        if (argc > 2) {   /* kill -l 9 -> KILL */
            int v = para_int(argv[2]);
            for (unsigned k = 0; k < sizeof sinais / sizeof sinais[0]; k++)
                if (sinais[k].num == v) { imprimir("%s\n", sinais[k].nome); return 0; }
            erro("kill: %s: señal desconocida", argv[2]);
            return 1;
        }
        for (unsigned k = 0; k < sizeof sinais / sizeof sinais[0]; k++)
            imprimir("%d) SIG%s%s", sinais[k].num, sinais[k].nome, k + 1 == sizeof sinais / sizeof sinais[0] ? "\n" : "  ");
        imprimir("(en SaguiOS todas terminan el proceso; 0 solo comprueba que existe)\n");
        return 0;
    }
    if (compara_texto(argv[1], "-s") == 0 && argc > 2) {
        sinal = num_sinal(argv[2]);
        if (sinal < 0) { erro("kill: %s: señal desconocida (kill -l)", argv[2]); return 1; }
        i = 3;
    } else if (argv[1][0] == '-' && argv[1][1]) {
        sinal = num_sinal(argv[1] + 1);
        if (sinal < 0) { erro("kill: %s: señal desconocida (kill -l)", argv[1] + 1); return 1; }
        i = 2;
    }
    if (i >= argc) { erro("kill: falta el pid o %%trabajo"); return 1; }
    for (; i < argc; i++) {
        int pids[16], n;
        if (argv[i][0] == '%') {
            n = pids_do_trabalho(argv[i], pids, 16);
            if (n < 0) { erro("kill: %s: no existe ese trabajo", argv[i]); st = 1; continue; }
        } else if (eh_numero(argv[i])) {
            pids[0] = para_int(argv[i]);
            n = 1;
        } else {
            erro("kill: %s: se esperaba un pid o %%trabajo", argv[i]);
            st = 1;
            continue;
        }
        for (int k = 0; k < n; k++) {
            if (sinal == 0) {   /* kill -0: ¿existe? */
                if (!pid_existe(pids[k])) { erro("kill: %d: el proceso no existe", pids[k]); st = 1; }
                continue;
            }
            int r = matar(pids[k]);
            if (r < 0) { erro("kill: %d: %s", pids[k], nome_erro(r)); st = 1; }
            else imprimir("Proceso %d terminado.\n", pids[k]);
        }
    }
    return st;
}

/* ------------------------------------------------------------ uso interno */

/* .sagsh_estado pid status ultimo_fundo opciones: el preludio de un sagsh
 * hijo (serializar_estado) pone el $$, $? y $! del shell padre. */
static int i_estado(int argc, char **argv)
{
    if (argc < 5) return 1;
    sh.pid = para_int(argv[1]);
    sh.ultimo_fundo = para_int(argv[3]);
    for (const char *c = argv[4]; *c; c++) {
        if (*c == 'e') sh.opt_e = true;
        else if (*c == 'u') sh.opt_u = true;
        else if (*c == 'x') sh.opt_x = true;
        else if (*c == 'f') sh.opt_f = true;
    }
    return para_int(argv[2]);
}

static char *trap_saida;

static int i_trap(int argc, char **argv)
{
    if (argc == 1) {
        if (trap_saida) {
            texto_t t = {0};
            tx_junta(&t, "trap -- ");
            aspas_simples(&t, trap_saida);
            tx_junta(&t, " EXIT\n");
            sai(t.s);
            tx_liberar(&t);
        }
        return 0;
    }
    if (argc < 3) { erro("trap: uso: trap 'comandos' EXIT  (solo se admite EXIT)"); return 2; }
    for (int i = 2; i < argc; i++) {
        if (compara_texto(argv[i], "EXIT") != 0 && compara_texto(argv[i], "0") != 0) {
            erro("trap: %s: en SaguiOS solo se admite EXIT (no hay señales)", argv[i]);
            continue;
        }
        libera(trap_saida);
        trap_saida = compara_texto(argv[1], "-") == 0 ? NULL : sh_dup(argv[1]);
    }
    return 0;
}

void rodar_trap_saida(void)
{
    if (!trap_saida) return;
    char *t = trap_saida;
    trap_saida = NULL;
    int st = sh.status;
    sh.fluxo = F_NORMAL;
    executar_texto(t, NULL);
    sh.status = st;
    libera(t);
}

static int i_exec(int argc, char **argv) { (void)argc; (void)argv; return 0; }   /* tratado en executar.c */

/* ------------------------------------------------------------ tabla */

typedef struct { const char *nome; interno_f f; bool especial; } interno_t;

static const interno_t internos[] = {
    { ":", i_verdade, true },        { "true", i_verdade, false },   { "false", i_falso, false },
    { "cd", i_cd, false },           { "pwd", i_pwd, false },        { "echo", i_echo, false },
    { "printf", i_printf, false },   { "test", i_test, false },      { "[", i_colchete, false },
    { "export", i_export, true },    { "readonly", i_readonly, true }, { "unset", i_unset, true },
    { "local", i_local, true },      { "set", i_set, true },         { "shift", i_shift, true },
    { "read", i_read, false },       { "exit", i_exit, true },       { "sair", i_exit, true },
    { "return", i_return, true },    { "break", i_break, true },     { "continue", i_continue, true },
    { "source", i_source, true },    { ".", i_source, true },        { "eval", i_eval, true },
    { "let", i_let, false },         { "alias", i_alias, false },    { "unalias", i_unalias, false },
    { "type", i_type, false },       { "command", i_command, false }, { "history", i_history, false },
    { "jobs", i_jobs, false },       { "wait", i_wait, false },      { "fg", i_fg, false },
    { "bg", i_bg, false },           { "kill", i_kill, false },      { "clear", i_limpar, false },
    { "ajuda", interno_ajuda, false }, { "reiniciar", i_reiniciar, false }, { "trap", i_trap, true },
    { "apagar-pc", i_apagar_pc, false }, { "acpi", i_acpi, false }, { "bateria", i_bateria, false },
    { "exec", i_exec, true },        { ".sagsh_estado", i_estado, true },
    { "umask", i_umask, false },
    { NULL, NULL, false }
};

static const interno_t *achar(const char *nome)
{
    for (int i = 0; internos[i].nome; i++)
        if (compara_texto(internos[i].nome, nome) == 0) return &internos[i];
    const char *c = nome_canonico(nome);
    if (c != nome)
        for (int i = 0; internos[i].nome; i++)
            if (compara_texto(internos[i].nome, c) == 0) return &internos[i];
    return NULL;
}

interno_f interno_achar(const char *nome)
{
    const interno_t *i = achar(nome);
    return i ? i->f : NULL;
}

bool interno_especial(const char *nome)
{
    const interno_t *i = achar(nome);
    return i && i->especial;
}

const char *nome_canonico(const char *nome)
{
    const comando_t *c = achar_comando(nome);
    return c ? c->nome : nome;
}

/* Nombres de los internos, para completar con Tab. */
const char *nome_interno(int i) { return internos[i].nome; }
