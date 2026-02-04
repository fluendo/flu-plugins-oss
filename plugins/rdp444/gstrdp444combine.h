/* rdp444combine.h - RDP 444 Combiner element
 * Copyright (C) 2026 Fluendo <engineering@fluendo.com>
 *
 * gstrdp444combine.h: Header for GstRDP444Combine Object
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

#ifndef GSTRDP444COMBINE_H___
#define GSTRDP444COMBINE_H___

#include <gst/gst.h>
#include <gst/base/gstaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_RDP444_COMBINE (gst_rdp444_combine_get_type())
G_DECLARE_FINAL_TYPE (GstRDP444Combine, gst_rdp444_combine, GST, RDP444_COMBINE, GstAggregator)

struct _GstRDP444Combine
{
  GstAggregator parent;
};

GST_ELEMENT_REGISTER_DECLARE (rdp444combine);

G_END_DECLS
#endif
