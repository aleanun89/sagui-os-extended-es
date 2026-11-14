# Atajos de teclado del escritorio

Los atrapa el gestor de ventanas antes que los programas, así que funcionan en cualquier
ventana.

| Atajo | Qué hace |
| --- | --- |
| **Alt+Tab** | Cambia de ventana. Un toque rápido alterna entre las dos últimas. |
| **Alt+Mayús+Tab** | Lo mismo, hacia atrás. |
| **Alt+F4** | Cierra la ventana activa, igual que la X: si hay cambios sin guardar, el programa pregunta. |
| **Windows** | Sola (pulsar y soltar), abre o cierra el menú Inicio. También **Ctrl+Esc**. |
| **Windows+D** | Minimiza todas las ventanas y muestra el escritorio. Repetido, las restaura. |
| **Ctrl+Alt+Supr** | Abre el Administrador de tareas (o lo trae al frente). También **Ctrl+Mayús+Esc**. |
| **F11** | Pantalla completa, si el programa la admite. **Esc** sale de ella. |

## Alt+Tab

Mientras se mantiene **Alt** apretada aparece un panel en el centro de la pantalla con el icono
de cada ventana, ordenadas por uso (la actual primero; las minimizadas al final, con una raya
gris). Cada **Tab** avanza (también las flechas), **Mayús+Tab** retrocede y **Esc** cancela. Al
soltar Alt se activa la ventana elegida; si estaba minimizada, se restaura.

## Capturas de pantalla (D125)

- **Impr Pant**: toda la pantalla, en `/home/capturas/captura-AAAAMMDD-HHMMSS.png` (o en
  `/tmp/capturas` si /home no deja escribir), con un aviso.
- **Alt+Impr Pant**: sólo la ventana activa, con su marco.
- **Windows+Mayús+S**: recorte. La pantalla se congela: arrastre un rectángulo, haga clic en
  una ventana o pulse Intro para toda la pantalla; Esc cancela. Después, Guardar, Copiar,
  Abrir en Paint o Nuevo recorte. «Copiar» deja la imagen en el portapapeles (Ctrl+V en Paint).
  Pulsar el aviso «Captura guardada» abre la imagen en el Visor. La carpeta y el clic de sonido se
  eligen en Configuración, pestaña Capturas.

En el terminal: `captura [archivo] [-v] [-r x,y,ancho,alto] [-t segundos]`.

## La tecla Windows

El menú se abre al **soltar** la tecla, y sólo si no se pulsó otra mientras tanto: así
**Windows+D** no abre el menú. Las demás combinaciones con Windows están reservadas para más
adelante y no llegan a los programas.

## AltGr

**AltGr** (Alt de la derecha) no es Alt: sigue escribiendo **@ # | \ [ ] { } ~ ¬** y no
dispara Alt+Tab ni Alt+F4.

## El Terminal: pestañas y preferencias (D231)

Estos atajos los trata el propio Terminal (no el gestor de ventanas). Con una sola pestaña no
se ve la barra de pestañas; aparece con la segunda (o siempre, en Preferencias).

| Atajo | Qué hace |
| --- | --- |
| **Ctrl+Mayús+T** | Pestaña nueva (un `sagsh` nuevo; hasta 8). También el botón **+**. |
| **Ctrl+Mayús+W** | Cierra la pestaña (avisa si hay una orden en marcha). También la **x** y el botón del medio. Al cerrar la última, se cierra la ventana. |
| **Ctrl+RePág / Ctrl+AvPág**, **Ctrl+Tab** | Pestaña anterior o siguiente. |
| **Ctrl+1 ... Ctrl+9** | Va a esa pestaña (Ctrl+9, a la última). También el clic. |
| **Ctrl+Mayús+C / Ctrl+Mayús+V** | Copiar y pegar (Ctrl+C sigue interrumpiendo). Arrastrar selecciona; doble clic, una palabra. |
| **Ctrl+Mayús+F** | Buscar en el historial. Intro: la aparición anterior; Mayús+Intro: la siguiente; Esc cierra. |
| **Ctrl+más / Ctrl+menos / Ctrl+0** | Ampliar, reducir y restablecer la letra (con una fuente vectorial monoespaciada). |
| **Ctrl+,** | Preferencias. |
| **Ctrl+clic** en un `http://...` | Abre el enlace en el Cipó. |
| **Botón derecho** | Menú: Limpiar, Copiar, Pegar, Seleccionar todo, Buscar, Nueva pestaña, Nueva ventana, Preferencias, Cerrar pestaña. |

El título de cada pestaña es la carpeta del indicador del shell o, mientras corre una orden,
su nombre. **Preferencias** guarda en `/home/.config/terminal.conf` (texto `clave=valor`, se
puede editar a mano): tema (Oscuro, Claro, Verde fósforo, Ámbar, Mar, Papel o Personalizado con los
16 colores ANSI), fuente (la interna de 8x16 o una vectorial monoespaciada de `/apps/fuentes`, con su
tamaño), columnas y filas iniciales, líneas de historial, forma y parpadeo del cursor y campana
(ninguna, visual o sonora). El Terminal entiende colores ANSI (`ESC [ 31 m`...), cursor
(`ESC [ f;c H`...) y borrado.
