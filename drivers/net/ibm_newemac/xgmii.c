/*
 * drivers/net/ibm_newemac/xgmii.c
 *
 * Driver for PowerPC 4xx on-chip ethernet controller, XGMII bridge support.
 *
 * Copyright (c) 2004, 2005 Zultys Technologies.
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 *
 * Based on original work by
 *      Armin Kuster <akuster@mvista.com>
 * 	Copyright 2001 MontaVista Softare Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/ethtool.h>
#include <asm/io.h>

#include "emac.h"
#include "core.h"


/* XGMII only supports MII, RMII and SMII
 * we also support autodetection for backward compatibility
 */
static inline int xgmii_valid_mode(int mode)
{
	return  mode == PHY_MODE_XGMII ||
		mode == PHY_MODE_NA;
}

static inline const char *xgmii_mode_name(int mode)
{
	switch (mode) {
	case PHY_MODE_XGMII:
		return "XGMII";
	default:
		BUG();
	}
}

static inline u32 xmii_mode_mask(int mode, int input)
{
	switch (mode) {
	case PHY_MODE_XGMII:
		//return XGMII_FER_SMII(input);
	default:
		return 0;
	}
}

int __devinit xgmii_attach(struct of_device *ofdev, int input, int mode)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);
	//struct xgmii_regs *p = dev->base;

	XGMII_DBG2(dev, "init(%d, %d)" NL, input, mode);

	if (!xgmii_valid_mode(mode))
		/* Probably an EMAC connected to RGMII,
		 * but it still may need XGII for MDIO so
		 * we don't fail here.
		 */
		return 0;

	mutex_lock(&dev->lock);

	/* Enable this input */
	//out_be32(&p->fer, in_be32(&p->fer) | zmii_mode_mask(dev->mode, input));
	++dev->users;

	mutex_unlock(&dev->lock);
	return 0;
}

void xgmii_get_mdio(struct of_device *ofdev, int input)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);

	XGMII_DBG2(dev, "get_mdio(%d)" NL, input);
	mutex_lock(&dev->lock);
}

void xgmii_put_mdio(struct of_device *ofdev, int input)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);

	XGMII_DBG2(dev, "put_mdio(%d)" NL, input);
	mutex_unlock(&dev->lock);
}


void xgmii_set_speed(struct of_device *ofdev, int input, int speed)
{
	XGMII_DBG(dev, "speed(%d, %d)" NL, input, speed);
}

void __devexit xgmii_detach(struct of_device *ofdev, int input)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);

	BUG_ON(!dev || dev->users == 0);

	mutex_lock(&dev->lock);

	XGMII_DBG(dev, "detach(%d)" NL, input);

	--dev->users;

	mutex_unlock(&dev->lock);
}

int xgmii_get_regs_len(struct of_device *ofdev)
{
	return sizeof(struct emac_ethtool_regs_subhdr) +
		sizeof(struct xgmii_regs);
}

void *xgii_dump_regs(struct of_device *ofdev, void *buf)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);
	struct emac_ethtool_regs_subhdr *hdr = buf;
	struct xgmii_regs *regs = (struct xgmii_regs *)(hdr + 1);

	hdr->version = 0;
	hdr->index = 0; /* for now, are there chips with more than one
			 * zmii ? if yes, then we'll add a cell_index
			 * like we do for emac
			 */
	memcpy_fromio(regs, dev->base, sizeof(struct xgmii_regs));
	return regs + 1;
}

static int __devinit xgmii_probe(struct of_device *ofdev,
				 const struct of_device_id *match)
{
	struct device_node *np = ofdev->node;
	struct xgmii_instance *dev;
	struct resource regs;
	int rc;

	rc = -ENOMEM;
	dev = kzalloc(sizeof(struct xgmii_instance), GFP_KERNEL);
	if (dev == NULL) {
		printk(KERN_ERR "%s: could not allocate XGMII device!\n",
		       np->full_name);
		goto err_gone;
	}

	mutex_init(&dev->lock);
	dev->ofdev = ofdev;
	dev->mode = PHY_MODE_NA;

	rc = -ENXIO;
	if (of_address_to_resource(np, 0, &regs)) {
		printk(KERN_ERR "%s: Can't get registers address\n",
		       np->full_name);
		goto err_free;
	}

	rc = -ENOMEM;
	dev->base = (struct xgmii_regs *)ioremap(regs.start,
						sizeof(struct xgmii_regs));
	if (dev->base == NULL) {
		printk(KERN_ERR "%s: Can't map device registers!\n",
		       np->full_name);
		goto err_free;
	}

	// reset device
	out_be32(&dev->base->tomal_emac_control,
		 in_be32(&dev->base->tomal_emac_control) & ~XMII_TECR_XENPAK_RESET);
	udelay(10000);
	out_be32(&dev->base->tomal_emac_control,
		 in_be32(&dev->base->tomal_emac_control) | XMII_TECR_XENPAK_RESET);
	udelay(10000);

	// signal PMA clocks ready
	out_be32(&dev->base->xgxs_ctrl0, in_be32(&dev->base->xgxs_ctrl0) | XMII_XCR0_PMA_CLKS_READY);

	printk(KERN_INFO
	       "XGMII %s initialized\n", ofdev->node->full_name);
	wmb();
	dev_set_drvdata(&ofdev->dev, dev);

	return 0;

 err_free:
	kfree(dev);
 err_gone:
	return rc;
}

static int __devexit xgmii_remove(struct of_device *ofdev)
{
	struct xgmii_instance *dev = dev_get_drvdata(&ofdev->dev);

	dev_set_drvdata(&ofdev->dev, NULL);

	WARN_ON(dev->users != 0);

	iounmap(dev->base);
	kfree(dev);

	return 0;
}

static struct of_device_id xgmii_match[] =
{
	{
		.compatible	= "ibm,xgmii",
	},
	{},
};

static struct of_platform_driver xgmii_driver = {
	.name = "emac-xgmii",
	.match_table = xgmii_match,

	.probe = xgmii_probe,
	.remove = xgmii_remove,
};

int __init xgmii_init(void)
{
	return of_register_platform_driver(&xgmii_driver);
}

void __exit xgmii_exit(void)
{
	of_unregister_platform_driver(&xgmii_driver);
}
