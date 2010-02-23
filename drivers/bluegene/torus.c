/*********************************************************************
 *
 * Copyright (C) 2007-2008, Volkmar Uhlig, IBM Corporation
 *
 * Description:   BG Torus driver
 *
 * All rights reserved
 *
 ********************************************************************/

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/highmem.h>

#include <linux/mm_inline.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>

#include <asm/system.h>
#include <asm/prom.h>
#include <asm/of_platform.h>

#include "link.h"
#include "torus.h"
#include "bgp_dcr.h"
#include "ppc450.h"

#define DRV_NAME	"bgtorus"
#define DRV_VERSION	"0.3"
#define DRV_DESC	"Blue Gene Torus (IBM, Project Kittyhawk)"

MODULE_DESCRIPTION(DRV_DESC);
MODULE_AUTHOR("IBM, Project Kittyhawk");

static int bgtorus_proc_register(struct bg_torus *torus);
static int bgtorus_proc_unregister(struct bg_torus *torus);

#ifdef USER_MODE_TORUS_ACCESS
static int __init bgtorus_userlevel_dma_init(struct bg_torus *torus);
static void bgtorus_userlevel_wakeup(struct bg_torus *torus,
				     int group, int ctr);
#endif /* USER_MODE_TORUS_ACCESS */

/*
 * torus helper functions
 */

#define _BGP_DCR_DMA	(torus->dcr_base + 0x80)
#define _BGP_DCR_TORUS	(torus->dcr_base)

#define TORUS_DCR(x) (torus->dcr_base + x)
#define DMA_DCR(x) (torus->dcr_base + 0x80 + x)

#define DUMP_DCR(desc, x) printk("Torus: " desc "(%x): %08x\n", x, mfdcrx(x))

static void dump_dcrs(struct bg_torus *torus)
{
    int idx;

    DUMP_DCR("torus reset", 0xc80);
    DUMP_DCR("number nodes", 0xc81);
    DUMP_DCR("node coordinates", 0xc82);
    DUMP_DCR("neighbour plus", 0xc83);
    DUMP_DCR("neighbour minus", 0xc84);
    DUMP_DCR("cutoff plus", 0xc85);
    DUMP_DCR("cutoff minus", 0xc86);
    DUMP_DCR("VC threshold", 0xc87);
    DUMP_DCR("torus loopback", 0xc92);
    DUMP_DCR("torus non-rec err", 0xcdc);
    DUMP_DCR("dma reset", 0xd00);
    DUMP_DCR("base ctrl", 0xd01);
    for (idx = 0; idx < 8; idx++) {
	DUMP_DCR("inj min valid", 0xd02 + idx * 2);
	DUMP_DCR("inj max valid", 0xd03 + idx * 2);
	DUMP_DCR("rec min valid", 0xd14 + idx * 2);
	DUMP_DCR("rec max valid", 0xd15 + idx * 2);
    }
    DUMP_DCR("inj fifo enable", 0xd2c);
    DUMP_DCR("inj fifo enable", 0xd2d);
    DUMP_DCR("inj fifo enable", 0xd2e);
    DUMP_DCR("inj fifo enable", 0xd2f);
    DUMP_DCR("rec fifo enable", 0xd30);
    DUMP_DCR("rec hdr fifo enable", 0xd31);
    DUMP_DCR("inj fifo prio", 0xd32);
    DUMP_DCR("inj fifo prio", 0xd33);
    DUMP_DCR("inj fifo prio", 0xd34);
    DUMP_DCR("inj fifo prio", 0xd35);
    DUMP_DCR("remote get inj fifo threshold", 0xd36);
    DUMP_DCR("remote get inj service quanta", 0xd37);
    DUMP_DCR("rec fifo type", 0xd38);
    DUMP_DCR("rec header fifo type", 0xd39);
    DUMP_DCR("threshold rec type 0", 0xd3a);
    DUMP_DCR("threshold rec type 1", 0xd3b);
    for (idx = 0; idx < 32; idx++)
	DUMP_DCR("inj fifo map", 0xd3c + idx);
    DUMP_DCR("rec fifo map 00", 0xd60);
    DUMP_DCR("rec fifo map 00", 0xd61);
    DUMP_DCR("rec fifo map 01", 0xd62);
    DUMP_DCR("rec fifo map 01", 0xd63);
    DUMP_DCR("rec fifo map 10", 0xd64);
    DUMP_DCR("rec fifo map 10", 0xd65);
    DUMP_DCR("rec fifo map 11", 0xd66);
    DUMP_DCR("rec fifo map 11", 0xd67);
    DUMP_DCR("inj fifo irq enable grp0", 0xd6d);
    DUMP_DCR("inj fifo irq enable grp1", 0xd6e);
    DUMP_DCR("inj fifo irq enable grp2", 0xd6f);
    DUMP_DCR("inj fifo irq enable grp3", 0xd70);
    DUMP_DCR("rec fifo irq enable type0", 0xd71);
    DUMP_DCR("rec fifo irq enable type1", 0xd72);
    DUMP_DCR("rec fifo irq enable type2", 0xd73);
    DUMP_DCR("rec fifo irq enable type3", 0xd74);
    DUMP_DCR("rec hdr irq enable", 0xd75);
    DUMP_DCR("inj cntr irq enable", 0xd76);
    DUMP_DCR("inj cntr irq enable", 0xd77);
    DUMP_DCR("inj cntr irq enable", 0xd78);
    DUMP_DCR("inj cntr irq enable", 0xd79);
    DUMP_DCR("rec cntr irq enable", 0xd7a);
    DUMP_DCR("rec cntr irq enable", 0xd7b);
    DUMP_DCR("rec cntr irq enable", 0xd7c);
    DUMP_DCR("rec cntr irq enable", 0xd7d);
    DUMP_DCR("ce count inj fifo0", 0xd8a);
    DUMP_DCR("ce count inj fifo1", 0xd8b);
    DUMP_DCR("ce count inj counter", 0xd8c);
    DUMP_DCR("ce count inj desc", 0xd8d);
    DUMP_DCR("ce count rec fifo0", 0xd8e);
    DUMP_DCR("ce count rec fifo1", 0xd8f);
    DUMP_DCR("ce count rec counter", 0xd90);
    DUMP_DCR("ce count local fifo0", 0xd91);
    DUMP_DCR("ce count local fifo1", 0xd92);
    DUMP_DCR("fatal error 0", 0xd93);
    DUMP_DCR("fatal error 1", 0xd94);
    DUMP_DCR("fatal error 2", 0xd95);
    DUMP_DCR("fatal error 3", 0xd96);
    DUMP_DCR("wr0 bad address", 0xd97);
    DUMP_DCR("rd0 bad address", 0xd98);
    DUMP_DCR("wr1 bad address", 0xd99);
    DUMP_DCR("rd1 bad address", 0xd9a);
    DUMP_DCR("imfu err 0", 0xd9b);
    DUMP_DCR("imfu err 1", 0xd9c);
    DUMP_DCR("rmfu err", 0xd9d);
    DUMP_DCR("rd out-of-range", 0xd9e);
    DUMP_DCR("wr out-of-range", 0xd9f);
    DUMP_DCR("dbg dma warn", 0xdaf);
}

static void dump_inj_channels(struct bg_torus *torus, int group)
{
    int i;

    for (i = 0; i < BGP_TORUS_INJ_FIFOS; i++) {
	struct torus_fifo *fifo = &torus->dma[group].inj.fifo[i];
	printk("inj fifo %d: start=%08x, end=%08x, head=%08x, tail=%08x\n",
	       i, fifo->start, fifo->end, fifo->head, fifo->tail);
    }
    printk("counter enabled: %08x, %08x\n",
	   torus->dma[group].inj.counter_enabled[0],
	   torus->dma[group].inj.counter_enabled[1]);

    printk("counter hit zero: %08x, %08x\n",
	   torus->dma[group].inj.counter_hit_zero[0],
	   torus->dma[group].inj.counter_hit_zero[1]);

    for (i = 0; i < BGP_TORUS_COUNTERS; i++) {
	printk("  ctr %02d: count=%08x, base=%08x, enabled=%d, hitzero=%d\n",
	       i, torus->dma[group].inj.counter[i].counter,
	       torus->dma[group].inj.counter[i].base,
	       (torus->dma[group].inj.counter_enabled[i/32] >> (31-i%32)) & 1,
	       (torus->dma[group].inj.counter_hit_zero[i/32] >> (31-i%32)) & 1);
    }
}

static void dump_rcv_channels(struct bg_torus *torus, int group)
{
    int i;

    for (i = 0; i < BGP_TORUS_RCV_FIFOS; i++) {
	struct torus_fifo *fifo = &torus->dma[group].rcv.fifo[i];
	printk("rec fifo %d: start=%08x, end=%08x, head=%08x, tail=%08x\n",
	       i, fifo->start, fifo->end, fifo->head, fifo->tail);
    }
    printk("counter enabled: %08x, %08x\n",
	   torus->dma[group].rcv.counter_enabled[0],
	   torus->dma[group].rcv.counter_enabled[1]);

    printk("counter hit zero: %08x, %08x\n",
	   torus->dma[group].rcv.counter_hit_zero[0],
	   torus->dma[group].rcv.counter_hit_zero[1]);

    printk("counter group status: %08x\n",
	   torus->dma[group].rcv.counter_group_status);

    for (i = 0; i < BGP_TORUS_COUNTERS; i++) {
	printk("  ctr %02d: count=%08x, base=%08x, enabled=%d, hitzero=%d\n",
	       i, torus->dma[group].rcv.counter[i].counter,
	       torus->dma[group].rcv.counter[i].base,
	       (torus->dma[group].rcv.counter_enabled[i/32] >> (31-i%32)) & 1,
	       (torus->dma[group].rcv.counter_hit_zero[i/32] >> (31-i%32)) & 1);
    }
}

/*
 * Resource book-keeping
 */

static void reserve_group(unsigned long *map, int size, int group)
{
    int i;
    for (i = 0; i < size; i++)
	set_bit((group * size) + i, map);
}

static int allocate(struct bg_torus *torus, unsigned long *map, int size)
{
    unsigned long flags;
    int bit;
    int rc = -1;

    spin_lock_irqsave(&torus->lock, flags);
    bit = find_first_zero_bit(map, size);
    if (bit < size) {
	set_bit(bit, map);
	rc = bit;
    }
    spin_unlock_irqrestore(&torus->lock, flags);
    return rc;
}

static int reserve(struct bg_torus *torus, unsigned long *map, int size,
		   int bit)
{
    unsigned long flags;
    int rc = 0;

    if ((bit >= 0) && (bit < size)) {
	spin_lock_irqsave(&torus->lock, flags);
	if (!test_bit(bit, map)) {
	    set_bit(bit, map);
	    rc = 1;
	}
	spin_unlock_irqrestore(&torus->lock, flags);
    }
    return rc;
}

static int release(struct bg_torus *torus, unsigned long *map, int size,
		   int bit)
{
    unsigned long flags;
    int rc = 0;

    if ((bit >= 0) && (bit < size)) {
	spin_lock_irqsave(&torus->lock, flags);
	if (test_bit(bit, map)) {
	    clear_bit(bit, map);
	    rc = 1;
	}
	spin_unlock_irqrestore(&torus->lock, flags);
    }
    return rc;
}

#define MANAGE(resource, size)						\
    static int allocate_ ## resource(struct bg_torus *torus)	\
    {									\
	return allocate(torus, torus->resource ## _map, (size));	\
    }									\
    static int reserve_ ## resource(struct bg_torus *torus, int i)	\
    {									\
	return reserve(torus, torus->resource ## _map, (size), i);	\
    }									\
    static int release_ ## resource(struct bg_torus *torus, int i)	\
    {									\
	return release(torus, torus->resource ## _map, (size), i);	\
    }

MANAGE(inj_counter, BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS);
MANAGE(rcv_counter, BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS);
MANAGE(inj_fifo, BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS);
MANAGE(rcv_fifo, BGP_TORUS_GROUPS * BGP_TORUS_RCV_FIFOS);
MANAGE(rcv_dma_region, BGP_TORUS_DMA_REGIONS);

static int remove_rcv_counter(struct bg_torus *torus, int i)
{
    return reserve(torus, torus->rcv_counter_removed,
		   BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS, i);
}

static int restore_rcv_counter(struct bg_torus *torus, int i)
{
    return release(torus, torus->rcv_counter_removed,
		   BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS, i);
}

static inline void enable_inj_fifo(struct bg_torus *torus, int group, int fifo)
{
    int dcr = DMA_DCR(0x2c + group);
    u32 val = mfdcrx(dcr) | (0x80000000 >> fifo);
    mtdcrx(dcr, val);
}

static inline void disable_inj_fifo(struct bg_torus *torus, int group, int fifo)
{
    int dcr = DMA_DCR(0x2c + group);
    u32 val = mfdcrx(dcr) & ~(0x80000000 >> fifo);
    printk("disable inj fifo g=%d, f=%d (%x -> %x)\n",
	   group, fifo, mfdcrx(dcr), val);
    mtdcrx(dcr, val);
}

static inline void enable_inj_fifo_user(struct bg_torus *torus,
					int group, int fifo)
{
    out_be32(&torus->dma[group].inj.dma_activate, 0x80000000U >> fifo);
}

static inline void disable_inj_fifo_user(struct bg_torus *torus,
					 int group, int fifo)
{
    out_be32(&torus->dma[group].inj.dma_deactivate, 0x80000000U >> fifo);
}

static inline void enable_rcv_fifo(struct bg_torus *torus, int group, int fifo)
{
    int dcr = DMA_DCR(0x30);
    u32 val = mfdcrx(dcr) |
		(0x80000000 >> (group * BGP_TORUS_RCV_FIFOS + fifo));
    mtdcrx(dcr, val);
}

static inline void disable_rcv_fifo(struct bg_torus *torus,
				    int group, int fifo)
{
    int dcr = DMA_DCR(0x30);
    u32 val = mfdcrx(dcr) &
		~(0x80000000 >> (group * BGP_TORUS_RCV_FIFOS + fifo));
    mtdcrx(dcr, val);
}

static inline void set_rcv_fifo_threshold(struct bg_torus *torus,
					  int type, u32 val)
{
    mtdcrx(DMA_DCR(0x3a + type), val);
}

static inline void enable_rcv_fifo_threshold_interrupt(struct bg_torus *torus,
						       int type,
						       int group, int fifo)
{
    int dcr = DMA_DCR(0x71 + type);
    mtdcrx(dcr, mfdcrx(dcr) |
		    (0x80000000 >> (group * BGP_TORUS_RCV_FIFOS + fifo)));
}

static inline void enable_rcv_counter_zero_interrupts(struct bg_torus *torus,
						      int irqgroup)
{
    int dcr = DMA_DCR(0x7a + irqgroup);
    mtdcrx(dcr, ~0); /* enable interrupts on all counters */
}

static inline void set_inj_counter_zero_interrupt(struct bg_torus *torus,
						  int group, int counter,
						  int irqgroup)
{
    int dcr = DMA_DCR(0x76 + irqgroup);
    mtdcrx(dcr, mfdcrx(dcr) |
		(0x80000000 >> ((group * BGP_TORUS_COUNTERS + counter) / 8)));
}

static inline void init_inj_counter(struct bg_torus *torus,
				    int group, int counter)
{
    u32 mask = 0x80000000U >> (counter % 32);
    out_be32(&torus->dma[group].inj.counter_enable[counter / 32], mask);
    out_be32(&torus->dma[group].inj.counter[counter].counter, 0);
    out_be32(&torus->dma[group].inj.counter_clear_hit_zero[counter / 32], mask);
}

static inline void enable_rcv_counter(struct bg_torus *torus,
				      int group, int counter)
{
    u32 mask = 0x80000000U >> (counter % 32);
    out_be32(&torus->dma[group].rcv.counter_enable[counter / 32], mask);
}

static inline void disable_rcv_counter(struct bg_torus *torus,
				       int group, int counter)
{
    u32 mask = 0x80000000U >> (counter % 32);
    out_be32(&torus->dma[group].rcv.counter_disable[counter / 32], mask);
}

static inline void set_dma_receive_range(struct bg_torus *torus, int range,
					 unsigned long start,
					 unsigned long end)
{
    mtdcrx(_BGP_DCR_rDMA_MIN_VALID_ADDR(range), start);
    mtdcrx(_BGP_DCR_rDMA_MAX_VALID_ADDR(range), end);
}

static inline void set_dma_inject_range(struct bg_torus *torus, int range,
					unsigned long start, unsigned long end)
{
    mtdcrx(_BGP_DCR_iDMA_MIN_VALID_ADDR(range), start);
    mtdcrx(_BGP_DCR_iDMA_MAX_VALID_ADDR(range), end);
}

/*
 * interrupts
 */

static irqreturn_t bgtorus_unhandled_interrupt(int irq, void *dev)
{
    dump_dcrs(dev);
    panic("bgtorus: unhandled IRQ %d\n", irq);
    return IRQ_HANDLED;
}

static void dump_inj_fifo_err(u8 val)
{
    if (val & 0x80)
	printk("Channel Arbiter: Illegal 36-bit vector in Stage2\n");
    if (val & 0x40)
	printk("Channel Arbiter: Grant received that did not match request\n");
    if (val & 0x20)
	printk("Channel Arbiter: State machine in unknown state\n");
    if (val & 0x10)
	printk("Header Check: + and - Hint Bits simultaneously set\n");
    if (val & 0x08)
	printk("Header Check: Node != Dest but +/- Hint bits zero\n");
    if (val & 0x04)
	printk("Header Check: Dynamic bit and VC bit inconsistent\n");
    if (val & 0x02)
	printk("Header Check: Destination not within Machine size\n");
    if (val & 0x01)
	printk("Header Check: Deposit Bit=1 AND, for each of X,Y,Z "
	       "(hint_xp | hint_xm) AND "
	       "((Ycoord != dest_Y) | (Zcoord != dest_Z))");
}

static irqreturn_t torus_fatal_error(int irq, void *dev)
{
    int group;
    u32 val;
    int idx;

    DUMP_DCR("torus NRE X+", 0xc80 + 0x5c);
    DUMP_DCR("torus NRE X-", 0xc80 + 0x5d);
    DUMP_DCR("torus NRE Y+", 0xc80 + 0x5e);
    DUMP_DCR("torus NRE Y-", 0xc80 + 0x5f);
    DUMP_DCR("torus NRE Z+", 0xc80 + 0x60);
    DUMP_DCR("torus NRE Z-", 0xc80 + 0x61);
    DUMP_DCR("torus NRE sndhdr chksum", 0xc80 + 0x62);
    DUMP_DCR("torus NRE sender parity", 0xc80 + 0x62);
    DUMP_DCR("torus NRE SRAM", 0xc80 + 0x63);

    DUMP_DCR("torus NRE inj fifo0", 0xc80 + 0x64);
    DUMP_DCR("torus NRE inj fifo1", 0xc80 + 0x65);

    for (group = 0; group < BGP_TORUS_GROUPS; group++) {
	val = mfdcrx(0xc80 + 0x64 + group);
	for (idx = 1; idx <= 4; idx++)
	    if ((val >> (32 - 8*idx)) & 0xff) {
		printk("Group %d inject Fifo %d: ", group, idx);
		dump_inj_fifo_err((val >> (32 - 8*idx)) & 0xff);
	    }
    }

    DUMP_DCR("torus NRE inj mem", 0xc80 + 0x66);
    DUMP_DCR("torus NRE rec mem", 0xc80 + 0x67);
    DUMP_DCR("torus NRE dcr", 0xc80 + 0x68);

    panic("bgtorus: torus fatal error (IRQ %d)", irq);
    return IRQ_HANDLED;
}

static irqreturn_t torus_fatal_count_threshold(int irq, void *dev)
{
    panic("bgtorus: fatal error: count threshold (IRQ %d)", irq);
    return IRQ_HANDLED;
}

static irqreturn_t dma_fatal_count_threshold(int irq, void *dev)
{
    panic("bgtorus: fatal error: DMA count threshold (IRQ %d)", irq);
    return IRQ_HANDLED;
}

static irqreturn_t dma_fatal_error(int irq, void *dev)
{
    panic("bgtorus: dma fatal error (IRQ %d)", irq);
    return IRQ_HANDLED;
}

static void enqueue_frag_skb(struct sk_buff_head *skb_list,
			     union torus_source_id src_id,
			     int total_len, int received_len,
			     struct sk_buff *skb)
{
    struct torus_skb_cb *cb = (struct torus_skb_cb *) skb->cb;
    cb->src_id = src_id;
    cb->total_len = total_len;
    cb->received_len = received_len;
    __skb_queue_tail(skb_list, skb);
}

static int dequeue_frag_skb(struct sk_buff_head *skb_list,
			    union torus_source_id src_id,
			    int *total_len_p, int *received_len_p,
			    struct sk_buff **skb_p)
{
    struct sk_buff *skb;

    skb = skb_list->next;
    while (skb != ((struct sk_buff *) skb_list)) {
	struct torus_skb_cb * cb= (struct torus_skb_cb *) skb->cb;

	if (cb->src_id.raw == src_id.raw) {
	    (*total_len_p) = cb->total_len;
	    (*received_len_p) = cb->received_len;
	    (*skb_p) = skb;
	    __skb_unlink(skb, skb_list);
	    return 1;
	}

	/* XXX: timeout packets */

	skb = skb->next;
    }

    return 0;
}

static void torus_process_rx(struct bg_torus *torus)
{
    int slot;
    u32 pending;

    for (slot = 0; slot < 2; slot++) {
	/*
	 * Clear all pending thresholds BEFORE looking at the fifos, so that
	 * any new fragments that arrive after we've read a fifo's tail
	 * pointer will raise new threshold IRQs.
	 */
	out_be32(&torus->dma[BGP_KERNEL_GROUP].rcv.clear_threshold[slot], ~0U);

	/* Note: empty bits are identical in all groups */
	pending = torus->dma[BGP_KERNEL_GROUP].rcv.empty[slot];

	while (pending) {
	    int bit = __ffs(pending);
	    int fifoidx = (slot + 1) * 32 - bit - 1;
	    int group = fifoidx / BGP_TORUS_RCV_FIFOS;
	    int fifoid = fifoidx % BGP_TORUS_RCV_FIFOS;
	    struct torus_fifo *fifo = &torus->dma[group].rcv.fifo[fifoid];
	    struct torus_rx_ring *rx = &torus->rx[fifoidx];

#if 0
	    unsigned int start, end, head, tail;
	    start = in_be32(&fifo->start);
	    end = in_be32(&fifo->end);
	    head = in_be32(&fifo->head);
	    tail = in_be32(&fifo->tail);
	    printk("fifo (idx=%d, bit=%d, pend=%08x) g=%d, f=%d: "
		   "start=%08x, end=%08x, head=%08x, tail=%08x\n",
		   fifoidx, bit, pending, group, fifoid,
		   start, end, head, tail);
#endif

	    unsigned int tail_idx = (in_be32(&fifo->tail) - rx->start) /
					(sizeof(union torus_rcv_desc) >> 4);

	    /* irqs disabled here */
	    spin_lock(&rx->lock);

	    while (rx->head_idx != tail_idx) {
		union torus_rcv_desc *desc = &rx->desc[rx->head_idx];
		int n;

		/*
		 * In rx: src_id, total_len, received_len, and skb all pertain
		 * to the "current" packet being reconstituted.  If src_id is
		 * TORUS_SOURCE_ID_NULL, there is no current packet, and the
		 * remaining fields are undefined.
		 */

		if (unlikely(desc->src_id.raw != rx->src_id.raw)) {
		    // The new fragment doesn't belong to the current packet.

		    if (unlikely(rx->src_id.raw != TORUS_SOURCE_ID_NULL)) {
			// We have a partially-constructed packet.  Put it
			// aside along with its state information.

			enqueue_frag_skb(&rx->skb_list, rx->src_id,
					 rx->total_len, rx->received_len,
					 rx->skb);
		    }

		    // Set up current packet for the new fragment, either by
		    // retrieving a partially-constructed packet from the side
		    // queue or by starting a new packet.
		    rx->src_id = desc->src_id;
		    if (likely(!dequeue_frag_skb(&rx->skb_list, rx->src_id,
						 &rx->total_len,
						 &rx->received_len,
						 &rx->skb)))
		    {
			// Start a new packet.

			if (likely(desc->counter == 0)) {
			    // We got the first fragment first, so we can
			    // determine the correct length.
			    struct bglink_hdr_torus *hdr =
				(struct bglink_hdr_torus *) desc->data;
			    rx->total_len = hdr->len;
			} else {
			    // We have to assume the maximum length.
			    rx->total_len = sizeof(struct bglink_hdr_torus) +
								TORUS_MAX_MTU;
			}
			rx->received_len = 0;  // nothing copied yet
			rx->skb = alloc_skb(rx->total_len, GFP_ATOMIC);
			if (unlikely(rx->skb == NULL)) {
			    rx->dropped++;
			    rx->src_id.raw = TORUS_SOURCE_ID_NULL;
			    goto next;
			}

			// The following goto is a small optimization.  If
			// we're here, the upcoming first-fragment clause,
			// while harmless, is redundant.
			goto skip_first_frag_check;
		    }
		}

		// At this point, the new fragment belongs to the current
		// packet but is not the fragment that caused the packet to
		// be created.  If it's the first fragment, we can now
		// determine the correct length.
		if (desc->counter == 0) {
		    struct bglink_hdr_torus *hdr =
			(struct bglink_hdr_torus *) desc->data;
		    BUG_ON(hdr->len > rx->total_len);
		    rx->total_len = hdr->len;
		}

	    skip_first_frag_check:
		// Copy the fragment into the current packet.
		BUG_ON(desc->counter >= rx->total_len);
		n = min(240u, rx->total_len - desc->counter);
		memcpy(rx->skb->data + desc->counter, desc->data, n);
		rx->received_len += n;
		if (rx->received_len >= rx->total_len) {
		    // We've received all the fragments.  Deliver the packet.
		    struct bglink_proto *proto;
		    struct bglink_hdr_torus *hdr =
			(struct bglink_hdr_torus *) rx->skb->data;

		    skb_put(rx->skb, rx->total_len);
		    __skb_pull(rx->skb, sizeof(*hdr));

		    // deliver to upper protocol layers
		    rcu_read_lock();
		    proto = bglink_find_proto(hdr->lnk_proto);
		    if ((proto != NULL) && (proto->torus_rcv != NULL)) {
			proto->torus_rcv(rx->skb, hdr);
			rx->delivered++;
		    } else {
			dev_kfree_skb(rx->skb);
			rx->dropped++;
		    }
		    rcu_read_unlock();

		    rx->src_id.raw = TORUS_SOURCE_ID_NULL;
		}

	    next:
		rx->head_idx = (rx->head_idx + 1) % BGP_TORUS_RX_ENTRIES;
		pr_debug("new head idx: %x, tail idx: %x\n",
			 rx->head_idx, tail_idx);
	    }

	    out_be32(&fifo->head,
		     rx->start + rx->head_idx*sizeof(union torus_rcv_desc)/16);
	    spin_unlock(&rx->lock);

	    pending &= ~(1 << bit);
	}
    }
#if 0
    printk("done: not empty: [%08x %08x], avail: [%08x %08x], "
	   "thresh: [%08x %08x]\n",
	   torus->dma[BGP_KERNEL_GROUP].rcv.empty[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.empty[1],
	   torus->dma[BGP_KERNEL_GROUP].rcv.available[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.available[1],
	   torus->dma[BGP_KERNEL_GROUP].rcv.threshold[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.threshold[1]);
#endif
}

static void torus_process_tx(struct bg_torus *torus, int txidx)
{
    int group = txidx / BGP_TORUS_INJ_FIFOS;
    int fifoidx = txidx % BGP_TORUS_INJ_FIFOS;
    struct torus_tx_ring *tx = &torus->tx[txidx];
    unsigned int head_idx =
	(in_be32(&torus->dma[group].inj.fifo[fifoidx].head) - tx->start) /
	(sizeof(union torus_inj_desc) >> 4);

    BUG_ON(tx->desc == NULL);

    spin_lock(&tx->lock);

    pr_debug("torus: processing tx queue %d, head_idx=%d, pending_idx=%d\n",
	     txidx, head_idx, tx->pending_idx);

    while (tx->pending_idx != head_idx) {
	BUG_ON(!tx->skbs[tx->pending_idx]);
	dev_kfree_skb_irq(tx->skbs[tx->pending_idx]);
	tx->skbs[tx->pending_idx] = NULL;
	tx->pending_idx = (tx->pending_idx + 1) % BGP_TORUS_TX_ENTRIES;
    }

    spin_unlock(&tx->lock);
}

static irqreturn_t dma_rcv_fifo_threshold(int irq, void *dev)
{
    struct bg_torus *torus = dev;

#if 0
    printk("torus receive fifo threshold (irq %d/%x)\n", irq, irq);
    printk("fifo: not empty: [%08x %08x], avail: [%08x %08x], "
	   "thresh: [%08x %08x]\n",
	   torus->dma[BGP_KERNEL_GROUP].rcv.empty[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.empty[1],
	   torus->dma[BGP_KERNEL_GROUP].rcv.available[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.available[1],
	   torus->dma[BGP_KERNEL_GROUP].rcv.threshold[0],
	   torus->dma[BGP_KERNEL_GROUP].rcv.threshold[1]);
#endif

    torus_process_rx(torus);

    return IRQ_HANDLED;
}

static irqreturn_t dma_inj_counter_not_enabled(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT0);
    int ctr = _DMA_MFU_STAT0_IMFU_NOT_ENABLED_COUNTER_ID(stat) % BGP_TORUS_COUNTERS;
    int group = _DMA_MFU_STAT0_IMFU_NOT_ENABLED_COUNTER_ID(stat) / BGP_TORUS_COUNTERS;

    printk("torus: group %d inj counter %d not enabled irq %d\n", group, ctr, irq);

    if (group == BGP_KERNEL_GROUP)
    {
        dump_dcrs(dev);
        dump_inj_channels(dev, BGP_KERNEL_GROUP);
        panic("torus: group %d inj counter %d not enabled irq %d\n", group, ctr, irq);
    }

    mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_IMFU_ARB_WERR);
    bgtorus_userlevel_wakeup(torus, group, ctr);
    return IRQ_HANDLED;
}


static irqreturn_t dma_inj_counter_message_length_error(int irq, void *dev)
{
    dump_dcrs(dev);
    dump_inj_channels(dev, BGP_KERNEL_GROUP);
    panic("torus: inj message length error irq %d\n", irq);
    return IRQ_HANDLED;
}

static irqreturn_t dma_inj_counter_underflow(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT0);
    int ctr = _DMA_MFU_STAT0_IMFU_UNDERFLOW_COUNTER_ID(stat) % BGP_TORUS_COUNTERS;
    int group = _DMA_MFU_STAT0_IMFU_UNDERFLOW_COUNTER_ID(stat) / BGP_TORUS_COUNTERS;

    printk("torus: group %d inj counter %d underflow irq %d\n", group, ctr, irq);

    if (group == BGP_KERNEL_GROUP)
    {
        dump_dcrs(dev);
        dump_inj_channels(dev, BGP_KERNEL_GROUP);
        panic("torus: group %d inj counter %d underflow irq %d\n", group, ctr, irq);
    }

    mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_IMFU_COUNTER_UNDERFLOW);
    bgtorus_userlevel_wakeup(torus, group, ctr);
    return IRQ_HANDLED;
}

static irqreturn_t dma_inj_counter_overflow(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT0);
    u32 addr = _DMA_MFU_STAT0_IMFU_OVERFLOW_NB_ADDR(stat);

    dump_dcrs(dev);
    dump_inj_channels(dev, BGP_KERNEL_GROUP);
    panic("torus: inj counter overflow @ %x irq %d\n", addr, irq);

    //mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_IMFU_COUNTER_OVERFLOW);
    return IRQ_HANDLED;

}

static irqreturn_t dma_rcv_counter_underflow(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT1);
    int ctr = _DMA_MFU_STAT1_RMFU_UNDERFLOW_COUNTER_ID(stat) % BGP_TORUS_COUNTERS;
    int group = _DMA_MFU_STAT1_RMFU_UNDERFLOW_COUNTER_ID(stat) / BGP_TORUS_COUNTERS;

    printk("torus: group %d rcv counter %d underflow irq %d\n", group, ctr, irq);

    if (group == BGP_KERNEL_GROUP)
    {
        dump_dcrs(dev);
        dump_rcv_channels(dev, BGP_KERNEL_GROUP);
        panic("torus: group %d rcv counter %d underflow irq %d\n", group, ctr, irq);
    }

    mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_RMFU_COUNTER_UNDERFLOW);
    bgtorus_userlevel_wakeup(torus, group, ctr);
    return IRQ_HANDLED;
}

static irqreturn_t dma_rcv_counter_overflow(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT1);
    u32 addr = _DMA_MFU_STAT1_RMFU_OVERFLOW_NB_ADDR(stat);

    dump_dcrs(dev);
    dump_rcv_channels(dev, BGP_KERNEL_GROUP);
    panic("torus: rcv counter overflow @ %x irq %d\n", addr, irq);

    //mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_RMFU_COUNTER_OVERFLOW);
    return IRQ_HANDLED;
}

static irqreturn_t dma_rcv_counter_disabled(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    u32 stat = mfdcrx(_BGP_DCR_DMA_MFU_STAT2);
    u32 ctr, group, index;

    if (!_DMA_MFU_STAT2_RMFU_COUNTER_NE(stat)) {
	panic("\n\ttorus: dma_rcv_counter_disabled (IRQ %d):"
	      "\n\t\texpected error not found.  (status 0x%08x)\n",
	      irq, stat);
    }

    ctr = _DMA_MFU_STAT2_RMFU_COUNTER_ID(stat);
    group = ctr / BGP_TORUS_COUNTERS;
    index = ctr % BGP_TORUS_COUNTERS;

    printk("torus: *******************************************************\n"
	   "torus: RX counter %d (grp %d, idx %d) used while disabled.\n"
	   "torus: Counter is taken out of service.\n"
	   "torus: Use ioctl(TORUS_BGSM_ALLOC_RX_COUNTER) to get it back.\n"
	   "torus: *******************************************************\n",
	   ctr, group, index);

    /* Reserve the counter and mark it as removed. */
    (void) reserve_rcv_counter(torus, ctr);
    (void) remove_rcv_counter(torus, ctr);

    /* Point the counter base above the 4G boundary so that incoming data
     * will go off into space and not corrupt actual memory.  By experiment
     * this seems to work.  Note that base and limit are shifted left 4 bits
     * to form addresses.
     */
    out_be32(&torus->dma[group].rcv.counter[index].base, 0x10000000);
    out_be32(&torus->dma[group].rcv.counter[index].limit, 0x1fffffff);

    /* Re-enable the counter and clear the DMA error condition. */
    enable_rcv_counter(torus, group, index);
    mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_RMFU_ARB_WERR);

    return IRQ_HANDLED;
}

static irqreturn_t dma_mem_fifo_full(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    irqreturn_t rc = dma_rcv_fifo_threshold(irq, dev);
    mtdcrx(_BGP_DCR_DMA_CLEAR0, _DMA_CLEAR0_RMFU_ARB_WERR);
    return rc;
}

static irqreturn_t dma_rcv_counter_hitzero(int irq, void *dev)
{
#ifdef USER_MODE_TORUS_ACCESS
    struct bg_torus *torus = dev;

    /* group_status from any group provides information about all groups */
    u32 subgroups =
	in_be32(&torus->dma[BGP_KERNEL_GROUP].rcv.counter_group_status);

    while (subgroups != 0) {
	int subgrp_bit = __ffs(subgroups);
	int subgroup = 7 - (subgrp_bit/4); /* lumping 4 subgroups together */
	int group = subgroup / 2;
	int slot = subgroup % 2;

	u32 ctrs = in_be32(&torus->dma[group].rcv.counter_hit_zero[slot]);
	out_be32(&torus->dma[group].rcv.counter_clear_hit_zero[slot], ctrs);

	while (ctrs != 0) {
	    int ctr_bit = __ffs(ctrs);
	    int ctr = (slot * 32) + 31 - ctr_bit;
	    bgtorus_userlevel_wakeup(torus, group, ctr);
	    ctrs &= ~(1 << ctr_bit);
	}

	subgroups &= ~(0xf << (4 * (subgrp_bit/4))); /* done with 4 subgrps */
    }
#else /* USER_MODE_TORUS_ACCESS */
    panic("torus: dma rcv counter hitzero (IRQ %d)\n", irq);
#endif /* USER_MODE_TORUS_ACCESS */

    return IRQ_HANDLED;
}

static irqreturn_t torus_rcv_fifo_watermark(int irq, void *dev)
{
    panic("torus: rcv fifo watermark (IRQ %d)\n", irq);
    return IRQ_HANDLED;
}

static irqreturn_t dma_inj_fifo_threshold(int irq, void *dev)
{
    panic("torus: inject fifo threshold (IRQ %d)\n", irq);
    return IRQ_HANDLED;
}

static irqreturn_t dma_inj_counter_hitzero(int irq, void *dev)
{
    struct bg_torus *torus = dev;
    int group = BGP_KERNEL_GROUP;
    int slot;

    pr_debug("torus inject counter hitzero (%d) [%08x %08x]\n", irq,
	     torus->dma[group].inj.counter_hit_zero[0],
	     torus->dma[group].inj.counter_hit_zero[1]);

    for (slot = 0; slot < 2; slot++) {
	u32 pending = in_be32(&torus->dma[group].inj.counter_hit_zero[slot]);
	u32 was_pending = pending;

	while(pending) {
	    int bit = __ffs(pending);
	    int counter = (slot + 1) * 32 - bit - 1;
	    torus_process_tx(torus,
			     torus->inj_counter_to_txidx[counter +
						 group * BGP_TORUS_COUNTERS]);
	    pending &= ~(1 << bit);
	}

	out_be32(&torus->dma[group].inj.counter_clear_hit_zero[slot],
		 was_pending);
    }

    return IRQ_HANDLED;
}

static irqreturn_t torus_inj_fifo_watermark(int irq, void *dev)
{
    panic("torus: inject fifo watermark (IRQ %d)\n", irq);
    return IRQ_HANDLED;
}


static irqreturn_t dma_error(int irq, void *dev)
{
    dump_dcrs(dev);
    panic("torus: DMA error (IRQ %d)\n", irq);
    return IRQ_HANDLED;
}

#define DEF_IRQ(_count, _used, _name, _handler) \
{ .count = _count, .used = _used, .name = _name, .handler = _handler }

struct bgtorus_irq {
    unsigned int count;
    unsigned int used;
    char *name;
    irqreturn_t (*handler)(int irq, void *dev);
};

static struct bgtorus_irq bgtorus_irqs[] =
{
    DEF_IRQ(  1, 1, "Torus fatal error", torus_fatal_error ),
    DEF_IRQ( 20, 0, "Torus count threshold", torus_fatal_count_threshold ),
    /* torus interrupt 21 is unused */
    DEF_IRQ(  9, 1, "Torus DMA fatal count threshold",
					dma_fatal_count_threshold ),
    DEF_IRQ(  1, 1, "Torus DMA fatal error", dma_fatal_error ),
    DEF_IRQ(  8, 0, "Torus inj fifo watermark", torus_inj_fifo_watermark ),
    DEF_IRQ( 14, 0, "Torus rcv fifo watermark", torus_rcv_fifo_watermark ),
    DEF_IRQ(  2, 1, "Torus read invalid address", bgtorus_unhandled_interrupt ),
    DEF_IRQ(  4, 1, "Torus inj fifo threshold", dma_inj_fifo_threshold ),
    DEF_IRQ(  4, 1, "Torus rcv fifo threshold", dma_rcv_fifo_threshold ),
    DEF_IRQ(  4, 1, "Torus inj counter 0", dma_inj_counter_hitzero ),
    DEF_IRQ(  4, 1, "Torus rec counter 0", dma_rcv_counter_hitzero ),
    DEF_IRQ(  1, 1, "Torus inj counter not enabled",
					dma_inj_counter_not_enabled ),
    DEF_IRQ(  1, 1, "Torus inj counter message length 0",
					dma_inj_counter_message_length_error),
    DEF_IRQ(  1, 1, "Torus inj counter underflow", dma_inj_counter_underflow),
    DEF_IRQ(  1, 1, "Torus inj counter overflow", dma_inj_counter_overflow),
    /* Rcv counter under- and overflows happen in normal operation. */
    DEF_IRQ(  1, 0, "Torus rcv counter underflow", dma_rcv_counter_underflow),
    DEF_IRQ(  1, 0, "Torus rcv counter overflow", dma_rcv_counter_overflow),
    /* In DD2, interrupt 46 is replaced by interrupts 54-57. */
    DEF_IRQ(  1, 0, "Torus rcv fifo/ctr error", bgtorus_unhandled_interrupt ),
    DEF_IRQ(  6, 1, "Torus DMA error", dma_error ),
    DEF_IRQ(  1, 1, "Torus link check", bgtorus_unhandled_interrupt ),
    DEF_IRQ(  1, 1, "Torus memory fifo full", dma_mem_fifo_full ),
    DEF_IRQ(  2, 1, "Torus DD2 error", bgtorus_unhandled_interrupt ),
    DEF_IRQ(  1, 1, "Torus rcv ctr disabled", dma_rcv_counter_disabled ),
    { .count = 0 },
};




/* 1. locate fifo inject start
 * 2. write descriptor
 *    * flags=torus msg, inj counter id=0, base=ptr, len
 *    * hardware header X,Y,Z, csum_skip, pids=0 (dest group 0), dm=0, hint=
 * 3
 */
int bgtorus_xmit(struct bg_torus *torus,
		 union torus_inj_desc *desc, struct sk_buff *skb)
{
    int group = BGP_KERNEL_GROUP;
    int injfifo = BGP_KERNEL_INJ_FIFO;
    int injidx = group * BGP_TORUS_INJ_FIFOS + injfifo;
    int rc = 0;
    union torus_inj_desc *injdesc;
    phys_addr_t paddr;
    unsigned long flags;

    struct torus_tx_ring *tx = &torus->tx[injidx];
    struct torus_fifo *fifo = &torus->dma[group].inj.fifo[injfifo];

    BUG_ON(desc->dma_hw.length == 0);

    spin_lock_irqsave(&tx->lock, flags);


    /* check for space in xmit queue */
    if ((tx->tail_idx + 1) % BGP_TORUS_TX_ENTRIES == tx->pending_idx) {
	rc = -EBUSY;
	goto out;
    }

    paddr = dma_map_single(/*torus->ofdev->dev*/ NULL,
			   skb->data, skb->len, DMA_TO_DEVICE);
    BUG_ON(paddr > 0xffffffffULL);

    injdesc = &tx->desc[tx->tail_idx];
    memcpy(injdesc, desc, sizeof(*injdesc));
    injdesc->dma_hw.base = paddr;

    tx->skbs[tx->tail_idx] = skb;

#if 0
    printk("tailidx=%d, injdesc=%p, vdata=%p, pdata=%llx\n",
	   tx->tail_idx, injdesc, skb->data, paddr);

    printk("inj [%08x, %08x, %08x, %08x] [%08x, %08x] [%08x, %08x]\n",
	   injdesc->raw32[0], injdesc->raw32[1],
	   injdesc->raw32[2], injdesc->raw32[3],
	   injdesc->raw32[4], injdesc->raw32[5],
	   injdesc->raw32[6], injdesc->raw32[7]);
#endif
    tx->tail_idx = (tx->tail_idx + 1) % BGP_TORUS_TX_ENTRIES;

    wmb();

    // arm inject fifo
    out_be32(&torus->dma[group].inj.counter[tx->counter].increment, skb->len);

    // update tail pointer to initiate DMA
    out_be32(&fifo->tail,
	     tx->start + tx->tail_idx * (sizeof(union torus_inj_desc) >> 4));

 out:
    spin_unlock_irqrestore(&tx->lock, flags);

    return rc;
}

static int bgtorus_map_irqs(struct bg_torus *torus, struct device_node *np)
{
    struct bgtorus_irq *desc = bgtorus_irqs;
    int count = 0, idx, virq, rc;

    while(desc->count) {
	for (idx = 0; idx < desc->count; idx++) {
	    if (desc->used) {
		if ((virq = irq_of_parse_and_map(np, count + idx)) == NO_IRQ)
		    return -EINVAL;

		if ((rc = request_irq(virq, desc->handler,
				      IRQF_DISABLED, desc->name, torus)))
		    return rc;

		/* swing the IRQ handler from level to fasteoi to avoid
		 * double IRQs */
		set_irq_handler(virq, handle_fasteoi_irq);
	    } else
		virq = NO_IRQ;
	    BUG_ON(count + idx >= BGP_TORUS_MAX_IRQS);
	    torus->virq[count + idx] = virq;
	    //printk("IRQ %d (%d): %s\n", virq, count + idx, desc->name);
	}
	count += desc->count;
	desc++;
    }
    return 0;
}

static void bgtorus_unmap_irqs(struct bg_torus *torus)
{
    struct bgtorus_irq *desc = bgtorus_irqs;
    int idx, count = 0;

    while(desc->count) {
	for (idx = 0; idx < desc->count; idx++)
	    if (torus->virq[count + idx] != NO_IRQ) {
		free_irq(torus->virq[count + idx], torus);
		irq_dispose_mapping(torus->virq[count + idx]);
	    }
	count += desc->count;
	desc++;
    }
}

static void *init_fifo(struct torus_fifo *fifo,
		       unsigned long size, dma_addr_t *paddr)
{
    void *vaddr = dma_alloc_coherent(NULL, size, paddr, GFP_KERNEL);

    pr_debug("torus: fifo init paddr=%llx, vaddr=%p\n", *paddr, vaddr);

    if (vaddr) {
	/* addresses have to be 16bit aligned */
	BUG_ON(*paddr & 0xf);
	out_be32(&fifo->start, *paddr >> 4);
	out_be32(&fifo->end, (*paddr + size) >> 4);
	out_be32(&fifo->head, *paddr >> 4);
	out_be32(&fifo->tail, *paddr >> 4);
    }
    return vaddr;
}

/* mapping from pid -> (group/rcv fifo) */
static void set_rcv_fifo_map(struct bg_torus *torus, int pid, int group,
			     int fifoid)
{
    u8 fifomap = (group << 3) + (fifoid & 0x7);

    mtdcrx(_BGP_DCR_DMA + 0x60 + (pid * 2),
	   _rDMA_TS_REC_FIFO_MAP_XP(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_XM(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_YP(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_YM(fifomap));

    mtdcrx(_BGP_DCR_DMA + 0x61 + (pid * 2),
	   _rDMA_TS_REC_FIFO_MAP_ZP(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_ZM(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_HIGH(fifomap) |
	   _rDMA_TS_REC_FIFO_MAP_LOCAL(fifomap));
}

static void set_inj_fifo_map(struct bg_torus *torus, int group, int fifo,
			     u8 fifomap)
{
    u32 val = mfdcrx(_BGP_DCR_DMA + 0x3c + group * 8 + (fifo / 4));
    val &= ~(0xff000000 >> (8 * (fifo % 4)));
    val |= (u32)fifomap << (8 * (3 - (fifo % 4)));
    mtdcrx(_BGP_DCR_DMA + 0x3c + group * 8 + (fifo / 4), val);
}

static void set_inj_fifo_prio(struct bg_torus *torus, int group, int fifo,
			      int prio)
{
    u32 val = mfdcrx(_BGP_DCR_DMA + 0x32 + group);
    val &= ~(0x80000000 >> (fifo % 32));
    val |= ((prio << 31) >> (fifo % 32));
    mtdcrx(_BGP_DCR_DMA + 0x32 + group, val);
}

static void set_inj_fifo_local_dma(struct bg_torus *torus, int group, int fifo,
				   int local_dma)
{
    u32 val = mfdcrx(_BGP_DCR_DMA + 0x5c + group);
    val &= ~(0x80000000 >> (fifo % 32));
    val |= ((local_dma << 31) >> (fifo % 32));
    mtdcrx(_BGP_DCR_DMA + 0x5c + group, val);
}

static void config_inj_fifo(struct bg_torus *torus, int group, int fifo,
			    u8 fifomap, int prio, int local_dma)
{
    set_inj_fifo_map(torus, group, fifo, fifomap);
    set_inj_fifo_prio(torus, group, fifo, prio);
    set_inj_fifo_local_dma(torus, group, fifo, local_dma);
}

static void reset_inj_fifo(struct bg_torus *torus, int group, int fifo)
{
    config_inj_fifo(torus, group, fifo, BGP_DEFAULT_FIFOMAP, 0, 0);
}

/*
 * The torus is already initialized by the boot loader.  We only
 * initialize the DMA unit
 */
static void bgtorus_reset(struct bg_torus *torus)
{
    int i, group;

    /* pull and release reset */
    mtdcrx(_BGP_DCR_DMA_RESET, 0xffffffff);
    udelay(50000);
    mtdcrx(_BGP_DCR_DMA_RESET, 0);
    udelay(50000);

    /* enable DMA use */
    mtdcrx(_BGP_DCR_DMA_BASE_CONTROL, _DMA_BASE_CONTROL_USE_DMA);

    /* set receive DMA ranges to all */
    set_dma_receive_range(torus, 0, 0, 0xffffffff);
    set_dma_inject_range(torus, 0, 0, 0xffffffff);

    /* clear rest */
    for (i = 1; i < BGP_TORUS_DMA_REGIONS; i++) {
	set_dma_receive_range(torus, i, 1, 0);
	set_dma_inject_range(torus, i, 1, 0);
    }

    /* set injection fifo watermarks */
    mtdcrx(_BGP_DCR_iDMA_TS_FIFO_WM(0), _iDMA_TS_FIFO_WM0_INIT);
    mtdcrx(_BGP_DCR_iDMA_TS_FIFO_WM(1), _iDMA_TS_FIFO_WM1_INIT);

    /* set reception fifo watermarks */
    for (i = 0; i < BGP_TORUS_GROUPS; i++)
	mtdcrx(_BGP_DCR_rDMA_TS_FIFO_WM(i),_rDMA_TS_FIFO_WM0_INIT);

    /* set local fifo watermark */
    mtdcrx(_BGP_DCR_iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY,
	   _iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT);
    mtdcrx(_BGP_DCR_rDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY,
	   _rDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT);

    mtdcrx(_BGP_DCR_iDMA_SERVICE_QUANTA, _iDMA_SERVICE_QUANTA_INIT);

    mtdcrx(_BGP_DCR_rDMA_FIFO_CLEAR_MASK(0),_rDMA_FIFO_CLEAR_MASK0_INIT);
    mtdcrx(_BGP_DCR_rDMA_FIFO_CLEAR_MASK(1),_rDMA_FIFO_CLEAR_MASK1_INIT);
    mtdcrx(_BGP_DCR_rDMA_FIFO_CLEAR_MASK(2),_rDMA_FIFO_CLEAR_MASK2_INIT);
    mtdcrx(_BGP_DCR_rDMA_FIFO_CLEAR_MASK(3),_rDMA_FIFO_CLEAR_MASK3_INIT);
    mtdcrx(_BGP_DCR_rDMA_FIFO_HEADER_CLEAR_MASK,
					_rDMA_FIFO_HEADER_CLEAR_MASK_INIT);

    /* disable all counters, init all fifos so that nothing bad can happen... */
    for (group = 0; group < BGP_TORUS_GROUPS; group++) {

	for (i = 0; i < BGP_TORUS_COUNTERS; i++) {
	    out_be32(&torus->dma[group].inj.counter[i].counter, ~0U);
	    out_be32(&torus->dma[group].inj.counter[i].base, 0);
	    out_be32(&torus->dma[group].rcv.counter[i].counter, ~0U);
	    out_be32(&torus->dma[group].rcv.counter[i].base, 0);
	    out_be32(&torus->dma[group].rcv.counter[i].limit, ~0);
	}

	out_be32(&torus->dma[group].inj.counter_disable[0], ~0U);
	out_be32(&torus->dma[group].inj.counter_disable[1], ~0U);
	out_be32(&torus->dma[group].rcv.counter_disable[0], ~0U);
	out_be32(&torus->dma[group].rcv.counter_disable[1], ~0U);

#if 0
	printk("reset %d #6.2...\n", group);
	out_be32(&torus->dma[group].inj.counter_clear_hit_zero[0], ~0U);
	out_be32(&torus->dma[group].inj.counter_clear_hit_zero[1], ~0U);
	out_be32(&torus->dma[group].rcv.counter_clear_hit_zero[0], ~0U);
	out_be32(&torus->dma[group].rcv.counter_clear_hit_zero[1], ~0U);
#endif

	out_be32(&torus->dma[group].inj.dma_deactivate, ~0U);

	for (i = 0; i < BGP_TORUS_INJ_FIFOS; i++) {
	    struct torus_fifo *fifo = &torus->dma[group].inj.fifo[i];
	    out_be32(&fifo->start, 0);
	    out_be32(&fifo->end, 0);
	    out_be32(&fifo->head, 0);
	    out_be32(&fifo->tail, 0);

	    reset_inj_fifo(torus, group, i);
	}

	for (i = 0; i < BGP_TORUS_RCV_FIFOS + 1; i++) {
	    struct torus_fifo *fifo = (i < BGP_TORUS_RCV_FIFOS) ?
		&torus->dma[group].rcv.fifo[i] :
		&torus->dma[group].rcv.hdrfifo;
	    out_be32(&fifo->start, 0);
	    out_be32(&fifo->end, 0);
	    out_be32(&fifo->head, 0);
	    out_be32(&fifo->tail, 0);
	}

	/* clear thresholds */
	out_be32(&torus->dma[group].inj.clear_threshold, ~0U);
	out_be32(&torus->dma[group].rcv.clear_threshold[0], ~0U);
	out_be32(&torus->dma[group].rcv.clear_threshold[1], ~0U);
    }

    /* clear error count dcrs */
    for (i = 0; i < 9; i++)
	mtdcrx(_BGP_DCR_DMA_CE_COUNT(i), 0);

    /* set all the error count threshold and enable all interrupts */
    mtdcrx(_BGP_DCR_DMA_CE_COUNT_THRESHOLD,
		_BGP_DCR_DMA_CE_COUNT_THRESHOLD_INIT);

    /* clear the errors associate with the UE's on initial sram writes */
    mtdcrx(_BGP_DCR_DMA_CLEAR0, ~0U);
    mtdcrx(_BGP_DCR_DMA_CLEAR1, ~0U);

    for (i = 0; i < BGP_TORUS_GROUPS; i++)
	mtdcrx(_BGP_DCR_DMA_FATAL_ERROR_ENABLE(i), ~0U);

    /* enable the state machines (enable DMA, L3Burst, inj, rcv, imfu, rmfu) */
    mtdcrx(_BGP_DCR_DMA_BASE_CONTROL, _BGP_DCR_DMA_BASE_CONTROL_INIT);

    isync();
}

static void init_tx_queue(struct bg_torus *torus, int group, int fifo,
			  int counter)
{
    int injidx = group * BGP_TORUS_INJ_FIFOS + fifo;
    struct torus_tx_ring *tx = &torus->tx[injidx];

    memset(tx, 0, sizeof(*tx));
    tx->desc = init_fifo(&torus->dma[group].inj.fifo[fifo],
			 sizeof(union torus_inj_desc) * BGP_TORUS_TX_ENTRIES,
			 &tx->paddr);
    tx->skbs = kzalloc(BGP_TORUS_TX_ENTRIES * sizeof(struct sk_buff*),
		       GFP_KERNEL);
    tx->start = tx->paddr >> 4;
    tx->counter = counter;
    spin_lock_init(&tx->lock);

    pr_debug("Torus: allocated tx queue %d: %p (start=%08x, end=%08x)\n",
	     injidx, tx->desc,
	     in_be32(&torus->dma[group].inj.fifo[fifo].start),
	     in_be32(&torus->dma[group].inj.fifo[fifo].end));
}

static void init_rx_queue(struct bg_torus *torus, int group, int fifo,
			  int region)
{
    int rcvidx = group * BGP_TORUS_RCV_FIFOS + fifo;
    struct torus_rx_ring *rx = &torus->rx[rcvidx];

    memset(rx, 0, sizeof(*rx));
    rx->desc = init_fifo(&torus->dma[group].rcv.fifo[fifo],
			 sizeof(union torus_rcv_desc) * BGP_TORUS_RX_ENTRIES,
			 &rx->paddr);
    rx->start = rx->paddr >> 4;
    skb_queue_head_init(&rx->skb_list);
    spin_lock_init(&rx->lock);
    rx->src_id.raw = TORUS_SOURCE_ID_NULL;

    /* XXX: set dma region for rx fifo */

    pr_debug("Torus: allocated rx queue %d: %p (start=%08x, end=%08x)\n",
	     rcvidx, rx->desc,
	     in_be32(&torus->dma[group].rcv.fifo[fifo].start),
	     in_be32(&torus->dma[group].rcv.fifo[fifo].end));
}

static int bgtorus_init(struct bg_torus *torus)
{
    int group = BGP_KERNEL_GROUP;
    int injfifo = BGP_KERNEL_INJ_FIFO;
    int rcvfifo = BGP_KERNEL_RCV_FIFO;
    int counter = BGP_KERNEL_INJ_COUNTER;
    int region = BGP_KERNEL_RCV_REGION;
    int pid;

    spin_lock_init(&torus->lock);

    bgtorus_reset(torus);

    reserve_group(torus->inj_fifo_map, BGP_TORUS_INJ_FIFOS, BGP_KERNEL_GROUP);
    reserve_group(torus->rcv_fifo_map, BGP_TORUS_RCV_FIFOS, BGP_KERNEL_GROUP);
    reserve_group(torus->inj_counter_map, BGP_TORUS_COUNTERS, BGP_KERNEL_GROUP);
    reserve_group(torus->rcv_counter_map, BGP_TORUS_COUNTERS, BGP_KERNEL_GROUP);

    /* create unique source and packet identifier */
    torus->source_id.conn_id = 0;
    torus->source_id.src_key = ( torus->coordinates[0] << 16 |
				 torus->coordinates[1] <<  8 |
				 torus->coordinates[2] <<  0 );

    config_inj_fifo(torus, group, injfifo, BGP_KERNEL_FIFOMAP, 1, 0);

     torus->inj_counter_to_txidx[group * BGP_TORUS_COUNTERS + counter] =
	group * BGP_TORUS_INJ_FIFOS + injfifo;

    /* Set all pids to point at the kernel's reception fifo. */
    for (pid = 0; pid < BGP_TORUS_PIDS; pid++)
	set_rcv_fifo_map(torus, pid, group, rcvfifo);

    if (!reserve_rcv_dma_region(torus, region)) {
	printk("Torus: error reserving rcv region\n");
	return -ENODEV;
    }

    init_tx_queue(torus, group, injfifo, counter);
    init_rx_queue(torus, group, rcvfifo, region);

    init_inj_counter(torus, group, counter);
    set_inj_counter_zero_interrupt(torus, group, counter, 0);

    set_rcv_fifo_threshold(torus, 0, ((BGP_TORUS_RX_ENTRIES - 1) * 256) / 16);
    enable_rcv_fifo_threshold_interrupt(torus, 0, group, rcvfifo);

    enable_inj_fifo(torus, group, injfifo);
    enable_inj_fifo_user(torus, group, injfifo);

    enable_rcv_fifo(torus, group, rcvfifo);

    return 0;
}

static int __devinit bgtorus_probe(struct of_device *ofdev,
				   const struct of_device_id *match)
{
    struct bg_torus *torus;
    struct device_node *np = ofdev->node;
    const u32 *prop;
    unsigned int sz, idx;

    torus = kzalloc(sizeof(struct bg_torus), GFP_KERNEL);
    if (!torus)
	return -ENOMEM;

    torus->ofdev = ofdev;

    if ( of_address_to_resource(np, 0, &torus->pdma) ||
	 of_address_to_resource(np, 1, &torus->pfifo0) ||
	 of_address_to_resource(np, 2, &torus->pfifo1) )
	goto err_fdt;

    if ((torus->pdma.end - torus->pdma.start + 1) != BGP_TORUS_DMA_SIZE) {
	printk(KERN_ERR "%s: unexpected DMA register size in FDT."
	       "Disabling driver.\n", np->full_name);
	goto err_fdt;
    }

    if ((prop = of_get_property(np, "dcr-reg", &sz)) == 0)
	goto err_fdt;
    torus->dcr_base = prop[0];
    torus->dcr_size = prop[1];

    if ((prop = of_get_property(np, "dimension", &sz)) == NULL)
	goto err_fdt;
    for (idx = 0; idx < 3; idx++)
	torus->dimension[idx] = prop[idx];

    if ((prop = of_get_property(np, "coordinates", &sz)) == NULL)
	goto err_fdt;
    for (idx = 0; idx < 3; idx++)
	torus->coordinates[idx] = prop[idx];

    pr_debug("DCR: %x, %x\n", torus->dcr_base, torus->dcr_size);
    pr_debug("DMA: %llx, %llx, flags=%lx\n", torus->pdma.start,
	     torus->pdma.end, torus->pdma.flags);
    pr_debug("fifo0: %llx, %llx, flags=%lx\n", torus->pfifo0.start,
	     torus->pfifo0.end, torus->pfifo0.flags);
    pr_debug("fifo0: %llx, %llx, flags=%lx\n", torus->pfifo1.start,
	     torus->pfifo1.end, torus->pfifo1.flags);

    printk(KERN_INFO "%s: coordinates: [%d, %d, %d], dimension: [%d, %d, %d]\n",
	   np->full_name,
	   torus->coordinates[0], torus->coordinates[1], torus->coordinates[2],
	   torus->dimension[0], torus->dimension[1], torus->dimension[2]);

    if (!request_mem_region(torus->pdma.start, BGP_TORUS_DMA_SIZE, np->name)) {
	printk(KERN_ERR "%s: could not allocate device region\n",
	       np->full_name);
	goto err_fdt;
    }

    if (!(torus->dma = ioremap(torus->pdma.start, BGP_TORUS_DMA_SIZE))) {
	printk(KERN_ERR "%s: ioremap failed\n", np->full_name);
	goto err_remap;
    }

    if (bgtorus_map_irqs(torus, np)) {
	printk(KERN_ERR "Error mapping torus IRQs\n");
	goto err_irq;
    }

    if (bgtorus_init(torus))
	goto err_init;

    dev_set_drvdata(&ofdev->dev, torus);
    bgtorus_proc_register(torus);

#ifdef USER_MODE_TORUS_ACCESS
    bgtorus_userlevel_dma_init(torus);
#endif /* USER_MODE_TORUS_ACCESS */

    return 0;

 err_init:

 err_irq:
    bgtorus_unmap_irqs(torus);
    iounmap(torus->dma);

 err_remap:
    release_mem_region(torus->pdma.start, BGP_TORUS_DMA_SIZE);

 err_fdt:
    kfree(torus);

    return -ENODEV;
}

static int __devinit bgtorus_remove(struct of_device *ofdev)
{
    bgtorus_proc_unregister((struct bg_torus*)&ofdev->dev);
    dev_set_drvdata(&ofdev->dev, NULL);
    return 0;
}

static struct of_device_id bgtorus_match[] = {
    {
	.compatible = "ibm,bgp-torus",
    },
    {},
};

static struct of_platform_driver bgtorus_driver = {
    .name = DRV_NAME,
    .match_table = bgtorus_match,
    .probe = bgtorus_probe,
    .remove = bgtorus_remove,
};

static int __init bgtorus_module_init (void)
{
    printk(KERN_INFO DRV_DESC ", version " DRV_VERSION "\n");
    of_register_platform_driver(&bgtorus_driver);
    return 0;
}

static void __exit bgtorus_module_exit (void)
{
    of_unregister_platform_driver(&bgtorus_driver);
}

module_init(bgtorus_module_init);
module_exit(bgtorus_module_exit);

#if defined(CONFIG_SYSCTL) && defined(CONFIG_PROC_FS)
static int proc_do_status (ctl_table *ctl, int write, struct file * filp,
                          void __user *buffer, size_t *lenp, loff_t *ppos)
{
    struct bg_torus *torus = ctl->extra1;

    printk("torus ptr: %p\n", torus);
    dump_dcrs(torus);
    dump_inj_channels(ctl->extra1, BGP_KERNEL_GROUP);
    dump_rcv_channels(ctl->extra1, BGP_KERNEL_GROUP);

    *lenp = 0;
    return 0;
}

//#define PROC_SEND
#ifdef PROC_SEND
static int proc_do_send (ctl_table *ctl, int write, struct file * filp,
                          void __user *buffer, size_t *lenp, loff_t *ppos)
{
    int rc, i;
    int buf[4] = {0, 0, 0, 0};
    struct sk_buff *skb;
    struct bg_torus *torus = ctl->extra1;
    struct bglink_hdr_torus *lnkhdr;
    union torus_inj_desc desc;

    if (!torus)
	return -ENOSYS;

    if (!write)
	return -ENOSYS;

    ctl->data = buf;
    ctl->maxlen = sizeof(buf);

    rc = proc_dointvec(ctl, write, filp, buffer, lenp, ppos);

    printk("torus send: x=%d, y=%d, z=%d, len=%d\n",
	   buf[0], buf[1], buf[2], buf[3]);

    skb = dev_alloc_skb(buf[3] + sizeof(struct bglink_hdr_torus));
    if (!skb)
	return -ENOMEM;

    skb_put(skb,  buf[3] + sizeof(struct bglink_hdr_torus));
    for (i = 0; i < skb->len; i++)
	((unsigned char*)skb->data)[i] = i;

    /* allocate headroom for torus packet link layer */
    lnkhdr = (struct bglink_hdr_torus*)skb->data;

    lnkhdr->dst_key = 10; /* random */
    lnkhdr->len = buf[3];
    lnkhdr->lnk_proto = BGLINK_P_NET;
    lnkhdr->opt.raw = 0;

    printk("sending skb (%p, data=%p, len=%d)\n", skb, skb->data, skb->len);

    bgtorus_init_inj_desc(torus, &desc, skb->len, buf[0], buf[1], buf[2]);
    bgtorus_xmit(torus, &desc, skb);

    return rc;
}
#endif

static struct ctl_table torus_table[] = {
    {
	.ctl_name = CTL_UNNUMBERED,
	.procname = "status",
	.mode = 0444,
	.proc_handler = proc_do_status,
    },
#ifdef PROC_SEND
    {
	.ctl_name = CTL_UNNUMBERED,
	.procname = "send",
	.mode = 0666,
	.proc_handler = proc_do_send,
    },
#endif
    { .ctl_name = 0 }
};

static struct ctl_table torus_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "bgtorus",
	.mode		= 0555,
	.child		= torus_table,
    },
    { .ctl_name = 0 }
};

static struct ctl_table dev_root[] = {
    {
	.ctl_name	= CTL_DEV,
	.procname	= "dev",
	.maxlen		= 0,
	.mode		= 0555,
	.child		= torus_root,
    },
    { .ctl_name = 0 }
};

static int bgtorus_proc_register(struct bg_torus *torus)
{
    struct ctl_table *table = torus_table;
    while(table->procname) {
	table->extra1 = torus;
	table++;
    }
    torus->sysctl_header = register_sysctl_table(dev_root);
    return 0;
}

static int bgtorus_proc_unregister(struct bg_torus *torus)
{
    if (torus->sysctl_header) {
	unregister_sysctl_table(torus->sysctl_header);
	torus->sysctl_header = NULL;
    }
    return 0;
}
#else
static int bgtorus_proc_register(struct bg_torus *torus) { return 0; }
static int bgtorus_proc_unregister(struct bg_torus *torus) { return 0; }
#endif

#ifdef USER_MODE_TORUS_ACCESS
/**********************************************************************
 *
 *			  Torus UL DMA hack
 *
 **********************************************************************/

static struct bg_torus *rdma_torus;

struct torus_file {
    struct bg_torus *torus;

    DECLARE_BITMAP(inj_counter_map, BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(rcv_counter_map, BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(inj_fifo_map, BGP_TORUS_INJ_FIFOS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(rcv_fifo_map, BGP_TORUS_RCV_FIFOS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(rcv_dma_region_map, BGP_TORUS_DMA_REGIONS);

    wait_queue_head_t waitq;
    volatile u32 *rcv_ctr_hint;	/* non-NULL iff this file has exactly one */
				/* reception counter allocated to it */
};

static void set_rcv_ctr_hint(struct bg_torus *torus, struct torus_file *tfile)
{
    int ctridx = find_first_bit(tfile->rcv_counter_map,
				BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS);
    if ((ctridx < (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS)) &&
	(find_next_bit(tfile->rcv_counter_map,
		       BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS, ctridx+1) >=
				(BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS))) {
	/* exactly one bit in set */
	int group = ctridx / BGP_TORUS_COUNTERS;
	int ctr = ctridx % BGP_TORUS_COUNTERS;
	tfile->rcv_ctr_hint = &torus->dma[group].rcv.counter[ctr].counter;
    } else {
	/* 0 or 2 or more bits in set */
	tfile->rcv_ctr_hint = NULL;
    }
}

static ssize_t torus_read(struct file * file, char __user * buf,
			  size_t count, loff_t *ppos)
{
    printk("read_torus %d\n", count);
    return -EFAULT;
}

static ssize_t torus_write(struct file * file, const char __user * buf,
			   size_t count, loff_t *ppos)
{
    printk("write_torus %d\n", count);
    return -EFAULT;
}

static int torus_mmap(struct file * file, struct vm_area_struct * vma)
{
    size_t size = vma->vm_end - vma->vm_start;
    unsigned long pfn;

    printk("mmapping torus fifo (len=0x%x, offset=0x%lx)\n",
	   size, vma->vm_pgoff * PAGE_SIZE);

    if ((vma->vm_pgoff + (size / PAGE_SIZE)) > BGP_TORUS_GROUPS)
	return -EINVAL;

    /* Don't allow user to map the kernel's DMA group */
    if ((vma->vm_pgoff <= BGP_KERNEL_GROUP) &&
	(BGP_KERNEL_GROUP < (vma->vm_pgoff + (size / PAGE_SIZE))))
	return -EINVAL;

    pfn = (rdma_torus->pdma.start / PAGE_SIZE) + vma->vm_pgoff;

    vma->vm_file = file;
    vma->vm_page_prot = phys_mem_access_prot(file, pfn, size,
					     vma->vm_page_prot);

    if (io_remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
	return -EAGAIN;

    return 0;
}

static int torus_open(struct inode * inode, struct file * file)
{
    struct torus_file *tfile;

    if (rdma_torus == NULL)
	return -ENOSYS;

    tfile = kzalloc(sizeof(struct torus_file), GFP_KERNEL);
    if (!tfile)
	return -ENOMEM;

    init_waitqueue_head(&tfile->waitq);
    tfile->torus = rdma_torus;
    file->private_data = tfile;
    return 0;
}

static int torus_release(struct inode *inode, struct file *file)
{
    int bit;
    struct torus_file *tfile = file->private_data;
    struct bg_torus *torus = tfile->torus;

    printk("torus release\n");

    bit = find_first_bit(tfile->inj_counter_map,
			 (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS));
    while (bit < (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS)) {
	printk(" freeing inj counter %d\n", bit);
	release_inj_counter(torus, bit);
	bit = find_next_bit(tfile->inj_counter_map,
			    (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS), bit+1);
    }

    bit = find_first_bit(tfile->rcv_counter_map,
			 (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS));
    while (bit < (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS)) {
	printk(" freeing rcv counter %d\n", bit);
	torus->rcv_counter_to_tfile[bit] = NULL;
	disable_rcv_counter(torus,
			    bit / BGP_TORUS_COUNTERS,
			    bit % BGP_TORUS_COUNTERS);
	release_rcv_counter(torus, bit);
	bit = find_next_bit(tfile->rcv_counter_map,
			    (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS), bit+1);
    }
    set_rcv_ctr_hint(torus, tfile);

    bit = find_first_bit(tfile->inj_fifo_map,
			 (BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS));
    while (bit < (BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS)) {
	int group = bit / BGP_TORUS_INJ_FIFOS;
	int fifo = bit % BGP_TORUS_INJ_FIFOS;
	disable_inj_fifo_user(torus, group, fifo);
	disable_inj_fifo(torus, group, fifo);
	reset_inj_fifo(torus, group, fifo);
	printk(" freeing inj fifo %d\n", bit);
	release_inj_fifo(torus, bit);
	bit = find_next_bit(tfile->inj_fifo_map,
			    (BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS), bit+1);
    }

    bit = find_first_bit(tfile->rcv_fifo_map,
			 (BGP_TORUS_GROUPS * BGP_TORUS_RCV_FIFOS));
    while (bit < (BGP_TORUS_GROUPS * BGP_TORUS_RCV_FIFOS)) {
	int group = bit / BGP_TORUS_RCV_FIFOS;
	int fifo = bit % BGP_TORUS_RCV_FIFOS;
	disable_rcv_fifo(torus, group, fifo);
	printk(" freeing rcv fifo %d\n", bit);
	release_rcv_fifo(torus, bit);
	bit = find_next_bit(tfile->rcv_fifo_map,
			    (BGP_TORUS_GROUPS * BGP_TORUS_RCV_FIFOS), bit+1);
    }

    bit = find_first_bit(tfile->rcv_dma_region_map, BGP_TORUS_DMA_REGIONS);
    while (bit < BGP_TORUS_DMA_REGIONS) {
	printk(" freeing dma region %d\n", bit);
	release_rcv_dma_region(torus, bit);
	bit = find_next_bit(tfile->rcv_dma_region_map, BGP_TORUS_DMA_REGIONS,
			    bit+1);
    }

    kfree(file->private_data);
    return 0;
}

static unsigned int torus_poll(struct file *file, poll_table *wait)
{
    struct torus_file *tfile = file->private_data;
    unsigned int mask = 0;

    poll_wait(file, &tfile->waitq, wait);

    if (tfile->rcv_ctr_hint != NULL) {
	if (((signed int) (*tfile->rcv_ctr_hint)) <= 0) {
	    mask |= POLLIN | POLLRDNORM;
	}
    } else {
	int ctridx = find_first_bit(tfile->rcv_counter_map,
				    (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS));
	while (ctridx < (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS)) {
	    int group = ctridx / BGP_TORUS_COUNTERS;
	    int ctr = ctridx % BGP_TORUS_COUNTERS;
	    u32 val = tfile->torus->dma[group].rcv.counter[ctr].counter;
	    if (((signed int) val) <= 0) {
		mask |= POLLIN | POLLRDNORM;
		break;
	    }
	    ctridx = find_next_bit(tfile->rcv_counter_map,
				   (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS),
				   ctridx + 1);
	}
    }

    return mask;
}

static void bgtorus_userlevel_wakeup(struct bg_torus *torus,
				     int group, int ctr)
{
    struct torus_file *tfile =
	torus->rcv_counter_to_tfile[(group * BGP_TORUS_COUNTERS) + ctr];

    if (tfile != NULL)
	wake_up_interruptible(&tfile->waitq);
}


/*
 * VU: This is ugly but works for the time being.  We touch and pin
 * all pages writable using get_user_pages.  Then we iterate over the
 * pages and replace them with a linear allocation of the total size
 * effectively swapping them out.  This is not a time critical
 * operation and not really pretty either.
 */

struct remap_info {
    struct page *page;
    unsigned long base;
    unsigned long count;
    struct page **page_list;
};

/*
 * Copy the page to its new location
 */
static void migrate_page_copy(struct page *newpage, struct page *page)
{
	copy_highpage(newpage, page);

	if (PageError(page))
		SetPageError(newpage);
	if (PageReferenced(page))
		SetPageReferenced(newpage);
	if (PageUptodate(page))
		SetPageUptodate(newpage);
	if (PageActive(page))
		SetPageActive(newpage);
	if (PageChecked(page))
		SetPageChecked(newpage);
	if (PageMappedToDisk(page))
		SetPageMappedToDisk(newpage);

	if (PageDirty(page)) {
		clear_page_dirty_for_io(page);
		set_page_dirty(newpage);
	}

#ifdef CONFIG_SWAP
	ClearPageSwapCache(page);
#endif
	ClearPageActive(page);
	ClearPagePrivate(page);
	set_page_private(page, 0);
	page->mapping = NULL;

	/*
	 * If any waiters have accumulated on the new page then
	 * wake them up.
	 */
	if (PageWriteback(newpage))
		end_page_writeback(newpage);
}

static int torus_remap_pte_fn(pte_t *ptep, struct page *pmd_page,
			      unsigned long addr, void *data)
{
    struct remap_info *info = (struct remap_info*)data;
    unsigned long pidx = (addr - info->base) / PAGE_SIZE;
    struct page *newpage = info->page + pidx;
    struct page *oldpage;
    pte_t newpte = mk_pte(newpage, PAGE_SHARED);
    swp_entry_t entry;
    struct vm_area_struct *vma;

    entry = pte_to_swp_entry(*ptep);

    /* try_to_unmap placed page_list index into swp entry field */
    oldpage = info->page_list[entry.val];

    vma = find_vma(current->mm, addr);
    BUG_ON(!vma);

    printk("torus_remap_pte_fn: *pte=%llx, addr=%lx, oldpage=%p, "
	   "newpage=%p (%llx), cnt=%d, base=%lx, idx=%ld, entry=%08lx\n",
	   pte_val(*ptep), addr, oldpage, newpage, pte_val(newpte),
	   page_count(newpage), info->base, pidx, entry.val);

    migrate_page_copy(newpage, oldpage);
    page_add_anon_rmap(newpage, vma, addr);

    get_page(newpage);
    *ptep = newpte;

//    __free_page(oldpage);

    info->count++;
    return 0;
}

static int torus_remap_memory(unsigned long start, unsigned long size)
{
    struct page *page;
    struct page **page_list;
    int pgcnt = size / PAGE_SIZE;
    int ret, res, idx;
    struct remap_info info;

    printk("register memory: start=%lx, size=%lx\n", start, size);
    if (pgcnt > PAGE_SIZE / sizeof(page))
	return -EINVAL;

    page_list = (struct page**)__get_free_page(GFP_KERNEL);
    if (!page_list)
	return -ENOMEM;

    page = alloc_pages(GFP_KERNEL, get_order(size));
    if (!page)
	return -ENOMEM; // XXX: free page_list

    down_write(&current->mm->mmap_sem);

    ret = get_user_pages(current, current->mm, start, pgcnt,
			 1, 0, page_list, NULL);

    /* drain LRU list so pages can be isolated */
    lru_add_drain_all();

    for (idx = 0; idx < pgcnt; idx++) {
	struct page *pg = page_list[idx];
	if (TestSetPageLocked(pg))
	    BUG();
	pg->private = idx;
	//printk("try_to_unmap: %p, cnt=%d\n", pg, page_count(pg));
	res = try_to_unmap(pg, 1);
	//printk("try_to_unmap done: %p, cnt=%d\n", pg, page_count(pg));
	if (res) {
	    printk("unmapping failed: undo operation\n");
	    BUG();
	}
    }

    info.page = page;
    info.base = start;
    info.count = 0;
    info.page_list = page_list;

    split_page(page, get_order(size));
    res = apply_to_page_range(current->mm, start, size,
			      torus_remap_pte_fn, &info);
    if (res < 0) {
	printk("remapping failed: undo operation\n");
	BUG();
	goto err;
    }

    /* we allocated a power-of-two sized page but may have only
     * remapped a part.  We free the remaining pieces. */
    for (idx = pgcnt; idx < (1 << get_order(size)); idx++)
	__free_page(page + idx);

    up_write(&current->mm->mmap_sem);

    return page_to_pfn(page);

 err:
    up_write(&current->mm->mmap_sem);
    free_pages((unsigned long)page, get_order(size));

    return ret;
}

static int torus_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
    struct torus_file *tfile = file->private_data;
    struct bg_torus *torus = tfile->torus;

    /* XXX: all not really SMP safe.  Use spinlock around ops */

    printk("torus IOCTL: cmd=%x, arg=%lx\n", cmd, arg);

    switch(cmd) {
    case TORUS_ALLOC_TX_COUNTER:
    {
	int ctridx = allocate_inj_counter(torus);
	if (ctridx < 0)
	    return -EBUSY;
	set_bit(ctridx, tfile->inj_counter_map);
	printk("allocate_tx_counter: %d\n", ctridx);
	return ctridx;
    }
    case TORUS_ALLOC_RX_COUNTER:
    {
	int ctridx = allocate_rcv_counter(torus);
	if (ctridx < 0)
	    return -EBUSY;
	set_bit(ctridx, tfile->rcv_counter_map);
	set_rcv_ctr_hint(torus, tfile);
	torus->rcv_counter_to_tfile[ctridx] = tfile;
	printk("allocate_rx_counter: %d\n", ctridx);
	return ctridx;
    }
    case TORUS_FREE_TX_COUNTER:
    {
	if (arg >= (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS))
	    return -EINVAL;
	if (!test_and_clear_bit(arg, tfile->inj_counter_map))
	    return -EINVAL;
	release_inj_counter(torus, arg);
	printk("free_tx_counter %ld\n", arg);
	return 0;
    }
    case TORUS_FREE_RX_COUNTER:
    {
	if (arg >= (BGP_TORUS_GROUPS * BGP_TORUS_COUNTERS))
	    return -EINVAL;
	if (!test_and_clear_bit(arg, tfile->rcv_counter_map))
	    return -EINVAL;
	set_rcv_ctr_hint(torus, tfile);
	torus->rcv_counter_to_tfile[arg] = NULL;
	disable_rcv_counter(torus,
			    arg / BGP_TORUS_COUNTERS,
			    arg % BGP_TORUS_COUNTERS);
	release_rcv_counter(torus, arg);
	printk("free_rx_counter %ld\n", arg);
	return 0;
    }
    case TORUS_ALLOC_TX_FIFO:
    {
	int group, fifo;
	int fifoidx = allocate_inj_fifo(torus);
	if (fifoidx < 0)
	    return -EBUSY;
	set_bit(fifoidx, tfile->inj_fifo_map);

	/* the inject fifo is in an unknown state: disable user inject first */
	group = fifoidx / BGP_TORUS_INJ_FIFOS;
	fifo = fifoidx % BGP_TORUS_INJ_FIFOS;
	disable_inj_fifo_user(torus, group, fifo);
	enable_inj_fifo(torus, group, fifo);
	printk("alloc_inj_fifo: f=%d\n", fifoidx);
	return fifoidx;
    }
    case TORUS_ALLOC_RX_FIFO:
    {
	int group, fifo;
	int fifoidx = allocate_rcv_fifo(torus);
	if (fifoidx < 0)
	    return -EBUSY;
	set_bit(fifoidx, tfile->rcv_fifo_map);
	group = fifoidx / BGP_TORUS_RCV_FIFOS;
	fifo = fifoidx % BGP_TORUS_RCV_FIFOS;
	enable_rcv_fifo(tfile->torus, group, fifo);
	printk("alloc_rcv_fifo: %d\n", fifoidx);
	return fifoidx;
    }
    case TORUS_FREE_TX_FIFO:
    {
	int group, fifo;
	if (arg >= (BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS))
	    return -EINVAL;
	if (!test_and_clear_bit(arg, tfile->inj_fifo_map))
	    return -EINVAL;
	release_inj_fifo(torus, arg);
	group = arg / BGP_TORUS_INJ_FIFOS;
	fifo = arg % BGP_TORUS_INJ_FIFOS;
	disable_inj_fifo_user(torus, group, fifo);
	disable_inj_fifo(torus, group, fifo);
	reset_inj_fifo(torus, group, fifo);
	printk("disable_inj_fifo: %ld\n", arg);
	return 0;
    }
    case TORUS_FREE_RX_FIFO:
    {
	int group, fifo;
	if (arg >= (BGP_TORUS_GROUPS * BGP_TORUS_RCV_FIFOS))
	    return -EINVAL;
	if (!test_and_clear_bit(arg, tfile->rcv_fifo_map))
	    return -EINVAL;
	release_rcv_fifo(torus, arg);
	group = arg / BGP_TORUS_RCV_FIFOS;
	fifo = arg % BGP_TORUS_RCV_FIFOS;
	disable_rcv_fifo(torus, group, fifo);
	printk("disable_rcv_fifo: %ld\n", arg);
	return 0;
    }
    case TORUS_REGISTER_TX_MEM: /* registers mem, return: phys base address */
    {
	struct {
	    unsigned long start;
	    unsigned long size;
	} param;

	if (copy_from_user(&param, (void __user *)arg, sizeof(param)))
	    return -EFAULT;

	return torus_remap_memory(param.start, param.size);
    }
    case TORUS_REGISTER_RX_MEM:
    {
	int res, region;
	struct {
	    unsigned long start;
	    unsigned long size;
	} param;

	if (copy_from_user(&param, (void __user *)arg, sizeof(param)))
	    return -EFAULT;

	region = allocate_rcv_dma_region(torus);
	if (region < 0)
	    return -EBUSY;

	res = torus_remap_memory(param.start, param.size);

	if (res < 0)
	    release_rcv_dma_region(torus, region);
	else
	    set_bit(region, tfile->rcv_dma_region_map);

	return res;
    }
    case TORUS_DMA_RANGECHECK:
    {
	/* enable/disable mem check, privileged and global!!! */
	return 0;
    }
    case TORUS_CONFIG_TX_FIFO:
    {
	int group, fifo;
	struct {
	    unsigned int fifo;
	    unsigned int map;
	    unsigned int priority;
	    unsigned int local_dma;
	} param;

	if (copy_from_user(&param, (void __user *)arg, sizeof(param)))
	    return -EFAULT;
	if ((param.fifo >= (BGP_TORUS_GROUPS * BGP_TORUS_INJ_FIFOS)) ||
	    !test_bit(param.fifo, tfile->inj_fifo_map))
	    return -EINVAL;

	group = param.fifo / BGP_TORUS_INJ_FIFOS;
	fifo = param.fifo % BGP_TORUS_INJ_FIFOS;
	disable_inj_fifo_user(torus, group, fifo);
	disable_inj_fifo(torus, group, fifo);
	config_inj_fifo(torus, group, fifo,
			param.map, param.priority, param.local_dma);
	enable_inj_fifo(torus, group, fifo);
	printk("config_inj_fifo: %d\n", param.fifo);
	return 0;
    }

#ifdef BGSM_SUPPORT
    case TORUS_BGSM_ALLOC_TX_COUNTER:
    {
	if (!reserve_inj_counter(torus, arg))
	    return -EBUSY;
	set_bit(arg, tfile->inj_counter_map);
	printk("allocate_bgsm_tx_counter: %ld\n", arg);
	return 0;
    }
    case TORUS_BGSM_ALLOC_RX_COUNTER:
    {
	if (!reserve_rcv_counter(torus, arg)) {
	    /* The counter is reserved.  It might have been removed from
	     * service.  Try to restore it.  If that succeeds, proceed with
	     * the reservation.  Otherwise, the reservation fails.
	     */
	    if (restore_rcv_counter(torus, arg))
		printk("torus: ****************************************\n"
		       "torus: Restoring out-of-service RX counter %ld.\n"
		       "torus: ****************************************\n",
		       arg);
	    else
		return -EBUSY;
	}
	set_bit(arg, tfile->rcv_counter_map);
	set_rcv_ctr_hint(torus, tfile);
	torus->rcv_counter_to_tfile[arg] = tfile;
	printk("allocate_bgsm_rx_counter: %ld\n", arg);
	return 0;
    }
    case TORUS_BGSM_ALLOC_TX_FIFO:
    {
	int group, fifo;
	if (!reserve_inj_fifo(torus, arg))
	    return -EBUSY;
	set_bit(arg, tfile->inj_fifo_map);

	/* the inject fifo is in an unknown state: disable user inject first */
	group = arg / BGP_TORUS_INJ_FIFOS;
	fifo = arg % BGP_TORUS_INJ_FIFOS;
	disable_inj_fifo_user(torus, group, fifo);
	enable_inj_fifo(torus, group, fifo);
	printk("allocate_bgsm_tx_fifo: %ld\n", arg);
	return 0;
    }
    case TORUS_BGSM_ALLOC_RX_FIFO:
    {
	int group, fifo;
	if (!reserve_rcv_fifo(torus, arg))
	    return -EBUSY;
	set_bit(arg, tfile->rcv_fifo_map);
	group = arg / BGP_TORUS_RCV_FIFOS;
	fifo = arg % BGP_TORUS_RCV_FIFOS;
	enable_rcv_fifo(tfile->torus, group, fifo);
	printk("alloc_bgsm_tx_fifo: %ld\n", arg);
	return 0;
    }
#endif /* BGSM_SUPPORT */

    default:
	return -ENOTTY;
    }
}

static const struct file_operations torus_fops = {
    .owner = THIS_MODULE,
    .open = torus_open,
    .read = torus_read,
    .write = torus_write,
    .mmap = torus_mmap,
    .poll = torus_poll,
    .ioctl = torus_ioctl,
    .release = torus_release,
};

/* use infiniband major */
#define TORUS_MAJOR	231

static int __init bgtorus_userlevel_dma_init(struct bg_torus *torus)
{
    printk("Torus: userlevel DMA init\n");

    rdma_torus = torus;

    if (register_chrdev(TORUS_MAJOR,"bgtorus", &torus_fops))
	printk("unable to get major %d for torus\n", TORUS_MAJOR);

    enable_rcv_counter_zero_interrupts(torus, 0);

    return 0;
}
#endif /* USER_MODE_TORUS_ACCESS */
