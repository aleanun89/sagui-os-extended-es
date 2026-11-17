#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""inflar_unidad (v8.5.4.2, D379 P5) - el inflate del núcleo (kernel/lib/inflar.c) frente a zlib, en el PC.

Compila kernel/lib/inflar.c con el gcc del PC (con tipos.h y texto.h de sustitución) y descomprime flujos DEFLATE
crudos hechos con zlib mezclando bloques comprimidos, guardados (datos al azar) y vaciados Z_BLOCK y Z_SYNC_FLUSH: el
resultado tiene que ser byte a byte el de zlib. La revisión de la v8.5.4.2 encontró que un bloque guardado tras
otro que dejaba bytes enteros en el acumulador se leía mal (13 de 300 flujos); el mismo fallo estaba en
userland/libsagui/compressao.c. También comprueba el CRC-32 contra zlib.crc32.
"""
import os
import random
import subprocess
import sys
import tempfile
import zlib

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PRINCIPAL = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inflar.h"
int main(int c, char **v)
{
    static uint8_t ent[1 << 20], sai[1 << 17], ref[1 << 17];
    FILE *f = fopen(v[1], "rb"); int n = (int)fread(ent, 1, sizeof ent, f); fclose(f);
    f = fopen(v[2], "rb"); int m = (int)fread(ref, 1, sizeof ref, f); fclose(f);
    static inflar_ctx_t ctx;
    int r = inflar_bloco(&ctx, ent, n, sai, (int)sizeof sai);
    printf("%d %d %u\n", r, r == m && !memcmp(ref, sai, (size_t)m), inflar_crc32(0, ref, (uint32_t)m));
    return 0;
}
'''


def main():
    with tempfile.TemporaryDirectory() as d:
        inc = os.path.join(d, "inc")
        os.makedirs(inc)
        with open(os.path.join(inc, "tipos.h"), "w") as f:
            f.write("#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n")
        with open(os.path.join(inc, "texto.h"), "w") as f:
            f.write("#include <string.h>\n")
        # inflar.h junto a los de sustitución (si no, su #include "tipos.h" tomaría el del núcleo)
        with open(os.path.join(RAIZ, "kernel", "inc", "inflar.h")) as f, open(os.path.join(inc, "inflar.h"), "w") as g:
            g.write(f.read())
        with open(os.path.join(d, "main.c"), "w") as f:
            f.write(PRINCIPAL)
        exe = os.path.join(d, "inf")
        env = dict(os.environ)
        env.pop("CPATH", None)
        subprocess.run(["gcc", "-O1", "-Wall", "-I", inc,
                        "-o", exe, os.path.join(d, "main.c"), os.path.join(RAIZ, "kernel", "lib", "inflar.c")],
                       check=True, env=env)
        random.seed(8542)
        falhas = 0
        casos = 400
        for t in range(casos):
            co = zlib.compressobj(9, zlib.DEFLATED, -15)
            dados, c = b"", b""
            for _ in range(random.randint(1, 7)):
                if random.random() < 0.5:
                    p = bytes(random.choice(b"ab ") for _ in range(random.randint(1, 3000)))
                else:
                    p = os.urandom(random.randint(1, 4000))
                    # incompresible: zlib lo emite como bloques guardados
                if len(dados) + len(p) > 65536:
                    break
                dados += p
                c += co.compress(p) + co.flush(random.choice((zlib.Z_BLOCK, zlib.Z_SYNC_FLUSH, zlib.Z_NO_FLUSH)))
            c += co.flush()
            assert zlib.decompress(c, -15) == dados
            fd, fr = os.path.join(d, "f.def"), os.path.join(d, "f.raw")
            open(fd, "wb").write(c)
            open(fr, "wb").write(dados)
            r, igual, crc = subprocess.run([exe, fd, fr], capture_output=True, text=True, check=True).stdout.split()
            if igual != "1" or int(crc) != (zlib.crc32(dados) & 0xFFFFFFFF):
                falhas += 1
                if falhas <= 3:
                    print(f"  caso {t}: devuelve {r}, {len(dados)} bytes esperados, crc {crc}")
        print(f"inflar_unidad: {casos - falhas}/{casos} flujos iguales a zlib")
        if falhas:
            print("inflar_unidad: FALLA")
            return 1
    print("inflar_unidad: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
