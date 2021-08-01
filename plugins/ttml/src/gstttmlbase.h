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

typedef struct
{
  gchar *data;
  gsize size;
  gsize len;
  gboolean enable;
  gboolean preserve_whitespace;
  gboolean insert_space;
  gboolean collapsing;
  gboolean line_has_chars;
} GstTTMLBuffer;

/* The GStreamer ttmlbase base element */
typedef struct _GstTTMLBase
{
  GstElement element;

  /* Sink pad */
  GstPad *sinkpad;
  /* Sourc pad */
  GstPad *srcpad;

  GstSegment *segment;
  GstSegment *pending_segment;
  gboolean newsegment_needed;

  GstFlowReturn current_gst_status;

  /* XML parsing */
  xmlParserCtxtPtr xml_parser;
  GstTTMLState state;
  GList *namespaces;
  gboolean is_std_ebu;
  gboolean in_styling_node;
  gboolean in_layout_node;
  gboolean in_metadata_node;

  /* Properties */
  gboolean assume_ordered_spans;

  /* Timeline management */
  GList *timeline;
  GstClockTime input_buf_start;
  GstClockTime input_buf_stop;
  GstClockTime base_time;
  GstClockTime last_out_time;

  /* Active span list */
  GList *active_spans;

  /* buffer to accumulate xml node content */
  GstTTMLBuffer buffer;

  /* To build demo plugins */
  GstFluDemoStatistics stats;
} GstTTMLBase;

/* The GStreamer ttmlbase element's class */
typedef struct _GstTTMLBaseClass
{
  GstElementClass parent_class;

  /* Passed a list of active text spans (with attributes), the derived class
   * must generate a GstBuffer (with whatever caps it declared when created
   * the src pad template).
   */
  GstBuffer *(*gen_buffer) (
      GstTTMLBase *base, GstClockTime ts, GstClockTime duration);

  /* Derived classes can set any unfixed value to whatever they please.
   * Used to set a default video size, if downstream does not request one,
   * for example. */
  void (*fixate_caps) (GstTTMLBase *base, GstCaps *caps);

  /* Before downstream negotiation, derived classes can use this method to
   * complete the caps they advertised in the pad template, to add info
   * found during parsing which was not present in class_init. */
  void (*complete_caps) (GstTTMLBase *base, GstCaps *caps);

  /* Inform the derived class of the final negotiated caps on its srcpad */
  void (*src_setcaps) (GstTTMLBase *base, GstCaps *caps);

  /* Tell the derived class to clear all internal data and go back to the
   * reset state. */
  void (*reset) (GstTTMLBase *base);
} GstTTMLBaseClass;

#define GST_TYPE_TTMLBASE (gst_ttmlbase_get_type ())
#define GST_TTMLBASE(obj)                                                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TTMLBASE, GstTTMLBase))
#define GST_TTMLBASE_CLASS(klass)                                             \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TTMLBASE, GstTTMLBaseClass))
#define GST_TTMLBASE_GET_CLASS(obj)                                           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TTMLBASE, GstTTMLBaseClass))
#define GST_IS_TTMLBASE(obj)                                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TTMLBASE))
#define GST_IS_TTMLBASE_CLASS(klass)                                          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TTMLBASE))

GType gst_ttmlbase_get_type (void);

gchar *gst_ttmlbase_uri_get (GstPad *pad);

G_END_DECLS
#endif /* __GST_TTMLBASE_H__ */
