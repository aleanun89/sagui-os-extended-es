/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Instancia única de un reproductor (D339).
 *
 * Fallo que corrige: con el reproductor sonando, un doble clic en otra pista de Archivos (o en el aviso del CD)
 * lanzaba OTRO proceso, que abría su propio flujo de audio; el kernel mezcla los flujos y se oían dos pistas a la
 * vez. Ahora el proceso nuevo, si hay un reproductor vivo, le entrega las rutas por un fichero de cola y se va; el
 * viejo las recoge entre evento y evento, para lo que suena y toca la pista pedida.
 *
 * Protocolo (todo en /tmp, sin tocar el kernel):
 *   /tmp/<nombre>.pid    pid de la instancia viva (se borra al salir; puede quedar viejo tras un fallo);
 *   /tmp/<nombre>.cola   rutas, una por línea, que escribe el proceso nuevo;
 *   la instancia viva renombra la cola a .lendo, la lee y la borra. El proceso nuevo espera hasta 1,5 s a que la
 *   cola desaparezca; si no desaparece (pid viejo), borra la cola y sigue como instancia nueva.
 */
#ifndef INSTANCIA_UNICA_H
#define INSTANCIA_UNICA_H

#define IU_ESPERA_MS 1500
#define IU_PERIODO_MS 150   /* cada cuánto mira la cola la instancia viva cuando está en reposo */

static void iu_ruta(char *d, size_t max, const char *nome, const char *ext)
{
    formatar_texto(d, max, "/tmp/%s.%s", nome, ext);
}

/* Entrega las rutas a la instancia viva. true = las recogió (el que llama debe salir). */
static bool iu_entregar(const char *nome, int n, char **rutas)
{
    char pid_c[64], cola[64];
    info_arquivo_t inf;
    iu_ruta(pid_c, sizeof pid_c, nome, "pid");
    iu_ruta(cola, sizeof cola, nome, "cola");
    if (n <= 0 || info_arquivo(pid_c, &inf) < 0) return false;
    int fd = abrir(cola, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0) return false;
    for (int i = 0; i < n; i++) {
        escrever(fd, rutas[i], (unsigned)tam_texto(rutas[i]));
        escrever(fd, "\n", 1);
    }
    fechar(fd);
    for (unsigned t = 0; t < IU_ESPERA_MS; t += 25) {
        dormir(25);
        if (info_arquivo(cola, &inf) < 0) return true;
    }
    apagar(cola);   /* nadie la recogió: el pid era de una instancia que ya no existe */
    return false;
}

static void iu_registrar(const char *nome)
{
    char c[64], t[32];
    iu_ruta(c, sizeof c, nome, "pid");
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    int k = formatar_texto(t, sizeof t, "%d\n", meu_pid());
    escrever(fd, t, (unsigned)k);
    fechar(fd);
}

static void iu_liberar(const char *nome)
{
    char c[64];
    iu_ruta(c, sizeof c, nome, "pid");
    apagar(c);
}

/* Recoge la cola: rutas separadas por '\0' en buf; devuelve cuántas (0 = nada). */
static int iu_recoger(const char *nome, char *buf, int max)
{
    char cola[64], lendo[64];
    info_arquivo_t inf;
    iu_ruta(cola, sizeof cola, nome, "cola");
    iu_ruta(lendo, sizeof lendo, nome, "lendo");
    if (info_arquivo(cola, &inf) < 0) return 0;
    apagar(lendo);
    if (renomear(cola, lendo) < 0) return 0;
    int fd = abrir(lendo, ABRIR_LER);
    if (fd < 0) return 0;
    int n = ler(fd, buf, (unsigned)max - 1);
    fechar(fd);
    apagar(lendo);
    if (n <= 0) return 0;
    int rutas = 0;
    for (int i = 0; i < n; i++)
        if (buf[i] == '\n') { buf[i] = 0; rutas++; }
    return rutas;
}

#endif
