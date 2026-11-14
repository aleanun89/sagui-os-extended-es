// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf.c - a API pública do motor de PDF (melhora 059, D138).
 *
 * O motor está repartido em userland/pdf/: o analisador (pdf_lex.c), a
 * criptografia (pdf_cripto.c), as tipografias (pdf_fonte.c), a geometria
 * (pdf_geom.c) e o renderizador (pdf_render.c). Este ficheiro é a porta de
 * entrada: o que o programa (visorpdf) usa é isto, e nada mais. */
#include "pdf.h"
#include "pdf_lex.h"

/* Abre o documento e diz quantas páginas tem. */
pdf_t *pdf_abrir(const char *caminho, int *erro)
{
    return pdf_doc_abrir(caminho, erro);
}

/* Abre o documento com a palavra-passe (para os PDF protegidos). */
pdf_t *pdf_abrir_com_contrasena(const char *caminho, const char *contrasena, int *erro)
{
    return pdf_doc_abrir_com_contrasena(caminho, contrasena, erro);
}

/* Fecha o documento e liberta tudo. */
void pdf_fechar(pdf_t *d)
{
    pdf_doc_fechar(d);
}

/* Quantas páginas tem. */
int pdf_paginas(const pdf_t *d)
{
    return pdf_doc_paginas((pdf_doc_t *)d);
}

/* Os metadatos (vêm em UTF-8; "" se não houver). */
const char *pdf_titulo(const pdf_t *d)
{
    return pdf_doc_titulo(d);
}

const char *pdf_autor(const pdf_t *d)
{
    return pdf_doc_autor(d);
}

const char *pdf_produtor(const pdf_t *d)
{
    return pdf_doc_produtor(d);
}

/* O tamanho da página, em pontos de PDF (1/72 de polegada). */
int pdf_pagina_largura(pdf_t *d, int pagina)
{
    return pdf_doc_pagina_largura(d, pagina);
}

int pdf_pagina_altura(pdf_t *d, int pagina)
{
    return pdf_doc_pagina_altura(d, pagina);
}

/* True se o documento está cifrado e a palavra-passe não chegou. */
bool pdf_necesita_contrasena(pdf_t *d)
{
    return pdf_doc_contrasena_necesaria(d);
}

/* O texto de um erro, em espanhol. */
const char *pdf_erro_texto(int erro)
{
    switch (erro) {
    case PDF_ERRO_FORMATO:    return "no parece un PDF";
    case PDF_ERRO_CORROMPIDO: return "el PDF está dañado";
    case PDF_ERRO_MEMORIA:    return "no hay memoria para el PDF";
    case PDF_ERRO_CIFRADO:    return "el PDF está cifrado: hace falta la contraseña";
    case PDF_ERRO_PAGINA:     return "no hay esa página";
    case PDF_ERRO_SEM_FONTE:  return "falta una fuente y no se puede seguir";
    case ERRO_NAO_EXISTE:     return "no existe el archivo";
    case ERRO_ACESSO:        return "sin permiso para leer el archivo";
    case 0:                   return "sin error";
    default:                  return "error desconocido";
    }
}
