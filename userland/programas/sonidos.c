// SPDX-License-Identifier: GPL-3.0-only
/* sonidos - los sonidos del sistema (D421): toca el de un evento y vigila USB y batería.
 *
 *   sonidos                          estado: esquema, volumen y qué eventos suenan
 *   sonidos --listar                 los esquemas instalados y los eventos
 *   sonidos --tocar EVENTO [--max MS] [--siempre] [--esquema ID] [--volumen N]
 *                                    toca el sonido de EVENTO (con la configuración; --siempre ignora el interruptor
 *                                    y los eventos apagados: es el botón ▶ de Configuración)
 *   sonidos --vigilar [--arranque]   se queda vigilando: conectar/desconectar USB y batería baja; con --arranque
 *                                    toca antes el de arranque (es lo que hace el servicio /etc/servicios/sonidos.conf)
 *   sonidos --activar | --desactivar | --esquema ID | --volumen N | --evento ID si|no
 *                                    cambian la configuración (/home/.config/sonidos.conf)
 *
 * Sin tarjeta de sonido, o con un archivo que no se pueda tocar, no hace nada (termina en silencio).
 * Reproduce con el mezclador del kernel y los decodificadores de la libsagui (WAV, MP3...), sin bloquear a nadie:
 * cada sonido es un proceso corto. */
#include "sagui.h"
#include "audio_formato.h"
#include "sonidos.h"

#define PED_QUADROS 1152

/* Toca el archivo hasta el final o hasta max_ms (0 = sin tope). 0 = tocado; 1 = no se pudo. */
static int tocar_arquivo(const char *ruta, int volumen, unsigned max_ms)
{
    int erro;
    audio_arquivo_t *arq = audio_arquivo_abrir(ruta, &erro);
    if (!arq) return 1;
    const audio_info_t *i = audio_arquivo_info(arq);
    uint32_t canais = i->canais;
    int a = audio_abrir((int)i->taxa, (int)canais, 16);
    if (a < 0) { audio_arquivo_fechar(arq); return 1; }   /* sin tarjeta: en silencio */
    audio_volume(a, volumen);
    static int16_t buf[PED_QUADROS * 2];
    unsigned t0 = tempo_ms();
    bool corte = false;
    int n;
    while ((n = audio_arquivo_ler(arq, buf, PED_QUADROS)) > 0) {
        const uint8_t *p = (const uint8_t *)buf;
        unsigned falta = (unsigned)n * canais * 2;
        while (falta) {
            int r = audio_escrever_sem_bloquear(a, p, falta);
            if (r < 0) { corte = true; break; }
            if (r == 0) dormir(10);
            p += r; falta -= (unsigned)r;
            if (max_ms && tempo_ms() - t0 >= max_ms) { corte = true; break; }
        }
        if (corte) break;
    }
    audio_fechar(a, corte ? 0 : 1);   /* con tope, calla al instante; si no, espera a que salga la última muestra */
    audio_arquivo_fechar(arq);
    return 0;
}

static int tocar_evento(const char *evento, unsigned max_ms, bool siempre, const char *esquema, int volumen)
{
    sonidos_conf_t c;
    sonidos_conf_ler(&c);
    int i = sonidos_evento_indice(evento);
    if (i < 0) { imprimir_em(ERROS, "sonidos: evento desconocido: %s\n", evento); return 1; }
    if (!siempre && (!c.activo || !c.ev[i])) return 0;
    if (!esquema) esquema = c.esquema;
    if (volumen < 0) volumen = c.volumen;
    char ruta[128];
    if (sonidos_ruta(esquema, evento, ruta, sizeof ruta) < 0) return 0;
    tocar_arquivo(ruta, volumen, max_ms);   /* un fallo se queda en silencio */
    return 0;
}

/* ---- vigilante ---- */

/* Los dispositivos USB conectados: líneas «Dispositivo N:» del informe de INFO_USB. Se compara una suma, no el texto. */
static int usb_cuantos(void)
{
    static char t[8192];
    int n = info_sistema(INFO_USB, t, sizeof t - 1);
    if (n <= 0) return -1;
    t[n] = 0;
    int k = 0;
    for (const char *p = t; *p;) {
        const char *fim = acha_char(p, '\n');
        if (comeca_com(p, "Dispositivo ")) k++;
        if (!fim) break;
        p = fim + 1;
    }
    return k;
}

static int vigilar(bool arranque)
{
    sonidos_conf_t c;
    if (arranque) sonido_evento("arranque");
    int usb = -1, bat_aviso = 0;
    for (;;) {
        sonidos_conf_ler(&c);
        if (c.activo) {
            if (c.ev[sonidos_evento_indice("usb_conectar")] || c.ev[sonidos_evento_indice("usb_desconectar")]) {
                int u = usb_cuantos();
                if (u >= 0) {
                    if (usb >= 0 && u > usb) sonido_evento("usb_conectar");
                    else if (usb >= 0 && u < usb) sonido_evento("usb_desconectar");
                    usb = u;
                }
            } else usb = -1;
            if (c.ev[sonidos_evento_indice("bateria")]) {
                info_bateria_t b;
                if (info_sistema(INFO_BATERIA, &b, sizeof b) == 0 && b.uacpi == UACPI_ATIVO && b.presente) {
                    bool baja = b.adaptador == 1 && b.porcentagem >= 0 && (b.porcentagem <= 10 || (b.estado & BATERIA_CRITICA));
                    if (baja && !bat_aviso) { sonido_evento("bateria"); bat_aviso = 1; }
                    else if (!baja && (b.adaptador == 2 || b.porcentagem > 15)) bat_aviso = 0;
                }
            }
        } else usb = -1;
        dormir(2000);
        int cod;
        while (recolher(&cod) > 0) { }
    }
}

static void estado(void)
{
    sonidos_conf_t c;
    int tem = sonidos_conf_ler(&c) == 0;
    imprimir("Sonidos del sistema: %s%s\n", c.activo ? "activados" : "desactivados", tem ? "" : " (de serie)");
    imprimir("Esquema: %s   Volumen: %d %%\n", c.esquema[0] ? c.esquema : "ninguno", c.volumen);
    for (int i = 0; i < SON_EV_N; i++) imprimir("  %-16s %s\n", sonidos_evento_id(i), c.ev[i] ? "si" : "no");
}

static void listar_esquemas(void)
{
    static sonidos_esquema_t v[16];
    int n = sonidos_esquemas(v, 16);
    imprimir("ninguno\n");
    for (int i = 0; i < n; i++) imprimir("%s\t%s\t%s%s\n", v[i].id, v[i].nombre, v[i].creditos, v[i].privado ? "\t(privado)" : "");
}

int principal(int argc, char **argv)
{
    sonidos_conf_t c;
    sonidos_conf_ler(&c);
    if (argc < 2) { estado(); return 0; }
    const char *o = argv[1];
    if (compara_texto(o, "--listar") == 0) { listar_esquemas(); return 0; }
    if (compara_texto(o, "--activar") == 0 || compara_texto(o, "--desactivar") == 0) {
        c.activo = o[2] == 'a' && o[3] == 'c';
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--esquema") == 0 && argc > 2) {
        copia_texto(c.esquema, argv[2], sizeof c.esquema);
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--volumen") == 0 && argc > 2) {
        int x = texto_para_int(argv[2]);
        c.volumen = x < 0 ? 0 : x > 100 ? 100 : x;
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--evento") == 0 && argc > 3) {
        int i = sonidos_evento_indice(argv[2]);
        if (i < 0) { imprimir_em(ERROS, "sonidos: evento desconocido: %s\n", argv[2]); return 1; }
        c.ev[i] = compara_texto(argv[3], "si") == 0 || compara_texto(argv[3], "on") == 0 || compara_texto(argv[3], "1") == 0;
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--vigilar") == 0) return vigilar(argc > 2 && compara_texto(argv[2], "--arranque") == 0);
    if (compara_texto(o, "--tocar") == 0 && argc > 2) {
        unsigned max = 0;
        bool siempre = false;
        const char *esq = NULL;
        int vol = -1;
        for (int i = 3; i < argc; i++) {
            if (compara_texto(argv[i], "--max") == 0 && i + 1 < argc) max = (unsigned)texto_para_int(argv[++i]);
            else if (compara_texto(argv[i], "--siempre") == 0) siempre = true;
            else if (compara_texto(argv[i], "--esquema") == 0 && i + 1 < argc) esq = argv[++i];
            else if (compara_texto(argv[i], "--volumen") == 0 && i + 1 < argc) vol = texto_para_int(argv[++i]);
        }
        return tocar_evento(argv[2], max, siempre, esq, vol);
    }
    imprimir_em(ERROS, "uso: sonidos [--listar | --tocar EVENTO | --vigilar | --activar | --desactivar | --esquema ID | --volumen N | --evento ID si|no]\n");
    return 1;
}
