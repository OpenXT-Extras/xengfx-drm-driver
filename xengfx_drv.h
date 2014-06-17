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

#ifndef XENGFX_DRV_H_
#define XENGFX_DRV_H_

#include "drm_fb_helper.h"
#include "xengfx_ioctl.h"

#define DRIVER_AUTHOR "Citrix Systems R&D Ltd."
#define DRIVER_NAME "xengfx"
#define DRIVER_DESC "Xen Graphics"
#define DRIVER_DATE "20110606"

#define DRIVER_MAJOR 1
#define DRIVER_MINOR 4
#define DRIVER_PATCHLEVEL 0

#define XENGFX_VENDOR_ID 0x5853
#define XENGFX_DEVICE_ID 0xc147

#define XGFX_EDID_LEN               256

struct xengfx_file_private {
        int dummy;
};

struct xengfx_crtc {
        struct drm_crtc drm_crtc;

        int crtc_id;

        struct drm_connector connector;

        unsigned long first_rescan;
        bool active;

        u8 edid[XGFX_EDID_LEN];
        u32 base;
};

struct xengfx_fbdev;

struct xengfx_private {
        struct drm_device *dev;

        unsigned int rev;

        void __iomem *mmio;
        unsigned int gart_size;
        unsigned int stolen_base;
        unsigned int stolen_size;

        resource_size_t aper_base;
        resource_size_t aper_size;

        struct drm_mm stolen_mm;
        struct drm_mm gart_mm;

        struct xengfx_crtc **crtcs;
        int crtc_count;

        struct xengfx_fbdev *fbdev;
        struct drm_encoder encoder;
};

/* This structure represents a range in the device GART */
struct xengfx_gem_object {
        struct drm_gem_object gem_object;

        /* Is object pinned into the aperture ? */
        unsigned int pin_count;

        /* Has pages been inserted using the fault handler ? */
        int faulted;

        /* List of pages */
        struct page **pages;

        /* Offset of the object in the aperture space managed by the GART */
        struct drm_mm_node *gart_space;
        uint32_t offset;
};

struct xengfx_framebuffer {
        struct drm_framebuffer drm_fb;
        struct xengfx_gem_object *obj;
};

struct xengfx_fbdev {
        struct drm_fb_helper helper;
        struct xengfx_framebuffer fb;
};

#define to_xengfx_crtc(x) container_of(x, struct xengfx_crtc, drm_crtc)
#define conn_to_xengfx_crtc(x) container_of(x, struct xengfx_crtc, connector)
#define to_xengfx_bo(x) container_of(x, struct xengfx_gem_object, gem_object)
#define to_xengfx_fb(x) container_of(x, struct xengfx_framebuffer, drm_fb)

static inline u32 xengfx_mmio_read(struct xengfx_private *dev_priv,
                                   unsigned int offset)
{
    return readl(dev_priv->mmio + offset);
}

static inline void xengfx_mmio_write(struct xengfx_private *dev_priv,
                                     unsigned int offset, u32 val)
{
    writel(val, dev_priv->mmio + offset);
}

/* xengfx_gem.c */
int xengfx_gem_init_object(struct drm_gem_object *obj);
void xengfx_gem_free_object(struct drm_gem_object *gem_obj);
int xengfx_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
struct xengfx_gem_object *xengfx_gem_alloc_object(struct drm_device *dev,
                                                  size_t size);
int xengfx_gem_object_pin(struct xengfx_gem_object *obj);
void xengfx_gem_object_unpin(struct xengfx_gem_object *obj);
int xengfx_gem_create_ioctl(struct drm_device *dev, void *data,
                            struct drm_file *file_priv);
int xengfx_gem_map_ioctl(struct drm_device *dev, void *data,
                         struct drm_file *file_priv);
int xengfx_gem_destroy_ioctl(struct drm_device *dev, void *data,
                             struct drm_file *file_priv);
/* xengfx_irq.c */
u32 xengfx_get_vblank_counter(struct drm_device *dev, int crtc);
int xengfx_enable_vblank(struct drm_device *dev, int crtc);
void xengfx_disable_vblank(struct drm_device *dev, int crtc);
int xengfx_get_scanout_position(struct drm_device *dev, int crtc, int *vpos,
                                int *hpos);
irqreturn_t xengfx_irq_handler(DRM_IRQ_ARGS);
void xengfx_irq_preinstall(struct drm_device *dev);
int xengfx_irq_postinstall(struct drm_device *dev);
void xengfx_irq_uninstall(struct drm_device *dev);
/* xengfx_display.c */
struct edid *xengfx_get_edid(struct drm_connector *connector, void *);
void xengfx_modeset_init(struct drm_device *dev);
void xengfx_modeset_cleanup(struct drm_device *dev);
void xengfx_crtc_status_onscreen(struct xengfx_crtc *crtc, int enable);
void xengfx_crtc_status_connected(struct xengfx_crtc *crtc, int enable);
int xengfx_framebuffer_init(struct drm_device *dev,
                            struct xengfx_framebuffer *xengfx_fb,
                            struct drm_mode_fb_cmd *mode_cmd,
                            struct xengfx_gem_object *obj);
u32 xengfx_stride_align(struct xengfx_crtc *crtc, u32 stride);
int xengfx_stride_valid(struct xengfx_crtc *crtc, u32 stride);
int xengfx_bpp_valid(struct xengfx_crtc *crtc, u32 bpp);
/* xengfx_fb.c */
int xengfx_fbdev_init(struct drm_device *dev);
void xengfx_fbdev_cleanup(struct drm_device *dev);

#endif /* XENGFX_IOCTL_H_ */
