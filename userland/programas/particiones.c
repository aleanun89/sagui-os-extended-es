// SPDX-License-Identifier: GPL-2.0-or-later
/* particiones - la tabla de particiones de los discos, desde el terminal (D179).
 *
 *   particiones                           todos los discos con su tabla
 *   particiones <disco>                   la tabla de un disco y sus huecos libres
 *   particiones crear <disco> <MB|todo> [fat32|exfat|ext2|extendida|tipo=0xNN] [--logica]
 *                     [--inicio MB] [--etiqueta TEXTO]
 *   particiones borrar <partición> [--trim]
 *   particiones redimensionar <partición> <MB>   (y su ext2 crece o encoge; un FAT32, crece: D213)
 *   particiones ampliar <disco>           la extendida hasta el final del disco (D213)
 *   particiones ampliar /home | <partición>   la partición, hasta todo el espacio libre del disco: las
 *                                         lógicas que tenga detrás se mueven al final si hace falta (D318);
 *                                         si /home está en la RAM, se crea su partición con el espacio libre
 *   particiones tamaño /home | <partición> <8G|512M|MB|max>   crecer o encoger (D318)
 *   particiones comprobar <partición>     fsck de solo lectura del FAT32 o ext2 (D213)
 *   particiones formatear <partición> fat32|exfat|ext2 [etiqueta]
 *   particiones activa <partición>
 *   particiones trim <partición|disco>     descarta (SSD) el contenido o el espacio libre
 *   particiones deshacer <disco>           vuelve a la tabla de antes del último cambio
 *   particiones tabla-nueva <disco> [mbr|gpt]   una tabla vacía, MBR (por defecto) o GPT (borra todas las particiones)
 *
 * D266: los discos GPT se leen con comprobación de CRC (también el respaldo) y se pueden editar: crear
 * (fat32, ext2, "esp" = partición EFI FAT32 o tipo=0xEF), borrar y formatear. Una GPT ajena (Windows,
 * Linux) se muestra sin tocarla; solo se escribe cuando se pide un cambio y se confirma.
 *
 * Antes de cambiar nada dice exactamente qué se pierde y pregunta (salvo -s/--si).
 * El disco del que arrancó el sistema solo se toca con --arranque y una segunda
 * confirmación escribiendo su nombre. D213: con particiones montadas en el disco se
 * puede cambiar el resto (crear lógicas, ampliar la extendida, redimensionar lo que no
 * esté montado); lo montado y la partición de SaguiOS no se tocan nunca. */
#include "sagui.h"
#include "contas.h"
#include "particoes.h"

static bool sim_a_tudo, permitir_arranque;

static void tamanho(uint32_t setores, char *s, int max)
{
    uint32_t mb = setores / 2048;
    if (mb >= 10240) formatar_texto(s, (size_t)max, "%u,%u GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else formatar_texto(s, (size_t)max, "%u MB", mb);
}

static bool info_de(const char *nome, info_disco_t *out)
{
    for (int i = 0; listar_discos(i, out) == 1; i++) if (compara_texto(out->nome, nome) == 0) return true;
    return false;
}

static void linha_part(const pt_tabela_t *t, int num, const pt_part_t *p)
{
    char vol[16], tam[16], tipo[32];
    info_disco_t d;
    pt_nome_particao(t->disco, num, vol, sizeof vol);
    tamanho(p->setores, tam, sizeof tam);
    formatar_texto(tipo, sizeof tipo, "%s (%02x)", pt_nome_tipo(p->tipo), p->tipo);
    bool tem = info_de(vol, &d);
    imprimir("  %-3d %-8s %c %8u  %9s  %-22s %-14s %-12s %s\n", num, vol, p->ativa ? '*' : ' ', p->inicio / 2048, tam, tipo,
             pt_tipo_extendida(p->tipo) ? "" : tem ? (d.suportado ? d.formato : "-") : "-",
             tem && d.suportado ? d.rotulo : "", tem && d.montado ? d.ponto : "");
}

static int mostrar(const char *disco)
{
    pt_tabela_t t;
    int r = pt_ler(disco, &t);
    if (r < 0) { imprimir_em(ERROS, "particiones: %s: %s\n", disco, nome_erro(r)); return 1; }
    info_disco_t d;
    info_bloco_t b;
    char tam[16];
    info_de(disco, &d);
    blocos_info(disco, &b);
    tamanho(t.setores, tam, sizeof tam);
    static const char *const esq[] = { "sin tabla", "MBR", "GPT", "volumen sin tabla" };
    imprimir("%s  %s %s  %s  %s  TRIM: %s%s\n", disco, d.fabricante, d.produto, tam, esq[t.esquema],
             b.trim == TRIM_SIM ? "sí" : b.trim == TRIM_SEM_DRIVER ? "el disco sí, el driver no" : "no",
             (d.flags & DISCO_ARRANQUE_REAL) ? "  [disco de arranque]" : "");
    if (t.esquema == PT_GPT) {
        if (t.gpt_mala) {
            imprimir("  El MBR dice que es GPT, pero no hay una GPT válida: %s.\n  No se toca; \"particiones tabla-nueva %s gpt\" empieza de cero.\n", t.gpt_aviso, disco);
            return 0;
        }
        if (t.gpt_respaldo) imprimir("  AVISO: la cabecera principal de la GPT está dañada; se lee la copia de respaldo (se repara al cambiar algo).\n");
        else if (t.gpt_sem_respaldo) imprimir("  AVISO: la copia de respaldo de la GPT no es válida (no está al final del disco, o está dañada).\n");
        imprimir("  Nº  Disp.      inicio MB   tamaño  tipo                   formato        etiqueta     montada   nombre\n");
        for (int i = 0; i < t.ngpt; i++) {
            char vol[16], tp[40];
            info_disco_t dv;
            tamanho(t.gpt[i].setores, tam, sizeof tam);
            if (t.gpt[i].numero) pt_nome_particao(disco, t.gpt[i].numero, vol, sizeof vol);
            else copia_texto(vol, "-", sizeof vol);
            bool tem = t.gpt[i].numero && info_de(vol, &dv);
            copia_texto(tp, t.gpt[i].tipo, sizeof tp);
            imprimir("  %-3d %-8s %c %8u  %9s  %-22s %-14s %-12s %-9s \"%s\"%s\n", t.gpt[i].slot + 1, vol, ' ', t.gpt[i].inicio / 2048, tam, tp,
                     tem ? (dv.suportado ? dv.formato : "-") : "-", tem && dv.suportado ? dv.rotulo : "", tem && dv.montado ? dv.ponto : "",
                     t.gpt[i].nome, t.gpt[i].numero ? "" : "  (SaguiOS solo ve las cuatro primeras: no la gestiona)");
        }
        pt_livre_t v[24];
        int n = pt_livres(&t, v, 24);
        for (int i = 0; i < n; i++) {
            tamanho(v[i].setores, tam, sizeof tam);
            imprimir("  libre          %8u  %9s\n", v[i].inicio / 2048, tam);
        }
        return 0;
    }
    if (t.esquema == PT_VOLUME) { imprimir("  El disco entero es un volumen (%s), sin tabla de particiones.\n", d.formato); return 0; }
    imprimir("  Nº  Disp.      inicio MB   tamaño  tipo                   formato        etiqueta     montada\n");
    for (int i = 0; i < 4; i++) {
        if (!t.prim[i].tipo) continue;
        linha_part(&t, i + 1, &t.prim[i]);
        if (pt_tipo_extendida(t.prim[i].tipo))
            for (int k = 0; k < t.nlog; k++) linha_part(&t, k + 5, &t.log[k]);
    }
    pt_livre_t v[24];
    int n = pt_livres(&t, v, 24);
    for (int i = 0; i < n; i++) {
        tamanho(v[i].setores, tam, sizeof tam);
        imprimir("  libre          %8u  %9s  %s\n", v[i].inicio / 2048, tam, v[i].logica ? "(lógica, en la extendida)" : "(primaria)");
    }
    int pd = pt_passos_deshacer(disco);
    if (pd) imprimir("  Cambios que se pueden deshacer: %d (particiones deshacer %s)\n", pd, disco);
    return 0;
}

static int listar_todos(void)
{
    info_disco_t d;
    int n = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (d.particao) continue;
        if (n++) imprimir("\n");
        mostrar(d.nome);
    }
    if (!n) imprimir("No hay discos.\n");
    return 0;
}

/* Pregunta en el terminal; "sí"/"si"/"s" sigue. */
static bool perguntar(const char *pergunta)
{
    if (sim_a_tudo) return true;
    char r[32];
    imprimir("%s [escribe sí para seguir]: ", pergunta);
    if (ler_linha(ENTRADA, r, sizeof r) < 0) return false;
    return compara_texto(r, "sí") == 0 || compara_texto(r, "si") == 0 || compara_texto(r, "s") == 0;
}

/* El disco de arranque: --arranque y, además, escribir su nombre. */
static bool confirmar_arranque(pt_acao_t *a)
{
    info_disco_t d;
    if (!info_de(a->disco, &d) || !(d.flags & DISCO_ARRANQUE_REAL)) return true;
    if (!permitir_arranque) {
        imprimir_em(ERROS, "particiones: %s es el disco de arranque de SaguiOS. Si de verdad quieres cambiarlo, añade --arranque.\n", a->disco);
        return false;
    }
    if (!sim_a_tudo) {
        char r[32];
        imprimir("ATENCIÓN: %s es el disco del que ha arrancado el sistema. Escribe su nombre para confirmarlo: ", a->disco);
        if (ler_linha(ENTRADA, r, sizeof r) < 0 || compara_texto(r, a->disco) != 0) { imprimir("No se ha cambiado nada.\n"); return false; }
    }
    a->conf |= DBLOCO_CONF_ARRANQUE;
    return true;
}

static void progresso(int pc, void *ctx)
{
    (void)ctx;
    static int ult = -1;
    if (pc / 10 != ult / 10 || pc == 100) { imprimir("\r  %d %%", pc); ult = pc; }
    if (pc == 100) { imprimir("\n"); ult = -1; }
}

static int usar(void)
{
    imprimir_em(ERROS,
        "uso: particiones [disco]\n"
        "     particiones crear <disco> <MB|todo> [fat32|exfat|ext2|esp|extendida|tipo=0xNN] [--logica] [--inicio MB] [--etiqueta X]\n"
        "     particiones borrar <partición> [--trim]\n"
        "     particiones redimensionar <partición> <MB>\n"
        "     particiones ampliar <disco> | /home | <partición>\n"
        "     particiones tamaño /home | <partición> <8G|512M|MB|max>\n"
        "     particiones comprobar <partición>\n"
        "     particiones formatear <partición> fat32|exfat|ext2 [etiqueta]\n"
        "     particiones activa <partición>\n"
        "     particiones trim <partición|disco>\n"
        "     particiones deshacer <disco>\n"
        "     particiones tabla-nueva <disco> [mbr|gpt]\n"
        "opciones: -s/--si (no preguntar), --arranque (permitir el disco de arranque)\n");
    return 2;
}

static void ayuda(void)
{
    imprimir(
        "particiones: la tabla de particiones de los discos (MBR: primarias, extendida y lógicas; GPT: hasta\n"
        "cuatro entradas, con la partición EFI).\n\n"
        "  particiones [disco]                 la tabla y los huecos libres\n"
        "  particiones crear <disco> <MB|todo> [fat32|exfat|ext2|esp|extendida|tipo=0xNN] [--logica]\n"
        "              [--inicio MB] [--etiqueta X]   una partición nueva en un hueco libre (esp = EFI, FAT32)\n"
        "  particiones borrar <partición> [--trim]    quita la partición de la tabla (se puede deshacer)\n"
        "  particiones redimensionar <partición> <MB> mueve su final; un ext2 crece o encoge con ella\n"
        "              (encoger solo si lo que se corta está libre: no se mueven datos); un FAT32\n"
        "              crece hasta donde le dé su FAT. Antes y después, una comprobación sin escribir.\n"
        "  particiones ampliar <disco>         la extendida hasta el final del disco (tras grabar la\n"
        "              imagen en un pendrive más grande); luego, \"crear ... --logica\" en el hueco\n"
        "  particiones ampliar /home           /home hasta todo el espacio libre del disco (las particiones de\n"
        "              detrás se mueven al final si hace falta; si /home está en la RAM, se le crea partición)\n"
        "  particiones ampliar <partición>     lo mismo con cualquier partición FAT32 o ext2\n"
        "  particiones tamaño <partición|/home> <8G|512M|MB|max>   crecer o encoger (encoger: ext2 y FAT32,\n"
        "              solo si lo que se corta está libre; NTFS no se toca)\n"
        "  particiones comprobar <partición>   comprueba el FAT32 o el ext2 sin cambiar nada\n"
        "  particiones formatear <partición> fat32|exfat|ext2 [etiqueta]\n"
        "  particiones activa | trim | deshacer | tabla-nueva <disco> [mbr|gpt] ...\n"
        "  En un disco GPT se puede crear, borrar y formatear; el resto (activa, tamaño, deshacer) solo en MBR.\n\n"
        "Lo montado no se toca: desmóntalo antes (umount). La partición de SaguiOS del disco de\n"
        "arranque, nunca. El disco de arranque pide --arranque y escribir su nombre (o -s).\n"
        "Opciones: -s/--si (no preguntar), --arranque (permitir el disco de arranque).\n");
}

/* Qué se pierde al borrar/formatear: el formato, la etiqueta y si tiene datos. */
static void descrever(const char *vol, char *s, int max)
{
    info_disco_t d;
    if (info_de(vol, &d)) formatar_texto(s, (size_t)max, "%s (%u MB, %s%s%s%s)%s", vol, d.setores / 2048, d.formato,
                                         d.rotulo[0] ? ", \"" : "", d.rotulo, d.rotulo[0] ? "\"" : "",
                                         /* D256: aviso fuerte: es la configuración del sistema */
                                         compara_texto(d.rotulo, "SAGUI-CONF") == 0
                                         ? "  ATENCION: es la CONFIGURACION DEL SISTEMA (usuarios, contrasenas y claves); sin ella SaguiOS arranca con los valores de fabrica" : "");
    else copia_texto(s, vol, (size_t)max);
}

/* D318: "8G", "512M", "1024" (MB) o "max"/"todo" (0). -1 si no se entiende. */
static int64_t ler_tamanho(const char *t)
{
    if (compara_texto(t, "max") == 0 || compara_texto(t, "todo") == 0 || compara_texto(t, "máximo") == 0) return 0;
    uint64_t v = 0, frac = 0, div = 1;
    const char *p = t;
    if (*p < '0' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') v = v * 10 + (uint64_t)(*p++ - '0');
    if (*p == ',' || *p == '.') { p++; while (*p >= '0' && *p <= '9' && div < 1000) { frac = frac * 10 + (uint64_t)(*p++ - '0'); div *= 10; } }
    uint64_t mb;
    if (*p == 'G' || *p == 'g') { mb = v * 1024 + frac * 1024 / div; p++; }
    else if (*p == 'T' || *p == 't') { mb = v * 1024 * 1024 + frac * 1024 * 1024 / div; p++; }
    else { mb = v; if (*p == 'M' || *p == 'm') p++; }
    if (*p == 'B' || *p == 'b') p++;
    if (*p || !mb || mb > 2u * 1024 * 1024) return -1;
    return (int64_t)mb * 2048;
}

static void progresso_mover(int pc, void *ctx) { progresso(pc > 100 ? 100 : pc, ctx); }

/* D318: «particiones ampliar /home» y «particiones tamaño X N». */
static int mudar_tamanho(const char *alvo, const char *qtd, bool ampliar)
{
    char vol[16], disco[12], m[300];
    int num = 0;
    bool home = compara_texto(alvo, "/home") == 0;
    if (alvo[0] == '/') {
        if (!pt_volume_de_ponto(alvo, vol, sizeof vol)) {
            if (home && ampliar) {   /* /home en la RAM: se crea su partición */
                home_plano_t h;
                int r = home_planejar(&h, m, sizeof m);
                if (r < 0) { imprimir_em(ERROS, "particiones: %s\n", m); return 1; }
                if (!h.ganho) { imprimir("%s\n", m); return 0; }
                imprimir("/home está en la RAM (se pierde al apagar). Se va a crear en %s una partición SAGUI-DATOS (ext2) de %u MB\n"
                         "con el espacio libre, copiar a ella lo que hay ahora en /home y montarla en /home.\n", h.disco, h.criar_set / 2048);
                if (!perguntar("¿Crear la partición de /home?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
                r = home_ampliar(&h, progresso, NULL, m, sizeof m);
                if (r < 0) { imprimir_em(ERROS, "particiones: %s\n", m); return 1; }
                imprimir("%s\n", m);
                return 0;
            }
            imprimir_em(ERROS, "particiones: no hay ninguna partición montada en %s\n", alvo);
            return 1;
        }
    } else copia_texto(vol, alvo, sizeof vol);
    if (!pt_separar_nome(vol, disco, sizeof disco, &num) || !num) { imprimir_em(ERROS, "particiones: %s no es una partición (mira \"discos\")\n", vol); return 1; }
    int64_t set = ampliar ? 0 : ler_tamanho(qtd);
    if (set < 0) { imprimir_em(ERROS, "particiones: tamaño no válido: %s (ejemplos: 8G, 512M, 2048, max)\n", qtd); return 2; }
    pt_tabela_t t;
    if (pt_ler(disco, &t) < 0) { imprimir_em(ERROS, "particiones: no se puede leer %s\n", disco); return 1; }
    pt_part_t *pp = pt_achar(&t, num);
    if (pp && pp->tipo == 0x07) {
        imprimir_em(ERROS, "particiones: %s es NTFS: SaguiOS no escribe en NTFS, así que no cambia su tamaño (encógela desde Windows; "
                           "el espacio libre que haya sí se puede usar)\n", vol);
        return 1;
    }
    pt_tamanho_t p;
    int r = pt_planejar_tamanho(&t, num, (uint32_t)set, true, &p, m, sizeof m);
    if (r < 0) { imprimir_em(ERROS, "particiones: %s: %s\n", vol, m); return 1; }
    if (p.depois < p.antes) {
        uint32_t min = fs_minimo(vol);
        if (min && p.depois < min) {
            imprimir_em(ERROS, "particiones: %s no se puede encoger por debajo de %u MB sin mover datos (lo que ocupa)\n", vol, min / 2048);
            return 1;
        }
    }
    if (p.depois == p.antes && !p.nmover) { imprimir("%s ya mide %u MB.\n", vol, p.antes / 2048); return 0; }
    info_disco_t d;
    bool arranque = info_de(disco, &d) && (d.flags & DISCO_ARRANQUE_REAL);
    char que[360];
    descrever(vol, que, sizeof que);
    imprimir("%s pasará de %u a %u MB%s.\n", que, p.antes / 2048, p.depois / 2048, p.depois > p.antes ? "" : " (encoger: solo si lo que se corta está libre)");
    if (p.ext_fim) imprimir("  La extendida crece hasta el final del disco.\n");
    for (int j = 0; j < p.nmover; j++) {
        char v2[16];
        pt_nome_particao(disco, p.mover[j], v2, sizeof v2);
        descrever(v2, que, sizeof que);
        imprimir("  Se mueve al final del disco: %s (se copia, se relee y se compara antes de cambiar la tabla)\n", que);
    }
    imprimir("  Antes se comprueba el sistema de archivos; lo montado que se toca se desmonta y se vuelve a montar.\n");
    if (!perguntar(p.depois > p.antes ? "¿Ampliar?" : "¿Encoger?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    int conf = 0;
    if (arranque) {
        /* /home (y lo que se mueva para ampliarlo) es lo único del disco de arranque que se toca sin
         * --arranque: la orden lo nombra a propósito. La partición de SaguiOS y la ESP, nunca. */
        if (!home && !permitir_arranque) {
            imprimir_em(ERROS, "particiones: %s es el disco de arranque de SaguiOS. Si de verdad quieres cambiarlo, añade --arranque.\n", disco);
            return 1;
        }
        conf = DBLOCO_CONF_ARRANQUE;
    }
    p.progresso = progresso_mover;
    r = pt_mudar_tamanho(&p, conf, true, m, sizeof m);
    if (r < 0) { imprimir_em(ERROS, "particiones: %s\n", m[0] ? m : nome_erro(r)); return 1; }
    imprimir("%s\n", m);
    return 0;
}

int principal(int argc, char **argv)
{
    char *pos[8];
    int np = 0;
    pt_acao_t a;
    memset(&a, 0, sizeof a);
    a.progresso = progresso;
    const char *etiqueta = NULL, *inicio = NULL;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-s") == 0 || compara_texto(argv[i], "--si") == 0) sim_a_tudo = true;
        else if (compara_texto(argv[i], "--arranque") == 0) permitir_arranque = true;
        else if (compara_texto(argv[i], "--logica") == 0) a.logica = true;
        else if (compara_texto(argv[i], "--trim") == 0) a.trim = true;
        else if (compara_texto(argv[i], "--etiqueta") == 0 && i + 1 < argc) etiqueta = argv[++i];
        else if (compara_texto(argv[i], "--inicio") == 0 && i + 1 < argc) inicio = argv[++i];
        else if (compara_texto(argv[i], "-h") == 0 || compara_texto(argv[i], "--ayuda") == 0) { ayuda(); return 0; }
        else if (np < 8) pos[np++] = argv[i];
    }
    if (falta_admin("particiones")) return 1;   /* D255: lee y escribe los discos en crudo */
    if (np == 0) return listar_todos();
    const char *orden = pos[0];
    if (np == 1) return mostrar(orden);
    /* D318: ampliar una partición o /home, y tamaño. */
    bool tam = compara_texto(orden, "tamaño") == 0 || compara_texto(orden, "tamano") == 0;
    if (tam || compara_texto(orden, "ampliar") == 0) {
        int nn = 0;
        char dd[12];
        bool e_part = pos[1][0] == '/' || (pt_separar_nome(pos[1], dd, sizeof dd, &nn) && nn);
        if (tam && np < 3) return usar();
        if (tam || e_part) return mudar_tamanho(pos[1], tam ? pos[2] : NULL, !tam);
    }
    char m[200];
    int num = 0;
    if (!pt_separar_nome(pos[1], a.disco, sizeof a.disco, &num)) {
        imprimir_em(ERROS, "particiones: no existe %s (mira \"discos\")\n", pos[1]);
        return 1;
    }
    a.numero = num;
    char que[360];
    descrever(pos[1], que, sizeof que);
    if (num) {   /* D266: aviso fuerte para la partición EFI (ESP), como el de SAGUI-CONF (D256) */
        pt_tabela_t tt;
        if (pt_ler(a.disco, &tt) == 0 && (tt.esquema == PT_MBR || tt.esquema == PT_GPT)) {
            pt_part_t *pp = pt_achar(&tt, num);
            if (pp && pp->tipo == 0xEF)
                junta_texto(que, "  ATENCION: es la PARTICION EFI (ESP): lleva el cargador UEFI; sin ella este disco no arranca por UEFI", sizeof que);
        }
    }

    if (compara_texto(orden, "crear") == 0) {
        if (np < 3 || num) return usar();
        a.op = PA_CRIAR;
        if (compara_texto(pos[2], "todo") != 0) {
            int mb;
            if (!texto_para_numero(pos[2], &mb) || mb <= 0) return usar();
            a.setores = (uint32_t)mb * 2048;
        }
        if (np >= 4) {
            if (compara_texto(pos[3], "fat32") == 0) a.fs = PA_FS_FAT32;
            else if (compara_texto(pos[3], "exfat") == 0) a.fs = PA_FS_EXFAT;   /* D325 */
            else if (compara_texto(pos[3], "ext2") == 0) a.fs = PA_FS_EXT2;
            else if (compara_texto(pos[3], "extendida") == 0) a.tipo = 0x0F;
            else if (compara_texto(pos[3], "esp") == 0) { a.fs = PA_FS_FAT32; a.tipo = 0xEF; if (!etiqueta) etiqueta = "SAGUI-ESP"; }   /* D266 */
            else if (comeca_com(pos[3], "tipo=")) {
                const char *h = pos[3] + 5;
                if (h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h += 2;
                unsigned v = 0;
                for (; *h; h++) v = v * 16 + (unsigned)(*h <= '9' ? *h - '0' : (*h | 32) - 'a' + 10);
                a.tipo = (uint8_t)v;
            } else return usar();
        }
        if (inicio) { int mb; if (!texto_para_numero(inicio, &mb)) return usar(); a.inicio = (uint32_t)mb * 2048; }
        if (etiqueta) copia_texto(a.rotulo, etiqueta, sizeof a.rotulo);
    } else if (compara_texto(orden, "borrar") == 0) {
        if (!num) return usar();
        a.op = PA_APAGAR;
        imprimir("Se va a borrar %s%s.\n", que, a.trim ? " y DESCARTAR su contenido (TRIM: no se puede recuperar ni con deshacer)" : "");
        if (!perguntar("¿Borrar la partición?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    } else if (compara_texto(orden, "redimensionar") == 0) {
        int mb;
        if (!num || np < 3 || !texto_para_numero(pos[2], &mb) || mb <= 0) return usar();
        a.op = PA_REDIMENSIONAR;
        a.setores = (uint32_t)mb * 2048;
        imprimir("%s pasará a medir %d MB (antes se comprueba su sistema de archivos).\n", que, mb);
        if (!perguntar("¿Cambiar el tamaño?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    } else if (compara_texto(orden, "ampliar") == 0) {
        if (num) return usar();
        a.op = PA_AMPLIAR;
        imprimir("La extendida de %s va a llegar hasta el final del disco (las particiones no cambian).\n", a.disco);
        if (!perguntar("¿Ampliar la extendida?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    } else if (compara_texto(orden, "comprobar") == 0) {
        if (!num) return usar();
        info_disco_t dv;
        if (info_de(pos[1], &dv) && dv.montado)
            imprimir("Aviso: %s está montada en %s; lo que esté a medio escribir puede no cuadrar todavía.\n", pos[1], dv.ponto);
        int r = fs_verificar(pos[1], m, sizeof m);
        if (r < 0) { imprimir_em(ERROS, "particiones: %s\n", m); return 1; }
        imprimir("%s\n", m);
        return 0;
    } else if (compara_texto(orden, "formatear") == 0) {
        if (!num || np < 3) return usar();
        a.op = PA_FORMATAR;
        if (compara_texto(pos[2], "fat32") == 0) a.fs = PA_FS_FAT32;
        else if (compara_texto(pos[2], "exfat") == 0) a.fs = PA_FS_EXFAT;   /* D325 */
        else if (compara_texto(pos[2], "ext2") == 0) a.fs = PA_FS_EXT2;
        else return usar();
        copia_texto(a.rotulo, np >= 4 ? pos[3] : etiqueta ? etiqueta : "", sizeof a.rotulo);
        imprimir("Se va a formatear %s en %s: SE BORRA TODO LO QUE HAY DENTRO.\n", que, pt_nome_fs(a.fs));
        if (!perguntar("¿Formatear?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    } else if (compara_texto(orden, "activa") == 0) {
        if (!num) return usar();
        a.op = PA_ATIVAR;
    } else if (compara_texto(orden, "trim") == 0) {
        a.op = PA_TRIM;
        imprimir(num ? "Se va a descartar (TRIM) TODO el contenido de %s: no se puede recuperar.\n"
                     : "Se va a descartar (TRIM) el espacio libre de %s (las particiones no se tocan).\n", que);
        if (!perguntar("¿Descartar?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
    } else if (compara_texto(orden, "deshacer") == 0) {
        if (num) return usar();
        a.op = PA_DESHACER;
    } else if (compara_texto(orden, "tabla-nueva") == 0) {
        if (num) return usar();
        a.op = PA_TABELA_NOVA;
        if (np >= 3) {
            if (compara_texto(pos[2], "gpt") == 0) a.op = PA_TABELA_GPT;
            else if (compara_texto(pos[2], "mbr") != 0) return usar();
        }
        imprimir("Se va a crear una tabla %s vacía en %s: TODAS sus particiones desaparecen.\n", a.op == PA_TABELA_GPT ? "GPT" : "MBR", que);
        if (!perguntar("¿Tabla nueva?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
        pt_tabela_t t;
        if (pt_ler(a.disco, &t) == 0 && t.esquema == PT_GPT) {
            if (!perguntar("El disco es GPT: se borrará la GPT entera. ¿Seguro?")) { imprimir("No se ha cambiado nada.\n"); return 1; }
        }
    } else return usar();

    if (!confirmar_arranque(&a)) return 1;
    int r = pt_executar(&a, m, sizeof m);
    if (r < 0) { imprimir_em(ERROS, "particiones: %s\n", m[0] ? m : nome_erro(r)); return 1; }
    imprimir("%s\n", m);
    return 0;
}
