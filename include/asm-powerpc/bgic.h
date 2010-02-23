/*
 * include/asm-powerpc/bg_bic.h
 *
 * IBM Blue Gene Interrupt Controller external definitions and structure.
 *
 * Maintainer: Volkmar Uhlig <vuhlig@us.ibm.com>
 * Copyright 2007 IBM Corporation.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _ASM_POWERPC_BGIC_H
#define _ASM_POWERPC_BGIC_H

#ifdef __KERNEL__

extern void __init bgic_init(void);
extern unsigned int bgic_get_virq(void);

extern void smp_bgic_message_pass(int target, int msg);
extern int __init smp_bgic_probe(void);

#endif /* __KERNEL__ */
#endif /* _ASM_POWERPC_BGIC_H */
