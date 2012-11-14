/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTMLPARSE_H__
#define __GST_TTMLPARSE_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_TTMLPARSE            (gst_ttmlparse_get_type())
#define GST_TTMLPARSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_TTMLPARSE, GstTTMLParse))
#define GST_TTMLPARSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_TTMLPARSE, GstTTMLParseClass))
#define GST_TTMLPARSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_TTMLPARSE, GstTTMLParseClass))
#define GST_IS_TTMLPARSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_TTMLPARSE))
#define GST_IS_TTMLPARSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_TTMLPARSE))
typedef struct _GstTTMLParse GstTTMLParse;
typedef struct _GstTTMLParseClass GstTTMLParseClass;

struct _GstTTMLParse
{
  GstElement element;

  /* Sink pad */
  GstPad *sinkpad;
  /* Sourc pad */
  GstPad *srcpad;

  GstSegment *segment;
  gboolean newsegment_needed;

  xmlParserCtxtPtr xml_parser;
  gboolean inside_p;
  GstBuffer *current_p;
  GstClockTime current_begin;
  GstClockTime current_end;
  GstClockTime current_pts;
  GstFlowReturn current_status;
};

struct _GstTTMLParseClass
{
  GstElementClass parent_class;
};

GType gst_ttmlparse_get_type (void);
GType gst_flussdemux_get_type (void);

G_END_DECLS
#endif /* __GST_TTMLPARSE_H__ */
