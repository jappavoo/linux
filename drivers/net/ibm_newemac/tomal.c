/*
 * drivers/net/ibm_newemac/tomal.c
 *
 * Memory Access Layer (TOMAL) support
 *
 * Copyright (c) 2004, 2005 Zultys Technologies.
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 *
 * Based on original work by
 *      Benjamin Herrenschmidt <benh@kernel.crashing.org>,
 *      David Gibson <hermes@gibson.dropbear.id.au>,
 *
 *      Armin Kuster <akuster@mvista.com>
 *      Copyright 2002 MontaVista Softare Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

//#include <linux/delay.h>

extern inline u32 get_tomal_reg(struct mal_instance *mal, int reg)
{
	BUG_ON(reg >= TOMAL_REG_SIZE);
	return in_be32((void*)mal->io_base + reg);
}

extern inline void set_tomal_reg(struct mal_instance *mal, int reg, u32 val)
{
	BUG_ON(reg >= TOMAL_REG_SIZE);
	//printk("TOMAL: W %04x, %08x\n", reg, val);
	out_be32((void*)mal->io_base + reg, val);
}

void mal_enable_tx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "enable_tx(%d)" NL, channel);

	set_tomal_reg(mal, TOMAL_CFG,
		      get_tomal_reg(mal, TOMAL_CFG) | TOMAL_CFG_TX_MAC(channel));

	spin_unlock_irqrestore(&mal->lock, flags);
}

void mal_disable_tx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;
	spin_lock_irqsave(&mal->lock, flags);

	set_tomal_reg(mal, TOMAL_CFG,
		      get_tomal_reg(mal, TOMAL_CFG) & ~TOMAL_CFG_TX_MAC(channel));

	MAL_DBG(mal, "disable_tx(%d)" NL, channel);

	spin_unlock_irqrestore(&mal->lock, flags);
}

void mal_enable_rx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "enable_rx(%d)" NL, channel);

	set_tomal_reg(mal, TOMAL_CFG,
		     get_tomal_reg(mal, TOMAL_CFG) | TOMAL_CFG_RX_MAC(channel));

	spin_unlock_irqrestore(&mal->lock, flags);
}

void mal_disable_rx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	set_tomal_reg(mal, TOMAL_CFG,
		      get_tomal_reg(mal, TOMAL_CFG) & ~TOMAL_CFG_RX_MAC(channel));

	MAL_DBG(mal, "disable_rx(%d)" NL, channel);

	spin_unlock_irqrestore(&mal->lock, flags);
}

static void __mal_enable_eob_irq(struct mal_instance *mal, int channel)
{
	set_tomal_reg(mal, TOMAL_TX_MASK(channel), 1);
	set_tomal_reg(mal, TOMAL_RX_MASK(channel), 1);
}

static void __mal_disable_eob_irq(struct mal_instance *mal, int channel)
{
	set_tomal_reg(mal, TOMAL_TX_MASK(channel), 0);
	set_tomal_reg(mal, TOMAL_RX_MASK(channel), 0);
}

/* synchronized by mal_poll() */
void mal_enable_eob_irq(struct mal_instance *mal)
{
	MAL_DBG2(mal, "enable_irq" NL);
	set_tomal_reg(mal, TOMAL_RX_NOTIFY,
		      TOMAL_RX_NOTIFY_COUNTER(0) | TOMAL_RX_NOTIFY_COUNTER(1));

	__mal_enable_eob_irq(mal, 0);
	__mal_enable_eob_irq(mal, 1);

}

/* synchronized by __LINK_STATE_RX_SCHED bit in ndev->state */
void mal_disable_eob_irq(struct mal_instance *mal)
{
	__mal_disable_eob_irq(mal, 0);
	__mal_disable_eob_irq(mal, 1);

	MAL_DBG2(mal, "disable_irq" NL);
}

static void __mal_enable_serr(struct mal_instance *mal, int channel)
{
	set_tomal_reg(mal, TOMAL_SWERR_ENABLE(channel), TOMAL_SWERR_EVENTS);
	set_tomal_reg(mal, TOMAL_SWERR_IRQMASK(channel), TOMAL_SWERR_EVENTS);
}

void mal_enable_serr(struct mal_instance *mal)
{
	__mal_enable_serr(mal, 0);
	__mal_enable_serr(mal, 1);

	set_tomal_reg(mal, TOMAL_HWERR_ENABLE, TOMAL_HWERR_MASK);
	set_tomal_reg(mal, TOMAL_HWERR_IRQMASK, TOMAL_HWERR_MASK);

	set_tomal_reg(mal, TOMAL_CRITERR_ENABLE, TOMAL_CRITERR_MASK);
	set_tomal_reg(mal, TOMAL_CRITERR_IRQMASK, TOMAL_CRITERR_MASK);
}

static void __mal_disable_serr(struct mal_instance *mal, int channel)
{
	set_tomal_reg(mal, TOMAL_SWERR_ENABLE(channel), 0);
	set_tomal_reg(mal, TOMAL_SWERR_IRQMASK(channel), 0);
}

void mal_disable_serr(struct mal_instance *mal)
{
	__mal_disable_serr(mal, 0);
	__mal_disable_serr(mal, 1);

	set_tomal_reg(mal, TOMAL_HWERR_ENABLE, 0);
	set_tomal_reg(mal, TOMAL_HWERR_IRQMASK, 0);

	set_tomal_reg(mal, TOMAL_CRITERR_ENABLE, 0);
	set_tomal_reg(mal, TOMAL_CRITERR_IRQMASK, 0);
}

void mal_set_tx_descriptor(struct mal_instance *mal, int channel,
			   unsigned long vaddr, dma_addr_t paddr)
{
	struct mal_descriptor *wrap_desc = (struct mal_descriptor*)vaddr + NUM_TX_BUFF;

	BUG_ON(channel < 0 || channel >= mal->num_tx_chans);

	MAL_DBG(mal, "TX desc: v=%lx, p=%llx, wrap=%p\n", vaddr, paddr, wrap_desc);

	set_tomal_reg(mal, TOMAL_TX_CURR_DESC_HIGH(channel), paddr >> 32);
	set_tomal_reg(mal, TOMAL_TX_CURR_DESC_LOW(channel), paddr & 0xffffffff);

	MAL_DBG(mal, "TX desc: h=%08x, l=%08x\n",
		get_tomal_reg(mal, TOMAL_TX_CURR_DESC_HIGH(channel)),
		get_tomal_reg(mal, TOMAL_TX_CURR_DESC_LOW(channel)));

	wrap_desc->ctrl = TOMAL_DESC_CODE_BRANCH;
	wrap_desc->data_ptr = paddr;
}

void mal_set_rx_descriptor(struct mal_instance *mal, int channel,
			   unsigned long vaddr, dma_addr_t paddr)
{
	struct mal_descriptor *wrap_desc = (struct mal_descriptor*)vaddr + NUM_RX_BUFF;

	BUG_ON(channel < 0 || channel >= mal->num_rx_chans);

	MAL_DBG(mal, "RX desc: v=%lx, p=%llx, wrap=%p\n", vaddr, paddr, wrap_desc);

	set_tomal_reg(mal, TOMAL_RX_CURR_DESC_HIGH(channel), paddr >> 32);
	set_tomal_reg(mal, TOMAL_RX_CURR_DESC_LOW(channel), paddr & 0xffffffff);

	wrap_desc->ctrl = TOMAL_DESC_CODE_BRANCH;
	wrap_desc->data_ptr = paddr;
}

void mal_announce_rxdesc(struct mal_instance *mal, int channel,
			 unsigned long count, int rx_size)
{
	unsigned long pending;

	BUG_ON(channel < 0 || channel > TOMAL_MAX_CHANNEL);

	mal->tomal_rx_bufs[channel] += count;

	pending = get_tomal_reg(mal, TOMAL_RX_TOTAL_BYTES(channel));
	count = mal->tomal_rx_bufs[channel];
	if (count * rx_size + pending >= TOMAL_MAX_TOTAL_RX_BYTES)
		count = (TOMAL_MAX_TOTAL_RX_BYTES - pending - 1) / rx_size;

	BUG_ON(count * rx_size + pending >= TOMAL_MAX_TOTAL_RX_BYTES);

	mal->tomal_rx_bufs[channel] -= count;
	while (count > 0) {
	    int n = (count * rx_size < TOMAL_MAX_ADD_RX_BYTES) ?
			count : ((TOMAL_MAX_ADD_RX_BYTES - 1) / rx_size);
	    set_tomal_reg(mal, TOMAL_RX_ADD_FREE_BYTES(channel), n * rx_size);
	    count -= n;
	}
}

int mal_set_rcbs(struct mal_instance *mal, int channel, unsigned long size)
{
	BUG_ON(channel < 0 || channel >= mal->num_rx_chans ||
	       size > MAL_MAX_RX_SIZE);

	MAL_DBG(mal, "set_rbcs(%d, %lu)" NL, channel, size);
	mal->tomal_rx_size[channel] = size;
	mal->tomal_rx_bufs[channel] = 0;

	return 0;
}

int mal_add_xmit(struct mal_instance *mal, int channel, unsigned long packets)
{
	MAL_DBG2(mal, "add %ld packets to xmit chn %d\n", packets, channel);
	set_tomal_reg(mal, TOMAL_TX_POSTED_FRAMES(channel), packets);
	set_tomal_reg(mal, TOMAL_TX_NOTIFY, TOMAL_TX_NOTIFY_COUNTER(channel));

	MAL_DBG2(mal, "pending: %x, transmitted: %x, tx status: %x, tx free: %x\n",
		 get_tomal_reg(mal, TOMAL_TX_PEND_FRAMES(channel)),
		 get_tomal_reg(mal, TOMAL_TX_TRANSMITTED_FRAMES(channel)),
		 get_tomal_reg(mal, TOMAL_TX_STATUS(channel)),
		 get_tomal_reg(mal, TOMAL_TX_DATABUF_FREE(channel)));
	return 0;
}


// tx end of buffers
u32 mal_ack_txeobisr(struct mal_instance *mal, int channel)
{
	u32 r = get_tomal_reg(mal, TOMAL_TX_STATUS(channel));
	set_tomal_reg(mal, TOMAL_TX_STATUS(channel), r);
	return r;
}

// rx end off buffers
u32 mal_ack_rxeobisr(struct mal_instance *mal, int channel)
{
	u32 r = get_tomal_reg(mal, TOMAL_RX_STATUS(channel));
	set_tomal_reg(mal, TOMAL_RX_STATUS(channel), r);
	return r;
}

// ack tx descriptor error
u32 mal_ack_txdeir(struct mal_instance *mal, int channel)
{
	u32 r = get_tomal_reg(mal, TOMAL_TX_MAC_STATUS(channel));
	set_tomal_reg(mal, TOMAL_TX_MAC_STATUS(channel), r);
	return r;
}

// ack rx descriptor error
u32 mal_ack_rxdeir(struct mal_instance *mal, int channel)
{
	u32 r = get_tomal_reg(mal, TOMAL_RX_MAC_STATUS(channel));
	set_tomal_reg(mal, TOMAL_RX_MAC_STATUS(channel), r);
	return r;
}

// software error
u32 mal_ack_esr(struct mal_instance *mal, int channel)
{
	u32 esr = get_tomal_reg(mal, TOMAL_SWERR_STATUS(channel));
	set_tomal_reg(mal, TOMAL_SWERR_STATUS(channel), esr);
	return esr;
}

void mal_reset(struct mal_instance *mal)
{
	int n = 50;

	MAL_DBG(mal, "tomal reset" NL);

	set_tomal_reg(mal, TOMAL_CFG, TOMAL_CFG_SOFT_RESET);

	/* Wait for reset to complete (1 system clock) */
	while ((get_tomal_reg(mal, TOMAL_CFG) & TOMAL_CFG_SOFT_RESET) && n--)
		udelay(100);

	if (unlikely(!n))
		printk(KERN_ERR "tomal%d: reset timeout\n", mal->index);
}

void mal_configure (struct mal_instance *mal, struct of_device *ofdev)
{
	set_tomal_reg(mal, TOMAL_CFG, TOMAL_CFG_PLB_FREQ_250 |
		      TOMAL_CFG_TX_MAC(0) | TOMAL_CFG_RX_MAC(0) |
		      TOMAL_CFG_TX_MAC(1) | TOMAL_CFG_RX_MAC(1));

	set_tomal_reg(mal, TOMAL_PACKET_DATA_ENGINE,
		      TOMAL_PDE_RX_PREFETCH_1 | TOMAL_PDE_TX_PREFETCH_1);

	// route channel #1 IRQs to TOE_PLB_INT[1]
	set_tomal_reg(mal, TOMAL_IRQ_ROUTE,
		      TOMAL_IRQ_TX(1) | TOMAL_IRQ_RX(1) |
		      TOMAL_IRQ_TX_MACERR(1) | TOMAL_IRQ_SW_ERR(1));

	set_tomal_reg(mal, TOMAL_CONSUMER_BASE_ADDR, 0);
}

void *mal_dump_regs(struct mal_instance *mal, void *buf)
{
	struct emac_ethtool_regs_subhdr *hdr = buf;
	struct mal_regs *regs = (struct mal_regs *)(hdr + 1);

	hdr->version = mal->version;
	hdr->index = mal->index;

	regs->tx_count = mal->num_tx_chans;
	regs->rx_count = mal->num_rx_chans;

	return regs + 1;
}

static irqreturn_t tomal_irq(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	int channel = (irq == mal->tomal_irq0) ? 0 : 1;
	u32 status, isr = get_tomal_reg(mal, TOMAL_IRQ_STATUS);

	if (isr & (TOMAL_IRQ_RX(0) | TOMAL_IRQ_RX(1)))
	{
		mal_rxeob(irq, dev_instance);
		/* if we have unannounced descriptors put them in
		 * early to keep the card humming... */
		if (mal->tomal_rx_bufs[channel])
			mal_announce_rxdesc(mal, channel, 0,
					    mal->tomal_rx_size[channel]);
	}

	if (isr & (TOMAL_IRQ_TX(0) | TOMAL_IRQ_TX(1)))
	{
		//printk("TOMAL TX interrupt\n");
		mal_txeob(irq, dev_instance);
	}

	if (isr & TOMAL_IRQ_TX_MACERR(channel))
	{
		status = get_tomal_reg(mal, TOMAL_TX_MAC_STATUS(channel));
		MAL_DBG(mal, "TX MAC error: %08x\n", status);
	}

	if (isr & TOMAL_IRQ_RX_MACERR(channel))
	{
		status = get_tomal_reg(mal, TOMAL_RX_MAC_STATUS(channel));
		MAL_DBG(mal, "RX MAC error: %08x\n", status);
	}

	if (isr & TOMAL_IRQ_SW_ERR(channel))
	{
		// Short packets masked, only watch incorrect TX desc
		status = mal_ack_esr(mal, channel);
		MAL_DBG(mal, "SW error: %08x\n", status);
	}

	if (isr & TOMAL_IRQ_CRITICAL_ERR)
	{
		status = get_tomal_reg(mal, TOMAL_CRITERR_STATUS);
		set_tomal_reg(mal, TOMAL_CRITERR_STATUS, status);

		MAL_DBG(mal, "Critical error: %08x (REC: %x, TX: %x)\n", status,
			get_tomal_reg(mal, TOMAL_CRITERR_BAD_RXDESC),
			get_tomal_reg(mal, TOMAL_CRITERR_BAD_TXDESC));
	}

	return IRQ_HANDLED;
}

/**********************************************************************
 *                           sysctl interface
 **********************************************************************/

#if defined(CONFIG_SYSCTL) && defined(CONFIG_PROC_FS)

/*
 * XXX This interface deals with channel 0 only, for now.
 */

static struct mal_instance *__mal = NULL;

static int doreg(ctl_table *ctl, int write, struct file * filp,
		 void __user *buffer, size_t *lenp, loff_t *ppos,
		 int *valp, int reg)
{
    int rc;
    rc = proc_dointvec_minmax(ctl, write, filp, buffer, lenp, ppos);
    if (rc == 0) {
	set_tomal_reg(__mal, reg, (*valp));
    } else {
	(*valp) = get_tomal_reg(__mal, reg);
    }
    return rc;
}

#define SYSCTL_DOREG(name, reg, min, max)				\
    static int name;							\
    static int name ## _min = min;					\
    static int name ## _max = max;					\
    static int proc_do_ ## name(ctl_table *ctl, int write,		\
				struct file * filp,			\
				void __user *buffer,			\
				size_t *lenp, loff_t *ppos)		\
    {									\
	return doreg(ctl, write, filp, buffer, lenp, ppos,		\
		     &name, reg(0));					\
    }

SYSCTL_DOREG(tx_raw_min_timer,         TOMAL_TX_MIN_TIMER,      0, 255);
SYSCTL_DOREG(tx_raw_max_timer,         TOMAL_TX_MAX_TIMER,      0, 255);
SYSCTL_DOREG(tx_raw_max_frame_num,     TOMAL_TX_MAX_FRAME,      0, 65535);
SYSCTL_DOREG(tx_raw_min_frame_num,     TOMAL_TX_MIN_FRAME,      0, 255);
SYSCTL_DOREG(tx_raw_frame_per_service, TOMAL_TX_FRAME_PER_SERV, 1, 15);
SYSCTL_DOREG(rx_raw_min_timer,         TOMAL_RX_MIN_TIMER,      0, 255);
SYSCTL_DOREG(rx_raw_max_timer,         TOMAL_RX_MAX_TIMER,      0, 255);
SYSCTL_DOREG(rx_raw_max_frame_num,     TOMAL_RX_MAX_FRAME,      0, 65535);
SYSCTL_DOREG(rx_raw_min_frame_num,     TOMAL_RX_MIN_FRAME,      0, 65535);
SYSCTL_DOREG(rx_raw_dropped_frames,    TOMAL_RX_DROPPED_FRAMES, 0, 255);

#ifdef CONFIG_EMAC_MULTICAST_TAGGING
int proc_do_mcast_tagging(ctl_table *ctl, int write, struct file *filp,
			  void __user *buffer, size_t *lenp, loff_t *ppos);
#endif /* CONFIG_EMAC_MULTICAST_TAGGING */

#define SYSCTL_REG(name) 			\
    {						\
	.ctl_name = CTL_UNNUMBERED,		\
	.procname = #name,			\
	.data = &name,				\
	.maxlen = sizeof(int),			\
	.mode = 0644,				\
	.proc_handler = proc_do_ ## name,	\
	.extra1 = &name ## _min,		\
	.extra2 = &name ## _max,		\
    }

static struct ctl_table tomal_table[] = {
    SYSCTL_REG(tx_raw_min_timer),
    SYSCTL_REG(tx_raw_max_timer),
    SYSCTL_REG(tx_raw_max_frame_num),
    SYSCTL_REG(tx_raw_min_frame_num),
    SYSCTL_REG(tx_raw_frame_per_service),
    SYSCTL_REG(rx_raw_min_timer),
    SYSCTL_REG(rx_raw_max_timer),
    SYSCTL_REG(rx_raw_max_frame_num),
    SYSCTL_REG(rx_raw_min_frame_num),
    SYSCTL_REG(rx_raw_dropped_frames),
#ifdef CONFIG_EMAC_MULTICAST_TAGGING
    {
	.ctl_name = CTL_UNNUMBERED,
	.procname = "mcast_tagging",
	.mode = 0644,
	.proc_handler = proc_do_mcast_tagging,
    },
#endif /* CONFIG_EMAC_MULTICAST_TAGGING */
    { .ctl_name = 0 }
};

static struct ctl_table tomal_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "tomal",
	.mode		= 0555,
	.child		= tomal_table,
    },
    { .ctl_name = 0 }
};

static struct ctl_table dev_root[] = {
    {
	.ctl_name	= CTL_DEV,
	.procname	= "dev",
	.maxlen		= 0,
	.mode		= 0555,
	.child		= tomal_root,
    },
    { .ctl_name = 0 }
};

void tomal_proc_register(struct mal_instance *mal)
{
    mal->sysctl_header = register_sysctl_table(dev_root);
    tx_raw_min_timer =         get_tomal_reg(mal, TOMAL_TX_MIN_TIMER(0));
    tx_raw_max_timer =         get_tomal_reg(mal, TOMAL_TX_MAX_TIMER(0));
    tx_raw_max_frame_num =     get_tomal_reg(mal, TOMAL_TX_MAX_FRAME(0));
    tx_raw_min_frame_num =     get_tomal_reg(mal, TOMAL_TX_MIN_FRAME(0));
    tx_raw_frame_per_service = get_tomal_reg(mal, TOMAL_TX_FRAME_PER_SERV(0));
    rx_raw_min_timer =         get_tomal_reg(mal, TOMAL_RX_MIN_TIMER(0));
    rx_raw_max_timer =         get_tomal_reg(mal, TOMAL_RX_MAX_TIMER(0));
    rx_raw_max_frame_num =     get_tomal_reg(mal, TOMAL_RX_MAX_FRAME(0));
    rx_raw_min_frame_num =     get_tomal_reg(mal, TOMAL_RX_MIN_FRAME(0));
    rx_raw_dropped_frames =    get_tomal_reg(mal, TOMAL_RX_DROPPED_FRAMES(0));
    __mal = mal;
}

void tomal_proc_unregister(struct mal_instance *mal)
{
    if (mal->sysctl_header != NULL) {
	unregister_sysctl_table(mal->sysctl_header);
	mal->sysctl_header = NULL;
    }
    __mal = NULL;
}

#endif /* PROC && SYSCTL */
