/*********************************************************************
 *
 * Copyright (C) 2007-2008, Volkmar Uhlig, IBM Corporation
 *
 * Description:   Console Driver over BG networks
 *
 * All rights reserved
 *
 ********************************************************************/

#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/bootmem.h>

#include <asm/of_platform.h>

#include "link.h"
#include "tree.h"

#define CON_TTY_NAME		"bgtty"

#define CON_TTY_MAJOR		230
#define CON_TTY_MINOR		0
#define CON_TTY_MAX		256
#define CON_TTY_BUF_SIZE	240

#define CON_TTY_BROADCAST	((union bgtree_header){ bcast: { p2p: 0 }})
#define CON_TTY_P2P(v)		((union bgtree_header){ p2p: { p2p: 1, \
							       vector: v }})

struct tty_route {
    unsigned int send_id;
    unsigned int receive_id;
    union bgtree_header dest;	// tree target (node/bcast)
    unsigned count;		// reference counter, >0 /proc becomes read-only
};

/* data structure for an open tty */
struct bgtty_data {
    struct bg_console *con;
    struct tty_struct *tty;
    unsigned bufidx;
    char buffer[240];

    /* retransmit */
    union bgtree_header dest;
    struct bglink_hdr_tree lnkhdr;
    struct list_head list;

    /* pending receive */
    struct sk_buff_head skb_list;
};

struct bg_console {
    struct tty_route *tty_route;
    unsigned link_protocol;
    unsigned tree_channel;
    unsigned tree_route;
    unsigned num_tty;
    u32 phandle_tree;

    struct tty_driver *tty_driver;
    struct bg_tree *tree;
    struct bglink_proto lnk;
    struct ctl_table_header *sysctl_header;
    unsigned count;

    /* retransmit list */
    spinlock_t lock;
    struct list_head list;
};

static struct bg_console bg_console;

#define CON_BUF_SIZE	2048

struct bg_initconsole {
    struct tty_route tty_route;
    unsigned link_protocol;
    unsigned tree_channel;
    unsigned tree_route;
    int index;
    struct bg_tree *tree;
    struct list_head list;

    int bufidx;
    char buffer[CON_BUF_SIZE];
};

static LIST_HEAD(initcon_list);

static int bg_tty_proc_register(struct bg_console *bgcon);
static int bg_tty_proc_unregister(struct bg_console *bgcon);

static void bgtty_link_hdr_init(struct bglink_hdr_tree *lnkhdr,
				unsigned long count)
{
    bgtree_link_hdr_init(lnkhdr);
    lnkhdr->opt.opt_con.len = count;
}

static int do_write(struct bg_initconsole *bgcon,
		    const char *b, unsigned count)
{
    struct bglink_hdr_tree lnkhdr __attribute__((aligned(16)));
    union bgtree_header dest = { raw: 0 };
    int rc;

    if (!bgcon->tree)
	return 0;

    lnkhdr.dst_key = bgcon->tty_route.send_id;
    lnkhdr.lnk_proto = bgcon->link_protocol;

    dest = bgcon->tty_route.dest;
    dest.p2p.pclass = bgcon->tree_route;

    bgtty_link_hdr_init(&lnkhdr, count);
    rc = bgtree_xmit(bgcon->tree, bgcon->tree_channel, dest,
		     &lnkhdr, (void*)b, count);
    return count;
}

static void bg_console_write(struct console *co, const char *b, unsigned count)
{
    struct bg_initconsole *bgcon = (struct bg_initconsole*)co->data;
    int len;

    if (!bgcon->tree)
	bgcon->tree = bgtree_get_dev();

    /* if anything is buffered flush it first */
    if (bgcon->bufidx) {
	len = do_write(bgcon, bgcon->buffer, bgcon->bufidx);
	bgcon->bufidx -= len;
	if (len > 0 && bgcon->bufidx)
	    memcpy(bgcon->buffer, bgcon->buffer + len, bgcon->bufidx);
    }

    /* if buffer is empty write the new data right away, otherwise buffer */
    if (!bgcon->bufidx)
	count -= do_write(bgcon, b, count);

    if (count) {
	if (count + bgcon->bufidx > CON_BUF_SIZE)
	    count = CON_BUF_SIZE - bgcon->bufidx;
	memcpy(&bgcon->buffer[bgcon->bufidx], b, count);
	bgcon->bufidx += count;
    }
}

/*
 * console=bgtty0[,sndid,rcvid,{broadcast,treeid}]
 */
static int bg_console_setup (struct console * con, char * options)
{
    struct bg_initconsole *bgcon;

    if (con->index < 0 || con->index >= CON_TTY_MAX)
	return -ENODEV;

    bgcon = alloc_bootmem(sizeof(struct bg_initconsole));
    memset(bgcon, 0, sizeof(struct bg_initconsole));

    bgcon->index = con->index;
    bgcon->link_protocol = BGLINK_P_CON;
    bgcon->tree = NULL;
    bgcon->tree_route = 15;
    bgcon->tree_channel = 0;

    list_add(&bgcon->list, &initcon_list);

    if (options) {
	unsigned val[3];
	int validx = 0;
	char *s = options;

	while(*s && validx < 3) {
	    if (validx == 2 && (*s == 'b' || *s == 'm')) {
		val[validx++] = ~0U;
		break;
	    }
	    if (*s < '0' || *s > '9')
		break;
	    val[validx++] = simple_strtoul(s, &s, 0);
	    if (*s != ',')
		break;
	    s++;
	}

	if (validx == 3) {
	    bgcon->tty_route.send_id = val[0];
	    bgcon->tty_route.receive_id = val[1];
	    bgcon->tty_route.dest = (val[2] == ~0U) ?
		CON_TTY_BROADCAST : CON_TTY_P2P(val[2]);
            bgcon->tty_route.count = 0;
	}
    }

    con->data = bgcon;
    return 0;
}


static struct tty_driver *bg_console_device(struct console *c, int *ip)
{
    *ip = 0;
    return bg_console.tty_driver;
}


static struct console treecon = {
	.name	= CON_TTY_NAME,
	.write	= bg_console_write,
	.setup  = bg_console_setup,
	.device = bg_console_device,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

int __init bg_console_init(void)
{
    register_console(&treecon);
    return 0;
}

/**********************************************************************
 *                                  TTY
 **********************************************************************/

static void bgtty_try_inject(struct bgtty_data *data)
{
    int len;
    unsigned long flags;
    struct sk_buff *skb;

    spin_lock_irqsave(&data->skb_list.lock, flags);

    while(skb_queue_len(&data->skb_list)) {
	skb = skb_peek(&data->skb_list);
	len = tty_insert_flip_string(data->tty, skb->data, skb->len);
	if (len < skb->len) {
	    __skb_pull(skb, len);
	    break;
	} else {
	    __skb_unlink(skb, &data->skb_list);
	    dev_kfree_skb(skb);
	}
    }

    spin_unlock_irqrestore(&data->skb_list.lock, flags);
    tty_schedule_flip(data->tty);
}

static void bg_tty_unthrottle(struct tty_struct *tty)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;
    bgtty_try_inject(data);
}

static int con_receive(struct sk_buff *skb, struct bglink_hdr_tree *lnkhdr)
{
    struct bg_console *bgcon = &bg_console;
    struct tty_struct *tty;
    struct bgtty_data *data;
    int idx, len;

    BUG_ON(!bgcon->tty_route || !bgcon->tty_driver);

    len = lnkhdr->opt.opt_con.len;
    if (len > skb->len)
	goto out;

    skb_trim(skb, len);

    // XXX: hash?
    for (idx = 0; idx < bgcon->num_tty; idx++)
	if (bgcon->tty_route[idx].receive_id == lnkhdr->dst_key &&
	    bgcon->tty_route[idx].count > 0)
	    break;

    if (idx < bgcon->num_tty) {
	tty = bgcon->tty_driver->ttys[idx];
	if (!tty || !tty->driver_data)
	    goto out;

	data = (struct bgtty_data*)tty->driver_data;

	skb_queue_tail(&data->skb_list, skb);
	if (!test_bit(TTY_THROTTLED, &tty->flags))
	    bgtty_try_inject(data);

	return 0;
    }

 out:
    dev_kfree_skb(skb);
    return 0;
}

static int con_flush(int chn)
{
    struct bgtty_data *data, *tmp;
    struct bg_console *bgcon = &bg_console;
    int rc = 0;

    if (list_empty(&bgcon->list))
	return 0;

    /* called from IRQ context */
    spin_lock(&bgcon->lock);

    list_for_each_entry_safe(data, tmp, &bgcon->list, list) {
	BUG_ON(data->bufidx > CON_TTY_BUF_SIZE);

	rc = __bgtree_xmit(bgcon->tree, bgcon->tree_channel, data->dest,
			   &data->lnkhdr, data->buffer, data->bufidx);
	if (rc != 0)
	    goto out;

	data->bufidx = 0;
	mb();
	list_del(&data->list);
	data->list.next = NULL;

	/* signal ldisc to get more data */
	tty_wakeup(data->tty);
    }

 out:
    spin_unlock(&bgcon->lock);
    return rc;
}

#if 0
/* serialized by tty code */
static void bg_tty_flush_chars(struct tty_struct *tty)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;
    struct bg_console *bgcon = data->con;

    /* do not flush if enqueued into tree flush queue */
    if (data->list.next != NULL)
	return;

    if (data->bufidx) {
	bgtty_link_hdr_init(&data->lnkhdr, data->bufidx);

	if (bgtree_xmit(bgcon->tree, bgcon->tree_channel, data->dest,
			&data->lnkhdr, data->buffer, data->bufidx) != 0)
	    enqueue_retransmit(data);
	else
	    data->bufidx = 0;
    }
}

static void bg_tty_put_char(struct tty_struct *tty, unsigned char ch)
{
    BUG_ON(data->list.next != NULL);

    if (data->bufidx < CON_TTY_BUF_SIZE)
	data->buffer[data->bufidx++] = ch;
}
#endif

extern void enqueue_retransmit(struct bgtty_data *data)
{
    struct bg_console *bgcon = data->con;
    list_add_rcu(&data->list, &bgcon->list);
    bgtree_enable_inj_wm_interrupt(bgcon->tree, bgcon->tree_channel);
}

static int bg_tty_write(struct tty_struct *tty,
			const unsigned char *buf, int count)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;
    struct bg_console *bgcon = data->con;
    unsigned flags;

    if (count == 0)
	return 0;

    /* early exit if we are in the flush queue --> wait for IRQ */
    if (data->list.next != NULL)
	return 0;

    spin_lock_irqsave(&bgcon->lock, flags);

    /* restrict the number of fragments to 1 */
    if (count > CON_TTY_BUF_SIZE)
	count = CON_TTY_BUF_SIZE;

    bgtty_link_hdr_init(&data->lnkhdr, count);

    if (bgtree_xmit(bgcon->tree, bgcon->tree_channel, data->dest,
		    &data->lnkhdr, (unsigned char*)buf, count) != 0) {
	memcpy(data->buffer, buf, count);
	data->bufidx = count;
	mb();
	enqueue_retransmit(data);
    }

    spin_unlock_irqrestore(&bgcon->lock, flags);

    return count;
}


static int bg_tty_write_room(struct tty_struct *tty)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;

    if (!data)
	return 0;

    /* report no room if we are in the flush queue */
    if (data->list.next != NULL)
	return 0;
    else
	return (CON_TTY_BUF_SIZE - data->bufidx);
}

static int bg_tty_chars_in_buffer(struct tty_struct *tty)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;

    if (!data)
	return 0;
    else
	return data->bufidx;
}

static int bg_tty_open(struct tty_struct *tty, struct file * filp)
{
    struct bg_console *bgcon = &bg_console;

    if (!bgcon->tty_route[tty->index].count) {
	struct bgtty_data *data =
	    kzalloc(sizeof(struct bgtty_data), GFP_KERNEL);
	if (!data)
	    return -ENOMEM;

	data->con = bgcon;
	data->tty = tty;
	data->list.next = NULL;
	skb_queue_head_init(&data->skb_list);

	/* pre-initialize link header and dest */
	data->lnkhdr.dst_key = bgcon->tty_route[tty->index].send_id;
	data->lnkhdr.lnk_proto = bgcon->lnk.lnk_proto;
	data->dest = bgcon->tty_route[tty->index].dest;
	data->dest.p2p.pclass = bgcon->tree_route;

	tty->low_latency = 1;
	tty->driver_data = data;
    }

    bgcon->tty_route[tty->index].count++;

    if (bgcon->count++ == 0)
	bglink_register_proto(&bgcon->lnk);

    return 0;
}


static void bg_tty_close(struct tty_struct *tty, struct file * filp)
{
    struct bgtty_data *data = (struct bgtty_data*)tty->driver_data;
    struct bg_console *bgcon = data->con;
    unsigned flags;

    if (!data)
	return;

    spin_lock_irqsave(&bgcon->lock, flags);

    if (--bgcon->tty_route[tty->index].count == 0) {
	tty->driver_data = NULL;
	mb();

	/* remove from retransmit */
	if (data->list.next != NULL)
	    list_del(&data->list);

	/* delete all unprocessed skbuffs */
	while(!skb_queue_empty(&data->skb_list))
	    dev_kfree_skb(skb_dequeue(&data->skb_list));

	/* dequeue from workqueue, flush send buffer, delete all skbuffs, ... */
	kfree(data);
    }

    spin_unlock_irqrestore(&bgcon->lock, flags);

    if (--bgcon->count == 0) {
	bglink_unregister_proto(&bgcon->lnk);
    }
}

static struct tty_operations treecon_ops = {
    .open = bg_tty_open,
    .close = bg_tty_close,
    .write = bg_tty_write,
    .write_room = bg_tty_write_room,
    .chars_in_buffer = bg_tty_chars_in_buffer,
    .unthrottle = bg_tty_unthrottle,
#if 0
    .put_char = bg_tty_put_char,
    .flush_chars = bg_tty_flush_chars,
#endif
};

static int __devinit of_read_uint_prop(struct device_node *np,
				       const char *name, u32 *val, int fatal)
{
    int len;
    const u32 *prop = get_property(np, name, &len);
    if (prop == NULL || len < sizeof(u32)) {
	if (fatal)
	    printk(KERN_ERR "%s: missing %s property\n",
		   np->full_name, name);
	return -ENODEV;
    }
    *val = *prop;
    return 0;
}

static int __devinit bgcon_probe(struct of_device *ofdev,
				 const struct of_device_id *match)
{
    struct device_node *np = ofdev->node;
    struct tty_driver *driver;
    struct bg_initconsole *con;
    struct bg_console *bgcon = &bg_console;
    int idx;

    if (of_read_uint_prop(np, "link-protocol", &bgcon->link_protocol, 0))
	bgcon->link_protocol = BGLINK_P_CON;
    if (of_read_uint_prop(np, "tree-device", &bgcon->phandle_tree, 1))
	return -ENODEV;
    if (of_read_uint_prop(np, "tree-route", &bgcon->tree_route, 0))
	bgcon->tree_route = 15;
    if (of_read_uint_prop(np, "tree-channel", &bgcon->tree_channel, 0))
	bgcon->tree_channel = 0;

    of_read_uint_prop(np, "num", &bgcon->num_tty, 0);
    if (bgcon->num_tty <= 0 || bgcon->num_tty > CON_TTY_MAX)
	bgcon->num_tty = CON_TTY_MAX;

    bgcon->tree = bgtree_get_dev();

    /* init retransmit queue */
    INIT_LIST_HEAD(&bgcon->list);
    spin_lock_init(&bgcon->lock);

    /* link protocol */
    bgcon->lnk.tree_rcv = con_receive;
    bgcon->lnk.tree_flush = con_flush;
    bgcon->lnk.torus_rcv = NULL;
    bgcon->lnk.lnk_proto = bgcon->link_protocol;
    bgcon->lnk.receive_from_self = 0;

    /* routes */
    bgcon->tty_route =
	kmalloc(sizeof(struct tty_route) * bg_console.num_tty, GFP_KERNEL);
    if (!bgcon->tty_route)
	return -ENOMEM;

    driver = alloc_tty_driver(bgcon->num_tty);
    if (!driver)
	goto err_alloc;

    driver->owner = THIS_MODULE;
    driver->name = CON_TTY_NAME;
    driver->driver_name = CON_TTY_NAME;
    driver->major = CON_TTY_MAJOR;
    driver->minor_start = CON_TTY_MINOR;
    driver->type = TTY_DRIVER_TYPE_SYSTEM;
    driver->init_termios = tty_std_termios;
    driver->flags = TTY_DRIVER_REAL_RAW;
    tty_set_operations(driver, &treecon_ops);

    /* pre-configure ttys */
    for (idx = 0; idx < bgcon->num_tty; idx++) {
	bgcon->tty_route[idx].send_id = idx;
	bgcon->tty_route[idx].receive_id = idx;
	bgcon->tty_route[idx].count = 0;
	bgcon->tty_route[idx].dest = CON_TTY_BROADCAST;
    }

    /* copy the console settings over */
    list_for_each_entry(con, &initcon_list, list)
	if (con->index < bg_console.num_tty)
	    bgcon->tty_route[con->index] = con->tty_route;

    if (tty_register_driver(driver)) {
	printk("Error registering BG tty driver\n");
	goto err_register;
    }

    bg_tty_proc_register(bgcon);

    bgcon->tty_driver = driver;

    printk("Bgcon: registered %d ttys\n", bgcon->num_tty);
    return 0;

 err_register:
    put_tty_driver(driver);

 err_alloc:
    kfree(bgcon->tty_route);
    return -ENOMEM;

}

static int __devinit bgcon_remove(struct of_device *ofdev)
{
    bg_tty_proc_unregister(&bg_console);

    // to be done
    return 0;
}

static struct of_device_id bgcon_match[] = {
    {
	.type = "tty",
	.compatible = "ibm,kittyhawk",
    },
    {},
};

static struct of_platform_driver bgcon_driver = {
    .name = "bgtty",
    .match_table = bgcon_match,
    .probe = bgcon_probe,
    .remove = bgcon_remove,
};

static int __init bgcon_module_init (void)
{
    of_register_platform_driver(&bgcon_driver);
    return 0;
}

static void __exit bgcon_module_exit (void)
{
    of_unregister_platform_driver(&bgcon_driver);
}

module_init(bgcon_module_init);
module_exit(bgcon_module_exit);
console_initcall(bg_console_init);

#if defined(CONFIG_SYSCTL) && defined(CONFIG_PROC_FS)

static int proc_do_tty (ctl_table *ctl, int write, struct file * filp,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
    char *page = (char*)get_zeroed_page(GFP_KERNEL);
    char *p;
    struct bg_console *bgcon = (struct bg_console *)ctl->extra1;
    int idx = (int)ctl->extra2;
    int len, rc = 0, validx;

    BUG_ON(!bgcon || idx < 0 || idx >= bgcon->num_tty);

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
	    if (bgcon->tty_route[idx].count > 0) {
		rc = -EBUSY;
		goto out;
	    }
	    bgcon->tty_route[idx].send_id = val[0];
	    bgcon->tty_route[idx].receive_id = val[1];
	    bgcon->tty_route[idx].dest = (val[2] == -1) ?
		CON_TTY_BROADCAST : CON_TTY_P2P(val[2]);
	}
    } else {
	len = sprintf(page, "%d %d ", bgcon->tty_route[idx].send_id,
		      bgcon->tty_route[idx].receive_id);
	if (bgcon->tty_route[idx].dest.bcast.p2p == 0)
	    len += sprintf(page + len, "broadcast\n");
	else
	    len += sprintf(page + len, "%d\n",
			   bgcon->tty_route[idx].dest.p2p.vector);
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

static struct ctl_table bgtty_root[] = {
    {
	.ctl_name	= CTL_UNNUMBERED,
	.procname	= "bgtty",
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
	.child		= bgtty_root,
    },
    { .ctl_name = 0 }
};

static int bg_tty_proc_register(struct bg_console *bgcon)
{
    int idx;
    struct ctl_table *bgtty_ent;

    bgtty_ent = (struct ctl_table*)kzalloc(sizeof(struct ctl_table) *
					    (bgcon->num_tty + 1), GFP_KERNEL);
    if (!bgtty_ent)
	return -ENOMEM;

    bgtty_root[0].child = bgtty_ent;

    for (idx = 0; idx < bgcon->num_tty; idx++)
    {
	bgtty_ent[idx].ctl_name = CTL_UNNUMBERED;
	bgtty_ent[idx].procname = kmalloc(5, GFP_KERNEL);
	BUG_ON(!bgtty_ent[idx].procname);
	sprintf((char*)bgtty_ent[idx].procname, "%d", idx);
	bgtty_ent[idx].mode = 0644;
	bgtty_ent[idx].proc_handler = proc_do_tty;
	bgtty_ent[idx].extra1 = (void*)bgcon;
	bgtty_ent[idx].extra2 = (void*)idx;
    }

    bgcon->sysctl_header = register_sysctl_table(dev_root);
    return 0;
}

static int bg_tty_proc_unregister(struct bg_console *bgcon)
{
    if (bgcon->sysctl_header) {
	unregister_sysctl_table(bgcon->sysctl_header);
	bgcon->sysctl_header = NULL;
    }
    return 0;
}
#else
static int bg_tty_proc_register(struct bg_console *bgcon) { return 0; }
static int bg_tty_proc_unregister(struct bg_console *bgcon) { return 0; }
#endif
