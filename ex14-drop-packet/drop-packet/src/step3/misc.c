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
