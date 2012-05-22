/* Project Kittyhawk:   Bluegene serial support */
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/termios.h>

#include "ppc450.h"

#include "link.h"
#include "tree.h"

// WARNING THIS REALLY BELONGS IN serial_core.h
#define BG_SERIAL_PORT_TYPE 77

#define BG_SERIAL_TYPE "bg_serial"
#define BG_SERIAL_MAX_PORTS 10 //256
#define BG_SERIAL_TREE_CHANNEL 0
#define BG_SERIAL_TREE_ROUTE 0xF
#define BG_SERIAL_FIF0_SIZE 240
// stealing IBM 3270 terminal UNIX tty access
#define BG_SERIAL_MAJOR 227

#if 0
#define TREE_HDR_BROADCAST	((union bgtree_header){ bcast: { pclass: BG_SERIAL_TREE_ROUTE, p2p: 0 }})
#define TREE_HDR_P2P(v)		((union bgtree_header){ p2p: { pclass: BG_SERIAL_TREE_ROUTE, p2p: 1, \
 						       vector: v }})
#endif

int bg_serial_tree_rcv_interrupt(struct sk_buff *skb, struct bglink_hdr_tree *lhdr);
static int bg_serial_tree_xmit_interrupt(int chn);

struct bg_serial_route {
  unsigned int send_id;
  unsigned int receive_id;
  union bgtree_header dest;
  int count;
};

#define BG_SERIAL_FIFO_SIZE (TREE_PAYLOAD - sizeof(struct bglink_hdr_tree))

struct bg_serial_port_info {
  struct bg_serial_route route;
};

static struct bg_serial {
  struct uart_driver uart_driver;
  
  struct bg_serial_port_info pinfo[BG_SERIAL_MAX_PORTS];
  struct uart_port  ports[BG_SERIAL_MAX_PORTS];  
  struct ctl_table_header *sysctl_header;
  struct bg_tree *bg_tree;
  struct bglink_proto bglink_proto;
  spinlock_t lock;
  struct uart_port * portsPendingTransmit; /* list of pending ports */ 
  int sndcnt;
  int rcvcnt;
} bg_serial = {
  .portsPendingTransmit = NULL, 
  .uart_driver =  {
    .owner = THIS_MODULE,
    .driver_name = "bg_serial",
    .dev_name    = "bgttyS",
    .major       = BG_SERIAL_MAJOR,
    .minor       = 0,
    .nr          = BG_SERIAL_MAX_PORTS
  },
  .bglink_proto =  {
    .lnk_proto = BGLINK_P_SERIAL,
    .receive_from_self = 0,
    .tree_rcv = bg_serial_tree_rcv_interrupt,
    .tree_flush = bg_serial_tree_xmit_interrupt,
    .torus_rcv = NULL
  },
  .sndcnt = 0,
  .rcvcnt = 0
};

#if 0 // 
static void bug_print(char *str){
  struct tty_struct *my_tty;
  my_tty = current->signal->tty;
  // assert we have a valid tty
  BUG_ON(my_tty == NULL);
  //  
  ((my_tty->driver)->write) (my_tty, str, strlen(str));	
  ((my_tty->driver)->write) (my_tty, 0, "\015\012", 2);
}
#endif


void 
bg_serial_sndcnt_inc(int inc)
{
  bg_serial.sndcnt += inc;
  return;
}

int
bg_serial_sndcnt(void)
{
  return  bg_serial.sndcnt;
}

void 
bg_serial_rcvcnt_inc(int inc)
{
  bg_serial.rcvcnt += inc;
  return;
}

int
bg_serial_rcvcnt(void)
{
  return bg_serial.rcvcnt;
}

static struct uart_port * bg_serial_getNextPending(struct uart_port *p) 
{
  return (struct uart_port *)p->private_data;
}

static void bg_serial_setNextPending(struct uart_port *p, struct uart_port *n)
{
  p->private_data = n;
}

// Add p to the enof the pendinglist
static void bg_serial_addPending(struct uart_port *p) 
{
  int flags;

  bg_serial_setNextPending(p, NULL);

  spin_lock_irqsave(&(bg_serial.lock), flags);
  if (bg_serial.portsPendingTransmit == NULL) bg_serial.portsPendingTransmit = p;
  else {
    struct uart_port *t;
    for (t = bg_serial.portsPendingTransmit; 
         bg_serial_getNextPending(t) != NULL;
         t = bg_serial_getNextPending(t)); 
    bg_serial_setNextPending(t,p);
  }
  spin_unlock_irqrestore(&(bg_serial.lock), flags);
  // enable watermark interrupt and exit 
  bgtree_enable_inj_wm_interrupt(bg_serial.bg_tree, BG_SERIAL_TREE_CHANNEL);
}

// reflecting return code of bgtree_xmit < 0 is error
static int 
_bg_serial_transmit(struct bg_serial_port_info *pi, char *s, int len)
{
  struct bglink_hdr_tree lnkhdr  __attribute__((aligned(16)));
  char fifo[BG_SERIAL_FIFO_SIZE]  __attribute__((aligned(16)));
  int rc;

  lnkhdr.dst_key = pi->route.send_id;
  lnkhdr.lnk_proto = BGLINK_P_SERIAL;
  bgtree_link_hdr_init(&lnkhdr);
  lnkhdr.opt.opt_con.len = len;
  memcpy(fifo, s, len);

  rc =  bgtree_xmit(bg_serial.bg_tree, BG_SERIAL_TREE_CHANNEL, pi->route.dest,
		    &lnkhdr, fifo, BG_SERIAL_FIF0_SIZE);
  return rc;
}

static int bg_serial_transmit(struct uart_port *port )
{
  struct circ_buf *xmit  = &port->info->xmit;
  struct bg_serial_port_info *pi = &(bg_serial.pinfo[port->line]);
  unsigned int len;
  unsigned int pending, t_pend, r_pend;

  /* xon/xoff char */
  if (port->x_char) {
    // handle xon/xoff software flow control as a high priority
    // single transmission
    if (_bg_serial_transmit(pi, &(port->x_char),1) < 0) {
      // since we noticed that the tree is busy, ensure that the the low-water
      // mark interrupt is enabled to ensure that our flush callback will be invoked
      // when the tree fifo has room.  We don't need to disable it as this is handled
      // automatically by the link code given that there maybe multiple protocols
      // using the tree.
      // either we are in an interrupt or are being called with port locked 
      /* return 0: no data sent */
      return 0;
    } else {
      port->x_char = 0;
      port->icount.tx++;
      /* return size sent */
      return 1;
    }
  } /* end of (port->x_char) */

  // return if uart is empty or stopped 
  if (uart_circ_empty(xmit) || uart_tx_stopped(port))
    return 0;

  pending = uart_circ_chars_pending(xmit);

  printk("%s: pending %d\n",__func__,pending);

  for(t_pend = 0; t_pend<pending; t_pend+=BG_SERIAL_FIFO_SIZE)
  {
    len = (pending < BG_SERIAL_FIFO_SIZE) ? pending : BG_SERIAL_FIFO_SIZE;
    printk("%s: t_pend %d; len %d; \n",__func__,t_pend, len);
    
    if (_bg_serial_transmit(pi, &(xmit->buf[xmit->tail]), len) < 0)  {
      /* return amount of data remaining to be sent */
      r_pend = pending - t_pend;
      xmit->tail = (xmit->tail + r_pend) & (UART_XMIT_SIZE - 1);
      port->icount.tx += r_pend;
      printk("%s: r_pend %d; len %d; \n",__func__,r_pend, len);
      return ((r_pend)*-1);
    } 
  }
  /* if send was successful, update tty queue and loop of nessessary */ 
  xmit->tail = (xmit->tail + pending) & (UART_XMIT_SIZE - 1);
  port->icount.tx += pending;

  /* wake up after send */
  //TODO: verify these lines are correct
  if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) uart_write_wakeup(port);

  return 1;
}

static int 
bg_serial_tree_xmit_interrupt(int chn)
{
  struct uart_port *new_list, *helper;
  int sent, flags;
  if( bg_serial.portsPendingTransmit == NULL )
      return 0;
  
  /* lock and swap full list for empty */  
  spin_lock_irqsave(&(bg_serial.lock), flags); 
  new_list = bg_serial.portsPendingTransmit;
  bg_serial.portsPendingTransmit = NULL;
  spin_unlock_irqrestore(&(bg_serial.lock), flags);

  while (new_list != NULL){
    if( (sent = bg_serial_transmit(new_list)) >  0 ){
      /* pop item from list */
      helper = new_list;
      new_list = (struct uart_port *)new_list->private_data; 
        /* if partial send, add remaning items to end of list */
        bg_serial_addPending(new_list); 
        /* partial send goes to very end of list */
        if(sent < 0) bg_serial_addPending(helper); 
          new_list = NULL;//exit loop
    
    }else
      new_list = (struct uart_port *)new_list->private_data; 
  }
  return 1; 
}

int
bg_serial_tree_rcv_interrupt(struct sk_buff *skb, struct bglink_hdr_tree *lhdr)
{
  char c = (skb->len && skb->data) ? skb->data[0] : '-';

  printk("%s: lhdr.optlen=%d skb->len=%d skb->data[0]=%c\n",__func__,
	 lhdr->opt.opt_con.len, skb->len, c);

  return 0;
}

int 
bg_serial_tree_protocol_init(void)
{
  bglink_register_proto(&(bg_serial.bglink_proto));
  return 0;
}

/*********************************/

static unsigned int	
bg_serial_tx_empty(struct uart_port *p) 
{
  unsigned long flags;
  unsigned int rc;

  printk("%s: %d\n",__func__,p->line);

  spin_lock_irqsave(&(p->lock), flags);
  //  do soemthing here
  rc = TIOCSER_TEMT;
  spin_unlock_irqrestore(&(p->lock), flags);

  return rc;
}

static void		
bg_serial_set_mctrl(struct uart_port *p, unsigned int mctrl)
{
  // WE DON'T SUPPORT CHANGES 
  printk("%s: %d\n",__func__,p->line);
  return;
}

static unsigned int 
bg_serial_get_mctrl(struct uart_port *p) 
{
  printk("%s: %d\n",__func__,p->line);
  return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
  return 0;
}

static void 
bg_serial_stop_tx(struct uart_port *p)
{
  printk("%s: %d\n",__func__,p->line);
  return;
}

static void 
bg_serial_start_tx(struct uart_port *p)
{
  printk("trace: %s \n",__func__);
  /* ad port to transmit queue is send was incomplete or unsuccessful */
  if( bg_serial_transmit(p) <= 0) bg_serial_addPending(p); 
  return;
}

static void 
bg_serial_stop_rx(struct uart_port *p)
{
  printk("%s: %d\n",__func__,p->line);
  return;
}

static void
bg_serial_enable_ms(struct uart_port *p)
{
  printk("%s: %d\n",__func__,p->line);
  return;
}

static void
bg_serial_break_ctl(struct uart_port *p, int ctl)
{
  printk("%s: %d\n",__func__,p->line);
  return;
}

static int
bg_serial_startup(struct uart_port *p)
{
  /* do port setup here */
  //  printk("%s: %d\n",__func__, p->line);
  return 0;
}

static void
bg_serial_shutdown(struct uart_port *p)
{
  /* do tear down here */
  printk("%s: %d\n",__func__,p->line);
  return;
}

static void
bg_serial_set_termios(struct uart_port *p, struct ktermios *new,
	    struct ktermios *old)
{
  printk("%s: %d\n",__func__,p->line);  
  /*
  set_termios(port,termios,oldtermios)
	Change the port parameters, including word length, parity, stop
	bits.  Update read_status_mask and ignore_status_mask to indicate
	the types of events we are interested in receiving.  Relevant
	termios->c_cflag bits are:
		CSIZE	- word size
		CSTOPB	- 2 stop bits
		PARENB	- parity enable
		PARODD	- odd parity (when PARENB is in force)
		CREAD	- enable reception of characters (if not set,
			  still receive characters from the port, but
			  throw them away.
		CRTSCTS	- if set, enable CTS status change reporting
		CLOCAL	- if not set, enable modem status change
			  reporting.
	Relevant termios->c_iflag bits are:
		INPCK	- enable frame and parity error events to be
			  passed to the TTY layer.
		BRKINT
		PARMRK	- both of these enable break events to be
			  passed to the TTY layer.

		IGNPAR	- ignore parity and framing errors
		IGNBRK	- ignore break errors,  If IGNPAR is also
			  set, ignore overrun errors as well.
	The interaction of the iflag bits is as follows (parity error
	given as an example):
	Parity error	INPCK	IGNPAR
	n/a		0	n/a	character received, marked as
					TTY_NORMAL
	None		1	n/a	character received, marked as
					TTY_NORMAL
	Yes		1	0	character received, marked as
					TTY_PARITY
	Yes		1	1	character discarded

	Other flags may be used (eg, xon/xoff characters) if your
	hardware supports hardware "soft" flow control.

	Locking: none.
	Interrupts: caller dependent.
	This call must not sleep
  */
  return;
}


/*
 * Return a string describing the type of the port
 */
static const char *
bg_serial_type(struct uart_port *p)
{
  //  printk("%s: %d\n",__func__,p->line);
  return p->type == BG_SERIAL_PORT_TYPE ? BG_SERIAL_TYPE : NULL;
}

/*
 * Release IO and memory resources used by the port.
 * This includes iounmap if necessary.
 */
static void
bg_serial_release_port(struct uart_port *p)
{
  printk("%s: %d\n",__func__,p->line);
  return;
}

/*
 * Request IO and memory resources used by the port.
 * This includes iomapping the port if necessary.
 */
static int		
bg_serial_request_port(struct uart_port *p)
{
  //  printk("%s: %d\n",__func__,p->line);
  return 0;
}

static void
bg_serial_config_port(struct uart_port *p, int flags)
{
  //  printk("%s: %d\n",__func__,p->line);
  if (bg_serial_request_port(p)==0)  p->type = BG_SERIAL_PORT_TYPE;
  return;
}

static int 
bg_serial_verify_port(struct uart_port *p, struct serial_struct *s)
{
  /* turn off core coe modification of port parms by returning -EINVAL */
  return -EINVAL;
}

struct uart_ops  bg_serial_ops = {
  .tx_empty = bg_serial_tx_empty,
  .set_mctrl = bg_serial_set_mctrl,
  .get_mctrl = bg_serial_get_mctrl,
  .stop_tx = bg_serial_stop_tx,
  .start_tx = bg_serial_start_tx,
  // we do not have any custom xon/xoff behaviour so no override .send_xchar = bg_serial_send_xchar,
  .stop_rx = bg_serial_stop_rx,
  .enable_ms = bg_serial_enable_ms,
  .break_ctl = bg_serial_break_ctl,
  .startup = bg_serial_startup,
  .shutdown = bg_serial_shutdown,
  .set_termios = bg_serial_set_termios,
  // no additional power management behaviour so no overide .pm = bg_serial_pm,
  .type = bg_serial_type,
  .release_port = bg_serial_release_port,
  .request_port = bg_serial_request_port,
  .config_port = bg_serial_config_port,
  .verify_port = bg_serial_verify_port,
  // no ioctls so we do not set: .ioctl = bg_serial_ioctl
};


static int
bg_serial_setup_ports(void)
{
  int i;
  struct uart_port *port;
  struct bg_serial_port_info *pi; 

  printk("%s\n",__func__);
  memset(bg_serial.pinfo, 0, sizeof(bg_serial.pinfo));
  memset(bg_serial.ports, 0, sizeof(bg_serial.ports));
  for (i=0; i<BG_SERIAL_MAX_PORTS; i++) {
    pi = &(bg_serial.pinfo[i]);
    pi->route.send_id = i;
    pi->route.receive_id = i;
    
    // we assume pinfo has been written to zero
    // since we are in broadcast, no further updates needed
    pi->route.dest.bcast.pclass = BG_SERIAL_TREE_ROUTE;

    port = &(bg_serial.ports[i]);
    port->fifosize  = BG_SERIAL_FIFO_SIZE;
    port->iotype    = UPIO_MEM;
    port->iobase    = 1; /* bogus non-zero value to mark port in use */
    port->mapbase   = 1; /* bogus non-zero value to mark port in use */ 
    port->membase   = NULL;
    port->ops       = &bg_serial_ops;
    port->irq       = 1; /* bogus non-zero value */
    port->flags     = UPF_FIXED_PORT | UPF_BOOT_AUTOCONF;
    port->line      = i;
    uart_add_one_port(&bg_serial.uart_driver, port);
  }
  return 0;
}

extern void mailbox_puts(const unsigned char *s, int count);

static int bg_serial_proc_register(struct ctl_table_header **sysctl_header);
static int bg_serial_proc_unregister(struct ctl_table_header **sysctl_header);

static int __init bg_serial_init(void)
{
  int rc;

  printk(KERN_INFO "Serial: bg serial driver init\n");

  spin_lock_init(&(bg_serial.lock));
  
  bg_serial.bg_tree = bgtree_get_dev();

  rc = uart_register_driver(&bg_serial.uart_driver);

  if (rc) return rc;
 
  rc = bg_serial_setup_ports();
 
  if (rc) uart_unregister_driver(&bg_serial.uart_driver);

  (void)bg_serial_proc_register(&(bg_serial.sysctl_header));

  mailbox_puts("bg_serial_init: before proto init call\n", 40);  
  (void)bg_serial_tree_protocol_init();

  return rc;
}

static void __exit bg_serial_exit(void)
{
  printk(KERN_INFO "Serial: bg serial driver exit\n");
  uart_unregister_driver(&bg_serial.uart_driver);
  bg_serial_proc_unregister(&bg_serial.sysctl_header);
}

module_init(bg_serial_init);
module_exit(bg_serial_exit);

MODULE_AUTHOR("Project Kittyhawk");
MODULE_DESCRIPTION("Kittyhawk Bluegene serial driver");
MODULE_LICENSE("GPL");

#if defined(CONFIG_SYSCTL) && defined(CONFIG_PROC_FS)

static int proc_do_bg_serial (ctl_table *ctl, int write, struct file * filp,
			   void __user *buffer, size_t *lenp, loff_t *ppos)
{
    char *page = (char*)get_zeroed_page(GFP_KERNEL);
    char *p;
    struct bg_serial_port_info *info = (struct bg_serial_port_info *)
      ctl->extra1;
    int idx = (int)ctl->extra2;
    int len, rc = 0, validx;

    BUG_ON(!info || idx < 0 || idx >= BG_SERIAL_MAX_PORTS);

    if (!page)
	return -ENOMEM;

    if (write) {
	int val[3];

	if (*lenp >= PAGE_SIZE || copy_from_user(page, buffer, *lenp)) {
	    rc = -EFAULT;
	    goto out;
	}
	len = *lenp;
	p = page;
	p[len] = 0;
	validx = 0;
	while(len > 0 && validx < 3) {
	    if (*p == ' ') {
		p++; len--;
		continue;
	    }
	    if (validx == 2 && (*p == 'b' || *p == 'm')) {
		val[validx++] = -1;
		break;
	    }
	    if (*p < '0' || *p > '9')
		break;
	    val[validx++] = simple_strtoul(p, &p, 0);
	    len = (page + *lenp) - p;
	}
	*ppos += *lenp;
	if (validx == 3) {
	    if (info->route.count > 0) {
		rc = -EBUSY;
		goto out;
	    }
	    info->route.send_id = val[0];
	    info->route.receive_id = val[1];
	
	    if (val[2] == -1){
	      info->route.dest.bcast.pclass = BG_SERIAL_TREE_ROUTE;
	      info->route.dest.bcast.p2p = 0;
	    }
	    else{
	      info->route.dest.p2p.pclass = BG_SERIAL_TREE_ROUTE;
	      info->route.dest.p2p.p2p = 1;
	      info->route.dest.p2p.vector = val[2];
  
	    }
	       
	}
    } else {
	len = sprintf(page, "%d %d ", info->route.send_id,
		      info->route.receive_id);
	if (info->route.dest.bcast.p2p == 0)
	    len += sprintf(page + len, "broadcast\n");
	else
	    len += sprintf(page + len, "%d\n",
			   info->route.dest.p2p.vector);
	len -= *ppos;
	if (len < 0)
	    len = 0;
	if (len > *lenp)
	    len = *lenp;
	if (len && copy_to_user(buffer, page + *ppos, len)) {
		rc = -EFAULT;
		goto out;
	}
	*lenp = len;
	*ppos += len;
    }

 out:
    free_page((unsigned long)page);
    return rc;
}

static struct ctl_table bg_serial_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "bg_serial",
	.mode		= 0555,
    },
    { .ctl_name = 0 }
};

static struct ctl_table dev_root[] = {
    {
	.ctl_name	= CTL_DEV,
	.procname	= "dev",
	.maxlen		= 0,
	.mode		= 0555,
	.child		= bg_serial_root,
    },
    { .ctl_name = 0 }
};

static int bg_serial_proc_register(struct ctl_table_header **sysctl_header)
{
    int idx;
    struct ctl_table *ent;

    ent = (struct ctl_table*)kzalloc(sizeof(struct ctl_table) *
				     (BG_SERIAL_MAX_PORTS + 1), GFP_KERNEL);
    if (!ent)
	return -ENOMEM;

    bg_serial_root[0].child = ent;

    for (idx = 0; idx < BG_SERIAL_MAX_PORTS; idx++)
    {
	ent[idx].ctl_name = CTL_UNNUMBERED;
	ent[idx].procname = kmalloc(5, GFP_KERNEL);
	BUG_ON(!ent[idx].procname);
	sprintf((char*)ent[idx].procname, "%d", idx);
	ent[idx].mode = 0644;
	ent[idx].proc_handler = proc_do_bg_serial;
	ent[idx].extra1 = (void*)&(bg_serial.pinfo[idx]);
	ent[idx].extra2 = (void*)idx;
    }

    *sysctl_header  = register_sysctl_table(dev_root);
    return 0;
}

static int bg_serial_proc_unregister(struct ctl_table_header **sysctl_header)
{
    if (*sysctl_header) {
	unregister_sysctl_table(*sysctl_header);
	*sysctl_header = NULL;
    }

    return 0;
}

#else
static int bg_serial_proc_register(struct ctl_table_header **sysctl_header) { return 0; }
static int bg_serial_proc_unregister(struct ctl_table_header **sysctl_header) { return 0; }
#endif
