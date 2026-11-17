# VPN WireGuard en SaguiOS (D363, D372)

SaguiOS habla el protocolo WireGuard oficial (Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s): se
conecta a un servidor Linux, a un router doméstico (OpenWrt, FRITZ!Box, MikroTik...), a un
VPS o a cualquier `wireguard-go`/`boringtun`. Solo IPv4 dentro y fuera del túnel (la pila
de SaguiOS no tiene IPv6: lo de IPv6 del archivo se ignora con un aviso).

## Uso rápido

```sh
wg genkey > /home/clave          # la clave privada (guárdala: chmod 600)
wg pubkey < /home/clave          # la pública: se la das al servidor
wg-quick up casa                 # /home/.config/wireguard/casa.conf (pide permiso de administrador)
wg show                          # estado: último saludo, bytes, extremo, descartados
wg-quick down                    # apaga el túnel
wg-quick check casa              # solo comprueba el archivo
```

`wg-quick up` acepta también una ruta (`wg-quick up /media/pen/casa.conf`). El nombre de
la interfaz es el del archivo sin `.conf` (como en Linux), hasta 15 letras.

En el escritorio: **Configuración > Red > VPN (WireGuard)...** abre la ventana de la VPN:
lista de configuraciones, *Conectar*/*Desconectar*, *Importar .conf...* (lo copia a
`/home/.config/wireguard` con permisos 600), *Borrar*, y *Generar claves* (enseña la
pública con *Copiar*; si luego importas un `.conf` sin `PrivateKey`, se le pone esa). Con
el túnel encendido sale un candado en la bandeja (verde con saludo, gris esperando al
servidor); el clic abre la misma ventana. Es `wg --ventana`.

**Permisos (D372):** encender una VPN desvía todo el tráfico y el DNS del sistema, así que
**solo se enciende con permiso de administrador, cada vez**:

- En el escritorio, *Conectar* (o `wg-quick up` en el Terminal) abre el diálogo de permisos del
  sistema («Control de permisos del sistema», D269): la contraseña de root o, si eres del grupo
  admin, la tuya. Este permiso **no se recuerda** 5 minutos como el de otros programas: cada
  encendido pregunta.
- Sin escritorio (consola, SSH), `wg-quick up` pide la contraseña en el terminal, como `admin`.
  `admin wg-quick up RUTA` también vale (el archivo lo lee entonces root: da la ruta completa).
- **Apagar no pide nada:** `wg-quick down`, *Desconectar* o el candado de la bandeja los puede
  usar la sesión local sin elevar (apagar devuelve el tráfico a la red física). Un servicio o
  una sesión SSH que no sea de root no puede ni encender ni apagar.
- Generar claves, importar, borrar y `wg-quick check` no necesitan permiso: son archivos tuyos.

Cómo funciona: `wg-quick` lee el `.conf` **con tus permisos** (antes de pedir el permiso), lo
comprueba y se lo pasa entero al demonio (`wg --demonio NOMBRE`), que corre como root, por una
tubería. El demonio no abre ninguna ruta: root nunca lee un archivo que haya elegido otro
usuario. El archivo puede tener como mucho 16 KB de texto. El demonio es root: la sesión no lo
puede matar con `kill` (apágalo con `wg-quick down`).

El `.conf` lleva la clave privada: debe ser solo tuyo (`chmod 600`); si otros pueden leerlo,
`wg-quick` avisa.

## El archivo (formato de wg-quick)

```ini
[Interface]
PrivateKey = <la de wg genkey>
Address = 10.8.0.2/24
DNS = 10.8.0.1            # opcional: mientras el túnel está encendido, el DNS es este
MTU = 1420                # opcional (1420 por defecto)
ListenPort = 51820        # opcional: solo si el otro lado tiene que llamarte a ti

[Peer]
PublicKey = <la del servidor>
PresharedKey = <wg genpsk>        # opcional
Endpoint = vpn.ejemplo.org:51820
AllowedIPs = 10.8.0.0/24          # o 0.0.0.0/0 para mandar todo por la VPN
PersistentKeepalive = 25          # recomendable detrás de un NAT
```

Hasta 8 `[Peer]` y 16 `AllowedIPs` por par. `PreUp`/`PostUp`/`Table`/`FwMark`/`SaveConfig`
se ignoran con un aviso. El nombre del `Endpoint` se resuelve al encender (antes de poner
las rutas).

**Rutas:** cada `AllowedIPs` es una ruta por el túnel y, a la vez, el filtro de lo que se
acepta de ese par (encaminamiento por clave). Con `0.0.0.0/0` todo va por el túnel **menos
la red local** (como wg-quick con su regla `suppress_prefixlength 0`): la impresora y el
router siguen al alcance, y los datagramas del propio WireGuard salen siempre por la
interfaz física (no hace falta una ruta al servidor). Una ruta del túnel tan larga como la
red local (p. ej. `192.168.1.0/24` estando en esa red) gana a la red local.

## Ejemplo con un servidor Linux

En el servidor (Debian/Ubuntu, con `wireguard-tools`):

```sh
umask 077
wg genkey | tee servidor.key | wg pubkey > servidor.pub
cat > /etc/wireguard/wg0.conf <<EOF
[Interface]
PrivateKey = $(cat servidor.key)
Address = 10.8.0.1/24
ListenPort = 51820
# para salir a Internet por el servidor:
PostUp = iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -o eth0 -j MASQUERADE
PostDown = iptables -t nat -D POSTROUTING -s 10.8.0.0/24 -o eth0 -j MASQUERADE

[Peer]
# SaguiOS
PublicKey = <la pública que da «wg pubkey» o la ventana de SaguiOS>
AllowedIPs = 10.8.0.2/32
EOF
sysctl -w net.ipv4.ip_forward=1
wg-quick up wg0
```

En SaguiOS, `/home/.config/wireguard/casa.conf`:

```ini
[Interface]
PrivateKey = <la privada de SaguiOS>
Address = 10.8.0.2/24
DNS = 10.8.0.1

[Peer]
PublicKey = <contenido de servidor.pub>
Endpoint = <IP o nombre del servidor>:51820
AllowedIPs = 0.0.0.0/0
PersistentKeepalive = 25
```

Y `wg-quick up casa`; `ping 10.8.0.1` y `wg show` en los dos lados deben enseñar el saludo.
Si no hay saludo: el puerto UDP 51820 del servidor (cortafuegos, NAT del router), las dos
claves públicas cruzadas, y la hora de SaguiOS (WireGuard rechaza una iniciación con una
marca de tiempo anterior a la última que vio del mismo par: si el reloj de SaguiOS va muy
atrasado, reinicia el servidor o ajusta la hora).

`DNS =` con un servidor que solo existe dentro del túnel (p. ej. el `dnsmasq` del servidor)
funciona mientras su IP esté en `AllowedIPs`.

## Depurar

- `wg show`: último saludo, bytes, extremo actual (cambia si el servidor «se mueve»:
  roaming), y los descartados (repetidos, MAC o etiqueta mala, origen no permitido).
- `ifconfig`: la interfaz del túnel con sus paquetes.
- `dmesg | grep TUNEL` y el registro del demonio (`[WG]`).
- Por dentro: `kernel/rede/tunel.c` (interfaz, rutas, MTU, DNS), `userland/libtls/wireguard.c`
  (protocolo) y `userland/programas/wg.c` (demonio, `wg`, `wg-quick`, ventana). Ver D363 en
  `docs/decisoes.md`.

## Límites

- Un túnel a la vez. Solo IPv4. Sin `PostUp` (no hay iptables).
- El demonio corre en espacio de usuario: unos pocos MB/s (lo que da ChaCha20 en
  software en una CPU de 32 bits), de sobra para SSH, la web y la depuración remota.
- Cookies (protección contra inundaciones): como iniciador se reciben y se usan (mac2); como
  respondedor se piden cuando llegan más de 32 saludos por segundo.
