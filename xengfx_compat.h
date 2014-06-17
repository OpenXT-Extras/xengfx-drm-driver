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

#include <linux/version.h>

#define XENGFX_PCI_DRIVER       \
{                               \
  .name = DRIVER_NAME,          \
  .id_table = pciidlist,        \
  .probe = xengfx_pci_probe,    \
  .remove = xengfx_pci_remove,  \
    .driver = {                 \
      .pm = &xengfx_pm_ops      \
    }                           \
}

int xengfx_fbdev_init_compat(struct drm_device *dev);
void xengfx_gem_object_release(struct drm_gem_object *gem_obj);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))

#define DRM_CALLOC(a,b) \
    drm_malloc_ab(a, b);

#else

#define DRM_CALLOC(a,b) \
    drm_calloc_large(a, b);

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))

#define IOCTL_IMPLEMENTATION \
  .unlocked_ioctl = drm_ioctl,
#define READ_IMPLEMENTATION \
  .read = drm_read,

#else

#define IOCTL_IMPLEMENTATION \
  .ioctl = drm_ioctl,
#define READ_IMPLEMENTATION

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))

#define EDID_IS_VALID drm_edid_is_valid
#define DRM_GEM_OBJECT_UNREFERENCE(a) drm_gem_object_unreference_unlocked(a)

#else

bool xengfx_edid_is_valid(struct edid *edid);
#define EDID_IS_VALID xengfx_edid_is_valid
#define DRM_GEM_OBJECT_UNREFERENCE(a) drm_gem_object_unreference(a)

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))

#define CRTC_HELPER_SET_CONFIG_EXTRA_WORK

void xengfx_output_poll_changed(struct drm_device *dev);
#define XENGFX_MODE_FUNCS_COMPAT \
        .output_poll_changed = xengfx_output_poll_changed

#define DRM_KMS_HELPER_POLL_INIT(a) \
  drm_kms_helper_poll_init(a)
#define LLSEEK_IMPLEMENTATION \
  .llseek = noop_llseek,

int xengfx_fb_probe(struct drm_fb_helper *helper,
                    struct drm_fb_helper_surface_size *sizes);
#define FB_PROBE_IMPLEMENTATION \
    .fb_probe = xengfx_fb_probe,

#define fbdev_to_fb_info(a) \
    a->helper.fbdev;
#define DRM_FB_HELPER_FREE(a) \
    drm_fb_helper_fini(a);

#else

void xengfx_crtc_unpin_framebuffer(struct drm_mode_set *set);
#define CRTC_HELPER_SET_CONFIG_EXTRA_WORK xengfx_crtc_unpin_framebuffer(set)

int xengfx_fb_probe(struct drm_device *dev,
                    uint32_t fb_width, uint32_t fb_height,
                    uint32_t surface_width, uint32_t surface_height,
                    uint32_t surface_depth, uint32_t surface_bpp,
                    struct drm_framebuffer **fb_p);
int xengfx_fb_changed(struct drm_device *dev);
#define XENGFX_MODE_FUNCS_COMPAT \
        .fb_changed = xengfx_fb_changed

#define DRM_KMS_HELPER_POLL_INIT(a)
#define LLSEEK_IMPLEMENTATION
#define FB_PROBE_IMPLEMENTATION

#define fbdev_to_fb_info(a) \
    a->fb.drm_fb.fbdev;
#define DRM_FB_HELPER_FREE(a) \
    drm_fb_helper_free(a);

int drm_gem_object_init(struct drm_device *dev,
                        struct drm_gem_object *obj,
                        size_t size);

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))

#define XENGFX_CONNECTOR_DETECT_SIGNATURE \
  xengfx_connector_detect(struct drm_connector *connector, bool force)
#define XENGFX_CONNECTOR_DETECT_CALL(a) \
  xengfx_connector_detect(a, true)
#define XENGFX_CRTC_GAMMA_SET_SIGNATURE \
  xengfx_crtc_gamma_set(struct drm_crtc *drm_crtc, u16 *red, u16 *green, \
      u16 *blue, uint32_t start, uint32_t size)
#define DRM_GET_DEV(a, b, c) drm_get_pci_dev(a, b, c)

#define FB_DEBUG_OPS \
  .fb_debug_enter = drm_fb_helper_debug_enter, \
  .fb_debug_leave = drm_fb_helper_debug_leave,


#else

#define XENGFX_CONNECTOR_DETECT_SIGNATURE \
  xengfx_connector_detect(struct drm_connector *connector)
#define XENGFX_CONNECTOR_DETECT_CALL(a) \
  xengfx_connector_detect(a)
#define XENGFX_CRTC_GAMMA_SET_SIGNATURE \
  xengfx_crtc_gamma_set(struct drm_crtc *drm_crtc, u16 *red, u16 *green, \
      u16 *blue, uint32_t size)
#define DRM_GET_DEV(a, b, c) drm_get_dev(a, b, c)
#define FB_DEBUG_OPS

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))

#define CHECK_IF_POWER_STATE_IS_OFF \
        if (drm_dev->switch_power_state == DRM_SWITCH_POWER_OFF) \
                return 0

#define GET_SCANOUT_POSITION_IMPLEMENTATION \
  .get_scanout_position = xengfx_get_scanout_position,

int xengfx_get_scanout_position(struct drm_device *dev, int crtc,
                                int *vpos, int *hpos);

#else

#define CHECK_IF_POWER_STATE_IS_OFF
#define GET_SCANOUT_POSITION_IMPLEMENTATION

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))

int xengfx_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
                           struct drm_mode_create_dumb *args);
int xengfx_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
                               uint32_t handle, uint64_t *offset);
int xengfx_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
                            uint32_t handle);

#define PCI_DRIVER_IMPLEMENTATION
#define PCI_DRIVER_STRUCTURE    \
  static struct pci_driver xengfx_pci_driver = XENGFX_PCI_DRIVER
#define DRM_INIT(a,b) drm_pci_init(a, b)
#define DRM_EXIT(a,b) drm_pci_exit(a, b)

#define DUMB_ALLOC_IMPLEMENTATION                       \
    .dumb_create = &xengfx_gem_dumb_create,             \
    .dumb_map_offset = &xengfx_gem_dumb_map_offset,     \
    .dumb_destroy = &xengfx_gem_dumb_destroy,

#else

#define PCI_DRIVER_IMPLEMENTATION \
  .pci_driver = XENGFX_PCI_DRIVER,
#define PCI_DRIVER_STRUCTURE
#define DRM_INIT(a,b) drm_init(a)
#define DRM_EXIT(a,b) drm_exit(a)

#define DUMB_ALLOC_IMPLEMENTATION

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))

#include <linux/shmem_fs.h>
#define READ_PAGE_GFP(a,b,c) \
    shmem_read_mapping_page_gfp(a, b, c)

#else

#define READ_PAGE_GFP(a,b,c) \
   read_cache_page_gfp(a, b, c)

#endif
