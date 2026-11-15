// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh-keygen - claves ed25519 para el cliente ssh (D317).
 *
 *   ssh-keygen [-t ed25519] [-f ARCHIVO] [-C COMENTARIO] [-N ""]   crea un par de claves
 *   ssh-keygen -l [-f ARCHIVO]      la huella SHA256 de una clave (pública o privada)
 *   ssh-keygen -y [-f ARCHIVO]      la clave pública de una privada
 *   ssh-keygen -R MÁQUINA [-f ARCHIVO]   quita una máquina de known_hosts
 *
 * El formato es el de OpenSSH («openssh-key-v1», sin frase de paso): la
 * privada sirve también para el ssh del PC y la pública (ARCHIVO.pub) es la
 * línea que va en el authorized_keys del servidor. La privada nunca se
 * enseña. Con los permisos de archivos (D282), la privada queda solo para su
 * dueño; hasta entonces, /home/.ssh es de la cuenta sagui. */
#include "sagui.h"
#include "contas.h"
#include "cripto.h"
#include "sshd_logica.h"
#include "ssh_transporte.h"
#include "ssh_cliente_logica.h"

#define DIR_SSH       "/home/.ssh"
#define CHAVE_PADRAO  DIR_SSH "/id_ed25519"
#define KNOWN_HOSTS   DIR_SSH "/known_hosts"

static const char *AYUDA =
    "uso: ssh-keygen [-t ed25519] [-f ARCHIVO] [-C COMENTARIO] [-N \"\"] [-q]\n"
    "     ssh-keygen -l [-f ARCHIVO]       huella de una clave\n"
    "     ssh-keygen -y [-f ARCHIVO]       clave pública de una privada\n"
    "     ssh-keygen -R MÁQUINA [-f ARCHIVO]   quita MÁQUINA de known_hosts (" KNOWN_HOSTS ")\n"
    "Crea un par de claves ed25519 en el formato de OpenSSH (por defecto " CHAVE_PADRAO "\n"
    "y " CHAVE_PADRAO ".pub). La línea del .pub va en el authorized_keys del servidor.\n"
    "Las frases de paso aún no se admiten: guarda la privada en un sitio seguro.\n";

static bool existe(const char *arq)
{
    info_arquivo_t i;
    return info_arquivo(arq, &i) == 0;
}

static int ler_tudo(const char *arq, char *buf, unsigned max)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) return fd;
    int n = ler(fd, buf, max - 1);
    fechar(fd);
    if (n < 0) return n;
    buf[n] = 0;
    return n;
}

static bool gravar(const char *arq, const char *texto)
{
    int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    unsigned n = (unsigned)tam_texto(texto);
    bool ok = escrever_tudo(fd, texto, n) == (int)n;
    sincronizar(fd);
    fechar(fd);
    return ok;
}

static int gerar(const char *arq, const char *comentario, bool quieto)
{
    char pubarq[200];
    formatar_texto(pubarq, sizeof pubarq, "%s.pub", arq);
    if (existe(arq) || existe(pubarq)) {
        imprimir_em(ERROS, "ssh-keygen: %s ya existe; no se sobrescribe (bórralo antes si quieres otra clave)\n",
                    existe(arq) ? arq : pubarq);
        return 1;
    }
    if (comeca_com(arq, DIR_SSH "/")) { criar_pasta("/home"); criar_pasta(DIR_SSH); }
    uint8_t semente[32], pub[32];
    uint32_t check;
    cripto_aleatorio(semente, 32);
    cripto_aleatorio(&check, sizeof check);
    ed25519_publica(pub, semente);
    static char texto[1024];
    int n = ssh_chave_privada_escrever(semente, pub, comentario, check, texto, sizeof texto);
    cripto_apagar(semente, sizeof semente);
    if (n < 0) { cripto_apagar(texto, sizeof texto); imprimir_em(ERROS, "ssh-keygen: comentario demasiado largo\n"); return 1; }
    bool ok = gravar(arq, texto);
    cripto_apagar(texto, sizeof texto);
    if (!ok) {
        apagar(arq);
        imprimir_em(ERROS, "ssh-keygen: no se pudo escribir %s\n", arq);
        return 1;
    }
    char linha[300], h[64];
    ssh_linha_publica(pub, linha, sizeof linha);
    junta_texto(linha, " ", sizeof linha);
    junta_texto(linha, comentario, sizeof linha);
    junta_texto(linha, "\n", sizeof linha);
    if (!gravar(pubarq, linha)) {
        imprimir_em(ERROS, "ssh-keygen: no se pudo escribir %s\n", pubarq);
        return 1;
    }
    ssh_huella(pub, h, sizeof h);
    if (!quieto) {
        imprimir("Clave privada: %s (no la compartas)\n", arq);
        imprimir("Clave pública: %s\n", pubarq);
        imprimir("Huella: %s %s (ED25519)\n", h, comentario);
        imprimir("Para entrar con ella, añade la línea de %s al authorized_keys del servidor.\n", pubarq);
    }
    return 0;
}

/* Clave pública de un archivo: .pub, línea de authorized_keys o privada. */
static bool publica_de(const char *arq, uint8_t pub[32], char *com, unsigned max)
{
    static char t[4096];
    int n = ler_tudo(arq, t, sizeof t);
    if (n <= 0) { cripto_apagar(t, sizeof t); return false; }
    uint8_t sem[32];
    int r = ssh_chave_privada_ler(t, (unsigned)n, sem, pub, com, max);
    cripto_apagar(sem, sizeof sem);
    if (r == SSH_CH_OK) { cripto_apagar(t, sizeof t); return true; }
    bool privada = r != SSH_CH_FORMATO;
    if (privada) {
        cripto_apagar(t, sizeof t);
        imprimir_em(ERROS, "ssh-keygen: %s: %s\n", arq, r == SSH_CH_CIFRADA ? "clave con frase de paso (no se admiten)" :
                    r == SSH_CH_TIPO ? "no es ed25519" : "clave dañada");
        return false;
    }
    char *p = t;
    while (*p) {
        char *f = p;
        while (*f && *f != '\n') f++;
        char c = *f;
        *f = 0;
        if (ssh_linha_autorizada(p, pub, com, max) == 1) return true;
        if (!c) break;
        p = f + 1;
    }
    return false;
}

static int huella(const char *arq)
{
    uint8_t pub[32];
    char com[80], h[64];
    if (!publica_de(arq, pub, com, sizeof com)) {
        imprimir_em(ERROS, "ssh-keygen: %s no tiene una clave ed25519\n", arq);
        return 1;
    }
    ssh_huella(pub, h, sizeof h);
    imprimir("256 %s %s (ED25519)\n", h, com[0] ? com : "sin comentario");
    return 0;
}

static int publica(const char *arq)
{
    static char t[4096];
    int n = ler_tudo(arq, t, sizeof t);
    uint8_t sem[32], pub[32];
    char com[80], linha[200];
    int r = n > 0 ? ssh_chave_privada_ler(t, (unsigned)n, sem, pub, com, sizeof com) : SSH_CH_FORMATO;
    cripto_apagar(sem, sizeof sem);
    cripto_apagar(t, sizeof t);
    if (r != SSH_CH_OK) {
        imprimir_em(ERROS, "ssh-keygen: %s no es una clave privada ed25519 de OpenSSH sin frase de paso\n", arq);
        return 1;
    }
    ssh_linha_publica(pub, linha, sizeof linha);
    imprimir("%s%s%s\n", linha, com[0] ? " " : "", com);
    return 0;
}

/* Quita las líneas de known_hosts cuyo patrón incluye "nome" (como OpenSSH);
 * deja la copia anterior en ARCHIVO.old. */
static int remover(const char *nome, const char *arq)
{
    int fd = abrir(arq, ABRIR_LER);
    if (fd < 0) { imprimir_em(ERROS, "ssh-keygen: no se puede abrir %s\n", arq); return 1; }
    char tmp[200], velho[200];
    formatar_texto(tmp, sizeof tmp, "%s.tmp", arq);
    formatar_texto(velho, sizeof velho, "%s.old", arq);
    int fo = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fo < 0) { fechar(fd); imprimir_em(ERROS, "ssh-keygen: no se puede escribir %s\n", tmp); return 1; }
    static leitor_linhas_t l;
    static char linha[2048];
    leitor_iniciar(&l, fd, false);
    int quitadas = 0, num = 0;
    bool com_salto;
    while (leitor_linha(&l, linha, sizeof linha, &com_salto) >= 0) {
        num++;
        const char *p = linha;
        while (*p == ' ' || *p == '\t') p++;
        const char *h0 = p;
        if (*p != '#' && *p != '@' && *p != '|') {
            while (*p && *p != ' ' && *p != '\t') p++;
            if (p > h0 && ssh_kh_lista(h0, p, nome)) {
                quitadas++;
                imprimir("# %s, línea %d: quitada\n", arq, num);
                continue;
            }
        }
        escrever_tudo(fo, linha, (unsigned)tam_texto(linha));
        escrever_tudo(fo, "\n", 1);
    }
    fechar(fd);
    sincronizar(fo);
    fechar(fo);
    if (!quitadas) {
        apagar(tmp);
        imprimir_em(ERROS, "ssh-keygen: %s no está en %s\n", nome, arq);
        return 1;
    }
    apagar(velho);
    renomear(arq, velho);
    if (renomear(tmp, arq) < 0) {
        renomear(velho, arq);
        imprimir_em(ERROS, "ssh-keygen: no se pudo reemplazar %s\n", arq);
        return 1;
    }
    imprimir("%s actualizado; la copia anterior está en %s\n", arq, velho);
    return 0;
}

int principal(int argc, char **argv)
{
    const char *tipo = "ed25519", *arq = NULL, *comentario = NULL, *frase = NULL, *maquina = NULL;
    bool ver_huella = false, ver_publica = false, quieto = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--ayuda") == 0 || compara_texto(a, "--help") == 0 || compara_texto(a, "-h") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        }
        if (a[0] != '-' || !a[1] || a[2]) { escrever_texto(ERROS, AYUDA); return 2; }
        switch (a[1]) {
        case 't': case 'f': case 'C': case 'N': case 'R': {
            if (i + 1 >= argc) { escrever_texto(ERROS, AYUDA); return 2; }
            const char *v = argv[++i];
            if (a[1] == 't') tipo = v;
            else if (a[1] == 'f') arq = v;
            else if (a[1] == 'C') comentario = v;
            else if (a[1] == 'N') frase = v;
            else maquina = v;
            break;
        }
        case 'l': ver_huella = true; break;
        case 'y': ver_publica = true; break;
        case 'q': quieto = true; break;
        default: escrever_texto(ERROS, AYUDA); return 2;
        }
    }
    if (maquina) return remover(maquina, arq ? arq : KNOWN_HOSTS);
    if (ver_huella) return huella(arq ? arq : CHAVE_PADRAO ".pub");
    if (ver_publica) return publica(arq ? arq : CHAVE_PADRAO);
    if (compara_texto(tipo, "ed25519") != 0) {
        imprimir_em(ERROS, "ssh-keygen: solo se admiten claves ed25519 (-t ed25519)\n");
        return 1;
    }
    if (frase && frase[0]) {
        imprimir_em(ERROS, "ssh-keygen: las frases de paso aún no se admiten (usa -N \"\")\n");
        return 1;
    }
    char com[80];
    if (!comentario) {
        char n[CONTA_NOME_MAX];
        conta_nome(meu_uid(), n, sizeof n);
        formatar_texto(com, sizeof com, "%s@saguios", n[0] ? n : "sagui");
        comentario = com;
    }
    for (const char *p = comentario; *p; p++)
        if ((unsigned char)*p < 0x20 || *p == 0x7F) { imprimir_em(ERROS, "ssh-keygen: comentario no válido\n"); return 1; }
    if (!quieto) imprimir("Creando un par de claves ed25519...\n");
    return gerar(arq ? arq : CHAVE_PADRAO, comentario, quieto);
}
