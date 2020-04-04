#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/tracepoint.h>

#include "drop_packet.h"

int hook_kprobe(struct kprobe *kp, const char *name,
		kprobe_pre_handler_t pre, kprobe_post_handler_t post)
{
	kprobe_opcode_t *addr;

	if (!name || strlen(name) >= 255)
		return -EINVAL;
	addr = (kprobe_opcode_t *)kallsyms_lookup_name(name);
	if (!addr)
		return -EINVAL;

	memset(kp, 0, sizeof(struct kprobe));
	kp->symbol_name = name;
	kp->pre_handler = pre;
	kp->post_handler = post;

	register_kprobe(kp);

	return 0;
}

void unhook_kprobe(struct kprobe *kp)
{
	unregister_kprobe(kp);
	memset(kp, 0, sizeof(struct kprobe));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
int hook_tracepoint(const char *name, void *probe, void *data)
{
	return tracepoint_probe_register(name, probe);
}

int unhook_tracepoint(const char *name, void *probe, void *data)
{
	return tracepoint_probe_unregister(name, probe);
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
int hook_tracepoint(const char *name, void *probe, void *data)
{
	return tracepoint_probe_register(name, probe, data);
}

int unhook_tracepoint(const char *name, void *probe, void *data)
{
	return tracepoint_probe_unregister(name, probe, data);
}
#else
static struct tracepoint *tp_ret;
static void probe_tracepoint(struct tracepoint *tp, void *priv)
{
	char *n = priv;

	if (strcmp(tp->name, n) == 0)
		tp_ret = tp;
}

static struct tracepoint *find_tracepoint(const char *name)
{
	tp_ret = NULL;
	for_each_kernel_tracepoint(probe_tracepoint, (void *)name);

	return tp_ret;
}

int hook_tracepoint(const char *name, void *probe, void *data)
{
	struct tracepoint *tp;

	tp = find_tracepoint(name);
	if (!tp)
		return 0;

	return tracepoint_probe_register(tp, probe, data);
}

int unhook_tracepoint(const char *name, void *probe, void *data)
{
	struct tracepoint *tp;

	tp = find_tracepoint(name);
	if (!tp)
		return 0;

	return tracepoint_probe_unregister(tp, probe, data);
}
#endif

#define IP_STR_LEN 18
#define MAC_STR_LEN 18
#define MAC_BIT_LEN 6
#define LITTLE_ENDIAN 0
#define BIG_ENDIAN 1

int big_little_endian(void)
{
	int data = 0x1;

	if (*((char *)&data) == 0x1)
		return LITTLE_ENDIAN;

	return BIG_ENDIAN;
}

unsigned int ipstr2int(const char *ipstr)
{
	unsigned int a, b, c, d;
	unsigned int ip = 0;
	int count;
	
	count = sscanf(ipstr, "%u.%u.%u.%u", &a, &b, &c, &d);
	if (count == 4) {
		a = (a << 24);
		b = (b << 16);
		c = (c << 8);
		d = (d << 0);
		ip = a | b | c | d;

		return ip;
	} else {
		return 0;
	}
}

char *int2ipstr(const unsigned int ip, char *ipstr, const unsigned int ip_str_len)
{
	int len;

	if (big_little_endian() == LITTLE_ENDIAN)
		len = snprintf(ipstr, ip_str_len, "%u.%u.%u.%u",
				(unsigned char) * ((char *)(&ip) + 3),
				(unsigned char) * ((char *)(&ip) + 2),
				(unsigned char) * ((char *)(&ip) + 1),
				(unsigned char) * ((char *)(&ip) + 0));
	else
		len = snprintf(ipstr, ip_str_len, "%u.%u.%u.%u",
				(unsigned char) * ((char *)(&ip) + 0),
				(unsigned char) * ((char *)(&ip) + 1),
				(unsigned char) * ((char *)(&ip) + 2),
				(unsigned char) * ((char *)(&ip) + 3));

	if (len < ip_str_len)
		return ipstr;
	else
		return NULL;
}
