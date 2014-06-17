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

#ifndef _XENGFX_REG_H_
#define _XENGFX_REG_H_

#define XGFX_MAGIC                  0x00000000
#define   XGFX_MAGIC_VALID                      0x58464758
#define XGFX_REV                    0x00000004

#define XGFX_CONTROL                0x00000100
#define   XGFX_CONTROL_HIRES_EN                 (1 << 0)
#define   XGFX_CONTROL_INT_EN                   (1 << 1)
#define XGFX_ISR                    0x00000104
#define   XGFX_ISR_INT                          (1 << 0)

#define XGFX_GART_SIZE              0x00000200
#define XGFX_GART_INVAL             0x00000204
#define XGFX_STOLEN_BASE            0x00000208
#define XGFX_STOLEN_SIZE            0x0000020C
#define XGFX_STOLEN_CLEAR           0x00000210
#define XGFX_NVCRTC                 0x00000300
#define XGFX_RESET                  0x00000400
#define XGFX_MADVISE                0x00001000

#define XGFX_VCRTC(c,reg)           (XGFX_VCRTC_##reg + (c) * 0x10000)

#define XGFX_VCRTC_STATUS           0x00100000
#define XGFX_VCRTC_STATUS_CHANGE    0x00100004
#define XGFX_VCRTC_STATUS_INT       0x00100008
#define   XGFX_VCRTC_STATUS_HOTPLUG             (1 << 0)
#define   XGFX_VCRTC_STATUS_ONSCREEN            (1 << 1)
#define   XGFX_VCRTC_STATUS_RETRACE             (1 << 2)
#define XGFX_VCRTC_SCANLINE         0x0010000C
#define XGFX_VCRTC_CURSOR_STATUS    0x00100010
#define   XGFX_VCRTC_CURSOR_STATUS_SUPPORTED    (1 << 0)
#define XGFX_VCRTC_CURSOR_CONTROL   0x00100014
#define   XGFX_VCRTC_CURSOR_CONTROL_SHOW        (1 << 0)
#define XGFX_VCRTC_CURSOR_MAXSIZE   0x00100018
#define XGFX_VCRTC_CURSOR_SIZE      0x0010001C
#define   XGFX_VCRTC_CURSOR_Y_MASK              (0xffff << 0)
#define   XGFX_VCRTC_CURSOR_Y_SHIFT             0
#define   XGFX_VCRTC_CURSOR_X_MASK              (0xffff << 16)
#define   XGFX_VCRTC_CURSOR_X_SHIFT             16
#define XGFX_VCRTC_CURSOR_BASE      0x00100020
#define XGFX_VCRTC_CURSOR_POS       0x00100024

#define XGFX_VCRTC_EDID_REQUEST     0x00101000

#define XGFX_VCRTC_CONTROL          0x00102000
#define   XGFX_VCRTC_CONTROL_ENABLE             (1 << 0)
#define XGFX_VCRTC_VALID_FORMAT     0x00102004
#define XGFX_VCRTC_FORMAT           0x00102008
#define   XGFX_FORMAT_RGB555                    (1 << 0)
#define   XGFX_FORMAT_BGR555                    (1 << 1)
#define   XGFX_FORMAT_RGB565                    (1 << 2)
#define   XGFX_FORMAT_BGR565                    (1 << 3)
#define   XGFX_FORMAT_RGB888                    (1 << 4)
#define   XGFX_FORMAT_BGR888                    (1 << 5)
#define   XGFX_FORMAT_RGB8888                   (1 << 6)
#define   XGFX_FORMAT_BGR8888                   (1 << 7)
#define XGFX_VCRTC_MAX_HORIZONTAL   0x00102010
#define XGFX_VCRTC_H_ACTIVE         0x00102014
#define XGFX_VCRTC_MAX_VERTICAL     0x00102018
#define XGFX_VCRTC_V_ACTIVE         0x0010201C
#define XGFX_VCRTC_STRIDE_ALIGNMENT 0x00102020
#define XGFX_VCRTC_STRIDE           0x00102024

#define XGFX_VCRTC_BASE             0x00103000

#define XGFX_VCRTC_LINEOFFSET       0x00104000
#define XGFX_VCRTC_EDID             0x00105000

#define XGFX_GART_BASE              0x00200000
#define   XGFX_GART_BIT_USED                    31
#define   XGFX_GART_BIT_RESERVED                30
#define   XGFX_GART_PFN_MASK                    ~((1 << XGFX_GART_BIT_USED)|            \
                                                  (1 << XGFX_GART_BIT_RESERVED))
#define   XGFX_GART_ENTRY_VALID                 (1 << XGFX_GART_BIT_USED)

#endif /* _XENGFX_REG_H_ */
