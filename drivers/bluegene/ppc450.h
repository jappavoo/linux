#ifndef __DRIVERS__BLUEGENE__PPC450_H__
#define __DRIVERS__BLUEGENE__PPC450_H__

// include asm instruction macros
#include <asm/ppc450.h>

/**********************************************************************
 * DCR access wrapper
 **********************************************************************/

extern inline uint32_t mfdcrx(uint32_t dcrn)
{
    uint32_t value;
    asm volatile ("mfdcrx %0,%1": "=r" (value) : "r" (dcrn) : "memory");
    return value;
}

extern inline void mtdcrx(uint32_t dcrn, uint32_t value)
{
    asm volatile("mtdcrx %0,%1": :"r" (dcrn), "r" (value) : "memory");
}

// volatile 32bit read
extern inline uint32_t in_be32_nosync(uint32_t *vaddr)
{
   volatile uint32_t *va = (volatile uint32_t *) vaddr;
   //_bgp_mbar();
   return *va;
}


/**********************************************************************
 * Helper functions to access IO via double hummer
 **********************************************************************/

extern inline void fpu_memcpy_16(void *dst, void *src)
{
    asm volatile("lfpdx 0,0,%0\n"
		 "stfpdx 0,0,%1\n"
		 :
		 : "b"(src), "b"(dst)
		 : "fr0", "memory");
}

extern inline void out_be128(void *port, void *ptrval)
{
    u32 tmp[4] __attribute__((aligned(16)));

    if ((u32)ptrval & 0xf) {
	memcpy(tmp, ptrval, 16);
	ptrval = tmp;
    }

    fpu_memcpy_16(port, ptrval);
}

extern inline void outs_be128(void *port, void *src, unsigned num)
{
    u32 tmp[4] __attribute__((aligned(16)));

    // port must be 16 byte aligned
    BUG_ON((u32)port & 0xf);

    if (unlikely((u32)src & 0xf)) {
	// unaligned destination
	while(num--) {
	    memcpy(tmp, src, 16);
	    fpu_memcpy_16(port, tmp);
	    src += 16;
	}
    } else {
	while(num--) {
	    fpu_memcpy_16(port, src);
	    src += 16;
	}
    }
}

extern inline void outs_zero128(void *port, unsigned num)
{
    static u32 zero[4] __attribute__((aligned(16))) = {0, };
    BUG_ON((u32)port & 0xf);

    while (num--)
	out_be128(port, zero);
}

/*
 * in string operation similar to x86: reads block of data from port
 * into memory
 */
extern inline void ins_be128(void *dest, void *port, unsigned num)
{
    u32 tmp[4] __attribute__((aligned(16)));

    // port must be 16 byte aligned
    BUG_ON((u32)port & 0xf);

    if ((u32)dest & 0xf)
    {
	// unaligned destination
	while(num--) {
	    fpu_memcpy_16(tmp, port);
	    memcpy(dest, tmp, 16);
	    dest += 16;
	}
    }
    else
    {
	while(num--) {
	    fpu_memcpy_16(dest, port);
	    dest += 16;
	}
    }
}

extern inline void in_be128(void *dest, void *port)
{
    char tmp[16] __attribute__((aligned(16)));
    void *ptr = dest;

    if ((u32)dest & 0xf)
	ptr = tmp;

    fpu_memcpy_16(ptr, port);

    if ((u32)dest & 0xf)
	memcpy(dest, tmp, 16);
}

#endif /* !__DRIVERS__BLUEGENE__PPC450_H__ */
