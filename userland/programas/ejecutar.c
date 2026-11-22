// SPDX-License-Identifier: GPL-2.0-or-later
/* ejecutar - los diálogos del menú Inicio (D065).
 *
 *   ejecutar               "Ejecutar...": pide una línea de comando y abre el programa
 *   ejecutar --reiniciar   "Reiniciar...": pregunta antes de reiniciar la PC
 *   ejecutar --apagar      "Apagar...": pregunta antes de apagar la PC (D113)
 *   ejecutar --cerrar-sesion     "Cerrar sesión...": pregunta y vuelve a la pantalla de inicio de sesión (D340)
 *   ejecutar --cambiar-usuario   "Cambiar de usuario...": lo mismo, empezando en otra cuenta
 *
 * El kernel no dibuja cajas de texto ni botones (eso es de la libsagui, en el
 * anillo 3); por eso el menú Inicio llama a este programa en vez de preguntar
 * él mismo. El comando se ejecuta directo, sin shell: es para abrir programas de
 * /bin (editor /sobre.txt, visor /imagenes, pantalla --ventana...). Los
 * comandos de texto (ls, cat...) se ejecutarían sin terminal; para ellos está el
 * Terminal. */
#include "sagui.h"
#include "gui.h"
#include "sonidos.h"

#define MAX_ARGS 16
#define CAMINHO_MAX 128

static int reiniciar_pc(void)
{
    if (!confirmar("Reiniciar", "¿Reiniciar el ordenador ahora?\nLo que no esté guardado se perderá."))
        return 0;
    registrar("ejecutar: reiniciando a pedido do menu Inicio");
    sonido_evento_esperar("reiniciar", SON_APAGADO_MAX_MS);   /* D421: suena antes, con plazo; sin sonido no espera nada */
    reiniciar();
    return 0;
}

static int apagar_pc(void)
{
    if (!confirmar("Apagar", "¿Apagar el ordenador ahora?\nLo que no esté guardado se perderá."))
        return 0;
    registrar("ejecutar: apagando a pedido do menu Inicio");
    sonido_evento_esperar("apagar", SON_APAGADO_MAX_MS);   /* D421 */
    desligar();
    return 0;
}

/* D340: cerrar la sesión (o cambiar de usuario) cierra todos los programas de la sesión. */
static int sair_sessao(int modo)
{
    const char *tit = modo == USU_SAIR_CAMBIAR ? _("Cambiar de usuario") : _("Cerrar sesión");
    if (!confirmar(tit, _("Se cerrarán todos los programas de esta sesión.\nLo que no esté guardado se perderá. ¿Seguir?")))
        return 0;
    int r = usuario_op(USU_CERRAR_SESION, modo, 0);
    registrar("ejecutar: %s -> %d", modo == USU_SAIR_CAMBIAR ? "cambiar de usuario" : "cerrar sesion", r);
    if (r < 0) mensagem(tit, nome_erro(r));
    return 0;
}

/* Divide la línea en palabras (en el lugar), con "comillas" para nombres con espacio. */
static int dividir(char *linha, char **argv)
{
    int n = 0;
    char *p = linha;
    while (*p && n < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '"') {
            argv[n++] = ++p;
            while (*p && *p != '"') p++;
        } else {
            argv[n++] = p;
            while (*p && *p != ' ') p++;
        }
        if (*p) *p++ = 0;
    }
    argv[n] = NULL;
    return n;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "--reiniciar") == 0) return reiniciar_pc();
    if (argc > 1 && compara_texto(argv[1], "--apagar") == 0) return apagar_pc();
    if (argc > 1 && compara_texto(argv[1], "--cerrar-sesion") == 0) return sair_sessao(USU_SAIR_CERRAR);
    if (argc > 1 && compara_texto(argv[1], "--cambiar-usuario") == 0) return sair_sessao(USU_SAIR_CAMBIAR);
    static char linha[160], copia[160], caminho[CAMINHO_MAX];
    for (;;) {
        if (!pedir_texto("Ejecutar", "Escribe el nombre de un programa y sus\nargumentos (por ejemplo: editor /sobre.txt).",
                         linha, sizeof linha))
            return 0;
        copia_texto(copia, linha, sizeof copia);
        char *args[MAX_ARGS + 1];
        if (dividir(copia, args) == 0) continue;
        /* Sin barra en el nombre, el programa se busca en /bin y luego en /apps/bin
         * (las aplicaciones de la partición SAGUI-APPS, D194), como en el shell. */
        info_arquivo_t info;
        if (acha_char(args[0], '/')) copia_texto(caminho, args[0], sizeof caminho);
        else {
            formatar_texto(caminho, sizeof caminho, "/bin/%s", args[0]);
            char alt[CAMINHO_MAX];
            formatar_texto(alt, sizeof alt, "/apps/bin/%s", args[0]);
            if (info_arquivo(caminho, &info) < 0 && info_arquivo(alt, &info) == 0) copia_texto(caminho, alt, sizeof caminho);
        }
        if (info_arquivo(caminho, &info) < 0) {
            char t[200];
            formatar_texto(t, sizeof t, "No se encontró el programa\n%s", caminho);
            mensagem("Ejecutar", t);
            continue;
        }
        int pid = executar(caminho, args);
        registrar("ejecutar: %s (pid %d)", caminho, pid);
        if (pid < 0) {
            char t[200];
            formatar_texto(t, sizeof t, "No se pudo ejecutar %s:\n%s", caminho, nome_erro(pid));
            mensagem("Ejecutar", t);
            continue;
        }
        return 0;
    }
}
