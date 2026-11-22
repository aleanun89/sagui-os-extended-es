// SPDX-License-Identifier: GPL-3.0-only
/* visorlogs - el Visor de registros de SaguiOS (D463).
 *
 * Origen: parte de «aplicaciones-saguios/visorlogs.c», un programa de ejemplo escrito para compilarse con
 * TCC dentro de SaguiOS (proyecto del mantenedor, GPL v3). Aquí se ha pasado a la libsagui nativa y se
 * ha ampliado. La versión sencilla original sigue en /ejemplos/tcc/visorlogs.c, para estudiarla.
 *
 * Qué hace: abre el registro del kernel (INFO_LOG, el anillo de 128 KB) o un archivo de texto (los últimos
 * 2 MiB, hasta 20 000 líneas), con búsqueda sin distinguir mayúsculas y filtros combinables:
 *   - categoría (Todas, Incidencias, GPU, USB, Disco, Red, Energía, Audio, Aplicaciones, Sistema), por
 *     las etiquetas reales del kernel ([GPU3D], [ACEL2D], [REDE], [OCIO]...);
 *   - proceso: las líneas de registrar() llevan «[nombre pid]» delante.
 * Colores por nivel (crítico, error, aviso) según el texto, con los colores del esquema vigente (D428).
 * Sigue en vivo el registro o el archivo que crece (Pausa/Seguir), exporta lo filtrado, copia la línea o la
 * selección al portapapeles y abre los archivos de /logs.
 *
 * Teclado: Ctrl+F busca, F5 recarga, F6 pausa/sigue, Ctrl+C copia, Ctrl+A selecciona todo, Ctrl+E exporta,
 * Ctrl+O abre, Esc cierra. Ratón: clic selecciona una línea, Mayús+clic una serie; la rueda desplaza.
 * Opciones: visorlogs [--filtro CAT] [--proceso NOMBRE] [--buscar TEXTO] [--exportar ARCHIVO] [--prueba]
 *           [ARCHIVO]. Con --exportar no abre ventana: escribe lo filtrado y termina (sirve en scripts).
 * Todo es entero (sin FPU). */
#include "sagui.h"
#include "gui.h"

enum { LIMITE_ARCHIVO = 2 * 1024 * 1024, LIMITE_KERNEL = 128 * 1024 + 1 };
enum { LIMITE_LINEAS = 20000, MAX_PROC = 32, LARGO_PROC = 24 };
enum { ANCHO_LAT = 162, ALTO_FILA = 20, Y_LISTA = 139, Y_CATEGORIAS = 112, PASO_CAT = 26 };
enum categoria { TODAS, INCIDENCIAS, GPU, USB, DISCO, RED, ENERGIA, AUDIO, APLICACIONES, SISTEMA, NUM_CATEGORIAS };
enum nivel { N_NORMAL, N_AVISO, N_ERROR, N_CRITICO };
enum { TECLA_RECARGAR = 0x10e, TECLA_PAUSA = 0x10f };
enum accion { B_ABRIR = 1, B_KERNEL, B_LOGS, B_RECARGAR, B_EXPORTAR, B_COPIAR, B_PAUSA, B_PROCESO, B_BORRAR };
enum { MENU_PROCESO = 77, REFRESCO_MS = 500 };

typedef struct {
    uint32_t inicio;
    uint32_t tam;
    uint16_t categorias;   /* bit i = categoría i (INCIDENCIAS ... SISTEMA) */
    uint8_t  nivel;
    uint8_t  proceso;      /* 0 = ninguno; k = procesos[k - 1] */
} linea_t;

typedef struct {
    int x, y, w, h, id;
    char texto[48];
} boton_t;

typedef struct {
    char *datos;
    int tam_datos, total_archivo;
    int total, visibles, fila, columna, max_columnas;
    int cuentas[NUM_CATEGORIAS];
    int coincidencias_n;
    enum categoria filtro;
    int proceso_sel;               /* 0 = todos; k = procesos[k - 1] */
    int nprocesos;
    char procesos[MAX_PROC][LARGO_PROC];
    int sel_a, sel_b, ancla;       /* selección: posiciones en la lista visible; -1 = ninguna */
    bool recortado, seguir, pegado;
    char ruta[512];
    char error[96], aviso[128];
    caixa_t busca;
} visor_t;

static linea_t lineas[LIMITE_LINEAS];
static int coincidencias[LIMITE_LINEAS];
static char peek[LIMITE_KERNEL + 8];   /* lectura nueva del registro del kernel, para ver si cambió */
static char copia_buf[PORTAPAPEIS_MAX + 4];
static visor_t V;
static jan_t *J;
static boton_t botones[12];
static int nbotones;

static const char *const nombres_cat[NUM_CATEGORIAS] = {
    N_("Todas"), N_("Incidencias"), N_("GPU"), N_("USB"), N_("Disco"), N_("Red"), N_("Energía"), N_("Audio"),
    N_("Aplicaciones"), N_("Sistema")
};
static const char *const claves_cat[NUM_CATEGORIAS] = {
    "todas", "incidencias", "gpu", "usb", "disco", "red", "energia", "audio", "apps", "sistema"
};

/* ------------------------------------------------------------ texto */

static unsigned plegar(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7))
        return c + 32;
    return c;
}

/* Las etiquetas del kernel son ASCII; los textos pueden llevar Latin-1. Para buscar solo se pliega el caso. */
static bool contiene(const char *texto, size_t tam, const char *patron)
{
    size_t n = tam_texto(patron);
    if (n > tam)
        return false;
    for (size_t i = 0; i + n <= tam; i++) {
        size_t j = 0;
        while (j < n && plegar((unsigned char)texto[i + j]) == plegar((unsigned char)patron[j]))
            j++;
        if (j == n)
            return true;
    }
    return false;
}

static bool alguno(const char *texto, size_t tam, const char *const *lista)
{
    for (int i = 0; lista[i]; i++)
        if (contiene(texto, tam, lista[i]))
            return true;
    return false;
}

/* ¿La etiqueta t (n letras, sin corchetes) empieza por alguno de los prefijos? */
static bool etiqueta_en(const char *t, int n, const char *const *lista)
{
    for (int i = 0; lista[i]; i++) {
        int m = (int)tam_texto(lista[i]);
        if (m > n)
            continue;
        int k = 0;
        while (k < m && plegar((unsigned char)t[k]) == (unsigned)(unsigned char)lista[i][k])
            k++;
        if (k == m)
            return true;
    }
    return false;
}

/* Prefijos (en minúsculas) de las etiquetas [XXXX] reales del kernel y de los programas, por categoría. */
static const char *const et_gpu[] = {
    "gpu", "acel2d", "video", "vesa", "gop", "bga", "vmware", "sdl2gpu", "sgl", "sagui-gl", "radeon", "intel3d", NULL
};
static const char *const et_usb[] = {
    "usb", "ehci", "uhci", "ohci", "xhci", "hid", "uvc", NULL
};
static const char *const et_disco[] = {
    "disco", "montagem", "ata", "ahci", "nvme", "ext2", "fat32", "exfat", "udf", "iso9660", "sgfs", "vfs",
    "partic", "smart", "blocos", "cache", "swap", "raiz", "logs", "fstab", "ramfs", "ram0", "cow", NULL
};
static const char *const et_red[] = {
    "red", "rede", "dhcp", "dns", "arp", "tcp", "wifi", "wpci", "rtl", "e1000", "ure", "ip", "r8169", "ne2000",
    "wg", "bt", "tunel", "ar9485", "bloqueo", NULL
};
static const char *const et_energia[] = {
    "energia", "ocio", "freq", "eist", "pss", "acpi", "uacpi", "bateria", "gpuenergia", "hpwmi", "wmi", "ec",
    "portatil", "toshiba", "fujitsu", "sensores", "cpu", "msr", NULL
};
static const char *const et_audio[] = {
    "audio", "alc", "hda", "ac97", "sb16", "mixer", "bip", "openal", "uac", "cdda", "sonidos", NULL
};
static const char *const et_apps[] = {
    "aoen", "sdl2", "rastro", "quake", "opentyrian", "openjazz", "recreativa", "dosbox", "gnuboy", "netsurf",
    "conky", "captura", "conquer", "freeciv", "openttd", "brumacanto", "doom", NULL
};
static const char *const et_sistema[] = {
    "gui", "sagui", "sesion", "servicos", "usuarios", "permisos", "arranque", "boot", "painel", "idioma", "conf",
    "kconf", "smp", "ioapic", "irq", "pci", "planificador", "isa", "isapnp", "teste", "tecla", "mouse", "i8042",
    "iconos", "portapapeles", "widgets", "telas", "hora", "accion", "mandos", "info", "sobre", NULL
};

static unsigned categorias_de(const char *texto, int tam)
{
    static const char *const *const grupos[] = {
        NULL, NULL, et_gpu, et_usb, et_disco, et_red, et_energia, et_audio, et_apps, et_sistema
    };
    unsigned r = 0;
    int limite = tam < 96 ? tam : 96;   /* las etiquetas van al principio de la línea */
    for (int i = 0; i < limite; i++) {
        if (texto[i] != '[')
            continue;
        int j = i + 1;
        while (j < tam && texto[j] != ']' && texto[j] != ' ' && j - i < 28)
            j++;
        if (j >= tam || texto[j] != ']')
            continue;   /* «[nombre pid]» (con espacio) no es una etiqueta de categoría */
        for (int c = GPU; c < NUM_CATEGORIAS; c++)
            if (etiqueta_en(texto + i + 1, j - i - 1, grupos[c]))
                r |= 1u << c;
    }
    return r;
}

static int nivel_de(const char *texto, int tam)
{
    static const char *const criticos[] = {
        "panic", "pánico", "panico", "excepci", "exception", "fallo de p", "page fault", "double fault",
        "triple fault", "oops", "#gp", "#pf", "#ud", "bug:", NULL
    };
    static const char *const errores[] = {
        "[error]", "error:", ": error", " error ", "errno", "fallo", "fallid", "failed", "no se pudo",
        "denegado", "denied", "[permisos]", "abort", NULL
    };
    static const char *const avisos[] = {
        "[aviso]", "[warn", "warning", "aviso:", "timeout", "agotado", "no encontrado", "not found",
        "reintent", "retry", NULL
    };
    if (alguno(texto, tam, criticos))
        return N_CRITICO;
    if (alguno(texto, tam, errores))
        return N_ERROR;
    if (alguno(texto, tam, avisos))
        return N_AVISO;
    return N_NORMAL;
}

/* «[nombre pid] resto»: devuelve el nombre del proceso (sin el pid) en dst; false si la línea no lo lleva. */
static bool proceso_de(const char *texto, int tam, char *dst)
{
    if (tam < 4 || texto[0] != '[')
        return false;
    int i = 1;
    while (i < tam && texto[i] != ' ' && texto[i] != ']' && i - 1 < LARGO_PROC - 1)
        i++;
    if (i >= tam || texto[i] != ' ' || i == 1)
        return false;
    int j = i + 1;
    if (j >= tam || texto[j] < '0' || texto[j] > '9')
        return false;
    while (j < tam && texto[j] >= '0' && texto[j] <= '9')
        j++;
    if (j >= tam || texto[j] != ']')
        return false;
    for (int k = 0; k < i - 1; k++)
        dst[k] = texto[1 + k];
    dst[i - 1] = 0;
    return true;
}

static int proceso_indice(visor_t *v, const char *nombre)
{
    for (int i = 0; i < v->nprocesos; i++)
        if (compara_texto(v->procesos[i], nombre) == 0)
            return i + 1;
    if (v->nprocesos >= MAX_PROC)
        return 0;
    copia_texto(v->procesos[v->nprocesos], nombre, LARGO_PROC);
    return ++v->nprocesos;
}

/* ------------------------------------------------------------ índice y filtros */

static void filtrar(visor_t *v, bool conservar)
{
    int fila = v->fila, columna = v->columna;
    for (int i = 0; i < NUM_CATEGORIAS; i++)
        v->cuentas[i] = 0;
    v->visibles = 0;
    v->max_columnas = 0;
    for (int i = 0; i < v->total; i++) {
        const linea_t *l = &lineas[i];
        if (v->proceso_sel && l->proceso != v->proceso_sel)
            continue;
        if (!contiene(v->datos + l->inicio, l->tam, v->busca.texto))
            continue;
        v->cuentas[TODAS]++;
        for (int j = INCIDENCIAS; j < NUM_CATEGORIAS; j++)
            if (j == INCIDENCIAS ? l->nivel >= N_AVISO : (l->categorias & (1u << j)))
                v->cuentas[j]++;
        bool pasa = v->filtro == TODAS ||
                    (v->filtro == INCIDENCIAS ? l->nivel >= N_AVISO : (l->categorias & (1u << v->filtro)) != 0);
        if (pasa) {
            coincidencias[v->visibles++] = i;
            if ((int)l->tam > v->max_columnas)
                v->max_columnas = (int)l->tam;
        }
    }
    v->fila = conservar ? fila : 0;
    v->columna = conservar ? columna : 0;
    if (!conservar)
        v->sel_a = v->sel_b = v->ancla = -1;
}

static void indexar(visor_t *v, int tam, bool inicio_incompleto, bool conservar)
{
    int inicio = 0;
    if (inicio_incompleto) {
        while (inicio < tam && v->datos[inicio++] != '\n')
            ;
    }
    int cuenta = 0;
    for (int i = inicio; i < tam; i++)
        if (v->datos[i] == '\n' || i == tam - 1)
            cuenta++;
    if (cuenta > LIMITE_LINEAS) {
        v->recortado = true;
        int saltar = cuenta - LIMITE_LINEAS;
        while (inicio < tam && saltar > 0)
            if (v->datos[inicio++] == '\n')
                saltar--;
    }
    v->nprocesos = 0;
    v->total = 0;
    while (inicio < tam && v->total < LIMITE_LINEAS) {
        int fin = inicio;
        while (fin < tam && v->datos[fin] != '\n')
            fin++;
        int longitud = fin - inicio;
        if (longitud && v->datos[fin - 1] == '\r')
            longitud--;
        for (int i = inicio; i < inicio + longitud; i++)
            if ((unsigned char)v->datos[i] < 32)
                v->datos[i] = ' ';
        linea_t *l = &lineas[v->total++];
        l->inicio = (uint32_t)inicio;
        l->tam = (uint32_t)longitud;
        l->nivel = (uint8_t)nivel_de(v->datos + inicio, longitud);
        l->categorias = (uint16_t)categorias_de(v->datos + inicio, longitud);
        char nombre[LARGO_PROC];
        l->proceso = proceso_de(v->datos + inicio, longitud, nombre) ? (uint8_t)proceso_indice(v, nombre) : 0;
        inicio = fin + 1;
    }
    if (v->proceso_sel > v->nprocesos)
        v->proceso_sel = 0;
    filtrar(v, conservar);
}

/* ------------------------------------------------------------ lectura */

static int leer_archivo(const char *ruta, char *datos, bool *inicio_incompleto, int *total)
{
    info_arquivo_t inf;
    if (info_arquivo(ruta, &inf) < 0 || inf.tipo == TIPO_PASTA)
        return -1;
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0)
        return -1;
    int tam = (int)inf.tamanho;
    *total = tam;
    int inicio = tam > LIMITE_ARCHIVO ? tam - LIMITE_ARCHIVO : 0;
    *inicio_incompleto = false;
    int resultado = -1;
    if (inicio > 0) {
        char c = 0;
        if (posicionar(fd, inicio - 1, POS_INICIO) >= 0 && ler(fd, &c, 1) == 1) {
            *inicio_incompleto = c != '\n';
            resultado = 0;
        }
    } else
        resultado = 0;
    if (resultado == 0) {
        int leido = 0, n;
        while (leido < tam - inicio && (n = ler(fd, datos + leido, (unsigned)(tam - inicio - leido))) > 0)
            leido += n;
        resultado = leido;
    }
    fechar(fd);
    return resultado;
}

static bool cargar(visor_t *v, const char *ruta, bool conservar)
{
    if (!v->datos) {
        v->datos = aloca(LIMITE_ARCHIVO + 8);
        if (!v->datos) {
            copia_texto(v->error, _("No hay memoria para abrir el registro."), sizeof v->error);
            return false;
        }
    }
    bool incompleto = false;
    int limite = ruta[0] ? LIMITE_ARCHIVO : LIMITE_KERNEL;
    int tam;
    if (ruta[0]) {
        int total = 0;
        tam = leer_archivo(ruta, v->datos, &incompleto, &total);
        v->total_archivo = total;
    } else {
        tam = info_sistema(INFO_LOG, v->datos, LIMITE_KERNEL);
    }
    if (tam < 0 || tam > limite) {
        copia_texto(v->error, ruta[0] ? _("No se pudo leer el archivo seleccionado.") :
                    _("No se pudo leer el registro del kernel."), sizeof v->error);
        return false;
    }
    if (ruta != v->ruta)
        copia_texto(v->ruta, ruta, sizeof v->ruta);
    v->error[0] = 0;
    v->recortado = tam == limite;
    v->tam_datos = tam;
    indexar(v, tam, incompleto, conservar);
    return true;
}

/* Sigue en vivo: true si hay algo nuevo (y entonces ya está reindexado). */
static bool refrescar(visor_t *v, int filas)
{
    bool nuevo = false;
    if (!v->ruta[0]) {
        int n = info_sistema(INFO_LOG, peek, LIMITE_KERNEL);
        if (n < 0 || n > LIMITE_KERNEL)
            return false;
        bool igual = n == v->tam_datos;
        if (igual) {
            int cmp = n < 256 ? n : 256;
            igual = memcmp(peek + n - cmp, v->datos + n - cmp, (size_t)cmp) == 0 &&
                    memcmp(peek, v->datos, (size_t)(n < 64 ? n : 64)) == 0;
        }
        if (igual)
            return false;
        memcpy(v->datos, peek, (size_t)n);
        v->tam_datos = n;
        v->recortado = n == LIMITE_KERNEL;
        indexar(v, n, false, true);
        nuevo = true;
    } else {
        info_arquivo_t inf;
        if (info_arquivo(v->ruta, &inf) < 0 || (int)inf.tamanho == v->total_archivo)
            return false;
        if (!cargar(v, v->ruta, true))
            return true;
        nuevo = true;
    }
    if (v->pegado) {
        int m = v->visibles - filas;
        v->fila = m > 0 ? m : 0;
    }
    return nuevo;
}

/* ------------------------------------------------------------ exportar y copiar */

/* Escribe en el archivo las líneas visibles [desde, hasta]; devuelve cuántas, o -1 si no se pudo. */
static int escribir_lineas(const visor_t *v, const char *ruta, int desde, int hasta)
{
    int fd = abrir(ruta, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0)
        return -1;
    int n = 0;
    for (int p = desde; p <= hasta && p < v->visibles; p++) {
        const linea_t *l = &lineas[coincidencias[p]];
        if (escrever(fd, v->datos + l->inicio, l->tam) < 0 || escrever(fd, "\n", 1) < 0) {
            fechar(fd);
            return -1;
        }
        n++;
    }
    fechar(fd);
    return n;
}

static void copiar_seleccion(visor_t *v)
{
    if (v->sel_a < 0) {
        copia_texto(v->aviso, _("Elige una línea para copiar."), sizeof v->aviso);
        return;
    }
    int desde = v->sel_a < v->sel_b ? v->sel_a : v->sel_b, hasta = v->sel_a < v->sel_b ? v->sel_b : v->sel_a;
    int n = 0, lineas_copiadas = 0;
    for (int p = desde; p <= hasta && p < v->visibles; p++) {
        const linea_t *l = &lineas[coincidencias[p]];
        if (n + (int)l->tam + 1 >= PORTAPAPEIS_MAX)
            break;
        memcpy(copia_buf + n, v->datos + l->inicio, l->tam);
        n += (int)l->tam;
        copia_buf[n++] = '\n';
        lineas_copiadas++;
    }
    copiar_texto(copia_buf, (unsigned)n);
    formatar_texto(v->aviso, sizeof v->aviso, _("Copiadas %d líneas al portapapeles."), lineas_copiadas);
    registrar("visorlogs: copiadas %d lineas\n", lineas_copiadas);
}

static void exportar(visor_t *v)
{
    char ruta[512];
    if (!v->visibles) {
        copia_texto(v->aviso, _("No hay líneas que exportar."), sizeof v->aviso);
        return;
    }
    if (!dialogo_guardar(_("Exportar registro"), _("Texto (*.txt)|*.txt|Registros (*.log)|*.log"),
                         "/home/registro.txt", ruta, sizeof ruta))
        return;
    int n = escribir_lineas(v, ruta, 0, v->visibles - 1);
    if (n < 0)
        copia_texto(v->aviso, _("No se pudo escribir el archivo."), sizeof v->aviso);
    else
        formatar_texto(v->aviso, sizeof v->aviso, _("Exportadas %d líneas."), n);
    registrar("visorlogs: exportadas %d lineas a %s\n", n, ruta);
}

/* ------------------------------------------------------------ colores */

static bool fondo_oscuro(void)
{
    return esq_lum(sagui_esq.lista_fundo) < 110;
}

static uint32_t color_nivel(int nivel)
{
    bool osc = fondo_oscuro();
    switch (nivel) {
    case N_AVISO: return osc ? sagui_esq.m_aviso : esq_mix(sagui_esq.m_aviso, 0, 40);
    case N_ERROR:
    case N_CRITICO: return osc ? sagui_esq.m_peligro : esq_mix(sagui_esq.m_peligro, 0, 12);
    default: return sagui_esq.lista_texto;
    }
}

/* ------------------------------------------------------------ geometría */

static int filas_pantalla(const jan_t *j)
{
    return (j->alt - Y_LISTA - 37) / ALTO_FILA;
}

static int columnas_pantalla(const jan_t *j)
{
    return (j->larg - ANCHO_LAT - 78) / LETRA_L;
}

static int acotar(int valor, int maximo)
{
    if (valor < 0 || maximo < 0)
        return 0;
    return valor > maximo ? maximo : valor;
}

static void mover(visor_t *v, const jan_t *j, int fila, int columna)
{
    int filas = filas_pantalla(j);
    v->fila = acotar(fila, v->visibles - filas);
    v->columna = acotar(columna, v->max_columnas - columnas_pantalla(j));
    v->pegado = v->fila >= v->visibles - filas;
}

static void anadir_boton(int *x, int y, int h, int id, const char *texto, bool derecha_a_izquierda)
{
    boton_t *b = &botones[nbotones++];
    b->w = g_largura_texto(texto) + 20;
    b->h = h;
    b->y = y;
    b->id = id;
    copia_texto(b->texto, texto, sizeof b->texto);
    if (derecha_a_izquierda)
        *x -= b->w;
    b->x = *x;
    *x += derecha_a_izquierda ? -6 : b->w + 6;
}

static void maquetar(const jan_t *j, const visor_t *v)
{
    int w = j->larg;
    nbotones = 0;
    int x = w - 16;
    anadir_boton(&x, 14, 28, B_EXPORTAR, _("Exportar"), true);
    anadir_boton(&x, 14, 28, B_RECARGAR, _("Recargar"), true);
    anadir_boton(&x, 14, 28, B_LOGS, "/logs", true);
    anadir_boton(&x, 14, 28, B_KERNEL, _("Kernel"), true);
    anadir_boton(&x, 14, 28, B_ABRIR, _("Abrir"), true);
    x = w - 16;
    anadir_boton(&x, 94, 24, B_PAUSA, v->seguir ? _("Pausa") : _("Seguir"), true);
    anadir_boton(&x, 94, 24, B_COPIAR, _("Copiar"), true);
    char p[48], nombre[16];
    copia_texto(nombre, v->proceso_sel ? v->procesos[v->proceso_sel - 1] : _("todos"), sizeof nombre);
    formatar_texto(p, sizeof p, "%s: %s", _("Proceso"), nombre);
    anadir_boton(&x, 94, 24, B_PROCESO, p, true);
    boton_t *b = &botones[nbotones++];
    b->w = 66; b->h = 24; b->x = w - 82; b->y = 65; b->id = B_BORRAR;
    copia_texto(b->texto, _("Borrar"), sizeof b->texto);
}

static void dibujar_boton(jan_t *j, const boton_t *b, bool activo)
{
    uint32_t fondo = activo ? sagui_esq.selecao : sagui_esq.botao;
    uint32_t texto = activo ? sagui_esq.selecao_texto : sagui_esq.texto;
    g_retangulo(j, b->x, b->y, b->w, b->h, fondo);
    g_moldura(j, b->x, b->y, b->w, b->h, sagui_esq.borda);
    g_texto(j, b->x + (b->w - g_largura_texto(b->texto)) / 2, b->y + (b->h - LETRA_A) / 2, b->texto, texto);
}

/* ------------------------------------------------------------ dibujo */

static void dibujar(jan_t *j, visor_t *v)
{
    int w = j->larg, h = j->alt;
    int filas = filas_pantalla(j), columnas = columnas_pantalla(j);
    maquetar(j, v);
    g_retangulo(j, 0, 0, w, h, COR_FUNDO);
    g_retangulo(j, 0, 0, w, 57, sagui_esq.barra);
    g_texto(j, 20, 20, _("VISOR DE REGISTROS"), sagui_esq.barra_texto);

    g_retangulo(j, 0, 57, ANCHO_LAT, h - 57, sagui_esq.cab_fundo);
    g_texto(j, 16, 86, _("CATEGORÍA"), COR_TEXTO_FRACO);
    for (int i = 0; i < NUM_CATEGORIAS; i++) {
        int y = Y_CATEGORIAS + i * PASO_CAT;
        bool elegido = i == (int)v->filtro;
        if (elegido)
            g_retangulo(j, 8, y, ANCHO_LAT - 16, PASO_CAT - 2, sagui_esq.selecao);
        uint32_t color = elegido ? sagui_esq.selecao_texto : sagui_esq.texto;
        char cuenta[12];
        formatar_texto(cuenta, sizeof cuenta, "%d", v->cuentas[i]);
        g_texto(j, 16, y + 4, _(nombres_cat[i]), color);
        g_texto(j, ANCHO_LAT - 16 - g_largura_texto(cuenta), y + 4, cuenta, color);
    }

    g_texto(j, ANCHO_LAT + 13, 70, _("Buscar"), COR_TEXTO);
    caixa_desenhar(j, &v->busca);
    const char *origen = v->ruta[0] ? v->ruta : _("Registro del kernel");
    int espacio = (w - ANCHO_LAT - 24) / LETRA_L;
    int n = (int)tam_texto(origen);
    if (n > espacio)
        origen += n - espacio;
    g_texto(j, ANCHO_LAT + 13, 99, origen, COR_TEXTO_FRACO);
    g_linha(j, ANCHO_LAT, 124, w, 124, sagui_esq.cab_linha);

    g_retangulo(j, ANCHO_LAT, 125, w - ANCHO_LAT, h - 125 - 29, sagui_esq.lista_fundo);
    int desde = v->sel_a < v->sel_b ? v->sel_a : v->sel_b, hasta = v->sel_a < v->sel_b ? v->sel_b : v->sel_a;
    for (int i = 0; i < filas && v->fila + i < v->visibles; i++) {
        int pos = v->fila + i;
        int indice = coincidencias[pos];
        const linea_t *l = &lineas[indice];
        int y = Y_LISTA + i * ALTO_FILA;
        uint32_t fondo = i % 2 ? sagui_esq.lista_fundo : esq_mix(sagui_esq.lista_fundo, sagui_esq.lista_texto, 5);
        if (l->nivel == N_CRITICO)
            fondo = sagui_esq.m_peligro_suave;
        if (v->sel_a >= 0 && pos >= desde && pos <= hasta)
            fondo = sagui_esq.sel_suave;
        g_retangulo(j, ANCHO_LAT + 6, y - 2, w - ANCHO_LAT - 18, ALTO_FILA, fondo);
        char numero[12];
        formatar_texto(numero, sizeof numero, "%5d", indice + 1);
        g_texto(j, ANCHO_LAT + 12, y, numero, COR_TEXTO_FRACO);
        int mostrar = (int)l->tam - v->columna;
        if (mostrar > columnas)
            mostrar = columnas;
        if (mostrar > 0) {
            uint32_t color = color_nivel(l->nivel);
            if (l->nivel == N_CRITICO && !fondo_oscuro())
                color = esq_mix(sagui_esq.m_peligro, 0, 35);
            g_texto_n(j, ANCHO_LAT + 62, y, v->datos + l->inicio + v->columna, mostrar, color);
            if (l->nivel == N_CRITICO)   /* negrita: el mismo texto desplazado un píxel */
                g_texto_n(j, ANCHO_LAT + 63, y, v->datos + l->inicio + v->columna, mostrar, color);
        }
    }
    if (!v->visibles)
        g_texto(j, ANCHO_LAT + 14, Y_LISTA + 12,
                v->total ? _("No hay coincidencias.") : _("El registro está vacío."), COR_TEXTO_FRACO);

    if (v->visibles > filas) {
        int alto = filas * ALTO_FILA;
        int barra = alto * filas / v->visibles;
        if (barra < 15)
            barra = 15;
        int y = (alto - barra) * v->fila / (v->visibles - filas);
        g_retangulo(j, w - 10, Y_LISTA, 5, alto, sagui_esq.scroll_pista);
        g_retangulo(j, w - 10, Y_LISTA + y, 5, barra, sagui_esq.scroll_polegar);
    }

    char estado[200];
    int ultima = v->fila + filas < v->visibles ? v->fila + filas : v->visibles;
    if (v->error[0])
        copia_texto(estado, v->error, sizeof estado);
    else if (v->aviso[0])
        copia_texto(estado, v->aviso, sizeof estado);
    else
        formatar_texto(estado, sizeof estado, "%d-%d / %d  |  %s %d  |  %s%s", v->visibles ? v->fila + 1 : 0, ultima,
                       v->visibles, _("Col"), v->columna + 1, v->seguir ? _("Siguiendo") : _("En pausa"),
                       v->recortado ? _("  |  Registro recortado") : "");
    g_retangulo(j, ANCHO_LAT, h - 29, w - ANCHO_LAT, 29, sagui_esq.cab_fundo);
    g_texto(j, ANCHO_LAT + 12, h - 23, estado, v->error[0] ? color_nivel(N_ERROR) : COR_TEXTO);

    for (int i = 0; i < nbotones; i++)
        dibujar_boton(j, &botones[i], botones[i].id == B_PAUSA && !v->seguir);
    jan_mostrar(j);
}

static void repintar(void)
{
    if (J)
        dibujar(J, &V);
}

/* ------------------------------------------------------------ acciones */

static void estado_log(const visor_t *v, const char *causa)
{
    registrar("visorlogs: %s filtro=%s proceso=%s visibles=%d total=%d busca=\"%s\" seguir=%d\n", causa,
              claves_cat[v->filtro], v->proceso_sel ? v->procesos[v->proceso_sel - 1] : "-", v->visibles, v->total,
              v->busca.texto, v->seguir ? 1 : 0);
}

static void abrir_dialogo(jan_t *j, visor_t *v, const char *inicial)
{
    char ruta[sizeof v->ruta];
    (void)j;
    if (dialogo_abrir(_("Abrir registro"), _("Registros (*.log;*.txt)|*.log;*.txt|Todos|*"), inicial, ruta, sizeof ruta)) {
        v->pegado = true;
        if (cargar(v, ruta, false)) {
            int m = v->visibles - filas_pantalla(j);
            v->fila = m > 0 ? m : 0;
        }
        estado_log(v, "abierto");
    }
}

static void menu_procesos(jan_t *j, const visor_t *v, const boton_t *b)
{
    char menu[MAX_PROC * (LARGO_PROC + 1) + 64];
    int n = formatar_texto(menu, sizeof menu, "%s", _("Todos los procesos"));
    for (int i = 0; i < v->nprocesos && n < (int)sizeof menu - LARGO_PROC - 2; i++)
        n += formatar_texto(menu + n, sizeof menu - (size_t)n, "\n%s", v->procesos[i]);
    menu_contextual(j->id, b->x, b->y + b->h, menu, MENU_PROCESO);
}

static void aplicar_accion(jan_t *j, visor_t *v, int id)
{
    v->aviso[0] = 0;
    switch (id) {
    case B_ABRIR: abrir_dialogo(j, v, v->ruta[0] ? v->ruta : "/"); break;
    case B_LOGS: {
        info_arquivo_t inf;
        abrir_dialogo(j, v, info_arquivo("/logs", &inf) == 0 ? "/logs" : "/");
        break;
    }
    case B_KERNEL:
        v->pegado = true;
        cargar(v, "", false);
        v->fila = acotar(v->visibles, v->visibles - filas_pantalla(j));
        estado_log(v, "kernel");
        break;
    case B_RECARGAR:
        cargar(v, v->ruta, true);
        estado_log(v, "recargado");
        break;
    case B_EXPORTAR: exportar(v); break;
    case B_COPIAR: copiar_seleccion(v); break;
    case B_PAUSA:
        v->seguir = !v->seguir;
        estado_log(v, v->seguir ? "siguiendo" : "pausa");
        break;
    case B_PROCESO: {
        for (int i = 0; i < nbotones; i++)
            if (botones[i].id == B_PROCESO)
                menu_procesos(j, v, &botones[i]);
        break;
    }
    case B_BORRAR:
        caixa_definir(&v->busca, "");
        v->busca.foco = true;
        filtrar(v, false);
        estado_log(v, "busqueda");
        break;
    }
}

static bool clic(jan_t *j, visor_t *v, const evento_t *e)
{
    int x = e->x, y = e->y;
    for (int i = 0; i < nbotones; i++) {
        const boton_t *b = &botones[i];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) {
            v->busca.foco = false;
            aplicar_accion(j, v, b->id);
            return true;
        }
    }
    if (x >= 8 && x < ANCHO_LAT - 8 && y >= Y_CATEGORIAS && y < Y_CATEGORIAS + NUM_CATEGORIAS * PASO_CAT &&
        (y - Y_CATEGORIAS) % PASO_CAT < PASO_CAT - 2) {
        v->busca.foco = false;
        v->filtro = (enum categoria)((y - Y_CATEGORIAS) / PASO_CAT);
        v->aviso[0] = 0;
        filtrar(v, false);
        v->pegado = false;
        estado_log(v, "categoria");
        return true;
    }
    if (x >= ANCHO_LAT && y >= Y_LISTA - 2 && y < Y_LISTA - 2 + filas_pantalla(j) * ALTO_FILA) {
        int pos = v->fila + (y - (Y_LISTA - 2)) / ALTO_FILA;
        v->busca.foco = false;
        if (pos < v->visibles) {
            if ((e->modificadores & 1) && v->ancla >= 0) {
                v->sel_a = v->ancla;
                v->sel_b = pos;
            } else {
                v->sel_a = v->sel_b = v->ancla = pos;
            }
            v->aviso[0] = 0;
        }
        return true;
    }
    bool foco = v->busca.foco;
    caixa_tratar(j, &v->busca, e);
    return foco || v->busca.foco;
}

static bool tecla(jan_t *j, visor_t *v, const evento_t *e)
{
    bool ctrl = (e->modificadores & 2) != 0;
    if (e->tecla == TECLA_RECARGAR) {
        aplicar_accion(j, v, B_RECARGAR);
        return true;
    }
    if (e->tecla == TECLA_PAUSA) {
        aplicar_accion(j, v, B_PAUSA);
        return true;
    }
    if (ctrl && (e->tecla == 'f' || e->tecla == 'F' || e->tecla == 6)) {
        v->busca.foco = true;
        return true;
    }
    if (ctrl && (e->tecla == 'o' || e->tecla == 'O' || e->tecla == 15)) {
        aplicar_accion(j, v, B_ABRIR);
        return true;
    }
    if (ctrl && (e->tecla == 'e' || e->tecla == 'E' || e->tecla == 5)) {
        aplicar_accion(j, v, B_EXPORTAR);
        return true;
    }
    if (v->busca.foco) {
        char anterior[sizeof v->busca.texto];
        copia_texto(anterior, v->busca.texto, sizeof anterior);
        if (caixa_tratar(j, &v->busca, e))
            v->busca.foco = false;
        if (compara_texto(anterior, v->busca.texto) != 0) {
            filtrar(v, false);
            estado_log(v, "busqueda");
        }
        return true;
    }
    if (ctrl && (e->tecla == 'c' || e->tecla == 'C' || e->tecla == 3)) {
        copiar_seleccion(v);
        return true;
    }
    if (ctrl && (e->tecla == 'a' || e->tecla == 'A' || e->tecla == 1)) {
        v->sel_a = v->visibles ? 0 : -1;
        v->sel_b = v->visibles - 1;
        v->ancla = 0;
        return true;
    }

    int fila = v->fila, col = v->columna;
    switch (e->tecla) {
    case TECLA_CIMA: fila--; break;
    case TECLA_BAIXO: fila++; break;
    case TECLA_HOME: fila = 0; col = 0; break;
    case TECLA_END: fila = v->visibles; break;
    case TECLA_PGUP: fila -= filas_pantalla(j); break;
    case TECLA_PGDN: fila += filas_pantalla(j); break;
    case TECLA_ESQUERDA: col -= 8; break;
    case TECLA_DIREITA: col += 8; break;
    default: return false;
    }
    mover(v, j, fila, col);
    return true;
}

static void geometria(visor_t *v, const jan_t *j)
{
    v->busca.x = ANCHO_LAT + 68;
    v->busca.y = 66;
    v->busca.l = j->larg - v->busca.x - 91;
    mover(v, j, v->fila, v->columna);
}

/* ------------------------------------------------------------ opciones y ventana */

static int categoria_por_clave(const char *c)
{
    for (int i = 0; i < NUM_CATEGORIAS; i++)
        if (compara_texto(c, claves_cat[i]) == 0)
            return i;
    return -1;
}

static int proceso_por_nombre(const visor_t *v, const char *nombre)
{
    for (int i = 0; i < v->nprocesos; i++)
        if (compara_texto(v->procesos[i], nombre) == 0)
            return i + 1;
    return -1;
}

static int abrir_visor(const char *ruta, const char *filtro, const char *proceso, const char *buscar,
                       const char *exportar_a)
{
    visor_t *v = &V;
    if (tam_texto(ruta) >= sizeof v->ruta) {
        imprimir_em(ERROS, "visorlogs: ruta demasiado larga\n");
        return 1;
    }
    copia_texto(v->ruta, ruta, sizeof v->ruta);
    v->seguir = true;
    v->pegado = true;
    v->sel_a = v->sel_b = v->ancla = -1;
    if (buscar)
        copia_texto(v->busca.texto, buscar, sizeof v->busca.texto);
    if (filtro) {
        int c = categoria_por_clave(filtro);
        if (c < 0) {
            imprimir_em(ERROS, "visorlogs: categoría desconocida: %s\n", filtro);
            return 2;
        }
        v->filtro = (enum categoria)c;
    }
    bool bien = cargar(v, v->ruta, false);
    if (proceso) {
        int p = proceso_por_nombre(v, proceso);
        if (p < 0) {
            imprimir_em(ERROS, "visorlogs: no hay líneas del proceso %s\n", proceso);
            return 2;
        }
        v->proceso_sel = p;
        filtrar(v, false);
    }
    if (exportar_a) {
        if (!bien) {
            imprimir_em(ERROS, "visorlogs: %s\n", v->error);
            return 1;
        }
        int n = escribir_lineas(v, exportar_a, 0, v->visibles - 1);
        if (n < 0) {
            imprimir_em(ERROS, "visorlogs: no se pudo escribir %s\n", exportar_a);
            return 1;
        }
        imprimir_em(SAIDA, "visorlogs: exportadas %d líneas a %s\n", n, exportar_a);
        registrar("visorlogs: exportadas %d lineas a %s\n", n, exportar_a);
        return 0;
    }

    J = jan_abrir(_("Visor de registros"), 872, 534);
    if (!J) {
        imprimir_em(ERROS, "visorlogs: no se pudo abrir la ventana\n");
        return 1;
    }
    jan_t *j = J;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 670, 405);
    tema_ao_mudar(repintar);
    geometria(v, j);
    v->fila = acotar(v->visibles, v->visibles - filas_pantalla(j));
    v->pegado = true;
    dibujar(j, v);
    registrar("visorlogs: abierto con %d lineas\n", v->total);
    estado_log(v, "listo");

    for (;;) {
        evento_t e;
        int r = esperar_evento(j->id, &e, v->seguir ? REFRESCO_MS : -1);
        if (r != 1) {
            if (v->seguir && !v->error[0] && refrescar(v, filas_pantalla(j))) {
                dibujar(j, v);
                if (v->ruta[0])   /* con el registro del kernel no: esta línea volvería a cambiarlo, sin fin */
                    registrar("visorlogs: nuevo contenido, %d lineas\n", v->total);
            }
            continue;
        }
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC))
            break;
        bool redibujar = false;
        if (e.tipo == EV_CLIQUE && (e.botoes & 1))
            redibujar = clic(j, v, &e);
        else if (e.tipo == EV_TECLA)
            redibujar = tecla(j, v, &e);
        else if (e.tipo == EV_RODA || e.tipo == EV_RODA_H) {
            int entalhes = (int)e.cliques;
            mover(v, j, v->fila + (e.tipo == EV_RODA ? 3 * entalhes : 0),
                  v->columna + (e.tipo == EV_RODA_H ? 8 * entalhes : 0));
            redibujar = true;
        } else if (e.tipo == EV_MENU && e.tecla == MENU_PROCESO && e.x >= 0) {
            v->proceso_sel = e.x <= v->nprocesos ? e.x : 0;
            filtrar(v, false);
            v->pegado = false;
            estado_log(v, "proceso");
            redibujar = true;
        } else if (e.tipo == EV_REDIMENSIONAR && jan_redimensionar(j, &e)) {
            geometria(v, j);
            redibujar = true;
        }
        if (redibujar)
            dibujar(j, v);
    }

    jan_fechar(j);
    J = NULL;
    libera(v->datos);
    return 0;
}

/* ------------------------------------------------------------ prueba (--prueba) */

static int probar(void)
{
    static const char muestra[] =
        "[GPU3D] Radeon activo\r\n"
        "[USB] Dispositivo conectado\n"
        "[DISCO] fallo en lectura\n"
        "[DHCP] red configurada\n"
        "[ACEL2D] error: GPU no responde\n"
        "[buscaminas 12] buscaminas: layout L=500\n"
        "[OCIO] aviso: batería baja\n"
        "[kernel] Kernel panic: excepcion\n"
        "texto general";
    char datos[sizeof muestra];
    memcpy(datos, muestra, sizeof datos);
    visor_t *v = &V;
    memset(v, 0, sizeof *v);
    v->datos = datos;
    indexar(v, (int)sizeof datos - 1, false, false);
    bool ok = v->total == 9 && v->cuentas[INCIDENCIAS] == 4 && v->cuentas[GPU] == 2 && v->cuentas[USB] == 1 &&
              v->cuentas[DISCO] == 1 && v->cuentas[RED] == 1 && v->cuentas[ENERGIA] == 1 &&
              lineas[0].tam == tam_texto("[GPU3D] Radeon activo");
    ok = ok && lineas[2].nivel == N_ERROR && lineas[6].nivel == N_AVISO && lineas[7].nivel == N_CRITICO &&
         lineas[0].nivel == N_NORMAL;
    ok = ok && v->nprocesos == 1 && compara_texto(v->procesos[0], "buscaminas") == 0 && lineas[5].proceso == 1;
    v->filtro = GPU;
    copia_texto(v->busca.texto, "AcTiVo", sizeof v->busca.texto);
    filtrar(v, false);
    ok = ok && v->visibles == 1 && coincidencias[0] == 0;
    copia_texto(v->busca.texto, "inexistente", sizeof v->busca.texto);
    filtrar(v, false);
    ok = ok && v->visibles == 0;
    v->filtro = TODAS;
    v->busca.texto[0] = 0;
    v->proceso_sel = 1;
    filtrar(v, false);
    ok = ok && v->visibles == 1 && coincidencias[0] == 5;
    imprimir_em(SAIDA, "%s\n", ok ? "visorlogs: filtros, niveles y busqueda correctos" : "visorlogs: fallo de prueba");
    registrar("visorlogs: prueba %s\n", ok ? "correcta" : "fallida");
    return ok ? 0 : 1;
}

int principal(int argc, char **argv)
{
    const char *ruta = "", *filtro = NULL, *proceso = NULL, *buscar = NULL, *exportar_a = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--prueba") == 0)
            return probar();
        if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "-h") == 0) {
            escrever_texto(SAIDA, "Uso: visorlogs [--filtro CAT] [--proceso NOMBRE] [--buscar TEXTO] [--exportar ARCHIVO] [ARCHIVO]\n"
                                  "Sin archivo, abre el registro del kernel. CAT: todas, incidencias, gpu, usb, disco, red,\n"
                                  "energia, audio, apps, sistema. Con --exportar no abre ventana: escribe lo filtrado.\n");
            return 0;
        }
        if ((compara_texto(a, "--filtro") == 0 || compara_texto(a, "--proceso") == 0 ||
             compara_texto(a, "--buscar") == 0 || compara_texto(a, "--exportar") == 0) && i + 1 < argc) {
            const char *val = argv[++i];
            if (a[2] == 'f') filtro = val;
            else if (a[2] == 'p') proceso = val;
            else if (a[2] == 'b') buscar = val;
            else exportar_a = val;
        } else if (a[0] == '-' && a[1]) {
            imprimir_em(ERROS, "visorlogs: opción no válida: %s\n", a);
            return 2;
        } else if (!ruta[0])
            ruta = a;
        else
            return 2;
    }
    return abrir_visor(ruta, filtro, proceso, buscar, exportar_a);
}
