# Energía y pantalla («energia»)

SaguiOS cuida la pantalla y la batería cuando no tocas el equipo (D422). Todo se configura en
**Configuración > Sistema > Energía y pantalla** y es de la máquina: se guarda en `/etc/energia.conf` y
cambiarlo pide permisos de administrador.

## Qué hace, por orden, al pasar los minutos sin tocar teclado ni ratón

1. **Salvapantallas** (apagado de serie): *Estrellas*, *Logotipo SaguiOS* rebotando, *Tuberías*, *Reloj* (cambia de
   sitio cada minuto) o *Pantalla negra*. Los dibuja el sistema por código, sin imágenes: son ligeros y valen
   para equipos antiguos. Empieza tras 1-60 minutos. La vista previa de Configuración lo enseña en vivo, y
   **Probar ahora** lo pone a pantalla completa.
2. **Bloqueo de pantalla** (apagado de serie): «Bloquear tras N minutos» o «Al salir del salvapantallas».
   **Solo bloquea si la cuenta tiene contraseña**: sin contraseña no hay nada que pedir para volver y la
   pantalla no se bloquea (como Windows+L sin contraseña, que solo la cubre).
3. **Apagar la pantalla**: en negro, tras N minutos distintos con batería (10 de serie) y enchufado (30), o
   nunca (0).

La primera tecla o el primer movimiento del ratón **solo despiertan**: no llegan al programa que tengas
detrás. Con el bloqueo puesto hay que escribir la contraseña de la cuenta, como siempre (Windows+L).

## No molestar

Mientras un programa lo pide, o hay una ventana a pantalla completa (F11), **la cuenta no avanza**: ni salvapantallas,
ni bloqueo, ni apagado. Un programa propio lo pide así (se suelta solo si el programa acaba):

    #include "energia.h"
    no_molestar(1);   /* al empezar el vídeo o la partida */
    ...
    no_molestar(0);   /* al acabar */

## Batería

Con una batería detectada por ACPI: **aviso de batería baja** (15 % de serie) y **crítica** (5 %), y la
opción **apagar el equipo al llegar a crítico** (apagado ordenado; avisa 30 s antes y se cancela si enchufas el
cargador). Además, el **perfil de la CPU** (rendimiento, ahorro, a demanda o sin cambiar) puede ser distinto con
batería y enchufado: cambia solo al conectar o desconectar el cargador (necesita el driver de frecuencia,
ver `frecuencia`).

## La orden `energia`

| Orden | Qué hace |
| --- | --- |
| `energia` | Estado: inactividad, qué está en marcha, la configuración que lee el kernel y la batería. |
| `energia recargar` | Vuelve a leer `/etc/energia.conf` (tras editarlo a mano). |
| `energia probar [N]` | Muestra ya el salvapantallas N (0 negro, 1 estrellas, 2 logotipo, 3 tuberías, 4 reloj). |
| `energia actividad` | Como una tecla: despierta y reinicia la cuenta. |
| `energia nomolestar SEG` | Pide «no molestar» durante SEG segundos (para probarlo). |
| `energia simular SEG`, `energia bateria PCT [enchufada]` | Solo administrador, para las pruebas. |

## `/etc/energia.conf`

    salvapantallas=0          # 0 apagado, 1 estrellas, 2 logotipo, 3 tuberías, 4 reloj
    salvapantallas_min=10     # 1-60
    bloqueo=0                 # 0 nunca, 1 tras bloqueo_min, 2 al salir del salvapantallas
    bloqueo_min=15
    pantalla_bateria_min=10   # 0 = nunca
    pantalla_red_min=30
    bateria_aviso=15          # 0 = no avisar
    bateria_critica=5
    bateria_accion=0          # 1 = apagar al llegar a crítico
    gobernador_bateria=3      # 0 rendimiento, 1 ahorro, 2 a demanda, 3 sin cambiar
    gobernador_red=3

## Límites

- La pantalla «apagada» es negra: SaguiOS no llama a la BIOS (VBE/PM, función 10h) ni toca el
  brillo del panel, así que el monitor no entra en ahorro de verdad. Queda anotado para más adelante.
- No hay suspensión (S3) ni hibernación todavía.
- El salvapantallas de fotos de una carpeta no está (haría falta decodificar imágenes en el kernel).
