# 085 - Las transiciones CSS en el motor de la web

- **Estado:** en curso
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** 081 (el navegador con motor de verdad), D141 (el motor de la web), D148 (el filtro CSS y las animaciones), D153 (esta decisión)

## Problema / motivación

El motor de la web del Cipó (`userland/web/motor.c`) ya sabía las **animaciones**
CSS (`@keyframes` con la propiedad `animation`, la mejora D148): cuando el valor
calculado de una propiedad cambiaba dentro de un `animation`, el motor interpolaba
entre los cuadros y avisaba con `motor_tick()` de que había que repintar.

Faltaba la otra mitad del CSS moderno de movimiento: la propiedad **`transition`**.
Sin ella, una página que cambia de color al pasar el ratón por encima, o al
cambiar una clase, o al cambiar el estilo con un script, **salta** de un valor al
otro. Es el efecto que hace que una interfaz se sienta viva, y es de lo primero que
se echa de menos en una página.

El motor de origen del que se trajo (`robson-apk/sagui-os`, rama `cipo-web`) no
tenía `transition`: solo `animation`. La hoja de ruta de la mejora 081 lo dejó
apuntado como una carencia.

## Propuesta

Implementar `transition` **dentro del motor**, reutilizando la maquinaria que ya
existe para las animaciones: el mismo reloj (`motor_tick`), la misma mezcla de
colores y el mismo "algo cambió, hay que repintar".

### La sintaxis que se entiende

| Forma | Ejemplo |
| --- | --- |
| Atajo | `transition: color 300ms;` |
| Atajo con lista | `transition: color 300ms ease, background-color 0.5s linear 100ms;` |
| Forma larga | `transition-property: color;` + `transition-duration: 300ms;` + `transition-delay: 0s;` + `transition-timing-function: ease;` |

- Los tiempos van en **segundos** (`0.3s`) o en **milisegundos** (`300ms`).
- En el atajo, el **primer** tiempo es la duración y el **segundo** es el retraso.
- Las listas de las formas largas se emparejan por posición; si una lista es más
  corta que la declaración, sus valores se repiten (como en el CSS).
- Las curvas: `linear`, `ease`, `ease-in`, `ease-out` y `ease-in-out` (las
  `cubic-bezier` del CSS, con los mismos puntos de control).

### Las propiedades que se interpolan

| Propiedad | Cómo se interpola |
| --- | --- |
| `color`, `background-color` | mezcla de los tres canales |
| `border-color` y `border-*-color` | mezcla de cada lado |
| `opacity` | número de 0 a 100 |
| `width`, `height` | la medida, si los dos lados tienen la misma unidad |
| `margin`, `padding` (y sus cuatro lados) | cada lado, con la misma regla |

Cuando el valor **no** se puede interpolar, la transición **se ignora y el valor
salta**, como antes: es el caso de `inherit`, `initial`, `auto`, `none`, de un
fondo transparente y de una medida cuya unidad cambia.

### Cómo se avisa del repintado

`motor_tick()` es el reloj del motor (D061): quien llama da el instante y el motor
devuelve `true` cuando algo cambió. Las transiciones usan **ese mismo reloj**:

- Una transición **empieza** cuando la cascata calcula un valor nuevo que no es
  igual al anterior (por ejemplo, porque ha entrado una regla `@media` al
  cambiar el ancho de la ventana).
- En cada `motor_tick()` el motor escribe en el estilo del nodo el valor
  intermedio y devuelve `true` mientras la transición corre.
- Cuando la transición acaba, `motor_tick()` vuelve a devolver `false`: **no
  quedan pinturas pendientes**.
- Las propiedades que cambian la caja (`width`, `height`, `margin`, `padding`)
  además obligan a **rehacer el layout**, porque la caja cambió de tamaño.

### Ficheros tocados

- `userland/web/motor.c`: la declaración (`transition` y las formas largas), la
  lista de transiciones a correr y la máquina que interpola.
- `userland/web/motor.h`: sin cambios en la API (se reutiliza `motor_tick`).
- `userland/testes/cipo-motor.c`: la cuarta fase del programa de prueba.
- `tests/h130_motor_transicoes.py`: la prueba.

## Criterios de aceptación

- [x] `transition: <prop> <tempo> [curva] [retraso], ...` y las formas largas
      `transition-property`, `transition-duration`, `transition-delay` y
      `transition-timing-function`
- [x] Tiempos en segundos (`0.3s`) y en milisegundos (`300ms`)
- [x] Interpolación de `color`, `background-color`, `border-color`, `opacity`,
      `width`, `height`, `margin` y `padding`
- [x] Curvas `linear`, `ease`, `ease-in`, `ease-out` y `ease-in-out`
- [x] `motor_tick()` avisa mientras la transición corre y se calla cuando acaba
- [x] Regresión cero: una página sin `transition` pinta igual que antes (los
      nodos, los comandos de pintura y la altura de la página no cambian)
- [x] `tests/h130_motor_transicoes.py` en verde
- [x] Decisión registrada en `docs/decisoes.md`: D153

## Notas

**El motor sigue sin reloj.** La decisión es la misma que con las animaciones
(D061): el motor es un renderizador puro y quien llama a `motor_tick()` le da
el instante. Sin llamadas a `motor_tick()` no hay transiciones (ni animaciones):
el valor salta, como antes.

**Sin reloj no hay transición.** Si el reloj del motor todavía no ha arrancado,
un cambio de valor **no** interpola: salta. Es lo que evita que una página se
quede con el valor viejo en el aire cuando nadie llama al motor.

**La lista de transiciones es corta a propósito.** Como mucho hay **64**
transiciones corriendo a la vez (unas 6 KB) y **4** declaradas por elemento. El
motor tiene la memoria contada y una página con `transition: all` en muchos
elementos se queda sin transiciones antes que con la memoria llena. Las
propiedades que cubre `all` son `color`, `background-color`, `border-color`,
`opacity`, `width`, `height`, `margin` y `padding`.

**Lo que no se interpola.** `transform`, `box-shadow`, los degradados y los
filtros siguen sin transición: no son números ni colores simples, y meterlos
costaría una máquina de mezcla por propiedad. Tampoco hay `transition` en las
propiedades que el motor no calcula (por ejemplo, `display`).

**Lo que falta de la lista de propiedades del CSS.** `transition-property`
acepta `all` y `none`; las curvas `cubic-bezier(...)`, `steps(...)` y
`step-start`/`step-end` se tratan como `linear`.
