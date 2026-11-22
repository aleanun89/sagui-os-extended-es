// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d - el 3D por la GPU (D285).
 *
 *   gpu3d               estado: qué driver hay (o por qué SaguiGL va por software),
 *                       quién tiene el motor y cuánto ha dibujado
 *   gpu3d probar        enciende el motor si hace falta y repite la autoprueba (limpiar,
 *                       color, Z, mezcla, textura, color interpolado, especular y Z de 16
 *                       bits, mirando píxeles del cuadro) y la medida (triángulos por
 *                       segundo, relleno y lo que cuesta traer el cuadro)
 *   gpu3d probar --detalle   lo mismo y los registros de la GPU en el log del kernel
 *                       (líneas [GPU3D]; «dmesg > /home/gpu3d.txt»), con lo que dejó la GPU
 *                       en el destino tras cada etapa (D390: sentinelas a1b2c3 y 5a5a5a5a)
 *   gpu3d probar --ver [s]   (D390) además, enseña en una ventana lo que dibujó la tarjeta
 *                       en cada etapa (el color a 3x y el Z a 2x), con su nombre y su
 *                       resultado; una tecla pasa a la siguiente, Esc termina, y sin
 *                       teclado avanza sola a los «s» segundos (5). No cambia el modo de
 *                       vídeo. Sin escritorio, o con --guardar, deja cada etapa en
 *                       /home/gpu3d-etapaN.bmp
 *   /sistema/conf/gpu3d-<driver>.conf   (D386, D390; solo root) «inicio REG VALOR» o
 *                       «dibujo REG VALOR» en hexadecimal: escrituras de registros del 3D
 *                       para probar hipótesis en la máquina; cada driver declara qué
 *                       registros admite (r500, r600, simulado; intel ninguno)
 *   gpu3d apagar        SaguiGL por software hasta «gpu3d encender»
 *   gpu3d encender      vuelve a encenderlo y a probarlo (órdenes por lotes)
 *   gpu3d encender anillo    igual, con las órdenes escritas en el anillo (Intel): para
 *                       comparar los dos caminos en una máquina real
 *   gpu3d encender gtt  (D358, Intel Gen3) además, el destino y las texturas en RAM del
 *                       sistema mapeada en el GTT (8 o 16 MB): traer el cuadro es más
 *                       rápido. Escribe entradas del GTT: solo a mano. Se puede juntar
 *                       con «anillo» («gpu3d encender anillo gtt»). En las R7xx confirmadas
 *                       (la RV710 del HP dv7, D406) la GART se enciende sola con el 3D
 *   gpu3d encender sin-gtt  (D406) sin esa GART: el cuadro vuelve por la CPU
 *
 * Para comparar con SaguiGL por software: «sgldemo -g».
 * El nombre del archivo es el del comando. */
#include "sagui.h"
#include "contas.h"
#include "gui.h"
#include "imagem.h"

static const char *nome_estado(unsigned e)
{
    switch (e) {
    case GPU3D_SIN_PROBAR: return "sin probar (se prueba con el primer uso)";
    case GPU3D_ATIVO:      return "activo";
    case GPU3D_FALHOU:     return "apagado por un fallo (software)";
    case GPU3D_APAGADO:    return "apagado (software; «gpu3d probar» o «gpu3d encender» lo enciende)";
    case GPU3D_DESATIVADO: return "desactivado por la opcion de arranque (software)";
    default:               return "sin GPU con driver 3D (software)";
    }
}

static const char *const provas[GPU3D_N_PROVAS] = {
    "limpiar", "rectangulo de color", "Z", "mezcla", "textura", "color interpolado", "especular", "Z de 16 bits",
};

/* ------------------------------------------------------------ «gpu3d probar --ver» (D390) */

static info_gpu3d_etapa_t et;           /* ~74 KB: la imagen de una etapa */

#define V_ESC_COR 3                     /* el color, a 3x */
#define V_ESC_Z   2                     /* el Z, a 2x */
#define V_MARGEM  12
#define V_LINHA   16
#define V_TOPO    (V_MARGEM + 2 * V_LINHA + 4)
#define V_COR_L   (GPU3D_VER_L * V_ESC_COR)
#define V_COR_A   (GPU3D_VER_A * V_ESC_COR)
#define V_Z_X     (V_MARGEM + V_COR_L + V_MARGEM)
#define V_LARG    (V_Z_X + GPU3D_VER_L * V_ESC_Z + V_MARGEM)
#define V_ALT     (V_TOPO + V_COR_A + V_LINHA + 6 * V_LINHA + V_MARGEM)

static const char *nome_resultado(int e)
{
    return e == GPU3D_ETAPA_BIEN ? "BIEN" : e == GPU3D_ETAPA_MAL ? "MAL" : e == GPU3D_ETAPA_SIN_IMAGEN ? "sin imagen" : "no se hizo";
}

/* El Z en gris (blanco = al fondo) y la sentinela intacta en magenta. */
static uint32_t cor_z(unsigned short z)
{
    return z == 0xFFFFu ? 0xFF00FFu : (uint32_t)(z & 0xFF) * 0x010101u;
}

static void linha_cores(char *t, int n)
{
    int k = 0;
    k += formatar_texto(t + k, (size_t)(n - k), "colores:");
    for (int i = 0; i < 4 && et.n_cores[i] && k < n - 24; i++)
        k += formatar_texto(t + k, (size_t)(n - k), " %06x x%u", et.cores[i], et.n_cores[i]);
    if (!et.n_cores[0]) formatar_texto(t + k, (size_t)(n - k), " ninguno");
    else if (et.resto && k < n - 16) formatar_texto(t + k, (size_t)(n - k), ", resto %u", et.resto);
}

static void resumo_etapa(int i)
{
    char c[96];
    linha_cores(c, sizeof c);
    imprimir("gpu3d probar --ver: etapa %d (%s): %s; color: %u sin tocar; %s; ", i, provas[i], nome_resultado(et.estado),
             et.cor_intacta, c);
    if (et.com_z) imprimir("Z: %u sin tocar, %u al maximo\n", et.z_intacto, et.z_maximo);
    else imprimir("Z: no se ve en esta tarjeta\n");
}

/* Color y Z, uno al lado del otro, a tamaño real: para el BMP. */
static int guardar_bmp(int i)
{
    imagem_t img;
    if (!imagem_criar(&img, GPU3D_VER_L * 2 + 8, GPU3D_VER_A, 0x404040)) return -1;
    for (int y = 0; y < GPU3D_VER_A; y++)
        for (int x = 0; x < GPU3D_VER_L; x++) {
            img.px[y * img.larg + x] = et.px[y * GPU3D_VER_L + x] & 0x00FFFFFFu;
            img.px[y * img.larg + GPU3D_VER_L + 8 + x] = et.com_z ? cor_z(et.z[y * GPU3D_VER_L + x]) : 0x404040;
        }
    char cam[48];
    formatar_texto(cam, sizeof cam, "/home/gpu3d-etapa%d.bmp", i);
    int r = imagem_gravar_bmp(cam, &img);
    imagem_liberar(&img);
    if (r == 0) imprimir("gpu3d probar --ver:   guardada en %s (color | Z)\n", cam);
    return r;
}

static void desenhar_etapa(jan_t *j, int i, int seg)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    char t[128];
    formatar_texto(t, sizeof t, "Etapa %d (de 0 a %d): %s", i, GPU3D_N_PROVAS - 1, provas[i]);
    g_texto(j, V_MARGEM, V_MARGEM, t, COR_TEXTO);
    uint32_t cr = et.estado == GPU3D_ETAPA_BIEN ? COR_DESTAQUE : 0xC0281E;
    g_texto(j, V_Z_X, V_MARGEM, nome_resultado(et.estado), cr);
    g_texto(j, V_MARGEM, V_MARGEM + V_LINHA, "color (3x)", COR_TEXTO_FRACO);
    g_texto(j, V_Z_X, V_MARGEM + V_LINHA, et.com_z ? "Z (2x): gris, magenta = sin tocar" : "Z: no se ve en esta tarjeta",
            COR_TEXTO_FRACO);
    if (et.estado != GPU3D_ETAPA_NADA && et.estado != GPU3D_ETAPA_SIN_IMAGEN) {
        for (int y = 0; y < V_COR_A; y++) {
            uint32_t *d = j->px + (V_TOPO + y) * j->larg + V_MARGEM;
            const unsigned *s = et.px + (y / V_ESC_COR) * GPU3D_VER_L;
            for (int x = 0; x < V_COR_L; x++) d[x] = s[x / V_ESC_COR] & 0x00FFFFFFu;
        }
        if (et.com_z)
            for (int y = 0; y < GPU3D_VER_A * V_ESC_Z; y++) {
                uint32_t *d = j->px + (V_TOPO + y) * j->larg + V_Z_X;
                const unsigned short *s = et.z + (y / V_ESC_Z) * GPU3D_VER_L;
                for (int x = 0; x < GPU3D_VER_L * V_ESC_Z; x++) d[x] = cor_z(s[x / V_ESC_Z]);
            }
        if (et.x >= 0 && et.y >= 0)   /* el píxel que no cuadra */
            g_moldura(j, V_MARGEM + et.x * V_ESC_COR - 4, V_TOPO + et.y * V_ESC_COR - 4, V_ESC_COR + 8, V_ESC_COR + 8, 0xFFE000);
    } else
        g_texto(j, V_MARGEM + 8, V_TOPO + 8, "sin imagen: la GPU no dejo leer el destino", COR_TEXTO);
    int y = V_TOPO + V_COR_A + V_LINHA / 2;
    formatar_texto(t, sizeof t, "color: %u de %u pixeles sin tocar (a1b2c3: la GPU no escribio)", et.cor_intacta,
                   GPU3D_VER_L * GPU3D_VER_A);
    g_texto(j, V_MARGEM, y, t, COR_TEXTO); y += V_LINHA;
    linha_cores(t, sizeof t);
    g_texto(j, V_MARGEM, y, t, COR_TEXTO); y += V_LINHA;
    if (et.com_z) formatar_texto(t, sizeof t, "Z: %u sin tocar (5a5a5a5a), %u al maximo", et.z_intacto, et.z_maximo);
    else formatar_texto(t, sizeof t, "Z: la CPU no lo ve en esta tarjeta");
    g_texto(j, V_MARGEM, y, t, COR_TEXTO); y += V_LINHA;
    if (et.x >= 0) {
        formatar_texto(t, sizeof t, "pixel %d,%d (recuadro amarillo): %06x, se esperaba %06x", et.x, et.y, et.obtido, et.esperado);
        g_texto(j, V_MARGEM, y, t, 0xC0281E);
    }
    y += V_LINHA;
    formatar_texto(t, sizeof t, "Una tecla: la siguiente.  Esc: terminar.  Sigue sola en %d s", seg);
    g_texto(j, V_MARGEM, y + V_LINHA / 2, t, COR_TEXTO_FRACO);
    jan_mostrar(j);
}

/* Enseña cada etapa en una ventana (o la guarda en BMP sin escritorio). 0 = bien. */
static int ver_etapas(int seg, int guardar)
{
    jan_t *j = NULL;
    int vistas = 0;
    for (int i = 0; i < GPU3D_N_PROVAS; i++) {
        memset(&et, 0, sizeof et);
        et.versao = GPU3D_ETAPA_VERSAO;
        et.etapa = i;
        if (info_sistema(INFO_GPU3D_PROVA, &et, sizeof et) != 0 || et.versao != 1) {
            imprimir_em(ERROS, "gpu3d probar --ver: el kernel no guarda las imagenes de las etapas\n");
            break;
        }
        if (et.estado == GPU3D_ETAPA_NADA) continue;
        resumo_etapa(i);
        int com_imagem = et.estado != GPU3D_ETAPA_SIN_IMAGEN;
        if (com_imagem && guardar) guardar_bmp(i);
        if (!j && !vistas) {
            j = jan_abrir("gpu3d probar --ver", V_LARG, V_ALT);
            if (!j) {
                imprimir("gpu3d probar --ver: sin escritorio: cada etapa se guarda en /home/gpu3d-etapaN.bmp\n");
                guardar = 1;
                if (com_imagem) guardar_bmp(i);
            }
        }
        vistas++;
        if (!j) continue;
        unsigned fim = tempo_ms() + (unsigned)seg * 1000u;
        int falta = -1, sair = 0;
        for (;;) {
            unsigned agora = tempo_ms();
            if ((int)(fim - agora) <= 0) break;
            int f = (int)((fim - agora + 999u) / 1000u);
            if (f != falta) { falta = f; desenhar_etapa(j, i, f); }
            evento_t e;
            if (proximo_evento(j->id, &e, false)) {
                if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 27)) { sair = 1; break; }
                if (e.tipo == EV_TECLA) break;
                continue;
            }
            dormir(40);
        }
        if (sair) { imprimir("gpu3d probar --ver: terminado a mano en la etapa %d\n", i); break; }
    }
    if (j) jan_fechar(j);
    if (!vistas) imprimir("gpu3d probar --ver: ninguna etapa que ensenar (la prueba no llego a la autoprueba)\n");
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-h") != 0 && compara_texto(argv[1], "--ayuda") != 0 && falta_admin("gpu3d"))
        return 1;
    if (argc > 1 && (compara_texto(argv[1], "probar") == 0 || compara_texto(argv[1], "-p") == 0)) {
        info_gpu3d_prova_t r;
        memset(&r, 0, sizeof r);
        int detalle = 0, ver = 0, seg = 5, guardar = 0;
        for (int i = 2; i < argc; i++) {
            if (compara_texto(argv[i], "--detalle") == 0 || compara_texto(argv[i], "-d") == 0) detalle = 1;
            else if (compara_texto(argv[i], "--ver") == 0 || compara_texto(argv[i], "-v") == 0) {
                ver = 1;
                if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') seg = texto_para_int(argv[++i]);
            } else if (compara_texto(argv[i], "--guardar") == 0) guardar = ver = 1;
            else {
                imprimir_em(ERROS, "uso: gpu3d probar [--detalle] [--ver [segundos]] [--guardar]\n");
                return 1;
            }
        }
        if (seg < 1) seg = 1;
        if (seg > 60) seg = 60;
        r.versao = (detalle ? GPU3D_PROVA_DETALLE : 0) | (ver ? GPU3D_PROVA_VER : 0);
        if (info_sistema(INFO_GPU3D_PROVA, &r, sizeof r) != 0) {
            imprimir_em(ERROS, "gpu3d: el kernel no tiene la prueba\n");
            return 1;
        }
        imprimir("gpu3d probar: %s\n", r.texto);
        if (r.resultado >= 0) {
            for (int i = 0; i < GPU3D_N_PROVAS; i++)
                if (r.feitas & (1u << i))
                    imprimir("gpu3d probar:   %d %-20s %s\n", i, provas[i], r.provas & (1u << i) ? "bien" : "MAL");
            if (r.falhou >= 0 && (r.esperado || r.obtido))
                imprimir("gpu3d probar:   pixel %d,%d: %06x, se esperaba %06x\n", r.x, r.y, r.obtido, r.esperado);
            imprimir("gpu3d probar: autoprueba en %u us\n", r.us_autoprova);
        }
        if (r.resultado == 1) {
            imprimir("gpu3d probar: medida: %u triangulos pequenos en %u us (%u/s)\n", r.tri_pequenos, r.us_pequenos, r.tri_s);
            imprimir("gpu3d probar: medida: %u triangulos grandes, %u pixeles con mezcla en %u us (%u kpix/s)\n",
                     r.tri_grandes, r.pix_grandes, r.us_grandes, r.kpix_s);
            imprimir("gpu3d probar: medida: traer %u pixeles a la RAM: %u us\n", r.pix_leitura, r.us_leitura);
            imprimir("gpu3d probar: puntuacion 3D: %u\n", r.pontos);
        }
        if (r.reservado[0] || r.reservado[1])
            imprimir("gpu3d probar: con %u ajustes de registros de /sistema/conf/gpu3d-*.conf (%u lineas malas; ver dmesg)\n",
                     r.reservado[0], r.reservado[1]);
        if (detalle) imprimir("gpu3d probar: detalle en el log del kernel (lineas [GPU3D]; dmesg > /home/gpu3d.txt)\n");
        if (ver && r.resultado >= 0) {
            if (r.reservado[2]) ver_etapas(seg, guardar);
            else imprimir("gpu3d probar --ver: el kernel no guardo las imagenes (sin memoria o sin autoprueba)\n");
        }
        imprimir("gpu3d probar: %s\n", r.resultado == 1 ? "BIEN" : r.resultado == 0 ? "MAL" : "sin 3D por GPU");
        return r.resultado == 1 ? 0 : 1;
    }
    if (argc > 1 && (compara_texto(argv[1], "apagar") == 0 || compara_texto(argv[1], "encender") == 0)) {
        int enc = compara_texto(argv[1], "encender") == 0;
        int anel = 0, gtt = 0, sem_gtt = 0;
        for (int i = 2; enc && i < argc; i++) {
            if (compara_texto(argv[i], "anillo") == 0) anel = 1;
            else if (compara_texto(argv[i], "gtt") == 0) gtt = 1;
            else if (compara_texto(argv[i], "sin-gtt") == 0) sem_gtt = 1;
            else {
                imprimir_em(ERROS, "uso: gpu3d encender [anillo] [gtt | sin-gtt]\n");
                return 1;
            }
        }
        if (gtt && sem_gtt) {
            imprimir_em(ERROS, "gpu3d encender: «gtt» y «sin-gtt» a la vez no tienen sentido\n");
            return 1;
        }
        /* 1 lotes, 2 anillo, 3 lotes con RAM en el GTT, 4 anillo con RAM en el GTT (D358); 5 y 6, sin la GART que
         * las R7xx confirmadas encienden solas (D406). */
        int r = info_sistema(INFO_GPU3D_CTL, (void *)(long)(enc ? (gtt ? 3 : sem_gtt ? 5 : 1) + anel : 0), 0);
        info_gpu3d_t v;
        memset(&v, 0, sizeof v);
        info_sistema(INFO_GPU3D, &v, sizeof v);
        imprimir("gpu3d %s: %s (%s)\n", argv[1], nome_estado(v.estado), v.motivo);
        return enc ? (r == 0 ? 0 : 1) : 0;
    }
    if (argc > 1) {
        imprimir_em(ERROS, "uso: gpu3d [probar [--detalle] [--ver [segundos]] [--guardar] | apagar | encender [anillo] [gtt | sin-gtt]]\n");
        return 1;
    }
    info_gpu3d_t v;
    memset(&v, 0, sizeof v);
    if (info_sistema(INFO_GPU3D, &v, sizeof v) != 0) {
        imprimir_em(ERROS, "gpu3d: el kernel no lo sabe\n");
        return 1;
    }
    imprimir("3D por GPU: %s\n", nome_estado(v.estado));
    imprimir("  driver %s, tarjeta %s\n", v.nome, v.placa);
    imprimir("  %s\n", v.motivo);
    if (v.estado == GPU3D_ATIVO) {
        imprimir("  ordenes: %s; plazo del vigilante: %u ms; memoria de GPU: %u KB usados de %u KB (%u libres)\n",
                 v.variante & GPU3D_VAR_ANEL ? "en el anillo" : "por lotes", v.prazo_ms, v.memoria_kb - v.livre_kb, v.memoria_kb, v.livre_kb);
        if ((v.variante & GPU3D_VAR_GTT) && v.reservado[0])
            imprimir("  RAM del sistema en el GTT: %u KB para el destino%s (D358)\n", v.reservado[0],
                     v.variante & GPU3D_VAR_GTT_TEX ? " y las texturas" : "; texturas en la memoria de video");
        else if (v.variante & GPU3D_VAR_GTT)
            imprimir("  RAM del sistema en el GTT: pedida (este driver no la usa)\n");
        if (v.dono) imprimir("  motor en uso por el proceso %u: destino %ux%u, Z pedido de %u bits\n", v.dono, v.larg, v.alt, v.zbits);
        else imprimir("  motor libre\n");
    }
    if (v.us_autoprova) imprimir("  autoprueba: %u us\n", v.us_autoprova);
    imprimir("  triangulos %u en %u entregas, cuadros %u, fallos %u, peticiones no atendidas %u\n",
             v.triangulos, v.lotes, v.quadros, v.falhas, v.recusas);
    /* D432: presentar sin volver a la RAM. */
    if (v.estado == GPU3D_ATIVO)
        imprimir("  presentar en la pantalla (D432): %s; %u cuadros directos, %u por la RAM; pantalla en exclusiva: %s\n",
                 v.reservado[1] & G3D_CAP_APRESENTAR ? "si" : "no", v.reservado[3], v.reservado[4],
                 v.reservado[5] ? "una ventana la tiene" : "nadie");
    if (v.pontos) imprimir("  ultima medida: %u triangulos/s, %u kpix/s, %u puntos\n", v.tri_s, v.kpix_s, v.pontos);
    return 0;
}
