/* injectbin
 * Copyright (C) 2023 Alexander Slobodeniuk <aslobodeniuk@fluendo.com>
 *
 * gstinjectbin.h: Header for GstInjectBin object
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

#ifndef GSTINJECTBIN_H___
#define GSTINJECTBIN_H___

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstInjectBin GstInjectBin;
typedef struct _GstInjectBinClass GstInjectBinClass;

#define GST_TYPE_INJECT_BIN             (gst_inject_bin_get_type())
#define GST_INJECT_BIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_INJECT_BIN, GstInjectBin))
#define GST_INJECT_BIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_INJECT_BIN, GstInjectBinClass))
#define GST_INJECT_BIN_CAST(obj)        ((GstInjectBin *)(obj))
#define GST_IS_INJECT_BIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_INJECT_BIN))
#define GST_IS_IBJECT_BIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_INJECT_BIN))

struct _GstInjectBin
{
  GstBin parent;

  GRecMutex lock;
  GstElement *current_element;
  GstElement *requested_element;
  GstPad *sinkpad, *srcpad;
  GstElement *input_identity;
  GstPad *identity_sinkpad;
  GstPad *identity_srcpad;
  gulong last_probe_id;
};

struct _GstInjectBinClass
{
  GstBinClass parent_class;
};

GType gst_inject_bin_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (injectbin);

G_END_DECLS
#endif
