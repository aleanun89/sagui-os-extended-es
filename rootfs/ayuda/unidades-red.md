# Unidades de red (WebDAV)

Una **unidad de red** es una carpeta de un servidor WebDAV (Nextcloud, ownCloud, un NAS, `rclone serve webdav`...) que se ve en
SaguiOS como **`/red/<nombre>`**. Las aplicaciones abren y guardan en ella como en cualquier carpeta.

- Es **de tu cuenta**: otra cuenta no la ve ni puede entrar (root sí).
- **Desde ahí no se ejecuta nada**: ni programas ni bibliotecas (lo impide el kernel). Copia el fichero fuera si lo necesitas.
- Se ve si hay **conexión**: en Archivos (sección RED de la barra lateral), en los diálogos de abrir y guardar y con `dav estado`.
  Sin conexión, abrir o listar da error enseguida (no se queda colgado); cuando vuelve, la unidad se reconecta sola y avisa.
- Lo que guardas se sube al **cerrar** el fichero. Si en ese momento no hay conexión, se sube al volver; si la unidad se desmonta
  antes, queda una copia en `~/.cache/dav-pendientes/<unidad>/`.

## Desde Configuración o Archivos

**Configuración > Unidades de red** (o **Archivos > Conectar unidad de red…**): nombre, dirección (`https://servidor/remote.php/dav/files/ana/`),
usuario, contraseña y «conectar al iniciar la sesión». **Probar** comprueba la conexión antes de guardar.

## Desde la terminal

    dav agregar nube https://servidor/dav/ ana     # pide la contraseña y la guarda cifrada
    dav agregar casa http://nas.local/dav/ ana --auto          # se monta al entrar en la sesión
    dav agregar propia https://10.0.0.5/dav/ ana --cacert /home/mi-ca.pem   # servidor con certificado propio
    dav montar nube          # /red/nube
    dav estado               # las montadas: conectada o desconectada
    dav lista                # las que tienes configuradas
    dav probar nube          # ¿conecta y acepta el usuario?
    dav desmontar nube
    dav borrar nube          # también quita su contraseña del llavero

Con `http://` la contraseña viaja sin cifrar por la red: usa `https://` salvo en tu red de casa. `--inseguro` no comprueba el certificado
(desaconsejado).

## El llavero

Las contraseñas no se guardan nunca en claro: van al **llavero** de tu cuenta (`~/.config/llavero`, solo tuyo), cifrado con
AES-256-GCM con una clave que sale de **la contraseña de tu cuenta** y que solo está en la memoria mientras dura la sesión.

- Si tu cuenta **no tiene contraseña**, el llavero está cifrado pero **protege poco** (quien copie el fichero lo abre). Ponle una
  contraseña propia: `dav llavero contrasena` (después, una vez por arranque, `dav llavero abrir`).
- Si cambias la contraseña de la cuenta, el llavero pide la anterior la primera vez (`dav llavero abrir`) y se vuelve a cifrar.
- `dav llavero estado` dice cómo está; `dav llavero cerrar` lo cierra hasta que lo abras otra vez.

Más detalles: decisión D485.
