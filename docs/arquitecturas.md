# Soporte de arquitecturas de procesador por versión

Documento vivo (se actualiza cuando cambia el plan; ver también `docs/gpus.md`). Estado a 2026-11-15 (v8.0).
**Leyenda:** ✅ funciona · 🔧 en desarrollo · 📅 planificado · 💡 idea, sin versión · — no previsto.

| Arquitectura | Estado | Versión | Mejora | Notas |
| --- | --- | --- | --- | --- |
| **x86 de 32 bits (i686)** | ✅ | desde el principio | — | La de hoy. Se **mantiene** después de la v10 para los PC sin 64 bits (Fujitsu Amilo, Core Duo). Arranque por BIOS y por UEFI IA32 (v8.0). **v8.5: PAE** (hasta 64 GB de RAM, NX) para los portátiles antiguos con 8-16 GB, sin pasar a 64 bits (mejora 125). **Requisito mínimo: CPU con CMOV** (Pentium Pro / Pentium II / Athlon o posterior); las i586 (AMD K6/K6-2/K6-III, Pentium, Pentium MMX) y los 486 reciben un aviso claro de la etapa 2 y la máquina se queda parada, sin reiniciar (v8.3.3, D310) |
| **x86-64** | 📅 | **v10.0** | 106 | Del **mismo código** que la de 32 bits (dos objetivos de compilación); arranque UEFI x64 (ya hecho en v8.0) y BIOS; programas de 32 bits en modo compatibilidad (por decidir, discusión de la v10). Base de Vulkan y las GPU modernas (v11) |
| **ARM64 (AArch64)**: Raspberry Pi 4/5, otros | 💡 | se decide **después de la v10** (mantenedor, 2026-11-15) | 028 | «No hacer por ahora». Aprox. la mitad del sistema se porta casi tal cual y la otra mitad (arranque, interrupciones, MMU, drivers) hay que escribirla. Tras la v10, la separación del código por arquitectura lo facilita |
| **ARM de 32 bits** (Raspberry Pi 2/3 en 32 bits) | — | — | 028 | Si se hace ARM, mejor directamente ARM64 |
| **RISC-V** | — | — | — | No previsto |

## Requisitos que se van preparando

| Pieza | Para qué arquitectura | Cuándo |
| --- | --- | --- |
| Arranque UEFI x64 e IA32 (D264) | x86-64, i686 | ✅ v8.0 |
| PAE: más de 4 GB de RAM y NX en 32 bits (mejora 125) | i686 con PAE (casi todas desde el Pentium Pro; sin PAE, como hoy) | 🔧 v8.5 |
| Separar el código dependiente de la arquitectura (`kernel/arch/<arq>/`: arranque, cambio de contexto, paginación, interrupciones, llamadas al sistema) | x86-64 y futuras (ARM) | 📅 v10.0 |
| Compiladores propios (`cc`, Pascal) con otro destino | x86-64 (y ARM si llega) | 📅 v10.0 |
| Drivers sin suposiciones de 32 bits (punteros, DMA, BAR de 64 bits) | x86-64 | 📅 v10.0 |
