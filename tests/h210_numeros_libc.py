#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H210 (D210) - strtod/strtof y el printf de reales de la libc común, contra la glibc.

La libc común (userland/libc/numeros.c) convierte reales y texto con enteros
grandes: strtod con redondeo correcto y %e/%f/%g con las cifras exactas. Esta
prueba la compila en el PC (sin QEMU) con tests/numeros/prova_numeros.c y la
compara con la glibc, que redondea bien:

  1. dobles al azar (todos los patrones de bits, cerca de 1 y subnormales) por
     %e/%f/%g con precisión y '#' al azar, ida y vuelta con %.17g y cadenas
     decimales al azar;
  2. casos difíciles generados aquí con fracciones exactas: los puntos medios
     entre dos dobles (y entre dos float) escritos con todas sus cifras, un poco
     por encima y por debajo (con más de 800 cifras significativas), 2^-1075,
     el máximo más medio ulp, 0.1, 1e23, hexadecimales, inf/nan y sintaxis rara.

Uso: h210_numeros_libc.py [N]   (N dobles al azar; por omisión 200000)
"""
import os
import random
import struct
import subprocess
import sys
import tempfile
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import verificar, RAIZ  # noqa: E402


def decimal_exacto(fr):
    """Las cifras exactas de una fracción positiva cuyo denominador es 2^k."""
    num, den = fr.numerator, fr.denominator
    k = den.bit_length() - 1
    assert den == 1 << k
    cifras = str(num * 5 ** k)
    if k == 0:
        return cifras
    if len(cifras) <= k:
        cifras = "0" * (k - len(cifras) + 1) + cifras
    return cifras[:-k] + "." + cifras[-k:]


def doble(bits):
    return struct.unpack("<d", struct.pack("<Q", bits))[0]


def casos_dificiles(rnd):
    casos = []
    # puntos medios entre dobles vecinos
    bits = [1, 2, 3, 0x000FFFFFFFFFFFFF, 0x0010000000000000, 0x0010000000000001,
            0x7FEFFFFFFFFFFFFE, 0x3FF0000000000000, 0x4340000000000000, 0x4330000000000001]
    bits += [rnd.getrandbits(63) for _ in range(150)]
    bits += [rnd.getrandbits(52) for _ in range(60)]                      # subnormales
    bits += [(rnd.getrandbits(52) | (rnd.randint(1000, 1100) << 52)) for _ in range(60)]
    for b in bits:
        if (b >> 52) >= 0x7FF or b >= 0x7FEFFFFFFFFFFFFF:
            continue
        a, c = Fraction(doble(b)), Fraction(doble(b + 1))
        medio = (a + c) / 2
        casos.append(decimal_exacto(medio))
        # un pelo por encima y por debajo, más allá de las 800 cifras
        ulp = c - a
        for delta in (ulp / 2 ** 900, ulp / 2 ** 60):
            for s in (1, -1):
                x = medio + s * delta
                t = decimal_exacto(x)
                casos.append(t[:1500])        # cortado: lo que falta solo «pega»
                casos.append(t)
        # en notación científica
        t = decimal_exacto(medio).replace(".", "")
        t = t.lstrip("0")
        exp = len(decimal_exacto(medio).split(".")[0].lstrip("0") or "") - 1
        if decimal_exacto(medio).split(".")[0].lstrip("0") == "":
            parte = decimal_exacto(medio).split(".")[1]
            exp = -(len(parte) - len(parte.lstrip("0")) + 1)
        casos.append(t[0] + "." + t[1:] + "e" + str(exp))
    # puntos medios entre float vecinos
    for _ in range(200):
        fb = rnd.getrandbits(31)
        if (fb >> 23) >= 0xFE:
            continue
        a = Fraction(struct.unpack("<f", struct.pack("<I", fb))[0])
        c = Fraction(struct.unpack("<f", struct.pack("<I", fb + 1))[0])
        medio = (a + c) / 2
        casos.append(decimal_exacto(medio))
        casos.append(decimal_exacto(medio + (c - a) / 2 ** 80))
        casos.append(decimal_exacto(medio - (c - a) / 2 ** 80))
    # bordes
    min_sub = Fraction(1, 2 ** 1074)
    casos += [decimal_exacto(min_sub / 2), decimal_exacto(min_sub / 2 + min_sub / 2 ** 70),
              decimal_exacto(min_sub / 2 - min_sub / 2 ** 70), decimal_exacto(min_sub),
              decimal_exacto(min_sub * 3 / 4), decimal_exacto(min_sub / 4)]
    maxd = Fraction(doble(0x7FEFFFFFFFFFFFFF))
    medio_ulp = Fraction(2 ** 970)
    casos += [decimal_exacto(maxd + medio_ulp), decimal_exacto(maxd + medio_ulp - 1),
              decimal_exacto(maxd + medio_ulp + 1), decimal_exacto(maxd)]
    casos += ["0.1", "0.2", "0.3", "1e23", "8.98846567431158e307", "2.4703282292062327e-324",
              "2.4703282292062328e-324", "4.9406564584124654e-324", "2.2250738585072011e-308",
              "2.2250738585072012e-308", "1.7976931348623157e308", "1.7976931348623158e308",
              "1.7976931348623159e308", "9007199254740993", "9007199254740993.0000000001",
              "9007199254740992.9999999999", "123456789012345678901234567890", "1e-400", "1e400",
              "1e-324", "1e-325", "3.4028235e38", "3.4028236e38", "1.17549435e-38", "1.4e-45",
              "7e-46", "7.1e-46", "1e-45"]
    # sintaxis
    casos += ["", " ", ".", "-", "+", "e5", ".e1", "-.5", "+.5e-1", " \t\n 1.5", "1.5abc", "1e", "1e+",
              "1e-", "1.e3", "00000.000001e+6", "1e99999999", "1e-99999999", "0e99999999",
              "-0", "-0.0e-5", "0x", "0x.", "0x.8", "0x1p-1074", "0x1p-1075", "0x1.8p-1075",
              "0x1.fffffffffffff8p1023", "0x1.fffffffffffff7ffp1023", "0X1P+3", "0x1.8", "0xABCDEFp-4",
              "0x123456789abcdef123p0", "0x1.00000000000008p0", "0x1.00000000000008000001p0",
              "0x1p", "0x1p+", "0x1.p1", "-0x0.0p0", "infinity", "INF", "-Inf", "infinit", "inFINITY",
              "nan", "NaN", "-nan", "nan(abc_12)", "nan(", "nan()", "nan(a b)", "1,5",
              "0" * 1000 + "1", "0." + "0" * 1000 + "1", "1" + "0" * 400, "0." + "0" * 330 + "1"]
    for _ in range(300):
        n = rnd.randint(20, 1200)
        t = "".join(rnd.choice("0123456789") for _ in range(n))
        p = rnd.randint(0, n)
        t = t[:p] + "." + t[p:]
        if rnd.random() < 0.7:
            t += "e%d" % rnd.randint(-1500, 400)
        casos.append(t)
    return casos


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200000
    rnd = random.Random(210)
    with tempfile.TemporaryDirectory() as tmp:
        obj = os.path.join(tmp, "numeros.o")
        prova = os.path.join(tmp, "prova")
        r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-Dstrtod=libc_strtod",
                            "-Dstrtof=libc_strtof", "-Datof=libc_atof", "-c", "-o", obj,
                            os.path.join(RAIZ, "userland/libc/numeros.c")],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, "numeros.c compila en el PC sin avisos " + r.stderr[-500:])
        r = subprocess.run(["gcc", "-O2", "-Wall", "-o", prova,
                            os.path.join(RAIZ, "tests/numeros/prova_numeros.c"), obj],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, "la prueba compila " + r.stderr[-500:])
        r = subprocess.run([prova, "aleatorio", str(n), "210"], capture_output=True, text=True)
        print(r.stdout[-3000:])
        verificar(r.returncode == 0, "dobles al azar, %e/%f/%g e ida y vuelta iguales a la glibc")
        casos = casos_dificiles(rnd)
        r = subprocess.run([prova, "entrada"], input="\n".join(casos) + "\n",
                           capture_output=True, text=True)
        print(r.stdout[-3000:])
        verificar(r.returncode == 0, "%d casos difíciles de strtod/strtof iguales a la glibc" % len(casos))
    print("H210 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
