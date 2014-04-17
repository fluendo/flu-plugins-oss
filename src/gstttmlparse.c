/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <libxml/parser.h>
#include <gst/gstconfig.h>

#include "gstttmlparse.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

enum
{
  PROP_0,
  PROP_ASSUME_ORDERED_SPANS,
  PROP_FORCE_BUFFER_CLEAR
};

#if GST_CHECK_VERSION (1,0,0)
#define GST_TTMLPARSE_SRC_CAPS "text/x-raw,format=pango-markup"
#else
#define GST_TTMLPARSE_SRC_CAPS "text/x-pango-markup"
#endif

static GstStaticPadTemplate ttmlparse_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_TTMLPARSE_SRC_CAPS)
    );

static GstStaticPadTemplate ttmlparse_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_MIME));

G_DEFINE_TYPE (GstTTMLParse, gst_ttmlparse, GST_TYPE_ELEMENT);
#define parent_class gst_ttmlparse_parent_class


#if GST_CHECK_VERSION (1,0,0)

/* Compatibility functions for segment handling */
GstEvent* gst_event_new_new_segment (gboolean update, gdouble rate,
    GstFormat format, gint64 start, gint64 stop, gint64 position)
{
  GstEvent *event;
  GstSegment seg;
  gst_segment_init (&seg, format);
  seg.start = start;
  seg.stop = stop;
  seg.position = position;
  event = gst_event_new_segment (&seg);
  return event;
}

void gst_event_parse_new_segment (GstEvent *event, gboolean *update,
    gdouble *rate, GstFormat *format, gint64 *start, gint64 *stop,
    gint64 *position)
{
  const GstSegment *seg;
  gst_event_parse_segment (event, &seg);
  *update = FALSE;
  *rate = seg->rate;
  *format = seg->format;
  *start = seg->start;
  *stop = seg->stop;
  *position = seg->position;
}

void gst_segment_set_newsegment (GstSegment *segment, gboolean update,
    gdouble rate, GstFormat format, gint64 start, gint64 stop, gint64 time)
{
  segment->rate = rate;
  segment->format = format;
  segment->start = start;
  segment->stop = stop;
  segment->position = time;
}

#endif

/* Generate and Pad push a buffer, using the correct timestamps and clipping */
static void
gst_ttmlparse_gen_buffer (GstClockTime begin, GstClockTime end,
    GstTTMLParse * parse)
{
  GstBuffer *buffer = NULL;
  GstTTMLSpan span = { 0 };
  gboolean in_seg = FALSE;
  GstMapInfo map_info;
#if GST_CHECK_VERSION (1,0,0)
  guint64 clip_start = 0, clip_stop = 0;
#else
  gint64 clip_start = 0, clip_stop = 0;
#endif

  /* Do not try to push anything if we have not recovered from previous
   * errors yet */
  if (parse->current_gst_status != GST_FLOW_OK)
    return;

  /* Check if there is any active span at all */
  if (!parse->active_spans) {
    if (!parse->force_buffer_clear) return;

    /* Generate an artifical empty buffer to clean the text renderer */
    buffer = gst_buffer_new_and_alloc (1);
    gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
    map_info.data[0] = ' ';
    gst_buffer_unmap (buffer, &map_info);
  } else {
    /* Compose output text based on currently active spans */
    g_list_foreach (parse->active_spans, (GFunc)gst_ttml_span_compose, &span);

    if (span.length == 0) {
      /* Empty buffers are useless and Pango complains about them */
      g_free (span.chars);
      return;
    }

    if (span.length == 1 && span.chars[0] == '\n') {
      /* Pango does not like buffers made entirely of invisible chars.
       * This requires a more robust fix... */
      span.chars[0] = ' ';
    }

    buffer = gst_buffer_new_and_alloc (span.length);
    gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
    memcpy (map_info.data, span.chars, span.length);
    gst_buffer_unmap (buffer, &map_info);
    g_free (span.chars);
  }

  in_seg = gst_segment_clip (parse->segment, GST_FORMAT_TIME,
      parse->base_time + begin, parse->base_time + end,
      &clip_start, &clip_stop);

  if (in_seg) {
    if (G_UNLIKELY (parse->newsegment_needed)) {
      GstEvent *event;

      event = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
          parse->base_time, -1, 0);
      GST_DEBUG_OBJECT (parse, "Pushing default newsegment");
      gst_pad_push_event (parse->srcpad, event);
      parse->newsegment_needed = FALSE;
    }

#if !GST_CHECK_VERSION (1,0,0)
    /* Set caps on buffer */
    gst_buffer_set_caps (buffer, GST_PAD_CAPS (parse->srcpad));
#endif

    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    GST_DEBUG_OBJECT (parse, "Pushing buffer of %u bytes, pts %"
        GST_TIME_FORMAT " duration %" GST_TIME_FORMAT,
        (guint)gst_buffer_get_size (buffer),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
    GST_TTML_UTILS_MEMDUMP_BUFFER_OBJECT (parse, "Content:", buffer);

    parse->current_gst_status = gstflu_demo_push_buffer (&parse->stats,
        parse->sinkpad, parse->srcpad, buffer);
  } else {
    GST_DEBUG_OBJECT (parse, "Buffer is out of segment (pts %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (begin));
    gst_buffer_unref (buffer);
  }
}

/* Execute the given event */
void
gst_ttmlparse_parse_event (GstTTMLEvent *event, GstTTMLParse *parse)
{
  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      parse->active_spans =
          gst_ttml_span_list_add (parse->active_spans,
              event->data.span_begin.span);
      /* Remove the span from the event, so that when we free the event below
       * the span does not get freed too (it belongs to the active_spans list
       * now) */
      event->data.span_begin.span = NULL;
      break;
    case GST_TTML_EVENT_TYPE_SPAN_END:
      parse->active_spans =
          gst_ttml_span_list_remove (parse->active_spans,
              event->data.span_end.id);
      break;
    case GST_TTML_EVENT_TYPE_ATTR_UPDATE:
      gst_ttml_span_list_update_attr (parse->active_spans,
          event->data.attr_update.id, event->data.attr_update.attr);
      break;
    default:
      GST_WARNING ("Unknown event type");
      break;
  }
  gst_ttml_event_free (event);
}

/* Allocate a new span to hold new characters, and insert into the timeline
 * BEGIN and END events to handle this new span. */
static void
gst_ttmlparse_add_characters (GstTTMLParse *parse, const gchar *content,
    int len, gboolean preserve_cr)
{
  const gchar *content_end = NULL;
  gint content_size = 0;
  GstTTMLSpan *span;
  GstTTMLEvent *event;
  guint id;

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

  if (parse->state.node_type == GST_TTML_NODE_TYPE_P &&
      parse->state.sequential_time_container) {
    /* Anonymous spans have 0 duration when inside sequential containers */
    return;
  }

  if (GST_CLOCK_TIME_IS_VALID (parse->state.begin) &&
      parse->state.begin >= parse->state.end) {
    GST_DEBUG ("Span with 0 duration. Dropping. (begin=%" GST_TIME_FORMAT
        ", end=%" GST_TIME_FORMAT ")", GST_TIME_ARGS (parse->state.begin),
        GST_TIME_ARGS (parse->state.end));
    return;
  }

  /* If assuming ordered spans, as soon as our begin is later than the
   * latest event in the timeline, we can flush the timeline */
  if (parse->assume_ordered_spans &&
      parse->state.begin >= parse->last_event_timestamp) {
    parse->timeline = gst_ttml_event_list_flush (parse->timeline,
        (GstTTMLEventParseFunc)gst_ttmlparse_parse_event,
        (GstTTMLEventGenBufferFunc)gst_ttmlparse_gen_buffer,
        parse);
  }

  /* Create a new span to hold these characters, with an ever-increasing
   * ID number. */
  id = parse->state.last_span_id++;
  span = gst_ttml_span_new (id, content_size, content, &parse->state.style,
      preserve_cr);
  if (!span) {
    GST_DEBUG ("Empty span. Dropping.");
    return;
  }

  /* Insert BEGIN and END events in the timeline, with the same ID */
  event = gst_ttml_event_new_span_begin (&parse->state, span);
  parse->timeline = gst_ttml_event_list_insert (parse->timeline, event);

  event = gst_ttml_event_new_span_end (&parse->state, id);
  parse->timeline = gst_ttml_event_list_insert (parse->timeline, event);

  parse->timeline =
      gst_ttml_style_gen_span_events (id, &parse->state.style,
          parse->timeline);

  parse->last_event_timestamp = event->timestamp;
}

/* Helper method to turn SAX2's gchar * attribute array into a GstTTMLAttribute
 * and push it into the stack */
static void
gst_ttmlparse_push_attr (GstTTMLParse *parse, const gchar **xml_attr,
    gboolean *dur_attr_found)
{
  /* Create a local copy of the attr value, since SAX2 does not
   * NULL-terminate the string */
  gsize value_len = xml_attr[4] - xml_attr[3];
  gchar *value = (gchar *)alloca(value_len + 1);
  GstTTMLAttribute *ttml_attr;
  memcpy (value, xml_attr[3], value_len);
  value[value_len] = '\0';
  ttml_attr = gst_ttml_attribute_parse (&parse->state,
      !xml_attr[1]?NULL:xml_attr[2], xml_attr[0],
      value);
  if (ttml_attr) {
    if (ttml_attr->type == GST_TTML_ATTR_DUR)
      *dur_attr_found = TRUE;
    gst_ttml_state_push_attribute (&parse->state, ttml_attr);
  }
}

/* Process a node start. Just push all its attributes onto the stack. */
static void
gst_ttmlparse_sax2_element_start_ns (void *ctx, const xmlChar *name,
    const xmlChar *prefix, const xmlChar *URI, int nb_namespaces,
    const xmlChar **namespaces, int nb_attributes, int nb_defaulted,
    const xmlChar **xml_attrs)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  const gchar **xml_attr = (const gchar **) xml_attrs;
  GstTTMLAttribute *ttml_attr;
  GstTTMLNodeType node_type;
  gboolean is_container_seq = parse->state.sequential_time_container;
  gboolean dur_attr_found = FALSE;
  int i = nb_attributes;

  GST_LOG_OBJECT (parse, "New element: %s prefix:%s URI:%s", name,
    prefix?(char *)prefix:"NULL", URI?(char *)URI:"NULL");

  node_type = gst_ttml_utils_node_type_parse (!prefix?NULL:(const gchar *)URI, (const gchar *)name);
  GST_DEBUG ("Parsed name '%s' into node type %s",
      name, gst_ttml_utils_node_type_name (node_type));

  /* Special actions for some node types */
  switch (node_type) {
    case GST_TTML_NODE_TYPE_STYLING:
      parse->in_styling_node = TRUE;
      break;
    default:
      break;
  }

  /* Push onto the stack the node type, which will serve as delimiter when
   * popping attributes. */
  ttml_attr = gst_ttml_attribute_new_node (node_type);
  gst_ttml_state_push_attribute (&parse->state, ttml_attr);
  /* If this node did not specify the time_container attribute, set it
   * manually to "parallel", as this is not inherited. */
  ttml_attr =
      gst_ttml_attribute_new_boolean (GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER,
      FALSE);
  gst_ttml_state_push_attribute (&parse->state, ttml_attr);
  /* Manually push a 0 BEGIN attribute when in sequential mode.
   * If the node defines it, its value will overwrite this one.
   * This seemed the simplest way to take container_begin into account when
   * the node does not define a BEGIN time, since it is taken into account in
   * the _merge_attribute method. */
  if (is_container_seq) {
    ttml_attr = gst_ttml_attribute_new_time (GST_TTML_ATTR_BEGIN, 0);
    gst_ttml_state_push_attribute (&parse->state, ttml_attr);
  }
  /* Push onto the stack the "style" attribute, if found.
   * It goes first, because the attributes defined by this style must be
   * overriden by the values defined in this node, regardless of their
   * parsing order. */
  while (i--) {
    if (strcmp (xml_attr[0], "style") == 0) {
      gst_ttmlparse_push_attr (parse, xml_attr, &dur_attr_found);
    }
    xml_attr = &xml_attr[5];
  }
  /* Push onto the stack the rest of the attributes defined by this element */
  xml_attr = (const gchar **) xml_attrs;
  i = nb_attributes;
  while (i--) {
    if (strcmp (xml_attr[0], "style") != 0) {
      gst_ttmlparse_push_attr (parse, xml_attr, &dur_attr_found);
    }
    xml_attr = &xml_attr[5];
  }


  /* Manually push a 0 DUR attribute if the node did not define it in
   * sequential mode. In this case this node must be ignored and this seemed
   * like the simplest way. */
  if (is_container_seq && !dur_attr_found) {
    ttml_attr = gst_ttml_attribute_new_time (GST_TTML_ATTR_DUR, 0);
    gst_ttml_state_push_attribute (&parse->state, ttml_attr);
  }

  /* Now that all attributes have been parsed, set this time framework as the
   * "container" for nested elements */
  parse->state.container_begin = parse->state.begin;
  parse->state.container_end = parse->state.end;

  /* Handle special node types which have effect as soon as they are found */
  if (node_type == GST_TTML_NODE_TYPE_BR) {
    gchar br = '\n';
    gst_ttmlparse_add_characters (parse, &br, 1, TRUE);
  }
}

/* Process a node end. Just pop previous state from the stack. */
static void
gst_ttmlparse_sax2_element_end_ns (void *ctx, const xmlChar *name,
    const xmlChar *prefix, const xmlChar *URI)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GstTTMLAttribute *prev_attr;
  GstTTMLAttributeType type;
  GstClockTime current_begin = parse->state.begin;
  GstClockTime current_end = parse->state.end;
  GstTTMLNodeType current_node_type;

  GST_LOG_OBJECT (parse, "End element: %s", name);
  current_node_type = gst_ttml_utils_node_type_parse (
      !prefix?NULL:(const gchar *)URI, (const gchar *)name);

  /* Special actions for some node types */
  switch (current_node_type) {
    case GST_TTML_NODE_TYPE_STYLING:
      if (!parse->in_styling_node) {
        GST_WARNING_OBJECT (parse, "Unmatched closing styling node");
      }
      parse->in_styling_node = FALSE;
      break;
    case GST_TTML_NODE_TYPE_STYLE:
      /* We are closing a style definition. Store the current style IF
       * we are inside a <styling> node. */
      if (parse->in_styling_node)
        gst_ttml_state_save_attr_stack (&parse->state, parse->state.id);
      break;
    default:
      break;
  }

  /* Remove from the attribute stack any attribute pushed by this element */
  do {
    type = gst_ttml_state_pop_attribute (&parse->state, &prev_attr);
    if (current_node_type == GST_TTML_NODE_TYPE_SET &&
        type > GST_TTML_ATTR_STYLE) {
      /* We are popping a styling attribute from a SET node: turn it into a
       * couple of entries in that attribute's timeline in the parent style */
      GstTTMLAttribute *attr;
      attr = gst_ttml_style_get_attr (&parse->state.style, type);
      gst_ttml_attribute_add_event (attr, current_begin, prev_attr);
      gst_ttml_attribute_add_event (attr, current_end - 1, attr);
    }
    if (prev_attr)
      gst_ttml_attribute_free (prev_attr);
  } while (type != GST_TTML_ATTR_NODE_TYPE);

  /* Now that we are back to our parent's context, set this time framework as
   * the "container" for nested elements.
   * Move forward the container_begin if our parent was a sequential time
   * container. */
  if (parse->state.sequential_time_container) {
    GST_DEBUG ("Getting back to a seq container. Setting container_begin to %"
        GST_TIME_FORMAT, GST_TIME_ARGS (current_end));
    parse->state.container_begin = current_end;
  } else {
    GST_DEBUG ("Getting back to a par container. Setting container_begin to %"
        GST_TIME_FORMAT, GST_TIME_ARGS (parse->state.begin));
    parse->state.container_begin = parse->state.begin;
  }
  parse->state.container_end = parse->state.end;
}

/* Process characters */
static void
gst_ttmlparse_sax_characters (void *ctx, const xmlChar *ch, int len)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  const gchar *content = (const gchar *) ch;

  GST_DEBUG_OBJECT (parse, "Found %d chars inside node type %s",
      len, gst_ttml_utils_node_type_name (parse->state.node_type));
  GST_MEMDUMP ("Content:", (guint8 *)ch, len);

  switch (parse->state.node_type) {
    case GST_TTML_NODE_TYPE_P:
    case GST_TTML_NODE_TYPE_SPAN:
      break;
    default:
      /* Ignore characters outside relevant nodes */
      return;
  }

  gst_ttmlparse_add_characters (parse, content, len,
      parse->state.whitespace_preserve);
}

/* Parse SAX warnings (simply shown as debug logs) */
static void
gst_ttmlparse_sax_warning (void *ctx, const char *message, ...)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_WARNING, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (parse), message, va);
  va_end (va);
#endif /* GST_DISABLE_GST_DEBUG */
}

/* Parse SAX errors (simply shown as debug logs) */
static void
gst_ttmlparse_sax_error (void *ctx, const char *message, ...)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_ERROR, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (parse), message, va);
  va_end (va);
#endif /* GST_DISABLE_GST_DEBUG */
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
gst_ttmlparse_sax_document_start (void *ctx)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GST_LOG_OBJECT (GST_TTMLPARSE (ctx), "Document start");

  parse->in_styling_node = FALSE;
  gst_ttml_state_reset (&parse->state);
}

static void
gst_ttmlparse_sax_document_end (void *ctx)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GST_LOG_OBJECT (GST_TTMLPARSE (ctx), "Document complete");

  parse->timeline = gst_ttml_event_list_flush (parse->timeline,
      (GstTTMLEventParseFunc)gst_ttmlparse_parse_event,
      (GstTTMLEventGenBufferFunc)gst_ttmlparse_gen_buffer, parse);
}

static xmlSAXHandler gst_ttmlparse_sax_handler = {
  /* .internalSubset = */ NULL,
  /* .isStandalone = */ NULL,
  /* .hasInternalSubset = */ NULL,
  /* .hasExternalSubset = */ NULL,
  /* .resolveEntity = */ NULL,
  /* .getEntity = */ gst_ttmlparse_sax_get_entity,
  /* .entityDecl = */ NULL,
  /* .notationDecl = */ NULL,
  /* .attributeDecl = */ NULL,
  /* .elementDecl = */ NULL,
  /* .unparsedEntityDecl = */ NULL,
  /* .setDocumentLocator = */ NULL,
  /* .startDocument = */ gst_ttmlparse_sax_document_start,
  /* .endDocument = */ gst_ttmlparse_sax_document_end,
  /* .startElement = */ NULL,
  /* .endElement = */ NULL,
  /* .reference = */ NULL,
  /* .characters = */ gst_ttmlparse_sax_characters,
  /* .ignorableWhitespace = */ NULL,
  /* .processingInstruction = */ NULL,
  /* .comment = */ gst_ttmlparse_sax_comment,
  /* .warning = */ gst_ttmlparse_sax_warning,
  /* .error = */ gst_ttmlparse_sax_error,
  /* .fatalError = */ gst_ttmlparse_sax_error,
  /* .getParameterEntity = */ NULL,
  /* .cdataBlock = */ NULL,
  /* .externalSubset = */ NULL,
  /* .initialized = */ XML_SAX2_MAGIC,
  /* ._private = */ NULL,
  /* .startElementNs = */ gst_ttmlparse_sax2_element_start_ns,
  /* .endElementNs = */ gst_ttmlparse_sax2_element_end_ns,
  /* .xmlStructuredError = */ NULL
};

/* Free any parsing-related information held by the element */
static void
gst_ttmlparse_reset (GstTTMLParse * parse)
{
  GST_DEBUG_OBJECT (parse, "Resetting parsing information");

  if (parse->xml_parser) {
    xmlFreeParserCtxt (parse->xml_parser);
    parse->xml_parser = NULL;
  }

  if (parse->timeline) {
    g_list_free_full (parse->timeline,
        (GDestroyNotify)gst_ttml_event_free);
    parse->timeline = NULL;
  }
  parse->last_event_timestamp = GST_CLOCK_TIME_NONE;

  if (parse->active_spans) {
    g_list_free_full (parse->active_spans,
        (GDestroyNotify)gst_ttml_span_free);
    parse->active_spans = NULL;
  }

  gst_ttml_state_reset (&parse->state);
}

static GstFlowReturn
gst_ttmlparse_handle_buffer (GstPad * pad, GstBuffer * buffer)
{
  GstTTMLParse *parse;
  const char *buffer_data;
  int buffer_len;
  GstMapInfo map;
  GstCaps *src_caps;

  parse = GST_TTMLPARSE (gst_pad_get_parent (pad));
  parse->current_gst_status = GST_FLOW_OK;

  /* Set caps on src pad */
#if GST_CHECK_VERSION (1,0,0)
  src_caps = gst_pad_get_current_caps (parse->srcpad);
#else
  src_caps = gst_pad_get_negotiated_caps (parse->srcpad);
#endif
  if (G_UNLIKELY (!src_caps)) {
    /* Last chance to set the src pad's caps. It can happen (when linking with
     * a filesrc, for example) that the 0.10 set_caps function is never called.
     */
    src_caps = gst_caps_from_string (GST_TTMLPARSE_SRC_CAPS);
    GST_DEBUG_OBJECT (parse->srcpad, "setting caps %" GST_PTR_FORMAT, src_caps);
    gst_pad_set_caps (parse->srcpad, src_caps);
  }
  gst_caps_unref (src_caps);

  GST_LOG_OBJECT (parse, "Handling buffer of %u bytes pts %" GST_TIME_FORMAT,
      (guint)gst_buffer_get_size (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  buffer_data = (const char *) map.data;
  buffer_len = map.size;
  do {
    const char *next_buffer_data = NULL;
    int next_buffer_len = 0;

    /* Store buffer timestamp. All future timestamps we produce will be relative
     * to this buffer time. */
    if (!GST_CLOCK_TIME_IS_VALID (parse->base_time)) {
      if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {
        parse->base_time = GST_BUFFER_TIMESTAMP (buffer);
      } else {
        /* If we never received a valid timestamp, ok, assume 0 */
        parse->base_time = 0;
      }
    }

    /* Look for end-of-document tags */
    next_buffer_data = g_strstr_len (buffer_data, buffer_len, "</tt>");

    /* If one was detected, this might be a concatenated XML file (multiple
     * XML files inside the same buffer) and we need to parse them one by one
     */
    if (next_buffer_data) {
      next_buffer_data += 5;
      GST_DEBUG_OBJECT (parse, "Detected XML document end at position %d of %d",
          (int)(next_buffer_data - buffer_data), buffer_len);
      next_buffer_len = buffer_len - (next_buffer_data - buffer_data);
      buffer_len = next_buffer_data - buffer_data;
    }

    /* Feed this data to the SAX parser. The rest of the processing takes place
     * in the callbacks. */
    if (!parse->xml_parser) {
      GST_DEBUG_OBJECT (parse,
          "Creating XML parser and parsing chunk (%d bytes)", buffer_len);
      parse->xml_parser =
          xmlCreatePushParserCtxt (&gst_ttmlparse_sax_handler, parse,
          buffer_data, buffer_len, NULL);
      if (!parse->xml_parser) {
        GST_ERROR_OBJECT (parse, "XML parser creation failed");
        goto beach;
      } else {
        GST_DEBUG_OBJECT (parse, "XML Chunk finished");
      }
    } else {
      int res;
      GST_DEBUG_OBJECT (parse, "Parsing XML chunk (%d bytes)", buffer_len);
      res = xmlParseChunk (parse->xml_parser, buffer_data, buffer_len, 0);
      if (res != 0) {
        GST_WARNING_OBJECT (parse, "XML Parsing failed");
      } else {
        GST_DEBUG_OBJECT (parse, "XML Chunk finished");
      }
    }

    /* If an end-of-document tag was found, terminate this parsing process */
    if (next_buffer_data) {
      /* Destroy parser, a new one will be created if more XML files arrive */
      GST_DEBUG_OBJECT (parse, "Terminating pending XML parsing works");
      xmlParseChunk (parse->xml_parser, NULL, 0, 1);

      gst_ttmlparse_reset (parse);
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
  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);

  gst_object_unref (parse);

  return parse->current_gst_status;
}

/* Free any information held by the element */
static void
gst_ttmlparse_cleanup (GstTTMLParse * parse)
{
  GST_DEBUG_OBJECT (parse, "cleaning up TTML parser");

  if (parse->segment) {
    gst_segment_init (parse->segment, GST_FORMAT_TIME);
  }
  parse->newsegment_needed = TRUE;
  parse->current_gst_status = GST_FLOW_OK;

  gst_ttmlparse_reset (parse);
}

static gboolean
gst_ttmlparse_handle_event (GstPad * pad, GstEvent * event)
{
  GstTTMLParse *parse;
  gboolean ret = TRUE;

  parse = GST_TTMLPARSE (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (parse, "handling event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
#if GST_CHECK_VERSION (1,0,0)
    case GST_EVENT_SEGMENT:
#else
    case GST_EVENT_NEWSEGMENT:
#endif
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

      gst_segment_set_newsegment (parse->segment, update, rate, format, start,
          stop, time);

      GST_DEBUG_OBJECT (parse, "our segment now is %" GST_SEGMENT_FORMAT,
          parse->segment);

      parse->newsegment_needed = FALSE;
      ret = gst_pad_push_event (parse->srcpad, event);
      event = NULL;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (parse, "Flushing TTML parser");
      gst_ttmlparse_cleanup (parse);
      ret = gst_pad_push_event (parse->srcpad, event);
      event = NULL;
      break;
#if GST_CHECK_VERSION (1,0,0)
    case GST_EVENT_CAPS:
    {
      GstCaps *src_caps = gst_caps_from_string (GST_TTMLPARSE_SRC_CAPS);
      GstEvent *src_event = gst_event_new_caps (src_caps);
      GST_DEBUG_OBJECT (parse->srcpad, "setting src caps to %" GST_PTR_FORMAT, src_caps);
      gst_caps_unref (src_caps);
      ret = gst_pad_push_event (parse->srcpad, src_event);
      break;
    }
#endif
    default:
      ret = gst_pad_push_event (parse->srcpad, event);
      event = NULL;
      break;
  }

beach:
  if (event) {
    /* If we haven't pushed the event downstream, then unref it */
    gst_event_unref (event);
  }
  gst_object_unref (parse);

  return ret;
}

#if GST_CHECK_VERSION (1,0,0)

static GstFlowReturn
gst_ttmlparse_chain (GstPad * pad, GstObject *parent, GstBuffer * buffer)
{
  return gst_ttmlparse_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlparse_sink_event (GstPad * pad, GstObject *parent, GstEvent * event)
{
  return gst_ttmlparse_handle_event (pad, event);
}

#else

static GstFlowReturn
gst_ttmlparse_chain (GstPad * pad, GstBuffer * buffer)
{
  return gst_ttmlparse_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlparse_sink_event (GstPad * pad, GstEvent * event)
{
  return gst_ttmlparse_handle_event (pad, event);
}
#endif

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
      gstflu_demo_reset_statistics (&parse->stats);
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
    case PROP_FORCE_BUFFER_CLEAR:
      g_value_set_boolean (value, parse->force_buffer_clear);
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
    case PROP_FORCE_BUFFER_CLEAR:
      parse->force_buffer_clear = g_value_get_boolean (value);
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
gst_ttmlparse_class_init (GstTTMLParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&ttmlparse_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&ttmlparse_src_template));

  gst_element_class_set_details_simple (gstelement_class,
    "TTML subtitle parser",
    "Codec/Parser/Subtitle",
    "Parse TTML subtitle streams into text stream",
    "Fluendo S.A. <support@fluendo.com>");
  parent_class = GST_ELEMENT_CLASS (g_type_class_peek_parent (klass));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ttmlparse_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_ttmlparse_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_ttmlparse_get_property);

  /* Register properties */
  g_object_class_install_property (gobject_class, PROP_ASSUME_ORDERED_SPANS,
      g_param_spec_boolean ("assume_ordered_spans", "Assume ordered spans",
          "Generate buffers as soon as possible, by assuming that text "
          "spans will arrive in order", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_FORCE_BUFFER_CLEAR,
      g_param_spec_boolean ("force_buffer_clear", "Force buffer clear",
          "Output an empty buffer after each text buffer to force its "
          "removal. Only needed for text renderers which do not honor "
          "buffer durations.", FALSE, G_PARAM_READWRITE));

  /* GstElement overrides */
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ttmlparse_change_state);
}

static void
gst_ttmlparse_init (GstTTMLParse * parse)
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

  parse->assume_ordered_spans = FALSE;
  parse->force_buffer_clear = FALSE;

  parse->state.attribute_stack = NULL;
  gst_ttml_state_reset (&parse->state);

  gst_ttmlparse_cleanup (parse);
  gstflu_demo_reset_statistics (&parse->stats);
}
