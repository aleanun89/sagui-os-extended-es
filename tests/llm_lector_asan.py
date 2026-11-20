#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prueba de regresión del lector de modelos de `llm` (D380, revisión de seguridad) en el PC,
con ASan y UBSan: GGUF mutados que antes desbordaban (búfer xq corto, dimensiones que dan la
vuelta, un token de 17 MB, más filas de embedding que vocabulario), un texto de más tokens
de los que caben, y el servidor con un Content-Length enorme y una conexión ociosa.
Cada caso tiene que acabar con un error claro (o funcionar, el de xq) y sin ningún informe
de ASan/UBSan. No usa QEMU. Uso: python3 tests/llm_lector_asan.py"""
import os
import random
import socket
import struct
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tools"))
import llm_modelo_prueba as P  # noqa: E402
from llm_modelo_prueba import kv, texto, matriz, vetor, TIPOS  # noqa: E402

SAIDA = os.path.join(RAIZ, "build", "host", "llm_asan")
BIN = os.path.join(SAIDA, "llm-asan")
PORTA = 18388


def gerar_mutado(saida, caso):
    r = random.Random(1)
    A = "lfm3"
    N_CAM, E, F, H, D, KVH, JAN = 8, 256, 512, 4, 64, 2, 8
    atencao = {2: False, 4: True, 6: False}
    if caso == "xq":            # Q = H*D = 8192 >> max(F, 3E): el búfer xq se queda corto
        H, D, KVH = 64, 128, 2
    tokens, tipos, fusoes, ids = P.entrenar_bpe(220)
    if caso == "vocab_gigante":
        # un token de 17 MB: gguf_abrir lo acepta, gguf_arr_texto (tope 16 MB) se desincroniza
        L = 17 << 20
        g = bytearray(b"x" * L)
        struct.pack_into("<Q", g, 0, (16 << 20) - 64)            # «longitud» del token falso 1
        struct.pack_into("<Q", g, 8 + (16 << 20) - 64, (16 << 20) - 64)   # y del falso 2: sale del búfer
        tokens = tokens[:300] + [bytes(g)] + tokens[300:] + ["z1", "z2", "z3"]
        tipos = tipos[:300] + [1] + tipos[300:] + [1, 1, 1]
    V = len(tokens)
    VE = V
    if caso == "vocab_modelo":   # el embedding tiene más filas que el vocabulario
        VE = V + 4000
    meta = [
        kv("general.architecture", 8, A), kv("general.name", 8, "poc"),
        kv(A + ".block_count", 4, N_CAM), kv(A + ".context_length", 4, 4096),
        kv(A + ".embedding_length", 4, E), kv(A + ".feed_forward_length", 4, F),
        kv(A + ".attention.head_count", 4, H),
        kv(A + ".attention.head_count_kv", 9, (5, [KVH if i in atencao else 0 for i in range(N_CAM)])),
        kv(A + ".rope.freq_base", 6, 1000000.0), kv(A + ".rope.freq_base_swa", 6, 10000.0),
        kv(A + ".attention.key_length", 4, D),
        kv(A + ".attention.sliding_window_pattern", 9, (7, [bool(atencao.get(i)) for i in range(N_CAM)])),
        kv(A + ".attention.sliding_window", 4, JAN), kv(A + ".shortconv.l_cache", 4, 7),
        kv("tokenizer.ggml.model", 8, "gpt2"),
    ]
    tk = struct.pack("<IQ", 8, len(tokens)) + b"".join(
        (struct.pack("<Q", len(t)) + t) if isinstance(t, bytes) else texto(t) for t in tokens)
    meta.append(texto("tokenizer.ggml.tokens") + struct.pack("<I", 9) + tk)
    meta += [kv("tokenizer.ggml.token_type", 9, (5, tipos)), kv("tokenizer.ggml.merges", 9, (8, fusoes)),
             kv("tokenizer.ggml.eos_token_id", 4, ids["<|im_end|>"])]
    tens = []
    tens.append(("output_norm.weight", [E], "F32", vetor(r, E)))
    tens.append(("rope_freqs.weight", [D // 2], "F32", vetor(r, D // 2, 1.5, 0.5)))
    if caso == "vocab_modelo":
        emb = matriz(r, "Q8_0", E, V) + matriz(r, "Q8_0", E, VE - V, 40.0)
        tens.append(("token_embd.weight", [E, VE], "Q8_0", emb))
    else:
        tens.append(("token_embd.weight", [E, VE], "Q6_K", matriz(r, "Q6_K", E, VE, 12.0)))
    for i in range(N_CAM):
        p = "blk.%d." % i
        for n in ("attn_norm", "ffn_norm"):
            tens.append((p + n + ".weight", [E], "F32", vetor(r, E)))
        tens.append((p + "ffn_gate.weight", [E, F], "Q4_K", matriz(r, "Q4_K", E, F)))
        tens.append((p + "ffn_up.weight", [E, F], "Q4_K", matriz(r, "Q4_K", E, F)))
        if caso == "ne_desborda" and i == 0:
            # ne2 = 2^47: E*F*ne2 = 2^64 -> 0 elementos, 0 bytes: pasa la comprobación de límites
            tens.append((p + "ffn_down.weight", [F, E, 1 << 47], "Q6_K", b""))
        else:
            tens.append((p + "ffn_down.weight", [F, E], "Q6_K", matriz(r, "Q6_K", F, E)))
        if i in atencao:
            Q, KVd = H * D, KVH * D
            tens.append((p + "attn_q.weight", [E, Q], "Q4_K", matriz(r, "Q4_K", E, Q)))
            tens.append((p + "attn_k.weight", [E, KVd], "Q4_K", matriz(r, "Q4_K", E, KVd)))
            tens.append((p + "attn_v.weight", [E, KVd], "Q6_K", matriz(r, "Q6_K", E, KVd)))
            tens.append((p + "attn_output.weight", [Q, E], "Q4_K", matriz(r, "Q4_K", Q, E)))
            tens.append((p + "attn_gate.weight", [E, H], "Q4_K", matriz(r, "Q4_K", E, H)))
            tens.append((p + "attn_q_norm.weight", [D], "F32", vetor(r, D)))
            tens.append((p + "attn_k_norm.weight", [D], "F32", vetor(r, D)))
        else:
            tens.append((p + "shortconv.conv.weight", [7, E], "F32", vetor(r, 7 * E, 0.0, 0.4)))
            tens.append((p + "shortconv.in_proj.weight", [E, 3 * E], "Q4_K", matriz(r, "Q4_K", E, 3 * E)))
            tens.append((p + "shortconv.out_proj.weight", [E, E], "Q4_K", matriz(r, "Q4_K", E, E)))
    if caso == "ne_desborda":   # el tensor vacío, al final de los datos
        tens.sort(key=lambda t: len(t[3]) == 0)
    cab = b"GGUF" + struct.pack("<IQQ", 3, len(tens), len(meta)) + b"".join(meta)
    desl, info, dados = 0, b"", b""
    for nome, ne, tipo, d in tens:
        info += texto(nome) + struct.pack("<I", len(ne)) + b"".join(struct.pack("<Q", x) for x in ne)
        info += struct.pack("<IQ", TIPOS[tipo], desl)
        pad = (-len(d)) % 32
        dados += d + b"\0" * pad
        desl += len(d) + pad
    cab += info
    cab += b"\0" * ((-len(cab)) % 32)
    open(saida, "wb").write(cab + dados)



def compilar():
    os.makedirs(SAIDA, exist_ok=True)
    d = os.path.join(RAIZ, "userland", "llm")
    fontes = sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".c"))
    env = dict(os.environ)
    env["CPATH"] = env.get("HOST_CPATH", "")   # D191: las cabeceras del PC solo para el PC
    subprocess.run(["gcc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                    "-fno-omit-frame-pointer", "-D_GNU_SOURCE", "-DLLM_ANFITRIAO", "-DLLM_MAT_X87", "-msse2", "-mssse3", "-mavx2",
                    "-o", BIN] + fontes + ["-lm", "-lpthread"], check=True, env=env)


def rodar(args, tempo=120):
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=0", UBSAN_OPTIONS="print_stacktrace=1")
    r = subprocess.run([BIN] + args, capture_output=True, text=True, errors="replace", timeout=tempo, env=env)
    return r.returncode, r.stdout + r.stderr


def limpo(saida):
    return "AddressSanitizer" not in saida and "runtime error" not in saida and "LeakSanitizer" not in saida


falhas = []


def verificar(cond, msg, saida=""):
    print(("  ok: " if cond else "  FALLO: ") + msg)
    if not cond:
        falhas.append(msg)
        print(saida[-3000:])


def servidor(modelo):
    p = subprocess.Popen([BIN, "-m", modelo, "--contexto", "512", "--servidor", "--puerto", str(PORTA)],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace",
                         env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    fim = time.time() + 60
    while time.time() < fim:
        try:
            socket.create_connection(("127.0.0.1", PORTA), 1).close()
            break
        except OSError:
            time.sleep(0.3)
    def pedir(dados, ler=True):
        s = socket.create_connection(("127.0.0.1", PORTA), 30)
        s.sendall(dados)
        r = b""
        if ler:
            while True:
                b = s.recv(65536)
                if not b:
                    break
                r += b
        s.close()
        return r.decode("utf-8", "replace")
    try:
        r = pedir(b"POST /v1/chat/completions HTTP/1.1\r\nContent-Length: 99999999999999\r\n\r\n{}")
        verificar(" 400 " in r, "servidor: un Content-Length enorme da 400")
        ocioso = socket.create_connection(("127.0.0.1", PORTA), 5)   # no manda nada
        t0 = time.time()
        corpo = b'{"messages":[{"role":"<|im_start|>","content":"hola"}],"max_tokens":4}'
        r = pedir(b"POST /v1/chat/completions HTTP/1.1\r\nContent-Length: %d\r\n\r\n" % len(corpo) + corpo)
        verificar(" 200 " in r and time.time() - t0 < 30, "servidor: una conexión ociosa no lo bloquea (%.0f s)" % (time.time() - t0))
        ocioso.close()
    finally:
        p.terminate()
        out = p.communicate(timeout=30)[0]
    verificar(limpo(out), "servidor: sin informes de ASan/UBSan", out)


def fase2(base):
    """D380 fase 2: la plantilla y la lectura de llamadas con entradas malas, el proyector de imágenes con
    imágenes raras y proyectores mutados, y el servidor con herramientas e imágenes malas."""
    import base64
    import json
    mmp = os.path.join(SAIDA, "mmproj.gguf")
    P.gerar_mmproj(mmp)
    pet = os.path.join(SAIDA, "pet.json")
    resp = os.path.join(SAIDA, "resp.txt")
    tools = [{"type": "function", "function": {"name": "f", "parameters": {"type": "object", "properties": {"a": {"type": "string"}}}}}]
    peticiones = [
        {"messages": [{"role": "assistant", "content": None, "tool_calls": [{"function": {"name": "f", "arguments": "{\"a\":"}}]}], "tools": tools},
        {"messages": [{"role": "assistant", "tool_calls": [{"function": {"name": "f", "arguments": "[1,2]"}}]}], "tools": tools},
        {"messages": [{"role": "assistant", "tool_calls": [{"function": {"arguments": "{}"}}]}]},
        {"messages": [{"role": "tool", "content": [{"type": "text", "text": "x" * 70000}]}], "tools": [{"function": {"name": "\u0000"}}]},
        {"messages": [{"role": "user", "content": [{"type": "image_url", "image_url": {"url": 5}}, {"type": "image"}]}]},
        {"messages": [{"role": "user", "content": "hola"}], "tools": [[], 3, "x", {"function": {}}], "tool_choice": {"function": {"name": "a>b"}}},
        {"messages": [{"role": "assistant", "tool_calls": [{"function": {"name": "f", "arguments": json.dumps({"a": {"b": [1, {"c": "\n"}] * 50}})}}]}], "tools": tools},
    ]
    respuestas = ["<tool_call>", "<tool_call>\n<function=", "<tool_call>\n<function=f>\n<parameter=", "<tool_call>\n<function=f>\n<parameter=a",
                  "<tool_call>\n<function=f>\n<parameter=a>\n", "<tool_call><function=f><parameter=a>{\"x\":[1,2</parameter></function></tool_call>",
                  "<tool_call>\n<function=f>\n" + "<parameter=a>\n1\n</parameter>\n" * 300, "texto <tool_c", "<tool_call>" * 50 + "<function=f>"]
    for i, pt in enumerate(peticiones):
        open(pet, "w").write(json.dumps(pt))
        rc, out = rodar(["--probar-plantilla", pet])
        verificar(limpo(out), "plantilla %d: sin informes de ASan/UBSan" % i, out)
        for j, r in enumerate(respuestas):
            open(resp, "w").write(r)
            rc, out = rodar(["--probar-respuesta", pet, resp])
            verificar(limpo(out) and rc in (0, 1), "llamadas %d/%d: sin informes de ASan/UBSan" % (i, j), out)
    # imágenes raras con el proyector sintético
    def ppm(w, h):
        return b"P6\n%d %d\n255\n" % (w, h) + bytes((x * 7) & 255 for x in range(w * h * 3))
    for w, h in ((1, 1), (1, 300), (300, 1), (17, 33), (500, 400)):
        f = os.path.join(SAIDA, "img.ppm")
        open(f, "wb").write(ppm(w, h))
        rc, out = rodar(["-m", base, "--mmproj", mmp, "--contexto", "512", "--imagen", f, "--volcar-imagen", os.path.join(SAIDA, "v.f32")])
        verificar(limpo(out) and rc == 0 and "VECTORES:" in out, "imagen de %dx%d: sin informes de ASan/UBSan" % (w, h), out)
    for nome, d in (("cortado.ppm", b"P6\n100 100\n255\n" + b"x" * 50), ("nada.ppm", b"P6\n0 0\n255\n"), ("bmp.bmp", b"BM" + b"\xff" * 60)):
        f = os.path.join(SAIDA, nome)
        open(f, "wb").write(d)
        rc, out = rodar(["-m", base, "--mmproj", mmp, "--contexto", "512", "--imagen", f, "-p", "x", "-n", "2"])
        verificar(limpo(out) and rc != 0, "imagen mala %s: se rechaza sin informes" % nome, out)
    # proyectores mutados: cortado y con un tensor de otro tamaño
    datos = open(mmp, "rb").read()
    for nome, d in (("corto", datos[:len(datos) // 2]), ("cabecera", datos[:200])):
        f = os.path.join(SAIDA, "mmproj-%s.gguf" % nome)
        open(f, "wb").write(d)
        rc, out = rodar(["-m", base, "--mmproj", f, "--contexto", "512", "--imagen", os.path.join(SAIDA, "img.ppm"), "-p", "x"])
        verificar(limpo(out) and rc != 0, "mmproj %s: se rechaza sin informes" % nome, out)
    # el servidor con herramientas e imágenes malas
    p = subprocess.Popen([BIN, "-m", base, "--mmproj", mmp, "--contexto", "1024", "--servidor", "--puerto", str(PORTA + 1)],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace",
                         env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    fim = time.time() + 60
    while time.time() < fim:
        try:
            socket.create_connection(("127.0.0.1", PORTA + 1), 1).close()
            break
        except OSError:
            time.sleep(0.3)
    try:
        img = base64.b64encode(ppm(40, 30)).decode()
        cuerpos = [
            {"messages": [{"role": "user", "content": "hola"}], "tools": tools, "tool_choice": "required", "max_tokens": 6, "stream": True},
            {"messages": [{"role": "user", "content": [{"type": "image_url", "image_url": {"url": "data:image/x;base64," + img}}]}], "max_tokens": 3},
            {"messages": [{"role": "user", "content": [{"type": "image_url", "image_url": {"url": "data:image/x;base64,!!!"}}]}]},
            {"messages": [{"role": "user", "content": [{"type": "image_url", "image_url": {"url": "file:///etc/usuarios"}}]}]},
            {"messages": [{"role": "user", "content": [{"type": "image_url", "image_url": {"url": "data:;base64," + img}}] * 6}]},
        ]
        for i, c in enumerate(cuerpos):
            d = json.dumps(c).encode()
            sk = socket.create_connection(("127.0.0.1", PORTA + 1), 60)
            sk.sendall(b"POST /v1/chat/completions HTTP/1.1\r\nContent-Length: %d\r\n\r\n" % len(d) + d)
            r = b""
            while True:
                b = sk.recv(65536)
                if not b:
                    break
                r += b
            sk.close()
            r = r.decode("utf-8", "replace")
            verificar((" 200 " in r) == (i < 2), "servidor fase 2, petición %d: %s" % (i, r.splitlines()[0] if r else "(nada)"), r[-500:])
    finally:
        p.terminate()
        out = p.communicate(timeout=30)[0]
    verificar(limpo(out), "servidor fase 2: sin informes de ASan/UBSan", out)


def main():
    compilar()
    base = os.path.join(SAIDA, "base.gguf")
    P.gerar(base)
    for caso, esperado in (("xq", None), ("ne_desborda", "dimensi"), ("vocab_gigante", "token"),
                           ("vocab_modelo", "filas de embedding")):
        f = os.path.join(SAIDA, caso + ".gguf")
        gerar_mutado(f, caso)
        rc, out = rodar(["-m", f, "--contexto", "256", "-p", "hola", "-n", "4", "--temp", "0"])
        verificar(limpo(out), "%s: sin informes de ASan/UBSan" % caso, out)
        if esperado:
            verificar(rc != 0 and esperado in out, "%s: se rechaza con un error claro" % caso, out)
        else:
            verificar(rc == 0 and "lectura:" in out, "%s: funciona (la entrada cuantizada cabe)" % caso, out)
    rc, out = rodar(["-m", base, "--contexto", "256", "--tokenizar", "q" * 20000])
    verificar(limpo(out) and rc == 0, "un texto de más tokens de los que caben se corta sin salirse", out)
    rc, out = rodar(["-m", base, "--contexto", "99999999999k", "-p", "x"])
    verificar(limpo(out) and rc != 0, "--contexto enorme se rechaza sin desbordar", out)
    rc, out = rodar(["-m", base, "--contexto", "256", "-p", "hola", "-n", "8", "--temp", "0.8", "--semilla", "3"])
    verificar(limpo(out) and rc == 0, "el modelo sintético normal funciona con ASan/UBSan", out)
    servidor(base)
    fase2(base)
    if falhas:
        print("LLM_LECTOR_ASAN: %d fallos" % len(falhas))
        sys.exit(1)
    print("LLM_LECTOR_ASAN OK")


if __name__ == "__main__":
    main()
