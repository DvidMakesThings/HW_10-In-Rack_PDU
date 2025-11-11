/**
 * @file src/drivers/ethernet_w5500regs.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config04 4. W5500 Register definitions
 * @ingroup config
 * @brief W5500 Register Definitions
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Complete W5500 register map.
 * 
 * @note This file is self-contained and doesn't depend on old library files.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef W5500_REGS_H
#define W5500_REGS_H

#include "../CONFIG.h"

/******************************************************************************
 *                          CHIP DEFINITIONS                                  *
 ******************************************************************************/

#define W5500_IOBASE 0x00000000
#define W5500_SOCKNUM 8

/* SPI operation modes */
#define _W5500_SPI_READ_ (0x00 << 2)
#define _W5500_SPI_WRITE_ (0x01 << 2)

/* Block select bits */
#define W5500_CREG_BLOCK 0x00
#define W5500_SREG_BLOCK(N) (1 + 4 * (N))
#define W5500_TXBUF_BLOCK(N) (2 + 4 * (N))
#define W5500_RXBUF_BLOCK(N) (3 + 4 * (N))

/* Address offset increment */
#define W5500_OFFSET_INC(ADDR, N) ((ADDR) + ((N) << 8))

/******************************************************************************
 *                          COMMON REGISTERS                                  *
 ******************************************************************************/

/* Mode Register */
#define MR (W5500_IOBASE + (0x0000 << 8) + (W5500_CREG_BLOCK << 3))

/* Gateway Address Register */
#define GAR (W5500_IOBASE + (0x0001 << 8) + (W5500_CREG_BLOCK << 3))

/* Subnet Mask Register */
#define SUBR (W5500_IOBASE + (0x0005 << 8) + (W5500_CREG_BLOCK << 3))

/* Source Hardware Address Register */
#define SHAR (W5500_IOBASE + (0x0009 << 8) + (W5500_CREG_BLOCK << 3))

/* Source IP Address Register */
#define SIPR (W5500_IOBASE + (0x000F << 8) + (W5500_CREG_BLOCK << 3))

/* Interrupt Low Level Timer Register */
#define INTLEVEL (W5500_IOBASE + (0x0013 << 8) + (W5500_CREG_BLOCK << 3))

/* Interrupt Register */
#define IR (W5500_IOBASE + (0x0015 << 8) + (W5500_CREG_BLOCK << 3))

/* Interrupt Mask Register */
#define _IMR_ (W5500_IOBASE + (0x0016 << 8) + (W5500_CREG_BLOCK << 3))

/* Socket Interrupt Register */
#define SIR (W5500_IOBASE + (0x0017 << 8) + (W5500_CREG_BLOCK << 3))

/* Socket Interrupt Mask Register */
#define SIMR (W5500_IOBASE + (0x0018 << 8) + (W5500_CREG_BLOCK << 3))

/* Retry Time Register */
#define _RTR_ (W5500_IOBASE + (0x0019 << 8) + (W5500_CREG_BLOCK << 3))

/* Retry Count Register */
#define _RCR_ (W5500_IOBASE + (0x001B << 8) + (W5500_CREG_BLOCK << 3))

/* PPP LCP Request Timer Register */
#define PTIMER (W5500_IOBASE + (0x001C << 8) + (W5500_CREG_BLOCK << 3))

/* PPP LCP Magic Number Register */
#define PMAGIC (W5500_IOBASE + (0x001D << 8) + (W5500_CREG_BLOCK << 3))

/* PPP Destination MAC Address Register */
#define PHAR (W5500_IOBASE + (0x001E << 8) + (W5500_CREG_BLOCK << 3))

/* PPP Session ID Register */
#define PSID (W5500_IOBASE + (0x0024 << 8) + (W5500_CREG_BLOCK << 3))

/* PPP Maximum Receive Unit Register */
#define PMRU (W5500_IOBASE + (0x0026 << 8) + (W5500_CREG_BLOCK << 3))

/* Unreachable IP Address Register */
#define UIPR (W5500_IOBASE + (0x0028 << 8) + (W5500_CREG_BLOCK << 3))

/* Unreachable Port Register */
#define UPORTR (W5500_IOBASE + (0x002C << 8) + (W5500_CREG_BLOCK << 3))

/* PHY Configuration Register */
#define PHYCFGR (W5500_IOBASE + (0x002E << 8) + (W5500_CREG_BLOCK << 3))

/* Chip Version Register */
#define VERSIONR (W5500_IOBASE + (0x0039 << 8) + (W5500_CREG_BLOCK << 3))

/******************************************************************************
 *                          SOCKET REGISTERS                                  *
 ******************************************************************************/

/* Socket n Mode Register */
#define Sn_MR(N) (W5500_IOBASE + (0x0000 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Command Register */
#define Sn_CR(N) (W5500_IOBASE + (0x0001 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Interrupt Register */
#define Sn_IR(N) (W5500_IOBASE + (0x0002 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Status Register */
#define Sn_SR(N) (W5500_IOBASE + (0x0003 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Source Port Register */
#define Sn_PORT(N) (W5500_IOBASE + (0x0004 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Destination Hardware Address Register */
#define Sn_DHAR(N) (W5500_IOBASE + (0x0006 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Destination IP Address Register */
#define Sn_DIPR(N) (W5500_IOBASE + (0x000C << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Destination Port Register */
#define Sn_DPORT(N) (W5500_IOBASE + (0x0010 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Maximum Segment Size Register */
#define Sn_MSSR(N) (W5500_IOBASE + (0x0012 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n IP TOS Register */
#define Sn_TOS(N) (W5500_IOBASE + (0x0015 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n IP TTL Register */
#define Sn_TTL(N) (W5500_IOBASE + (0x0016 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n RX Buffer Size Register */
#define Sn_RXBUF_SIZE(N) (W5500_IOBASE + (0x001E << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n TX Buffer Size Register */
#define Sn_TXBUF_SIZE(N) (W5500_IOBASE + (0x001F << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n TX Free Size Register */
#define Sn_TX_FSR(N) (W5500_IOBASE + (0x0020 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n TX Read Pointer Register */
#define Sn_TX_RD(N) (W5500_IOBASE + (0x0022 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n TX Write Pointer Register */
#define Sn_TX_WR(N) (W5500_IOBASE + (0x0024 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n RX Received Size Register */
#define Sn_RX_RSR(N) (W5500_IOBASE + (0x0026 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n RX Read Pointer Register */
#define Sn_RX_RD(N) (W5500_IOBASE + (0x0028 << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n RX Write Pointer Register */
#define Sn_RX_WR(N) (W5500_IOBASE + (0x002A << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Interrupt Mask Register */
#define Sn_IMR(N) (W5500_IOBASE + (0x002C << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Fragment Offset in IP Header Register */
#define Sn_FRAG(N) (W5500_IOBASE + (0x002D << 8) + (W5500_SREG_BLOCK(N) << 3))

/* Socket n Keep Alive Timer Register */
#define Sn_KPALVTR(N) (W5500_IOBASE + (0x002F << 8) + (W5500_SREG_BLOCK(N) << 3))

/******************************************************************************
 *                          MODE REGISTER BITS                                *
 ******************************************************************************/

/* MR (Mode Register) bits */
#define MR_RST 0x80   /* Software reset */
#define MR_WOL 0x20   /* Wake on LAN */
#define MR_PB 0x10    /* Ping block */
#define MR_PPPOE 0x08 /* PPPoE mode */
#define MR_FARP 0x02  /* Force ARP */

/* PHYCFGR (PHY Configuration Register) bits */
#define PHYCFGR_RST 0x80         /* PHY reset */
#define PHYCFGR_OPMD 0x40        /* Operation mode */
#define PHYCFGR_OPMDC_ALLA 0x38  /* All auto-negotiation */
#define PHYCFGR_OPMDC_PDOWN 0x30 /* Power down */
#define PHYCFGR_OPMDC_NA 0x20    /* Auto-negotiation disabled */
#define PHYCFGR_OPMDC_100FA 0x18 /* 100BASE-TX full-duplex */
#define PHYCFGR_OPMDC_100H 0x10  /* 100BASE-TX half-duplex */
#define PHYCFGR_OPMDC_10FA 0x08  /* 10BASE-T full-duplex */
#define PHYCFGR_OPMDC_10H 0x00   /* 10BASE-T half-duplex */
#define PHYCFGR_DPX_FULL 0x04    /* Full duplex */
#define PHYCFGR_SPD_100 0x02     /* 100Mbps */
#define PHYCFGR_LNK_ON 0x01      /* Link up */

/******************************************************************************
 *                          SOCKET MODE REGISTER BITS                         *
 ******************************************************************************/

/* Sn_MR (Socket Mode Register) values */
#define Sn_MR_CLOSE 0x00  /* Closed */
#define Sn_MR_TCP 0x01    /* TCP mode */
#define Sn_MR_UDP 0x02    /* UDP mode */
#define Sn_MR_IPRAW 0x03  /* IP raw mode */
#define Sn_MR_MACRAW 0x04 /* MAC raw mode */

/* Sn_MR flags */
#define Sn_MR_UCASTB 0x10 /* Unicast block (UDP multicast) */
#define Sn_MR_MIP6B 0x10  /* IPv6 block (MACRAW) */
#define Sn_MR_MMB 0x20    /* Multicast block (MACRAW) */
#define Sn_MR_BCASTB 0x20 /* Broadcast block (UDP) */
#define Sn_MR_ND 0x20     /* No delayed ACK (TCP) */
#define Sn_MR_MC 0x20     /* IGMP version (UDP multicast) */
#define Sn_MR_MFEN 0x80   /* MAC filter (MACRAW) */
#define Sn_MR_MULTI 0x80  /* Multicast (UDP) */

/******************************************************************************
 *                          SOCKET COMMAND REGISTER VALUES                    *
 ******************************************************************************/

/* Sn_CR (Socket Command Register) values */
#define Sn_CR_OPEN 0x01      /* Open socket */
#define Sn_CR_LISTEN 0x02    /* Listen (TCP server) */
#define Sn_CR_CONNECT 0x04   /* Connect (TCP client) */
#define Sn_CR_DISCON 0x08    /* Disconnect */
#define Sn_CR_CLOSE 0x10     /* Close socket */
#define Sn_CR_SEND 0x20      /* Send data */
#define Sn_CR_SEND_MAC 0x21  /* Send MAC (UDP) */
#define Sn_CR_SEND_KEEP 0x22 /* Send keep-alive (TCP) */
#define Sn_CR_RECV 0x40      /* Receive data */

/******************************************************************************
 *                          SOCKET INTERRUPT REGISTER BITS                    *
 ******************************************************************************/

/* Sn_IR (Socket Interrupt Register) bits */
#define Sn_IR_SENDOK 0x10  /* Send OK */
#define Sn_IR_TIMEOUT 0x08 /* Timeout */
#define Sn_IR_RECV 0x04    /* Receive */
#define Sn_IR_DISCON 0x02  /* Disconnected */
#define Sn_IR_CON 0x01     /* Connected */

/******************************************************************************
 *                          SOCKET STATUS REGISTER VALUES                     *
 ******************************************************************************/

/* Sn_SR (Socket Status Register) values */
#define SOCK_CLOSED 0x00      /* Socket closed */
#define SOCK_INIT 0x13        /* Socket initialized (TCP) */
#define SOCK_LISTEN 0x14      /* Socket listening (TCP server) */
#define SOCK_SYNSENT 0x15     /* SYN sent (TCP client connecting) */
#define SOCK_SYNRECV 0x16     /* SYN received */
#define SOCK_ESTABLISHED 0x17 /* Connection established (TCP) */
#define SOCK_FIN_WAIT 0x18    /* FIN wait */
#define SOCK_CLOSING 0x1A     /* Closing */
#define SOCK_TIME_WAIT 0x1B   /* Time wait */
#define SOCK_CLOSE_WAIT 0x1C  /* Close wait */
#define SOCK_LAST_ACK 0x1D    /* Last ACK */
#define SOCK_UDP 0x22         /* UDP socket */
#define SOCK_IPRAW 0x32       /* IP raw socket */
#define SOCK_MACRAW 0x42      /* MAC raw socket */

/******************************************************************************
 *                          PHY CONFIGURATION VALUES                          *
 ******************************************************************************/

/* PHY configuration modes */
#define PHY_CONFBY_HW 0     /* Configure by hardware */
#define PHY_CONFBY_SW 1     /* Configure by software */
#define PHY_MODE_MANUAL 0   /* Manual mode */
#define PHY_MODE_AUTONEGO 1 /* Auto-negotiation */
#define PHY_SPEED_10 0      /* 10Mbps */
#define PHY_SPEED_100 1     /* 100Mbps */
#define PHY_DUPLEX_HALF 0   /* Half-duplex */
#define PHY_DUPLEX_FULL 1   /* Full-duplex */

#endif /* W5500_REGS_H */

/** @} */