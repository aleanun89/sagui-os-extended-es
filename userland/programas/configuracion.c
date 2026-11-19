// SPDX-License-Identifier: GPL-2.0-or-later
/* configuracion - la aplicación «Configuración» de SaguiOS (D112, mejoras 007 y 047).
 *
 *   configuracion                    la ventana, con las secciones Teclado, Fecha y hora, Pantalla,
 *                                    Sonido, Escritorio, Capturas e IA
 *   configuracion --aplicar          lo que el kernel lanza al arrancar: lee el archivo de
 *                                    configuración y lo aplica (teclado, volumen, zona, acento, fondo)
 *   configuracion --usuarios         la ventana, en la página Usuarios (D340: crear, borrar y renombrar
 *                                    cuentas, su contraseña y el grupo administrador)
 *   configuracion mostrar            el contenido del archivo y el estado actual
 *   configuracion teclado es|abnt2
 *   configuracion volumen 0-100|silencio|activar
 *   configuracion zona NOMBRE        (por ejemplo Europe/Madrid)
 *   configuracion acento RRGGBB      color de acento (barra de título y barra de tareas)
 *   configuracion fondo ARCHIVO [centrado|ajustado|rellenar|estirado|mosaico]
 *   configuracion fondo ninguno      vuelve al degradado
 *   configuracion barra CLAVE=N...   la barra y el menú Inicio (D361), con las claves del archivo (barra_posicion=1,
 *                                    inicio_menu=2...); «barra restablecer» vuelve a lo de serie; «barra mostrar»
 *   La Configuración tiene, dentro de «Escritorio», la vista «Barra y menú Inicio» (posición, altura, ocultar sola,
 *   reloj, bandeja, agrupar, rótulos, tema y los cuatro estilos de menú Inicio), con una vista previa.
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
#include "contas_gestao.h"   /* D340: Usuarios (sin contas.h: tiene su propio conf_gravar) */
#include "opcoes_kernel.h"   /* D353: Pantalla > Kernel */
#include "conf_maquina.h"    /* D371: la configuración de la máquina en /etc */
#include "conf_reglas.h"      /* D367: dependencias entre opciones */
#include "pcinomes.h"      /* nombres PCI de la tarjeta (solo lectura, como infopc) */

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
    /* D361: la barra de tareas y el menú Inicio (BC_* de abi.h). barra_def = 1 si alguna clave está en el archivo. */
    int      barra_def;
    unsigned char barra[BC_N];
} conf_t;

static conf_t conf = { -1, -1, 0, "", 0xFFFFFFFF, "", M_AJUSTADO, "", 1, .a_2d = -1, .a_3d = -1, .a_sdl = -1, .a_disco = -1 };

/* Colores de acento (D367). Todos dan al menos 3:1 de contraste con el texto blanco de la barra de título y de
 * tareas (lo vigila h369): por eso no hay amarillos ni grises claros. */
#define CORES_QTD 16
static const struct { const char *nome; uint32_t cor; } cores[CORES_QTD] = {
    { N_("Verde"), 0x0B7A3E }, { N_("Verde claro"), 0x3A9B3A }, { N_("Azul"), 0x2A6FD8 }, { N_("Azul marino"), 0x1F3A7A },
    { N_("Cian"), 0x0A7F9E }, { N_("Turquesa"), 0x0E8A8A }, { N_("Rojo"), 0xC0392B }, { N_("Naranja"), 0xD97706 },
    { N_("Ámbar"), 0xB7791F }, { N_("Marrón"), 0x7A4A2A }, { N_("Violeta"), 0x7B3FA0 }, { N_("Rosa"), 0xC2437A },
    { N_("Rosa claro"), 0xC96B93 }, { N_("Gris"), 0x6B7480 }, { N_("Grafito"), 0x4A5560 }, { N_("Negro"), 0x20262C },
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

/* D361: las claves de sagui.conf de la barra y del menú Inicio, en el orden de BC_* (abi.h). */
static const char *const claves_barra[BC_N] = {
    "barra_posicion", "barra_altura", "barra_autoocultar", "barra_reloj", "barra_fecha", "barra_segundos",
    "bandeja_volumen", "bandeja_red", "bandeja_temperatura", "bandeja_bateria", "barra_agrupar", "barra_rotulos",
    "barra_iconos", "inicio_texto", "barra_tema", "inicio_menu", "inicio_buscador", "inicio_translucido",
};
static const unsigned char barra_padrao[BC_N] = { 0, 24, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1 };
static const unsigned char barra_maximo[BC_N] = { 1, 48, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 2, 4, 3, 1, 1 };

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

/* D371: guardar las pantallas es de la máquina (con /etc/sistema.conf, el administrador): el diálogo si hace falta. */
static int telas_guardar_admin(void)
{
    int r = telas_sis(TELAS_OP_GUARDAR, 0, 0);
    if (r == ERRO_ACESSO && !sou_root() && elevar_grafico(_("guardar la configuración de las pantallas")) == 0) {
        r = telas_sis(TELAS_OP_GUARDAR, 0, 0);
        elevacao_soltar();
    }
    return r;
}

/* D371: qué claves se toman de un archivo. */
enum { CL_TODO, CL_PERSONA, CL_MAQUINA };

static bool conf_ler_arq(const char *arq, int modo);

/* Lee el archivo; false = no existe. Las claves desconocidas se ignoran.
 * D371: con la configuración de la máquina separada (/etc/sistema.conf), las claves de la máquina salen de
 * ahí y no de sagui.conf; el teclado es de los dos: el de la persona manda y, si no tiene, el del sistema. */
static bool conf_ler(void)
{
    if (!maquina_separada()) return conf_ler_arq(ARQ_CONF, CL_TODO);
    bool tem = conf_ler_arq(ARQ_CONF, CL_PERSONA);
    conf_ler_arq(MAQ_ARQ_CONF, CL_MAQUINA);
    return tem;   /* si existe el de la persona (lo de la máquina, aplicar_tudo lo mira aparte) */
}

static bool conf_ler_arq(const char *arq, int modo)
{
    int fd = abrir(arq, ABRIR_LER);
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
            bool maq = maquina_clave(k);
            if ((modo == CL_PERSONA && maq && compara_texto(k, "teclado") != 0) || (modo == CL_MAQUINA && !maq) ||
                (modo == CL_MAQUINA && compara_texto(k, "teclado") == 0 && conf.teclado >= 0))
                ;   /* D371: no es de este archivo */
            else if (compara_texto(k, "teclado") == 0) conf.teclado = compara_texto(v, "abnt2") == 0 ? TECLADO_ABNT2
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
            else {
                for (int b = 0; b < BC_N; b++)
                    if (compara_texto(k, claves_barra[b]) == 0) {
                        if (!conf.barra_def) memcpy(conf.barra, barra_padrao, sizeof conf.barra);
                        conf.barra_def = 1;
                        int x = texto_para_int(v);
                        conf.barra[b] = (unsigned char)(x < 0 ? 0 : x > barra_maximo[b] ? barra_maximo[b] : x);
                    }
            }
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
    char t[3072];
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
    char bar[700] = "";   /* D361: la barra y el menú Inicio */
    if (conf.barra_def) {
        int u = 0;
        for (int b = 0; b < BC_N; b++) u += formatar_texto(bar + u, sizeof bar - (unsigned)u, "%s=%d\n", claves_barra[b], conf.barra[b]);
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
                   "# El kernel la aplica al arrancar con \"configuracion --aplicar\".\n%s%s%s%s%s%s%s%s%s%s%s%s",
                   tec, vol, ecu, son, zon, ace, fun, cap, fnt, idi, ace2, bar);
    int fd = abrir(ARQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    if (r >= 0 && maquina_separada()) {
        /* D371: lo de la máquina va a /etc/sistema.conf (solo si cambió; si no es root, el diálogo de permisos).
         * En sagui.conf se queda también, sin efecto, por si se vuelve a una versión anterior. */
        static char maq[600];
        formatar_texto(maq, sizeof maq, "%s%s", son, ace2);
        int rm = maquina_gravar_linhas(maq, "guardar la configuración de la máquina");
        if (rm < 0) return rm;
    }
    return r < 0 ? r : 0;
}

/* ------------------------------------------------------------ aplicar */

/* D361: pone los ajustes de la barra y del menú Inicio en el kernel (se aplican al momento). */
static int aplicar_barra(void)
{
    info_barra_t ib;
    memset(&ib, 0, sizeof ib);
    ib.versao = 1;
    memcpy(ib.v, conf.barra, sizeof conf.barra);
    return info_sistema(INFO_BARRA_GRAVAR, &ib, sizeof ib);
}

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
    if (info_arquivo(maquina_separada() ? "/etc/sonido.conf" : "/home/.config/sonido.conf", &ia) < 0) return;   /* D371 */
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
static info_video_t  ac_vid;     /* memoria de vídeo (VBE), para el bloque de la tarjeta */
static info_gpu_energia_t ac_en; /* D399: relojes, temperatura, VRAM y modo de energía */
static char          ac_modelo[96];   /* nombre PCI de la tarjeta, como «infopc -t gpu» */
static bool          ac_modelo_ok;
static void acel_ler_estado(void)
{
    memset(&ac2, 0, sizeof ac2);
    memset(&ac3, 0, sizeof ac3);
    memset(&ac_vid, 0, sizeof ac_vid);
    info_sistema(INFO_ACEL2D, &ac2, sizeof ac2);
    info_sistema(INFO_GPU3D, &ac3, sizeof ac3);
    info_sistema(INFO_VIDEO, &ac_vid, sizeof ac_vid);
    memset(&ac_en, 0, sizeof ac_en);
    if (info_sistema(INFO_GPU_ENERGIA, &ac_en, sizeof ac_en) != 0 || ac_en.versao != 1) memset(&ac_en, 0, sizeof ac_en);
    if (!ac_modelo_ok) {   /* el PCI no cambia: se lee una vez (solo lectura, D346) */
        ac_modelo_ok = true;
        ac_modelo[0] = 0;
        static info_pci_t pv[64];
        int np = info_sistema(INFO_PCI, pv, 64);
        for (int i = 0; i < np && !ac_modelo[0]; i++) {
            const info_pci_t *p = &pv[i];
            if (p->classe != 0x03) continue;
            const char *f = pci_nome_fabricante(p->fabricante), *d = pci_nome_dispositivo(p->fabricante, p->modelo);
            if (!d && ac2.placa[0] && ac2.fabricante == p->fabricante && ac2.modelo == p->modelo) { f = NULL; d = ac2.placa; }
            formatar_texto(ac_modelo, sizeof ac_modelo, "%s%s%s (%04x:%04x)", f ? f : "", f && d ? " " : "", d ? d : "", p->fabricante, p->modelo);
        }
    }
}

/* Tarjetas con 3D sin probar en hardware (D284, D287, D357): R5xx, R6xx y R7xx. */
static bool acel_3d_experimental(void) { return contem(ac3.placa, "R5xx") || contem(ac3.placa, "R6xx") || contem(ac3.placa, "R7xx"); }
static bool acel_3d_hay_driver(void) { return ac3.estado != GPU3D_SEM_GPU && ac3.estado != GPU3D_DESATIVADO; }

static const char *acel_texto_2d(void)
{
    if (ac2.ativo) return _("activo (la GPU copia y rellena)");
    if (ac2.apagado == 2) return _("apagado a mano (la CPU hace todo)");
    if (ac2.apagado == 1) return _("apagado por el vigilante: la GPU no respondió");
    if ((ac2.modo & 0x0F) == 1 || (ac2.modo & 0x80)) return _("desactivado (la CPU hace todo)");
    return _("CPU (sin acelerador o la autoprueba falló)");
}

static const char *acel_texto_3d(void)
{
    switch (ac3.estado) {
    case GPU3D_SIN_PROBAR:  return _("disponible, sin probar (se prueba con el primer uso)");
    case GPU3D_ATIVO:       return _("activo");
    case GPU3D_FALHOU:      return _("falló: SaguiGL va por software");
    case GPU3D_APAGADO:     return _("apagado (SaguiGL por software)");
    case GPU3D_DESATIVADO:  return _("desactivado en el arranque (software)");
    default:                return _("sin driver 3D para esta tarjeta (software)");
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
    if (conf.a_disco >= 0) {
        /* D367: la cabecera de arranque manda: si «kconfig» guardó dma o cache_disco, lo de sagui.conf no lo pisa */
        info_kconf_t kk[KCONF_MAX];
        int nk = kconf_ler(kk, KCONF_MAX, NULL), guardado = 0;
        for (int i = 0; i < nk; i++)
            if ((compara_texto(kk[i].nome, "dma") == 0 || compara_texto(kk[i].nome, "cache_disco") == 0) && kk[i].guardado) guardado = 1;
        if (guardado) registrar("configuracion: disco dma/cache de sagui.conf no se aplica: manda lo guardado con kconfig");
        else registrar("configuracion: disco dma/cache %s -> %d", nomes_tri[conf.a_disco], acel_aplicar_disco(conf.a_disco));
    }
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
    /* D340: «--aplicar UID GID --sesion»: una sesión que empieza (o se cierra) después del arranque. El kernel
     * nos lanza ya con la identidad y la carpeta de la cuenta: solo lo suyo, ni conectores ni aceleración. */
    bool sesion = argc >= 5 && compara_texto(argv[4], "--sesion") == 0;
    /* D371: la primera vez con SAGUI-CONF, lo de la máquina pasa de la carpeta de sagui a /etc (como root). */
    if (!sesion && sou_root()) { int rm = maquina_migrar(); if (rm) registrar("configuracion: configuracion de la maquina a /etc -> %d", rm); }
    /* D371 (fase 4): después (lee lo de la máquina de /home), sagui pasa a /home/sagui. */
    if (!sesion && sou_root()) { int rc = casa_migrar_sagui(); if (rc) registrar("configuracion: sagui a su carpeta -> %d", rc); }
    if (!sesion && sou_root()) casa_pasta_modelos();   /* D380: la carpeta de los modelos de IA local */
    if (!sesion) aplicar_conectores();
    bool tem_conf = conf_ler() || maquina_separada();   /* D371: lo de la máquina se aplica aunque la cuenta no tenga sagui.conf */
    if (!sesion) acel_arranque(tem_conf);   /* D323: modo seguro, marca de arranque y aceleración (el kernel nos lanza como root) */
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
    if (conf.barra_def) registrar("configuracion: barra posicion %d altura %d menu %d tema %d -> %d", conf.barra[BC_POSICION], conf.barra[BC_ALTURA],
                                  conf.barra[BC_MENU], conf.barra[BC_TEMA], aplicar_barra());
    if (conf.fundo[0] && conf.modo != M_DEGRADE)
        registrar("configuracion: fondo %s (%s) -> %d", conf.fundo, nomes_modo[conf.modo], fundo_aplicar(conf.fundo, conf.modo));
    return 0;
}

/* ------------------------------------------------------------ línea de comandos */

/* configuracion barra mostrar | restablecer | CLAVE=N ... (D361). -1 = nada que guardar, > 0 = error. */
static int barra_cli(int argc, char **argv)
{
    info_barra_t ib;
    memset(&ib, 0, sizeof ib);
    info_sistema(INFO_BARRA, &ib, sizeof ib);
    if (!conf.barra_def) memcpy(conf.barra, ib.v, sizeof conf.barra);
    if (compara_texto(argv[2], "mostrar") == 0) {
        for (int b = 0; b < BC_N; b++) imprimir("%s=%d\n", claves_barra[b], ib.v[b]);
        imprimir("barra: visible %d en (%d,%d) %dx%d; menu Inicio abierto %d en (%d,%d) %dx%d\n", ib.visivel, ib.x, ib.y, ib.l, ib.a,
                 ib.menu_aberto, ib.menu_x, ib.menu_y, ib.menu_l, ib.menu_a);
        return -1;
    }
    if (compara_texto(argv[2], "restablecer") == 0) memcpy(conf.barra, barra_padrao, sizeof conf.barra);
    else {
        for (int i = 2; i < argc; i++) {
            char *eq = acha_char(argv[i], '=');
            int b = -1;
            if (eq) { *eq = 0; for (int k = 0; k < BC_N; k++) if (compara_texto(argv[i], claves_barra[k]) == 0) b = k; }
            if (b < 0) { imprimir_em(ERROS, "configuracion barra: %s no es CLAVE=N con una clave de la barra (barra_posicion, inicio_menu...)\n", argv[i]); return 2; }
            int x = texto_para_int(eq + 1);
            conf.barra[b] = (unsigned char)(x < 0 ? 0 : x > barra_maximo[b] ? barra_maximo[b] : x);
        }
    }
    conf.barra_def = 1;
    int r = aplicar_barra();
    imprimir("barra: aplicada (%d)\n", r);
    return 0;
}

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
    imprimir("  Reparto 2D: GPU %u copias y %u rellenos; software %u mover, %u consola, %u rellenos, %u volcados\n",
             ac2.copias, ac2.rellenos, ac2.sw_ops[0], ac2.sw_ops[1], ac2.sw_ops[2], ac2.sw_ops[3]);   /* D346 */
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

/* D367: las reglas de dependencia de la aceleración (conf_reglas.c): SDL 2 por la GPU usa el 3D, y en las Intel el 3D
 * comparte el anillo con el 2D. Y el disco: un solo sitio manda, la cabecera de arranque (kconfig dma / cache_disco); la
 * fila «Disco» de Aceleración se lee de ahí y, si lo de kconfig es una mezcla, lo dice. */
static const char *const acel_nom_regla[4] = { "a_2d", "a_3d", "a_sdl", "a_disco" };
static int acel_leer_regla(const char *n, void *c)
{
    (void)c;
    for (int f = 0; f < 4; f++)
        if (compara_texto(n, acel_nom_regla[f]) == 0) {
            int v = f == 0 ? conf.a_2d : f == 1 ? conf.a_3d : f == 2 ? conf.a_sdl : conf.a_disco;
            return v < 0 ? 0 : v;
        }
    return -1;
}
static int acel_comparte_anillo(void) { return contem(ac2.nome, "intel"); }
static const char *acel_bloqueo(int f) { const char *m = conf_bloqueo(acel_nom_regla[f], acel_leer_regla, NULL, acel_comparte_anillo()); return m ? _(m) : NULL; }
static int acel_fila_de(const char *n) { for (int f = 0; f < 4; f++) if (compara_texto(n, acel_nom_regla[f]) == 0) return f; return -1; }

static int acel_poner(int k, int t, char *res, size_t n)
{
    int r = 0;
    if (k == 0) { conf.a_2d = t; r = acel_aplicar_2d(t, res, n); }
    else if (k == 1) { conf.a_3d = t; r = acel_aplicar_3d(t, res, n); }
    else if (k == 2) { conf.a_sdl = t; r = acel_aplicar_sdl(t); formatar_texto(res, n, "SDL 2 por la GPU: %s", rotulos_tri[t]); }
    else { conf.a_disco = t; r = acel_aplicar_disco(t); formatar_texto(res, n, "disco: %s (se aplica al reiniciar)%s", rotulos_tri[t], r < 0 ? "; no se pudo grabar en la cabecera de arranque" : ""); if (r < 0) r = 0; }
    return r;
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
    acel_ler_estado();
    if (conf_activo(acel_nom_regla[k], t) && acel_bloqueo(k)) {   /* D367: nunca una combinación sin sentido */
        imprimir_em(ERROS, "configuracion: %s %s: %s; no se cambia nada\n", que, v, acel_bloqueo(k));
        return 2;
    }
    conf_ajuste_t aj[4];
    int na = conf_ajustes(acel_nom_regla[k], t, acel_leer_regla, NULL, acel_comparte_anillo(), aj, 4);
    int r = acel_poner(k, t, res, sizeof res);
    for (int a = 0; a < na; a++) {   /* lo que depende de lo que se apaga se apaga con aviso */
        char rr[200];
        int f = acel_fila_de(aj[a].nome);
        if (f >= 0) { acel_poner(f, aj[a].valor, rr, sizeof rr); imprimir("Ajustado: %s (%s)\n", rr, _(aj[a].motivo)); }
    }
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
    } else if (compara_texto(c, "barra") == 0 && argc >= 3) {   /* D361 */
        int r = barra_cli(argc, argv);
        if (r < 0) return 0;
        if (r > 0) return r;
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
                           "                      fuente documentos|titulos RUTA|ninguna | aceleracion [2d|3d|sdl|disco VALOR | restablecer] |\n"
                           "                      barra mostrar | restablecer | CLAVE=N...]\n");
        return 2;
    }
    aviso_persistencia(conf_gravar());
    return 0;
}

/* ------------------------------------------------------------ la ventana (D367) */

/* La ventana tiene un árbol de categorías a la izquierda, un panel con la sección elegida a la derecha y,
 * abajo, Aceptar / Cancelar / Aplicar (más «Predeterminado» de la sección). Cada sección dibuja y trata
 * sus eventos en coordenadas LOCALES del panel: el origen es la esquina de arriba a la izquierda del panel
 * (con 16 de margen a la izquierda, de ahí que las filas empiecen en x = 24) y el panel se desplaza con la
 * barra cuando la sección es más alta que él. Las macros de más abajo suman ese origen a los g_*() y a los
 * componentes (botón, caja, lista, casilla), así que el código de una sección no sabe dónde está.
 *
 *   LARG   ancho local; el contenido llega hasta LARG - 24 (a la derecha queda la barra de desplazamiento)
 *   ALT    alto local para las filas ancladas abajo: ALT - 100 = PH - 30
 *   Y_PAG  0: la primera fila de una sección está en y = 0..8 */
static int OX, OY;                 /* origen local del panel en la ventana; 0 cuando se dibuja lo que no es del panel */
static int PX, PY, PW, PH;         /* el rectángulo del panel (en la ventana) */
static int rolagem;                /* cuánto se ha desplazado el contenido */
static int cont_h;                 /* alto del contenido de la sección (lo pone su función de dibujo; 0 = cabe) */
#define LARG (PW + 22)
#define ALT  (PH + 70)
#define Y_PAG 0
#define XL   30                    /* columna de los controles de las secciones nuevas (dentro de un grupo) */
#define XR   (LARG - 36)           /* y su borde derecho */
#define LARG_MIN 700
#define ALT_MIN  440

/* Componentes con el origen del panel. Los de abajo llaman a los de la libsagui con las coordenadas de la ventana. */
static void botao_desenhar_o(botao_t *b) { b->x += OX; b->y += OY; botao_desenhar(jn, b); b->x -= OX; b->y -= OY; }
static bool botao_tratar_o(botao_t *b, const evento_t *e)
{
    evento_t a = *e;
    a.x += OX; a.y += OY; b->x += OX; b->y += OY;
    bool r = botao_tratar(jn, b, &a);
    b->x -= OX; b->y -= OY;
    return r;
}
static void caixa_desenhar_o(caixa_t *c) { c->x += OX; c->y += OY; caixa_desenhar(jn, c); c->x -= OX; c->y -= OY; }
static bool caixa_tratar_o(caixa_t *c, const evento_t *e)
{
    evento_t a = *e;
    a.x += OX; a.y += OY; c->x += OX; c->y += OY;
    bool r = caixa_tratar(jn, c, &a);
    c->x -= OX; c->y -= OY;
    return r;
}
static void lista_desenhar_o(lista_t *l) { l->x += OX; l->y += OY; lista_desenhar(jn, l); l->x -= OX; l->y -= OY; }
static int lista_tratar_o(lista_t *l, const evento_t *e)
{
    evento_t a = *e;
    a.x += OX; a.y += OY; l->x += OX; l->y += OY;
    int r = lista_tratar(jn, l, &a);
    l->x -= OX; l->y -= OY;
    return r;
}
#define botao_desenhar(j, b)    botao_desenhar_o(b)
#define botao_tratar(j, b, e)   botao_tratar_o((b), (e))
#define caixa_desenhar(j, c)    caixa_desenhar_o(c)
#define caixa_tratar(j, c, e)   caixa_tratar_o((c), (e))
#define lista_desenhar(j, l)    lista_desenhar_o(l)
#define lista_tratar(j, l, e)   lista_tratar_o((l), (e))
#define g_retangulo(j, x, y, ...)  g_retangulo(j, (x) + OX, (y) + OY, __VA_ARGS__)
#define g_moldura(j, x, y, ...)    g_moldura(j, (x) + OX, (y) + OY, __VA_ARGS__)
#define g_linha(j, x0, y0, x1, y1, c) g_linha(j, (x0) + OX, (y0) + OY, (x1) + OX, (y1) + OY, c)
#define g_texto(j, x, y, ...)      g_texto(j, (x) + OX, (y) + OY, __VA_ARGS__)
#define g_texto_n(j, x, y, ...)    g_texto_n(j, (x) + OX, (y) + OY, __VA_ARGS__)
#define g_relevo(j, x, y, ...)     g_relevo(j, (x) + OX, (y) + OY, __VA_ARGS__)
#define jan_mostrar_area(j, x, y, l, a) jan_mostrar_area(j, (x) + OX, (y) + OY, l, a)

/* Los grupos de una sección: un recuadro con título, como los de un panel de opciones clásico. Devuelve la y donde
 * empieza lo de dentro. El recuadro ocupa de x = 18 a LARG - 24. */
static int ui_grupo(int y, int alto, const char *titulo)
{
    const char *t = idioma_traducir_sl(titulo);
    int l = g_largura_texto(t);
    g_moldura(jn, 18, y + 8, LARG - 42, alto - 8, COR_BORDA);
    g_retangulo(jn, 26, y, l + 8, 16, COR_FUNDO);
    g_texto(jn, 30, y, t, COR_TEXTO);
    return y + 26;
}

/* Modo inmediato: una sección escribe UNA función que dibuja (ui_ev == NULL) y trata el evento (ui_ev != NULL) con
 * las mismas llamadas, así lo dibujado y lo pulsable nunca se separan. Los botones guardan su estado «apretado» en
 * un conjunto fijo, por orden de llamada. */
static const evento_t *ui_ev;
static bool ui_mudou;
static bool ui_gris;         /* lo que se dibuja a continuación está deshabilitado (texto gris, sin respuesta al ratón) */
static int ui_n;
static botao_t ui_bt[40];

static void ui_inicio(const evento_t *e) { ui_ev = e; ui_n = 0; ui_mudou = false; }

static bool ui_clic(int x, int y, int l, int a)
{
    return ui_ev && ui_ev->tipo == EV_CLIQUE && (ui_ev->botoes & 1) && ui_ev->x >= x && ui_ev->x < x + l && ui_ev->y >= y && ui_ev->y < y + a;
}

/* Botón; `premido` lo dibuja hundido (un valor elegido). true = pulsado (al soltar). */
static bool ui_boton(int x, int y, int l, const char *t, bool premido)
{
    botao_t *b = &ui_bt[ui_n < 39 ? ui_n++ : 39];
    b->x = x; b->y = y; b->l = l; b->a = 26; b->texto = t;
    if (!ui_ev) {
        bool ap = b->apertado;
        b->apertado = ap || premido;
        botao_desenhar(jn, b);
        b->apertado = ap;
        return false;
    }
    return botao_tratar(jn, b, ui_ev);
}

/* Casilla de verificación con su texto; true = se ha pulsado (la sección cambia el valor). */
static bool ui_casilla(int x, int y, const char *t, bool marcada)
{
    const char *r = idioma_traducir_sl(t);
    int l = 22 + g_largura_texto(r);
    if (ui_ev) return !ui_gris && ui_clic(x, y - 1, l, 18);
    g_retangulo(jn, x, y, 14, 14, ui_gris ? COR_BOTAO : COR_BRANCO);
    g_moldura(jn, x, y, 14, 14, COR_BORDA);
    if (marcada) {
        g_linha(jn, x + 3, y + 7, x + 6, y + 10, COR_DESTAQUE);
        g_linha(jn, x + 6, y + 10, x + 11, y + 3, COR_DESTAQUE);
        g_linha(jn, x + 3, y + 8, x + 6, y + 11, COR_DESTAQUE);
        g_linha(jn, x + 6, y + 11, x + 11, y + 4, COR_DESTAQUE);
    }
    g_texto(jn, x + 22, y - 1, r, ui_gris ? COR_TEXTO_FRACO : COR_TEXTO);
    return false;
}

/* Opción de un grupo de opciones (un círculo); true = se ha elegido. */
static bool ui_opcion(int x, int y, const char *t, bool elegida)
{
    const char *r = idioma_traducir_sl(t);
    int l = 22 + g_largura_texto(r);
    if (ui_ev) return !ui_gris && ui_clic(x, y - 1, l, 18);
    static const signed char borde[14][2] = { {4, 6}, {2, 2}, {1, 1}, {1, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {1, 1}, {1, 1}, {2, 2}, {4, 6} };
    for (int f = 0; f < 14; f++) {
        int e0 = borde[f][0], n = borde[f][1];
        if (f == 0 || f == 13) { g_retangulo(jn, x + 4, y + f, 6, 1, COR_BORDA); g_retangulo(jn, x + 4, y + f + (f == 0 ? 1 : -1), 6, 1, COR_BRANCO); continue; }
        int ancho = 14 - 2 * e0;
        g_retangulo(jn, x + e0 + n, y + f, ancho - 2 * n, 1, COR_BRANCO);
        g_retangulo(jn, x + e0, y + f, n, 1, COR_BORDA);
        g_retangulo(jn, x + 14 - e0 - n, y + f, n, 1, COR_BORDA);
    }
    if (elegida) g_retangulo(jn, x + 4, y + 4, 6, 6, COR_DESTAQUE);
    g_texto(jn, x + 22, y - 1, r, ui_gris ? COR_TEXTO_FRACO : COR_TEXTO);
    return false;
}

static void ui_texto(int x, int y, const char *t, uint32_t cor)
{
    if (ui_ev) return;
    g_texto_n(jn, x, y, t, (XR - x) / LETRA_L, cor);
}

/* Párrafo con ajuste de línea por palabras; devuelve la y siguiente. */
static int ui_parrafo(int x, int y, int l, const char *t, uint32_t cor)
{
    int cols = l / LETRA_L;
    if (cols < 8) cols = 8;
    int n = (int)tam_texto(t), i = 0;
    while (i < n) {
        int fin = i + cols;
        if (fin >= n) fin = n;
        else { int k = fin; while (k > i && t[k] != ' ') k--; if (k > i) fin = k; }
        if (!ui_ev) g_texto_n(jn, x, y, t + i, fin - i, cor);
        y += LETRA_A + 2;
        i = fin;
        while (i < n && t[i] == ' ') i++;
    }
    return y;
}

/* El registro de secciones. Está más abajo (seccoes[]); aquí los índices. */
enum { S_SISTEMA, S_TELA, S_ACEL, S_KERNEL, S_ZONA, S_IDIOMA, S_USUARIOS,
       S_ESCRITORIO, S_COLORES, S_BARRA, S_WIDGETS, S_FUENTES, S_CAPTURAS,
       S_DISPOSITIVOS, S_TECLADO, S_MANDOS, S_SONIDO,
       S_REDES, S_BLOQUEO, S_VPN, S_VNC,
       S_IA, S_QTD };

static int pagina = S_TELA;      /* la sección abierta (índice de la tabla) */
static char estado[160];
static bool persiste;
static caixa_t c_capturas;   /* carpeta de las capturas (D125) */
static bool cap_pend;        /* la carpeta escrita aún no se ha aplicado */
static const char *const codigos_idi[3] = { "es", "pt_BR", "en" };

/* Fecha y hora */
#define MAXZ 128
static char itens_zona[MAXZ][64];
static int  indice_zona[MAXZ];
static lista_t lista_zona;
static botao_t b_zona = { 24, 0, 120, 28, N_("Aplicar"), false };

/* Pantalla */
#define MAXM 192
static modo_video_t modos[MAXM];
static int qtd_modos;
static char itens_modo[MAXM][64];
static int indice_modo_l[MAXM];
static lista_t lista_modo;
static botao_t b_tela[2] = { { 24, 0, 120, 28, N_("Aplicar"), false }, { 154, 0, 180, 28, N_("Aplicar y guardar"), false } };
static info_tela_t tela;
/* D290: varias pantallas. Solo se ven si el kernel tiene más de una salida de vídeo. */
static info_telas_t telas;
static int sel_res;   /* la pantalla a la que se aplica la resolución de la lista */
static char rot_prin[32], rot_lado[32], rot_res[40];
static botao_t b_telas[5] = { { 24, 0, 100, 28, N_("Extender"), false }, { 128, 0, 90, 28, N_("Espejo"), false },
                              { 222, 0, 120, 28, rot_prin, false }, { 346, 0, 150, 28, rot_lado, false },
                              { 24, 0, 250, 28, rot_res, false } };

/* Sonido: las posiciones se calculan en snd_calc() según lo que tenga la tarjeta. */
static botao_t b_vol[3] = { { 0, 0, 28, 24, "-", false }, { 0, 0, 28, 24, "+", false }, { 0, 0, 140, 28, N_("Silenciar"), false } };
static bool arrastando_vol;
/* Ecualizador (D201): cinco deslizadores verticales de -12 a +12 dB y los preajustes. */
#define EQ_ALT  96                /* alto del carril: 4 px por dB */
#define EQ_PASSO 84
static const char *const eq_rotulos[EQ_BANDAS] = { "60 Hz", "250 Hz", "1 kHz", "4 kHz", "12 kHz" };
static const char *const eq_nomes_pre[4] = { "plano", "graves", "voz", "agudos" };
static botao_t b_eq[4] = { { 0, 0, 110, 26, N_("Plano"), false }, { 0, 0, 110, 26, N_("Graves"), false },
                           { 0, 0, 110, 26, N_("Voz"), false }, { 0, 0, 110, 26, N_("Agudos"), false } };
static int eq_arrasta = -1;

/* D292: balance, pruebas de altavoz, latencia y micrófono. */
#define BAL_L 200
#define BAL_A 24
static botao_t b_bal = { 0, 0, 76, 28, N_("Centro"), false };
static botao_t b_prob[2] = { { 0, 0, 110, 26, N_("Probar izq."), false }, { 0, 0, 110, 26, N_("Probar der."), false } };
static botao_t b_lat[2] = { { 0, 0, 24, 26, "-", false }, { 0, 0, 24, 26, "+", false } };
#define MIC_GL 200
static botao_t b_mic[5] = { { 0, 0, 24, 24, "<", false }, { 0, 0, 24, 24, ">", false },
                            { 0, 0, 24, 24, "-", false }, { 0, 0, 24, 24, "+", false },
                            { 0, 0, 216, 26, N_("Silenciar micrófono"), false } };
static botao_t b_sal = { 0, 0, 100, 24, "", false };
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

/* Escritorio > Colores y fondo */
#define MAXF 64
static char itens_img[MAXF][64];
static lista_t lista_img;

/* ------------------------------------------------------------ las fuentes (D196) */

static void titulo_pag(int x, int y, const char *t);
#define FN_LISTA_L 230
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
static botao_t b_fnt[3] = { { 0, 0, 140, 28, N_("Para documentos"), false },
                            { 0, 0, 110, 28, N_("Para títulos"), false },
                            { 0, 0, 70, 28, N_("Quitar"), false } };
static botao_t b_fnt_inst = { 0, 0, 190, 28, N_("Instalar fuentes..."), false };   /* D356: abre «fuentes --ventana» */
/* La fila de botones de abajo (se recoloca al dibujar: depende del alto del panel). */
static void fuentes_pos(void)
{
    int y = ALT - 100;
    b_fnt[0].x = 24;  b_fnt[0].y = y;
    b_fnt[1].x = 172; b_fnt[1].y = y;
    b_fnt[2].x = 290; b_fnt[2].y = y;
    b_fnt_inst.x = 372; b_fnt_inst.y = y;
    if (n_fnt <= 0) { b_fnt_inst.x = 24; b_fnt_inst.y = 90; }   /* sin fuentes: solo el botón de instalar, arriba */
}
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
        fonte_texto_buf_n(f, jn->px, jn->larg, jn->alt, x + OX, y + OY, t + i, largo, px, 0x000000);
        y += linea;
        i += largo;
        while (i < n && t[i] == ' ') i++;
    }
    return y;
}

static void fuentes_desenhar(void)
{
    char t[160];
    fuentes_pos();
    if (n_fnt <= 0) {
        titulo_pag(24, Y_PAG + 14, _("No hay fuentes instaladas."));
        {
            char b[160];
            formatar_texto(b, sizeof b, _("Se buscan ficheros .otf y .ttf en %s y en %s."), FUENTES_DIR_APPS, FUENTES_DIR_HOME);
            g_texto(jn, 24, Y_PAG + 40, b, COR_TEXTO_FRACO);
        }
        g_texto(jn, 24, Y_PAG + 60, _("Las de Typodermic (CC0) vienen en la partición SAGUI-APPS."), COR_TEXTO_FRACO);
        botao_desenhar(jn, &b_fnt_inst);
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
    botao_desenhar(jn, &b_fnt_inst);
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
static botao_t b_ia[2] = { { 230, 0, 100, 28, N_("Guardar"), false },
                           { 338, 0, 170, 28, N_("Cargar modelos"), false } };
/* Los botones que eligen dónde va el modelo que se elige en la lista. */
static botao_t b_alvo[4] = { { 230, 0, 82, 28, N_("General"), false },
                             { 316, 0, 82, 28, N_("Terminal"), false },
                             { 402, 0, 82, 28, N_("Chat"), false },
                             { 488, 0, 82, 28, N_("Mico"), false } };

static void ia_desenhar(void);

static void tela_pos(void);
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
    tela_pos();
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

/* Escritorio remoto (D273, mejora 104): el servidor VNC `vncd`. Lo de root se hace llamando a
 * «vncd --ajustar/--clave/--desconectar» y «servicio» con el diálogo de permisos (D269): la contraseña
 * pasa por la línea de órdenes del hijo, que es root solo mientras dura. */
static botao_t b_vnc[5] = {
    { 24, Y_PAG + 58, 230, 28, N_("Activar y arrancar"), false },
    { 262, Y_PAG + 58, 130, 28, N_("Reiniciar"), false },
    { 252, Y_PAG + 122, 170, 28, N_("Fijar contraseña"), false },
    { 24, Y_PAG + 170, 220, 28, "", false },
    { 252, Y_PAG + 170, 316, 28, "", false },
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
    cont_h = ui_parrafo(24, Y_PAG + 326, LARG - 48,
               _("«Solo esta máquina»: se entra por un túnel SSH (ssh -L 5900:localhost:5900 root@IP) y el tráfico va cifrado. «Red local» deja entrar sin túnel, pero el tráfico VNC no se cifra. Mientras haya alguien conectado se ve una ventana «Escritorio remoto» en la barra de tareas."),
               COR_TEXTO_FRACO);
}

/* ------------------------------------------------------- la página de los mandos (D307) */

#define MD_X0   24                  /* la prueba en vivo, debajo de la lista (D367) */
#define MD_Y0   (Y_PAG + 216)
#define MD_CX   96                  /* lado del cuadro de cada par de ejes */
static info_mando_t md_l[MANDOS_MAX];
static int md_n;
static unsigned md_gen = 0xFFFFFFFFu;
static char itens_md[MANDOS_MAX][64];
static lista_t lista_md;
static botao_t b_md[3] = { { 366, Y_PAG + 30, 196, 28, N_("Buscar joystick"), false },
                           { 366, Y_PAG + 64, 196, 28, N_("Calibrar puerto"), false },
                           { 366, Y_PAG + 98, 196, 28, N_("Calibración automática"), false } };
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
    lista_md = (lista_t){ 24, Y_PAG + 30, 330, 170, itens_md, 0, -1, 0, NULL };
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
    lista_desenhar(jn, &lista_md);
    if (md_n <= 0) ui_parrafo(30, Y_PAG + 38, 316, _("Ninguno (USB, Xbox 360 o puerto de juegos)."), COR_TEXTO_FRACO);
    for (int i = 0; i < 3; i++) botao_desenhar(jn, &b_md[i]);
    ui_parrafo(366, Y_PAG + 134, 196, _("El joystick del puerto de juegos (0x201) se busca al arrancar y con «Buscar»."), COR_TEXTO_FRACO);
    cont_h = MD_Y0;
    if (lista_md.sel < 0 || lista_md.sel >= md_n) return;
    cont_h = MD_Y0 + 332;
    const info_mando_t *m = &md_l[lista_md.sel];
    titulo_pag(MD_X0, MD_Y0, _("Prueba en vivo"));
    if (m->fabricante || m->produto)
        formatar_texto(t, sizeof t, _("%s, %04x:%04x: %u ejes, %u botones, %u hats"), m->via, m->fabricante, m->produto,
                       m->n_eixos, m->n_botoes, m->n_hats);
    else formatar_texto(t, sizeof t, _("%s: %u ejes, %u botones, %u hats"), m->via, m->n_eixos, m->n_botoes, m->n_hats);
    g_texto_n(jn, MD_X0, MD_Y0 + 20, t, (LARG - MD_X0 - 20) / LETRA_L, COR_TEXTO_FRACO);
    /* Ejes de dos en dos: un cuadro con un punto (X a la derecha, Y hacia abajo). El Xbox 360 tiene
     * el orden de SDL (LX LY LT RX RY RT): se dibujan los sticks y después los gatillos. */
    static const signed char pares_xbox[3][2] = { { 0, 1 }, { 3, 4 }, { 2, 5 } };
    static const char *const nomes_xbox[3] = { N_("Stick izq."), N_("Stick der."), N_("Gatillos") };
    bool xbox = m->mapa == MANDO_MAPA_XBOX360 && m->n_eixos >= 6;
    int pares = xbox ? 3 : (m->n_eixos + 1) / 2;
    if (pares > 4) pares = 4;
    for (int p = 0; p < pares; p++) {
        int x = MD_X0 + p * (MD_CX + 12), y = MD_Y0 + 40;
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
    int yb = MD_Y0 + 164;
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
        g_texto(jn, MD_X0, MD_Y0 + 296, t, COR_TEXTO_FRACO);
        formatar_texto(t, sizeof t, _("Calibración %s: X %u-%u-%u, Y %u-%u-%u us"), manual ? _("guardada") : _("automática"),
                       c[0][0] / 10, c[0][1] / 10, c[0][2] / 10, c[1][0] / 10, c[1][1] / 10, c[1][2] / 10);
        g_texto_n(jn, MD_X0, MD_Y0 + 314, t, (LARG - MD_X0 - 20) / LETRA_L, COR_TEXTO_FRACO);
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

/* ------------------------------------------------------------ Usuarios (D340, mejora 098 fase 3) */
/* Las cuentas de persona del ordenador. Verlas no pide nada; cambiarlas pide permisos de administrador
 * con el diálogo del sistema (D269) justo antes del cambio, y se vuelve a la cuenta de antes después. */
#define US_MAX 16
static conta_resumo_t us_v[US_MAX];
static int us_n;
static char itens_us[US_MAX][64];
static lista_t lista_us;
static bool us_donos;
#define US_BX 336
static botao_t b_us[5] = { { US_BX, Y_PAG + 34, 226, 28, N_("Nueva cuenta..."), false },
                           { US_BX, Y_PAG + 70, 226, 28, N_("Cambiar la contraseña..."), false },
                           { US_BX, Y_PAG + 106, 226, 28, N_("Cambiar el nombre..."), false },
                           { US_BX, Y_PAG + 142, 226, 28, N_("Administrador: sí / no"), false },
                           { US_BX, Y_PAG + 178, 226, 28, N_("Borrar la cuenta..."), false } };

static void us_listar(void)
{
    int sel = lista_us.sel;
    us_n = contas_resumo(us_v, US_MAX);
    for (int i = 0; i < us_n; i++)
        formatar_texto(itens_us[i], sizeof itens_us[i], "%s%s%s%s", us_v[i].nome, us_v[i].admin ? _(" (administrador)") : "",
                       us_v[i].descricao[0] ? " - " : "", us_v[i].descricao);
    lista_us.qtd = us_n;
    lista_us.sel = sel >= 0 && sel < us_n ? sel : (us_n ? 0 : -1);
    us_donos = conta_home_com_donos();
}

static void us_preparar(void)
{
    lista_us = (lista_t){ 24, Y_PAG + 34, US_BX - 24 - 16, 230, itens_us, 0, -1, 0, NULL, NULL };
    us_listar();
}

static void us_desenhar(void)
{
    titulo_pag(24, Y_PAG + 10, _("Cuentas de usuario de este ordenador"));
    lista_desenhar(jn, &lista_us);
    for (int i = 0; i < 5; i++) botao_desenhar(jn, &b_us[i]);
    int y = Y_PAG + 276, w = LARG - 48;
    y = ui_parrafo(24, y, w, _("Cada cuenta tiene su carpeta /home/<nombre>, con su configuración."), COR_TEXTO_FRACO);
    y = ui_parrafo(24, y, w, _("La cuenta principal (sagui) sigue usando /home."), COR_TEXTO_FRACO);
    y = ui_parrafo(24, y, w, _("Un administrador autoriza los cambios del sistema con su propia contraseña."), COR_TEXTO_FRACO);
    y = ui_parrafo(24, y, w, _("Los cambios piden permisos de administrador."), COR_TEXTO_FRACO);
    if (!us_donos)
        y = ui_parrafo(24, y + 4, w, _("Aviso: /home no guarda dueños (FAT32): no se pueden crear cuentas."), 0xB03000);
    cont_h = y + 8;
}

/* Pide permisos de administrador; false (y lo dice) si no se conceden. */
static bool us_elevar(void)
{
    int r = elevar_grafico(_("administrar las cuentas de usuario"));
    if (r == 0) return true;
    dizer(r == ERRO_ACESSO ? _("No se concedieron los permisos de administrador.") : nome_erro(r));
    return false;
}

static void us_resultado(int r, const char *bien)
{
    if (r == 0) dizer(bien);
    else if (r == ERRO_ACESSO) dizer(_("No se puede: es una cuenta del sistema, la principal o la de esta sesión."));
    else if (r == ERRO_JA_EXISTE) dizer(_("Ya hay una cuenta (o una carpeta en /home) con ese nombre."));
    else if (r == ERRO_NAO_SUPORTADO) dizer(_("/home no guarda dueños ni permisos (FAT32): no se pueden crear cuentas."));
    else if (r == ERRO_INVALIDO) dizer(_("Nombre o contraseña no válidos (contraseña: al menos 4 caracteres)."));
    else formatar_texto(estado, sizeof estado, _("No se pudo: %s"), nome_erro(r));
}

/* Una contraseña nueva dos veces (vacía = sin contraseña si se permite). false = cancelado o distintas. */
static bool us_senha(const char *tit, char *s, int max, bool vazia)
{
    char otra[PED_SENHA_MAX];
    s[0] = 0;
    if (!pedir_texto_oculto(tit, vazia ? _("Contraseña (vacía = sin contraseña; al menos 4 caracteres):")
                                       : _("Contraseña nueva (al menos 4 caracteres):"), s, max)) return false;
    if (!s[0] && vazia) return true;
    otra[0] = 0;
    bool ok = pedir_texto_oculto(tit, _("Repite la contraseña:"), otra, sizeof otra) && compara_texto(s, otra) == 0;
    memset(otra, 0, sizeof otra);
    if (!ok) { memset(s, 0, (size_t)max); dizer(_("Las dos contraseñas no coinciden.")); }
    return ok;
}

static void us_nova(void)
{
    const char *tit = _("Nueva cuenta");
    char nome[32] = "", descr[64] = "", senha[PED_SENHA_MAX];
    if (!pedir_texto(tit, _("Nombre de la cuenta (minúsculas, números, _ y -):"), nome, sizeof nome)) return;
    if (!conta_nome_valido(nome)) { dizer(_("Nombre no válido: minúsculas, números, _ y -, hasta 31.")); return; }
    if (!pedir_texto(tit, _("Nombre completo o descripción (opcional):"), descr, sizeof descr)) return;
    if (!us_senha(tit, senha, sizeof senha, true)) return;
    bool admin = confirmar(tit, _("¿Será administrador?\n(Podrá autorizar cambios del sistema con su contraseña.)"));
    if (!us_elevar()) { memset(senha, 0, sizeof senha); return; }
    int r = conta_criar(nome, descr, senha, admin, NULL);
    memset(senha, 0, sizeof senha);
    elevacao_soltar();
    registrar("configuracion: usuarios: crear %s -> %d", nome, r);
    us_resultado(r, _("Cuenta creada. Puede entrar desde «Cerrar sesión» o al reiniciar."));
    us_listar();
}

static void us_mudar_senha(conta_resumo_t *c)
{
    char senha[PED_SENHA_MAX];
    if (!us_senha(_("Cambiar la contraseña"), senha, sizeof senha, true)) return;
    if (!us_elevar()) { memset(senha, 0, sizeof senha); return; }
    int r = conta_senha(c->nome, senha);
    memset(senha, 0, sizeof senha);
    elevacao_soltar();
    registrar("configuracion: usuarios: contraseña de %s -> %d", c->nome, r);
    us_resultado(r, _("Contraseña cambiada."));
}

static void us_renomear(conta_resumo_t *c)
{
    char novo[32];
    copia_texto(novo, c->nome, sizeof novo);
    if (!pedir_texto(_("Cambiar el nombre"), _("Nombre nuevo de la cuenta (su carpeta cambia con él):"), novo, sizeof novo)) return;
    if (compara_texto(novo, c->nome) == 0) return;
    if (!us_elevar()) return;
    int r = conta_renomear(c->nome, novo);
    elevacao_soltar();
    registrar("configuracion: usuarios: renombrar %s a %s -> %d", c->nome, novo, r);
    us_resultado(r, _("Nombre cambiado."));
    us_listar();
}

static void us_admin(conta_resumo_t *c)
{
    if (!us_elevar()) return;
    int r = conta_admin_fixar(c->nome, !c->admin);
    elevacao_soltar();
    registrar("configuracion: usuarios: administrador %s %s -> %d", c->nome, c->admin ? "no" : "si", r);
    us_resultado(r, c->admin ? _("Ya no es administrador.") : _("Ahora es administrador."));
    us_listar();
}

static void us_apagar(conta_resumo_t *c)
{
    char p[160];
    formatar_texto(p, sizeof p, _("¿Borrar la cuenta %s?\nYa no podrá entrar."), c->nome);
    if (!confirmar(_("Borrar la cuenta"), p)) return;
    formatar_texto(p, sizeof p, _("¿Borrar también su carpeta /home/%s con todos sus archivos?\n(No = se conservan.)"), c->nome);
    bool arquivos = confirmar(_("Borrar la cuenta"), p);
    char nome[32];
    copia_texto(nome, c->nome, sizeof nome);
    if (!us_elevar()) return;
    int r = conta_apagar(nome, arquivos);
    elevacao_soltar();
    registrar("configuracion: usuarios: borrar %s (archivos %d) -> %d", nome, arquivos, r);
    us_resultado(r, arquivos ? _("Cuenta y carpeta borradas.") : _("Cuenta borrada; su carpeta se conserva."));
    us_listar();
}

static bool us_tratar(const evento_t *e)
{
    bool mudou = lista_tratar(jn, &lista_us, e) != 0;
    for (int i = 0; i < 5; i++)
        if (botao_tratar(jn, &b_us[i], e)) {
            mudou = true;
            if (i == 0) { us_nova(); continue; }
            if (lista_us.sel < 0 || lista_us.sel >= us_n) { dizer(_("Elige una cuenta de la lista.")); continue; }
            conta_resumo_t c = us_v[lista_us.sel];
            if (i == 1) us_mudar_senha(&c);
            else if (i == 2) us_renomear(&c);
            else if (i == 3) us_admin(&c);
            else us_apagar(&c);
        }
    return mudou;
}

/* ------------------------------------------------------------ Sistema > Aceleración gráfica (D323, D367) */
static char acel_res[4][160];  /* el resultado de lo último hecho en cada fila */
static char acel_arq[200];     /* el aviso que dejó el arranque (/tmp/acel-aviso) */

static void acel_preparar(void)
{
    acel_ler_estado();
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

/* D399: energía de la tarjeta en el bloque «Tarjeta gráfica»: relojes (actuales y de la BIOS), temperatura (o
 * «sin sensor») con las zonas térmicas del ACPI aparte, la VRAM usada y el modo, con «Cambiar» (aplica ahora:
 * BIOS, bajo, alto, automático). En una tarjeta sin confirmar el botón queda gris: se cambia con «acel2d energia»
 * como administrador; el modo al arrancar está en Sistema > Kernel (gpu_energia). */
static const char *const en_modos[] = { "?", N_("BIOS (sin tocar)"), N_("bajo"), N_("alto"), N_("automático"), N_("manual") };

static void acel_energia_cambiar(void)
{
    if (!acel_permiso(_("cambiar el modo de energía de la tarjeta"))) return;
    unsigned sig = ac_en.modo >= GPU_EN_MODO_AUTO ? GPU_EN_MODO_BIOS : ac_en.modo + 1;
    int r = info_sistema(INFO_GPU_ENERGIA_CTL, (void *)(long)GPU_EN_PEDIDO(GPU_EN_OP_MODO, sig), 0);
    acel_ler_estado();
    registrar("configuracion: gpu energia modo %u -> %d (%s)", sig, r, ac_en.ultimo);
    formatar_texto(estado, sizeof estado, r == 0 ? _("Energía de la GPU: %s") : _("No se pudo: %s"), r == 0 ? _(en_modos[ac_en.modo <= 5 ? ac_en.modo : 0]) : ac_en.ultimo);
    elevacao_soltar();
}

static int acel_bloque_energia(int ey, int w)
{
    char t[200], a[24], b[24], c[24], d[24];
    if (!ac_en.versao || ac_en.soporte == GPU_EN_SIN) {
        return ui_parrafo(XL, ey, w, _("Energía: sin driver de energía para esta tarjeta (relojes y temperatura no se leen)."), COR_TEXTO_FRACO);
    }
    formatar_texto(a, sizeof a, "%u MHz", (ac_en.sclk_khz + 500) / 1000);
    formatar_texto(b, sizeof b, "%u MHz", (ac_en.sclk_def_khz + 500) / 1000);
    formatar_texto(c, sizeof c, "%u MHz", (ac_en.mclk_khz + 500) / 1000);
    formatar_texto(d, sizeof d, "%u MHz", (ac_en.mclk_def_khz + 500) / 1000);
    formatar_texto(t, sizeof t, _("Relojes: motor %s (BIOS %s), memoria %s (BIOS %s)%s"), a, b, c, d,
                   ac_en.relojes_fuente == 2 ? _("; sin leer: los de por defecto") : "");
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    if (ac_en.temp_mili_c != GPU_EN_SIN_SENSOR) formatar_texto(t, sizeof t, _("Temperatura de la tarjeta: %d °C"), ac_en.temp_mili_c / 1000);
    else formatar_texto(t, sizeof t, _("Temperatura de la tarjeta: %s"), ac_en.temp_fuente);
    for (unsigned i = 0; i < ac_en.n_zonas && i < GPU_EN_MAX_ZONAS; i++) {
        char z[40];
        formatar_texto(z, sizeof z, i ? ", %s %d °C" : _("; zonas del ACPI: %s %d °C"), ac_en.zona_nome[i], ac_en.zona_mili_c[i] / 1000);
        junta_texto(t, z, sizeof t);
    }
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    if (ac_en.vram_kb) {
        formatar_texto(t, sizeof t, _("VRAM usada por SaguiOS: unos %u de %u MB"), (ac_en.vram_usada_kb + 1023) / 1024, ac_en.vram_kb / 1024);
        ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    }
    if (ac_en.estado >= 0) formatar_texto(a, sizeof a, _("estado %d"), ac_en.estado);
    else copia_texto(a, _("relojes de la BIOS"), sizeof a);
    formatar_texto(t, sizeof t, _("Energía: %s, %s%s"), _(en_modos[ac_en.modo <= 5 ? ac_en.modo : 0]), a,
                   ac_en.bloqueado ? _(" (bloqueado tras un fallo hasta reiniciar)") : "");
    ey = ui_parrafo(XL, ey + 4, w - 140, t, COR_TEXTO);
    bool puede = ac_en.soporte == GPU_EN_CONTROL && !ac_en.bloqueado && ac_en.confirmada;
    ui_gris = !puede;
    if (ui_boton(XR - 130, ey - 20, 130, N_("Cambiar modo"), false)) { acel_energia_cambiar(); ui_mudou = true; }
    ui_gris = false;
    if (ac_en.soporte == GPU_EN_CONTROL && !ac_en.confirmada)
        ey = ui_parrafo(XL, ey, w, _("Cambiar los relojes en esta tarjeta está sin confirmar: «acel2d energia» como administrador."), COR_TEXTO_FRACO);
    return ey + 4;
}

static int acel_valor(int f) { int v = f == 0 ? conf.a_2d : f == 1 ? conf.a_3d : f == 2 ? conf.a_sdl : conf.a_disco; return v < 0 ? 0 : v; }

/* El valor de la fila Disco según la cabecera (kconfig): 0 automático, 2 desactivado, -1 una mezcla (DMA y caché distintos). */
static int acel_disco_cabecera(void)
{
    info_kconf_t kk[KCONF_MAX];
    int nk = kconf_ler(kk, KCONF_MAX, NULL), dma = -1, cache = -1;
    for (int i = 0; i < nk; i++) {
        if (compara_texto(kk[i].nome, "dma") == 0) dma = kk[i].proximo != 0;
        if (compara_texto(kk[i].nome, "cache_disco") == 0) cache = kk[i].proximo != 0;
    }
    if (dma < 0 || cache < 0) return acel_valor(3);
    return dma && cache ? 0 : !dma && !cache ? 2 : -1;
}

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

/* D367: cambia una fila y, en cascada, las que dependen de ella (el 3D si se apaga el 2D en una Intel; la SDL 2 si se apaga el 3D). */
static void acel_cambiar(int f, int t)
{
    conf_ajuste_t aj[4];
    int na = conf_ajustes(acel_nom_regla[f], t, acel_leer_regla, NULL, acel_comparte_anillo(), aj, 4);
    acel_escolher(f, t);
    for (int a = 0; a < na; a++) {
        int g = acel_fila_de(aj[a].nome);
        if (g < 0) continue;
        char prev[160];
        copia_texto(prev, estado, sizeof prev);
        acel_escolher(g, aj[a].valor);
        formatar_texto(estado, sizeof estado, "%s -> %s (%s)", prev, g == 3 ? "disco" : g == 2 ? "SDL 2" : g == 1 ? "3D" : "2D", _(aj[a].motivo));
        registrar("configuracion: aceleracion ajuste %s (%s)", acel_nom_regla[g], aj[a].motivo);
    }
}

/* «Predeterminado» de la sección: los valores seguros (todo en automático). */
static void acel_predet(void)
{
    if (!acel_permiso("restablecer la aceleración gráfica a los valores seguros")) return;
    acel_restablecer();
    for (int f = 0; f < 4; f++) acel_res[f][0] = 0;
    int g = conf_gravar();
    registrar("configuracion: aceleracion restablecida a los valores seguros");
    dizer(g == 1 ? "Valores seguros restablecidos en esta sesión (/home no es persistente)." : "Valores seguros restablecidos: todo en automático.");
    acel_ler_estado();
    elevacao_soltar();
}

/* Dibuja (ui_ev == NULL) o trata un evento (ui_ev) de la sección. Los cuatro grupos de opciones van primero y miden
 * siempre lo mismo (88 de alto + 10 de separación, con sitio para dos líneas de resultado): así sus controles no se mueven
 * cuando aparece un resultado. Debajo, el estado de la tarjeta y los drivers. */
static void pg_acel(void)
{
    char t[200];
    static const char *const rot_fila[4] = { N_("2D por la GPU"), N_("3D por la GPU"), N_("SDL 2 por la GPU"), N_("Disco: DMA y caché") };
    static const char *const rot_tri[3] = { N_("Automático"), N_("Activado"), N_("Desactivado") };
    int w = XR - XL;
    if (!ui_ev) acel_ler_estado();
    int y = 0;
    for (int f = 0; f < 4; f++) {
        int gy = y, cy = y + 26, nb = f == 3 ? 2 : 3, v = f == 3 ? acel_disco_cabecera() : acel_valor(f);
        const char *bloq = acel_bloqueo(f);   /* D367: deshabilitada por otra fila: solo «Desactivado» */
        for (int s = 0; s < nb; s++) {
            int tv = f == 3 ? (s == 0 ? TRI_AUTO : TRI_NO) : s;
            const char *rot = f == 3 && s == 1 ? rot_tri[2] : rot_tri[s];
            ui_gris = bloq != NULL && tv != TRI_NO;
            if (ui_opcion(XL + s * 124, cy, rot, v == tv) && v != tv) {
                acel_cambiar(f, tv);
                acel_ler_estado();
                ui_mudou = true;
            }
        }
        ui_gris = false;
        if (f < 2 && ui_boton(XR - 130, cy - 6, 130, N_("Probar ahora"), false)) { acel_probar_fila(f); ui_mudou = true; }
        if (bloq && !ui_ev) ui_parrafo(XL, cy + 24, w, bloq, COR_TEXTO_FRACO);
        else if (f == 3 && v == -1 && !ui_ev) ui_parrafo(XL, cy + 24, w, _("Mezcla en Sistema > Kernel: DMA y caché de disco con valores distintos."), COR_TEXTO_FRACO);
        else if (acel_res[f][0] && !ui_ev) {
            char rr[160];
            int maxc = w / LETRA_L;
            copia_texto(rr, acel_res[f], sizeof rr);
            /* dos líneas como mucho; si no caben, termina en «...» */
            if ((int)tam_texto(rr) > 2 * maxc - 4) { rr[2 * maxc - 4] = '.'; rr[2 * maxc - 3] = '.'; rr[2 * maxc - 2] = '.'; rr[2 * maxc - 1] = 0; }
            ui_parrafo(XL, cy + 24, w, rr, COR_TEXTO_FRACO);
        }
        if (!ui_ev) ui_grupo(gy, 88, rot_fila[f]);
        y += 98;
    }
    /* El estado de la tarjeta y de los drivers: bloque fijo de solo lectura (nada se enciende por mirar, D346) */
    int ey = y + 26;
    char t2[200];
    formatar_texto(t, sizeof t, _("Tarjeta: %s"), ac_modelo[0] ? ac_modelo : (ac3.placa[0] ? ac3.placa : _("sin acelerador reconocido")));
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    /* D387: la de la tarjeta (su driver) y, si es otra, la que anuncia la BIOS (VBE). */
    if (ac2.vram_kb) {
        formatar_texto(t, sizeof t, _("Memoria de vídeo: %u MB"), ac2.vram_kb / 1024);
        if (ac_vid.vbe_versao && ac_vid.memoria_kb && ac_vid.memoria_kb != ac2.vram_kb) {
            formatar_texto(t2, sizeof t2, _(" (la BIOS anuncia %u MB)"), ac_vid.memoria_kb / 1024);
            junta_texto(t, t2, sizeof t);
        }
    } else if (ac_vid.vbe_versao && ac_vid.memoria_kb) formatar_texto(t, sizeof t, _("Memoria de vídeo: %u MB (anunciada por la BIOS)"), ac_vid.memoria_kb / 1024);
    else formatar_texto(t, sizeof t, "%s", _("Memoria de vídeo: no informada"));
    if (ac3.memoria_kb && ac3.estado == GPU3D_ATIVO) {
        formatar_texto(t2, sizeof t2, _("; 3D: %u de %u MB usados"), (ac3.memoria_kb - ac3.livre_kb) / 1024, ac3.memoria_kb / 1024);
        junta_texto(t, t2, sizeof t);
    }
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("2D (driver %s): %s"), ac2.nome, acel_texto_2d());
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    formatar_texto(t, sizeof t, _("3D (driver %s): %s"), ac3.nome, acel_texto_3d());
    if (ac3.pontos) { formatar_texto(t2, sizeof t2, _("; puntuación %u"), ac3.pontos); junta_texto(t, t2, sizeof t); }
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    {
        static const char *const ev[6] = { N_("no disponible"), N_("activo"), N_("apagado (kconfig)"), N_("apagado a mano"), N_("falló"),
                                           N_("más lento que la CPU") };
        formatar_texto(t, sizeof t, _("Volcado por la GPU: %s"), _(ev[ac2.volcado_estado < 6 ? ac2.volcado_estado : 0]));
        if (ac2.volcado_estado == 1) {
            formatar_texto(t2, sizeof t2, _("; %u volcados (%u kpíxeles)"), ac2.volcado_ops, ac2.volcado_kpx);
            junta_texto(t, t2, sizeof t);
        }
        ey = ui_parrafo(XL, ey, w, t, COR_TEXTO);
    }
    ey = acel_bloque_energia(ey, w);
    /* D346: el reparto GPU / software, para cualquier usuario (INFO_ACEL2D es de lectura) */
    formatar_texto(t, sizeof t, _("2D: GPU %u copias y %u rellenos; software %u mover, %u consola, %u rellenos, %u volcados"),
                   ac2.copias, ac2.rellenos, ac2.sw_ops[0], ac2.sw_ops[1], ac2.sw_ops[2], ac2.sw_ops[3]);
    ey = ui_parrafo(XL, ey, w, t, COR_TEXTO_FRACO);
    if (!ui_ev) ui_grupo(y, ey - y + 6, N_("Tarjeta gráfica"));
    y = ey + 16;
    if (acel_3d_hay_driver() && acel_3d_experimental()) {
        if (conf.a_3d == TRI_SI)
            y = ui_parrafo(XL, y, w, "Experimental: si la pantalla se queda congelada, reinicia; el sistema volverá al modo seguro.", 0xB03000);
        else
            y = ui_parrafo(XL, y, w, "El 3D de esta tarjeta es experimental (sin probar en hardware): por eso está apagado.", COR_TEXTO_FRACO);
    }
    if (acel_arq[0]) y = ui_parrafo(XL, y, w, acel_arq, 0xB03000);
    else y = ui_parrafo(XL, y, w, "Mayús al arrancar = modo seguro (2D por la CPU, 3D por software, sin esta configuración).", COR_TEXTO_FRACO);
    cont_h = y + 8;
}

/* ------------------------------------------------------------ Sistema > Kernel (D353, D367) */
/* Las opciones del kernel que se guardan en la cabecera de arranque y valen al reiniciar (lo mismo que la orden
 * «kconfig»). Las de sí/no son casillas; las de varios valores (tamaños, gobernador) llevan «Cambiar», que pasa al
 * siguiente de su ciclo. «De la imagen» quita lo guardado de esa opción. Cambiar pide administrador (D269). */
static info_kconf_t kc[KCONF_MAX];
static int kc_n;
static unsigned kc_est;
static int kc_ayuda;   /* la fila cuya explicación se ve arriba */

static void kc_preparar(void)
{
    kc_n = kconf_ler(kc, KCONF_MAX, &kc_est);
    if (kc_n < 0) kc_n = 0;
    if (kc_ayuda >= kc_n) kc_ayuda = 0;
}

static bool kc_permiso(void)
{
    if (sou_root()) return true;
    if (elevar_grafico(_("cambiar las opciones del kernel")) == 0) return true;
    dizer(_("Sin permisos de administrador no se pueden cambiar las opciones del kernel."));
    return false;
}

static void kc_resultado(int r, const info_kconf_t *o)
{
    if (r == ERRO_SO_LEITURA) { dizer(_("Arranque desde CD: no se puede guardar.")); return; }
    if (r < 0) { formatar_texto(estado, sizeof estado, _("No se pudo guardar: %s"), nome_erro(r)); return; }
    kc_preparar();
    if (!o) { dizer(_("Borrado lo guardado: al reiniciar, los valores de la imagen.")); return; }
    for (int i = 0; i < kc_n; i++)
        if (kc[i].hueco == o->hueco) {
            char t[24];
            kconf_texto_valor(&kc[i], kc[i].proximo, t, sizeof t);
            formatar_texto(estado, sizeof estado, kconf_pide_reinicio(&kc[i]) ? _("%s: %s; se aplica al reiniciar") : _("%s: %s"),
                           kconf_rotulo(kc[i].nome), t);
        }
}

/* D367: un cambio pasa por las reglas de dependencia (conf_reglas.c): se rechaza si dejaría una combinación sin sentido y
 * arrastra lo que dependa (nx si se apaga pae...), con aviso. Lo mismo que hace «kconfig». */
static void kc_cambiar(const info_kconf_t *o0, bool quitar)
{
    info_kconf_t o = *o0;
    kconf_cambio_t p = { o.hueco, quitar ? 0 : kconf_siguiente(&o, o.proximo), quitar, NULL }, fin[KCONF_MAX];
    char err[120];
    int n = kconf_resolver(kc, kc_n, &p, 1, fin, KCONF_MAX, err, sizeof err);
    if (n < 0) { formatar_texto(estado, sizeof estado, _("No se puede: %s"), err); return; }
    if (!kc_permiso()) return;
    int r = 0;
    for (int i = 0; i < n && r >= 0; i++) r = fin[i].quitar ? kconf_quitar(fin[i].hueco) : kconf_fijar(fin[i].hueco, fin[i].valor);
    registrar("configuracion: kernel %s %s -> %d", o.nome, quitar ? "imagen" : "cambiar", r);
    kc_resultado(r, &o);
    for (int i = 1; i < n && r >= 0; i++)
        for (int k = 0; k < kc_n; k++)
            if (kc[k].hueco == fin[i].hueco) {
                char t[160];
                formatar_texto(t, sizeof t, "%s; %s -> %s (%s)", estado, kconf_rotulo(kc[k].nome), fin[i].valor ? _("sí") : _("no"), fin[i].aviso);
                copia_texto(estado, t, sizeof estado);
                registrar("configuracion: kernel ajuste %s -> %u (%s)", kc[k].nome, fin[i].valor, fin[i].aviso);
            }
    elevacao_soltar();
}

static void kc_predet(void)
{
    if (!kc_permiso()) return;
    int r = kconf_quitar_todo();
    registrar("configuracion: kernel valores de la imagen -> %d", r);
    kc_resultado(r, NULL);
    elevacao_soltar();
}

/* El grupo de una opción: 0 memoria, 1 caché y discos, 2 procesador, 3 otras. */
static int kc_grupo(const char *nome)
{
    static const char *const mem[] = { "pae", "nx", "swap", "swap_mb", NULL };
    static const char *const cac[] = { "cache_disco", "cache_alta", "escritura_diferida", "dma", "cache_max_mb", "sucios_max_mb", "vaciado_s", NULL };
    static const char *const cpu[] = { "frecuencia", "gobernador", "avx", NULL };   /* avx: D381 */
    for (int i = 0; mem[i]; i++) if (compara_texto(nome, mem[i]) == 0) return 0;
    for (int i = 0; cac[i]; i++) if (compara_texto(nome, cac[i]) == 0) return 1;
    for (int i = 0; cpu[i]; i++) if (compara_texto(nome, cpu[i]) == 0) return 2;
    return 3;
}

static void pg_kernel(void)
{
    static const char *const tit[4] = { N_("Memoria"), N_("Caché y discos"), N_("Procesador"), N_("Otras opciones") };
    char t[200], b[24];
    int w = XR - XL, y = 10;
    int xv = XL + 256, xc = XL + 338, xi = XL + 418;
    if (kc_est & KCONF_EST_RESCATE) copia_texto(t, _("Modo rescate: lo guardado no se aplica en este arranque."), sizeof t);
    else if (kc_est & KCONF_EST_CD) copia_texto(t, _("Arranque desde CD: no se puede guardar nada."), sizeof t);
    else if (kc_est & KCONF_EST_DANADA) copia_texto(t, _("Lo guardado está dañado y no se aplica."), sizeof t);
    else t[0] = 0;
    /* el aviso y la ayuda de la opción ocupan siempre dos líneas cada uno: así las filas no se mueven con el ancho de la ventana */
    ui_parrafo(XL - 6, y, w, _("Se aplican al reiniciar. Si no arranca: Ctrl al arrancar y «kconfig --por-defecto»."), 0xB03000);
    y += 2 * (LETRA_A + 2);
    if (t[0]) { ui_parrafo(XL - 6, y, w, t, 0xB03000); y += LETRA_A + 2; }
    if (!kc_n) { ui_texto(XL - 6, y + 4, _("Este kernel no tiene opciones guardables."), COR_TEXTO); cont_h = y + 30; return; }
    y += 2;
    if (kc_ayuda < kc_n) ui_parrafo(XL - 6, y, w + 6, kconf_ayuda(kc[kc_ayuda].nome), COR_TEXTO_FRACO);
    y += 2 * (LETRA_A + 2) + 8;
    for (int g = 0; g < 4; g++) {
        int gy = y, cy = y + 26, n = 0;
        for (int i = 0; i < kc_n; i++) {
            info_kconf_t *o = &kc[i];
            if (kc_grupo(o->nome) != g) continue;
            n++;
            if (ui_clic(XL, cy - 3, xc - XL - 4, 22) && kc_ayuda != i) { kc_ayuda = i; ui_mudou = true; }
            bool reinicio = kconf_pide_reinicio(o);
            const char *bloq = kconf_bloqueo(kc, kc_n, o->nome);   /* D367: deshabilitada por otra opción (se enseña el motivo) */
            ui_gris = bloq != NULL && !(o->tipo == KCONF_SINO && o->proximo != 0);   /* apagar siempre se puede */
            if (o->tipo == KCONF_SINO) {
                if (ui_casilla(XL, cy, kconf_rotulo(o->nome), o->proximo != 0)) {
                    ui_mudou = true;
                    kc_ayuda = i;
                    kc_cambiar(o, false);
                }
                if (reinicio) ui_texto(XL + 32 + g_largura_texto(kconf_rotulo(o->nome)), cy, _("(al reiniciar)"), 0xB03000);
            } else {
                ui_texto(XL, cy, kconf_rotulo(o->nome), COR_TEXTO);
                kconf_texto_valor(o, o->proximo, b, sizeof b);
                formatar_texto(t, sizeof t, "%s%s", b, reinicio ? " *" : "");
                ui_texto(xv, cy, t, reinicio ? 0xB03000 : o->guardado ? COR_TEXTO : COR_TEXTO_FRACO);
                if (!bloq && ui_boton(xc, cy - 5, 74, N_("Cambiar"), false)) {
                    ui_mudou = true;
                    kc_ayuda = i;
                    kc_cambiar(o, false);
                }
            }
            if (o->guardado && ui_boton(xi, cy - 5, 108, N_("De la imagen"), false)) {
                ui_mudou = true;
                kc_cambiar(o, true);
            }
            ui_gris = false;
            cy += 30;
            if (bloq) { ui_texto(XL + 22, cy - 12, bloq, COR_TEXTO_FRACO); cy += 16; }
            if (ui_ev && ui_mudou) return;   /* las filas cambiaron: se repinta y se vuelve a calcular */
        }
        if (!n) continue;
        if (!ui_ev) ui_grupo(gy, cy - gy, tit[g]);
        y = cy + 6;
    }
    ui_texto(XL, y, _("* = cambia al reiniciar"), COR_TEXTO_FRACO);
    cont_h = y + 28;
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
               190 / LETRA_L, COR_TEXTO_FRACO);
    cont_h = 398;

    g_texto(jn, 230, Y_PAG + 4, _("Dirección del servidor (URL)"), COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[0]);
    formatar_texto(t, sizeof t, _("Clave de API (%s)"), chave_ia[0] ? _("definida") : _("sin clave"));
    g_texto(jn, 230, Y_PAG + 48, t, COR_TEXTO_FRACO);
    caixa_desenhar(jn, &c_ia[1]);

    for (int i = 0; i < 4; i++) {
        int x = i % 2 == 0 ? 230 : 416;
        int y = Y_PAG + 100 + (i / 2) * 48;
        g_texto(jn, x, y, _(rotulos_uso[i]), COR_TEXTO_FRACO);
        caixa_desenhar(jn, &c_ia[2 + i]);
        /* La casilla de destino se marca con un marco azul: así se ve de dónde
         * a dónde va el modelo que se elige en la lista. */
        if (2 + i == cx_alvo)
            g_moldura(jn, c_ia[2 + i].x - 2, c_ia[2 + i].y - 2, c_ia[2 + i].l + 4, LETRA_A + 12, COR_SELECAO);
    }
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_ia[i]);
    g_texto(jn, 230, Y_PAG + 246, _("Modelo para:"), COR_TEXTO_FRACO);
    for (int i = 0; i < 4; i++) desenhar_alvo(&b_alvo[i], 2 + i == cx_alvo);
    lista_desenhar(jn, &lista_mod);
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
/* ------------------------------------------------------------ las secciones nuevas (D367) */

/* Dispositivos > Teclado */
static void pg_teclado(void)
{
    static const char *const nombres[2] = { N_("Español (ES)"), N_("Portugués de Brasil (ABNT2)") };
    int cur = teclado_layout(-1), y = 26;
    for (int i = 0; i < 2; i++)
        if (ui_opcion(XL, y + i * 26, nombres[i], cur == i) && cur != i) {
            aplicar_teclado(i);
            conf.teclado = i;
            dizer_gravado(conf_gravar());
            ui_mudou = true;
        }
    int fin = ui_parrafo(XL, y + 62, XR - XL, _("Prueba: la ñ, la ç y los acentos cambian según la distribución."), COR_TEXTO_FRACO);
    if (!ui_ev) ui_grupo(0, fin + 4, N_("Distribución del teclado"));
    cont_h = fin + 12;
}

/* Sistema > Idioma (D313: antes iba dentro de Teclado) */
static void traducir_rotulos(void);
static void pg_idioma(void)
{
    static const char *const nombres[3] = { "Español", "Português (Brasil)", "English" };
    int y = 26;
    for (int i = 0; i < 3; i++)
        if (ui_opcion(XL + (i == 0 ? 0 : i == 1 ? 130 : 330), y, nombres[i], compara_texto(idioma_actual(), codigos_idi[i]) == 0)
            && compara_texto(idioma_actual(), codigos_idi[i]) != 0) {
            copia_texto(conf.idioma, codigos_idi[i], sizeof conf.idioma);
            idioma_fijar(codigos_idi[i]);
            traducir_rotulos();
            ler_modos_video();   /* los textos de las listas llevan palabras traducibles */
            dizer_gravado(conf_gravar());
            ui_mudou = true;
            return;
        }
    int fin = ui_parrafo(XL, y + 32, XR - XL, _("Se aplica a los programas que abras después y al escritorio."), COR_TEXTO_FRACO);
    if (!ui_ev) ui_grupo(0, fin + 4, N_("Idioma de la interfaz"));
    cont_h = fin + 12;
}

/* Escritorio > Capturas (D125) */
static void cap_aplicar(void)
{
    copia_texto(conf.capturas, c_capturas.texto, sizeof conf.capturas);
    cap_pend = false;
    dizer_gravado(conf_gravar());
}

static void cap_sair(void) { if (cap_pend) cap_aplicar(); }

static void cap_predet(void)
{
    conf.capturas[0] = 0;
    conf.cap_sonido = 1;
    caixa_definir(&c_capturas, "");
    cap_pend = false;
    dizer_gravado(conf_gravar());
}

static void pg_capturas(void)
{
    int y = 26;
    int w = XR - XL;
    c_capturas.x = XL; c_capturas.y = y + 38; c_capturas.l = w; c_capturas.foco = true;
    y = ui_parrafo(XL, y, w, _("Carpeta de las capturas de pantalla (Impr Pant). Vacía = /home/capturas (o /tmp/capturas si /home no deja escribir)."), COR_TEXTO);
    c_capturas.y = y + 6;
    if (!ui_ev) caixa_desenhar(jn, &c_capturas);
    else {
        char antes[NOME_MAX_F];
        copia_texto(antes, c_capturas.texto, sizeof antes);
        bool enter = caixa_tratar(jn, &c_capturas, ui_ev);
        if (compara_texto(antes, c_capturas.texto) != 0) { cap_pend = true; ui_mudou = true; }
        if (enter) { cap_aplicar(); ui_mudou = true; }
    }
    y = c_capturas.y + 34;
    if (cap_pend) y = ui_parrafo(XL, y, w, _("Pulsa Intro o Aplicar para usar esta carpeta."), 0xB03000);
    if (!ui_ev) ui_grupo(0, y + 2, N_("Carpeta"));
    int g2 = y + 14;
    int c = g2 + 26;
    if (ui_casilla(XL, c, N_("Sonido al capturar"), conf.cap_sonido != 0)) {
        conf.cap_sonido = !conf.cap_sonido;
        dizer_gravado(conf_gravar());
        ui_mudou = true;
    }
    if (!ui_ev) ui_grupo(g2, 52, N_("Opciones"));
    cont_h = g2 + 64;
}

/* Escritorio > Colores y fondo */
static bool fondo_pedido(void)
{
    if (lista_img.sel < 0) { dizer(_("Elige una imagen de la lista.")); return true; }
    if (conf.modo == M_DEGRADE) conf.modo = M_AJUSTADO;
    dizer(_("Aplicando el fondo..."));
    mostrando_barra = true;
    return false;
}

static void desenhar(void);
static void colores_predet(void)
{
    conf.acento = cores[0].cor;
    acento_cor((int)conf.acento);
    fundo_aplicar("", M_DEGRADE);
    conf.fundo[0] = 0;
    conf.modo = M_DEGRADE;
    lista_img.sel = -1;
    dizer_gravado(conf_gravar());
}

static void pg_colores(void)
{
    info_aparencia_t ap;
    memset(&ap, 0, sizeof ap);
    info_sistema(INFO_APARENCIA, &ap, sizeof ap);
    int y = 26;
    y = ui_parrafo(XL, y, XR - XL, _("Color de la barra de título y de la barra de tareas."), COR_TEXTO_FRACO);
    int sy = y + 4, paso = 32, cols = (XR - XL + 6) / paso, sel = -1;
    if (cols < 1) cols = 1;
    for (int i = 0; i < CORES_QTD; i++) {
        int x = XL + (i % cols) * paso, yy = sy + (i / cols) * paso;
        if (cores[i].cor == ap.acento) sel = i;
        if (ui_ev) {
            if (ui_clic(x, yy, 26, 26)) {
                acento_cor((int)cores[i].cor);
                conf.acento = cores[i].cor;
                dizer_gravado(conf_gravar());
                ui_mudou = true;
            }
            continue;
        }
        g_retangulo(jn, x, yy, 26, 26, cores[i].cor);
        g_moldura(jn, x, yy, 26, 26, i == sel ? 0x000000 : COR_BORDA);
        if (i == sel) { g_moldura(jn, x + 1, yy + 1, 24, 24, 0xFFFFFF); g_moldura(jn, x + 2, yy + 2, 22, 22, 0x000000); }
    }
    y = sy + ((CORES_QTD + cols - 1) / cols) * paso + 2;
    if (!ui_ev) {
        char nt[80];
        formatar_texto(nt, sizeof nt, _("Elegido: %s"), sel >= 0 ? _(cores[sel].nome) : _("otro"));
        g_texto(jn, XL, y, nt, COR_TEXTO);
    }
    y += 22;
    if (!ui_ev) ui_grupo(0, y, N_("Color de acento"));
    int g2 = y + 10, c = g2 + 26;
    for (int i = 0; i < M_QTD; i++) {
        if (ui_opcion(XL + (i % 3) * 150, c + (i / 3) * 24, rotulos_modo[i], conf.modo == i)) {
            conf.modo = i;
            if (i == M_DEGRADE) {
                fundo_aplicar("", M_DEGRADE);
                conf.fundo[0] = 0;
                lista_img.sel = -1;
                dizer_gravado(conf_gravar());
            }
            ui_mudou = true;
        }
    }
    int ly = c + 56;
    if (!ui_ev) g_texto(jn, XL, ly, _("Imagen (BMP o PNG de /imagenes y de /home):"), COR_TEXTO_FRACO);
    int la = ALT - 100 - ly - 60;   /* la lista ocupa lo que sobra, entre 90 y 150 */
    if (la < 90) la = 90;
    if (la > 150) la = 150;
    lista_img.x = XL; lista_img.y = ly + 20; lista_img.l = XR - XL; lista_img.a = la;
    int by = lista_img.y + la + 8;
    if (!ui_ev) lista_desenhar(jn, &lista_img);
    else {
        int at = lista_tratar(jn, &lista_img, ui_ev);
        if (at) ui_mudou = true;
        if (at == LISTA_ATIVOU) goto aplicar;
    }
    if (ui_boton(XL, by, 140, N_("Aplicar fondo"), false)) {
aplicar:
        ui_mudou = true;
        if (!fondo_pedido()) {
            desenhar();
            int r = fundo_aplicar(itens_img[lista_img.sel], conf.modo);
            mostrando_barra = false;
            if (r < 0) formatar_texto(estado, sizeof estado, _("No se pudo: %s"), texto_erro(r));
            else {
                copia_texto(conf.fundo, itens_img[lista_img.sel], sizeof conf.fundo);
                dizer_gravado(conf_gravar());
            }
        }
    }
    y = by + 36;
    if (!ui_ev) ui_grupo(g2, y - g2, N_("Fondo del escritorio"));
    cont_h = y + 8;
}

/* Red > Bloqueo de publicidad (D218): llama al programa «bloqueo», que compila la tabla y escribe bloqueo.conf. */
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

static void pg_bloqueo(void)
{
    char t[160];
    int w = XR - XL;
    config_rede_t c;
    memset(&c, 0, sizeof c);
    char a[16], d[16];
    if (config_rede(REDE_CONSULTAR, &c) == 0 && c.ip)
        formatar_texto(t, sizeof t, _("Red: IP %s, DNS %s"), ip_para_texto(c.ip, a), c.dns ? ip_para_texto(c.dns, d) : _("ninguno"));
    else copia_texto(t, _("Red: sin dirección IP"), sizeof t);
    int y = ui_parrafo(XL, 26, w, t, COR_TEXTO);
    if (!ui_ev) ui_grupo(0, y + 2, N_("Conexión"));
    int g2 = y + 14;
    y = g2 + 26;
    info_bloqueo_t b;
    memset(&b, 0, sizeof b);
    bloqueo_dns(BLOQ_ESTADO, &b, sizeof b);
    if (ui_casilla(XL, y, N_("Bloquear publicidad y rastreo por DNS"), b.activo != 0)) {
        dizer(_("Un momento..."));
        desenhar();
        int r = rodar_bloqueo(b.activo ? "desactivar" : "activar");
        if (r < 0) dizer(_("No se pudo cambiar el bloqueo (mira «bloqueo estado» en el terminal)."));
        else dizer(b.activo ? _("Bloqueo desactivado.") : _("Bloqueo activado."));
        ui_mudou = true;
        return;
    }
    y += 26;
    if (b.activo) formatar_texto(t, sizeof t, _("Activado: %u dominios bloqueados, %u permitidos (%u KB)."),
                                 b.bloqueados, b.permitidos, (b.bytes + 1023) / 1024);
    else copia_texto(t, b.pedido ? _("Desactivado: no hay tabla (pulsa Actualizar listas).") : _("Desactivado: se resuelven todos los nombres."), sizeof t);
    y = ui_parrafo(XL, y, w, t, COR_TEXTO);
    if (ui_boton(XL, y + 6, 180, N_("Actualizar listas"), false)) {
        dizer(_("Descargando las listas..."));
        desenhar();
        int r = rodar_bloqueo("actualizar");
        dizer(r == 0 ? _("Listas actualizadas.") : _("No se pudieron actualizar las listas (¿hay red?)."));
        ui_mudou = true;
        return;
    }
    y += 42;
    formatar_texto(t, sizeof t, _("Consultas: %u   Bloqueadas: %u"), b.consultas, b.bloqueadas);
    y = ui_parrafo(XL, y, w, t, COR_TEXTO);
    static char u[6][64];
    int n = bloqueo_dns(BLOQ_ULTIMOS, u, 6);
    if (n > 0) {
        y = ui_parrafo(XL, y + 4, w, _("Últimos bloqueados:"), COR_TEXTO_FRACO);
        for (int i = 0; i < n; i++) { ui_texto(XL + 16, y, u[i], COR_TEXTO); y += 18; }
    }
    if (!ui_ev) ui_grupo(g2, y - g2 + 4, N_("Bloqueo de publicidad y rastreo por DNS"));
    y += 16;
    y = ui_parrafo(XL - 6, y, w + 6, _("Los dominios de las listas responden 0.0.0.0 a todos los programas."), COR_TEXTO_FRACO);
    y = ui_parrafo(XL - 6, y, w + 6, _("Permitir o bloquear a mano, y el servidor para la red: bloqueo --ayuda, dnsbloqueo."), COR_TEXTO_FRACO);
    cont_h = y + 8;
}

/* ------------------------------------------------------------ Dispositivos > Sonido (D201, D292, D367) */

typedef struct {
    info_audio_t ia;
    bool ok, sal;
    int g1, g2, g3, g4;          /* y de cada grupo */
    int vol_x, vol_y, vol_l;     /* la barra del volumen */
    int bal_x, bal_y;            /* la del balance */
    int pr_y, lat_x, sal_y;      /* pruebas y latencia; salida */
    int mic_y, mic_gx, mic_my;   /* entrada, ganancia y medidor del micrófono */
    int eq_x0, eq_y0;            /* ecualizador: primera columna y arriba del carril */
    int total;
} snd_t;
static snd_t sn;

static void snd_calc(void)
{
    memset(&sn, 0, sizeof sn);
    sn.ok = info_sistema(INFO_AUDIO, &sn.ia, sizeof sn.ia) >= 0 && sn.ia.estado == AUDIO_FUNCIONANDO;
    if (!sn.ok) { sn.total = 40; return; }
    int y = 0;
    sn.g1 = y;
    sn.vol_x = XL + 34; sn.vol_y = y + 26 + 22; sn.vol_l = XR - XL - 72;
    y += 124 + 8;
    sn.g2 = y;
    sn.bal_x = XL + 70; sn.bal_y = y + 26;
    sn.pr_y = y + 26 + 38;
    sn.lat_x = XL + 250;
    int n_sal = 0;
    for (int k = 0; k < AUDIO_SALIDAS; k++) n_sal += sn.ia.salidas[k].usada;
    sn.sal = n_sal > 1;
    sn.sal_y = y + 26 + 76;
    y += 26 + 38 + 38 + (sn.sal ? 34 : 0) + 8 + 8;
    sn.g3 = y;
    if (!sn.ia.captura) y += 26 + 24 + 8 + 8;
    else {
        sn.mic_y = y + 26; sn.mic_gx = XL + 30; sn.mic_my = y + 26 + 66;
        y += 26 + 66 + 22 + 22 + 24 + 8 + 8;
    }
    sn.g4 = y;
    sn.eq_x0 = XL + 40; sn.eq_y0 = y + 26 + 20;
    y += 26 + 20 + EQ_ALT + 8 + 18 + 8 + 8;
    sn.total = y;
    /* los botones fijos */
    b_vol[0].x = XL; b_vol[0].y = sn.vol_y;
    b_vol[1].x = sn.vol_x + sn.vol_l + 6; b_vol[1].y = sn.vol_y;
    b_bal.x = sn.bal_x + BAL_L + 10; b_bal.y = sn.bal_y - 2;
    b_prob[0].x = XL; b_prob[0].y = sn.pr_y;
    b_prob[1].x = XL + 118; b_prob[1].y = sn.pr_y;
    b_lat[0].x = sn.lat_x; b_lat[0].y = sn.pr_y;
    b_lat[1].x = sn.lat_x + 160; b_lat[1].y = sn.pr_y;
    b_sal.x = XL; b_sal.y = sn.sal_y; b_sal.l = XR - XL;
    for (int i = 0; i < 4; i++) { b_eq[i].x = sn.eq_x0 + 4 * EQ_PASSO + 46; b_eq[i].y = sn.eq_y0 - 4 + i * 30; }
    b_mic[0].x = XL; b_mic[0].y = sn.mic_y;
    b_mic[1].x = XL + 196; b_mic[1].y = sn.mic_y;
    b_mic[2].x = XL; b_mic[2].y = sn.mic_y + 30;
    b_mic[3].x = sn.mic_gx + MIC_GL + 6; b_mic[3].y = sn.mic_y + 30;
}

static void definir_volume_pelo_mouse(int x)
{
    int v = (x - sn.vol_x) * 100 / sn.vol_l;
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    conf.volume = v;
    conf.silencio = 0;
    aplicar_volume(v, 0);
}

static bool dentro(const evento_t *e, int x, int y, int l, int a);
static void balance_poner(int b);
static void mic_guardar(void);
static void mic_ganancia_poner(int g, bool mudo);
static void eq_mudar(int banda, int db);

static void eq_pelo_mouse(int banda, int y)
{
    int db = 12 - ((y - sn.eq_y0) * 24 + EQ_ALT / 2) / EQ_ALT;
    eq_mudar(banda, db);
}

static int eq_coluna(const evento_t *e)
{
    for (int i = 0; i < EQ_BANDAS; i++)
        if (dentro(e, sn.eq_x0 + i * EQ_PASSO - 20, sn.eq_y0 - 6, 40, EQ_ALT + 12)) return i;
    return -1;
}

static void snd_predet(void)
{
    eq_preajuste("plano", conf.eq);
    conf.eq_def = 1;
    eq_aplicar(conf.eq);
    balance_poner(100);
    dizer_gravado(conf_gravar());
}

static void snd_desenhar(void)
{
    char t[160];
    snd_calc();
    cont_h = sn.total;
    if (!sn.ok) {
        titulo_pag(24, Y_PAG + 14, _("No hay tarjeta de sonido (solo el altavoz del PC)."));
        return;
    }
    const info_audio_t *ia = &sn.ia;
    ui_inicio(NULL);
    /* Volumen */
    formatar_texto(t, sizeof t, _("Volumen general: %u %%%s"), ia->volume, ia->silencio ? _(" (silencio)") : "");
    g_texto(jn, XL, sn.g1 + 26, t, COR_TEXTO);
    ui_casilla(XR - 110, sn.g1 + 26, N_("Silenciar"), ia->silencio != 0);
    g_retangulo(jn, sn.vol_x, sn.vol_y, sn.vol_l, BAL_A, COR_BOTAO);
    g_moldura(jn, sn.vol_x, sn.vol_y, sn.vol_l, BAL_A, COR_BORDA);
    g_retangulo(jn, sn.vol_x + 1, sn.vol_y + 1, (sn.vol_l - 2) * ia->volume / 100, BAL_A - 2, ia->silencio ? COR_BORDA : COR_DESTAQUE);
    botao_desenhar(jn, &b_vol[0]);
    botao_desenhar(jn, &b_vol[1]);
    formatar_texto(t, sizeof t, _("Tarjeta: %s (%s)"), ia->placa, ia->driver);
    g_texto_n(jn, XL, sn.g1 + 26 + 56, t, (XR - XL) / LETRA_L, COR_TEXTO_FRACO);
    g_texto_n(jn, XL, sn.g1 + 26 + 74, ia->codec, (XR - XL) / LETRA_L, COR_TEXTO_FRACO);
    ui_grupo(sn.g1, 124, N_("Volumen"));
    /* Balance, pruebas, latencia y salida */
    g_texto(jn, XL, sn.bal_y + 4, _("Balance"), COR_TEXTO);
    g_retangulo(jn, sn.bal_x, sn.bal_y, BAL_L, BAL_A, COR_BOTAO);
    g_moldura(jn, sn.bal_x, sn.bal_y, BAL_L, BAL_A, COR_BORDA);
    {
        int cx = sn.bal_x + BAL_L / 2, px = sn.bal_x + (BAL_L - 2) * ia->balance / 200;
        g_retangulo(jn, cx, sn.bal_y + 2, 1, BAL_A - 4, COR_BORDA);
        g_retangulo(jn, px - 3, sn.bal_y + 2, 7, BAL_A - 4, COR_DESTAQUE);
        int b = (int)ia->balance - 100;
        g_texto(jn, b_bal.x + b_bal.l + 8, sn.bal_y + 4, b == 0 ? _("centro") : b < 0 ? _("izquierda") : _("derecha"), COR_TEXTO_FRACO);
    }
    botao_desenhar(jn, &b_bal);
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_prob[i]);
    botao_desenhar(jn, &b_lat[0]);
    botao_desenhar(jn, &b_lat[1]);
    formatar_texto(t, sizeof t, _("Latencia: %u ms"), ia->latencia_base_ms);
    g_texto(jn, sn.lat_x + 30, sn.pr_y + 5, t, COR_TEXTO);
    if (sn.sal) {
        static char txt_sal[80];
        int act = 0;
        for (int k = 0; k < AUDIO_SALIDAS; k++) if (ia->salidas[k].actual) act = k;
        formatar_texto(txt_sal, sizeof txt_sal, _("Salida: %s (pulsa para cambiar)"), ia->salidas[act].nombre);
        b_sal.texto = txt_sal;
        botao_desenhar(jn, &b_sal);
    }
    ui_grupo(sn.g2, sn.g3 - sn.g2 - 8, N_("Balance y salida"));
    /* Micrófono */
    if (!ia->captura) {
        g_texto_n(jn, XL, sn.g3 + 26, _("Esta tarjeta no tiene entrada de sonido que podamos usar."), (XR - XL) / LETRA_L, COR_TEXTO_FRACO);
        ui_grupo(sn.g3, 60, N_("Micrófono"));
    } else {
        botao_desenhar(jn, &b_mic[0]);
        botao_desenhar(jn, &b_mic[1]);
        g_texto_n(jn, XL + 30, sn.mic_y + 5, mic_inf.n_entradas ? mic_inf.entradas[mic_inf.entrada < CAP_ENTRADAS_MAX ? mic_inf.entrada : 0] : "-", 20, COR_TEXTO);
        botao_desenhar(jn, &b_mic[2]);
        botao_desenhar(jn, &b_mic[3]);
        g_retangulo(jn, sn.mic_gx, sn.mic_y + 30, MIC_GL, 24, COR_BOTAO);
        g_moldura(jn, sn.mic_gx, sn.mic_y + 30, MIC_GL, 24, COR_BORDA);
        g_retangulo(jn, sn.mic_gx + 1, sn.mic_y + 31, (MIC_GL - 2) * (int)mic_inf.ganancia / 100, 22, mic_inf.mudo ? COR_BORDA : COR_DESTAQUE);
        formatar_texto(t, sizeof t, "%u %%", mic_inf.ganancia);
        g_texto(jn, sn.mic_gx + 6, sn.mic_y + 34, t, COR_BRANCO);
        {   /* medidor de nivel en vivo */
            int mx = XL, my = sn.mic_my, ml = 250, ma = 22;
            unsigned pico = mic_inf.pico > 32767 ? 32767 : mic_inf.pico;
            int lp = (int)((unsigned long long)pico * (unsigned)(ml - 4) / 32767);
            g_retangulo(jn, mx, my, ml, ma, 0x20262C);
            if (lp > 0) g_retangulo(jn, mx + 2, my + 2, lp, ma - 4, pico > 29000 ? 0xD83A2A : pico > 20000 ? 0xE0B020 : 0x2EB872);
            g_moldura(jn, mx, my, ml, ma, COR_BORDA);
            g_texto_n(jn, mx + ml + 10, my + 3, mic_aberto ? _("Habla para probar el nivel.") : _("Micrófono en uso por otro programa."), (XR - mx - ml - 10) / LETRA_L, COR_TEXTO_FRACO);
        }
        ui_casilla(XL, sn.mic_my + 32, N_("Silenciar micrófono"), mic_inf.mudo != 0);
        ui_grupo(sn.g3, sn.g4 - sn.g3 - 8, N_("Micrófono"));
    }
    /* Ecualizador */
    for (int i = 0; i < EQ_BANDAS; i++) {
        int cx = sn.eq_x0 + i * EQ_PASSO, v = ia->eq_db[i];
        int ym = sn.eq_y0 + EQ_ALT / 2, yv = sn.eq_y0 + (12 - v) * EQ_ALT / 24;
        g_retangulo(jn, cx - 2, sn.eq_y0, 4, EQ_ALT, COR_BOTAO);
        g_moldura(jn, cx - 2, sn.eq_y0, 4, EQ_ALT, COR_BORDA);
        g_retangulo(jn, cx - 14, ym, 28, 1, COR_BORDA);                 /* la marca de 0 dB */
        g_retangulo(jn, cx - 2, v >= 0 ? yv : ym, 4, v >= 0 ? ym - yv : yv - ym, COR_DESTAQUE);
        g_retangulo(jn, cx - 10, yv - 4, 20, 9, COR_DESTAQUE);
        g_moldura(jn, cx - 10, yv - 4, 20, 9, COR_BORDA);
        eq_db_texto(t, v);
        g_texto(jn, cx - (int)tam_texto(t) * LETRA_L / 2, sn.eq_y0 - 18, t, COR_TEXTO);
        g_texto(jn, cx - (int)tam_texto(eq_rotulos[i]) * LETRA_L / 2, sn.eq_y0 + EQ_ALT + 6, eq_rotulos[i], COR_TEXTO_FRACO);
    }
    for (int i = 0; i < 4; i++) botao_desenhar(jn, &b_eq[i]);
    ui_grupo(sn.g4, sn.total - sn.g4 - 8, N_("Ecualizador (dB)"));
}

static bool snd_tratar(const evento_t *ep)
{
    const evento_t e = *ep;
    bool mudou = false;
    snd_calc();
    if (!sn.ok) return false;
    ui_inicio(&e);
    if (ui_casilla(XR - 110, sn.g1 + 26, N_("Silenciar"), sn.ia.silencio != 0)) {
        int s = audio_silencio(-1);
        if (s >= 0) {
            conf.silencio = !s;
            if (conf.volume < 0) conf.volume = audio_volume_mestre(-1);
            aplicar_volume(conf.volume, conf.silencio);
            dizer_gravado(conf_gravar());
        }
        mudou = true;
    }
    if (botao_tratar(jn, &b_vol[0], &e) || botao_tratar(jn, &b_vol[1], &e)) {
        int v = audio_volume_mestre(-1);
        if (v >= 0) {
            v += (e.x < sn.vol_x ? -5 : 5);
            v = v < 0 ? 0 : v > 100 ? 100 : v;
            conf.volume = v; conf.silencio = 0;
            aplicar_volume(v, 0);
            dizer_gravado(conf_gravar());
        }
        mudou = true;
    }
    if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, sn.vol_x, sn.vol_y, sn.vol_l, BAL_A)) {
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
    if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, sn.bal_x, sn.bal_y, BAL_L, BAL_A)) {
        arrastando_bal = true;
        balance_poner((e.x - sn.bal_x) * 200 / BAL_L);
        mudou = true;
    } else if (e.tipo == EV_MOVER && arrastando_bal && (e.botoes & 1)) {
        balance_poner((e.x - sn.bal_x) * 200 / BAL_L);
        mudou = true;
    } else if (e.tipo == EV_SOLTOU && arrastando_bal) {
        arrastando_bal = false;
        dizer_gravado(conf_gravar());
        mudou = true;
    }
    if (sn.sal && botao_tratar(jn, &b_sal, &e)) {
        info_audio_t is;
        if (info_sistema(INFO_AUDIO, &is, sizeof is) == 0) {
            int act = 0, sig;
            for (int k = 0; k < AUDIO_SALIDAS; k++) if (is.salidas[k].actual) act = k;
            for (sig = (act + 1) % AUDIO_SALIDAS; sig != act && !is.salidas[sig].usada; sig = (sig + 1) % AUDIO_SALIDAS) { }
            if (sig != act && audio_salida(sig) >= 0) {
                copia_texto(conf.salida, is.salidas[sig].nombre, sizeof conf.salida);
                dizer_gravado(conf_gravar());
            }
        }
        mudou = true;
    }
    if (botao_tratar(jn, &b_bal, &e)) { balance_poner(100); dizer_gravado(conf_gravar()); mudou = true; }
    for (int i = 0; i < 2; i++)
        if (botao_tratar(jn, &b_prob[i], &e)) { probar_lado(i == 0); mudou = true; }
    for (int i = 0; i < 2; i++)
        if (botao_tratar(jn, &b_lat[i], &e)) {
            int ms = (int)sn.ia.latencia_base_ms + (i ? 20 : -20);
            ms = ms < 20 ? 20 : ms > 300 ? 300 : ms;
            conf.latencia = ms;
            conf.son_def |= 2;
            aplicar_latencia(ms);
            dizer_gravado(conf_gravar());
            mudou = true;
        }
    if (mic_inf.presente && sn.ia.captura) {
        if (botao_tratar(jn, &b_mic[0], &e) || botao_tratar(jn, &b_mic[1], &e)) {
            unsigned n = mic_inf.n_entradas;
            if (n > 1) {
                unsigned nueva = e.x < XL + 100 ? (mic_inf.entrada + n - 1) % n : (mic_inf.entrada + 1) % n;
                captura_entrada(nueva);
                captura_estado(CAP_FUENTE_REAL, &mic_inf);
                mic_guardar();
            }
            mudou = true;
        }
        if (botao_tratar(jn, &b_mic[2], &e) || botao_tratar(jn, &b_mic[3], &e)) {
            mic_ganancia_poner((int)mic_inf.ganancia + (e.x < sn.mic_gx ? -5 : 5), mic_inf.mudo != 0);
            mic_guardar();
            mudou = true;
        }
        if (ui_casilla(XL, sn.mic_my + 32, N_("Silenciar micrófono"), mic_inf.mudo != 0)) {
            mic_ganancia_poner((int)mic_inf.ganancia, !mic_inf.mudo);
            mic_guardar();
            mudou = true;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && dentro(&e, sn.mic_gx, sn.mic_y + 30, MIC_GL, 24)) {
            arrastando_mic = true;
            mic_ganancia_poner((e.x - sn.mic_gx) * 100 / MIC_GL, mic_inf.mudo != 0);
            mudou = true;
        } else if (e.tipo == EV_MOVER && arrastando_mic && (e.botoes & 1)) {
            mic_ganancia_poner((e.x - sn.mic_gx) * 100 / MIC_GL, mic_inf.mudo != 0);
            mudou = true;
        } else if (e.tipo == EV_SOLTOU && arrastando_mic) {
            arrastando_mic = false;
            mic_guardar();
            mudou = true;
        }
    }
    return mudou;
}

/* ------------------------------------------------------------ secciones heredadas, con el origen del panel */

/* Sistema > Fecha y hora */
static void zona_desenhar(void)
{
    char t[160];
    config_zona_t cz;
    ler_zona(&cz);
    fuso_data_t l;
    agora_local(&l);
    formatar_texto(t, sizeof t, _("Zona horaria actual: %s   (%02d:%02d)"), cz.nome, l.hora, l.minuto);
    titulo_pag(24, Y_PAG + 8, t);
    lista_zona.a = ALT - 34 - 112;
    lista_zona.l = LARG - 48;
    b_zona.y = ALT - 100;
    lista_desenhar(jn, &lista_zona);
    botao_desenhar(jn, &b_zona);
}

static bool zona_tratar(const evento_t *e)
{
    bool mudou = false;
    int at = lista_tratar(jn, &lista_zona, e);
    bool ap = botao_tratar(jn, &b_zona, e) || at == LISTA_ATIVOU;
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
    return mudou;
}

/* Sistema > Pantalla (la resolución y, con varias salidas de vídeo, su reparto) */
static void tela_pos(void)
{
    int yb = ALT - 100;
    bool multi = telas.qtd > 1;
    b_tela[0].y = b_tela[1].y = yb;
    lista_modo.x = 24; lista_modo.y = 40; lista_modo.l = LARG - 48;
    lista_modo.a = yb - 8 - 40 - (multi ? 72 : 0);
    for (int i = 0; i < 4; i++) b_telas[i].y = yb - 72;
    b_telas[4].y = yb - 36;
}

static void tela_desenhar(void)
{
    char t[160];
    tela_pos();
    formatar_texto(t, sizeof t, _("Resolución actual: %ux%u, %u bits"), tela.largura, tela.altura, tela.bpp);
    titulo_pag(24, Y_PAG + 4, t);
    if (!tela.a_quente) g_texto_n(jn, 24, Y_PAG + 22, _("Esta tarjeta cambia al reiniciar."), (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    lista_desenhar(jn, &lista_modo);
    for (int i = 0; i < 2; i++) botao_desenhar(jn, &b_tela[i]);
    if (telas.qtd > 1) {
        b_telas[0].apertado = telas.modo == TELAS_MODO_EXTENDER;
        b_telas[1].apertado = telas.modo == TELAS_MODO_ESPELHO;
        for (int i = 0; i < 5; i++) botao_desenhar(jn, &b_telas[i]);
        formatar_texto(t, sizeof t, _("%u pantallas: escritorio de %ux%u"), telas.qtd, telas.desk_largura, telas.desk_altura);
        g_texto_n(jn, 24, b_telas[0].y - 18, t, (LARG - 48) / LETRA_L, COR_TEXTO_FRACO);
    }
}

static bool tela_tratar(const evento_t *e)
{
    bool mudou = false;
    tela_pos();
    if (telas.qtd > 1) {   /* D290: cada botón se aplica y se guarda al momento */
        int n = (int)telas.qtd, modo = (int)telas.modo, lado = (int)telas.lado, prin = (int)telas.principal;
        bool tocou = false;
        if (botao_tratar(jn, &b_telas[0], e)) { modo = TELAS_MODO_EXTENDER; tocou = true; }
        if (botao_tratar(jn, &b_telas[1], e)) { modo = TELAS_MODO_ESPELHO; tocou = true; }
        if (botao_tratar(jn, &b_telas[2], e)) { prin = (prin + 1) % n; tocou = true; }
        if (botao_tratar(jn, &b_telas[3], e)) { lado = lado == TELAS_LADO_ABAIXO ? TELAS_LADO_DIREITA : TELAS_LADO_ABAIXO; tocou = true; }
        if (botao_tratar(jn, &b_telas[4], e)) { sel_res = (sel_res + 1) % n; mudou = true; ler_modos_video(); }
        if (tocou) {
            int r = telas_sis(TELAS_OP_CONFIG, (unsigned)modo | (unsigned)lado << 8 | (unsigned)prin << 16, 0);
            if (r == 0) { telas_guardar_admin(); copia_texto(estado, _("Pantallas cambiadas y guardadas"), sizeof estado); }
            else formatar_texto(estado, sizeof estado, _("No se pudo: %s"), nome_erro(r));
            ler_modos_video();
            mudou = true;
        }
    }
    int at = lista_tratar(jn, &lista_modo, e);
    bool ap = botao_tratar(jn, &b_tela[0], e) || at == LISTA_ATIVOU;
    bool gu = botao_tratar(jn, &b_tela[1], e);
    if ((ap || gu) && lista_modo.sel >= 0) {
        modo_video_t *m = &modos[indice_modo_l[lista_modo.sel]];
        bool igual = m->largura == tela.largura && m->altura == tela.altura;
        int op = tela.a_quente && !igual ? MUDAR_AGORA : 0;
        if (gu || !tela.a_quente) op |= MUDAR_GUARDAR;
        if (sel_res > 0 && telas.qtd > 1) {   /* D290: la resolución de una pantalla extra */
            int r = telas_sis(TELAS_OP_RESOLUCAO, (unsigned)sel_res, m->largura | (unsigned)m->altura << 16);
            if (r == 0) { telas_guardar_admin(); formatar_texto(estado, sizeof estado, _("Pantalla %d: %ux%u"), sel_res + 1, m->largura, m->altura); }
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
    return mudou;
}

/* Escritorio > Fuentes */
static bool fuentes_tratar(const evento_t *e)
{
    bool mudou = false;
    fuentes_pos();
    if (botao_tratar(jn, &b_fnt_inst, e)) {   /* D356: instalar y quitar fuentes (para la cuenta o para todas) */
        char *av[3] = { "fuentes", "--ventana", NULL };
        executar("/bin/fuentes", av);
        mudou = true;
    }
    if (n_fnt > 0) {
        int at = lista_tratar(jn, &lista_fnt, e);
        if (at) { fuentes_vista(); mudou = true; }
        for (int k = 0; k < 3; k++)
            if (botao_tratar(jn, &b_fnt[k], e)) {
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
    return mudou;
}

static void fuentes_predet(void)
{
    conf.fuente_doc[0] = 0;
    conf.fuente_tit[0] = 0;
    dizer_gravado(conf_gravar());
}

/* IA */
static void ia_guardar(void)
{
    ia_recolher();
    int r = ia_config_gravar(&cfg_ia, 1);
    if (r < 0) formatar_texto(estado, sizeof estado, _("No se pudo guardar: %s"), nome_erro(r));
    else if (!persiste) dizer(_("Guardado solo en esta sesión: /home no es un disco."));
    else dizer(_("Guardado en /home/.config/ia. Los programas ya lo usan."));
}

static bool ia_tratar(const evento_t *ep)
{
    const evento_t e = *ep;
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
    if (botao_tratar(jn, &b_ia[0], &e)) { ia_guardar(); mudou_ia = true; }
    if (botao_tratar(jn, &b_ia[1], &e)) { ia_cargar_modelos(); mudou_ia = true; }
    return mudou_ia;
}

/* Red > Escritorio remoto */
static bool vnc_tratar(const evento_t *e)
{
    if (caixa_tratar(jn, &c_vnc_clave, e)) { vnc_accion(2); vnc_refrescar(); }
    lista_tratar(jn, &lista_vnc, e);
    for (int k = 0; k < 5; k++)
        if (botao_tratar(jn, &b_vnc[k], e)) { vnc_accion(k); vnc_refrescar(); }
    if (botao_tratar(jn, &b_vnc_des, e)) { vnc_desconectar_sel(); vnc_refrescar(); }
    return true;
}

/* ------------------------------------------------------------ el registro de secciones (D367) */

/* ------------------------------------------------------------ Escritorio > Barra y menú Inicio (D361, D367) */
static info_barra_t ib;
static void barra_ler(void) { memset(&ib, 0, sizeof ib); info_sistema(INFO_BARRA, &ib, sizeof ib); }

/* Las dependencias (conf_reglas.c) leen el valor por la clave de sagui.conf. */
static int barra_leer_regla(const char *n, void *c)
{
    (void)c;
    for (int b = 0; b < BC_N; b++) if (compara_texto(n, claves_barra[b]) == 0) return ib.v[b];
    return -1;
}
static const char *barra_bloqueo(int bc) { const char *m = conf_bloqueo(claves_barra[bc], barra_leer_regla, NULL, 0); return m ? _(m) : NULL; }

/* Cambia un ajuste: se aplica al momento y se guarda. */
static void barra_cambiar(int bc, int v)
{
    barra_ler();
    ib.versao = 1;
    ib.v[bc] = (unsigned char)(v < 0 ? 0 : v > barra_maximo[bc] ? barra_maximo[bc] : v);
    info_sistema(INFO_BARRA_GRAVAR, &ib, sizeof ib);
    barra_ler();
    memcpy(conf.barra, ib.v, sizeof conf.barra);
    conf.barra_def = 1;
    registrar("configuracion: barra %s=%d", claves_barra[bc], ib.v[bc]);
    dizer_gravado(conf_gravar());
}

static void barra_predet(void)
{
    memcpy(conf.barra, barra_padrao, sizeof conf.barra);
    conf.barra_def = 1;
    aplicar_barra();
    barra_ler();
    registrar("configuracion: barra restablecida");
    dizer_gravado(conf_gravar());
}

/* Mezcla simple para los colores de la vista previa. */
static uint32_t mezcla(uint32_t c, uint32_t alvo, int pct)
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int a = (int)((c >> s) & 255), b = (int)((alvo >> s) & 255);
        r |= (uint32_t)(a + (b - a) * pct / 100) << s;
    }
    return r;
}

/* La vista previa: una pantalla pequeña con la barra (arriba o abajo, su color, el botón Inicio, tareas, bandeja y
 * reloj) y el menú Inicio del estilo elegido abierto a su lado. */
static void barra_previa(int x, int y, int l, int a, uint32_t acento)
{
    g_retangulo(jn, x, y, l, a, 0x4C6A8C);
    g_retangulo(jn, x, y + a / 2, l, a - a / 2, 0x3A5578);
    g_moldura(jn, x - 1, y - 1, l + 2, a + 2, COR_BORDA);
    static const uint32_t fundos[5] = { 0x18202A, 0xD8DEE4, 0, 0x000000, 0x0E1A38 }, botoes[5] = { 0x2C3A46, 0xEEF1F4, 0, 0x1C1C1C, 0x1C2E5A };
    int tema = ib.v[BC_TEMA];
    uint32_t fb = tema == 2 ? mezcla(acento, 0, 60) : fundos[tema], bt = tema == 2 ? mezcla(acento, 0, 35) : botoes[tema];
    int bh = ib.v[BC_ALTURA] / 5 + 1;
    bool abajo = ib.v[BC_POSICION] == BC_POS_ABAJO;
    int by = abajo ? y + a - bh : y;
    if (ib.v[BC_AUTOOCULTAR]) {   /* oculta sola: solo un filo de color en el borde */
        g_retangulo(jn, x, abajo ? y + a - 2 : y, l, 2, fb);
    } else {
        g_retangulo(jn, x, by, l, bh, fb);
        g_retangulo(jn, x, abajo ? by : by + bh - 1, l, 1, acento);
        int iw = ib.v[BC_INICIO_TXT] == 1 ? 10 : 24;
        g_retangulo(jn, x + 2, by + 2, iw, bh - 4, acento);
        int tx = x + 4 + iw + 2;
        int nt = ib.v[BC_AGRUPAR] ? 2 : 3;
        int tw = ib.v[BC_ROTULOS] == 1 ? 8 : 20;
        for (int i = 0; i < nt; i++, tx += tw + 2) g_retangulo(jn, tx, by + 2, tw, bh - 4, i == 0 ? mezcla(bt, 0xFFFFFF, 25) : bt);
        int rx = x + l - 4;
        if (ib.v[BC_RELOJ]) { rx -= 22; g_retangulo(jn, rx, by + bh / 2 - 1, 22, 2, tema == 1 ? 0x2A3640 : 0xE8EEF4); rx -= 4; }
        for (int k = 3; k >= 0; k--)
            if (ib.v[BC_BAND_VOL + k]) { rx -= 5; g_retangulo(jn, rx, by + bh / 2 - 2, 4, 4, 0x6FD98F); }
    }
    /* el menú, pegado al botón Inicio y alejándose de la barra */
    int est = ib.v[BC_MENU];
    int mw = est == BC_MENU_DOS_COL ? 96 : est == BC_MENU_MODERNO ? 104 : 58, mh = est == BC_MENU_SAGUI ? 70 : 86;
    int mx = x + 2;
    int my = abajo ? by - mh - 1 : by + (ib.v[BC_AUTOOCULTAR] ? 2 : bh) + 1;
    if (est == BC_MENU_SAGUI) {
        g_retangulo(jn, mx, my, mw, mh, 0xF4F4F0); g_moldura(jn, mx, my, mw, mh, 0x101010);
        g_retangulo(jn, mx + 1, my + 1, 2, mh - 2, 0x0B7A3E);
        for (int i = 0; i < 5; i++) g_retangulo(jn, mx + 8, my + 6 + i * 12, mw - 16, 3, i == 2 ? 0x0B7A3E : 0x8A96A2);
    } else if (est == BC_MENU_CLASICO) {
        g_retangulo(jn, mx, my, mw, mh, 0xD4D0C8); g_moldura(jn, mx, my, mw, mh, 0x404040);
        for (int i = 0; i < mh - 2; i++) g_retangulo(jn, mx + 1, my + 1 + i, 12, 1, mezcla(0x6C9CD8, 0x08246A, i * 100 / (mh > 3 ? mh - 3 : 1)));
        for (int i = 0; i < 5; i++) g_retangulo(jn, mx + 18, my + 8 + i * 14, mw - 26, 3, i == 1 ? 0x0A246A : 0x6A6A6A);
    } else if (est == BC_MENU_DOS_COL) {
        g_retangulo(jn, mx, my, mw, mh, 0xFFFFFF); g_retangulo(jn, mx + mw / 2, my + 14, mw / 2, mh - 14 - 8, 0xD3E5FA);
        g_retangulo(jn, mx, my, mw, 14, 0x2A5FC8); g_retangulo(jn, mx + 3, my + 2, 10, 10, 0xF2F2F2);
        g_retangulo(jn, mx, my + mh - 8, mw, 8, 0x1B3F94); g_moldura(jn, mx, my, mw, mh, 0x0A3C9C);
        for (int i = 0; i < 4; i++) { g_retangulo(jn, mx + 5, my + 20 + i * 11, mw / 2 - 12, 3, 0x6A737C); g_retangulo(jn, mx + mw / 2 + 5, my + 20 + i * 11, mw / 2 - 12, 3, 0x0F2A5C); }
    } else {
        bool tr = ib.v[BC_TRANSLUCIDO];
        g_retangulo(jn, mx, my, mw, mh, tr ? 0x1E3A5E : 0x0E1A2C);
        g_retangulo(jn, mx + 3, my + 3, mw * 6 / 10, mh - 6, tr ? 0xDCE6F0 : 0xF2F6FA);
        int sy = ib.v[BC_BUSCADOR] ? my + mh - 12 : my + 4;
        g_retangulo(jn, mx + 5, sy, mw * 6 / 10 - 4, 7, 0xFFFFFF); g_moldura(jn, mx + 5, sy, mw * 6 / 10 - 4, 7, 0x7A8EA8);
        for (int i = 0; i < 4; i++) g_retangulo(jn, mx + 8, my + (ib.v[BC_BUSCADOR] ? 8 : 16) + i * 11, mw * 6 / 10 - 12, 3, 0x6A737C);
        for (int i = 0; i < 4; i++) g_retangulo(jn, mx + mw * 6 / 10 + 8, my + 8 + i * 11, mw * 4 / 10 - 14, 3, 0xE8EEF4);
        g_moldura(jn, mx, my, mw, mh, 0x8AA8D0);
    }
}


/* Una fila de opciones (radio): el rótulo a la izquierda y las opciones a su derecha, que pasan a la línea siguiente si no caben.
 * `bc` es el ajuste (BC_*); las opciones que las dependencias dejan sin efecto salen en gris (solo la primera, la «apagada», se puede). */
static int br_opciones(int y, const char *rot, int bc, const char *const *op, int n)
{
    int x = XL + 150;
    const char *bloq = barra_bloqueo(bc);
    ui_texto(XL, y, _(rot), COR_TEXTO);
    for (int k = 0; k < n; k++) {
        int w = 22 + g_largura_texto(_(op[k])) + 16;
        if (x + w > XR && x > XL + 150) { x = XL + 150; y += 22; }
        ui_gris = bloq != NULL && k != 0;
        if (ui_opcion(x, y, op[k], ib.v[bc] == k) && ib.v[bc] != k) { barra_cambiar(bc, k); ui_mudou = true; }
        x += w;
    }
    ui_gris = false;
    y += 24;
    if (bloq) { ui_texto(XL + 22, y - 6, bloq, COR_TEXTO_FRACO); y += 14; }
    return y;
}

/* Una casilla (sí/no) de un ajuste. */
static int br_casilla(int y, const char *rot, int bc)
{
    const char *bloq = barra_bloqueo(bc);
    ui_gris = bloq != NULL && ib.v[bc] == 0;
    if (ui_casilla(XL, y, rot, ib.v[bc] != 0)) { barra_cambiar(bc, !ib.v[bc]); ui_mudou = true; }
    ui_gris = false;
    y += 24;
    if (bloq) { ui_texto(XL + 22, y - 8, bloq, COR_TEXTO_FRACO); y += 14; }
    return y;
}

static void pg_barra(void)
{
    static const char *const o_pos[2] = { N_("Arriba"), N_("Abajo") };
    static const char *const o_ico[2] = { N_("Pequeños (16)"), N_("Grandes (32)") };
    static const char *const o_rot[3] = { N_("Icono y texto"), N_("Solo icono"), N_("Solo texto") };
    static const char *const o_ini[3] = { N_("Logo y texto"), N_("Solo logo"), N_("Solo texto") };
    static const char *const o_fec[3] = { N_("Automática"), N_("Con fecha"), N_("Solo la hora") };
    static const char *const o_tema[5] = { N_("Oscuro"), N_("Claro"), N_("Acento"), N_("Negro"), N_("Azul") };
    static const char *const o_menu[4] = { N_("SaguiOS"), N_("Clásico"), N_("Dos columnas"), N_("Moderno") };
    info_aparencia_t ap;
    int y = 0, gy;
    if (!ui_ev) { memset(&ap, 0, sizeof ap); info_sistema(INFO_APARENCIA, &ap, sizeof ap); }
    /* vista previa */
    if (!ui_ev) barra_previa(XL, y + 26, 192, 130, ap.acento);
    ui_parrafo(XL + 210, y + 26, XR - XL - 210, _("Se aplica al momento. La vista previa dibuja la barra y el menú Inicio elegidos."), COR_TEXTO_FRACO);
    if (!ui_ev) ui_grupo(y, 170, N_("Vista previa"));
    y += 178;
    /* posición y tamaño */
    gy = y; y += 28;
    y = br_opciones(y, N_("Posición"), BC_POSICION, o_pos, 2);
    ui_texto(XL, y + 2, _("Altura"), COR_TEXTO);
    if (ui_boton(XL + 150, y - 2, 24, "-", false)) { int v = ib.v[BC_ALTURA] - 4; barra_cambiar(BC_ALTURA, v < 20 ? 20 : v); ui_mudou = true; }
    { char t[16]; formatar_texto(t, sizeof t, "%d", ib.v[BC_ALTURA]); ui_texto(XL + 150 + 30, y + 2, t, COR_TEXTO); }
    if (ui_boton(XL + 150 + 64, y - 2, 24, "+", false)) { barra_cambiar(BC_ALTURA, ib.v[BC_ALTURA] + 4); ui_mudou = true; }
    y += 30;
    y = br_opciones(y, N_("Iconos"), BC_ICONOS, o_ico, 2);
    y = br_casilla(y, N_("Ocultar sola"), BC_AUTOOCULTAR);
    y = br_casilla(y, N_("Agrupar ventanas"), BC_AGRUPAR);
    if (!ui_ev) ui_grupo(gy, y - gy, N_("Posición y tamaño"));
    y += 8;
    /* botones, reloj */
    gy = y; y += 28;
    y = br_opciones(y, N_("Botones de tareas"), BC_ROTULOS, o_rot, 3);
    y = br_opciones(y, N_("Botón Inicio"), BC_INICIO_TXT, o_ini, 3);
    y = br_casilla(y, N_("Reloj"), BC_RELOJ);
    y = br_opciones(y, N_("Fecha"), BC_FECHA, o_fec, 3);
    y = br_casilla(y, N_("Segundos"), BC_SEGUNDOS);
    if (!ui_ev) ui_grupo(gy, y - gy, N_("Botones de tareas y reloj"));
    y += 8;
    /* bandeja y color */
    gy = y; y += 28;
    ui_texto(XL, y, _("Bandeja"), COR_TEXTO);
    {
        static const char *const o_band[4] = { N_("Volumen"), N_("Red"), N_("Temperatura"), N_("Batería") };
        for (int k = 0; k < 4; k++)
            if (ui_casilla(XL + 150 + (k % 2) * 170, y + (k / 2) * 22, o_band[k], ib.v[BC_BAND_VOL + k] != 0)) { barra_cambiar(BC_BAND_VOL + k, !ib.v[BC_BAND_VOL + k]); ui_mudou = true; }
        y += 52;
    }
    y = br_opciones(y, N_("Color de la barra"), BC_TEMA, o_tema, 5);
    if (!ui_ev) ui_grupo(gy, y - gy, N_("Bandeja y color"));
    y += 8;
    /* menú Inicio */
    gy = y; y += 28;
    y = br_opciones(y, N_("Menú Inicio"), BC_MENU, o_menu, 4);
    { static const char *const o_bus[2] = { N_("Arriba"), N_("Abajo") }; y = br_opciones(y, N_("Buscador"), BC_BUSCADOR, o_bus, 2); }
    y = br_casilla(y, N_("Translúcido"), BC_TRANSLUCIDO);
    if (!ui_ev) ui_grupo(gy, y - gy, N_("Menú Inicio"));
    cont_h = y + 12;
}

/* Escritorio > Widgets (D365): el gestor es el programa «widgets»; aquí solo se abre. */
static void pg_widgets(void)
{
    int y = ui_parrafo(XL, 26, XR - XL, _("Los widgets de escritorio son pequeñas ventanas siempre a la vista (reloj, notas, monitor...). Se eligen y se colocan en su gestor."), COR_TEXTO);
    if (ui_boton(XL, y + 8, 240, N_("Widgets de escritorio..."), false)) {
        char *argv_w[] = { "widgets", NULL };
        executar("/bin/widgets", argv_w);
        ui_mudou = true;
    }
    y += 44;
    if (!ui_ev) ui_grupo(0, y, N_("Widgets"));
    cont_h = y + 12;
}

/* Red > VPN (WireGuard) (D363): la ventana es la del programa wg (importar, claves, conectar y estado); aquí el resumen y el botón. */
static void vpn_abrir(void)
{
    char *av[] = { "wg", "--ventana", NULL };
    int pid = executar("/bin/wg", av);
    if (pid > 0) definir_grupo(pid, 0);
}

static void pg_vpn(void)
{
    char t[200], a[16];
    static info_wg_t wg;
    if (info_sistema(INFO_WG, &wg, sizeof wg) > 0)
        formatar_texto(t, sizeof t, _("Encendida: %s, %s. Último saludo: %s."), wg.config.nome, ip_para_texto(wg.config.ip, a),
                       wg.wg.npares && wg.wg.pares[0].ultimo_hs ? _("sí") : _("todavía no"));
    else copia_texto(t, _("Apagada. Importa un .conf o genera tus claves en la ventana de la VPN."), sizeof t);
    int y = ui_parrafo(XL, 26, XR - XL, t, COR_TEXTO);
    if (ui_boton(XL, y + 8, 220, N_("VPN (WireGuard)..."), false)) { vpn_abrir(); ui_mudou = true; }
    y += 44;
    if (!ui_ev) ui_grupo(0, y, N_("VPN (WireGuard)"));
    cont_h = y + 12;
}

/* Para añadir una sección: una entrada en seccoes[] (y su índice en el enum de más arriba, en el mismo orden) y las
 * funciones que use. Ver la guía «cómo portar una sección al árbol» en docs/decisoes.md (D367). */
typedef struct {
    const char *chave;                 /* nombre para «configuracion --ventana CLAVE» y las pruebas */
    const char *titulo;                /* N_(): lo que se ve en el árbol y en el título del panel */
    const char *log;                   /* lo que sale en «configuracion: pagina X» (lo esperan las pruebas) */
    unsigned char nivel;               /* 0 = categoría de primer nivel, 1 = dentro de la categoría de arriba */
    void (*preparar)(void);            /* al abrirla: leer estado (NULL = nada) */
    void (*sair)(void);                /* al dejarla */
    void (*desenhar)(void);            /* NULL = es solo una categoría (se abre su primera sección) */
    bool (*tratar)(const evento_t *e); /* eventos en coordenadas locales; true = repintar */
    void (*predet)(void);              /* «Predeterminado» (NULL = el botón no sale) */
    void (*aplicar)(void);             /* «Aplicar» / «Aceptar»: confirma lo pendiente (NULL = nada) */
    int  (*intervalo)(void);           /* ms entre repintados automáticos (NULL o 0 = solo con eventos) */
    bool (*tique)(void);               /* en cada intervalo; true = repintar */
} seccao_t;

#define PAGINA_UI(nome) \
    static void d_##nome(void) { ui_inicio(NULL); pg_##nome(); } \
    static bool t_##nome(const evento_t *e) { ui_inicio(e); pg_##nome(); return ui_mudou; }
PAGINA_UI(teclado)
PAGINA_UI(idioma)
PAGINA_UI(capturas)
PAGINA_UI(colores)
PAGINA_UI(acel)
PAGINA_UI(kernel)
PAGINA_UI(bloqueo)
PAGINA_UI(barra)
PAGINA_UI(widgets)
PAGINA_UI(vpn)

static void p_tela(void) { ler_modos_video(); }
static void p_acel(void)
{
    acel_preparar();
    registrar("configuracion: pantalla vista aceleracion");
    /* el bloque fijo de la tarjeta (solo lectura): lo que se ve, también al registro para las pruebas */
    registrar("configuracion: tarjeta [%s] vram_kb=%u 2d=%s 3d=%s volcado=%u", ac_modelo, ac_vid.vbe_versao ? ac_vid.memoria_kb : 0,
              ac2.ativo ? ac2.nome : "cpu", ac3.nome, ac2.volcado_estado);
    registrar("configuracion: gpu energia soporte=%u modo=%u estado=%d motor=%u memoria=%u temp=%d zonas=%u vram=%u/%u", ac_en.soporte,
              ac_en.modo, ac_en.estado, ac_en.sclk_khz, ac_en.mclk_khz, ac_en.temp_mili_c, ac_en.n_zonas, ac_en.vram_usada_kb, ac_en.vram_kb);
}
static void p_kernel(void) { kc_preparar(); registrar("configuracion: pantalla vista kernel"); }
static void p_sonido(void) { mic_abrir(); }
static void p_colores(void) { listar_imagens(); }
static void p_fuentes(void) { fuentes_preparar(); }
static void p_ia(void) { ia_preparar(); }
static void p_barra(void) { barra_ler(); registrar("configuracion: escritorio vista barra"); }
static void p_vnc(void) { vnc_refrescar(); }
static void p_mandos(void) { md_preparar(); }
static void p_usuarios(void) { us_preparar(); }
static void p_capturas(void) { caixa_definir(&c_capturas, conf.capturas); cap_pend = false; }
static void s_sonido(void) { mic_cerrar(); }
static void s_mandos(void) { md_fase = 0; }
static void d_sonido(void) { snd_desenhar(); }
static void d_zona(void) { zona_desenhar(); }
static void d_tela(void) { tela_desenhar(); }
static void d_fuentes(void) { cont_h = 0; fuentes_desenhar(); }
static void d_ia(void) { ia_desenhar(); }
static void d_vnc(void) { vnc_desenhar(); }
static void d_mandos(void) { md_desenhar(); }
static void d_usuarios(void) { us_desenhar(); }
static void predet_teclado(void) { aplicar_teclado(TECLADO_ES); conf.teclado = TECLADO_ES; dizer_gravado(conf_gravar()); }
static void predet_idioma(void)
{
    copia_texto(conf.idioma, "es", sizeof conf.idioma);
    idioma_fijar("es");
    traducir_rotulos();
    ler_modos_video();
    dizer_gravado(conf_gravar());
}
static int iv_vnc(void) { return 1000; }
static int iv_sonido(void) { return mic_aberto ? 60 : 0; }   /* el medidor del micrófono (D292) */
static int iv_mandos(void) { return 50; }                    /* la prueba en vivo de los mandos (D307) */
static bool tq_vnc(void) { vnc_refrescar(); return true; }
static bool tq_sonido(void) { if (!mic_aberto) return false; mic_refrescar(); return true; }
static bool tq_mandos(void) { return md_tique(); }
static bool t_sonido(const evento_t *e) { return snd_tratar(e); }
static bool t_zona(const evento_t *e) { return zona_tratar(e); }
static bool t_tela(const evento_t *e) { return tela_tratar(e); }
static bool t_fuentes(const evento_t *e) { return fuentes_tratar(e); }
static bool t_ia(const evento_t *e) { return ia_tratar(e); }
static bool t_vnc(const evento_t *e) { return vnc_tratar(e); }
static bool t_mandos(const evento_t *e) { return md_tratar(e); }
static bool t_usuarios(const evento_t *e) { return us_tratar(e); }

static const seccao_t seccoes[S_QTD] = {
    [S_SISTEMA]   = { "sistema", N_("Sistema"), "Sistema", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    [S_TELA]      = { "tela", N_("Pantalla"), "Pantalla", 1, p_tela, NULL, d_tela, t_tela, NULL, NULL, NULL, NULL },
    [S_ACEL]      = { "aceleracion", N_("Aceleración gráfica"), "Aceleración gráfica", 1, p_acel, NULL, d_acel, t_acel, acel_predet, NULL, NULL, NULL },
    [S_KERNEL]    = { "kernel", N_("Kernel"), "Kernel", 1, p_kernel, NULL, d_kernel, t_kernel, kc_predet, NULL, NULL, NULL },
    [S_ZONA]      = { "zona", N_("Fecha y hora"), "Fecha y hora", 1, NULL, NULL, d_zona, t_zona, NULL, NULL, NULL, NULL },
    [S_IDIOMA]    = { "idioma", N_("Idioma"), "Idioma", 1, NULL, NULL, d_idioma, t_idioma, predet_idioma, NULL, NULL, NULL },
    [S_USUARIOS]  = { "usuarios", N_("Usuarios"), "Usuarios", 1, p_usuarios, NULL, d_usuarios, t_usuarios, NULL, NULL, NULL, NULL },
    [S_ESCRITORIO] = { "escritorio-cat", N_("Escritorio"), "Escritorio", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    [S_COLORES]   = { "escritorio", N_("Colores y fondo"), "Escritorio", 1, p_colores, NULL, d_colores, t_colores, colores_predet, NULL, NULL, NULL },
    [S_BARRA]     = { "barra", N_("Barra y menú Inicio"), "Barra y menú Inicio", 1, p_barra, NULL, d_barra, t_barra, barra_predet, NULL, NULL, NULL },
    [S_WIDGETS]   = { "widgets", N_("Widgets"), "Widgets", 1, NULL, NULL, d_widgets, t_widgets, NULL, NULL, NULL, NULL },
    [S_FUENTES]   = { "fuentes", N_("Fuentes"), "Fuentes", 1, p_fuentes, NULL, d_fuentes, t_fuentes, fuentes_predet, NULL, NULL, NULL },
    [S_CAPTURAS]  = { "capturas", N_("Capturas"), "Capturas", 1, p_capturas, cap_sair, d_capturas, t_capturas, cap_predet, cap_aplicar, NULL, NULL },
    [S_DISPOSITIVOS] = { "dispositivos", N_("Dispositivos"), "Dispositivos", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    [S_TECLADO]   = { "teclado", N_("Teclado"), "Teclado", 1, NULL, NULL, d_teclado, t_teclado, predet_teclado, NULL, NULL, NULL },
    [S_MANDOS]    = { "mandos", N_("Mandos"), "Mandos", 1, p_mandos, s_mandos, d_mandos, t_mandos, NULL, NULL, iv_mandos, tq_mandos },
    [S_SONIDO]    = { "sonido", N_("Sonido"), "Sonido", 1, p_sonido, s_sonido, d_sonido, t_sonido, snd_predet, NULL, iv_sonido, tq_sonido },
    [S_REDES]     = { "redes", N_("Red"), "Red", 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    [S_BLOQUEO]   = { "red", N_("Publicidad (DNS)"), "Red", 1, NULL, NULL, d_bloqueo, t_bloqueo, NULL, NULL, NULL, NULL },
    [S_VPN]       = { "vpn", N_("VPN (WireGuard)"), "VPN", 1, NULL, NULL, d_vpn, t_vpn, NULL, NULL, NULL, NULL },
    [S_VNC]       = { "vnc", N_("Escritorio remoto"), "Escritorio remoto", 1, p_vnc, NULL, d_vnc, t_vnc, NULL, NULL, iv_vnc, tq_vnc },
    [S_IA]        = { "ia", N_("IA"), "IA", 0, p_ia, NULL, d_ia, t_ia, NULL, ia_guardar, NULL, NULL },
};

/* Nombres antiguos y alias que acepta «--ventana SECCION». */
static int seccion_por_nombre(const char *n)
{
    static const struct { const char *alias; int s; } alias[] = {
        { "pantalla", S_TELA }, { "resolucion", S_TELA }, { "aceleracion", S_ACEL }, { "gpu", S_ACEL }, { "fecha", S_ZONA },
        { "hora", S_ZONA }, { "fondo", S_COLORES }, { "colores", S_COLORES }, { "fuente", S_FUENTES }, { "sonido", S_SONIDO },
        { "teclado", S_TECLADO }, { "mandos", S_MANDOS }, { "joystick", S_MANDOS }, { "usuarios", S_USUARIOS },
        { "idioma", S_IDIOMA }, { "bloqueo", S_BLOQUEO }, { "escritorio-remoto", S_VNC }, { "kernel", S_KERNEL },
    };
    for (int i = 0; i < S_QTD; i++) if (compara_texto(n, seccoes[i].chave) == 0) return i;
    for (unsigned i = 0; i < sizeof alias / sizeof alias[0]; i++) if (compara_texto(n, alias[i].alias) == 0) return alias[i].s;
    return -1;
}

/* ------------------------------------------------------------ la ventana: árbol, panel y botones */

static int Wj, Hj;                       /* tamaño de la ventana */
static arvore_t arv;
static const char *arv_txt[S_QTD];
static unsigned char arv_nivel[S_QTD], arv_aberto[S_QTD];
static botao_t b_ok = { 0, 0, 88, 28, N_("Aceptar"), false };
static botao_t b_cancel = { 0, 0, 88, 28, N_("Cancelar"), false };
static botao_t b_apl = { 0, 0, 88, 28, N_("Aplicar"), false };
static botao_t b_pred = { 0, 0, 150, 28, N_("Predeterminado"), false };
static bool foco_arvore = true;          /* el teclado va al árbol (true) o a la sección (false) */
static bool sb_arrasta;
static conf_t conf0;                     /* lo que había en el archivo al abrir */
static uint32_t acento_ini;              /* y lo que había puesto en el sistema (aunque no hubiera archivo) */
static int teclado_ini = -1, volumen_ini = -1, silencio_ini;
#define ARQ_UI     "/home/.config/configuracion.ui"   /* sección y tamaño de la última vez (y la copia de la sesión, en /tmp) */
#define ARQ_UI_TMP "/tmp/configuracion.ui"

/* La barra de desplazamiento del panel: x, y, alto del carril; y el pulgar. */
static void sb_geo(int *x, int *y, int *h, int *py, int *ph)
{
    *x = PX + PW - 12; *y = PY; *h = PH;
    int max = cont_h - PH;
    int ta = cont_h > 0 ? PH * PH / cont_h : PH;
    if (ta < 24) ta = 24;
    if (ta > PH) ta = PH;
    *ph = ta;
    *py = max > 0 ? PY + (PH - ta) * rolagem / max : PY;
}

static void limitar_rolagem(void)
{
    int max = cont_h - PH;
    if (max < 0) max = 0;
    if (rolagem > max) rolagem = max;
    if (rolagem < 0) rolagem = 0;
}

static void layout_paginas(void)
{
    lista_zona.x = 24; lista_zona.y = Y_PAG + 34; lista_zona.l = LARG - 48; lista_zona.a = ALT - 34 - 112;
    lista_modo.x = 24; lista_modo.y = 40; lista_modo.l = LARG - 48;
    tela_pos();
    c_vnc_clave.x = 24; c_vnc_clave.y = Y_PAG + 124; c_vnc_clave.l = 220;
    lista_vnc.x = 24; lista_vnc.y = Y_PAG + 238; lista_vnc.l = LARG - 48 - 170; lista_vnc.a = 80;
    lista_fnt.x = 24; lista_fnt.y = Y_PAG + 34; lista_fnt.l = FN_LISTA_L; lista_fnt.a = ALT - 34 - 112;
    /* IA: la lista de proveedores a la izquierda; a la derecha las cajas y los botones (D145, D367) */
    c_ia[0].x = 230; c_ia[0].y = Y_PAG + 22;  c_ia[0].l = LARG - 24 - 230;
    c_ia[1].x = 230; c_ia[1].y = Y_PAG + 66;  c_ia[1].l = LARG - 24 - 230;
    int cw = (LARG - 24 - 230 - 8) / 2;
    c_ia[2].x = 230;          c_ia[2].y = Y_PAG + 118; c_ia[2].l = cw;
    c_ia[3].x = 230 + cw + 8; c_ia[3].y = Y_PAG + 118; c_ia[3].l = cw;
    c_ia[4].x = 230;          c_ia[4].y = Y_PAG + 166; c_ia[4].l = cw;
    c_ia[5].x = 230 + cw + 8; c_ia[5].y = Y_PAG + 166; c_ia[5].l = cw;
    c_ia[0].oculta = false;
    lista_prov.x = 24; lista_prov.y = Y_PAG + 24; lista_prov.l = 190; lista_prov.a = 220;
    lista_mod.x = 230; lista_mod.y = Y_PAG + 296; lista_mod.l = LARG - 24 - 230; lista_mod.a = 90;
    for (int i = 0; i < 2; i++) b_ia[i].y = Y_PAG + 206;
    for (int i = 0; i < 4; i++) b_alvo[i].y = Y_PAG + 262;
}

static void layout(void)
{
    Wj = jn->larg; Hj = jn->alt;
    int tw = Wj >= 900 ? 214 : 192;
    PX = 8 + tw + 12;
    PY = 40;
    PW = Wj - PX - 8;
    PH = Hj - PY - 72;
    arv.x = 8; arv.y = 8; arv.l = tw; arv.a = Hj - 16;
    b_ok.x = Wj - 8 - 3 * 88 - 16;  b_ok.y = Hj - 40;
    b_cancel.x = b_ok.x + 96;       b_cancel.y = Hj - 40;
    b_apl.x = b_cancel.x + 96;      b_apl.y = Hj - 40;
    b_pred.x = PX; b_pred.y = Hj - 40;
    /* la barra de progreso del fondo, en la línea del estado */
    if (barra_fundo) progresso_liberar(barra_fundo);
    barra_fundo = progresso_criar(PX, Hj - 66, 260, 14, false);
    if (barra_fundo) progresso_texto(barra_fundo, true);
    layout_paginas();
    limitar_rolagem();
}

/* Los rótulos que se calculan con el idioma: el árbol, la ventana y los modos del fondo (D145, D313). */
static void traducir_rotulos(void)
{
    for (int i = 0; i < S_QTD; i++) { arv_txt[i] = _(seccoes[i].titulo); arv_nivel[i] = seccoes[i].nivel; }
    arv.n = S_QTD; arv.textos = arv_txt; arv.nivel = arv_nivel; arv.aberto = arv_aberto;
    if (jn) titulo_janela(jn->id, _("Configuración"));
}

static void sair_seccion(void)
{
    if (seccoes[pagina].sair) seccoes[pagina].sair();
}

/* Abre una sección (una categoría abre su primera hoja). */
static void ir_seccion(int i)
{
    if (i < 0 || i >= S_QTD) return;
    if (!seccoes[i].desenhar && i + 1 < S_QTD && seccoes[i + 1].nivel > seccoes[i].nivel) i++;
    if (!seccoes[i].desenhar) return;
    sair_seccion();
    pagina = i;
    rolagem = 0;
    cont_h = 0;
    arv.sel = i;
    arvore_mostrar(&arv, i);
    if (seccoes[i].preparar) seccoes[i].preparar();
    registrar("configuracion: pagina %s", seccoes[i].log);
}

/* Después de «Cancelar»: lo que se cambió en esta apertura vuelve a lo que había. La aceleración, el kernel y el micrófono
 * quedan fuera: piden permisos de administrador o se aplican con su propia autoprueba, y se deshacen en su sección. */
static void deshacer_apertura(void)
{
    if (teclado_ini >= 0 && teclado_layout(-1) != teclado_ini) aplicar_teclado(teclado_ini);
    if (volumen_ini >= 0 && (audio_volume_mestre(-1) != volumen_ini || audio_silencio(-1) != silencio_ini)) aplicar_volume(volumen_ini, silencio_ini);
    if ((conf0.son_def & 1) ? conf.balance != conf0.balance : (conf.son_def & 1)) aplicar_balance((conf0.son_def & 1) ? conf0.balance : 100);
    if ((conf0.son_def & 2) && conf.latencia != conf0.latencia) aplicar_latencia(conf0.latencia);
    if (conf.eq_def && memcmp(conf.eq, conf0.eq, sizeof conf.eq) != 0) {
        int plano[EQ_BANDAS] = { 0 };
        eq_aplicar(conf0.eq_def ? conf0.eq : plano);
    }
    if (conf0.zona[0] && compara_texto(conf.zona, conf0.zona) != 0) aplicar_zona(conf0.zona);
    {
        info_aparencia_t ap;
        memset(&ap, 0, sizeof ap);
        info_sistema(INFO_APARENCIA, &ap, sizeof ap);
        if (acento_ini && ap.acento != acento_ini) acento_cor((int)acento_ini);
    }
    if (compara_texto(conf.idioma, conf0.idioma) != 0 && conf0.idioma[0]) idioma_fijar(conf0.idioma);
    if (compara_texto(conf.fundo, conf0.fundo) != 0 || conf.modo != conf0.modo) fundo_aplicar(conf0.fundo, conf0.fundo[0] ? conf0.modo : M_DEGRADE);
    conf.teclado = conf0.teclado; conf.volume = conf0.volume; conf.silencio = conf0.silencio;
    conf.son_def = (conf.son_def & ~3) | (conf0.son_def & 3);
    conf.balance = conf0.balance; conf.latencia = conf0.latencia;
    conf.eq_def = conf0.eq_def; memcpy(conf.eq, conf0.eq, sizeof conf.eq);
    copia_texto(conf.zona, conf0.zona, sizeof conf.zona);
    conf.acento = conf0.acento;
    copia_texto(conf.idioma, conf0.idioma, sizeof conf.idioma);
    copia_texto(conf.fundo, conf0.fundo, sizeof conf.fundo);
    conf.modo = conf0.modo;
    copia_texto(conf.capturas, conf0.capturas, sizeof conf.capturas); conf.cap_sonido = conf0.cap_sonido;
    copia_texto(conf.fuente_doc, conf0.fuente_doc, sizeof conf.fuente_doc);
    copia_texto(conf.fuente_tit, conf0.fuente_tit, sizeof conf.fuente_tit);
    conf_gravar();
    registrar("configuracion: cancelar: se deshace lo de esta apertura");
}

static void ui_guardar(void)
{
    char t[96];
    formatar_texto(t, sizeof t, "seccion=%s\nancho=%d\nalto=%d\n", seccoes[pagina].chave, Wj, Hj);
    int fd = abrir(ARQ_UI_TMP, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever_tudo(fd, t, (unsigned)tam_texto(t)); fechar(fd); }
    if (!persiste) return;
    criar_pasta(DIR_CONF);
    fd = abrir(ARQ_UI, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) { escrever_tudo(fd, t, (unsigned)tam_texto(t)); fechar(fd); }
}

/* Lee lo de la última vez: la sección y el tamaño de la ventana (0 = no hay). */
static void ui_ler(int *ancho, int *alto, int *sec)
{
    *ancho = *alto = 0;
    int fd = abrir(ARQ_UI_TMP, ABRIR_LER);
    if (fd < 0) fd = abrir(ARQ_UI, ABRIR_LER);
    if (fd < 0) return;
    char t[160];
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        char *eq = acha_char(p, '=');
        if (eq) {
            *eq = 0;
            if (compara_texto(p, "ancho") == 0) *ancho = texto_para_int(eq + 1);
            else if (compara_texto(p, "alto") == 0) *alto = texto_para_int(eq + 1);
            else if (compara_texto(p, "seccion") == 0 && *sec < 0) *sec = seccion_por_nombre(eq + 1);
        }
        if (!fim) break;
        p = fim + 1;
    }
}

/* El dibujo: primero el panel (que puede pasarse de sus bordes al desplazarse) y encima el resto. */
static void desenhar(void)
{
    const evento_t *ev_ant = ui_ev;      /* se puede llamar desde dentro de un evento (progreso del fondo, bloqueo) */
    int n_ant = ui_n;
    bool mud_ant = ui_mudou;
    OX = OY = 0;
    g_retangulo(jn, 0, 0, Wj, Hj, COR_FUNDO);
    OX = PX - 16; OY = PY - rolagem;
    cont_h = 0;
    seccoes[pagina].desenhar();
    if (cont_h < 0) cont_h = 0;
    limitar_rolagem();
    OX = OY = 0;
    /* lo que tapa lo que se salga del panel */
    g_retangulo(jn, PX - 6, 0, Wj - PX + 6, PY, COR_FUNDO);
    g_retangulo(jn, PX - 6, PY + PH, Wj - PX + 6, Hj - PY - PH, COR_FUNDO);
    g_retangulo(jn, PX - 6, 0, 6, Hj, COR_FUNDO);
    /* título de la sección: «Categoría > Sección» */
    {
        int pai = pagina;
        while (pai > 0 && seccoes[pai].nivel > 0) pai--;
        int x = PX;
        if (pai != pagina) {
            const char *c = _(seccoes[pai].titulo);
            g_texto(jn, x, 12, c, COR_TEXTO_FRACO);
            x += g_largura_texto(c) + 8;
            g_texto(jn, x, 12, ">", COR_TEXTO_FRACO);
            x += 24;
        }
        const char *t = _(seccoes[pagina].titulo);
        g_texto(jn, x, 12, t, COR_TEXTO);
        g_texto(jn, x + 1, 12, t, COR_TEXTO);
        g_retangulo(jn, PX, PY - 10, PW, 1, COR_BORDA);
    }
    /* barra de desplazamiento */
    if (cont_h > PH) {
        int sx, sy, sh, py, ph;
        sb_geo(&sx, &sy, &sh, &py, &ph);
        g_retangulo(jn, sx, sy, 12, sh, 0xDCE0E4);
        g_moldura(jn, sx, sy, 12, sh, COR_BORDA);
        g_retangulo(jn, sx + 1, py + 1, 10, ph - 2, 0x9AA3AC);
        g_moldura(jn, sx + 1, py + 1, 10, ph - 2, COR_BORDA);
    }
    arvore_desenhar(jn, &arv);
    /* el estado y la barra del fondo */
    if (mostrando_barra && barra_fundo) progresso_desenhar(jn, barra_fundo);
    else if (estado[0]) g_texto_n(jn, PX, Hj - 66, estado, PW / LETRA_L, COR_TEXTO_FRACO);
    else if (!persiste) g_texto_n(jn, PX, Hj - 66, _("Aviso: /home no es un disco persistente: los cambios no se recordarán."), PW / LETRA_L, 0xB03000);
    botao_desenhar(jn, &b_ok);
    botao_desenhar(jn, &b_cancel);
    botao_desenhar(jn, &b_apl);
    if (seccoes[pagina].predet) botao_desenhar(jn, &b_pred);
    jan_mostrar(jn);
    ui_ev = ev_ant; ui_n = n_ant; ui_mudou = mud_ant;
}

static bool dentro(const evento_t *e, int x, int y, int l, int a)
{
    return e->x >= x && e->x < x + l && e->y >= y && e->y < y + a;
}

/* ¿Está el puntero sobre una lista de la sección? Entonces la rueda es de la lista, no del panel. */
static bool roda_en_lista(const evento_t *el)
{
    lista_t *ls[3] = { NULL, NULL, NULL };
    if (pagina == S_MANDOS) ls[0] = &lista_md;
    else if (pagina == S_USUARIOS) ls[0] = &lista_us;
    else if (pagina == S_VNC) ls[0] = &lista_vnc;
    else if (pagina == S_IA) { ls[0] = &lista_prov; ls[1] = &lista_mod; }
    else if (pagina == S_COLORES) ls[0] = &lista_img;
    for (int i = 0; i < 3; i++) if (ls[i] && dentro(el, ls[i]->x, ls[i]->y, ls[i]->l, ls[i]->a)) return true;
    return false;
}

static void aplicar_seccion(void)
{
    if (seccoes[pagina].aplicar) seccoes[pagina].aplicar();
    else dizer(_("Los cambios de esta sección ya están aplicados."));
}

/* Las flechas, Inicio y Fin son del árbol cuando él tiene el foco; lo que se teclea (letras, Intro...) va siempre a la sección,
 * para que una caja de texto reciba lo escrito aunque lo último pulsado fuera una fila del árbol (D367). */
static bool tecla_de_flechas(int t)
{
    return t == TECLA_CIMA || t == TECLA_BAIXO || t == TECLA_ESQUERDA || t == TECLA_DIREITA || t == TECLA_HOME || t == TECLA_END;
}

static int janela(int sec_inicial)
{
    int sw = 800, sh = 600;
    info_aparencia_t ap;
    memset(&ap, 0, sizeof ap);
    if (info_sistema(INFO_APARENCIA, &ap, sizeof ap) >= 0 && ap.largura && ap.altura) { sw = (int)ap.largura; sh = (int)ap.altura; }
    int ancho, alto, sec_guardada = -1;
    ui_ler(&ancho, &alto, &sec_guardada);
    int W = ancho > 0 ? ancho : (sw - 12 < 900 ? sw - 12 : 900);
    int H = alto > 0 ? alto : (sh - 100 < 640 ? sh - 100 : 640);
    if (W > sw - 8) W = sw - 8;
    if (H > sh - 60) H = sh - 60;
    if (W < LARG_MIN && sw - 8 >= LARG_MIN) W = LARG_MIN;
    if (H < ALT_MIN && sh - 60 >= ALT_MIN) H = ALT_MIN;
    jn = jan_abrir(_("Configuración"), W, H);
    if (!jn) { imprimir("configuracion: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    jan_estilo(jn, JAN_REDIMENSIONAVEL, LARG_MIN < W ? LARG_MIN : W, ALT_MIN < H ? ALT_MIN : H);
    conf_ler();
    conf0 = conf;
    {
        info_aparencia_t ap;
        memset(&ap, 0, sizeof ap);
        info_sistema(INFO_APARENCIA, &ap, sizeof ap);
        acento_ini = ap.acento;
        teclado_ini = teclado_layout(-1);
        volumen_ini = audio_volume_mestre(-1);
        silencio_ini = audio_silencio(-1) > 0;
    }
    persiste = home_persistente();
    for (int i = 0; i < S_QTD; i++) arv_aberto[i] = 1;
    traducir_rotulos();
    c_vnc_clave = (caixa_t){ .x = 24, .y = Y_PAG + 124, .l = 220, .oculta = true, .foco = true };
    lista_vnc = (lista_t){ 24, Y_PAG + 238, 200, 80, itens_vnc, 0, -1, 0, NULL };
    c_capturas = (caixa_t){ .x = XL, .y = 0, .l = 200, .foco = true };
    caixa_definir(&c_capturas, conf.capturas);
    lista_zona = (lista_t){ 24, Y_PAG + 34, 200, 100, itens_zona, 0, -1, 0, NULL };
    lista_modo = (lista_t){ 24, 40, 200, 100, itens_modo, 0, -1, 0, NULL };
    lista_img = (lista_t){ XL, 0, 200, 100, itens_img, 0, -1, 0, NULL };
    /* La página de la IA (D145): las seis cajas, las dos listas y los botones. */
    c_ia[1].oculta = true;
    lista_prov = (lista_t){ 24, Y_PAG + 24, 190, 220, itens_prov, 0, -1, 0, NULL };
    lista_mod = (lista_t){ 230, Y_PAG + 296, 300, 90, itens_mod, 0, -1, 0, NULL };
    layout();
    ler_zonas();
    ler_modos_video();
    listar_imagens();
    if (!persiste) dizer(_("Sin /home persistente los cambios se aplican, pero no se recordarán."));
    int sec = sec_inicial >= 0 ? sec_inicial : sec_guardada >= 0 ? sec_guardada : S_TELA;
    pagina = S_TELA;
    ir_seccion(sec);
    desenhar();
    registrar("configuracion: ventana abierta (persistente %d)", persiste);

    evento_t e;
    bool aceptar = false;
    for (;;) {
        int ms = seccoes[pagina].intervalo ? seccoes[pagina].intervalo() : 0;
        int hay = ms > 0 ? esperar_evento(jn->id, &e, ms) : proximo_evento(jn->id, &e, true);
        if (hay != 1) {
            if (seccoes[pagina].tique && seccoes[pagina].tique()) desenhar();
            continue;
        }
        if (e.tipo == EV_FECHAR) { aceptar = true; break; }   /* cerrar con la X = Aceptar: lo cambiado ya está aplicado */
        if (jan_redimensionar(jn, &e)) { layout(); desenhar(); continue; }
        if (e.tipo == EV_TELA) { ler_modos_video(); desenhar(); continue; }
        bool mudou = false;
        OX = OY = 0;
        /* el árbol (el teclado solo si lo tiene) */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            if (dentro(&e, arv.x, arv.y, arv.l, arv.a)) foco_arvore = true;
            else if (e.x >= PX && e.y >= PY && e.y < PY + PH) foco_arvore = false;
        }
        if (e.tipo == EV_TECLA && e.tecla == '\t') { foco_arvore = !foco_arvore; mudou = true; }
        else if (e.tipo != EV_TECLA || (foco_arvore && tecla_de_flechas(e.tecla))) {
            int r = arvore_tratar(jn, &arv, &e);
            if (r == ARVORE_ESCOLHEU) { if (arv.sel != pagina) { arv_aberto[arv.sel] = 1; ir_seccion(arv.sel); } mudou = true; }
            else if (r == ARVORE_ALTERNOU) mudou = true;
        }
        /* los botones de abajo */
        if (botao_tratar(jn, &b_ok, &e)) { aplicar_seccion(); aceptar = true; break; }
        if (botao_tratar(jn, &b_cancel, &e)) { deshacer_apertura(); break; }
        if (botao_tratar(jn, &b_apl, &e)) { aplicar_seccion(); mudou = true; }
        if (seccoes[pagina].predet && botao_tratar(jn, &b_pred, &e)) { seccoes[pagina].predet(); mudou = true; }
        /* la barra de desplazamiento */
        if (cont_h > PH) {
            int sx, sy, sh2, py, ph2;
            sb_geo(&sx, &sy, &sh2, &py, &ph2);
            static int sb_desl;
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.x >= sx && e.x < sx + 12 && e.y >= sy && e.y < sy + sh2) {
                if (e.y >= py && e.y < py + ph2) { sb_arrasta = true; sb_desl = e.y - py; }
                else { rolagem += e.y < py ? -PH * 3 / 4 : PH * 3 / 4; limitar_rolagem(); }
                mudou = true;
            } else if (e.tipo == EV_MOVER && sb_arrasta && (e.botoes & 1)) {
                int max = cont_h - PH, libre = PH - ph2;
                if (libre > 0) rolagem = (e.y - sb_desl - PY) * max / libre;
                limitar_rolagem();
                mudou = true;
            }
        }
        if (e.tipo == EV_SOLTOU) sb_arrasta = false;
        /* la sección, en coordenadas locales */
        {
            evento_t el = e;
            el.x -= PX - 16;
            el.y -= PY - rolagem;
            bool en_panel = e.x >= PX && e.x < PX + PW - 12 && e.y >= PY && e.y < PY + PH;
            bool pasa = (e.tipo == EV_CLIQUE || e.tipo == EV_RODA) ? en_panel : e.tipo == EV_TECLA ? !(foco_arvore && tecla_de_flechas(e.tecla)) : true;
            if (sb_arrasta) pasa = false;
            if (pasa) {
                OX = PX - 16; OY = PY - rolagem;
                if (seccoes[pagina].tratar(&el)) mudou = true;
                OX = OY = 0;
            }
            if (e.tipo == EV_RODA && en_panel && cont_h > PH && !roda_en_lista(&el)) {
                rolagem -= (int)e.cliques * RODA_LINHAS * 20;
                limitar_rolagem();
                mudou = true;
            }
        }
        if (mudou) desenhar();
    }
    (void)aceptar;
    ui_guardar();
    mic_cerrar();
    sair_seccion();
    jan_fechar(jn);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "--aplicar") == 0) return aplicar_tudo(argc, argv);
    if (argc >= 2 && compara_texto(argv[1], "--usuarios") == 0) return janela(S_USUARIOS);   /* D340 */
    if (argc >= 2 && compara_texto(argv[1], "--ventana") != 0 && compara_texto(argv[1], "--seccion") != 0) return linha_de_comandos(argc, argv);
    /* «--ventana [SECCION]» o «--seccion SECCION»: la ventana, abierta en esa sección (D367) */
    int sec = -1;
    if (argc >= 3) {
        sec = seccion_por_nombre(argv[2]);
        if (sec < 0) imprimir("configuracion: la sección «%s» no existe; abro la ventana normal\n", argv[2]);
    }
    return janela(sec);
}
