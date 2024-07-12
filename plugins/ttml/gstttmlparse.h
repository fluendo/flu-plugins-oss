/*
 * Copyright 2012 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef __GST_TTMLPARSE_H__
#define __GST_TTMLPARSE_H__

#include <gst/gst.h>
#include <libxml/parser.h>
#include <gst/gst.h>
#include "gstttmlbase.h"
#include "gstttmlstate.h"

G_BEGIN_DECLS

/* The GStreamer ttmlparse element */
typedef struct _GstTTMLParse
{
  GstTTMLBase base;
} GstTTMLParse;

/* The GStreamer ttmlparse element's class */
typedef struct _GstTTMLParseClass
{
  GstTTMLBaseClass parent_class;
} GstTTMLParseClass;

#define GST_TYPE_TTMLPARSE (gst_ttmlparse_get_type ())
#define GST_TTMLPARSE(obj)                                                    \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TTMLPARSE, GstTTMLParse))
#define GST_TTMLPARSE_CLASS(klass)                                            \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TTMLPARSE, GstTTMLParseClass))
#define GST_TTMLPARSE_GET_CLASS(obj)                                          \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TTMLPARSE, GstTTMLParseClass))
#define GST_IS_TTMLPARSE(obj)                                                 \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TTMLPARSE))
#define GST_IS_TTMLPARSE_CLASS(klass)                                         \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TTMLPARSE))

GType gst_ttmlparse_get_type (void);

G_END_DECLS
#endif /* __GST_TTMLPARSE_H__ */
