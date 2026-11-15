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
#include "agentes.h"

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
    "  --agente NOMBRE       usa un agente con herramientas (D295): lee, busca y, con tu\n"
    "                        permiso en cada accion, escribe o ejecuta. Ver docs/agentes-ia.md\n"
    "  --pasos N             con --agente: limite de pasos (por omision, el del agente)\n"
    "  --agentes             lista los agentes disponibles\n"
    "  --registro            muestra el registro de lo que hicieron los agentes\n"
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

/* ---- D295: agentes. El texto del modelo llega en UTF-8 y la terminal espera Latin-1; un carácter
 * partido entre dos pedazos se guarda para el siguiente. ---- */
static char cola_utf8[4];
static int n_cola;

static void agente_texto(const char *t, int n, void *ctx)
{
    (void)ctx;
    char buf[1100];
    while (n > 0) {
        int parte = n > 512 ? 512 : n;
        char tmp[520];
        int k = n_cola;
        memcpy(tmp, cola_utf8, (size_t)n_cola);
        memcpy(tmp + k, t, (size_t)parte);
        k += parte;
        t += parte;
        n -= parte;
        /* si acaba a medias un carácter UTF-8, se queda para la próxima */
        int corte = k;
        for (int b = 1; b <= 3 && b <= k; b++) {
            unsigned char c = (unsigned char)tmp[k - b];
            if ((c & 0xC0) == 0x80) continue;
            int largo = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            if (largo > b) corte = k - b;
            break;
        }
        n_cola = k - corte;
        memcpy(cola_utf8, tmp + corte, (size_t)n_cola);
        int m = ia_utf8_latin1(tmp, corte, buf, sizeof buf);
        /* Revisión de seguridad (D295): el texto del modelo no puede llevar secuencias de escape al
         * terminal (borrar líneas, mover el cursor, letra del color del fondo) que falseen la pregunta. */
        for (int i = 0; i < m; i++) {
            unsigned char c = (unsigned char)buf[i];
            if ((c < 0x20 && c != '\n' && c != '\t') || c == 0x7F || (c >= 0x80 && c < 0xA0)) buf[i] = '?';
        }
        escrever_tudo(SAIDA, buf, (unsigned)m);
        pedacos++;
    }
}

static void agente_passo(int num, const char *herr, const char *args, void *ctx)
{
    (void)ctx;
    imprimir_em(ERROS, "\n[paso %d] %s %s\n", num, herr, args);
}

static void agente_resultado(int num, const char *herr, const char *res, int estado, void *ctx)
{
    (void)ctx; (void)herr;
    char corto[161];
    int n = 0;
    for (; res[n] && n < 160; n++) corto[n] = res[n] == '\n' ? ' ' : res[n];
    corto[n] = 0;
    imprimir_em(ERROS, "[resultado %d: %s] %s%s\n", num, estado == 0 ? "ok" : estado == 2 ? "denegado" : "error", corto, res[n] ? "..." : "");
}

/* Pide permiso en el terminal: solo si hay una persona delante (la entrada es el terminal). */
static int agente_confirmar(const char *herr, const char *desc, const char *aviso, bool puede_siempre, void *ctx)
{
    (void)ctx;
    /* Revisión de seguridad (D295): con escrever_tudo, no con imprimir_em, que formatea en 512 bytes y
     * callaba el resto: lo que pasara de ahí en un diff o una orden se hacía sin haberse enseñado. */
    imprimir_em(ERROS, "\n=== El agente pide permiso: %s ===\n", herr);
    escrever_tudo(ERROS, desc, (unsigned)tam_texto(desc));
    escrever_texto(ERROS, "\n");
    if (aviso[0]) { escrever_texto(ERROS, "ATENCION: "); escrever_tudo(ERROS, aviso, (unsigned)tam_texto(aviso)); escrever_texto(ERROS, "\n"); }
    if (tipo_descritor(ENTRADA) != TIPO_TERMINAL) {
        imprimir_em(ERROS, "(no hay terminal para preguntar: denegado)\n");
        return AG_NEGAR;
    }
    imprimir_em(ERROS, "Permitir? [u] una vez  %s[n] denegar: ", puede_siempre ? "[s] siempre en esta sesion  " : "");
    char r[16];
    int n = ler(ENTRADA, r, sizeof r - 1);
    if (n <= 0) return AG_NEGAR;
    r[n] = 0;
    int c = 0;
    while (r[c] == ' ') c++;
    if (r[c] == 'u' || r[c] == 'U') return AG_UNA_VEZ;
    if ((r[c] == 's' || r[c] == 'S') && puede_siempre) return AG_SIEMPRE;
    return AG_NEGAR;
}

static int agente_parar(void *ctx) { (void)ctx; return interrupcoes() != ctrl_c_antes; }

static int pasos_arg;   /* --pasos N: límite de pasos del agente */

static int correr_agente(ia_config_t *cfg, const char *nome, const char *tarea)
{
    ag_def_t *def = aloca(sizeof *def);
    if (!def) return 1;
    if (ag_def_buscar(nome, def) < 0) {
        imprimir_em(ERROS, "ia: no existe el agente «%s» (ia --agentes los lista)\n", nome);
        return 2;
    }
    if (pasos_arg > 0) def->passos = pasos_arg > AG_PASSOS_MAX ? AG_PASSOS_MAX : pasos_arg;
    ag_sesion_t ses;
    memset(&ses, 0, sizeof ses);
    ag_ui_t ui = { agente_texto, agente_passo, agente_resultado, agente_confirmar, agente_parar, NULL };
    ag_resultado_t res;
    char erro[256];
    ctrl_c_antes = interrupcoes();
    int grupo_antes = primeiro_plano(0);
    int r = ag_correr(cfg, def, NULL, &ses, tarea, &ui, NULL, &res, erro, sizeof erro);
    primeiro_plano(grupo_antes);
    escrever_texto(SAIDA, "\n");
    if (r < 0) imprimir_em(ERROS, "ia: %s\n", erro);
    if (res.sin_herramientas) imprimir_em(ERROS, "ia: este modelo no admite herramientas: se contesto como chat normal\n");
    if (res.limite_passos) imprimir_em(ERROS, "ia: el agente llego al limite de %d pasos y se detuvo\n", def->passos);
    if (res.limite_tiempo) imprimir_em(ERROS, "ia: el agente llego al limite de tiempo y se detuvo\n");
    if (res.cancelado) imprimir_em(ERROS, "ia: agente interrumpido\n");
    libera(def);
    return r < 0 ? 1 : 0;
}

static int listar_agentes(void)
{
    ag_def_t *v = aloca(sizeof *v * 32);
    if (!v) return 1;
    int n = ag_def_listar(v, 32);
    for (int i = 0; i < n; i++) imprimir("%-26s %s%s\n", v[i].nome, v[i].descricao, v[i].de_serie ? "" : " (tuyo)");
    libera(v);
    return 0;
}

static int ver_registro(void)
{
    int fd = abrir(ag_caminho_registro(), ABRIR_LER);
    if (fd < 0) { imprimir("(el registro esta vacio)\n"); return 0; }
    char b[512];
    int n;
    while ((n = ler(fd, b, sizeof b)) > 0) escrever_tudo(SAIDA, b, (unsigned)n);
    fechar(fd);
    return 0;
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
    const char *agente = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ajuda") == 0 || compara_texto(a, "-h") == 0) { imprimir("%s", AJUDA); return 0; }
        else if (compara_texto(a, "--modelos") == 0) listar = true;
        else if (compara_texto(a, "--provar") == 0) provar = true;
        else if (compara_texto(a, "--config") == 0) ver_config = true;
        else if (compara_texto(a, "--agentes") == 0) return listar_agentes();
        else if (compara_texto(a, "--registro") == 0) return ver_registro();
        else if (compara_texto(a, "--agente") == 0 && i + 1 < argc) agente = argv[++i];
        else if (compara_texto(a, "--pasos") == 0 && i + 1 < argc) pasos_arg = texto_para_int(argv[++i]);
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
    if (agente) return correr_agente(&cfg, agente, pergunta);
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
