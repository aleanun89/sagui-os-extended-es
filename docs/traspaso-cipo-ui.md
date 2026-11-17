# Traspaso: Cipó moderno (D373, rama v8.5.3-cipo-ui)

## Hecho (compila; commits en la rama)
- `userland/programas/cipo.c` (ahora todo UTF-8; `tools/extraer_textos.py` ya no lo trata como Latin-1):
  - Pestañas: favicon (link rel=icon o /favicon.ico; PNG, ICO con PNG o DIB 24/32), icono genérico, título con «...», × por pestaña,
    «+», activa unida a la barra, clic central cierra, arrastrar reordena (`mover_aba`), menú contextual (recargar/duplicar/cerrar/cerrar otras),
    spinner de carga, ABAS_MAX 16 con desplazamiento (rueda sobre la tira cambia de pestaña).
  - Barra: iconos propios (atrás, adelante, recargar/parar, inicio), candado con info del certificado (`cert_resumo`), sugerencias
    de marcadores e historial, estrella, menú ≡, barra de marcadores opcional, la dirección interpreta búsquedas (`interpretar_entrada`).
  - Marcadores con carpetas (3.er campo del fichero), exportar/importar HTML Netscape (`marcadores_exportar/importar`), gestor (panel).
  - Página de inicio propia (`inicio_gerar`: buscador, accesos rápidos), en blanco o URL; `url_inicio()`.
  - Ajustes en `~/.config/cipo/cipo.conf` (`ler_conf/gravar_conf`): panel General/Privacidad/Rendimiento: inicio, buscador, carpeta de descargas,
    barra de marcadores, bloqueo DNS (activar/desactivar con /bin/bloqueo), JS, imágenes, descargas simultáneas 1-12
    (`hilos_efectivos` recorta por memoria libre y el panel lo dice).
  - Descargas simultáneas reales: `pf_lote/pf_trabalhador/pf_pegar` (hilos solo bajan bytes; TLS serializada con `tls_trava`),
    `motor_urls_pendentes` nuevo en `userland/web/motor.c/.h`. Solo imágenes/fuentes web; CSS y JS siguen en serie.
  - Gestor de descargas (hilos, pausa, cancelar, abrir carpeta), historial persistente + panel con búsqueda (Ctrl+H), Ctrl+J, Ctrl+Mayús+B.
  - `--hilos N` en la línea de órdenes (solo esa ejecución).
- `tests/h47_cipo.py`: coordenadas de la barra actualizadas. `tests/h373_cipo_ui.py` (en curso).

## Estado (actualizado)
- Mergeado main (38feaaae). `tests/h373_cipo_ui.py` pasa entera (ui + 1/4/12 hilos; 7800/2080/870 ms). Pasan h47 (adaptada), h82, h112, h113, h114, h121, h123, h124, h128, h271, h349, h132.
- Hecho: catálogo pt_BR/en (`comprobar cipo` = 0), D373 en docs/decisoes.md, docs/cipo.md.
- Arreglos de esta tanda: los paneles dejan pasar EV_SOLTOU a botao_tratar; `formatar_texto` no admite `%.N`/`%.*s`; recursos de la página siguen con el panel abierto.

## Pendiente
1. Zoom (Ctrl +/-/0), user-agent y modo de compatibilidad (el motor/`libsagui/http.c` no los admiten; hace falta tocarlos).
2. CSS y JS en paralelo (hoy solo imágenes y fuentes web).
3. Hilos por defecto = 1; decidir si subir.
4. Ejecutar la prueba del catálogo/idiomas global (p. ej. tests de h313/h318 si cuentan catálogos) y la reducida antes de publicar.
5. Revisar `git status` por capturas regeneradas ajenas antes de subir.
