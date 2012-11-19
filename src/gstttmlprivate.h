/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTMLPRIVATE_H__
#define __GST_TTMLPRIVATE_H__

#include <gst/gst.h>
#include <gstttmlparse.h>

G_BEGIN_DECLS

/* The GStreamer ttmlparse element */
typedef struct _GstTTMLParse
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

  gboolean assume_ordered_spans;

  /* Timeline management */
  GList *timeline;
} GstTTMLParse;

/* The GStreamer ttmlparse element's class */
typedef struct _GstTTMLParseClass
{
  GstElementClass parent_class;
} GstTTMLParseClass;

/* A text scan, with all attributes, except timing info, that is stored in the
 * timeline */
typedef struct _GstTTMLScan
{
  guint id;
  const gchar *text;
} GstTTMLScan;

/* Event types */
typedef enum {
  GST_TTML_EVENT_SCAN_BEGIN,
  GST_TTML_EVENT_SCAN_END
} GstTTMLEventType;

/* An event to be stored in the timeline. It has a type and a timestamp */
typedef struct _GstTTMLEvent
{
  GstClockTime timestamp;
  GstTTMLEventType type;
  void *data;
} GstTTMLEvent;

G_END_DECLS
#endif /* __GST_TTMLPRIVATE_H__ */
