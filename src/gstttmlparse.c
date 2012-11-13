/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstttmlparse.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

static GstElementDetails ttmlparse_details = {
  "TTML subtitle parser",
  "Codec/Parser/Subtitle",
  "Parse TTML subtitle streams into text stream",
  "Fluendo S.A. <support@fluendo.com>",
};

static GstStaticPadTemplate ttmlparse_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("text/plain; text/x-pango-markup"));

static GstStaticPadTemplate ttmlparse_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-subtitle-ttml"));
    
static GstElementClass * parent_class = NULL;

static void
gst_ttmlparse_cleanup (GstTTMLParse * parse)
{
  GST_DEBUG_OBJECT (parse, "cleaning up TTML parser");
  
  if (parse->segment) {
    gst_segment_init (parse->segment, GST_FORMAT_TIME);
  }
 
  return;
}

static GstFlowReturn
gst_ttmlparse_handle_doc (GstTTMLParse * parse, xmlDocPtr doc, GstClockTime pts)
{
  GstFlowReturn ret = GST_FLOW_OK;
  xmlXPathContextPtr xpathCtx = NULL;
  xmlXPathObjectPtr xpathObj = NULL;
  gint nb_nodes;
  
  xpathCtx = xmlXPathNewContext (doc);
  if (G_UNLIKELY (!xpathCtx)) {
    GST_WARNING_OBJECT (parse, "failed creating an XPATH context from doc");
    ret = GST_FLOW_ERROR;
    goto beach;
  }
  
  /* Register namespaces */
  xmlXPathRegisterNs (xpathCtx, BAD_CAST "ttml",
      BAD_CAST "http://www.w3.org/2006/10/ttaf1");
  xmlXPathRegisterNs (xpathCtx, BAD_CAST "ttmlstyle",
      BAD_CAST "http://www.w3.org/2006/10/ttaf1#styling");
  xmlXPathRegisterNs (xpathCtx, BAD_CAST "ttmlmetadata",
      BAD_CAST "http://www.w3.org/2006/10/ttaf1#metadata");
  
  xpathObj = xmlXPathEvalExpression (BAD_CAST "//ttml:p", xpathCtx);
  if (G_UNLIKELY (!xpathObj)) {
    GST_WARNING_OBJECT (parse, "failed evaluating XPATH expression");
    ret = GST_FLOW_ERROR;
    goto beach;
  }
  
  nb_nodes = xpathObj->nodesetval->nodeNr;

  GST_LOG_OBJECT (parse, "found %d matching nodes in this doc",
      nb_nodes);
  
  if (G_LIKELY (nb_nodes)) {
    gint i;
    xmlNodePtr node;
    
    /*  Foreach p node we found */
    for (i = 0; i < nb_nodes; i++) {
      node = xpathObj->nodesetval->nodeTab[i];
      
      if (node->children && node->children->content &&
          node->children->type == XML_TEXT_NODE) {
        GstBuffer * buffer = NULL;
        GstClockTime begin = GST_CLOCK_TIME_NONE, end = GST_CLOCK_TIME_NONE;
        const gchar * content = (const gchar *) node->children->content;
        const gchar * content_end = NULL;
        gint content_size = 0;
        gboolean in_seg = FALSE;
        gint64 clip_start, clip_stop;
        
        /* Start by validating UTF-8 content */
        if (!g_utf8_validate (content, -1, &content_end)) {
          GST_WARNING_OBJECT (parse, "content is not valid UTF-8");
          continue;
        }
        
        content_size = content_end - content;
        
        buffer = gst_buffer_new_and_alloc (content_size + 1);
        memcpy (GST_BUFFER_DATA (buffer), content, content_size);
        
        if (node->properties) {
          xmlAttr * attr = node->properties;
          
          while (attr) {
            if (g_ascii_strcasecmp ((const gchar *) attr->name, "begin") == 0) {
              guint h, m, s;
              char * content = (char *) attr->children->content;
              
              if (sscanf (content, "%u:%u:%u", &h, &m, &s) == 3) {
                begin = ((guint64) h * 3600 + m * 60 + s) * GST_SECOND;
              }
            }
            else if (g_ascii_strcasecmp ((const gchar *) attr->name, "end") == 0) {
              guint h, m, s;
              char * content = (char *) attr->children->content;

              if (sscanf (content, "%u:%u:%u", &h, &m, &s) == 3) {
                end = ((guint64) h * 3600 + m * 60 + s) * GST_SECOND;
              }
            }

            attr = attr->next;
          }
          
          if (GST_CLOCK_TIME_IS_VALID (begin)) {
            begin += pts;
          }
          if (GST_CLOCK_TIME_IS_VALID (end)) {
            end += pts;
          }
        }
        
        /* Special hack for Authentec :
         * As the drmas demuxer will start pushing buffers with timestamps
         * starting from zero after a seek, we have to offset all the 
         * timestamps using the time position of the NEWSEGMENT event that
         * was sent. */
        if (begin >= parse->segment->time) {
          begin -= parse->segment->time;
          end -= parse->segment->time;
        }
        else {
          GST_DEBUG_OBJECT (parse, "drop buffer as it's out of segment " \
              "(Authentec case)");
          gst_buffer_unref (buffer);
          continue;
        }
        
        gst_buffer_set_caps (buffer, GST_PAD_CAPS (parse->srcpad));
        
        in_seg =
            gst_segment_clip (parse->segment, GST_FORMAT_TIME, begin, end,
                &clip_start, &clip_stop);
                
        if (in_seg) {
          if (G_UNLIKELY (parse->newsegment_needed)) {
            GstEvent *event;

            event = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME, 0, -1, 0);
            GST_DEBUG_OBJECT (parse, "Pushing default newsegment");
            gst_pad_push_event (parse->srcpad, event);
            parse->newsegment_needed = FALSE;
          }

          GST_BUFFER_TIMESTAMP (buffer) = clip_start;
          GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
        
          GST_DEBUG_OBJECT (parse, "pushing buffer of %u bytes, pts %" \
              GST_TIME_FORMAT " duration %" GST_TIME_FORMAT,
              GST_BUFFER_SIZE (buffer),
              GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
              GST_TIME_ARGS (GST_BUFFER_DURATION (buffer))); 
          GST_LOG_OBJECT (parse, "subtitle content %s", GST_BUFFER_DATA (buffer));
              
          ret = gst_pad_push (parse->srcpad, buffer);
          
          if (ret != GST_FLOW_OK) {
            GST_WARNING_OBJECT (parse, "flow return %s stopping here",
                gst_flow_get_name (ret));
            goto beach;
          }
        }
        else {
          GST_DEBUG_OBJECT (parse, "buffer is out of segment (pts %" \
              GST_TIME_FORMAT ")", GST_TIME_ARGS (begin));
          gst_buffer_unref (buffer);
        }
      }
    }
  }
  
beach:
  if (xpathObj) {
    xmlXPathFreeObject (xpathObj);
  }
  if (xpathCtx) {
    xmlXPathFreeContext (xpathCtx);
  }
  
  return ret;
}

static GstFlowReturn
gst_ttmlparse_chain (GstPad * pad, GstBuffer * buffer)
{
  GstTTMLParse * parse;
  xmlDocPtr doc = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  
  parse = GST_TTMLPARSE (gst_pad_get_parent (pad));
  
  GST_LOG_OBJECT (parse, "handling buffer of %u bytes pts %" GST_TIME_FORMAT,
      GST_BUFFER_SIZE (buffer), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  
  /* Parse this buffer as a complete XML document */
  doc = xmlReadMemory ((const char *) GST_BUFFER_DATA (buffer),
      GST_BUFFER_SIZE (buffer), NULL, NULL, XML_PARSE_NONET);
  if (G_UNLIKELY (!doc)) {
    GST_WARNING_OBJECT (parse, "failed parsing XML document");
    ret = GST_FLOW_ERROR;
    goto beach;
  }
  
  if (G_UNLIKELY (!GST_PAD_CAPS (parse->srcpad))) {
    GstCaps * caps = gst_caps_new_simple ("text/plain", NULL);
    
    GST_DEBUG_OBJECT (parse->srcpad, "setting caps %" GST_PTR_FORMAT, caps);
    gst_pad_set_caps (parse->srcpad, caps);
    
    gst_caps_unref (caps);
  }

  ret = gst_ttmlparse_handle_doc (parse, doc, GST_BUFFER_TIMESTAMP (buffer));

  /* Free the parsed tree */
  xmlFreeDoc (doc);
  
beach:
  gst_buffer_unref (buffer);
  
  gst_object_unref (parse);
  
  return ret;
}

static gboolean
gst_ttmlparse_sink_event (GstPad * pad, GstEvent * event)
{
  GstTTMLParse * parse;
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
        GST_DEBUG_OBJECT (parse, "dropping it because it is not in TIME format");
        goto beach;
      }
      
      GST_DEBUG_OBJECT (parse, "received new segment update %d, rate %f, " \
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
  GstTTMLParse * parse;
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
gst_ttmlparse_dispose (GObject * object)
{
  GstTTMLParse * parse = GST_TTMLPARSE (object);

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
  GstElementClass * element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&ttmlparse_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&ttmlparse_src_template));
      
  gst_element_class_set_details (element_class, &ttmlparse_details);
}

static void
gst_ttmlparse_class_init (GstTTMLParseClass * klass)
{
  GstElementClass * gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_ttmlparse_dispose;
  //gobject_class->set_property = gst_ttmlparse_set_property;
  //gobject_class->get_property = gst_ttmlparse_get_property;
 
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

