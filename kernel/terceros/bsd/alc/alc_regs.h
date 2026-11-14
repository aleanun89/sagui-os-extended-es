// SPDX-License-Identifier: BSD-2-Clause
/*
 * Extraído de sys/dev/alc/if_alcreg.h y sys/dev/mii/atphyreg.h de FreeBSD
 * (driver alc(4) de las Atheros AR813x/AR815x y PHY atphy(4)).
 *
 * Copyright (c) 2008, 2009, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * SaguiOS (D200): solo DEFINICIONES, sin lógica: identificadores PCI,
 * desplazamientos y bits de los registros de la familia AR813x/AR815x
 * (L1C/L2C), el formato de los descriptores (TPD, RFD, RRD), los registros
 * de depuración del PHY con sus valores del fabricante y el registro de
 * estado propio del PHY Atheros F1. Solo lo que usa kernel/rede/alc.c; los
 * nombres son los de FreeBSD para poder cotejarlos con el original. Las
 * familias AR816x/AR817x y Killer no están: el driver no las soporta.
 * Cambio frente al original: los valores ANA_CFG0/ANA_CFG54 se dan ya
 * calculados (ALC_ANA_CFG0_VALOR, ALC_ANA_CFG54_VALOR); ver D200.
 */
#ifndef SAGUI_TERCEROS_ALC_REGS_H
#define SAGUI_TERCEROS_ALC_REGS_H

/* ---------------------------------------------------------- PCI */
#define VENDORID_ATHEROS            0x1969
#define DEVICEID_ATHEROS_AR8131     0x1063   /* L1C */
#define DEVICEID_ATHEROS_AR8132     0x1062   /* L2C */
#define DEVICEID_ATHEROS_AR8151     0x1073   /* L1D V1.0 */
#define DEVICEID_ATHEROS_AR8151_V2  0x1083   /* L1D V2.0 */
#define DEVICEID_ATHEROS_AR8152_B   0x2060   /* L2C V1.1 */
#define DEVICEID_ATHEROS_AR8152_B2  0x2062   /* L2C V2.0 */
#define ATHEROS_AR8152_B_V10        0xC0     /* revisión PCI */
#define ATHEROS_AR8152_B_V11        0xC1

/* ---------------------------------------------------------- registros */
/* 0x0000-0x02FF: copia del espacio de configuración PCIe. */
#define ALC_PEX_UNC_ERR_SEV         0x10C
#define PEX_UNC_ERR_SEV_DLP         0x00000010
#define PEX_UNC_ERR_SEV_FCP         0x00002000

#define ALC_TWSI_CFG                0x218
#define TWSI_CFG_SW_LD_START        0x00000800

#define ALC_PCIE_PHYMISC            0x1000
#define PCIE_PHYMISC_FORCE_RCV_DET  0x00000004
#define ALC_PCIE_PHYMISC2           0x1004
#define PCIE_PHYMISC2_SERDES_CDR_MASK   0x00030000
#define PCIE_PHYMISC2_SERDES_TH_MASK    0x000C0000
#define PCIE_PHYMISC2_SERDES_CDR_SHIFT  16
#define PCIE_PHYMISC2_SERDES_TH_SHIFT   18

#define ALC_TWSI_DEBUG              0x1108
#define TWSI_DEBUG_DEV_EXIST        0x20000000

#define ALC_OPT_CFG                 0x12F0
#define OPT_CFG_CLK_ENB             0x00000002

#define ALC_PM_CFG                  0x12F8
#define PM_CFG_ASPM_L1_ENB          0x00000008
#define PM_CFG_SERDES_L1_ENB        0x00000010
#define PM_CFG_SERDES_PLL_L1_ENB    0x00000020
#define PM_CFG_SERDES_PD_EX_L1      0x00000040
#define PM_CFG_SERDES_BUDS_RX_L1_ENB 0x00000080
#define PM_CFG_ASPM_L0S_ENB         0x00001000
#define PM_CFG_CLK_SWH_L1           0x00002000
#define PM_CFG_PCIE_RECV            0x00008000
#define PM_CFG_L1_ENTRY_TIMER_MASK  0x000F0000
#define PM_CFG_PM_REQ_TIMER_MASK    0x00F00000
#define PM_CFG_LCKDET_TIMER_MASK    0x0F000000
#define PM_CFG_EN_BUFS_RX_L0S       0x10000000
#define PM_CFG_SA_DLY_ENB           0x20000000
#define PM_CFG_MAC_ASPM_CHK         0x40000000
#define PM_CFG_HOTRST               0x80000000
#define PM_CFG_L1_ENTRY_TIMER_SHIFT 16
#define PM_CFG_PM_REQ_TIMER_SHIFT   20
#define PM_CFG_LCKDET_TIMER_SHIFT   24
#define PM_CFG_L1_ENTRY_TIMER_DEFAULT 1
#define PM_CFG_LCKDET_TIMER_DEFAULT 12
#define PM_CFG_PM_REQ_TIMER_DEFAULT 12

#define ALC_LTSSM_ID_CFG            0x12FC
#define LTSSM_ID_WRO_ENB            0x00001000

#define ALC_MASTER_CFG              0x1400
#define MASTER_RESET                0x00000001
#define MASTER_OOB_DIS_OFF          0x00000040
#define MASTER_SA_TIMER_ENB         0x00000080
#define MASTER_IM_TX_TIMER_ENB      0x00000400
#define MASTER_IM_RX_TIMER_ENB      0x00000800
#define MASTER_OTP_SEL              0x80000000
#define MASTER_CHIP_REV_SHIFT       16

/* Tics de 2 us en los temporizadores de las AR813x/AR815x. */
#define ALC_TICK_USECS              2
#define ALC_USECS(x)                ((x) / ALC_TICK_USECS)
#define ALC_IM_TIMER                0x1408
#define IM_TIMER_TX_SHIFT           0
#define IM_TIMER_RX_SHIFT           16
#define ALC_IM_RX_TIMER_DEFAULT     100      /* us */
#define ALC_IM_TX_TIMER_DEFAULT     1000     /* us */

#define ALC_GPHY_CFG                0x140C   /* 16 bits */
#define GPHY_CFG_EXT_RESET          0x0001
#define GPHY_CFG_SEL_ANA_RESET      0x1000

#define ALC_IDLE_STATUS             0x1410
#define IDLE_STATUS_RXMAC           0x00000001
#define IDLE_STATUS_TXMAC           0x00000002
#define IDLE_STATUS_RXQ             0x00000004
#define IDLE_STATUS_TXQ             0x00000008

#define ALC_MDIO                    0x1414
#define MDIO_DATA_MASK              0x0000FFFF
#define MDIO_REG_ADDR_MASK          0x001F0000
#define MDIO_OP_READ                0x00200000
#define MDIO_OP_WRITE               0x00000000
#define MDIO_SUP_PREAMBLE           0x00400000
#define MDIO_OP_EXECUTE             0x00800000
#define MDIO_CLK_25_4               0x00000000
#define MDIO_OP_BUSY                0x08000000
#define MDIO_REG_ADDR_SHIFT         16
#define MDIO_REG_ADDR(x)            (((x) << MDIO_REG_ADDR_SHIFT) & MDIO_REG_ADDR_MASK)

#define ALC_SERDES_LOCK             0x1424
#define SERDES_MAC_CLK_SLOWDOWN     0x00020000
#define SERDES_PHY_CLK_SLOWDOWN     0x00040000

#define ALC_MAC_CFG                 0x1480
#define MAC_CFG_TX_ENB              0x00000001
#define MAC_CFG_RX_ENB              0x00000002
#define MAC_CFG_TX_FC               0x00000004
#define MAC_CFG_RX_FC               0x00000008
#define MAC_CFG_FULL_DUPLEX         0x00000020
#define MAC_CFG_TX_CRC_ENB          0x00000040
#define MAC_CFG_TX_AUTO_PAD         0x00000080
#define MAC_CFG_PREAMBLE_MASK       0x00003C00
#define MAC_CFG_VLAN_TAG_STRIP      0x00004000
#define MAC_CFG_PROMISC             0x00008000
#define MAC_CFG_SPEED_MASK          0x00300000
#define MAC_CFG_SPEED_10_100        0x00100000
#define MAC_CFG_SPEED_1000          0x00200000
#define MAC_CFG_ALLMULTI            0x02000000
#define MAC_CFG_BCAST               0x04000000
#define MAC_CFG_HASH_ALG_CRC32      0x20000000
#define MAC_CFG_SPEED_MODE_SW       0x40000000
#define MAC_CFG_PREAMBLE_SHIFT      10
#define MAC_CFG_PREAMBLE_DEFAULT    7

#define ALC_IPG_IFG_CFG             0x1484
#define IPG_IFG_IPGT_MASK           0x0000007F
#define IPG_IFG_MIFG_MASK           0x0000FF00
#define IPG_IFG_IPG1_MASK           0x007F0000
#define IPG_IFG_IPG2_MASK           0x7F000000
#define IPG_IFG_IPGT_SHIFT          0
#define IPG_IFG_IPGT_DEFAULT        0x60
#define IPG_IFG_MIFG_SHIFT          8
#define IPG_IFG_MIFG_DEFAULT        0x50
#define IPG_IFG_IPG1_SHIFT          16
#define IPG_IFG_IPG1_DEFAULT        0x40
#define IPG_IFG_IPG2_SHIFT          24
#define IPG_IFG_IPG2_DEFAULT        0x60

#define ALC_PAR0                    0x1488   /* bytes 2..5 de la MAC */
#define ALC_PAR1                    0x148C   /* bytes 0..1 */
#define ALC_MAR0                    0x1490
#define ALC_MAR1                    0x1494

#define ALC_HDPX_CFG                0x1498
#define HDPX_CFG_LCOL_MASK          0x000003FF
#define HDPX_CFG_RETRY_MASK         0x0000F000
#define HDPX_CFG_EXC_DEF_EN         0x00010000
#define HDPX_CFG_ABEBT_MASK         0x00F00000
#define HDPX_CFG_JAMIPG_MASK        0x0F000000
#define HDPX_CFG_LCOL_SHIFT         0
#define HDPX_CFG_LCOL_DEFAULT       0x37
#define HDPX_CFG_RETRY_SHIFT        12
#define HDPX_CFG_RETRY_DEFAULT      0x0F
#define HDPX_CFG_ABEBT_SHIFT        20
#define HDPX_CFG_ABEBT_DEFAULT      0x0A
#define HDPX_CFG_JAMIPG_SHIFT       24
#define HDPX_CFG_JAMIPG_DEFAULT     0x07

#define ALC_FRAME_SIZE              0x149C
#define ALC_WOL_CFG                 0x14A0

#define ALC_RSS_IDT_TABLE0          0x14E0
#define ALC_SRAM_RD0_ADDR           0x1500
#define ALC_SRAM_TD_ADDR            0x1518
#define ALC_SRAM_RX_FIFO_ADDR       0x1520
#define ALC_SRAM_RX_FIFO_LEN        0x1524
#define ALC_SRAM_TX_FIFO_ADDR       0x1528
#define ALC_SRAM_TX_FIFO_LEN        0x152C
#define ALC_DMA_BLOCK               0x1534
#define DMA_BLOCK_LOAD              0x00000001
#define ALC_RX_BASE_ADDR_HI         0x1540
#define ALC_TX_BASE_ADDR_HI         0x1544
#define ALC_SMB_BASE_ADDR_HI        0x1548
#define ALC_SMB_BASE_ADDR_LO        0x154C
#define ALC_RD0_HEAD_ADDR_LO        0x1550
#define ALC_RD1_HEAD_ADDR_LO        0x1554
#define ALC_RD2_HEAD_ADDR_LO        0x1558
#define ALC_RD3_HEAD_ADDR_LO        0x155C
#define ALC_RD_RING_CNT             0x1560
#define RD_RING_CNT_MASK            0x00000FFF
#define ALC_RX_BUF_SIZE             0x1564
/* Con búferes de más de 1536 bytes la tarjeta se cuelga (límite del hardware). */
#define RX_BUF_SIZE_MAX             1536
#define ALC_RRD0_HEAD_ADDR_LO       0x1568
#define ALC_RRD1_HEAD_ADDR_LO       0x156C
#define ALC_RRD2_HEAD_ADDR_LO       0x1570
#define ALC_RRD3_HEAD_ADDR_LO       0x1574
#define ALC_RRD_RING_CNT            0x1578
#define RRD_RING_CNT_MASK           0x00000FFF
#define ALC_TDH_HEAD_ADDR_LO        0x157C
#define ALC_TDL_HEAD_ADDR_LO        0x1580
#define ALC_TD_RING_CNT             0x1584
#define TD_RING_CNT_MASK            0x0000FFFF
#define ALC_CMB_BASE_ADDR_LO        0x1588

#define ALC_TXQ_CFG                 0x1590
#define TXQ_CFG_TD_BURST_MASK       0x0000000F
#define TXQ_CFG_IP_OPTION_ENB       0x00000010
#define TXQ_CFG_ENB                 0x00000020
#define TXQ_CFG_ENHANCED_MODE       0x00000040
#define TXQ_CFG_8023_ENB            0x00000080
#define TXQ_CFG_TX_FIFO_BURST_MASK  0xFFFF0000
#define TXQ_CFG_TD_BURST_SHIFT      0
#define TXQ_CFG_TD_BURST_DEFAULT    5
#define TXQ_CFG_TX_FIFO_BURST_SHIFT 16

#define ALC_TSO_OFFLOAD_THRESH      0x1594   /* unidades de 8 bytes */
#define TSO_OFFLOAD_THRESH_MASK     0x000007FF
#define TSO_OFFLOAD_THRESH_UNIT_SHIFT 3
#define ALC_TXF_WATER_MARK          0x1598

#define ALC_RXQ_CFG                 0x15A0
#define RXQ_CFG_ASPM_THROUGHPUT_LIMIT_100M 0x00000003
#define RXQ_CFG_QUEUE1_ENB          0x00000010
#define RXQ_CFG_QUEUE2_ENB          0x00000020
#define RXQ_CFG_QUEUE3_ENB          0x00000040
#define RXQ_CFG_RD_BURST_MASK       0x03F00000
#define RXQ_CFG_RSS_MODE_DIS        0x00000000
#define RXQ_CFG_QUEUE0_ENB          0x80000000
#define RXQ_CFG_RD_BURST_DEFAULT    8
#define RXQ_CFG_RD_BURST_SHIFT      20
#define RXQ_CFG_ENB                 (RXQ_CFG_QUEUE0_ENB | RXQ_CFG_QUEUE1_ENB | \
                                     RXQ_CFG_QUEUE2_ENB | RXQ_CFG_QUEUE3_ENB)

#define ALC_RX_RD_FREE_THRESH       0x15A4
#define RX_RD_FREE_THRESH_HI_MASK   0x0000003F
#define RX_RD_FREE_THRESH_LO_MASK   0x00000FC0
#define RX_RD_FREE_THRESH_HI_SHIFT  0
#define RX_RD_FREE_THRESH_LO_SHIFT  6
#define RX_RD_FREE_THRESH_HI_DEFAULT 16
#define RX_RD_FREE_THRESH_LO_DEFAULT 8

#define ALC_RX_FIFO_PAUSE_THRESH    0x15A8
#define RX_FIFO_PAUSE_THRESH_LO_MASK 0x00000FFF
#define RX_FIFO_PAUSE_THRESH_HI_MASK 0x0FFF0000
#define RX_FIFO_PAUSE_THRESH_LO_SHIFT 0
#define RX_FIFO_PAUSE_THRESH_HI_SHIFT 16

#define ALC_RD_DMA_CFG              0x15AC
#define ALC_RSS_CPU                 0x15B8

#define ALC_DMA_CFG                 0x15C0
#define DMA_CFG_OUT_ORDER           0x00000004
#define DMA_CFG_RCB_64              0x00000000
#define DMA_CFG_RCB_128             0x00000008
#define DMA_CFG_RD_REQ_PRI          0x00000400
#define DMA_CFG_RD_DELAY_CNT_MASK   0x0000F800
#define DMA_CFG_WR_DELAY_CNT_MASK   0x000F0000
#define DMA_CFG_CMB_ENB             0x00100000
#define DMA_CFG_SMB_ENB             0x00200000
#define DMA_CFG_SMB_DIS             0x01000000
#define DMA_CFG_RD_BURST_MASK       0x07
#define DMA_CFG_RD_BURST_SHIFT      4
#define DMA_CFG_WR_BURST_MASK       0x07
#define DMA_CFG_WR_BURST_SHIFT      7
#define DMA_CFG_RD_DELAY_CNT_SHIFT  11
#define DMA_CFG_WR_DELAY_CNT_SHIFT  16
#define DMA_CFG_RD_DELAY_CNT_DEFAULT 15
#define DMA_CFG_WR_DELAY_CNT_DEFAULT 4

#define ALC_SMB_STAT_TIMER          0x15C4
#define ALC_CMB_TD_THRESH           0x15C8
#define ALC_CMB_TX_TIMER            0x15CC

#define ALC_MBOX_RD0_PROD_IDX       0x15E0
#define ALC_MBOX_TD_PROD_IDX        0x15F0
#define MBOX_TD_PROD_LO_IDX_MASK    0xFFFF0000
#define MBOX_TD_PROD_LO_IDX_SHIFT   16
#define ALC_MBOX_TD_CONS_IDX        0x15F4
#define MBOX_TD_CONS_LO_IDX_MASK    0xFFFF0000
#define MBOX_TD_CONS_LO_IDX_SHIFT   16
#define ALC_MBOX_RD01_CONS_IDX      0x15F8

#define ALC_INTR_STATUS             0x1600
#define INTR_RX_FIFO_OFLOW          0x00000008
#define INTR_RD0_UNDERRUN           0x00000010
#define INTR_TX_FIFO_UNDERRUN       0x00000100
#define INTR_DMA_RD_TO_RST          0x00000200
#define INTR_DMA_WR_TO_RST          0x00000400
#define INTR_GPHY                   0x00001000
#define INTR_TXQ_TO_RST             0x00004000
#define INTR_TX_PKT0                0x00008000
#define INTR_RX_PKT0                0x00010000
#define INTR_DIS_INT                0x80000000
#define INTR_TX_PKT                 INTR_TX_PKT0
#define INTR_RX_PKT                 INTR_RX_PKT0
#define INTR_RD_UNDERRUN            INTR_RD0_UNDERRUN
#define ALC_INTR_MASK               0x1604
#define ALC_INTRS                   (INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST | \
                                     INTR_TXQ_TO_RST | INTR_RX_PKT | INTR_TX_PKT | \
                                     INTR_RX_FIFO_OFLOW | INTR_RD_UNDERRUN | \
                                     INTR_TX_FIFO_UNDERRUN)
#define ALC_INTR_RETRIG_TIMER       0x1608
#define ALC_HDS_CFG                 0x160C

/* Contadores del MAC (se borran al leerlos): 24 de recepción y 25 de envío. */
#define ALC_RX_MIB_BASE             0x1700
#define ALC_TX_MIB_BASE             0x1760
#define ALC_RX_MIB_N                24
#define ALC_TX_MIB_N                25
#define ALC_MIB_RX_FRAMES           0    /* índices dentro de cada bloque */
#define ALC_MIB_RX_CRCERRS          5
#define ALC_MIB_RX_RUNTS            8
#define ALC_MIB_RX_FIFO_OFLOWS      18
#define ALC_MIB_RX_RRS_ERRS         19
#define ALC_MIB_TX_FRAMES           0
#define ALC_MIB_TX_UNDERRUN         19

#define ALC_CLK_GATING_CFG          0x1814

/* ---------------------------------------------------------- PHY */
#define ALC_PHY_ADDR                0
/* Registros de depuración del PHY: dirección en 0x1D, dato en 0x1E. */
#define ALC_MII_DBG_ADDR            0x1D
#define ALC_MII_DBG_DATA            0x1E
#define MII_ANA_CFG0                0x00
#define MII_ANA_CFG4                0x04
#define MII_ANA_CFG5                0x05
#define MII_ANA_CFG18               0x12
#define MII_ANA_CFG54               0x36
/* Valores del fabricante que escribe alc_phy_reset_813x(). */
#define ALC_ANA_CFG18_VALOR         0x4C04   /* LOOP_SEL_10BT|EN_MASK_TB|EN_10BT_IDLE|intervalo 1 */
#define ALC_ANA_CFG5_VALOR          0x2C42   /* CDR_BW 2|EN_DEEM|SEL_HSP|EN_PLL|EN_LCKDT */
#define ALC_ANA_CFG54_VALOR         0xE12C   /* cable largo 44, corto 33, BP_BAD_LINK_ACCUM|BP_SMALL_BW */
#define ALC_ANA_CFG4_VALOR          0x88BB   /* IECHO_ADJ 11,11,8,8 */
#define ALC_ANA_CFG0_VALOR          0x02EF   /* MANUL_SWICH_ON 7|RESTART_CAL|MAN_ENABLE|SEL_HSP|EN_HB|OEN_125M */

/* PHY Atheros F1 (atphy): registro de control y de estado propios. */
#define ATPHY_SCR                   0x10
#define ATPHY_SCR_POLARITY_REVERSAL 0x0002
#define ATPHY_SCR_MAC_PDOWN         0x0008
#define ATPHY_SCR_AUTO_X_MODE       0x0060
#define ATPHY_SCR_ASSERT_CRS_ON_TX  0x0800
#define ATPHY_SSR                   0x11
#define ATPHY_SSR_SPD_DPLX_RESOLVED 0x0800
#define ATPHY_SSR_DUPLEX            0x2000
#define ATPHY_SSR_SPEED_MASK        0xC000
#define ATPHY_SSR_10MBS             0x0000
#define ATPHY_SSR_100MBS            0x4000
#define ATPHY_SSR_1000MBS           0x8000

/* ---------------------------------------------------------- descriptores */
/* TPD (envío), 16 bytes: palabra 0 = longitud (bits 0-13) y VLAN (16-31);
 * palabra 1 = indicadores; palabras 2-3 = dirección física de 64 bits. */
#define TD_BUFLEN_MASK              0x00003FFF
#define TD_ETHERNET                 0x00000000
#define TD_EOP                      0x80000000
/* RFD (búfer libre de recepción), 8 bytes: la dirección física. */
/* RRD (retorno de recepción), 16 bytes: rdinfo, rss, vtag, status. */
#define RRD_RD_CNT_MASK             0x000F0000
#define RRD_RD_IDX_MASK             0xFFF00000
#define RRD_RD_CNT_SHIFT            16
#define RRD_RD_IDX_SHIFT            20
#define RRD_LEN_MASK                0x00003FFF
#define RRD_VLAN_TAG                0x00010000
#define RRD_ERR_SUM                 0x00100000
#define RRD_ERR_CRC                 0x00200000
#define RRD_ERR_ALIGN               0x00400000
#define RRD_ERR_TRUNC               0x00800000
#define RRD_ERR_RUNT                0x01000000
#define RRD_FIFO_FULL               0x20000000
#define RRD_ERR_LENGTH              0x40000000
#define RRD_VALID                   0x80000000

/* Tamaño máximo de trama que alc(4) programa en ALC_FRAME_SIZE. */
#define ALC_MAX_FRAMELEN_813X       (9 * 1024)   /* AR8131/AR8132 */
#define ALC_MAX_FRAMELEN_815X       (6 * 1024)   /* AR8151/AR8152 */

#endif
