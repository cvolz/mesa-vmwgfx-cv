/**************************************************************************
 *
 * Copyright © 2013 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors:
 *     Thomas Hellstrom <thellstrom@vmware.com>
 *
 */

#include "vmwgfx_drv.h"
#ifndef VMWGFX_STANDALONE
#include <linux/dma-buf.h>
#endif
#include "ttm/ttm_object.h"

/*
 * DMA-BUF attach- and mapping methods. No need to implement
 * these until we have other virtual devices use them.
 */

static int vmw_prime_map_attach(struct dma_buf *dma_buf,
//#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
//				struct device *target_dev,
//#endif
				struct dma_buf_attachment *attach)
{
	return -ENOSYS;
}

static void vmw_prime_map_detach(struct dma_buf *dma_buf,
				 struct dma_buf_attachment *attach)
{
}

static struct sg_table *vmw_prime_map_dma_buf(struct dma_buf_attachment *attach,
					      enum dma_data_direction dir)
{
	return ERR_PTR(-ENOSYS);
}

static void vmw_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
				    struct sg_table *sgb,
				    enum dma_data_direction dir)
{
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
static void *vmw_prime_dmabuf_kmap_atomic(struct dma_buf *dma_buf,
		unsigned long page_num)
{
	return NULL;
}

static void vmw_prime_dmabuf_kunmap_atomic(struct dma_buf *dma_buf,
		unsigned long page_num, void *addr)
{

}
#endif

static void *vmw_prime_dmabuf_kmap(struct dma_buf *dma_buf,
		unsigned long page_num)
{
	return NULL;
}

static void vmw_prime_dmabuf_kunmap(struct dma_buf *dma_buf,
		unsigned long page_num, void *addr)
{

}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0) &&\
     !defined(DMA_BUF_STANDALONE))
static int vmw_prime_dmabuf_mmap(struct dma_buf *dma_buf,
				 struct vm_area_struct *vma)
{
	WARN_ONCE(true, "Attempted use of dmabuf mmap. Bad.\n");
	return -ENOSYS;
}


static void *vmw_prime_dmabuf_vmap(struct dma_buf *dma_buf)
{
	return NULL;
}

static void vmw_prime_dmabuf_vunmap(struct dma_buf *dma_buf, void *vaddr)
{
}
#endif

const struct dma_buf_ops vmw_prime_dmabuf_ops =  {
	.attach = vmw_prime_map_attach,
	.detach = vmw_prime_map_detach,
	.map_dma_buf = vmw_prime_map_dma_buf,
	.unmap_dma_buf = vmw_prime_unmap_dma_buf,
	.release = NULL,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)) || \
     (RHEL_VERSION_CODE >= RHEL_RELEASE_VERSION(7, 5)))
	.map = vmw_prime_dmabuf_kmap,
	.unmap = vmw_prime_dmabuf_kunmap,
/*
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
	.map_atomic = vmw_prime_dmabuf_kmap_atomic,
	.unmap_atomic = vmw_prime_dmabuf_kunmap_atomic,
#endif
*/
#else
	.kmap = vmw_prime_dmabuf_kmap,
	.kmap_atomic = vmw_prime_dmabuf_kmap_atomic,
	.kunmap = vmw_prime_dmabuf_kunmap,
	.kunmap_atomic = vmw_prime_dmabuf_kunmap_atomic,
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0) &&	\
     !defined(DMA_BUF_STANDALONE))
	.mmap = vmw_prime_dmabuf_mmap,
	.vmap = vmw_prime_dmabuf_vmap,
	.vunmap = vmw_prime_dmabuf_vunmap,
#endif
};

int vmw_prime_fd_to_handle(struct drm_device *dev,
			   struct drm_file *file_priv,
			   int fd, u32 *handle)
{
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;

	return ttm_prime_fd_to_handle(tfile, fd, handle);
}

int vmw_prime_handle_to_fd(struct drm_device *dev,
			   struct drm_file *file_priv,
			   uint32_t handle, uint32_t flags,
			   int *prime_fd)
{
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;

	return ttm_prime_handle_to_fd(tfile, handle, flags, prime_fd);
}
