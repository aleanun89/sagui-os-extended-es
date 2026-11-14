// SPDX-License-Identifier: GPL-2.0-or-later
/* smp - prueba de las primitivas del SMP y de la sincronización (D150, H126).
 *
 * El programa no tiene hilos de verdad (el SO todavía no los tiene, D150),
 * así que los "hilos" aquí son los turnos de un bucle: cada vuelta hace el
 * trabajo de un hilo y cede el turno al siguiente. Con eso se comprueba que
 * el contador compartido con atómicas suma exactamente lo que debe, y que
 * el mutex y el semáforo sirven.
 *
 * Lo que NO se puede probar todavía:
 *   - que un hilo espere de verdad a que otro suelte el cerrojo (espera
 *     bloqueante): con un solo hilo por proceso, el que espera se quedaría
 *     colgado sin nadie que le suelte el cerrojo. Cuando haya hilos de
 *     verdad, esa parte se probará con dos hilos del mismo programa
 *     (ver D150);
 *   - que las atómicas aguanten dos núcleos a la vez: hoy el arranque no
 *     enciende los procesadores secundarios.
 *
 * Lo que se imprime son valores exactos, para que la prueba H126 los
 * compare sin adivinar nada. */
#include "sagui.h"
#include "verifica.h"

#define HILOS 4        /* "hilos" simulados */
#define RONDAS 100     /* vueltas de cada uno */

static volatile uint32_t contador_atomico;   /* compartido, con atómicas */
static volatile uint32_t contador_mutex;     /* compartido, con el mutex */
static volatile uint32_t dentro;             /* 1 = hay alguien en la sección crítica */

/* Contador de 64 bits: el cmpxchg8b necesita la dirección alineada a 8
 * bytes, y en un i386 un uint64_t no siempre lo está. */
static volatile uint64_t contador64 __attribute__((aligned(8)));

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;

    /* ---- 1. las atómicas de 32 bits ---- */
    volatile uint32_t v = 5;
    /* Cada comprobación va justo detrás de su operación: el valor de "v" que
     * se compara es el que quedó después de esa operación, no el de después
     * de todas (que era el error de la primera vez). */
    int cas = atomico_cas32(&v, 5, 7);
    verifica(cas == 1 && v == 7, "la comparación y el cambio mueven el valor si el esperado acierta");
    int falhou = atomico_cas32(&v, 5, 9);
    verifica(falhou == 0 && v == 7, "la comparación y el cambio no mueven nada si el esperado está mal");
    uint32_t add = atomico_add32(&v, 3);
    verifica(add == 7 && v == 10, "la suma devuelve el valor anterior");
    uint32_t sub = atomico_sub32(&v, 4);
    verifica(sub == 10 && v == 6, "la resta devuelve el valor anterior");
    uint32_t inc = atomico_inc32(&v);
    verifica(inc == 6 && v == 7, "el incremento devuelve el valor anterior");
    uint32_t dec = atomico_dec32(&v);
    verifica(dec == 7 && v == 6, "el decremento devuelve el valor anterior");
    atomico_barreira();
    imprimir("SMP atomicas: cas=%d falhou=%d add=%u sub=%u inc=%u dec=%u final=%u\n",
             cas, falhou, add, sub, inc, dec, v);

    /* ---- 2. las atómicas de 64 bits (si la CPU tiene cmpxchg8b) ---- */
    int tem64 = atomico_tem_cas64() ? 1 : 0;
    uint64_t add64 = 0;
    int cas64 = 0, falhou64 = 0;
    if (tem64) {
        contador64 = 100;
        add64 = atomico_add64(&contador64, 5);
        verifica(add64 == 100 && contador64 == 105, "la suma de 64 bits devuelve el valor anterior");
        cas64 = atomico_cas64(&contador64, 105, 200) ? 1 : 0;
        verifica(cas64 == 1 && contador64 == 200, "la comparación y el cambio de 64 bits");
        falhou64 = atomico_cas64(&contador64, 105, 300) ? 1 : 0;
        verifica(falhou64 == 0 && contador64 == 200, "la comparación y el cambio de 64 bits falla bien");
    }
    imprimir("SMP atomicas64: tem=%d add=%u cas=%d falhou=%d final=%u\n",
             tem64, (unsigned)add64, cas64, falhou64, (unsigned)contador64);

    /* ---- 3. el mutex ---- */
    mutex_t m;
    mutex_criar(&m);
    verifica(mutex_tomar(&m), "toma el mutex libre");
    verifica(!mutex_tentar(&m), "con el mutex tomado, intentar no espera y devuelve falso");
    mutex_largar(&m);
    verifica(mutex_tentar(&m), "después de soltar, el mutex vuelve a estar libre");
    mutex_largar(&m);

    /* ---- 4. el semáforo ---- */
    semaforo_t s;
    semaforo_criar(&s, 2);
    verifica(semaforo_tentar(&s), "tira del semáforo con dos disponibles");
    verifica(semaforo_tentar(&s), "tira el segundo del semáforo");
    verifica(!semaforo_tentar(&s), "ya no hay más en el semáforo");
    semaforo_sinalizar(&s);
    semaforo_sinalizar(&s);
    semaforo_sinalizar(&s);
    verifica(semaforo_valor(&s) == 3, "sinalizar suma al contador del semáforo");
    semaforo_tomar(&s);
    semaforo_tomar(&s);
    semaforo_tomar(&s);
    verifica(semaforo_valor(&s) == 0, "tomar resta del contador del semáforo");

    /* ---- 5. los "hilos": el contador atómico y el del mutex ---- */
    int erros = 0;
    for (int r = 0; r < RONDAS; r++) {
        for (int h = 0; h < HILOS; h++) {
            /* el "hilo" suma en el contador compartido con una atómica */
            atomico_add32(&contador_atomico, 1);
            /* y lo hace también dentro del mutex, para comprobar que el
             * mutex excluye de verdad */
            if (!mutex_tentar(&m)) {
                erros++;
            } else {
                if (dentro) erros++;     /* dos dentro a la vez: imposible */
                dentro = 1;
                contador_mutex++;
                dentro = 0;
                mutex_largar(&m);
            }
        }
    }
    uint32_t total = (uint32_t)HILOS * RONDAS;
    verifica(contador_atomico == total, "el contador con atómicas suma exactamente lo que debe");
    verifica(contador_mutex == total, "el contador protegido por el mutex suma exactamente lo que debe");
    verifica(erros == 0, "el mutex no dejó entrar a dos a la vez");
    imprimir("SMP contador atomico: %u\n", contador_atomico);
    imprimir("SMP contador mutex: %u\n", contador_mutex);
    imprimir("SMP semaforo: %u\n", semaforo_valor(&s));
    imprimir("SMP erros: %d\n", erros);
    return resultado("smp");
}
