# 057 - Compilación con GPL y sin GPL (dos variantes de la imagen)

- **Estado:** hecha en la rama `licencias-build` (D109), pendiente de fusionar (**v5.1**)
- **Prioridad:** alta (requisito de la D073 antes de distribuir)
- **Alta:** 2026-11-07
- **Relacionadas:** D073 (puntos 4 y 5), rama `licencia`, `docs/terceros.md`, D091 (r8169), 050
  (sensores)

## Problema / motivación

La D073 permite usar código GPL de Linux en los drivers, siempre que se pueda **excluir al
compilar** y que siga siendo posible generar una imagen 100 % propia y de licencia permisiva.
Hoy no existe esa opción.

El código GPL presente a 2026-11-07, según `docs/terceros.md` y las cabeceras SPDX:
- `kernel/terceros/linux/r8169/r8169_versoes.h`: tabla de versiones MAC y registros del r8169;
- `kernel/terceros/linux/r8169/r8169_inicio.h`: secuencias de inicio de las RTL8168g/h;
- `kernel/lib/sensores_conv.c`: **tablas** de modelos (TjMax y desvíos Tctl/Tdie) tomadas como
  referencia de `coretemp`/`k8temp`/`k10temp`. Hay que decidir si cuentan como derivadas; por
  prudencia, tratarlas como GPL y separarlas.

Futuro: firmware de AMD (`firmware/`, `SEM_FIRMWARE=1`) y más drivers según la D073.

## Propuesta

- **Opciones del Makefile:**
  - `make` (por defecto: **con** GPL);
  - `make SEM_GPL=1` (sin GPL);
  - `make SEM_FIRMWARE=1` (sin firmware binario).
  Cada combinación compila en su propia carpeta (`build/`, `build-sem-gpl/`…) para no mezclar
  objetos.
- **Aislamiento:**
  - todo lo GPL vive en `kernel/terceros/linux/` (mover allí las tablas de sensores, en
    `kernel/terceros/linux/hwmon/`);
  - el código propio que lo usa queda detrás de `#ifdef SAGUI_COM_GPL`, con una alternativa
    propia o una función desactivada.
- **Qué se pierde sin GPL** («descartando lo que no soporte»):
  - **r8169:** las revisiones modernas (8111G/H) no se inician; quedan «sin soporte en esta
    compilación» en `ip`/`infopc`. Las clásicas (la del dv7) siguen, si su inicio es propio
    (comprobarlo).
  - **Sensores:** sin las tablas, TjMax por CPUID/MSR cuando la CPU lo diga, y si no,
    «no disponible»; AMD sin los desvíos por modelo.
  - Lo que se añada en el futuro, igual: cada pieza GPL declara qué hace el sistema sin ella.
- **Identificación de la variante:**
  - la imagen lleva la variante en la cabecera;
  - `infopc`, el informe y `ajuda` muestran «Compilación: con GPL / sin GPL (permisiva) / sin
    firmware»;
  - en la variante con GPL, la imagen incluye el aviso de la GPLv2 y dónde está el código
    fuente.
- **Auditoría automática:** `tools/auditar_licencias.py` recorre los fuentes que entran en cada
  variante (con la lista real de objetos del Makefile) y **falla** si:
  - una variante «sin GPL» contiene un archivo con `SPDX-License-Identifier: GPL*`, o algo de
    `kernel/terceros/linux/`;
  - hay código de terceros sin fila en `docs/terceros.md`;
  - un archivo de `kernel/terceros/` no tiene cabecera SPDX.
- **Versiones:** `entrega/vN/` incluye las dos imágenes:
  - `saguios.img` (con GPL);
  - `saguios-sin-gpl.img`;
  más sus variantes persistente, ISO y vdi si interesa, y en `SHA256SUMS.txt` cada una
  etiquetada.

## Pruebas

- La **batería completa** pasa en las dos variantes. Las pruebas que dependen de lo GPL se
  saltan (con aviso) en la variante sin GPL, o comprueban el mensaje «sin soporte en esta
  compilación».
- `auditar_licencias.py` en el flujo de pruebas (una prueba `h7x_licencias.py`).
- En la variante sin GPL, comprobar que ningún símbolo ni cadena de `kernel/terceros/linux/`
  acaba en el binario (`nm`/`strings` del kernel).

## Criterios de aceptación

- [x] `make SEM_GPL=1` y `make SEM_FIRMWARE=1` funcionan y producen imágenes distintas (carpetas `build-sem-gpl/`, `build-sem-firmware/`; sin firmware real todavía)
- [x] La variante sin GPL no contiene nada GPL (auditoría y comprobación del binario: `tools/auditar_licencias.py`, h81)
- [x] Lo que se pierde sin GPL está documentado y se ve en `infopc`, el arranque y `ip` (D109, `docs/terceros.md`)
- [x] La batería normal pasa; con SEM_GPL un subconjunto (arranque, consola, e1000 h44, sensores h71, h81), ver D109
- [ ] `entrega/v5.1/` con las dos imágenes (`make entrega-sin-gpl` genera `build-sem-gpl/saguios-sin-gpl.img`; falta copiarlas al cerrar la v5.1)
- [x] Decisión registrada en `docs/decisoes.md` (D109; nota en la D073 pendiente al fusionar)

## Notas

- **Esto no resuelve la licencia del conjunto.** Sigue haciendo falta el permiso de Eli
  Rigobeli y M4R4G0N (rama `licencia`) antes de publicar o distribuir cualquiera de las dos
  variantes.
- Modelo sugerido: **Sonnet medio** (Makefile, `#ifdef`, auditoría y pruebas, bien acotado).
  La revisión de qué cuenta como GPL en las tablas de sensores la haría Opus o el mantenedor.

## Resultado (D109)

- Implementado: opciones del Makefile, `r8169_sem_gpl.h`, tablas de sensores en
  `kernel/terceros/linux/hwmon/`, `SAGUI_VARIANTE` (arranque e `infopc`), `tools/auditar_licencias.py`,
  `make entrega-sin-gpl`, `tests/h81_licencias.py` y `SAGUI_BUILD` en `tests/qemu.py`.
- Además, las tablas del MP3 ya no dependen de bibliotecas LGPL del anfitrión (`tools/dados/`).
- Sin hardware no se ha probado el r8169 sin GPL en una placa real (solo compila y arranca en QEMU).

> **Integración v5.1 (2026-11-08):** sin GPL tampoco entra el ajuste de los códecs IDT de HP
> (D107, `kernel/terceros/linux/hda/`): en el HP dv7 la variante sin GPL vuelve a sonar solo por
> el subwoofer. La auditoría lo detectó en h81.
