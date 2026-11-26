// SPDX-License-Identifier: GPL-3.0-only
/* sonidos - los sonidos del sistema (D421): toca el de un evento y vigila USB y batería.
 *
 *   sonidos                          estado: esquema, volumen y qué eventos suenan
 *   sonidos --listar                 los esquemas instalados y los eventos
 *   sonidos --tocar EVENTO [--max MS] [--siempre] [--esquema ID] [--volumen N] [--archivo RUTA]
 *                                    toca el sonido de EVENTO (con la configuración; --siempre ignora el interruptor
 *                                    y los eventos apagados: es el botón ▶ de Configuración)
 *   sonidos --vigilar [--arranque]   se queda vigilando: conectar/desconectar USB y batería baja; con --arranque
 *                                    toca antes el de arranque (es lo que hace el servicio /etc/servicios/sonidos.conf)
 *   sonidos --activar | --desactivar | --esquema ID | --volumen N | --evento ID si|no
 *                                    cambian la configuración (/home/.config/sonidos.conf)
 *   sonidos --asignar EVENTO RUTA|-|esquema   un sonido por acción (D484): archivo propio, «-» = ninguno, «esquema» = el del esquema
 *   sonidos --vol-evento EVENTO N     volumen de esa acción (0-100, multiplica al general)
 *   sonidos --presets                 los presets (esquemas de partida y propios)
 *   sonidos --preset-guardar N | --preset-cargar N | --preset-borrar N | --preset-renombrar N NUEVO
 *   sonidos --preset-exportar N ARCHIVO.sonidos | --preset-importar ARCHIVO.sonidos
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

static int tocar_evento(const char *evento, unsigned max_ms, bool siempre, const char *esquema, int volumen, const char *archivo)
{
    sonidos_conf_t c;
    sonidos_conf_ler(&c);
    int i = sonidos_evento_indice(evento);
    if (i < 0) { imprimir_em(ERROS, "sonidos: evento desconocido: %s\n", evento); return 1; }
    if (!siempre && (!c.activo || !c.ev[i])) return 0;
    char ruta[SON_RUTA_MAX];
    if (archivo) copia_texto(ruta, archivo, sizeof ruta);   /* «Probar» una opción aún sin guardar */
    else if (esquema) {   /* un esquema a la fuerza (compatibilidad): ignora las asignaciones */
        if (sonidos_ruta(esquema, evento, ruta, sizeof ruta) < 0) return 0;
    } else if (sonidos_ruta_accion(&c, i, ruta, sizeof ruta) < 0) return 0;
    if (volumen < 0) volumen = sonidos_volumen_accion(&c, i);
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

/* Ventanas (D484): se compara la lista del kernel cada 250 ms (solo si alguna acción de ventana está activada). */
#define VJ_MAX 64
typedef struct { int id; unsigned estado; } vj_t;
static vj_t vj_prev[VJ_MAX];
static int vj_n, vj_ok;

static void vigilar_ventanas(const sonidos_conf_t *c)
{
    static info_janela_t j[VJ_MAX];
    int n = info_sistema(INFO_JANELAS, j, VJ_MAX);
    if (n < 0) return;
    static vj_t ahora[VJ_MAX];
    int m = 0;
    for (int k = 0; k < n; k++) {
        if ((j[k].estado & JAN_EST_WIDGET) || !j[k].titulo[0]) continue;
        ahora[m].id = j[k].id;
        ahora[m++].estado = j[k].estado;
    }
    if (vj_ok) {   /* la primera lectura solo toma la foto */
        int abrir_ = 0, cerrar_ = 0, min_ = 0, max_ = 0;
        for (int k = 0; k < m; k++) {
            int p = -1;
            for (int q = 0; q < vj_n; q++) if (vj_prev[q].id == ahora[k].id) { p = q; break; }
            if (p < 0) { abrir_ = 1; continue; }
            unsigned d = ahora[k].estado ^ vj_prev[p].estado;
            if ((d & JAN_EST_MINIMIZADA) && (ahora[k].estado & JAN_EST_MINIMIZADA)) min_ = 1;
            if ((d & (JAN_EST_MAXIMIZADA | JAN_EST_TELA_CHEIA)) && (ahora[k].estado & (JAN_EST_MAXIMIZADA | JAN_EST_TELA_CHEIA))) max_ = 1;
        }
        for (int q = 0; q < vj_n; q++) {
            bool sigue = false;
            for (int k = 0; k < m; k++) if (ahora[k].id == vj_prev[q].id) sigue = true;
            if (!sigue) cerrar_ = 1;
        }
        if (abrir_ || cerrar_ || min_ || max_) registrar("sonidos: ventanas abrir=%d cerrar=%d minimizar=%d maximizar=%d", abrir_, cerrar_, min_, max_);
        if (cerrar_ && c->ev[sonidos_evento_indice("ventana_cerrar")]) sonido_evento("ventana_cerrar");
        else if (abrir_ && c->ev[sonidos_evento_indice("ventana_abrir")]) sonido_evento("ventana_abrir");
        if (min_) sonido_evento("minimizar");
        if (max_) sonido_evento("maximizar");
    }
    for (int k = 0; k < m; k++) vj_prev[k] = ahora[k];
    vj_n = m;
    vj_ok = 1;
}

static int vigilar(bool arranque)
{
    static sonidos_conf_t c;
    if (arranque) sonido_evento("arranque");
    int usb = -1, bat_aviso = 0;
    unsigned t_lento = 0;
    for (;;) {
        unsigned ahora = tempo_ms();
        if (!t_lento || ahora - t_lento >= 2000) {   /* configuración, USB y batería: cada 2 s */
            t_lento = ahora ? ahora : 1;
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
        }
        bool vent = c.activo && (c.ev[sonidos_evento_indice("ventana_abrir")] || c.ev[sonidos_evento_indice("ventana_cerrar")] ||
                                 c.ev[sonidos_evento_indice("minimizar")] || c.ev[sonidos_evento_indice("maximizar")]);
        if (vent) vigilar_ventanas(&c); else vj_ok = 0;
        dormir(vent ? 250 : 500);
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
    imprimir("Preset: %s\n", c.preset[0] ? c.preset : "(ninguno)");
    for (int i = 0; i < SON_EV_N; i++) {
        const char *a = c.asig[i];
        imprimir("  %-16s %-3s vol %3d  %s\n", sonidos_evento_id(i), c.ev[i] ? "si" : "no", c.vol[i],
                 !a[0] ? "(esquema)" : compara_texto(a, "-") == 0 ? "(ninguno)" : a);
    }
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
    if (compara_texto(o, "--asignar") == 0 && argc > 3) {
        int i = sonidos_evento_indice(argv[2]);
        if (i < 0) { imprimir_em(ERROS, "sonidos: evento desconocido: %s\n", argv[2]); return 1; }
        copia_texto(c.asig[i], compara_texto(argv[3], "esquema") == 0 ? "" : argv[3], sizeof c.asig[i]);
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--vol-evento") == 0 && argc > 3) {
        int i = sonidos_evento_indice(argv[2]);
        if (i < 0) { imprimir_em(ERROS, "sonidos: evento desconocido: %s\n", argv[2]); return 1; }
        int x = texto_para_int(argv[3]);
        c.vol[i] = (unsigned char)(x < 0 ? 0 : x > 100 ? 100 : x);
        return sonidos_conf_gravar(&c) < 0 ? 1 : 0;
    }
    if (compara_texto(o, "--ventanas") == 0) {   /* lo que ve el vigilante: id, estado y título */
        static info_janela_t jv[VJ_MAX];
        int n = info_sistema(INFO_JANELAS, jv, VJ_MAX);
        for (int i = 0; i < n; i++) imprimir("%d\t%u\t%s\n", jv[i].id, jv[i].estado, jv[i].titulo);
        return 0;
    }
    if (compara_texto(o, "--presets") == 0) {
        static sonidos_preset_t pv[48];
        int n = sonidos_presets(pv, 48);
        for (int i = 0; i < n; i++) imprimir("%s%s\n", pv[i].nombre, pv[i].esquema ? "\t(esquema)" : "");
        return 0;
    }
    if (comeca_com(o, "--preset-") && argc > 2) {
        int r;
        if (compara_texto(o, "--preset-guardar") == 0) { copia_texto(c.preset, argv[2], sizeof c.preset); r = sonidos_preset_guardar(argv[2], &c); if (r == 0) sonidos_conf_gravar(&c); }
        else if (compara_texto(o, "--preset-cargar") == 0) { r = sonidos_preset_cargar(argv[2], &c); if (r == 0) r = sonidos_conf_gravar(&c); }
        else if (compara_texto(o, "--preset-borrar") == 0) r = sonidos_preset_borrar(argv[2]);
        else if (compara_texto(o, "--preset-renombrar") == 0 && argc > 3) {
            r = sonidos_preset_renombrar(argv[2], argv[3]);
            if (r == 0 && compara_texto(c.preset, argv[2]) == 0) { copia_texto(c.preset, argv[3], sizeof c.preset); sonidos_conf_gravar(&c); }
        }
        else if (compara_texto(o, "--preset-exportar") == 0 && argc > 3) r = sonidos_preset_exportar(argv[2], argv[3]);
        else if (compara_texto(o, "--preset-importar") == 0) {
            char nm[40];
            r = sonidos_preset_importar(argv[2], nm, sizeof nm);
            if (r == 0) imprimir("%s\n", nm);
        } else { imprimir_em(ERROS, "sonidos: orden desconocida: %s\n", o); return 1; }
        if (r < 0) { imprimir_em(ERROS, "sonidos: no se pudo (%d)\n", r); return 1; }
        return 0;
    }
    if (compara_texto(o, "--vigilar") == 0) return vigilar(argc > 2 && compara_texto(argv[2], "--arranque") == 0);
    if (compara_texto(o, "--tocar") == 0 && argc > 2) {
        unsigned max = 0;
        bool siempre = false;
        const char *esq = NULL;
        int vol = -1;
        const char *arch = NULL;
        for (int i = 3; i < argc; i++) {
            if (compara_texto(argv[i], "--max") == 0 && i + 1 < argc) max = (unsigned)texto_para_int(argv[++i]);
            else if (compara_texto(argv[i], "--siempre") == 0) siempre = true;
            else if (compara_texto(argv[i], "--esquema") == 0 && i + 1 < argc) esq = argv[++i];
            else if (compara_texto(argv[i], "--volumen") == 0 && i + 1 < argc) vol = texto_para_int(argv[++i]);
            else if (compara_texto(argv[i], "--archivo") == 0 && i + 1 < argc) arch = argv[++i];
        }
        return tocar_evento(argv[2], max, siempre, esq, vol, arch);
    }
    imprimir_em(ERROS, "uso: sonidos [--presets | --asignar EV RUTA | --preset-guardar N | --listar | --tocar EVENTO | --vigilar | --activar | --desactivar | --esquema ID | --volumen N | --evento ID si|no]\n");
    return 1;
}
