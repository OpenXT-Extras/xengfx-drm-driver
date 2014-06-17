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

#include <linux/module.h>
#include "drmP.h"
#include "drm_crtc_helper.h"
#include "xengfx_drv.h"
#include "xengfx_reg.h"
#include "xengfx_compat.h"

static void xengfx_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
                                u16 blue, int regno)
{
    /* I doubt we are going to implement this */
}

static void xengfx_fb_gamma_get(struct drm_crtc *crtc, u16 *red, u16 *green,
                                u16 *blue, int regno)
{
    /* I doubt we are going to implement this */
}

struct fb_ops xengfx_fb_ops = {
        .owner = THIS_MODULE,
        .fb_check_var = drm_fb_helper_check_var,
        .fb_set_par = drm_fb_helper_set_par,
        .fb_fillrect = cfb_fillrect,
        .fb_copyarea = cfb_copyarea,
        .fb_imageblit = cfb_imageblit,
        .fb_pan_display = drm_fb_helper_pan_display,
        .fb_blank = drm_fb_helper_blank,
        .fb_setcmap = drm_fb_helper_setcmap,
        FB_DEBUG_OPS
};

struct drm_fb_helper_funcs xengfx_fb_helper_funcs = {
        .gamma_set = xengfx_fb_gamma_set,
        .gamma_get = xengfx_fb_gamma_get,
        FB_PROBE_IMPLEMENTATION
};

int xengfx_fbdev_init(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_fbdev *fbdev;

        fbdev = kzalloc(sizeof (*fbdev), GFP_KERNEL);
        if (!fbdev)
                return -ENOMEM;

        dev_priv->fbdev = fbdev;

        return xengfx_fbdev_init_compat(dev);
}

void xengfx_fbdev_cleanup(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_fbdev *fbdev = dev_priv->fbdev;
        struct fb_info *info;

        if (!fbdev)
                return;

        info = fbdev_to_fb_info(fbdev);

        if (info) {
                unregister_framebuffer(info);
                iounmap(info->screen_base);
                framebuffer_release(info);
        }

        DRM_FB_HELPER_FREE(&fbdev->helper);

        drm_framebuffer_cleanup(&fbdev->fb.drm_fb);

        if (fbdev->fb.obj) {
		DRM_GEM_OBJECT_UNREFERENCE(&fbdev->fb.obj->gem_object);
                fbdev->fb.obj = NULL;
        }

        kfree(dev_priv->fbdev);
        dev_priv->fbdev = NULL;
}

