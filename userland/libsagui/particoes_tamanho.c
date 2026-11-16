// SPDX-License-Identifier: GPL-2.0-or-later
/* particoes_tamanho.c - cambiar el tamaño de una partición hasta donde haga falta,
 * moviendo hacia el final del disco las lógicas que tenga detrás (D318, v8.4.3).
 *
 * El caso que lo pide: una imagen de SaguiOS grabada en un pendrive (o disco) mucho
 * mayor que ella. Las lógicas de la imagen van seguidas (/home, juegos, /apps,
 * SAGUI-CONF) y el espacio libre queda al final: /home no tiene nada libre al lado.
 * Para «ampliar /home a todo el disco» las lógicas de detrás se copian al final del
 * disco y /home crece hasta ellas.
 *
 * Dos partes:
 *   - pt_planejar_tamanho: solo cálculos sobre la tabla en memoria (se prueba en el
 *     anfitrión, h318). Dice el tamaño final, si la extendida crece, qué lógicas se
 *     mueven y adónde, y lo máximo que se podría;
 *   - pt_mudar_tamanho: lo hace, en este orden, cada paso recuperable si se corta:
 *       1. comprobar el sistema de archivos de la partición (fsck de solo lectura);
 *       2. ampliar la extendida (solo su entrada del MBR: nada más cambia);
 *       3. copiar cada lógica que se mueve a su sitio nuevo, que está en espacio libre y
 *          NUNCA se solapa con el viejo (si no hay sitio para eso, no se mueve nada), y
 *          releerla entera para compararla. Hasta aquí la tabla no ha cambiado: si se
 *          corta, el disco sigue igual que antes (la copia queda en espacio libre);
 *       4. escribir la tabla con las lógicas en su sitio nuevo: pt_gravar escribe los EBR
 *          nuevos (en espacio libre, sin enlazar) y al final el EBR de la partición que
 *          crece, que pasa a enlazarlos: un solo sector cambia la cadena entera de una vez;
 *       5. agrandar la partición en la tabla y, después, su sistema de archivos (ext2:
 *          fs_redimensionar_ext2; FAT32: hasta donde le dé su FAT) y comprobarlo.
 *     Encoger va al revés: primero el sistema de archivos y después la tabla (pt_executar
 *     con PA_REDIMENSIONAR, que ya lo hace así).
 * Lo montado: la partición que cambia y las que se mueven se desmontan antes (si se
 * permite) y se vuelven a montar donde estaban al acabar, también si algo falla. La de
 * SaguiOS y la ESP no se mueven nunca (son primarias). */
#include "particoes.h"

static void txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

static uint32_t abaixo(uint32_t x) { return x / PT_MB * PT_MB; }

/* El final hasta el que puede llegar la extendida x: el disco, o la primaria que la siga. */
static uint32_t limite_extendida(const pt_tabela_t *t, int x)
{
    const pt_part_t *e = &t->prim[x];
    uint32_t lim = t->setores;
    for (int i = 0; i < 4; i++)
        if (i != x && t->prim[i].tipo && t->prim[i].inicio >= e->inicio + e->setores && t->prim[i].inicio < lim) lim = t->prim[i].inicio;
    return abaixo(lim);
}

int pt_planejar_tamanho(const pt_tabela_t *t, int numero, uint32_t setores, bool mover, pt_tamanho_t *p, char *erro, int max)
{
    memset(p, 0, sizeof *p);
    copia_texto(p->disco, t->disco, sizeof p->disco);
    p->numero = numero;
    if (t->esquema != PT_MBR) { txt(erro, max, "%s no tiene tabla MBR: el tamaño solo se cambia en MBR", t->disco); return ERRO_NAO_SUPORTADO; }
    pt_tabela_t c = *t;
    pt_part_t *a = pt_achar(&c, numero);
    if (!a) { txt(erro, max, "no existe la partición %d en %s", numero, t->disco); return ERRO_NAO_EXISTE; }
    if (pt_tipo_extendida(a->tipo)) { txt(erro, max, "la extendida se amplía con \"particiones ampliar %s\"", t->disco); return ERRO_INVALIDO; }
    if (a->tipo == 0xDA || a->tipo == 0xEF) { txt(erro, max, "la partición %d es la de %s: no se cambia su tamaño", numero, a->tipo == 0xDA ? "SaguiOS" : "EFI"); return ERRO_ACESSO; }
    p->antes = a->setores;
    uint32_t fim_sem, fim_com = 0;   /* hasta dónde puede llegar su final, sin mover nada y moviendo */
    int x = pt_extendida(&c);
    if (numero <= 4) {   /* una primaria: hasta la siguiente partición o el final del disco */
        uint32_t lim = c.setores;
        for (int i = 0; i < 4; i++)
            if (c.prim[i].tipo && i != numero - 1 && c.prim[i].inicio >= a->inicio + a->setores && c.prim[i].inicio < lim) lim = c.prim[i].inicio;
        fim_sem = abaixo(lim);
    } else {
        int k = numero - 5;
        uint32_t ext_lim = limite_extendida(&c, x);
        if (k + 1 < c.nlog) {
            fim_sem = abaixo(c.log[k + 1].ebr);
            /* Mover las de detrás al final: de la última a la primera, cada una pegada a la
             * siguiente (su EBR en el MB de delante). Solo vale si el sitio nuevo de todas
             * queda detrás del final viejo de la última: nada se pisa mientras se copia. */
            uint32_t pos = ext_lim, fim_velho = c.log[c.nlog - 1].inicio + c.log[c.nlog - 1].setores;
            bool cabe = true;
            for (int j = c.nlog - 1; j > k && cabe; j--) {
                if (pos < c.log[j].setores + PT_MB) { cabe = false; break; }
                uint32_t ini = abaixo(pos - c.log[j].setores);
                if (ini < PT_MB) { cabe = false; break; }
                p->novo_ini[j - k - 1] = ini;
                pos = ini - PT_MB;
            }
            if (cabe && pos >= fim_velho && pos > fim_sem) {
                fim_com = pos;
                p->nmover_possivel = c.nlog - 1 - k;
            }
        } else fim_sem = ext_lim;   /* la última lógica: hasta el final (la extendida crece si hace falta) */
    }
    uint32_t max_sem = fim_sem > a->inicio ? fim_sem - a->inicio : 0;
    uint32_t max_com = fim_com > a->inicio ? fim_com - a->inicio : 0;
    p->maximo = mover && max_com > max_sem ? max_com : max_sem;
    p->maximo_sem_mover = max_sem;
    uint32_t alvo = setores ? abaixo(a->inicio + setores) - a->inicio : p->maximo;
    if (!alvo || alvo < PT_MB) { txt(erro, max, "el tamaño nuevo es demasiado pequeño"); return ERRO_INVALIDO; }
    if (alvo > p->maximo) {
        if (!mover && max_com > max_sem)
            txt(erro, max, "como mucho %u MB sin mover las particiones de detrás (moviéndolas, %u MB)", max_sem / PT_MB, max_com / PT_MB);
        else if (max_com == 0 && numero > 4 && numero - 5 + 1 < c.nlog && max_sem < alvo)
            txt(erro, max, "como mucho %u MB: las particiones de detrás no caben en el espacio libre del final para moverlas sin riesgo", max_sem / PT_MB);
        else txt(erro, max, "como mucho %u MB", p->maximo / PT_MB);
        return ERRO_LIMITE;
    }
    p->depois = alvo;
    if (alvo > max_sem) {   /* hay que mover */
        p->nmover = p->nmover_possivel;
        for (int j = 0; j < p->nmover; j++) {
            p->mover[j] = numero + 1 + j;
            p->mover_setores += c.log[numero - 5 + 1 + j].setores;
        }
    } else memset(p->novo_ini, 0, sizeof p->novo_ini);
    /* ¿La extendida tiene que crecer? Cuando se mueve algo al final, o la última lógica pasa de su final. */
    if (numero > 4) {
        uint32_t ext_fim = c.prim[x].inicio + c.prim[x].setores, precisa = a->inicio + alvo;
        if (p->nmover) precisa = limite_extendida(&c, x);
        if (precisa > ext_fim) p->ext_fim = limite_extendida(&c, x);
    }
    /* Comprobación final: la tabla resultante tiene que ser válida. */
    pt_tabela_t r = c;
    char e[120];
    if (p->ext_fim && pt_redimensionar(&r, x + 1, p->ext_fim - r.prim[x].inicio, e, sizeof e) < 0) { txt(erro, max, "la extendida: %s", e); return ERRO_INVALIDO; }
    for (int j = 0; j < p->nmover; j++) {
        pt_part_t *l = &r.log[numero - 5 + 1 + j];
        l->inicio = p->novo_ini[j];
        l->ebr = p->novo_ini[j] - PT_MB;
    }
    pt_part_t *ra = pt_achar(&r, numero);
    ra->setores = alvo;
    if (!pt_validar(&r, e, sizeof e)) { txt(erro, max, "la tabla resultante no sería válida: %s", e); return ERRO_INVALIDO; }
    return 0;
}

/* ------------------------------------------------------------ hacerlo */

typedef struct { char vol[16], ponto[64]; bool so_leitura; } montagem_guardada_t;

static int desmontar_se(const char *disco, int numero, montagem_guardada_t *g)
{
    info_disco_t d;
    char vol[16];
    pt_nome_particao(disco, numero, vol, sizeof vol);
    memset(g, 0, sizeof *g);
    copia_texto(g->vol, vol, sizeof g->vol);
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (compara_texto(d.nome, vol) == 0 && d.montado) {
            copia_texto(g->ponto, d.ponto, sizeof g->ponto);
            g->so_leitura = (d.flags & DISCO_SO_LEITURA) != 0;
            int r = desmontar(d.ponto);
            registrar("particiones: %s desmontada de %s para cambiar tamaños: %d", vol, g->ponto, r);
            return r;
        }
    return 0;
}

static void remontar(montagem_guardada_t *g, int numero_novo, const char *disco)
{
    if (!g->ponto[0]) return;
    char vol[16];
    pt_nome_particao(disco, numero_novo, vol, sizeof vol);
    criar_pasta(g->ponto);
    int r = montar_em(vol, g->ponto, g->so_leitura ? MONTAR_SO_LEITURA : MONTAR_LEITURA_ESCRITA);
    registrar("particiones: %s montada otra vez en %s: %d", vol, g->ponto, r);
}

/* Copia [de, de+n) a [para, para+n) del disco y lo relee. Las dos zonas no se solapan. */
static int copiar_zona(const char *disco, uint32_t de, uint32_t para, uint32_t n, pt_tamanho_t *p, char *m, int max)
{
    enum { PASSO = 256 };
    uint8_t *a = aloca(PASSO * 512), *b = aloca(PASSO * 512);
    if (!a || !b) { if (a) libera(a); if (b) libera(b); txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    int r = 0;
    for (int volta = 0; volta < 2 && r == 0; volta++)
        for (uint32_t f = 0; f < n && r == 0; ) {
            uint32_t q = n - f > PASSO ? PASSO : n - f;
            if ((r = blocos_ler(disco, de + f, q, a)) < 0) { txt(m, max, "error al leer el sector %u: %s", de + f, nome_erro(r)); break; }
            if (volta == 0) {
                if ((r = blocos_escrever(disco, para + f, q, a)) < 0) { txt(m, max, "error al escribir el sector %u: %s", para + f, nome_erro(r)); break; }
            } else {
                if ((r = blocos_ler(disco, para + f, q, b)) < 0) { txt(m, max, "error al releer el sector %u: %s", para + f, nome_erro(r)); break; }
                if (memcmp(a, b, q * 512) != 0) { txt(m, max, "la copia releída no es igual al original (sector %u)", para + f); r = ERRO_FORMATO; break; }
            }
            f += q;
            p->copiados += q;
            if (p->progresso) p->progresso((int)((uint64_t)p->copiados * 100 / ((uint64_t)p->mover_setores * 2 + 1)), p->ctx);
        }
    libera(a);
    libera(b);
    return r;
}

int pt_mudar_tamanho(pt_tamanho_t *p, int conf, bool desmontar_permitido, char *m, int max)
{
    if (m && max) m[0] = 0;
    pt_tabela_t t;
    int r = pt_ler(p->disco, &t);
    if (r < 0) { txt(m, max, "no se puede leer la tabla de %s: %s", p->disco, nome_erro(r)); return r; }
    /* El plan se rehace sobre la tabla de ahora: si el disco cambió, se nota aquí. */
    pt_tamanho_t q;
    char e[200];
    if ((r = pt_planejar_tamanho(&t, p->numero, p->depois, p->nmover > 0, &q, e, sizeof e)) < 0) { txt(m, max, "%s", e); return r; }
    if (q.depois != p->depois || q.nmover != p->nmover || q.ext_fim != p->ext_fim) { txt(m, max, "la tabla de %s ha cambiado desde que se calculó el plan: no se toca", p->disco); return ERRO_INVALIDO; }
    char vol[16], cmp[200] = "", extra[200] = "";
    pt_nome_particao(p->disco, p->numero, vol, sizeof vol);
    if (p->depois < p->antes) {   /* encoger: pt_executar ya lo hace en el orden seguro */
        montagem_guardada_t g;
        if ((r = desmontar_se(p->disco, p->numero, &g)) < 0 || (g.ponto[0] && !desmontar_permitido)) {
            if (g.ponto[0] && !desmontar_permitido) remontar(&g, p->numero, p->disco);
            txt(m, max, "%s está montada en %s: desmóntala antes", vol, g.ponto);
            return ERRO_OCUPADO;
        }
        pt_acao_t a;
        memset(&a, 0, sizeof a);
        a.op = PA_REDIMENSIONAR;
        copia_texto(a.disco, p->disco, sizeof a.disco);
        a.numero = p->numero;
        a.setores = p->depois;
        a.conf = conf;
        a.progresso = p->progresso;
        a.ctx = p->ctx;
        r = pt_executar(&a, m, max);
        remontar(&g, p->numero, p->disco);
        return r;
    }
    if (p->depois == p->antes && !p->nmover) { txt(m, max, "%s ya mide %u MB", vol, p->antes / PT_MB); return 0; }
    char tipo_fs[16];
    uint32_t ocupa = 0;
    unsigned t0 = 0, t1 = 0;
    bool fat = false;
    /* Lo montado de lo que se toca, fuera (y se recuerda dónde estaba): antes de comprobarlo, porque
     * un ext2 montado dice «no desmontado bien» hasta que se desmonta. */
    montagem_guardada_t gm[PT_MAX_LOG + 1];
    int ng = 0;
    for (int j = 0; j <= p->nmover; j++) {
        int num = j < p->nmover ? p->mover[j] : p->numero;
        info_disco_t d;
        char v2[16];
        pt_nome_particao(p->disco, num, v2, sizeof v2);
        bool esta = false;
        for (int i = 0; listar_discos(i, &d) == 1; i++) if (compara_texto(d.nome, v2) == 0 && d.montado) esta = true;
        if (esta && !desmontar_permitido) { txt(m, max, "%s está montada: desmóntala antes (o pide que se desmonte)", v2); r = ERRO_OCUPADO; goto remontar_todo; }
        if ((r = desmontar_se(p->disco, num, &gm[ng])) < 0) { txt(m, max, "no se puede desmontar %s: %s", v2, nome_erro(r)); ng++; goto remontar_todo; }
        ng++;
    }
    /* 1. El sistema de archivos tiene que estar sano antes de nada (fsck de solo lectura). */
    t0 = tempo_ms();   /* D338: el tiempo de cada paso, al registro */
    ocupa = fs_tamanho(vol, tipo_fs, sizeof tipo_fs);
    fat = ocupa && compara_texto(tipo_fs, "FAT32") == 0;
    if (ocupa && (r = fs_verificar(vol, cmp, sizeof cmp)) < 0) { txt(m, max, "comprobación previa de %s: %s. No se ha cambiado nada", vol, cmp); goto remontar_todo; }
    registrar("particiones: tamaño de %s: comprobación previa en %u ms", vol, (t1 = tempo_ms()) - t0);
    if ((r = blocos_reservar(p->disco, conf | DBLOCO_CONF_MONTADO)) < 0) {
        txt(m, max, r == ERRO_DISCO_ARRANQUE ? "%s es el disco de arranque: hace falta confirmarlo" : "no se puede reservar %s: %s", p->disco, nome_erro(r));
        goto remontar_todo;
    }
    int x = pt_extendida(&t);
    /* 2. La extendida. */
    if (p->ext_fim) {
        if ((r = pt_redimensionar(&t, x + 1, p->ext_fim - t.prim[x].inicio, m, max)) < 0 || (r = pt_gravar(&t, "ampliar la extendida", m, max)) < 0) goto soltar;
    }
    /* 3. Copiar y comprobar las que se mueven (a espacio libre, sin solaparse). */
    p->copiados = 0;
    for (int j = 0; j < p->nmover; j++) {
        pt_part_t *l = &t.log[p->mover[j] - 5];
        registrar("particiones: %s: la %d se copia de %u a %u (%u sectores)", p->disco, p->mover[j], l->inicio, p->novo_ini[j], l->setores);
        if ((r = copiar_zona(p->disco, l->inicio, p->novo_ini[j], l->setores, p, m, max)) < 0) goto soltar;
    }
    /* 4. La tabla con las lógicas en su sitio nuevo (un solo EBR enlaza la cadena nueva). */
    if (p->nmover) {
        unsigned t2 = tempo_ms();
        registrar("particiones: tamaño de %s: %u MB copiados y releídos en %u ms", vol, p->mover_setores / PT_MB, t2 - t1);
        t1 = t2;
        for (int j = 0; j < p->nmover; j++) {
            pt_part_t *l = &t.log[p->mover[j] - 5];
            l->inicio = p->novo_ini[j];
            l->ebr = p->novo_ini[j] - PT_MB;
        }
        if ((r = pt_gravar(&t, "mover lógicas al final", m, max)) < 0) goto soltar;
        registrar("particiones: %s: %d lógicas movidas al final del disco", p->disco, p->nmover);
    }
    /* 5. Primero la partición, después su sistema de archivos. */
    if ((r = pt_redimensionar(&t, p->numero, p->depois, m, max)) < 0 || (r = pt_gravar(&t, "ampliar partición", m, max)) < 0) goto soltar;
    if (ocupa) {
        if (fat) fs_crescer_fat32(vol, extra, sizeof extra);
        else if ((r = fs_redimensionar_ext2(vol, p->depois, p->progresso, p->ctx, extra, sizeof extra)) < 0) {
            txt(m, max, "%s ya mide %u MB, pero su ext2 no ha crecido: %s", vol, p->depois / PT_MB, extra);
            goto soltar;
        }
        unsigned t2 = tempo_ms();
        if ((r = fs_verificar(vol, cmp, sizeof cmp)) < 0) { txt(m, max, "%s ha crecido, pero la comprobación de después falla: %s", vol, cmp); goto soltar; }
        registrar("particiones: tamaño de %s: sistema de archivos ampliado en %u ms y comprobado en %u ms", vol, t2 - t1, tempo_ms() - t2);
    }
    txt(m, max, "%s pasa de %u a %u MB%s%s%s", vol, p->antes / PT_MB, p->depois / PT_MB,
        p->nmover ? " (las particiones de detrás se han movido al final del disco)" : "", extra[0] ? "; " : "", extra);
    r = 0;
soltar:
    blocos_liberar(p->disco);   /* y el kernel relee la tabla */
remontar_todo:
    for (int j = 0; j < ng; j++) remontar(&gm[j], j < p->nmover ? p->mover[j] : p->numero, p->disco);
    registrar("particiones: tamaño de %s: %d %s", vol, r, m ? m : "");
    return r;
}

/* El volumen montado en un punto ("/home" -> "hda5"). */
bool pt_volume_de_ponto(const char *ponto, char *vol, int max)
{
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++)
        if (d.particao && d.montado && compara_texto(d.ponto, ponto) == 0) { copia_texto(vol, d.nome, (size_t)max); return true; }
    return false;
}
