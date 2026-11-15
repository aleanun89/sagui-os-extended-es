# 118 - Sistema multiidioma: catálogos de traducción, idioma elegible y formatos regionales

- **Estado:** fase 1 hecha en la v8.4 (D313, `docs/idiomas.md`); queda lo de la v8.4.1 (abajo)
- **Prioridad:** alta
- **Alta:** 2026-11-15
- **Relacionadas:** traducciones del README (Haiku), componentes de la libsagui, Pluma (116), Macaco (117), correo (113), entorno (`ambiente.c`)

## Motivación
Hoy todos los textos están en español dentro del código (unas 1400 cadenas en los programas y 600 en la
interfaz del kernel). Para llegar a usuarios de Brasil y de habla inglesa, el sistema tiene que poder
mostrarse en varios idiomas sin duplicar programas.

## Propuesta
1. **Catálogos propios al estilo gettext:** `_("texto")` en el código, con la cadena en español como clave
   (si falta la traducción sale en español). `tools/extraer_textos.py` genera ficheros `.po` (formato
   estándar, editables con Poedit/Weblate) y los compila a catálogos binarios
   `/sistema/idiomas/<idioma>/<programa>.cat`. Lector propio en la libsagui (hash), sin código de GNU gettext.
2. **Idioma elegible:** `LANG` del sistema (`/etc`) y por usuario (`/home/.config`), pasado por el entorno;
   pestaña **Idioma** en Configuración y en el instalador. La interfaz del kernel (barra, menús, avisos,
   diálogos) carga su catálogo al arrancar; el `dmesg` sigue en español.
3. **Formatos regionales** en la libsagui: fecha y hora, separador decimal y de miles, moneda, primer día de
   la semana, tamaño de papel (A4/Carta).
4. **Codificación:** la fase 1 se queda en Latin-1 (es, pt, en, fr, de, it caben). UTF-8 interno (cirílico,
   griego, CJK) es una fase aparte, solo si se decide.
5. **Pruebas:** pseudo-idioma («[Ğüåŕđåŕ...]») para encontrar textos sin traducir y botones donde no cabe el
   texto; prueba en QEMU con `LANG=en` y `LANG=pt_BR` con capturas.

## Fases
- **Fase 1 (v8.4):** infraestructura + Configuración, instalador, escritorio/barra, Archivos, Pluma, Macaco y
  correo en es, pt_BR y en.
- **Fase 2:** el resto de programas.
- **Fase 3 (por decidir):** UTF-8 interno y fuentes con más glifos.

## Quién lo hace
- **Sonnet:** infraestructura (lector de catálogos, herramienta de extracción, `LANG`, Configuración, catálogo
  del kernel, formatos regionales, pseudo-idioma y prueba).
- **Haiku:** pasar los programas a `_()` (mecánico) y traducir los `.po` a pt_BR y en, como hace con los README.
- Lo nuevo (Pluma, Macaco, correo) se escribe con `_()` en cuanto exista la macro.

## Criterios de aceptación
- [x] `_()`, catálogos y `tools/extraer_textos.py`
- [x] Idioma elegible en Configuración (sección en la página Teclado) y la interfaz del kernel traducida; [ ] en el instalador (v8.4.1)
- [x] Formatos regionales en la libsagui: decimal, miles y fecha (moneda, primer día de la semana y papel, v8.4.1)
- [x] es, pt_BR y en completos en Configuración, Archivos, el escritorio y la barra y el diálogo de archivos; [ ] Pluma, Macaco y correo (v8.4.1)
- [x] Prueba con pseudo-idioma y capturas en `LANG=en`/`pt_BR` (`tests/h315_idiomas.py`)
- [x] Decisión en `docs/decisoes.md` (D313)

## Pendiente para la v8.4.1
- Pasar a `_()` y traducir Pluma, Macaco, correo, el instalador (con su selector de idioma) y el resto de programas (fase 2);
  Haiku puede encargarse del paso mecánico con `tools/extraer_textos.py`.
- La línea de comandos de los programas (ayudas, errores) y los «motivos» que el kernel muestra en el diálogo de permisos.
- Moneda, primer día de la semana y tamaño de papel; usar los formatos regionales en Macaco.
- Que las ventanas ya abiertas cambien de idioma sin cerrarlas.
- Una pestaña **Idioma** propia en Configuración si se amplía la ventana (hoy, sección de la página Teclado).
