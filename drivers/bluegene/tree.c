/*********************************************************************
 *
 * Copyright (C) 2007-2008, IBM Corporation, Project Kittyhawk
 *		       Volkmar Uhlig (vuhlig@us.ibm.com)
 *
 * Description: Blue Gene low-level driver for tree
 *
 * All rights reserved
 *
 ********************************************************************/

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <net/arp.h>

#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/prom.h>
#include <asm/of_platform.h>

#include "link.h"
#include "tree.h"
#include "bgp_dcr.h"
#include "ppc450.h"

#define DRV_NAME	"bgtree"
#define DRV_VERSION	"0.2"
#define DRV_DESC	"Blue Gene Collective (IBM, Project Kittyhawk)"

MODULE_DESCRIPTION(DRV_DESC);
MODULE_AUTHOR("IBM, Project Kittyhawk");


//#define TRACE(x...) printk(x)
#define TRACE(x...)

#define _BGP_DCR_TREE 0

#define FRAGMENT_TIMEOUT	(HZ/10)
#define FRAGMENT_LISTS		256

#define TREE_LNKHDRLEN		(sizeof(struct bglink_hdr_tree))
#define TREE_FRAGPAYLOAD	(TREE_PAYLOAD - TREE_LNKHDRLEN)
#define TREE_SKB_ALIGN		16

extern void bgic_raise_irq(unsigned int hwirq);
static int bgtree_proc_register(struct bg_tree *tree);
static int bgtree_proc_unregister(struct bg_tree *tree);


/*
 * device management
 */

struct bgtree_channel {
    struct resource paddr;
    unsigned long mioaddr;
    unsigned int dcrbase;
    unsigned long irq_rcv_pending_mask;
    unsigned long irq_inj_pending_mask;
    struct timer_list inj_timer;
    unsigned int inj_wedged;
    unsigned int injected;
    unsigned int partial_injections;
    unsigned int unaligned_hdr_injections;
    unsigned int unaligned_data_injections;
    unsigned int received;
    unsigned int inject_fail;
    unsigned int dropped;
    unsigned int delivered;
};

#define MCAST_TARGET_MAX 1

struct mcast_table_entry {
    unsigned int group;
    unsigned int target[MCAST_TARGET_MAX];
};

struct bg_tree {
    spinlock_t lock;
    spinlock_t irq_lock;
    struct bgtree_channel chn[BGP_MAX_CHANNEL];
    unsigned int dcrbase;
    unsigned int curr_conn;
    struct sk_buff_head skb_list[FRAGMENT_LISTS];
    unsigned int nodeid;
    unsigned int inj_wm_mask;
    struct mcast_table_entry *mcast_table;
    int mcast_table_cur;
    int mcast_table_size;

    /* statistics */
    unsigned fragment_timeout;
    struct device_node *node;
    struct of_device *ofdev;
    struct ctl_table_header *sysctl_header;
};

static struct bg_tree *__tree;
struct bg_tree *bgtree_get_dev()
{
    return __tree;
}

unsigned int bgtree_get_nodeid(struct bg_tree* tree)
{
    return tree->nodeid;
}

/**********************************************************************
 * IRQs
 **********************************************************************/

irqreturn_t bgtree_unhandled_interrupt(int irq, void *dev)
{
    panic("tree: unhandled irq %d\n", irq);
}

static irqreturn_t bgtree_inject_interrupt(int irq, void *dev);
static irqreturn_t bgtree_receive_interrupt(int irq, void *dev);

#define IRQ_IDX_INJECT	0
#define IRQ_IDX_RECEIVE	1

#define DEF_IRQ(_name, _handler) \
{ .irq = 0, .name = _name, .handler = _handler }

static struct {
    unsigned irq;
    char *name;
    irqreturn_t (*handler)(int irq, void *dev);
} bgtree_irqs [] = {
    DEF_IRQ("Tree inject", bgtree_inject_interrupt),	/* IRQ_IDX_INJECT */
    DEF_IRQ("Tree receive", bgtree_receive_interrupt),	/* IRQ_IDX_RECEIVE */
    DEF_IRQ("Tree VC0", bgtree_receive_interrupt),
    DEF_IRQ("Tree VC1", bgtree_receive_interrupt),
#if 0
    DEF_IRQ("Tree CRNI timeout", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree no-target", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ALU overflow", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree local client inject", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree local client receive", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree write send CH0", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC send CH0", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC send CH0", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree write send CH1", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC send CH1", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC send CH1", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree write send CH2", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC send CH2", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC send CH2", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC rcv CH0", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC rcv CH0", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC rcv CH1", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC rcv CH1", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree ECC rcv CH2", bgtree_unhandled_interrupt),
    DEF_IRQ("Tree link CRC rcv CH2", bgtree_unhandled_interrupt),
#endif
    { 0, NULL, NULL }
};

/**********************************************************************
 * fragmentation handling
 **********************************************************************/

struct frag_skb
{
    unsigned long jiffies;
    struct bglink_hdr_tree lnk;
    unsigned char payload[0]; // begin of encapsulated payload
} __attribute__((packed));

/**********************************************************************
 *                                 Debug
 **********************************************************************/

void dump_skb(struct sk_buff *skb)
{
    int i;
    //return;

    //if (skb->len <= 280) return;

    for (i = 0; i < skb->len / 4 + 1; i++)
	printk("%08x%c", ((u32*)skb->data)[i], (i + 1) % 8 ? ' ' : '\n');
    printk("\n");
}

/**********************************************************************
 *                          Interrupt handling
 **********************************************************************/

static void bgtree_enable_interrupts(struct bg_tree *tree)
{
    unsigned long flags;
    spin_lock_irqsave(&tree->lock, flags);

    /* Set watermarks.  Currently receive watermarks fire the moment
     * one packet is in the queue (this should change in the future
     * using the signal bit to coalesce interrutps).  The inject
     * watermarks are set to half the queue length.  They get
     * activated when the inject fifos overrun and we need to back-log
     * packets.  The moment at least half the queue is empty the IRQ
     * fires and new packets can be injected.
     */
    mtdcrx( tree->dcrbase + _BGP_DCR_TR_GLOB_VCFG0,
	    _TR_GLOB_VCFG_RWM(0) | _TR_GLOB_VCFG_IWM(4));
    mtdcrx( tree->dcrbase + _BGP_DCR_TR_GLOB_VCFG1,
	    _TR_GLOB_VCFG_RWM(0) | _TR_GLOB_VCFG_IWM(4));

    // enable interrupts
    mtdcrx( tree->dcrbase + _BGP_DCR_TR_INJ_PIXEN, TREE_IRQMASK_INJ );
    mtdcrx( tree->dcrbase + _BGP_DCR_TR_REC_PRXEN, TREE_IRQMASK_REC );

    // clear exception flags
    mfdcrx( tree->dcrbase + _BGP_DCR_TR_INJ_PIXF );
    mfdcrx( tree->dcrbase + _BGP_DCR_TR_REC_PRXF );

    spin_unlock_irqrestore(&tree->lock, flags);
}


static void bgtree_disable_interrupts(struct bg_tree *tree)
{
    spin_lock(&tree->lock);

    mtdcrx( tree->dcrbase + _BGP_DCR_TR_INJ_PIXEN, 0 );
    mtdcrx( tree->dcrbase + _BGP_DCR_TR_REC_PRXEN, 0 );

    spin_unlock(&tree->lock);
}

void bgtree_enable_inj_wm_interrupt(struct bg_tree *tree, int channel)
{
    unsigned long flags;

    spin_lock_irqsave(&tree->lock, flags);

    if ((tree->inj_wm_mask & (_TR_INJ_PIX_WM0 >> channel)) == 0) {
	tree->inj_wm_mask |= _TR_INJ_PIX_WM0 >> channel;
	mtdcrx(tree->dcrbase + _BGP_DCR_TR_INJ_PIXEN,
	       TREE_IRQMASK_INJ | tree->inj_wm_mask);

	mod_timer(&tree->chn[channel].inj_timer, jiffies + HZ/4);
    }

    spin_unlock_irqrestore(&tree->lock, flags);
}

/* prereq: tree lock held and irqs disabled */
void bgtree_disable_inj_wm_interrupt(struct bg_tree *tree, int channel)
{
    if ((tree->inj_wm_mask & (_TR_INJ_PIX_WM0 >> channel)) != 0) {
	if (tree->chn[channel].inj_wedged) {
	    printk(KERN_INFO "bgtree: inject fifo recovered.\n");
	    tree->chn[channel].inj_wedged = 0;
	}
	del_timer(&tree->chn[channel].inj_timer);

	tree->inj_wm_mask &= ~(_TR_INJ_PIX_WM0 >> channel);
	mtdcrx(tree->dcrbase + _BGP_DCR_TR_INJ_PIXEN,
	       TREE_IRQMASK_INJ | tree->inj_wm_mask);
    }
}

/* prereq: tree lock held and irqs disabled */
static void bgtree_continue_inj_wm_interrupt(struct bg_tree *tree, int channel)
{
    if (tree->chn[channel].inj_wedged) {
	printk(KERN_INFO "bgtree: inject fifo recovered.\n");
	tree->chn[channel].inj_wedged = 0;
    }
    mod_timer(&tree->chn[channel].inj_timer, jiffies + HZ/4);
}

static void inj_timeout(unsigned long chnArg)
{
    struct bgtree_channel *chn = (struct bgtree_channel *) chnArg;
    printk(KERN_INFO "bgtree: inject fifo timed out!\n");
    chn->inj_wedged = 1;
}

/**********************************************************************
 * Receive and transmit
 **********************************************************************/

/*
 * calculate a hash bucket for a particular link header
 */
static unsigned hash_fragment_list(struct bglink_hdr_tree *lnk)
{
    return lnk->src_key % FRAGMENT_LISTS;
}


/*
 * this walker searches for a specific skb but also drops packets that
 * have been in the queue for too long
 */
static struct sk_buff* find_frag_skb(struct bg_tree *tree,
				     struct bglink_hdr_tree *lnk)
{
    struct sk_buff_head *list_ = &tree->skb_list[hash_fragment_list(lnk)];
    struct sk_buff *list = ((struct sk_buff *)list_);

    while (list->next != (struct sk_buff *)list_)
    {
	struct sk_buff * currskb = list->next;
	struct frag_skb *fskb = (struct frag_skb*)(currskb->data);

	if ( (fskb->lnk.src_key == lnk->src_key) &&
	     (fskb->lnk.dst_key == lnk->dst_key) &&
	     (fskb->lnk.conn_id == lnk->conn_id) )
	{
	    return currskb;
	}
	else if (time_after(jiffies, fskb->jiffies + FRAGMENT_TIMEOUT))
	{
	    TRACE("bgtree: fragment skb timed out (%lx, %lx), "
		  "%p/%d/%d/%d %x/%x, len=%d\n",
		   jiffies, fskb->jiffies, currskb, fskb->lnk.conn_id,
		   fskb->lnk.this_pkt, fskb->lnk.total_pkt,
		   fskb->lnk.src_key, lnk->dst_key, skb_queue_len(list_));
	    __skb_unlink(currskb, list_);
	    dev_kfree_skb(currskb);
	    tree->fragment_timeout++;
	    continue;
	}

	list = list->next;
    }

    return NULL;
}

static void enqueue_frag_skb(struct bg_tree* tree,
			     struct bglink_hdr_tree *lnkhdr,
			     struct sk_buff *skb)
{
    struct sk_buff_head *list = &tree->skb_list[hash_fragment_list(lnkhdr)];

    if (skb_queue_len(list) > 1000) {
	struct sk_buff *tmpskb = __skb_dequeue_tail(list);
#if 0
	struct frag_skb *fskb = (struct frag_skb*)(tmpskb->data);
	printk("bgtree: fragment skb cut list (%lx, %lx), %d/%d/%d %x/%x\n",
	       jiffies, fskb->jiffies, fskb->lnk.conn_id,
	       fskb->lnk.this_pkt, fskb->lnk.total_pkt,
	       fskb->lnk.src_key, lnkhdr->dst_key);
#endif
	dev_kfree_skb(tmpskb);
	tree->fragment_timeout++;
    }

    __skb_queue_head(list, skb);
}

static void dequeue_frag_skb(struct bg_tree* tree,
			     struct bglink_hdr_tree *lnkhdr,
			     struct sk_buff *skb)
{
    __skb_unlink(skb, &tree->skb_list[hash_fragment_list(lnkhdr)]);
}

static inline void bgtree_receive_240(void *payload, u32 mioaddr)
{
    asm volatile(
	"lfpdx   1,0,%1;"	// F1=Q1  load
	"lfpdx   2,0,%1;"	// F2=Q2  load
	"lfpdx   3,0,%1;"	// F3=Q3  load
	"lfpdx   4,0,%1;"	// F4=Q4  load
	"lfpdx   5,0,%1;"	// F5=Q5  load
	"stfpdx  1,0,%0;"	// Q1  store to *payload
	"stfpdux 2,%0,%2;"	// Q2  store to *(payload+16)
	"lfpdx   6,0,%1;"	// F6=Q6  load
	"lfpdx   7,0,%1;"	// F7=Q7  load
	"lfpdx   8,0,%1;"	// F8=Q8  load
	"stfpdux 3,%0,%2;"	// Q3  store
	"stfpdux 4,%0,%2;"	// Q4  store
	"stfpdux 5,%0,%2;"	// Q5  store
	"lfpdx   9,0,%1;"	// F9=Q9  load
	"lfpdx   0,0,%1;"	// F0=Q10 load
	"lfpdx   1,0,%1;"	// F1=Q11 load
	"stfpdux 6,%0,%2;"	// Q6  store
	"stfpdux 7,%0,%2;"	// Q7  store
	"stfpdux 8,%0,%2;"	// Q8  store
	"lfpdx   2,0,%1;"	// F2=Q12 load
	"lfpdx   3,0,%1;"	// F3=Q13 load
	"lfpdx   4,0,%1;"	// F4=Q14 load
	"stfpdux 9,%0,%2;"	// Q9  store
	"stfpdux 0,%0,%2;"	// Q10 store
	"stfpdux 1,%0,%2;"	// Q11 store
	"lfpdx   5,0,%1;"	// F5=Q15 load
	"stfpdux 2,%0,%2;"	// Q12 store
	"stfpdux 3,%0,%2;"	// Q13 store
	"stfpdux 4,%0,%2;"	// Q14 store
	"stfpdux 5,%0,%2;"	// Q15 store
	: "=b"   (payload)	// 0
	: "b"    (mioaddr +
		  _BGP_TRx_DR),	// 1 (data reception fifo address)
	  "b"    (16),		// 2 (bytes per quad)
	  "0"    (payload)	// "payload" is input and output
	: "fr0", "fr1", "fr2",
	  "fr3", "fr4", "fr5",
	  "fr6", "fr7", "fr8",
	  "fr9", "memory"
    );
}

static void bgtree_receive(struct bg_tree *tree, unsigned channel)
{
    struct bglink_hdr_tree lnkhdr __attribute__((aligned(16)));
    unsigned drop, i;
    void *payloadptr;
    struct bgtree_channel *chn = &tree->chn[channel];
    union bgtree_status status;
    union bgtree_header dst;
    struct sk_buff *skb;
    struct frag_skb *fskb;

    status.raw = in_be32_nosync((unsigned*)(chn->mioaddr + _BGP_TRx_Sx));

    while ( status.x.rcv_hdr )
    {
	//TRACE("bgnet channel %d status: %x (rcv count=%d, rcv hdr=%d)\n",
	//      channel, status.raw, status.x.rcv_cnt, status.x.rcv_hdr);

	drop = 0;

	// packet destination
	dst.raw = in_be32_nosync((unsigned*)(chn->mioaddr + _BGP_TRx_HR));

	// read the first 16 bytes from the payload
	in_be128(&lnkhdr, (void*)chn->mioaddr + _BGP_TRx_DR);

	TRACE("bgnet: stat=%x, dst=%x, hdr: conn=%x, "
	      "this_pkt=%x, tot_pkt=%x, dst=%x, src=%x]\n",
	      status.raw, dst.raw, lnkhdr.conn_id,
	      lnkhdr.this_pkt, lnkhdr.total_pkt,
	      lnkhdr.dst_key, lnkhdr.src_key);

	// early check if packet is valid
	if (lnkhdr.total_pkt < 1 || lnkhdr.this_pkt >= lnkhdr.total_pkt) {
	    drop = 1;
	    TRACE("bgnet: invalid packet tot: %d, this: %d\n",
		   lnkhdr.total_pkt, lnkhdr.this_pkt);
	    goto rcv_pkt;
	}

	// are there other fragment already?
	skb = find_frag_skb(tree, &lnkhdr);

	if (!skb) {
	    int size = lnkhdr.total_pkt * TREE_FRAGPAYLOAD +
			    sizeof(struct frag_skb);
	    skb = alloc_skb(size + TREE_SKB_ALIGN, GFP_ATOMIC);
	    //skb = dev_alloc_skb(size + TREE_SKB_ALIGN);
	    if (skb) {
		// align such that the data is starting at 16 byte boundary
		skb_reserve(skb, TREE_SKB_ALIGN -
				    (offsetof(struct frag_skb, payload) %
					TREE_SKB_ALIGN));
		fskb = (struct frag_skb*)skb_put(skb, size);
		memcpy(&fskb->lnk, &lnkhdr, sizeof(struct bglink_hdr_tree));
		fskb->lnk.this_pkt = 0; // reset counter
		enqueue_frag_skb(tree, &lnkhdr, skb);
	    }
	    else {
		TRACE("bgtree: Couldn't allocate a sk_buff of size %d.\n",
		      size);
		drop = 1;
	    }
	}
	else {
	    fskb = (struct frag_skb*)skb->data;

	    // perform some sanity checks
	    if ( (fskb->lnk.total_pkt != lnkhdr.total_pkt) ||
		 (fskb->lnk.this_pkt > lnkhdr.total_pkt) )
	    {
		drop = 1;
		TRACE("bgtree: packet link header differs from previous "
		      "packet(s); drop (%d, %d, %d), j=%lx/%lx\n",
		      fskb->lnk.total_pkt, lnkhdr.total_pkt,
		      fskb->lnk.this_pkt, fskb->jiffies, jiffies);
	    }
	}

    rcv_pkt:
	if (drop)
	{
	    // drain the packet buffer
	    TRACE("bgtree drained and dropped: stat=%x, dst=%x, hdr: "
		  "conn=%x, this_pkt=%x, tot_pkt=%x, dst=%x, src=%x]\n",
		   status.raw, dst.raw, lnkhdr.conn_id, lnkhdr.this_pkt,
		   lnkhdr.total_pkt, lnkhdr.dst_key, lnkhdr.src_key);
	    for (i = 16; i < TREE_PAYLOAD; i += 16)
		asm("lfpdx 0,0,%0" ::"r"((void*)chn->mioaddr + _BGP_TRx_DR));
	    chn->dropped++;
	}
	else {
	    // receive packet into skb
	    payloadptr = fskb->payload + (lnkhdr.this_pkt * TREE_FRAGPAYLOAD);
	    //printk("tree: receiving payload to %p\n", payloadptr);


	    // read remaining payload

	    if ((((u32) payloadptr) & 0xF) == 0) {
		bgtree_receive_240(payloadptr, chn->mioaddr);
	    } else {
		ins_be128(payloadptr, (void*)chn->mioaddr + _BGP_TRx_DR,
			  (TREE_PAYLOAD - sizeof(struct bglink_hdr_tree)) / 16);
	    }

	    fskb->jiffies = jiffies;
	    fskb->lnk.this_pkt++;
	    chn->received++;

	    if (fskb->lnk.this_pkt == fskb->lnk.total_pkt)
	    {
		struct bglink_proto *proto;

		// received all fragments of a packet
		dequeue_frag_skb(tree, &lnkhdr, skb);

		// remove frag header
		__skb_pull(skb, sizeof(struct frag_skb));

		// deliver to upper protocol layers
		rcu_read_lock();
		proto = bglink_find_proto(lnkhdr.lnk_proto);
		if (proto && ((lnkhdr.src_key != tree->nodeid) ||
			      proto->receive_from_self))
		{
		    proto->tree_rcv(skb, &lnkhdr);
		    chn->delivered++;
		} else {
		    //dump_skb(skb);
		    dev_kfree_skb(skb);
		    chn->dropped++;
		}
		rcu_read_unlock();
	    }
	}

	// pick up packets that arrived meanwhile...
	if ( (status.x.rcv_hdr--) == 0)
	    status.raw = in_be32_nosync((unsigned*)(chn->mioaddr+_BGP_TRx_Sx));
    }
}


static irqreturn_t bgtree_receive_interrupt(int irq, void *dev)
{
    struct bg_tree *tree = (struct bg_tree*)dev;
    unsigned pend_irq;
    unsigned chn;

    TRACE("bgnet: interrupt %d\n", irq);

    spin_lock(&tree->irq_lock);

    if (!(mfmsr() & MSR_FP))
	enable_kernel_fp();

    // read & clear interrupts
    pend_irq = mfdcrx(tree->dcrbase +_BGP_DCR_TR_REC_PRXF);

    if (pend_irq & ~(_TR_REC_PRX_WM0 | _TR_REC_PRX_WM1)) {
	BUG();
	printk("bgtree: pending IRQ bits=%x\n", pend_irq);
	// XXX: handle error situation
    }

    for (chn = 0; chn < BGP_MAX_CHANNEL; chn++)
	if (pend_irq & tree->chn[chn].irq_rcv_pending_mask)
	    bgtree_receive(tree, chn);

    spin_unlock(&tree->irq_lock);

    return IRQ_HANDLED;
}

void bgtree_link_hdr_init(struct bglink_hdr_tree *lnkhdr)
{
    // Set total_pkt to 0 as a flag to cause __bgtree_xmit to do the real
    // initialization (which requires that tree->lock be held).
    lnkhdr->total_pkt = 0;
}

unsigned int bgtree_mcast_redirect(struct bg_tree *tree, const u8 *dest_addr)
{
    unsigned int group, target;
    int i;

    if ((dest_addr[0] == 0x01) &&
	(dest_addr[1] == 0x00) &&
	(dest_addr[2] == 0x5E) &&
	((dest_addr[3]&0x80) == 0))
    {
	group = (dest_addr[3] << 16) | (dest_addr[4] << 8) | dest_addr[5];
	for (i = 0; i < tree->mcast_table_cur; i++) {
	    if (tree->mcast_table[i].group == group) {
		/* NOTE: only one target supported at this time. */
		target = tree->mcast_table[i].target[0];
		if (target != -1) {
		    return target;
		}
	    }
	}
    }

    return -1;
}

static inline void bgtree_inject_packet(u32 *dest, void *lnkhdr, void *payload,
					u32 mioaddr)
{
    asm volatile(
	"lfpdx   0,0,%4;"	// F0=Q0 load *lnkhdr(%4)
	"stw     %3,%c5(%1);"	// store dest to HI offset from mioaddr
	"lfpdx   1,0,%0;"	// F1=Q1 load from *payload
	"lfpdux  2,%0,%6;"	// F2=Q2 load from *(payload+16)
	"lfpdux  3,%0,%6;"	// F3=Q3 load
	"stfpdx  0,0,%2;"	// Q0 store to *(mioaddr + TRx_DI)
	"lfpdux  4,%0,%6;"	// F4=Q4 load
	"lfpdux  5,%0,%6;"	// F5=Q5 load
	"lfpdux  6,%0,%6;"	// F6=Q6 load
	"stfpdx  1,0,%2;"	// Q1 store
	"stfpdx  2,0,%2;"	// Q2 store
	"stfpdx  3,0,%2;"	// Q3 store
	"lfpdux  7,%0,%6;"	// F7=Q7 load
	"lfpdux  8,%0,%6;"	// F8=Q8 load
	"lfpdux  9,%0,%6;"	// F9=Q9 load
	"stfpdx  4,0,%2;"	// Q4 store
	"stfpdx  5,0,%2;"	// Q5 store
	"stfpdx  6,0,%2;"	// Q6 store
	"lfpdux  0,%0,%6;"	// F0=Q10 load
	"lfpdux  1,%0,%6;"	// F1=Q11 load
	"lfpdux  2,%0,%6;"	// F2=Q12 load
	"stfpdx  7,0,%2;"	// Q7 store
	"stfpdx  8,0,%2;"	// Q8 store
	"stfpdx  9,0,%2;"	// Q9 store
	"lfpdux  3,%0,%6;"	// F3=Q13 load
	"lfpdux  4,%0,%6;"	// F4=Q14 load
	"lfpdux  5,%0,%6;"	// F5=Q15 load
	"stfpdx  0,0,%2;"	// Q10 store
	"stfpdx  1,0,%2;"	// Q11 store
	"stfpdx  2,0,%2;"	// Q12 store
	"stfpdx  3,0,%2;"	// Q13 store
	"stfpdx  4,0,%2;"	// Q14 store
	"stfpdx  5,0,%2;"	// Q15 store
	: "=b"  (payload)	// 0
	: "b"   (mioaddr),	// 1
	  "b"   (mioaddr +
		 _BGP_TRx_DI),	// 2 (data injection fifo address)
	  "b"   (*dest),	// 3
	  "b"   (lnkhdr),	// 4
	  "i"   (_BGP_TRx_HI),	// 5
	  "b"   (16),		// 6 (bytes per quad)
	  "0"   (payload)	// "payload" is input and output
	: "fr0", "fr1", "fr2",
	  "fr3", "fr4", "fr5",
	  "fr6", "fr7", "fr8",
	  "fr9", "memory"
    );
}

/* can be called from within inject IRQ */
int __bgtree_xmit(struct bg_tree *tree, int chnidx, union bgtree_header dest,
		  struct bglink_hdr_tree *lnkhdr, void *data, int len)
{
    struct bgtree_channel *chn;
    union bgtree_status status;
    int n;

    BUG_ON(chnidx >= BGP_MAX_CHANNEL);

    chn = &tree->chn[chnidx];

    TRACE("bgnet: transmit: dest=%08x, len=%u, type=%d, chn=%d\n",
	  dest.raw, len, lnkhdr->lnk_proto, chnidx);

    if (!(mfmsr() & MSR_FP))
	enable_kernel_fp();

    if (lnkhdr->total_pkt == 0) {
	// prepare link header
	lnkhdr->src_key = tree->nodeid;
	lnkhdr->conn_id = tree->curr_conn++;
	lnkhdr->total_pkt = ((len - 1) / TREE_FRAGPAYLOAD) + 1;
	lnkhdr->this_pkt = 0;
    } else {
	// adjust data and len for fragments that have already been sent
	data += (lnkhdr->this_pkt * TREE_FRAGPAYLOAD);
	len -= (lnkhdr->this_pkt * TREE_FRAGPAYLOAD);
    }

    // iterate over fragments and send them out...
    for (; lnkhdr->this_pkt < lnkhdr->total_pkt; lnkhdr->this_pkt++)
    {
	/* pacing: if send fifo is full put packets into backlog and
	 * enable low-watermark IRQ on inject fifo */
	status.raw = in_be32_nosync((unsigned*)(chn->mioaddr + _BGP_TRx_Sx));

	if (status.x.inj_hdr >= TREE_FIFO_SIZE) {
	    chn->inject_fail++;
	    return -EBUSY;
	}

	if ((((((u32)data) | ((u32)lnkhdr)) & 0xF) == 0) &&
					(len >= TREE_FRAGPAYLOAD)) {
	    // special case -- data and lnkhdr are aligned and full fragment

	    bgtree_inject_packet(&dest.raw, lnkhdr, data, chn->mioaddr);

	    data += TREE_FRAGPAYLOAD;
	    len -= TREE_FRAGPAYLOAD;

	} else {
	    // general case

	    if (len < TREE_FRAGPAYLOAD) {
		chn->partial_injections++;
	    }

	    if ((u32) lnkhdr & 0xF) {
		chn->unaligned_hdr_injections++;
	    }

	    if ((u32) data & 0xF) {
		chn->unaligned_data_injections++;
	    }

	    // write destination header
	    out_be32((unsigned*)(chn->mioaddr + _BGP_TRx_HI), dest.raw);

	    // send link header (16 bytes)
	    out_be128((void*)(chn->mioaddr + _BGP_TRx_DI), lnkhdr);

	    TRACE("bgtree: ptr=%p, len=%d\n", data, len);

	    // send payload
	    n = min(len, (int) TREE_FRAGPAYLOAD);
	    if (n >= 16) {
		// copy whole quads
		outs_be128((void*)(chn->mioaddr + _BGP_TRx_DI), data, n / 16);
	    }
	    if ((n % 16) > 0) {
		// copy partial quad, padded with zeros
		u32 tmp[4] __attribute__((aligned(16))) = {0,0,0,0};
		memcpy(tmp, data + ((n / 16) * 16), n % 16);
		out_be128((void*)(chn->mioaddr + _BGP_TRx_DI), tmp);
	    }
	    if ((TREE_FRAGPAYLOAD - n) >= 16) {
		// pad whole quads with zeros
		outs_zero128((void*)(chn->mioaddr + _BGP_TRx_DI),
			     (TREE_FRAGPAYLOAD - n) / 16);
	    }

	    data += n;
	    len -= n;
	}

	chn->injected++;
    }

#if 0
    // when everything is written there should be no pending qwords
    status.raw = in_be32_nosync((unsigned*)(chn->mioaddr + _BGP_TRx_Sx));
    BUG_ON(status.x.inj_qwords != 0);
#endif

    return 0;
}


int bgtree_xmit(struct bg_tree *tree, int chnidx, union bgtree_header dest,
		struct bglink_hdr_tree *lnkhdr, void *data, int len)
{
    unsigned long flags;
    int rc;

    spin_lock_irqsave(&tree->lock, flags);

    rc = __bgtree_xmit(tree, chnidx, dest, lnkhdr, data, len);

    spin_unlock_irqrestore(&tree->lock, flags);

    return rc;
}


static irqreturn_t bgtree_inject_interrupt(int irq, void *dev)
{
    struct bg_tree *tree = (struct bg_tree*)dev;
    unsigned pend_irq;
    unsigned chn;
    int rc = 0;

    spin_lock(&tree->lock);
    spin_lock(&tree->irq_lock);

    // read & clear interrupts
    pend_irq = mfdcrx(tree->dcrbase +_BGP_DCR_TR_INJ_PIXF);

    if (pend_irq & ~(_TR_INJ_PIX_WM0 | _TR_INJ_PIX_WM1 | _TR_INJ_PIX_ENABLE)) {
	BUG();
	printk("bgtree: unhandled inject IRQ bits=%x\n", pend_irq);
	// XXX: handle error situation
    }

    /* chn in outer loop since typically only one inject IRQ is pending */
    for (chn = 0; chn < BGP_MAX_CHANNEL; chn++) {
	if (pend_irq & tree->inj_wm_mask & tree->chn[chn].irq_inj_pending_mask) {
	    struct bglink_proto *proto;
	    list_for_each_entry_rcu(proto, &linkproto_list, list)
		if (proto->tree_flush)
		    if ((rc = proto->tree_flush(chn)) != 0)
			break;

	    /* If we successfully flushed all packets, clear the high
	     * watermark.  We are still holding tree->lock and thus no new
	     * packets can be enqueued. */
	    if (rc == 0) {
		bgtree_disable_inj_wm_interrupt(tree, chn);
	    } else {
		bgtree_continue_inj_wm_interrupt(tree, chn);
	    }
	}
    }

    spin_unlock(&tree->irq_lock);
    spin_unlock(&tree->lock);

    return IRQ_HANDLED;
}

/**********************************************************************
 * Initialization and shut-down
 **********************************************************************/

static inline void bgtree_reset_channel(struct bgtree_channel *chn)
{
    mtdcrx(chn->dcrbase + _BGP_DCR_TR_RCTRL, _TR_RCTRL_RST);
    mtdcrx(chn->dcrbase + _BGP_DCR_TR_SCTRL, _TR_RCTRL_RST);
}

static int bgtree_init_channel(struct bgtree_channel *chn, struct bg_tree *tree)
{
    if (!request_mem_region(chn->paddr.start, _BGP_TREE_SIZE, TREE_DEV_NAME))
	return -1;

    chn->mioaddr = (unsigned long)ioremap(chn->paddr.start, _BGP_TREE_SIZE);
    if (!chn->mioaddr)
	goto err_remap;

    return 0;

 err_remap:
    printk("error mapping tree\n");
    release_mem_region(chn->mioaddr, _BGP_TREE_SIZE);

    return -1;
}

static int bgtree_uninit_channel(struct bgtree_channel *chn,
				 struct bg_tree *tree)
{
    if (chn->mioaddr)
    {
	iounmap((void*)chn->mioaddr);
	chn->mioaddr = 0;

	// unconditionally...
	release_mem_region(chn->paddr.start, _BGP_TREE_SIZE);
    }
    return 0;
}

static int bgtree_init (struct bg_tree *tree)
{
    int cidx, rc, idx;
    const u32 *prop;
    unsigned int sz;

    spin_lock_init(&tree->lock);
    spin_lock_init(&tree->irq_lock);

    for (idx = 0; idx < FRAGMENT_LISTS; idx++)
	skb_queue_head_init(&tree->skb_list[idx]);

    if ((prop = of_get_property(tree->node, "dcr-reg", &sz)) == NULL)
	return -ENODEV;
    if (prop[1] != TREE_DCR_SIZE) {
	printk(KERN_ERR "tree: DCR register size differs from "
	       "device specification\n");
	return -ENODEV;
    }
    tree->dcrbase = prop[0];

    for (idx = 0; idx < BGP_MAX_CHANNEL; idx++) {
	if (of_address_to_resource(tree->node, idx, &tree->chn[idx].paddr))
	    return -ENODEV;
	tree->chn[idx].dcrbase = tree->dcrbase + TREE_CHANNEL_DCROFF(idx);
	tree->chn[idx].irq_rcv_pending_mask = TREE_IRQ_RCV_PENDING_MASK(idx);
	tree->chn[idx].irq_inj_pending_mask = TREE_IRQ_INJ_PENDING_MASK(idx);

	init_timer(&tree->chn[idx].inj_timer);
	tree->chn[idx].inj_timer.function = inj_timeout;
	tree->chn[idx].inj_timer.data = (unsigned long) &tree->chn[idx];
	tree->chn[idx].inj_timer.expires = 0;
	tree->chn[idx].inj_wedged = 0;
    }

    // abuse IO port structure for DCRs
    if (!request_region(tree->dcrbase, TREE_DCR_SIZE, TREE_DEV_NAME))
	return -1;

    // disable device IRQs before we attach them
    bgtree_disable_interrupts(tree);

    tree->nodeid = mfdcrx(tree->dcrbase + _BGP_DCR_TR_GLOB_NADDR);

    for (cidx = 0; cidx < BGP_MAX_CHANNEL; cidx++) {
	if (bgtree_init_channel(&tree->chn[cidx], tree) != 0)
	    goto err_channel;
    }

    // allocate IRQs last; otherwise, if an IRQ is still pending, we
    // get kernel segfaults
    for (idx = 0; bgtree_irqs[idx].irq != 0; idx++)
    {
	rc = request_irq(bgtree_irqs[idx].irq, bgtree_irqs[idx].handler,
			 IRQF_DISABLED, bgtree_irqs[idx].name, tree);
	if (rc)
	    goto err_irq_alloc;

	/* swing the IRQ handler from level to fasteoi to avoid double IRQs */
	set_irq_handler(bgtree_irqs[idx].irq, handle_fasteoi_irq);
    }

    tree->mcast_table = NULL;
    tree->mcast_table_cur = 0;
    tree->mcast_table_size = 0;

    return 0;

 err_irq_alloc:
    for (idx = 0; bgtree_irqs[idx].irq != 0; idx++)
	free_irq(bgtree_irqs[idx].irq, tree);

 err_channel:
    for (cidx = 0; cidx < BGP_MAX_CHANNEL; cidx++)
	bgtree_uninit_channel(&tree->chn[cidx], tree);

    release_region(tree->dcrbase, TREE_DCR_SIZE);

    return -1;
}

/*
 * The driver is initialized early to drain the tree.  The matching is
 * to integrate with Linux' OF infrastructure
 */

static int __devinit bgtree_probe(struct of_device *ofdev,
				  const struct of_device_id *match)
{
    if (__tree && !__tree->ofdev) {
	dev_set_drvdata(&ofdev->dev, __tree);
	__tree->ofdev = ofdev;
	bgtree_proc_register(__tree);
    }
    return 0;
}

static int __devinit bgtree_remove(struct of_device *ofdev)
{
    if (__tree->ofdev == ofdev) {
	bgtree_proc_unregister(__tree);
	__tree->ofdev = NULL;
    }
    dev_set_drvdata(&ofdev->dev, NULL);
    return 0;
}

static struct of_device_id bgtree_match[] = {
    {
	.compatible = "ibm,bgp-tree",
    },
    {},
};

static struct of_platform_driver bgtree_driver = {
    .name = "bgtree",
    .match_table = bgtree_match,
    .probe = bgtree_probe,
    .remove = bgtree_remove,
};

static int __init bgtree_early_probe(void)
{
    struct device_node *np;
    struct bg_tree *tree;
    int idx;
    int rc;

    np = of_find_compatible_node(NULL, NULL, "ibm,bgp-tree");
    if (!np)
	return -ENODEV;

    for (idx = 0; bgtree_irqs[idx].name != NULL; idx++)
	bgtree_irqs[idx].irq = irq_of_parse_and_map(np, idx);

    tree = kmalloc(sizeof(struct bg_tree), GFP_KERNEL);
    if (!tree) {
	rc = -ENOMEM;
	goto err_tree_alloc;
    }

    memset(tree, 0, sizeof(struct bg_tree));
    tree->node = np;

    rc = bgtree_init(tree);
    if (rc)
	goto err_tree_init;

    mb();

    // make tree globally visible
    __tree = tree;

    printk(KERN_NOTICE "bgtree: %s node %06x\n", np->full_name, tree->nodeid);

    /* immidiately enable interrupts so that the driver starts pulling
     * packets off the tree... */
    bgtree_enable_interrupts(tree);

    return 0;

 err_tree_init:
    kfree(tree);
 err_tree_alloc:
    /* XXX: unmap IRQs */
    return rc;
}

static int __init bgtree_module_init (void)
{
    printk(KERN_INFO DRV_DESC ", version " DRV_VERSION "\n");
    of_register_platform_driver(&bgtree_driver);
    bgtree_early_probe();
    return 0;
}

arch_initcall(bgtree_module_init);

/**********************************************************************
 *                           sysctl interface
 **********************************************************************/

#if defined(CONFIG_SYSCTL) && defined(CONFIG_PROC_FS)

#define TGREAD(r, d) \
        rc = sprintf(page + len, "%.30s (%03x): %08x\n", d, \
		      tree->dcrbase + r, mfdcrx(tree->dcrbase + r)); \
	if (rc < 0 || rc + len > PAGE_SIZE - 128) { rc = -ENOMEM; goto err; } \
	len += rc;


static int proc_do_status (ctl_table *ctl, int write, struct file * filp,
                          void __user *buffer, size_t *lenp, loff_t *ppos)
{
    unsigned long flags;
    struct bg_tree *tree = bgtree_get_dev();
    char *page;
    int len = 0, rc;

    if (write)
	return -ENOSYS;
    if (!tree)
	return -ENOSYS;

    page = (char*)get_zeroed_page(GFP_KERNEL);
    if (!page)
	return -ENOMEM;

    spin_lock_irqsave(&tree->lock, flags);

    TGREAD(_BGP_DCR_TR_GLOB_FPTR, "Fifo Pointer");
    TGREAD(_BGP_DCR_TR_GLOB_NADDR, "Node Address");
    TGREAD(_BGP_DCR_TR_GLOB_VCFG0, "VC0 Configuration");
    TGREAD(_BGP_DCR_TR_GLOB_VCFG1, "VC1 Configuration");
    TGREAD(_BGP_DCR_TR_REC_PRXEN, "Receive Exception Enable");
    TGREAD(_BGP_DCR_TR_REC_PRDA, "Receive Diagnostic Address");
    TGREAD(_BGP_DCR_TR_REC_PRDD, "Receive Diagnostic Data");
    TGREAD(_BGP_DCR_TR_INJ_PIXEN, "Injection Exception Enable");
    TGREAD(_BGP_DCR_TR_INJ_PIDA, "Injection Diagnostic Address");
    TGREAD(_BGP_DCR_TR_INJ_PIDD, "Injection Diagnostic Data");
    TGREAD(_BGP_DCR_TR_INJ_CSPY0, "VC0 payload checksum");
    TGREAD(_BGP_DCR_TR_INJ_CSHD0, "VC0 header checksum");
    TGREAD(_BGP_DCR_TR_INJ_CSPY1, "VC1 payload checksum");
    TGREAD(_BGP_DCR_TR_INJ_CSHD1, "VC1 header checksum");

    TGREAD(_BGP_DCR_TR_CLASS_RDR0, "Route Desc 0, 1");
    TGREAD(_BGP_DCR_TR_CLASS_RDR1, "Route Desc 2, 3");
    TGREAD(_BGP_DCR_TR_CLASS_RDR2, "Route Desc 4, 5");
    TGREAD(_BGP_DCR_TR_CLASS_RDR3, "Route Desc 6, 7");
    TGREAD(_BGP_DCR_TR_CLASS_RDR4, "Route Desc 8, 9");
    TGREAD(_BGP_DCR_TR_CLASS_RDR5, "Route Desc 10, 11");
    TGREAD(_BGP_DCR_TR_CLASS_RDR6, "Route Desc 12, 13");
    TGREAD(_BGP_DCR_TR_CLASS_RDR7, "Route Desc 14, 15");
    TGREAD(_BGP_DCR_TR_CLASS_ISRA, "Idle pattern low");
    TGREAD(_BGP_DCR_TR_CLASS_ISRB, "Idle pattern high");

    TGREAD(_BGP_DCR_TR_DMA_DMAA, "SRAM diagnostic addr");
    TGREAD(_BGP_DCR_TR_DMA_DMAD, "SRAM diagnostic data");
    TGREAD(_BGP_DCR_TR_DMA_DMADI, "SRAM diagnostic data inc");
    TGREAD(_BGP_DCR_TR_DMA_DMAH, "SRAM diagnostic header");

    TGREAD(_BGP_DCR_TR_ERR_R0_CRC, "CH0: Receiver link CRC errors");
    TGREAD(_BGP_DCR_TR_ERR_R0_CE, "CH0: Receiver SRAM errors corrected");
    TGREAD(_BGP_DCR_TR_ERR_S0_RETRY, "CH0: Sender link retransmissions");
    TGREAD(_BGP_DCR_TR_ERR_S0_CE, "CH0: Sender SRAM errors corrected");

    TGREAD(_BGP_DCR_TR_ERR_R1_CRC, "CH1: Receiver link CRC errors");
    TGREAD(_BGP_DCR_TR_ERR_R1_CE, "CH1: Receiver SRAM errors corrected");
    TGREAD(_BGP_DCR_TR_ERR_S1_RETRY, "CH1: Sender link retransmissions");
    TGREAD(_BGP_DCR_TR_ERR_S1_CE, "CH1: Sender SRAM errors corrected");

    TGREAD(_BGP_DCR_TR_ERR_R2_CRC, "CH2: Receiver link CRC errors");
    TGREAD(_BGP_DCR_TR_ERR_R2_CE, "CH2: Receiver SRAM errors corrected");
    TGREAD(_BGP_DCR_TR_ERR_S2_RETRY, "CH2: Sender link retransmissions");
    TGREAD(_BGP_DCR_TR_ERR_S2_CE, "CH2: Sender SRAM errors corrected");

    TGREAD(_BGP_DCR_TR_ARB_RCFG, "ARB: General router config");
    TGREAD(_BGP_DCR_TR_ARB_RSTAT, "ARB: General router status");
    TGREAD(_BGP_DCR_TR_ARB_HD00, "ARB: Next hdr, CH0, VC0");
    TGREAD(_BGP_DCR_TR_ARB_HD01, "ARB: Next hdr, CH0, VC1");
    TGREAD(_BGP_DCR_TR_ARB_HD10, "ARB: Next hdr, CH1, VC0");
    TGREAD(_BGP_DCR_TR_ARB_HD11, "ARB: Next hdr, CH1, VC1");
    TGREAD(_BGP_DCR_TR_ARB_HD20, "ARB: Next hdr, CH2, VC0");
    TGREAD(_BGP_DCR_TR_ARB_HD21, "ARB: Next hdr, CH2, VC1");

    len += sprintf(page + len, "CH0: status=%x\n",
		   in_be32((unsigned*)(tree->chn[0].mioaddr + _BGP_TRx_Sx)));

    len += sprintf(page + len, "CH1: status=%x\n",
		   in_be32((unsigned*)(tree->chn[1].mioaddr + _BGP_TRx_Sx)));

    len += sprintf(page + len,
		   "injected: %d, %d\n"
		   "injected fail: %d, %d\n"
		   "partial injections: %d, %d\n"
		   "unaligned hdr injections: %d, %d\n"
		   "unaligned data injections: %d, %d\n"
		   "received: %d, %d\n"
		   "dropped: %d, %d\n"
		   "delivered: %d, %d\n"
		   "fragment timeout: %d\n",
		   tree->chn[0].injected, tree->chn[1].injected,
		   tree->chn[0].inject_fail, tree->chn[1].inject_fail,
		   tree->chn[0].partial_injections,
			    tree->chn[1].partial_injections,
		   tree->chn[0].unaligned_hdr_injections,
			    tree->chn[1].unaligned_hdr_injections,
		   tree->chn[0].unaligned_data_injections,
			    tree->chn[1].unaligned_data_injections,
		   tree->chn[0].received, tree->chn[1].received,
		   tree->chn[0].dropped, tree->chn[1].dropped,
		   tree->chn[0].delivered, tree->chn[1].delivered,
		   tree->fragment_timeout);

    spin_unlock_irqrestore(&tree->lock, flags);

    len -= *ppos;
    if (len > *lenp)
	len = *lenp;
    if (copy_to_user(buffer, page + *ppos, len)) {
	rc = -EFAULT;
	goto out;
    }

    *lenp = len;
    *ppos += len;
    rc = 0;
    goto out;

 err:
    spin_unlock_irqrestore(&tree->lock, flags);
 out:
    free_page((unsigned long)page);
    return rc;
}

static int proc_do_route (ctl_table *ctl, int write, struct file * filp,
                          void __user *buffer, size_t *lenp, loff_t *ppos)
{
    unsigned val;
    unsigned mask = 0;
    int len, route = (int)ctl->extra1;
    struct bg_tree *tree = bgtree_get_dev();
    char *page;
    int rc = 0;
    int dcrn = tree->dcrbase + _BGP_DCR_TR_CLASS + (route / 2);
    int shift = 16 * (route % 2);

    BUG_ON(route < 0 || route >= 16);
    if (!tree)
	return -ENOSYS;

    page = (char*)get_zeroed_page(GFP_KERNEL);
    if (!page)
	return -ENOMEM;

    if (write) {
	if (*lenp >= PAGE_SIZE)
	    goto out;

	len = *lenp;
	if (copy_from_user(page, buffer, len)) {
	    rc = -EFAULT;
	    goto out;
	}

	// we assume a transaction--everything written at once
	while(len > 0) {
	    if (isspace(*page)) {
		page++; len--;
		continue;
	    }

	    if (len < 2)
		goto parse_err;

	    if (toupper(page[0]) == 'S')
		mask |= ((toupper(page[1]) == 'L') ? _TR_CLASS_RDR_LO_SRCL :
			 (page[1] >= '0' && page[1] <= '2') ?
			     _TR_CLASS_RDR_LO_SRC0 << (page[1] - '0') : 0);
	    else if (toupper(page[0]) == 'T')
		mask |= ((toupper(page[1]) == 'L') ? _TR_CLASS_RDR_LO_TGTL :
			 (page[1] >= '0' && page[1] <= '2') ?
			     _TR_CLASS_RDR_LO_TGT0 << (page[1] - '0') : 0);
	    else
		goto parse_err;

	    len -= 2;
	    page+= 2;
	}

	spin_lock(&tree->lock);
	val = (mfdcrx(dcrn) & ~(0xffff0000 >> shift)) | (mask >> shift);
	mtdcrx(dcrn, val);
	spin_unlock(&tree->lock);

	*ppos += *lenp;
    } else {
	len = 0;
	val = mfdcrx(dcrn) << shift;

#define STATUS(SRCTGT, LINK, IDX) \
	if (val & _TR_CLASS_RDR_LO_##SRCTGT##IDX) \
	    len += sprintf(page + len, #LINK #IDX " ");

	STATUS(SRC,S,0); STATUS(SRC,S,1); STATUS(SRC,S,2); STATUS(SRC,S,L);
	STATUS(TGT,T,0); STATUS(TGT,T,1); STATUS(TGT,T,2); STATUS(TGT,T,L);
	len += sprintf(page + len, "\n");
	len -= *ppos;

	if (len < 0)
	    len = 0;
	if (len > *lenp)
	    len = *lenp;
	if (len) {
	    if (copy_to_user(buffer, page + *ppos, len)) {
		rc = -EFAULT;
		goto out;
	    }
	}

	*lenp = len;
	*ppos += len;
    }

 out:

    if (page)
	free_page((unsigned long)page);

    return rc;

 parse_err:
    rc = -EINVAL;
    goto out;
}

static int proc_do_mcast_filter(ctl_table *ctl, int write, struct file *filp,
			        void __user *buffer, size_t *lenp, loff_t *ppos)
{
    struct bg_tree *tree = bgtree_get_dev();
    char *space;
    int len;
    struct mcast_table_entry *table;
    unsigned int group, target[MCAST_TARGET_MAX];
    unsigned int a, b, c, d, e, f;
    int n, idx, i, j;
    char *s;

    if (!tree) {
	return -ENOSYS;
    }

    if (write) {
	if ((*ppos) != 0) {
	    return -EINVAL;
	}
	len = *lenp;
	space = kmalloc(len+1, GFP_KERNEL);
	if (space == NULL) {
	    return -ENOMEM;
	}
	if (copy_from_user(space, buffer, len)) {
	    kfree(space);
	    return -EFAULT;
	}
	space[len] = '\0';  /* for sscanf */

	if ((len >= 5) && (strncmp(space, "clear", 5) == 0)) {
	    tree->mcast_table_cur = 0;
	    kfree(space);
	    return 0;
	}

	s = space;
	while (len > 0) {
	    n = sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
		       &a, &b, &c, &d, &e, &f);
	    if (n != 6) goto parse_err;
	    s += 17; len -= 17;

	    if ((a != 0x01) || (b != 0x00) || (c != 0x5E) || ((d&0x80) != 0))
		goto parse_err;
	    group = (d << 16) | (e << 8) | f;

	    for (i = 0; i < MCAST_TARGET_MAX; i++) {
		if ((len > 0) && (*s == ' ')) {
		    s++; len--;
		    n = sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x",
			       &a, &b, &c, &d, &e, &f);
		    if (n != 6) goto parse_err;
		    s += 17; len -= 17;

		    if ((a != 0x02) || (b != 0x00)) goto parse_err;
		    target[i] = (c << 24) | (d << 16) | (e << 8) | f;
		} else {
		    target[i] = -1;
		}
	    }

	    if ((len <= 0) || (*s != '\n')) goto parse_err;
	    s++; len--;

	    for (idx = 0; idx < tree->mcast_table_cur; idx++) {
		if (tree->mcast_table[idx].group == group) break;
	    }

	    if (target[0] == -1) {
		/* deleting entry */
		if (idx < tree->mcast_table_cur) {
		    for (i = idx + 1; i < tree->mcast_table_cur; i++) {
			tree->mcast_table[i-1] = tree->mcast_table[i];
		    }
		    tree->mcast_table_cur--;
		}
	    } else {
		/* modifying or adding entry */
		if (idx == tree->mcast_table_cur) {
		    if (tree->mcast_table_cur == tree->mcast_table_size) {
			tree->mcast_table_size += 4;
			table = (struct mcast_table_entry *)
				kmalloc(tree->mcast_table_size *
					    sizeof(struct mcast_table_entry),
					GFP_KERNEL);
			if (table == NULL) {
			    kfree(space);
			    return -ENOMEM;
			}
			for (i = 0; i < tree->mcast_table_cur; i++) {
			    table[i] = tree->mcast_table[i];
			}
			if (tree->mcast_table != NULL) {
			    kfree(tree->mcast_table);
			}
			tree->mcast_table = table;
		    }
		    tree->mcast_table[idx].group = group;
		    tree->mcast_table_cur++;
		}
		for (i = 0; i < MCAST_TARGET_MAX; i++) {
		    tree->mcast_table[idx].target[i] = target[i];
		}
	    }
	}

	kfree(space);
    } else {
	space = kmalloc(1 + (tree->mcast_table_cur *
				(18 * (MCAST_TARGET_MAX + 1))),
			GFP_KERNEL);
	if (space == NULL) {
	    return -ENOMEM;
	}

	len = 0;
	for (i = 0; i < tree->mcast_table_cur; i++) {
	    snprintf(space + len, 18, "01:00:5E:%02X:%02X:%02X",
		     (tree->mcast_table[i].group >> 16) & 0x7f,
		     (tree->mcast_table[i].group >> 8) & 0xff,
		     (tree->mcast_table[i].group >> 0) & 0xff);
	    len += 17;

	    for (j = 0; j < MCAST_TARGET_MAX; j++) {
		if (tree->mcast_table[i].target[j] != -1) {
		    snprintf(space + len, 19, " 02:00:%02X:%02X:%02X:%02X",
			     (tree->mcast_table[i].target[j] >> 24) & 0xff,
			     (tree->mcast_table[i].target[j] >> 16) & 0xff,
			     (tree->mcast_table[i].target[j] >> 8) & 0xff,
			     (tree->mcast_table[i].target[j] >> 0) & 0xff);
		    len += 18;
		}
	    }

	    snprintf(space + len, 2, "\n");
	    len += 1;
	}

	len -= *ppos;
	if (len < 0) {
	    len = 0;
	}
	if (len > *lenp) {
	    len = *lenp;
	}
	if (len > 0) {
	    if (copy_to_user(buffer, space + *ppos, len)) {
		return -EFAULT;
	    }
	}

	kfree(space);

	*lenp = len;
	*ppos += len;
    }

    return 0;

parse_err:
    kfree(space);
    return -EINVAL;
}

#define SYSCTL_ROUTE(x)				\
    {						\
	.ctl_name	= CTL_UNNUMBERED,	\
	.procname	= "route" #x,		\
	.mode		= 0644,			\
	.proc_handler	= proc_do_route,	\
	.extra1		= (void*)x,		\
    }

static struct ctl_table tree_table[] = {
    SYSCTL_ROUTE(0),
    SYSCTL_ROUTE(1),
    SYSCTL_ROUTE(2),
    SYSCTL_ROUTE(3),
    SYSCTL_ROUTE(4),
    SYSCTL_ROUTE(5),
    SYSCTL_ROUTE(6),
    SYSCTL_ROUTE(7),
    SYSCTL_ROUTE(8),
    SYSCTL_ROUTE(9),
    SYSCTL_ROUTE(10),
    SYSCTL_ROUTE(11),
    SYSCTL_ROUTE(12),
    SYSCTL_ROUTE(13),
    SYSCTL_ROUTE(14),
    SYSCTL_ROUTE(15),
    {
	.ctl_name = CTL_UNNUMBERED,
	.procname = "status",
	.mode = 0444,
	.proc_handler = proc_do_status,
    },
    {
	.ctl_name = CTL_UNNUMBERED,
	.procname = "mcast_filter",
	.mode = 0644,
	.proc_handler = proc_do_mcast_filter,
    },
    { .ctl_name = 0 }
};

static struct ctl_table tree_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "bgtree",
	.mode		= 0555,
	.child		= tree_table,
    },
    { .ctl_name = 0 }
};

static struct ctl_table dev_root[] = {
    {
	.ctl_name	= CTL_DEV,
	.procname	= "dev",
	.maxlen		= 0,
	.mode		= 0555,
	.child		= tree_root,
    },
    { .ctl_name = 0 }
};

static int bgtree_proc_register(struct bg_tree *tree)
{
    tree->sysctl_header = register_sysctl_table(dev_root);
    return 0;
}

static int bgtree_proc_unregister(struct bg_tree *tree)
{
    if (tree->sysctl_header) {
	unregister_sysctl_table(tree->sysctl_header);
	tree->sysctl_header = NULL;
    }

    return 0;
}

#endif /* PROC && SYSCTL */

/*
 * Deactivates tree routes and drains tree fifos.
 * Note: this is independent of the tree driver!
 */

extern void mailbox_puts(const unsigned char *s, int count);

void bluegene_tree_deactivate(void)
{
    int rt, c, i;
    u32 val;
    struct bg_tree *tree = bgtree_get_dev();
    struct bgtree_channel *chn;
    union bgtree_status status;
    char buf[64];

    for (rt = 0; rt < 8; rt++) {
	val = mfdcrx(0xc00 + _BGP_DCR_TR_CLASS + rt);
	val &= ~(0x3 << 16 | 0x3);
	mtdcrx(0xc00 + _BGP_DCR_TR_CLASS + rt, val);
    }

    mailbox_puts("bluegene_tree_deactivate\n", 25);

    for (c = 0; c < BGP_MAX_CHANNEL; c++) {
	chn = &tree->chn[c];
	status.raw = in_be32_nosync((unsigned*) (chn->mioaddr + _BGP_TRx_Sx));
	snprintf(buf, sizeof(buf),
		 "draining %d packet(s) from tree channel %d.\n",
		 status.x.rcv_hdr, c);
	mailbox_puts(buf, strlen(buf));
	while (status.x.rcv_hdr > 0) {
	    val = in_be32_nosync((unsigned*) (chn->mioaddr + _BGP_TRx_HR));
	    for (i = 0; i < TREE_PAYLOAD; i += 16) {
		asm("lfpdx 0,0,%0" :: "b" ((void*) chn->mioaddr + _BGP_TRx_DR));
	    }
	    status.x.rcv_hdr--;
	}
    }
}
