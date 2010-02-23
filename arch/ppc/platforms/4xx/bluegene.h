/*
 * Blue Gene board definitions
 *
 * Todd Inglett <tinglett@us.ibm.com>
 *
 * Copyright 2005 International Business Machines, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifdef __KERNEL__
#ifndef __PPC_PLATFORMS_BLUEGENE_H__
#define __PPC_PLATFORMS_BLUEGENE_H__

#include <platforms/4xx/ibm440gx.h>

/* No PCI on Blue Gene */
#undef _IO_BASE
#define _IO_BASE	0
#undef _ISA_MEM_BASE
#define _ISA_MEM_BASE	0
#undef PCI_DRAM_OFFSET
#define PCI_DRAM_OFFSET	0

/* MAL is at a different base than 440gx */
#undef DCRN_MAL_BASE
#define DCRN_MAL_BASE		0x300


/* Bluegene firmware */
#define BGP_MAX_MAILBOX			(4)
#define BGP_MAILBOX_DESC_OFFSET		(0x7fd0)

#define BGP_DCR_TEST(x)			(0x400 + (x))
#define BGP_DCR_GLOB_ATT_WRITE_SET	BGP_DCR_TEST(0x17)
#define BGP_DCR_GLOB_ATT_WRITE_CLEAR	BGP_DCR_TEST(0x18)
#define BGP_DCR_TEST_STATUS6		BGP_DCR_TEST(0x3a)
#define   BGP_TEST_STATUS6_IO_CHIP	(0x80000000U >> 3)

#define BGP_ALERT_OUT(core)	        (0x80000000U >> (24 + core))
#define BGP_ALERT_IN(core)	        (0x80000000U >> (28 + core))

#define BGP_JMB_CMD2HOST_PRINT          (0x0002)

/*
 * SRAM
 */
#define BGP_SRAM_BASE		(0x7FFFF8000ULL)
#define BGP_SRAM_SIZE		(32*1024)
#define BGP_PERSONALITY_OFFSET	(0xfffff800U - 0xffff8000U)

/*
 * Interrupt controller
 */
#define BGP_IRQCTRL_BASE	(0x730000000ULL)
#define BGP_IRQCTRL_SIZE	(0x1000)
#define BGP_MAX_GROUP		(10)
#define BGP_MAX_CORE		4

#define IRQ_TO_GROUP(irq)	((irq >> 5) & 0xf)
#define GROUP_TO_IRQ(group)	(group << 5)
#define IRQ_OF_GROUP(irq)	((irq & 0x1f))

#define BGP_IRQCTRL_TARGET_DISABLED		(0x0)
#define BGP_IRQCTRL_TARGET_BCAST_NONCRIT	(0x1)
#define BGP_IRQCTRL_TARGET_BCAST_CRIT		(0x2)
#define BGP_IRQCTRL_TARGET_BCAST_MCHECK		(0x3)
#define BGP_IRQCTRL_TARGET_NONCRIT(cpu)		(0x4 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_CRIT(cpu)		(0x8 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_MCHECK(cpu)		(0xc | (cpu & 0x3))

#endif /* __PPC_PLATFORMS_BLUEGENE_H__ */
#endif /* __KERNEL__ */
