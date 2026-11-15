// SPDX-License-Identifier: GPL-2.0-or-later
/* idioma.c - idioma de la interfaz del kernel y SIS_IDIOMA (D313, mejora 118). Ver idioma.h. */
#include "idioma.h"
#include "abi.h"
#include "catalogo.h"
#include "console.h"
#include "gui.h"
#include "memoria.h"
#include "sistema.h"
#include "texto.h"
#include "video.h"
#include "vfs.h"

static char nome[IDIOMA_MAX] = "es";
static catalogo_t cat;
static uint8_t *cat_dados;
static bool cat_ok;

const char *idioma_actual(void) { return nome; }
bool idioma_fecha_mdy(void) { return nome[0] == 'e' && nome[1] == 'n' && !nome[2]; }

const char *kernel_traducir(const char *clave)
{
    if (!cat_ok) return clave;
    const char *t = catalogo_buscar(&cat, clave);
    return t ? t : clave;
}

/* Carga el catálogo de "nuevo" (o ninguno si es el español o no existe). */
static void cargar(const char *nuevo)
{
    uint8_t *dados = NULL;
    uint32_t tam = 0;
    catalogo_t c;
    bool ok = false;
    if (txt_compara(nuevo, "es") != 0) {
        char caminho[CAMINHO_MAX];
        txt_copia_n(caminho, "/sistema/idiomas/", sizeof caminho);
        txt_copia_n(caminho + txt_tamanho(caminho), nuevo, sizeof caminho - txt_tamanho(caminho));
        txt_copia_n(caminho + txt_tamanho(caminho), "/kernel.cat", sizeof caminho - txt_tamanho(caminho));
        if (vfs_ler_inteiro(caminho, &dados, &tam) == 0) {
            if (catalogo_abrir(&c, dados, tam) == 0) ok = true;
            else { kfree(dados); dados = NULL; klog("[IDIOMA] %s: catalogo no valido\n", caminho); }
        } else klog("[IDIOMA] %s: no existe, la interfaz queda en español\n", caminho);
    }
    uint8_t *viejo = cat_dados;
    cat_ok = false;                    /* (un solo hilo del kernel: no hay carrera) */
    cat_dados = dados;
    if (ok) cat = c;
    cat_ok = ok;
    if (viejo) kfree(viejo);
    txt_copia_n(nome, nuevo, sizeof nome);
    klog("[IDIOMA] interfaz del kernel: %s%s\n", nome, ok ? " (catalogo cargado)" : "");
}

/* SIS_IDIOMA (op, ptr, n) */
int32_t sis_idioma(uint32_t op, uint32_t ptr, uint32_t n, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (op == IDIOMA_LER) {
        if (n == 0 || !usuario_ok((void *)ptr, n, true)) return ERRO_ENDERECO;
        txt_copia_n((char *)ptr, nome, n);
        return 0;
    }
    if (op == IDIOMA_FIXAR) {
        char pedido[40], nuevo[IDIOMA_MAX];
        if (usuario_texto((const char *)ptr, pedido, sizeof pedido) < 0) return ERRO_ENDERECO;
        catalogo_idioma_norm(pedido, nuevo);
        if (txt_compara(nuevo, nome) == 0 && (cat_ok || txt_compara(nuevo, "es") == 0)) return 0;
        cargar(nuevo);
        if (gui_ativa()) {
            area_idioma_mudou();
            gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
            gui_mostrar();
        }
        return 0;
    }
    return ERRO_INVALIDO;
}
