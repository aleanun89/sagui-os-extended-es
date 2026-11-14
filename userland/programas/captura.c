// SPDX-License-Identifier: GPL-2.0-or-later
/* captura [archivo] [-v|--ventana] [-r x,y,ancho,alto] [-t segundos] (D125)
 *
 * Guarda la pantalla ya compuesta (sin cursor) como PNG. Sin archivo, el nombre es
 * captura-AAAAMMDD-HHMMSS.png en /home/capturas (o /tmp/capturas si /home no deja
 * escribir). -v: sólo la ventana activa con su marco; -r: un rectángulo; -t: espera
 * unos segundos antes (para capturar un menú abierto). Impr Pant lo lanza el
 * gestor de ventanas (atalhos.c); Alt+Impr Pant lo lanza con -v. */
#include "sagui.h"
#include "imagem.h"

static int numero(const char **s)
{
    int n = 0;
    while (**s >= '0' && **s <= '9') { n = n * 10 + (**s - '0'); (*s)++; }
    return n;
}

/* Ajustes de Configuración (D125): carpeta y sonido. Se leen de /tmp/capturas.conf (la sesión) o de
 * /home/.config/sagui.conf. */
static char carpeta_conf[120];
static bool sonido_conf = true;

static void leer_ajustes(void)
{
    static char t[2048];
    static const char *const arq[2] = { "/home/.config/sagui.conf", "/tmp/capturas.conf" };
    for (int a = 0; a < 2; a++) {
        int fd = abrir(arq[a], ABRIR_LER);
        if (fd < 0) continue;
        int n = ler(fd, t, sizeof t - 1);
        fechar(fd);
        if (n <= 0) continue;
        t[n] = 0;
        for (char *p = t; *p;) {
            char *fim = acha_char(p, '\n');
            if (fim) *fim = 0;
            if (comeca_com(p, "capturas_carpeta=")) {
                copia_texto(carpeta_conf, p + 17, sizeof carpeta_conf);
                size_t m = tam_texto(carpeta_conf);
                while (m && carpeta_conf[m - 1] == '\r') carpeta_conf[--m] = 0;
            } else if (comeca_com(p, "capturas_sonido=")) sonido_conf = texto_para_int(p + 16) != 0;
            if (!fim) break;
            p = fim + 1;
        }
    }
}

int principal(int argc, char **argv)
{
    leer_ajustes();
    const char *arquivo = NULL;
    bool janela = false, verificar = false, hex = false;
    int rect[4] = { 0, 0, 0, 0 }, espera = 0;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-v") || !compara_texto(argv[i], "--ventana")) janela = true;
        else if (!compara_texto(argv[i], "--verificar")) verificar = true;
        else if (!compara_texto(argv[i], "--hex")) hex = true;
        else if (!compara_texto(argv[i], "-t") && i + 1 < argc) { const char *p = argv[++i]; espera = numero(&p); }
        else if (!compara_texto(argv[i], "-r") && i + 1 < argc) {
            const char *p = argv[++i];
            for (int k = 0; k < 4; k++) { rect[k] = numero(&p); if (*p == ',') p++; }
            if (rect[2] <= 0 || rect[3] <= 0) { imprimir("captura: -r x,y,ancho,alto\n"); return 1; }
        } else if (argv[i][0] != '-') arquivo = argv[i];
        else { imprimir("uso: captura [archivo] [-v] [-r x,y,ancho,alto] [-t segundos]\n"); return 1; }
    }
    if (espera > 0) dormir((unsigned)espera * 1000);
    int L = 4096, A = 4096;   /* la syscall recorta; el búfer cubre cualquier modo */
    unsigned max = 1920u * 1200u;
    (void)L; (void)A;
    uint32_t *px = aloca(max * 4);
    if (!px) { imprimir("captura: sin memoria\n"); return 1; }
    int n = capturar_tela(janela ? -1 : 0, rect, px, max);
    if (n < 0) { imprimir("captura: no se pudo leer la pantalla\n"); return 1; }
    imagem_t img = { .larg = rect[2], .alt = rect[3], .px = px };

    char nome[160];
    const char *aviso = "";
    if (arquivo) copia_texto(nome, arquivo, sizeof nome);
    else {
        data_t d;
        ler_data(&d);
        char base[64];
        formatar_texto(base, sizeof base, "captura-%04u%02u%02u-%02u%02u%02u.png", d.ano, d.mes, d.dia, d.hora, d.minuto, d.segundo);
        const char *dir = carpeta_conf[0] ? carpeta_conf : "/home/capturas";
        criar_pasta(dir);
        formatar_texto(nome, sizeof nome, "%s/%s", dir, base);
        info_arquivo_t inf;
        if (info_arquivo(dir, &inf) < 0) {
            criar_pasta("/tmp/capturas");
            formatar_texto(nome, sizeof nome, "/tmp/capturas/%s", base);
            aviso = " (temporal)";
        }
    }
    int r = imagem_gravar_png(nome, &img);
    char msg[200];
    if (r < 0 && !arquivo && nome[1] != 't') {   /* la carpeta no deja escribir: probar /tmp */
        criar_pasta("/tmp/capturas");
        char base2[64];
        const char *b = acha_ultimo_char(nome, '/') + 1;
        copia_texto(base2, b, sizeof base2);
        formatar_texto(nome, sizeof nome, "/tmp/capturas/%s", base2);
        aviso = " (temporal)";
        r = imagem_gravar_png(nome, &img);
    }
    if (r < 0) { imprimir("captura: no se pudo guardar\n"); return 1; }
    if (verificar) {   /* lee el archivo con el lector de la libsagui y compara (pruebas) */
        imagem_t lida;
        int e = imagem_ler(nome, &lida);
        uint32_t h = 2166136261u;
        bool igual = e == 0 && lida.larg == img.larg && lida.alt == img.alt;
        for (int i = 0; igual && i < img.larg * img.alt; i++) if (lida.px[i] != (img.px[i] & 0xFFFFFF)) igual = false;
        for (int i = 0; i < img.larg * img.alt; i++) h = (h ^ (img.px[i] & 0xFFFFFF)) * 16777619u;
        char v[100];
        formatar_texto(v, sizeof v, "verificar: %s %dx%d en %d,%d hash %08x\n", igual ? "OK" : "FALLA", img.larg, img.alt, rect[0], rect[1], h);
        imprimir(v);
    }
    formatar_texto(msg, sizeof msg, "Captura guardada en %s%s", nome, aviso);
    aviso_tela(msg, nome);
    if (sonido_conf) { static const nota_t clic[] = { { 1400, 25 } }; tocar(clic, 1, BIP_SUBSTITUIR); }
    imprimir(msg);
    imprimir("\n");
    if (hex) dormir(800);
    if (hex) {   /* el archivo en hexadecimal, para que las pruebas lo lean desde el anfitrión */
        int fd = abrir(nome, ABRIR_LER);
        uint8_t blq[48];
        int k, tot = 0;
        while (fd >= 0 && (k = ler(fd, blq, sizeof blq)) > 0) {
            char l[120];
            int p = 0;
            l[p++] = 'H'; l[p++] = 'X'; l[p++] = ':';
            for (int q = 0; q < k; q++) { l[p++] = "0123456789abcdef"[blq[q] >> 4]; l[p++] = "0123456789abcdef"[blq[q] & 15]; }
            l[p++] = '\n'; l[p] = 0;
            imprimir(l);
            tot += k;
        }
        if (fd >= 0) fechar(fd);
        char v[40];
        formatar_texto(v, sizeof v, "HEXFIM %d\n", tot);
        imprimir(v);
        dormir(1500);   /* que el terminal termine de escribir antes del aviso de fin de proceso */
    }
    return 0;
}
