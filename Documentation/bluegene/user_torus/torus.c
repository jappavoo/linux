#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "torus.h"

struct tx_channel {
    void *buf;
    int buf_size;
    struct torus_fifo *fifo;
    union torus_inj_desc *fifo_desc;
    u32 fifo_start;
    int fifo_size;
    int fifo_idx;
};

struct rx_channel {
    void *buf;
    int buf_size;
    struct torus_counter *ctr;
    u32 received;
};

void init_fifo(struct torus_fifo *fifo,
	       unsigned long long start, int fifo_size)
{
    unsigned long long end = start + (fifo_size*sizeof(union torus_inj_desc));
    fifo->start = start >> 4;
    fifo->head = start >> 4;
    fifo->tail = start >> 4;
    fifo->end = end >> 4;
}

int init_tx(struct tx_channel *tx, int fdt, struct torus_dma *dma,
	    int fifo_size, int buf_size, int x, int y, int z, int dst_ctr)
{
    int i;
    int fsize, bsize;
    void *mem;
    int pfn;
    struct torus_register_mem_param map_parm;
    int ctr, ctr_grp, ctr_idx;
    int fifo, fifo_grp, fifo_idx;

    const int PAGESIZE = sysconf(_SC_PAGESIZE);
    fsize = ((fifo_size * sizeof(union torus_inj_desc)) +
				    (PAGESIZE-1)) & ~(PAGESIZE-1);
    bsize = (buf_size + (PAGESIZE-1)) & ~(PAGESIZE-1);

    mem = mmap(0, fsize + bsize, PROT_WRITE|PROT_READ,
	       MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (mem == ((void *) -1)) {
	fprintf(stderr, "allocation of tx DMA region failed\n");
        return -1;
    }

    map_parm.start = (unsigned long) mem;
    map_parm.size = fsize + bsize;
    pfn = ioctl(fdt, TORUS_REGISTER_TX_MEM, &map_parm);
    if (pfn < 0) {
	fprintf(stderr, "registration of tx DMA region failed\n");
        return -1;
    }

    tx->fifo_desc = mem;
    tx->fifo_size = fifo_size;
    tx->fifo_idx = 0;

    tx->buf = mem + fsize;
    tx->buf_size = buf_size;

    ctr = ioctl(fdt, TORUS_ALLOC_TX_COUNTER);
    fifo = ioctl(fdt, TORUS_ALLOC_TX_FIFO);
    if ((ctr < 0) || (fifo < 0)) {
	fprintf(stderr, "tx counter or fifo allocation failed\n");
	return -1;
    }

    ctr_grp = ctr / BGP_TORUS_COUNTERS;
    ctr_idx = ctr % BGP_TORUS_COUNTERS;

    fifo_grp = fifo / BGP_TORUS_INJ_FIFOS;
    fifo_idx = fifo % BGP_TORUS_INJ_FIFOS;

    unsigned long long paddr = pfn * PAGESIZE;
    tx->fifo = &dma[fifo_grp].inj.fifo[fifo_idx];
    init_fifo(tx->fifo, paddr, fifo_size);
    tx->fifo_start = tx->fifo->start;
    dma[ctr_grp].inj.counter_enable[ctr_idx / 32] =
					(0x80000000 >> (ctr_idx % 32));
    dma[ctr_grp].inj.counter[ctr_idx].base = (paddr + fsize) >> 4;
    dma[ctr_grp].inj.counter[ctr_idx].counter = 0xffffffff;

    dma[fifo_grp].inj.dma_activate = 0x80000000 >> fifo_idx;

    for (i = 0; i < fifo_size; i++) {
	union torus_inj_desc *desc = &tx->fifo_desc[i];

	memset(desc, 0, sizeof(*desc));

	desc->fifo.sk = 1;	// skip checksum
	desc->fifo.size = 7;// always full 240 bytes packets
	desc->fifo.dyn_routing = 1;
	desc->fifo.dest_x = x;
	desc->fifo.dest_y = y;
	desc->fifo.dest_z = z;

	/* direct put, use mfifo 0 */
	desc->fifo.pid0 = 0;
	desc->fifo.pid1 = 0;

	desc->fifo.dma = 1;

	desc->dma_hw.counter = ctr;
	desc->dma_sw.counter_id = dst_ctr;
    }

    printf("allocated tx region: (mem=%p, pfn=%x, ctr=%d, fifo=%d)\n",
	   mem, pfn, ctr, fifo);

    return 0;
}

int init_rx(struct rx_channel *rx, int fdt, struct torus_dma *dma, int buf_size)
{
    int bsize;
    void *mem;
    int pfn;
    struct torus_register_mem_param map_parm;
    int ctr, ctr_grp, ctr_idx;

    const int PAGESIZE = sysconf(_SC_PAGESIZE);
    bsize = (buf_size + (PAGESIZE-1)) & ~(PAGESIZE-1);

    mem = mmap(0, bsize, PROT_WRITE|PROT_READ,
	       MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (mem == ((void *) -1)) {
	fprintf(stderr, "allocation of rx DMA region failed\n");
        return -1;
    }

    map_parm.start = (unsigned long) mem;
    map_parm.size = bsize;
    pfn = ioctl(fdt, TORUS_REGISTER_RX_MEM, &map_parm);
    if (pfn < 0) {
	fprintf(stderr, "registration of rx DMA region failed\n");
	return -1;
    }

    rx->buf = mem;
    rx->buf_size = buf_size;

    ctr = ioctl(fdt, TORUS_ALLOC_RX_COUNTER);

    if (ctr < 0) {
	fprintf(stderr, "rx counter allocation failed\n");
	return -1;
    }

    ctr_grp = ctr / BGP_TORUS_COUNTERS;
    ctr_idx = ctr % BGP_TORUS_COUNTERS;

    dma[ctr_grp].rcv.counter_enable[ctr_idx / 32] =
				(0x80000000 >> (ctr_idx % 32));
    rx->ctr = &dma[ctr_grp].rcv.counter[ctr_idx];

    unsigned long long paddr = pfn << 12;
    rx->ctr->base = paddr >> 4;
    rx->ctr->counter = 0xffffffff;

    rx->received = 0;

    printf("allocated rx region: (mem=%p, pfn=%x, ctr=%d)\n",
	   mem, pfn, ctr);

    return 0;
}

void torus_send(struct tx_channel *tx, int len)
{
    union torus_inj_desc *desc;
    desc = &tx->fifo_desc[tx->fifo_idx];

    desc->dma_hw.length = len;
    desc->dma_sw.offset = 0;
    desc->dma_sw.bytes = len;

    asm("mbar\n":::"memory");

    tx->fifo_idx++;
    if (tx->fifo_idx >= tx->fifo_size) tx->fifo_idx = 0;
    u32 new_tail = tx->fifo_start +
		    ((tx->fifo_idx * sizeof(union torus_inj_desc)) >> 4);
    while (tx->fifo->head == new_tail) { /* wait */ }
    tx->fifo->tail = new_tail;
}

void torus_receive(struct rx_channel *rx, int len)
{
    u32 newcnt = 0xfffffffful - (rx->received + len);

    while (rx->ctr->counter > newcnt) { /* wait */ }
    asm("msync\n":::"memory");

    rx->received += len;
}

void send(struct tx_channel *tx, int len, int verify, u32 data)
{
    if (verify) {
	int i;
	for (i = 0; i < len/4; i++) {
	    ((u32 *)(tx->buf))[i] = data;
	}
    }

    torus_send(tx, len);
}

void receive(struct rx_channel *rx, int len, int verify, u32 data)
{
    torus_receive(rx, len);

    if (verify) {
	u32 sum;
	int i;

	sum = 0 ;

	asm volatile ("dcbt 0,%0" : : "b" (rx->buf));
	for (i = 0; i < (len - 32)/4; i += 8) {
	    asm volatile ("dcbt 0,%0" : : "b" (rx->buf + (32 + i*4)));
	    sum |= ((u32 *)(rx->buf))[i+0] |
		   ((u32 *)(rx->buf))[i+1] |
		   ((u32 *)(rx->buf))[i+2] |
		   ((u32 *)(rx->buf))[i+3] |
		   ((u32 *)(rx->buf))[i+4] |
		   ((u32 *)(rx->buf))[i+5] |
		   ((u32 *)(rx->buf))[i+6] |
		   ((u32 *)(rx->buf))[i+7];
	}
	for (; i < len/4; i++) {
	    sum |= ((u32 *)(rx->buf))[i];
	}

	if (sum != data) {
	    fprintf(stderr, "bad data, sum 0x%x, expected 0x%x.\n", sum, data);
	}
    }
}

int main(int argc, char **argv)
{
    int x, y, z;
    int client = 0;
    int server = 0;
    int roundtrip = 0;
    int verify = 0;
    int num = 1;
    int fifo_size = 128;
    int buf_size = 240;
    int opt;
    int rc;
    int fdt;
    struct torus_dma *dma;
    struct tx_channel tx;
    struct rx_channel rx;
    int i;

    while((opt = getopt(argc, argv, "csrvn:f:b:")) != -1) {
	switch(opt) {
	case 'c':
	    client = 1;
	    break;
	case 's':
	    server = 1;
	    break;
	case 'r':
	    roundtrip = 1;
	    break;
	case 'v':
	    verify = 1;
	    break;
	case 'n':
	    num = atoi(optarg);
	    break;
	case 'f':
	    fifo_size = strtol(optarg, NULL, 0);
	    break;
	case 'b':
	    buf_size = strtol(optarg, NULL, 0);
	    break;
	default:
	    printf("Usage: %s -c|s [-rv] [x y z] \\\n"
		   "\t\t[-n <num>] [-f <fifo_size>] [-b <buf_size>]\n",
		   argv[0]);
	    return -1;
	}
    }

    if (client == server) {
	fprintf(stderr, "must specify either client or server\n");
	return -1;
    }

    if (client || roundtrip) {
	if (optind + 3 > argc) {
	    fprintf(stderr, "x y z arguments missing\n");
	    return -1;
	} else {
	    x = atoi(argv[optind + 0]);
	    y = atoi(argv[optind + 1]);
	    z = atoi(argv[optind + 2]);
	    printf("destination: [%d, %d, %d]\n", x, y, z);
	}
    }

    if ((buf_size < 4) || ((buf_size % 4) != 0)) {
	fprintf(stderr, "buf_size must be a positive multiple of 4\n");
	return -1;
    }

    printf("torus DMA test\n");

    fdt = open(TORUSFILE, O_RDWR);
    if (fdt < 0) {
	printf("failed opening torus descriptors\n");
	return -1;
    }

    dma = (struct torus_dma *) mmap(0, 0x3000, PROT_WRITE|PROT_READ,
				    MAP_SHARED, fdt, 0);
    if (dma == (void*) -1) {
	printf("failed mapping torus descriptor\n");
	return -1;
    }

    if (client || roundtrip) {
	int dst_ctr = 0;  // FIXME: must get this from receiver
	rc = init_tx(&tx, fdt, dma, fifo_size, buf_size, x, y, z, dst_ctr);
	if (rc != 0) {
	    printf("failed to initialize tx channel\n");
	    return -1;
	}
    }

    if (server || roundtrip) {
	rc = init_rx(&rx, fdt, dma, buf_size);
	if (rc != 0) {
	    printf("failed to initialize rx channel\n");
	    return -1;
	}
    }

    for (i = 0; i < num; i++) {
	if (server) {
	    receive(&rx, buf_size, verify, i);
	    if (roundtrip) {
		send(&tx, buf_size, verify, i);
	    }
	} else {
	    send(&tx, buf_size, verify, i);
	    if (roundtrip) {
		receive(&rx, buf_size, verify, i);
	    }
	}
    }

    return 0;
}
