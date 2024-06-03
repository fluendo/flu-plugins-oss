/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTMLSEGMENTEDPARSE_H__
#define __GST_TTMLSEGMENTEDPARSE_H__

#include <gst/gst.h>
#include <libxml/parser.h>
#include <gst/gst.h>
#include "gst-demo.h"
#include "gstttmlbase.h"
#include "gstttmlstate.h"

G_BEGIN_DECLS

/* The GStreamer ttmlparse element */
typedef struct _GstTTMLSegmentedParse
{
  GstTTMLBase base;
} GstTTMLSegmentedParse;

/* The GStreamer ttmlparse element's class */
typedef struct _GstTTMLSegmentedParseClass
{
  GstTTMLBaseClass parent_class;
} GstTTMLSegmentedParseClass;

#define GST_TYPE_TTMLSEGMENTEDPARSE (gst_ttmlsegmentedparse_get_type ())
#define GST_TTMLSEGMENTEDPARSE(obj)                                           \
  (G_TYPE_CHECK_INSTANCE_CAST (                                               \
      (obj), GST_TYPE_TTMLSEGMENTEDPARSE, GstTTMLSegmentedParse))
#define GST_TTMLSEGMENTEDPARSE_CLASS(klass)                                   \
  (G_TYPE_CHECK_CLASS_CAST (                                                  \
      (klass), GST_TYPE_TTMLSEGMENTEDPARSE, GstTTMLSegmentedParseClass))
#define GST_TTMLSEGMENTEDPARSE_GET_CLASS(obj)                                 \
  (G_TYPE_INSTANCE_GET_CLASS (                                                \
      (obj), GST_TYPE_TTMLSEGMENTEDPARSE, GstTTMLSegmentedParseClass))
#define GST_IS_TTMLSEGMENTEDPARSE(obj)                                        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TTMLSEGMENTEDPARSE))
#define GST_IS_TTMLSEGMENTEDPARSE_CLASS(klass)                                \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TTMLSEGMENTEDPARSE))

GType gst_ttmlsegmentedparse_get_type (void);

G_END_DECLS
#endif /* __GST_TTMLSEGMENTEDPARSE_H__ */
