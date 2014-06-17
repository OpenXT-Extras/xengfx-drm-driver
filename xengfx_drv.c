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
#include <linux/device.h>
#include "drmP.h"
#include "drm_crtc_helper.h"
#include "xengfx_drv.h"
#include "xengfx_reg.h"
#include "xengfx_compat.h"

static struct drm_driver xengfx_drm_driver;

static struct pci_device_id pciidlist[] = {
        {
                .vendor = XENGFX_VENDOR_ID,
                .device = XENGFX_DEVICE_ID,
                .subvendor = PCI_ANY_ID,
                .subdevice = PCI_ANY_ID,
                .class = PCI_CLASS_DISPLAY_VGA << 8,
                .class_mask = 0xff0000,
                .driver_data = 0
        },
        { 0, 0, 0}
};

static int xengfx_pm_suspend(struct device *dev)
{
        struct pci_dev *pdev = to_pci_dev(dev);
        struct drm_device *drm_dev = pci_get_drvdata(pdev);

        if (!drm_dev || !drm_dev->dev_private) {
                dev_err(dev, "DRM not initialized, aborting suspend.\n");
                return -ENODEV;
        }

        CHECK_IF_POWER_STATE_IS_OFF;

        pci_save_state(pdev);
        pci_disable_device(pdev);
        pci_set_power_state(pdev, PCI_D3hot);

        return 0;
}

static int xengfx_pm_resume(struct device *dev)
{
        struct pci_dev *pdev = to_pci_dev(dev);

        pci_set_power_state(pdev, PCI_D0);
        pci_restore_state(pdev);

        return pci_enable_device(pdev);
}

static const struct dev_pm_ops xengfx_pm_ops = {
        .suspend = xengfx_pm_suspend,
        .resume = xengfx_pm_resume,
};

static int xengfx_driver_load(struct drm_device *dev, unsigned long driver_data)
{
        struct xengfx_private *dev_priv;
        int error = 0;
        u32 val;
        u32 gart_size, stolen_base, stolen_size;

        dev_priv = kzalloc(sizeof (*dev_priv), GFP_KERNEL);
        if (!dev_priv)
                return -ENOMEM;

        dev_priv->dev = dev;
        dev->dev_private = dev_priv;

        dev_priv->mmio = pci_iomap(dev->pdev, 2, 0); /* map registers + GART BAR */
        if (!dev_priv->mmio) {
                error = -EIO;
                goto err_mmio;
        }

        val = xengfx_mmio_read(dev_priv, XGFX_MAGIC);
        if (val != XGFX_MAGIC_VALID) {
                DRM_ERROR("Invalid MMIO magic number (found: %x, valid: %x)\n",
                          val, XGFX_MAGIC_VALID);
                error = -ENODEV;
                goto err_magic;
        }

        dev_priv->rev = xengfx_mmio_read(dev_priv, XGFX_REV);
        DRM_INFO("Found XenGFX device Rev %d\n", dev_priv->rev);

        /* Reset device before using it */
        xengfx_mmio_read(dev_priv, XGFX_RESET);

        val = xengfx_mmio_read(dev_priv, XGFX_NVCRTC);
        error = drm_vblank_init(dev, val);
        if (error) {
		DRM_ERROR("Failed to initialize vblank\n");
                goto err_vblank;
        }

        /* XGFX_GART_SIZE reports gart_size in number of pages */
        gart_size = xengfx_mmio_read(dev_priv, XGFX_GART_SIZE);
        /* These are PFNs too */
        stolen_base = xengfx_mmio_read(dev_priv, XGFX_STOLEN_BASE);
        stolen_size = xengfx_mmio_read(dev_priv, XGFX_STOLEN_SIZE);

        dev_priv->gart_size = gart_size << PAGE_SHIFT;
        dev_priv->stolen_base = stolen_base << PAGE_SHIFT;
        dev_priv->stolen_size = stolen_size << PAGE_SHIFT;

        dev_priv->aper_base = pci_resource_start(dev->pdev, 0);
        /* 1 page of GART makes 4MB of aperture */
        dev_priv->aper_size = gart_size * (4 * 1024 * 1024);

        /* Give pages from stolen memory to the DRM memrange allocator */
        drm_mm_init(&dev_priv->stolen_mm, dev_priv->stolen_base,
                    dev_priv->stolen_size);

        /* Also, let DRM manage GART space allocation */
        drm_mm_init(&dev_priv->gart_mm, 0, dev_priv->aper_size);

        /* Initialize CRTCs and outputs */
        xengfx_modeset_init(dev);

        error = drm_irq_install(dev);
        if (error) {
                DRM_ERROR("Failed to install IRQ handler");
                goto err_irqinstall;
        }

        error = xengfx_fbdev_init(dev);
        if (error)
                goto err_fbdev;

        DRM_KMS_HELPER_POLL_INIT(dev);

        return 0;
err_fbdev:
        drm_irq_uninstall(dev);
err_irqinstall:
        xengfx_modeset_cleanup(dev);
        drm_mm_takedown(&dev_priv->gart_mm);
        drm_mm_takedown(&dev_priv->stolen_mm);
err_vblank:
err_magic:
        pci_iounmap(dev->pdev, dev_priv->mmio);
err_mmio:
        kfree(dev_priv);
        return error;
}

static int xengfx_driver_unload(struct drm_device *dev)
{
        struct xengfx_private *dev_priv = dev->dev_private;

        /* XXX: Move this to lastclose ? */
        drm_irq_uninstall(dev);

        xengfx_fbdev_cleanup(dev);

        xengfx_modeset_cleanup(dev);

        drm_mm_takedown(&dev_priv->gart_mm);
        drm_mm_takedown(&dev_priv->stolen_mm);

        if (dev_priv->mmio) {
                pci_iounmap(dev->pdev, dev_priv->mmio);
        }

        kfree(dev_priv);

        return 0;
}

static int xengfx_driver_open(struct drm_device *dev, struct drm_file *file)
{
	struct xengfx_file_private *file_priv;

	file_priv = kzalloc(sizeof (*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	return 0;
}

static void xengfx_driver_lastclose(struct drm_device *dev)
{

}

static void xengfx_driver_postclose(struct drm_device *dev,
                                    struct drm_file *file)
{
	struct xengfx_file_private *file_priv = file->driver_priv;

	kfree(file_priv);
}

static int xengfx_master_create(struct drm_device *dev,
                                struct drm_master *master)
{
    /* FIXME */
    return 0;
}

static void xengfx_master_destroy(struct drm_device *dev,
			          struct drm_master *master)
{
    /* FIXME */
}

static struct vm_operations_struct xengfx_gem_vm_ops = {
        .fault = xengfx_gem_fault,
        .open = drm_gem_vm_open,
        .close = drm_gem_vm_close
};

static struct drm_ioctl_desc xengfx_ioctls[] = {
        DRM_IOCTL_DEF_DRV(XENGFX_GEM_CREATE, xengfx_gem_create_ioctl, DRM_UNLOCKED),
        DRM_IOCTL_DEF_DRV(XENGFX_GEM_MAP, xengfx_gem_map_ioctl, DRM_UNLOCKED),
};

static int __devinit xengfx_pci_probe(struct pci_dev *pdev,
                                      const struct pci_device_id *ent)
{
        return DRM_GET_DEV(pdev, ent, &xengfx_drm_driver);
}

static void xengfx_pci_remove(struct pci_dev *pdev)
{
        struct drm_device *dev = pci_get_drvdata(pdev);

        drm_put_dev(dev);
}

static struct drm_driver xengfx_drm_driver = {
        .driver_features =
            DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED |
            DRIVER_MODESET | DRIVER_GEM,
        .load = xengfx_driver_load,
        .unload = xengfx_driver_unload,
        .open = xengfx_driver_open,
        .lastclose = xengfx_driver_lastclose,
        .postclose = xengfx_driver_postclose,

        .get_vblank_counter = xengfx_get_vblank_counter,
        .enable_vblank = xengfx_enable_vblank,
        .disable_vblank = xengfx_disable_vblank,

        GET_SCANOUT_POSITION_IMPLEMENTATION
        // We don't implement the callback so drm thinks we do not support the vblank queries
        //.get_vblank_timestamp = xengfx_get_vblank_timestamp,

        .irq_preinstall = xengfx_irq_preinstall,
	.irq_postinstall = xengfx_irq_postinstall,
	.irq_uninstall = xengfx_irq_uninstall,
	.irq_handler = xengfx_irq_handler,

        .master_create = xengfx_master_create,
        .master_destroy = xengfx_master_destroy,

        .gem_init_object = xengfx_gem_init_object,
        .gem_free_object = xengfx_gem_free_object,
        .gem_vm_ops = &xengfx_gem_vm_ops,

        DUMB_ALLOC_IMPLEMENTATION

        .ioctls = xengfx_ioctls,
        .num_ioctls = DRM_ARRAY_SIZE(xengfx_ioctls),

        .fops = {
                .owner = THIS_MODULE,
                .open = drm_open,
                .release = drm_release,
                IOCTL_IMPLEMENTATION
                .mmap = drm_gem_mmap,
                .poll = drm_poll,
                .fasync = drm_fasync,
                READ_IMPLEMENTATION
#if defined(CONFIG_COMPAT)
                .compat_ioctl = drm_compat_ioctl,
#endif
                LLSEEK_IMPLEMENTATION
        },

        PCI_DRIVER_IMPLEMENTATION

        .name = DRIVER_NAME,
        .desc = DRIVER_DESC,
        .date = DRIVER_DATE,
        .major = DRIVER_MAJOR,
        .minor = DRIVER_MINOR,
        .patchlevel = DRIVER_PATCHLEVEL
};

PCI_DRIVER_STRUCTURE;

static int __init xengfx_init(void)
{
        int ret;

        ret = DRM_INIT(&xengfx_drm_driver, &xengfx_pci_driver);
        if (ret)
                DRM_ERROR("Failed initializing DRM.\n");
        return ret;
}

static void __exit xengfx_exit(void)
{
        DRM_EXIT(&xengfx_drm_driver, &xengfx_pci_driver);
}

module_init(xengfx_init);
module_exit(xengfx_exit);

MODULE_LICENSE("GPL");

