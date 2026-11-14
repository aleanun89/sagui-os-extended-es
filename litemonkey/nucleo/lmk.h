// SPDX-License-Identifier: GPL-2.0-or-later
/* lmk.h - el núcleo de lite-monkey: documentos BSON, consultas,
 * actualizaciones, índices, almacenamiento y órdenes del protocolo.
 *
 * Es C99 portátil: lo que depende del sistema (memoria, archivos, red,
 * reloj, azar) está detrás de plataforma.h. */
#ifndef LMK_H
#define LMK_H
#include "base.h"

#define LMK_VERSION        "litemonkey 1"
#define LMK_PUERTO         7979
#define LMK_DOC_MAX        (1024u * 1024u)        /* tamaño máximo de un documento */
#define LMK_PROF_MAX       32                     /* anidamiento máximo */
#define LMK_TRAMA_MAX      (48u * 1024u * 1024u)  /* una petición (inserción por lotes) */
#define LMK_LOTE_BYTES     (4u * 1024u * 1024u)   /* respuesta de find/getMore */
#define LMK_NOMBRE_MAX     64
#define LMK_INDICES_MAX    16

/* Códigos de error (los mismos números que MongoDB, para que los clientes
 * que ya los conocen sepan qué pasó). */
#define ERR_INTERNO        1
#define ERR_VALOR          2      /* BadValue */
#define ERR_SIN_MEMORIA    4      /* (propio) */
#define ERR_NO_AUTORIZADO  13
#define ERR_TIPO           14     /* TypeMismatch */
#define ERR_AUTENTICACION  18
#define ERR_NS_NO_EXISTE   26
#define ERR_INDICE_NO_EXISTE 27
#define ERR_PATH           28     /* PathNotViable */
#define ERR_CURSOR         43
#define ERR_ORDEN          59     /* CommandNotFound */
#define ERR_ID_INMUTABLE   66
#define ERR_INDICE_EXISTE  85
#define ERR_ES             10334  /* BSONObjectTooLarge */
#define ERR_DUPLICADO      11000
#define ERR_DISCO          30001  /* (propio) */
#define ERR_FORMATO        30002  /* (propio) BSON mal formado */

typedef struct {
    int  codigo;
    char msg[240];
} lmk_error;
bool lmk_fallo(lmk_error *e, int codigo, const char *msg);   /* siempre false */

/* ------------------------------------------------------------ búfer */
typedef struct {
    uint8_t *p;
    uint32_t n, cap;
    bool     sin_memoria;    /* una ampliación falló: el contenido no vale */
} lmk_buf;

void buf_liberar(lmk_buf *b);
bool buf_reservar(lmk_buf *b, uint32_t extra);
void buf_poner(lmk_buf *b, const void *d, uint32_t n);
void buf_byte(lmk_buf *b, uint8_t v);
void buf_u32(lmk_buf *b, uint32_t v);
void buf_u64(lmk_buf *b, uint64_t v);

/* ------------------------------------------------------------ arena */
typedef struct lmk_bloque lmk_bloque;
typedef struct { lmk_bloque *b; } lmk_arena;
void *arena_aloca(lmk_arena *a, uint32_t n);    /* NULL sin memoria */
void  arena_liberar(lmk_arena *a);

/* ------------------------------------------------------------ BSON */
#define BT_DOUBLE   0x01
#define BT_STRING   0x02
#define BT_DOC      0x03
#define BT_ARRAY    0x04
#define BT_BINARIO  0x05
#define BT_OID      0x07
#define BT_BOOL     0x08
#define BT_FECHA    0x09
#define BT_NULL     0x0A
#define BT_REGEX    0x0B
#define BT_INT32    0x10
#define BT_TIMESTAMP 0x11
#define BT_INT64    0x12
#define BT_MAXKEY   0x7F
#define BT_MINKEY   0xFF

typedef struct {
    uint8_t        tipo;
    const char    *nombre;
    uint32_t       nlen;
    const uint8_t *v;        /* el valor tal como está en el documento */
    uint32_t       vlen;
} bson_elem;

typedef struct { const uint8_t *p, *fin; } bson_it;

static inline uint32_t bson_tam(const uint8_t *doc) { return lmk_le32(doc); }
uint32_t bson_tam_valor(uint8_t tipo, const uint8_t *v, uint32_t max);   /* 0 = no cabe o tipo desconocido */
bool     bson_validar(const uint8_t *p, uint32_t n, lmk_error *err);      /* tamaños, cadenas y anidamiento */
void     bson_it_iniciar(bson_it *it, const uint8_t *doc);
bool     bson_it_sig(bson_it *it, bson_elem *e);                         /* el documento ya está validado */
bool     bson_buscar(const uint8_t *doc, const char *nombre, bson_elem *e);
bool     bson_vacio(const uint8_t *doc);
uint32_t bson_contar(const uint8_t *doc);

/* Escritura: bson_inicio deja sitio para el tamaño; bson_fin pone el 0 final
 * y lo rellena. Un subdocumento: bson_clave(BT_DOC o BT_ARRAY) + inicio/fin. */
uint32_t bson_inicio(lmk_buf *b);
void     bson_fin(lmk_buf *b, uint32_t ini);
void     bson_clave(lmk_buf *b, uint8_t tipo, const char *nombre, uint32_t nlen);
uint32_t bson_inicio_sub(lmk_buf *b, uint8_t tipo, const char *nombre);
void     bson_poner_double(lmk_buf *b, const char *nombre, double d);
void     bson_poner_int32(lmk_buf *b, const char *nombre, int32_t v);
void     bson_poner_int64(lmk_buf *b, const char *nombre, int64_t v);
void     bson_poner_entero(lmk_buf *b, const char *nombre, int64_t v);   /* int32 si cabe */
void     bson_poner_bool(lmk_buf *b, const char *nombre, bool v);
void     bson_poner_null(lmk_buf *b, const char *nombre);
void     bson_poner_cadena(lmk_buf *b, const char *nombre, const char *s, uint32_t len);
void     bson_poner_texto(lmk_buf *b, const char *nombre, const char *s);
void     bson_poner_elem(lmk_buf *b, const char *nombre, uint32_t nlen, const bson_elem *e);   /* copia el valor */
uint32_t bson_indice(char *dst, uint32_t i);   /* "0", "1"... para los arrays; devuelve la longitud */

/* Lectura de valores */
static inline int32_t bson_int32(const uint8_t *v) { return (int32_t)lmk_le32(v); }
static inline int64_t bson_int64(const uint8_t *v) { return (int64_t)lmk_le64(v); }
static inline const char *bson_cadena(const bson_elem *e) { return (const char *)e->v + 4; }
static inline uint32_t bson_cadena_len(const bson_elem *e) { return lmk_le32(e->v) - 1; }
bool     bson_es_numero(uint8_t tipo);
double   bson_a_double(const bson_elem *e);
int64_t  bson_a_int64(const bson_elem *e);
bool     bson_verdadero(const bson_elem *e);    /* números != 0, true, lo demás que no sea null */
bool     elem_es_texto(const bson_elem *e, const char *s);
bool     nombre_es(const bson_elem *e, const char *s);

/* Orden canónico de MongoDB: MinKey < null < números < cadenas < documentos
 * < arrays < binario < ObjectId < bool < fecha < timestamp < regex < MaxKey. */
int      bson_rango(uint8_t tipo);
int      bson_comparar(uint8_t ta, const uint8_t *va, uint8_t tb, const uint8_t *vb);
static inline int elem_comparar(const bson_elem *a, const bson_elem *b) { return bson_comparar(a->tipo, a->v, b->tipo, b->v); }
uint32_t bson_hash(uint8_t tipo, const uint8_t *v, uint32_t vlen);

/* Caminos con punto ("direccion.ciudad"): visita cada valor; un array al
 * final se visita entero y también elemento a elemento (de_array = true),
 * y los arrays del medio se recorren buscando el resto del camino en cada
 * documento. La función devuelve true para parar. */
typedef bool (*bson_visita_fn)(void *ctx, const bson_elem *e, bool de_array);
bool bson_camino(const uint8_t *doc, const char *camino, uint32_t clen, bson_visita_fn f, void *ctx);
/* El primer valor del camino, sin recorrer arrays (salvo índices numéricos). */
bool bson_camino_simple(const uint8_t *doc, const char *camino, uint32_t clen, bson_elem *e);

/* ------------------------------------------------------------ árbol (para modificar documentos) */
typedef struct lmk_nodo {
    const char      *nombre;
    uint32_t         nlen;
    uint8_t          tipo;
    bool             expandido;     /* documento o array con los hijos en "hijos" */
    const uint8_t   *v;             /* valor crudo (escalar, o documento sin expandir) */
    uint32_t         vlen;
    struct lmk_nodo *hijos;
    uint32_t         n, cap;
} lmk_nodo;

lmk_nodo *arbol_desde(lmk_arena *a, const uint8_t *doc);
bool      arbol_expandir(lmk_arena *a, lmk_nodo *n);
int       arbol_hijo(lmk_nodo *n, const char *nombre, uint32_t nlen);    /* -1 = no está */
lmk_nodo *arbol_anadir(lmk_arena *a, lmk_nodo *n, const char *nombre, uint32_t nlen);   /* al final */
void      arbol_quitar(lmk_nodo *n, uint32_t i);
void      arbol_poner(lmk_nodo *dst, const bson_elem *e);   /* el valor de e, sin copiar */
void      arbol_escribir(lmk_nodo *raiz, lmk_buf *b);       /* serializa el documento */
void      arbol_escribir_valor(lmk_nodo *n, lmk_buf *b);     /* solo el valor */

/* ------------------------------------------------------------ expresiones regulares */
#define RX_MAX 96
typedef struct {
    uint8_t  op, cuant;     /* RX_*; cuantificador 0 = uno, '*', '+', '?' */
    uint8_t  c;
    uint8_t  clase[32];     /* bits de los 256 bytes */
} lmk_rx_item;
typedef struct {
    lmk_rx_item it[RX_MAX];
    int  n;
    bool ancla_ini, ignorar, multilinea;
} lmk_regex;
bool regex_compilar(lmk_regex *r, const char *pat, uint32_t len, const char *opciones, lmk_error *err);
bool regex_buscar(const lmk_regex *r, const char *s, uint32_t len);

/* ------------------------------------------------------------ consultas */
bool consulta_validar(const uint8_t *filtro, lmk_error *err);
bool consulta_coincide(const uint8_t *doc, const uint8_t *filtro);
bool consulta_valor_cumple(const bson_elem *valor, const uint8_t *ops);   /* {$gt: 3, ...} contra un valor */
void consulta_olvidar_regex(void);    /* al empezar cada consulta: la caché de regex apunta a su filtro */

bool proyeccion_validar(const uint8_t *proy, lmk_error *err);
void proyectar(const uint8_t *doc, const uint8_t *proy, lmk_buf *out);   /* escribe un documento */

bool orden_validar(const uint8_t *orden, lmk_error *err);
void orden_clave(const uint8_t *doc, const uint8_t *orden, lmk_buf *out); /* documento con los valores */
int  orden_comparar(const uint8_t *ka, const uint8_t *kb, const uint8_t *orden);
void ordenar_u32(uint32_t *v, uint32_t n, int (*cmp)(void *ctx, uint32_t a, uint32_t b), void *ctx);   /* estable */

/* ------------------------------------------------------------ actualizaciones */
bool actualizacion_con_operadores(const uint8_t *u);
bool actualizacion_validar(const uint8_t *u, lmk_error *err);
/* Aplica los operadores; out recibe el documento nuevo; *cambio = false si quedó igual. */
bool actualizar_doc(const uint8_t *doc, const uint8_t *u, lmk_buf *out, bool *cambio, lmk_error *err);
bool reemplazar_doc(const uint8_t *doc, const uint8_t *nuevo, lmk_buf *out, lmk_error *err);
bool upsert_doc(const uint8_t *filtro, const uint8_t *u, lmk_buf *out, lmk_error *err);   /* sin _id: lo pone el motor */

/* ------------------------------------------------------------ índices */
typedef struct lmk_inodo lmk_inodo;
typedef struct {
    char       nombre[LMK_NOMBRE_MAX];
    char       campo[128];
    int        dir;          /* 1 o -1 */
    bool       unico;
    lmk_inodo *raiz;
    uint32_t   n;            /* entradas */
    uint32_t   semilla;
} lmk_indice;
typedef bool (*indice_fn)(void *ctx, uint32_t ranura);   /* true = parar */
bool indice_insertar(lmk_indice *ix, const bson_elem *clave, uint32_t ranura);
void indice_borrar(lmk_indice *ix, const bson_elem *clave, uint32_t ranura);
bool indice_hay_otro(lmk_indice *ix, const bson_elem *clave, uint32_t ranura);
/* Entradas con lo <= clave <= hi (lo/hi NULL = sin límite); incl = incluir el extremo. */
uint32_t indice_recorrer(lmk_indice *ix, const bson_elem *lo, bool lo_incl, const bson_elem *hi, bool hi_incl,
                         indice_fn f, void *ctx);
void indice_vaciar(lmk_indice *ix);
/* Las claves de un documento (sin repetir; null si falta): f(ctx, &clave). */
typedef bool (*clave_fn)(void *ctx, const bson_elem *clave);
bool indice_claves(const lmk_indice *ix, const uint8_t *doc, clave_fn f, void *ctx);

/* ------------------------------------------------------------ almacenamiento y colecciones */
typedef struct lmk_motor lmk_motor;
typedef struct lmk_base lmk_base;
typedef struct lmk_coleccion lmk_coleccion;

typedef struct {
    uint8_t  *doc;           /* en memoria, o NULL (se lee del disco) */
    uint32_t  len;           /* del documento */
    uint32_t  off;           /* del documento en el archivo */
    uint32_t  rlen;          /* del registro entero */
    uint32_t  hash;          /* del _id */
    uint32_t  gen;           /* cambia cada vez que la ranura recibe otro documento */
    uint8_t   viva;
} lmk_ranura;

struct lmk_coleccion {
    char        nombre[LMK_NOMBRE_MAX + 1];
    lmk_base   *base;
    char        ruta[384];
    lmk_ranura *r;
    uint32_t    nr, capr;
    uint32_t   *libres;
    uint32_t    nlibres, caplibres;
    uint32_t    vivas;
    /* Tabla hash del _id: direccionamiento abierto, ranura + 1 (0 = vacío,
     * 0xFFFFFFFF = borrado). */
    uint32_t   *tabla;
    uint32_t    captabla, ntabla, borrados_tabla;
    lmk_indice *ix[LMK_INDICES_MAX];
    int         nix;
    int         fd;          /* -1 = sin archivo todavía */
    uint32_t    fin;         /* tamaño del archivo (+ lo pendiente) */
    uint32_t    vivos;       /* bytes de registros vivos */
    lmk_buf     pendiente;   /* registros aún no escritos */
    uint32_t    pendiente_off;
    uint32_t    sin_sync;    /* escrituras desde la última sincronización */
    lmk_buf     temporal;    /* documento leído del disco */
    uint32_t    generacion;
    uint32_t    gen_sig;     /* para lmk_ranura.gen: los cursores detectan una ranura reutilizada */
    bool        cargada;     /* el archivo ya se leyó */
    bool        error_disco; /* una escritura falló: la colección queda de solo lectura */
};

struct lmk_base {
    char            nombre[LMK_NOMBRE_MAX + 1];
    lmk_motor      *motor;
    char            ruta[320];
    lmk_coleccion **col;
    uint32_t        ncol, capcol;
};

struct lmk_motor {
    char       datos[256];
    lmk_base **bases;
    uint32_t   nbases, capbases;
    uint64_t   mem_limite, mem_usada;
    bool       sync;              /* sincronizar tras cada escritura */
    uint32_t   sync_cada;         /* o cada N escrituras (y cada segundo) */
    uint32_t   contador_oid;
    uint8_t    azar_oid[5];
    uint32_t   generacion;
    void     (*aviso)(const char *texto);
    /* estadísticas */
    uint64_t   insertados, actualizados, borrados, consultas;
};

typedef struct {
    const char *plan;        /* "COLLSCAN", "IDHACK" o "IXSCAN" */
    const char *indice;
    uint32_t    examinados, claves;
} lmk_plan;

bool motor_abrir(lmk_motor *m, const char *datos, uint64_t mem_limite, bool sync, lmk_error *err);
void motor_cerrar(lmk_motor *m);
void motor_sincronizar(lmk_motor *m, bool forzar);   /* escribe lo pendiente y, si toca, sincroniza */
void motor_oid(lmk_motor *m, uint8_t oid[12]);
bool nombre_valido(const char *s, bool coleccion);
lmk_base *motor_base(lmk_motor *m, const char *nombre, bool crear, lmk_error *err);
bool motor_borrar_base(lmk_motor *m, lmk_base *b, lmk_error *err);
lmk_coleccion *base_coleccion(lmk_base *b, const char *nombre, bool crear, lmk_error *err);
bool base_borrar_coleccion(lmk_base *b, lmk_coleccion *c, lmk_error *err);
uint64_t base_tamano(lmk_base *b);

const uint8_t *coleccion_doc(lmk_coleccion *c, uint32_t ranura);   /* vale hasta la próxima llamada */
bool coleccion_insertar(lmk_coleccion *c, const uint8_t *doc, uint8_t *id_tipo, lmk_buf *id, lmk_error *err);
bool coleccion_cambiar(lmk_coleccion *c, uint32_t ranura, const uint8_t *nuevo, lmk_error *err);
bool coleccion_quitar(lmk_coleccion *c, uint32_t ranura, lmk_error *err);
/* Las ranuras que cumplen el filtro (lmk_aloca; el llamador libera).
 * max = 0: todas. */
uint32_t *coleccion_buscar(lmk_coleccion *c, const uint8_t *filtro, uint32_t max, uint32_t *n,
                           lmk_plan *plan, lmk_error *err);
int  coleccion_indice_de(lmk_coleccion *c, const char *nombre);
bool coleccion_crear_indice(lmk_coleccion *c, const char *nombre, const char *campo, int dir, bool unico,
                            bool *ya_estaba, lmk_error *err);
bool coleccion_borrar_indice(lmk_coleccion *c, const char *nombre, lmk_error *err);
bool coleccion_compactar(lmk_coleccion *c, lmk_error *err);
bool coleccion_volcar(lmk_coleccion *c);    /* escribe lo pendiente */
uint32_t lmk_crc32(uint32_t crc, const uint8_t *p, uint32_t n);

/* ------------------------------------------------------------ agregación */
/* Ejecuta la tubería; los documentos de salida van, uno tras otro, en "out"
 * (cada uno es un documento BSON completo). */
bool agregar(lmk_coleccion *c, const uint8_t *tuberia, lmk_buf *out, uint32_t *ndocs, lmk_error *err);

/* ------------------------------------------------------------ servidor (órdenes del protocolo) */
#define LMK_OP_PETICION  1
#define LMK_OP_RESPUESTA 2

typedef struct lmk_servidor lmk_servidor;
typedef struct {
    bool     autenticado;
    uint8_t  nonce[24];      /* el del servidor, para el desafío */
    char     usuario[64];
    bool     pendiente_auth;
} lmk_sesion;

lmk_servidor *servidor_nuevo(lmk_motor *m, const char *usuario, const char *clave);
void servidor_liberar(lmk_servidor *s);
/* Ejecuta una orden (documento BSON validado) y escribe la respuesta
 * (un documento BSON) en "resp". Devuelve false si hay que cerrar el servidor
 * (orden shutdown). */
bool servidor_orden(lmk_servidor *s, lmk_sesion *ses, const uint8_t *orden, lmk_buf *resp);
void servidor_tic(lmk_servidor *s);   /* cada segundo: caducar cursores, sincronizar */

/* HMAC-SHA256 (autenticación) */
void lmk_hmac_sha256(const uint8_t *clave, uint32_t nclave, const uint8_t *msg, uint32_t nmsg, uint8_t out[32]);

#endif
