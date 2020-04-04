/**
 * Baoyou Xie's drop-packet module
 *
 * Copyright (C) 2018 Baoyou Xie.
 *
 * Author: Baoyou Xie <baoyou.xie@gmail.com>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/version.h>
#include <linux/module.h>

#include "drop_packet.h"

static int drop_packet_init(void)
{
	printk("drop-packet loaded.\n");

	return 0;
}

static void drop_packet_exit(void)
{
	printk("drop-packet unloaded.\n");
}

module_init(drop_packet_init)
module_exit(drop_packet_exit)

MODULE_DESCRIPTION("Baoyou Xie's drop-packet module");
MODULE_AUTHOR("Baoyou Xie <baoyou.xie@gmail.com>");
MODULE_LICENSE("GPL v2");
