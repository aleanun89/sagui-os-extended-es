// SPDX-License-Identifier: GPL-2.0-or-later
/* tablasacpi - vuelca las tablas ACPI (DSDT, SSDT, FACP...) de la máquina a ficheros .dat,
 * para estudiar su AML en otro equipo (iasl -d) o con tools/uacpi_volcado (D405). Solo
 * lee: no cambia nada del sistema. */
#include "sagui.h"

static const char *AYUDA =
    "uso: tablasacpi [carpeta]\n"
    "Escribe cada tabla ACPI en la carpeta (por defecto /home/acpi) como FIRMA-N.dat\n"
    "(dsdt-0.dat, ssdt-3.dat...) y un indice.txt con direcciones, tamaños y sumas.\n"
    "Las tablas son de la BIOS: pueden llevar el número de serie del equipo; no las\n"
    "publiques sin mirarlas.\n";

static info_acpi_t lista;
static info_acpi_tabela_dados_t trozo;

static void minusculas(char *s)
{
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

int principal(int argc, char **argv)
{
    if (argc > 1 && (compara_texto(argv[1], "-h") == 0 || compara_texto(argv[1], "--ayuda") == 0)) {
        escrever_texto(SAIDA, AYUDA);
        return 0;
    }
    const char *pasta = argc > 1 ? argv[1] : "/home/acpi";
    if (info_sistema(INFO_ACPI, &lista, sizeof lista) < 0 || !lista.achou) {
        imprimir_em(ERROS, "tablasacpi: no hay ACPI (sin RSDP)\n");
        return 1;
    }
    criar_pasta(pasta);   /* puede existir ya */
    char cam[160];
    formatar_texto(cam, sizeof cam, "%s/indice.txt", pasta);
    int ind = abrir(cam, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (ind < 0) {
        imprimir_em(ERROS, "tablasacpi: %s: %s\n", cam, nome_erro(ind));
        return 1;
    }
    imprimir_em(ind, "RSDP en %x, revisión %u, OEM %s, %s\n", lista.rsdp, lista.revisao, lista.oem,
                lista.usou_xsdt ? "XSDT" : "RSDT");
    int erros = 0, feitas = 0;
    for (unsigned i = 0; i < lista.qtd && i < ACPI_MAX_TABELAS; i++) {
        const info_acpi_tabela_t *t = &lista.tabela[i];
        char nome[8];
        formatar_texto(nome, sizeof nome, "%s", t->assinatura);
        minusculas(nome);
        formatar_texto(cam, sizeof cam, "%s/%s-%u.dat", pasta, nome, i);
        imprimir_em(ind, "%2u %s %-6s en %x, %u bytes, rev %u, suma %s -> %s\n", i, t->assinatura, t->oem, t->endereco,
                    t->tamanho, t->revisao, t->soma_ok ? "bien" : "MAL", cam);
        int fd = abrir(cam, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) { imprimir_em(ERROS, "tablasacpi: %s: %s\n", cam, nome_erro(fd)); erros++; continue; }
        unsigned desl = 0;
        int n = 0;
        for (;;) {
            trozo.indice = i;
            trozo.deslocamento = desl;
            n = info_sistema(INFO_ACPI_TABELA, &trozo, sizeof trozo);
            if (n <= 0) break;
            escrever_tudo(fd, trozo.dados, (unsigned)n);
            desl += (unsigned)n;
            if (desl >= trozo.tamanho) break;
        }
        fechar(fd);
        if (n < 0) {
            imprimir_em(ERROS, "tablasacpi: %s: %s\n", t->assinatura, nome_erro(n));
            erros++;
            continue;
        }
        imprimir("%-4s %6u bytes -> %s\n", t->assinatura, desl, cam);
        feitas++;
    }
    fechar(ind);
    imprimir("%d tablas en %s (índice en %s/indice.txt)\n", feitas, pasta, pasta);
    return erros ? 1 : 0;
}
