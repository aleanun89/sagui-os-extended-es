// SPDX-License-Identifier: GPL-2.0-or-later
/* Nombres cortos de fabricantes y dispositivos PCI para infopc (compartidos por sus
 * secciones PCI, Vídeo y GPU). Solo texto: no toca el hardware. Si no se conoce el
 * dispositivo, los llamadores muestran al menos el fabricante. */
#ifndef PCINOMES_H
#define PCINOMES_H

#include <stdint.h>
#include <stddef.h>

#define PCI_NOTA_VOODOO12 "3dfx Voodoo Graphics / Voodoo2: acelerador 3D adicional (en cadena con la tarjeta de vídeo), sin driver en SaguiOS"

static inline int pci_es_voodoo12(uint16_t v, uint16_t d) { return v == 0x121A && (d == 0x0001 || d == 0x0002); }

static inline const char *pci_nome_fabricante(uint16_t v)
{
    switch (v) {
    case 0x8086: return "Intel"; case 0x1022: return "AMD"; case 0x1002: return "AMD/ATI";
    case 0x10DE: return "NVIDIA"; case 0x10EC: return "Realtek"; case 0x168C: return "Atheros";
    case 0x1969: return "Atheros"; case 0x14E4: return "Broadcom"; case 0x1AF4: return "Red Hat (virtio)";
    case 0x1B36: return "Red Hat (QEMU)"; case 0x1234: return "Bochs/QEMU"; case 0x80EE: return "VirtualBox";
    case 0x15AD: return "VMware"; case 0x1106: return "VIA"; case 0x1039: return "SiS";
    case 0x197B: return "JMicron"; case 0x1180: return "Ricoh"; case 0x1217: return "O2 Micro";
    case 0x104C: return "Texas Instr."; case 0x11AB: return "Marvell"; case 0x1B21: return "ASMedia";
    case 0x1414: return "Microsoft"; case 0x10B7: return "3Com"; case 0x1033: return "NEC";
    case 0x5333: return "S3"; case 0x121A: return "3dfx"; case 0x102B: return "Matrox";
    case 0x1274: return "Ensoniq"; case 0x1013: return "Cirrus Logic"; case 0x12D2: return "NVIDIA/SGS";
    case 0x1023: return "Trident"; case 0x105D: return "Number Nine"; case 0x100C: return "Tseng Labs";
    case 0x1011: return "DEC"; case 0x1057: return "Motorola"; case 0x10B9: return "ALi";
    case 0x1095: return "Silicon Image"; case 0x10AD: return "Winbond"; case 0x1102: return "Creative Labs";
    case 0x1073: return "Yamaha"; case 0x125D: return "ESS"; case 0x13F6: return "C-Media";
    case 0x1186: return "D-Link";
    default: return NULL;
    }
}

static inline const char *pci_nome_dispositivo(uint16_t v, uint16_t d)
{
    uint32_t id = ((uint32_t)v << 16) | d;
    switch (id) {
    case 0x80861237: return "440FX (i440FX PMC)";
    case 0x80867000: return "PIIX3 ISA";
    case 0x80867010: return "PIIX3 IDE";
    case 0x80867020: return "PIIX3 USB";
    case 0x80867113: return "PIIX4 ACPI";
    case 0x808629C0: return "Q35 (MCH)";
    case 0x80862918: return "ICH9 LPC";
    case 0x80862922: return "ICH9 AHCI";
    case 0x8086100E: return "82540EM Gigabit (e1000)";
    case 0x808624CD: return "82801DB EHCI";
    case 0x8086293A: return "ICH9 EHCI";
    case 0x80862668: return "ICH6 HD Audio";
    case 0x80860104: return "Sandy Bridge (host)";
    case 0x80860106: case 0x80860116: case 0x80860126: return "HD Graphics (Sandy Bridge)";
    case 0x80861C3A: return "6 Series MEI";
    case 0x80861C26: case 0x80861C2D: return "6 Series EHCI";
    case 0x80861C20: return "6 Series HD Audio";
    case 0x80861C49: return "HM65 LPC";
    case 0x80861C03: return "6 Series SATA AHCI";
    case 0x80861C22: return "6 Series SMBus";
    case 0x12341111: return "VGA estándar (Bochs)";
    case 0x10EC8136: return "RTL810xE Fast Ethernet";
    case 0x10EC8168: return "RTL8111/8168 Gigabit";
    case 0x10EC8139: return "RTL8139";
    case 0x19692060: return "AR8152 v1.1 Fast Ethernet";
    case 0x19692062: return "AR8152 v2.0 Fast Ethernet";
    case 0x19691073: return "AR8151 v1.0 Gigabit";
    case 0x19691083: return "AR8151 v2.0 Gigabit";
    case 0x19691063: return "AR8131 Gigabit";
    case 0x19691062: return "AR8132 Fast Ethernet";
    case 0x168C002B: return "AR9285 Wi-Fi";
    case 0x168C0032: return "AR9485 Wi-Fi";
    case 0x80EEBEEF: return "VGA de VirtualBox";
    case 0x80EECAFE: return "Guest Service";
    /* 3dfx */
    case 0x121A0001: return "Voodoo Graphics";
    case 0x121A0002: return "Voodoo2";
    case 0x121A0003: return "Voodoo Banshee";
    case 0x121A0005: return "Voodoo3";
    case 0x121A0009: return "Voodoo4/5 (VSA-100)";
    /* S3 (las de acel_s3.c) */
    case 0x53335631: return "ViRGE (86C325)";
    case 0x5333883D: return "ViRGE/VX (86C988)";
    case 0x53338A01: return "ViRGE/DX o GX (86C375/385)";
    case 0x53338A10: return "ViRGE/GX2 (86C357)";
    case 0x53338904: return "Trio3D (86C365)";
    case 0x53338A13: return "Trio3D/2X (86C362)";
    case 0x53338811: return "Trio64/Trio64V+ (86C764/765)";
    case 0x53338812: return "Aurora64V+";
    case 0x53338814: return "Trio64UV+ (86C767)";
    case 0x53338901: return "Trio64V2/DX o GX (86C775/785)";
    case 0x53338C01: return "Savage/MX";
    case 0x53338A20: return "Savage3D";
    case 0x53338A22: return "Savage4";
    /* ATI Mach64 y Rage (las de acel_mach64.c) */
    case 0x10024758: return "Mach64 GX"; case 0x10024358: return "Mach64 CX";
    case 0x10024354: return "Mach64 CT"; case 0x10024554: return "Mach64 ET";
    case 0x10025654: return "Mach64 VT"; case 0x10025655: return "Mach64 VT3"; case 0x10025656: return "Mach64 VT4";
    case 0x10024754: return "3D Rage (GT)"; case 0x10024755: return "3D Rage II+ (GU)";
    case 0x10024756: return "3D Rage IIC (GV)"; case 0x10024757: return "3D Rage IIC AGP (GW)";
    case 0x10024742: return "Rage Pro AGP (GB)"; case 0x10024744: return "Rage Pro AGP 1x (GD)";
    case 0x10024749: return "Rage Pro PCI (GI)"; case 0x10024750: return "Rage Pro PCI (GP)";
    case 0x10024751: return "Rage Pro PCI (GQ)";
    case 0x10024C42: return "Rage LT Pro AGP"; case 0x10024C49: return "Rage LT Pro PCI";
    case 0x1002474D: return "Rage XL AGP"; case 0x10024752: return "Rage XL PCI";
    case 0x10024C4D: return "Rage Mobility AGP 2x"; case 0x10024C52: return "Rage Mobility PCI";
    case 0x10025144: return "Radeon 7200 (R100)"; case 0x10025159: return "Radeon 7000/VE";
    /* Matrox */
    case 0x102B0518: return "MGA-2085PX Atlas"; case 0x102B0519: return "MGA-2064W Millennium";
    case 0x102B051A: return "MGA-1064SG Mystique"; case 0x102B051B: return "MGA-2164W Millennium II";
    case 0x102B051E: return "MGA-1064SG Mystique (rev.)"; case 0x102B051F: return "MGA-2164W Millennium II AGP";
    case 0x102B0520: return "MGA G200"; case 0x102B0521: return "MGA G200 AGP";
    case 0x102B1000: return "MGA G100"; case 0x102B1001: return "MGA G100 AGP";
    case 0x102B0525: return "MGA G400/G450";
    /* NVIDIA de época */
    case 0x10DE0020: return "RIVA TNT"; case 0x10DE0028: return "RIVA TNT2"; case 0x10DE0029: return "RIVA TNT2 Ultra";
    case 0x10DE0100: return "GeForce 256"; case 0x10DE0110: return "GeForce2 MX";
    case 0x10DE0150: return "GeForce2 GTS";
    /* SiS, VIA, Ensoniq, Cirrus */
    case 0x10390300: return "SiS 300/305 (VGA)"; case 0x10396326: return "SiS 6326 (VGA)";
    case 0x10398201: return "SiS 5591/5592 AGP";
    case 0x12741371: return "AudioPCI (ES1371)"; case 0x12745000: return "AudioPCI (ES1370)";
    case 0x101300A8: return "GD5434 Alpine"; case 0x101300B8: return "GD5446 Alpine";
    case 0x10130040: return "GD5465 Laguna3D";
    default: return NULL;
    }
}

#endif
