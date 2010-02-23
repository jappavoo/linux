/*********************************************************************
 *
 * Copyright (C) 2007-2008, Volkmar Uhlig, IBM Corporation
 *
 * Description:   Torus definitions
 *
 * All rights reserved
 *
 ********************************************************************/
#ifndef __DRIVERS__BLUEGENE__TORUS_H__
#define __DRIVERS__BLUEGENE__TORUS_H__

#include "link.h"
#include <linux/ioctl.h>

#define TORUS_MAX_MTU	(39 * 240)

#define BGP_TORUS_MAX_IRQS	96

#define BGP_TORUS_PIDS		4
#define BGP_TORUS_GROUPS	4
#define BGP_TORUS_DMA_SIZE	(sizeof(struct torus_dma) * BGP_TORUS_GROUPS)

#define BGP_TORUS_INJ_FIFOS	32
#define BGP_TORUS_RCV_FIFOS	8
#define BGP_TORUS_COUNTERS	64
#define BGP_TORUS_DMA_REGIONS	8

#define BGP_TORUS_TX_ENTRIES	256
#define BGP_TORUS_RX_ENTRIES	512

#define USER_MODE_TORUS_ACCESS
#ifdef USER_MODE_TORUS_ACCESS

/* IOCTLs for UL DMA */
#define TORUS_IOCTL	'T'
#define TORUS_ALLOC_TX_COUNTER		_IO(TORUS_IOCTL, 1)
#define TORUS_ALLOC_RX_COUNTER		_IO(TORUS_IOCTL, 2)
#define TORUS_ALLOC_TX_FIFO		_IO(TORUS_IOCTL, 3)
#define TORUS_ALLOC_RX_FIFO		_IO(TORUS_IOCTL, 4)
#define TORUS_FREE_TX_COUNTER		_IO(TORUS_IOCTL, 5)
#define TORUS_FREE_RX_COUNTER		_IO(TORUS_IOCTL, 6)
#define TORUS_FREE_TX_FIFO		_IO(TORUS_IOCTL, 7)
#define TORUS_FREE_RX_FIFO		_IO(TORUS_IOCTL, 8)
#define TORUS_REGISTER_TX_MEM		_IO(TORUS_IOCTL, 9)
#define TORUS_REGISTER_RX_MEM		_IO(TORUS_IOCTL, 10)
#define TORUS_DMA_RANGECHECK		_IO(TORUS_IOCTL, 11)
#define TORUS_CONFIG_TX_FIFO		_IO(TORUS_IOCTL, 12)

#define BGSM_SUPPORT
#ifdef BGSM_SUPPORT
#define TORUS_BGSM_ALLOC_TX_COUNTER	_IO(TORUS_IOCTL, 13)
#define TORUS_BGSM_ALLOC_RX_COUNTER	_IO(TORUS_IOCTL, 14)
#define TORUS_BGSM_ALLOC_TX_FIFO	_IO(TORUS_IOCTL, 15)
#define TORUS_BGSM_ALLOC_RX_FIFO	_IO(TORUS_IOCTL, 16)
#endif /* BGSM_SUPPORT */

struct torus_file;  /* forward declaration */

#endif /* USER_MODE_TORUS_ACCESS */

#define BGP_KERNEL_GROUP (BGP_TORUS_GROUPS-1)
#define BGP_KERNEL_PID (BGP_TORUS_PIDS-1)
#define BGP_KERNEL_INJ_FIFO (BGP_TORUS_INJ_FIFOS-1)
#define BGP_KERNEL_RCV_FIFO (BGP_TORUS_RCV_FIFOS-1)
#define BGP_KERNEL_INJ_COUNTER (BGP_TORUS_COUNTERS-1)
#define BGP_KERNEL_RCV_REGION (BGP_TORUS_DMA_REGIONS-1)
#define BGP_KERNEL_FIFOMAP 0x01
#define BGP_DEFAULT_FIFOMAP 0x80


struct torus_fifo {
    u32 start;
    u32 end;
    volatile u32 head;
    volatile u32 tail;
};

struct torus_dma {
    struct {
	struct torus_fifo fifo[BGP_TORUS_INJ_FIFOS];	// 0 - 1ff
	u32 empty;			// 200
	u32 __unused0;			// 204
	u32 avail;			// 208
	u32 __unused1;			// 20c
	u32 threshold;			// 210
	u32 __unused2;			// 214
	u32 clear_threshold;		// 218
	u32 __unused3;			// 21c
	u32 dma_active;			// 220
	u32 dma_activate;		// 224
	u32 dma_deactivate;		// 228
	u8 __unused4[0x100-0x2c];	// 22c - 2ff

	u32 counter_enabled[2];		// 300
	u32 counter_enable[2];		// 308
	u32 counter_disable[2];		// 310
	u32 __unused5[2];		// 318
	u32 counter_hit_zero[2];	// 320
	u32 counter_clear_hit_zero[2];	// 328
	u32 counter_group_status;	// 330
	u8 __unused6[0x400-0x334];	// 334 - 3ff

	struct {
	    u32 counter;
	    u32 increment;
	    u32 base;
	    u32 __unused;
	} counter[BGP_TORUS_COUNTERS];// 400 - 7ff
    } __attribute__((packed)) inj;

    struct {
	struct torus_fifo fifo[BGP_TORUS_RCV_FIFOS];	// 800 - 87f
	struct torus_fifo hdrfifo;	// 880 - 88f
	u8 __unused0[0x900-0x890];	// 890 - 900

	u32 glob_ints[16];		// 900 - 93f
	u8 __unused1[0xa00-0x940];	// 940 - 9ff

	u32 empty[2];			// a00
	u32 available[2];		// a08
	u32 threshold[2];		// a10
	u32 clear_threshold[2];		// a18
	u8 __unused2[0xb00 - 0xa20];	// a20 - aff

	u32 counter_enabled[2];		// b00
	u32 counter_enable[2];		// b08
	u32 counter_disable[2];		// b10
	u32 __unused3[2];		// b18
	u32 counter_hit_zero[2];	// b20
	u32 counter_clear_hit_zero[2];	// b28
	u32 counter_group_status;	// b30
	u8 __unused4[0xc00 - 0xb34];	// b34 - bff

	struct {
	    u32 counter;
	    u32 increment;
	    u32 base;
	    u32 limit;
	} counter[BGP_TORUS_COUNTERS];// c00 - fff
    } __attribute__((packed)) rcv;
};

enum {
    torus_dir_xplus = 0x20,
    torus_dir_xminus = 0x10,
    torus_dir_yplus = 0x08,
    torus_dir_yminus = 0x04,
    torus_dir_zplus = 0x02,
    torus_dir_zminus = 0x01
};

union torus_fifo_hw_header {
    struct {
	u32 csum_skip		: 7;	// number of shorts to skip in chksum
	u32 sk			: 1;	// 0= use csum_skip, 1 skip pkt
	u32 dirhint		: 6;	// x-,x+,y-,y+,z-,z+
	u32 deposit		: 1;	// multicast deposit
	u32 pid0		: 1;	// destination fifo group MSb
	u32 size		: 3;	// size: (size + 1) * 32bytes
	u32 pid1		: 1;	// destination fifo group LSb
	u32 dma			: 1;	// 1=DMA mode, 0=Fifo mode
	u32 dyn_routing		: 1;	// 1=dynamic routing,
					// 0=deterministic routing
	u32 virt_channel	: 2;	// channel (0=Dynamic CH0,
					// 1=Dynamic CH1, 2=Bubble, 3=Prio)
	u32 dest_x		: 8;
	u32 dest_y		: 8;
	u32 dest_z		: 8;
	u32 reserved		: 16;
    };
    u8 raw8[8];
    u32 raw32[2];
} __attribute__((packed));

union torus_dma_hw_header {
    struct {
	u32			: 30;
	u32 prefetch		: 1;
	u32 local_copy		: 1;
	u32			: 24;
	u32 counter		: 8;
	u32 base;
	u32 length;
    };
    u32 raw32[2];
} __attribute__((packed));

union torus_dma_sw_header {
    struct {
	u32 offset;
	u8 counter_id;
	u8 bytes;
	u8 unused		: 6;
	u8 pacing		: 1;
	u8 remote_get		: 1;
    };
    u32 raw32[2];
} __attribute__((packed));

union torus_inj_desc {
    u32 raw32[8];
    struct {
	union torus_dma_hw_header dma_hw;
	union torus_fifo_hw_header fifo;
	union torus_dma_sw_header dma_sw;
    };
} __attribute__((packed));

struct torus_tx_ring {
    union torus_inj_desc *desc;
    struct sk_buff **skbs;
    u32 start;
    unsigned int tail_idx, pending_idx;
    unsigned int counter;
    phys_addr_t paddr;
    spinlock_t lock;
};

union torus_source_id {
    u32 raw;
    atomic_t raw_atomic;
    struct {
	u32 conn_id	: 8;
	u32 src_key	: 24;
    };
};

#define TORUS_SOURCE_ID_NULL (~0ul)  // anything that can't be a legitimate id

union torus_rcv_desc {
    u32 raw32[256 / sizeof(u32)];
    u8 raw8[256];
    struct {
	union torus_fifo_hw_header fifo;
	u32 counter;
	union torus_source_id src_id;
	u32 data[];
    };
} __attribute__((packed));

struct torus_skb_cb {
    union torus_source_id src_id;
    u32 received_len;
    u32 total_len;
};

struct torus_rx_ring {
    union torus_rcv_desc *desc;
    struct sk_buff_head skb_list;
    u32 start;
    unsigned int head_idx;
    phys_addr_t paddr;
    spinlock_t lock;

    // bookkeeping for packet currently being reconstructed
    union torus_source_id src_id;
    u32 received_len;
    u32 total_len;
    struct sk_buff *skb;

    // statistics
    u32 dropped;
    u32 delivered;
};

struct bg_torus {
    u8 coordinates[3];
    u8 dimension[3];
    union torus_source_id source_id;

    spinlock_t lock;
    struct torus_dma *dma;

    struct torus_tx_ring tx[BGP_TORUS_INJ_FIFOS * BGP_TORUS_GROUPS];
    struct torus_rx_ring rx[BGP_TORUS_RCV_FIFOS * BGP_TORUS_GROUPS];

    /* mapping from counter to tx ring index */
    int inj_counter_to_txidx[BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS];

    /* counters used */
    DECLARE_BITMAP(inj_counter_map, BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(rcv_counter_map, BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS);

    /* fifos used */
    DECLARE_BITMAP(inj_fifo_map, BGP_TORUS_INJ_FIFOS * BGP_TORUS_GROUPS);
    DECLARE_BITMAP(rcv_fifo_map, BGP_TORUS_RCV_FIFOS * BGP_TORUS_GROUPS);

    /* dma regions used */
    DECLARE_BITMAP(rcv_dma_region_map, BGP_TORUS_DMA_REGIONS);

    /* rcv counters taken out of service because of use while disabled */
    DECLARE_BITMAP(rcv_counter_removed, BGP_TORUS_COUNTERS * BGP_TORUS_GROUPS);

    unsigned int dcr_base, dcr_size;
    struct resource pdma, pfifo0, pfifo1;
    int virq[BGP_TORUS_MAX_IRQS];

    struct of_device *ofdev;
    struct ctl_table_header *sysctl_header;

#ifdef USER_MODE_TORUS_ACCESS
    struct torus_file *rcv_counter_to_tfile[BGP_TORUS_COUNTERS *
						    BGP_TORUS_GROUPS];
#endif /* USER_MODE_TORUS_ACCESS */
};


extern inline void bgtorus_init_inj_desc(struct bg_torus *torus,
					 union torus_inj_desc *desc,
					 int len, u8 x, u8 y, u8 z)
{
    memset(desc, 0, sizeof(*desc));

    desc->fifo.sk = 1;		// skip checksum
    desc->fifo.size = 7;	// always full 240 bytes packets
    desc->fifo.pid0 = (BGP_KERNEL_PID >> 1) & 0x1;
    desc->fifo.pid1 = BGP_KERNEL_PID & 0x1;
    desc->fifo.dyn_routing = 1;
    desc->fifo.dest_x = x;
    desc->fifo.dest_y = y;
    desc->fifo.dest_z = z;

    desc->dma_hw.length = len;
    desc->dma_hw.counter =
	(BGP_KERNEL_GROUP * BGP_TORUS_COUNTERS) + BGP_KERNEL_INJ_COUNTER;

    /* atomic { desc->dma_sw.raw32[1] = ++torus->source_id.conn_id; } */
    desc->dma_sw.raw32[1] =
	atomic_add_return(1U << 24, &torus->source_id.raw_atomic);
}

int bgtorus_xmit(struct bg_torus *torus, union torus_inj_desc *desc,
		 struct sk_buff *skb);


#endif /* !__DRIVERS__BLUEGENE__TORUS_H__ */
