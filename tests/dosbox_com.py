# SPDX-License-Identifier: GPL-2.0-or-later
"""dosbox_com.py - genera a mano el programa .COM de las pruebas de DOSBox (h294, D294).

Sin ensamblador: los bytes del 8086 se escriben aquí con etiquetas. El programa (PRUEBA.COM):
  1. escribe en modo texto («SAGUI-DOS prueba: modo texto OK»), activa el ratón de DOS (INT 33h) y
     espera una tecla;
  2. pasa al modo gráfico 13h (320x200, 256 colores), pinta toda la pantalla con un degradado
     diagonal con la paleta VGA por defecto;
  3. enciende el altavoz del PC a 1 kHz (PIT canal 2) y reinicia el DSP de la Sound Blaster
     (puerto 0x226) para ver si contesta 0xAA en el 0x22A;
  4. espera otra tecla, apaga el altavoz, vuelve al modo texto, escribe «SB=1» si la Sound
     Blaster contestó (o «SB=0») en pantalla y en PRUEBA.TXT, y espera una tecla más;
  5. sale a DOS (INT 21h, AH=4Ch).
"""
import struct

TEXTO1 = b"SAGUI-DOS prueba: modo texto OK\r\nPulse una tecla para pasar al modo 13h$"
TEXTO2 = b"Vuelta al modo texto. Sound Blaster DSP: SB="


def generar():
    code = bytearray()
    etiquetas = {}
    abs16 = []   # (posición, etiqueta): dirección absoluta (ORG 0x100)
    rel8 = []    # (posición, etiqueta): desplazamiento de un salto corto

    def emite(*b):
        code.extend(b)

    def etiqueta(n):
        etiquetas[n] = len(code)

    def dir16(n):
        abs16.append((len(code), n))
        code.extend(b"\0\0")

    def salto(op, n):
        code.append(op)
        rel8.append((len(code), n))
        code.append(0)

    # 1. texto y espera
    emite(0xB4, 0x09, 0xBA); dir16("msg1"); emite(0xCD, 0x21)
    emite(0x31, 0xC0, 0xCD, 0x33)                       # INT 33h AX=0: el ratón de DOS (DOSBox lo captura al clic)
    emite(0x31, 0xC0, 0xCD, 0x16)                       # INT 16h AH=0: esperar tecla
    # 2. modo 13h y degradado: AL = CL en cada píxel
    emite(0xB8, 0x13, 0x00, 0xCD, 0x10)
    emite(0xB8, 0x00, 0xA0, 0x8E, 0xC0, 0x31, 0xFF, 0xB9, 0x00, 0xFA)
    etiqueta("pix"); emite(0x88, 0xC8, 0xAA); salto(0xE2, "pix")   # mov al,cl; stosb; loop
    # 3a. altavoz a ~1 kHz: PIT canal 2, modo 3, divisor 1193; puerto 0x61 bits 0 y 1
    emite(0xB0, 0xB6, 0xE6, 0x43, 0xB8, 0xA9, 0x04, 0xE6, 0x42, 0x88, 0xE0, 0xE6, 0x42)
    emite(0xE4, 0x61, 0x0C, 0x03, 0xE6, 0x61)
    # 3b. Sound Blaster: DSP reset (1 y luego 0 en 0x226) y esperar 0xAA en 0x22A
    emite(0xBA, 0x26, 0x02, 0xB0, 0x01, 0xEE)
    emite(0xB9, 0x00, 0x10); etiqueta("espera"); salto(0xE2, "espera")
    emite(0x30, 0xC0, 0xEE)
    emite(0xBB, 0x00, 0x40)                             # mov bx,0x4000 (intentos)
    etiqueta("otra")
    emite(0xBA, 0x2E, 0x02, 0xEC, 0x84, 0xC0); salto(0x79, "sig")        # jns: sin dato
    emite(0xBA, 0x2A, 0x02, 0xEC, 0x3C, 0xAA); salto(0x75, "sig")        # jne
    emite(0xC6, 0x06); dir16("sbres"); emite(0x31)      # mov byte [sbres],'1'
    salto(0xEB, "esperar")
    etiqueta("sig"); emite(0x4B); salto(0x75, "otra")   # dec bx; jnz otra
    # 4. esperar, apagar el altavoz, volver al texto
    etiqueta("esperar")
    emite(0x31, 0xC0, 0xCD, 0x16)
    emite(0xE4, 0x61, 0x24, 0xFC, 0xE6, 0x61)
    emite(0xB8, 0x03, 0x00, 0xCD, 0x10)
    emite(0xB4, 0x09, 0xBA); dir16("msg2"); emite(0xCD, 0x21)
    emite(0x31, 0xC0, 0xCD, 0x16)
    # 4b. escribir PRUEBA.TXT con «SB=1» o «SB=0» (la prueba lo lee desde fuera de DOSBox)
    emite(0xB4, 0x3C, 0x31, 0xC9, 0xBA); dir16("nombre"); emite(0xCD, 0x21)
    emite(0x89, 0xC3, 0xB4, 0x40, 0xB9, 0x04, 0x00, 0xBA); dir16("resultado"); emite(0xCD, 0x21)
    emite(0xB4, 0x3E, 0xCD, 0x21)
    # 5. salir
    emite(0xB8, 0x00, 0x4C, 0xCD, 0x21)
    etiqueta("nombre"); code.extend(b"PRUEBA.TXT\0")
    etiqueta("msg1"); code.extend(TEXTO1)
    etiqueta("msg2"); code.extend(TEXTO2[:-3])
    etiqueta("resultado"); code.extend(b"SB=")
    etiqueta("sbres"); code.extend(b"0\r\nFIN$")
    for pos, n in abs16:
        struct.pack_into("<H", code, pos, 0x100 + etiquetas[n])
    for pos, n in rel8:
        d = etiquetas[n] - (pos + 1)
        assert -128 <= d <= 127, (n, d)
        code[pos] = d & 0xFF
    assert len(code) < 0xFF00
    return bytes(code)


def generar_bench(ticks=55):
    """BENCH.COM: cuenta cuántas vueltas de un bucle de 7 instrucciones de 32 bits (suma, giro, xor,
    mov, imul, dec, salto) caben en «ticks» tics del BIOS (18,2 por segundo; 55 = 3 s) y escribe
    «BENCH=XXXX» (vueltas de 16384 iteraciones, en hexadecimal) en BENCH.TXT. Sirve para comparar
    el núcleo normal con el dinámico a igualdad de ciclos (cycles=max)."""
    code = bytearray()
    etiquetas = {}
    rel8 = []
    abs16 = []

    def emite(*b):
        code.extend(b)

    def etiqueta(n):
        etiquetas[n] = len(code)

    def salto(op, n):
        code.append(op)
        rel8.append((len(code), n))
        code.append(0)

    def dir16(n):
        abs16.append((len(code), n))
        code.extend(b"\0\0")

    emite(0xB4, 0x00, 0xCD, 0x1A)       # INT 1Ah AH=0: tics en CX:DX
    emite(0x83, 0xC2, ticks)            # add dx,ticks
    emite(0x89, 0xD6)                   # mov si,dx
    emite(0x31, 0xED)                   # xor bp,bp
    etiqueta("fuera")
    emite(0x66, 0xB9, 0x00, 0x40, 0x00, 0x00)        # mov ecx,0x4000
    etiqueta("dentro")
    emite(0x66, 0x01, 0xCB)                          # add ebx,ecx
    emite(0x66, 0xC1, 0xC3, 0x03)                    # rol ebx,3
    emite(0x66, 0x81, 0xF3, 0x78, 0x56, 0x34, 0x12)  # xor ebx,0x12345678
    emite(0x66, 0x89, 0xD8)                          # mov eax,ebx
    emite(0x66, 0x0F, 0xAF, 0xC1)                    # imul eax,ecx
    emite(0x66, 0x49)                                # dec ecx
    salto(0x75, "dentro")
    emite(0x45)                                      # inc bp
    emite(0xB4, 0x00, 0xCD, 0x1A)
    emite(0x39, 0xF2)                                # cmp dx,si
    salto(0x72, "fuera")                             # jb fuera
    emite(0xBF); dir16("cifras")                     # mov di,cifras
    emite(0xB9, 0x04, 0x00)                          # mov cx,4 dígitos
    etiqueta("dig")
    emite(0xC1, 0xC5, 0x04)                          # rol bp,4
    emite(0x89, 0xE8, 0x24, 0x0F, 0x04, 0x30, 0x3C, 0x39)   # mov ax,bp; and al,0Fh; add al,'0'; cmp al,'9'
    salto(0x76, "dec")                               # jbe dec
    emite(0x04, 0x07)                                # add al,7
    etiqueta("dec")
    emite(0xAA)                                      # stosb
    salto(0xE2, "dig")
    # crear BENCH.TXT, escribir 12 bytes y cerrar
    emite(0xB4, 0x3C, 0x31, 0xC9, 0xBA); dir16("nombre"); emite(0xCD, 0x21)
    emite(0x89, 0xC3)                                # mov bx,ax
    emite(0xB4, 0x40, 0xB9, 0x0C, 0x00, 0xBA); dir16("texto"); emite(0xCD, 0x21)
    emite(0xB4, 0x3E, 0xCD, 0x21)
    emite(0xB8, 0x00, 0x4C, 0xCD, 0x21)
    etiqueta("nombre"); code.extend(b"BENCH.TXT\0")
    etiqueta("texto"); code.extend(b"BENCH=")
    etiqueta("cifras"); code.extend(b"0000\r\n")
    for pos, n in rel8:
        d = etiquetas[n] - (pos + 1)
        assert -128 <= d <= 127, (n, d)
        code[pos] = d & 0xFF
    for pos, n in abs16:
        struct.pack_into("<H", code, pos, 0x100 + etiquetas[n])
    return bytes(code)


if __name__ == "__main__":
    import sys
    open(sys.argv[1] if len(sys.argv) > 1 else "PRUEBA.COM", "wb").write(generar())
    if len(sys.argv) > 2:
        open(sys.argv[2], "wb").write(generar_bench())
