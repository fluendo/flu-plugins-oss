/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTMLPRIVATE_H__
#define __GST_TTMLPRIVATE_H__

#include <gst/gst.h>
#include "gstttmlparse.h"

G_BEGIN_DECLS

/* Attributes currently supported */
typedef enum _GstTTMLAttributeType {
  GST_TTML_ATTR_NODE_TYPE,
  GST_TTML_ATTR_BEGIN,
  GST_TTML_ATTR_END,
  GST_TTML_ATTR_DUR,
  GST_TTML_ATTR_TICK_RATE,
  GST_TTML_ATTR_FRAME_RATE,
  GST_TTML_ATTR_FRAME_RATE_MULTIPLIER,
  GST_TTML_ATTR_WHITESPACE_PRESERVE
} GstTTMLAttributeType;

/* Types of TTML nodes */
typedef enum _GstTTMLNodeType {
  GST_TTML_NODE_TYPE_UNKNOWN,
  GST_TTML_NODE_TYPE_P,
  GST_TTML_NODE_TYPE_SPAN,
  GST_TTML_NODE_TYPE_BR
} GstTTMLNodeType;

/* A stored attribute */
typedef struct _GstTTMLAttribute {
  GstTTMLAttributeType type;
  union {
    GstTTMLNodeType node_type;
    GstClockTime time;
    gdouble d;
    gboolean b;
    struct {
      gint num;
      gint den;
    };
  } value;
} GstTTMLAttribute;

/* Current state of all attributes */
typedef struct _GstTTMLState {
  GstTTMLNodeType node_type;
  guint last_span_id;
  GstClockTime begin;
  GstClockTime end;
  GstClockTime container_begin;
  GstClockTime container_end;
  gdouble tick_rate;
  gdouble frame_rate;
  gdouble frame_rate_num;
  gdouble frame_rate_den;
  gboolean whitespace_preserve;

  GList *history;
} GstTTMLState;

/* The GStreamer ttmlparse element */
typedef struct _GstTTMLParse {
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
  guint current_span_id;
  GstTTMLState state;

  /* Properties */
  gboolean assume_ordered_spans;

  /* Timeline management */
  GList *timeline;
  GstClockTime last_event_timestamp;

  /* Active span list */
  GList *active_spans;
} GstTTMLParse;

/* The GStreamer ttmlparse element's class */
typedef struct _GstTTMLParseClass {
  GstElementClass parent_class;
} GstTTMLParseClass;

/* A text span, with all attributes, except timing info, that is stored in the
 * timeline */
typedef struct _GstTTMLSpan {
  guint id;
  guint length;
  gchar *chars;
} GstTTMLSpan;

typedef struct _GstTTMLEventSpanBegin {
  GstTTMLSpan *span;
} GstTTMLEventSpanBegin;

typedef struct _GstTTMLEventSpanEnd {
  guint id;
} GstTTMLEventSpanEnd;

/* Event types */
typedef enum _GstTTMLEventType {
  GST_TTML_EVENT_TYPE_SPAN_BEGIN,
  GST_TTML_EVENT_TYPE_SPAN_END
} GstTTMLEventType;

/* An event to be stored in the timeline. It has a type, a timestamp and
 * type-specific data. */
typedef struct _GstTTMLEvent
{
  GstClockTime timestamp;
  GstTTMLEventType type;
  union {
    GstTTMLEventSpanBegin span_begin;
    GstTTMLEventSpanEnd span_end;
  };
} GstTTMLEvent;

G_END_DECLS

#ifndef GST_MEMDUMP
#define GST_MEMDUMP(title, data, len) while(0) {}
#endif

#ifndef GST_MEMDUMP_OBJECT
#define GST_MEMDUMP_OBJECT(obj, title, data, len) while(0) {}
#endif

#endif /* __GST_TTMLPRIVATE_H__ */
