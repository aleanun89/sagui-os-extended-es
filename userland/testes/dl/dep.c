// SPDX-License-Identifier: GPL-3.0-only
/* dep.c - biblioteca compartida de prueba libdldep.so (D378, h385).
 *
 * Depende de libdlbase.so (DT_NEEDED, encontrada por DT_RUNPATH=$ORIGIN):
 * llama a dl_soma por la PLT (JMP_SLOT) y lee dl_contador por la GOT
 * (GLOB_DAT). Llama además a una función del programa que la abre
 * (anfitriao_registrar, ofrecida con dl_exportar) desde su constructor y su
 * destructor. Su tabla de símbolos solo lleva DT_HASH (el clásico de System V). */

extern int dl_contador;
extern int dl_soma(int a, int b);
extern void anfitriao_registrar(const char *texto);

__attribute__((constructor)) static void iniciar(void) { anfitriao_registrar("dep:iniciar"); }
__attribute__((destructor)) static void terminar(void) { anfitriao_registrar("dep:terminar"); }

int dl_soma3(int a, int b, int c) { return dl_soma(dl_soma(a, b), c); }
int dl_ler_contador(void) { return dl_contador; }
int *dl_endereco_contador(void) { return &dl_contador; }
