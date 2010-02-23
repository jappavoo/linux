/*
 * Copyright (C) 2007 Volkmar Uhlig (vuhlig@us.ibm.com), IBM Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Blue Gene Console over JTAG.
 */

#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/kbd_kern.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include "ppc450.h"

#define BLUEGENE_MAJOR	229
#define BLUEGENE_MINOR	0

#define MB_BUF_SIZE	128

/* Bluegene firmware */
#define BGP_MAX_MAILBOX			(4)
#define BGP_MAILBOX_DESC_OFFSET		(0x7fd0)

#define BGP_DCR_TEST(x)			(0x400 + (x))
#define BGP_DCR_GLOB_ATT_WRITE_SET	BGP_DCR_TEST(0x17)
#define BGP_DCR_GLOB_ATT_WRITE_CLEAR	BGP_DCR_TEST(0x18)
#define BGP_DCR_TEST_STATUS6		BGP_DCR_TEST(0x3a)
#define   BGP_TEST_STATUS6_IO_CHIP	(0x80000000U >> 3)

#define BGP_ALERT_OUT(core)	        (0x80000000U >> (24 + core))
#define BGP_ALERT_IN(core)	        (0x80000000U >> (28 + core))

#define BGP_JMB_CMD2HOST_PRINT          (0x0002)

/*
 * SRAM
 */
#define BGP_SRAM_BASE		(0x7FFFF8000ULL)
#define BGP_SRAM_SIZE		(32*1024)
#define BGP_PERSONALITY_OFFSET	(0xfffff800U - 0xffff8000U)

/* XXX: convert to use OF device tree */

/*
 * mailbox access
 */

struct bgp_mailbox_desc {
	u16 offset;	// offset from SRAM base
	u16 size;	// size including header, 0=not present
} __attribute__((packed));

struct bgp_mailbox {
	volatile u16 command;	// comand; upper bit=ack
	u16 len;		// length (does not include header)
	u16 result;		// return code from reader
	u16 crc;		// 0=no CRC
	char data[0];
};

static struct {
	struct bgp_mailbox *mb;
	unsigned size;
} mb_in[BGP_MAX_MAILBOX], mb_out[BGP_MAX_MAILBOX];

static int default_outbox = -1;
static void *__sram_mapping = NULL;

static int setup_mailboxes(void *sram_mapping)
{
	int i;
	struct bgp_mailbox_desc* mb_desc = sram_mapping + BGP_MAILBOX_DESC_OFFSET;

	for (i = 0; i < BGP_MAX_MAILBOX; i++)
	{
		if ( mb_desc->size ) {
			mb_in[i].mb = sram_mapping + mb_desc->offset;
			mb_in[i].size = mb_desc->size - sizeof(struct bgp_mailbox);
		}
		mb_desc++;

		if ( mb_desc->size ) {
			mb_out[i].mb = sram_mapping + mb_desc->offset;
			mb_out[i].size = mb_desc->size - sizeof(struct bgp_mailbox);
			if (default_outbox == -1 || mb_out[default_outbox].size < mb_desc->size)
				default_outbox = i;
		}
		mb_desc++;
	}
	return (default_outbox != -1);
}


void mailbox_write(int mailbox, unsigned command, void *msg, unsigned len)
{
	mailbox = 0;
	struct bgp_mailbox *mbox = mb_out[mailbox].mb;

	if (!mbox)
		return;

	if (len > mb_out[mailbox].size)
		len = mb_out[mailbox].size;

	memcpy(&mbox->data, msg, len);
	mbox->len = len;
	mbox->command = command;
	mb();
	mtdcrx(BGP_DCR_GLOB_ATT_WRITE_SET, BGP_ALERT_OUT(mailbox));
}


int mailbox_test_completion(int mailbox)
{
	int rc;
	mailbox = 0;
	struct bgp_mailbox *mbox = mb_out[mailbox].mb;

	if (!mbox)
		return 0;

	// check for handshake
	rc = mbox->command & 0x8000;

	if (rc)
		mtdcrx(BGP_DCR_GLOB_ATT_WRITE_CLEAR, BGP_ALERT_OUT(mailbox));
	return rc;
}

static spinlock_t mailbox_lock;
static char mailbox_buffer[MB_BUF_SIZE];
static int buflen = 0;

void mailbox_putc (const unsigned char c)
{
	mailbox_buffer[buflen] = c;
	buflen++;

	if (buflen >= MB_BUF_SIZE || c == '\n')
	{
		mailbox_write(smp_processor_id(), 2, mailbox_buffer, buflen);
		while ( !mailbox_test_completion(smp_processor_id()) ) { /* wait */ }
		buflen = 0;
	}
}

void mailbox_puts (const unsigned char *s, int count)
{
	unsigned long flags;
	if (count > 0)
	{
		spin_lock_irqsave(&mailbox_lock, flags);
		while (count--)
			mailbox_putc(*s++);
		spin_unlock_irqrestore(&mailbox_lock, flags);
	}
}



/*
 * TTY driver access
 */
static struct tty_driver *bluegene_tty_driver;

static int bluegenecons_open(struct tty_struct *tty, struct file * filp)
{
	return 0;
}

static void bluegenecons_close(struct tty_struct *tty, struct file * filp)
{
}

static int bluegenecons_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	mailbox_puts(buf, count);
	return count;
}

static int bluegenecons_write_room(struct tty_struct *tty)
{
	return MB_BUF_SIZE; /* something, we don't buffer anyhow */
}

static int bluegenecons_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}

static struct tty_operations bgcons_tty_ops = {
	.open = bluegenecons_open,
	.close = bluegenecons_close,
	.write = bluegenecons_write,
	.write_room = bluegenecons_write_room,
	.chars_in_buffer = bluegenecons_chars_in_buffer,
};

static int __init bluegenecons_init(void)
{
	bluegene_tty_driver = alloc_tty_driver(1);
	if (!bluegene_tty_driver)
		return -ENOMEM;

	bluegene_tty_driver->owner = THIS_MODULE;
	bluegene_tty_driver->name = "bgcons";
	bluegene_tty_driver->name_base = 1;
	bluegene_tty_driver->major = BLUEGENE_MAJOR;
	bluegene_tty_driver->minor_start = BLUEGENE_MINOR;
	bluegene_tty_driver->type = TTY_DRIVER_TYPE_SYSTEM;
	bluegene_tty_driver->init_termios = tty_std_termios;
	bluegene_tty_driver->flags = TTY_DRIVER_REAL_RAW;
	tty_set_operations(bluegene_tty_driver, &bgcons_tty_ops);

	if (tty_register_driver(bluegene_tty_driver))
		panic("Couldn't register tty driver");

	return 0;
}
module_init(bluegenecons_init);

/*
 * Early console driver.
 */
static void bluegene_console_write(struct console *co, const char *b, unsigned count)
{
	mailbox_puts(b, count);
}

static struct tty_driver *bluegene_console_device(struct console *c, int *ip)
{
	*ip = 0;
	return bluegene_tty_driver;
}

static struct console bgcons = {
	.name	= "bgcons",
	.write	= bluegene_console_write,
	.device = bluegene_console_device,
	.flags	= CON_PRINTBUFFER,
	.index	= 0,
};

int __init bluegene_console_init(void)
{
	__sram_mapping = ioremap(BGP_SRAM_BASE, BGP_SRAM_SIZE);
	if (!__sram_mapping)
		return -1;

	if (setup_mailboxes(__sram_mapping))
		register_console(&bgcons);
	return 0;
}

console_initcall(bluegene_console_init);
