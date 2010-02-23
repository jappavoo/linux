/*
 * drivers/net/ibm_newemac/xgmii.h
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
#ifndef __IBM_NEWEMAC_XGMII_H
#define __IBM_NEWEMAC_XGMII_H

/* XGMII bridge registers */
struct xgmii_regs {
	u32 interrupt;
	u32 l3_coherency;
	u32 tomal_emac_control;
	u32 xgxs_ctrl0;
	u32 xgxs_ctrl1;
	u32 xgxs_ctrl2;
	u32 xgxs_status;
	u32 dma_arbiter;
};

#define XMII_XCR0_PMA_CLKS_READY	0x00000400
#define XMII_TECR_XENPAK_RESET		0x00004000

/* XGMII device */
struct xgmii_instance {
	struct xgmii_regs __iomem	*base;

	/* Only one EMAC whacks us at a time */
	struct mutex			lock;

	/* subset of PHY_MODE_XXXX */
	int				mode;

	/* number of EMACs using this XGMII bridge */
	int				users;

	/* OF device instance */
	struct of_device		*ofdev;
};

#ifdef CONFIG_IBM_NEW_EMAC_XGMII

extern int xgmii_init(void);
extern void xgmii_exit(void);
extern int xgmii_attach(struct of_device *ofdev, int input, int mode);
extern void xgmii_detach(struct of_device *ofdev, int input);
extern void xgmii_get_mdio(struct of_device *ofdev, int input);
extern void xgmii_put_mdio(struct of_device *ofdev, int input);
extern void xgmii_set_speed(struct of_device *ofdev, int input, int speed);
extern int xgmii_get_regs_len(struct of_device *ocpdev);
extern void *xgmii_dump_regs(struct of_device *ofdev, void *buf);

#else
# define xgmii_init()		0
# define xgmii_exit()		do { } while(0)
# define xgmii_attach(x,y,z)	(-ENXIO)
# define xgmii_detach(x,y)	do { } while(0)
# define xgmii_get_mdio(x,y)	do { } while(0)
# define xgmii_put_mdio(x,y)	do { } while(0)
# define xgmii_set_speed(x,y,z)	do { } while(0)
# define xgmii_get_regs_len(x)	0
# define xgmii_dump_regs(x,buf)	(buf)
#endif				/* !CONFIG_IBM_NEW_EMAC_XGMII */

#endif /* __IBM_NEWEMAC_XGMII_H */
