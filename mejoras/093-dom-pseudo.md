# 093 - El navegador no se muere con las páginas que tienen pseudo-elementos

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-11

## Problema / motivación

Al abrir `https://www.amd.com/` el navegador **murió**:

```
[SaguiOS] Proceso 7 (cipo) finalizado: Fallo de página: escritura 0xffbefffc
```

La dirección (`0xffbefffc`) está justo por debajo de la pila del programa
(64 KB), así que no era un acceso cualquiera: era la **pila desbordada**. Y el
motivo era un **ciclo en el árbol del DOM**: la función que calcula el
`textContent` de un elemento (`qjs_dom_tam`) se llama a sí misma sin parar.

La causa estaba en el motor de la web (`userland/web/motor.c`): los nodos
**generados** (el `::before`, el `::after` y los bloques anónimos que crea el
motor para agrupar texto en línea) se creaban con `m_novo()`, que no les daba
identificador de nodo. Todos se quedaban con el identificador **0**, que es el
del primer nodo real de la página (el `#doc`). Al montar el DOM en el puente
del QuickJS, el segundo nodo con identificador 0 **machacaba** al primero, y
el `#doc` (padre del `html`) se quedaba con el `html` (padre del `#doc`): un
ciclo, y la recursión sin fin.

## Propuesta

- **En el motor de la web**: que los nodos generados se marquen con el
  identificador **-1** (`dom_gerado`), que no es un elemento del DOM (como en
  un navegador a serio, donde el `::before` no aparece en el árbol), y que al
  recoger el árbol se salten esos nodos (pero no sus hijos, que sí son
  elementos). El padre de un elemento también salta los nodos generados, para
  que un elemento dentro de un bloque anónimo siga teniendo como padre al
  elemento de verdad.
- **En el puente del QuickJS**: tres defensas para que un DOM raro nunca
  tumbe el navegador:
  - un nodo con un identificador que ya está en uso **no machaca** al otro;
  - un nodo solo se cuelga de su padre si el padre **no es un descendiente
    suyo** (`qjs_pai_valido`), lo que impide el ciclo;
  - el recorrido del árbol tiene un **límite de profundidad**, por si acaso.

## Criterios de aceptación

- [x] Una página con `::before` y `::after` carga y el navegador sigue vivo.
- [x] El `::before` no aparece en el DOM (como en un navegador a serio).
- [x] `https://www.amd.com/` carga: 4836 nodos, 14 scripts y la página pintada.
- [x] Test automático en `tests/` que lo comprueba (`tests/h137_dom_pseudo.py`).
- [x] Decisión registrada en `docs/decisoes.md` (D161).

## Notas

- El fallo salía con **cualquier** página que tuviera un `::before` o un
  `::after`: no era un caso raro de `amd.com`, era la mayoría de las páginas de
  la web moderna (el `::before` está en la mitad de los frameworks).
- El mismo fallo se vio en el puente y no en el motor: por eso el motor es
  quien da los identificadores y el puente solo los comprueba.
