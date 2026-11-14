// SPDX-License-Identifier: GPL-2.0-or-later
/* mktemp - crea un archivo o carpeta temporal con nombre único y escribe su ruta (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: mktemp [-d] [-u] [-p CARPETA] [PLANTILLA]\n"
    "  Crea un archivo vacío (o una carpeta con -d) con nombre único y escribe su ruta.\n"
    "  PLANTILLA termina en XXXXXX, que se sustituye por letras y números al azar (por defecto tmp.XXXXXXXXXX en /tmp).\n"
    "  -p C  crea en la carpeta C   -u  solo escribe el nombre, sin crear nada   -t  se acepta (la plantilla va a /tmp)\n";

int principal(int argc, char **argv)
{
    bool carpeta = false, seco = false;
    const char *dir = NULL;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "dutp:q", &o)) >= 0) {
        switch (r) {
        case 'd': carpeta = true; break;
        case 'u': seco = true; break;
        case 't': case 'q': break;
        case 'p': dir = o.valor; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("mktemp", r, &o);
        }
    }
    const char *plantilla = o.indice < argc ? argv[o.indice] : "tmp.XXXXXXXXXX";
    char nombre[200];
    if (acha_char(plantilla, '/') && !dir) copia_texto(nombre, plantilla, sizeof nombre);
    else caminho_juntar(nombre, sizeof nombre, dir ? dir : "/tmp", plantilla);
    int n = (int)tam_texto(nombre), x = 0;
    while (x < n && nombre[n - 1 - x] == 'X') x++;
    if (x < 3) { imprimir_em(ERROS, "mktemp: la plantilla debe terminar en al menos 3 X (mktemp --help)\n"); return 1; }
    static const char alfa[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int intento = 0; intento < 20; intento++) {
        unsigned char az[32];
        bytes_aleatorios(az, (unsigned)x);
        for (int i = 0; i < x; i++) nombre[n - x + i] = alfa[az[i] % 62];
        info_arquivo_t inf;
        if (info_arquivo(nombre, &inf) == 0) continue;
        if (seco) { saida_linha(nombre, -1); return 0; }
        int e;
        if (carpeta) e = criar_pasta(nombre);
        else { e = abrir(nombre, ABRIR_ESCREVER | ABRIR_CRIAR); if (e >= 0) { fechar(e); e = 0; } }
        if (e < 0) { imprimir_em(ERROS, "mktemp: no se puede crear %s: %s\n", nombre, nome_erro(e)); return 1; }
        saida_linha(nombre, -1);
        return 0;
    }
    imprimir_em(ERROS, "mktemp: no se encontró un nombre libre\n");
    return 1;
}
