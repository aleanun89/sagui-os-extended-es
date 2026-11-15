// SPDX-License-Identifier: GPL-2.0-or-later
/* particoes_acoes.c - cada operación de particiones, de principio a fin (D179).
 *
 * El terminal ("particiones") y la ventana ("discos") llaman a pt_executar, así
 * los dos hacen exactamente lo mismo: reservar el disco (el kernel se niega si es el
 * de arranque sin la confirmación doble), leer la tabla, cambiarla en memoria,
 * validarla, escribirla (EBR primero, MBR al final), releerla y soltar el disco,
 * siempre, también si algo falla.
 * D213: se reserva con DBLOCO_CONF_MONTADO, así se puede cambiar el disco de arranque
 * (crear una lógica, ampliar la extendida, redimensionar una partición desmontada)
 * con /home y /apps montados: el kernel no deja tocar nada montado ni la partición de
 * SaguiOS. Aquí se mira antes, para dar un mensaje claro en vez de un error de E/S.
 * D266: en un disco GPT se puede crear, borrar y formatear (gpt.c escribe la cabecera de
 * respaldo, la principal y el MBR protector, y comprueba los CRC); se reserva con
 * DBLOCO_CONF_GPT y sin nada montado (el kernel no admite GPT con particiones montadas). */
#include "particoes.h"
#include "gpt.h"

static void txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

const char *pt_nome_fs(int fs) { return fs == PA_FS_FAT32 ? "FAT32" : fs == PA_FS_EXT2 ? "ext2" : "sin formato"; }

bool pt_separar_nome(const char *nome, char *disco, int max, int *numero)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (compara_texto(d.nome, nome) == 0) {
            if (d.particao) { copia_texto(disco, d.pai, (size_t)max); *numero = (int)d.particao; }
            else { copia_texto(disco, d.nome, (size_t)max); *numero = 0; }
            return true;
        }
    return false;
}

/* ¿Está montada esta partición? Devuelve dónde (o NULL). */
static const char *montada(const char *disco, int numero, char *ponto, int max)
{
    char vol[16];
    info_disco_t d;
    pt_nome_particao(disco, numero, vol, sizeof vol);
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (compara_texto(d.nome, vol) == 0 && d.montado) { copia_texto(ponto, d.ponto, (size_t)max); return ponto; }
    return NULL;
}

/* D213: lo que no se puede tocar con el disco en uso, dicho antes de reservar nada. */
static int comprobar_uso(pt_acao_t *a, char *m, int max)
{
    pt_tabela_t t;
    char ponto[64], vol[16];
    if (pt_ler(a->disco, &t) < 0 || (t.esquema != PT_MBR && t.esquema != PT_GPT)) return 0;
    info_disco_t d;
    bool arranque = false;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (compara_texto(d.nome, a->disco) == 0) arranque = (d.flags & DISCO_ARRANQUE_REAL) != 0;
    if (a->op == PA_TABELA_NOVA || a->op == PA_TABELA_GPT) {
        for (int n = 1; n < 5 + t.nlog; n++)
            if (pt_achar(&t, n) && montada(a->disco, n, ponto, sizeof ponto)) {
                pt_nome_particao(a->disco, n, vol, sizeof vol);
                txt(m, max, "%s está montada en %s: para una tabla nueva hay que desmontar todo el disco", vol, ponto);
                return ERRO_OCUPADO;
            }
        return 0;
    }
    if (a->op != PA_APAGAR && a->op != PA_REDIMENSIONAR && a->op != PA_FORMATAR && !(a->op == PA_TRIM && a->numero)) return 0;
    pt_part_t *p = pt_achar(&t, a->numero);
    if (!p) return 0;
    pt_nome_particao(a->disco, a->numero, vol, sizeof vol);
    if (p->tipo == 0xDA && arranque) {
        txt(m, max, "%s es la partición de SaguiOS del disco de arranque: no se toca", vol);
        return ERRO_ACESSO;
    }
    if (montada(a->disco, a->numero, ponto, sizeof ponto)) {
        txt(m, max, "%s está montada en %s: desmóntala antes (umount %s)", vol, ponto, ponto);
        return ERRO_OCUPADO;
    }
    if (a->numero <= 4 && pt_tipo_extendida(p->tipo) && a->op == PA_APAGAR)
        for (int k = 0; k < t.nlog; k++)
            if (montada(a->disco, k + 5, ponto, sizeof ponto)) {
                pt_nome_particao(a->disco, k + 5, vol, sizeof vol);
                txt(m, max, "la extendida tiene %s montada en %s: desmóntala antes", vol, ponto);
                return ERRO_OCUPADO;
            }
    return 0;
}

static int reservar(pt_acao_t *a, char *m, int max)
{
    /* D266: una GPT (la que hay o la que se va a crear) se reserva con DBLOCO_CONF_GPT; sin
     * DBLOCO_CONF_MONTADO, porque el kernel no deja tocar una GPT con particiones montadas. */
    pt_tabela_t t0;
    bool gpt = a->op == PA_TABELA_GPT || (pt_ler(a->disco, &t0) == 0 && t0.esquema == PT_GPT);
    int r = blocos_reservar(a->disco, a->conf | (gpt ? DBLOCO_CONF_GPT : DBLOCO_CONF_MONTADO));
    if (r == ERRO_OCUPADO) {
        info_bloco_t i;
        if (blocos_info(a->disco, &i) == 0 && i.reservado_pid)
            txt(m, max, "%s está en uso por otro programa (pid %u)", a->disco, i.reservado_pid);
        else
            txt(m, max, "%s tiene particiones montadas%s: desmóntalas antes (umount)", a->disco, gpt ? " y una tabla GPT" : "");
    } else if (r == ERRO_DISCO_ARRANQUE)
        txt(m, max, "%s es el disco de arranque de SaguiOS: hace falta confirmarlo dos veces", a->disco);
    else if (r < 0)
        txt(m, max, "no se puede usar %s: %s", a->disco, nome_erro(r));
    return r;
}

static int formatar_acao(pt_acao_t *a, int numero, char *m, int max)
{
    char vol[16];
    pt_nome_particao(a->disco, numero, vol, sizeof vol);
    int r = a->fs == PA_FS_FAT32 ? fs_formatar_fat32(vol, a->rotulo, a->progresso, a->ctx, m, max)
          : a->fs == PA_FS_EXT2 ? fs_formatar_ext2(vol, a->rotulo, a->progresso, a->ctx, m, max) : ERRO_INVALIDO;
    if (r < 0) return r;
    /* El tipo de la tabla, de acuerdo con el formato. */
    pt_tabela_t t;
    uint8_t tipo = a->fs == PA_FS_FAT32 ? 0x0C : 0x83;
    if ((r = pt_ler(a->disco, &t)) < 0) return r;
    pt_part_t *p = pt_achar(&t, numero);
    if (t.esquema == PT_MBR && p && p->tipo != tipo && p->tipo != 0xEF) {   /* la ESP (0xEF) conserva su tipo (D266) */
        if ((r = pt_mudar_tipo(&t, numero, tipo, m, max)) < 0) return r;
        if ((r = pt_gravar(&t, "tipo tras formatear", m, max)) < 0) return r;
    }
    txt(m, max, "%s formateada en %s%s%s%s", vol, pt_nome_fs(a->fs), a->rotulo[0] ? " (\"" : "", a->rotulo, a->rotulo[0] ? "\")" : "");
    return 0;
}

/* D266: crear y borrar en una GPT. Se relee la GPT entera (con las entradas que no vemos), se
 * cambia en memoria, se valida y solo entonces se escribe (respaldo, principal y comprobación). */
static int gpt_carregar(pt_acao_t *a, gpt_t **g, char *m, int max)
{
    *g = aloca(sizeof **g);
    if (!*g) { txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    char e[120];
    int r = gpt_ler(a->disco, *g, e, sizeof e);
    if (r < 0) { txt(m, max, "la GPT de %s no es válida (%s): no se toca; usa una tabla nueva si quieres empezar de cero", a->disco, e); libera(*g); *g = NULL; }
    return r;
}

static int executar_gpt(pt_acao_t *a, pt_tabela_t *t, char *m, int max)
{
    char d[64], vol[16];
    gpt_t *g;
    int r;
    if (t->gpt_mala) { txt(m, max, "%s: la GPT no es válida (%s): solo vale una tabla nueva", a->disco, t->gpt_aviso); return ERRO_FORMATO; }
    if (a->op == PA_CRIAR) {
        if (a->logica) { txt(m, max, "una GPT no tiene particiones lógicas"); return ERRO_INVALIDO; }
        uint8_t tipo = a->tipo ? a->tipo : a->fs == PA_FS_FAT32 ? 0x0C : 0x83;
        const uint8_t *guid = gpt_guid_de_tipo_mbr(tipo);
        if (!guid) { txt(m, max, "ese tipo de partición no existe en una GPT (usa fat32, ext2, tipo=0xEF para la ESP, 0xDA, 0x83 o 0x07)"); return ERRO_INVALIDO; }
        pt_livre_t v[24];
        int n = pt_livres(t, v, 24), k = -1;
        for (int i = 0; i < n && k < 0; i++) {
            if (a->inicio && (a->inicio < v[i].inicio || a->inicio >= v[i].inicio + v[i].setores)) continue;
            k = i;
        }
        if (k < 0) { txt(m, max, "no hay espacio libre en %s%s", a->disco, a->inicio ? " en esa posición" : ""); return ERRO_LIMITE; }
        uint32_t ini = a->inicio ? a->inicio : v[k].inicio;
        uint32_t cabe = v[k].inicio + v[k].setores - ini, qtd = a->setores ? a->setores : cabe;
        if (qtd > cabe) { txt(m, max, "no cabe: en ese hueco hay %u MB libres", cabe / PT_MB); return ERRO_LIMITE; }
        qtd = qtd / PT_MB * PT_MB;
        if (!qtd) { txt(m, max, "la partición tiene que tener al menos 1 MB"); return ERRO_INVALIDO; }
        if ((r = gpt_carregar(a, &g, m, max)) < 0) return r;
        int slot = gpt_adicionar(g, guid, ini, qtd, a->rotulo, m, max);
        if (slot < 0) { libera(g); return slot; }
        int num = gpt_numero_kernel(g, slot);
        if (!num) { txt(m, max, "la entrada nueva no quedaría entre las cuatro que ve el sistema: no se crea"); libera(g); return ERRO_LIMITE; }
        r = gpt_gravar(g, false, NULL, m, max);
        libera(g);
        if (r < 0) return r;
        a->resultado = num;
        if (a->fs != PA_FS_NENHUM && (r = formatar_acao(a, num, m, max)) < 0) return r;
        pt_nome_particao(a->disco, num, vol, sizeof vol);
        txt(m, max, "creada %s: %u MB, %s%s%s", vol, qtd / PT_MB, gpt_nome_tipo(guid), a->fs ? ", formateada en " : "", a->fs ? pt_nome_fs(a->fs) : "");
        return 0;
    }
    if (a->op == PA_APAGAR) {
        int slot = a->numero - 1;
        pt_nome_particao(a->disco, a->numero, vol, sizeof vol);
        if (a->trim) {
            r = blocos_descartar(vol, 0, pt_achar(t, a->numero) ? pt_achar(t, a->numero)->setores : 0);
            if (r < 0 && r != ERRO_NAO_SUPORTADO) { txt(m, max, "TRIM de %s: %s", vol, nome_erro(r)); return r; }
        }
        if ((r = gpt_carregar(a, &g, m, max)) < 0) return r;
        if ((r = gpt_remover(g, slot, m, max)) < 0) { libera(g); return r; }
        formatar_texto(d, sizeof d, "borrar la partición %d", a->numero);
        r = gpt_gravar(g, false, NULL, m, max);
        libera(g);
        if (r < 0) return r;
        txt(m, max, "borrada %s%s", vol, a->trim ? " y descartado su contenido (TRIM)" : "");
        return 0;
    }
    return ERRO_INVALIDO;
}

static int executar_acao(pt_acao_t *a, char *m, int max)
{
    pt_tabela_t t;
    char d[64];
    int r = pt_ler(a->disco, &t);
    if (r < 0) { txt(m, max, "no se puede leer la tabla de %s: %s", a->disco, nome_erro(r)); return r; }
    if (a->op == PA_TABELA_GPT) {   /* D266: una GPT vacía, con su MBR protector */
        gpt_t *g = aloca(sizeof *g);
        if (!g) { txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
        gpt_nova(g, a->disco, t.setores);
        r = gpt_gravar(g, true, NULL, m, max);
        libera(g);
        if (r < 0) return r;
        txt(m, max, "%s tiene ahora una tabla GPT vacía (con MBR protector)", a->disco);
        return 0;
    }
    if (t.esquema == PT_GPT) {   /* D266: crear, borrar y formatear; lo demás (activa, tamaño, deshacer...) no */
        switch (a->op) {
        case PA_CRIAR: case PA_APAGAR: return executar_gpt(a, &t, m, max);
        case PA_FORMATAR: case PA_TRIM: case PA_TABELA_NOVA: break;
        default:
            txt(m, max, "%s tiene tabla GPT: esta operación no está disponible en GPT (se puede crear, borrar y formatear)", a->disco);
            return ERRO_NAO_SUPORTADO;
        }
        if (t.gpt_mala && a->op != PA_TABELA_NOVA) { txt(m, max, "%s: la GPT no es válida (%s): solo vale una tabla nueva", a->disco, t.gpt_aviso); return ERRO_FORMATO; }
    }
    if (t.esquema == PT_VOLUME && a->op != PA_TABELA_NOVA && a->op != PA_DESHACER) {
        txt(m, max, "%s es un volumen sin tabla de particiones: crea antes una tabla nueva", a->disco);
        return ERRO_INVALIDO;
    }
    switch (a->op) {
    case PA_TABELA_NOVA: {
        bool era_gpt = t.esquema == PT_GPT;
        memset(t.prim, 0, sizeof t.prim);
        t.nlog = 0;
        t.esquema = PT_MBR;
        if (era_gpt) {   /* la cabecera y las entradas de la GPT, al principio y al final */
            blocos_zerar(a->disco, 1, 33);
            blocos_zerar(a->disco, t.setores - 33, 33);
        }
        if ((r = pt_gravar(&t, "tabla nueva (vacía)", m, max)) < 0) return r;
        txt(m, max, "%s tiene ahora una tabla MBR vacía", a->disco);
        return 0;
    }
    case PA_CRIAR: {
        pt_livre_t v[24];
        int n = pt_livres(&t, v, 24), k = -1;
        uint8_t tipo = a->tipo ? a->tipo : a->fs == PA_FS_FAT32 ? 0x0C : 0x83;
        for (int i = 0; i < n && k < 0; i++) {
            if (v[i].logica != a->logica) continue;
            if (a->inicio && (a->inicio < v[i].inicio || a->inicio >= v[i].inicio + v[i].setores)) continue;
            k = i;
        }
        if (k < 0) {
            txt(m, max, a->logica ? "no hay sitio libre dentro de la extendida%s" : "no hay espacio libre en %s%s",
                a->logica ? "" : a->disco, a->inicio ? " en esa posición" : "");
            return ERRO_LIMITE;
        }
        uint32_t ini = a->inicio ? a->inicio : v[k].inicio;
        uint32_t cabe = v[k].inicio + v[k].setores - ini;
        uint32_t qtd = a->setores ? a->setores : cabe;
        if (qtd > cabe) { txt(m, max, "no cabe: en ese hueco hay %u MB libres", cabe / PT_MB); return ERRO_LIMITE; }
        int num = pt_criar(&t, ini, qtd, tipo, a->logica, m, max);
        if (num < 0) return num;
        formatar_texto(d, sizeof d, "crear la partición %d", num);
        if ((r = pt_gravar(&t, d, m, max)) < 0) return r;
        a->resultado = num;
        if (a->fs != PA_FS_NENHUM) {
            if ((r = formatar_acao(a, num, m, max)) < 0) return r;
        }
        char vol[16];
        pt_nome_particao(a->disco, num, vol, sizeof vol);
        pt_part_t *p = pt_achar(&t, num);
        txt(m, max, "creada %s: %u MB, %s%s%s", vol, p ? p->setores / PT_MB : 0, pt_nome_tipo(tipo),
            a->fs ? ", formateada en " : "", a->fs ? pt_nome_fs(a->fs) : "");
        return 0;
    }
    case PA_APAGAR: {
        pt_part_t *p = pt_achar(&t, a->numero);
        if (!p) { txt(m, max, "no existe la partición %d en %s", a->numero, a->disco); return ERRO_NAO_EXISTE; }
        char vol[16];
        pt_nome_particao(a->disco, a->numero, vol, sizeof vol);
        if (a->trim && !pt_tipo_extendida(p->tipo)) {
            r = blocos_descartar(vol, 0, p->setores);
            if (r < 0 && r != ERRO_NAO_SUPORTADO) { txt(m, max, "TRIM de %s: %s", vol, nome_erro(r)); return r; }
        }
        bool ext = a->numero <= 4 && pt_tipo_extendida(p->tipo);
        int nlog = t.nlog;
        if ((r = pt_apagar(&t, a->numero, m, max)) < 0) return r;
        formatar_texto(d, sizeof d, "borrar la partición %d", a->numero);
        if ((r = pt_gravar(&t, d, m, max)) < 0) return r;
        if (ext && nlog) txt(m, max, "borrada %s (la extendida y sus %d lógicas)", vol, nlog);
        else txt(m, max, "borrada %s%s", vol, a->trim ? " y descartado su contenido (TRIM)" : "");
        return 0;
    }
    case PA_REDIMENSIONAR: {
        pt_part_t *p = pt_achar(&t, a->numero);
        if (!p) { txt(m, max, "no existe la partición %d en %s", a->numero, a->disco); return ERRO_NAO_EXISTE; }
        char vol[16], tipo_fs[16], cmp[160] = "", extra[200] = "";
        pt_nome_particao(a->disco, a->numero, vol, sizeof vol);
        uint32_t antes = p->setores;
        formatar_texto(d, sizeof d, "redimensionar la partición %d", a->numero);
        if (pt_tipo_extendida(p->tipo)) {   /* la extendida: solo la tabla (sus lógicas no cambian) */
            if ((r = pt_redimensionar(&t, a->numero, a->setores, m, max)) < 0) return r;
            if ((r = pt_gravar(&t, d, m, max)) < 0) return r;
            txt(m, max, "la extendida %s pasa de %u a %u MB", vol, antes / PT_MB, pt_achar(&t, a->numero)->setores / PT_MB);
            return 0;
        }
        /* D213: el tamaño final de verdad (el final cae en un límite de 1 MB), sin escribir nada. */
        pt_tabela_t prova = t;
        if ((r = pt_redimensionar(&prova, a->numero, a->setores, m, max)) < 0) return r;
        uint32_t depois = pt_achar(&prova, a->numero)->setores;
        if (depois == antes) { txt(m, max, "%s ya mide %u MB", vol, antes / PT_MB); return 0; }
        uint32_t ocupa = fs_tamanho(vol, tipo_fs, sizeof tipo_fs);
        bool fat = ocupa && compara_texto(tipo_fs, "FAT32") == 0, ext = ocupa && !fat;
        /* Antes de tocar nada, el sistema de archivos tiene que estar sano (fsck de solo lectura). */
        if (ocupa && (r = fs_verificar(vol, cmp, sizeof cmp)) < 0) {
            txt(m, max, "comprobación previa: %s. No se ha cambiado nada", cmp);
            return r;
        }
        /* Encoger: primero el sistema de archivos (si no cabe), después la partición. */
        if (depois < antes && ocupa > depois) {
            if (fat) {
                txt(m, max, "el FAT32 de %s ocupa %u MB: un FAT32 solo crece, no se encoge por debajo de eso", vol, (ocupa + PT_MB - 1) / PT_MB);
                return ERRO_INVALIDO;
            }
            if ((r = fs_redimensionar_ext2(vol, depois, a->progresso, a->ctx, extra, sizeof extra)) < 0) {
                txt(m, max, "no se encoge el ext2 de %s: %s. La partición no ha cambiado", vol, extra);
                return r;
            }
        }
        if ((r = pt_redimensionar(&t, a->numero, a->setores, m, max)) < 0) return r;
        if ((r = pt_gravar(&t, d, m, max)) < 0) return r;
        /* Crecer: primero la partición (ya está), después el sistema de archivos. */
        if (depois > antes && ocupa) {
            if (fat) fs_crescer_fat32(vol, extra, sizeof extra);
            else if (ext && (r = fs_redimensionar_ext2(vol, depois, a->progresso, a->ctx, extra, sizeof extra)) < 0) {
                txt(m, max, "%s pasa de %u a %u MB, pero el ext2 se queda como estaba: %s", vol, antes / PT_MB, depois / PT_MB, extra);
                return r;
            }
        }
        /* Y la comprobación de después. */
        if (ocupa && fs_verificar(vol, cmp, sizeof cmp) < 0) {
            txt(m, max, "%s pasa de %u a %u MB, pero la comprobación de después falla: %s", vol, antes / PT_MB, depois / PT_MB, cmp);
            return ERRO_FORMATO;
        }
        txt(m, max, "%s pasa de %u a %u MB%s%s", vol, antes / PT_MB, depois / PT_MB, extra[0] ? "; " : "", extra);
        return 0;
    }
    case PA_AMPLIAR: {   /* D213: la extendida, hasta el final del disco (o hasta la primaria que la siga) */
        int x = pt_extendida(&t);
        if (x < 0) { txt(m, max, "%s no tiene partición extendida", a->disco); return ERRO_NAO_EXISTE; }
        pt_part_t *e = &t.prim[x];
        uint32_t lim = t.setores;
        for (int i = 0; i < 4; i++)
            if (i != x && t.prim[i].tipo && t.prim[i].inicio >= e->inicio + e->setores && t.prim[i].inicio < lim) lim = t.prim[i].inicio;
        uint32_t antes = e->setores, fim = lim / PT_MB * PT_MB;
        if (fim <= e->inicio + e->setores) {
            txt(m, max, "la extendida de %s ya llega hasta donde puede (%u MB)", a->disco, antes / PT_MB);
            return 0;
        }
        if ((r = pt_redimensionar(&t, x + 1, fim - e->inicio, m, max)) < 0) return r;
        if ((r = pt_gravar(&t, "ampliar la extendida", m, max)) < 0) return r;
        e = &t.prim[x];
        txt(m, max, "la extendida de %s pasa de %u a %u MB: hay %u MB libres dentro para lógicas nuevas", a->disco,
            antes / PT_MB, e->setores / PT_MB, (e->setores - antes) / PT_MB);
        return 0;
    }
    case PA_FORMATAR:
        if (!pt_achar(&t, a->numero)) { txt(m, max, "no existe la partición %d en %s", a->numero, a->disco); return ERRO_NAO_EXISTE; }
        if (pt_tipo_extendida(pt_achar(&t, a->numero)->tipo)) { txt(m, max, "una extendida no se formatea"); return ERRO_INVALIDO; }
        return formatar_acao(a, a->numero, m, max);
    case PA_ATIVAR:
        if ((r = pt_ativar(&t, a->numero, m, max)) < 0) return r;
        formatar_texto(d, sizeof d, "marcar activa la %d", a->numero);
        if ((r = pt_gravar(&t, d, m, max)) < 0) return r;
        txt(m, max, "la partición %d es ahora la activa", a->numero);
        return 0;
    case PA_TRIM: {
        char vol[16];
        uint32_t ini = 1, qtd;
        if (a->numero) {
            pt_part_t *p = pt_achar(&t, a->numero);
            if (!p || pt_tipo_extendida(p->tipo)) { txt(m, max, "esa partición no existe o es la extendida"); return ERRO_INVALIDO; }
            pt_nome_particao(a->disco, a->numero, vol, sizeof vol);
            ini = 0; qtd = p->setores;
        } else {
            /* El disco entero: solo el espacio que no es de ninguna partición. */
            pt_livre_t v[24];
            int n = pt_livres(&t, v, 24);
            uint32_t total = 0;
            for (int i = 0; i < n; i++) {
                uint32_t i0 = v[i].logica ? v[i].inicio : v[i].inicio, q = v[i].setores;
                r = blocos_descartar(a->disco, i0, q);
                if (r < 0) { txt(m, max, "TRIM de %s: %s", a->disco, nome_erro(r)); return r; }
                total += q;
            }
            txt(m, max, "descartado (TRIM) el espacio libre de %s: %u MB", a->disco, total / PT_MB);
            return 0;
        }
        r = blocos_descartar(vol, ini, qtd);
        if (r < 0) { txt(m, max, "TRIM de %s: %s", vol, nome_erro(r)); return r; }
        txt(m, max, "descartado (TRIM) todo el contenido de %s: %u MB", vol, qtd / PT_MB);
        return 0;
    }
    case PA_DESHACER:
        return pt_deshacer(a->disco, m, max);
    }
    return ERRO_INVALIDO;
}

int pt_executar(pt_acao_t *a, char *m, int max)
{
    if (m && max) m[0] = 0;
    int r = comprobar_uso(a, m, max);
    if (r < 0) { registrar("particiones: op %d en %s (%d): %d %s", a->op, a->disco, a->numero, r, m ? m : ""); return r; }
    r = reservar(a, m, max);
    if (r < 0) return r;
    r = executar_acao(a, m, max);
    blocos_liberar(a->disco);   /* siempre: y el kernel relee la tabla */
    registrar("particiones: op %d en %s (%d): %d %s", a->op, a->disco, a->numero, r, m ? m : "");
    return r;
}
