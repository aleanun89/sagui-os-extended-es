// SPDX-License-Identifier: GPL-2.0-or-later
/* admin - ejecuta una orden como administrador (root) (D255, mejora 098).
 *
 *   admin ORDEN [ARGUMENTOS...]
 *
 * No hay setuid: admin pide al gestor de servicios que compruebe la contraseña
 * de root; si es buena, el kernel convierte a este mismo proceso en root, que
 * ejecuta la orden con sagsh -c y con la misma entrada y salida (el mismo
 * terminal). La primera vez, si root aún no tiene contraseña, la elige quien
 * esté en la sesión local. */
#include "sagui.h"
#include "contas.h"

static const char *AYUDA =
    "uso: admin ORDEN [ARGUMENTOS...]\n"
    "Ejecuta la orden como administrador (root) después de pedir la contraseña de root.\n"
    "  admin montar usb0 /datos     admin servicio iniciar sshd     admin particiones\n"
    "La primera vez, si root aún no tiene contraseña, se elige (solo en la sesión local).\n"
    "Para un shell de root: su. Para cambiar la contraseña: passwd root.\n";

/* Añade "a" a la línea, entre comillas simples si hace falta (como lo lee sagsh). */
static bool juntar(char *linha, int max, const char *a)
{
    int n = (int)tam_texto(linha);
    bool comillas = !*a;
    for (const char *p = a; *p; p++)
        if (*p == ' ' || *p == '\t' || *p == '\'' || *p == '"' || *p == '|' || *p == ';' || *p == '&' ||
            *p == '<' || *p == '>' || *p == '$' || *p == '*' || *p == '?' || *p == '\\' || *p == '#') comillas = true;
    if (n && n < max - 1) linha[n++] = ' ';
    if (comillas && n < max - 1) linha[n++] = '\'';
    for (const char *p = a; *p; p++) {
        if (*p == '\'') {   /* ' -> '\'' */
            if (n + 4 >= max) return false;
            linha[n++] = '\''; linha[n++] = '\\'; linha[n++] = '\''; linha[n++] = '\'';
        } else {
            if (n + 1 >= max) return false;
            linha[n++] = *p;
        }
    }
    if (comillas) { if (n + 1 >= max) return false; linha[n++] = '\''; }
    linha[n] = 0;
    return true;
}

int principal(int argc, char **argv)
{
    if (argc < 2 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0 ||
        compara_texto(argv[1], "-h") == 0) {
        escrever_texto(argc < 2 ? ERROS : SAIDA, AYUDA);
        return argc < 2 ? 2 : 0;
    }
    char linha[1024] = "";
    /* Una sola palabra con espacios ("admin 'a && b'") va tal cual: es una línea de sagsh. */
    if (argc == 2) copia_texto(linha, argv[1], sizeof linha);
    else
        for (int i = 1; i < argc; i++)
            if (!juntar(linha, sizeof linha, argv[i])) { imprimir_em(ERROS, "admin: la orden es demasiado larga\n"); return 2; }
    int r = elevar_interativo(UID_ROOT, "admin");
    if (r < 0) return 1;
    /* El entorno de quien lo pidió, con USER=root. */
    char *envp[64];
    int ne = 0;
    static char user[] = "USER=root";
    for (int i = 0; environ && environ[i] && ne < 62; i++)
        if (!comeca_com(environ[i], "USER=")) envp[ne++] = environ[i];
    envp[ne++] = user;
    envp[ne] = NULL;
    char *av[] = { "sagsh", "-c", linha, NULL };
    int pid = executar_env("/bin/sagsh", av, envp);
    if (pid < 0) { imprimir_em(ERROS, "admin: no se pudo lanzar sagsh: %s\n", nome_erro(pid)); return 1; }
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo;
}
