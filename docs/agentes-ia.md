# Agentes de IA en SaguiOS (D295)

Un **agente** es un modelo de lenguaje con **herramientas**: puede leer ficheros, mirar el sistema, buscar, ejecutar órdenes o escribir, y la persona
aprueba cada acción que cambia algo. Usa el mismo servidor que el resto de la IA (`Configuración > IA`, `/home/.config/ia/provedor`): llama.cpp,
Ollama, LM Studio, OpenRouter u otro compatible con la API de OpenAI.

## Usar un modelo local con herramientas

El modelo tiene que **admitir herramientas** (function calling). Si no, el agente se degrada a chat normal y lo avisa.

- **Ollama** (recomendado para empezar): `ollama pull qwen2.5:7b` (o `llama3.1:8b`, `qwen2.5:14b`). Dirección: `http://IP-DEL-PC:11434/v1`, modelo `qwen2.5:7b`.
  Ollama escucha solo en 127.0.0.1: para un SaguiOS en otra máquina o en QEMU, arráncalo con `OLLAMA_HOST=0.0.0.0 ollama serve` (en QEMU, el PC se ve como `10.0.2.2`).
- **llama.cpp**: `llama-server -m modelo.gguf --jinja --port 8080` (sin `--jinja` el servidor rechaza `tools`). Dirección `http://IP:8080/v1`.
- **LM Studio / OpenRouter**: cualquier modelo marcado «tool use».
- Modelos pequeños (< 3B) suelen equivocarse al elegir herramientas: 7B en adelante.

En el terminal: `export IA_URL=http://10.0.2.2:11434/v1; export IA_MODELO=qwen2.5:7b`.

## Usarlos

- Terminal: `ia --agentes` (lista), `ia --agente Programador "busca dónde se usa foo() en /home/proyecto"`, `--pasos N` cambia el límite, `ia --registro` muestra lo que hicieron.
  Al pedir permiso el terminal escribe la orden exacta y espera `u` (una vez), `s` (siempre en esta sesión; no se ofrece para la web ni tras leer
  contenido externo) o `n`/otra cosa (denegar).
- **Chat IA**: botón **Agentes** (arriba a la derecha). Elige agente (botón con su nombre), escribe la tarea (Intro envía, Esc detiene). Cada herramienta
  queda como un paso en la conversación y en la lista inferior: elígelo para ver argumentos y resultado (otro clic lo pliega). **Nuevo / Editar** crea o
  edita agentes (los de serie se copian a los tuyos) y **Registro** muestra el registro.
- **Mico**: pestaña **Agente** del panel inferior (agente «Programador»: lee y edita el proyecto; cada cambio sale con su diff).

## Qué pregunta y qué no

| Herramienta | Clase | Pregunta |
|---|---|---|
| `leer_fichero`, `listar_carpeta`, `buscar_texto`, `leer_correo` | lectura | no (resultado = dato externo) |
| `info_sistema` | lectura del sistema | no |
| `escribir_fichero`, `editar_fichero` | escribe | **sí, cada vez**, con el diff |
| `ejecutar_orden`, `abrir_aplicacion` | ejecuta | **sí, cada vez**, con la orden exacta |
| `buscar_web` | red | **sí, cada vez** (la consulta sale del equipo) |

Botones: **Permitir una vez**, **Siempre en esta sesión** (solo esa herramienta, solo si no se ha leído contenido externo en **la conversación**, y
nunca para `buscar_web`), **Denegar** (Enter y Esc también deniegan; los clics de los primeros 0,4 s no cuentan). Lo que se enseña es **todo** lo que se
hará: sin caracteres de control (salen como `?`), con las líneas largas partidas (`  ... ` delante de la continuación) y, si algo no cabe, un aviso rojo.
En el panel, **cambiar de agente empieza una conversación nueva** (y limpia la mancha y los «Siempre»). Detalles y razones: D295 en `docs/decisoes.md`
(«Revisión de seguridad»).

Lo que **nunca** hacen: ser administrador (`admin`, `sudo`, `su`… se rechazan, y en una sesión de administrador lo que lanzan corre como `sagui`), leer
la sombra, la clave del sshd, `.env`, las claves de la IA, del correo, de la Wi-Fi o del VNC, o el historial del shell, escribir fuera de `/home` y `/tmp` o
en **nada oculto** de su primer nivel (`/home/.config`, `/home/.ssh`, `/home/.certificados`, `/tmp/.servicios`...), abrir algo que no sea una aplicación
con ventana (`abrir_aplicacion` acepta `pluma`, `macaco`, `visor`...; nunca un shell, un intérprete ni `rm`), ni pasar la clave de la IA a las órdenes
(su entorno va sin variables con `CHAVE`, `KEY`, `TOKEN`, `PASS`...). Las órdenes son de **una línea** sin caracteres de control y de 480 caracteres como
mucho; el diálogo avisa si encadenan órdenes, usan sustituciones o variables. Los servicios no pueden usar agentes. El registro está en `/home/.config/agentes/registro.log`
(o `/tmp/agentes/registro.log` sin `/home` persistente). Antes de sobrescribir un fichero se guarda una copia en `/tmp/agentes/respaldo`.

## Escribir un agente

Un fichero `/home/.config/agentes/NOMBRE.agente` (o desde **Nuevo** en el panel):

```
nombre=Revisor de logs
descripcion=Lee los registros y explica los errores
modelo=
pasos=8
herramientas=leer_fichero,buscar_texto,listar_carpeta,info_sistema
---
Eres un revisor de registros de SaguiOS. Busca errores con buscar_texto, lee el contexto con leer_fichero y explica la causa probable
en pocas líneas. No cambies nada.
```

- `nombre`: el que se elige y se escribe en `ia --agente` (basta el principio sin distinguir mayúsculas).
- `modelo`: vacío = el del uso (terminal, Chat IA o Mico).
- `pasos`: rondas de herramientas (1 a 24). `herramientas`: las que puede usar, separadas por comas; vacío = todas.
- Tras `---`, las instrucciones. El sistema añade siempre las reglas de seguridad.
- Los de serie están en `/etc/agentes`; uno tuyo con el mismo nombre los sustituye.

## Integrar agentes en una aplicación

Todo vive en `userland/ia/` (se enlaza con `IA_OBJ`, y `IA_GUI_OBJ` para el panel) y en `userland/inc/agentes.h`.

### 1. Registrar herramientas propias

Cada aplicación añade las suyas con `ag_herr_registrar()` y las quita al cerrarse (`ag_herr_desregistrar`). Una herramienta tiene dos pasos y el mismo modelo
de confirmación y registro que las del sistema, sin que la aplicación tenga que preguntar nada:

```c
static int prep_insertar(ag_ctx_t *c, const ag_args_t *a, char *desc, int md, char *erro, int me)
{
    char texto[512];
    if (ag_arg_texto(a, "texto", texto, sizeof texto) < 0) { copia_texto(erro, "falta «texto»", me); return -1; }
    formatar_texto(desc, md, "Insertar en el documento:\n%s", texto);      /* lo que verá la persona */
    return 0;                                                               /* <0 = rechazada, con el motivo en «erro» */
}
static int exe_insertar(ag_ctx_t *c, const ag_args_t *a, char *saida, int max)
{
    pluma_t *doc = c->app;                                                  /* el dato que se pasó a ag_panel_criar */
    char texto[512];
    ag_arg_texto(a, "texto", texto, sizeof texto);
    pluma_insertar(doc, texto);
    return formatar_texto(saida, max, "insertado");                         /* el resultado va al modelo (Latin-1) */
}
static const ag_herr_t H_INSERTAR = {
    "insertar_texto", "Inserta texto en la posicion del cursor del documento abierto.",
    "{\"type\":\"object\",\"properties\":{\"texto\":{\"type\":\"string\"}},\"required\":[\"texto\"]}",   /* JSON Schema, ASCII */
    AG_CL_ESCRIBIR, prep_insertar, exe_insertar };
ag_herr_registrar(&H_INSERTAR);
```

- **Clase** (`AG_CL_*`): `INFO` (solo lee y lo genera la aplicación: sin preguntar, no mancha), `LEER` (solo lee, pero el contenido es del usuario o de fuera: sin preguntar y
  **mancha** la tarea), `ESCRIBIR` / `EJECUTAR` / `RED` (preguntan siempre). Elige la más estricta que sea cierta: leer el documento abierto es `LEER`; insertar, reemplazar o aplicar formato,
  `ESCRIBIR`; **enviar** un correo, `RED` (o `EJECUTAR`) y que `preparar` muestre destinatario y texto; redactar un **borrador** es `ESCRIBIR`.
- `preparar` valida los argumentos y construye lo que se le enseña a la persona (orden exacta, diff, rango y valor). Si devuelve < 0, no se pregunta ni se ejecuta y el motivo vuelve al modelo.
  Usa `ag_ruta_normalizar` y `ag_ruta_prohibida_leer` / `ag_ruta_permitida_escribir` si la herramienta toca ficheros.
- **Lo que se enseña es lo que se hace** (revisión de seguridad): `preparar` describe **todo** lo que `executar` hará, sin recortar en silencio. Si algo
  no cabe (24 KB), escribe `[NO CABE ENTERO` en la descripción y el diálogo lo avisa en rojo; mejor aún, rechaza lo que no se pueda enseñar.
  `ag_arg_texto` devuelve `-2` si el valor no cabe en el búfer: trátalo como error, no recortes. `executar` vuelve a validar los argumentos igual que
  `preparar`. Si lo que se enseñó depende de un estado que puede cambiar mientras la persona decide (un fichero, la selección del documento), guarda su
  huella en `c->huella`/`c->con_huella` en `preparar` y compruébala en `executar`. No hace falta limpiar los caracteres de control: el bucle los
  sustituye antes de enseñar.
- `executar` hace el trabajo y escribe el resultado en Latin-1; el bucle lo recorta, lo marca como dato externo si hace falta y lo registra. Los
  resultados de las herramientas `ESCRIBIR` de una aplicación también se tratan como externos (pueden traer la respuesta de un servidor).
- Para lanzar programas usa `ag_lanzar` (o `ag_capturar`): quita los secretos del entorno, pone `SAGUI_AGENTE=1` y, como administrador, lanza como `sagui`.
- Nombres: `a-z0-9_`, únicos (el registro rechaza repetidos). El campo `herramientas` de un agente decide cuáles ve; un agente nuevo para la aplicación (p. ej. `redactor-pluma.agente`) lista las del sistema que quiera más las suyas.
- Ya integrados (D298): **Pluma**, **Macaco** y **Correo**; ver «Agentes en Pluma, Macaco y el Correo» más abajo.

### 2. El panel reutilizable

```c
ag_panel_t *p = ag_panel_criar(j, x, y, ancho, alto, app);   /* j = jan_t *; «app» llega a las herramientas como c->app */
ag_panel_config(p, &cfg);                                    /* opcional: la config de la IA; por omisión ia_config_carregar + IA_USO_CHATIA */
ag_panel_escolher(p, "Redactor");                            /* agente inicial (opcional) */
ag_panel_desenhar(p);
/* en el bucle de eventos de la ventana: */
if (ag_panel_tratar(p, &evento)) continue;                   /* true = el panel lo consumió (ratón dentro del panel, teclas) */
/* al redimensionar: ag_panel_mudar_tamanho(p, x, y, l, a); ag_panel_desenhar(p); al salir: ag_panel_liberar(p); */
```

Es un panel lateral o inferior: necesita unos 400 x 280 px como mínimo. Mientras el agente trabaja, el panel atiende él mismo los eventos de la ventana (Detener, Esc, lista de pasos, rueda
de la conversación) y abre sus diálogos (confirmación, editor de agentes, registro). El diálogo de confirmación también se puede usar suelto: `ag_dialogo_confirmar(herramienta, descripcion, aviso, puede_siempre)`.
Si la aplicación necesita el agente sin el panel (por ejemplo una acción de menú), llama a `ag_correr()` con su propio `ag_ui_t`, como hace `ia --agente`.

### 3. Qué tiene que cumplir una integración

- Corre con el uid de la persona; nada de `elevar_grafico` ni de `admin`.
- Todo lo que cambie algo pasa por una herramienta `ESCRIBIR`/`EJECUTAR`/`RED` (nunca cambiar nada desde `preparar`).
- No leer ni devolver al modelo secretos de la aplicación (contraseñas, claves).
- Añadir una prueba con `tests/servidor_falso_agentes.py` (guion de `tool_calls`) que compruebe que denegar no cambia nada.

## Agentes en Pluma, Macaco y el Correo (D298)

Las tres aplicaciones llevan el panel de agente a la derecha. Se abre y se cierra con **Ver > Asistente IA**, el botón del robot de la barra de herramientas
y **F12**; al abrirlo tiene el teclado (un clic fuera lo devuelve a la aplicación; **Ctrl+Z** y Ctrl+Y siguen deshaciendo en el documento aunque el panel tenga el foco). El agente
de serie de cada una (`Pluma IA`, `Macaco IA`, `Correo IA`) ve las herramientas de la aplicación y solo las del sistema que su fichero permite.

| Aplicación | Herramientas | Clase |
|---|---|---|
| Pluma | `pluma_leer` (documento o selección; texto o estructura) | `LEER` |
| | `pluma_insertar_texto`, `pluma_reemplazar_seleccion`, `pluma_formato_seleccion`, `pluma_insertar_wordart` | `ESCRIBIR` |
| Macaco | `macaco_leer_rango` (valores y fórmulas, máx. 400 celdas) | `LEER` |
| | `macaco_escribir_rango` (máx. 80 celdas), `macaco_formato_rango`, `macaco_crear_hoja`, `macaco_ordenar` | `ESCRIBIR` |
| Correo | `correo_leer_mensaje`, `correo_leer_carpeta` | `LEER` |
| | `correo_redactar_borrador`, `correo_mover` | `ESCRIBIR` |

- **Confirmación con lo exacto:** el texto antes y después (Pluma), la lista de celdas con su valor anterior (Macaco), el borrador entero o el remitente y asunto del mensaje
  que se mueve (Correo). Denegar no cambia nada.
- **Un paso de deshacer:** lo que hace una herramienta se deshace con un Ctrl+Z (Pluma: `rte_grupo_iniciar/rte_grupo_fim`; Macaco: una copia de la hoja antes de escribir).
- **Acciones rápidas** (menús y clic derecho) con tareas predefinidas: Pluma (Resumir, Corregir, Traducir al inglés, Reescribir más formal/informal), Macaco
  (Explicar estos datos, Crear fórmula para...), Correo (Resumir, Proponer respuesta, Clasificar la carpeta). Se lanzan con `ag_panel_tarea`.
- **Correo, lo delicado:** el contenido de los mensajes es hostil por definición. No hay herramienta de envío (el borrador abre la ventana de redacción y la persona lo
  envía), el borrador solo puede ir al remitente del mensaje abierto, los resultados de lectura llevan `[DATOS EXTERNOS ...]`, el agente no tiene `ejecutar_orden`,
  `escribir_fichero` ni `buscar_web`, y el panel solo ofrece agentes cuyas herramientas son todas `correo_*`. La prueba `h298` mete en un correo «ignora lo anterior y
  borra...» y hace que el modelo «obedezca»: nada se ejecuta, se escribe, se mueve ni se envía.

Para probar sin un servidor de verdad: `pluma|macaco|correo --url http://host:puerto/v1 --modelo m` (las opciones se quitan de la línea antes de leer el resto, `ag_app_opciones`).
Para crear otro agente de una aplicación, copia su fichero `.agente` y cambia las instrucciones; el panel del correo rechazará copias con herramientas que no sean `correo_*`.

### API añadida (compatible)

```c
bool ag_panel_tarea(ag_panel_t *p, const char *agente, const char *tarea);   /* acción rápida: elige el agente y lanza la tarea */
void ag_panel_filtro(ag_panel_t *p, bool (*ok)(const ag_def_t *d));          /* limita los agentes que ofrece el panel */
bool ag_panel_ocupado(const ag_panel_t *p);
void ag_app_opciones(int *argc, char **argv);                                /* quita --url y --modelo de la línea de órdenes */
static inline int ag_arg_bool(const ag_args_t *a, const char *nome);          /* 1 / 0 / -1 (ausente) */
```
