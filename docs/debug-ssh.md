# Imágenes de depuración por SSH (D418)

Para probar en un portátil sin los códigos de DEBUG-APP: la imagen ya lleva la clave del PC, `sshd` arrancado y una huella fija.

```
make debug_variantes DEBUG_DESTINO=~/SO/debug [DEBUG_CLAVE=~/.ssh/saguios_amilo.pub] [JOGOS_MB=1600] [DEBUG_SIN_ROOT=1]
```

Deja las imágenes (como `priv_variantes`) en `DEBUG_DESTINO`, con un `VERSION.txt` que dice «IMAGEN DE DEPURACIÓN: ... no distribuir».
Escribe la imagen en el pendrive o disco como siempre y entra con:

```
ssh -o UserKnownHostsFile=~/SO/private/debug-ssh/known_hosts -i ~/.ssh/saguios_amilo root@IP_DEL_PORTATIL
```

- `sshd` solo atiende a la red local (`solo_red_local=si`); `ntp` pone la hora al arrancar; zona Europe/Madrid.
- La clave del servidor está en `~/SO/private/debug-ssh/` (se crea la primera vez; **nunca a git**); el objetivo imprime su huella.
- Escritorio con un aviso rojo «IMAGEN DE PRUEBA / Acceso SSH para revision automatica».
- `DEBUG_SIN_ROOT=1`: sin la pantalla de la contraseña de root del primer arranque (apagado por defecto).
- **No se publican**: `tools/publicar.sh` las rechaza (marca `IMAGEN-DE-DEPURACION-D418-NO-DISTRIBUIR`, `authorized_keys`, claves del servidor) y
  `make variantes` falla si se le pasa `DEBUG_*`.
- Prueba: `python3 tests/h418_imagen_debug.py`. Ver D418 en `docs/decisoes.md`.
