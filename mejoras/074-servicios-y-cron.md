# 074 - Gestor de servicios (programas en segundo plano) y cron

- **Estado:** hecha en la rama `v7.3.1-servicios-cron` (**v7.3.1**, D216), **pendiente de compilar y de pasar la prueba `h216`**. Antes: propuesta (v6.3, decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media-alta
- **Alta:** 2026-11-08
- **Relacionadas:** lite-monkey (067), Python web (065), SSH (052), `/logs` (D120), apagado ACPI (D113), Administrador de tareas, Configuración (D112), cortafuegos (073)

## Motivación

Hoy un programa que debe quedarse funcionando, como `litemonkey` en el puerto 7979, un servidor
web o una sincronización de hora, se lanza a mano desde un terminal y muere al cerrarlo o al
reiniciar. Faltan dos piezas clásicas: un **gestor de servicios**, que arranca, vigila y
reinicia programas en segundo plano, y **cron**, que ejecuta tareas a horas fijas.

## 1. Gestor de servicios

- **Definición de un servicio:** un archivo de texto por servicio, en `/etc/servicios/` (los del
  sistema) y en `/home/.config/servicios/` (los del usuario, que se conservan si `/home` es
  persistente). Formato `clave=valor`:
  ```
  nombre=litemonkey
  descripcion=Base de datos lite-monkey (puerto 7979)
  comando=/bin/litemonkey --datos /home/litemonkey
  arrancar=si              # al iniciar el sistema
  reiniciar=fallo          # siempre | fallo | no
  espera=red,/home         # arrancar después de la red y del montaje de /home
  entorno=LMK_MEMORIA=64
  registro=/logs/servicios/litemonkey.log   # o /tmp si no hay /logs
  ```
- **Supervisor `servicios`:**
  - un proceso que el kernel lanza al arrancar, después de montar los discos y antes del
    escritorio, y que nunca debe morir. Si muere, el kernel lo relanza y lo anota;
  - arranca los servicios en orden de dependencias;
  - vigila su salida y los reinicia según la política, con espera creciente para que no se
    reinicie en bucle (1 s, 2 s, 4 s… hasta 1 min; tras 5 fallos seguidos se marca como
    «fallido»);
  - redirige su salida al registro con rotación;
  - al apagar o reiniciar (D113), los para ordenadamente antes de desmontar los discos: aviso,
    espera y, si no paran, se matan.
- **Lo que hace falta del kernel:**
  - lanzar procesos sin terminal y redirigir su salida a un archivo;
  - saber cuándo termina un hijo y con qué código (ya existe `esperar`);
  - avisar de parada antes de matar, con una señal o evento «terminar» que el programa puede
    atender para cerrar limpio.

  Las llamadas al sistema nuevas, si hacen falta, desde las libres.
- **Herramientas:**
  - terminal: `servicio lista`, `estado <nombre>`, `iniciar`, `parar`, `reiniciar`, `activar`
    (arranque automático), `desactivar`, `registro <nombre>`, `crear` (asistente) y `recargar`
    (relee las definiciones);
  - Administrador de tareas: pestaña **«Servicios»** con el estado (en marcha, parado,
    fallido), el PID, la memoria, el tiempo en marcha, los reinicios y los botones;
  - la bandeja avisa si un servicio queda «fallido».
- **Servicios de ejemplo:** `litemonkey`, `ntp` (hora al arrancar y cada día), un servidor HTTP
  de archivos y, cuando existan, Microdot y SSH.

## 2. cron

- `cron` es **un servicio más** del supervisor.
- **Formato clásico** de 5 campos (`minuto hora día mes día_semana comando`), con `*`, listas,
  rangos y pasos (`*/15`), y los atajos `@reboot`, `@hourly`, `@daily`, `@weekly` y `@monthly`.
- **Tablas:** `/etc/crontab` (sistema) y `/home/.config/crontab` (usuario). Las horas siguen la
  **zona horaria** configurada (D080).
- **Tareas perdidas:** si el PC estaba apagado a la hora de una tarea `@daily` o `@weekly`, se
  ejecuta una vez al arrancar, al estilo de anacron (opcional por línea).
- **Cada ejecución** se anota en `/logs/cron.log`: hora, comando, código de salida y duración.
  Su salida va al registro de la tarea.
- **Herramientas:**
  - `crontab -l` (ver), `crontab -e` (editar con el Editor o Mico) y `crontab -r` (borrar),
    validando la sintaxis al guardar;
  - `cron proximas` lista lo que se ejecutará y cuándo;
  - opcional: una pestaña «Tareas programadas» en Configuración.

## Pruebas

- Servicio que arranca al iniciar, muere y se reinicia con espera creciente; uno marcado
  «fallido» tras 5 fallos; dependencias (espera a la red y a `/home`); parada ordenada al
  apagar (QEMU termina y el servicio dejó su archivo cerrado); registro con rotación.
- Pestaña Servicios con capturas.
- cron:
  - sintaxis de los 5 campos con casos difíciles, probada en el anfitrión contra una
    referencia en Python;
  - en QEMU con el reloj adelantado o una hora simulada: tareas de cada minuto, `@reboot`,
    `@daily` perdida que se ejecuta al arrancar, y la zona horaria;
  - `crontab -e` con un error de sintaxis que se rechaza.

## Criterios de aceptación

- [x] Servicios del sistema y del usuario, arranque automático, reinicio con espera, parada ordenada
- [x] `servicio` en el terminal y pestaña Servicios en el Administrador de tareas
- [x] cron con formato clásico, atajos, zona horaria y tareas perdidas; `crontab -l/-e/-r`
- [x] Registros en `/logs` (o `/tmp`)
- [x] Tests automáticos en `tests/` (`h216_servicios_cron.py`, escrita; falta ejecutarla)
- [x] Decisión registrada en `docs/decisoes.md` (D216)

Lo que no se hizo (ver D216): `servicio crear` (asistente), la rotación del registro mientras el
servicio corre (solo al abrirlo), la pestaña «Tareas programadas» en Configuración y las reglas de
cortafuegos por servicio (073).

## Notas

- Modelo sugerido: **Opus** para el supervisor y lo que toque al kernel (procesos sin terminal,
  aviso de parada, apagado); **Sonnet** para cron, `crontab` y la pestaña de Servicios.
- Con el cortafuegos (073, v8), un servicio que escucha en un puerto puede declarar su regla en
  la definición (`puerto=7979/tcp red-local`).
