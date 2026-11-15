// SPDX-License-Identifier: GPL-2.0-or-later
/* instalar_asistente.h - el asistente gráfico de «Instalar SaguiOS» (D318, v8.4.3). Lo incluye
 * instalar.c (que tiene el plan y la instalación: aquí solo está la ventana).
 *
 * Aspecto de Windows 98 (D289, D291): gris, biseles, una franja a la izquierda con los pasos y
 * «< Atrás», «Siguiente >» y «Cancelar» abajo. Pasos:
 *   1 Bienvenida: lo que se ha detectado (CPU, RAM, discos, BIOS o UEFI);
 *   2 Disco: la lista de discos con su tipo y sus particiones dibujadas en una barra;
 *   3 Tipo: «Usar todo el disco» (se borra todo: hay que escribir el nombre del disco),
 *     «Junto a lo que hay» (el espacio libre, o reducir una partición FAT32/ext2: el tirador de la
 *     barra o el cuadro de MB marcan su tamaño nuevo) o «Avanzado» (se elige en la barra qué hueco
 *     usar, qué partición reducir o cuál sustituir);
 *   4 Tamaños: /home, /apps, juegos, /tmp y /logs, cada uno con su sistema de archivos;
 *   5 Opciones: arranque BIOS/UEFI/los dos, idioma, teclado, zona, contraseñas, nombre del equipo,
 *     servicios y qué copiar de /apps y de los juegos;
 *   6 Configuración: qué se copia de esta sesión (con su tamaño);
 *   7 Resumen: qué se borra, qué se crea y qué se copia, dicho claro; «Instalar» pide confirmación;
 *   8 Progreso (barra y registro) y el final con «Reiniciar ahora».
 * Para las pruebas (h318), cada vez que cambia lo que hay en la ventana se escribe en el registro
 * «instalar: zona <nombre> x y l a» de cada cosa que se puede pulsar (coordenadas de la ventana). */

#define AJ_L 720
#define AJ_A 540
#define FRANJA 150                 /* la franja de la izquierda */
#define CX0 (FRANJA + 16)          /* donde empieza el contenido */
#define CXL (AJ_L - CX0 - 16)
#define BOT_Y (AJ_A - 40)

enum { P_BIENVENIDA, P_DISCO, P_TIPO, P_TAMANOS, P_OPCIONES, P_CONFIG, P_RESUMEN, P_PROGRESSO, P_FIM, N_PAGINAS };
static const char *const nome_pagina[N_PAGINAS] = { "Bienvenida", "Disco", "Tipo de instalación", "Tamaños", "Opciones",
                                                    "Configuración", "Resumen", "Instalando", "Terminado" };
enum { TI_ENTERO, TI_JUNTO, TI_AVANZADO };
enum { ES_NADA, ES_HUECO, ES_REDUCIR, ES_SUSTITUIR };   /* lo elegido en «junto» o «avanzado» */
enum { AR_BIOS, AR_UEFI, AR_AMBOS };
enum { D_HOME, D_APPS, D_JOGOS, D_TMP, D_LOGS, N_DADOS };
static const char *const nome_dado[N_DADOS] = { "/home", "/apps", "juegos (/jogos)", "/tmp", "/logs" };
static const char *const id_dado[N_DADOS] = { "home", "apps", "jogos", "tmp", "logs" };

static jan_t *j;
static int pagina, pagina_logada = -1;
static bool layout_mudou = true;
static plano_t pl;
static char plano_erro[300], resultado[600];
static bool resultado_ok;
/* discos */
static char destinos[12][12];
static bool destino_ok[12];
static char destino_por[12][96];
static int ndest, sel_dest = -1;
static char origem_txt[12];
/* tipo */
static int tipo = TI_ENTERO;
static pt_tabela_t tab;
static bool tab_ok;
static caixa_t cx_confirma;
static int esc = ES_NADA, esc_part;
static uint32_t esc_hueco, esc_hueco_set;
static uint32_t esc_red, esc_min, esc_max;   /* reducir: tamaño nuevo, mínimo (lo que ocupa) y el de ahora */
static caixa_t cx_red;
static bool arrastando;
static int sel_part;                         /* avanzado: la partición pulsada en la barra */
static char tipo_aviso[200];
/* tamaños */
static casilla_t cs_usa[N_DADOS];
static caixa_t cx_mb[N_DADOS];
static int fs_esc[N_DADOS];
static casilla_t cs_resto;
static bool tamanhos_prontos;
/* opciones */
static int arranque = AR_AMBOS, tecl = TECLADO_ES;
static lista_t lz;
static char (*zonas_txt)[64];
static const zona_info_t *zonas;
static int nzonas;
static caixa_t cx_nome, cx_r1, cx_r2, cx_u1, cx_u2;
static casilla_t cs_sshd, cs_vncd, cs_apps, cs_jogos;
/* configuración */
static const unsigned cc_bit[7] = { CC_PREFS, CC_WIFI, CC_SSH, CC_CORREO, CC_IA, CC_IA_CHAVE, CC_DOCS };
static const char *const cc_id[7] = { "cc-prefs", "cc-wifi", "cc-ssh", "cc-correo", "cc-ia", "cc-ia-clave", "cc-docs" };
static casilla_t cs_cc[7];
/* progreso */
static int prog_fase, prog_pc;
static char prog_log[3000];

/* ------------------------------------------------------------ zonas pulsables */

typedef struct { char id[20]; int x, y, l, a; } zona_t;
static zona_t zonas_ui[80];
static int nzonas_ui;

static void zona(const char *id, int x, int y, int l, int a)
{
    if (nzonas_ui >= 80) return;
    zona_t *z = &zonas_ui[nzonas_ui++];
    copia_texto(z->id, id, sizeof z->id);
    z->x = x; z->y = y; z->l = l; z->a = a;
}

static const char *zona_em(int x, int y)
{
    for (int i = nzonas_ui - 1; i >= 0; i--) {
        zona_t *z = &zonas_ui[i];
        if (x >= z->x && x < z->x + z->l && y >= z->y && y < z->y + z->a) return z->id;
    }
    return "";
}

static bool e_zona(const char *a, const char *b) { return compara_texto(a, b) == 0; }

/* ------------------------------------------------------------ dibujo al estilo Win98 */

static void negrito(int x, int y, const char *t, uint32_t cor) { g_texto(j, x, y, t, cor); g_texto(j, x + 1, y, t, cor); }

static void botao98(const char *id, int x, int y, int l, const char *t, bool ativo)
{
    g_retangulo(j, x, y, l, 26, W98_FACE);
    g_bisel(j, x, y, l, 26, false);
    int tx = x + (l - g_largura_texto(t)) / 2;
    if (ativo) g_texto(j, tx, y + 5, t, W98_NEGRO);
    else { g_texto(j, tx + 1, y + 6, t, W98_LUZ); g_texto(j, tx, y + 5, t, W98_SOMBRA); }
    if (ativo) zona(id, x, y, l, 26);
}

static void radio98(const char *id, int x, int y, bool marcado, const char *t, bool ativo)
{
    /* Un círculo de 12 px, como el botón de opción de Windows 98: borde de sombra arriba a la
     * izquierda y de luz abajo a la derecha, fondo blanco y el punto negro si está marcado. */
    static const int meia[12] = { 2, 4, 5, 5, 6, 6, 6, 6, 5, 5, 4, 2 };
    int cx = x + 6, cy = y + 2;
    for (int k = 0; k < 12; k++) {
        int h = meia[k];
        g_retangulo(j, cx - h, cy + k, 2 * h, 1, k < 6 ? W98_SOMBRA : W98_LUZ);
        if (h > 1) g_retangulo(j, cx - h + 1, cy + k, 2 * h - 2, 1, ativo ? W98_LUZ : W98_FACE);
        g_retangulo(j, cx - h, cy + k, 1, 1, W98_SOMBRA);
        g_retangulo(j, cx + h - 1, cy + k, 1, 1, W98_LUZ);
    }
    g_retangulo(j, cx - 1, cy, 2, 1, W98_SOMBRA);
    g_retangulo(j, cx - 1, cy + 11, 2, 1, W98_LUZ);
    if (marcado) { g_retangulo(j, cx - 2, cy + 4, 4, 4, W98_NEGRO); g_retangulo(j, cx - 1, cy + 3, 2, 6, W98_NEGRO); g_retangulo(j, cx - 3, cy + 5, 6, 2, W98_NEGRO); }
    g_texto(j, x + 20, y, t, ativo ? W98_NEGRO : W98_SOMBRA);
    if (ativo) zona(id, x, y, 20 + g_largura_texto(t), 16);
}

static void marcar98(const char *id, casilla_t *c, int x, int y, const char *t, bool ativo)
{
    c->x = x; c->y = y + 1;
    if (ativo) casilla98(j, c, t);   /* la casilla Win98 común (D291) */
    else {
        g_retangulo(j, x, y + 1, 13, 13, W98_FACE);
        g_bisel(j, x, y + 1, 13, 13, true);
        g_texto(j, x + 20, y, t, W98_SOMBRA);
    }
    if (ativo) zona(id, x, y, 20 + g_largura_texto(t), 16);
}

static void caixa98(const char *id, caixa_t *c, int x, int y, int l)
{
    c->x = x; c->y = y; c->l = l;
    caixa_desenhar(j, c);
    zona(id, x, y, l, 24);
}

static void quadro(int x, int y, int l, int a)   /* un recuadro hundido blanco */
{
    g_retangulo(j, x, y, l, a, W98_LUZ);
    g_bisel(j, x, y, l, a, true);
}

static void quebrar(const char *t, char *s, int max, int n)
{
    int k = 0, col = 0;
    while (*t && k < max - 1) {
        const char *p = t;
        while (*p && *p != ' ' && *p != '\n') p++;
        int len = (int)(p - t);
        if (col && col + 1 + len > n) { s[k++] = '\n'; col = 0; }
        else if (col && k < max - 1) { s[k++] = ' '; col++; }
        for (int i = 0; i < len && k < max - 1; i++) s[k++] = t[i];
        col += len;
        t = p;
        if (*t == '\n') { if (k < max - 1) s[k++] = '\n'; col = 0; t++; }
        else if (*t == ' ') t++;
    }
    s[k] = 0;
}

static int texto_linhas(int x, int y, const char *t, int n, uint32_t cor)
{
    char q[2000];
    quebrar(t, q, sizeof q, n);
    const char *l = q;
    while (*l) {
        const char *f = acha_char(l, '\n');
        int c = f ? (int)(f - l) : (int)tam_texto(l);
        g_texto_n(j, x, y, l, c, cor);
        y += 17;
        l += c;
        if (*l == '\n') l++;
    }
    return y;
}

/* ------------------------------------------------------------ datos de la máquina */

static void cpuid_(uint32_t f, uint32_t r[4]) { __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(0)); }

static void nome_cpu(char *s, int max)
{
    uint32_t r[4];
    char m[49];
    cpuid_(0x80000000u, r);
    if (r[0] < 0x80000004u) { copia_texto(s, "procesador x86", (size_t)max); return; }
    for (uint32_t i = 0; i < 3; i++) { cpuid_(0x80000002u + i, r); memcpy(m + 16 * i, r, 16); }
    m[48] = 0;
    const char *p = m;
    while (*p == ' ') p++;
    copia_texto(s, p, (size_t)max);
}

static const char *tipo_disco(const info_disco_t *d)
{
    if (d->flags & DISCO_CD) return "CD/DVD";
    if (d->flags & DISCO_NVME) return (d->flags & DISCO_BLOCO4K) ? "NVMe (bloques de 4096)" : "NVMe";
    if (d->flags & DISCO_REMOVIVEL) return (d->flags & DISCO_USB3) ? "pendrive USB 3" : "pendrive USB";
    if (compara_texto(d->fabricante, "SATA") == 0) return "SATA";
    if (compara_texto(d->fabricante, "ATA") == 0) return "IDE";
    return "disco";
}

/* ------------------------------------------------------------ la barra de un disco */

static uint32_t cor_tipo(uint8_t t)
{
    switch (t) {
    case 0xDA: return 0xE8891C;
    case 0xEF: return 0xC9A60C;
    case 0x0B: case 0x0C: return 0x3A7BD5;
    case 0x07: return 0x7E57C2;
    case 0x83: return 0x2E9E4F;
    default: return 0x7F8C8D;
    }
}

/* Dibuja la tabla t como una barra a escala en (x, y, l, 30). Devuelve la escala en sectores por píxel. */
static uint32_t barra_disco(const pt_tabela_t *t, int x, int y, int l, const char *pref, bool zonas_part)
{
    uint32_t tot = t->setores ? t->setores : 1, esc_px = tot / (uint32_t)l + 1;
    g_retangulo(j, x, y, l, 30, W98_LUZ);
    for (int k = 0; k < l; k += 6) g_linha(j, x + k, y + 29, x + k + 6 < x + l ? x + k + 6 : x + l - 1, y, 0xD0D0D0);   /* libre: rayado */
    for (int i = 0; i < 4 + t->nlog; i++) {
        const pt_part_t *p = i < 4 ? &t->prim[i] : &t->log[i - 4];
        if (!p->tipo || pt_tipo_extendida(p->tipo)) continue;
        int px = x + (int)(p->inicio / esc_px), pl2 = (int)(p->setores / esc_px);
        if (pl2 < 3) pl2 = 3;
        if (px + pl2 > x + l) pl2 = x + l - px;
        g_retangulo(j, px, y, pl2, 30, cor_tipo(p->tipo));
        g_moldura(j, px, y, pl2, 30, W98_LUZ);
        char n[16], id[20];
        pt_nome_particao(t->disco, i < 4 ? i + 1 : i + 1, n, sizeof n);
        if (pl2 > g_largura_texto(n) + 6) g_texto(j, px + 3, y + 7, n, W98_LUZ);
        if (zonas_part) { txt(id, sizeof id, "%s%d", pref, i + 1); zona(id, px, y, pl2, 30); }
    }
    if (zonas_part) {
        pt_livre_t v[24];
        int n = pt_livres(t, v, 24);
        for (int i = 0; i < n; i++) {
            if (v[i].logica) continue;
            char id[20];
            txt(id, sizeof id, "%slibre%d", pref, i);
            int px = x + (int)(v[i].inicio / esc_px), pl2 = (int)(v[i].setores / esc_px);
            if (pl2 < 3) pl2 = 3;
            zona(id, px, y, pl2, 30);
        }
    }
    g_bisel(j, x - 2, y - 2, l + 4, 34, true);
    return esc_px;
}

/* ------------------------------------------------------------ el plan */

static void ler_destinos(void)
{
    ndest = 0;
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1 && ndest < 12; i++) {
        if (d.particao) continue;
        copia_texto(destinos[ndest], d.nome, 12);
        destino_ok[ndest] = destino_valido(&d, destino_por[ndest], 96);
        ndest++;
    }
    if (sel_dest >= ndest || (sel_dest >= 0 && !destino_ok[sel_dest])) sel_dest = -1;
    for (int i = 0; i < ndest && sel_dest < 0; i++) if (destino_ok[i]) sel_dest = i;
}

/* Lo que ocupa una partición (MB del mínimo al que se puede reducir) y si se puede. */
static bool redutivel(int num, uint32_t *min, char *por, int max)
{
    pt_part_t *p = pt_achar(&tab, num);
    char n[16], tp[16];
    pt_nome_particao(tab.disco, num, n, sizeof n);
    *min = 0;
    if (!p || pt_tipo_extendida(p->tipo)) { txt(por, max, "no es una partición de datos"); return false; }
    if (p->tipo == 0x07) { txt(por, max, "%s es NTFS: SaguiOS no escribe en NTFS, así que no la reduce (redúcela desde Windows)", n); return false; }
    if (p->tipo == 0xDA || p->tipo == 0xEF) { txt(por, max, "%s es de arranque (SaguiOS o EFI): no se reduce", n); return false; }
    if (!fs_tamanho(n, tp, sizeof tp)) { txt(por, max, "%s no es FAT32 ni ext2: SaguiOS no sabe reducirla", n); return false; }
    *min = fs_minimo(n);
    if (!*min || *min + 256u * MB > p->setores) { txt(por, max, "%s está casi llena: no queda sitio que quitarle", n); return false; }
    return true;
}

/* Lo que hace falta como poco para instalar en un hueco (sistema, datos mínimos y configuración). */
static uint32_t minimo_instalar(void) { return 512u * MB; }

/* «Junto a lo que hay», automático: el hueco libre más grande si basta; si no, reducir la partición
 * de la que más se puede sacar (dejándole al menos el doble de lo que ocupa, o lo que se pueda). */
static void escolher_junto(void)
{
    esc = ES_NADA;
    tipo_aviso[0] = 0;
    if (!tab_ok || (tab.esquema != PT_MBR && tab.esquema != PT_SEM_TABELA)) { txt(tipo_aviso, sizeof tipo_aviso, "Este disco no tiene tabla MBR: solo se puede usar entero."); return; }
    pt_livre_t v[24];
    int n = pt_livres(&tab, v, 24);
    uint32_t mejor = 0;
    for (int i = 0; i < n; i++) if (!v[i].logica && v[i].setores > mejor) { mejor = v[i].setores; esc_hueco = v[i].inicio; esc_hueco_set = v[i].setores; }
    if (mejor >= 2048u * MB || (mejor >= minimo_instalar() && n)) { esc = ES_HUECO; return; }
    uint32_t ganho = 0;
    char por[160];
    for (int i = 1; i <= 4; i++) {
        uint32_t mn;
        if (!pt_achar(&tab, i) || !redutivel(i, &mn, por, sizeof por)) continue;
        pt_part_t *p = pt_achar(&tab, i);
        uint32_t deixar = mn * 2 > mn + 256u * MB ? mn * 2 : mn + 256u * MB;
        if (deixar >= p->setores) deixar = mn;
        uint32_t g = p->setores - deixar;
        if (g > ganho && g >= minimo_instalar()) { ganho = g; esc = ES_REDUCIR; esc_part = i; esc_min = mn; esc_max = p->setores; esc_red = (p->setores - g / 2 > deixar ? p->setores - g / 2 : deixar) / MB * MB; }
    }
    if (esc == ES_NADA) {
        if (tab.prim[0].tipo == 0x07 || tab.prim[1].tipo == 0x07) txt(tipo_aviso, sizeof tipo_aviso, "No hay espacio libre y la partición NTFS no se puede reducir desde SaguiOS: redúcela antes desde Windows.");
        else txt(tipo_aviso, sizeof tipo_aviso, "No hay espacio libre suficiente ni ninguna partición FAT32 o ext2 que se pueda reducir.");
    }
}

static uint32_t mb_caixa(const caixa_t *c)
{
    int v = 0;
    if (!texto_para_numero(c->texto, &v) || v < 0) return 0;
    return (uint32_t)v;
}

static void mb_para_caixa(caixa_t *c, uint32_t mb) { char t[16]; txt(t, sizeof t, "%u", mb); caixa_definir(c, t); }

/* El plan con todo lo elegido. con_tamanhos: con los cuadros de la página de tamaños. */
static bool montar_plano(bool com_tamanhos)
{
    memset(&pl, 0, sizeof pl);
    plano_erro[0] = 0;
    if (sel_dest < 0) { txt(plano_erro, sizeof plano_erro, "elige un disco"); return false; }
    copia_texto(pl.destino, destinos[sel_dest], sizeof pl.destino);
    pl.teclado = tecl;
    if (tipo == TI_ENTERO) pl.modo = M_ENTERO;
    else if (esc == ES_HUECO) { pl.modo = M_HUECO; pl.hueco_ini = esc_hueco; }
    else if (esc == ES_REDUCIR) { pl.modo = M_HUECO; pl.reduzir = true; pl.reduzir_part = esc_part; pl.reduzir_set = esc_red; }
    else if (esc == ES_SUSTITUIR) { pl.modo = M_PARTICION; pl.particao = esc_part; }
    else { txt(plano_erro, sizeof plano_erro, "elige dónde instalar"); return false; }
    pl.tabela = arranque == AR_UEFI ? T_GPT : T_MBR;
    pl.esp = arranque != AR_BIOS;
    pl.apps = cs_usa[D_APPS].marcada;
    pl.jogos = cs_usa[D_JOGOS].marcada;
    pl.tmp = cs_usa[D_TMP].marcada;
    pl.logs = cs_usa[D_LOGS].marcada;
    pl.copiar_jogos = cs_jogos.marcada && ha_jogos();
    pl.sem_copiar_apps = !cs_apps.marcada;
    pl.fs_home = fs_esc[D_HOME]; pl.fs_apps = fs_esc[D_APPS]; pl.fs_jogos = fs_esc[D_JOGOS]; pl.fs_tmp = fs_esc[D_TMP]; pl.fs_logs = fs_esc[D_LOGS];
    if (com_tamanhos) {
        if (!cs_resto.marcada) pl.home_mb = mb_caixa(&cx_mb[D_HOME]);
        if (pl.apps) pl.apps_mb = mb_caixa(&cx_mb[D_APPS]);
        if (pl.jogos) pl.jogos_mb = mb_caixa(&cx_mb[D_JOGOS]);
        if (pl.tmp) pl.tmp_mb = mb_caixa(&cx_mb[D_TMP]);
        if (pl.logs) pl.logs_mb = mb_caixa(&cx_mb[D_LOGS]);
    } else {
        info_disco_t d;
        bool grande = info_de(pl.destino, &d) && d.setores >= 8192u * MB;
        if (pl.tmp) pl.tmp_mb = grande ? 512 : 64;
        if (pl.logs) pl.logs_mb = grande ? 128 : 40;
    }
    copia_texto(pl.nome_equipo, cx_nome.texto, sizeof pl.nome_equipo);
    if (lz.sel >= 0 && lz.sel < nzonas) copia_texto(pl.zona, zonas[lz.sel].nome, sizeof pl.zona);
    copia_texto(pl.senha_root, cx_r1.texto, sizeof pl.senha_root);
    copia_texto(pl.senha_usuario, cx_u1.texto, sizeof pl.senha_usuario);
    pl.sshd = cs_sshd.marcada;
    pl.vncd = cs_vncd.marcada;
    for (int i = 0; i < 7; i++) if (cs_cc[i].marcada) pl.copiar_conf |= cc_bit[i];
    if (!(pl.copiar_conf & CC_IA)) pl.copiar_conf &= ~(unsigned)CC_IA_CHAVE;
    return planejar(&pl, plano_erro, sizeof plano_erro);
}

/* Los valores por defecto de la página de tamaños, según el disco (lo que elige el plan automático). */
static void tamanhos_por_defecto(void)
{
    info_disco_t d;
    bool grande = sel_dest >= 0 && info_de(destinos[sel_dest], &d) && d.setores >= 8192u * MB;
    cs_usa[D_HOME].marcada = true;
    cs_usa[D_APPS].marcada = true;
    cs_usa[D_JOGOS].marcada = true;
    cs_usa[D_TMP].marcada = grande;
    cs_usa[D_LOGS].marcada = grande;
    cs_resto.marcada = true;
    for (int i = 0; i < N_DADOS; i++) fs_esc[i] = PA_FS_EXT2;
    if (montar_plano(false)) {
        mb_para_caixa(&cx_mb[D_HOME], pl.home_set / MB);
        mb_para_caixa(&cx_mb[D_APPS], pl.apps ? pl.apps_set / MB : 64);
        mb_para_caixa(&cx_mb[D_JOGOS], pl.jogos ? pl.jogos_set / MB : 64);
        cs_usa[D_APPS].marcada = pl.apps;
        cs_usa[D_JOGOS].marcada = pl.jogos;
    } else {
        mb_para_caixa(&cx_mb[D_HOME], 256);
        mb_para_caixa(&cx_mb[D_APPS], 64);
        mb_para_caixa(&cx_mb[D_JOGOS], 64);
    }
    mb_para_caixa(&cx_mb[D_TMP], grande ? 512 : 64);
    mb_para_caixa(&cx_mb[D_LOGS], grande ? 128 : 40);
    tamanhos_prontos = true;
}

/* ------------------------------------------------------------ las páginas */

static void cabecera(const char *titulo, const char *sub)
{
    negrito(CX0, 14, titulo, W98_NEGRO);
    if (sub) texto_linhas(CX0, 36, sub, CXL / 8, W98_NEGRO);
}

static void franja(void)
{
    for (int y = 0; y < AJ_A - 52; y++) {   /* degradado azul, como el asistente de Windows 98 */
        uint32_t b = 0x80 + (uint32_t)(y * 0x50 / (AJ_A - 52));
        g_retangulo(j, 0, y, FRANJA, 1, (0x00u << 16) | ((uint32_t)(0x30 + y * 0x30 / (AJ_A - 52)) << 8) | b);
    }
    negrito(14, 18, "SaguiOS", W98_LUZ);
    g_texto(j, 14, 38, "Instalación", 0xD8E4F8);
    for (int i = 0; i < P_PROGRESSO; i++) {
        bool at = i == pagina || (pagina >= P_PROGRESSO && i == P_RESUMEN);
        uint32_t c = i < pagina ? 0xB8D0F0 : at ? W98_LUZ : 0x8FA6C8;
        char t[40];
        static const char *const curto[P_PROGRESSO] = { "Bienvenida", "Disco", "Tipo", "Tamaños", "Opciones", "Configuración", "Resumen" };
        txt(t, sizeof t, "%d %s", i + 1, curto[i]);
        if (at) negrito(14, 84 + i * 24, t, c);
        else g_texto(j, 14, 84 + i * 24, t, c);
    }
}

static void pagina_bienvenida(void)
{
    cabecera("Bienvenido al asistente de instalación de SaguiOS",
             "Este asistente copia el SaguiOS que está funcionando ahora a un disco duro, un SSD o un pendrive, "
             "para que arranque desde allí y guarde lo que hagas. Antes de borrar o cambiar nada te enseña un resumen "
             "y te pide confirmación.");
    char t[160], c[64];
    int y = 120;
    negrito(CX0, y, "Lo que se ha detectado", W98_NEGRO);
    y += 24;
    quadro(CX0, y, CXL, 210);
    int yy = y + 10;
    nome_cpu(c, sizeof c);
    info_nucleo_t nn[16];
    int ncpu = info_sistema(INFO_NUCLEOS, nn, 16);
    txt(t, sizeof t, "Procesador:  %s%s", c, ncpu > 1 ? "" : "");
    g_texto(j, CX0 + 10, yy, t, W98_NEGRO); yy += 20;
    if (ncpu > 1) { txt(t, sizeof t, "             %d núcleos", ncpu); g_texto(j, CX0 + 10, yy, t, W98_NEGRO); yy += 20; }
    info_memoria_t m;
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) >= 0) { txt(t, sizeof t, "Memoria:     %u MB", m.ram_total_kb / 1024); g_texto(j, CX0 + 10, yy, t, W98_NEGRO); yy += 20; }
    txt(t, sizeof t, "Arranque:    %s", arrancou_por_uefi() ? "UEFI (se recomienda instalar con GPT o con los dos)" : "BIOS (o UEFI en modo heredado)");
    g_texto(j, CX0 + 10, yy, t, W98_NEGRO); yy += 20;
    txt(t, sizeof t, "Origen:      %s%s", origem_txt[0] ? origem_txt : "no se sabe (no se puede instalar)", pl.desde_cd ? " (CD)" : "");
    g_texto(j, CX0 + 10, yy, t, origem_txt[0] ? W98_NEGRO : 0xC00000); yy += 26;
    g_texto(j, CX0 + 10, yy, "Discos:", W98_NEGRO);
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1 && yy < y + 190; i++) {
        if (d.particao) continue;
        char tm[16];
        tamanho(d.setores, tm, sizeof tm);
        txt(t, sizeof t, "%s  %s, %s%s", d.nome, tm, tipo_disco(&d), (d.flags & DISCO_ARRANQUE_REAL) ? " (el de arranque)" : "");
        g_texto(j, CX0 + 90, yy, t, W98_NEGRO);
        yy += 18;
    }
    texto_linhas(CX0, y + 222, "Requisitos: un disco de al menos 160 MB que no sea el de arranque. Para seguir, pulsa Siguiente.", CXL / 8, W98_NEGRO);
}

static void pagina_disco(void)
{
    cabecera("Disco de destino", "Elige el disco donde instalar SaguiOS. El disco del que ha arrancado el sistema no se puede elegir.");
    int y = 78;
    for (int i = 0; i < ndest && y < BOT_Y - 70; i++) {
        info_disco_t d;
        if (!info_de(destinos[i], &d)) continue;
        bool sel = i == sel_dest;
        int alt = 74;
        g_retangulo(j, CX0, y, CXL, alt, sel ? 0xC8D8F0 : W98_FACE);
        g_bisel(j, CX0, y, CXL, alt, sel);
        char tm[16], l1[160], id[16];
        tamanho(d.setores, tm, sizeof tm);
        txt(l1, sizeof l1, "%s  -  %s %s", d.nome, d.fabricante, d.produto);
        negrito(CX0 + 8, y + 6, l1, destino_ok[i] ? W98_NEGRO : W98_SOMBRA);
        txt(l1, sizeof l1, "%s, %s%s%s", tm, tipo_disco(&d), destino_ok[i] ? "" : "  -  no: ", destino_ok[i] ? "" : destino_por[i]);
        g_texto(j, CX0 + 8, y + 24, l1, destino_ok[i] ? W98_NEGRO : 0xA00000);
        pt_tabela_t t;
        if (pt_ler(d.nome, &t) == 0) barra_disco(&t, CX0 + 10, y + 42, CXL - 20, "", false);
        txt(id, sizeof id, "disco%d", i);
        if (destino_ok[i]) zona(id, CX0, y, CXL, alt);
        y += alt + 8;
    }
    g_texto(j, CX0, BOT_Y - 30, "Colores: naranja SaguiOS, azul FAT32, verde Linux/ext2, violeta NTFS, rayado libre.", W98_SOMBRA);
}

static void pagina_tipo(void)
{
    char s[200];
    txt(s, sizeof s, "Disco %s. ¿Cómo quieres usarlo?", destinos[sel_dest]);
    cabecera("Tipo de instalación", s);
    int y = 64;
    radio98("tipo-entero", CX0, y, tipo == TI_ENTERO, "Usar todo el disco (se borra todo lo que tiene)", true);
    radio98("tipo-junto", CX0, y + 22, tipo == TI_JUNTO, "Junto a lo que hay (espacio libre o reducir una partición)", true);
    radio98("tipo-avanzado", CX0, y + 44, tipo == TI_AVANZADO, "Avanzado: elegir en la barra qué hueco usar, reducir o sustituir", true);
    y += 76;
    if (!tab_ok) { g_texto(j, CX0, y, "No se puede leer la tabla de este disco.", 0xC00000); return; }
    int bx = CX0 + 4, bw = CXL - 8, by = y;
    uint32_t px = barra_disco(&tab, bx, by, bw, "parte", tipo == TI_AVANZADO);
    y += 44;
    if (tipo == TI_ENTERO) {
        for (int k = 0; k < bw; k += 4) g_linha(j, bx + k, by + 29, bx + k + 4 < bx + bw ? bx + k + 4 : bx + bw - 1, by, 0xC00000);
        char perde[300] = "";
        for (int i = 0; i < 4 + tab.nlog; i++) {
            const pt_part_t *p = i < 4 ? &tab.prim[i] : &tab.log[i - 4];
            if (!p->tipo || pt_tipo_extendida(p->tipo)) continue;
            char n[16], tm[16], x[80];
            info_disco_t q;
            pt_nome_particao(tab.disco, i + 1, n, sizeof n);
            tamanho(p->setores, tm, sizeof tm);
            if (info_de(n, &q) && q.suportado) txt(x, sizeof x, "%s%s (%s, %s \"%s\")", perde[0] ? ", " : "", n, tm, q.formato, q.rotulo);
            else txt(x, sizeof x, "%s%s (%s, %s)", perde[0] ? ", " : "", n, tm, pt_nome_tipo(p->tipo));
            junta_texto(perde, x, sizeof perde);
        }
        char t[400];
        txt(t, sizeof t, "SE BORRA TODO: %s.", perde[0] ? perde : "el disco está vacío");
        y = texto_linhas(CX0, y, t, CXL / 8, 0xC00000) + 8;
        txt(t, sizeof t, "Para confirmar que es el disco correcto, escribe su nombre (%s):", destinos[sel_dest]);
        g_texto(j, CX0, y, t, W98_NEGRO);
        caixa98("confirmar", &cx_confirma, CX0, y + 20, 160);
        bool ok = compara_texto(cx_confirma.texto, destinos[sel_dest]) == 0;
        g_texto(j, CX0 + 172, y + 24, ok ? "correcto" : "", 0x007000);
        return;
    }
    /* junto / avanzado */
    if (esc == ES_REDUCIR) {
        pt_part_t *p = pt_achar(&tab, esc_part);
        int hx = bx + (int)((p->inicio + esc_red) / px);
        int fx = bx + (int)((p->inicio + p->setores) / px);
        g_retangulo(j, hx, by, fx - hx, 30, 0xE8891C);   /* lo que pasa a SaguiOS */
        g_texto(j, hx + 3, by + 7, fx - hx > 70 ? "SaguiOS" : "", W98_LUZ);
        g_retangulo(j, hx - 3, by - 6, 7, 42, W98_FACE);   /* el tirador */
        g_bisel(j, hx - 3, by - 6, 7, 42, arrastando);
        if (tipo != TI_AVANZADO) zona("barra", bx, by, bw, 30);   /* pulsar en la barra lleva el tirador allí */
        zona("tirador", hx - 8, by - 6, 17, 42);
        char n[16], t[300], tr[16], tg[16], tm[16];
        pt_nome_particao(tab.disco, esc_part, n, sizeof n);
        tamanho(esc_red, tr, sizeof tr);
        tamanho(p->setores - esc_red, tg, sizeof tg);
        tamanho(esc_min, tm, sizeof tm);
        txt(t, sizeof t, "%s se reduce a %s (como mínimo %s, por lo que ocupan sus archivos, que se conservan; antes se comprueba). SaguiOS tendrá %s. "
                         "Mueve el tirador o escribe el tamaño nuevo de %s en MB:", n, tr, tm, tg, n);
        y = texto_linhas(CX0, y, t, CXL / 8, W98_NEGRO) + 4;
        caixa98("reducir-mb", &cx_red, CX0, y, 100);
        g_texto(j, CX0 + 108, y + 4, "MB", W98_NEGRO);
        y += 34;
    } else if (esc == ES_HUECO) {
        int hx = bx + (int)(esc_hueco / px), hl = (int)(esc_hueco_set / px);
        if (hl < 4) hl = 4;
        g_retangulo(j, hx, by, hl, 30, 0xF2B66D);
        g_texto(j, hx + 3, by + 7, hl > 70 ? "SaguiOS" : "", W98_NEGRO);
        char t[200], tm[16];
        tamanho(esc_hueco_set, tm, sizeof tm);
        txt(t, sizeof t, "Se usa el espacio libre de %s (desde %u MB). No se borra ni se reduce nada.", tm, esc_hueco / MB);
        y = texto_linhas(CX0, y, t, CXL / 8, W98_NEGRO) + 8;
    } else if (esc == ES_SUSTITUIR) {
        char n[16], t[200];
        pt_nome_particao(tab.disco, esc_part, n, sizeof n);
        txt(t, sizeof t, "Se BORRA %s y SaguiOS ocupa su sitio.", n);
        y = texto_linhas(CX0, y, t, CXL / 8, 0xC00000) + 8;
    }
    if (tipo_aviso[0]) y = texto_linhas(CX0, y, tipo_aviso, CXL / 8, 0xA05000) + 6;
    if (tipo == TI_AVANZADO) {
        y = texto_linhas(CX0, y, "Pulsa en la barra una partición o un hueco libre (rayado). Luego elige qué hacer con ella:", CXL / 8, W98_NEGRO) + 4;
        uint32_t mn;
        char por[160];
        botao98("av-reducir", CX0, y, 150, "Reducir y usar", sel_part > 0 && redutivel(sel_part, &mn, por, sizeof por));
        botao98("av-sustituir", CX0 + 158, y, 150, "Sustituir (borrar)", sel_part > 0 && sel_part <= 4 && pt_achar(&tab, sel_part) &&
                pt_achar(&tab, sel_part)->tipo != 0xDA && !pt_tipo_extendida(pt_achar(&tab, sel_part)->tipo));
        y += 34;
    }
    texto_linhas(CX0, BOT_Y - 48, "NTFS (Windows) no se reduce: SaguiOS no escribe en NTFS. Su espacio libre sí se puede usar.", CXL / 8, W98_SOMBRA);
}

static void pagina_tamanos(void)
{
    cabecera("Tamaños de las particiones", "Cada partición con su tamaño en MB y su sistema de archivos (pulsa ext2/FAT32 para cambiarlo). "
             "FAT32 se lee también desde Windows; ext2 guarda mejor los permisos.");
    int y = 98;
    g_texto(j, CX0 + 24, y, "Partición", W98_SOMBRA);
    g_texto(j, CX0 + 200, y, "MB", W98_SOMBRA);
    g_texto(j, CX0 + 300, y, "Sistema", W98_SOMBRA);
    y += 22;
    bool gpt = arranque == AR_UEFI && tipo == TI_ENTERO;
    for (int i = 0; i < N_DADOS; i++, y += 32) {
        char id[20];
        bool ativo = i == D_HOME || !gpt;
        txt(id, sizeof id, "usa-%s", id_dado[i]);
        if (i == D_HOME) { cs_usa[i].marcada = true; g_texto(j, CX0 + 24, y + 4, nome_dado[i], W98_NEGRO); }
        else marcar98(id, &cs_usa[i], CX0, y + 4, nome_dado[i], ativo);
        bool caixa_ativa = cs_usa[i].marcada && ativo && !(i == D_HOME && cs_resto.marcada);
        txt(id, sizeof id, "mb-%s", id_dado[i]);
        if (caixa_ativa) caixa98(id, &cx_mb[i], CX0 + 200, y, 80);
        else { g_retangulo(j, CX0 + 200, y, 80, 24, W98_FACE); g_bisel(j, CX0 + 200, y, 80, 24, true); g_texto(j, CX0 + 206, y + 4, cx_mb[i].texto, W98_SOMBRA); }
        txt(id, sizeof id, "fs-%s", id_dado[i]);
        botao98(id, CX0 + 300, y - 1, 80, fs_esc[i] == PA_FS_FAT32 ? "FAT32" : "ext2", cs_usa[i].marcada && ativo);
    }
    marcar98("resto", &cs_resto, CX0, y + 4, "Usar el espacio restante para /home", true);
    y += 30;
    if (gpt) y = texto_linhas(CX0, y, "Con arranque solo UEFI (GPT) el sistema ve cuatro particiones: EFI, SaguiOS, /home y la configuración. "
                                     "Para tener /apps y juegos, elige «BIOS y UEFI» en Opciones.", CXL / 8, 0xA05000) + 4;
    bool ok = montar_plano(true);
    if (ok && cs_resto.marcada) mb_para_caixa(&cx_mb[D_HOME], pl.home_set / MB);   /* lo que le queda a /home */
    quadro(CX0, y, CXL, BOT_Y - 16 - y);
    if (!ok) texto_linhas(CX0 + 8, y + 8, plano_erro, CXL / 8 - 2, 0xC00000);
    else {
        char d[300];
        descrever_plano(&pl, d, sizeof d);
        texto_linhas(CX0 + 8, y + 8, d, CXL / 8 - 2, W98_NEGRO);
    }
}

static void pagina_opcoes(void)
{
    cabecera("Opciones", NULL);
    int y = 40;
    negrito(CX0, y, "Arranque", W98_NEGRO);
    bool so_entero = tipo == TI_ENTERO;
    bool efi = efi_disponivel();
    radio98("ar-bios", CX0 + 8, y + 20, arranque == AR_BIOS, "BIOS (MBR)", true);
    radio98("ar-uefi", CX0 + 130, y + 20, arranque == AR_UEFI, "UEFI (GPT)", so_entero);
    radio98("ar-ambos", CX0 + 252, y + 20, arranque == AR_AMBOS, "BIOS y UEFI (híbrida)", efi);
    y += 46;
    negrito(CX0, y, "Idioma y teclado", W98_NEGRO);
    g_texto(j, CX0 + 8, y + 20, "Idioma: Español (otros idiomas llegarán con la D313)", W98_NEGRO);
    radio98("tec-es", CX0 + 8, y + 40, tecl == TECLADO_ES, "Teclado español", true);
    radio98("tec-abnt2", CX0 + 170, y + 40, tecl == TECLADO_ABNT2, "Portugués de Brasil (ABNT2)", true);
    y += 66;
    negrito(CX0, y, "Zona horaria", W98_NEGRO);
    lz.x = CX0 + 8; lz.y = y + 18; lz.l = 250; lz.a = 4 * LISTA_ALTURA_ITEM;
    lista_desenhar(j, &lz);
    zona("zona", lz.x, lz.y, lz.l, lz.a);
    int x2 = CX0 + 280;
    negrito(x2, y, "Nombre del equipo", W98_NEGRO);
    caixa98("nombre", &cx_nome, x2, y + 18, 150);
    y += 18 + lz.a + 10;
    negrito(CX0, y, "Contraseñas (vacías: se fijan al entrar la primera vez)", W98_NEGRO);
    g_texto(j, CX0 + 8, y + 24, "root:", W98_NEGRO);
    caixa98("clave-root", &cx_r1, CX0 + 90, y + 20, 120);
    caixa98("clave-root2", &cx_r2, CX0 + 220, y + 20, 120);
    g_texto(j, CX0 + 8, y + 52, "sagui:", W98_NEGRO);
    caixa98("clave-usuario", &cx_u1, CX0 + 90, y + 48, 120);
    caixa98("clave-usuario2", &cx_u2, CX0 + 220, y + 48, 120);
    g_texto(j, CX0 + 350, y + 24, "(repítela)", W98_SOMBRA);
    y += 80;
    negrito(CX0, y, "Servicios", W98_NEGRO);
    marcar98("sshd", &cs_sshd, CX0 + 8, y + 20, "Servidor SSH (sshd)", true);
    marcar98("vncd", &cs_vncd, CX0 + 200, y + 20, "Escritorio remoto (VNC)", true);
    y += 44;
    negrito(CX0, y, "Qué copiar", W98_NEGRO);
    char t[96];
    txt(t, sizeof t, "Aplicaciones de /apps (%u MB)", ha_apps() ? kb_apps() / 1024 + 1 : 0);
    marcar98("copiar-apps", &cs_apps, CX0 + 8, y + 20, t, ha_apps());
    txt(t, sizeof t, "Datos de juegos (%u MB)", ha_jogos() ? kb_jogos(true) / 1024 + 1 : 0);
    marcar98("copiar-jogos", &cs_jogos, CX0 + 260, y + 20, t, ha_jogos());
    if (plano_erro[0]) texto_linhas(CX0, BOT_Y - 30, plano_erro, CXL / 8, 0xC00000);
}

static void pagina_config(void)
{
    cabecera("Copiar la configuración de esta sesión", "Lo marcado se copia al /home del disco instalado. Los secretos (claves SSH, "
             "contraseñas del correo, la clave de la IA) solo los lee quien entre en ese sistema; aun así, piensa si quieres llevarlos.");
    static const char *const nomes[7] = { "Preferencias del escritorio y de Configuración (/home/.config)", "Redes Wi-Fi guardadas",
        "Claves SSH (/home/.ssh)  -  AVISO: son secretas", "Cuentas de correo (y sus mensajes)", "Agentes de IA y su configuración",
        "    incluida la clave de la API de la IA", "Documentos de /home" };
    int y = 100;
    for (int i = 0; i < 7; i++, y += 30) {
        unsigned b = cc_bit[i];
        uint32_t kb = kb_copia(b == CC_IA_CHAVE ? CC_IA | CC_IA_CHAVE : b) - (b == CC_IA_CHAVE ? kb_copia(CC_IA) : 0);
        bool hay = b == CC_WIFI ? existe("/home/.config/wifi.conf") : b == CC_SSH ? existe("/home/.ssh")
                 : b == CC_CORREO ? existe("/home/.config/correo") || existe("/home/correo")
                 : b == CC_IA ? existe("/home/.config/agentes") || existe("/home/.config/ia")
                 : b == CC_IA_CHAVE ? existe("/home/.config/ia/chave") && cs_cc[4].marcada : kb > 0;
        if (!hay) cs_cc[i].marcada = false;
        char t[120];
        txt(t, sizeof t, "%s%s", nomes[i], hay ? "" : "  (no hay)");
        marcar98(cc_id[i], &cs_cc[i], CX0, y, t, hay);
        if (hay) { char tm[24]; txt(tm, sizeof tm, kb >= 1024 ? "%u MB" : "%u KB", kb >= 1024 ? kb / 1024 : kb); g_texto(j, AJ_L - 90, y, tm, W98_SOMBRA); }
    }
    char t[80];
    unsigned cc = 0;
    for (int i = 0; i < 7; i++) if (cs_cc[i].marcada) cc |= cc_bit[i];
    uint32_t kb = kb_copia(cc);
    txt(t, sizeof t, "En total: %u %s", kb >= 1024 ? kb / 1024 : kb, kb >= 1024 ? "MB" : "KB");
    negrito(CX0, y + 10, t, W98_NEGRO);
}

static void resumo(char *s, int max)
{
    char x[400], tm[16];
    info_disco_t d;
    info_de(pl.destino, &d);
    tamanho(d.setores, tm, sizeof tm);
    txt(s, max, "Disco de destino: %s (%s %s, %s, %s).\n\n", pl.destino, d.fabricante, d.produto, tm, tipo_disco(&d));
    if (pl.reduzir) { txt(x, sizeof x, "SE REDUCE: %s.\n", pl.reduzir_txt); junta_texto(s, x, (size_t)max); }
    txt(x, sizeof x, pl.modo == M_HUECO ? "SE BORRA: nada (se usa espacio libre).\n" : "SE BORRA: %s.\n", pl.perde);
    junta_texto(s, x, (size_t)max);
    if (pl.modo != M_ENTERO)
        junta_texto(s, "AVISO: el arranque por BIOS de este disco pasa a ser el de SaguiOS: el otro sistema sigue ahí, pero no "
                       "arrancará desde este disco hasta que se restaure su cargador.\n", (size_t)max);
    char d2[300];
    descrever_plano(&pl, d2, sizeof d2);
    txt(x, sizeof x, "\nSE CREA: %s%s.\n", d2, pl.tabela == T_GPT ? "" : "  |  tabla MBR");
    junta_texto(s, x, (size_t)max);
    if (pl.aviso[0]) { txt(x, sizeof x, "AVISO: %s.\n", pl.aviso); junta_texto(s, x, (size_t)max); }
    txt(x, sizeof x, "\nSE COPIA: el sistema que está funcionando (de %s)%s%s", pl.origem, pl.apps && !pl.sem_copiar_apps ? ", las aplicaciones de /apps" : "",
        pl.copiar_jogos ? ", los datos de juegos" : "");
    junta_texto(s, x, (size_t)max);
    if (pl.copiar_conf) {
        uint32_t kb = kb_copia(pl.copiar_conf);
        txt(x, sizeof x, "; y de esta sesión (%u KB):", kb);
        junta_texto(s, x, (size_t)max);
        static const char *const nome_cc[] = { "preferencias", "redes Wi-Fi", "claves SSH", "correo", "agentes de IA", "", "documentos" };
        bool prim = true;
        for (int i = 0; i < 7; i++) {
            if (!(pl.copiar_conf & cc_bit[i]) || cc_bit[i] == CC_IA_CHAVE) continue;
            txt(x, sizeof x, "%s %s%s", prim ? "" : ",", nome_cc[i], cc_bit[i] == CC_IA ? ((pl.copiar_conf & CC_IA_CHAVE) ? " (con la clave)" : " (sin la clave)") : "");
            junta_texto(s, x, (size_t)max);
            prim = false;
        }
    }
    junta_texto(s, ".\n", (size_t)max);
    txt(x, sizeof x, "\nOPCIONES: arranque %s; equipo \"%s\"; zona %s; teclado %s; root %s, sagui %s; sshd %s, VNC %s.",
        pl.tabela == T_GPT ? "UEFI (GPT)" : pl.esp ? "BIOS y UEFI" : "BIOS", pl.nome_equipo[0] ? pl.nome_equipo : "sagui", pl.zona[0] ? pl.zona : "la de ahora",
        pl.teclado == TECLADO_ABNT2 ? "ABNT2" : "español", pl.senha_root[0] ? "con contraseña" : "sin contraseña todavía",
        pl.senha_usuario[0] ? "con contraseña" : "sin contraseña todavía", pl.sshd ? "encendido" : "apagado", pl.vncd ? "encendido" : "apagado");
    junta_texto(s, x, (size_t)max);
}

static void pagina_resumo(void)
{
    cabecera("Resumen", "Esto es lo que se va a hacer. Léelo con calma: lo que dice «SE BORRA» no se puede recuperar.");
    int y = 76;
    quadro(CX0, y, CXL, BOT_Y - 16 - y);
    if (plano_erro[0]) { texto_linhas(CX0 + 8, y + 8, plano_erro, CXL / 8 - 2, 0xC00000); return; }
    char s[1800];
    resumo(s, sizeof s);
    texto_linhas(CX0 + 8, y + 8, s, CXL / 8 - 2, W98_NEGRO);
}

static void pagina_progresso(void)
{
    cabecera(pagina == P_FIM ? (resultado_ok ? "SaguiOS está instalado" : "La instalación ha fallado")
                             : "Instalando SaguiOS...",
             pagina == P_FIM ? NULL : "No apagues el ordenador ni quites ningún disco.");
    int y = 70;
    for (int i = 0; i < FASES; i++, y += 20) {
        uint32_t c = i < prog_fase || (pagina == P_FIM && resultado_ok) ? 0x007000 : i == prog_fase ? W98_AZUL : W98_SOMBRA;
        g_texto(j, CX0, y, i < prog_fase || (pagina == P_FIM && resultado_ok) ? "v" : i == prog_fase ? ">" : " ", c);
        g_texto(j, CX0 + 16, y, nome_fase[i], c);
    }
    y += 6;
    int total = pagina == P_FIM && resultado_ok ? 100 : (prog_fase * 100 + prog_pc) / FASES;
    g_retangulo(j, CX0, y, CXL, 22, W98_LUZ);
    g_bisel(j, CX0, y, CXL, 22, true);
    for (int b = 0; b < (CXL - 6) * total / 100; b += 10) g_retangulo(j, CX0 + 3 + b, y + 3, b + 8 < CXL - 6 ? 8 : CXL - 6 - b, 16, W98_AZUL);
    y += 32;
    quadro(CX0, y, CXL, BOT_Y - 16 - y);
    if (pagina == P_FIM) {   /* al final, el resultado en el recuadro (el registro está en el log del sistema) */
        texto_linhas(CX0 + 8, y + 6, resultado, CXL / 8 - 2, resultado_ok ? 0x005000 : 0xC00000);
        return;
    }
    const char *log = prog_log;
    int linhas = 0;
    for (const char *p = prog_log; *p; p++) if (*p == '\n') linhas++;
    int cabem = (BOT_Y - 26 - y) / 17;
    while (linhas > cabem && *log) { const char *f = acha_char(log, '\n'); if (!f) break; log = f + 1; linhas--; }
    texto_linhas(CX0 + 8, y + 6, log, CXL / 8 - 2, W98_NEGRO);
}

static void desenhar(void)
{
    nzonas_ui = 0;
    g_retangulo(j, 0, 0, AJ_L, AJ_A, W98_FACE);
    franja();
    g_linha(j, 8, AJ_A - 52, AJ_L - 8, AJ_A - 52, W98_SOMBRA);
    g_linha(j, 8, AJ_A - 51, AJ_L - 8, AJ_A - 51, W98_LUZ);
    switch (pagina) {
    case P_BIENVENIDA: pagina_bienvenida(); break;
    case P_DISCO: pagina_disco(); break;
    case P_TIPO: pagina_tipo(); break;
    case P_TAMANOS: pagina_tamanos(); break;
    case P_OPCIONES: pagina_opcoes(); break;
    case P_CONFIG: pagina_config(); break;
    case P_RESUMEN: pagina_resumo(); break;
    default: pagina_progresso(); break;
    }
    if (pagina < P_PROGRESSO) {
        bool pode = true;
        if (pagina == P_BIENVENIDA) pode = origem_txt[0] != 0;
        if (pagina == P_DISCO) pode = sel_dest >= 0;
        if (pagina == P_TIPO) pode = tipo == TI_ENTERO ? compara_texto(cx_confirma.texto, destinos[sel_dest]) == 0 : esc != ES_NADA;
        if (pagina == P_TAMANOS || pagina == P_RESUMEN) pode = !plano_erro[0];
        botao98("atras", AJ_L - 330, BOT_Y, 96, "< Atrás", pagina > P_BIENVENIDA);
        botao98("siguiente", AJ_L - 230, BOT_Y, 96, pagina == P_RESUMEN ? "Instalar" : "Siguiente >", pode);
        botao98("cancelar", AJ_L - 112, BOT_Y, 96, "Cancelar", true);
    } else if (pagina == P_FIM) {
        if (resultado_ok) botao98("reiniciar", AJ_L - 290, BOT_Y, 160, "Reiniciar ahora", true);
        botao98("cerrar", AJ_L - 112, BOT_Y, 96, "Cerrar", true);
    }
    jan_mostrar(j);
    /* Se escribe cuando cambia la página o lo que se puede pulsar (una suma de las zonas). */
    static uint32_t soma_logada;
    uint32_t soma = (uint32_t)nzonas_ui;
    for (int i = 0; i < nzonas_ui; i++) {
        for (const char *c = zonas_ui[i].id; *c; c++) soma = soma * 31 + (uint8_t)*c;
        soma = soma * 31 + (uint32_t)(zonas_ui[i].x * 7 + zonas_ui[i].y * 13 + zonas_ui[i].l);
    }
    if (pagina != pagina_logada || layout_mudou || soma != soma_logada) {
        soma_logada = soma;
        registrar("instalar: pagina %d %s", pagina, nome_pagina[pagina]);
        for (int i = 0; i < nzonas_ui; i++)
            registrar("instalar: zona %s %d %d %d %d", zonas_ui[i].id, zonas_ui[i].x, zonas_ui[i].y, zonas_ui[i].l, zonas_ui[i].a);
        if (pagina == P_TAMANOS || pagina == P_RESUMEN) registrar("instalar: plan %s | erro=%s", plano_erro[0] ? "-" : "ok", plano_erro);
        registrar("instalar: fin de zonas %d", pagina);
        pagina_logada = pagina;
        layout_mudou = false;
    }
}

/* ------------------------------------------------------------ la instalación desde la ventana */

static void log_linha(const char *t)
{
    if (!t || !*t) return;
    if (tam_texto(prog_log) + tam_texto(t) + 2 >= sizeof prog_log) {   /* se descarta lo más viejo */
        const char *f = acha_char(prog_log + sizeof prog_log / 2, '\n');
        if (f) memmove(prog_log, f + 1, tam_texto(f + 1) + 1); else prog_log[0] = 0;
    }
    junta_texto(prog_log, t, sizeof prog_log);
    junta_texto(prog_log, "\n", sizeof prog_log);
}

static void inf_janela(int f, int pc, const char *t)
{
    if (f != prog_fase) log_linha(nome_fase[f]);
    prog_fase = f;
    prog_pc = pc > 100 ? 100 : pc;
    if (t) log_linha(t);
    desenhar();
    registrar("instalar: fase %d %d%%", f + 1, prog_pc);
}

static void comecar(void)
{
    char t[500], q[700];
    txt(t, sizeof t, pl.modo == M_HUECO ? "Se va a instalar SaguiOS en %s%s%s.\n\n¿Instalar ahora?"
                                         : "Se va a instalar SaguiOS en %s.\nSE BORRA: %s.\n\n¿Instalar ahora?",
        pl.destino, pl.modo == M_HUECO ? (pl.reduzir ? ". Antes, " : ", en el espacio libre") : pl.perde, pl.modo == M_HUECO && pl.reduzir ? pl.reduzir_txt : "");
    quebrar(t, q, sizeof q, 44);
    if (!confirmar("Instalar SaguiOS", q)) return;
    info_disco_t d;
    if (info_de(pl.destino, &d) && d.montado) {
        txt(t, sizeof t, "%s tiene particiones montadas. ¿Desmontarlas para instalar?", pl.destino);
        quebrar(t, q, sizeof q, 44);
        if (!confirmar("Instalar SaguiOS", q)) return;
        desmontar(pl.destino);
    }
    pagina = P_PROGRESSO;
    prog_fase = 0; prog_pc = 0; prog_log[0] = 0;
    layout_mudou = true;
    desenhar();
    informar = inf_janela;
    char e[300];
    registrar("instalar: comienza en %s", pl.destino);
    int r = instalar(&pl, e, sizeof e);
    resultado_ok = r >= 0;
    char d2[300];
    descrever_plano(&pl, d2, sizeof d2);
    if (resultado_ok)
        txt(resultado, sizeof resultado, "SaguiOS se ha instalado en %s y se ha releído entero para compararlo con el original.\n%s\n%s"
            "Para usarlo: %s y arranca desde %s (o pulsa «Reiniciar ahora» y elige ese disco en la BIOS).",
            pl.destino, d2, pl.copiar_conf ? "Se ha copiado la configuración elegida.\n" : "",
            pl.desde_cd ? "saca el CD" : "quita el disco de arranque actual", pl.destino);
    else
        txt(resultado, sizeof resultado, "Error en \"%s\": %s.\n\nEl disco %s NO ARRANCA tal como ha quedado. Se puede repetir la instalación.%s",
            nome_fase[prog_fase], e, pl.destino, pl.reduzir ? " La partición reducida conserva sus archivos." : "");
    memset(pl.senha_root, 0, sizeof pl.senha_root);
    memset(pl.senha_usuario, 0, sizeof pl.senha_usuario);
    registrar("instalar: resultado %d: %s", r, resultado);
    pagina = P_FIM;
    layout_mudou = true;
    desenhar();
}

/* ------------------------------------------------------------ eventos */

static void entrar_tipo(void)
{
    tab_ok = pt_ler(destinos[sel_dest], &tab) == 0;
    bool vazio = true;
    for (int i = 0; tab_ok && i < 4 + tab.nlog; i++) { const pt_part_t *p = i < 4 ? &tab.prim[i] : &tab.log[i - 4]; if (p->tipo) vazio = false; }
    tipo = vazio || !tab_ok || (tab.esquema != PT_MBR && tab.esquema != PT_SEM_TABELA) ? TI_ENTERO : TI_JUNTO;
    caixa_definir(&cx_confirma, "");
    if (tipo == TI_JUNTO) escolher_junto();
    if (esc == ES_REDUCIR) mb_para_caixa(&cx_red, esc_red / MB);
    tamanhos_prontos = false;
}

/* El tirador: la x de la ventana -> tamaño nuevo de la partición que se reduce. */
static void mover_tirador(int x)
{
    pt_part_t *p = pt_achar(&tab, esc_part);
    if (!p) return;
    int bx = CX0 + 4, bw = CXL - 8;
    uint32_t px = (tab.setores ? tab.setores : 1) / (uint32_t)bw + 1;
    int64_t pos = (int64_t)(x - bx) * px;
    int64_t novo = pos - (int64_t)p->inicio;
    uint32_t lo = esc_min, hi = p->setores > minimo_instalar() ? p->setores - minimo_instalar() : esc_min;
    if (novo < (int64_t)lo) novo = lo;
    if (novo > (int64_t)hi) novo = hi;
    esc_red = (uint32_t)novo / MB * MB;
    if (esc_red < esc_min) esc_red = (esc_min + MB - 1) / MB * MB;
    mb_para_caixa(&cx_red, esc_red / MB);
}

/* Avanzado: pulsar en la barra. Un hueco se usa tal cual; una partición FAT32/ext2 se propone
 * reducir (a la mitad, o a lo que ocupa); las demás se pueden sustituir con su botón. */
static void clique_barra(const char *id)
{
    tipo_aviso[0] = 0;
    if (comeca_com(id, "partelibre")) {
        int k = 0;
        texto_para_numero(id + 10, &k);
        pt_livre_t v[24];
        int n = pt_livres(&tab, v, 24);
        sel_part = 0;
        if (k < n) { esc = ES_HUECO; esc_hueco = v[k].inicio; esc_hueco_set = v[k].setores; }
        if (esc == ES_HUECO && esc_hueco_set < minimo_instalar()) txt(tipo_aviso, sizeof tipo_aviso, "Ese hueco es pequeño: hacen falta al menos %u MB.", minimo_instalar() / MB);
        return;
    }
    if (!comeca_com(id, "parte")) return;
    int k = 0;
    texto_para_numero(id + 5, &k);
    sel_part = k;
    char por[160];
    uint32_t mn;
    if (redutivel(k, &mn, por, sizeof por)) {
        pt_part_t *p = pt_achar(&tab, k);
        esc = ES_REDUCIR; esc_part = k; esc_min = mn; esc_max = p->setores;
        esc_red = (p->setores / 2 > mn ? p->setores / 2 : mn) / MB * MB;
        mb_para_caixa(&cx_red, esc_red / MB);
    } else {
        esc = ES_NADA;
        copia_texto(tipo_aviso, por, sizeof tipo_aviso);
    }
}

static caixa_t *caixas_da_pagina(int *n, caixa_t **v)
{
    int k = 0;
    if (pagina == P_TIPO) { if (tipo == TI_ENTERO) v[k++] = &cx_confirma; else if (esc == ES_REDUCIR) v[k++] = &cx_red; }
    if (pagina == P_TAMANOS) for (int i = 0; i < N_DADOS; i++) v[k++] = &cx_mb[i];
    if (pagina == P_OPCIONES) { v[k++] = &cx_nome; v[k++] = &cx_r1; v[k++] = &cx_r2; v[k++] = &cx_u1; v[k++] = &cx_u2; }
    *n = k;
    for (int i = 0; i < k; i++) if (v[i]->foco) return v[i];
    return NULL;
}

/* Comprobaciones al pulsar Siguiente; false = se queda (y plano_erro dice por qué). */
static bool validar_pagina(void)
{
    plano_erro[0] = 0;
    if (pagina == P_OPCIONES) {
        if (compara_texto(cx_r1.texto, cx_r2.texto) != 0) { txt(plano_erro, sizeof plano_erro, "las dos contraseñas de root no son iguales"); return false; }
        if (compara_texto(cx_u1.texto, cx_u2.texto) != 0) { txt(plano_erro, sizeof plano_erro, "las dos contraseñas de sagui no son iguales"); return false; }
        for (const char *c = cx_nome.texto; *c; c++)
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-')) {
                txt(plano_erro, sizeof plano_erro, "el nombre del equipo solo lleva letras sin acentos, números y guiones"); return false;
            }
        if (arranque == AR_UEFI && tipo != TI_ENTERO) arranque = AR_AMBOS;
    }
    return true;
}

static void seguinte(void)
{
    if (pagina == P_BIENVENIDA) { ler_destinos(); pagina = P_DISCO; }
    else if (pagina == P_DISCO) { if (sel_dest < 0) return; entrar_tipo(); pagina = P_TIPO; }
    else if (pagina == P_TIPO) {
        /* Lo mismo que el botón: con Intro tampoco se pasa sin escribir el nombre del disco. */
        if (tipo == TI_ENTERO ? compara_texto(cx_confirma.texto, destinos[sel_dest]) != 0 : esc == ES_NADA) return;
        if (!tamanhos_prontos) tamanhos_por_defecto();
        pagina = P_TAMANOS;
    }
    else if (pagina == P_TAMANOS) { if (!montar_plano(true)) return; pagina = P_OPCIONES; }
    else if (pagina == P_OPCIONES) { if (!validar_pagina()) { layout_mudou = true; return; } pagina = P_CONFIG; }
    else if (pagina == P_CONFIG) { montar_plano(true); pagina = P_RESUMEN; }
    else if (pagina == P_RESUMEN) { if (montar_plano(true)) comecar(); return; }
    layout_mudou = true;
}

static void atras(void)
{
    if (pagina > P_BIENVENIDA && pagina < P_PROGRESSO) { pagina--; plano_erro[0] = 0; layout_mudou = true; }
}

static int janela(void)
{
    if (!sou_root() && elevar_grafico("instalar SaguiOS: escribe y borra discos enteros") != 0) {   /* D255, D269: diálogo de permisos */
        mensagem("Instalar SaguiOS", "Instalar SaguiOS necesita permisos de administrador y no se concedieron.\n"
                 "Ábrelo otra vez y escribe la contraseña de administrador, o usa el Terminal:  admin instalar --ventana");
        return 1;
    }
    j = jan_abrir("Instalar SaguiOS", AJ_L, AJ_A);
    if (!j) { imprimir("instalar: no se pudo abrir la ventana (¿hay escritorio?)\n"); return 1; }
    /* Valores iniciales. */
    origem_txt[0] = 0;
    achar_origem(origem_txt, sizeof origem_txt);
    info_disco_t o;
    pl.desde_cd = origem_txt[0] && info_de(origem_txt, &o) && (o.flags & DISCO_CD);
    arranque = arrancou_por_uefi() ? AR_UEFI : efi_disponivel() ? AR_AMBOS : AR_BIOS;
    tecl = teclado_layout(-1) == TECLADO_ABNT2 ? TECLADO_ABNT2 : TECLADO_ES;
    caixa_definir(&cx_nome, "sagui");
    cx_r1.oculta = cx_r2.oculta = cx_u1.oculta = cx_u2.oculta = true;
    cs_apps.marcada = ha_apps();
    cs_jogos.marcada = false;
    cs_cc[0].marcada = true;   /* las preferencias, sí por defecto; lo demás lo elige la persona */
    cs_cc[1].marcada = true;
    zonas = zona_tabela(&nzonas);
    zonas_txt = aloca_zerado((size_t)(nzonas > 0 ? nzonas : 1) * 64);
    const char *atual = NULL;
    zona_do_processo(&atual);
    for (int i = 0; i < nzonas; i++) {
        copia_texto(zonas_txt[i], zonas[i].mostrar, 64);
        if (atual && compara_texto(zonas[i].nome, atual) == 0) lz.sel = i;
    }
    lz.itens = zonas_txt; lz.qtd = nzonas;
    if (lz.sel > 2) lz.topo = lz.sel - 1;
    ler_destinos();
    pagina = P_BIENVENIDA;
    desenhar();
    registrar("instalar: ventana abierta");
    evento_t e;
    for (;;) {
        if (esperar_evento(j->id, &e, 1000) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pagina != P_PROGRESSO) break; continue; }
        int nc;
        caixa_t *cs[8], *foco = caixas_da_pagina(&nc, cs);
        if (e.tipo == EV_TECLA) {
            if (e.tecla == '\t' && nc) {   /* Tab: el cuadro siguiente */
                int k = 0;
                for (int i = 0; i < nc; i++) if (cs[i]->foco) k = i + 1;
                for (int i = 0; i < nc; i++) cs[i]->foco = false;
                cs[k % nc]->foco = true;
                desenhar();
                continue;
            }
            if (foco && e.tecla != TECLA_ESC) {
                bool enter = caixa_tratar(j, foco, &e);
                if (pagina == P_TIPO && foco == &cx_red) {
                    uint32_t v = mb_caixa(&cx_red) * MB;
                    pt_part_t *p = pt_achar(&tab, esc_part);
                    if (p && v >= esc_min && v + minimo_instalar() <= p->setores) esc_red = v;
                }
                if (enter && pagina < P_PROGRESSO) seguinte();
                desenhar();
                continue;
            }
            if (e.tecla == TECLA_ESC && pagina < P_PROGRESSO) break;
            if (e.tecla == '\n') {
                if (pagina == P_FIM) break;
                if (pagina < P_PROGRESSO) { seguinte(); desenhar(); }
                continue;
            }
            if (pagina == P_TIPO && esc == ES_REDUCIR && (e.tecla == TECLA_ESQUERDA || e.tecla == TECLA_DIREITA)) {
                pt_part_t *p = pt_achar(&tab, esc_part);
                uint32_t passo = 64u * MB;
                if (e.tecla == TECLA_ESQUERDA && esc_red >= esc_min + passo) esc_red -= passo;
                if (e.tecla == TECLA_DIREITA && p && esc_red + passo + minimo_instalar() <= p->setores) esc_red += passo;
                mb_para_caixa(&cx_red, esc_red / MB);
                desenhar();
            }
            if (pagina == P_OPCIONES && lista_tratar(j, &lz, &e) != LISTA_NADA) desenhar();
            continue;
        }
        if (e.tipo == EV_RODA && pagina == P_OPCIONES) { lista_tratar(j, &lz, &e); desenhar(); continue; }
        if (e.tipo == EV_MOVER && arrastando) { if (e.botoes & 1) { mover_tirador(e.x); desenhar(); } else arrastando = false; continue; }
        if (e.tipo == EV_SOLTOU) { if (arrastando) { arrastando = false; layout_mudou = true; desenhar(); } continue; }
        if (e.tipo != EV_CLIQUE) continue;
        const char *z = zona_em(e.x, e.y);
        for (int i = 0; i < nc; i++) caixa_tratar(j, cs[i], &e);   /* el foco, al cuadro pulsado */
        if (e_zona(z, "cancelar")) { if (pagina < P_PROGRESSO) break; continue; }
        if (e_zona(z, "cerrar")) break;
        if (e_zona(z, "reiniciar")) { registrar("instalar: reiniciar ahora"); reiniciar(); continue; }
        if (e_zona(z, "atras")) { atras(); desenhar(); continue; }
        if (e_zona(z, "siguiente")) { seguinte(); desenhar(); continue; }
        if (pagina == P_DISCO && comeca_com(z, "disco")) { int k = 0; texto_para_numero(z + 5, &k); sel_dest = k; layout_mudou = true; }
        else if (pagina == P_TIPO) {
            if (e_zona(z, "tipo-entero")) { tipo = TI_ENTERO; layout_mudou = true; }
            else if (e_zona(z, "tipo-junto")) { tipo = TI_JUNTO; escolher_junto(); if (esc == ES_REDUCIR) mb_para_caixa(&cx_red, esc_red / MB); layout_mudou = true; tamanhos_prontos = false; }
            else if (e_zona(z, "tipo-avanzado")) { tipo = TI_AVANZADO; sel_part = esc == ES_REDUCIR || esc == ES_SUSTITUIR ? esc_part : 0; layout_mudou = true; tamanhos_prontos = false; }
            else if (e_zona(z, "tirador") || (e_zona(z, "barra") && esc == ES_REDUCIR && tipo != TI_AVANZADO)) { arrastando = e_zona(z, "tirador"); mover_tirador(e.x); layout_mudou = true; tamanhos_prontos = false; }
            else if (comeca_com(z, "parte")) { clique_barra(z); layout_mudou = true; tamanhos_prontos = false; }
            else if (e_zona(z, "av-reducir") && sel_part) { char id[20]; txt(id, sizeof id, "parte%d", sel_part); clique_barra(id); layout_mudou = true; }
            else if (e_zona(z, "av-sustituir") && sel_part && sel_part <= 4) {
                char n[16], q[200];
                pt_nome_particao(tab.disco, sel_part, n, sizeof n);
                txt(q, sizeof q, "¿Sustituir %s? Se BORRA entera y SaguiOS ocupa su sitio.", n);
                if (confirmar("Instalar SaguiOS", q)) { esc = ES_SUSTITUIR; esc_part = sel_part; tipo_aviso[0] = 0; }
                layout_mudou = true; tamanhos_prontos = false;
            }
        } else if (pagina == P_TAMANOS) {
            for (int i = 0; i < N_DADOS; i++) {
                char id[20];
                txt(id, sizeof id, "usa-%s", id_dado[i]);
                if (e_zona(z, id)) { cs_usa[i].marcada = !cs_usa[i].marcada; layout_mudou = true; }
                txt(id, sizeof id, "fs-%s", id_dado[i]);
                if (e_zona(z, id)) { fs_esc[i] = fs_esc[i] == PA_FS_FAT32 ? PA_FS_EXT2 : PA_FS_FAT32; layout_mudou = true; }
            }
            if (e_zona(z, "resto")) {
                cs_resto.marcada = !cs_resto.marcada;
                if (!cs_resto.marcada && montar_plano(true)) mb_para_caixa(&cx_mb[D_HOME], pl.home_set / MB);
                layout_mudou = true;
            }
        } else if (pagina == P_OPCIONES) {
            if (e_zona(z, "ar-bios")) arranque = AR_BIOS;
            else if (e_zona(z, "ar-uefi")) arranque = AR_UEFI;
            else if (e_zona(z, "ar-ambos")) arranque = AR_AMBOS;
            else if (e_zona(z, "tec-es")) tecl = TECLADO_ES;
            else if (e_zona(z, "tec-abnt2")) tecl = TECLADO_ABNT2;
            else if (e_zona(z, "zona")) lista_tratar(j, &lz, &e);
            else if (e_zona(z, "sshd")) cs_sshd.marcada = !cs_sshd.marcada;
            else if (e_zona(z, "vncd")) cs_vncd.marcada = !cs_vncd.marcada;
            else if (e_zona(z, "copiar-apps")) cs_apps.marcada = !cs_apps.marcada;
            else if (e_zona(z, "copiar-jogos")) cs_jogos.marcada = !cs_jogos.marcada;
            layout_mudou = true;
        } else if (pagina == P_CONFIG) {
            for (int i = 0; i < 7; i++) if (e_zona(z, cc_id[i])) { cs_cc[i].marcada = !cs_cc[i].marcada; layout_mudou = true; }
        }
        desenhar();
    }
    jan_fechar(j);
    return 0;
}
