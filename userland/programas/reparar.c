// SPDX-License-Identifier: GPL-3.0-or-later
/* reparar - repara un ext2 que quedó mal tras un corte o un cuelgue (D397).
 *
 *   reparar <partición>              dice lo que está mal, pregunta y lo arregla
 *   reparar -n <partición>           solo dice lo que haría (no escribe nada, ni desmonta)
 *   reparar -y <partición>           lo arregla sin preguntar
 *   reparar -p <partición>           «preen»: lo arregla sin preguntar solo si no se pierde nada (mapas,
 *                                    contadores, enlaces, punteros a bloques que no existen, bloques
 *                                    compartidos con copia); si hace falta algo más, no toca nada (sale con 4)
 *   reparar --arranque               lo que lanza el servicio del arranque: cada ext2 montado de solo lectura
 *                                    por no haberse desmontado bien se repara con -p y se vuelve a montar con
 *                                    escritura; lo que no se pueda, se queda en solo lectura y se avisa
 *     --copia ARCHIVO                 antes de escribir, guarda ahí los bloques que cambian (por defecto en
 *                                     /tmp/reparar/; con --sin-copia, no)
 *     --no-montar                     no vuelve a montarla al acabar
 *   reparar --deshacer ARCHIVO <partición>   vuelve a poner los bloques de una copia
 *
 * Solo con volúmenes sin montar o montados de solo lectura (el kernel monta así uno que no se desmontó
 * bien y no puede comprobar): se desmonta, se repara, se comprueba otra vez y se vuelve a montar donde
 * estaba, ya con escritura. Nunca uno montado con escritura, ni la raíz en uso (la base o la capa de la
 * live: para la capa, arranca con «todo a la RAM»). El trabajo lo hace reparo_ext2.c (libsagui).
 *
 * Termina con 0 si estaba bien, 1 si lo ha reparado, 4 si quedan problemas (con -n, o si no se quiso
 * reparar) y 8 con un error. */
#include "sagui.h"
#include "contas.h"
#include "particoes.h"

static bool so_ver, sem_perguntar, sem_copia, sem_montar, preen, em_arranque;
static const char *copia_arq;

static void usar(void)
{
    imprimir_em(ERROS, "uso: reparar [-n|-y|-p] [--copia ARCHIVO|--sin-copia] [--no-montar] <partición>\n"
                       "     reparar --deshacer ARCHIVO <partición>\n"
                       "Mira «reparar --ayuda».\n");
}

static void ayuda(void)
{
    imprimir("reparar - repara un sistema de archivos ext2 que quedó mal (corte de luz, cuelgue)\n\n"
             "  reparar hda9          comprueba hda9, dice qué está mal y pregunta antes de arreglarlo\n"
             "  reparar -n hda9       solo lo dice: no escribe nada\n"
             "  reparar -y hda9       lo arregla sin preguntar\n"
             "  reparar -p hda9       lo arregla sin preguntar SOLO si no se pierde nada; si no, no toca nada\n"
             "  reparar --arranque    lo que hace el servicio del arranque con los ext2 en solo lectura (como -p)\n"
             "  --copia ARCHIVO       antes de escribir guarda ahí los bloques que se cambian (por defecto en\n"
             "                        /tmp/reparar/, que se pierde al apagar; --sin-copia: ninguna)\n"
             "  --no-montar           no la vuelve a montar al acabar\n"
             "  reparar --deshacer ARCHIVO hda9   deja hda9 como estaba antes de repararla con esa copia\n\n"
             "Lo que arregla: mapas de bits y contadores, punteros a bloques fuera del volumen, bloques que usan\n"
             "dos archivos (cada uno se queda una copia), inodos estropeados, entradas de carpeta rotas o que\n"
             "apuntan a inodos borrados, «.» y «..», archivos y carpetas sin carpeta (a /lost+found), número de\n"
             "enlaces, y la marca «no se desmontó bien». Solo ext2 (ni ext3 ni ext4: para esos, e2fsck en Linux).\n\n"
             "La partición no puede estar montada con escritura. Si está montada de solo lectura (el kernel lo\n"
             "hace con un ext2 que no se desmontó bien y no pudo comprobar), se desmonta, se repara y se vuelve a\n"
             "montar donde estaba, ya con escritura. La capa de la live (SAGUI-PERSIST) está en uso mientras\n"
             "es la raíz: para repararla, arranca con «todo a la RAM» (tecla M en el arranque).\n\n"
             "Sale con 0 si estaba bien, 1 si la ha reparado, 4 si quedan problemas y 8 con un error.\n");
}

static void linha(const char *s, void *ctx)
{
    (void)ctx;
    imprimir("%s\n", s);
}

static bool perguntar(const char *p)
{
    if (sem_perguntar) return true;
    char r[32];
    imprimir("%s [escribe sí para seguir]: ", p);
    if (ler_linha(ENTRADA, r, sizeof r) < 0) return false;
    return compara_texto(r, "sí") == 0 || compara_texto(r, "si") == 0 || compara_texto(r, "s") == 0;
}

static bool info_de(const char *nome, info_disco_t *out)
{
    for (int i = 0; listar_discos(i, out) == 1; i++) if (compara_texto(out->nome, nome) == 0) return true;
    return false;
}

/* ¿Se puede tocar? Rellena dónde está montada (ponto = "" si no) y si es de solo lectura. */
static int comprobar_volume(const char *vol, info_disco_t *d, bool escrever)
{
    if (!info_de(vol, d) || !d->particao) {
        imprimir_em(ERROS, "reparar: no existe la partición %s (mira «particiones» o «discos»)\n", vol);
        return 8;
    }
    if (d->montado && compara_texto(d->tipo_fs, "ext2") != 0 && d->tipo_fs[0]) {
        imprimir_em(ERROS, "reparar: %s es %s, no ext2: este reparador solo sabe de ext2\n", vol, d->tipo_fs);
        return 8;
    }
    char base[16], capa[16];
    if (pt_raiz_live(base, capa, sizeof base) && (compara_texto(vol, capa) == 0 || (escrever && compara_texto(vol, base) == 0))) {
        imprimir_em(ERROS, "reparar: %s es %s de la raíz en marcha: no se puede reparar ahora. Arranca con «todo a la RAM»\n"
                           "(tecla M en el arranque, opción 2) para que no esté en uso y repárala entonces.\n",
                    vol, compara_texto(vol, capa) == 0 ? "la capa de cambios (SAGUI-PERSIST)" : "la base");
        return 8;
    }
    if (d->montado && !(d->flags & DISCO_SO_LEITURA)) {
        imprimir_em(ERROS, "reparar: %s está montada con escritura en %s: no se repara un volumen en uso. Desmóntala antes\n"
                           "(umount %s) y vuelve a pasar reparar.\n", vol, d->ponto, d->ponto);
        return 8;
    }
    if (d->montado && compara_texto(d->ponto, "/") == 0) {
        imprimir_em(ERROS, "reparar: %s es la raíz del sistema (montada en /): no se puede reparar con el sistema en marcha\n", vol);
        return 8;
    }
    return 0;
}

static int reservar(const char *vol, char *disco, int max)
{
    int num = 0;
    if (!pt_separar_nome(vol, disco, max, &num) || !num) {
        imprimir_em(ERROS, "reparar: %s no es una partición\n", vol);
        return -1;
    }
    info_disco_t dd;
    int conf = DBLOCO_CONF_MONTADO;
    /* Solo se escribe dentro de esta partición (el kernel no deja salir de ella ni tocar lo montado), así que
     * en el disco de arranque basta la confirmación de reparar: no hace falta la doble de «particiones». */
    if (info_de(disco, &dd) && (dd.flags & DISCO_ARRANQUE_REAL)) conf |= DBLOCO_CONF_ARRANQUE;
    int r = blocos_reservar(disco, conf);
    if (r < 0) imprimir_em(ERROS, "reparar: no se puede reservar el disco %s: %s\n", disco, nome_erro(r));
    return r;
}

static int desmontar_vol(const char *vol, info_disco_t *d, char *ponto, int max)
{
    ponto[0] = 0;
    if (!d->montado) return 0;
    copia_texto(ponto, d->ponto, (size_t)max);
    int r = desmontar(d->ponto);
    registrar("reparar: %s desmontada de %s: %d", vol, ponto, r);
    if (r < 0) {
        imprimir_em(ERROS, "reparar: no se puede desmontar %s de %s (%s): cierra lo que la esté usando y vuelve a probar\n",
                    vol, ponto, nome_erro(r));
        ponto[0] = 0;
    } else imprimir("%s desmontada de %s.\n", vol, ponto);
    return r;
}

static void remontar(const char *vol, const char *ponto, bool rw)
{
    if (!ponto[0]) return;
    criar_pasta(ponto);
    int r = montar_em(vol, ponto, rw ? MONTAR_LEITURA_ESCRITA : MONTAR_SO_LEITURA);
    registrar("reparar: %s montada otra vez en %s (%s): %d", vol, ponto, rw ? "rw" : "ro", r);
    info_disco_t d;
    if (r < 0) imprimir_em(ERROS, "reparar: no se pudo volver a montar %s en %s: %s\n", vol, ponto, nome_erro(r));
    else if (info_de(vol, &d) && (d.flags & DISCO_SO_LEITURA))
        imprimir("%s montada otra vez en %s, pero de solo lectura (mira «dmesg»).\n", vol, ponto);
    else imprimir("%s montada otra vez en %s, %s.\n", vol, ponto, rw ? "con escritura" : "de solo lectura");
}

static int deshacer(const char *arq, const char *vol)
{
    info_disco_t d;
    int c = comprobar_volume(vol, &d, true);
    if (c) return c;
    imprimir("Se van a volver a poner en %s los bloques guardados en %s (como estaba antes de repararla).\n", vol, arq);
    if (!perguntar("¿Deshacer?")) { imprimir("No se ha cambiado nada.\n"); return 4; }
    char ponto[64], disco[12], m[300];
    if (desmontar_vol(vol, &d, ponto, sizeof ponto) < 0) return 8;
    if (reservar(vol, disco, sizeof disco) < 0) { remontar(vol, ponto, false); return 8; }
    int r = reparo_desfazer(arq, vol, m, sizeof m);
    blocos_liberar(disco);
    imprimir("%s\n", m);
    remontar(vol, ponto, false);
    return r < 0 ? 8 : 0;
}

/* El aviso al usuario (consola, registro y notificación en pantalla). */
static void avisar(const char *fmt, ...)
{
    char t[300];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(t, sizeof t, fmt, ap);
    va_end(ap);
    imprimir("reparar: %s\n", t);
    registrar("reparar: %s", t);
    aviso_tela(t, NULL);
}

/* ¿Quedó marcado «no se desmontó bien» o con errores? (el kernel no toca el superbloque al montar de solo lectura) */
static bool esta_suja(const char *vol)
{
    uint8_t sb[512];
    if (blocos_ler(vol, 2, 1, sb) < 0) return false;
    uint32_t estado = (uint32_t)sb[58] | (uint32_t)sb[59] << 8;
    return sb[56] == 0x53 && sb[57] == 0xEF && (!(estado & 1) || (estado & 2));
}

static int reparar_vol(const char *vol);

/* El servicio del arranque (D397): cada ext2 montado de solo lectura y sucio se intenta con -p. */
static int arranque(void)
{
    info_arquivo_t ia;
    if (info_arquivo(CONF_SISTEMA_PONTO "/reparar-arranque.no", &ia) == 0) {
        registrar("reparar: reparación automática desactivada (" CONF_SISTEMA_PONTO "/reparar-arranque.no)");
        return 0;
    }
    preen = sem_perguntar = em_arranque = true;
    int n = 0;
    info_disco_t d;
    char nomes[16][12];
    for (int i = 0; listar_discos(i, &d) == 1 && n < 16; i++)
        if (d.particao && d.montado && (d.flags & DISCO_SO_LEITURA) && compara_texto(d.tipo_fs, "ext2") == 0 && esta_suja(d.nome))
            copia_texto(nomes[n++], d.nome, sizeof nomes[0]);
    registrar("reparar: arranque: %d volumen(es) ext2 en solo lectura y sucios", n);
    for (int i = 0; i < n; i++) {
        int r = reparar_vol(nomes[i]);
        if (r == 1 || r == 0) avisar("%s estaba sin desmontar bien y se ha reparado: ya se puede escribir en ella", nomes[i]);
        else avisar("%s sigue de solo lectura: ejecuta «reparar %s» para verlo y repararlo", nomes[i], nomes[i]);
    }
    return 0;
}

int principal(int argc, char **argv)
{
    const char *vol = NULL, *desfazer = NULL;
    bool so_arranque = false;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-n") == 0) so_ver = true;
        else if (compara_texto(argv[i], "-p") == 0) preen = sem_perguntar = true;
        else if (compara_texto(argv[i], "--arranque") == 0) so_arranque = true;
        else if (compara_texto(argv[i], "-y") == 0 || compara_texto(argv[i], "-s") == 0 || compara_texto(argv[i], "--si") == 0)
            sem_perguntar = true;
        else if (compara_texto(argv[i], "--copia") == 0 && i + 1 < argc) copia_arq = argv[++i];
        else if (compara_texto(argv[i], "--sin-copia") == 0) sem_copia = true;
        else if (compara_texto(argv[i], "--no-montar") == 0) sem_montar = true;
        else if (compara_texto(argv[i], "--salida") == 0 && i + 1 < argc) {   /* la ventana de Discos lee el resultado de ahí */
            int fd = abrir(argv[++i], ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
            if (fd >= 0) { duplicar(fd, SAIDA); duplicar(fd, ERROS); fechar(fd); }
        }
        else if (compara_texto(argv[i], "--deshacer") == 0 && i + 1 < argc) desfazer = argv[++i];
        else if (compara_texto(argv[i], "-h") == 0 || compara_texto(argv[i], "--ayuda") == 0) { ayuda(); return 0; }
        else if (argv[i][0] == '-' || vol) { usar(); return 8; }
        else vol = argv[i];
    }
    if (so_arranque) { if (falta_admin("reparar")) return 8; return arranque(); }
    if (!vol || (so_ver && sem_perguntar)) { usar(); return 8; }
    if (falta_admin("reparar")) return 8;   /* lee y escribe el disco en crudo */
    if (desfazer) return deshacer(desfazer, vol);
    return reparar_vol(vol);
}

static int reparar_vol(const char *vol)
{
    info_disco_t d;
    int c = comprobar_volume(vol, &d, !so_ver);
    if (c) return c;
    char ponto[64] = "", disco[12], m[300];
    bool estava_ro = d.montado;
    /* Con -n se lee tal cual (un montaje de solo lectura no escribe nada); para reparar, primero se
     * desmonta: así lo que se analiza es exactamente lo que se va a escribir. */
    if (!so_ver && desmontar_vol(vol, &d, ponto, sizeof ponto) < 0) return 8;
    unsigned t0 = tempo_ms();
    reparo_t *rp = NULL;
    int n = reparo_analisar(vol, linha, NULL, &rp, m, sizeof m);
    if (n < 0) {
        imprimir_em(ERROS, "reparar: %s\n", m);
        registrar("reparar: %s: %s", vol, m);
        remontar(vol, ponto, false);
        return 8;
    }
    reparo_resumo(rp);
    uint32_t bytes = 0, nb = reparo_blocos(rp, &bytes);
    registrar("reparar: %s analizado en %u ms: %d problemas, %u bloques que cambiarían%s", vol, tempo_ms() - t0, n, nb,
              so_ver ? " (-n: no se escribe)" : "");
    if (n == 0) {
        reparo_fechar(rp);
        if (!sem_montar) remontar(vol, ponto, true);
        else if (estava_ro && ponto[0]) imprimir("Ya se puede montar con escritura: mount -o rw %s %s\n", vol, ponto);
        return 0;
    }
    if (so_ver) {
        imprimir("Con -n no se ha cambiado nada. Para arreglarlo: reparar %s%s\n", vol,
                 d.montado ? " (se desmontará y se volverá a montar)" : "");
        reparo_fechar(rp);
        return 4;
    }
    if (preen && reparo_arriscados(rp)) {
        imprimir("%s: hace falta algo que podría perder datos (%u arreglos: inodos borrados, entradas descartadas, bloques "
                 "soltados...): no se toca nada.\n", vol, reparo_arriscados(rp));
        registrar("reparar: %s: -p: %u arreglos arriesgados, no se toca", vol, reparo_arriscados(rp));
        reparo_fechar(rp);
        remontar(vol, ponto, false);
        return 4;
    }
    imprimir("Se van a escribir %u bloques de %s.\n", nb, vol);
    if (!perguntar("¿Reparar?")) {
        imprimir("No se ha cambiado nada.\n");
        reparo_fechar(rp);
        remontar(vol, ponto, false);
        return 4;
    }
    char copia[96];
    if (!sem_copia) {
        if (copia_arq) copia_texto(copia, copia_arq, sizeof copia);
        else {
            criar_pasta("/tmp/reparar");
            formatar_texto(copia, sizeof copia, "/tmp/reparar/%s-%u.copia", vol, hora_utc(NULL));
        }
        if (reparo_guardar_copia(rp, copia, m, sizeof m) < 0) {
            imprimir_em(ERROS, "reparar: %s\n", m);
            if (!perguntar("No se pudo guardar la copia. ¿Reparar sin copia?")) {
                imprimir("No se ha cambiado nada.\n");
                reparo_fechar(rp);
                remontar(vol, ponto, false);
                return 4;
            }
        } else {
            imprimir("%s. Para deshacer: reparar --deshacer %s %s%s\n", m, copia, vol,
                     copia_arq ? "" : " (en /tmp: se pierde al apagar; --copia para guardarla en otro sitio)");
            registrar("reparar: %s", m);
        }
    }
    if (reservar(vol, disco, sizeof disco) < 0) { reparo_fechar(rp); remontar(vol, ponto, false); return 8; }
    int r = reparo_gravar(rp, m, sizeof m);
    reparo_fechar(rp);
    blocos_liberar(disco);
    if (r < 0) {
        imprimir_em(ERROS, "reparar: %s\n", m);
        registrar("reparar: %s", m);
        return 8;
    }
    imprimir("%s.\n", m);
    /* Otra pasada, sin escribir: tiene que salir limpio. */
    n = reparo_analisar(vol, NULL, NULL, &rp, m, sizeof m);
    if (n == 0) imprimir("Comprobado otra vez: todo cuadra.\n");
    else if (n > 0) imprimir("Comprobado otra vez: quedan %d problemas (vuelve a pasar reparar; si persisten, e2fsck en Linux).\n", n);
    else imprimir_em(ERROS, "reparar: la comprobación de después falla: %s\n", m);
    if (rp) reparo_fechar(rp);
    registrar("reparar: %s: comprobación de después: %d", vol, n);
    if (!sem_montar && n == 0) remontar(vol, ponto, true);
    else if (ponto[0]) imprimir("No se ha vuelto a montar. Para montarla: mount -o rw %s %s\n", vol, ponto);
    else if (n == 0) imprimir("Ya se puede montar con escritura (mount -o rw %s CARPETA).\n", vol);
    return n == 0 ? 1 : 4;
}
