/*
 * arch/ppc/platforms/4xx/bluegene.c
 *
 * Blue Gene node specific routines
 *
 * Volkmar Uhlig <vuhlig@us.ibm.com>
 * Todd Inglett <tinglett@us.ibm.com>
 *
 * Copyright 2005, 2007 International Business Machines, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kdev_t.h>
#include <linux/root_dev.h>
#include <linux/seq_file.h>
#include <linux/stddef.h>

#include <asm/bootinfo.h>
#include <asm/machdep.h>
#include <asm/ppcboot.h>
#include <asm/pgtable.h>
#include <asm/time.h>
#include <asm/ocp.h>
#include <asm/smp.h>
#include <asm/ibm4xx.h>
#include <asm/ppc4xx_pic.h>

#include <syslib/ibm440gx_common.h>
#include <platforms/4xx/bluegene.h>

/* residual board information from ibm44x_common.c */
extern bd_t __res;



/**********************************************************************
 *
 *   BG/P Interrupt controller
 *
 **********************************************************************/

struct bg_irqctrl_group {
	volatile unsigned int status;		// status (read and write) 0
	volatile unsigned int rd_clr_status;	// status (read and clear) 4
	volatile unsigned int status_clr;	// status (write and clear)8
	volatile unsigned int status_set;	// status (write and set) c

	// 4 bits per IRQ
	volatile unsigned int target_irq[4];	// target selector 10-20

	volatile unsigned int noncrit_mask[BGP_MAX_CORE];// mask 20-30
	volatile unsigned int crit_mask[BGP_MAX_CORE];	// mask 30-40
	volatile unsigned int mchk_mask[BGP_MAX_CORE];	// mask 40-50

	unsigned char __align[0x80 - 0x50];
} __attribute__((packed));

struct bg_irqctrl {
	struct bg_irqctrl_group groups[15];
	volatile unsigned int core_non_crit[BGP_MAX_CORE];
	volatile unsigned int core_crit[BGP_MAX_CORE];
	volatile unsigned int core_mchk[BGP_MAX_CORE];
} __attribute__((packed));

static struct bg_irqctrl *bg_irqctrl;
static spinlock_t irqlock;

#ifdef CONFIG_SMP
static int get_irq_server(unsigned int irq)
{
        cpumask_t cpumask = irq_desc[irq].affinity;
        cpumask_t tmp = CPU_MASK_NONE;
	int target_cpu = 0; /* default IRQ server is CPU 0 */

	if (!cpus_equal(cpumask, CPU_MASK_ALL))
	{
		cpus_and(tmp, cpu_online_map, cpumask);
		if (!cpus_empty(tmp))
			target_cpu = get_hard_smp_processor_id(first_cpu(tmp));
	}

	return target_cpu;
}
#else
static int get_irq_server(unsigned int irq) { return 0; }
#endif

// returns the pending IRQ
static int bg_irqctrl_get_irq(void)
{
	int group = -1, irq = -1;
	int cpu = smp_processor_id();
	unsigned long hierarchy, mask = 0;

	hierarchy = in_be32(&bg_irqctrl->core_non_crit[cpu]);

	// handle spurious interrupts...
	if (unlikely(hierarchy == 0))
		goto out;

	// ffs is in little endian format...
	group = BITS_PER_LONG - ffs(hierarchy);

	if (unlikely(group >= BGP_MAX_GROUP))
		goto out;

	mask = in_be32(&bg_irqctrl->groups[group].noncrit_mask[cpu]);

	if (unlikely(mask == 0))
		goto out;

	irq = GROUP_TO_IRQ(group) + (BITS_PER_LONG - ffs(mask));

 out:
	return irq;
}

static void bg_irqctrl_set_target(unsigned int irq, unsigned int target)
{
	unsigned offset = ((7 - (irq & 0x7)) * 4);

	volatile void *reg =
		&bg_irqctrl->groups[IRQ_TO_GROUP(irq)].target_irq[IRQ_OF_GROUP(irq) / 8];

	spin_lock(&irqlock);
	out_be32(reg, (in_be32(reg) & ~(0xf << offset)) | ((target & 0xf) << offset));
	spin_unlock(&irqlock);
}

#ifdef CONFIG_SMP
static void bg_irqctrl_raise_irq(unsigned int irq)
{
	unsigned mask = 1U << (31 - IRQ_OF_GROUP(irq));
	out_be32(&bg_irqctrl->groups[IRQ_TO_GROUP(irq)].status_set, mask);
}
#endif

static void bg_irqctrl_ack_irq(unsigned int irq)
{
	unsigned mask = 1U << (31 - IRQ_OF_GROUP(irq));
	out_be32(&bg_irqctrl->groups[IRQ_TO_GROUP(irq)].status_clr, mask);
}

/*
 * device IRQ handling, routed to one CPU
 */
static void bg_devirq_enable(unsigned int irq)
{
	bg_irqctrl_set_target(irq, BGP_IRQCTRL_TARGET_NONCRIT( get_irq_server(irq) ));
}

static void bg_devirq_disable(unsigned int irq)
{
	bg_irqctrl_set_target(irq, BGP_IRQCTRL_TARGET_DISABLED);
}

static void bg_devirq_ack(unsigned int irq)
{
	bg_irqctrl_ack_irq(irq);
}

static void bg_devirq_set_affinity(unsigned int irq, cpumask_t cpumask)
{
	/* affinity is re-evaluated when IRQs get re-enabled; nothing
	 * to be done here... */
}

static struct irq_chip bg_devirq_ctrl = {
	.name = "BG IRQ Controller",
	.enable = bg_devirq_enable,
	.disable = bg_devirq_disable,
	.mask = bg_devirq_disable,
	.unmask = bg_devirq_enable,
	.ack = bg_devirq_ack,
	.set_affinity = bg_devirq_set_affinity,
};


/**********************************************************************
 *
 *   SMP specific code
 *
 **********************************************************************/

#ifdef CONFIG_SMP

/* These must match definitions in smp.c.  Trying not to change smp.c for now */
#define PPC_MSG_CALL_FUNCTION	0
#define PPC_MSG_RESCHEDULE	1
#define PPC_MSG_INVALIDATE_TLB	2
#define PPC_MSG_XMON_BREAK	3

/* The IPIs subdivide the group 0 interrupt word into 4 bytes.
 *
 *  CRIX.... CRIX.... CRIX.... CRIX....
 *  0        8        16       24     31
 *  --cpu0-- --cpu1-- --cpu2-- --cpu3--
 *
 * where C=call, R=resched, I=invalidate, and .=unused
 *
 * We simply register each IPI func separately and let Linux sort them
 * out and call the specific handler that is registered.  The IRQs
 * reside in idependent registers and therefore require no
 * synchronization on the registers.
 */
#define IPI_IRQ(cpu, func) (((cpu) << 3) + (func))

/*
 * multiprocessor IRQ controller
 */
static void bg_ipi_irq_enable(unsigned int irq)
{
	int cpu = irq / 8;
	bg_irqctrl_set_target(irq, BGP_IRQCTRL_TARGET_NONCRIT(cpu));
}

static struct irq_chip bg_ipi_irq_ctrl = {
	.name = "BG IPI",
	.enable = bg_ipi_irq_enable,
	.disable = bg_devirq_disable,
	.ack = bg_devirq_ack,
};

static void smp_bluegene_message_pass(int target, int msg)
{
	unsigned int i;

        cpumask_t mask = CPU_MASK_ALL;
        /* make sure we're sending something that translates to an IPI */
        if (msg > 0x3) {
                printk("SMP %d: smp_message_pass: unknown msg %d\n",
                       smp_processor_id(), msg);
                return;
        }

	if (target < NR_CPUS) {
		bg_irqctrl_raise_irq(IPI_IRQ(target, msg));
	} else {
		for_each_online_cpu(i) {
			if (target == MSG_ALL_BUT_SELF
			    && i == smp_processor_id())
				continue;
			bg_irqctrl_raise_irq(IPI_IRQ(i, msg));
		}
	}
}

extern void smp_call_function_interrupt(void); /* in smp.c */
static irqreturn_t bluegene_ipi_call_function(int irq, void *dev_id)
{
	smp_call_function_interrupt();
	return IRQ_HANDLED;
}

static irqreturn_t bluegene_ipi_reschedule(int irq, void *dev_id)
{
	set_need_resched();
	return IRQ_HANDLED;
}

static irqreturn_t bluegene_ipi_invalidate_tlb(int irq, void *dev_id)
{
	smp_invalidate_interrupt();
	return IRQ_HANDLED;
}

#ifdef CONFIG_XMON
static irqreturn_t bluegene_ipi_xmon_break(int irq, void *dev_id)
{
	xmon(regs);
	return IRQ_HANDLED;
}
#endif

/*
 * Return number of cpus present in the system.
 */
static int smp_bluegene_probe(void)
{
#warning hard-coded number of CPUs
	return 4;
}

static void smp_bluegene_kick_cpu(int nr)
{
	//secondary_ack[nr] = 0;
	mb();
}

extern void MMU_init_hw(void);	/* in mm/44x_mmu.c */
extern int mmu_mapin_ram(void);		/* in mm/44x_mmu.c */
static void smp_bluegene_setup_cpu(int nr)
{
	/* enable decrementer */
	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_DIS | TSR_FIS);
	mtspr(SPRN_TCR, TCR_DIE);

	/* flush icache */
	MMU_init_hw();

	request_irq(IPI_IRQ(nr, PPC_MSG_CALL_FUNCTION),
		    bluegene_ipi_call_function, IRQF_DISABLED | IRQF_PERCPU,
		    "IPI func call", NULL);
	request_irq(IPI_IRQ(nr, PPC_MSG_RESCHEDULE),
		    bluegene_ipi_reschedule, IRQF_DISABLED | IRQF_PERCPU,
		    "IPI reschedule", NULL);
	request_irq(IPI_IRQ(nr, PPC_MSG_INVALIDATE_TLB),
		    bluegene_ipi_invalidate_tlb, IRQF_DISABLED | IRQF_PERCPU,
		    "IPI TLB invalidate", NULL);
#ifdef CONFIG_XMON
	request_irq(IPI_IRQ(nr, PPC_MSG_XMON_BREAK),
		    bluegene_ipi_xmon_break, IRQF_DISABLED | IRQF_PERCPU,
		    "IPI xmon break", NULL);
#endif
}


static void smp_bluegene_take_timebase(void)
{
	/* nothing to do (timebase already in sync) */
}


static void smp_bluegene_give_timebase(void)
{
	/* nothing to do (timebase already in sync) */
}

static struct smp_ops_t bluegene_smp_ops = {
	.message_pass = smp_bluegene_message_pass,
	.probe = smp_bluegene_probe,
	.kick_cpu = smp_bluegene_kick_cpu,
	.setup_cpu = smp_bluegene_setup_cpu,
	.take_timebase = smp_bluegene_take_timebase,
	.give_timebase = smp_bluegene_give_timebase,
};
#endif /* CONFIG_SMP */



static void __init bluegene_irq_init(void)
{
        int irq, group;

	bg_irqctrl = (struct bg_irqctrl*)ioremap(BGP_IRQCTRL_BASE, BGP_IRQCTRL_SIZE);

	if (!bg_irqctrl)
	    panic("Could not map IRQ controller\n");

#ifdef CONFIG_SMP
	for (i = 0; i < 32; i++)
		set_irq_chip_and_handler(i, &bg_ipi_irq_ctrl, handle_percpu_irq);
#endif

	for (irq = 32; irq < NR_IRQS; irq++)
		set_irq_chip_and_handler(irq, &bg_devirq_ctrl, handle_level_irq);

	// reset IRQ controller
	for (group = 0; group < BGP_MAX_GROUP; group++)
	{
		// mask all interrupts
		volatile unsigned *reg = &bg_irqctrl->groups[group].target_irq[0];
		out_be32(&reg[0], 0);
		out_be32(&reg[1], 0);
		out_be32(&reg[2], 0);
		out_be32(&reg[3], 0);

		// ack if any was pending
		out_be32(&bg_irqctrl->groups[group].status_clr, 0xffffffff);
	}

	ppc_md.get_irq = bg_irqctrl_get_irq;
}


static void __init bluegene_calibrate_decr(void)
{
	ibm44x_calibrate_decr(__res.bi_intfreq * 1000000);
}

static unsigned long __init bluegene_find_end_of_memory(void)
{
	return __res.bi_memsize;
}

static int bluegene_show_percpuinfo(struct seq_file *m, int i)
{
        seq_printf(m, "clock\t\t: %ldMHz\n", (long)__res.bi_intfreq / 1000000);
        return 0;
}

static int bluegene_show_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "vendor\t\t: IBM\n");
	seq_printf(m, "machine\t\t: Blue Gene\n");
	return 0;
}

static void __init bluegene_setup_arch(void)
{
	int cpu;

	/* init to some ~sane value until calibrate_delay() runs */
        loops_per_jiffy = 50000000/HZ;

#ifdef CONFIG_SMP
	/* Count any cpus that checked in. */
	bluegene_num_cpus = 1;
	for (cpu = 1; cpu < NR_CPUS; cpu++) {
		if (secondary_ack[cpu])
			bluegene_num_cpus++;
	}
	for (cpu = 0; cpu < bluegene_num_cpus; cpu++) {
		cpu_set(cpu, cpu_possible_map);
		cpu_set(cpu, cpu_present_map);
		set_hard_smp_processor_id(cpu, cpu); // 1:1 mapping
	}
#endif
}

void __init platform_init(unsigned long r3, unsigned long r4,
			  unsigned long r5, unsigned long r6, unsigned long r7)
{
	int i, numcores;

	parse_bootinfo(find_bootinfo());

	/* standard platform initialization and then we overwrite the
	 * handlers */
	ibm44x_platform_init(r3, r4, r5, r6, r7);

	ppc_md.setup_arch = bluegene_setup_arch;
	ppc_md.show_cpuinfo = bluegene_show_cpuinfo;
	ppc_md.show_percpuinfo = bluegene_show_percpuinfo;
	ppc_md.init_IRQ = bluegene_irq_init;
	ppc_md.get_irq = NULL;

	ppc_md.calibrate_decr = bluegene_calibrate_decr;
	ppc_md.find_end_of_memory = bluegene_find_end_of_memory;

	//ppc_md.setup_io_mappings = 0;

#ifdef CONFIG_SMP
	smp_ops = &bluegene_smp_ops;

	numcores = 1;
	for (i = 1; i < numcores; i++) {
		/* TODO: release core */
	}
#endif
}
