/**
 * Baoyou Xie's drop-packet module
 *
 * Copyright (C) 2018 Baoyou Xie.
 *
 * Author: Baoyou Xie <baoyou.xie@gmail.com>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#include <linux/kprobes.h>

int hook_kprobe(struct kprobe *kp, const char *name,
		kprobe_pre_handler_t pre, kprobe_post_handler_t post);
void unhook_kprobe(struct kprobe *kp);
int hook_tracepoint(const char *name, void *probe, void *data);
int unhook_tracepoint(const char *name, void *probe, void *data);

unsigned int ipstr2int(const char *ipstr);
char *int2ipstr(const unsigned int ip, char *ipstr, const unsigned int ip_str_len);
