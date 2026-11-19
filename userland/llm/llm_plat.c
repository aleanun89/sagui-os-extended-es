// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_plat.c - lo que depende de la plataforma (D380): ficheros, memoria, tiempo, salida
 * e hilos. Con LLM_ANFITRIAO compila para el PC con la libc (para comparar con llama.cpp);
 * si no, para SaguiOS con la libsagui. */
#include "llm.h"

#ifdef LLM_ANFITRIAO
/* ================================================================== PC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/sysinfo.h>

void *llm_aloca(size_t n) { return malloc(n ? n : 1); }
void *llm_aloca_zero(size_t n) { return calloc(1, n ? n : 1); }
void  llm_libera(void *p) { free(p); }
void *llm_mapa(size_t n)
{
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
void llm_mapa_libera(void *p, size_t n) { if (p) munmap(p, n); }
int  llm_abrir(const char *c) { return open(c, O_RDONLY); }
long long llm_tamanho_fd(int fd) { struct stat st; return fstat(fd, &st) ? -1 : (long long)st.st_size; }
int  llm_ler_em(int fd, unsigned long long desloc, void *buf, size_t n)
{
    uint8_t *b = buf;
    while (n) {
        ssize_t r = pread(fd, b, n > (1u << 30) ? (1u << 30) : n, (off_t)desloc);
        if (r <= 0) return -1;
        b += r; n -= (size_t)r; desloc += (unsigned long long)r;
    }
    return 0;
}
void llm_fechar(int fd) { close(fd); }
unsigned llm_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
void llm_escrever(int fd, const char *s, size_t n) { if (write(fd, s, n) < 0) { /* nada */ } }
void llm_printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); fflush(stdout); }
void llm_eprintf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
int  llm_formatar(char *d, size_t max, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = vsnprintf(d, max, fmt, ap); va_end(ap); return r;
}
size_t llm_strlen(const char *s) { return strlen(s); }
int  llm_strcmp(const char *a, const char *b) { return strcmp(a, b); }
int  llm_strncmp(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
void *llm_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void *llm_memset(void *d, int v, size_t n) { return memset(d, v, n); }
int  llm_memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
bool llm_cpu_sse2(void) { return true; }
bool llm_cpu_ssse3(void) { return __builtin_cpu_supports("ssse3"); }
bool llm_cpu_avx2(void) { return __builtin_cpu_supports("avx2"); }   /* la libgcc mira también XCR0 */
int  llm_cpus(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n < 1 ? 1 : (int)n; }
unsigned long long llm_ram_fisica(void) { struct sysinfo si; return sysinfo(&si) ? 0 : (unsigned long long)si.totalram * si.mem_unit; }
unsigned long long llm_memoria_livre(void) { return 0; }   /* en el PC no se comprueba */
unsigned long long llm_espaco_processo(void) { return 1ull << 40; }
uint32_t llm_aleatorio(void) { return (uint32_t)time(NULL) ^ (uint32_t)getpid() * 2654435761u; }

/* Hilos con pthread: trabajadores fijos que esperan una tarea. */
static int n_hilos = 1;
static pthread_t hilos[64];
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_tarefa = PTHREAD_COND_INITIALIZER, cv_fim = PTHREAD_COND_INITIALIZER;
static llm_tarefa_t tarefa; static void *tarefa_ctx; static int tarefa_n, geracao, pendentes, parar;

static void *trabalhador(void *arg)
{
    int id = (int)(intptr_t)arg, visto = 0;
    for (;;) {
        pthread_mutex_lock(&mx);
        while (geracao == visto && !parar) pthread_cond_wait(&cv_tarefa, &mx);
        if (parar) { pthread_mutex_unlock(&mx); return NULL; }
        visto = geracao;
        llm_tarefa_t f = tarefa; void *c = tarefa_ctx; int n = tarefa_n;
        pthread_mutex_unlock(&mx);
        int ini = (int)((long long)n * id / n_hilos), fim = (int)((long long)n * (id + 1) / n_hilos);
        if (fim > ini) f(c, ini, fim);
        pthread_mutex_lock(&mx);
        if (--pendentes == 0) pthread_cond_signal(&cv_fim);
        pthread_mutex_unlock(&mx);
    }
}
int llm_hilos_iniciar(int n)
{
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    n_hilos = n;
    for (int i = 1; i < n; i++) pthread_create(&hilos[i], NULL, trabalhador, (void *)(intptr_t)i);
    return n_hilos;
}
void llm_hilos_parar(void)
{
    pthread_mutex_lock(&mx); parar = 1; pthread_cond_broadcast(&cv_tarefa); pthread_mutex_unlock(&mx);
    for (int i = 1; i < n_hilos; i++) pthread_join(hilos[i], NULL);
    n_hilos = 1;
}
void llm_hilos_repartir(llm_tarefa_t f, void *ctx, int n)
{
    if (n_hilos <= 1 || n < n_hilos) { f(ctx, 0, n); return; }
    pthread_mutex_lock(&mx);
    tarefa = f; tarefa_ctx = ctx; tarefa_n = n; pendentes = n_hilos - 1; geracao++;
    pthread_cond_broadcast(&cv_tarefa);
    pthread_mutex_unlock(&mx);
    f(ctx, 0, (int)((long long)n / n_hilos));
    pthread_mutex_lock(&mx);
    while (pendentes) pthread_cond_wait(&cv_fim, &mx);
    pthread_mutex_unlock(&mx);
}

#else
/* ================================================================== SaguiOS */
#include "sagui.h"

void *llm_aloca(size_t n) { return aloca(n ? n : 1); }
void *llm_aloca_zero(size_t n) { return aloca_zerado(n ? n : 1); }
void  llm_libera(void *p) { if (p) libera(p); }
/* Los bloques grandes, con mapas de páginas (D209): vuelven al sistema al liberarlos. */
void *llm_mapa(size_t n) { return mapear_memoria(n); }
void  llm_mapa_libera(void *p, size_t n) { if (p) desmapear_memoria(p, n); }
int   llm_abrir(const char *c) { return abrir(c, ABRIR_LER); }
long long llm_tamanho_fd(int fd)
{
    int fim = posicionar(fd, 0, POS_FIM);
    posicionar(fd, 0, POS_INICIO);
    return fim < 0 ? -1 : (long long)(unsigned)fim;
}
int llm_ler_em(int fd, unsigned long long desloc, void *buf, size_t n)
{
    if (desloc > 0x7FFFFFFFull) return -1;
    if (posicionar(fd, (int)desloc, POS_INICIO) < 0) return -1;
    uint8_t *b = buf;
    while (n) {
        unsigned pedaco = n > (1u << 20) ? (1u << 20) : (unsigned)n;
        int r = ler(fd, b, pedaco);
        if (r <= 0) return -1;
        b += r; n -= (size_t)r;
    }
    return 0;
}
void llm_fechar(int fd) { fechar(fd); }
unsigned llm_ms(void) { return tempo_ms(); }
void llm_escrever(int fd, const char *s, size_t n) { escrever_tudo(fd, s, (unsigned)n); }
void llm_printf(const char *fmt, ...)
{
    char b[1024]; va_list ap; va_start(ap, fmt); int n = formatar_lista(b, sizeof b, fmt, ap); va_end(ap);
    if (n > (int)sizeof b - 1) n = sizeof b - 1;
    if (n > 0) escrever_tudo(SAIDA, b, (unsigned)n);
}
void llm_eprintf(const char *fmt, ...)
{
    char b[1024]; va_list ap; va_start(ap, fmt); int n = formatar_lista(b, sizeof b, fmt, ap); va_end(ap);
    if (n > (int)sizeof b - 1) n = sizeof b - 1;
    if (n > 0) escrever_tudo(ERROS, b, (unsigned)n);
}
int llm_formatar(char *d, size_t max, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = formatar_lista(d, max, fmt, ap); va_end(ap); return r;
}
size_t llm_strlen(const char *s) { return tam_texto(s); }
int  llm_strcmp(const char *a, const char *b) { return compara_texto(a, b); }
int  llm_strncmp(const char *a, const char *b, size_t n) { return compara_texto_n(a, b, n); }
void *llm_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void *llm_memset(void *d, int v, size_t n) { return memset(d, v, n); }
int  llm_memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
bool llm_cpu_sse2(void) { return cpu_tem_sse2(); }
bool llm_cpu_ssse3(void) { return cpu_tem_ssse3(); }
bool llm_cpu_avx2(void) { return cpu_tem_avx2(); }   /* D381: con el estado YMM guardado por el kernel */
/* Las CPU que trabajan (D157): sin la imagen SMP (D164) los secundarios no arrancan y no tienen tiques. */
int llm_cpus(void)
{
    info_nucleo_t v[16];
    int r = info_sistema(INFO_NUCLEOS, v, sizeof v), n = 0;
    for (int i = 0; i < r && i < 16; i++) if (v[i].tiques) n++;
    return n < 1 ? 1 : n;
}

static bool info_mem(info_memoria_t *m)
{
    llm_memset(m, 0, sizeof *m);
    return info_sistema(INFO_MEMORIA, m, sizeof *m) >= 0;
}
unsigned long long llm_ram_fisica(void)
{
    info_memoria_t m;
    return info_mem(&m) ? (unsigned long long)m.ram_total_kb * 1024ull : 0;
}
/* Lo libre más la caché de disco, que se devuelve a los programas cuando la piden (D350). */
unsigned long long llm_memoria_livre(void)
{
    info_memoria_t m;
    if (!info_mem(&m)) return 0;
    return ((unsigned long long)m.livre_kb + m.cache_kb) * 1024ull;
}
/* El proceso tiene 0x80000000-0xDBFFFFFF para código y datos (kernel/inc/paginacao.h).
 * El malloc reserva por tramos (no hay un «break» continuo): se resta lo que ya tiene
 * y un margen para el código y la pila. */
unsigned long long llm_espaco_processo(void)
{
    const unsigned long long total = 0xDC000000ull - 0x80000000ull, margem = 24ull << 20;
    unsigned long long usado = memoria_total() + margem;
    return usado < total ? total - usado : 0;
}
uint32_t llm_aleatorio(void)
{
    uint32_t v = 0;
    if (bytes_aleatorios(&v, sizeof v) != (int)sizeof v) v = tempo_ms() * 2654435761u;
    return v;
}

/* Hilos (D167): trabajadores fijos que duermen en un semáforo y avisan con otro. Con un
 * solo núcleo no se gana nada (el kernel corre en la CPU 0, D164): por eso son opcionales. */
static int n_hilos = 1;
static thread_t *hilos[16];
static semaforo_t sem_tarefa[16], sem_fim;
static llm_tarefa_t tarefa; static void *tarefa_ctx; static volatile int tarefa_n, parar;

static void *trabalhador(void *arg)
{
    int id = (int)(unsigned long)arg;
    for (;;) {
        semaforo_tomar(&sem_tarefa[id]);
        if (parar) return NULL;
        int n = tarefa_n;
        int ini = (int)((long long)n * id / n_hilos), fim = (int)((long long)n * (id + 1) / n_hilos);
        if (fim > ini) tarefa(tarefa_ctx, ini, fim);
        semaforo_sinalizar(&sem_fim);
    }
}
int llm_hilos_iniciar(int n)
{
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    semaforo_criar(&sem_fim, 0);
    n_hilos = 1;
    for (int i = 1; i < n; i++) {
        semaforo_criar(&sem_tarefa[i], 0);
        hilos[i] = thread_create(trabalhador, (void *)(unsigned long)i, 0);
        if (!hilos[i]) break;
        n_hilos = i + 1;
    }
    return n_hilos;
}
void llm_hilos_parar(void)
{
    parar = 1;
    for (int i = 1; i < n_hilos; i++) semaforo_sinalizar(&sem_tarefa[i]);
    for (int i = 1; i < n_hilos; i++) thread_join(hilos[i]);
    n_hilos = 1; parar = 0;
}
void llm_hilos_repartir(llm_tarefa_t f, void *ctx, int n)
{
    if (n_hilos <= 1 || n < n_hilos) { f(ctx, 0, n); return; }
    tarefa = f; tarefa_ctx = ctx; tarefa_n = n;
    for (int i = 1; i < n_hilos; i++) semaforo_sinalizar(&sem_tarefa[i]);
    f(ctx, 0, (int)((long long)n / n_hilos));
    for (int i = 1; i < n_hilos; i++) semaforo_tomar(&sem_fim);
}
#endif
