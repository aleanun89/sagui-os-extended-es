/* ulab_sagui.h - configuración de ulab (el «NumPy» de MicroPython) para SaguiOS (D479).
 * Se pasa con -DULAB_CONFIG_FILE. Lo no definido aquí queda con los valores de ulab.h. */
#ifndef ULAB_SAGUI_H
#define ULAB_SAGUI_H
/* Hasta 3 dimensiones (series de tiempo y matrices; 4 gasta más código). */
#define ULAB_MAX_DIMS                   3
/* load/save/loadtxt/savetxt usan el VFS de MicroPython, que SaguiOS no tiene (D067):
 * el módulo «tablas» lee y escribe los ficheros con el open() propio. */
#define ULAB_NUMPY_HAS_LOAD             (0)
#define ULAB_NUMPY_HAS_LOADTXT          (0)
#define ULAB_NUMPY_HAS_SAVE             (0)
#define ULAB_NUMPY_HAS_SAVETXT          (0)
#endif
