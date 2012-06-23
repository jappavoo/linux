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
#define BG_SERIAL_MAX_PORT 0xFF
#define BG_SERIAL_TREE_CHANNEL 0
#define BG_SERIAL_TREE_ROUTE 0xF
#define BG_SERIAL_FIF0_SIZE 240
// stealing IBM 3270 terminal UNIX tty access
#define BG_SERIAL_MAJOR 227

extern void mailbox_puts(const unsigned char *s, int count);

int bg_serial_tree_rcv_interrupt(struct sk_buff *skb, struct bglink_hdr_tree *lhdr);

static void bg_serial_stop_tx(struct uart_port *p);
static int bg_serial_tree_xmit_interrupt(int chn);

struct bg_serial_route {
  unsigned int send_id;
  unsigned int receive_id;
  union bgtree_header dest;
  int count;
};

//#define VERBOSE
//#define DEBUG 
//#define MAILBOX

#ifdef VERBOSE
#define VERBOSE_PRINTK(...) printk(__VA_ARGS__)
#else
#define VERBOSE_PRINTK(...)
#endif

#ifdef  DEBUG
#define DEBUG_PRINTK(...) printk(__VA_ARGS__)
#else
#define DEBUG_PRINTK(...)
#endif

#ifdef MAILBOX
#define MAILBOX_PRINTK(...) mailbox_puts(__VA_ARGS__)
#else
#define MAILBOX_PRINTK(...)
#endif 


#define BG_SERIAL_FIFO_SIZE (TREE_PAYLOAD - sizeof(struct bglink_hdr_tree))

struct bg_serial_port_info {
  struct bg_serial_route route;
  struct bg_serial_port_info *next;
  int opencnt;
};

static struct bg_serial {
  struct uart_driver uart_driver;
  
  struct bg_serial_port_info pinfo[BG_SERIAL_MAX_PORT+1];
  struct bg_serial_port_info *openports[BG_SERIAL_MAX_PORT+1];
  struct uart_port  ports[BG_SERIAL_MAX_PORT+1];  
  struct ctl_table_header *sysctl_header;
  struct bg_tree *bg_tree;
  struct bglink_proto bglink_proto;
  spinlock_t lock;
  struct pending {
    struct uart_port *head;
    struct uart_port *tail;
  } pending; /* list of pending ports */ 
  int sndcnt;
  int rcvcnt;
} bg_serial = {
  .pending = { 
    .head = NULL, 
    .tail = NULL
  }, 
  .uart_driver =  {
    .owner = THIS_MODULE,
    .driver_name = "bg_serial",
    .dev_name    = "bgttyS",
    .major       = BG_SERIAL_MAJOR,
    .minor       = 0,
    .nr          = (BG_SERIAL_MAX_PORT + 1),
    // .cons        = NULL,
    .state       = NULL,
    .tty_driver  = NULL
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


void 
bg_serial_sndcnt_inc(int inc)
{
  int flags;
  char buf[160];
  spin_lock_irqsave(&(bg_serial.lock), flags); 
  //VERBOSE_PRINTK("! %s: inc:%d sndcnt:%d",__func__,inc, bg_serial.sndcnt);
  bg_serial.sndcnt += inc;
 
  sprintf(buf, "%s: sndcnt=%d\n", __func__, bg_serial.sndcnt);
  printk(buf);
  MAILBOX_PRINTK(buf, strlen(buf));
  VERBOSE_PRINTK("%s: inc:%d sndcnt:%d",__func__,inc, bg_serial.sndcnt);
  spin_unlock_irqrestore(&(bg_serial.lock), flags); 
  return;
}

int
bg_serial_sndcnt(void)
{
  return  bg_serial.sndcnt;
}

extern void bgtree_receive_interrupt_skip_toggle(void);

void 
bg_serial_rcvcnt_inc(int inc)
{
  int flags;
  char buf[160];
  spin_lock_irqsave(&(bg_serial.lock), flags);
  bg_serial.rcvcnt += inc;
  sprintf(buf, "%s: rcvcnt={%d}\n", __func__, bg_serial.rcvcnt);
  MAILBOX_PRINTK(buf, strlen(buf));
  VERBOSE_PRINTK("%s: inc:%d rcvcnt:%d\n",__func__,inc, bg_serial.rcvcnt);
  /*
  if (bg_serial.rcvcnt == 4096) {
    bgtree_receive_interrupt_skip_toggle();
    mailbox_puts("PAUSED: TREE RCV\n",17);
    for (i=0; i<10000000; i++);
    bgtree_receive_interrupt_skip_toggle();
    mailbox_puts("STARTED: TREE RCV\n",18);
  }
  */
  spin_unlock_irqrestore(&(bg_serial.lock), flags);
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

// Add list pointed to by p to  the end of the pendinglist assumes last link of
// assumes last link on p has a next pointer of NULL
static void bg_serial_addPending(struct uart_port *p) 
{
  int flags;

  spin_lock_irqsave(&(bg_serial.lock), flags);

    if (bg_serial.pending.head == NULL) {
      bg_serial.pending.head = bg_serial.pending.tail = p;
    } else {
      bg_serial_setNextPending(bg_serial.pending.tail, p);
    }
    while (bg_serial_getNextPending(bg_serial.pending.tail) != NULL) {
      bg_serial.pending.tail = bg_serial_getNextPending(bg_serial.pending.tail);
    }

  spin_unlock_irqrestore(&(bg_serial.lock), flags);

  // enable watermark interrupt and exit 
  bgtree_enable_inj_wm_interrupt(bg_serial.bg_tree, BG_SERIAL_TREE_CHANNEL);
}

// reflecting return code of bgtree_xmit < 0 is error
static int _bg_serial_transmit(struct bg_serial_port_info *pi, char *s, int len)
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

/* returns 1 on success, 0 on failure - no data sent,  -1 on failure - partial data sent */
/* LOCKING: should be called with port lock acquired */
static int bg_serial_transmit(struct uart_port *port)
{
  struct circ_buf *xmit  = &port->info->xmit; 
  struct bg_serial_port_info *pi = &(bg_serial.pinfo[port->line]);
  unsigned int total, remaining, len, status; 
  char buf[160];
  
  /* xon/xoff char */
  if (port->x_char) {
    // BUG_ON(1);
    // handle xon/xoff software flow control as a high priority
    // single transmission
    if (_bg_serial_transmit(pi, &(port->x_char), 1) < 0) {
      // since we noticed that the tree is busy, ensurxcthat the the low-water
      // mark interrupt is enabled to ensure that our flush callback will be invoked
      // when the tree fifo has room.  We don't need to disable it as this is handled
      // automatically by the link code given that there maybe multiple protocols
      // using the tree.
      return 0;
    } else { 
      mailbox_puts("bg_serial_transmit: X_CHAR SENT \n",34);
      port->x_char = 0;
      port->icount.tx++;
      return 1;
    }
  } /* end of (port->x_char) */

  // return if uart is empty or stopped 
  if (uart_circ_empty(xmit) || uart_tx_stopped(port)){
      mailbox_puts("bg_serial_transmit: UART EMPTY / STOPPED\n",42);
      bg_serial_stop_tx(port);
    return 0; 
  }

  // continue sending blocks of FIFO_SIZE until there is nothing left to send
  remaining = total = uart_circ_chars_pending(xmit);
  while (remaining > 0) {
    len = ( remaining < BG_SERIAL_FIFO_SIZE ) ? remaining : BG_SERIAL_FIFO_SIZE;    
    status = _bg_serial_transmit(pi, &(xmit->buf[xmit->tail]), len);
   
    // if send failed, process and return
    if ( status < 0)  {
      DEBUG_PRINTK("%s transmit failed;  attempted:%d remaining:%d \n",__func__,total,remaining);
      sprintf(buf, "%s: transmit failed:  attemted:%d sndcnt:%d \n", __func__, bg_serial.sndcnt, bg_serial.sndcnt);
      MAILBOX_PRINTK(buf, strlen(buf));
     
      // if it was a partial send
      if(total - remaining){
	xmit->tail = (xmit->tail + (total-remaining)) & (UART_XMIT_SIZE - 1);
	port->icount.tx += (total-remaining);
	bg_serial_sndcnt_inc(total-remaining);
	return -1;
      }
      // return 0 is no data was sent
      return 0; 
    } 
    DEBUG_PRINTK("%s sent=%d sndcnt=%d \n",__func__, len, bg_serial.sndcnt);
    sprintf(buf, "%s: sent=%d sndcnt=%d \n", __func__, len, bg_serial.sndcnt );
    MAILBOX_PRINTK(buf, strlen(buf));

    /* record sent data  */
    remaining -= len;
    xmit->tail = (xmit->tail + len) & (UART_XMIT_SIZE - 1);
    port->icount.tx += len;
    bg_serial_sndcnt_inc(len);

  }/* end of while loop */

  /* wake up after send */
  /* number of characters left in xmit buffer is less than WAKEUP_CHARS then we ask for more */
  if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) uart_write_wakeup(port);
    DEBUG_PRINTK("%s sent successful tot=%d rem=%d sndcnt=%d \n",__func__,t, r, bg_serial.sndcnt);
  return 1;
}

static int 
bg_serial_tree_xmit_interrupt(int chn)
{
  struct uart_port *p,*t, *tail;
  int flags, rc;
  char buf[160];
  
  sprintf(buf, "%s: sndcnt=%d chn=%d\n", __func__, bg_serial.sndcnt, chn );
  MAILBOX_PRINTK(buf, strlen(buf));
  DEBUG_PRINTK("%s sndcnt=%d chn=%d \n",__func__, bg_serial.sndcnt, chn );
  
  /* lock and swap full list for empty */  
  spin_lock_irqsave(&(bg_serial.lock), flags); 
  
  /* unlock & return if there is nothing to send */
  if( bg_serial.pending.head == NULL ) {
  sprintf(buf, "%s: NOTHING TO SEND sndcnt=%d chn=%d\n", __func__, bg_serial.sndcnt, chn );
  MAILBOX_PRINTK(buf, strlen(buf));
  DEBUG_PRINTK("%s NOTHING TO SEND chn=%d \n",__func__, chn);

      spin_unlock_irqrestore(&(bg_serial.lock), flags);
      return 0;
  }
  p = bg_serial.pending.head;
  tail = bg_serial.pending.tail;
  bg_serial.pending.head = NULL;
  bg_serial.pending.tail = NULL;
  
  spin_unlock_irqrestore(&(bg_serial.lock), flags);

  // walk ports that were on the pending list
  // and attempt to drain them
  while (p != NULL) { 
    spin_lock_irqsave(&(p->lock),flags);
      rc = bg_serial_transmit(p);
      if (rc == 1) {
	// sucessfully send p's data
	t = p;
        p = bg_serial_getNextPending(p);
        bg_serial_setNextPending(t, NULL);
      } else { /* partial or no data sent */
        spin_unlock_irqrestore(&(p->lock),flags);
        break;
      }
    spin_unlock_irqrestore(&(p->lock),flags);
  }

  /* if we break loop while there are still ports waiting*/
  if (p) {
    spin_lock_irqsave(&(bg_serial.lock), flags);
    // restructure pending list
    t = bg_serial.pending.head;
    bg_serial.pending.head = p;
    bg_serial_setNextPending(tail, t);
    if (bg_serial.pending.tail == NULL) {
        bg_serial.pending.tail = tail;
    }
  sprintf(buf, "%s: FAILED TO SEND ALL DATA sndcnt=%d chn=%d\n", __func__, bg_serial.sndcnt, chn );
  MAILBOX_PRINTK(buf, strlen(buf));
  DEBUG_PRINTK("%s FAILED TO SEND ALL DATA chn=%d \n",__func__, chn);

    spin_unlock_irqrestore(&(bg_serial.lock), flags);
    return 1; /* unable to empty queue */
  }

  sprintf(buf, "%s: SUCCESS sndcnt=%d chn=%d\n", __func__, bg_serial.sndcnt, chn );
  MAILBOX_PRINTK(buf, strlen(buf));
  DEBUG_PRINTK("%s SUCCESS chn=%d \n",__func__, chn);

  return 0; 
}

struct uart_port *
bg_serial_find_openport(u32 destkey)
{
  struct bg_serial_port_info *pinfo;
  /* create a hash of our destkey */
  int hk = destkey & BG_SERIAL_MAX_PORT;

  /* attempt to write char to connected ttys */
  pinfo = bg_serial.openports[hk];

  if (pinfo == NULL) return NULL;
  
  for (;pinfo!=NULL;pinfo = pinfo->next) {
    if (pinfo->route.receive_id == destkey) 
      return &(bg_serial.ports[ pinfo - bg_serial.pinfo ]);
  }

  {
    char buf[160];
    sprintf(buf, "ERROR: %s: no open port found for destkey=%x\n",
	    __func__, destkey);
    MAILBOX_PRINTK(buf, strlen(buf));
    VERBOSE_PRINTK(buf);
  }

  return NULL;
}


int

bg_serial_tree_rcv_interrupt(struct sk_buff *skb, struct bglink_hdr_tree *lhdr)
{
  struct uart_port *p;
  int len; 
  char buf[160];
  
  // HOT-PATH: We expect that we will often times be having to process
  // serial data that is either not destined for one of our ttys or that
  // our tty that it is destined for is not open
  if ((p = bg_serial_find_openport(lhdr->dst_key)) == NULL) {
#if 0
    int i;
    DEBUG_PRINTK("%s: dropping data: dst_key=%08x(%d) src_key=%08x(%d) %d\n",
	   __func__, 
	   lhdr->dst_key, lhdr->dst_key,
	   lhdr->src_key, lhdr->src_key,
	   lhdr->opt.opt_con.len);
    for (i=0; i<lhdr->opt.opt_con.len; i++) {
      DEBUG_PRINTK("%02x (%c) ", skb->data[i], skb->data[i]);
      if (i % 16 == 0) printk("\n");
    }
    DEBUG_PRINTK("\n");
#endif
    dev_kfree_skb(skb); 
    return 0;
  }

  // looks like we actually have a tty for whom the data should be delivered
  // too
  BUG_ON(!p->info || !p->info->tty || p->info->tty->count == 0);

  len = lhdr->opt.opt_con.len; 
  skb_trim(skb, len);  
  //  bg_serial_rcvcnt_inc(len);

  DEBUG_PRINTK("%s recived len=%d, trim(skb->len)=%d rcvcnt=%d \n",__func__, len, skb->len, bg_serial.rcvcnt);  

  len = tty_insert_flip_string(p->info->tty, skb->data, skb->len);
  sprintf(buf, "%s: TTY READ atmpt_snd:%d, actl_snd:%d rcvcnt:%d TTYflag:%lX, TTYbuf:%d \n", __func__, lhdr->opt.opt_con.len, len, bg_serial.rcvcnt, p->info->tty->flags, p->info->tty->receive_room);
  MAILBOX_PRINTK(buf, strlen(buf));
  bg_serial_rcvcnt_inc(len);

  if(lhdr->opt.opt_con.len != len)
      DEBUG_PRINTK("%s Incorrect data amount sent  len_orig=%d, len_tty=%d \n",__func__, lhdr->opt.opt_con.len, len);  

  tty_flip_buffer_push(p->info->tty);
  dev_kfree_skb(skb);
  

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

  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);

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
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return;
}

static unsigned int 
bg_serial_get_mctrl(struct uart_port *p) 
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void 
bg_serial_stop_tx(struct uart_port *p)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  // set a flag to prevent writes?  
  char buf[50];
  sprintf(buf, "%s: sendcnt:%d\n",__func__,bg_serial.sndcnt);
  // set a flag to prevent writes
  mailbox_puts(buf, strlen(buf));
  return;
}

static void 
bg_serial_start_tx(struct uart_port *p)
{
  DEBUG_PRINTK("trace: %s sndcnt=%d \n",__func__, bg_serial.sndcnt);
  /* add port to transmit queue is send was incomplete or unsuccessful */
  if( bg_serial_transmit(p) <= 0) {
    BUG_ON(bg_serial_getNextPending(p) != NULL);
    // add port to end of send list
    // If a flow control (x_char) was unsuccessful, we may
    // not want to put it at the END of the send queue.
    bg_serial_addPending(p); 
  }
  return;
}

static void 
bg_serial_stop_rx(struct uart_port *p)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  mailbox_puts("bg_serial_stop_rx \n",20);
  return;
}

static void
bg_serial_enable_ms(struct uart_port *p)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return;
}

static void
bg_serial_break_ctl(struct uart_port *p, int ctl)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return;
}

static int
bg_serial_startup(struct uart_port *p)
{
  /* record this port as being open in the open ports array */
  int i = p->line;
  struct bg_serial_port_info *pi = &(bg_serial.pinfo[i]); 
  int hk = pi->route.receive_id & BG_SERIAL_MAX_PORT; 

  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);

  BUG_ON(pi->next != NULL);
  pi->next =  bg_serial.openports[hk];
  pi->opencnt++;
  BUG_ON(pi->opencnt <= 0);
  bg_serial.openports[hk] = pi; 

  return 0; 
}

static void
bg_serial_shutdown(struct uart_port *p)
{
  /* record this port as being open in the open ports array */
  int i = p->line;
  struct bg_serial_port_info *t, *pi = &(bg_serial.pinfo[i]); 
  int hk = pi->route.receive_id & BG_SERIAL_MAX_PORT; 

  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);

  t=bg_serial.openports[hk];

  BUG_ON(t == NULL);

  if (t == pi) { 
    bg_serial.openports[hk] = t->next; 
  } else {
    for ( ;t && t->next != pi; t = t->next);
    BUG_ON(t == NULL || t->next != pi);
    t->next = t->next->next;
  }
  pi->opencnt--;
  BUG_ON(pi->opencnt < 0);
  pi->next = NULL;
  return;
}

static void
bg_serial_set_termios(struct uart_port *p, struct ktermios *new,
	    struct ktermios *old)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);  
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
  VERBOSE_PRINTK("%s: {%d} %s \n",__func__,p->line, BG_SERIAL_TYPE);
  return (p->type == BG_SERIAL_PORT_TYPE) ? BG_SERIAL_TYPE : NULL;
}

/*
 * Release IO and memory resources used by the port.
 * This includes iounmap if necessary.
 */
static void
bg_serial_release_port(struct uart_port *p)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return;
}

/*
 * Request IO and memory resources used by the port.
 * This includes iomapping the port if necessary.
 */
static int		
bg_serial_request_port(struct uart_port *p)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  return 0;
}

static void
bg_serial_config_port(struct uart_port *p, int flags)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
  if (bg_serial_request_port(p)==0)  p->type = BG_SERIAL_PORT_TYPE;
  return;
}

static int 
bg_serial_verify_port(struct uart_port *p, struct serial_struct *s)
{
  VERBOSE_PRINTK("%s: %d\n",__func__,p->line);
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


static void
bg_serial_setup_ports(void)
{
  int i;
  struct uart_port *port;
  struct bg_serial_port_info *pi; 

  VERBOSE_PRINTK("%s\n",__func__);
  memset(bg_serial.pinfo, 0, sizeof(bg_serial.pinfo));
  memset(bg_serial.ports, 0, sizeof(bg_serial.ports));
  memset(bg_serial.openports, 0, sizeof(bg_serial.openports));

  for (i=0; i<=BG_SERIAL_MAX_PORT; i++) {
    pi = &(bg_serial.pinfo[i]);
    pi->route.send_id = i;
    pi->route.receive_id = i;
    // we assume pinfo has been written to zero
    // since we are in broadcast, no further updates needed
    pi->route.dest.bcast.pclass = BG_SERIAL_TREE_ROUTE;

    port = &(bg_serial.ports[i]);
    port->fifosize  = BG_SERIAL_FIFO_SIZE;
    port->iotype    = UPIO_PORT;
    port->iobase    = i+1; /* bogus non-zero value to mark port in use */
    port->mapbase   = i+1; /* bogus non-zero value to mark port in use */ 
    port->line      = i;
    port->membase   = NULL;
    port->ops       = &bg_serial_ops;
    port->irq       = 1; /* bogus non-zero value */
    port->flags     = UPF_FIXED_PORT  |  UPF_BOOT_AUTOCONF;

    if  (uart_add_one_port(&bg_serial.uart_driver, port)<0) {
      char buf[160];
      sprintf(buf, "%s: add failed: i=%d\n", __func__, i);
      MAILBOX_PRINTK(buf, strlen(buf));  
    }
  }
}

static int bg_serial_proc_register(struct ctl_table_header **sysctl_header);
static int bg_serial_proc_unregister(struct ctl_table_header **sysctl_header);

static int __init bg_serial_init(void)
{
  int rc;

  printk(KERN_INFO "Serial: bg serial driver init\n");

  spin_lock_init(&(bg_serial.lock));
  
  bg_serial.bg_tree = bgtree_get_dev();

  rc = uart_register_driver(&bg_serial.uart_driver);

  if (rc) {
    char buf[160];
    sprintf(buf, "%s: register failed: rc=%d\n", __func__, rc);
    MAILBOX_PRINTK(buf, strlen(buf));  
    return rc;
  }

  bg_serial_setup_ports();
 
  (void)bg_serial_proc_register(&(bg_serial.sysctl_header));

  (void)bg_serial_tree_protocol_init();

  return rc;
}

static void __exit bg_serial_exit(void)
{
  DEBUG_PRINTK(KERN_INFO "Serial: bg serial driver exit\n");
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
    int len, rc = 0, validx, flags;

    BUG_ON(!info || idx < 0 || idx > BG_SERIAL_MAX_PORT);

    if (!page)
	return -ENOMEM;

    if (write) {
	int val[3];

	/* check if this port is open */ 
	spin_lock_irqsave(&(bg_serial.ports[idx].lock), flags);  
	if (info->opencnt) {
	  rc = -EBUSY;
	  goto out;
	}

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
    if (write) spin_unlock_irqrestore(&(bg_serial.ports[idx].lock), flags);  
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
				     (BG_SERIAL_MAX_PORT + 2), GFP_KERNEL);
    if (!ent)
	return -ENOMEM;

    bg_serial_root[0].child = ent;

    for (idx = 0; idx <= BG_SERIAL_MAX_PORT; idx++)
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
