# 063 - Mejoras del navegador Cipó

- **Estado:** hecha en la rama `navegador` (D110); quedan los puntos de «Pendiente»
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 018 (ventanas redimensionables), 047 (JPEG), 049 (HMKD en Cipó), D047

## Problema / motivación

El Cipó (D047) tenía una ventana fija de 860x600 que no se podía maximizar, sin «Adelante»,
sin rueda del ratón ni búsqueda, y no mostraba imágenes.

## Hecho (D110, `tests/h82_cipo_ventana.py`)

- Ventana redimensionable y maximizable: se vuelve a maquetar al nuevo ancho conservando la
  posición de lectura; barra, pie y desplazamiento se adaptan; mínimo 560x300.
- Atrás / Adelante (botones y Alt+Izquierda/Derecha), Recargar (F5, Ctrl+R), Inicio (botón y
  Alt+Inicio), Ctrl+L, título de la ventana y URL del enlace bajo el ratón en el pie.
- Rueda, AvPág/RePág, Inicio/Fin, flechas y barra con arrastre.
- Buscar en la página: Ctrl+F, F3 / Mayús+F3, resaltado de todas las coincidencias.
- Tab / Mayús+Tab entre enlaces y campos, foco visible, Intro abre.
- Imágenes `<img>` PNG, BMP y PPM: descargadas, escaladas al ancho disponible, con límites;
  si fallan, el texto `[alt]`.

## Pendiente (otra versión)

- Imágenes JPEG (cuando exista 047), GIF y SVG; usar `width`/`height` del HTML; descarga de
  imágenes sin bloquear la interfaz.
- Caché de páginas y posición de lectura al volver atrás; pestañas; marcadores.
- Selección y copia de texto; búsqueda que ignore las tildes.
- CSS básico (colores, `<center>`), tablas con columnas de verdad.
- Que el HMKD (049) se muestre aquí.

## Feito na v6.3 (D148)

A lista de «Pendiente» ficou assim (a do motor da web, D141, resolve o CSS e as
tabelas com colunas de verdade):

- **Imagens**: o JPEG e o PNG são lidos pelo motor e o `width`/`height` do HTML
  manda no tamanho da caixa. Uma imagem grande já não é deitada fora: é
  **reduzida** para 800x600 (mais do que a janela mostra) e o limite de
  descarga sobe para 4 MB. O GIF continua sem haver: não há descodificador.
- **Abas, cache e posição de leitura**: barra de abas (Ctrl+T, Ctrl+W, Ctrl+Tab
  ou um clique), cada aba com a sua página em memória — trocar de aba não
  volta a descarregar nada — e, ao voltar atrás, a página volta à posição em
  que se estava a ler.
- **Marcadores**: Ctrl+D guarda a página, Ctrl+B abre a lista.
- **Copiar**: Ctrl+C copia o link que está debaixo do rato, o texto do campo
  que tem o foco, o endereço, ou o texto todo da página, para a área de
  transferência do sistema.
- **Busca sem acentos**: «cancion» encontra «canción».

Fica de fora: a **seleção livre** de texto com o rato (o motor não dá a posição
do texto, só a dos links e das imagens) e o **HMKD** (049).
