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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "drop_packet.h"

static unsigned long drop_packet_activated;

static int activate_drop_packet(void)
{
	return 1;
}

static void deactivate_drop_packet(void)
{
}

static int drop_packet_show(struct seq_file *m, void *v)
{
	seq_printf(m, "settings:\n");
	seq_printf(m, "  activated: %s\n", drop_packet_activated ? "y" : "N");

	return 0;
}

static int drop_packet_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, drop_packet_show, NULL);
}

static ssize_t drop_packet_write(struct file *file,
	const char __user *buf, size_t count, loff_t *offs)
{
	int ret;
	char cmd[255];
	char chr[255];

	if (count < 1 || *offs)
		return -EINVAL;

	if (copy_from_user(chr, buf, 255))
		return -EFAULT;

	ret = sscanf(chr, "%255s", cmd);
	if (ret <= 0)
		return -EINVAL;

	if (strcmp(cmd, "activate") == 0) {
		if (!drop_packet_activated)
			drop_packet_activated = activate_drop_packet();
	} else if (strcmp(cmd, "deactivate") == 0) {
		if (drop_packet_activated)
			deactivate_drop_packet();
		drop_packet_activated = 0;
	}

	return count;
}

const struct file_operations drop_packet_fops = {
	.open           = drop_packet_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = drop_packet_write,
	.release        = single_release,
};

static int drop_packet_init(void)
{
	int ret;
	struct proc_dir_entry *pe;

	proc_mkdir("mooc", NULL);
	proc_mkdir("mooc/net", NULL);

	ret = -ENOMEM;
	pe = proc_create("mooc/net/drop-packet",
				S_IFREG | 0644,
				NULL,
				&drop_packet_fops);
	if (!pe)
		goto err_proc;

	printk("drop-packet loaded.\n");

	return 0;

err_proc:
	return ret;
}

static void drop_packet_exit(void)
{
	remove_proc_entry("mooc/net/drop-packet", NULL);
	remove_proc_entry("mooc/net", NULL);
	remove_proc_entry("mooc", NULL);
	
	printk("drop-packet unloaded.\n");
}

module_init(drop_packet_init)
module_exit(drop_packet_exit)

MODULE_DESCRIPTION("Baoyou Xie's drop-packet module");
MODULE_AUTHOR("Baoyou Xie <baoyou.xie@gmail.com>");
MODULE_LICENSE("GPL v2");
