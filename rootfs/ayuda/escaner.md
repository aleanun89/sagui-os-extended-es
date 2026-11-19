# Escáner de red («escaner»)

**Aviso:** escanea solo redes propias o con permiso expreso de quien las administra. Un escaneo
puede ser ilegal en redes ajenas y salta las alarmas de cualquier cortafuegos.

`escaner` es un escáner de red propio de SaguiOS (D398): descubre equipos y busca puertos TCP
abiertos. Está escrito desde cero; se inspira en el uso de los escáneres clásicos, no copia su código.

    escaner 192.168.1.1                     # los 100 puertos más comunes de un equipo
    escaner -sn 192.168.1.0/24              # qué equipos hay vivos
    escaner -p 22,80,1-1024 -sV servidor    # puertos concretos, con el rótulo del servicio
    escaner -F -T4 -oN informe.txt 10.0.0.1-50

## Objetivos

IP, nombre (se resuelve por DNS), rango de la última cifra (`192.168.1.1-50`) o red CIDR
(`192.168.1.0/24`, como mínimo `/22`, unas mil direcciones; sin la de red ni la de difusión).
Se pueden dar varios.

## Opciones

| Opción | Qué hace |
| --- | --- |
| `-sn` | Solo descubre equipos: no escanea puertos. |
| `-Pn` | No descubre: trata todos los objetivos como activos. |
| `-p LISTA` | Puertos: `22,80,1-1024`; `-p-` son todos. |
| `--top-ports N` | Los N puertos más comunes de la tabla propia (de serie 100). |
| `-F` | Rápido: los 25 más comunes. |
| `-sV` | Intenta leer el rótulo del servicio (lo que dice el servidor al conectar o la cabecera `Server:` de un HEAD de HTTP). |
| `-T1`...`-T4` | Plazo por puerto y paralelismo: T1 5 s y 4 conexiones, T2 3 s y 8, T3 (de serie) 1,5 s y 16, T4 0,8 s y 24. |
| `-oN FICHERO` | Guarda también el informe en un fichero. |
| `-v` | Muestra los hallazgos según llegan. |

## Salida

    PUERTO     ESTADO    SERVICIO       VERSIÓN
    22/tcp     abierto   ssh            SSH-2.0-OpenSSH_9.6

- **abierto**: el equipo aceptó la conexión. **cerrado**: la rechazó (RST). **filtrado**: no
  contestó a tiempo (un cortafuegos, o el equipo no existe).
- Solo se listan los abiertos; de los demás se da el recuento.
- El descubrimiento prueba conexiones a 80, 443, 22 y 445 (un rechazo también delata un equipo vivo)
  y, si no hay respuesta, un eco ICMP.

## Límites

- Solo TCP por conexión completa (sin SYN a medias, sin UDP, sin huellas del sistema operativo).
- Hasta 24 conexiones a la vez: el kernel tiene 32 conexiones TCP en total.
- El servicio de un puerto sin `-sV` sale de una tabla por número, no de lo que responde.
