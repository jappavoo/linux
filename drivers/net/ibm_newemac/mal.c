/*
 * drivers/net/ibm_newemac/mal.c
 *
 * Memory Access Layer (MAL) support
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

#include <linux/delay.h>

#include "core.h"

static int mal_count;

static irqreturn_t mal_txeob(int irq, void *dev_instance);
static irqreturn_t mal_rxeob(int irq, void *dev_instance);
static irqreturn_t mal_txde(int irq, void *dev_instance);
static irqreturn_t mal_rxde(int irq, void *dev_instance);

int __devinit mal_register_commac(struct mal_instance	*mal,
				  struct mal_commac	*commac)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "reg(%08x, %08x)" NL,
		commac->tx_chan_mask, commac->rx_chan_mask);

	/* Don't let multiple commacs claim the same channel(s) */
	if ((mal->tx_chan_mask & commac->tx_chan_mask) ||
	    (mal->rx_chan_mask & commac->rx_chan_mask)) {
		spin_unlock_irqrestore(&mal->lock, flags);
		printk(KERN_WARNING "mal%d: COMMAC channels conflict!\n",
		       mal->index);
		return -EBUSY;
	}

	mal->tx_chan_mask |= commac->tx_chan_mask;
	mal->rx_chan_mask |= commac->rx_chan_mask;
	list_add(&commac->list, &mal->list);

	spin_unlock_irqrestore(&mal->lock, flags);

	return 0;
}

void __devexit mal_unregister_commac(struct mal_instance	*mal,
				     struct mal_commac		*commac)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "unreg(%08x, %08x)" NL,
		commac->tx_chan_mask, commac->rx_chan_mask);

	mal->tx_chan_mask &= ~commac->tx_chan_mask;
	mal->rx_chan_mask &= ~commac->rx_chan_mask;
	list_del_init(&commac->list);

	spin_unlock_irqrestore(&mal->lock, flags);
}

int mal_tx_bd_offset(struct mal_instance *mal, int channel)
{
	BUG_ON(channel < 0 || channel >= mal->num_tx_chans);

	return channel * (NUM_TX_BUFF + NUM_WRAP_DESC);
}

int mal_rx_bd_offset(struct mal_instance *mal, int channel)
{
	BUG_ON(channel < 0 || channel >= mal->num_rx_chans);
	return mal->num_tx_chans * (NUM_TX_BUFF + NUM_WRAP_DESC) +
	    channel * (NUM_RX_BUFF + NUM_WRAP_DESC);
}


#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
#include "tomal.c"
#else

void mal_parse_rx_error(struct emac_instance *dev, struct mal_descriptor *desc)
{
	struct emac_error_stats *st = &dev->estats;
	u16 ctrl = desc->ctrl;

	DBG(dev, "BD RX error %04x" NL, ctrl);

	++st->rx_bd_errors;
	if (ctrl & EMAC_RX_ST_OE)
		++st->rx_bd_overrun;
	if (ctrl & EMAC_RX_ST_BP)
		++st->rx_bd_bad_packet;
	if (ctrl & EMAC_RX_ST_RP)
		++st->rx_bd_runt_packet;
	if (ctrl & EMAC_RX_ST_SE)
		++st->rx_bd_short_event;
	if (ctrl & EMAC_RX_ST_AE)
		++st->rx_bd_alignment_error;
	if (ctrl & EMAC_RX_ST_BFCS)
		++st->rx_bd_bad_fcs;
	if (ctrl & EMAC_RX_ST_PTL)
		++st->rx_bd_packet_too_long;
	if (ctrl & EMAC_RX_ST_ORE)
		++st->rx_bd_out_of_range;
	if (ctrl & EMAC_RX_ST_IRE)
		++st->rx_bd_in_range;
}

/* Tx lock BHs */
void mal_parse_tx_error(struct emac_instance *dev, struct mal_descriptor *desc)
{
	struct emac_error_stats *st = &dev->estats;
	u16 ctrl = desc->ctrl;

	DBG(dev, "BD TX error %04x" NL, ctrl);

	++st->tx_bd_errors;
	if (ctrl & EMAC_TX_ST_BFCS)
		++st->tx_bd_bad_fcs;
	if (ctrl & EMAC_TX_ST_LCS)
		++st->tx_bd_carrier_loss;
	if (ctrl & EMAC_TX_ST_ED)
		++st->tx_bd_excessive_deferral;
	if (ctrl & EMAC_TX_ST_EC)
		++st->tx_bd_excessive_collisions;
	if (ctrl & EMAC_TX_ST_LC)
		++st->tx_bd_late_collision;
	if (ctrl & EMAC_TX_ST_MC)
		++st->tx_bd_multple_collisions;
	if (ctrl & EMAC_TX_ST_SC)
		++st->tx_bd_single_collision;
	if (ctrl & EMAC_TX_ST_UR)
		++st->tx_bd_underrun;
	if (ctrl & EMAC_TX_ST_SQE)
		++st->tx_bd_sqe;
}

static inline u32 get_mal_dcrn(struct mal_instance *mal, int reg)
{
	return dcr_read(mal->dcr_host, mal->dcr_base + reg);
}

static inline void set_mal_dcrn(struct mal_instance *mal, int reg, u32 val)
{
	dcr_write(mal->dcr_host, mal->dcr_base + reg, val);
}

int mal_set_rcbs(struct mal_instance *mal, int channel, unsigned long size)
{
	BUG_ON(channel < 0 || channel >= mal->num_rx_chans ||
	       size > MAL_MAX_RX_SIZE);

	MAL_DBG(mal, "set_rbcs(%d, %lu)" NL, channel, size);

	if (size & 0xf) {
		printk(KERN_WARNING
		       "mal%d: incorrect RX size %lu for the channel %d\n",
		       mal->index, size, channel);
		return -EINVAL;
	}

	set_mal_dcrn(mal, MAL_RCBS(channel), size >> 4);
	return 0;
}

void mal_enable_tx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "enable_tx(%d)" NL, channel);

	set_mal_dcrn(mal, MAL_TXCASR,
		     get_mal_dcrn(mal, MAL_TXCASR) | MAL_CHAN_MASK(channel));

	spin_unlock_irqrestore(&mal->lock, flags);
}

void mal_disable_tx_channel(struct mal_instance *mal, int channel)
{
	set_mal_dcrn(mal, MAL_TXCARR, MAL_CHAN_MASK(channel));

	MAL_DBG(mal, "disable_tx(%d)" NL, channel);
}

void mal_enable_rx_channel(struct mal_instance *mal, int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "enable_rx(%d)" NL, channel);

	set_mal_dcrn(mal, MAL_RXCASR,
		     get_mal_dcrn(mal, MAL_RXCASR) | MAL_CHAN_MASK(channel));

	spin_unlock_irqrestore(&mal->lock, flags);
}

void mal_disable_rx_channel(struct mal_instance *mal, int channel)
{
	set_mal_dcrn(mal, MAL_RXCARR, MAL_CHAN_MASK(channel));

	MAL_DBG(mal, "disable_rx(%d)" NL, channel);
}

void mal_enable_serr(struct mal_instance *mal)
{
	set_mal_dcrn(mal, MAL_IER, (mal->version == 1) ? MAL1_IER_EVENTS : MAL2_IER_EVENTS);
}

/* synchronized by mal_poll() */
void mal_enable_eob_irq(struct mal_instance *mal)
{
	MAL_DBG2(mal, "enable_irq" NL);

	// XXX might want to cache MAL_CFG as the DCR read can be slooooow
	set_mal_dcrn(mal, MAL_CFG, get_mal_dcrn(mal, MAL_CFG) | MAL_CFG_EOPIE);
}

/* synchronized by __LINK_STATE_RX_SCHED bit in ndev->state */
void mal_disable_eob_irq(struct mal_instance *mal)
{
	// XXX might want to cache MAL_CFG as the DCR read can be slooooow
	set_mal_dcrn(mal, MAL_CFG, get_mal_dcrn(mal, MAL_CFG) & ~MAL_CFG_EOPIE);

	MAL_DBG2(mal, "disable_irq" NL);
}

void mal_set_tx_descriptor(struct mal_instance *mal, int channel,
			   unsigned long vaddr, dma_addr_t addr)
{
	BUG_ON(channel < 0 || channel >= mal->num_tx_chans);

	set_mal_dcrn(mal, MAL_TXCTPR(channel), addr);
}

void mal_set_rx_descriptor(struct mal_instance *mal, int channel,
			   unsigned long vaddr, dma_addr_t addr)
{
	BUG_ON(channel < 0 || channel >= mal->num_rx_chans);

	set_mal_dcrn(mal, MAL_RXCTPR(channel), addr);
}

u32 mal_ack_txeobisr(struct mal_instance *mal, int channel)
{
	u32 r = get_mal_dcrn(mal, MAL_TXEOBISR);
	set_mal_dcrn(mal, MAL_TXEOBISR, r);
	return r;
}

u32 mal_ack_rxeobisr(struct mal_instance *mal, int channel)
{
	u32 r = get_mal_dcrn(mal, MAL_RXEOBISR);
	set_mal_dcrn(mal, MAL_RXEOBISR, r);
	return r;
}

u32 mal_ack_txdeir(struct mal_instance *mal, int channel)
{
	u32 deir = get_mal_dcrn(mal, MAL_TXDEIR);
	set_mal_dcrn(mal, MAL_TXDEIR, deir);
	return deir;
}

u32 mal_ack_rxdeir(struct mal_instance *mal, int channel)
{
	u32 deir = get_mal_dcrn(mal, MAL_RXDEIR);
	set_mal_dcrn(mal, MAL_RXDEIR, deir);
	return deir;
}

u32 mal_ack_esr(struct mal_instance *mal, int channel)
{
	u32 esr = get_mal_dcrn(mal, MAL_ESR);

	/* Clear the error status register */
	set_mal_dcrn(mal, MAL_ESR, esr);
	return esr;
}

void mal_reset(struct mal_instance *mal)
{
	int n = 10;

	MAL_DBG(mal, "reset" NL);

	set_mal_dcrn(mal, MAL_CFG, MAL_CFG_SR);

	/* Wait for reset to complete (1 system clock) */
	while ((get_mal_dcrn(mal, MAL_CFG) & MAL_CFG_SR) && n)
		--n;

	if (unlikely(!n))
		printk(KERN_ERR "mal%d: reset timeout\n", mal->index);
}

void mal_configure (struct mal_instance *mal, struct of_device *ofdev)
{
	/* Set the MAL configuration register */
	cfg = mal->version == 1 ? MAL1_CFG_DEFAULT : MAL2_CFG_DEFAULT;
	cfg |= MAL_CFG_PLBB | MAL_CFG_OPBBL | MAL_CFG_LEA;

	/* Current Axon is not happy with priority being non-0, it can
	 * deadlock, fix it up here
	 */
	if (device_is_compatible(ofdev->node, "ibm,mcmal-axon"))
		cfg &= ~(MAL2_CFG_RPP_10 | MAL2_CFG_WPP_10);

	/* Apply configuration */
	set_mal_dcrn(mal, MAL_CFG, cfg);
}

void *mal_dump_regs(struct mal_instance *mal, void *buf)
{
	struct emac_ethtool_regs_subhdr *hdr = buf;
	struct mal_regs *regs = (struct mal_regs *)(hdr + 1);
	int i;

	hdr->version = mal->version;
	hdr->index = mal->index;

	regs->tx_count = mal->num_tx_chans;
	regs->rx_count = mal->num_rx_chans;

	regs->cfg = get_mal_dcrn(mal, MAL_CFG);
	regs->esr = get_mal_dcrn(mal, MAL_ESR);
	regs->ier = get_mal_dcrn(mal, MAL_IER);
	regs->tx_casr = get_mal_dcrn(mal, MAL_TXCASR);
	regs->tx_carr = get_mal_dcrn(mal, MAL_TXCARR);
	regs->tx_eobisr = get_mal_dcrn(mal, MAL_TXEOBISR);
	regs->tx_deir = get_mal_dcrn(mal, MAL_TXDEIR);
	regs->rx_casr = get_mal_dcrn(mal, MAL_RXCASR);
	regs->rx_carr = get_mal_dcrn(mal, MAL_RXCARR);
	regs->rx_eobisr = get_mal_dcrn(mal, MAL_RXEOBISR);
	regs->rx_deir = get_mal_dcrn(mal, MAL_RXDEIR);

	for (i = 0; i < regs->tx_count; ++i)
		regs->tx_ctpr[i] = get_mal_dcrn(mal, MAL_TXCTPR(i));

	for (i = 0; i < regs->rx_count; ++i) {
		regs->rx_ctpr[i] = get_mal_dcrn(mal, MAL_RXCTPR(i));
		regs->rcbs[i] = get_mal_dcrn(mal, MAL_RCBS(i));
	}
	return regs + 1;
}

#endif /* !CONFIG_IBM_NEW_EMAC_TOMAL */

int mal_get_regs_len(struct mal_instance *mal)
{
	return sizeof(struct emac_ethtool_regs_subhdr) +
		sizeof(struct mal_regs);
}

// OK
void mal_poll_add(struct mal_instance *mal, struct mal_commac *commac)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "poll_add(%p)" NL, commac);

	/* starts disabled */
	set_bit(MAL_COMMAC_POLL_DISABLED, &commac->flags);

	list_add_tail(&commac->poll_list, &mal->poll_list);

	spin_unlock_irqrestore(&mal->lock, flags);
}

// OK
void mal_poll_del(struct mal_instance *mal, struct mal_commac *commac)
{
	unsigned long flags;

	spin_lock_irqsave(&mal->lock, flags);

	MAL_DBG(mal, "poll_del(%p)" NL, commac);

	list_del(&commac->poll_list);

	spin_unlock_irqrestore(&mal->lock, flags);
}

static irqreturn_t mal_serr(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	int channel = 0;

	u32 esr = mal_ack_esr(mal, channel);

	MAL_DBG(mal, "SERR %08x" NL, esr);

#ifndef CONFIG_IBM_NEW_EMAC_TOMAL
	if (esr & MAL_ESR_EVB) {
		if (esr & MAL_ESR_DE) {
			/* We ignore Descriptor error,
			 * TXDE or RXDE interrupt will be generated anyway.
			 */
			return IRQ_HANDLED;
		}

		if (esr & MAL_ESR_PEIN) {
			/* PLB error, it's probably buggy hardware or
			 * incorrect physical address in BD (i.e. bug)
			 */
			if (net_ratelimit())
				printk(KERN_ERR
				       "mal%d: system error, "
				       "PLB (ESR = 0x%08x)\n",
				       mal->index, esr);
			return IRQ_HANDLED;
		}

		/* OPB error, it's probably buggy hardware or incorrect
		 * EBC setup
		 */
		if (net_ratelimit())
			printk(KERN_ERR
			       "mal%d: system error, OPB (ESR = 0x%08x)\n",
			       mal->index, esr);
	}
#endif
	return IRQ_HANDLED;
}

static inline void mal_schedule_poll(struct mal_instance *mal)
{
	if (likely(netif_rx_schedule_prep(&mal->poll_dev))) {
		MAL_DBG2(mal, "schedule_poll" NL);
		mal_disable_eob_irq(mal);
		__netif_rx_schedule(&mal->poll_dev);
	} else
		MAL_DBG2(mal, "already in poll" NL);
}

static irqreturn_t mal_txeob(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	int channel = 0;

	MAL_DBG2(mal, "txeob" NL);

	mal_schedule_poll(mal);
	mal_ack_txeobisr(mal, channel);

	return IRQ_HANDLED;
}

static irqreturn_t mal_rxeob(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	int channel = 0;

	MAL_DBG2(mal, "rxeob" NL);

	mal_schedule_poll(mal);
	mal_ack_rxeobisr(mal, channel);

	return IRQ_HANDLED;
}

static irqreturn_t mal_txde(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	int channel = 0;

	u32 deir = mal_ack_txdeir(mal, channel);

	MAL_DBG(mal, "txde %08x" NL, deir);

	if (net_ratelimit())
		printk(KERN_ERR
		       "mal%d: TX descriptor error (TXDEIR = 0x%08x)\n",
		       mal->index, deir);

	return IRQ_HANDLED;
}

static irqreturn_t mal_rxde(int irq, void *dev_instance)
{
	struct mal_instance *mal = dev_instance;
	struct list_head *l;
	int channel = 0;

	u32 deir = mal_ack_rxdeir(mal, channel);

	MAL_DBG(mal, "rxde %08x" NL, deir);

	list_for_each(l, &mal->list) {
		struct mal_commac *mc = list_entry(l, struct mal_commac, list);
		if (deir & mc->rx_chan_mask) {
			set_bit(MAL_COMMAC_RX_STOPPED, &mc->flags);
			mc->ops->rxde(mc->dev);
		}
	}

	mal_schedule_poll(mal);

	return IRQ_HANDLED;
}

void mal_poll_disable(struct mal_instance *mal, struct mal_commac *commac)
{
	/* Spinlock-type semantics: only one caller disable poll at a time */
	while (test_and_set_bit(MAL_COMMAC_POLL_DISABLED, &commac->flags))
		msleep(1);

	/* Synchronize with the MAL NAPI poller. */
	while (test_bit(__LINK_STATE_RX_SCHED, &mal->poll_dev.state))
		msleep(1);
}

void mal_poll_enable(struct mal_instance *mal, struct mal_commac *commac)
{
	smp_wmb();
	clear_bit(MAL_COMMAC_POLL_DISABLED, &commac->flags);

	// XXX might want to kick a poll now...
}

static int mal_poll(struct net_device *ndev, int *budget)
{
	struct mal_instance *mal = ndev->priv;
	struct list_head *l;
	int rx_work_limit = min(ndev->quota, *budget), received = 0, done;
	unsigned long flags;

	MAL_DBG2(mal, "poll(%d) %d ->" NL, *budget,
		 rx_work_limit);
 again:
	/* Process TX skbs */
	list_for_each(l, &mal->poll_list) {
		struct mal_commac *mc =
			list_entry(l, struct mal_commac, poll_list);
		mc->ops->poll_tx(mc->dev);
	}

	/* Process RX skbs.
	 *
	 * We _might_ need something more smart here to enforce polling
	 * fairness.
	 */
	list_for_each(l, &mal->poll_list) {
		struct mal_commac *mc =
			list_entry(l, struct mal_commac, poll_list);
		int n;
		if (unlikely(test_bit(MAL_COMMAC_POLL_DISABLED, &mc->flags)))
			continue;
		n = mc->ops->poll_rx(mc->dev, rx_work_limit);
		if (n) {
			received += n;
			rx_work_limit -= n;
			if (rx_work_limit <= 0) {
				done = 0;
				// XXX What if this is the last one ?
				goto more_work;
			}
		}
	}

	/* We need to disable IRQs to protect from RXDE IRQ here */
	spin_lock_irqsave(&mal->lock, flags);
	__netif_rx_complete(ndev);
	mal_enable_eob_irq(mal);
	spin_unlock_irqrestore(&mal->lock, flags);

	done = 1;

	/* Check for "rotting" packet(s) */
	list_for_each(l, &mal->poll_list) {
		struct mal_commac *mc =
			list_entry(l, struct mal_commac, poll_list);
		if (unlikely(test_bit(MAL_COMMAC_POLL_DISABLED, &mc->flags)))
			continue;
		if (unlikely(mc->ops->peek_rx(mc->dev) ||
			     test_bit(MAL_COMMAC_RX_STOPPED, &mc->flags))) {
			MAL_DBG2(mal, "rotting packet" NL);
			if (netif_rx_reschedule(ndev, received))
			{
				spin_lock_irqsave(&mal->lock, flags);
				mal_disable_eob_irq(mal);
				spin_unlock_irqrestore(&mal->lock, flags);
			}
			else
				MAL_DBG2(mal, "already in poll list" NL);

			if (rx_work_limit > 0)
				goto again;
			else
				goto more_work;
		}
		mc->ops->poll_tx(mc->dev);
	}

 more_work:
	ndev->quota -= received;
	*budget -= received;

	MAL_DBG2(mal, "poll() %d <- %d" NL, *budget,
		 done ? 0 : 1);

	return done ? 0 : 1;
}


static int __devinit mal_probe(struct of_device *ofdev,
			       const struct of_device_id *match)
{
	struct mal_instance *mal;
	phys_addr_t offset;
	int err = 0, i, bd_size;
	int index = mal_count++;
	const u32 *prop;

	mal = kzalloc(sizeof(struct mal_instance), GFP_KERNEL);
	if (!mal) {
		printk(KERN_ERR
		       "mal%d: out of memory allocating MAL structure!\n",
		       index);
		return -ENOMEM;
	}
	mal->index = index;
	mal->ofdev = ofdev;

	if (device_is_compatible(ofdev->node, "ibm,tomal")) {
		mal->version = 3;
	} else if (device_is_compatible(ofdev->node, "ibm,mcmal2")) {
		mal->version = 2;
	} else {
		mal->version = 1;
	}

	MAL_DBG(mal, "probe" NL);

	prop = get_property(ofdev->node, "num-tx-chans", NULL);
	if (prop == NULL) {
		printk(KERN_ERR
		       "mal%d: can't find MAL num-tx-chans property!\n",
		       index);
		err = -ENODEV;
		goto fail;
	}
	mal->num_tx_chans = prop[0];

	prop = get_property(ofdev->node, "num-rx-chans", NULL);
	if (prop == NULL) {
		printk(KERN_ERR
		       "mal%d: can't find MAL num-rx-chans property!\n",
		       index);
		err = -ENODEV;
		goto fail;
	}
	mal->num_rx_chans = prop[0];

#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
	if (of_address_to_resource(ofdev->node, 0, &mal->tomal_regs)) {
		printk(KERN_ERR
		       "tomal%d: can't get register address\n", index);
		err = -ENODEV;
		goto fail;
	}
	mal->io_base = ioremap(mal->tomal_regs.start, TOMAL_REG_SIZE);
	if (!mal->io_base) {
		printk(KERN_ERR
		       "tomal%d: failed to map io registers!\n", index);
		err = -ENODEV;
		goto fail;
	}

	mal->tomal_irq0 = irq_of_parse_and_map(ofdev->node, 0);
	mal->tomal_irq1 = irq_of_parse_and_map(ofdev->node, 1);
#else
	mal->dcr_base = dcr_resource_start(ofdev->node, 0);
	if (mal->dcr_base == 0) {
		printk(KERN_ERR
		       "mal%d: can't find DCR resource!\n", index);
		err = -ENODEV;
		goto fail;
	}
        mal->dcr_host = dcr_map(ofdev->node, mal->dcr_base, 0x100);
	if (!DCR_MAP_OK(mal->dcr_host)) {
		printk(KERN_ERR
		       "mal%d: failed to map DCRs !\n", index);
		err = -ENODEV;
		goto fail;
	}

	mal->txeob_irq = irq_of_parse_and_map(ofdev->node, 0);
	mal->rxeob_irq = irq_of_parse_and_map(ofdev->node, 1);
	mal->serr_irq = irq_of_parse_and_map(ofdev->node, 2);
	mal->txde_irq = irq_of_parse_and_map(ofdev->node, 3);
	mal->rxde_irq = irq_of_parse_and_map(ofdev->node, 4);
	if (mal->txeob_irq == NO_IRQ || mal->rxeob_irq == NO_IRQ ||
	    mal->serr_irq == NO_IRQ || mal->txde_irq == NO_IRQ ||
	    mal->rxde_irq == NO_IRQ) {
		printk(KERN_ERR
		       "mal%d: failed to map interrupts !\n", index);
		err = -ENODEV;
		goto fail_unmap;
	}
#endif

	INIT_LIST_HEAD(&mal->poll_list);
	set_bit(__LINK_STATE_START, &mal->poll_dev.state);
	mal->poll_dev.weight = CONFIG_IBM_NEW_EMAC_POLL_WEIGHT;
	mal->poll_dev.poll = mal_poll;
	mal->poll_dev.priv = mal;
	atomic_set(&mal->poll_dev.refcnt, 1);
	INIT_LIST_HEAD(&mal->list);
	spin_lock_init(&mal->lock);

	/* Load power-on reset defaults */
	mal_reset(mal);
	mal_configure(mal, ofdev);

	/* Allocate space for BD rings */
	BUG_ON(mal->num_tx_chans <= 0 || mal->num_tx_chans > 32);
	BUG_ON(mal->num_rx_chans <= 0 || mal->num_rx_chans > 32);

	bd_size = sizeof(struct mal_descriptor) *
		((NUM_TX_BUFF + NUM_WRAP_DESC) * mal->num_tx_chans +
		 (NUM_RX_BUFF + NUM_WRAP_DESC) * mal->num_rx_chans);
	mal->bd_virt =
		dma_alloc_coherent(&ofdev->dev, bd_size, &mal->bd_dma,
				   GFP_KERNEL);
	if (mal->bd_virt == NULL) {
		printk(KERN_ERR
		       "mal%d: out of memory allocating RX/TX descriptors!\n",
		       index);
		err = -ENOMEM;
		goto fail_unmap;
	}
	memset(mal->bd_virt, 0, bd_size);

	for (i = 0; i < mal->num_tx_chans; ++i) {
		offset = mal_tx_bd_offset(mal, i) * sizeof(struct mal_descriptor);
		mal_set_tx_descriptor(mal, i, (unsigned long)mal->bd_virt + offset,
				      mal->bd_dma + offset);
	}

	for (i = 0; i < mal->num_rx_chans; ++i) {
		offset = mal_rx_bd_offset(mal, i) * sizeof(struct mal_descriptor);
		mal_set_rx_descriptor(mal, i, (unsigned long)mal->bd_virt + offset,
				      mal->bd_dma + offset);
	}

#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
	err = request_irq(mal->tomal_irq0, tomal_irq, 0, "TOMAL IRQ0", mal);
	if (err)
		goto fail2;
	err = request_irq(mal->tomal_irq1, tomal_irq, 0, "TOMAL IRQ1", mal);
	if (err)
		goto fail3;
#else
	err = request_irq(mal->serr_irq, mal_serr, 0, "MAL SERR", mal);
	if (err)
		goto fail2;
	err = request_irq(mal->txde_irq, mal_txde, 0, "MAL TX DE", mal);
	if (err)
		goto fail3;
	err = request_irq(mal->txeob_irq, mal_txeob, 0, "MAL TX EOB", mal);
	if (err)
		goto fail4;
	err = request_irq(mal->rxde_irq, mal_rxde, 0, "MAL RX DE", mal);
	if (err)
		goto fail5;
	err = request_irq(mal->rxeob_irq, mal_rxeob, 0, "MAL RX EOB", mal);
	if (err)
		goto fail6;
#endif

	/* Enable all MAL SERR interrupt sources */
	mal_enable_serr(mal);

	/* Enable EOB interrupt */
	mal_enable_eob_irq(mal);

	printk(KERN_INFO
	       "MAL v%d %s, %d TX channels, %d RX channels\n",
	       mal->version, ofdev->node->full_name,
	       mal->num_tx_chans, mal->num_rx_chans);

	/* Advertise this instance to the rest of the world */
	wmb();
	dev_set_drvdata(&ofdev->dev, mal);

	mal_dbg_register(mal);

	return 0;

#ifndef CONFIG_IBM_NEW_EMAC_TOMAL
 fail6:
	free_irq(mal->rxde_irq, mal);
 fail5:
	free_irq(mal->txeob_irq, mal);
 fail4:
	free_irq(mal->txde_irq, mal);
 fail3:
	free_irq(mal->serr_irq, mal);
#else
 fail3:
	free_irq(mal->tomal_irq0, mal);
#endif
 fail2:

	dma_free_coherent(&ofdev->dev, bd_size, mal->bd_virt, mal->bd_dma);
 fail_unmap:
	dcr_unmap(mal->dcr_host, mal->dcr_base, 0x100);
 fail:
	kfree(mal);

	return err;
}

#ifdef CONFIG_IBM_NEW_EMAC_TOMAL
void mal_reinit(struct mal_instance *mal)
{
	phys_addr_t offset;
	int i;

	MAL_DBG(mal, "reinit" NL);

	/* Load power-on reset defaults */
	mal_reset(mal);
	mal_configure(mal, mal->ofdev);

	/*
	 * Note: we don't clear the descriptor space.  Calling code expects
	 * the descriptors to survive intact.
	 */

	for (i = 0; i < mal->num_tx_chans; ++i) {
		offset = mal_tx_bd_offset(mal, i) * sizeof(struct mal_descriptor);
		mal_set_tx_descriptor(mal, i, (unsigned long)mal->bd_virt + offset,
				      mal->bd_dma + offset);
	}

	for (i = 0; i < mal->num_rx_chans; ++i) {
		offset = mal_rx_bd_offset(mal, i) * sizeof(struct mal_descriptor);
		mal_set_rx_descriptor(mal, i, (unsigned long)mal->bd_virt + offset,
				      mal->bd_dma + offset);
	}

	/* Enable all MAL SERR interrupt sources */
	mal_enable_serr(mal);

	/* Enable EOB interrupt */
	mal_enable_eob_irq(mal);
}
#endif

static int __devexit mal_remove(struct of_device *ofdev)
{
	struct mal_instance *mal = dev_get_drvdata(&ofdev->dev);

	MAL_DBG(mal, "remove" NL);

	/* Syncronize with scheduled polling,
	   stolen from net/core/dev.c:dev_close()
	 */
	clear_bit(__LINK_STATE_START, &mal->poll_dev.state);
	netif_poll_disable(&mal->poll_dev);

	if (!list_empty(&mal->list)) {
		/* This is *very* bad */
		printk(KERN_EMERG
		       "mal%d: commac list is not empty on remove!\n",
		       mal->index);
		WARN_ON(1);
	}

	dev_set_drvdata(&ofdev->dev, NULL);

#ifndef CONFIG_IBM_NEW_EMAC_TOMAL
	free_irq(mal->serr_irq, mal);
	free_irq(mal->txde_irq, mal);
	free_irq(mal->txeob_irq, mal);
	free_irq(mal->rxde_irq, mal);
	free_irq(mal->rxeob_irq, mal);
#else
	free_irq(mal->tomal_irq0, mal);
	free_irq(mal->tomal_irq1, mal);
#endif

	mal_reset(mal);

	mal_dbg_unregister(mal);

	dma_free_coherent(&ofdev->dev,
			  sizeof(struct mal_descriptor) *
			  ((NUM_TX_BUFF + NUM_WRAP_DESC) * mal->num_tx_chans +
			  (NUM_RX_BUFF + NUM_WRAP_DESC) * mal->num_rx_chans),
			  mal->bd_virt, mal->bd_dma);
	kfree(mal);

	return 0;
}

static struct of_device_id mal_platform_match[] =
{
	{
		.compatible	= "ibm,mcmal",
	},
	{
		.compatible	= "ibm,mcmal2",
	},
	{
		.compatible	= "ibm,tomal",
	},
	/* Backward compat */
	{
		.type		= "mcmal-dma",
		.compatible	= "ibm,mcmal",
	},
	{
		.type		= "mcmal-dma",
		.compatible	= "ibm,mcmal2",
	},
	{},
};

static struct of_platform_driver mal_of_driver = {
	.name = "mcmal",
	.match_table = mal_platform_match,

	.probe = mal_probe,
	.remove = mal_remove,
};

int __init mal_init(void)
{
	return of_register_platform_driver(&mal_of_driver);
}

void __exit mal_exit(void)
{
	of_unregister_platform_driver(&mal_of_driver);
}
