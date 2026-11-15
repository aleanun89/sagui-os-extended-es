# 102 - Diálogo gráfico de permisos de administrador (y avisos que se leen enteros)

- **Estado:** implementada en la rama `v8.0-dialogo-admin` (**v8.0**, D269; pendiente de fusión)
- **Prioridad:** alta
- **Alta:** 2026-11-14
- **Relacionadas:** usuarios fase 1 (D255, mejora 098: `SERV_PEDIR`, `pedir_conta`, `admin`, `su`),
  discusión #15 (pregunta 3), avisos emergentes (issue #16), Terminal con root visible (D263)

## Motivación

Desde la D255 los programas de administración (Discos, Particiones, instalar, Configuración de red,
servicios, Administrador de tareas…) dicen «hace falta ser administrador: usa admin…», pero en el
escritorio eso obliga a abrir un Terminal. El mantenedor quiere una **ventana al estilo del Control
de cuentas de usuario de Windows** que pida la contraseña cuando una aplicación gráfica la necesita.
Además, los avisos emergentes cortan el texto (issue #16).

## Propuesta

### Diálogo de permisos
- Cuando una aplicación gráfica necesita ser root: ventana modal del sistema, **atenuando el resto
  del escritorio**, con el icono y el nombre del programa que lo pide, qué quiere hacer («Particiones
  quiere modificar los discos»), campo de contraseña de **root** y botones «Sí, continuar» / «No».
- La comprobación la hace el gestor de servicios (D255, sin setuid): la interfaz `SERV_PEDIR` /
  `pedir_conta` ya existe. El diálogo lo dibuja el sistema (no el programa que pide), para que un
  programa no pueda falsificarlo ni leer la contraseña.
- Si root aún no tiene contraseña (modo de primera vez, D255): el diálogo pide elegirla (dos veces).
- Errores claros (contraseña mala, espera de 2 s, bloqueo tras 5 fallos), y Esc = No.
- Opción (discusión #15, pregunta 3, recomendada A): recordar la autorización 5 minutos para ese
  programa.
- En SSH o sin escritorio, se sigue pidiendo en el terminal (`admin`, `su`).
- Los programas gráficos de administración (Discos, Particiones, instalar --ventana, Configuración,
  Administrador de tareas, servicios en la bandeja…) piden la elevación con este diálogo en vez de
  mostrar el error.

### Avisos emergentes (issue #16)
- Texto completo siempre: caja que crece en alto, corte de línea por el ancho real del texto, más
  capacidad que los 96 bytes actuales; duración según la longitud; cola de avisos.

## Pruebas
- QEMU: abrir Particiones como `sagui`, aparece el diálogo (captura), contraseña mala y buena, Esc;
  primera vez sin contraseña de root; recordar 5 minutos; un programa no puede leer la contraseña.
- Aviso largo (varias líneas) completo en la captura; dos avisos seguidos se ven los dos.

## Criterios de aceptación
- [x] Diálogo modal del sistema que pide la contraseña de root a las apps gráficas
- [x] Las apps de administración lo usan
- [x] Avisos con el texto completo (issue #16 cerrada)
- [x] Test automático en `tests/`
- [x] Decisión registrada en `docs/decisoes.md`

## Notas
- Modelo sugerido: Sonnet (gestor de ventanas y userland), con la interfaz de D255 ya hecha.
