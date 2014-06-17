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
 *    Julian Pidancet <julian.pidancet@gmail.com>
 *
 **************************************************************************/

#include "drmP.h"
#include "xengfx_drv.h"
#include "xengfx_compat.h"
#include "xengfx_reg.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
#else
/* Basic check is EDID is valid, from Linux code */
bool xengfx_edid_is_valid(struct edid *edid)
{
        int i;
        u8 csum = 0;
        u8 *raw_edid = (u8 *) edid;

        if (raw_edid[0] != 0x00 || raw_edid[1] != 0xff ||
            raw_edid[2] != 0xff || raw_edid[3] != 0xff ||
            raw_edid[4] != 0xff || raw_edid[6] != 0xff ||
            raw_edid[6] != 0xff || raw_edid[7] != 0x00) {
                DRM_ERROR("Invalid EDID header\n");
                return 0;
        }

        for (i = 0; i < 128; ++i)
                csum += raw_edid[i];

        if (csum) {
                DRM_ERROR("EDID checksum is invalid, remainder is %d\n", csum);
                return 0;
        }

        if (raw_edid[0x12] != 1) {
                DRM_ERROR("EDID has major version %d instead of 1\n", raw_edid[0x12]);
                return 0;
        }

        if (raw_edid[0x13] > 4) {
                DRM_ERROR("EDID has minor version > 4, assuming backward compatibility\n");
                return 0;
        }

        return 1;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))

void xengfx_output_poll_changed(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;

        drm_fb_helper_hotplug_event(&dev_priv->fbdev->helper);
}

void xengfx_gem_object_release(struct drm_gem_object *gem_obj)
{
        drm_gem_object_release(gem_obj);
        kfree(gem_obj);
}


#else

void xengfx_crtc_unpin_framebuffer(struct drm_mode_set *set)
{
        struct drm_crtc *crtc = set->crtc;

        if (set->fb == NULL && crtc->fb != NULL) {
                struct xengfx_gem_object *obj = to_xengfx_fb(crtc->fb)->obj;

                mutex_lock(&crtc->dev->struct_mutex);
                xengfx_gem_object_unpin(obj);
                mutex_unlock(&crtc->dev->struct_mutex);
        }
}

int xengfx_fb_changed(struct drm_device *dev)
{
        return drm_fb_helper_single_fb_probe(dev, 32, xengfx_fb_probe);
}

/*
 * Backport from Linux 2.6.35+ since we dot not use drm_gem_object_alloc
 */
int drm_gem_object_init(struct drm_device *dev,
                        struct drm_gem_object *obj,
                        size_t size)
{
        BUG_ON((size & (PAGE_SIZE - 1)) != 0);

        obj->dev = dev;
        obj->filp = shmem_file_setup("drm mm object", size, VM_NORESERVE);
        if (IS_ERR(obj->filp))
          return -ENOMEM;

        kref_init(&obj->refcount);
        kref_init(&obj->handlecount);
        obj->size = size;

        atomic_inc(&dev->object_count);
        atomic_add(obj->size, &dev->object_memory);

        return 0;
}

void xengfx_gem_object_release(struct drm_gem_object *gem_obj)
{
        struct drm_device *dev = gem_obj->dev;

        fput(gem_obj->filp);
        atomic_dec(&dev->object_count);
        atomic_sub(gem_obj->size, &dev->object_memory);
}

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
int xengfx_get_scanout_position(struct drm_device *dev, int crtc, int *vpos,
                                int *hpos)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        u32 cur_scanline;
        u32 vactive, vmax;
        int ret = 0;

        /* FIXME: This code is probably very wrong */

        cur_scanline = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, SCANLINE));
        if (cur_scanline == 0xffffffff)
                return 0; /* Unimplemented ? */
        vactive = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, V_ACTIVE));
        vmax = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, MAX_VERTICAL));

        /* No HW pixelcount register */
        *hpos = 0;

        if (cur_scanline < (vactive + 1))
                *vpos = cur_scanline;
        else {
                *vpos = cur_scanline - (vmax + 1);
                ret |= DRM_SCANOUTPOS_INVBL;
        }

        /* Don't consider it accurate for now */
        //ret |= DRM_SCANOUTPOS_ACCURATE;

        return DRM_SCANOUTPOS_VALID | ret;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))

// Dumb functions are just binders to our interfaces

int xengfx_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
                           struct drm_mode_create_dumb *args)
{
        struct drm_xengfx_gem_create data;
        int ret;

        data.width = args->width;
        data.height = args->height;
        data.bpp = args->bpp;

        ret = xengfx_gem_create_ioctl(dev, &data, file);
        if (ret)
            return ret;

        args->handle = data.handle;
        args->pitch = data.pitch;
        args->size = data.size;
        return 0;
}


int xengfx_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
                               uint32_t handle, uint64_t *offset)
{
        struct drm_xengfx_gem_map data;
        int ret;

        data.handle = handle;
        ret = xengfx_gem_map_ioctl(dev, &data, file);
        if (ret)
            return ret;

        *offset = data.offset;
        return 0;
}


int xengfx_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
                            uint32_t handle)
{
        return drm_gem_handle_delete(file, handle);
}

#endif
