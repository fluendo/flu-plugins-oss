/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_EVENT_H__
#define __GST_TTML_EVENT_H__

#include "gst-compat.h"
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

typedef struct _GstTTMLEventSpanBegin {
  GstTTMLSpan *span;
} GstTTMLEventSpanBegin;

typedef struct _GstTTMLEventSpanEnd {
  guint id;
} GstTTMLEventSpanEnd;

/* An event to be stored in the timeline. It has a type, a timestamp and
 * type-specific data. */
struct _GstTTMLEvent
{
  GstClockTime timestamp;
  GstTTMLEventType type;
  union _GstTTMLEventData {
    GstTTMLEventSpanBegin span_begin;
    GstTTMLEventSpanEnd span_end;
  } data;
};

typedef void (* GstTTMLEventParseFunc)(GstTTMLEvent *event,
    void *userdata);

typedef void (* GstTTMLEventGenBufferFunc)(GstClockTime begin,
    GstClockTime end, void *userdata);

void gst_ttml_event_free (GstTTMLEvent *event);

GstTTMLEvent * gst_ttml_event_new_span_begin (GstTTMLState *state,
    GstTTMLSpan *span);

GstTTMLEvent *gst_ttml_event_new_span_end (GstTTMLState *state,
    guint id);

GList *gst_ttml_event_list_insert (GList *timeline, GstTTMLEvent *event);

GList *gst_ttml_event_list_get_next (GList *timeline,
      GstTTMLEvent **event);

GList *gst_ttml_event_list_flush (GList *timeline,
    GstTTMLEventParseFunc parse,
    GstTTMLEventGenBufferFunc gen_buffer,
    void *userdata);

G_END_DECLS

#endif /* __GST_TTML_EVENT_H__ */
