# 099 - r8169 de sala limpia (sustituir el código de Linux GPL-2.0-only)

- **Estado:** propuesta (opción B del mantenedor, 2026-11-14); la opción A (D259) ya está hecha
- **Prioridad:** media
- **Alta:** 2026-11-14
- **Versión:** v8.4 o cuando toque la red
- **Relacionadas:** D073, D091, D124, D258, D259; mejora 057 (variantes), 098 no

## Motivación

`kernel/terceros/linux/r8169/` (tabla de revisiones y secuencias de inicio de las RTL8168E/E-VL/G/H)
viene de Linux (`r8169_main.c`, **GPL-2.0-only**), incompatible con la GPL v3 del proyecto. Con la D259
ese código solo entra en las compilaciones privadas (`PUBLICABLE=0`) y lo publicable usa el inicio
clásico propio, con lo que **las 8168E-VL (Toshiba L850) y las G/H no funcionan en lo publicado**.
Esta mejora lo sustituye por código sin esa procedencia.

## Por qué no se pudo sustituir por una fuente existente (D259)

- `re(4)` de FreeBSD y OpenBSD, y el driver de Realtek para FreeBSD: BSD de 4 cláusulas (publicidad),
  incompatible con la GPL; además los `re(4)` de los BSD no tienen las secuencias ERI/MAC OCP.
- U-Boot (`rtl8169.c`, GPL-2.0+) e iPXE (`realtek.c`, GPL-2.0+/UBDL): licencia compatible, pero solo
  conocen la tabla de nombres y el desbloqueo de RXDV_GATED_EN; no tienen las secuencias de inicio.
- El **`r8168` de Realtek para Linux** (8.048.00, 2019, espejo `Moxa-Linux/realtek-r8168-driver`):
  la cabecera dice «either version 2 of the License, or (at your option) any later version»
  (GPL-2.0-or-later) y tiene las secuencias completas (E, E-VL, G, GU, 8411B, H). **Pero** las versiones
  recientes (8.057, 2025) añaden `SPDX-License-Identifier: GPL-2.0-only` que contradice ese texto, y
  sus secuencias **no son idénticas** a las probadas en el L850 (p. ej. en la 8168E-VL el registro EPHY
  0x00 recibe 0x08 en vez de 0x04). Adoptarlo sin probarlo en el L850 puede romper la red que hoy funciona.
  Es una vía alternativa más corta que la sala limpia si el mantenedor acepta la ambigüedad de licencia.

## Propuesta: proceso de sala limpia

1. **Agente A (especificación):** escribe, **en prosa**, qué hace el chip en cada paso (qué registros hay
   que tocar y para qué, en qué orden y con qué esperas), a partir de **comportamiento observable**: las
   hojas de datos públicas, el volcado de registros del chip en el L850 con el inicio que ya funciona
   (`ifconfig`/`ip` con diagnóstico) y las pruebas de prueba y error en la máquina. **No** lee Linux ni
   `r8168`, y no copia valores de ellos. Entrega un documento (`docs/r8169-especificacion.md`).
2. **Agente B (implementación):** que **no ha visto** el código de Linux ni el `r8169_inicio.h` actual,
   implementa `kernel/rede/r8169_inicio_limpio.h` solo desde esa especificación, con las mismas funciones
   que usa `r8169.c`.
3. **Revisión:** el mantenedor comprueba que ni el agente B ni la especificación citan Linux. Los valores
   mágicos (EPHY, ERI, MAC OCP) que la especificación no pueda justificar con comportamiento se
   descartan o se obtienen por tanteo con el chip real, anotando cómo.
4. Se borra `kernel/terceros/linux/r8169/` y el interruptor `PUBLICABLE` deja de tocar el r8169.

## Pruebas

- Anfitrión: una versión simulada de la tarjeta (como `tests/recursos/r8169_8168e_anfitriao.c`, h96)
  comprueba que escribe lo que dice la especificación.
- **Hardware real: Toshiba L850 (RTL8168evl)**: DHCP y ping con el inicio nuevo (hoy lo hace el de
  Linux, probado el 2026-11-08, D124). Y, si hay máquina, una RTL8111G/H.

## Riesgos

- Sin hardware, el inicio de la E-VL por tanteo puede tardar varias rondas de prueba en el L850.
- Es un trabajo de decenas de registros que el chip real tiene que confirmar.
