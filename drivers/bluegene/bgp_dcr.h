/*********************************************************************
 *
 * Copyright (C) 2007-2008, IBM Corporation
 *
 * Description:   BGP DCR map (copied from bpcore)
 *
 * All rights reserved
 *
 ********************************************************************/

#ifndef _BGP_DCR_H_
#define _BGP_DCR_H_

#define _BN(b)    ((1<<(31-(b))))
#define _B1(b,x)  (((x)&0x1)<<(31-(b)))
#define _B2(b,x)  (((x)&0x3)<<(31-(b)))
#define _B3(b,x)  (((x)&0x7)<<(31-(b)))
#define _B4(b,x)  (((x)&0xF)<<(31-(b)))
#define _B5(b,x)  (((x)&0x1F)<<(31-(b)))
#define _B6(b,x)  (((x)&0x3F)<<(31-(b)))
#define _B7(b,x)  (((x)&0x7F)<<(31-(b)))
#define _B8(b,x)  (((x)&0xFF)<<(31-(b)))
#define _B9(b,x)  (((x)&0x1FF)<<(31-(b)))
#define _B10(b,x) (((x)&0x3FF)<<(31-(b)))
#define _B11(b,x) (((x)&0x7FF)<<(31-(b)))
#define _B12(b,x) (((x)&0xFFF)<<(31-(b)))
#define _B13(b,x) (((x)&0x1FFF)<<(31-(b)))
#define _B14(b,x) (((x)&0x3FFF)<<(31-(b)))
#define _B15(b,x) (((x)&0x7FFF)<<(31-(b)))
#define _B16(b,x) (((x)&0xFFFF)<<(31-(b)))
#define _B17(b,x) (((x)&0x1FFFF)<<(31-(b)))
#define _B18(b,x) (((x)&0x3FFFF)<<(31-(b)))
#define _B19(b,x) (((x)&0x7FFFF)<<(31-(b)))
#define _B20(b,x) (((x)&0xFFFFF)<<(31-(b)))
#define _B21(b,x) (((x)&0x1FFFFF)<<(31-(b)))
#define _B22(b,x) (((x)&0x3FFFFF)<<(31-(b)))
#define _B23(b,x) (((x)&0x7FFFFF)<<(31-(b)))
#define _B24(b,x) (((x)&0xFFFFFF)<<(31-(b)))
#define _B25(b,x) (((x)&0x1FFFFFF)<<(31-(b)))
#define _B26(b,x) (((x)&0x3FFFFFF)<<(31-(b)))
#define _B27(b,x) (((x)&0x7FFFFFF)<<(31-(b)))
#define _B28(b,x) (((x)&0xFFFFFFF)<<(31-(b)))
#define _B29(b,x) (((x)&0x1FFFFFFF)<<(31-(b)))
#define _B30(b,x) (((x)&0x3FFFFFFF)<<(31-(b)))
#define _B31(b,x) (((x)&0x7FFFFFFF)<<(31-(b)))

// These defines ease extraction of bitfields.  (Not useful in assembler code.)
//  x = 32 bit value
//  b = IBM bit number of least significant bit of field
//  when b is a const, compiler should generate a single rotate-and-mask instruction
#define _GN(x,b)  (((x)>>(31-(b)))&0x1)
#define _G2(x,b)  (((x)>>(31-(b)))&0x3)
#define _G3(x,b)  (((x)>>(31-(b)))&0x7)
#define _G4(x,b)  (((x)>>(31-(b)))&0xF)
#define _G5(x,b)  (((x)>>(31-(b)))&0x1F)
#define _G6(x,b)  (((x)>>(31-(b)))&0x3F)
#define _G7(x,b)  (((x)>>(31-(b)))&0x7F)
#define _G8(x,b)  (((x)>>(31-(b)))&0xFF)
#define _G9(x,b)  (((x)>>(31-(b)))&0x1FF)
#define _G10(x,b) (((x)>>(31-(b)))&0x3FF)
#define _G11(x,b) (((x)>>(31-(b)))&0x7FF)
#define _G12(x,b) (((x)>>(31-(b)))&0xFFF)
#define _G13(x,b) (((x)>>(31-(b)))&0x1FFF)
#define _G14(x,b) (((x)>>(31-(b)))&0x3FFF)
#define _G15(x,b) (((x)>>(31-(b)))&0x7FFF)
#define _G16(x,b) (((x)>>(31-(b)))&0xFFFF)
#define _G17(x,b) (((x)>>(31-(b)))&0x1FFFF)
#define _G18(x,b) (((x)>>(31-(b)))&0x3FFFF)
#define _G19(x,b) (((x)>>(31-(b)))&0x7FFFF)
#define _G20(x,b) (((x)>>(31-(b)))&0xFFFFF)
#define _G21(x,b) (((x)>>(31-(b)))&0x1FFFFF)
#define _G22(x,b) (((x)>>(31-(b)))&0x3FFFFF)
#define _G23(x,b) (((x)>>(31-(b)))&0x7FFFFF)
#define _G24(x,b) (((x)>>(31-(b)))&0xFFFFFF)
#define _G25(x,b) (((x)>>(31-(b)))&0x1FFFFFF)
#define _G26(x,b) (((x)>>(31-(b)))&0x3FFFFFF)
#define _G27(x,b) (((x)>>(31-(b)))&0x7FFFFFF)
#define _G28(x,b) (((x)>>(31-(b)))&0xFFFFFFF)
#define _G29(x,b) (((x)>>(31-(b)))&0x1FFFFFFF)
#define _G30(x,b) (((x)>>(31-(b)))&0x3FFFFFFF)
#define _G31(x,b) (((x)>>(31-(b)))&0x7FFFFFFF)

#if 0
#define _BGP_DCR_BIC        (0x000)                     // 0x000-0x1ff: BIC (includes MCCU functionality)
#define _BGP_DCR_BIC_END    (_BGP_DCR_BIC + 0x1FF)      // 0x1ff: BIC (includes MCCU functionality)

#define _BGP_DCR_SERDES     (0x200)                     // 0x200-0x3ff: Serdes Config
#define _BGP_DCR_SERDES_END (_BGP_DCR_SERDES + 0x1FF)   // 0x3ff: Serdes Config End

#define _BGP_DCR_TEST       (0x400)                     // 0x400-0x47f: Test Interface
#define _BGP_DCR_TEST_END   (_BGP_DCR_TEST + 0x07F)     // 0x400-0x47f: Test Interface End

#define _BGP_DCR_L30        (0x500)                     // 0x500-0x53f: L3-Cache 0
#define _BGP_DCR_L30_END    (_BGP_DCR_L30 + 0x03F)      // 0x53f: L3-Cache 0 End

#define _BGP_DCR_L31        (0x540)                     // 0x540-0x57f: L3-Cache 1
#define _BGP_DCR_L31_END    (_BGP_DCR_L31 + 0x03F)      // 0x57f: L3-Cache 1 End

#define _BGP_DCR_XAUI       (0x580)                     // 0x580-0x5bf: XAUI config
#define _BGP_DCR_XAUI_END   (_BGP_DCR_XAUI + 0x03F)     // 0x5bf: XAUI config End

#define _BGP_DCR_SRAM       (0x610)                     // 0x610-0x61f: SRAM unit (Includes Lockbox functionality)
#define _BGP_DCR_SRAM_END   (_BGP_DCR_SRAM + 0x00F)     // 0x61f: SRAM unit (Includes Lockbox functionality)

#define _BGP_DCR_DEVBUS     (0x620)                     // 0x620-0x62f: DevBus Arbiter
#define _BGP_DCR_DEVBUS_END (_BGP_DCR_DEVBUS + 0x00F)   // 0x62f: DevBus Arbiter End

#define _BGP_DCR_NETBUS     (0x630)                     // 0x630-0x63f: NetBus Arbiter
#define _BGP_DCR_NETBUS_END (_BGP_DCR_NETBUS + 0x00F)   // 0x63f: NetBus Arbiter End

#define _BGP_DCR_DMAARB     (0x640)                     // 0x640-0x64f: DMA arbiter (former PLB slave)
#define _BGP_DCR_DMAARB_END (_BGP_DCR_DMAARB + 0x00F)   // 0x64f: DMA arbiter (former PLB slave) End

#define _BGP_DCR_DCRARB     (0x650)                     // 0x650-0x65f: DCR arbiter
#define _BGP_DCR_DCRARB_END (_BGP_DCR_DCRARB + 0x00F)   // 0x65f: DCR arbiter End

#define _BGP_DCR_GLOBINT     (0x660)                    // 0x660-0x66F: Global Interrupts
#define _BGP_DCR_GLOBINT_END (_BGP_DCR_GLOBINT + 0x00F) // 0x66F: Global Interrupts End

#define _BGP_DCR_CLOCKSTOP     (0x670)                      // 0x670-0x67F: Clock Stop
#define _BGP_DCR_CLOCKSTOP_END (_BGP_DCR_CLOCKSTOP + 0x00F) // 0x67F: Clock Stop End

#define _BGP_DCR_ENVMON      (0x680)                    // 0x670-0x67F: Environmental Monitor
#define _BGP_DCR_ENVMON_END  (_BGP_DCR_ENVMON + 0x00F)  // 0x67F: Env Mon End

#define _BGP_DCR_FPU        (0x700)                     // 0x700-0x77f: Hummer3 00/01/10/11
#define _BGP_DCR_FPU_END    (_BGP_DCR_FPU + 0x07F)      // 0x77f: Hummer3 00/01/10/11 End

#define _BGP_DCR_L2         (0x780)                     // 0x780-0x7ff: L2-Cache 00/01/10/11
#define _BGP_DCR_L2_END     (_BGP_DCR_L2 + 0x07F)       // 0x7ff: L2-Cache 00/01/10/11 End

#define _BGP_DCR_SNOOP      (0x800)                     // 0x800-0xbff: Snoop 00/01/10/11
#define _BGP_DCR_SNOOP0     (0x800)                     // 0x800-0x8ff: Snoop 00
#define _BGP_DCR_SNOOP1     (0x900)                     // 0x900-0x9ff: Snoop 01
#define _BGP_DCR_SNOOP2     (0xA00)                     // 0xa00-0xaff: Snoop 10
#define _BGP_DCR_SNOOP3     (0xB00)                     // 0xb00-0xbff: Snoop 11
#define _BGP_DCR_SNOOP_END  (_BGP_DCR_SNOOP + 0x3FF)    // 0xbff: Snoop 00/01/10/11 End

#define _BGP_DCR_TREE       (0xc00)                     // 0xc00-0xc7f: Tree
#define _BGP_DCR_TREE_END   (_BGP_DCR_TREE + 0x07F)     // 0xc7f: Tree End

#define _BGP_DCR_TORUS      (0xc80)                     // 0xc80-0xcff: Torus
#define _BGP_DCR_TORUS_END  (_BGP_DCR_TORUS + 0x07F)    // 0xcff: Torus End

#define _BGP_DCR_DMA        (0xd00)                     // 0xd00-0xdff: DMA
#define _BGP_DCR_DMA_END    (_BGP_DCR_DMA + 0x0FF)      // 0xdff: DMA End

#define _BGP_DCR_DDR0       (0xe00)                     // 0xe00-0xeff: DDR controller 0
#define _BGP_DCR_DDR0_END   (_BGP_DCR_DDR0 + 0x0FF)     // 0xeff: DDR controller 0 End

#define _BGP_DCR_DDR1       (0xf00)                     // 0xf00-0xfff: DDR controller 1
#define _BGP_DCR_DDR1_END   (_BGP_DCR_DDR1 + 0x0FF)     // 0xfff: DDR controller 1 End

#endif

/*
 * Tree
 */

#define _BGP_TRx_DI      (0x00)    // Offset from Tree VCx for Data   Injection   (WO,Quad)
#define _BGP_TRx_HI      (0x10)    // Offset from Tree VCx for Header Injection   (WO,Word)
#define _BGP_TRx_DR      (0x20)    // Offset from Tree VCx for Data   Reception   (RO,Quad)
#define _BGP_TRx_HR      (0x30)    // Offset from Tree VCx for Header Reception   (RO,Word)
#define _BGP_TRx_Sx      (0x40)    // Offset from Tree VCx for Status             (RO,Word)
#define _BGP_TRx_SO      (0x50)    // Offset from Tree VCx for Status of Other VC (RO,Word)

// Virtual Addresses for Tree VC0
#define _BGP_TR0_DI    (_BGP_VA_TREE0 | _BGP_TRx_DI)
#define _BGP_TR0_HI    (_BGP_VA_TREE0 | _BGP_TRx_HI)
#define _BGP_TR0_DR    (_BGP_VA_TREE0 | _BGP_TRx_DR)
#define _BGP_TR0_HR    (_BGP_VA_TREE0 | _BGP_TRx_HR)
#define _BGP_TR0_S0    (_BGP_VA_TREE0 | _BGP_TRx_Sx)
#define _BGP_TR0_S1    (_BGP_VA_TREE0 | _BGP_TRx_SO)

// Virtual Addresses for Tree VC1
#define _BGP_TR1_DI    (_BGP_VA_TREE1 | _BGP_TRx_DI)
#define _BGP_TR1_HI    (_BGP_VA_TREE1 | _BGP_TRx_HI)
#define _BGP_TR1_DR    (_BGP_VA_TREE1 | _BGP_TRx_DR)
#define _BGP_TR1_HR    (_BGP_VA_TREE1 | _BGP_TRx_HR)
#define _BGP_TR1_S1    (_BGP_VA_TREE1 | _BGP_TRx_Sx)
#define _BGP_TR1_S0    (_BGP_VA_TREE1 | _BGP_TRx_SO)

// Packet Payload: fixed size for all Tree packets
#define _BGP_TREE_PKT_MAX_BYTES    (256)       // bytes in a tree packet
#define _BGP_TREE_PKT_MAX_SHORT    (128)
#define _BGP_TREE_PKT_MAX_LONG      (64)
#define _BGP_TREE_PKT_MAX_LONGLONG  (32)
#define _BGP_TREE_PKT_MAX_QUADS     (16)       // quads in a tree packet


// Packet header
#define  _BGP_TR_HDR_CLASS(x)           _B4( 3,x)      //  Packet class (virtual tree)
#define  _BGP_TR_HDR_P2P                _BN( 4)        //  Point-to-point enable
#define  _BGP_TR_HDR_IRQ                _BN( 5)        //  Interrupt request (at receiver) enable
#define  _BGP_TR_HDR_OPCODE(x)          _B3( 8,x)      //  ALU opcode
#define    _BGP_TR_OP_NONE                0x0          //    No operand.  Use for ordinary routed packets.
#define    _BGP_TR_OP_OR                  0x1          //    Bitwise logical OR.
#define    _BGP_TR_OP_AND                 0x2          //    Bitwise logical AND.
#define    _BGP_TR_OP_XOR                 0x3          //    Bitwise logical XOR.
#define    _BGP_TR_OP_MAX                 0x5          //    Unsigned integer maximum.
#define    _BGP_TR_OP_ADD                 0x6          //    Unsigned integer addition.
#define  _BGP_TR_HDR_OPSIZE(x)          _B7(15,x)      //  Operand size (# of 16-bit words minus 1)
#define  _BGP_TR_HDR_TAG(x)             _B14(29,x)     //  User-specified tag (for ordinary routed packets only)
#define  _BGP_TR_HDR_NADDR(x)           _B24(29,x)     //  Target address (for P2P packets only)
#define  _BGP_TR_HDR_CSUM(x)            _B2(31,x)      //  Injection checksum mode
#define    _BGP_TR_CSUM_NONE              0x0          //    Do not include packet in checksums.
#define    _BGP_TR_CSUM_SOME              0x1          //    Include header in header checksum.  Include all but
                                                       //     first quadword in payload checksum.
#define    _BGP_TR_CSUM_CFG               0x2          //    Include header in header checksum.  Include all but
                                                       //     specified number of 16-bit words in payload checksum.
#define    _BGP_TR_CSUM_ALL               0x3          //    Include entire packet in checksums.

// Packet status
#define  _BGP_TR_STAT_IPY_CNT(x)        _B8( 7,x)      //  Injection payload qword count
#define  _BGP_TR_STAT_IHD_CNT(x)        _B4(15,x)      //  Injection header word count
#define  _BGP_TR_STAT_RPY_CNT(x)        _B8(23,x)      //  Reception payload qword count
#define  _BGP_TR_STAT_IRQ               _BN(27)        //  One or more reception headers with IRQ bit set
#define  _BGP_TR_STAT_RHD_CNT(x)        _B4(31,x)      //  Reception header word count

// Tree Map of DCR Groupings
#define _BGP_DCR_TR_CLASS  (_BGP_DCR_TREE + 0x00)      // Class Definition Registers (R/W)
#define _BGP_DCR_TR_DMA    (_BGP_DCR_TREE + 0x0C)      // Network Port Diagnostic Memory Access Registers (R/W)
#define _BGP_DCR_TR_ARB    (_BGP_DCR_TREE + 0x10)      // Arbiter Control Registers (R/W)
#define _BGP_DCR_TR_CH0    (_BGP_DCR_TREE + 0x20)      // Channel 0 Control Registers (R/W)
#define _BGP_DCR_TR_CH1    (_BGP_DCR_TREE + 0x28)      // Channel 1 Control Registers (R/W)
#define _BGP_DCR_TR_CH2    (_BGP_DCR_TREE + 0x30)      // Channel 2 Control Registers (R/W)
#define _BGP_DCR_TR_GLOB   (_BGP_DCR_TREE + 0x40)      // Global Registers (R/W)
#define _BGP_DCR_TR_REC    (_BGP_DCR_TREE + 0x44)      // Processor Reception Registers (R/W)
#define _BGP_DCR_TR_INJ    (_BGP_DCR_TREE + 0x48)      // Processor Injection Registers (R/W)
#define _BGP_DCR_TR_LCRC   (_BGP_DCR_TREE + 0x50)      // Link CRC's
#define _BGP_DCR_TR_ERR    (_BGP_DCR_TREE + 0x60)      // Internal error counters


// Tree Class Registers
// Note: each route descriptor register contains two class descriptors.  "LO" will refer to the lower-numbered
//       of the two and "HI" will refer to the higher numbered.
#define _BGP_DCR_TR_CLASS_RDR0     (_BGP_DCR_TR_CLASS + 0x00)  // CLASS: Route Descriptor Register for classes 0,  1
#define _BGP_DCR_TR_CLASS_RDR1     (_BGP_DCR_TR_CLASS + 0x01)  // CLASS: Route Descriptor Register for classes 2,  3
#define _BGP_DCR_TR_CLASS_RDR2     (_BGP_DCR_TR_CLASS + 0x02)  // CLASS: Route Descriptor Register for classes 4,  5
#define _BGP_DCR_TR_CLASS_RDR3     (_BGP_DCR_TR_CLASS + 0x03)  // CLASS: Route Descriptor Register for classes 6,  7
#define _BGP_DCR_TR_CLASS_RDR4     (_BGP_DCR_TR_CLASS + 0x04)  // CLASS: Route Descriptor Register for classes 8,  9
#define _BGP_DCR_TR_CLASS_RDR5     (_BGP_DCR_TR_CLASS + 0x05)  // CLASS: Route Descriptor Register for classes 10, 11
#define _BGP_DCR_TR_CLASS_RDR6     (_BGP_DCR_TR_CLASS + 0x06)  // CLASS: Route Descriptor Register for classes 12, 13
#define _BGP_DCR_TR_CLASS_RDR7     (_BGP_DCR_TR_CLASS + 0x07)  // CLASS: Route Descriptor Register for classes 14, 15
#define  _TR_CLASS_RDR_LO_SRC2      _BN( 1)                    //  Class low,  source channel 2
#define  _TR_CLASS_RDR_LO_SRC1      _BN( 2)                    //  Class low,  source channel 1
#define  _TR_CLASS_RDR_LO_SRC0      _BN( 3)                    //  Class low,  source channel 0
#define  _TR_CLASS_RDR_LO_TGT2      _BN( 5)                    //  Class low,  target channel 2
#define  _TR_CLASS_RDR_LO_TGT1      _BN( 6)                    //  Class low,  target channel 1
#define  _TR_CLASS_RDR_LO_TGT0      _BN( 7)                    //  Class low,  target channel 0
#define  _TR_CLASS_RDR_LO_SRCL      _BN(14)                    //  Class low,  source local client (injection)
#define  _TR_CLASS_RDR_LO_TGTL      _BN(15)                    //  Class low,  target local client (reception)
#define  _TR_CLASS_RDR_HI_SRC2      _BN(17)                    //  Class high, source channel 2
#define  _TR_CLASS_RDR_HI_SRC1      _BN(18)                    //  Class high, source channel 1
#define  _TR_CLASS_RDR_HI_SRC0      _BN(19)                    //  Class high, source channel 0
#define  _TR_CLASS_RDR_HI_TGT2      _BN(21)                    //  Class high, target channel 2
#define  _TR_CLASS_RDR_HI_TGT1      _BN(22)                    //  Class high, target channel 1
#define  _TR_CLASS_RDR_HI_TGT0      _BN(23)                    //  Class high, target channel 0
#define  _TR_CLASS_RDR_HI_SRCL      _BN(30)                    //  Class high, source local client (injection)
#define  _TR_CLASS_RDR_HI_TGTL      _BN(31)                    //  Class high, target local client (reception)
#define _BGP_DCR_TR_CLASS_ISRA     (_BGP_DCR_TR_CLASS + 0x08)  // CLASS: Bits 0-31 of 64-bit idle pattern
#define _BGP_DCR_TR_CLASS_ISRB     (_BGP_DCR_TR_CLASS + 0x09)  // CLASS: Bits 32-63 of 64-bit idle pattern

// Tree Network Port Diagnostic Memory Access Registers
// Note: Diagnostic access to processor injection and reception fifos is through TR_REC and TR_INJ registers.
#define _BGP_DCR_TR_DMA_DMAA       (_BGP_DCR_TR_DMA + 0x00)   // DMA: Diagnostic SRAM address
#define  _TR_DMA_DMAA_TGT(x)        _B3(21,x)                 //  Target
#define   _TR_DMAA_TGT_RCV0           0x0                     //   Channel 0 receiver
#define   _TR_DMAA_TGT_RCV1           0x1                     //   Channel 1 receiver
#define   _TR_DMAA_TGT_RCV2           0x2                     //   Channel 2 receiver
#define   _TR_DMAA_TGT_SND0           0x4                     //   Channel 0 sender
#define   _TR_DMAA_TGT_SND1           0x5                     //   Channel 1 sender
#define   _TR_DMAA_TGT_SND2           0x6                     //   Channel 2 sender
#define  _TR_DMA_DMAA_VC(x)         _B1(22,x)                 //  Virtual channel
#define  _TR_DMA_DMAA_PCKT(x)       _B2(24,x)                 //  Packet number
#define  _TR_DMA_DMAA_WORD(x)       _B7(31,x)                 //  Word offset within packet
#define _BGP_DCR_TR_DMA_DMAD       (_BGP_DCR_TR_DMA + 0x01)   // DMA: Diagnostic SRAM data
#define _BGP_DCR_TR_DMA_DMADI      (_BGP_DCR_TR_DMA + 0x02)   // DMA: Diagnostic SRAM data with address increment
#define  _TR_DMA_DMAD_ECC(x)        _B6(15,x)                 //  ECC
#define  _TR_DMA_DMAD_DATA(x)       _B16(31,x)                //  Data
#define _BGP_DCR_TR_DMA_DMAH       (_BGP_DCR_TR_DMA + 0x03)   // DMA: Diagnostic header access

// Tree Arbiter Control Registers
#define _BGP_DCR_TR_ARB_RCFG       (_BGP_DCR_TR_ARB + 0x00)   // ARB: General router configuration
#define  _TR_ARB_RCFG_SRC00         _BN( 0)                   //  Disable source channel 0, VC0
#define  _TR_ARB_RCFG_SRC01         _BN( 1)                   //  Disable source channel 0, VC1
#define  _TR_ARB_RCFG_TGT00         _BN( 2)                   //  Disable target channel 0, VC0
#define  _TR_ARB_RCFG_TGT01         _BN( 3)                   //  Disable target channel 0, VC1
#define  _TR_ARB_RCFG_SRC10         _BN( 4)                   //  Disable source channel 1, VC0
#define  _TR_ARB_RCFG_SRC11         _BN( 5)                   //  Disable source channel 1, VC1
#define  _TR_ARB_RCFG_TGT10         _BN( 6)                   //  Disable target channel 1, VC0
#define  _TR_ARB_RCFG_TGT11         _BN( 7)                   //  Disable target channel 1, VC1
#define  _TR_ARB_RCFG_SRC20         _BN( 8)                   //  Disable source channel 2, VC0
#define  _TR_ARB_RCFG_SRC21         _BN( 9)                   //  Disable source channel 2, VC1
#define  _TR_ARB_RCFG_TGT20         _BN(10)                   //  Disable target channel 2, VC0
#define  _TR_ARB_RCFG_TGT21         _BN(11)                   //  Disable target channel 2, VC1
#define  _TR_ARB_RCFG_LB2           _BN(25)                   //  Channel 2 loopback enable
#define  _TR_ARB_RCFG_LB1           _BN(26)                   //  Channel 1 loopback enable
#define  _TR_ARB_RCFG_LB0           _BN(27)                   //  Channel 0 loopback enable
#define  _TR_ARB_RCFG_TOM(x)        _B2(29,x)                 //  Timeout mode
#define   _TR_RCFG_TOM_NONE           0x0                     //   Disable.
#define   _TR_RCFG_TOM_NRML           0x1                     //   Normal mode, irq enabled.
#define   _TR_RCFG_TOM_WD             0x2                     //   Watchdog mode, irq enabled.
#define  _TR_ARB_RCFG_MAN           _BN(30)                   //  Manual mode (router is disabled).
#define  _TR_ARB_RCFG_RST           _BN(31)                   //  Full arbiter reset.
#define _BGP_DCR_TR_ARB_RTO        (_BGP_DCR_TR_ARB + 0x01)   // ARB: 32 MSBs of router timeout value
#define _BGP_DCR_TR_ARB_RTIME      (_BGP_DCR_TR_ARB + 0x02)   // ARB: Value of router timeout counter
#define _BGP_DCR_TR_ARB_RSTAT      (_BGP_DCR_TR_ARB + 0x03)   // ARB: General router status
#define  _TR_ARB_RSTAT_REQ20        _BN( 0)                   //  Packet available in channel 2, VC0
#define  _TR_ARB_RSTAT_REQ10        _BN( 1)                   //  Packet available in channel 1, VC0
#define  _TR_ARB_RSTAT_REQ00        _BN( 2)                   //  Packet available in channel 0, VC0
#define  _TR_ARB_RSTAT_REQP0        _BN( 3)                   //  Packet available in local client, VC0
#define  _TR_ARB_RSTAT_REQ21        _BN( 4)                   //  Packet available in channel 2, VC1
#define  _TR_ARB_RSTAT_REQ11        _BN( 5)                   //  Packet available in channel 1, VC1
#define  _TR_ARB_RSTAT_REQ01        _BN( 6)                   //  Packet available in channel 0, VC1
#define  _TR_ARB_RSTAT_REQP1        _BN( 7)                   //  Packet available in local client, VC1
#define  _TR_ARB_RSTAT_FUL20        _BN( 8)                   //  Channel 2, VC0 is full
#define  _TR_ARB_RSTAT_FUL10        _BN( 9)                   //  Channel 1, VC0 is full
#define  _TR_ARB_RSTAT_FUL00        _BN(10)                   //  Channel 0, VC0 is full
#define  _TR_ARB_RSTAT_FULP0        _BN(11)                   //  Local client, VC0 is full
#define  _TR_ARB_RSTAT_FUL21        _BN(12)                   //  Channel 2, VC1 is full
#define  _TR_ARB_RSTAT_FUL11        _BN(13)                   //  Channel 1, VC1 is full
#define  _TR_ARB_RSTAT_FUL01        _BN(14)                   //  Channel 0, VC1 is full
#define  _TR_ARB_RSTAT_FULP1        _BN(15)                   //  Local client, VC1 is full
#define  _TR_ARB_RSTAT_MAT20        _BN(16)                   //  Channel 2, VC0 is mature
#define  _TR_ARB_RSTAT_MAT10        _BN(17)                   //  Channel 1, VC0 is mature
#define  _TR_ARB_RSTAT_MAT00        _BN(18)                   //  Channel 0, VC0 is mature
#define  _TR_ARB_RSTAT_MATP0        _BN(19)                   //  Local client, VC0 is mature
#define  _TR_ARB_RSTAT_MAT21        _BN(20)                   //  Channel 2, VC1 is mature
#define  _TR_ARB_RSTAT_MAT11        _BN(21)                   //  Channel 1, VC1 is mature
#define  _TR_ARB_RSTAT_MAT01        _BN(22)                   //  Channel 0, VC1 is mature
#define  _TR_ARB_RSTAT_MATP1        _BN(23)                   //  Local client, VC1 is mature
#define  _TR_ARB_RSTAT_BSY20        _BN(24)                   //  Channel 2, VC0 is busy
#define  _TR_ARB_RSTAT_BSY10        _BN(25)                   //  Channel 1, VC0 is busy
#define  _TR_ARB_RSTAT_BSY00        _BN(26)                   //  Channel 0, VC0 is busy
#define  _TR_ARB_RSTAT_BSYP0        _BN(27)                   //  Local client, VC0 is busy
#define  _TR_ARB_RSTAT_BSY21        _BN(28)                   //  Channel 2, VC1 is busy
#define  _TR_ARB_RSTAT_BSY11        _BN(29)                   //  Channel 1, VC1 is busy
#define  _TR_ARB_RSTAT_BSY01        _BN(30)                   //  Channel 0, VC1 is busy
#define  _TR_ARB_RSTAT_BSYP1        _BN(31)                   //  Local client, VC1 is busy
#define _BGP_DCR_TR_ARB_HD00       (_BGP_DCR_TR_ARB + 0x04)   // ARB: Next header, channel 0, VC0
#define _BGP_DCR_TR_ARB_HD01       (_BGP_DCR_TR_ARB + 0x05)   // ARB: Next header, channel 0, VC1
#define _BGP_DCR_TR_ARB_HD10       (_BGP_DCR_TR_ARB + 0x06)   // ARB: Next header, channel 1, VC0
#define _BGP_DCR_TR_ARB_HD11       (_BGP_DCR_TR_ARB + 0x07)   // ARB: Next header, channel 1, VC1
#define _BGP_DCR_TR_ARB_HD20       (_BGP_DCR_TR_ARB + 0x08)   // ARB: Next header, channel 2, VC0
#define _BGP_DCR_TR_ARB_HD21       (_BGP_DCR_TR_ARB + 0x09)   // ARB: Next header, channel 2, VC1
#define _BGP_DCR_TR_ARB_HDI0       (_BGP_DCR_TR_ARB + 0x0A)   // ARB: Next header, injection, VC0
#define _BGP_DCR_TR_ARB_HDI1       (_BGP_DCR_TR_ARB + 0x0B)   // ARB: Next header, injection, VC1
#define _BGP_DCR_TR_ARB_FORCEC     (_BGP_DCR_TR_ARB + 0x0C)   // ARB: Force control for manual mode
#define  _TR_ARB_FORCEC_CH0         _BN( 0)                   //  Channel 0 is a target
#define  _TR_ARB_FORCEC_CH1         _BN( 1)                   //  Channel 1 is a target
#define  _TR_ARB_FORCEC_CH2         _BN( 2)                   //  Channel 2 is a target
#define  _TR_ARB_FORCEC_P           _BN( 3)                   //  Local client is a target
#define  _TR_ARB_FORCEC_ALU         _BN( 4)                   //  ALU is a target
#define  _TR_ARB_FORCEC_RT          _BN( 5)                   //  Force route immediately
#define  _TR_ARB_FORCEC_STK         _BN( 6)                   //  Sticky route: always force route
#define _BGP_DCR_TR_ARB_FORCER     (_BGP_DCR_TR_ARB + 0x0D)   // ARB: Forced route for manual mode
#define  _TR_ARB_FORCER_CH20        _BN( 0)                   //  Channel 2 is a source for channel 0
#define  _TR_ARB_FORCER_CH10        _BN( 1)                   //  Channel 1 is a source for channel 0
#define  _TR_ARB_FORCER_CH00        _BN( 2)                   //  Channel 0 is a source for channel 0
#define  _TR_ARB_FORCER_CHP0        _BN( 3)                   //  Local client is a source for channel 0
#define  _TR_ARB_FORCER_CHA0        _BN( 4)                   //  ALU is a source for channel 0
#define  _TR_ARB_FORCER_VC0         _BN( 5)                   //  VC that is source for channel 0
#define  _TR_ARB_FORCER_CH21        _BN( 6)                   //  Channel 2 is a source for channel 1
#define  _TR_ARB_FORCER_CH11        _BN( 7)                   //  Channel 1 is a source for channel 1
#define  _TR_ARB_FORCER_CH01        _BN( 8)                   //  Channel 0 is a source for channel 1
#define  _TR_ARB_FORCER_CHP1        _BN( 9)                   //  Local client is a source for channel 1
#define  _TR_ARB_FORCER_CHA1        _BN(10)                   //  ALU is a source for channel 1
#define  _TR_ARB_FORCER_VC1         _BN(11)                   //  VC that is source for channel 1
#define  _TR_ARB_FORCER_CH22        _BN(12)                   //  Channel 2 is a source for channel 2
#define  _TR_ARB_FORCER_CH12        _BN(13)                   //  Channel 1 is a source for channel 2
#define  _TR_ARB_FORCER_CH02        _BN(14)                   //  Channel 0 is a source for channel 2
#define  _TR_ARB_FORCER_CHP2        _BN(15)                   //  Local client is a source for channel 2
#define  _TR_ARB_FORCER_CHA2        _BN(16)                   //  ALU is a source for channel 2
#define  _TR_ARB_FORCER_VC2         _BN(17)                   //  VC that is source for channel 2
#define  _TR_ARB_FORCER_CH2P        _BN(18)                   //  Channel 2 is a source for local client
#define  _TR_ARB_FORCER_CH1P        _BN(19)                   //  Channel 1 is a source for local client
#define  _TR_ARB_FORCER_CH0P        _BN(20)                   //  Channel 0 is a source for local client
#define  _TR_ARB_FORCER_CHPP        _BN(21)                   //  Local client is a source for local client
#define  _TR_ARB_FORCER_CHAP        _BN(22)                   //  ALU is a source for local client
#define  _TR_ARB_FORCER_VCP         _BN(23)                   //  VC that is source for local client
#define  _TR_ARB_FORCER_CH2A        _BN(24)                   //  Channel 2 is a source for ALU
#define  _TR_ARB_FORCER_CH1A        _BN(25)                   //  Channel 1 is a source for ALU
#define  _TR_ARB_FORCER_CH0A        _BN(26)                   //  Channel 0 is a source for ALU
#define  _TR_ARB_FORCER_CHPA        _BN(27)                   //  Local client is a source for ALU
#define  _TR_ARB_FORCER_CHAA        _BN(28)                   //  ALU is a source for ALU
#define  _TR_ARB_FORCER_VCA         _BN(29)                   //  VC that is source for ALU
#define _BGP_DCR_TR_ARB_FORCEH     (_BGP_DCR_TR_ARB + 0x0E)   // ARB: Forced header for manual mode
#define _BGP_DCR_TR_ARB_XSTAT      (_BGP_DCR_TR_ARB + 0x0F)   // ARB: Extended router status
#define  _TR_ARB_XSTAT_BLK20        _BN( 0)                   //  Request from channel 2, VC0 is blocked
#define  _TR_ARB_XSTAT_BLK10        _BN( 1)                   //  Request from channel 1, VC0 is blocked
#define  _TR_ARB_XSTAT_BLK00        _BN( 2)                   //  Request from channel 0, VC0 is blocked
#define  _TR_ARB_XSTAT_BLKP0        _BN( 3)                   //  Request from local client, VC0 is blocked
#define  _TR_ARB_XSTAT_BLK21        _BN( 4)                   //  Request from channel 2, VC1 is blocked
#define  _TR_ARB_XSTAT_BLK11        _BN( 5)                   //  Request from channel 1, VC1 is blocked
#define  _TR_ARB_XSTAT_BLK01        _BN( 6)                   //  Request from channel 0, VC1 is blocked
#define  _TR_ARB_XSTAT_BLKP1        _BN( 7)                   //  Request from local client, VC1 is blocked
#define  _TR_ARB_XSTAT_BSYR2        _BN( 8)                   //  Channel 2 receiver is busy
#define  _TR_ARB_XSTAT_BSYR1        _BN( 9)                   //  Channel 1 receiver is busy
#define  _TR_ARB_XSTAT_BSYR0        _BN(10)                   //  Channel 0 receiver is busy
#define  _TR_ARB_XSTAT_BSYPI        _BN(11)                   //  Local client injection is busy
#define  _TR_ARB_XSTAT_BSYA         _BN(12)                   //  ALU is busy
#define  _TR_ARB_XSTAT_BSYS2        _BN(13)                   //  Channel 2 sender is busy
#define  _TR_ARB_XSTAT_BSYS1        _BN(14)                   //  Channel 1 sender is busy
#define  _TR_ARB_XSTAT_BSYS0        _BN(15)                   //  Channel 0 sender is busy
#define  _TR_ARB_XSTAT_BSYPR        _BN(16)                   //  Local client reception is busy
#define  _TR_ARB_XSTAT_ARB_TO(x)    _B15(31,x)                //  Greedy-Arbitration timeout

// Tree Channel 0 Control Registers
#define _BGP_DCR_TR_CH0_RSTAT      (_BGP_DCR_TR_CH0 + 0x00)   // CH0: Receiver status
#define  _TR_RSTAT_RCVERR           _BN( 0)                   //  Receiver error
#define  _TR_RSTAT_LHEXP            _BN( 1)                   //  Expect link header
#define  _TR_RSTAT_PH0EXP           _BN( 2)                   //  Expect packet header 0
#define  _TR_RSTAT_PH1EXP           _BN( 3)                   //  Expect packet header 1
#define  _TR_RSTAT_PDRCV            _BN( 4)                   //  Receive packet data
#define  _TR_RSTAT_CWEXP            _BN( 5)                   //  Expect packet control word
#define  _TR_RSTAT_CSEXP            _BN( 6)                   //  Expect packet checksum
#define  _TR_RSTAT_SCRBRD0          _B8(14,0xff)              //  VC0 fifo scoreboard
#define  _TR_RSTAT_SCRBRD1          _B8(22,0xff)              //  VC1 fifo scoreboard
#define  _TR_RSTAT_RMTSTAT          _B9(31,0x1ff)             //  Remote status
#define _BGP_DCR_TR_CH0_RCTRL      (_BGP_DCR_TR_CH0 + 0x01)   // CH0: Receiver control
#define  _TR_RCTRL_FERR             _BN( 0)                   //  Force receiver into error state
#define  _TR_RCTRL_RST              _BN( 1)                   //  Reset all internal pointers
#define  _TR_RCTRL_FRZ0             _BN( 2)                   //  Freeze VC0
#define  _TR_RCTRL_FRZ1             _BN( 3)                   //  Freeze VC1
#define  _TR_RCTRL_RCVALL           _BN( 4)                   //  Disable receiver CRC check and accept all packets
#define _BGP_DCR_TR_CH0_SSTAT      (_BGP_DCR_TR_CH0 + 0x02)   // CH0: Sender status
#define  _TR_SSTAT_SYNC             _BN( 0)                   //  Phase of sender
#define  _TR_SSTAT_ARB              _BN( 1)                   //  Arbitrating
#define  _TR_SSTAT_PH0SND           _BN( 2)                   //  Sending packet header 0
#define  _TR_SSTAT_PH1SND           _BN( 3)                   //  Sending packet header 1
#define  _TR_SSTAT_PDSND            _BN( 4)                   //  Sending packet payload
#define  _TR_SSTAT_CWSND            _BN( 5)                   //  Sending packet control word
#define  _TR_SSTAT_CSSND            _BN( 6)                   //  Sending packet checksum
#define  _TR_SSTAT_IDLSND           _BN( 7)                   //  Sending idle packet
#define  _TR_SSTAT_RPTR0            _B3(10,0x7)               //  VC0 read pointer
#define  _TR_SSTAT_WPTR0            _B3(13,0x7)               //  VC0 write pointer
#define  _TR_SSTAT_RPTR1            _B3(16,0x7)               //  VC1 read pointer
#define  _TR_SSTAT_WPTR1            _B3(19,0x7)               //  VC1 write pointer
#define _BGP_DCR_TR_CH0_SCTRL      (_BGP_DCR_TR_CH0 + 0x03)   // CH0: Sender control
#define  _TR_SCTRL_SYNC             _BN( 0)                   //  Force sender to send SYNC
#define  _TR_SCTRL_IDLE             _BN( 1)                   //  Force sender to send IDLE
#define  _TR_SCTRL_RST              _BN( 2)                   //  Reset all internal pointers
#define  _TR_SCTRL_INVMSB           _BN( 3)                   //  Invert MSB of class for loopback packets
#define  _TR_SCTRL_OFF              _BN( 4)                   //  Disable (black hole) the sender
#define _BGP_DCR_TR_CH0_TNACK      (_BGP_DCR_TR_CH0 + 0x04)   // CH0: Tolerated dalay from NACK to ACK status
#define _BGP_DCR_TR_CH0_CNACK      (_BGP_DCR_TR_CH0 + 0x05)   // CH0: Time since last NACK received
#define _BGP_DCR_TR_CH0_TIDLE      (_BGP_DCR_TR_CH0 + 0x06)   // CH0: Frequency to send IDLE packets
#define _BGP_DCR_TR_CH0_CIDLE      (_BGP_DCR_TR_CH0 + 0x07)   // CH0: Time since last IDLE sent

// Tree Channel 1 Control Registers
// Note: Register definitions are the same as those of channel 0.
#define _BGP_DCR_TR_CH1_RSTAT      (_BGP_DCR_TR_CH1 + 0x00)   // CH1: Receiver status
#define _BGP_DCR_TR_CH1_RCTRL      (_BGP_DCR_TR_CH1 + 0x01)   // CH1: Receiver control
#define _BGP_DCR_TR_CH1_SSTAT      (_BGP_DCR_TR_CH1 + 0x02)   // CH1: Sender status
#define _BGP_DCR_TR_CH1_SCTRL      (_BGP_DCR_TR_CH1 + 0x03)   // CH1: Sender control
#define _BGP_DCR_TR_CH1_TNACK      (_BGP_DCR_TR_CH1 + 0x04)   // CH1: Tolerated dalay from NACK to ACK status
#define _BGP_DCR_TR_CH1_CNACK      (_BGP_DCR_TR_CH1 + 0x05)   // CH1: Time since last NACK received
#define _BGP_DCR_TR_CH1_TIDLE      (_BGP_DCR_TR_CH1 + 0x06)   // CH1: Frequency to send IDLE packets
#define _BGP_DCR_TR_CH1_CIDLE      (_BGP_DCR_TR_CH1 + 0x07)   // CH1: Time since last IDLE sent

// Tree Channel 2 Control Registers
// Note: Register definitions are the same as those of channel 0.
#define _BGP_DCR_TR_CH2_RSTAT      (_BGP_DCR_TR_CH2 + 0x00)   // CH2: Receiver status
#define _BGP_DCR_TR_CH2_RCTRL      (_BGP_DCR_TR_CH2 + 0x01)   // CH2: Receiver control
#define _BGP_DCR_TR_CH2_SSTAT      (_BGP_DCR_TR_CH2 + 0x02)   // CH2: Sender status
#define _BGP_DCR_TR_CH2_SCTRL      (_BGP_DCR_TR_CH2 + 0x03)   // CH2: Sender control
#define _BGP_DCR_TR_CH2_TNACK      (_BGP_DCR_TR_CH2 + 0x04)   // CH2: Tolerated dalay from NACK to ACK status
#define _BGP_DCR_TR_CH2_CNACK      (_BGP_DCR_TR_CH2 + 0x05)   // CH2: Time since last NACK received
#define _BGP_DCR_TR_CH2_TIDLE      (_BGP_DCR_TR_CH2 + 0x06)   // CH2: Frequency to send IDLE packets
#define _BGP_DCR_TR_CH2_CIDLE      (_BGP_DCR_TR_CH2 + 0x07)   // CH2: Time since last IDLE sent

// Tree Global Registers
#define _BGP_DCR_TR_GLOB_FPTR      (_BGP_DCR_TR_GLOB + 0x00)  // GLOB: Fifo Pointer Register
#define  _TR_GLOB_FPTR_IPY0(x)      _B3( 3,x)                 //  VC0 injection payload FIFO packet write pointer
#define  _TR_GLOB_FPTR_IHD0(x)      _B3( 7,x)                 //  VC0 injection header  FIFO packet write pointer
#define  _TR_GLOB_FPTR_IPY1(x)      _B3(11,x)                 //  VC1 injection payload FIFO packet write pointer
#define  _TR_GLOB_FPTR_IHD1(x)      _B3(15,x)                 //  VC1 injection header  FIFO packet write pointer
#define  _TR_GLOB_FPTR_RPY0(x)      _B3(19,x)                 //  VC0 reception payload FIFO packet read  pointer
#define  _TR_GLOB_FPTR_RHD0(x)      _B3(23,x)                 //  VC0 reception header  FIFO packet read  pointer
#define  _TR_GLOB_FPTR_RPY1(x)      _B3(27,x)                 //  VC1 reception payload FIFO packet read  pointer
#define  _TR_GLOB_FPTR_RHD1(x)      _B3(31,x)                 //  VC1 reception header  FIFO packet read  pointer
#define _BGP_DCR_TR_GLOB_NADDR     (_BGP_DCR_TR_GLOB + 0x01)  // GLOB: Node Address Register
#define  _TR_GLOB_NADDR(x)          _B24(31,x)                //  Node address
#define _BGP_DCR_TR_GLOB_VCFG0     (_BGP_DCR_TR_GLOB + 0x02)  // GLOB: VC0 Configuration Register (use macros below)
#define _BGP_DCR_TR_GLOB_VCFG1     (_BGP_DCR_TR_GLOB + 0x03)  // GLOB: VC1 Configuration Register
#define  _TR_GLOB_VCFG_RCVALL       _BN( 0)                   //  Disable P2P reception filering
#define  _TR_GLOB_VCFG_CSUMX(x)     _B8(15,x)                 //  Injection checksum mode 2 exclusion
#define  _TR_GLOB_VCFG_RWM(x)       _B3(23,x)                 //  Payload reception FIFO watermark
#define  _TR_GLOB_VCFG_IWM(x)       _B3(31,x)                 //  Payload injection FIFO watermark

// Tree Processor Reception Registers
#define _BGP_DCR_TR_REC_PRXF       (_BGP_DCR_TR_REC + 0x00)   // REC: Receive Exception Flag Register
#define _BGP_DCR_TR_REC_PRXEN      (_BGP_DCR_TR_REC + 0x01)   // REC: Receive Exception Enable Register
#define  _TR_REC_PRX_APAR0          _BN( 8)                   //  P0 address parity error
#define  _TR_REC_PRX_APAR1          _BN( 9)                   //  P1 address parity error
#define  _TR_REC_PRX_ALIGN0         _BN(10)                   //  P0 address alignment error
#define  _TR_REC_PRX_ALIGN1         _BN(11)                   //  P1 address alignment error
#define  _TR_REC_PRX_ADDR0          _BN(12)                   //  P0 bad (unrecognized) address error
#define  _TR_REC_PRX_ADDR1          _BN(13)                   //  P1 bad (unrecognized) address error
#define  _TR_REC_PRX_COLL           _BN(14)                   //  FIFO read collision error
#define  _TR_REC_PRX_UE             _BN(15)                   //  Uncorrectable SRAM ECC error
#define  _TR_REC_PRX_PFU0           _BN(26)                   //  VC0 payload FIFO under-run error
#define  _TR_REC_PRX_PFU1           _BN(27)                   //  VC1 payload FIFO under-run error
#define  _TR_REC_PRX_HFU0           _BN(28)                   //  VC0 header FIFO under-run error
#define  _TR_REC_PRX_HFU1           _BN(29)                   //  VC1 header FIFO under-run error
#define  _TR_REC_PRX_WM0            _BN(30)                   //  VC0 payload FIFO above watermark
#define  _TR_REC_PRX_WM1            _BN(31)                   //  VC1 payload FIFO above watermark
#define _BGP_DCR_TR_REC_PRDA       (_BGP_DCR_TR_REC + 0x02)   // REC: Receive Diagnostic Address Register
#define  _TR_PRDA_VC(x)             _B1(21,x)                 //  Select VC to access
#define  _TR_PRDA_MAC(x)            _B1(22,x)                 //  Select SRAM macro to access
#define  _TR_PRDA_LINE(x)           _B7(29,x)                 //  Select line in SRAM or RA
#define  _TR_PRDA_TGT(x)            _B2(31,x)                 //  Select target sub-line or RA
#define   _TR_PRDA_TGT_LO             0x0                     //   Least significant word of SRAM
#define   _TR_PRDA_TGT_HI             0x1                     //   Most significant word of SRAM
#define   _TR_PRDA_TGT_ECC            0x2                     //   ECC syndrome of SRAM
#define   _TR_PRDA_TGT_HDR            0x3                     //   Header fifo
#define _BGP_DCR_TR_REC_PRDD       (_BGP_DCR_TR_REC + 0x03)   // REC: Receive Diagnostic Data Register
#define  _TR_PRDD_ECC(x)            _B8(31,x)                 //  ECC
#define  _TR_PRDD_DATA(x)           (x)                       //  Data

// Tree Processor Injection Registers
#define _BGP_DCR_TR_INJ_PIXF       (_BGP_DCR_TR_INJ + 0x00)   // INJ: Injection Exception Flag Register
#define _BGP_DCR_TR_INJ_PIXEN      (_BGP_DCR_TR_INJ + 0x01)   // INJ: Injection Exception Enable Register
#define  _TR_INJ_PIX_APAR0          _BN( 6)                   //  P0 address parity error
#define  _TR_INJ_PIX_APAR1          _BN( 7)                   //  P1 address parity error
#define  _TR_INJ_PIX_ALIGN0         _BN( 8)                   //  P0 address alignment error
#define  _TR_INJ_PIX_ALIGN1         _BN( 9)                   //  P1 address alignment error
#define  _TR_INJ_PIX_ADDR0          _BN(10)                   //  P0 bad (unrecognized) address error
#define  _TR_INJ_PIX_ADDR1          _BN(11)                   //  P1 bad (unrecognized) address error
#define  _TR_INJ_PIX_DPAR0          _BN(12)                   //  P0 data parity error
#define  _TR_INJ_PIX_DPAR1          _BN(13)                   //  P1 data parity error
#define  _TR_INJ_PIX_COLL           _BN(14)                   //  FIFO write collision error
#define  _TR_INJ_PIX_UE             _BN(15)                   //  Uncorrectable SRAM ECC error
#define  _TR_INJ_PIX_PFO0           _BN(25)                   //  VC0 payload FIFO overflow error
#define  _TR_INJ_PIX_PFO1           _BN(26)                   //  VC1 payload FIFO overflow error
#define  _TR_INJ_PIX_HFO0           _BN(27)                   //  VC0 header FIFO overflow error
#define  _TR_INJ_PIX_HFO1           _BN(28)                   //  VC1 header FIFO overflow error
#define  _TR_INJ_PIX_WM0            _BN(29)                   //  VC0 payload FIFO at or below watermark
#define  _TR_INJ_PIX_WM1            _BN(30)                   //  VC1 payload FIFO at or below watermark
#define  _TR_INJ_PIX_ENABLE         _BN(31)                   //  Injection interface enable (if enabled in PIXEN)
#define _BGP_DCR_TR_INJ_PIDA       (_BGP_DCR_TR_INJ + 0x02)   // INJ: Injection Diagnostic Address Register
//        Use _TR_PRDA_* defined above.
#define _BGP_DCR_TR_INJ_PIDD       (_BGP_DCR_TR_INJ + 0x03)   // INJ: Injection Diagnostic Data Register
//        Use _TR_PRDD_* defined above.
#define _BGP_DCR_TR_INJ_CSPY0      (_BGP_DCR_TR_INJ + 0x04)   // INJ: VC0 payload checksum
#define _BGP_DCR_TR_INJ_CSHD0      (_BGP_DCR_TR_INJ + 0x05)   // INJ: VC0 header checksum
#define _BGP_DCR_TR_INJ_CSPY1      (_BGP_DCR_TR_INJ + 0x06)   // INJ: VC1 payload checksum
#define _BGP_DCR_TR_INJ_CSHD1      (_BGP_DCR_TR_INJ + 0x07)   // INJ: VC1 header checksum


// Link CRC's for the receivers 0..2 (vc0,1)
#define _BGP_DCR_TR_LCRC_R00  (_BGP_DCR_TR_LCRC + 0)
#define _BGP_DCR_TR_LCRC_R01  (_BGP_DCR_TR_LCRC + 1)
#define _BGP_DCR_TR_LCRC_R10  (_BGP_DCR_TR_LCRC + 2)
#define _BGP_DCR_TR_LCRC_R11  (_BGP_DCR_TR_LCRC + 3)
#define _BGP_DCR_TR_LCRC_R20  (_BGP_DCR_TR_LCRC + 4)
#define _BGP_DCR_TR_LCRC_R21  (_BGP_DCR_TR_LCRC + 5)

// Link CRC'c for the senders 0..2 (vc0,1)
#define _BGP_DCR_TR_LCRC_S00  (_BGP_DCR_TR_LCRC + 8)
#define _BGP_DCR_TR_LCRC_S01  (_BGP_DCR_TR_LCRC + 9)
#define _BGP_DCR_TR_LCRC_S10  (_BGP_DCR_TR_LCRC + 10)
#define _BGP_DCR_TR_LCRC_S11  (_BGP_DCR_TR_LCRC + 11)
#define _BGP_DCR_TR_LCRC_S20  (_BGP_DCR_TR_LCRC + 12)
#define _BGP_DCR_TR_LCRC_S21  (_BGP_DCR_TR_LCRC + 13)

// Internal error counters and thresholds
#define _BGP_DCR_TR_ERR_R0_CRC   (_BGP_DCR_TR_ERR + 0x00)    // CH0: Receiver link CRC errors detected
#define _BGP_DCR_TR_ERR_R0_CE    (_BGP_DCR_TR_ERR + 0x01)    // CH0: Receiver SRAM errors corrected
#define _BGP_DCR_TR_ERR_S0_RETRY (_BGP_DCR_TR_ERR + 0x02)    // CH0: Sender link retransmissions
#define _BGP_DCR_TR_ERR_S0_CE    (_BGP_DCR_TR_ERR + 0x03)    // CH0: Sender SRAM errors corrected
#define _BGP_DCR_TR_ERR_R1_CRC   (_BGP_DCR_TR_ERR + 0x04)    // CH1: Receiver link CRC errors detected
#define _BGP_DCR_TR_ERR_R1_CE    (_BGP_DCR_TR_ERR + 0x05)    // CH1: Receiver SRAM errors corrected
#define _BGP_DCR_TR_ERR_S1_RETRY (_BGP_DCR_TR_ERR + 0x06)    // CH1: Sender link retransmissions
#define _BGP_DCR_TR_ERR_S1_CE    (_BGP_DCR_TR_ERR + 0x07)    // CH1: Sender SRAM errors corrected
#define _BGP_DCR_TR_ERR_R2_CRC   (_BGP_DCR_TR_ERR + 0x08)    // CH2: Receiver link CRC errors detected
#define _BGP_DCR_TR_ERR_R2_CE    (_BGP_DCR_TR_ERR + 0x09)    // CH2: Receiver SRAM errors corrected
#define _BGP_DCR_TR_ERR_S2_RETRY (_BGP_DCR_TR_ERR + 0x0A)    // CH2: Sender link retransmissions
#define _BGP_DCR_TR_ERR_S2_CE    (_BGP_DCR_TR_ERR + 0x0B)    // CH2: Sender SRAM errors corrected
#define _BGP_DCR_TR_ERR_INJ_SE   (_BGP_DCR_TR_ERR + 0x0C)    // INJ: SRAM errors (correctable and uncorrectable)
#define _BGP_DCR_TR_ERR_REC_SE   (_BGP_DCR_TR_ERR + 0x0D)    // REC: SRAM errors (correctable and uncorrectable)

#define _BGP_DCR_TR_ERR_R0_CRC_T   (_BGP_DCR_TR_ERR + 0x10)  // Interrupt thresholds for corresponding error
#define _BGP_DCR_TR_ERR_R0_CE_T    (_BGP_DCR_TR_ERR + 0x11)  // counters.
#define _BGP_DCR_TR_ERR_S0_RETRY_T (_BGP_DCR_TR_ERR + 0x12)
#define _BGP_DCR_TR_ERR_S0_CE_T    (_BGP_DCR_TR_ERR + 0x13)
#define _BGP_DCR_TR_ERR_R1_CRC_T   (_BGP_DCR_TR_ERR + 0x14)
#define _BGP_DCR_TR_ERR_R1_CE_T    (_BGP_DCR_TR_ERR + 0x15)
#define _BGP_DCR_TR_ERR_S1_RETRY_T (_BGP_DCR_TR_ERR + 0x16)
#define _BGP_DCR_TR_ERR_S1_CE_T    (_BGP_DCR_TR_ERR + 0x17)
#define _BGP_DCR_TR_ERR_R2_CRC_T   (_BGP_DCR_TR_ERR + 0x18)
#define _BGP_DCR_TR_ERR_R2_CE_T    (_BGP_DCR_TR_ERR + 0x19)
#define _BGP_DCR_TR_ERR_S2_RETRY_T (_BGP_DCR_TR_ERR + 0x1A)
#define _BGP_DCR_TR_ERR_S2_CE_T    (_BGP_DCR_TR_ERR + 0x1B)
#define _BGP_DCR_TR_ERR_INJ_SE_T   (_BGP_DCR_TR_ERR + 0x1C)
#define _BGP_DCR_TR_ERR_REC_SE_T   (_BGP_DCR_TR_ERR + 0x1D)

// For _bgp_tree_configure_class
#define _BGP_TREE_RDR_NUM      (16)  // classes are 0..15

// The following interface allows for fine-grain control of the RDR register
// contents.  Use bit-wize OR'd together to create a route specification.
#define _BGP_TREE_RDR_SRC0    (0x1000)  // Bit Number  3 (MSb is bit number 0)
#define _BGP_TREE_RDR_SRC1    (0x2000)  // Bit Number  2
#define _BGP_TREE_RDR_SRC2    (0x4000)  // Bit Number  1
#define _BGP_TREE_RDR_SRCL    (0x0002)  // Bit Number 14
#define _BGP_TREE_RDR_TGT0    (0x0100)  // Bit Number  7
#define _BGP_TREE_RDR_TGT1    (0x0200)  // Bit Number  6
#define _BGP_TREE_RDR_TGT2    (0x0400)  // Bit Number  5
#define _BGP_TREE_RDR_TGTL    (0x0001)  // Bit Number 15

// OR of all valid Source and Target bits for SrtTgtEnable validation.
#define _BGP_TREE_RDR_ACCEPT (0x7703)














/**********************************************************************
 *
 * Torus
 *
 **********************************************************************/

#define _BGP_DCR_DMA_NUM_VALID_ADDR       8             // g range
#define _BGP_DCR_iDMA_NUM_TS_FIFO_WM      2             // j range
#define _BGP_DCR_rDMA_NUM_TS_FIFO_WM      4             // p range
#define _BGP_DCR_iDMA_NUM_FIFO_REGS       4             // i range
#define _BGP_DCR_iDMA_NUM_FIFO_MAP_REGS   32            // k range


//use g for repeated 8X, i repeated 4x, j repeated 2X, k repeated 32x, p repeated 4x

// -------------------
// ---- Controls -----
// -------------------

#define _BGP_DCR_DMA_RESET             (_BGP_DCR_DMA+0x00) // All bits reset to 1.
#define  _DMA_RESET_DCR                 _BN( 0)            // Reset the DMA's DCR unit
#define  _DMA_RESET_PQUE                _BN( 1)            // Reset the DMA's Processor Queue unit
#define  _DMA_RESET_IMFU                _BN( 2)            // Reset the DMA's Injection Memory Fifo/Counter Unit
#define  _DMA_RESET_RMFU                _BN( 3)            // Reset the DMA's Reception Memory Fifo/Counter Unit
#define  _DMA_RESET_LF                  _BN( 4)            // Reset the DMA's Local Fifo
#define  _DMA_RESET_ITIU                _BN( 5)            // Reset the DMA's Injection Torus Interface Unit
#define  _DMA_RESET_ICONU               _BN( 6)            // Reset the DMA's Injection Transfer Control Unit
#define  _DMA_RESET_IDAU                _BN( 7)            // Reset the DMA's Injection Data Alignment Unit
#define  _DMA_RESET_IMIU                _BN( 8)            // Reset the DMA's Injection L3 Memory Interface Unit
#define  _DMA_RESET_RTIU                _BN( 9)            // Reset the DMA's Reception Torus Interface Unit
#define  _DMA_RESET_RCONU               _BN(10)            // Reset the DMA's Reception Transfer Control Unit
#define  _DMA_RESET_RDAU                _BN(11)            // Reset the DMA's Reception Data Alignment Unit
#define  _DMA_RESET_RMIU                _BN(12)            // Reset the DMA's Reception L3 Memory Interface Unit
#define  _DMA_RESET_PF                  _BN(13)            // Reset the DMA's Torus Prefetch Unit
                                                           //  14-30 reserved.
#define  _DMA_RESET_LNKCHK              _BN(31)            // Reset the DMA's Torus Link Packet Capture Unit

#define _BGP_DCR_DMA_BASE_CONTROL      (_BGP_DCR_DMA+0x01)
#define   _DMA_BASE_CONTROL_USE_DMA     _BN( 0)            // Use DMA and *not* the Torus if 1, reset state is 0.
#define   _DMA_BASE_CONTROL_STORE_HDR   _BN( 1)            // Store DMA Headers in Reception Header Fifo (debugging)
#define   _DMA_BASE_CONTROL_PF_DIS      _BN( 2)            // Disable Torus Prefetch Unit (should be 0)
#define   _DMA_BASE_CONTROL_L3BURST_EN  _BN( 3)            // Enable L3 Burst when 1 (should be enabled, except for debugging)
#define   _DMA_BASE_CONTROL_ITIU_EN     _BN( 4)            // Enable Torus Injection Data Transfer Unit (never make this zero)
#define   _DMA_BASE_CONTROL_RTIU_EN     _BN( 5)            // Enable Torus Reception Data Transfer Unit
#define   _DMA_BASE_CONTROL_IMFU_EN     _BN( 6)            // Enable DMA Injection Fifo Unit Arbiter
#define   _DMA_BASE_CONTROL_RMFU_EN     _BN( 7)            // Enable DMA Reception fifo Unit Arbiter
#define   _DMA_BASE_CONTROL_L3PF_DIS    _BN( 8)            // Disable L3 Read Prefetch (should be 0)
                                                           //  9..27 reserved.
#define   _DMA_BASE_CONTROL_REC_FIFO_FULL_STOP_RDMA   _BN( 28) // DD2 Only, ECO 777, RDMA stops when fifo is full
#define   _DMA_BASE_CONTROL_REC_FIFO_CROSSTHRESH_NOTSTICKY  _BN( 29) // DD2 Only, ECO 777, Rec. Fifo Threshold crossed is not sticky
#define   _DMA_BASE_CONTROL_INJ_FIFO_CROSSTHRESH_NOTSTICKY  _BN( 30) // DD2 Only, ECO 777, Inj. Fifo Threshold crossed is not sticky
                                                           // 31 - ECO 653, leave at 0
#define _BGP_DCR_DMA_BASE_CONTROL_INIT  ( _DMA_BASE_CONTROL_USE_DMA    | \
                                          _DMA_BASE_CONTROL_L3BURST_EN | \
                                          _DMA_BASE_CONTROL_ITIU_EN    | \
                                          _DMA_BASE_CONTROL_RTIU_EN    | \
                                          _DMA_BASE_CONTROL_IMFU_EN    | \
                                          _DMA_BASE_CONTROL_RMFU_EN)

// g in the interval [0:7]:
//  32bit 16Byte aligned Physical Addresses containing (0..3 of UA | 0..27 of PA).
#define _BGP_DCR_iDMA_MIN_VALID_ADDR(g)                 (_BGP_DCR_DMA+((2*(g))+0x02))
#define _BGP_DCR_iDMA_MAX_VALID_ADDR(g)                 (_BGP_DCR_DMA+((2*(g))+0x03))

#define _BGP_DCR_iDMA_INJ_RANGE_TLB                     (_BGP_DCR_DMA+0x12)
#define   _iDMA_INT_RANGE_TLB_L3CIN(r) _BN( 0+((r)*4))   // (oops typo) 'r' in {0..7} Bit 0 of each range is L3 Cache Inhibit
#define   _iDMA_INT_RANGE_TLB_L3SCR(r) _BN( 1+((r)*4))   // (oops typo) 'r' in {0..7} Bit 1 of each range is L3 ScratchPad.
#define   _iDMA_INJ_RANGE_TLB_L3CIN(r) _BN( 0+((r)*4))   // 'r' in {0..7} Bit 0 of each range is L3 Cache Inhibit
#define   _iDMA_INJ_RANGE_TLB_L3SCR(r) _BN( 1+((r)*4))   // 'r' in {0..7} Bit 1 of each range is L3 ScratchPad.
                                                         // Bits 2,3 of each range are reserved.

#define _BGP_DCR_rDMA_REC_RANGE_TLB                     (_BGP_DCR_DMA+0x13)
#define   _rDMA_REC_RANGE_TLB_L3CIN(r) _BN( 0+((r)*4))   // 'r' in {0..7} Bit 0 of each range is L3 Cache Inhibit
#define   _rDMA_REC_RANGE_TLB_L3SCR(r) _BN( 1+((r)*4))   // 'r' in {0..7} Bit 1 of each range is L3 ScratchPad.

// g in the interval [0:7]
//  32bit 16Byte aligned Physical Addresses containing (0..3 of UA | 0..27 of PA).
#define _BGP_DCR_rDMA_MIN_VALID_ADDR(g)                 (_BGP_DCR_DMA+((2*(g))+0x14))
#define _BGP_DCR_rDMA_MAX_VALID_ADDR(g)                 (_BGP_DCR_DMA+((2*(g))+0x15))

// j in the interval [0:1]
#define _BGP_DCR_iDMA_TS_FIFO_WM(j)                     (_BGP_DCR_DMA+(0x24+(j)))
#define  _iDMA_TS_FIFO_WM_N0(x) 	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N1(x)         _B6(15,(x))	// bit {10..15} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N2(x)         _B6(23,(x))	// bit {18..23} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_P0(x)         _B6(31,(x))	// bit {26..31} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N3(x)		_B6(7,(x))	// bit {2..7}   of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N4(x)		_B6(15,(x))	// bit {10..15} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N5(x)		_B6(23,(x))	// bit {18..23} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_P1(x)         _B6(31,(x))	// bit {26..31} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20

#define  _iDMA_TS_FIFO_WM0_INIT   	(_iDMA_TS_FIFO_WM_N0(20) | \
					 _iDMA_TS_FIFO_WM_N1(20) | \
					 _iDMA_TS_FIFO_WM_N2(20) | \
					 _iDMA_TS_FIFO_WM_P0(20))
#define  _iDMA_TS_FIFO_WM1_INIT   	(_iDMA_TS_FIFO_WM_N3(20) | \
					 _iDMA_TS_FIFO_WM_N4(20) | \
					 _iDMA_TS_FIFO_WM_N5(20) | \
					 _iDMA_TS_FIFO_WM_P1(20))

#define _BGP_DCR_iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY             (_BGP_DCR_DMA+0x26)
#define  _iDMA_LOCAL_FIFO_WM(x)		_B7(7,(x))	// bit {1..7}   of _BGP_DCR_iDMA_LOCAL_FIFO_WM_RPT_CNT, set to decimal 55, 0x37
#define  _iDMA_HP_INJ_FIFO_RPT_CNT(x)   _B4(11,(x))	// bit {8..11}  dma repeat count for using torus high priority injection fifo
#define  _iDMA_NP_INJ_FIFO_RPT_CNT(x)   _B4(15,(x))	// bit {12..15} dma repeat count for using torus normal priority injection fifo
#define  _iDMA_INJ_DELAY(x)		_B4(23,(x))	// bit {20..23} dma delay this amount of clock_x2 cycles before injecting next packet

#define  _iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT	(_iDMA_LOCAL_FIFO_WM(55) | \
						 _iDMA_HP_INJ_FIFO_RPT_CNT(0) | \
						 _iDMA_NP_INJ_FIFO_RPT_CNT(0) | \
						 _iDMA_INJ_DELAY(0))

// p in the interval [0:3]
#define _BGP_DCR_rDMA_TS_FIFO_WM(p)                     (_BGP_DCR_DMA+(0x27+(p)))
#define  _rDMA_TS_FIFO_WM_G0N0(x)	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N1(x)	_B6(15,(x))	// bit {10..15} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N2(x)	_B6(23,(x))	// bit {18..23} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N3(x)	_B6(31,(x))	// bit {26..31} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N4(x)	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_rDMA_TORUS_FIFO_WM(1), must be 0
#define  _rDMA_TS_FIFO_WM_G0N5(x)	_B6(15,(x))	// bit {10..15} of _BGP_DCR_rDMA_TORUS_FIFO_WM(1), must be 0
#define  _rDMA_TS_FIFO_WM_G0P(x)	_B6(23,(x))     // bit {18..23} of _BGP_DCR_rDMA_TORUS_FIFO_WM(1), must be 0
#define  _rDMA_TS_FIFO_WM_G1N0(x)	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_rDMA_TORUS_FIFO_WM(2), must be 0
#define  _rDMA_TS_FIFO_WM_G1N1(x)	_B6(15,(x))	// bit {10..15} of _BGP_DCR_rDMA_TORUS_FIFO_WM(2), must be 0
#define  _rDMA_TS_FIFO_WM_G1N2(x)	_B6(23,(x))	// bit {18..23} of _BGP_DCR_rDMA_TORUS_FIFO_WM(2), must be 0
#define  _rDMA_TS_FIFO_WM_G1N3(x)	_B6(31,(x))	// bit {26..31} of _BGP_DCR_rDMA_TORUS_FIFO_WM(2), must be 0
#define  _rDMA_TS_FIFO_WM_G1N4(x)	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_rDMA_TORUS_FIFO_WM(3), must be 0
#define  _rDMA_TS_FIFO_WM_G1N5(x)	_B6(15,(x))	// bit {10..15} of _BGP_DCR_rDMA_TORUS_FIFO_WM(3), must be 0
#define  _rDMA_TS_FIFO_WM_G1P(x)	_B6(23,(x))     // bit {18..23} of _BGP_DCR_rDMA_TORUS_FIFO_WM(3), must be 0

#define  _rDMA_TS_FIFO_WM0_INIT		(_rDMA_TS_FIFO_WM_G0N0(0) | \
					 _rDMA_TS_FIFO_WM_G0N1(0) | \
					 _rDMA_TS_FIFO_WM_G0N2(0) | \
					 _rDMA_TS_FIFO_WM_G0N3(0))
#define  _rDMA_TS_FIFO_WM1_INIT		(_rDMA_TS_FIFO_WM_G0N4(0) | \
					 _rDMA_TS_FIFO_WM_G0N5(0) | \
					 _rDMA_TS_FIFO_WM_G0P(0))
#define  _rDMA_TS_FIFO_WM2_INIT		(_rDMA_TS_FIFO_WM_G1N0(0) | \
					 _rDMA_TS_FIFO_WM_G1N1(0) | \
					 _rDMA_TS_FIFO_WM_G1N2(0) | \
					 _rDMA_TS_FIFO_WM_G1N3(0))
#define  _rDMA_TS_FIFO_WM3_INIT		(_rDMA_TS_FIFO_WM_G1N4(0) | \
					 _rDMA_TS_FIFO_WM_G1N5(0) | \
					 _rDMA_TS_FIFO_WM_G1P(0))

#define _BGP_DCR_rDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY       (_BGP_DCR_DMA+0x2b)
#define  _rDMA_LOCAL_FIFO_WM(x)		_B7(7,(x))	// bit {1..7}, local fifo watermark, must be 0
#define  _rDMA_HP_REC_FIFO_RPT_CNT(x)	_B4(11,(x))	// bit {8..11}, dma repeat count for torus high priority reception fifos
#define  _rDMA_NP_REC_FIFO_RPT_CNT(x)	_B4(15,(x))	// bit {12..15}, dma repeat count for torus normal priority reception fifos
#define  _rDMA_DELAY(x)			_B4(23,(x))	// bit {20..23}, dma delay this amount of clock_x2 cycles between packets

#define  _rDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT	(_rDMA_LOCAL_FIFO_WM(0) | \
						 _rDMA_HP_REC_FIFO_RPT_CNT(0) | \
						 _rDMA_NP_REC_FIFO_RPT_CNT(0) | \
						 _rDMA_DELAY(0))

// i in the interval [0:3]
#define _BGP_DCR_iDMA_FIFO_ENABLE(i)                    (_BGP_DCR_DMA+(0x2c+(i))) // each bit, if '1', enables an injection fifo
#define _BGP_DCR_rDMA_FIFO_ENABLE                       (_BGP_DCR_DMA+0x30)	// each bit, if '1', enables a reception fifo
#define _BGP_DCR_rDMA_FIFO_ENABLE_HEADER                (_BGP_DCR_DMA+0x31)
#define  _rDMA_FIFO_ENABLE_HEADER0	_BN(28)
#define  _rDMA_FIFO_ENABLE_HEADER1	_BN(29)
#define  _rDMA_FIFO_ENABLE_HEADER2	_BN(30)
#define  _rDMA_FIFO_ENABLE_HEADER3	_BN(31)

// i in the interval [0:3]
#define _BGP_DCR_iDMA_FIFO_PRIORITY(i)                  (_BGP_DCR_DMA+(0x32+(i)))
#define _BGP_DCR_iDMA_FIFO_RGET_THRESHOLD               (_BGP_DCR_DMA+0x36)
#define _BGP_DCR_iDMA_SERVICE_QUANTA                    (_BGP_DCR_DMA+0x37)
#define  _iDMA_SERVICE_QUANTA_HP(x)	_B16(15,(x))
#define  _iDMA_SERVICE_QUANTA_NP(x)	_B16(31,(x))
#define  _iDMA_SERVICE_QUANTA_INIT	(_iDMA_SERVICE_QUANTA_HP(0) | _iDMA_SERVICE_QUANTA_NP(0))

#define _BGP_DCR_rDMA_FIFO_TYPE                         (_BGP_DCR_DMA+0x38)
#define _BGP_DCR_rDMA_FIFO_TYPE_HEADER                  (_BGP_DCR_DMA+0x39)
#define  _rDMA_FIFO_TYPE_HEADER0	_BN(28)
#define  _rDMA_FIFO_TYPE_HEADER1	_BN(29)
#define  _rDMA_FIFO_TYPE_HEADER2	_BN(30)
#define  _rDMA_FIFO_TYPE_HEADER3	_BN(31)
#define _BGP_DCR_rDMA_FIFO_THRESH0                      (_BGP_DCR_DMA+0x3a)
#define _BGP_DCR_rDMA_FIFO_THRESH1                      (_BGP_DCR_DMA+0x3b)

// k in the interval [0:31]
#define _BGP_DCR_iDMA_TS_INJ_FIFO_MAP(k)                (_BGP_DCR_DMA+(0x3c+(k)))  // 8 bits for every dma injection fifo
//@ Dong, for MG, is the following line good?
// j in the interval [0:3]
#define  _iDMA_TS_INJ_FIFO_MAP_FIELD(j, x)	_B8((7+(j)*8), (x))
// i in the interval [0:3]
#define _BGP_DCR_iDMA_LOCAL_COPY(i)                     (_BGP_DCR_DMA+(0x5c+(i)))  // one bit for every dma injection fifo

// XY  = X, Y
// ZHL = Z, High Priority, Local Copy
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G0_PID00_XY	(_BGP_DCR_DMA+0x60) // torus recv group 0, (pid0, pid1) = "00"
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G0_PID00_ZHL	(_BGP_DCR_DMA+0x61)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G0_PID01_XY	(_BGP_DCR_DMA+0x62)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G0_PID01_ZHL	(_BGP_DCR_DMA+0x63)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G1_PID10_XY       (_BGP_DCR_DMA+0x64)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G1_PID10_ZHL      (_BGP_DCR_DMA+0x65)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G1_PID11_XY       (_BGP_DCR_DMA+0x66)
#define _BGP_DCR_rDMA_TS_REC_FIFO_MAP_G1_PID11_ZHL      (_BGP_DCR_DMA+0x67)
#define  _rDMA_TS_REC_FIFO_MAP_XP(x)		_B8(7,(x))
#define  _rDMA_TS_REC_FIFO_MAP_XM(x)		_B8(15,(x))
#define  _rDMA_TS_REC_FIFO_MAP_YP(x)		_B8(23,(x))
#define  _rDMA_TS_REC_FIFO_MAP_YM(x)		_B8(31,(x))
#define  _rDMA_TS_REC_FIFO_MAP_ZP(x)		_B8(7,(x))
#define  _rDMA_TS_REC_FIFO_MAP_ZM(x)		_B8(15,(x))
#define  _rDMA_TS_REC_FIFO_MAP_HIGH(x)		_B8(23,(x))
#define  _rDMA_TS_REC_FIFO_MAP_LOCAL(x)		_B8(31,(x))

// ii in the interval [0:3]  group 0, group 1, ..., group 3
#define _BGP_DCR_rDMA_FIFO_CLEAR_MASK(ii)               (_BGP_DCR_DMA+(0x68+(ii)))
#define  _rDMA_FIFO_CLEAR_MASK0_INIT		0xFF000000
#define  _rDMA_FIFO_CLEAR_MASK1_INIT		0x00FF0000
#define  _rDMA_FIFO_CLEAR_MASK2_INIT		0x0000FF00
#define  _rDMA_FIFO_CLEAR_MASK3_INIT		0x000000FF
#define _BGP_DCR_rDMA_FIFO_HEADER_CLEAR_MASK            (_BGP_DCR_DMA+0x6c)
#define  _rDMA_FIFO_HEADER_CLEAR_MASK_INIT	0x08040201

// g in the interval [0:3]  group 0, group 1, group2, and group 3
#define _BGP_DCR_iDMA_FIFO_INT_ENABLE_GROUP(g)		(_BGP_DCR_DMA+(0x6d+(g)))
// t in the interval [0:3]  type 0, type 1, ..., type 3
#define _BGP_DCR_rDMA_FIFO_INT_ENABLE_TYPE(t)		(_BGP_DCR_DMA+(0x71+(t)))
#define _BGP_DCR_rDMA_HEADER_FIFO_INT_ENABLE		(_BGP_DCR_DMA+0x75)
#define  _rDMA_HEADER_HEADER_FIFO_INT_ENABLE_TYPE(t,x)	_B4((7+(t)*8), (x))

// g in the interval [0:3]  group 0, group 1, ..., group 3
#define _BGP_DCR_iDMA_COUNTER_INT_ENABLE_GROUP(g) (_BGP_DCR_DMA+(0x76+(g)))

// g in the interval [0:3]  group 0, group 1, ..., group 3
#define _BGP_DCR_rDMA_COUNTER_INT_ENABLE_GROUP(g) 	(_BGP_DCR_DMA+(0x7a+(g)))

// ----------------------------
// ---- Fatal Error Enables -----
// ----------------------------
// e in the interval [0:3], bit definition in the fatal errors at 0x93 - 0x96
#define _BGP_DCR_DMA_FATAL_ERROR_ENABLE(e)		(_BGP_DCR_DMA +(0x7e +(e)))

// -------------------------------
// ---- Backdoor Access Regs -----
// -------------------------------
#define _BGP_DCR_DMA_LF_IMFU_DESC_BD_CTRL		(_BGP_DCR_DMA+0x82)
#define  _DMA_LF_IMFU_DESC_BD_CTRL_ENABLE	_BN(0)	// if '1', enable backdoor read/write
#define  _DMA_LF_IMFU_DESC_BD_CTRL_NOECC	_BN(1)	// if '1', do not do ECC on backdoor read/write
#define  _DMA_LF_IMFU_DESC_BD_CTRL_RD_REQ	_BN(2)	// if '1', do read
#define  _DMA_LF_IMFU_DESC_BD_CTRL_WR_REQ	_BN(3)	// if '1', do write
#define  _DMA_LF_IMFU_DESC_BD_CTRL_IMFU_SEL	_BN(4)	// unit select, '0' local fifo, '1' imfu descriptor
#define  _DMA_LF_IMFU_DESC_BD_CTRL_LF_ADDR(x)	_B7(15,(x))	// 7 bit sram address for local fifo
#define  _DMA_LF_IMFU_DESC_BD_CTRL_IMFU_ADDR(x)	_B8(15,(x))	// 8 bit sram address for imfu descriptor
#define  _DMA_LF_IMFU_DESC_BD_CTRL_WR_ECC0(x)	_B8(23,(x))	// 8 bit write ECC for data bits 0 to 63
#define  _DMA_LF_IMFU_DESC_BD_CTRL_WR_ECC1(x)	_B8(31,(x))	// 8 bit write ECC for data bits 64 to 128
// i in the interval [0:3]
#define _BGP_DCR_DMA_LF_IMFU_DESC_BACKDOOR_WR_DATA(i)   (_BGP_DCR_DMA+(0x83+(i))) //128 bit backdoor write data
#define _BGP_DCR_DMA_ARRAY_BD_CTRL			(_BGP_DCR_DMA+0x87)  // fifo/counter array backdoor control
#define  _DMA_ARRAY_BD_CTRL_ENABLE		_BN(0)
#define  _DMA_ARRAY_BD_CTRL_RD_SEL_IMFU_FIFO	_B2(2,0)	// unit select for backdoor read
#define  _DMA_ARRAY_BD_CTRL_RD_SEL_IMFU_COUNTER	_B2(2,1)
#define  _DMA_ARRAY_BD_CTRL_RD_SEL_RMFU_FIFO	_B2(2,2)
#define  _DMA_ARRAY_BD_CTRL_RD_SEL_RMFU_COUNTER	_B2(2,3)
#define  _DMA_ARRAY_BD_CTRL_WR_ECC(x)		_B7(15,(x))

// -------------------------------------
// ---- Torus Link Checker Control -----
// -------------------------------------
#define _BGP_DCR_DMA_TS_LINK_CHK_CTRL			(_BGP_DCR_DMA+0x88)
#define  _DMA_TS_LINK_CHK_CTRL_SEL(x)		_B3(2,(x))	// 0 - xp, 1 - xm, 2 - yp, 3 - ym, 4 - zp, 5 - zm, 6, 7 disable
#define  _DMA_TS_LINK_CHK_CTRL_RW_ENABLE	_BN(8)		// if 1, enable read/write to link checker internal sram
#define  _DMA_TS_LINK_CHK_CTRL_WR_REQ		_BN(12)
#define  _DMA_TS_LINK_CHK_CTRL_RD_REQ		_BN(13)
#define  _DMA_TS_LINK_CHK_CTRL_ADDR(x)		_B10(23,(x))
#define  _DMA_TS_LINK_CHK_CTRL_WR_DATA(x)	_B8(31,(x))
#define  _DMA_TS_LINK_CHK_BAD_OFFSET            (0)          // sram address where bad packet starts
#define  _DMA_TS_LINK_CHK_GOOD_OFFSET           (320)        // sram address where good packet starts


// --------------------
// ---- Threshold -----
// --------------------
#define _BGP_DCR_DMA_CE_COUNT_THRESHOLD			(_BGP_DCR_DMA+0x89) // correctable ecc error count threshold, reset to 0xFFFFFFFF
// default used when system comes out of reset, will have to be tuned
#define  _BGP_DCR_DMA_CE_COUNT_THRESHOLD_INIT  1

// ----------------------------------
// ---- Correctable error count -----
// ----------------------------------
// c in the interval [0:8]  count 0, count 1, ..., count 8
#define _BGP_DCR_DMA_CE_COUNT(c)			(_BGP_DCR_DMA+(0x8A+(c)))
#define _BGP_DCR_DMA_CE_COUNT_INJ_FIFO0			(_BGP_DCR_DMA+0x8A)
#define _BGP_DCR_DMA_CE_COUNT_INJ_FIFO1			(_BGP_DCR_DMA+0x8B)
#define _BGP_DCR_DMA_CE_COUNT_INJ_COUNTER		(_BGP_DCR_DMA+0x8C)
#define _BGP_DCR_DMA_CE_COUNT_INJ_DESC                  (_BGP_DCR_DMA+0x8D)
#define _BGP_DCR_DMA_CE_COUNT_REC_FIFO0                 (_BGP_DCR_DMA+0x8E)
#define _BGP_DCR_DMA_CE_COUNT_REC_FIFO1                 (_BGP_DCR_DMA+0x8F)
#define _BGP_DCR_DMA_CE_COUNT_REC_COUNTER               (_BGP_DCR_DMA+0x90)
#define _BGP_DCR_DMA_CE_COUNT_LOCAL_FIFO0               (_BGP_DCR_DMA+0x91)
#define _BGP_DCR_DMA_CE_COUNT_LOCAL_FIFO1               (_BGP_DCR_DMA+0x92)

// upon termination, create RAS event if any of the above counts are greater than this value
#define _BGP_DCR_DMA_CE_TERM_THRESH  0

// -----------------
// ---- Status -----
// -----------------
// e in the interval [0:3]  error0, error1, ..., error 3
#define _BGP_DCR_DMA_FATAL_ERROR(e)                     (_BGP_DCR_DMA+(0x93+(e)))

// Below are are error conditions most likely caused by software
#define _BGP_DCR_DMA_FATAL_ERROR0_WR0_MSB  _BN(4)   // pque wr0 msb not 0
#define _BGP_DCR_DMA_FATAL_ERROR0_RD0_MSB  _BN(8)   // pque rd0 msb not 0
#define _BGP_DCR_DMA_FATAL_ERROR0_WR1_MSB  _BN(12)  // pque wr1 msb not 0
#define _BGP_DCR_DMA_FATAL_ERROR0_RD1_MSB  _BN(16)  // pque rd1 msb not 0

#define _BGP_DCR_DMA_FATAL_ERROR1_REC_MAP  _BN(22)  //  multiple bits set for the dcr rec fifo map


#define _BGP_DCR_DMA_FATAL_ERROR2_FIFO_SEL   _BN(14) // fifo_sel_n error
#define _BGP_DCR_DMA_FATAL_ERROR2_FIFO_SEL_FORM  _BN(15) // fifo_sel_n_form error
#define _BGP_DCR_DMA_FATAL_ERROR2_READ_RANGE _BN(25) // read from address not in one of dcr address ranges

#define _BGP_DCR_DMA_FATAL_ERROR3_DPUT_SIZE   _BN(8)  // direct put packet had greater than 240 bytes
#define _BGP_DCR_DMA_FATAL_ERROR3_RGET_SIZE   _BN(9)  // remote get packet had greater than 240 bytes
#define _BGP_DCR_DMA_FATAL_ERROR3_MAX_ADDRESS _BN(18) // write to address larger than counter max
#define _BGP_DCR_DMA_FATAL_ERROR3_WRITE_RANGE _BN(26) // write to address not in one of dcr address ranges

#define _BGP_DCR_DMA_PQUE_WR0_BAD_ADDR                  (_BGP_DCR_DMA+0x97)
#define _BGP_DCR_DMA_PQUE_RD0_BAD_ADDR                  (_BGP_DCR_DMA+0x98)
#define _BGP_DCR_DMA_PQUE_WR1_BAD_ADDR                  (_BGP_DCR_DMA+0x99)
#define _BGP_DCR_DMA_PQUE_RD1_BAD_ADDR                  (_BGP_DCR_DMA+0x9a)

#define _BGP_DCR_DMA_MFU_STAT0                          (_BGP_DCR_DMA+0x9b)
#define  _DMA_MFU_STAT0_IMFU_NOT_ENABLED_COUNTER_ID(x)	_G8((x), 7)		// idma not enabled counter id
#define  _DMA_MFU_STAT0_IMFU_UNDERFLOW_COUNTER_ID(x)	_G8((x), 15)		// idma underflow counter id
#define  _DMA_MFU_STAT0_IMFU_OVERFLOW_NB_ADDR(x)	_G16((x), 31)		// idma netbus addr that caused counter overflow
#define _BGP_DCR_DMA_MFU_STAT1                          (_BGP_DCR_DMA+0x9c)
#define  _DMA_MFU_STAT1_IMFU_CUR_FIFO_ID(x)		_G7((x), 7)		// current fifo id that idma is working on
#define  _DMA_MFU_STAT1_RMFU_UNDERFLOW_COUNTER_ID(x)	_G8((x), 15)		// rdma underflow counter id
#define  _DMA_MFU_STAT1_RMFU_OVERFLOW_NB_ADDR(x)	_G16((x), 31)		// rdma netbus addr that caused counter overflow
#define _BGP_DCR_DMA_MFU_STAT2                          (_BGP_DCR_DMA+0x9d)
#define  _DMA_MFU_STAT2_RMFU_FIFO_NE_OR_NA(x)		_GN((x), 0)		// rdma fifo not enabled or not all_available
#define  _DMA_MFU_STAT2_RMFU_HDR_FIFO_NE_OR_NA(x)	_GN((x), 1)		// rdma header fifo not enabled or not all_available
#define  _DMA_MFU_STAT2_RMFU_INJ_FIFO_NE_OR_NA(x)	_GN((x), 2)		// rdma injection fifo for remote get not enabled or not all_available
#define  _DMA_MFU_STAT2_RMFU_COUNTER_NE(x)		_GN((x), 3)		// rdma accessing not enabled counter
#define  _DMA_MFU_STAT2_RMFU_PKT_PID(x)			_G2((x), 7)		// rdma receiving packet pid
#define  _DMA_MFU_STAT2_RMFU_FIFO_BIT(x)		_G8((x), 15)		// rdma receiving packet fifo bit, only one bit should be set
										// bit orders are xp, xm, yp, ym, zp, zm, hp, local
#define  _DMA_MFU_STAT2_RMFU_RGET_FIFO_ID(x)		_G8((x), 23)		// rdma remote get (injection) fifo id
#define  _DMA_MFU_STAT2_RMFU_COUNTER_ID(x)		_G8((x), 31)		// rdma direct put counter id
#define _BGP_DCR_DMA_L3_RD_ERROR_ADDR                   (_BGP_DCR_DMA+0x9e)
#define _BGP_DCR_DMA_L3_WR_ERROR_ADDR                   (_BGP_DCR_DMA+0x9f)

// i in the interval [0:3]
#define _BGP_DCR_DMA_LF_IMFU_DESC_BD_RD_DATA(i)		(_BGP_DCR_DMA+(0xa0+(i)))
#define _BGP_DCR_DMA_LF_IMFU_DESC_BD_RD_ECC             (_BGP_DCR_DMA+0xa4)
#define  _DMA_LF_IMFU_DESC_BD_RD_ECC_DWORD0(x)		_G8((x),23)		// ecc for data bits 0 to 63
#define  _DMA_LF_IMFU_DESC_BD_RD_ECC_DWORD1(x)		_G8((x),31)		// ecc for data bits 64 to 127
#define _BGP_DCR_DMA_ARRAY_RD_ECC                       (_BGP_DCR_DMA+0xa5)
#define  _DMA_ARRAY_RD_ECC_WORD0(x)			_G7((x), 7)		// word address offset 0
#define  _DMA_ARRAY_RD_ECC_WORD1(x)			_G7((x), 15)		// word address offset 1
#define  _DMA_ARRAY_RD_ECC_WORD2(x)			_G7((x), 23)		// word address offset 2
#define  _DMA_ARRAY_RD_ECC_WORD3(x)			_G7((x), 31)		// word address offset 3
#define _BGP_DCR_DMA_TS_LINK_CHK_STAT                   (_BGP_DCR_DMA+0xa6)
#define  _DMA_TS_LINK_CHK_STAT_PKT_CAPTURED(x)		_GN((x), 0) 		// bad packet captured flag
#define  _DMA_TS_LINK_CHK_STAT_RECV_PIPE_FERR(x)	_GN((x), 1) 		// receive pipe fatal error
#define  _DMA_TS_LINK_CHK_STAT_STATE(x)			_G4((x), 7) 		// state machine state
#define  _DMA_TS_LINK_CHK_STAT_SRAM_ADDR(x)		_G10((x), 23) 		// current sram read or write address
#define  _DMA_TS_LINK_CHK_STAT_SRAM_RD_DATA(x)		_G8((x), 31) 		// sram read data

// ---- Debug -----
// i in the interval [0:3]
#define _BGP_DCR_DMA_iFIFO_DESC_RD_FLAG(i)              (_BGP_DCR_DMA+(0xa7+(i)))
// j in the interval [0:1]
#define _BGP_DCR_DMA_INTERNAL_STATE(j)                  (_BGP_DCR_DMA+(0xab+(j)))
#define  _DMA_INTERNAL_STATE0_IMFU_SEL_STATE(x)		_G3((x), 2)
#define  _DMA_INTERNAL_STATE0_IMFU_ARB_STATE(x)		_G5((x), 7)
#define  _DMA_INTERNAL_STATE0_IMFU_FIFO_ARB_STATE(x)	_G5((x), 12)
#define  _DMA_INTERNAL_STATE0_IMFU_CNT_ARB_STATE(x)	_G4((x), 16)
#define  _DMA_INTERNAL_STATE0_RMFU_ARB_STATE(x)		_G5((x), 23)
#define  _DMA_INTERNAL_STATE0_RMFU_FIFO_ARB_STATE(x)	_G4((x), 27)
#define  _DMA_INTERNAL_STATE0_RMFU_CNT_ARB_STATE(x)	_G4((x), 31)

#define  _DMA_INTERNAL_STATE1_PQUE_ARB_STATE(x)		_G3((x), 2)
#define  _DMA_INTERNAL_STATE1_ICONU_SM_STATE(x)		_G4((x), 6)
#define  _DMA_INTERNAL_STATE1_IFSU_SM_STATE(x)		_G3((x), 9)
#define  _DMA_INTERNAL_STATE1_IDAU_L3RSM_STATE(x)	_G3((x), 12)
#define  _DMA_INTERNAL_STATE1_IDAU_L3VSM_STATE(x)	_G3((x), 15)
#define  _DMA_INTERNAL_STATE1_IDAU_TTSM_STATE(x)	_G3((x), 18)
#define  _DMA_INTERNAL_STATE1_RCONU_SM_STATE(x)		_G4((x), 22)
#define  _DMA_INTERNAL_STATE1_RFSU_SM_STATE(x)		_G3((x), 25)
#define  _DMA_INTERNAL_STATE1_RDAU_QRSM_STATE(x)	_G3((x), 28)
#define  _DMA_INTERNAL_STATE1_RDAU_L3SM_STATE(x)	_G3((x), 31)

// values for _BGP_DCR_DMA_INTERNAL_STATE when all state machines are in idle, or wait state
#define _BGP_DCR_DMA_INTERNAL_STATE_0_IDLE               (0x21088111)

// values for _BGP_DCR_DMA_INTERNAL_STATE when all state machines are in idle, or wait state
#define _BGP_DCR_DMA_INTERNAL_STATE_0_IDLE               (0x21088111)
#define _BGP_DCR_DMA_INTERNAL_STATE_1_IDLE               (0x22492249)

#define _BGP_DCR_DMA_PQUE_POINTER                       (_BGP_DCR_DMA+0xad)
#define  _DMA_PQUE_POINTER_WR0_BEGIN(x)			_G4((x),3)
#define  _DMA_PQUE_POINTER_WR0_END(x)			_G4((x),7)
#define  _DMA_PQUE_POINTER_RD0_BEGIN(x)			_G4((x),11)
#define  _DMA_PQUE_POINTER_RD0_END(x)			_G4((x),15)
#define  _DMA_PQUE_POINTER_WR1_BEGIN(x)			_G4((x),19)
#define  _DMA_PQUE_POINTER_WR1_END(x)			_G4((x),23)
#define  _DMA_PQUE_POINTER_RD1_BEGIN(x)			_G4((x),27)
#define  _DMA_PQUE_POINTER_RD1_END(x)			_G4((x),31)
#define _BGP_DCR_DMA_LOCAL_FIFO_POINTER                 (_BGP_DCR_DMA+0xae)
#define  _DMA_LOCAL_FIFO_POINTER_BEGIN(x)		_G8((x),7)
#define  _DMA_LOCAL_FIFO_POINTER_END(x)			_G8((x),15)
#define  _DMA_LOCAL_FIFO_POINTER_END_OF_PKT(x)		_G8((x),23)
#define _BGP_DCR_DMA_WARN_ERROR                         (_BGP_DCR_DMA+0xaf)

// offsets  0xb0 are reserved

// ---- Clears -----
#define _BGP_DCR_DMA_CLEAR0                             (_BGP_DCR_DMA+0xb1)
#define  _DMA_CLEAR0_IMFU_ARB_WERR		_BN(0)
#define  _DMA_CLEAR0_IMFU_COUNTER_UNDERFLOW	_BN(1)
#define  _DMA_CLEAR0_IMFU_COUNTER_OVERFLOW	_BN(2)
#define  _DMA_CLEAR0_RMFU_COUNTER_UNDERFLOW	_BN(3)
#define  _DMA_CLEAR0_RMFU_COUNTER_OVERFLOW	_BN(4)
#define  _DMA_CLEAR0_RMFU_ARB_WERR		_BN(5)
#define  _DMA_CLEAR0_PQUE_WR0_BEN_WERR		_BN(6)
#define  _DMA_CLEAR0_PQUE_WR0_ADDR_CHK_WERR	_BN(7)
#define  _DMA_CLEAR0_PQUE_RD0_ADDR_CHK_WERR	_BN(8)
#define  _DMA_CLEAR0_PQUE_WR1_BEN_WERR		_BN(9)
#define  _DMA_CLEAR0_PQUE_WR1_ADDR_CHK_WERR	_BN(10)
#define  _DMA_CLEAR0_PQUE_RD1_ADDR_CHK_WERR	_BN(11)
#define  _DMA_CLEAR0_PQUE_WR0_HOLD_BAD_ADDR	_BN(12)
#define  _DMA_CLEAR0_PQUE_RD0_HOLD_BAD_ADDR	_BN(13)
#define  _DMA_CLEAR0_PQUE_WR1_HOLD_BAD_ADDR	_BN(14)
#define  _DMA_CLEAR0_PQUE_RD1_HOLD_BAD_ADDR	_BN(15)
#define  _DMA_CLEAR0_IFIFO_ARRAY_UE0		_BN(16)
#define  _DMA_CLEAR0_IFIFO_ARRAY_UE1		_BN(17)
#define  _DMA_CLEAR0_ICOUNTER_ARRAY_UE		_BN(18)
#define  _DMA_CLEAR0_IMFU_DESC_UE		_BN(19)
#define  _DMA_CLEAR0_RFIFO_ARRAY_UE0		_BN(20)
#define  _DMA_CLEAR0_RFIFO_ARRAY_UE1		_BN(21)
#define  _DMA_CLEAR0_RCOUNTER_ARRAY_UE		_BN(22)
#define  _DMA_CLEAR0_LOCAL_FIFO_UE0		_BN(23)
#define  _DMA_CLEAR0_LOCAL_FIFO_UE1		_BN(24)

#define _BGP_DCR_DMA_CLEAR1                             (_BGP_DCR_DMA+0xb2)
#define  _DMA_CLEAR1_TS_LINK_CHK		_BN(0)


#endif
