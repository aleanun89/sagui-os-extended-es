#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""raices_ssl.py - copia las raíces públicas que trae SaguiOS (D063).

Uso: python3 tools/raices_ssl.py [carpeta_origen]

Lee los certificados raíz del almacén del anfitrión (por defecto
/etc/ssl/certs, el paquete ca-certificates de Ubuntu/WSL, que es la lista de
Mozilla) y graba en rootfs/etc/ssl/certs/ un PEM por raíz, con nombre corto
(el SGFS acepta rutas de hasta 55 letras). Los certificados son datos
públicos, no código. Para cada uno comprueba con el openssl del anfitrión que es
una CA autofirmada y aún no caducada, y escribe el resumen (sujeto,
validez y huella SHA-256) en rootfs/etc/ssl/certs/LEEME.txt.

La lista cubre las cadenas de la mayoría de sitios: Let's Encrypt (ISRG X1/X2),
DigiCert, GlobalSign, Amazon, Google (GTS), Sectigo/USERTrust, Microsoft,
GoDaddy/Starfield, SSL.com (Cloudflare), HARICA (universidades
europeas) y Actalis. Cada raíz ocupa ~2 KB en la imagen.
"""
import os
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DESTINO = os.path.join(RAIZ, "rootfs", "etc", "ssl", "certs")

# nombre en el anfitrión (Ubuntu) -> nombre en SaguiOS
RAIZES = {
    "ISRG_Root_X1.pem": "isrg-root-x1.pem",
    "ISRG_Root_X2.pem": "isrg-root-x2.pem",
    "DigiCert_Global_Root_G2.pem": "digicert-global-g2.pem",
    "DigiCert_Global_Root_G3.pem": "digicert-global-g3.pem",
    "DigiCert_TLS_RSA4096_Root_G5.pem": "digicert-tls-rsa4096-g5.pem",
    "DigiCert_TLS_ECC_P384_Root_G5.pem": "digicert-tls-ecc-g5.pem",
    "GlobalSign_Root_CA_-_R3.pem": "globalsign-r3.pem",
    "GlobalSign_Root_CA_-_R6.pem": "globalsign-r6.pem",
    "GlobalSign_Root_R46.pem": "globalsign-r46.pem",
    "GlobalSign_Root_E46.pem": "globalsign-e46.pem",
    "GlobalSign_ECC_Root_CA_-_R4.pem": "globalsign-ecc-r4.pem",
    "Amazon_Root_CA_1.pem": "amazon-root-ca-1.pem",
    "Amazon_Root_CA_2.pem": "amazon-root-ca-2.pem",
    "Amazon_Root_CA_3.pem": "amazon-root-ca-3.pem",
    "Amazon_Root_CA_4.pem": "amazon-root-ca-4.pem",
    "GTS_Root_R1.pem": "gts-root-r1.pem",
    "GTS_Root_R3.pem": "gts-root-r3.pem",
    "GTS_Root_R4.pem": "gts-root-r4.pem",
    "USERTrust_RSA_Certification_Authority.pem": "usertrust-rsa.pem",
    "USERTrust_ECC_Certification_Authority.pem": "usertrust-ecc.pem",
    "Sectigo_Public_Server_Authentication_Root_R46.pem": "sectigo-r46.pem",
    "Sectigo_Public_Server_Authentication_Root_E46.pem": "sectigo-e46.pem",
    "Microsoft_RSA_Root_Certificate_Authority_2017.pem": "microsoft-rsa-2017.pem",
    "Microsoft_ECC_Root_Certificate_Authority_2017.pem": "microsoft-ecc-2017.pem",
    "Go_Daddy_Root_Certificate_Authority_-_G2.pem": "godaddy-g2.pem",
    "Starfield_Root_Certificate_Authority_-_G2.pem": "starfield-g2.pem",
    "SSL.com_TLS_RSA_Root_CA_2022.pem": "sslcom-tls-rsa-2022.pem",
    "SSL.com_TLS_ECC_Root_CA_2022.pem": "sslcom-tls-ecc-2022.pem",
    "SSL.com_Root_Certification_Authority_RSA.pem": "sslcom-rsa.pem",
    "HARICA_TLS_RSA_Root_CA_2021.pem": "harica-tls-rsa-2021.pem",
    "HARICA_TLS_ECC_Root_CA_2021.pem": "harica-tls-ecc-2021.pem",
    "Actalis_Authentication_Root_CA.pem": "actalis.pem",
}


def openssl(*args, entrada=None):
    return subprocess.run(["openssl", *args], input=entrada, capture_output=True, check=True).stdout


def main():
    origem = sys.argv[1] if len(sys.argv) > 1 else "/etc/ssl/certs"
    os.makedirs(DESTINO, exist_ok=True)
    resumo = ["Raíces de confianza de SaguiOS (copiadas de %s con tools/raices_ssl.py)." % origem,
              "Añade o quita con el comando \"certificados\". Ver docs/decisoes.md, D063.", ""]
    total = 0
    for nome, novo in RAIZES.items():
        caminho = os.path.join(origem, nome)
        if not os.path.exists(caminho):
            print(f"AVISO: falta {nome} en {origem}; se omite")
            continue
        pem = openssl("x509", "-in", caminho)   # solo el bloque PEM, sin texto extra
        info = openssl("x509", "-noout", "-subject", "-enddate", "-fingerprint", "-sha256", "-nameopt", "utf8",
                       entrada=pem).decode()
        destino = os.path.join(DESTINO, novo)
        with open(destino, "wb") as f:
            f.write(pem)
        # autofirmada (la firma se comprueba con la propia clave) y aún válida
        openssl("verify", "-CAfile", destino, "-check_ss_sig", destino)
        linhas = dict(l.split("=", 1) for l in info.strip().splitlines())
        resumo.append(f"{novo}\n  {linhas['subject'].strip()}\n  caduca: {linhas['notAfter'].strip()}\n"
                      f"  SHA-256: {linhas['sha256 Fingerprint'].strip()}")
        total += len(pem)
    with open(os.path.join(DESTINO, "LEEME.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(resumo) + "\n")
    print(f"{len(resumo) - 3} raíces, {total} bytes en {DESTINO}")


if __name__ == "__main__":
    main()
