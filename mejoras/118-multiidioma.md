# 118 - Sistema multiidioma: catálogos de traducción, idioma elegible y formatos regionales

- **Estado:** aprobada (**v8.4**, decidido por el mantenedor el 2026-11-15)
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
- [ ] `_()`, catálogos y `tools/extraer_textos.py`
- [ ] Idioma elegible en Configuración y en el instalador; la interfaz del kernel traducida
- [ ] Formatos regionales en la libsagui
- [ ] es, pt_BR y en completos en las aplicaciones de la fase 1
- [ ] Prueba con pseudo-idioma y capturas en `LANG=en`/`pt_BR`
- [ ] Decisión en `docs/decisoes.md`
