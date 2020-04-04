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
#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include "drop_packet.h"

static unsigned long drop_packet_activated;

static struct kprobe kprobe_eth_type_trans;
static struct kprobe kprobe_napi_gro_receive;
static struct kprobe kprobe___netif_receive_skb_core;
static struct kprobe kprobe_tcp_v4_rcv;

enum packet_footprint
{
	ETH_RECV,
	GRO_RECV,
	RECV_SKB,
	TCP_V4_RCV,
	SEND_SKB,
	TRACK_COUNT,
};

static void trace_packet(const struct sk_buff *skb,
	const struct iphdr *iphdr, enum packet_footprint fp)
{
	unsigned int source = iphdr->saddr;
	unsigned int dest = iphdr->daddr;
	unsigned char *saddr = (void *)&source;
	unsigned char *daddr = (void *)&dest;

	printk("Source IP：%u.%u.%u.%u, Dest IP：%u.%u.%u.%u, fp: %d\n",
				saddr[0], saddr[1], saddr[2], saddr[3],
				daddr[0], daddr[1], daddr[2], daddr[3], fp);
}

static void trace_net_dev_xmit_hit(void *ignore, struct sk_buff *skb,
								   int rc, struct net_device *dev, unsigned int skb_len)
{
	struct iphdr *iphdr;

	if (!drop_packet_activated)
		return;

	if (rc != NETDEV_TX_OK)
		return;

	iphdr = ip_hdr(skb);
	trace_packet(skb, iphdr, SEND_SKB);
}

static int kprobe_eth_type_trans_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct sk_buff *skb = (void *)regs->di;
	struct iphdr *iphdr;

	if (!drop_packet_activated)
		return 0;

	iphdr = (struct iphdr *)(skb->data + ETH_HLEN);
	trace_packet(skb, iphdr, ETH_RECV);

	return 0;
}

static int kprobe_napi_gro_receive_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct sk_buff *skb = (void *)regs->si;
	struct iphdr *iphdr;

	if (!drop_packet_activated)
		return 0;

	if (skb->protocol != cpu_to_be16(ETH_P_IP))
		return 0;

	iphdr = (struct iphdr *)skb->data;
	trace_packet(skb, iphdr, GRO_RECV);

	return 0;
}

static int kprobe___netif_receive_skb_core_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct sk_buff *skb = (void *)regs->di;
	struct iphdr *iphdr;

	if (!drop_packet_activated)
		return 0;

	if (skb->protocol != cpu_to_be16(ETH_P_IP))
		return 0;

	iphdr = (struct iphdr *)skb->data;
	trace_packet(skb, iphdr, RECV_SKB);

	return 0;
}

static int kprobe_tcp_v4_rcv_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct sk_buff *skb = (void *)regs->di;
	struct iphdr *iphdr;

	if (!drop_packet_activated)
		return 0;

	if (skb->protocol != cpu_to_be16(ETH_P_IP))
		return 0;

	iphdr = ip_hdr(skb);
	trace_packet(skb, iphdr, TCP_V4_RCV);

	return 0;
}

static int activate_drop_packet(void)
{
	hook_tracepoint("net_dev_xmit", trace_net_dev_xmit_hit, NULL);
	hook_kprobe(&kprobe_eth_type_trans, "eth_type_trans",
				kprobe_eth_type_trans_pre, NULL);
	hook_kprobe(&kprobe_napi_gro_receive, "napi_gro_receive",
				kprobe_napi_gro_receive_pre, NULL);
	hook_kprobe(&kprobe___netif_receive_skb_core, "__netif_receive_skb_core",
				kprobe___netif_receive_skb_core_pre, NULL);
	hook_kprobe(&kprobe_tcp_v4_rcv, "tcp_v4_rcv",
				kprobe_tcp_v4_rcv_pre, NULL);

	return 1;
}

static void deactivate_drop_packet(void)
{
	unhook_tracepoint("net_dev_xmit", trace_net_dev_xmit_hit, NULL);
	unhook_kprobe(&kprobe_eth_type_trans);
	unhook_kprobe(&kprobe_napi_gro_receive);
	unhook_kprobe(&kprobe___netif_receive_skb_core);
	unhook_kprobe(&kprobe_tcp_v4_rcv);
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

	if (drop_packet_activated)
		deactivate_drop_packet();
	drop_packet_activated = 0;

	printk("drop-packet unloaded.\n");
}

module_init(drop_packet_init)
module_exit(drop_packet_exit)

MODULE_DESCRIPTION("Baoyou Xie's drop-packet module");
MODULE_AUTHOR("Baoyou Xie <baoyou.xie@gmail.com>");
MODULE_LICENSE("GPL v2");
