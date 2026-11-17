// SPDX-License-Identifier: GPL-2.0-or-later
/* login - inicio de sesión en la consola de texto (D371, fase 6, P12).
 *
 * Sin escritorio (imágenes --console, máquinas sin modo gráfico), con la configuración guardada (SAGUI-CONF) y sin
 * el inicio automático, el kernel lanza «login» como root en vez del shell. Pide la cuenta y su contraseña,
 * la comprueba él mismo con la sombra (es root: no hace falta el gestor de servicios, que en la consola puede no
 * estar), pasa a esa cuenta (el kernel le da su carpeta en /home) y abre su shell. Al salir del shell, el
 * kernel vuelve a lanzar «login».
 *
 * Cuentas: las de persona (uid 1000-59999 con shell) y root. Una cuenta sin contraseña entra sin preguntar,
 * salvo root: sin contraseña no entra (se elige con «su» desde la cuenta).
 * Cada fallo tarda 2 s; tras 5 seguidos, un minuto sin poder. Nunca se escribe la contraseña en ningún sitio. */
#include "sagui.h"
#include "contas.h"
#include "pbkdf2.h"
#include "conf_maquina.h"

#define FALHAS_MAX 5

static void ler_linha_eco(const char *pergunta, char *buf, int max)
{
    escrever_texto(SAIDA, pergunta);
    int n = 0;
    for (;;) {
        char c;
        int r = ler(ENTRADA, &c, 1);
        if (r <= 0) break;
        if (c == '\n' || c == '\r') break;
        if ((c == 8 || c == 127)) { if (n > 0) n--; continue; }
        if (n < max - 1 && (unsigned char)c >= 32) buf[n++] = c;
    }
    buf[n] = 0;
}

int principal(int argc, char **argv)
{
    /* «login --inicio-automatico si|no»: la opción del sistema (P5), como administrador. */
    if (argc == 3 && compara_texto(argv[1], "--inicio-automatico") == 0) {
        bool si = compara_texto(argv[2], "si") == 0 || compara_texto(argv[2], "sí") == 0;
        if (!si && compara_texto(argv[2], "no") != 0) { imprimir_em(ERROS, "uso: login --inicio-automatico si|no\n"); return 2; }
        if (!maquina_separada()) { imprimir_em(ERROS, "login: sin la configuración guardada (en vivo) se entra siempre solo\n"); return 1; }
        int r = maquina_gravar_linhas(si ? "inicio_automatico=si\n" : "inicio_automatico=no\n", "cambiar el inicio automático");
        if (r < 0) { imprimir_em(ERROS, "login: no se guardó: %s\n", r == ERRO_ACESSO ? "hace falta ser administrador (admin login ...)" : nome_erro(r)); return 1; }
        imprimir("Inicio automático: %s (vale al reiniciar).\n", si ? "sí" : "no");
        return 0;
    }
    if (!sou_root()) { imprimir_em(ERROS, "login: lo lanza el sistema (como root)\n"); return 1; }
    char host[48] = "saguios";
    int fh = abrir("/etc/hostname", ABRIR_LER);
    if (fh >= 0) { int n = ler(fh, host, sizeof host - 1); fechar(fh); host[n > 0 ? n : 0] = 0; char *nl = acha_char(host, '\n'); if (nl) *nl = 0; }
    int falhas = 0;
    for (;;) {
        char nome[CONTA_NOME_MAX + 8];
        imprimir("\n%s - SaguiOS\n", host);
        ler_linha_eco("Cuenta: ", nome, sizeof nome);
        if (!nome[0]) continue;
        conta_t c;
        bool existe = conta_por_nome(nome, &c) && (c.uid == UID_ROOT || (c.uid >= UID_SAGUI && c.uid < 60000 && c.shell[0]));
        char h[SOMBRA_TEXTO_MAX] = "";
        int sh = existe ? sombra_ler(c.nome, h, sizeof h) : 0;
        bool bloqueada = sh > 0 && compara_texto(h, "!") == 0;
        bool tem = sh > 0 && h[0] && !bloqueada;
        bool ok;
        if (existe && c.uid == UID_ROOT && !tem) {
            /* D371 (P3 b): root sin contraseña no entra sin ella por la consola (sería un shell de root para
             * quien llegue). Se entra con la cuenta y se elige con «su» (sin escritorio lo admite el gestor). */
            memset(h, 0, sizeof h);
            registrar("login: root sin contraseña: no entra por la consola");
            imprimir("root aún no tiene contraseña: entra con tu cuenta y elígela con «su».\n");
            continue;
        }
        if (existe && !bloqueada && !tem) {
            ok = true;   /* sin contraseña: entra (como en la pantalla de inicio de sesión) */
        } else {
            char senha[PED_SENHA_MAX];
            if (ler_senha("Contraseña: ", senha, sizeof senha) < 0) senha[0] = 0;
            ok = existe && tem && pbkdf2_conferir(senha, h);
            memset(senha, 0, sizeof senha);
        }
        memset(h, 0, sizeof h);
        if (!ok) {
            registrar("login: entrada negada para «%s»", nome);
            dormir(2000);
            imprimir("Cuenta o contraseña incorrectas.\n");
            if (++falhas >= FALHAS_MAX) {
                imprimir("Demasiados fallos: espera un minuto.\n");
                dormir(60000);
                falhas = 0;
            }
            continue;
        }
        falhas = 0;
        registrar("login: entra %s (uid %u) en la consola", c.nome, c.uid);
        if (c.uid != UID_ROOT && usuario_op(USU_FIJAR, (int)c.uid, (int)c.gid) < 0) {
            imprimir_em(ERROS, "login: no se pudo pasar a la cuenta %s\n", c.nome);
            continue;
        }
        char user[48], home[80];
        formatar_texto(user, sizeof user, "USER=%s", c.nome);
        formatar_texto(home, sizeof home, "HOME=%s", c.uid == UID_ROOT ? "/root" : "/home");
        char *envp[] = { user, home, NULL };
        if (c.uid == UID_ROOT) mudar_pasta("/root");
        else mudar_pasta("/home");
        char *av[] = { "sagsh", NULL };
        int pid = executar_env(c.shell[0] ? c.shell : "/bin/sagsh", av, envp);
        if (pid < 0) { imprimir_em(ERROS, "login: no se pudo abrir el shell (%s)\n", nome_erro(pid)); return 1; }
        int codigo = 0;
        esperar(pid, &codigo);
        return 0;   /* el kernel vuelve a lanzar «login» */
    }
}
