// SPDX-License-Identifier: GPL-2.0-or-later
/* passwd - cambia la contraseña de una cuenta (D255, mejora 098).
 *
 *   passwd           la de tu cuenta
 *   passwd USUARIO   la de otra (p. ej. «passwd root»): hace falta su contraseña
 *                    actual, o ser root
 *
 * La comprueba y la guarda el gestor de servicios (la sombra solo la lee
 * root). Si la cuenta aún no tiene contraseña, desde la sesión local basta con
 * elegir la nueva. */
#include "sagui.h"
#include "contas.h"

static const char *AYUDA =
    "uso: passwd [USUARIO]\n"
    "Cambia la contraseña de tu cuenta o la de USUARIO (p. ej. passwd root).\n"
    "Hace falta la contraseña actual de esa cuenta, salvo si eres root o si aún no tiene\n"
    "(entonces solo desde la sesión local, no por SSH). Al menos 4 caracteres.\n";

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "--help") == 0 ||
                     compara_texto(argv[1], "-h") == 0)) { escrever_texto(SAIDA, AYUDA); return 0; }
    if (argc > 2) { escrever_texto(ERROS, AYUDA); return 2; }
    conta_t c;
    if (argc == 2 ? !conta_por_nome(argv[1], &c) : !conta_por_uid(meu_uid(), &c)) {
        imprimir_em(ERROS, "passwd: no existe la cuenta «%s»\n", argc == 2 ? argv[1] : "(la tuya)");
        return 1;
    }
    pedido_conta_t p;
    memset(&p, 0, sizeof p);
    p.tipo = PED_SENHA;
    p.uid_alvo = c.uid;
    imprimir_em(ERROS, "Cambiando la contraseña de %s.\n", c.nome);
    if (!sou_root()) {
        /* ¿Tiene contraseña? (pregunta sin contraseña: no cuenta como fallo) */
        int r = pedir_conta(&p);
        if (r == ERRO_ACESSO) {
            char q[64];
            formatar_texto(q, sizeof q, "Contraseña actual de %s: ", c.nome);
            if (ler_senha(q, p.senha, sizeof p.senha) < 0) return 1;
        } else if (r == ERRO_SEM_SENHA) {
            if (usuario_op(USU_ESTADO, 0, 0) & USU_EST_SERVICIO) {
                imprimir_em(ERROS, "passwd: %s aún no tiene contraseña y solo se puede fijar desde la sesión local\n", c.nome);
                return 1;
            }
        } else if (r != 1) {
            imprimir_em(ERROS, "passwd: %s\n", r == ERRO_NAO_EXISTE ? "el gestor de servicios no está en marcha" : nome_erro(r));
            return 1;
        }
    }
    char otra[PED_SENHA_MAX];
    for (int i = 0;; i++) {
        if (i == 3) { memset(&p, 0, sizeof p); return 1; }
        if (ler_senha("Contraseña nueva: ", p.senha_nova, sizeof p.senha_nova) < 0) { memset(&p, 0, sizeof p); return 1; }
        if (tam_texto(p.senha_nova) < 4) { imprimir_em(ERROS, "Demasiado corta (al menos 4 caracteres).\n"); continue; }
        if (ler_senha("Repítela: ", otra, sizeof otra) < 0) { memset(&p, 0, sizeof p); return 1; }
        bool igual = compara_texto(otra, p.senha_nova) == 0;
        memset(otra, 0, sizeof otra);
        if (igual) break;
        imprimir_em(ERROS, "No coinciden.\n");
    }
    int r = pedir_conta(&p);
    memset(&p, 0, sizeof p);
    if (r == 1) { imprimir("passwd: contraseña de %s cambiada\n", c.nome); return 0; }
    imprimir_em(ERROS, "passwd: no se cambió: %s\n", r == ERRO_ACESSO ? "contraseña actual incorrecta" : nome_erro(r));
    return 1;
}
