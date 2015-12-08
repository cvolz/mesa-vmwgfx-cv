/**
 * \file drm_bufs.c
 * Generic buffer template
 *
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/vmalloc.h>
#include <linux/log2.h>
#include <asm/shmparam.h>
#include "drmP.h"

resource_size_t drm_get_resource_start(struct drm_device *dev, unsigned int resource)
{
	return pci_resource_start(dev->pdev, resource);
}
EXPORT_SYMBOL(drm_get_resource_start);

resource_size_t drm_get_resource_len(struct drm_device *dev, unsigned int resource)
{
	return pci_resource_len(dev->pdev, resource);
}

EXPORT_SYMBOL(drm_get_resource_len);

static struct drm_map_list *drm_find_matching_map(struct drm_device *dev,
						  struct drm_local_map *map)
{
	struct drm_map_list *entry;
	list_for_each_entry(entry, &dev->maplist, head) {
		/*
		 * Because the kernel-userspace ABI is fixed at a 32-bit offset
		 * while PCI resources may live above that, we ignore the map
		 * offset for maps of type _DRM_FRAMEBUFFER or _DRM_REGISTERS.
		 * It is assumed that each driver will have only one resource of
		 * each type.
		 */
		if (!entry->map ||
		    map->type != entry->map->type ||
		    entry->master != dev->primary->master)
			continue;
		switch (map->type) {
		case _DRM_SHM:
			if (map->flags != _DRM_CONTAINS_LOCK)
				break;
		case _DRM_REGISTERS:
		case _DRM_FRAME_BUFFER:
			return entry;
		default: /* Make gcc happy */
			;
		}
		if (entry->map->offset == map->offset)
			return entry;
	}

	return NULL;
}

static int drm_map_handle(struct drm_device *dev, struct drm_hash_item *hash,
			  unsigned long user_token, int hashed_handle, int shm)
{
	int use_hashed_handle, shift;
	unsigned long add;

#if (BITS_PER_LONG == 64)
	use_hashed_handle = ((user_token & 0xFFFFFFFF00000000UL) || hashed_handle);
#elif (BITS_PER_LONG == 32)
	use_hashed_handle = hashed_handle;
#else
#error Unsupported long size. Neither 64 nor 32 bits.
#endif

	if (!use_hashed_handle) {
		int ret;
		hash->key = user_token >> PAGE_SHIFT;
		ret = drm_ht_insert_item(&dev->map_hash, hash);
		if (ret != -EINVAL)
			return ret;
	}

	shift = 0;
	add = DRM_MAP_HASH_OFFSET >> PAGE_SHIFT;
	if (shm && (SHMLBA > PAGE_SIZE)) {
		int bits = ilog2(SHMLBA >> PAGE_SHIFT) + 1;

		/* For shared memory, we have to preserve the SHMLBA
		 * bits of the eventual vma->vm_pgoff value during
		 * mmap().  Otherwise we run into cache aliasing problems
		 * on some platforms.  On these platforms, the pgoff of
		 * a mmap() request is used to pick a suitable virtual
		 * address for the mmap() region such that it will not
		 * cause cache aliasing problems.
		 *
		 * Therefore, make sure the SHMLBA relevant bits of the
		 * hash value we use are equal to those in the original
		 * kernel virtual address.
		 */
		shift = bits;
		add |= ((user_token >> PAGE_SHIFT) & ((1UL << bits) - 1UL));
	}

	return drm_ht_just_insert_please(&dev->map_hash, hash,
					 user_token, 32 - PAGE_SHIFT - 3,
					 shift, add);
}

/**
 * Core function to create a range of memory available for mapping by a
 * non-root process.
 *
 * Adjusts the memory offset to its absolute value according to the mapping
 * type.  Adds the map to the map list drm_device::maplist. Adds MTRR's where
 * applicable and if supported by the kernel.
 */
static int drm_addmap_core(struct drm_device * dev, resource_size_t offset,
			   unsigned int size, enum drm_map_type type,
			   enum drm_map_flags flags,
			   struct drm_map_list ** maplist)
{
	struct drm_local_map *map;
	struct drm_map_list *list;
	unsigned long user_token;
	int ret;

	map = kmalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	map->offset = offset;
	map->size = size;
	map->flags = flags;
	map->type = type;

	/* Only allow shared memory to be removable since we only keep enough
	 * book keeping information about shared memory to allow for removal
	 * when processes fork.
	 */
	if ((map->flags & _DRM_REMOVABLE) && map->type != _DRM_SHM) {
		kfree(map);
		return -EINVAL;
	}
	DRM_DEBUG("offset = 0x%08llx, size = 0x%08lx, type = %d\n",
		  (unsigned long long)map->offset, map->size, map->type);

	/* page-align _DRM_SHM maps. They are allocated here so there is no security
	 * hole created by that and it works around various broken drivers that use
	 * a non-aligned quantity to map the SAREA. --BenH
	 */
	if (map->type == _DRM_SHM)
		map->size = PAGE_ALIGN(map->size);

	if ((map->offset & (~(resource_size_t)PAGE_MASK)) || (map->size & (~PAGE_MASK))) {
		kfree(map);
		return -EINVAL;
	}
	map->mtrr = -1;
	map->handle = NULL;

	switch (map->type) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:
#if !defined(__sparc__) && !defined(__alpha__) && !defined(__ia64__) && !defined(__powerpc64__) && !defined(__x86_64__)
		if (map->offset + (map->size-1) < map->offset ||
		    map->offset < virt_to_phys(high_memory)) {
			kfree(map);
			return -EINVAL;
		}
#endif
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		/* Some drivers preinitialize some maps, without the X Server
		 * needing to be aware of it.  Therefore, we just return success
		 * when the server tries to create a duplicate map.
		 */
		list = drm_find_matching_map(dev, map);
		if (list != NULL) {
			if (list->map->size != map->size) {
				DRM_DEBUG("Matching maps of type %d with "
					  "mismatched sizes, (%ld vs %ld)\n",
					  map->type, map->size,
					  list->map->size);
				list->map->size = map->size;
			}

			kfree(map);
			*maplist = list;
			return 0;
		}

		if (map->type == _DRM_REGISTERS) {
			map->handle = (void __force *)ioremap(map->offset, map->size);
			if (!map->handle) {
				kfree(map);
				return -ENOMEM;
			}
		}

		break;
	case _DRM_SHM:
		list = drm_find_matching_map(dev, map);
		if (list != NULL) {
			if(list->map->size != map->size) {
				DRM_DEBUG("Matching maps of type %d with "
					  "mismatched sizes, (%ld vs %ld)\n",
					  map->type, map->size, list->map->size);
				list->map->size = map->size;
			}

			kfree(map);
			*maplist = list;
			return 0;
		}
		map->handle = vmalloc_user(map->size);
		DRM_DEBUG("%lu %d %p\n",
			  map->size, drm_order(map->size), map->handle);
		if (!map->handle) {
			kfree(map);
			return -ENOMEM;
		}
		map->offset = (unsigned long)map->handle;
		if (map->flags & _DRM_CONTAINS_LOCK) {
			/* Prevent a 2nd X Server from creating a 2nd lock */
			if (dev->primary->master->lock.hw_lock != NULL) {
				vfree(map->handle);
				kfree(map);
				return -EBUSY;
			}
		}
		break;
	case _DRM_AGP: {
		struct drm_agp_mem *entry;
		int valid = 0;

		if (!drm_core_has_AGP(dev)) {
			kfree(map);
			return -EINVAL;
		}
#ifdef __alpha__
		map->offset += dev->hose->mem_space->start;
#endif
		/* In some cases (i810 driver), user space may have already
		 * added the AGP base itself, because dev->agp->base previously
		 * only got set during AGP enable.  So, only add the base
		 * address if the map's offset isn't already within the
		 * aperture.
		 */
		if (map->offset < dev->agp->base ||
		    map->offset > dev->agp->base +
		    dev->agp->agp_info.aper_size * 1024 * 1024 - 1) {
			map->offset += dev->agp->base;
		}
		map->mtrr = dev->agp->agp_mtrr;	/* for getmap */

		/* This assumes the DRM is in total control of AGP space.
		 * It's not always the case as AGP can be in the control
		 * of user space (i.e. i810 driver). So this loop will get
		 * skipped and we double check that dev->agp->memory is
		 * actually set as well as being invalid before EPERM'ing
		 */
		list_for_each_entry(entry, &dev->agp->memory, head) {
			if ((map->offset >= entry->bound) &&
			    (map->offset + map->size <= entry->bound + entry->pages * PAGE_SIZE)) {
				valid = 1;
				break;
			}
		}
		if (!list_empty(&dev->agp->memory) && !valid) {
			kfree(map);
			return -EPERM;
		}
		DRM_DEBUG("AGP offset = 0x%08llx, size = 0x%08lx\n",
			  (unsigned long long)map->offset, map->size);

		break;
	}
	default:
		kfree(map);
		return -EINVAL;
	}

	list = kmalloc(sizeof(*list), GFP_KERNEL);
	if (!list) {
		if (map->type == _DRM_REGISTERS)
			iounmap((void __iomem *)map->handle);
		kfree(map);
		return -EINVAL;
	}
	memset(list, 0, sizeof(*list));
	list->map = map;

	mutex_lock(&dev->struct_mutex);
	list_add(&list->head, &dev->maplist);

	/* Assign a 32-bit handle */
	/* We do it here so that dev->struct_mutex protects the increment */
	user_token = (map->type == _DRM_SHM) ? (unsigned long)map->handle :
		map->offset;
	ret = drm_map_handle(dev, &list->hash, user_token, 0,
			     (map->type == _DRM_SHM));
	if (ret) {
		if (map->type == _DRM_REGISTERS)
			iounmap((void __iomem *)map->handle);
		kfree(map);
		kfree(list);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	list->user_token = list->hash.key << PAGE_SHIFT;
	mutex_unlock(&dev->struct_mutex);

	if (!(map->flags & _DRM_DRIVER))
		list->master = dev->primary->master;
	*maplist = list;
	return 0;
	}

int drm_addmap(struct drm_device * dev, resource_size_t offset,
	       unsigned int size, enum drm_map_type type,
	       enum drm_map_flags flags, struct drm_local_map ** map_ptr)
{
	struct drm_map_list *list;
	int rc;

	rc = drm_addmap_core(dev, offset, size, type, flags, &list);
	if (!rc)
		*map_ptr = list->map;
	return rc;
}

EXPORT_SYMBOL(drm_addmap);

/**
 * Ioctl to specify a range of memory that is available for mapping by a
 * non-root process.
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_map structure.
 * \return zero on success or a negative value on error.
 *
 */
int drm_addmap_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_map *map = data;
	struct drm_map_list *maplist;
	int err;

	if (!(capable(CAP_SYS_ADMIN) || map->type == _DRM_AGP || map->type == _DRM_SHM))
		return -EPERM;

	err = drm_addmap_core(dev, map->offset, map->size, map->type,
			      map->flags, &maplist);

	if (err)
		return err;

	/* avoid a warning on 64-bit, this casting isn't very nice, but the API is set so too late */
	map->handle = (void *)(unsigned long)maplist->user_token;
	return 0;
}

/**
 * Remove a map private from list and deallocate resources if the mapping
 * isn't in use.
 *
 * Searches the map on drm_device::maplist, removes it from the list, see if
 * its being used, and free any associate resource (such as MTRR's) if it's not
 * being on use.
 *
 * \sa drm_addmap
 */
int drm_rmmap_locked(struct drm_device *dev, struct drm_local_map *map)
{
	struct drm_map_list *r_list = NULL, *list_t;
	int found = 0;
	struct drm_master *master;

	/* Find the list entry for the map and remove it */
	list_for_each_entry_safe(r_list, list_t, &dev->maplist, head) {
		if (r_list->map == map) {
			master = r_list->master;
			list_del(&r_list->head);
			drm_ht_remove_key(&dev->map_hash,
					  r_list->user_token >> PAGE_SHIFT);
			kfree(r_list);
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	switch (map->type) {
	case _DRM_REGISTERS:
		iounmap((void __iomem *)map->handle);
		/* FALLTHROUGH */
	case _DRM_FRAME_BUFFER:
		break;
	case _DRM_SHM:
		vfree(map->handle);
		if (master) {
			master->lock.hw_lock = NULL;   /* SHM removed */
			master->lock.file_priv = NULL;
			wake_up_interruptible_all(&master->lock.lock_queue);
		}
		break;
	case _DRM_AGP:
		break;
	}
	kfree(map);

	return 0;
}
EXPORT_SYMBOL(drm_rmmap_locked);

int drm_rmmap(struct drm_device *dev, struct drm_local_map *map)
{
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_rmmap_locked(dev, map);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}
EXPORT_SYMBOL(drm_rmmap);

/* The rmmap ioctl appears to be unnecessary.  All mappings are torn down on
 * the last close of the device, and this is necessary for cleanup when things
 * exit uncleanly.  Therefore, having userland manually remove mappings seems
 * like a pointless exercise since they're going away anyway.
 *
 * One use case might be after addmap is allowed for normal users for SHM and
 * gets used by drivers that the server doesn't need to care about.  This seems
 * unlikely.
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a struct drm_map structure.
 * \return zero on success or a negative value on error.
 */
int drm_rmmap_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_map *request = data;
	struct drm_local_map *map = NULL;
	struct drm_map_list *r_list;
	int ret;

	mutex_lock(&dev->struct_mutex);
	list_for_each_entry(r_list, &dev->maplist, head) {
		if (r_list->map &&
		    r_list->user_token == (unsigned long)request->handle &&
		    r_list->map->flags & _DRM_REMOVABLE) {
			map = r_list->map;
			break;
		}
	}

	/* List has wrapped around to the head pointer, or its empty we didn't
	 * find anything.
	 */
	if (list_empty(&dev->maplist) || !map) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	/* Register and framebuffer maps are permanent */
	if ((map->type == _DRM_REGISTERS) || (map->type == _DRM_FRAME_BUFFER)) {
		mutex_unlock(&dev->struct_mutex);
		return 0;
	}

	ret = drm_rmmap_locked(dev, map);

	mutex_unlock(&dev->struct_mutex);

	return ret;
}

/**
 * Add buffers for DMA transfers (ioctl).
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a struct drm_buf_desc request.
 * \return zero on success or a negative number on failure.
 *
 * According with the memory type specified in drm_buf_desc::flags and the
 * build options, it dispatches the call either to addbufs_agp(),
 * addbufs_sg() or addbufs_pci() for AGP, scatter-gather or consistent
 * PCI memory respectively.
 */
int drm_addbufs(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	return -EINVAL;
}

/**
 * Get information about the buffer mappings.
 *
 * This was originally mean for debugging purposes, or by a sophisticated
 * client library to determine how best to use the available buffers (e.g.,
 * large buffers can be used for image transfer).
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_buf_info structure.
 * \return zero on success or a negative number on failure.
 *
 * Increments drm_device::buf_use while holding the drm_device::count_lock
 * lock, preventing of allocating more buffers after this call. Information
 * about each requested buffer is then copied into user space.
 */
int drm_infobufs(struct drm_device *dev, void *data,
		 struct drm_file *file_priv)
{
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf_info *request = data;
	int i;
	int count;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	spin_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		spin_unlock(&dev->count_lock);
		return -EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	spin_unlock(&dev->count_lock);

	for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
		if (dma->bufs[i].buf_count)
			++count;
	}

	DRM_DEBUG("count = %d\n", count);

	if (request->count >= count) {
		for (i = 0, count = 0; i < DRM_MAX_ORDER + 1; i++) {
			if (dma->bufs[i].buf_count) {
				struct drm_buf_desc __user *to =
				    &request->list[count];
				struct drm_buf_entry *from = &dma->bufs[i];
				struct drm_freelist *list = &dma->bufs[i].freelist;
				if (copy_to_user(&to->count,
						 &from->buf_count,
						 sizeof(from->buf_count)) ||
				    copy_to_user(&to->size,
						 &from->buf_size,
						 sizeof(from->buf_size)) ||
				    copy_to_user(&to->low_mark,
						 &list->low_mark,
						 sizeof(list->low_mark)) ||
				    copy_to_user(&to->high_mark,
						 &list->high_mark,
						 sizeof(list->high_mark)))
					return -EFAULT;

				DRM_DEBUG("%d %d %d %d %d\n",
					  i,
					  dma->bufs[i].buf_count,
					  dma->bufs[i].buf_size,
					  dma->bufs[i].freelist.low_mark,
					  dma->bufs[i].freelist.high_mark);
				++count;
			}
		}
	}
	request->count = count;

	return 0;
}

/**
 * Specifies a low and high water mark for buffer allocation
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg a pointer to a drm_buf_desc structure.
 * \return zero on success or a negative number on failure.
 *
 * Verifies that the size order is bounded between the admissible orders and
 * updates the respective drm_device_dma::bufs entry low and high water mark.
 *
 * \note This ioctl is deprecated and mostly never used.
 */
int drm_markbufs(struct drm_device *dev, void *data,
		 struct drm_file *file_priv)
{
	struct drm_device_dma *dma = dev->dma;
	struct drm_buf_desc *request = data;
	int order;
	struct drm_buf_entry *entry;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		return -EINVAL;

	if (!dma)
		return -EINVAL;

	DRM_DEBUG("%d, %d, %d\n",
		  request->size, request->low_mark, request->high_mark);
	order = drm_order(request->size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER)
		return -EINVAL;
	entry = &dma->bufs[order];

	if (request->low_mark < 0 || request->low_mark > entry->buf_count)
		return -EINVAL;
	if (request->high_mark < 0 || request->high_mark > entry->buf_count)
		return -EINVAL;

	entry->freelist.low_mark = request->low_mark;
	entry->freelist.high_mark = request->high_mark;

	return 0;
}

/**
 * Unreserve the buffers in list, previously reserved using drmDMA.
 *
 * \param inode device inode.
 * \param file_priv DRM file private.
 * \param cmd command.
 * \param arg pointer to a drm_buf_free structure.
 * \return zero on success or a negative number on failure.
 *
 * Calls free_buffer() for each used buffer.
 * This function is primarily used for debugging.
 */
int drm_freebufs(struct drm_device *dev, void *data,
		 struct drm_file *file_priv)
{
	return -EINVAL;
}

/**
 * Compute size order.  Returns the exponent of the smaller power of two which
 * is greater or equal to given number.
 *
 * \param size size.
 * \return order.
 *
 * \todo Can be made faster.
 */
int drm_order(unsigned long size)
{
	int order;
	unsigned long tmp;

	for (order = 0, tmp = size >> 1; tmp; tmp >>= 1, order++) ;

	if (size & (size - 1))
		++order;

	return order;
}
EXPORT_SYMBOL(drm_order);
