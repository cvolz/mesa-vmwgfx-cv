#ifndef _COMMON_COMPAT_
#define _COMMON_COMPAT_

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/rcupdate.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

/*
 * For non-RHEL distros, set major and minor to 0
 */
#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#define RHEL_MAJOR 0
#define RHEL_MINOR 0
#endif

#define RHEL_VERSION_CODE RHEL_RELEASE_VERSION(RHEL_MAJOR, RHEL_MINOR)

#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(_sym)
#undef EXPORT_SYMBOL_GPL
#define EXPORT_SYMBOL_GPL(_sym)

/*
 * This function was at the beginning not available in
 * older kernels, unfortunately it was backported as a security
 * fix and as such was applied to a multitude of older kernels.
 * There is no reliable way of detecting if the kernel have the
 * fix or not, so we just uncoditionally do this workaround.
 */
#define kref_get_unless_zero vmwgfx_standalone_kref_get_unless_zero

static inline int __must_check kref_get_unless_zero(struct kref *kref)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	return refcount_inc_not_zero(&kref->refcount);
#else
	return atomic_add_unless(&kref->refcount, 1, 0);
#endif
}

/* lockdep_assert_held_once appeared in 3.18 */
#ifndef lockdep_assert_held_once
#ifdef CONFIG_LOCKDEP
#define lockdep_assert_held_once (l) do {				\
		WARN_ON_ONCE(debug_locks && !lockdep_is_held(l));	\
	} while (0)
#else
#define lockdep_assert_held_once(l)             do { (void)(l); } while (0)
#endif /* !LOCKDEP */
#endif /* !lockdep_assert_held_once */

#ifndef list_next_entry
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)
#endif
#ifndef list_prev_entry
#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)
#endif

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

#ifndef kvfree
#define kvfree drm_kvfree
static inline void kvfree(const void *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37))
#ifndef vzalloc
#define vzalloc drm_vzalloc
static inline void *vzalloc(unsigned long size)
{
	void *ptr = vmalloc(size);

	if (!ptr)
		return NULL;

	memset(ptr, 0, size);
	return ptr;
}
#endif
#endif

/**
 * kfree_rcu appears as a macro in v2.6.39.
 * This version assumes (and checks) that the struct rcu_head is the
 * first member in the structure about to be freed, so that we can just
 * call kfree directly on the rcu_head member.
 */
#ifndef kfree_rcu
#define kfree_rcu(_obj, _rcu_head)					\
	do {								\
		BUILD_BUG_ON(offsetof(typeof(*(_obj)), _rcu_head) != 0); \
		call_rcu(&(_obj)->_rcu_head, (void *)kfree);		\
	} while (0)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
#define kmalloc_array(_n, _size, _gfp)		\
	kmalloc((_n) * (_size), _gfp)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36))
#define VMWGFX_COMPAT_NO_VAF
#define drm_ut_debug_printk(__fname, __fmt, ...)			\
	printk(KERN_DEBUG  "[" DRM_NAME "]:%s " __fmt, __fname, ##__VA_ARGS__)
#define drm_err(__fmt, ...)						\
	printk(KERN_ERR  "[" DRM_NAME "] " __fmt, ##__VA_ARGS__)

extern __printf(6, 7)
void drm_dev_printk(const struct device *dev, const char *level,
		    unsigned int category, const char *function_name,
		    const char *prefix, const char *format, ...);
extern __printf(3, 4)
void drm_printk(const char *level, unsigned int category,
		const char *format, ...);
#endif

/* Ignore the annotation if not present */
#ifndef __rcu
#define __rcu
#endif

#ifndef rcu_pointer_handoff
#define rcu_pointer_handoff(p) (p)
#endif

#endif
