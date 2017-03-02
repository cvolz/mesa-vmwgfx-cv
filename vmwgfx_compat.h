/*
 * Copyright (C) 2009 VMware, Inc., Palo Alto, CA., USA
 *  
 * This file is put together using various bits from the linux kernel.
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive
 * for more details.
 */


/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com> and others.
 *
 * Compatibility defines to make it possible to use the standalone
 * vmwgfx driver in newer kernels.
 */

#ifndef _VMWGFX_COMPAT_H_
#define _VMWGFX_COMPAT_H_

#include "common_compat.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/rcupdate.h>
#ifndef CONFIG_FB_DEFERRED_IO
#include <linux/fb.h>
#endif
#include <linux/scatterlist.h>
#include <linux/time.h>
#include <linux/pfn_t.h>
#include "core/dma-fence.h"

#include "drm_cache.h"
#include "ttm/ttm_page_alloc.h"
#include "ttm/ttm_object.h"

extern int ttm_init(void);
extern void ttm_exit(void);

/**
 * Handover available?
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31))
#define VMWGFX_HANDOVER
#endif

/** 
 * getrawmonotonic
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28))
static inline void getrawmonotonic(struct timespec *ts)
{
	*ts = current_kernel_time();
}
#endif

/**
 * timespec_sub
 */

static inline struct timespec __vmw_timespec_sub(struct timespec t1,
						 struct timespec t2)
{
	t1.tv_sec -= t2.tv_sec;
	if (t2.tv_nsec > t1.tv_nsec) {
		t1.tv_nsec += (1000000000L - t2.tv_nsec);
		t1.tv_sec -= 1L;
	} else
		t1.tv_nsec -= t2.tv_nsec;
	
	return t1;
}

#undef timespec_sub
#define timespec_sub(_t1, _t2) \
	__vmw_timespec_sub(_t1, _t2)

/* 
 * dev_set_name 
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30))
#define dev_set_name(_dev, _name) \
	({snprintf((_dev)->bus_id, BUS_ID_SIZE, (_name)); 0;})
#endif

/*
 * ioremap_wc
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#undef ioremap_wc
#define ioremap_wc(_offset, _size) \
	ioremap_nocache(_offset, _size)
#endif

/*
 * kmap_atomic_prot
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
#undef kmap_atomic_prot
#define kmap_atomic_prot(_page, _km_type, _prot) \
	kmap_atomic(_page, _km_type)
#endif

/*
 * set_memory_wc
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
#undef set_memory_wc
#define set_memory_wc(_pa, _num) \
	set_memory_uc(_pa, _num)
#endif

/*
 * shmem_file_setup
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28))
#undef shmem_file_setup
#define shmem_file_setup(_fname, _size, _dummy) \
	(ERR_PTR(-ENOMEM))
#endif

/*
 * vm_insert_mixed
 */ 
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
#define vm_insert_mixed(_vma, _addr, _pfn) \
	vm_insert_pfn(_vma, _addr, _pfn)
#undef VM_MIXEDMAP
#define VM_MIXEDMAP VM_PFNMAP
#endif

/*
 * fault vs nopfn
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#define TTM_HAVE_NOPFN
#define VMWGFX_HAVE_NOPFN
#endif

/*
 * pgprot_writecombine
 */
#if defined(__i386__) || defined(__x86_64__)
#undef pgprot_writecombine
#define pgprot_writecombine(_prot) \
	pgprot_noncached(_prot)
#endif

/*
 * const vm_operations_struct
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
#define TTM_HAVE_CVOS
#endif

/*
 * const sysfs_ops
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
#define TTM_HAVE_CSO
#endif

/*
 * list_cut_position
 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,27))
static inline void __vmwgfx_lcp(struct list_head *list,
		struct list_head *head, struct list_head *entry)
{
	struct list_head *new_first = entry->next;
	list->next = head->next;
	list->next->prev = list;
	list->prev = entry;
	entry->next = list;
	head->next = new_first;
	new_first->prev = head;
}

static inline void vmwgfx_lcp(struct list_head *list,
		struct list_head *head, struct list_head *entry)
{
	if (list_empty(head))
		return;
	if ((head->next == head->prev) &&
		(head->next != entry && head != entry))
		return;
	if (entry == head)
		INIT_LIST_HEAD(list);
	else
		__vmwgfx_lcp(list, head, entry);
}

#undef list_cut_position
#define list_cut_position(_list, _head, _entry) \
  vmwgfx_lcp(_list, _head, _entry)
#endif

/**
 * set_pages_array_wc
 * No caching attributes on vmwgfx.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(6, 5))
static inline int set_pages_array_wc(struct page **pages, int addrinarray)
{
	return 0;
}
#endif

/**
 * set_pages_array_uc and set_pages_array_wb()
 * No caching attributes on vmwgfx.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30))
static inline int set_pages_array_uc(struct page **pages, int addrinarray)
{
	return 0;
}

static inline int set_pages_array_wb(struct page **pages, int addrinarray)
{
	return 0;
}
#endif

/**
 * CONFIG_FB_DEFERRED_IO might not be defined. Yet we rely on it.
 * Thus, provide a copy of the implementation in vmw_defio.c.
 * Declarations and prototypes go here.
 */

#ifndef CONFIG_FB_DEFERRED_IO
struct vmw_fb_deferred_par;
struct vmw_fb_deferred_io {
	/* delay between mkwrite and deferred handler */
	unsigned long delay;
	struct mutex lock; /* mutex that protects the page list */
	struct list_head pagelist; /* list of touched pages */
	/* callback */
	void (*deferred_io)(struct vmw_fb_deferred_par *par,
			    struct list_head *pagelist);
};

struct vmw_fb_deferred_par {
	struct delayed_work deferred_work;
	struct vmw_fb_deferred_io *fbdefio;
	struct fb_info *info;
};

#define fb_deferred_io_init(_info) \
	vmw_fb_deferred_io_init(_info)
#define fb_deferred_io_cleanup(_info) \
	vmw_fb_deferred_io_cleanup(_info)

extern void vmw_fb_deferred_io_init(struct fb_info *info);
extern void vmw_fb_deferred_io_cleanup(struct fb_info *info);

#define VMWGFX_FB_DEFERRED
#endif

/**
 * Power management
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29))
#define dev_pm_ops pm_ops
#endif

/**
 * kref_sub
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
static inline int vmwgfx_kref_sub(struct kref *kref, unsigned int count,
				  void (*release)(struct kref *kref))
{
	if (atomic_sub_and_test((int) count, &kref->refcount)) {
		release(kref);
		return 1;
	}
	return 0;
}
#define kref_sub(_a, _b, _c) vmwgfx_kref_sub(_a, _b, _c)
#endif

/**
 * kmap_atomic
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#define VMW_HAS_STACK_KMAP_ATOMIC
#else
static inline void *kmap_atomic_drm(struct page *page)
{
	return kmap_atomic(page, KM_USER0);
}

static inline void *kmap_atomic_prot_drm(struct page *page, pgprot_t prot)
{
	return kmap_atomic_prot(page, KM_USER0, prot);
}

static inline void kunmap_atomic_drm(void *addr)
{
	kunmap_atomic(addr, KM_USER0);
}

#undef kmap_atomic
#define kmap_atomic kmap_atomic_drm
#undef kmap_atomic_prot
#define kmap_atomic_prot kmap_atomic_prot_drm
#undef kunmap_atomic
#define kunmap_atomic kunmap_atomic_drm

#endif

/**
 * shmem_read_mapping_page appeared in 3.0-rc5
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(6, 5))
#define shmem_read_mapping_page(_a, _b) read_mapping_page(_a, _b, NULL)
#endif

/**
 * VM_RESERVED disappeared in 3.7, and is replaced in upstream
 * ttm_bo_vm.c with VM_DONTDUMP. Try to keep code in sync with
 * upstream
 */
#ifndef VM_DONTDUMP
#define VM_DONTDUMP VM_RESERVED
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(6, 5))
#ifdef CONFIG_SWIOTLB
#define swiotlb_nr_tbl() (1)
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
int sg_alloc_table_from_pages(struct sg_table *sgt,
			      struct page **pages, unsigned int n_pages,
			      unsigned long offset, unsigned long size,
			      gfp_t gfp_mask);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(6, 8))
/*
 * sg page iterator
 *
 * Iterates over sg entries page-by-page.  On each successful iteration,
 * you can call sg_page_iter_page(@piter) and sg_page_iter_dma_address(@piter)
 * to get the current page and its dma address. @piter->sg will point to the
 * sg holding this page and @piter->sg_pgoffset to the page's page offset
 * within the sg. The iteration will stop either when a maximum number of sg
 * entries was reached or a terminating sg (sg_last(sg) == true) was reached.
 */
struct sg_page_iter {
	struct scatterlist	*sg;		/* sg holding the page */
	unsigned int		sg_pgoffset;	/* page offset within the sg */

	/* these are internal states, keep away */
	unsigned int		__nents;	/* remaining sg entries */
	int			__pg_advance;	/* nr pages to advance at the
						 * next step */
};

bool __sg_page_iter_next(struct sg_page_iter *piter);
void __sg_page_iter_start(struct sg_page_iter *piter,
			  struct scatterlist *sglist, unsigned int nents,
			  unsigned long pgoffset);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(6, 8))
/**
 * sg_page_iter_page - get the current page held by the page iterator
 * @piter:	page iterator holding the page
 */
static inline struct page *sg_page_iter_page(struct sg_page_iter *piter)
{
	return nth_page(sg_page(piter->sg), piter->sg_pgoffset);
}

/**
 * sg_page_iter_dma_address - get the dma address of the current page held by
 * the page iterator.
 * @piter:	page iterator holding the page
 */
static inline dma_addr_t sg_page_iter_dma_address(struct sg_page_iter *piter)
{
	return sg_dma_address(piter->sg) + (piter->sg_pgoffset << PAGE_SHIFT);
}
#endif

/*
 * Minimal implementation of the dma-buf stuff
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
#ifndef __DMA_BUF_H__
#define __DMA_BUF_H__
#define DMA_BUF_STANDALONE

#include <linux/dma-mapping.h>
struct dma_buf_attachment;

struct dma_buf {
	struct file *file;
	const struct dma_buf_ops *ops;
	void *priv;
};

struct dma_buf_ops {
	int (*attach)(struct dma_buf *, struct device *,
			struct dma_buf_attachment *);

	void (*detach)(struct dma_buf *, struct dma_buf_attachment *);

	struct sg_table * (*map_dma_buf)(struct dma_buf_attachment *,
						enum dma_data_direction);
	void (*unmap_dma_buf)(struct dma_buf_attachment *,
						struct sg_table *,
						enum dma_data_direction);
	/* after final dma_buf_put() */
	void (*release)(struct dma_buf *);

	int (*begin_cpu_access)(struct dma_buf *, size_t, size_t,
				enum dma_data_direction);
	void (*end_cpu_access)(struct dma_buf *, size_t, size_t,
			       enum dma_data_direction);
	void *(*kmap_atomic)(struct dma_buf *, unsigned long);
	void (*kunmap_atomic)(struct dma_buf *, unsigned long, void *);
	void *(*kmap)(struct dma_buf *, unsigned long);
	void (*kunmap)(struct dma_buf *, unsigned long, void *);
};

struct dma_buf *dma_buf_export(void *priv, const struct dma_buf_ops *ops,
			       size_t size, int flags);

void dma_buf_put(struct dma_buf *dmabuf);

struct dma_buf *dma_buf_get(int fd);

static inline void get_dma_buf(struct dma_buf *dmabuf)
{
	get_file(dmabuf->file);
}

int dma_buf_fd(struct dma_buf *dmabuf, int flags);

#endif /* __DMA_BUF_H__ */

#else

#include <linux/dma-buf.h>

#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
#define U32_MAX ((u32)~0U)
#define U16_MAX ((u16)~0U)
#define S32_MAX ((s32)(U32_MAX>>1))
#define S32_MIN ((s32)(-S32_MAX - 1))
#endif

/* set_need_resched() disappeared in linux 3.16. Temporary fix. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
#define set_need_resched()
#endif

/* set_mb__[before|after]_atomic appeared in 3.16 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(7, 3))
#define smp_mb__before_atomic() smp_mb__before_atomic_inc()
#define smp_mb__after_atomic() smp_mb__after_atomic_inc()
#endif

/* memremap appeared in 4.3 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(7, 3))
#define memremap(_offset, _size, _flag)		\
	(void __force *)ioremap_cache(_offset, _size)
#define memunmap(_addr)				\
	iounmap((void __iomem *) _addr)
#endif

/*
 * pfn_t was introduced in 4.5, and also the pfn flag PFN_DEV
 */
#ifndef PFN_DEV
#define __pfn_to_pfn_t(__pfn, __dummy) (__pfn)
#define PFN_DEV (1UL << (BITS_PER_LONG - 3))
#endif

/* __get_user_pages_unlocked() appeared in 4.0 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0) && \
     RHEL_VERSION_CODE < RHEL_RELEASE_VERSION(7, 3))
static inline long
__get_user_pages_unlocked(struct task_struct *tsk, struct mm_struct *mm,
			  unsigned long start, unsigned long nr_pages,
			  int write, int force, struct page **pages,
			  unsigned int gup_flags)
{
	long ret;

	down_read(&mm->mmap_sem);
	ret = get_user_pages(tsk, mm, start, nr_pages, write, force, pages,
			     NULL);
	up_read(&mm->mmap_sem);
	return ret;
}
#endif

#define drm_clflush_pages(_a, _b)					\
	WARN_ONCE(true, "drm_clflush_pages() illegal call.\n")

/* Never take the FAULT_FLAG_RETRY_NOWAIT path if it's not supported. */
#ifndef FAULT_FLAG_RETRY_NOWAIT
#define FAULT_FLAG_RETRY_NOWAIT 0
#endif
#ifndef FAULT_FLAG_ALLOW_RETRY
#define FAULT_FLAG_ALLOW_RETRY 0
#define VM_FAULT_RETRY ({ BUG(); 0;})
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
#define file_inode(_f) (_f)->f_path.dentry->d_inode
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0))
static inline u64 ktime_get_raw_ns(void)
{
	struct timespec ts;
	
	getrawmonotonic(&ts);
	return timespec_to_ns(&ts);
}
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
#include <linux/shrinker.h>
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0))
struct shrink_control {
	gfp_t gfp_mask;
	unsigned long nr_to_scan;
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0))
#define SHRINK_STOP (~0UL)

static inline int __ttm_compat_shrink(struct shrinker *shrink,
				      unsigned long (*count_objects)
				      (struct shrinker *,
				       struct shrink_control *),
				      unsigned long (*scan_objects)
				      (struct shrinker *,
				       struct shrink_control *),
				      int nr_to_scan,
				      gfp_t gfp_mask)
{
	unsigned long ret;
	struct shrink_control sc;

	sc.gfp_mask = gfp_mask;
	sc.nr_to_scan = nr_to_scan;

	if (nr_to_scan != 0) {
		ret = scan_objects(shrink, &sc);
		if (ret == SHRINK_STOP)
			return -1;
	}
	return (int) count_objects(shrink, &sc);
}

#define TTM_STANDALONE_DEFINE_COMPAT_SHRINK(__co, __so)			\
	static int ttm_compat_shrink(struct shrinker *shrink,		\
				     int nr_to_scan,			\
				     gfp_t gfp_mask)			\
{									\
	return __ttm_compat_shrink(shrink, __co, __so, nr_to_scan,	\
				   gfp_mask);				\
}									\

#define TTM_STANDALONE_COMPAT_SHRINK ttm_compat_shrink
#else
#define TTM_STANDALONE_DEFINE_COMPAT_SHRINK(__co, __so)
#endif

#endif
