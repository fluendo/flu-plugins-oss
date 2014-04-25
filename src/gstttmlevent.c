/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlstate.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

/* Free an event. Some of them might have internal allocated memory, so
 * always use this function and do not g_free events directly. */
void
gst_ttml_event_free (GstTTMLEvent *event)
{
  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      if (event->data.span_begin.span)
        gst_ttml_span_free (event->data.span_begin.span);
      break;
    case GST_TTML_EVENT_TYPE_SPAN_ATTR_UPDATE:
      if (event->data.attr_update.attr)
        gst_ttml_attribute_free (event->data.attr_update.attr);
      break;
    default:
      break;
  }
  g_free (event);
}

/* Comparison function for events, using their timestamps */
static gint
gst_ttml_event_compare (GstTTMLEvent *a, GstTTMLEvent *b)
{
  return a->timestamp > b->timestamp ? 1 : -1;
}

/* Creates a new SPAN BEGIN event */
GstTTMLEvent *
gst_ttml_event_new_span_begin (GstTTMLState *state, GstTTMLSpan *span)
{
  GstTTMLEvent *event = g_new0 (GstTTMLEvent, 1);
  if (GST_CLOCK_TIME_IS_VALID (state->begin))
    event->timestamp = state->begin;
  else
    event->timestamp = 0;
  event->type = GST_TTML_EVENT_TYPE_SPAN_BEGIN;
  event->data.span_begin.span = span;
  return event;
}

/* Creates a new SPAN END event */
GstTTMLEvent *
gst_ttml_event_new_span_end (GstTTMLState *state, guint id)
{
  GstTTMLEvent *event = g_new0 (GstTTMLEvent, 1);
  /* Substracting one nanosecond is a cheap way of making intervals
   * open on the right */
  if (GST_CLOCK_TIME_IS_VALID (state->end))
    event->timestamp = state->end - 1;
  else
    event->timestamp = state->end;
  event->type = GST_TTML_EVENT_TYPE_SPAN_END;
  event->data.span_end.id = id;
  return event;
}

/* Creates a new ATTRIBUTE UPDATE event */
GstTTMLEvent *
gst_ttml_event_new_attr_update (guint id,
    GstClockTime timestamp, GstTTMLAttribute *attr)
{
  GstTTMLEvent *event = g_new0 (GstTTMLEvent, 1);
  event->timestamp = timestamp;
  event->type = GST_TTML_EVENT_TYPE_SPAN_ATTR_UPDATE;
  event->data.attr_update.id = id;
  event->data.attr_update.attr = gst_ttml_attribute_copy (attr, FALSE);
  return event;
}

/* Insert an event into an event list (timeline), ordered by timestamp.
 * You lose ownership of the event. */
GList *
gst_ttml_event_list_insert (GList *timeline, GstTTMLEvent *event)
{
  GST_DEBUG ("Inserting event %s at %" GST_TIME_FORMAT,
      gst_ttml_event_type_name (event->type),
      GST_TIME_ARGS (event->timestamp));
  return g_list_insert_sorted (timeline, event,
      (GCompareFunc)gst_ttml_event_compare);
}

/* Returns the first event in the timeline, i.e., the next one.
 * You are the owner of the returned event. */
GList *
gst_ttml_event_list_get_next (GList *timeline, GstTTMLEvent **event)
{
  *event = (GstTTMLEvent *)timeline->data;
  GST_DEBUG ("Removing event %s at %" GST_TIME_FORMAT,
      gst_ttml_event_type_name ((*event)->type),
      GST_TIME_ARGS ((*event)->timestamp));
  return g_list_delete_link (timeline, timeline);
}

/* Remove all events from the timeline, parse them and generate output
 * buffers */
GList *
gst_ttml_event_list_flush (GList *timeline,
    GstTTMLEventParseFunc parse, GstTTMLEventGenBufferFunc gen_buffer,
    void *userdata)
{
  GstTTMLEvent *event;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  if (!timeline) {
    /* Empty timeline, nothing to do */
    return timeline;
  }

  do {
    timeline = gst_ttml_event_list_get_next (timeline, &event);

    if (event->timestamp != time && GST_CLOCK_TIME_IS_VALID (time)) {
      gen_buffer (time, event->timestamp, userdata);
    }
    time = event->timestamp;
    parse (event, userdata);
  } while (timeline);

  /* Generate one last buffer to clear the last span. It will be empty,
   * because the timeline is empty, so its duration does not really matter.
   */
  gen_buffer (time, time + 1, userdata);

  return timeline;
}

/* Get the string representation of an event type (for debugging) */
const gchar *
gst_ttml_event_type_name (GstTTMLEventType type)
{
  switch (type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      return "SPAN_BEGIN";
    case GST_TTML_EVENT_TYPE_SPAN_END:
      return "SPAN_END";
    case GST_TTML_EVENT_TYPE_SPAN_ATTR_UPDATE:
      return "ATTRIBUTE_UPDATE";
    default:
      break;
  }
  return "Unknown!";
}
