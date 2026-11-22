// SPDX-License-Identifier: GPL-3.0-only
/* visorlogs: lee el registro del kernel o un archivo y lo filtra (ejemplo de lectura de registros y filtros).
 * Origen: proyecto SaguiOS (ejemplos para aprender C compilando con tcc dentro del sistema).
 * Es la versión sencilla; la del sistema (más completa) está en userland/programas/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"

enum { LIMITE_ARCHIVO = 2 * 1024 * 1024, LIMITE_KERNEL = 128 * 1024 };
enum { LIMITE_LINEAS = 20000, ANCHO_LATERAL = 162, ALTO_FILA = 20, Y_LISTA = 139 };
enum categoria { TODAS, INCIDENCIAS, GPU, USB, DISCO, RED, NUM_CATEGORIAS };
enum { TECLA_RECARGAR = 0x10e };

typedef struct {
    uint32_t inicio;
    uint32_t tam;
    unsigned categorias;
} linea_t;

typedef struct {
    char *datos;
    linea_t lineas[LIMITE_LINEAS];
    int coincidencias[LIMITE_LINEAS];
    int cuentas[NUM_CATEGORIAS];
    int total, visibles, fila, columna;
    enum categoria filtro;
    int max_columnas;
    bool recortado;
    char ruta[512];
    char error[96];
    caixa_t busca;
} visor_t;

static const char *const nombres[] = {
    "Todos", "Incidencias", "GPU", "USB", "Disco", "Red"
};

static unsigned plegar(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7))
        return c + 32;
    return c;
}

/* Las etiquetas del kernel son ASCII. Los textos de la consola también
 * pueden contener caracteres Latin-1. Para buscar solo se pliega el caso. */
static bool contiene(const char *texto, size_t tam, const char *patron)
{
    size_t n = strlen(patron);
    if (n > tam)
        return false;
    for (size_t i = 0; i <= tam - n; i++) {
        size_t j = 0;
        while (j < n && plegar(texto[i + j]) == plegar(patron[j]))
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

static unsigned clasificar(const char *texto, size_t tam)
{
    static const char *const errores[] = {
        "[error]", "[aviso]", "[warn", "[permisos]", "error:", ": error",
        "fallo", "fallida", "timeout", "agotado", "denegado", "warning", NULL
    };
    static const char *const gpu[] = {
        "[gpu", "[acel2d]", "[video]", "[vesa]", "[gop]", "[bga]", "[vmware]", NULL
    };
    static const char *const usb[] = {
        "[usb", "[ehci]", "[uhci]", "[ohci]", "[xhci]", "[hid]", NULL
    };
    static const char *const disco[] = {
        "[disco]", "[montagem]", "[ata]", "[ahci]", "[nvme]", "[ext2]",
        "[fat32]", "[vfs]", "[partic", "[smart]", NULL
    };
    static const char *const red[] = {
        "[red", "[dhcp", "[dns", "[arp]", "[tcp", "[wifi", "[wpci]",
        "[rtl", "[e1000]", "[ure]", "[ip]", NULL
    };
    const char *const *const grupos[] = { errores, gpu, usb, disco, red };
    unsigned resultado = 0;
    for (int i = INCIDENCIAS; i < NUM_CATEGORIAS; i++)
        if (alguno(texto, tam, grupos[i - INCIDENCIAS]))
            resultado |= 1u << i;
    return resultado;
}

static void filtrar(visor_t *v)
{
    memset(v->cuentas, 0, sizeof v->cuentas);
    v->visibles = 0;
    v->max_columnas = 0;
    for (int i = 0; i < v->total; i++) {
        const linea_t *l = &v->lineas[i];
        if (!contiene(v->datos + l->inicio, l->tam, v->busca.texto))
            continue;
        v->cuentas[TODAS]++;
        for (int j = INCIDENCIAS; j < NUM_CATEGORIAS; j++)
            if (l->categorias & (1u << j))
                v->cuentas[j]++;
        if (v->filtro == TODAS || (l->categorias & (1u << v->filtro))) {
            v->coincidencias[v->visibles++] = i;
            if ((int)l->tam > v->max_columnas)
                v->max_columnas = l->tam;
        }
    }
    v->fila = 0;
    v->columna = 0;
}

static void indexar(visor_t *v, int tam, bool inicio_incompleto)
{
    int inicio = 0;
    if (inicio_incompleto) {
        while (inicio < tam && v->datos[inicio++] != '\n')
            ;
    }
    int lineas = 0;
    for (int i = inicio; i < tam; i++)
        if (v->datos[i] == '\n' || i == tam - 1)
            lineas++;
    if (lineas > LIMITE_LINEAS) {
        v->recortado = true;
        int saltar = lineas - LIMITE_LINEAS;
        while (inicio < tam && saltar > 0)
            if (v->datos[inicio++] == '\n')
                saltar--;
    }

    v->total = 0;
    while (inicio < tam) {
        int fin = inicio;
        while (fin < tam && v->datos[fin] != '\n')
            fin++;
        int longitud = fin - inicio;
        if (longitud && v->datos[fin - 1] == '\r')
            longitud--;
        for (int i = inicio; i < inicio + longitud; i++)
            if ((unsigned char)v->datos[i] < 32)
                v->datos[i] = ' ';
        linea_t *l = &v->lineas[v->total++];
        l->inicio = inicio;
        l->tam = longitud;
        l->categorias = clasificar(v->datos + inicio, longitud);
        inicio = fin + 1;
    }
    filtrar(v);
}

static int leer_archivo(const char *ruta, char *datos, bool *inicio_incompleto)
{
    FILE *archivo = fopen(ruta, "rb");
    if (!archivo)
        return -1;

    int resultado = -1;
    if (fseek(archivo, 0, SEEK_END) == 0) {
        long total = ftell(archivo);
        if (total >= 0) {
            long inicio = total > LIMITE_ARCHIVO ? total - LIMITE_ARCHIVO : 0;
            if (inicio == 0 || fseek(archivo, inicio - 1, SEEK_SET) == 0) {
                *inicio_incompleto = inicio > 0 && fgetc(archivo) != '\n';
                if (inicio > 0 || fseek(archivo, 0, SEEK_SET) == 0) {
                    size_t leido = fread(datos, 1, (size_t)(total - inicio), archivo);
                    if (!ferror(archivo))
                        resultado = (int)leido;
                }
            }
        }
    }
    fclose(archivo);
    return resultado;
}

static bool cargar(visor_t *v, const char *ruta)
{
    int limite = ruta[0] ? LIMITE_ARCHIVO : LIMITE_KERNEL;
    char *datos = malloc(limite);
    if (!datos) {
        snprintf(v->error, sizeof v->error, "No hay memoria para abrir el registro.");
        return false;
    }
    bool incompleto = false;
    int tam = ruta[0] ? leer_archivo(ruta, datos, &incompleto)
                     : info_sistema(INFO_LOG, datos, limite);
    if (tam < 0 || tam > limite) {
        free(datos);
        snprintf(v->error, sizeof v->error, "%s", ruta[0] ?
                 "No se pudo leer el archivo seleccionado." :
                 "No se pudo leer el registro del kernel.");
        return false;
    }

    free(v->datos);
    v->datos = datos;
    if (ruta != v->ruta)
        snprintf(v->ruta, sizeof v->ruta, "%s", ruta);
    v->error[0] = 0;
    v->recortado = tam == limite;
    indexar(v, tam, incompleto);
    return true;
}

static int filas_pantalla(const jan_t *j)
{
    return (j->alt - Y_LISTA - 37) / ALTO_FILA;
}

static int columnas_pantalla(const jan_t *j)
{
    return (j->larg - ANCHO_LATERAL - 78) / LETRA_L;
}

static int acotar(int valor, int maximo)
{
    if (valor < 0 || maximo < 0)
        return 0;
    return valor > maximo ? maximo : valor;
}

static void mover(visor_t *v, const jan_t *j, int fila, int columna)
{
    v->fila = acotar(fila, v->visibles - filas_pantalla(j));
    v->columna = acotar(columna, v->max_columnas - columnas_pantalla(j));
}

static void boton(jan_t *j, int x, int ancho, const char *nombre)
{
    g_retangulo(j, x, 14, ancho, 28, 0x244657);
    g_moldura(j, x, 14, ancho, 28, 0x547686);
    g_texto(j, x + (ancho - g_largura_texto(nombre)) / 2, 20, nombre, 0xe8f6f6);
}

static void dibujar(jan_t *j, visor_t *v)
{
    int w = j->larg, h = j->alt;
    int filas = filas_pantalla(j), columnas = columnas_pantalla(j);
    g_retangulo(j, 0, 0, w, h, 0xf7f8f6);
    g_retangulo(j, 0, 0, w, 57, 0x153342);
    g_texto(j, 20, 20, "VISOR DE REGISTROS", 0xe6fbf2);
    boton(j, w - 285, 81, "Abrir");
    boton(j, w - 196, 81, "Kernel");
    boton(j, w - 107, 87, "Recargar");

    g_retangulo(j, 0, 57, ANCHO_LATERAL, h - 57, 0xeaf0ed);
    g_texto(j, 16, 99, "CATEGORIA", 0x4a625e);
    for (int i = 0; i < NUM_CATEGORIAS; i++) {
        int y = 124 + i * 35;
        bool elegido = i == v->filtro;
        if (elegido)
            g_retangulo(j, 8, y, ANCHO_LATERAL - 16, 29, 0x1e5a63);
        uint32_t color = elegido ? 0xf2fff7 : 0x294754;
        char cuenta[12];
        snprintf(cuenta, sizeof cuenta, "%d", v->cuentas[i]);
        g_texto(j, 16, y + 7, nombres[i], color);
        g_texto(j, ANCHO_LATERAL - 16 - g_largura_texto(cuenta), y + 7, cuenta, color);
    }

    g_texto(j, ANCHO_LATERAL + 13, 70, "Buscar", 0x28434d);
    caixa_desenhar(j, &v->busca);
    g_retangulo(j, w - 82, 65, 66, 24, 0xdfebe6);
    g_texto(j, w - 73, 69, "Borrar", 0x274854);
    const char *origen = v->ruta[0] ? v->ruta : "Registro del kernel";
    int espacio = (w - ANCHO_LATERAL - 24) / LETRA_L;
    int n = (int)strlen(origen);
    if (n > espacio)
        origen += n - espacio;
    g_texto(j, ANCHO_LATERAL + 13, 101, origen, 0x4a625e);
    g_linha(j, ANCHO_LATERAL, 124, w, 124, 0xc3d0cb);

    for (int i = 0; i < filas && v->fila + i < v->visibles; i++) {
        int indice = v->coincidencias[v->fila + i];
        const linea_t *l = &v->lineas[indice];
        int y = Y_LISTA + i * ALTO_FILA;
        if (i % 2 == 0)
            g_retangulo(j, ANCHO_LATERAL + 6, y - 2, w - ANCHO_LATERAL - 18,
                        ALTO_FILA, 0xeef2f0);
        char numero[8];
        snprintf(numero, sizeof numero, "%5d", indice + 1);
        g_texto(j, ANCHO_LATERAL + 12, y, numero, 0x5e7377);
        int mostrar = (int)l->tam - v->columna;
        if (mostrar > columnas)
            mostrar = columnas;
        if (mostrar > 0)
            g_texto_n(j, ANCHO_LATERAL + 62, y, v->datos + l->inicio + v->columna,
                      mostrar, l->categorias & (1u << INCIDENCIAS) ? 0x983e19 : 0x18323b);
    }
    if (!v->visibles)
        g_texto(j, ANCHO_LATERAL + 14, Y_LISTA + 12,
                v->total ? "No hay coincidencias." : "El registro esta vacio.", 0x445d62);

    if (v->visibles > filas) {
        int alto = filas * ALTO_FILA;
        int barra = alto * filas / v->visibles;
        if (barra < 15)
            barra = 15;
        int y = (alto - barra) * v->fila / (v->visibles - filas);
        g_retangulo(j, w - 10, Y_LISTA, 5, alto, 0xdce7e1);
        g_retangulo(j, w - 10, Y_LISTA + y, 5, barra, 0x518083);
    }

    char estado[128];
    int hasta = v->fila + filas < v->visibles ? v->fila + filas : v->visibles;
    if (v->error[0])
        snprintf(estado, sizeof estado, "%s", v->error);
    else
        snprintf(estado, sizeof estado, "%d-%d de %d  |  Col %d%s",
                 v->visibles ? v->fila + 1 : 0, hasta, v->visibles,
                 v->columna + 1, v->recortado ? "  |  Registro recortado" : "");
    g_retangulo(j, ANCHO_LATERAL, h - 29, w - ANCHO_LATERAL, 29, 0xe3ebe7);
    g_texto(j, ANCHO_LATERAL + 12, h - 23, estado, v->error[0] ? 0x983e19 : 0x28434d);
    jan_mostrar(j);
}

static bool clic(jan_t *j, visor_t *v, const evento_t *e)
{
    int x = e->x, y = e->y, w = j->larg;
    if (y >= 14 && y < 42 && x >= w - 285 && x < w - 20) {
        v->busca.foco = false;
        if (x < w - 204) {
            char ruta[sizeof v->ruta];
            if (dialogo_abrir("Abrir registro", "Registros (*.log;*.txt)|*.log;*.txt|Todos|*",
                              v->ruta, ruta, sizeof ruta))
                cargar(v, ruta);
        } else if (x >= w - 196 && x < w - 115) {
            cargar(v, "");
        } else if (x >= w - 107) {
            cargar(v, v->ruta);
        }
        return true;
    }

    if (x >= 8 && x < ANCHO_LATERAL - 8 && y >= 124 &&
        y < 124 + NUM_CATEGORIAS * 35 && (y - 124) % 35 < 29) {
        v->busca.foco = false;
        v->filtro = (y - 124) / 35;
        filtrar(v);
        return true;
    }
    if (y >= 65 && y < 89 && x >= w - 82 && x < w - 16) {
        caixa_definir(&v->busca, "");
        v->busca.foco = true;
        filtrar(v);
        return true;
    }

    bool foco = v->busca.foco;
    caixa_tratar(j, &v->busca, e);
    return foco || v->busca.foco;
}

static bool tecla(jan_t *j, visor_t *v, const evento_t *e)
{
    if (e->tecla == TECLA_RECARGAR) {
        cargar(v, v->ruta);
        return true;
    }
    if ((e->modificadores & 2) &&
        (e->tecla == 'f' || e->tecla == 'F' || e->tecla == 6)) {
        v->busca.foco = true;
        return true;
    }
    if (v->busca.foco) {
        char anterior[sizeof v->busca.texto];
        strcpy(anterior, v->busca.texto);
        if (caixa_tratar(j, &v->busca, e))
            v->busca.foco = false;
        if (strcmp(anterior, v->busca.texto) != 0)
            filtrar(v);
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
    v->busca.x = ANCHO_LATERAL + 68;
    v->busca.y = 66;
    v->busca.l = j->larg - v->busca.x - 91;
    mover(v, j, v->fila, v->columna);
}

static int abrir_visor(const char *ruta)
{
    visor_t *v = calloc(1, sizeof *v);
    if (!v)
        return 1;
    if (strlen(ruta) >= sizeof v->ruta) {
        fputs("visorlogs: ruta demasiado larga\n", stderr);
        free(v);
        return 1;
    }
    snprintf(v->ruta, sizeof v->ruta, "%s", ruta);
    cargar(v, v->ruta);
    jan_t *j = jan_abrir("Visor de registros", 872, 534);
    if (!j) {
        fputs("visorlogs: no se pudo abrir la ventana\n", stderr);
        free(v->datos);
        free(v);
        return 1;
    }
    jan_estilo(j, JAN_REDIMENSIONAVEL, 670, 405);
    geometria(v, j);
    dibujar(j, v);
    registrar("visorlogs: abierto con %d lineas\n", v->total);

    for (;;) {
        evento_t e;
        if (esperar_evento(j->id, &e, -1) != 1)
            continue;
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
        } else if (e.tipo == EV_REDIMENSIONAR && jan_redimensionar(j, &e)) {
            geometria(v, j);
            redibujar = true;
        }
        if (redibujar)
            dibujar(j, v);
    }

    jan_fechar(j);
    free(v->datos);
    free(v);
    return 0;
}

static int probar(void)
{
    static const char muestra[] =
        "[GPU3D] Radeon activo\r\n"
        "[USB] Dispositivo conectado\n"
        "[DISCO] fallo en lectura\n"
        "[DHCP] red configurada\n"
        "[ACEL2D] error: GPU no responde\n"
        "texto general";
    char datos[sizeof muestra];
    memcpy(datos, muestra, sizeof datos);
    visor_t *v = calloc(1, sizeof *v);
    if (!v)
        return 1;
    v->datos = datos;
    indexar(v, sizeof datos - 1, false);
    bool ok = v->total == 6 && v->cuentas[INCIDENCIAS] == 2 &&
              v->cuentas[GPU] == 2 && v->cuentas[USB] == 1 &&
              v->cuentas[DISCO] == 1 && v->cuentas[RED] == 1 &&
              v->lineas[0].tam == strlen("[GPU3D] Radeon activo");
    v->filtro = GPU;
    strcpy(v->busca.texto, "AcTiVo");
    filtrar(v);
    ok = ok && v->visibles == 1 && v->coincidencias[0] == 0;
    strcpy(v->busca.texto, "inexistente");
    filtrar(v);
    ok = ok && v->visibles == 0;
    free(v);
    puts(ok ? "visorlogs: filtros y busqueda correctos" : "visorlogs: fallo de prueba");
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--prueba") == 0)
        return probar();
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--ayuda") == 0)) {
        puts("Uso: visorlogs [archivo.log]");
        puts("Sin archivo, abre el registro del kernel.");
        return argc > 2 ? 2 : 0;
    }
    return abrir_visor(argc == 2 ? argv[1] : "");
}
