# 082 - Cliente de correo: POP3, SMTP e IMAP, con ventana, adjuntos y HTML

- **Estado:** en curso
- **Prioridad:** alta
- **Alta:** 2026-11-11
- **Relacionadas:** 043 (el cliente de IA), 048 (el vídeo), 081 (el navegador), D146 (el cliente de correo)

## Problema / motivation

El SaguiOS tenía red (el TLS propio, el HTTP del Cipó, el cliente de IA) pero
**nada para el correo**. En una máquina de 32 bits sin libc, con la pila de 64 KB
y sin coma flotante, escribir un cliente de correo de verdad no es trivial: son
tres protocolos de líneas, con autenticación, con mensajes que traen adjuntos
codificados y con cabeceras que a veces no son ASCII.

Sin correo, el sistema no puede pedir una contraseña, avisar de una alarma ni
mandar un parte.

## Propuesta

Un programa de userland, `correo`, con **ventana**, que recibe por POP3 o IMAP
y envía por SMTP. Vive en:

- `userland/inc/correo.h`: el contrato de la biblioteca (sesión, protocolos y MIME);
- `userland/mail/protocolo.c`: la sesión de red (TCP y TLS) y los tres protocolos;
- `userland/mail/mime.c`: base64, quoted-printable, HTML a texto y el analizador
  y el constructor de mensajes MIME;
- `userland/programas/correo.c`: la interfaz.

### Las tres ventanas

| Ventana | Qué hay |
| --- | --- |
| **Correo** | la lista de los mensajes (con `*` si tienen adjunto), el cuerpo del que está elegido y tres botones: «Recibir», «Redactar» y «Cuenta» |
| **Redactar** | destinatario, asunto, cuerpo, los adjuntos y los botones «Adjuntar», «Enviar» y «Cancelar» |
| **Cuenta de correo** | servidor, puerto de recepción, puerto de envío, usuario, clave (con asteriscos), protocolo (POP3/IMAP) y TLS |

Está en el **menú Inicio**, en el grupo «Internet», con su icono de envelope
(`arte_mini_correo`), como el Cipó.

### Los protocolos, de verdad

- **SMTP**: `EHLO`, `STARTTLS` (solo si el servidor lo anuncia; si lo rechaza,
  se sigue en claro, como hacen los clientes de verdad), `AUTH LOGIN`,
  `MAIL FROM`, `RCPT TO`, `DATA` con el punto de transparencia (RFC 5321) y
  `QUIT`.
- **POP3**: `USER`, `PASS`, `STAT`, `RETR` (con el punto de transparencia del
  lado del servidor) y `QUIT`. Todos los mensajes se traen en una sola sesión
  y se entregan de uno en uno, para no tener el buzón entero en memoria.
- **IMAP**: `LOGIN`, `SELECT` (el `* n EXISTS` da el número de mensajes) y
  `FETCH n (BODY.PEEK[])` de cada uno, leyendo el literal `{N}` que va detrás.

### MIME

- **base64** (RFC 4648) para codificar y decodificar: los adjuntos van así.
- **quoted-printable** (RFC 2045) para decodificar.
- **multipart/alternative**: si el mensaje trae texto plano y HTML, sale el
  texto plano.
- **multipart/mixed**: los adjuntos se separan del cuerpo y se listan aparte.
- **HTML a texto**: las etiquetas de bloque dan saltos de línea y las entidades
  comunes (`&amp;`, `&lt;`, `&nbsp;`…) se convierten; el `<script>` y el
  `<style>` se ignoran.
- **RFC 2047**: un asunto que no es ASCII se manda como
  `=?UTF-8?B?...?=`.

### Los mensajes se guardan

Cada mensaje recibido se guarda en `/home/correo/mNNNN.eml`, uno por mensaje.
Así se pueden volver a ver, y la lista los relee de ahí (con el remitente y el
asunto). Sin disco persistente, `/home` está en la capa de RAM: los mensajes
se pierden al reiniciar, y el programa lo avisa.

### La cuenta

Se guarda en `/home/.config/correo`, en líneas `clave=valor`: `host`, `porta`,
`porta_envio`, `utilizador`, `clave`, `proto` y `tls`. **La clave nunca se
escribe en el repositorio ni en la imagen**: la ventana la enseña con
asteriscos (el campo `oculta` de la `libsagui`).

El puerto de recepción y el de envío **son distintos**: 110 o 143 para
recibir, 587 o 25 para enviar. Por eso hay dos casillas.

### La línea de comandos

```
correo [--proto pop3|imap] [--host H] [--porta P] [--porta-envio P]
       [--utilizador U] [--clave C] [--tls]
```

Sin argumentos, usa la cuenta de `/home/.config/correo`.

## Criterios de aceptación

- [x] Recibe por POP3 y por IMAP, y envía por SMTP.
- [x] Ventana con la lista de mensajes y el cuerpo.
- [x] Adjuntar un fichero y que llegue entero al servidor.
- [x] Cuerpo en texto plano y en HTML (el HTML se ve como texto).
- [x] La cuenta se guarda con la clave escondida.
- [x] Test automático en `tests/` que lo comprueba (`tests/h118_correo.py`).
- [x] Decisión registrada en `docs/decisoes.md` (D146).

## Notas

- El texto de la red llega en UTF-8 y se convierte a Latin-1 para la pantalla;
  al enviar, se convierte de vuelta.
- El `STARTTLS` solo se pide si el servidor lo anuncia en el `EHLO`, y si lo
  rechaza se continúa en claro: hay servidores de los puertos 25 que no lo
  ofrecen.
- La ventana no tiene OCR en las pruebas: los rótulos no caen en la rejilla de
  8x16 del reconocedor. Lo que se comprueba es lo que se puede comprobar de
  verdad: lo que llega al servidor falso, lo que queda en el disco y lo que el
  programa anota en el registro del sistema (la serie).
