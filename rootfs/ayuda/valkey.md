# valkey: base de datos en memoria

**valkey-server** y **valkey-cli** (`/apps/bin`) son Valkey 8.0.2, la continuación libre de Redis. Guardan claves con cadenas,
listas, hashes, conjuntos, conjuntos ordenados y más, en memoria, con copia en disco opcional.

## Usarlo

    valkey-server /apps/valkey/valkey.conf &        arranca el servidor (127.0.0.1, puerto 6379)
    valkey-cli SET color verde                      una orden
    valkey-cli GET color
    valkey-cli                                      modo interactivo
    valkey-cli SAVE                                 guarda /home/dump.rdb (se carga al arrancar)

## Desde programas

- **C:** `tcc ejemplo.c -lhiredis` con `#include <hiredis/hiredis.h>` (ejemplo en `/apps/valkey/ejemplos/`).
- **MicroPython:** `import valkey; r = valkey.Valkey(); r.set("a", 1); r.get("a")`.
- **Lua:** `local r = require("valkey").conectar("127.0.0.1", 6379); r:set("a", 1)`.

## Límites en SaguiOS

Hasta unos 90 clientes a la vez (128 descriptores por proceso). Sin sockets de dominio Unix, sin TLS, sin módulos y sin hilos de
entrada y salida. Más en `/apps/valkey/leeme.txt`.
