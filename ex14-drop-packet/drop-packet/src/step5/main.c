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
#include <linux/udp.h>
#include <linux/tcp.h>

#include "drop_packet.h"

static unsigned long drop_packet_activated;

static unsigned int drop_patcket_saddr = 0;
static unsigned int drop_patcket_sport = 0;
static unsigned int drop_patcket_daddr = 0;
static unsigned int drop_patcket_dport = 0;

static struct rb_root drop_packet_tree = RB_ROOT;
static LIST_HEAD(drop_packet_list);
static DEFINE_SPINLOCK(drop_packet_lock);

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

static char *packet_footprint_str[TRACK_COUNT] = {
	"ETH_RECV",
	"GRO_RECV",
	"RECV_SKB",
	"TCP_V4_RCV",
	"SEND_SKB",
};

struct conn_desc
{
	int protocol;
	int saddr;
	int sport;
	int daddr;
	int dport;

	atomic64_t packages[TRACK_COUNT];

	struct rb_node rb_node;
	struct list_head list;
};

__maybe_unused int compare_conn(__u8 protocol,
	unsigned int saddr,
	unsigned int sport,
	unsigned int daddr,
	unsigned int dport,
	struct conn_desc *conn)
{
	if (protocol < conn->protocol)
		return -1;
	if (protocol > conn->protocol)
		return 1;
	if (saddr < conn->saddr)
		return -1;
	if (saddr > conn->saddr)
		return 1;
	if (sport < conn->sport)
		return -1;
	if (sport > conn->sport)
		return 1;
	if (daddr < conn->daddr)
		return -1;
	if (daddr > conn->daddr)
		return 1;
	if (dport < conn->dport)
		return -1;
	if (dport > conn->dport)
		return 1;

	return 0;
}

__maybe_unused static struct conn_desc *__find_alloc_desc(struct rb_root *root,
	__u8	protocol,
	unsigned int saddr,
	unsigned int sport,
	unsigned int daddr,
	unsigned int dport)
{
	struct rb_node **node, *parent;
	struct conn_desc *conn_desc = NULL;

	node = &root->rb_node;
	parent = NULL;
	while (*node != NULL) {
		struct conn_desc *this;
		int ret = 0;

		parent = *node;
		this = container_of(parent, struct conn_desc, rb_node);
		ret = compare_conn(protocol, saddr, sport, daddr, dport, this);

		if (ret < 0)
			node = &parent->rb_left;
		else if (ret > 0)
			node = &parent->rb_right;
		else
			return this;
	}

	conn_desc = kmalloc(sizeof(struct conn_desc), GFP_ATOMIC | __GFP_ZERO);
	if (conn_desc) {
		conn_desc->protocol = protocol;
		conn_desc->saddr = saddr;
		conn_desc->sport = sport;
		conn_desc->daddr = daddr;
		conn_desc->dport = dport;

		INIT_LIST_HEAD(&conn_desc->list);
		list_add_tail(&conn_desc->list, &drop_packet_list);
		rb_link_node(&conn_desc->rb_node, parent, node);
		rb_insert_color(&conn_desc->rb_node, root);
	}

	return conn_desc;
}

__maybe_unused static struct conn_desc *find_alloc_desc(__u8 protocol,
	unsigned int saddr,
	unsigned int sport,
	unsigned int daddr,
	unsigned int dport)
{
	unsigned long flags;
	struct conn_desc *ret;

	spin_lock_irqsave(&drop_packet_lock, flags);
	ret = __find_alloc_desc(&drop_packet_tree, protocol,
		saddr, sport, daddr, dport);
	spin_unlock_irqrestore(&drop_packet_lock, flags);

	return ret;
}

static void trace_packet(const struct sk_buff *skb,
	const struct iphdr *iphdr, enum packet_footprint fp)
{
	int source = 0;
	int dest = 0;
	struct conn_desc *conn_desc;

	if (fp >= TRACK_COUNT)
		return;

	if (iphdr->protocol == IPPROTO_UDP)
	{
		struct udphdr *uh;

		uh = (void *)iphdr + iphdr->ihl * 4;
		source = be16_to_cpu(uh->source);
		dest = be16_to_cpu(uh->dest);
	}
	else if (iphdr->protocol == IPPROTO_TCP)
	{
		struct tcphdr *th;

		th = (void *)iphdr + iphdr->ihl * 4;
		source = be16_to_cpu(th->source);
		dest = be16_to_cpu(th->dest);
	} else
		return;

	conn_desc = find_alloc_desc(iphdr->protocol,
		iphdr->saddr, source, iphdr->daddr, dest);
	if (!conn_desc)
		return;

	atomic64_inc(&conn_desc->packages[fp]);
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
	struct conn_desc *this;
	unsigned long flags;
	struct list_head header;
	int i;

	seq_printf(m, "settings:\n");
	seq_printf(m, "  activated: %s\n", drop_packet_activated ? "y" : "N");

	INIT_LIST_HEAD(&header);
	spin_lock_irqsave(&drop_packet_lock, flags);
	drop_packet_tree = RB_ROOT;
	list_splice_init(&drop_packet_list, &header);
	spin_unlock_irqrestore(&drop_packet_lock, flags);

	synchronize_sched();

	list_for_each_entry(this, &header, list)
	{
		unsigned char *saddr = (void *)&this->saddr;
		unsigned char *daddr = (void *)&this->daddr;

		seq_printf(m,
				"protocol：%s, Source IP: %u.%u.%u.%u, Source Port: %d, "
				"Dest IP: %u.%u.%u.%u, Dest Port: %d\n",
				this->protocol == IPPROTO_UDP ? "UDP" : "TCP",
				saddr[0], saddr[1], saddr[2], saddr[3], this->sport,
				daddr[0], daddr[1], daddr[2], daddr[3], this->dport);
		for (i = 0; i < TRACK_COUNT; i++) {
			seq_printf(m,
				"    %s: %lu\n",
				packet_footprint_str[i],
				atomic64_read(&this->packages[i]));
		}
	}

	while (!list_empty(&header))
	{
		struct conn_desc *this = list_first_entry(&header,
			struct conn_desc, list);

		list_del_init(&this->list);
		kfree(this);
	}

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
	}  else if (strcmp(cmd, "source-addr") == 0) {
		char addr[255];

		ret = sscanf(chr, "%255s %255s", cmd, addr);
		if (ret == 2) {
			drop_patcket_saddr = ipstr2int(addr);
		} else if (ret == 1) {
			drop_patcket_saddr = 0;
		}
	} else if (strcmp(cmd, "source-port") == 0) {
		unsigned int port;

		ret = sscanf(chr, "%255s %255d", cmd, &port);
		if (ret == 2) {
			drop_patcket_sport = port;
		} else if (ret == 1) {
			drop_patcket_sport = 0;
		}
	} else if (strcmp(cmd, "dest-addr") == 0) {
		char addr[255];

		ret = sscanf(chr, "%255s %255s", cmd, addr);
		if (ret == 2) {
			drop_patcket_daddr = ipstr2int(addr);
		} else if (ret == 1) {
			drop_patcket_daddr = 0;
		}
	} else if (strcmp(cmd, "dest-port") == 0) {
		unsigned int port;

		ret = sscanf(chr, "%255s %255d", cmd, &port);
		if (ret == 2) {
			drop_patcket_dport = port;
		} else if (ret == 1) {
			drop_patcket_dport = 0;
		}
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
