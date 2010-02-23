/*
 * Blue Gene/P board specific routines
 *
 * Matt Porter <mporter@kernel.crashing.org>
 * Copyright 2002-2005 MontaVista Software Inc.
 *
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 * Copyright (c) 2003-2005 Zultys Technologies
 *
 * Rewritten and ported to the merged powerpc tree:
 * Copyright 2007 David Gibson <dwg@au1.ibm.com>, IBM Corporation.
 *
 * Adopted for Blue Gene:
 * Copyright 2007 Volkmar Uhlig <vuhlig@us.ibm.com>, IBM Corporation.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/rtc.h>
#include <asm/machdep.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/time.h>
#include <asm/bgic.h>
#include <asm/of_platform.h>
#include <asm/ppc450.h>

#include "44x.h"

extern void bluegene_tree_deactivate(void);

#ifdef CONFIG_SMP
extern unsigned long secondary_ack[NR_CPUS]; /* head_44x.S */
#endif

static struct of_device_id bluegene_of_bus[] = {
	{ .compatible = "ibm,plb4", },
	{ .compatible = "ibm,opb", },
	{},
};

static int __init bluegene_device_probe(void)
{
	if (!machine_is(bluegene))
		return 0;

	of_platform_bus_probe(NULL, bluegene_of_bus, NULL);

	return 0;
}
device_initcall(bluegene_device_probe);

/*
 * Called very early, MMU is off, device-tree isn't unflattened
 */
static int __init bluegene_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (!of_flat_dt_is_compatible(root, "ibm,bluegene/p"))
	{
		return 0;
	}

	return 1;
}

#ifdef CONFIG_SMP
static void smp_bluegene_kick_cpu(int nr)
{
	if (secondary_ack[nr] == 0)
		printk("Kick CPU of offline processor %d\n", nr);
	secondary_ack[nr] = 0;
	mb();
}

static void smp_bluegene_setup_cpu(int nr)
{
	/* Clear any pending timer interrupts */
	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_DIS | TSR_FIS);

	/* Enable decrementer interrupt */
	mtspr(SPRN_TCR, TCR_DIE);
}

static volatile u64 timebase = 0;

static void smp_bluegene_give_timebase(void)
{
	timebase = get_tb();
	mb();
	while(timebase)
		barrier();
}

static void smp_bluegene_take_timebase(void)
{
	while(!timebase)
		barrier();
	set_tb(timebase >> 32, timebase);
	wmb();
	timebase = 0;
	wmb();
}

static struct smp_ops_t bluegene_smp_ops = {
	.message_pass = smp_bgic_message_pass,
	.probe = smp_bgic_probe,
	.kick_cpu = smp_bluegene_kick_cpu,
	.setup_cpu = smp_bluegene_setup_cpu,
	.give_timebase = smp_bluegene_give_timebase,
	.take_timebase = smp_bluegene_take_timebase,
};
#endif /* CONFIG_SMP */

static void bluegene_restart(char *cmd)
{
	bluegene_tree_deactivate();
	while(1);
}

static void bluegene_power_off(void)
{
	bluegene_tree_deactivate();
	while(1);
}

static void bluegene_panic(char *str)
{
	bluegene_tree_deactivate();
	while(1);
}

static void __init bluegene_setup_arch(void)
{
#ifdef CONFIG_SMP
	smp_ops = &bluegene_smp_ops;
#endif
}

#define READ_RTC_FDT(name, val)					\
	if ((prop = get_property(np, name, &len)) != NULL)	\
		val = *prop;


static unsigned long bluegene_get_boot_time(void)
{
	struct rtc_time tm;
	int len;
	const u32 *prop;
	struct device_node *np = of_find_node_by_path("/rtc");
	if (np == NULL)
		return 0;

	memset(&tm, 0, sizeof(tm));

	READ_RTC_FDT("second", tm.tm_sec);
	READ_RTC_FDT("minute", tm.tm_min);
	READ_RTC_FDT("hour", tm.tm_hour);
	READ_RTC_FDT("mday", tm.tm_mday);
	READ_RTC_FDT("month", tm.tm_mon);
	READ_RTC_FDT("year", tm.tm_year);
	if (tm.tm_year < 1900)
		tm.tm_year += 1900;

	return mktime(tm.tm_year, tm.tm_mon, tm.tm_mday,
		      tm.tm_hour, tm.tm_min, tm.tm_sec);
}

pgprot_t bluegene_phys_mem_access_prot(struct file *file, unsigned long pfn,
				       unsigned long size, pgprot_t vma_prot)
{
	// XXX: workaround for broken page_is_ram: first BGP device starts @ 6.0000.0000
	if (pfn >=  (0x600000000ULL >> PAGE_SHIFT))
		vma_prot = __pgprot(pgprot_val(vma_prot) | _PAGE_GUARDED | _PAGE_NO_CACHE);
	return vma_prot;
}


define_machine(bluegene) {
	.name			= "Blue Gene/P",
	.probe			= bluegene_probe,
	.setup_arch		= bluegene_setup_arch,
	.init_IRQ		= bgic_init,
	.get_irq		= bgic_get_virq,
	.get_boot_time		= bluegene_get_boot_time,
	.restart		= bluegene_restart,
	.power_off		= bluegene_power_off,
	.panic			= bluegene_panic,
	.calibrate_decr		= generic_calibrate_decr,
	.phys_mem_access_prot	= bluegene_phys_mem_access_prot,
};
