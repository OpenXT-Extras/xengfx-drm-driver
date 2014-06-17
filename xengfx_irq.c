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

irqreturn_t xengfx_irq_handler(DRM_IRQ_ARGS)
{
        struct drm_device *dev = arg;
	struct xengfx_private *dev_priv = dev->dev_private;
        u32 isr;
        irqreturn_t ret = IRQ_NONE;
        struct drm_crtc *crtc;

        isr = xengfx_mmio_read(dev_priv, XGFX_ISR);
        if (!isr)
                return IRQ_NONE;

        list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
                struct xengfx_crtc *xengfx_crtc = to_xengfx_crtc(crtc);
                int crtc_id = xengfx_crtc->crtc_id;
                u32 status, change;
                int enable;

                status = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id,
                                                               STATUS));
                change = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc_id,
                                                               STATUS_CHANGE));

                if (change & XGFX_VCRTC_STATUS_ONSCREEN) {
                        enable = status & XGFX_VCRTC_STATUS_ONSCREEN;
                        xengfx_crtc_status_onscreen(xengfx_crtc, enable);
                        ret = IRQ_HANDLED;
                }

                if (change & XGFX_VCRTC_STATUS_HOTPLUG) {
                        enable = status & XGFX_VCRTC_STATUS_HOTPLUG;
                        xengfx_crtc_status_connected(xengfx_crtc, enable);
                        ret = IRQ_HANDLED;
                }

                if (change & XGFX_VCRTC_STATUS_RETRACE) {
                        drm_handle_vblank(dev, crtc_id);
                        ret = IRQ_HANDLED;
                }

                /* Clear status for this CRTC */
                xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc_id, STATUS_CHANGE),
                                  change);
        }

        xengfx_mmio_write(dev_priv, XGFX_ISR, isr);

        return ret;
}

void xengfx_irq_preinstall(struct drm_device *dev)
{

}

int xengfx_irq_postinstall(struct drm_device *dev)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        int crtc;
        u32 val;

        for (crtc = 0; crtc < dev->num_crtcs; crtc++) {
                u32 status;

                /* Clear CRTC status change register */
                status = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, STATUS_CHANGE));
                xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc, STATUS_CHANGE),
                                  status);

                /*
                 * Enable HOTPLUG and ONSCREEN interrupts, RETRACE is enabled
                 * independantly by the vblank DRM code.
                 */
                xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc, STATUS_INT),
                                  XGFX_VCRTC_STATUS_HOTPLUG |
                                  XGFX_VCRTC_STATUS_ONSCREEN);
        }

        xengfx_mmio_write(dev_priv, XGFX_ISR, 0);
        val = xengfx_mmio_read(dev_priv, XGFX_CONTROL);
        xengfx_mmio_write(dev_priv, XGFX_CONTROL, val | XGFX_CONTROL_INT_EN);
	return 0;
}

void xengfx_irq_uninstall(struct drm_device *dev)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        u32 val;

        val = xengfx_mmio_read(dev_priv, XGFX_CONTROL);
        xengfx_mmio_write(dev_priv, XGFX_CONTROL, val & ~XGFX_CONTROL_INT_EN);

        xengfx_mmio_write(dev_priv, XGFX_ISR, 0);
}

u32 xengfx_get_vblank_counter(struct drm_device *dev, int crtc)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_crtc *xengfx_crtc = dev_priv->crtcs[crtc];

        if (xengfx_crtc == NULL)
                return -EINVAL;

        if (!xengfx_crtc->first_rescan)
                return -EINVAL;

        /* Lets assume we rescan 60 times a second */
        return ((xengfx_crtc->first_rescan - jiffies) * 60) / HZ;
}

int xengfx_enable_vblank(struct drm_device *dev, int crtc)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        u32 status_int;

        status_int = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, STATUS_INT));
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc, STATUS_INT),
                          status_int | XGFX_VCRTC_STATUS_RETRACE);

        return 0;
}

void xengfx_disable_vblank(struct drm_device *dev, int crtc)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        u32 status_int;

        status_int = xengfx_mmio_read(dev_priv, XGFX_VCRTC(crtc, STATUS_INT));
        xengfx_mmio_write(dev_priv, XGFX_VCRTC(crtc, STATUS_INT),
                          status_int & ~XGFX_VCRTC_STATUS_RETRACE);
}

// Basic implementation of get_vblank_timestamp for drm_driver struct
// in case we would need it

#if 0
int xengfx_get_vblank_timestamp(struct drm_device *dev, int crtc,
                                int *max_error, struct timeval *vblank_time,
			        unsigned flags)
{
	struct xengfx_private *dev_priv = dev->dev_private;
        struct xengfx_crtc *xengfx_crtc = dev_priv->crtcs[crtc];

        if (xengfx_crtc == NULL)
                return -EINVAL;

        return drm_calc_vbltimestamp_from_scanoutpos(dev, crtc, max_error,
                                                     vblank_time, flags,
                                                     &xengfx_crtc->drm_crtc);
}
#endif
