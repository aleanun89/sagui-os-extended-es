// SPDX-License-Identifier: GPL-2.0-or-later
/* acpi_pss.c - lector mínimo de _PSS, _PSD y _PCT sin intérprete AML (D177).
 *
 * Archivo sin acceso a hardware, para probarlo en el anfitrión
 * (tests/h147_eist_pss.py lo compila con -DSAGUI_HOST, también con
 * -fsanitize=address,undefined y tablas estropeadas al azar). En el kernel
 * corre antes de la paginación (eist_antiguo.c), con toda la memoria física a
 * la vista: cualquier lectura fuera de una tabla tumbaría el arranque, así que
 * cada acceso se comprueba contra el final de la tabla o del paquete.
 *
 * Qué hace:
 *   1. Recorre la estructura del AML (Scope, Device, Processor, Name, Method,
 *      OperationRegion, Field, If/Else...) sin ejecutar nada, y anota los
 *      objetos que interesan con su ruta absoluta: procesadores, Name con
 *      paquetes, los métodos _PSS/_PSD/_PCT, las OperationRegion de memoria y
 *      los Load(). Los cuerpos de los demás métodos se saltan por su longitud
 *      (solo se buscan en ellos OperationRegion y Load).
 *   2. Encuentra las SSDT que la BIOS carga con Load() desde una
 *      OperationRegion (esquema de Intel de 2005-2010: el paquete \SSDT con
 *      «CPU0IST », dirección, longitud...): la región con argumentos
 *      constantes o DerefOf(Index(SSDT, n)), y además cualquier trío
 *      (cadena, dirección, longitud) de un paquete. Solo se lee lo que
 *      mapear() permite y tiene firma «SSDT» y suma correctas; no se ejecuta
 *      el _PDC/_OSC que decide si se cargarían.
 *   3. Para cada procesador, el _PSS: un Name con paquete literal, o un
 *      Method cuyos Return devuelven un paquete literal o el nombre de otro
 *      (NPSS/SPSS, \_PR.CPU0._PSS). Igual con _PSD y _PCT.
 * Lo que no entiende (un paquete calculado, Return(Local0), referencias
 * dentro del paquete) queda como ilegible, nunca adivinado. */
#include "acpi_pss.h"
#ifdef SAGUI_HOST
#include <string.h>
#else
#include "texto.h"
#endif

#define MAXSEG     8
#define MAX_OBJ    640
#define MAX_LOAD   32
#define MAX_CAND   6
#define PROF_MAX   24

typedef struct {
    uint32_t seg[MAXSEG];
    uint8_t  n;
} caminho_t;

typedef struct {
    uint8_t  raiz, sobe, n;
    uint32_t seg[MAXSEG];
} nome_t;

enum { OBJ_NOME = 1, OBJ_METODO, OBJ_PROC, OBJ_DISP, OBJ_REGIAO };

typedef struct {
    caminho_t c;
    uint8_t   tipo, tab, proc_id, reg_ok;
    const uint8_t *ini, *fim;   /* Name: el objeto de datos; Method: el cuerpo; Region: los argumentos */
} obj_t;

typedef struct {
    caminho_t esc;
    nome_t    nome;
} load_t;

typedef struct {
    const uint8_t *p, *fim;     /* un paquete (PackageOp) */
    uint8_t tab;
} cand_t;

static acpi_pss_t *R;
static pss_mapear_t mapear_fn;
static const uint8_t *tab_ptr[PSS_MAX_TABELAS];
static obj_t obj[MAX_OBJ];
static uint32_t nobj;
static load_t loads[MAX_LOAD];
static uint32_t nloads;
static uint8_t tab_atual;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

#define SEG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define SEG_PSS SEG('_', 'P', 'S', 'S')
#define SEG_PSD SEG('_', 'P', 'S', 'D')
#define SEG_PCT SEG('_', 'P', 'C', 'T')
#define SEG_HID SEG('_', 'H', 'I', 'D')
#define SEG_UID SEG('_', 'U', 'I', 'D')

/* ------------------------------------------------------------ AML básico */

/* PkgLength: devuelve lo que sigue a la longitud y, en *fim_pkg, el final
 * del paquete (la longitud cuenta desde su propio primer byte). */
static const uint8_t *pkglen(const uint8_t *p, const uint8_t *fim, const uint8_t **fim_pkg)
{
    if (!p || p >= fim) return NULL;
    uint32_t n = p[0] >> 6, len;
    if ((uint32_t)(fim - p) < n + 1) return NULL;
    if (n == 0) {
        len = p[0] & 0x3F;
    } else {
        if (p[0] & 0x30) return NULL;   /* con bytes extra, los bits 5-4 son 0 */
        len = p[0] & 0x0F;
        for (uint32_t i = 1; i <= n; i++) len |= (uint32_t)p[i] << (4 + 8 * (i - 1));
    }
    if (len < n + 1 || len > (uint32_t)(fim - p)) return NULL;
    *fim_pkg = p + len;
    return p + 1 + n;
}

static bool seg_ok(const uint8_t *s)
{
    if (!((s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_')) return false;
    for (int i = 1; i < 4; i++)
        if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return false;
    return true;
}

static bool inicio_nome(uint8_t c)
{
    return c == '\\' || c == '^' || c == 0x2E || c == 0x2F || (c >= 'A' && c <= 'Z') || c == '_';
}

static const uint8_t *ler_nome(const uint8_t *p, const uint8_t *fim, nome_t *nm)
{
    memset(nm, 0, sizeof *nm);
    if (!p || p >= fim) return NULL;
    if (*p == '\\') {
        nm->raiz = 1;
        p++;
    } else {
        while (p < fim && *p == '^') {
            if (++nm->sobe > MAXSEG) return NULL;
            p++;
        }
    }
    if (p >= fim) return NULL;
    uint32_t n;
    if (*p == 0x00) return p + 1;            /* NullName */
    if (*p == 0x2E) { n = 2; p++; }          /* DualNamePrefix */
    else if (*p == 0x2F) {                   /* MultiNamePrefix */
        if (fim - p < 2) return NULL;
        n = p[1];
        p += 2;
        if (n == 0 || n > MAXSEG) return NULL;
    } else n = 1;
    if ((uint32_t)(fim - p) < 4 * n) return NULL;
    for (uint32_t i = 0; i < n; i++, p += 4) {
        if (!seg_ok(p)) return NULL;
        nm->seg[i] = le32(p);
    }
    nm->n = (uint8_t)n;
    return p;
}

/* La ruta absoluta de un nombre declarado en el ámbito esc. */
static bool juntar(const caminho_t *esc, const nome_t *nm, caminho_t *out)
{
    if (nm->raiz) out->n = 0;
    else {
        *out = *esc;
        if (nm->sobe > out->n) return false;
        out->n = (uint8_t)(out->n - nm->sobe);
    }
    if (out->n + nm->n > MAXSEG) return false;
    for (uint32_t i = 0; i < nm->n; i++) out->seg[out->n++] = nm->seg[i];
    return true;
}

static bool mesmo(const caminho_t *a, const caminho_t *b)
{
    if (a->n != b->n) return false;
    for (uint32_t i = 0; i < a->n; i++)
        if (a->seg[i] != b->seg[i]) return false;
    return true;
}

static obj_t *achar(const caminho_t *c, int tipo)
{
    for (uint32_t i = 0; i < nobj; i++)
        if ((!tipo || obj[i].tipo == tipo) && mesmo(&obj[i].c, c)) return &obj[i];
    return NULL;
}

/* Resolución de un nombre usado en el ámbito esc: con un solo segmento y sin
 * prefijo se busca hacia arriba (regla de ACPI); si no, la ruta tal cual. */
static obj_t *resolver(const caminho_t *esc, const nome_t *nm)
{
    caminho_t c;
    if (nm->n == 1 && !nm->raiz && !nm->sobe) {
        c = *esc;
        for (;;) {
            if (c.n >= MAXSEG) return NULL;
            c.seg[c.n] = nm->seg[0];
            c.n++;
            obj_t *o = achar(&c, 0);
            if (o) return o;
            c.n--;
            if (c.n == 0) return NULL;
            c.n--;
        }
    }
    if (!juntar(esc, nm, &c)) return NULL;
    return achar(&c, 0);
}

static obj_t *anotar(const caminho_t *c, int tipo, const uint8_t *ini, const uint8_t *fim)
{
    obj_t *o = achar(c, tipo);
    if (o) return o;   /* la primera declaración manda (los dos lados de un If, tablas repetidas) */
    if (nobj >= MAX_OBJ) {
        R->objetos_perdidos++;
        return NULL;
    }
    o = &obj[nobj++];
    memset(o, 0, sizeof *o);
    o->c = *c;
    o->tipo = (uint8_t)tipo;
    o->tab = tab_atual;
    o->ini = ini;
    o->fim = fim;
    R->objetos = nobj;
    return o;
}

/* Un entero constante (ZeroOp, OneOp, OnesOp, Byte/Word/DWord/QWordPrefix). */
static const uint8_t *ler_int(const uint8_t *p, const uint8_t *fim, uint64_t *v)
{
    if (!p || p >= fim) return NULL;
    uint32_t n;
    switch (p[0]) {
    case 0x00: *v = 0; return p + 1;
    case 0x01: *v = 1; return p + 1;
    case 0xFF: *v = 0xFFFFFFFFu; return p + 1;   /* Ones, en tablas de revisión 1 (32 bits) */
    case 0x0A: n = 1; break;
    case 0x0B: n = 2; break;
    case 0x0C: n = 4; break;
    case 0x0E: n = 8; break;
    default: return NULL;
    }
    if ((uint32_t)(fim - p) < n + 1) return NULL;
    *v = 0;
    for (uint32_t i = 0; i < n; i++) *v |= (uint64_t)p[1 + i] << (8 * i);
    return p + 1 + n;
}

static const uint8_t *ler_int32(const uint8_t *p, const uint8_t *fim, uint32_t *v)
{
    uint64_t x;
    p = ler_int(p, fim, &x);
    if (!p || x > 0xFFFFFFFFu) return NULL;
    *v = (uint32_t)x;
    return p;
}

static const uint8_t *pular_arg(const uint8_t *p, const uint8_t *fim, int prof);

static const uint8_t *pular_alvo(const uint8_t *p, const uint8_t *fim, int prof)
{
    if (!p || p >= fim) return NULL;
    if (*p == 0x00) return p + 1;
    return pular_arg(p, fim, prof);
}

static const uint8_t *pular_n(const uint8_t *p, const uint8_t *fim, int n, int prof)
{
    while (p && n-- > 0) p = pular_arg(p, fim, prof);
    return p;
}

static void anotar_load(const caminho_t *esc, const uint8_t *p, const uint8_t *fim)
{
    nome_t nm;
    if (!ler_nome(p, fim, &nm) || !nm.n || nloads >= MAX_LOAD) return;
    loads[nloads].esc = *esc;
    loads[nloads].nome = nm;
    nloads++;
}

/* Salta un TermArg (o un SuperName, o una instrucción de un método) sin
 * entenderlo. Una invocación de método se toma sin argumentos: si los tiene,
 * el lector se pierde y lo notará el que llama (devuelve NULL más adelante o
 * el paquete no tendrá la forma esperada). */
static const uint8_t *pular_arg(const uint8_t *p, const uint8_t *fim, int prof)
{
    const uint8_t *e;
    uint64_t v;
    nome_t nm;
    if (!p || p >= fim || prof > PROF_MAX) return NULL;
    uint8_t op = p[0];
    if (op == 0x00 || op == 0x01 || op == 0xFF || op == 0x0A || op == 0x0B || op == 0x0C || op == 0x0E)
        return ler_int(p, fim, &v);
    if (op == 0x0D) {                                        /* String */
        for (const uint8_t *q = p + 1; q < fim; q++)
            if (*q == 0) return q + 1;
        return NULL;
    }
    if (op == 0x11 || op == 0x12 || op == 0x13) {            /* Buffer, Package, VarPackage */
        if (!pkglen(p + 1, fim, &e)) return NULL;
        return e;
    }
    if (op >= 0x60 && op <= 0x6E) return p + 1;              /* Local0-7, Arg0-6 */
    if (op == 0xA3 || op == 0xA5 || op == 0xCC) return p + 1;   /* Noop, Break, BreakPoint */
    if (inicio_nome(op)) return ler_nome(p, fim, &nm);
    prof++;
    switch (op) {
    case 0x71: case 0x75: case 0x76: case 0x83: case 0x87: case 0x8E: case 0x92: case 0xA4:
        return pular_arg(p + 1, fim, prof);                  /* RefOf, Inc, Dec, DerefOf, SizeOf, ObjectType, LNot, Return */
    case 0x90: case 0x91: case 0x93: case 0x94: case 0x95: case 0x70: case 0x86:
        return pular_n(p + 1, fim, 2, prof);                 /* LAnd, LOr, LEqual, LGreater, LLess, Store, Notify */
    case 0x72: case 0x73: case 0x74: case 0x77: case 0x79: case 0x7A: case 0x7B: case 0x7C:
    case 0x7D: case 0x7E: case 0x7F: case 0x84: case 0x85: case 0x88: case 0x9C:
        return pular_alvo(pular_n(p + 1, fim, 2, prof), fim, prof);   /* aritmética, Index, ToString */
    case 0x78:                                               /* Divide: dos operandos, dos destinos */
        return pular_alvo(pular_alvo(pular_n(p + 1, fim, 2, prof), fim, prof), fim, prof);
    case 0x80: case 0x81: case 0x82: case 0x96: case 0x97: case 0x98: case 0x99:
        return pular_alvo(pular_arg(p + 1, fim, prof), fim, prof);    /* Not, FindSetBit, To* */
    case 0x9D:                                               /* CopyObject */
        return pular_arg(pular_arg(p + 1, fim, prof), fim, prof);
    case 0x9E:                                               /* Mid */
        return pular_alvo(pular_n(p + 1, fim, 3, prof), fim, prof);
    case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8F:  /* Create*Field */
        return ler_nome(pular_n(p + 1, fim, 2, prof), fim, &nm);
    case 0x89: {                                             /* Match */
        const uint8_t *q = pular_arg(p + 1, fim, prof);
        if (!q || q >= fim) return NULL;
        q = pular_arg(q + 1, fim, prof);
        if (!q || q >= fim) return NULL;
        return pular_n(q + 1, fim, 2, prof);
    }
    case 0xA0: case 0xA1: case 0xA2:                         /* If, Else, While: enteros */
        if (!pkglen(p + 1, fim, &e)) return NULL;
        return e;
    case 0x5B:
        if (fim - p < 2) return NULL;
        switch (p[1]) {
        case 0x30: case 0x31: return p + 2;                  /* Revision, Debug */
        case 0x12: return pular_alvo(pular_arg(p + 2, fim, prof), fim, prof);   /* CondRefOf */
        case 0x21: case 0x22: case 0x24: case 0x25: case 0x26: case 0x27:
            return pular_arg(p + 2, fim, prof);              /* Stall, Sleep, Signal, Reset, Release */
        case 0x23: {                                         /* Acquire: mutex + timeout (Word) */
            const uint8_t *q = pular_arg(p + 2, fim, prof);
            return q && fim - q >= 2 ? q + 2 : NULL;
        }
        case 0x13: return ler_nome(pular_n(p + 2, fim, 3, prof), fim, &nm);   /* CreateField */
        case 0x20: return pular_alvo(ler_nome(p + 2, fim, &nm), fim, prof);   /* Load */
        case 0x2A: return pular_arg(p + 2, fim, prof);       /* Unload */
        default: return NULL;
        }
    default:
        return NULL;
    }
}

/* ------------------------------------------------------------ paquetes */

/* El principio de los elementos de un Package/VarPackage; *fim_pkg y *decl. */
static const uint8_t *pacote(const uint8_t *p, const uint8_t *fim, const uint8_t **fim_pkg, uint32_t *decl)
{
    if (!p || p >= fim || (p[0] != 0x12 && p[0] != 0x13)) return NULL;
    const uint8_t *q = pkglen(p + 1, fim, fim_pkg);
    if (!q || q >= *fim_pkg) return NULL;
    if (p[0] == 0x12) {
        *decl = *q;
        return q + 1;
    }
    return ler_int32(q, *fim_pkg, decl);
}

/* _PSS: Package de paquetes de 6 enteros. Devuelve cuántos estados o -1. */
static int ler_pss(const uint8_t *p, const uint8_t *fim, pss_estado_t *est)
{
    const uint8_t *e, *e2;
    uint32_t decl, d2, n = 0;
    const uint8_t *q = pacote(p, fim, &e, &decl);
    if (!q) return -1;
    while (q < e) {
        if (n >= PSS_MAX_ESTADOS) return -1;
        const uint8_t *s = pacote(q, e, &e2, &d2);
        if (!s || d2 != 6) return -1;
        uint32_t v[6];
        for (int k = 0; k < 6; k++)
            if (!(s = ler_int32(s, e2, &v[k]))) return -1;
        if (s != e2) return -1;
        est[n].mhz = v[0];
        est[n].mw = v[1];
        est[n].lat_us = v[2];
        est[n].lat_bm_us = v[3];
        est[n].controle = v[4];
        est[n].status = v[5];
        n++;
        q = e2;
    }
    if (n == 0 || decl != n) return -1;
    return (int)n;
}

/* _PSD: Package(){ Package(){ 5, 0, dominio, tipo, num } }. */
static bool ler_psd(const uint8_t *p, const uint8_t *fim, uint32_t *dom, uint32_t *tipo, uint32_t *num)
{
    const uint8_t *e, *e2;
    uint32_t decl, d2, v[5];
    const uint8_t *q = pacote(p, fim, &e, &decl);
    if (!q || decl < 1) return false;
    const uint8_t *s = pacote(q, e, &e2, &d2);
    if (!s || d2 != 5) return false;
    for (int k = 0; k < 5; k++)
        if (!(s = ler_int32(s, e2, &v[k]))) return false;
    if (v[0] != 5) return false;
    *dom = v[2];
    *tipo = v[3];
    *num = v[4];
    return true;
}

/* _PCT: Package(){ Buffer(registro de control), Buffer(registro de estado) }.
 * Devuelve el espacio de direcciones del de control (0x7F = FFixedHW) o -1. */
static int ler_pct(const uint8_t *p, const uint8_t *fim)
{
    const uint8_t *e, *eb;
    uint32_t decl, tam;
    const uint8_t *q = pacote(p, fim, &e, &decl);
    if (!q || decl != 2 || q >= e || *q != 0x11) return -1;
    const uint8_t *b = pkglen(q + 1, e, &eb);
    if (!b) return -1;
    b = ler_int32(b, eb, &tam);
    /* Generic Register Descriptor: 0x82, longitud 0x000C, espacio, ... */
    if (!b || eb - b < 15 || b[0] != 0x82 || b[1] != 0x0C || b[2] != 0) return -1;
    return b[3];
}

/* ------------------------------------------------------------ recorrido */

static void lista(const uint8_t *p, const uint8_t *fim, const caminho_t *esc, int prof);

/* En los cuerpos de los métodos que no se leen enteros: OperationRegion y Load,
 * buscados byte a byte (el _PDC de Intel declara la región y la carga ahí). */
static void varrer_metodo(const uint8_t *p, const uint8_t *fim, const caminho_t *esc)
{
    for (; p + 2 < fim; p++) {
        if (p[0] != 0x5B) continue;
        if (p[1] == 0x20) anotar_load(esc, p + 2, fim);
        else if (p[1] == 0x80) {
            nome_t nm;
            caminho_t c;
            const uint8_t *q = ler_nome(p + 2, fim, &nm);
            if (q && nm.n && q < fim && *q == 0x00 && juntar(esc, &nm, &c)) anotar(&c, OBJ_REGIAO, q + 1, fim);
        }
    }
}

static bool interessa(uint32_t seg)
{
    return seg == SEG_PSS || seg == SEG_PSD || seg == SEG_PCT;
}

/* Una lista de términos (el cuerpo de una tabla, Scope, Device...). Si algo no
 * se entiende, se abandona el resto de esta lista (no de las de fuera, que
 * conocen su propia longitud). */
static void lista(const uint8_t *p, const uint8_t *fim, const caminho_t *esc, int prof)
{
    const uint8_t *e, *q;
    nome_t nm;
    caminho_t c;
    if (prof > PROF_MAX) { R->aml_incompleto++; return; }
    while (p && p < fim) {
        uint8_t op = p[0];
        if (op == 0x08) {                                    /* Name */
            q = ler_nome(p + 1, fim, &nm);
            const uint8_t *d = q;
            q = pular_arg(q, fim, 0);
            if (!q || !nm.n || !juntar(esc, &nm, &c)) break;
            uint32_t ult = c.seg[c.n - 1];
            if (*d == 0x12 || *d == 0x13 || ult == SEG_HID || ult == SEG_UID || interessa(ult))
                anotar(&c, OBJ_NOME, d, q);
            p = q;
        } else if (op == 0x10) {                             /* Scope */
            q = pkglen(p + 1, fim, &e);
            q = ler_nome(q, e, &nm);
            if (!q || !juntar(esc, &nm, &c)) break;
            lista(q, e, &c, prof + 1);
            p = e;
        } else if (op == 0x14) {                             /* Method */
            q = pkglen(p + 1, fim, &e);
            q = ler_nome(q, e, &nm);
            if (!q || q >= e || !nm.n || !juntar(esc, &nm, &c)) break;
            if (interessa(c.seg[c.n - 1])) anotar(&c, OBJ_METODO, q + 1, e);
            varrer_metodo(q + 1, e, &c);
            p = e;
        } else if (op == 0x06) {                             /* Alias */
            q = ler_nome(p + 1, fim, &nm);
            p = ler_nome(q, fim, &nm);
        } else if (op == 0x15) {                             /* External */
            q = ler_nome(p + 1, fim, &nm);
            p = q && fim - q >= 2 ? q + 2 : NULL;
        } else if (op == 0xA0) {                             /* If: se leen las declaraciones de dentro */
            q = pkglen(p + 1, fim, &e);
            q = pular_arg(q, e, 0);
            if (!q) break;
            lista(q, e, esc, prof + 1);
            p = e;
        } else if (op == 0xA1) {                             /* Else */
            q = pkglen(p + 1, fim, &e);
            if (!q) break;
            lista(q, e, esc, prof + 1);
            p = e;
        } else if (op == 0x5B && fim - p >= 2) {
            uint8_t op2 = p[1];
            if (op2 == 0x82 || op2 == 0x85) {                /* Device, ThermalZone */
                q = pkglen(p + 2, fim, &e);
                q = ler_nome(q, e, &nm);
                if (!q || !nm.n || !juntar(esc, &nm, &c)) break;
                if (op2 == 0x82) anotar(&c, OBJ_DISP, q, e);
                lista(q, e, &c, prof + 1);
                p = e;
            } else if (op2 == 0x83) {                        /* Processor: nombre, id, PBLK (4), longitud (1) */
                q = pkglen(p + 2, fim, &e);
                q = ler_nome(q, e, &nm);
                if (!q || !nm.n || e - q < 6 || !juntar(esc, &nm, &c)) break;
                obj_t *o = anotar(&c, OBJ_PROC, q, e);
                if (o) o->proc_id = q[0];
                lista(q + 6, e, &c, prof + 1);
                p = e;
            } else if (op2 == 0x84) {                        /* PowerResource */
                q = pkglen(p + 2, fim, &e);
                q = ler_nome(q, e, &nm);
                if (!q || e - q < 3 || !juntar(esc, &nm, &c)) break;
                lista(q + 3, e, &c, prof + 1);
                p = e;
            } else if (op2 == 0x81 || op2 == 0x86 || op2 == 0x87) {   /* Field, IndexField, BankField */
                if (!pkglen(p + 2, fim, &e)) break;
                p = e;
            } else if (op2 == 0x80) {                        /* OperationRegion */
                q = ler_nome(p + 2, fim, &nm);
                if (!q || q >= fim || !nm.n || !juntar(esc, &nm, &c)) break;
                const uint8_t *args = q + 1;
                if (*q == 0x00) anotar(&c, OBJ_REGIAO, args, fim);   /* SystemMemory */
                p = pular_n(args, fim, 2, 0);
            } else if (op2 == 0x01) {                        /* Mutex */
                q = ler_nome(p + 2, fim, &nm);
                p = q && q < fim ? q + 1 : NULL;
            } else if (op2 == 0x02) {                        /* Event */
                p = ler_nome(p + 2, fim, &nm);
            } else if (op2 == 0x20) {                        /* Load en el cuerpo de la tabla */
                anotar_load(esc, p + 2, fim);
                p = pular_arg(p, fim, 0);
            } else {
                p = pular_arg(p, fim, 0);
            }
        } else {
            p = pular_arg(p, fim, 0);                        /* Store, Notify... en el cuerpo de la tabla */
        }
    }
    if (!p || p != fim) R->aml_incompleto++;
}

/* ------------------------------------------------------------ tablas */

static bool soma_ok(const uint8_t *t, uint32_t n)
{
    uint8_t s = 0;
    for (uint32_t i = 0; i < n; i++) s = (uint8_t)(s + t[i]);
    return s == 0;
}

static void copiar_txt(char *d, const uint8_t *s, uint32_t n)
{
    uint32_t i = 0;
    for (; i < n && s[i] >= 0x20 && s[i] < 0x7F; i++) d[i] = (char)s[i];
    while (i > 0 && d[i - 1] == ' ') i--;
    d[i] = 0;
}

/* Lee una tabla (DSDT/SSDT); devuelve false si no se puede o no es válida. */
static bool ler_tabela(uint32_t fis, uint32_t tam_max, bool dinamica)
{
    if (R->tabelas >= PSS_MAX_TABELAS) return false;
    for (uint32_t i = 0; i < R->tabelas; i++)
        if (R->tabela[i].fis == fis) return true;   /* ya leída */
    const uint8_t *h = mapear_fn(fis, 36, !dinamica);
    if (!h) return false;
    uint32_t tam = le32(h + 4);
    if (tam < 36 || tam > (1u << 20) || (tam_max && tam > tam_max)) return false;
    if (memcmp(h, "DSDT", 4) != 0 && memcmp(h, "SSDT", 4) != 0) return false;
    if (dinamica && memcmp(h, "SSDT", 4) != 0) return false;
    const uint8_t *t = mapear_fn(fis, tam, !dinamica);
    if (!t || !soma_ok(t, tam)) return false;
    uint32_t k = R->tabelas++;
    tab_ptr[k] = t;
    R->tabela[k].fis = fis;
    R->tabela[k].tam = tam;
    R->tabela[k].dinamica = dinamica;
    copiar_txt(R->tabela[k].assinatura, t, 4);
    copiar_txt(R->tabela[k].oem, t + 16, 8);
    if (dinamica) R->dinamicas++;
    /* Las declaraciones de _PSS en bytes: Name (0x08 [\\] _PSS) o Method
     * (0x14, PkgLength de 1 o 2 bytes, _PSS), para ver si se escapa alguna. */
    for (const uint8_t *p = t + 40; p + 4 <= t + tam; p++) {
        if (p[0] != '_' || p[1] != 'P' || p[2] != 'S' || p[3] != 'S') continue;
        if (p[-1] == 0x08 || (p[-1] == '\\' && p[-2] == 0x08) || (p[-2] == 0x14 && (p[-1] >> 6) == 0) ||
            (p[-3] == 0x14 && (p[-2] >> 6) == 1))
            R->pss_bytes++;
    }
    caminho_t raiz = { { 0 }, 0 };
    tab_atual = (uint8_t)k;
    lista(t + 36, t + tam, &raiz, 0);
    return true;
}

/* Métodos _PSS con Method(...) { ... Return(...) }: las cuentas de bytes
 * cuentan las de los métodos aparte (el patrón es 0x14 PkgLength _PSS). */
static void contar_pss_metodos(void)
{
    for (uint32_t i = 0; i < nobj; i++)
        if (obj[i].c.n && obj[i].c.seg[obj[i].c.n - 1] == SEG_PSS) R->pss_lidos++;
}

/* El valor de un argumento de OperationRegion: constante o DerefOf(Index(PAQ, k)). */
static bool valor_arg(const uint8_t *p, const uint8_t *fim, const caminho_t *esc, uint32_t *v, const uint8_t **prox)
{
    const uint8_t *q = ler_int32(p, fim, v);
    if (q) { *prox = q; return true; }
    if (!p || fim - p < 3 || p[0] != 0x83 || p[1] != 0x88) { *prox = pular_arg(p, fim, 0); return false; }
    nome_t nm;
    uint32_t k, decl;
    q = ler_nome(p + 2, fim, &nm);
    q = ler_int32(q, fim, &k);
    if (!q || q >= fim || *q != 0x00) { *prox = pular_arg(p, fim, 0); return false; }
    *prox = q + 1;
    obj_t *o = resolver(esc, &nm);
    if (!o || o->tipo != OBJ_NOME) return false;
    const uint8_t *e, *el = pacote(o->ini, o->fim, &e, &decl);
    for (uint32_t i = 0; el && el < e; i++) {
        if (i == k) return ler_int32(el, e, v) != NULL;
        el = pular_arg(el, e, 0);
    }
    return false;
}

/* Las direcciones ya rechazadas (la misma SSDT puede salir de la región y del paquete). */
static uint32_t rejeitadas[16];
static uint32_t nrejeitadas;

static void tentar_dinamica(uint32_t fis, uint32_t tam)
{
    if (fis < 0x1000 || tam < 36 || tam > (1u << 20)) return;
    for (uint32_t i = 0; i < R->tabelas; i++)
        if (R->tabela[i].fis == fis) return;
    for (uint32_t i = 0; i < nrejeitadas; i++)
        if (rejeitadas[i] == fis) return;
    if (ler_tabela(fis, tam, true)) return;
    R->rejeitadas++;
    if (nrejeitadas < sizeof rejeitadas / sizeof rejeitadas[0]) rejeitadas[nrejeitadas++] = fis;
}

/* Busca SSDT dinámicas: regiones cargadas con Load() y tríos de los paquetes. */
static void buscar_dinamicas(void)
{
    for (uint32_t i = 0; i < nobj; i++) {
        obj_t *o = &obj[i];
        if (o->tipo == OBJ_REGIAO) {
            bool carregada = false;
            for (uint32_t l = 0; l < nloads && !carregada; l++) {
                obj_t *r = resolver(&loads[l].esc, &loads[l].nome);
                carregada = r == o || (loads[l].nome.n && loads[l].nome.seg[loads[l].nome.n - 1] == o->c.seg[o->c.n - 1]);
            }
            if (!carregada) continue;
            caminho_t esc = o->c;
            esc.n--;
            uint32_t fis, tam;
            const uint8_t *q;
            if (valor_arg(o->ini, o->fim, &esc, &fis, &q) && valor_arg(q, o->fim, &esc, &tam, &q)) tentar_dinamica(fis, tam);
        } else if (o->tipo == OBJ_NOME && (o->ini[0] == 0x12 || o->ini[0] == 0x13)) {
            /* («CPU0IST », dirección, longitud) repetido */
            const uint8_t *e;
            uint32_t decl;
            const uint8_t *el = pacote(o->ini, o->fim, &e, &decl);
            while (el && el < e) {
                if (*el == 0x0D) {
                    const uint8_t *q = pular_arg(el, e, 0);
                    uint32_t fis = 0, tam = 0;
                    const uint8_t *q2 = ler_int32(q, e, &fis);
                    const uint8_t *q3 = ler_int32(q2, e, &tam);
                    if (q3 && q - el >= 3 && q - el <= 11) {   /* cadena de 1 a 9 caracteres */
                        tentar_dinamica(fis, tam);
                        el = q3;
                        continue;
                    }
                }
                el = pular_arg(el, e, 0);
            }
        }
    }
}

/* ------------------------------------------------------------ procesadores */

static void texto_caminho(char *d, uint32_t n, const caminho_t *c)
{
    uint32_t k = 0;
    if (n < 2) return;
    d[k++] = '\\';
    for (uint32_t i = 0; i < c->n && k + 6 < n; i++) {
        if (i) d[k++] = '.';
        for (int b = 0; b < 4; b++) d[k++] = (char)((c->seg[i] >> (8 * b)) & 0xFF);
    }
    d[k] = 0;
}

/* Los paquetes que puede devolver el objeto c: un Name con paquete, o los
 * Return de un método (literales o nombres, seguidos hasta prof 3). */
static int candidatos(const obj_t *o, cand_t *out, int qtd, int prof, bool *ilegivel, const obj_t **de);

static int retornos(const uint8_t *p, const uint8_t *fim, const caminho_t *esc, cand_t *out, int qtd, int prof,
                    bool *ilegivel, const obj_t **de, uint8_t tab)
{
    const uint8_t *e, *q;
    nome_t nm;
    while (p && p < fim) {
        uint8_t op = p[0];
        if (op == 0xA0 || op == 0xA2) {
            q = pkglen(p + 1, fim, &e);
            q = pular_arg(q, e, 0);
            if (!q) { *ilegivel = true; return qtd; }
            qtd = retornos(q, e, esc, out, qtd, prof, ilegivel, de, tab);
            p = e;
        } else if (op == 0xA1) {
            q = pkglen(p + 1, fim, &e);
            if (!q) { *ilegivel = true; return qtd; }
            qtd = retornos(q, e, esc, out, qtd, prof, ilegivel, de, tab);
            p = e;
        } else if (op == 0xA4) {
            q = p + 1;
            if (q < fim && (*q == 0x12 || *q == 0x13)) {
                if (qtd < MAX_CAND) {
                    out[qtd].p = q;
                    out[qtd].fim = fim;
                    out[qtd].tab = tab;
                    qtd++;
                }
            } else if (q < fim && inicio_nome(*q) && ler_nome(q, fim, &nm)) {
                obj_t *r = resolver(esc, &nm);
                if (r) qtd = candidatos(r, out, qtd, prof + 1, ilegivel, de);
                else *ilegivel = true;
            } else {
                *ilegivel = true;   /* Return(Local0), Return(DerefOf(...)): calculado */
            }
            p = pular_arg(q, fim, 0);
        } else {
            p = pular_arg(p, fim, 0);
        }
    }
    if (!p) *ilegivel = true;
    return qtd;
}

static int candidatos(const obj_t *o, cand_t *out, int qtd, int prof, bool *ilegivel, const obj_t **de)
{
    if (prof > 3) { *ilegivel = true; return qtd; }
    if (o->tipo == OBJ_NOME) {
        if (o->ini[0] != 0x12 && o->ini[0] != 0x13) { *ilegivel = true; return qtd; }
        if (qtd < MAX_CAND) {
            out[qtd].p = o->ini;
            out[qtd].fim = o->fim;
            out[qtd].tab = o->tab;
            qtd++;
            if (de && !*de) *de = o;
        }
        return qtd;
    }
    if (o->tipo == OBJ_METODO) return retornos(o->ini, o->fim, &o->c, out, qtd, prof, ilegivel, de, o->tab);
    *ilegivel = true;
    return qtd;
}

static bool estados_iguais(const pss_estado_t *a, const pss_estado_t *b, int n)
{
    return memcmp(a, b, (size_t)n * sizeof *a) == 0;
}

/* Nativo (FFixedHW): Control == Status en todos (el SMI de la BIOS usa índices). */
static bool nativo(const pss_estado_t *e, int n)
{
    for (int i = 0; i < n; i++)
        if (e[i].controle != e[i].status || !(e[i].controle & 0xFF00)) return false;
    return true;
}

static void ler_cpu(pss_cpu_t *cpu, const caminho_t *pc)
{
    caminho_t c = *pc;
    cand_t cand[MAX_CAND];
    static pss_estado_t est[MAX_CAND][PSS_MAX_ESTADOS];
    int nest[MAX_CAND];
    if (c.n >= MAXSEG) return;
    c.n++;

    /* _PSS */
    c.seg[c.n - 1] = SEG_PSS;
    obj_t *o = achar(&c, 0);
    if (o) {
        bool ileg = false;
        const obj_t *de = NULL;
        int n = candidatos(o, cand, 0, 0, &ileg, &de);
        int nv = 0, escolha = -1, escolha_cand = -1;
        for (int i = 0; i < n; i++) {
            int k = ler_pss(cand[i].p, cand[i].fim, est[nv]);
            if (k < 0) { ileg = true; continue; }
            bool repetido = false;
            for (int j = 0; j < nv; j++)
                if (nest[j] == k && estados_iguais(est[j], est[nv], k)) repetido = true;
            if (repetido) continue;
            nest[nv] = k;
            if (escolha < 0 || (!nativo(est[escolha], nest[escolha]) && nativo(est[nv], k))) {
                escolha = nv;
                escolha_cand = i;
            }
            nv++;
        }
        cpu->pss_variantes = (uint8_t)nv;
        cpu->pss_incerto = ileg;
        if (escolha < 0) {
            cpu->pss_origem = PSS_ILEGIVEL;
        } else {
            cpu->pss_origem = o->tipo == OBJ_NOME ? PSS_NOME : PSS_METODO;
            cpu->qtd = (uint8_t)nest[escolha];
            memcpy(cpu->est, est[escolha], (size_t)nest[escolha] * sizeof cpu->est[0]);
            uint8_t t = cand[escolha_cand].tab;
            cpu->pss_dinamica = R->tabela[t].dinamica;
            memcpy(cpu->pss_tabela, R->tabela[t].oem, sizeof cpu->pss_tabela);
            /* El objeto del que sale: el Name que contiene el paquete elegido. */
            const obj_t *quem = o->tipo == OBJ_NOME ? o : NULL;
            for (uint32_t i = 0; !quem && i < nobj; i++)
                if (obj[i].tipo == OBJ_NOME && obj[i].ini == cand[escolha_cand].p) quem = &obj[i];
            texto_caminho(cpu->pss_de, sizeof cpu->pss_de, quem ? &quem->c : &o->c);
        }
    }

    /* _PSD */
    c.seg[c.n - 1] = SEG_PSD;
    o = achar(&c, 0);
    if (o) {
        bool ileg = false;
        int n = candidatos(o, cand, 0, 0, &ileg, NULL);
        cpu->psd_dominios_iguais = 1;
        for (int i = 0; i < n; i++) {
            uint32_t d, t, k;
            if (!ler_psd(cand[i].p, cand[i].fim, &d, &t, &k)) { ileg = true; continue; }
            if (!cpu->psd_variantes) {
                cpu->psd_dominio = d;
                cpu->psd_tipo = t;
                cpu->psd_num = k;
            } else if (d != cpu->psd_dominio) cpu->psd_dominios_iguais = 0;
            cpu->psd_variantes++;
            cpu->psd_tipos |= t == PSD_SW_ALL ? PSD_M_SW_ALL : t == PSD_SW_ANY ? PSD_M_SW_ANY : t == PSD_HW_ALL ? PSD_M_HW_ALL : PSD_M_OUTRO;
        }
        cpu->psd_ilegivel = ileg;
    }

    /* _PCT */
    c.seg[c.n - 1] = SEG_PCT;
    o = achar(&c, 0);
    if (o) {
        bool ileg = false;
        int n = candidatos(o, cand, 0, 0, &ileg, NULL);
        for (int i = 0; i < n; i++) {
            int s = ler_pct(cand[i].p, cand[i].fim);
            if (s < 0) continue;
            cpu->pct_variantes++;
            cpu->pct_espacos |= s == 0x7F ? PCT_M_FFH : s == 0x01 ? PCT_M_IO : PCT_M_OUTRO;
        }
    }
}

/* ¿Es un Device con _HID "ACPI0007"? Devuelve su _UID en *uid. */
static bool disp_processador(const obj_t *d, uint8_t *uid)
{
    caminho_t c = d->c;
    if (c.n >= MAXSEG) return false;
    c.seg[c.n++] = SEG_HID;
    obj_t *h = achar(&c, OBJ_NOME);
    if (!h || h->fim - h->ini < 10 || h->ini[0] != 0x0D || memcmp(h->ini + 1, "ACPI0007", 9) != 0) return false;
    c.seg[c.n - 1] = SEG_UID;
    obj_t *u = achar(&c, OBJ_NOME);
    uint32_t v = 0;
    *uid = u && ler_int32(u->ini, u->fim, &v) ? (uint8_t)v : 0xFF;
    return true;
}

void acpi_pss_ler(acpi_pss_t *r, const uint32_t *tabelas, uint32_t qtd, pss_mapear_t mapear)
{
    memset(r, 0, sizeof *r);
    R = r;
    mapear_fn = mapear;
    nobj = nloads = nrejeitadas = 0;
    for (uint32_t i = 0; i < qtd; i++) ler_tabela(tabelas[i], 0, false);
    /* Las SSDT cargadas pueden cargar otras: unas pocas vueltas. */
    for (int volta = 0; volta < 3; volta++) {
        uint32_t antes = r->tabelas;
        buscar_dinamicas();
        if (r->tabelas == antes) break;
    }
    contar_pss_metodos();
    for (uint32_t i = 0; i < nobj; i++) {
        uint8_t uid = 0;
        if (obj[i].tipo == OBJ_DISP && !disp_processador(&obj[i], &uid)) continue;
        if (obj[i].tipo != OBJ_PROC && obj[i].tipo != OBJ_DISP) continue;
        r->cpus_total++;
        if (r->qtd_cpus >= PSS_MAX_CPUS) continue;
        pss_cpu_t *cpu = &r->cpu[r->qtd_cpus++];
        texto_caminho(cpu->caminho, sizeof cpu->caminho, &obj[i].c);
        cpu->tipo = obj[i].tipo == OBJ_PROC ? 1 : 2;
        cpu->id = obj[i].tipo == OBJ_PROC ? obj[i].proc_id : uid;
        ler_cpu(cpu, &obj[i].c);
    }
    R = NULL;
}

/* ------------------------------------------------------------ validación */

bool pss_core2(uint32_t m)
{
    return m == 0x0F || m == 0x16 || m == 0x17 || m == 0x1D;
}

/* FID en medios: bits 12:8; en los Core 2 el bit 14 suma medio (N/2). */
uint32_t pss_fid_x2(uint32_t ctl, uint32_t modelo)
{
    return ((ctl >> 8) & 0x1F) * 2 + (pss_core2(modelo) ? (ctl >> 14) & 1 : 0);
}

uint32_t pss_vid(uint32_t ctl) { return ctl & 0xFF; }

static void cpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i + 1 < n && s[i]; i++) d[i] = s[i];
    if (n) d[i] = 0;
}

static bool falha(pss_cpu_t *c, const char *m)
{
    c->fiavel = 0;
    cpy(c->motivo, m, sizeof c->motivo);
    return false;
}

/* El multiplicador efectivo en medios: con SuperLFM (bit 15, Core 2 con
 * Dynamic FSB) el bus va a la mitad, que equivale a medio multiplicador. */
static uint32_t fid_efetivo_x2(uint32_t ctl, uint32_t m)
{
    uint32_t f2 = pss_fid_x2(ctl, m);
    return pss_core2(m) && (ctl & 0x8000) ? f2 / 2 : f2;
}

static bool validar_cpu(pss_cpu_t *c, const pss_cpu_info_t *in)
{
    c->validado = 1;
    c->fiavel = 0;
    c->turbo = 0;
    c->slfm = 0;
    c->motivo[0] = 0;
    if (c->pss_origem == PSS_SEM) return falha(c, "sin _PSS");
    if (c->pss_origem == PSS_ILEGIVEL || !c->qtd) return falha(c, "_PSS ilegible (calculado o con referencias)");
    if (c->pss_incerto) return falha(c, "el _PSS tiene un Return que no se entiende");
    if (c->qtd < 2) return falha(c, "un solo estado: nada que elegir");
    if (!(c->pct_espacos & PCT_M_FFH))
        return falha(c, c->pct_espacos ? "el _PCT no usa FFixedHW (MSR 0x199)" : "sin _PCT legible");
    if (!in->intel_fidvid) return falha(c, "la CPU no es un Intel con FID/VID en PERF_CTL");
    uint32_t m = in->modelo;
    /* IDA (Core 2): el estado 0 es «P1 + 1 MHz» con un multiplicador más
     * alto que el bus no explica; no cuenta para el bus ni para el máximo. */
    c->turbo = pss_core2(m) && c->qtd >= 3 && c->est[0].mhz == c->est[1].mhz + 1;
    uint32_t base = c->turbo ? 1 : 0, bus0 = 0;
    for (uint32_t i = 0; i < c->qtd; i++) {
        const pss_estado_t *e = &c->est[i];
        if (e->mhz < 100 || e->mhz > 10000) return falha(c, "frecuencia fuera de 100-10000 MHz");
        if (i && e->mhz >= c->est[i - 1].mhz) return falha(c, "frecuencias no decrecientes");
        if (i && e->mw && c->est[i - 1].mw && e->mw > c->est[i - 1].mw) return falha(c, "potencia que crece al bajar la frecuencia");
        if ((e->controle & 0xFFFF0000u) || (e->controle & 0x2080)) return falha(c, "Control con bits fuera de FID/VID");
        if (!pss_core2(m) && (e->controle & 0xC000)) return falha(c, "Control con bits de Core 2 (medio multiplicador, SLFM)");
        if (e->controle & 0x8000) {
            if (i + 1 != c->qtd) return falha(c, "SuperLFM (bit 15) fuera del último estado");
            c->slfm = 1;
        }
        if ((e->status & 0xFFFF) != (e->controle & 0xFFFF)) return falha(c, "Status distinto de Control");
        uint32_t f2 = fid_efetivo_x2(e->controle, m), vid = pss_vid(e->controle);
        if (f2 < 4 || !vid) return falha(c, "FID o VID nulos");
        if (i) {
            const pss_estado_t *a = &c->est[i - 1];
            if (f2 >= fid_efetivo_x2(a->controle, m)) return falha(c, "FID que no baja con la frecuencia");
            if (vid > pss_vid(a->controle)) return falha(c, "VID que sube al bajar la frecuencia");
        }
        if (i < base) continue;
        /* Reloj del bus = MHz / multiplicador: el mismo en todos (+-3 %). */
        uint32_t bus = e->mhz * 2000 / f2;   /* kHz */
        if (bus < 60000 || bus > 300000) return falha(c, "reloj de bus implausible (MHz / FID)");
        if (i == base) bus0 = bus;
        else if (bus * 100 < bus0 * 97 || bus * 100 > bus0 * 103) return falha(c, "MHz y FID no cuadran entre estados");
    }
    if (in->tsc_khz) {
        uint32_t mhz = in->tsc_khz / 1000, f = c->est[base].mhz;
        if (f * 100 < mhz * 96 || f * 100 > mhz * 104) return falha(c, "el estado 0 no es la frecuencia máxima (TSC)");
    }
    if (in->tem_status) {
        uint32_t st = (uint32_t)(in->perf_status & 0xFFFF);
        bool fid = false, exato = false;
        for (uint32_t i = 0; i < c->qtd; i++) {
            if ((c->est[i].controle & 0xFF00) == (st & 0xFF00)) fid = true;
            if ((c->est[i].controle & 0xFFFF) == st) exato = true;
        }
        if (!fid) return falha(c, "el FID actual (PERF_STATUS) no está en el _PSS");
        if (!exato) return falha(c, "el VID actual (PERF_STATUS) no es el del _PSS");
        if (pss_core2(m)) {
            /* Core 2: el máximo en los bits 44:40 (+ medio en el 46). */
            uint32_t mx = (uint32_t)((in->perf_status >> 40) & 0x1F) * 2 + (uint32_t)((in->perf_status >> 46) & 1);
            if (mx && mx != pss_fid_x2(c->est[base].controle, m) && mx != pss_fid_x2(c->est[0].controle, m))
                return falha(c, "el FID máximo de PERF_STATUS no es el del estado 0");
        }
    }
    c->fiavel = 1;
    return true;
}

uint32_t acpi_pss_validar(acpi_pss_t *r, const pss_cpu_info_t *in)
{
    for (uint32_t i = 0; i < r->qtd_cpus; i++) {
        pss_cpu_t *c = &r->cpu[i];
        c->ausente = 0;
        if (in->qtd_presentes) {
            c->ausente = 1;
            for (uint32_t k = 0; k < in->qtd_presentes && k < sizeof in->presentes; k++)
                if (in->presentes[k] == c->id) c->ausente = 0;
        }
        if (c->ausente) {
            c->validado = 1;
            falha(c, "no está en la MADT (procesador ausente)");
        } else {
            validar_cpu(c, in);
        }
    }
    /* Un dominio _PSD (o todos, si no hay _PSD) comparte la tensión: todos sus
     * procesadores tienen que declarar los mismos estados. */
    for (uint32_t i = 0; i < r->qtd_cpus; i++) {
        pss_cpu_t *a = &r->cpu[i];
        if (!a->fiavel) continue;
        if (r->cpus_total > r->qtd_cpus) {
            falha(a, "más procesadores de los que caben en la tabla");
            continue;
        }
        if (a->psd_variantes && (!a->psd_dominios_iguais || a->psd_ilegivel)) {
            falha(a, "_PSD con dominios distintos según la variante");
            continue;
        }
        for (uint32_t j = 0; j < r->qtd_cpus; j++) {
            pss_cpu_t *b = &r->cpu[j];
            if (i == j || b->ausente) continue;
            bool mesmo_dom = !a->psd_variantes || !b->psd_variantes || a->psd_dominio == b->psd_dominio;
            if (!mesmo_dom) continue;
            if (b->qtd != a->qtd || !estados_iguais(a->est, b->est, a->qtd) || !b->fiavel) {
                falha(a, b->fiavel || b->qtd ? "procesadores del mismo dominio con _PSS distintos"
                                             : "otro procesador del mismo dominio sin _PSS fiable");
                break;
            }
        }
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < r->qtd_cpus; i++) n += r->cpu[i].fiavel;
    return n;
}
