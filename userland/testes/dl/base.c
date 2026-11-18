// SPDX-License-Identifier: GPL-3.0-only
/* base.c - biblioteca compartida de prueba libdlbase.so (D378, h385).
 *
 * Se compila con -fPIC -shared. Tiene lo que necesita relocaciones de tipo
 * RELATIVE (una tabla de punteros a función en la zona RELRO y un puntero a
 * texto en .data), datos en la BSS y un constructor (.init_array). Su tabla
 * de símbolos solo lleva DT_GNU_HASH. */

int dl_contador;                       /* BSS: lo lee libdldep.so (GLOB_DAT) */
int dl_iniciado;                       /* lo pone el constructor */

static int dobro(int x) { return 2 * x; }
static int triplo(int x) { return 3 * x; }

/* Tabla constante de punteros: con -z relro queda en solo lectura tras relocar. */
int (*const dl_tabela[2])(int) = { dobro, triplo };
const char *dl_saudacao = "hola desde libdlbase";

__attribute__((constructor)) static void iniciar(void) { dl_iniciado = 1234; }

int dl_soma(int a, int b)
{
    dl_contador++;
    return a + b;
}

int dl_aplicar(int i, int x) { return dl_tabela[i & 1](x); }

/* Para comprobar W^X desde el programa: la dirección de una función propia. */
void *dl_endereco_codigo(void) { return (void *)dl_soma; }
void *dl_endereco_tabela(void) { return (void *)dl_tabela; }
