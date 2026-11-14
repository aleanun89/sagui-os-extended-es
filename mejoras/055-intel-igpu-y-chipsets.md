# 055 - Gráficas integradas y chipsets Intel: blitter 2D, AHCI, e1000e, 3D Gen3 y resolución nativa

- **Estado:** propuesta, repartida por versiones (ver `docs/ROADMAP.md`):
  - **v7:** blitter 2D de Intel: **escrito en la v7.2 (D195)**, Gen3 a Gen6, sobre la capa común
    `acel2d` con autoprueba al arrancar; falta probarlo en el G470 y el Amilo (qué comprobar, en la D195);
  - **v6:** AHCI;
  - **v8:** e1000e y 3D en GMA 900/950/3100;
  - **más adelante:** 3D Gen6 y resolución nativa del panel.
- **Prioridad:** media-alta (AHCI alta)
- **Alta:** 2026-11-07
- **Relacionadas:** 054 (SaguiGL y plan AMD), 036 (PC antiguos), 029 (UHCI), D073 (drivers de
  Linux/Mesa: los de Intel son en buena parte MIT)

## Motivación

Las gráficas integradas Intel son las más comunes en portátiles y PC de oficina de 2002-2012
(entre ellas la HD 3000 del G470), e Intel publicó documentación de programación de muchas
generaciones. Del chipset, **AHCI** es lo que más se nota: hoy los discos SATA en modo AHCI no
se ven, que es el caso del G470 sin modo IDE en la BIOS, del Ryzen y de casi cualquier PC
desde 2008.

## Propuesta

| Pieza | Contenido | Versión |
| --- | --- | --- |
| **Blitter 2D** (Gen2-Gen6: i830-865, GMA 900/950/3100, X3100/X4500, HD 2000/3000) | anillo de órdenes; copiar y rellenar rectángulos para el compositor de ventanas; vigilante y vuelta a la ruta SSE2 | v7 |
| **AHCI** (SATA; Intel ICH6+ y también AMD) | puertos, FIS, tablas de órdenes y DMA; lectura y escritura con la misma política que ATA (D040: los discos internos solo con `mount -o rw`); `discos`, `mount` e informe | v8 |
| **e1000e** (82566/82567/82574/82579…) | red de portátiles de empresa; base en el e1000 existente | v8 |
| **3D en Gen3** (GMA 900/950/3100) | tubería casi fija (los vértices los calcula la CPU), muy cercana a SaguiGL / OpenGL ES 1.1: motor `i915` de SaguiGL | v8 |
| 3D en Gen6 (HD 2000/3000, G470) | shaders (EU), documentación oficial; grande | más adelante |
| **Resolución nativa del panel** (p. ej. 1366x768 del G470) | motor de pantalla: PLL, LVDS/eDP y escalador; **siempre con vuelta a VESA** y opción de arranque para desactivarlo | más adelante |

## Pruebas

- **AHCI:** QEMU `-device ich9-ahci` con discos de prueba; lectura, escritura, FAT32 y ext2,
  con las reglas de seguridad de la D040.
- **e1000e:** QEMU `-device e1000e`.
- **Blitter y 3D de Intel:** QEMU no los emula, así que van en hardware real (G470, GMA 950 si
  se consigue) con un programa de prueba que compara con la ruta por software y deja un
  informe en `/home`.

## Criterios de aceptación

- [ ] Blitter 2D en el G470: capturas idénticas a la ruta por software y menos CPU (escrito, D195;
  la autoprueba del arranque y `acel2d probar` lo comprueban; falta el hardware)
- [ ] AHCI en QEMU y en un PC real (G470 o Ryzen) respetando la D040
- [ ] e1000e en QEMU
- [ ] 3D Gen3 igual que el motor por software (cuando haya hardware)
- [ ] Decisiones en `docs/decisoes.md`; código ajeno registrado en `docs/terceros.md`
