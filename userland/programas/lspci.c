// SPDX-License-Identifier: GPL-2.0-or-later
/* lspci - lista los dispositivos PCI en el formato de Linux (D232), sobre lo que da el kernel. */
#include "sagui.h"

static const char *AYUDA =
    "uso: lspci [-n] [-v]\n"
    "  Una línea por función PCI: bus:dispositivo.función Clase: Fabricante [vendor:device] (rev NN).\n"
    "  -n  solo números (sin nombres)   -v  añade la IRQ y la interfaz de programación\n"
    "  Para los nombres de dispositivo concretos y el driver que usa cada uno: infopc -t pci.\n"
    "  Al final, los dispositivos ISA que detectó el kernel (puerto, IRQ y DMA), si hay.\n";

static const char *clase(uint8_t c, uint8_t s)
{
    switch ((c << 8) | s) {
    case 0x0100: return "SCSI storage controller"; case 0x0101: return "IDE interface"; case 0x0106: return "SATA controller";
    case 0x0108: return "Non-Volatile memory controller"; case 0x0180: return "Mass storage controller";
    case 0x0200: return "Ethernet controller"; case 0x0280: return "Network controller";
    case 0x0300: return "VGA compatible controller"; case 0x0302: return "3D controller"; case 0x0380: return "Display controller";
    case 0x0401: return "Multimedia audio controller"; case 0x0403: return "Audio device"; case 0x0480: return "Multimedia controller";
    case 0x0500: return "RAM memory"; case 0x0600: return "Host bridge"; case 0x0601: return "ISA bridge";
    case 0x0604: return "PCI bridge"; case 0x0680: return "Bridge"; case 0x0700: return "Serial controller";
    case 0x0780: return "Communication controller"; case 0x0805: return "SD Host controller"; case 0x0880: return "System peripheral";
    case 0x0C00: return "FireWire (IEEE 1394)"; case 0x0C03: return "USB controller"; case 0x0C05: return "SMBus";
    case 0x0D11: return "Bluetooth"; case 0x0D80: return "Network controller";
    }
    return NULL;
}

static const char *fabricante(uint16_t v)
{
    switch (v) {
    case 0x8086: return "Intel Corporation"; case 0x1022: return "Advanced Micro Devices, Inc. [AMD]"; case 0x1002: return "Advanced Micro Devices, Inc. [AMD/ATI]";
    case 0x10DE: return "NVIDIA Corporation"; case 0x10EC: return "Realtek Semiconductor Co., Ltd."; case 0x168C: return "Qualcomm Atheros";
    case 0x1969: return "Qualcomm Atheros"; case 0x14E4: return "Broadcom Inc."; case 0x1AF4: return "Red Hat, Inc.";
    case 0x1B36: return "Red Hat, Inc. (QEMU)"; case 0x1234: return "QEMU/Bochs"; case 0x80EE: return "InnoTek Systemberatung GmbH (VirtualBox)";
    case 0x15AD: return "VMware"; case 0x1106: return "VIA Technologies, Inc."; case 0x1039: return "Silicon Integrated Systems [SiS]";
    case 0x197B: return "JMicron Technology Corp."; case 0x104C: return "Texas Instruments"; case 0x11AB: return "Marvell Technology Group Ltd.";
    case 0x1B21: return "ASMedia Technology Inc."; case 0x1414: return "Microsoft Corporation";
    case 0x5333: return "S3 Graphics Co., Ltd"; case 0x121A: return "3Dfx Interactive, Inc."; case 0x102B: return "Matrox Electronics Systems Ltd.";
    case 0x1274: return "Ensoniq"; case 0x1013: return "Cirrus Logic"; case 0x12D2: return "NVidia / SGS Thomson";
    }
    return NULL;
}

int principal(int argc, char **argv)
{
    bool numeros = false, verboso = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "nvk", &o)) >= 0) {
        switch (r) {
        case 'n': numeros = true; break;
        case 'v': verboso = true; break;
        case 'k': break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("lspci", r, &o);
        }
    }
    static info_pci_t v[128];
    int n = info_sistema(INFO_PCI, v, 128);
    if (n < 0) { imprimir_em(ERROS, "lspci: no se puede leer el bus PCI: %s\n", nome_erro(n)); return 1; }
    for (int i = 0; i < n; i++) {
        const info_pci_t *p = &v[i];
        char cl[64], rev[16] = "";
        const char *c = clase(p->classe, p->subclasse), *f = fabricante(p->fabricante);
        if (numeros || !c) formatar_texto(cl, sizeof cl, "Class %02x%02x", p->classe, p->subclasse);
        else copia_texto(cl, c, sizeof cl);
        if (p->revisao) formatar_texto(rev, sizeof rev, " (rev %02x)", p->revisao);
        if (numeros || !f) imprimir("%02x:%02x.%u %s: %04x:%04x%s\n", p->barramento, p->dispositivo, p->funcao, cl, p->fabricante, p->modelo, rev);
        else imprimir("%02x:%02x.%u %s: %s Device [%04x:%04x]%s\n", p->barramento, p->dispositivo, p->funcao, cl, f, p->fabricante, p->modelo, rev);
        if (verboso) imprimir("\tIRQ %u, interfaz de programación %02x\n", p->irq, p->interface);
    }
    /* D302: los dispositivos ISA (no están en el PCI: los detecta su driver) */
    static info_isa_t isa[8];
    int ni = info_sistema(INFO_ISA, isa, 8);
    for (int i = 0; i < ni && i < 8; i++) {
        const info_isa_t *d = &isa[i];
        char rec[48] = "";
        int k = 0;
        if (d->irq != 0xFF) k += formatar_texto(rec + k, sizeof rec - (unsigned)k, ", IRQ %u", d->irq);
        if (d->dma8 != 0xFF) k += formatar_texto(rec + k, sizeof rec - (unsigned)k, ", DMA %u", d->dma8);
        if (d->dma16 != 0xFF) k += formatar_texto(rec + k, sizeof rec - (unsigned)k, "%s %u", d->dma8 != 0xFF ? " y" : ", DMA", d->dma16);
        char det[72] = "";
        if (d->detalhe[0]) formatar_texto(det, sizeof det, " [%s]", d->detalhe);
        if (d->porta) imprimir("ISA %03xh %s%s%s\n", d->porta, d->nome, rec, det);
        else imprimir("ISA ---- %s%s\n", d->nome, det);
    }
    return 0;
}
