/*********************************************************************
 *
 * Copyright (C) 2007-2008, IBM Corporation, Project Kittyhawk
 *		       Volkmar Uhlig (vuhlig@us.ibm.com)
 *
 * Description: Blue Gene driver exposing tree and torus as a NIC
 *
 * All rights reserved
 *
 ********************************************************************/

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/etherdevice.h>

#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/of_platform.h>

#include "link.h"
#include "tree.h"
#include "torus.h"


/**********************************************************************
 *                           defines
 **********************************************************************/

#define DRV_NAME	"bgnet"
#define DRV_VERSION	"0.5"
#define DRV_DESC	"Blue Gene NIC (IBM, Project Kittyhawk)"

MODULE_DESCRIPTION(DRV_DESC);
MODULE_AUTHOR("IBM, Project Kittyhawk");

#define BGNET_FRAG_MTU		240
#define BGNET_MAX_MTU		(BGNET_FRAG_MTU * 128)
#define BGNET_DEFAULT_MTU	ETH_DATA_LEN

struct bgnet_dev
{
    /* tree-specific variables */
    struct bg_tree *tree;
    unsigned int tree_route;
    unsigned int tree_channel;
    unsigned int eth_bridge_vector;
    struct sk_buff_head pending_skb_list;

    /* torus-specific variables */
    struct bg_torus *torus;

    unsigned int network_id;
    unsigned short link_protocol;
    struct net_device_stats stats;
    struct list_head list;

    struct of_device *tree_ofdev;
    struct of_device *torus_ofdev;
};

static struct bglink_proto bgnet_lnk;

static DEFINE_SPINLOCK(bgnet_lock);
static LIST_HEAD(bgnet_list);

struct skb_cb_lnk {
    struct bglink_hdr_tree lnkhdr;
    union bgtree_header dest;
};

/**********************************************************************
 *                         Linux module
 **********************************************************************/

MODULE_DESCRIPTION("BlueGene Ethernet driver");
MODULE_LICENSE("Proprietary");

//#define TRACE(x...) printk(x)
#define TRACE(x...)

/**********************************************************************
 *                   Linux' packet and skb management
 **********************************************************************/

static void bgnet_link_hdr_init(struct bglink_hdr_tree *lnkhdr,
				struct bgnet_dev *bgnet,
				unsigned long offset,
				unsigned long trailer)
{
    bgtree_link_hdr_init(lnkhdr);

    lnkhdr->dst_key = bgnet->network_id;
    lnkhdr->lnk_proto = bgnet->link_protocol;
    lnkhdr->opt.opt_net.pad_head = offset;
    lnkhdr->opt.opt_net.pad_tail = trailer;
}

static int bgnet_change_mtu(struct net_device *dev, int new_mtu)
{
    if (new_mtu < 60 || new_mtu > BGNET_MAX_MTU )
	return -EINVAL;
    dev->mtu = new_mtu;
    return 0;
}

static struct bgnet_dev* find_bgnet_device(unsigned int network_id)
{
    struct bgnet_dev *bgnet;
    list_for_each_entry_rcu(bgnet, &bgnet_list, list) {
	if (bgnet->network_id == network_id)
	    return bgnet;
    }
    return NULL;
}

static int bgnet_receive(struct sk_buff *skb, struct bgnet_dev *bgnet)
{
    struct net_device *dev = (struct net_device*)((void *)bgnet -
						    netdev_priv(NULL));

    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);

#ifdef CONFIG_BGP_SUPPRESS_CHECKSUMS
    skb->ip_summed = CHECKSUM_UNNECESSARY; // avoid all checksum verification
#endif /* CONFIG_BGP_SUPPRESS_CHECKSUMS */

    dev->last_rx = jiffies;
    bgnet->stats.rx_packets++;
    bgnet->stats.rx_bytes += skb->len;

    netif_rx(skb);
    return 0;
}

static int tree_receive(struct sk_buff *skb, struct bglink_hdr_tree *lnkhdr)
{
    struct bgnet_dev *bgnet = find_bgnet_device(lnkhdr->dst_key);
    if (!bgnet) {
	dev_kfree_skb(skb);
	return 0;
    }

    TRACE("bgnet rcvd pkt: data=%p, len=%d, head=%d, tail=%d, res len=%d\n",
	  skb->data, skb->len, lnkhdr->opt.opt_net.pad_head,
	  lnkhdr->opt.opt_net.pad_tail,
	  skb->len - lnkhdr->opt.opt_net.pad_head -
				lnkhdr->opt.opt_net.pad_tail);

    if (skb->len % BGNET_FRAG_MTU != 0)
	printk("bgnet: received packet size not multiple of %d\n",
	       BGNET_FRAG_MTU);

    /* skb_pull and trim check for over/underruns. For 0 size the
     * add/subtract is the same as a test */
    skb_pull(skb, lnkhdr->opt.opt_net.pad_head);
    skb_trim(skb, skb->len - lnkhdr->opt.opt_net.pad_tail);

    return bgnet_receive(skb, bgnet);
}


/* We assume that typically we can deplete the queue and if not we
 * just requeue the packet.  Thus we avoid the SMP headaches. */
static int __tree_flush(struct bgnet_dev *bgnet)
{
    int rc;

    while (skb_queue_len(&bgnet->pending_skb_list) > 0) {
	struct sk_buff *skb = skb_dequeue(&bgnet->pending_skb_list);
	struct skb_cb_lnk *cb = (struct skb_cb_lnk*)skb->cb;
	struct bglink_hdr_tree lnkhdr __attribute__((aligned(16)));

	TRACE("bgnet xmit %x: dst=%08x, src=%08x, idx=%d, ldst=%08x, "
	      "head=%d, tail=%d, len=%d\n",
	      cb->lnkhdr.conn_id, cb->lnkhdr.dst_key, cb->lnkhdr.src_key,
	      cb->lnkhdr.this_pkt, cb->dest.raw,
	      cb->lnkhdr.opt.opt_net.pad_head, cb->lnkhdr.opt.opt_net.pad_tail,
	      skb->len);

	lnkhdr = cb->lnkhdr;
	lnkhdr.opt.opt_net.option = 1;

	rc = __bgtree_xmit(bgnet->tree, bgnet->tree_channel, cb->dest,
			   &lnkhdr, skb->data, skb->len);

	if (rc == 0) {
	    bgnet->stats.tx_packets++;
	    bgnet->stats.tx_bytes += skb->len;
	    dev_kfree_skb_irq(skb);
	} else {
	    /* retransmit failed--keep in queue for later flush */
	    cb->lnkhdr = lnkhdr;
	    skb_queue_head(&bgnet->pending_skb_list, skb);
	    return rc;
	}
    }

    /* nothing pending... kick device */
    //netif_wake_queue(dev);

    return 0;
}

static int tree_flush(int chn)
{
    struct bgnet_dev *bgnet;
    int rc;

    list_for_each_entry_rcu(bgnet, &bgnet_list, list) {
	if (chn == bgnet->tree_channel)
	    if ((rc = __tree_flush(bgnet)))
		return rc;
    }

    return 0;
}

static int torus_receive(struct sk_buff *skb, struct bglink_hdr_torus *lnkhdr)
{
    struct bgnet_dev *bgnet = find_bgnet_device(lnkhdr->dst_key);

    TRACE("torus_receive: dstkey=%x, len=%d (skb=%d), proto=%x\n",
	  lnkhdr->dst_key, lnkhdr->len, skb->len, lnkhdr->lnk_proto);

    if (!bgnet)
	return -ENODEV;

    return bgnet_receive(skb, bgnet);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void bgnet_poll(struct net_device *dev)
{
    /* no-op; packets are fed by the tree device */
}
#endif

static inline int is_torus_ether_addr(const u8 *addr)
{
    return ((addr[0] & 0x7) == 0x6);
}

/* compute the tree destination based on the mac address */
extern inline void bgnet_ethaddr_to_treedest(struct bgnet_dev *bgnet,
					     union bgtree_header *dest,
					     const u8 *dev_addr,
					     const u8 *dest_addr)
{
    dest->raw = 0;

    if (is_multicast_ether_addr(dest_addr)) {
	unsigned int vector = bgtree_mcast_redirect(bgnet->tree, dest_addr);
	if (vector == -1) {
	    dest->bcast.tag = 0;
	} else {
	    dest->p2p.p2p = 1;
	    dest->p2p.vector = vector;
	}
    } else {
	dest->p2p.p2p = 1;

	if ( bgnet->eth_bridge_vector != -1 && (
		 (dest_addr[0] != dev_addr[0]) ||
		 (dest_addr[1] != dev_addr[1]) ||
		 (dest_addr[2] != dev_addr[2])) )
	{
	    /* target MAC address outside of reachable scope */
	    dest->p2p.vector = bgnet->eth_bridge_vector;
	} else
	    dest->p2p.vector = *(unsigned int *)(&dest_addr[2]);
    }

    dest->p2p.pclass = bgnet->tree_route;
}

static int tree_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    union bgtree_header dest;
    struct bglink_hdr_tree lnkhdr __attribute__((aligned(16)));
    struct bgnet_dev *bgnet = netdev_priv(dev);
    unsigned long offset, trailer;

    TRACE("%s: skb=%p, eth=%p, dev=%p, len=%d\n",
	  __FUNCTION__, skb, eth, dev, skb->len);

    bgnet_ethaddr_to_treedest(bgnet, &dest, dev->dev_addr, eth->h_dest);

    /* pad out head of packet so it starts at a 16 byte boundary. */
    offset = ((unsigned long)skb->data) & 0xf;
    skb_push(skb, offset);

    /* remember how much of the last fragment is unused. */
    trailer = (BGNET_FRAG_MTU - 1) - ((skb->len - 1) % BGNET_FRAG_MTU);

    bgnet_link_hdr_init(&lnkhdr, bgnet, offset, trailer);

    TRACE("bgnet xmit: dst=%08x, src=%08x, ldst=%08x, head=%d, tail=%d\n",
	  lnkhdr.dst_key, lnkhdr.src_key, dest.raw,
	  lnkhdr.opt.opt_net.pad_head, lnkhdr.opt.opt_net.pad_tail);

    if ((skb_queue_len(&bgnet->pending_skb_list) == 0) &&
	(bgtree_xmit(bgnet->tree, bgnet->tree_channel, dest,
		     &lnkhdr, skb->data, skb->len) == 0))
    {
	bgnet->stats.tx_packets++;
	bgnet->stats.tx_bytes += skb->len;
	dev_kfree_skb(skb);
    } else {
	/* pending queue was already non-empty, or transmit failed */
	/* queue skb until device becomes available again */
	/* keep lnkhdr and dest around for re-transmit */
	if (skb_queue_len(&bgnet->pending_skb_list) <= 1000) {
	    struct skb_cb_lnk *cb = (struct skb_cb_lnk*)skb->cb;
	    cb->lnkhdr = lnkhdr;
	    cb->dest = dest;
	    skb_queue_tail(&bgnet->pending_skb_list, skb);
	    bgtree_enable_inj_wm_interrupt(bgnet->tree, bgnet->tree_channel);
	} else {
	    /* Too many pending.  Just drop the new packet. */
	    bgnet->stats.tx_dropped++;
	    dev_kfree_skb(skb);
	}
    }

    return 0;
}

static int torus_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    struct bgnet_dev *bgnet = netdev_priv(dev);
    union torus_inj_desc desc;
    struct bglink_hdr_torus *lnkhdr;

    if (bgnet->torus == NULL) {
	printk("Target MAC address torus but no device available\n");
	bgnet->stats.tx_dropped++;
	dev_kfree_skb(skb);
	return 0;
    }

    lnkhdr = (struct bglink_hdr_torus*)skb_push(skb, sizeof(*lnkhdr));
    lnkhdr->dst_key = bgnet->network_id;
    lnkhdr->lnk_proto = bgnet->link_protocol;
    lnkhdr->len = skb->len;
    lnkhdr->opt.raw = 0;

    TRACE("torus_start_xmit: [%d, %d, %d], len=%d\n",
	  eth->h_dest[3], eth->h_dest[4], eth->h_dest[5], skb->len);

    bgtorus_init_inj_desc(bgnet->torus, &desc, skb->len, eth->h_dest[3],
			  eth->h_dest[4], eth->h_dest[5]);

    bgtorus_xmit(bgnet->torus, &desc, skb);

    bgnet->stats.tx_packets++;
    bgnet->stats.tx_bytes += skb->len;

    return 0;
}

static int bgnet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;

    if (is_torus_ether_addr(eth->h_dest))
	return torus_start_xmit(skb, dev);
    else
	return tree_start_xmit(skb, dev);
}


static void bgnet_uninit(struct net_device *dev)
{
    struct bgnet_dev *bgnet = netdev_priv(dev);

    spin_lock(&bgnet_lock);
    list_del_rcu(&bgnet->list);

    if (list_empty(&bgnet_list))
	bglink_unregister_proto(&bgnet_lnk);

    spin_unlock(&bgnet_lock);
    synchronize_rcu();
}

static struct net_device_stats *bgnet_get_stats(struct net_device *dev)
{
    struct bgnet_dev *bgnet = netdev_priv(dev);
    return &bgnet->stats;
}


static int bgnet_init(struct net_device *dev)
{
    struct bgnet_dev *bgnet = netdev_priv(dev);

    bgnet->tree = dev_get_drvdata(&bgnet->tree_ofdev->dev);
    if (!bgnet->tree)
	return -1;

    if (bgnet->torus_ofdev) {
	bgnet->torus = dev_get_drvdata(&bgnet->torus_ofdev->dev);
    }

    spin_lock(&bgnet_lock);
    if (list_empty(&bgnet_list)) {
	// register with tree
	bgnet_lnk.lnk_proto = bgnet->link_protocol;
	bgnet_lnk.receive_from_self = 0;
	bgnet_lnk.tree_rcv = tree_receive;
	bgnet_lnk.tree_flush = tree_flush;
	bgnet_lnk.torus_rcv = torus_receive;
	bglink_register_proto(&bgnet_lnk);
    }
    list_add_rcu(&bgnet->list, &bgnet_list);

    spin_unlock(&bgnet_lock);

    skb_queue_head_init(&bgnet->pending_skb_list);

    return 0;
}

static int __devinit bgnet_read_uint_prop(struct device_node *np,
					  const char *name,
					  u32 *val, int fatal)
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


static int __devinit bgnet_probe(struct of_device *ofdev,
				 const struct of_device_id *match)
{
    const void *p;
    struct device_node *np = ofdev->node;
    struct bgnet_dev *bgnet;
    struct net_device *dev;
    u32 frame_size;
    u32 phandle;
    u32 protocol;

    dev = alloc_etherdev(sizeof(struct bgnet_dev));
    if (!dev)
	return -ENOMEM;

    SET_MODULE_OWNER(dev);
    SET_NETDEV_DEV(dev, &ofdev->dev);

    bgnet = netdev_priv(dev);
    memset(bgnet, 0, sizeof(*bgnet));

    if (bgnet_read_uint_prop(np, "tree-device", &phandle, 1) ||
	(bgnet->tree_ofdev = of_find_device_by_phandle(phandle)) == NULL) {
	printk(KERN_ERR "%s: could not find tree device\n", np->full_name);
	goto err;
    }
    if (!bgnet_read_uint_prop(np, "torus-device", &phandle, 0))
	bgnet->torus_ofdev = of_find_device_by_phandle(phandle);
    if (bgnet_read_uint_prop(np, "tree-route", &bgnet->tree_route, 0))
	bgnet->tree_route = 15;
    if (bgnet_read_uint_prop(np, "tree-channel", &bgnet->tree_channel, 0))
	bgnet->tree_channel = 0;
    if (bgnet_read_uint_prop(np, "bridge-vector",
                             &bgnet->eth_bridge_vector, 0))
        bgnet->eth_bridge_vector = -1;
    if (bgnet_read_uint_prop(np, "frame-size", &frame_size, 0))
	frame_size = BGNET_DEFAULT_MTU;

    if (bgnet_read_uint_prop(np, "network-id", &bgnet->network_id, 1)) {
	printk("%s: network id not specified\n", np->full_name);
	goto err;
    }
    if (bgnet_read_uint_prop(np, "link-protocol", &protocol, 0))
	protocol = BGLINK_P_NET;
    bgnet->link_protocol = protocol;
    if (find_bgnet_device(bgnet->network_id)) {
	printk("%s: duplicate network id (%d)\n",
	       np->full_name, bgnet->network_id);
	goto err;
    }

    p = get_property(np, "local-mac-address", NULL);
    if (p == NULL) {
	printk(KERN_ERR "%s: Can't find local-mac-address property\n",
	       np->full_name);
	goto err;
    }

    memcpy(dev->dev_addr, p, 6);

    dev->init			= bgnet_init;
    dev->uninit			= bgnet_uninit;
    dev->get_stats	        = bgnet_get_stats;
    dev->hard_start_xmit	= bgnet_start_xmit;
    dev->change_mtu		= bgnet_change_mtu;
    dev->mtu			= frame_size;
#ifdef CONFIG_NET_POLL_CONTROLLER
    dev->poll_controller	= bgnet_poll;
#endif

#ifdef CONFIG_BGP_SUPPRESS_CHECKSUMS
    /*
     * We don't need checksums on ANY packets inside bluegene, but we rely on
     * the xemac device to add checksums to any packets that reach the outside
     * world.  Because xemac can add checksums only for ipv4 udp and tcp
     * packets, we suppress checksum generation only for those packets.
     */
    dev->features |= NETIF_F_IP_CSUM;
#endif /* CONFIG_BGP_SUPPRESS_CHECKSUMS */

    if (register_netdev(dev) != 0)
	goto err;

    /* increase header size to fit torus hardware header */
    if (bgnet->torus_ofdev)
	dev->hard_header_len += 16;

    printk(KERN_INFO
	   "%s: BGNET %s, MAC %02x:%02x:%02x:%02x:%02x:%02x, network %d\n",
	   dev->name, np->full_name,
	   dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
	   dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5],
           bgnet->network_id);
    if (bgnet->eth_bridge_vector != -1)
        printk(KERN_INFO "      bridge 0x%06x\n", bgnet->eth_bridge_vector);

    return 0;

 err:
    free_netdev(dev);
    return -1;
}

static int __devinit bgnet_remove(struct of_device *ofdev)
{
    // to be done
    return 0;
}

static struct of_device_id bgnet_match[] = {
    {
	.type = "network",
	.compatible = "ibm,kittyhawk",
    },
    {},
};

static struct of_platform_driver bgnet_driver = {
    .name = "bgnet",
    .match_table = bgnet_match,
    .probe = bgnet_probe,
    .remove = bgnet_remove,
};

static int __init bgnet_module_init (void)
{
    printk(KERN_INFO DRV_DESC ", version " DRV_VERSION "\n");
    of_register_platform_driver(&bgnet_driver);
    return 0;
}

static void __exit bgnet_module_exit (void)
{
    of_unregister_platform_driver(&bgnet_driver);
}

module_init(bgnet_module_init);
module_exit(bgnet_module_exit);
