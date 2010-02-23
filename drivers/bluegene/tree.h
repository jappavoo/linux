/*********************************************************************
 *
 * Copyright (C) 2007-2008, Volkmar Uhlig, IBM Corporation
 *
 * Description:   Header file for tree device
 *
 * All rights reserved
 *
 ********************************************************************/
#ifndef __DRIVERS__NET__BLUEGENE__TREE_H__
#define __DRIVERS__NET__BLUEGENE__TREE_H__

/**********************************************************************
 *				BlueGene L
 **********************************************************************/
#ifdef CONFIG_BGL

#define TREE_IRQ_GROUP		4
#define TREE_IRQ_BASE		0

union bgtree_header {
	unsigned int raw;
	struct {
		unsigned int pclass	: 4;
		unsigned int p2p	: 1;
		unsigned int irq	: 1;
		unsigned int __res	: 4;
		unsigned int sum_mode	: 2;
		unsigned int vector	: 20;
	} p2p;
	struct {
		unsigned int pclass	: 4;
		unsigned int p2p	: 1;
		unsigned int irq	: 1;
		unsigned int __res	: 4;
		unsigned int sum_mode	: 2;
		unsigned int tag	: 15;
		unsigned int op		: 5;
	} bcast;
} __attribute__((packed));

#error BGL tree definitions incomplete

/**********************************************************************
 *				BlueGene P
 **********************************************************************/
#elif CONFIG_BGP


#define _BGP_TREE_BASE		(0x610000000ULL)
#define _BGP_TREE_OFFSET	(0x001000000ULL)
#define _BGP_TREE_SIZE		(0x400)

#define _BGP_TORUS_BASE		(0x601140000ULL)
#define _BGP_TORUS_OFFSET	(0x000010000ULL)

#define BGP_MAX_CHANNEL 2
#define BGP_TREE_ADDR_BITS	24

#define TREE_CHANNEL_PADDR(c)	(_BGP_TREE_BASE + ((c)*_BGP_TREE_OFFSET))
#define TREE_CHANNEL_DCROFF(c)	(0x20 + ((c) * 8))
#define TREE_DCR_BASE		(0xc00)
#define TREE_DCR_SIZE		(0x80)

#define TREE_IRQMASK_INJ	(_TR_INJ_PIX_APAR0  | _TR_INJ_PIX_APAR1  |\
                                 _TR_INJ_PIX_ALIGN0 | _TR_INJ_PIX_ALIGN1 |\
                                 _TR_INJ_PIX_ADDR0  | _TR_INJ_PIX_ADDR1  |\
                                 _TR_INJ_PIX_DPAR0  | _TR_INJ_PIX_DPAR1  |\
                                 _TR_INJ_PIX_COLL   | _TR_INJ_PIX_UE     |\
                                 _TR_INJ_PIX_PFO0   | _TR_INJ_PIX_PFO1   |\
                                 _TR_INJ_PIX_HFO0   | _TR_INJ_PIX_HFO1)

#define TREE_IRQMASK_REC	(_TR_REC_PRX_APAR0  | _TR_REC_PRX_APAR1  |\
                                 _TR_REC_PRX_ALIGN0 | _TR_REC_PRX_ALIGN1 |\
                                 _TR_REC_PRX_ADDR0  | _TR_REC_PRX_ADDR1  |\
                                 _TR_REC_PRX_COLL   | _TR_REC_PRX_UE     |\
                                 _TR_REC_PRX_PFU0   | _TR_REC_PRX_PFU1   |\
                                 _TR_REC_PRX_HFU0   | _TR_REC_PRX_HFU1   |\
				 _TR_REC_PRX_WM0    | _TR_REC_PRX_WM1 )

#define TREE_IRQ_RCV_PENDING_MASK(idx) (1U << (1 - idx))
#define TREE_IRQ_INJ_PENDING_MASK(idx) (1U << (2 - idx))


#define TREE_IRQ_GROUP		5
#define TREE_IRQ_BASE		20
#define TREE_IRQ_NONCRIT_NUM	20
#define TREE_NONCRIT_BASE	0
#define TREE_FIFO_SIZE		8


union bgtree_header {
	unsigned int raw;
	struct {
		unsigned int pclass	: 4;
		unsigned int p2p	: 1;
		unsigned int irq	: 1;
		unsigned vector		: 24;
		unsigned int csum_mode	: 2;
	} p2p;
	struct {
		unsigned int pclass	: 4;
		unsigned int p2p	: 1;
		unsigned int irq	: 1;
		unsigned int op		: 3;
		unsigned int opsize	: 7;
		unsigned int tag	: 14;
		unsigned int csum_mode	: 2;
	} bcast;
} __attribute__((packed));

union bgtree_status {
	unsigned int raw;
	struct {
		unsigned int inj_pkt	: 4;
		unsigned int inj_qwords	: 4;
	        unsigned int __res0	: 4;
		unsigned int inj_hdr	: 4;
		unsigned int rcv_pkt	: 4;
		unsigned int rcv_qwords : 4;
		unsigned int __res1	: 3;
		unsigned int irq	: 1;
		unsigned int rcv_hdr	: 4;
	} x;
} __attribute__((packed));

/* some device defined */
#define _BGP_DCR_TR_RCTRL	(_BGP_DCR_TR_CH0_RCTRL - _BGP_DCR_TR_CH0)
#define _BGP_DCR_TR_SCTRL	(_BGP_DCR_TR_CH0_SCTRL - _BGP_DCR_TR_CH0)
#define _BGP_DCR_TR_RSTAT	(_BGP_DCR_TR_CH0_RSTAT - _BGP_DCR_TR_CH0)
#else
# error unknown BlueGene version
#endif

// hardware specification: 4 bytes address, 256 bytes payload
#define TREE_ALEN	4
#define TREE_PAYLOAD	256

/**********************************************************************
 * driver
 **********************************************************************/

#define TREE_DEV_NAME "bgtree"

struct bg_tree;

struct bg_tree *bgtree_get_dev(void);
unsigned int bgtree_get_nodeid(struct bg_tree* tree);
void bgtree_link_hdr_init(struct bglink_hdr_tree *lnkhdr);
unsigned int bgtree_mcast_redirect(struct bg_tree *tree, const u8 *dest_addr);
int bgtree_xmit(struct bg_tree *tree, int chnidx, union bgtree_header dest,
		struct bglink_hdr_tree *lnkhdr, void *data, int len);
int __bgtree_xmit(struct bg_tree *tree, int chnidx, union bgtree_header dest,
		  struct bglink_hdr_tree *lnkhdr, void *data, int len);

void bgtree_enable_inj_wm_interrupt(struct bg_tree *tree, int channel);
void bgtree_disable_inj_wm_interrupt(struct bg_tree *tree, int channel);

#endif /* !__DRIVERS__NET__BLUEGENE__TREE_H__ */
