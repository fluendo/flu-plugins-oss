/* rdp444split.h - RDP 444 Splitter element
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * gstrdp444split.h: Header for GstRDP444Split Object
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GSTRDP444SPLIT_H___
#define GSTRDP444SPLIT_H___

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_RDP444_SPLIT (gst_rdp444_split_get_type())
G_DECLARE_FINAL_TYPE (GstRDP444Split, gst_rdp444_split, GST, RDP444_SPLIT, GstElement)

struct _GstRDP444Split
{
  GstElement parent;

  GstPad *sinkpad;
  GstPad *srcpad_yuv420, *srcpad_chroma420;
};

GST_ELEMENT_REGISTER_DECLARE (rdp444split);

G_END_DECLS
#endif
