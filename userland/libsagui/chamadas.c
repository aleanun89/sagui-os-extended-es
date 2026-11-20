// SPDX-License-Identifier: GPL-3.0-only
/* chamadas.c - una función por llamada de sistema (convención en abi.h). */
#include "sagui.h"

static inline int chamar(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

/* El programa nuevo hereda el entorno actual (environ), como en Unix (D037). */
int  executar(const char *c, char *const argv[]) { return chamar(SIS_EXECUTAR, (unsigned)c, (unsigned)argv, (unsigned)environ); }
int  executar_env(const char *c, char *const argv[], char *const envp[]) { return chamar(SIS_EXECUTAR, (unsigned)c, (unsigned)argv, (unsigned)envp); }
int  esperar(int pid, int *codigo)    { return chamar(SIS_ESPERAR, (unsigned)pid, (unsigned)codigo, 0); }
int  recolher(int *codigo)            { return chamar(SIS_ESPERAR, (unsigned)-1, (unsigned)codigo, 1); }
void sair(int codigo)                 { chamar(SIS_SAIR, (unsigned)codigo, 0, 0); for (;;) ; }
int  meu_pid(void)                    { return chamar(SIS_PID, 0, 0, 0); }
int  matar(int pid)                   { return chamar(SIS_MATAR, (unsigned)pid, 0, 0); }
int  abrir(const char *c, int modo)   { return chamar(SIS_ABRIR, (unsigned)c, (unsigned)modo, 0); }
int  ler(int fd, void *b, unsigned n) { return chamar(SIS_LER, (unsigned)fd, (unsigned)b, n); }
int  escrever(int fd, const void *b, unsigned n) { return chamar(SIS_ESCREVER, (unsigned)fd, (unsigned)b, n); }
int  fechar(int fd)                   { return chamar(SIS_FECHAR, (unsigned)fd, 0, 0); }
int  posicionar(int fd, int d, int o) { return chamar(SIS_POSICIONAR, (unsigned)fd, (unsigned)d, (unsigned)o); }
int  info_arquivo(const char *c, info_arquivo_t *i) { return chamar(SIS_INFO_ARQUIVO, (unsigned)c, (unsigned)i, 0); }
int  criar_pasta(const char *c)       { return chamar(SIS_CRIAR_PASTA, (unsigned)c, 0, 0); }
int  remover_pasta(const char *c)     { return chamar(SIS_REMOVER_PASTA, (unsigned)c, 0, 0); }
int  apagar(const char *c)            { return chamar(SIS_APAGAR, (unsigned)c, 0, 0); }
int  listar(const char *c, int i, entrada_pasta_t *e) { return chamar(SIS_LISTAR, (unsigned)c, (unsigned)i, (unsigned)e); }
int  mudar_pasta(const char *c)       { return chamar(SIS_MUDAR_PASTA, (unsigned)c, 0, 0); }
int  pasta_atual(char *b, unsigned n) { return chamar(SIS_PASTA_ATUAL, (unsigned)b, n, 0); }
int  truncar(int fd, unsigned tamanho) { return chamar(SIS_TRUNCAR, (unsigned)fd, tamanho, 0); }
int  sincronizar(int fd) { return chamar(SIS_SINCRONIZAR, (unsigned)fd, 0, 0); }
int  renomear(const char *o, const char *d) { return chamar(SIS_RENOMEAR, (unsigned)o, (unsigned)d, 0); }
int  criar_pipe(int fds[2])           { return chamar(SIS_PIPE, (unsigned)fds, 0, 0); }
int  abrir_tv(int fds[2])             { return chamar(SIS_ABRIR_TV, (unsigned)fds, 0, 0); }
int  primeiro_plano(int grupo)        { return chamar(SIS_PRIMEIRO_PLANO, (unsigned)grupo, 0, 0); }
int  definir_grupo(int pid, int grupo) { return chamar(SIS_GRUPO, (unsigned)pid, (unsigned)grupo, 0); }
int  interrupcoes(void)               { return chamar(SIS_INTERRUPCOES, 0, 0, 0); }
int  tipo_descritor(int fd)           { return chamar(SIS_TIPO_DESCRITOR, (unsigned)fd, 0, 0); }
int  duplicar(int a, int n)           { return chamar(SIS_DUPLICAR, (unsigned)a, (unsigned)n, 0); }
int  disponivel(int fd)               { return chamar(SIS_DISPONIVEL, (unsigned)fd, 0, 0); }
int  esperar_rede(unsigned visto, unsigned ms) { return chamar(SIS_ESPERAR_REDE, visto, ms, 0); }
int  dormir(unsigned ms)              { return chamar(SIS_DORMIR, ms, 0, 0); }
int  bytes_aleatorios(void *b, unsigned n) { return chamar(SIS_ALEATORIO, (unsigned)b, n, 0); }
int  info_aleatorio(info_aleatorio_t *i) { return chamar(SIS_INFO_ALEATORIO, (unsigned)i, 0, 0); }
int  copiar_texto(const char *t, unsigned n) { return chamar(SIS_COPIAR_TEXTO, (unsigned)t, n, 0); }
/* Copia hasta max - 1 bytes y termina en cero; devuelve el tamaño guardado en el
 * kernel (si es >= max, el texto llegó cortado). buf NULL / max 0: solo el tamaño. */
int  colar_texto(char *buf, unsigned max)
{
    int r = chamar(SIS_COLAR_TEXTO, (unsigned)buf, max ? max - 1 : 0, 0);
    if (r >= 0 && buf && max) buf[(unsigned)r < max - 1 ? (unsigned)r : max - 1] = 0;
    return r;
}
unsigned tempo_ms(void)               { return (unsigned)chamar(SIS_TEMPO, 0, 0, 0); }
int  ler_data(data_t *d)              { return chamar(SIS_DATA, (unsigned)d, 0, 0); }
int  info_sistema(int t, void *b, unsigned n) { return chamar(SIS_INFO_SISTEMA, (unsigned)t, (unsigned)b, n); }
void *aumentar_memoria(unsigned n)    { int r = chamar(SIS_AUMENTAR_MEM, n, 0, 0); return (r < 0 && r > -100) ? NULL : (void *)r; }
/* D209: páginas del área de datos (MEM_* en abi.h); la usa el malloc. */
int  memoria_paginas(int op, void *e, unsigned n) { return chamar(SIS_MEMORIA, (unsigned)op, (unsigned)e, n); }
/* D218: el bloqueo de publicidad por DNS (BLOQ_* en abi.h). */
int  bloqueo_dns(int op, const void *a, unsigned b) { return chamar(SIS_BLOQUEO, (unsigned)op, (unsigned)a, b); }
/* D216: con servicios en marcha, el kernel reinicia desde su tarea "apagar"
 * después de pararlos: la llamada vuelve y aquí solo se espera. */
/* D255: si el kernel lo niega (un servicio, una sesión SSH sin ser root), vuelve con el error. */
int  reiniciar(void)                  { int r = chamar(SIS_REINICIAR, 0, 0, 0); if (r == ERRO_ACESSO) return r; for (;;) dormir(1000); }
/* D216: el gestor de servicios (SERV_* en abi.h). */
int  servicos_kernel(int op, int arg) { return chamar(SIS_SERVICOS, (unsigned)op, (unsigned)arg, 0); }
int  servicos_kernel3(int op, int a, int b) { return chamar(SIS_SERVICOS, (unsigned)op, (unsigned)a, (unsigned)b); }
/* D255: usuarios (USU_* en abi.h). */
int  usuario_op(int op, int a, int b) { return chamar(SIS_USUARIO, (unsigned)op, (unsigned)a, (unsigned)b); }
/* El kernel apaga en una tarea propia: aquí solo se espera (D113). Sin ACPI la
 * máquina queda parada con el aviso «Ya puedes apagar el ordenador». */
int  desligar(void)                   { int r = chamar(SIS_DESLIGAR, 0, 0, 0); if (r == ERRO_ACESSO) return r; for (;;) dormir(1000); }
int  acpi_controle(int op)            { return chamar(SIS_ACPI, (unsigned)op, 0, 0); }
int  modos_video(modo_video_t *v, int n) { return chamar(SIS_MODOS_VIDEO, (unsigned)v, (unsigned)n, 0); }
int  trocar_modo_video(int l, int a, int op) { return chamar(SIS_MUDAR_MODO, (unsigned)l | (unsigned)a << 16, (unsigned)op, 0); }
/* D290: varias pantallas (TELAS_OP_* en abi.h). */
int  telas_sis(int op, unsigned a, unsigned b) { return chamar(SIS_TELAS, (unsigned)op, a, b); }
int  modo_terminal(int m)             { return chamar(SIS_MODO_TERMINAL, (unsigned)m, 0, 0); }
int  limpar_tela(void)                { return chamar(SIS_LIMPAR_TELA, 0, 0, 0); }
int  registrar_texto(const char *t)   { return chamar(SIS_REGISTRAR, (unsigned)t, 0, 0); }
int  listar_discos(int i, info_disco_t *d) { return chamar(SIS_LISTAR_DISCOS, (unsigned)i, (unsigned)d, 0); }
int  montar(const char *nome)         { return chamar(SIS_MONTAR, (unsigned)nome, 0, 0); }
int  montar_em(const char *nome, const char *ponto, int opcoes)
                                      { return chamar(SIS_MONTAR, (unsigned)nome, (unsigned)ponto, (unsigned)opcoes); }
int  desmontar(const char *nome)      { return chamar(SIS_DESMONTAR, (unsigned)nome, 0, 0); }
/* D233: desmonta el CD/DVD y abre la bandeja (fechar = 1: la cierra). */
int  expulsar_disco(const char *nome, int fechar)
                                      { return chamar(SIS_DISCO_BLOCOS, DBLOCO_EXPULSAR, (unsigned)nome, (unsigned)fechar); }
int  tocar(const nota_t *notas, int n, int modo) { return chamar(SIS_BIP, (unsigned)notas, (unsigned)n, (unsigned)modo); }
int  bip(int freq, int ms)
{
    nota_t n = { (uint16_t)freq, (uint16_t)ms };
    return tocar(&n, 1, BIP_SUBSTITUIR);
}
int  teclas_seguradas(unsigned char mapa[32]) { return chamar(SIS_TECLAS_SEGURADAS, (unsigned)mapa, 0, 0); }
int  teclado_layout(int layout)        { return chamar(SIS_TECLADO, (unsigned)layout, 0, 0); }
int  acento_cor(int rgb)               { return chamar(SIS_ACENTO, (unsigned)rgb, 0, 0); }
void silenciar(void)                  { chamar(SIS_BIP, 0, 0, 0); }

/* ---- audio (D088): la cuarta llamada usa ESI ---- */
static inline int chamar4(unsigned n, unsigned a, unsigned b, unsigned c, unsigned d)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return r;
}
int  memoria_proteger(void *e, unsigned n, int prot)   /* D349 */
                                      { return chamar4(SIS_MEMORIA, MEM_PROTEGER, (unsigned)e, n, (unsigned)prot); }
int  fundo_enviar(int op, const uint32_t *px, int y, int n)
                                      { return chamar4(SIS_FUNDO, (unsigned)op, (unsigned)px, (unsigned)y, (unsigned)n); }
int  audio_abrir(int taxa, int canais, int bits)
{
    return chamar(SIS_AUDIO_ABRIR, (unsigned)taxa, (unsigned)(canais | bits << 8), 0);
}
/* Escribe todo, durmiendo cuando el flujo se llena; devuelve n o el error. */
int  audio_escrever(int fd, const void *buf, unsigned n)
{
    const unsigned char *p = buf;
    unsigned feito = 0;
    while (feito < n) {
        int r = chamar4(SIS_AUDIO_ESCREVER, (unsigned)fd, (unsigned)(p + feito), n - feito, 0);
        if (r < 0) return r;
        if (r == 0) break;                /* menos de un cuadro: descartado */
        feito += (unsigned)r;
    }
    return (int)n;
}
int  audio_escrever_sem_bloquear(int fd, const void *buf, unsigned n)
{
    return chamar4(SIS_AUDIO_ESCREVER, (unsigned)fd, (unsigned)buf, n, 1);
}
int  audio_controle(int fd, int op, int valor) { return chamar(SIS_AUDIO_CONTROLE, (unsigned)fd, (unsigned)op, (unsigned)valor); }
int  audio_volume(int fd, int v)      { return audio_controle(fd, AUDIO_VOLUME, v); }
int  audio_volume_mestre(int v)       { return audio_controle(-1, AUDIO_VOLUME, v); }
int  audio_silencio(int v)            { return audio_controle(-1, AUDIO_SILENCIO, v); }
int  audio_salida(int i)              { return audio_controle(-1, AUDIO_SALIDA, i); }
int  audio_pendente(int fd)           { return audio_controle(fd, AUDIO_PENDENTE, 0); }
int  audio_livre(int fd)              { return audio_controle(fd, AUDIO_LIVRE, 0); }
int  audio_fechar(int fd, int esperar) { return chamar(SIS_AUDIO_FECHAR, (unsigned)fd, (unsigned)esperar, 0); }
int  audio_hda(int op, unsigned a, unsigned b) { return chamar(SIS_AUDIO_HDA, (unsigned)op, a, b); }   /* D107 */
int  criar_janela(const char *t, int l, int a, uint32_t **px)
                                      { return chamar(SIS_CRIAR_JANELA, (unsigned)t, (unsigned)(l | (a << 16)), (unsigned)px); }
int  fechar_janela(int id)            { return chamar(SIS_FECHAR_JANELA, (unsigned)id, 0, 0); }
int  atualizar_janela(int id, const retangulo_t *r) { return chamar(SIS_ATUALIZAR_JANELA, (unsigned)id, (unsigned)r, 0); }
int  proximo_evento(int id, evento_t *e, bool esperar) { return chamar(SIS_PROXIMO_EVENTO, (unsigned)id, (unsigned)e, esperar); }
/* Espera un evento durante hasta ms milisegundos (D083); ms < 0 = sin plazo. */
int  esperar_evento(int id, evento_t *e, int ms)
{
    if (ms < 0) return proximo_evento(id, e, true);
    if (ms == 0) return proximo_evento(id, e, false);
    unsigned prazo = (tempo_ms() + (unsigned)ms) & ~EVENTO_PRAZO;
    return chamar(SIS_PROXIMO_EVENTO, (unsigned)id, (unsigned)e, EVENTO_PRAZO | prazo);
}
int  titulo_janela(int id, const char *t) { return chamar(SIS_TITULO_JANELA, (unsigned)id, (unsigned)t, 0); }
int  estilo_janela(int id, unsigned estilo, int min_l, int min_a)
                                      { return chamar(SIS_ESTILO_JANELA, (unsigned)id, estilo, (unsigned)(min_l | (min_a << 16))); }
/* D230: oculta (o vuelve a mostrar) el puntero del sistema sobre la ventana, sin tocar lo demás del estilo. */
int  cursor_janela(int id, bool visivel)
                                      { return estilo_janela(id, JAN_SO_CURSOR | (visivel ? 0u : (unsigned)JAN_SEM_CURSOR), 0, 0); }
int  redimensionar_janela(int id, int l, int a, uint32_t **px)
                                      { return chamar(SIS_REDIMENSIONAR_JANELA, (unsigned)id, (unsigned)(l | (a << 16)), (unsigned)px); }
int  mover_janela(int id, int x, int y) { return chamar(SIS_MOVER_JANELA, (unsigned)id, (unsigned)x, (unsigned)y); }
int  frequencia_controlar(int op, int arg) { return chamar(SIS_FREQUENCIA, (unsigned)op, (unsigned)arg, 0); }   /* D106 */
int  pedir_fechar_janela(int id)     { return chamar(SIS_PEDIR_FECHAR, (unsigned)id, 0, 0); }
/* Menú contextual (D051): cinco argumentos, en EBX, ECX, EDX, ESI y EDI. */
int  menu_contextual(int id, int x, int y, const char *itens, int codigo)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_MENU_CONTEXTUAL), "b"(id), "c"(x), "d"(y), "S"(itens), "D"(codigo)
                     : "memory");
    return r;
}


int  capturar_tela(int id, int rect[4], uint32_t *destino, unsigned max_px)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_CAPTURAR_TELA), "b"(id), "c"(rect), "d"(destino), "S"(max_px) : "memory");
    return r;
}
int  inyectar_entrada(int op, int a, int b) { return chamar(SIS_INYECTAR, (unsigned)op, (unsigned)a, (unsigned)b); }
int  aviso_tela(const char *texto, const char *arquivo) { return chamar(SIS_AVISAR, (unsigned)texto, (unsigned)arquivo, 0); }
int  copiar_imagem(int l, int a, const uint32_t *px) { return chamar(SIS_COPIAR_IMAGEM, (unsigned)(l | (a << 16)), (unsigned)px, 0); }
int  colar_imagem(uint32_t *destino, unsigned max_px) { return chamar(SIS_COLAR_IMAGEM, (unsigned)destino, max_px, 0); }

/* ---- D282: permisos de archivos y dueño en el montaje ---- */
int  permisos_arquivo(const char *c, info_permisos_t *p) { return chamar4(SIS_PERMISOS_ARQ, PARQ_CONSULTAR, (unsigned)c, (unsigned)p, 0); }
int  mudar_permisos(const char *c, unsigned modo)          { return chamar4(SIS_PERMISOS_ARQ, PARQ_MODO, (unsigned)c, modo, 0); }
int  mudar_dono(const char *c, int uid, int gid)       { return chamar4(SIS_PERMISOS_ARQ, PARQ_DONO, (unsigned)c, (unsigned)uid, (unsigned)gid); }
int  mascara_criacao(int nova)                         { return chamar4(SIS_PERMISOS_ARQ, PARQ_UMASK, (unsigned)nova, 0, 0); }
int  fixar_datas(const char *c, unsigned mod, unsigned ace) { return chamar4(SIS_PERMISOS_ARQ, PARQ_DATAS, (unsigned)c, mod, ace); }   /* D341 */
int  fixar_datas_fd(int fd, unsigned mod, unsigned ace)     { return chamar4(SIS_PERMISOS_ARQ, PARQ_DATAS_FD, (unsigned)fd, mod, ace); }

int  montar_dono(const char *nome, const char *ponto, int opcoes, int uid, int gid)
{
    return chamar4(SIS_MONTAR, (unsigned)nome, (unsigned)ponto, (unsigned)opcoes | MONTAR_DONO,
                   (unsigned)(uid & 0xFFFF) | (unsigned)(gid & 0xFFFF) << 16);
}

/* "rwxr-xr-x" (9 letras y el 0; con el pegajoso, "t" o "T" en la última). */
void texto_modo(unsigned modo, char saida[10])
{
    static const char l[] = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) saida[i] = (modo & (0400u >> i)) ? l[i] : '-';
    if (modo & MODO_PEGAJOSO) saida[8] = saida[8] == 'x' ? 't' : 'T';
    saida[9] = 0;
}

/* D401: procesos POSIX (fork, exec, waitpid, señales, poll y fcntl). La libc común los envuelve
 * con los nombres de siempre; aquí solo la llamada. */
int  bifurcar(void)                   { return chamar(SIS_BIFURCAR, 0, 0, 0); }
int  substituir(const char *c, char *const argv[], char *const envp[])
                                      { return chamar(SIS_SUBSTITUIR, (unsigned)c, (unsigned)argv, (unsigned)envp); }
int  esperar_pid(int pid, int *estado, int opcoes)
                                      { return chamar(SIS_ESPERAR_PID, (unsigned)pid, (unsigned)estado, (unsigned)opcoes); }
int  sinal_op(int op, unsigned a, unsigned b, unsigned c) { return chamar4(SIS_SINAL, (unsigned)op, a, b, c); }
int  sondar(sondagem_t *v, unsigned n, int ms) { return chamar(SIS_SONDAR, (unsigned)v, n, (unsigned)ms); }
int  descritor_op(int fd, int op, unsigned arg) { return chamar(SIS_DESCRITOR, (unsigned)fd, (unsigned)op, arg); }
