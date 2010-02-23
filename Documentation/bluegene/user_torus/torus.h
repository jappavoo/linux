#ifndef __BGP_TORUS__
#define __BGP_TORUS__

#define TORUSFILE "/dev/bgtorus"

#define TORUS_IOCTL 'T'
#define TORUS_ALLOC_TX_COUNTER _IO(TORUS_IOCTL, 1)
#define TORUS_ALLOC_RX_COUNTER _IO(TORUS_IOCTL, 2)
#define TORUS_ALLOC_TX_FIFO _IO(TORUS_IOCTL, 3)
#define TORUS_ALLOC_RX_FIFO _IO(TORUS_IOCTL, 4)
#define TORUS_FREE_TX_COUNTER _IO(TORUS_IOCTL, 5)
#define TORUS_FREE_RX_COUNTER _IO(TORUS_IOCTL, 6)
#define TORUS_FREE_TX_FIFO _IO(TORUS_IOCTL, 7)
#define TORUS_FREE_RX_FIFO _IO(TORUS_IOCTL, 8)
#define TORUS_REGISTER_TX_MEM _IO(TORUS_IOCTL, 9)
#define TORUS_REGISTER_RX_MEM _IO(TORUS_IOCTL, 10)
#define TORUS_DMA_RANGECHECK _IO(TORUS_IOCTL, 11)
#define TORUS_CONFIG_TX_FIFO _IO(TORUS_IOCTL, 12)

#define BGSM_SUPPORT
#ifdef BGSM_SUPPORT
#define TORUS_BGSM_ALLOC_TX_COUNTER _IO(TORUS_IOCTL, 13)
#define TORUS_BGSM_ALLOC_RX_COUNTER _IO(TORUS_IOCTL, 14)
#define TORUS_BGSM_ALLOC_TX_FIFO _IO(TORUS_IOCTL, 15)
#define TORUS_BGSM_ALLOC_RX_FIFO _IO(TORUS_IOCTL, 16)
#endif /* BGSM_SUPPORT */

struct torus_register_mem_param {
    unsigned long start;
    unsigned long size;
};

struct torus_config_rx_fifo_param {
    unsigned int fifo;
    unsigned int map;
    unsigned int priority;
    unsigned int local_dma;
};

#define BGP_TORUS_GROUPS	4
#define BGP_TORUS_INJ_FIFOS	32
#define BGP_TORUS_RCV_FIFOS	9
#define BGP_TORUS_COUNTERS	64

typedef unsigned int u32;
typedef unsigned char u8;

struct torus_fifo {
    u32 start;
    u32 end;
    volatile u32 head;
    volatile u32 tail;
};

struct torus_counter {
    volatile u32 counter;
    volatile u32 increment;
    u32 base;
    u32 limit;
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

	struct torus_counter counter[BGP_TORUS_COUNTERS];// 400 - 7ff
    } __attribute__((packed)) inj;

    struct {
	struct torus_fifo fifo[BGP_TORUS_RCV_FIFOS];	// 800 - 88f
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

	struct torus_counter counter[BGP_TORUS_COUNTERS];// c00 - fff
    } __attribute__((packed)) rcv;
};

union torus_fifo_hw_header {
    struct {
	u32 csum_skip: 7;// number of shorts to skip in chksum
	u32 sk: 1;// 0= use csum_skip, 1 skip pkt
	u32 dirhint: 6;// x-,x+,y-,y+,z-,z+
	u32 deposit: 1;// multicast deposit
	u32 pid0: 1;// destination fifo group MSb
	u32 size: 3;// size: (size + 1) * 32bytes
	u32 pid1: 1;// destination fifo group LSb
	u32 dma: 1;// 1=DMA mode, 0=Fifo mode
	u32 dyn_routing: 1;// 1=dynamic routing, 0=deterministic routing
	u32 virt_channel: 2;// channel (0=Dynamic CH0, 1=Dynamic CH1, 2=Bubble, 3=Prio)
	u32 dest_x: 8;
	u32 dest_y: 8;
	u32 dest_z: 8;
	u32 reserved: 16;
    };
    u8 raw8[8];
    u32 raw32[2];
} __attribute__((packed));

union torus_dma_hw_header {
    struct {
	u32: 30;
	u32 prefetch: 1;
	u32 local_copy: 1;
	u32: 24;
	u32 counter: 8;
	u32 base;
	u32 length;
    };
    u32 raw32[4];
} __attribute__((packed));

union torus_dma_sw_header {
    struct {
	u32 offset;
	u8 counter_id;
	u8 bytes;
	u8 unused: 6;
	u8 pacing: 1;
	u8 remote_get: 1;
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

union torus_rcv_desc {
    u32 raw32[256 / sizeof(u32)];
    u8 raw8[256];
    struct {
	union torus_fifo_hw_header fifo;
	u32 counter;
	union torus_dma_sw_header dma_sw;
	u32 data[];
    };
} __attribute__((packed));

#endif /* __BGP_TORUS__ */
