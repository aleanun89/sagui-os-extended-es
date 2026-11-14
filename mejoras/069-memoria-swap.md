# 069 - Que el sistema no muera sin RAM: OOM, swap comprimida en RAM y partición de swap

- **Estado:** idea para **mucho más adelante** (mejora «gorda»; decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 035 (arquitectura de almacenamiento: carga bajo demanda), 068 (imagen con varias particiones), 017 (SMP), 055 (AHCI)

## Problema

Cuando se acaba la RAM, SaguiOS no tiene a dónde ir:
- Si el que falla es un programa, solo muere ese programa ([excecoes.c](../kernel/arch/excecoes.c)).
- Si el que se queda sin memoria es el kernel, por ejemplo para las tablas de páginas, la
  máquina se para ([paginacao.c](../kernel/mem/paginacao.c)).
- La 035 descarta de la RAM lo que se puede volver a leer, pero **no** contempla swap.

## Alternativas, por orden

### 1. Que no muera (lo primero y lo más barato)

- Ninguna reserva de un programa para el kernel: si falla, la llamada devuelve un error.
- Si de verdad no queda RAM, «OOM killer» sencillo: se cierra el proceso que más memoria
  usa (nunca el escritorio ni el shell de arranque) con un aviso visible.
- Límites de memoria por proceso, e indicador de memoria en la bandeja y en el
  Administrador de tareas.

### 2. Descartar lo que se puede releer (la 035)

Código de programas y bibliotecas, y caché de disco: se descartan y se vuelven a cargar bajo
demanda. Requisito de lo siguiente: **paginación bajo demanda** (un fallo de página de una
página ausente la trae y el proceso sigue).

### 3. Swap **comprimida en RAM** (idea del mantenedor)

En PC con **más de 2 GB de RAM**, el kernel de 32 bits no aprovecha toda la memoria (en el dv7
gestiona 1,8 GB de 4). Esa RAM sobrante, por encima de lo que el kernel mapea, se usa como
**almacén de páginas comprimidas**, al estilo de zram/zswap:
- Las páginas frías de los procesos se comprimen y se guardan ahí. Se accede a ese almacén
  mapeándolo por ventanas temporales, o con PAE si hace falta pasar de 4 GB.
- **Algoritmo trivial y rápido**, que no perjudique al rendimiento:
  - páginas de ceros o rellenas de un solo valor: no ocupan nada;
  - el resto, con un LZ sencillo al estilo de LZ4 (sin entropía);
  - si una página no comprime al menos un 25 %, no merece la pena y se guarda tal cual o va
    a disco.
- Mucho más rápida que un disco y no desgasta pendrives. Se mide con SaguiMark (grupo
  Memoria) que no se nota cuando no hay presión de memoria.

### 4. Partición de swap

- Partición `SAGUI-SWAP` (tipo 0x82), creada por el generador de imágenes de la 068
  (`SWAP_MB=`), o un archivo de swap.
- Se usa después de la comprimida: primero RAM comprimida, luego disco.
- Avisar de que en un pendrive es lenta y lo desgasta; mejor en disco duro o SSD (AHCI, 055).

## Lo difícil

- Leer del disco *dentro* de un fallo de página: hoy las llamadas al sistema corren con las
  interrupciones desactivadas y el USB rápido depende de ellas. Hay que poder dormir el
  proceso hasta que llegue su página, lo que toca el planificador y los drivers.
- No sacar nunca páginas del kernel, de búferes de DMA ni de la pila de interrupciones.
- Elegir qué página sacar (reloj o LRU aproximado) sin coste apreciable.
- Con varios núcleos (017): invalidación de TLB entre núcleos.

## Criterios de aceptación (por fases)

- [ ] Fase 1: un programa que reserva sin parar no tumba el sistema; aviso y cierre del culpable
- [ ] Fase 2: paginación bajo demanda (con la 035)
- [ ] Fase 3: swap comprimida en la RAM sobrante, medida con SaguiMark
- [ ] Fase 4: partición `SAGUI-SWAP`
- [ ] Tests automáticos con poca RAM en QEMU (`-m 64`, `-m 128`)
- [ ] Decisiones registradas en `docs/decisoes.md`

## Notas

Modelo sugerido: **Opus** (núcleo del kernel, memoria y planificador).
