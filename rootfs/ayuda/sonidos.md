# Sonidos del sistema

SaguiOS puede tocar un sonido en los momentos importantes: **arranque**, **apagado**, **reinicio**, **aviso**,
**error**, **pregunta** (confirmaciones), **conectar** y **desconectar un USB**, **batería baja** y **vaciar la
papelera**. Es **opcional y está apagado de serie**: se activa en **Configuración > Escritorio > Sonidos del sistema**
o con la orden `sonidos`. Sin tarjeta de sonido no hace nada.

## Configuración

- **Activar los sonidos del sistema**: el interruptor general.
- **Esquema**: «Ninguno» o uno de los instalados. Cada esquema es una carpeta con un `esquema.conf`
  (`evento=archivo`), sus archivos de sonido (WAV o MP3) y un `CREDITOS.txt`.
- **Volumen**: el de los sonidos del sistema (de 0 a 100 %, de 10 en 10), aparte del volumen general.
- **Eventos**: cada uno se activa o desactiva por separado y se prueba con **Probar**. Con los sonidos
  activados suenan todos menos **arranque, apagado y reinicio**, que se piden aparte (retrasan unos segundos).
- Se guarda por cuenta en `/home/.config/sonidos.conf`.

## Esquemas

| Esquema | De dónde | Licencia |
|---|---|---|
| **SaguiOS** | Generado por código (`tools/gerar_sonidos.py`): acordes y campanas sintetizadas | Generado por SaguiOS-Extended, GPL v3 |
| **Kenney** | «Interface Sounds» de Kenney (www.kenney.nl, https://kenney.nl/assets/interface-sounds) | CC0 (dominio público); créditos en `/sistema/sonidos/kenney/CREDITOS.txt` |

Los esquemas del sistema están en `/sistema/sonidos/<id>`. Quien compila con `INPUT_SHARE` puede tener además
**esquemas privados** en `/apps/sonidos/<id>` (jingles de arranque de Windows, sonidos de Pixabay): salen con
«(privado)» y **no se distribuyen** (ver `docs/decisoes.md`, D421).

## La orden `sonidos`

    sonidos                          estado: esquema, volumen y eventos
    sonidos --listar                 esquemas instalados
    sonidos --tocar aviso            toca el sonido de un evento (si está activado)
    sonidos --tocar error --siempre  lo toca aunque esté apagado (es lo que hace «Probar»)
    sonidos --activar | --desactivar
    sonidos --esquema kenney         elige el esquema («ninguno» lo calla)
    sonidos --volumen 80
    sonidos --evento usb_conectar no

Los programas de la libsagui lo usan con `sonido_evento("aviso")` (no bloquea) y
`sonido_evento_esperar("apagar", 3000)` (apagado y reinicio: espera hasta 3 s y nunca más).
El servicio **sonidos** (`/etc/servicios/sonidos.conf`) toca el de arranque y vigila los USB y la batería.
