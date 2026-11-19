// SPDX-License-Identifier: GPL-2.0-or-later
/* wmi_wdg.c - el análisis de _WDG y los búferes de HP, sin hardware (D405). Ver wmi_wdg.h. */
#include "wmi_wdg.h"

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void poe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static bool imprimivel(uint8_t c) { return c >= 0x20 && c < 0x7F; }

int wmi_wdg_analisar(const uint8_t *buf, uint32_t n, wmi_bloco_t *saida, int max)
{
    if (!buf || n == 0 || n % WMI_BLOCO_BYTES) return -1;
    int qtd = (int)(n / WMI_BLOCO_BYTES);
    for (int i = 0; i < qtd; i++) {
        const uint8_t *b = buf + (uint32_t)i * WMI_BLOCO_BYTES;
        uint8_t flags = b[19];
        bool evento = (flags & WMI_EVENTO) != 0;
        /* Un bloque que no es evento se llama por su ObjectId: dos letras. */
        if (!evento && (!imprimivel(b[16]) || !imprimivel(b[17]))) return -1;
        if (i >= max || !saida) continue;
        wmi_bloco_t *s = &saida[i];
        for (int k = 0; k < 16; k++) s->guid[k] = b[k];
        if (evento) {
            s->id[0] = 0;
            s->notificacao = b[16];
        } else {
            s->id[0] = (char)b[16];
            s->id[1] = (char)b[17];
            s->notificacao = 0;
        }
        s->id[2] = 0;
        s->instancias = b[18];
        s->flags = flags;
    }
    return qtd;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Posición de cada byte del texto en el orden binario: Data1 (4 bytes), Data2 (2) y Data3
 * (2) van al revés; los 8 de Data4, tal cual. */
static const uint8_t ordem[16] = { 3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15 };

bool wmi_guid_ler(const char *t, uint8_t guid[16])
{
    if (!t) return false;
    int k = 0;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (t[i] != '-') return false;
            continue;
        }
        int a = hex(t[i]);
        if (a < 0) return false;
        int b = hex(t[++i]);
        if (b < 0) return false;
        guid[ordem[k++]] = (uint8_t)(a << 4 | b);
    }
    return t[36] == 0 && k == 16;
}

void wmi_guid_texto(const uint8_t guid[16], char s[37])
{
    static const char h[] = "0123456789ABCDEF";
    int k = 0, j = 0;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { s[i] = '-'; continue; }
        uint8_t v = guid[ordem[k]];
        s[i] = h[j ? (v & 15) : (v >> 4)];
        if (j) k++;
        j ^= 1;
    }
    s[36] = 0;
}

static bool guid_igual(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) return false;
    return true;
}

int wmi_achar(const wmi_bloco_t *v, int n, const uint8_t guid[16])
{
    for (int i = 0; i < n; i++) if (guid_igual(v[i].guid, guid)) return i;
    return -1;
}

int wmi_achar_evento(const wmi_bloco_t *v, int n, uint8_t notificacao)
{
    for (int i = 0; i < n; i++)
        if ((v[i].flags & WMI_EVENTO) && v[i].notificacao == notificacao) return i;
    return -1;
}

void wmi_nome_metodo(const wmi_bloco_t *b, const char *prefixo, char s[5])
{
    static const char h[] = "0123456789ABCDEF";
    s[0] = prefixo[0];
    s[1] = prefixo[1];
    if (b->flags & WMI_EVENTO) {
        s[2] = h[b->notificacao >> 4];
        s[3] = h[b->notificacao & 15];
    } else {
        s[2] = b->id[0];
        s[3] = b->id[1];
    }
    s[4] = 0;
}

/* ---------------------------------------------------------------- HP */

uint32_t hpwmi_montar(uint8_t *buf, uint32_t max, uint32_t ordem_, uint32_t tipo, const void *dados, uint32_t n)
{
    if (!buf || max < HPWMI_CABECALHO || n > max - HPWMI_CABECALHO) return 0;
    poe32(buf, HPWMI_FIRMA);
    poe32(buf + 4, ordem_);
    poe32(buf + 8, tipo);
    poe32(buf + 12, n);
    const uint8_t *d = dados;
    for (uint32_t i = 0; i < n; i++) buf[HPWMI_CABECALHO + i] = d ? d[i] : 0;
    return HPWMI_CABECALHO + n;
}

bool hpwmi_resposta(const uint8_t *buf, uint32_t n, uint32_t *codigo, const uint8_t **dados, uint32_t *ndados)
{
    if (!buf || n < HPWMI_RESPOSTA_CAB) return false;
    *codigo = le32(buf + 4);
    *dados = buf + HPWMI_RESPOSTA_CAB;
    *ndados = n - HPWMI_RESPOSTA_CAB;
    return true;
}

uint32_t hpwmi_id_metodo(uint32_t bytes)
{
    if (bytes == 0) return 1;
    if (bytes <= 4) return 2;
    if (bytes <= 128) return 3;
    if (bytes <= 1024) return 4;
    return 5;
}

void hpwmi_radios_ler(const uint8_t e[4], hpwmi_radio_t r[HPWMI_RADIOS])
{
    for (int i = 0; i < HPWMI_RADIOS; i++) {
        r[i].presente = (e[0] >> i) & 1;
        r[i].sw = r[i].presente && (e[1 + i] & HPWMI_RADIO_SW);
        r[i].hw = r[i].presente && (e[1 + i] & HPWMI_RADIO_HW);
    }
}

uint32_t hpwmi_radios_pedido(int radio, bool ligar)
{
    if (radio < 0 || radio >= HPWMI_RADIOS) return 0;
    uint32_t bit = 1u << radio;
    return bit << 8 | (ligar ? bit : 0);
}

bool hpwmi_evento_ler(const uint8_t *buf, uint32_t n, uint32_t *evento, uint32_t *dado)
{
    if (!buf || (n != 8 && n != 16)) return false;
    *evento = le32(buf);
    *dado = le32(buf + 4);
    return true;
}

/* Códigos de las teclas del panel que dan las BIOS de HP (los que devuelve la consulta
 * 0x0C en el AML de cada máquina). Los que no están aquí se apuntan en el registro con su
 * código: así se añaden los del dv7 cuando se vean. */
static const hpwmi_tecla_t teclas[] = {
    { 0x20E4, HPWMI_ACAO_AVISO, "botón QuickLaunch" },
    { 0x20E6, HPWMI_ACAO_AVISO, "botón QuickPlay" },
    { 0x20E8, HPWMI_ACAO_AVISO, "botón multimedia" },
    { 0x2142, HPWMI_ACAO_AVISO, "botón DVD/música" },
    { 0x213B, HPWMI_ACAO_AVISO, "botón de información" },
    { 0x2169, HPWMI_ACAO_AVISO, "girar la pantalla" },
    { 0x216A, HPWMI_ACAO_AVISO, "botón de configuración" },
    { 0x231B, HPWMI_ACAO_AVISO, "botón de ayuda" },
};

const hpwmi_tecla_t *hpwmi_tecla(uint32_t codigo)
{
    for (size_t i = 0; i < sizeof teclas / sizeof teclas[0]; i++)
        if (teclas[i].codigo == codigo) return &teclas[i];
    return NULL;
}
