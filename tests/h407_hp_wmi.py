#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""h407 - el puente WMI de ACPI (PNP0C14) y el WMI de los HP: radios y teclas (D405).

QEMU no tiene WMI de HP: la prueba carga con -acpitable una SSDT hecha aquí (AML
generado a mano, como h158; no hace falta iasl) con un \\_SB.WMID que imita el
interfaz de las BIOS de HP de consumo:
  _WDG con el GUID del método (5FB7F034-..., «AD») y el del evento (95F24279-...,
  Notify 0x80); WMAD que contesta «SECU»/orden/tipo con la firma y el código, lee las
  radios (tipo 5) y la última tecla del panel (tipo 0x0C) y cambia las radios
  (escribir tipo 5, máscara << 8 | encendidas), guardando el estado; _WED con
  (evento, dato); WE80 para encender el evento.
Los eventos los dispara el _BST de una batería falsa (la tarea los lee cada 5 s):
unas lecturas después de encender la Wi-Fi, el botón inalámbrico (bloqueo por
hardware, evento 5) y luego la tecla DVD/música del panel (evento 4, código 0x2142).

Criterio:
  (0) anfitrión: kernel/lib/wmi_wdg.c (análisis de _WDG con el de un HP G62 real y casos
      rotos, GUID en texto y vuelta, búferes de HP, radios, eventos), también con
      -fsanitize=address,undefined;
  (a) imagen con --uacpi y la SSDT: el kernel encuentra el PNP0C14 y sus dos bloques;
      `radios` dice Wi-Fi apagada por software y Bluetooth encendida; `radios wmi` lista
      los GUID; `radios wifi si` la enciende (la BIOS falsa guarda el estado) y avisa;
      como la tarjeta no aparece en el PCI, avisa de reiniciar y `radios` lo dice; el
      botón inalámbrico (evento 5) avisa de que la bloquea y `radios` lo dice; la tecla
      del panel (evento 4) avisa con su nombre; `radios bluetooth no` la apaga; las teclas
      de volumen (QEMU volumeup/volumedown/audiomute, E0 30/2E/20) cambian el volumen del
      sistema (`volumen`); `tablasacpi` vuelca las tablas y la SSDT sale entera;
  (b) imagen sin --uacpi: `radios` dice que no está disponible y las teclas de volumen
      siguen funcionando.

SAGUI_SO_ANFITRIAO=1: solo la parte (0).
"""
import os
import re
import struct
import subprocess
import sys
import time
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, BUILD  # noqa: E402
from h158_bateria import (pkglen, nome, inteiro, cadeia, pacote, name, scope, device, method,  # noqa: E402
                          se, ret, igual, eisaid, tabela, esperar)

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(BUILD, "testes", "h407")

GUID_BIOS = "5FB7F034-2C63-45E9-BE91-3D44E2C707E4"
GUID_EVENTO = "95F24279-4D7B-4334-9387-ACCDC67EF61C"


# ------------------------------------------------------------ AML (lo que falta en h158)

def local(i):
    return bytes([0x60 + i])


def arg(i):
    return bytes([0x68 + i])


def buffer(tam, dados=b""):
    corpo = inteiro(tam) + dados
    return b"\x11" + pkglen(len(corpo)) + corpo


def cdword(origem, desl, n):          # CreateDWordField(origem, desl, n)
    return b"\x8a" + origem + inteiro(desl) + nome(n)


def store(valor, destino):             # destino = valor
    return b"\x70" + valor + destino


def e_(a, b):                          # And(a, b) sin destino
    return b"\x7b" + a + b + b"\x00"


def ou(a, b):
    return b"\x7d" + a + b + b"\x00"


def shl(a, n):
    return b"\x79" + a + inteiro(n) + b"\x00"


def shr(a, n):
    return b"\x7a" + a + inteiro(n) + b"\x00"


def senao(*termos):
    corpo = b"".join(termos)
    return b"\xa1" + pkglen(len(corpo)) + corpo


def notify(n, v):
    return b"\x86" + nome(n) + inteiro(v)


def incrementar(n):
    return b"\x75" + nome(n)


def wdg():
    b = uuid.UUID(GUID_BIOS).bytes_le + b"AD" + bytes([1, 0x02])
    b += uuid.UUID(GUID_EVENTO).bytes_le + bytes([0x80, 0x00, 1, 0x08])
    return b


def radio_escrever(rw, bit):
    """Si la máscara (Local2) tiene el bit: encender (Local3) o apagar la radio rw."""
    return se(e_(local(2), inteiro(bit)),
              se(e_(local(3), inteiro(bit)), store(ou(nome(rw), inteiro(2)), nome(rw))),
              senao(store(e_(nome(rw), inteiro(0xFD)), nome(rw))))


def ssdt_hp(sem_radios=False):
    """sem_radios: la BIOS no ve ninguna radio (como el dv7 con la tarjeta fuera del bus al
    arrancar): leer pone WMIF = 1 y el driver tiene que devolverla a 0; entonces el _BST
    manda el botón inalámbrico (evento 5)."""
    estado = [name(n, inteiro(v)) for n, v in
              (("RPRS", 0 if sem_radios else 3), ("RW0", 0x08), ("RW1", 0x0A), ("RW2", 0), ("STEP", 0), ("BBV", 0),
               ("WEID", 0), ("WMED", 0), ("EVON", 0), ("NQRY", 0))]
    wmad = method("WMAD", 3,
                  store(buffer(12), local(1)),
                  cdword(local(1), 0, "SNOU"), cdword(local(1), 4, "RTCD"), cdword(local(1), 8, "DOUT"),
                  cdword(arg(2), 0, "SNIN"), cdword(arg(2), 4, "COMD"), cdword(arg(2), 8, "CMTP"),
                  cdword(arg(2), 16, "DDWD"),
                  store(inteiro(0x4C494146), nome("SNOU")),
                  store(inteiro(2), nome("RTCD")),
                  incrementar("NQRY"),
                  se(igual(nome("SNIN"), inteiro(0x55434553)),
                     store(inteiro(3), nome("RTCD")),
                     se(igual(nome("COMD"), inteiro(1)),
                        store(inteiro(4), nome("RTCD")),
                        se(igual(nome("CMTP"), inteiro(5)),
                           store(inteiro(1), nome("WMIF")),
                           store(ou(ou(nome("RPRS"), shl(nome("RW0"), 8)), ou(shl(nome("RW1"), 16), shl(nome("RW2"), 24))),
                                 nome("DOUT")),
                           store(inteiro(0), nome("RTCD"))),
                        se(igual(nome("CMTP"), inteiro(0x0C)),
                           store(nome("BBV"), nome("DOUT")),
                           store(inteiro(0), nome("BBV")),
                           store(inteiro(0), nome("RTCD")))),
                     se(igual(nome("COMD"), inteiro(2)),
                        store(inteiro(4), nome("RTCD")),
                        se(igual(nome("CMTP"), inteiro(5)),
                           store(e_(shr(nome("DDWD"), 8), inteiro(0xFF)), local(2)),
                           store(e_(nome("DDWD"), inteiro(0xFF)), local(3)),
                           radio_escrever("RW0", 1),
                           radio_escrever("RW1", 2),
                           store(inteiro(0), nome("RTCD")),
                           se(igual(nome("STEP"), inteiro(0)), store(inteiro(1), nome("STEP"))),
                           store(inteiro(5), nome("WEID")),
                           store(inteiro(0), nome("WMED")),
                           notify("\\_SB.WMID", 0x80)))),
                  ret(local(1)))
    wed = method("_WED", 1,
                 store(buffer(8), local(0)),
                 cdword(local(0), 0, "EVID"), cdword(local(0), 4, "EVDA"),
                 store(nome("WEID"), nome("EVID")),
                 store(nome("WMED"), nome("EVDA")),
                 ret(local(0)))
    we80 = method("WE80", 1, store(arg(0), nome("EVON")))
    wmid = device("WMID",
                  name("_HID", cadeia("PNP0C14")),
                  name("WMIF", inteiro(0)),
                  name("_UID", inteiro(0)),
                  name("_WDG", buffer(len(wdg()), wdg())),
                  wmad, wed, we80)
    # La batería falsa: su _BST (cada 5 s) hace de reloj de los eventos. STEP pasa a 1 al
    # cambiar una radio; luego sube en cada lectura: en 6, el botón inalámbrico; en 8, la tecla.
    if sem_radios:
        bst = method("_BST", 0,
                     se(igual(nome("STEP"), inteiro(0)),
                        se(igual(nome("\\_SB.WMID.WMIF"), inteiro(0)),
                           se(igual(igual(nome("NQRY"), inteiro(0)), inteiro(0)),
                              store(inteiro(1), nome("STEP")),
                              store(inteiro(5), nome("WEID")),
                              store(inteiro(0), nome("WMED")),
                              notify("\\_SB.WMID", 0x80)))),
                     ret(pacote(inteiro(1), inteiro(1000), inteiro(3000), inteiro(11000))))
    else:
        bst = None
    bst = bst or method("_BST", 0,
                 se(ou(igual(nome("STEP"), inteiro(0)), igual(nome("STEP"), inteiro(9))),
                    ret(pacote(inteiro(1), inteiro(1000), inteiro(3000), inteiro(11000)))),
                 incrementar("STEP"),
                 se(igual(nome("STEP"), inteiro(6)),
                    store(e_(nome("RW0"), inteiro(0xF7)), nome("RW0")),
                    store(inteiro(5), nome("WEID")),
                    store(inteiro(0), nome("WMED")),
                    notify("\\_SB.WMID", 0x80)),
                 se(igual(nome("STEP"), inteiro(8)),
                    store(inteiro(0x2142), nome("BBV")),
                    store(inteiro(4), nome("WEID")),
                    store(inteiro(0), nome("WMED")),
                    notify("\\_SB.WMID", 0x80)),
                 ret(pacote(inteiro(1), inteiro(1000), inteiro(3000), inteiro(11000))))
    bat = device("BAT0",
                 name("_HID", inteiro(eisaid("PNP0C0A"))),
                 name("_UID", inteiro(1)),
                 method("_STA", 0, ret(inteiro(0x1F))),
                 method("_BIF", 0, ret(pacote(*[inteiro(v) if not isinstance(v, str) else cadeia(v) for v in
                                                [0, 5000, 5000, 1, 11100, 500, 200, 10, 10, "HP-FALSA", "1", "LION",
                                                 "SaguiOS"]]))),
                 bst)
    corpo = scope("\\_SB", *estado, wmid, bat)
    return tabela("SSDT", "HPWMI", corpo)


# ------------------------------------------------------------ (0) anfitrión

# El _WDG del HP G62 (volcado público de uACPI/ACPIDumps, CC-BY-4.0): 5 bloques.
WDG_G62 = bytes([
    0x34, 0xF0, 0xB7, 0x5F, 0x63, 0x2C, 0xE9, 0x45, 0xBE, 0x91, 0x3D, 0x44, 0xE2, 0xC7, 0x07, 0xE4,
    0x41, 0x44, 0x01, 0x02, 0x79, 0x42, 0xF2, 0x95, 0x7B, 0x4D, 0x34, 0x43, 0x93, 0x87, 0xAC, 0xCD,
    0xC6, 0x7E, 0xF6, 0x1C, 0x80, 0x00, 0x01, 0x08, 0x21, 0x12, 0x90, 0x05, 0x66, 0xD5, 0xD1, 0x11,
    0xB2, 0xF0, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0x10, 0x41, 0x45, 0x01, 0x00, 0xD4, 0x2B, 0x99, 0xD0,
    0x7C, 0xA4, 0xFE, 0x4E, 0xB0, 0x72, 0x32, 0x4A, 0xEC, 0x92, 0x29, 0x6C, 0x42, 0x43, 0x01, 0x00,
    0x18, 0x43, 0x81, 0x2B, 0xE8, 0x4B, 0x07, 0x47, 0x9D, 0x84, 0xA1, 0x90, 0xA8, 0x59, 0xB5, 0xD0,
    0xA0, 0x00, 0x01, 0x08])

PROGRAMA_ANFITRIAO = r'''
#include <stdio.h>
#include <string.h>
#include "wmi_wdg.h"

static int falhas;
#define OK(c, m) do { if (!(c)) { printf("FALLA: %s\n", m); falhas++; } } while (0)

static const unsigned char g62[] = { @G62@ };

int main(void)
{
    wmi_bloco_t b[8];
    int n = wmi_wdg_analisar(g62, sizeof g62, b, 8);
    OK(n == 5, "G62: 5 bloques");
    char t[37];
    wmi_guid_texto(b[0].guid, t);
    OK(strcmp(t, HPWMI_GUID_BIOS) == 0, "G62: el primero es el método de HP");
    OK(strcmp(b[0].id, "AD") == 0 && b[0].flags == WMI_METODO && b[0].instancias == 1, "G62: WMAD, 1 instancia");
    wmi_guid_texto(b[1].guid, t);
    OK(strcmp(t, HPWMI_GUID_EVENTO) == 0, "G62: el segundo es el evento");
    OK(b[1].flags == WMI_EVENTO && b[1].notificacao == 0x80 && b[1].id[0] == 0, "G62: evento con Notify 0x80");
    wmi_guid_texto(b[2].guid, t);
    OK(strcmp(t, "05901221-D566-11D1-B2F0-00A0C9062910") == 0 && strcmp(b[2].id, "AE") == 0, "G62: el MOF (WQAE)");
    OK(b[4].flags == WMI_EVENTO && b[4].notificacao == 0xA0, "G62: segundo evento en 0xA0");
    char m[5];
    wmi_nome_metodo(&b[0], "WM", m); OK(strcmp(m, "WMAD") == 0, "nombre WMAD");
    wmi_nome_metodo(&b[1], "WE", m); OK(strcmp(m, "WE80") == 0, "nombre WE80");
    wmi_nome_metodo(&b[4], "WE", m); OK(strcmp(m, "WEA0") == 0, "nombre WEA0");
    uint8_t g[16];
    OK(wmi_guid_ler("95f24279-4d7b-4334-9387-accdc67ef61c", g), "GUID en minúsculas");
    OK(wmi_achar(b, n, g) == 1, "encontrar el GUID del evento");
    OK(wmi_achar_evento(b, n, 0xA0) == 4 && wmi_achar_evento(b, n, 0x81) < 0, "encontrar por Notify");
    OK(!wmi_guid_ler("95F24279-4D7B-4334-9387-ACCDC67EF61", g), "GUID corto");
    OK(!wmi_guid_ler("95F24279-4D7B-4334-9387-ACCDC67EF61CX", g), "GUID largo");
    OK(!wmi_guid_ler("95F24279+4D7B-4334-9387-ACCDC67EF61C", g), "GUID sin guion");
    OK(!wmi_guid_ler("95F2427G-4D7B-4334-9387-ACCDC67EF61C", g), "GUID con letra mala");
    OK(!wmi_guid_ler(NULL, g), "GUID nulo");
    /* Rotos. */
    OK(wmi_wdg_analisar(g62, 19, b, 8) == -1, "19 bytes");
    OK(wmi_wdg_analisar(g62, 0, b, 8) == -1, "vacío");
    OK(wmi_wdg_analisar(NULL, 20, b, 8) == -1, "nulo");
    unsigned char malo[20];
    memcpy(malo, g62, 20); malo[16] = 0x01;
    OK(wmi_wdg_analisar(malo, 20, b, 8) == -1, "ObjectId no imprimible en un método");
    memcpy(malo, g62 + 20, 20); malo[16] = 0x01;
    OK(wmi_wdg_analisar(malo, 20, b, 8) == 1, "un evento puede tener cualquier NotifyId");
    OK(wmi_wdg_analisar(g62, sizeof g62, b, 2) == 5, "max pequeño: cuenta todos, copia 2");
    OK(wmi_wdg_analisar(g62, sizeof g62, NULL, 0) == 5, "sin salida");
    /* Búferes de HP. */
    unsigned char e[16 + 128];
    uint32_t d = 0x0101;
    OK(hpwmi_montar(e, sizeof e, HPWMI_ESCREVER, HPWMI_T_RADIOS, &d, 4) == 20, "montar: 20 bytes");
    OK(memcmp(e, "SECU", 4) == 0 && e[4] == 2 && e[8] == 5 && e[12] == 4 && e[16] == 1 && e[17] == 1, "montar: campos");
    OK(hpwmi_montar(e, 15, 1, 5, NULL, 0) == 0, "montar: no cabe la cabecera");
    OK(hpwmi_montar(e, 20, 1, 5, NULL, 5) == 0, "montar: no caben los datos");
    unsigned char r[12] = { 0x46, 0x41, 0x49, 0x4C, 0, 0, 0, 0, 0x07, 0x0A, 0x08, 0x00 };
    uint32_t cod; const uint8_t *dd; uint32_t nd;
    OK(hpwmi_resposta(r, 12, &cod, &dd, &nd) && cod == 0 && nd == 4 && dd[0] == 7, "respuesta");
    OK(!hpwmi_resposta(r, 7, &cod, &dd, &nd), "respuesta corta");
    hpwmi_radio_t ra[3];
    hpwmi_radios_ler(dd, ra);
    OK(ra[0].presente && ra[0].sw && ra[0].hw, "Wi-Fi encendida");
    OK(ra[1].presente && !ra[1].sw && ra[1].hw, "Bluetooth apagada por software");
    OK(ra[2].presente && !ra[2].sw && !ra[2].hw, "WWAN apagada");
    unsigned char sinwwan[4] = { 0x03, 0x02, 0x08, 0x0A };
    hpwmi_radios_ler(sinwwan, ra);
    OK(!ra[2].presente && !ra[2].sw && !ra[2].hw, "sin WWAN: nada aunque el byte diga encendida");
    OK(ra[0].sw && !ra[0].hw, "Wi-Fi bloqueada por el botón");
    OK(hpwmi_radios_pedido(0, 1) == 0x101 && hpwmi_radios_pedido(1, 0) == 0x200 && hpwmi_radios_pedido(2, 1) == 0x404,
       "pedidos");
    OK(hpwmi_radios_pedido(3, 1) == 0 && hpwmi_radios_pedido(-1, 1) == 0, "pedido fuera de rango");
    OK(hpwmi_id_metodo(0) == 1 && hpwmi_id_metodo(4) == 2 && hpwmi_id_metodo(5) == 3 && hpwmi_id_metodo(128) == 3 &&
       hpwmi_id_metodo(129) == 4 && hpwmi_id_metodo(5000) == 5, "id del método por tamaño");
    unsigned char ev[16] = { 5, 0, 0, 0, 0x42, 0x21, 0, 0 };
    uint32_t a, bb;
    OK(hpwmi_evento_ler(ev, 8, &a, &bb) && a == 5 && bb == 0x2142, "evento de 8 bytes");
    OK(hpwmi_evento_ler(ev, 16, &a, &bb) && a == 5, "evento de 16 bytes");
    OK(!hpwmi_evento_ler(ev, 4, &a, &bb) && !hpwmi_evento_ler(ev, 12, &a, &bb), "eventos de tamaño raro");
    const hpwmi_tecla_t *k = hpwmi_tecla(0x2142);
    OK(k && k->acao == HPWMI_ACAO_AVISO, "tecla DVD/música");
    OK(hpwmi_tecla(0x1234) == NULL, "tecla desconocida");
    printf("falhas %d\n", falhas);
    return falhas != 0;
}
'''


def anfitriao():
    os.makedirs(DIR, exist_ok=True)
    fonte = os.path.join(DIR, "h407_anfitriao.c")
    with open(fonte, "w", encoding="utf-8") as f:
        f.write(PROGRAMA_ANFITRIAO.replace("@G62@", ", ".join(f"0x{x:02X}" for x in WDG_G62)))
    inc = os.path.join(RAIZ, "kernel", "inc")
    lib = os.path.join(RAIZ, "kernel", "lib", "wmi_wdg.c")
    for nome_exe, extra in (("h407_anfitriao", ["-O2"]),
                            ("h407_anfitriao_asan", ["-O1", "-g", "-fsanitize=address,undefined",
                                                     "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome_exe)
        r = subprocess.run(["gcc", *extra, "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", inc, "-o", exe, fonte, lib],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, f"anfitrión: compila wmi_wdg.c ({nome_exe})\n{r.stderr}")
        r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
        verificar(r.returncode == 0 and "falhas 0" in r.stdout, f"anfitrión: casos de wmi_wdg ({nome_exe})\n{r.stdout}")
    print("  anfitrión: _WDG (G62 real y rotos), GUID, búferes de HP, radios y eventos correctos")


# ------------------------------------------------------------ QEMU

def volumen(m):
    out = m.comando("volumen")
    g = re.search(r"Volumen: (\d+) %( \(silencio\))?", out)
    verificar(g is not None, f"`volumen` da el volumen\n{out}")
    return int(g.group(1)), g.group(2) is not None


def teclas_volume(m, caso):
    v0, _ = volumen(m)
    m.tecla("volumedown")
    time.sleep(1.0)
    v1, _ = volumen(m)
    verificar(v1 == max(0, v0 - 5), f"{caso}: volumedown (E0 2E) baja 5 ({v0} -> {v1})")
    m.tecla("volumeup")
    m.tecla("volumeup")
    time.sleep(1.0)
    v2, _ = volumen(m)
    verificar(v2 == min(100, v1 + 10), f"{caso}: volumeup (E0 30) dos veces sube 10 ({v1} -> {v2})")
    m.tecla("audiomute")
    time.sleep(1.0)
    _, mudo = volumen(m)
    verificar(mudo, f"{caso}: audiomute (E0 20) silencia")
    m.tecla("audiomute")
    time.sleep(1.0)
    _, mudo = volumen(m)
    verificar(not mudo, f"{caso}: audiomute otra vez quita el silencio")


def parte_a():
    os.makedirs(DIR, exist_ok=True)
    ssdt = os.path.join(DIR, "ssdt-hpwmi.aml")
    dados = ssdt_hp()
    with open(ssdt, "wb") as f:
        f.write(dados)
    img = imagem_de_teste("", ["--vesa", "--uacpi"])
    with Maquina("h407a", "ide", imagem=img, som="ac97", extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        esperar(m, r"\[UACPI\] listo en \d+ ms", 90)
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s.upper(), "a: arranca sin excepciones")
        verificar(re.search(r"\[WMI\] \\_SB_?\.WMID_?: 2 bloques en _WDG", s) is not None, "a: encuentra el PNP0C14 y _WDG")
        verificar(f"[WMI]   {GUID_BIOS} metodo AD" in s, "a: el bloque del método (WMAD)")
        verificar(f"[WMI]   {GUID_EVENTO} evento Notify 80" in s, "a: el bloque del evento (Notify 0x80)")
        verificar("[HPWMI] WMI de HP encontrado" in s, "a: el driver de HP lo reconoce")
        verificar(re.search(r"\[WMI\] evento 95F24279[^\r\n]*WE80\(1\): ok", s) is not None, "a: WE80(1) enciende el evento")
        verificar("[HPWMI] radios (al arrancar): estado 3 8 a 0" in s, "a: lee las radios al arrancar")
        out = m.comando("radios")
        verificar(re.search(r"Wi-Fi +apagada \(apagada por software\)", out) is not None, f"a: `radios`: Wi-Fi apagada\n{out}")
        verificar(re.search(r"Bluetooth +encendida", out) is not None, f"a: `radios`: Bluetooth encendida\n{out}")
        verificar("WWAN" not in out, "a: sin WWAN (no está en el primer byte)")
        out = m.comando("radios wmi")
        verificar(GUID_BIOS in out and GUID_EVENTO in out and "2 bloques WMI" in out, f"a: `radios wmi`\n{out}")
        marca = len(m.serial())
        out = m.comando("radios wifi si")
        verificar(re.search(r"Wi-Fi +encendida", out) is not None, f"a: `radios wifi si` la enciende\n{out}")
        esperar(m, r"\[GUI\] notificacao: Wi-Fi encendida\.", 15, marca)
        verificar(True, "a: aviso «Wi-Fi encendida»")
        esperar(m, r"\[GUI\] notificacao: La Wi-Fi est. encendida pero la tarjeta no aparece en el PCI: reinicia", 40, marca)
        verificar(True, "a: la tarjeta no aparece en el PCI: aviso de reiniciar")
        out = m.comando("radios")
        verificar("Reinicia para usar la tarjeta Wi-Fi" in out, f"a: `radios` dice que hay que reiniciar\n{out}")
        m.captura("H407-1-radios")
        esperar(m, r"\[HPWMI\] evento 5 \(radios\)", 60, marca)
        esperar(m, r"\[GUI\] notificacao: Wi-Fi apagada por el bot.n inal.mbrico\.", 15, marca)
        verificar(True, "a: el botón inalámbrico (evento 5) avisa")
        out = m.comando("radios")
        verificar(re.search(r"Wi-Fi +apagada \(bloqueada por el bot.n inal.mbrico\)", out) is not None,
                  f"a: `radios`: bloqueada por el botón\n{out}")
        esperar(m, r"\[HPWMI\] tecla del panel 2142", 30, marca)
        esperar(m, r"\[GUI\] notificacao: HP: bot.n DVD/m.sica\.", 15, marca)
        verificar(True, "a: la tecla del panel (evento 4, 0x2142) avisa")
        out = m.comando("radios bluetooth no")
        verificar(re.search(r"Bluetooth +apagada \(apagada por software\)", out) is not None,
                  f"a: `radios bluetooth no` la apaga\n{out}")
        teclas_volume(m, "a")
        out = m.comando("tablasacpi /tmp/acpi")
        g = re.search(r"SSDT +(\d+) bytes -> (/tmp/acpi/ssdt-\d+\.dat)", out)
        verificar(g is not None and int(g.group(1)) == len(dados), f"a: `tablasacpi` vuelca la SSDT entera\n{out}")
        verificar(re.search(r"DSDT +\d+ bytes", out) is not None, "a: y la DSDT")
        s = m.serial()
        verificar("EXCECAO" not in s and "[UACPI] la pila de uACPI se ha desbordado" not in s, "a: sin excepciones al final")


def parte_b():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h407b", "ide", imagem=img, som="ac97") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        out = m.comando("radios")
        verificar("Radios: no disponible (uACPI desactivado" in out, f"b: sin --uacpi, `radios` no disponible\n{out}")
        out = m.comando("radios wifi si")
        verificar("no se pudo encender Wi-Fi" in out, f"b: y no puede encender nada\n{out}")
        teclas_volume(m, "b")


def parte_c():
    """La BIOS no ve radios: se devuelve el botón a la BIOS (WMIF = 0) y el botón avisa."""
    ssdt = os.path.join(DIR, "ssdt-hpwmi-sin-radios.aml")
    with open(ssdt, "wb") as f:
        f.write(ssdt_hp(sem_radios=True))
    img = imagem_de_teste("", ["--vesa", "--uacpi"])
    with Maquina("h407c", "ide", imagem=img, extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        esperar(m, r"\[HPWMI\] la BIOS no ve ninguna radio: WMIF = 0", 90)
        verificar(True, "c: sin radios, la marca WMIF vuelve a 0")
        out = m.comando("radios")
        verificar("La BIOS dice que no hay radios" in out, f"c: `radios` lo dice\n{out}")
        out = m.comando("radios wifi si")
        verificar("no se pudo encender Wi-Fi" in out, f"c: no se puede encender por WMI\n{out}")
        esperar(m, r"\[GUI\] notificacao: Bot.n inal.mbrico pulsado", 60)
        verificar(True, "c: el botón (con WMIF = 0, la BIOS lo manda) avisa y se mira el PCI")
        esperar(m, r"\[HPWMI\] PCI: ninguna tarjeta nueva tras el boton", 40)
        verificar(True, "c: el PCI se mira y no hay aviso falso")


def main():
    try:
        anfitriao()
        if os.environ.get("SAGUI_SO_ANFITRIAO") != "1":
            for parte in (parte_a, parte_b, parte_c):
                parte()
                print(f"  {parte.__name__}: ok")
    except (FalhaTeste, AssertionError) as e:
        print(f"FALHOU: {e}")
        return 1
    print("h407: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
