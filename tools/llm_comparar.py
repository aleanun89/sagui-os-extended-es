#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compara el motor `llm` (D380) con llama.cpp en el PC: tokenización, logits de cada
posición y los tokens que salen con el más probable.

  python3 tools/llm_comparar.py MODELO.gguf ["texto"] [-n 16] [--q8]
  python3 tools/llm_comparar.py --sintetico          (el modelo de tools/llm_modelo_prueba.py)
  python3 tools/llm_comparar.py --compilar           (solo compila llama.cpp y las herramientas)

llama.cpp (MIT) se baja y compila fuera de git, en ~/.local/saguitools/llama.cpp, con un
cmake de ~/.local/saguitools/llmvenv (python3 -m venv; pip install cmake). Las dos
herramientas del PC salen en build/host: `llm-pc` (el motor, make llm-pc) y
`llm_referencia` (tools/llm_referencia.c, sobre la libllama). Nada de esto va a SaguiOS."""
import array
import math
import os
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERR = os.path.expanduser("~/.local/saguitools")
LLAMA = os.path.join(HERR, "llama.cpp")
VENV = os.path.join(HERR, "llmvenv")
HOST = os.path.join(RAIZ, "build", "host")
LLM = os.path.join(HOST, "llm-pc")
REF = os.path.join(HOST, "llm_referencia")


def entorno_pc():
    """D191: las herramientas del PC usan las cabeceras del anfitrión (HOST_CPATH)."""
    e = dict(os.environ)
    if "HOST_CPATH" in e:
        e["CPATH"] = e["HOST_CPATH"]
    return e


def compilar_motor():
    """El motor para el PC, con la x87 como en SaguiOS (make llm-pc)."""
    subprocess.run(["make", "-s", "llm-pc"], cwd=RAIZ, check=True, stdout=subprocess.DEVNULL)


def compilar_llama():
    if not os.path.isdir(LLAMA):
        subprocess.run(["git", "clone", "--depth", "1", "https://github.com/ggml-org/llama.cpp.git", LLAMA], check=True)
    cmake = os.path.join(VENV, "bin", "cmake")
    if not os.path.exists(cmake):
        subprocess.run([sys.executable, "-m", "venv", VENV], check=True)
        subprocess.run([os.path.join(VENV, "bin", "pip"), "install", "-q", "cmake"], check=True)
    if not os.path.exists(os.path.join(LLAMA, "build", "bin", "libllama.so")):
        subprocess.run([cmake, "-B", "build", "-DGGML_NATIVE=OFF", "-DLLAMA_CURL=OFF", "-DGGML_OPENMP=OFF",
                        "-DCMAKE_BUILD_TYPE=Release", "-DLLAMA_BUILD_SERVER=OFF", "-DLLAMA_BUILD_TESTS=OFF"],
                       cwd=LLAMA, check=True, env=entorno_pc())
        subprocess.run([cmake, "--build", "build", "-j6", "--target", "llama", "llama-tokenize"], cwd=LLAMA, check=True,
                       env=entorno_pc())
    os.makedirs(HOST, exist_ok=True)
    subprocess.run(["gcc", "-O2", "-Wno-deprecated-declarations", "-o", REF, os.path.join(RAIZ, "tools", "llm_referencia.c"),
                    "-I" + os.path.join(LLAMA, "include"), "-I" + os.path.join(LLAMA, "ggml", "include"),
                    "-L" + os.path.join(LLAMA, "build", "bin"), "-lllama", "-lggml", "-lggml-base",
                    "-Wl,-rpath," + os.path.join(LLAMA, "build", "bin")], check=True, env=entorno_pc())


def ler_floats(caminho):
    a = array.array("f")
    with open(caminho, "rb") as f:
        a.frombytes(f.read())
    return a


def gerados(saida):
    for linha in saida.splitlines():
        if linha.startswith("GERADOS:"):
            return [int(x) for x in linha.split()[1:]]
    return []


def tokenizar(modelo, texto):
    r = subprocess.run([LLM, "-m", modelo, "--contexto", "256", "--tokenizar", texto, "--especiales"],
                       capture_output=True, text=True, check=True)
    return [int(x) for x in r.stdout.split("TOKENS:")[1].split()]


def comparar(modelo, texto, n, q8, ctx=2048, tol_top1=0.9):
    toks = tokenizar(modelo, texto)
    ids = ",".join(map(str, toks))
    tmp = os.path.join(HOST, "llm_cmp")
    os.makedirs(tmp, exist_ok=True)
    fr, fm = os.path.join(tmp, "ref.bin"), os.path.join(tmp, "mio.bin")
    r = subprocess.run([REF, modelo, str(ctx), ids, str(n), fr] + (["q8"] if q8 else []), capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        raise SystemExit("llm_referencia falló")
    gr = gerados(r.stdout)
    m = subprocess.run([LLM, "-m", modelo, "--contexto", str(ctx), "--tokens", ids, "-n", str(n), "--volcar-logits", fm],
                       capture_output=True, text=True)
    if m.returncode:
        print(m.stderr)
        raise SystemExit("llm falló")
    gm = gerados(m.stdout)
    a, b = ler_floats(fr), ler_floats(fm)
    nv = len(a) // (len(toks) + n - 1)
    filas = min(len(a), len(b)) // nv
    peor, cos_min, top1 = 0.0, 1.0, 0
    for i in range(filas):
        x, y = a[i * nv:(i + 1) * nv], b[i * nv:(i + 1) * nv]
        d = max(abs(p - q) for p, q in zip(x, y))
        nx = math.sqrt(sum(p * p for p in x)) or 1
        ny = math.sqrt(sum(q * q for q in y)) or 1
        cs = sum(p * q for p, q in zip(x, y)) / (nx * ny)
        peor = max(peor, d)
        cos_min = min(cos_min, cs)
        top1 += max(range(nv), key=x.__getitem__) == max(range(nv), key=y.__getitem__)
    print("tokens de entrada: %d; posiciones comparadas: %d; vocabulario %d" % (len(toks), filas, nv))
    print("diferencia máxima de logits: %.4f; coseno mínimo: %.6f; mismo top-1: %d/%d" % (peor, cos_min, top1, filas))
    print("llama.cpp: %s" % gr)
    print("llm:       %s" % gm)
    ok = gr == gm or top1 >= tol_top1 * filas
    print("RESULTADO:", "OK" if ok else "DIFERENTE")
    return ok


def main():
    args = sys.argv[1:]
    if "--compilar" in args:
        compilar_llama()
        compilar_motor()
        return
    compilar_motor()
    if not os.path.exists(REF):
        compilar_llama()
    q8 = "--q8" in args
    n = 16
    if "-n" in args:
        n = int(args[args.index("-n") + 1])
        del args[args.index("-n"):args.index("-n") + 2]
    args = [a for a in args if a != "--q8"]
    if args and args[0] == "--sintetico":
        modelo = os.path.join(HOST, "llm_cmp", "sintetico.gguf")
        os.makedirs(os.path.dirname(modelo), exist_ok=True)
        subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "llm_modelo_prueba.py"), modelo], check=True)
        texto = args[1] if len(args) > 1 else "<|startoftext|><|im_start|>user\nhola sagui<|im_end|>\n<|im_start|>assistant\n"
    else:
        modelo = args[0]
        texto = args[1] if len(args) > 1 else ("<|startoftext|><|im_start|>user\n¿Cuál es la capital de Francia? "
                                               "Responde en una frase.<|im_end|>\n<|im_start|>assistant\n<think></think>")
    sys.exit(0 if comparar(modelo, texto, n, q8) else 1)


if __name__ == "__main__":
    main()
