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

#include "gstttmlbase.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

static GstElementClass *parent_class = NULL;

enum
{
  PROP_0,
  PROP_ASSUME_ORDERED_SPANS,
  PROP_FORCE_BUFFER_CLEAR
};

static GstStaticPadTemplate ttmlbase_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_MIME));

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
gst_ttmlbase_gen_buffer (GstClockTime begin, GstClockTime end,
    GstTTMLBase * base)
{
  GstTTMLBaseClass *klass = GST_TTMLBASE_GET_CLASS (base);
  GstBuffer *buffer;
  gboolean in_seg = FALSE;
#if GST_CHECK_VERSION (1,0,0)
  guint64 clip_start = 0, clip_stop = 0;
#else
  gint64 clip_start = 0, clip_stop = 0;
#endif

  /* Do not try to push anything if we have not recovered from previous
   * errors yet */
  if (base->current_gst_status != GST_FLOW_OK)
    return;

  /* If there are no spans, check if a cleaning buffer if required */
  if (!base->active_spans && !base->force_buffer_clear) {
    return;
  }

  /* Compose output buffer based on currently active spans */
  if (!klass->gen_buffer) {
    return;
  }
  buffer = klass->gen_buffer (base);
  if (!buffer) {
    return;
  }

  in_seg = gst_segment_clip (base->segment, GST_FORMAT_TIME,
      base->base_time + begin, base->base_time + end,
      &clip_start, &clip_stop);

  if (in_seg) {
    if (G_UNLIKELY (base->newsegment_needed)) {
      GstEvent *event;

      event = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
          base->base_time, -1, 0);
      GST_DEBUG_OBJECT (base, "Pushing default newsegment");
      gst_pad_push_event (base->srcpad, event);
      base->newsegment_needed = FALSE;
    }

#if !GST_CHECK_VERSION (1,0,0)
    /* Set caps on buffer */
    gst_buffer_set_caps (buffer, GST_PAD_CAPS (base->srcpad));
#endif

    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    GST_DEBUG_OBJECT (base, "Pushing buffer of %u bytes, pts %"
        GST_TIME_FORMAT " duration %" GST_TIME_FORMAT,
        (guint)gst_buffer_get_size (buffer),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
    GST_TTML_UTILS_MEMDUMP_BUFFER_OBJECT (base, "Content:", buffer);

    base->current_gst_status = gstflu_demo_push_buffer (&base->stats,
        base->sinkpad, base->srcpad, buffer);
  } else {
    GST_DEBUG_OBJECT (base, "Buffer is out of segment (pts %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (begin));
    gst_buffer_unref (buffer);
  }
}

/* Execute the given event */
void
gst_ttmlbase_parse_event (GstTTMLEvent *event, GstTTMLBase *base)
{
  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      base->active_spans =
          gst_ttml_span_list_add (base->active_spans,
              event->data.span_begin.span);
      /* Remove the span from the event, so that when we free the event below
       * the span does not get freed too (it belongs to the active_spans list
       * now) */
      event->data.span_begin.span = NULL;
      break;
    case GST_TTML_EVENT_TYPE_SPAN_END:
      base->active_spans =
          gst_ttml_span_list_remove (base->active_spans,
              event->data.span_end.id);
      break;
    case GST_TTML_EVENT_TYPE_SPAN_ATTR_UPDATE:
      gst_ttml_span_list_update_attr (base->active_spans,
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
gst_ttmlbase_add_characters (GstTTMLBase *base, const gchar *content,
    int len, gboolean preserve_cr)
{
  const gchar *content_end = NULL;
  gint content_size = 0;
  GstTTMLSpan *span;
  GstTTMLEvent *event;
  guint id;

  /* Start by validating UTF-8 content */
  if (!g_utf8_validate (content, len, &content_end)) {
    GST_WARNING_OBJECT (base, "Content is not valid UTF-8");
    return;
  }
  content_size = content_end - content;

  /* Check if timing information is present */
  if (!GST_CLOCK_TIME_IS_VALID (base->state.begin) &&
      !GST_CLOCK_TIME_IS_VALID (base->state.end)) {
    GST_DEBUG_OBJECT (base, "Span without timing information. Dropping.");
    return;
  }

  if (base->state.node_type == GST_TTML_NODE_TYPE_P &&
      base->state.sequential_time_container) {
    /* Anonymous spans have 0 duration when inside sequential containers */
    return;
  }

  if (GST_CLOCK_TIME_IS_VALID (base->state.begin) &&
      base->state.begin >= base->state.end) {
    GST_DEBUG ("Span with 0 duration. Dropping. (begin=%" GST_TIME_FORMAT
        ", end=%" GST_TIME_FORMAT ")", GST_TIME_ARGS (base->state.begin),
        GST_TIME_ARGS (base->state.end));
    return;
  }

  /* If assuming ordered spans, as soon as our begin is later than the
   * latest event in the timeline, we can flush the timeline */
  if (base->assume_ordered_spans &&
      base->state.begin >= base->last_event_timestamp) {
    base->timeline = gst_ttml_event_list_flush (base->timeline,
        (GstTTMLEventParseFunc)gst_ttmlbase_parse_event,
        (GstTTMLEventGenBufferFunc)gst_ttmlbase_gen_buffer,
        base);
  }

  /* Create a new span to hold these characters, with an ever-increasing
   * ID number. */
  id = base->state.last_span_id++;
  span = gst_ttml_span_new (id, content_size, content, &base->state.style,
      preserve_cr);
  if (!span) {
    GST_DEBUG ("Empty span. Dropping.");
    return;
  }

  /* Insert BEGIN and END events in the timeline, with the same ID */
  event = gst_ttml_event_new_span_begin (&base->state, span);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  event = gst_ttml_event_new_span_end (&base->state, id);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  base->timeline =
      gst_ttml_style_gen_span_events (id, &base->state.style,
          base->timeline);

  base->last_event_timestamp = event->timestamp;
}

/* Helper method to turn SAX2's gchar * attribute array into a GstTTMLAttribute
 * and push it into the stack */
static void
gst_ttmlbase_push_attr (GstTTMLBase *base, const gchar **xml_attr,
    gboolean *dur_attr_found)
{
  /* Create a local copy of the attr value, since SAX2 does not
   * NULL-terminate the string */
  gsize value_len = xml_attr[4] - xml_attr[3];
  gchar *value = (gchar *)alloca(value_len + 1);
  GstTTMLAttribute *ttml_attr;
  memcpy (value, xml_attr[3], value_len);
  value[value_len] = '\0';
  ttml_attr = gst_ttml_attribute_parse (&base->state,
      !xml_attr[1]?NULL:xml_attr[2], xml_attr[0],
      value);
  if (ttml_attr) {
    if (ttml_attr->type == GST_TTML_ATTR_DUR)
      *dur_attr_found = TRUE;
    gst_ttml_state_push_attribute (&base->state, ttml_attr);
  }
}

/* Process a node start. Just push all its attributes onto the stack. */
static void
gst_ttmlbase_sax2_element_start_ns (void *ctx, const xmlChar *name,
    const xmlChar *prefix, const xmlChar *URI, int nb_namespaces,
    const xmlChar **namespaces, int nb_attributes, int nb_defaulted,
    const xmlChar **xml_attrs)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  const gchar **xml_attr = (const gchar **) xml_attrs;
  GstTTMLAttribute *ttml_attr;
  GstTTMLNodeType node_type;
  gboolean is_container_seq = base->state.sequential_time_container;
  gboolean dur_attr_found = FALSE;
  int i = nb_attributes;

  GST_LOG_OBJECT (base, "New element: %s prefix:%s URI:%s", name,
    prefix?(char *)prefix:"NULL", URI?(char *)URI:"NULL");

  node_type = gst_ttml_utils_node_type_parse (!prefix?NULL:(const gchar *)URI, (const gchar *)name);
  GST_DEBUG ("Parsed name '%s' into node type %s",
      name, gst_ttml_utils_node_type_name (node_type));

  /* Special actions for some node types */
  switch (node_type) {
    case GST_TTML_NODE_TYPE_STYLING:
      base->in_styling_node = TRUE;
      break;
    case GST_TTML_NODE_TYPE_LAYOUT:
      base->in_layout_node = TRUE;
    default:
      break;
  }

  /* Style nodes inside region nodes use nested styling. This means that they
   * are not defining a new style, buy applying their attributes directly to
   * the parent region node. The easiest way to implement this is not to push
   * the Node delimiter, so the style node attributes are appended to the
   * Region node's, as if the style node didn't exist.
   */
  if (node_type != GST_TTML_NODE_TYPE_STYLE || !base->in_layout_node) {
    /* Push onto the stack the node type, which will serve as delimiter when
     * popping attributes. */
    ttml_attr = gst_ttml_attribute_new_node (node_type);
    gst_ttml_state_push_attribute (&base->state, ttml_attr);
    /* If this node did not specify the time_container attribute, set it
     * manually to "parallel", as this is not inherited. */
    ttml_attr =
        gst_ttml_attribute_new_boolean (GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER,
        FALSE);
    gst_ttml_state_push_attribute (&base->state, ttml_attr);
    /* Manually push a 0 BEGIN attribute when in sequential mode.
     * If the node defines it, its value will overwrite this one.
     * This seemed the simplest way to take container_begin into account when
     * the node does not define a BEGIN time, since it is taken into account in
     * the _merge_attribute method. */
    if (is_container_seq) {
      ttml_attr = gst_ttml_attribute_new_time (GST_TTML_ATTR_BEGIN, 0);
      gst_ttml_state_push_attribute (&base->state, ttml_attr);
    }
  } else {
    GST_DEBUG ("  Style node inside region, attributes belong to parent node");
  }

  /* Push onto the stack the "style" and "region" attributes, if found.
   * They go first, because the attributes defined by these styles must be
   * overriden by the values defined in this node, regardless of their
   * parsing order. */
  while (i--) {
    if (strcmp (xml_attr[0], "style") == 0 ||
        strcmp (xml_attr[0], "region") == 0) {
      gst_ttmlbase_push_attr (base, xml_attr, &dur_attr_found);
    }
    xml_attr = &xml_attr[5];
  }
  /* Push onto the stack the rest of the attributes defined by this element */
  xml_attr = (const gchar **) xml_attrs;
  i = nb_attributes;
  while (i--) {
    if (strcmp (xml_attr[0], "style") != 0 &&
        strcmp (xml_attr[0], "region") != 0) {
      gst_ttmlbase_push_attr (base, xml_attr, &dur_attr_found);
    }
    xml_attr = &xml_attr[5];
  }

  /* Manually push a 0 DUR attribute if the node did not define it in
   * sequential mode. In this case this node must be ignored and this seemed
   * like the simplest way. */
  if (is_container_seq && !dur_attr_found) {
    ttml_attr = gst_ttml_attribute_new_time (GST_TTML_ATTR_DUR, 0);
    gst_ttml_state_push_attribute (&base->state, ttml_attr);
  }

  /* Now that all attributes have been parsed, set this time framework as the
   * "container" for nested elements */
  base->state.container_begin = base->state.begin;
  base->state.container_end = base->state.end;

  /* Handle special node types which have effect as soon as they are found */
  if (node_type == GST_TTML_NODE_TYPE_BR) {
    gchar br = '\n';
    gst_ttmlbase_add_characters (base, &br, 1, TRUE);
  }
}

/* Process a node end. Just pop previous state from the stack. */
static void
gst_ttmlbase_sax2_element_end_ns (void *ctx, const xmlChar *name,
    const xmlChar *prefix, const xmlChar *URI)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  GstTTMLAttribute *prev_attr;
  GstTTMLAttributeType type;
  GstClockTime current_begin = base->state.begin;
  GstClockTime current_end = base->state.end;
  GstTTMLNodeType current_node_type;

  GST_LOG_OBJECT (base, "End element: %s", name);
  current_node_type = gst_ttml_utils_node_type_parse (
      !prefix?NULL:(const gchar *)URI, (const gchar *)name);

  if (current_node_type == GST_TTML_NODE_TYPE_STYLE && base->in_layout_node) {
    /* We are closing a style node inside a layout. Its attributes are to be
     * merged with the parent region node as if this style node did not exist.
     * Therefore, we do nothing here. */
    GST_DEBUG ("  Style node inside region, no attributes are popped");
    return;
  }

  /* Special actions for some node types */
  switch (current_node_type) {
    case GST_TTML_NODE_TYPE_STYLING:
      if (!base->in_styling_node) {
        GST_WARNING_OBJECT (base, "Unmatched closing styling node");
      }
      base->in_styling_node = FALSE;
      break;
    case GST_TTML_NODE_TYPE_STYLE:
      /* We are closing a style definition. Store the current style IF
       * we are inside a <styling> node. */
      if (base->in_styling_node)
        gst_ttml_state_save_attr_stack (&base->state,
            &base->state.saved_styling_attr_stacks, base->state.id);
      break;
    case GST_TTML_NODE_TYPE_P:
      {
        /* P nodes represent paragraphs: they all should end with a line break
         * (TTML spec 7.1.5) */
        gchar br = '\n';
        gst_ttmlbase_add_characters (base, &br, 1, TRUE);
      }
      break;
    case GST_TTML_NODE_TYPE_LAYOUT:
      if (!base->in_layout_node) {
        GST_WARNING_OBJECT (base, "Unmatched closing layout node");
      }
      base->in_layout_node = FALSE;
      break;
    case GST_TTML_NODE_TYPE_REGION:
      /* We are closing a region definition. Store the current style IF
       * we are inside a <layout> node. */
      if (base->in_layout_node)
        gst_ttml_state_save_attr_stack (&base->state,
            &base->state.saved_region_attr_stacks, base->state.id);
      break;
    default:
      break;
  }

  /* Remove from the attribute stack any attribute pushed by this element */
  do {
    type = gst_ttml_state_pop_attribute (&base->state, &prev_attr);
    if (current_node_type == GST_TTML_NODE_TYPE_SET &&
        type > GST_TTML_ATTR_STYLE) {
      /* We are popping a styling attribute from a SET node: turn it into a
       * couple of entries in that attribute's timeline in the parent style */
      GstTTMLAttribute *attr;
      attr = gst_ttml_style_get_attr (&base->state.style, type);
      /* Just make sure this was actually an attribute */
      if (attr) {
        gst_ttml_attribute_add_event (attr, current_begin, prev_attr);
        gst_ttml_attribute_add_event (attr, current_end - 1, attr);
      }
    }
    if (prev_attr)
      gst_ttml_attribute_free (prev_attr);
  } while (type != GST_TTML_ATTR_NODE_TYPE);

  /* Now that we are back to our parent's context, set this time framework as
   * the "container" for nested elements.
   * Move forward the container_begin if our parent was a sequential time
   * container. */
  if (base->state.sequential_time_container) {
    GST_DEBUG ("Getting back to a seq container. Setting container_begin to %"
        GST_TIME_FORMAT, GST_TIME_ARGS (current_end));
    base->state.container_begin = current_end;
  } else {
    GST_DEBUG ("Getting back to a par container. Setting container_begin to %"
        GST_TIME_FORMAT, GST_TIME_ARGS (base->state.begin));
    base->state.container_begin = base->state.begin;
  }
  base->state.container_end = base->state.end;
}

/* Process characters */
static void
gst_ttmlbase_sax_characters (void *ctx, const xmlChar *ch, int len)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  const gchar *content = (const gchar *) ch;

  GST_DEBUG_OBJECT (base, "Found %d chars inside node type %s",
      len, gst_ttml_utils_node_type_name (base->state.node_type));
  GST_MEMDUMP ("Content:", (guint8 *)ch, len);

  switch (base->state.node_type) {
    case GST_TTML_NODE_TYPE_P:
    case GST_TTML_NODE_TYPE_SPAN:
      break;
    default:
      /* Ignore characters outside relevant nodes */
      return;
  }

  gst_ttmlbase_add_characters (base, content, len,
      base->state.whitespace_preserve);
}

/* Parse SAX warnings (simply shown as debug logs) */
static void
gst_ttmlbase_sax_warning (void *ctx, const char *message, ...)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_WARNING, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (base), message, va);
  va_end (va);
#endif /* GST_DISABLE_GST_DEBUG */
}

/* Parse SAX errors (simply shown as debug logs) */
static void
gst_ttmlbase_sax_error (void *ctx, const char *message, ...)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  va_list va;
  va_start (va, message);
  gst_debug_log_valist (GST_CAT_DEFAULT, GST_LEVEL_ERROR, __FILE__,
      __FUNCTION__, __LINE__, G_OBJECT (base), message, va);
  va_end (va);
#endif /* GST_DISABLE_GST_DEBUG */
}

/* Parse comments from XML (simply shown as debug logs) */
static void
gst_ttmlbase_sax_comment (void *ctx, const xmlChar * comment)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  GST_LOG_OBJECT (base, "Comment parsed: %s", comment);
}

/* Default handler for entities (&amp; and company) */
static xmlEntityPtr
gst_ttmlbase_sax_get_entity (void *ctx, const xmlChar * name)
{
  return xmlGetPredefinedEntity (name);
}

static void
gst_ttmlbase_sax_document_start (void *ctx)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  GST_LOG_OBJECT (GST_TTMLBASE (ctx), "Document start");

  base->in_styling_node = FALSE;
  base->in_layout_node = FALSE;
  gst_ttml_state_reset (&base->state);
}

static void
gst_ttmlbase_sax_document_end (void *ctx)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  GST_LOG_OBJECT (GST_TTMLBASE (ctx), "Document complete");

  base->timeline = gst_ttml_event_list_flush (base->timeline,
      (GstTTMLEventParseFunc)gst_ttmlbase_parse_event,
      (GstTTMLEventGenBufferFunc)gst_ttmlbase_gen_buffer, base);
}

static xmlSAXHandler gst_ttmlbase_sax_handler = {
  /* .internalSubset = */ NULL,
  /* .isStandalone = */ NULL,
  /* .hasInternalSubset = */ NULL,
  /* .hasExternalSubset = */ NULL,
  /* .resolveEntity = */ NULL,
  /* .getEntity = */ gst_ttmlbase_sax_get_entity,
  /* .entityDecl = */ NULL,
  /* .notationDecl = */ NULL,
  /* .attributeDecl = */ NULL,
  /* .elementDecl = */ NULL,
  /* .unparsedEntityDecl = */ NULL,
  /* .setDocumentLocator = */ NULL,
  /* .startDocument = */ gst_ttmlbase_sax_document_start,
  /* .endDocument = */ gst_ttmlbase_sax_document_end,
  /* .startElement = */ NULL,
  /* .endElement = */ NULL,
  /* .reference = */ NULL,
  /* .characters = */ gst_ttmlbase_sax_characters,
  /* .ignorableWhitespace = */ NULL,
  /* .processingInstruction = */ NULL,
  /* .comment = */ gst_ttmlbase_sax_comment,
  /* .warning = */ gst_ttmlbase_sax_warning,
  /* .error = */ gst_ttmlbase_sax_error,
  /* .fatalError = */ gst_ttmlbase_sax_error,
  /* .getParameterEntity = */ NULL,
  /* .cdataBlock = */ NULL,
  /* .externalSubset = */ NULL,
  /* .initialized = */ XML_SAX2_MAGIC,
  /* ._private = */ NULL,
  /* .startElementNs = */ gst_ttmlbase_sax2_element_start_ns,
  /* .endElementNs = */ gst_ttmlbase_sax2_element_end_ns,
  /* .xmlStructuredError = */ NULL
};

/* Free any parsing-related information held by the element */
static void
gst_ttmlbase_reset (GstTTMLBase * base)
{
  GST_DEBUG_OBJECT (base, "Resetting parsing information");

  if (base->xml_parser) {
    xmlFreeParserCtxt (base->xml_parser);
    base->xml_parser = NULL;
  }

  if (base->timeline) {
    g_list_free_full (base->timeline,
        (GDestroyNotify)gst_ttml_event_free);
    base->timeline = NULL;
  }
  base->last_event_timestamp = GST_CLOCK_TIME_NONE;

  if (base->active_spans) {
    g_list_free_full (base->active_spans,
        (GDestroyNotify)gst_ttml_span_free);
    base->active_spans = NULL;
  }

  gst_ttml_state_reset (&base->state);
}

/* Set downstream caps, if not done already.
 * Intersect our template caps with the peer caps and fixate
 * if required. A pad template named "src" should have been
 * installed by the derived class. */
static gboolean
gst_ttmlbase_downstream_negotiation (GstTTMLBase *base)
{
  GstCaps *src_caps, *template_caps;
  GstPadTemplate *src_pad_template;
  GstTTMLBaseClass *klass = GST_TTMLBASE_GET_CLASS (base);

#if GST_CHECK_VERSION (1,0,0)
  src_caps = gst_pad_get_current_caps (base->srcpad);
#else
  src_caps = gst_pad_get_negotiated_caps (base->srcpad);
#endif
  if (G_LIKELY (src_caps != NULL)) {
    gst_caps_unref (src_caps);
    return TRUE;
  }

  src_pad_template = gst_element_class_get_pad_template (
      GST_ELEMENT_GET_CLASS (base), "src");
  template_caps = gst_pad_template_get_caps (src_pad_template);
  gst_caps_ref (template_caps);

#if GST_CHECK_VERSION (1,0,0)
  src_caps = gst_pad_peer_query_caps (base->srcpad, template_caps);
#else
  {
    GstCaps *peer_caps = gst_pad_peer_get_caps (base->srcpad);
    if (!peer_caps) {
      return FALSE;
    }
    src_caps = gst_caps_intersect (peer_caps, template_caps);
    gst_caps_unref (peer_caps);
  }
#endif

  gst_caps_unref (template_caps);

  if (!src_caps) {
    GST_WARNING_OBJECT (base, "Could not find compatible caps for src pad");
    return FALSE;
  }

  if (!gst_caps_is_fixed (src_caps)) {
    /* If there are still values left to fixate, allow the derived class to
     * choose its preferred values. It should have overriden the fixate_caps
     * method. */
    if (klass->fixate_caps) {
      klass->fixate_caps (base, src_caps);
    } else {
      GST_WARNING_OBJECT (base, "Caps are unfixed and derived class did not "
          "provide a fixate_caps method");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (base, "setting caps %s" , gst_caps_to_string (src_caps));
  gst_pad_set_caps (base->srcpad, src_caps);

  /* Inform derived class of its final src caps.
   * This direct call avoids the hassle of having to deal with the different
   * setcaps mechanism for GStreamer 0.10 and 1.0 */
  if (klass->src_setcaps) {
    klass->src_setcaps (base, src_caps);
  }

  gst_caps_unref (src_caps);

  return TRUE;
}

static GstFlowReturn
gst_ttmlbase_handle_buffer (GstPad * pad, GstBuffer * buffer)
{
  GstTTMLBase *base;
  const char *buffer_data;
  int buffer_len;
  GstMapInfo map;

  base = GST_TTMLBASE (gst_pad_get_parent (pad));
  base->current_gst_status = GST_FLOW_OK;

  /* Last chance to set the src pad's caps. It can happen (when linking with
   * a filesrc, for example) that the 0.10 set_caps function is never called.
   * Same thing happens with the 1.0 GST_EVENT_CAPS.
   */
  if (!gst_ttmlbase_downstream_negotiation (base)) {
    base->current_gst_status = GST_FLOW_NOT_NEGOTIATED;
    goto negotiation_error;
  }

  GST_LOG_OBJECT (base, "Handling buffer of %u bytes pts %" GST_TIME_FORMAT,
      (guint)gst_buffer_get_size (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  buffer_data = (const char *) map.data;
  buffer_len = map.size;
  do {
    const char *next_buffer_data = NULL;
    int next_buffer_len = 0;

    /* Store buffer timestamp. All future timestamps we produce will be relative
     * to this buffer time. */
    if (!GST_CLOCK_TIME_IS_VALID (base->base_time)) {
      if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {
        base->base_time = GST_BUFFER_TIMESTAMP (buffer);
      } else {
        /* If we never received a valid timestamp, ok, assume 0 */
        base->base_time = 0;
      }
    }

    /* Look for end-of-document tags */
    next_buffer_data = g_strstr_len (buffer_data, buffer_len, "</tt>");

    /* If one was detected, this might be a concatenated XML file (multiple
     * XML files inside the same buffer) and we need to base them one by one
     */
    if (next_buffer_data) {
      next_buffer_data += 5;
      GST_DEBUG_OBJECT (base, "Detected XML document end at position %d of %d",
          (int)(next_buffer_data - buffer_data), buffer_len);
      next_buffer_len = buffer_len - (next_buffer_data - buffer_data);
      buffer_len = next_buffer_data - buffer_data;
    }

    /* Feed this data to the SAX parser. The rest of the processing takes place
     * in the callbacks. */
    if (!base->xml_parser) {
      GST_DEBUG_OBJECT (base,
          "Creating XML parser and parsing chunk (%d bytes)", buffer_len);
      base->xml_parser =
          xmlCreatePushParserCtxt (&gst_ttmlbase_sax_handler, base,
          buffer_data, buffer_len, NULL);
      if (!base->xml_parser) {
        GST_ERROR_OBJECT (base, "XML parser creation failed");
        goto beach;
      } else {
        GST_DEBUG_OBJECT (base, "XML Chunk finished");
      }
    } else {
      int res;
      GST_DEBUG_OBJECT (base, "Parsing XML chunk (%d bytes)", buffer_len);
      res = xmlParseChunk (base->xml_parser, buffer_data, buffer_len, 0);
      if (res != 0) {
        GST_WARNING_OBJECT (base, "XML Parsing failed");
      } else {
        GST_DEBUG_OBJECT (base, "XML Chunk finished");
      }
    }

    /* If an end-of-document tag was found, terminate this parsing process */
    if (next_buffer_data) {
      /* Destroy parser, a new one will be created if more XML files arrive */
      GST_DEBUG_OBJECT (base, "Terminating pending XML parsing works");
      xmlParseChunk (base->xml_parser, NULL, 0, 1);

      gst_ttmlbase_reset (base);
      base->base_time = GST_CLOCK_TIME_NONE;

      /* Remove trailing whitespace, or the first thing the new parser will
       * find will not be the start-of-document tag */
      while (next_buffer_len && g_ascii_isspace (*next_buffer_data)) {
        GST_DEBUG_OBJECT (base, "Skipping trailing whitespace char 0x%02x",
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
negotiation_error:
  gst_buffer_unref (buffer);

  gst_object_unref (base);

  return base->current_gst_status;
}

/* Free any information held by the element */
static void
gst_ttmlbase_cleanup (GstTTMLBase * base)
{
  GST_DEBUG_OBJECT (base, "cleaning up TTML parser");

  if (base->segment) {
    gst_segment_init (base->segment, GST_FORMAT_TIME);
  }
  base->newsegment_needed = TRUE;
  base->current_gst_status = GST_FLOW_OK;

  gst_ttmlbase_reset (base);
}

static gboolean
gst_ttmlbase_handle_event (GstPad * pad, GstEvent * event)
{
  GstTTMLBase *base;
  gboolean ret = TRUE;

  base = GST_TTMLBASE (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (base, "handling event %s", GST_EVENT_TYPE_NAME (event));

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

      GST_DEBUG_OBJECT (base, "received newsegment");

      gst_event_parse_new_segment (event, &update, &rate, &format, &start,
          &stop, &time);
      if (format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (base,
            "dropping it because it is not in TIME format");
        goto beach;
      }

      GST_DEBUG_OBJECT (base, "received new segment update %d, rate %f, "
          "start %" GST_TIME_FORMAT ", stop %" GST_TIME_FORMAT, update, rate,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

      GST_DEBUG_OBJECT (base, "our segment was %" GST_SEGMENT_FORMAT,
          base->segment);

      gst_segment_set_newsegment (base->segment, update, rate, format, start,
          stop, time);

      GST_DEBUG_OBJECT (base, "our segment now is %" GST_SEGMENT_FORMAT,
          base->segment);

      base->newsegment_needed = FALSE;
      ret = gst_pad_push_event (base->srcpad, event);
      event = NULL;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (base, "Flushing TTML parser");
      gst_ttmlbase_cleanup (base);
      ret = gst_pad_push_event (base->srcpad, event);
      event = NULL;
      break;
#if GST_CHECK_VERSION (1,0,0)
    case GST_EVENT_CAPS:
    {
      gst_ttmlbase_downstream_negotiation (base);
      break;
    }
#endif
    default:
      ret = gst_pad_push_event (base->srcpad, event);
      event = NULL;
      break;
  }

beach:
  if (event) {
    /* If we haven't pushed the event downstream, then unref it */
    gst_event_unref (event);
  }
  gst_object_unref (base);

  return ret;
}

#if GST_CHECK_VERSION (1,0,0)

static GstFlowReturn
gst_ttmlbase_chain (GstPad * pad, GstObject *parent, GstBuffer * buffer)
{
  return gst_ttmlbase_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlbase_sink_event (GstPad * pad, GstObject *parent, GstEvent * event)
{
  return gst_ttmlbase_handle_event (pad, event);
}

#else

static GstFlowReturn
gst_ttmlbase_chain (GstPad * pad, GstBuffer * buffer)
{
  return gst_ttmlbase_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlbase_sink_event (GstPad * pad, GstEvent * event)
{
  return gst_ttmlbase_handle_event (pad, event);
}
#endif

static GstStateChangeReturn
gst_ttmlbase_change_state (GstElement * element, GstStateChange transition)
{
  GstTTMLBase *base;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS, bret;

  base = GST_TTMLBASE (element);

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
      GST_DEBUG_OBJECT (base, "going from PAUSED to READY");
      gst_ttmlbase_cleanup (base);
      gstflu_demo_reset_statistics (&base->stats);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_ttmlbase_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstTTMLBase *base = GST_TTMLBASE (object);

  switch (prop_id) {
    case PROP_ASSUME_ORDERED_SPANS:
      g_value_set_boolean (value, base->assume_ordered_spans);
      break;
    case PROP_FORCE_BUFFER_CLEAR:
      g_value_set_boolean (value, base->force_buffer_clear);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlbase_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTTMLBase *base = GST_TTMLBASE (object);

  switch (prop_id) {
    case PROP_ASSUME_ORDERED_SPANS:
      base->assume_ordered_spans = g_value_get_boolean (value);
      break;
    case PROP_FORCE_BUFFER_CLEAR:
      base->force_buffer_clear = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlbase_dispose (GObject * object)
{
  GstTTMLBase *base = GST_TTMLBASE (object);

  GST_DEBUG_OBJECT (base, "disposing TTML parser");

  if (base->segment) {
    gst_segment_free (base->segment);
    base->segment = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
gst_ttmlbase_class_init (GstTTMLBaseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /* Add sink pad template. Src pad template is to be added by derived class */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&ttmlbase_sink_template));

  parent_class = GST_ELEMENT_CLASS (g_type_class_peek_parent (klass));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ttmlbase_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_ttmlbase_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_ttmlbase_get_property);

  /* Register properties */
  g_object_class_install_property (gobject_class, PROP_ASSUME_ORDERED_SPANS,
      g_param_spec_boolean ("assume_ordered_spans", "Assume ordered spans",
          "Generate buffers as soon as possible, by assuming that text "
          "spans will arrive in chronological order", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_FORCE_BUFFER_CLEAR,
      g_param_spec_boolean ("force_buffer_clear", "Force buffer clear",
          "Output an empty buffer after each text buffer to force its "
          "removal. Only needed for text renderers which do not honor "
          "buffer durations.", FALSE, G_PARAM_READWRITE));

  /* GstElement overrides */
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ttmlbase_change_state);
}

static void
gst_ttmlbase_init (GstTTMLBase * base, GstTTMLBaseClass * klass)
{
  GstPadTemplate *src_pad_template;

  /* Create sink pad */
  base->sinkpad = gst_pad_new_from_static_template (&ttmlbase_sink_template,
      "sink");
  gst_pad_set_event_function (base->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttmlbase_sink_event));
  gst_pad_set_chain_function (base->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttmlbase_chain));

  /* Create src pad. A template named "src" should have been added by the
   * derived class. */
  src_pad_template = gst_element_class_get_pad_template (
      GST_ELEMENT_CLASS (klass), "src");
  base->srcpad = gst_pad_new_from_template (src_pad_template, "src");

  gst_element_add_pad (GST_ELEMENT (base), base->sinkpad);
  gst_element_add_pad (GST_ELEMENT (base), base->srcpad);

  base->segment = gst_segment_new ();
  base->newsegment_needed = TRUE;

  base->xml_parser = NULL;
  base->base_time = GST_CLOCK_TIME_NONE;
  base->current_gst_status = GST_FLOW_OK;
  base->timeline = NULL;

  base->assume_ordered_spans = FALSE;
  base->force_buffer_clear = FALSE;

  base->state.attribute_stack = NULL;
  gst_ttml_state_reset (&base->state);

  gst_ttmlbase_cleanup (base);
  gstflu_demo_reset_statistics (&base->stats);
}

GType
gst_ttmlbase_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstTTMLBaseClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_ttmlbase_class_init,
      NULL,
      NULL,
      sizeof (GstTTMLBase),
      0,
      (GInstanceInitFunc) gst_ttmlbase_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstTTMLBase", &info,
            G_TYPE_FLAG_ABSTRACT));
  }

  return type;
}
