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

  return;
}

#if 0
static gint
gst_ttmlparse_event_compare (GstTTMLEvent *a, GstTTMLEvent *b)
{
  return a->timestamp > b->timestamp ? 1 : -1;
}

static GList *
gst_ttmlparse_timeline_insert (GList *timeline, GstClockTime timestamp,
    GstTTMLEventType type, void *data)
{
  GstTTMLEvent *event = g_new (GstTTMLEvent, 1);
  event->timestamp = timestamp;
  event->type = type;
  event->data = data;

  return g_list_insert_sorted (timeline, event,
    (GCompareFunc)gst_ttmlparse_event_compare);
}

static GList *
gst_ttmlparse_timeline_get_next (GList *timeline,
    GstClockTime *timestamp, GstTTMLEventType *type, void **data)
{
  GstTTMLEvent *event = (GstTTMLEvent *)timeline->data;
  *timestamp = event->timestamp;
  *type = event->type;
  *data = event->data;
  return g_list_delete_link (timeline, timeline);
}
#endif

/* Check if the given node or attribute name matches a type, disregarding
 * possible namespaces */
static gboolean
gst_ttmlparse_is_type (const gchar * name, const gchar * type)
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
gst_ttmlparse_parse_time_expression (const gchar * expr)
{
  gdouble h, m, s, count;
  char metric[3] = "\0\0";
  GstClockTime res = GST_CLOCK_TIME_NONE;

  if (sscanf (expr, "%lf:%lf:%lf", &h, &m, &s) == 3) {
    res = (h * 3600 + m * 60 + s) * GST_SECOND;
  } else if (sscanf (expr, "%lf%2[hms]", &count, metric) == 2) {
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
      default:
        /* TODO: Handle 'f' and 't' metrics */
        GST_WARNING ("Unknown metric %s", metric);
    }
    res = count * scale;
  } else {
    GST_WARNING ("Unrecognized time expression: %s", expr);
  }
  GST_LOG ("Parsed %s into %" GST_TIME_FORMAT, expr, GST_TIME_ARGS (res));
  return res;
}

/* Pad push the stored buffer, using the correct timestamps and clipping */
static void
gst_ttmlparse_send_buffer (GstTTMLParse * parse)
{
  GstBuffer *buffer = parse->current_p;
  gboolean in_seg = FALSE;
  gint64 clip_start, clip_stop;

  /* If we have no buffer, this was an empty node, ignore it. */
  if (!buffer)
    return;

  /* Do not try to push anything if we have not recovered from previous
   * errors yet */
  if (parse->current_status != GST_FLOW_OK)
    return;

  gst_buffer_set_caps (buffer, GST_PAD_CAPS (parse->srcpad));

  /* BEGIN and END times are relative to their "container", in this case,
   * the buffer that brought us the TTML file. If there was no timing info
   * associated to this buffer, ignore it. */
  if (!GST_CLOCK_TIME_IS_VALID (parse->current_begin) ||
      !GST_CLOCK_TIME_IS_VALID (parse->current_end))
    return;
  parse->current_begin += parse->current_pts;
  parse->current_end += parse->current_pts;

  in_seg = gst_segment_clip (parse->segment, GST_FORMAT_TIME,
      parse->current_begin, parse->current_end, &clip_start, &clip_stop);

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
    GST_MEMDUMP_OBJECT (parse, "Subtitle content:",
        GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    parse->current_status = gst_pad_push (parse->srcpad, buffer);
  } else {
    GST_DEBUG_OBJECT (parse, "Buffer is out of segment (pts %"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (parse->current_begin));
    gst_buffer_unref (buffer);
  }

  parse->current_p = NULL;
}

/* Process a node start. If it is a <p> node, store interesting info. */
static void
gst_ttmlparse_sax_element_start (void *ctx, const xmlChar * name,
    const xmlChar ** attrs)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  const gchar **current_attr = (const gchar **) attrs;
  GST_LOG_OBJECT (parse, "New element: %s", name);
  while (current_attr && current_attr[0]) {
    GST_LOG_OBJECT (parse, "  %s=%s", current_attr[0], current_attr[1]);
    current_attr = &current_attr[2];
  }

  if (gst_ttmlparse_is_type ((const gchar *) name, "p")) {
    parse->inside_p = TRUE;

    parse->current_begin = GST_CLOCK_TIME_NONE;
    parse->current_end = GST_CLOCK_TIME_NONE;

    current_attr = (const gchar **) attrs;
    while (current_attr && current_attr[0]) {
      if (gst_ttmlparse_is_type (current_attr[0], "begin")) {
        parse->current_begin =
            gst_ttmlparse_parse_time_expression (current_attr[1]);
      } else if (gst_ttmlparse_is_type (current_attr[0], "end")) {
        parse->current_end =
            gst_ttmlparse_parse_time_expression (current_attr[1]);
      } else if (gst_ttmlparse_is_type (current_attr[0], "dur")) {
        parse->current_end = parse->current_begin +
            gst_ttmlparse_parse_time_expression (current_attr[1]);
      }

      current_attr = &current_attr[2];
    }

    if (parse->current_p) {
      GST_WARNING_OBJECT (parse, "Removed dangling buffer");
      gst_buffer_unref (parse->current_p);
      parse->current_p = NULL;
    }
  }
}

/* Process a node end. If it is a <p> node, send any stored buffer. */
static void
gst_ttmlparse_sax_element_end (void *ctx, const xmlChar * name)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);

  GST_LOG_OBJECT (parse, "End element: %s", name);
  if (gst_ttmlparse_is_type ((const gchar *) name, "p")) {
    parse->inside_p = FALSE;
    gst_ttmlparse_send_buffer (parse);
  }
}

/* Process character. If they are inside a <p> node, create buffer to hold
 * them and store it for later (it might grow in size) */
static void
gst_ttmlparse_sax_characters (void *ctx, const xmlChar * ch, int len)
{
  GstTTMLParse *parse = GST_TTMLPARSE (ctx);
  GstBuffer *new_buffer;
  const gchar *content = (const gchar *) ch;
  const gchar *content_end = NULL;
  gint content_size = 0;

  if (!parse->inside_p)
    return;

  /* Start by validating UTF-8 content */
  if (!g_utf8_validate (content, len, &content_end)) {
    GST_WARNING_OBJECT (parse, "Content is not valid UTF-8");
    return;
  }

  content_size = content_end - content;

  new_buffer = gst_buffer_new_and_alloc (content_size);
  memcpy (GST_BUFFER_DATA (new_buffer), content, content_size);

  if (parse->current_p) {
    /* TODO */
    GST_WARNING_OBJECT (parse, "Multiple strings inside <p> unimplemented");
  } else {
    parse->current_p = new_buffer;
  }
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
  /*. endDocument = */ NULL,
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
  parse->current_status = GST_FLOW_OK;

  /* Set caps on src pad */
  if (G_UNLIKELY (!GST_PAD_CAPS (parse->srcpad))) {
    GstCaps *caps = gst_caps_new_simple ("text/plain", NULL);

    GST_DEBUG_OBJECT (parse->srcpad, "setting caps %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps (parse->srcpad, caps);

    gst_caps_unref (caps);
  }

  /* Store buffer timestamp. All future timestamps we produce will be relative
   * to this buffer time. */
  if (!GST_CLOCK_TIME_IS_VALID (parse->current_pts) &&
      GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {
    parse->current_pts = GST_BUFFER_TIMESTAMP (buffer);
  }

  GST_LOG_OBJECT (parse, "Handling buffer of %u bytes pts %" GST_TIME_FORMAT,
      GST_BUFFER_SIZE (buffer), GST_TIME_ARGS (parse->current_pts));

  buffer_data = (const char *) GST_BUFFER_DATA (buffer);
  buffer_len = GST_BUFFER_SIZE (buffer);
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
      parse->current_pts = GST_CLOCK_TIME_NONE;

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

  return parse->current_status;
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

  if (parse->timeline) {
    g_list_free_full (parse->timeline, g_free);
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
  parse->inside_p = FALSE;
  parse->current_p = NULL;
  parse->current_begin = GST_CLOCK_TIME_NONE;
  parse->current_end = GST_CLOCK_TIME_NONE;
  parse->current_pts = GST_CLOCK_TIME_NONE;
  parse->current_status = GST_FLOW_OK;
  parse->timeline = NULL;

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
