# 113 - Cliente de correo con interfaz al estilo de Outlook clásico

- **Estado:** hecha (**v8.3**, D289); pendiente de probar con una cuenta real
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** cliente de correo (082, D146), diálogos de archivo (103, D272), iconos (D135)

## Problema / motivación

El cliente de correo de la D146 tenía una lista de asuntos y un cuerpo, sin carpetas, sin saber qué mensajes estaban leídos
y sin responder ni reenviar. El mantenedor pidió una interfaz parecida a Outlook de Windows 98 / Office 97-2000, simplificada.

## Propuesta

- Tres paneles con separadores arrastrables: árbol de carpetas (Bandeja de entrada, Enviados, Borradores, Elementos eliminados y las
  carpetas IMAP de la cuenta, con los no leídos en negrita), lista de mensajes en columnas (leído, adjunto, De, Asunto, Recibido;
  ordenable con un clic en la cabecera) y vista previa con De, Para, Asunto y Fecha.
- Barra de herramientas con icono y texto (Nuevo, Responder, Resp. a todos, Reenviar, Eliminar, Enviar y recibir, Libreta, Buscar),
  menú (Archivo, Edición, Ver, Herramientas, Ayuda), menú contextual y barra de estado.
- Ventana de redacción con Para, CC, Asunto, adjuntos por el diálogo de archivos (D272), borradores y libreta de direcciones.
- Teclado: Supr, Ctrl+N, Ctrl+R, Ctrl+Mayús+R, Ctrl+F, Ctrl+E, F5, Intro; doble clic abre el mensaje en su ventana.

## Criterios de aceptación

- [x] Los tres paneles, la barra, el menú y la barra de estado (capturas `docs/capturas/H289-*.png`)
- [x] POP3, IMAP, SMTP, TLS y adjuntos de la D146 siguen funcionando (`tests/h118_correo.py`)
- [x] Test automático `tests/h289_correo_outlook.py`
- [x] Decisión D289 en `docs/decisoes.md`
- [ ] Prueba con una cuenta real

## Notas

Límites: sin hilos de conversación ni reglas; al reenviar no se reenvían los adjuntos; al reabrir un borrador no se recuperan sus adjuntos.
