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
#include "drm_crtc_helper.h"
#include "xengfx_drv.h"
#include "xengfx_compat.h"

extern struct fb_ops xengfx_fb_ops;
extern struct drm_fb_helper_funcs xengfx_fb_helper_funcs;

/*
 * This functions creates the framebuffer used by fbdev.
 * Userspace framebuffer is created using xengfx_display.c::xengfx_fb_create()
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
int xengfx_fb_probe(struct drm_fb_helper *helper,
                    struct drm_fb_helper_surface_size *sizes)
#else
int xengfx_fb_probe(struct drm_device *dev,
                    uint32_t fb_width, uint32_t fb_height,
                    uint32_t surface_width, uint32_t surface_height,
                    uint32_t surface_depth, uint32_t surface_bpp,
                    struct drm_framebuffer **fb_p)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        struct drm_device *dev = helper->dev;
#endif
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_fbdev *fbdev = dev_priv->fbdev;
        struct drm_framebuffer *drm_fb;
        struct drm_mode_fb_cmd mode_cmd;
        struct xengfx_gem_object *obj;
        struct fb_info *info;
        int size;
        int ret;

        if (fbdev->helper.fb)
                return 0;

        /*
         * Valid framebuffer format and pitch alignment parametters exist
         * on a per-crtc basis. So just fixup some sane values here:
         * 32 bits per pixel, 128 bytes pitch alignment.
         */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        sizes->surface_bpp = 32;

        mode_cmd.width = sizes->surface_width;
        mode_cmd.height = sizes->surface_height;
        mode_cmd.bpp = sizes->surface_bpp;
        mode_cmd.depth = sizes->surface_depth;
#else
        mode_cmd.width = surface_width;
        mode_cmd.height = surface_height;
        mode_cmd.bpp = surface_bpp;
        mode_cmd.depth = surface_depth;
#endif

        mode_cmd.pitch = ALIGN(mode_cmd.width * ((mode_cmd.bpp + 7) / 8), 128);

        size = mode_cmd.pitch * mode_cmd.height;
        size = ALIGN(size, PAGE_SIZE);

        obj = xengfx_gem_alloc_object(dev, size);
        if (!obj) {
                DRM_ERROR("failed to allocate framebuffer\n");
                return -ENOMEM;
        }

        mutex_lock(&dev->struct_mutex);

        ret = xengfx_gem_object_pin(obj);
        if (ret) {
                DRM_ERROR("Failed to pin framebuffer to the GART: %d\n", ret);
                goto unref;
        }

        info = framebuffer_alloc(0, &dev->pdev->dev);
        if (!info) {
                ret = -ENOMEM;
                goto unbind;
        }

        info->par = fbdev;

        ret = xengfx_framebuffer_init(dev, &fbdev->fb, &mode_cmd, obj);
        if (ret)
                goto unbind;

        drm_fb = &fbdev->fb.drm_fb;
        fbdev->helper.fb = drm_fb;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        fbdev->helper.fbdev = info;
#else
        drm_fb->fbdev = info;
#endif
        strcpy(info->fix.id, "xengfxdrmfb");

        info->flags = FBINFO_DEFAULT;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
        info->flags |= FBINFO_CAN_FORCE_OUTPUT;
#endif
        info->fbops = &xengfx_fb_ops;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        info->apertures = alloc_apertures(1);
        if (!info->apertures) {
                ret = -ENOMEM;
                goto unbind;
        }
        info->apertures->ranges[0].base = dev_priv->aper_base;
        info->apertures->ranges[0].size = dev_priv->aper_size;
#else
        info->aperture_base = dev_priv->aper_base;
        info->aperture_size = dev_priv->aper_size;
#endif

        info->fix.smem_start = dev_priv->aper_base + obj->offset;
        info->fix.smem_len = size;

        info->screen_base = ioremap_wc(dev_priv->aper_base + obj->offset, size);
        if (!info->screen_base) {
                ret = -ENOSPC;
                goto unbind;
        }
        info->screen_size = size;

        drm_fb_helper_fill_fix(info, drm_fb->pitch, drm_fb->depth);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        drm_fb_helper_fill_var(info, &fbdev->helper,
                               sizes->fb_width, sizes->fb_height);
#else
        drm_fb_helper_fill_var(info, drm_fb, fb_width, fb_height);
#endif

        mutex_unlock(&dev->struct_mutex);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        return 1;
#else
        *fb_p = drm_fb;
        return 0;
#endif

unbind:
        xengfx_gem_object_unpin(obj);
unref:
        drm_gem_object_unreference(&obj->gem_object);
        mutex_unlock(&dev->struct_mutex);

        /* The rest of deinitialization is done in xengfx_fbdev_cleanup */
        return ret;
}

int xengfx_fbdev_init_compat(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_fbdev *fbdev  = dev_priv->fbdev;
        int ret;

        fbdev->helper.funcs = &xengfx_fb_helper_funcs;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
        ret = drm_fb_helper_init(dev, &fbdev->helper, dev_priv->crtc_count,
                                 dev_priv->crtc_count);
        if (ret) {
                kfree(fbdev);
                dev_priv->fbdev = NULL;
                return ret;
        }

        drm_fb_helper_single_add_all_connectors(&fbdev->helper);
        drm_fb_helper_initial_config(&fbdev->helper, 32);
#else
        INIT_LIST_HEAD(&fbdev->helper.kernel_fb_list);
        fbdev->helper.dev = dev;

        ret = drm_fb_helper_init_crtc_count(&fbdev->helper, dev_priv->crtc_count,
                                            dev_priv->crtc_count);
        if (ret) {
                kfree(fbdev);
                dev_priv->fbdev = NULL;
                return ret;
        }

        drm_helper_initial_config(dev);
#endif
        return 0;
}

