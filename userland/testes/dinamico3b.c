// SPDX-License-Identifier: GPL-3.0-only
/* dinamico3b - /bin dinámico salvo lo esencial (D462, enlace dinámico fase 3b; tests/h462_dinamico_3b.py).
 *
 *   dinamico3b [espera_ms]
 *
 * 1. La memoria de la sesión tal como está al empezar (el escritorio, el panel, los widgets y los
 *    servicios ya arrancados): «D3B MEM usada=... cache=... compartida=... procesos=... suma=...».
 * 2. Recorre /bin: cada ELF es dinámico (lleva PT_INTERP) o estático: «D3B BIN nombre din|est bytes».
 *    Qué tiene que ser cada uno lo comprueba la prueba del PC contra la lista del Makefile.
 * 3. Lanza cada programa dinámico con «--help» (la entrada, un pipe ya cerrado; la salida y los errores,
 *    a un pipe que se va vaciando) y espera hasta espera_ms: si sale con 127 o escribe «ld-sagui:», el
 *    intérprete no pudo cargarlo (falta una biblioteca o un símbolo); si sigue vivo al acabar el plazo
 *    (un programa de ventana o un servicio que no mira «--help»), ya pasó del intérprete y se termina.
 *    «D3B RUN nombre codigo|vivo ms».
 * 4. Tiempos de lanzamiento: 200 veces «uname -s» y 200 «basename /a/b» (dinámicos en la imagen de serie,
 *    estáticos con DINAMICO=0): «D3B TEMPO programa veces ms».
 * Acaba con «RESULTADO dinamico3b: ...». */
#include "sagui.h"
#include "verifica.h"

#define PT_INTERP 3
#define MAX_PROG  320

/* Programas que no se lanzan: «falha» provoca un error a propósito (la prueba busca excepciones). */
static const char *const no_lanzar[] = { "falha", NULL };

typedef struct { char nome[NOME_MAX]; bool dinamico; unsigned tam; } prog_t;
static prog_t progs[MAX_PROG];
static int nprogs;

static bool lee_elf(const char *cam, bool *elf, bool *din)
{
    static uint8_t b[4096];
    *elf = *din = false;
    int fd = abrir(cam, ABRIR_LER);
    if (fd < 0) return false;
    int n = ler(fd, b, sizeof b);
    fechar(fd);
    if (n < 52 || b[0] != 0x7F || b[1] != 'E' || b[2] != 'L' || b[3] != 'F' || b[4] != 1) return true;
    *elf = true;
    uint32_t phoff = b[28] | b[29] << 8 | b[30] << 16 | (uint32_t)b[31] << 24;
    unsigned phent = b[42] | b[43] << 8, phnum = b[44] | b[45] << 8;
    for (unsigned i = 0; i < phnum; i++) {
        uint32_t o = phoff + i * phent;
        if (o + 4 > (uint32_t)n) break;
        uint32_t tipo = b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24;
        if (tipo == PT_INTERP) *din = true;
    }
    return true;
}

static bool contem(const char *s, const char *p)
{
    size_t n = tam_texto(p);
    for (; *s; s++)
        if (compara_texto_n(s, p, n) == 0) return true;
    return false;
}

static void memoria_sessao(void)
{
    info_memoria_t m;
    memset(&m, 0, sizeof m);
    if (info_sistema(INFO_MEMORIA, &m, sizeof m) < 0) { verifica(false, "INFO_MEMORIA"); return; }
    static info_processo_t v[96];
    int n = info_sistema(INFO_PROCESSOS, v, 96);
    unsigned soma = 0, usu = 0;
    for (int i = 0; i < n; i++)
        if (v[i].usuario) { soma += v[i].memoria_kb; usu++; }
    imprimir("D3B MEM usada=%u cache=%u compartida=%u compartida_mapeada=%u procesos=%u suma=%u\n",
             m.usada_kb, m.cache_kb, m.compartida_kb, m.compartida_mapeada_kb, usu, soma);
}

/* Lanza caminho con argv; la entrada es un pipe cerrado y la salida otro pipe que se vacía.
 * Devuelve el código de salida, o -1000 si seguía vivo a los espera_ms (y lo termina), o < -1000 si no
 * se pudo lanzar. *ld = true si la salida trae «ld-sagui:». *ms = lo que tardó. */
static int lancar(const char *caminho, char *const argv[], unsigned espera_ms, bool *ld, unsigned *ms)
{
    int pin[2], pout[2];
    *ld = false;
    if (criar_pipe(pin) < 0) return -2000;
    if (criar_pipe(pout) < 0) { fechar(pin[0]); fechar(pin[1]); return -2001; }
    fechar(pin[1]);                       /* la entrada del hijo: fin de fichero enseguida */
    duplicar(ENTRADA, 20); duplicar(SAIDA, 21); duplicar(ERROS, 22);
    duplicar(pin[0], ENTRADA); duplicar(pout[1], SAIDA); duplicar(pout[1], ERROS);
    unsigned t0 = tempo_ms();
    int pid = executar(caminho, argv);
    duplicar(20, ENTRADA); duplicar(21, SAIDA); duplicar(22, ERROS);
    fechar(20); fechar(21); fechar(22);
    fechar(pin[0]); fechar(pout[1]);
    if (pid < 0) { fechar(pout[0]); return -3000 + pid; }
    if (espera_ms == 0) {   /* para medir: esperar sin sondear (la salida es corta y cabe en el pipe) */
        int c = -1;
        esperar(pid, &c);
        *ms = tempo_ms() - t0;
        fechar(pout[0]);
        return c;
    }
    static char cola[512];
    unsigned ncola = 0;
    int codigo = -1000;
    for (;;) {
        int d = disponivel(pout[0]);
        while (d > 0) {
            char b[256];
            int r = ler(pout[0], b, d < (int)sizeof b ? (unsigned)d : sizeof b);
            if (r <= 0) break;
            for (int i = 0; i < r; i++) {   /* busca «ld-sagui:» en lo que va saliendo */
                if (ncola == sizeof cola - 1) { memmove(cola, cola + 256, ncola - 256); ncola -= 256; }
                cola[ncola++] = b[i];
            }
            cola[ncola] = 0;
            if (contem(cola, "ld-sagui:")) *ld = true;
            d -= r;
        }
        int c = 0, estado = 0;
        int r = esperar_pid(pid, &estado, ESPERA_SEM_BLOQUEAR);
        if (r == pid) {   /* como en Linux: (código << 8) si salió; el número de la señal si no */
            codigo = (estado & 0x7F) == 0 ? (estado >> 8) & 0xFF : -100 - (estado & 0x7F);
            break;
        }
        if (tempo_ms() - t0 >= espera_ms) {
            matar(pid);
            esperar(pid, &c);
            *ms = tempo_ms() - t0;
            fechar(pout[0]);
            return -1000;
        }
        dormir(5);
    }
    *ms = tempo_ms() - t0;
    fechar(pout[0]);
    return codigo;
}

int principal(int argc, char **argv)
{
    unsigned espera = argc > 1 ? (unsigned)texto_para_int(argv[1]) : 2500;
    if (espera < 200) espera = 2500;
    mudar_pasta("/tmp");   /* touch, tee, split... con «--help» podrían crear ficheros: aquí, no en /bin */
    memoria_sessao();

    entrada_pasta_t e;
    for (int i = 0; nprogs < MAX_PROG && listar("/bin", i, &e) == 1; i++) {
        if (e.tipo == TIPO_PASTA) continue;
        char cam[NOME_MAX + 8];
        formatar_texto(cam, sizeof cam, "/bin/%s", e.nome);
        bool elf, din;
        if (!lee_elf(cam, &elf, &din) || !elf) continue;
        copia_texto(progs[nprogs].nome, e.nome, sizeof progs[nprogs].nome);
        progs[nprogs].dinamico = din;
        progs[nprogs].tam = e.tamanho;
        imprimir("D3B BIN %s %s %u\n", e.nome, din ? "din" : "est", e.tamanho);
        nprogs++;
    }
    verifica(nprogs > 30, "/bin tiene programas ELF");

    int lanzados = 0, malos = 0;
    for (int i = 0; i < nprogs; i++) {
        if (!progs[i].dinamico) continue;
        bool saltar = false;
        for (int k = 0; no_lanzar[k]; k++) if (compara_texto(progs[i].nome, no_lanzar[k]) == 0) saltar = true;
        if (saltar) continue;
        char cam[NOME_MAX + 8];
        formatar_texto(cam, sizeof cam, "/bin/%s", progs[i].nome);
        char *av[] = { cam, "--help", NULL };
        bool ld;
        unsigned ms = 0;
        int c = lancar(cam, av, espera, &ld, &ms);
        lanzados++;
        bool mal = c == 127 || ld || c < -1000 || (c <= -100 && c > -200);   /* 127, el intérprete; -1xx, una señal */
        if (mal) malos++;
        if (c == -1000) imprimir("D3B RUN %s vivo %u\n", progs[i].nome, ms);
        else imprimir("D3B RUN %s %d %u%s\n", progs[i].nome, c, ms, mal ? " MAL" : "");
    }
    imprimir("D3B LANZADOS %d MALOS %d\n", lanzados, malos);
    verifica(malos == 0, "todos los programas dinamicos de /bin pasan del interprete (ni 127 ni su aviso)");

    /* Tiempos de lanzamiento de dos programas pequeños de línea de órdenes. */
    static const char *const tempo_progs[][3] = { { "/bin/uname", "-s", NULL }, { "/bin/basename", "/a/b", NULL } };
    for (int k = 0; k < 2; k++) {
        unsigned total = 0, t_ini = tempo_ms();   /* el reloj va por tics: se mide la vuelta entera */
        int bien = 0;
        for (int v = 0; v < 200; v++) {
            char *av[] = { (char *)tempo_progs[k][0], (char *)tempo_progs[k][1], NULL };
            bool ld;
            unsigned ms = 0;
            if (lancar(tempo_progs[k][0], av, 0, &ld, &ms) == 0) bien++;
        }
        total = tempo_ms() - t_ini;
        imprimir("D3B TEMPO %s 200 %u\n", tempo_progs[k][0], total);
        verifica(bien == 200, "lanzado 200 veces sin fallos");
    }
    memoria_sessao();

    /* 5. La memoria con ocho programas de ventana abiertos a la vez (como una sesión de trabajo). */
    static const char *const abiertos[] = { "/bin/calculadora", "/bin/arquivos", "/bin/editor", "/bin/tareas",
                                            "/bin/visor", "/bin/widget-relogio", "/bin/widget-sistema",
                                            "/bin/configuracion" };
    int pids[8], vivos = 0;
    for (int k = 0; k < 8; k++) {
        char *av[] = { (char *)abiertos[k], NULL };
        pids[k] = executar(abiertos[k], av);
        if (pids[k] >= 0) vivos++;
    }
    dormir(5000);
    imprimir("D3B ABIERTOS %d\n", vivos);
    memoria_sessao();
    for (int k = 0; k < 8; k++)
        if (pids[k] >= 0) { int c; matar(pids[k]); esperar(pids[k], &c); }
    verifica(vivos == 8, "ocho programas de ventana abiertos a la vez");
    return resultado("dinamico3b");
}
