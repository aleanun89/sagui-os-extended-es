// SPDX-License-Identifier: GPL-2.0-or-later
/* desmontador.c - opcode -> texto na sintaxe do chip8asm (D071).
 *
 * Usado pelo depurador do emulador (F10) e por "chip8asm -d". A saída é
 * sintaxe válida do montador, e todo opcode volta a dar os mesmos bytes
 * (um opcode sem sentido sai como "dw"): desmontar e montar de novo
 * reproduz a ROM, o que as provas conferem. Não conhece o modo: decodifica
 * tudo o que algum dos três (CHIP-8, SCHIP, XO-CHIP) entende. */
#include "chip8.h"

int c8_desmontar(const uint8_t *mem, uint32_t mascara, uint32_t end, char *b, int max)
{
    uint16_t op = (uint16_t)(mem[end & mascara] << 8 | mem[(end + 1) & mascara]);
    int x = (op >> 8) & 0xF, y = (op >> 4) & 0xF, n = op & 0xF;
    unsigned nn = op & 0xFF, nnn = op & 0xFFF;
    size_t m = (size_t)max;

    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) { formatar_texto(b, m, "cls"); return 2; }
        if (op == 0x00EE) { formatar_texto(b, m, "ret"); return 2; }
        if ((op & 0xFFF0) == 0x00C0 && n) { formatar_texto(b, m, "scd %d", n); return 2; }
        if ((op & 0xFFF0) == 0x00D0 && n) { formatar_texto(b, m, "scu %d", n); return 2; }
        if (op == 0x00FB) { formatar_texto(b, m, "scr"); return 2; }
        if (op == 0x00FC) { formatar_texto(b, m, "scl"); return 2; }
        if (op == 0x00FD) { formatar_texto(b, m, "exit"); return 2; }
        if (op == 0x00FE) { formatar_texto(b, m, "low"); return 2; }
        if (op == 0x00FF) { formatar_texto(b, m, "high"); return 2; }
        formatar_texto(b, m, "sys 0x%03x", nnn);
        return 2;
    case 0x1: formatar_texto(b, m, "jp 0x%03x", nnn); return 2;
    case 0x2: formatar_texto(b, m, "call 0x%03x", nnn); return 2;
    case 0x3: formatar_texto(b, m, "se v%x, 0x%02x", x, nn); return 2;
    case 0x4: formatar_texto(b, m, "sne v%x, 0x%02x", x, nn); return 2;
    case 0x5:
        if (n == 0) { formatar_texto(b, m, "se v%x, v%x", x, y); return 2; }
        if (n == 2) { formatar_texto(b, m, "save v%x - v%x", x, y); return 2; }
        if (n == 3) { formatar_texto(b, m, "load v%x - v%x", x, y); return 2; }
        break;
    case 0x6: formatar_texto(b, m, "ld v%x, 0x%02x", x, nn); return 2;
    case 0x7: formatar_texto(b, m, "add v%x, 0x%02x", x, nn); return 2;
    case 0x8: {
        static const char *const nomes[16] = { "ld", "or", "and", "xor", "add", "sub", "shr", "subn",
                                               0, 0, 0, 0, 0, 0, "shl", 0 };
        if (!nomes[n]) break;
        formatar_texto(b, m, "%s v%x, v%x", nomes[n], x, y);
        return 2;
    }
    case 0x9:
        if (n) break;
        formatar_texto(b, m, "sne v%x, v%x", x, y);
        return 2;
    case 0xA: formatar_texto(b, m, "ld i, 0x%03x", nnn); return 2;
    case 0xB: formatar_texto(b, m, "jp v0, 0x%03x", nnn); return 2;
    case 0xC: formatar_texto(b, m, "rnd v%x, 0x%02x", x, nn); return 2;
    case 0xD: formatar_texto(b, m, "drw v%x, v%x, %d", x, y, n); return 2;
    case 0xE:
        if (nn == 0x9E) { formatar_texto(b, m, "skp v%x", x); return 2; }
        if (nn == 0xA1) { formatar_texto(b, m, "sknp v%x", x); return 2; }
        break;
    case 0xF:
        if (op == 0xF000) {
            unsigned l = (unsigned)(mem[(end + 2) & mascara] << 8 | mem[(end + 3) & mascara]);
            formatar_texto(b, m, "ld i, long 0x%04x", l);
            return 4;
        }
        if (op == 0xF002) { formatar_texto(b, m, "audio"); return 2; }
        switch (nn) {
        case 0x01: formatar_texto(b, m, "plane %d", x); return 2;
        case 0x07: formatar_texto(b, m, "ld v%x, dt", x); return 2;
        case 0x0A: formatar_texto(b, m, "ld v%x, k", x); return 2;
        case 0x15: formatar_texto(b, m, "ld dt, v%x", x); return 2;
        case 0x18: formatar_texto(b, m, "ld st, v%x", x); return 2;
        case 0x1E: formatar_texto(b, m, "add i, v%x", x); return 2;
        case 0x29: formatar_texto(b, m, "ld f, v%x", x); return 2;
        case 0x30: formatar_texto(b, m, "ld hf, v%x", x); return 2;
        case 0x33: formatar_texto(b, m, "ld b, v%x", x); return 2;
        case 0x3A: formatar_texto(b, m, "pitch v%x", x); return 2;
        case 0x55: formatar_texto(b, m, "ld [i], v%x", x); return 2;
        case 0x65: formatar_texto(b, m, "ld v%x, [i]", x); return 2;
        case 0x75: formatar_texto(b, m, "ld r, v%x", x); return 2;
        case 0x85: formatar_texto(b, m, "ld v%x, r", x); return 2;
        }
        break;
    }
    formatar_texto(b, m, "dw 0x%04x", op);
    return 2;
}
