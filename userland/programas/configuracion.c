// SPDX-License-Identifier: GPL-2.0-or-later
/* configuracion - la aplicación «Configuración» de SaguiOS (D112, mejoras 007 y 047).
 *
 *   configuracion                    la ventana, con las secciones Teclado, Fecha y hora, Pantalla,
 *                                    Sonido, Escritorio, Capturas e IA
 *   configuracion --aplicar          lo que el kernel lanza al arrancar: lee el archivo de
 *                                    configuración y lo aplica (teclado, volumen, zona, acento, fondo)
 *   configuracion mostrar            el contenido del archivo y el estado actual
 *   configuracion teclado es|abnt2
 *   configuracion volumen 0-100|silencio|activar
 *   configuracion zona NOMBRE        (por ejemplo Europe/Madrid)
 *   configuracion acento RRGGBB      color de acento (barra de título y barra de tareas)
 *   configuracion fondo ARCHIVO [centrado|ajustado|rellenar|estirado|mosaico]
 *   configuracion fondo ninguno      vuelve al degradado
 *   configuracion fuente documentos|titulos RUTA|ninguna   fuente por defecto (D196)
 *
 * Todo se guarda en /home/.config/sagui.conf (texto «clave=valor») cuando /home es un disco
 * persistente (D040). Sin él, los cambios valen hasta apagar y el programa lo avisa.
 * La sección Fuentes (D196) lista las fuentes de /apps/fuentes y /home/fuentes, enseña una vista
 * previa y guarda la fuente por defecto para documentos y para títulos (fuente_documentos= y
 * fuente_titulos= en sagui.conf, y /tmp/fuentes.conf para la sesión; los lee fuentes_defecto).
 * La sección IA (D145) escribe su propia configuración en /home/.config/ia, que es la que
 * leen el terminal, el chat con ventana y el Mico.
 * El kernel no decodifica imágenes: este programa lee el BMP/PNG con la libsagui, lo
 * escala al tamaño de la pantalla según el modo y se lo entrega por tandas de líneas
 * (SIS_FUNDO), sin necesitar un búfer del tamaño de la pantalla. */
#include "sagui.h"
#include "gui.h"
#include "imagem.h"
#include "ia.h"
#include "fontes.h"

#define DIR_CONF   "/home/.config"
#define ARQ_CONF   "/home/.config/sagui.conf"
#define NOME_MAX_F 120

/* Modos del fondo. */
enum { M_DEGRADE, M_CENTRADO, M_AJUSTADO, M_RELLENAR, M_ESTIRADO, M_MOSAICO, M_QTD };
static const char *const nomes_modo[M_QTD] = { "degradado", "centrado", "ajustado", "rellenar", "estirado", "mosaico" };
static const char *const rotulos_modo[M_QTD] = { "Degradado", "Centrado", "Ajustado", "Rellenar", "Estirado", "Mosaico" };

typedef struct {
    int      teclado;          /* -1 = no elegido */
    int      volume;           /* -1 = no elegido */
    int      silencio;
    char     zona[48];
    uint32_t acento;           /* 0xFFFFFFFF = no elegido */
    char     fundo[NOME_MAX_F];
    int      modo;
    char     capturas[NOME_MAX_F];   /* carpeta de las capturas (D125); vacía = /home/capturas */
    int      cap_sonido;             /* clic al capturar */
    char     fuente_doc[96];         /* fuente por defecto para documentos (D196); vacía = ninguna */
    char     fuente_tit[96];         /* y para títulos */
    int      eq[EQ_BANDAS];          /* ecualizador (D201): dB de 60 Hz, 250 Hz, 1 kHz, 4 kHz y 12 kHz */
    int      eq_def;                 /* 1 = la clave está en el archivo */
} conf_t;

static conf_t conf = { -1, -1, 0, "", 0xFFFFFFFF, "", M_AJUSTADO, "", 1 };

#define CORES_QTD 8
static const struct { const char *nome; uint32_t cor; } cores[CORES_QTD] = {
    { "Verde", 0x0B7A3E }, { "Azul", 0x2A6FD8 }, { "Rojo", 0xC0392B }, { "Naranja", 0xD97706 },
    { "Violeta", 0x7B3FA0 }, { "Turquesa", 0x0E8A8A }, { "Rosa", 0xC2437A }, { "Grafito", 0x4A5560 },
};

/* ------------------------------------------------------------ el archivo */

static bool home_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

static int hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool ler_cor(const char *s, uint32_t *cor)
{
    if (*s == '#') s++;
    uint32_t v = 0;
    int n = 0;
    for (; *s; s++, n++) {
        int h = hex(*s);
        if (h < 0) return false;
        v = v << 4 | (uint32_t)h;
    }
    if (n != 6) return false;
    *cor = v;
    return true;
}

static int indice_modo(const char *s)
{
    for (int i = 0; i < M_QTD; i++)
        if (compara_texto(s, nomes_modo[i]) == 0) return i;
    return -1;
}

/* Lee el archivo; false = no existe. Las claves desconocidas se ignoran. */
static bool conf_ler(void)
{
    int fd = abrir(ARQ_CONF, ABRIR_LER);
    if (fd < 0) return false;
    static char t[2048];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n < 0) return false;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *eq = acha_char(p, '=');
        if (p[0] != '#' && eq) {
            *eq = 0;
            const char *k = p, *v = eq + 1;
            size_t tv = tam_texto(v);
            while (tv && (v[tv - 1] == '\r' || v[tv - 1] == ' ')) ((char *)v)[--tv] = 0;
            uint32_t c;
            if (compara_texto(k, "teclado") == 0) conf.teclado = compara_texto(v, "abnt2") == 0 ? TECLADO_ABNT2
                                                                : compara_texto(v, "es") == 0 ? TECLADO_ES : -1;
            else if (compara_texto(k, "volumen") == 0) { int x = texto_para_int(v); conf.volume = x < 0 ? 0 : x > 100 ? 100 : x; }
            else if (compara_texto(k, "ecualizador") == 0) conf.eq_def = eq_texto_ler(v, conf.eq) == 0;
            else if (compara_texto(k, "silencio") == 0) conf.silencio = texto_para_int(v) != 0;
            else if (compara_texto(k, "zona") == 0) copia_texto(conf.zona, v, sizeof conf.zona);
            else if (compara_texto(k, "acento") == 0) { if (ler_cor(v, &c)) conf.acento = c; }
            else if (compara_texto(k, "fondo") == 0) copia_texto(conf.fundo, v, sizeof conf.fundo);
            else if (compara_texto(k, "capturas_carpeta") == 0) copia_texto(conf.capturas, v, sizeof conf.capturas);
            else if (compara_texto(k, "fuente_documentos") == 0) copia_texto(conf.fuente_doc, v, sizeof conf.fuente_doc);
            else if (compara_texto(k, "fuente_titulos") == 0) copia_texto(conf.fuente_tit, v, sizeof conf.fuente_tit);
            else if (compara_texto(k, "capturas_sonido") == 0) conf.cap_sonido = texto_para_int(v) != 0;
            else if (compara_texto(k, "fondo_modo") == 0) { int m = indice_modo(v); if (m >= 0) conf.modo = m; }
        }
        if (!fim) break;
        p = fim + 1;
    }
    return true;
}

/* Graba el archivo. 0 = grabado; 1 = no hay /home persistente (no se recordará); <0 = error. */
static int conf_gravar(void)
{
    /* Las capturas (D125) leen también /tmp/capturas.conf: vale en la sesión aunque /home no persista. */
    {
        char c[NOME_MAX_F + 60];
        formatar_texto(c, sizeof c, "capturas_carpeta=%s\ncapturas_sonido=%d\n", conf.capturas, conf.cap_sonido);
        int fdc = abrir("/tmp/capturas.conf", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fdc >= 0) { escrever_tudo(fdc, c, (unsigned)tam_texto(c)); fechar(fdc); }
    }
    /* Las fuentes por defecto (D196) también valen en la sesión: fuentes_defecto lee /tmp/fuentes.conf primero. */
    char fnt[240] = "";
    if (conf.fuente_doc[0]) formatar_texto(fnt, sizeof fnt, "fuente_documentos=%s\n", conf.fuente_doc);
    if (conf.fuente_tit[0]) {
        size_t l = tam_texto(fnt);
        formatar_texto(fnt + l, sizeof fnt - l, "fuente_titulos=%s\n", conf.fuente_tit);
    }
    {
        int fdf = abrir("/tmp/fuentes.conf", ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fdf >= 0) { escrever_tudo(fdf, fnt, (unsigned)tam_texto(fnt)); fechar(fdf); }
    }
    if (!home_persistente()) return 1;
    criar_pasta(DIR_CONF);
    char t[1100];
    char cap[NOME_MAX_F + 60];
    formatar_texto(cap, sizeof cap, "capturas_carpeta=%s\ncapturas_sonido=%d\n", conf.capturas, conf.cap_sonido);
    char tec[24] = "";
    if (conf.teclado >= 0) formatar_texto(tec, sizeof tec, "teclado=%s\n", conf.teclado == TECLADO_ABNT2 ? "abnt2" : "es");
    char vol[48] = "";
    if (conf.volume >= 0) formatar_texto(vol, sizeof vol, "volumen=%d\nsilencio=%d\n", conf.volume, conf.silencio);
    char ecu[48] = "";
    if (conf.eq_def) formatar_texto(ecu, sizeof ecu, "ecualizador=%d,%d,%d,%d,%d\n", conf.eq[0], conf.eq[1], conf.eq[2], conf.eq[3], conf.eq[4]);
    char zon[80] = "";
    if (conf.zona[0]) formatar_texto(zon, sizeof zon, "zona=%s\n", conf.zona);
    char ace[24] = "";
    if (conf.acento != 0xFFFFFFFFu) formatar_texto(ace, sizeof ace, "acento=%06x\n", conf.acento);
    char fun[NOME_MAX_F + 40] = "";
    if (conf.fundo[0]) formatar_texto(fun, sizeof fun, "fondo=%s\nfondo_modo=%s\n", conf.fundo, nomes_modo[conf.modo]);
    formatar_texto(t, sizeof t,
                   "# Configuración de SaguiOS (la escribe el programa \"configuracion\"; D112).\n"
                   "# El kernel la aplica al arrancar con \"configuracion --aplicar\".\n%s%s%s%s%s%s%s%s",
                   tec, vol, ecu, zon, ace, fun, cap, fnt);
    int fd = abrir(ARQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    return r < 0 ? r : 0;
}

/* ------------------------------------------------------------ aplicar */

static int aplicar_teclado(int l)
{
    return teclado_layout(l) < 0 ? ERRO_INVALIDO : 0;
}

static int aplicar_volume(int v, int silencio)
{
    if (audio_volume_mestre(-1) < 0) return ERRO_NAO_EXISTE;   /* sin tarjeta de sonido */
    if (v >= 0) audio_volume_mestre(v);
    audio_silencio(silencio);
    return 0;
}

/* Cambia la zona con el programa "zona" (que ya sabe aplicar y grabar /etc/zona y /home/.zona). */
static int aplicar_zona(const char *nome)
{
    char *av[] = { "zona", (char *)nome, NULL };
    int pid = executar("/bin/zona", av);
    if (pid < 0) return pid;
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo == 0 ? 0 : ERRO_INVALIDO;
}

/* Rellena filas de l píxeles (la tanda empieza en la línea y0) con la imagen en el modo pedido. */
typedef struct { int ox, oy, l, a; } caixa_img_t;

static void geometria(int modo, const imagem_t *im, int L, int A, caixa_img_t *c)
{
    int il = im->larg, ia = im->alt;
    c->l = il; c->a = ia;
    if (modo == M_ESTIRADO) { c->l = L; c->a = A; }
    else if (modo == M_AJUSTADO || modo == M_RELLENAR) {
        /* escala común: L/il contra A/ia, sin deformar (en 16.16) */
        int sx = (L << 10) / il, sy = (A << 10) / ia;   /* L, il < 2^14: cabe */
        int s = modo == M_AJUSTADO ? (sx < sy ? sx : sy) : (sx > sy ? sx : sy);
        c->l = (il * s) >> 10; c->a = (ia * s) >> 10;
        if (c->l < 1) c->l = 1;
        if (c->a < 1) c->a = 1;
    }
    c->ox = (L - c->l) / 2;
    c->oy = (A - c->a) / 2;
}

/* La ventana (NULL si el programa va por la línea de comandos). Se declara
 * aquí porque fondo_aplicar() la usa para dibujar la barra de progreso. */
static jan_t *jn;

/* Barra de progreso del fondo (mejora 042). Mientras se aplica la imagen se
 * ve el avance: se manda al kernel en tandas de líneas, y en una máquina vieja
 * eso tarda. Es NULL cuando no hay ventana (al arrancar) o cuando no toca. */
static progresso_t *barra_fundo;
static bool mostrando_barra;
#define BARRA_FUNDO_X 14
#define BARRA_FUNDO_Y 442
#define BARRA_FUNDO_L 260
#define BARRA_FUNDO_A 14

/* Pone el tanto por ciento y lo dibuja. */
static void barra_fundo_porcento(int feito, int total)
{
    static bool avisada;
    if (!barra_fundo || !jn) return;
    if (total <= 0) total = 1;
    mostrando_barra = true;
    progresso_definir(barra_fundo, feito * 100 / total);
    progresso_desenhar(jn, barra_fundo);
    jan_mostrar_area(jn, BARRA_FUNDO_X, BARRA_FUNDO_Y, BARRA_FUNDO_L, BARRA_FUNDO_A);
    if (!avisada) {
        avisada = true;
        registrar("configuracion: barra de progreso del fondo dibujada (%d%%)", feito * 100 / total);
    }
}

/* Aplica el fondo: 0 = ok. */
static int fundo_aplicar(const char *caminho, int modo)
{
    if (modo == M_DEGRADE || !caminho[0]) return fundo_enviar(FUNDO_PADRAO, NULL, 0, 0);
    if (fundo_enviar(3, NULL, 0, 0) == ERRO_ACESSO) return ERRO_ACESSO;   /* sin escritorio */
    info_aparencia_t ap;
    if (info_sistema(INFO_APARENCIA, &ap, sizeof ap) < 0 || !ap.largura || !ap.altura) return ERRO_INVALIDO;
    int L = (int)ap.largura, A = (int)ap.altura;
    imagem_t im;
    int r = imagem_ler(caminho, &im);
    if (r < 0) return r;
    if (im.larg > 16000 || im.alt > 16000) { imagem_liberar(&im); return IMG_ERRO_GRANDE; }
    caixa_img_t c;
    geometria(modo, &im, L, A, &c);
    enum { TANDA = 32 };
    uint32_t *buf = aloca((size_t)L * TANDA * 4);
    if (!buf) { imagem_liberar(&im); return ERRO_SEM_MEMORIA; }
    bool suave = c.l != im.larg || c.a != im.alt;
    for (int y0 = 0; y0 < A; y0 += TANDA) {
        int n = A - y0 < TANDA ? A - y0 : TANDA;
        for (int k = 0; k < L * n; k++) buf[k] = 0x000000;
        uint32_t *dst = buf - (size_t)y0 * L;   /* las coordenadas de imagem_desenhar son de la pantalla */
        if (modo == M_MOSAICO) {
            int oy = -(y0 % im.alt);
            for (int ty = y0 + oy; ty < y0 + n; ty += im.alt)
                for (int tx = 0; tx < L; tx += im.larg)
                    imagem_desenhar(dst, L, 0, y0, L, n, &im, tx, ty, im.larg, im.alt, false);
        } else {
            imagem_desenhar(dst, L, 0, y0, L, n, &im, c.ox, c.oy, c.l, c.a, suave);
        }
        r = fundo_enviar(FUNDO_LINHAS, buf, y0, n);
        if (r < 0) break;
        barra_fundo_porcento(y0 + n, A);
    }
    barra_fundo_porcento(A, A);
    libera(buf);
    imagem_liberar(&im);
    if (r >= 0) r = fundo_enviar(FUNDO_FIM, NULL, 0, 0);
    return r < 0 ? r : 0;
}

static const char *texto_erro(int r)
{
    if (r <= -200 && r >= -204) return imagem_erro(r);
    return nome_erro(r);
}

/* Lo que hace el arranque. Los errores van al log; nada de esto debe detener el sistema. */
static int aplicar_tudo(void)
{
    if (!conf_ler()) { registrar("configuracion: sin %s, nada que aplicar", ARQ_CONF); return 0; }
    if (conf.teclado >= 0) registrar("configuracion: teclado %d -> %d", conf.teclado, aplicar_teclado(conf.teclado));
    if (conf.volume >= 0) registrar("configuracion: volumen %d silencio %d -> %d", conf.volume, conf.silencio,
                                    aplicar_volume(conf.volume, conf.silencio));
    if (conf.eq_def) registrar("configuracion: ecualizador %d,%d,%d,%d,%d -> %d", conf.eq[0], conf.eq[1], conf.eq[2],
                               conf.eq[3], conf.eq[4], eq_aplicar(conf.eq));
    if (conf.zona[0]) {
        config_zona_t z;
        ler_zona(&z);
        if (compara_texto(z.nome, conf.zona) != 0) registrar("configuracion: zona %s -> %d", conf.zona, aplicar_zona(conf.zona));
    }
    if (conf.acento != 0xFFFFFFFFu) registrar("configuracion: acento %06x -> %d", conf.acento, acento_cor((int)conf.acento) < 0);
    if (conf.fundo[0] && conf.modo != M_DEGRADE)
        registrar("configuracion: fondo %s (%s) -> %d", conf.fundo, nomes_modo[conf.modo], fundo_aplicar(conf.fundo, conf.modo));
    return 0;
}

/* ------------------------------------------------------------ línea de comandos */

static void aviso_persistencia(int g)
{
    if (g == 1) imprimir("Aviso: /home no es un disco persistente; el cambio vale hasta apagar y no se recordará.\n");
    else if (g < 0) imprimir("Aviso: no se pudo guardar %s: %s\n", ARQ_CONF, nome_erro(g));
    else imprimir("Guardado en %s: se aplicará en cada arranque.\n", ARQ_CONF);
}

static int mostrar(void)
{
    bool tem = conf_ler();
    imprimir("Archivo: %s (%s)\n", ARQ_CONF, tem ? "existe" : "no existe");
    imprimir("Teclado:   %s\n", teclado_layout(-1) == TECLADO_ABNT2 ? "abnt2" : "es");
    info_aparencia_t ap;
    memset(&ap, 0, sizeof ap);
    info_sistema(INFO_APARENCIA, &ap, sizeof ap);
    imprimir("Acento:    %06x\n", ap.acento);
    imprimir("Fondo:     %s\n", ap.fundo_imagem ? "imagen" : "degradado");
    int v = audio_volume_mestre(-1);
    if (v >= 0) imprimir("Volumen:   %d %%%s\n", v, audio_silencio(-1) > 0 ? " (silencio)" : "");
    config_zona_t z;
    ler_zona(&z);
    imprimir("Zona:      %s\n", z.nome);
    if (tem) {
        imprimir("En el archivo: teclado %d, volumen %d, zona '%s', acento %06x, fondo '%s' (%s)\n", conf.teclado, conf.volume,
                 conf.zona, conf.acento == 0xFFFFFFFFu ? 0 : conf.acento, conf.fundo, nomes_modo[conf.modo]);
    }
    return 0;
}

static int linha_de_comandos(int argc, char **argv)
{
    conf_ler();
    const char *c = argv[1];
    int r = 0;
    if (compara_texto(c, "mostrar") == 0) return mostrar();
    if (compara_texto(c, "teclado") == 0 && argc == 3) {
        int l = compara_texto(argv[2], "abnt2") == 0 ? TECLADO_ABNT2 : compara_texto(argv[2], "es") == 0 ? TECLADO_ES : -1;
        if (l < 0) { imprimir_em(ERROS, "configuracion: teclado es | abnt2\n"); return 2; }
        r = aplicar_teclado(l);
        conf.teclado = l;
    } else if (compara_texto(c, "volumen") == 0 && argc == 3) {
        if (compara_texto(argv[2], "silencio") == 0) conf.silencio = 1;
        else if (compara_texto(argv[2], "activar") == 0) conf.silencio = 0;
        else { int v = texto_para_int(argv[2]); conf.volume = v < 0 ? 0 : v > 100 ? 100 : v; conf.silencio = 0; }
        if (conf.volume < 0) conf.volume = audio_volume_mestre(-1);
        r = aplicar_volume(conf.volume, conf.silencio);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no hay tarjeta de sonido\n"); return 1; }
    } else if (compara_texto(c, "zona") == 0 && argc == 3) {
        r = aplicar_zona(argv[2]);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no se pudo poner la zona %s\n", argv[2]); return 1; }
        copia_texto(conf.zona, argv[2], sizeof conf.zona);
    } else if (compara_texto(c, "acento") == 0 && argc == 3) {
        uint32_t cor;
        if (!ler_cor(argv[2], &cor)) { imprimir_em(ERROS, "configuracion: acento RRGGBB (por ejemplo 2A6FD8)\n"); return 2; }
        acento_cor((int)cor);
        conf.acento = cor;
    } else if (compara_texto(c, "fondo") == 0 && argc >= 3) {
        int modo = M_AJUSTADO;
        if (compara_texto(argv[2], "ninguno") == 0) { modo = M_DEGRADE; conf.fundo[0] = 0; }
        else {
            if (argc >= 4 && (modo = indice_modo(argv[3])) <= 0) { imprimir_em(ERROS, "configuracion: modo no válido\n"); return 2; }
            copia_texto(conf.fundo, argv[2], sizeof conf.fundo);
        }
        conf.modo = modo;
        r = fundo_aplicar(conf.fundo, modo);
        if (r < 0) { imprimir_em(ERROS, "configuracion: fondo: %s\n", texto_erro(r)); if (r == ERRO_ACESSO) imprimir("(sin escritorio gráfico)\n"); }
    } else if (compara_texto(c, "fuente") == 0 && argc == 4 &&
               (compara_texto(argv[2], "documentos") == 0 || compara_texto(argv[2], "titulos") == 0)) {
        char *d = compara_texto(argv[2], "documentos") == 0 ? conf.fuente_doc : conf.fuente_tit;
        if (compara_texto(argv[3], "ninguna") == 0) d[0] = 0;
        else {
            fonte_t *f = fonte_abrir_archivo(argv[3]);
            if (!f) { imprimir_em(ERROS, "configuracion: %s no es una fuente\n", argv[3]); return 1; }
            fonte_fechar(f);
            copia_texto(d, argv[3], 96);
        }
    } else {
        imprimir_em(ERROS, "uso: configuracion [--aplicar | mostrar | teclado es|abnt2 | volumen N | zona NOMBRE |\n"
                           "                      acento RRGGBB | fondo ARCHIVO [modo] | fondo ninguno |\n"
                           "                      fuente documentos|titulos RUTA|ninguna]\n");
        return 2;
    }
    aviso_persistencia(conf_gravar());
    return 0;
}

/* ------------------------------------------------------------ la ventana */

#define LARG 740
#define ALT  500
#define Y_PAG 50
enum { P_TECLADO, P_ZONA, P_TELA, P_SOM, P_ESCRITORIO, P_CAPTURAS, P_IA, P_FUENTES, P_RED, P_QTD };
static const char *const titulos_pag[P_QTD] = { "Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red" };

static int pagina;
static char estado[160];
static bool persiste;

static botao_t b_pag[P_QTD];
static int  pos_pag[P_QTD];      /* cada pestaña es del ancho de su título (D145) */
static int  larg_pag[P_QTD];
static caixa_t c_capturas;   /* carpeta de las capturas (D125) */
static botao_t b_cap[2] = { { 24, Y_PAG + 112, 120, 28, "Aplicar", false }, { 24, Y_PAG + 170, 260, 28, "", false } };
static botao_t b_cerrar = { LARG - 110, ALT - 40, 96, 28, "Cerrar", false };

/* Teclado */
static botao_t b_tec[2] = { { 24, Y_PAG + 56, 260, 30, "Español (ES)", false },
                            { 24, Y_PAG + 96, 260, 30, "Portugués de Brasil (ABNT2)", false } };

/* Fecha y hora */
#define MAXZ 128
static char itens_zona[MAXZ][64];
static int  indice_zona[MAXZ];
static lista_t lista_zona;
static botao_t b_zona = { 24, ALT - 100, 120, 28, "Aplicar", false };

/* Pantalla */
#define MAXM 192
static modo_video_t modos[MAXM];
static int qtd_modos;
static char itens_modo[MAXM][64];
static int indice_modo_l[MAXM];
static lista_t lista_modo;
static botao_t b_tela[2] = { { 24, ALT - 100, 120, 28, "Aplicar", false }, { 154, ALT - 100, 180, 28, "Aplicar y guardar", false } };
static info_tela_t tela;

/* Sonido */
#define BARRA_X 60
#define BARRA_Y (Y_PAG + 70)
#define BARRA_L 400
#define BARRA_A 24
static botao_t b_vol[3] = { { 24, BARRA_Y, 28, 24, "-", false }, { BARRA_X + BARRA_L + 8, BARRA_Y, 28, 24, "+", false },
                            { 24, BARRA_Y + 50, 140, 28, "Silenciar", false } };
static bool arrastando_vol;
/* Ecualizador (D201): cinco deslizadores verticales de -12 a +12 dB y los preajustes. */
#define EQ_Y0   (Y_PAG + 250)     /* arriba del carril: +12 dB (250 y no 290: los rótulos de las bandas pisaban el aviso de /home, v7.4) */
#define EQ_ALT  96                /* alto del carril: 4 px por dB */
#define EQ_X0   60
#define EQ_PASSO 84
static const char *const eq_rotulos[EQ_BANDAS] = { "60 Hz", "250 Hz", "1 kHz", "4 kHz", "12 kHz" };
static const char *const eq_nomes_pre[4] = { "plano", "graves", "voz", "agudos" };
static botao_t b_eq[4] = { { 500, EQ_Y0 - 14, 150, 26, "Plano", false }, { 500, EQ_Y0 + 16, 150, 26, "Graves", false },
                           { 500, EQ_Y0 + 46, 150, 26, "Voz", false }, { 500, EQ_Y0 + 76, 150, 26, "Agudos", false } };
static int eq_arrasta = -1;

/* Escritorio */
#define SW_X 24
#define SW_Y (Y_PAG + 28)
#define SW_L 56
#define SW_PASSO 70
static botao_t b_modo[M_QTD];
#define MAXF 64
static char itens_img[MAXF][64];
static lista_t lista_img;
static botao_t b_fundo = { 24, ALT - 100, 140, 28, "Aplicar fondo", false };

/* ------------------------------------------------------------ las fuentes (D196) */

static void titulo_pag(int x, int y, const char *t);
#define FN_LISTA_L 270
#define FN_VX (24 + FN_LISTA_L + 16)
#define FN_VL (LARG - FN_VX - 24)
#define FN_VY (Y_PAG + 24)
#define FN_VA 190
static fuente_info_t cat_fnt[FUENTES_MAX];
static char    itens_fnt[FUENTES_MAX][64];
static lista_t lista_fnt;
static int     n_fnt = -1;          /* -1 = el catálogo aún no se leyó */
static fonte_t *vista_fnt;          /* la fuente de la vista previa */
static int     vista_i = -1;
static botao_t b_fnt[3] = { { FN_VX, Y_PAG + 276, 150, 28, "Para documentos", false },
                            { FN_VX + 158, Y_PAG + 276, 120, 28, "Para títulos", false },
                            { FN_VX + 286, Y_PAG + 276, 90, 28, "Quitar", false } };
#define TXT_MUESTRA "El veloz murciélago hindú comía feliz cardillo y kiwi"
#define TXT_CIFRAS  "ABC abc 0123456789 ¿?¡!"

static const char *nombre_ruta(const char *r)
{
    for (int i = 0; i < n_fnt; i++) if (compara_texto(cat_fnt[i].ruta, r) == 0) return cat_fnt[i].familia;
    return nome_base(r);
}

/* Abre la fuente de la vista previa para el elemento elegido de la lista. */
static void fuentes_vista(void)
{
    if (lista_fnt.sel == vista_i) return;
    if (vista_fnt) { fonte_fechar(vista_fnt); vista_fnt = NULL; }
    vista_i = lista_fnt.sel;
    if (vista_i >= 0) vista_fnt = fonte_abrir_archivo(cat_fnt[vista_i].ruta);
}

static void fuentes_preparar(void)
{
    n_fnt = fuentes_catalogo(cat_fnt, FUENTES_MAX);
    for (int i = 0; i < n_fnt; i++) {
        if (cat_fnt[i].estilo[0]) formatar_texto(itens_fnt[i], 64, "%s %s", cat_fnt[i].familia, cat_fnt[i].estilo);
        else copia_texto(itens_fnt[i], cat_fnt[i].familia, 64);
    }
    lista_fnt = (lista_t){ 24, Y_PAG + 34, FN_LISTA_L, ALT - Y_PAG - 34 - 112, itens_fnt, n_fnt, -1, 0, NULL };
    for (int i = 0; i < n_fnt; i++) if (conf.fuente_doc[0] && compara_texto(cat_fnt[i].ruta, conf.fuente_doc) == 0) lista_fnt.sel = i;
    if (lista_fnt.sel < 0 && n_fnt) lista_fnt.sel = 0;
    if (lista_fnt.sel > 5) lista_fnt.topo = lista_fnt.sel - 5;
    vista_i = -2;
    fuentes_vista();
}

/* Escribe un texto con ajuste de línea por palabras; devuelve la y siguiente. */
static int vista_parrafo(fonte_t *f, int x, int y, int l, int ymax, const char *t, int px)
{
    int linea = fonte_altura_normal(f, px);
    int n = (int)tam_texto(t), i = 0;
    while (i < n && y + linea <= ymax) {
        int fin = i, ultimo = -1;
        while (fin < n) {
            int sig = fin;
            while (sig < n && t[sig] != ' ') sig++;
            if (fonte_medir_n(f, t + i, sig - i, px) > l && ultimo >= 0) break;
            ultimo = sig;
            fin = sig < n ? sig + 1 : sig;
        }
        int largo = (ultimo >= 0 ? ultimo : n) - i;
        fonte_texto_buf_n(f, jn->px, jn->larg, jn->alt, x, y, t + i, largo, px, 0x000000);
        y += linea;
        i += largo;
        while (i < n && t[i] == ' ') i++;
    }
    return y;
}

static void fuentes_desenhar(void)
{
    char t[160];
    if (n_fnt <= 0) {
        titulo_pag(24, Y_PAG + 14, "No hay fuentes instaladas.");
        g_texto(jn, 24, Y_PAG + 40, "Se buscan ficheros .otf y .ttf en " FUENTES_DIR_APPS " y en " FUENTES_DIR_HOME ".", COR_TEXTO_FRACO);
        g_texto(jn, 24, Y_PAG + 60, "Las de Typodermic (CC0) vienen en la partición SAGUI-APPS.", COR_TEXTO_FRACO);
        return;
    }
    formatar_texto(t, sizeof t, "%d fuentes (" FUENTES_DIR_APPS ", " FUENTES_DIR_HOME ")", n_fnt);
    titulo_pag(24, Y_PAG + 8, t);
    lista_desenhar(jn, &lista_fnt);
    g_retangulo(jn, FN_VX, FN_VY, FN_VL, FN_VA, 0xFFFFFF);
    g_moldura(jn, FN_VX, FN_VY, FN_VL, FN_VA, COR_BORDA);
    if (vista_fnt) {
        int y = vista_parrafo(vista_fnt, FN_VX + 8, FN_VY + 6, FN_VL - 16, FN_VY + FN_VA - 4, TXT_MUESTRA, 26);
        vista_parrafo(vista_fnt, FN_VX + 8, y + 6, FN_VL - 16, FN_VY + FN_VA - 4, TXT_CIFRAS, 22);
    } else g_texto(jn, FN_VX + 8, FN_VY + 8, "No se pudo cargar esta fuente.", 0xB03000);
    formatar_texto(t, sizeof t, "Documentos: %s", conf.fuente_doc[0] ? nombre_ruta(conf.fuente_doc) : "(ninguna)");
    g_texto_n(jn, FN_VX, Y_PAG + FN_VA + 36, t, FN_VL / LETRA_L, COR_TEXTO);
    formatar_texto(t, sizeof t, "Títulos:     %s", conf.fuente_tit[0] ? nombre_ruta(conf.fuente_tit) : "(ninguna)");
    g_texto_n(jn, FN_VX, Y_PAG + FN_VA + 56, t, FN_VL / LETRA_L, COR_TEXTO);
    for (int i = 0; i < 3; i++) botao_desenhar(jn, &b_fnt[i]);
}

/* ------------------------------------------------------------ la IA (D145) */

/* Los proveedores con nombre que trae la biblioteca (ia_provedores). */
#define MAX_PROV 16
#define MAX_MOD  64
static ia_provedor_t provedores[MAX_PROV];
static int   n_provedores;
static char  itens_prov[MAX_PROV][64];
static lista_t lista_prov;
static char  itens_mod[MAX_MOD][64];
static lista_t lista_mod;
/* Las seis cajas de la página: 0 URL, 1 clave, 2 general, 3 terminal, 4 chat, 5 mico. */
static caixa_t c_ia[6];
static const char *const rotulos_uso[4] = { "Modelo por omisión", "Terminal", "Chat", "Mico" };
static ia_config_t cfg_ia;
/* La clave de API entera: la caja solo cabe 127 caracteres, así que la clave se
 * guarda aparte y la caja solo la enseña con asteriscos (D145). */
static char chave_ia[IA_CHAVE_MAX];
static char chave_vista[128];
static int  cx_ativa = -1;    /* la casilla con el foco del teclado (-1 = ninguna) */
static int  cx_alvo = 2;      /* la casilla donde va el modelo de la lista (2 = la general) */
static int  lst_ativa = -1;   /* la lista con el foco: 0 proveedores, 1 modelos */
static botao_t b_ia[2] = { { 250, 0, 120, 28, "Guardar", false },
                           { 380, 0, 170, 28, "Cargar modelos", false } };
/* Los botones que eligen dónde va el modelo que se elige en la lista. */
static botao_t b_alvo[4] = { { 345, 0, 88, 28, "General", false },
                             { 437, 0, 88, 28, "Terminal", false },
                             { 529, 0, 88, 28, "Chat", false },
                             { 621, 0, 88, 28, "Mico", false } };

static void ia_desenhar(void);

static void ler_modos_video(void)
{
    qtd_modos = modos_video(modos, MAXM);
    if (qtd_modos < 0) qtd_modos = 0;
    memset(&tela, 0, sizeof tela);
    info_sistema(INFO_TELA, &tela, sizeof tela);
    int n = 0;
    lista_modo.sel = -1;
    for (int i = 0; i < qtd_modos && n < MAXM; i++) {
        modo_video_t *m = &modos[i];
        if (m->bpp != 32) continue;
        formatar_texto(itens_modo[n], sizeof itens_modo[n], "%ux%u%s%s", m->largura, m->altura,
                       (m->flags & MODO_ATUAL) ? "  (actual)" : "", (m->flags & MODO_A_QUENTE) ? "" : "  (al reiniciar)");
        if (m->flags & MODO_ATUAL) lista_modo.sel = n;
        indice_modo_l[n++] = i;
    }
    lista_modo.qtd = n;
}

static void ler_zonas(void)
{
    int n;
    const zona_info_t *z = zona_tabela(&n);
    config_zona_t cz;
    ler_zona(&cz);
    lista_zona.qtd = 0;
    lista_zona.sel = -1;
    uint32_t t = hora_utc(NULL);
    for (int i = 0; i < n && lista_zona.qtd < MAXZ; i++) {
        fuso_t fz;
        fuso_ler(z[i].tz, &fz);
        int d = fuso_desvio(&fz, t, NULL), a = d < 0 ? -d : d;
        formatar_texto(itens_zona[lista_zona.qtd], sizeof itens_zona[0], "UTC%c%02d:%02d  %s", d < 0 ? '-' : '+', a / 3600,
                       a / 60 % 60, z[i].mostrar);
        indice_zona[lista_zona.qtd] = i;
        if (compara_texto(z[i].nome, cz.nome) == 0) lista_zona.sel = lista_zona.qtd;
        lista_zona.qtd++;
    }
    if (lista_zona.sel > 4) lista_zona.topo = lista_zona.sel - 4;
}

static bool ext_imagem(const char *n) { return imagem_extensao(n); }

static void listar_imagens(void)
{
    static const char *const pastas[] = { "/imagenes", "/home" };
    lista_img.qtd = 0;
    lista_img.sel = -1;
    for (int p = 0; p < 2; p++) {
        entrada_pasta_t *v = NULL;
        int n = listar_tudo(pastas[p], &v);
        if (n < 0) continue;
        for (int i = 0; i < n && lista_img.qtd < MAXF; i++) {
            if (v[i].tipo != TIPO_ARQUIVO || !ext_imagem(v[i].nome)) continue;
            char caminho[NOME_MAX_F];
            caminho_juntar(caminho, sizeof caminho, pastas[p], v[i].nome);
            if (tam_texto(caminho) >= 63) continue;
            copia_texto(itens_img[lista_img.qtd], caminho, 64);
            if (compara_texto(caminho, conf.fundo) == 0) lista_img.sel = lista_img.qtd;
            lista_img.qtd++;
        }
        if (v) libera(v);
    }
}

static void dizer(const char *t) { copia_texto(estado, t, sizeof estado); }

static void dizer_gravado(int g)
{
    if (g == 1) dizer("Aplicado. /home no es persistente: no se recordará al reiniciar.");
    else if (g < 0) formatar_texto(estado, sizeof estado, "Aplicado, pero no se pudo guardar: %s", nome_erro(g));
    else dizer("Aplicado y guardado en /home/.config/sagui.conf.");
}

static void titulo_pag(int x, int y, const char *t)
{
    g_texto(jn, x, y, t, COR_TEXTO);
    g_texto(jn, x + 1, y, t, COR_TEXTO);
}

/* Red (D218): el bloqueo de publicidad y rastreo por DNS. Los botones llaman
 * al programa «bloqueo», que es quien compila la tabla y escribe bloqueo.conf. */
static botao_t b_bloq[2] = { { 24, Y_PAG + 100, 160, 28, "Activar", false },
                             { 194, Y_PAG + 100, 180, 28, "Actualizar listas", false } };

static int rodar_bloqueo(const char *ordem)
{
    char *av[] = { "bloqueo", (char *)ordem, NULL };
    int pid = executar("/bin/bloqueo", av);
    if (pid < 0) return pid;
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo == 0 ? 0 : ERRO_INVALIDO;
}

static void red_desenhar(void)
{
    char t[160];
    config_rede_t c;
    memset(&c, 0, sizeof c);
    char a[16], d[16];
    if (config_rede(REDE_CONSULTAR, &c) == 0 && c.ip)
        formatar_texto(t, sizeof t, "Red: IP %s, DNS %s", ip_para_texto(c.ip, a), c.dns ? ip_para_texto(c.dns, d) : "ninguno");
    else copia_texto(t, "Red: sin dirección IP", sizeof t);
    g_texto(jn, 24, Y_PAG + 8, t, COR_TEXTO_FRACO);
    titulo_pag(24, Y_PAG + 36, "Bloqueo de publicidad y rastreo por DNS");
    info_bloqueo_t b;
    memset(&b, 0, sizeof b);
    bloqueo_dns(BLOQ_ESTADO, &b, sizeof b);
    if (b.activo) formatar_texto(t, sizeof t, "Activado: %u dominios bloqueados, %u permitidos (%u KB).",
                                 b.bloqueados, b.permitidos, (b.bytes + 1023) / 1024);
    else copia_texto(t, b.pedido ? "Desactivado: no hay tabla (pulsa Actualizar listas)." : "Desactivado: se resuelven todos los nombres.", sizeof t);
    g_texto(jn, 24, Y_PAG + 64, t, COR_TEXTO);
    b_bloq[0].texto = b.activo ? "Desactivar" : "Activar";
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_bloq[i]);
    formatar_texto(t, sizeof t, "Consultas: %u   Bloqueadas: %u", b.consultas, b.bloqueadas);
    g_texto(jn, 24, Y_PAG + 144, t, COR_TEXTO);
    static char u[6][64];
    int n = bloqueo_dns(BLOQ_ULTIMOS, u, 6);
    if (n > 0) {
        g_texto(jn, 24, Y_PAG + 170, "Últimos bloqueados:", COR_TEXTO_FRACO);
        for (int i = 0; i < n; i++) g_texto_n(jn, 40, Y_PAG + 190 + i * 18, u[i], (LARG - 80) / LETRA_L, COR_TEXTO);
    }
    g_texto_n(jn, 24, ALT - 110, "Los dominios de las listas responden 0.0.0.0 a todos los programas.", (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    g_texto_n(jn, 24, ALT - 92, "Permitir o bloquear a mano, y el servidor para la red: bloqueo --ayuda, dnsbloqueo.", (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
}

static void desenhar_pagina(void)
{
    char t[160];
    if (pagina == P_TECLADO) {
        titulo_pag(24, Y_PAG + 14, "Distribución del teclado");
        int cur = teclado_layout(-1);
        for (int i = 0; i < 2; i++) { b_tec[i].apertado = cur == i; botao_desenhar(jn, &b_tec[i]); }
        g_texto(jn, 24, Y_PAG + 144, "Prueba: la ñ, la ç y los acentos cambian según la distribución.", COR_TEXTO_FRACO);
    } else if (pagina == P_ZONA) {
        config_zona_t cz;
        ler_zona(&cz);
        fuso_data_t l;
        agora_local(&l);
        formatar_texto(t, sizeof t, "Zona horaria actual: %s   (%02d:%02d)", cz.nome, l.hora, l.minuto);
        titulo_pag(24, Y_PAG + 8, t);
        lista_desenhar(jn, &lista_zona);
        botao_desenhar(jn, &b_zona);
    } else if (pagina == P_TELA) {
        formatar_texto(t, sizeof t, "Resolución actual: %ux%u, %u bits", tela.largura, tela.altura, tela.bpp);
        titulo_pag(24, Y_PAG + 8, t);
        lista_desenhar(jn, &lista_modo);
        for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_tela[i]);
        if (!tela.a_quente) g_texto(jn, 350, ALT - 94, "Esta tarjeta cambia al reiniciar.", COR_TEXTO_FRACO);
    } else if (pagina == P_SOM) {
        info_audio_t ia;
        if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) {
            titulo_pag(24, Y_PAG + 14, "No hay tarjeta de sonido (solo el altavoz del PC).");
        } else {
            formatar_texto(t, sizeof t, "Volumen general: %u %%%s", ia.volume, ia.silencio ? " (silencio)" : "");
            titulo_pag(24, Y_PAG + 14, t);
            g_retangulo(jn, BARRA_X, BARRA_Y, BARRA_L, BARRA_A, COR_BOTAO);
            g_moldura(jn, BARRA_X, BARRA_Y, BARRA_L, BARRA_A, COR_BORDA);
            g_retangulo(jn, BARRA_X + 1, BARRA_Y + 1, (BARRA_L - 2) * ia.volume / 100, BARRA_A - 2, ia.silencio ? COR_BORDA : COR_DESTAQUE);
            for (int i = 0; i < 3; i++) botao_desenhar(jn, &b_vol[i]);
            formatar_texto(t, sizeof t, "Tarjeta: %s (%s)", ia.placa, ia.driver);
            g_texto_n(jn, 24, BARRA_Y + 100, t, (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
            g_texto_n(jn, 24, BARRA_Y + 120, ia.codec, (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
            titulo_pag(24, EQ_Y0 - 32, "Ecualizador (dB)");
            for (int i = 0; i < EQ_BANDAS; i++) {
                int cx = EQ_X0 + i * EQ_PASSO, v = ia.eq_db[i];
                int ym = EQ_Y0 + EQ_ALT / 2, yv = EQ_Y0 + (12 - v) * EQ_ALT / 24;
                g_retangulo(jn, cx - 2, EQ_Y0, 4, EQ_ALT, COR_BOTAO);
                g_moldura(jn, cx - 2, EQ_Y0, 4, EQ_ALT, COR_BORDA);
                g_retangulo(jn, cx - 14, ym, 28, 1, COR_BORDA);                 /* la marca de 0 dB */
                g_retangulo(jn, cx - 2, v >= 0 ? yv : ym, 4, v >= 0 ? ym - yv : yv - ym, COR_DESTAQUE);
                g_retangulo(jn, cx - 10, yv - 4, 20, 9, COR_DESTAQUE);
                g_moldura(jn, cx - 10, yv - 4, 20, 9, COR_BORDA);
                eq_db_texto(t, v);
                g_texto(jn, cx - (int)tam_texto(t) * LETRA_L / 2, EQ_Y0 - 20, t, COR_TEXTO);
                g_texto(jn, cx - (int)tam_texto(eq_rotulos[i]) * LETRA_L / 2, EQ_Y0 + EQ_ALT + 8, eq_rotulos[i], COR_TEXTO_FRACO);
            }
            for (int i = 0; i < 4; i++) botao_desenhar(jn, &b_eq[i]);
        }
    } else if (pagina == P_CAPTURAS) {
        titulo_pag(24, Y_PAG + 14, "Carpeta de las capturas de pantalla (Impr Pant)");
        g_texto(jn, 24, Y_PAG + 40, "Vacía = /home/capturas (o /tmp/capturas si /home no deja escribir).", COR_TEXTO_FRACO);
        caixa_desenhar(jn, &c_capturas);
        botao_desenhar(jn, &b_cap[0]);
        b_cap[1].texto = conf.cap_sonido ? "Sonido al capturar: sí" : "Sonido al capturar: no";
        botao_desenhar(jn, &b_cap[1]);
    } else if (pagina == P_IA) {
        ia_desenhar();
    } else if (pagina == P_FUENTES) {
        fuentes_desenhar();
    } else if (pagina == P_RED) {
        red_desenhar();
    } else {
        info_aparencia_t ap;
        memset(&ap, 0, sizeof ap);
        info_sistema(INFO_APARENCIA, &ap, sizeof ap);
        titulo_pag(24, Y_PAG + 6, "Color de acento (barra de título y barra de tareas)");
        for (int i = 0; i < CORES_QTD; i++) {
            int x = SW_X + i * SW_PASSO;
            g_retangulo(jn, x, SW_Y, SW_L, 30, cores[i].cor);
            g_moldura(jn, x, SW_Y, SW_L, 30, cores[i].cor == ap.acento ? 0x000000 : COR_BORDA);
            if (cores[i].cor == ap.acento) g_moldura(jn, x + 1, SW_Y + 1, SW_L - 2, 28, 0xFFFFFF);
            g_texto(jn, x, SW_Y + 34, cores[i].nome, COR_TEXTO_FRACO);
        }
        titulo_pag(24, Y_PAG + 98, "Fondo del escritorio");
        for (int i = 0; i < M_QTD; i++) { b_modo[i].apertado = conf.modo == i; botao_desenhar(jn, &b_modo[i]); }
        g_texto(jn, 24, Y_PAG + 160, "Imagen (BMP o PNG de /imagenes y de /home):", COR_TEXTO_FRACO);
        lista_desenhar(jn, &lista_img);
        botao_desenhar(jn, &b_fundo);
    }
}

static void desenhar(void)
{
    g_retangulo(jn, 0, 0, LARG, ALT, COR_FUNDO);
    for (int i = 0; i < P_QTD; i++) {
        /* «apertado» son dos cosas: que esta es la página actual y que el ratón
         * tiene pulsado el botón. Al redibujar no se puede perder lo segundo: si se
         * pierde, al soltar el ratón la página ya no cambia (Capturas, v7.3.1). */
        bool actual = (i == pagina);
        bool premido = b_pag[i].apertado && !actual;
        b_pag[i].apertado = actual || premido;
        botao_desenhar(jn, &b_pag[i]);
    }
    g_retangulo(jn, 10, Y_PAG - 6, LARG - 20, 1, COR_BORDA);
    desenhar_pagina();
    if (!persiste) g_texto_n(jn, 14, ALT - 62, "Aviso: /home no es un disco persistente: los cambios no se recordarán.", (LARG - 28) / LETRA_L, 0xB03000);
    g_texto_n(jn, 14, ALT - 38, estado, (LARG - 130) / LETRA_L, COR_TEXTO_FRACO);
    /* La barra de progreso del fondo (042): sólo mientras se aplica. */
    if (mostrando_barra && barra_fundo) progresso_desenhar(jn, barra_fundo);
    botao_desenhar(jn, &b_cerrar);
    jan_mostrar(jn);
}

static bool dentro(const evento_t *e, int x, int y, int l, int a)
{
    return e->x >= x && e->x < x + l && e->y >= y && e->y < y + a;
}

/* ------------------------------------------------------- la página de la IA */

/* Lee la configuración guardada y la pone en las cajas. */
static void ia_preparar(void)
{
    n_provedores = ia_provedores(provedores, MAX_PROV);
    for (int i = 0; i < n_provedores; i++) copia_texto(itens_prov[i], provedores[i].nome, 64);
    lista_prov.qtd = n_provedores;
    lista_prov.sel = -1;
    ia_config_carregar(&cfg_ia);
    copia_texto(chave_ia, cfg_ia.chave, sizeof chave_ia);
    /* La caja no cabe si la clave es larga: se enseña recortada, pero se guarda
     * entera mientras el usuario no la toque. */
    int n = (int)tam_texto(chave_ia);
    copia_texto(chave_vista, chave_ia, n > 100 ? 101 : (int)sizeof chave_vista);
    caixa_definir(&c_ia[0], cfg_ia.url);
    caixa_definir(&c_ia[1], chave_vista);
    caixa_definir(&c_ia[2], cfg_ia.modelo);
    caixa_definir(&c_ia[3], cfg_ia.modelo_uso[IA_USO_TERMINAL]);
    caixa_definir(&c_ia[4], cfg_ia.modelo_uso[IA_USO_CHATIA]);
    caixa_definir(&c_ia[5], cfg_ia.modelo_uso[IA_USO_MICO]);
    for (int i = 0; i < 6; i++) c_ia[i].foco = false;
    cx_ativa = -1;
    cx_alvo = 2;          /* el destino por omisión es el modelo general */
    lst_ativa = -1;
    lista_mod.qtd = 0;
    lista_mod.sel = -1;
    /* El proveedor que coincide con la dirección guardada. */
    for (int i = 0; i < n_provedores; i++)
        if (provedores[i].url[0] && compara_texto(provedores[i].url, cfg_ia.url) == 0) lista_prov.sel = i;
}

/* De las cajas a la estructura: lo que se guarda. */
static void ia_recolher(void)
{
    copia_texto(cfg_ia.url, c_ia[0].texto, sizeof cfg_ia.url);
    /* La clave: si el usuario la tocó, vale lo que hay escrito; si no, la que estaba. */
    if (compara_texto(c_ia[1].texto, chave_vista) != 0) copia_texto(chave_ia, c_ia[1].texto, sizeof chave_ia);
    copia_texto(cfg_ia.chave, chave_ia, sizeof cfg_ia.chave);
    copia_texto(cfg_ia.modelo, c_ia[2].texto, sizeof cfg_ia.modelo);
    for (int i = 0; i < 3; i++)
        copia_texto(cfg_ia.modelo_uso[i], c_ia[3 + i].texto, sizeof cfg_ia.modelo_uso[0]);
}

/* El proveedor elegido: pone su dirección y su modelo por omisión. */
static void ia_escolher_provedor(void)
{
    if (lista_prov.sel < 0 || lista_prov.sel >= n_provedores) return;
    const ia_provedor_t *p = &provedores[lista_prov.sel];
    if (p->url[0]) {
        caixa_definir(&c_ia[0], p->url);
        if (p->modelo[0]) caixa_definir(&c_ia[2], p->modelo);
        /* El proveedor cambia el modelo por omisión: el destino pasa a ser esa
         * casilla, que además queda marcada. */
        cx_alvo = 2;
        if (p->precisa_chave) dizer("Proveedor elegido. Escribe la clave y pulsa Guardar.");
        else dizer("Proveedor elegido (no necesita clave). Pulsa Guardar.");
    } else {
        caixa_definir(&c_ia[0], "");
        dizer("Personalizado: escribe la dirección del servidor.");
    }
}

/* Pide los modelos al servidor y los deja en la lista. */
static void ia_cargar_modelos(void)
{
    ia_recolher();
    if (!cfg_ia.url[0]) { dizer("Primero pon la dirección del servidor."); return; }
    char erro[192];
    int n = ia_modelos(&cfg_ia, itens_mod, MAX_MOD, erro, (int)sizeof erro);
    if (n < 0) {
        formatar_texto(estado, sizeof estado, "No se pudieron pedir los modelos: %s", erro);
        return;
    }
    lista_mod.qtd = n;
    lista_mod.sel = -1;
    lista_mod.topo = 0;
    /* El catálogo del servidor se guarda con la configuración: así el chat con
     * ventana puede ofrecer los modelos sin volver a pedirlos (D145). */
    cfg_ia.modelos[0] = 0;
    int usados = 0;
    for (int i = 0; i < n; i++) {
        int ln = (int)tam_texto(itens_mod[i]);
        if (usados + ln + 2 >= (int)sizeof cfg_ia.modelos) break;
        if (i > 0) cfg_ia.modelos[usados++] = ',';
        for (int k = 0; k < ln; k++) cfg_ia.modelos[usados++] = itens_mod[i][k];
        cfg_ia.modelos[usados] = 0;
    }
    formatar_texto(estado, sizeof estado,
                    "%d modelos del servidor. Elige uno: va a la casilla donde está el cursor.", n);
}

/* El modelo elegido va a la casilla de destino (la que está marcada). La
 * casilla destino se queda con el foco, para que se vea dónde ha ido. */
static void ia_poner_modelo(const char *m)
{
    if (!m || !*m) return;
    if (cx_alvo < 2 || cx_alvo > 5) cx_alvo = 2;
    caixa_definir(&c_ia[cx_alvo], m);
    cx_ativa = cx_alvo;
    for (int i = 0; i < 6; i++) c_ia[i].foco = (i == cx_alvo);
    dizer("Modelo puesto. Pulsa Guardar para que lo usen los programas.");
}

/* El botón del destino del modelo. No se usa el campo "apertado" de los
 * botones para marcar el destino: ese campo lo lleva el ratón (y se gasta con
 * la suelta), así que el destino se dibuja aparte, con su marco azul. */
static void desenhar_alvo(botao_t *b, bool escolhido)
{
    if (!escolhido) { botao_desenhar(jn, b); return; }
    g_retangulo(jn, b->x, b->y, b->l, b->a, COR_BOTAO_APERTADO);
    g_relevo(jn, b->x, b->y, b->l, b->a, true);
    g_moldura(jn, b->x - 1, b->y - 1, b->l + 2, b->a + 2, 0x5A6470);
    int tx = b->x + (b->l - g_largura_texto(b->texto)) / 2;
    int ty = b->y + (b->a - LETRA_A) / 2;
    g_texto(jn, tx, ty, b->texto, COR_TEXTO);
    g_moldura(jn, b->x - 2, b->y - 2, b->l + 4, b->a + 4, COR_SELECAO);
    jan_mostrar_area(jn, b->x - 2, b->y - 2, b->l + 4, b->a + 4);
}

/* El dibujo de la página. */
static void ia_desenhar(void)
{
    char t[200];
    titulo_pag(24, Y_PAG + 6, "Proveedor");
    lista_desenhar(jn, &lista_prov);
    g_texto_n(jn, 24, Y_PAG + 252, "Elige uno y pon su URL.",
               200 / LETRA_L, COR_TEXTO_FRACO);

    g_texto(jn, 250, Y_PAG + 4, "Dirección del servidor (URL)", COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[0]);
    formatar_texto(t, sizeof t, "Clave de API (%s)", chave_ia[0] ? "definida" : "sin clave");
    g_texto(jn, 250, Y_PAG + 42, t, COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[1]);

    for (int i = 0; i < 4; i++) {
        int x = i % 2 == 0 ? 250 : 490;
        int y = Y_PAG + 100 + (i / 2) * 48;
        g_texto(jn, x, y, rotulos_uso[i], COR_TEXTO_FRACO);
        caixa_desenhar(jn, &c_ia[2 + i]);
        /* La casilla de destino se marca con un marco azul: así se ve de dónde
         * a dónde va el modelo que se elige en la lista. */
        if (2 + i == cx_alvo)
            g_moldura(jn, c_ia[2 + i].x - 2, c_ia[2 + i].y - 2, c_ia[2 + i].l + 4, LETRA_A + 12, COR_SELECAO);
    }
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_ia[i]);
    g_texto(jn, 250, Y_PAG + 244, "Modelo para:", COR_TEXTO_FRACO);
    for (int i = 0; i < 4; i++) desenhar_alvo(&b_alvo[i], 2 + i == cx_alvo);
    lista_desenhar(jn, &lista_mod);
}

static void definir_volume_pelo_mouse(int x)
{
    int v = (x - BARRA_X) * 100 / BARRA_L;
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    conf.volume = v;
    conf.silencio = 0;
    aplicar_volume(v, 0);
}

static void eq_mudar(int banda, int db)
{
    db = db < -12 ? -12 : db > 12 ? 12 : db;
    if (conf.eq[banda] == db && conf.eq_def) return;
    conf.eq[banda] = db;
    conf.eq_def = 1;
    eq_aplicar(conf.eq);
}

static void eq_pelo_mouse(int banda, int y)
{
    int db = 12 - ((y - EQ_Y0) * 24 + EQ_ALT / 2) / EQ_ALT;
    eq_mudar(banda, db);
}

static int eq_coluna(const evento_t *e)
{
    for (int i = 0; i < EQ_BANDAS; i++)
        if (dentro(e, EQ_X0 + i * EQ_PASSO - 20, EQ_Y0 - 6, 40, EQ_ALT + 12)) return i;
    return -1;
}

static int janela(void)
{
    jn = jan_abrir("Configuración", LARG, ALT);
    if (!jn) { imprimir("configuracion: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    barra_fundo = progresso_criar(BARRA_FUNDO_X, BARRA_FUNDO_Y, BARRA_FUNDO_L, BARRA_FUNDO_A, false);
    if (barra_fundo) progresso_texto(barra_fundo, true);
    conf_ler();
    persiste = home_persistente();
    /* Cada pestaña es del ancho de su título, para que quepan las siete (D145). */
    int xp = 10;
    for (int i = 0; i < P_QTD; i++) {
        int l = g_largura_texto(titulos_pag[i]) + 16;
        pos_pag[i] = xp;
        larg_pag[i] = l;
        b_pag[i] = (botao_t){ xp, 10, l, 28, titulos_pag[i], false };
        xp += l + 4;
    }
    c_capturas = (caixa_t){ .x = 24, .y = Y_PAG + 66, .l = LARG - 48, .foco = true };
    caixa_definir(&c_capturas, conf.capturas);
    int x = 24;
    for (int i = 0; i < M_QTD; i++) { b_modo[i] = (botao_t){ x, Y_PAG + 118, 90, 28, rotulos_modo[i], false }; x += 94; }
    lista_zona = (lista_t){ 24, Y_PAG + 34, LARG - 48, ALT - Y_PAG - 34 - 112, itens_zona, 0, -1, 0, NULL };
    lista_modo = (lista_t){ 24, Y_PAG + 34, LARG - 48, ALT - Y_PAG - 34 - 112, itens_modo, 0, -1, 0, NULL };
    lista_img = (lista_t){ 24, Y_PAG + 178, LARG - 48, ALT - Y_PAG - 178 - 112, itens_img, 0, -1, 0, NULL };
    /* La página de la IA (D145): las seis cajas, las dos listas y los botones. */
    c_ia[0] = (caixa_t){ .x = 250, .y = Y_PAG + 22, .l = 460 };
    c_ia[1] = (caixa_t){ .x = 250, .y = Y_PAG + 60, .l = 460, .oculta = true };
    c_ia[2] = (caixa_t){ .x = 250, .y = Y_PAG + 118, .l = 220 };
    c_ia[3] = (caixa_t){ .x = 490, .y = Y_PAG + 118, .l = 220 };
    c_ia[4] = (caixa_t){ .x = 250, .y = Y_PAG + 166, .l = 220 };
    c_ia[5] = (caixa_t){ .x = 490, .y = Y_PAG + 166, .l = 220 };
    lista_prov = (lista_t){ 24, Y_PAG + 24, 200, 220, itens_prov, 0, -1, 0, NULL };
    lista_mod = (lista_t){ 250, Y_PAG + 280, 460, 90, itens_mod, 0, -1, 0, NULL };
    b_ia[0].y = Y_PAG + 206;
    b_ia[1].y = Y_PAG + 206;
    for (int i = 0; i < 4; i++) b_alvo[i].y = Y_PAG + 240;
    ia_preparar();
    ler_zonas();
    ler_modos_video();
    listar_imagens();
    if (!persiste) dizer("Sin /home persistente los cambios se aplican, pero no se recordarán.");
    desenhar();
    registrar("configuracion: ventana abierta (persistente %d)", persiste);

    evento_t e;
    for (;;) {
        if (proximo_evento(jn->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TELA) { ler_modos_video(); desenhar(); continue; }
        bool mudou = false;
        if (botao_tratar(jn, &b_cerrar, &e)) break;
        for (int i = 0; i < P_QTD; i++)
            if (botao_tratar(jn, &b_pag[i], &e)) {
                pagina = i;
                registrar("configuracion: pagina %s", titulos_pag[i]);
                if (i == P_TELA) ler_modos_video();
                if (i == P_IA) ia_preparar();   /* relee lo guardado (D145) */
                if (i == P_FUENTES) fuentes_preparar();   /* (D196) */
                mudou = true;
            }
        if (pagina == P_TECLADO) {
            for (int i = 0; i < 2; i++)
                if (botao_tratar(jn, &b_tec[i], &e)) {
                    aplicar_teclado(i);
                    conf.teclado = i;
                    dizer_gravado(conf_gravar());
                    mudou = true;
                }
        } else if (pagina == P_ZONA) {
            int at = lista_tratar(jn, &lista_zona, &e);
            bool ap = botao_tratar(jn, &b_zona, &e) || at == LISTA_ATIVOU;
            if (ap && lista_zona.sel >= 0) {
                int n;
                const zona_info_t *z = zona_tabela(&n);
                const char *nome = z[indice_zona[lista_zona.sel]].nome;
                int r = aplicar_zona(nome);
                if (r < 0) dizer("No se pudo cambiar la zona horaria.");
                else { copia_texto(conf.zona, nome, sizeof conf.zona); dizer_gravado(conf_gravar()); }
                ler_zonas();
                mudou = true;
            } else if (at) mudou = true;
        } else if (pagina == P_TELA) {
            int at = lista_tratar(jn, &lista_modo, &e);
            bool ap = botao_tratar(jn, &b_tela[0], &e) || at == LISTA_ATIVOU;
            bool gu = botao_tratar(jn, &b_tela[1], &e);
            if ((ap || gu) && lista_modo.sel >= 0) {
                modo_video_t *m = &modos[indice_modo_l[lista_modo.sel]];
                bool igual = m->largura == tela.largura && m->altura == tela.altura;
                int op = tela.a_quente && !igual ? MUDAR_AGORA : 0;
                if (gu || !tela.a_quente) op |= MUDAR_GUARDAR;
                if (!op) dizer("La pantalla ya está en esa resolución.");
                else {
                    int r = trocar_modo_video((int)m->largura, (int)m->altura, op);
                    if (r == ERRO_SO_LEITURA) dizer("Arranque desde CD: no se puede guardar la resolución.");
                    else if (r < 0) formatar_texto(estado, sizeof estado, "No se pudo: %s", nome_erro(r));
                    else if (op & MUDAR_AGORA) formatar_texto(estado, sizeof estado, "%ux%u%s", m->largura, m->altura, (r & MUDAR_GUARDAR) ? ", guardada" : "");
                    else formatar_texto(estado, sizeof estado, "%ux%u al reiniciar", m->largura, m->altura);
                }
                ler_modos_video();
                mudou = true;
            } else if (at) mudou = true;
        } else if (pagina == P_SOM) {
            if (botao_tratar(jn, &b_vol[0], &e) || botao_tratar(jn, &b_vol[1], &e)) {
                int v = audio_volume_mestre(-1);
                if (v >= 0) {
                    v += (e.x < BARRA_X ? -5 : 5);
                    v = v < 0 ? 0 : v > 100 ? 100 : v;
                    conf.volume = v; conf.silencio = 0;
                    aplicar_volume(v, 0);
                    dizer_gravado(conf_gravar());
                }
                mudou = true;
            }
            if (botao_tratar(jn, &b_vol[2], &e)) {
                int s = audio_silencio(-1);
                if (s >= 0) {
                    conf.silencio = !s;
                    if (conf.volume < 0) conf.volume = audio_volume_mestre(-1);
                    aplicar_volume(conf.volume, conf.silencio);
                    dizer_gravado(conf_gravar());
                }
                mudou = true;
            }
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, BARRA_X, BARRA_Y, BARRA_L, BARRA_A)) {
                arrastando_vol = true;
                definir_volume_pelo_mouse(e.x);
                mudou = true;
            } else if (e.tipo == EV_MOVER && arrastando_vol && (e.botoes & 1)) {
                definir_volume_pelo_mouse(e.x);
                mudou = true;
            } else if (e.tipo == EV_SOLTOU && arrastando_vol) {
                arrastando_vol = false;
                dizer_gravado(conf_gravar());
                mudou = true;
            }
            {
                int c = -1;
                if (e.tipo == EV_CLIQUE && (e.botoes & 1) && (c = eq_coluna(&e)) >= 0) {
                    eq_arrasta = c;
                    eq_pelo_mouse(c, e.y);
                    mudou = true;
                } else if (e.tipo == EV_MOVER && eq_arrasta >= 0 && (e.botoes & 1)) {
                    eq_pelo_mouse(eq_arrasta, e.y);
                    mudou = true;
                } else if (e.tipo == EV_SOLTOU && eq_arrasta >= 0) {
                    eq_arrasta = -1;
                    dizer_gravado(conf_gravar());
                    mudou = true;
                }
                for (int i = 0; i < 4; i++) {
                    if (!botao_tratar(jn, &b_eq[i], &e)) continue;
                    eq_preajuste(eq_nomes_pre[i], conf.eq);
                    conf.eq_def = 1;
                    eq_aplicar(conf.eq);
                    dizer_gravado(conf_gravar());
                    mudou = true;
                }
            }
            b_vol[2].texto = audio_silencio(-1) > 0 ? "Activar sonido" : "Silenciar";
        } else if (pagina == P_CAPTURAS) {
            bool enter = caixa_tratar(jn, &c_capturas, &e);
            if (enter || botao_tratar(jn, &b_cap[0], &e)) {
                copia_texto(conf.capturas, c_capturas.texto, sizeof conf.capturas);
                dizer_gravado(conf_gravar());
                mudou = true;
            }
            if (botao_tratar(jn, &b_cap[1], &e)) {
                conf.cap_sonido = !conf.cap_sonido;
                dizer_gravado(conf_gravar());
                mudou = true;
            }
            mudou = true;
        } else if (pagina == P_RED) {
            for (int k = 0; k < 2; k++)
                if (botao_tratar(jn, &b_bloq[k], &e)) {
                    info_bloqueo_t b;
                    memset(&b, 0, sizeof b);
                    bloqueo_dns(BLOQ_ESTADO, &b, sizeof b);
                    const char *ordem = k == 1 ? "actualizar" : b.activo ? "desactivar" : "activar";
                    dizer(k == 1 ? "Descargando las listas..." : "Un momento...");
                    desenhar();
                    int r = rodar_bloqueo(ordem);
                    if (k == 1) dizer(r == 0 ? "Listas actualizadas." : "No se pudieron actualizar las listas (¿hay red?).");
                    else if (r < 0) dizer("No se pudo cambiar el bloqueo (mira «bloqueo estado» en el terminal).");
                    else dizer(b.activo ? "Bloqueo desactivado." : "Bloqueo activado.");
                    mudou = true;
                }
        } else if (pagina == P_FUENTES) {
            if (n_fnt > 0) {
                int at = lista_tratar(jn, &lista_fnt, &e);
                if (at) { fuentes_vista(); mudou = true; }
                for (int k = 0; k < 3; k++)
                    if (botao_tratar(jn, &b_fnt[k], &e)) {
                        if (k < 2 && lista_fnt.sel < 0) dizer("Elige una fuente de la lista.");
                        else {
                            char *d = k == 0 ? conf.fuente_doc : conf.fuente_tit;
                            if (k == 2) { conf.fuente_doc[0] = 0; conf.fuente_tit[0] = 0; }
                            else copia_texto(d, cat_fnt[lista_fnt.sel].ruta, 96);
                            dizer_gravado(conf_gravar());
                        }
                        mudou = true;
                    }
            }
        } else if (pagina == P_IA) {
            /* Las casillas de texto (solo una con el foco) y las dos listas. */
            bool mudou_ia = false;
            if (e.tipo == EV_CLIQUE) {
                int i = -1;
                for (int k = 0; k < 6; k++)
                    if (dentro(&e, c_ia[k].x, c_ia[k].y, c_ia[k].l, LETRA_A + 8)) { i = k; break; }
                if (i >= 0) {
                    /* Clic en una casilla: queda como destino del modelo y con el
                     * foco, para poder escribir en ella. */
                    if (i >= 2) cx_alvo = i;
                    cx_ativa = i;
                    lst_ativa = -1;
                    for (int k = 0; k < 6; k++) c_ia[k].foco = (k == i);
                    caixa_tratar(jn, &c_ia[i], &e);
                    mudou_ia = true;
                } else if (dentro(&e, lista_prov.x, lista_prov.y, lista_prov.l, lista_prov.a)) {
                    lst_ativa = 0;
                    cx_ativa = -1;
                    for (int k = 0; k < 6; k++) c_ia[k].foco = false;
                    int at = lista_tratar(jn, &lista_prov, &e);
                    if (at) { ia_escolher_provedor(); mudou_ia = true; }
                } else if (dentro(&e, lista_mod.x, lista_mod.y, lista_mod.l, lista_mod.a)) {
                    lst_ativa = 1;
                    cx_ativa = -1;
                    for (int k = 0; k < 6; k++) c_ia[k].foco = false;
                    int at = lista_tratar(jn, &lista_mod, &e);
                    if (at && lista_mod.sel >= 0) { ia_poner_modelo(itens_mod[lista_mod.sel]); mudou_ia = true; }
                }
            } else if (e.tipo == EV_TECLA) {
                if (cx_ativa >= 0) caixa_tratar(jn, &c_ia[cx_ativa], &e);
                else if (lst_ativa == 0) {
                    int at = lista_tratar(jn, &lista_prov, &e);
                    if (at) { ia_escolher_provedor(); mudou_ia = true; }
                } else if (lst_ativa == 1) {
                    int at = lista_tratar(jn, &lista_mod, &e);
                    if (at && lista_mod.sel >= 0) { ia_poner_modelo(itens_mod[lista_mod.sel]); mudou_ia = true; }
                }
            } else {
                /* La rueda desplaza la lista que tenga el puntero encima. */
                lista_tratar(jn, &lista_prov, &e);
                lista_tratar(jn, &lista_mod, &e);
            }
            /* Los botones que eligen el destino del modelo de la lista. */
            for (int k = 0; k < 4; k++)
                if (botao_tratar(jn, &b_alvo[k], &e)) {
                    cx_alvo = 2 + k;
                    cx_ativa = cx_alvo;
                    lst_ativa = -1;
                    for (int m = 0; m < 6; m++) c_ia[m].foco = (m == cx_alvo);
                    mudou_ia = true;
                }
            if (botao_tratar(jn, &b_ia[0], &e)) {
                ia_recolher();
                int r = ia_config_gravar(&cfg_ia, 1);
                if (r < 0) formatar_texto(estado, sizeof estado, "No se pudo guardar: %s", nome_erro(r));
                else if (!persiste) dizer("Guardado solo en esta sesión: /home no es un disco.");
                else dizer("Guardado en /home/.config/ia. Los programas ya lo usan.");
                mudou_ia = true;
            }
            if (botao_tratar(jn, &b_ia[1], &e)) {
                ia_cargar_modelos();
                mudou_ia = true;
            }
            if (mudou_ia) mudou = true;
        } else {
            if (e.tipo == EV_CLIQUE && (e.botoes & 1))
                for (int i = 0; i < CORES_QTD; i++)
                    if (dentro(&e, SW_X + i * SW_PASSO, SW_Y, SW_L, 30)) {
                        acento_cor((int)cores[i].cor);
                        conf.acento = cores[i].cor;
                        dizer_gravado(conf_gravar());
                        mudou = true;
                    }
            for (int i = 0; i < M_QTD; i++)
                if (botao_tratar(jn, &b_modo[i], &e)) {
                    conf.modo = i;
                    if (i == M_DEGRADE) {
                        fundo_aplicar("", M_DEGRADE);
                        conf.fundo[0] = 0;
                        lista_img.sel = -1;
                        dizer_gravado(conf_gravar());
                    }
                    mudou = true;
                }
            int at = lista_tratar(jn, &lista_img, &e);
            bool ap = botao_tratar(jn, &b_fundo, &e) || at == LISTA_ATIVOU;
            if (ap) {
                if (lista_img.sel < 0) dizer("Elige una imagen de la lista.");
                else {
                    if (conf.modo == M_DEGRADE) conf.modo = M_AJUSTADO;
                    dizer("Aplicando el fondo...");
                    mostrando_barra = true;
                    desenhar();
                    int r = fundo_aplicar(itens_img[lista_img.sel], conf.modo);
                    mostrando_barra = false;
                    if (r < 0) formatar_texto(estado, sizeof estado, "No se pudo: %s", texto_erro(r));
                    else {
                        copia_texto(conf.fundo, itens_img[lista_img.sel], sizeof conf.fundo);
                        dizer_gravado(conf_gravar());
                    }
                }
                mudou = true;
            } else if (at) mudou = true;
        }
        if (mudou) desenhar();
    }
    jan_fechar(jn);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--aplicar") == 0) return aplicar_tudo();
    if (argc >= 2 && compara_texto(argv[1], "--ventana") != 0) return linha_de_comandos(argc, argv);
    return janela();
}
