# Capibara

Motor de juegos 2D sencillo, en C11 con guiones en Lua 5.4, sobre la SDL 2. Licencia MIT (ver `LICENSE`).
Árbol de nodos, escenas en texto, señales y guiones, con nombres y API en español.

    capibara ejemplos/plataformas          abre un proyecto (una carpeta con capibara.cfg)
    capibara juego.capi                    abre un paquete
    capibara --empaquetar carpeta juego.capi

En SaguiOS: Inicio > Juegos > «Capibara: plataformas» y «Capibara: escuadrón»; los ejemplos están en `/apps/capibara/`.
En Linux: `make` en esta carpeta (necesita SDL2, SDL2_image y las fuentes de Lua 5.4; ver el `Makefile`).
Para hacer juegos con un editor visual: Inicio > Programación > «Capibara: editor» (`capibara-editor [carpeta]`, solo en SaguiOS).
F3: HUD de rendimiento. F4: ver las colisiones. F11: pantalla completa. Esc: salir.

Documentación completa (arquitectura, formato de escena, API de Lua): `docs/capibara.md` del repositorio de SaguiOS.
Los ejemplos usan gráficos CC0 de Kenney (www.kenney.nl); créditos en `ejemplos/*/CREDITOS.txt`.
