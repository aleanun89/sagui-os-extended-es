# Estilos de apariencia

Un **estilo** (o esquema de apariencia) cambia a la vez los colores de todo el sistema: las ventanas y sus
títulos, los menús, los botones, los campos y las listas, la selección, las barras de desplazamiento, la barra de
tareas, el menú Inicio y el escritorio. Se elige en **Configuración > Escritorio > Estilos** y se aplica al
momento: las ventanas abiertas se repintan solas.

## Estilos incluidos

| Estilo | Cómo es |
|---|---|
| **SaguiOS** | El de siempre (verde y turquesa). Es el de serie; aquí manda el color de acento que se elige en *Colores y fondo*. |
| **SaguiOS clásico** | Gris con biseles y títulos azules, a la manera de los escritorios de los años noventa. |
| **Desierto** | Arena, ocre y marrón; títulos marrones con degradado. |
| **Berenjena** | Violetas oscuros y claros. |
| **Pizarra** | Grises azulados. |
| **Marino** | Azules marinos. |
| **Ladrillo** | Rojos de ladrillo y arcilla. |
| **Lluvia** | Grises azules claros, con los títulos claros y el texto oscuro. |
| **Bosque** | Verdes de bosque. |
| **Alto contraste negro** | Fondo negro, texto blanco y selección amarilla: contraste de 7:1 o más (accesibilidad). |
| **Alto contraste blanco** | Fondo blanco, texto negro y selección azul marino: contraste de 7:1 o más. |
| **Oscuro** | Fondos oscuros y texto claro; los iconos de trazo oscuro se aclaran solos. |

Cada estilo trae su propio acento. En **SaguiOS** el acento elegido en *Colores y fondo* es el que se ve; en los
demás estilos, elegir otro acento recolorea también los títulos y el estilo pasa a ser personalizado.

Más estilos: **Arce** (otoñal), **Lavanda**, **Océano**, **Cereza**, **Grafito** (oscuro con acento naranja), **Nórdico**
(azul grisáceo claro), **Terminal** (verde fósforo sobre negro), **Solarizado claro**, **Solarizado oscuro** y **Alto contraste amarillo**
(amarillo sobre negro).

## Fuente del sistema

Bajo la miniatura se elige la fuente de mapa de bits que usan el kernel (títulos, menús, barra, avisos) y la libsagui: **Normal (SaguiOS)**,
**Terminus**, **Terminus negrita** y **Fixed** (todas 8x16 con acentos, ñ y ç). Se aplica en vivo, se guarda por cuenta y cambiar de estilo no la
toca. Desde el terminal: `configuracion estilo fuente 0-3`.

## Vista previa y cambio en vivo

A la derecha de la lista hay una **miniatura** con una ventana activa, otra inactiva, un menú abierto, un botón y
un campo dibujados con el estilo elegido. Al pulsar un estilo de la lista se aplica en vivo y se recuerda para la
próxima vez que entres (se guarda por cuenta, en `/home/.config/estilo.conf`). **Cancelar** devuelve el que había al
abrir Configuración; **Predeterminado** vuelve a «SaguiOS».

## Personalizar

**Personalizar...** abre una lista de elementos (el escritorio arriba y abajo, el título activo con sus dos
colores del degradado, el texto del título, el marco de la ventana, la ventana, el texto, los controles, el campo, la
selección, el menú y su resaltado, la barra de tareas, el acento, la información o tooltip, el texto
deshabilitado y la barra de desplazamiento). Elige uno, pulsa **Cambiar color...** (el selector de color de siempre) y
se aplica al momento. Para quedarte con el resultado, escribe un **nombre** y pulsa **Guardar estilo**: aparece en la
lista (con un asterisco) y se guarda en `/home/.config/estilos.lista`. **Borrar** quita uno guardado.

## Pantalla de inicio de sesión

**Usar en el inicio de sesión** guarda el estilo actual en `/etc/estilo.conf` (pide permisos de administrador); es el
que se ve antes de entrar y al cerrar la sesión. Cada cuenta recupera el suyo al entrar.

## Desde el terminal

```
configuracion estilo                       # el estilo vigente
configuracion estilo lista                 # los incluidos y los guardados
configuracion estilo desierto              # aplica un estilo incluido (clave en minúsculas) o uno guardado
configuracion estilo color 19 123456       # cambia el elemento 19 (barra de tareas) a 123456 (0-25, como en la lista)
configuracion estilo guardar "Mi estilo"   # guarda el vigente con ese nombre
configuracion estilo inicio                # lo usa también en la pantalla de inicio de sesión
```

## Para programadores

Los colores viven en una sola estructura (`esquema_t`, `kernel/inc/esquemas.h`): el kernel guarda el vigente
(`SIS_ESQUEMA`) y la libsagui lo lee (`sagui_esq`): `COR_FUNDO`, `COR_TEXTO`, `COR_SELECAO`, `W98_FACE`... y `mod_tema`
salen de él. Al cambiar el estilo, las ventanas reciben el evento `EV_TEMA`: la libsagui ya ha recargado los colores,
y el programa solo tiene que repintarse (lo más sencillo es registrar su función de repintado con
`tema_ao_mudar(f)`). Los programas que no lo atienden cogen el estilo nuevo al abrirse.
