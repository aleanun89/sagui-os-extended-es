// SPDX-License-Identifier: GPL-2.0-or-later
/* logs_disco.c - el registro del kernel en la partición SAGUI-LOGS (D120).
 *
 * Si hay un volumen montado en /logs para escribir, el kernel guarda en él el anillo de klog
 * (console_log_copiar, los últimos 32 KB) en /logs/kernel-AAAAMMDD-HHMM.log: al montar el
 * volumen (lo acumulado hasta entonces) y otra vez antes de desmontar todo al apagar o
 * reiniciar (montagem_desmontar_tudo). Un archivo por arranque, siempre el mismo nombre; se
 * conservan los LOGS_CONSERVAR más recientes (los nombres ordenan por fecha). Sin /logs no
 * se escribe nada. */
#include "montagem.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "texto.h"

#define LOGS_CONSERVAR 10
#define LOGS_TAM       (33 * 1024)   /* un poco más que el anillo, con la cabecera */

static char nome_log[48];   /* vacío = aún no se ha creado el de este arranque */

static montagem_t *montagem_logs(void)
{
    const char *rel;
    montagem_t *m = montagem_de("/logs/x", &rel);
    if (!m || txt_compara(m->ponto, "/logs") != 0 || (m->opcoes & MONTAR_SO_LEITURA)) return NULL;
    return m;
}

static bool e_log(const char *n)
{
    size_t t = txt_tamanho(n);
    return t > 11 && txt_comeca_com(n, "kernel-") && txt_compara(n + t - 4, ".log") == 0;
}

/* Borra los más antiguos hasta que queden como mucho LOGS_CONSERVAR (el actual nunca). */
static void rotar(montagem_t *m)
{
    for (int vuelta = 0; vuelta < 64; vuelta++) {
        entrada_pasta_t e;
        char menor[NOME_MAX];
        int cuenta = 0;
        menor[0] = 0;
        for (int i = 0; m->fs->listar(m->priv, "", i, &e) == 1; i++) {
            if (e.tipo == TIPO_PASTA || !e_log(e.nome)) continue;
            cuenta++;
            if (txt_compara(e.nome, nome_log) != 0 && (!menor[0] || txt_compara(e.nome, menor) < 0))
                txt_copia_n(menor, e.nome, sizeof menor);
        }
        if (cuenta <= LOGS_CONSERVAR || !menor[0]) return;
        int r = m->fs->apagar(m->priv, menor);
        klog("[LOGS] rotación: %s borrado (%d)\n", menor, r);
        if (r < 0) return;
    }
}

static bool existe(montagem_t *m, const char *rel)
{
    info_arquivo_t inf;
    return m->fs->info(m->priv, rel, &inf) >= 0;
}

/* Escribe el anillo en el archivo de este arranque. motivo: "montaje", "apagado"... */
void logs_disco_gravar(const char *motivo)
{
    montagem_t *m = montagem_logs();
    if (!m) return;
    bool primera = !nome_log[0];
    if (primera) {
        fuso_data_t d;
        hora_local(&d);
        for (int n = 0; n < 100; n++) {
            if (n) snformatar(nome_log, sizeof nome_log, "kernel-%04u%02u%02u-%02u%02u-%d.log", d.ano, d.mes, d.dia, d.hora, d.minuto, n + 1);
            else   snformatar(nome_log, sizeof nome_log, "kernel-%04u%02u%02u-%02u%02u.log", d.ano, d.mes, d.dia, d.hora, d.minuto);
            if (!existe(m, nome_log)) break;
        }
    }
    char *buf = kmalloc(LOGS_TAM);
    if (!buf) return;
    int cab = snformatar(buf, 160, "# SaguiOS - registro del kernel (%s)\n", motivo);
    uint32_t n = console_log_copiar(buf + cab, LOGS_TAM - (uint32_t)cab);
    arq_montado_t *a = kcalloc(sizeof(arq_montado_t) + m->fs->tam_arq);
    if (a) {
        uint32_t tam = 0;
        int r = m->fs->abrir(m->priv, nome_log, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR, a->dados, &tam);
        if (r >= 0) {
            r = m->fs->escrever(m->priv, a->dados, 0, buf, (uint32_t)cab + n, &tam);
            if (r >= 0 && m->fs->sincronizar) m->fs->sincronizar(m->priv, a->dados);
            m->fs->fechar(m->priv, a->dados);
        }
        klog("[LOGS] %s: %s (%u bytes) -> %d\n", motivo, nome_log, (uint32_t)cab + n, r);
        if (primera && r >= 0) rotar(m);
        kfree(a);
    }
    kfree(buf);
}
