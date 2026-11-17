/* jvm_printf_renombrado.h - se incluye (-include) solo al compilar utils/d_type.c (D376): su jvm_printf
 * pasa a llamarse jvm_printf_orig y el de verdad es el de registro_sagui.c, que calla los avisos
 * [INFO] de la VM salvo con la variable de entorno JAVA_INFO. */
#ifndef SAGUI_JVM_PRINTF_RENOMBRADO_H
#define SAGUI_JVM_PRINTF_RENOMBRADO_H
#define jvm_printf jvm_printf_orig
#endif
