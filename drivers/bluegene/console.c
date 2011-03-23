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
#include <linux/sysctl.h>
#include <linux/ctype.h>
#include <asm/of_platform.h>
#include <asm/time.h>

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

#define BGP_JMB_CMD2HOST_RAS 0x0004

#define ECID0 BGP_DCR_TEST(0x00D)
#define ECID1 BGP_DCR_TEST(0x00E)
#define ECID2 BGP_DCR_TEST(0x00F)
#define ECID3 BGP_DCR_TEST(0x010)

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
	struct bgp_mailbox *mbox;

	mailbox = 0;
	mbox = mb_out[mailbox].mb;

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
	struct bgp_mailbox *mbox;

	mailbox = 0;
	mbox = mb_out[mailbox].mb;

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

/* FIXME: JA: Ugly kludge : pulled in constants from IBM header files here: */
/*       arch/include/common/bgp_ras.h  */

// List of all BGP facilities (high level component detecting the condition)
// NOTE: These need to correspond to the components defined in bgp/control/ras/RasEvent.h
typedef enum {

             // Facilities
             _bgp_fac_none          = 0x00,

             _bgp_fac_kernel        = 0x01,
             _bgp_fac_application   = 0x02,
             _bgp_fac_card          = 0x03,
             _bgp_fac_mc            = 0x04,
             _bgp_fac_mcserver      = 0x05,
             _bgp_fac_mmcs          = 0x06,
             _bgp_fac_diags         = 0x07,

             _bgp_fac_max
             } _BGP_Facility;

// List of all BGP units that can generate RAS events
typedef enum {
             _bgp_unit_none         = 0x00,

             // hardware detected events
             _bgp_unit_ppc450       = 0x01,
             _bgp_unit_fpu          = 0x02,
             _bgp_unit_snoop        = 0x03,
             _bgp_unit_dp0          = 0x04,
             _bgp_unit_dp1          = 0x05,
             _bgp_unit_l2           = 0x06,
             _bgp_unit_l3           = 0x07,
             _bgp_unit_ddr          = 0x08,
             _bgp_unit_sram         = 0x09,
             _bgp_unit_dma          = 0x0A,
             _bgp_unit_testint      = 0x0B,
             _bgp_unit_testint_dcr  = 0x0C,
             _bgp_unit_lockbox      = 0x0D,
             _bgp_unit_plb          = 0x0E,
             _bgp_unit_collective   = 0x0F,
             _bgp_unit_torus        = 0x10,
             _bgp_unit_globint      = 0x11,
             _bgp_unit_serdes       = 0x12,
             _bgp_unit_upc          = 0x13,
             _bgp_unit_dcr          = 0x14,
             _bgp_unit_bic          = 0x15,
             _bgp_unit_devbus       = 0x16,
             _bgp_unit_netbus       = 0x17,
             _bgp_unit_envmon       = 0x18,
             _bgp_unit_tomal        = 0x19,
             _bgp_unit_xemac        = 0x1A,
             _bgp_unit_phy          = 0x1B,

             // software detected events/problems
             _bgp_unit_bootloader   = 0x1C,
             _bgp_unit_kernel       = 0x1D,
             _bgp_unit_ciod         = 0x1E,
             _bgp_unit_svc_host     = 0x1F,
             _bgp_unit_diagnostic   = 0x20,
             _bgp_unit_application  = 0x21,
             _bgp_unit_linux	    = 0x22,
	     _bgp_unit_cns          = 0x23, // Common Node Services
	     _bgp_unit_ethernet     = 0x24,
             _bgp_unit_max          = 0xFF
             }
             _BGP_RAS_Units;

/* arch/include/cio/ciod_ras.h */
typedef enum {
   CiodInitialized                     = 0x00,
   CiodException                       = 0x01,
   CiodFunctionFailed                  = 0x02,
   CiodLockCreateFailed                = 0x03,
   CiodDeviceOpenFailed                = 0x04,
   CiodQueueCreateFailed               = 0x05,
   CiodThreadCreateFailed              = 0x06,
   CiodInvalidControlMsg               = 0x07,
   CiodInvalidDataMsg                  = 0x08,
   CiodWrongControlMsg                 = 0x09,
   CiodWrongDataMsg                    = 0x0a,
   CiodCreateListenerFailed            = 0x0b,
   CiodAcceptConnFailed                = 0x0c,
   CiodAllocateFailed                  = 0x0d,
   CiodControlReadFailed               = 0x0e,
   CiodDataReadFailed                  = 0x0f,
   CiodDuplicateFdFailed               = 0x10,
   CiodSetGroupsFailed                 = 0x11,
   CiodSetGidFailed                    = 0x12,
   CiodSetUidFailed                    = 0x13,
   CiodExecFailed                      = 0x14,
   CiodNodeExited                      = 0x15,
   CiodInvalidDebugRequest             = 0x16,
   CiodDebuggerReadFailed              = 0x17,
   CiodInvalidNode                     = 0x18,
   CiodInvalidThread                   = 0x19,
   CiodDebuggerWriteFailed             = 0x1a,
   CiodPersonalityOpenFailed           = 0x1b,
   CiodInvalidPersonality              = 0x1c,
   CiodTooManyNodes                    = 0x1d,
   CiodSharedMemoryCreateFailed        = 0x1e,
   CiodSharedMemoryAttachFailed        = 0x1f,
   CiodIncompleteCoreFile              = 0x20,
   CiodBadAlignment                    = 0x21,
   CiodSharedMemoryExtendFailed        = 0x22,
   CiodHungProxy                       = 0x23,
   CiodInvalidProtocol                 = 0x24,
   CiodControlWriteFailed              = 0x25,
   CiodDataWriteFailed                 = 0x26,
   CiodCreateConnFailed                = 0x27,
   CiodSecondaryControlReadFailed      = 0x28,
   CiodSecondaryControlWriteFailed     = 0x29,
   CiodSecondaryDataReadFailed         = 0x2a,
   CiodSecondaryDataWriteFailed        = 0x2b,
   CiodNotEnoughMemory                 = 0x2c,

} BGP_RAS_CIOD_ErrCodes;

#define BGRAS_MAX_DETAIL_WORDS 32
struct ras_event {
    uint32_t uci;
    uint8_t  facility;
    uint8_t  unit;
    uint8_t  errcode;
    uint8_t  detailwords;
    uint64_t timestamp;
    uint32_t ecid[4];
    union {
        uint32_t words[BGRAS_MAX_DETAIL_WORDS];
        char text[BGRAS_MAX_DETAIL_WORDS * sizeof(uint32_t)];
    } __attribute__((packed)) details;
} __attribute__((packed));

static uint32_t MyUCI;

void     cache_uci(void);
uint32_t get_uci(void);
uint64_t get_timebase(void);

void
cache_uci(void)
{
  struct device_node *np;
  int len;
  const uint32_t *prop;

  np = of_find_node_by_path("/personality/kernel");
  if (!np) MyUCI=1;
  else {
    prop = get_property(np, "uci", &len);
    if (prop == NULL || len != sizeof(MyUCI)) return;
    MyUCI = *prop;
  }
}

uint32_t
get_uci()
{
  if (MyUCI==0) cache_uci();
  return MyUCI;
}

int
write_ras_event(unsigned facility, unsigned unit, unsigned short errcode,
		unsigned numdetails, unsigned details[], unsigned send,
		struct ras_event *event)
{
    unsigned long flags;
    int i;
    unsigned int len;

    event->uci       = get_uci();
    event->facility  = facility;
    event->unit      = unit;
    event->errcode   = errcode;
    event->timestamp = get_tb();
    event->ecid[0]   = mfdcrx(ECID0);
    event->ecid[1]   = mfdcrx(ECID1);
    event->ecid[2]   = mfdcrx(ECID2);
    event->ecid[3]   = mfdcrx(ECID3);


    event->detailwords = numdetails;
    for (i = 0; i < numdetails; i++)
        event->details.words[i] = details[i];

    len = sizeof(struct ras_event) - sizeof(event->details) +
      numdetails * sizeof(uint32_t);

    if (send) {
      int i;

      printk("write_ras_event: sending: uci=0x%x fac=0x%x unit=0x%x "
	     "ec=0x%x ts=%llu ecid[0]=0x%x ecid[1]=0x%x, ecid[2]=0x%x, "
	     "ecid[3]=0x%x detailwords=%d\n",
	     event->uci, event->facility, event->unit, event->errcode,
	     event->timestamp, event->ecid[0], event->ecid[1],
	     event->ecid[2], event->ecid[3], event->detailwords);
      for (i=0; i<event->detailwords; i++) {
	printk("  detail[%d]=0x%x\n ", i, event->details.words[i]);
      }
#if 1
      // FIXME:  I have decided to simply use the console mailbox lock to
      //         to protect assess to the underlying mailbox.  This appears
      //         at first glance safe given our restricted code paths.
      spin_lock_irqsave(&mailbox_lock, flags);
      mailbox_write(smp_processor_id(), BGP_JMB_CMD2HOST_RAS, event, len);
      // FIXME:  JA:  Have added this explicit wait here to make sure that
      //         the ras message is sent prior to us going on and potentially
      //         writing more info to the mailbox either as console or RAS
      //         info.
      while ( !mailbox_test_completion(smp_processor_id()) ) { /* wait */ }
      spin_unlock_irqrestore(&mailbox_lock, flags);
#endif
    }
    return 0;
}

int
proc_do_bgras(ctl_table *ctl, int write, struct file *filp,
	      void *buffer, size_t *lenp, loff_t *ppos)
{
    char *page;
    int len, rc;
    unsigned fac, unit, ec, details;
    int rasval;
    struct ras_event event;

    page   = 0;
    rc     = 0;
    rasval = (int)ctl->extra1;

    fac     = (rasval & 0xff000000) >> 24;
    unit    = (rasval & 0x00ff0000) >> 16;
    ec      = (rasval & 0x0000ff00) >> 8;
    details = (rasval & 0x000000ff) >> 0;

    page = (char*)get_zeroed_page(GFP_KERNEL);
    if (!page) {
      rc=-ENOMEM;
      goto done;
    }

    if (write) {
      char *dp;
      uint32_t da[BGRAS_MAX_DETAIL_WORDS];
      unsigned nd=0;

      if (*lenp >= PAGE_SIZE) goto done;
      len=*lenp;

      if (copy_from_user(page, buffer, len)) {
	rc = -EFAULT;
	goto done;
      }

      // FIXME: JA: we assume a transaction--everything written at once
      if (len > 0 && details) {
	if (toupper(page[0]) == 'T') {
	  // FIXME: JA: add code to allow for text details to be sent
	  printk("proc_do_bgras: Text details not supported\n");
	} else {
	  for (dp=page,nd=0;
	       nd<BGRAS_MAX_DETAIL_WORDS && dp < page+PAGE_SIZE;
	       nd++) {
	    if (sscanf(dp, "%i", &da[nd])!=1) break;
	    while (dp < page+PAGE_SIZE && *dp != 0 && *dp != ' ') dp++;
	    while (dp < page+PAGE_SIZE && *dp == ' ') dp++;
	  }
	}
      }

      write_ras_event(fac, unit, ec,
		      nd,    // zero details
		      da,    // NULL detail array
		      1,     // SEND
		      &event);
    } else {
      write_ras_event(fac, unit, ec,
		      0,    // zero details
		      NULL, // NULL detail array
		      0,    // do not send
		      &event);

      len = snprintf(page, PAGE_SIZE,
		     "name=%s uci=0x%x fac=0x%x unit=0x%x "
		     "ec=0x%x ts=%llu ecid[0]=0x%x ecid[1]=0x%x,"
		     " ecid[2]=0x%x, ecid[3]=0x%x [requires details=%d]\n",
		     ctl->procname,
		     event.uci, event.facility, event.unit, event.errcode,
		     event.timestamp, event.ecid[0], event.ecid[1],
		     event.ecid[2], event.ecid[3], details);
      len -= *ppos;

      if (len < 0) len = 0;
      if (len >*lenp) len = *lenp;

      if (len) {
	if (copy_to_user(buffer,page,len)) {
	  rc=-EFAULT;
	  goto done;
	}
      }
      *lenp = len;
      *ppos += len;
    }
 done:
    if (page)
	free_page((unsigned long)page);

    return rc;
}


#define SYSCTL_RASENTRY(fac,unit,ec,details)                \
    {						            \
	.ctl_name	= CTL_UNNUMBERED,	            \
	.procname	= #ec,		                    \
	.mode		= 0644,			            \
	.proc_handler	= proc_do_bgras,	            \
	.extra1		= (void*)(fac<<24 | unit << 16 | ec << 8 | details),\
    }


static ctl_table bgras_ciod_root[] = {
    SYSCTL_RASENTRY(_bgp_fac_kernel,_bgp_unit_ciod,CiodInitialized,0),
    { .ctl_name = 0 }
};

static struct ctl_table bgras_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "bgras",
	.mode		= 0555,
	.child		= bgras_ciod_root,
    },
    { .ctl_name = 0 }
};

static struct ctl_table dev_root[] = {
    {
	.ctl_name	= CTL_DEV,
	.procname	= "dev",
	.maxlen		= 0,
	.mode		= 0555,
	.child		= bgras_root,
    },
    { .ctl_name = 0 }
};

static int bgras_proc_register(void)
{
  MyUCI = 0;
  (void) register_sysctl_table(dev_root);
  return 0;
}

static int __init bluegeneras_init(void)
{
    if (bgras_proc_register())
	panic("Could not register bgras in proc/sys filesystem");
    return 0;
}

// FIXME: JA this should be revaluated and potentially changed to be one
//        of the architecture specific init calls
module_init(bluegeneras_init);

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
