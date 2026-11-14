// SPDX-License-Identifier: GPL-3.0-only
/* sagui3d.c - Sagui 3D (bônus B4): ventana, bucle del juego, pantallas y sonidos.
 *
 * El sagui quedó atrapado en un laberinto de piedra en medio de la Mata Atlántica. En
 * cada fase tiene que juntar bananas para abrir la cerca de la salida,
 * mientras los monos-prego guardianes intentan robarle las bananas del bolsillo.
 * La honda dispara semillas de jabuticaba (tres dejan a un mono aturdido y
 * huye) y cada banana comida deja una cáscara, que puede dejarse en el
 * suelo para que un mono resbale.
 *
 * Uso: sagui3d             las tres fases de /jogos/sagui3d
 *      sagui3d <archivo>   una fase suelta (los tests usan esto)
 *      sagui3d --medir [n] dibuja n cuadros girando en la fase 1 y muestra
 *                          cuadros por segundo, dónde va el tiempo y una suma
 *                          de los píxeles (la misma en cualquier CPU, D042)
 *
 * El bucle separa las reglas del dibujo: en cada vuelta, el tiempo que pasó se
 * convierte en tiques de 30 ms de regla (mundo_tique) y después se dibuja un cuadro.
 * En una computadora rápida sobran cuadros; en una lenta, se saltean algunos
 * cuadros, pero el juego avanza a la misma velocidad. */
#include "jogo.h"

extern const unsigned char fonte_sagui[256][16];

#define QTD_FASES 3
static const char *const arquivos_fases[QTD_FASES] = {
    "/jogos/sagui3d/fase1.txt", "/jogos/sagui3d/fase2.txt", "/jogos/sagui3d/fase3.txt",
};

static jan_t *j;
static int relogio;         /* tiques desde el comienzo (animaciones) */
static int cachos;          /* racimos dorados encontrados en todo el juego */
static int mouse_x = -1;

enum { SAIR = 100 };

/* ------------------------------------------------------------ sonidos */

#define TOCAR(...) do { static const nota_t n_[] = { __VA_ARGS__ }; efeito_tocar(n_, (int)(sizeof n_ / sizeof n_[0])); } while (0)
void som_tiro(void)            { TOCAR({ 700, 20 }, { 480, 30 }); }
void som_banana(void)          { TOCAR({ 1047, 40 }, { 1319, 60 }); }
void som_acerto(void)          { TOCAR({ 220, 40 }, { 170, 60 }); }
void som_vida_perdida(void)    { TOCAR({ 392, 100 }, { 330, 100 }, { 262, 200 }); }
void som_casca(void)           { TOCAR({ 880, 40 }, { 660, 60 }); }
void som_escorregou(void)      { TOCAR({ 300, 30 }, { 400, 30 }, { 520, 30 }, { 680, 30 }, { 820, 40 }, { 600, 40 }, { 420, 60 }); }
void som_cacho(void)           { TOCAR({ 1047, 80 }, { 1319, 80 }, { 1568, 80 }, { 2093, 200 }); }
void som_porteira_fechada(void){ TOCAR({ 196, 80 }, { 0, 40 }, { 196, 120 }); }
void som_fase_concluida(void)
{
    TOCAR({ 523, 120 }, { 659, 120 }, { 784, 120 }, { 1047, 240 }, { 0, 60 }, { 784, 120 }, { 1047, 360 });
}

/* ------------------------------------------------------------ dibujo en la ventana */

static void figura_na_janela(int id, int x, int y, int esc, int luz)
{
    const figura_t *f = &figuras[id];
    for (int v = 0; v < f->a; v++)
        for (int u = 0; u < f->l; u++) {
            uint8_t p = f->px[v * f->l + u];
            if (p) g_retangulo(j, x + u * esc, y + v * esc, esc, esc, sombra[luz][p]);
        }
}

/* Texto ampliado (títulos): cada píxel de la letra se vuelve un cuadrado. */
static void texto_grande(int x, int y, const char *t, uint32_t cor, int esc)
{
    for (; *t; t++, x += 8 * esc) {
        const unsigned char *g = fonte_sagui[(uint8_t)*t];
        for (int lin = 0; lin < 16; lin++)
            for (int col = 0; col < 8; col++)
                if (g[lin] & (0x80 >> col)) g_retangulo(j, x + col * esc, y + lin * esc, esc, esc, cor);
    }
}

static void texto_centro(int y, const char *t, uint32_t cor)
{
    int x = (JAN_L - g_largura_texto(t)) / 2;
    g_texto(j, x + 1, y + 1, t, 0x000000);
    g_texto(j, x, y, t, cor);
}

static void ampliar_vista(void)
{
    for (int y = 0; y < VA; y++) {
        uint32_t *l0 = j->px + (y * ESCALA) * j->larg;
        uint32_t *l1 = l0 + j->larg;
        ampliar2x32(l0, l1, vista[y], VL);   /* SSE2 si compensa (D042) */
    }
}

/* Oscurece el área de la vista (fondo de las pantallas de inicio y de fin). */
static void escurecer(void)
{
    escurecer32(j->px, JAN_L * VA * ESCALA, 2, 0x3F3F3F);
}

static void barra_de_status(void)
{
    int y0 = VA * ESCALA;
    g_retangulo(j, 0, y0, JAN_L, BARRA, 0x0E2A18);
    g_retangulo(j, 0, y0, JAN_L, 2, 0x0B7A3E);
    char t[48];
    /* Bananas: actuales / mínimo de la cerca (verde cuando ya abre). */
    figura_na_janela(F_BANANA, 10, y0 + 10, 2, 15);
    formatar_texto(t, sizeof t, "%d/%d", sagui.bananas, mapa.minimo);
    g_texto(j, 40, y0 + 8, t, porteira_aberta() ? 0x7CE07C : 0xF4D03F);
    g_texto(j, 40, y0 + 26, "verja", 0x9FB8A4);
    figura_na_janela(F_CASCA, 120, y0 + 14, 2, 15);
    formatar_texto(t, sizeof t, "%d", sagui.cascas);
    g_texto(j, 152, y0 + 8, t, 0xFFFFFF);
    g_texto(j, 124, y0 + 26, "cáscaras", 0x9FB8A4);
    figura_na_janela(F_SEMENTE, 200, y0 + 10, 3, 15);
    formatar_texto(t, sizeof t, "%d", sagui.sementes);
    g_texto(j, 224, y0 + 8, t, sagui.sementes ? 0xFFFFFF : 0xE04848);
    g_texto(j, 196, y0 + 26, "semillas", 0x9FB8A4);
    for (int v = 0; v < sagui.vidas && v < 9; v++)
        figura_na_janela(F_CORACAO, 290 + v * 17, y0 + 10, 2, 15);
    g_texto(j, 290, y0 + 26, "vidas", 0x9FB8A4);
    if (sagui.cacho) figura_na_janela(F_CACHO, 452, y0 + 8, 2, 15);
    formatar_texto(t, sizeof t, "Fase %d", fase_atual);
    g_texto(j, 492, y0 + 8, t, 0xFFFFFF);
    formatar_texto(t, sizeof t, "Puntos %d", sagui.pontos);
    g_texto(j, 492, y0 + 26, t, 0xF4D03F);
}

static void desenhar_quadro(bool pausado)
{
    camera_t c = { sagui.x, sagui.y, sagui.z, sagui.ang, sagui.sala };
    tela3d_desenhar(&c, relogio);
    ampliar_vista();
    /* La honda, balanceándose un poco mientras el sagui anda. */
    int luz = mapa.salas[sagui.sala].luz < 6 ? 6 : mapa.salas[sagui.sala].luz;
    int balanco = INTEIRO(seno(relogio * 90) * 3);
    figura_na_janela(sagui.puxado ? F_ESTILINGUE_PUXADO : F_ESTILINGUE, JAN_L / 2 - 80,
                     VA * ESCALA - 116 + (sagui.puxado ? 6 : balanco), 4, luz);
    if (sagui.invulneravel > 70) {   /* se asustó: borde rojo */
        g_retangulo(j, 0, 0, JAN_L, 6, 0xC03030);
        g_retangulo(j, 0, VA * ESCALA - 6, JAN_L, 6, 0xC03030);
        g_retangulo(j, 0, 0, 6, VA * ESCALA, 0xC03030);
        g_retangulo(j, JAN_L - 6, 0, 6, VA * ESCALA, 0xC03030);
    }
    if (aviso_tempo > 0) {
        int l = g_largura_texto(aviso) + 24;
        g_retangulo(j, (JAN_L - l) / 2, 14, l, 26, 0x10241A);
        g_moldura(j, (JAN_L - l) / 2, 14, l, 26, 0x5FB33A);
        texto_centro(19, aviso, 0xFFFFFF);
    }
    if (pausado) {
        escurecer();
        texto_grande((JAN_L - 7 * 8 * 3) / 2, 140, "Pausado", 0xFFFFFF, 3);
        texto_centro(220, "Esc: continuar      Q: salir del juego", 0xD8E8DC);
    }
    barra_de_status();
    jan_mostrar(j);
}

/* ------------------------------------------------------------ pantallas */

/* Espera Enter (true) o Esc / cerrar (false), animando el fondo. */
static int esperar_tecla(void (*fundo)(int quadro))
{
    for (int q = 0;; q++) {
        evento_t e;
        while (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) return SAIR;
            if (e.tipo == EV_TECLA && (e.tecla == '\n' || e.tecla == '\r')) return 1;
            if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) return 0;
            if (e.tipo == EV_CLIQUE) return 1;
        }
        if (fundo) fundo(q);
        dormir(40);
    }
}

static void fundo_inicio(int q)
{
    /* La primera fase girando despacio detrás del título. */
    camera_t c = { mapa.inicio_x, mapa.inicio_y, mapa.salas[mapa.inicio_sala].chao + FX(OLHO), mapa.inicio_ang + q * 6, mapa.inicio_sala };
    tela3d_desenhar(&c, q);
    ampliar_vista();
    escurecer();
    figura_na_janela(F_SAGUI, JAN_L / 2 - 200, 30, 4, 15);
    figura_na_janela(F_MACACO, JAN_L / 2 + 136, 22, 3, 15);
    texto_grande((JAN_L - 8 * 8 * 4) / 2, 40, "Sagui 3D", 0xF4D03F, 4);
    static const char *const historia[] = {
        "El sagui está atrapado en un laberinto de piedra en la Mata Atlántica.",
        "En cada fase, junta bananas para abrir la verja de la salida.",
        "Los monos capuchinos guardianes quieren las bananas de tu bolsillo:",
        "tres semillas de jabuticaba con el tirachinas y se marean y huyen.",
        "Cada banana deja una cáscara. ¡Suéltala y verás resbalar al mono!",
        "En cada fase hay un racimo dorado escondido tras una pared de lianas.",
    };
    for (int i = 0; i < 6; i++) texto_centro(130 + i * 20, historia[i], 0xE6EFE8);
    texto_centro(268, "Flechas: andar y girar    Shift: correr    Ctrl o espacio: tirachinas", 0x9FD8A8);
    texto_centro(288, "B: soltar una cáscara    Esc: pausa    Ratón: girar y disparar", 0x9FD8A8);
    if ((q / 12) % 2 == 0) texto_centro(336, "Enter: empezar", 0xFFFFFF);   /* parpadeando */
    g_retangulo(j, 0, VA * ESCALA, JAN_L, BARRA, 0x0E2A18);
    texto_centro(VA * ESCALA + 16, "Un juego de SaguiOS-Extended, basado en el proyecto de Eli Rigobeli", 0x9FB8A4);
    jan_mostrar(j);
}

static void tela_texto(const char *titulo, uint32_t cor_titulo, const char *const *linhas, int n, const char *rodape)
{
    desenhar_quadro(false);
    escurecer();
    int esc = 3, l = (int)tam_texto(titulo) * 8 * esc;
    texto_grande((JAN_L - l) / 2, 60, titulo, cor_titulo, esc);
    for (int i = 0; i < n; i++) texto_centro(150 + i * 24, linhas[i], 0xFFFFFF);
    texto_centro(330, rodape, 0x9FD8A8);
    jan_mostrar(j);
}

/* ------------------------------------------------------------ jugar una fase */

static int jogar_fase(void)
{
    unsigned antes = tempo_ms(), medida = antes;
    int acumulado = 0, quadros = 0;
    bool pausado = false, casca = false, clique = false;
    int giro_mouse = 0;
    for (;;) {
        evento_t e;
        while (proximo_evento(j->id, &e, false) == 1) {
            if (e.tipo == EV_FECHAR) return SAIR;
            if (e.tipo == EV_TECLA) {
                if (e.tecla == TECLA_ESC) {
                    pausado = !pausado;
                    registrar("sagui3d: %s", pausado ? "pausa" : "continua");
                } else if (pausado && (e.tecla == 'q' || e.tecla == 'Q')) {
                    return SAIR;
                } else if (!pausado && (e.tecla == 'b' || e.tecla == 'B')) {
                    casca = true;
                }
            } else if (e.tipo == EV_MOVER) {
                if (e.y < VA * ESCALA && mouse_x >= 0) giro_mouse -= (e.x - mouse_x) * 5;
                mouse_x = e.x;
            } else if (e.tipo == EV_CLIQUE && e.y < VA * ESCALA) {
                clique = true;
            }
        }
        unsigned agora = tempo_ms();
        acumulado += (int)(agora - antes);
        antes = agora;
        if (acumulado > 200) acumulado = 200;   /* después de una pausa larga, no corre detrás */
        if (pausado) {
            acumulado = 0;
            desenhar_quadro(true);
            dormir(50);
            continue;
        }
        unsigned char teclas[32];
        teclas_seguradas(teclas);
        comandos_t cmd = {
            .frente = SEGURADA(teclas, CV_CIMA), .tras = SEGURADA(teclas, CV_BAIXO),
            .esquerda = SEGURADA(teclas, CV_ESQUERDA), .direita = SEGURADA(teclas, CV_DIREITA),
            .atirar = SEGURADA(teclas, CV_CTRL) || SEGURADA(teclas, CV_CTRL_DIR) || SEGURADA(teclas, CV_ESPACO) || clique,
            .correr = SEGURADA(teclas, CV_SHIFT),
            .giro_mouse = giro_mouse,
        };
        while (acumulado >= TIQUE_MS) {
            acumulado -= TIQUE_MS;
            relogio++;
            if (casca) { mundo_largar_casca(); casca = false; }
            int r = mundo_tique(&cmd);
            cmd.giro_mouse = 0;
            giro_mouse = 0;
            if (clique) { clique = false; cmd.atirar = false; }
            if (r != R_NADA) { desenhar_quadro(false); return r; }
        }
        desenhar_quadro(false);
        quadros++;
        if (agora - medida >= 5000) {
            registrar("sagui3d: %d quadros por segundo", quadros * 1000 / (int)(agora - medida));
            quadros = 0;
            medida = agora;
        }
        if (acumulado < TIQUE_MS / 2) dormir(5);
    }
}

static bool carregar_fase(const char *arquivo)
{
    char erro[96];
    if (mapa_carregar(arquivo, erro, sizeof erro)) return true;
    registrar("sagui3d: erro na fase %s: %s", arquivo, erro);
    g_retangulo(j, 0, 0, JAN_L, JAN_A, 0x101820);
    texto_centro(180, "No se ha podido abrir la fase:", 0xFFFFFF);
    texto_centro(204, erro, 0xF4D03F);
    jan_mostrar(j);
    esperar_tecla(NULL);
    return false;
}

static int partida(const char *avulsa)
{
    sagui.vidas = 3;
    sagui.pontos = 0;
    sagui.sementes = 15;
    cachos = 0;
    int fases = avulsa ? 1 : QTD_FASES;
    for (int f = 0; f < fases; f++) {
        fase_atual = f + 1;
        if (!carregar_fase(avulsa ? avulsa : arquivos_fases[f])) return SAIR;
        mundo_iniciar_fase();
        int r = jogar_fase();
        if (r == SAIR) return SAIR;
        if (r == R_FIM_DE_JOGO) {
            registrar("sagui3d: fim de jogo, pontos %d", sagui.pontos);
            char p[48];
            formatar_texto(p, sizeof p, "Puntos: %d", sagui.pontos);
            const char *l[] = { "Los monos capuchinos le han quitado todas las bananas al sagui.", p };
            tela_texto("Fin del juego", 0xE04848, l, 2, "Enter: volver a jugar      Esc: salir");
            return esperar_tecla(NULL) == 1 ? 1 : SAIR;
        }
        if (sagui.cacho) cachos++;
        registrar("sagui3d: tela de vitoria da fase %d", fase_atual);
        char t1[64], t2[48], t3[48], t4[48], tit[32];
        formatar_texto(tit, sizeof tit, "¡Fase %d completada!", fase_atual);
        formatar_texto(t1, sizeof t1, "%s", mapa.nome);
        formatar_texto(t2, sizeof t2, "Bananas: %d de %d", sagui.bananas, mapa.total_bananas);
        formatar_texto(t3, sizeof t3, "Racimo dorado: %s", sagui.cacho ? "encontrado" : "no encontrado");
        formatar_texto(t4, sizeof t4, "Monos ahuyentados: %d      Puntos: %d", sagui.espantados, sagui.pontos);
        const char *l[] = { t1, t2, t3, t4 };
        bool ultima = f == fases - 1;
        tela_texto(tit, 0x7CE07C, l, 4, ultima ? "Enter: continuar" : "Enter: siguiente fase");
        if (esperar_tecla(NULL) == SAIR) return SAIR;
    }
    if (!avulsa) {
        registrar("sagui3d: jogo completo, pontos %d, cachos %d", sagui.pontos, cachos);
        char p[48], c[48];
        formatar_texto(p, sizeof p, "Puntos: %d", sagui.pontos);
        formatar_texto(c, sizeof c, "Racimos dorados: %d de %d", cachos, QTD_FASES);
        const char *l[] = { "El sagui ha encontrado la salida del laberinto y ha vuelto a la selva.", p, c };
        tela_texto("¡Libertad!", 0xF4D03F, l, 3, "Enter: volver a jugar      Esc: salir");
        return esperar_tecla(NULL) == 1 ? 1 : SAIR;
    }
    return 1;
}

/* ------------------------------------------------------------ medida (D042) */

static inline uint64_t ciclos(void)
{
    uint32_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return ((uint64_t)d << 32) | a;
}

static uint32_t somar_pixels(uint32_t h, const uint32_t *p, int n)
{
    for (int i = 0; i < n; i++) h = h * 31 + p[i];
    return h;
}

/* Dibuja n cuadros, girando el sagui en el punto de partida de la fase 1, sin las
 * reglas del juego (siempre la misma secuencia de imágenes). Separa el tiempo del
 * dibujo 3D, de la ampliación para la ventana y del kernel (jan_mostrar: componer y
 * copiar a la placa). La suma de los píxeles (con la pantalla oscurecida de la pausa
 * al final) tiene que ser la misma con y sin SSE2. */
static int medir(int n)
{
    if (!carregar_fase(arquivos_fases[0])) return 1;
    mundo_iniciar_fase();
    uint64_t t3d = 0, tamp = 0, tjan = 0, tsoma = 0;
    uint32_t soma = 0;
    unsigned ms0 = tempo_ms();
    uint64_t c0 = ciclos();
    for (int q = 0; q < n; q++) {
        camera_t c = { sagui.x, sagui.y, sagui.z, ANG(sagui.ang + q * 16), sagui.sala };
        uint64_t a = ciclos();
        tela3d_desenhar(&c, q);
        uint64_t b = ciclos();
        ampliar_vista();
        uint64_t d = ciclos();
        jan_mostrar(j);
        uint64_t e = ciclos();
        soma = somar_pixels(soma, j->px, JAN_L * VA * ESCALA);
        if (q == n - 1) { escurecer(); soma = somar_pixels(soma, j->px, JAN_L * VA * ESCALA); }
        t3d += b - a; tamp += d - b; tjan += e - d; tsoma += ciclos() - e;
    }
    /* Sin división de 64 bits en libsagui: los ciclos van en unidades de 1024. */
    uint32_t total = (uint32_t)((ciclos() - c0) >> 10), soma_k = (uint32_t)(tsoma >> 10);
    unsigned ms = tempo_ms() - ms0;
    if (!ms) ms = 1;
    uint32_t util_k = total - soma_k, pct = util_k / 100 + 1;
    uint32_t por_ms = total / ms + 1;   /* unidades por milisegundo */
    unsigned util = util_k / por_ms;
    if (!util) util = 1;
    char t[160];
    formatar_texto(t, sizeof t, "sagui3d: medida: %d cuadros en %u ms = %u.%u cuadros/s "
                   "(3D %u%%, ampliar %u%%, ventana %u%%) suma=%x",
                   n, util, (unsigned)n * 1000 / util, ((unsigned)n * 10000 / util) % 10,
                   (unsigned)(t3d >> 10) / pct, (unsigned)(tamp >> 10) / pct, (unsigned)(tjan >> 10) / pct, soma);
    registrar("%s", t);
    /* El resultado queda en la ventana hasta Enter, Esc o cerrar. (No va a la
     * terminal: el sagui3d corre en segundo plano y escribir allí mientras el
     * sagsh espera un comando lo termina, ver el informe de D042.) */
    escurecer();
    char l1[64], l2[80];
    formatar_texto(l1, sizeof l1, "%d cuadros en %u ms: %u.%u cuadros/s", n, util,
                   (unsigned)n * 1000 / util, ((unsigned)n * 10000 / util) % 10);
    formatar_texto(l2, sizeof l2, "3D %u%%   ampliar %u%%   ventana (kernel) %u%%   SSE2: %s",
                   (unsigned)(t3d >> 10) / pct, (unsigned)(tamp >> 10) / pct, (unsigned)(tjan >> 10) / pct,
                   cpu_tem_sse2() ? "sí" : "no");
    texto_centro(150, "Sagui 3D: medida de rendimiento", 0xF4D03F);
    texto_centro(180, l1, 0xFFFFFF);
    texto_centro(204, l2, 0xD8E8DC);
    texto_centro(260, "Enter o Esc: cerrar", 0x9FD8A8);
    jan_mostrar(j);
    esperar_tecla(NULL);
    return 0;
}

int principal(int argc, char **argv)
{
    contas_iniciar();
    arte_iniciar();
    j = jan_abrir("Sagui 3D", JAN_L, JAN_A);
    if (!j) { imprimir_em(ERROS, "sagui3d: no he podido abrir la ventana\n"); return 1; }
    /* F11: pantalla completa; el kernel amplía la imagen de 640x448 manteniendo la proporción (D044). */
    jan_estilo(j, JAN_TELA_CHEIA_OK, 0, 0);
    if (argc > 1 && compara_texto(argv[1], "--medir") == 0) {
        int r = medir(argc > 2 ? texto_para_int(argv[2]) : 100);
        jan_fechar(j);
        return r;
    }
    const char *avulsa = argc > 1 ? argv[1] : NULL;
    for (;;) {
        if (!carregar_fase(avulsa ? avulsa : arquivos_fases[0])) break;
        registrar("sagui3d: tela de inicio");
        if (esperar_tecla(fundo_inicio) != 1) break;
        registrar("sagui3d: inicio do jogo");
        if (partida(avulsa) == SAIR) break;
    }
    efeito_parar();
    jan_fechar(j);
    return 0;
}
