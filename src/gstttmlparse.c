/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstttmlprivate.h"
#include "gstttmltype.h"

#include <libxml/parser.h>

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

static GstElementDetails ttmlparse_details = {
  "TTML subtitle parser",
  "Codec/Parser/Subtitle",
  "Parse TTML subtitle streams into text stream",
  "Fluendo S.A. <support@fluendo.com>",
};

enum
{
  PROP_0,
  PROP_ASSUME_ORDERED_SPANS,
};

static GstStaticPadTemplate ttmlparse_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/plain; text/x-pango-markup"));

static GstStaticPadTemplate ttmlparse_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_MIME));

static GstElementClass *parent_class = NULL;

/* Free a text span */
static void
gst_ttmlparse_span_free (GstTTMLSpan *span)
{
  g_free (span->chars);
  g_free (span);
}

/* Create a new text span. Timing information does not belong to the span
 * but to the event that contains it. */
static GstTTMLSpan *
gst_ttmlparse_span_new (guint id, guint length, const gchar *chars)
{
  GstTTMLSpan *span = g_new (GstTTMLSpan, 1);
  span->id = id;
  span->length = length;
  span->chars = g_memdup (chars, length);
  return span;
}

/* Comparison function for spans */
static gint
gst_ttmlparse_span_compare_id (GstTTMLSpan *a, guint *id)
{
  return a->id - *id;
}

/* Insert a span into the active spans list. The list takes ownership. */
static GList *
gst_ttmlparse_active_spans_add (GList *active_spans, GstTTMLSpan *span)
{
  GST_DEBUG ("Inserting span with id %d, length %d", span->id, span->length);
  /* Insert the spans sorted by ID, so they keep the order they had in the
   * XML file. */
  return g_list_insert_sorted (active_spans, span,
      (GCompareFunc)gst_ttmlparse_span_compare_id);
}

/* Remove the span with the given ID from the list of active spans and 
 * free it */
static GList *
gst_ttmlparse_active_spans_remove (GList *active_spans, guint id)
{
  GST_DEBUG ("Removing span with id %d", id);
  GList *link = g_list_find_custom (active_spans, &id,
      (GCompareFunc)gst_ttmlparse_span_compare_id);
  if (!link) {
    GST_WARNING ("Could not find span with id %d", id);
    return active_spans;
  }
  gst_ttmlparse_span_free ((GstTTMLSpan *)link->data);
  link->data = NULL;
  return g_list_delete_link (active_spans, link);
}

/* Free an event. Some of them might have internal allocated memory, so
 * always use this function and do not g_free events directly. */
static void
gst_ttmlparse_event_free (GstTTMLEvent *event)
{
  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      if (event->span_begin.span)
        gst_ttmlparse_span_free (event->span_begin.span);
      break;
    default:
      break;
  }
  g_free (event);
}

/* Comparison function for events, using their timestamps */
static gint
gst_ttmlparse_event_compare (GstTTMLEvent *a, GstTTMLEvent *b)
{
  return a->timestamp > b->timestamp ? 1 : -1;
}

/* Creates a new SPAN BEGIN event */
static GstTTMLEvent *
gst_ttmlparse_event_new_span_begin (GstTTMLState *state, GstTTMLSpan *span)
{
  GstTTMLEvent *event = g_new0(GstTTMLEvent, 1);
  if (GST_CLOCK_TIME_IS_VALID (state->begin))
    event->timestamp = state->begin;
  else
    event->timestamp = 0;
  event->type = GST_TTML_EVENT_TYPE_SPAN_BEGIN;
  event->span_begin.span = span;
  return event;
}

/* Creates a new SPAN END event */
static GstTTMLEvent *
gst_ttmlparse_event_new_span_end (GstTTMLState *state, guint id)
{
  GstTTMLEvent *event = g_new0(GstTTMLEvent, 1);
  event->timestamp = state->end;
  event->type = GST_TTML_EVENT_TYPE_SPAN_END;
  event->span_end.id = id;
  return event;
}

/* Execute the given event */
static void
gst_ttmlparse_event_parse (GstTTMLParse *parse, GstTTMLEvent *event)
{
  GstTTMLSpan *span;
  guint id;

  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      span = event->span_begin.span;
      parse->active_spans =
          gst_ttmlparse_active_spans_add (parse->active_spans, span);
      /* Remove the span from the event, so that when we free the event below
       * the span does not get fred too (it belongs to the active_spans list
       * now) */
      event->span_begin.span = NULL;
      break;
    case GST_TTML_EVENT_TYPE_SPAN_END:
      id = event->span_end.id;
      parse->active_spans =
          gst_ttmlparse_active_spans_remove (parse->active_spans, id);
      break;
    default:
      GST_WARNING ("Unknown event type");
      break;
  }
  gst_ttmlparse_event_free (event);
}

/* Insert an event in the timeline, ordered by timestamp. You loose ownership
 * of the event. */
static GList *
gst_ttmlparse_timeline_insert_event (GList *timeline, GstTTMLEvent *event)
{
  GST_DEBUG ("Inserting event type %d at %" GST_TIME_FORMAT,
      event->type, GST_TIME_ARGS (event->timestamp));
  return g_list_insert_sorted (timeline, event,
    (GCompareFunc)gst_ttmlparse_event_compare);
}

/* Returns the first event in the timeline, i.e., the next one.
 * You are the owner of the returned event. */
static GList *
gst_ttmlparse_timeline_get_next_event (GList *timeline, GstTTMLEvent **event)
{
  *event = (GstTTMLEvent *)timeline->data;
  return g_list_delete_link (timeline, timeline);
}

/* Check if the given node or attribute name matches a type, disregarding
 * possible namespaces */
static gboolean
gst_ttmlparse_element_is_type (const gchar * name, const gchar * type)
{
  if (!g_ascii_strcasecmp (name, type))
    return TRUE;
  if (strlen (name) > strlen (type)) {
    const gchar *suffix = name + strlen (name) - strlen (type);
    if (suffix[-1] == ':' && !g_ascii_strcasecmp (suffix, type))
      return TRUE;
  }
  return FALSE;
}

/* Parse both types of time expressions as specified in the TTML specification,
 * be it in 00:00:00:00 or 00s forms */
static GstClockTime
gst_ttmlparse_parse_time_expression (const GstTTMLState *state,
    const gchar *expr)
{
  gdouble h, m, s, count;
  char metric[3] = "\0\0";
  GstClockTime res = GST_CLOCK_TIME_NONE;

  if (sscanf (expr, "%lf:%lf:%lf", &h, &m, &s) == 3) {
    res = (h * 3600 + m * 60 + s) * GST_SECOND;
  } else if (sscanf (expr, "%lf%2[hmstf]", &count, metric) == 2) {
    double scale = 0;
    switch (metric[0]) {
      case 'h':
        scale = 3600 * GST_SECOND;
        break;
      case 'm':
        if (metric[1] == 's')
          scale = GST_MSECOND;
        else
          scale = 60 * GST_SECOND;
        break;
      case 's':
        scale = GST_SECOND;
        break;
      case 't':
        scale = state->tick_period;
        break;
      default:
        /* TODO: Handle 'f' metric */
        GST_WARNING ("Unknown metric %s", metric);
    }
    res = count * scale;
  } else {
    GST_WARNING ("Unrecognized time expression: %s", expr);
  }
  GST_LOG ("Parsed %s into %" GST_TIME_FORMAT, expr, GST_TIME_ARGS (res));
  return res;
}

/* Read a name-value pair of strings and produce a new GstTTMLattribute.
 * Returns NULL if the attribute was unknown, and uses g_new to allocate
 * the new attribute. */
static GstTTMLAttribute *
gst_ttmlparse_attribute_parse (const GstTTMLState *state, const char *name,
    const char *value)
{
  GstTTMLAttribute *attr;
  GST_LOG ("Parsing %s=%s", name, value);
  if (gst_ttmlparse_element_is_type (name, "begin")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_BEGIN;
    attr->value.time = gst_ttmlparse_parse_time_expression (state, value);
  } else if (gst_ttmlparse_element_is_type (name, "end")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_END;
    attr->value.time = gst_ttmlparse_parse_time_expression (state, value);
  } else if (gst_ttmlparse_element_is_type (name, "dur")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_DUR;
    attr->value.time = gst_ttmlparse_parse_time_expression (state, value);
  } else if (gst_ttmlparse_element_is_type (name, "tickRate")) {
    attr = g_new (GstTTMLAttribute, 1);
    attr->type = GST_TTML_ATTR_TICK_PERIOD;
    attr->value.d = GST_SECOND / g_ascii_strtod (value, NULL);
    GST_LOG ("Parsed '%s' ticks per second into a %g tick period", value,
        attr->value.d);
  } else {
    attr = NULL;
    GST_DEBUG ("  Skipping unknown attribute: %s=%s", name, value);
  }

  return attr;
}

/* Deallocates a GstTTMLAttribute. Required for possible attributes with
 * allocated internal memory. */
static void
gst_ttmlparse_attribute_free (GstTTMLAttribute *attr)
{
  /* Placeholder for future attributes which might want to free some internal
   * memory before being destroyed. */
  switch (attr->type) {
    default:
      break;
  }
  g_free (attr);
}

/* Create a new "node_type" attribute. All other attribute types are created
 * in the _attribute_parse() method above. */
static GstTTMLAttribute *
gst_ttmlparse_attribute_new_node (GstTTMLNodeType node_type)
{
  GstTTMLAttribute *attr = g_new (GstTTMLAttribute, 1);
  attr->type = GST_TTML_ATTR_NODE_TYPE;
  attr->value.node_type = node_type;
  return attr;
}

/* Puts the given GstTTMLAttribute into the state, overwritting the current
 * value. Normally you would use gst_ttmlparse_state_push_attribute() to
 * store the current value into an attribute stack before overwritting it. */
static void
gst_ttmlparse_state_set_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      state->node_type = attr->value.node_type;
      break;
    case GST_TTML_ATTR_BEGIN:
      state->begin = attr->value.time;
      break;
    case GST_TTML_ATTR_END:
      state->end = attr->value.time;
      break;
    case GST_TTML_ATTR_DUR:
      state->end = state->begin + attr->value.time;
      break;
    case GST_TTML_ATTR_TICK_PERIOD:
      state->tick_period = attr->value.d;
      break;
    default:
      GST_DEBUG ("Unknown attribute type %d", attr->type);
      break;
  }
}

/* MERGES the given GstTTMLAttribute into the state. The effect of the merge
 * depends on the type of attribute. By default it calls the _set_ method
 * above. */
static void
gst_ttmlparse_state_merge_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_BEGIN:
      state->begin = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->container_begin))
        state->begin += state->container_begin;
      break;
    case GST_TTML_ATTR_END:
      state->end = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->container_begin))
        state->end += state->container_begin;
      if (GST_CLOCK_TIME_IS_VALID (state->container_end))
        state->end = MIN (state->end, state->container_end);
      break;
    case GST_TTML_ATTR_DUR:
      state->end = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->begin))
        state->end += state->begin;
      if (GST_CLOCK_TIME_IS_VALID (state->container_begin))
        state->end += state->container_begin;
      if (GST_CLOCK_TIME_IS_VALID (state->container_end))
        state->end = MIN (state->end, state->container_end);
      break;
    default:
      gst_ttmlparse_state_set_attribute (state, attr);
      break;
  }
}

/* Read from the state an attribute specified in attr->type and store it in
 * attr->value */
static void
gst_ttmlparse_state_get_attribute (GstTTMLState *state,
    GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      attr->value.node_type = state->node_type;
      break;
    case GST_TTML_ATTR_BEGIN:
      attr->value.time = state->begin;
      break;
    case GST_TTML_ATTR_END:
      attr->value.time = state->end;
      break;
    case GST_TTML_ATTR_DUR:
      attr->value.time = state->end - state->begin;
      break;
    case GST_TTML_ATTR_TICK_PERIOD:
      attr->value.d = state->tick_period;
      break;
    default:
      GST_DEBUG ("Unknown attribute type %d", attr->type);
      return;
  }
}

/* Puts the passed-in attribute into the state, and pushes the previous value
 * into the attribute stack, for later retrieval.
 * The GstTTMLAttribute now belongs to the stack, do not free! */
static void
gst_ttmlparse_state_push_attribute (GstTTMLState *state,
    GstTTMLAttribute *new_attr)
{
  GstTTMLAttribute *old_attr = g_new (GstTTMLAttribute, 1);
  old_attr->type = new_attr->type;
  gst_ttmlparse_state_get_attribute (state, old_attr);
  state->history = g_list_prepend (state->history, old_attr);
  gst_ttmlparse_state_merge_attribute (state, new_attr);
  gst_ttmlparse_attribute_free (new_attr);

  GST_LOG ("Pushed attribute %p (type %d)", old_attr,
      old_attr==NULL?-1:old_attr->type);
}

/* Pops an attribute from the stack and puts in the state, overwritting the
 * current value */
static GstTTMLAttributeType
gst_ttmlparse_state_pop_attribute (GstTTMLState *state)
{
  GstTTMLAttribute *attr;
  GstTTMLAttributeType type;

  if (!state->history) {
    GST_ERROR ("Unable to pop attribute: empty stack");
  }
  attr  = (GstTTMLAttribute *)state->history->data;
  type = attr->type;
  state->history = g_list_delete_link (state->history, state->history);

  GST_LOG ("Popped attribute %p (type %d)", attr, attr==NULL?-1:type);

  gst_ttmlparse_state_set_attribute (state, attr);

  gst_ttmlparse_attribute_free (attr);

  return type;
}

/* Generate one output (text) buffer combining all currently active spans */
static void
gst_ttmlparse_span_compose (GstTTMLSpan *span, GstTTMLSpan *output_span)
{
  output_span->chars =
      g_realloc (output_span->chars, output_span->length + span->length);
  memcpy (output_span->chars + output_span->length, span->chars, span->length);
  output_span->length += span->length;
}

/* Generate and Pad push a buffer, using the correct timestamps and clipping */
static void
gst_ttmlparse_send_buffer (GstTTMLParse * parse, GstClockTime begin,
    GstClockTime end)
{
  GstBuffer *buffer = NULL;
  GstTTMLSpan span = { 0 };
  gboolean in_seg = FALSE;
  gint64 clip_start, clip_stop;

  /* Do not try to push anything if we have not recovered from previous
   * errors yet */
  if (parse->current_gst_status != GST_FLOW_OK)
    return;

  /* Compose output text based on currently active spans */
  g_list_foreach (parse->active_spans, (GFunc)gst_ttmlparse_span_compose,
      &span);

  buffer = gst_buffer_new_and_alloc (span.length);
  memcpy (GST_BUFFER_DATA (buffer), span.chars, span.length);
  gst_buffer_set_caps (buffer, GST_PAD_CAPS (parse->srcpad));

  g_free (span.chars);

  in_seg = gst_segment_clip (parse->segment, GST_FORMAT_TIME,
      parse->base_time + begin, parse->base_time + end,
      &clip_start, &clip_stop);

  if (in_seg) {
    if (G_UNLIKELY (parse->newsegment_needed)) {
      GstEvent *event;

      event = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
          clip_start, -1, 0);
      GST_DEBUG_OBJECT (parse, "Pushing default newsegment");
      gst_pad_push_event (parse->srcpad, event);
      parse->newsegment_needed = FALSE;
    }

    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    GST_DEBUG_OBJECT (parse, "Pushing buffer of %u bytes, pts %"
        GST_TIME_FORMAT " duration %" GST_TIME_FORMAT,
        GST_BUFFER_SIZE (buffer),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
    GST_MEMDUMP_OBJECT (parse, "Content:",
        GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    parse->current_gst_status = gst_pad_push (parse->srcpad, buffer);
  } else {
    GST_DEBUG_OBJECT (parse, "Buffer is out of segment (pts %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (begin));
    gst_buffer_unref (buffer);
  }
}

/* Convert a node type name into a node type enum */
static GstTTMLNodeType
gst_ttmlparse_node_type_parse (const gchar *name)
{
  if (gst_ttmlparse_element_is_type (name, "p")) {
    return GST_TTML_NODE_TYPE_P;
  } else
  if (gst_ttmlparse_element_is_type (name, "span")) {
    return GST_TTML_NODE_TYPE_SPAN;
  }
  return GST_TTML_NODE_TYPE_UNKNOWN;
}

/* Process a node start. Just push all its attributes onto the stack. */
static void
gst_ttmlparse_sax_element_start (void *ctx, const xmlChar *name,
    const xmlChar **xml_attrs)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  const gchar **xml_attr = (const gchar **) xml_attrs;
  GstTTMLAttribute *ttml_attr;
  GstTTMLNodeType node_type;

  GST_LOG_OBJECT (parse, "New element: %s", name);

  node_type = gst_ttmlparse_node_type_parse ((const gchar *)name);
  GST_DEBUG ("Parsed name '%s' into node type %d", name, node_type);

  /* Push onto the stack all attributes defined by this element, starting with
   * the node type, which will serve as delimiter when popping attributes. */
  ttml_attr = gst_ttmlparse_attribute_new_node (node_type);
  gst_ttmlparse_state_push_attribute (&parse->state, ttml_attr);
  while (xml_attr && xml_attr[0]) {
    ttml_attr = gst_ttmlparse_attribute_parse (&parse->state, xml_attr[0],
        xml_attr[1]);
    if (ttml_attr) {
      gst_ttmlparse_state_push_attribute (&parse->state, ttml_attr);
    }

    xml_attr = &xml_attr[2];
  }

  /* Now that all attributes have been parsed, set this time framework as the
   * "container" for nested elements */
  parse->state.container_begin = parse->state.begin;
  parse->state.container_end = parse->state.end;
}

/* Process a node end. Just pop previous state from the stack. */
static void
gst_ttmlparse_sax_element_end (void *ctx, const xmlChar * name)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GstTTMLAttributeType type;

  GST_LOG_OBJECT (parse, "End element: %s", name);

  /* Remove from the attribute stack any attribute pushed by this element */
  do {
    type = gst_ttmlparse_state_pop_attribute (&parse->state);
  } while (type != GST_TTML_ATTR_NODE_TYPE);

  /* Now that all attributes have been parsed, set this time framework as the
   * "container" for nested elements */
  parse->state.container_begin = parse->state.begin;
  parse->state.container_end = parse->state.end;
}

/* Process characters */
static void
gst_ttmlparse_sax_characters (void *ctx, const xmlChar *ch, int len)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  const gchar *content = (const gchar *) ch;
  const gchar *content_end = NULL;
  gint content_size = 0;
  GstTTMLSpan *span;
  GstTTMLEvent *event;
  guint id;

  GST_DEBUG_OBJECT (parse, "Found %d chars inside node type %d",
      len, parse->state.node_type);

  switch (parse->state.node_type) {
    case GST_TTML_NODE_TYPE_P:
    case GST_TTML_NODE_TYPE_SPAN:
      break;
    default:
      /* Ignore characters outside relevant nodes */
      return;
  }

  /* Start by validating UTF-8 content */
  if (!g_utf8_validate (content, len, &content_end)) {
    GST_WARNING_OBJECT (parse, "Content is not valid UTF-8");
    return;
  }
  content_size = content_end - content;

  /* Check if timing information is present */
  if (!GST_CLOCK_TIME_IS_VALID (parse->state.begin) &&
      !GST_CLOCK_TIME_IS_VALID (parse->state.end)) {
    GST_DEBUG_OBJECT (parse, "Span without timing information. Dropping.");
    return;
  }

  /* Create a new span to hold these characters, with an ever-increasing
   * ID number. */
  id = parse->state.last_span_id++;
  span = gst_ttmlparse_span_new (id, content_size, content);

  /* Insert BEGIN and END events in the timeline, with the same ID */
  event = gst_ttmlparse_event_new_span_begin (&parse->state, span);
  parse->timeline =
      gst_ttmlparse_timeline_insert_event (parse->timeline, event);

  event = gst_ttmlparse_event_new_span_end (&parse->state, id);
  parse->timeline =
      gst_ttmlparse_timeline_insert_event (parse->timeline, event);
}

/* Parse SAX warnings (simply shown as debug logs) */
static void
gst_ttmlparse_sax_warning (void *ctx, const char *message, ...)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_WARNING, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (parse), message, va);
  va_end (va);
}

/* Parse SAX errors (simply shown as debug logs) */
static void
gst_ttmlparse_sax_error (void *ctx, const char *message, ...)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_ERROR, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (parse), message, va);
  va_end (va);
}

/* Parse comments from XML (simply shown as debug logs) */
static void
gst_ttmlparse_sax_comment (void *ctx, const xmlChar * comment)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GST_LOG_OBJECT (parse, "Comment parsed: %s", comment);
}

/* Default handler for entities (&amp; and company) */
static xmlEntityPtr
gst_ttmlparse_sax_get_entity (void *ctx, const xmlChar * name)
{
  return xmlGetPredefinedEntity (name);
}

static void
gst_ttmlparse_sax_document_end (void *ctx)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GstTTMLEvent *event = NULL;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  GST_LOG_OBJECT (parse, "Document complete");
  if (parse->assume_ordered_spans) {
    /* Nothing to do here */
    return;
  }

  if (!parse->timeline) {
    /* Empty timeline, nothing to do */
    return;
  }

  /* Get all events from the timeline, parse them and generate output
   * buffers */
  do {
    parse->timeline =
      gst_ttmlparse_timeline_get_next_event (parse->timeline, &event);

    if (event->timestamp != time && GST_CLOCK_TIME_IS_VALID (time)) {
      gst_ttmlparse_send_buffer (parse, time, event->timestamp);
    }
    time = event->timestamp;
    gst_ttmlparse_event_parse (parse, event);
  } while (parse->timeline);
}

static xmlSAXHandler gst_ttmlparse_sax_handler = {
  /* .internalSubset = */ NULL,
  /* .isStandalone = */ NULL,
  /*. hasInternalSubset = */ NULL,
  /*. hasExternalSubset = */ NULL,
  /*. resolveEntity = */ NULL,
  /*. getEntity = */ gst_ttmlparse_sax_get_entity,
  /*. entityDecl = */ NULL,
  /*. notationDecl = */ NULL,
  /*. attributeDecl = */ NULL,
  /*. elementDecl = */ NULL,
  /*. unparsedEntityDecl = */ NULL,
  /*. setDocumentLocator = */ NULL,
  /*. startDocument = */ NULL,
  /*. endDocument = */ gst_ttmlparse_sax_document_end,
  /*. startElement = */ gst_ttmlparse_sax_element_start,
  /*. endElement = */ gst_ttmlparse_sax_element_end,
  /*. reference = */ NULL,
  /*. characters = */ gst_ttmlparse_sax_characters,
  /*. ignorableWhitespace = */ NULL,
  /*. processingInstruction = */ NULL,
  /*. comment = */ gst_ttmlparse_sax_comment,
  /*. warning = */ gst_ttmlparse_sax_warning,
  /*. error = */ gst_ttmlparse_sax_error,
  /*. fatalError = */ gst_ttmlparse_sax_error,
};

static GstFlowReturn
gst_ttmlparse_chain (GstPad * pad, GstBuffer * buffer)
{
  GstTTMLParse *parse;
  const char *buffer_data;
  int buffer_len;

  parse = GST_TTMLPARSE (gst_pad_get_parent (pad));
  parse->current_gst_status = GST_FLOW_OK;

  /* Set caps on src pad */
  if (G_UNLIKELY (!GST_PAD_CAPS (parse->srcpad))) {
    GstCaps *caps = gst_caps_new_simple ("text/plain", NULL);

    GST_DEBUG_OBJECT (parse->srcpad, "setting caps %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps (parse->srcpad, caps);

    gst_caps_unref (caps);
  }

  GST_LOG_OBJECT (parse, "Handling buffer of %u bytes pts %" GST_TIME_FORMAT,
      GST_BUFFER_SIZE (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  buffer_data = (const char *) GST_BUFFER_DATA (buffer);
  buffer_len = GST_BUFFER_SIZE (buffer);
  do {
    const char *next_buffer_data = NULL;
    int next_buffer_len = 0;

    /* Store buffer timestamp. All future timestamps we produce will be relative
     * to this buffer time. */
    if (!GST_CLOCK_TIME_IS_VALID (parse->base_time) &&
        GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {
      parse->base_time = GST_BUFFER_TIMESTAMP (buffer);
    }

    /* Look for end-of-document tags */
    next_buffer_data = g_strstr_len (buffer_data, buffer_len, "</tt>");

    /* If one was detected, this might be a concatenated XML file (multiple
     * XML files inside the same buffer) and we need to parse them one by one
     */
    if (next_buffer_data) {
      next_buffer_data += 5;
      GST_DEBUG_OBJECT (parse, "Detected document end at position %d of %d",
          next_buffer_data - buffer_data, buffer_len);
      next_buffer_len = buffer_len - (next_buffer_data - buffer_data);
      buffer_len = next_buffer_data - buffer_data;
    }

    /* Feed this data to the SAX parser. The rest of the processing takes place
     * in the callbacks. */
    if (!parse->xml_parser) {
      GST_DEBUG_OBJECT (parse, "Creating parser and parsing chunk (%d bytes)",
          buffer_len);
      parse->xml_parser = xmlCreatePushParserCtxt (&gst_ttmlparse_sax_handler,
          parse, buffer_data, buffer_len, NULL);
      if (!parse->xml_parser) {
        GST_ERROR_OBJECT (parse, "xml parser creation failed");
        goto beach;
      } else {
        GST_DEBUG_OBJECT (parse, "Chunk finished");
      }
    } else {
      int res;
      GST_DEBUG_OBJECT (parse, "Parsing chunk (%d bytes)", buffer_len);
      res = xmlParseChunk (parse->xml_parser, buffer_data, buffer_len, 0);
      if (res != 0) {
        GST_WARNING_OBJECT (parse, "Parsing failed");
      } else {
        GST_DEBUG_OBJECT (parse, "Chunk finished");
      }
    }

    /* If an end-of-document tag was found, terminate this parsing process */
    if (next_buffer_data) {
      /* Destroy parser, a new one will be created if more XML files arrive */
      GST_DEBUG_OBJECT (parse, "Terminating pending parsing works");
      xmlParseChunk (parse->xml_parser, NULL, 0, 1);
      GST_DEBUG_OBJECT (parse, "Destroying parser");
      xmlFreeParserCtxt (parse->xml_parser);
      parse->xml_parser = NULL;
      parse->base_time = GST_CLOCK_TIME_NONE;

      /* Remove trailing whitespace, or the first thing the new parser will
       * find will not be the start-of-document tag */
      while (next_buffer_len && g_ascii_isspace (*next_buffer_data)) {
        GST_DEBUG_OBJECT (parse, "Skipping trailing whitespace char 0x%02x",
            *next_buffer_data);
        next_buffer_data++;
        next_buffer_len--;
      }
    }

    /* Process the next XML inside this buffer. If the end-of-document tag was
     * at the end of the buffer (single XML inside single buffer case), then
     * buffer_len will be 0 after this adjustment and no more loops will be
     * performed. */
    buffer_data = next_buffer_data;
    buffer_len = next_buffer_len;

  } while (buffer_len);

beach:
  gst_buffer_unref (buffer);

  gst_object_unref (parse);

  return parse->current_gst_status;
}

static gboolean
gst_ttmlparse_sink_event (GstPad * pad, GstEvent * event)
{
  GstTTMLParse *parse;
  gboolean ret = TRUE;

  parse = GST_TTMLPARSE (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (parse, "handling event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gdouble rate;
      gint64 start, stop, time;
      gboolean update;

      GST_DEBUG_OBJECT (parse, "received newsegment");

      gst_event_parse_new_segment (event, &update, &rate, &format, &start,
          &stop, &time);
      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (parse,
            "dropping it because it is not in TIME format");
        goto beach;
      }

      GST_DEBUG_OBJECT (parse, "received new segment update %d, rate %f, "
          "start %" GST_TIME_FORMAT ", stop %" GST_TIME_FORMAT, update, rate,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

      GST_DEBUG_OBJECT (parse, "our segment was %" GST_SEGMENT_FORMAT,
          parse->segment);

      if (format == GST_FORMAT_TIME) {
        gst_segment_set_newsegment (parse->segment, update, rate, format, start,
            stop, time);

        GST_DEBUG_OBJECT (parse, "our segment now is %" GST_SEGMENT_FORMAT,
            parse->segment);
      }

      parse->newsegment_needed = FALSE;
      ret = gst_pad_event_default (pad, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

beach:
  gst_object_unref (parse);

  return ret;
}

/* Set the state to default values */
static void
gst_ttmlparse_state_reset (GstTTMLState *state)
{
  state->last_span_id = 0;
  state->begin = GST_CLOCK_TIME_NONE;
  state->end = GST_CLOCK_TIME_NONE;
  state->container_begin = GST_CLOCK_TIME_NONE;
  state->container_end = GST_CLOCK_TIME_NONE;
  state->tick_period = GST_SECOND;
  if (state->history) {
    GST_WARNING ("Attribute stack should have been empty");
    g_list_free_full (state->history,
        (GDestroyNotify)gst_ttmlparse_attribute_free);
    state->history = NULL;
  }
}

/* Free any information held by the element */
static void
gst_ttmlparse_cleanup (GstTTMLParse * parse)
{
  GST_DEBUG_OBJECT (parse, "cleaning up TTML parser");

  if (parse->segment) {
    gst_segment_init (parse->segment, GST_FORMAT_TIME);
  }

  if (parse->xml_parser) {
    xmlFreeParserCtxt (parse->xml_parser);
    parse->xml_parser = NULL;
  }

  if (parse->timeline) {
    g_list_free_full (parse->timeline,
        (GDestroyNotify)gst_ttmlparse_event_free);
  }

  gst_ttmlparse_state_reset (&parse->state);

  return;
}

static GstStateChangeReturn
gst_ttmlparse_change_state (GstElement * element, GstStateChange transition)
{
  GstTTMLParse *parse;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS, bret;

  parse = GST_TTMLPARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  bret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (bret == GST_STATE_CHANGE_FAILURE)
    return bret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (parse, "going from PAUSED to READY");
      gst_ttmlparse_cleanup (parse);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}
static void
gst_ttmlparse_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstTTMLParse *parse = GST_TTMLPARSE (object);

  switch (prop_id) {
    case PROP_ASSUME_ORDERED_SPANS:
      g_value_set_boolean (value, parse->assume_ordered_spans);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlparse_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTTMLParse *parse = GST_TTMLPARSE (object);

  switch (prop_id) {
    case PROP_ASSUME_ORDERED_SPANS:
      parse->assume_ordered_spans = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlparse_dispose (GObject * object)
{
  GstTTMLParse *parse = GST_TTMLPARSE (object);

  GST_DEBUG_OBJECT (parse, "disposing TTML parser");

  if (parse->segment) {
    gst_segment_free (parse->segment);
    parse->segment = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_ttmlparse_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&ttmlparse_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&ttmlparse_src_template));

  gst_element_class_set_details (element_class, &ttmlparse_details);
}

static void
gst_ttmlparse_class_init (GstTTMLParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ttmlparse_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_ttmlparse_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_ttmlparse_get_property);

  /* Register properties */
  g_object_class_install_property (gobject_class, PROP_ASSUME_ORDERED_SPANS,
      g_param_spec_boolean ("assume_ordered_spans", "Assume ordered spans",
          "Generate buffers as soon as possible, by assuming that text "
          "spans will arrive in order", 
          FALSE, G_PARAM_READWRITE));

  /* GstElement overrides */
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ttmlparse_change_state);
}

static void
gst_ttmlparse_init (GstTTMLParse * parse, GstTTMLParseClass * g_class)
{
  parse->sinkpad = gst_pad_new_from_static_template (&ttmlparse_sink_template,
      "sink");

  gst_pad_set_event_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttmlparse_sink_event));
  gst_pad_set_chain_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttmlparse_chain));

  parse->srcpad = gst_pad_new_from_static_template (&ttmlparse_src_template,
      "src");

  gst_pad_use_fixed_caps (parse->srcpad);

  gst_element_add_pad (GST_ELEMENT (parse), parse->sinkpad);
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);

  parse->segment = gst_segment_new ();
  parse->newsegment_needed = TRUE;

  parse->xml_parser = NULL;
  parse->base_time = GST_CLOCK_TIME_NONE;
  parse->current_gst_status = GST_FLOW_OK;
  parse->timeline = NULL;

  parse->state.history = NULL;
  gst_ttmlparse_state_reset (&parse->state);

  gst_ttmlparse_cleanup (parse);
}

GType
gst_ttmlparse_get_type ()
{
  static GType ttmlparse_type = 0;

  if (!ttmlparse_type) {
    static const GTypeInfo ttmlparse_info = {
      sizeof (GstTTMLParseClass),
      (GBaseInitFunc) gst_ttmlparse_base_init,
      NULL,
      (GClassInitFunc) gst_ttmlparse_class_init,
      NULL,
      NULL,
      sizeof (GstTTMLParse),
      0,
      (GInstanceInitFunc) gst_ttmlparse_init,
    };

    ttmlparse_type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstTTMLParse", &ttmlparse_info, 0);
  }
  return ttmlparse_type;
}
