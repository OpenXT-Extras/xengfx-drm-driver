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
#include "xengfx_reg.h"
#include "xengfx_compat.h"

#include "drm_crtc_helper.h"

struct edid *xengfx_get_edid(struct drm_connector *connector,
                             void *dummy)
{
        struct xengfx_crtc *crtc = conn_to_xengfx_crtc(connector);
        struct drm_device *dev = connector->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;
        u32 v;
        int i;
        int timeout = 1000;

        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, EDID_REQUEST),
                          0x1);
        /* Wait till EDID ready... XXX: Timeout ??? */
        v = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id, EDID_REQUEST));
        while ((v & 0x1) && --timeout) {
                v = xengfx_mmio_read(dev_priv,
                                     XGFX_VCRTC(crtc_id, EDID_REQUEST));
        }

        if (timeout <= 0)
                return NULL;

        for (i = 0; i < XGFX_EDID_LEN; i += 4) {
                v = xengfx_mmio_read(dev_priv,
                                     XGFX_VCRTC(crtc_id, EDID) + i);
                *(u32 *)(crtc->edid + i) = v;
        }

        return (struct edid *)crtc->edid;
}

/* Connector/Encoder part */

static int xengfx_connector_get_modes(struct drm_connector *connector)
{
        struct xengfx_crtc *crtc = conn_to_xengfx_crtc(connector);
        struct edid *edid = (struct edid *)crtc->edid;
        int count;

        if (!EDID_IS_VALID(edid))
                edid = xengfx_get_edid(connector, NULL);

        count = drm_add_edid_modes(connector, edid);

        if (!count) {
                /* XXX: Fall back on hardcoded default mode here */
        }

        return count;
}

static int xengfx_connector_mode_valid(struct drm_connector *connector,
			               struct drm_display_mode *mode)
{
        struct xengfx_crtc *crtc = conn_to_xengfx_crtc(connector);
        struct drm_device *dev = connector->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;
        u32 maxh;
        u32 maxv;

        maxh = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id, MAX_HORIZONTAL));
        maxv = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id, MAX_VERTICAL));
        maxh++;
        maxv++;

        if (mode->hdisplay > maxh)
                return MODE_PANEL;
        if (mode->vdisplay > maxv)
                return MODE_PANEL;

        return MODE_OK;
}

static struct drm_encoder *
xengfx_connector_best_encoder(struct drm_connector *connector)
{
        struct drm_device *dev = connector->dev;
        struct xengfx_private *dev_priv = dev->dev_private;

        return &dev_priv->encoder;
}

static void xengfx_connector_destroy(struct drm_connector *connector)
{
        struct xengfx_crtc *crtc = conn_to_xengfx_crtc(connector);

        drm_connector_cleanup(&crtc->connector);
}

static enum drm_connector_status
XENGFX_CONNECTOR_DETECT_SIGNATURE
{
        struct xengfx_crtc *crtc = conn_to_xengfx_crtc(connector);
        struct drm_device *dev = connector->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;
        u32 status;

        status = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id, STATUS));

        if (status & XGFX_VCRTC_STATUS_HOTPLUG)
            return connector_status_connected;

        return connector_status_disconnected;
}

static void xengfx_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool xengfx_encoder_mode_fixup(struct drm_encoder *encoder,
                                      struct drm_display_mode *mode,
                                      struct drm_display_mode *adjusted_mode)
{
        return true;
}

static void xengfx_encoder_prepare(struct drm_encoder *encoder)
{
}

static void xengfx_encoder_destroy(struct drm_encoder *encoder)
{
        struct drm_device *dev = encoder->dev;
        struct xengfx_private *dev_priv = dev->dev_private;

        drm_encoder_cleanup(&dev_priv->encoder);
}

static void xengfx_encoder_mode_set(struct drm_encoder *encoder,
			            struct drm_display_mode *mode,
			            struct drm_display_mode *adjusted_mode)
{
}

static void xengfx_encoder_commit(struct drm_encoder *encoder)
{
}

static const struct drm_connector_helper_funcs xengfx_connector_helper_funcs = {
        .get_modes = xengfx_connector_get_modes,
        .mode_valid = xengfx_connector_mode_valid,
        .best_encoder = xengfx_connector_best_encoder
};

static const struct drm_connector_funcs xengfx_connector_funcs = {
        .detect = xengfx_connector_detect,
        .fill_modes = drm_helper_probe_single_connector_modes,
        .destroy = xengfx_connector_destroy,
};

static const struct drm_encoder_helper_funcs xengfx_encoder_helper_funcs = {
        .dpms = xengfx_encoder_dpms,
        .mode_fixup = xengfx_encoder_mode_fixup,
        .prepare = xengfx_encoder_prepare,
        .mode_set = xengfx_encoder_mode_set,
        .commit = xengfx_encoder_commit,
};

static const struct drm_encoder_funcs xengfx_encoder_funcs = {
        .destroy = xengfx_encoder_destroy,
};

/* CRTC part */

static void xengfx_crtc_disable(struct drm_crtc *drm_crtc)
{
        struct xengfx_crtc *crtc = to_xengfx_crtc(drm_crtc);
        struct drm_device *dev = drm_crtc->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;

        if (!crtc->active)
                return;

        drm_vblank_off(dev, crtc_id);

        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, CONTROL), 0);

        /* Post */
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, BASE), 0);

        if (drm_crtc->fb) {
                struct xengfx_gem_object *obj = to_xengfx_fb(drm_crtc->fb)->obj;

                mutex_lock(&dev->struct_mutex);
                xengfx_gem_object_unpin(obj);
                mutex_unlock(&dev->struct_mutex);
        }

        crtc->active = false;
}

static void xengfx_crtc_enable(struct drm_crtc *drm_crtc)
{
        struct xengfx_crtc *crtc = to_xengfx_crtc(drm_crtc);
        struct drm_device *dev = drm_crtc->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;

        if (crtc->active)
                return;

        crtc->active = true;

        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, CONTROL), 1);

        /* Post */
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, BASE), crtc->base);
}

static void xengfx_crtc_prepare(struct drm_crtc *drm_crtc)
{
    return xengfx_crtc_disable(drm_crtc);
}

static void xengfx_crtc_commit(struct drm_crtc *drm_crtc)
{
    return xengfx_crtc_enable(drm_crtc);
}

static void xengfx_crtc_dpms(struct drm_crtc *drm_crtc, int mode)
{
        switch (mode) {
        case DRM_MODE_DPMS_ON:
        case DRM_MODE_DPMS_STANDBY:
        case DRM_MODE_DPMS_SUSPEND:
                xengfx_crtc_enable(drm_crtc);
                break;
        case DRM_MODE_DPMS_OFF:
                xengfx_crtc_disable(drm_crtc);
                break;
        }
}

static bool xengfx_crtc_mode_fixup(struct drm_crtc *drm_crtc,
                                   struct drm_display_mode *mode,
                                   struct drm_display_mode *adjusted_mode)
{
        drm_mode_set_crtcinfo(adjusted_mode, 0);

        return true;
}

static int
xengfx_crtc_set_base(struct drm_crtc *drm_crtc, int x, int y,
                     struct drm_framebuffer *old_fb)
{
        struct xengfx_crtc *crtc = to_xengfx_crtc(drm_crtc);
        struct drm_device *dev = drm_crtc->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;
        struct xengfx_framebuffer *fb;
        struct xengfx_gem_object *obj;
        int ret;
        u32 format;
        u32 base;
        u32 stride;
        u32 align;

        if (!drm_crtc->fb) {
                return 0;
        }
        fb = to_xengfx_fb(drm_crtc->fb);

        stride = fb->drm_fb.pitch;

        /* Framebuffer format is BGR by default ? */
        switch (fb->drm_fb.bits_per_pixel) {
        case 15:
                format = XGFX_FORMAT_BGR555;
                break;
        case 16:
                format = XGFX_FORMAT_BGR565;
                break;
        case 24:
                format = XGFX_FORMAT_BGR888;
                break;
        case 32:
                format = XGFX_FORMAT_BGR8888;
                break;
        default:
                return -EINVAL;
        }

        obj = fb->obj;
        if (!fb->obj)
                return -EINVAL;

        mutex_lock(&dev->struct_mutex);

        ret = xengfx_gem_object_pin(obj);
        if (ret) {
                mutex_unlock(&dev->struct_mutex);
                return ret;
        }

        if (old_fb) {
                struct xengfx_gem_object *old_obj = to_xengfx_fb(old_fb)->obj;

                xengfx_gem_object_unpin(old_obj);
        }

        base = obj->offset;
        base += x * ((fb->drm_fb.bits_per_pixel + 7) / 8) + y * fb->drm_fb.pitch;

        /* Ultimately check base and stride alignment */
        align = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id, STRIDE_ALIGNMENT));
        if ((stride & align) || (base & align)) {
                xengfx_gem_object_unpin(obj);
                mutex_unlock(&dev->struct_mutex);

                return -EINVAL;
        }

        mutex_unlock(&dev->struct_mutex);

        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, FORMAT), format);
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, STRIDE), stride);


        /*
         * Posted write to the CRTC base register is done in xengfx_crtc_commit,
         * it will then push down all the parameters to the HW.
         */
        crtc->base = base;

        /* If crtc is already active, we have to rewrite its base */
        if (crtc->active)
            xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, BASE), crtc->base);

        return 0;
}


static int
xengfx_crtc_mode_set(struct drm_crtc *drm_crtc, struct drm_display_mode *mode,
		     struct drm_display_mode *adjusted_mode, int x, int y,
		     struct drm_framebuffer *old_fb)
{
        struct xengfx_crtc *crtc = to_xengfx_crtc(drm_crtc);
        struct drm_device *dev = drm_crtc->dev;
        struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id = crtc->crtc_id;
        int ret;

        drm_vblank_pre_modeset(dev, crtc_id);

        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, H_ACTIVE),
                          adjusted_mode->crtc_hdisplay - 1);
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, V_ACTIVE),
                          adjusted_mode->crtc_vdisplay - 1);

        ret = xengfx_crtc_set_base(drm_crtc, x, y, old_fb);
        drm_vblank_post_modeset(dev, crtc_id);

        return ret;
}

static void xengfx_crtc_load_lut(struct drm_crtc *crtc)
{
}


static int
xengfx_crtc_cursor_set(struct drm_crtc *crtc, struct drm_file *file, uint32_t handle,
                       uint32_t width, uint32_t height)
{
        // FIXME
        return 0;
}


static int
xengfx_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
        // FIXME
        return 0;
}


static void
XENGFX_CRTC_GAMMA_SET_SIGNATURE
{
        /* I doubt we are going to implement this */
}

static void xengfx_crtc_destroy(struct drm_crtc *drm_crtc)
{
        struct xengfx_crtc *crtc = to_xengfx_crtc(drm_crtc);
        struct drm_device *dev = drm_crtc->dev;
        struct xengfx_private *dev_priv = dev->dev_private;

        drm_crtc_cleanup(drm_crtc);
        dev_priv->crtcs[crtc->crtc_id] = NULL;

        /*
         * drm_mode_config_cleanup() should already have called
         * encoder_destroy() and connector_destroy() at this point,
         * so there's nothing else to do here.
         */

        kfree(crtc);
}

static int xengfx_crtc_helper_set_config(struct drm_mode_set *set)
{
        CRTC_HELPER_SET_CONFIG_EXTRA_WORK;
        return drm_crtc_helper_set_config(set);
}

static const struct drm_crtc_helper_funcs xengfx_crtc_helper_funcs = {
        .dpms = xengfx_crtc_dpms,
        .mode_fixup = xengfx_crtc_mode_fixup,
        .mode_set = xengfx_crtc_mode_set,
        .mode_set_base = xengfx_crtc_set_base,
        .load_lut = xengfx_crtc_load_lut,
        .prepare = xengfx_crtc_prepare,
        .commit = xengfx_crtc_commit,
};

static const struct drm_crtc_funcs xengfx_crtc_funcs = {
        .cursor_set = xengfx_crtc_cursor_set,
        .cursor_move = xengfx_crtc_cursor_move,
        .gamma_set = xengfx_crtc_gamma_set,
        .set_config = xengfx_crtc_helper_set_config,
        .destroy = xengfx_crtc_destroy,
};

/*
 * These are called from irq context
 */
void xengfx_crtc_status_onscreen(struct xengfx_crtc *crtc, int enable)
{
        /* XXX */
}

void xengfx_crtc_status_connected(struct xengfx_crtc *crtc, int enable)
{
        /* XXX */
}

static void xengfx_crtc_init(struct drm_device *dev, int crtc_id)
{
        struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_crtc *crtc;
        struct edid *edid;

        crtc = kzalloc(sizeof (*crtc), GFP_KERNEL);
        if (!crtc)
                return;
        crtc->crtc_id = crtc_id;

        drm_connector_init(dev, &crtc->connector, &xengfx_connector_funcs,
                           DRM_MODE_CONNECTOR_LVDS);
        drm_connector_helper_add(&crtc->connector, &xengfx_connector_helper_funcs);
        crtc->connector.interlace_allowed = 0;
        crtc->connector.doublescan_allowed = 0;
        crtc->connector.status = XENGFX_CONNECTOR_DETECT_CALL(&crtc->connector);

        drm_mode_connector_attach_encoder(&crtc->connector, &dev_priv->encoder);
        dev_priv->encoder.possible_crtcs |= (1 << crtc_id);

        drm_crtc_init(dev, &crtc->drm_crtc, &xengfx_crtc_funcs);
        drm_mode_crtc_set_gamma_size(&crtc->drm_crtc, 256);
        drm_crtc_helper_add(&crtc->drm_crtc, &xengfx_crtc_helper_funcs);
        dev_priv->crtcs[crtc_id] = crtc;

        drm_sysfs_connector_add(&crtc->connector);

        if (crtc->connector.status == connector_status_connected) {
                edid = xengfx_get_edid(&crtc->connector, NULL);
                if (edid && drm_add_edid_modes(&crtc->connector, edid)) {
                       drm_mode_connector_update_edid_property(&crtc->connector,
		       					       edid);
                }
        }
}

/* Framebuffer part */

static void xengfx_fb_destroy(struct drm_framebuffer *drm_fb)
{
        struct xengfx_framebuffer *xengfx_fb = to_xengfx_fb(drm_fb);
        struct xengfx_gem_object *obj = xengfx_fb->obj;

        drm_framebuffer_cleanup(drm_fb);
	DRM_GEM_OBJECT_UNREFERENCE(&obj->gem_object);

        kfree(xengfx_fb);
}

static int xengfx_fb_create_handle(struct drm_framebuffer *drm_fb,
                                   struct drm_file *file,
                                   unsigned int *handle)
{
        struct xengfx_framebuffer *xengfx_fb = to_xengfx_fb(drm_fb);
        struct xengfx_gem_object *obj = xengfx_fb->obj;

        return drm_gem_handle_create(file, &obj->gem_object, handle);
}

static const struct drm_framebuffer_funcs xengfx_fb_funcs = {
        .destroy = xengfx_fb_destroy,
        .create_handle = xengfx_fb_create_handle
};

/**
 * This function can be called for both userspace AND fbdev framebuffer
 * creation
 */
int xengfx_framebuffer_init(struct drm_device *dev,
                            struct xengfx_framebuffer *xengfx_fb,
                            struct drm_mode_fb_cmd *mode_cmd,
                            struct xengfx_gem_object *obj)
{
        int ret;

        ret = drm_framebuffer_init(dev, &xengfx_fb->drm_fb, &xengfx_fb_funcs);
        if (ret) {
                DRM_ERROR("framebuffer init failed %d\n", ret);
                return ret;
        }

        drm_helper_mode_fill_fb_struct(&xengfx_fb->drm_fb, mode_cmd);
        xengfx_fb->obj = obj;
        return 0;
}

/**
 * This is the userspace framebuffer creation function.
 * For fbdev, check xengfx_fb.c::xengfx_fb_probe()
 */
static struct drm_framebuffer *xengfx_fb_create(struct drm_device *dev,
                                                struct drm_file *file_priv,
                                                struct drm_mode_fb_cmd *mode_cmd)
{
        struct xengfx_gem_object *obj;
        struct xengfx_framebuffer *xengfx_fb;
        int ret;

        obj = to_xengfx_bo(drm_gem_object_lookup(dev, file_priv,
                                                 mode_cmd->handle));
        if (&obj->gem_object == NULL)
                return ERR_PTR(-ENOENT);

        xengfx_fb = kzalloc(sizeof (*xengfx_fb), GFP_KERNEL);
        if (!xengfx_fb) {
                DRM_GEM_OBJECT_UNREFERENCE(&obj->gem_object);
                return ERR_PTR(-ENOMEM);
        }

        ret = xengfx_framebuffer_init(dev, xengfx_fb, mode_cmd, obj);
        if (ret) {
                DRM_GEM_OBJECT_UNREFERENCE(&obj->gem_object);
                kfree(xengfx_fb);
                return ERR_PTR(ret);
        }

        return &xengfx_fb->drm_fb;
}

static struct drm_mode_config_funcs xengfx_mode_funcs = {
        .fb_create = xengfx_fb_create,
        XENGFX_MODE_FUNCS_COMPAT,
};

/* Modesetting part */

static void xengfx_disable_vga(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;
        u32 v;

        v = xengfx_mmio_read(dev_priv, XGFX_CONTROL);
        xengfx_mmio_write(dev_priv, XGFX_CONTROL, v | XGFX_CONTROL_HIRES_EN);
}

static void xengfx_encoder_init(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;

        drm_encoder_init(dev, &dev_priv->encoder, &xengfx_encoder_funcs,
                         DRM_MODE_ENCODER_LVDS);
        drm_encoder_helper_add(&dev_priv->encoder, &xengfx_encoder_helper_funcs);
}

void xengfx_modeset_init(struct drm_device *dev)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        int crtc_id;
        u32 ncrtc;

        drm_mode_config_init(dev);

        dev->mode_config.funcs = &xengfx_mode_funcs;

        dev->mode_config.min_width = 1;
        dev->mode_config.min_height = 1;
        dev->mode_config.max_width = 8192;
        dev->mode_config.max_height = 8192;

        dev->mode_config.fb_base = dev_priv->aper_base;

        ncrtc = xengfx_mmio_read(dev_priv, XGFX_NVCRTC);
        dev_priv->crtcs = kzalloc(sizeof(*dev_priv->crtcs) * ncrtc, GFP_KERNEL);
        if (!dev_priv->crtcs)
                return;

        xengfx_encoder_init(dev);

        for (crtc_id = 0; crtc_id < ncrtc; crtc_id++) {
                xengfx_crtc_init(dev, crtc_id);
        }
        xengfx_disable_vga(dev);

        dev_priv->crtc_count = ncrtc;
}

void xengfx_modeset_cleanup(struct drm_device *dev)
{
        drm_mode_config_cleanup(dev);
}

