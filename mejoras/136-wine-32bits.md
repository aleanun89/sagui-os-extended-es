# 136 - Wine de 32 bits: aplicaciones y juegos antiguos de Windows

- **Estado:** propuesta (a futuro; pedido del mantenedor, 2026-11-17)
- **Prioridad:** baja (después de la v11)
- **Alta:** 2026-11-17

## Problema / motivación

Muchos programas y juegos de Windows 95/98/XP de 32 bits son justo los de la época de los equipos a los que apunta
SaguiOS (Pentium II-III, Athlon, Core de 1.ª-7.ª gen.). Poder ejecutarlos sería un gran salto de utilidad.

## Propuesta

**Wine** (LGPL-2.1-or-later: compatible como programa aparte; se distribuiría como paquete según D375 o dentro si
las licencias lo permiten) no emula: carga los `.exe` PE en el propio proceso y traduce la API de Windows a la del
sistema. Para eso SaguiOS necesita, de menos a más difícil:

1. **Memoria:** reservar y mapear direcciones fijas (los `.exe` se cargan en 0x400000), proteger por páginas
   (ya hay `MEM_PROTEGER`, D352) y espacio de usuario suficiente (hoy la frontera está en 2 GB, D049: Windows de
   32 bits también usa 2 GB de usuario, así que cuadra).
2. **TEB por hilo en el segmento FS:** Windows de 32 bits lee `FS:[0]`, `FS:[0x18]`… Hace falta que cada hilo
   tenga su descriptor (LDT o GDT por hilo, como `modify_ldt`/`set_thread_area` de Linux). Lo mismo pide Go
   oficial (mejora 135, fase 2): se puede hacer una vez para los dos.
3. **Excepciones:** las del procesador (fallo de página, división…) deben llegar al proceso con su contexto de
   registros (señales con `ucontext`) para que Wine las convierta en SEH.
4. **Carga dinámica:** Wine carga sus DLL internas como bibliotecas compartidas (`dlopen`). SaguiOS hoy enlaza en
   estático: hace falta un enlazador dinámico ELF mínimo, o compilar Wine con sus DLL incorporadas.
5. **POSIX que falta:** `fork`/`exec` o equivalentes, `poll`, `pthread` completos, sockets (hay), señales.
6. **Pantalla, sonido y entrada:** un **driver de Wine para SaguiOS** (como los de X11, macOS o Android) sobre las
   ventanas, la SDL 2 o directamente el compositor; sonido sobre el de SaguiOS.
7. **3D:** Direct3D 1-9 de los juegos antiguos va por **wined3d → OpenGL**: depende de OpenGL con Mesa (v8.6) y de
   la aceleración 3D por GPU.

**Alternativas a valorar antes:** un cargador PE mínimo propio para programas Win32 sencillos (sin Wine), o un
enfoque tipo «Wine dentro de un entorno Linux emulado» (más lento, menos dependencias del kernel).

## Criterios de aceptación

- [ ] Un programa Win32 de consola («hola mundo» compilado con MinGW) se ejecuta.
- [ ] Un programa Win32 con ventana (Bloc de notas de Wine, buscaminas propio) funciona.
- [ ] Un juego antiguo libre de 32 bits con DirectDraw/Direct3D sencillo funciona.
- [ ] Test automático en `tests/` que lo comprueba
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Requisitos del kernel compartidos con Go oficial (mejora 135): TLS por segmento, señales con contexto, `poll`.
- Licencia: Wine es LGPL-2.1-or-later. Los programas y juegos de Windows no los distribuye SaguiOS.
