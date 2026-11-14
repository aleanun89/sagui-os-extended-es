// SPDX-License-Identifier: GPL-2.0-or-later
/* ia - cliente de chat com IA na linha de comando (D134).
 *
 *   ia "pergunta"          pergunta ao proveedor configurado e escreve a
 *                           resposta a medida que chega (streaming)
 *   ia -m modelo "..."     escolhe o modelo
 *   ia --url URL "..."     escolhe o endereco (por omissao, IA_URL)
 *   ia --modelos           lista os modelos do servidor
 *   ia --provar            teste de conexao (o mesmo que --modelos)
 *   ia --config            mostra a configuracao (sem mostrar a chave)
 *   ia --ajuda             esta ajuda
 *
 * A resposta chega em pedacos (SSE) e escreve-se na saida assim que cada
 * pedaco chega, sem esperar pelo fim. Se a resposta for cortada a meio
 * (Ctrl+C, ou o servidor fecha), o que ja veio fica escrito e o programa
 * avisa que a resposta ficou incompleta.
 *
 * A configuracao vem do ambiente (IA_URL, IA_MODELO, IA_CHAVE, IA_SISTEMA)
 * ou de /home/.config/ia/provedor (e a chave em /home/.config/ia/chave).
 * A chave nunca se escreve no repositorio nem na imagem. */
#include "ia.h"

static const char *AJUDA =
    "uso: ia [opcoes] \"pergunta\"\n"
    "  -m, --modelo MODELO   modelo a usar\n"
    "  --url URL             endereco base (http://host:porta/v1)\n"
    "  --sistema TEXTO       mensagem de sistema\n"
    "  --modelos             lista os modelos do servidor\n"
    "  --provar              teste de conexao\n"
    "  --limite N            para a resposta depois de N pedacos (como o\n"
    "                        botao \"Detener\", para probar que se respeita)\n"
    "  -k, --inseguro         no HTTPS, aceita qualquer certificado\n"
    "  --config              mostra a configuracao\n"
    "  --ajuda               esta ajuda\n"
    "\n"
    "A configuracao tambem vem do ambiente: IA_URL, IA_MODELO, IA_CHAVE,\n"
    "IA_SISTEMA, ou do ficheiro /home/.config/ia/provedor.\n";

/* "Detener" no terminal: o Ctrl+C (onde o terminal conta as interrupcoes) e
 * o limite de pedacos ("--limite N"), que corta a resposta depois de N
 * pedacos. O `ia` ve o contador de interrupcoes entre pedacos: quando sobe,
 * para de ler e fecha a ligacao. */
static int ctrl_c_antes;
static int limite;          /* 0 = sem limite */
static int pedacos;         /* pedacos ja escritos */

/* O que se vai escrevendo na saida (por pedaco). */
static void escrever_pedaco(const char *t, int n, void *ctx)
{
    (void)ctx;
    escrever_tudo(SAIDA, t, (unsigned)n);
    pedacos++;
}

static void fim(int completa, void *ctx)
{
    (void)ctx;
    if (!completa) escrever_texto(SAIDA, "\n[resposta interrompida]\n");
}

static int deve_parar(void *ctx)
{
    (void)ctx;
    if (limite > 0 && pedacos >= limite) return 1;
    return interrupcoes() != ctrl_c_antes;
}

static int uso(void)
{
    imprimir_em(ERROS, "%s", AJUDA);
    return 2;
}

int principal(int argc, char **argv)
{
    ia_config_t cfg;
    ia_config_carregar(&cfg);
    /* Cada programa usa el modelo que le asignó la ventana de Configuración (D145). */
    ia_config_escolher(&cfg, IA_USO_TERMINAL);

    const char *pergunta = NULL, *modelo = NULL, *url = NULL, *sistema = NULL;
    bool listar = false, provar = false, ver_config = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ajuda") == 0 || compara_texto(a, "-h") == 0) { imprimir("%s", AJUDA); return 0; }
        else if (compara_texto(a, "--modelos") == 0) listar = true;
        else if (compara_texto(a, "--provar") == 0) provar = true;
        else if (compara_texto(a, "--config") == 0) ver_config = true;
        else if ((compara_texto(a, "-m") == 0 || compara_texto(a, "--modelo") == 0) && i + 1 < argc) modelo = argv[++i];
        else if (compara_texto(a, "--url") == 0 && i + 1 < argc) url = argv[++i];
        else if (compara_texto(a, "--sistema") == 0 && i + 1 < argc) sistema = argv[++i];
        else if (compara_texto(a, "--limite") == 0 && i + 1 < argc) limite = texto_para_int(argv[++i]);
        else if (compara_texto(a, "-k") == 0 || compara_texto(a, "--inseguro") == 0) cfg.inseguro = true;
        else if (a[0] == '-' && a[1]) return uso();
        else {
            if (pergunta) {
                /* Varias palavras: junta-as com espaco. */
                static char junto[4096];
                copia_texto(junto, pergunta, sizeof junto);
                junta_texto(junto, " ", sizeof junto);
                junta_texto(junto, a, sizeof junto);
                pergunta = junto;
            } else pergunta = a;
        }
    }
    if (modelo) copia_texto(cfg.modelo, modelo, sizeof cfg.modelo);
    if (url) copia_texto(cfg.url, url, sizeof cfg.url);
    if (sistema) copia_texto(cfg.sistema, sistema, sizeof cfg.sistema);

    if (ver_config) {
        imprimir("URL:    %s\n", cfg.url[0] ? cfg.url : "(sem endereco)");
        imprimir("Modelo: %s\n", cfg.modelo[0] ? cfg.modelo : "(sem modelo)");
        imprimir("Chave:  %s\n", cfg.chave[0] ? "definida (nao se mostra)" : "(sem chave)");
        return 0;
    }

    if (listar || provar) {
        if (!cfg.url[0]) {
            imprimir_em(ERROS, "ia: sem endereco. Defina IA_URL ou use --url http://host:porta/v1\n");
            return 2;
        }
        char (*nomes)[64] = aloca(sizeof *nomes * IA_NOMES_MAX);
        if (!nomes) return 1;
        char erro[256];
        int n = ia_modelos(&cfg, nomes, IA_NOMES_MAX, erro, sizeof erro);
        if (n < 0) {
            imprimir_em(ERROS, "ia: %s\n", erro);
            return 1;
        }
        if (provar) {
            imprimir("Conexao correcta com %s (%d modelos)\n", cfg.url, n);
            return 0;
        }
        for (int i = 0; i < n; i++) imprimir("%s\n", nomes[i]);
        return 0;
    }

    if (!pergunta) return uso();
    if (!cfg.url[0]) {
        imprimir_em(ERROS, "ia: sem endereco. Defina IA_URL ou use --url http://host:porta/v1\n");
        return 2;
    }
    /* "Detener" com o Ctrl+C: o `ia` sai do grupo de primeiro plano (para
     * que o kernel nao o mate) e ve o contador de interrupcoes entre
     * pedacos. Assim a resposta a meio fica escrita e avisa-se. */
    ctrl_c_antes = interrupcoes();
    int grupo_antes = primeiro_plano(0);
    char erro[256];
    int r = ia_chat(&cfg, pergunta, escrever_pedaco, NULL, fim, NULL, deve_parar, NULL, erro, sizeof erro);
    primeiro_plano(grupo_antes);
    if (r < 0) {
        imprimir_em(ERROS, "ia: %s\n", erro);
        return 1;
    }
    return 0;
}
