#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera un GGUF sintético diminuto con la arquitectura LFM3 (D380) para las pruebas
del motor `llm`: el modelo real (LFM Open License, no libre) no puede ir a git ni a las
imágenes. Mismo formato y mismas piezas, en pequeño:

  - 8 bloques: convolución corta (l_cache 7) y atención; una capa completa (RoPE 1e6
    con rope_freqs) y otra con ventana deslizante de 8 posiciones (RoPE 1e4), para que
    el anillo de la caché de ventana dé la vuelta en una prueba corta;
  - embedding 256, FFN 512, 4 cabezas de 64 (2 de K/V), normas posteriores;
  - pesos aleatorios con semilla en Q4_K, Q6_K (ffn_down, attn_v, token_embd) y,
    para probar esas rutas, un attn_output en Q8_0 y un shortconv.out_proj en Q4_0;
  - tokenizador BPE «gpt2» (pretokenizador lfm2) entrenado aquí sobre unas frases en
    español, con los tokens especiales de la plantilla de chat de LFM3.

Con --mmproj genera en su lugar un proyector de imágenes sintético de LFM3-VL (arquitectura
«clip», proyector «lfm3vl», D380 fase 2) que encaja con ese modelo (salida de 256): 4 bloques
(convolución 3x3 y atención alternas), embedding 64, 2 cabezas de 32 (1 de K/V), parches de 8,
pesos en Q8_0 y los dos núcleos de parche en F16.

Sin numpy (solo la biblioteca estándar).
Uso: python3 tools/llm_modelo_prueba.py SALIDA.gguf [--grande | --mmproj]"""
import random
import re
import struct
import sys

SEMILLA = 380
ALIN = 32

# ---------------------------------------------------------------- f16


def f16(x):
    return struct.unpack("<H", struct.pack("<e", x))[0]


# ---------------------------------------------------------------- bloques cuantizados (aleatorios y válidos)


def bloco_q4_K(r, d):
    return struct.pack("<HH", f16(d), f16(d * 6)) + bytes(r.randrange(256) for _ in range(12)) + \
        bytes(r.randrange(256) for _ in range(128))


def bloco_q6_K(r, d):
    return bytes(r.randrange(256) for _ in range(128)) + bytes(r.randrange(256) for _ in range(64)) + \
        struct.pack("<16b", *[r.randrange(-64, 64) for _ in range(16)]) + struct.pack("<H", f16(d))


def bloco_q8_0(r, d):
    return struct.pack("<H", f16(d)) + struct.pack("<32b", *[r.randrange(-127, 128) for _ in range(32)])


def bloco_q4_0(r, d):
    return struct.pack("<H", f16(d)) + bytes(r.randrange(256) for _ in range(16))


TIPOS = {"F32": 0, "F16": 1, "Q4_0": 2, "Q8_0": 8, "Q4_K": 12, "Q6_K": 14}
BLOCO = {"Q4_K": (256, bloco_q4_K, 1.2e-4), "Q6_K": (256, bloco_q6_K, 2.5e-5),
         "Q8_0": (32, bloco_q8_0, 4e-4), "Q4_0": (32, bloco_q4_0, 8e-3)}


def matriz(r, tipo, ne0, ne1, escala=1.0):
    n, f, d = BLOCO[tipo]
    d *= escala
    assert ne0 % n == 0
    return b"".join(f(r, d * r.uniform(0.6, 1.4)) for _ in range(ne0 * ne1 // n))


def vetor(r, n, centro=1.0, var=0.1):
    return struct.pack("<%df" % n, *[centro + r.uniform(-var, var) for _ in range(n)])


# ---------------------------------------------------------------- tokenizador


def byte_cp():
    vis = list(range(33, 127)) + list(range(161, 173)) + list(range(174, 256))
    m, n = {}, 0
    for b in range(256):
        if b in vis:
            m[b] = chr(b)
        else:
            m[b] = chr(256 + n)
            n += 1
    return m


CORPUS = """El sagui es un mono pequeño de Brasil. SaguiOS es un sistema operativo libre para equipos antiguos.
La inteligencia artificial local funciona sin conexión. Hola, ¿qué tal? Muy bien, gracias.
El modelo de lenguaje escribe texto en español y en portugués. La capital de Francia es París.
Los ordenadores viejos todavía sirven: tienen memoria, disco y teclado. Una prueba de tokens."""


def entrenar_bpe(n_fusoes):
    bc = byte_cp()
    palavras = re.findall(r" ?\w+| ?[^\w\s]+|\s+", CORPUS)
    seqs = [[bc[b] for b in p.encode("utf-8")] for p in palavras]
    fusoes = []
    for _ in range(n_fusoes):
        pares = {}
        for s in seqs:
            for a, b in zip(s, s[1:]):
                pares[(a, b)] = pares.get((a, b), 0) + 1
        if not pares:
            break
        melhor = max(sorted(pares), key=lambda p: pares[p])
        if pares[melhor] < 2:
            break
        fusoes.append(melhor)
        novas = []
        for s in seqs:
            o, i = [], 0
            while i < len(s):
                if i + 1 < len(s) and (s[i], s[i + 1]) == melhor:
                    o.append(s[i] + s[i + 1])
                    i += 2
                else:
                    o.append(s[i])
                    i += 1
            novas.append(o)
        seqs = novas
    tokens = [bc[b] for b in range(256)]
    for a, b in fusoes:
        if a + b not in tokens:
            tokens.append(a + b)
    tipos = [1] * len(tokens)
    especiais = [("<|pad|>", 3), ("<|startoftext|>", 3), ("<|endoftext|>", 3), ("<|im_start|>", 3), ("<|im_end|>", 3),
                 ("<think>", 4), ("</think>", 4)]
    ids = {}
    for t, tp in especiais:
        ids[t] = len(tokens)
        tokens.append(t)
        tipos.append(tp)
    return tokens, tipos, ["%s %s" % p for p in fusoes], ids


# ---------------------------------------------------------------- escritura GGUF


def texto(s):
    b = s.encode("utf-8")
    return struct.pack("<Q", len(b)) + b


def kv(nome, tipo, valor):
    o = texto(nome) + struct.pack("<I", tipo)
    if tipo == 4:
        o += struct.pack("<I", valor)
    elif tipo == 5:
        o += struct.pack("<i", valor)
    elif tipo == 6:
        o += struct.pack("<f", valor)
    elif tipo == 7:
        o += struct.pack("<B", 1 if valor else 0)
    elif tipo == 8:
        o += texto(valor)
    elif tipo == 9:
        st, vals = valor
        o += struct.pack("<IQ", st, len(vals))
        for v in vals:
            o += texto(v) if st == 8 else struct.pack({4: "<I", 5: "<i", 6: "<f", 7: "<B"}[st], v)
    return o


def escrever_gguf(saida, meta, tens):
    cab = b"GGUF" + struct.pack("<IQQ", 3, len(tens), len(meta)) + b"".join(meta)
    desl, info, dados = 0, b"", b""
    for nome, ne, tipo, d in tens:
        info += texto(nome) + struct.pack("<I", len(ne)) + b"".join(struct.pack("<Q", x) for x in ne)
        info += struct.pack("<IQ", TIPOS[tipo], desl)
        pad = (-len(d)) % ALIN
        dados += d + b"\0" * pad
        desl += len(d) + pad
    cab += info
    cab += b"\0" * ((-len(cab)) % ALIN)
    with open(saida, "wb") as f:
        f.write(cab + dados)
    return len(cab) + len(dados)


def gerar_mmproj(saida):
    """Proyector sintético de LFM3-VL para el modelo de arriba (embedding 256)."""
    r = random.Random(SEMILLA + 1)
    E, F, L, H, KVH, P, SAIDA = 64, 128, 4, 2, 1, 8, 256
    D = E // H
    meta = [
        kv("general.architecture", 8, "clip"), kv("general.type", 8, "mmproj"),
        kv("general.name", 8, "LFM3 VL sintetico de prueba"), kv("general.file_type", 4, 7),
        kv("clip.has_vision_encoder", 7, True), kv("clip.vision.projection_dim", 4, SAIDA),
        kv("clip.vision.image_size", 4, 64), kv("clip.vision.patch_size", 4, P),
        kv("clip.vision.embedding_length", 4, E), kv("clip.vision.feed_forward_length", 4, F),
        kv("clip.vision.block_count", 4, L), kv("clip.vision.attention.head_count", 4, H),
        kv("clip.vision.attention.head_count_kv", 4, KVH),
        kv("clip.vision.image_mean", 9, (6, [0.5, 0.5, 0.5])), kv("clip.vision.image_std", 9, (6, [0.5, 0.5, 0.5])),
        kv("clip.vision.projector_type", 8, "lfm3vl"), kv("clip.vision.spatial_merge_size", 4, 2),
        kv("clip.vision.attention.layer_norm_epsilon", 6, 1e-5), kv("clip.use_silu", 7, True),
        kv("clip.vision.image_min_pixels", 4, 64 * 64), kv("clip.vision.image_max_pixels", 4, 256 * 256),
        kv("clip.vision.image_resize_algo", 8, "bicubic"),
    ]
    tens = []

    def f16v(n, esc):
        return struct.pack("<%dH" % n, *[f16(r.uniform(-esc, esc)) for _ in range(n)])
    tens.append(("v.patch_embd.weight", [P, P, 3, E], "F16", f16v(P * P * 3 * E, 0.08)))
    tens.append(("v.patch_embd.weight.1", [P, P, 3, E], "F16", f16v(P * P * 3 * E, 0.08)))
    tens.append(("v.embedding_norm.weight", [E], "F32", vetor(r, E)))
    for i in range(L):
        p = "v.blk.%d." % i
        tens.append((p + "ln1.weight", [E], "F32", vetor(r, E)))
        tens.append((p + "ln2.weight", [E], "F32", vetor(r, E)))
        tens.append((p + "ffn_gate.weight", [E, F], "Q8_0", matriz(r, "Q8_0", E, F, 4.0)))
        tens.append((p + "ffn_up.weight", [E, F], "Q8_0", matriz(r, "Q8_0", E, F, 4.0)))
        tens.append((p + "ffn_down.weight", [F, E], "Q8_0", matriz(r, "Q8_0", F, E, 4.0)))
        if i % 2 == 0:
            tens.append((p + "conv2d_in_proj.weight", [E, 3 * E], "Q8_0", matriz(r, "Q8_0", E, 3 * E, 4.0)))
            tens.append((p + "conv2d_spatial.weight", [3, 3, 1, E], "F32", vetor(r, 9 * E, 0.0, 0.5)))
            tens.append((p + "conv2d_out_proj.weight", [E, E], "Q8_0", matriz(r, "Q8_0", E, E, 4.0)))
        else:
            tens.append((p + "attn_q.weight", [E, E], "Q8_0", matriz(r, "Q8_0", E, E, 4.0)))
            tens.append((p + "attn_k.weight", [E, KVH * D], "Q8_0", matriz(r, "Q8_0", E, KVH * D, 4.0)))
            tens.append((p + "attn_v.weight", [E, KVH * D], "Q8_0", matriz(r, "Q8_0", E, KVH * D, 4.0)))
            tens.append((p + "attn_out.weight", [E, E], "Q8_0", matriz(r, "Q8_0", E, E, 4.0)))
            tens.append((p + "attn_q_norm.weight", [D], "F32", vetor(r, D)))
            tens.append((p + "attn_k_norm.weight", [D], "F32", vetor(r, D)))
    tens.append(("mm.input_norm.weight", [E], "F32", vetor(r, E)))
    tens.append(("mm.input_norm.bias", [E], "F32", vetor(r, E, 0.0, 0.1)))
    tens.append(("mm.0.weight", [4 * E, 4 * E], "Q8_0", matriz(r, "Q8_0", 4 * E, 4 * E, 4.0)))
    tens.append(("mm.0.bias", [4 * E], "F32", vetor(r, 4 * E, 0.0, 0.2)))
    tens.append(("mm.2.weight", [4 * E, SAIDA], "Q8_0", matriz(r, "Q8_0", 4 * E, SAIDA, 4.0)))
    tens.append(("mm.2.bias", [SAIDA], "F32", vetor(r, SAIDA, 0.0, 0.2)))
    n = escrever_gguf(saida, meta, tens)
    print("%s: proyector sintético, %d tensores, %d bytes" % (saida, len(tens), n))


def gerar(saida, grande=False):
    r = random.Random(SEMILLA)
    A = "lfm3"
    N_CAM, E, F, H, D, KVH, JAN = 8, 256, 512, 4, 64, 2, 8
    atencao = {2: False, 4: True, 6: False}   # capa -> ventana
    if grande:
        # --grande: pesos pequeños pero una caché K/V enorme (24 capas de atención completa con
        # 8 cabezas K/V de 128): con 48k no cabe en el proceso; para probar el error de memoria.
        N_CAM, H, D, KVH = 24, 8, 128, 8
        atencao = {i: i == 1 for i in range(N_CAM)}
    tokens, tipos, fusoes, ids = entrenar_bpe(220)
    meta = [
        kv("general.architecture", 8, A), kv("general.name", 8, "LFM3 sintetico de prueba"),
        kv("general.file_type", 4, 15), kv("general.quantization_version", 4, 2),
        kv(A + ".block_count", 4, N_CAM), kv(A + ".context_length", 4, 262144 if grande else 4096),
        kv(A + ".embedding_length", 4, E), kv(A + ".feed_forward_length", 4, F),
        kv(A + ".attention.head_count", 4, H),
        kv(A + ".attention.head_count_kv", 9, (5, [KVH if i in atencao else 0 for i in range(N_CAM)])),
        kv(A + ".rope.freq_base", 6, 1000000.0), kv(A + ".rope.freq_base_swa", 6, 10000.0),
        kv(A + ".attention.layer_norm_rms_epsilon", 6, 1e-5),
        kv(A + ".attention.key_length", 4, D), kv(A + ".attention.value_length", 4, D),
        kv(A + ".rope.dimension_count", 4, D),
        kv(A + ".attention.sliding_window_pattern", 9, (7, [bool(atencao.get(i)) for i in range(N_CAM)])),
        kv(A + ".attention.sliding_window", 4, JAN), kv(A + ".shortconv.l_cache", 4, 7),
        kv("tokenizer.ggml.model", 8, "gpt2"), kv("tokenizer.ggml.pre", 8, "lfm2"),
        kv("tokenizer.ggml.tokens", 9, (8, tokens)), kv("tokenizer.ggml.token_type", 9, (5, tipos)),
        kv("tokenizer.ggml.merges", 9, (8, fusoes)),
        kv("tokenizer.ggml.bos_token_id", 4, ids["<|startoftext|>"]),
        kv("tokenizer.ggml.eos_token_id", 4, ids["<|im_end|>"]),
        kv("tokenizer.ggml.padding_token_id", 4, ids["<|pad|>"]),
        kv("tokenizer.ggml.add_bos_token", 7, False),
    ]
    V = len(tokens)
    tens = []   # (nombre, [ne], tipo, datos)
    tens.append(("output_norm.weight", [E], "F32", vetor(r, E)))
    tens.append(("rope_freqs.weight", [D // 2], "F32", vetor(r, D // 2, 1.5, 0.5)))
    # el embedding (que también es la salida) más grande: logits bien separados, sin empates
    tens.append(("token_embd.weight", [E, V], "Q6_K", matriz(r, "Q6_K", E, V, 12.0)))
    for i in range(N_CAM):
        p = "blk.%d." % i
        tens.append((p + "attn_norm.weight", [E], "F32", vetor(r, E)))
        tens.append((p + "ffn_norm.weight", [E], "F32", vetor(r, E)))
        tens.append((p + "post_attention_norm.weight", [E], "F32", vetor(r, E, 0.5, 0.1)))
        tens.append((p + "post_ffw_norm.weight", [E], "F32", vetor(r, E, 0.5, 0.1)))
        tens.append((p + "ffn_gate.weight", [E, F], "Q4_K", matriz(r, "Q4_K", E, F)))
        tens.append((p + "ffn_up.weight", [E, F], "Q4_K", matriz(r, "Q4_K", E, F)))
        tens.append((p + "ffn_down.weight", [F, E], "Q6_K", matriz(r, "Q6_K", F, E)))
        if i in atencao:
            Q, KV = H * D, KVH * D
            tens.append((p + "attn_q.weight", [E, Q], "Q4_K", matriz(r, "Q4_K", E, Q)))
            tens.append((p + "attn_k.weight", [E, KV], "Q4_K", matriz(r, "Q4_K", E, KV)))
            tens.append((p + "attn_v.weight", [E, KV], "Q6_K", matriz(r, "Q6_K", E, KV)))
            to = "Q8_0" if i == 6 else "Q4_K"
            tens.append((p + "attn_output.weight", [Q, E], to, matriz(r, to, Q, E)))
            tens.append((p + "attn_gate.weight", [E, H], "Q4_K", matriz(r, "Q4_K", E, H)))
            tens.append((p + "attn_q_norm.weight", [D], "F32", vetor(r, D)))
            tens.append((p + "attn_k_norm.weight", [D], "F32", vetor(r, D)))
        else:
            tens.append((p + "shortconv.conv.weight", [7, E], "F32", vetor(r, 7 * E, 0.0, 0.4)))
            tens.append((p + "shortconv.in_proj.weight", [E, 3 * E], "Q4_K", matriz(r, "Q4_K", E, 3 * E)))
            to = "Q4_0" if i == 3 else "Q4_K"
            tens.append((p + "shortconv.out_proj.weight", [E, E], to, matriz(r, to, E, E)))
    n = escrever_gguf(saida, meta, tens)
    print("%s: %d tensores, vocabulario %d, %d fusiones, %d bytes" % (saida, len(tens), V, len(fusoes), n))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a not in ("--grande", "--mmproj")]
    if "--mmproj" in sys.argv[1:]:
        gerar_mmproj(args[0] if args else "mmproj-sintetico.gguf")
    else:
        gerar(args[0] if args else "sintetico.gguf", "--grande" in sys.argv[1:])
