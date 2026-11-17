// SPDX-License-Identifier: GPL-3.0-only
/* tcp.c - TCP: cliente e servidor simples. Ver D046.
 *
 * Base: o TCP cliente do PR #3 do repositório original (M4R4G0N), estendido
 * com escuta/aceitação, RST para portas fechadas, estimativa do tempo de ida
 * e volta (RTO de Jacobson/Karn), retransmissão rápida com 3 ACKs repetidos,
 * sonda de janela zero e anéis de envio e recepção.
 *
 * O essencial:
 *   - abertura em três passos (SYN, SYN+ACK, ACK) com reenvio do SYN;
 *   - tudo que foi enviado fica no anel de envio até o outro lado confirmar
 *     (ACK); sem confirmação no prazo (RTO), volta ao primeiro byte não
 *     confirmado e reenvia ("go-back-N"), dobrando o prazo;
 *   - recepção em ordem num anel de 32 KB; a janela anunciada é o espaço
 *     livre do anel, então quem manda rápido demais espera o programa ler.
 *     Segmento fora de ordem é descartado (o ACK repetido faz o outro lado
 *     reenviar); um segmento repetido que traz bytes novos é aproveitado;
 *   - fechamento pelos dois lados (FIN) e RST; sem TIME-WAIT: a conexão
 *     some quando os dois FIN foram confirmados (ou 10 s depois de fechada).
 * Sem opções além do MSS, sem controle de congestionamento (manda o que a
 * janela do outro lado permitir) e sem "keep-alive". */
#include "rede.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"
#include "aleatorio.h"

#define CONEXOES   32
#define RX_TAM     32768
#define TX_TAM     16384
#define RTO_MIN    (TIQUES_POR_SEGUNDO / 5)    /* 200 ms */
#define RTO_MAX    (10 * TIQUES_POR_SEGUNDO)
#define TENTATIVAS 8

#define F_FIN 0x01
#define F_SYN 0x02
#define F_RST 0x04
#define F_PSH 0x08
#define F_ACK 0x10

enum { T_LIVRE, T_ESCUTA, T_SYN_ENVIADO, T_SYN_RECEBIDO, T_ESTABELECIDA, T_FECHADA };

typedef struct tcp_conexao {
    int      estado;
    int      erro;                /* <0: a conexão caiu (recusada, sem resposta...) */
    bool     com_socket;          /* ainda tem um socket (descritor) apontando para ela */
    int      pid;                 /* dono, para o INFO_CONEXOES */
    struct tcp_conexao *pai;      /* escuta que a criou, até ser aceita */
    bool     na_fila;             /* estabelecida, esperando o aceitar */
    int      fila_max;            /* escuta: quantas podem esperar */
    uint32_t ip;
    uint16_t porta_local, porta_remota;
    uint32_t iss, snd_una, snd_nxt, rcv_nxt;
    uint32_t janela_remota, mss, janela_anunciada;
    uint8_t *rx; uint32_t rx_ini, rx_qtd;
    uint8_t *tx; uint32_t tx_ini, tx_qtd;   /* bytes a partir de snd_una (não confirmados + não enviados) */
    bool     fin_recebido, fechar_pedido, fin_enviado;
    uint32_t rto, reenviar_em, desistir_em;
    int      tentativas;
    bool     medindo;             /* RTT: um segmento por vez (Karn) */
    uint32_t medindo_seq, medindo_desde;
    int32_t  srtt, rttvar;        /* em tiques, x8 e x4 */
    uint32_t acks_repetidos;
    uint32_t rx_total, tx_total;  /* bytes recebidos e confirmados (INFO_SOCKETS, D079) */
} tcp_t;

static tcp_t conexoes[CONEXOES];

static uint32_t agora(void) { return relogio_tiques(); }
static bool antes(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }   /* comparação de sequência com volta */
static bool passou(uint32_t t) { return (int32_t)(agora() - t) >= 0; }

/* ------------------------------------------------------------ envio */

/* Monta e manda um segmento com "n" bytes do anel de envio a partir de
 * "desloc" (contado de snd_una). */
static void segmento_para(uint32_t ip, uint16_t pl, uint16_t pr, uint32_t seq, uint32_t ack, uint8_t flags,
                          uint16_t janela, const tcp_t *c, uint32_t desloc, uint32_t n)
{
    uint8_t *t = rede_carga();
    uint32_t cab = (flags & F_SYN) ? 24 : 20;
    poe16(t, pl);
    poe16(t + 2, pr);
    poe32(t + 4, seq);
    poe32(t + 8, (flags & F_ACK) ? ack : 0);
    t[12] = (uint8_t)((cab / 4) << 4);
    t[13] = flags;
    poe16(t + 14, janela);
    poe16(t + 16, 0);
    poe16(t + 18, 0);
    if (flags & F_SYN) { t[20] = 2; t[21] = 4; poe16(t + 22, (uint16_t)rede_mss_para(ip)); }   /* opção MSS (D363: menor por el túnel) */
    for (uint32_t i = 0; i < n; i++) t[cab + i] = c->tx[(c->tx_ini + desloc + i) % TX_TAM];
    poe16(t + 16, soma_pseudo(rede_origem_para(ip), ip, IP_TCP, t, cab + n));
    ip_enviar(ip, IP_TCP, cab + n);
}

static void segmento(tcp_t *c, uint32_t seq, uint8_t flags, uint32_t desloc, uint32_t n)
{
    uint32_t livre = RX_TAM - c->rx_qtd;
    c->janela_anunciada = livre > 65535 ? 65535 : livre;
    segmento_para(c->ip, c->porta_local, c->porta_remota, seq, c->rcv_nxt, flags,
                  (uint16_t)c->janela_anunciada, c, desloc, n);
}

/* Resposta a um segmento que não pertence a nenhuma conexão (RFC 793). */
static void rst_responder(uint32_t origem, const uint8_t *t, uint32_t tam_dados)
{
    uint8_t flags = t[13];
    if (flags & F_RST) return;
    uint16_t po = be16(t), pd = be16(t + 2);
    if (flags & F_ACK) segmento_para(origem, pd, po, be32(t + 8), 0, F_RST, 0, NULL, 0, 0);
    else {
        uint32_t ack = be32(t + 4) + tam_dados + ((flags & F_SYN) ? 1 : 0) + ((flags & F_FIN) ? 1 : 0);
        segmento_para(origem, pd, po, 0, ack, F_RST | F_ACK, 0, NULL, 0, 0);
    }
}

static void armar_reenvio(tcp_t *c) { c->reenviar_em = agora() + c->rto; }

/* Manda o que cabe na janela do outro lado; depois dos dados, o FIN. */
static void enviar_pendente(tcp_t *c)
{
    if (c->estado != T_ESTABELECIDA) return;
    for (;;) {
        uint32_t em_voo = c->snd_nxt - c->snd_una - (c->fin_enviado ? 1 : 0);
        if (em_voo >= c->tx_qtd) break;
        uint32_t janela = c->janela_remota > em_voo ? c->janela_remota - em_voo : 0;
        uint32_t n = c->tx_qtd - em_voo;
        if (n > c->mss) n = c->mss;
        if (n > janela) n = janela;
        if (!n) {
            /* Janela zero: a sonda sai quando o prazo vencer (tcp_tique). */
            if (!c->reenviar_em) armar_reenvio(c);
            break;
        }
        segmento(c, c->snd_nxt, F_ACK | F_PSH, em_voo, n);
        if (!c->reenviar_em) armar_reenvio(c);
        if (!c->medindo) { c->medindo = true; c->medindo_seq = c->snd_nxt; c->medindo_desde = agora(); }
        c->snd_nxt += n;
    }
    uint32_t em_voo = c->snd_nxt - c->snd_una;
    if (c->fechar_pedido && !c->fin_enviado && em_voo == c->tx_qtd) {
        segmento(c, c->snd_nxt, F_FIN | F_ACK, 0, 0);
        if (!c->reenviar_em) armar_reenvio(c);
        c->snd_nxt++;
        c->fin_enviado = true;
    }
}

/* ------------------------------------------------------------ vida da conexão */

static tcp_t *nova(void)
{
    for (int i = 0; i < CONEXOES; i++)
        if (conexoes[i].estado == T_LIVRE) {
            tcp_t *c = &conexoes[i];
            memset(c, 0, sizeof *c);
            return c;
        }
    return NULL;
}

static bool buffers(tcp_t *c)
{
    c->rx = kmalloc(RX_TAM);
    c->tx = kmalloc(TX_TAM);
    if (c->rx && c->tx) return true;
    if (c->rx) kfree(c->rx);
    if (c->tx) kfree(c->tx);
    c->rx = c->tx = NULL;
    return false;
}

/* Despierta a quien espera en esta conexión y avisa de la actividad de la red
 * a quien espera en varios sockets a la vez (SIS_ESPERAR_REDE, D117). */
static void despertar(const tcp_t *c)
{
    acordar(c);
    rede_atividade();
}

static void liberar(tcp_t *c)
{
    if (c->rx) kfree(c->rx);
    if (c->tx) kfree(c->tx);
    despertar(c);
    memset(c, 0, sizeof *c);
}

static bool solta(const tcp_t *c) { return !c->com_socket && !c->pai; }

static void cair(tcp_t *c, int erro)
{
    if (!c->erro) c->erro = erro;
    c->estado = T_FECHADA;
    c->reenviar_em = 0;
    despertar(c);
    if (c->pai) despertar(c->pai);
    if (solta(c)) liberar(c);
}

/* Número de sequência inicial imprevisível (D061): sem ele, quem está na
 * rede pode adivinhar o próximo e injetar segmentos numa conexão. */
static uint32_t isn(void)
{
    return aleatorio_u32();
}

uint16_t tcp_porta_livre(void)
{
    static uint16_t proxima;
    for (int t = 0; t < 16000; t++) {
        uint16_t p = (uint16_t)(32768 + (proxima++ + agora()) % 7000);   /* 32768-39767; o UDP usa 40000+ */
        bool usada = false;
        for (int i = 0; i < CONEXOES && !usada; i++)
            if (conexoes[i].estado != T_LIVRE && conexoes[i].porta_local == p) usada = true;
        if (!usada) return p;
    }
    return 0;
}

/* ------------------------------------------------------------ chegada */

static void ler_mss(tcp_t *c, const uint8_t *t, uint32_t cab)
{
    c->mss = 536;
    for (uint32_t i = 20; i < cab;) {          /* procura a opção MSS */
        if (t[i] == 0) break;
        if (t[i] == 1) { i++; continue; }
        if (i + 1 >= cab || t[i + 1] < 2 || i + t[i + 1] > cab) break;
        if (t[i] == 2 && t[i + 1] == 4) c->mss = be16(t + i + 2);
        i += t[i + 1];
    }
    if (c->mss > rede_mss_para(c->ip)) c->mss = rede_mss_para(c->ip);   /* D363: 1380 por el túnel */
    if (c->mss < 64) c->mss = 64;
}

static void medir_rtt(tcp_t *c, uint32_t ack)
{
    if (!c->medindo || !antes(c->medindo_seq, ack)) return;
    c->medindo = false;
    int32_t amostra = (int32_t)(agora() - c->medindo_desde);
    if (c->srtt == 0) { c->srtt = amostra * 8 + 1; c->rttvar = amostra * 2; }
    else {
        int32_t delta = amostra - (c->srtt >> 3);
        c->srtt += delta;
        if (c->srtt <= 0) c->srtt = 1;
        if (delta < 0) delta = -delta;
        c->rttvar += delta - (c->rttvar >> 2);
    }
    uint32_t rto = (uint32_t)((c->srtt >> 3) + c->rttvar);
    c->rto = rto < RTO_MIN ? RTO_MIN : rto > RTO_MAX ? RTO_MAX : rto;
}

/* Uma escuta recebeu um SYN: cria a conexão filha em SYN_RECEBIDO. */
static void aceitar_syn(tcp_t *e, uint32_t origem, const uint8_t *t, uint32_t cab)
{
    int na_fila = 0;
    for (int i = 0; i < CONEXOES; i++)
        if (conexoes[i].estado != T_LIVRE && conexoes[i].pai == e) na_fila++;
    if (na_fila >= e->fila_max) return;   /* fila cheia: o outro lado repete o SYN depois */
    tcp_t *c = nova();
    if (!c || !buffers(c)) { if (c) c->estado = T_LIVRE; return; }
    c->estado = T_SYN_RECEBIDO;
    c->pai = e;
    c->pid = e->pid;
    c->ip = origem;
    c->porta_local = e->porta_local;
    c->porta_remota = be16(t);
    c->rcv_nxt = be32(t + 4) + 1;
    c->iss = isn();
    c->snd_una = c->iss;
    c->snd_nxt = c->iss + 1;
    c->janela_remota = be16(t + 14);
    ler_mss(c, t, cab);
    c->rto = TIQUES_POR_SEGUNDO;
    segmento(c, c->iss, F_SYN | F_ACK, 0, 0);
    armar_reenvio(c);
}

void tcp_receber(uint32_t origem, uint32_t destino, const uint8_t *t, uint32_t n)
{
    if (n < 20) return;
    uint32_t cab = (t[12] >> 4) * 4u;
    if (cab < 20 || cab > n) return;
    if (soma_pseudo(origem, destino, IP_TCP, t, n) != 0) return;

    uint16_t po = be16(t), pd = be16(t + 2);
    uint32_t seq = be32(t + 4), ack = be32(t + 8);
    uint8_t flags = t[13];
    const uint8_t *dados = t + cab;
    uint32_t tam = n - cab;
    if (po == 0 || pd == 0) return;

    tcp_t *c = NULL, *escuta = NULL;
    for (int i = 0; i < CONEXOES; i++) {
        tcp_t *x = &conexoes[i];
        if (x->estado == T_LIVRE || x->porta_local != pd) continue;
        if (x->estado == T_ESCUTA) escuta = x;
        else if (x->ip == origem && x->porta_remota == po) c = x;
    }
    if (!c || c->estado == T_FECHADA) {
        if (!c && escuta && (flags & (F_SYN | F_ACK | F_RST)) == F_SYN) { aceitar_syn(escuta, origem, t, cab); return; }
        if (!c) rst_responder(origem, t, tam);   /* porta fechada: "conexão recusada" para o outro lado */
        return;
    }

    if (c->estado == T_SYN_ENVIADO) {
        if ((flags & F_ACK) && ack != c->iss + 1) { if (!(flags & F_RST)) rst_responder(origem, t, tam); return; }
        if (flags & F_RST) { if (flags & F_ACK) cair(c, ERRO_RECUSADA); return; }
        if (!(flags & F_SYN) || !(flags & F_ACK)) return;
        c->rcv_nxt = seq + 1;
        c->snd_una = c->snd_nxt = ack;
        c->janela_remota = be16(t + 14);
        ler_mss(c, t, cab);
        c->estado = T_ESTABELECIDA;
        c->tentativas = 0;
        c->rto = TIQUES_POR_SEGUNDO;
        c->reenviar_em = 0;
        segmento(c, c->snd_nxt, F_ACK, 0, 0);
        despertar(c);
        return;
    }

    /* RST: só vale com o número de sequência dentro da janela. */
    if (flags & F_RST) {
        if (seq == c->rcv_nxt || (!antes(seq, c->rcv_nxt) && antes(seq, c->rcv_nxt + c->janela_anunciada + 1))) {
            if (c->estado == T_SYN_RECEBIDO && c->pai) { c->pai = NULL; liberar(c); return; }
            cair(c, ERRO_CONEXAO_CAIU);
        }
        return;
    }
    if (c->estado == T_SYN_RECEBIDO) {
        if ((flags & F_SYN) && !(flags & F_ACK)) {   /* o nosso SYN+ACK se perdeu */
            segmento(c, c->iss, F_SYN | F_ACK, 0, 0);
            return;
        }
        if (!(flags & F_ACK) || ack != c->iss + 1) { if (flags & F_ACK) rst_responder(origem, t, tam); return; }
        c->snd_una = ack;
        c->estado = T_ESTABELECIDA;
        c->tentativas = 0;
        c->rto = TIQUES_POR_SEGUNDO;
        c->reenviar_em = 0;
        c->na_fila = true;
        if (c->pai) despertar(c->pai);
        /* continua: o ACK pode trazer dados */
    }

    if (flags & F_ACK) {
        uint16_t janela = be16(t + 14);
        if (antes(c->snd_una, ack) && !antes(c->snd_nxt, ack)) {   /* confirma algo novo */
            uint32_t conf = ack - c->snd_una;
            if (c->fin_enviado && ack == c->snd_nxt) conf--;      /* o FIN ocupa um número, não um byte */
            if (conf > c->tx_qtd) conf = c->tx_qtd;
            c->tx_ini = (c->tx_ini + conf) % TX_TAM;
            c->tx_qtd -= conf;
            c->tx_total += conf;
            medir_rtt(c, ack);
            c->snd_una = ack;
            c->tentativas = 0;
            c->acks_repetidos = 0;
            c->reenviar_em = c->snd_una == c->snd_nxt ? 0 : agora() + c->rto;
            despertar(c);                                            /* abriu espaço para escrever */
        } else if (ack == c->snd_una && tam == 0 && !(flags & (F_SYN | F_FIN)) && c->snd_nxt != c->snd_una &&
                   janela == c->janela_remota) {
            /* ACK repetido: no terceiro, reenvia sem esperar o prazo. */
            if (++c->acks_repetidos == 3) {
                c->snd_nxt = c->snd_una;
                c->fin_enviado = false;
                c->medindo = false;
                c->reenviar_em = 0;
            }
        }
        c->janela_remota = janela;
    }

    bool responder = false;
    if (tam || (flags & F_FIN)) {
        if (antes(seq, c->rcv_nxt)) {          /* repetido: aproveita só o que for novo */
            uint32_t pular = c->rcv_nxt - seq;
            if (pular > tam) { tam = 0; flags = (uint8_t)(flags & ~F_FIN); }   /* até o FIN já tinha vindo */
            else { dados += pular; tam -= pular; }
            seq = c->rcv_nxt;
        }
        if (seq == c->rcv_nxt && !c->fin_recebido) {
            if (!c->com_socket && !c->pai && tam) {   /* ninguém mais vai ler: corta */
                segmento(c, c->snd_nxt, F_RST | F_ACK, 0, 0);
                cair(c, ERRO_CONEXAO_CAIU);
                return;
            }
            uint32_t livre = RX_TAM - c->rx_qtd, k = tam < livre ? tam : livre;
            for (uint32_t i = 0; i < k; i++) c->rx[(c->rx_ini + c->rx_qtd + i) % RX_TAM] = dados[i];
            c->rx_qtd += k;
            c->rx_total += k;
            c->rcv_nxt += k;
            if ((flags & F_FIN) && k == tam) { c->rcv_nxt++; c->fin_recebido = true; }
            if (k || c->fin_recebido) despertar(c);
        }
        responder = true;   /* em ordem ou não, confirma até onde temos */
    }
    if (flags & F_SYN) responder = true;   /* o SYN+ACK voltou: o nosso ACK se perdeu */
    if (responder) segmento(c, c->snd_nxt, F_ACK, 0, 0);
    enviar_pendente(c);
    /* Os dois lados já fecharam e o nosso FIN foi confirmado: acabou. */
    if (c->fin_recebido && c->fin_enviado && c->snd_una == c->snd_nxt) {
        if (solta(c)) liberar(c);
        else { c->estado = T_FECHADA; c->reenviar_em = 0; despertar(c); }
    }
}

void tcp_tique(void)
{
    for (int i = 0; i < CONEXOES; i++) {
        tcp_t *c = &conexoes[i];
        if (c->estado == T_LIVRE || c->estado == T_ESCUTA) continue;
        if (c->estado == T_FECHADA) { if (solta(c)) liberar(c); continue; }
        if (solta(c) && c->desistir_em && passou(c->desistir_em)) { liberar(c); continue; }
        if (!c->reenviar_em || !passou(c->reenviar_em)) continue;
        bool sonda = c->estado == T_ESTABELECIDA && c->janela_remota == 0 && c->snd_nxt == c->snd_una;
        int limite = (c->estado == T_SYN_ENVIADO || c->estado == T_SYN_RECEBIDO) ? 5 : TENTATIVAS;   /* SYN: ~25 s */
        if (!sonda && ++c->tentativas > limite) {
            if (c->estado == T_SYN_RECEBIDO && c->pai) { c->pai = NULL; liberar(c); continue; }
            cair(c, c->estado == T_SYN_ENVIADO ? ERRO_TEMPO : ERRO_CONEXAO_CAIU);
            continue;
        }
        c->rto = c->rto * 2 > RTO_MAX ? RTO_MAX : c->rto * 2;
        c->medindo = false;
        if (c->estado == T_SYN_ENVIADO) {
            segmento(c, c->iss, F_SYN, 0, 0);
            armar_reenvio(c);
        } else if (c->estado == T_SYN_RECEBIDO) {
            segmento(c, c->iss, F_SYN | F_ACK, 0, 0);
            armar_reenvio(c);
        } else if (sonda) {
            /* Sonda de janela zero: um byte além da janela; a resposta traz a janela nova. */
            if (c->tx_qtd) { segmento(c, c->snd_nxt, F_ACK, 0, 1); c->snd_nxt++; }
            armar_reenvio(c);
        } else {
            /* Volta ao primeiro byte não confirmado e manda de novo. */
            c->snd_nxt = c->snd_una;
            c->fin_enviado = false;
            c->reenviar_em = 0;
            enviar_pendente(c);
            if (!c->reenviar_em) armar_reenvio(c);
        }
    }
}

/* ------------------------------------------------------------ para os sockets */

tcp_t *tcp_abrir(uint32_t ip, uint16_t porta, int32_t *erro)
{
    tcp_t *c = nova();
    if (!c) { *erro = ERRO_LIMITE; return NULL; }
    if (!buffers(c)) { *erro = ERRO_SEM_MEMORIA; return NULL; }
    uint16_t local = tcp_porta_livre();
    if (!local) { liberar(c); *erro = ERRO_LIMITE; return NULL; }
    c->estado = T_SYN_ENVIADO;
    c->com_socket = true;
    c->pid = processo_atual()->pid;
    c->ip = ip;
    c->porta_remota = porta;
    c->porta_local = local;
    c->iss = isn();
    c->snd_una = c->snd_nxt = c->iss + 1;
    c->rto = TIQUES_POR_SEGUNDO;
    c->mss = 536;
    segmento(c, c->iss, F_SYN, 0, 0);
    armar_reenvio(c);
    return c;
}

tcp_t *tcp_nova_escuta(uint16_t porta, int fila, int32_t *erro)
{
    for (int i = 0; i < CONEXOES; i++)
        if (conexoes[i].estado == T_ESCUTA && conexoes[i].porta_local == porta) { *erro = ERRO_OCUPADO; return NULL; }
    tcp_t *c = nova();
    if (!c) { *erro = ERRO_LIMITE; return NULL; }
    c->estado = T_ESCUTA;
    c->com_socket = true;
    c->pid = processo_atual()->pid;
    c->porta_local = porta;
    c->fila_max = fila < 1 ? 1 : fila > 8 ? 8 : fila;
    return c;
}

/* 0 = estabelecida (ou já fechando), 1 = ainda conectando, <0 = erro. */
int32_t tcp_situacao(tcp_t *c)
{
    if (c->estado == T_SYN_ENVIADO) return 1;
    if (c->estado == T_FECHADA && c->erro) return c->erro;
    return 0;
}

tcp_t *tcp_tirar_da_fila(tcp_t *e)
{
    for (int i = 0; i < CONEXOES; i++) {
        tcp_t *c = &conexoes[i];
        if (c->estado != T_LIVRE && c->pai == e && c->na_fila) {
            c->pai = NULL;
            c->na_fila = false;
            c->com_socket = true;
            c->pid = processo_atual()->pid;
            return c;
        }
    }
    return NULL;
}

int32_t tcp_ler(tcp_t *c, char *destino, uint32_t n)
{
    if (c->estado == T_ESCUTA) return ERRO_INVALIDO;
    if (n == 0) return 0;
    if (c->rx_qtd == 0) {
        if (c->fin_recebido) return 0;              /* fim do arquivo */
        if (c->estado == T_FECHADA) return c->erro ? c->erro : 0;
        return SIS_REPETIR;                          /* quem chamou bloqueia no canal c */
    }
    uint32_t k = n < c->rx_qtd ? n : c->rx_qtd;
    for (uint32_t i = 0; i < k; i++) destino[i] = (char)c->rx[(c->rx_ini + i) % RX_TAM];
    c->rx_ini = (c->rx_ini + k) % RX_TAM;
    c->rx_qtd -= k;
    /* A janela anunciada estava pequena: avisa o outro lado que abriu espaço. */
    if (c->estado == T_ESTABELECIDA && c->janela_anunciada < RX_TAM / 2 && RX_TAM - c->rx_qtd >= RX_TAM / 2)
        segmento(c, c->snd_nxt, F_ACK, 0, 0);
    return (int32_t)k;
}

int32_t tcp_escrever(tcp_t *c, const char *origem, uint32_t n)
{
    if (c->estado == T_ESCUTA || c->estado == T_SYN_ENVIADO || c->estado == T_SYN_RECEBIDO) return ERRO_INVALIDO;
    if (c->estado == T_FECHADA) return c->erro ? c->erro : ERRO_PIPE_QUEBRADO;
    if (c->fechar_pedido) return ERRO_PIPE_QUEBRADO;
    if (n == 0) return 0;
    uint32_t livre = TX_TAM - c->tx_qtd;
    if (!livre) return SIS_REPETIR;
    uint32_t k = n < livre ? n : livre;
    for (uint32_t i = 0; i < k; i++) c->tx[(c->tx_ini + c->tx_qtd + i) % TX_TAM] = (uint8_t)origem[i];
    c->tx_qtd += k;
    enviar_pendente(c);
    return (int32_t)k;
}

int32_t tcp_disponivel(tcp_t *c)
{
    if (c->estado == T_ESCUTA) {
        for (int i = 0; i < CONEXOES; i++)
            if (conexoes[i].estado != T_LIVRE && conexoes[i].pai == c && conexoes[i].na_fila) return 1;
        return 0;
    }
    if (c->rx_qtd) return (int32_t)c->rx_qtd;
    if (c->fin_recebido || c->estado == T_FECHADA) return ERRO_PIPE_QUEBRADO;
    return 0;
}

void tcp_fechar_envio(tcp_t *c)
{
    if (c->estado != T_ESTABELECIDA || c->fechar_pedido) return;
    c->fechar_pedido = true;
    enviar_pendente(c);
}

/* O socket fechou: escuta some (e as filhas não aceitas levam RST); uma
 * conexão manda o que falta e o FIN, e termina sozinha (no máximo 10 s
 * depois). Se ainda havia dados por ler, corta com RST, como os Unix. */
void tcp_soltar(tcp_t *c)
{
    c->com_socket = false;
    if (c->estado == T_ESCUTA) {
        for (int i = 0; i < CONEXOES; i++) {
            tcp_t *f = &conexoes[i];
            if (f->estado == T_LIVRE || f->pai != c) continue;
            f->pai = NULL;
            if (f->estado != T_FECHADA) segmento(f, f->snd_nxt, F_RST | F_ACK, 0, 0);
            liberar(f);
        }
        liberar(c);
        return;
    }
    if (c->estado == T_FECHADA || c->estado == T_SYN_ENVIADO) {
        if (c->estado == T_SYN_ENVIADO) segmento(c, c->snd_nxt, F_RST, 0, 0);
        liberar(c);
        return;
    }
    if (c->rx_qtd) {
        segmento(c, c->snd_nxt, F_RST | F_ACK, 0, 0);
        liberar(c);
        return;
    }
    c->desistir_em = agora() + 10 * TIQUES_POR_SEGUNDO;
    tcp_fechar_envio(c);
    if (c->fin_recebido && c->fin_enviado && c->snd_una == c->snd_nxt) liberar(c);
}

void tcp_enderecos(tcp_t *c, uint32_t *ip_l, uint16_t *p_l, uint32_t *ip_r, uint16_t *p_r)
{
    *ip_l = rede_origem_para(c->ip);
    *p_l = c->porta_local;
    *ip_r = c->ip;
    *p_r = c->porta_remota;
}

static uint32_t estado_abi(const tcp_t *c)
{
    switch (c->estado) {
    case T_ESCUTA: return CON_ESCUTANDO;
    case T_SYN_ENVIADO: return CON_SYN_ENVIADO;
    case T_SYN_RECEBIDO: return CON_SYN_RECEBIDO;
    case T_ESTABELECIDA: return (c->fin_recebido || c->fin_enviado) ? CON_FECHANDO : CON_ESTABELECIDA;
    default: return CON_FECHADA;
    }
}

uint32_t tcp_estado(tcp_t *c) { return estado_abi(c); }

int tcp_listar(void *v, int max)
{
    info_conexao_t *o = v;
    int k = 0;
    for (int i = 0; i < CONEXOES && k < max; i++) {
        tcp_t *c = &conexoes[i];
        if (c->estado == T_LIVRE) continue;
        o[k].tipo = SOCK_TCP;
        o[k].estado = estado_abi(c);
        o[k].ip_local = c->estado == T_ESCUTA ? rede_meu_ip() : rede_origem_para(c->ip);
        o[k].porta_local = c->porta_local;
        o[k].ip_remoto = c->ip;
        o[k].porta_remota = c->porta_remota;
        o[k].na_fila_rx = c->rx_qtd;
        o[k].na_fila_tx = c->tx_qtd;
        o[k].pid = c->com_socket ? c->pid : 0;
        k++;
    }
    return k;
}

/* ------------------------------------------------------------ Administrador de tareas (D079) */

static uint32_t estado_tcp(const tcp_t *c)
{
    switch (c->estado) {
    case T_ESCUTA: return TCP_ESCUTA;
    case T_SYN_ENVIADO: return TCP_SYN_ENVIADO;
    case T_SYN_RECEBIDO: return TCP_SYN_RECEBIDO;
    case T_ESTABELECIDA:
        if (c->fin_enviado && c->fin_recebido) return TCP_ULTIMO_ACK;
        if (c->fin_enviado) return TCP_FIN_ESPERA;
        if (c->fin_recebido) return TCP_FECHAR_ESPERA;
        return TCP_ESTABELECIDA;
    default: return TCP_FECHADA;
    }
}

/* INFO_SOCKETS: como tcp_listar, com o estado do RFC 793, os bytes e um id
 * (o índice + 1) para REDE_FECHAR. */
int tcp_listar_detalhado(info_socket_t *o, int max)
{
    int k = 0;
    for (int i = 0; i < CONEXOES && k < max; i++) {
        tcp_t *c = &conexoes[i];
        if (c->estado == T_LIVRE) continue;
        memset(&o[k], 0, sizeof o[k]);
        o[k].c.tipo = SOCK_TCP;
        o[k].c.estado = estado_abi(c);
        o[k].c.ip_local = c->estado == T_ESCUTA ? rede_meu_ip() : rede_origem_para(c->ip);
        o[k].c.porta_local = c->porta_local;
        o[k].c.ip_remoto = c->ip;
        o[k].c.porta_remota = c->porta_remota;
        o[k].c.na_fila_rx = c->rx_qtd;
        o[k].c.na_fila_tx = c->tx_qtd;
        o[k].c.pid = (c->com_socket || c->pai) ? c->pid : 0;
        o[k].estado_tcp = estado_tcp(c);
        o[k].id = (uint32_t)i + 1;
        o[k].rx_bytes = c->rx_total;
        o[k].tx_bytes = c->tx_total;
        k++;
    }
    return k;
}

/* REDE_FECHAR: corta a conexão como um "kill" de rede. O outro lado recebe
 * um RST e o dono passa a receber ERRO_CONEXAO_CAIU no próximo ler/escrever
 * (o descritor continua dele, até fechar). Uma escuta deixa de aceitar. */
int32_t tcp_abortar(uint32_t id)
{
    if (id < 1 || id > CONEXOES) return ERRO_NAO_EXISTE;
    tcp_t *c = &conexoes[id - 1];
    if (c->estado == T_LIVRE || c->estado == T_FECHADA) return ERRO_NAO_EXISTE;
    klog("[REDE] conexao %u (porta %u -> %u) cortada a pedido (RST)\n", id, c->porta_local, c->porta_remota);
    if (c->estado != T_ESCUTA) segmento(c, c->snd_nxt, F_RST | F_ACK, 0, 0);
    if (c->estado == T_SYN_RECEBIDO && c->pai) { c->pai = NULL; liberar(c); return 0; }
    cair(c, ERRO_CONEXAO_CAIU);
    return 0;
}
