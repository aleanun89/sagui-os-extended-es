// SPDX-License-Identifier: GPL-2.0-or-later
/* vncd - servidor de escritorio remoto VNC (mejora 097, D252).
 *
 * Deja ver y manejar la pantalla de SaguiOS desde el PC con un cliente VNC
 * normal (TightVNC, TigerVNC, RealVNC, UltraVNC, vncviewer...). Protocolo RFB
 * 3.8 (RFC 6143), con 3.7 y 3.3 por compatibilidad; código propio, sin portar
 * ningún servidor existente.
 *
 *   vncd              escucha (lo normal es como servicio: servicio activar vncd)
 *   vncd --clave [C]  fija la contraseña (hasta 8 caracteres; sin argumento la pide)
 *   vncd --ayuda
 *
 * Seguridad (se elige «VNC Authentication»: DES del reto con la contraseña
 * como clave, ver des.c; la opción «None» solo existe con seguridad=ninguna):
 *   - el servicio viene desactivado (arrancar=no);
 *   - por defecto solo acepta conexiones desde la propia máquina (127.0.0.1): se
 *     entra por un túnel, «ssh -L 5900:localhost:5900 root@IP» (D250), y el
 *     cliente VNC se conecta a localhost:5900; con escuchar=red-local acepta
 *     también la red local (la misma lógica que el sshd);
 *   - máximo de clientes a la vez, y una dirección que falla varias veces
 *     queda bloqueada un tiempo;
 *   - solo_ver=si: el cliente mira pero no toca (y es lo que ocurre solo si el
 *     kernel no deja inyectar entrada, INY_ESTADO);
 *   - cada conexión sale en pantalla (aviso) y en /logs/vncd.log.
 *
 * Pantalla: capturar_tela (D125), comparada por bloques de 16 x 16 con una
 * suma; solo se envía lo que cambió desde la última actualización. Codificaciones:
 * ZRLE (con un flujo zlib continuo, zflujo_*), Hextile y Raw, y la
 * pseudocodificación DesktopSize (cambios de resolución). Formatos de píxel de
 * 8, 16 y 32 bits de color verdadero. No hay CopyRect ni cursor remoto, y el
 * portapapeles del cliente (ClientCutText) se descarta.
 *
 * Procesos: el que escucha lanza un «vncd --sesion IP PUERTO» por conexión con
 * el socket como entrada (igual que el sshd).
 *
 * Configuración (clave=valor) en /etc/vnc/vncd.conf y /home/.config/vncd.conf
 * (gana la segunda): ver docs/vnc.md. */
#include "sagui.h"
#include "contas.h"
#include "servicos.h"
#include "des.h"
#include "compressao.h"
#include "sshd_logica.h"
#include "vnc_codec.h"

#define DIR_ETC       "/etc/vnc"
#define DIR_CASA      "/home/.config"
#define ARQ_CLAVE     "vncd.passwd"
#define REGISTRO_MAX  (256u * 1024u)
#define RX_TAM        65536u

#define SAIR_BEM      0
#define SAIR_SEM_AUT  10
#define SAIR_ERRO     11

#define ENC_RAW       0
#define ENC_HEXTILE   5
#define ENC_ZRLE      16
#define ENC_TAMANO    (-223)   /* DesktopSize */

/* ------------------------------------------------------------ configuración */

static struct {
    int  porta;
    bool red_local;       /* escuchar=red-local */
    bool solo_ver;
    bool sin_clave;       /* seguridad=ninguna */
    int  max_clientes;
    int  falhas_bloqueio;
    int  bloqueio_s;
    int  graca_s;         /* segundos para autenticarse */
    int  fps;             /* actualizaciones por segundo como mucho */
    bool aviso;
} conf = { 5900, false, false, false, 2, 3, 60, 30, 12, true };

static bool valor_sim(const char *v)
{
    return compara_texto(v, "si") == 0 || compara_texto(v, "1") == 0 || compara_texto(v, "yes") == 0;
}

static void conf_ler(const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return;
    static leitor_linhas_t l;
    char linha[200];
    leitor_iniciar(&l, fd, false);
    while (leitor_linha(&l, linha, sizeof linha, NULL) >= 0) {
        char *p = linha;
        while (eh_espaco(*p)) p++;
        if (!*p || *p == '#') continue;
        char *v = acha_char(p, '=');
        if (!v) continue;
        *v++ = 0;
        char *f = p + tam_texto(p);
        while (f > p && eh_espaco(f[-1])) *--f = 0;
        while (eh_espaco(*v)) v++;
        f = v + tam_texto(v);
        while (f > v && eh_espaco(f[-1])) *--f = 0;
        int n = texto_para_int(v);
        if (compara_texto(p, "puerto") == 0 && n > 0 && n < 65536) conf.porta = n;
        else if (compara_texto(p, "escuchar") == 0) conf.red_local = compara_texto(v, "red-local") == 0;
        else if (compara_texto(p, "solo_ver") == 0) conf.solo_ver = valor_sim(v);
        else if (compara_texto(p, "seguridad") == 0) conf.sin_clave = compara_texto(v, "ninguna") == 0;
        else if (compara_texto(p, "max_clientes") == 0 && n > 0 && n <= 4) conf.max_clientes = n;
        else if (compara_texto(p, "bloqueo_fallos") == 0 && n > 0 && n <= 100) conf.falhas_bloqueio = n;
        else if (compara_texto(p, "bloqueo_segundos") == 0 && n >= 0 && n <= 86400) conf.bloqueio_s = n;
        else if (compara_texto(p, "tiempo_entrada") == 0 && n >= 5 && n <= 600) conf.graca_s = n;
        else if (compara_texto(p, "fps_max") == 0 && n >= 1 && n <= 30) conf.fps = n;
        else if (compara_texto(p, "aviso") == 0) conf.aviso = valor_sim(v);
    }
    fechar(fd);
}

static void conf_carregar(void)
{
    conf_ler(DIR_ETC "/vncd.conf");
    conf_ler(DIR_CASA "/vncd.conf");
}

/* ------------------------------------------------------------ registro */

static char quem[40];

static void anotar(const char *fmt, ...)
{
    char texto[400], linha[480], hora[32], arq[96];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(texto, sizeof texto, fmt, ap);
    va_end(ap);
    for (char *p = texto; *p; p++)
        if ((unsigned char)*p < 0x20) *p = '?';
    fuso_data_t d;
    agora_local(&d);
    formatar_data(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &d);
    formatar_texto(linha, sizeof linha, "%s [%d]%s%s %s\n", hora, meu_pid(), quem[0] ? " " : "", quem, texto);
    const char *pasta = servicos_pasta_registros();
    servicos_criar_pastas(pasta);
    formatar_texto(arq, sizeof arq, "%s/vncd.log", pasta);
    info_arquivo_t i;
    if (info_arquivo(arq, &i) == 0 && i.tamanho > REGISTRO_MAX) {
        char velho[112];
        formatar_texto(velho, sizeof velho, "%s.1", arq);
        apagar(velho);
        renomear(arq, velho);
    }
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd >= 0) {
        escrever_tudo(fd, linha, (unsigned)tam_texto(linha));
        fechar(fd);
    }
    registrar("vncd:%s%s %s", quem[0] ? " " : "", quem, texto);
}

/* ------------------------------------------------------------ contraseña */

static char clave[72];

/* Lee la contraseña (primera línea) de un archivo; false si no existe o está vacía. */
static bool clave_ler(const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return false;
    char b[72];
    int n = ler(fd, b, sizeof b - 1);
    fechar(fd);
    if (n <= 0) return false;
    b[n] = 0;
    for (int i = 0; i < n; i++) if (b[i] == '\n' || b[i] == '\r') { b[i] = 0; break; }
    if (!b[0]) return false;
    copia_texto(clave, b, sizeof clave);
    return true;
}

/* D255: primero /etc/vnc (que viene de SAGUI-CONF, solo root); la de
 * /home/.config (donde iba antes) se pasa a SAGUI-CONF la primera vez. */
static bool clave_cargar(void)
{
    if (clave_ler(DIR_ETC "/" ARQ_CLAVE)) return true;
    if (!clave_ler(DIR_CASA "/" ARQ_CLAVE)) return false;
    if (sou_root() && conf_persistente()) {
        char t[80];
        formatar_texto(t, sizeof t, "%s\n", clave);
        if (conf_gravar("vnc/" ARQ_CLAVE, t, (int)tam_texto(t)) == 0)
            registrar("vncd: contraseña pasada de " DIR_CASA " a SAGUI-CONF (" DIR_ETC ")");
        memset(t, 0, sizeof t);
    }
    return true;
}

static int clave_fijar(const char *c)
{
    if (!c[0]) { imprimir_em(ERROS, "vncd: la contraseña no puede estar vacía\n"); return 2; }
    if (falta_admin("vncd")) return 1;   /* D255: la contraseña del escritorio remoto es del sistema */
    if (conf_persistente()) {
        char t[80];
        formatar_texto(t, sizeof t, "%s\n", c);
        int r = conf_gravar("vnc/" ARQ_CLAVE, t, (int)tam_texto(t));
        memset(t, 0, sizeof t);
        if (r < 0) { imprimir_em(ERROS, "vncd: no se puede guardar la contraseña: %s\n", nome_erro(r)); return 1; }
        apagar(DIR_CASA "/" ARQ_CLAVE);   /* la vieja, si la había */
        imprimir("vncd: contraseña guardada en la configuración del sistema (SAGUI-CONF)%s\n",
                 tam_texto(c) > 8 ? " (el protocolo VNC solo usa los 8 primeros caracteres)" : "");
        return 0;
    }
    criar_pasta("/home/.config");
    int fd = abrir(DIR_CASA "/" ARQ_CLAVE, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { imprimir_em(ERROS, "vncd: no se puede escribir " DIR_CASA "/" ARQ_CLAVE ": %s\n", nome_erro(fd)); return 1; }
    char t[80];
    formatar_texto(t, sizeof t, "%s\n", c);
    escrever_tudo(fd, t, (unsigned)tam_texto(t));
    fechar(fd);
    imprimir("vncd: contraseña guardada en " DIR_CASA "/" ARQ_CLAVE "%s\n",
             tam_texto(c) > 8 ? " (el protocolo VNC solo usa los 8 primeros caracteres)" : "");
    if (!servicos_montado("/home")) imprimir("aviso: /home no es persistente: la contraseña se perderá al reiniciar\n");
    return 0;
}

/* ------------------------------------------------------------ la sesión */

static int sk;
static uint8_t *rx;
static unsigned rx_ini, rx_fim;
static unsigned saltar;             /* bytes por descartar (portapapeles del cliente) */
static char cliente_ip[16];
static int  cliente_porta;

static void terminar(int codigo) { sair(codigo); }

static void fatal(const char *motivo)
{
    anotar("conexión cerrada: %s", motivo);
    terminar(SAIR_ERRO);
}

static void enviar(const void *d, unsigned n)
{
    if (escrever_tudo(sk, d, n) < 0) {
        anotar("el cliente cerró la conexión (escritura)");
        terminar(SAIR_BEM);
    }
}

static void enviar_u32(uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    enviar(b, 4);
}

/* Lee exactamente n bytes, esperando como mucho ms en total (apretón de manos). */
static bool leer_exacto(void *dst, unsigned n, unsigned ms)
{
    uint8_t *d = dst;
    unsigned t0 = tempo_ms();
    while (n) {
        unsigned gastado = tempo_ms() - t0;
        if (gastado >= ms) return false;
        tempo_limite(sk, ms - gastado);
        int r = ler(sk, d, n);
        if (r <= 0) return false;
        d += r; n -= (unsigned)r;
    }
    return true;
}

static void enviar_fallo(int ver, const char *motivo)
{
    if (ver >= 8 || ver == 0) { enviar_u32(1); enviar_u32((uint32_t)tam_texto(motivo)); enviar(motivo, (unsigned)tam_texto(motivo)); }
    else enviar_u32(1);
}

/* El apretón de manos: versión y seguridad. false = no pasó (ya contestado). */
static bool saludo(int *ver_out)
{
    enviar("RFB 003.008\n", 12);
    char v[12];
    if (!leer_exacto(v, 12, (unsigned)conf.graca_s * 1000)) fatal("el cliente no mandó su versión");
    if (memcmp(v, "RFB 003.", 8) != 0 || v[11] != '\n' || v[8] < '0' || v[8] > '9' || v[9] < '0' || v[9] > '9' || v[10] < '0' || v[10] > '9') {
        fatal("no es un cliente RFB");
    }
    int menor = (v[8] - '0') * 100 + (v[9] - '0') * 10 + (v[10] - '0');
    int ver = menor >= 8 ? 8 : menor >= 7 ? 7 : 3;
    anotar("conexión; cliente RFB 3.%d", menor);
    *ver_out = ver;
    bool con_clave = !conf.sin_clave;
    if (con_clave && !clave[0]) {
        const char *m = "el servidor no tiene contraseña (vncd --clave)";
        anotar("rechazada: %s", m);
        if (ver == 3) { enviar_u32(0); enviar_u32((uint32_t)tam_texto(m)); enviar(m, (unsigned)tam_texto(m)); }
        else { uint8_t z = 0; enviar(&z, 1); enviar_u32((uint32_t)tam_texto(m)); enviar(m, (unsigned)tam_texto(m)); }
        return false;
    }
    int tipo = con_clave ? 2 : 1;
    if (ver == 3) enviar_u32((uint32_t)tipo);
    else {
        uint8_t l[2] = { 1, (uint8_t)tipo };
        enviar(l, 2);
        uint8_t eleg;
        if (!leer_exacto(&eleg, 1, (unsigned)conf.graca_s * 1000)) fatal("el cliente no eligió seguridad");
        if (eleg != tipo) {
            enviar_fallo(8, "tipo de seguridad no admitido");
            return false;
        }
    }
    if (tipo == 1) {
        if (ver >= 8) enviar_u32(0);
        return true;
    }
    uint8_t reto[16], resp[16], esperada[16];
    bytes_aleatorios(reto, 16);
    enviar(reto, 16);
    if (!leer_exacto(resp, 16, (unsigned)conf.graca_s * 1000)) fatal("el cliente no respondió al reto");
    vnc_respuesta(clave, reto, esperada);
    uint8_t dif = 0;
    for (int i = 0; i < 16; i++) dif |= (uint8_t)(resp[i] ^ esperada[i]);
    if (dif) {
        anotar("contraseña incorrecta");
        dormir(1000);   /* frena la fuerza bruta */
        enviar_fallo(ver, "Autenticación fallida");
        return false;
    }
    enviar_u32(0);
    return true;
}

/* ------------------------------------------------------------ estado del cliente */

static vnc_pf_t pf;
static int enc_pref = ENC_RAW;       /* la mejor de las que el cliente ofrece */
static bool cl_tamano;               /* el cliente admite DesktopSize */
static int  mods;                    /* INY_MOD_* de los modificadores pulsados */
static bool puede_inyectar;
static int  ult_botones = 0, ult_x = -1, ult_y = -1;

static int W, H;                     /* tamaño que ve el cliente */
static uint32_t *cap, *vista;        /* captura (cw x ch) y su copia recortada si el tamaño no coincide */
static unsigned cap_max;
static int cw, ch;
static uint32_t *hprev;
static uint8_t  *sucio, *usado_tmp;
static int bw, bh;
static uint32_t *vbuf;               /* píxeles convertidos al formato del cliente */
static uint8_t  *obuf, *zbuf;
static zflujo_t *zf;
static unsigned long total_bytes, total_act, total_rects;

static bool pedido;
static int  p_x, p_y, p_w, p_h;
static bool p_incremental;

static void marcar_region(int x, int y, int w, int h)
{
    int bx0 = x / VNC_BLOQUE, by0 = y / VNC_BLOQUE;
    int bx1 = (x + w + VNC_BLOQUE - 1) / VNC_BLOQUE, by1 = (y + h + VNC_BLOQUE - 1) / VNC_BLOQUE;
    if (bx1 > bw) bx1 = bw;
    if (by1 > bh) by1 = bh;
    for (int by = by0; by < by1; by++) for (int bx = bx0; bx < bx1; bx++) sucio[by * bw + bx] = 1;
}

static bool reservar(void)
{
    libera(hprev); libera(sucio); libera(usado_tmp); libera(vbuf); libera(obuf); libera(zbuf); libera(vista);
    bw = (W + VNC_BLOQUE - 1) / VNC_BLOQUE;
    bh = (H + VNC_BLOQUE - 1) / VNC_BLOQUE;
    size_t px = (size_t)W * H;
    hprev = aloca((size_t)bw * bh * 4);
    sucio = aloca((size_t)bw * bh);
    usado_tmp = aloca((size_t)bw * bh);
    vbuf = aloca(px * 4);
    obuf = aloca((size_t)vnc_limite(W, H));
    zbuf = aloca((size_t)zflujo_limite(vnc_limite(W, H)) + 16);
    vista = NULL;
    if (!hprev || !sucio || !usado_tmp || !vbuf || !obuf || !zbuf) return false;
    memset(hprev, 0, (size_t)bw * bh * 4);
    memset(sucio, 1, (size_t)bw * bh);
    return true;
}

/* Captura la pantalla y marca los bloques que cambiaron; devuelve false si no se pudo. */
static bool capturar(bool *cambio_tamano)
{
    int rect[4] = { 0, 0, 0, 0 };
    int n = capturar_tela(0, rect, cap, cap_max);
    *cambio_tamano = false;
    if (n < 0 || rect[2] <= 0 || rect[3] <= 0) return false;
    cw = rect[2]; ch = rect[3];
    if (cw != W || ch != H) {
        if (cl_tamano) { *cambio_tamano = true; return true; }
        /* el cliente no admite DesktopSize: se le sigue dando su tamaño, recortado o con relleno negro */
        if (!vista) { vista = aloca((size_t)W * H * 4); if (!vista) return false; }
        memset(vista, 0, (size_t)W * H * 4);
        for (int y = 0; y < H && y < ch; y++) memcpy(vista + (long)y * W, cap + (long)y * cw, (size_t)(cw < W ? cw : W) * 4);
    }
    const uint32_t *img = (cw == W && ch == H) ? cap : vista;
    for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
        uint32_t h = vnc_hash_bloque(img, W, bx, by, H);
        if (h != hprev[by * bw + bx]) { hprev[by * bw + bx] = h; sucio[by * bw + bx] = 1; }
    }
    return true;
}

static void enviar_tamano(void)
{
    uint8_t m[4 + 12] = { 0, 0, 0, 1 };
    uint8_t *o = m + 4;
    *o++ = 0; *o++ = 0; *o++ = 0; *o++ = 0;
    *o++ = (uint8_t)(cw >> 8); *o++ = (uint8_t)cw; *o++ = (uint8_t)(ch >> 8); *o++ = (uint8_t)ch;
    uint32_t e = (uint32_t)ENC_TAMANO;
    *o++ = (uint8_t)(e >> 24); *o++ = (uint8_t)(e >> 16); *o++ = (uint8_t)(e >> 8); *o++ = (uint8_t)e;
    enviar(m, sizeof m);
    W = cw; H = ch;
    if (!reservar()) fatal("sin memoria al cambiar de resolución");
    anotar("resolución nueva: %dx%d", W, H);
}

/* Envía lo sucio dentro del pedido. Devuelve true si mandó algo. */
static bool enviar_actualizacion(void)
{
    static vnc_rect_t rs[256];
    static uint8_t sel[1 << 14];
    int nb = bw * bh;
    if ((unsigned)nb > sizeof sel) nb = (int)sizeof sel;
    memset(sel, 0, sizeof sel);
    int bx0 = p_x / VNC_BLOQUE, by0 = p_y / VNC_BLOQUE;
    int bx1 = (p_x + p_w + VNC_BLOQUE - 1) / VNC_BLOQUE, by1 = (p_y + p_h + VNC_BLOQUE - 1) / VNC_BLOQUE;
    if (bx1 > bw) bx1 = bw;
    if (by1 > bh) by1 = bh;
    bool algo = false;
    if ((unsigned)(bw * bh) > sizeof sel) return false;   /* pantallas de más de 16384 bloques (p. ej. 4096x1024): no admitidas */
    for (int by = by0; by < by1; by++) for (int bx = bx0; bx < bx1; bx++)
        if (sucio[by * bw + bx]) { sel[by * bw + bx] = 1; algo = true; }
    if (!algo) return false;
    int nr = vnc_agrupar(sel, usado_tmp, bw, bh, W, H, rs, 256);
    if (nr <= 0) return false;
    for (int by = by0; by < by1; by++) for (int bx = bx0; bx < bx1; bx++) sucio[by * bw + bx] = 0;
    const uint32_t *img = (cw == W && ch == H) ? cap : vista;
    uint8_t cab[4] = { 0, 0, (uint8_t)(nr >> 8), (uint8_t)nr };
    enviar(cab, 4);
    for (int i = 0; i < nr; i++) {
        vnc_rect_t *r = &rs[i];
        vnc_convertir(&pf, img + (long)r->y * W + r->x, W, r->w, r->h, vbuf);
        int n, enc = enc_pref;
        if (enc == ENC_ZRLE) n = vnc_zrle_datos(&pf, vbuf, r->w, r->h, obuf);
        else if (enc == ENC_HEXTILE) n = vnc_hextile(&pf, vbuf, r->w, r->h, obuf);
        else n = vnc_raw(&pf, vbuf, r->w, r->h, obuf);
        uint8_t h[12] = { (uint8_t)(r->x >> 8), (uint8_t)r->x, (uint8_t)(r->y >> 8), (uint8_t)r->y,
                          (uint8_t)(r->w >> 8), (uint8_t)r->w, (uint8_t)(r->h >> 8), (uint8_t)r->h,
                          0, 0, 0, (uint8_t)enc };
        enviar(h, 12);
        if (enc == ENC_ZRLE) {
            int z = zflujo_comprimir(zf, obuf, n, zbuf, zflujo_limite(vnc_limite(W, H)));
            if (z < 0) fatal("fallo del compresor ZRLE");
            enviar_u32((uint32_t)z);
            enviar(zbuf, (unsigned)z);
            total_bytes += (unsigned)z + 4;
        } else {
            enviar(obuf, (unsigned)n);
            total_bytes += (unsigned)n;
        }
        total_bytes += 12;
    }
    total_bytes += 4;
    total_act++;
    total_rects += (unsigned)nr;
    return true;
}

/* ------------------------------------------------------------ mensajes del cliente */

/* El kernel solo da el permiso de inyectar al proceso que lanzó el gestor de
 * servicios (D251), es decir, al que escucha, no a las sesiones (hijas suyas).
 * Cada sesión le manda por su salida estándar (una tubería) mensajes de 12
 * bytes (op, a, b) y el que escucha llama a inyectar_entrada. */
static void emitir(int op, int a, int b)
{
    int m[3] = { op, a, b };
    if (escrever_tudo(SAIDA, m, sizeof m) < 0) puede_inyectar = false;
}

static void inyectar_tecla(uint32_t ks, bool abajo)
{
    int m = vnc_modificador(ks);
    if (m) {
        if (abajo) mods |= m; else mods &= ~m;
        if (puede_inyectar && !conf.solo_ver) emitir(INY_TECLA, 0, mods);   /* solo cambian los modificadores */
        return;
    }
    if (!puede_inyectar || conf.solo_ver) return;
    int c = vnc_tecla(ks);
    if (!c) return;
    emitir(INY_TECLA, c, (abajo ? INY_PULSADA : 0) | mods);
}

static void inyectar_puntero(int botones, int x, int y)
{
    if (!puede_inyectar || conf.solo_ver) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    int b = 0;
    if (botones & 1) b |= INY_BOTON_IZQ;
    if (botones & 2) b |= INY_BOTON_MEDIO;
    if (botones & 4) b |= INY_BOTON_DER;
    if (b != ult_botones || x != ult_x || y != ult_y) emitir(INY_RATON, (x << 16) | (y & 0xFFFF), b);
    if ((botones & 8) && !(ult_botones & 8)) emitir(INY_RUEDA, -1, 0);
    if ((botones & 16) && !(ult_botones & 16)) emitir(INY_RUEDA, 1, 0);
    ult_botones = (b) | (botones & 24);
    ult_x = x; ult_y = y;
}

static unsigned be16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* Procesa lo que haya en el búfer. Devuelve true si hizo algo. */
static bool mensajes(void)
{
    bool hizo = false;
    for (;;) {
        if (saltar) {
            unsigned t = rx_fim - rx_ini;
            if (!t) break;
            unsigned c = t < saltar ? t : saltar;
            rx_ini += c; saltar -= c; hizo = true;
            continue;
        }
        unsigned t = rx_fim - rx_ini;
        if (!t) break;
        const uint8_t *p = rx + rx_ini;
        unsigned need;
        switch (p[0]) {
        case 0: need = 20; break;
        case 2: need = 4; if (t >= 4) need = 4 + 4 * be16(p + 2); break;
        case 3: need = 10; break;
        case 4: need = 8; break;
        case 5: need = 6; break;
        case 6: need = 8; break;
        default: fatal("mensaje de cliente desconocido"); need = 0;
        }
        if (need > RX_TAM - 64) fatal("mensaje demasiado grande");
        if (t < need) break;
        hizo = true;
        switch (p[0]) {
        case 0: {   /* SetPixelFormat */
            vnc_pf_t n;
            memset(&n, 0, sizeof n);
            n.bpp = p[4]; n.profundidad = p[5]; n.big = p[6]; n.color_verdadero = p[7];
            n.max[0] = (uint16_t)be16(p + 8); n.max[1] = (uint16_t)be16(p + 10); n.max[2] = (uint16_t)be16(p + 12);
            n.desp[0] = p[14]; n.desp[1] = p[15]; n.desp[2] = p[16];
            if (!vnc_pf_preparar(&n)) fatal("formato de píxel no admitido (solo color verdadero de 8, 16 o 32 bits)");
            pf = n;
            marcar_region(0, 0, W, H);
            break;
        }
        case 2: {   /* SetEncodings */
            unsigned n = be16(p + 2);
            int mejor = ENC_RAW;
            bool hay = false;
            cl_tamano = false;
            for (unsigned i = 0; i < n; i++) {
                int32_t e = (int32_t)be32(p + 4 + 4 * i);
                if (e == ENC_TAMANO) cl_tamano = true;
                if (!hay && (e == ENC_ZRLE || e == ENC_HEXTILE || e == ENC_RAW)) { mejor = e; hay = true; }
            }
            enc_pref = mejor;
            break;
        }
        case 3: {   /* FramebufferUpdateRequest */
            pedido = true;
            p_incremental = p[1] != 0;
            p_x = (int)be16(p + 2); p_y = (int)be16(p + 4); p_w = (int)be16(p + 6); p_h = (int)be16(p + 8);
            if (p_x > W) p_x = W;
            if (p_y > H) p_y = H;
            if (p_x + p_w > W) p_w = W - p_x;
            if (p_y + p_h > H) p_h = H - p_y;
            if (!p_incremental) marcar_region(p_x, p_y, p_w, p_h);
            break;
        }
        case 4: inyectar_tecla(be32(p + 4), p[1] != 0); break;
        case 5: inyectar_puntero(p[1], (int)be16(p + 2), (int)be16(p + 4)); break;
        case 6: saltar = be32(p + 4); break;   /* ClientCutText: se descarta */
        }
        rx_ini += need;
    }
    return hizo;
}

static int sesion(const char *ip, const char *porta, bool inyecta)
{
    copia_texto(cliente_ip, ip, sizeof cliente_ip);
    cliente_porta = texto_para_int(porta);
    formatar_texto(quem, sizeof quem, "%s:%d", cliente_ip, cliente_porta);
    sk = duplicar(ENTRADA, 10);
    if (sk < 0) return SAIR_ERRO;
    int pp[2];
    if (criar_pipe(pp) == 0) { duplicar(pp[0], ENTRADA); fechar(pp[0]); fechar(pp[1]); }
    conf_carregar();
    clave_cargar();
    rx = aloca(RX_TAM);
    if (!rx) { anotar("sin memoria"); return SAIR_ERRO; }

    int ver;
    if (!saludo(&ver)) return SAIR_SEM_AUT;

    uint8_t compartir;
    if (!leer_exacto(&compartir, 1, 30000)) fatal("el cliente no mandó ClientInit");

    puede_inyectar = inyecta;
    if (!puede_inyectar) anotar("no hay permiso para inyectar entrada (hay que arrancar como servicio): sesión de solo ver");
    else if (conf.solo_ver) anotar("solo_ver=si: el cliente no puede teclear ni usar el ratón");

    cap_max = 1920u * 1200u;
    cap = aloca((size_t)cap_max * 4);
    zf = aloca(sizeof *zf);
    if (!cap || !zf) fatal("sin memoria");
    zflujo_iniciar(zf);
    int rect[4] = { 0, 0, 0, 0 };
    if (capturar_tela(0, rect, cap, cap_max) < 0 || rect[2] <= 0) fatal("no se puede capturar la pantalla");
    W = cw = rect[2]; H = ch = rect[3];
    vnc_pf_nativo(&pf);
    vnc_pf_preparar(&pf);
    if (!reservar()) fatal("sin memoria");

    static const char NOMBRE[] = "SaguiOS";
    uint8_t si[24];
    si[0] = (uint8_t)(W >> 8); si[1] = (uint8_t)W; si[2] = (uint8_t)(H >> 8); si[3] = (uint8_t)H;
    si[4] = pf.bpp; si[5] = pf.profundidad; si[6] = pf.big; si[7] = pf.color_verdadero;
    si[8] = (uint8_t)(pf.max[0] >> 8); si[9] = (uint8_t)pf.max[0]; si[10] = (uint8_t)(pf.max[1] >> 8); si[11] = (uint8_t)pf.max[1];
    si[12] = (uint8_t)(pf.max[2] >> 8); si[13] = (uint8_t)pf.max[2];
    si[14] = pf.desp[0]; si[15] = pf.desp[1]; si[16] = pf.desp[2]; si[17] = si[18] = si[19] = 0;
    si[20] = 0; si[21] = 0; si[22] = 0; si[23] = (uint8_t)(sizeof NOMBRE - 1);
    enviar(si, 24);
    enviar(NOMBRE, sizeof NOMBRE - 1);
    anotar("autenticado; pantalla %dx%d%s", W, H, (conf.solo_ver || !puede_inyectar) ? " (solo ver)" : "");
    if (conf.aviso) {
        char t[80];
        formatar_texto(t, sizeof t, "VNC: %s conectado desde %s", (conf.solo_ver || !puede_inyectar) ? "cliente (solo ver)" : "cliente", cliente_ip);
        aviso_tela(t, NULL);
    }

    unsigned intervalo = 1000u / (unsigned)conf.fps, espera = intervalo, ultima = 0;
    unsigned visto = (unsigned)esperar_rede(0, 0);
    unsigned t_ini = tempo_ms();
    for (;;) {
        bool fez = false;
        int d = disponivel(sk);
        if (d < 0) break;
        if (d > 0) {
            if (rx_ini == rx_fim) rx_ini = rx_fim = 0;
            else if (rx_ini && RX_TAM - rx_fim < 4096) { memmove(rx, rx + rx_ini, rx_fim - rx_ini); rx_fim -= rx_ini; rx_ini = 0; }
            if (rx_fim < RX_TAM) {
                tempo_limite(sk, 1000);
                int n = ler(sk, rx + rx_fim, RX_TAM - rx_fim);
                if (n <= 0) break;
                rx_fim += (unsigned)n;
                fez = true;
            }
        }
        if (mensajes()) { fez = true; espera = intervalo; }
        unsigned ahora = tempo_ms();
        if (pedido && ahora - ultima >= espera) {
            bool tam;
            ultima = ahora;
            if (capturar(&tam)) {
                if (tam) { enviar_tamano(); pedido = false; espera = intervalo; fez = true; }
                else if (enviar_actualizacion()) { pedido = false; espera = intervalo; fez = true; }
                else espera = espera < 250 ? espera + 25 : 250;   /* pantalla quieta: se mira menos */
            }
        }
        if (fez) hilo_ceder();
        else visto = (unsigned)esperar_rede(visto, 10);
    }
    if (puede_inyectar && !conf.solo_ver) { emitir(INY_TECLA, 0, 0); emitir(INY_RATON, (ult_x < 0 ? 0 : ult_x) << 16 | (ult_y < 0 ? 0 : ult_y), 0); }
    unsigned seg = (tempo_ms() - t_ini) / 1000;
    anotar("el cliente cerró la conexión tras %u s: %lu actualizaciones, %lu rectángulos, %lu KB enviados", seg, total_act, total_rects, total_bytes / 1024);
    return SAIR_BEM;
}

/* ------------------------------------------------------------ el proceso que escucha */

#define MAX_FILHOS 8
#define MAX_FALHAS 16

static struct { int pid; uint32_t ip; int tubo; } filhos[MAX_FILHOS];   /* tubo: lo que la sesión pide inyectar */
static bool inyecta_ok;
static struct { uint32_t ip; int falhas; unsigned ate; bool bloqueado; } falhas[MAX_FALHAS];

static int n_filhos(void)
{
    int n = 0;
    for (int i = 0; i < MAX_FILHOS; i++) n += filhos[i].pid != 0;
    return n;
}

static void anotar_falha(uint32_t ip, bool falhou)
{
    int livre = -1, velho = 0;
    char t[16];
    for (int i = 0; i < MAX_FALHAS; i++) {
        if (falhas[i].ip == ip) {
            if (!falhou) { falhas[i].ip = 0; return; }
            if (++falhas[i].falhas >= conf.falhas_bloqueio && conf.bloqueio_s) {
                falhas[i].bloqueado = true;
                falhas[i].ate = tempo_ms() + (unsigned)conf.bloqueio_s * 1000;
                anotar("%s bloqueada %d s: %d conexiones seguidas sin autenticarse", ip_para_texto(ip, t), conf.bloqueio_s, falhas[i].falhas);
            }
            return;
        }
        if (!falhas[i].ip && livre < 0) livre = i;
        if (falhas[i].ate < falhas[velho].ate) velho = i;
    }
    if (!falhou) return;
    if (livre < 0) livre = velho;
    falhas[livre].ip = ip;
    falhas[livre].falhas = 1;
    falhas[livre].bloqueado = false;
    falhas[livre].ate = tempo_ms();
    if (conf.falhas_bloqueio <= 1 && conf.bloqueio_s) {
        falhas[livre].bloqueado = true;
        falhas[livre].ate = tempo_ms() + (unsigned)conf.bloqueio_s * 1000;
    }
}

static bool bloqueada(uint32_t ip)
{
    for (int i = 0; i < MAX_FALHAS; i++) {
        if (falhas[i].ip != ip || !falhas[i].bloqueado) continue;
        if ((int)(falhas[i].ate - tempo_ms()) > 0) return true;
        falhas[i].ip = 0;
        falhas[i].bloqueado = false;
    }
    return false;
}

/* Pasa al kernel lo que piden las sesiones (mensajes de 12 bytes por su tubería). */
static void atender_tubos(void)
{
    for (int i = 0; i < MAX_FILHOS; i++) {
        if (!filhos[i].pid || filhos[i].tubo < 0) continue;
        int d;
        while ((d = disponivel(filhos[i].tubo)) >= 12) {
            int m[3];
            if (ler(filhos[i].tubo, m, sizeof m) != (int)sizeof m) break;
            if (inyecta_ok && (m[0] == INY_TECLA || m[0] == INY_RATON || m[0] == INY_RUEDA)) inyectar_entrada(m[0], m[1], m[2]);
        }
        if (d < 0) { fechar(filhos[i].tubo); filhos[i].tubo = -1; }
    }
}

static void recolher_filhos(void)
{
    int pid, cod;
    while ((pid = recolher(&cod)) > 0)
        for (int i = 0; i < MAX_FILHOS; i++)
            if (filhos[i].pid == pid) {
                anotar_falha(filhos[i].ip, cod == SAIR_SEM_AUT);
                atender_tubos();
                if (filhos[i].tubo >= 0) fechar(filhos[i].tubo);
                filhos[i].pid = 0;
                filhos[i].tubo = -1;
            }
}

static int escutar_vnc(int porta_pedida)
{
    conf_carregar();
    if (porta_pedida) conf.porta = porta_pedida;
    bool tem_clave = clave_cargar();
    int e = criar_socket(SOCK_TCP);
    int r = e < 0 ? e : escutar(e, conf.porta, 4);
    if (r < 0) {
        anotar("no se puede escuchar en el puerto %d: %s", conf.porta, nome_erro(r));
        imprimir_em(ERROS, "vncd: puerto %d: %s\n", conf.porta, nome_erro(r));
        return 1;
    }
    anotar("escuchando en el puerto %d (%s)%s%s", conf.porta,
           conf.red_local ? "red local" : "solo desde esta máquina, 127.0.0.1: usar un túnel ssh -L",
           conf.solo_ver ? "; solo ver" : "", conf.sin_clave ? "; SIN CONTRASEÑA (seguridad=ninguna)" : "");
    if (!tem_clave && !conf.sin_clave)
        anotar("aviso: no hay contraseña (vncd --clave): nadie puede entrar");
    imprimir("vncd: escuchando en el puerto %d\n", conf.porta);
    inyecta_ok = !conf.solo_ver && inyectar_entrada(INY_ESTADO, 1, 0) == 0;
    if (conf.solo_ver) anotar("solo_ver=si: no se pide permiso de inyección");
    else if (!inyecta_ok) anotar("aviso: el kernel no da permiso de inyectar entrada (hay que arrancar como servicio): solo ver");
    else anotar("permiso de inyección de teclado y ratón concedido");
    for (int i = 0; i < MAX_FILHOS; i++) filhos[i].tubo = -1;
    bool com_tubo = tipo_descritor(ENTRADA) == TIPO_PIPE;
    unsigned lim = 0;
    for (;;) {
        unsigned quer = n_filhos() ? 8 : 300;   /* con sesiones, se mira su tubería a menudo */
        if (quer != lim) { tempo_limite(e, quer); lim = quer; }
        atender_tubos();
        if (com_tubo && disponivel(ENTRADA) == ERRO_PIPE_QUEBRADO) {
            anotar("parada pedida por el gestor de servicios");
            fechar(e);
            return 0;
        }
        recolher_filhos();
        endereco_rede_t de;
        int s = aceitar_conexao(e, &de);
        if (s == ERRO_TEMPO) continue;
        if (s < 0) { dormir(300); continue; }
        char ip[16], porta[8];
        ip_para_texto(de.ip, ip);
        formatar_texto(porta, sizeof porta, "%u", de.porta);
        config_rede_t cr;
        memset(&cr, 0, sizeof cr);
        config_rede(REDE_CONSULTAR, &cr);
        const char *motivo = NULL;
        if (!conf.red_local && (de.ip >> 24) != 127) motivo = "no viene de esta máquina (escuchar=local)";
        else if (conf.red_local && !ssh_ip_local(de.ip, cr.ip, cr.mascara)) motivo = "no es de la red local";
        else if (bloqueada(de.ip)) motivo = "dirección bloqueada por fallos de autenticación";
        else if (n_filhos() >= conf.max_clientes) motivo = "demasiados clientes a la vez";
        int pid = 0, tb[2] = { -1, -1 };
        if (!motivo) {
            char *argv[] = { "vncd", "--sesion", ip, porta, inyecta_ok ? "1" : "0", NULL };
            if (inyecta_ok && criar_pipe(tb) != 0) { tb[0] = tb[1] = -1; }
            int g0 = duplicar(ENTRADA, 28), g1 = -1;
            duplicar(s, ENTRADA);
            if (tb[1] >= 0) { g1 = duplicar(SAIDA, 27); duplicar(tb[1], SAIDA); }
            pid = executar("/bin/vncd", argv);
            duplicar(g0, ENTRADA);
            fechar(g0);
            if (g1 >= 0) { duplicar(g1, SAIDA); fechar(g1); }
            if (tb[1] >= 0) fechar(tb[1]);
            if (pid < 0) { motivo = nome_erro(pid); if (tb[0] >= 0) fechar(tb[0]); tb[0] = -1; }
        }
        fechar(s);
        if (motivo) { anotar("conexión de %s:%s rechazada: %s", ip, porta, motivo); continue; }
        for (int i = 0; i < MAX_FILHOS; i++)
            if (!filhos[i].pid) { filhos[i].pid = pid; filhos[i].ip = de.ip; filhos[i].tubo = tb[0]; break; }
    }
}

/* ------------------------------------------------------------ principal */

static const char AYUDA[] =
    "uso: vncd                 servidor de escritorio remoto VNC (mejora 097, D252)\n"
    "     vncd --clave [TEXTO] fija la contraseña (solo cuentan los 8 primeros caracteres)\n"
    "Como servicio (desactivado de serie):  vncd --clave ...; servicio activar vncd ; servicio iniciar vncd\n"
    "Por defecto solo acepta conexiones desde la propia máquina (127.0.0.1): desde el PC,\n"
    "  ssh -L 5900:localhost:5900 root@IP   y el cliente VNC a localhost:5900.\n"
    "Opciones en " DIR_ETC "/vncd.conf y " DIR_CASA "/vncd.conf: puerto, escuchar=local|red-local,\n"
    "solo_ver, max_clientes, bloqueo_fallos, fps_max... Registro: /logs/vncd.log. Ver docs/vnc.md.\n";

int principal(int argc, char **argv)
{
    int porta = 0;
    if (argc >= 4 && compara_texto(argv[1], "--sesion") == 0) return sesion(argv[2], argv[3], argc >= 5 && argv[4][0] == '1');
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--ayuda") == 0 || compara_texto(argv[i], "--help") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        } else if (compara_texto(argv[i], "--clave") == 0) {
            char t[80];
            if (i + 1 < argc) copia_texto(t, argv[i + 1], sizeof t);
            else {
                imprimir("Contraseña nueva: ");
                int n = ler(ENTRADA, t, sizeof t - 1);
                if (n < 0) n = 0;
                t[n] = 0;
                for (int k = 0; k < n; k++) if (t[k] == '\n' || t[k] == '\r') { t[k] = 0; break; }
            }
            return clave_fijar(t);
        } else if (compara_texto(argv[i], "-p") == 0 && i + 1 < argc) {
            porta = texto_para_int(argv[++i]);
            if (porta <= 0 || porta > 65535) { imprimir_em(ERROS, "vncd: puerto no válido\n"); return 2; }
        } else {
            escrever_texto(ERROS, AYUDA);
            return 2;
        }
    }
    return escutar_vnc(porta);
}
