// SPDX-License-Identifier: GPL-2.0-or-later
/* home_ampliar.c - «Ampliar /home para usar todo el espacio libre» del disco de arranque
 * (D318, v8.4.3). Lo usan la orden «particiones ampliar /home» y el diálogo del primer
 * arranque (ampliar-home), así los dos hacen lo mismo.
 *
 * Dos casos:
 *   - /home es una partición del disco de arranque (imagen persistente, disco instalado):
 *     crece hasta el final del disco con pt_mudar_tamanho, que mueve antes al final las
 *     lógicas que tenga detrás (juegos, /apps, SAGUI-CONF) si no hay otro modo;
 *   - /home está en la RAM (la imagen básica no lleva partición de datos): se crea una
 *     lógica ext2 SAGUI-DATOS con todo el espacio libre (la extendida crece antes), se le
 *     copia lo que hay ahora en /home y se monta en /home. El kernel la monta sola en los
 *     arranques siguientes (fstab: «arranque /home», D040, D179).
 * Nada de esto toca la partición de SaguiOS ni la ESP. */
#include "particoes.h"

static void txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

/* El disco del que arrancó el sistema (no un CD). */
static bool disco_arranque(char *nome, int max)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (!d.particao && (d.flags & DISCO_ARRANQUE_REAL) && !(d.flags & DISCO_CD)) {
            info_bloco_t b;
            if (blocos_info(d.nome, &b) == 0 && b.arranque_real) { copia_texto(nome, d.nome, (size_t)max); return true; }
        }
    return false;
}

int home_planejar(home_plano_t *h, char *m, int max)
{
    memset(h, 0, sizeof *h);
    if (!disco_arranque(h->disco, sizeof h->disco)) { txt(m, max, "no se sabe de qué disco arrancó el sistema (¿un CD?)"); return ERRO_NAO_EXISTE; }
    pt_tabela_t t;
    int r = pt_ler(h->disco, &t);
    if (r < 0) { txt(m, max, "no se puede leer la tabla de %s: %s", h->disco, nome_erro(r)); return r; }
    if (t.esquema != PT_MBR) { txt(m, max, "%s no tiene tabla MBR (¿GPT?): /home no se amplía aquí", h->disco); return ERRO_NAO_SUPORTADO; }
    char disco[12];
    int num = 0;
    if (pt_volume_de_ponto("/home", h->vol, sizeof h->vol) && pt_separar_nome(h->vol, disco, sizeof disco, &num) &&
        compara_texto(disco, h->disco) == 0 && num) {
        char e[200];
        if ((r = pt_planejar_tamanho(&t, num, 0, true, &h->plano, e, sizeof e)) < 0) { txt(m, max, "%s", e); return r; }
        h->ganho = h->plano.depois > h->plano.antes ? h->plano.depois - h->plano.antes : 0;
        h->total_novo = h->plano.depois;
        if (!h->ganho) { txt(m, max, "/home (%s) ya ocupa todo el espacio libre que tiene al lado", h->vol); return 0; }
        txt(m, max, "/home (%s) puede pasar de %u a %u MB%s", h->vol, h->plano.antes / PT_MB, h->plano.depois / PT_MB,
            h->plano.nmover ? " (las particiones de detrás se mueven al final del disco)" : "");
        return 0;
    }
    /* /home en la RAM: el hueco más grande, con la extendida ampliada hasta el final. */
    h->vol[0] = 0;
    h->criar = true;
    pt_tabela_t c = t;
    int x = pt_extendida(&c);
    char e[120];
    if (x >= 0) {
        uint32_t lim = c.setores;
        for (int i = 0; i < 4; i++)
            if (i != x && c.prim[i].tipo && c.prim[i].inicio >= c.prim[x].inicio + c.prim[x].setores && c.prim[i].inicio < lim) lim = c.prim[i].inicio;
        lim = lim / PT_MB * PT_MB;
        if (lim > c.prim[x].inicio + c.prim[x].setores && pt_redimensionar(&c, x + 1, lim - c.prim[x].inicio, e, sizeof e) >= 0) h->ext_ampliar = true;
    }
    pt_livre_t v[24];
    int n = pt_livres(&c, v, 24);
    for (int i = 0; i < n; i++) {
        if (x >= 0 && !v[i].logica) continue;   /* con extendida, dentro de ella (no gasta una entrada del MBR) */
        if (v[i].setores > h->criar_set) { h->criar_ini = v[i].inicio; h->criar_set = v[i].setores; }
    }
    if (x < 0) {
        int libres = 0;
        for (int i = 0; i < 4; i++) if (!t.prim[i].tipo) libres++;
        if (!libres) { txt(m, max, "%s no tiene extendida ni entradas libres en la tabla", h->disco); return ERRO_LIMITE; }
    }
    h->ganho = h->total_novo = h->criar_set;
    if (!h->ganho) { txt(m, max, "no hay espacio libre en %s", h->disco); return 0; }
    txt(m, max, "/home está en la RAM: se puede crear una partición SAGUI-DATOS de %u MB para él", h->criar_set / PT_MB);
    return 0;
}

/* ---- copiar /home (RAM) a la partición nueva ---- */
static int copiar_arq(const char *de, const char *para)
{
    int fi = abrir(de, ABRIR_LER);
    if (fi < 0) return fi;
    int fo = abrir(para, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fo < 0) { fechar(fi); return fo; }
    static uint8_t b[16384];
    int r = 0, n;
    while ((n = ler(fi, b, sizeof b)) > 0) if (escrever_tudo(fo, b, (unsigned)n) < 0) { r = ERRO_LIMITE; break; }
    if (n < 0) r = n;
    fechar(fi);
    fechar(fo);
    return r;
}

static int copiar_arvore(const char *de, const char *para, int nivel)
{
    if (nivel > 16) return 0;
    criar_pasta(para);
    entrada_pasta_t e;
    for (int i = 0; listar(de, i, &e) > 0; i++) {
        if (compara_texto(e.nome, ".") == 0 || compara_texto(e.nome, "..") == 0 || compara_texto(e.nome, "lost+found") == 0) continue;
        char a[256], b[256];
        caminho_juntar(a, sizeof a, de, e.nome);
        caminho_juntar(b, sizeof b, para, e.nome);
        int r = e.tipo == TIPO_PASTA ? copiar_arvore(a, b, nivel + 1) : copiar_arq(a, b);
        if (r < 0) return r;
        info_permisos_t pm;   /* D282: el modo de antes; /home es de sagui */
        if (permisos_arquivo(a, &pm) == 0) mudar_permisos(b, pm.modo);
        mudar_dono(b, UID_SAGUI, GID_SAGUI);
    }
    return 0;
}

int home_ampliar(home_plano_t *h, fs_progresso_t prog, void *ctx, char *m, int max)
{
    if (!h->ganho) { txt(m, max, "no hay nada que ampliar"); return 0; }
    if (!h->criar) {
        h->plano.progresso = prog;
        h->plano.ctx = ctx;
        return pt_mudar_tamanho(&h->plano, DBLOCO_CONF_ARRANQUE, true, m, max);
    }
    pt_acao_t a;
    int r;
    if (h->ext_ampliar) {
        memset(&a, 0, sizeof a);
        a.op = PA_AMPLIAR;
        copia_texto(a.disco, h->disco, sizeof a.disco);
        a.conf = DBLOCO_CONF_ARRANQUE;
        if ((r = pt_executar(&a, m, max)) < 0) return r;
    }
    pt_tabela_t t;
    if ((r = pt_ler(h->disco, &t)) < 0) { txt(m, max, "no se puede releer %s", h->disco); return r; }
    memset(&a, 0, sizeof a);
    a.op = PA_CRIAR;
    copia_texto(a.disco, h->disco, sizeof a.disco);
    a.inicio = h->criar_ini;
    a.setores = h->criar_set;
    a.logica = pt_extendida(&t) >= 0;
    a.fs = PA_FS_EXT2;
    copia_texto(a.rotulo, "SAGUI-DATOS", sizeof a.rotulo);
    a.conf = DBLOCO_CONF_ARRANQUE;
    a.progresso = prog;
    a.ctx = ctx;
    if ((r = pt_executar(&a, m, max)) < 0) return r;
    char vol[16];
    pt_nome_particao(h->disco, a.resultado, vol, sizeof vol);
    copia_texto(h->vol, vol, sizeof h->vol);
    /* Lo que hay ahora en /home (en la RAM) pasa a la partición; después se monta en /home. */
    criar_pasta("/mnt/home-nueva");
    if ((r = montar_em(vol, "/mnt/home-nueva", MONTAR_LEITURA_ESCRITA)) < 0) { txt(m, max, "%s creada, pero no se puede montar: %s", vol, nome_erro(r)); return r; }
    r = copiar_arvore("/home", "/mnt/home-nueva", 0);
    mudar_dono("/mnt/home-nueva", UID_SAGUI, GID_SAGUI);   /* D282: /home es de sagui */
    desmontar("/mnt/home-nueva");
    if (r < 0) { txt(m, max, "%s creada, pero copiar /home ha fallado (%s): /home sigue en la RAM; los archivos están a salvo allí", vol, nome_erro(r)); return r; }
    if ((r = montar_em(vol, "/home", MONTAR_LEITURA_ESCRITA)) < 0) { txt(m, max, "%s creada con una copia de /home, pero no se puede montar en /home ahora (%s): se montará al reiniciar", vol, nome_erro(r)); return 0; }
    txt(m, max, "/home está ahora en %s (%u MB, ext2 SAGUI-DATOS), con lo que había en la RAM", vol, h->criar_set / PT_MB);
    registrar("particiones: /home creada en %s (%u MB)", vol, h->criar_set / PT_MB);
    return 0;
}
