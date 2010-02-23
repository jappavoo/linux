/*
 * drivers/net/ibm_newemac/mal.h
 *
 * Memory Access Layer (MAL) support
 *
 * Copyright (c) 2004, 2005 Zultys Technologies.
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 *
 * Based on original work by
 *      Armin Kuster <akuster@mvista.com>
 *      Copyright 2002 MontaVista Softare Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef __IBM_NEWEMAC_MAL_H
#define __IBM_NEWEMAC_MAL_H

/*
 * There are some variations on the MAL, we express them in this driver as
 * MAL Version 1 and 2 though that doesn't match any IBM terminology.
 *
 * We call MAL 1 the version in 405GP, 405GPR, 405EP, 440EP, 440GR and
 * NP405H.
 *
 * We call MAL 2 the version in 440GP, 440GX, 440SP, 440SPE and Axon
 *
 * The driver expects a "version" property in the emac node containing
 * a number 1 or 2. New device-trees for EMAC capable platforms are thus
 * required to include that when porting to arch/powerpc.
 */

/* MALx DCR registers */
#define	MAL_CFG			0x00
#define	  MAL_CFG_SR		0x80000000
#define   MAL_CFG_PLBB		0x00004000
#define   MAL_CFG_OPBBL		0x00000080
#define   MAL_CFG_EOPIE		0x00000004
#define   MAL_CFG_LEA		0x00000002
#define   MAL_CFG_SD		0x00000001

/* MAL V1 CFG bits */
#define   MAL1_CFG_PLBP_MASK	0x00c00000
#define   MAL1_CFG_PLBP_10	0x00800000
#define   MAL1_CFG_GA		0x00200000
#define   MAL1_CFG_OA		0x00100000
#define   MAL1_CFG_PLBLE	0x00080000
#define   MAL1_CFG_PLBT_MASK	0x00078000
#define   MAL1_CFG_DEFAULT	(MAL1_CFG_PLBP_10 | MAL1_CFG_PLBT_MASK)

/* MAL V2 CFG bits */
#define   MAL2_CFG_RPP_MASK	0x00c00000
#define   MAL2_CFG_RPP_10	0x00800000
#define   MAL2_CFG_RMBS_MASK	0x00300000
#define   MAL2_CFG_WPP_MASK	0x000c0000
#define   MAL2_CFG_WPP_10	0x00080000
#define   MAL2_CFG_WMBS_MASK	0x00030000
#define   MAL2_CFG_PLBLE	0x00008000
#define   MAL2_CFG_DEFAULT	(MAL2_CFG_RMBS_MASK | MAL2_CFG_WMBS_MASK | \
				 MAL2_CFG_RPP_10 | MAL2_CFG_WPP_10)

#define MAL_ESR			0x01
#define   MAL_ESR_EVB		0x80000000
#define   MAL_ESR_CIDT		0x40000000
#define   MAL_ESR_CID_MASK	0x3e000000
#define   MAL_ESR_CID_SHIFT	25
#define   MAL_ESR_DE		0x00100000
#define   MAL_ESR_OTE		0x00040000
#define   MAL_ESR_OSE		0x00020000
#define   MAL_ESR_PEIN		0x00010000
#define   MAL_ESR_DEI		0x00000010
#define   MAL_ESR_OTEI		0x00000004
#define   MAL_ESR_OSEI		0x00000002
#define   MAL_ESR_PBEI		0x00000001

/* MAL V1 ESR bits */
#define   MAL1_ESR_ONE		0x00080000
#define   MAL1_ESR_ONEI		0x00000008

/* MAL V2 ESR bits */
#define   MAL2_ESR_PTE		0x00800000
#define   MAL2_ESR_PRE		0x00400000
#define   MAL2_ESR_PWE		0x00200000
#define   MAL2_ESR_PTEI		0x00000080
#define   MAL2_ESR_PREI		0x00000040
#define   MAL2_ESR_PWEI		0x00000020


#define MAL_IER			0x02
#define   MAL_IER_DE		0x00000010
#define   MAL_IER_OTE		0x00000004
#define   MAL_IER_OE		0x00000002
#define   MAL_IER_PE		0x00000001
/* MAL V1 IER bits */
#define   MAL1_IER_NWE		0x00000008
#define   MAL1_IER_SOC_EVENTS	MAL1_IER_NWE
#define   MAL1_IER_EVENTS	(MAL1_IER_SOC_EVENTS | MAL_IER_OTE | \
				 MAL_IER_OTE | MAL_IER_OE | MAL_IER_PE)

/* MAL V2 IER bits */
#define   MAL2_IER_PT		0x00000080
#define   MAL2_IER_PRE		0x00000040
#define   MAL2_IER_PWE		0x00000020
#define   MAL2_IER_SOC_EVENTS	(MAL2_IER_PT | MAL2_IER_PRE | MAL2_IER_PWE)
#define   MAL2_IER_EVENTS	(MAL2_IER_SOC_EVENTS | MAL_IER_OTE | \
				 MAL_IER_OTE | MAL_IER_OE | MAL_IER_PE)


#define MAL_TXCASR		0x04
#define MAL_TXCARR		0x05
#define MAL_TXEOBISR		0x06
#define MAL_TXDEIR		0x07
#define MAL_RXCASR		0x10
#define MAL_RXCARR		0x11
#define MAL_RXEOBISR		0x12
#define MAL_RXDEIR		0x13
#define MAL_TXCTPR(n)		((n) + 0x20)
#define MAL_RXCTPR(n)		((n) + 0x40)
#define MAL_RCBS(n)		((n) + 0x60)

#define MAL_CHAN_MASK(n)	(0x80000000 >> (n))



#ifdef CONFIG_IBM_NEW_EMAC_TOMAL

#include "tomal.h"
#include <linux/sysctl.h>

#define TOMAL_REG_SIZE		0x4000

#define MAL_MAX_TX_SIZE		9018
#define MAL_MAX_RX_SIZE		9018

#define NUM_WRAP_DESC		1

/* TOMAL Buffer Descriptor structure */
struct mal_descriptor {
	u16 ctrl;
	u16 data_len;
	u16 status;
	u16 frame_len;
	u64 data_ptr;
};

/* the following defines are for the TOMAL status and control registers. */
/* TOMAL transmit and receive status/control bits  */
/* tomal descriptor */
#define TOMAL_DESC_CODE_RX	0x6000
#define TOMAL_DESC_CODE_TX	0x6000
#define TOMAL_DESC_CODE_BRANCH  0x2000

#define TOMAL_DESC_TX_SIGNAL	0x0400
#define TOMAL_DESC_TX_NOTIFY	0x0200
#define TOMAL_DESC_TX_LAST	0x0100
#define TOMAL_DESC_TX_CHECKSUM	0x0040
#define TOMAL_DESC_TX_FCS	0x0020
#define TOMAL_DESC_TX_PADDING	0x0010
#define TOMAL_DESC_TX_ADD_SRC	0x0008
#define TOMAL_DESC_TX_REPL_SRC	0x0004
#define TOMAL_DESC_TX_ADD_VLAN	0x0002
#define TOMAL_DESC_TX_REPL_VLAN	0x0001

#define TOMAL_RX_STATUS_LAST	0x8000
#define TOMAL_RX_STATUS_MASK	0x03ff
#define TOMAL_DESC_RX_CHKSUM_VALID	0x0002
#define TOMAL_DESC_RX_CHKSUM_IP		0x0004
#define TOMAL_DESC_RX_CHKSUM_TCP_UDP	0x0008
#define TOMAL_DESC_RX_CHKSUM_MASK	0x000e
#define TOMAL_DESC_RX_CHKSUM_PASSED	0x000e

#define TOMAL_TX_STATUS_MASK	0x00ff
#define   TOMAL_TX_STATUS_SUCCESS 0x0001

#define TOMAL_MAX_CHANNEL		2
#define TOMAL_MAX_TOTAL_RX_BYTES	(1 << 20)
#define TOMAL_MAX_ADD_RX_BYTES		(1 << 16)

#define MAL_IS_TX_COMPLETED(desc) (((desc)->status & TOMAL_TX_STATUS_MASK) != 0)
#define MAL_IS_RX_EMPTY(desc) (((desc)->status & TOMAL_RX_STATUS_LAST) && ((desc)->data_len == 0))

extern inline void mal_set_rx_desc(struct mal_descriptor *desc, phys_addr_t addr,
				   u16 len, int wrap)
{
	desc->ctrl = TOMAL_DESC_CODE_RX;
	desc->data_len = len;
	desc->status = 0;
	desc->frame_len = 0;
	desc->data_ptr = addr;
}

extern inline void mal_set_tx_desc(struct mal_descriptor *desc, phys_addr_t addr,
				   u16 len, int last, int wrap, u16 flags)
{
	desc->data_len = len;
	desc->status = 0;
	desc->data_ptr = addr;
	wmb();
	desc->ctrl = TOMAL_DESC_CODE_TX | TOMAL_DESC_TX_FCS | TOMAL_DESC_TX_PADDING | flags;
	if (last)
		desc->ctrl |= TOMAL_DESC_TX_LAST | TOMAL_DESC_TX_SIGNAL | TOMAL_DESC_TX_NOTIFY;
}

extern inline void mal_clear_desc(struct mal_descriptor *desc)
{
	desc->data_ptr = 0;
	desc->ctrl = 0;
}

#else /* !CONFIG_IBM_NEW_EMAC_TOMAL */

/* In reality MAL can handle TX buffers up to 4095 bytes long,
 * but this isn't a good round number :) 		 --ebs
 */
#define MAL_MAX_TX_SIZE		4080
#define MAL_MAX_RX_SIZE		4080

#define NUM_WRAP_DESC		0

/* MAL Buffer Descriptor structure */
struct mal_descriptor {
	u16 ctrl;		/* MAL / Commac status control bits */
	u16 data_len;		/* Max length is 4K-1 (12 bits)     */
	u32 data_ptr;		/* pointer to actual data buffer    */
};

/* the following defines are for the MadMAL status and control registers. */
/* MADMAL transmit and receive status/control bits  */
#define MAL_RX_CTRL_EMPTY	0x8000
#define MAL_RX_CTRL_WRAP	0x4000
#define MAL_RX_CTRL_CM		0x2000
#define MAL_RX_CTRL_LAST	0x1000
#define MAL_RX_CTRL_FIRST	0x0800
#define MAL_RX_CTRL_INTR	0x0400
#define MAL_RX_CTRL_SINGLE	(MAL_RX_CTRL_LAST | MAL_RX_CTRL_FIRST)

#define MAL_TX_CTRL_READY	0x8000
#define MAL_TX_CTRL_WRAP	0x4000
#define MAL_TX_CTRL_CM		0x2000
#define MAL_TX_CTRL_LAST	0x1000
#define MAL_TX_CTRL_INTR	0x0400

#define MAL_IS_TX_COMPLETED(desc) (!((desc)->ctrl & MAL_TX_CTRL_READY))
#define MAL_IS_RX_EMPTY(desc) ((desc)->ctrl & MAL_RX_CTRL_EMPTY)

extern inline void mal_set_rx_desc(struct mal_descriptor *desc, phys_addr_t addr,
				   u16 len, int wrap)
{
	desc->data_len = 0; //len;
	desc->data_ptr = addr;
	wmb();
	dev->rx_desc[slot].ctrl = MAL_RX_CTRL_EMPTY | (wrap ? MAX_RX_CTRL_WRAP : 0)
}

extern inline void mal_set_tx_desc(struct mal_descriptor *desc, phys_addr_t addr,
				   u16 len, int last, int wrap, u16 flags)
{
	desc->data_len = len;
	desc->data_ptr = addr;
	wmb();
	desc->ctrl = EMAC_TX_CTRL_GFCS | EMAC_TX_CTRL_GP | MAL_TX_CTRL_READY |
		(last ? MAL_TX_CTRL_LAST : 0) | (wrap ? MAL_TX_CTRL_WRAP : 0) | flags;
}

extern inline void mal_clear_desc(struct mal_descriptor *desc)
{
	desc->data_ptr = 0;
	desc->ctrl = 0;
}

#endif /* !CONFIG_IBM_NEW_EMAC_TOMAL */

static inline int mal_rx_size(int len)
{
	len = (len + 0xf) & ~0xf;
	return len > MAL_MAX_RX_SIZE ? MAL_MAX_RX_SIZE : len;
}

static inline int mal_tx_chunks(int len)
{
	return (len + MAL_MAX_TX_SIZE - 1) / MAL_MAX_TX_SIZE;
}

struct mal_commac_ops {
	void	(*poll_tx) (void *dev);
	int	(*poll_rx) (void *dev, int budget);
	int	(*peek_rx) (void *dev);
	void	(*rxde) (void *dev);
};

struct mal_commac {
	struct mal_commac_ops	*ops;
	void			*dev;
	struct list_head	poll_list;
	long       		flags;
#define MAL_COMMAC_RX_STOPPED		0
#define MAL_COMMAC_POLL_DISABLED	1
	u32			tx_chan_mask;
	u32			rx_chan_mask;
	struct list_head	list;
};


struct mal_instance {
	int			version;
#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
	struct resource		tomal_regs;
	void			*io_base;
	int			tomal_irq0;
	int			tomal_irq1;
	unsigned long		tomal_rx_bufs[TOMAL_MAX_CHANNEL];
	unsigned long		tomal_rx_size[TOMAL_MAX_CHANNEL];
	struct ctl_table_header *sysctl_header;
#else
	int			dcr_base;
	dcr_host_t		dcr_host;

	int 			txeob_irq;	/* TX End Of Buffer IRQ  */
	int 			rxeob_irq;	/* RX End Of Buffer IRQ  */
	int			txde_irq;	/* TX Descriptor Error IRQ */
	int			rxde_irq;	/* RX Descriptor Error IRQ */
	int			serr_irq;	/* MAL System Error IRQ    */
#endif

	int			num_tx_chans;	/* Number of TX channels */
	int			num_rx_chans;	/* Number of RX channels */

	struct list_head	poll_list;
	struct net_device	poll_dev;

	struct list_head	list;
	u32			tx_chan_mask;
	u32			rx_chan_mask;

	dma_addr_t		bd_dma;
	struct mal_descriptor	*bd_virt;
	struct mal_ops		*ops;

	struct of_device	*ofdev;
	int			index;
	spinlock_t		lock;
};

/* Register MAL devices */
int mal_init(void) __init;
void mal_exit(void) __exit;

int mal_register_commac(struct mal_instance *mal,
			struct mal_commac *commac);
void mal_unregister_commac(struct mal_instance *mal,
			   struct mal_commac *commac);
int mal_set_rcbs(struct mal_instance *mal, int channel, unsigned long size);

/* Returns BD ring offset for a particular channel
   (in 'struct mal_descriptor' elements)
*/
int mal_tx_bd_offset(struct mal_instance *mal, int channel);
int mal_rx_bd_offset(struct mal_instance *mal, int channel);

void mal_poll_disable(struct mal_instance *mal, struct mal_commac *commac);
void mal_poll_enable(struct mal_instance *mal, struct mal_commac *commac);

/* Add/remove EMAC to/from MAL polling list */
void mal_poll_add(struct mal_instance *mal, struct mal_commac *commac);
void mal_poll_del(struct mal_instance *mal, struct mal_commac *commac);

/* Error parsing */
struct emac_instance;
void mal_parse_rx_error(struct emac_instance *dev, struct mal_descriptor *desc);
void mal_parse_tx_error(struct emac_instance *dev, struct mal_descriptor *desc);

extern void mal_enable_tx_channel(struct mal_instance *mal, int channel);
extern void mal_disable_tx_channel(struct mal_instance *mal, int channel);
extern void mal_enable_rx_channel(struct mal_instance *mal, int channel);
extern void mal_disable_rx_channel(struct mal_instance *mal, int channel);
extern void mal_enable_eob_irq(struct mal_instance *mal);
extern void mal_disable_eob_irq(struct mal_instance *mal);
extern void mal_enable_serr(struct mal_instance *mal);
extern void mal_set_tx_descriptor(struct mal_instance *mal, int channel, unsigned long vaddr, dma_addr_t paddr);
extern void mal_set_rx_descriptor(struct mal_instance *mal, int channel, unsigned long vaddr, dma_addr_t paddr);
extern int mal_set_rcbs(struct mal_instance *mal, int channel, unsigned long size);
extern void mal_configure (struct mal_instance *mal, struct of_device *ofdev);
extern void *mal_dump_regs(struct mal_instance *mal, void *buf);
extern u32 mal_ack_txeobisr(struct mal_instance *mal, int channel);
extern u32 mal_ack_rxeobisr(struct mal_instance *mal, int channel);
extern u32 mal_ack_txdeir(struct mal_instance *mal, int channel);
extern u32 mal_ack_rxdeir(struct mal_instance *mal, int channel);
extern u32 mal_ack_esr(struct mal_instance *mal, int channel);
extern void mal_reset(struct mal_instance *mal);
#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
extern void mal_reinit(struct mal_instance *mal);
#endif
extern int mal_add_xmit(struct mal_instance *mal, int channel, unsigned long packets);
extern void mal_announce_rxdesc(struct mal_instance *mal, int channel, unsigned long bytes, int rx_size);
#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
extern void tomal_proc_register(struct mal_instance *mal);
extern void tomal_proc_unregister(struct mal_instance *mal);
#endif

/* Ethtool MAL registers */
struct mal_regs {
	u32 tx_count;
	u32 rx_count;

	u32 cfg;
	u32 esr;
	u32 ier;
	u32 tx_casr;
	u32 tx_carr;
	u32 tx_eobisr;
	u32 tx_deir;
	u32 rx_casr;
	u32 rx_carr;
	u32 rx_eobisr;
	u32 rx_deir;
	u32 tx_ctpr[32];
	u32 rx_ctpr[32];
	u32 rcbs[32];
};

int mal_get_regs_len(struct mal_instance *mal);
void *mal_dump_regs(struct mal_instance *mal, void *buf);

#endif /* __IBM_NEWEMAC_MAL_H */
