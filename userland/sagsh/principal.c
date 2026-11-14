// SPDX-License-Identifier: GPL-3.0-only
/* principal.c - opciones, variables iniciales y el bucle de lectura.
 *
 *   sagsh                      interactivo (si la entrada es un terminal)
 *   sagsh script.sh args...    ejecuta un script ($0 = script, $1... = args)
 *   sagsh -c 'órdenes' [nombre [args...]]
 *   sagsh -s args...           lee las órdenes de la entrada
 *
 * En el modo interactivo cada línea leída va a un buffer; si el comando
 * quedó abierto (comillas, if sin fi, "|" al final, here-doc), el shell muestra
 * el PS2 ("> ") y une la siguiente línea antes de ejecutar. */
#include "sagsh.h"
#include "contas.h"

static int ler_linha_fd(char **saida)
{
    texto_t t = {0};
    bool algum = false;
    for (;;) {
        char c;
        int r = ler(ENTRADA, &c, 1);
        if (r <= 0) break;
        algum = true;
        if (c == '\n') break;
        tx_car(&t, c);
    }
    if (!algum) { tx_liberar(&t); return -1; }
    *saida = tx_texto(&t);
    return t.n;
}

void sair_do_shell(int codigo)
{
    rodar_trap_saida();
    if (sh.interativo) escrever_texto(SAIDA, "sagsh: ¡hasta luego!\n");
    sair(codigo);
}

static void variaveis_iniciais(void)
{
    for (char **e = environ; e && *e; e++) {
        char *igual = acha_char(*e, '=');
        if (!igual) continue;
        char *nome = sh_dup_n(*e, (int)(igual - *e));
        if (eh_nome(nome)) var_definir(nome, igual + 1, V_EXPORTADA);
        libera(nome);
    }
    if (!var_obter("PATH")) var_definir("PATH", SAGUI_PATH_PADRAO, V_EXPORTADA);   /* D194: también /apps/bin */
    if (!var_obter("HOME")) var_definir("HOME", "/", V_EXPORTADA);
    if (!var_obter("SHELL")) var_definir("SHELL", "/bin/sagsh", V_EXPORTADA);
    char pasta[256];
    if (pasta_atual(pasta, sizeof pasta) >= 0) var_definir("PWD", pasta, V_EXPORTADA);
    if (!var_obter("PS1")) var_definir("PS1", sh_raiz_visivel() ? "root:\\w\\$ " : "sagui:\\w\\$ ", 0);
    char euid[16];
    formatar_texto(euid, sizeof euid, "%u", meu_uid());   /* como EUID de bash */
    var_definir("EUID", euid, 0);
    var_definir("SAGSH_RAIZ", sh_raiz_visivel() ? "1" : "0", 0);   /* sagshrc lo usa para el color del prompt */
    if (!var_obter("PS2")) var_definir("PS2", "> ", 0);
    var_definir("IFS", " \t\n", 0);
    char buf[16];
    const char *nivel = var_obter("SHLVL");
    formatar_texto(buf, sizeof buf, "%d", (nivel ? para_int(nivel) : 0) + 1);
    var_definir("SHLVL", buf, V_EXPORTADA);
}

/* D263: título de la ventana (OSC 0) en cada prompt: «[root] usuario@saguios:carpeta» si el shell es
 * root, y sin «[root]» si no. El Terminal lo lee para marcar la ventana (franja roja); en SSH el
 * cliente lo muestra como título. Solo con TERM: la consola de texto y el serie no lo reciben. */
static void titulo_ventana(void)
{
    char n[32], pasta[256];
    if (pasta_atual(pasta, sizeof pasta) < 0) copia_texto(pasta, "?", sizeof pasta);
    texto_t t = {0};
    tx_junta(&t, "\x1b]0;");
    if (sh_raiz_visivel()) tx_junta(&t, "[root] ");
    tx_formatar(&t, "%s@saguios:%s\a", conta_nome(meu_uid(), n, sizeof n), pasta);
    escrever_texto(SAIDA, t.s);
    tx_liberar(&t);
}

/* Bucle de lectura de la entrada (terminal o pipe). */
static int laco_interativo(void)
{
    texto_t buf = {0};
    int linha = 1;
    for (;;) {
        recolher_trabalhos(sh.interativo);
        if (sh.interativo) {
            sh.fluxo = F_NORMAL;
            sh.base_ctrl_c = interrupcoes();
        }
        char *l = NULL;
        int n;
        if (sh.interativo) {
            if (sh_tem_term() && !buf.n) titulo_ventana();
            char *ps = expandir_prompt(var_obter(buf.n ? "PS2" : "PS1") ? var_obter(buf.n ? "PS2" : "PS1") : "");
            n = ler_linha_editada(ps, &l);
            libera(ps);
            if (n == -2) { buf.n = 0; continue; }   /* Ctrl+C: abandona el comando */
        } else {
            n = ler_linha_fd(&l);
        }
        if (n < 0) {
            if (buf.n) {
                resultado_t r;
                memset(&r, 0, sizeof r);
                r.linha_erro = linha;
                relatar_sintaxe(&r);
                sh.status = 2;
            }
            break;
        }
        tx_junta(&buf, l);
        tx_car(&buf, '\n');
        libera(l);
        int pos = 0;
        for (;;) {
            arena_t *ar = arena_nova();
            resultado_t r = analisar(ar, buf.s, pos, linha, false);
            if (r.estado == A_VAZIO) {
                for (int i = pos; i < buf.n; i++) if (buf.s[i] == '\n') linha++;
                arena_liberar(ar);
                buf.n = 0;
                break;
            }
            if (r.estado == A_INCOMPLETO) {   /* falta texto: quita lo que ya se ejecutó y pide más */
                arena_liberar(ar);
                for (int i = 0; i < pos; i++) if (buf.s[i] == '\n') linha++;
                memmove(buf.s, buf.s + pos, (size_t)(buf.n - pos));
                buf.n -= pos;
                buf.s[buf.n] = 0;
                break;
            }
            if (r.estado == A_ERRO) {
                relatar_sintaxe(&r);
                arena_liberar(ar);
                for (int i = pos; i < buf.n; i++) if (buf.s[i] == '\n') linha++;
                buf.n = 0;
                sh.status = 2;
                if (!sh.interativo) { tx_liberar(&buf); return 2; }
                break;
            }
            for (int i = pos; i < r.fim; i++) if (buf.s[i] == '\n') linha++;
            pos = r.fim;
            if (!sh.opt_n) executar_no(r.no);
            arena_liberar(ar);
            if (sh.fluxo == F_SAIR) { tx_liberar(&buf); return sh.status; }
            if (sh.fluxo == F_ABORTAR && !sh.interativo) { tx_liberar(&buf); return sh.status; }
            if (sh.fluxo == F_INTERROMPIDO || sh.fluxo == F_ABORTAR) {
                if (sh.fluxo == F_INTERROMPIDO) sh.status = 130;
                sh.fluxo = F_NORMAL;
                buf.n = 0;
                break;
            }
            sh.fluxo = F_NORMAL;   /* break/return sueltos en el nivel de arriba */
            if (!sh.interativo) recolher_trabalhos(false);
        }
    }
    tx_liberar(&buf);
    return sh.status;
}

int principal(int argc, char **argv)
{
    sh.pilha_base = __builtin_frame_address(0);
    sh.pid = meu_pid();
    sh.nome0 = argc > 0 && argv[0] ? argv[0] : "sagsh";
    const char *comando = NULL, *script = NULL;
    char *texto_temp = NULL;
    bool forcar_interativo = false, ler_entrada = false;
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "-c") == 0 && i + 1 < argc) { comando = argv[++i]; i++; break; }
        if (compara_texto(a, "--temporario") == 0 && i + 1 < argc) {
            int e;
            texto_temp = ler_arquivo_texto(argv[++i], &e);
            apagar(argv[i]);
            if (!texto_temp) { erro("%s: %s", argv[i], nome_erro(e)); return 126; }
            comando = texto_temp;
            i++;
            break;
        }
        if (compara_texto(a, "--") == 0 || compara_texto(a, "-") == 0) { i++; break; }
        if (compara_texto(a, "--version") == 0) { imprimir("sagsh 2.0 (SaguiOS-Extended)\n"); return 0; }
        for (const char *c = a + 1; *c; c++) {
            switch (*c) {
            case 's': ler_entrada = true; break;
            case 'i': forcar_interativo = true; break;
            case 'e': sh.opt_e = true; break;
            case 'u': sh.opt_u = true; break;
            case 'x': sh.opt_x = true; break;
            case 'f': sh.opt_f = true; break;
            case 'n': sh.opt_n = true; break;
            case 'a': sh.opt_a = true; break;
            default:
                erro("-%c: opción no válida (uso: sagsh [-c órdenes | script] [argumentos])", *c);
                return 2;
            }
        }
    }
    if (comando) {
        if (i < argc) sh.nome0 = argv[i++];
    } else if (i < argc && !ler_entrada) {
        script = argv[i++];
        sh.nome0 = (char *)script;
    }
    params_definir(argv + i, argc - i);
    sh.terminal = tipo_descritor(ENTRADA) == TIPO_TERMINAL;
    sh.interativo = forcar_interativo || (!comando && !script && sh.terminal);
    variaveis_iniciais();

    int st;
    if (comando) {
        st = executar_texto(comando, NULL);
    } else if (script) {
        int e;
        char *texto = ler_arquivo_texto(script, &e);
        if (!texto) {
            erro("%s: %s", script, nome_erro(e));
            sair(e == ERRO_NAO_EXISTE ? 127 : 126);
        }
        sh.script = (char *)script;
        st = executar_texto(texto, script);
        libera(texto);
    } else {
        if (sh.interativo) {
            definir_grupo(0, 0);   /* el shell lidera su propio grupo; cada trabajo tendrá el suyo */
            imprimir("sagsh: el shell de SaguiOS (proceso %d). Escribe ajuda para ver los comandos.\n", meu_pid());
            historico_carregar();
            info_arquivo_t inf;
            if (info_arquivo("/etc/sagshrc", &inf) == 0) {
                char *a[] = { ".", "/etc/sagshrc", NULL };
                interno_achar(".")(2, a);
                sh.fluxo = F_NORMAL;
            }
        }
        st = laco_interativo();
    }
    if (sh.fluxo == F_SAIR || sh.fluxo == F_INTERROMPIDO || sh.fluxo == F_ABORTAR) st = sh.status;
    sair_do_shell(st);
}
