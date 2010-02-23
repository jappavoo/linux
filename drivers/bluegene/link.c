/*********************************************************************
 *
 * Copyright (C) 2007-2008, IBM Corporation, Project Kittyhawk
 *		       Volkmar Uhlig (vuhlig@us.ibm.com)
 *
 * Description: Blue Gene low-level driver for tree and torus
 *
 * All rights reserved
 *
 ********************************************************************/

/**********************************************************************
 * link protocol handling
 **********************************************************************/

#include "link.h"

LIST_HEAD(linkproto_list);
DEFINE_SPINLOCK(linkproto_lock);

void bglink_register_proto(struct bglink_proto *proto)
{
    spin_lock(&linkproto_lock);
    list_add_rcu(&proto->list, &linkproto_list);
    spin_unlock(&linkproto_lock);
}

void bglink_unregister_proto(struct bglink_proto *proto)
{
    spin_lock(&linkproto_lock);
    list_del_rcu(&proto->list);
    spin_unlock(&linkproto_lock);

    synchronize_rcu();
}

struct bglink_proto* bglink_find_proto(u16 proto)
{
    struct bglink_proto *pos;

    list_for_each_entry_rcu(pos, &linkproto_list, list) {
	if (pos->lnk_proto == proto)
	    return pos;
    }
    return NULL;
}
