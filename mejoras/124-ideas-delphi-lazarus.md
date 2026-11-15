# 124 - Componentes y utilidades inspirados en Delphi/Lazarus (DCPcrypt, pascalutils, awesome-pascal)

- **Estado:** aprobada (**v8.3.2**, decidido por el mantenedor el 2026-11-15)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** VisualPythonS (123, fase 2), Pluma (116), Macaco (117), correo (113), Mico (043), TLS (libtls)

## Origen
El mantenedor señaló tres proyectos libres del mundo Pascal como fuente de **ideas** (no se trae su código):
DCPcrypt para Lazarus (cifrados, hashes, cifrado de flujos), pascalutils (tipos y utilidades) y la lista
awesome-delphi/awesome-pascal (VirtualTreeView, SynEdit, HtmlViewer, Graphics32/AggPas, Vampyre Imaging,
Report Manager, ZeosLib...). Todo se escribe desde cero o con lo que ya hay en el sistema.

## Propuesta (por prioridad)
1. **Cifrar archivos y carpetas** («Cifrar...» en Archivos, órdenes `cifrar`/`descifrar`): AES-256 de la libtls,
   clave derivada de la contraseña (PBKDF2 o similar con sal), IV aleatorio, autenticación (GCM o HMAC), barra de
   progreso. Sin criptografía nueva escrita a mano. **Revisión de Opus** (seguridad).
2. **Árbol y tabla virtuales** en la libsagui (solo se dibuja y se guarda lo visible): quitar el límite de 400
   mensajes por carpeta del correo y acelerar Archivos con carpetas enormes.
3. **Comprobar sumas** (SHA-256/SHA-512) en Propiedades de Archivos.
4. **Editor de código con resaltado como componente** de la libsagui (sacado de Mico): para Mico, VisualPythonS y el Editor.
5. **Visor HTML ligero** como componente: ayuda de las aplicaciones y correo HTML.
6. **Dibujo 2D suavizado** (líneas, curvas de Bézier, degradados, antialias) en la libsagui: gráficos de Macaco,
   WordArt más fino y base para fuentes escalables.
7. **Más formatos de imagen** (TGA, PCX, ICO, TIFF básico).
8. **Diseñador de informes sencillo** que imprime a PDF (datos de Macaco o SQLite).
9. **Tamaños y duraciones legibles** comunes en la libsagui («1,5 GB», «2 h 3 min»).

## VisualPythonS, fase 2 (mejora 123)
Rejilla de datos enlazada a SQLite/CSV con barra de navegación; componentes no visuales (Base de datos,
Configuración, Descargar HTTP, Cifrado/Hash, Sonido, Correo, **Agente IA**); controles ricos (pestañas, árbol,
texto con formato de Pluma, gráfico, visor HTML); autocompletado de propiedades en el editor; imprimir a PDF;
componentes de terceros que aparecen en la caja de herramientas.

## Criterios de aceptación
- [ ] Cifrar y descifrar archivos desde Archivos y el terminal, revisado por Opus
- [ ] Tabla virtual usada por el correo (sin límite de 400) y por Archivos
- [ ] Al menos los puntos 3, 4 y 9; el resto según el tiempo
- [ ] Fase 2 de VisualPythonS con la rejilla de datos y los componentes no visuales
- [ ] Tests automáticos y decisiones en `docs/decisoes.md`
