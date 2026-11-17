// SPDX-License-Identifier: GPL-2.0-or-later
/* su - abre un shell con otra cuenta, root por defecto (D255, mejora 098).
 *
 *   su [USUARIO]          shell interactivo (sagsh) de esa cuenta
 *   su [USUARIO] -c ORDEN  solo esa orden
 *
 * Sin setuid: la contraseña la comprueba el gestor de servicios y el kernel
 * cambia la identidad de este proceso, que lanza el shell con la misma entrada
 * y salida. Al salir del shell se vuelve al usuario de antes (el shell de
 * antes nunca dejó de serlo). */
#include "sagui.h"
#include "contas.h"

static const char *AYUDA =
    "uso: su [USUARIO] [-c ORDEN]\n"
    "Abre un shell como USUARIO (root si no se dice) después de pedir su contraseña.\n"
    "Con -c, ejecuta solo esa orden. «exit» vuelve al usuario de antes.\n"
    "La primera vez, si root aún no tiene contraseña, se elige (solo en la sesión local).\n";

int principal(int argc, char **argv)
{
    const char *nome = "root", *ordem = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--ayuda") == 0 || compara_texto(argv[i], "--help") == 0 ||
            compara_texto(argv[i], "-h") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(argv[i], "-c") == 0 && i + 1 < argc) ordem = argv[++i];
        else if (compara_texto(argv[i], "-") == 0 || compara_texto(argv[i], "-l") == 0) continue;
        else if (argv[i][0] == '-') { imprimir_em(ERROS, "su: opción desconocida %s\n%s", argv[i], AYUDA); return 2; }
        else nome = argv[i];
    }
    conta_t c;
    if (!conta_por_nome(nome, &c)) { imprimir_em(ERROS, "su: no existe el usuario «%s»\n", nome); return 1; }
    if (elevar_interativo(c.uid, "su") < 0) return 1;
    /* D371 (P2): «su» a root es hacerse root del todo: el árbol real y /root, sin la vista de la cuenta de antes. */
    if (c.uid == UID_ROOT) usuario_op(USU_CASA_SOLTAR, 0, 0);
    char *envp[64];
    int ne = 0;
    char user[48], home[80];
    formatar_texto(user, sizeof user, "USER=%s", c.nome);
    /* D371: una cuenta ve su carpeta como /home (el kernel le da la suya al pasar a ella); HOME=/home/ana sería
     * /home/ana/ana. */
    formatar_texto(home, sizeof home, "HOME=%s", c.uid == UID_ROOT ? "/root" : "/home");
    for (int i = 0; environ && environ[i] && ne < 60; i++)
        if (!comeca_com(environ[i], "USER=") && !comeca_com(environ[i], "HOME=")) envp[ne++] = environ[i];
    envp[ne++] = user;
    envp[ne++] = home;
    envp[ne] = NULL;
    char *av_c[] = { "sagsh", "-c", (char *)ordem, NULL };
    char *av_i[] = { "sagsh", NULL };
    if (!ordem) imprimir_em(ERROS, "Ahora eres %s. «exit» para volver.\n", c.nome);
    int pid = executar_env("/bin/sagsh", ordem ? av_c : av_i, envp);
    if (pid < 0) { imprimir_em(ERROS, "su: no se pudo lanzar sagsh: %s\n", nome_erro(pid)); return 1; }
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo;
}
