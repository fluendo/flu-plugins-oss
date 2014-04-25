/*
 * FLUENDO S.A.
 * Copyright (C) <2014>  <support@fluendo.com>
 */

#ifndef __GST_TTMLBASE_H__
#define __GST_TTMLBASE_H__

#include <gst/gst.h>
#include <libxml/parser.h>
#include "gst-compat.h"
#include "gst-demo.h"
#include "gstttmlstate.h"

G_BEGIN_DECLS
  
/* The GStreamer ttmlbase base element */
typedef struct _GstTTMLBase {
  GstElement element;

  /* Sink pad */
  GstPad *sinkpad;
  /* Sourc pad */
  GstPad *srcpad;

  GstSegment *segment;
  gboolean newsegment_needed;
  GstClockTime base_time;
  GstFlowReturn current_gst_status;

  /* XML parsing */
  xmlParserCtxtPtr xml_parser;
  GstTTMLState state;
  gboolean in_styling_node;
  gboolean in_layout_node;

  /* Properties */
  gboolean assume_ordered_spans;
  gboolean force_buffer_clear;

  /* Timeline management */
  GList *timeline;
  GstClockTime last_event_timestamp;

  /* Active span list */
  GList *active_spans;

  /* For building demo plugins */
  GstFluDemoStatistics stats;
} GstTTMLBase;

/* The GStreamer ttmlbase element's class */
typedef struct _GstTTMLBaseClass {
  GstElementClass parent_class;

  /* Passed a list of active text spans (with attributes), the derived class
   * must generate a GstBuffer (with whatever caps it declared when created
   * the src pad template).
   */
  GstBuffer *(*gen_buffer)(GstTTMLBase *base);
} GstTTMLBaseClass;

#define GST_TYPE_TTMLBASE            (gst_ttmlbase_get_type())
#define GST_TTMLBASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_TTMLBASE, GstTTMLBase))
#define GST_TTMLBASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_TTMLBASE, GstTTMLBaseClass))
#define GST_TTMLBASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_TTMLBASE, GstTTMLBaseClass))
#define GST_IS_TTMLBASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_TTMLBASE))
#define GST_IS_TTMLBASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_TTMLBASE))

GType gst_ttmlbase_get_type (void);

G_END_DECLS
#endif /* __GST_TTMLBASE_H__ */
