/**************************************************************************
 *
 * Copyright (c) 2011 Citrix Systems, Inc.
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
 * Authors:
 *      Julian Pidancet <julian.pidancet@citrix.com>
 *
 **************************************************************************/

#include "drmP.h"
#include "xengfx_drv.h"
#include "xengfx_reg.h"
#include "xengfx_compat.h"

static void xengfx_gart_flush(struct xengfx_private *dev_priv)
{
        u32 tmp = xengfx_mmio_read(dev_priv, XGFX_GART_INVAL);

        (void)tmp;
}

static void xengfx_gart_write_entry(struct xengfx_private *dev_priv,
                                    dma_addr_t addr,
                                    unsigned int entry)
{
        u32 pte = (u32)addr >> PAGE_SHIFT;

        if (pte) {
                pte &= XGFX_GART_PFN_MASK;
                pte |= XGFX_GART_ENTRY_VALID;
        }

        writel(pte, dev_priv->mmio + XGFX_GART_BASE + entry * 4);
}

static int
xengfx_gem_object_get_pages(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        int npages, i;
        struct inode *inode;
        struct page *page;
        struct address_space *mapping;
        gfp_t gfpmask = 0;

        npages = gem_obj->size / PAGE_SIZE;
        obj->pages = DRM_CALLOC(npages, sizeof (struct page *));

        if (!obj->pages)
                return -ENOMEM;

        inode = gem_obj->filp->f_path.dentry->d_inode;

        mapping = inode->i_mapping;
        gfpmask |= mapping_gfp_mask(mapping);

        for (i = 0; i < npages; i++) {
                page = READ_PAGE_GFP(mapping, i, gfpmask);
                if (IS_ERR(page)) {
                        while (--i >= 0)
                                page_cache_release(obj->pages[i]);
                        drm_free_large(obj->pages);
                        obj->pages = NULL;
                        return PTR_ERR(page);
                }

                obj->pages[i] = page;
        }

        return 0;
}

static void
xengfx_gem_object_put_pages(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        int npages, i;

        npages = gem_obj->size / PAGE_SIZE;

        if (!obj->pages)
                return;

        for (i = 0; i < npages; i++) {
                page_cache_release(obj->pages[i]);
        }
        drm_free_large(obj->pages);
        obj->pages = NULL;
}

static int
xengfx_gem_object_bind_gart(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        unsigned int first_entry;
        int npages, i;

        BUG_ON(!obj->pages);
        BUG_ON(!obj->gart_space);

        first_entry = obj->offset / PAGE_SIZE;
        npages = gem_obj->size / PAGE_SIZE;
        for (i = 0; i < npages; i++) {
                struct page *page = obj->pages[i];
                dma_addr_t addr = page_to_phys(page);

                xengfx_gart_write_entry(dev_priv, addr, first_entry + i);
        }

        return 0;
}

static void
xengfx_gem_object_unbind_gart(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        unsigned int first_entry;

        int npages, i;

        BUG_ON(!obj->pages);

        first_entry = obj->offset / PAGE_SIZE;
        npages = gem_obj->size / PAGE_SIZE;
        for (i = 0; i < npages; i++) {
                xengfx_gart_write_entry(dev_priv, 0, first_entry + i);
        }
        xengfx_gart_flush(dev_priv);
}

static int xengfx_gem_object_bind(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        struct drm_mm_node *free_space;
        int ret;

        if (gem_obj->size > dev_priv->aper_size) {
                DRM_ERROR("Attempting to bind an object larger than the aperture\n");
                return -E2BIG;
        }

        free_space = drm_mm_search_free(&dev_priv->gart_mm, gem_obj->size,
                                        PAGE_SIZE, 0);
        if (free_space)
                obj->gart_space = drm_mm_get_block(free_space, gem_obj->size,
                                                   PAGE_SIZE);

        if (!obj->gart_space) {
                return -ENOSPC;
        }
        obj->offset = obj->gart_space->start;

        ret = xengfx_gem_object_get_pages(obj);
        if (ret) {
                drm_mm_put_block(obj->gart_space);
                obj->gart_space = NULL;

                return ret;
        }

        ret = xengfx_gem_object_bind_gart(obj);
        if (ret) {
                xengfx_gem_object_put_pages(obj);
                drm_mm_put_block(obj->gart_space);
                obj->gart_space = NULL;

                return ret;
        }

        DRM_DEBUG_DRIVER("Bound buffer object %p to aperture: "
                         "offset=%lx size=%lx\n", obj,
                         obj->gart_space->start, obj->gart_space->size);

        xengfx_gart_flush(dev_priv);

        return 0;
}

static int xengfx_gem_object_unbind(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        struct xengfx_private *dev_priv = dev->dev_private;

        if (!obj->gart_space)
                return 0;

        if (obj->pin_count) {
                DRM_ERROR("Can't unbind pinned buffer\n");
        }

        if (dev->dev_mapping && obj->faulted) {
            unmap_mapping_range(dev->dev_mapping,
                                (loff_t)gem_obj->map_list.hash.key << PAGE_SHIFT,
                                gem_obj->size, 1);
            obj->faulted = 0;
        }


        xengfx_gem_object_unbind_gart(obj);
        xengfx_gem_object_put_pages(obj);
        drm_mm_put_block(obj->gart_space);
        obj->gart_space = NULL;
        obj->offset = 0;

        DRM_DEBUG_DRIVER("Unbound buffer object %p from aperture\n", obj);

        xengfx_gart_flush(dev_priv);

        return 0;
}

int xengfx_gem_object_pin(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        int ret = 0;

        BUG_ON(!mutex_is_locked(&dev->struct_mutex));

        if (!obj->gart_space) {
                ret = xengfx_gem_object_bind(obj);
                if (ret)
                        return ret;
        }

        obj->pin_count++;

        return ret;
}

void xengfx_gem_object_unpin(struct xengfx_gem_object *obj)
{
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;

        BUG_ON(!mutex_is_locked(&dev->struct_mutex));

        if (!--obj->pin_count) {
                xengfx_gem_object_unbind(obj);
        }
}

int xengfx_gem_init_object(struct drm_gem_object *obj)
{
        /*
         * This function is normally called by drm_gem_object_alloc() to
         * set up obj->driver_private. But we use our own version
         * xengfx_gem_alloc_object() which ensapsulate the object into our
         * own structure xengfx_gem_object. Thus, this function should
         * never be called.
         */
	BUG();

	return 0;
}

struct xengfx_gem_object *xengfx_gem_alloc_object(struct drm_device *dev,
                                                  size_t size)
{
        struct xengfx_gem_object *obj;

        obj = kzalloc(sizeof (*obj), GFP_KERNEL);
        if (!obj)
                return NULL;

        if (drm_gem_object_init(dev, &obj->gem_object, size) != 0) {
                kfree(obj);
                return NULL;
        }

        DRM_DEBUG_DRIVER("Allocated buffer object %p, size=%lx\n", obj, (long unsigned) size);

        return obj;
}


static void
xengfx_gem_free_mmap_offset(struct xengfx_gem_object *obj)
{
        struct drm_device *dev = obj->gem_object.dev;
        struct drm_gem_mm *mm = dev->mm_private;
        struct drm_map_list *list = &obj->gem_object.map_list;

        drm_ht_remove_item(&mm->offset_hash, &list->hash);
        drm_mm_put_block(list->file_offset_node);
        kfree(list->map);
        list->map = NULL;
}


void xengfx_gem_free_object(struct drm_gem_object *gem_obj)
{
        struct xengfx_gem_object *obj = to_xengfx_bo(gem_obj);
        struct drm_device *dev = gem_obj->dev;

        DRM_DEBUG_DRIVER("Freeing buffer object %p\n", obj);

        BUG_ON(!mutex_is_locked(&dev->struct_mutex));

        if (obj->pin_count) {
                xengfx_gem_object_unpin(obj);
        }

        if (obj->pin_count) {
                DRM_ERROR("Freeing a buffer object which has not been properly "
                          "unpined from the aperture. "
                          "Aperture space will be lost\n");
        }

        if (obj->gem_object.map_list.map)
               xengfx_gem_free_mmap_offset(obj);

        xengfx_gem_object_release(gem_obj);
}

int xengfx_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
        struct xengfx_gem_object *obj = to_xengfx_bo(vma->vm_private_data);
        struct drm_gem_object *gem_obj = &obj->gem_object;
        struct drm_device *dev = gem_obj->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        pgoff_t file_offset;
        int ret;
        unsigned long pfn;

        /* vmf->pgoff is a fake offset */
        file_offset = (unsigned long)vmf->virtual_address - vma->vm_start;

        ret = mutex_lock_interruptible(&dev->struct_mutex);
        if (ret)
                goto out;

        if (!obj->gart_space) {
                ret = xengfx_gem_object_bind(obj);
                if (ret)
                        goto unlock;
        }

        pfn = (dev_priv->aper_base + obj->offset + file_offset) >> PAGE_SHIFT;
        ret = vm_insert_pfn(vma, (unsigned long)vmf->virtual_address, pfn);

        obj->faulted = 1;

unlock:
        mutex_unlock(&dev->struct_mutex);
out:
        /* see i915_gem.c for explanations */
        switch (ret) {
        case -EIO:
        case -EAGAIN:
                set_need_resched();
        case 0:
        case -ERESTARTSYS:
        case -EINTR:
                return VM_FAULT_NOPAGE;
        case -ENOMEM:
                return VM_FAULT_OOM;
        default:
                return VM_FAULT_ERROR;
        }
}

static int
xengfx_gem_create(struct drm_file *file,
                  struct drm_device *dev,
                  uint64_t size,
                  uint32_t *handle_p)
{
        struct xengfx_gem_object *obj;
        int ret;
        u32 handle;

        size = roundup(size, PAGE_SIZE);
        if (size == 0)
                return -EINVAL;

        /* Allocate the new object */
        obj = xengfx_gem_alloc_object(dev, size);
        if (obj == NULL)
                return -ENOMEM;

        ret = drm_gem_handle_create(file, &obj->gem_object, &handle);
        if (ret) {
                xengfx_gem_object_release(&obj->gem_object);
                kfree(obj);
                return ret;
        }

        // drop reference from allocate - handle holds it now
        drm_gem_object_unreference(&obj->gem_object);

        *handle_p = handle;
        return 0;
}


int
xengfx_gem_create_ioctl(struct drm_device *dev,
                        void *data,
                        struct drm_file *file_priv)
{
        struct drm_xengfx_gem_create *args = data;

        args->pitch = ALIGN(args->width * ((args->bpp + 7) / 8), 128);
        args->size = args->pitch * args->height;
        return xengfx_gem_create(file_priv, dev, args->size, &args->handle);
}


static int
xengfx_gem_create_mmap_offset(struct xengfx_gem_object *obj)
{
        struct drm_device *dev = obj->gem_object.dev;
        struct drm_gem_mm *mm = dev->mm_private;
        struct drm_map_list *list;
        struct drm_local_map *map;
        int ret = 0;

        // set the object up for mmap'ing
        list = &obj->gem_object.map_list;
        list->map = kzalloc(sizeof (struct drm_map_list), GFP_KERNEL);
        if (!list->map)
                return -ENOMEM;

        map = list->map;
        map->type = _DRM_GEM;
        map->size = obj->gem_object.size;
        map->handle = obj;

        // Get a DRM GEM mmap offset allocated
        list->file_offset_node = drm_mm_search_free(&mm->offset_manager,
                                                    obj->gem_object.size / PAGE_SIZE,
                                                    0, 0);
        if (!list->file_offset_node) {
                DRM_ERROR("failed to allocate offset for bo %d\n",
                          obj->gem_object.name);
                ret = -ENOSPC;
                goto out_free_list;
        }

        list->file_offset_node = drm_mm_get_block(list->file_offset_node,
                                                  obj->gem_object.size / PAGE_SIZE,
                                                  0);

        if (!list->file_offset_node) {
                ret = -ENOMEM;
                goto out_free_list;
        }

        list->hash.key = list->file_offset_node->start;
        ret = drm_ht_insert_item(&mm->offset_hash, &list->hash);
        if (ret) {
                DRM_ERROR("failed to add to map hash\n");
                goto out_free_mm;
        }

        return 0;

out_free_mm:
        drm_mm_put_block(list->file_offset_node);
out_free_list:
        kfree(list->map);
        list->map = NULL;

        return ret;
}


int
xengfx_gem_map_ioctl(struct drm_device *dev,
                     void *data,
                     struct drm_file *file_priv)

{
        struct drm_xengfx_gem_map *args = data;
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_gem_object *obj;
        int ret;

        ret = mutex_lock_interruptible(&dev->struct_mutex);
        if (ret)
                return ret;

        obj = to_xengfx_bo(drm_gem_object_lookup(dev, file_priv, args->handle));
        if (&obj->gem_object == NULL) {
                ret = ENOENT;
                goto unlock;
        }

        // paranoia
        if (obj->gem_object.size > dev_priv->aper_size) {
                ret = -E2BIG;
                goto out;
        }

        if (!obj->gem_object.map_list.map) {
                ret = xengfx_gem_create_mmap_offset(obj);
                if (ret)
                        goto out;
        }

        args->offset = (uint64_t) obj->gem_object.map_list.hash.key << PAGE_SHIFT;

out:
        drm_gem_object_unreference(&obj->gem_object);
unlock:
        mutex_unlock(&dev->struct_mutex);
        return ret;
}


