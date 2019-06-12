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
#include "gstttmlnamespace.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug
#define TTML_DEBUG_XML_INPUT 0

/* We dynamically allocate and reallocate a buffer for sax chars accumulation,
 * minimizing reallocations.
 * This is the minimum free size in the buffer before we reallocate more */
#define TTML_SAX_BUFFER_MIN_FREE_SIZE 0x10
#define TTML_SAX_BUFFER_GROW_SIZE 0x400

/* check for white space as required for TTML & UTF-8 */
#define is_whitespace(c) ((guchar)(c) <= 0x20)

static GstElementClass *parent_class = NULL;

enum
{
  PROP_0,
  PROP_ASSUME_ORDERED_SPANS,
};

static GstStaticPadTemplate ttmlbase_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_MIME));

#if GST_CHECK_VERSION (1,0,0)

/* Compatibility functions for segment handling */
GstEvent *
gst_event_new_new_segment (gboolean update, gdouble rate,
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

void
gst_event_parse_new_segment (GstEvent * event, gboolean * update,
    gdouble * rate, GstFormat * format, gint64 * start, gint64 * stop,
    gint64 * position)
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

void
gst_segment_set_newsegment (GstSegment * segment, gboolean update,
    gdouble rate, GstFormat format, gint64 start, gint64 stop, gint64 time)
{
  segment->rate = rate;
  segment->format = format;
  segment->start = start;
  segment->stop = stop;
  segment->position = time;
}

#endif

static gboolean gst_ttmlbase_downstream_negotiation (GstTTMLBase * base);
static void
gst_ttmlbase_sax_characters (void *ctx, const xmlChar * ch, int len);

/* Remove CR characters and surrounding white space
 * @single_space: generate a single space on newline between non space */
static gsize
gst_ttmlbase_clean_whitespace (gchar * buf, gsize len, gboolean single_space)
{
  gchar *src = buf;
  gchar *dst = buf;
  gchar *end = buf + len;
  gchar c;
  gboolean collapsing = TRUE;

  while (src < end) {
    c = *src++;
    if (c == '\n') {
      collapsing = TRUE;
      /* clear space before newline */
      while (dst > buf && is_whitespace (*(dst - 1)))
        dst--;
    } else if (collapsing) {
      /* clear space after newline */
      if (is_whitespace (c)) {
        src++;
      } else {
        collapsing = FALSE;
        if (single_space && dst != buf)
          *dst++ = ' ';
        *dst++ = c;
      }
    } else {
      /* not collapsing, copy character */
      *dst++ = c;
    }
  }
  return dst - buf;
}

/* Generate and Pad push a buffer, using the correct timestamps and clipping */
static void
gst_ttmlbase_gen_buffer (GstClockTime begin, GstClockTime end,
    GstTTMLBase * base)
{
  GstTTMLBaseClass *klass = GST_TTMLBASE_GET_CLASS (base);
  gboolean in_seg = FALSE;
#if GST_CHECK_VERSION (1,0,0)
  guint64 clip_start = 0, clip_stop = 0;
#else
  gint64 clip_start = 0, clip_stop = 0;
#endif

  GST_DEBUG_OBJECT (base, "gen_buffer %" GST_TIME_FORMAT
      " - %" GST_TIME_FORMAT, GST_TIME_ARGS (begin), GST_TIME_ARGS (end));
  if (begin < base->last_out_time) {
    begin = base->last_out_time;
    GST_DEBUG_OBJECT (base, "adjusted %" GST_TIME_FORMAT
        " - %" GST_TIME_FORMAT, GST_TIME_ARGS (begin), GST_TIME_ARGS (end));
  }

  /* Last chance to set the src pad's caps. It can happen (when linking with
   * a filesrc, for example) that the 0.10 set_caps function is never called.
   * Same thing happens with the 1.0 GST_EVENT_CAPS.
   */
  if (!gst_ttmlbase_downstream_negotiation (base)) {
    base->current_gst_status = GST_FLOW_NOT_NEGOTIATED;
    return;
  }

  /* Do not try to push anything if we have not recovered from previous
   * errors yet */
  if (base->current_gst_status != GST_FLOW_OK)
    return;

  /* Compose output buffer based on currently active spans */
  if (!klass->gen_buffer) {
    return;
  }

  if (!base->segment) {
    GstClockTime start;

    /* We have not received any newsegment from upstream, make our own */
    base->segment = gst_segment_new ();
    /* It might happen that we need to push a clear buffer, if so, and we
     * don't have configured any new segment yet, we'll choose the input
     * buffer timestamp as newsgement start, but that is invalid for the
     * clear buffer
     */
    if (!base->active_spans)
      start = begin;
    else
      start = base->base_time;
    gst_segment_set_newsegment (base->segment, FALSE, 1.0, GST_FORMAT_TIME,
        start, -1, clip_start);
    GST_DEBUG_OBJECT (base, "Generating a new segment start:%" GST_TIME_FORMAT
        " stop:%" GST_TIME_FORMAT " time:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (base->segment->start),
        GST_TIME_ARGS (base->segment->stop),
        GST_TIME_ARGS (base->segment->time));
  }

  in_seg = gst_segment_clip (base->segment, GST_FORMAT_TIME,
      begin, end, &clip_start, &clip_stop);

  if (in_seg) {
    GstBuffer *buffer;

    buffer = klass->gen_buffer (base, clip_start, clip_stop - clip_start);
    if (!buffer) {
      return;
    }

    if (G_UNLIKELY (base->newsegment_needed)) {
      GstEvent *event;
      event = gst_event_new_new_segment (FALSE, base->segment->rate,
          base->segment->format, base->segment->start, base->segment->stop,
          base->segment->time);
      GST_DEBUG_OBJECT (base, "Pushing newsegment start:%" GST_TIME_FORMAT
          " stop:%" GST_TIME_FORMAT " time:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (base->segment->start),
          GST_TIME_ARGS (base->segment->stop),
          GST_TIME_ARGS (base->segment->time));
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
        (guint) gst_buffer_get_size (buffer),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));
    GST_TTML_UTILS_MEMDUMP_BUFFER_OBJECT (base, "Content:", buffer);

    base->current_gst_status = gstflu_demo_push_buffer (&base->stats,
        base->sinkpad, base->srcpad, buffer);
    base->last_out_time = clip_stop;
  } else {
    GST_DEBUG_OBJECT (base, "Buffer is out of segment (pts %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (begin));
  }
}

/* Execute the given event */
GList *
gst_ttmlbase_parse_event (GstTTMLEvent * event, GstTTMLBase * base,
    GList * timeline)
{
  GList *output_timeline = timeline;

  switch (event->type) {
    case GST_TTML_EVENT_TYPE_SPAN_BEGIN:
      /* Add span to the list of active spans */
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
    case GST_TTML_EVENT_TYPE_REGION_BEGIN:
      gst_ttml_state_new_region (&base->state, event->data.region_begin.id,
          &event->data.region_begin.style);
      /* Remove the style from the event, so that when we free the event below
       * the style does not get freed too (it belongs to the hash of regions now)
       */
      event->data.region_begin.style.attributes = NULL;
      break;
    case GST_TTML_EVENT_TYPE_REGION_END:
      gst_ttml_state_remove_region (&base->state, event->data.region_end.id);
      break;
    case GST_TTML_EVENT_TYPE_REGION_ATTR_UPDATE:
      gst_ttml_state_update_region_attr (&base->state,
          event->data.region_update.id, event->data.region_update.attr);
      break;
    default:
      GST_WARNING ("Unknown event type");
      break;
  }
  gst_ttml_event_free (event);
  return output_timeline;
}

/* Allocate a new span to hold new characters, and insert into the timeline
 * BEGIN and END events to handle this new span.
 * @newline: Append a newline (useful for <br/> and </p>, as
 * these must generate spans ended with \n while </span> doesn't
 */
static void
gst_ttmlbase_add_span (GstTTMLBase * base, gboolean newline)
{
  const gchar *content_end = NULL;
  GstTTMLSpan *span;
  GstTTMLEvent *event;
  guint id;

  /* clean whitespace */
  if (base->buf_len && !base->state.whitespace_preserve)
    base->buf_len =
        gst_ttmlbase_clean_whitespace (base->buf, base->buf_len, TRUE);

  /* validate UTF-8 content. Should we care? */
  if (!g_utf8_validate (base->buf, base->buf_len, &content_end)) {
    GST_WARNING_OBJECT (base, "Content is not valid UTF-8");
    goto beach;
  }

  if (newline) {
    xmlChar ch = '\n';
    gst_ttmlbase_sax_characters (base, &ch, 1);
  }

  if (!base->buf_len) {
    GST_DEBUG ("Empty span. Dropping.");
    goto beach;
  }

  /* our buffer allocator has always extra space to reduce reallocations */
  base->buf[base->buf_len] = '\0';
  GST_LOG_OBJECT (base, "span content: %s", base->buf);


  /* Check if timing information is present */
  if (!GST_CLOCK_TIME_IS_VALID (base->state.begin) &&
      !GST_CLOCK_TIME_IS_VALID (base->state.end)) {
    GST_DEBUG_OBJECT (base, "Span without timing information. Dropping.");
    goto beach;
  }

  if (base->state.node_type == GST_TTML_NODE_TYPE_P &&
      base->state.sequential_time_container) {
    /* Anonymous spans have 0 duration when inside sequential containers */
    goto beach;
  }

  if (GST_CLOCK_TIME_IS_VALID (base->state.begin) &&
      base->state.begin >= base->state.end) {
    GST_DEBUG ("Span with 0 duration. Dropping. (begin=%" GST_TIME_FORMAT
        ", end=%" GST_TIME_FORMAT ")", GST_TIME_ARGS (base->state.begin),
        GST_TIME_ARGS (base->state.end));
    goto beach;
  }

  /* If assuming ordered spans, as soon as our begin is later than the
   * latest event in the timeline, we can flush the timeline */
  if (base->assume_ordered_spans && base->state.begin > base->last_out_time) {
    base->timeline = gst_ttml_event_list_flush (base->timeline,
        (GstTTMLEventParseFunc) gst_ttmlbase_parse_event,
        (GstTTMLEventGenBufferFunc) gst_ttmlbase_gen_buffer, base);
  }

  /* Create a new span to hold these characters, with an ever-increasing
   * ID number. */
  id = base->state.last_span_id++;
  span = gst_ttml_span_new (id, base->buf_len, base->buf, &base->state.style);
  if (!span) {
    GST_DEBUG ("Empty span. Dropping.");
    goto beach;
  }

  /* Insert BEGIN and END events in the timeline, with the same ID */
  event = gst_ttml_event_new_span_begin (&base->state, span);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  event = gst_ttml_event_new_span_end (&base->state, id);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  base->timeline =
      gst_ttml_style_gen_span_events (id, &base->state.style, base->timeline);

beach:
  /* empty the accumulator buffer */
  base->buf_len = 0;
}

/* Insert into the timeline new BEGIN and END events to handle this region. */
static void
gst_ttmlbase_add_region (GstTTMLBase * base)
{
  GstTTMLEvent *event;
  GstTTMLAttribute *attr;

  if (GST_CLOCK_TIME_IS_VALID (base->state.begin) &&
      base->state.begin >= base->state.end) {
    GST_DEBUG ("Region with 0 duration. Dropping. (begin=%" GST_TIME_FORMAT
        ", end=%" GST_TIME_FORMAT ")", GST_TIME_ARGS (base->state.begin),
        GST_TIME_ARGS (base->state.end));
    return;
  }

  attr = gst_ttml_style_get_attr (&base->state.style, GST_TTML_ATTR_ZINDEX);
  if (!attr) {
    /* Insert a manual "0" zIndex attribute with its ever-increasing 1e-3 index.
     * See gst_ttml_attribute_parse() for GST_TTML_ATTR_ZINDEX.
     * This forces lexical order of rendering on overlapping regions without
     * explicit zIndex. */
    attr = gst_ttml_attribute_new_int (GST_TTML_ATTR_ZINDEX,
        base->state.last_zindex_micro);
    base->state.last_zindex_micro++;
    gst_ttml_state_push_attribute (&base->state, attr);
  }

  /* Insert BEGIN and END events in the timeline */
  event = gst_ttml_event_new_region_begin (base->state.begin, base->state.id,
      &base->state.style);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  event = gst_ttml_event_new_region_end (base->state.end, base->state.id);
  base->timeline = gst_ttml_event_list_insert (base->timeline, event);

  base->timeline =
      gst_ttml_style_gen_region_events (base->state.id, &base->state.style,
      base->timeline);

  /* We keep the attr pointer, but its content does not belong to us, there
   * is no harm in overwritting it here. */
  gst_ttml_state_pop_attribute (&base->state, &attr);
  gst_ttml_attribute_free (attr);

}

/* Base64-decodes the now complete embedded data and adds it to the hash of
 * saved data, using the current ID. Data still needs to be PNG-decoded if
 * it is an image. */
static void
gst_ttmlbase_decode_data (GstTTMLBase * base)
{
  gsize decoded_length;
  GstTTMLAttribute *attr;

  /* clean whitespace */
  if (base->buf_len)
    base->buf_len =
        gst_ttmlbase_clean_whitespace (base->buf, base->buf_len, FALSE);

  if (!base->buf || !base->buf_len) {
    GST_WARNING_OBJECT (base, "Found empty data node. Ignoring.");
    goto beach;
  }

  attr = gst_ttml_style_get_attr (&base->state.style,
      GST_TTML_ATTR_SMPTE_ENCODING);
  if (attr && attr->value.smpte_encoding != GST_TTML_SMPTE_ENCODING_BASE64) {
    GST_WARNING_OBJECT (base, "Only Base64 encoding is supported. "
        "Discarding data.");
    goto beach;
  }

  /* FIXME: This check does not belong here. The imagetype should be stored
   * with the data, and whoever reads the data should care about its type */
  attr = gst_ttml_style_get_attr (&base->state.style,
      GST_TTML_ATTR_SMPTE_IMAGETYPE);
  if (attr && attr->value.smpte_image_type != GST_TTML_SMPTE_IMAGE_TYPE_PNG) {
    GST_WARNING_OBJECT (base, "Only PNG images are supported. "
        "Discarding image.");
    goto beach;
  }

  if (!base->state.id) {
    GST_WARNING_OBJECT (base, "Found data node without ID. "
        "Discarding data.");
    goto beach;
  }

  /* Add terminator, for glib's base64 decoder. Our allocator ensures there's
   * space for it, no need to reallocate */
  base->buf[base->buf_len] = '\0';

  /* Decode */
  g_base64_decode_inplace ((gchar *) base->buf, &decoded_length);

  /* Store */
  gst_ttml_state_save_data (&base->state, (guint8 *) base->buf,
      decoded_length, base->state.id);
  goto beach;

beach:
  base->buf_len = 0;
}

/* Helper method to turn SAX2's gchar * attribute array into a GstTTMLAttribute
 * and push it into the stack */
static void
gst_ttmlbase_push_attr (GstTTMLBase * base, const gchar ** xml_attr,
    gboolean * dur_attr_found)
{
  /* Create a local copy of the attr value, since SAX2 does not
   * NULL-terminate the string */
  gsize value_len = xml_attr[4] - xml_attr[3];
  gchar *value = (gchar *) alloca (value_len + 1);
  GstTTMLAttribute *ttml_attr;
  memcpy (value, xml_attr[3], value_len);
  value[value_len] = '\0';
  ttml_attr = gst_ttml_attribute_parse (&base->state,
      !xml_attr[1] ? NULL : xml_attr[2], xml_attr[0], value);
  if (ttml_attr) {
    if (ttml_attr->type == GST_TTML_ATTR_DUR)
      *dur_attr_found = TRUE;
    gst_ttml_state_push_attribute (&base->state, ttml_attr);
  }
}

/* Process a node start. Just push all its attributes onto the stack. */
static void
gst_ttmlbase_sax2_element_start_ns (void *ctx, const xmlChar * name,
    const xmlChar * prefix, const xmlChar * URI, int nb_namespaces,
    const xmlChar ** namespaces, int nb_attributes, int nb_defaulted,
    const xmlChar ** xml_attrs)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  const gchar **xml_attr = (const gchar **) xml_attrs;
  GstTTMLAttribute *ttml_attr;
  GstTTMLNodeType node_type;
  gboolean is_container_seq = base->state.sequential_time_container;
  gboolean dur_attr_found = FALSE;
  int i = nb_attributes;

  GST_LOG_OBJECT (base, "New element: %s prefix:%s URI:%s", name,
      prefix ? (char *) prefix : "NULL", URI ? (char *) URI : "NULL");

  node_type =
      gst_ttml_utils_node_type_parse (!prefix ? NULL : (const gchar *) URI,
      (const gchar *) name);
  GST_DEBUG ("Parsed name '%s' into node type %s", name,
      gst_ttml_utils_enum_name (node_type, NodeType));
  /* Special actions for some node types */
  switch (node_type) {
    case GST_TTML_NODE_TYPE_P:
    case GST_TTML_NODE_TYPE_SPAN:
      base->buf_len = 0;
      break;
    case GST_TTML_NODE_TYPE_TT:
    {
      /* store namespaces and initialize default values depending on
       * standards identified by its namespaces */
      gchar **nss = (gchar **) namespaces;
      GstTTMLNamespace *ns;

      base->is_std_ebu = FALSE;
      base->state.cell_resolution_x = 32;
      base->state.cell_resolution_y = 15;

      while (nb_namespaces--) {
        gchar *nsname = *(nss++);
        gchar *nsvalue = *(nss++);
        GST_DEBUG_OBJECT (base, "Storing namespace %s=%s", nsname ? nsname :
            (gchar *) "", nsvalue);
        ns = gst_ttml_namespace_new (nsname, nsvalue);
        base->namespaces = g_list_append (base->namespaces, ns);
        if (strstr (nsvalue, "ebu:tt")) {
          base->is_std_ebu = TRUE;
          base->state.cell_resolution_x = 40;
          base->state.cell_resolution_y = 24;
        }
      }
      break;
    }
    case GST_TTML_NODE_TYPE_STYLING:
      base->in_styling_node = TRUE;
      break;
    case GST_TTML_NODE_TYPE_LAYOUT:
      base->in_layout_node = TRUE;
      break;
    case GST_TTML_NODE_TYPE_METADATA:
      base->in_metadata_node = TRUE;
      break;
    case GST_TTML_NODE_TYPE_SMPTE_IMAGE:
      if (!base->in_metadata_node) {
        GST_WARNING_OBJECT (base,
            "Image node is invalid outside of metadata node. Parsing anyway.");
      }
      base->buf_len = 0;
      break;
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

  /* Push onto the stack the "style" attributes, if found.
   * They go first, because the attributes defined by these styles must be
   * overriden by the values defined in this node, regardless of their
   * parsing order. */
  while (i--) {
    if (strcmp (xml_attr[0], "style") == 0) {
      gst_ttmlbase_push_attr (base, xml_attr, &dur_attr_found);
    }
    xml_attr = &xml_attr[5];
  }
  /* Push onto the stack the rest of the attributes defined by this element */
  xml_attr = (const gchar **) xml_attrs;
  i = nb_attributes;
  while (i--) {
    if (strcmp (xml_attr[0], "style") != 0) {
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
    gst_ttmlbase_add_span (base, TRUE);
  }
}

/* Process a node end. Just pop previous state from the stack. */
static void
gst_ttmlbase_sax2_element_end_ns (void *ctx, const xmlChar * name,
    const xmlChar * prefix, const xmlChar * URI)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  GstTTMLAttribute *prev_attr;
  GstTTMLAttributeType type;
  GstClockTime current_begin = base->state.begin;
  GstClockTime current_end = base->state.end;
  GstTTMLNodeType current_node_type;

  GST_LOG_OBJECT (base, "End element: %s", name);
  current_node_type =
      gst_ttml_utils_node_type_parse (!prefix ? NULL : (const gchar *) URI,
      (const gchar *) name);

  if (current_node_type == GST_TTML_NODE_TYPE_STYLE && base->in_layout_node) {
    /* We are closing a style node inside a layout. Its attributes are to be
     * merged with the parent region node as if this style node did not exist.
     * Therefore, we do nothing here. */
    GST_DEBUG ("  Style node inside region, no attributes are popped");
    return;
  }

  /* Special actions for some node types */
  switch (current_node_type) {
    case GST_TTML_NODE_TYPE_P:
      gst_ttmlbase_add_span (base, TRUE);
      break;
    case GST_TTML_NODE_TYPE_SPAN:
      gst_ttmlbase_add_span (base, FALSE);
      break;
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
    case GST_TTML_NODE_TYPE_LAYOUT:
      if (!base->in_layout_node) {
        GST_WARNING_OBJECT (base, "Unmatched closing layout node");
      }
      base->in_layout_node = FALSE;
      break;
    case GST_TTML_NODE_TYPE_REGION:
      /* We are closing a region definition. Store the current style IF
       * we are inside a <layout> node. */
      if (base->in_layout_node) {
        gst_ttmlbase_add_region (base);
      }
      break;
    case GST_TTML_NODE_TYPE_METADATA:
      if (!base->in_metadata_node) {
        GST_WARNING_OBJECT (base, "Unmatched closing metadata node");
      }
      base->in_metadata_node = FALSE;
      break;
    case GST_TTML_NODE_TYPE_SMPTE_IMAGE:
      gst_ttmlbase_decode_data (base);
      break;
    default:
      break;
  }

  /* Remove from the attribute stack any attribute pushed by this element */
  do {
    /* FIXME: Popping a region attribute will push other attributes, which will
     * need to be popped. Not a problem, unless inside a SET node */
    type = gst_ttml_state_pop_attribute (&base->state, &prev_attr);
    if (current_node_type == GST_TTML_NODE_TYPE_SET &&
        type > GST_TTML_ATTR_STYLE) {
      /* We are popping a styling attribute from a SET node: turn it into a
       * couple of entries in that attribute's timeline in the parent style */
      GstTTMLAttribute *attr;
      attr = gst_ttml_style_get_attr (&base->state.style, type);
      /* We are changing an attr which has not been defined yet */
      if (!attr) {
        /* FIXME: This can be certainly improved */
        attr = g_new0 (GstTTMLAttribute, 1);
        attr->type = type;
        base->state.style.attributes =
            g_list_append (base->state.style.attributes, attr);
      }
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
gst_ttmlbase_sax_characters (void *ctx, const xmlChar * ch, int len)
{
  GstTTMLBase *base = GST_TTMLBASE (ctx);
  gsize tlen = base->buf_len + len;

  /* allocate or reallocate buffer if needed */
  if (tlen + TTML_SAX_BUFFER_MIN_FREE_SIZE >= base->buf_size) {
    gsize size = tlen + TTML_SAX_BUFFER_GROW_SIZE;
    if (!base->buf_size) {
      base->buf = g_malloc (size);
    } else {
      base->buf = g_realloc (base->buf, size);
    }
    base->buf_size = size;
  }

  /* accumulate characters in buffer */
  memcpy (base->buf + base->buf_len, ch, len);
  base->buf_len = tlen;
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
      (GstTTMLEventParseFunc) gst_ttmlbase_parse_event,
      (GstTTMLEventGenBufferFunc) gst_ttmlbase_gen_buffer, base);
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
  GstTTMLBaseClass *klass = GST_TTMLBASE_GET_CLASS (base);

  GST_DEBUG_OBJECT (base, "Resetting parsing information");

  if (base->xml_parser) {
    xmlFreeParserCtxt (base->xml_parser);
    base->xml_parser = NULL;
  }

  if (base->timeline) {
    g_list_free_full (base->timeline, (GDestroyNotify) gst_ttml_event_free);
    base->timeline = NULL;
  }

  if (base->active_spans) {
    g_list_free_full (base->active_spans, (GDestroyNotify) gst_ttml_span_free);
    base->active_spans = NULL;
  }

  if (base->namespaces) {
    g_list_free_full (base->namespaces,
        (GDestroyNotify) gst_ttml_namespace_free);
    base->namespaces = NULL;
  }

  if (klass->reset) {
    klass->reset (base);
  }

  gst_ttml_state_reset (&base->state);
}

/* Set downstream caps, if not done already.
 * Intersect our template caps with the peer caps and fixate
 * if required. A pad template named "src" should have been
 * installed by the derived class. */
static gboolean
gst_ttmlbase_downstream_negotiation (GstTTMLBase * base)
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

  src_pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (base), "src");
  template_caps = gst_caps_copy (gst_pad_template_get_caps (src_pad_template));

  /* Allow derived class to add last-minute restrictions, like PAR, which
   * is only found during parsing. */
  if (klass->complete_caps) {
    klass->complete_caps (base, template_caps);
  }
#if GST_CHECK_VERSION (1,0,0)
  src_caps = gst_pad_peer_query_caps (base->srcpad, template_caps);
#else
  {
    GstCaps *peer_caps = gst_pad_peer_get_caps (base->srcpad);
    if (!peer_caps) {
      GST_WARNING_OBJECT (base, "Peer has no caps!");
      return FALSE;
    }
    src_caps = gst_caps_intersect (peer_caps, template_caps);
    gst_caps_unref (peer_caps);
  }
#endif

  gst_caps_unref (template_caps);

  if (!src_caps || gst_caps_is_empty (src_caps)) {
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
      gst_caps_unref (src_caps);
      return FALSE;
    }
  }

  /* Fixate the caps by common means. This give an issue with some default
   * fields that are fixed to the first option available, like for example
   * the interlaced field. Sadly, if we dont fixate like this, the pad
   * can not have non-fixed caps
   */
#if GST_CHECK_VERSION (1,0,0)
  src_caps = gst_caps_fixate (src_caps);
#else
  gst_pad_fixate_caps (base->srcpad, src_caps);
#endif

  GST_DEBUG_OBJECT (base, "setting caps %" GST_PTR_FORMAT, src_caps);
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

void
gst_ttmlbase_dump_buffer (GstTTMLBase * thiz, GstBuffer * buffer)
{
  FILE *file;
  file = fopen ("ttml.xml", "ab");
  if (file) {
    fprintf (file, "----------------------------------------------------------"
        "---------------------------------------------------------------------"
        "\n%s\n", GST_ELEMENT_NAME (thiz));
    fwrite (GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer), 1, file);
    fclose (file);
  }
}

static GstFlowReturn
gst_ttmlbase_handle_buffer (GstPad * pad, GstBuffer * buffer)
{
  GstTTMLBase *base;
  const char *buffer_data;
  int buffer_len;
  GstMapInfo map;
  GstClockTime ts, dur;

  base = GST_TTMLBASE (gst_pad_get_parent (pad));
  base->current_gst_status = GST_FLOW_OK;

  ts = GST_BUFFER_TIMESTAMP (buffer);
  dur = GST_BUFFER_DURATION (buffer);
  GST_LOG_OBJECT (base, "Handling buffer of %u bytes pts %" GST_TIME_FORMAT
      "dur %" GST_TIME_FORMAT, (guint) gst_buffer_get_size (buffer),
      GST_TIME_ARGS (ts), GST_TIME_ARGS (dur));
#if TTML_DEBUG_XML_INPUT
  gst_ttmlbase_dump_buffer (base, buffer);
#endif

  if (GST_CLOCK_TIME_IS_VALID (ts)) {
    base->input_buf_start = ts;
    base->input_buf_stop = GST_CLOCK_TIME_IS_VALID (dur) ?
        ts + dur : GST_CLOCK_TIME_NONE;
  } else {
    base->input_buf_start = 0;
    base->input_buf_stop = GST_CLOCK_TIME_NONE;
  }
  if (!GST_CLOCK_TIME_IS_VALID (base->base_time))
    base->base_time = base->input_buf_start;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  buffer_data = (const char *) map.data;
  buffer_len = map.size;
  do {
    const char *next_buffer_data = NULL;
    int next_buffer_len = 0;

    /* Look for end-of-document tags */
    next_buffer_data = g_strstr_len (buffer_data, buffer_len, "</tt>");

    /* If one was detected, this might be a concatenated XML file (multiple
     * XML files inside the same buffer) and we need to parse them one by one
     */
    if (next_buffer_data) {
      next_buffer_data += 5;
      GST_DEBUG_OBJECT (base, "Detected XML document end at position %d of %d",
          (int) (next_buffer_data - buffer_data), buffer_len);
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
  gst_buffer_unref (buffer);

  gst_object_unref (base);

  return base->current_gst_status;
}

/* Retrieve the URI of the TTML file we are currently parsing by asking
 * upstream elements until we find a Source with a "location" property.
 * Free after use.
 * Code copied from fludashparser.c */
gchar *
gst_ttmlbase_uri_get (GstPad * pad)
{
  GstObject *parent;
  gchar *uri = NULL;

  parent = gst_pad_get_parent (pad);
  if (!parent)
    return NULL;

  if (GST_IS_GHOST_PAD (parent)) {
    GstPad *peer;

    peer = gst_pad_get_peer (GST_PAD (parent));
    uri = gst_ttmlbase_uri_get (peer);
    gst_object_unref (peer);

  } else {
    GstElementFactory *f;
    const gchar *klass;

    f = gst_element_get_factory (GST_ELEMENT (parent));
    klass = gst_element_factory_get_klass (f);
    if (g_strstr_len (klass, -1, "Source") != NULL) {
      gchar *scheme;

      /* try to get the location property */
      g_object_get (G_OBJECT (parent), "location", &uri, NULL);
      scheme = g_uri_parse_scheme (uri);
      if (!scheme) {
        if (!g_path_is_absolute (uri)) {
          gchar *absolute, *cwd;
          cwd = g_get_current_dir ();
          absolute = g_build_filename (cwd, uri, NULL);
          g_free (cwd);
          g_free (uri);
          uri = g_strdup_printf ("file://%s", absolute);
          g_free (absolute);
        } else {
          gchar *tmp = uri;
          uri = g_strdup_printf ("file://%s", tmp);
          g_free (tmp);
        }
      } else {
        g_free (scheme);
      }
    } else {
      GstPad *sink_pad;
      GstPad *peer;
      GstIterator *iter = gst_element_iterate_sink_pads (GST_ELEMENT (parent));

      /* iterate over the sink pads */
#if !GST_CHECK_VERSION (1,0,0)
      while (gst_iterator_next (iter,
              (gpointer *) & sink_pad) != GST_ITERATOR_DONE) {
#else
      GValue sink_pad_value = G_VALUE_INIT;
      while (gst_iterator_next (iter, &sink_pad_value) != GST_ITERATOR_DONE) {
        sink_pad = (GstPad *) g_value_get_object (&sink_pad_value);
#endif

        peer = gst_pad_get_peer (sink_pad);
        uri = gst_ttmlbase_uri_get (peer);
#if !GST_CHECK_VERSION (1,0,0)
        gst_object_unref (sink_pad);
#else
        g_value_reset (&sink_pad_value);
#endif
        gst_object_unref (peer);
        if (uri)
          break;
      }
      gst_iterator_free (iter);
    }
  }
  gst_object_unref (parent);

  return uri;
}

/* Free any information held by the element */
static void
gst_ttmlbase_cleanup (GstTTMLBase * base)
{
  GST_DEBUG_OBJECT (base, "cleaning up TTML parser");

  if (base->segment) {
    gst_segment_init (base->segment, GST_FORMAT_TIME);
  }

  if (base->pending_segment) {
    GST_DEBUG_OBJECT (base, "We have a pending new segment at start:%"
        GST_TIME_FORMAT " stop:%" GST_TIME_FORMAT " using it",
        GST_TIME_ARGS (base->pending_segment->start),
        GST_TIME_ARGS (base->pending_segment->stop));
    if (base->segment)
      gst_segment_free (base->segment);
    base->segment = base->pending_segment;
    base->pending_segment = NULL;
  }

  base->newsegment_needed = TRUE;
  base->current_gst_status = GST_FLOW_OK;
  base->input_buf_start = 0;
  base->last_out_time = 0;

  gst_ttmlbase_reset (base);
}

static gboolean
gst_ttmlbase_handle_sink_event (GstPad * pad, GstEvent * event)
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
        GST_DEBUG_OBJECT (base, "dropping it because it is not in TIME format");
        goto beach;
      }

      GST_DEBUG_OBJECT (base, "received new segment update %d, rate %f, "
          "start %" GST_TIME_FORMAT ", stop %" GST_TIME_FORMAT
          ", time %" GST_TIME_FORMAT, update, rate,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop), GST_TIME_ARGS (time));

      GST_DEBUG_OBJECT (base, "our segment was %" GST_SEGMENT_FORMAT,
          base->segment);

      if (!base->segment)
        base->segment = gst_segment_new ();

      gst_segment_set_newsegment (base->segment, update, rate, format, start,
          stop, time);

      base->last_out_time = start;

      GST_DEBUG_OBJECT (base, "our segment now is %" GST_SEGMENT_FORMAT,
          base->segment);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      /* FIXME we have a race condition when seeking/flushing. The FLUSH_START
       * triggers a flush in the src pad that causes that every gst_pad_push()
       * will return GST_FLOW_WRONG_STATE, therefore the the streaming thread
       * that is iterating over the list of ttml events to render will take time
       * to clean it up.
       * After the FLUSH_START we'll receive the FLUSH_STOP where we are just
       * cleaning the list of attributes/tineline but the chain might be still
       * using it. Remember, FLUSH_START/FLUSH_STOP might come from different
       * threads
       */
      GST_DEBUG_OBJECT (base, "Flushing TTML parser");
      gst_ttmlbase_cleanup (base);
      ret = gst_pad_push_event (base->srcpad, event);
      event = NULL;
      break;
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

static gboolean
gst_ttmlbase_do_seek (GstTTMLBase * base, GstEvent * seek)
{
  GstSeekFlags flags;
  gdouble rate;
  gint64 stop;
  gint64 start;
  gboolean ret;

  /* set our new segment to the request seek parameters */
  gst_event_parse_seek (seek, &rate, NULL, &flags, NULL, &start, NULL, &stop);
  if (!base->pending_segment)
    base->pending_segment = gst_segment_new ();

  GST_DEBUG_OBJECT (base, "Seeking to start:%" GST_TIME_FORMAT " stop:%"
      GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
  gst_segment_set_newsegment (base->pending_segment, FALSE, rate,
      GST_FORMAT_TIME, start, stop, start);
  gst_event_unref (seek);

  /* do a 0, -1 seek upstream in bytes */
  seek = gst_event_new_seek (1.0, GST_FORMAT_BYTES, flags, GST_SEEK_TYPE_SET,
      0, GST_SEEK_TYPE_SET, -1);
  ret = gst_pad_push_event (base->sinkpad, seek);

  return ret;
}

static gboolean
gst_ttmlbase_handle_src_event (GstPad * pad, GstEvent * event)
{
  GstTTMLBase *base;
  gboolean ret = TRUE;

  base = GST_TTMLBASE (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (base, "handling event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:{
      GstFormat format;

      /* check if TIME format, the only format we support */
      gst_event_parse_seek (event, NULL, &format, NULL, NULL, NULL, NULL, NULL);
      if (format == GST_FORMAT_TIME) {
        GstQuery *upstream;

        upstream = gst_query_new_seeking (GST_FORMAT_TIME);
        if (!gst_pad_peer_query (base->sinkpad, upstream)) {
          gst_query_unref (upstream);
          upstream = gst_query_new_seeking (GST_FORMAT_BYTES);
          if (!gst_pad_peer_query (base->sinkpad, upstream)) {
            ret = FALSE;
          } else {
            /* upstream handles in bytes, let's seek there and clip ourselves */
            ret = gst_ttmlbase_do_seek (base, event);
            event = NULL;
          }
        } else {
          /* upstream handles in time, forward the seek event */
          ret = gst_pad_push_event (base->sinkpad, event);
          event = NULL;

        }
        gst_query_unref (upstream);
      } else {
        ret = FALSE;
      }
      break;
    }

    default:
      ret = gst_pad_push_event (base->sinkpad, event);
      event = NULL;
      break;
  }

  if (event) {
    /* If we haven't pushed the event upstream, then unref it */
    gst_event_unref (event);
  }
  gst_object_unref (base);

  return ret;
}

static gboolean
gst_ttmlbase_handle_src_query (GstPad * pad, GstQuery * query)
{
  GstTTMLBase *base;
  gboolean ret = TRUE;

  base = GST_TTMLBASE (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (base, "handling query %s", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_SEEKING:{
      GstFormat format;

      /* we can seek on time if upstream handles on time or bytes */
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_TIME) {
        if (!gst_pad_peer_query (base->sinkpad, query)) {
          GstQuery *upstream;

          upstream = gst_query_new_seeking (GST_FORMAT_BYTES);
          if (!gst_pad_peer_query (base->sinkpad, upstream)) {
            ret = FALSE;
          } else {
            GST_DEBUG_OBJECT (base, "Upstream supports seeking on bytes");
            gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, -1);
          }
          gst_query_unref (upstream);
        } else {
          GST_DEBUG_OBJECT (base, "Upstream supports seeking on time");
        }
      } else {
        ret = FALSE;
      }
      break;
    }
    default:
      ret = gst_pad_peer_query (base->sinkpad, query);
      break;
  }

  gst_object_unref (base);

  return ret;
}

#if GST_CHECK_VERSION (1,0,0)

static GstFlowReturn
gst_ttmlbase_sink_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  return gst_ttmlbase_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlbase_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return gst_ttmlbase_handle_sink_event (pad, event);
}

static gboolean
gst_ttmlbase_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  return gst_ttmlbase_handle_src_query (pad, query);
}

static gboolean
gst_ttmlbase_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  return gst_ttmlbase_handle_src_event (pad, event);
}

#else

static GstFlowReturn
gst_ttmlbase_sink_chain (GstPad * pad, GstBuffer * buffer)
{
  return gst_ttmlbase_handle_buffer (pad, buffer);
}

static gboolean
gst_ttmlbase_sink_event (GstPad * pad, GstEvent * event)
{
  return gst_ttmlbase_handle_sink_event (pad, event);
}

static gboolean
gst_ttmlbase_src_query (GstPad * pad, GstQuery * query)
{
  return gst_ttmlbase_handle_src_query (pad, query);
}

static gboolean
gst_ttmlbase_src_event (GstPad * pad, GstEvent * event)
{
  return gst_ttmlbase_handle_src_event (pad, event);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ttmlbase_dispose (GObject * object)
{
  GstTTMLBase *base = GST_TTMLBASE (object);

  GST_DEBUG_OBJECT (base, "disposing TTML base");

  if (base->segment) {
    gst_segment_free (base->segment);
    base->segment = NULL;
  }

  if (base->pending_segment) {
    gst_segment_free (base->pending_segment);
    base->pending_segment = NULL;
  }

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));

  if (base->buf) {
    g_free (base->buf);
  }
}

static void
gst_ttmlbase_base_init (GstTTMLBaseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  /* Add sink pad template. Src pad template is to be added by derived class.
   * This is done in base_init instead of class_init because of issues with
   * GStreamer 0.10: The template is not shown with gst-inspect.
   */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&ttmlbase_sink_template));
}

static void
gst_ttmlbase_class_init (GstTTMLBaseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = GST_ELEMENT_CLASS (g_type_class_peek_parent (klass));

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_ttmlbase_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_ttmlbase_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_ttmlbase_get_property);

  /* Register properties */
  g_object_class_install_property (gobject_class, PROP_ASSUME_ORDERED_SPANS,
      g_param_spec_boolean ("assume_ordered_spans", "Assume ordered spans",
          "Generate buffers as soon as possible, by assuming that text "
          "spans will arrive in chronological order", FALSE,
          G_PARAM_READWRITE));

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
      GST_DEBUG_FUNCPTR (gst_ttmlbase_sink_chain));

  /* Create src pad. A template named "src" should have been added by the
   * derived class. */
  src_pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  base->srcpad = gst_pad_new_from_template (src_pad_template, "src");
  gst_pad_set_event_function (base->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttmlbase_src_event));
  gst_pad_set_query_function (base->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttmlbase_src_query));

  gst_element_add_pad (GST_ELEMENT (base), base->sinkpad);
  gst_element_add_pad (GST_ELEMENT (base), base->srcpad);

  base->segment = NULL;
  base->newsegment_needed = TRUE;

  base->xml_parser = NULL;
  base->base_time = GST_CLOCK_TIME_NONE;
  base->current_gst_status = GST_FLOW_OK;
  base->timeline = NULL;

  base->assume_ordered_spans = FALSE;

  base->state.attribute_stack = NULL;
  gst_ttml_state_reset (&base->state);

  gst_ttmlbase_cleanup (base);
  gstflu_demo_reset_statistics (&base->stats);

  base->state.frame_width = base->state.frame_height = 0;
}

GType
gst_ttmlbase_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstTTMLBaseClass),
      (GBaseInitFunc) gst_ttmlbase_base_init,
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
