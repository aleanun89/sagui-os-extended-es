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
 * La sección Mandos (D307) lista los mandos y joysticks, los dibuja en vivo (ejes, botones y
 * hats) y calibra el joystick del puerto de juegos; la calibración va en sagui.conf (clave
 * mando_puerto=, la misma que escribe «mandos --calibrar») y se aplica al arrancar.
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
/* contas.h choca con el conf_gravar propio de este programa: solo lo que hace falta (D269). */
int  elevar_grafico(const char *motivo);
void elevacao_soltar(void);
bool sou_root(void);
#include "imagem.h"
#include "ia.h"
#include "fontes.h"
#include "servicos.h"
#include "captura.h"
#include "vncd_estado.h"
#include "mandos.h"

#define DIR_CONF   "/home/.config"
#define ARQ_CONF   "/home/.config/sagui.conf"
#define NOME_MAX_F 120

/* Modos del fondo. */
enum { M_DEGRADE, M_CENTRADO, M_AJUSTADO, M_RELLENAR, M_ESTIRADO, M_MOSAICO, M_QTD };
static const char *const nomes_modo[M_QTD] = { "degradado", "centrado", "ajustado", "rellenar", "estirado", "mosaico" };
static const char *const rotulos_modo[M_QTD] = { N_("Degradado"), N_("Centrado"), N_("Ajustado"), N_("Rellenar"), N_("Estirado"), N_("Mosaico") };

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
    /* D292: balance (0-200, 100 = centro), latencia (ms) y micrófono; cada grupo vale si su bit de son_def está a 1 */
    int      son_def;                /* 1 balance, 2 latencia, 4 micrófono */
    int      balance, latencia;
    int      mic_ent, mic_gan, mic_mudo;
    char     salida[48];             /* D292: la tarjeta de salida elegida (su nombre); vacía = la de siempre */
    char     mando_cal[96];          /* D307: calibración del joystick del puerto de juegos; vacía = automática */
    char     idioma[16];             /* D313: es, pt_BR, en; vacío = el del sistema (/etc/idioma) o el español */
    /* D323: aceleración gráfica. -1 = la clave no está en el archivo (no se toca nada); 0 automático, 1 sí, 2 no. */
    int      a_2d, a_3d, a_sdl, a_disco;
} conf_t;

static conf_t conf = { -1, -1, 0, "", 0xFFFFFFFF, "", M_AJUSTADO, "", 1, .a_2d = -1, .a_3d = -1, .a_sdl = -1, .a_disco = -1 };

#define CORES_QTD 8
static const struct { const char *nome; uint32_t cor; } cores[CORES_QTD] = {
    { N_("Verde"), 0x0B7A3E }, { N_("Azul"), 0x2A6FD8 }, { N_("Rojo"), 0xC0392B }, { N_("Naranja"), 0xD97706 },
    { N_("Violeta"), 0x7B3FA0 }, { N_("Turquesa"), 0x0E8A8A }, { N_("Rosa"), 0xC2437A }, { N_("Grafito"), 0x4A5560 },
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

/* D323: «auto», «si» o «no» (también sí, yes, on, off, 0 y 1); lo demás vale automático. */
static int tri_texto(const char *v)
{
    if (compara_texto(v, "si") == 0 || compara_texto(v, "sí") == 0 || compara_texto(v, "yes") == 0 ||
        compara_texto(v, "on") == 0 || compara_texto(v, "1") == 0) return 1;
    if (compara_texto(v, "no") == 0 || compara_texto(v, "off") == 0 || compara_texto(v, "0") == 0) return 2;
    return 0;
}

/* Lee el archivo; false = no existe. Las claves desconocidas se ignoran. */
static bool conf_ler(void)
{
    int fd = abrir(ARQ_CONF, ABRIR_LER);
    if (fd < 0) return false;
    static char t[3072];
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
            else if (compara_texto(k, "salida") == 0) copia_texto(conf.salida, v, sizeof conf.salida);
            else if (compara_texto(k, "mando_puerto") == 0) copia_texto(conf.mando_cal, v, sizeof conf.mando_cal);
            else if (compara_texto(k, "balance") == 0) { int x = texto_para_int(v); conf.balance = x < 0 ? 0 : x > 200 ? 200 : x; conf.son_def |= 1; }
            else if (compara_texto(k, "latencia") == 0) { int x = texto_para_int(v); conf.latencia = x < 20 ? 20 : x > 300 ? 300 : x; conf.son_def |= 2; }
            else if (compara_texto(k, "micro_entrada") == 0) { conf.mic_ent = texto_para_int(v); conf.son_def |= 4; }
            else if (compara_texto(k, "micro_ganancia") == 0) { int x = texto_para_int(v); conf.mic_gan = x < 0 ? 0 : x > 100 ? 100 : x; conf.son_def |= 4; }
            else if (compara_texto(k, "micro_silencio") == 0) { conf.mic_mudo = texto_para_int(v) != 0; conf.son_def |= 4; }
            else if (compara_texto(k, "silencio") == 0) conf.silencio = texto_para_int(v) != 0;
            else if (compara_texto(k, "zona") == 0) copia_texto(conf.zona, v, sizeof conf.zona);
            else if (compara_texto(k, "idioma") == 0) idioma_normalizar(v, conf.idioma);
            else if (compara_texto(k, "acento") == 0) { if (ler_cor(v, &c)) conf.acento = c; }
            else if (compara_texto(k, "fondo") == 0) copia_texto(conf.fundo, v, sizeof conf.fundo);
            else if (compara_texto(k, "capturas_carpeta") == 0) copia_texto(conf.capturas, v, sizeof conf.capturas);
            else if (compara_texto(k, "fuente_documentos") == 0) copia_texto(conf.fuente_doc, v, sizeof conf.fuente_doc);
            else if (compara_texto(k, "fuente_titulos") == 0) copia_texto(conf.fuente_tit, v, sizeof conf.fuente_tit);
            else if (compara_texto(k, "capturas_sonido") == 0) conf.cap_sonido = texto_para_int(v) != 0;
            else if (compara_texto(k, "fondo_modo") == 0) { int m = indice_modo(v); if (m >= 0) conf.modo = m; }
            else if (compara_texto(k, "acel2d") == 0) conf.a_2d = tri_texto(v);
            else if (compara_texto(k, "gpu3d") == 0) conf.a_3d = tri_texto(v);
            else if (compara_texto(k, "sdl2_gpu") == 0) conf.a_sdl = tri_texto(v);
            else if (compara_texto(k, "disco_dma_cache") == 0) conf.a_disco = tri_texto(v);
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
    char t[2048];
    char cap[NOME_MAX_F + 60];
    formatar_texto(cap, sizeof cap, "capturas_carpeta=%s\ncapturas_sonido=%d\n", conf.capturas, conf.cap_sonido);
    char tec[24] = "";
    if (conf.teclado >= 0) formatar_texto(tec, sizeof tec, "teclado=%s\n", conf.teclado == TECLADO_ABNT2 ? "abnt2" : "es");
    char vol[48] = "";
    if (conf.volume >= 0) formatar_texto(vol, sizeof vol, "volumen=%d\nsilencio=%d\n", conf.volume, conf.silencio);
    char ecu[48] = "";
    if (conf.eq_def) formatar_texto(ecu, sizeof ecu, "ecualizador=%d,%d,%d,%d,%d\n", conf.eq[0], conf.eq[1], conf.eq[2], conf.eq[3], conf.eq[4]);
    char son[400] = "";
    {
        int u = 0;
        if (conf.salida[0]) u += formatar_texto(son + u, sizeof son - (unsigned)u, "salida=%s\n", conf.salida);
        if (conf.son_def & 1) u += formatar_texto(son + u, sizeof son - (unsigned)u, "balance=%d\n", conf.balance);
        if (conf.son_def & 2) u += formatar_texto(son + u, sizeof son - (unsigned)u, "latencia=%d\n", conf.latencia);
        if (conf.son_def & 4)
            formatar_texto(son + u, sizeof son - (unsigned)u, "micro_entrada=%d\nmicro_ganancia=%d\nmicro_silencio=%d\n",
                           conf.mic_ent, conf.mic_gan, conf.mic_mudo);
    }
    if (conf.mando_cal[0]) {   /* D307: va con el sonido (mismo búfer, sitio de sobra) */
        size_t l = tam_texto(son);
        formatar_texto(son + l, sizeof son - l, "mando_puerto=%s\n", conf.mando_cal);
    }
    char ace2[160] = "";   /* D323: la aceleración gráfica */
    {
        static const char *const nt[3] = { "auto", "si", "no" };
        int u = 0;
        if (conf.a_2d >= 0) u += formatar_texto(ace2 + u, sizeof ace2 - (unsigned)u, "acel2d=%s\n", nt[conf.a_2d]);
        if (conf.a_3d >= 0) u += formatar_texto(ace2 + u, sizeof ace2 - (unsigned)u, "gpu3d=%s\n", nt[conf.a_3d]);
        if (conf.a_sdl >= 0) u += formatar_texto(ace2 + u, sizeof ace2 - (unsigned)u, "sdl2_gpu=%s\n", nt[conf.a_sdl]);
        if (conf.a_disco >= 0) formatar_texto(ace2 + u, sizeof ace2 - (unsigned)u, "disco_dma_cache=%s\n", nt[conf.a_disco]);
    }
    char zon[80] = "";
    if (conf.zona[0]) formatar_texto(zon, sizeof zon, "zona=%s\n", conf.zona);
    char idi[32] = "";
    if (conf.idioma[0]) formatar_texto(idi, sizeof idi, "idioma=%s\n", conf.idioma);
    char ace[24] = "";
    if (conf.acento != 0xFFFFFFFFu) formatar_texto(ace, sizeof ace, "acento=%06x\n", conf.acento);
    char fun[NOME_MAX_F + 40] = "";
    if (conf.fundo[0]) formatar_texto(fun, sizeof fun, "fondo=%s\nfondo_modo=%s\n", conf.fundo, nomes_modo[conf.modo]);
    formatar_texto(t, sizeof t,
                   "# Configuración de SaguiOS (la escribe el programa \"configuracion\"; D112).\n"
                   "# El kernel la aplica al arrancar con \"configuracion --aplicar\".\n%s%s%s%s%s%s%s%s%s%s%s",
                   tec, vol, ecu, son, zon, ace, fun, cap, fnt, idi, ace2);
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

/* D292: balance, latencia y micrófono (entrada, ganancia y silencio). */
static int aplicar_balance(int b) { return audio_controle(-1, AUDIO_BALANCE, b) < 0 ? ERRO_NAO_EXISTE : 0; }
static int aplicar_latencia(int ms) { return audio_controle(-1, AUDIO_LATENCIA, ms) < 0 ? ERRO_NAO_EXISTE : 0; }
static int aplicar_micro(int ent, int gan, int mudo)
{
    info_captura_t i;
    if (captura_estado(CAP_FUENTE_REAL, &i) < 0 || !i.presente) return ERRO_NAO_EXISTE;
    if (ent >= 0 && (unsigned)ent < i.n_entradas) captura_entrada((unsigned)ent);
    return captura_ganancia(gan < 0 ? i.ganancia : (unsigned)gan, mudo != 0) < 0 ? ERRO_INVALIDO : 0;
}

/* Pone como salida la tarjeta de ese nombre (si está enchufada). */
static int aplicar_salida(const char *nome)
{
    info_audio_t ia;
    if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) return ERRO_NAO_EXISTE;
    for (int k = 0; k < AUDIO_SALIDAS; k++)
        if (ia.salidas[k].usada && compara_texto(ia.salidas[k].nombre, nome) == 0) return audio_salida(k) < 0 ? ERRO_INVALIDO : 0;
    return ERRO_NAO_EXISTE;
}

/* sonido --aplicar: los ajustes de conectores guardados para este modelo (D292, mejora 075 punto 9). */
static void aplicar_conectores(void)
{
    info_arquivo_t ia;
    if (info_arquivo("/home/.config/sonido.conf", &ia) < 0) return;
    char *av[] = { "sonido", "--aplicar", NULL };
    int pid = executar("/bin/sonido", av), codigo = 0;
    if (pid < 0) return;
    esperar(pid, &codigo);
    registrar("configuracion: ajustes de conectores (sonido --aplicar) -> %d", codigo);
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
    /* D269: «zona» cambia la hora del sistema (solo root): el hijo hereda el permiso que dé el diálogo. */
    if (!sou_root() && elevar_grafico(_("cambiar la zona horaria del sistema")) != 0) return ERRO_ACESSO;
    char *av[] = { "zona", (char *)nome, NULL };
    int pid = executar("/bin/zona", av);
    if (pid < 0) { elevacao_soltar(); return pid; }
    int codigo = 0;
    esperar(pid, &codigo);
    elevacao_soltar();
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

/* Lo de la aceleración va antes de aplicar_tudo, y la orden y la ventana más abajo (necesitan aviso_persistencia). */
/* ------------------------------------------------------------ aceleración gráfica (D323) */

#define TRI_AUTO 0
#define TRI_SI   1
#define TRI_NO   2
#define CAB_OFF_ACEL2D 135     /* bytes de la cabecera de arranque (kernel/inc/bootinfo.h) */
#define CAB_OFF_DISCO  143
#define CAB_DISCO_BITS 3       /* CAB_DISCO_SEM_DMA | CAB_DISCO_SEM_CACHE */
#define OPC_ACEL_SEGURA 0x10000   /* boot_info.opcoes: Mayús al arrancar */
#define ARQ_MARCA  "/home/.config/acel-arranque"   /* existe mientras el arranque no llegó al escritorio */
#define ARQ_AVISO  "/tmp/acel-aviso"               /* lo que dijo el modo seguro (la ventana lo enseña) */
#define ARQ_SDL    "/tmp/acel-sdl2"                /* «1» o «0»: lo lee la SDL 2 (sdl2gpu.h) */

static const char *const nomes_tri[3] = { "auto", "si", "no" };
static const char *const rotulos_tri[3] = { "Automático", "Activado", "Desactivado" };

static bool contem(const char *t, const char *p)
{
    size_t n = tam_texto(p);
    for (; *t; t++)
        if (compara_texto_n(t, p, n) == 0) return true;
    return false;
}

/* Cambia bits de la cabecera de arranque del disco de arranque (solo administrador). < 0: no se pudo
 * (CD, sin disco de arranque, sin permiso): no pasa nada, vale lo que se aplicó en la sesión. */
static int cab_gravar(int desl, int mascara, int valor)
{
    return info_sistema(INFO_CAB_OPC, (void *)(long)CAB_OPC_PEDIDO(desl, mascara, valor), 0);
}

static bool acel_seguro_pedido(void)
{
    info_motor_gpu_t m;
    memset(&m, 0, sizeof m);
    return info_sistema(INFO_MOTOR_GPU, &m, sizeof m) == 0 && (m.boot_opcoes & OPC_ACEL_SEGURA) != 0;
}

/* El estado que enseñan la ventana y «configuracion aceleracion». */
static info_acel2d_t ac2;
static info_gpu3d_t  ac3;
static void acel_ler_estado(void)
{
    memset(&ac2, 0, sizeof ac2);
    memset(&ac3, 0, sizeof ac3);
    info_sistema(INFO_ACEL2D, &ac2, sizeof ac2);
    info_sistema(INFO_GPU3D, &ac3, sizeof ac3);
}

/* Tarjetas con 3D sin probar en hardware (D284, D287): R5xx y R7xx. */
static bool acel_3d_experimental(void) { return contem(ac3.placa, "R5xx") || contem(ac3.placa, "R7xx"); }
static bool acel_3d_hay_driver(void) { return ac3.estado != GPU3D_SEM_GPU && ac3.estado != GPU3D_DESATIVADO; }

static const char *acel_texto_2d(void)
{
    if (ac2.ativo) return "activo (la GPU copia y rellena)";
    if (ac2.apagado == 2) return "apagado a mano (la CPU hace todo)";
    if (ac2.apagado == 1) return "apagado por el vigilante: la GPU no respondió";
    if ((ac2.modo & 0x0F) == 1 || (ac2.modo & 0x80)) return "desactivado (la CPU hace todo)";
    return "CPU (sin acelerador o la autoprueba falló)";
}

static const char *acel_texto_3d(void)
{
    switch (ac3.estado) {
    case GPU3D_SIN_PROBAR:  return "disponible, sin probar (se prueba con el primer uso)";
    case GPU3D_ATIVO:       return "activo";
    case GPU3D_FALHOU:      return "falló: SaguiGL va por software";
    case GPU3D_APAGADO:     return "apagado (SaguiGL por software)";
    case GPU3D_DESATIVADO:  return "desactivado en el arranque (software)";
    default:                return "sin driver 3D para esta tarjeta (software)";
    }
}

/* 2D: aplica la elección ahora. Devuelve 0 bien, < 0 no se pudo; "res" dice qué pasó. */
static int acel_aplicar_2d(int t, char *res, size_t n)
{
    acel_ler_estado();
    int r = 0;
    if (t == TRI_NO) {
        if ((ac2.modo & 0x0F) != 1) cab_gravar(CAB_OFF_ACEL2D, 0x0F, 1);   /* también para los próximos arranques */
        if (ac2.ativo) r = info_sistema(INFO_ACEL2D_CTL, (void *)0L, 0);
    } else {
        bool estaba_cpu = (ac2.modo & 0x0F) == 1;
        if (estaba_cpu) cab_gravar(CAB_OFF_ACEL2D, 0x0F, 0);
        /* Automático solo repone lo que Configuración apagó; Activado lo reintenta siempre. */
        if (!ac2.ativo && (t == TRI_SI || ac2.apagado == 2 || estaba_cpu)) r = info_sistema(INFO_ACEL2D_CTL, (void *)1L, 0);
    }
    acel_ler_estado();
    formatar_texto(res, n, "2D: %s", acel_texto_2d());
    if (t == TRI_SI && !ac2.ativo) r = -1;
    return r < 0 ? -1 : 0;
}

static int acel_aplicar_3d(int t, char *res, size_t n)
{
    acel_ler_estado();
    int r = 0;
    if (!acel_3d_hay_driver()) {
        formatar_texto(res, n, "3D: %s", acel_texto_3d());
        return t == TRI_SI ? -1 : 0;
    }
    if (t == TRI_NO) {
        if (ac3.estado != GPU3D_APAGADO) r = info_sistema(INFO_GPU3D_CTL, (void *)0L, 0);
    } else if (t == TRI_SI) {
        if (ac3.estado != GPU3D_ATIVO) r = info_sistema(INFO_GPU3D_CTL, (void *)1L, 0);   /* enciende y hace la autoprueba */
    } else if (acel_3d_experimental()) {
        if (ac3.estado == GPU3D_ATIVO || ac3.estado == GPU3D_SIN_PROBAR) r = info_sistema(INFO_GPU3D_CTL, (void *)0L, 0);
    } else if (ac3.estado == GPU3D_APAGADO) {
        r = info_sistema(INFO_GPU3D_CTL, (void *)1L, 0);
    }
    acel_ler_estado();
    formatar_texto(res, n, "3D: %s", acel_texto_3d());
    if (t == TRI_SI && ac3.estado != GPU3D_ATIVO) r = -1;
    return r < 0 ? -1 : 0;
}

/* SDL 2: /tmp/acel-sdl2 (la lee la biblioteca al crear un renderizador). */
static int acel_aplicar_sdl(int t)
{
    if (t == TRI_AUTO) { apagar(ARQ_SDL); return 0; }
    int fd = abrir(ARQ_SDL, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    escrever_tudo(fd, t == TRI_SI ? "1" : "0", 1);
    fechar(fd);
    return 0;
}

/* Disco (D281): DMA y caché solo se deciden al arrancar: va a la cabecera. */
static int acel_aplicar_disco(int t)
{
    return cab_gravar(CAB_OFF_DISCO, CAB_DISCO_BITS, t == TRI_NO ? CAB_DISCO_BITS : 0);
}

/* Lo que se pide con la configuración, sin la marca de arranque. */
static void acel_aplicar_conf(void)
{
    char r[200];
    if (conf.a_2d >= 0) { int q = acel_aplicar_2d(conf.a_2d, r, sizeof r); registrar("configuracion: acel2d %s -> %d (%s)", nomes_tri[conf.a_2d], q, r); }
    if (conf.a_3d >= 0) { int q = acel_aplicar_3d(conf.a_3d, r, sizeof r); registrar("configuracion: gpu3d %s -> %d (%s)", nomes_tri[conf.a_3d], q, r); }
    if (conf.a_sdl >= 0) registrar("configuracion: sdl2_gpu %s -> %d", nomes_tri[conf.a_sdl], acel_aplicar_sdl(conf.a_sdl));
    if (conf.a_disco >= 0) registrar("configuracion: disco dma/cache %s -> %d", nomes_tri[conf.a_disco], acel_aplicar_disco(conf.a_disco));
}

static void acel_aviso(const char *t)
{
    int fd = abrir(ARQ_AVISO, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever_tudo(fd, t, (unsigned)tam_texto(t)); fechar(fd); }
}

/* Restablece los valores seguros: todo en automático (y la cabecera del disco también). */
static void acel_restablecer(void)
{
    conf.a_2d = conf.a_3d = conf.a_sdl = conf.a_disco = TRI_AUTO;
    char r[200];
    acel_aplicar_2d(TRI_AUTO, r, sizeof r);
    acel_aplicar_3d(TRI_AUTO, r, sizeof r);
    acel_aplicar_sdl(TRI_AUTO);
    acel_aplicar_disco(TRI_AUTO);
}

/* El arranque (D323). Pasos:
 *  - Mayús al arrancar (o --acel-segura): el kernel ya dejó la 2D por la CPU y el 3D por software; aquí no se
 *    aplica nada y se avisa. La configuración guardada no se toca.
 *  - La marca de arranque sigue ahí: el arranque anterior no llegó al escritorio con la aceleración forzada a
 *    mano. Se restablecen los valores seguros (en el archivo y en la cabecera), se avisa y se sigue.
 *  - Si no, se pone la marca, se aplica lo guardado y, al llegar aquí (el escritorio ya está), se borra. */
static void acel_arranque(bool tem)
{
    if (acel_seguro_pedido()) {
        acel_aviso("Modo seguro de la aceleración (Mayús al arrancar): 2D por la CPU, 3D por software. La configuración guardada no se ha aplicado.");
        registrar("configuracion: aceleracion en MODO SEGURO pedido al arrancar: no se aplica la configuracion");
        return;
    }
    int fd = abrir(ARQ_MARCA, ABRIR_LER);
    if (fd >= 0) {
        fechar(fd);
        apagar(ARQ_MARCA);
        acel_aviso("Arranque anterior sin llegar al escritorio: valores seguros restablecidos (todo en automático).");
        registrar("configuracion: la marca de arranque seguia puesta: valores seguros de la aceleracion");
        if (tem) {
            acel_restablecer();
            conf_gravar();
        }
        return;
    }
    if (conf.a_2d < 0 && conf.a_3d < 0 && conf.a_sdl < 0 && conf.a_disco < 0) return;
    bool riesgo = conf.a_2d == TRI_SI || conf.a_3d == TRI_SI;
    if (riesgo) {
        criar_pasta(DIR_CONF);
        int fm = abrir(ARQ_MARCA, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fm >= 0) { escrever_tudo(fm, "1\n", 2); fechar(fm); }
    }
    acel_aplicar_conf();
    if (riesgo) apagar(ARQ_MARCA);
    registrar("configuracion: aceleracion aplicada, marca de arranque borrada");
}


/* Lo que hace el arranque. Los errores van al log; nada de esto debe detener el sistema. */
static int aplicar_tudo(int argc, char **argv)
{
    aplicar_conectores();
    bool tem_conf = conf_ler();
    acel_arranque(tem_conf);   /* D323: modo seguro, marca de arranque y aceleración (el kernel nos lanza como root) */
    /* D313: el idioma del usuario (sagui.conf) manda sobre el del sistema (/etc/idioma). Aunque no haya
     * sagui.conf, el del sistema se aplica: el kernel traduce su interfaz con él. */
    if (!conf.idioma[0]) idioma_sistema_ler(conf.idioma, sizeof conf.idioma);
    if (conf.idioma[0]) registrar("configuracion: idioma %s -> %d", conf.idioma, idioma_fijar(conf.idioma));
    if (argc >= 4 && sou_root()) {   /* y ahora, a la identidad de la sesión: el resto va con los permisos de siempre */
        int uid = texto_para_int(argv[2]), gid = texto_para_int(argv[3]);
        if (uid > 0) registrar("configuracion: bajo a uid %d -> %d", uid, usuario_op(USU_FIJAR, uid, gid));
    }
    if (!tem_conf) { registrar("configuracion: sin %s, nada que aplicar", ARQ_CONF); return 0; }
    if (conf.salida[0]) registrar("configuracion: salida '%s' -> %d", conf.salida, aplicar_salida(conf.salida));
    if (conf.teclado >= 0) registrar("configuracion: teclado %d -> %d", conf.teclado, aplicar_teclado(conf.teclado));
    if (conf.volume >= 0) registrar("configuracion: volumen %d silencio %d -> %d", conf.volume, conf.silencio,
                                    aplicar_volume(conf.volume, conf.silencio));
    if (conf.son_def & 1) registrar("configuracion: balance %d -> %d", conf.balance, aplicar_balance(conf.balance));
    if (conf.son_def & 2) registrar("configuracion: latencia %d ms -> %d", conf.latencia, aplicar_latencia(conf.latencia));
    if (conf.son_def & 4) registrar("configuracion: micro entrada %d ganancia %d silencio %d -> %d", conf.mic_ent, conf.mic_gan,
                                    conf.mic_mudo, aplicar_micro(conf.mic_ent, conf.mic_gan, conf.mic_mudo));
    if (conf.eq_def) registrar("configuracion: ecualizador %d,%d,%d,%d,%d -> %d", conf.eq[0], conf.eq[1], conf.eq[2],
                               conf.eq[3], conf.eq[4], eq_aplicar(conf.eq));
    if (conf.mando_cal[0]) {   /* D307: la calibración del joystick del puerto de juegos */
        unsigned short c[4][3];
        int r = mandos_cal_de_texto(conf.mando_cal, c);
        if (r == 0) r = mandos_calibrar(c);
        registrar("configuracion: mando_puerto %s -> %d", conf.mando_cal, r);
    }
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

/* Autopruebas del botón «Probar ahora» y de «configuracion aceleracion 2d|3d probar». 1 bien, 0 mal, < 0 sin acelerador. */
static int acel_probar_2d(char *res, size_t n)
{
    info_acel2d_prova_t r;
    memset(&r, 0, sizeof r);
    if (info_sistema(INFO_ACEL2D_PROVA, &r, sizeof r) != 0) { copia_texto(res, "el kernel no tiene la autoprueba", n); return -2; }
    if (r.resultado == 1) formatar_texto(res, n, "BIEN: %u órdenes iguales a la CPU; GPU %u us, CPU %u us", r.ordens, r.us_gpu, r.us_cpu);
    else if (r.resultado == 0) formatar_texto(res, n, "MAL: %u píxeles distintos (%s)", r.diferentes, r.texto);
    else copia_texto(res, r.texto, n)   /* el kernel ya antepone «sin acelerador: » */;
    return r.resultado;
}

static int acel_probar_3d(char *res, size_t n)
{
    info_gpu3d_prova_t r;
    memset(&r, 0, sizeof r);
    if (info_sistema(INFO_GPU3D_PROVA, &r, sizeof r) != 0) { copia_texto(res, "el kernel no tiene la autoprueba", n); return -2; }
    if (r.resultado == 1) formatar_texto(res, n, "BIEN: puntuación 3D %u (%u triángulos/s)", r.pontos, r.tri_s);
    else if (r.resultado == 0) formatar_texto(res, n, "MAL: %s (se apaga el 3D)", r.texto);
    else copia_texto(res, r.texto, n)   /* el kernel ya antepone «sin 3D por GPU: » */;
    return r.resultado;
}

bool falta_admin(const char *programa);

/* configuracion aceleracion [2d|3d|sdl|disco [auto|si|no|probar]] | restablecer | --simular-cuelgue (D323) */
static void acel_mostrar(void)
{
    acel_ler_estado();
    imprimir("Aceleración gráfica\n");
    if (ac2.modelo) imprimir("  Tarjeta:   %04x:%04x%s%s\n", ac2.fabricante, ac2.modelo, ac3.placa[0] ? ", 3D: " : "", ac3.placa[0] ? ac3.placa : "");
    else imprimir("  Tarjeta:   %s\n", ac3.placa[0] ? ac3.placa : "sin acelerador reconocido");
    imprimir("  2D (%s): %s\n", ac2.nome, acel_texto_2d());
    imprimir("  3D (%s): %s\n", ac3.nome, acel_texto_3d());
    if (ac3.pontos) imprimir("  Puntuación 3D: %u\n", ac3.pontos);
    if (acel_3d_experimental()) imprimir("  Aviso: el 3D de esta tarjeta es experimental (sin probar en hardware)\n");
    conf_ler();
    imprimir("  Elegido:   2D %s, 3D %s, SDL 2 %s, disco (DMA y caché) %s\n",
             conf.a_2d < 0 ? "auto" : nomes_tri[conf.a_2d], conf.a_3d < 0 ? "auto" : nomes_tri[conf.a_3d],
             conf.a_sdl < 0 ? "auto" : nomes_tri[conf.a_sdl], conf.a_disco < 0 ? "auto" : nomes_tri[conf.a_disco]);
    char av[200];
    int fd = abrir(ARQ_AVISO, ABRIR_LER);
    if (fd >= 0) {
        int n = ler(fd, av, sizeof av - 1);
        fechar(fd);
        if (n > 0) { av[n] = 0; imprimir("  Último aviso del arranque: %s\n", av); }
    }
}

static int acel_cli(int argc, char **argv)
{
    if (argc < 3) { acel_mostrar(); return 0; }
    const char *que = argv[2];
    char res[200];
    if (compara_texto(que, "--simular-cuelgue") == 0) {   /* para las pruebas: como un arranque que no llegó al escritorio */
        if (falta_admin("configuracion")) return 1;
        criar_pasta(DIR_CONF);
        int fm = abrir(ARQ_MARCA, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fm < 0) return 1;
        escrever_tudo(fm, "1\n", 2);
        fechar(fm);
        imprimir("Marca de arranque puesta: el próximo arranque volverá a los valores seguros.\n");
        return 0;
    }
    if (compara_texto(que, "restablecer") == 0) {
        if (falta_admin("configuracion")) return 1;
        conf_ler();
        acel_restablecer();
        imprimir("Valores seguros restablecidos (todo en automático).\n");
        aviso_persistencia(conf_gravar());
        acel_mostrar();
        return 0;
    }
    int k = compara_texto(que, "2d") == 0 ? 0 : compara_texto(que, "3d") == 0 ? 1 : compara_texto(que, "sdl") == 0 ? 2
          : compara_texto(que, "disco") == 0 ? 3 : -1;
    if (k < 0 || argc < 4) {
        imprimir_em(ERROS, "uso: configuracion aceleracion [2d|3d|sdl [auto|si|no|probar] | disco [auto|no] | restablecer]\n");
        return 2;
    }
    const char *v = argv[3];
    if (compara_texto(v, "probar") == 0) {
        if (k > 1) { imprimir_em(ERROS, "configuracion: solo 2d y 3d se pueden probar\n"); return 2; }
        if (falta_admin("configuracion")) return 1;
        int p = k == 0 ? acel_probar_2d(res, sizeof res) : acel_probar_3d(res, sizeof res);
        imprimir("%s probar: %s\n", k == 0 ? "2d" : "3d", res);
        return p == 1 ? 0 : 1;
    }
    int t = compara_texto(v, "auto") == 0 ? TRI_AUTO : compara_texto(v, "si") == 0 || compara_texto(v, "sí") == 0 ? TRI_SI
          : compara_texto(v, "no") == 0 ? TRI_NO : -1;
    if (t < 0 || (k == 3 && t == TRI_SI)) {
        imprimir_em(ERROS, "configuracion: el valor es auto, si o no%s\n", k == 3 ? " (en disco, solo auto o no)" : "");
        return 2;
    }
    if (k != 2 && falta_admin("configuracion")) return 1;   /* la SDL 2 no toca la GPU: no pide permiso */
    conf_ler();
    int r = 0;
    if (k == 0) { conf.a_2d = t; r = acel_aplicar_2d(t, res, sizeof res); }
    else if (k == 1) { conf.a_3d = t; r = acel_aplicar_3d(t, res, sizeof res); }
    else if (k == 2) { conf.a_sdl = t; r = acel_aplicar_sdl(t); formatar_texto(res, sizeof res, "SDL 2 por la GPU: %s", rotulos_tri[t]); }
    else { conf.a_disco = t; r = acel_aplicar_disco(t); formatar_texto(res, sizeof res, "disco: %s (se aplica al reiniciar)%s", rotulos_tri[t], r < 0 ? "; no se pudo grabar en la cabecera de arranque" : ""); if (r < 0) r = 0; }
    imprimir("%s\n", res);
    if (r < 0) imprimir("Aviso: no quedó activado%s.\n", k == 1 && acel_3d_experimental() ? " (3D experimental: no pasó la autoprueba)" : "");
    aviso_persistencia(conf_gravar());
    return r < 0 ? 1 : 0;
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
    if (v >= 0) {
        imprimir("Volumen:   %d %%%s\n", v, audio_silencio(-1) > 0 ? " (silencio)" : "");
        info_audio_t ia;
        if (info_sistema(INFO_AUDIO, &ia, sizeof ia) == 0)
            imprimir("Balance:   %d (-100 izquierda, 100 derecha); latencia %u ms\n", (int)ia.balance - 100, ia.latencia_base_ms);
        info_captura_t ic;
        if (captura_estado(CAP_FUENTE_REAL, &ic) == 0 && ic.presente)
            imprimir("Micrófono: %s, ganancia %u %%%s\n", ic.entradas[ic.entrada < CAP_ENTRADAS_MAX ? ic.entrada : 0], ic.ganancia, ic.mudo ? " (en silencio)" : "");
    }
    config_zona_t z;
    ler_zona(&z);
    imprimir("Zona:      %s\n", z.nome);
    imprimir("Idioma:    %s\n", idioma_actual());
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
    if (compara_texto(c, "aceleracion") == 0 || compara_texto(c, "acel") == 0) return acel_cli(argc, argv);
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
    } else if (compara_texto(c, "salida") == 0 && argc == 3) {
        info_audio_t ia;
        int k = texto_para_int(argv[2]);
        if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) { imprimir_em(ERROS, "configuracion: no hay tarjeta de sonido\n"); return 1; }
        if (k < 0 || k >= AUDIO_SALIDAS || !ia.salidas[k].usada) { imprimir_em(ERROS, "configuracion: no existe la salida %s (volumen salida)\n", argv[2]); return 1; }
        r = audio_salida(k) < 0 ? ERRO_INVALIDO : 0;
        copia_texto(conf.salida, ia.salidas[k].nombre, sizeof conf.salida);
    } else if (compara_texto(c, "balance") == 0 && argc == 3) {
        int b = texto_para_int(argv[2]);   /* -100 (izquierda) a +100 (derecha); 0 = centro */
        b = b < -100 ? -100 : b > 100 ? 100 : b;
        conf.balance = b + 100;
        conf.son_def |= 1;
        r = aplicar_balance(conf.balance);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no hay tarjeta de sonido\n"); return 1; }
    } else if (compara_texto(c, "latencia") == 0 && argc == 3) {
        int ms = texto_para_int(argv[2]);
        conf.latencia = ms < 20 ? 20 : ms > 300 ? 300 : ms;
        conf.son_def |= 2;
        r = aplicar_latencia(conf.latencia);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no hay tarjeta de sonido\n"); return 1; }
    } else if (compara_texto(c, "microfono") == 0 && argc >= 3) {
        info_captura_t i;
        if (captura_estado(CAP_FUENTE_REAL, &i) < 0 || !i.presente) { imprimir_em(ERROS, "configuracion: no hay entrada de sonido (micrófono)\n"); return 1; }
        if (!(conf.son_def & 4)) { conf.mic_ent = (int)i.entrada; conf.mic_gan = (int)i.ganancia; conf.mic_mudo = i.mudo != 0; conf.son_def |= 4; }
        if (compara_texto(argv[2], "entrada") == 0 && argc == 4) conf.mic_ent = texto_para_int(argv[3]);
        else if (compara_texto(argv[2], "ganancia") == 0 && argc == 4) { int g = texto_para_int(argv[3]); conf.mic_gan = g < 0 ? 0 : g > 100 ? 100 : g; }
        else if (compara_texto(argv[2], "silencio") == 0) conf.mic_mudo = 1;
        else if (compara_texto(argv[2], "activar") == 0) conf.mic_mudo = 0;
        else { imprimir_em(ERROS, "configuracion: microfono entrada N | ganancia 0-100 | silencio | activar\n"); return 2; }
        if (conf.mic_ent < 0 || (unsigned)conf.mic_ent >= i.n_entradas) { imprimir_em(ERROS, "configuracion: entrada %d inexistente (grabar --info)\n", conf.mic_ent); return 1; }
        r = aplicar_micro(conf.mic_ent, conf.mic_gan, conf.mic_mudo);
    } else if (compara_texto(c, "zona") == 0 && argc == 3) {
        r = aplicar_zona(argv[2]);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no se pudo poner la zona %s\n", argv[2]); return 1; }
        copia_texto(conf.zona, argv[2], sizeof conf.zona);
    } else if (compara_texto(c, "idioma") == 0 && (argc == 3 || (argc == 4 && compara_texto(argv[3], "--sistema") == 0))) {
        char norm[16];
        idioma_normalizar(argv[2], norm);
        if (argc == 4) {   /* el idioma del sistema (/etc/idioma): solo root */
            int g = idioma_sistema_gravar(norm);
            if (g < 0) { imprimir_em(ERROS, "configuracion: idioma del sistema: %s\n", nome_erro(g)); return 1; }
            imprimir("Idioma del sistema: %s (se aplica en el próximo arranque)\n", norm);
            return 0;
        }
        copia_texto(conf.idioma, norm, sizeof conf.idioma);
        r = idioma_fijar(norm);
        if (r < 0) { imprimir_em(ERROS, "configuracion: no se pudo cambiar el idioma: %s\n", nome_erro(r)); return 1; }
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
        imprimir_em(ERROS, "uso: configuracion [--aplicar | mostrar | teclado es|abnt2 | idioma es|pt_BR|en [--sistema] | volumen N | zona NOMBRE |\n"
                           "                      salida N | balance -100..100 | latencia MS | microfono entrada N|ganancia N|silencio|activar |\n"
                           "                      acento RRGGBB | fondo ARCHIVO [modo] | fondo ninguno |\n"
                           "                      fuente documentos|titulos RUTA|ninguna | aceleracion [2d|3d|sdl|disco VALOR | restablecer]]\n");
        return 2;
    }
    aviso_persistencia(conf_gravar());
    return 0;
}

/* ------------------------------------------------------------ la ventana */

#define LARG 788   /* cabe en 800x600 (issue #43); las pestañas se aprietan solas si no caben */
#define ALT  500
#define Y_PAG 50
enum { P_TECLADO, P_ZONA, P_TELA, P_SOM, P_ESCRITORIO, P_CAPTURAS, P_IA, P_FUENTES, P_RED, P_VNC, P_MANDOS, P_QTD };
static const char *const titulos_pag[P_QTD] = { N_("Teclado"), N_("Fecha y hora"), N_("Pantalla"), N_("Sonido"), N_("Escritorio"), N_("Capturas"), N_("IA"), N_("Fuentes"), N_("Red"), N_("Escritorio remoto"), N_("Mandos") };

static int pagina;
static char estado[160];
static bool persiste;

static botao_t b_pag[P_QTD];
static int  pos_pag[P_QTD];      /* cada pestaña es del ancho de su título (D145) */
static int  larg_pag[P_QTD];
static caixa_t c_capturas;   /* carpeta de las capturas (D125) */
static botao_t b_cap[2] = { { 24, Y_PAG + 112, 120, 28, N_("Aplicar"), false }, { 24, Y_PAG + 170, 260, 28, "", false } };
static botao_t b_cerrar = { LARG - 110, ALT - 40, 96, 28, N_("Cerrar"), false };

/* Teclado */
/* D313: el idioma de la interfaz se elige en la misma página (la barra de pestañas no admite una más sin cambiar el ancho). */
static botao_t b_idi[3] = { { 24, Y_PAG + 226, 200, 30, "Español", false },
                            { 234, Y_PAG + 226, 200, 30, "Português (Brasil)", false },
                            { 444, Y_PAG + 226, 200, 30, "English", false } };
static const char *const codigos_idi[3] = { "es", "pt_BR", "en" };
static botao_t b_tec[2] = { { 24, Y_PAG + 56, 260, 30, N_("Español (ES)"), false },
                            { 24, Y_PAG + 96, 260, 30, N_("Portugués de Brasil (ABNT2)"), false } };

/* Fecha y hora */
#define MAXZ 128
static char itens_zona[MAXZ][64];
static int  indice_zona[MAXZ];
static lista_t lista_zona;
static botao_t b_zona = { 24, ALT - 100, 120, 28, N_("Aplicar"), false };

/* Pantalla */
#define MAXM 192
static modo_video_t modos[MAXM];
static int qtd_modos;
static char itens_modo[MAXM][64];
static int indice_modo_l[MAXM];
static lista_t lista_modo;
static botao_t b_tela[2] = { { 24, ALT - 100, 120, 28, N_("Aplicar"), false }, { 154, ALT - 100, 180, 28, N_("Aplicar y guardar"), false } };
static info_tela_t tela;
/* D290: varias pantallas. Solo se ven si el kernel tiene más de una salida de vídeo. */
static info_telas_t telas;
static int sel_res;   /* la pantalla a la que se aplica la resolución de la lista */
static char rot_prin[32], rot_lado[32], rot_res[40];
static botao_t b_telas[5] = { { 24, 0, 100, 28, N_("Extender"), false }, { 128, 0, 90, 28, N_("Espejo"), false },
                              { 222, 0, 120, 28, rot_prin, false }, { 346, 0, 150, 28, rot_lado, false },
                              { 500, 0, 190, 28, rot_res, false } };

/* Sonido */
#define BARRA_X 60
#define BARRA_Y (Y_PAG + 70)
#define BARRA_L 400
#define BARRA_A 24
static botao_t b_vol[3] = { { 24, BARRA_Y, 28, 24, "-", false }, { BARRA_X + BARRA_L + 8, BARRA_Y, 28, 24, "+", false },
                            { 24, BARRA_Y + 50, 140, 28, N_("Silenciar"), false } };
static bool arrastando_vol;
/* Ecualizador (D201): cinco deslizadores verticales de -12 a +12 dB y los preajustes. */
#define EQ_Y0   (Y_PAG + 250)     /* arriba del carril: +12 dB (250 y no 290: los rótulos de las bandas pisaban el aviso de /home, v7.4) */
#define EQ_ALT  96                /* alto del carril: 4 px por dB */
#define EQ_X0   60
#define EQ_PASSO 84
static const char *const eq_rotulos[EQ_BANDAS] = { "60 Hz", "250 Hz", "1 kHz", "4 kHz", "12 kHz" };
static const char *const eq_nomes_pre[4] = { "plano", "graves", "voz", "agudos" };
static botao_t b_eq[4] = { { 500, EQ_Y0 - 14, 150, 26, N_("Plano"), false }, { 500, EQ_Y0 + 16, 150, 26, N_("Graves"), false },
                           { 500, EQ_Y0 + 46, 150, 26, N_("Voz"), false }, { 500, EQ_Y0 + 76, 150, 26, N_("Agudos"), false } };
static int eq_arrasta = -1;

/* D292: balance, pruebas de altavoz, latencia y micrófono. */
#define BAL_X 250
#define BAL_Y (BARRA_Y + 50)
#define BAL_L 160
#define BAL_A 24
static botao_t b_bal = { 420, BARRA_Y + 50, 76, 28, N_("Centro"), false };
static botao_t b_prob[2] = { { 24, BARRA_Y + 86, 110, 26, N_("Probar izq."), false }, { 142, BARRA_Y + 86, 110, 26, N_("Probar der."), false } };
static botao_t b_lat[2] = { { 270, BARRA_Y + 86, 24, 26, "-", false }, { 440, BARRA_Y + 86, 24, 26, "+", false } };
#define MIC_X 540
#define MIC_GX 568
#define MIC_GL 148
static botao_t b_mic[5] = { { MIC_X, Y_PAG + 38, 24, 24, "<", false }, { 732, Y_PAG + 38, 24, 24, ">", false },
                            { MIC_X, Y_PAG + 70, 24, 24, "-", false }, { 722, Y_PAG + 70, 24, 24, "+", false },
                            { MIC_X, Y_PAG + 146, 216, 26, N_("Silenciar micrófono"), false } };
#define MIC_MY (Y_PAG + 102)
static botao_t b_sal = { 24, BARRA_Y + 116, 472, 24, "", false };
static bool arrastando_bal, arrastando_mic;
static info_captura_t mic_inf;
static bool mic_aberto;

static void mic_cerrar(void)
{
    if (mic_aberto) captura_cerrar();
    mic_aberto = false;
}

static void mic_refrescar(void)
{
    if (!mic_aberto) return;
    static uint8_t tmp[4096];
    while (captura_ler(tmp, sizeof tmp) > 0) { }   /* se tira: solo importa el medidor */
    captura_estado(CAP_FUENTE_REAL, &mic_inf);
}

/* Al entrar en Sonido: abre la captura para el medidor en vivo (si hay micrófono y nadie más lo usa). */
static void mic_abrir(void)
{
    mic_cerrar();
    memset(&mic_inf, 0, sizeof mic_inf);
    if (captura_estado(CAP_FUENTE_REAL, &mic_inf) < 0 || !mic_inf.presente) return;
    if (captura_abrir(CAP_FUENTE_REAL, 48000, 1) >= 0) mic_aberto = true;
    captura_estado(CAP_FUENTE_REAL, &mic_inf);
}

/* seno aproximado (Bhaskara I) en grados, escala 32767 */
static int seno_g(unsigned g)
{
    g %= 360;
    int sg = 1;
    if (g >= 180) { g -= 180; sg = -1; }
    int x = (int)g * (180 - (int)g);
    return sg * (32767 * 4 * x / (40500 - x));
}

/* Prueba del altavoz: 1 kHz medio segundo solo por el canal izquierdo o el derecho. */
static void probar_lado(bool esq)
{
    enum { TAXA = 48000, N = 24000 };
    static int16_t buf[2 * 2400];
    int fd = audio_abrir(TAXA, 2, 16);
    if (fd < 0) return;
    unsigned fase = 0;
    for (int feito = 0; feito < N; feito += 2400) {
        for (int k = 0; k < 2400; k++) {
            int v = seno_g(fase / 1000) / 3;
            buf[2 * k] = (int16_t)(esq ? v : 0);
            buf[2 * k + 1] = (int16_t)(esq ? 0 : v);
            fase = (fase + 1000u * 360000u / TAXA) % 360000u;
        }
        audio_escrever(fd, buf, sizeof buf);
    }
    audio_fechar(fd, 1);
}

/* Escritorio */
#define SW_X 24
#define SW_Y (Y_PAG + 28)
#define SW_L 56
#define SW_PASSO 70
static botao_t b_modo[M_QTD];
#define MAXF 64
static char itens_img[MAXF][64];
static lista_t lista_img;
static botao_t b_fundo = { 24, ALT - 100, 140, 28, N_("Aplicar fondo"), false };

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
static botao_t b_fnt[3] = { { FN_VX, Y_PAG + 276, 150, 28, N_("Para documentos"), false },
                            { FN_VX + 158, Y_PAG + 276, 120, 28, N_("Para títulos"), false },
                            { FN_VX + 286, Y_PAG + 276, 90, 28, N_("Quitar"), false } };
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
        titulo_pag(24, Y_PAG + 14, _("No hay fuentes instaladas."));
        {
            char b[160];
            formatar_texto(b, sizeof b, _("Se buscan ficheros .otf y .ttf en %s y en %s."), FUENTES_DIR_APPS, FUENTES_DIR_HOME);
            g_texto(jn, 24, Y_PAG + 40, b, COR_TEXTO_FRACO);
        }
        g_texto(jn, 24, Y_PAG + 60, _("Las de Typodermic (CC0) vienen en la partición SAGUI-APPS."), COR_TEXTO_FRACO);
        return;
    }
    formatar_texto(t, sizeof t, _("%d fuentes (%s, %s)"), n_fnt, FUENTES_DIR_APPS, FUENTES_DIR_HOME);
    titulo_pag(24, Y_PAG + 8, t);
    lista_desenhar(jn, &lista_fnt);
    g_retangulo(jn, FN_VX, FN_VY, FN_VL, FN_VA, 0xFFFFFF);
    g_moldura(jn, FN_VX, FN_VY, FN_VL, FN_VA, COR_BORDA);
    if (vista_fnt) {
        int y = vista_parrafo(vista_fnt, FN_VX + 8, FN_VY + 6, FN_VL - 16, FN_VY + FN_VA - 4, TXT_MUESTRA, 26);
        vista_parrafo(vista_fnt, FN_VX + 8, y + 6, FN_VL - 16, FN_VY + FN_VA - 4, TXT_CIFRAS, 22);
    } else g_texto(jn, FN_VX + 8, FN_VY + 8, _("No se pudo cargar esta fuente."), 0xB03000);
    formatar_texto(t, sizeof t, _("Documentos: %s"), conf.fuente_doc[0] ? nombre_ruta(conf.fuente_doc) : _("(ninguna)"));
    g_texto_n(jn, FN_VX, Y_PAG + FN_VA + 36, t, FN_VL / LETRA_L, COR_TEXTO);
    formatar_texto(t, sizeof t, _("Títulos:     %s"), conf.fuente_tit[0] ? nombre_ruta(conf.fuente_tit) : _("(ninguna)"));
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
static const char *const rotulos_uso[4] = { N_("Modelo por omisión"), N_("Terminal"), N_("Chat"), N_("Mico") };
static ia_config_t cfg_ia;
/* La clave de API entera: la caja solo cabe 127 caracteres, así que la clave se
 * guarda aparte y la caja solo la enseña con asteriscos (D145). */
static char chave_ia[IA_CHAVE_MAX];
static char chave_vista[128];
static int  cx_ativa = -1;    /* la casilla con el foco del teclado (-1 = ninguna) */
static int  cx_alvo = 2;      /* la casilla donde va el modelo de la lista (2 = la general) */
static int  lst_ativa = -1;   /* la lista con el foco: 0 proveedores, 1 modelos */
static botao_t b_ia[2] = { { 250, 0, 120, 28, N_("Guardar"), false },
                           { 380, 0, 170, 28, N_("Cargar modelos"), false } };
/* Los botones que eligen dónde va el modelo que se elige en la lista. */
static botao_t b_alvo[4] = { { 345, 0, 88, 28, N_("General"), false },
                             { 437, 0, 88, 28, N_("Terminal"), false },
                             { 529, 0, 88, 28, N_("Chat"), false },
                             { 621, 0, 88, 28, N_("Mico"), false } };

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
                       (m->flags & MODO_ATUAL) ? _("  (actual)") : "", (m->flags & MODO_A_QUENTE) ? "" : _("  (al reiniciar)"));
        if (m->flags & MODO_ATUAL) lista_modo.sel = n;
        indice_modo_l[n++] = i;
    }
    lista_modo.qtd = n;
    memset(&telas, 0, sizeof telas);
    if (telas_sis(TELAS_OP_INFO, (unsigned)&telas, sizeof telas) < 0) telas.qtd = 0;
    if (sel_res >= (int)telas.qtd) sel_res = 0;
    /* Con varias pantallas, una fila de botones encima de Aplicar: la lista se acorta. */
    lista_modo.a = ALT - Y_PAG - 34 - 112 - (telas.qtd > 1 ? 38 : 0);
    formatar_texto(rot_prin, sizeof rot_prin, _("Principal: %u"), telas.principal + 1);
    formatar_texto(rot_lado, sizeof rot_lado, _("Lado: %s"), telas.lado == TELAS_LADO_ABAIXO ? _("abajo") : _("derecha"));
    formatar_texto(rot_res, sizeof rot_res, _("Resolución de: pantalla %d"), sel_res + 1);
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
    if (g == 1) dizer(_("Aplicado. /home no es persistente: no se recordará al reiniciar."));
    else if (g < 0) formatar_texto(estado, sizeof estado, _("Aplicado, pero no se pudo guardar: %s"), nome_erro(g));
    else dizer(_("Aplicado y guardado en /home/.config/sagui.conf."));
}

static void titulo_pag(int x, int y, const char *t)
{
    g_texto(jn, x, y, t, COR_TEXTO);
    g_texto(jn, x + 1, y, t, COR_TEXTO);
}

/* Red (D218): el bloqueo de publicidad y rastreo por DNS. Los botones llaman
 * al programa «bloqueo», que es quien compila la tabla y escribe bloqueo.conf. */
static botao_t b_bloq[2] = { { 24, Y_PAG + 100, 160, 28, N_("Activar"), false },
                             { 194, Y_PAG + 100, 180, 28, N_("Actualizar listas"), false } };

static int rodar_bloqueo(const char *ordem)
{
    /* D269: el bloqueo de publicidad es del sistema: permiso con el diálogo para este hijo. */
    if (!sou_root() && elevar_grafico(_("cambiar el bloqueo de publicidad y rastreadores")) != 0) return ERRO_ACESSO;
    char *av[] = { "bloqueo", (char *)ordem, NULL };
    int pid = executar("/bin/bloqueo", av);
    if (pid < 0) { elevacao_soltar(); return pid; }
    int codigo = 0;
    esperar(pid, &codigo);
    elevacao_soltar();
    return codigo == 0 ? 0 : ERRO_INVALIDO;
}

static void red_desenhar(void)
{
    char t[160];
    config_rede_t c;
    memset(&c, 0, sizeof c);
    char a[16], d[16];
    if (config_rede(REDE_CONSULTAR, &c) == 0 && c.ip)
        formatar_texto(t, sizeof t, _("Red: IP %s, DNS %s"), ip_para_texto(c.ip, a), c.dns ? ip_para_texto(c.dns, d) : _("ninguno"));
    else copia_texto(t, _("Red: sin dirección IP"), sizeof t);
    g_texto(jn, 24, Y_PAG + 8, t, COR_TEXTO_FRACO);
    titulo_pag(24, Y_PAG + 36, _("Bloqueo de publicidad y rastreo por DNS"));
    info_bloqueo_t b;
    memset(&b, 0, sizeof b);
    bloqueo_dns(BLOQ_ESTADO, &b, sizeof b);
    if (b.activo) formatar_texto(t, sizeof t, _("Activado: %u dominios bloqueados, %u permitidos (%u KB)."),
                                 b.bloqueados, b.permitidos, (b.bytes + 1023) / 1024);
    else copia_texto(t, b.pedido ? _("Desactivado: no hay tabla (pulsa Actualizar listas).") : _("Desactivado: se resuelven todos los nombres."), sizeof t);
    g_texto(jn, 24, Y_PAG + 64, t, COR_TEXTO);
    b_bloq[0].texto = b.activo ? _("Desactivar") : _("Activar");
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_bloq[i]);
    formatar_texto(t, sizeof t, _("Consultas: %u   Bloqueadas: %u"), b.consultas, b.bloqueadas);
    g_texto(jn, 24, Y_PAG + 144, t, COR_TEXTO);
    static char u[6][64];
    int n = bloqueo_dns(BLOQ_ULTIMOS, u, 6);
    if (n > 0) {
        g_texto(jn, 24, Y_PAG + 170, _("Últimos bloqueados:"), COR_TEXTO_FRACO);
        for (int i = 0; i < n; i++) g_texto_n(jn, 40, Y_PAG + 190 + i * 18, u[i], (LARG - 80) / LETRA_L, COR_TEXTO);
    }
    g_texto_n(jn, 24, ALT - 110, _("Los dominios de las listas responden 0.0.0.0 a todos los programas."), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    g_texto_n(jn, 24, ALT - 92, _("Permitir o bloquear a mano, y el servidor para la red: bloqueo --ayuda, dnsbloqueo."), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
}

/* Escritorio remoto (D273, mejora 104): el servidor VNC `vncd`. Lo de root se hace llamando a
 * «vncd --ajustar/--clave/--desconectar» y «servicio» con el diálogo de permisos (D269): la contraseña
 * pasa por la línea de órdenes del hijo, que es root solo mientras dura. */
static botao_t b_vnc[5] = {
    { 24, Y_PAG + 58, 250, 28, N_("Activar y arrancar"), false },
    { 284, Y_PAG + 58, 130, 28, N_("Reiniciar"), false },
    { 294, Y_PAG + 122, 170, 28, N_("Fijar contraseña"), false },
    { 24, Y_PAG + 170, 250, 28, "", false },
    { 284, Y_PAG + 170, 400, 28, "", false },
};
static botao_t b_vnc_des = { 0, Y_PAG + 238, 150, 28, N_("Desconectar"), false };
static caixa_t c_vnc_clave;
static char itens_vnc[VNCD_MAX_SES][64];
static lista_t lista_vnc;
static vnc_estado_t vnc_est;
static bool vnc_hay_estado, vnc_en_marcha, vnc_arrancar, vnc_clave_puesta, vnc_conf_red, vnc_conf_solo_ver;
static int  vnc_conf_puerto = 5900;
static int  vnc_pids[VNCD_MAX_SES];

static void vnc_conf_ler(const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return;
    static char t[1024];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *eq = acha_char(p, '=');
        if (p[0] != '#' && eq) {
            *eq = 0;
            const char *v = eq + 1;
            size_t tv = tam_texto(v);
            while (tv && (v[tv - 1] == '\r' || v[tv - 1] == ' ')) ((char *)v)[--tv] = 0;
            if (compara_texto(p, "escuchar") == 0) vnc_conf_red = compara_texto(v, "red-local") == 0;
            else if (compara_texto(p, "solo_ver") == 0) vnc_conf_solo_ver = compara_texto(v, "si") == 0 || compara_texto(v, "1") == 0;
            else if (compara_texto(p, "puerto") == 0 && texto_para_int(v) > 0) vnc_conf_puerto = texto_para_int(v);
        }
        if (!fim) break;
        p = fim + 1;
    }
}

/* ¿Hay un archivo con ese nombre en la carpeta? (se mira el listado: /etc/vnc/vncd.passwd no se puede abrir como sagui) */
static bool vnc_existe(const char *pasta, const char *nombre)
{
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) == 1; i++)
        if (compara_texto(e.nome, nombre) == 0) return true;
    return false;
}

static void vnc_refrescar(void)
{
    servico_info_t sv[32];
    int n = servicos_ler_estado(sv, 32);
    vnc_en_marcha = false;
    vnc_arrancar = false;
    for (int i = 0; i < n && i < 32; i++)
        if (compara_texto(sv[i].nome, "vncd") == 0) {
            vnc_en_marcha = compara_texto(sv[i].estado, SERV_E_MARCHA) == 0;
            vnc_arrancar = sv[i].arrancar;
        }
    vnc_hay_estado = vnc_en_marcha && vncd_estado_ler(&vnc_est);
    if (!vnc_hay_estado) memset(&vnc_est, 0, sizeof vnc_est);
    vnc_conf_red = vnc_conf_solo_ver = false;
    vnc_conf_puerto = 5900;
    vnc_conf_ler("/etc/vnc/vncd.conf");
    vnc_conf_ler("/home/.config/vncd.conf");
    vnc_clave_puesta = vnc_existe("/etc/vnc", "vncd.passwd") || vnc_existe("/home/.config", "vncd.passwd");
    int sel_pid = lista_vnc.sel >= 0 && lista_vnc.sel < lista_vnc.qtd ? vnc_pids[lista_vnc.sel] : -1;
    lista_vnc.qtd = vnc_est.n;
    lista_vnc.sel = -1;
    for (int i = 0; i < vnc_est.n; i++) {
        unsigned s = vnc_est.s[i].seg;
        formatar_texto(itens_vnc[i], 64, _("%s - hace %u:%02u%s"), vnc_est.s[i].ip, s / 60, s % 60, vnc_est.s[i].solo_ver ? _(" - solo ver") : "");
        vnc_pids[i] = vnc_est.s[i].pid;
        if (vnc_est.s[i].pid == sel_pid) lista_vnc.sel = i;
    }
    b_vnc[0].texto = vnc_arrancar || vnc_en_marcha ? _("Desactivar y parar") : _("Activar y arrancar");
    b_vnc[3].texto = vnc_conf_solo_ver ? _("Solo ver: sí") : _("Solo ver: no");
    b_vnc[4].texto = vnc_conf_red ? _("Escuchar: red local (sin cifrar)") : _("Escuchar: solo esta máquina (por SSH)");
}

/* Ejecuta un programa del sistema y devuelve su código de salida (< 0: no se pudo lanzar). */
static int vnc_ejecutar(const char *prog, char *const *av)
{
    int pid = executar(prog, av);
    if (pid < 0) return pid;
    int codigo = 0;
    esperar(pid, &codigo);
    return codigo;
}

static int vnc_servicio(const char *orden)
{
    char *av[] = { "servicio", (char *)orden, "vncd", NULL };
    return vnc_ejecutar("/bin/servicio", av);
}

static bool vnc_permiso(const char *motivo)
{
    if (sou_root()) return true;
    if (elevar_grafico(motivo) == 0) return true;
    dizer(_("Sin permisos de administrador no se puede cambiar el escritorio remoto."));
    return false;
}

static void vnc_reiniciar_si_corre(void)
{
    if (vnc_en_marcha) vnc_servicio("reiniciar");
}

static void vnc_accion(int k)
{
    if (!vnc_permiso(_("configurar el escritorio remoto (servidor VNC)"))) return;
    char t[80];
    if (k == 0) {
        bool apagar_ = vnc_arrancar || vnc_en_marcha;
        if (apagar_) {
            vnc_servicio("desactivar");
            vnc_servicio("parar");
            dizer(_("Escritorio remoto desactivado y parado."));
        } else if (!vnc_clave_puesta) {
            dizer(_("Antes de activarlo, pon una contraseña (nadie podría entrar sin ella)."));
        } else {
            vnc_servicio("activar");
            int r = vnc_servicio("iniciar");
            dizer(r == 0 ? _("Escritorio remoto activado: arrancará solo y ya está en marcha.") : _("Activado, pero no se pudo iniciar (mira /logs/vncd.log)."));
        }
    } else if (k == 1) {
        dizer(vnc_servicio("reiniciar") == 0 ? _("Servicio reiniciado.") : _("No se pudo reiniciar (¿está activado?)."));
    } else if (k == 2) {
        if (!c_vnc_clave.texto[0]) dizer(_("Escribe la contraseña en el cuadro."));
        else {
            char *av[] = { "vncd", "--clave", c_vnc_clave.texto, NULL };
            int r = vnc_ejecutar("/bin/vncd", av);
            if (r == 0) {
                formatar_texto(t, sizeof t, _("Contraseña fijada%s."), tam_texto(c_vnc_clave.texto) > 8 ? _(" (solo cuentan los 8 primeros caracteres)") : "");
                dizer(t);
                caixa_definir(&c_vnc_clave, "");
            } else dizer(_("No se pudo fijar la contraseña."));
        }
    } else {
        const char *opc = k == 3 ? (vnc_conf_solo_ver ? "solo_ver=no" : "solo_ver=si")
                                 : (vnc_conf_red ? "escuchar=local" : "escuchar=red-local");
        char *av[] = { "vncd", "--ajustar", (char *)opc, NULL };
        if (vnc_ejecutar("/bin/vncd", av) == 0) {
            vnc_reiniciar_si_corre();
            formatar_texto(t, sizeof t, _("Guardado: %s%s."), opc, vnc_en_marcha ? _(" (servicio reiniciado)") : "");
            dizer(t);
        } else dizer(_("No se pudo guardar la opción."));
    }
    elevacao_soltar();
}

static void vnc_desconectar_sel(void)
{
    if (lista_vnc.sel < 0 || lista_vnc.sel >= lista_vnc.qtd) { dizer(_("Elige a alguien de la lista.")); return; }
    if (!vnc_permiso(_("desconectar a alguien del escritorio remoto"))) return;
    char pid[16];
    formatar_texto(pid, sizeof pid, "%d", vnc_pids[lista_vnc.sel]);
    char *av[] = { "vncd", "--desconectar", pid, NULL };
    dizer(vnc_ejecutar("/bin/vncd", av) == 0 ? _("Sesión desconectada.") : _("No se pudo desconectar."));
    elevacao_soltar();
}

static void vnc_desenhar(void)
{
    char t[200];
    titulo_pag(24, Y_PAG + 8, _("Servidor de escritorio remoto (VNC)"));
    formatar_texto(t, sizeof t, _("Servicio vncd: %s%s, puerto %d, arranque automático: %s"), vnc_en_marcha ? _("en marcha") : _("parado"),
                   vnc_en_marcha && !vnc_hay_estado ? _(" (iniciando)") : "", vnc_conf_puerto, vnc_arrancar ? _("sí") : _("no"));
    g_texto_n(jn, 24, Y_PAG + 34, t, (LARG - 48) / LETRA_L, COR_TEXTO);
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_vnc[i]);
    formatar_texto(t, sizeof t, _("Contraseña (como mucho 8 caracteres): %s"), vnc_clave_puesta ? _("ya hay una puesta") : _("SIN PONER"));
    g_texto_n(jn, 24, Y_PAG + 102, t, (LARG - 48) / LETRA_L, COR_TEXTO);
    caixa_desenhar(jn, &c_vnc_clave);
    botao_desenhar(jn, &b_vnc[2]);
    botao_desenhar(jn, &b_vnc[3]);
    botao_desenhar(jn, &b_vnc[4]);
    formatar_texto(t, sizeof t, _("Conectados ahora: %d"), vnc_est.n);
    titulo_pag(24, Y_PAG + 216, t);
    lista_desenhar(jn, &lista_vnc);
    b_vnc_des.x = lista_vnc.x + lista_vnc.l + 10;
    botao_desenhar(jn, &b_vnc_des);
    g_texto_n(jn, 24, ALT - 116, _("«Solo esta máquina»: se entra por un túnel SSH (ssh -L 5900:localhost:5900 root@IP) y el"), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    g_texto_n(jn, 24, ALT - 98, _("tráfico va cifrado. «Red local» deja entrar sin túnel, pero el tráfico VNC no se cifra."), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    g_texto_n(jn, 24, ALT - 80, _("Mientras haya alguien conectado se ve una ventana «Escritorio remoto» en la barra de tareas."), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
}

/* ------------------------------------------------------- la página de los mandos (D307) */

#define MD_X0   380                 /* la prueba en vivo, a la derecha de la lista */
#define MD_CX   96                  /* lado del cuadro de cada par de ejes */
static info_mando_t md_l[MANDOS_MAX];
static int md_n;
static unsigned md_gen = 0xFFFFFFFFu;
static char itens_md[MANDOS_MAX][64];
static lista_t lista_md;
static botao_t b_md[3] = { { 24, Y_PAG + 208, 160, 28, N_("Buscar joystick"), false },
                           { 190, Y_PAG + 208, 150, 28, N_("Calibrar puerto"), false },
                           { 24, Y_PAG + 242, 316, 28, N_("Calibración automática"), false } };
static estado_mando_t md_est;
static unsigned short md_crudo[4];
static int md_fase;                 /* calibración: 0 nada, 1 esquinas, 2 centro */
static unsigned md_fim;
static unsigned short md_cal[4][3];
static unsigned md_soma[4], md_q;

/* Vuelve a leer la lista si se ha conectado o desconectado algo. true = cambió. */
static bool md_listar(void)
{
    unsigned gen = 0;
    int n = mandos_listar(md_l, MANDOS_MAX, &gen);
    if (n < 0) n = 0;
    if (gen == md_gen && n == md_n) return false;
    md_gen = gen;
    md_n = n;
    for (int i = 0; i < n; i++) formatar_texto(itens_md[i], 64, "%s", md_l[i].nome);
    lista_md.qtd = n;
    if (lista_md.sel >= n) lista_md.sel = n - 1;
    if (lista_md.sel < 0 && n > 0) lista_md.sel = 0;
    return true;
}

static int md_puerto(void)
{
    for (int i = 0; i < md_n; i++) if (md_l[i].tipo == MANDO_TIPO_PUERTO) return i;
    return -1;
}

/* Lee el mando elegido; true = el estado cambió (hay que repintar). */
static bool md_ler(void)
{
    if (lista_md.sel < 0 || lista_md.sel >= md_n) return false;
    estado_mando_t e;
    unsigned short c[4];
    if (mando_ler_crudo(lista_md.sel, &e, c) < 0) return false;
    unsigned inf = e.informes;
    e.informes = md_est.informes;
    bool mudou = memcmp(&e, &md_est, sizeof e) != 0 || memcmp(c, md_crudo, sizeof c) != 0;
    md_est = e;
    md_est.informes = inf;
    memcpy(md_crudo, c, sizeof c);
    return mudou;
}

static void md_calibrar_fim(void)
{
    bool ok = false;
    for (int i = 0; i < 4; i++) {
        md_cal[i][1] = md_q ? (unsigned short)(md_soma[i] / md_q) : 0;
        if (md_cal[i][0] == 0xFFFF || !(md_cal[i][0] < md_cal[i][1] && md_cal[i][1] < md_cal[i][2])) {
            md_cal[i][0] = md_cal[i][1] = md_cal[i][2] = 0;
            continue;
        }
        ok = true;
    }
    md_fase = 0;
    if (!ok) { dizer(_("No se ha movido ningún eje: la calibración no cambia.")); return; }
    mandos_calibrar(md_cal);
    mandos_cal_texto(md_cal, conf.mando_cal, sizeof conf.mando_cal);
    registrar("configuracion: mando_puerto calibrado %s", conf.mando_cal);
    dizer_gravado(conf_gravar());
}

/* Cada 50 ms en la página: la lista, el estado y la calibración. true = repintar. */
static bool md_tique(void)
{
    bool r = md_listar();
    if (md_fase) {
        int k = md_puerto();
        if (k < 0) { md_fase = 0; dizer(_("El joystick del puerto de juegos ha desaparecido.")); return true; }
        estado_mando_t e;
        unsigned short t[4];
        if (mando_ler_crudo(k, &e, t) == 0)
            for (int i = 0; i < 4; i++) {
                if (t[i] == 0xFFFF) continue;
                if (md_fase == 1) {
                    if (t[i] < md_cal[i][0]) md_cal[i][0] = t[i];
                    if (t[i] > md_cal[i][2]) md_cal[i][2] = t[i];
                } else if (i == 0) md_q++;
                if (md_fase == 2) md_soma[i] += t[i];
            }
        int falta = (int)(md_fim - tempo_ms());
        if (falta <= 0) {
            if (md_fase == 1) {
                md_fase = 2;
                md_fim = tempo_ms() + 2500;
                for (int i = 0; i < 4; i++) md_soma[i] = 0;
                md_q = 0;
                dizer(_("Ahora suelta el joystick en el centro..."));
            } else md_calibrar_fim();
        } else if (md_fase == 1) formatar_texto(estado, sizeof estado, _("Mueve el joystick hasta las cuatro esquinas, varias veces (%d s)"), falta / 1000 + 1);
        r = true;
    }
    if (md_ler()) r = true;
    return r;
}

static void md_preparar(void)
{
    lista_md = (lista_t){ 24, Y_PAG + 30, MD_X0 - 24 - 40, 170, itens_md, 0, -1, 0, NULL };
    md_gen = 0xFFFFFFFFu;
    md_n = -1;
    md_listar();
    memset(&md_est, 0, sizeof md_est);
    md_ler();
}

static void md_hat(int x, int y, unsigned h)
{
    static const int dx[5] = { 1, 1, 2, 1, 0 }, dy[5] = { 1, 0, 1, 2, 1 };
    static const unsigned bit[5] = { 0, MANDO_HAT_CIMA, MANDO_HAT_DIREITA, MANDO_HAT_BAIXO, MANDO_HAT_ESQUERDA };
    for (int i = 0; i < 5; i++) {
        bool on = i == 0 ? h == 0 : (h & bit[i]) != 0;
        g_retangulo(jn, x + dx[i] * 15, y + dy[i] * 15, 14, 14, on ? 0x2A6FD8 : 0xE4E4E4);
        g_moldura(jn, x + dx[i] * 15, y + dy[i] * 15, 14, 14, COR_BORDA);
    }
}

static void md_desenhar(void)
{
    char t[160];
    titulo_pag(24, Y_PAG + 8, _("Mandos y joysticks conectados"));
    if (md_n <= 0) g_texto(jn, 30, Y_PAG + 40, _("Ninguno (USB, Xbox 360 o puerto de juegos)."), COR_TEXTO_FRACO);
    lista_desenhar(jn, &lista_md);
    for (int i = 0; i < 3; i++) botao_desenhar(jn, &b_md[i]);
    g_texto(jn, 24, Y_PAG + 282, _("El joystick del puerto de juegos (0x201)"), COR_TEXTO_FRACO);
    g_texto(jn, 24, Y_PAG + 300, _("se busca al arrancar y con «Buscar»."), COR_TEXTO_FRACO);
    if (lista_md.sel < 0 || lista_md.sel >= md_n) return;
    const info_mando_t *m = &md_l[lista_md.sel];
    titulo_pag(MD_X0, Y_PAG + 8, _("Prueba en vivo"));
    if (m->fabricante || m->produto)
        formatar_texto(t, sizeof t, _("%s, %04x:%04x: %u ejes, %u botones, %u hats"), m->via, m->fabricante, m->produto,
                       m->n_eixos, m->n_botoes, m->n_hats);
    else formatar_texto(t, sizeof t, _("%s: %u ejes, %u botones, %u hats"), m->via, m->n_eixos, m->n_botoes, m->n_hats);
    g_texto_n(jn, MD_X0, Y_PAG + 28, t, (LARG - MD_X0 - 20) / LETRA_L, COR_TEXTO_FRACO);
    /* Ejes de dos en dos: un cuadro con un punto (X a la derecha, Y hacia abajo). El Xbox 360 tiene
     * el orden de SDL (LX LY LT RX RY RT): se dibujan los sticks y después los gatillos. */
    static const signed char pares_xbox[3][2] = { { 0, 1 }, { 3, 4 }, { 2, 5 } };
    static const char *const nomes_xbox[3] = { N_("Stick izq."), N_("Stick der."), N_("Gatillos") };
    bool xbox = m->mapa == MANDO_MAPA_XBOX360 && m->n_eixos >= 6;
    int pares = xbox ? 3 : (m->n_eixos + 1) / 2;
    if (pares > 4) pares = 4;
    for (int p = 0; p < pares; p++) {
        int x = MD_X0 + p * (MD_CX + 12), y = Y_PAG + 48;
        int ax = xbox ? pares_xbox[p][0] : 2 * p, ay = xbox ? pares_xbox[p][1] : 2 * p + 1;
        g_retangulo(jn, x, y, MD_CX, MD_CX, 0xFFFFFF);
        g_moldura(jn, x, y, MD_CX, MD_CX, COR_BORDA);
        g_retangulo(jn, x + MD_CX / 2, y + 2, 1, MD_CX - 4, 0xD0D0D0);
        g_retangulo(jn, x + 2, y + MD_CX / 2, MD_CX - 4, 1, 0xD0D0D0);
        int ex = md_est.eixos[ax];
        int ey = ay < m->n_eixos ? md_est.eixos[ay] : 0;
        int px = x + MD_CX / 2 + ex * (MD_CX / 2 - 6) / 32768, py = y + MD_CX / 2 + ey * (MD_CX / 2 - 6) / 32768;
        g_retangulo(jn, px - 4, py - 4, 9, 9, 0xC0392B);
        if (xbox) copia_texto(t, _(nomes_xbox[p]), sizeof t);
        else if (ay < m->n_eixos) formatar_texto(t, sizeof t, _("Ejes %d y %d"), ax + 1, ay + 1);
        else formatar_texto(t, sizeof t, _("Eje %d"), ax + 1);
        g_texto(jn, x, y + MD_CX + 4, t, COR_TEXTO_FRACO);
    }
    /* Botones: casillas numeradas, de 16 en 16. */
    int yb = Y_PAG + 172;
    g_texto(jn, MD_X0, yb, _("Botones:"), COR_TEXTO);
    for (int b = 0; b < m->n_botoes && b < 32; b++) {
        int x = MD_X0 + (b % 16) * 27, y = yb + 18 + (b / 16) * 26;
        bool on = (md_est.botoes >> b) & 1;
        g_retangulo(jn, x, y, 24, 22, on ? 0x2A6FD8 : 0xF2F2F2);
        g_moldura(jn, x, y, 24, 22, COR_BORDA);
        formatar_texto(t, sizeof t, "%d", b + 1);
        g_texto(jn, x + (b + 1 >= 10 ? 4 : 8), y + 4, t, on ? 0xFFFFFF : COR_TEXTO);
    }
    int yh = yb + 18 + ((m->n_botoes + 15) / 16) * 26 + 8;
    for (int h = 0; h < m->n_hats && h < MANDO_HATS_MAX; h++) {
        formatar_texto(t, sizeof t, _("Hat %d"), h + 1);
        g_texto(jn, MD_X0 + h * 70, yh, t, COR_TEXTO);
        md_hat(MD_X0 + h * 70, yh + 18, md_est.hats[h]);
    }
    if (m->tipo == MANDO_TIPO_PUERTO) {
        unsigned short c[4][3];
        int manual = 0;
        mandos_calibracao(c, &manual);
        int u = formatar_texto(t, sizeof t, _("Tiempos (us):"));
        for (int i = 0; i < 4; i++)
            u += md_crudo[i] == 0xFFFF ? formatar_texto(t + u, sizeof t - (unsigned)u, " -")
                                       : formatar_texto(t + u, sizeof t - (unsigned)u, " %u", md_crudo[i] / 10);
        g_texto(jn, MD_X0, ALT - 128, t, COR_TEXTO_FRACO);
        formatar_texto(t, sizeof t, _("Calibración %s: X %u-%u-%u, Y %u-%u-%u us"), manual ? _("guardada") : _("automática"),
                       c[0][0] / 10, c[0][1] / 10, c[0][2] / 10, c[1][0] / 10, c[1][1] / 10, c[1][2] / 10);
        g_texto_n(jn, MD_X0, ALT - 110, t, (LARG - MD_X0 - 20) / LETRA_L, COR_TEXTO_FRACO);
    }
}

/* Los eventos de la página; true = repintar. */
static bool md_tratar(const evento_t *e)
{
    bool mudou = false;
    int at = lista_tratar(jn, &lista_md, e);
    if (at) { memset(&md_est, 0, sizeof md_est); md_ler(); mudou = true; }
    if (botao_tratar(jn, &b_md[0], e)) {
        int r = mandos_detectar();
        dizer(r > 0 ? _("Hay un joystick en el puerto de juegos.") : _("No hay joystick en el puerto de juegos."));
        registrar("configuracion: mandos detectar -> %d", r);
        md_listar();
        mudou = true;
    }
    if (botao_tratar(jn, &b_md[1], e)) {
        if (md_puerto() < 0) dizer(_("No hay joystick en el puerto de juegos: pulsa «Buscar joystick»."));
        else {
            for (int i = 0; i < 4; i++) { md_cal[i][0] = 0xFFFF; md_cal[i][1] = 0; md_cal[i][2] = 0; }
            md_fase = 1;
            md_fim = tempo_ms() + 6000;
            dizer(_("Mueve el joystick hasta las cuatro esquinas, varias veces (6 s)"));
        }
        mudou = true;
    }
    if (botao_tratar(jn, &b_md[2], e)) {
        md_fase = 0;
        mandos_calibrar(NULL);
        conf.mando_cal[0] = 0;
        registrar("configuracion: mando_puerto automatico");
        dizer_gravado(conf_gravar());
        mudou = true;
    }
    return mudou;
}

/* Pantalla > Aceleración gráfica (D323). La página Pantalla tiene dos vistas: la resolución de siempre y esta. */
static int sub_tela;   /* 0 = Resolución, 1 = Aceleración gráfica */
static botao_t b_sub[2] = { { 436, Y_PAG + 2, 150, 26, "Resolución", false }, { 592, Y_PAG + 2, 190, 26, "Aceleración gráfica", false } };
#define AC_X0   180            /* primera columna de botones */
#define AC_PASO 124
#define AC_Y0   (Y_PAG + 122)  /* fila 2D; las demás, AC_FILA más abajo */
#define AC_FILA 44
static botao_t b_ac[4][3];     /* 2D, 3D, SDL 2 y disco; tres opciones (el disco solo dos) */
static botao_t b_acp[2];       /* «Probar ahora» del 2D y del 3D */
static botao_t b_acr = { 24, Y_PAG + 350, 220, 28, "Restablecer valores seguros", false };
static char acel_res[4][160];  /* el resultado de lo último hecho en cada fila */
static char acel_arq[200];     /* el aviso que dejó el arranque (/tmp/acel-aviso) */
static const char *const acel_rot_fila[4] = { "2D por la GPU", "3D por la GPU", "SDL 2 por la GPU", "Disco: DMA y caché" };

static void acel_preparar(void)
{
    for (int f = 0; f < 4; f++) {
        int nb = f == 3 ? 2 : 3;
        for (int s = 0; s < nb; s++) {
            const char *rot = f == 3 ? (s == 0 ? "Automático" : "Desactivado") : rotulos_tri[s];
            b_ac[f][s] = (botao_t){ AC_X0 + s * AC_PASO, AC_Y0 + f * AC_FILA, 120, 26, rot, false };
        }
    }
    b_acp[0] = (botao_t){ AC_X0 + 3 * AC_PASO + 8, AC_Y0, 130, 26, "Probar ahora", false };
    b_acp[1] = (botao_t){ AC_X0 + 3 * AC_PASO + 8, AC_Y0 + AC_FILA, 130, 26, "Probar ahora", false };
    acel_ler_estado();
    conf_ler();
    acel_arq[0] = 0;
    int fd = abrir(ARQ_AVISO, ABRIR_LER);
    if (fd >= 0) {
        int n = ler(fd, acel_arq, sizeof acel_arq - 1);
        fechar(fd);
        acel_arq[n > 0 ? n : 0] = 0;
    }
}

/* Cambiar la GPU es cosa del administrador: el diálogo de permisos (D269); sin contraseña en modo en vivo. */
static bool acel_permiso(const char *motivo)
{
    if (sou_root()) return true;
    if (elevar_grafico(motivo) == 0) return true;
    dizer("Sin permisos de administrador no se puede cambiar la aceleración.");
    return false;
}

static int acel_valor(int f) { int v = f == 0 ? conf.a_2d : f == 1 ? conf.a_3d : f == 2 ? conf.a_sdl : conf.a_disco; return v < 0 ? 0 : v; }

static void acel_escolher(int f, int t)
{
    bool gpu = f != 2;
    if (gpu && !acel_permiso(f == 0 ? "cambiar el 2D por la GPU" : f == 1 ? "cambiar el 3D por la GPU" : "cambiar el DMA y la caché del disco")) return;
    int r = 0;
    char *res = acel_res[f];
    if (f == 0) { conf.a_2d = t; r = acel_aplicar_2d(t, res, sizeof acel_res[0]); }
    else if (f == 1) { conf.a_3d = t; r = acel_aplicar_3d(t, res, sizeof acel_res[0]); }
    else if (f == 2) { conf.a_sdl = t; r = acel_aplicar_sdl(t); formatar_texto(res, sizeof acel_res[0], "SDL 2: %s", rotulos_tri[t]); }
    else { conf.a_disco = t; r = acel_aplicar_disco(t); formatar_texto(res, sizeof acel_res[0], "Disco: %s; se aplica al reiniciar%s", t == TRI_NO ? "DMA y caché desactivados" : "automático", r < 0 ? " (no se pudo grabar en la cabecera de arranque)" : ""); r = 0; }
    registrar("configuracion: aceleracion fila %d -> %s: %s", f, nomes_tri[t], res);
    int g = conf_gravar();
    registrar("configuracion: aceleracion guardada en %s -> %d", ARQ_CONF, g);
    if (r < 0) {
        if (f == 1 && acel_3d_experimental()) dizer("El 3D experimental no pasó la autoprueba: SaguiGL sigue por software.");
        else dizer("No se pudo activar: la autoprueba falló o la tarjeta no lo permite.");
    } else if (g == 1) dizer("Aplicado en esta sesión, pero /home no es persistente: no se recordará.");
    else dizer("Aplicado y guardado: se recordará en cada arranque.");
    if (gpu) elevacao_soltar();
}

static void acel_probar_fila(int f)
{
    if (!acel_permiso(f == 0 ? "probar el 2D por la GPU" : "probar el 3D por la GPU")) return;
    int p = f == 0 ? acel_probar_2d(acel_res[f], sizeof acel_res[0]) : acel_probar_3d(acel_res[f], sizeof acel_res[0]);
    registrar("configuracion: aceleracion probar %s -> %d: %s", f == 0 ? "2d" : "3d", p, acel_res[f]);
    acel_ler_estado();
    dizer(p == 1 ? "Autoprueba superada." : "La autoprueba no se superó (el driver queda apagado si falló).");
    elevacao_soltar();
}

static bool acel_tratar(const evento_t *e)
{
    bool mudou = false;
    for (int f = 0; f < 4; f++) {
        int nb = f == 3 ? 2 : 3;
        for (int s = 0; s < nb; s++)
            if (botao_tratar(jn, &b_ac[f][s], e)) {
                int t = f == 3 ? (s == 0 ? TRI_AUTO : TRI_NO) : s;
                acel_escolher(f, t);
                acel_ler_estado();
                mudou = true;
            }
    }
    for (int k = 0; k < 2; k++)
        if (botao_tratar(jn, &b_acp[k], e)) { acel_probar_fila(k); mudou = true; }
    if (botao_tratar(jn, &b_acr, e)) {
        if (acel_permiso("restablecer la aceleración gráfica a los valores seguros")) {
            acel_restablecer();
            for (int f = 0; f < 4; f++) acel_res[f][0] = 0;
            int g = conf_gravar();
            registrar("configuracion: aceleracion restablecida a los valores seguros");
            dizer(g == 1 ? "Valores seguros restablecidos en esta sesión (/home no es persistente)." : "Valores seguros restablecidos: todo en automático.");
            acel_ler_estado();
            elevacao_soltar();
        }
        mudou = true;
    }
    return mudou;
}

static void acel_desenhar(void)
{
    char t[200];
    acel_ler_estado();
    titulo_pag(24, Y_PAG + 8, "Aceleración gráfica");
    int y = Y_PAG + 34;
    if (ac2.modelo) formatar_texto(t, sizeof t, "Tarjeta: PCI %04x:%04x; 3D: %s", ac2.fabricante, ac2.modelo, ac3.placa[0] && acel_3d_hay_driver() ? ac3.placa : "sin driver");
    else formatar_texto(t, sizeof t, "Tarjeta: %s", ac3.placa[0] && acel_3d_hay_driver() ? ac3.placa : "sin acelerador reconocido");
    g_texto_n(jn, 24, y, t, (LARG - 48) / LETRA_L, COR_TEXTO);
    formatar_texto(t, sizeof t, "2D (driver %s): %s", ac2.nome, acel_texto_2d());
    g_texto_n(jn, 24, y + 18, t, (LARG - 48) / LETRA_L, COR_TEXTO);
    formatar_texto(t, sizeof t, "3D (driver %s): %s", ac3.nome, acel_texto_3d());
    g_texto_n(jn, 24, y + 36, t, (LARG - 48) / LETRA_L, COR_TEXTO);
    if (ac3.pontos) formatar_texto(t, sizeof t, "Puntuación 3D (infopc): %u; autoprueba 2D: GPU %u us, CPU %u us", ac3.pontos, ac2.us_gpu, ac2.us_cpu);
    else formatar_texto(t, sizeof t, "Puntuación: sin medir (Probar ahora, o la pestaña GPU de infopc)");
    g_texto_n(jn, 24, y + 54, t, (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    g_retangulo(jn, 24, Y_PAG + 112, LARG - 48, 1, COR_BORDA);
    for (int f = 0; f < 4; f++) {
        int yy = AC_Y0 + f * AC_FILA;
        g_texto(jn, 24, yy + 6, acel_rot_fila[f], COR_TEXTO);
        int v = acel_valor(f);
        int nb = f == 3 ? 2 : 3;
        for (int s = 0; s < nb; s++) {
            int t_ = f == 3 ? (s == 0 ? TRI_AUTO : TRI_NO) : s;
            b_ac[f][s].apertado = v == t_;
            botao_desenhar(jn, &b_ac[f][s]);
        }
        if (f < 2) botao_desenhar(jn, &b_acp[f]);
        if (acel_res[f][0]) {   /* desde el borde izquierdo (99 caracteres); si aun así no cabe, termina en «...» */
            char rr[160];
            int maxc = (LARG - 48) / LETRA_L;
            copia_texto(rr, acel_res[f], sizeof rr);
            if ((int)tam_texto(rr) > maxc) { rr[maxc - 3] = '.'; rr[maxc - 2] = '.'; rr[maxc - 1] = '.'; rr[maxc] = 0; }
            g_texto_n(jn, 24, yy + 29, rr, maxc, COR_TEXTO_FRACO);
        }
    }
    if (acel_3d_hay_driver() && acel_3d_experimental()) {
        if (conf.a_3d == TRI_SI)
            g_texto_n(jn, 24, Y_PAG + 306, "Experimental: si la pantalla se queda congelada, reinicia; el sistema volverá al modo seguro.", (LARG - 48) / LETRA_L, 0xB03000);
        else
            g_texto_n(jn, 24, Y_PAG + 306, "El 3D de esta tarjeta es experimental (sin probar en hardware): por eso está apagado.", (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    }
    if (acel_arq[0]) g_texto_n(jn, 24, Y_PAG + 324, acel_arq, (LARG - 48) / LETRA_L, 0xB03000);
    else g_texto_n(jn, 24, Y_PAG + 324, "Mayús al arrancar = modo seguro (2D por la CPU, 3D por software, sin esta configuración).", (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    botao_desenhar(jn, &b_acr);
}

static void desenhar_pagina(void)
{
    char t[160];
    if (pagina == P_TECLADO) {
        titulo_pag(24, Y_PAG + 14, _("Distribución del teclado"));
        int cur = teclado_layout(-1);
        for (int i = 0; i < 2; i++) { b_tec[i].apertado = cur == i; botao_desenhar(jn, &b_tec[i]); }
        g_texto(jn, 24, Y_PAG + 144, _("Prueba: la ñ, la ç y los acentos cambian según la distribución."), COR_TEXTO_FRACO);
        titulo_pag(24, Y_PAG + 192, _("Idioma de la interfaz"));
        g_texto(jn, 24, Y_PAG + 266, _("Se aplica a los programas que abras después y al escritorio."), COR_TEXTO_FRACO);
        for (int i = 0; i < 3; i++) { b_idi[i].apertado = compara_texto(idioma_actual(), codigos_idi[i]) == 0; botao_desenhar(jn, &b_idi[i]); }
    } else if (pagina == P_ZONA) {
        config_zona_t cz;
        ler_zona(&cz);
        fuso_data_t l;
        agora_local(&l);
        formatar_texto(t, sizeof t, _("Zona horaria actual: %s   (%02d:%02d)"), cz.nome, l.hora, l.minuto);
        titulo_pag(24, Y_PAG + 8, t);
        lista_desenhar(jn, &lista_zona);
        botao_desenhar(jn, &b_zona);
    } else if (pagina == P_TELA && sub_tela == 1) {
        acel_desenhar();
    } else if (pagina == P_TELA) {
        formatar_texto(t, sizeof t, _("Resolución actual: %ux%u, %u bits"), tela.largura, tela.altura, tela.bpp);
        titulo_pag(24, Y_PAG + 8, t);
        lista_desenhar(jn, &lista_modo);
        for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_tela[i]);
        if (telas.qtd > 1) {
            b_telas[0].apertado = telas.modo == TELAS_MODO_EXTENDER;
            b_telas[1].apertado = telas.modo == TELAS_MODO_ESPELHO;
            for (int i = 0; i < 5; i++) { b_telas[i].y = ALT - 138; botao_desenhar(jn, &b_telas[i]); }
            formatar_texto(t, sizeof t, _("%u pantallas: escritorio de %ux%u"), telas.qtd, telas.desk_largura, telas.desk_altura);
            g_texto(jn, 24, ALT - 154 + 4, t, COR_TEXTO_FRACO);
        }
        if (!tela.a_quente) g_texto(jn, 350, ALT - 94, _("Esta tarjeta cambia al reiniciar."), COR_TEXTO_FRACO);
    } else if (pagina == P_SOM) {
        info_audio_t ia;
        if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) {
            titulo_pag(24, Y_PAG + 14, _("No hay tarjeta de sonido (solo el altavoz del PC)."));
        } else {
            formatar_texto(t, sizeof t, _("Volumen general: %u %%%s"), ia.volume, ia.silencio ? _(" (silencio)") : "");
            titulo_pag(24, Y_PAG + 14, t);
            g_retangulo(jn, BARRA_X, BARRA_Y, BARRA_L, BARRA_A, COR_BOTAO);
            g_moldura(jn, BARRA_X, BARRA_Y, BARRA_L, BARRA_A, COR_BORDA);
            g_retangulo(jn, BARRA_X + 1, BARRA_Y + 1, (BARRA_L - 2) * ia.volume / 100, BARRA_A - 2, ia.silencio ? COR_BORDA : COR_DESTAQUE);
            for (int i = 0; i < 3; i++) botao_desenhar(jn, &b_vol[i]);
            formatar_texto(t, sizeof t, _("Tarjeta: %s (%s)"), ia.placa, ia.driver);
            g_texto_n(jn, 24, Y_PAG + 36, t, (MIC_X - 40 - 24) / LETRA_L, COR_TEXTO_FRACO);
            g_texto_n(jn, 24, Y_PAG + 52, ia.codec, (MIC_X - 40 - 24) / LETRA_L, COR_TEXTO_FRACO);
            /* balance */
            g_texto(jn, 180, BAL_Y + 4, _("Balance"), COR_TEXTO);
            g_retangulo(jn, BAL_X, BAL_Y, BAL_L, BAL_A, COR_BOTAO);
            g_moldura(jn, BAL_X, BAL_Y, BAL_L, BAL_A, COR_BORDA);
            {
                int cx = BAL_X + BAL_L / 2, px = BAL_X + (BAL_L - 2) * ia.balance / 200;
                g_retangulo(jn, cx, BAL_Y + 2, 1, BAL_A - 4, COR_BORDA);
                g_retangulo(jn, px - 3, BAL_Y + 2, 7, BAL_A - 4, COR_DESTAQUE);
                int b = (int)ia.balance - 100;
                formatar_texto(t, sizeof t, "%s", b == 0 ? _("centro") : b < 0 ? _("izquierda") : _("derecha"));
                g_texto(jn, BAL_X, BAL_Y - 18, t, COR_TEXTO_FRACO);
            }
            botao_desenhar(jn, &b_bal);
            for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_prob[i]);
            {   /* la salida (tarjetas PCI y USB): un botón que pasa a la siguiente; se recuerda por su nombre */
                int n_sal = 0;
                for (int k = 0; k < AUDIO_SALIDAS; k++) n_sal += ia.salidas[k].usada;
                if (n_sal > 1) {
                    static char txt_sal[80];
                    int act = 0;
                    for (int k = 0; k < AUDIO_SALIDAS; k++) if (ia.salidas[k].actual) act = k;
                    formatar_texto(txt_sal, sizeof txt_sal, _("Salida: %s (pulsa para cambiar)"), ia.salidas[act].nombre);
                    b_sal.texto = txt_sal;
                    botao_desenhar(jn, &b_sal);
                }
            }
            botao_desenhar(jn, &b_lat[0]);
            botao_desenhar(jn, &b_lat[1]);
            formatar_texto(t, sizeof t, _("Latencia: %u ms"), ia.latencia_base_ms);
            g_texto(jn, 300, BARRA_Y + 91, t, COR_TEXTO);
            /* micrófono */
            titulo_pag(MIC_X, Y_PAG + 14, _("Micrófono"));
            if (!ia.captura) {
                g_texto_n(jn, MIC_X, Y_PAG + 42, _("Esta tarjeta no tiene entrada de sonido que podamos usar."), 27, COR_TEXTO_FRACO);
            } else {
                botao_desenhar(jn, &b_mic[0]);
                botao_desenhar(jn, &b_mic[1]);
                g_texto_n(jn, MIC_X + 30, Y_PAG + 42, mic_inf.n_entradas ? mic_inf.entradas[mic_inf.entrada < CAP_ENTRADAS_MAX ? mic_inf.entrada : 0] : "-", 20, COR_TEXTO);
                botao_desenhar(jn, &b_mic[2]);
                botao_desenhar(jn, &b_mic[3]);
                g_retangulo(jn, MIC_GX, Y_PAG + 70, MIC_GL, 24, COR_BOTAO);
                g_moldura(jn, MIC_GX, Y_PAG + 70, MIC_GL, 24, COR_BORDA);
                g_retangulo(jn, MIC_GX + 1, Y_PAG + 71, (MIC_GL - 2) * (int)mic_inf.ganancia / 100, 22, mic_inf.mudo ? COR_BORDA : COR_DESTAQUE);
                formatar_texto(t, sizeof t, "%u %%", mic_inf.ganancia);
                g_texto(jn, MIC_GX + 6, Y_PAG + 74, t, COR_BRANCO);
                /* medidor de nivel en vivo */
                int mx = MIC_X, my = MIC_MY, ml = 216, ma = 22;
                unsigned pico = mic_inf.pico > 32767 ? 32767 : mic_inf.pico;
                int lp = (int)((unsigned long long)pico * (unsigned)(ml - 4) / 32767);
                g_retangulo(jn, mx, my, ml, ma, 0x20262C);
                if (lp > 0) g_retangulo(jn, mx + 2, my + 2, lp, ma - 4, pico > 29000 ? 0xD83A2A : pico > 20000 ? 0xE0B020 : 0x2EB872);
                g_moldura(jn, mx, my, ml, ma, COR_BORDA);
                g_texto_n(jn, mx, my + ma + 4, mic_aberto ? _("Habla para probar el nivel.") : _("Micrófono en uso por otro programa."), 27, COR_TEXTO_FRACO);
                b_mic[4].texto = mic_inf.mudo ? _("Activar micrófono") : _("Silenciar micrófono");
                botao_desenhar(jn, &b_mic[4]);
            }
            titulo_pag(24, EQ_Y0 - 32, _("Ecualizador (dB)"));
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
        titulo_pag(24, Y_PAG + 14, _("Carpeta de las capturas de pantalla (Impr Pant)"));
        g_texto(jn, 24, Y_PAG + 40, _("Vacía = /home/capturas (o /tmp/capturas si /home no deja escribir)."), COR_TEXTO_FRACO);
        caixa_desenhar(jn, &c_capturas);
        botao_desenhar(jn, &b_cap[0]);
        b_cap[1].texto = conf.cap_sonido ? _("Sonido al capturar: sí") : _("Sonido al capturar: no");
        botao_desenhar(jn, &b_cap[1]);
    } else if (pagina == P_IA) {
        ia_desenhar();
    } else if (pagina == P_FUENTES) {
        fuentes_desenhar();
    } else if (pagina == P_RED) {
        red_desenhar();
    } else if (pagina == P_VNC) {
        vnc_desenhar();
    } else if (pagina == P_MANDOS) {
        md_desenhar();
    } else {
        info_aparencia_t ap;
        memset(&ap, 0, sizeof ap);
        info_sistema(INFO_APARENCIA, &ap, sizeof ap);
        titulo_pag(24, Y_PAG + 6, _("Color de acento (barra de título y barra de tareas)"));
        for (int i = 0; i < CORES_QTD; i++) {
            int x = SW_X + i * SW_PASSO;
            g_retangulo(jn, x, SW_Y, SW_L, 30, cores[i].cor);
            g_moldura(jn, x, SW_Y, SW_L, 30, cores[i].cor == ap.acento ? 0x000000 : COR_BORDA);
            if (cores[i].cor == ap.acento) g_moldura(jn, x + 1, SW_Y + 1, SW_L - 2, 28, 0xFFFFFF);
            g_texto(jn, x, SW_Y + 34, _(cores[i].nome), COR_TEXTO_FRACO);
        }
        titulo_pag(24, Y_PAG + 98, _("Fondo del escritorio"));
        for (int i = 0; i < M_QTD; i++) { b_modo[i].apertado = conf.modo == i; botao_desenhar(jn, &b_modo[i]); }
        g_texto(jn, 24, Y_PAG + 160, _("Imagen (BMP o PNG de /imagenes y de /home):"), COR_TEXTO_FRACO);
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
    if (pagina == P_TELA) {   /* D323: las dos vistas de Pantalla */
        for (int i = 0; i < 2; i++) { b_sub[i].apertado = sub_tela == i; botao_desenhar(jn, &b_sub[i]); }
    }
    if (!persiste) g_texto_n(jn, 14, ALT - 62, _("Aviso: /home no es un disco persistente: los cambios no se recordarán."), (LARG - 28) / LETRA_L, 0xB03000);
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
        if (p->precisa_chave) dizer(_("Proveedor elegido. Escribe la clave y pulsa Guardar."));
        else dizer(_("Proveedor elegido (no necesita clave). Pulsa Guardar."));
    } else {
        caixa_definir(&c_ia[0], "");
        dizer(_("Personalizado: escribe la dirección del servidor."));
    }
}

/* Pide los modelos al servidor y los deja en la lista. */
static void ia_cargar_modelos(void)
{
    ia_recolher();
    if (!cfg_ia.url[0]) { dizer(_("Primero pon la dirección del servidor.")); return; }
    char erro[192];
    int n = ia_modelos(&cfg_ia, itens_mod, MAX_MOD, erro, (int)sizeof erro);
    if (n < 0) {
        formatar_texto(estado, sizeof estado, _("No se pudieron pedir los modelos: %s"), erro);
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
                    _("%d modelos del servidor. Elige uno: va a la casilla donde está el cursor."), n);
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
    dizer(_("Modelo puesto. Pulsa Guardar para que lo usen los programas."));
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
    titulo_pag(24, Y_PAG + 6, _("Proveedor"));
    lista_desenhar(jn, &lista_prov);
    g_texto_n(jn, 24, Y_PAG + 252, _("Elige uno y pon su URL."),
               200 / LETRA_L, COR_TEXTO_FRACO);

    g_texto(jn, 250, Y_PAG + 4, _("Dirección del servidor (URL)"), COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[0]);
    formatar_texto(t, sizeof t, _("Clave de API (%s)"), chave_ia[0] ? _("definida") : _("sin clave"));
    g_texto(jn, 250, Y_PAG + 42, t, COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[1]);

    for (int i = 0; i < 4; i++) {
        int x = i % 2 == 0 ? 250 : 490;
        int y = Y_PAG + 100 + (i / 2) * 48;
        g_texto(jn, x, y, _(rotulos_uso[i]), COR_TEXTO_FRACO);
        caixa_desenhar(jn, &c_ia[2 + i]);
        /* La casilla de destino se marca con un marco azul: así se ve de dónde
         * a dónde va el modelo que se elige en la lista. */
        if (2 + i == cx_alvo)
            g_moldura(jn, c_ia[2 + i].x - 2, c_ia[2 + i].y - 2, c_ia[2 + i].l + 4, LETRA_A + 12, COR_SELECAO);
    }
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_ia[i]);
    g_texto(jn, 250, Y_PAG + 244, _("Modelo para:"), COR_TEXTO_FRACO);
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

static void balance_poner(int b)
{
    if (b < 0) b = 0;
    if (b > 200) b = 200;
    if (b > 92 && b < 108) b = 100;          /* se «pega» al centro */
    conf.balance = b;
    conf.son_def |= 1;
    aplicar_balance(b);
}

static void mic_guardar(void)
{
    conf.mic_ent = (int)mic_inf.entrada;
    conf.mic_gan = (int)mic_inf.ganancia;
    conf.mic_mudo = mic_inf.mudo != 0;
    conf.son_def |= 4;
    dizer_gravado(conf_gravar());
}

static void mic_ganancia_poner(int g, bool mudo)
{
    g = g < 0 ? 0 : g > 100 ? 100 : g;
    captura_ganancia((unsigned)g, mudo);
    captura_estado(CAP_FUENTE_REAL, &mic_inf);
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

/* Los rótulos que se calculan al abrir la ventana (las pestañas, de ancho el de su título, D145, y los modos
 * del fondo): se repiten al cambiar el idioma (D313). */
static void traducir_rotulos(void)
{
    int xp = 10;
    /* Relleno de cada pestaña: 10 px, y menos si todas juntas no caben en LARG (issue #43). */
    int suma = 0, relleno = 10;
    for (int i = 0; i < P_QTD; i++) suma += g_largura_texto(_(titulos_pag[i]));
    while (relleno > 2 && 10 + suma + P_QTD * (relleno + 1) > LARG - 4) relleno--;
    for (int i = 0; i < P_QTD; i++) {
        int l = g_largura_texto(_(titulos_pag[i])) + relleno;
        pos_pag[i] = xp;
        larg_pag[i] = l;
        b_pag[i] = (botao_t){ xp, 10, l, 28, _(titulos_pag[i]), false };
        xp += l + 1;
    }
    int x = 24;
    for (int i = 0; i < M_QTD; i++) { b_modo[i] = (botao_t){ x, Y_PAG + 118, 90, 28, _(rotulos_modo[i]), false }; x += 94; }
    if (jn) titulo_janela(jn->id, _("Configuración"));
}

static int janela(void)
{
    jn = jan_abrir(_("Configuración"), LARG, ALT);
    if (!jn) { imprimir("configuracion: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    barra_fundo = progresso_criar(BARRA_FUNDO_X, BARRA_FUNDO_Y, BARRA_FUNDO_L, BARRA_FUNDO_A, false);
    if (barra_fundo) progresso_texto(barra_fundo, true);
    conf_ler();
    persiste = home_persistente();
    traducir_rotulos();
    c_vnc_clave = (caixa_t){ .x = 24, .y = Y_PAG + 124, .l = 260, .oculta = true, .foco = true };
    lista_vnc = (lista_t){ 24, Y_PAG + 238, LARG - 48 - 170, 80, itens_vnc, 0, -1, 0, NULL };
    c_capturas = (caixa_t){ .x = 24, .y = Y_PAG + 66, .l = LARG - 48, .foco = true };
    caixa_definir(&c_capturas, conf.capturas);
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
    if (!persiste) dizer(_("Sin /home persistente los cambios se aplican, pero no se recordarán."));
    desenhar();
    registrar("configuracion: ventana abierta (persistente %d)", persiste);

    evento_t e;
    for (;;) {
        /* En «Escritorio remoto» se repinta cada segundo: la lista de conectados cambia sola. */
        int hay = pagina == P_VNC ? esperar_evento(jn->id, &e, 1000)
                : (pagina == P_SOM && mic_aberto) ? esperar_evento(jn->id, &e, 60)   /* el medidor del micrófono (D292) */
                : pagina == P_MANDOS ? esperar_evento(jn->id, &e, 50)   /* la prueba en vivo de los mandos (D307) */
                : proximo_evento(jn->id, &e, true);
        if (hay != 1) {
            if (pagina == P_VNC) { vnc_refrescar(); desenhar(); }
            if (pagina == P_SOM && mic_aberto) { mic_refrescar(); desenhar(); }
            if (pagina == P_MANDOS && md_tique()) desenhar();
            continue;
        }
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TELA) { ler_modos_video(); desenhar(); continue; }
        bool mudou = false;
        if (botao_tratar(jn, &b_cerrar, &e)) break;
        for (int i = 0; i < P_QTD; i++)
            if (botao_tratar(jn, &b_pag[i], &e)) {
                pagina = i;
                registrar("configuracion: pagina %s", titulos_pag[i]);
                if (i == P_SOM) mic_abrir(); else mic_cerrar();   /* D292 */
                if (i == P_TELA) { ler_modos_video(); if (sub_tela == 1) acel_preparar(); }
                if (i == P_IA) ia_preparar();   /* relee lo guardado (D145) */
                if (i == P_FUENTES) fuentes_preparar();   /* (D196) */
                if (i == P_VNC) vnc_refrescar();
                if (i == P_MANDOS) md_preparar();
                else md_fase = 0;
                mudou = true;
            }
        if (pagina == P_TELA)
            for (int i = 0; i < 2; i++)
                if (botao_tratar(jn, &b_sub[i], &e)) {
                    sub_tela = i;
                    if (i == 1) acel_preparar(); else ler_modos_video();
                    registrar("configuracion: pantalla vista %s", i ? "aceleracion" : "resolucion");
                    mudou = true;
                }
        if (pagina == P_MANDOS) {
            if (md_tratar(&e)) mudou = true;
        } else if (pagina == P_TELA && sub_tela == 1) {
            if (acel_tratar(&e)) mudou = true;
        } else if (pagina == P_TECLADO) {
            for (int i = 0; i < 2; i++)
                if (botao_tratar(jn, &b_tec[i], &e)) {
                    aplicar_teclado(i);
                    conf.teclado = i;
                    dizer_gravado(conf_gravar());
                    mudou = true;
                }
            for (int i = 0; i < 3; i++)
                if (botao_tratar(jn, &b_idi[i], &e)) {
                    copia_texto(conf.idioma, codigos_idi[i], sizeof conf.idioma);
                    idioma_fijar(codigos_idi[i]);
                    traducir_rotulos();
                    ler_modos_video();   /* los textos de las listas llevan palabras traducibles */
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
                if (r < 0) dizer(_("No se pudo cambiar la zona horaria."));
                else { copia_texto(conf.zona, nome, sizeof conf.zona); dizer_gravado(conf_gravar()); }
                ler_zonas();
                mudou = true;
            } else if (at) mudou = true;
        } else if (pagina == P_TELA) {
            if (telas.qtd > 1) {   /* D290: cada botón se aplica y se guarda al momento */
                int n = (int)telas.qtd, modo = (int)telas.modo, lado = (int)telas.lado, prin = (int)telas.principal;
                bool tocou = false;
                for (int i = 0; i < 5; i++) b_telas[i].y = ALT - 138;
                if (botao_tratar(jn, &b_telas[0], &e)) { modo = TELAS_MODO_EXTENDER; tocou = true; }
                if (botao_tratar(jn, &b_telas[1], &e)) { modo = TELAS_MODO_ESPELHO; tocou = true; }
                if (botao_tratar(jn, &b_telas[2], &e)) { prin = (prin + 1) % n; tocou = true; }
                if (botao_tratar(jn, &b_telas[3], &e)) { lado = lado == TELAS_LADO_ABAIXO ? TELAS_LADO_DIREITA : TELAS_LADO_ABAIXO; tocou = true; }
                if (botao_tratar(jn, &b_telas[4], &e)) { sel_res = (sel_res + 1) % n; mudou = true; ler_modos_video(); }
                if (tocou) {
                    int r = telas_sis(TELAS_OP_CONFIG, (unsigned)modo | (unsigned)lado << 8 | (unsigned)prin << 16, 0);
                    if (r == 0) { telas_sis(TELAS_OP_GUARDAR, 0, 0); copia_texto(estado, _("Pantallas cambiadas y guardadas"), sizeof estado); }
                    else formatar_texto(estado, sizeof estado, _("No se pudo: %s"), nome_erro(r));
                    ler_modos_video();
                    mudou = true;
                }
            }
            int at = lista_tratar(jn, &lista_modo, &e);
            bool ap = botao_tratar(jn, &b_tela[0], &e) || at == LISTA_ATIVOU;
            bool gu = botao_tratar(jn, &b_tela[1], &e);
            if ((ap || gu) && lista_modo.sel >= 0) {
                modo_video_t *m = &modos[indice_modo_l[lista_modo.sel]];
                bool igual = m->largura == tela.largura && m->altura == tela.altura;
                int op = tela.a_quente && !igual ? MUDAR_AGORA : 0;
                if (gu || !tela.a_quente) op |= MUDAR_GUARDAR;
                if (sel_res > 0 && telas.qtd > 1) {   /* D290: la resolución de una pantalla extra */
                    int r = telas_sis(TELAS_OP_RESOLUCAO, (unsigned)sel_res, m->largura | (unsigned)m->altura << 16);
                    if (r == 0) { telas_sis(TELAS_OP_GUARDAR, 0, 0); formatar_texto(estado, sizeof estado, _("Pantalla %d: %ux%u"), sel_res + 1, m->largura, m->altura); }
                    else formatar_texto(estado, sizeof estado, _("No se pudo: %s"), nome_erro(r));
                } else
                if (!op) dizer(_("La pantalla ya está en esa resolución."));
                else {
                    int r = trocar_modo_video((int)m->largura, (int)m->altura, op);
                    if (r == ERRO_SO_LEITURA) dizer(_("Arranque desde CD: no se puede guardar la resolución."));
                    else if (r < 0) formatar_texto(estado, sizeof estado, _("No se pudo: %s"), nome_erro(r));
                    else if (op & MUDAR_AGORA) formatar_texto(estado, sizeof estado, "%ux%u%s", m->largura, m->altura, (r & MUDAR_GUARDAR) ? _(", guardada") : "");
                    else formatar_texto(estado, sizeof estado, _("%ux%u al reiniciar"), m->largura, m->altura);
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
            /* D292: balance, pruebas, latencia y micrófono */
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, BAL_X, BAL_Y, BAL_L, BAL_A)) {
                arrastando_bal = true;
                balance_poner((e.x - BAL_X) * 200 / BAL_L);
                mudou = true;
            } else if (e.tipo == EV_MOVER && arrastando_bal && (e.botoes & 1)) {
                balance_poner((e.x - BAL_X) * 200 / BAL_L);
                mudou = true;
            } else if (e.tipo == EV_SOLTOU && arrastando_bal) {
                arrastando_bal = false;
                dizer_gravado(conf_gravar());
                mudou = true;
            }
            {
                info_audio_t is;
                if (botao_tratar(jn, &b_sal, &e) && info_sistema(INFO_AUDIO, &is, sizeof is) == 0) {
                    int act = 0, sig;
                    for (int k = 0; k < AUDIO_SALIDAS; k++) if (is.salidas[k].actual) act = k;
                    for (sig = (act + 1) % AUDIO_SALIDAS; sig != act && !is.salidas[sig].usada; sig = (sig + 1) % AUDIO_SALIDAS) { }
                    if (sig != act && audio_salida(sig) >= 0) {
                        copia_texto(conf.salida, is.salidas[sig].nombre, sizeof conf.salida);
                        dizer_gravado(conf_gravar());
                    }
                    mudou = true;
                }
            }
            if (botao_tratar(jn, &b_bal, &e)) { balance_poner(100); dizer_gravado(conf_gravar()); mudou = true; }
            for (int i = 0; i < 2; i++)
                if (botao_tratar(jn, &b_prob[i], &e)) { probar_lado(i == 0); mudou = true; }
            for (int i = 0; i < 2; i++)
                if (botao_tratar(jn, &b_lat[i], &e)) {
                    info_audio_t ia2;
                    if (info_sistema(INFO_AUDIO, &ia2, sizeof ia2) == 0) {
                        int ms = (int)ia2.latencia_base_ms + (i ? 20 : -20);
                        ms = ms < 20 ? 20 : ms > 300 ? 300 : ms;
                        conf.latencia = ms;
                        conf.son_def |= 2;
                        aplicar_latencia(ms);
                        dizer_gravado(conf_gravar());
                    }
                    mudou = true;
                }
            if (mic_inf.presente) {
                if (botao_tratar(jn, &b_mic[0], &e) || botao_tratar(jn, &b_mic[1], &e)) {
                    unsigned n = mic_inf.n_entradas;
                    if (n > 1) {
                        unsigned nueva = e.x < MIC_X + 100 ? (mic_inf.entrada + n - 1) % n : (mic_inf.entrada + 1) % n;
                        captura_entrada(nueva);
                        captura_estado(CAP_FUENTE_REAL, &mic_inf);
                        mic_guardar();
                    }
                    mudou = true;
                }
                if (botao_tratar(jn, &b_mic[2], &e) || botao_tratar(jn, &b_mic[3], &e)) {
                    mic_ganancia_poner((int)mic_inf.ganancia + (e.x < MIC_GX ? -5 : 5), mic_inf.mudo != 0);
                    mic_guardar();
                    mudou = true;
                }
                if (botao_tratar(jn, &b_mic[4], &e)) {
                    mic_ganancia_poner((int)mic_inf.ganancia, !mic_inf.mudo);
                    mic_guardar();
                    mudou = true;
                }
                if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, MIC_GX, Y_PAG + 70, MIC_GL, 24)) {
                    arrastando_mic = true;
                    mic_ganancia_poner((e.x - MIC_GX) * 100 / MIC_GL, mic_inf.mudo != 0);
                    mudou = true;
                } else if (e.tipo == EV_MOVER && arrastando_mic && (e.botoes & 1)) {
                    mic_ganancia_poner((e.x - MIC_GX) * 100 / MIC_GL, mic_inf.mudo != 0);
                    mudou = true;
                } else if (e.tipo == EV_SOLTOU && arrastando_mic) {
                    arrastando_mic = false;
                    mic_guardar();
                    mudou = true;
                }
            }
            b_vol[2].texto = audio_silencio(-1) > 0 ? _("Activar sonido") : _("Silenciar");
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
                    dizer(k == 1 ? _("Descargando las listas...") : _("Un momento..."));
                    desenhar();
                    int r = rodar_bloqueo(ordem);
                    if (k == 1) dizer(r == 0 ? _("Listas actualizadas.") : _("No se pudieron actualizar las listas (¿hay red?)."));
                    else if (r < 0) dizer(_("No se pudo cambiar el bloqueo (mira «bloqueo estado» en el terminal)."));
                    else dizer(b.activo ? _("Bloqueo desactivado.") : _("Bloqueo activado."));
                    mudou = true;
                }
        } else if (pagina == P_VNC) {
            if (caixa_tratar(jn, &c_vnc_clave, &e)) { vnc_accion(2); vnc_refrescar(); }
            if (lista_tratar(jn, &lista_vnc, &e)) mudou = true;
            for (int k = 0; k < 5; k++)
                if (botao_tratar(jn, &b_vnc[k], &e)) { vnc_accion(k); vnc_refrescar(); mudou = true; }
            if (botao_tratar(jn, &b_vnc_des, &e)) { vnc_desconectar_sel(); vnc_refrescar(); mudou = true; }
            mudou = true;
        } else if (pagina == P_FUENTES) {
            if (n_fnt > 0) {
                int at = lista_tratar(jn, &lista_fnt, &e);
                if (at) { fuentes_vista(); mudou = true; }
                for (int k = 0; k < 3; k++)
                    if (botao_tratar(jn, &b_fnt[k], &e)) {
                        if (k < 2 && lista_fnt.sel < 0) dizer(_("Elige una fuente de la lista."));
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
                if (r < 0) formatar_texto(estado, sizeof estado, _("No se pudo guardar: %s"), nome_erro(r));
                else if (!persiste) dizer(_("Guardado solo en esta sesión: /home no es un disco."));
                else dizer(_("Guardado en /home/.config/ia. Los programas ya lo usan."));
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
                if (lista_img.sel < 0) dizer(_("Elige una imagen de la lista."));
                else {
                    if (conf.modo == M_DEGRADE) conf.modo = M_AJUSTADO;
                    dizer(_("Aplicando el fondo..."));
                    mostrando_barra = true;
                    desenhar();
                    int r = fundo_aplicar(itens_img[lista_img.sel], conf.modo);
                    mostrando_barra = false;
                    if (r < 0) formatar_texto(estado, sizeof estado, _("No se pudo: %s"), texto_erro(r));
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
    mic_cerrar();
    jan_fechar(jn);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--aplicar") == 0) return aplicar_tudo(argc, argv);
    if (argc >= 2 && compara_texto(argv[1], "--ventana") != 0) return linha_de_comandos(argc, argv);
    return janela();
}
