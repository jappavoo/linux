/*
 * arch/powerpc/sysdev/bgic.c
 *
 * IBM Blue Gene/P Interrupt Controller
 *
 * Copyright 2007 Volkmar Uhlig <vuhlig@us.ibm.com>, IBM Corporation.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <asm/io.h>
#include <asm/prom.h>

/*
 * Interrupt controller
 */
#define BGP_IRQCTRL_BASE	(0x730000000ULL)
#define BGP_IRQCTRL_SIZE	(0x1000)
#define BGP_MAX_GROUP		(10)
#define BGP_MAX_CORE		4

#define IRQ_TO_GROUP(irq)	((irq >> 5) & 0xf)
#define GROUP_TO_IRQ(group)	(group << 5)
#define IRQ_OF_GROUP(irq)	((irq & 0x1f))

#define BGP_IRQCTRL_TARGET_DISABLED		(0x0)
#define BGP_IRQCTRL_TARGET_BCAST_NONCRIT	(0x1)
#define BGP_IRQCTRL_TARGET_BCAST_CRIT		(0x2)
#define BGP_IRQCTRL_TARGET_BCAST_MCHECK		(0x3)
#define BGP_IRQCTRL_TARGET_NONCRIT(cpu)		(0x4 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_CRIT(cpu)		(0x8 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_MCHECK(cpu)		(0xc | (cpu & 0x3))

#define NR_BGIC_INTS	(BGP_MAX_GROUP * 32)
#define NR_BGIC_IPI	(32)

struct bgic_group {
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

struct bgic {
	struct bgic_group groups[15];
	volatile unsigned int core_non_crit[BGP_MAX_CORE];
	volatile unsigned int core_crit[BGP_MAX_CORE];
	volatile unsigned int core_mchk[BGP_MAX_CORE];
} __attribute__((packed));

struct bg_bic_irq_desc {
	int server_cpu;
	int server_id;
	int count;
	int rotation_interval;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *ri_entry;
#endif
};

struct bg_bic {
	struct bgic *ctrl;
	struct irq_host *irqhost;
	struct device_node *of_node;
	spinlock_t lock;
	struct bg_bic_irq_desc desc[NR_IRQS];
};

static struct bg_bic bic;

#define virq_to_hw(virq)	(irq_map[virq].hwirq)

#ifdef CONFIG_SMP
static int get_irq_server(unsigned int virq)
{
	struct bg_bic_irq_desc *d = &bic.desc[virq];

	if (d->rotation_interval > 0) {
		d->count++;
		if (d->count >= d->rotation_interval) {
			cpumask_t tmp;
			cpus_and(tmp, cpu_online_map, irq_desc[virq].affinity);
			if (!cpus_empty(tmp)) {
				d->server_cpu = next_cpu(d->server_cpu, tmp);
				if (d->server_cpu == NR_CPUS) {
				    d->server_cpu = first_cpu(tmp);
				}
				d->server_id = get_hard_smp_processor_id(
								d->server_cpu);
			}
			d->count = 0;
		}
	}

	return d->server_id;
}
#else
static int get_irq_server(unsigned int virq) { return 0; }
#endif

// returns the pending HW IRQ
static int bgic_get_hwirq(void)
{
	int group = -1, irq = NO_IRQ;
	int cpu = smp_processor_id();
	unsigned long hierarchy, mask = 0;

	BUG_ON(!bic.ctrl);

	hierarchy = in_be32(&bic.ctrl->core_non_crit[cpu]);

	// handle spurious interrupts...
	if (unlikely(hierarchy == 0))
		goto out;

	// ffs is in little endian format...
	group = BITS_PER_LONG - ffs(hierarchy);

	if (unlikely(group >= BGP_MAX_GROUP))
		goto out;

	mask = in_be32(&bic.ctrl->groups[group].noncrit_mask[cpu]);

	if (unlikely(mask == 0))
		goto out;

	irq = GROUP_TO_IRQ(group) + (BITS_PER_LONG - ffs(mask));

 out:
	return irq;
}

static void bgic_set_target(unsigned int hwirq, unsigned int target)
{
	unsigned offset = ((7 - (hwirq & 0x7)) * 4);
	volatile void *reg;

	BUG_ON(!bic.ctrl);
	BUG_ON(hwirq >= NR_BGIC_INTS);

	reg = &bic.ctrl->groups[IRQ_TO_GROUP(hwirq)].target_irq[IRQ_OF_GROUP(hwirq) / 8];

	spin_lock(&bic.lock);
	out_be32(reg, (in_be32(reg) & ~(0xf << offset)) | ((target & 0xf) << offset));
	spin_unlock(&bic.lock);
}

#ifdef CONFIG_SMP
void bgic_raise_irq(unsigned int hwirq)
{
	unsigned mask = 1U << (31 - IRQ_OF_GROUP(hwirq));
	out_be32(&bic.ctrl->groups[IRQ_TO_GROUP(hwirq)].status_set, mask);
}
#endif

static void bgic_ack_irq(unsigned int hwirq)
{
	unsigned mask = 1U << (31 - IRQ_OF_GROUP(hwirq));
	out_be32(&bic.ctrl->groups[IRQ_TO_GROUP(hwirq)].status_clr, mask);
}


/*
 * device IRQ handling (routed to one CPU)
 */
static void bg_devirq_enable(unsigned int virq)
{
	bgic_set_target(virq_to_hw(virq), BGP_IRQCTRL_TARGET_NONCRIT( get_irq_server(virq) ));
}

static void bg_devirq_disable(unsigned int virq)
{
	bgic_set_target(virq_to_hw(virq), BGP_IRQCTRL_TARGET_DISABLED);
}

static void bg_devirq_ack(unsigned int virq)
{
	bgic_ack_irq(virq_to_hw(virq));
}

static void bg_devirq_eoi(unsigned int virq)
{
	bg_devirq_ack(virq);
	bg_devirq_enable(virq);
}

static void bg_devirq_set_affinity(unsigned int virq, cpumask_t cpumask)
{
	struct bg_bic_irq_desc *d = &bic.desc[virq];

	cpus_and(cpumask, cpu_online_map, cpumask);
	d->server_cpu = cpus_empty(cpumask) ? 0 : first_cpu(cpumask);
#ifdef CONFIG_SMP
	d->server_id = get_hard_smp_processor_id(d->server_cpu);
#endif
	d->count = 0;
}

#ifdef CONFIG_PROC_FS
static int rotation_interval_read_proc(char *page, char **start, off_t off,
				       int count, int *eof, void *data)
{
	unsigned int virq = (unsigned int) (long) data;
	struct bg_bic_irq_desc *d = &bic.desc[virq];
	int len;

	len = snprintf(page, count, "%d", d->rotation_interval);

	if (count - len < 2) {
		return -EINVAL;
	}
	len += sprintf(page + len, "\n");
	return len;
}

static int rotation_interval_write_proc(struct file *file,
					const char __user *buffer,
				        unsigned long count, void *data)
{
	unsigned int virq = (unsigned int) (long) data;
	struct bg_bic_irq_desc *d = &bic.desc[virq];

	sscanf(buffer, "%d", &d->rotation_interval);

	return count;
}
#endif

static unsigned int bg_devirq_startup(unsigned int virq)
{
	struct bg_bic_irq_desc *d = &bic.desc[virq];

#ifdef CONFIG_PROC_FS
	if (irq_desc[virq].dir != NULL) {
		/* create /proc/irq/<irq>/rotation_interval */
		d->ri_entry = create_proc_entry("rotation_interval", 0600,
					        irq_desc[virq].dir);
		if (d->ri_entry != NULL) {
			d->ri_entry->data = (void *) (long) virq;
			d->ri_entry->read_proc = rotation_interval_read_proc;
			d->ri_entry->write_proc = rotation_interval_write_proc;
		}
	}
#endif

	d->rotation_interval = 0;
#ifdef CONFIG_SMP
	bg_devirq_set_affinity(virq, irq_desc[virq].affinity);
#endif
	bg_devirq_enable(virq);
	return 0;
}

static void bg_devirq_shutdown(unsigned int virq)
{
	struct bg_bic_irq_desc *d = &bic.desc[virq];

#ifdef CONFIG_PROC_FS
	if (d->ri_entry != NULL) {
		BUG_ON(irq_desc[virq].dir == NULL);
		remove_proc_entry(d->ri_entry->name, irq_desc[virq].dir);
		d->ri_entry = NULL;
	}
#endif
}

static struct irq_chip bg_devirq_chip = {
	.name = "BG IRQ Controller",
	.mask = bg_devirq_disable,
	.unmask = bg_devirq_enable,
	.ack = bg_devirq_ack,
	.eoi = bg_devirq_eoi,
	.set_affinity = bg_devirq_set_affinity,
	.startup = bg_devirq_startup,
	.shutdown = bg_devirq_shutdown,
};

#ifdef CONFIG_SMP

/* 8 IPIs per-CPU; each IPI maps into a different control register
 * avoiding locks */
#define IPI_IRQ(cpu, func) (((cpu) << 3) + (4 + func))

/*
 * multiprocessor IRQ controller
 */
static void bg_ipi_enable(unsigned int virq)
{
	int hwirq = virq_to_hw(virq);
	int cpu = hwirq / 8;
	BUG_ON(hwirq >= 32);
	bgic_set_target(hwirq, BGP_IRQCTRL_TARGET_NONCRIT(cpu));
}

static struct irq_chip bg_ipi_chip = {
	.name = "BG IPI",
	.enable = bg_ipi_enable,
	.disable = bg_devirq_disable,
	.ack = bg_devirq_ack,
};

static irqreturn_t bgic_ipi_action(int virq, void *devid)
{
	smp_message_recv(virq_to_hw(virq) - IPI_IRQ(smp_processor_id(), 0));
	return IRQ_HANDLED;
}

static void bgic_request_ipi(int cpu)
{
	int i, err;
	static char *ipi_names[] = {
		"IPI0 (call function)",
		"IPI1 (reschedule)",
		"IPI2 (unused)",
		"IPI3 (debugger break)",
	};
	for (i = 0; i < 4; i++) {
		unsigned int vipi = irq_create_mapping(bic.irqhost, IPI_IRQ(cpu, i));
		if (vipi == NO_IRQ) {
			printk(KERN_ERR "Failed to map IPI %d\n", i);
			break;
		}
		err = request_irq(vipi, bgic_ipi_action,
				  IRQF_DISABLED|IRQF_PERCPU,
				  ipi_names[i], &bic);
		if (err) {
			printk(KERN_ERR "Request of irq %d for IPI %d failed\n",
			       vipi, i);
			break;
		}
	}
}

void smp_bgic_message_pass(int target, int msg)
{
	unsigned int i;

        /* make sure we're sending something that translates to an IPI */
        if (msg > 0x3) {
                printk("SMP %d: smp_message_pass: unknown msg %d\n",
                       smp_processor_id(), msg);
                return;
        }

	if (target < NR_CPUS) {
		bgic_raise_irq(IPI_IRQ(target, msg));
	} else {
		for_each_online_cpu(i) {
			if (target == MSG_ALL_BUT_SELF
			    && i == smp_processor_id())
				continue;
			bgic_raise_irq(IPI_IRQ(i, msg));
		}
	}
}

int __init smp_bgic_probe(void)
{
	int cpu;

	if (num_present_cpus() > 1) {
		for_each_possible_cpu(cpu)
			bgic_request_ipi(cpu);
	}

	return num_present_cpus();
}

#endif /* CONFIG_SMP */



static int bgic_host_map(struct irq_host *h, unsigned int virq,
			 irq_hw_number_t hw)
{
#ifdef CONFIG_SMP
	if (hw < NR_BGIC_IPI)
		set_irq_chip_and_handler(virq, &bg_ipi_chip, handle_percpu_irq);
	else
#endif
		set_irq_chip_and_handler(virq, &bg_devirq_chip, handle_level_irq);

	set_irq_type(virq, IRQ_TYPE_NONE);
	return 0;
}

static struct irq_host_ops bgic_host_ops = {
	.map	= bgic_host_map,
};


void __init bgic_init(void)
{
	phys_addr_t *addr;
	int len, irq;
	struct device_node *np;

	/* find device node.  We only support one. */
	np = of_find_compatible_node(NULL, NULL, "ibm,bgic");
	BUG_ON(!np);

	addr = (phys_addr_t*)of_get_property(np, "reg", &len);
	if (!*addr || len != sizeof(phys_addr_t) + sizeof(int)) {
		panic("bgic: Device node %s has missing or invalid property\n",
		       np->full_name);
	}

	spin_lock_init(&bic.lock);

	bic.irqhost = irq_alloc_host(IRQ_HOST_MAP_LINEAR, NR_BGIC_INTS,
				     &bgic_host_ops, -1);
	if (!bic.irqhost)
		panic("bgic: could not allocate irqhost");

	bic.ctrl = (struct bgic*)ioremap(*addr, BGP_IRQCTRL_SIZE);
	if (!bic.ctrl)
	    panic("bgic: could not map IRQ controller");

	// mask all IRQs
	for (irq = 0; irq < NR_BGIC_INTS; irq++)
		bgic_set_target(irq, BGP_IRQCTRL_TARGET_DISABLED);

	bic.of_node = np;
	irq_set_default_host(bic.irqhost);
	of_node_put(np);
}


int bgic_get_virq(void)
{
	return irq_linear_revmap(bic.irqhost, bgic_get_hwirq());
}
