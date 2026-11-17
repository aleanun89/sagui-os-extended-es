# Traspaso: usuarios v8.5.3 (D371)

Rama `v8.5.3-usuarios`, en `/home/aanun/SO/miniOS_master_os_usuarios`. `main` está fusionado hasta 4bf332a7.
D371 está **decidido e implementado** (las fases 1 a 6). El resumen de lo hecho y lo pendiente está en `docs/decisoes.md`
(D371, «Implementado»). El diseño completo está en `docs/usuarios-v853.md` y la guía, en `docs/usuarios.md`.

## Fases y pruebas

| Fase | Qué | Prueba |
|---|---|---|
| 1 | Configuración de la máquina en `/etc` (SAGUI-CONF) | h375 |
| 2 | `/root` y `HOME`; lo que ven los programas elevados | h376 |
| 3 | El instalador (cuenta, contraseña de root) y la primera contraseña de root al arrancar | h381 |
| 4 | Migración de sagui a `/home/sagui` (reanudable) | h384 |
| 5 | SSH, cron y servicios por cuenta | h382 |
| 6 | `login` en la consola, inicio automático y bloqueo de pantalla | h383 |

**Pasan, una a una, tras su fase:**
- las de las seis fases: h375, h376, h381, h382, h383 y h384;
- h340, h371, h254, h282, h318, h149 y h216;
- h67, h78, h83, h290, h292, h307, h323, h333, h369, h355, h218, h356, h200, h117, h353 y h250.

**Batería reducida** (`tests/reducida.txt` y las de usuarios, KVM, -j 4): 89 de 91 a la primera. Fallaron h55 (la raíz tiene ahora `/root`) y h248 (`exec_utf8` en `/home/.ssh/sshd.conf`). Las dos están arregladas y pasan, junto con h251 y h382. h133 solo pasó al repetirla.

**Numeración de pruebas:** h377-h380 son de otras ramas. Las de D371 son h375, h376 y h381-h384.

## Revisión de seguridad (2026-11-17)

Hecha antes de fusionar, con `main` integrado (b68aa924). Ocho correcciones (D371, «Revisión de seguridad»): la
primera contraseña de root tras «más tarde», `login` con root sin contraseña, las teclas pulsadas con la pantalla de
bloqueo, las claves de la máquina en la carpeta de sagui, `/home/compartido` común de verdad, la migración más
estricta, cron/su y los servicios del sistema que encendía (o reemplazaba) la carpeta de la cuenta. Pruebas ampliadas: h381 (parte C), h383 (atajos con la pantalla bloqueada) y h384 (claves y
común).

## Para las pruebas

- `tests/qemu.py` pone `SAGUI_PRUEBA_USUARIOS=1`, y con esa variable `mkimagem.py` usa `--sin-pedir-root` y `--sin-migrar-home` de serie:
  - `--sin-pedir-root`: no aparece la pantalla de la contraseña de root, la consola no pide la cuenta y se entra directo;
  - `--sin-migrar-home`: sagui no se pasa a `/home/sagui`.
- Una prueba que quiera lo real usa `--pedir-root` o `--migrar-home`.
- Las imágenes que hace `make` (las publicadas) no llevan esos bits.

## Pendiente

- Servicios de persona de otras cuentas: hoy solo los de la principal (habría que dar nombres que no choquen).
- Redes y túneles «de la máquina» (P10 b).
- Estos ficheros siguen en `/home` aunque los escribe root: la marca `acel-arranque`, `debug-app.conf` y `ampliar-home.no`.
- Algunos programas root crean `/home/.config` en la raíz de `/home`.
- La Configuración aún no tiene la casilla del inicio automático (se cambia con `login --inicio-automatico si|no`).
- Después de cerrar sesión, la pantalla de inicio de sesión usa el teclado de la cuenta principal, no el del sistema (P7, parcial).
- `mv` copia cuando se le niega el renombrado.
- Al fusionar en main, revisar las pruebas que construyen sus propias imágenes con `mkimagem.py` (ya heredan los bits por la variable de entorno).
